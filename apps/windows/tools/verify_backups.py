#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_backups.py — Verifica marcadores y hashes de los respaldos pre-fix.
Uso: python verify_backups.py <archivo1> [archivo2 ...]"""
import hashlib
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
NEEDLES = [b"slot extra 0x10", b"power-on (slot 0x10)",
           b"powerReq[0] = 1", b"residentTaskId", b"powerfirst",
           b"ScanForRomPattern", b"powerCommandId", b"diagnosticPowerLegacy",
           b"resetResult"]
for path in sys.argv[1:]:
    data = open(path, "rb").read()
    print(path)
    print(f"  bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}")
    for needle in NEEDLES:
        count = data.count(needle)
        if count:
            print(f"    {needle.decode()} = {count}")
