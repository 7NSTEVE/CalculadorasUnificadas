#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
divergence_locator.py — Multi-tier automated first divergence localization engine.
Part of the Plataforma Integrada de Pruebas Diferenciales (Requirement R4).

Tiers:
  - Tier A (Macro): Keystroke-level bisection & step-by-step lockstep execution.
  - Tier B (Meso): Memory window discrepancy isolation (History, Work, Display buffers).
  - Tier C (Micro): Instruction trace alignment (ordinals, PC, opcodes).
"""

from dataclasses import dataclass, asdict
from enum import Enum
import json
import os
import sys
import time
from typing import List, Dict, Any, Optional, Tuple, Callable

# Standard Casio OS memory addresses (SH7305 guest architecture)
CASIO_MEM_WINDOWS = {
    "History": (0x880D3400, 0x0E00),  # Expression history & tokens (3584 B)
    "Work":    (0x88184D00, 0x1100),  # Active calculation registers / scratchpad (4352 B)
    "Display": (0x8804F800, 0x0400),  # Text screen character matrix (1024 B)
    "FullRAM": (0x88000000, 0x200000) # 2 MB guest RAM
}


class DivergenceType(str, Enum):
    LCD_MISMATCH = "LCD_MISMATCH"
    LCD_PIXEL_MISMATCH = "LCD_PIXEL_MISMATCH"
    REGISTER_MISMATCH = "REGISTER_MISMATCH"
    MEMORY_MISMATCH = "MEMORY_MISMATCH"
    CRASH = "CRASH"


@dataclass
class MemoryDiscrepancy:
    buffer_name: str
    base_address: str
    divergent_address: str
    offset: int
    native_byte: str
    official_byte: str
    native_context: str
    official_context: str
    total_bytes_compared: int

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class TraceDiscrepancy:
    ordinal: int
    native_pc: str
    official_pc: str
    native_op: str
    official_op: str
    message: str = ""
    line_number: int = 0

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class FirstDivergence:
    step: int
    key: str
    divergence_type: str
    details: str
    diff_bbox: Optional[List[int]] = None
    changed_pixels: int = 0
    changed_percent: float = 0.0
    diff_image: Optional[str] = None
    native_pc: Optional[str] = None
    official_pc: Optional[str] = None
    memory_discrepancy: Optional[Dict[str, Any]] = None
    trace_discrepancy: Optional[Dict[str, Any]] = None

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


class DivergenceLocator:
    """
    Orchestrates multi-tier divergence isolation across keystrokes, memory buffers, and instruction traces.
    """

    def __init__(self, out_dir: str = "", stop_on_first: bool = True, pixel_tolerance: int = 4):
        self.out_dir = out_dir or os.getcwd()
        self.stop_on_first = stop_on_first
        self.pixel_tolerance = pixel_tolerance
        if self.out_dir:
            os.makedirs(self.out_dir, exist_ok=True)

    def bisect_keystrokes(
        self,
        key_sequence: str,
        step_executor: Callable[[int, str], Tuple[Dict[str, Any], Dict[str, Any], Dict[str, Any]]],
        build_control: Dict[str, Any]
    ) -> Tuple[int, Dict[str, Any]]:
        """
        Execute key sequence step-by-step; halt on first divergence if stop_on_first is True.
        
        step_executor: callback(step_idx, key) -> (native_state, official_state, comparison_result)
        Returns: (exit_code, report)
        """
        keys = [k.strip() for k in key_sequence.split(",") if k.strip()]
        total_steps = len(keys)
        steps_executed = 0
        steps = []
        first_div: Optional[Dict[str, Any]] = None

        for idx, key in enumerate(keys, start=1):
            steps_executed += 1
            nat_st, off_st, comp_res = step_executor(idx, key)

            step_entry = {
                "step": idx,
                "key": key,
                "native": nat_st,
                "official": off_st,
                "comparison": comp_res
            }
            steps.append(step_entry)

            # Check if this step diverged
            is_diff = comp_res.get("status") == "DIFF"
            if is_diff and first_div is None:
                # Classify divergence
                px_diff = comp_res.get("pixel_diff", {})
                ch_px = px_diff.get("changed_pixels", 0)
                ch_pct = px_diff.get("changed_percent", 0.0)
                bbox = px_diff.get("diff_bbox")
                diff_png = px_diff.get("diff_png") or px_diff.get("diff_image")

                st_diff = comp_res.get("state_diff", {})
                if comp_res.get("crash", False) or nat_st.get("crashed", False) or off_st.get("crashed", False):
                    div_type = DivergenceType.CRASH.value
                elif not st_diff.get("pc_match", True):
                    div_type = DivergenceType.REGISTER_MISMATCH.value
                elif comp_res.get("memory_discrepancy"):
                    div_type = DivergenceType.MEMORY_MISMATCH.value
                else:
                    div_type = DivergenceType.LCD_PIXEL_MISMATCH.value

                first_div = {
                    "step": idx,
                    "key": key,
                    "divergence_type": div_type,
                    "details": comp_res.get("details") or f"Divergence detected at step {idx} ('{key}'): {ch_px} pixels changed ({ch_pct:.2f}%)",
                    "diff_bbox": list(bbox) if bbox else None,
                    "changed_pixels": ch_px,
                    "changed_percent": ch_pct,
                    "diff_image": diff_png,
                    "native_pc": nat_st.get("pc"),
                    "official_pc": off_st.get("pc"),
                    "memory_discrepancy": comp_res.get("memory_discrepancy"),
                    "trace_discrepancy": comp_res.get("trace_discrepancy")
                }

                if self.stop_on_first:
                    break

        verdict = "PASS" if first_div is None else "DIVERGENCIA"
        report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "build_control": build_control,
            "verdict": verdict,
            "total_steps": total_steps,
            "steps_executed": steps_executed,
            "first_divergence": first_div,
            "steps": steps
        }

        exit_code = 0 if verdict == "PASS" else 1
        return exit_code, report

    @staticmethod
    def isolate_memory_divergence(
        buffer_name: str,
        base_addr: int,
        native_bytes: bytes,
        official_bytes: bytes
    ) -> Optional[Dict[str, Any]]:
        """
        Byte-level comparison of memory buffers, returning the exact first differing offset.
        """
        limit = min(len(native_bytes), len(official_bytes))
        for offset in range(limit):
            if native_bytes[offset] != official_bytes[offset]:
                diff_addr = base_addr + offset
                ctx_start = max(0, offset - 8)
                ctx_end = min(limit, offset + 9)
                return {
                    "buffer_name": buffer_name,
                    "base_address": f"0x{base_addr:08X}",
                    "divergent_address": f"0x{diff_addr:08X}",
                    "offset": offset,
                    "native_byte": f"0x{native_bytes[offset]:02X}",
                    "official_byte": f"0x{official_bytes[offset]:02X}",
                    "native_context": native_bytes[ctx_start:ctx_end].hex().upper(),
                    "official_context": official_bytes[ctx_start:ctx_end].hex().upper(),
                    "total_bytes_compared": limit
                }
        return None

    @staticmethod
    def compare_memory_windows(
        native_buffers: Dict[str, bytes],
        official_buffers: Dict[str, bytes]
    ) -> Optional[Dict[str, Any]]:
        """
        Compare known Casio RAM buffers (History, Work, Display) and return the first discrepancy.
        """
        for buf_name, (base_addr, expected_size) in CASIO_MEM_WINDOWS.items():
            if buf_name not in native_buffers or buf_name not in official_buffers:
                continue

            nat_bytes = native_buffers[buf_name]
            off_bytes = official_buffers[buf_name]
            disc = DivergenceLocator.isolate_memory_divergence(
                buf_name, base_addr, nat_bytes, off_bytes
            )
            if disc:
                return disc
        return None

    @staticmethod
    def compare_instruction_traces(
        native_lines: List[str],
        official_records: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """
        In-memory instruction trace comparator conforming to Cg50TraceDiff logic.
        Handles empty inputs safely returning ERR_TRACE_EMPTY.
        """
        if not native_lines or not official_records:
            return {
                "status": "ERROR",
                "exit_code": 2,
                "error": "ERR_TRACE_EMPTY",
                "details": "One or both trace inputs are empty"
            }

        limit = min(len(native_lines), len(official_records))
        mismatches = []
        compared = 0

        for i in range(limit):
            compared += 1
            nat_parts = native_lines[i].split()
            nat_pc = None
            nat_op = None
            for p in nat_parts:
                if p.startswith("PC="):
                    nat_pc = p[3:].strip()
                elif p.startswith("OP="):
                    nat_op = p[3:].strip()

            off = official_records[i]
            raw_off_pc = off.get("guest_pc_ebp", "")
            off_pc = "0x" + raw_off_pc if not raw_off_pc.startswith("0x") else raw_off_pc
            raw_off_op = off.get("opcode", "")
            off_op = "0x" + raw_off_op if not raw_off_op.startswith("0x") else raw_off_op

            if (nat_pc and nat_pc.upper() != off_pc.upper()) or (nat_op and nat_op.upper() != off_op.upper()):
                mismatches.append({
                    "ordinal": i + 1,
                    "native_pc": nat_pc or "UNKNOWN",
                    "official_pc": off_pc,
                    "native_op": nat_op or "UNKNOWN",
                    "official_op": off_op,
                    "message": "Opcode or PC deviation at instruction fetch",
                    "line_number": i + 1
                })
                break  # First divergence found

        if mismatches:
            return {
                "status": "DIFF",
                "first_divergence": mismatches[0],
                "total_compared": compared
            }
        return {
            "status": "EQUAL",
            "first_divergence": None,
            "total_compared": compared
        }

    @staticmethod
    def format_console_report(report: Dict[str, Any]) -> str:
        """
        Format clean, readable terminal output.
        """
        lines = [
            "=" * 80,
            "  DIFFERENTIAL TESTING PLATFORM — DIVERGENCE REPORT",
            "=" * 80,
            f"  Verdict          : {report.get('verdict')}",
            f"  Total Steps      : {report.get('total_steps')}",
            f"  Steps Executed   : {report.get('steps_executed')}",
        ]
        bc = report.get("build_control", {})
        if bc:
            lines.extend([
                f"  Build Status     : {bc.get('status')} ({bc.get('compiler', 'N/A')})",
                f"  Binary Target    : {bc.get('target', 'N/A')}",
                f"  Binary SHA-256   : {bc.get('binary_sha256', 'N/A')[:32]}...",
            ])
        fd = report.get("first_divergence")
        if fd:
            lines.extend([
                "-" * 80,
                "  FIRST DIVERGENCE LOCALIZED:",
                f"  Step Number      : #{fd.get('step')} (1-based)",
                f"  Triggering Key   : '{fd.get('key')}'",
                f"  Divergence Type  : {fd.get('divergence_type')}",
                f"  Details          : {fd.get('details')}",
                f"  Pixel BBox       : {fd.get('diff_bbox')}",
                f"  Changed Pixels   : {fd.get('changed_pixels')} ({fd.get('changed_percent', 0.0):.2f}%)",
                f"  Native PC        : {fd.get('native_pc')}",
                f"  Official PC      : {fd.get('official_pc')}",
            ])
            mem = fd.get("memory_discrepancy")
            if mem:
                lines.extend([
                    f"  Memory Buffer    : {mem.get('buffer_name')} at {mem.get('divergent_address')}",
                    f"  Byte Difference  : Native={mem.get('native_byte')} vs Official={mem.get('official_byte')}"
                ])
            tr = fd.get("trace_discrepancy")
            if tr:
                lines.extend([
                    f"  Trace Ordinal    : #{tr.get('ordinal')}",
                    f"  Trace PC / Op    : Nat({tr.get('native_pc')}, {tr.get('native_op')}) vs Off({tr.get('official_pc')}, {tr.get('official_op')})"
                ])
        lines.append("=" * 80)
        return "\n".join(lines)


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description="Multi-tier divergence locator CLI")
    parser.add_argument("--trace-native", help="Path to native execution trace file")
    parser.add_argument("--trace-official", help="Path to official JSONL fetch trace file")
    parser.add_argument("--report-json", help="Path to report JSON to format")

    args = parser.parse_args(argv)

    if args.report_json:
        with open(args.report_json, "r", encoding="utf-8") as f:
            data = json.load(f)
        print(DivergenceLocator.format_console_report(data))
        return 0

    if args.trace_native and args.trace_official:
        with open(args.trace_native, "r", encoding="utf-8") as f:
            nat_lines = [line.strip() for line in f if line.strip() and not line.startswith("#")]
        off_records = []
        with open(args.trace_official, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    off_records.append(json.loads(line))
        res = DivergenceLocator.compare_instruction_traces(nat_lines, off_records)
        print(json.dumps(res, indent=2))
        return 0 if res.get("status") == "EQUAL" else 1

    parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
