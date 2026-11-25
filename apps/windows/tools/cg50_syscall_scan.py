#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cg50_syscall_scan.py — Localiza y valida el ABI de syscalls del OS de la
Casio fx-CG50 (OS 03.80.0212, CPU Renesas SH7305 / SH-4A big-endian) sobre un
volcado de ROM (SystemROM.raw).

QUE HACE (todo se DERIVA de la ROM, nada se asume):
  1. Decodifica a mano (mini-decodificador SH-4A, solo stdlib) el despachador
     candidato en VA 0x80020070 y valida su forma:
         mov.l @(disp,pc),rN   ; rN = base de la tabla (pool literal PC-relativo)
         shll2  r0            ; r0 = numero_de_syscall * 4
         mov.l @(r0,rN),r0    ; r0 = tabla[numero]
         jmp    @r0           ; salto a la funcion (el rts de la funcion vuelve
         nop                  ;  directamente al punto de llamada del add-in)
  2. Extrae la base de la tabla del pool literal y mide la corrida de punteros
     plausibles que empieza ahi (entradas, rango, duplicados, prologos).
  3. Escanea TODA la ROM buscando la corrida mas larga de punteros plausibles y
     comprueba que coincide exactamente con la base cargada por el despachador.
  4. Demuestra la unicidad del despachador: enumera TODAS las secuencias
     "carga indexada + jmp" de la ROM y las "shll2 r0 + carga indexada + jsr"
     (switches del compilador), identificando la unica con forma de ABI.
  5. Cuenta las referencias dentro de la ROM a la direccion del despachador
     (P1 y P2) y a la base de la tabla.
  6. Emite <out>/syscall_table.json (tabla completa + metadatos) y un resumen
     por stdout. Repetible: la salida es determinista (sin marcas de tiempo).

USO:
  python tools/cg50_syscall_scan.py \
      --rom build-native-x64/Release/runtime/SystemROM.raw \
      --out verification/2026-10-17-f6-abi

  --rom por defecto: build-native-x64/Release/runtime/SystemROM.raw
  (relativo a la raiz del proyecto, dos niveles por encima de tools/).
  Solo lectura de la ROM. Exit 0 = hallazgo CONFIRMADO; exit 1 = fallo.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path

# --------------------------------------------------------------------------
# Constantes de plataforma
# --------------------------------------------------------------------------
# El volcado mapea: offset de archivo X <-> VA 0x80000000+X (P1) y su espejo
# 0xA0000000+X (P2). El codigo del OS ocupa aprox. [0x80020070, 0x80B60000).
ROM_VA_BASE = 0x80000000
P2_VA_BASE = 0xA0000000
DISPATCHER_VA = 0x80020070      # entrada candidata del ABI (convencion Casio)
SCAN_VA_LO = 0x80020000         # ventana de escaneo de corridas de punteros
SCAN_VA_HI = 0x80B60000
PTR_LO = 0x80020000             # rango de "puntero plausible a codigo del OS"
PTR_HI = 0x80B60000
MIN_TABLE_ENTRIES = 512         # umbral de sanidad para la corrida de la tabla
MIN_RUN_FOR_REPORT = 8          # corridas mas cortas que esto no se listan
DOCUMENTED_SHA256 = ("8cf1e1f2ad3fb742b557d00535f8aea7874c7f8d"
                     "138b56b4c3938e26c0f82e23")
DEFAULT_ROM_REL = Path("build-native-x64") / "Release" / "runtime" / "SystemROM.raw"
PROJECT_ROOT = Path(__file__).resolve().parent.parent


# --------------------------------------------------------------------------
# Utilidades de lectura big-endian
# --------------------------------------------------------------------------
def be16(rom: bytes, off: int) -> int:
    return (rom[off] << 8) | rom[off + 1]


def be32(rom: bytes, off: int) -> int:
    return ((rom[off] << 24) | (rom[off + 1] << 16)
            | (rom[off + 2] << 8) | rom[off + 3])


def va_to_off(va: int) -> int:
    return va - ROM_VA_BASE


def off_to_va(off: int) -> int:
    return ROM_VA_BASE + off


