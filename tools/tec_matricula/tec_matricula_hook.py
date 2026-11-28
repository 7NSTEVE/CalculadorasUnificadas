"""Antigravity Lifecycle Hook for TEC Matrícula MCP Safety & Audit Trail.

Implements the official Antigravity hooks.json PreToolUse / PostToolUse JSON stdin/stdout contract:
  - PreToolUse: Enforces confirmation phrase on live arming and blocks accidental raw credential dumps.
  - PostToolUse: Appends a sanitized (redacted) JSONL audit trail in %LOCALAPPDATA%/CuadernU/matricula_logs/.
"""

from __future__ import annotations

import json
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict

CONFIRMATION_PHRASE = "CONFIRMAR MATRICULA REAL"
REDACTED_KEYS = {"password", "pwd", "secret", "cookie", "cookies", "viewstate", "eventvalidation"}


def _sanitize(obj: Any) -> Any:
    if isinstance(obj, dict):
        return {
            k: ("<REDACTED>" if k.lower() in REDACTED_KEYS else _sanitize(v))
            for k, v in obj.items()
        }
    if isinstance(obj, list):
        return [_sanitize(x) for x in obj]
    return obj


def _audit_path() -> Path:
    custom = os.environ.get("CUADERNU_AUDIT_LOG_PATH")
    if custom:
        return Path(custom)
    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
    return Path(base) / "CuadernU" / "matricula_logs" / "agent_audit.jsonl"


def _write_audit(entry: Dict[str, Any]) -> None:
    try:
        p = _audit_path()
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps(entry, ensure_ascii=False) + "\n")
    except Exception:
        pass


def handle_payload(payload: Dict[str, Any]) -> Dict[str, Any]:
    event = str(payload.get("hookEventName") or "")
    tool = payload.get("toolCall") or {}
    name = str(tool.get("name") or "")
    args = tool.get("args") or {}
    if isinstance(args, dict) and "Arguments" in args and isinstance(args["Arguments"], dict):
        # Unwrap call_mcp_tool wrapper if invoked lazily
        name = str(args.get("ToolName") or name)
        args = args["Arguments"]

    if event == "PreToolUse":
        if "tec_matricula_arm_live" in name and args.get("armed") is True:
            phrase = str(args.get("confirmation_phrase") or "").strip()
            if phrase != CONFIRMATION_PHRASE:
                return {
                    "decision": "deny",
                    "reason": f"Bloqueado por tec-matricula-guard: se requiere confirmation_phrase='{CONFIRMATION_PHRASE}'.",
                }
            return {
                "decision": "ask",
                "reason": "Confirmar apertura del candado para matrícula real en el TEC.",
            }

        if any(k in name for k in ("tec_matricula_enroll_course", "tec_matricula_execute_scenarios", "tec_matricula_send_informe")):
            if args.get("dry_run") is False:
                return {
                    "decision": "ask",
                    "reason": f"Acción real en el portal del TEC ({name} con dry_run=false). Confirmar antes de enviar.",
                }

        if "tec_matricula_watch_and_snipe_start" in name and args.get("auto_enroll") is True:
            return {
                "decision": "ask",
                "reason": "El Seat Sniper tiene auto_enroll=true (matriculará automáticamente al detectarse cupo).",
            }

        return {"decision": "allow"}

    if event == "PostToolUse":
        _write_audit({
            "ts_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "hookEvent": "PostToolUse",
            "tool": name,
            "args": _sanitize(args),
            "error": str(payload.get("error") or ""),
        })
        return {}

    return {}


def main() -> None:
    raw = sys.stdin.read().strip()
    if not raw:
        sys.stdout.write("{}\n")
        return
    try:
        payload = json.loads(raw)
        out = handle_payload(payload)
    except Exception as exc:
        out = {"decision": "allow", "reason": f"Hook fallback: {exc}"}
    sys.stdout.write(json.dumps(out, ensure_ascii=False) + "\n")
    sys.stdout.flush()


if __name__ == "__main__":
    main()
