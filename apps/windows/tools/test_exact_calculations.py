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
    scaleX = 950.0 / 664.0
    scaleY = 1308.0 / 1364.0
    x1 = int(1028 + 92 * scaleX)
    y1 = int(20 + 153 * scaleY)
    x2 = int(1028 + 572 * scaleX)
    y2 = int(20 + 403 * scaleY)
    return img.crop((x1, y1, x2, y2))

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().upper()

def main():
    root = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial"
    exe = os.path.join(root, "build-win32", "Release", "CalculadoraUnificada.exe")
    outdir = os.path.join(root, "verification", "run_exact_calculations")
    os.makedirs(outdir, exist_ok=True)

    # Matar instancias previas si las hay
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)
    time.sleep(0.5)

    print(f"Lanzando {exe}...")
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
    target = canvas if canvas else hwnd

    print("Esperando arranque completo de GCal (6.0 s)...")
    time.sleep(6.0)

    # 1. Modo Solo CG50
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.0)

    # 2. Abrir Run-Matrix
    print("Abriendo Run-Matrix con tecla '1'...")
    send_key(target, 0x31, hold_s=0.18, wait_s=0.30)
    print("Esperando estabilizacion de Run-Matrix (3.0 s)...")
    time.sleep(3.0)

    # Limpiar pantalla con AC (VK_DELETE = 0x2E)
    print("Enviando AC para asegurar linea limpia...")
    send_key(target, 0x2E, hold_s=0.18, wait_s=0.40)

    # 3. Calculo 1: 1 + 2 EXE
    print("Calculo 1: 1 + 2 EXE...")
    send_key(target, 0x31, hold_s=0.18, wait_s=0.30) # 1
    send_key(target, 0x6B, hold_s=0.18, wait_s=0.30) # +
    send_key(target, 0x32, hold_s=0.18, wait_s=0.30) # 2
    send_key(target, 0x0D, hold_s=0.18, wait_s=0.60) # EXE

    p1 = os.path.join(outdir, "calc01_1mas2.png")
    p1_lcd = os.path.join(outdir, "calc01_1mas2_lcd.png")
    im1 = capture(canvas, p1)
    if im1: crop_lcd_from_solo(im1).save(p1_lcd)
    print(f"Captura 1 guardada: {p1_lcd}")

    # 4. Calculo 2: 12 * 34 EXE
    print("Calculo 2: 12 * 34 EXE...")
    send_key(target, 0x2E, hold_s=0.18, wait_s=0.40) # AC
    send_key(target, 0x31, hold_s=0.18, wait_s=0.25) # 1
    send_key(target, 0x32, hold_s=0.18, wait_s=0.25) # 2
    send_key(target, 0x6A, hold_s=0.18, wait_s=0.25) # *
    send_key(target, 0x33, hold_s=0.18, wait_s=0.25) # 3
    send_key(target, 0x34, hold_s=0.18, wait_s=0.25) # 4
    send_key(target, 0x0D, hold_s=0.18, wait_s=0.60) # EXE

    p2 = os.path.join(outdir, "calc02_12por34.png")
    p2_lcd = os.path.join(outdir, "calc02_12por34_lcd.png")
    im2 = capture(canvas, p2)
    if im2: crop_lcd_from_solo(im2).save(p2_lcd)
    print(f"Captura 2 guardada: {p2_lcd}")

    # 5. Calculo 3: 789 - 456 EXE
    print("Calculo 3: 789 - 456 EXE...")
    send_key(target, 0x2E, hold_s=0.18, wait_s=0.40) # AC
    send_key(target, 0x37, hold_s=0.18, wait_s=0.25) # 7
    send_key(target, 0x38, hold_s=0.18, wait_s=0.25) # 8
    send_key(target, 0x39, hold_s=0.18, wait_s=0.25) # 9
    send_key(target, 0x6D, hold_s=0.18, wait_s=0.25) # - (VK_SUBTRACT)
    send_key(target, 0x34, hold_s=0.18, wait_s=0.25) # 4
    send_key(target, 0x35, hold_s=0.18, wait_s=0.25) # 5
    send_key(target, 0x36, hold_s=0.18, wait_s=0.25) # 6
    send_key(target, 0x0D, hold_s=0.18, wait_s=0.60) # EXE

    p3 = os.path.join(outdir, "calc03_789menos456.png")
    p3_lcd = os.path.join(outdir, "calc03_789menos456_lcd.png")
    im3 = capture(canvas, p3)
    if im3: crop_lcd_from_solo(im3).save(p3_lcd)
    print(f"Captura 3 guardada: {p3_lcd}")

    # 6. Retorno al Menu Principal
    print("Regresando al Menu Principal con MENU (VK_HOME = 0x24)...")
    send_key(target, 0x24, hold_s=0.18, wait_s=1.50)
    p_menu = os.path.join(outdir, "nav_main_menu.png")
    p_menu_lcd = os.path.join(outdir, "nav_main_menu_lcd.png")
    im_menu = capture(canvas, p_menu)
    if im_menu: crop_lcd_from_solo(im_menu).save(p_menu_lcd)
    print(f"Captura Menu guardada: {p_menu_lcd}")

    # 7. Cierre limpio
    print("Cerrando proceso con WM_CLOSE...")
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    time.sleep(1.0)
    if proc.poll() is None:
        proc.wait(timeout=3)

    # 8. Reporte y OCR
    results = {}
    for name, p in [("1mas2", p1_lcd), ("12por34", p2_lcd), ("789menos456", p3_lcd), ("main_menu", p_menu_lcd)]:
        if os.path.exists(p):
            results[name] = {
                "path": p,
                "sha256": sha256_file(p),
                "size_bytes": os.path.getsize(p)
            }

    rep_path = os.path.join(outdir, "reporte_calculos_exactos.json")
    with open(rep_path, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)

    print("\nResultados:")
    print(json.dumps(results, indent=2))
    return 0

if __name__ == "__main__":
    sys.exit(main())
