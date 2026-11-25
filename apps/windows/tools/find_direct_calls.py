#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
find_direct_calls.py — Busca llamadas directas E8 (call rel32) y saltos EB
hacia una VA de un PE (x86) y desensambla el contexto de cada sitio.

Uso:
    python find_direct_calls.py <archivo.exe> <va-hex> [radio-hex]

Complementa a find_xrefs.py (que solo cubre FF15/FF25/A1 indirectos).
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
except ImportError:
    print("capstone no disponible: pip install capstone")
    sys.exit(2)


def pe_info(data: bytes):
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    file_header = e_lfanew + 4
    opt = file_header + 20
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    num_sections = struct.unpack_from("<H", data, file_header + 2)[0]
    opt_header_size = struct.unpack_from("<H", data, file_header + 16)[0]
    sections_off = file_header + 20 + opt_header_size
    out = []
    for i in range(num_sections):
        s = sections_off + i * 40
        name = data[s:s + 8].rstrip(b"\x00").decode("ascii", "replace")
        virt_size, virt_addr, raw_size, raw_ptr = struct.unpack_from(
            "<IIII", data, s + 8)
        out.append((name, virt_size, virt_addr, raw_size, raw_ptr))
    return image_base, out


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    path = sys.argv[1]
    target_va = int(sys.argv[2], 16)
    radius = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x40
    data = open(path, "rb").read()
    image_base, secs = pe_info(data)
    if target_va < image_base:
        target_va += image_base
    print(f"Buscando call/jmp directos a VA 0x{target_va:08X} en {path} "
          f"(ImageBase 0x{image_base:08X})")
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    hits = 0
    for name, vsize, vaddr, rsize, rptr in secs:
        if name != ".text":
            continue
        blob = data[rptr:rptr + rsize]
        for i in range(len(blob) - 5):
            op = blob[i]
            if op == 0xE8:  # call rel32
                rel = struct.unpack_from("<i", blob, i + 1)[0]
                kind = "call"
            elif op == 0xEB:  # jmp rel8 (solo si el destino cae en rango)
                rel = struct.unpack_from("<b", blob, i + 1)[0]
                if not (-(1 << 16) < rel < (1 << 16)):
                    continue
                kind = "jmp"
            else:
                continue
            site_rva = vaddr + i
            dst = image_base + site_rva + 5 + rel if op == 0xE8 else \
                image_base + site_rva + 2 + rel
            if dst != target_va:
                continue
            hits += 1
            print(f"\n== {kind.upper()} directo en RVA 0x{site_rva:08X} ==")
            ctx_start = max(0, i - radius)
            ctx = blob[ctx_start:i + radius]
            for insn in md.disasm(ctx, vaddr + ctx_start):
                marker = " <<<<" if insn.address == site_rva else ""
                print(f"  {insn.address:08X}  "
                      f"{insn.mnemonic} {insn.op_str}".rstrip() + marker)
    if hits == 0:
        print("Sin llamadas directas E8/EB.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
