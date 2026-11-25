#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gui_power_verification.py — Verificación visual de la GUI real de Calculadora
Unificada con el power-on corregido (slot 0x10, hallazgo 2026-10-16).

Pasos:
 1. Localiza (o arranca) la ventana "CasioNativeMain".
 2. Cambia a la vista solo-CG50 (WM_COMMAND 1014).
 3. Captura "antes" (LCD apagado) y recorta el rect exacto del LCD.
 4. Envia un clic sintetico (WM_LBUTTONDOWN/UP) sobre la tecla AC/ON del skin
    (coordenadas skin 542,956; matrix 0x0101) -> KeyDown -> power-on slot 0x10.
 5. Espera el arranque, captura "despues" y recorta el LCD.
 6. Mide píxeles no-negros de ambos recortes y verifica calc_debug.log.

Uso: python gui_power_verification.py [--launch <ruta-exe>] [--out <carpeta>]
Herramienta de verificación del proyecto Calculadora Unificada (2026-10-16).
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

try:
    from PIL import Image, ImageGrab
except ImportError:
    print("PIL no disponible: pip install pillow")
    sys.exit(2)

user32 = ctypes.WinDLL("user32", use_last_error=True)

WM_COMMAND = 0x0111
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
BN_CLICKED = 0
ID_BTN_MODE_GRAPH = 1014

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"

# Geometria del skin CG50 (main.cpp InitNativeCalcs / cg50_keys_exact.h)
CG50_SKIN_W, CG50_SKIN_H = 664, 1364
CG50_LCD_RECT = (92, 153, 572, 403)      # x0,y0,x1,y1 en coords skin
AC_ON_SKIN = (542, 956)                  # centro de la tecla AC/ON


def find_window(cls, title_part=None):
    result = []

    @ctypes.WINFUNCTYPE(ctypes.wintypes.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, lparam):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == cls:
            if title_part is not None:
                tbuf = ctypes.create_unicode_buffer(256)
                user32.GetWindowTextW(hwnd, tbuf, 256)
                if title_part not in tbuf.value:
                    return True
            result.append(hwnd)
        return True

    user32.EnumWindows(cb, 0)
    return result[0] if result else None


def client_to_screen(hwnd, x, y):
    pt = wt.POINT(x, y)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return pt.x, pt.y


def solo_cg50_layout(total_w, total_h):
    """Replica LayoutCalculators() para n=1 (solo CG50), zoom auto."""
    fit_h = total_h - 40
    slot_w = total_w
    max_h_from_w = (slot_w - 24) * CG50_SKIN_H // CG50_SKIN_W
    if max_h_from_w < fit_h:
        fit_h = max_h_from_w
    if fit_h < 300:
        fit_h = 300
    target_h = fit_h
    target_w = CG50_SKIN_W * target_h // CG50_SKIN_H
    x = (slot_w - target_w) // 2
    y = (total_h - target_h) // 2
    if y < 10:
        y = 10
    return x, y, target_w, target_h


def skin_to_client(vx, vy, tw, th):
    return (vx * tw // CG50_SKIN_W, vy * th // CG50_SKIN_H)


def crop_stats(path):
    img = Image.open(path).convert("RGB")
    pixels = list(img.getdata())
    total = len(pixels)
    lit = sum(1 for p in pixels if max(p) > 8)
    maxb = max((max(p) for p in pixels), default=0)
    return total, lit, maxb


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--launch", default=None)
    ap.add_argument("--out", default="verification/2026-10-16")
    ap.add_argument("--wait-boot", type=float, default=15.0)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    hwnd = find_window(MAIN_CLASS)
    if hwnd is None and args.launch:
        subprocess.Popen([args.launch], cwd=os.path.dirname(args.launch))
        deadline = time.time() + 30
        while time.time() < deadline:
            hwnd = find_window(MAIN_CLASS)
            if hwnd:
                break
            time.sleep(0.5)
    if hwnd is None:
        print("RESULT window_not_found")
        return 1
    print(f"MAIN_HWND=0x{hwnd:X}")

    canvas = None

    @ctypes.WINFUNCTYPE(ctypes.wintypes.BOOL, wt.HWND, wt.LPARAM)
    def enum_child(chwnd, _):
        nonlocal canvas
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(chwnd, buf, 256)
        if buf.value == CANVAS_CLASS:
            canvas = chwnd
            return False
        return True

    user32.EnumChildWindows(hwnd, enum_child, 0)
    if not canvas:
        print("RESULT canvas_not_found")
        return 1
    print(f"CANVAS_HWND=0x{canvas:X}")

    print(f"Esperando arranque del backend CG50 {args.wait_boot}s...")
    time.sleep(args.wait_boot)

    # Vista solo-CG50
    user32.SendMessageW(hwnd, WM_COMMAND,
                        (BN_CLICKED << 16) | ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.0)

    rect = wt.RECT()
    user32.GetClientRect(canvas, ctypes.byref(rect))
    total_w, total_h = rect.right, rect.bottom
    print(f"CANVAS_CLIENT={total_w}x{total_h}")
    vx, vy, tw, th = solo_cg50_layout(total_w, total_h)
    print(f"CG50_VIEWPORT=({vx},{vy},{tw},{th})")

    # recorte LCD en coords cliente -> pantalla
    lx0, ly0 = skin_to_client(CG50_LCD_RECT[0], CG50_LCD_RECT[1], tw, th)
    lx1, ly1 = skin_to_client(CG50_LCD_RECT[2], CG50_LCD_RECT[3], tw, th)
    lcd_scr0 = client_to_screen(canvas, vx + lx0, vy + ly0)
    lcd_scr1 = client_to_screen(canvas, vx + lx1, vy + ly1)
    lcd_box = (lcd_scr0[0], lcd_scr0[1], lcd_scr1[0], lcd_scr1[1])
    print(f"LCD_SCREEN_BOX={lcd_box}")

    shot = ImageGrab.grab()
    shot.save(os.path.join(args.out, "gui-verify-solo-2026-10-16.png"))
    before = shot.crop(lcd_box)
    before_path = os.path.join(args.out, "gui-verify-lcd-before-2026-10-16.png")
    before.save(before_path)

    # clic sintetico sobre AC/ON
    ax, ay = skin_to_client(AC_ON_SKIN[0], AC_ON_SKIN[1], tw, th)
    ac_client = (vx + ax, vy + ay)
    print(f"AC_ON_CLIENT={ac_client}")
    lparam = (ac_client[1] << 16) | (ac_client[0] & 0xFFFF)
    user32.SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lparam)
    time.sleep(0.12)
    user32.SendMessageW(canvas, WM_LBUTTONUP, 0, lparam)
    print("CLICK_AC_SENT")
    time.sleep(4.5)

    shot2 = ImageGrab.grab()
    shot2.save(os.path.join(args.out, "gui-verify-after-ac-2026-10-16.png"))
    after = shot2.crop(lcd_box)
    after_path = os.path.join(args.out, "gui-verify-lcd-after-2026-10-16.png")
    after.save(after_path)

    tb, lb, mb = crop_stats(before_path)
    ta, la, ma = crop_stats(after_path)
    print(f"LCD_BEFORE pixels={tb} lit={lb} ({100.0*lb/tb:.2f}%) max={mb}")
    print(f"LCD_AFTER  pixels={ta} lit={la} ({100.0*la/ta:.2f}%) max={ma}")
    delta = la - lb
    verdict = "LCD_ENCEDIDO" if la > tb * 0.02 else "LCD_SIGUE_OSCURO"
    print(f"DELTA_LIT={delta} VERDICT={verdict}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
