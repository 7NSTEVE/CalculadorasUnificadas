#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""hash_files.py — SHA-256 de una lista de archivos (para REGISTRO/SHA256SUMS).
Uso: python hash_files.py <archivo1> [archivo2 ...]"""
import hashlib
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
for path in sys.argv[1:]:
    if not os.path.exists(path):
        print(f"NO_EXISTE  {path}")
        continue
    data = open(path, "rb").read()
    print(f"{hashlib.sha256(data).hexdigest()}  {len(data):>9} B  {path}")
