"""Gemini Cloud Engines (Visual Director + Gemini 3.8 Live Audio + Gemini Native Spanish TTS).

Uses keys stored encrypted via Windows DPAPI in private_local/tutor_secrets.dpapi:
- gemini_visual_key -> fast multimodal JSON whiteboard beats, circle-to-ask OCR, provenance & steps
- gemini_audio_key  -> real-time bidirectional Gemini 3.8 Live audio + NON_BLOCKING whiteboard tools
                       and Gemini Native TTS (gemini-3.8-flash-lite-tts / gemini-2.5-flash-preview-tts)
"""

from __future__ import annotations

import array
import asyncio
import base64
import io
import json
import math
import re
import sys
import time
import uuid
import wave
from typing import Any, Awaitable, Callable, Dict, List, Optional, Tuple


class _ModuloPerezoso:
    """Importa un módulo pesado la primera vez que se usa uno de sus nombres.

    `google-genai` tarda varios segundos en importarse (arrastra requests, httpx, mcp…) y el tutor
    solo lo necesita al llamar a Gemini. Importarlo al arrancar hacía que CuadernU se rindiera
    esperando el motor ("Timeout esperando arranque del Tutor Runtime").
    """

    def __init__(self, nombre: str) -> None:
        self.__dict__["_nombre"] = nombre
        self.__dict__["_modulo"] = None

    def __getattr__(self, atributo: str):
        if atributo.startswith("__"):
            raise AttributeError(atributo)
        modulo = self.__dict__.get("_modulo")
        if modulo is None:
            import importlib

            modulo = importlib.import_module(self.__dict__["_nombre"])
            self.__dict__["_modulo"] = modulo
        return getattr(modulo, atributo)


genai = _ModuloPerezoso("google.genai")
types = _ModuloPerezoso("google.genai.types")

_SIN_CREAR = object()


class _ClienteGeminiPerezoso:
    """Atributo `_client` que crea `genai.Client` la primera vez que se usa, no al construir.

    Con una clave guardada, crear los clientes al arrancar importaba `google-genai` igual y
    alargaba el arranque del motor. Sin clave vale `None`, como antes.
    """

    def __get__(self, obj, owner=None):
        if obj is None:
            return self
        cliente = obj.__dict__.get("_client_creado", _SIN_CREAR)
        if cliente is _SIN_CREAR:
            clave = getattr(obj, "_api_key", "") or ""
            cliente = genai.Client(api_key=clave) if clave else None
            obj.__dict__["_client_creado"] = cliente
        return cliente

    def __set__(self, obj, valor) -> None:
        obj.__dict__["_client_creado"] = valor

from .secrets_dpapi import sanitize_text
from .sympy_verifier import build_verified_integral_plan, verify_student_step

VISUAL_MODEL_DEFAULT = "gemini-4-hydrogen"

VISUAL_MODEL_CHAIN = [
    "gemini-4-hydrogen",
    "gemini-3.8-flash",
    "gemini-3.7-flash",
    "gemini-3.6-flash",
    "gemini-3.5-flash",
    "gemini-3.5-flash-lite",
    "gemini-3.1-flash-lite",
    "gemma-4-31b-it",
]

LEGACY_VISUAL_MODEL_UPGRADE = {
    "gemini-2.5-flash": VISUAL_MODEL_DEFAULT,
    "gemini-2.5-pro": VISUAL_MODEL_DEFAULT,
    "gemini-2.5-flash-lite": "gemini-3.5-flash-lite",
    "gemini-2.0-flash": VISUAL_MODEL_DEFAULT,
}

_VALID_MODEL_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{2,63}$")


def normalizar_modelo_visual(modelo: Optional[str]) -> str:
    """Normaliza un ID de modelo visual y sustituye modelos heredados (2.5/2.0) por Gemini 4 Hydrogen."""
    m = (modelo or "").strip()
    if m.startswith("models/"):
        m = m[7:]
    if not m or not _VALID_MODEL_ID_RE.match(m):
        return VISUAL_MODEL_DEFAULT
    return LEGACY_VISUAL_MODEL_UPGRADE.get(m, m)


TTS_MODEL_CHAIN = [
    "gemini-3.8-flash-lite-tts",
    "gemini-3.8-flash-tts",
]

VALID_GEMINI_VOICES = {
    "Kore",
    "Aoede",
    "Puck",
    "Charon",
    "Fenrir",
    "Leda",
    "Orus",
    "Zephyr",
}

LIVE_AUDIO_MODEL_PRIMARY = "gemini-3.8-live"
LIVE_AUDIO_MODEL_FALLBACK = "gemini-3.8-live-extended-thinking"

_CUE_RE = re.compile(r"<cue\s+id=['\"]([A-Za-z0-9_\-]+)['\"]\s*/?>")


def parse_cue_markup(speech_markup: str) -> Tuple[str, List[Tuple[str, int]]]:
    """Strip <cue id='...'/> tags and return (clean_text, [(cue_id, char_offset)])."""
    if not speech_markup:
        return "", []
    cues: List[Tuple[str, int]] = []
    out_parts: List[str] = []
    pos = 0
    clean_len = 0
    for m in _CUE_RE.finditer(speech_markup):
        chunk = speech_markup[pos : m.start()]
        out_parts.append(chunk)
        clean_len += len(chunk)
        cues.append((m.group(1), clean_len))
        pos = m.end()
    out_parts.append(speech_markup[pos:])
    raw_clean = "".join(out_parts)
    # Normalize double whitespace while tracking cue offsets safely
    clean_text = re.sub(r"\s{2,}", " ", raw_clean).strip()
    scale = (len(clean_text) / max(1, len(raw_clean))) if raw_clean else 1.0
    adjusted = [(cid, min(len(clean_text), int(round(off * scale)))) for cid, off in cues]
    return clean_text, adjusted


def estimate_cues_fallback(clean_text: str, cues: List[Tuple[str, int]], speech_rate: float = 0.96) -> Dict[str, Any]:
    """Deterministic timing estimation when TTS engine provides no word boundaries."""
    words = clean_text.split()
    ms_per_word = int(round(390.0 / max(0.75, min(1.35, speech_rate))))
    word_ts: List[Dict[str, Any]] = []
    char_cursor = 0
    t_ms = 0
    for w in words:
        idx = clean_text.find(w, char_cursor)
        if idx < 0:
            idx = char_cursor
        dur = max(160, int(ms_per_word * (0.65 + 0.08 * len(w))))
        word_ts.append({"w": w, "start_ms": t_ms, "end_ms": t_ms + dur, "char_idx": idx})
        char_cursor = idx + len(w)
        t_ms += dur

    total_ms = max(600, t_ms)
    cues_ms: Dict[str, int] = {}
    for cid, coff in cues:
        ratio = coff / max(1, len(clean_text))
        cues_ms[cid] = int(round(ratio * total_ms))
    return {"cues_ms": cues_ms, "words": word_ts, "duration_ms": total_ms}


def ensure_wav_and_duration_ms(raw_bytes: bytes, mime_type: str = "") -> Tuple[bytes, int]:
    """Ensure audio bytes are a valid RIFF/WAVE container and return (wav_bytes, duration_ms)."""
    if not raw_bytes:
        return b"", 0
    if raw_bytes[:4] == b"RIFF" and len(raw_bytes) > 44:
        try:
            with wave.open(io.BytesIO(raw_bytes), "rb") as wf:
                frames = wf.getnframes()
                rate = wf.getframerate() or 24000
                dur_ms = max(250, int(round((frames / float(rate)) * 1000.0)))
                return raw_bytes, dur_ms
        except Exception:
            pass

    # Wrap raw PCM16 mono (e.g., audio/L16;codec=pcm;rate=24000) into a standard WAV container
    sample_rate = 24000
    m = re.search(r"rate=(\d+)", mime_type or "", re.IGNORECASE)
    if m:
        sample_rate = max(8000, min(96000, int(m.group(1))))
    if len(raw_bytes) % 2 == 1:
        raw_bytes = raw_bytes[:-1]
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(raw_bytes)
    nframes = len(raw_bytes) // 2
    dur_ms = max(250, int(round((nframes / float(sample_rate)) * 1000.0)))
    return buf.getvalue(), dur_ms


def rescale_alignment_to_duration(est: Dict[str, Any], real_duration_ms: int) -> Dict[str, Any]:
    """Scale proportional word and <cue> timestamps to match real synthesized WAV duration."""
    target_ms = max(300, int(real_duration_ms))
    est_ms = max(1, int(est.get("duration_ms") or target_ms))
    scale = target_ms / float(est_ms)
    scaled_cues = {
        cid: min(target_ms, max(0, int(round(ms * scale))))
        for cid, ms in (est.get("cues_ms") or {}).items()
    }
    scaled_words = []
    for w in est.get("words") or []:
        s_ms = min(target_ms, max(0, int(round(w["start_ms"] * scale))))
        e_ms = min(target_ms, max(s_ms + 40, int(round(w["end_ms"] * scale))))
        scaled_words.append({
            "w": w["w"],
            "start_ms": s_ms,
            "end_ms": e_ms,
            "char_idx": w["char_idx"],
        })
    return {
        "cues_ms": scaled_cues,
        "words": scaled_words,
        "duration_ms": target_ms,
    }


def _extract_wav_pcm16_mono(wav_bytes: bytes) -> Tuple[bytes, int]:
    """Extract raw mono PCM16 bytes and sample rate from a WAV container."""
    if not wav_bytes or len(wav_bytes) <= 44 or wav_bytes[:4] != b"RIFF":
        return b"", 24000
    try:
        with wave.open(io.BytesIO(wav_bytes), "rb") as wf:
            ch = wf.getnchannels() or 1
            sw = wf.getsampwidth() or 2
            sr = wf.getframerate() or 24000
            frames = wf.readframes(wf.getnframes())
            if sw != 2:
                return b"", sr
            if ch == 1:
                return frames, sr
            arr = array.array("h")
            arr.frombytes(frames)
            if sys.byteorder != "little":
                arr.byteswap()
            mono = array.array("h", arr[::ch])
            if sys.byteorder != "little":
                mono.byteswap()
            return mono.tobytes(), sr
    except Exception:
        return b"", 24000


