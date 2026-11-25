#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_gui.py — Verificación consolidada de la GUI real de Calculadora Unificada (JSON).

Implementa el ítem 34 (Fase 5.1):
1. Localiza la ventana principal (clase CasioNativeMain) con canvas hijo (CasioCanvasArea).
   Si no está en ejecución, lanza build-verify\\Release\\CalculadoraUnificada.exe y espera
   a que el backend esté listo (CG50: READY en calc_debug.log).
2. Maneja restauración si la ventana está minimizada (SW_RESTORE).
3. Conmuta a la vista Solo-CG50 (WM_COMMAND 1014).
4. Captura el canvas mediante PrintWindow.
5. Recorta el LCD mediante detección de la región clara (>=236, x>900, >=300 px/fila)
   con fallback a geometría fija.
6. Genera versión ampliada 3x (LANCZOS) y, opcionalmente (--ocr), ejecuta el OCR de Windows
   mediante tools\\ocr_png.ps1.
7. Emite un informe estructurado JSON por stdout y lo guarda en <out>\\capture-<timestamp>.json.

Uso:
    python tools/verify_gui.py --out <directorio> [--ocr] [--no-click]
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import os
import subprocess
import sys
import time

from PIL import Image, ImageChops

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

user32.SendMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.SendMessageW.restype = wt.LPARAM
user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, wt.INT]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.IsIconic.argtypes = [wt.HWND]
user32.IsIconic.restype = wt.BOOL
user32.ShowWindowAsync.argtypes = [wt.HWND, ctypes.c_int]
user32.ShowWindowAsync.restype = wt.BOOL
user32.ShowWindow.argtypes = [wt.HWND, ctypes.c_int]
user32.ShowWindow.restype = wt.BOOL
user32.SetForegroundWindow.argtypes = [wt.HWND]
user32.SetForegroundWindow.restype = wt.BOOL
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetWindowThreadProcessId.restype = wt.DWORD
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
gdi32.GetDIBits.argtypes = [wt.HDC, wt.HBITMAP, wt.UINT, wt.UINT, wt.LPVOID, wt.LPVOID, wt.UINT]
gdi32.GetDIBits.restype = wt.INT
user32.GetDC.argtypes = [wt.HWND]
user32.GetDC.restype = wt.HDC
user32.ReleaseDC.argtypes = [wt.HWND, wt.HDC]
user32.ReleaseDC.restype = wt.INT

WM_COMMAND = 0x0111
ID_BTN_MODE_GRAPH = 1014
PW_CLIENTONLY = 1
BI_RGB = 0
DIB_RGB_COLORS = 0
SW_RESTORE = 9

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"
CG50_SKIN_W, CG50_SKIN_H = 664, 1364
CG50_LCD_RECT = (92, 153, 572, 403)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_RELEASE_DIR = os.path.join(PROJECT_ROOT, "build-verify", "Release")
DEFAULT_EXE = os.path.join(DEFAULT_RELEASE_DIR, "CalculadoraUnificada.exe")
DEFAULT_LOG = os.path.join(DEFAULT_RELEASE_DIR, "calc_debug.log")
OCR_PS1 = os.path.join(PROJECT_ROOT, "tools", "ocr_png.ps1")


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wt.UINT), ("biWidth", wt.LONG),
        ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
        ("biBitCount", wt.WORD), ("biCompression", wt.UINT),
        ("biSizeImage", wt.UINT), ("biXPelsPerMeter", wt.LONG),
        ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.UINT),
        ("biClrImportant", wt.UINT)
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.UINT * 3)]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().upper()


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
    got = gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), DIB_RGB_COLORS)
    user32.ReleaseDC(hwnd, hdc_window)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    if not ok or got != h:
        return None
    return Image.frombytes("RGB", (w, h), buf.raw, "raw", "BGRX", 0, 1)


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


