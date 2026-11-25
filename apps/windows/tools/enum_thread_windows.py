import ctypes
from ctypes import wintypes

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

class THREADENTRY32(ctypes.Structure):
    _fields_ = [
        ('dwSize', wintypes.DWORD),
        ('cntUsage', wintypes.DWORD),
        ('th32ThreadID', wintypes.DWORD),
        ('th32OwnerProcessID', wintypes.DWORD),
        ('tpBasePri', wintypes.LONG),
        ('tpDeltaPri', wintypes.LONG),
        ('dwFlags', wintypes.DWORD)
    ]

def enum_thread_windows(pid):
    hSnapshot = kernel32.CreateToolhelp32Snapshot(0x00000004, 0)
    te = THREADENTRY32()
    te.dwSize = ctypes.sizeof(THREADENTRY32)
    
    threads = []
    if kernel32.Thread32First(hSnapshot, ctypes.byref(te)):
        while True:
            if te.th32OwnerProcessID == pid:
                threads.append(te.th32ThreadID)
            if not kernel32.Thread32Next(hSnapshot, ctypes.byref(te)):
                break
    kernel32.CloseHandle(hSnapshot)
    
    print(f'Threads for PID {pid}:', threads)
    
    def cb(hwnd, lparam):
        length = user32.GetWindowTextLengthW(hwnd)
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buf, length + 1)
        cls_buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, cls_buf, 256)
        vis = user32.IsWindowVisible(hwnd)
        rect = wintypes.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(rect))
        print(f'  HWND: 0x{hwnd:X}, Class: {cls_buf.value}, Visible: {vis}, Rect: ({rect.left},{rect.top},{rect.right},{rect.bottom}), Title: "{buf.value}"')
        return True

    for tid in threads:
        print(f'Enum windows for thread {tid}:')
        user32.EnumThreadWindows(tid, WNDENUMPROC(cb), 0)

if __name__ == '__main__':
    import sys
    pid = int(sys.argv[1]) if len(sys.argv) > 1 else 31936
    enum_thread_windows(pid)
