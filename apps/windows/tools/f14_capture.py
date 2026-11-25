#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_capture.py — Arnés Frida para trazar el manager oficial CG50 (F1.4/F2.4).

Uso:
    python f14_capture.py [duracion-segundos] [salida.jsonl]

Spawnea el manager oficial, inyecta f14_trace_script.js, guarda los eventos
como JSONL (con buffers binarios anexos en <salida>.bufs/) y cierra limpio.
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import ctypes
import ctypes.wintypes as wt
import json
import os
import sys
import time

import frida

EXE_DIR = (r"C:\Program Files (x86)\CASIO"
           r"\fx-CG Manager PLUS Subscription for fx-CG50series")
EXE = os.path.join(EXE_DIR, "fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe")
HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "f14_trace_script.js")

user32 = ctypes.windll.user32
WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)


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


def click_ctrl(pid, cid, timeout=15.0):
    """Click en un control por ID dentro de las ventanas visibles del pid."""
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
                return True
        time.sleep(0.4)
    return False


def drive_activation_wizard(pid):
    """Responde el wizard: 'No. Deseo continuar con la version actual.' + Siguiente."""
    time.sleep(5.0)
    ok1 = click_ctrl(pid, 14021, timeout=20)
    time.sleep(0.8)
    ok2 = click_ctrl(pid, 12324, timeout=15)
    time.sleep(0.5)
    click_ctrl(pid, 12325, timeout=6)  # Finalizar si aparece
    print(f"[f14] wizard: radio={ok1} siguiente={ok2}")


def main() -> int:
    duration = float(sys.argv[1]) if len(sys.argv) > 1 else 60.0
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        HERE, "..", "verification", "2026-10-16-f1-dispatcher",
        "frida-oficial-traza.jsonl")
    out_path = os.path.abspath(out_path)
    bufs_dir = out_path + ".bufs"
    os.makedirs(bufs_dir, exist_ok=True)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    device = frida.get_local_device()
    print(f"[f14] spawn {EXE}")
    # Argumento requerido: sin /n"<producto>" el manager muere con
    # "A problem occurred while accessing the registry" (parser 0x123E0,
    # verificado 2026-10-16 contra el .lnk del Menu Inicio).
    argv = [EXE, '/n"fx-CG Manager PLUS Subscription for fx-CG50series"']
    pid = device.spawn(argv, cwd=EXE_DIR)
    session = device.attach(pid)
    with open(SCRIPT, encoding="utf-8") as fh:
        code = fh.read()
    script = session.create_script(code)

    out = open(out_path, "w", encoding="utf-8", buffering=1)
    counters = {"events": 0, "bufs": 0}

    def on_message(message, data):
        counters["events"] += 1
        rec = {"seq": counters["events"], "wall": time.time(), "message": message}
        if data:
            # frida entrega listas de buffers como bytes concatenados solo en
            # scripts de una pieza; aqui data es un solo ArrayBuffer.
            name = os.path.join(bufs_dir, f"{counters['events']:06d}.bin")
            with open(name, "wb") as fh:
                fh.write(data)
            rec["buf_file"] = name
            counters["bufs"] += 1
        out.write(json.dumps(rec, ensure_ascii=False, default=str) + "\n")
        if counters["events"] % 500 == 0:
            print(f"[f14] eventos={counters['events']} bufs={counters['bufs']}")

    script.on("message", on_message)
    script.load()
    device.resume(pid)
    # Conducir el Activation Wizard mientras corre la traza (licencia trial
    # vigente: "No. Deseo continuar con la version actual." + Siguiente).
    drive_activation_wizard(pid)
    print(f"[f14] corriendo {duration}s …")
    deadline = time.time() + duration
    try:
        while time.time() < deadline:
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    try:
        session.detach()
    except Exception:
        pass
    try:
        device.kill(pid)
    except Exception:
        pass
    out.close()
    print(f"[f14] hecho: {out_path} eventos={counters['events']} "
          f"bufs={counters['bufs']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
