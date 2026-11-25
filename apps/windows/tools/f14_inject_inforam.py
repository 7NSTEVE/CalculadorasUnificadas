#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
f14_inject_inforam.py — A6 (2026-10-16): inyecta un byte en el InfoRam del
probe res0e para probar si InfoRam+0x2FF=0xAA (escrito por el ROM oficial a
t=4.871 s y ausente en nuestro guest) es la compuerta que habilita el MAIN
MENU en la app propia.

Lanza GCalHostProbe.exe en modo res0e, localiza GCalAppIFA.dll en el proceso,
lee el puntero InfoRam global de base+0x9D64, escribe --valor en ptr+--offset
cada --intervalo-ms hasta que el probe termina, y registra cada cambio de la
ventana ptr+0x2E0..0x340. Al final analiza gcal-info-ram-res0e2.bin y
lcd-image-res0e2.bin y exporta PNG del LCD (si PIL está disponible).

Uso:
  python f14_inject_inforam.py [--hold MS] [--valor 0xAA] [--offset 0x2FF]
                               [--intervalo-ms 150]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import os
import subprocess
import sys
import time

PROJECT = (r"E:\Descompilados\11_Otros\Emuladores\Calculadoras_blanca_y_gris"
           r"\Calculadora Unificada")
PROBE = os.path.join(PROJECT, "build-probe32", "Release", "GCalHostProbe.exe")
RUNTIME = os.path.join(PROJECT, "build-verify", "Release", "runtime")
VERIF = os.path.join(PROJECT, "verification", "2026-10-16-f1-dispatcher")

kernel32 = ctypes.windll.kernel32

PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
PROCESS_VM_WRITE = 0x0020
PROCESS_VM_OPERATION = 0x0008
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010

GCALIFA_RVA_POINTER = 0x9D64
WINDOW_BASE = 0x2E0
WINDOW_SIZE = 0x60


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
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.OpenProcess.argtypes = (wt.DWORD, wt.BOOL, wt.DWORD)
kernel32.ReadProcessMemory.restype = wt.BOOL
kernel32.ReadProcessMemory.argtypes = (wt.HANDLE, ctypes.c_void_p,
                                       ctypes.c_void_p, ctypes.c_size_t,
                                       ctypes.POINTER(ctypes.c_size_t))
kernel32.WriteProcessMemory.restype = wt.BOOL
kernel32.WriteProcessMemory.argtypes = (wt.HANDLE, ctypes.c_void_p,
                                        ctypes.c_void_p, ctypes.c_size_t,
                                        ctypes.POINTER(ctypes.c_size_t))


def find_module_base(pid, wanted):
    snapshot = kernel32.CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snapshot == wt.HANDLE(-1).value or not snapshot:
        return None
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


def read_mem(handle, address, size):
    buf = (ctypes.c_char * size)()
    read = ctypes.c_size_t(0)
    ok = kernel32.ReadProcessMemory(handle, ctypes.c_void_p(address), buf,
                                    size, ctypes.byref(read))
    if not ok or read.value != size:
        return None
    return bytes(buf)


def write_mem(handle, address, data):
    buf = ctypes.create_string_buffer(data, len(data))
    written = ctypes.c_size_t(0)
    ok = kernel32.WriteProcessMemory(handle, ctypes.c_void_p(address), buf,
                                     len(data), ctypes.byref(written))
    return bool(ok) and written.value == len(data)


def read_u32(handle, address):
    raw = read_mem(handle, address, 4)
    return None if raw is None else int.from_bytes(raw, "little")


def log(msg):
    print(msg, flush=True)


