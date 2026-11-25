"""Voz de las lecciones: motor neural gratuito (Microsoft Edge Neural TTS) con marcas por palabra,
caché en disco, respaldo opcional en Gemini TTS y respaldo sin conexión en Windows.

  * Primario: `edge-tts` (voces neuronales en español de estudio, gratis, sin clave ni cuotas 429,
    con marcas exactas `WordBoundary` al milisegundo para sincronizar trazo y subtítulos).
  * Caché en `private_local/voz_cache` (MP3 compacto ~40 KB/paso; la segunda vez responde en ~3 ms).
  * Respaldo 1: Gemini TTS si hay clave y cuota disponible.
  * Respaldo 2: `motor=local` (voz del sistema) únicamente sin conexión a internet.
"""

from __future__ import annotations

import asyncio
import hashlib
import json
import pathlib
import re
import threading
import time
import unicodedata
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

try:
    import edge_tts as _edge_tts
except Exception:  # pragma: no cover - si falta edge-tts cae al respaldo
    _edge_tts = None  # type: ignore[assignment]

VERSION_CACHE = "v3"
TOPE_CACHE_MB = 600
ESPERA_URGENTE_S = 12.0
VOZ_DEFECTO = "es-MX-DaliaNeural"

VOCES_NEURALES: List[Dict[str, str]] = [
    {"id": "es-MX-DaliaNeural", "etiqueta": "Dalia (México · clara y natural)", "genero": "F"},
    {"id": "es-CR-MariaNeural", "etiqueta": "María (Costa Rica · natural)", "genero": "F"},
    {"id": "es-CR-JuanNeural", "etiqueta": "Juan (Costa Rica · natural)", "genero": "M"},
    {"id": "es-CO-SalomeNeural", "etiqueta": "Salomé (Colombia · cálida)", "genero": "F"},
    {"id": "es-CO-GonzaloNeural", "etiqueta": "Gonzalo (Colombia · cálido)", "genero": "M"},
    {"id": "es-MX-JorgeNeural", "etiqueta": "Jorge (México · claro y firme)", "genero": "M"},
    {"id": "en-US-AvaMultilingualNeural", "etiqueta": "Ava (Multilingüe · muy expresiva)", "genero": "F"},
    {"id": "en-US-AndrewMultilingualNeural", "etiqueta": "Andrew (Multilingüe · conversacional)", "genero": "M"},
    {"id": "es-ES-XimenaNeural", "etiqueta": "Ximena (España · expresiva)", "genero": "F"},
    {"id": "es-ES-AlvaroNeural", "etiqueta": "Álvaro (España · claro)", "genero": "M"},
]

_IDS_NEURALES = {v["id"] for v in VOCES_NEURALES}

# Mapeo suave de nombres legados de Gemini -> voces neuronales equivalentes
MAPA_VOCES_LEGADO: Dict[str, str] = {
    "Kore": "es-MX-DaliaNeural",
    "Aoede": "es-CO-SalomeNeural",
    "Charon": "es-MX-JorgeNeural",
    "Puck": "es-CR-JuanNeural",
    "Fenrir": "es-CO-GonzaloNeural",
}


def normalizar_voz(voz: str, por_defecto: str = VOZ_DEFECTO) -> str:
    """Devuelve un identificador de voz neuronal válido (o mapea nombres legados)."""
    raw = (voz or "").strip()
    if raw in _IDS_NEURALES:
        return raw
    if raw in MAPA_VOCES_LEGADO:
        return MAPA_VOCES_LEGADO[raw]
    # Acepta cualquier voz oficial es-*Neural o *MultilingualNeural de Edge
    if re.fullmatch(r"[a-z]{2}-[A-Z]{2}-[A-Za-z0-9]+Neural", raw):
        return raw
    if por_defecto in _IDS_NEURALES:
        return por_defecto
    return MAPA_VOCES_LEGADO.get(por_defecto, VOZ_DEFECTO)


def _clave(texto: str, voz: str) -> str:
    return hashlib.sha1(f"{VERSION_CACHE}|{voz}|{texto}".encode("utf-8")).hexdigest()[:24]


def _sin_tildes(s: str) -> str:
    return "".join(c for c in unicodedata.normalize("NFKD", s.lower()) if not unicodedata.combining(c))


