"""Voz de las lecciones: caché en disco, una sola voz por sesión y respaldo local inmediato.

Antes cada paso probaba tres modelos TTS desde cero: con la cuota agotada (429) la voz cambiaba
de timbre a media lección y cada paso costaba de 4 a 10 s. Aquí:

  * el audio se guarda en `private_local/voz_cache` y la segunda vez es instantáneo;
  * un modelo se usa hasta que se agota; solo entonces se pasa al siguiente y se recuerda;
  * si no hay nube (o tarda demasiado) se responde `motor=local` y el navegador habla con la voz
    de Windows, sin dejar a nadie esperando;
  * junto al audio va un mapa tiempo -> carácter para que la mano escriba al ritmo de la voz.
"""

from __future__ import annotations

import asyncio
import hashlib
import json
import pathlib
import re
import threading
import time
from typing import Any, Dict, List, Optional, Sequence, Tuple

from .gemini_engines import (
    TTS_MODEL_CHAIN,
    _extract_wav_pcm16_mono,
    _trim_pcm16_edge_silence,
    align_cues_with_wav_rms,
    ensure_wav_and_duration_ms,
    estimate_cues_fallback,
    normalize_gemini_voice,
)
from .secrets_dpapi import sanitize_text

VERSION_CACHE = "v2"
TOPE_CACHE_MB = 600
ESPERA_URGENTE_S = 9.0


def _clave(texto: str, voz: str) -> str:
    return hashlib.sha1(f"{VERSION_CACHE}|{voz}|{texto}".encode("utf-8")).hexdigest()[:24]


def _segundos_de_espera(mensaje: str) -> float:
    """Lee el retryDelay de un 429. Cuota diaria -> se aparta el modelo por una hora."""
    m = re.search(r"retry(?:Delay)?['\"]?\s*[:=]?\s*['\"]?(?:in\s+)?(\d+(?:\.\d+)?)\s*s", mensaje, re.I)
    if re.search(r"per\s*day|PerDay|daily", mensaje, re.I):
        return 3600.0
    if m:
        return max(5.0, min(3600.0, float(m.group(1)) + 1.0))
    return 60.0


