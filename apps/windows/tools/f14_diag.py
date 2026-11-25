#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_diag.py — Arnés de diagnóstico del arranque del manager oficial.

Uso: python f14_diag.py [duracion-seg] [salida.jsonl]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import json
import os
import sys
import time

import frida

EXE_DIR = (r"C:\Program Files (x86)\CASIO"
           r"\fx-CG Manager PLUS Subscription for fx-CG50series")
EXE = os.path.join(EXE_DIR, "fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe")
HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "f14_diag_script.js")


def main() -> int:
    duration = float(sys.argv[1]) if len(sys.argv) > 1 else 60.0
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        HERE, "..", "verification", "2026-10-16-f1-dispatcher",
        "frida-diag-manager.jsonl")
    out_path = os.path.abspath(out_path)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    device = frida.get_local_device()
    argv = [EXE, '/n"fx-CG Manager PLUS Subscription for fx-CG50series"']
    pid = device.spawn(argv, cwd=EXE_DIR)
    session = device.attach(pid)
    with open(SCRIPT, encoding="utf-8") as fh:
        code = fh.read()
    script = session.create_script(code)
    out = open(out_path, "w", encoding="utf-8", buffering=1)
    count = [0]

    def on_message(message, data):
        count[0] += 1
        out.write(json.dumps(
            {"seq": count[0], "wall": time.time(), "message": message},
            ensure_ascii=False, default=str) + "\n")

    script.on("message", on_message)
    script.load()
    device.resume(pid)
    print(f"[diag] pid={pid} corriendo hasta {duration}s o exit…")
    deadline = time.time() + duration
    while time.time() < deadline:
        time.sleep(0.5)
    try:
        device.kill(pid)
    except Exception:
        pass
    try:
        session.detach()
    except Exception:
        pass
    out.close()
    print(f"[diag] eventos={count[0]} -> {out_path}")
    os._exit(0)


if __name__ == "__main__":
    sys.exit(main())
