"""FastAPI + Binary/JSON WebSocket Server for CuadernU Audiovisual Whiteboard Tutor.

Launch modes:
  1. Embedded sidecar from CuadernU.exe:
     python -u tools/tutor_runtime/server.py --bootstrap-stdin
     Reads single-line JSON from stdin (session_token, optional vault keys),
     binds ephemeral loopback port, and prints {"ready": true, "port": <N>}.
  2. Direct standalone test/dev mode:
     python -u tools/tutor_runtime/server.py --port 8765
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import json
import pathlib
import re
import socket
import struct
import sys
import time
import uuid
from typing import Any, Dict, List, Optional

import uvicorn
from fastapi import FastAPI, HTTPException, Query, Request, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.tutor_runtime.banco_api import Progreso, buscar as buscar_banco, crear_router
from tools.tutor_runtime.fisica3 import registro as registro_banco
from tools.tutor_runtime.vision import generar_leccion_ia, reconocer_imagen
from tools.tutor_runtime.demo_lessons import get_lesson_beats, get_seed_lessons_catalog
from tools.tutor_runtime.gemini_engines import (
    AlignedSpanishTts,
    GeminiLiveVoiceBridge,
    GeminiVisualDirector,
    estimate_cues_fallback,
    parse_cue_markup,
)
from tools.tutor_runtime.pdf_rag import (
    LocalPdfRagStore,
    find_phrase_norm_box,
    fold_accents,
    normalize_problem_id,
)
from tools.tutor_runtime.pedagogy_memory import PedagogyMemoryStore
from tools.tutor_runtime.secrets_dpapi import load_tutor_secrets, sanitize_text
from tools.tutor_runtime.sympy_verifier import verify_student_step
from tools.tutor_runtime.documentos import Documentos
from tools.tutor_runtime.voz import ServicioVoz
from tools.tutor_runtime.telemetry_recorder import (
    LocalTelemetryRecorder,
    detect_tutor_antipattern,
    detect_user_correction,
)

BINARY_MAGIC = b"CU"
CH_MIC_PCM16 = 1
CH_TUTOR_PCM24 = 2
RUNTIME_BUILD = "2026-11-25-universal-v6-hardened"
_RUNTIME_DIR = pathlib.Path(__file__).resolve().parent
_SERVER_STARTED_AT = time.time()


def _compute_runtime_max_mtime() -> float:
    latest = 0.0
    try:
        for p in _RUNTIME_DIR.glob("*.py"):
            try:
                mt = p.stat().st_mtime
                if mt > latest:
                    latest = mt
            except OSError:
                pass
    except OSError:
        pass
    return latest


_BOOT_CODE_MTIME = _compute_runtime_max_mtime()


def is_runtime_code_stale() -> bool:
    """Return True if any tutor_runtime/*.py file on disk was modified after this process booted."""
    return _compute_runtime_max_mtime() > (_BOOT_CODE_MTIME + 0.5)

_LOCAL_PDF_PATH_RE = re.compile(r'["\']?([A-Za-z]:\\[^"\'\r\n<>|]+?\.pdf)["\']?', re.IGNORECASE)
_PROBLEM_ID_QUERY_RE = re.compile(
    r"\b(?:([qQ]\s*0*\d{1,4})|(?:problema|ejercicio|pregunta)\s+#?0*(\d{1,4}))\b",
    re.IGNORECASE,
)
_AUDIT_REPEATED_RE = re.compile(
    r"\b(mas\s+se\s+repiten|se\s+repiten|repetid[oa]s|frecuencia|audita[r]?|resumen\s+del\s+pdf|temas?\s+que\s+mas\s+salen|patrones?\s+repetidos?)\b",
    re.IGNORECASE,
)
_TOPIC_SEARCH_RE = re.compile(
    r"\b(busca|busque|buscar|encuentra|dame|muestra|trae|quiero|resuelve|resuelva)\b.*\b(problema|ejercicio|pregunta|tema|sobre|de)\b",
    re.IGNORECASE,
)
_AFFIRMATIVE_CONTINUE_RE = re.compile(
    r"^\s*(si|dale|claro|entendi|todo\s+claro|perfecto|listo|ok|okay|sigue|siga|continua|continuemos|siguiente|avanza|vamos|si,\s*entendi.*)\s*[.!¡?¿]*\s*$",
    re.IGNORECASE,
)
_EXPLAIN_SPACE_VISUAL_RE = re.compile(
    r"\b(de\s+donde\s+salio|pasos?\s+intermedios?|me\s+perdi|me\s+quede\s+pegado|no\s+entendi|grafica|dibujo|visualmente|espacio\s+en\s+blanco)\b",
    re.IGNORECASE,
)
_LIVE_CLEAR_ASR_RE = re.compile(
    r"\b(borr[aeo]\w*|limpi[aeo]\w*|vaci[aeo]\w*|quit[aeo]\w*)\b.*\b(pizarra|pizarron|tablero|todo|toda|pantalla)\b"
    r"|\b(pizarra|pizarron)\s+(limpia|en\s+blanco|nueva|vacia)\b",
    re.IGNORECASE,
)
_LIVE_CLEAR_MODEL_CLAIM_RE = re.compile(
    r"\b(ya\s+(?:la\s+|te\s+la\s+|lo\s+)?(?:limpie|borre|esta\s+limpia|quedo\s+limpia|esta\s+en\s+blanco|quedo\s+en\s+blanco|esta\s+vacia))"
    r"|\b(?:he|acabo\s+de)\s+(?:borrado|borrar|limpiado|limpiar)\s+(?:todo|toda|la\s+pizarra|el\s+pizarron)"
    r"|\b(?:la\s+)?pizarra\s+(?:ya\s+)?(?:esta|quedo)\s+(?:totalmente\s+)?(?:limpia|borrada|vacia|en\s+blanco)\b",
    re.IGNORECASE,
)


def detect_live_clear_intent(event_type: str, text_buffer: str) -> bool:
    """Deterministic guardrail so 'limpia la pizarra' NEVER gets hallucinated without clearing."""
    folded = fold_accents(text_buffer or "")
    if not folded:
        return False
    if event_type == "ASR_FINAL":
        return bool(_LIVE_CLEAR_ASR_RE.search(folded))
    if event_type == "LIVE_OUTPUT_TRANSCRIPT":
        return bool(_LIVE_CLEAR_MODEL_CLAIM_RE.search(folded))
    return False


def _extract_problem_id_from_query(text: str) -> Optional[str]:
    m = _PROBLEM_ID_QUERY_RE.search(text or "")
    if not m:
        return None
    if m.group(1):
        return normalize_problem_id(m.group(1))
    if m.group(2):
        return normalize_problem_id(f"Q{int(m.group(2)):03d}")
    return None


def _strip_local_pdf_paths(text: str) -> str:
    return _LOCAL_PDF_PATH_RE.sub("", text or "").strip(" ,.;:-")


class TutorRuntimeContext:
    def __init__(self, bootstrap: Dict[str, Any]) -> None:
        self.session_token: str = str(bootstrap.get("session_token") or "")
        self.repo_root = pathlib.Path(bootstrap.get("repo_root") or str(REPO_ROOT))
        self.assets_web_dir = pathlib.Path(
            bootstrap.get("assets_web_dir") or str(self.repo_root / "assets" / "cuadernu" / "tutor_web")
        )
        dpapi_secrets = {}
        try:
            dpapi_secrets = load_tutor_secrets()
        except Exception:
            dpapi_secrets = {}

        visual_key = (
            dpapi_secrets.get("gemini_visual_key")
            or bootstrap.get("vault_gemini_key")
            or ""
        )
        audio_key = (
            dpapi_secrets.get("gemini_audio_key")
            or visual_key
            or ""
        )

        self.visual_director = GeminiVisualDirector(
            api_key=visual_key,
            primary_model=str(bootstrap.get("visual_model") or "gemini-2.5-flash"),
        )
        self.audio_api_key = audio_key
        self.live_audio_model = str(bootstrap.get("live_audio_model") or "gemini-3.8-live")
        self.tts = AlignedSpanishTts(api_key=audio_key, voice=str(bootstrap.get("tts_voice") or "Kore"))
        self.pdf_store = LocalPdfRagStore(db_path=":memory:")
        mem_path = self.repo_root / "private_local" / "tutor_memory.sqlite"
        self.memory = PedagogyMemoryStore(db_path=mem_path if mem_path.parent.exists() else ":memory:")
        telem_dir_raw = bootstrap.get("telemetry_dir")
        telem_dir = pathlib.Path(telem_dir_raw) if telem_dir_raw else (self.repo_root / "private_local" / "telemetry")
        telem_enabled = bool(bootstrap.get("telemetry_enabled", True)) and (
            telem_dir_raw is not None or (self.repo_root / "private_local").exists()
        )
        self.telemetry = LocalTelemetryRecorder(base_dir=telem_dir, enabled=telem_enabled)
        privada = self.repo_root / "private_local"
        hay_privada = privada.exists()
        self.voz = ServicioVoz(audio_key, (privada / "voz_cache") if hay_privada else None, voz=str(bootstrap.get("tts_voice") or "es-MX-DaliaNeural"))
        self.progreso = Progreso((privada / "tutor_progreso.json") if hay_privada else None)
        self.documentos = Documentos(self.repo_root, cache=(privada / "tutor_recortes") if hay_privada else None)
        self.sessions: Dict[str, Dict[str, Any]] = {}


_ALLOWED_ORIGIN_RE = re.compile(
    r"^(?:https://tutor\.cuadernu\.local|https?://(?:127\.0\.0\.1|localhost|\[::1\])(?::\d{1,5})?)$",
    re.IGNORECASE,
)
_ALLOWED_HOST_RE = re.compile(
    r"^(?:127\.0\.0\.1|localhost|\[::1\]|testserver|tutor\.cuadernu\.local)(?::\d{1,5})?$",
    re.IGNORECASE,
)
_SESSION_ID_RE = re.compile(r"^[A-Za-z0-9_-]{6,48}$")
_SESSION_TTL_SEC = 7200.0
_MAX_SESSIONS = 32


def _is_allowed_origin(origin: str) -> bool:
    if not origin:
        return True
    return bool(_ALLOWED_ORIGIN_RE.match(origin.strip()))


def _is_allowed_host(host: str) -> bool:
    if not host:
        return True
    return bool(_ALLOWED_HOST_RE.match(host.strip()))


def create_app(bootstrap: Optional[Dict[str, Any]] = None) -> FastAPI:
    ctx = TutorRuntimeContext(bootstrap or {})
    app = FastAPI(title="CuadernU Audiovisual Tutor Runtime", version="1.1.0")
    app.state.ctx = ctx

    app.add_middleware(
        CORSMiddleware,
        allow_origins=["https://tutor.cuadernu.local"],
        allow_origin_regex=r"^https?://(127\.0\.0\.1|localhost|\[::1\])(:\d{1,5})?$",
        allow_credentials=False,
        allow_methods=["GET", "POST", "OPTIONS"],
        allow_headers=["Content-Type", "X-CuadernU-Token"],
    )

    @app.middleware("http")
    async def _enforce_loopback_origin_and_host(request: Request, call_next):
        host = request.headers.get("host", "")
        origin = request.headers.get("origin", "")
        if not _is_allowed_host(host) or not _is_allowed_origin(origin):
            return JSONResponse(status_code=403, content={"detail": "Origen o host no permitido"})
        return await call_next(request)

    def _check_token(token_q: Optional[str], req: Optional[Request] = None) -> None:
        if not ctx.session_token:
            return
        hdr = req.headers.get("X-CuadernU-Token", "") if req else ""
        if token_q != ctx.session_token and hdr != ctx.session_token:
            raise HTTPException(status_code=401, detail="Token de sesión inválido")

    def _revisa_token(req: Request) -> None:
        _check_token(req.query_params.get("token"), req)

    app.include_router(crear_router(ctx.voz, ctx.progreso, _revisa_token, ctx.documentos, ctx.visual_director))

    @app.get("/api/health")
    async def api_health(request: Request, token: Optional[str] = Query(default=None)) -> Dict[str, Any]:
        _check_token(token, request)
        return {
            "ok": True,
            "protocol": "cuadernu.tutor.v1.1",
            "runtime_build": RUNTIME_BUILD,
            "started_at_unix": round(_SERVER_STARTED_AT, 2),
            "code_stale": is_runtime_code_stale(),
            "capabilities": [
                "universal_notes",
                "custom_diagram",
                "unit_cancel",
                "provenance_trace",
                "lasso_vision",
                "pasted_image_live_vision",
                "speech_locked_handwriting",
                "local_telemetry_flight_recorder",
                "clear_guardrail",
            ],
            "telemetry": ctx.telemetry.get_summary(),
            "visual_cloud_ready": ctx.visual_director.configured,
            "visual_model": ctx.visual_director.primary_model,
            "live_audio_ready": bool(ctx.audio_api_key),
            "live_audio_model": ctx.live_audio_model,
            "tts_voice": ctx.tts.voice,
            "pdfs_loaded": len(ctx.pdf_store.list_docs()),
        }

    @app.get("/api/lessons")
    async def api_lessons(request: Request, token: Optional[str] = Query(default=None)) -> Dict[str, Any]:
        _check_token(token, request)
        return {"lessons": get_seed_lessons_catalog()}

    def _index_and_summarize_pdf(filename: str, raw: bytes, file_path: str = "") -> Dict[str, Any]:
        meta = ctx.pdf_store.index_pdf_bytes(filename, raw, file_path=file_path)
        first_page = ctx.pdf_store.render_page_png(meta.doc_id, 1)
        audit = ctx.pdf_store.audit_repeated_problems(meta.doc_id, top_k=6)
        return {
            "ok": True,
            "doc_id": meta.doc_id,
            "filename": meta.filename,
            "page_count": meta.page_count,
            "problem_count": meta.problem_count,
            "first_page": first_page,
            "audit": audit,
        }

    @app.post("/api/pdf/upload")
    async def api_pdf_upload(request: Request, token: Optional[str] = Query(default=None)) -> Dict[str, Any]:
        _check_token(token, request)
        body = await request.json()
        filename = str(body.get("filename") or "documento.pdf")
        b64 = str(body.get("pdf_b64") or "")
        if not b64:
            raise HTTPException(status_code=400, detail="Falta pdf_b64")
        try:
            raw = base64.b64decode(b64)
            return await asyncio.to_thread(_index_and_summarize_pdf, filename, raw)
        except Exception as exc:
            raise HTTPException(status_code=400, detail=sanitize_text(str(exc)))

    @app.post("/api/pdf/upload_raw")
    async def api_pdf_upload_raw(
        request: Request,
        filename: str = Query(default="documento.pdf"),
        token: Optional[str] = Query(default=None),
    ) -> Dict[str, Any]:
        """Fast raw binary PDF upload (avoids base64 memory bloat for large exam banks)."""
        _check_token(token, request)
        raw = await request.body()
        if not raw:
            raise HTTPException(status_code=400, detail="Cuerpo PDF vacío")
        try:
            return await asyncio.to_thread(_index_and_summarize_pdf, filename, raw)
        except Exception as exc:
            raise HTTPException(status_code=400, detail=sanitize_text(str(exc)))

    @app.post("/api/pdf/open_local")
    async def api_pdf_open_local(request: Request, token: Optional[str] = Query(default=None)) -> Dict[str, Any]:
        _check_token(token, request)
        body = await request.json()
        path_str = str(body.get("path") or "").strip().strip('"').strip("'")
        p = pathlib.Path(path_str)
        if not path_str or not p.is_file() or p.suffix.lower() != ".pdf":
            raise HTTPException(status_code=400, detail="Ruta PDF local no encontrada o inválida")
        try:
            raw = p.read_bytes()
            return await asyncio.to_thread(_index_and_summarize_pdf, p.name, raw, str(p))
        except Exception as exc:
            raise HTTPException(status_code=400, detail=sanitize_text(str(exc)))

    @app.get("/api/pdf/audit/{doc_id}")
    async def api_pdf_audit(
        doc_id: str,
        request: Request,
        top_k: int = Query(default=8),
        token: Optional[str] = Query(default=None),
    ) -> Dict[str, Any]:
        _check_token(token, request)
        res = ctx.pdf_store.audit_repeated_problems(doc_id, top_k=max(3, min(top_k, 16)))
        if not res.get("ok"):
            raise HTTPException(status_code=404, detail=res.get("error", "PDF no encontrado"))
        return res

    @app.get("/api/pdf/problem/{doc_id}/{problem_id}")
    async def api_pdf_problem(
        doc_id: str,
        problem_id: str,
        request: Request,
        token: Optional[str] = Query(default=None),
    ) -> Dict[str, Any]:
        _check_token(token, request)
        try:
            return ctx.pdf_store.crop_problem_visual(doc_id, problem_id)
        except KeyError as exc:
            raise HTTPException(status_code=404, detail=str(exc))

    @app.get("/api/pdf/search")
    async def api_pdf_search(
        request: Request,
        q: str = Query(default=""),
        doc_id: Optional[str] = Query(default=None),
        limit: int = Query(default=8),
        token: Optional[str] = Query(default=None),
    ) -> Dict[str, Any]:
        _check_token(token, request)
        hits = ctx.pdf_store.search_problems(q, doc_id=doc_id, limit=max(1, min(limit, 16)))
        return {"results": hits}

    @app.get("/api/pdf/page/{doc_id}/{page_num}")
    async def api_pdf_page(
        doc_id: str,
        page_num: int,
        request: Request,
        token: Optional[str] = Query(default=None),
    ) -> Dict[str, Any]:
        _check_token(token, request)
        try:
            return ctx.pdf_store.render_page_png(doc_id, page_num)
        except KeyError as exc:
            raise HTTPException(status_code=404, detail=str(exc))

    @app.post("/api/verify/step")
    async def api_verify_step(request: Request, token: Optional[str] = Query(default=None)) -> Dict[str, Any]:
        _check_token(token, request)
        body = await request.json()
        expected = str(body.get("expected_expr") or "")
        student = str(body.get("student_expr") or "")
        var_name = str(body.get("variable") or "x")
        integrand = body.get("integrand")
        skill_id = str(body.get("skill_id") or "calc.int.general")
        skill_title = str(body.get("skill_title") or "Integración paso a paso")

        res = verify_student_step(expected, student, var_name=var_name, integrand_text=integrand)
        mastery = ctx.memory.record_practice_outcome(skill_id, skill_title, bool(res["equivalent"]))
        misconception = None
        if not res["equivalent"] and res["error_category"] != "SyntaxError":
            misconception = ctx.memory.log_misconception(
                skill_id=skill_id,
                error_category=res["error_category"],
                wrong_expr=student,
                correct_expr=expected,
                note_es=res["message_es"],
            )
        return {"verification": res, "mastery": mastery, "misconception": misconception}

    @app.get("/api/memory/summary")
    async def api_memory_summary(request: Request, token: Optional[str] = Query(default=None)) -> Dict[str, Any]:
        _check_token(token, request)
        return ctx.memory.build_pedagogical_summary()

    @app.get("/api/telemetry/summary")
    async def api_telemetry_summary(request: Request, token: Optional[str] = Query(default=None)) -> Dict[str, Any]:
        _check_token(token, request)
        return ctx.telemetry.get_summary()

    @app.websocket("/ws/tutor")
    async def ws_tutor(
        ws: WebSocket,
        token: Optional[str] = Query(default=None),
        sesion: Optional[str] = Query(default=None),
        session_id_q: Optional[str] = Query(default=None, alias="session_id"),
    ) -> None:
        origin = ws.headers.get("origin", "")
        host = ws.headers.get("host", "")
        if not _is_allowed_origin(origin) or not _is_allowed_host(host):
            await ws.close(code=4403)
            return
        hdr_tok = ws.headers.get("x-cuadernu-token", "")
        if ctx.session_token and token != ctx.session_token and hdr_tok != ctx.session_token:
            await ws.close(code=4401)
            return
        await ws.accept()

        now = time.time()
        # Evict expired sessions
        for k in [sid for sid, st in ctx.sessions.items() if now - float(st.get("updated_at", 0.0)) > _SESSION_TTL_SEC]:
            ctx.sessions.pop(k, None)

        requested_sid = (sesion or session_id_q or "").strip()
        if requested_sid and _SESSION_ID_RE.match(requested_sid):
            session_id = requested_sid
        else:
            session_id = f"ses_{uuid.uuid4().hex[:10]}"

        saved_state = ctx.sessions.get(session_id)
        resumed = saved_state is not None
        beat_counter = int(saved_state["beat_counter"]) if saved_state else 0
        lesson_beats: List[Dict[str, Any]] = list(saved_state["lesson_beats"]) if saved_state else []
        lesson_cursor = int(saved_state["lesson_cursor"]) if saved_state else -1
        scene_history: List[Dict[str, Any]] = list(saved_state["scene_history"]) if saved_state else []
        provenance_edges: List[Dict[str, Any]] = list(saved_state["provenance_edges"]) if saved_state else []
        active_problem: Optional[Dict[str, Any]] = dict(saved_state["active_problem"]) if (saved_state and saved_state.get("active_problem")) else None
        speech_enabled = bool(saved_state["speech_enabled"]) if saved_state else True
        speech_rate = float(saved_state["speech_rate"]) if saved_state else 0.96
        tts_voice = str(saved_state["tts_voice"]) if saved_state else ctx.tts.voice
        active_task: Optional[asyncio.Task[Any]] = None
        out_seq = 0
        mic_frames_received = 0
        ws_lock = asyncio.Lock()

        def _persist_session_state() -> None:
            if len(ctx.sessions) >= _MAX_SESSIONS and session_id not in ctx.sessions:
                oldest = min(ctx.sessions.items(), key=lambda kv: float(kv[1].get("updated_at", 0.0)))[0]
                ctx.sessions.pop(oldest, None)
            ctx.sessions[session_id] = {
                "beat_counter": beat_counter,
                "lesson_beats": lesson_beats[-24:],
                "lesson_cursor": lesson_cursor,
                "scene_history": scene_history[-60:],
                "provenance_edges": provenance_edges[-40:],
                "active_problem": dict(active_problem) if active_problem else None,
                "speech_enabled": speech_enabled,
                "speech_rate": speech_rate,
                "tts_voice": tts_voice,
                "updated_at": time.time(),
            }

        async def send_json_safe(payload: Dict[str, Any]) -> None:
            async with ws_lock:
                await ws.send_text(json.dumps(payload, ensure_ascii=False))

        async def send_pcm24_safe(pcm_bytes: bytes) -> None:
            nonlocal out_seq
            if not pcm_bytes:
                return
            out_seq += 1
            header = struct.pack("<2sBBI", BINARY_MAGIC, CH_TUTOR_PCM24, 0, out_seq)
            async with ws_lock:
                await ws.send_bytes(header + pcm_bytes)

        def last_math_object_id() -> str:
            for item in reversed(scene_history):
                if item.get("type") == "write_math" and item.get("object_id"):
                    return str(item["object_id"])
            return scene_history[-1]["object_id"] if scene_history else ""

        async def record_and_dispatch_beat(
            beat: Dict[str, Any],
            meta: Optional[Dict[str, Any]] = None,
            synthesize_tts: bool = True,
        ) -> None:
            clip_oid = (active_problem or {}).get("clip_object_id") or ""
            crop_words = (active_problem or {}).get("crop_words") or []
            for act in beat.get("actions", []):
                payload = act.get("payload")
                if isinstance(payload, dict) and crop_words and clip_oid:
                    # Enrich PDF statement annotations from Visual Director with exact PyMuPDF norm_box
                    t_oid = str(payload.get("target_object_id") or "")
                    m_txt = str(payload.get("match_text") or payload.get("source_text") or "").strip()
                    if t_oid == clip_oid and m_txt and not payload.get("norm_box"):
                        nb = find_phrase_norm_box(m_txt, crop_words)
                        if nb:
                            payload["norm_box"] = nb
                oid = act.get("object_id")
                if oid:
                    # Avoid storing giant base64 strings or full word tables inside LLM scene_summary history
                    clean_payload = {
                        k: v for k, v in (act.get("payload") or {}).items()
                        if not str(k).endswith("_b64") and k != "crop_words"
                    }
                    scene_history.append({
                        "object_id": oid,
                        "type": act.get("type"),
                        "step_index": beat.get("step_index", 0),
                        "checkpoint_id": beat.get("checkpoint_id", ""),
                        "payload": clean_payload,
                    })
            for pe in beat.get("provenance", []):
                provenance_edges.append(pe)

            await send_json_safe({
                "type": "BEAT_PLAN",
                "beat": beat,
                "meta": meta or {},
            })

            if synthesize_tts and speech_enabled:
                eff_rate = float(beat.get("speech_rate", speech_rate))
                tts_res = await ctx.tts.synthesize_with_cues(
                    beat.get("speech_markup", ""),
                    speech_rate=eff_rate,
                    voice_override=tts_voice,
                )
                await send_json_safe({
                    "type": "AUDIO_ALIGNMENT",
                    "beat_id": beat.get("beat_id"),
                    "engine": tts_res.get("engine"),
                    "voice": tts_res.get("voice", tts_voice),
                    "plain_text": tts_res.get("plain_text", ""),
                    "audio_b64": tts_res.get("audio_b64", ""),
                    "audio_mime": tts_res.get("audio_mime", "audio/wav"),
                    "audio_mp3_b64": tts_res.get("audio_mp3_b64", ""),
                    "speech_rate": eff_rate,
                    "cues_ms": tts_res.get("cues_ms", {}),
                    "words": tts_res.get("words", []),
                    "duration_ms": tts_res.get("duration_ms", 0),
                })
            else:
                clean_txt, cues = parse_cue_markup(beat.get("speech_markup", ""))
                fb = estimate_cues_fallback(clean_txt, cues, speech_rate)
                await send_json_safe({
                    "type": "AUDIO_ALIGNMENT",
                    "beat_id": beat.get("beat_id"),
                    "engine": "silent_visual_only",
                    "voice": tts_voice,
                    "plain_text": clean_txt,
                    "audio_b64": "",
                    "audio_mime": "",
                    "audio_mp3_b64": "",
                    "speech_rate": speech_rate,
                    "cues_ms": fb["cues_ms"],
                    "words": fb["words"],
                    "duration_ms": fb["duration_ms"],
                })

        async def dispatch_pdf_audit_beat(doc_id: Optional[str] = None, synthesize_tts: bool = True) -> Dict[str, Any]:
            nonlocal beat_counter
            audit = ctx.pdf_store.audit_repeated_problems(doc_id, top_k=8)
            if not audit.get("ok"):
                return audit

            beat_counter += 1
            top_arch = audit.get("top_archetypes", [])
            dups = audit.get("near_duplicate_clusters", [])
            top_str = ", ".join(f"{a['subtema']} ({a['count']} ejercicios)" for a in top_arch[:3])
            dup_str = (
                f" Además detecté {len(dups)} grupos de problemas casi repetidos con otros datos, como "
                + ", ".join(" y ".join(c["problem_ids"][:2]) for c in dups[:2])
                + "."
                if dups
                else ""
            )
            speech = (
                f"<cue id='c1'/> Revisé visualmente todo el documento {audit['filename']}: "
                f"tiene {audit['total_problems']} problemas ({audit['problems_with_figures']} con figuras). "
                f"Los temas que más se repiten son {top_str}.{dup_str} "
                "Toca cualquier código en la tarjeta o dime cuál quieres que recorte y desarrolle paso a paso."
            )
            suggested: List[str] = []
            if dups:
                suggested.append(f"Resolver {dups[0]['problem_ids'][0]}")
            if top_arch and top_arch[0].get("sample_ids"):
                cid0 = top_arch[0]["sample_ids"][0]
                if f"Resolver {cid0}" not in suggested:
                    suggested.append(f"Resolver {cid0}")
            suggested.append("Buscar un problema con figura")

            audit_beat = {
                "protocol": "cuadernu.tutor.v1.1",
                "session_id": session_id,
                "turn_id": f"audit_{beat_counter}",
                "beat_id": f"beat_{beat_counter:04d}",
                "step_index": len(scene_history) + 1,
                "checkpoint_id": f"cp_audit_{beat_counter}",
                "branch_id": None,
                "speech_markup": speech,
                "speech_rate": speech_rate,
                "actions": [
                    {
                        "action_id": f"act_audit_{beat_counter}",
                        "type": "render_pdf_audit",
                        "object_id": f"pdf_audit_{beat_counter}",
                        "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 20},
                        "animation": {"style": "fade_in", "duration_ms": 500, "cue_id": "c1"},
                        "color_token": "accent_blue",
                        "payload": audit,
                    }
                ],
                "provenance": [],
                "verification": {"status": "conceptual"},
                "pedagogy": {
                    "micro_goal": "Auditar temas frecuentes y problemas repetidos del PDF",
                    "concept_tag": "pdf.audit",
                    "wait_for_student": True,
                    "check_question": "¿Cuál de estos problemas repetidos quieres recortar y resolver primero paso a paso?",
                    "suggested_followups": suggested,
                },
            }
            await record_and_dispatch_beat(audit_beat, meta={"model": "pdf_visual_auditor"}, synthesize_tts=synthesize_tts)
            return audit

        async def start_solving_pdf_problem(
            prob_visual: Dict[str, Any],
            custom_question: str = "",
            runner_ups: Optional[List[Dict[str, Any]]] = None,
            start_step_one: bool = True,
        ) -> Dict[str, Any]:
            nonlocal beat_counter, active_problem, lesson_beats, lesson_cursor
            lesson_beats = []
            lesson_cursor = -1

            beat_counter += 1
            clip_oid = f"problem_clip_{prob_visual['problem_id'].lower()}"
            active_problem = {
                "problem_id": prob_visual["problem_id"],
                "doc_id": prob_visual["doc_id"],
                "filename": prob_visual["filename"],
                "page_start": prob_visual["page_start"],
                "subtema": prob_visual.get("subtema", ""),
                "procedimiento": prob_visual.get("procedimiento", ""),
                "source_label": f"{prob_visual['problem_id']} · Pág. {prob_visual['page_start']} ({prob_visual['filename']})",
                "problem_text": prob_visual.get("body_text", ""),
                "problem_png_b64": prob_visual.get("problem_png_b64", ""),
                "figure_png_b64": prob_visual.get("figure_png_b64", ""),
                "crop_words": prob_visual.get("crop_words", []),
                "clip_object_id": clip_oid,
                "step_count": 1,
            }

            # 1. Instantly pin cropped problem + figure onto the whiteboard
            clip_beat = {
                "protocol": "cuadernu.tutor.v1.1",
                "session_id": session_id,
                "turn_id": f"clip_{beat_counter}",
                "beat_id": f"beat_{beat_counter:04d}",
                "step_index": 0,
                "checkpoint_id": f"cp_clip_{prob_visual['problem_id']}",
                "branch_id": None,
                "speech_markup": "",
                "speech_rate": speech_rate,
                "actions": [
                    {
                        "action_id": f"act_clip_{beat_counter}",
                        "type": "paste_problem_clip",
                        "object_id": clip_oid,
                        "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 18},
                        "animation": {"style": "fade_in", "duration_ms": 400, "cue_id": "c1"},
                        "color_token": "accent_blue",
                        "payload": {
                            "problem_id": prob_visual["problem_id"],
                            "doc_id": prob_visual["doc_id"],
                            "filename": prob_visual["filename"],
                            "page": prob_visual["page_start"],
                            "level": prob_visual.get("level", ""),
                            "family": prob_visual.get("family", ""),
                            "subtema": prob_visual.get("subtema", ""),
                            "procedimiento": prob_visual.get("procedimiento", ""),
                            "has_figure": prob_visual.get("has_figure", False),
                            "problem_png_b64": prob_visual.get("problem_png_b64", ""),
                            "figure_png_b64": prob_visual.get("figure_png_b64", ""),
                            "crop_words": prob_visual.get("crop_words", []),
                            "alternatives": [
                                {
                                    "problem_id": r["problem_id"],
                                    "page": r["page_start"],
                                    "subtema": r.get("subtema", ""),
                                }
                                for r in (runner_ups or [])[:4]
                                if r["problem_id"] != prob_visual["problem_id"]
                            ],
                        },
                    }
                ],
                "provenance": [],
                "verification": {"status": "conceptual"},
                "pedagogy": {
                    "micro_goal": f"Recorte visual de {prob_visual['problem_id']}",
                    "wait_for_student": False,
                },
            }
            await record_and_dispatch_beat(clip_beat, meta={"model": "pdf_problem_cropper"}, synthesize_tts=False)

            # If Gemini Live voice is already active, share the FULL problem statement + crisp crop image silently
            if live_bridge.is_running:
                full_stmt = str(prob_visual.get("body_text") or "")[:5500]
                if full_stmt:
                    await live_bridge.send_silent_context(
                        f"[ENUNCIADO COMPLETO EN PIZARRA ({prob_visual['problem_id']}, bloque={clip_oid})]:\n{full_stmt}"
                    )
                if prob_visual.get("problem_png_b64"):
                    try:
                        await live_bridge.send_canvas_jpeg_or_png(
                            base64.b64decode(prob_visual["problem_png_b64"]),
                            mime_type="image/png",
                        )
                    except Exception:
                        pass

            if not start_step_one:
                return active_problem

            # 2. Generate ONLY Step 1 interactively with multimodal crop + figure + warm check-in
            beat_counter += 1
            await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
            prompt = custom_question.strip() or (
                f"Acabamos de recortar y pegar el problema {prob_visual['problem_id']} en la pizarra. "
                "Desarrolla ÚNICAMENTE el Paso 1 (identificar los datos del enunciado con unidades SI, qué pide el problema y la ecuación/ley física inicial). "
                "Explica de dónde sale cada dato y al final pregúntame si entendí o me perdí en algún lado antes de continuar."
            )
            b, m = await ctx.visual_director.generate_beat(
                session_id=session_id,
                beat_num=beat_counter,
                step_index=1,
                user_prompt=prompt,
                intent="start_problem",
                scene_summary=scene_history,
                provenance_edges=provenance_edges,
                rag_citations=[{
                    "doc_name": prob_visual["filename"],
                    "page": prob_visual["page_start"],
                    "excerpt": prob_visual.get("body_text", "")[:350],
                }],
                pedagogy_summary=ctx.memory.build_pedagogical_summary(),
                crop_png_b64=prob_visual.get("problem_png_b64"),
                extra_images_b64=[prob_visual["figure_png_b64"]] if prob_visual.get("figure_png_b64") else [],
                active_problem=active_problem,
            )
            await record_and_dispatch_beat(b, meta=m)
            return active_problem

        async def advance_active_problem_step(user_reply: str = "") -> None:
            nonlocal beat_counter
            if not active_problem:
                return
            active_problem["step_count"] = int(active_problem.get("step_count", 1)) + 1
            next_idx = active_problem["step_count"]
            beat_counter += 1
            await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
            prompt = (
                f"El estudiante confirmó que entendió el paso anterior ('{user_reply or 'Continuar'}'). "
                f"Desarrolla ahora ÚNICAMENTE el Paso {next_idx} del problema {active_problem['problem_id']} sin saltarte pasos algebraicos, "
                "enlazando en `provenance` de dónde sale cada término nuevo. "
                "Si el problema aún tiene pasos o incisos pendientes, termina preguntando si entendió o si se quedó pegado en algo."
            )
            b, m = await ctx.visual_director.generate_beat(
                session_id=session_id,
                beat_num=beat_counter,
                step_index=next_idx,
                user_prompt=prompt,
                intent="next_problem_step",
                scene_summary=scene_history,
                provenance_edges=provenance_edges,
                rag_citations=[],
                pedagogy_summary=ctx.memory.build_pedagogical_summary(),
                crop_png_b64=active_problem.get("problem_png_b64"),
                extra_images_b64=[active_problem["figure_png_b64"]] if active_problem.get("figure_png_b64") else [],
                active_problem=active_problem,
            )
            await record_and_dispatch_beat(b, meta=m)

        def find_scene_object_containing_text(needle: str, exclude_oid: str = "") -> str:
            """Locate the most recent previous step on the whiteboard that produced or contains `needle`."""
            folded_n = fold_accents(needle or "").strip()
            nums = re.findall(r"\d+(?:\.\d+)?", folded_n.replace(",", "."))
            math_objs = [s for s in scene_history if s.get("type") in ("write_math", "expand_intermediate_steps", "write_text")]
            for s in reversed(math_objs):
                oid = str(s.get("object_id") or "")
                if not oid or oid == exclude_oid:
                    continue
                p = s.get("payload") or {}
                blob = fold_accents(
                    f"{p.get('latex', '')} {p.get('step_label', '')} {p.get('note_es', '')} {p.get('text', '')}"
                ).replace(",", ".")
                if folded_n and folded_n in blob:
                    return oid
                if nums and any(num in blob for num in nums):
                    return oid
            # Fallback: return the previous math step before exclude_oid
            for s in reversed(math_objs):
                oid = str(s.get("object_id") or "")
                if oid and oid != exclude_oid:
                    return oid
            return ""

        async def dispatch_live_canvas_action(
            action_dict: Dict[str, Any],
            prov_list: Optional[List[Dict[str, Any]]] = None,
            step_idx: int = 0,
        ) -> None:
            """Low-latency fast-path (< 35 ms) for Gemini Live NON_BLOCKING whiteboard & marker actions."""
            oid = str(action_dict.get("object_id") or "")
            act_type = str(action_dict.get("type") or "")
            if oid and act_type in (
                "write_math",
                "write_structured_notes",
                "draw_sketch",
                "expand_intermediate_steps",
                "draw_inline_graph",
                "write_text",
                "plot_riemann_sum",
                "plot_pv_diagram",
                "draw_thermal_shell",
            ):
                clean_payload = {
                    k: v for k, v in (action_dict.get("payload") or {}).items()
                    if not str(k).endswith("_b64") and k != "crop_words"
                }
                scene_history.append({
                    "object_id": oid,
                    "type": act_type,
                    "step_index": step_idx or (len(scene_history) + 1),
                    "checkpoint_id": f"cp_live_{beat_counter}",
                    "payload": clean_payload,
                })
            for pe in prov_list or []:
                provenance_edges.append(pe)
            await send_json_safe({
                "type": "LIVE_CANVAS_ACTION",
                "action": action_dict,
                "step_index": step_idx or len(scene_history),
                "server_ts_ms": int(time.time() * 1000),
            })

        async def handle_live_tool_call(tool_evt: Dict[str, Any]) -> Dict[str, Any]:
            nonlocal beat_counter, active_problem, lesson_beats, lesson_cursor
            beat_counter += 1
            tname = tool_evt.get("tool_name", "")
            args = tool_evt.get("args", {})
            step_idx = len(scene_history) + 1
            actions: List[Dict[str, Any]] = []
            prov: List[Dict[str, Any]] = []
            extra_result: Dict[str, Any] = {}

            if tname == "clear_whiteboard":
                clear_ink = bool(args.get("clear_ink", True))
                clear_media = bool(args.get("clear_media", True))
                await execute_full_board_reset(
                    clear_ink=clear_ink,
                    clear_media=clear_media,
                    reason="live_tool_clear_whiteboard",
                )
                return {"ok": True, "cleared": True, "clear_ink": clear_ink, "clear_media": clear_media}

            if tname == "buscar_leccion_banco":
                q = str(args.get("query") or "").strip()
                hits = await asyncio.to_thread(buscar_banco, q[:200], 8)
                return {
                    "ok": True,
                    "total": len(hits),
                    "resultados": [
                        {
                            "id": h.get("id"),
                            "titulo": h.get("titulo"),
                            "tema": h.get("tema"),
                            "pasos": h.get("pasos"),
                        }
                        for h in hits
                    ],
                }

            if tname == "abrir_leccion_banco":
                lid = str(args.get("lesson_id") or "").strip().upper()
                if not registro_banco.existe(lid):
                    hits = await asyncio.to_thread(buscar_banco, lid, 3)
                    if hits:
                        lid = str(hits[0]["id"])
                if not registro_banco.existe(lid):
                    return {"ok": False, "error": f"No existe la lección '{lid}' en el banco."}
                lec = await asyncio.to_thread(registro_banco.obtener, lid)
                await send_json_safe({"type": "ABRIR_LECCION", "id": lid, "titulo": lec.titulo})
                resps = [
                    f"{(r.get('inciso') + ') ') if r.get('inciso') else ''}{r.get('tex', '')} {r.get('nota', '')}".strip()
                    for r in (lec.respuestas or [])
                ]
                return {
                    "ok": True,
                    "id": lec.id,
                    "titulo": lec.titulo,
                    "enunciado": (lec.enunciado or "")[:1200],
                    "pasos": [p.titulo for p in lec.pasos],
                    "respuestas_verificadas": resps[:8],
                    "aviso": lec.aviso or "",
                }

            if tname == "find_and_crop_pdf_problem":
                q = str(args.get("query") or "").strip()
                pid = _extract_problem_id_from_query(q)
                prob_vis = None
                if pid:
                    try:
                        prob_vis = ctx.pdf_store.crop_problem_visual(None, pid)
                    except KeyError:
                        prob_vis = None
                if not prob_vis:
                    hits = ctx.pdf_store.search_problems(q, limit=5)
                    if hits:
                        prob_vis = ctx.pdf_store.crop_problem_visual(hits[0]["doc_id"], hits[0]["problem_id"])
                if not prob_vis:
                    return {"ok": False, "error": f"No se encontró '{q}' en el PDF cargado."}
                await start_solving_pdf_problem(prob_vis, start_step_one=False)
                # Send cropped problem image into Gemini Live so voice tutor sees the exact diagram too
                if prob_vis.get("problem_png_b64") and live_bridge.is_running:
                    try:
                        await live_bridge.send_canvas_jpeg_or_png(base64.b64decode(prob_vis["problem_png_b64"]), "image/png")
                    except Exception:
                        pass
                return {
                    "ok": True,
                    "problem_id": prob_vis["problem_id"],
                    "clip_object_id": (active_problem or {}).get("clip_object_id", ""),
                    "page": prob_vis["page_start"],
                    "subtema": prob_vis.get("subtema", ""),
                    "has_figure": prob_vis.get("has_figure", False),
                    "body_text": prob_vis.get("body_text", "")[:5500],
                }

            if tname == "audit_pdf_repeated_problems":
                audit = await dispatch_pdf_audit_beat(synthesize_tts=False)
                if not audit.get("ok"):
                    return {"ok": False, "error": audit.get("error", "No hay PDF cargado")}
                return {
                    "ok": True,
                    "filename": audit["filename"],
                    "total_problems": audit["total_problems"],
                    "top_archetypes": audit["top_archetypes"][:5],
                    "near_duplicate_clusters": audit["near_duplicate_clusters"][:5],
                }

            if tname == "trace_data_provenance":
                val_txt = str(args.get("value_text") or "").strip()
                origin_kind = str(args.get("origin_kind") or "statement").strip().lower()
                src_phrase = str(args.get("source_phrase_or_text") or val_txt).strip()
                tgt_oid = str(args.get("target_object_id") or "").strip() or last_math_object_id()
                tgt_txt = str(args.get("target_text") or val_txt).strip()
                temporary = bool(args.get("temporary", True))
                ttl_ms = max(3000, min(25000, int(args.get("ttl_ms") or 9500)))
                crop_words = (active_problem or {}).get("crop_words") or []
                clip_oid = (active_problem or {}).get("clip_object_id") or ""

                src_oid = str(args.get("source_object_id") or "").strip()
                norm_box = None
                default_color = "orange"
                default_style = "underline"
                default_note = "Dato del enunciado"

                if origin_kind == "statement":
                    src_oid = src_oid or clip_oid
                    norm_box = find_phrase_norm_box(src_phrase, crop_words)
                    if not norm_box and val_txt and val_txt != src_phrase:
                        norm_box = find_phrase_norm_box(val_txt, crop_words)
                elif origin_kind == "intermediate_step":
                    default_color = "purple"
                    default_style = "circle"
                    default_note = "Calculado en paso anterior"
                    if not src_oid:
                        src_oid = find_scene_object_containing_text(src_phrase or val_txt, exclude_oid=tgt_oid)
                else:
                    origin_kind = "constant_or_law"
                    default_color = "teal"
                    default_style = "underline"
                    default_note = f"Constante / Ley ({src_phrase})" if src_phrase else "Constante física"
                    if crop_words and src_phrase:
                        norm_box = find_phrase_norm_box(src_phrase, crop_words)
                        if norm_box:
                            src_oid = src_oid or clip_oid

                color = str(args.get("color") or default_color).strip().lower()
                style = str(args.get("style") or default_style).strip().lower()
                note_es = str(args.get("note_es") or default_note).strip()

                actions.append({
                    "action_id": f"act_prov_{beat_counter}",
                    "type": "trace_provenance",
                    "object_id": f"prov_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "overlay_on"},
                    "animation": {"style": "draw_stroke", "duration_ms": 620},
                    "color_token": f"accent_{color}",
                    "payload": {
                        "origin_kind": origin_kind,
                        "value_text": val_txt,
                        "source_object_id": src_oid,
                        "source_text": src_phrase,
                        "source_norm_box": norm_box,
                        "target_object_id": tgt_oid,
                        "target_text": tgt_txt,
                        "color": color,
                        "style": style,
                        "note_es": note_es,
                        "temporary": temporary,
                        "ttl_ms": ttl_ms,
                    },
                })
                prov.append({
                    "target_ref": f"{tgt_oid}:{tgt_txt}",
                    "source_ref": f"{src_oid}:{src_phrase}",
                    "rule_es": note_es,
                })
                extra_result = {
                    "origin_kind": origin_kind,
                    "source_object_id": src_oid,
                    "target_object_id": tgt_oid,
                    "matched_statement_box": bool(norm_box),
                }
            elif tname == "cancel_units_or_terms":
                tgt_oid = str(args.get("object_id") or "").strip() or last_math_object_id()
                palette_cycle = ["red", "green", "purple", "orange", "teal", "pink", "blue"]
                raw_pairs = args.get("pairs") if isinstance(args.get("pairs"), list) else []
                clean_pairs = []
                for idx, item in enumerate(raw_pairs[:8]):
                    if isinstance(item, dict):
                        ut = str(item.get("unit_or_term") or item.get("unit") or item.get("term") or "").strip()
                        if not ut:
                            continue
                        col = str(item.get("color") or palette_cycle[idx % len(palette_cycle)]).strip().lower()
                        clean_pairs.append({
                            "unit_or_term": ut,
                            "color": col,
                            "replacement_note": str(item.get("replacement_note") or "").strip(),
                        })
                    elif isinstance(item, str) and item.strip():
                        clean_pairs.append({
                            "unit_or_term": item.strip(),
                            "color": palette_cycle[idx % len(palette_cycle)],
                            "replacement_note": "",
                        })
                surv = str(args.get("surviving_unit") or "").strip()
                surv_col = str(args.get("surviving_color") or "blue").strip().lower()
                actions.append({
                    "action_id": f"act_cancel_{beat_counter}",
                    "type": "cancel_terms",
                    "object_id": f"cancel_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "overlay_on"},
                    "animation": {"style": "draw_stroke", "duration_ms": 500},
                    "color_token": "accent_red",
                    "payload": {
                        "target_object_id": tgt_oid,
                        "pairs": clean_pairs,
                        "surviving_unit": surv,
                        "surviving_color": surv_col,
                        "temporary": bool(args.get("temporary", False)),
                        "ttl_ms": int(args.get("ttl_ms") or 0),
                    },
                })
                extra_result = {"target_object_id": tgt_oid, "cancelled_pairs": len(clean_pairs), "surviving_unit": surv}
            elif tname == "annotate_whiteboard":
                kind_map = {
                    "underline": "underline_term",
                    "circle": "circle_term",
                    "highlight": "highlight_term",
                    "point": "point_at_term",
                    "colorize": "colorize_term",
                    "margin_note": "write_margin_note",
                    "box_callout": "write_margin_note",
                    "cancel": "cancel_terms",
                }
                raw_act = str(args.get("action") or "underline").strip().lower()
                mapped_type = kind_map.get(raw_act, "underline_term")
                tgt_txt = str(args.get("target_text") or "").strip()
                tgt_oid = str(args.get("object_id") or "").strip()
                crop_words = (active_problem or {}).get("crop_words") or []
                clip_oid = (active_problem or {}).get("clip_object_id") or ""
                norm_box = args.get("norm_box") if isinstance(args.get("norm_box"), dict) else None

                if not tgt_oid:
                    tgt_oid = last_math_object_id() or clip_oid
                if tgt_oid == clip_oid and tgt_txt and not norm_box and crop_words:
                    norm_box = find_phrase_norm_box(tgt_txt, crop_words)

                col = str(args.get("color") or "blue").strip().lower()
                payload_ann: Dict[str, Any] = {
                    "target_object_id": tgt_oid,
                    "match_text": tgt_txt,
                    "color": col,
                    "style": str(args.get("style") or "solid").strip().lower(),
                    "note_es": str(args.get("note_es") or "").strip(),
                    "norm_box": norm_box,
                    "temporary": bool(args.get("temporary", raw_act == "point")),
                    "ttl_ms": int(args.get("ttl_ms") or (2400 if raw_act == "point" else 0)),
                }
                if mapped_type == "cancel_terms":
                    payload_ann["pairs"] = [{"unit_or_term": tgt_txt, "color": col}]
                actions.append({
                    "action_id": f"act_ann_{beat_counter}",
                    "type": mapped_type,
                    "object_id": f"ann_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "overlay_on"},
                    "animation": {"style": "draw_stroke", "duration_ms": 480},
                    "color_token": f"accent_{col}",
                    "payload": payload_ann,
                })
                extra_result = {"mapped_type": mapped_type, "target_object_id": tgt_oid, "target_text": tgt_txt}
            elif tname == "write_whiteboard_notes":
                clean_headers = [str(h).strip() for h in (args.get("table_headers") or args.get("headers") or []) if str(h).strip()]
                raw_rows = args.get("table_rows") if args.get("table_rows") is not None else (args.get("rows") or [])
                clean_rows: List[List[str]] = []
                if isinstance(raw_rows, list):
                    for row in raw_rows:
                        if isinstance(row, list):
                            cells = [str(cell).strip() for cell in row]
                            if any(cells):
                                clean_rows.append(cells)
                        elif isinstance(row, dict):
                            if clean_headers:
                                cells = [str(row.get(h, "")).strip() for h in clean_headers]
                                if not any(cells):
                                    cells = [str(v).strip() for v in row.values()]
                            else:
                                cells = [str(v).strip() for v in row.values()]
                            if any(cells):
                                clean_rows.append(cells)
                        elif isinstance(row, str) and row.strip():
                            sep = "|" if "|" in row else ("\t" if "\t" in row else (";" if ";" in row else None))
                            cells = [c.strip() for c in row.split(sep)] if sep else [row.strip()]
                            if any(cells):
                                clean_rows.append(cells)
                clean_items = [str(it).strip() for it in (args.get("items") or []) if str(it).strip()]
                # Rescue case: model passed table_headers and placed pipe-separated rows inside `items`
                if clean_headers and not clean_rows and clean_items and any("|" in it for it in clean_items):
                    for it in clean_items:
                        cells = [c.strip() for c in it.split("|")]
                        if any(cells):
                            clean_rows.append(cells)
                    clean_items = []
                est_notes_ms = max(2400, min(14000, 1200 + len(clean_items) * 1450 + len(clean_rows) * 1300))
                actions.append({
                    "action_id": f"act_notes_{beat_counter}",
                    "type": "write_structured_notes",
                    "object_id": str(args.get("object_id") or f"notes_live_{beat_counter}"),
                    "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 20},
                    "animation": {"style": "handwriting_svg", "duration_ms": est_notes_ms},
                    "color_token": str(args.get("color_token") or "accent_blue"),
                    "payload": {
                        "title": str(args.get("title") or "Apuntes en pizarra"),
                        "items": clean_items,
                        "table_headers": clean_headers,
                        "table_rows": clean_rows,
                        "note_es": str(args.get("note_es") or ""),
                    },
                })
                extra_result = {"title": str(args.get("title") or ""), "items": len(clean_items), "table_rows": len(clean_rows)}
            elif tname == "draw_custom_whiteboard_diagram":
                elems_raw = args.get("elements") if isinstance(args.get("elements"), list) else []
                est_diag_ms = max(2800, min(8000, 1400 + len(elems_raw) * 320))
                actions.append({
                    "action_id": f"act_cdiag_{beat_counter}",
                    "type": "draw_sketch",
                    "object_id": f"diagram_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 18},
                    "animation": {"style": "draw_stroke", "duration_ms": est_diag_ms},
                    "color_token": "accent_blue",
                    "payload": {
                        "kind": "custom_diagram",
                        "title": str(args.get("title") or "Diagrama en pizarra"),
                        "caption_es": str(args.get("caption_es") or ""),
                        "elements": elems_raw,
                    },
                })
                extra_result = {"title": str(args.get("title") or ""), "elements": len(elems_raw)}
            elif tname == "connect_origin_arrow":
                actions.append({
                    "action_id": f"act_arr_{beat_counter}",
                    "type": "trace_origin",
                    "object_id": f"arr_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "overlay_on"},
                    "animation": {"style": "draw_stroke", "duration_ms": 620},
                    "color_token": f"accent_{str(args.get('color') or 'orange').strip().lower()}",
                    "payload": {
                        "from_ref": str(args.get("from_text_or_id") or ""),
                        "to_ref": str(args.get("to_text_or_id") or ""),
                        "color": str(args.get("color") or "orange").strip().lower(),
                        "rule_es": str(args.get("rule_es") or "Origen"),
                        "temporary": bool(args.get("temporary", True)),
                        "ttl_ms": int(args.get("ttl_ms") or 9000),
                    },
                })
            elif tname == "open_inline_visual_explanation":
                after_id = str(args.get("after_object_id") or last_math_object_id())
                substeps = [{"latex": str(lx), "note": ""} for lx in (args.get("substeps_latex") or [])]
                vgraph = None
                if args.get("graph_kind") or args.get("graph_expr"):
                    vgraph = {
                        "kind": str(args.get("graph_kind") or "function_plot"),
                        "title": str(args.get("title") or "Gráfica explicativa"),
                        "expr": str(args.get("graph_expr") or "1/(x^2)"),
                        "x_min": float(args.get("x_min", 0.15)),
                        "x_max": float(args.get("x_max", 1.0)),
                        "x_label": str(args.get("x_label") or "x"),
                        "y_label": str(args.get("y_label") or "y"),
                        "caption_es": str(args.get("caption_es") or ""),
                    }
                actions.append({
                    "action_id": f"act_vexp_{beat_counter}",
                    "type": "expand_intermediate_steps",
                    "object_id": f"exp_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "between", "target_id": after_id, "gap_px": 18},
                    "animation": {"style": "spring_reflow", "duration_ms": 700, "cue_id": "c1"},
                    "color_token": "accent_blue",
                    "payload": {
                        "after_object_id": after_id,
                        "title": str(args.get("title") or "Espacio de aclaración paso a paso"),
                        "substeps": substeps,
                        "visual_graph": vgraph,
                    },
                })
            elif tname == "draw_whiteboard_sketch":
                actions.append({
                    "action_id": f"act_sk_{beat_counter}",
                    "type": "draw_sketch",
                    "object_id": f"sketch_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 18},
                    "animation": {"style": "draw_stroke", "duration_ms": 1800, "cue_id": "c1"},
                    "color_token": "accent_blue",
                    "payload": {
                        "kind": str(args.get("kind") or "piston_cylinder"),
                        "title": str(args.get("title") or "Dibujo en pizarra"),
                        "caption_es": str(args.get("caption_es") or ""),
                        "expr": str(args.get("expr") or ""),
                    },
                })
            elif tname == "write_whiteboard_equation":
                latex_val = str(args.get("latex") or "")
                est_eq_ms = max(1800, min(7800, 1100 + len(latex_val) * 65))
                actions.append({
                    "action_id": f"act_live_{beat_counter}",
                    "type": "write_math",
                    "object_id": str(args.get("object_id") or f"eq_live_{beat_counter}"),
                    "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": est_eq_ms, "cue_id": "c1"},
                    "color_token": str(args.get("color_token") or "ink_primary"),
                    "payload": {
                        "latex": latex_val,
                        "step_label": str(args.get("step_label") or f"Paso {step_idx}"),
                        "note_es": str(args.get("note_es") or ""),
                    },
                })
            elif tname == "trace_symbol_origin":
                actions.append({
                    "action_id": f"act_tr_{beat_counter}",
                    "type": "trace_origin",
                    "object_id": f"trace_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "overlay_on"},
                    "animation": {"style": "draw_stroke", "duration_ms": 650, "cue_id": "c1"},
                    "color_token": "accent_red",
                    "payload": {
                        "from_ref": str(args.get("from_ref") or ""),
                        "to_ref": str(args.get("to_ref") or ""),
                        "color": "red",
                        "rule_es": str(args.get("rule_es") or "Origen del término"),
                        "temporary": True,
                        "ttl_ms": 9500,
                    },
                })
                prov.append({
                    "target_ref": str(args.get("to_ref") or ""),
                    "source_ref": str(args.get("from_ref") or ""),
                    "rule_es": str(args.get("rule_es") or ""),
                })
            elif tname == "expand_intermediate_algebra":
                after_id = str(args.get("after_object_id") or last_math_object_id())
                substeps = [{"latex": str(lx), "note": ""} for lx in (args.get("substeps_latex") or [])]
                actions.append({
                    "action_id": f"act_exp_{beat_counter}",
                    "type": "expand_intermediate_steps",
                    "object_id": f"exp_live_{beat_counter}",
                    "placement": {"zone": "derivation_flow", "relation": "between", "target_id": after_id, "gap_px": 18},
                    "animation": {"style": "spring_reflow", "duration_ms": 700, "cue_id": "c1"},
                    "color_token": "accent_blue",
                    "payload": {
                        "after_object_id": after_id,
                        "substeps": substeps,
                    },
                })
            elif tname == "show_physics_simulation":
                kind = str(args.get("widget_kind") or "riemann")
                if kind == "riemann":
                    actions.append({
                        "action_id": f"act_sim_{beat_counter}",
                        "type": "plot_riemann_sum",
                        "object_id": f"widget_live_{beat_counter}",
                        "placement": {"zone": "visual_lab", "relation": "auto_next", "gap_px": 20},
                        "animation": {"style": "fade_in", "duration_ms": 600, "cue_id": "c1"},
                        "payload": {"title": str(args.get("title") or "Suma de Riemann W = ∫ P(V) dV"), "expr": "2 + 4*x", "a": 1.0, "b": 5.0, "n_init": 6, "n_max": 100, "exact_value": 56.0, "unit": "J", "interactive_slider": True},
                    })
                elif kind.startswith("pv_"):
                    proc = "adiabatic" if "adiabatic" in kind else "isothermal"
                    actions.append({
                        "action_id": f"act_sim_{beat_counter}",
                        "type": "plot_pv_diagram",
                        "object_id": f"widget_live_{beat_counter}",
                        "placement": {"zone": "visual_lab", "relation": "auto_next", "gap_px": 20},
                        "animation": {"style": "fade_in", "duration_ms": 600, "cue_id": "c1"},
                        "payload": {"title": str(args.get("title") or f"Diagrama P–V ({proc})"), "process": proc, "nRT": 2400.0, "Vi": 1.0, "Vf": 3.0, "shade_work": True},
                    })
                else:
                    geom = "cylindrical" if "cyl" in kind else "spherical"
                    actions.append({
                        "action_id": f"act_sim_{beat_counter}",
                        "type": "draw_thermal_shell",
                        "object_id": f"widget_live_{beat_counter}",
                        "placement": {"zone": "visual_lab", "relation": "auto_next", "gap_px": 20},
                        "animation": {"style": "fade_in", "duration_ms": 600, "cue_id": "c1"},
                        "payload": {"title": str(args.get("title") or f"Cascarón térmico ({geom})"), "geometry": geom, "r1": 0.05, "r2": 0.12, "T1": 380, "T2": 300, "highlight_dr": True},
                    })

            for act in actions:
                await dispatch_live_canvas_action(act, prov_list=prov, step_idx=step_idx)
            ctx.telemetry.record(
                session_id,
                "tool_call",
                {
                    "tool_name": tname,
                    "rendered": len(actions),
                    "action_types": [a.get("type") for a in actions],
                    "extra": extra_result,
                },
                runtime_build=RUNTIME_BUILD,
            )
            return {"ok": True, "rendered": len(actions), **extra_result}

        live_asr_turn_buf = ""
        live_out_turn_buf = ""
        cleared_this_live_turn = False
        correction_flagged_this_turn = False
        antipattern_flagged_this_turn = False

        async def execute_full_board_reset(
            *,
            clear_ink: bool = True,
            clear_media: bool = True,
            reason: str = "board_reset",
        ) -> None:
            nonlocal active_problem, lesson_beats, lesson_cursor, cleared_this_live_turn
            cleared_this_live_turn = True
            scene_history.clear()
            provenance_edges.clear()
            active_problem = None
            lesson_beats = []
            lesson_cursor = -1
            ctx.telemetry.record(
                session_id,
                "board_reset",
                {"clear_ink": clear_ink, "clear_media": clear_media, "reason": reason},
                runtime_build=RUNTIME_BUILD,
            )
            await send_json_safe({
                "type": "BOARD_RESET",
                "clear_ink": clear_ink,
                "clear_media": clear_media,
                "reason": reason,
            })

        async def on_live_event_out(evt_obj: Dict[str, Any]) -> None:
            nonlocal live_asr_turn_buf, live_out_turn_buf, cleared_this_live_turn
            nonlocal correction_flagged_this_turn, antipattern_flagged_this_turn
            await send_json_safe(evt_obj)
            et = str(evt_obj.get("type") or "")
            if et == "ASR_FINAL":
                live_asr_turn_buf = (live_asr_turn_buf + " " + str(evt_obj.get("text") or ""))[-280:]
                if not cleared_this_live_turn and detect_live_clear_intent("ASR_FINAL", live_asr_turn_buf):
                    await execute_full_board_reset(clear_ink=True, clear_media=True, reason="asr_clear_guardrail")
                corr = detect_user_correction(live_asr_turn_buf)
                if corr and not correction_flagged_this_turn:
                    correction_flagged_this_turn = True
                    ctx.telemetry.record(
                        session_id,
                        "user_correction_detected",
                        {"matched": corr, "text": live_asr_turn_buf.strip()},
                        severity="incident",
                        runtime_build=RUNTIME_BUILD,
                    )
                    await send_json_safe({
                        "type": "REQUEST_INCIDENT_SNAPSHOT",
                        "reason": "user_correction_detected",
                        "note": f"Corrección detectada ({corr}): {live_asr_turn_buf.strip()[:160]}",
                    })
            elif et == "LIVE_OUTPUT_TRANSCRIPT":
                live_out_turn_buf = (live_out_turn_buf + " " + str(evt_obj.get("text") or ""))[-320:]
                if not cleared_this_live_turn and detect_live_clear_intent("LIVE_OUTPUT_TRANSCRIPT", live_out_turn_buf):
                    await execute_full_board_reset(clear_ink=True, clear_media=True, reason="model_claim_clear_guardrail")
                anti = detect_tutor_antipattern(live_out_turn_buf)
                if anti and not antipattern_flagged_this_turn:
                    antipattern_flagged_this_turn = True
                    ctx.telemetry.record(
                        session_id,
                        "tutor_antipattern",
                        {"matched": anti, "text": live_out_turn_buf.strip()},
                        severity="incident",
                        runtime_build=RUNTIME_BUILD,
                    )
                    await send_json_safe({
                        "type": "REQUEST_INCIDENT_SNAPSHOT",
                        "reason": "tutor_antipattern",
                        "note": f"Antipatrón detectado ({anti})",
                    })
            elif et == "ERROR_RECOVERY":
                ctx.telemetry.record(
                    session_id,
                    "server_error",
                    {"scope": evt_obj.get("scope", ""), "message": evt_obj.get("message", "")},
                    severity="error",
                    runtime_build=RUNTIME_BUILD,
                )
            elif et == "STATE_CHANGE" and evt_obj.get("state") == "WAITING_FOR_STUDENT":
                if live_asr_turn_buf.strip():
                    ctx.telemetry.record(
                        session_id, "asr_user", {"text": live_asr_turn_buf.strip()}, runtime_build=RUNTIME_BUILD
                    )
                if live_out_turn_buf.strip():
                    ctx.telemetry.record(
                        session_id, "tutor_transcript", {"text": live_out_turn_buf.strip()}, runtime_build=RUNTIME_BUILD
                    )
                live_asr_turn_buf = ""
                live_out_turn_buf = ""
                cleared_this_live_turn = False
                correction_flagged_this_turn = False
                antipattern_flagged_this_turn = False

        live_bridge = GeminiLiveVoiceBridge(
            api_key=ctx.audio_api_key,
            model_name=ctx.live_audio_model,
            on_pcm_out=send_pcm24_safe,
            on_event_out=on_live_event_out,
            on_tool_beat=handle_live_tool_call,
        )

        def cancel_active_generation() -> None:
            nonlocal active_task
            if active_task and not active_task.done():
                active_task.cancel()
            active_task = None

        await send_json_safe({
            "type": "SESSION_READY",
            "session_id": session_id,
            "resumed": resumed,
            "scene_count": len(scene_history),
            "lesson_cursor": lesson_cursor,
            "active_problem_id": (active_problem or {}).get("problem_id", ""),
            "lessons": get_seed_lessons_catalog(),
            "visual_cloud_ready": ctx.visual_director.configured,
            "visual_model": ctx.visual_director.primary_model,
            "live_audio_ready": bool(ctx.audio_api_key),
            "live_audio_model": ctx.live_audio_model,
            "runtime_build": RUNTIME_BUILD,
            "code_stale": is_runtime_code_stale(),
            "pdfs": ctx.pdf_store.list_docs(),
        })

        try:
            while True:
                msg = await ws.receive()
                if msg["type"] == "websocket.disconnect":
                    break

                if msg.get("bytes"):
                    raw_b = msg["bytes"]
                    if len(raw_b) >= 8 and raw_b[:2] == BINARY_MAGIC:
                        channel = raw_b[2]
                        payload_pcm = raw_b[8:]
                        if channel == CH_MIC_PCM16 and live_bridge.is_running:
                            try:
                                await live_bridge.send_mic_pcm16(payload_pcm)
                                mic_frames_received += 1
                                if mic_frames_received == 1 or mic_frames_received % 150 == 0:
                                    await send_json_safe({
                                        "type": "MIC_STREAM_CONFIRMED",
                                        "frames": mic_frames_received,
                                    })
                            except Exception as exc:
                                await send_json_safe({
                                    "type": "ERROR_RECOVERY",
                                    "scope": "mic_stream",
                                    "message": sanitize_text(str(exc))[:160],
                                })
                    continue

                text_data = msg.get("text")
                if not text_data:
                    continue
                try:
                    evt = json.loads(text_data)
                except Exception:
                    continue

                etype = str(evt.get("type") or "")

                if etype == "UPDATE_SETTINGS":
                    if "speech_enabled" in evt:
                        speech_enabled = bool(evt["speech_enabled"])
                    if "speech_rate" in evt:
                        speech_rate = float(max(0.75, min(1.35, float(evt["speech_rate"]))))
                    if "tts_voice" in evt and isinstance(evt["tts_voice"], str):
                        tts_voice = evt["tts_voice"]
                    if "visual_model" in evt and isinstance(evt["visual_model"], str):
                        ctx.visual_director.primary_model = evt["visual_model"]
                    await send_json_safe({"type": "SETTINGS_UPDATED", "speech_enabled": speech_enabled, "speech_rate": speech_rate, "visual_model": ctx.visual_director.primary_model})

                elif etype == "CLIENT_INTERRUPT":
                    cancel_active_generation()
                    ctx.memory.log_event(session_id, "barge_in", str(evt.get("beat_id") or ""), payload_json=json.dumps(evt))
                    await send_json_safe({"type": "STATE_CHANGE", "state": "INTERRUPTED", "preserved_objects": len(scene_history)})

                elif etype == "CLEAR_BOARD":
                    cancel_active_generation()
                    scene_history.clear()
                    provenance_edges.clear()
                    active_problem = None
                    lesson_beats = []
                    lesson_cursor = -1
                    await send_json_safe({
                        "type": "BOARD_RESET",
                        "clear_ink": bool(evt.get("clear_ink", True)),
                        "clear_media": bool(evt.get("clear_media", False)),
                    })

                elif etype == "OPEN_PDF_PATH":
                    cancel_active_generation()
                    path_str = str(evt.get("path") or "").strip().strip('"').strip("'")
                    p = pathlib.Path(path_str)
                    if not p.is_file() or p.suffix.lower() != ".pdf":
                        await send_json_safe({"type": "ERROR_RECOVERY", "message": "No se encontró el archivo PDF indicado."})
                    else:
                        await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                        info = await asyncio.to_thread(_index_and_summarize_pdf, p.name, p.read_bytes(), str(p))
                        await send_json_safe({"type": "PDF_INDEXED", **info})
                        if evt.get("show_audit", True):
                            active_task = asyncio.create_task(dispatch_pdf_audit_beat(info["doc_id"]))

                elif etype == "AUDIT_PDF":
                    cancel_active_generation()
                    doc_id = evt.get("doc_id") or ctx.pdf_store.latest_doc_id()
                    if not doc_id:
                        await send_json_safe({"type": "ERROR_RECOVERY", "message": "Carga o arrastra un PDF primero para auditarlo."})
                    else:
                        active_task = asyncio.create_task(dispatch_pdf_audit_beat(str(doc_id)))

                elif etype == "SOLVE_PDF_PROBLEM":
                    cancel_active_generation()
                    doc_id = evt.get("doc_id") or ctx.pdf_store.latest_doc_id()
                    pid = str(evt.get("problem_id") or "").strip()
                    if not doc_id or not pid:
                        await send_json_safe({"type": "ERROR_RECOVERY", "message": "Indica un PDF cargado y un número de problema (ej. Q057)."})
                    else:
                        try:
                            prob_vis = await asyncio.to_thread(ctx.pdf_store.crop_problem_visual, str(doc_id), pid)
                            active_task = asyncio.create_task(
                                start_solving_pdf_problem(prob_vis, custom_question=str(evt.get("question_text") or ""))
                            )
                        except KeyError as exc:
                            await send_json_safe({"type": "ERROR_RECOVERY", "message": str(exc)})

                elif etype == "SOLVE_PASTED_IMAGE":
                    cancel_active_generation()
                    img_b64 = str(evt.get("image_b64") or "")
                    mime_in = str(evt.get("mime_type") or "image/png")
                    caption = str(evt.get("caption") or "Recorte pegado")
                    q_extra = str(evt.get("question_text") or "").strip()
                    auto_solve = bool(evt.get("auto_solve", True))
                    if not img_b64:
                        continue
                    lesson_beats = []
                    lesson_cursor = -1
                    clip_oid = f"pasted_img_{beat_counter + 1}"
                    active_problem = {
                        "problem_id": caption,
                        "doc_id": "",
                        "filename": caption,
                        "page_start": 1,
                        "subtema": "Ejercicio o imagen pegada",
                        "procedimiento": "Desarrollo guiado paso a paso",
                        "source_label": caption,
                        "problem_text": q_extra or "Lee el enunciado, fórmula, texto, mapa o diagrama presente en la imagen pegada.",
                        "problem_png_b64": img_b64,
                        "figure_png_b64": "",
                        "clip_object_id": clip_oid,
                        "step_count": 1,
                    }

                    if live_bridge.is_running:
                        raw_img = b""
                        try:
                            raw_img = base64.b64decode(img_b64)
                        except Exception:
                            raw_img = b""
                        live_prompt = (
                            f"[IMAGEN PEGADA POR EL ESTUDIANTE EN LA PIZARRA ({caption}):\n"
                            + (
                                f"Instrucción del estudiante: {q_extra}\n"
                                if q_extra
                                else (
                                    "Lee exactamente lo que aparece en esta imagen (ecuaciones, texto, símbolos, gráficas o figuras), "
                                    "explícalo de forma clara y concisa y escribe el Paso 1 en la pizarra usando tus herramientas."
                                    if auto_solve
                                    else "Observa esta imagen que el estudiante acaba de pegar en la pizarra y tenla lista para responder sus preguntas."
                                )
                            )
                            + "]"
                        )
                        if raw_img:
                            await live_bridge.send_visual_context_bundle(
                                [(raw_img, mime_in)],
                                live_prompt,
                                turn_complete=auto_solve,
                            )
                        await send_json_safe({
                            "type": "PASTED_IMAGE_VISION_ACK",
                            "caption": caption,
                            "sent_to_live": bool(raw_img),
                            "auto_solve": auto_solve,
                        })
                        continue

                    if not auto_solve:
                        await send_json_safe({
                            "type": "PASTED_IMAGE_VISION_ACK",
                            "caption": caption,
                            "sent_to_live": False,
                            "auto_solve": False,
                        })
                        continue

                    async def _solve_img_step1() -> None:
                        nonlocal beat_counter
                        beat_counter += 1
                        await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                        prompt = q_extra or (
                            "El estudiante pegó una imagen en la pizarra. "
                            "Lee visualmente todo el contenido (fórmulas, texto, diagramas, mapas o gráficas) y desarrolla ÚNICAMENTE el Paso 1 "
                            "(qué muestra la imagen, datos/conceptos clave y punto de partida). "
                            "Al final pregunta si entendió o si se perdió en algún lado antes de seguir al Paso 2."
                        )
                        b, m = await ctx.visual_director.generate_beat(
                            session_id=session_id,
                            beat_num=beat_counter,
                            step_index=1,
                            user_prompt=prompt,
                            intent="start_problem",
                            scene_summary=scene_history,
                            provenance_edges=provenance_edges,
                            rag_citations=[],
                            pedagogy_summary=ctx.memory.build_pedagogical_summary(),
                            crop_png_b64=img_b64,
                            active_problem=active_problem,
                        )
                        await record_and_dispatch_beat(b, meta=m)

                    active_task = asyncio.create_task(_solve_img_step1())

                elif etype == "LOAD_LESSON":
                    cancel_active_generation()
                    lesson_id = str(evt.get("lesson_id") or "fis3_trabajo_cero")
                    lesson_beats = get_lesson_beats(lesson_id, session_id)
                    lesson_cursor = 0
                    active_problem = None
                    scene_history.clear()
                    provenance_edges.clear()
                    await send_json_safe({"type": "BOARD_RESET", "lesson_id": lesson_id, "total_steps": len(lesson_beats)})
                    if lesson_beats:
                        active_task = asyncio.create_task(
                            record_and_dispatch_beat(lesson_beats[0], meta={"model": "sympy_verified_seed", "step": 1, "total": len(lesson_beats)})
                        )

                elif etype == "NEXT_STEP":
                    cancel_active_generation()
                    if active_problem:
                        active_task = asyncio.create_task(advance_active_problem_step("Siguiente paso"))
                    elif lesson_beats and lesson_cursor + 1 < len(lesson_beats):
                        lesson_cursor += 1
                        active_task = asyncio.create_task(
                            record_and_dispatch_beat(
                                lesson_beats[lesson_cursor],
                                meta={"model": "sympy_verified_seed", "step": lesson_cursor + 1, "total": len(lesson_beats)},
                            )
                        )
                    else:
                        async def _gen_next() -> None:
                            nonlocal beat_counter
                            beat_counter += 1
                            await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                            b, m = await ctx.visual_director.generate_beat(
                                session_id=session_id,
                                beat_num=beat_counter,
                                step_index=len(scene_history) + 1,
                                user_prompt="Continúa con el siguiente micro-paso lógico de la lección actual.",
                                intent="next_step",
                                scene_summary=scene_history,
                                provenance_edges=provenance_edges,
                                rag_citations=[],
                                pedagogy_summary=ctx.memory.build_pedagogical_summary(),
                            )
                            await record_and_dispatch_beat(b, meta=m)
                        active_task = asyncio.create_task(_gen_next())

                elif etype == "REPEAT_STEP":
                    cancel_active_generation()
                    if lesson_beats and 0 <= lesson_cursor < len(lesson_beats):
                        active_task = asyncio.create_task(
                            record_and_dispatch_beat(lesson_beats[lesson_cursor], meta={"model": "repeat_step"})
                        )
                    elif active_problem:
                        async def _rep_prob() -> None:
                            nonlocal beat_counter
                            beat_counter += 1
                            await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                            b, m = await ctx.visual_director.generate_beat(
                                session_id=session_id,
                                beat_num=beat_counter,
                                step_index=active_problem["step_count"],
                                user_prompt="Repite el último paso de forma aún más clara y lenta, sin avanzar al siguiente.",
                                intent="repeat_step",
                                scene_summary=scene_history,
                                provenance_edges=provenance_edges,
                                rag_citations=[],
                                pedagogy_summary=ctx.memory.build_pedagogical_summary(),
                                crop_png_b64=active_problem.get("problem_png_b64"),
                                active_problem=active_problem,
                            )
                            await record_and_dispatch_beat(b, meta=m)
                        active_task = asyncio.create_task(_rep_prob())

                elif etype in ("USER_TEXT_QUERY", "USER_CIRCLE_QUERY", "USER_TRACE_ORIGIN", "USER_EXPAND_INTERMEDIATE"):
                    cancel_active_generation()
                    q_text = str(evt.get("question_text") or evt.get("text") or "").strip()
                    intent = str(evt.get("intent") or etype.lower())
                    crop_b64 = evt.get("visual_crop_png_b64")
                    pdf_ref = evt.get("pdf_region_ref")
                    extra_ctx: Dict[str, Any] = {
                        "hit_targets": evt.get("hit_targets") or [],
                        "target_object_id": evt.get("object_id") or last_math_object_id(),
                        "term_class": evt.get("term_class"),
                        "circled_text": str(evt.get("circled_text") or "").strip(),
                    }
                    if extra_ctx["circled_text"] and extra_ctx["circled_text"].lower() not in q_text.lower():
                        q_text = f"{q_text} [El estudiante encerró en la pizarra: '{extra_ctx['circled_text']}' en el bloque '{extra_ctx['target_object_id']}']".strip()

                    # 0. Check if user pasted a local Windows .pdf path directly in chat
                    path_match = _LOCAL_PDF_PATH_RE.search(q_text)
                    if path_match and etype == "USER_TEXT_QUERY":
                        pdf_path = pathlib.Path(path_match.group(1).strip())
                        if pdf_path.is_file():
                            await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                            info = await asyncio.to_thread(_index_and_summarize_pdf, pdf_path.name, pdf_path.read_bytes(), str(pdf_path))
                            await send_json_safe({"type": "PDF_INDEXED", **info})
                            q_text = _strip_local_pdf_paths(q_text)
                            if not q_text:
                                active_task = asyncio.create_task(dispatch_pdf_audit_beat(info["doc_id"]))
                                continue

                    folded_q = fold_accents(q_text)

                    # 0b. Clear whiteboard command in text mode ("borra la pizarra", "limpia todo")
                    if etype == "USER_TEXT_QUERY" and not live_bridge.is_running and re.search(
                        r"\b(borra|borrar|limpia|limpiar)\s+(todo|toda|la\s+pizarra|el\s+pizarron)\b", folded_q
                    ):
                        scene_history.clear()
                        provenance_edges.clear()
                        active_problem = None
                        lesson_beats = []
                        lesson_cursor = -1
                        await send_json_safe({"type": "BOARD_RESET", "clear_ink": True, "clear_media": False})
                        continue

                    # 1. Agentic PDF routing on natural text queries when a PDF is loaded
                    if etype == "USER_TEXT_QUERY" and ctx.pdf_store.latest_doc_id() and not live_bridge.is_running:
                        # 1a. Audit repeated problems?
                        if _AUDIT_REPEATED_RE.search(folded_q):
                            active_task = asyncio.create_task(dispatch_pdf_audit_beat())
                            continue

                        # 1b. Specific problem ID (Q057, Q122, problema 57)?
                        pid_req = _extract_problem_id_from_query(q_text)
                        if pid_req:
                            found_prob = ctx.pdf_store.get_problem_by_id(pid_req)
                            if found_prob:
                                prob_vis = await asyncio.to_thread(ctx.pdf_store.crop_problem_visual, found_prob["doc_id"], found_prob["problem_id"])
                                active_task = asyncio.create_task(start_solving_pdf_problem(prob_vis))
                                continue

                        # 1c. Search problem by topic?
                        if _TOPIC_SEARCH_RE.search(folded_q):
                            need_fig = bool(re.search(r"\b(figura|imagen|diagrama|grafica)\b", folded_q))
                            hits = ctx.pdf_store.search_problems(q_text, limit=6, require_figure=need_fig)
                            if not hits and need_fig:
                                hits = ctx.pdf_store.search_problems(q_text, limit=6, require_figure=False)
                            if hits:
                                prob_vis = await asyncio.to_thread(ctx.pdf_store.crop_problem_visual, hits[0]["doc_id"], hits[0]["problem_id"])
                                active_task = asyncio.create_task(start_solving_pdf_problem(prob_vis, runner_ups=hits[1:]))
                                continue

                    # 2. Affirmative continuation ("sí", "entendí", "dale", "continúa") -> advance active problem or lesson!
                    if etype == "USER_TEXT_QUERY" and _AFFIRMATIVE_CONTINUE_RE.match(folded_q) and not live_bridge.is_running:
                        if active_problem:
                            active_task = asyncio.create_task(advance_active_problem_step(q_text))
                            continue
                        if lesson_beats and lesson_cursor + 1 < len(lesson_beats):
                            lesson_cursor += 1
                            active_task = asyncio.create_task(
                                record_and_dispatch_beat(
                                    lesson_beats[lesson_cursor],
                                    meta={"model": "sympy_verified_seed", "step": lesson_cursor + 1, "total": len(lesson_beats)},
                                )
                            )
                            continue

                    if isinstance(pdf_ref, dict) and pdf_ref.get("doc_id"):
                        try:
                            reg = ctx.pdf_store.extract_region_text_and_crop(
                                str(pdf_ref["doc_id"]),
                                int(pdf_ref.get("page", 1)),
                                pdf_ref.get("rect_norm") or {},
                            )
                            extra_ctx["pdf_region_text"] = reg.get("region_text", "")
                            extra_ctx["pdf_citation"] = {"doc_name": reg["filename"], "page": reg["page"]}
                            if not crop_b64:
                                crop_b64 = reg.get("crop_png_b64")
                        except Exception:
                            pass

                    if not q_text:
                        if etype == "USER_TRACE_ORIGIN":
                            q_text = "¿De dónde salió exactamente este término o signo? Señala su origen con una flecha y abre un espacio intermedio si hace falta."
                        elif etype == "USER_EXPAND_INTERMEDIATE":
                            q_text = "Abre un espacio en blanco aquí mismo y despliega todos los pasos algebraicos intermedios (y una gráfica visual si ayuda) sin omitir nada."
                        else:
                            q_text = "Explícame paso a paso esto que acabo de señalar o encerrar en la pizarra."

                    if etype == "USER_TEXT_QUERY" and _EXPLAIN_SPACE_VISUAL_RE.search(folded_q):
                        intent = "explain_with_space_and_visual"

                    # If Gemini Live voice session is active, feed context + visual crop into live conversation
                    if live_bridge.is_running:
                        prompt_live = f"[Acción del alumno: {intent}] {q_text} (Contexto: {json.dumps(extra_ctx, ensure_ascii=False)[:450]})"
                        if crop_b64:
                            try:
                                await live_bridge.send_visual_context_bundle(
                                    [(base64.b64decode(crop_b64), "image/png")],
                                    prompt_live,
                                    turn_complete=True,
                                )
                            except Exception:
                                await live_bridge.send_text_turn(prompt_live)
                        else:
                            await live_bridge.send_text_turn(prompt_live)
                    else:
                        rag_hits = ctx.pdf_store.search(q_text, limit=3)
                        use_crop = crop_b64 or (active_problem.get("problem_png_b64") if active_problem else None)
                        extra_imgs = (
                            [active_problem["figure_png_b64"]]
                            if active_problem and active_problem.get("figure_png_b64")
                            else []
                        )

                        async def _run_visual_query() -> None:
                            nonlocal beat_counter
                            beat_counter += 1
                            await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                            b, m = await ctx.visual_director.generate_beat(
                                session_id=session_id,
                                beat_num=beat_counter,
                                step_index=len(scene_history) + 1,
                                user_prompt=q_text,
                                intent=intent,
                                scene_summary=scene_history,
                                provenance_edges=provenance_edges,
                                rag_citations=rag_hits,
                                pedagogy_summary=ctx.memory.build_pedagogical_summary(),
                                crop_png_b64=use_crop,
                                extra_images_b64=extra_imgs,
                                active_problem=active_problem,
                                extra_context=extra_ctx,
                            )
                            await record_and_dispatch_beat(b, meta=m)

                        active_task = asyncio.create_task(_run_visual_query())

                elif etype == "VERIFY_STUDENT_WORK":
                    expected = str(evt.get("expected_expr") or "")
                    student = str(evt.get("student_expr") or "")
                    var_name = str(evt.get("variable") or "x")
                    skill_id = str(evt.get("concept_tag") or "calc.int.general")
                    res = verify_student_step(expected, student, var_name=var_name, integrand_text=evt.get("integrand"))
                    mastery = ctx.memory.record_practice_outcome(skill_id, skill_id, bool(res["equivalent"]))
                    misconception = None
                    if not res["equivalent"] and res["error_category"] != "SyntaxError":
                        misconception = ctx.memory.log_misconception(
                            skill_id, res["error_category"], student, expected, res["message_es"]
                        )
                    await send_json_safe({
                        "type": "STUDENT_WORK_VERIFIED",
                        "verification": res,
                        "mastery": mastery,
                        "misconception": misconception,
                    })

                elif etype == "PREGUNTA_LIBRE":
                    cancel_active_generation()
                    q_text = str(evt.get("texto") or "").strip()
                    ctx_lec = evt.get("contexto") if isinstance(evt.get("contexto"), dict) else None
                    sel = evt.get("seleccion") if isinstance(evt.get("seleccion"), dict) else None
                    img_b64 = str(evt.get("imagen") or "")
                    mod_req = str(evt.get("modelo") or "")
                    if mod_req:
                        ctx.visual_director.primary_model = mod_req
                    if not q_text:
                        continue
                    extra_ctx_pl: Dict[str, Any] = {}
                    if ctx_lec:
                        extra_ctx_pl["leccion_actual"] = ctx_lec
                    if sel:
                        extra_ctx_pl["seleccion"] = sel
                    q_full = q_text
                    if sel and sel.get("texto"):
                        q_full = f"{q_text} [Seleccionado en la pizarra: '{sel['texto']}' (fórmula: {sel.get('tex', '')})]"

                    async def _run_pregunta_libre() -> None:
                        nonlocal beat_counter
                        beat_counter += 1
                        await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                        b, m = await ctx.visual_director.generate_beat(
                            session_id=session_id,
                            beat_num=beat_counter,
                            step_index=len(scene_history) + 1,
                            user_prompt=q_full,
                            intent="question",
                            scene_summary=scene_history,
                            provenance_edges=provenance_edges,
                            rag_citations=[],
                            pedagogy_summary=ctx.memory.build_pedagogical_summary(),
                            crop_png_b64=img_b64 or None,
                            active_problem=active_problem,
                            extra_context=extra_ctx_pl,
                        )
                        await record_and_dispatch_beat(b, meta=m, synthesize_tts=False)

                    active_task = asyncio.create_task(_run_pregunta_libre())

                elif etype == "RESOLVER_IMAGEN":
                    cancel_active_generation()
                    img_b64 = str(evt.get("imagen") or "")
                    mime_in = str(evt.get("tipo") or "image/png")
                    nota = str(evt.get("nota") or "").strip()
                    texto_ocr = str(evt.get("texto") or "").strip()
                    nombre = str(evt.get("nombre") or "Imagen pegada").strip()
                    mod_req = str(evt.get("modelo") or "")
                    sin_saltos = bool(evt.get("sin_saltos", True))
                    if not img_b64:
                        continue
                    try:
                        raw_img = base64.b64decode(img_b64, validate=False)
                    except Exception:
                        await send_json_safe({"type": "ERROR_RECOVERY", "message": "Imagen inválida."})
                        continue

                    async def _run_resolver_imagen() -> None:
                        await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                        try:
                            prompt = nota or (
                                "Resuelve completamente el ejercicio o explica el contenido de esta imagen "
                                "paso a paso sin saltarte despejes ni unidades en español claro para alguien que necesita pasos cortos."
                            )
                            lec = await generar_leccion_ia(
                                ctx.visual_director,
                                prompt=prompt,
                                imagen_bytes=raw_img,
                                mime=mime_in,
                                texto_ocr=texto_ocr,
                                modelo=mod_req,
                                titulo_hint=nombre,
                                sin_saltos=sin_saltos,
                            )
                            await send_json_safe({"type": "LECCION_IA", "leccion": lec})
                            await send_json_safe({"type": "STATE_CHANGE", "state": "WAITING_FOR_STUDENT"})
                        except Exception as exc:
                            await send_json_safe({"type": "ERROR_RECOVERY", "message": sanitize_text(str(exc))[:180]})

                    active_task = asyncio.create_task(_run_resolver_imagen())

                elif etype == "RESOLVER_DOCUMENTO":
                    cancel_active_generation()
                    doc_id = str(evt.get("doc") or "").strip()
                    clave = str(evt.get("clave") or "").strip()
                    mod_req = str(evt.get("modelo") or "")
                    sin_saltos = bool(evt.get("sin_saltos", True))
                    if not doc_id or not clave:
                        continue

                    async def _run_resolver_doc() -> None:
                        await send_json_safe({"type": "STATE_CHANGE", "state": "THINKING_VERIFYING"})
                        try:
                            lec_id = await asyncio.to_thread(ctx.documentos.leccion_de, doc_id, clave)
                            if lec_id and registro_banco.existe(lec_id):
                                await send_json_safe({"type": "ABRIR_LECCION", "id": lec_id})
                                return
                            prob = await asyncio.to_thread(ctx.documentos.problema, doc_id, clave)
                            png_bytes = await asyncio.to_thread(ctx.documentos.png, doc_id, clave)
                            sol_txt = await asyncio.to_thread(ctx.documentos.texto_solucion, doc_id, clave)
                            etiq = ctx.documentos.etiqueta(doc_id, clave)
                            lec = await generar_leccion_ia(
                                ctx.visual_director,
                                prompt=f"Resuelve paso a paso SIN SALTARTE PASOS INTERMEDIOS este ejercicio ({etiq}).",
                                imagen_bytes=png_bytes,
                                mime="image/png",
                                texto_ocr=prob.texto,
                                guia_solucion=sol_txt,
                                modelo=mod_req,
                                titulo_hint=etiq,
                                sin_saltos=sin_saltos,
                            )
                            lec["recorte"] = await asyncio.to_thread(ctx.documentos.recorte, doc_id, clave)
                            await send_json_safe({"type": "LECCION_IA", "leccion": lec})
                            await send_json_safe({"type": "STATE_CHANGE", "state": "WAITING_FOR_STUDENT"})
                        except Exception as exc:
                            await send_json_safe({"type": "ERROR_RECOVERY", "message": sanitize_text(str(exc))[:180]})

                    active_task = asyncio.create_task(_run_resolver_doc())

                elif etype == "START_LIVE_VOICE":
                    cancel_active_generation()
                    mic_frames_received = 0
                    lines = []
                    ctx_lec = evt.get("contexto") if isinstance(evt.get("contexto"), dict) else None
                    if ctx_lec:
                        lines.append(
                            f"Lección abierta en la pizarra: {ctx_lec.get('id', '')} — {ctx_lec.get('titulo', '')} "
                            f"(paso {ctx_lec.get('paso', 1)} de {ctx_lec.get('total', 1)}: {ctx_lec.get('titulo_paso', '')})"
                        )
                        if ctx_lec.get("enunciado"):
                            lines.append(f"Enunciado: {str(ctx_lec['enunciado'])[:1500]}")
                        if ctx_lec.get("escrito"):
                            lines.append("Escrito en este paso: " + " | ".join(str(x)[:160] for x in ctx_lec["escrito"][:10]))
                        if ctx_lec.get("respuestas"):
                            lines.append("Respuestas verificadas del banco: " + " ; ".join(str(x)[:120] for x in ctx_lec["respuestas"][:6]))
                    if ctx.pdf_store.list_docs():
                        docs_s = ", ".join(f"{d['filename']} ({d['problem_count']} problemas)" for d in ctx.pdf_store.list_docs())
                        lines.append(f"PDFs cargados disponibles para buscar/recortar/auditar: {docs_s}")
                    if active_problem:
                        lines.append(
                            f"Problema activo en la pizarra ({active_problem['source_label']}, clip_object_id={active_problem.get('clip_object_id', '')}):\n"
                            f"{str(active_problem.get('problem_text') or '')[:5500]}"
                        )
                    for s in scene_history[-12:]:
                        p = s.get("payload") or {}
                        snippet = str(p.get("latex") or p.get("text") or p.get("step_label") or "")[:180]
                        lines.append(f"- {s['object_id']} ({s['type']}): {snippet}")
                    st = await live_bridge.start(board_context_summary="\n".join(lines))
                    await send_json_safe({"type": "LIVE_VOICE_STATUS", **st})
                    # Push active pasted image / PDF problem immediately via deterministic visual bundle
                    if st.get("ok") and active_problem and active_problem.get("problem_png_b64"):
                        try:
                            await live_bridge.send_visual_context_bundle(
                                [(base64.b64decode(active_problem["problem_png_b64"]), "image/png")],
                                f"[IMAGEN O ENUNCIADO ACTIVO EN LA PIZARRA ({active_problem.get('source_label', '')}): "
                                f"{str(active_problem.get('problem_text') or '')[:1200]}]",
                                turn_complete=False,
                            )
                        except Exception:
                            pass

                elif etype == "STOP_LIVE_VOICE":
                    await live_bridge.stop()
                    await send_json_safe({"type": "LIVE_VOICE_STATUS", "ok": True, "active": False})

                elif etype == "SEND_AUDIO_END":
                    if live_bridge.is_running:
                        await live_bridge.send_audio_end()

                elif etype == "SEND_CANVAS_SNAPSHOT":
                    img_b64 = str(evt.get("image_b64") or "")
                    mime = str(evt.get("mime_type") or "image/jpeg")
                    if img_b64 and live_bridge.is_running:
                        try:
                            await live_bridge.send_canvas_jpeg_or_png(base64.b64decode(img_b64), mime_type=mime)
                        except Exception:
                            pass

                elif etype == "USER_LASSO_SELECTION":
                    img_b64 = str(evt.get("image_b64") or "")
                    crop_b64 = str(evt.get("crop_b64") or "")
                    mime = str(evt.get("mime_type") or "image/jpeg")
                    circled_text = str(evt.get("circled_text") or "").strip()
                    obj_id = str(evt.get("object_id") or "").strip()
                    step_title = str(evt.get("step_title") or "").strip()
                    line_context = str(evt.get("line_context") or "").strip()
                    gesture = str(evt.get("gesture") or "lasso").strip()
                    pdf_box = evt.get("pdf_norm_box") if isinstance(evt.get("pdf_norm_box"), dict) else None
                    # If circled over PDF crop, also cross-check exact PyMuPDF words inside pdf_box
                    if pdf_box and active_problem and active_problem.get("crop_words"):
                        bx0 = float(pdf_box.get("x", 0.0))
                        by0 = float(pdf_box.get("y", 0.0))
                        bx1 = bx0 + float(pdf_box.get("w", 0.0))
                        by1 = by0 + float(pdf_box.get("h", 0.0))
                        pdf_hits = []
                        for cw in active_problem["crop_words"]:
                            cx = float(cw["x"]) + float(cw["w"]) * 0.5
                            cy = float(cw["y"]) + float(cw["h"]) * 0.5
                            if bx0 - 0.015 <= cx <= bx1 + 0.015 and by0 - 0.015 <= cy <= by1 + 0.015:
                                pdf_hits.append(str(cw["t"]))
                        if pdf_hits:
                            circled_text = " ".join(pdf_hits).strip()

                    if live_bridge.is_running:
                        img_list = []
                        if img_b64:
                            try:
                                img_list.append((base64.b64decode(img_b64), mime))
                            except Exception:
                                pass
                        if crop_b64:
                            try:
                                img_list.append((base64.b64decode(crop_b64), "image/jpeg"))
                            except Exception:
                                pass
                        ctx_lines = [
                            f"[ATENCIÓN VISUAL · LO QUE EL ESTUDIANTE ACABA DE SEÑALAR/ENCERRAR ({gesture}):",
                            f" - Símbolo o texto exacto señalado/encerrado: \"{circled_text or 'marca en pantalla'}\"",
                        ]
                        if step_title or obj_id:
                            ctx_lines.append(f" - Paso / bloque actual: \"{step_title}\" (object_id={obj_id})")
                        if line_context:
                            ctx_lines.append(f" - Ecuación / contexto completo del bloque: \"{line_context[:260]}\"")
                        ctx_lines.append(
                            "Responde EXCLUSIVAMENTE sobre este símbolo/dato exacto y NO lo confundas con otros diagramas o pasos anteriores arriba en la pizarra. "
                            "Si pregunta de dónde salió, invoca trace_data_provenance.]"
                        )
                        await live_bridge.send_visual_context_bundle(img_list, "\n".join(ctx_lines))
                    ctx.telemetry.record(
                        session_id,
                        "student_gesture",
                        {
                            "gesture": gesture,
                            "circled_text": circled_text,
                            "step_title": step_title,
                            "object_id": obj_id,
                            "sent_to_live": bool(live_bridge.is_running),
                        },
                        runtime_build=RUNTIME_BUILD,
                    )
                    await send_json_safe({
                        "type": "LASSO_VISION_ACK",
                        "circled_text": circled_text,
                        "step_title": step_title,
                        "object_id": obj_id,
                        "gesture": gesture,
                        "sent_to_live": bool(live_bridge.is_running),
                    })

                elif etype == "CLIENT_TELEMETRY_EVENT":
                    kind = str(evt.get("kind") or "client_event")[:48]
                    sev = str(evt.get("severity") or "info")[:16]
                    data_obj = evt.get("data") if isinstance(evt.get("data"), dict) else {}
                    ctx.telemetry.record(session_id, kind, data_obj, severity=sev, runtime_build=RUNTIME_BUILD)

                elif etype == "USER_REPORT_INCIDENT":
                    reason = str(evt.get("reason") or "manual_bug_mark")[:48]
                    note = str(evt.get("note") or "")[:300]
                    img_b64 = str(evt.get("image_b64") or "")
                    mime = str(evt.get("mime_type") or "image/jpeg")
                    dom_audit = evt.get("dom_audit") if isinstance(evt.get("dom_audit"), dict) else {}
                    saved = ctx.telemetry.save_incident_snapshot(
                        session_id,
                        reason,
                        img_b64,
                        mime_type=mime,
                        note=note,
                        dom_audit=dom_audit,
                        extra={"scene_count": len(scene_history), "live_active": bool(live_bridge.is_running)},
                        runtime_build=RUNTIME_BUILD,
                    )
                    await send_json_safe({"type": "INCIDENT_SAVED", **saved})

        except WebSocketDisconnect:
            pass
        finally:
            _persist_session_state()
            cancel_active_generation()
            await live_bridge.stop()

    if ctx.assets_web_dir.is_dir():
        @app.get("/")
        async def root_index() -> FileResponse:
            return FileResponse(ctx.assets_web_dir / "index.html")

        app.mount("/", StaticFiles(directory=str(ctx.assets_web_dir), html=True), name="tutor_web")

    return app


def _bind_ephemeral_port(host: str, requested_port: int) -> int:
    if requested_port > 0:
        return requested_port
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, 0))
        return int(s.getsockname()[1])


def main() -> int:
    ap = argparse.ArgumentParser(description="CuadernU Audiovisual Tutor Runtime")
    ap.add_argument("--bootstrap-stdin", action="store_true", help="Read JSON bootstrap config from stdin pipe")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=0)
    args = ap.parse_args()

    bootstrap: Dict[str, Any] = {}
    if args.bootstrap_stdin:
        raw_line = sys.stdin.readline()
        if raw_line.strip():
            bootstrap = json.loads(raw_line)

    host = str(bootstrap.get("bind_host") or args.host or "127.0.0.1")
    port = _bind_ephemeral_port(host, int(args.port))
    app = create_app(bootstrap)

    config = uvicorn.Config(app, host=host, port=port, log_level="warning", access_log=False)
    server = uvicorn.Server(config)

    async def _serve_and_announce() -> None:
        async def _announce_when_ready() -> None:
            for _ in range(200):
                if server.started:
                    print(json.dumps({"ready": True, "host": host, "port": port}), flush=True)
                    return
                await asyncio.sleep(0.025)
        asyncio.create_task(_announce_when_ready())
        await server.serve()

    asyncio.run(_serve_and_announce())
    return 0


if __name__ == "__main__":
    sys.exit(main())
