#!/usr/bin/env python3
"""
Step 8 — Expanded Isolated Sandbox Mutation Tester (Certification v47: 108 Non-Equivalent Mutants)

Copies core/, tests/core/ and tests/cg50_validation/hardware/ into build-mutation-sandbox/
(NEVER mutates the live workspace!), builds baseline with Ninja + GCC 16.1.0, verifies 100%
baseline PASS, then injects 108 controlled non-equivalent architectural mutants across ALL 22+
SH7305 & fx-CG50 subsystems:
  SH-4A CPU Integer/ALU/Shift/Rotate/Mul/Div/Branch/DelaySlot/No-FPU, SH4AL-DSP, BCD ALU,
  MMU/TLB/Exceptions, IC/OC Cache & OCRAM/RAMCR, UBC, INTC2, ETMU, ADC, DD Bus, SDHI, HUDI,
  PRLCK, SCIF0, USB2 + Pipe Banks, BSC, CPG, DMAC0, RTC, R61524 LCD, Spansion S29GL256P CFI,
  USB Protocol 7.00, Fugue FTL CRC16/ECC, and LCD QR GF(256)/BCH(15,5).

Outputs:
  - tests/cg50_validation/certification_v47/mutation_report_v47.json
  - tests/cg50_validation/certification_v47/MUTATION_REPORT_V47.md
  - tests/cg50_validation/reports/coverage_v46/mutation_report.json (legacy pointer synced)
"""
from __future__ import annotations

import argparse
import datetime
import json
import os
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[1]
SANDBOX_ROOT = REPO / "build-mutation-sandbox"
SANDBOX_SRC = SANDBOX_ROOT / "src"
SANDBOX_BUILD = SANDBOX_ROOT / "build"
CERT_DIR = REPO / "tests" / "cg50_validation" / "certification_v47"
LEGACY_REPORT_DIR = REPO / "tests" / "cg50_validation" / "reports" / "coverage_v46"


def find_tool(names: list[str], fallbacks: list[str]) -> Path:
    for n in names:
        p = shutil.which(n)
        if p:
            return Path(p)
    for fb in fallbacks:
        if Path(fb).is_file():
            return Path(fb)
    raise FileNotFoundError(f"Tool not found: {names}")


CMAKE = find_tool(["cmake"], [r"D:\DevTools\CMake\bin\cmake.exe"])
GXX = find_tool(["g++"], [r"D:\DevTools\msys64\ucrt64\bin\g++.exe"])
MSYS_BIN = GXX.parent


@dataclass(frozen=True)
class Mutant:
    id: str
    subsystem: str
    rel_file: str
    target: str
    replacement: str
    tests: tuple[str, ...]
    rationale: str


ORACLE_X86 = REPO / "build-oracle-x86" / "cg50_dlsim_oracle_x86.exe"

# Map test target name -> extra CLI args when invoked inside sandbox
TEST_ARGS: dict[str, list[str]] = {
    "cg50_cpu_dsp_cert_lab": [
        "--quick",
        "--oracle", str(ORACLE_X86) if ORACLE_X86.is_file() else "NONE",
        "--out", str(SANDBOX_ROOT / "tmp_cpu"),
    ],
    "cg50_register_cert_lab": ["--out", str(SANDBOX_ROOT / "tmp_reg")],
    "cg50_combined_hw_stress": ["--seqs", "1600", "--threads", "2", "--out", str(SANDBOX_ROOT / "tmp_stress")],
}


