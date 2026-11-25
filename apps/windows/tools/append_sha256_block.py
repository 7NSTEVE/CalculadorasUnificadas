#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
append_sha256_block.py — Anexa un bloque de hashes de evidencia nueva al
SHA256SUMS.txt del proyecto (formato: hash  tamaño  ruta).

Uso: python append_sha256_block.py
Herramienta de retención del proyecto Calculadora Unificada (2026-10-16).
"""
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VER = os.path.join(ROOT, "verification", "2026-10-16")
SUMS = os.path.join(VER, "SHA256SUMS.txt")

RELATIVE = [
    "gcal-host-probe-power-0x10-fix-20261016.log",
    "gcal-host-probe-power-0x10-fix-20261016.veh.log",
    "gcal-host-probe-power1-legacy-fix-20261016.log",
    "gcal-host-probe-power1-legacy-fix-20261016.veh.log",
    "gcal-host-probe-powerfirst-slots-20261016.log",
    "gcal-host-probe-powerfirst-slots-20261016.veh.log",
    "ctest-after-power-fix-20261016.log",
    "gui-verify-solo-2026-10-16.png",
    "gui-verify-after-ac-2026-10-16.png",
    "gui-verify-lcd-before-2026-10-16.png",
    "gui-verify-lcd-after-2026-10-16.png",
    "gui-verify-lcd-fresh-2026-10-16.png",
    "gui-fullscreen-state-2026-10-16.png",
    "gui-window-state-2026-10-16.png",
    "gui-v2-lcd-before.png",
    "gui-v2-lcd-after.png",
    "gui-v3-canvas-antes.png",
    "gui-v3-canvas-despues.png",
    "gui-v3-lcd-before.png",
    "gui-v3-lcd-after.png",
    "gui-v3-cg50-lcd-allthree-antes.png",
    "gui-v3-cg50-lcd-allthree-despues.png",
    "gui-ab-canvas-antes.png",
    "gui-ab-canvas-despues.png",
    "gui-ab-cg50-lcd-antes.png",
    "gui-ab-cg50-lcd-despues.png",
    "_seccion16_informe.tmp.md",
    "_entrada30_registro.tmp.md",
]

TOOLS = [
    "disasm_pe.py", "find_xrefs.py", "dump_log.py", "lcd_image_stats.py",
    "img_diff.py", "lcd_surface_map.py", "gui_power_verification.py",
    "gui_power_verification2.py", "gui_power_verification3.py",
    "gui_power_final_ab.py", "crop_cg50_allthree.py", "grab_full.py",
    "grab_app_window.py", "gui_lcd_fresh_capture.py",
    "extract_rom_native_reference.py", "analyze_session_json.py",
    "verify_backups.py", "backup_before_edit.py", "hash_files.py",
    "check_snapshot_state.py",
]

PROJECT = [
    "cg50_backend.cpp",
    "tools/gcal-host-probe/main.cpp",
    "build-verify/Release/CalculadoraUnificada.exe",
    "build-verify/Release/Cg50BackendContractTest.exe",
    "build-verify/Release/GCalHostProbe.exe",
    "third_party/rom-native-reference/MANIFEST.txt",
    "INFORME-VERIFICACION-2026-10-16.md",
]


def sha(path):
    data = open(path, "rb").read()
    return hashlib.sha256(data).hexdigest(), len(data)


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    lines = []
    lines.append("")
    lines.append("## Sesión power-fix + ROM-native (Servulus/GLM, 2026-10-16; REGISTRO_CAMBIOS #30)")
    lines.append("")
    lines.append("### Evidencia verification/2026-10-16/")
    for rel in RELATIVE:
        path = os.path.join(VER, rel)
        if not os.path.exists(path):
            continue
        digest, size = sha(path)
        lines.append(f"{digest}  {size:>9} B  verification\\2026-10-16\\{rel}")
    lines.append("")
    lines.append("### Herramientas nuevas (tools/)")
    for name in TOOLS:
        path = os.path.join(ROOT, "tools", name)
        if not os.path.exists(path):
            continue
        digest, size = sha(path)
        lines.append(f"{digest}  {size:>9} B  tools\\{name}")
    lines.append("")
    lines.append("### Archivos del proyecto modificados/clave post-fix")
    for rel in PROJECT:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue
        digest, size = sha(path)
        lines.append(f"{digest}  {size:>9} B  {rel}")
    lines.append("")
    with open(SUMS, "a", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    print(f"BLOQUE_ANEXADO a {SUMS}")
    for line in lines:
        if line.strip():
            print(" ", line)


if __name__ == "__main__":
    sys.exit(main())
