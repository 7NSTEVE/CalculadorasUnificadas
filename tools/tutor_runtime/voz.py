"""Voz de las lecciones: arquitectura modular multi-motor con caché en disco y sincronización exacta.

Motores intercambiables (todos compatibles con el normalizador matemático `normalizador_voz.py`
y estilos prosódicos pedagógicos `normal`, `despacio`, `alerta`, `enfasis`):

  1. `edge` (por defecto, gratis, sin clave ni cuota): Microsoft Edge Neural TTS (`edge-tts`) con
     10 voces neuronales en español/multilingüe y marcas exactas `WordBoundary` al milisegundo.
  2. `local_http` (100% local en GPU AMD/Vulkan o CPU, OpenAI-compatible `/v1/audio/speech`):
     conecta directamente con servidores locales en loopback como `ZONOS3-Mini` (`zonos3-server`
     C++/ggml Vulkan), `VoxCPM2`, `Qwen3-TTS 1.7B` o `Kokoro-FastAPI`, con alineación RMS exacta.
  3. `kokoro` (100% local sin internet en ONNX CPU): se activa automáticamente si `kokoro_onnx` y
     los pesos (`kokoro-v1.0.onnx` + `voices-v1.0.bin`) están en `D:\\DevTools\\TTSModels\\kokoro`
     o en `TUTOR_KOKORO_DIR` (`ef_dora`, `em_alex`, `em_santa`).
  4. `gemini`: respaldo en nube si hay API key y cuota.
  5. `local`: último respaldo sin conexión usando la voz del sistema operativo.
"""

from __future__ import annotations

import asyncio
import hashlib
import io
import json
import os
import pathlib
import re
import threading
import time
import unicodedata
import urllib.parse
import urllib.request
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
from .normalizador_voz import detectar_estilo_pedagogico, normalizar_para_voz
from .secrets_dpapi import sanitize_text

try:
    import edge_tts as _edge_tts
except Exception:  # pragma: no cover
    _edge_tts = None  # type: ignore[assignment]

try:
    import kokoro_onnx as _kokoro_onnx  # type: ignore[import-not-found]
except Exception:
    _kokoro_onnx = None

VERSION_CACHE = "v4"
TOPE_CACHE_MB = 600
ESPERA_URGENTE_S = 12.0
VOZ_DEFECTO = "es-MX-DaliaNeural"
DEFAULT_LOCAL_TTS_URL = "http://127.0.0.1:8880/v1/audio/speech"

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

VOCES_KOKORO: List[Dict[str, str]] = [
    {"id": "ef_dora", "etiqueta": "Dora (Kokoro-82M local · femenina)", "genero": "F"},
    {"id": "em_alex", "etiqueta": "Alex (Kokoro-82M local · masculina)", "genero": "M"},
    {"id": "em_santa", "etiqueta": "Santa (Kokoro-82M local · masculina)", "genero": "M"},
]

CATALOGO_LOCAL: List[Dict[str, str]] = [
    {"id": "zonos3-mini", "nombre": "ZONOS3-Mini", "licencia": "Apache 2.0 / MIT", "backend": "C++/ggml Vulkan (OpenAI /v1/audio/speech)"},
    {"id": "voxcpm2", "nombre": "VoxCPM2", "licencia": "Apache 2.0", "backend": "llama.cpp-omni Vulkan (48 kHz)"},
    {"id": "qwen3-tts", "nombre": "Qwen3-TTS 1.7B", "licencia": "Apache 2.0", "backend": "GGUF / Vulkan"},
    {"id": "chatterbox-latam", "nombre": "Chatterbox V3 LATAM", "licencia": "MIT", "backend": "PyTorch / servidor local"},
    {"id": "kokoro-82m", "nombre": "Kokoro-82M", "licencia": "Apache 2.0", "backend": "ONNX CPU local (ef_dora, em_alex, em_santa)"},
]

_IDS_NEURALES = {v["id"] for v in VOCES_NEURALES}
_IDS_KOKORO = {v["id"] for v in VOCES_KOKORO}