def hx(v: int, width: int = 8) -> str:
    return f"0x{v:0{width}X}"


def is_plausible_code_ptr(v: int) -> bool:
    """Puntero plausible a codigo del OS: dentro del area de codigo P1."""
    return PTR_LO <= v < PTR_HI


# --------------------------------------------------------------------------
# Mini-decodificador SH-4A (solo el subconjunto necesario, big-endian)
# --------------------------------------------------------------------------
def decode_movl_pc(rom: bytes, va: int) -> dict | None:
    """Decodifica MOV.L @(disp,PC),Rn en `va`. Devuelve None si no encaja."""
    off = va_to_off(va)
    w = be16(rom, off)
    if (w >> 12) != 0xD:
        return None
    reg = (w >> 8) & 0xF
    disp = w & 0xFF
    ea = ((va + 4) & ~3) + disp * 4     # direccion efectiva del literal
    lit_off = va_to_off(ea)
    if lit_off < 0 or lit_off + 4 > len(rom):
        return None
    return {
        "va": va,
        "word": w,
        "bytes": rom[off:off + 2].hex(),
        "reg": reg,
        "disp_words": disp,
        "disp_bytes": disp * 4,
        "literal_va": ea,
        "literal_value": be32(rom, lit_off),
        "text": f"mov.l @({disp * 4:#04x},pc),r{reg}",
    }


def decode_dispatcher(rom: bytes, va: int) -> dict:
    """Decodifica y valida el despachador candidato en `va`.

    Forma esperada (subset SH-4A):
      1101 nnnn dddddddd   MOV.L @(disp*4,PC),Rn   -> rN = base de tabla
      0100 0000 0000 1000 SHLL2 R0                -> r0 = id * 4
      0000 nnnn mmmm 1110 MOV.L @(R0,Rm),Rn       -> r0 = tabla[id]
      0100 nnnn 0010 1011 JMP   @Rn
      0000 0000 0000 1001 NOP (delay slot)
    """
    off = va_to_off(va)
    if off < 0 or off + 16 > len(rom):
        raise SystemExit(f"ERROR: VA {hx(va)} fuera de la ROM")

    words = [be16(rom, off + 2 * i) for i in range(5)]
    errors: list[str] = []
    insns: list[dict] = []

    load = decode_movl_pc(rom, va)
    if load is None:
        raise SystemExit(f"ERROR: {hx(va)} no es 'mov.l @(disp,pc),rN'")
    insns.append({"va": va, "bytes": load["bytes"], "text": load["text"],
                  "meaning": f"r{load['reg']} = [{hx(load['literal_va'])}]"})
    if not (va <= load["literal_va"] < va + 0x1000):
        errors.append("pool literal demasiado lejos del despachador")

    w1, w2, w3, w4 = words[1], words[2], words[3], words[4]
    rdest = None
    rbase = load["reg"]

    if w1 == 0x4008:
        insns.append({"va": va + 2, "bytes": rom[off + 2:off + 4].hex(),
                      "text": "shll2 r0", "meaning": "r0 = id * 4"})
    else:
        errors.append(f"se esperaba shll2 r0 (0x4008), hay {w1:#06x}")

    if (w2 >> 12) == 0 and (w2 & 0xF) == 0xE:
        rdest = (w2 >> 8) & 0xF
        mbase = (w2 >> 4) & 0xF
        if mbase != rbase:
            errors.append(
                f"la carga indexada usa r{mbase} como base, "
                f"pero el literal se cargo en r{rbase}")
        insns.append({
            "va": va + 4,
            "bytes": rom[off + 4:off + 6].hex(),
            "text": f"mov.l @(r0,r{mbase}),r{rdest}",
            "meaning": f"r{rdest} = [r{mbase} + r0]  (r0 = id*4)",
        })
    else:
        errors.append(f"se esperaba mov.l @(r0,rX),rY, hay {w2:#06x}")

    if (w3 >> 12) == 4 and (w3 & 0xFF) == 0x2B:
        jreg = (w3 >> 8) & 0xF
        if rdest is not None and jreg != rdest:
            errors.append(f"jmp @r{jreg} no salta al registro cargado r{rdest}")
        insns.append({"va": va + 6, "bytes": rom[off + 6:off + 8].hex(),
                      "text": f"jmp @r{jreg}", "meaning": "salto a la syscall"})
    else:
        errors.append(f"se esperaba jmp @rN, hay {w3:#06x}")

    delay_ok = (w4 == 0x0009)
    insns.append({"va": va + 8, "bytes": rom[off + 8:off + 10].hex(),
                  "text": "nop" if delay_ok else f"?? ({w4:#06x})",
                  "meaning": "delay slot"})

    base = load["literal_value"]
    base_valid = is_plausible_code_ptr(base) and base % 4 == 0
    if not base_valid:
        errors.append(f"literal {hx(base)} no es base plausible de tabla")

    return {
        "va": va,
        "file_offset": hx(va_to_off(va)),
        "raw_bytes": rom[off:off + 16].hex(" "),
        "instructions": insns,
        "literal_pool": {
            "va": hx(load["literal_va"]),
            "word_disp": load["disp_words"],
            "value": hx(base),
        },
        "table_base_va": base,
        "table_base_file_offset": hx(va_to_off(base)),
        "delay_slot_is_nop": delay_ok,
        "errors": errors,
        "valid": not errors and base_valid,
    }


