import ctypes
from ctypes import wintypes

user32 = ctypes.windll.user32
user32.GetDesktopWindow.restype = wintypes.HWND
user32.FindWindowExW.restype = wintypes.HWND
user32.FindWindowExW.argtypes = [wintypes.HWND, wintypes.HWND, wintypes.LPCWSTR, wintypes.LPCWSTR]
user32.GetWindowTextLengthW.restype = ctypes.c_int
user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
user32.GetWindowTextW.restype = ctypes.c_int
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.ShowWindow.restype = wintypes.BOOL
user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
user32.SetForegroundWindow.restype = wintypes.BOOL
user32.SetForegroundWindow.argtypes = [wintypes.HWND]

desktop = user32.GetDesktopWindow()
hwnd = user32.FindWindowExW(desktop, 0, None, None)
count = 0
found = []
while hwnd:
    length = user32.GetWindowTextLengthW(hwnd)
    if length > 0:
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        if buf.value:
            found.append((hwnd, buf.value))
            if 'Calculadoras' in buf.value or 'Casio' in buf.value:
                print(f'MATCH: HWND 0x{hwnd:X}, Title: "{buf.value}"')
                user32.ShowWindow(hwnd, 9)
                user32.SetForegroundWindow(hwnd)
    hwnd = user32.FindWindowExW(desktop, hwnd, None, None)
    count += 1
print('Scanned total windows:', count, 'Found titles count:', len(found))
for h, t in found[:10]:
    print(f'  0x{h:08X}: "{t}"')
