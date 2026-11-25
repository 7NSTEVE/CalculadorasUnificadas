import ctypes
from ctypes import wintypes
import time

user32 = ctypes.windll.user32

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

found_hwnd = []

def enum_cb(hwnd, lparam):
    length = user32.GetWindowTextLengthW(hwnd)
    if length > 0:
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        if 'Calculadoras CASIO' in buf.value or 'Casio' in buf.value:
            found_hwnd.append((hwnd, buf.value))
    return True

user32.EnumWindows(WNDENUMPROC(enum_cb), 0)
print('Found:', found_hwnd)

for hwnd, title in found_hwnd:
    print('Bringing to front:', hex(hwnd), title)
    user32.ShowWindow(hwnd, 3) # SW_MAXIMIZE
    user32.SetForegroundWindow(hwnd)
    user32.BringWindowToTop(hwnd)
