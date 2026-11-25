#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gui_lcd_fresh_capture.py — Captura fresca del rectangulo LCD de la CG50 en la
GUI en ejecucion (usa la misma geometria que gui_power_verification.py).

Uso: python gui_lcd_fresh_capture.py <salida-base>
Escribe <base>.png (recorte LCD) e imprime estadisticas.
Herramienta de verificación del proyecto Calculadora Unificada (2026-10-16).
"""
import ctypes
import ctypes.wintypes as wt
import os
import sys

from PIL import ImageGrab

user32 = ctypes.WinDLL("user32", use_last_error=True)
MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"
CG50_SKIN_W, CG50_SKIN_H = 664, 1364
CG50_LCD_RECT = (92, 153, 572, 403)


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
    return result[0] if result else None


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    hwnd = find_window(MAIN_CLASS)
    if not hwnd:
        print("RESULT window_not_found")
        return 1
    canvas = []

    @ctypes.WINFUNCTYPE(ctypes.wintypes.BOOL, wt.HWND, wt.LPARAM)
    def enum_child(chwnd, _):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(chwnd, buf, 256)
        if buf.value == CANVAS_CLASS:
            canvas.append(chwnd)
            return False
        return True

    user32.EnumChildWindows(hwnd, enum_child, 0)
    if not canvas:
        print("RESULT canvas_not_found")
        return 1
    chwnd = canvas[0]
    rect = wt.RECT()
    user32.GetClientRect(chwnd, ctypes.byref(rect))
    total_w, total_h = rect.right, rect.bottom
    fit_h = total_h - 40
    max_h_from_w = (total_w - 24) * CG50_SKIN_H // CG50_SKIN_W
    if max_h_from_w < fit_h:
        fit_h = max_h_from_w
    if fit_h < 300:
        fit_h = 300
    target_h = fit_h
    target_w = CG50_SKIN_W * target_h // CG50_SKIN_H
    vx = (total_w - target_w) // 2
    vy = (total_h - target_h) // 2
    if vy < 10:
        vy = 10
    lx0 = CG50_LCD_RECT[0] * target_w // CG50_SKIN_W
    ly0 = CG50_LCD_RECT[1] * target_h // CG50_SKIN_H
    lx1 = CG50_LCD_RECT[2] * target_w // CG50_SKIN_W
    ly1 = CG50_LCD_RECT[3] * target_h // CG50_SKIN_H

    def to_screen(cx, cy):
        pt = wt.POINT(cx, cy)
        user32.ClientToScreen(chwnd, ctypes.byref(pt))
        return pt.x, pt.y

    s0 = to_screen(vx + lx0, vy + ly0)
    s1 = to_screen(vx + lx1, vy + ly1)
    box = (s0[0], s0[1], s1[0], s1[1])
    shot = ImageGrab.grab()
    crop = shot.crop(box)
    out = sys.argv[1]
    crop.save(out)
    pixels = list(crop.getdata())
    lit = sum(1 for p in pixels if max(p) > 8)
    maxb = max((max(p) for p in pixels), default=0)
    colors = {}
    for p in pixels:
        q = (p[0] // 32 * 32 + 16, p[1] // 32 * 32 + 16, p[2] // 32 * 32 + 16)
        colors[q] = colors.get(q, 0) + 1
    top = sorted(colors.items(), key=lambda kv: -kv[1])[:6]
    print(f"LCD_FRESH_BOX={box} pixels={len(pixels)} lit={lit} "
          f"({100.0 * lit / max(1, len(pixels)):.2f}%) max={maxb}")
    print("TOP_COLORS=" +
          str([(f'#{r:02X}{g:02X}{b:02X}', n) for (r, g, b), n in top]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
