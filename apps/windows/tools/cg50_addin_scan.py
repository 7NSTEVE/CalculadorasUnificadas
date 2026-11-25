#!/usr/bin/env python3
"""Escaneo estatico de add-ins .g3a de la fx-CG50 (plan N-11, etapa 0)."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT_DIR = ROOT / "verification" / "gcal-host-runtime-original"
DEFAULT_OUT_DIR = ROOT / "verification" / "2026-10-19-n11-addins"
DEFAULT_SYSCALL_TABLE = ROOT / "verification" / "2026-10-17-f6-abi" / "syscall_table.json"

DEFAULT_FILES = (
    "3DGraph.g3a",
    "Conv.g3a",
    "Geometry.g3a",
    "Physium.g3a",
    "PictPlot.g3a",
    "ProbSim.g3a",
)

PAYLOAD_OFFSET = 0x7000
BASE_VA = 0x00300000
DISPATCHER_VA = 0x80020070
SYSCALL_LIMIT = 8040
STUB_SIZE = 16
MAGIC = bytes.fromhex("aaacbdaf90889a8d")
DISPATCHER_LITERAL = struct.pack(">I", DISPATCHER_VA)

SCHEMA_VERSION = 1


def fmt_hex(value: int, width: int = 6) -> str:
    return f"0x{value:0{width}X}"


def fmt_va(value: int) -> str:
    return fmt_hex(value, 8)


def relpath(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_syscall_table(path: Path):
    info = {"path": relpath(path), "available": False}
    if not path.is_file():
        info["reason"] = "archivo no encontrado"
        return info, None
    info["sha256"] = sha256_file(path)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        info["reason"] = f"lectura fallida: {exc}"
        return info, None
    raw_entries = data.get("entries") if isinstance(data, dict) else None
    if not isinstance(raw_entries, list):
        info["reason"] = "estructura sin lista 'entries'"
        return info, None
    table = {}
    invalid = []
    for position, item in enumerate(raw_entries):
        try:
            index = int(item["index"])
            target = item["target"]
            target = target if isinstance(target, int) else int(str(target), 16)
            table[index] = target
        except (KeyError, TypeError, ValueError):
            invalid.append(position)
    metadata = data.get("metadata") if isinstance(data.get("metadata"), dict) else {}
    info.update(
        {
            "available": True,
            "base": data.get("base"),
            "count": data.get("count", len(raw_entries)),
            "entries_loaded": len(table),
            "invalid_entries": len(invalid),
            "verdict": metadata.get("verdict"),
        }
    )
    return info, table


def check_header(data: bytes):
    checks = []
    magic = data[:8]
    checks.append(
        {
            "check": "magic de add-in g3a en offset 0x00",
            "ok": magic == MAGIC,
            "detail": magic.hex(),
        }
    )
    size_ok = len(data) > PAYLOAD_OFFSET + STUB_SIZE
    checks.append(
        {
            "check": "payload presente en offset 0x7000",
            "ok": size_ok,
            "detail": f"tamano 0x{len(data):X}",
        }
    )
    entry_words = ()
    if len(data) >= PAYLOAD_OFFSET + 8:
        entry_words = struct.unpack_from(">4H", data, PAYLOAD_OFFSET)
    if entry_words:
        plausible = entry_words[0] not in (0x0000, 0xFFFF) and (
            0x4F22 in entry_words
            or (entry_words[0] & 0xF000) in (0x2000, 0x6000, 0xD000, 0xE000)
        )
        detail = " ".join(f"{word:04X}" for word in entry_words)
    else:
        plausible = False
        detail = "sin datos"
    checks.append(
        {
            "check": "entrada SH-4 plausible en VA 0x00300000",
            "ok": plausible,
            "detail": detail,
        }
    )
    header = {
        "magic": magic.hex(),
        "magic_ok": checks[0]["ok"],
        "payload_offset": fmt_hex(PAYLOAD_OFFSET, 4),
        "payload_size": max(0, len(data) - PAYLOAD_OFFSET),
        "base_va": fmt_va(BASE_VA),
        "entry_va": fmt_va(BASE_VA),
        "entry_bytes": data[PAYLOAD_OFFSET : PAYLOAD_OFFSET + STUB_SIZE].hex(" "),
        "entry_prologue_ok": plausible,
        "checks": checks,
    }
    return header, all(item["ok"] for item in checks)


def scan_stubs(data: bytes):
    stubs = []
    orphans = []
    search_from = PAYLOAD_OFFSET
    while True:
        hit = data.find(DISPATCHER_LITERAL, search_from)
        if hit < 0:
            break
        search_from = hit + 1
        start = hit - 8
        if start < PAYLOAD_OFFSET or start + STUB_SIZE > len(data) or start % 4 != 0:
            orphans.append(
                {
                    "literal_offset": fmt_hex(hit),
                    "va": fmt_va(BASE_VA + hit - PAYLOAD_OFFSET),
                    "reason": "fuera de payload, sin espacio o sin alineacion a 4",
                }
            )
            continue
        w0, w1, w2, w3 = struct.unpack_from(">4H", data, start)
        raw_id = struct.unpack_from(">I", data, start + 12)[0]
        if not (0xD000 <= w0 <= 0xD7FF and 0xD000 <= w1 <= 0xD7FF):
            orphans.append(
                {
                    "literal_offset": fmt_hex(hit),
                    "va": fmt_va(BASE_VA + hit - PAYLOAD_OFFSET),
                    "reason": "los dos opcodes previos no son mov.l @(disp,PC),Rn",
                }
            )
            continue
        reg0 = (w0 >> 8) & 7
        reg1 = (w1 >> 8) & 7
        expected_jmp = 0x4000 | (reg0 << 8) | 0x2B
        disp0_target = (start & ~3) + 4 + (w0 & 0xFF) * 4
        disp1_target = (((start + 2) & ~3) + 4 + (w1 & 0xFF) * 4)
        issues = []
        if reg0 == 0:
            issues.append("registro_despachador_es_r0")
        if w2 != expected_jmp:
            issues.append("jmp_no_coincide_con_registro")
        if w3 != 0x0009:
            issues.append("delay_slot_no_es_nop")
        if disp0_target != start + 8:
            issues.append("literal_despachador_no_apunta_a_+8")
        if disp1_target != start + 12:
            issues.append("id_no_apunta_a_+12")
        if reg1 != 0:
            issues.append("id_no_cargado_en_r0")
        if raw_id > 0xFFFF:
            issues.append("id_ocupa_mas_de_16_bits")
        canonical = (
            w0 == 0xD201
            and w1 == 0xD002
            and w2 == 0x422B
            and w3 == 0x0009
            and not issues
        )
        stubs.append(
            {
                "file_offset": fmt_hex(start),
                "va": fmt_va(BASE_VA + start - PAYLOAD_OFFSET),
                "words": [f"{w0:04X}", f"{w1:04X}", f"{w2:04X}", f"{w3:04X}"],
                "id": raw_id,
                "id_hex": fmt_hex(raw_id, 4),
                "valid": not issues,
                "standard": canonical,
                "issues": issues,
            }
        )
    return stubs, orphans


def scan_addin(path: Path, table_info: dict, table):
    name = path.name
    record = {
        "name": name,
        "path": relpath(path),
        "exists": path.is_file(),
        "parse_ok": False,
        "size": None,
        "sha256": None,
        "header": None,
        "stubs": [],
        "stub_count": 0,
        "standard_stub_count": 0,
        "nonstandard_stub_count": 0,
        "invalid_stub_count": 0,
        "ids": [],
        "id_min": None,
        "id_max": None,
        "unique_id_count": 0,
        "duplicate_ids": [],
        "ids_ge_limit": [],
        "destinations_resolved": 0,
        "destinations_unresolved": [],
        "dispatcher_orphans": [],
        "discrepancies": [],
    }
    if not path.is_file():
        record["discrepancies"].append("archivo no encontrado")
        return record
    try:
        data = path.read_bytes()
    except OSError as exc:
        record["discrepancies"].append(f"lectura fallida: {exc}")
        return record
    record["size"] = len(data)
    record["sha256"] = sha256_file(path)
    header, header_ok = check_header(data)
    record["header"] = header
    if not header_ok:
        record["discrepancies"].append("cabecera/payload no verificados")
    stubs, orphans = scan_stubs(data)
    record["dispatcher_orphans"] = orphans
    record["stubs"] = stubs
    record["stub_count"] = len(stubs)
    record["standard_stub_count"] = sum(1 for stub in stubs if stub["standard"])
    record["nonstandard_stub_count"] = record["stub_count"] - record["standard_stub_count"]
    record["invalid_stub_count"] = sum(1 for stub in stubs if not stub["valid"])
    record["ids"] = [stub["id"] for stub in stubs]
    if record["ids"]:
        record["id_min"] = min(record["ids"])
        record["id_max"] = max(record["ids"])
        record["unique_id_count"] = len(set(record["ids"]))
        seen = {}
        for stub in stubs:
            key = stub["id"]
            if key in seen:
                record["duplicate_ids"].append(key)
            else:
                seen[key] = stub["file_offset"]
    for stub in stubs:
        if stub["id"] >= SYSCALL_LIMIT:
            record["ids_ge_limit"].append(
                {"id": stub["id"], "file_offset": stub["file_offset"]}
            )
        target = None
        if table is not None:
            target = table.get(stub["id"])
        stub["syscall_target"] = fmt_va(target) if target is not None else None
        if target is not None:
            record["destinations_resolved"] += 1
        elif table is not None:
            record["destinations_unresolved"].append(stub["id"])
    if orphans:
        record["discrepancies"].append(
            f"{len(orphans)} referencia(s) a 0x80020070 sin stub de 16 bytes"
        )
    if record["nonstandard_stub_count"]:
        detail = f"{record['nonstandard_stub_count']} stub(s) no canonicos"
        if record["invalid_stub_count"]:
            detail += f" ({record['invalid_stub_count']} con problemas estructurales)"
        record["discrepancies"].append(detail)
    if record["ids_ge_limit"]:
        record["discrepancies"].append(
            f"{len(record['ids_ge_limit'])} ID(s) >= {SYSCALL_LIMIT}"
        )
    if record["duplicate_ids"]:
        record["discrepancies"].append(
            f"{len(record['duplicate_ids'])} ID(s) duplicado(s) dentro del archivo"
        )
    if not stubs:
        record["discrepancies"].append("no se encontro ningun stub de syscall")
    if table is None:
        record["discrepancies"].append("tabla de syscalls no disponible; destinos omitidos")
    elif record["destinations_unresolved"]:
        record["discrepancies"].append(
            f"{len(record['destinations_unresolved'])} ID(s) sin entrada en la tabla"
        )
    record["parse_ok"] = header_ok
    return record


def build_result(input_dir: Path, table_path: Path) -> dict:
    table_info, table = load_syscall_table(table_path)
    files = [scan_addin(input_dir / name, table_info, table) for name in DEFAULT_FILES]
    all_ids = [value for record in files for value in record["ids"]]
    ids_ge = [
        {"file": record["name"], "id": item["id"], "file_offset": item["file_offset"]}
        for record in files
        for item in record["ids_ge_limit"]
    ]
    files_parsed = sum(1 for record in files if record["parse_ok"])
    files_found = sum(1 for record in files if record["exists"])
    totals = {
        "files_expected": len(DEFAULT_FILES),
        "files_found": files_found,
        "files_parsed": files_parsed,
        "size_bytes": sum(record["size"] or 0 for record in files),
        "stubs": sum(record["stub_count"] for record in files),
        "standard_stubs": sum(record["standard_stub_count"] for record in files),
        "nonstandard_stubs": sum(record["nonstandard_stub_count"] for record in files),
        "invalid_stubs": sum(record["invalid_stub_count"] for record in files),
        "unique_ids": len(set(all_ids)),
        "id_min": min(all_ids) if all_ids else None,
        "id_max": max(all_ids) if all_ids else None,
        "ids_ge_limit": len(ids_ge),
        "destinations_resolved": sum(record["destinations_resolved"] for record in files),
        "destinations_unresolved": sum(
            len(record["destinations_unresolved"]) for record in files
        ),
    }
    reasons = []
    missing = [record["name"] for record in files if not record["exists"]]
    if missing:
        reasons.append("archivos ausentes: " + ", ".join(missing))
    unparsed = [record["name"] for record in files if not record["parse_ok"]]
    if unparsed:
        reasons.append("archivos no parseados: " + ", ".join(unparsed))
    if ids_ge:
        reasons.append(f"{len(ids_ge)} ID(s) >= {SYSCALL_LIMIT}")
    result = {
        "tool": relpath(Path(__file__)),
        "schema_version": SCHEMA_VERSION,
        "generated_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "python": sys.version.split()[0],
        "input_dir": relpath(input_dir),
        "payload_offset": fmt_hex(PAYLOAD_OFFSET, 4),
        "base_va": fmt_va(BASE_VA),
        "dispatcher_va": fmt_va(DISPATCHER_VA),
        "syscall_limit": SYSCALL_LIMIT,
        "syscall_table": table_info,
        "files": files,
        "totals": totals,
        "ids_ge_limit": ids_ge,
        "check": {
            "ok": not reasons,
            "files_expected": len(DEFAULT_FILES),
            "files_parsed": files_parsed,
            "ids_ge_limit": len(ids_ge),
            "reasons": reasons,
        },
    }
    return result


def render_markdown(result: dict) -> str:
    table_info = result["syscall_table"]
    table_state = "no disponible"
    if table_info.get("available"):
        table_state = (
            f"`{table_info['path']}` (base {table_info.get('base')}, "
            f"{table_info.get('count')} entradas, veredicto {table_info.get('verdict')})"
        )
    lines = []
    lines.append("# Escaneo estatico de add-ins .g3a (ETAPA 0, plan N-11)")
    lines.append("")
    lines.append(f"- Fecha UTC: `{result['generated_utc']}`")
    lines.append(f"- Herramienta: `{result['tool']}` (Python {result['python']})")
    lines.append(f"- Entrada: `{result['input_dir']}`")
    lines.append(
        f"- Payload: offset `{result['payload_offset']}`, VA base `{result['base_va']}`"
    )
    lines.append(f"- Despachador referenciado por los stubs: `{result['dispatcher_va']}`")
    lines.append(f"- Limite de ID: `{result['syscall_limit']}`")
    lines.append(f"- Tabla de syscalls: {table_state}")
    verdict = "PASS" if result["check"]["ok"] else "FAIL"
    lines.append(f"- Resultado `--check`: **{verdict}**")
    lines.append("")
    lines.append("## Resumen por archivo")
    lines.append("")
    lines.append(
        "| Archivo | Tamano (B) | Stubs | IDs unicos | Rango IDs | Offsets en archivo | Rango VA | Destinos resueltos | No estandar |"
    )
    lines.append("|---|---:|---:|---:|---|---|---|---:|---:|")
    for record in result["files"]:
        if not record["parse_ok"]:
            lines.append(
                f"| {record['name']} | - | - | - | - | - | - | - | - |"
            )
            continue
        offset_min = record["stubs"][0]["file_offset"] if record["stubs"] else "-"
        offset_max = record["stubs"][-1]["file_offset"] if record["stubs"] else "-"
        va_min = record["stubs"][0]["va"] if record["stubs"] else "-"
        va_max = record["stubs"][-1]["va"] if record["stubs"] else "-"
        id_range = (
            f"{record['id_min']}..{record['id_max']}"
            if record["id_min"] is not None
            else "-"
        )
        lines.append(
            f"| {record['name']} | {record['size']} | {record['stub_count']} | "
            f"{record['unique_id_count']} | {id_range} | {offset_min}..{offset_max} | "
            f"{va_min}..{va_max} | {record['destinations_resolved']}/{record['stub_count']} | "
            f"{record['nonstandard_stub_count']} |"
        )
    totals = result["totals"]
    lines.append(
        f"| **TOTAL** | **{totals['size_bytes']}** | **{totals['stubs']}** | "
        f"**{totals['unique_ids']}** | **{totals['id_min']}..{totals['id_max']}** | - | - | "
        f"**{totals['destinations_resolved']}/{totals['stubs']}** | "
        f"**{totals['nonstandard_stubs']}** |"
    )
    lines.append("")
    lines.append("## SHA-256 de los archivos")
    lines.append("")
    for record in result["files"]:
        lines.append(f"- `{record['name']}`: `{record['sha256']}`")
    lines.append("")
    lines.append("## Verificacion de cabecera y payload")
    lines.append("")
    lines.append("| Archivo | Magic 0x00 | Payload 0x7000 | Tamano payload | Entrada VA | Prologo |")
    lines.append("|---|---|---|---:|---|---|")
    for record in result["files"]:
        header = record["header"] or {}
        lines.append(
            f"| {record['name']} | {'OK' if header.get('magic_ok') else 'FALLA'} | "
            f"{'OK' if record['parse_ok'] else 'FALLA'} | {header.get('payload_size', '-')} | "
            f"{header.get('entry_va', '-')} | {'OK' if header.get('entry_prologue_ok') else 'FALLA'} |"
        )
    lines.append("")
    lines.append("## IDs por archivo")
    lines.append("")
    for record in result["files"]:
        ids = ", ".join(str(value) for value in record["ids"]) or "(ninguno)"
        lines.append(f"**{record['name']}** ({record['stub_count']}): {ids}")
        lines.append("")
    lines.append("## Discrepancias")
    lines.append("")
    found = False
    for record in result["files"]:
        if record["discrepancies"]:
            found = True
            lines.append(f"- `{record['name']}`: " + "; ".join(record["discrepancies"]))
    if not result["syscall_table"].get("available"):
        found = True
        lines.append(
            "- Tabla de syscalls no disponible: "
            + result["syscall_table"].get("reason", "sin detalle")
        )
    if not found:
        lines.append("- Ninguna.")
    lines.append("")
    lines.append("## Verificaciones realizadas")
    lines.append("")
    lines.append(
        "- Magic de add-in `AA AC BD AF 90 88 9A 8D` en el offset 0x00 de cada archivo."
    )
    lines.append(
        "- Presencia de payload a partir del offset 0x7000 y tamano coherente."
    )
    lines.append(
        "- Entrada en VA 0x00300000 con prologo SH-4 plausible (no se interpretan mas campos del contenedor)."
    )
    lines.append(
        "- Stubs de 16 bytes: dos `mov.l @(disp,PC),Rn`, `jmp @Rn`, `nop`, literal `0x80020070`, ID de 32 bits con mitad alta cero."
    )
    lines.append(
        "- Forma canonica observada: `D201 D002 422B 0009`; las variantes de registro se aceptan y se contabilizan como no canonicas."
    )
    lines.append(
        "- Coherencia del desplazamiento PC-relativo de ambos `mov.l` y alineacion a 4 del stub."
    )
    lines.append(
        f"- Todos los IDs comparados con el limite {result['syscall_limit']} y resueltos contra la tabla si esta disponible."
    )
    lines.append("")
    lines.append("## Limitaciones")
    lines.append("")
    lines.append(
        "- Es un analisis estatico: no se ejecuta el add-in ni se emula ninguna syscall."
    )
    lines.append(
        "- La tabla solo confirma que existe una entrada para el ID; no valida la semantica ni el destino en tiempo de ejecucion."
    )
    lines.append(
        "- No se valida checksum ni firma del contenedor .g3a; solo magic, tamano, offset de payload y prologo de entrada."
    )
    lines.append(
        "- Solo se detectan stubs que referencian exactamente la VA 0x80020070; otra VA del despachador no se reconoceria."
    )
    lines.append(
        "- La correspondencia entre los destinos de `syscall_table.json` y el comportamiento real del add-in no se ha probado en ejecucion."
    )
    lines.append(
        "- La VA base `0x00300000` se toma de los hallazgos del plan N-11; no se ha localizado un campo del contenedor que la declare, por lo que no se valida contra el header."
    )
    lines.append("")
    return "\n".join(lines)


def write_outputs(result: dict, out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "addin_scan.json"
    md_path = out_dir / "addin_scan.md"
    json_path.write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    md_path.write_text(render_markdown(result), encoding="utf-8")
    return json_path, md_path


def print_scan(result: dict):
    table = result["syscall_table"]
    state = "disponible" if table.get("available") else "NO disponible"
    print(f"entrada: {result['input_dir']}")
    print(f"tabla de syscalls: {state}")
    for record in result["files"]:
        if not record["parse_ok"]:
            print(f"  {record['name']}: ERROR ({'; '.join(record['discrepancies'])})")
            continue
        id_range = f"{record['id_min']}..{record['id_max']}"
        print(
            f"  {record['name']}: {record['size']} B, {record['stub_count']} stubs, "
            f"IDs {id_range} ({record['unique_id_count']} unicos), "
            f"destinos {record['destinations_resolved']}/{record['stub_count']}, "
            f"no estandar {record['nonstandard_stub_count']}"
        )
    totals = result["totals"]
    print(
        f"archivos parseados: {totals['files_parsed']}/{totals['files_expected']}; "
        f"stubs: {totals['stubs']}; IDs unicos: {totals['unique_ids']}; "
        f"rango global: {totals['id_min']}..{totals['id_max']}"
    )
    print(f"IDs >= {result['syscall_limit']}: {totals['ids_ge_limit']}")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Escaneo estatico de add-ins .g3a de la fx-CG50 (N-11 etapa 0)."
    )
    parser.add_argument("--input-dir", type=Path, default=DEFAULT_INPUT_DIR)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--table", type=Path, default=DEFAULT_SYSCALL_TABLE)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args(argv)

    try:
        result = build_result(args.input_dir, args.table)
        json_path, md_path = write_outputs(result, args.out)
    except OSError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    if not args.quiet:
        print("cg50_addin_scan - add-ins .g3a (N-11 etapa 0)")
        print_scan(result)
        print(f"JSON: {json_path}")
        print(f"MD:   {md_path}")

    if args.check:
        check = result["check"]
        if check["ok"]:
            print("RESULTADO: PASS")
            return 0
        print("RESULTADO: FAIL")
        for reason in check["reasons"]:
            print(f"  - {reason}")
        return 1
    return 0 if result["check"]["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
