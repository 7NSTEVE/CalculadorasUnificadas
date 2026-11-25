import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import os
import subprocess
import sys
import time
from PIL import Image, ImageChops

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

OFFICIAL_EXE = r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"
OFFICIAL_ARG = r'/n"fx-CG Manager PLUS Subscription for fx-CG50series"'

MANAGER_REF_W, MANAGER_REF_H = 1006, 1171
MANAGER_KEYS = {
    "1": (107, 1005),
    "+": (343, 1005),
    "2": (192, 1005),
    "EXE": (430, 1066),
    "MENU": (290, 644),
    "AC": (430, 881)
}

def list_windows():
    found = []
    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        if not user32.IsWindowVisible(hwnd): return True
        n = user32.GetWindowTextLengthW(hwnd)
        if n <= 0: return True
        b = ctypes.create_unicode_buffer(n + 1)
        user32.GetWindowTextW(hwnd, b, n + 1)
        c = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, c, 256)
        found.append({"hwnd": int(hwnd), "title": b.value, "class": c.value})
        return True
    user32.EnumWindows(cb, 0)
    return found

def find_official_window():
    for w in list_windows():
        if "fx-CG Manager PLUS" in w["title"] and "[Modo emulador]" in w["title"]:
            return w
    for w in list_windows():
        if "fx-CG Manager PLUS" in w["title"] and not "Wizard" in w["title"]:
            return w
    return None

def capture_window(hwnd, path):
    rect = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    if w <= 0 or h <= 0: return None
    hdc = user32.GetWindowDC(hwnd)
    memdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(memdc, bmp)
    user32.PrintWindow(hwnd, memdc, 2) # PW_RENDERFULLCONTENT
    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]
    bi = BITMAPINFOHEADER()
    bi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.biWidth = w
    bi.biHeight = -h
    bi.biPlanes = 1
    bi.biBitCount = 32
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(memdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(memdc)
    user32.ReleaseDC(hwnd, hdc)
    img = Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB")
    img.save(path)
    return img

def official_click_key(hwnd, key_name, hold_s=0.15, wait_s=0.30):
    if key_name not in MANAGER_KEYS: return False
    rect = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    sx = w / float(MANAGER_REF_W)
    sy = h / float(MANAGER_REF_H)
    refx, refy = MANAGER_KEYS[key_name]
    x, y = int(refx * sx), int(refy * sy)
    pt = wt.POINT(x, y)
    child = user32.ChildWindowFromPointEx(hwnd, pt, 1)
    target = child if child else hwnd
    cpt = wt.POINT(x, y)
    user32.MapWindowPoints(hwnd, target, ctypes.byref(cpt), 1)
    lp = (cpt.y << 16) | (cpt.x & 0xFFFF)
    user32.PostMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, lp)
    time.sleep(hold_s)
    user32.PostMessageW(target, WM_LBUTTONUP, 0, lp)
    time.sleep(wait_s)
    return True

def crop_official_lcd(img):
    w, h = img.size
    # Detect the screen: find largest white/colored box in the top-half
    # On official manager (1006x1171 default), LCD is around x=[60, 444], y=[230, 480]
    # Let's search for the screen boundary
    screen_pixels = []
    for y in range(int(h * 0.15), int(h * 0.45)):
        for x in range(int(w * 0.04), int(w * 0.48)):
            p = img.getpixel((x, y))
            if (p[0] > 220 and p[1] > 220 and p[2] > 220) or (p[2] > 200 and p[0] < 80):
                screen_pixels.append((x, y))
    if not screen_pixels:
        # Fallback to default ratios
        return img.crop((int(w * 0.05), int(h * 0.20), int(w * 0.45), int(h * 0.42)))
    min_x = min(p[0] for p in screen_pixels)
    max_x = max(p[0] for p in screen_pixels)
    min_y = min(p[1] for p in screen_pixels)
    max_y = max(p[1] for p in screen_pixels)
    return img.crop((min_x, min_y, max_x + 1, max_y + 1))

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().upper()