def _segundos_de_espera(mensaje: str) -> float:
    """Lee el retryDelay de un 429. Cuota diaria -> se aparta el modelo por una hora."""
    m = re.search(r"retry(?:Delay)?['\"]?\s*[:=]?\s*['\"]?(?:in\s+)?(\d+(?:\.\d+)?)\s*s", mensaje, re.I)
    if re.search(r"per\s*day|PerDay|daily", mensaje, re.I):
        return 3600.0
    if m:
        return max(5.0, min(3600.0, float(m.group(1)) + 1.0))
    return 60.0


def _interpolar_tiempo(mapa: Sequence[Sequence[int]], char_idx: int) -> int:
    if not mapa:
        return 0
    if char_idx <= mapa[0][1]:
        return int(mapa[0][0])
    for i in range(1, len(mapa)):
        t0, c0 = mapa[i - 1]
        t1, c1 = mapa[i]
        if char_idx <= c1:
            if c1 <= c0:
                return int(t0)
            frac = (char_idx - c0) / (c1 - c0)
            return int(round(t0 + (t1 - t0) * frac))
    return int(mapa[-1][0])


def _mapa_desde_palabras(
    texto: str,
    palabras: Sequence[Tuple[int, int, str]],
    cortes: Sequence[int],
    bytes_mp3: int = 0,
) -> Dict[str, Any]:
    """Convierte eventos `WordBoundary` de Edge TTS `(start_ms, dur_ms, word)` en mapa tiempo->carácter."""
    n = len(texto)
    texto_low = texto.lower()
    texto_fold = _sin_tildes(texto)
    cursor = 0
    puntos: List[List[int]] = [[0, 0]]
    ultimo_fin_ms = 0

    for start_ms, dur_ms, w in palabras:
        w_str = (w or "").strip()
        if not w_str:
            continue
        ultimo_fin_ms = max(ultimo_fin_ms, int(start_ms) + int(dur_ms))
        idx = texto_low.find(w_str.lower(), cursor)
        if idx < 0:
            wf = _sin_tildes(w_str)
            if wf:
                idx = texto_fold.find(wf, cursor)
        if idx < 0:
            # Siguiente palabra no vacía desde el cursor
            m = re.search(r"\S+", texto[cursor:])
            idx = (cursor + m.start()) if m else cursor
        char_pos = max(cursor, min(n, idx))
        if int(start_ms) > puntos[-1][0] and char_pos >= puntos[-1][1]:
            puntos.append([int(start_ms), char_pos])
        cursor = min(n, char_pos + max(1, len(w_str)))

    # 48 kbps CBR MP3 => 6 bytes/ms
    dur_mp3 = int(round((bytes_mp3 * 8) / 48.0)) if bytes_mp3 > 0 else 0
    dur = max(ultimo_fin_ms + 120, dur_mp3, 400)
    if dur > puntos[-1][0]:
        puntos.append([dur, n])
    else:
        puntos[-1][1] = n

    cortes_ms = [_interpolar_tiempo(puntos, int(c)) for c in cortes]
    return {"dur_ms": dur, "mapa": puntos, "cortes_ms": cortes_ms}