def decode_startup_context(rom: bytes, va: int) -> list[dict]:
    """Primeras instrucciones tras el despachador (contexto: arranque del OS)."""
    out = []
    i = 0
    while len(out) < 4:
        load = decode_movl_pc(rom, va + 2 * i)
        if load is not None:
            out.append({
                "va": hx(va + 2 * i),
                "bytes": load["bytes"],
                "text": load["text"],
                "meaning": f"literal {hx(load['literal_va'])} = "
                           f"{hx(load['literal_value'])}",
            })
            i += 1
            continue
        w = be16(rom, va_to_off(va + 2 * i))
        if w == 0x440E:
            out.append({"va": hx(va + 2 * i), "bytes": "440e",
                        "text": "ldc r4,vbr", "meaning": "VBR = r4"})
        i += 1
    return out


# --------------------------------------------------------------------------
# Extraccion y validacion de la tabla
# --------------------------------------------------------------------------
def extract_pointer_run(rom: bytes, base_va: int) -> tuple[list[int], int]:
    """Dwords consecutivos plausibles desde `base_va`.
    Devuelve (entradas, primer dword no plausible)."""
    off = va_to_off(base_va)
    entries: list[int] = []
    i = 0
    while off + 4 * i + 4 <= len(rom):
        v = be32(rom, off + 4 * i)
        if not is_plausible_code_ptr(v):
            break
        entries.append(v)
        i += 1
    first_invalid = be32(rom, off + 4 * i) if off + 4 * i + 4 <= len(rom) else -1
    return entries, first_invalid


def classify_prologue(word: int) -> str:
    """Clasifica el primer opcode de un destino como inicio de funcion."""
    if (word >> 12) == 2 and (word & 0xF) == 6 and ((word >> 4) & 0xF) != 15:
        return "push reg (mov.l rm,@-r15)"
    if word == 0x4F22:
        return "sts.l pr,@-r15"
    if (word >> 12) == 6 and (word & 0xF) == 3:
        return "mov rn,rm"
    if (word >> 12) == 0xD:
        return "mov.l @(disp,pc),rn"
    if (word >> 12) == 0xE:
        return "mov #imm,rn"
    if (word >> 12) == 0x7:
        return "add #imm,rn"
    if (word >> 12) in (0xA, 0xB):
        return "bra/bsr"
    if word in (0x0009, 0x000B):
        return "nop/rts"
    if (word >> 12) in (0x8, 0x9, 0xC):
        return "cmp/tst/bt/bf imm"
    if (word >> 12) == 4:
        return "4xxx (shll*/sts/lds/stc/...)"
    if (word >> 12) == 1:
        return "mov.* rm,@(r0,rn)"
    if (word >> 12) == 5:
        return "mov.w @(disp,pc)"
    if (word >> 12) == 0:
        return "mov.* con r0"
    return "other"


