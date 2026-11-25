#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
find_xrefs.py — Localiza referencias indirectas (call/jmp/mov [mem]) a una
dirección VA de un PE (x86) y desensambla el contexto de cada xref.

Uso:
    python find_xrefs.py <archivo.exe> <va-hex> [radio-hex]

Imprime cada xref con instrucciones alrededor para reconstruir los payloads.
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
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10B:
        raise ValueError("No es PE32")
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
    radius = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x48
    data = open(path, "rb").read()
    image_base, secs = pe_info(data)
    if target_va < image_base:
        target_va += image_base
    target_bytes = struct.pack("<I", target_va)
    print(f"Buscando VA 0x{target_va:08X} (opcodes FF15/FF25 + disp) en "
          f"{path} (ImageBase 0x{image_base:08X})")
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    hits = 0
    for name, vsize, vaddr, rsize, rptr in secs:
        if name != ".text":
            continue
        blob = data[rptr:rptr + rsize]
        for opcode, oplen, kind in ((b"\xff\x15", 6, "call"),
                                    (b"\xff\x25", 6, "jmp"),
                                    (b"\xa1", 5, "mov eax,")):
            start = 0
            while True:
                idx = blob.find(opcode, start)
                if idx < 0:
                    break
                disp = blob[idx + oplen - 4:idx + oplen]
                if disp == target_bytes:
                    rva = vaddr + idx
                    hits += 1
                    print(f"\n== XREF {kind} en RVA 0x{rva:08X} ==")
                    ctx_start = max(0, idx - radius)
                    ctx = blob[ctx_start:idx + radius]
                    for insn in md.disasm(ctx, vaddr + ctx_start):
                        marker = " <<<<" if insn.address == rva else ""
                        print(f"  {insn.address:08X}  "
                              f"{insn.mnemonic} {insn.op_str}".rstrip()
                              + marker)
                start = idx + 1
    if hits == 0:
        print("Sin xrefs directas FF15/FF25/A1 (quizas via thunk o registro).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
