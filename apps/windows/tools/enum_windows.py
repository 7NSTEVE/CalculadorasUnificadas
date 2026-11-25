import ctypes
from ctypes import wintypes

user32 = ctypes.windll.user32

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

def check_pid(pid):
    def enum_cb(hwnd, lparam):
        w_pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(w_pid))
        if w_pid.value == pid:
            length = user32.GetWindowTextLengthW(hwnd)
            buf = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buf, length + 1)
            cls_buf = ctypes.create_unicode_buffer(256)
            user32.GetClassNameW(hwnd, cls_buf, 256)
            vis = user32.IsWindowVisible(hwnd)
            print(f'HWND: 0x{hwnd:X}, Class: {cls_buf.value}, Visible: {vis}, Title: "{buf.value}"')
        return True

    user32.EnumWindows(WNDENUMPROC(enum_cb), 0)

if __name__ == '__main__':
    import sys
    pid = int(sys.argv[1]) if len(sys.argv) > 1 else 30304
    check_pid(pid)
