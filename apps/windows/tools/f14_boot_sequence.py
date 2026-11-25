#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_boot_sequence.py — Extrae la secuencia cronologica de arranque del manager
oficial (CreateEmulator -> MAIN MENU) desde la traza JSONL de Frida.

Uso: python f14_boot_sequence.py <traza.jsonl>
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import json
import sys


def fmt_dwords(dw):
    if not dw:
        return ''
    parts = []
    for v in dw[:8]:
        if isinstance(v, int):
            parts.append(f"{v:08X}" if v > 9 else str(v))
        else:
            parts.append(str(v))
    return ','.join(parts)


def main():
    path = sys.argv[1]
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    t0 = None
    n = 0
    for line in open(path, encoding="utf-8"):
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        msg = rec.get("message", {})
        if msg.get("type") != "send":
            continue
        p = msg.get("payload", {})
        ev = p.get("event", "")
        if ev in ("hooked", "trace_loaded", "hook_error"):
            continue
        n += 1
        if t0 is None:
            t0 = rec["wall"]
        t = rec["wall"] - t0
        if ev.startswith("GCAI._GCAI_"):
            name = ev[len("GCAI._GCAI_"):]
            args = p.get("args", [])
            dw = {}
            for k in ("d0", "d1", "d2", "d3"):
                if p.get(k):
                    dw[k] = fmt_dwords(p[k])
            extra = " ".join(f"{k}[{v}]" for k, v in dw.items() if v)
            text = p.get("text")
            if text:
                extra += f' text="{text}"'
            res = p.get("result")
            print(f"{n:5d} t={t:7.2f} {name:28s} args={args} {extra} result={res}")
        elif ev == "EXE.dispatch_0x40d4d0":
            objdw = fmt_dwords(p.get("obj_dwords"))
            print(f"{n:5d} t={t:7.2f} DISPATCH a0={p.get('a0')} a1={p.get('a1')} "
                  f"obj={p.get('obj')} obj_dw=[{objdw}] caller={p.get('caller')}")
        elif ev == "GCAI.callback":
            pdw = fmt_dwords(p.get("payload_dwords"))
            print(f"{n:5d} t={t:7.2f} CALLBACK code={p.get('code')} "
                  f"payload=[{pdw}] ctx={p.get('context')}")
        elif ev == "IO.CreateFileW" or ev == "IO.CreateFileA":
            print(f"{n:5d} t={t:7.2f} IO {p.get('name')}")
        elif ev.startswith("DLSIM."):
            name = ev[len("DLSIM."):]
            print(f"{n:5d} t={t:7.2f} DLSIM {name} args={p.get('args')} "
                  f"result={p.get('result')}")
        else:
            print(f"{n:5d} t={t:7.2f} {ev} {json.dumps(p, ensure_ascii=False)[:150]}")
    print(f"\ntotal eventos utiles: {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
