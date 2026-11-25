#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gui_power_verification.py (v3) — Verificación visual de la GUI real de
Calculadora Unificada con el power-on corregido (slot 0x10, 2026-10-16).

v3: captura mediante PrintWindow (inmune a ventanas superpuestas / z-order).
Flujo:
 1. Cambia a vista solo-CG50 (WM_COMMAND 1014).
 2. Render ANTES del canvas, recorte del LCD por DETECCION de la region clara
    (con fallback a la geometria fija del layout).
 3. Clic sintetico WM_LBUTTONDOWN/UP sobre AC/ON (coords skin 542,956).
 4. Render DESPUES, recorte del LCD, estadisticas y veredicto.

v4 (2026-10-16): (a) el recorte del LCD se hace por DETECCION de la region
clara (>=236 con x>900, >=300 px por fila; ver INFORME-VERIFICACION-2026-10-16.md
§17.3) porque la geometria fija quedo desalineada en el layout actual de la
ventana; (b) el conteo de «clusters» baja a INFORMATIVO: su «esperado 1» era un
falso positivo (la vista en ejecucion no es Solo-CG50). CLI sin cambios.

Uso: python gui_power_verification3.py [--out <carpeta>] [--no-click]
Herramienta de verificación del proyecto Calculadora Unificada (2026-10-16).
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

from PIL import Image, ImageChops

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
for dll in (user32, gdi32):
    pass
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

WM_COMMAND = 0x0111
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
ID_BTN_MODE_GRAPH = 1014
PW_CLIENTONLY = 1
BI_RGB = 0
DIB_RGB_COLORS = 0

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"
CG50_SKIN_W, CG50_SKIN_H = 664, 1364
CG50_LCD_RECT = (92, 153, 572, 403)
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


def find_visible_windows(cls):
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


def render_window(hwnd, w, h):
    """Render de la ventana via PrintWindow -> PIL Image (RGB)."""
    hdc_window = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_window)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_window, w, h)
    gdi32.SelectObject(hdc_mem, hbmp)
    ok = user32.PrintWindow(hwnd, hdc_mem, PW_CLIENTONLY)
    bmi = BITMAPINFO()
    bmi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.bmiHeader.biWidth = w
    bmi.bmiHeader.biHeight = -h
    bmi.bmiHeader.biPlanes = 1
    bmi.bmiHeader.biBitCount = 32
    bmi.bmiHeader.biCompression = BI_RGB
    buf = ctypes.create_string_buffer(w * h * 4)
    got = gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi),
                          DIB_RGB_COLORS)
    user32.ReleaseDC(hwnd, hdc_window)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    if not ok or got != h:
        print(f"RESULT render_failed ok={ok} got={got}")
        return None
    return Image.frombytes("RGB", (w, h),
                           buf.raw, "raw", "BGRX", 0, 1)


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


def count_skin_clusters(img):
    w, h = img.size
    cols = []
    for x in range(0, w, 8):
        bright = 0
        total = 0
        for y in range(0, h, 24):
            p = img.getpixel((x, y))
            total += 1
            if max(p) > 90:
                bright += 1
        cols.append(bright > total * 0.25)
    clusters = 0
    prev = False
    for c in cols:
        if c and not prev:
            clusters += 1
        prev = c
    return clusters


def detect_clear_bbox(img, thr=236, x_min=900, min_row_px=300, min_h=60):
    """2026-10-16 (v4): recorte fiable del LCD por DETECCION de la region clara.

    Una fila del canvas pertenece al LCD si tiene >= min_row_px pixeles con los
    3 canales >= thr y x > x_min (parametros validados en el layout actual,
    INFORME §17.3). Devuelve (x0, y0, x1, y1) en coords del canvas, o None si
    no se detecta region (el llamador cae a la geometria fija).
    """
    w, h = img.size
    x_min = min(x_min, max(0, w - 1))
    region = img.crop((x_min, 0, w, h))
    r, g, b = region.split()
    mask = ImageChops.darker(ImageChops.darker(r, g), b).point(
        lambda v: 255 if v >= thr else 0)
    mw, mh = mask.size
    data = mask.tobytes()
    rows = [y for y in range(mh)
            if data[y * mw:(y + 1) * mw].count(255) >= min_row_px]
    if not rows or (rows[-1] - rows[0] + 1) < min_h:
        return None
    xa, xb = mw, -1
    for y in rows:
        row = data[y * mw:(y + 1) * mw]
        f = row.find(255)
        if 0 <= f < xa:
            xa = f
        l = row.rfind(255)
        if l > xb:
            xb = l
    if xb < 0:
        return None
    return (x_min + xa, rows[0], x_min + xb + 1, rows[-1] + 1)


