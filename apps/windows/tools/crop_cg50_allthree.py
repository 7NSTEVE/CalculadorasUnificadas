#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
crop_cg50_allthree.py — Recorta el LCD de la CG50 (tercera calculadora) de un
render del canvas en vista "las 3" (layout replicado de LayoutCalculators).

Uso: python crop_cg50_allthree.py <canvas.png> <lcd-out.png> [stats]
Herramienta de verificación del proyecto Calculadora Unificada (2026-10-16).
"""
import sys

from PIL import Image

SKINS = [(644, 1250), (405, 816), (664, 1364)]  # CW, VN, CG50
CG50_LCD = (92, 153, 572, 403)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    img = Image.open(sys.argv[1]).convert("RGB")
    total_w, total_h = img.size
    n = 3
    fit_h = total_h - 40
    slot_w = total_w // n
    for sw, sh in SKINS:
        max_h_from_w = (slot_w - 24) * sh // sw
        if max_h_from_w < fit_h:
            fit_h = max_h_from_w
    if fit_h < 300:
        fit_h = 300
    target_h = fit_h
    tw = SKINS[2][0] * target_h // SKINS[2][1]
    x = 2 * slot_w + (slot_w - tw) // 2
    y = max(10, (total_h - target_h) // 2)
    lx0 = CG50_LCD[0] * tw // SKINS[2][0]
    ly0 = CG50_LCD[1] * target_h // SKINS[2][1]
    lx1 = CG50_LCD[2] * tw // SKINS[2][0]
    ly1 = CG50_LCD[3] * target_h // SKINS[2][1]
    box = (x + lx0, y + ly0, x + lx1, y + ly1)
    crop = img.crop(box)
    crop.save(sys.argv[2])
    pixels = list(crop.getdata())
    lit = sum(1 for p in pixels if max(p) > 24)
    white = sum(1 for p in pixels if min(p) > 200)
    maxb = max((max(p) for p in pixels), default=0)
    colors = {}
    for p in pixels:
        q = (p[0] // 32 * 32 + 16, p[1] // 32 * 32 + 16, p[2] // 32 * 32 + 16)
        colors[q] = colors.get(q, 0) + 1
    top = sorted(colors.items(), key=lambda kv: -kv[1])[:6]
    print(f"CG50_LCD_BOX={box} pixels={len(pixels)} lit={lit} "
          f"({100.0 * lit / max(1, len(pixels)):.2f}%) "
          f"blancos(min>200)={white} max={maxb}")
    print("TOP_COLORS=" + str([(f"#{r:02X}{g:02X}{b:02X}", n2)
                               for (r, g, b), n2 in top]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
