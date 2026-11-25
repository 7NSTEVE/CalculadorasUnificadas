#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gui_power_final_ab.py — Verificación visual DEFINITIVA del power-on corregido
en la GUI real: secuencia limpia APAGADO -> clic AC/ON -> ENCENDIDO.

Requisito: la app debe estar recién lanzada (LCD de la CG50 apagado).
Usa la vista por defecto "las 3" y la geometria exacta de LayoutCalculators
(tercer slot = CG50). Captura via PrintWindow (inmune a ventanas superpuestas).

Uso: python gui_power_final_ab.py [--out <carpeta>] [--wait-boot <s>]
Herramienta de verificación del proyecto Calculadora Unificada (2026-10-16).
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

from PIL import Image

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
user32.SendMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.SendMessageW.restype = wt.LPARAM
user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, wt.INT]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.PrintWindow.argtypes = [wt.HWND, wt.HDC, wt.UINT]
user32.PrintWindow.restype = wt.BOOL
gdi32.CreateCompatibleDC.argtypes = [wt.HDC]
gdi32.CreateCompatibleDC.restype = wt.HDC
gdi32.CreateCompatibleBitmap.argtypes = [wt.HDC, wt.INT, wt.INT]
gdi32.CreateCompatibleBitmap.restype = wt.HBITMAP
gdi32.SelectObject.argtypes = [wt.HDC, wt.HGDIOBJ]
gdi32.SelectObject.restype = wt.HGDIOBJ
gdi32.DeleteObject.argtypes = [wt.HGDIOBJ]
gdi32.DeleteObject.restype = wt.BOOL
gdi32.DeleteDC.argtypes = [wt.HDC]
gdi32.DeleteDC.restype = wt.BOOL
gdi32.GetDIBits.argtypes = [wt.HDC, wt.HBITMAP, wt.UINT, wt.UINT,
                            wt.LPVOID, wt.LPVOID, wt.UINT]
gdi32.GetDIBits.restype = wt.INT
user32.GetDC.argtypes = [wt.HWND]
user32.GetDC.restype = wt.HDC
user32.ReleaseDC.argtypes = [wt.HWND, wt.HDC]
user32.ReleaseDC.restype = wt.INT

WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"

SKINS = [(644, 1250), (405, 816), (664, 1364)]  # CW, VN, CG50
CG50_LCD = (92, 153, 572, 403)
AC_ON_SKIN = (542, 956)


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.UINT), ("biWidth", wt.LONG),
                ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
                ("biBitCount", wt.WORD), ("biCompression", wt.UINT),
                ("biSizeImage", wt.UINT), ("biXPelsPerMeter", wt.LONG),
                ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.UINT),
                ("biClrImportant", wt.UINT)]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.UINT * 3)]


def find_visible(cls):
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


def canvas_size(canvas):
    rect = wt.RECT()
    user32.GetClientRect(canvas, ctypes.byref(rect))
    return rect.right, rect.bottom


def render_window2(hwnd, w, h):
    """PrintWindow + GetDIBits -> PIL (devuelve RGB correcto)."""
    hdc_window = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_window)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_window, w, h)
    gdi32.SelectObject(hdc_mem, hbmp)
    ok = user32.PrintWindow(hwnd, hdc_mem, 1)
    bmi = BITMAPINFO()
    bmi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.bmiHeader.biWidth = w
    bmi.bmiHeader.biHeight = -h
    bmi.bmiHeader.biPlanes = 1
    bmi.bmiHeader.biBitCount = 32
    buf = ctypes.create_string_buffer(w * h * 4)
    got = gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), 0)
    user32.ReleaseDC(hwnd, hdc_window)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    if not ok or got != h:
        print(f"RESULT render_failed ok={ok} got={got}")
        return None
    return Image.frombytes("RGB", (w, h), buf.raw, "raw", "BGRX", 0, 1)


def allthree_cg50(total_w, total_h):
    """Layout 'las 3': devuelve (vx, vy, tw, th) del tercer slot (CG50)."""
    n = 3
    fit_h = total_h - 40
    slot_w = total_w // n
    for sw, sh in SKINS:
        m = (slot_w - 24) * sh // sw
        if m < fit_h:
            fit_h = m
    if fit_h < 300:
        fit_h = 300
    th = fit_h
    tw = SKINS[2][0] * th // SKINS[2][1]
    vx = 2 * slot_w + (slot_w - tw) // 2
    vy = max(10, (total_h - th) // 2)
    return vx, vy, tw, th


def crop_stats(img):
    pixels = list(img.getdata())
    lit = sum(1 for p in pixels if max(p) > 24)
    white = sum(1 for p in pixels if min(p) > 200)
    return len(pixels), lit, white


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="verification/2026-10-16")
    ap.add_argument("--wait-boot", type=float, default=15.0)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    hwnds = find_visible(MAIN_CLASS)
    if not hwnds:
        print("RESULT window_not_found")
        return 1
    canvas = find_child(hwnds[0], CANVAS_CLASS)
    if not canvas:
        print("RESULT canvas_not_found")
        return 1
    print(f"Esperando boot {args.wait_boot}s...")
    time.sleep(args.wait_boot)

    cw, chh = canvas_size(canvas)
    vx, vy, tw, th = allthree_cg50(cw, chh)
    lx0 = CG50_LCD[0] * tw // SKINS[2][0]
    ly0 = CG50_LCD[1] * th // SKINS[2][1]
    lx1 = CG50_LCD[2] * tw // SKINS[2][0]
    ly1 = CG50_LCD[3] * th // SKINS[2][1]
    box = (vx + lx0, vy + ly0, vx + lx1, vy + ly1)
    print(f"CG50_LCD_CLIENT_BOX={box} canvas={cw}x{chh}")

    img_before = render_window2(canvas, cw, chh)
    if img_before is None:
        return 2
    img_before.save(os.path.join(args.out, "gui-ab-canvas-antes.png"))
    lcd_before = img_before.crop(box)
    lcd_before.save(os.path.join(args.out, "gui-ab-cg50-lcd-antes.png"))
    nb, lb, wb = crop_stats(lcd_before)
    print(f"LCD_ANTES pixels={nb} lit={lb} ({100.0*lb/nb:.2f}%) "
          f"blancos={wb} ({100.0*wb/nb:.2f}%)")

    ax = vx + AC_ON_SKIN[0] * tw // SKINS[2][0]
    ay = vy + AC_ON_SKIN[1] * th // SKINS[2][1]
    lparam = (ay << 16) | (ax & 0xFFFF)
    print(f"CLICK_AC at=({ax},{ay})")
    user32.SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lparam)
    time.sleep(0.15)
    user32.SendMessageW(canvas, WM_LBUTTONUP, 0, lparam)
    time.sleep(5.0)

    img_after = render_window2(canvas, cw, chh)
    if img_after is None:
        return 2
    img_after.save(os.path.join(args.out, "gui-ab-canvas-despues.png"))
    lcd_after = img_after.crop(box)
    lcd_after.save(os.path.join(args.out, "gui-ab-cg50-lcd-despues.png"))
    na, la, wa = crop_stats(lcd_after)
    print(f"LCD_DESPUES pixels={na} lit={la} ({100.0*la/na:.2f}%) "
          f"blancos={wa} ({100.0*wa/na:.2f}%)")

    verdict = "FAIL"
    if wa > na * 0.05 and wb < nb * 0.02:
        verdict = "PASS_APAGADO_A_ENCENDIDO"
    elif wa > na * 0.05:
        verdict = "PASS_YA_ENCENDIDA"
    print(f"VERDICT={verdict}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
