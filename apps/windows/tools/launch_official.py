#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
launch_official.py — Arranca el Manager oficial y elimina el Activation Wizard
de forma automatica (sin intervencion humana), dejando el emulador listo para
que cualquier agente lo use via cg50_agent.py.

- Si ya hay una ventana de emulador, la reutiliza.
- Si aparece el Activation Wizard (#32770), pulsa su boton Cancelar/Cancel por
  BM_CLICK (funciona desde un proceso de la misma integridad).
- Espera la ventana "[Modo emulador]" y emite JSON con el resultado.

Uso:
  python tools\\launch_official.py [--timeout 90] [--exe RUTA]
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import json
import os
import subprocess
import sys
import time

user32 = ctypes.windll.user32
BM_CLICK = 0x00F5

DEFAULT_EXE = r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"


def windows():
    out = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def cb(h, l):
        if user32.IsWindowVisible(h):
            n = user32.GetWindowTextLengthW(h)
            if n > 0:
                b = ctypes.create_unicode_buffer(n + 1)
                user32.GetWindowTextW(h, b, n + 1)
                c = ctypes.create_unicode_buffer(256)
                user32.GetClassNameW(h, c, 256)
                out.append({"hwnd": int(h), "title": b.value, "class": c.value})
        return True

    user32.EnumWindows(cb, 0)
    return out


def find_emulator():
    for w in windows():
        if "fx-CG Manager PLUS" in w["title"] and "[Modo emulador]" in w["title"]:
            return w
    return None


def find_wizard():
    for w in windows():
        if "Activation Wizard" in w["title"] or ("fx-CG" in w["title"] and w["class"] == "#32770"):
            return w
    return None


def child_buttons(hwnd):
    out = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def cb(h, l):
        c = ctypes.create_unicode_buffer(64)
        user32.GetClassNameW(h, c, 64)
        n = user32.GetWindowTextLengthW(h)
        b = ctypes.create_unicode_buffer(n + 2)
        user32.GetWindowTextW(h, b, n + 2)
        out.append({"hwnd": int(h), "class": c.value, "text": b.value})
        return True

    user32.EnumChildWindows(hwnd, cb, 0)
    return out


def dismiss_wizard(wizard):
    buttons = child_buttons(wizard["hwnd"])
    cancel = [b for b in buttons if b["class"].lower() == "button"
              and any(k in b["text"].lower() for k in ("cancel", "cancelar", "cerrar", "close"))]
    if not cancel:
        return {"dismissed": False, "buttons": [b["text"] for b in buttons]}
    user32.SendMessageW(cancel[0]["hwnd"], BM_CLICK, 0, 0)
    return {"dismissed": True, "button": cancel[0]["text"]}


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--timeout", type=int, default=90)
    ap.add_argument("--exe", default=DEFAULT_EXE)
    args = ap.parse_args(argv)

    emu = find_emulator()
    if emu:
        print(json.dumps({"ok": True, "reused": True, "window": emu}, ensure_ascii=False))
        return 0
    if not os.path.exists(args.exe):
        print(json.dumps({"ok": False, "error": "exe no encontrado", "exe": args.exe}))
        return 2
    subprocess.Popen([args.exe], close_fds=True)

    deadline = time.time() + args.timeout
    wizard_reports = []
    while time.time() < deadline:
        emu = find_emulator()
        if emu:
            print(json.dumps({"ok": True, "reused": False, "window": emu,
                              "wizard": wizard_reports}, ensure_ascii=False))
            return 0
        wz = find_wizard()
        if wz:
            rep = dismiss_wizard(wz)
            rep["window"] = wz["title"]
            wizard_reports.append(rep)
            time.sleep(1.0)
        time.sleep(0.5)
    print(json.dumps({"ok": False, "error": "timeout esperando emulador",
                      "wizard": wizard_reports}, ensure_ascii=False))
    return 2


if __name__ == "__main__":
    sys.exit(main())
