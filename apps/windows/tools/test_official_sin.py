import ctypes
import ctypes.wintypes as wt
import subprocess
import time
import os

user32 = ctypes.windll.user32
BM_CLICK = 0x00F5
OFFICIAL_EXE = r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"

# Launch official manager
subprocess.run(["powershell", "-NoProfile", "-Command", "Get-Process fx-CG_Manager* | Stop-Process -Force"], capture_output=True)
time.sleep(1.0)
proc = subprocess.Popen([OFFICIAL_EXE, '/n"fx-CG Manager PLUS Subscription for fx-CG50series"'])
time.sleep(6.0)

# Find window and buttons
hwnd = None
buttons = {}
def cb(h, _):
    global hwnd
    buf = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(h, buf, 256)
    if "fx-CG" in buf.value or "Afx:" in buf.value:
        hwnd = h
    return True
user32.EnumWindows(ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(cb), 0)

def cb_child(h, _):
    cls = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(h, cls, 256)
    if cls.value == "Button":
        txt = ctypes.create_unicode_buffer(64)
        user32.GetWindowTextW(h, txt, 64)
        buttons[txt.value] = h
    return True
user32.EnumChildWindows(hwnd, ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(cb_child), 0)

print(f"Official Window: {hex(hwnd)}, Buttons found: {len(buttons)}")
print("Has '47' (SIN)?", "47" in buttons)
print("Has '37' (COS)?", "37" in buttons)
print("Has '57' (LN)?", "57" in buttons)
print("Has '77' (X)?", "77" in buttons)

# Press '1' to enter Run-Matrix
user32.SendMessageW(buttons["73"], BM_CLICK, 0, 0)
time.sleep(2.5)

# Clear with AC (11)
user32.SendMessageW(buttons["11"], BM_CLICK, 0, 0)
time.sleep(0.5)

# Press SIN (47)
if "47" in buttons:
    print("Clicking SIN (47)...")
    user32.SendMessageW(buttons["47"], BM_CLICK, 0, 0)
    time.sleep(1.0)

# Crop LCD and OCR
# Find Static 1404
lcd_hwnd = user32.GetDlgItem(hwnd, 1404)
print(f"LCD HWND: {hex(lcd_hwnd)}")

# Capture and OCR
rect = wt.RECT()
user32.GetWindowRect(lcd_hwnd, ctypes.byref(rect))
w = rect.right - rect.left
h = rect.bottom - rect.top
print(f"LCD size: {w}x{h}")

hdc_src = user32.GetDC(lcd_hwnd)
hdc_mem = ctypes.windll.gdi32.CreateCompatibleDC(hdc_src)
hbmp = ctypes.windll.gdi32.CreateCompatibleBitmap(hdc_src, w, h)
ctypes.windll.gdi32.SelectObject(hdc_mem, hbmp)
user32.PrintWindow(lcd_hwnd, hdc_mem, 1)

from PIL import Image
bmpinfo = wt.BITMAPINFO()
bmpinfo.bmiHeader.biSize = ctypes.sizeof(wt.BITMAPINFOHEADER)
bmpinfo.bmiHeader.biWidth = w
bmpinfo.bmiHeader.biHeight = -h
bmpinfo.bmiHeader.biPlanes = 1
bmpinfo.bmiHeader.biBitCount = 32
bmpinfo.bmiHeader.biCompression = 0
buf = ctypes.create_string_buffer(w * h * 4)
ctypes.windll.gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmpinfo), 0)
img = Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB")
img.save("official_sin_test.png")
print("Saved official_sin_test.png")

user32.ReleaseDC(lcd_hwnd, hdc_src)
ctypes.windll.gdi32.DeleteDC(hdc_mem)
ctypes.windll.gdi32.DeleteObject(hbmp)

proc.terminate()
time.sleep(1.0)
proc.kill()
