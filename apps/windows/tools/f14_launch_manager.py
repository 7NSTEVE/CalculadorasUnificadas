#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_launch_manager.py — Lanza el manager oficial con el argumento /n correcto
y responde el Activation Wizard (continuar con la licencia trial vigente),
dejando el manager listo para trazas o para llegar al MAIN MENU.

Uso: python f14_launch_manager.py [--wait-segundos N]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

user32 = ctypes.windll.user32
WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)

EXE = (r"C:\Program Files (x86)\CASIO\\"
       r"fx-CG Manager PLUS Subscription for fx-CG50series\\"
       r"fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe")
ARGS = '/n"fx-CG Manager PLUS Subscription for fx-CG50series"'


def get_text(hwnd):
    n = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(n + 2)
    user32.GetWindowTextW(hwnd, buf, n + 2)
    return buf.value


def visible_windows(pid):
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


def click_ctrl(pid, cid, timeout=10.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        for root in visible_windows(pid):
            found = []

            @WNDENUMPROC
            def cb(hwnd, _):
                if user32.GetDlgCtrlID(hwnd) == cid:
                    found.append(hwnd)
                return True

            user32.EnumChildWindows(root, cb, 0)
            if found:
                user32.SendMessageW(found[0], 0x00F5, 0, 0)  # BM_CLICK
                return f"ctrl {cid} ({get_text(found[0])!r}) clickeado"
        time.sleep(0.4)
    return f"ctrl {cid}: NO ENCONTRADO tras {timeout}s"


def main():
    wait_after = float(sys.argv[sys.argv.index("--wait-segundos") + 1]) \
        if "--wait-segundos" in sys.argv else 25.0
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    print("[launch] iniciando manager con /n …")
    proc = subprocess.Popen([EXE, ARGS])
    pid = proc.pid
    print(f"[launch] pid={pid}")
    time.sleep(6.0)
    # Pagina 1: radio "No. Deseo continuar con la version actual." (14021)
    print("[launch]", click_ctrl(pid, 14021, timeout=15))
    time.sleep(0.8)
    # Siguiente >
    print("[launch]", click_ctrl(pid, 12324, timeout=15))
    time.sleep(1.5)
    # Puede aparecer "Finalizar" (12325) en la ultima pagina
    print("[launch]", click_ctrl(pid, 12325, timeout=8))
    time.sleep(2.0)
    print("[launch] ventanas visibles:")
    for w in visible_windows(pid):
        print("   ", hex(w), repr(get_text(w)))
    print(f"[launch] esperando {wait_after}s (el emulador arranca)…")
    time.sleep(wait_after)
    print("[launch] ventanas finales:")
    for w in visible_windows(pid):
        print("   ", hex(w), repr(get_text(w)))
    alive = proc.poll() is None
    print(f"[launch] vivo={alive} pid={pid}")
    if not alive:
        return 1
    print(f"MANAGER_PID={pid}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
