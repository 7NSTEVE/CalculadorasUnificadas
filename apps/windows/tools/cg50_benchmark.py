#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cg50_benchmark.py — Benchmark de rendimiento (MIPS) del intérprete nativo SH7305.

P0 del INFORME-REQUERIMIENTOS-EMULADOR-REAL-CG50-2026-10-17.md (§4 fila 12):
«Rendimiento — medir MIPS del intérprete; para iOS meta ≥10-20M instr/s».

Método (evidencia mínima y reproducible):
  1. Modo boot-idle: Cg50RomProbe con SystemROM/UserROM canónicas y presupuesto
     alto SIN teclas. El core ejecuta el arranque real del OS hasta el primer
     SLEEP. MIPS = instrucciones ejecutadas / tiempo de pared.
  2. Modo interactivo: misma corrida con --key-on-sleep (24 × AC/ON) para
     medir el intérprete también sobre los segmentos de procesado de teclas
     (IRQ KIU 0x0BE0 + redibujado del OS).

El cuello de botella medido es el bucle StepInstruction() del core
(cg50_native_core.cpp) sobre el workload REAL del OS 03.80.0212 — no un bucle
sintético. La salida se escribe en JSON + MD para asiento en el registro.

Uso:
    python tools/cg50_benchmark.py --label <etiqueta> [--keys 24] [--budget 200000000]
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime, timezone

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
PROBE = os.path.join(
    PROJECT_ROOT, "third_party", "rom-native-reference",
    "build-probe-x64", "Release", "Cg50RomProbe.exe")
ROMS_DIR = os.path.join(PROJECT_ROOT, "build-native-x64", "Release", "runtime")
SYSTEM_ROM = os.path.join(ROMS_DIR, "SystemROM.raw")
USER_ROM = os.path.join(ROMS_DIR, "UserROM.raw")
DEFAULT_OUT_DIR = os.path.join(
    PROJECT_ROOT, "verification", "2026-10-17-p0-benchmark")

EXECUTADAS_RE = re.compile(r"ejecutadas=(\d+)")
CICLOS_RE = re.compile(r"ciclos~=(\d+)")
INTEC_RE = re.compile(r"\[INTC\] aceptadas=(\d+).*ultimo_evento=0x([0-9A-Fa-f]{8})")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def require_file(path, what):
    if not os.path.isfile(path):
        print(f"[ERROR] Falta {what}: {path}", file=sys.stderr)
        sys.exit(2)


