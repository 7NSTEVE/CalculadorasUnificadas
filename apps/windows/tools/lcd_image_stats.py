#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lcd_image_stats.py — Estadísticas objetivas de capturas del LCD de la CG50.

Para cada imagen: tamaño, píxeles no-negros (y %), color dominante de los
píxeles no-negros, brillo máximo. Sirve para verificar si el LCD muestra
contenido (boot/menú) o está apagado, sin depender de inspección visual.

Uso: python lcd_image_stats.py <imagen.png> [imagen2.png ...]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import sys

try:
    from PIL import Image
except ImportError:
    print("PIL no disponible: pip install pillow")
    sys.exit(2)


def stats(path: str) -> None:
    img = Image.open(path)
    img = img.convert("RGB")
    w, h = img.size
    pixels = list(img.getdata())
    total = len(pixels)
    nonzero = [p for p in pixels if p != (0, 0, 0)]
    # tolerancia: píxeles casi negros (<=8 por canal) cuentan como negros
    lit = [p for p in nonzero if max(p) > 8]
    maxb = max((max(p) for p in pixels), default=0)
    dominant = None
    if lit:
        # color dominante por cuantización a 32
        buckets = {}
        for p in lit:
            q = (p[0] // 32, p[1] // 32, p[2] // 32)
            buckets[q] = buckets.get(q, 0) + 1
        dq = max(buckets, key=buckets.get)
        dominant = (dq[0] * 32 + 16, dq[1] * 32 + 16, dq[2] * 32 + 16)
    print(f"{path}")
    print(f"  tamano={w}x{h} total={total} "
          f"no_negros={len(nonzero)} ({100.0*len(nonzero)/total:.3f}%) "
          f"encendidos(>8)={len(lit)} ({100.0*len(lit)/total:.3f}%) "
          f"brillo_max={maxb}")
    if dominant:
        print(f"  color_dominante_aprox={dominant} "
              f"rgb_hex=#{dominant[0]:02X}{dominant[1]:02X}{dominant[2]:02X}")
    else:
        print("  color_dominante=NINGUNO (todo oscuro)")


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    for path in sys.argv[1:]:
        try:
            stats(path)
        except Exception as exc:
            print(f"{path}: ERROR {exc}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
