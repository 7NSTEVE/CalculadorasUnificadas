#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
backup_before_edit.py — Respaldo previo (regla de retención) de los archivos
que se van a editar, con verificación SHA-256 y registro imprimible.

Uso: python backup_before_edit.py <archivo1> [archivo2 ...]
Copia cada archivo a <archivo>.bak_<AAAA-MM-DD-HHMMSS> (sufijo único por
invocación) e imprime origen → respaldo + SHA-256 (deben cotejarse).
Herramienta de retención del proyecto Calculadora Unificada (2026-10-16).
"""
import datetime
import hashlib
import shutil
import sys

SUFFIX = ".bak_" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    for src in sys.argv[1:]:
        data = open(src, "rb").read()
        dst = src + SUFFIX
        shutil.copyfile(src, dst)
        check = open(dst, "rb").read()
        src_sha = hashlib.sha256(data).hexdigest()
        dst_sha = hashlib.sha256(check).hexdigest()
        ok = "OK" if src_sha == dst_sha else "MISMATCH"
        print(f"{ok} {src}")
        print(f"    sha256={src_sha} -> {dst} ({len(data)} bytes)")


if __name__ == "__main__":
    sys.exit(main())
