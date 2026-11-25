#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""img_diff.py — Diferencia píxel a píxel entre dos imágenes + colores top.

Uso: python img_diff.py <antes.png> <despues.png> [umbral]
Herramienta de verificación del proyecto Calculadora Unificada (2026-10-16).
"""
import sys

from PIL import Image, ImageChops


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    thr = int(sys.argv[3]) if len(sys.argv) > 3 else 4
    b = Image.open(sys.argv[1]).convert("RGB")
    a = Image.open(sys.argv[2]).convert("RGB")
    if b.size != a.size:
        print(f"TAMANOS_DISTINTOS {b.size} vs {a.size}")
        return 2
    diff = ImageChops.difference(b, a)
    bbox = diff.getbbox()
    px = list(diff.getdata())
    changed = sum(1 for p in px if max(p) > thr)
    print(f"DIFF_BBOX={bbox} PIXELES_CAMBIADOS(>{thr})={changed} de {len(px)}")
    for name, img in (("ANTES", b), ("DESPUES", a)):
        colors = {}
        for p in img.getdata():
            q = (p[0] // 32 * 32 + 16, p[1] // 32 * 32 + 16, p[2] // 32 * 32 + 16)
            colors[q] = colors.get(q, 0) + 1
        top = sorted(colors.items(), key=lambda kv: -kv[1])[:5]
        pretty = [(f"#{r:02X}{g:02X}{b2:02X}", n) for (r, g, b2), n in top]
        print(f"{name}_TOP_COLORS={pretty}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