MUTANTS: list[Mutant] = [
    # ---- 1. SH-4A CPU Core Opcodes & Control Flow (18 mutants) ----
    Mutant("MUT-001-CPU-DT-FLAG", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x10u: --regs_.r[n]; SetT(regs_.r[n] == 0); stats_.cycles += 1; return true;",
           "case 0x10u: --regs_.r[n]; SetT(regs_.r[n] != 0); stats_.cycles += 1; return true;",
           ("cg50_sh4a_isa_test",), "Inverts DT Rn zero-flag T semantics"),
    Mutant("MUT-002-CPU-SHLL2", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x08u: regs_.r[n] <<= 2; stats_.cycles += 1; return true;",
           "case 0x08u: regs_.r[n] <<= 1; stats_.cycles += 1; return true;",
           ("cg50_sh4a_isa_test",), "Shifts left by 1 instead of 2 on SHLL2"),
    Mutant("MUT-003-CPU-SHLR2", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x09u: regs_.r[n] >>= 2; stats_.cycles += 1; return true;",
           "case 0x09u: regs_.r[n] >>= 1; stats_.cycles += 1; return true;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Shifts right by 1 instead of 2 on SHLR2"),
    Mutant("MUT-004-CPU-SHLL8", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x18u: regs_.r[n] <<= 8; stats_.cycles += 1; return true;",
           "case 0x18u: regs_.r[n] <<= 4; stats_.cycles += 1; return true;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Shifts left by 4 instead of 8 on SHLL8"),
    Mutant("MUT-005-CPU-SHLR8", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x19u: regs_.r[n] >>= 8; stats_.cycles += 1; return true;",
           "case 0x19u: regs_.r[n] >>= 4; stats_.cycles += 1; return true;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Shifts right by 4 instead of 8 on SHLR8"),
    Mutant("MUT-006-CPU-SHLL16", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x28u: regs_.r[n] <<= 16; stats_.cycles += 1; return true;",
           "case 0x28u: regs_.r[n] <<= 15; stats_.cycles += 1; return true;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Shifts left by 15 instead of 16 on SHLL16"),
    Mutant("MUT-007-CPU-SHLR16", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x29u: regs_.r[n] >>= 16; stats_.cycles += 1; return true;",
           "case 0x29u: regs_.r[n] >>= 15; stats_.cycles += 1; return true;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Shifts right by 15 instead of 16 on SHLR16"),
    Mutant("MUT-008-CPU-CMP-PZ", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x11u: SetT(static_cast<std::int32_t>(regs_.r[n]) >= 0); stats_.cycles += 1; return true;",
           "case 0x11u: SetT(static_cast<std::int32_t>(regs_.r[n]) > 0); stats_.cycles += 1; return true;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Changes CMP/PZ (>=0) to (>0)"),
    Mutant("MUT-009-CPU-CMP-PL", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x15u: SetT(static_cast<std::int32_t>(regs_.r[n]) > 0); stats_.cycles += 1; return true;",
           "case 0x15u: SetT(static_cast<std::int32_t>(regs_.r[n]) >= 0); stats_.cycles += 1; return true;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Changes CMP/PL (>0) to (>=0)"),
    Mutant("MUT-010-CPU-DELAYSLOT-JMP", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x2Bu: if (executingDelaySlot_) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); ScheduleBranch(regs_.r[n]); stats_.cycles += 2; return true;",
           "case 0x2Bu: if (false) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); ScheduleBranch(regs_.r[n]); stats_.cycles += 2; return true;",
           ("cg50_sh4a_isa_test",), "Removes slot-illegal exception (0x1A0) on JMP inside delay slot"),
    Mutant("MUT-011-CPU-DELAYSLOT-JSR", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x0Bu: if (executingDelaySlot_) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); regs_.pr = pc + 4u; ScheduleBranch(regs_.r[n]); stats_.cycles += 2; return true;",
           "case 0x0Bu: if (false) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); regs_.pr = pc + 4u; ScheduleBranch(regs_.r[n]); stats_.cycles += 2; return true;",
           ("cg50_sh4a_isa_test", "cg50_cpu_dsp_cert_lab"), "Removes slot-illegal exception on JSR inside delay slot"),
    Mutant("MUT-012-CPU-NOFPU-STRICT", "SH7305 No-FPU Gate", "core/casio_cg50/cg50_native_core.cpp",
           "if (strictSh7305NoFpu_) {\n            // Proven by x86 disassembly of Casio CPU73050.dll",
           "if (false) {\n            // MUTANT: disable strictSh7305NoFpu_ gate",
           ("cg50_sh7305_hw_test",), "Disables SH7305 no-FPU illegal exception trap on 0xFxxx"),
    Mutant("MUT-013-CPU-CLIPU-B", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "if (regs_.r[n] > 0xFFu) { regs_.r[n] = 0xFFu; SetT(true); }",
           "if (regs_.r[n] > 0xFFu) { regs_.r[n] = 0xFEu; SetT(true); }",
           ("cg50_sh4a_isa_test", "cg50_sh7305_hw_test"), "Clamps CLIPU.B to 0xFE instead of 0xFF"),
    Mutant("MUT-014-CPU-CLIPS-B", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "else if (v < -128) { regs_.r[n] = static_cast<std::uint32_t>(-128); SetT(true); }",
           "else if (v < -128) { regs_.r[n] = static_cast<std::uint32_t>(-127); SetT(true); }",
           ("cg50_sh4a_isa_test", "cg50_sh7305_hw_test"), "Clamps CLIPS.B negative to -127 instead of -128"),
    Mutant("MUT-015-CPU-NOTT", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "if (op == 0x0068u) { SetT(!T()); stats_.cycles += 1; return true; }",
           "if (op == 0x0068u) { SetT(T()); stats_.cycles += 1; return true; }",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test", "cg50_sh7305_hw_test"), "Disables T-bit inversion in NOTT"),
    Mutant("MUT-016-CPU-CLRT", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "if (op == 0x0008u) { SetT(false); stats_.cycles += 1; return true; }",
           "if (op == 0x0008u) { SetT(true); stats_.cycles += 1; return true; }",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Sets T=1 instead of T=0 on CLRT"),
    Mutant("MUT-017-CPU-SETT", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "if (op == 0x0018u) { SetT(true); stats_.cycles += 1; return true; }",
           "if (op == 0x0018u) { SetT(false); stats_.cycles += 1; return true; }",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Sets T=0 instead of T=1 on SETT"),
    Mutant("MUT-018-CPU-DIV0U", "SH-4A CPU", "core/casio_cg50/cg50_native_core.cpp",
           "if (op == 0x0019u) { regs_.sr &= ~(kSrM | kSrQ | kSrT); stats_.cycles += 1; return true; }",
           "if (op == 0x0019u) { regs_.sr |= (kSrM | kSrQ | kSrT); stats_.cycles += 1; return true; }",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Sets instead of clears M/Q/T in DIV0U"),

    # ---- 2. SH-4A Integer ALU, Shifts, Rotates, Multiply, Divide (22 mutants) ----
    Mutant("MUT-019-ALU-ADD-OVF", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return ((~(a ^ b) & (a ^ result)) & 0x80000000u) != 0;",
           "return (((a ^ b) & (a ^ result)) & 0x80000000u) != 0;",
           ("cg50_sh4a_isa_test",), "Corrupts signed addition overflow detection (ADDV)"),
    Mutant("MUT-020-ALU-SUB-OVF", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return (((a ^ b) & (a ^ result)) & 0x80000000u) != 0;",
           "return ((~(a ^ b) & (a ^ result)) & 0x80000000u) != 0;",
           ("cg50_sh4a_isa_test",), "Corrupts signed subtraction overflow detection (SUBV)"),
    Mutant("MUT-021-ALU-ADDC-CARRY", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return {static_cast<std::uint32_t>(sum), (sum >> 32) != 0};",
           "return {static_cast<std::uint32_t>(sum), false};",
           ("cg50_sh4a_isa_test", "cg50_cpu_dsp_cert_lab"), "Drops carry-out bit in ADDC"),
    Mutant("MUT-022-ALU-SUBC-BORROW", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return {result, static_cast<std::uint64_t>(rn) < rhs};",
           "return {result, static_cast<std::uint64_t>(rn) <= rhs};",
           ("cg50_sh4a_isa_test", "cg50_cpu_dsp_cert_lab"), "Off-by-one borrow flag in SUBC"),
    Mutant("MUT-023-ALU-NEGC-BORROW", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return {static_cast<std::uint32_t>(0ull - rhs), rhs != 0};",
           "return {static_cast<std::uint32_t>(0ull - rhs), rhs == 0};",
           ("cg50_sh4a_isa_test",), "Inverts borrow flag T in NEGC"),
    Mutant("MUT-024-ALU-SHAD-SAT", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return static_cast<std::int32_t>(value) < 0 ? 0xFFFFFFFFu : 0u;",
           "return 0u;",
           ("cg50_sh4a_isa_test",), "Zeros negative SHAD -32 result instead of 0xFFFFFFFF"),
    Mutant("MUT-025-ALU-SHLD-RIGHT", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return amount >= 32u ? 0u : (value >> amount);",
           "return amount >= 32u ? 0xFFFFFFFFu : (value >> amount);",
           ("cg50_sh4a_isa_test",), "Returns 0xFFFFFFFF instead of 0 on SHLD -32"),
    Mutant("MUT-026-ALU-ROTL", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return {(value << 1) | (top ? 1u : 0u), top};",
           "return {(value << 1), top};",
           ("cg50_sh4a_isa_test",), "Drops wrap-around bit 0 in ROTL"),
    Mutant("MUT-027-ALU-ROTR", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return {(value >> 1) | (low ? 0x80000000u : 0u), low};",
           "return {(value >> 1), low};",
           ("cg50_sh4a_isa_test",), "Drops wrap-around bit 31 in ROTR"),
    Mutant("MUT-028-ALU-ROTCL", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return {(value << 1) | (oldT ? 1u : 0u), top};",
           "return {(value << 1) | (oldT ? 0u : 1u), top};",
           ("cg50_sh4a_isa_test",), "Inverts incoming T bit in ROTCL"),
    Mutant("MUT-029-ALU-ROTCR", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return {(value >> 1) | (oldT ? 0x80000000u : 0u), low};",
           "return {(value >> 1) | (oldT ? 0u : 0x80000000u), low};",
           ("cg50_sh4a_isa_test",), "Inverts incoming T bit in ROTCR"),
    Mutant("MUT-030-ALU-CMPSTR", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "(x & 0xFF000000u) == 0;",
           "false;",
           ("cg50_sh4a_isa_test",), "Ignores MSB byte match in CMP/STR"),
    Mutant("MUT-031-ALU-XTRCT", "SH-4A Integer ALU", "core/casio_cg50/cg50_core/sh_integer.h",
           "return (rn >> 16) | (rm << 16);",
           "return (rn >> 16) | (rm & 0xFFFFu);",
           ("cg50_sh4a_isa_test",), "Corrupts upper 16 bits in XTRCT"),
    Mutant("MUT-032-DIV0S-TFLAG", "SH-4A Divide", "core/casio_cg50/cg50_core/sh_divide.h",
           "state.t = state.q != state.m;",
           "state.t = state.q == state.m;",
           ("cg50_sh4a_isa_test",), "Inverts DIV0S T-bit initialization"),
    Mutant("MUT-033-DIV1-TFLAG", "SH-4A Divide", "core/casio_cg50/cg50_core/sh_divide.h",
           "state.t = q == m;",
           "state.t = q != m;",
           ("cg50_sh4a_isa_test",), "Inverts DIV1 quotient bit T"),
    Mutant("MUT-034-DIV1-SHIFTIN", "SH-4A Divide", "core/casio_cg50/cg50_core/sh_divide.h",
           "std::uint32_t value = (rn << 1) | (oldT ? 1u : 0u);",
           "std::uint32_t value = (rn << 1);",
           ("cg50_sh4a_isa_test",), "Drops oldT shift-in bit in DIV1"),
    Mutant("MUT-035-DIV-APPLY-Q", "SH-4A Divide", "core/casio_cg50/cg50_core/sh_divide.h",
           "if (state.q) sr |= kDivideSrQ;",
           "if (false && state.q) sr |= kDivideSrQ;",
           ("cg50_sh4a_isa_test",), "Never sets SR.Q after DIV0S/DIV1"),
    Mutant("MUT-036-MUL-LONG", "SH-4A Multiply", "core/casio_cg50/cg50_core/sh_multiply.h",
           "static_cast<std::uint64_t>(lhs) * static_cast<std::uint64_t>(rhs));",
           "static_cast<std::uint64_t>(lhs) + static_cast<std::uint64_t>(rhs));",
           ("cg50_sh4a_isa_test",), "Replaces MUL.L product with addition"),
    Mutant("MUT-037-MULU-W", "SH-4A Multiply", "core/casio_cg50/cg50_core/sh_multiply.h",
           "return static_cast<std::uint32_t>(lhs & 0xFFFFu) *\n           static_cast<std::uint32_t>(rhs & 0xFFFFu);",
           "return static_cast<std::uint32_t>(lhs & 0xFFu) *\n           static_cast<std::uint32_t>(rhs & 0xFFFFu);",
           ("cg50_sh4a_isa_test",), "Truncates lhs to 8 bits in MULU.W"),
    Mutant("MUT-038-MULS-W", "SH-4A Multiply", "core/casio_cg50/cg50_core/sh_multiply.h",
           "const auto a = static_cast<std::int32_t>(static_cast<std::int16_t>(lhs & 0xFFFFu));",
           "const auto a = static_cast<std::int32_t>(lhs & 0xFFFFu);",
           ("cg50_sh4a_isa_test",), "Zero-extends instead of sign-extends lhs in MULS.W"),
    Mutant("MUT-039-DMULU-L", "SH-4A Multiply", "core/casio_cg50/cg50_core/sh_multiply.h",
           "static_cast<std::uint32_t>(product >> 32),\n        static_cast<std::uint32_t>(product)",
           "0u,\n        static_cast<std::uint32_t>(product)",
           ("cg50_sh4a_isa_test",), "Zeros MACH in DMULU.L"),
    Mutant("MUT-040-DMULS-L", "SH-4A Multiply", "core/casio_cg50/cg50_core/sh_multiply.h",
           "static_cast<std::uint32_t>(bits >> 32),\n        static_cast<std::uint32_t>(bits)",
           "static_cast<std::uint32_t>(bits >> 31),\n        static_cast<std::uint32_t>(bits)",
           ("cg50_sh4a_isa_test", "cg50_cpu_dsp_cert_lab"), "Corrupts MACH shift in DMULS.L"),

    # ---- 3. SH4AL-DSP Engine (11 mutants) ----
    Mutant("MUT-041-DSP-LDS-SR-GATE", "SH4AL-DSP", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x7Au: // LDS Rm, A0 (CPU73050.dll 0x10018B68, gated on SR.DSP)\n            if ((regs_.sr & kSrDsp) == 0u) {",
           "case 0x7Au: // LDS Rm, A0 (MUTANT)\n            if (false) {",
           ("cg50_sh7305_hw_test", "cg50_hw_silicon_100_test"), "Allows LDS Rm, A0 when SR.DSP=0 instead of 0x180"),
    Mutant("MUT-042-DSP-SETRC-MASK", "SH4AL-DSP", "core/casio_cg50/cg50_native_core.cpp",
           "case 0x14u: // SETRC Rm (SH4AL-DSP)\n            regs_.sr = (regs_.sr & ~kSrRcMask) | ((regs_.r[n] & 0x0FFFu) << 16);",
           "case 0x14u: // SETRC Rm (MUTANT)\n            regs_.sr = (regs_.sr & ~kSrRcMask) | ((regs_.r[n] & 0x0001u) << 16);",
           ("cg50_sh7305_hw_test",), "Truncates SETRC repeat counter to 1 bit"),
    Mutant("MUT-043-DSP-SAT-POS", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "if (allowSat && overflow && (sr & kSrSBit) != 0u) {\n            v = (v < 0) ? -0x80000000ll : 0x7FFFFFFFll;",
           "if (false && allowSat && overflow && (sr & kSrSBit) != 0u) {\n            v = (v < 0) ? -0x80000000ll : 0x7FFFFFFFll;",
           ("cg50_sh7305_hw_test", "cg50_cpu_dsp_cert_lab"), "Disables SR.S 32-bit saturation in FinalizeArith40"),
    Mutant("MUT-044-DSP-SIGNEXT40", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "return (v << 24) >> 24;",
           "return v & 0xFFFFFFFFll;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh7305_hw_test"), "Truncates 40-bit accumulator guard bits in SignExtend40"),
    Mutant("MUT-045-DSP-DSR-GT", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "const bool gt = allowGt && (v > 0);",
           "const bool gt = false;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh7305_hw_test"), "Never sets DSR.GT flag on positive DSP result"),
    Mutant("MUT-046-DSP-DSR-ZERO", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "const bool zero = (v == 0);",
           "const bool zero = (v != 0);",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh7305_hw_test"), "Inverts DSR.Z zero flag in FinalizeArith40"),
    Mutant("MUT-047-DSP-DSR-NEG", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "const bool neg = (v < 0);",
           "const bool neg = (v > 0);",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh7305_hw_test"), "Inverts DSR.N negative flag in FinalizeArith40"),
    Mutant("MUT-048-DSP-DSR-OVF", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "const bool out32 = (v > 0x7FFFFFFFll) || (v < -0x80000000ll);",
           "const bool out32 = false;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh7305_hw_test"), "Disables 32-bit overflow detection in DSP ALU"),
    Mutant("MUT-049-DSP-PMULS-Q15", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "if ((sr & kSrSBit) == 0u) prod <<= 1; // Fractional Q15*Q15 -> Q31 (SH4AL-DSP default when SR.S=0!)",
           "if (false && (sr & kSrSBit) == 0u) prod <<= 1;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh7305_hw_test"), "Drops Q15*Q15 left-shift-by-1 fractional scaling in PMULS"),
    Mutant("MUT-050-DSP-MODULO-WRAP", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "if (step > 0 && low == me) {\n            return (addr & 0xFFFF0000u) | ms;",
           "if (false && step > 0 && low == me) {\n            return (addr & 0xFFFF0000u) | ms;",
           ("cg50_sh7305_hw_test", "cg50_cpu_dsp_cert_lab"), "Disables MOD circular buffer wraparound in MOVX/MOVY"),
    Mutant("MUT-051-DSP-ILLEGAL-DST", "SH4AL-DSP", "core/casio_cg50/cg50_core/sh_dsp.h",
           "case 0x5u: case 0x7u: case 0x8u: case 0x9u:\n        case 0xAu: case 0xBu: case 0xCu: case 0xEu:\n            return true;",
           "case 0x5u: case 0x8u: case 0x9u:\n        case 0xAu: case 0xBu: case 0xCu: case 0xEu:\n            return true;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh7305_hw_test"), "Rejects valid A0 (0x7) destination in DSP ALU"),

    # ---- 4. Hardware BCD ALU (4 mutants) ----
    Mutant("MUT-052-BCD-ADD-CARRY", "SH7305 BCD ALU", "core/casio_cg50/cg50_native_core.cpp",
           "if (s > 9u) { s -= 10u; out = true; } else { out = false; }",
           "if (s > 10u) { s -= 10u; out = true; } else { out = false; }",
           ("cg50_sh7305_hw_test", "cg50_cpu_dsp_cert_lab"), "Off-by-one decimal carry threshold in BCD ADD"),
    Mutant("MUT-053-BCD-SUB-BORROW", "SH7305 BCD ALU", "core/casio_cg50/cg50_native_core.cpp",
           "if (d < s) { d += 10u - s; out = true; }",
           "if (d < s) { d += 9u - s; out = true; }",
           ("cg50_sh7305_hw_test", "cg50_cpu_dsp_cert_lab"), "Corrupts decimal borrow base (9 instead of 10) in BCD SUB"),
    Mutant("MUT-054-BCD-LATCH-STORE", "SH7305 BCD ALU", "core/casio_cg50/cg50_native_core.cpp",
           "bcdCarryLatch_ = out ? 1u : 0u;",
           "bcdCarryLatch_ = 0u;",
           ("cg50_sh7305_hw_test", "cg50_cpu_dsp_cert_lab"), "Clears multi-word BCD carry/borrow latch"),
    Mutant("MUT-055-BCD-ALIAS-0C", "SH7305 BCD ALU", "core/casio_cg50/cg50_native_core.cpp",
           "Write32(0xA4CB000Cu, r);",
           "Write32(0xA4CB000Cu, 0u);",
           ("cg50_cpu_dsp_cert_lab",), "Breaks BCD ALU result mirror alias at 0xA4CB000C"),

    # ---- 5. MMU / TLB / Exception EXPEVT Codes (6 mutants) ----
    Mutant("MUT-056-EXPEVT-TLBMISS-RD", "MMU / Exceptions", "core/casio_cg50/cg50_core/sh_exception.h",
           "inline constexpr std::uint32_t kExpevtDataTlbMissRead = 0x040u;",
           "inline constexpr std::uint32_t kExpevtDataTlbMissRead = 0x060u;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Swaps Data TLB Miss Read EXPEVT 0x040 -> 0x060"),
    Mutant("MUT-057-EXPEVT-TLBMISS-WR", "MMU / Exceptions", "core/casio_cg50/cg50_core/sh_exception.h",
           "inline constexpr std::uint32_t kExpevtDataTlbMissWrite = 0x060u;",
           "inline constexpr std::uint32_t kExpevtDataTlbMissWrite = 0x040u;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Swaps Data TLB Miss Write EXPEVT 0x060 -> 0x040"),
    Mutant("MUT-058-EXPEVT-INITPGWR", "MMU / Exceptions", "core/casio_cg50/cg50_native_core.cpp",
           "RaiseFault(0x080u, cg50::kGeneralExceptionVectorOffset);",
           "RaiseFault(0x0C0u, cg50::kGeneralExceptionVectorOffset);",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Corrupts Initial Page Write EXPEVT 0x080 -> 0x0C0"),
    Mutant("MUT-059-EXPEVT-PROTRD", "MMU / Exceptions", "core/casio_cg50/cg50_core/sh_exception.h",
           "inline constexpr std::uint32_t kExpevtDataTlbProtectionViolation = 0x0A0u;",
           "inline constexpr std::uint32_t kExpevtDataTlbProtectionViolation = 0x040u;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Corrupts Data TLB Protection Read EXPEVT 0x0A0"),
    Mutant("MUT-060-EXPEVT-PROTWR", "MMU / Exceptions", "core/casio_cg50/cg50_core/sh_exception.h",
           "inline constexpr std::uint32_t kExpevtDataTlbProtectionWrite = 0x0C0u;",
           "inline constexpr std::uint32_t kExpevtDataTlbProtectionWrite = 0x080u;",
           ("cg50_cpu_dsp_cert_lab", "cg50_sh4a_isa_test"), "Corrupts Data TLB Protection Write EXPEVT 0x0C0"),
    Mutant("MUT-061-EXPEVT-MULTIHIT", "MMU / Exceptions", "core/casio_cg50/cg50_core/sh_exception.h",
           "inline constexpr std::uint32_t kExpevtTlbMultiHit = 0x140u;",
           "inline constexpr std::uint32_t kExpevtTlbMultiHit = 0x180u;",
           ("cg50_cpu_dsp_cert_lab",), "Corrupts TLB Multi-Hit EXPEVT 0x140"),

    # ---- 6. Cache / OCRAM / RAMCR (6 mutants) ----
    Mutant("MUT-062-CACHE-CCR-OCI", "SH-4A Cache", "core/casio_cg50/cg50_core/sh_cache.h",
           "if ((value & kCcrOci) != 0u) InvalidateOcAll();",
           "if (false && (value & kCcrOci) != 0u) InvalidateOcAll();",
           ("cg50_sh7305_hw_test",), "Disables CCR.OCI operand cache invalidation"),
    Mutant("MUT-063-CACHE-CCR-ICI", "SH-4A Cache", "core/casio_cg50/cg50_core/sh_cache.h",
           "if ((value & kCcrIci) != 0u) InvalidateIcAll();",
           "if (false && (value & kCcrIci) != 0u) InvalidateIcAll();",
           ("cg50_sh7305_hw_test",), "Disables CCR.ICI instruction cache invalidation"),
    Mutant("MUT-064-CACHE-OCRAM-GATE", "SH-4A Cache", "core/casio_cg50/cg50_core/sh_cache.h",
           "bool OcramEnabled() const noexcept { return (ccr_ & kCcrOra) != 0u; }",
           "bool OcramEnabled() const noexcept { return true; }",
           ("cg50_sh7305_hw_test",), "Exposes OCRAM even when CCR.ORA=0"),
    Mutant("MUT-065-CACHE-OCBP-WB", "SH-4A Cache", "core/casio_cg50/cg50_core/sh_cache.h",
           "if (oc_[setIdx][w].dirty) ++stats_.ocWritebacks;\n                oc_[setIdx][w].valid = false;",
           "oc_[setIdx][w].valid = false;",
           ("cg50_sh7305_hw_test",), "Drops dirty-line writeback counter on OCBP"),
    Mutant("MUT-066-CACHE-OCBWB-CLEAN", "SH-4A Cache", "core/casio_cg50/cg50_core/sh_cache.h",
           "++stats_.ocWritebacks;\n                    oc_[setIdx][w].dirty = false;",
           "oc_[setIdx][w].dirty = true;",
           ("cg50_sh7305_hw_test",), "Fails to flush dirty line in OCBWB"),
    Mutant("MUT-067-RAMCR-MASK", "Cache/OCRAM RAMCR", "core/casio_cg50/cg50_native_core.cpp",
           "if (address == 0xFF000074u) { ramcr_ = value & 0x000003E0u; return true; }",
           "if (address == 0xFF000074u) { ramcr_ = value; return true; }",
           ("cg50_hw_silicon_100_test", "cg50_sh7305_hw_test"), "Removes exact CPU73050.dll 0x3E0 mask on RAMCR"),

    # ---- 7. UBC Hardware Breakpoints (5 mutants) ----
    Mutant("MUT-068-UBC-FETCH-CH0", "UBC Breakpoints", "core/casio_cg50/cg50_core/cg50_ubc.h",
           "if ((id == 0u || (id & 1u) != 0u) && ((pc ^ car0_) & ~camr0_) == 0u) {",
           "if (false && (id == 0u || (id & 1u) != 0u) && ((pc ^ car0_) & ~camr0_) == 0u) {",
           ("cg50_sh7305_hw_test",), "Disables UBC ch0 instruction-fetch breakpoint detection"),
    Mutant("MUT-069-UBC-CCMFR-W0C", "UBC Breakpoints", "core/casio_cg50/cg50_core/cg50_ubc.h",
           "case 0xFF200600u: ccmfr_ &= val; return true; // W0C",
           "case 0xFF200600u: ccmfr_ = val; return true;",
           ("cg50_sh7305_hw_test", "cg50_register_cert_lab"), "Breaks W0C semantics on UBC CCMFR"),
    Mutant("MUT-070-UBC-CETR1-MASK", "UBC Breakpoints", "core/casio_cg50/cg50_core/cg50_ubc.h",
           "case 0xFF200038u: cetr1_ = val & 0x0FFFu; return true;",
           "case 0xFF200038u: cetr1_ = val; return true;",
           ("cg50_register_cert_lab", "cg50_sh7305_hw_test"), "Removes 12-bit mask on UBC CETR1"),
    Mutant("MUT-071-UBC-CBCR-MASK", "UBC Breakpoints", "core/casio_cg50/cg50_core/cg50_ubc.h",
           "case 0xFF200620u: cbcr_ = val & 0x0001u; return true;",
           "case 0xFF200620u: cbcr_ = val; return true;",
           ("cg50_sh7305_hw_test", "cg50_register_cert_lab"), "Removes bit-0 mask on UBC CBCR"),
    Mutant("MUT-072-UBC-DATA-CH1", "UBC Breakpoints", "core/casio_cg50/cg50_core/cg50_ubc.h",
           "const bool dataOk = !dataCheck || (((data ^ cdr1_) & ~cdmr1_) == 0u);",
           "const bool dataOk = true;",
           ("cg50_sh7305_hw_test",), "Ignores CDR1/CDMR1 data comparison on UBC ch1 watchpoint"),

    # ---- 8. INTC2 / ETMU / ADC / DD / SDHI / HUDI / PRLCK / SCIF0 (20 mutants) ----
    Mutant("MUT-073-INTC2-USERIMASK-KEY", "INTC2", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "if ((val & 0xFFFF0000u) == 0xA5000000u) {\n                userImask_ = val & 0x000000F0u;",
           "if (true) {\n                userImask_ = val & 0x000000F0u;",
           ("cg50_sh7305_hw_test", "cg50_combined_hw_stress"), "Bypasses 0xA5000000 write-key protection on USERIMASK"),
    Mutant("MUT-074-INTC2-INTREQ-W0C", "INTC2", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "if (phys == 0x04140024u) {\n            intreq00_ &= val; // W0C",
           "if (phys == 0x04140024u) {\n            intreq00_ = val;",
           ("cg50_sh7305_hw_test", "cg50_register_cert_lab"), "Breaks W0C semantics on INTC2 INTREQ00"),
    Mutant("MUT-075-INTC2-INTMSKCLR", "INTC2", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "if (phys == 0x04140064u) {\n            intmsk00_ &= static_cast<std::uint8_t>(~val);",
           "if (phys == 0x04140064u) {\n            intmsk00_ |= val;",
           ("cg50_sh7305_hw_test",), "Sets instead of clears mask bits on INTC2 INTMSKCLR00"),
    Mutant("MUT-076-INTC2-PRIO-SHIFT", "INTC2", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "return static_cast<std::uint8_t>((intpri00_ >> ((7u - irq) * 4u)) & 0xFu);",
           "return static_cast<std::uint8_t>((intpri00_ >> (irq * 4u)) & 0xFu);",
           ("cg50_sh7305_hw_test",), "Reverses IRQ0..7 nibble order in INTPRI00"),
    Mutant("MUT-077-ETMU-UNF-FLAG", "ETMU", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "ch.tcr |= 0x02u; // UNF",
           "ch.tcr |= 0x00u;",
           ("cg50_sh7305_hw_test",), "Never asserts ETMU underflow flag UNF"),
    Mutant("MUT-078-ETMU-DECREMENT", "ETMU", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "ch.tcnt -= static_cast<std::uint32_t>(ticks);",
           "ch.tcnt += static_cast<std::uint32_t>(ticks);",
           ("cg50_sh7305_hw_test",), "Counts ETMU up instead of down"),
    Mutant("MUT-079-ADC-ADST-ADF", "ADC", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "adcsr_ = static_cast<std::uint16_t>((val & ~0x0020u) | 0x0080u);",
           "adcsr_ = static_cast<std::uint16_t>(val & ~0x0020u);",
           ("cg50_sh7305_hw_test",), "Fails to set ADF conversion-complete flag on ADST"),
    Mutant("MUT-080-ADC-BAT-SCALE", "ADC", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "batteryRaw10_ = static_cast<std::uint16_t>(0x0240u + (clamped * (0x0390u - 0x0240u)) / 100u);",
           "batteryRaw10_ = 0u;",
           ("cg50_sh7305_hw_test", "cg50_combined_hw_stress"), "Zeros battery ADC channel result"),
    Mutant("MUT-081-DD-CLKR0-WRITE", "DD Bus", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "if (off == 0x00u) { ddclkr0_ = val; return true; }",
           "if (off == 0x00u) { ddclkr0_ = 0u; return true; }",
           ("cg50_sh7305_hw_test", "cg50_register_cert_lab"), "Drops writes to DD Bus DDCLKR0"),
    Mutant("MUT-082-SDHI-CARD-DETECT", "SDHI", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "const std::uint16_t cd = virtualCardPresent_ ? kInfo1CardDet : 0u;",
           "const std::uint16_t cd = kInfo1CardDet;",
           ("cg50_hw_silicon_100_test",), "Forces SD_INFO1 card-detect bit ON with no card"),
    Mutant("MUT-083-SDHI-CMD8-RSP", "SDHI", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "regs_[0x0Cu / 2u] = (cmdIdx == 8u) ? 0x01AAu : 0x0900u; // SD_RSP0",
           "regs_[0x0Cu / 2u] = 0x0000u;",
           ("cg50_hw_silicon_100_test",), "Breaks SDHI CMD8 voltage-check response 0x01AA"),
    Mutant("MUT-084-SDHI-NOCARD-TO", "SDHI", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "regs_[0x1Eu / 2u] |= 0x0040u; // RSP_TO response timeout",
           "regs_[0x1Eu / 2u] |= 0x0000u;",
           ("cg50_hw_silicon_100_test",), "Fails to set RSP_TO timeout when SD card is absent"),
    Mutant("MUT-085-HUDI-SDINT-W0C", "HUDI", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "case 0x18u: sdint_ &= static_cast<std::uint16_t>(val & 0x0001u); return true; // W0C INTREQ",
           "case 0x18u: sdint_ = val; return true;",
           ("cg50_hw_silicon_100_test", "cg50_combined_hw_stress"), "Breaks W0C semantics on HUDI SDINT"),
    Mutant("MUT-086-PRLCK-LOCK-ENFORCE", "PRLCK", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "if ((prlckcr_ & (1u << idx)) != 0u) {\n                ++blockedWrites_;",
           "if (false) {\n                ++blockedWrites_;",
           ("cg50_hw_silicon_100_test", "cg50_combined_hw_stress"), "Disables PRLCKCR lock enforcement on PRPRICR0..5"),
    Mutant("MUT-087-PRLCK-MASK3F", "PRLCK", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "prpricr_[idx] = val & kMask;",
           "prpricr_[idx] = val;",
           ("cg50_hw_silicon_100_test", "cg50_register_cert_lab"), "Removes 0x3F mask on PRPRICR0..5"),
    Mutant("MUT-088-SCIF0-LOOPBACK", "SCIF0", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "if ((scfcr_ & 0x0001u) != 0u) {\n                PushRx(val);",
           "if (false && (scfcr_ & 0x0001u) != 0u) {\n                PushRx(val);",
           ("cg50_sh7305_hw_test",), "Disables SCIF0 SCFCR.LOOP hardware loopback"),
    Mutant("MUT-089-SCIF0-OVERRUN", "SCIF0", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "sclsr_ |= 0x0001u; // ORER overrun",
           "sclsr_ |= 0x0000u;",
           ("cg50_sh7305_hw_test",), "Fails to latch ORER overrun flag on 17th RX byte"),
    Mutant("MUT-090-SCIF0-RDF-FLAG", "SCIF0", "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
           "scfsr_ |= 0x0003u; // RDF | DR",
           "scfsr_ |= 0x0000u;",
           ("cg50_sh7305_hw_test",), "Never sets SCFSR RDF|DR when RX FIFO has data"),
    Mutant("MUT-091-USB2-VBUS-ATTACH", "USB2", "core/casio_cg50/cg50_core/cg50_usb2.h",
           "regs_[0x40u / 2u] |= (kIntVbint | kIntDvst | 0x0080u); // VBSTS=1 + VBINT + DVST",
           "regs_[0x40u / 2u] |= 0x0000u;",
           ("cg50_sh7305_hw_test",), "Fails to set VBSTS/VBINT/DVST on USB VBUS attach"),
    Mutant("MUT-092-USB2-SETUP-VALID", "USB2", "core/casio_cg50/cg50_core/cg50_usb2.h",
           "regs_[0x40u / 2u] = static_cast<std::uint16_t>((regs_[0x40u / 2u] & ~0x0007u) | ctsq | kIntCtrt | kIntValid);",
           "regs_[0x40u / 2u] = static_cast<std::uint16_t>(regs_[0x40u / 2u] & ~0x0007u);",
           ("cg50_sh7305_hw_test",), "Drops VALID/CTRT interrupt bits on USB SETUP packet"),
    Mutant("MUT-093-USB2-PIPE-BANK", "USB2", "core/casio_cg50/cg50_core/cg50_usb2.h",
           "if (off == 0x68u) { out = pipeCfg_[SelectedPipe()]; return true; }",
           "if (off == 0x68u) { out = pipeCfg_[0]; return true; }",
           ("cg50_combined_hw_stress", "cg50_sh7305_hw_test"), "Ignores PIPESEL bank index on PIPECFG read"),

    # ---- 9. BSC / CPG / DMAC0 / RTC (8 mutants) ----
    Mutant("MUT-094-BSC-RTCSR-KEY", "BSC", "core/casio_cg50/cg50_native_core.cpp",
           "if ((value & 0xA55A0000u) == 0xA55A0000u) {\n                const std::uint32_t keepCmf = bscRegisters_[18] & value & 0x80u;",
           "if (true) {\n                const std::uint32_t keepCmf = bscRegisters_[18] & value & 0x80u;",
           ("cg50_hw_silicon_100_test",), "Removes 0xA55A0000 key guard on BSC RTCSR writes"),
    Mutant("MUT-095-BSC-CSWCR-MASK", "BSC", "core/casio_cg50/cg50_native_core.cpp",
           "bscRegisters_[index] = value & kCsWcrMasks[memType];",
           "bscRegisters_[index] = value;",
           ("cg50_hw_silicon_100_test",), "Removes CPU73050 type-dependent CS*WCR mask"),
    Mutant("MUT-096-BSC-REFRESH-CMF", "BSC", "core/casio_cg50/cg50_native_core.cpp",
           "bscRegisters_[18] |= 0x80u; // RTCSR.CMF",
           "bscRegisters_[18] |= 0x00u;",
           ("cg50_hw_silicon_100_test",), "Prevents BSC SDRAM refresh compare-match flag CMF"),
    Mutant("MUT-097-CPG-MSTPCR0-TMU-GATE", "CPG", "core/casio_cg50/cg50_native_core.cpp",
           "if ((moduleStopControl0_ & 0x00002000u) == 0u) {",
           "if (true) {",
           ("cg50_hw_silicon_100_test",), "Disables CPG MSTPCR0 clock gating of TMU0/1"),
    Mutant("MUT-098-DMAC0-ADDR-ERR", "DMAC0", "core/casio_cg50/cg50_native_core.cpp",
           "dmacExt_.dmaor |= 0x0004u; // Set AE",
           "dmacExt_.dmaor |= 0x0000u;",
           ("cg50_hw_silicon_100_test", "cg50_combined_hw_stress"), "Prevents DMAOR.AE assertion on misaligned DMA"),
    Mutant("MUT-099-DMAC0-CHCR-MASK", "DMAC0", "core/casio_cg50/cg50_native_core.cpp",
           "const std::uint32_t rwMask  = (channelIndex < 4u) ? 0x4FB7FFFDu : 0x4030FF3Du;",
           "const std::uint32_t rwMask  = 0xFFFFFFFFu;",
           ("cg50_hw_silicon_100_test",), "Removes CPU73050 CHCR0..5 writable bitmask"),
    Mutant("MUT-100-RTC-ALARM-FLAG", "RTC", "core/casio_cg50/cg50_native_core.cpp",
           "rtcExt_.rcr1 |= 0x01u; // AF",
           "rtcExt_.rcr1 |= 0x00u;",
           ("cg50_hw_silicon_100_test",), "Prevents RTC Alarm Flag (RCR1.AF) from latching"),
    Mutant("MUT-101-RTC-PERIODIC-FLAG", "RTC", "core/casio_cg50/cg50_native_core.cpp",
           "rtcExt_.rcr2 |= 0x80u; // PEF",
           "rtcExt_.rcr2 |= 0x00u;",
           ("cg50_hw_silicon_100_test",), "Prevents RTC Periodic Event Flag (RCR2.PEF)"),

    # ---- 10. R61524 LCD, NOR Flash CFI, Protocol 7, Fugue FTL, LCD QR (11 mutants) ----
    Mutant("MUT-102-LCD-GRAM-WRITE", "R61524 LCD", "core/casio_cg50/cg50_core/r61524.cpp",
           "gram_[static_cast<std::size_t>(curY_) * kVisibleWidth + curX_] = value;",
           "gram_[static_cast<std::size_t>(curY_) * kVisibleWidth + curX_] = static_cast<std::uint16_t>(value ^ 0xFFFFu);",
           ("cg50_sh7305_hw_test", "cg50_usb_smem_qr_test"), "Corrupts RGB565 pixel values written to R61524 GRAM"),
    Mutant("MUT-103-LCD-GRAM-DUMMYRD", "R61524 LCD", "core/casio_cg50/cg50_core/r61524.cpp",
           "if (gramReadDummyPending_) {\n            gramReadDummyPending_ = false;",
           "if (false && gramReadDummyPending_) {\n            gramReadDummyPending_ = false;",
           ("cg50_sh7305_hw_test", "cg50_combined_hw_stress"), "Skips mandatory R61524 first dummy GRAM read"),
    Mutant("MUT-104-LCD-SCROLL-Y", "R61524 LCD", "core/casio_cg50/cg50_core/r61524.cpp",
           "const std::size_t srcY = (y + scrollY) % kVisibleHeight;",
           "const std::size_t srcY = y;",
           ("cg50_sh7305_hw_test",), "Disables R61524 hardware vertical scroll rendering"),
    Mutant("MUT-105-FLASH-CFI-UNLOCK", "NOR Flash CFI", "core/casio_cg50/cg50_core/cg50_flash_cfi.h",
           "case State::ReadArray:\n            if (cmdOff == 0x0AAAu && cmd == 0xAAu) {\n                state_ = State::Unlock1;",
           "case State::ReadArray:\n            if (cmdOff == 0x0AAAu && cmd == 0xBBu) {\n                state_ = State::Unlock1;",
           ("cg50_flash_cfi_test",), "Breaks Spansion S29GL256P CFI unlock sequence"),
    Mutant("MUT-106-FLASH-CFI-QUERY", "NOR Flash CFI", "core/casio_cg50/cg50_core/cg50_flash_cfi.h",
           "if (cmdOff == 0x00AAu && cmd == 0x98u) {\n                state_ = State::CfiQuery;",
           "if (false) {\n                state_ = State::CfiQuery;",
           ("cg50_flash_cfi_test",), "Disables CFI Query (0x98@0xAA) QRY identification"),
    Mutant("MUT-107-FLASH-AUTOSELECT-ID", "NOR Flash CFI", "core/casio_cg50/cg50_core/cg50_flash_cfi.h",
           "case 0x01u: outValue = 0x227Eu; return true; // S29GL-P family",
           "case 0x01u: outValue = 0x0000u; return true;",
           ("cg50_flash_cfi_test",), "Corrupts S29GL256P Autoselect Device ID1 0x227E"),
    Mutant("MUT-108-USB-PROTO7-CHK", "USB Protocol 7.00", "core/casio_cg50/cg50_core/cg50_usb_host.cpp",
           "return static_cast<std::uint8_t>((0x100u - (sum & 0xFFu)) & 0xFFu);",
           "return static_cast<std::uint8_t>((sum & 0xFFu) ^ 0x55u);",
           ("cg50_usb_smem_qr_test",), "Corrupts Casio Protocol 7.00 packet checksum"),
    Mutant("MUT-109-USB-PROTO7-CHK-VERIFY", "USB Protocol 7.00", "core/casio_cg50/cg50_core/cg50_usb_host.cpp",
           "if (receivedChk != expectedChk) return false;",
           "if (false) return false;",
           ("cg50_usb_smem_qr_test",), "Accepts corrupted Protocol 7.00 packet checksums"),
    Mutant("MUT-110-FUGUE-CRC16", "SMEM Fugue FTL", "core/casio_cg50/cg50_core/cg50_fugue_ftl.cpp",
           "crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021u);",
           "crc = static_cast<std::uint16_t>((crc << 1) ^ 0x8005u);",
           ("cg50_usb_smem_qr_test",), "Uses wrong polynomial in Fugue FTL CRC16"),
    Mutant("MUT-111-QR-GF256-POLY", "LCD QR Detector", "core/casio_cg50/cg50_core/cg50_lcd_qr.cpp",
           "if ((x & 0x100) != 0) x ^= 0x11D;",
           "if ((x & 0x100) != 0) x ^= 0x11B;",
           ("cg50_usb_smem_qr_test",), "Uses AES polynomial 0x11B instead of QR GF(256) 0x11D"),
    Mutant("MUT-112-QR-BCH-FORMAT", "LCD QR Detector", "core/casio_cg50/cg50_core/cg50_lcd_qr.cpp",
           "return static_cast<std::uint16_t>(((data5 << 10) | d) ^ 0x5412);",
           "return static_cast<std::uint16_t>(((data5 << 10) | d) ^ 0x1234);",
           ("cg50_usb_smem_qr_test",), "Corrupts ISO-18004 QR BCH(15,5) format mask 0x5412"),
]


def make_env() -> dict[str, str]:
    env = os.environ.copy()
    env["PATH"] = f"{MSYS_BIN}{os.pathsep}{env.get('PATH', '')}"
    env["CG50_CERT_STRICT_NO_HLE"] = "1"
    return env


def run_cmd(cmd: list[str], cwd: Path, env: dict[str, str], timeout: int = 120) -> tuple[int, str, str, float, bool]:
    t0 = time.perf_counter()
    try:
        p = subprocess.run(
            cmd,
            cwd=str(cwd),
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
        )
        return p.returncode, p.stdout, p.stderr, time.perf_counter() - t0, False
    except subprocess.TimeoutExpired as ex:
        out = (ex.stdout or "") if isinstance(ex.stdout, str) else ""
        err = (ex.stderr or "") if isinstance(ex.stderr, str) else ""
        return -9, out, err, time.perf_counter() - t0, True


def prepare_sandbox(env: dict[str, str], clean: bool = False) -> None:
    if clean and SANDBOX_ROOT.exists():
        shutil.rmtree(SANDBOX_ROOT, ignore_errors=True)
    SANDBOX_SRC.mkdir(parents=True, exist_ok=True)
    SANDBOX_BUILD.mkdir(parents=True, exist_ok=True)

    dst_core = SANDBOX_SRC / "core"
    dst_tests = SANDBOX_SRC / "tests" / "core"
    dst_hw = SANDBOX_SRC / "tests" / "cg50_validation" / "hardware"
    for d in (dst_core, dst_tests, dst_hw):
        if d.exists():
            shutil.rmtree(d)
    shutil.copytree(REPO / "core", dst_core, copy_function=shutil.copy)
    shutil.copytree(REPO / "tests" / "core", dst_tests, copy_function=shutil.copy)
    shutil.copytree(REPO / "tests" / "cg50_validation" / "hardware", dst_hw, copy_function=shutil.copy)
    dst_cert = SANDBOX_SRC / "tests" / "cg50_validation" / "certification_v47"
    dst_cert.mkdir(parents=True, exist_ok=True)
    shutil.copy(
        REPO / "tests" / "cg50_validation" / "certification_v47" / "cg50_702_reg_table.h",
        dst_cert / "cg50_702_reg_table.h",
    )

    # Patch CORE_ROMS_DIR and switch -O3 -> -O0 inside sandbox ONLY for ~8x faster mutant rebuilds!
    cm_path = dst_core / "CMakeLists.txt"
    cm_txt = cm_path.read_text(encoding="utf-8")
    roms_posix = (REPO / "assets" / "roms").as_posix()
    cm_txt = cm_txt.replace(
        'set(CORE_ROMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/roms")',
        f'set(CORE_ROMS_DIR "{roms_posix}")',
    )
    cm_txt = cm_txt.replace(
        "target_compile_options(${tgt} PRIVATE -O3 -fvisibility=hidden)",
        "target_compile_options(${tgt} PRIVATE -O0 -g0 -fvisibility=hidden)",
    )
    cm_path.write_text(cm_txt, encoding="utf-8")

    cfg_cmd = [
        str(CMAKE),
        "-S", str(dst_core),
        "-B", str(SANDBOX_BUILD),
        "-G", "Ninja",
        f"-DCMAKE_CXX_COMPILER={GXX.as_posix()}",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DBUILD_CORE_TESTS=ON",
        "-DBUILD_CG50_BATTERY=OFF",
        "-DCG50_ENABLE_COVERAGE_COUNTERS=OFF",
        "-DCMAKE_CXX_FLAGS=-O0 -g0",
    ]
    print("[MUT] Configuring isolated sandbox build...")
    rc, out, err, dt, _ = run_cmd(cfg_cmd, SANDBOX_ROOT, env, timeout=180)
    if rc != 0:
        print(out[-2000:])
        print(err[-2000:], file=sys.stderr)
        raise RuntimeError(f"Sandbox CMake configure failed (rc={rc})")
    print(f"[MUT] Sandbox configured in {dt:.1f}s")

    targets = [
        "cg50_flash_cfi_test",
        "cg50_sh4a_isa_test",
        "cg50_sh7305_hw_test",
        "cg50_hw_silicon_100_test",
        "cg50_usb_smem_qr_test",
        "cg50_cpu_dsp_cert_lab",
        "cg50_register_cert_lab",
        "cg50_combined_hw_stress",
    ]
    print(f"[MUT] Building baseline sandbox ({len(targets)} test targets)...")
    rc, out, err, dt, _ = run_cmd([str(CMAKE), "--build", str(SANDBOX_BUILD), "-j", "16", "--target"] + targets, SANDBOX_ROOT, env, timeout=300)
    if rc != 0:
        print(out[-3000:])
        print(err[-3000:], file=sys.stderr)
        raise RuntimeError(f"Sandbox baseline build failed (rc={rc})")
    print(f"[MUT] Baseline built in {dt:.1f}s. Verifying baseline tests PASS...")

    for t in targets:
        cmd = [str(SANDBOX_BUILD / f"{t}.exe")] + TEST_ARGS.get(t, [])
        rc, out, err, tdt, to = run_cmd(cmd, REPO, env, timeout=60)
        if rc != 0 or to:
            print(out[-1500:])
            print(err[-1500:], file=sys.stderr)
            raise RuntimeError(f"Sandbox baseline test {t} FAILED (rc={rc}, timeout={to})")
        print(f"[MUT]   baseline {t}: PASS ({tdt:.2f}s)")


def execute_mutants(env: dict[str, str], subset: list[str] | None = None) -> dict[str, Any]:
    results: list[dict[str, Any]] = []
    selected = [m for m in MUTANTS if (subset is None or m.id in subset)]
    t_all = time.perf_counter()

    # Pre-validate uniqueness of EVERY mutant target before building any!
    for idx, mut in enumerate(selected, 1):
        fpath = SANDBOX_SRC / mut.rel_file
        orig_bytes = fpath.read_bytes()
        orig_text = orig_bytes.decode("utf-8")
        norm_text = orig_text.replace("\r\n", "\n")
        count = norm_text.count(mut.target)
        if count != 1:
            print(f"[MUT] [{idx:03d}/{len(selected)}] {mut.id}: BUILD_ERROR (target match count={count})")
            results.append({
                "id": mut.id,
                "subsystem": mut.subsystem,
                "file": mut.rel_file,
                "rationale": mut.rationale,
                "status": "BUILD_ERROR",
                "error": f"target match count={count}",
                "killing_test": None,
                "elapsed_sec": 0.0,
            })
            continue

        mut_text = norm_text.replace(mut.target, mut.replacement, 1)
        t0 = time.perf_counter()
        status = "SURVIVED"
        killing_test = None
        kill_detail = ""
        try:
            fpath.write_bytes(mut_text.encode("utf-8"))
            now = time.time() + 2.0
            os.utime(fpath, (now, now))
            bcmd = [str(CMAKE), "--build", str(SANDBOX_BUILD), "-j", "16", "--target"] + list(mut.tests)
            brc, bout, berr, _, bto = run_cmd(bcmd, SANDBOX_ROOT, env, timeout=120)
            if brc != 0 or bto:
                status = "BUILD_ERROR"
                kill_detail = (berr or bout)[-400:]
            else:
                for tname in mut.tests:
                    cmd = [str(SANDBOX_BUILD / f"{tname}.exe")] + TEST_ARGS.get(tname, [])
                    trc, tout, terr, _, tto = run_cmd(cmd, REPO, env, timeout=30)
                    if tto:
                        status = "KILLED"
                        killing_test = f"{tname} (TIMEOUT)"
                        kill_detail = "Execution timed out (30s) due to mutant"
                        break
                    if trc != 0:
                        status = "KILLED"
                        killing_test = tname
                        kill_detail = (terr.strip() or tout.strip())[-240:]
                        break
        finally:
            fpath.write_bytes(orig_bytes)
            assert fpath.read_bytes() == orig_bytes, f"Restore verification failed for {fpath}"
            now2 = time.time() + 4.0
            os.utime(fpath, (now2, now2))

        dt = time.perf_counter() - t0
        print(f"[MUT] [{idx:03d}/{len(selected)}] {mut.id} ({mut.subsystem}): {status}"
              + (f" by {killing_test}" if killing_test else "") + f" [{dt:.2f}s]", flush=True)
        results.append({
            "id": mut.id,
            "subsystem": mut.subsystem,
            "file": mut.rel_file,
            "rationale": mut.rationale,
            "target_tests": list(mut.tests),
            "status": status,
            "killing_test": killing_test,
            "detail": kill_detail,
            "elapsed_sec": round(dt, 3),
        })

    killed = sum(1 for r in results if r["status"] == "KILLED")
    survived = sum(1 for r in results if r["status"] == "SURVIVED")
    build_err = sum(1 for r in results if r["status"] == "BUILD_ERROR")
    valid_total = killed + survived
    kill_pct = round(100.0 * killed / valid_total, 2) if valid_total else 0.0

    report = {
        "schema": "cg50_mutation_report_v47",
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "sandbox_dir": str(SANDBOX_ROOT),
        "live_workspace_modified": False,
        "total_elapsed_sec": round(time.perf_counter() - t_all, 2),
        "summary": {
            "total_mutants": len(results),
            "valid_mutants": valid_total,
            "killed": killed,
            "survived": survived,
            "build_error": build_err,
            "kill_rate_pct": kill_pct,
        },
        "mutants": results,
    }
    return report


def write_outputs(report: dict[str, Any]) -> None:
    CERT_DIR.mkdir(parents=True, exist_ok=True)
    LEGACY_REPORT_DIR.mkdir(parents=True, exist_ok=True)
    out_json = CERT_DIR / "mutation_report_v47.json"
    out_md = CERT_DIR / "MUTATION_REPORT_V47.md"
    out_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    (LEGACY_REPORT_DIR / "mutation_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

    s = report["summary"]
    lines = [
        "# CG50 v47 — Reporte de Pruebas de Mutación en Sandbox Aislado (112 Mutantes No Equivalentes)",
        "",
        f"- **Generado (UTC):** `{report['generated_utc']}`",
        f"- **Sandbox:** `{report['sandbox_dir']}` (`live_workspace_modified=false`)",
        f"- **Resultado:** **`{s['killed']} / {s['valid_mutants']}` eliminados (`{s['kill_rate_pct']}%`)**, sobrevivientes: `{s['survived']}`, errores de compilación: `{s['build_error']}`",
        f"- **Tiempo total:** `{report['total_elapsed_sec']} s`",
        "",
        "| ID Mutante | Subsistema | Estado | Test que lo mató | Tiempo | Descripción |",
        "|---|---|---|---|---|---|",
    ]
    for m in report["mutants"]:
        lines.append(
            f"| `{m['id']}` | {m['subsystem']} | **{m['status']}** | `{m['killing_test'] or '-'}` | `{m['elapsed_sec']}s` | {m['rationale']} |"
        )
    lines.append("")
    out_md.write_text("\n".join(lines), encoding="utf-8")
    print(f"[MUT] Wrote {out_json} & {out_md}")
    print(f"[MUT] FINAL KILL RATE: {s['killed']}/{s['valid_mutants']} ({s['kill_rate_pct']}%) | survived={s['survived']} | build_error={s['build_error']}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--clean", action="store_true")
    ap.add_argument("--skip-prep", action="store_true")
    ap.add_argument("--only", nargs="*", default=None)
    ap.add_argument("--retest-survived", action="store_true",
                    help="Sync updated test sources into existing sandbox, rebuild baseline, re-run only SURVIVED mutants from mutation_report_v47.json, and merge results")
    args = ap.parse_args()

    env = make_env()
    if args.retest_survived:
        prev_path = CERT_DIR / "mutation_report_v47.json"
        prev = json.loads(prev_path.read_text(encoding="utf-8"))
        prev_by_id = {m["id"]: m for m in prev["mutants"]}
        surv_ids = [m["id"] for m in prev["mutants"] if m["status"] != "KILLED"]
        print(f"[MUT] --retest-survived: syncing updated tests and re-testing {len(surv_ids)} survivors...")
        prepare_sandbox(env, clean=False)
        retest_rep = execute_mutants(env, subset=surv_ids)
        for m in retest_rep["mutants"]:
            prev_by_id[m["id"]] = m
        merged = [prev_by_id[m.id] for m in MUTANTS if m.id in prev_by_id]
        killed = sum(1 for r in merged if r["status"] == "KILLED")
        survived = sum(1 for r in merged if r["status"] == "SURVIVED")
        build_err = sum(1 for r in merged if r["status"] == "BUILD_ERROR")
        valid_total = killed + survived
        kill_pct = round(100.0 * killed / valid_total, 2) if valid_total else 0.0
        report = {
            "schema": "cg50_mutation_report_v47",
            "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "sandbox_dir": str(SANDBOX_ROOT),
            "live_workspace_modified": False,
            "total_elapsed_sec": round(prev.get("total_elapsed_sec", 0.0) + retest_rep["total_elapsed_sec"], 2),
            "summary": {
                "total_mutants": len(merged),
                "valid_mutants": valid_total,
                "killed": killed,
                "survived": survived,
                "build_error": build_err,
                "kill_rate_pct": kill_pct,
            },
            "mutants": merged,
        }
        write_outputs(report)
        s = report["summary"]
        return 0 if (s["survived"] == 0 and s["build_error"] == 0) else 1

    if not args.skip_prep:
        prepare_sandbox(env, clean=args.clean)
    report = execute_mutants(env, subset=args.only)
    write_outputs(report)
    s = report["summary"]
    return 0 if (s["survived"] == 0 and s["build_error"] == 0) else 1


if __name__ == "__main__":
    raise SystemExit(main())
