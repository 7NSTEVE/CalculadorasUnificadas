#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_registro_prefix.py — Diagnóstico de la frontera pre/post-#30 del
REGISTRO_CAMBIOS.md: hash del prefijo de 302007 B, texto en la frontera y
comparación con el inicio de la entrada #30."""
import hashlib
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
CUR = (r"E:\Descompilados\11_Otros\00_DOCUMENTACION"
       r"\REGISTRO_CAMBIOS.md")
TMP = (r"E:\Descompilados\11_Otros\Emuladores\Calculadoras_blanca_y_gris"
       r"\Calculadora Unificada\verification\2026-10-16"
       r"\_entrada30_registro.tmp.md")

data = open(CUR, "rb").read()
tmp = open(TMP, "rb").read()
print(f"actual: {len(data)} B  tmp: {len(tmp)} B")
for cut in (302007, len(data) - len(tmp)):
    prefix = data[:cut]
    print(f"prefijo[{cut}]: {len(prefix)} B sha256="
          f"{hashlib.sha256(prefix).hexdigest()}")
print("--- FIN DEL PREFIJO (últimos 240 bytes) ---")
print(data[302007 - 240:302007].decode("utf-8", errors="replace"))
print("--- INICIO DEL SUFIJO (primeros 240 bytes tras el corte) ---")
print(data[302007:302007 + 240].decode("utf-8", errors="replace"))
print("--- INICIO DEL TMP ---")
print(tmp[:240].decode("utf-8", errors="replace"))
print("--- ¿EL SUFIJO == TMP? ---")
print(f"igual={data[len(data) - len(tmp):] == tmp}")
# busca si el tmp aparece tal cual en el archivo
pos = data.find(tmp)
print(f"tmp_encontrado_en_byte={pos}")
# y con conversión LF->CRLF
tmp_crlf = tmp.replace(b"\n", b"\r\n")
pos2 = data.find(tmp_crlf)
print(f"tmp_crlf_encontrado_en_byte={pos2} (tamaño_crlf={len(tmp_crlf)})")