# Mapeo suave de nombres legados de Gemini -> voces neuronales equivalentes
MAPA_VOCES_LEGADO: Dict[str, str] = {
    "Kore": "es-MX-DaliaNeural",
    "Aoede": "es-CO-SalomeNeural",
    "Charon": "es-MX-JorgeNeural",
    "Puck": "es-CR-JuanNeural",
    "Fenrir": "es-CO-GonzaloNeural",
}

# Ajustes prosódicos por estilo pedagógico
ESTILOS_PROSODIA: Dict[str, Dict[str, Any]] = {
    "normal": {"edge_rate": "+0%", "edge_pitch": "+0Hz", "speed": 1.0},
    "despacio": {"edge_rate": "-10%", "edge_pitch": "-1Hz", "speed": 0.90},
    "alerta": {"edge_rate": "-6%", "edge_pitch": "-2Hz", "speed": 0.94},
    "enfasis": {"edge_rate": "-3%", "edge_pitch": "+1Hz", "speed": 0.97},
}

_LOOPBACK_HOSTS = {"127.0.0.1", "localhost", "::1"}


def validar_url_loopback(url: str) -> str:
    """Valida que una URL de servidor TTS local apunte estrictamente a loopback (evita SSRF)."""
    u = (url or "").strip()
    if not u:
        return ""
    parsed = urllib.parse.urlparse(u)
    if parsed.scheme not in ("http", "https"):
        raise ValueError("La URL del servidor local de voz debe usar http:// o https://")
    host = (parsed.hostname or "").lower()
    if host not in _LOOPBACK_HOSTS:
        raise ValueError("Por seguridad, el servidor local de voz solo puede apuntar a 127.0.0.1 o localhost")
    return u


def normalizar_voz(voz: str, por_defecto: str = VOZ_DEFECTO) -> str:
    """Devuelve un identificador de voz neuronal válido (o mapea nombres legados)."""
    raw = (voz or "").strip()
    if raw in _IDS_NEURALES:
        return raw
    if raw in MAPA_VOCES_LEGADO:
        return MAPA_VOCES_LEGADO[raw]
    if re.fullmatch(r"[a-z]{2}-[A-Z]{2}-[A-Za-z0-9]+Neural", raw):
        return raw
    if por_defecto in _IDS_NEURALES:
        return por_defecto
    return MAPA_VOCES_LEGADO.get(por_defecto, VOZ_DEFECTO)


def _clave(texto: str, motor: str, voz: str, estilo: str) -> str:
    return hashlib.sha1(f"{VERSION_CACHE}|{motor}|{voz}|{estilo}|{texto}".encode("utf-8")).hexdigest()[:24]


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


def _buscar_pesos_kokoro(dir_explicito: str = "") -> Optional[Tuple[pathlib.Path, pathlib.Path]]:
    candidatos = [
        pathlib.Path(dir_explicito) if dir_explicito else None,
        pathlib.Path(os.environ.get("TUTOR_KOKORO_DIR", "")) if os.environ.get("TUTOR_KOKORO_DIR") else None,
        pathlib.Path(r"D:\DevTools\TTSModels\kokoro"),
        pathlib.Path.home() / ".cache" / "kokoro-onnx",
    ]
    for base in candidatos:
        if base is None or not base.is_dir():
            continue
        m = base / "kokoro-v1.0.onnx"
        v = base / "voices-v1.0.bin"
        if m.is_file() and v.is_file():
            return m, v
    return None