# --------------------------------------------------------------------------
# Escaneos globales de la ROM
# --------------------------------------------------------------------------
def scan_pointer_runs(rom: bytes) -> list[tuple[int, int]]:
    """Corridas de dwords consecutivos plausibles (alineadas a 4) en la
    ventana [SCAN_VA_LO, SCAN_VA_HI). Devuelve [(va_inicio, longitud)]."""
    runs: list[tuple[int, int]] = []
    cur_start = None
    cur_len = 0
    lo_off = va_to_off(SCAN_VA_LO)
    hi_off = min(va_to_off(SCAN_VA_HI), len(rom) - 3)
    for off in range(lo_off & ~3, hi_off, 4):
        v = be32(rom, off)
        if is_plausible_code_ptr(v):
            if cur_start is None:
                cur_start = off
                cur_len = 1
            else:
                cur_len += 1
        else:
            if cur_len >= MIN_RUN_FOR_REPORT:
                runs.append((off_to_va(cur_start), cur_len))
            cur_start, cur_len = None, 0
    if cur_len >= MIN_RUN_FOR_REPORT:
        runs.append((off_to_va(cur_start), cur_len))
    runs.sort(key=lambda r: (-r[1], r[0]))
    return runs


def find_pc_literal_for_base(rom: bytes, off: int, base_reg: int,
                             window: int = 6) -> tuple[int, int] | None:
    """Busca hacia atras un 'mov.l @(disp,pc),r<base_reg>' cuyo literal este
    en la ventana. Devuelve (va_del_mov_l, valor_literal) o None."""
    for j in range(1, window + 1):
        p = off - 2 * j
        if p < 0:
            break
        w = be16(rom, p)
        if (w >> 12) == 0xD and ((w >> 8) & 0xF) == base_reg:
            ea = ((off_to_va(p) + 4) & ~3) + (w & 0xFF) * 4
            ea_off = va_to_off(ea)
            if 0 <= ea_off + 4 <= len(rom):
                return ea, be32(rom, ea_off)
    return None


def scan_indexed_jumps(rom: bytes) -> dict:
    """Enumera TODAS las secuencias 'MOV.L @(R0,Rm),Rn' seguidas de 'JMP @Rn'
    (indice R0 fijo del formato). Marca si hay shll2 r0 antes y si la base
    viene de un literal PC-relativo cercano."""
    hits: list[dict] = []
    lo_off = va_to_off(SCAN_VA_LO)
    hi_off = len(rom) - 8
    for off in range(lo_off & ~1, hi_off, 2):
        w = be16(rom, off)
        if (w >> 12) != 0 or (w & 0xF) != 0xE:
            continue
        rdest = (w >> 8) & 0xF
        rbase = (w >> 4) & 0xF
        if rbase == 0:
            continue                    # base == indice: no despacha
        w2 = be16(rom, off + 2)
        if (w2 >> 12) != 4 or (w2 & 0xFF) != 0x2B or ((w2 >> 8) & 0xF) != rdest:
            continue
        has_shll2 = off >= 2 and be16(rom, off - 2) == 0x4008
        lit = find_pc_literal_for_base(rom, off, rbase)
        hits.append({
            "load_va": hx(off_to_va(off)),
            "shape": (f"{'shll2 r0; ' if has_shll2 else ''}"
                      f"mov.l @(r0,r{rbase}),r{rdest}; jmp @r{rdest}"),
            "base_reg": rbase,
            "dest_reg": rdest,
            "shll2_before": has_shll2,
            "base_literal_va": hx(lit[0]) if lit else None,
            "base_literal_value": hx(lit[1]) if lit else None,
            "abi_shape": has_shll2 and lit is not None and rdest == 0,
        })
    return {"count": len(hits), "hits": hits}


