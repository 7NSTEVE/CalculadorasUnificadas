#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
disasm_pe.py — Volcado y desensamblado dirigido de funciones de un PE (Win32 x86).

Uso:
    python disasm_pe.py <archivo.dll> <rva-hex> [bytes-hex] [rva2 rva3 ...]
    python disasm_pe.py <archivo.dll> exports

Mapea RVA→offset de archivo con la tabla de secciones; desensambla con capstone.
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
except ImportError:
    print("capstone no disponible: pip install capstone")
    sys.exit(2)


def sections(data: bytes):
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        raise ValueError("No es un PE")
    file_header = e_lfanew + 4
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
    return out


def rva_to_offset(data: bytes, rva: int):
    """Mapea un RVA a offset de archivo usando la tabla de secciones del PE."""
    for name, virt_size, virt_addr, raw_size, raw_ptr in sections(data):
        if virt_addr <= rva < virt_addr + max(virt_size, raw_size):
            if rva - virt_addr >= raw_size:
                return None, name  # RVA dentro de la sección pero sin respaldo
            return raw_ptr + (rva - virt_addr), name
    return None, "?"


def disasm(data: bytes, rva: int, size: int) -> None:
    off, section = rva_to_offset(data, rva)
    print(f"== RVA 0x{rva:08X} (sección {section}, file offset "
          f"{'0x%X' % off if off is not None else 'SIN-DATOS'}) ==")
    if off is None:
        print("  (RVA sin respaldo en archivo: memoria solo-inicializada)")
        return
    code = data[off:off + size]
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    count = 0
    for insn in md.disasm(code, rva):
        text = f"{insn.mnemonic} {insn.op_str}".rstrip()
        print(f"  {insn.address:08X}  {text}")
        count += 1
    if count == 0:
        print("  (nada desensamblable)")


def exports(data: bytes):
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    opt = e_lfanew + 4 + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10B:
        print("No es PE32")
        return
    dd = opt + 96
    export_rva, export_size = struct.unpack_from("<II", data, dd)
    if export_rva == 0:
        print("Sin exports")
        return
    exp_off, _ = rva_to_offset(data, export_rva)
    if exp_off is None:
        return
    (_char, _ts, _maj, _min, name_rva, _base, _n_funcs, n_names,
     addr_funcs, addr_names, addr_ords) = \
        struct.unpack_from("<IIHHIIIIIII", data, exp_off)
    names = {}
    for i in range(n_names):
        nrva = struct.unpack_from(
            "<I", data, exp_off + (addr_names - export_rva) + i * 4)[0]
        ordv = struct.unpack_from(
            "<H", data, exp_off + (addr_ords - export_rva) + i * 2)[0]
        noff, _ = rva_to_offset(data, nrva)
        if noff is None:
            continue
        end = data.index(b"\x00", noff)
        names[data[noff:end].decode("ascii", "replace")] = ordv
    for name, ordv in names.items():
        arva = struct.unpack_from(
            "<I", data, exp_off + (addr_funcs - export_rva) + ordv * 4)[0]
        print(f"EXPORT {name} rva=0x{arva:08X}")


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    path = sys.argv[1]
    data = open(path, "rb").read()
    if sys.argv[2] in ("--exports", "exports"):
        exports(data)
        return 0
    rva = int(sys.argv[2], 16)
    size = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x100
    disasm(data, rva, size)
    for extra in sys.argv[4:]:
        disasm(data, int(extra, 16), size)
    return 0


if __name__ == "__main__":
    sys.exit(main())
