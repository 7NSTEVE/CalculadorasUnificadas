"""Local Privacy-First Flight Recorder / Telemetry for CuadernU Audiovisual Tutor.

Stores 100% local, Git-ignored diagnostic JSONL event timelines and incident
JPEG snapshots inside `<repo>/private_local/telemetry/` (never transmitted externally,
every string passed through `sanitize_text()` so API keys/tokens can never leak).

Designed to automatically capture:
  - User voice transcripts (`ASR_FINAL`), text queries, and tutor responses
  - Tool invocations, latency, and voice-to-whiteboard sync drift (ms)
  - Student gestures (`lasso`, `pen_mark`, `pointer_tap`) and `Ctrl+V` pasted images
  - Client JS console errors, unhandled rejections, DOM layout collisions, and raw LaTeX leaks
  - Automatic detection of user corrections/frustration (e.g., "no ves lo que encerré",
    "muy rápido", "no la limpiaste", "eso está mal")
  - Manual one-click incident marks (`Ctrl+Shift+B` / "Marcar fallo aquí")
"""

from __future__ import annotations

import base64
import json
import pathlib
import re
import threading
import time
import uuid
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from tools.tutor_runtime.pdf_rag import fold_accents
from tools.tutor_runtime.secrets_dpapi import sanitize_text


_USER_CORRECTION_RE = re.compile(
    r"\b("
    r"no\s+(?:ves|ve|logras?\s+ver|esta[s]?\s+viendo|puedes?\s+ver)\s+(?:lo\s+que|mi|esa|la)"
    r"|no\s+(?:la\s+|lo\s+)?(?:limpiaste|borraste|quitaste|hiciste)"
    r"|(?:escribe|escribes|habla|hablas|va[s]?)\s+(?:muy|demasiado|mas)\s+(?:rapido|lento|desfasad[oa])"
    r"|no\s+(?:se\s+)?sincroniza"
    r"|eso\s+esta\s+(?:mal|incorrecto|equivocado|cortado|montado)"
    r"|se\s+(?:monto|solapo|corto|trabo|congelo|pego)"
    r"|no\s+(?:entendiste|me\s+entendiste|sirve|funciona)"
    r"|por\s+que\s+(?:no\s+)?(?:puedes|dibujas|escribes|borras|ves)"
    r"|fallo|bug|error\s+visual"
    r")\b",
    re.IGNORECASE,
)

_FORBIDDEN_REFUSAL_RE = re.compile(
    r"\b("
    r"limitaciones\s+tecnicas"
    r"|me\s+enfoco\s+en\s+esos\s+temas"
    r"|solo\s+(?:soy\s+)?tutor\s+de\s+matematicas"
    r"|no\s+logro\s+visualizar\s+ninguna\s+imagen"
    r"|no\s+puedo\s+plasmar\s+mapas"
    r")\b",
    re.IGNORECASE,
)

_RAW_LATEX_LEAK_RE = re.compile(
    r"\\(?:frac|sqrt|int|gamma|Delta|theta|pi|alpha|beta|text|mathrm|mathbf|left|right|cdot|times)\b"
)


def detect_user_correction(text: str) -> Optional[str]:
    """Return matched frustration/correction phrase if user speech/text indicates a tutor/UI failure."""
    folded = fold_accents(text or "")
    m = _USER_CORRECTION_RE.search(folded)
    return m.group(0) if m else None


def detect_tutor_antipattern(text: str) -> Optional[str]:
    """Return matched forbidden refusal/blindness phrase from tutor output."""
    folded = fold_accents(text or "")
    m = _FORBIDDEN_REFUSAL_RE.search(folded)
    return m.group(0) if m else None


def detect_raw_latex_leak(visible_text: str) -> Optional[str]:
    """Return matched raw LaTeX command if unrendered markup leaked into student-visible text."""
    m = _RAW_LATEX_LEAK_RE.search(visible_text or "")
    return m.group(0) if m else None


def _sanitize_obj(obj: Any, depth: int = 0) -> Any:
    if depth > 6:
        return "<truncated>"
    if isinstance(obj, str):
        s = sanitize_text(obj)
        return s[:2400] if len(s) > 2400 else s
    if isinstance(obj, (int, float, bool)) or obj is None:
        return obj
    if isinstance(obj, list):
        return [_sanitize_obj(x, depth + 1) for x in obj[:60]]
    if isinstance(obj, dict):
        out: Dict[str, Any] = {}
        for k, v in list(obj.items())[:60]:
            ks = str(k)
            if ks.endswith("_b64") or ks in ("api_key", "token", "secret", "password", "authorization"):
                out[ks] = f"<redacted_len:{len(str(v))}>"
            else:
                out[ks] = _sanitize_obj(v, depth + 1)
        return out
    return sanitize_text(str(obj))[:400]


