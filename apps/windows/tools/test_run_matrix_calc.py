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
WM_CLOSE = 0x0010
PW_CLIENTONLY = 1

ID_BTN_MODE_ALL = 1010
ID_BTN_MODE_GRAPH = 1014

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"

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
        _fields_ = [("biSize", wt.UINT), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.UINT),
                    ("biSizeImage", wt.UINT), ("biXPelsPerMeter", wt.LONG),
                    ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.UINT), ("biClrImportant", wt.UINT)]
    class BITMAPINFO(ctypes.Structure):
        _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.UINT * 3)]
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

def send_key(target_hwnd, vk, hold_s=0.15, wait_s=0.25):
    user32.SendMessageW(target_hwnd, WM_KEYDOWN, vk, 0)
    time.sleep(hold_s)
    user32.SendMessageW(target_hwnd, WM_KEYUP, vk, 0)
    time.sleep(wait_s)

def crop_lcd_from_solo(img):
    # In solo CG50 mode (w ~ 2004, h ~ 1330)
    w, h = img.size
    # Let's locate the LCD exactly by color:
    # In solo mode, the skin is centered horizontally or placed in target rect.
    # From earlier measurements:
    # bounds of calc: x=[1028, 1978], y=[20, 1328], w=950, h=1308
    # LCD in skin: { 92, 153, 572, 403 }
    # Or let's scan for the LCD border/content
    # Let's use the verified offsets:
    scaleX = 950.0 / 664.0
    scaleY = 1308.0 / 1364.0
    x1 = int(1028 + 92 * scaleX)
    y1 = int(20 + 153 * scaleY)
    x2 = int(1028 + 572 * scaleX)
    y2 = int(20 + 403 * scaleY)
    return img.crop((x1, y1, x2, y2))

def main():
    root = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial"
    exe = os.path.join(root, "build-win32", "Release", "CalculadoraUnificada.exe")
    outdir = os.path.join(root, "verification", "run_matrix_test")
    os.makedirs(outdir, exist_ok=True)

    print(f"1. Lanzando: {exe}")
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))

    hwnd = None
    for _ in range(30):
        time.sleep(0.5)
        hwnd = find_window()
        if hwnd: break

    if not hwnd:
        print("ERROR: Ventana principal no encontrada")
        proc.kill()
        return 1

    canvas = find_canvas(hwnd)
    print(f"Ventana: {hex(hwnd)}, Canvas: {hex(canvas) if canvas else 'None'}")
    target = canvas if canvas else hwnd

    # Esperar a que el emulador y GCal estén listos
    print("Esperando arranque completo de GCal (6 segundos)...")
    time.sleep(6.0)

    # 1. Cambiar a modo Solo CG50
    print("Cambiando a modo Solo CG50 (1014)...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.0)

    p_menu = os.path.join(outdir, "01_main_menu.png")
    p_menu_lcd = os.path.join(outdir, "01_main_menu_lcd.png")
    im_menu = capture(canvas, p_menu)
    if im_menu:
        crop_lcd_from_solo(im_menu).save(p_menu_lcd)
    print("Captura 1 (Main Menu) realizada.")

    # 2. Entrar a Run-Matrix pulsando '1' o 'EXE'
    print("Enviando tecla '1' (0x31) para abrir Run-Matrix...")
    send_key(target, 0x31, hold_s=0.15, wait_s=0.3)

    # Esperar 2.0 s a que la app cargue
    print("Esperando 2.0 s a que Run-Matrix cargue...")
    time.sleep(2.0)

    p_runmat = os.path.join(outdir, "02_run_matrix_ready.png")
    p_runmat_lcd = os.path.join(outdir, "02_run_matrix_ready_lcd.png")
    im_runmat = capture(canvas, p_runmat)
    if im_runmat:
        crop_lcd_from_solo(im_runmat).save(p_runmat_lcd)
    print("Captura 2 (Run-Matrix listo) realizada.")

    # 3. Enviar cálculo: 1 + 2 EXE
    # '1' = 0x31
    # '+' = 0x6B (VK_ADD)
    # '2' = 0x32
    # 'EXE' = 0x0D (VK_RETURN)
    print("Inyectando calculo: 1 + 2 EXE...")
    send_key(target, 0x31, hold_s=0.15, wait_s=0.25)
    send_key(target, 0x6B, hold_s=0.15, wait_s=0.25)
    send_key(target, 0x32, hold_s=0.15, wait_s=0.25)
    send_key(target, 0x0D, hold_s=0.15, wait_s=0.50)

    p_calc = os.path.join(outdir, "03_calc_1_plus_2.png")
    p_calc_lcd = os.path.join(outdir, "03_calc_1_plus_2_lcd.png")
    im_calc = capture(canvas, p_calc)
    if im_calc:
        crop_lcd_from_solo(im_calc).save(p_calc_lcd)
    print("Captura 3 (1+2=3) realizada.")

    # 4. Enviar segundo cálculo: 12 * 34 EXE
    # '1' = 0x31, '2' = 0x32
    # '*' = 0x6A (VK_MULTIPLY)
    # '3' = 0x33, '4' = 0x34
    # 'EXE' = 0x0D (VK_RETURN)
    print("Inyectando calculo: 12 * 34 EXE...")
    send_key(target, 0x31, hold_s=0.15, wait_s=0.20)
    send_key(target, 0x32, hold_s=0.15, wait_s=0.20)
    send_key(target, 0x6A, hold_s=0.15, wait_s=0.20)
    send_key(target, 0x33, hold_s=0.15, wait_s=0.20)
    send_key(target, 0x34, hold_s=0.15, wait_s=0.20)
    send_key(target, 0x0D, hold_s=0.15, wait_s=0.50)

    p_calc2 = os.path.join(outdir, "04_calc_12_x_34.png")
    p_calc2_lcd = os.path.join(outdir, "04_calc_12_x_34_lcd.png")
    im_calc2 = capture(canvas, p_calc2)
    if im_calc2:
        crop_lcd_from_solo(im_calc2).save(p_calc2_lcd)
    print("Captura 4 (12*34=408) realizada.")

    # 5. Volver al Main Menu pulsando MENU (VK_HOME = 0x24)
    print("Regresando al Main Menu con MENU (VK_HOME)...")
    send_key(target, 0x24, hold_s=0.15, wait_s=1.0)

    p_back_menu = os.path.join(outdir, "05_back_to_menu.png")
    p_back_menu_lcd = os.path.join(outdir, "05_back_to_menu_lcd.png")
    im_back_menu = capture(canvas, p_back_menu)
    if im_back_menu:
        crop_lcd_from_solo(im_back_menu).save(p_back_menu_lcd)
    print("Captura 5 (Retorno al Menu) realizada.")

    # 6. Cierre limpio
    print("Cerrando proceso limpiamente con WM_CLOSE...")
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    time.sleep(1.0)
    if proc.poll() is None:
        proc.wait(timeout=3)

    print("Proceso terminado. Evaluando capturas...")
    return 0

if __name__ == "__main__":
    sys.exit(main())
