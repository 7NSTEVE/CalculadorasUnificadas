#!/usr/bin/env python3
"""Analyzer CLI for CuadernU Local Flight Recorder Telemetry (`private_local/telemetry/`).

Reads recent session JSONL timelines, computes voice-to-whiteboard sync latency
quantiles (p50/p95), aggregates user corrections, tutor refusal antipatterns,
JS errors, DOM overlaps, raw LaTeX leaks, and outputs exact incident snapshot paths
so bugs can be diagnosed and fixed with hard evidence.

Usage:
  python tools/tutor_runtime/analyze_telemetry.py --latest 5
  python tools/tutor_runtime/analyze_telemetry.py --dir private_local/telemetry --json-out report.json
"""

from __future__ import annotations

import argparse
import json
import pathlib
import statistics
import sys
from typing import Any, Dict, List


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent


def _percentile(values: List[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    idx = min(len(ordered) - 1, max(0, int(round((pct / 100.0) * (len(ordered) - 1)))))
    return round(float(ordered[idx]), 1)


def analyze_directory(telemetry_dir: pathlib.Path, latest_n: int = 10) -> Dict[str, Any]:
    if not telemetry_dir.exists():
        return {"ok": False, "error": f"Directorio no encontrado: {telemetry_dir}", "sessions_analyzed": 0}

    session_files = sorted(
        telemetry_dir.glob("session_*.jsonl"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )[: max(1, latest_n)]

    total_events = 0
    counts_by_kind: Dict[str, int] = {}
    onset_to_ink_ms: List[float] = []
    write_durations_ms: List[float] = []
    tool_latencies_ms: List[float] = []
    incidents: List[Dict[str, Any]] = []
    user_corrections: List[Dict[str, Any]] = []
    tutor_antipatterns: List[Dict[str, Any]] = []
    js_errors: List[Dict[str, Any]] = []
    dom_overlaps: List[Dict[str, Any]] = []
    raw_latex_leaks: List[Dict[str, Any]] = []

    for sf in session_files:
        try:
            lines = sf.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for ln in lines:
            ln = ln.strip()
            if not ln:
                continue
            try:
                ev = json.loads(ln)
            except json.JSONDecodeError:
                continue
            total_events += 1
            kind = str(ev.get("kind") or "unknown")
            counts_by_kind[kind] = counts_by_kind.get(kind, 0) + 1
            data = ev.get("data") or {}

            if kind == "sync_metrics":
                if isinstance(data.get("onset_to_write_ms"), (int, float)):
                    onset_to_ink_ms.append(float(data["onset_to_write_ms"]))
                if isinstance(data.get("write_duration_ms"), (int, float)):
                    write_durations_ms.append(float(data["write_duration_ms"]))
            elif kind == "tool_call":
                if isinstance(data.get("latency_ms"), (int, float)):
                    tool_latencies_ms.append(float(data["latency_ms"]))
            elif kind == "user_correction_detected":
                user_corrections.append({
                    "session": sf.name,
                    "iso": ev.get("iso"),
                    "matched": data.get("matched"),
                    "text": data.get("text"),
                })
            elif kind == "tutor_antipattern":
                tutor_antipatterns.append({
                    "session": sf.name,
                    "iso": ev.get("iso"),
                    "matched": data.get("matched"),
                    "text": data.get("text"),
                })
            elif kind == "js_error":
                js_errors.append({
                    "session": sf.name,
                    "iso": ev.get("iso"),
                    "message": data.get("message"),
                    "source": data.get("source"),
                })
            elif kind == "dom_audit_anomaly":
                if data.get("overlaps"):
                    dom_overlaps.append({"session": sf.name, "iso": ev.get("iso"), "overlaps": data.get("overlaps")})
                if data.get("raw_latex_leaks"):
                    raw_latex_leaks.append({"session": sf.name, "iso": ev.get("iso"), "leaks": data.get("raw_latex_leaks")})
            elif kind == "incident_snapshot":
                incidents.append({
                    "session": sf.name,
                    "iso": ev.get("iso"),
                    "reason": data.get("reason"),
                    "note": data.get("note"),
                    "snapshot_abs": data.get("snapshot_abs"),
                    "dom_audit": data.get("dom_audit"),
                })

    return {
        "ok": True,
        "telemetry_dir": str(telemetry_dir),
        "sessions_analyzed": len(session_files),
        "session_files": [p.name for p in session_files],
        "total_events": total_events,
        "counts_by_kind": counts_by_kind,
        "sync_latency": {
            "samples": len(onset_to_ink_ms),
            "onset_to_write_p50_ms": _percentile(onset_to_ink_ms, 50),
            "onset_to_write_p95_ms": _percentile(onset_to_ink_ms, 95),
            "write_duration_avg_ms": round(statistics.mean(write_durations_ms), 1) if write_durations_ms else 0.0,
            "tool_latency_p50_ms": _percentile(tool_latencies_ms, 50),
            "tool_latency_p95_ms": _percentile(tool_latencies_ms, 95),
        },
        "user_corrections": user_corrections,
        "tutor_antipatterns": tutor_antipatterns,
        "js_errors": js_errors,
        "dom_overlaps": dom_overlaps,
        "raw_latex_leaks": raw_latex_leaks,
        "incidents": incidents,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="Analyze CuadernU local telemetry sessions")
    ap.add_argument("--dir", default=str(REPO_ROOT / "private_local" / "telemetry"))
    ap.add_argument("--latest", type=int, default=10)
    ap.add_argument("--json-out", default="")
    args = ap.parse_args()

    report = analyze_directory(pathlib.Path(args.dir), latest_n=args.latest)
    text = json.dumps(report, ensure_ascii=False, indent=2)
    if args.json_out:
        out_p = pathlib.Path(args.json_out)
        out_p.parent.mkdir(parents=True, exist_ok=True)
        out_p.write_text(text, encoding="utf-8")
    print(text)
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
