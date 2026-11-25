#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lcd_surface_map.py — Mapa del volcado BGRA 1024x512 del LCD de la CG50.

Informa: píxeles no-negros totales, dentro de la región de contenido
(396x224, offset de fila 1024) y fuera de ella, mas una mini-visualizacion
ASCII 40x14 del contenido y colores dominantes del contenido.

Uso: python lcd_surface_map.py <lcd-image-bgra.bin> [ascii]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import struct
import sys

W, H = 1024, 512
CW, CH = 396, 224


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    data = open(sys.argv[1], "rb").read()
    if len(data) < W * H * 4:
        print(f"VOLCADO_CORTO bytes={len(data)} esperado={W * H * 4}")
        return 2
    pix = struct.unpack_from("<%dI" % (W * H), data, 0)

    def px(x, y):
        return pix[y * W + x]

    total = 0
    inside = 0
    for v in pix:
        if v & 0x00FFFFFF or v >> 24:
            total += 1
    for y in range(CH):
        for x in range(CW):
            v = px(x, y)
            if v & 0x00FFFFFF or v >> 24:
                inside += 1
    print(f"NONZERO_TOTAL={total} ({100.0 * total / (W * H):.2f}% de {W}x{H})")
    print(f"NONZERO_CONTENIDO={inside} ({100.0 * inside / (CW * CH):.2f}% de {CW}x{CH})")
    print(f"NONZERO_FUERA={total - inside}")

    colors = {}
    for y in range(CH):
        for x in range(CW):
            v = px(x, y)
            if v & 0x00FFFFFF or v >> 24:
                b, g, r = v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF
                q = (r // 32 * 32 + 16, g // 32 * 32 + 16, b // 32 * 32 + 16)
                colors[q] = colors.get(q, 0) + 1
    top = sorted(colors.items(), key=lambda kv: -kv[1])[:8]
    print("CONTENIDO_TOP_COLORS=" +
          str([(f"#{r:02X}{g:02X}{b:02X}", n) for (r, g, b), n in top]))

    # mini-mapa ASCII del contenido (muestreo 10x16 px por celda)
    if len(sys.argv) > 2 and sys.argv[2] == "ascii":
        ramp = " .:-=+*#%@"
        for gy in range(0, CH, 16):
            row = ""
            for gx in range(0, CW, 10):
                s = 0
                n = 0
                for y in range(gy, min(gy + 16, CH), 4):
                    for x in range(gx, min(gx + 10, CW), 2):
                        v = px(x, y)
                        b, g, r = v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF
                        s += (r + g + b) / 3
                        n += 1
                avg = s / max(1, n)
                row += ramp[min(9, int(avg * 10 // 256))]
            print("|" + row + "|")
    return 0


if __name__ == "__main__":
    sys.exit(main())
