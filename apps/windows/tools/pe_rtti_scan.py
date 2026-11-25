#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pe_rtti_scan.py — Escanea el RTTI (MSVC) de un PE32 para mapear
clases C++ -> vtable -> métodos, con foco en las CGCAI_Task_* del exe oficial.

Uso:
    python pe_rtti_scan.py <archivo.exe> [filtro-subcadena]

Imprime por cada clase encontrada: nombre, VA de la vtable y la lista de
métodos (VAs). Los punteros se validan contra .text para cortar la tabla.
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import struct
import sys


def parse_pe(data: bytes):
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    file_header = e_lfanew + 4
    opt = file_header + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10B:
        raise ValueError("No es PE32")
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    num_sections = struct.unpack_from("<H", data, file_header + 2)[0]
    opt_header_size = struct.unpack_from("<H", data, file_header + 16)[0]
    sections_off = file_header + 20 + opt_header_size
    sections = []
    for i in range(num_sections):
        s = sections_off + i * 40
        name = data[s:s + 8].rstrip(b"\x00").decode("ascii", "replace")
        vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, s + 8)
        sections.append((name, vsize, vaddr, rsize, rptr))
    return image_base, sections


def rva_to_off(sections, rva: int):
    for name, vsize, vaddr, rsize, rptr in sections:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            if rva - vaddr >= rsize:
                return None
            return rptr + (rva - vaddr)
    return None


def off_to_rva(sections, off: int):
    for name, vsize, vaddr, rsize, rptr in sections:
        if rptr <= off < rptr + rsize:
            return vaddr + (off - rptr)
    return None


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    path = sys.argv[1]
    name_filter = sys.argv[2].lower() if len(sys.argv) > 2 else "cgcai"
    data = open(path, "rb").read()
    image_base, sections = parse_pe(data)
    text = next(s for s in sections if s[0] == ".text")
    # (nombre, vsize, vaddr, rsize, rptr): rango RVA de .text
    text_rva_lo, text_rva_hi = text[2], text[2] + text[1]

    # 1) Type descriptors: cadenas ".?AV..." en todo el archivo mapeable
    #    Estructura: [vptr(4)][spare(4)]name  -> name_va - 8 = TD va
    hits = []
    for name, vsize, vaddr, rsize, rptr in sections:
        if name not in (".rdata", ".data", ".text"):
            continue
        blob = data[rptr:rptr + rsize]
        idx = 0
        while True:
            idx = blob.find(b".?AV", idx)
            if idx < 0:
                break
            end = blob.find(b"\x00", idx)
            if end < 0 or end - idx > 128:
                idx += 1
                continue
            cls_name = blob[idx:end].decode("ascii", "replace")
            td_rva = off_to_rva(sections, rptr + idx) - 8
            if td_rva:
                hits.append((cls_name, td_rva))
            idx = end
    if name_filter:
        hits = [h for h in hits if name_filter in h[0].lower()]
    if not hits:
        print("Sin type descriptors que coincidan.")
        return 0

    # 2) Por cada TD, buscar COLs que lo referencien (COL+12 = TD rva)
    #    y por cada COL, buscar vtables ([vt-4] = COL rva)
    for cls_name, td_rva in sorted(hits):
        td_va = image_base + td_rva
        print(f"\n== {cls_name}  TypeDescriptor VA 0x{td_va:08X} ==")
        cols = []
        for name, vsize, vaddr, rsize, rptr in sections:
            if name not in (".rdata", ".data"):
                continue
            blob = data[rptr:rptr + rsize]
            # RTTI 32-bit: punteros internos como VA absoluta
            needle = struct.pack("<I", td_va)
            idx = 0
            while True:
                idx = blob.find(needle, idx)
                if idx < 0:
                    break
                rva = off_to_rva(sections, rptr + idx)
                cols.append(rva - 12)  # COL RVA (TD en COL+0xC)
                idx += 1
        cols = sorted(set(cols))
        if not cols:
            print("  (sin COL que lo referencie)")
            continue
        for col_rva in cols:
            # vtable: dword cuyo valor == VA del COL, en .rdata/.data
            vts = []
            for name, vsize, vaddr, rsize, rptr in sections:
                if name not in (".rdata", ".data"):
                    continue
                blob = data[rptr:rptr + rsize]
                needle = struct.pack("<I", image_base + col_rva)
                idx = 0
                while True:
                    idx = blob.find(needle, idx)
                    if idx < 0:
                        break
                    rva = off_to_rva(sections, rptr + idx)
                    vts.append(rva + 4)
                    idx += 1
            for vt_rva in sorted(set(vts)):
                off = rva_to_off(sections, vt_rva)
                if off is None:
                    continue
                print(f"  vtable VA 0x{image_base + vt_rva:08X}:")
                va_lo = image_base + text_rva_lo
                va_hi = image_base + text_rva_hi
                i = 0
                while True:
                    m_va = struct.unpack_from("<I", data, off + i * 4)[0]
                    if not (va_lo <= m_va < va_hi):
                        break
                    print(f"    [{i:2d}] 0x{m_va:08X}")
                    i += 1
                    if i > 63:
                        print("    …(corte en 64)")
                        break
    return 0


if __name__ == "__main__":
    sys.exit(main())