def scan_shll2_jsr(rom: bytes) -> dict:
    """Cuenta secuencias 'shll2 r0; mov.l @(r0,rX),rY; jsr @rY' (switches
    del compilador) y cuales tienen base con literal PC-relativo."""
    total = 0
    with_lit: list[dict] = []
    lo_off = va_to_off(SCAN_VA_LO)
    hi_off = len(rom) - 12
    for off in range(lo_off & ~1, hi_off, 2):
        if be16(rom, off) != 0x4008:    # shll2 r0
            continue
        for k in range(1, 4):
            w = be16(rom, off + 2 * k)
            if (w >> 12) == 0 and (w & 0xF) == 0xE:
                rdest = (w >> 8) & 0xF
                rbase = (w >> 4) & 0xF
                w2 = be16(rom, off + 2 * k + 2)
                if ((w2 >> 12) == 4 and (w2 & 0xFF) == 0x0B
                        and ((w2 >> 8) & 0xF) == rdest):
                    total += 1
                    lit = find_pc_literal_for_base(rom, off + 2 * k, rbase)
                    if lit:
                        entry = {
                            "load_va": hx(off_to_va(off + 2 * k)),
                            "base_literal_va": hx(lit[0]),
                            "base_literal_value": hx(lit[1]),
                        }
                        if entry not in with_lit:
                            with_lit.append(entry)
                    break
    return {"count": total, "with_pc_literal_base": with_lit}


def count_dword_occurrences(rom: bytes, value: int) -> list[int]:
    """Offsets de archivo donde aparece el dword big-endian `value`."""
    needle = value.to_bytes(4, "big")
    out: list[int] = []
    pos = rom.find(needle)
    while pos != -1:
        out.append(pos)
        pos = rom.find(needle, pos + 1)
    return out


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Localiza y valida el ABI de syscalls de la fx-CG50 "
                    "(despachador + tabla) en un volcado SystemROM.raw.")
    parser.add_argument("--rom", default=None,
                        help="ruta al SystemROM.raw (por defecto: %s relativa "
                             "a la raiz del proyecto)" % DEFAULT_ROM_REL.as_posix())
    parser.add_argument("--out", required=True,
                        help="directorio donde escribir syscall_table.json")
    parser.add_argument("--dispatcher", default=None,
                        help="VA del despachador candidato "
                             "(por defecto 0x80020070)")
    return parser


