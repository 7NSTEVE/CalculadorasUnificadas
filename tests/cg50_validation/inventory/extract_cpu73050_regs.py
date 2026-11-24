#!/usr/bin/env python3
"""Extract official SH7305 register descriptors from local Casio CPU73050.dll (read-only audit)."""
import json
import pathlib
import re
import struct
import sys

CASIO_DIR = pathlib.Path(r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series")
DLL_PATH = CASIO_DIR / "CPU73050.dll"


def parse_pe_sections(data: bytes):
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    num_sec = struct.unpack_from("<H", data, pe_off + 6)[0]
    opt_sz = struct.unpack_from("<H", data, pe_off + 20)[0]
    sec_off = pe_off + 24 + opt_sz
    image_base = struct.unpack_from("<I", data, pe_off + 24 + 28)[0]
    sections = []
    for i in range(num_sec):
        s = sec_off + i * 40
        name = data[s : s + 8].rstrip(b"\0").decode("ascii", errors="replace")
        vsize, vaddr, rsize, rptr = struct.unpack_from("<4I", data, s + 8)
        sections.append((name, vaddr, vsize, rptr, rsize))
    return image_base, sections


def extract_cpu73050_registers(dll_path: pathlib.Path = DLL_PATH):
    data = dll_path.read_bytes()
    image_base, sections = parse_pe_sections(data)

    def va_to_raw(va: int):
        rva = va - image_base
        for _, vaddr, vsize, rptr, rsize in sections:
            if vaddr <= rva < vaddr + min(vsize, rsize):
                return rptr + (rva - vaddr)
        return None

    def read_cstr(va: int):
        raw = va_to_raw(va)
        if raw is None:
            return None
        end = data.find(b"\0", raw, min(len(data), raw + 120))
        if end <= raw:
            return None
        s = data[raw:end]
        if all(32 <= b < 127 for b in s):
            return s.decode("ascii")
        return None

    records = []
    for sec_name, vaddr, vsize, rptr, rsize in sections:
        if sec_name not in (".data", ".rdata"):
            continue
        off = rptr
        end_raw = rptr + rsize - 124
        while off <= end_raw:
            w = struct.unpack_from("<31I", data, off)
            name = read_cstr(w[0])
            desc = read_cstr(w[1])
            addr = w[3]
            sz = w[6]
            if (
                name
                and desc
                and 1 <= len(name) <= 28
                and 2 <= len(desc) <= 96
                and re.match(r"^[A-Za-z0-9_.\-/()+ ]+$", name)
                and sz in (1, 2, 4)
                and (addr >= 0x04000000 or addr == 0)
            ):
                records.append(
                    {
                        "raw_off": f"0x{off:X}",
                        "name": name,
                        "desc": desc,
                        "addr": addr,
                        "addr_hex": f"0x{addr:08X}",
                        "size_bytes": sz,
                        "width_bits": sz * 8,
                        "access_word": w[7],
                        "reset_word": w[4],
                        "read_rva": f"0x{w[13] - image_base:X}" if w[13] >= image_base else "0x0",
                        "write_rva": f"0x{w[17] - image_base:X}" if w[17] >= image_base else "0x0",
                        "has_read_fn": w[13] >= image_base,
                        "has_write_fn": w[17] >= image_base,
                    }
                )
                off += 124
                continue
            off += 4
    return records


def main():
    if not DLL_PATH.exists():
        print(f"ERROR: {DLL_PATH} not found", file=sys.stderr)
        return 1
    records = extract_cpu73050_registers(DLL_PATH)
    print(f"Total 124-byte register descriptors extracted from CPU73050.dll: {len(records)}")

    by_prefix = {}
    for r in records:
        pfx = f"0x{(r['addr'] >> 16) & 0xFFFF:04X}xxxx"
        by_prefix.setdefault(pfx, []).append(r)

    for pfx, items in sorted(by_prefix.items()):
        sample = ", ".join(x["name"] for x in items[:8])
        print(f"  {pfx}: {len(items):3d} regs | {sample}")

    print("=== SPECIFIC ADDRESS CHECK IN CPU73050.dll TABLE ===")
    checks = [
        ("CGM win0 0x04050100..1FF", 0x04050100, 0x040501FF),
        ("CGM win1 0xFE000000..FE3FFFFF", 0xFE000000, 0xFE3FFFFF),
        ("CGM win2 0x04080000..0FF", 0x04080000, 0x040800FF),
        ("CGM win3 0xFFFFFFE0..FFF", 0xFFFFFFE0, 0xFFFFFFFF),
        ("0x04CF0000..0FF (live MMIO hit!)", 0x04CF0000, 0x04CF00FF),
        ("0x04CB0000..0FF (BCD ALU)", 0x04CB0000, 0x04CB00FF),
        ("0xFC110000..FFF (ILC)", 0xFC110000, 0xFC11FFFF),
        ("0x04490000..0FF (TMU1)", 0x04490000, 0x044900FF),
        ("0x0413FEC0..FFF (RTC)", 0x0413FEC0, 0x0413FFFF),
        ("0x04D80000..0FF (USB2)", 0x04D80000, 0x04D800FF),
    ]
    for label, lo, hi in checks:
        hits = [r for r in records if lo <= r["addr"] <= hi]
        preview = ", ".join(f"{h['name']}@{h['addr_hex']}({h['desc']})" for h in hits[:8])
        print(f"  {label}: {len(hits)} regs -> {preview}")

    if len(sys.argv) >= 2:
        out_path = pathlib.Path(sys.argv[1])
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(records, indent=2), encoding="utf-8")
        print(f"Wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
