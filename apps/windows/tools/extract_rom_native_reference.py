#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
extract_rom_native_reference.py — Extracción de solo-lectura de los archivos
clave de la rama agent/cg50-native-rom-core hacia
third_party/rom-native-reference/ (sin checkout, sin git-lfs, sin tocar la
linea GCal verificada).

Uso: python tools/extract_rom_native_reference.py
Herramienta de integración del proyecto Calculadora Unificada (2026-10-16).
"""
import hashlib
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRANCH = "agent/cg50-native-rom-core"
SAFE = (
    "E:/Descompilados/11_Otros/Emuladores/"
    "Calculadoras_blanca_y_gris/Calculadora Unificada"
)
DEST = os.path.join(ROOT, "third_party", "rom-native-reference")

# (ruta_en_la_rama, nombre_destino)
FILES = [
    ("Calculadora Unificada/cg50_native_core.cpp", "cg50_native_core.cpp"),
    ("Calculadora Unificada/cg50_native_core.h", "cg50_native_core.h"),
    ("Calculadora Unificada/cg50_native_runtime.cpp", "cg50_native_runtime.cpp"),
    ("Calculadora Unificada/cg50_trace.cpp", "cg50_trace.cpp"),
    ("Calculadora Unificada/cg50_trace.h", "cg50_trace.h"),
    ("Calculadora Unificada/cg50_trace_diff.h", "cg50_trace_diff.h"),
    ("Calculadora Unificada/cg50_trace_tail.h", "cg50_trace_tail.h"),
    ("Calculadora Unificada/cg50_stop_info.h", "cg50_stop_info.h"),
    ("Calculadora Unificada/cg50_debug_irq.h", "cg50_debug_irq.h"),
    ("Calculadora Unificada/cg50_firmware_pairing.h", "cg50_firmware_pairing.h"),
    ("Calculadora Unificada/cg50_oracle_capture.h", "cg50_oracle_capture.h"),
    ("Calculadora Unificada/cg50_oracle_probe_log.h", "cg50_oracle_probe_log.h"),
    ("Calculadora Unificada/cg50_oracle_rom_gate.h", "cg50_oracle_rom_gate.h"),
    ("Calculadora Unificada/cg50_backend_file.cpp", "cg50_backend_file.cpp"),
    ("Calculadora Unificada/cg50_backend.cpp", "rama-cg50_backend.cpp"),
    ("Calculadora Unificada/cg50_backend.h", "rama-cg50_backend.h"),
    ("Calculadora Unificada/CMakeLists.txt", "rama-CMakeLists.txt"),
    ("Calculadora Unificada/tests/cg50_backend_contract_test.cpp",
     "rama-tests-cg50_backend_contract_test.cpp"),
    ("PLAN-CG50.md", "PLAN-CG50.md"),
    ("PENDIENTE.md", "PENDIENTE.md"),
    ("BACKLOG.md", "BACKLOG.md"),
    # --- F2.0 (sesion 2, 2026-10-16): herramientas de la rama ---
    ("Calculadora Unificada/tools/gcal-trace/inspect_dlsim_mem.py",
     "tools-rama/gcal-trace/inspect_dlsim_mem.py"),
    ("Calculadora Unificada/tools/gcal-trace/test_inspect_dlsim_mem.py",
     "tools-rama/gcal-trace/test_inspect_dlsim_mem.py"),
    ("Calculadora Unificada/tools/gcal-trace/inspect_pe_imports.py",
     "tools-rama/gcal-trace/inspect_pe_imports.py"),
    ("Calculadora Unificada/tools/gcal-trace/inspect_pe_exports.py",
     "tools-rama/gcal-trace/inspect_pe_exports.py"),
    ("Calculadora Unificada/tools/gcal-trace/trace_dlsim_pipeline.py",
     "tools-rama/gcal-trace/trace_dlsim_pipeline.py"),
    ("Calculadora Unificada/tools/gcal-trace/analyze_dlsim_pipeline.py",
     "tools-rama/gcal-trace/analyze_dlsim_pipeline.py"),
    ("Calculadora Unificada/tools/gcal-trace/trace_gcal_api.js",
     "tools-rama/gcal-trace/trace_gcal_api.js"),
    ("Calculadora Unificada/tools/gcal-trace/run_frida_script.py",
     "tools-rama/gcal-trace/run_frida_script.py"),
    ("Calculadora Unificada/tools/gcal-trace/cg50_lfs_manifest.py",
     "tools-rama/gcal-trace/cg50_lfs_manifest.py"),
    ("Calculadora Unificada/tools/gcal-trace/analyze_cpu_memory_trace.py",
     "tools-rama/gcal-trace/analyze_cpu_memory_trace.py"),
    ("Calculadora Unificada/tools/inspect-cg50-variants.ps1",
     "tools-rama/inspect-cg50-variants.ps1"),
    ("Calculadora Unificada/tools/capture-cg50-oracle.ps1",
     "tools-rama/capture-cg50-oracle.ps1"),
    ("Tools/analysis/generar_proxy_dlsim.py",
     "tools-rama/analysis/generar_proxy_dlsim.py"),
    ("Tools/analysis/test_generar_proxy_dlsim.py",
     "tools-rama/analysis/test_generar_proxy_dlsim.py"),
    ("Calculadora Unificada/compilar_cg50_x64.bat", "compilar_cg50_x64.bat"),
    # --- F2.7 / Fase 3: componentes del core nativo y probe ---
    ("Calculadora Unificada/cg50_core/cg50_board_wiring.h", "cg50_core/cg50_board_wiring.h"),
    ("Calculadora Unificada/cg50_core/cg50_keyboard_matrix.cpp", "cg50_core/cg50_keyboard_matrix.cpp"),
    ("Calculadora Unificada/cg50_core/cg50_keyboard_matrix.h", "cg50_core/cg50_keyboard_matrix.h"),
    ("Calculadora Unificada/cg50_core/cg50_peripheral_bus.cpp", "cg50_core/cg50_peripheral_bus.cpp"),
    ("Calculadora Unificada/cg50_core/cg50_peripheral_bus.h", "cg50_core/cg50_peripheral_bus.h"),
    ("Calculadora Unificada/cg50_core/r61524.cpp", "cg50_core/r61524.cpp"),
    ("Calculadora Unificada/cg50_core/r61524.h", "cg50_core/r61524.h"),
    ("Calculadora Unificada/cg50_core/sh_address.h", "cg50_core/sh_address.h"),
    ("Calculadora Unificada/cg50_core/sh_control.h", "cg50_core/sh_control.h"),
    ("Calculadora Unificada/cg50_core/sh_divide.h", "cg50_core/sh_divide.h"),
    ("Calculadora Unificada/cg50_core/sh_exception.cpp", "cg50_core/sh_exception.cpp"),
    ("Calculadora Unificada/cg50_core/sh_exception.h", "cg50_core/sh_exception.h"),
    ("Calculadora Unificada/cg50_core/sh_integer.h", "cg50_core/sh_integer.h"),
    ("Calculadora Unificada/cg50_core/sh_interrupt.cpp", "cg50_core/sh_interrupt.cpp"),
    ("Calculadora Unificada/cg50_core/sh_interrupt.h", "cg50_core/sh_interrupt.h"),
    ("Calculadora Unificada/cg50_core/sh_mac.h", "cg50_core/sh_mac.h"),
    ("Calculadora Unificada/cg50_core/sh_memory.h", "cg50_core/sh_memory.h"),
    ("Calculadora Unificada/cg50_core/sh_multiply.h", "cg50_core/sh_multiply.h"),
    ("Calculadora Unificada/cg50_core/sh_stack.h", "cg50_core/sh_stack.h"),
    ("Calculadora Unificada/tools/cg50-rom-probe/main.cpp", "tools/cg50-rom-probe/main.cpp"),
]


def git(*args):
    result = subprocess.run(
        ["git", "-c", f"safe.directory={SAFE}"] + list(args),
        cwd=ROOT, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"git {args} fallo: {result.stderr.decode('utf-8', 'replace')}")
    return result.stdout


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    os.makedirs(DEST, exist_ok=True)
    commit = git("rev-parse", BRANCH).decode().strip()
    print(f"Rama: {BRANCH} commit={commit}")
    manifest = []
    for branch_path, dest_name in FILES:
        data = git("show", f"{BRANCH}:{branch_path}")
        dest = os.path.join(DEST, dest_name)
        parent = os.path.dirname(dest)
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(dest, "wb") as fh:
            fh.write(data)
        sha = hashlib.sha256(data).hexdigest()
        manifest.append((branch_path, dest_name, len(data), sha))
        print(f"  {dest_name:44s} {len(data):8d} B  {sha[:16]}…  <- {branch_path}")
    with open(os.path.join(DEST, "MANIFEST.txt"), "w", encoding="utf-8") as fh:
        fh.write(
            "# Manifiesto de extraccion rom-native-reference\n"
            f"# Rama: {BRANCH}\n"
            f"# Commit: {commit}\n"
            "# Fecha: 2026-10-16\n"
            "# Metodo: git show (solo lectura; sin checkout, sin git-lfs;\n"
            "#   los binarios LFS de la rama NO estan materializados aqui)\n"
            "# Destino: third_party/rom-native-reference/\n\n")
        for branch_path, dest_name, size, sha in manifest:
            fh.write(f"{sha}  {size:8d}  {dest_name}  <- {branch_path}\n")
    print(f"MANIFEST escrito: {os.path.join(DEST, 'MANIFEST.txt')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
