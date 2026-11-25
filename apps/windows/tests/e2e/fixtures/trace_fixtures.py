#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
trace_fixtures.py — Execution trace generators and validators for R4 divergence testing.
Supports generating:
- Matched SH-4A native execution traces
- Matched official JSONL opcode fetch traces
- Controlled divergence traces at specified instruction ordinals
- Bisection test sequences
"""

import json
from typing import List, Tuple, Dict, Any


def generate_matching_traces(count: int = 50, start_pc: int = 0x08013820,
                             start_cycle: int = 200000000) -> Tuple[List[str], List[Dict[str, Any]]]:
    """
    Generate matched native trace lines and official JSONL records.
    """
    native_lines = []
    official_records = []

    # Standard SH-4A instruction sequence simulation (e.g., mov, add, nop, cmp)
    opcodes = ["D005", "4010", "0009", "3018", "6023", "2012", "7001", "8902"]

    for i in range(count):
        idx = i + 1
        pc = start_pc + (i * 2)
        op = opcodes[i % len(opcodes)]
        cyc = start_cycle + (i * 2)

        # Native line format: "00000001 PC=0x08013820 OP=0xD005 CYC=200000000"
        nat_line = f"{idx:08d} PC=0x{pc:08X} OP=0x{op} CYC={cyc}"
        native_lines.append(nat_line)

        # Official JSONL record: {"event":"fetch","index":1,"guest_pc_ebp":"08013820","opcode":"D005"}
        off_rec = {
            "event": "fetch",
            "index": idx,
            "guest_pc_ebp": f"{pc:08X}",
            "opcode": op,
            "cycles": cyc
        }
        official_records.append(off_rec)

    return native_lines, official_records


def generate_diverging_traces(count: int = 50, diverge_at: int = 25,
                              diverge_type: str = "OPCODE") -> Tuple[List[str], List[Dict[str, Any]]]:
    """
    Generate traces that match up to `diverge_at - 1`, and diverge at `diverge_at`.
    diverge_type:
      - "OPCODE": Different opcode at same PC
      - "PC": Different jump target / PC branch
      - "CYCLE": Substantial cycle delta
    """
    native_lines, official_records = generate_matching_traces(count)

    if 1 <= diverge_at <= count:
        target_idx = diverge_at - 1
        orig_rec = official_records[target_idx]
        divergent_rec = dict(orig_rec)

        if diverge_type == "OPCODE":
            divergent_rec["opcode"] = "FFFF"
        elif diverge_type == "PC":
            divergent_rec["guest_pc_ebp"] = "08099999"
        elif diverge_type == "CYCLE":
            divergent_rec["cycles"] = orig_rec["cycles"] + 50000

        official_records[target_idx] = divergent_rec

    return native_lines, official_records


def compare_traces_in_memory(native_lines: List[str], official_records: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    Deterministic trace comparator matching Cg50TraceDiff logic.
    Identifies exact first differing instruction ordinal.
    """
    mismatches = []
    compared = 0
    limit = min(len(native_lines), len(official_records))

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
        off_pc = "0x" + off["guest_pc_ebp"]
        off_op = "0x" + off["opcode"]

        if nat_pc != off_pc or nat_op.upper() != off_op.upper():
            mismatches.append({
                "ordinal": i + 1,
                "native_pc": nat_pc,
                "official_pc": off_pc,
                "native_op": nat_op,
                "official_op": off_op
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