class LocalTelemetryRecorder:
    """Append-only local JSONL flight recorder + rotating incident JPEG snapshot store."""

    def __init__(
        self,
        base_dir: Optional[pathlib.Path] = None,
        enabled: bool = True,
        max_sessions: int = 30,
        max_total_mb: int = 150,
    ) -> None:
        self.enabled = bool(enabled and base_dir is not None)
        self.base_dir = pathlib.Path(base_dir) if base_dir is not None else None
        self.snapshots_dir = (self.base_dir / "snapshots") if self.base_dir else None
        self.max_sessions = max(5, int(max_sessions))
        self.max_total_bytes = max(10, int(max_total_mb)) * 1024 * 1024
        self._lock = threading.Lock()
        self._session_files: Dict[str, pathlib.Path] = {}
        self._incident_count = 0
        self._event_count = 0

        if self.enabled and self.base_dir and self.snapshots_dir:
            self.base_dir.mkdir(parents=True, exist_ok=True)
            self.snapshots_dir.mkdir(parents=True, exist_ok=True)
            self._enforce_retention()

    def _session_path(self, session_id: str) -> Optional[pathlib.Path]:
        if not self.enabled or not self.base_dir:
            return None
        safe_sid = re.sub(r"[^A-Za-z0-9_-]", "_", session_id or "default")[:48]
        p = self._session_files.get(safe_sid)
        if p is None:
            stamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
            p = self.base_dir / f"session_{stamp}_{safe_sid}.jsonl"
            self._session_files[safe_sid] = p
        return p

    def record(
        self,
        session_id: str,
        kind: str,
        data: Optional[Dict[str, Any]] = None,
        *,
        severity: str = "info",
        runtime_build: str = "",
    ) -> Dict[str, Any]:
        evt = {
            "event_id": f"ev_{uuid.uuid4().hex[:10]}",
            "ts_ms": int(time.time() * 1000),
            "iso": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
            "session_id": str(session_id or "unknown"),
            "runtime_build": str(runtime_build or ""),
            "kind": str(kind or "event"),
            "severity": str(severity or "info"),
            "data": _sanitize_obj(data or {}),
        }
        self._event_count += 1
        if severity in ("warn", "error", "incident"):
            self._incident_count += 1

        if not self.enabled:
            return evt

        path = self._session_path(session_id)
        if path is None:
            return evt
        line = json.dumps(evt, ensure_ascii=False) + "\n"
        with self._lock:
            try:
                with path.open("a", encoding="utf-8") as fh:
                    fh.write(line)
            except OSError:
                pass
        return evt

    def save_incident_snapshot(
        self,
        session_id: str,
        reason: str,
        image_b64: str = "",
        *,
        mime_type: str = "image/jpeg",
        note: str = "",
        dom_audit: Optional[Dict[str, Any]] = None,
        extra: Optional[Dict[str, Any]] = None,
        runtime_build: str = "",
    ) -> Dict[str, Any]:
        snap_rel = ""
        snap_abs = ""
        snap_bytes = 0
        if self.enabled and self.snapshots_dir and image_b64:
            safe_sid = re.sub(r"[^A-Za-z0-9_-]", "_", session_id or "default")[:48]
            safe_reason = re.sub(r"[^A-Za-z0-9_-]", "_", reason or "incident")[:36]
            ext = "png" if "png" in (mime_type or "").lower() else "jpg"
            sdir = self.snapshots_dir / safe_sid
            sdir.mkdir(parents=True, exist_ok=True)
            fname = f"{int(time.time() * 1000)}_{safe_reason}_{uuid.uuid4().hex[:6]}.{ext}"
            fpath = sdir / fname
            try:
                raw = base64.b64decode(image_b64)
                if raw and len(raw) <= 6 * 1024 * 1024:
                    fpath.write_bytes(raw)
                    snap_bytes = len(raw)
                    snap_abs = str(fpath)
                    snap_rel = f"snapshots/{safe_sid}/{fname}"
            except Exception:
                pass

        evt = self.record(
            session_id,
            "incident_snapshot",
            {
                "reason": reason,
                "note": note,
                "snapshot_rel": snap_rel,
                "snapshot_abs": snap_abs,
                "snapshot_bytes": snap_bytes,
                "dom_audit": dom_audit or {},
                "extra": extra or {},
            },
            severity="incident",
            runtime_build=runtime_build,
        )
        if self.enabled and snap_bytes > 0:
            self._enforce_retention()
        return {
            "ok": True,
            "event_id": evt["event_id"],
            "snapshot_rel": snap_rel,
            "snapshot_abs": snap_abs,
            "snapshot_bytes": snap_bytes,
        }

    def _enforce_retention(self) -> None:
        if not self.base_dir or not self.base_dir.exists():
            return
        with self._lock:
            try:
                sessions = sorted(
                    self.base_dir.glob("session_*.jsonl"),
                    key=lambda p: p.stat().st_mtime,
                    reverse=True,
                )
                for old in sessions[self.max_sessions :]:
                    try:
                        old.unlink()
                    except OSError:
                        pass

                # Enforce total byte cap across snapshots (oldest first)
                if self.snapshots_dir and self.snapshots_dir.exists():
                    snaps = sorted(
                        (p for p in self.snapshots_dir.rglob("*") if p.is_file()),
                        key=lambda p: p.stat().st_mtime,
                    )
                    total = sum(p.stat().st_size for p in snaps)
                    for sp in snaps:
                        if total <= self.max_total_bytes:
                            break
                        try:
                            sz = sp.stat().st_size
                            sp.unlink()
                            total -= sz
                        except OSError:
                            pass
            except OSError:
                pass

    def get_summary(self) -> Dict[str, Any]:
        files: List[str] = []
        snap_count = 0
        if self.enabled and self.base_dir and self.base_dir.exists():
            files = [p.name for p in sorted(self.base_dir.glob("session_*.jsonl"), key=lambda p: p.stat().st_mtime, reverse=True)[:10]]
            if self.snapshots_dir and self.snapshots_dir.exists():
                snap_count = sum(1 for p in self.snapshots_dir.rglob("*") if p.is_file())
        return {
            "enabled": self.enabled,
            "telemetry_dir": str(self.base_dir) if self.base_dir else "",
            "events_this_run": self._event_count,
            "incidents_this_run": self._incident_count,
            "recent_sessions": files,
            "stored_snapshots": snap_count,
        }
