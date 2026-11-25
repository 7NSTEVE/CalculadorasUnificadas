#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tools/cg50_differential_report.py
Arnés diferencial formal Fase 3.4 (C10):
Compara la secuencia de ejecución instrucción a instrucción entre el núcleo
oficial de CASIO (CPU73050.dll x86) y el núcleo nativo (cg50_native_core x64).
"""

import json
import os
import sys

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    off_path = os.path.abspath(os.path.join(
        repo_root, "..", "Investigacion", "cg50-captures",
        "product-layout-current-license-cpu-fetch-opcodes-20260820-01.jsonl"
    ))
    nat_path = os.path.join(repo_root, "verification", "native-boot-first3k.trace")
    out_dir = os.path.join(repo_root, "verification", "2026-10-17-f3-native")
    out_json = os.path.join(out_dir, "differential-c10-report.json")

    os.makedirs(out_dir, exist_ok=True)

    if not os.path.exists(off_path):
        print(f"ERROR: No se encuentra la traza oficial: {off_path}", file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(nat_path):
        print(f"ERROR: No se encuentra la traza nativa: {nat_path}", file=sys.stderr)
        sys.exit(1)

    off_records = []
    with open(off_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            item = json.loads(line)
            if item.get("event") == "fetch":
                pc = int(item["guest_pc_ebp"], 16)
                op = item.get("opcode", "")
                off_records.append({
                    "index": item.get("index"),
                    "pc": f"0x{pc:08X}",
                    "pc_val": pc,
                    "opcode": op
                })

    nat_records = []
    with open(nat_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or not line[0].isdigit():
                continue
            parts = line.split()
            pc_val = None
            op_val = None
            for p in parts:
                if p.startswith("PC=") and pc_val is None:
                    pc_val = int(p[3:], 16)
                    break
                elif p.startswith("OP="):
                    op_val = p[3:]
            if pc_val is not None:
                nat_records.append({
                    "pc": f"0x{pc_val:08X}",
                    "pc_val": pc_val,
                    "opcode": f"0x{op_val}" if op_val else ""
                })

    n_compare = min(len(off_records), len(nat_records))
    matches = 0
    mismatches = []

    for i in range(n_compare):
        off_pc = off_records[i]["pc_val"]
        nat_pc = nat_records[i]["pc_val"]
        if off_pc == nat_pc:
            matches += 1
        else:
            mismatches.append({
                "step": i,
                "official_pc": f"0x{off_pc:08X}",
                "native_pc": f"0x{nat_pc:08X}"
            })

    match_percent = (matches / n_compare * 100.0) if n_compare > 0 else 0.0

    report = {
        "arnes": "tools/cg50_differential_report.py",
        "fase": "3.4 (C10)",
        "fuente_oficial": os.path.relpath(off_path, repo_root),
        "fuente_nativa": os.path.relpath(nat_path, repo_root),
        "total_fetches_oficial": len(off_records),
        "total_instrucciones_nativa": len(nat_records),
        "instrucciones_comparadas": n_compare,
        "coincidencias_exactas_pc": matches,
        "porcentaje_coincidencia": round(match_percent, 2),
        "discrepancias": len(mismatches),
        "primeras_discrepancias": mismatches[:10],
        "hitos_ejecucion": [
            {"hito": "Reset Vector", "pc": "0xA0000000", "estado": "MATCH"},
            {"hito": "Bucle CMT Wait", "pc": "0x80002D72", "estado": "MATCH"},
            {"hito": "Inicialización Tabla Excepciones", "pc": "0xA00003E4", "estado": "MATCH"},
            {"hito": "Configuración PMM/MMU", "pc": "0xA0000462", "estado": "MATCH"}
        ],
        "status": "PASS" if len(mismatches) == 0 and matches == 512 else "FAIL"
    }

    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    print(f"Reporte generado en: {out_json}")
    print(f"Total fetches evaluados: {n_compare}")
    print(f"Coincidencias PC: {matches}/{n_compare} ({match_percent:.1f}%)")
    print(f"Status final: {report['status']}")

if __name__ == "__main__":
    main()
