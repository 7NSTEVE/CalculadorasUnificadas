import win32gui, win32process

def cb(hwnd, extra):
    if win32gui.IsWindowVisible(hwnd):
        t = win32gui.GetWindowText(hwnd)
        if t:
            _, pid = win32process.GetWindowThreadProcessId(hwnd)
            print(f'PID {pid:5d} | HWND 0x{hwnd:08X} | "{t}"')

win32gui.EnumWindows(cb, None)
