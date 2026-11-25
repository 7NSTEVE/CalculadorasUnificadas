#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""restore_pre30_backup.py — Reconstruye el respaldo pre-#30 de REGISTRO_CAMBIOS.md
(trunca los últimos 8246 bytes = entrada #30) y verifica el SHA-256 esperado
0ad1ff6f... (documentado por la sesión opencode en #29)."""
import hashlib
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = (r"E:\Descompilados\11_Otros\00_DOCUMENTACION"
       r"\REGISTRO_CAMBIOS.md.bak_sesion-powerfix_2026-10-16")
DST = (r"E:\Descompilados\11_Otros\00_DOCUMENTACION"
       r"\REGISTRO_CAMBIOS.md.bak_pre-30_2026-10-16")
ENTRY_BYTES = 8246
EXPECTED = ("0ad1ff6fc7af997ffb504d66f0f1c2c541722e546f8bbb3c658"
            "645960cb901e3b")

data = open(SRC, "rb").read()
pre = data[:-ENTRY_BYTES]
sha = hashlib.sha256(pre).hexdigest()
open(DST, "wb").write(pre)
print(f"reconstruido={DST}")
print(f"bytes={len(pre)} sha256={sha}")
print(f"COINCIDE_CON_0AD1FF6F={sha == EXPECTED}")
