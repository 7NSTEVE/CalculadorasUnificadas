#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
grab_app_window.py — Captura el rect exacto de la ventana "CasioNativeMain"
(archivo o pantalla) y recorta sus sub-regiones (sidebar izquierdo, canvas).

Uso: python grab_app_window.py <salida-base.png>
Escribe <base>.png (ventana completa). Imprime el rect y estadisticas.
Herramienta de verificación del proyecto Calculadora Unificada (2026-10-16).
"""
import ctypes
import ctypes.wintypes as wt
import sys

from PIL import ImageGrab

user32 = ctypes.WinDLL("user32", use_last_error=True)
user32.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.GetWindowRect.restype = wt.BOOL
MAIN_CLASS = "CasioNativeMain"


def find_window(cls):
    result = []

    @ctypes.WINFUNCTYPE(ctypes.wintypes.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == cls:
            result.append(hwnd)
        return True

    user32.EnumWindows(cb, 0)
    return result


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    hwnds = find_window(MAIN_CLASS)
    print(f"VENTANAS_ENCONTRADAS={len(hwnds)}")
    if not hwnds:
        return 1
    for hwnd in hwnds:
        rect = wt.RECT()
        if not user32.GetWindowRect(hwnd, ctypes.byref(rect)):
            print(f"GetWindowRect_FAILED 0x{hwnd:X}")
            continue
        visible = user32.IsWindowVisible(hwnd)
        iconic = user32.IsIconic(hwnd)
        print(f"HWND=0x{hwnd:X} RECT=({rect.left},{rect.top},{rect.right},{rect.bottom}) "
              f"visible={bool(visible)} minimizada={bool(iconic)}")
        if not visible or iconic:
            continue
        box = (rect.left, rect.top, rect.right, rect.bottom)
        shot = ImageGrab.grab(bbox=box)
        out = sys.argv[1]
        shot.save(out)
        pixels = list(shot.getdata())
        lit = sum(1 for p in pixels if max(p) > 8)
        colors = {}
        for p in pixels:
            q = (p[0] // 32 * 32 + 16, p[1] // 32 * 32 + 16, p[2] // 32 * 32 + 16)
            colors[q] = colors.get(q, 0) + 1
        top = sorted(colors.items(), key=lambda kv: -kv[1])[:6]
        print(f"  GUARDADO={out} pixels={len(pixels)} lit={lit} "
              f"({100.0 * lit / max(1, len(pixels)):.2f}%)")
        print("  TOP_COLORS=" +
              str([(f"#{r:02X}{g:02X}{b:02X}", n) for (r, g, b), n in top]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