class ServicioVoz:
    def __init__(
        self,
        api_key: str,
        carpeta: Optional[pathlib.Path],
        voz: str = VOZ_DEFECTO,
        modelos: Sequence[str] = tuple(TTS_MODEL_CHAIN),
    ) -> None:
        self.voz = normalizar_voz(voz, VOZ_DEFECTO)
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
    def edge_disponible(self) -> bool:
        return _edge_tts is not None and self._apartado.get("edge-tts", 0) <= time.time()

    @property
    def configurada(self) -> bool:
        return self.edge_disponible or bool(self._api_key)

    def _cli(self):
        if self._cliente is None and self._api_key:
            from google import genai

            self._cliente = genai.Client(api_key=self._api_key)
        return self._cliente

    def modelos_disponibles(self) -> List[str]:
        ahora = time.time()
        mods: List[str] = []
        if self.edge_disponible:
            mods.append("edge-tts")
        if self._api_key:
            mods.extend(m for m in self._modelos if self._apartado.get(m, 0) <= ahora)
        return mods

    def estado(self) -> Dict[str, Any]:
        return {
            "configurada": self.configurada,
            "voz": self.voz,
            "motor_primario": "edge-tts" if self.edge_disponible else ("gemini" if self._api_key else "local"),
            "voces": VOCES_NEURALES,
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
        j = self._ruta(clave, "json")
        if j is None or not j.is_file():
            return None
        try:
            meta = json.loads(j.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None
        ext = "mp3" if str(meta.get("mime") or "").endswith("mpeg") else "wav"
        a = self._ruta(clave, ext)
        if a is None or not a.is_file():
            alt = self._ruta(clave, "wav" if ext == "mp3" else "mp3")
            a = alt if (alt is not None and alt.is_file()) else None
        if a is None:
            return None
        try:
            datos = a.read_bytes()
        except OSError:
            return None
        if len(datos) < 100:
            return None
        try:
            a.touch()
        except OSError:
            pass
        if "mime" not in meta:
            meta["mime"] = "audio/mpeg" if a.suffix == ".mp3" else "audio/wav"
        return datos, meta

    def _guardar(self, clave: str, datos: bytes, meta: Dict[str, Any], ext: str = "mp3") -> None:
        a, j = self._ruta(clave, ext), self._ruta(clave, "json")
        if a is None or j is None:
            self._memoria[clave] = (datos, meta)
            if len(self._memoria) > 80:
                self._memoria.pop(next(iter(self._memoria)))
            return
        try:
            tmp = a.with_suffix(".parcial")
            tmp.write_bytes(datos)
            tmp.replace(a)
            j.write_text(json.dumps(meta, ensure_ascii=False), encoding="utf-8")
        except OSError:
            self._memoria[clave] = (datos, meta)
        self._podar()

    def _podar(self) -> None:
        if self.carpeta is None:
            return
        with self._candado:
            try:
                fs = sorted(
                    list(self.carpeta.glob("*.mp3")) + list(self.carpeta.glob("*.wav")),
                    key=lambda p: p.stat().st_mtime,
                )
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

    async def _sintetiza_edge(self, texto: str, voz: str) -> Tuple[bytes, List[Tuple[int, int, str]]]:
        if _edge_tts is None:
            raise RuntimeError("edge-tts no instalado")
        ultimo: Optional[Exception] = None
        for intento in range(2):
            try:
                comm = _edge_tts.Communicate(texto, voice=voz, boundary="WordBoundary")
                audio = bytearray()
                palabras: List[Tuple[int, int, str]] = []
                async for chunk in comm.stream():
                    tipo = chunk.get("type")
                    if tipo == "audio":
                        audio.extend(chunk.get("data") or b"")
                    elif tipo == "WordBoundary":
                        palabras.append((
                            int(chunk.get("offset", 0)) // 10_000,
                            int(chunk.get("duration", 0)) // 10_000,
                            str(chunk.get("text") or ""),
                        ))
                if len(audio) < 100:
                    raise RuntimeError("edge-tts devolvió audio vacío")
                return bytes(audio), palabras
            except Exception as exc:  # noqa: BLE001
                ultimo = exc
                if intento == 0:
                    await asyncio.sleep(0.25)
        msg = sanitize_text(str(ultimo))
        self._ultimo_error = f"edge-tts: {msg[:240]}"
        if "403" in msg or "401" in msg:
            self._apartado["edge-tts"] = time.time() + 180.0
        raise RuntimeError(self._ultimo_error)

    def _sintetiza_gemini_bloqueante(self, texto: str, voz_gemini: str) -> Tuple[bytes, str]:
        from google.genai import types

        cli = self._cli()
        if cli is None:
            raise RuntimeError("sin clave de Gemini")
        ahora = time.time()
        mods = [m for m in self._modelos if self._apartado.get(m, 0) <= ahora]
        errores: List[str] = []
        for modelo in mods:
            try:
                resp = cli.models.generate_content(
                    model=modelo,
                    contents=texto,
                    config=types.GenerateContentConfig(
                        response_modalities=["AUDIO"],
                        speech_config=types.SpeechConfig(
                            voice_config=types.VoiceConfig(
                                prebuilt_voice_config=types.PrebuiltVoiceConfig(voice_name=voz_gemini)
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
            except Exception as exc:  # noqa: BLE001
                msg = sanitize_text(str(exc))
                errores.append(f"{modelo}: {msg[:120]}")
                if "429" in msg or "RESOURCE_EXHAUSTED" in msg:
                    self._apartado[modelo] = time.time() + _segundos_de_espera(msg)
                elif "404" in msg or "NOT_FOUND" in msg or "not found" in msg or "400" in msg:
                    self._apartado[modelo] = time.time() + 3600.0
        self._ultimo_error = " | ".join(errores)[:400] or "ningún modelo de Gemini disponible"
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
        limpio: List[List[int]] = [[0, 0]]
        for t, c in puntos:
            if t > limpio[-1][0] and c >= limpio[-1][1]:
                limpio.append([t, c])
        return {"dur_ms": dur, "mapa": limpio, "cortes_ms": [int(al["cues_ms"].get(f"s{i}", 0)) for i in range(len(cortes))]}

    async def _fabricar(self, clave: str, texto: str, cortes: Sequence[int], voz: str) -> Dict[str, Any]:
        if self._sem is None:
            self._sem = asyncio.Semaphore(4)
        async with self._sem:
            ya = self.leer(clave)
            if ya:
                return ya[1]

            # 1) Primario: Edge Neural TTS (gratis, sin cuota, natural, WordBoundary exacto)
            if self.edge_disponible:
                try:
                    mp3, palabras = await self._sintetiza_edge(texto, voz)
                    meta = _mapa_desde_palabras(texto, palabras, cortes, bytes_mp3=len(mp3))
                    meta.update({
                        "motor": f"edge:{voz}",
                        "voz": voz,
                        "clave": clave,
                        "mime": "audio/mpeg",
                    })
                    await asyncio.to_thread(self._guardar, clave, mp3, meta, "mp3")
                    return meta
                except Exception:  # noqa: BLE001 - pasa al respaldo Gemini si existe
                    if not self._api_key:
                        raise

            # 2) Respaldo: Gemini TTS si hay clave y cuota
            voz_g = normalize_gemini_voice("Kore", "Kore")
            wav, modelo = await asyncio.to_thread(self._sintetiza_gemini_bloqueante, texto, voz_g)
            pcm, sr = _extract_wav_pcm16_mono(wav)
            if pcm:
                rec, _ = _trim_pcm16_edge_silence(pcm, sr, max_lead_ms=30, max_tail_ms=160)
                wav, _ = ensure_wav_and_duration_ms(rec, f"audio/L16;codec=pcm;rate={sr}")
            meta = self._alinear(wav, texto, cortes)
            meta.update({
                "motor": f"gemini:{modelo}",
                "voz": voz_g,
                "clave": clave,
                "mime": "audio/wav",
            })
            await asyncio.to_thread(self._guardar, clave, wav, meta, "wav")
            return meta

    async def pedir(self, texto: str, cortes: Sequence[int] = (), voz: str = "", urgente: bool = True) -> Dict[str, Any]:
        """Devuelve metadatos con `clave` si hay audio neural, o `motor=local` solo sin conexión."""
        texto = " ".join((texto or "").split())
        if not texto:
            return {"motor": "silencio", "dur_ms": 0, "mapa": [[0, 0]], "clave": ""}
        v = normalizar_voz(voz, self.voz)
        clave = _clave(texto, v)
        ya = self.leer(clave)
        if ya:
            return {**ya[1], "cache": True}
        if not self.modelos_disponibles():
            return {"motor": "local", "motivo": "sin motores de voz disponibles", "clave": ""}
        tarea = self._en_curso.get(clave)
        if tarea is None or tarea.done():
            tarea = asyncio.create_task(self._fabricar(clave, texto, list(cortes) or [0], v))
            self._en_curso[clave] = tarea
            tarea.add_done_callback(lambda t, k=clave: self._en_curso.pop(k, None))
        try:
            meta = await asyncio.wait_for(asyncio.shield(tarea), timeout=ESPERA_URGENTE_S if urgente else 60.0)
            return {**meta, "cache": False}
        except asyncio.TimeoutError:
            return {"motor": "local", "motivo": "el servicio de voz tardó demasiado", "clave": ""}
        except Exception as exc:  # noqa: BLE001
            return {"motor": "local", "motivo": sanitize_text(str(exc))[:160], "clave": ""}
