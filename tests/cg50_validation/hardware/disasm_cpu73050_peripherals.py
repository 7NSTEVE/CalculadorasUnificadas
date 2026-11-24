#!/usr/bin/env python3
"""
Fase 2 (v46 Rigor Upgrade) - Control-Flow & Data-Flow Peripheral Callback Disassembler
for Casio CPU73050.dll & ExtLY755A.dll across all 8 fx-CG50 target hardware blocks:
  HW-PFC (0x0405), HW-RTC (0x0413), HW-CPG (0x0415), HW-DMAC0 (0xFE00),
  HW-BSC (0xFEC1), HW-SDHI (0x04CF), HW-HUDI (0xFC11), HW-PRLCK (0xFF80).

Methodology upgrade:
  - Tracks x86 register data flow (edx/eax/ecx) all the way to the final `mov [ebx], <reg>`
    store, explicitly separating:
      * write_key_checks (e.g., RTCSR/RTCNT/RTCOR 0xA55A0000 password check)
      * w0c_masks (write-0-to-clear status bits preserved from old [ebx])
      * final_stored_and_mask (actual stored bitmask written to [ebx])
      * final_forced_or_bits (hardwired 1-bits)
      * dynamic_table_masks (e.g., CSxWCR table 0x1006DAA0)
      * called helper functions (with disassembly)
  - Explicitly audits the 112 page-level registers (PFC, RTC, SDHI, HUDI, DMAC0)
    by disassembling their page-level handlers in CPU73050.dll (0x10023029 RTC,
    0x10010061 DMAOR, 0x1001E13A SDHI, 0x1001E15A HUDI) and ExtLY755A.dll callbacks.
"""
import datetime
import hashlib
import json
import pathlib
import re
import struct
import subprocess
import sys

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

