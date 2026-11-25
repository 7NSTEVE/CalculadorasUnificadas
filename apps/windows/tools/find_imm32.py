#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
find_imm32.py — Busca un valor dword inmediato en el .text de un PE32 y
desensambla el contexto de cada ocurrencia (para hallar `push ID`,
`mov reg, VA`, etc.).

Uso:
    python find_imm32.py <archivo.exe> <valor-hex|decimal> [radio-hex] [seccion]

Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
except ImportError:
    print("capstone no disponible")
    sys.exit(2)


def pe_info(data):
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    fh = e_lfanew + 4
    opt = fh + 20
    image_base = struct.unpack_from('<I', data, opt + 28)[0]
    n = struct.unpack_from('<H', data, fh + 2)[0]
    optsz = struct.unpack_from('<H', data, fh + 16)[0]
    so = fh + 20 + optsz
    out = []
    for i in range(n):
        s = so + i * 40
        name = data[s:s + 8].rstrip(b'\0').decode('ascii', 'replace')
        vs, va, rs, rp = struct.unpack_from('<IIII', data, s + 8)
        out.append((name, vs, va, rs, rp))
    return image_base, out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    path = sys.argv[1]
    raw = sys.argv[2]
    value = int(raw, 16) if any(c in 'xXabcdefABCDEF' for c in raw) or \
        raw.lower().startswith('0x') else int(raw)
    radius = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x40
    section = sys.argv[4] if len(sys.argv) > 4 else '.text'
    data = open(path, 'rb').read()
    image_base, secs = pe_info(data)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    needle = struct.pack('<I', value)
    print(f'Buscando dword 0x{value:X} en seccion {section} de {path}')
    hits = 0
    for name, vs, va, rs, rp in secs:
        if name != section:
            continue
        blob = data[rp:rp + rs]
        start = 0
        while True:
            idx = blob.find(needle, start)
            if idx < 0:
                break
            hits += 1
            rva = va + idx
            print(f'\n== ocurrencia en RVA 0x{rva:08X} (file off 0x{rp + idx:X}) ==')
            ctx_start = max(0, idx - radius)
            ctx = blob[ctx_start:idx + radius]
            for insn in md.disasm(ctx, va + ctx_start):
                marker = ' <<<<' if insn.address == rva else ''
                print(f'  {insn.address:08X}  {insn.mnemonic} {insn.op_str}'.rstrip() + marker)
            start = idx + 1
    if not hits:
        print('Sin ocurrencias.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
