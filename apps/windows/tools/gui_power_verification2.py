#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gui_power_verification.py (v2) — Verificación visual de la GUI real de
Calculadora Unificada con el power-on corregido (slot 0x10, 2026-10-16).

Mejoras v2:
 - argtypes explicitos para todas las APIs user32 (evita params corruptos).
 - Verificacion empirica del cambio de vista (cuenta agrupaciones de skin
   brillantes en el canvas antes de continuar).
 - Geometria re-leida justo antes de cada paso (tolerante a resize).
 - Si el LCD ya esta encendido ANTES del clic, lo declara PASS por estado
   (powered) y de todos modos envia el clic AC/ON para ejercitar el flujo.

Uso: python gui_power_verification2.py [--out <carpeta>]
Herramienta de verificación del proyecto Calculadora Unificada (2026-10-16).
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

from PIL import Image, ImageGrab

user32 = ctypes.WinDLL("user32", use_last_error=True)
user32.SendMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.SendMessageW.restype = wt.LPARAM
user32.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.PostMessageW.restype = wt.BOOL
user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, wt.INT]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]
user32.SetForegroundWindow.argtypes = [wt.HWND]
user32.IsWindowVisible.argtypes = [wt.HWND]

WM_COMMAND = 0x0111
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
ID_BTN_MODE_GRAPH = 1014

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"
CG50_SKIN_W, CG50_SKIN_H = 664, 1364
CG50_LCD_RECT = (92, 153, 572, 403)
AC_ON_SKIN = (542, 956)


def find_windows(cls):
    result = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == cls and user32.IsWindowVisible(hwnd):
            result.append(hwnd)
        return True

    user32.EnumWindows(cb, 0)
    return result


def find_child(parent, cls):
    result = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == cls:
            result.append(hwnd)
            return False
        return True

    user32.EnumChildWindows(parent, cb, 0)
    return result[0] if result else None


def canvas_rect(canvas):
    rect = wt.RECT()
    user32.GetClientRect(canvas, ctypes.byref(rect))
    return rect.right, rect.bottom