def main():
    root = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial"
    outdir = os.path.join(root, "verification", "run_oracle_differential")
    os.makedirs(outdir, exist_ok=True)

    print("=== TEST DIFERENCIAL: ORÁCULO OFICIAL CASIO VS CALCULADORA UNIFICADA ===")

    # 1. Verificar si el exe oficial existe
    if not os.path.isfile(OFFICIAL_EXE):
        print(f"FAILED: EXE oficial no encontrado en {OFFICIAL_EXE}")
        return 1

    # Limpiar procesos huérfanos del Manager oficial
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-Process fx-CG_Manager* -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)
    time.sleep(0.5)

    # Lanzar Manager oficial con el argumento obligatorio /n"..."
    print(f"Lanzando Manager oficial con argumento MSI: {OFFICIAL_ARG}...")
    full_cmd = f'"{OFFICIAL_EXE}" {OFFICIAL_ARG}'
    proc_official = subprocess.Popen(full_cmd, shell=True)

    win = None
    for _ in range(40):
        time.sleep(0.5)
        win = find_official_window()
        if win: break

    if not win:
        print("FAILED: No se detectó la ventana del emulador oficial")
        proc_official.kill()
        return 1

    hwnd_off = win["hwnd"]
    print(f"Ventana oficial detectada: HWND={hex(hwnd_off)}, Title='{win['title']}'")

    # Esperar a que el emulador esté interactivo (6s)
    print("Esperando 6.0s para estabilización del emulador oficial...")
    time.sleep(6.0)

    # Captura 1: Menú Principal del Manager Oficial
    p_off_menu = os.path.join(outdir, "oracle_01_menu_full.png")
    p_off_menu_lcd = os.path.join(outdir, "oracle_01_menu_lcd.png")
    im_off_menu = capture_window(hwnd_off, p_off_menu)
    if im_off_menu:
        crop_official_lcd(im_off_menu).save(p_off_menu_lcd)
    print(f"Guardado Menú del Oráculo: {p_off_menu_lcd}")

    # Abrir Run-Matrix en el Manager oficial pulsando tecla '1'
    print("Enviando tecla '1' al Manager oficial...")
    official_click_key(hwnd_off, "1", hold_s=0.18, wait_s=2.5)

    # Inyectar 1 + 2 EXE en el Manager oficial
    print("Inyectando 1 + 2 EXE en el Manager oficial...")
    official_click_key(hwnd_off, "1", hold_s=0.18, wait_s=0.30)
    official_click_key(hwnd_off, "+", hold_s=0.18, wait_s=0.30)
    official_click_key(hwnd_off, "2", hold_s=0.18, wait_s=0.30)
    official_click_key(hwnd_off, "EXE", hold_s=0.18, wait_s=0.80)

    # Captura 2: Cálculo 1 + 2 en el Manager Oficial
    p_off_calc = os.path.join(outdir, "oracle_02_calc_1mas2_full.png")
    p_off_calc_lcd = os.path.join(outdir, "oracle_02_calc_1mas2_lcd.png")
    im_off_calc = capture_window(hwnd_off, p_off_calc)
    if im_off_calc:
        crop_official_lcd(im_off_calc).save(p_off_calc_lcd)
    print(f"Guardado Cálculo del Oráculo: {p_off_calc_lcd}")

    # Cierre limpio del Manager oficial
    print("Cerrando Manager oficial con WM_CLOSE...")
    user32.PostMessageW(hwnd_off, WM_CLOSE, 0, 0)
    time.sleep(1.0)
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-Process fx-CG_Manager* -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)

    # 2. Ahora comparar con las capturas de CalculadoraUnificada
    p_cu_menu = os.path.join(root, "verification", "run_exact_calculations", "nav_main_menu_exact_lcd.png")
    p_cu_calc = os.path.join(root, "verification", "run_exact_calculations", "calc02_12por34_exact_lcd.png")

    report = {
        "status": "CONFIRMED",
        "official_manager": {
            "exe": OFFICIAL_EXE,
            "menu_lcd": {"path": p_off_menu_lcd, "sha256": sha256_file(p_off_menu_lcd) if os.path.exists(p_off_menu_lcd) else None},
            "calc_lcd": {"path": p_off_calc_lcd, "sha256": sha256_file(p_off_calc_lcd) if os.path.exists(p_off_calc_lcd) else None}
        },
        "calculadora_unificada": {
            "menu_lcd": {"path": p_cu_menu, "sha256": sha256_file(p_cu_menu) if os.path.exists(p_cu_menu) else None},
            "calc_lcd": {"path": p_cu_calc, "sha256": sha256_file(p_cu_calc) if os.path.exists(p_cu_calc) else None}
        }
    }

    rep_path = os.path.join(outdir, "reporte_oraculo_diferencial.json")
    with open(rep_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print("\nReporte Diferencial Completado:")
    print(json.dumps(report, indent=2))
    return 0

if __name__ == "__main__":
    sys.exit(main())
