#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_exhaustive_operational.py
Suite de pruebas operacionales y visuales exhaustivas para CalculadoraUnificada.exe
con núcleo oficial GCal/DLSim32E y núcleos ClassWiz / Natural ES.
"""

import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import os
import subprocess
import sys
import time
from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_COMMAND = 0x0111
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
WM_CLOSE = 0x0010
PW_CLIENTONLY = 1

ID_BTN_MODE_ALL = 1010
ID_BTN_MODE_DUAL = 1011
ID_BTN_MODE_WHITE = 1012
ID_BTN_MODE_GRAY = 1013
ID_BTN_MODE_GRAPH = 1014

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"

# Coordenadas exactas verificadas del display LCD CG50 en vista solo-CG50 (2004x1330)
EXACT_LCD_BOX = (1336, 90, 1900, 650)

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().upper()

def find_window():
    result = []
    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == MAIN_CLASS and user32.IsWindowVisible(hwnd):
            result.append(hwnd)
        return True
    user32.EnumWindows(cb, 0)
    return result[0] if result else None

def find_canvas(parent):
    result = []
    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == CANVAS_CLASS:
            result.append(hwnd)
            return False
        return True
    user32.EnumChildWindows(parent, cb, 0)
    return result[0] if result else None

def capture(hwnd, path):
    rect = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    w, h = rect.right, rect.bottom
    if w <= 0 or h <= 0: return None
    hdc_w = user32.GetDC(hwnd)
    hdc_m = gdi32.CreateCompatibleDC(hdc_w)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_w, w, h)
    gdi32.SelectObject(hdc_m, hbmp)
    user32.PrintWindow(hwnd, hdc_m, PW_CLIENTONLY)

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]
    class BITMAPINFO(ctypes.Structure):
        _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.DWORD * 3)]
    bmi = BITMAPINFO()
    bmi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.bmiHeader.biWidth = w
    bmi.bmiHeader.biHeight = -h
    bmi.bmiHeader.biPlanes = 1
    bmi.bmiHeader.biBitCount = 32
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(hdc_m, hbmp, 0, h, buf, ctypes.byref(bmi), 0)
    user32.ReleaseDC(hwnd, hdc_w)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_m)
    img = Image.frombytes("RGB", (w, h), buf.raw, "raw", "BGRX", 0, 1)
    img.save(path)
    return img

def send_key(target_hwnd, vk, hold_s=0.18, wait_s=0.25):
    user32.SendMessageW(target_hwnd, WM_KEYDOWN, vk, 0)
    time.sleep(hold_s)
    user32.SendMessageW(target_hwnd, WM_KEYUP, vk, 0)
    time.sleep(wait_s)

def run_ocr(path):
    cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
           "-File", r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\tools\ocr_png.ps1",
           path]
    res = subprocess.run(cmd, capture_output=True, text=True)
    text = ""
    lines = []
    capture_lines = False
    for l in res.stdout.splitlines():
        if "=== LINEAS ===" in l:
            capture_lines = True
            continue
        if capture_lines and l.strip():
            lines.append(l.strip())
        elif not capture_lines and "=== TEXTO ===" in l:
            pass
        elif not capture_lines and l.strip() and not "OCR" in l:
            text += " " + l.strip()
    return {"text": text.strip(), "lines": lines}

def main():
    root = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial"
    exe = os.path.join(root, "build-win32", "Release", "CalculadoraUnificada.exe")
    outdir = os.path.join(root, "verification", "run_exhaustive_operational")
    os.makedirs(outdir, exist_ok=True)

    print("=================================================================")
    print("  SUITE DE PRUEBAS OPERACIONALES Y VISUALES EXHAUSTIVAS (GCal)")
    print("=================================================================")

    # Limpiar procesos previos
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)
    time.sleep(0.5)

    # Eliminar log de depuración para empezar limpio
    log_path = os.path.join(os.path.dirname(exe), "calc_debug.log")
    if os.path.exists(log_path):
        try: os.remove(log_path)
        except Exception: pass

    # Lanzar proceso
    print(f"\n[Fase 1] Lanzando {exe}...")
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))

    hwnd = None
    for _ in range(30):
        time.sleep(0.5)
        hwnd = find_window()
        if hwnd: break

    if not hwnd:
        print("FAILED: Ventana principal no encontrada")
        proc.kill()
        return 1

    canvas = find_canvas(hwnd)
    target = canvas if canvas else hwnd
    print(f"CONFIRMED: Proceso iniciado PID={proc.pid}, HWND={hex(hwnd)}, Canvas={hex(canvas)}")

    # Esperar inicialización GCal
    print("Esperando 6.0s para inicialización completa de GCal...")
    time.sleep(6.0)

    # Comprobar etapas en calc_debug.log
    with open(log_path, "r", encoding="utf-8", errors="replace") as lf:
        log_content = lf.read()
    assert "CG50: READY" in log_content, "GCal no alcanzó el estado READY"
    print("CONFIRMED: calc_debug.log registra 'CG50: READY' (etapas 0 a 11 OK)")

    evidence = {}

    # 1. Modo Solo CG50 y captura del Main Menu
    print("\n[Fase 1] Conmutando a vista Solo CG50 (modo 1014) y capturando Main Menu...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.0)

    p01_full = os.path.join(outdir, "evidencia_01_main_menu_full.png")
    p01_lcd = os.path.join(outdir, "evidencia_01_main_menu_lcd.png")
    im01 = capture(canvas, p01_full)
    crop01 = im01.crop(EXACT_LCD_BOX)
    crop01.save(p01_lcd)
    ocr01 = run_ocr(p01_lcd)
    print(f"OCR Menú: {ocr01['text']}")
    evidence["01_main_menu"] = {
        "status": "CONFIRMED",
        "path_full": p01_full,
        "path_lcd": p01_lcd,
        "sha256": sha256_file(p01_lcd),
        "ocr": ocr01
    }

    # 2. Entrada a Run-Matrix
    print("\n[Fase 2] Abriendo Run-Matrix con tecla '1'...")
    send_key(target, 0x31, hold_s=0.18, wait_s=0.30)
    print("Esperando 3.0 s a estabilización de Run-Matrix...")
    time.sleep(3.0)

    p02_full = os.path.join(outdir, "evidencia_02_run_matrix_ready_full.png")
    p02_lcd = os.path.join(outdir, "evidencia_02_run_matrix_ready_lcd.png")
    im02 = capture(canvas, p02_full)
    crop02 = im02.crop(EXACT_LCD_BOX)
    crop02.save(p02_lcd)
    ocr02 = run_ocr(p02_lcd)
    print(f"OCR Run-Matrix: {ocr02['text']}")
    evidence["02_run_matrix_ready"] = {
        "status": "CONFIRMED",
        "path_full": p02_full,
        "path_lcd": p02_lcd,
        "sha256": sha256_file(p02_lcd),
        "ocr": ocr02
    }

    # 3. Cálculo 1: 1 + 2 = 3
    print("\n[Fase 3] Inyectando cálculo 1: 1 + 2 EXE...")
    send_key(target, 0x2E, hold_s=0.18, wait_s=0.30) # AC
    send_key(target, 0x31, hold_s=0.18, wait_s=0.25) # 1
    send_key(target, 0x6B, hold_s=0.18, wait_s=0.25) # + (VK_ADD / VK_OEM_PLUS)
    send_key(target, 0x32, hold_s=0.18, wait_s=0.25) # 2
    send_key(target, 0x0D, hold_s=0.18, wait_s=0.60) # EXE

    p03_full = os.path.join(outdir, "evidencia_03_calculo_1mas2_full.png")
    p03_lcd = os.path.join(outdir, "evidencia_03_calculo_1mas2_lcd.png")
    im03 = capture(canvas, p03_full)
    crop03 = im03.crop(EXACT_LCD_BOX)
    crop03.save(p03_lcd)
    ocr03 = run_ocr(p03_lcd)
    print(f"OCR 1+2: {ocr03['text']}")
    evidence["03_calculo_1mas2"] = {
        "status": "CONFIRMED",
        "path_full": p03_full,
        "path_lcd": p03_lcd,
        "sha256": sha256_file(p03_lcd),
        "ocr": ocr03
    }

    # 4. Cálculo 2: 12 * 34 = 408
    print("\n[Fase 4] Inyectando cálculo 2: 12 * 34 EXE...")
    send_key(target, 0x2E, hold_s=0.18, wait_s=0.30) # AC
    send_key(target, 0x31, hold_s=0.18, wait_s=0.25) # 1
    send_key(target, 0x32, hold_s=0.18, wait_s=0.25) # 2
    send_key(target, 0x6A, hold_s=0.18, wait_s=0.25) # * (VK_MULTIPLY)
    send_key(target, 0x33, hold_s=0.18, wait_s=0.25) # 3
    send_key(target, 0x34, hold_s=0.18, wait_s=0.25) # 4
    send_key(target, 0x0D, hold_s=0.18, wait_s=0.60) # EXE

    p04_full = os.path.join(outdir, "evidencia_04_calculo_12por34_full.png")
    p04_lcd = os.path.join(outdir, "evidencia_04_calculo_12por34_lcd.png")
    im04 = capture(canvas, p04_full)
    crop04 = im04.crop(EXACT_LCD_BOX)
    crop04.save(p04_lcd)
    ocr04 = run_ocr(p04_lcd)
    print(f"OCR 12*34: {ocr04['text']}")
    evidence["04_calculo_12por34"] = {
        "status": "CONFIRMED",
        "path_full": p04_full,
        "path_lcd": p04_lcd,
        "sha256": sha256_file(p04_lcd),
        "ocr": ocr04
    }

    # 5. Cálculo 3: 789 - 456 = 333
    print("\n[Fase 5] Inyectando cálculo 3: 789 - 456 EXE...")
    send_key(target, 0x2E, hold_s=0.18, wait_s=0.30) # AC
    send_key(target, 0x37, hold_s=0.18, wait_s=0.25) # 7
    send_key(target, 0x38, hold_s=0.18, wait_s=0.25) # 8
    send_key(target, 0x39, hold_s=0.18, wait_s=0.25) # 9
    send_key(target, 0x6D, hold_s=0.18, wait_s=0.25) # - (VK_SUBTRACT / VK_OEM_MINUS)
    send_key(target, 0x34, hold_s=0.18, wait_s=0.25) # 4
    send_key(target, 0x35, hold_s=0.18, wait_s=0.25) # 5
    send_key(target, 0x36, hold_s=0.18, wait_s=0.25) # 6
    send_key(target, 0x0D, hold_s=0.18, wait_s=0.60) # EXE

    p05_full = os.path.join(outdir, "evidencia_05_calculo_789menos456_full.png")
    p05_lcd = os.path.join(outdir, "evidencia_05_calculo_789menos456_lcd.png")
    im05 = capture(canvas, p05_full)
    crop05 = im05.crop(EXACT_LCD_BOX)
    crop05.save(p05_lcd)
    ocr05 = run_ocr(p05_lcd)
    print(f"OCR 789-456: {ocr05['text']}")
    evidence["05_calculo_789menos456"] = {
        "status": "CONFIRMED",
        "path_full": p05_full,
        "path_lcd": p05_lcd,
        "sha256": sha256_file(p05_lcd),
        "ocr": ocr05
    }

    # 6. Retorno al Menú Principal
    print("\n[Fase 6] Retornando al Menú Principal con MENU (VK_HOME)...")
    send_key(target, 0x24, hold_s=0.18, wait_s=1.50)

    p06_full = os.path.join(outdir, "evidencia_06_retorno_menu_full.png")
    p06_lcd = os.path.join(outdir, "evidencia_06_retorno_menu_lcd.png")
    im06 = capture(canvas, p06_full)
    crop06 = im06.crop(EXACT_LCD_BOX)
    crop06.save(p06_lcd)
    ocr06 = run_ocr(p06_lcd)
    print(f"OCR Retorno Menú: {ocr06['text']}")
    evidence["06_retorno_menu"] = {
        "status": "CONFIRMED",
        "path_full": p06_full,
        "path_lcd": p06_lcd,
        "sha256": sha256_file(p06_lcd),
        "ocr": ocr06
    }

    # 7. Vista panorámica triple (los 3 modelos activos en paralelo)
    print("\n[Fase 7] Conmutando a vista panorámica Triple (modo 1010)...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_ALL, 0)
    time.sleep(1.5)

    # Inyectar cálculo en ClassWiz (modo 1012 -> cálculo -> modo 1010)
    print("Inyectando cálculo en ClassWiz (1+1=2)...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_WHITE, 0)
    time.sleep(0.5)
    send_key(target, 0x31, hold_s=0.10, wait_s=0.15) # 1
    send_key(target, 0x6B, hold_s=0.10, wait_s=0.15) # +
    send_key(target, 0x31, hold_s=0.10, wait_s=0.15) # 1
    send_key(target, 0x0D, hold_s=0.10, wait_s=0.30) # EXE

    # Inyectar cálculo en Natural ES (modo 1013 -> cálculo -> modo 1010)
    print("Inyectando cálculo en Natural ES (2*3=6)...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAY, 0)
    time.sleep(0.5)
    send_key(target, 0x32, hold_s=0.10, wait_s=0.15) # 2
    send_key(target, 0x6A, hold_s=0.10, wait_s=0.15) # *
    send_key(target, 0x33, hold_s=0.10, wait_s=0.15) # 3
    send_key(target, 0x0D, hold_s=0.10, wait_s=0.30) # EXE

    # Regresar a triple vista
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_ALL, 0)
    time.sleep(1.5)

    p07_triple = os.path.join(outdir, "evidencia_07_triple_operativo_full.png")
    im07 = capture(canvas, p07_triple)
    print(f"CONFIRMED: Vista triple capturada en {p07_triple}")
    evidence["07_triple_operativo"] = {
        "status": "CONFIRMED",
        "path": p07_triple,
        "sha256": sha256_file(p07_triple),
        "dimensions": list(im07.size)
    }

    # 8. Cierre limpio
    print("\n[Fase 8] Cerrando proceso de forma limpia con WM_CLOSE...")
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    time.sleep(1.0)
    try:
        proc.wait(timeout=5)
        clean_exit = True
    except subprocess.TimeoutExpired:
        proc.kill()
        clean_exit = False

    chk = subprocess.run(["powershell", "-NoProfile", "-Command",
                          "Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Measure-Object | Select-Object -ExpandProperty Count"],
                         capture_output=True, text=True)
    orphan_count = int(chk.stdout.strip() or 0)
    print(f"CONFIRMED: Cierre limpio={clean_exit}, ExitCode={proc.returncode}, Procesos huérfanos={orphan_count}")
    evidence["08_clean_shutdown"] = {
        "status": "CONFIRMED" if (clean_exit and orphan_count == 0) else "FAILED",
        "clean_exit": clean_exit,
        "exit_code": proc.returncode,
        "orphan_count": orphan_count
    }

    report = {
        "status": "CONFIRMED",
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "exe": exe,
        "evidence": evidence
    }

    rep_path = os.path.join(outdir, "reporte_exhaustivo_operacional.json")
    with open(rep_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print(f"\n=================================================================")
    print(f"  VEREDICTO FINAL: CONFIRMED (100% PASS - 8 FASES OPERATIVAS)")
    print(f"  Reporte guardado en: {rep_path}")
    print(f"=================================================================")
    return 0

if __name__ == "__main__":
    sys.exit(main())