def run_probe(out_dir, tag, budget, keys):
    """Corre el probe una vez y devuelve el resultado medido."""
    stdout_path = os.path.join(out_dir, f"{tag}.txt")
    stderr_path = os.path.join(out_dir, f"{tag}.err.txt")
    args = [PROBE, SYSTEM_ROM, "--userrom", USER_ROM,
            "--instructions", str(budget)]
    if keys > 0:
        args += ["--key-on-sleep", ",".join(["0x0101"] * keys)]

    t0 = time.perf_counter()
    with open(stdout_path, "wb") as out, open(stderr_path, "wb") as err:
        proc = subprocess.run(args, stdout=out, stderr=err, cwd=out_dir)
    wall = time.perf_counter() - t0

    text = open(stdout_path, "r", encoding="utf-8", errors="replace").read()
    m = EXECUTADAS_RE.search(text)
    if not m:
        print(f"[ERROR] No se encontró 'ejecutadas=' en {stdout_path}",
              file=sys.stderr)
        sys.exit(3)
    executed = int(m.group(1))
    cycles = 0
    mc = CICLOS_RE.search(text)
    if mc:
        cycles = int(mc.group(1))
    intc = INTEC_RE.search(text)
    interrupts = int(intc.group(1)) if intc else 0
    last_irq = int(intc.group(2), 16) if intc else 0

    return {
        "tag": tag,
        "exit_code": proc.returncode,
        "wall_seconds": round(wall, 3),
        "executed_instructions": executed,
        "approx_cycles": cycles,
        "mips": round(executed / wall / 1_000_000.0, 2),
        "instructions_per_cycle": round(executed / max(cycles, 1), 4),
        "interrupts_accepted": interrupts,
        "last_interrupt_event_hex": f"0x{last_irq:08X}",
        "sleep_at_end": "[SLEEP]" in text,
        "stdout_file": os.path.relpath(stdout_path, PROJECT_ROOT),
        "command": " ".join(os.path.basename(a) if i == 0 else a
                            for i, a in enumerate(args)),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--label", required=True,
                        help="Etiqueta de la corrida (p.ej. baseline-pre-cambios)")
    parser.add_argument("--budget", type=int, default=200_000_000)
    parser.add_argument("--keys", type=int, default=24)
    parser.add_argument("--key-budget", type=int, default=260_000_000)
    parser.add_argument("--out-dir", default=DEFAULT_OUT_DIR)
    args = parser.parse_args()

    require_file(PROBE, "Cg50RomProbe.exe")
    require_file(SYSTEM_ROM, "SystemROM.raw canónica")
    require_file(USER_ROM, "UserROM.raw canónica")

    os.makedirs(args.out_dir, exist_ok=True)

    boot = run_probe(args.out_dir, f"{args.label}-boot-idle",
                     args.budget, keys=0)
    keys_run = run_probe(args.out_dir, f"{args.label}-teclas",
                         args.key_budget, keys=args.keys)

    result = {
        "tool": os.path.relpath(__file__, PROJECT_ROOT),
        "label": args.label,
        "utc_time": datetime.now(timezone.utc).isoformat(),
        "probe_exe": os.path.relpath(PROBE, PROJECT_ROOT),
        "probe_sha256": sha256_file(PROBE),
        "system_rom": os.path.relpath(SYSTEM_ROM, PROJECT_ROOT),
        "system_rom_sha256": sha256_file(SYSTEM_ROM),
        "user_rom_sha256": sha256_file(USER_ROM),
        "runs": [boot, keys_run],
        "ios_target_mips": [10_000_000, 20_000_000],
        "meets_ios_lower_target": boot["mips"] >= 10.0,
    }

    json_path = os.path.join(args.out_dir, f"{args.label}.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    md_path = os.path.join(args.out_dir, f"{args.label}.md")
    with open(md_path, "w", encoding="utf-8") as f:
        f.write(f"# Benchmark MIPS del intérprete nativo — {args.label}\n\n")
        f.write(f"UTC: {result['utc_time']}\n\n")
        f.write(f"- Probe: `{result['probe_exe']}` SHA-256 `{result['probe_sha256']}`\n")
        f.write(f"- SystemROM: `{result['system_rom']}` SHA-256 `{result['system_rom_sha256'][:16]}…`\n")
        f.write(f"- UserROM SHA-256 `{result['user_rom_sha256'][:16]}…`\n\n")
        f.write("| Modo | Instrucciones | Tiempo (s) | MIPS | ≈ciclos | IRQ aceptadas |\n")
        f.write("|---|---:|---:|---:|---:|---:|\n")
        for run in result["runs"]:
            f.write(f"| {run['tag']} | {run['executed_instructions']:,} "
                    f"| {run['wall_seconds']} | {run['mips']} "
                    f"| {run['approx_cycles']:,} | {run['interrupts_accepted']} |\n")
        f.write(f"\nMeta iOS (≥10–20M instr/s): "
                f"{'CUMPLE el umbral inferior' if result['meets_ios_lower_target'] else 'POR DEBAJO del umbral inferior'}"
                f" con {boot['mips']}M instr/s en boot-idle.\n")
        f.write("\nMétodo: workload real del OS 03.80.0212 (boot hasta primer SLEEP; "
                "teclas 0x0101 inyectadas en cada SLEEP para el modo interactivo). "
                "Sin bucles sintéticos; el cuello medido es el bucle StepInstruction() "
                "del core sobre ROM real.\n")

    print(json.dumps(result["runs"], indent=2, ensure_ascii=False))
    print(f"[OUT] {json_path}")
    print(f"[OUT] {md_path}")


if __name__ == "__main__":
    main()
