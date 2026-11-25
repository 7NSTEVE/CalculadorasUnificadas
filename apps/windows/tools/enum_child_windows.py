#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
enum_child_windows.py — Enumera ventanas hijas (clase, texto, ID, rect) de una
ventana padre por PID o HWND, para automatizar dialogos del manager oficial.

Uso: python enum_child_windows.py <pid> [hwnd-hex]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import ctypes
import ctypes.wintypes as wt
import sys

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


def get_text(hwnd):
    n = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(n + 2)
    user32.GetWindowTextW(hwnd, buf, n + 2)
    return buf.value


def get_class(hwnd):
    buf = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, buf, 256)
    return buf.value


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    pid = int(sys.argv[1])
    target_hwnd = int(sys.argv[2], 16) if len(sys.argv) > 2 else None

    def is_ours(hwnd):
        wpid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        return wpid.value == pid and user32.IsWindowVisible(hwnd)

    def find_roots():
        roots = []

        @WNDENUMPROC
        def cb(hwnd, _lparam):
            if is_ours(hwnd):
                roots.append(hwnd)
            return True

        user32.EnumWindows(cb, 0)
        return roots

    roots = find_roots()
    if target_hwnd:
        roots = [h for h in roots if h == target_hwnd] or [target_hwnd]
    for root in roots:
        print(f"VENTANA {hex(root)} class={get_class(root)!r} text={get_text(root)!r}")
        children = []

        @WNDENUMPROC
        def cb_child(hwnd, _lparam):
            children.append(hwnd)
            return True

        user32.EnumChildWindows(root, cb_child, 0)
        for ch in children:
            cls = get_class(ch)
            text = get_text(ch)
            cid = user32.GetDlgCtrlID(ch)
            style = user32.GetWindowLongW(ch, -16)
            visible = bool(user32.IsWindowVisible(ch))
            enabled = not bool(user32.GetWindowLongW(ch, 0) & 0x08000000)
            print(f"  {hex(ch):>10} id={cid:<6} class={cls:<14} vis={int(visible)} "
                  f"en={int(enabled)} text={text!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