def analyze_outputs():
    info_path = os.path.join(RUNTIME, "gcal-info-ram-res0e2.bin")
    lcd_path = os.path.join(RUNTIME, "lcd-image-res0e2.bin")
    log("=== POST: info-ram ===")
    if os.path.exists(info_path):
        with open(info_path, "rb") as fh:
            data = fh.read()
        log(f"[post] {info_path} bytes={len(data)} "
            f"sha256={hashlib.sha256(data).hexdigest().upper()[:16]}")
        for off in range(0x2E0, 0x340, 16):
            chunk = data[off:off + 16]
            log(f"[post] {off:04X}: " +
                " ".join(f"{b:02X}" for b in chunk))
    else:
        log("[post] gcal-info-ram-res0e2.bin ausente")
    log("=== POST: lcd ===")
    if os.path.exists(lcd_path):
        with open(lcd_path, "rb") as fh:
            lcd = fh.read()
        log(f"[post] {lcd_path} bytes={len(lcd)} "
            f"sha256={hashlib.sha256(lcd).hexdigest().upper()[:16]}")
        nonzero = sum(1 for b in lcd if b)
        log(f"[post] lcd bytes_no_cero={nonzero}")
        try:
            from PIL import Image
            img = Image.frombytes("RGBA", (1024, 512), lcd)
            img = img.convert("RGB")
            full_png = os.path.join(VERIF, "lcd-inject0xaa-full-20261016.png")
            img.save(full_png)
            crop = img.crop((0, 0, 396, 224))
            crop_png = os.path.join(VERIF, "lcd-inject0xaa-crop-20261016.png")
            crop.save(crop_png)
            log(f"[post] PNG guardados: {full_png} / {crop_png}")
            colors = {tuple(px) for px in crop.getdata()}
            log(f"[post] colores distintos en crop 396x224: {len(colors)}")
        except ImportError:
            log("[post] PIL no disponible: sin PNG")
    else:
        log("[post] lcd-image-res0e2.bin ausente")


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser()
    parser.add_argument("--hold", type=int, default=3000)
    parser.add_argument("--valor", type=lambda s: int(s, 0), default=0xAA)
    parser.add_argument("--offset", type=lambda s: int(s, 0), default=0x2FF)
    parser.add_argument("--intervalo-ms", type=int, default=150)
    parser.add_argument("--sin-analisis", action="store_true")
    parser.add_argument("--log", default=os.path.join(
        VERIF, "probe-inject0xaa-20261016.log"))
    args = parser.parse_args()

    for required in (PROBE, RUNTIME):
        if not os.path.exists(required):
            log(f"[inject] falta {required}")
            return 2

    log(f"[inject] lanzando probe res0e hold={args.hold} "
        f"valor=0x{args.valor:02X} offset=0x{args.offset:X} "
        f"intervalo={args.intervalo_ms}ms")
    log_stderr = args.log + ".stderr.txt"
    with open(args.log, "wb") as out, open(log_stderr, "wb") as err:
        proc = subprocess.Popen(
            [PROBE, RUNTIME, str(args.hold), "0", "res0e"],
            stdout=out, stderr=err, cwd=RUNTIME)
    log(f"[inject] pid={proc.pid} log={args.log}")

    handle = kernel32.OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
        PROCESS_VM_OPERATION, False, proc.pid)
    if not handle:
        log(f"[inject] OpenProcess falló win32={ctypes.get_last_error()}")
        return 3

    t0 = time.time()
    base = None
    while time.time() - t0 < 30.0 and proc.poll() is None:
        base = find_module_base(proc.pid, "GCalAppIFA.dll")
        if base:
            break
        time.sleep(0.2)
    if not base:
        log("[inject] GCalAppIFA.dll no encontrada (timeout/exit)")
        if not args.sin_analisis:
            analyze_outputs()
        return 4
    log(f"[inject] GCalAppIFA base=0x{base:08X} t={time.time()-t0:.2f}s")

    info_ram = None
    while time.time() - t0 < 30.0 and proc.poll() is None:
        info_ram = read_u32(handle, base + GCALIFA_RVA_POINTER)
        if info_ram:
            break
        time.sleep(0.1)
    if not info_ram:
        log("[inject] puntero InfoRam nulo (timeout/exit)")
        if not args.sin_analisis:
            analyze_outputs()
        return 5
    log(f"[inject] puntero InfoRam=0x{info_ram:08X} t={time.time()-t0:.2f}s")

    window_at = info_ram + WINDOW_BASE
    target_at = info_ram + args.offset
    prev = read_mem(handle, window_at, WINDOW_SIZE)
    if prev is not None:
        log(f"[inject] ventana inicial {WINDOW_BASE:03X}: " +
            " ".join(f"{b:02X}" for b in prev))
    writes = 0
    write_failures = 0
    ticks = 0
    while proc.poll() is None:
        now = read_mem(handle, window_at, WINDOW_SIZE)
        if now is not None and prev is not None and now != prev:
            for i, (a, b) in enumerate(zip(prev, now)):
                if a != b:
                    log(f"[inject] t={time.time()-t0:6.2f}s diff @"
                        f"0x{WINDOW_BASE+i:03X}: {a:02X}->{b:02X}")
            prev = now
        elif now is not None:
            prev = now
        if write_mem(handle, target_at, bytes([args.valor])):
            writes += 1
        else:
            write_failures += 1
        ticks += 1
        if ticks % 20 == 0:
            cur = read_mem(handle, target_at, 1)
            cur_txt = f"{cur[0]:02X}" if cur else "??"
            log(f"[inject] t={time.time()-t0:6.2f}s writes={writes} "
                f"valor_actual@0x{args.offset:X}={cur_txt}")
        time.sleep(args.intervalo_ms / 1000.0)

    exit_code = proc.returncode
    log(f"[inject] probe terminó exit={exit_code} t={time.time()-t0:.2f}s "
        f"writes={writes} fallos={write_failures}")

    final = read_mem(handle, window_at, WINDOW_SIZE)
    if final is not None:
        log("[inject] ventana final: " +
            " ".join(f"{b:02X}" for b in final))
    kernel32.CloseHandle(handle)

    if not args.sin_analisis:
        analyze_outputs()
    log("[inject] fin")
    return 0


if __name__ == "__main__":
    sys.exit(main())
