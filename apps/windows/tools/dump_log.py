#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""dump_log.py — Volcado de logs del probe (UTF-16/UTF-8/auto) a stdout UTF-8.

Uso: python dump_log.py <archivo.log> [cola-caracteres]
Cola por defecto: 4000 caracteres (0 = todo).
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import sys


def decode_any(data: bytes) -> str:
    # BOM explícito primero (logs capturados via PowerShell = UTF-16 con BOM);
    # sin BOM se asume ANSI/UTF-8 (logs capturados via cmd '>').
    if data.startswith(b"\xff\xfe"):
        return data.decode("utf-16-le", errors="replace")
    if data.startswith(b"\xfe\xff"):
        return data.decode("utf-16-be", errors="replace")
    if data.startswith(b"\xef\xbb\xbf"):
        return data.decode("utf-8-sig", errors="replace")
    try:
        text = data.decode("utf-8")
        # valida: si contiene caracteres de control extraños no textuales, cae
        if sum(1 for ch in text[:4000] if ch < " " and ch not in "\t\n\r") < 40:
            return text
    except (UnicodeDecodeError, UnicodeError):
        pass
    return data.decode("cp1252", errors="replace")


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    path = sys.argv[1]
    tail = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
    data = open(path, "rb").read()
    text = decode_any(data)
    if tail and len(text) > tail:
        print(f"[... {len(text) - tail} caracteres omitidos ...]")
        text = text[-tail:]
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
