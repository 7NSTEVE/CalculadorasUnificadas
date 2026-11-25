import ctypes
from ctypes import wintypes
import time

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

def enum_cb(hwnd, lparam):
    pid = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    if pid.value == lparam:
        print(f"Found Window for PID {pid.value}: HWND 0x{hwnd:X}")
        user32.ShowWindow(hwnd, 9) # SW_RESTORE
        user32.SetWindowPos(hwnd, -1, 100, 100, 1400, 900, 0x0040) # HWND_TOPMOST | SWP_SHOWWINDOW
        user32.SetWindowPos(hwnd, -2, 100, 100, 1400, 900, 0x0040) # HWND_NOTOPMOST | SWP_SHOWWINDOW
        user32.SetForegroundWindow(hwnd)
        user32.BringWindowToTop(hwnd)
    return True

# Get PID of CalculadoraUnificada
import subprocess
p = subprocess.run(['powershell', '-Command', '(Get-Process -Name CalculadoraUnificada).Id'], capture_output=True, text=True)
pids = [int(x.strip()) for x in p.stdout.split() if x.strip().isdigit()]
print('CalculadoraUnificada PIDs:', pids)

for pid in pids:
    user32.EnumWindows(WNDENUMPROC(enum_cb), pid)