def lcd_box_fixed(total_w, total_h):
    vx, vy, tw, th = solo_layout(total_w, total_h)
    lx0 = CG50_LCD_RECT[0] * tw // CG50_SKIN_W
    ly0 = CG50_LCD_RECT[1] * th // CG50_SKIN_H
    lx1 = CG50_LCD_RECT[2] * tw // CG50_SKIN_W
    ly1 = CG50_LCD_RECT[3] * th // CG50_SKIN_H
    return (vx + lx0, vy + ly0, vx + lx1, vy + ly1)


def detect_clear_bbox(img, thr=236, x_min=900, min_row_px=300, min_h=60):
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


def count_ready_in_log(log_path):
    if not os.path.isfile(log_path):
        return 0
    try:
        with open(log_path, "rb") as f:
            return f.read().count(b"CG50: READY")
    except Exception:
        return 0


def run_ocr(png3x_path):
    if not os.path.isfile(OCR_PS1):
        return {"error": "ocr_script_missing", "path": OCR_PS1}
    cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", OCR_PS1, "-PngPath", png3x_path]
    try:
        proc = subprocess.run(cmd, capture_output=True, timeout=30)
        raw = proc.stdout.decode("utf-8", errors="replace")
        lines = [line.strip() for line in raw.splitlines() if line.strip()]
        text = "\n".join(lines)
        return {
            "exit_code": proc.returncode,
            "raw_output": text,
            "contains_main_menu": "main menu" in text.lower()
        }
    except Exception as e:
        return {"error": str(e)}


