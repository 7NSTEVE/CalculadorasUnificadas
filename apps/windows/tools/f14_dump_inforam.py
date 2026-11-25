#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_dump_inforam.py — Captura el InfoRam (512 KB) del manager oficial o de un
proceso ya en ejecución, leyendo el puntero global de GCalAppIFA.dll (RVA
0x9D64) vía ReadProcessMemory. Sirve para comparar el estado de intercambio UI
del oficial (MAIN MENU) contra el del runtime propio.

Uso:
  python f14_dump_inforam.py [--pid N] [--wait-segundos N] [--salida RUTA]
                             [--no-launch]

Sin --pid lanza el manager oficial reutilizando f14_launch_manager (wizard de
activación incluido). Herramienta de investigación del proyecto Calculadora
Unificada (2026-10-16).
"""
import ctypes
import ctypes.wintypes as wt
import hashlib
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f14_launch_manager as fl  # noqa: E402

kernel32 = ctypes.windll.kernel32

PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD),
        ("th32ModuleID", wt.DWORD),
        ("th32ProcessID", wt.DWORD),
        ("GlblcntUsage", wt.DWORD),
        ("ProccntUsage", wt.DWORD),
        ("modBaseAddr", ctypes.c_void_p),
        ("modBaseSize", wt.DWORD),
        ("hModule", ctypes.c_void_p),
        ("szModule", wt.WCHAR * 256),
        ("szExePath", wt.WCHAR * 260),
    ]


kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.CreateToolhelp32Snapshot.argtypes = (wt.DWORD, wt.DWORD)
kernel32.Module32FirstW.restype = wt.BOOL
kernel32.Module32FirstW.argtypes = (wt.HANDLE,
                                    ctypes.POINTER(MODULEENTRY32W))
kernel32.Module32NextW.restype = wt.BOOL
kernel32.Module32NextW.argtypes = (wt.HANDLE,
                                   ctypes.POINTER(MODULEENTRY32W))

GCALIFA_RVA_POINTER = 0x9D64
INFORAM_BYTES = 0x80000

DEFAULT_OUT = (r"E:\Descompilados\11_Otros\Emuladores"
               r"\Calculadoras_blanca_y_gris\Calculadora Unificada"
               r"\verification\2026-10-16-f1-dispatcher"
               r"\official-info-ram-20261016.bin")

kernel32.OpenProcess.restype = wt.HANDLE
kernel32.OpenProcess.argtypes = (wt.DWORD, wt.BOOL, wt.DWORD)
kernel32.ReadProcessMemory.restype = wt.BOOL
kernel32.ReadProcessMemory.argtypes = (wt.HANDLE, ctypes.c_void_p,
                                       ctypes.c_void_p, ctypes.c_size_t,
                                       ctypes.POINTER(ctypes.c_size_t))


def read_mem(handle, address, size):
    buf = (ctypes.c_char * size)()
    read = ctypes.c_size_t(0)
    ok = kernel32.ReadProcessMemory(handle, ctypes.c_void_p(address), buf,
                                    size, ctypes.byref(read))
    if not ok or read.value != size:
        raise OSError(f"ReadProcessMemory falló en 0x{address:08X}: "
                      f"win32={ctypes.get_last_error()} leidos={read.value}")
    return bytes(buf)


def find_module_base(pid, wanted):
    snapshot = kernel32.CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snapshot == wt.HANDLE(-1).value or not snapshot:
        raise OSError("CreateToolhelp32Snapshot falló")
    entry = MODULEENTRY32W()
    entry.dwSize = ctypes.sizeof(MODULEENTRY32W)
    found = None
    ok = kernel32.Module32FirstW(snapshot, ctypes.byref(entry))
    while ok:
        if entry.szModule.lower() == wanted.lower():
            found = entry.modBaseAddr
            break
        ok = kernel32.Module32NextW(snapshot, ctypes.byref(entry))
    kernel32.CloseHandle(snapshot)
    return found


def launch_manager(wait_after):
    print("[inforam] iniciando manager con /n …")
    proc = subprocess.Popen([fl.EXE, fl.ARGS])
    pid = proc.pid
    print(f"[inforam] pid={pid}")
    time.sleep(6.0)
    print("[inforam]", fl.click_ctrl(pid, 14021, timeout=15))
    time.sleep(0.8)
    print("[inforam]", fl.click_ctrl(pid, 12324, timeout=15))
    time.sleep(1.5)
    print("[inforam]", fl.click_ctrl(pid, 12325, timeout=8))
    print(f"[inforam] esperando {wait_after}s (arranque del emulador)…")
    time.sleep(wait_after)
    return proc, pid


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    args = sys.argv[1:]
    wait_after = float(args[args.index("--wait-segundos") + 1]) \
        if "--wait-segundos" in args else 30.0
    out_path = args[args.index("--salida") + 1] \
        if "--salida" in args else DEFAULT_OUT
    pid = int(args[args.index("--pid") + 1]) if "--pid" in args else None
    proc = None
    if pid is None:
        proc, pid = launch_manager(wait_after)
    else:
        print(f"[inforam] adjuntando a pid={pid}")

    handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                  False, pid)
    if not handle:
        print(f"[inforam] OpenProcess falló win32={ctypes.get_last_error()}")
        return 1
    base = find_module_base(pid, "GCalAppIFA.dll")
    if base is None:
        print("[inforam] GCalAppIFA.dll no encontrada en el proceso")
        return 2
    print(f"[inforam] GCalAppIFA base=0x{base:08X}")
    raw_pointer = read_mem(handle, base + GCALIFA_RVA_POINTER, 4)
    info_ram = int.from_bytes(raw_pointer, "little")
    print(f"[inforam] puntero InfoRam=0x{info_ram:08X}")
    if info_ram == 0:
        print("[inforam] puntero nulo: la emulación aún no inicializó InfoRam")
        return 3
    data = read_mem(handle, info_ram, INFORAM_BYTES)
    nonzero = sum(1 for b in data if b)
    digest = hashlib.sha256(data).hexdigest().upper()
    with open(out_path, "wb") as fh:
        fh.write(data)
    print(f"[inforam] guardado {out_path}")
    print(f"[inforam] bytes_no_cero={nonzero} sha256={digest}")
    offsets = [i for i, b in enumerate(data) if b][:40]
    print("[inforam] primeros offsets no cero:",
          " ".join(f"{o:06X}" for o in offsets))
    if proc is not None:
        print(f"[inforam] manager sigue vivo pid={pid} (cerrar a mano)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
