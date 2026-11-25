#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pe_imports_dump.py — Vuelca la tabla de imports de un PE32 con el VA de cada
ranura IAT, para poder interpretar `call dword ptr [0x45xxxx]` en el
desensamblado.

Uso:
    python pe_imports_dump.py <archivo.exe|dll> [filtro]

Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import struct
import sys


def parse_pe(data: bytes):
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    file_header = e_lfanew + 4
    opt = file_header + 20
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
    return image_base, sections, opt


def rva_to_off(sections, rva: int):
    for name, vsize, vaddr, rsize, rptr in sections:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            if rva - vaddr >= rsize:
                return None
            return rptr + (rva - vaddr)
    return None


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    path = sys.argv[1]
    filt = sys.argv[2] if len(sys.argv) > 2 else None
    data = open(path, "rb").read()
    image_base, sections, opt = parse_pe(data)
    # Data directories en opt+96; entrada 1 (Import) en opt+96+8
    import_rva, import_size = struct.unpack_from("<II", data, opt + 104)
    if not import_rva:
        print("Sin imports.")
        return 0
    desc_off = rva_to_off(sections, import_rva)
    idx = 0
    total = 0
    while True:
        ilt_rva, ts, fc, name_rva, iat_rva = struct.unpack_from(
            "<IIIII", data, desc_off + idx * 20)
        if not ilt_rva and not iat_rva:
            break
        name_off = rva_to_off(sections, name_rva)
        dll = data[name_off:data.index(b"\x00", name_off)].decode(
            "ascii", "replace") if name_off else "?"
        rows = []
        i = 0
        iat_off0 = rva_to_off(sections, iat_rva)
        ilt_off = rva_to_off(sections, ilt_rva) if ilt_rva else None
        # Si no hay OriginalFirstThunk, los nombres salen del propio IAT
        thunk_off = ilt_off if ilt_off is not None else iat_off0
        print(f"[descriptor {idx}] dll={dll} OFT=0x{ilt_rva:08X} "
              f"FT=0x{iat_rva:08X}")
        while thunk_off is not None:
            entry = struct.unpack_from("<I", data, thunk_off + i * 4)[0]
            if entry == 0:
                break
            iat_va = image_base + iat_rva + i * 4
            if entry & 0x80000000:
                imp = f"ord#{entry & 0xFFFF}"
            else:
                hint_off = rva_to_off(sections, entry)
                if hint_off is None:
                    imp = f"?raw=0x{entry:08X}"
                else:
                    end = data.index(b"\x00", hint_off + 2)
                    imp = data[hint_off + 2:end].decode("ascii", "replace")
            rows.append((iat_va, imp))
            i += 1
        if filt is None or filt.lower() in dll.lower() or any(
                filt.lower() in r[1].lower() for r in rows):
            print(f"\n[{dll}]  ({len(rows)} entradas)")
            for iat_va, imp in rows:
                print(f"  IAT 0x{iat_va:08X}  {imp}")
        else:
            print(f"  ({len(rows)} entradas, filtrado)")
        total += len(rows)
        idx += 1
    print(f"\nTotal entradas de import: {total}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