def main():
    parser = argparse.ArgumentParser(description="Verificación GUI consolidada de Calculadora Unificada")
    parser.add_argument("--out", required=True, help="Directorio donde guardar artefactos")
    parser.add_argument("--no-click", action="store_true", help="Compatibilidad: verify_gui nunca envía clics")
    parser.add_argument("--ocr", action="store_true", help="Ejecutar OCR nativo sobre el recorte LCD 3x")
    parser.add_argument("--exe", default=DEFAULT_EXE, help="Ruta al ejecutable")
    parser.add_argument("--log", default=None, help="Ruta a calc_debug.log")
    args = parser.parse_args()
    if not args.log:
        args.log = os.path.join(os.path.dirname(args.exe), "calc_debug.log")

    os.makedirs(args.out, exist_ok=True)
    ts = time.strftime("%Y%m%d-%H%M%S")

    # 1. Localizar o lanzar app
    launched = False
    hwnds = find_visible_windows(MAIN_CLASS)
    if not hwnds:
        if not os.path.isfile(args.exe):
            print(f"ERROR: No se encontró el ejecutable en {args.exe}", file=sys.stderr)
            sys.exit(1)
        n0 = count_ready_in_log(args.log)
        subprocess.Popen([args.exe], cwd=os.path.dirname(args.exe))
        launched = True
        for _ in range(30):
            time.sleep(0.5)
            hwnds = find_visible_windows(MAIN_CLASS)
            if hwnds:
                break
        if not hwnds:
            print("ERROR: Timeout esperando ventana principal CasioNativeMain", file=sys.stderr)
            sys.exit(1)
        # Esperar a que el log registre READY
        for _ in range(60):
            time.sleep(0.5)
            if count_ready_in_log(args.log) > n0:
                break

    hwnd = hwnds[0]
    pid = wt.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))

    # Restaurar si está minimizada o no visible
    restored = False
    if user32.IsIconic(hwnd) or not user32.IsWindowVisible(hwnd):
        user32.ShowWindow(hwnd, SW_RESTORE)
        user32.SetForegroundWindow(hwnd)
        time.sleep(1.0)
        restored = not user32.IsIconic(hwnd)
    else:
        user32.ShowWindow(hwnd, SW_RESTORE)
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.5)

    canvas = find_child(hwnd, CANVAS_CLASS)
    if not canvas:
        print("ERROR: No se encontró canvas hijo CasioCanvasArea", file=sys.stderr)
        sys.exit(1)

    # Conmutar a vista solo-CG50
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.5)

    cw, ch = canvas_size(canvas)
    if cw <= 0 or ch <= 0:
        print(f"ERROR: Dimensiones de canvas inválidas: {cw}x{ch}", file=sys.stderr)
        sys.exit(2)

    # Sondeo hasta que el guest OS termine de arrancar y renderice el MAIN MENU (hasta 20 s)
    img = None
    bbox = None
    source = "geometria_fija"
    lcd_crop = None
    colorful_px = 0
    lit_px = 0
    total_px = 0

    deadline = time.time() + 20.0
    while time.time() < deadline:
        img = render_window(canvas, cw, ch)
        if img is not None:
            detected = detect_clear_bbox(img)
            if detected is not None:
                crop_temp = img.crop(detected)
                pixels_temp = list(crop_temp.getdata())
                c_count = sum(1 for p in pixels_temp if (max(p) - min(p)) > 40)
                if c_count >= 20000:
                    bbox = detected
                    source = "deteccion"
                    lcd_crop = crop_temp
                    colorful_px = c_count
                    lit_px = sum(1 for p in pixels_temp if max(p) > 24)
                    total_px = len(pixels_temp)
                    break
        time.sleep(1.0)

    if img is None:
        print("ERROR: Fallo al capturar ventana via PrintWindow", file=sys.stderr)
        sys.exit(2)

    if lcd_crop is None:
        if bbox is None:
            bbox = detect_clear_bbox(img)
        if bbox is not None:
            source = "deteccion"
        else:
            bbox = lcd_box_fixed(cw, ch)
            source = "geometria_fija"
        lcd_crop = img.crop(bbox)
        pixels = list(lcd_crop.getdata())
        total_px = len(pixels)
        lit_px = sum(1 for p in pixels if max(p) > 24)
        colorful_px = sum(1 for p in pixels if (max(p) - min(p)) > 40)

    canvas_png = os.path.join(args.out, "gui-canvas.png")
    img.save(canvas_png)

    crop_png = os.path.join(args.out, "gui-lcd-crop.png")
    lcd_crop.save(crop_png)

    crop3x = lcd_crop.resize((lcd_crop.width * 3, lcd_crop.height * 3), Image.LANCZOS)
    crop3x_png = os.path.join(args.out, "gui-lcd-crop-3x.png")
    crop3x.save(crop3x_png)

    ocr_result = None
    if args.ocr:
        ocr_result = run_ocr(crop3x_png)

    result_data = {
        "herramienta": "tools/verify_gui.py",
        "timestamp": ts,
        "app": {
            "exe": args.exe,
            "exe_sha256": sha256_file(args.exe) if os.path.isfile(args.exe) else None,
            "pid": pid.value,
            "hwnd": f"0x{hwnd:X}",
            "lanzada_por_script": launched,
            "restaurada_de_minimizada": restored,
        },
        "captura": {
            "canvas_w": cw,
            "canvas_h": ch,
            "canvas_sha256": sha256_file(canvas_png),
            "crop_bbox": list(bbox),
            "crop_source": source,
            "crop_w": lcd_crop.width,
            "crop_h": lcd_crop.height,
            "crop_sha256": sha256_file(crop_png),
            "crop_3x_sha256": sha256_file(crop3x_png),
            "total_pixels": total_px,
            "lit_pixels": lit_px,
            "lit_percent": round(100.0 * lit_px / max(1, total_px), 2),
            "colorful_pixels": colorful_px,
            "colorful_percent": round(100.0 * colorful_px / max(1, total_px), 2),
        },
        "ocr": ocr_result,
        "status": "PASS" if lit_px > 0.05 * total_px else "FAIL_EMPTY"
    }

    json_path = os.path.join(args.out, f"capture-{ts}.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result_data, f, indent=2, ensure_ascii=False)

    print(json.dumps(result_data, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
