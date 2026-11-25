#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
find_field_access.py — Encuentra instrucciones x86 que leen/escriben un campo
con desplazamiento dado (p. ej. +0x238) en un PE32 y desensambla el contexto.

Uso:
    python find_field_access.py <archivo.exe> <disp-hex> [radio-hex]

Cubre: mov/lea/cmp/push de [reg+disp32] (opcode C7, 89, 8B, 3B, 39, 8D, FF…)
usando capstone sobre toda la sección .text alineada por búsqueda de patrones
de 6 bytes <op> <mod=10> <reg field> <disp32>.
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


# (opcode, nombre) de formas comunes con modrm mod=10 (reg+disp32)
OPCODES = {
    0xC7: "mov [r+d],imm32",
    0x89: "mov [r+d],r",
    0x8B: "mov r,[r+d]",
    0x39: "cmp [r+d],r",
    0x3B: "cmp r,[r+d]",
    0x8D: "lea r,[r+d]",
    0xFF: "inc/jmp/call [r+d]",
    0xFF - 0x100: "n/a",
}


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    path = sys.argv[1]
    disp = int(sys.argv[2], 16)
    radius = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x30
    data = open(path, "rb").read()
    image_base, secs = pe_info(data)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    print(f"Buscando accesos a [reg+0x{disp:X}] en {path}")
    for name, vsize, vaddr, rsize, rptr in secs:
        if name != ".text":
            continue
        blob = data[rptr:rptr + rsize]
        found = 0
        for op, desc in OPCODES.items():
            if op < 0:
                continue
            for rm in range(8):  # reg base field en modrm (mod=10)
                modrm = 0x80 | (0 << 3) | rm  # reg field 0, variará
                # buscamos cualquier reg-field: 8 variantes
                for regf in range(8):
                    modrm = 0x80 | (regf << 3) | rm
                    if rm == 4:  # SIB: requiere byte extra, no cubierto aquí
                        continue
                    needle = bytes([op, modrm]) + struct.pack("<I", disp)
                    start = 0
                    while True:
                        idx = blob.find(needle, start)
                        if idx < 0:
                            break
                        # desensambla desde unos bytes antes para contexto
                        found += 1
                        site_rva = vaddr + idx
                        print(f"\n== {desc} [reg=rm{rm}] en RVA 0x{site_rva:08X} ==")
                        ctx_start = max(0, idx - radius)
                        ctx = blob[ctx_start:idx + radius]
                        for insn in md.disasm(ctx, vaddr + ctx_start):
                            marker = " <<<<" if insn.address == site_rva else ""
                            print(f"  {insn.address:08X}  "
                                  f"{insn.mnemonic} {insn.op_str}".rstrip()
                                  + marker)
                        start = idx + 1
        if not found:
            print("  (sin accesos con disp32 directo)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
