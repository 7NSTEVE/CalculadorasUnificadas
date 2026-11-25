#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
click_dlg.py — Hace click en un control de un dialogo por ID de control
(BM_CLICK via WM_COMMAND), para automatizar el Activation Wizard del manager
oficial. Uso legítimo: continuar con la licencia trial ya activa.

Uso: python click_dlg.py <pid> <ctrl-id> [ctrl-id2 ...]
Herramienta de investigacion del proyecto Calculadora Unificada (2026-10-16).
"""
import ctypes
import ctypes.wintypes as wt
import sys
import time

user32 = ctypes.windll.user32

WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)


def get_text(hwnd):
    n = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(n + 2)
    user32.GetWindowTextW(hwnd, buf, n + 2)
    return buf.value


def find_windows(pid):
    out = []

    @WNDENUMPROC
    def cb(hwnd, _):
        wpid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        if wpid.value == pid and user32.IsWindowVisible(hwnd):
            out.append(hwnd)
        return True

    user32.EnumWindows(cb, 0)
    return out


def find_child_by_id(root, cid):
    found = []

    @WNDENUMPROC
    def cb(hwnd, _):
        if user32.GetDlgCtrlID(hwnd) == cid:
            found.append(hwnd)
        return True

    user32.EnumChildWindows(root, cb, 0)
    return found[0] if found else None


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    pid = int(sys.argv[1])
    for cid_str in sys.argv[2:]:
        cid = int(cid_str)
        target = None
        for root in find_windows(pid):
            ch = find_child_by_id(root, cid)
            if ch:
                target = ch
                parent = root
                break
        if target is None:
            print(f"ctrl {cid}: NO ENCONTRADO")
            continue
        print(f"click ctrl {cid} ({get_text(target)!r}) en {hex(parent)}")
        user32.SendMessageW(target, 0x00F5, 0, 0)  # BM_CLICK
        time.sleep(0.6)
    # estado final de ventanas
    time.sleep(1.0)
    for root in find_windows(pid):
        print(f"VENTANA {hex(root)} text={get_text(root)!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
