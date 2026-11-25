#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_snapshot_state.py — Inspecciona qué versiones contiene el snapshot.
Uso: python check_snapshot_state.py <carpeta-snapshot>"""
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
base = sys.argv[1]
NEEDLES = [b"powerCommandId", b"residentTaskId", b"power-on (slot 0x10)",
           b"powerfirst", b"ScanForRomPattern", b"etapa 0b",
           b"powerReq[0] = 1", b"slot extra 0x10"]
FILES = [
    "cg50_backend.cpp",
    "tools/gcal-host-probe/main.cpp",
    "CMakeLists.txt",
    "tests/cg50_backend_contract_test.cpp",
    "main.cpp",
]
for rel in FILES:
    path = os.path.join(base, rel)
    if not os.path.exists(path):
        print(f"=== {rel}: NO EXISTE")
        continue
    data = open(path, "rb").read()
    print(f"=== {rel}: {len(data)} bytes")
    for needle in NEEDLES:
        n = data.count(needle)
        if n:
            print(f"    {needle.decode()}: {n} vez/veces")