class ServicioVoz:
    def __init__(
        self,
        api_key: str,
        carpeta: Optional[pathlib.Path],
        voz: str = VOZ_DEFECTO,
        modelos: Sequence[str] = tuple(TTS_MODEL_CHAIN),
        local_tts_url: str = "",
        kokoro_dir: str = "",
    ) -> None:
        self.voz = normalizar_voz(voz, VOZ_DEFECTO)
        self.carpeta = carpeta
        if carpeta is not None:
            carpeta.mkdir(parents=True, exist_ok=True)
        self._cliente = None
        self._api_key = (api_key or "").strip()
        self._modelos = list(modelos)
        env_url = (local_tts_url or os.environ.get("TUTOR_LOCAL_TTS_URL") or "").strip()
        self._local_tts_url = validar_url_loopback(env_url) if env_url else ""
        self._local_tts_model = (os.environ.get("TUTOR_LOCAL_TTS_MODEL") or "zonos3-mini").strip()
        self._kokoro_dir = kokoro_dir
        self._kokoro_inst = None
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
    def kokoro_disponible(self) -> bool:
        return _kokoro_onnx is not None and _buscar_pesos_kokoro(self._kokoro_dir) is not None

    @property
    def configurada(self) -> bool:
        return self.edge_disponible or bool(self._local_tts_url) or self.kokoro_disponible or bool(self._api_key)

    def _cli(self):
        if self._cliente is None and self._api_key:
            from google import genai

            self._cliente = genai.Client(api_key=self._api_key)
        return self._cliente

    def modelos_disponibles(self) -> List[str]:
        ahora = time.time()
        mods: List[str] = []
        if self._local_tts_url:
            mods.append("local-http")
        if self.edge_disponible:
            mods.append("edge-tts")
        if self.kokoro_disponible:
            mods.append("kokoro-onnx")
        if self._api_key:
            mods.extend(m for m in self._modelos if self._apartado.get(m, 0) <= ahora)
        return mods

    def estado(self) -> Dict[str, Any]:
        primario = (
            "local-http"
            if self._local_tts_url
            else ("edge-tts" if self.edge_disponible else ("kokoro-onnx" if self.kokoro_disponible else ("gemini" if self._api_key else "local")))
        )
        return {
            "configurada": self.configurada,
            "voz": self.voz,
            "motor_primario": primario,
            "voces": VOCES_NEURALES,
            "voces_kokoro": VOCES_KOKORO,
            "kokoro_listo": self.kokoro_disponible,
            "servidor_local_url": self._local_tts_url or DEFAULT_LOCAL_TTS_URL,
            "servidor_local_activo": bool(self._local_tts_url),
            "catalogo_local": CATALOGO_LOCAL,
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

    async def _sintetiza_edge(self, texto: str, voz: str, estilo: str) -> Tuple[bytes, List[Tuple[int, int, str]]]:
        if _edge_tts is None:
            raise RuntimeError("edge-tts no instalado")
        pros = ESTILOS_PROSODIA.get(estilo, ESTILOS_PROSODIA["normal"])
        ultimo: Optional[Exception] = None
        for intento in range(2):
            try:
                comm = _edge_tts.Communicate(
                    texto,
                    voice=voz,
                    rate=str(pros["edge_rate"]),
                    pitch=str(pros["edge_pitch"]),
                    boundary="WordBoundary",
                )
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

    def _sintetiza_local_http_bloqueante(
        self,
        texto: str,
        voz: str,
        estilo: str,
        url: str,
    ) -> Tuple[bytes, str]:
        """Llama a un servidor TTS local OpenAI-compatible (`/v1/audio/speech`), p. ej. ZONOS3-Mini Vulkan."""
        url_ok = validar_url_loopback(url)
        pros = ESTILOS_PROSODIA.get(estilo, ESTILOS_PROSODIA["normal"])
        payload = json.dumps({
            "model": self._local_tts_model,
            "input": texto,
            "voice": voz or "es-default",
            "language": "es",
            "response_format": "wav",
            "speed": float(pros["speed"]),
        }).encode("utf-8")
        req = urllib.request.Request(
            url_ok,
            data=payload,
            headers={"Content-Type": "application/json", "Accept": "audio/wav, audio/mpeg, */*"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=18.0) as resp:
            datos = resp.read()
            ctype = str(resp.headers.get("Content-Type") or "audio/wav").split(";")[0].strip().lower()
        if len(datos) < 100:
            raise RuntimeError("servidor local de voz devolvió audio vacío")
        if ctype in ("audio/mpeg", "audio/mp3"):
            return datos, "audio/mpeg"
        wav, _ = ensure_wav_and_duration_ms(datos, ctype or "audio/wav")
        return wav, "audio/wav"

    def _sintetiza_kokoro_bloqueante(self, texto: str, voz: str, estilo: str) -> bytes:
        if _kokoro_onnx is None:
            raise RuntimeError("kokoro-onnx no instalado")
        pesos = _buscar_pesos_kokoro(self._kokoro_dir)
        if pesos is None:
            raise RuntimeError("faltan kokoro-v1.0.onnx y voices-v1.0.bin en D:\\DevTools\\TTSModels\\kokoro")
        if self._kokoro_inst is None:
            self._kokoro_inst = _kokoro_onnx.Kokoro(str(pesos[0]), str(pesos[1]))
        import soundfile as sf

        pros = ESTILOS_PROSODIA.get(estilo, ESTILOS_PROSODIA["normal"])
        v_k = voz if voz in _IDS_KOKORO else "ef_dora"
        samples, sr = self._kokoro_inst.create(texto, voice=v_k, speed=float(pros["speed"]), lang="es")
        buf = io.BytesIO()
        sf.write(buf, samples, int(sr), format="WAV", subtype="PCM_16")
        return buf.getvalue()

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
    def _alinear_wav(wav: bytes, texto: str, cortes: Sequence[int]) -> Dict[str, Any]:
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

    def _alinear_y_recortar_wav(self, wav: bytes, texto: str, cortes: Sequence[int]) -> Tuple[bytes, Dict[str, Any]]:
        pcm, sr = _extract_wav_pcm16_mono(wav)
        if pcm:
            rec, _ = _trim_pcm16_edge_silence(pcm, sr, max_lead_ms=30, max_tail_ms=160)
            wav, _ = ensure_wav_and_duration_ms(rec, f"audio/L16;codec=pcm;rate={sr}")
        return wav, self._alinear_wav(wav, texto, cortes)

    async def _fabricar(
        self,
        clave: str,
        texto_hablado: str,
        texto_original: str,
        cortes: Sequence[int],
        voz: str,
        motor_pref: str,
        estilo: str,
        url_local: str,
    ) -> Dict[str, Any]:
        if self._sem is None:
            self._sem = asyncio.Semaphore(4)
        async with self._sem:
            ya = self.leer(clave)
            if ya:
                return ya[1]

            orden: List[str] = []
            if motor_pref == "local_http":
                orden = ["local_http", "edge", "gemini"]
            elif motor_pref == "kokoro":
                orden = ["kokoro", "edge", "gemini"]
            elif motor_pref == "gemini":
                orden = ["gemini", "edge"]
            elif motor_pref == "edge":
                orden = ["edge", "gemini"]
            else:
                if url_local:
                    orden.append("local_http")
                if self.edge_disponible:
                    orden.append("edge")
                if self.kokoro_disponible:
                    orden.append("kokoro")
                if self._api_key:
                    orden.append("gemini")

            errores: List[str] = []
            for m in orden:
                try:
                    if m == "local_http":
                        if not url_local:
                            continue
                        datos, mime = await asyncio.to_thread(
                            self._sintetiza_local_http_bloqueante, texto_hablado, voz, estilo, url_local
                        )
                        if mime == "audio/wav":
                            datos, meta = await asyncio.to_thread(
                                self._alinear_y_recortar_wav, datos, texto_original, cortes
                            )
                            ext = "wav"
                        else:
                            meta = _mapa_desde_palabras(texto_original, [], cortes, bytes_mp3=len(datos))
                            ext = "mp3"
                        meta.update({
                            "motor": f"local_http:{self._local_tts_model}",
                            "voz": voz,
                            "estilo": estilo,
                            "clave": clave,
                            "mime": mime,
                        })
                        await asyncio.to_thread(self._guardar, clave, datos, meta, ext)
                        return meta

                    if m == "edge" and self.edge_disponible:
                        v_edge = normalizar_voz(voz, self.voz)
                        mp3, palabras = await self._sintetiza_edge(texto_hablado, v_edge, estilo)
                        meta = _mapa_desde_palabras(texto_original, palabras, cortes, bytes_mp3=len(mp3))
                        meta.update({
                            "motor": f"edge:{v_edge}",
                            "voz": v_edge,
                            "estilo": estilo,
                            "clave": clave,
                            "mime": "audio/mpeg",
                        })
                        await asyncio.to_thread(self._guardar, clave, mp3, meta, "mp3")
                        return meta

                    if m == "kokoro" and self.kokoro_disponible:
                        v_k = voz if voz in _IDS_KOKORO else "ef_dora"
                        wav = await asyncio.to_thread(self._sintetiza_kokoro_bloqueante, texto_hablado, v_k, estilo)
                        wav, meta = await asyncio.to_thread(
                            self._alinear_y_recortar_wav, wav, texto_original, cortes
                        )
                        meta.update({
                            "motor": f"kokoro:{v_k}",
                            "voz": v_k,
                            "estilo": estilo,
                            "clave": clave,
                            "mime": "audio/wav",
                        })
                        await asyncio.to_thread(self._guardar, clave, wav, meta, "wav")
                        return meta

                    if m == "gemini" and self._api_key:
                        voz_g = normalize_gemini_voice("Kore", "Kore")
                        wav, modelo = await asyncio.to_thread(self._sintetiza_gemini_bloqueante, texto_hablado, voz_g)
                        wav, meta = await asyncio.to_thread(
                            self._alinear_y_recortar_wav, wav, texto_original, cortes
                        )
                        meta.update({
                            "motor": f"gemini:{modelo}",
                            "voz": voz_g,
                            "estilo": estilo,
                            "clave": clave,
                            "mime": "audio/wav",
                        })
                        await asyncio.to_thread(self._guardar, clave, wav, meta, "wav")
                        return meta
                except Exception as exc:  # noqa: BLE001
                    errores.append(f"{m}: {sanitize_text(str(exc))[:140]}")

            self._ultimo_error = " | ".join(errores)[:400] or "sin motores de voz disponibles"
            raise RuntimeError(self._ultimo_error)

    async def pedir(
        self,
        texto: str,
        cortes: Sequence[int] = (),
        voz: str = "",
        urgente: bool = True,
        motor: str = "",
        estilo: str = "",
        servidor_local: str = "",
    ) -> Dict[str, Any]:
        """Devuelve metadatos con `clave` si hay audio neural/local, o `motor=local` solo sin conexión."""
        texto_orig = " ".join((texto or "").split())
        if not texto_orig:
            return {"motor": "silencio", "dur_ms": 0, "mapa": [[0, 0]], "clave": ""}

        motor_pref = (motor or "").strip().lower()
        if motor_pref not in ("", "auto", "edge", "local_http", "kokoro", "gemini"):
            motor_pref = "auto"

        url_local = self._local_tts_url
        if servidor_local and motor_pref in ("local_http", "auto"):
            try:
                url_local = validar_url_loopback(servidor_local)
            except ValueError as exc:
                return {"motor": "local", "motivo": str(exc), "clave": ""}

        estilo_eff = detectar_estilo_pedagogico(texto_orig, estilo)
        texto_hablado = normalizar_para_voz(texto_orig) or texto_orig

        if motor_pref == "local_http":
            v_eff = (voz or "es-default").strip()[:60] or "es-default"
        elif motor_pref == "kokoro":
            v_eff = voz if voz in _IDS_KOKORO else "ef_dora"
        else:
            v_eff = normalizar_voz(voz, self.voz)

        clave = _clave(texto_hablado, motor_pref or "auto", v_eff, estilo_eff)
        ya = self.leer(clave)
        if ya:
            return {**ya[1], "cache": True}

        if not self.modelos_disponibles() and not url_local:
            return {"motor": "local", "motivo": "sin motores de voz disponibles", "clave": ""}

        tarea = self._en_curso.get(clave)
        if tarea is None or tarea.done():
            tarea = asyncio.create_task(
                self._fabricar(clave, texto_hablado, texto_orig, list(cortes) or [0], v_eff, motor_pref, estilo_eff, url_local)
            )
            self._en_curso[clave] = tarea
            tarea.add_done_callback(lambda t, k=clave: self._en_curso.pop(k, None))
        try:
            meta = await asyncio.wait_for(asyncio.shield(tarea), timeout=ESPERA_URGENTE_S if urgente else 60.0)
            return {**meta, "cache": False}
        except asyncio.TimeoutError:
            return {"motor": "local", "motivo": "el servicio de voz tardó demasiado", "clave": ""}
        except Exception as exc:  # noqa: BLE001
            return {"motor": "local", "motivo": sanitize_text(str(exc))[:160], "clave": ""}
