#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_pc_trace.py — Lanza el manager oficial y muestrea el PC del CPU emulado
(CPU73050.dll + 0x675A18 vía ReadProcessMemory, el mismo offset que usa el
GCalHostProbe) cada --intervalo-ms milisegundos durante --segundos segundos,
registrando solo los cambios. Sirve para comparar la secuencia de regiones
que ejecuta el guest del manager oficial durante el arranque contra la del
probe (PC_TRACE del modo res0e).

Uso: python f14_pc_trace.py [--segundos N] [--intervalo-ms M] [--salida RUTA]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f14_launch_manager as lm

k32 = ctypes.windll.kernel32
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
CPU_RVA = 0x675A18
TARGET_DLL = "CPU73050.dll"


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD),
        ("th32ModuleID", wt.DWORD),
        ("th32ProcessID", wt.DWORD),
        ("GlblcntUsage", wt.DWORD),
        ("ProccntUsage", wt.DWORD),
        ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wt.DWORD),
        ("hModule", wt.HMODULE),
        ("szModule", wt.WCHAR * 256),
        ("szExePath", wt.WCHAR * 260),
    ]


def find_module_base(pid, name):
    snap = k32.CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snap == INVALID_HANDLE_VALUE:
        return None
    try:
        entry = MODULEENTRY32W()
        entry.dwSize = ctypes.sizeof(MODULEENTRY32W)
        ok = k32.Module32FirstW(snap, ctypes.byref(entry))
        while ok:
            if entry.szModule.lower() == name.lower():
                return ctypes.cast(entry.modBaseAddr, ctypes.c_void_p).value
            ok = k32.Module32NextW(snap, ctypes.byref(entry))
    finally:
        k32.CloseHandle(snap)
    return None


def main():
    secs = 12.0
    interval_ms = 20
    out_path = None
    args = sys.argv[1:]
    if "--segundos" in args:
        secs = float(args[args.index("--segundos") + 1])
    if "--intervalo-ms" in args:
        interval_ms = int(args[args.index("--intervalo-ms") + 1])
    if "--salida" in args:
        out_path = args[args.index("--salida") + 1]
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    lines = []

    def emit(text):
        print(text)
        lines.append(text)

    emit("[pc-trace] lanzando manager …")
    proc = subprocess.Popen([lm.EXE, lm.ARGS])
    pid = proc.pid
    launch_t0 = time.time()
    emit(f"[pc-trace] pid={pid}")

    # El wizard de activación BLOQUEA la carga del emulador (verificado:
    # sin clicks, CPU73050.dll no aparece). Los clicks van en un hilo a los
    # tiempos del flujo f14_launch_manager mientras el hilo principal rastrea
    # el PC desde t=0 (el guest puede bootear en cualquier momento tras los
    # clicks; la captura no debe perderse nada).
    def click_job():
        for delay, cid, name in ((6.0, 14021, "No. deseo continuar"),
                                 (6.8, 12324, "Siguiente >"),
                                 (8.3, 12325, "Finalizar")):
            wait = delay - (time.time() - launch_t0)
            if wait > 0:
                time.sleep(wait)
            emit("[pc-trace] " + lm.click_ctrl(pid, cid, timeout=12))

    threading.Thread(target=click_job, daemon=True).start()

    emit("[pc-trace] esperando CPU73050.dll (desde t=0) …")
    base = None
    deadline = time.time() + 60.0
    while time.time() < deadline:
        base = find_module_base(pid, TARGET_DLL)
        if base is not None:
            break
        time.sleep(0.1)
    if base is None:
        emit("[pc-trace] ERROR: CPU73050.dll no apareció")
        return 2
    emit(f"[pc-trace] base CPU73050.dll = {base:08X}")
    handle = k32.OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not handle:
        emit("[pc-trace] ERROR: OpenProcess falló")
        return 3
    addr = base + CPU_RVA
    buf = ctypes.c_uint32()
    read = ctypes.c_size_t()
    irdata = (ctypes.c_ubyte * 16)()

    def read_dword(a):
        tmp = ctypes.c_uint32()
        okr = k32.ReadProcessMemory(
            handle, ctypes.c_void_p(a), ctypes.byref(tmp), 4,
            ctypes.byref(read))
        return tmp.value if okr else None

    last = None
    last_ir = None
    last_ir_base = None
    t0 = time.time()
    emit("[pc-trace] PC_TRACE_OFFICIAL begin")
    tick = 0
    while time.time() - t0 < secs:
        pc = read_dword(addr)
        if pc != last:
            emit(f"PC_TRACE_OFFICIAL t_ms={int((time.time() - t0) * 1000)} "
                 f"pc={pc:08X}")
            last = pc
        tick += 1
        if tick % 20 == 0:
            # Ancla temporal: marcador del InfoRam del oficial (0xAA en
            # +0x2FF y cadena de versión en +0x300) vía el puntero en
            # GCalAppIFA.dll+0x9D64.
            if last_ir_base is None:
                last_ir_base = find_module_base(pid, "GCalAppIFA.dll")
            if last_ir_base is not None:
                ptr = read_dword(last_ir_base + 0x9D64)
                if ptr:
                    okr = k32.ReadProcessMemory(
                        handle, ctypes.c_void_p(ptr + 0x2F0),
                        ctypes.byref(irdata), 16, ctypes.byref(read))
                    if okr:
                        ir = bytes(irdata)
                        if ir != last_ir:
                            emit(f"IR_SIGNAL t_ms="
                                 f"{int((time.time() - t0) * 1000)} "
                                 f"ptr={ptr:08X} bytes={ir.hex()}")
                            last_ir = ir
        time.sleep(interval_ms / 1000.0)
    emit("[pc-trace] PC_TRACE_OFFICIAL end")
    # Reintento final del wizard (por si el hilo no encontró algún control),
    # para dejar el manager en estado normal.
    time.sleep(1.0)
    emit("[pc-trace] " + lm.click_ctrl(pid, 14021, timeout=3))
    time.sleep(0.5)
    emit("[pc-trace] " + lm.click_ctrl(pid, 12324, timeout=3))
    time.sleep(0.8)
    emit("[pc-trace] " + lm.click_ctrl(pid, 12325, timeout=3))
    time.sleep(1.0)
    for w in lm.visible_windows(pid):
        emit(f"[pc-trace] ventana {hex(w)} {lm.get_text(w)!r}")
    k32.CloseHandle(handle)
    if out_path:
        with open(out_path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("\n".join(lines) + "\n")
        print(f"[pc-trace] log escrito en {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