def _trim_pcm16_edge_silence(
    pcm_bytes: bytes,
    sample_rate: int = 24000,
    max_lead_ms: int = 40,
    max_tail_ms: int = 120,
) -> Tuple[bytes, int]:
    """Trim excessive dead silence at the start/end of synthesized PCM16 so voice begins instantaneously."""
    if not pcm_bytes or len(pcm_bytes) < 64:
        return pcm_bytes, 0
    arr = array.array("h")
    arr.frombytes(pcm_bytes[: len(pcm_bytes) - (len(pcm_bytes) % 2)])
    if sys.byteorder != "little":
        arr.byteswap()
    n = len(arr)
    if n < 240:
        return pcm_bytes, 0

    win = max(1, int(sample_rate * 0.01))  # 10 ms windows
    rms_vals: List[float] = []
    peak = 0.0
    for start in range(0, n, win):
        end = min(n, start + win)
        s = 0.0
        for v in arr[start:end]:
            s += float(v) * float(v)
        r = math.sqrt(s / max(1, end - start))
        rms_vals.append(r)
        if r > peak:
            peak = r

    if peak < 150.0:
        return pcm_bytes, 0

    thresh = max(120.0, peak * 0.045)
    first_active = 0
    for idx, r in enumerate(rms_vals):
        if r >= thresh:
            first_active = idx
            break
    last_active = len(rms_vals) - 1
    for idx in range(len(rms_vals) - 1, -1, -1):
        if rms_vals[idx] >= thresh:
            last_active = idx
            break

    keep_lead_wins = max(0, max_lead_ms // 10)
    keep_tail_wins = max(0, max_tail_ms // 10)
    cut_start_win = max(0, first_active - keep_lead_wins)
    cut_end_win = min(len(rms_vals), last_active + 1 + keep_tail_wins)
    start_sample = cut_start_win * win
    end_sample = min(n, cut_end_win * win)
    if end_sample <= start_sample + win:
        return pcm_bytes, 0

    trimmed = arr[start_sample:end_sample]
    if sys.byteorder != "little":
        trimmed.byteswap()
    return trimmed.tobytes(), int(round((start_sample / float(sample_rate)) * 1000.0))


def align_cues_with_wav_rms(
    wav_bytes: bytes,
    clean_text: str,
    cues: List[Tuple[str, int]],
    fallback_est: Dict[str, Any],
    pinned_cues_ms: Optional[Dict[str, int]] = None,
) -> Dict[str, Any]:
    """Align <cue> and word timestamps to real acoustic clause boundaries using 20 ms RMS valleys on WAV PCM16."""
    pcm_bytes, sr = _extract_wav_pcm16_mono(wav_bytes)
    if not pcm_bytes:
        return fallback_est

    arr = array.array("h")
    arr.frombytes(pcm_bytes[: len(pcm_bytes) - (len(pcm_bytes) % 2)])
    if sys.byteorder != "little":
        arr.byteswap()
    n_samples = len(arr)
    real_dur_ms = max(250, int(round((n_samples / float(sr)) * 1000.0)))

    win_ms = 20
    win = max(1, int(sr * (win_ms / 1000.0)))
    rms_frames: List[float] = []
    peak_rms = 0.0
    for s in range(0, n_samples, win):
        e = min(n_samples, s + win)
        acc = 0.0
        for v in arr[s:e]:
            acc += float(v) * float(v)
        r = math.sqrt(acc / max(1, e - s))
        rms_frames.append(r)
        if r > peak_rms:
            peak_rms = r

    if peak_rms < 120.0 or not rms_frames:
        return rescale_alignment_to_duration(fallback_est, real_dur_ms)

    active_thresh = max(110.0, peak_rms * 0.055)
    valley_thresh = max(130.0, peak_rms * 0.085)

    first_f = 0
    for idx, r in enumerate(rms_frames):
        if r >= active_thresh:
            first_f = idx
            break
    last_f = len(rms_frames) - 1
    for idx in range(len(rms_frames) - 1, -1, -1):
        if rms_frames[idx] >= active_thresh:
            last_f = idx
            break

    speech_start_ms = first_f * win_ms
    speech_end_ms = min(real_dur_ms, (last_f + 1) * win_ms)
    speech_span_ms = max(250, speech_end_ms - speech_start_ms)

    # Detect internal acoustic pause valleys (>= 100 ms of low RMS) -> clause onset is right at valley_end_ms
    valleys: List[Tuple[int, int, int]] = []  # (start_ms, end_ms, dur_ms)
    in_v = False
    v_start = 0
    for idx in range(first_f + 1, last_f):
        if rms_frames[idx] < valley_thresh:
            if not in_v:
                in_v = True
                v_start = idx
        else:
            if in_v:
                dur_frames = idx - v_start
                if dur_frames >= 5:  # >= 100 ms
                    valleys.append((v_start * win_ms, idx * win_ms, dur_frames * win_ms))
                in_v = False

    total_chars = max(1, len(clean_text))
    pinned = pinned_cues_ms or {}
    cues_ms: Dict[str, int] = {}
    ordered_cues = sorted(cues, key=lambda pair: pair[1])
    prev_cue_ms = speech_start_ms

    for cid, coff in ordered_cues:
        if cid in pinned:
            ms = max(prev_cue_ms, min(real_dur_ms, int(pinned[cid])))
            cues_ms[cid] = ms
            prev_cue_ms = ms
            continue
        if coff <= 2:
            cues_ms[cid] = speech_start_ms
            prev_cue_ms = max(prev_cue_ms, speech_start_ms)
            continue

        ratio = min(1.0, max(0.0, coff / float(total_chars)))
        prop_ms = speech_start_ms + int(round(ratio * speech_span_ms))

        # Check if cue sits right after punctuation (stronger expectation of a spoken breath pause)
        before_txt = clean_text[max(0, coff - 4) : coff].strip()
        after_punct = bool(before_txt and before_txt[-1] in ".?!;:,")
        search_rad = max(550, int(speech_span_ms * (0.28 if after_punct else 0.18)))

        best_ms = prop_ms
        best_score = float("inf")
        for _vs, ve, vdur in valleys:
            if ve < prev_cue_ms + 80:
                continue
            dist = abs(ve - prop_ms)
            if dist <= search_rad:
                # Favor closer valleys and slightly reward longer clause pauses
                score = dist - min(180.0, vdur * 0.35)
                if score < best_score:
                    best_score = score
                    best_ms = ve

        best_ms = max(prev_cue_ms, min(speech_end_ms, best_ms))
        cues_ms[cid] = best_ms
        prev_cue_ms = best_ms

    # Piecewise-interpolate word timestamps using pinned/RMS cue anchors
    anchors: List[Tuple[int, int]] = [(0, speech_start_ms)]
    for cid, coff in ordered_cues:
        if cid in cues_ms and coff > anchors[-1][0]:
            anchors.append((coff, cues_ms[cid]))
    if anchors[-1][0] < total_chars:
        anchors.append((total_chars, speech_end_ms))

    def _char_to_ms(ch_idx: int) -> int:
        if ch_idx <= anchors[0][0]:
            return anchors[0][1]
        for k in range(len(anchors) - 1):
            c0, t0 = anchors[k]
            c1, t1 = anchors[k + 1]
            if c0 <= ch_idx <= c1:
                span = max(1, c1 - c0)
                return int(round(t0 + ((ch_idx - c0) / float(span)) * (t1 - t0)))
        return anchors[-1][1]

    aligned_words: List[Dict[str, Any]] = []
    for w in fallback_est.get("words") or []:
        c_idx = int(w.get("char_idx", 0))
        w_len = max(1, len(str(w.get("w", ""))))
        s_ms = _char_to_ms(c_idx)
        e_ms = max(s_ms + 55, _char_to_ms(min(total_chars, c_idx + w_len)))
        aligned_words.append({
            "w": w["w"],
            "start_ms": min(real_dur_ms, s_ms),
            "end_ms": min(real_dur_ms, e_ms),
            "char_idx": c_idx,
        })

    return {
        "cues_ms": cues_ms,
        "words": aligned_words,
        "duration_ms": real_dur_ms,
    }


def _split_speech_for_parallel_tts(speech_markup: str) -> Optional[Tuple[str, str]]:
    """Split a multi-clause speech_markup into 2 balanced segments for parallel TTS + exact seam cue timing."""
    clean_text, cues = parse_cue_markup(speech_markup)
    if len(clean_text.split()) < 15:
        return None
    # Prefer splitting right before a middle <cue id='...'/> tag
    cue_matches = list(_CUE_RE.finditer(speech_markup))
    n = len(speech_markup)
    best_pos = -1
    best_dist = n
    for m in cue_matches:
        if 0.24 * n <= m.start() <= 0.76 * n:
            d = abs(m.start() - n * 0.5)
            if d < best_dist:
                best_dist = d
                best_pos = m.start()
    if best_pos < 0:
        # Fallback: split after sentence punctuation near the middle
        for m in re.finditer(r"[.?!;]\s+", speech_markup):
            pos = m.end()
            if 0.25 * n <= pos <= 0.75 * n:
                d = abs(pos - n * 0.5)
                if d < best_dist:
                    best_dist = d
                    best_pos = pos
    if best_pos <= 0:
        return None
    left = speech_markup[:best_pos].strip()
    right = speech_markup[best_pos:].strip()
    c_left, _ = parse_cue_markup(left)
    c_right, _ = parse_cue_markup(right)
    if len(c_left.split()) < 4 or len(c_right.split()) < 4:
        return None
    return left, right


def normalize_gemini_voice(voice_name: Optional[str], default: str = "Kore") -> str:
    v = (voice_name or "").strip()
    if v in VALID_GEMINI_VOICES:
        return v
    legacy_map = {
        "es-CR-MariaNeural": "Kore",
        "es-CR-JuanNeural": "Charon",
        "es-MX-DaliaNeural": "Aoede",
    }
    return legacy_map.get(v, default)


class AlignedSpanishTts:
    """Synthesizes natural Latin American Spanish audio via Gemini Native TTS with exact WAV cue alignment."""

    _client = _ClienteGeminiPerezoso()

    def __init__(self, api_key: str = "", voice: str = "Kore") -> None:
        self._api_key = (api_key or "").strip()
        self.voice = normalize_gemini_voice(voice, "Kore")

    @property
    def configured(self) -> bool:
        return bool(self._api_key)

    async def _synth_single_segment_wav(self, clean_text: str, chosen_voice: str) -> Tuple[bytes, int, str]:
        """Synthesize one text segment to WAV via Gemini Native TTS failover chain."""
        last_err = ""
        for model_name in TTS_MODEL_CHAIN:
            try:
                resp = await asyncio.to_thread(
                    self._client.models.generate_content,
                    model=model_name,
                    contents=clean_text,
                    config=types.GenerateContentConfig(
                        response_modalities=["AUDIO"],
                        speech_config=types.SpeechConfig(
                            voice_config=types.VoiceConfig(
                                prebuilt_voice_config=types.PrebuiltVoiceConfig(
                                    voice_name=chosen_voice
                                )
                            )
                        ),
                    ),
                )
                raw_chunks: List[bytes] = []
                mime_seen = ""
                for cand in getattr(resp, "candidates", None) or []:
                    content = getattr(cand, "content", None)
                    for part in getattr(content, "parts", None) or []:
                        inline = getattr(part, "inline_data", None)
                        if inline and getattr(inline, "data", None):
                            raw_chunks.append(inline.data)
                            if not mime_seen and getattr(inline, "mime_type", None):
                                mime_seen = str(inline.mime_type)
                raw_audio = b"".join(raw_chunks)
                if not raw_audio:
                    raise RuntimeError(f"Empty audio from {model_name}")
                wav_bytes, dur_ms = ensure_wav_and_duration_ms(raw_audio, mime_seen)
                return wav_bytes, dur_ms, model_name
            except Exception as exc:
                last_err = sanitize_text(str(exc))[:160]
                continue
        raise RuntimeError(last_err or "Gemini TTS unavailable")

    async def synthesize_with_cues(
        self,
        speech_markup: str,
        speech_rate: float = 0.96,
        voice_override: Optional[str] = None,
    ) -> Dict[str, Any]:
        clean_text, cues = parse_cue_markup(speech_markup)
        if not clean_text:
            return {
                "ok": True,
                "engine": "silent",
                "voice": self.voice,
                "plain_text": "",
                "audio_b64": "",
                "audio_mime": "",
                "audio_mp3_b64": "",
                "speech_rate": speech_rate,
                "cues_ms": {cid: 0 for cid, _ in cues},
                "words": [],
                "duration_ms": 0,
            }

        fallback = estimate_cues_fallback(clean_text, cues, speech_rate)
        chosen_voice = normalize_gemini_voice(voice_override, self.voice)

        if self._client is not None:
            try:
                split_segs = _split_speech_for_parallel_tts(speech_markup)
                pinned_cues: Dict[str, int] = {}
                used_model = ""
                if split_segs:
                    left_m, right_m = split_segs
                    left_clean, _ = parse_cue_markup(left_m)
                    right_clean, right_cues = parse_cue_markup(right_m)
                    (wav1, _d1, m1), (wav2, _d2, _m2) = await asyncio.gather(
                        self._synth_single_segment_wav(left_clean, chosen_voice),
                        self._synth_single_segment_wav(right_clean, chosen_voice),
                    )
                    used_model = m1
                    pcm1, sr1 = _extract_wav_pcm16_mono(wav1)
                    pcm2, sr2 = _extract_wav_pcm16_mono(wav2)
                    if pcm1 and pcm2 and sr1 == sr2:
                        pcm1_t, _ = _trim_pcm16_edge_silence(pcm1, sr1, max_lead_ms=30, max_tail_ms=90)
                        pcm2_t, _ = _trim_pcm16_edge_silence(pcm2, sr2, max_lead_ms=25, max_tail_ms=110)
                        breath = b"\x00\x00" * int(sr1 * 0.08)
                        seam_ms = int(round(((len(pcm1_t) // 2) + (len(breath) // 2)) / float(sr1) * 1000.0))
                        # Pin any cue right at the start of segment 2 to the exact acoustic seam!
                        for cid, coff in right_cues:
                            if coff <= 2:
                                pinned_cues[cid] = seam_ms
                        wav_bytes, _real_dur_ms = ensure_wav_and_duration_ms(
                            pcm1_t + breath + pcm2_t,
                            f"audio/L16;codec=pcm;rate={sr1}",
                        )
                    else:
                        wav_bytes = wav1
                else:
                    wav_raw, _dur, used_model = await self._synth_single_segment_wav(clean_text, chosen_voice)
                    pcm_raw, sr_raw = _extract_wav_pcm16_mono(wav_raw)
                    if pcm_raw:
                        pcm_t, _ = _trim_pcm16_edge_silence(pcm_raw, sr_raw, max_lead_ms=30, max_tail_ms=110)
                        wav_bytes, _ = ensure_wav_and_duration_ms(pcm_t, f"audio/L16;codec=pcm;rate={sr_raw}")
                    else:
                        wav_bytes = wav_raw

                aligned = align_cues_with_wav_rms(
                    wav_bytes,
                    clean_text,
                    cues,
                    fallback,
                    pinned_cues_ms=pinned_cues,
                )
                return {
                    "ok": True,
                    "engine": f"gemini_tts:{used_model}",
                    "voice": chosen_voice,
                    "plain_text": clean_text,
                    "audio_b64": base64.b64encode(wav_bytes).decode("ascii"),
                    "audio_mime": "audio/wav",
                    "audio_mp3_b64": "",
                    "speech_rate": speech_rate,
                    "cues_ms": aligned["cues_ms"],
                    "words": aligned["words"],
                    "duration_ms": aligned["duration_ms"],
                }
            except Exception as exc:
                last_err = sanitize_text(str(exc))[:160]
                return {
                    "ok": False,
                    "engine": "fallback_timing_only",
                    "voice": chosen_voice,
                    "error": last_err or "Gemini TTS unavailable",
                    "plain_text": clean_text,
                    "audio_b64": "",
                    "audio_mime": "",
                    "audio_mp3_b64": "",
                    "speech_rate": speech_rate,
                    "cues_ms": fallback["cues_ms"],
                    "words": fallback["words"],
                    "duration_ms": fallback["duration_ms"],
                }

        return {
            "ok": False,
            "engine": "fallback_timing_only",
            "voice": chosen_voice,
            "error": "TTS cloud key not configured",
            "plain_text": clean_text,
            "audio_b64": "",
            "audio_mime": "",
            "audio_mp3_b64": "",
            "speech_rate": speech_rate,
            "cues_ms": fallback["cues_ms"],
            "words": fallback["words"],
            "duration_ms": fallback["duration_ms"],
        }


_DIRECTOR_SYSTEM_PROMPT = """Eres un profesor particular humano UNIVERSAL (dominas TODAS las materias: física, cálculo, matemáticas, historia, geografía, estudios sociales, biología, química, programación, medicina, idiomas, derecho y cualquier PDF o imagen que suba el alumno) dibujando y escribiendo A MANO en una pizarra blanca interactiva para un estudiante en Costa Rica con pasos cortos.

REGLAS PEDAGÓGICAS Y VISUALES ESTRICTAS (PROHIBIDO VIOLARLAS):
1. HABLA CORTO: `speech_markup` debe tener MÁXIMO 2 frases cortas (<= 38 palabras en total), en español natural de Costa Rica, cálido y directo. NUNCA sueltes párrafos largos.
2. SIEMPRE DIBUJA O ESCRIBE EN LA PIZARRA PARA CUALQUIER TEMA: JAMÁS respondas que "no puedes plasmar mapas, listas, tablas o temas fuera de física". Puedes enseñar CUALQUIER materia desde cero (con o sin PDF):
   - Para listas estructuradas, resúmenes, comparaciones, tablas, líneas de tiempo o apuntes de cualquier materia: usa `type: "write_structured_notes"` con `payload: { "title": "...", "items": ["...", "..."], "table_headers": [...], "table_rows": [[...]], "note_es": "..." }`.
   - Para CUALQUIER dibujo (mapas esquemáticos como Costa Rica y territorios indígenas, mapas mentales, flujos, células, circuitos, moléculas, ejes históricos): usa `type: "draw_sketch"` con `kind: "custom_diagram"` y `elements: [{ "kind": "polygon"|"polyline"|"pin"|"box"|"circle"|"arrow"|"wavy_arrow"|"line"|"label", "x": 30..530, "y": 25..295, "w": ..., "h": ..., "x2": ..., "y2": ..., "r": ..., "points": [{"x":..., "y":...}], "color": "ink|blue|red|green|orange|purple|teal|pink", "fill": "blue|red|green|orange|purple|teal|", "text": "..." }]`.
   - También tienes atajos físicos listos para `draw_sketch.payload`: `piston_cylinder`, `entropy_particles`, `heat_engine_carnot`, `thermal_sphere`, `thermal_cylinder`, `thermal_bar`, `pv_cycle`, `pv_diagram`, `function_graph`, `thermal_gradient`.
   - Para fórmulas matemáticas/físicas: usa `type: "write_math"`.
3. UN SOLO MICRO-PASO A LA VEZ: NUNCA mezcles dos leyes o dos pasos largos en un mismo turno. Por ejemplo, si piden "las leyes de la termodinámica", explica PRIMERO solo la 1ª Ley dibujando el pistón (`piston_cylinder`) + escribiendo `\\Delta U = Q - W`, y al final pregunta: "¿Entendiste este dibujo o te perdiste en algo? ¿Seguimos con la Segunda Ley?".
4. MARCADORES A MANO, COLORES, CANCELACIÓN DE UNIDADES Y RASTREO VISUAL:
   - Usa `\\class{term-nombre}{...}` dentro de `write_math.payload.latex` e incluye `provenance`.
   - Usa acciones de marcador sincronizadas con `<cue id='c2'/>`, `<cue id='c3'/>` para subrayar, encerrar, resaltar, colorear o señalar términos exactos (`target_object_id` + `match_text` o `term_class`, `color`: `red|blue|green|orange|purple|yellow|teal|pink`, `style`: `solid|wavy|double|marker`, `temporary`: true/false, `ttl_ms`):
     * `underline_term`, `circle_term`, `highlight_term`, `colorize_term`, `point_at_term`, `write_margin_note`.
   - **CANCELACIÓN DE UNIDADES (`cancel_terms`):** SIEMPRE que sustituyas valores con unidades físicas en una fracción (ej. `atm`, `L`, `mol`, `m`, `s`, `J`), emite `type: "cancel_terms"` con `payload: { "target_object_id": "eq_...", "pairs": [{"unit_or_term": "atm", "color": "red"}, {"unit_or_term": "L", "color": "green"}, {"unit_or_term": "mol", "color": "purple"}], "surviving_unit": "K", "surviving_color": "blue" }` para tachar las unidades con diagonales de colores al ritmo de la voz.
   - **ORIGEN DE DATOS ("¿De dónde salió X?"):** NUNCA respondas solo con texto. Si el dato viene del enunciado del PDF (`active_problem.clip_object_id`), subraya/encierra el dato sobre el enunciado (`underline_term` o `circle_term` con `target_object_id: active_problem.clip_object_id`, `match_text: "1.50 atm"`, `color: "orange"`, `temporary: true`, `ttl_ms: 9500`) + `trace_origin`. Si el dato se calculó en un paso intermedio previo, encierra con círculo de color temporal ese resultado en el paso previo + `trace_origin` y/o `expand_intermediate_steps`.
   - Código de colores pedagógico consistente: `red` = calor / signo clave / alerta, `blue` = fórmulas principales / unidad final, `green` = trabajo / resultado verificado, `orange` = diferenciales `dx, dr, dV` o rastreo temporal de dato del enunciado, `purple` = energía / variable de estado / paso intermedio.
5. Si SymPy provee verificación previa, OBEDECE sus resultados exactos.
6. Coloca `<cue id='c1'/>`, `<cue id='c2'/>`, `<cue id='c3'/>` en `speech_markup` justo antes de mencionar cada dibujo, fórmula, cancelación o subrayado, enlazado a `actions[i].animation.cue_id`.
7. Cuando `active_problem` tiene un problema en curso: si `intent` es `start_problem`, haz SOLO el Paso 1 (dibujo del esquema físico + datos y fórmula inicial). Si `intent` es `next_problem_step`, avanza SOLO al siguiente paso lógico.
8. Devuelve ÚNICAMENTE JSON válido del contrato `cuadernu.tutor.v1.1` (sin markdown).

FORMATOS DE ACCIÓN ADMITIDOS EN `actions[i].type`:
`draw_sketch | write_math | write_structured_notes | write_text | annotate_step | highlight_term | underline_term | circle_term | colorize_term | point_at_term | write_margin_note | cancel_terms | trace_origin | expand_intermediate_steps | draw_inline_graph | plot_riemann_sum | sync_calculator_state`

FORMATO JSON EXACTO:
{
  "protocol": "cuadernu.tutor.v1.1",
  "session_id": "<session_id>",
  "turn_id": "<turn_id>",
  "beat_id": "beat_0001",
  "step_index": 1,
  "checkpoint_id": "cp_...",
  "branch_id": null,
  "speech_markup": "<cue id='c1'/> Mira este pistón: el calor entra y empuja el gas. <cue id='c2'/> Por eso escribimos delta U igual a Q menos W. <cue id='c3'/> Subrayo Q en rojo porque es el calor que entra. ¿Entendiste o te perdiste en algo?",
  "speech_rate": 0.96,
  "actions": [
    {
      "action_id": "act_1",
      "type": "draw_sketch | write_math | write_text | annotate_step | highlight_term | underline_term | circle_term | colorize_term | point_at_term | write_margin_note | cancel_terms | trace_origin | expand_intermediate_steps | draw_inline_graph | plot_riemann_sum | sync_calculator_state",
      "object_id": "sketch_1",
      "placement": { "zone": "derivation_flow | visual_lab | clarification_sidecar", "relation": "auto_next | below | between | overlay_on", "target_id": null, "gap_px": 20 },
      "animation": { "style": "handwriting_svg | draw_stroke | spring_reflow", "duration_ms": 1400, "cue_id": "c1" },
      "color_token": "ink_primary | accent_blue | accent_indigo | accent_green | accent_orange | accent_red",
      "payload": {}
    }
  ],
  "provenance": [{ "target_ref": "eq_...:term-...", "source_ref": "eq_...", "rule_es": "..." }],
  "verification": { "status": "sympy_verified | conceptual", "sympy_rule": null, "dimensional_check": null, "source_citation": null },
  "pedagogy": {
    "micro_goal": "...",
    "concept_tag": "fis3.general",
    "wait_for_student": true,
    "check_question": "¿Entendiste este dibujo y paso, o te quedaste pegado en algo?",
    "expected_answer_expr": null,
    "cognitive_load": "low",
    "suggested_followups": ["Sí, entendí · Continuar", "¿De dónde salió esto?", "Pasos intermedios", "Explicar con otro dibujo"],
    "origin_glossary": [{ "symbol": "...", "meaning": "...", "origin": "..." }]
  }
}
"""


def _extract_json_object(raw_text: str) -> Dict[str, Any]:
    s = (raw_text or "").strip()
    if s.startswith("```"):
        s = re.sub(r"^```(?:json)?\s*", "", s)
        s = re.sub(r"\s*```$", "", s)
    start = s.find("{")
    end = s.rfind("}")
    if start >= 0 and end > start:
        s = s[start : end + 1]
    return json.loads(s)


def _normalize_beat(
    raw: Dict[str, Any],
    session_id: str,
    turn_id: str,
    beat_num: int,
    step_index: int,
    *,
    verified_by_code: bool = False,
) -> Dict[str, Any]:
    from .verificador_numerico import limpiar_voz_latex

    beat_id = f"beat_{beat_num:04d}"
    actions = raw.get("actions") if isinstance(raw.get("actions"), list) else []
    clean_actions = []
    for idx, act in enumerate(actions[:8]):
        if not isinstance(act, dict):
            continue
        act_type = str(act.get("type", "write_text"))
        clean_actions.append({
            "action_id": str(act.get("action_id") or f"act_{beat_num}_{idx+1}"),
            "type": act_type,
            "object_id": str(act.get("object_id") or f"obj_{beat_num}_{idx+1}"),
            "placement": act.get("placement") if isinstance(act.get("placement"), dict) else {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 24},
            "animation": act.get("animation") if isinstance(act.get("animation"), dict) else {"style": "handwriting_svg", "duration_ms": 950, "cue_id": f"c{idx+1}"},
            "color_token": str(act.get("color_token") or "ink_primary"),
            "payload": act.get("payload") if isinstance(act.get("payload"), dict) else {},
        })

    ped = raw.get("pedagogy") if isinstance(raw.get("pedagogy"), dict) else {}
    ver = raw.get("verification") if isinstance(raw.get("verification"), dict) else {}
    prov = raw.get("provenance") if isinstance(raw.get("provenance"), list) else []

    # P0 #2: NUNCA confiar en un status "sympy_verified" inventado por el LLM.
    raw_status = str(ver.get("status") or "conceptual").strip()
    if verified_by_code:
        safe_status = raw_status if raw_status in ("sympy_verified", "numeric_verified", "conceptual", "discrepancy_detected") else "sympy_verified"
    else:
        safe_status = "conceptual" if raw_status in ("sympy_verified", "numeric_verified", "verified", "") else raw_status

    raw_speech = str(raw.get("speech_markup") or "Revisemos este paso en la pizarra.")
    clean_speech = limpiar_voz_latex(raw_speech, preservar_cues=True) or "Revisemos este paso en la pizarra."

    return {
        "protocol": "cuadernu.tutor.v1.1",
        "session_id": session_id,
        "turn_id": turn_id,
        "beat_id": beat_id,
        "step_index": int(raw.get("step_index", step_index)),
        "checkpoint_id": str(raw.get("checkpoint_id") or f"cp_{beat_id}"),
        "branch_id": raw.get("branch_id"),
        "speech_markup": clean_speech,
        "speech_rate": float(max(0.75, min(1.35, float(raw.get("speech_rate", 0.95))))),
        "actions": clean_actions,
        "provenance": prov,
        "verification": {
            "status": safe_status,
            "sympy_rule": ver.get("sympy_rule") if verified_by_code else None,
            "dimensional_check": ver.get("dimensional_check"),
            "source_citation": ver.get("source_citation"),
        },
        "pedagogy": {
            "micro_goal": str(ped.get("micro_goal") or "Explicar el paso actual con claridad"),
            "concept_tag": str(ped.get("concept_tag") or "calc.int.general"),
            "wait_for_student": bool(ped.get("wait_for_student", True)),
            "check_question": ped.get("check_question"),
            "expected_answer_expr": ped.get("expected_answer_expr"),
            "cognitive_load": "medium" if ped.get("cognitive_load") == "medium" else "low",
            "suggested_followups": ped.get("suggested_followups") if isinstance(ped.get("suggested_followups"), list) else ["¿De dónde salió esto?", "Pasos intermedios", "Continuar"],
            "origin_glossary": ped.get("origin_glossary") if isinstance(ped.get("origin_glossary"), list) else [],
        },
    }


def _try_extract_integral_from_text(text: str) -> Optional[Dict[str, Any]]:
    """Heuristic pre-check to run SymPy integral_steps() before asking the LLM."""
    m = re.search(r"(?:integral|integrar|∫)\s*(?:de\s+)?([A-Za-z0-9\^\*\+\-\/\(\)\s\.]+?)\s*d([A-Za-z])", text, re.IGNORECASE)
    if not m:
        return None
    integrand = m.group(1).strip()
    var = m.group(2).strip()
    lim = re.search(r"(?:desde|de)\s+([\-0-9\.A-Za-z_]+)\s+(?:hasta|a)\s+([\-0-9\.A-Za-z_]+)", text, re.IGNORECASE)
    low, high = (lim.group(1), lim.group(2)) if lim else (None, None)
    plan = build_verified_integral_plan(integrand, var, low, high, physics_context=text)
    return plan.to_dict() if plan.ok else None


class GeminiVisualDirector:
    """Fast cloud multimodal director generating SymPy-grounded cuadernu.tutor.v1.1 beats."""

    _client = _ClienteGeminiPerezoso()

    def __init__(self, api_key: str, primary_model: str = VISUAL_MODEL_DEFAULT) -> None:
        self._api_key = (api_key or "").strip()
        self.primary_model = normalizar_modelo_visual(primary_model)

    @property
    def configured(self) -> bool:
        return bool(self._api_key)

    async def generate_beat(
        self,
        *,
        session_id: str,
        beat_num: int,
        step_index: int,
        user_prompt: str,
        intent: str = "question",
        scene_summary: List[Dict[str, Any]],
        provenance_edges: List[Dict[str, Any]],
        rag_citations: List[Dict[str, Any]],
        pedagogy_summary: Dict[str, Any],
        crop_png_b64: Optional[str] = None,
        extra_images_b64: Optional[List[str]] = None,
        active_problem: Optional[Dict[str, Any]] = None,
        extra_context: Optional[Dict[str, Any]] = None,
    ) -> Tuple[Dict[str, Any], Dict[str, Any]]:
        turn_id = f"turn_{uuid.uuid4().hex[:8]}"
        sympy_plan = _try_extract_integral_from_text(user_prompt)

        problem_meta = None
        if isinstance(active_problem, dict):
            problem_meta = {
                "problem_id": active_problem.get("problem_id"),
                "source_label": active_problem.get("source_label"),
                "subtema": active_problem.get("subtema"),
                "procedimiento": active_problem.get("procedimiento"),
                "problem_text": str(active_problem.get("problem_text") or "")[:2400],
                "current_step": int(active_problem.get("step_count", 1)),
                "clip_object_id": active_problem.get("clip_object_id", "problem_clip"),
            }

        context_block = {
            "intent": intent,
            "session_id": session_id,
            "turn_id": turn_id,
            "next_beat_id": f"beat_{beat_num:04d}",
            "next_step_index": step_index,
            "active_problem": problem_meta,
            "current_board_objects": scene_summary[-14:],
            "provenance_graph": provenance_edges[-14:],
            "pdf_citations": rag_citations[:4],
            "sympy_preverification": sympy_plan,
            "student_memory": pedagogy_summary,
            "extra_context": extra_context or {},
            "student_message": user_prompt,
        }

        if not self._client:
            beat = self._offline_sympy_fallback(session_id, turn_id, beat_num, step_index, user_prompt, sympy_plan)
            return beat, {"model": "offline_sympy_fallback", "latency_ms": 1}

        contents: List[Any] = [json.dumps(context_block, ensure_ascii=False)]
        all_imgs: List[str] = []
        if crop_png_b64:
            all_imgs.append(crop_png_b64)
        for im in extra_images_b64 or []:
            if im and im not in all_imgs:
                all_imgs.append(im)
        for b64_img in all_imgs[:3]:
            try:
                img_bytes = base64.b64decode(b64_img)
                contents.append(types.Part.from_bytes(data=img_bytes, mime_type="image/png"))
            except Exception:
                pass

        from .fisica3.formulario_ii_2026 import FORMULARIO_PROMPT_TXT

        primario = normalizar_modelo_visual(self.primary_model)
        chain = [primario] + [m for m in VISUAL_MODEL_CHAIN if m != primario]
        last_err = ""
        for model_name in chain:
            t0 = time.perf_counter()
            try:
                resp = await asyncio.wait_for(
                    asyncio.to_thread(
                        self._client.models.generate_content,
                        model=model_name,
                        contents=contents,
                        config=types.GenerateContentConfig(
                            system_instruction=_DIRECTOR_SYSTEM_PROMPT + "\n\n" + FORMULARIO_PROMPT_TXT,
                            response_mime_type="application/json",
                            temperature=0.25,
                        ),
                    ),
                    timeout=28.0,
                )
                latency_ms = int((time.perf_counter() - t0) * 1000)
                parsed = _extract_json_object(resp.text or "{}")
                beat = _normalize_beat(parsed, session_id, turn_id, beat_num, step_index, verified_by_code=False)
                if sympy_plan and sympy_plan.get("ok"):
                    beat["verification"]["status"] = "sympy_verified"
                    beat["verification"]["sympy_rule"] = "manualintegrate.integral_steps"
                    if not beat["verification"].get("dimensional_check") and sympy_plan.get("dimensional_note"):
                        beat["verification"]["dimensional_check"] = sympy_plan["dimensional_note"]
                else:
                    from .verificador_numerico import verificar_beat_numerico

                    vnum = verificar_beat_numerico(beat)
                    if vnum["discrepancias"]:
                        beat["verification"]["status"] = "discrepancy_detected"
                        beat["verification"]["numeric_check"] = vnum
                        aviso = " ".join(d["mensaje"] for d in vnum["discrepancias"][:2])
                        beat["actions"].append({
                            "action_id": f"act_chk_{beat_num}",
                            "type": "write_text",
                            "object_id": f"chk_{beat_num}",
                            "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 14},
                            "animation": {"style": "fade_in", "duration_ms": 400, "cue_id": "c1"},
                            "color_token": "accent_red",
                            "payload": {"text": f"Aviso numérico (Python): {aviso}"},
                        })
                    elif vnum["verificado"]:
                        beat["verification"]["status"] = "numeric_verified"
                        beat["verification"]["numeric_check"] = vnum
                return beat, {"model": model_name, "latency_ms": latency_ms}
            except Exception as exc:
                last_err = sanitize_text(str(exc))[:160]
                continue

        beat = self._offline_sympy_fallback(session_id, turn_id, beat_num, step_index, user_prompt, sympy_plan)
        return beat, {"model": "fallback_after_error", "error": last_err, "latency_ms": 0}

    @staticmethod
    def _offline_sympy_fallback(
        session_id: str,
        turn_id: str,
        beat_num: int,
        step_index: int,
        user_prompt: str,
        sympy_plan: Optional[Dict[str, Any]],
    ) -> Dict[str, Any]:
        if sympy_plan and sympy_plan.get("ok"):
            steps_latex = [s["latex"] for s in sympy_plan.get("steps", [])][:4]
            final_ltx = sympy_plan.get("evaluation_bridge_latex") or f"\\int {sympy_plan['integrand_latex']}\\,d{sympy_plan['variable']} = {sympy_plan['antiderivative_latex']} + C"
            return _normalize_beat(
                {
                    "speech_markup": f"Verifiqué la integral simbólicamente. <cue id='c1'/> Aquí tienes el desarrollo exacto paso a paso sin saltos algebraicos.",
                    "actions": [
                        {
                            "action_id": f"act_sym_{beat_num}",
                            "type": "write_math",
                            "object_id": f"eq_sym_{beat_num}",
                            "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 24},
                            "animation": {"style": "handwriting_svg", "duration_ms": 1100, "cue_id": "c1"},
                            "color_token": "accent_blue",
                            "payload": {
                                "latex": final_ltx,
                                "step_label": f"Paso {step_index} · Verificado simbólicamente",
                            },
                        },
                        {
                            "action_id": f"act_sym_sub_{beat_num}",
                            "type": "expand_intermediate_steps",
                            "object_id": f"sub_sym_{beat_num}",
                            "placement": {"zone": "derivation_flow", "relation": "below", "target_id": f"eq_sym_{beat_num}", "gap_px": 16},
                            "animation": {"style": "spring_reflow", "duration_ms": 800, "cue_id": "c1"},
                            "payload": {
                                "after_object_id": f"eq_sym_{beat_num}",
                                "substeps": [{"latex": lx, "note": st.get("title_es", "")} for lx, st in zip(steps_latex, sympy_plan.get("steps", []))],
                            },
                        },
                    ],
                    "verification": {
                        "status": "sympy_verified",
                        "sympy_rule": "manualintegrate.integral_steps",
                        "dimensional_check": sympy_plan.get("dimensional_note") or None,
                    },
                    "pedagogy": {
                        "micro_goal": "Resolver la integral con verificación simbólica exacta",
                        "wait_for_student": True,
                        "check_question": "¿Quieres que desglosemos algún paso intermedio de esta integral?",
                    },
                },
                session_id,
                turn_id,
                beat_num,
                step_index,
                verified_by_code=True,
            )

        return _normalize_beat(
            {
                "speech_markup": f"<cue id='c1'/> Recibí tu consulta: {user_prompt[:120]}. Escribe una integral concreta o rodea un paso de la pizarra con el lazo para revisarlo juntos.",
                "actions": [
                    {
                        "action_id": f"act_fb_{beat_num}",
                        "type": "annotate_step",
                        "object_id": f"note_fb_{beat_num}",
                        "placement": {"zone": "clarification_sidecar", "relation": "auto_next", "gap_px": 18},
                        "animation": {"style": "fade_in", "duration_ms": 500, "cue_id": "c1"},
                        "payload": {"text": f"Duda registrada: {user_prompt[:180]}"},
                    }
                ],
                "pedagogy": {"micro_goal": "Clarificar duda", "wait_for_student": True},
            },
            session_id,
            turn_id,
            beat_num,
            step_index,
        )


class GeminiLiveVoiceBridge:
    """Real-time full-duplex Gemini 3.8 Live voice tutor with NON_BLOCKING whiteboard tools and native barge-in."""

    _client = _ClienteGeminiPerezoso()

    def __init__(
        self,
        api_key: str,
        model_name: str = LIVE_AUDIO_MODEL_PRIMARY,
        on_pcm_out: Optional[Callable[[bytes], Awaitable[None]]] = None,
        on_event_out: Optional[Callable[[Dict[str, Any]], Awaitable[None]]] = None,
        on_tool_beat: Optional[Callable[[Dict[str, Any]], Awaitable[Dict[str, Any]]]] = None,
    ) -> None:
        self._api_key = (api_key or "").strip()
        self.model_name = model_name
        self._on_pcm_out = on_pcm_out
        self._on_event_out = on_event_out
        self._on_tool_beat = on_tool_beat
        self._session: Any = None
        self._ctx_mgr: Any = None
        self._recv_task: Optional[asyncio.Task[Any]] = None
        self._running = False

    @property
    def is_running(self) -> bool:
        return self._running and self._session is not None

    @staticmethod
    def _build_live_tools() -> List[types.Tool]:
        return [
            types.Tool(
                function_declarations=[
                    types.FunctionDeclaration(
                        name="write_whiteboard_equation",
                        description="Escribe una ecuación 2D animada en la pizarra sincronizada con lo que estás explicando por voz.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["object_id", "latex", "step_label"],
                            properties={
                                "object_id": types.Schema(type=types.Type.STRING, description="ID único del bloque ej. eq_live_1"),
                                "latex": types.Schema(type=types.Type.STRING, description="Ecuación LaTeX válida"),
                                "step_label": types.Schema(type=types.Type.STRING, description="Título corto del paso en español"),
                                "note_es": types.Schema(type=types.Type.STRING, description="Explicación breve opcional"),
                                "color_token": types.Schema(type=types.Type.STRING, description="ink_primary|accent_blue|accent_green|accent_red"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="trace_symbol_origin",
                        description="Señala visualmente en la pizarra de dónde salió un término, signo o fórmula conectando origen y destino con flecha animada.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["from_ref", "to_ref", "rule_es"],
                            properties={
                                "from_ref": types.Schema(type=types.Type.STRING),
                                "to_ref": types.Schema(type=types.Type.STRING),
                                "rule_es": types.Schema(type=types.Type.STRING),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="expand_intermediate_algebra",
                        description="Despliega pasos algebraicos intermedios in-place entre dos ecuaciones cuando el alumno se perdió.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["after_object_id", "substeps_latex"],
                            properties={
                                "after_object_id": types.Schema(type=types.Type.STRING),
                                "substeps_latex": types.Schema(
                                    type=types.Type.ARRAY,
                                    items=types.Schema(type=types.Type.STRING),
                                ),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="verify_integral_sympy",
                        description="Verifica simbólicamente una integral con SymPy antes de afirmarla en clase.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["integrand", "variable"],
                            properties={
                                "integrand": types.Schema(type=types.Type.STRING),
                                "variable": types.Schema(type=types.Type.STRING),
                                "lower": types.Schema(type=types.Type.STRING),
                                "upper": types.Schema(type=types.Type.STRING),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="clear_whiteboard",
                        description="Borra realmente todo lo escrito y dibujado en la pizarra (pasos del profesor y trazos a lápiz del estudiante). SIEMPRE llama esta herramienta cuando el alumno pida borrar o limpiar la pizarra.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            properties={
                                "clear_ink": types.Schema(type=types.Type.BOOLEAN, description="Si true (default), borra también los trazos de lápiz del alumno."),
                                "clear_media": types.Schema(type=types.Type.BOOLEAN, description="Si true, quita también recortes o PDFs montados."),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="draw_whiteboard_sketch",
                        description="Dibuja a mano trazo por trazo un esquema físico en la pizarra mientras hablas (pistón 1ª ley, partículas entropía 2ª ley, máquina Carnot, esfera térmica, barra de conducción, diagrama P-V o gráfica).",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["kind", "title"],
                            properties={
                                "kind": types.Schema(type=types.Type.STRING, description="piston_cylinder | entropy_particles | heat_engine_carnot | thermal_sphere | thermal_cylinder | thermal_bar | pv_cycle | pv_diagram | function_graph"),
                                "title": types.Schema(type=types.Type.STRING),
                                "caption_es": types.Schema(type=types.Type.STRING),
                                "expr": types.Schema(type=types.Type.STRING),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="show_physics_simulation",
                        description="Abre un widget interactivo en la pizarra (rectángulos de Riemann, diagrama P-V o cascarón térmico esférico/cilíndrico).",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["widget_kind"],
                            properties={
                                "widget_kind": types.Schema(type=types.Type.STRING, description="riemann | pv_isothermal | pv_adiabatic | thermal_spherical | thermal_cylindrical"),
                                "title": types.Schema(type=types.Type.STRING),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="find_and_crop_pdf_problem",
                        description="Busca un problema por código (ej. Q057, Q122) o por tema dentro del PDF cargado, recorta su enunciado y figura en alta resolución, lo pega en la pizarra y devuelve el texto completo.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["query"],
                            properties={
                                "query": types.Schema(type=types.Type.STRING, description="Código de problema ej. Q057 o tema ej. conducción esférica"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="audit_pdf_repeated_problems",
                        description="Audita visual y estructuralmente todo el PDF cargado para encontrar qué temas, procedimientos y problemas se repiten más y mostrar la auditoría interactiva en la pizarra.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            properties={
                                "top_k": types.Schema(type=types.Type.INTEGER, description="Cantidad máxima de grupos a mostrar (default 6)"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="open_inline_visual_explanation",
                        description="Abre un espacio en blanco justo debajo de un paso en la pizarra para explicar un paso intermedio con álgebra y/o una gráfica visual sincronizada.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["after_object_id", "title"],
                            properties={
                                "after_object_id": types.Schema(type=types.Type.STRING),
                                "title": types.Schema(type=types.Type.STRING),
                                "substeps_latex": types.Schema(type=types.Type.ARRAY, items=types.Schema(type=types.Type.STRING)),
                                "graph_kind": types.Schema(type=types.Type.STRING, description="function_plot | pv_cycle | thermal_gradient"),
                                "graph_expr": types.Schema(type=types.Type.STRING, description="Expresión en x ej. 1/(x^2) o 2400/x"),
                                "x_min": types.Schema(type=types.Type.NUMBER),
                                "x_max": types.Schema(type=types.Type.NUMBER),
                                "x_label": types.Schema(type=types.Type.STRING),
                                "y_label": types.Schema(type=types.Type.STRING),
                                "caption_es": types.Schema(type=types.Type.STRING),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="trace_data_provenance",
                        description="OBLIGATORIA cuando el alumno pregunta '¿de dónde salió este número/dato?' (ej. 1.50 atm o 63.98 K). Subraya el dato directamente en el enunciado del PDF si es dato inicial (origin_kind='statement'), o encierra con un círculo de color temporal el valor calculado en un paso intermedio previo (origin_kind='intermediate_step'), o muestra la constante/pista (origin_kind='constant_or_law'), conectando origen y destino con flecha temporal.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["value_text", "origin_kind", "source_phrase_or_text"],
                            properties={
                                "value_text": types.Schema(type=types.Type.STRING, description="Dato o símbolo preguntado ej. '1.50 atm', '63.98 K', '0.08206'"),
                                "origin_kind": types.Schema(type=types.Type.STRING, description="statement | intermediate_step | constant_or_law"),
                                "source_phrase_or_text": types.Schema(type=types.Type.STRING, description="Frase exacta en el enunciado PDF (ej. '1.50 atm', 'diatómico') o valor/símbolo en el paso previo (ej. '63.98 K')"),
                                "source_object_id": types.Schema(type=types.Type.STRING, description="Opcional: object_id del paso intermedio previo donde se calculó"),
                                "target_object_id": types.Schema(type=types.Type.STRING, description="Opcional: object_id de la ecuación actual donde se usa"),
                                "target_text": types.Schema(type=types.Type.STRING, description="Opcional: texto exacto a encerrar en la ecuación destino"),
                                "color": types.Schema(type=types.Type.STRING, description="orange | red | purple | blue | green | pink | teal"),
                                "style": types.Schema(type=types.Type.STRING, description="underline | circle | highlight"),
                                "note_es": types.Schema(type=types.Type.STRING, description="Etiqueta breve ej. 'Dato del enunciado' o 'Calculado en Estado 1'"),
                                "temporary": types.Schema(type=types.Type.BOOLEAN, description="Si true (default), la marca temporal se desvanece suavemente tras unos segundos"),
                                "ttl_ms": types.Schema(type=types.Type.INTEGER, description="Duración en ms para marcas temporales (default 9500)"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="cancel_units_or_terms",
                        description="Tacha con líneas diagonales de colores (/) pares de unidades o factores que se cancelan entre numerador y denominador (ej. atm con atm en rojo, L con L en verde, mol con mol en morado) y opcionalmente encierra la unidad sobreviviente (ej. K en azul).",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["pairs"],
                            properties={
                                "object_id": types.Schema(type=types.Type.STRING, description="Opcional: object_id de la ecuación (default: última ecuación)"),
                                "pairs": types.Schema(
                                    type=types.Type.ARRAY,
                                    description="Lista de unidades o factores repetidos a tachar con diagonales de colores",
                                    items=types.Schema(
                                        type=types.Type.OBJECT,
                                        required=["unit_or_term", "color"],
                                        properties={
                                            "unit_or_term": types.Schema(type=types.Type.STRING, description="Unidad o factor exacto ej. 'atm', 'L', 'mol', 'm', 's'"),
                                            "color": types.Schema(type=types.Type.STRING, description="red | green | purple | orange | blue | teal | pink"),
                                            "replacement_note": types.Schema(type=types.Type.STRING, description="Nota pequeña opcional ej. '1'"),
                                        },
                                    ),
                                ),
                                "surviving_unit": types.Schema(type=types.Type.STRING, description="Unidad final sobreviviente a encerrar ej. 'K' o 'J'"),
                                "surviving_color": types.Schema(type=types.Type.STRING, description="Color para encerrar la unidad final (default 'blue')"),
                                "temporary": types.Schema(type=types.Type.BOOLEAN, description="Si true, se desvanece luego; por defecto false para quedar en el paso"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="annotate_whiteboard",
                        description="Aplica un marcador a mano en vivo sincronizado con tu voz: subrayar de color (underline), encerrar en círculo/óvalo (circle), resaltar translúcido (highlight), señalar con puntero láser (point), cambiar color de tinta de un término (colorize), o escribir nota al margen (margin_note).",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["action", "target_text", "color"],
                            properties={
                                "action": types.Schema(type=types.Type.STRING, description="underline | circle | highlight | point | colorize | margin_note | cancel"),
                                "target_text": types.Schema(type=types.Type.STRING, description="Texto, número o símbolo exacto a marcar ej. 'Q', '1.50 atm', '-1/r'"),
                                "object_id": types.Schema(type=types.Type.STRING, description="Opcional: object_id del bloque o problem_clip"),
                                "color": types.Schema(type=types.Type.STRING, description="red | blue | green | orange | purple | yellow | teal | pink"),
                                "style": types.Schema(type=types.Type.STRING, description="solid | wavy | double | marker"),
                                "note_es": types.Schema(type=types.Type.STRING, description="Nota breve opcional junto a la marca"),
                                "temporary": types.Schema(type=types.Type.BOOLEAN, description="Marca temporal con fade-out automático"),
                                "ttl_ms": types.Schema(type=types.Type.INTEGER, description="Duración en ms si temporary=true"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="write_whiteboard_notes",
                        description="Escribe a mano en la pizarra listas estructuradas, resúmenes, comparaciones, tablas, líneas de tiempo o apuntes claros de CUALQUIER materia (historia, estudios sociales, geografía, biología, química, programación, física, idiomas, etc.).",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["title"],
                            properties={
                                "object_id": types.Schema(type=types.Type.STRING, description="ID único opcional ej. notes_live_1"),
                                "title": types.Schema(type=types.Type.STRING, description="Título del tema en la pizarra"),
                                "items": types.Schema(
                                    type=types.Type.ARRAY,
                                    description="Lista de puntos, conceptos, pueblos, pasos o hechos a escribir a mano en la pizarra",
                                    items=types.Schema(type=types.Type.STRING),
                                ),
                                "table_headers": types.Schema(
                                    type=types.Type.ARRAY,
                                    description="Opcional: encabezados de tabla comparativa ej. ['Fase', 'Lugar', 'Requiere luz', 'Produce']",
                                    items=types.Schema(type=types.Type.STRING),
                                ),
                                "table_rows": types.Schema(
                                    type=types.Type.ARRAY,
                                    description="OBLIGATORIO si usas table_headers: filas de la tabla como lista de strings separados por ' | ' o lista de celdas, ej. ['Fase luminosa | Tilacoides | Sí | ATP y O2', 'Fase oscura | Estroma | No | Glucosa']",
                                    items=types.Schema(type=types.Type.STRING),
                                ),
                                "note_es": types.Schema(type=types.Type.STRING, description="Nota resumen opcional al pie"),
                                "color_token": types.Schema(type=types.Type.STRING, description="accent_blue|accent_green|accent_orange|accent_purple|accent_red"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="draw_custom_whiteboard_diagram",
                        description="Dibuja A MANO CUALQUIER diagrama, mapa esquemático (ej. mapa de Costa Rica con pines de territorios indígenas), mapa mental, diagrama de flujo (deja al menos 24 px de espacio entre cajas), célula, circuito, molécula o línea de tiempo en un lienzo de 560x320.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["title", "elements"],
                            properties={
                                "title": types.Schema(type=types.Type.STRING),
                                "caption_es": types.Schema(type=types.Type.STRING),
                                "elements": types.Schema(
                                    type=types.Type.ARRAY,
                                    description="Elementos vectoriales dibujados a mano en coordenadas x: 20..540, y: 20..300",
                                    items=types.Schema(
                                        type=types.Type.OBJECT,
                                        required=["kind"],
                                        properties={
                                            "kind": types.Schema(type=types.Type.STRING, description="polygon | polyline | pin | box | circle | arrow | wavy_arrow | line | label"),
                                            "x": types.Schema(type=types.Type.NUMBER),
                                            "y": types.Schema(type=types.Type.NUMBER),
                                            "x2": types.Schema(type=types.Type.NUMBER),
                                            "y2": types.Schema(type=types.Type.NUMBER),
                                            "w": types.Schema(type=types.Type.NUMBER),
                                            "h": types.Schema(type=types.Type.NUMBER),
                                            "r": types.Schema(type=types.Type.NUMBER),
                                            "points": types.Schema(
                                                type=types.Type.ARRAY,
                                                items=types.Schema(
                                                    type=types.Type.OBJECT,
                                                    properties={
                                                        "x": types.Schema(type=types.Type.NUMBER),
                                                        "y": types.Schema(type=types.Type.NUMBER),
                                                    },
                                                ),
                                            ),
                                            "color": types.Schema(type=types.Type.STRING, description="ink|blue|red|green|orange|purple|teal|pink"),
                                            "fill": types.Schema(type=types.Type.STRING, description="Opcional color de relleno suave"),
                                            "text": types.Schema(type=types.Type.STRING),
                                            "bold": types.Schema(type=types.Type.BOOLEAN),
                                        },
                                    ),
                                ),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="buscar_leccion_banco",
                        description="Busca en el banco verificado de 166 lecciones de Física General III (21 Recomendados E1..E21, 137 Superexamen Q001..Q137 y 8 mini-lecciones de teoría T-DIL..T-ENT) por código, tema o palabras del enunciado. Úsala SIEMPRE antes de improvisar un ejercicio de Física III.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["query"],
                            properties={
                                "query": types.Schema(type=types.Type.STRING, description="Código (ej. Q049, E8) o palabras del problema/tema"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="abrir_leccion_banco",
                        description="Abre directamente en la pizarra una lección verificada del banco (ej. E8, Q049, T-CON) y devuelve sus pasos y respuestas exactas calculadas en Python para que nunca inventes números.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["lesson_id"],
                            properties={
                                "lesson_id": types.Schema(type=types.Type.STRING, description="Identificador exacto del banco ej. E1, E8, Q049, Q122, T-PV"),
                            },
                        ),
                    ),
                    types.FunctionDeclaration(
                        name="connect_origin_arrow",
                        description="Dibuja a mano una flecha curva de color conectando un dato/término previo con otro término en la pizarra.",
                        behavior=types.Behavior.NON_BLOCKING,
                        parameters=types.Schema(
                            type=types.Type.OBJECT,
                            required=["from_text_or_id", "to_text_or_id", "rule_es"],
                            properties={
                                "from_text_or_id": types.Schema(type=types.Type.STRING),
                                "to_text_or_id": types.Schema(type=types.Type.STRING),
                                "color": types.Schema(type=types.Type.STRING, description="orange | red | blue | green | purple"),
                                "rule_es": types.Schema(type=types.Type.STRING),
                                "temporary": types.Schema(type=types.Type.BOOLEAN),
                                "ttl_ms": types.Schema(type=types.Type.INTEGER),
                            },
                        ),
                    ),
                ]
            )
        ]

    async def start(self, board_context_summary: str = "") -> Dict[str, Any]:
        await self.stop()
        if not self._client:
            return {"ok": False, "error": "Clave de audio Gemini Live no configurada."}

        sys_instr = (
            "Eres un profesor particular audiovisual UNIVERSAL (dominas TODAS las materias: física, cálculo, matemáticas, "
            "historia, geografía, estudios sociales, biología, química, programación, medicina, idiomas y cualquier tema que "
            "el estudiante te pida, CON O SIN PDF cargado), hablando en español natural y cercano de Costa Rica. "
            "NUNCA digas que solo eres tutor de matemáticas o física ni que solo te enfocas en temas del TEC: puedes enseñar, "
            "escribir apuntes y dibujar diagramas sobre CUALQUIER materia o tema que pida el estudiante. "
            "Explica en pasos muy cortos: explica UN micro-paso claro a la vez, nunca te saltes detalles clave, y al terminar CADA paso "
            "pregunta de forma natural: '¿Entendiste bien o te perdiste en algún lado? ¿Continuamos o te quedaste pegado en algo?'. "
            "Acepta interrupciones al instante con total paciencia. "
            "REGLA DE LIBERTAD TOTAL EN PIZARRA (PROHIBIDO DECIR 'POR LIMITACIONES TÉCNICAS NO PUEDO'): "
            "Puedes escribir y dibujar CUALQUIER tema en la pizarra sin estar limitado a física ni a PDFs precargados: "
            "- Para listas, resúmenes, comparaciones, tablas, historia, estudios sociales, biología o apuntes de cualquier materia: llama a `write_whiteboard_notes`! "
            "- Para mapas esquemáticos (como el mapa de Costa Rica y sus territorios indígenas), mapas mentales, diagramas de flujo, células, circuitos o cualquier ilustración libre: llama a `draw_custom_whiteboard_diagram` (usando `polygon`, `polyline`, `pin`, `box`, `circle`, `arrow`, `label`)! "
            "- Para esquemas físicos prearmados: llama a `draw_whiteboard_sketch`. Para fórmulas matemáticas: llama a `write_whiteboard_equation`. "
            "REGLA OBLIGATORIA DE SINCRONIZACIÓN VOZ ↔ PIZARRA (NARRA EN PRESENTE LO MISMO QUE ESCRIBES): "
            "PROHIBIDO hablar en pasado diciendo 'ahí te anoté la lista' o 'ya te escribí la ecuación' sin decir lo que escribiste. "
            "Habla SIEMPRE en presente progresivo AL RITMO de tu escritura y NOMBRA EN EL MISMO ORDEN EXACTO cada elemento que pusiste en la pizarra: "
            "- Si llamas a `write_whiteboard_equation`: ve leyendo en orden los términos que estás escribiendo (ej. 'Voy escribiendo W igual a uno sobre gamma menos uno, multiplicado por p uno V uno menos p dos V dos...'). "
            "- Si llamas a `write_whiteboard_notes` con una lista o tabla: ve mencionando en voz alta CADA punto o pueblo en el MISMO orden de la lista mientras aparece (ej. 'Vamos anotándolos uno por uno: primero los Bribri..., luego los Cabécar..., después los Maleku...'). "
            "- Si llamas a `cancel_units_or_terms`: nombra cada unidad al tacharla en el mismo orden (ej. 'Tachamos atmósferas arriba y abajo en rojo, tachamos litros con litros en verde, y nos queda Kelvin'). "
            "Mantén un ritmo pausado, claro y humano (1 micro-paso a la vez; para listas o fórmulas largas, tómate las frases necesarias para leer lo que escribes sin correr). "
            "REGLA DE IMÁGENES PEGADAS CON CTRL+V: Cuando recibas `[IMAGEN PEGADA POR EL ESTUDIANTE EN LA PIZARRA]`, la imagen "
            "ya está adjunta directamente en ese mismo turno: léela de inmediato (fórmulas, texto, gráficas, mapas o esquemas), "
            "explícala claramente y escribe los pasos en la pizarra usando tus herramientas. "
            "REGLA OBLIGATORIA DE MARCADORES EN VIVO: usa `annotate_whiteboard` mientras hablas para subrayar de colores, "
            "encerrar en círculos a mano, resaltar, señalar con el puntero o colorear términos clave en tiempo real. "
            "REGLA OBLIGATORIA DE CANCELACIÓN DE UNIDADES: SIEMPRE que escribas una sustitución con unidades físicas en una fracción "
            "(por ejemplo atm, L, mol, K, m, s, J), llama INMEDIATAMENTE a `cancel_units_or_terms` para tachar cada par de unidades "
            "con líneas diagonales de colores distintos (ej. atm en rojo, L en verde, mol en morado) y encerrar la unidad final sobreviviente. "
            "REGLA OBLIGATORIA DE ORIGEN DE DATOS ('¿de dónde salió X?'): PROHIBIDO responder solo con voz cuando el estudiante "
            "pregunte de dónde salió un número, dato o variable (ej. '¿de dónde salió 1.50 atm?' o '¿de dónde salió 63.98 K?'). "
            "DEBES llamar a `trace_data_provenance` AL MISMO TIEMPO que respondes: "
            "(1) Si el dato viene del enunciado del problema, usa `origin_kind='statement'` y `source_phrase_or_text` exacto del enunciado (ej. '1.50 atm') para subrayarlo directamente en el enunciado con color temporal y conectarlo; "
            "(2) Si el dato se calculó en un paso intermedio anterior del desarrollo, usa `origin_kind='intermediate_step'` para encerrar ese paso anterior con un círculo de color temporal y conectarlo al paso actual; "
            "(3) Si es una constante física o pista del enunciado (ej. R o 'diatómico' -> Cv), usa `origin_kind='constant_or_law'`. "
            "Si el estudiante pide borrar o limpiar la pizarra, DEBES llamar a `clear_whiteboard` (NUNCA digas que borraste sin llamarla). "
            "REGLA DE LECTURA COMPLETA DE ENUNCIADOS: Si el estudiante te pide leer el enunciado completo del problema "
            "o pregunta qué dice todo el problema, LÉELO COMPLETO palabra por palabra de principio a fin (incluyendo todos "
            "los valores numéricos y todos los incisos a, b, c) sin acortarlo; la regla de hablar corto (máximo 2 frases) "
            "aplica SOLO cuando explicas un paso de solución, NO cuando lees el enunciado del problema. "
            "REGLA DE VISIÓN DE LO QUE EL ALUMNO SEÑALA O ENCIERRA: Recibirás en tiempo real (1) la captura exacta "
            "de la ventana visible actual de la pizarra con las fórmulas y trazos del estudiante, (2) un acercamiento "
            "en zoom del símbolo/zona exacta que el alumno señaló con el puntero, encerró con Encerrar o marcó/apuntó con Lápiz, "
            "y (3) mensajes de contexto `[ATENCIÓN VISUAL · LO QUE EL ESTUDIANTE ACABA DE SEÑALAR/ENCERRAR]`. "
            "SIEMPRE prioriza la ÚLTIMA selección/marca del alumno (símbolo exacto, bloque y fórmula indicados en el banner rojo "
            "y el zoom ampliado) y responde directamente sobre ESE símbolo exacto sin confundirlo con dibujos o pasos anteriores "
            "más arriba en la pizarra (y llama a `trace_data_provenance` si pregunta de dónde salió). "
            "Si el alumno menciona un ejercicio del banco de Física General III (ej. E1..E21, Q001..Q137, Recomendados, Superexamen o un tema de calorimetría, dilatación, conducción, radiación, gases, ciclos p-V o máquinas), "
            "llama PRIMERO a `buscar_leccion_banco` o `abrir_leccion_banco` para abrir la lección verificada en la pizarra y usar SUS cifras exactas (jamás calcules de memoria lo que ya está verificado en Python). "
            "Si el alumno pide buscar un problema de un PDF externo (ej. Q057, Q122 o un tema), llama a `find_and_crop_pdf_problem`. "
            "Si pregunta qué problemas se repiten más en el PDF, llama a `audit_pdf_repeated_problems`. "
            "Antes de resolver una integral nueva, llama a `verify_integral_sympy`. "
            "Si se queda pegado en álgebra intermedia, llama a `open_inline_visual_explanation` "
            "para abrir un espacio en blanco con desarrollo intermedio y gráfica visual."
        )
        from .fisica3.formulario_ii_2026 import FORMULARIO_PROMPT_TXT
        sys_instr += "\n\n" + FORMULARIO_PROMPT_TXT
        if board_context_summary:
            sys_instr += f"\n\nEstado actual de la pizarra:\n{board_context_summary[:8000]}"

        cfg = types.LiveConnectConfig(
            response_modalities=[types.Modality.AUDIO],
            speech_config=types.SpeechConfig(
                voice_config=types.VoiceConfig(
                    prebuilt_voice_config=types.PrebuiltVoiceConfig(voice_name="Kore")
                )
            ),
            system_instruction=types.Content(parts=[types.Part(text=sys_instr)]),
            input_audio_transcription=types.AudioTranscriptionConfig(),
            output_audio_transcription=types.AudioTranscriptionConfig(),
            tools=self._build_live_tools(),
        )

        last_err = ""
        for model_try in (self.model_name, LIVE_AUDIO_MODEL_FALLBACK):
            try:
                ctx = self._client.aio.live.connect(model=model_try, config=cfg)
                sess = await ctx.__aenter__()
                self._ctx_mgr = ctx
                self._session = sess
                self.model_name = model_try
                self._running = True
                self._recv_task = asyncio.create_task(self._receive_loop())
                return {"ok": True, "model": model_try}
            except Exception as exc:
                last_err = sanitize_text(str(exc))[:160]
        return {"ok": False, "error": last_err or "No se pudo conectar a Gemini Live."}

    async def stop(self) -> None:
        self._running = False
        if self._recv_task and not self._recv_task.done():
            self._recv_task.cancel()
            try:
                await self._recv_task
            except asyncio.CancelledError:
                pass
        self._recv_task = None
        if self._ctx_mgr is not None:
            try:
                await self._ctx_mgr.__aexit__(None, None, None)
            except Exception:
                pass
        self._ctx_mgr = None
        self._session = None

    async def send_mic_pcm16(self, pcm_bytes: bytes) -> None:
        if not self.is_running or not pcm_bytes:
            return
        await self._session.send_realtime_input(
            audio=types.Blob(data=pcm_bytes, mime_type="audio/pcm;rate=16000")
        )

    async def send_audio_end(self) -> None:
        if not self.is_running:
            return
        await self._session.send_realtime_input(audio_stream_end=True)

    async def send_text_turn(self, text: str) -> None:
        if not self.is_running or not text:
            return
        await self._session.send_realtime_input(text=text)

    @staticmethod
    def _ensure_jpeg_bytes(img_bytes: bytes, mime_type: str = "image/jpeg") -> bytes:
        """Convert PNG/other image bytes to crisp JPEG because Gemini Live video/vision expects image/jpeg."""
        if not img_bytes:
            return b""
        if img_bytes[:2] == b"\xff\xd8" and "jpeg" in (mime_type or "").lower():
            return img_bytes
        try:
            import fitz  # PyMuPDF already installed
            pix = fitz.Pixmap(img_bytes)
            if pix.alpha:
                pix = fitz.Pixmap(fitz.csRGB, pix)
            return pix.tobytes("jpeg", jpg_quality=88)
        except Exception:
            return img_bytes

    async def send_silent_context(self, text: str) -> None:
        """Inject ground-truth board/lasso/PDF context into Gemini Live without forcing an immediate reply."""
        if not self.is_running or not text:
            return
        try:
            await self._session.send_client_content(
                turns=[types.Content(role="user", parts=[types.Part(text=text)])],
                turn_complete=False,
            )
        except Exception:
            pass

    async def send_visual_context_bundle(
        self,
        images: List[Tuple[bytes, str]],
        context_text: str,
        turn_complete: bool = False,
    ) -> None:
        """Send strict JPEG image(s) + ground-truth text together via send_client_content
        AND latest JPEG frame via send_realtime_input(video=...) for 100% deterministic Live vision ordering!
        """
        if not self.is_running:
            return
        parts: List[types.Part] = []
        last_jpeg: bytes = b""
        for raw_b, raw_mime in images:
            if not raw_b:
                continue
            jpg = self._ensure_jpeg_bytes(raw_b, raw_mime)
            if jpg:
                last_jpeg = jpg
                parts.append(types.Part(inline_data=types.Blob(data=jpg, mime_type="image/jpeg")))
        if context_text:
            parts.append(types.Part(text=context_text))

        if last_jpeg:
            try:
                await self._session.send_realtime_input(
                    video=types.Blob(data=last_jpeg, mime_type="image/jpeg")
                )
            except Exception:
                pass
        if parts:
            try:
                await self._session.send_client_content(
                    turns=[types.Content(role="user", parts=parts)],
                    turn_complete=bool(turn_complete),
                )
            except Exception:
                pass

    async def send_canvas_jpeg_or_png(self, img_bytes: bytes, mime_type: str = "image/jpeg") -> None:
        if not self.is_running or not img_bytes:
            return
        jpg = self._ensure_jpeg_bytes(img_bytes, mime_type)
        await self._session.send_realtime_input(
            video=types.Blob(data=jpg, mime_type="image/jpeg")
        )

    async def _receive_loop(self) -> None:
        unexpected_close = False
        try:
            # IMPORTANT: google-genai AsyncSession.receive() yields a SINGLE turn and breaks
            # on `turn_complete=True`. Wrap it in `while self._running` so multi-turn live
            # voice conversation stays open continuously across unlimited turns!
            while self._running and self._session is not None:
                got_any = False
                turn_had_voice = False
                async for response in self._session.receive():
                    got_any = True
                    if not self._running:
                        break
                    content = getattr(response, "server_content", None)
                    if content:
                        if getattr(content, "interrupted", False):
                            turn_had_voice = False
                            if self._on_event_out:
                                await self._on_event_out({"type": "CONTROL_HARD_STOP", "reason": "gemini_live_barge_in"})
                        mt = getattr(content, "model_turn", None)
                        if mt and getattr(mt, "parts", None):
                            for part in mt.parts:
                                inline = getattr(part, "inline_data", None)
                                if inline and getattr(inline, "data", None) and self._on_pcm_out:
                                    turn_had_voice = True
                                    await self._on_pcm_out(inline.data)
                        in_tr = getattr(content, "input_transcription", None)
                        if in_tr and getattr(in_tr, "text", None) and self._on_event_out:
                            await self._on_event_out({"type": "ASR_FINAL", "text": in_tr.text})
                        out_tr = getattr(content, "output_transcription", None)
                        if out_tr and getattr(out_tr, "text", None) and self._on_event_out:
                            turn_had_voice = True
                            await self._on_event_out({"type": "LIVE_OUTPUT_TRANSCRIPT", "text": out_tr.text})
                        if getattr(content, "turn_complete", False) and self._on_event_out:
                            if turn_had_voice:
                                await self._on_event_out({"type": "STATE_CHANGE", "state": "WAITING_FOR_STUDENT"})
                            turn_had_voice = False

                    tc = getattr(response, "tool_call", None)
                    if tc and getattr(tc, "function_calls", None):
                        responses = []
                        for fc in tc.function_calls:
                            name = getattr(fc, "name", "")
                            args = dict(getattr(fc, "args", None) or {})
                            call_id = getattr(fc, "id", None)
                            result_obj: Dict[str, Any] = {"ok": True}
                            try:
                                if name == "verify_integral_sympy":
                                    plan = build_verified_integral_plan(
                                        str(args.get("integrand", "")),
                                        str(args.get("variable", "x")),
                                        args.get("lower"),
                                        args.get("upper"),
                                    )
                                    result_obj = plan.to_dict()
                                elif self._on_tool_beat:
                                    result_obj = await self._on_tool_beat({"tool_name": name, "args": args})
                            except Exception as tool_exc:
                                result_obj = {"ok": False, "error": sanitize_text(str(tool_exc))[:140]}
                            responses.append(
                                types.FunctionResponse(
                                    id=call_id,
                                    name=name,
                                    response=result_obj,
                                )
                            )
                        if responses and self._session:
                            await self._session.send_tool_response(function_responses=responses)

                if not got_any:
                    unexpected_close = self._running
                    break
        except asyncio.CancelledError:
            pass
        except Exception as exc:
            unexpected_close = self._running
            if self._on_event_out:
                await self._on_event_out({
                    "type": "ERROR_RECOVERY",
                    "scope": "gemini_live",
                    "message": sanitize_text(str(exc))[:180],
                })
        finally:
            was_running = self._running
            self._running = False
            if unexpected_close and was_running and self._on_event_out:
                try:
                    await self._on_event_out({
                        "type": "LIVE_VOICE_STATUS",
                        "ok": False,
                        "active": False,
                        "error": "La conexión de voz en vivo se cerró. Pulsa Hablar para reconectar.",
                    })
                except Exception:
                    pass
