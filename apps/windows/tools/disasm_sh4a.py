#!/usr/bin/env python3
"""Desensamblador SH-4A (SH7305 del CG50) para volcados guest.

Uso:
  python disasm_sh4a.py <archivo.bin> <base-hex> [--start hex] [--stop hex]
                        [--order raw|swap|auto] [--count n]

El probe F1.3 escribe cada palabra de 16 bits del guest como
[byte bajo][byte alto] del valor devuelto por method5; si --order swap
el archivo se reordena por pares a orden big-endian de guest real.
"""
import argparse
import sys

from capstone import Cs, CS_ARCH_SH, CS_MODE_SH4A, CS_MODE_BIG_ENDIAN


def swap_pairs(data: bytes) -> bytes:
    out = bytearray(data)
    for i in range(0, len(out) - 1, 2):
        out[i], out[i + 1] = out[i + 1], out[i]
    return bytes(out)


def build_disassembler() -> Cs:
    md = Cs(CS_ARCH_SH, CS_MODE_SH4A | CS_MODE_BIG_ENDIAN)
    md.skipdata = True
    return md


def branch_score(md: Cs, data: bytes, base: int, start: int, stop: int) -> int:
    offset = start - base
    blob = data[offset:stop - base]
    score = 0
    for insn in md.disasm(blob, start):
        mnemonic = insn.mnemonic
        if mnemonic.startswith(("bt", "bf", "bra", "bsr", "jmp", "jsr",
                                "bt/s", "bf/s", "rts", "rte")):
            score += 3
        elif "@(disp,pc)" in insn.op_str or "@(disp,gbr)" in insn.op_str:
            score += 1
        if mnemonic == ".byte":
            score -= 2
    return score


def render(md: Cs, data: bytes, base: int, start: int, stop: int,
           count: int) -> None:
    offset = start - base
    blob = data[offset:stop - base]
    printed = 0
    for insn in md.disasm(blob, start):
        text = f"{insn.bytes.hex(' '):<16} {insn.mnemonic} {insn.op_str}"
        print(f"{insn.address:08X}  {text.rstrip()}")
        printed += 1
        if count and printed >= count:
            break


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("file")
    parser.add_argument("base")
    parser.add_argument("--start")
    parser.add_argument("--stop")
    parser.add_argument("--order", choices=["raw", "swap", "auto"],
                        default="auto")
    parser.add_argument("--count", type=int, default=0)
    args = parser.parse_args()

    with open(args.file, "rb") as handle:
        data = handle.read()
    base = int(args.base, 16)
    start = int(args.start, 16) if args.start else base
    stop = int(args.stop, 16) if args.stop else base + len(data)
    stop = min(stop, base + len(data))
    if start < base or start >= stop:
        print(f"rango invalido start={start:08X} stop={stop:08X} "
              f"base={base:08X} size={len(data)}", file=sys.stderr)
        return 2

    md = build_disassembler()
    if args.order == "raw":
        render(md, data, base, start, stop, args.count)
        return 0
    if args.order == "swap":
        render(md, swap_pairs(data), base, start, stop, args.count)
        return 0

    raw_score = branch_score(md, data, base, start, stop)
    swapped = swap_pairs(data)
    swap_score = branch_score(md, swapped, base, start, stop)
    print(f"; auto: score raw={raw_score} swap={swap_score}")
    if swap_score > raw_score:
        print("; usando orden swap (pares invertidos)")
        render(md, swapped, base, start, stop, args.count)
    else:
        print("; usando orden raw")
        render(md, data, base, start, stop, args.count)
    return 0


if __name__ == "__main__":
    sys.exit(main())