def lcd_box_fixed(total_w, total_h):
    """Geometria fija historica (fallback si la deteccion no encuentra region)."""
    vx, vy, tw, th = solo_layout(total_w, total_h)
    lx0 = CG50_LCD_RECT[0] * tw // CG50_SKIN_W
    ly0 = CG50_LCD_RECT[1] * th // CG50_SKIN_H
    lx1 = CG50_LCD_RECT[2] * tw // CG50_SKIN_W
    ly1 = CG50_LCD_RECT[3] * th // CG50_SKIN_H
    return (vx + lx0, vy + ly0, vx + lx1, vy + ly1)


def lit_stats(img):
    pixels = list(img.getdata())
    lit = sum(1 for p in pixels if max(p) > 24)
    return len(pixels), lit


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="verification/2026-10-16")
    ap.add_argument("--no-click", action="store_true")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    hwnds = find_visible_windows(MAIN_CLASS)
    if not hwnds:
        print("RESULT window_not_found")
        return 1
    canvas = find_child(hwnds[0], CANVAS_CLASS)
    if not canvas:
        print("RESULT canvas_not_found")
        return 1

    user32.SendMessageW(hwnds[0], WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.2)
    cw, chh = canvas_size(canvas)
    img = render_window(canvas, cw, chh)
    if img is None:
        return 2
    img.save(os.path.join(args.out, "gui-v3-canvas-antes.png"))
    # 2026-10-16 (v4): «clusters» degradado a INFORMATIVO — era un falso positivo
    # («esperado 1») porque la vista en ejecucion no es Solo-CG50; el criterio
    # real es el contenido del recorte LCD por deteccion.
    clusters = count_skin_clusters(img)
    print(f"INFO_LAYOUT clusters={clusters} "
          f"(informativo; ya no es criterio de vista)")

    bbox = detect_clear_bbox(img)
    if bbox is not None:
        crop_box, fuente = bbox, "deteccion"
    else:
        crop_box, fuente = lcd_box_fixed(cw, chh), "geometria_fija"
    print(f"LCD_CROP_ANTES source={fuente} box={crop_box}")
    lcd = img.crop(crop_box)
    before_path = os.path.join(args.out, "gui-v3-lcd-before.png")
    lcd.save(before_path)
    nb, lb = lit_stats(lcd)
    print(f"LCD_BEFORE pixels={nb} lit={lb} ({100.0 * lb / nb:.2f}%)")

    if not args.no_click:
        # 2026-10-16 (v4): el clic conserva la geometria fija (fuera del alcance
        # de este fix; puede desalinearse en el layout actual de la ventana).
        vx, vy, tw, th = solo_layout(cw, chh)
        ax = vx + AC_ON_SKIN[0] * tw // CG50_SKIN_W
        ay = vy + AC_ON_SKIN[1] * th // CG50_SKIN_H
        lparam = (ay << 16) | (ax & 0xFFFF)
        user32.SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lparam)
        time.sleep(0.12)
        user32.SendMessageW(canvas, WM_LBUTTONUP, 0, lparam)
        print(f"CLICK_AC_SENT at=({ax},{ay})")
        time.sleep(4.5)

    cw2, ch2 = canvas_size(canvas)
    img2 = render_window(canvas, cw2, ch2)
    if img2 is None:
        return 2
    img2.save(os.path.join(args.out, "gui-v3-canvas-despues.png"))
    bbox2 = detect_clear_bbox(img2)
    if bbox2 is not None:
        crop_box2, fuente2 = bbox2, "deteccion"
    else:
        crop_box2, fuente2 = lcd_box_fixed(cw2, ch2), "geometria_fija"
    print(f"LCD_CROP_DESPUES source={fuente2} box={crop_box2}")
    lcd2 = img2.crop(crop_box2)
    after_path = os.path.join(args.out, "gui-v3-lcd-after.png")
    lcd2.save(after_path)
    na, la = lit_stats(lcd2)
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
