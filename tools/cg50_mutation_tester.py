#!/usr/bin/env python3
"""
Phase 4 — Isolated Sandbox Mutation Tester (Plan Maestro v46)

Copies core/ and tests/ into build-mutation-sandbox/ (NEVER mutates the live workspace!),
builds baseline with Ninja + GCC 16.1.0, verifies 100% baseline PASS, then injects 30
controlled architectural mutants across SH-4A CPU, SH4AL-DSP, BCD ALU, Cache/RAMCR, UBC,
INTC2, HUDI, PRLCK, BSC, CPG, DMAC0, RTC, SDHI, R61524 LCD, NOR Flash CFI, USB Protocol 7,
Fugue FTL CRC16, and LCD QR BCH(15,5).

Records exact KILLED / SURVIVED / BUILD_ERROR counts and outputs:
  - tests/cg50_validation/reports/coverage_v46/mutation_report.json
  - tests/cg50_validation/reports/coverage_v46/MUTATION_REPORT_V46.md
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

REPO = Path(r"D:\Proyectos\CalculadorasUnificadas")
SANDBOX_ROOT = REPO / "build-mutation-sandbox"
SANDBOX_SRC = SANDBOX_ROOT / "src"
SANDBOX_BUILD = SANDBOX_ROOT / "build"
REPORT_DIR = REPO / "tests" / "cg50_validation" / "reports" / "coverage_v46"

CMAKE = Path(r"D:\DevTools\CMake\bin\cmake.exe")
MSYS_BIN = Path(r"D:\DevTools\msys64\ucrt64\bin")
GXX = MSYS_BIN / "g++.exe"


@dataclass(frozen=True)
class Mutant:
    id: str
    subsystem: str
    rel_file: str  # relative to SANDBOX_SRC (e.g. core/casio_cg50/...)
    target: str
    replacement: str
    tests: tuple[str, ...]
    rationale: str


MUTANTS: list[Mutant] = [
    Mutant(
        "MUT-01-CPU-DT-FLAG",
        "SH-4A CPU",
        "core/casio_cg50/cg50_native_core.cpp",
        "case 0x10u: --regs_.r[n]; SetT(regs_.r[n] == 0); stats_.cycles += 1; return true;",
        "case 0x10u: --regs_.r[n]; SetT(regs_.r[n] != 0); stats_.cycles += 1; return true;",
        ("cg50_sh4a_isa_test",),
        "Inverts DT Rn zero-flag T semantics",
    ),
    Mutant(
        "MUT-02-CPU-SHLL2",
        "SH-4A CPU",
        "core/casio_cg50/cg50_native_core.cpp",
        "case 0x08u: regs_.r[n] <<= 2; stats_.cycles += 1; return true;",
        "case 0x08u: regs_.r[n] <<= 1; stats_.cycles += 1; return true;",
        ("cg50_sh4a_isa_test",),
        "Shifts left by 1 instead of 2 on SHLL2",
    ),
    Mutant(
        "MUT-03-CPU-DELAYSLOT-JMP",
        "SH-4A CPU",
        "core/casio_cg50/cg50_native_core.cpp",
        "case 0x2Bu: if (executingDelaySlot_) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); ScheduleBranch(regs_.r[n]); stats_.cycles += 2; return true;",
        "case 0x2Bu: if (false) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); ScheduleBranch(regs_.r[n]); stats_.cycles += 2; return true;",
        ("cg50_sh4a_isa_test",),
        "Removes slot-illegal exception (0x1A0) on JMP inside delay slot",
    ),
    Mutant(
        "MUT-04-CPU-NOFPU-STRICT",
        "SH7305 No-FPU Gate",
        "core/casio_cg50/cg50_native_core.cpp",
        "if (strictSh7305NoFpu_) {\n            // Proven by x86 disassembly of Casio CPU73050.dll",
        "if (false) {\n            // MUTANT: disable strictSh7305NoFpu_ gate",
        ("cg50_sh7305_hw_test",),
        "Disables SH7305 no-FPU illegal exception trap on 0xFxxx",
    ),
    Mutant(
        "MUT-05-DSP-LDS-SR-GATE",
        "SH4AL-DSP",
        "core/casio_cg50/cg50_native_core.cpp",
        "case 0x7Au: // LDS Rm, A0 (CPU73050.dll 0x10018B68, gated on SR.DSP)\n            if ((regs_.sr & kSrDsp) == 0u) {",
        "case 0x7Au: // LDS Rm, A0 (MUTANT)\n            if (false) {",
        ("cg50_sh7305_hw_test", "cg50_hw_silicon_100_test"),
        "Allows LDS Rm, A0 when SR.DSP=0 instead of raising 0x180",
    ),
    Mutant(
        "MUT-06-DSP-SATURATE32",
        "SH4AL-DSP",
        "core/casio_cg50/cg50_core/sh_dsp.h",
        "if (v > 0x7FFFFFFFll) return 0x7FFFFFFFll;",
        "if (false && v > 0x7FFFFFFFll) return 0x7FFFFFFFll;",
        ("cg50_sh7305_hw_test",),
        "Disables positive 32-bit DSP saturation",
    ),
    Mutant(
        "MUT-07-DSP-SETRC-MASK",
        "SH4AL-DSP",
        "core/casio_cg50/cg50_native_core.cpp",
        "case 0x14u: // SETRC Rm (SH4AL-DSP)\n            regs_.sr = (regs_.sr & ~kSrRcMask) | ((regs_.r[n] & 0x0FFFu) << 16);",
        "case 0x14u: // SETRC Rm (MUTANT)\n            regs_.sr = (regs_.sr & ~kSrRcMask) | ((regs_.r[n] & 0x0001u) << 16);",
        ("cg50_sh7305_hw_test",),
        "Truncates SETRC repeat counter to 1 bit",
    ),
    Mutant(
        "MUT-08-BCD-ADD-CARRY",
        "SH7305 BCD ALU",
        "core/casio_cg50/cg50_native_core.cpp",
        "if (s > 9u) { s -= 10u; out = true; } else { out = false; }",
        "if (s > 10u) { s -= 10u; out = true; } else { out = false; }",
        ("cg50_sh7305_hw_test",),
        "Off-by-one decimal carry threshold in hardware BCD ALU",
    ),
    Mutant(
        "MUT-09-CACHE-CCR-OCI",
        "SH-4A Cache",
        "core/casio_cg50/cg50_core/sh_cache.h",
        "if ((value & kCcrOci) != 0u) InvalidateOcAll();",
        "if (false && (value & kCcrOci) != 0u) InvalidateOcAll();",
        ("cg50_sh7305_hw_test",),
        "Disables CCR.OCI operand cache invalidation",
    ),
    Mutant(
        "MUT-10-RAMCR-MASK",
        "Cache/OCRAM RAMCR",
        "core/casio_cg50/cg50_native_core.cpp",
        "if (address == 0xFF000074u) { ramcr_ = value & 0x000003E0u; return true; }",
        "if (address == 0xFF000074u) { ramcr_ = value; return true; }",
        ("cg50_hw_silicon_100_test", "cg50_sh7305_hw_test"),
        "Removes exact CPU73050.dll 0x3E0 mask on RAMCR",
    ),
    Mutant(
        "MUT-11-UBC-FETCH-MATCH",
        "UBC Breakpoints",
        "core/casio_cg50/cg50_core/cg50_ubc.h",
        "if ((id == 0u || (id & 1u) != 0u) && ((pc ^ car0_) & ~camr0_) == 0u) {",
        "if (false && (id == 0u || (id & 1u) != 0u) && ((pc ^ car0_) & ~camr0_) == 0u) {",
        ("cg50_sh7305_hw_test",),
        "Disables UBC Channel 0 instruction-fetch breakpoint detection",
    ),
    Mutant(
        "MUT-12-INTC2-USERIMASK-KEY",
        "INTC2",
        "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
        "if ((val & 0xFFFF0000u) == 0xA5000000u) {\n                userImask_ = val & 0x000000F0u;",
        "if (true) {\n                userImask_ = val & 0x000000F0u;",
        ("cg50_sh7305_hw_test",),
        "Bypasses 0xA5000000 write-key protection on USERIMASK",
    ),
    Mutant(
        "MUT-13-INTC2-MASK-FILTER",
        "INTC2",
        "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
        "const std::uint8_t active = static_cast<std::uint8_t>(intreq00_ & ~intmsk00_);",
        "const std::uint8_t active = static_cast<std::uint8_t>(intreq00_);",
        ("cg50_sh7305_hw_test",),
        "Ignores INTMSK00 masking on external IRQ delivery",
    ),
    Mutant(
        "MUT-14-HUDI-SDINT-W0C",
        "HUDI",
        "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
        "case 0x18u: sdint_ &= static_cast<std::uint16_t>(val & 0x0001u); return true; // W0C INTREQ",
        "case 0x18u: sdint_ = val; return true; // MUTANT non-W0C",
        ("cg50_hw_silicon_100_test",),
        "Breaks W0C semantics on HUDI SDINT",
    ),
    Mutant(
        "MUT-15-PRLCK-LOCK-ENFORCE",
        "PRLCK",
        "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
        "if ((prlckcr_ & (1u << idx)) != 0u) {\n                ++blockedWrites_;",
        "if (false) {\n                ++blockedWrites_;",
        ("cg50_hw_silicon_100_test",),
        "Disables PRLCKCR lock enforcement on PRPRICR0..5",
    ),
    Mutant(
        "MUT-16-BSC-RTCSR-KEY",
        "BSC",
        "core/casio_cg50/cg50_native_core.cpp",
        "if ((value & 0xA55A0000u) == 0xA55A0000u) {\n                const std::uint32_t keepCmf = bscRegisters_[18] & value & 0x80u;",
        "if (true) {\n                const std::uint32_t keepCmf = bscRegisters_[18] & value & 0x80u;",
        ("cg50_hw_silicon_100_test",),
        "Removes 0xA55A0000 key guard on BSC RTCSR writes",
    ),
    Mutant(
        "MUT-17-BSC-CSWCR-MASK",
        "BSC",
        "core/casio_cg50/cg50_native_core.cpp",
        "bscRegisters_[index] = value & kCsWcrMasks[memType];",
        "bscRegisters_[index] = value;",
        ("cg50_hw_silicon_100_test",),
        "Removes CPU73050 type-dependent CS*WCR mask",
    ),
    Mutant(
        "MUT-18-BSC-REFRESH-CMF",
        "BSC",
        "core/casio_cg50/cg50_native_core.cpp",
        "bscRegisters_[18] |= 0x80u; // RTCSR.CMF",
        "bscRegisters_[18] |= 0x00u; // MUTANT no CMF",
        ("cg50_hw_silicon_100_test",),
        "Prevents BSC SDRAM refresh compare-match flag CMF from asserting",
    ),
    Mutant(
        "MUT-19-CPG-MSTPCR0-TMU-GATE",
        "CPG",
        "core/casio_cg50/cg50_native_core.cpp",
        "if ((moduleStopControl0_ & 0x00002000u) == 0u) {",
        "if (true) {",
        ("cg50_hw_silicon_100_test",),
        "Disables CPG MSTPCR0 clock gating of TMU0/1",
    ),
    Mutant(
        "MUT-20-DMAC0-ADDR-ERR",
        "DMAC0",
        "core/casio_cg50/cg50_native_core.cpp",
        "dmacExt_.dmaor |= 0x0004u; // Set AE",
        "dmacExt_.dmaor |= 0x0000u; // MUTANT no AE",
        ("cg50_hw_silicon_100_test",),
        "Prevents DMAOR.AE assertion on misaligned DMA transfer",
    ),
    Mutant(
        "MUT-21-DMAC0-CHCR-MASK",
        "DMAC0",
        "core/casio_cg50/cg50_native_core.cpp",
        "const std::uint32_t rwMask  = (channelIndex < 4u) ? 0x4FB7FFFDu : 0x4030FF3Du;",
        "const std::uint32_t rwMask  = 0xFFFFFFFFu;",
        ("cg50_hw_silicon_100_test",),
        "Removes CPU73050 CHCR0..5 writable bitmask enforcement",
    ),
    Mutant(
        "MUT-22-RTC-ALARM-FLAG",
        "RTC",
        "core/casio_cg50/cg50_native_core.cpp",
        "rtcExt_.rcr1 |= 0x01u; // AF",
        "rtcExt_.rcr1 |= 0x00u; // MUTANT no AF",
        ("cg50_hw_silicon_100_test",),
        "Prevents RTC Alarm Flag (RCR1.AF) from latching on alarm match",
    ),
    Mutant(
        "MUT-23-RTC-PERIODIC-FLAG",
        "RTC",
        "core/casio_cg50/cg50_native_core.cpp",
        "rtcExt_.rcr2 |= 0x80u; // PEF",
        "rtcExt_.rcr2 |= 0x00u; // MUTANT no PEF",
        ("cg50_hw_silicon_100_test",),
        "Prevents RTC Periodic Event Flag (RCR2.PEF) from asserting",
    ),
    Mutant(
        "MUT-24-SDHI-CARD-DETECT",
        "SDHI",
        "core/casio_cg50/cg50_core/cg50_sh7305_periph.h",
        "const std::uint16_t cd = virtualCardPresent_ ? kInfo1CardDet : 0u;",
        "const std::uint16_t cd = kInfo1CardDet;",
        ("cg50_hw_silicon_100_test",),
        "Forces SD_INFO1 card-detect bit ON even with no card inserted",
    ),
    Mutant(
        "MUT-25-LCD-GRAM-WRITE",
        "R61524 LCD",
        "core/casio_cg50/cg50_core/r61524.cpp",
        "gram_[static_cast<std::size_t>(curY_) * kVisibleWidth + curX_] = value;",
        "gram_[static_cast<std::size_t>(curY_) * kVisibleWidth + curX_] = static_cast<std::uint16_t>(value ^ 0xFFFFu);",
        ("cg50_sh7305_hw_test", "cg50_usb_smem_qr_test"),
        "Corrupts RGB565 pixel values written to R61524 GRAM 0x0202",
    ),
    Mutant(
        "MUT-26-FLASH-CFI-UNLOCK",
        "NOR Flash CFI",
        "core/casio_cg50/cg50_core/cg50_flash_cfi.h",
        "case State::ReadArray:\n            if (cmdOff == 0x0AAAu && cmd == 0xAAu) {\n                state_ = State::Unlock1;",
        "case State::ReadArray:\n            if (cmdOff == 0x0AAAu && cmd == 0xBBu) {\n                state_ = State::Unlock1;",
        ("cg50_flash_cfi_test",),
        "Breaks Spansion S29GL256P CFI unlock command sequence",
    ),
    Mutant(
        "MUT-27-FLASH-CFI-QUERY",
        "NOR Flash CFI",
        "core/casio_cg50/cg50_core/cg50_flash_cfi.h",
        "if (cmdOff == 0x00AAu && cmd == 0x98u) {\n                state_ = State::CfiQuery;",
        "if (false) {\n                state_ = State::CfiQuery;",
        ("cg50_flash_cfi_test",),
        "Disables CFI Query (0x98@0xAA) QRY identification",
    ),
    Mutant(
        "MUT-28-USB-PROTO7-CHK",
        "USB Protocol 7.00",
        "core/casio_cg50/cg50_core/cg50_usb_host.cpp",
        "return static_cast<std::uint8_t>((0x100u - (sum & 0xFFu)) & 0xFFu);",
        "return static_cast<std::uint8_t>((sum & 0xFFu) ^ 0x55u);",
        ("cg50_usb_smem_qr_test",),
        "Corrupts Casio Protocol 7.00 two's-complement packet checksum",
    ),
    Mutant(
        "MUT-29-FUGUE-CRC16",
        "SMEM Fugue FTL",
        "core/casio_cg50/cg50_core/cg50_fugue_ftl.cpp",
        "crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021u);",
        "crc = static_cast<std::uint16_t>((crc << 1) ^ 0x8005u);",
        ("cg50_usb_smem_qr_test",),
        "Uses wrong polynomial (0x8005 instead of CCITT 0x1021) in Fugue FTL CRC16",
    ),
    Mutant(
        "MUT-30-QR-BCH-FORMAT",
        "LCD QR Detector",
        "core/casio_cg50/cg50_core/cg50_lcd_qr.cpp",
        "return static_cast<std::uint16_t>(((data5 << 10) | d) ^ 0x5412);",
        "return static_cast<std::uint16_t>(((data5 << 10) | d) ^ 0x1234);",
        ("cg50_usb_smem_qr_test",),
        "Corrupts ISO-18004 QR BCH(15,5) format mask 0x5412",
    ),
]


def make_env() -> dict[str, str]:
    env = os.environ.copy()
    env["PATH"] = f"{MSYS_BIN};{env.get('PATH', '')}"
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

    # Sync core/ and tests/core/ into sandbox (never touch live repo!)
    dst_core = SANDBOX_SRC / "core"
    dst_tests = SANDBOX_SRC / "tests" / "core"
    if dst_core.exists():
        shutil.rmtree(dst_core)
    if dst_tests.exists():
        shutil.rmtree(dst_tests)
    shutil.copytree(REPO / "core", dst_core, copy_function=shutil.copy)
    shutil.copytree(REPO / "tests" / "core", dst_tests, copy_function=shutil.copy)

    # Patch CORE_ROMS_DIR in sandbox CMakeLists.txt so tests find real ROM assets read-only
    cm_path = dst_core / "CMakeLists.txt"
    cm_txt = cm_path.read_text(encoding="utf-8")
    roms_posix = (REPO / "assets" / "roms").as_posix()
    cm_txt = cm_txt.replace(
        'set(CORE_ROMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/roms")',
        f'set(CORE_ROMS_DIR "{roms_posix}")',
    )
    cm_path.write_text(cm_txt, encoding="utf-8")

    # Configure Ninja + GCC 16.1.0 with -O1 for fast incremental rebuilds (~2s per mutant!)
    cfg_cmd = [
        str(CMAKE),
        "-S", str(dst_core),
        "-B", str(SANDBOX_BUILD),
        "-G", "Ninja",
        f"-DCMAKE_CXX_COMPILER={GXX.as_posix()}",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_CORE_TESTS=ON",
        "-DBUILD_CG50_BATTERY=OFF",
        "-DCG50_ENABLE_COVERAGE_COUNTERS=OFF",
        "-DCMAKE_CXX_FLAGS=-O1",
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
        "cg50_thread_isolation_test",
    ]
    print(f"[MUT] Building baseline sandbox ({len(targets)} test targets)...")
    rc, out, err, dt, _ = run_cmd([str(CMAKE), "--build", str(SANDBOX_BUILD), "-j", "16", "--target"] + targets, SANDBOX_ROOT, env, timeout=300)
    if rc != 0:
        print(out[-3000:])
        print(err[-3000:], file=sys.stderr)
        raise RuntimeError(f"Sandbox baseline build failed (rc={rc})")
    print(f"[MUT] Baseline built in {dt:.1f}s. Verifying baseline tests PASS...")

    for t in targets:
        rc, out, err, tdt, to = run_cmd([str(SANDBOX_BUILD / f"{t}.exe")], REPO, env, timeout=60)
        if rc != 0 or to:
            print(out[-1500:])
            print(err[-1500:], file=sys.stderr)
            raise RuntimeError(f"Sandbox baseline test {t} FAILED (rc={rc}, timeout={to})")
        print(f"[MUT]   baseline {t}: PASS ({tdt:.2f}s)")


def execute_mutants(env: dict[str, str], subset: list[str] | None = None) -> dict[str, Any]:
    results: list[dict[str, Any]] = []
    selected = [m for m in MUTANTS if (subset is None or m.id in subset)]
    t_all = time.perf_counter()

    for idx, mut in enumerate(selected, 1):
        fpath = SANDBOX_SRC / mut.rel_file
        orig_bytes = fpath.read_bytes()
        orig_text = orig_bytes.decode("utf-8")
        # Normalize CRLF matching cleanly
        norm_text = orig_text.replace("\r\n", "\n")
        count = norm_text.count(mut.target)
        if count != 1:
            print(f"[MUT] [{idx:02d}/{len(selected)}] {mut.id}: BUILD_ERROR (target match count={count})")
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
            # Incremental build ONLY needed test targets
            bcmd = [str(CMAKE), "--build", str(SANDBOX_BUILD), "-j", "16", "--target"] + list(mut.tests)
            brc, bout, berr, _, bto = run_cmd(bcmd, SANDBOX_ROOT, env, timeout=120)
            if brc != 0 or bto:
                status = "BUILD_ERROR"
                kill_detail = (berr or bout)[-400:]
            else:
                for tname in mut.tests:
                    trc, tout, terr, _, tto = run_cmd([str(SANDBOX_BUILD / f"{tname}.exe")], REPO, env, timeout=30)
                    if tto:
                        status = "KILLED"
                        killing_test = f"{tname} (TIMEOUT)"
                        kill_detail = "Execution timed out (30s) due to mutant"
                        break
                    if trc != 0:
                        status = "KILLED"
                        killing_test = tname
                        kill_detail = (terr.strip() or tout.strip())[-300:]
                        break
        finally:
            # Always restore exact original sandbox file bytes AND advance mtime so Ninja never reuses mutated .obj!
            fpath.write_bytes(orig_bytes)
            assert fpath.read_bytes() == orig_bytes, f"Restore verification failed for {fpath}"
            now2 = time.time() + 4.0
            os.utime(fpath, (now2, now2))

        dt = time.perf_counter() - t0
        print(f"[MUT] [{idx:02d}/{len(selected)}] {mut.id} ({mut.subsystem}): {status}"
              + (f" by {killing_test}" if killing_test else "") + f" [{dt:.2f}s]")
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

    # Rebuild restored baseline in sandbox to leave sandbox clean
    killed = sum(1 for r in results if r["status"] == "KILLED")
    survived = sum(1 for r in results if r["status"] == "SURVIVED")
    build_err = sum(1 for r in results if r["status"] == "BUILD_ERROR")
    valid_total = killed + survived
    kill_pct = round(100.0 * killed / valid_total, 2) if valid_total else 0.0

    report = {
        "schema": "cg50_mutation_report_v46",
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
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    out_json = REPORT_DIR / "mutation_report.json"
    out_md = REPORT_DIR / "MUTATION_REPORT_V46.md"
    out_json.write_text(json.dumps(report, indent=2), encoding="utf-8")

    s = report["summary"]
    lines = [
        "# CG50 v46 — Reporte de Pruebas de Mutación en Sandbox Aislado",
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
    args = ap.parse_args()

    env = make_env()
    if not args.skip_prep:
        prepare_sandbox(env, clean=args.clean)
    report = execute_mutants(env, subset=args.only)
    write_outputs(report)
    s = report["summary"]
    return 0 if (s["survived"] == 0 and s["build_error"] == 0) else 1


if __name__ == "__main__":
    raise SystemExit(main())
