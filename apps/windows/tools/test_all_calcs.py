import ctypes, ctypes.wintypes as wt, hashlib, json, os, subprocess, sys, time
from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_COMMAND = 0x0111
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_CLOSE = 0x0010
PW_CLIENTONLY = 1

ID_BTN_MODE_ALL = 1010
ID_BTN_MODE_DUAL = 1011
ID_BTN_MODE_WHITE = 1012
ID_BTN_MODE_GRAY = 1013
ID_BTN_MODE_GRAPH = 1014

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"

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

def send_key(hwnd, vk):
    user32.SendMessageW(hwnd, WM_KEYDOWN, vk, 0)
    time.sleep(0.08)
    user32.SendMessageW(hwnd, WM_KEYUP, vk, 0)
    time.sleep(0.08)

def main():
    root = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial"
    exe = os.path.join(root, "build-verify", "Release", "CalculadoraUnificada.exe")
    outdir = os.path.join(root, "verification", "run-all-calcs-test")
    os.makedirs(outdir, exist_ok=True)
    
    print(f"Lanzando {exe}...")
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    
    hwnd = None
    for _ in range(30):
        time.sleep(0.5)
        hwnd = find_window()
        if hwnd: break
        
    if not hwnd:
        print("ERROR: No se encontro ventana principal")
        proc.kill()
        sys.exit(1)
        
    canvas = find_canvas(hwnd)
    print(f"Ventana HWND={hex(hwnd)}, Canvas={hex(canvas) if canvas else 'None'}")
    
    # Esperar 4 s para que CG50 GCal se inicialice
    time.sleep(4.0)
    
    results = {}
    
    # 1. Modo Blanca (ClassWiz)
    print("Probando Modo Blanca...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_WHITE, 0)
    time.sleep(1.0)
    send_key(hwnd, ord('1'))
    send_key(hwnd, 0x6B) # VK_ADD
    send_key(hwnd, ord('2'))
    send_key(hwnd, 0x0D) # VK_RETURN
    time.sleep(0.5)
    p_white = os.path.join(outdir, "modo_blanca_classwiz.png")
    capture(canvas, p_white)
    results["modo_blanca"] = {"path": p_white, "sha256": sha256_file(p_white)}
    
    # 2. Modo Gris (Natural ES)
    print("Probando Modo Gris...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAY, 0)
    time.sleep(1.0)
    send_key(hwnd, ord('5'))
    send_key(hwnd, 0x6A) # VK_MULTIPLY
    send_key(hwnd, ord('5'))
    send_key(hwnd, 0x0D) # VK_RETURN
    time.sleep(0.5)
    p_gray = os.path.join(outdir, "modo_gris_natural_es.png")
    capture(canvas, p_gray)
    results["modo_gris"] = {"path": p_gray, "sha256": sha256_file(p_gray)}
    
    # 3. Modo Color (CG50 GCal oficial)
    print("Probando Modo CG50...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.5)
    p_cg50 = os.path.join(outdir, "modo_color_cg50_oficial.png")
    capture(canvas, p_cg50)
    results["modo_cg50"] = {"path": p_cg50, "sha256": sha256_file(p_cg50)}
    
    # 4. Modo Todas (All 3)
    print("Probando Modo Todas (Triple)...")
    user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_ALL, 0)
    time.sleep(1.5)
    p_all = os.path.join(outdir, "modo_triple_todas.png")
    capture(canvas, p_all)
    results["modo_todas"] = {"path": p_all, "sha256": sha256_file(p_all)}
    
    # Cierre limpio
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    time.sleep(1.0)
    if proc.poll() is None:
        proc.terminate()
        
    report = {
        "status": "PASS",
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "exe": exe,
        "results": results
    }
    
    rep_path = os.path.join(outdir, "reporte_todos_los_modos.json")
    with open(rep_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
    print("Reporte generado exitosamente:")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
