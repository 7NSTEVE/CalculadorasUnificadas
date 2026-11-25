#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_analyze.py — Resume la traza JSONL del manager oficial (F1.4/F2.4).

Uso:
    python f14_analyze.py <traza.jsonl> [--seq] [--hooks]
Imprime: distribución de eventos, secuencia cronológica filtrada de las
llamadas GCAI/DLSIM/callbacks con payloads resumidos.
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import json
import sys
from collections import Counter


def payload_summary(rec):
    fields = rec.get("message", {}).get("payload", rec)
    out = []
    for key in ("args", "d0", "d1", "d2", "d3", "d4", "result", "code",
                "payload_dwords", "obj_dwords", "caller", "name", "text"):
        if key in fields:
            value = fields[key]
            if isinstance(value, list) and len(value) > 8:
                value = value[:8] + ["…"]
            out.append(f"{key}={value}")
    return " ".join(out)


def main() -> int:
    path = sys.argv[1]
    mode_seq = "--seq" in sys.argv
    events = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    counter = Counter()
    for rec in events:
        msg = rec.get("message", {})
        if msg.get("type") == "send":
            counter[msg.get("payload", {}).get("event", "?")] += 1
        else:
            counter["_" + str(msg.get("type"))] += 1
    print("=== distribución de eventos ===")
    for name, count in counter.most_common():
        print(f"{count:6d}  {name}")

    if mode_seq:
        print("\n=== secuencia (GCAI/DLSIM/callbacks/IO) ===")
        for rec in events:
            msg = rec.get("message", {})
            if msg.get("type") != "send":
                continue
            payload = msg.get("payload", {})
            event = payload.get("event", "")
            if event in ("hooked", "trace_loaded", "hook_error"):
                continue
            print(f"{rec['seq']:5d} {event:34s} {payload_summary(payload)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