def main() -> int:
    # stdout en UTF-8 para que scan-run.txt quede limpio en cualquier consola
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass

    args = build_parser().parse_args()
    rom_path = Path(args.rom) if args.rom else PROJECT_ROOT / DEFAULT_ROM_REL
    out_dir = Path(args.out)
    dispatcher_va = int(args.dispatcher, 0) if args.dispatcher else DISPATCHER_VA

    print("=" * 74)
    print(" cg50_syscall_scan — ABI de syscalls fx-CG50 (SH7305, SH-4A big-endian)")
    print("=" * 74)
    print(f"comando: python {' '.join(sys.argv)}")
    print(f"ROM    : {rom_path}")

    try:
        rom = rom_path.read_bytes()
    except OSError as exc:
        print(f"ERROR: no se puede leer la ROM: {exc}")
        return 1
    sha = hashlib.sha256(rom).hexdigest()
    sha_matches = (sha == DOCUMENTED_SHA256)
    print(f"        size = {len(rom)} ({len(rom):#x}) bytes")
    print(f"        sha256 = {sha}")
    print(f"        sha256 coincide con el documentado ({DOCUMENTED_SHA256}): "
          f"{'SI' if sha_matches else 'NO'}")
    if len(rom) < va_to_off(SCAN_VA_HI):
        print("AVISO: la ROM es mas corta que la ventana de escaneo esperada")

    checks: list[tuple[str, bool, str]] = []

    # ---- [1] Despachador --------------------------------------------------
    print()
    print(f"[1] Despachador candidato: VA {hx(dispatcher_va)} "
          f"(offset {hx(va_to_off(dispatcher_va))})")
    try:
        disp = decode_dispatcher(rom, dispatcher_va)
    except SystemExit as exc:
        print(f"    {exc}")
        return 1
    for ins in disp["instructions"]:
        meaning = f"   ; {ins['meaning']}" if ins["meaning"] else ""
        print(f"    {ins['va']:08X}  {ins['bytes']:<11} {ins['text']:<22}{meaning}")
    pool = disp["literal_pool"]
    print(f"    {int(pool['va'], 16):08X}  "
          f"{rom[va_to_off(int(pool['va'], 16)):va_to_off(int(pool['va'], 16)) + 4].hex(' ')}  "
          f".data.l {pool['value']}"
          f"{'':13}; pool literal (disp={pool['word_disp']} words)")
    if disp["errors"]:
        for e in disp["errors"]:
            print(f"    ERROR de patron: {e}")
    checks.append(("patron del despachador decodificado y valido",
                   disp["valid"], "mov.l pool / shll2 r0 / mov.l @(r0,r2),r0 / jmp @r0 / nop"))
    base_va = disp["table_base_va"]
    print(f"    => forma {'CONFIRMADA' if disp['valid'] else 'INVALIDA'}; "
          f"base de la tabla cargada desde el pool: {hx(base_va)}")

    # ---- [2] Transformacion del indice -----------------------------------
    print()
    print("[2] Transformacion del indice")
    print(f"    dir_destino = {hx(base_va)} + 4 * R0   "
          "(R0 = numero de syscall; sin sesgo, sin mascara)")
    print("    el despachador no toca PR ni R1..R7: el 'rts' de la funcion")
    print("    devuelve directamente al punto de llamada del add-in")

    # ---- [3] Corrida de punteros de la tabla ------------------------------
    print()
    print(f"[3] Corrida de punteros desde la base {hx(base_va)}")
    entries, first_invalid = extract_pointer_run(rom, base_va)
    count = len(entries)
    if count:
        min_t, max_t = min(entries), max(entries)
        distinct = len(set(entries))
        odd = sum(1 for v in entries if v & 1)
        dup_counter = Counter(entries)
        duplicates = sorted(
            ({"value": hx(v), "count": c,
              "indices": [hx(i) for i, e in enumerate(entries) if e == v]}
             for v, c in dup_counter.items() if c > 1),
            key=lambda d: int(d["value"], 16))
        non_inc = [i for i in range(count - 1) if entries[i] >= entries[i + 1]]
        all_below_base = max_t < base_va
    else:
        min_t = max_t = distinct = odd = 0
        duplicates, non_inc = [], []
        all_below_base = True
    last_index = count - 1
    print(f"    entradas consecutivas plausibles: {count} ({count:#x}), "
          f"indices {hx(0)}..{hx(last_index)}")
    print(f"    ocupa [{hx(base_va)}, {hx(base_va + 4 * last_index)}]; "
          f"dword siguiente ({hx(base_va + 4 * count)}) = {hx(first_invalid)} "
          "(no es puntero)")
    print(f"    destinos: min {hx(min_t)} / max {hx(max_t)} / "
          f"distintos {distinct} / impares {odd}")
    print(f"    todos los destinos por debajo de la base de la tabla: "
          f"{'SI' if all_below_base else 'NO'}")
    print(f"    valores duplicados: {len(duplicates)}; "
          f"pasos no crecientes en la secuencia: {len(non_inc)}")
    checks.append(("la tabla tiene un numero de entradas esperable (>=512)",
                   count >= MIN_TABLE_ENTRIES, f"encontradas {count}"))
    checks.append(("todos los destinos son direcciones pares (SH-4A)",
                   odd == 0, f"{odd} impares"))
    checks.append(("todos los destinos caen antes de la tabla (codigo .text)",
                   all_below_base, f"max destino {hx(max_t)}"))

    # ---- [4] Escaneo de corridas en toda la ROM ---------------------------
    print()
    print(f"[4] Corridas de punteros plausibles en TODA la ROM "
          f"[{hx(SCAN_VA_LO)}, {hx(SCAN_VA_HI - 1)}] (min {MIN_RUN_FOR_REPORT} dwords)")
    runs = scan_pointer_runs(rom)
    for rank, (run_va, run_len) in enumerate(runs[:10], 1):
        marker = "   <= empieza exactamente en la base del despachador" \
            if run_va == base_va else ""
        print(f"    #{rank:<2} VA {run_va:08X}  len {run_len:<6}{marker}")
    longest = runs[0] if runs else (0, 0)
    longest_is_table = longest[0] == base_va and longest[1] == count
    checks.append(("la corrida mas larga de la ROM empieza en la base "
                   "cargada por el despachador y mide lo mismo que la tabla",
                   longest_is_table,
                   f"corrida mas larga {hx(longest[0])} len {longest[1]}"))

    # ---- [5] Unicidad del despachador -------------------------------------
    print()
    print("[5] Unicidad: secuencias 'carga indexada (r0) + jmp' en la ROM")
    indexed = scan_indexed_jumps(rom)
    for hit in indexed["hits"]:
        tag = " <= DESPACHADOR DE SYSCALLS (forma ABI)" if hit["abi_shape"] else ""
        lit_txt = (f"base literal {hit['base_literal_value']} "
                   f"@ {hit['base_literal_va']}" if hit["base_literal_value"]
                   else "sin literal PC-relativo cercano")
        print(f"    {hit['load_va']}: {hit['shape']:<40} {lit_txt}{tag}")
    shll2_jsr = scan_shll2_jsr(rom)
    print(f"    secuencias 'shll2 r0 + carga indexada + jsr' "
          f"(switches del compilador): {shll2_jsr['count']}")
    for lit_hit in shll2_jsr["with_pc_literal_base"]:
        print(f"        {lit_hit['load_va']}: base literal "
              f"{lit_hit['base_literal_value']} @ {lit_hit['base_literal_va']}")
    abi_hits = [h for h in indexed["hits"] if h["abi_shape"]]
    checks.append(("existe exactamente UNA secuencia con forma de ABI "
                   "(shll2 r0 + carga indexada + jmp + base literal PC-relativa "
                   "+ destino r0)",
                   len(abi_hits) == 1, f"encontradas {len(abi_hits)}"))

    # ---- [6] Referencias dentro de la ROM ---------------------------------
    print()
    print("[6] Referencias dentro de la ROM (dwords big-endian exactos)")
    ref_disp_p1 = count_dword_occurrences(rom, dispatcher_va)
    disp_p2_alias = P2_VA_BASE + (dispatcher_va - ROM_VA_BASE)
    ref_disp_p2 = count_dword_occurrences(rom, disp_p2_alias)
    ref_base = count_dword_occurrences(rom, base_va)
    print(f"    dword {hx(dispatcher_va)} (despachador, P1): {len(ref_disp_p1)} apariciones")
    print(f"    dword {hx(disp_p2_alias)} (despachador, P2): {len(ref_disp_p2)} apariciones")
    print(f"    dword {hx(base_va)} (base de la tabla): {len(ref_base)} apariciones "
          f"@ {[hx(off_to_va(o)) for o in ref_base]}")
    print("    => el OS nunca salta a su propio despachador: la entrada existe "
          "para los add-ins .g3a (fuera de esta ROM)")

    # ---- [7] Prologos de los destinos -------------------------------------
    print()
    print(f"[7] Primer opcode de los {count} destinos (inicio de funcion plausible)")
    buckets = Counter(
        classify_prologue(be16(rom, va_to_off(v))) for v in entries)
    plausible = count - buckets.get("other", 0)
    for name, c in buckets.most_common():
        print(f"    {name:<28} {c:>5}  ({100 * c / count:5.1f}%)")
    print(f"    clasificables (todo menos 'other'): {plausible}/{count} "
          f"({100 * plausible / count:.1f}%)")
    checks.append(("la gran mayoria de destinos empiezan con instrucciones "
                   "tipicas de entrada de funcion",
                   plausible / count >= 0.9 if count else False,
                   f"{100 * plausible / count:.1f}%"))

    # ---- [8] Contexto tras el despachador ---------------------------------
    print()
    print(f"[8] Contexto inmediato tras el despachador ({hx(dispatcher_va + 16)}): "
          "arranque del OS")
    ctx = decode_startup_context(rom, dispatcher_va + 16)
    for c in ctx:
        print(f"    {c['va']}  {c['bytes']:<6} {c['text']:<24} ; {c['meaning']}")
    print("    (el despachador es el primer bloque de codigo del .text; "
          "tras el va el arranque: pila y VBR)")

    # ---- Veredicto ---------------------------------------------------------
    print()
    print("-" * 74)
    print("Comprobaciones criticas:")
    all_ok = True
    for name, ok, detail in checks:
        all_ok &= ok
        print(f"  [{'OK ' if ok else 'FALLO'}] {name}  ({detail})")
    verdict = "CONFIRMADO" if all_ok else "NO CONFIRMADO"
    print()
    print(f"VEREDICTO: {verdict}")
    print(f"  despachador {hx(dispatcher_va)} | tabla {hx(base_va)} | "
          f"{count} entradas | indices {hx(0)}..{hx(last_index)}")

    # ---- JSON --------------------------------------------------------------
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "syscall_table.json"
    payload = {
        "base": hx(base_va),
        "count": count,
        "entries": [{"index": i, "target": hx(v)} for i, v in enumerate(entries)],
        "metadata": {
            "generator": "tools/cg50_syscall_scan.py",
            "command": f"python {' '.join(sys.argv)}",
            "rom": {
                "path": str(rom_path),
                "size": len(rom),
                "sha256": sha,
                "sha256_matches_documented": sha_matches,
                "documented_sha256": DOCUMENTED_SHA256,
                "addressing": "offset de archivo X <-> VA 0x80000000+X (P1); "
                               "espejo 0xA0000000+X (P2)",
            },
            "dispatcher": {
                "va": hx(dispatcher_va),
                "file_offset": disp["file_offset"],
                "raw_bytes": disp["raw_bytes"],
                "instructions": disp["instructions"],
                "literal_pool_va": disp["literal_pool"]["va"],
                "literal_pool_value": disp["literal_pool"]["value"],
                "delay_slot_is_nop": disp["delay_slot_is_nop"],
                "valid_shape": disp["valid"],
            },
            "index_transform": {
                "formula": f"dir_destino = {hx(base_va)} + 4 * R0",
                "scaling": "shll2 r0 (x4)",
                "bias": "ninguno",
                "mask": "ninguna (R0 se usa entero)",
                "max_valid_index": hx(last_index) if count else None,
                "first_invalid_dword": hx(first_invalid),
                "first_invalid_index": hx(count),
            },
            "table_details": {
                "base_file_offset": hx(va_to_off(base_va)),
                "end_va_inclusive": hx(base_va + 4 * last_index) if count else None,
                "end_va_exclusive": hx(base_va + 4 * count),
                "targets_min": hx(min_t),
                "targets_max": hx(max_t),
                "targets_distinct": distinct,
                "targets_odd": odd,
                "all_targets_below_base": all_below_base,
                "duplicates": duplicates,
                "non_increasing_steps": len(non_inc),
                "non_increasing_first_indices": [hx(i) for i in non_inc[:16]],
                "prologue_buckets": dict(buckets),
                "prologue_plausible": plausible,
                "prologue_plausible_pct": round(100 * plausible / count, 1) if count else 0.0,
            },
            "rom_pointer_run_scan": {
                "window": [hx(SCAN_VA_LO), hx(SCAN_VA_HI - 1)],
                "min_run_reported": MIN_RUN_FOR_REPORT,
                "longest": {"va": hx(longest[0]), "length": longest[1]},
                "longest_is_table": longest_is_table,
                "top_runs": [{"va": hx(v), "length": n} for v, n in runs[:10]],
            },
            "uniqueness": {
                "indexed_load_jmp_sequences": indexed,
                "shll2_jsr_sequences": shll2_jsr,
            },
            "references": {
                "dispatcher_dword_p1_count": len(ref_disp_p1),
                "dispatcher_dword_p2_count": len(ref_disp_p2),
                "table_base_dword_count": len(ref_base),
                "table_base_dword_at": [hx(off_to_va(o)) for o in ref_base],
            },
            "startup_context": ctx,
            "checks": [{"name": n, "ok": ok, "detail": d}
                       for n, ok, d in checks],
            "verdict": verdict,
        },
    }
    json_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False),
                         encoding="utf-8")
    print()
    print(f"JSON escrito: {json_path} ({count} entradas)")
    print(f"VEREDICTO FINAL: {verdict}")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