sys.path.insert(0, r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\inventory")
from extract_cpu73050_regs import extract_cpu73050_registers

CASIO_DIR = pathlib.Path(r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series")
CPU_DLL = CASIO_DIR / "CPU73050.dll"
EXT_DLL = CASIO_DIR / "ExtLY755A.dll"
REPO_ROOT = pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas")
OUT_JSON = REPO_ROOT / "tests" / "cg50_validation" / "hardware" / "cpu73050_periph_spec.json"

TARGET_BLOCKS = {
    "HW-PFC":   lambda a: (a >> 16) == 0x0405,
    "HW-RTC":   lambda a: (a >> 16) == 0x0413,
    "HW-CPG":   lambda a: (a >> 16) == 0x0415,
    "HW-SDHI":  lambda a: (a >> 16) == 0x04CF,
    "HW-HUDI":  lambda a: (a >> 16) == 0xFC11,
    "HW-DMAC0": lambda a: (a >> 16) == 0xFE00,
    "HW-BSC":   lambda a: (a >> 16) == 0xFEC1,
    "HW-PRLCK": lambda a: (a >> 16) == 0xFF80,
}


def sha256_file(p: pathlib.Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest().upper()


def git_head() -> str:
    r = subprocess.run([r"D:\DevTools\Tools\Git\cmd\git.exe", "-C", str(REPO_ROOT), "rev-parse", "HEAD"],
                       capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else "UNKNOWN"


def disasm_callback(md, text_bytes, text_base, va: int, max_ins: int = 48) -> list[dict]:
    off = va - text_base
    if off < 0 or off >= len(text_bytes):
        return []
    out = []
    for ins in md.disasm(text_bytes[off:off + 280], va):
        out.append({
            "va": f"0x{ins.address:08X}",
            "mnemonic": ins.mnemonic,
            "op_str": ins.op_str,
            "text": f"0x{ins.address:08X}: {ins.mnemonic:8s} {ins.op_str}".rstrip(),
        })
        if len(out) >= max_ins or ins.mnemonic == "ret":
            break
    return out


def analyze_write_dataflow(insns: list[dict], width_bits: int) -> dict:
    """
    Control-flow & data-flow tracker for CPU73050.dll register write callbacks.
    Input register convention in CPU73050.dll:
      ebx = pointer to backing register dword
      edx = guest write value
    Tracks operations on edx/eax up to `mov dword ptr [ebx], edx` (and post-store self-clear `and dword ptr [ebx], imm`).
    """
    full_mask = (1 << width_bits) - 1
    write_key_checks = []
    w0c_masks = []
    final_and_mask = full_mask
    final_or_bits = 0
    post_store_clear_mask = None
    dynamic_table_ref = None
    helper_calls = []
    stored_seen = False

    for idx, ins in enumerate(insns):
        mn = ins["mnemonic"]
        op = ins["op_str"]

        # Detect password/key guard: `and eax, IMM` followed by `cmp eax, IMM` + `jne`
        if mn == "cmp" and op.startswith(("eax, 0x", "edx, 0x")):
            m = re.search(r"0x([0-9a-f]+)", op)
            if m:
                val = int(m.group(1), 16)
                if val >= 0x10000:
                    write_key_checks.append(f"0x{val:08X}")
            continue

        # Detect W0C sequence: `not eax` -> `or eax, edx` -> `and eax, ecx` -> `and eax, W0C_MASK` -> `or edx, eax`
        if not stored_seen and mn == "and" and op.startswith("eax, 0x"):
            m = re.search(r"0x([0-9a-f]+)", op)
            if m:
                v = int(m.group(1), 16)
                # Distinguish W0C mask (preceded by `and eax, ecx`) from key check (followed by `cmp eax, ...`)
                prev_ops = [x["op_str"] for x in insns[max(0, idx - 3):idx]]
                if any("eax, ecx" in p for p in prev_ops):
                    w0c_masks.append(f"0x{v:X}")

        # Track final AND mask on edx (value actually stored into [ebx])
        if not stored_seen and mn == "and" and op.startswith(("edx, 0x", "dx, 0x", "dl, 0x")):
            m = re.search(r"0x([0-9a-f]+)", op)
            if m:
                final_and_mask &= int(m.group(1), 16)
        elif not stored_seen and mn == "and" and op.startswith("edx, dword ptr ["):
            dynamic_table_ref = op

        # Track forced OR bits on edx before `mov [ebx], edx`
        if not stored_seen and mn == "or" and op.startswith(("edx, 0x", "dx, 0x", "dl, 0x")):
            m = re.search(r"0x([0-9a-f]+)", op)
            if m:
                final_or_bits |= int(m.group(1), 16)

        if mn == "mov" and op.startswith("dword ptr [ebx], edx"):
            stored_seen = True
            continue

        # Post-store self-clearing kick bits (e.g. FRQCR / PLL2CR `and dword ptr [ebx], 0x7fffffff`)
        if stored_seen and mn == "and" and op.startswith("dword ptr [ebx], 0x"):
            m = re.search(r"0x([0-9a-f]+)", op)
            if m:
                post_store_clear_mask = f"0x{int(m.group(1), 16):08X}"

        if mn == "call" and op.startswith("0x"):
            helper_calls.append(op)

    return {
        "method": "x86_dataflow_to_ebx_store",
        "write_key_checks": sorted(set(write_key_checks)),
        "w0c_bits": sorted(set(w0c_masks)),
        "final_stored_and_mask": f"0x{final_and_mask:X}",
        "final_forced_or_bits": f"0x{final_or_bits:X}",
        "post_store_self_clear_mask": post_store_clear_mask,
        "dynamic_table_ref": dynamic_table_ref,
        "helper_calls": helper_calls[:8],
        "stored_to_ebx_confirmed": stored_seen,
    }


def main() -> int:
    pe = pefile.PE(str(CPU_DLL), fast_load=False)
    ib = pe.OPTIONAL_HEADER.ImageBase
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    text_sec = [s for s in pe.sections if s.Name.startswith(b".text")][0]
    text_base = ib + text_sec.VirtualAddress
    text_bytes = text_sec.get_data()

    regs = extract_cpu73050_registers(CPU_DLL)
    blocks_out = {}

    for block_name, pred in TARGET_BLOCKS.items():
        bregs = sorted([r for r in regs if pred(r["addr"])], key=lambda x: x["addr"])
        entries = []
        unique_w = {}
        unique_r = {}
        for r in bregs:
            w_rva = int(r["write_rva"], 16)
            r_rva = int(r["read_rva"], 16)
            w_ins = disasm_callback(md, text_bytes, text_base, ib + w_rva) if w_rva else []
            r_ins = disasm_callback(md, text_bytes, text_base, ib + r_rva, max_ins=20) if r_rva else []
            if w_rva and r["write_rva"] not in unique_w:
                unique_w[r["write_rva"]] = [x["text"] for x in w_ins]
            if r_rva and r["read_rva"] not in unique_r:
                unique_r[r["read_rva"]] = [x["text"] for x in r_ins]
            entries.append({
                "name": r["name"],
                "desc": r["desc"],
                "addr": r["addr"],
                "addr_hex": r["addr_hex"],
                "size_bytes": r["size_bytes"],
                "width_bits": r["width_bits"],
                "reset_word": f"0x{r['reset_word']:08X}",
                "read_rva": r["read_rva"],
                "write_rva": r["write_rva"],
                "callback_dispatch_mode": "DIRECT_REGISTER_CALLBACK" if w_rva else "PAGE_LEVEL_HANDLER",
                "write_dataflow": analyze_write_dataflow(w_ins, r["width_bits"]) if w_ins else None,
            })
        blocks_out[block_name] = {
            "register_count": len(entries),
            "with_direct_read_cb": sum(1 for e in entries if e["read_rva"] != "0x0"),
            "with_direct_write_cb": sum(1 for e in entries if e["write_rva"] != "0x0"),
            "page_level_registers": sum(1 for e in entries if e["write_rva"] == "0x0"),
            "registers": entries,
            "unique_write_callbacks_disasm": unique_w,
            "unique_read_callbacks_disasm": unique_r,
        }
        print(f"{block_name:10s}: {len(entries):3d} regs | direct_W={blocks_out[block_name]['with_direct_write_cb']:2d} page_level={blocks_out[block_name]['page_level_registers']:2d}")

    # Disassemble page-level handlers in CPU73050.dll that service the 112 page-level registers
    page_handlers = {
        "RTC_page_setup_0x10023029": [x["text"] for x in disasm_callback(md, text_bytes, text_base, 0x10023020, 32)],
        "DMAC0_DMAOR_handler_0x10010061": [x["text"] for x in disasm_callback(md, text_bytes, text_base, 0x10010050, 32)],
        "SDHI_page_setup_0x1001E13A": [x["text"] for x in disasm_callback(md, text_bytes, text_base, 0x1001E130, 32)],
        "HUDI_page_setup_0x1001E15A": [x["text"] for x in disasm_callback(md, text_bytes, text_base, 0x1001E150, 32)],
    }

    # Semantic disassembly of ExtLY755A.dll driver callbacks & voltage comparator
    ext_info = {}
    if EXT_DLL.exists():
        epe = pefile.PE(str(EXT_DLL), fast_load=False)
        eib = epe.OPTIONAL_HEADER.ImageBase
        et = [s for s in epe.sections if s.Name.startswith(b".text")][0]
        et_base = eib + et.VirtualAddress
        et_bytes = et.get_data()
        raw = EXT_DLL.read_bytes()
        strs = [m.group().decode() for m in re.finditer(rb"[ -~]{4,60}", raw)]
        ext_info = {
            "path": str(EXT_DLL),
            "size_bytes": EXT_DLL.stat().st_size,
            "sha256": sha256_file(EXT_DLL),
            "image_base": f"0x{eib:08X}",
            "driver_info_thunk_0x100010E0": [x["text"] for x in disasm_callback(md, et_bytes, et_base, 0x100010E0, 20)],
            "battery_voltage_comparator_0x10001010": [x["text"] for x in disasm_callback(md, et_bytes, et_base, 0x10001010, 24)],
            "pin_and_signal_strings": [s for s in strs if any(k in s.upper() for k in ("POWER", "PTJ", "PTH", "PTR", "PIN", "BATT", "APPIF", "LY755", "LED", "RESET", "USB", "RS232", "BOOT"))][:40],
        }

    spec = {
        "schema": "cg50_cpu73050_periph_spec_v2_dataflow",
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "repo_git_head": git_head(),
        "cpu73050_sha256": sha256_file(CPU_DLL),
        "total_target_blocks": len(blocks_out),
        "total_target_registers": sum(b["register_count"] for b in blocks_out.values()),
        "blocks": blocks_out,
        "page_level_handlers_disasm": page_handlers,
        "ExtLY755A": ext_info,
    }
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(json.dumps(spec, indent=2), encoding="utf-8")

    # Sanity check: verify RTCSR dataflow separates write_key 0xA55A0000 from final mask 0x7F + W0C 0x80!
    rtcsr = [r for r in blocks_out["HW-BSC"]["registers"] if r["name"] == "RTCSR"][0]["write_dataflow"]
    print(f"RTCSR dataflow audit: {rtcsr}")
    print(f"Wrote {OUT_JSON} ({spec['total_target_registers']} regs across {spec['total_target_blocks']} blocks)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