def solo_layout(total_w, total_h):
    fit_h = total_h - 40
    max_h_from_w = (total_w - 24) * CG50_SKIN_H // CG50_SKIN_W
    if max_h_from_w < fit_h:
        fit_h = max_h_from_w
    if fit_h < 300:
        fit_h = 300
    target_h = fit_h
    target_w = CG50_SKIN_W * target_h // CG50_SKIN_H
    vx = (total_w - target_w) // 2
    vy = max(10, (total_h - target_h) // 2)
    return vx, vy, target_w, target_h


def to_screen(hwnd, cx, cy):
    pt = wt.POINT(cx, cy)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return pt.x, pt.y


def screen_crop(box):
    return ImageGrab.grab(bbox=box)


def crop_lit(img):
    pixels = list(img.getdata())
    lit = sum(1 for p in pixels if max(p) > 24)
    return len(pixels), lit


def count_skin_clusters(canvas_img):
    """Cuenta agrupaciones horizontales de columnas 'brillantes' (skins)."""
    w, h = canvas_img.size
    rgb = canvas_img.convert("RGB")
    cols = []
    for x in range(0, w, 8):
        bright = 0
        for y in range(0, h, 24):
            p = rgb.getpixel((x, y))
            if max(p) > 90:
                bright += 1
        cols.append(bright > (h // 24) * 0.25)
    clusters = 0
    prev = False
    for c in cols:
        if c and not prev:
            clusters += 1
        prev = c
    return clusters


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="verification/2026-10-16")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    hwnds = find_windows(MAIN_CLASS)
    if not hwnds:
        print("RESULT window_not_found")
        return 1
    hwnd = hwnds[0]
    canvas = find_child(hwnd, CANVAS_CLASS)
    if not canvas:
        print("RESULT canvas_not_found")
        return 1

    # 1) Cambio a vista solo-CG50 + verificacion empirica
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.2)
    cw, chh = canvas_rect(canvas)
    scr = to_screen(canvas, 0, 0)
    canvas_img = screen_crop((scr[0], scr[1], scr[0] + cw, scr[1] + chh))
    clusters = count_skin_clusters(canvas_img)
    print(f"VISTA_SOLO_CG50 clusters_detectados={clusters} (esperado 1)")
    if clusters > 1:
        # reintento con PostMessage
        user32.PostMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
        time.sleep(1.2)
        cw, chh = canvas_rect(canvas)
        scr = to_screen(canvas, 0, 0)
        canvas_img = screen_crop((scr[0], scr[1], scr[0] + cw, scr[1] + chh))
        clusters = count_skin_clusters(canvas_img)
        print(f"VISTA_SOLO_CGLO reintento clusters={clusters}")
        if clusters > 1:
            print("RESULT view_switch_failed")
            return 2

    # 2) Geometria LCD y captura ANTES
    vx, vy, tw, th = solo_layout(cw, chh)
    lx0 = CG50_LCD_RECT[0] * tw // CG50_SKIN_W
    ly0 = CG50_LCD_RECT[1] * th // CG50_SKIN_H
    lx1 = CG50_LCD_RECT[2] * tw // CG50_SKIN_W
    ly1 = CG50_LCD_RECT[3] * th // CG50_SKIN_H
    s0 = to_screen(canvas, vx + lx0, vy + ly0)
    s1 = to_screen(canvas, vx + lx1, vy + ly1)
    lcd_box = (s0[0], s0[1], s1[0], s1[1])
    print(f"LCD_BOX={lcd_box}")
    before = screen_crop(lcd_box)
    before_path = os.path.join(args.out, "gui-v2-lcd-before.png")
    before.save(before_path)
    nb, lb = crop_lit(before)
    print(f"LCD_BEFORE pixels={nb} lit={lb} ({100.0 * lb / nb:.2f}%)")

    # 3) Clic sintetico AC/ON (solo-CG50: coords directas)
    ax = vx + AC_ON_SKIN[0] * tw // CG50_SKIN_W
    ay = vy + AC_ON_SKIN[1] * th // CG50_SKIN_H
    lparam = (ay << 16) | (ax & 0xFFFF)
    user32.SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lparam)
    time.sleep(0.12)
    user32.SendMessageW(canvas, WM_LBUTTONUP, 0, lparam)
    print("CLICK_AC_SENT")
    time.sleep(4.5)

    # 4) Captura DESPUES (geometria re-leida)
    cw2, ch2 = canvas_rect(canvas)
    if (cw2, ch2) != (cw, chh):
        vx, vy, tw, th = solo_layout(cw2, ch2)
        lx0 = CG50_LCD_RECT[0] * tw // CG50_SKIN_W
        ly0 = CG50_LCD_RECT[1] * th // CG50_SKIN_H
        lx1 = CG50_LCD_RECT[2] * tw // CG50_SKIN_W
        ly1 = CG50_LCD_RECT[3] * th // CG50_SKIN_H
        s0 = to_screen(canvas, vx + lx0, vy + ly0)
        s1 = to_screen(canvas, vx + lx1, vy + ly1)
        lcd_box = (s0[0], s0[1], s1[0], s1[1])
        print(f"LCD_BOX_RECALC={lcd_box} (canvas cambió de tamaño)")
    after = screen_crop(lcd_box)
    after_path = os.path.join(args.out, "gui-v2-lcd-after.png")
    after.save(after_path)
    na, la = crop_lit(after)
    print(f"LCD_AFTER pixels={na} lit={la} ({100.0 * la / na:.2f}%)")

    verdict = "FAIL_OSCURO"
    if la > na * 0.05:
        verdict = "PASS_LCD_CON_CONTENIDO"
    elif lb > nb * 0.05:
        verdict = "PASS_YA_ENCENDIDA_ANTES_DEL_CLIC"
    print(f"DELTA_LIT={la - lb} VERDICT={verdict}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
