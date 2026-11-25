#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_boot_dedup.py — Como f14_boot_sequence pero colapsando ráfagas de eventos
idénticos consecutivos (los DISPATCH repetidos) en una línea con contador.

Uso: python f14_boot_dedup.py <traza.jsonl>
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import json
import sys


def summarize(rec):
    msg = rec.get("message", {})
    if msg.get("type") != "send":
        return None, None
    p = msg.get("payload", {})
    ev = p.get("event", "")
    if ev in ("hooked", "trace_loaded", "hook_error"):
        return None, None
    if ev == "EXE.dispatch_0x40d4d0":
        return (ev, (p.get("a0"), p.get("a1"), p.get("obj"))), \
            f"DISPATCH a0={p.get('a0')} a1={p.get('a1')} obj={p.get('obj')}"
    if ev.startswith("GCAI._GCAI_"):
        name = ev[len("GCAI._GCAI_"):]
        args = p.get("args", [])
        d1 = p.get("d1") or []
        d2 = p.get("d2") or []
        d3 = p.get("d3") or []
        def hx(v):
            if isinstance(v, int):
                return f"{v:08X}" if v > 9 else str(v)
            return str(v)
        line = (f"{name} args={args} "
                f"d1[{','.join(hx(v) for v in d1[:6])}] "
                f"d2[{','.join(hx(v) for v in d2[:4])}] "
                f"d3[{','.join(hx(v) for v in d3[:4])}] result={p.get('result')}")
        if p.get("text"):
            line += f" text={p.get('text')!r}"
        return (ev, tuple(map(str, args))), line
    if ev == "GCAI.callback":
        pd = p.get("payload_dwords") or []
        line = f"CALLBACK code={p.get('code')} payload[{','.join(str(v) for v in pd[:8])}]"
        return (ev, (str(p.get("code")), str(pd[:4]))), line
    if ev.startswith("IO."):
        return (ev, p.get("name")), f"IO {p.get('name')}"
    if ev.startswith("DLSIM."):
        name = ev[len("DLSIM."):]
        return (ev, tuple(map(str, p.get("args", [])[:3]))), \
            f"DLSIM {name} args={p.get('args')} result={p.get('result')}"
    return (ev, json.dumps(p, sort_keys=True)), json.dumps(p, ensure_ascii=False)[:160]


def main():
    path = sys.argv[1]
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    t0 = None
    n = 0
    last_key = None
    burst = 0
    last_line = None
    last_t = None

    def flush():
        nonlocal burst
        if last_line is not None:
            suffix = f"  ×{burst}" if burst > 1 else ""
            print(f"{last_line}{suffix}")
        burst = 0

    for line in open(path, encoding="utf-8"):
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        key, text = summarize(rec)
        if key is None:
            continue
        n += 1
        if t0 is None:
            t0 = rec["wall"]
        t = rec["wall"] - t0
        if key == last_key and t - last_t < 2.0:
            burst += 1
            last_t = t
            continue
        flush()
        last_key = key
        last_line = f"{n:5d} t={t:7.2f} {text}"
        last_t = t
        burst = 1
    flush()
    print(f"\ntotal eventos utiles: {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
