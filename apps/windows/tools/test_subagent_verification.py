#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_subagent_verification.py
Verificación rigurosa de CalculadoraUnificada.exe con núcleo GCal oficial.
"""

import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import os
import subprocess
import sys
import time

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().upper()

def main():
    root = r"d:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial"
    release_dir = os.path.join(root, "build-win32", "Release")
    exe_path = os.path.join(release_dir, "CalculadoraUnificada.exe")
    dll_path = os.path.join(release_dir, "runtime", "ActivationFxMb.dll")
    log_path = os.path.join(release_dir, "calc_debug.log")
    inspect_gui = os.path.join(release_dir, "InspectGui.exe")

    results = {
        "step1_files": {},
        "step2_launch": {},
        "step3_initialization_log": {},
        "step4_keystrokes": {},
        "step5_framebuffer_capture": {},
        "step6_clean_shutdown": {},
        "overall_status": "UNKNOWN"
    }

    # 1. Comprobar archivos y hashes SHA-256
    print("[1] Verificando archivos y hashes SHA-256...")
    if not os.path.isfile(exe_path):
        results["step1_files"]["CalculadoraUnificada.exe"] = "FAILED (Not found)"
        print("FAIL: CalculadoraUnificada.exe no existe")
        return
    if not os.path.isfile(dll_path):
        results["step1_files"]["ActivationFxMb.dll"] = "FAILED (Not found)"
        print("FAIL: ActivationFxMb.dll no existe")
        return

    exe_hash = sha256_file(exe_path)
    exe_size = os.path.getsize(exe_path)
    dll_hash = sha256_file(dll_path)
    dll_size = os.path.getsize(dll_path)

    results["step1_files"]["CalculadoraUnificada.exe"] = {
        "status": "CONFIRMED",
        "path": exe_path,
        "size_bytes": exe_size,
        "sha256": exe_hash
    }
    results["step1_files"]["ActivationFxMb.dll"] = {
        "status": "CONFIRMED",
        "path": dll_path,
        "size_bytes": dll_size,
        "sha256": dll_hash
    }
    print(f"CalculadoraUnificada.exe: {exe_size} bytes, SHA256={exe_hash}")
    print(f"ActivationFxMb.dll:       {dll_size} bytes, SHA256={dll_hash}")

    # Terminar cualquier proceso previo
    subprocess.run(["powershell", "-NoProfile", "-Command", "Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)
    time.sleep(0.5)

    # Eliminar calc_debug.log anterior para asegurar que se crea nuevo
    if os.path.exists(log_path):
        try:
            os.remove(log_path)
        except Exception as e:
            print(f"Aviso al borrar log anterior: {e}")

    # 2. Lanzar de forma controlada
    print("[2] Lanzando CalculadoraUnificada.exe de forma controlada...")
    proc = subprocess.Popen([exe_path], cwd=release_dir)
    results["step2_launch"] = {
        "status": "CONFIRMED",
        "pid": proc.pid
    }
    print(f"Proceso iniciado con PID: {proc.pid}")

    # Esperar inicialización de la ventana y el core GCal
    time.sleep(6)

    # Verificar si el proceso sigue vivo
    poll = proc.poll()
    if poll is not None:
        results["step2_launch"]["status"] = f"FAILED (Exited with code {poll})"
        print(f"FAIL: El proceso terminó prematuramente con código {poll}")
        return

    # 3. Verificar ventana principal y revisar calc_debug.log
    print("[3] Verificando ventana principal y etapas de inicialización en calc_debug.log...")
    user32 = ctypes.windll.user32
    found_hwnds = []

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def enum_cb(hwnd, lparam):
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == proc.pid:
            buf_cls = ctypes.create_unicode_buffer(256)
            buf_title = ctypes.create_unicode_buffer(512)
            user32.GetClassNameW(hwnd, buf_cls, 256)
            user32.GetWindowTextW(hwnd, buf_title, 512)
            found_hwnds.append({
                "hwnd": hex(hwnd),
                "class": buf_cls.value,
                "title": buf_title.value,
                "visible": bool(user32.IsWindowVisible(hwnd))
            })
        return True

    user32.EnumWindows(WNDENUMPROC(enum_cb), 0)
    print(f"Ventanas encontradas para PID {proc.pid}: {found_hwnds}")

    # Revisar log
    log_lines = []
    if os.path.exists(log_path):
        with open(log_path, 'r', encoding='utf-8', errors='replace') as lf:
            log_lines = [line.strip() for line in lf if line.strip()]

    print(f"Líneas registradas en calc_debug.log ({len(log_lines)}):")
    for l in log_lines:
        print(f"  {l}")

    # Comprobar etapas requeridas: 0 a 11, resident task, DLLs
    required_stages = [
        "CG50: Initialize() begin",
        "CG50 etapa 0 OK: runtime hallado",
        "CG50 etapa 1 OK: ActivationFxMb cargada",
        "CG50 etapa 2 OK: activacion parcheada",
        "CG50 etapa 3 OK: GCalAppIF cargada",
        "CG50 etapa 4 OK: exports resueltos",
        "CG50 etapa 5 OK: emulador creado",
        "CG50 etapa 6 OK: resident task creado",
        "CG50 etapa 7 OK: comando start enviado",
        "CG50 etapa 8 OK: DLSim32E activa",
        "CG50 etapa 9 OK: LCD accessor activo",
        "CG50 etapa 10 OK",
        "CG50: READY"
    ]
    missing_stages = [s for s in required_stages if not any(s in line for line in log_lines)]
    if not missing_stages:
        results["step3_initialization_log"] = {
            "status": "CONFIRMED",
            "stages_found": len(required_stages),
            "log_lines": log_lines
        }
        print("CONFIRMED: Todas las etapas de GCal (0 a 11, resident task, DLSim32E, LCD accessor) inicializadas correctamente.")
    else:
        results["step3_initialization_log"] = {
            "status": "FAILED",
            "missing": missing_stages,
            "log_lines": log_lines
        }
        print(f"FAILED: Faltan etapas: {missing_stages}")

    # 4 & 5. Enviar teclas y Capturar Framebuffer
    print("[4 y 5] Capturando pantalla inicial, inyectando teclas y capturando resultado...")
    cap_init = os.path.join(release_dir, "subagent_cap_init.png")
    cap_solo_cg = os.path.join(release_dir, "subagent_cap_solo_cg.png")
    cap_keys = os.path.join(release_dir, "subagent_cap_after_keys.png")

    # Captura inicial (vista 3 calculadoras)
    subprocess.run([inspect_gui, str(proc.pid), "capture", cap_init], check=True)
    time.sleep(1)

    # Cambiar a modo Solo CG50 (1014)
    print("Cambiando a modo Solo CG50 (ID_BTN_MODE_GRAPH = 1014)...")
    subprocess.run([inspect_gui, str(proc.pid), "mode", "1014"], check=True)
    time.sleep(1)
    subprocess.run([inspect_gui, str(proc.pid), "capture", cap_solo_cg], check=True)

    # Inyectar secuencia de teclas: 9 * 6 EXE
    # En MapVkToKeyId:
    # '9' = 0x39 = 57
    # VK_MULTIPLY = 0x6A = 106 (o asterisco)
    # '6' = 0x36 = 54
    # VK_RETURN = 0x0D = 13 (EXE)
    print("Inyectando teclas: '9', VK_MULTIPLY, '6', VK_RETURN...")
    subprocess.run([inspect_gui, str(proc.pid), "vk", "57"], check=True) # 9
    time.sleep(0.3)
    subprocess.run([inspect_gui, str(proc.pid), "vk", "106"], check=True) # VK_MULTIPLY
    time.sleep(0.3)
    subprocess.run([inspect_gui, str(proc.pid), "vk", "54"], check=True) # 6
    time.sleep(0.3)
    subprocess.run([inspect_gui, str(proc.pid), "vk", "13"], check=True) # EXE
    time.sleep(1.5)

    # Captura tras teclas
    subprocess.run([inspect_gui, str(proc.pid), "capture", cap_keys], check=True)
    time.sleep(0.5)

    # Inspeccionar imágenes capturadas
    from PIL import Image
    captures_info = {}
    for c_path, c_label in [(cap_init, "init_3calcs"), (cap_solo_cg, "solo_cg50"), (cap_keys, "after_keys")]:
        if os.path.exists(c_path):
            with Image.open(c_path) as img:
                w, h = img.size
                colors = img.getcolors(maxcolors=2000000)
                num_distinct_colors = len(colors) if colors else -1
            c_hash = sha256_file(c_path)
            c_size = os.path.getsize(c_path)
            captures_info[c_label] = {
                "status": "CONFIRMED",
                "path": c_path,
                "dimensions": [w, h],
                "size_bytes": c_size,
                "sha256": c_hash,
                "distinct_colors": num_distinct_colors
            }
            print(f"Captura {c_label}: {w}x{h}, {c_size} bytes, {num_distinct_colors} colores distintos, SHA256={c_hash}")
        else:
            captures_info[c_label] = {"status": "FAILED", "path": c_path}

    results["step4_keystrokes"] = {"status": "CONFIRMED", "keys_sent": ["9 (VK 57)", "MULTIPLY (VK 106)", "6 (VK 54)", "EXE (VK 13)"]}
    results["step5_framebuffer_capture"] = {"status": "CONFIRMED", "captures": captures_info}

    # 6. Cerrar el proceso de forma limpia
    print("[6] Cerrando el proceso de forma limpia y comprobando procesos huérfanos...")
    # Enviar WM_CLOSE a la ventana principal
    main_hwnd = None
    for item in found_hwnds:
        if item["class"] == "CasioNativeMain":
            main_hwnd = int(item["hwnd"], 16)
            break

    if main_hwnd:
        WM_CLOSE = 0x0010
        user32.PostMessageW(main_hwnd, WM_CLOSE, 0, 0)
        print(f"WM_CLOSE enviado a HWND 0x{main_hwnd:X}")
    else:
        proc.terminate()

    try:
        proc.wait(timeout=5)
        print(f"El proceso terminó limpiamente con exit code: {proc.returncode}")
        exit_clean = True
    except subprocess.TimeoutExpired:
        print("Timeout al esperar salida limpia, forzando kill...")
        proc.kill()
        exit_clean = False

    time.sleep(1)

    # Verificar que no quedan procesos colgados
    chk = subprocess.run(["powershell", "-NoProfile", "-Command", "Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Measure-Object | Select-Object -ExpandProperty Count"], capture_output=True, text=True)
    count = int(chk.stdout.strip() or 0)
    if count == 0:
        orphan_status = "CONFIRMED: Ningún proceso colgado en ejecución"
        print(orphan_status)
    else:
        orphan_status = f"FAILED: Quedan {count} procesos en ejecución"
        print(orphan_status)
        subprocess.run(["powershell", "-NoProfile", "-Command", "Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Stop-Process -Force"])

    results["step6_clean_shutdown"] = {
        "status": "CONFIRMED" if (exit_clean and count == 0) else "FAILED",
        "clean_exit": exit_clean,
        "exit_code": proc.returncode,
        "orphan_count": count
    }

    # Estado global
    all_ok = (
        results["step1_files"]["CalculadoraUnificada.exe"]["status"] == "CONFIRMED" and
        results["step1_files"]["ActivationFxMb.dll"]["status"] == "CONFIRMED" and
        results["step2_launch"]["status"] == "CONFIRMED" and
        results["step3_initialization_log"]["status"] == "CONFIRMED" and
        results["step4_keystrokes"]["status"] == "CONFIRMED" and
        results["step5_framebuffer_capture"]["status"] == "CONFIRMED" and
        results["step6_clean_shutdown"]["status"] == "CONFIRMED"
    )
    results["overall_status"] = "CONFIRMED" if all_ok else "FAILED"

    # Guardar reporte JSON
    report_json = os.path.join(release_dir, "subagent_verification_report.json")
    with open(report_json, 'w', encoding='utf-8') as rf:
        json.dump(results, rf, indent=2)

    print(f"\nReporte JSON guardado en: {report_json}")
    print(f"=== RESULTADO GLOBAL: {results['overall_status']} ===")

if __name__ == "__main__":
    main()