class ServicioVoz:
    def __init__(self, api_key: str, carpeta: Optional[pathlib.Path], voz: str = "Kore", modelos: Sequence[str] = tuple(TTS_MODEL_CHAIN)) -> None:
        self.voz = normalize_gemini_voice(voz, "Kore")
        self.carpeta = carpeta
        if carpeta is not None:
            carpeta.mkdir(parents=True, exist_ok=True)
        self._cliente = None
        self._api_key = (api_key or "").strip()
        self._modelos = list(modelos)
        self._apartado: Dict[str, float] = {}
        self._ultimo_error = ""
        self._en_curso: Dict[str, asyncio.Task] = {}
        self._sem: Optional[asyncio.Semaphore] = None
        self._candado = threading.Lock()
        self._memoria: Dict[str, Tuple[bytes, Dict[str, Any]]] = {}

    # ------------------------------------------------------------ estado

    @property
    def configurada(self) -> bool:
        return bool(self._api_key)

    def _cli(self):
        if self._cliente is None and self._api_key:
            from google import genai

            self._cliente = genai.Client(api_key=self._api_key)
        return self._cliente

    def modelos_disponibles(self) -> List[str]:
        ahora = time.time()
        return [m for m in self._modelos if self._apartado.get(m, 0) <= ahora]

    def estado(self) -> Dict[str, Any]:
        return {
            "configurada": self.configurada,
            "voz": self.voz,
            "modelos": self.modelos_disponibles(),
            "apartados": {m: int(t - time.time()) for m, t in self._apartado.items() if t > time.time()},
            "ultimo_error": self._ultimo_error,
        }

    # ------------------------------------------------------------ caché

    def _ruta(self, clave: str, ext: str) -> Optional[pathlib.Path]:
        return (self.carpeta / f"{clave}.{ext}") if self.carpeta is not None else None

    def leer(self, clave: str) -> Optional[Tuple[bytes, Dict[str, Any]]]:
        if not re.fullmatch(r"[0-9a-f]{24}", clave):
            return None
        if clave in self._memoria:
            return self._memoria[clave]
        w, j = self._ruta(clave, "wav"), self._ruta(clave, "json")
        if w is None or j is None or not w.is_file() or not j.is_file():
            return None
        try:
            meta = json.loads(j.read_text(encoding="utf-8"))
            datos = w.read_bytes()
        except (OSError, ValueError):
            return None
        if len(datos) < 100:
            return None
        try:
            w.touch()
        except OSError:
            pass
        return datos, meta

    def _guardar(self, clave: str, wav: bytes, meta: Dict[str, Any]) -> None:
        w, j = self._ruta(clave, "wav"), self._ruta(clave, "json")
        if w is None or j is None:
            self._memoria[clave] = (wav, meta)
            if len(self._memoria) > 60:
                self._memoria.pop(next(iter(self._memoria)))
            return
        try:
            tmp = w.with_suffix(".parcial")
            tmp.write_bytes(wav)
            tmp.replace(w)
            j.write_text(json.dumps(meta, ensure_ascii=False), encoding="utf-8")
        except OSError:
            self._memoria[clave] = (wav, meta)
        self._podar()

    def _podar(self) -> None:
        if self.carpeta is None:
            return
        with self._candado:
            try:
                fs = sorted(self.carpeta.glob("*.wav"), key=lambda p: p.stat().st_mtime)
                total = sum(p.stat().st_size for p in fs)
                for p in fs:
                    if total <= TOPE_CACHE_MB * 1024 * 1024:
                        break
                    total -= p.stat().st_size
                    p.unlink(missing_ok=True)
                    p.with_suffix(".json").unlink(missing_ok=True)
            except OSError:
                pass

    # ------------------------------------------------------------ síntesis

    def _sintetiza_bloqueante(self, texto: str, voz: str) -> Tuple[bytes, str]:
        from google.genai import types

        cli = self._cli()
        if cli is None:
            raise RuntimeError("sin clave de voz")
        errores: List[str] = []
        for modelo in self.modelos_disponibles():
            try:
                resp = cli.models.generate_content(
                    model=modelo,
                    contents=texto,
                    config=types.GenerateContentConfig(
                        response_modalities=["AUDIO"],
                        speech_config=types.SpeechConfig(
                            voice_config=types.VoiceConfig(
                                prebuilt_voice_config=types.PrebuiltVoiceConfig(voice_name=voz)
                            )
                        ),
                    ),
                )
                crudo, mime = b"", ""
                for cand in getattr(resp, "candidates", None) or []:
                    for parte in getattr(getattr(cand, "content", None), "parts", None) or []:
                        ind = getattr(parte, "inline_data", None)
                        if ind and getattr(ind, "data", None):
                            crudo += ind.data
                            mime = mime or str(getattr(ind, "mime_type", "") or "")
                if not crudo:
                    raise RuntimeError("audio vacío")
                wav, _ = ensure_wav_and_duration_ms(crudo, mime)
                return wav, modelo
            except Exception as exc:  # noqa: BLE001 - cualquier fallo pasa al siguiente modelo
                msg = sanitize_text(str(exc))
                errores.append(f"{modelo}: {msg[:120]}")
                if "429" in msg or "RESOURCE_EXHAUSTED" in msg:
                    self._apartado[modelo] = time.time() + _segundos_de_espera(msg)
                elif "404" in msg or "NOT_FOUND" in msg or "not found" in msg or "400" in msg:
                    self._apartado[modelo] = time.time() + 3600.0
        self._ultimo_error = " | ".join(errores)[:400] or "ningún modelo de voz disponible"
        raise RuntimeError(self._ultimo_error)

    @staticmethod
    def _alinear(wav: bytes, texto: str, cortes: Sequence[int]) -> Dict[str, Any]:
        cues = [(f"s{i}", int(c)) for i, c in enumerate(cortes)]
        est = estimate_cues_fallback(texto, cues, 1.0)
        al = align_cues_with_wav_rms(wav, texto, cues, est)
        dur = int(al["duration_ms"])
        puntos: List[List[int]] = []
        for p in al.get("words") or []:
            puntos.append([int(p["start_ms"]), int(p["char_idx"])])
        puntos.append([dur, len(texto)])
        # Monótono en ambas coordenadas
        limpio: List[List[int]] = [[0, 0]]
        for t, c in puntos:
            if t > limpio[-1][0] and c >= limpio[-1][1]:
                limpio.append([t, c])
        return {"dur_ms": dur, "mapa": limpio, "cortes_ms": [int(al["cues_ms"].get(f"s{i}", 0)) for i in range(len(cortes))]}

    async def _fabricar(self, clave: str, texto: str, cortes: Sequence[int], voz: str) -> Dict[str, Any]:
        if self._sem is None:
            self._sem = asyncio.Semaphore(2)
        async with self._sem:
            ya = self.leer(clave)
            if ya:
                return ya[1]
            wav, modelo = await asyncio.to_thread(self._sintetiza_bloqueante, texto, voz)
            pcm, sr = _extract_wav_pcm16_mono(wav)
            if pcm:
                rec, _ = _trim_pcm16_edge_silence(pcm, sr, max_lead_ms=30, max_tail_ms=160)
                wav, _ = ensure_wav_and_duration_ms(rec, f"audio/L16;codec=pcm;rate={sr}")
            meta = self._alinear(wav, texto, cortes)
            meta.update({"motor": f"gemini:{modelo}", "voz": voz, "clave": clave})
            await asyncio.to_thread(self._guardar, clave, wav, meta)
            return meta

    async def pedir(self, texto: str, cortes: Sequence[int] = (), voz: str = "", urgente: bool = True) -> Dict[str, Any]:
        """Devuelve metadatos con `clave` si hay audio, o `motor=local` si hay que usar la voz de Windows."""
        texto = " ".join((texto or "").split())
        if not texto:
            return {"motor": "silencio", "dur_ms": 0, "mapa": [[0, 0]], "clave": ""}
        v = normalize_gemini_voice(voz, self.voz)
        clave = _clave(texto, v)
        ya = self.leer(clave)
        if ya:
            return {**ya[1], "cache": True}
        if not self.configurada or not self.modelos_disponibles():
            return {"motor": "local", "motivo": "sin cuota o sin clave", "clave": ""}
        tarea = self._en_curso.get(clave)
        if tarea is None or tarea.done():
            tarea = asyncio.create_task(self._fabricar(clave, texto, list(cortes) or [0], v))
            self._en_curso[clave] = tarea
            tarea.add_done_callback(lambda t, k=clave: self._en_curso.pop(k, None))
        try:
            meta = await asyncio.wait_for(asyncio.shield(tarea), timeout=ESPERA_URGENTE_S if urgente else 60.0)
            return {**meta, "cache": False}
        except asyncio.TimeoutError:
            return {"motor": "local", "motivo": "la nube tardó demasiado", "clave": ""}
        except Exception as exc:  # noqa: BLE001
            return {"motor": "local", "motivo": sanitize_text(str(exc))[:160], "clave": ""}
