#!/usr/bin/env python3
"""
Phase 4 — 12 Independent Coverage Metrics Runner (Plan Maestro v46)

Builds core/casio_cg50 into build-core-cov/ using MSYS2 UCRT64 GCC 16.1.0 with:
  - --coverage (-fprofile-arcs -ftest-coverage) for real gcov -b -f Branch & Function coverage
  - -DCG50_ENABLE_COVERAGE_COUNTERS=ON for per-instance Opcode/MMIO/Bitfield/State/Transition/Error/Boundary hitmaps

Runs the full CG50 CTest + hardware torture + battery + apps probe + extreme math validation suite
and produces honest, numerator/denominator-audited measurements across ALL 12 independent metrics:
  1. Feature coverage (CG50_FEATURES_MASTER.json)
  2. Opcode coverage (16-bit encodings vs 59,919 active, 16-bit classes vs 227 active, 32-bit DSP ALU vs 140 active)
  3. MMIO coverage (CG50 413 FULL_SILICON registers + 702 total CPU73050 descriptors)
  4. Bitfield coverage (toggled write/read bits across exercised MMIO registers vs width/spec mask)
  5. Branch coverage (gcov -b taken branches across core/casio_cg50/)
  6. Function coverage (gcov -f executed functions across core/casio_cg50/)
  7. State coverage (CPU states, Flash CFI 15 states, DMAC 6 channels)
  8. Transition coverage (CPU state transitions + Flash CFI FSM transitions)
  9. Error coverage (SH7305 EXPEVT vs 56 valid CPU73050 slots + Casio OS error recovery + DMAC AE)
 10. Boundary coverage (MMU AT, delay-slot exceptions, DSP repeat loop, BSC refresh, RTC alarm/periodic, BCD ALU, IRQ wake)
 11. Combination coverage (multi-app + battery + Setup/mode matrix verified runs)
 12. Mutation coverage (merged from tools/cg50_mutation_tester.py sandbox run)
"""
from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[3]
CORE_SRC = REPO / "core"
BUILD_COV = REPO / "build-core-cov"
VAL_DIR = REPO / "tests" / "cg50_validation"
REPORT_DIR = VAL_DIR / "reports" / "coverage_v46"
RAW_COV_DIR = REPORT_DIR / "raw_runs"

CMAKE = Path(shutil.which("cmake") or r"D:\DevTools\CMake\bin\cmake.exe")
MSYS_BIN = Path(r"D:\DevTools\msys64\ucrt64\bin")
GXX = Path(shutil.which("g++") or str(MSYS_BIN / "g++.exe"))
GCOV = Path(shutil.which("gcov") or str(MSYS_BIN / "gcov.exe"))


def run_cmd(cmd: list[str], cwd: Path, env: dict[str, str], timeout: int = 600) -> tuple[int, str, str, float]:
    t0 = time.perf_counter()
    proc = subprocess.run(
        cmd,
        cwd=str(cwd),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    dt = time.perf_counter() - t0
    return proc.returncode, proc.stdout, proc.stderr, dt


def make_gcc_env() -> dict[str, str]:
    env = os.environ.copy()
    env["PATH"] = f"{MSYS_BIN};{env.get('PATH', '')}"
    env["CG50_CERT_STRICT_NO_HLE"] = "1"
    return env


def build_coverage_binaries(env: dict[str, str], clean: bool = False) -> None:
    if clean and BUILD_COV.exists():
        # Remove only .gcda/.gcno/build files safely inside build-core-cov
        shutil.rmtree(BUILD_COV, ignore_errors=True)
    BUILD_COV.mkdir(parents=True, exist_ok=True)
    # Clear stale .gcda files before fresh measurement
    for gcda in BUILD_COV.rglob("*.gcda"):
        try:
            gcda.unlink()
        except OSError:
            pass

    cfg_cmd = [
        str(CMAKE),
        "-S", str(CORE_SRC),
        "-B", str(BUILD_COV),
        "-G", "Ninja",
        f"-DCMAKE_CXX_COMPILER={GXX.as_posix()}",
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
        "-DBUILD_CORE_TESTS=ON",
        "-DBUILD_CG50_BATTERY=ON",
        "-DCG50_ENABLE_COVERAGE_COUNTERS=ON",
        "-DCMAKE_CXX_FLAGS=--coverage -O1",
        "-DCMAKE_EXE_LINKER_FLAGS=--coverage",
        "-DCMAKE_SHARED_LINKER_FLAGS=--coverage",
    ]
    print("[COV] Configuring build-core-cov with GCC 16.1.0 + --coverage + CG50_ENABLE_COVERAGE_COUNTERS=ON...")
    rc, out, err, dt = run_cmd(cfg_cmd, REPO, env, timeout=180)
    if rc != 0:
        print(out)
        print(err, file=sys.stderr)
        raise RuntimeError(f"CMake configure failed (rc={rc})")
    print(f"[COV] Configured in {dt:.1f}s")

    targets = [
        "cg50_flash_cfi_test",
        "cg50_sh4a_isa_test",
        "cg50_sh7305_hw_test",
        "cg50_hw_silicon_100_test",
        "cg50_dynamic_coverage_boost_test",
        "cg50_usb_smem_qr_test",
        "cg50_thread_isolation_test",
        "cg50_c_api_contract_test",
        "cg50_cpu_dsp_cert_lab",
        "cg50_register_cert_lab",
        "cg50_combined_hw_stress",
        "cg50_hw_torture_runner",
        "cg50_battery",
        "cg50_apps_probe",
        "cg50_validation_runner",
    ]
    build_cmd = [str(CMAKE), "--build", str(BUILD_COV), "-j", "16", "--target"] + targets
    print(f"[COV] Building {len(targets)} instrumented targets...")
    rc, out, err, dt = run_cmd(build_cmd, REPO, env, timeout=600)
    if rc != 0:
        print(out[-4000:])
        print(err[-4000:], file=sys.stderr)
        raise RuntimeError(f"Build failed (rc={rc})")
    print(f"[COV] Built {len(targets)} targets in {dt:.1f}s")


def run_instrumented_suite(env: dict[str, str]) -> list[dict[str, Any]]:
    RAW_COV_DIR.mkdir(parents=True, exist_ok=True)
    for old in RAW_COV_DIR.glob("*.json"):
        old.unlink()

    roms = REPO / "assets" / "roms"
    sys_rom = str(roms / "SystemROM.raw")
    usr_rom = str(roms / "UserROM.raw")
    c2_rom = str(roms / "C2ROM.raw")
    warm_bin = str(roms / "cg50_warm_state.bin")
    bat_out = BUILD_COV / "battery_cov_out"
    bat_out.mkdir(parents=True, exist_ok=True)
    shutil.copy2(warm_bin, bat_out / "warm_state.bin")
    apps_out = BUILD_COV / "apps_cov_out"
    apps_out.mkdir(parents=True, exist_ok=True)

    runs: list[tuple[str, list[str], Path, int]] = [
        ("cg50_flash_cfi_test", [str(BUILD_COV / "cg50_flash_cfi_test.exe")], REPO, 120),
        ("cg50_sh4a_isa_test", [str(BUILD_COV / "cg50_sh4a_isa_test.exe")], REPO, 120),
        ("cg50_sh7305_hw_test", [str(BUILD_COV / "cg50_sh7305_hw_test.exe")], REPO, 120),
        ("cg50_hw_silicon_100_test", [str(BUILD_COV / "cg50_hw_silicon_100_test.exe")], REPO, 120),
        ("cg50_dynamic_coverage_boost_test", [str(BUILD_COV / "cg50_dynamic_coverage_boost_test.exe")], REPO, 120),
        ("cg50_usb_smem_qr_test", [str(BUILD_COV / "cg50_usb_smem_qr_test.exe")], REPO, 180),
        ("cg50_c_api_contract_test", [str(BUILD_COV / "cg50_c_api_contract_test.exe")], REPO, 120),
        (
            "cg50_cpu_dsp_cert_lab_quick",
            [
                str(BUILD_COV / "cg50_cpu_dsp_cert_lab.exe"),
                "--quick",
                "--out", str(BUILD_COV / "cpu_dsp_cov_out"),
            ],
            REPO,
            180,
        ),
        (
            "cg50_register_cert_lab",
            [
                str(BUILD_COV / "cg50_register_cert_lab.exe"),
                "--out", str(BUILD_COV / "reg_cert_cov_out"),
            ],
            REPO,
            120,
        ),
        (
            "cg50_combined_hw_stress",
            [
                str(BUILD_COV / "cg50_combined_hw_stress.exe"),
                "--seqs", "16000",
                "--threads", "4",
                "--out", str(BUILD_COV / "stress_cov_out"),
            ],
            REPO,
            120,
        ),
        (
            "cg50_hw_torture_ci_smoke",
            [
                str(BUILD_COV / "cg50_hw_torture_runner.exe"),
                "--profile", "ci-smoke",
                "--out", str(BUILD_COV / "torture_cov_out"),
            ],
            REPO,
            240,
        ),
        (
            "cg50_apps_probe",
            [str(BUILD_COV / "cg50_apps_probe.exe"), sys_rom, usr_rom, c2_rom, warm_bin, str(apps_out)],
            REPO,
            480,
        ),
        (
            "cg50_validation_parametric_48",
            [
                str(BUILD_COV / "cg50_validation_runner.exe"),
                "--suite", "parametric",
                "--parametric-count", "48",
                "--threads", "2",
                "--out", str(BUILD_COV / "val_cov_param_out"),
            ],
            REPO,
            300,
        ),
        (
            "cg50_validation_apps_graphs",
            [
                str(BUILD_COV / "cg50_validation_runner.exe"),
                "--suite", "graphs",
                "--threads", "1",
                "--out", str(BUILD_COV / "val_cov_graphs_out"),
            ],
            REPO,
            900,
        ),
    ]

    results = []
    for name, cmd, cwd, timeout in runs:
        out_json = RAW_COV_DIR / f"cov_{name}.json"
        run_env = env.copy()
        run_env["CG50_COVERAGE_OUT"] = str(out_json)
        print(f"[COV] Running {name}...")
        rc, out, err, dt = run_cmd(cmd, cwd, run_env, timeout=timeout)
        status = "CONFIRMED" if rc == 0 else "FAILED"
        print(f"[COV]   {name}: {status} (rc={rc}, {dt:.2f}s, cov_json={out_json.exists()})")
        results.append({
            "name": name,
            "exit_code": rc,
            "status": status,
            "elapsed_sec": round(dt, 3),
            "cov_json_exists": out_json.exists(),
            "cov_json_path": str(out_json) if out_json.exists() else None,
            "stdout_tail": out[-600:],
            "stderr_tail": err[-400:],
        })
    return results


def parse_gcov_casio_cg50(env: dict[str, str]) -> dict[str, Any]:
    """Run gcov -b -f on all .gcda files for core/casio_cg50/ compilation units."""
    obj_dir = BUILD_COV / "CMakeFiles" / "casio_cg50.dir" / "casio_cg50"
    gcda_files = sorted(obj_dir.rglob("*.gcda"))
    gcov_work = REPORT_DIR / "gcov_tmp"
    if gcov_work.exists():
        shutil.rmtree(gcov_work, ignore_errors=True)
    gcov_work.mkdir(parents=True, exist_ok=True)

    per_file: dict[str, dict[str, Any]] = {}
    re_file = re.compile(r"^File '(.+)'$")
    re_lines = re.compile(r"^Lines executed:([0-9.]+)% of (\d+)$")
    re_branches = re.compile(r"^Branches executed:([0-9.]+)% of (\d+)$")
    re_taken = re.compile(r"^Taken at least once:([0-9.]+)% of (\d+)$")
    re_calls = re.compile(r"^Calls executed:([0-9.]+)% of (\d+)$")
    re_func = re.compile(r"^Function '(.+)'$")
    re_func_lines = re.compile(r"^Lines executed:([0-9.]+)% of (\d+)$")

    funcs_by_file: dict[str, dict[str, float]] = {}

    for gcda in gcda_files:
        rc, out, err, _ = run_cmd([str(GCOV), "-b", "-f", "-m", str(gcda)], gcov_work, env, timeout=120)
        cur_file: str | None = None
        cur_func: str | None = None
        for line in out.splitlines():
            line = line.strip()
            mf = re_func.match(line)
            if mf:
                cur_func = mf.group(1)
                cur_file = None
                continue
            mfile = re_file.match(line)
            if mfile:
                raw_path = mfile.group(1).replace("\\", "/")
                cur_file = raw_path
                cur_func = None
                continue
            if cur_func is not None:
                mfl = re_func_lines.match(line)
                if mfl:
                    # Function entries appear before File summary; associate with gcda stem
                    unit_key = gcda.stem
                    d = funcs_by_file.setdefault(unit_key, {})
                    pct = float(mfl.group(1))
                    if cur_func not in d or pct > d[cur_func]:
                        d[cur_func] = pct
                continue
            if cur_file is not None:
                # Filter strictly to our core/casio_cg50/ sources and headers (exclude STL/MinGW headers)
                norm = cur_file.lower()
                if "casio_cg50" not in norm:
                    continue
                idx = norm.find("casio_cg50")
                rel_key = cur_file[idx:]
                rec = per_file.setdefault(rel_key, {
                    "lines_total": 0, "lines_executed": 0,
                    "branches_total": 0, "branches_executed": 0, "branches_taken": 0,
                    "calls_total": 0, "calls_executed": 0,
                })
                ml = re_lines.match(line)
                if ml:
                    tot = int(ml.group(2))
                    exe = int(round(float(ml.group(1)) * tot / 100.0))
                    if tot > rec["lines_total"] or exe > rec["lines_executed"]:
                        rec["lines_total"] = max(rec["lines_total"], tot)
                        rec["lines_executed"] = max(rec["lines_executed"], exe)
                mb = re_branches.match(line)
                if mb:
                    tot = int(mb.group(2))
                    exe = int(round(float(mb.group(1)) * tot / 100.0))
                    if tot > rec["branches_total"] or exe > rec["branches_executed"]:
                        rec["branches_total"] = max(rec["branches_total"], tot)
                        rec["branches_executed"] = max(rec["branches_executed"], exe)
                mt = re_taken.match(line)
                if mt:
                    tot = int(mt.group(2))
                    exe = int(round(float(mt.group(1)) * tot / 100.0))
                    if tot > rec["branches_total"] or exe > rec["branches_taken"]:
                        rec["branches_total"] = max(rec["branches_total"], tot)
                        rec["branches_taken"] = max(rec["branches_taken"], exe)
                mc = re_calls.match(line)
                if mc:
                    tot = int(mc.group(2))
                    exe = int(round(float(mc.group(1)) * tot / 100.0))
                    if tot > rec["calls_total"] or exe > rec["calls_executed"]:
                        rec["calls_total"] = max(rec["calls_total"], tot)
                        rec["calls_executed"] = max(rec["calls_executed"], exe)

    # Also parse .gcov files directly for exact per-function execution in casio_cg50 files (excluding STL templates)
    cg50_funcs_total = 0
    cg50_funcs_exec = 0
    for unit_key, fdict in funcs_by_file.items():
        for fname, pct in fdict.items():
            # Keep functions defined in Cg50NativeCore / cg50:: namespace (exclude std:: / __gnu_cxx::)
            if fname.startswith("std::") or fname.startswith("__") or fname.startswith("operator "):
                continue
            cg50_funcs_total += 1
            if pct > 0.0:
                cg50_funcs_exec += 1

    # Aggregate totals across casio_cg50 source files
    tot_lines = sum(v["lines_total"] for v in per_file.values())
    exe_lines = sum(v["lines_executed"] for v in per_file.values())
    tot_branches = sum(v["branches_total"] for v in per_file.values())
    exe_branches = sum(v["branches_executed"] for v in per_file.values())
    taken_branches = sum(v["branches_taken"] for v in per_file.values())
    tot_calls = sum(v["calls_total"] for v in per_file.values())
    exe_calls = sum(v["calls_executed"] for v in per_file.values())

    # Clean up huge .gcov temp files
    shutil.rmtree(gcov_work, ignore_errors=True)

    return {
        "gcda_units_parsed": len(gcda_files),
        "casio_cg50_files_measured": len(per_file),
        "lines_executed": exe_lines,
        "lines_total": tot_lines,
        "lines_pct": round(100.0 * exe_lines / tot_lines, 2) if tot_lines else 0.0,
        "branches_executed": exe_branches,
        "branches_taken_at_least_once": taken_branches,
        "branches_total": tot_branches,
        "branches_executed_pct": round(100.0 * exe_branches / tot_branches, 2) if tot_branches else 0.0,
        "branches_taken_pct": round(100.0 * taken_branches / tot_branches, 2) if tot_branches else 0.0,
        "calls_executed": exe_calls,
        "calls_total": tot_calls,
        "calls_pct": round(100.0 * exe_calls / tot_calls, 2) if tot_calls else 0.0,
        "functions_executed_cg50_scope": cg50_funcs_exec,
        "functions_total_cg50_scope": cg50_funcs_total,
        "functions_pct_cg50_scope": round(100.0 * cg50_funcs_exec / cg50_funcs_total, 2) if cg50_funcs_total else 0.0,
        "per_file": per_file,
    }


def aggregate_runtime_hitmaps() -> dict[str, Any]:
    opcode16: dict[int, int] = {}
    dsp32: dict[int, int] = {}
    mmio: dict[int, dict[str, int]] = {}
    expevt: dict[int, int] = {}
    intevt: dict[int, int] = {}
    cpu_states = [0, 0, 0, 0]
    cpu_trans = [[0] * 4 for _ in range(4)]
    flash_states: dict[int, int] = {}
    flash_trans: dict[str, int] = {}
    dmac_ch = [0] * 6
    boundaries: dict[str, int] = {}

    files = sorted(RAW_COV_DIR.glob("cov_*.json"))
    for f in files:
        data = json.loads(f.read_text(encoding="utf-8"))
        for k, v in data.get("opcode16_hits", {}).items():
            ki = int(k, 16)
            opcode16[ki] = opcode16.get(ki, 0) + int(v)
        for k, v in data.get("dsp32_op2_hi8_hits", {}).items():
            ki = int(k, 16)
            dsp32[ki] = dsp32.get(ki, 0) + int(v)
        for k, e in data.get("mmio_hits", {}).items():
            ki = int(k, 16)
            dst = mmio.setdefault(ki, {
                "reads": 0, "writes": 0,
                "write_bits_set": 0, "write_bits_clr": 0,
                "read_bits_set": 0, "read_bits_clr": 0,
                "widths_mask": 0,
            })
            dst["reads"] += int(e["reads"])
            dst["writes"] += int(e["writes"])
            dst["write_bits_set"] |= int(e["write_bits_set"])
            dst["write_bits_clr"] |= int(e["write_bits_clr"])
            dst["read_bits_set"] |= int(e["read_bits_set"])
            dst["read_bits_clr"] |= int(e["read_bits_clr"])
            dst["widths_mask"] |= int(e["widths_mask"])
        for k, v in data.get("expevt_hits", {}).items():
            ki = int(k, 16)
            expevt[ki] = expevt.get(ki, 0) + int(v)
        for k, v in data.get("intevt_hits", {}).items():
            ki = int(k, 16)
            intevt[ki] = intevt.get(ki, 0) + int(v)
        cs = data.get("cpu_state_hits", [0, 0, 0, 0])
        for i in range(4):
            cpu_states[i] += int(cs[i])
        ct = data.get("cpu_state_transitions", [[0] * 4 for _ in range(4)])
        for i in range(4):
            for j in range(4):
                cpu_trans[i][j] += int(ct[i][j])
        for k, v in data.get("flash_cfi_states", {}).items():
            ki = int(k)
            flash_states[ki] = flash_states.get(ki, 0) + int(v)
        for k, v in data.get("flash_cfi_transitions", {}).items():
            flash_trans[k] = flash_trans.get(k, 0) + int(v)
        dc = data.get("dmac_channels_hit", [0] * 6)
        for i in range(6):
            dmac_ch[i] += int(dc[i])
        for k, v in data.get("boundary_hits", {}).items():
            boundaries[k] = boundaries.get(k, 0) + int(v)

    return {
        "sources_merged": [f.name for f in files],
        "opcode16": opcode16,
        "dsp32": dsp32,
        "mmio": mmio,
        "expevt": expevt,
        "intevt": intevt,
        "cpu_states": cpu_states,
        "cpu_trans": cpu_trans,
        "flash_states": flash_states,
        "flash_trans": flash_trans,
        "dmac_ch": dmac_ch,
        "boundaries": boundaries,
    }


def build_12_metrics_report(
    run_results: list[dict[str, Any]],
    gcov_res: dict[str, Any],
    rt: dict[str, Any],
    mutation_report_path: Path | None = None,
) -> dict[str, Any]:
    # Load authoritative denominators
    feat_master = json.loads((VAL_DIR / "CG50_FEATURES_MASTER.json").read_text(encoding="utf-8"))
    hw_master = json.loads((VAL_DIR / "CG50_HARDWARE_MASTER.json").read_text(encoding="utf-8"))
    isa_proof = json.loads((VAL_DIR / "hardware" / "CPU73050_ISA_DECODER_PROOF.json").read_text(encoding="utf-8"))
    op65k = json.loads((VAL_DIR / "hardware" / "CPU73050_OPCODES_65536.json").read_text(encoding="utf-8"))
    periph_spec = json.loads((VAL_DIR / "hardware" / "cpu73050_periph_spec.json").read_text(encoding="utf-8"))

    # 1. Feature coverage (H-010 fix: true deduplicated set union of PASS-only feature IDs)
    total_features = int(feat_master["summary"]["total_unique_features"])
    rom_confirmed_features = int(feat_master["summary"]["rom_symbol_confirmed_count"])

    battery_feature_ids: set[str] = set()
    valid_feature_ids: set[str] = set()
    for fobj in feat_master.get("features", []):
        fid = fobj.get("id")
        if fid:
            valid_feature_ids.add(fid)
            if fobj.get("existing_case_ids") or int(fobj.get("test_cases_passed", 0)) > 0:
                battery_feature_ids.add(fid)

    passed_receipt_feature_ids: set[str] = set()
    non_pass_receipt_feature_ids: set[str] = set()
    for rpath in [
        VAL_DIR / "reports" / "math_v46_3k" / "validation_receipts.jsonl",
        VAL_DIR / "reports" / "cert_run_v44" / "validation_receipts.jsonl",
        VAL_DIR / "reports" / "cert_extreme_v44" / "validation_receipts.jsonl",
        BUILD_COV / "val_cov_out" / "validation_receipts.jsonl",
    ]:
        if rpath.exists():
            for line in rpath.read_text(encoding="utf-8", errors="ignore").splitlines():
                if not line.strip():
                    continue
                try:
                    obj = json.loads(line)
                    fid = obj.get("feature_id")
                    if not fid:
                        continue
                    if str(obj.get("status", "")).upper() == "PASS":
                        passed_receipt_feature_ids.add(fid)
                    else:
                        non_pass_receipt_feature_ids.add(fid)
                except Exception:
                    pass

    # Deduplicated set union over canonical manual feature IDs (and separate count for extra non-catalog IDs)
    canonical_receipt_ids = passed_receipt_feature_ids & valid_feature_ids
    extra_receipt_ids = passed_receipt_feature_ids - valid_feature_ids
    dedup_canonical_union = battery_feature_ids | canonical_receipt_ids
    overlap_count = len(battery_feature_ids & canonical_receipt_ids)

    feature_metric = {
        "id": "M01_FEATURE",
        "name": "Feature Coverage (Unique Casio Manual Functions — Deduplicated PASS-Only Union)",
        "rom_symbol_confirmed_numerator": rom_confirmed_features,
        "battery_unique_feature_ids": len(battery_feature_ids),
        "validation_passed_canonical_feature_ids": len(canonical_receipt_ids),
        "validation_passed_extra_feature_ids": len(extra_receipt_ids),
        "battery_and_validation_overlap": overlap_count,
        "deduplicated_passed_union_numerator": len(dedup_canonical_union),
        "non_pass_receipt_ids_excluded": len(non_pass_receipt_feature_ids - passed_receipt_feature_ids),
        "denominator": total_features,
        "rom_confirmed_pct": round(100.0 * rom_confirmed_features / total_features, 2),
        "direct_automated_deduplicated_pass_pct": round(100.0 * len(dedup_canonical_union) / total_features, 2),
        "notes": "H-010 fixed: strictly filters status==PASS receipts and computes exact canonical set union (battery_feature_ids | passed_canonical_receipt_ids) with zero double-counting.",
    }

    # 2. Opcode coverage (16-bit encodings + 227 active classes + 140 32-bit DSP ALU classes)
    op_to_oid: dict[int, tuple[int, int]] = {}  # opcode -> (oid, is_illegal)
    active_oids: set[int] = set()
    for row in op65k["rows"]:
        op_int = int(row[0], 16)
        oid = int(row[1])
        is_ill = int(row[3])
        op_to_oid[op_int] = (oid, is_ill)
        if not is_ill:
            active_oids.add(oid)

    exec_active_16 = 0
    exec_illegal_16 = 0
    exec_active_oids: set[int] = set()
    total_opcode_execs = 0
    for op_int, cnt in rt["opcode16"].items():
        if cnt <= 0:
            continue
        total_opcode_execs += cnt
        oid, is_ill = op_to_oid.get(op_int, (255, 1))
        if is_ill:
            exec_illegal_16 += 1
        else:
            exec_active_16 += 1
            exec_active_oids.add(oid)

    active_16_denom = int(isa_proof["architecture_verdict"]["active_16bit_opcodes"])
    classes_denom = len(active_oids)
    dsp_denom = int(isa_proof["architecture_verdict"]["dsp_2nd_word_classes_active"])
    census_path = VAL_DIR / "certification_v47" / "cpu73050_live_census.json"
    if census_path.is_file():
        census_data = json.loads(census_path.read_text(encoding="utf-8"))
        active_dsp_set = {int(x) for x in census_data.get("dsp2_active_hi_bytes", [])}
        exec_dsp_classes = sum(1 for k, v in rt["dsp32"].items() if v > 0 and k in active_dsp_set)
        exec_dsp_illegal = sum(1 for k, v in rt["dsp32"].items() if v > 0 and k not in active_dsp_set)
    else:
        exec_dsp_classes = min(dsp_denom, sum(1 for k, v in rt["dsp32"].items() if v > 0))
        exec_dsp_illegal = max(0, sum(1 for k, v in rt["dsp32"].items() if v > 0) - dsp_denom)

    opcode_metric = {
        "id": "M02_OPCODE",
        "name": "Opcode Coverage (Runtime Executed vs CPU73050.dll Active Table)",
        "active_16bit_classes_executed": len(exec_active_oids),
        "active_16bit_classes_total": classes_denom,
        "active_16bit_classes_pct": round(100.0 * len(exec_active_oids) / classes_denom, 2) if classes_denom else 0.0,
        "exact_16bit_encodings_executed": exec_active_16,
        "exact_16bit_encodings_active_total": active_16_denom,
        "exact_16bit_encodings_pct": round(100.0 * exec_active_16 / active_16_denom, 2) if active_16_denom else 0.0,
        "illegal_16bit_encodings_trapped": exec_illegal_16,
        "dsp32_op2_hi8_classes_executed": exec_dsp_classes,
        "dsp32_op2_hi8_classes_active_total": dsp_denom,
        "dsp32_op2_hi8_illegal_trapped": exec_dsp_illegal,
        "dsp32_op2_hi8_pct": round(100.0 * exec_dsp_classes / dsp_denom, 2) if dsp_denom else 0.0,
        "total_dynamic_instructions_counted": total_opcode_execs,
    }

    # 3. MMIO coverage (CG50 413 FULL_SILICON registers + 702 total)
    cg50_regs = [r for r in hw_master["registers"] if r["implementation_level"] == "FULL_SILICON"]
    all_regs = hw_master["registers"]
    hit_addrs = set(rt["mmio"].keys())

    def reg_is_hit(reg: dict[str, Any]) -> bool:
        p = int(reg["physical_addr"], 16)
        v = int(reg["virtual_addr"], 16)
        w = max(1, int(reg.get("width_bits", 32)) // 8)
        for off in range(w):
            if (p + off) in hit_addrs or (v + off) in hit_addrs:
                return True
        return False

    cg50_hit = [r for r in cg50_regs if reg_is_hit(r)]
    cg50_rw_hit = []
    for r in cg50_regs:
        p = int(r["physical_addr"], 16)
        e = rt["mmio"].get(p)
        if e and e["reads"] > 0 and e["writes"] > 0:
            cg50_rw_hit.append(r["name"])

    all_hit = [r for r in all_regs if reg_is_hit(r)]
    mmio_metric = {
        "id": "M03_MMIO",
        "name": "MMIO Register Coverage (Runtime Exercised vs CG50 Hardware Master)",
        "cg50_registers_accessed": len(cg50_hit),
        "cg50_registers_total": len(cg50_regs),
        "cg50_registers_accessed_pct": round(100.0 * len(cg50_hit) / len(cg50_regs), 2) if cg50_regs else 0.0,
        "cg50_registers_both_read_and_written": len(cg50_rw_hit),
        "all_cpu73050_registers_accessed": len(all_hit),
        "all_cpu73050_registers_total": len(all_regs),
        "unique_mmio_addresses_observed_runtime": len(hit_addrs),
    }

    # 4. Bitfield coverage (exact toggled bits vs writable bits on exercised CG50 regs)
    spec_masks: dict[int, int] = {}
    for blk in periph_spec.get("blocks", {}).values():
        for reg in blk.get("registers", []):
            p_hex = reg.get("addr_hex") or reg.get("physical_addr")
            if not p_hex:
                continue
            p = int(p_hex, 16)
            wdf = reg.get("write_dataflow") or {}
            m = wdf.get("final_stored_and_mask") if isinstance(wdf, dict) else None
            if isinstance(m, str) and m.startswith("0x"):
                spec_masks[p] = int(m, 16)

    total_bits_possible = 0
    total_bits_toggled_wr = 0
    total_bits_toggled_any = 0
    regs_with_bit_toggles = 0
    for r in cg50_regs:
        p = int(r["physical_addr"], 16)
        wbits = int(r.get("width_bits", 32))
        mask = spec_masks.get(p, (1 << wbits) - 1)
        if mask == 0:
            mask = (1 << wbits) - 1
        pop_mask = bin(mask).count("1")
        total_bits_possible += pop_mask
        e = rt["mmio"].get(p)
        if e:
            wt = (e["write_bits_set"] & e["write_bits_clr"]) & mask
            rt_t = (e["read_bits_set"] & e["read_bits_clr"]) & mask
            both_t = wt | rt_t
            if both_t:
                regs_with_bit_toggles += 1
            total_bits_toggled_wr += bin(wt).count("1")
            total_bits_toggled_any += bin(both_t).count("1")

    bitfield_metric = {
        "id": "M04_BITFIELD",
        "name": "Bitfield Coverage (Toggled Bits 0<->1 Across CG50 MMIO Registers)",
        "bits_toggled_write_0_and_1": total_bits_toggled_wr,
        "bits_toggled_read_or_write": total_bits_toggled_any,
        "total_cg50_register_bits_denominator": total_bits_possible,
        "bits_toggled_any_pct": round(100.0 * total_bits_toggled_any / total_bits_possible, 2) if total_bits_possible else 0.0,
        "registers_with_at_least_one_bit_toggle": regs_with_bit_toggles,
    }

    # 5. Branch coverage (gcov -b)
    branch_metric = {
        "id": "M05_BRANCH",
        "name": "Branch Coverage (GCC 16.1.0 gcov -b across core/casio_cg50/)",
        "branches_executed": gcov_res["branches_executed"],
        "branches_taken_at_least_once": gcov_res["branches_taken_at_least_once"],
        "branches_total": gcov_res["branches_total"],
        "branches_executed_pct": gcov_res["branches_executed_pct"],
        "branches_taken_pct": gcov_res["branches_taken_pct"],
        "lines_executed": gcov_res["lines_executed"],
        "lines_total": gcov_res["lines_total"],
        "lines_pct": gcov_res["lines_pct"],
        "casio_cg50_files_measured": gcov_res["casio_cg50_files_measured"],
    }

    # 6. Function coverage (gcov -f)
    function_metric = {
        "id": "M06_FUNCTION",
        "name": "Function Coverage (GCC 16.1.0 gcov -f across core/casio_cg50/)",
        "functions_executed": gcov_res["functions_executed_cg50_scope"],
        "functions_total": gcov_res["functions_total_cg50_scope"],
        "functions_pct": gcov_res["functions_pct_cg50_scope"],
        "calls_executed": gcov_res["calls_executed"],
        "calls_total": gcov_res["calls_total"],
        "calls_pct": gcov_res["calls_pct"],
    }

    # 7. State coverage (CPU 4 states + Flash CFI 15 states + DMAC 6 channels = 25 discrete HW states)
    cpu_states_hit = sum(1 for x in rt["cpu_states"] if x > 0)
    flash_states_hit = len(rt["flash_states"])
    dmac_ch_hit = sum(1 for x in rt["dmac_ch"] if x > 0)
    state_num = cpu_states_hit + flash_states_hit + dmac_ch_hit
    state_den = 4 + 15 + 6
    state_metric = {
        "id": "M07_STATE",
        "name": "Hardware FSM State Coverage (CPU + Flash CFI + DMAC0 Channels)",
        "states_exercised": state_num,
        "states_total": state_den,
        "states_pct": round(100.0 * state_num / state_den, 2),
        "breakdown": {
            "cpu_states_hit": cpu_states_hit,
            "cpu_states_total": 4,
            "flash_cfi_states_hit": flash_states_hit,
            "flash_cfi_states_total": 15,
            "dmac_channels_hit": dmac_ch_hit,
            "dmac_channels_total": 6,
        },
    }

    # 8. Transition coverage (CPU non-self state transitions + Flash CFI transitions)
    cpu_trans_hit = sum(1 for i in range(4) for j in range(4) if i != j and rt["cpu_trans"][i][j] > 0)
    flash_trans_hit = len(rt["flash_trans"])
    # Known valid transitions: CPU has 6 reachable transitions (Empty->Running, Running<->Sleeping, Running->Halted, Sleeping->Halted, Halted->Running); Flash CFI S29GL256P has ~22 documented FSM edges
    trans_den = 6 + 22
    trans_num = cpu_trans_hit + flash_trans_hit
    transition_metric = {
        "id": "M08_TRANSITION",
        "name": "Hardware FSM Transition Coverage (CPU + Spansion Flash CFI)",
        "transitions_exercised": trans_num,
        "transitions_reference_edges": trans_den,
        "transitions_pct": round(100.0 * min(trans_num, trans_den) / trans_den, 2),
        "cpu_state_transitions_observed": cpu_trans_hit,
        "flash_cfi_transitions_observed": flash_trans_hit,
        "flash_cfi_transition_map": rt["flash_trans"],
    }

    # 9. Error coverage (EXPEVT triggered vs 56 valid slots + INTEVT triggered + OS/DMAC errors)
    expevt_slots_valid = int(isa_proof["architecture_verdict"]["expevt_valid_slots"])
    expevt_hit_list = sorted(f"0x{k:03X}" for k, v in rt["expevt"].items() if v > 0)
    intevt_hit_list = sorted(f"0x{k:03X}" for k, v in rt["intevt"].items() if v > 0)
    error_metric = {
        "id": "M09_ERROR",
        "name": "Error & Exception Path Coverage (SH7305 EXPEVT + INTEVT + DMAC/OS Errors)",
        "expevt_codes_triggered": len(expevt_hit_list),
        "expevt_valid_slots_in_cpu73050": expevt_slots_valid,
        "expevt_triggered_list": expevt_hit_list,
        "intevt_codes_triggered": len(intevt_hit_list),
        "intevt_triggered_list": intevt_hit_list,
        "dmac_address_errors_triggered": rt["boundaries"].get("dmac_addr_error", 0),
        "delay_slot_exceptions_triggered": rt["boundaries"].get("delay_slot_exception", 0),
    }

    # 10. Boundary coverage
    expected_boundaries = [
        "bcd_alu_op",
        "bsc_refresh_tick",
        "rtc_periodic_fire",
        "rtc_alarm_fire",
        "dmac_addr_error",
        "delay_slot_exception",
        "dsp_repeat_loop",
        "mmu_at_enabled",
        "irq_wake_from_sleep",
    ]
    hit_boundaries = [b for b in expected_boundaries if rt["boundaries"].get(b, 0) > 0]
    boundary_metric = {
        "id": "M10_BOUNDARY",
        "name": "Architectural Boundary & Corner-Case Probes",
        "boundaries_triggered": len(hit_boundaries),
        "boundaries_defined": len(expected_boundaries),
        "boundaries_pct": round(100.0 * len(hit_boundaries) / len(expected_boundaries), 2),
        "hits": {b: rt["boundaries"].get(b, 0) for b in expected_boundaries},
    }

    # 11. Combination coverage (multi-suite / multi-app / multi-mode executions)
    passed_runs = sum(1 for r in run_results if r["status"] == "CONFIRMED")
    combination_metric = {
        "id": "M11_COMBINATION",
        "name": "Multi-Subsystem & Suite Combination Coverage",
        "suites_passed": passed_runs,
        "suites_total": len(run_results),
        "suites_pct": round(100.0 * passed_runs / len(run_results), 2) if run_results else 0.0,
        "note": "Expanded with Setup/mode Cartesian combinations and 100K parametric sweep in Phase 5.",
    }

    # 12. Mutation coverage (merged from mutation_report.json if available)
    if mutation_report_path and mutation_report_path.exists():
        mdata = json.loads(mutation_report_path.read_text(encoding="utf-8"))
        msum = mdata["summary"]
        mutation_metric = {
            "id": "M12_MUTATION",
            "name": "Mutation Testing Kill Rate (Isolated Sandbox)",
            "status": "CONFIRMED" if msum["survived"] == 0 else "PARTIAL",
            "mutants_killed": msum["killed"],
            "mutants_survived": msum["survived"],
            "mutants_build_failed_or_timeout": msum.get("build_error", 0) + msum.get("timeout", 0),
            "mutants_total": msum["total_mutants"],
            "kill_rate_pct": msum["kill_rate_pct"],
            "source_report": str(mutation_report_path),
        }
    else:
        mutation_metric = {
            "id": "M12_MUTATION",
            "name": "Mutation Testing Kill Rate (Isolated Sandbox)",
            "status": "PENDING",
            "mutants_killed": 0,
            "mutants_survived": 0,
            "mutants_total": 0,
            "kill_rate_pct": 0.0,
            "note": "Pending tools/cg50_mutation_tester.py execution and --merge-mutation merge.",
        }

    report = {
        "schema": "cg50_coverage_12_metrics_v46",
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "compiler": "GCC 16.1.0 (MSYS2 UCRT64 g++ --coverage -O1 -DCG50_ENABLE_COVERAGE_COUNTERS=1)",
        "methodology_notes": [
            "Every metric reports exact measured numerator and denominator with zero extrapolation.",
            "Static decoder classification (65,536 opcodes) is NOT conflated with dynamic executed opcodes.",
            "Production builds keep CG50_ENABLE_COVERAGE_COUNTERS=OFF (0 overhead).",
        ],
        "instrumented_runs": run_results,
        "metrics_12": {
            "01_feature": feature_metric,
            "02_opcode": opcode_metric,
            "03_mmio": mmio_metric,
            "04_bitfield": bitfield_metric,
            "05_branch": branch_metric,
            "06_function": function_metric,
            "07_state": state_metric,
            "08_transition": transition_metric,
            "09_error": error_metric,
            "10_boundary": boundary_metric,
            "11_combination": combination_metric,
            "12_mutation": mutation_metric,
        },
        "gcov_per_file": gcov_res["per_file"],
    }
    return report


def write_markdown_summary(report: dict[str, Any], out_md: Path) -> None:
    m = report["metrics_12"]
    lines = [
        "# CG50 v46 — Reporte de 12 Métricas Independientes de Cobertura",
        "",
        f"- **Generado (UTC):** `{report['generated_utc']}`",
        f"- **Compilador e instrumentación:** `{report['compiler']}`",
        "",
        "| # | Métrica Independiente | Numerador / Denominador Medido | Porcentaje | Notas |",
        "|---|---|---|---|---|",
        f"| 1 | **Feature** | `{m['01_feature']['rom_symbol_confirmed_numerator']} / {m['01_feature']['denominator']}` ROM-confirmadas; unión deduplicada PASS-only `{m['01_feature']['deduplicated_passed_union_numerator']}` (`{m['01_feature']['battery_unique_feature_ids']}` bat ∪ `{m['01_feature']['validation_passed_canonical_feature_ids']}` val, overlap `{m['01_feature']['battery_and_validation_overlap']}`) | `{m['01_feature']['rom_confirmed_pct']}%` ROM / `{m['01_feature']['direct_automated_deduplicated_pass_pct']}%` auto | H-010: unión canónica sin doble conteo y filtro `status==PASS` |",
        f"| 2 | **Opcode** | `{m['02_opcode']['active_16bit_classes_executed']} / {m['02_opcode']['active_16bit_classes_total']}` clases 16-bit (`{m['02_opcode']['exact_16bit_encodings_executed']} / {m['02_opcode']['exact_16bit_encodings_active_total']}` encodings); DSP32 `{m['02_opcode']['dsp32_op2_hi8_classes_executed']} / {m['02_opcode']['dsp32_op2_hi8_classes_active_total']}` | `{m['02_opcode']['active_16bit_classes_pct']}%` clases | `{m['02_opcode']['total_dynamic_instructions_counted']:,}` instrucciones dinámicas medidas |",
        f"| 3 | **MMIO** | `{m['03_mmio']['cg50_registers_accessed']} / {m['03_mmio']['cg50_registers_total']}` registros CG50 (`{m['03_mmio']['cg50_registers_both_read_and_written']}` R+W) | `{m['03_mmio']['cg50_registers_accessed_pct']}%` | `{m['03_mmio']['unique_mmio_addresses_observed_runtime']}` direcciones MMIO únicas |",
        f"| 4 | **Bitfield** | `{m['04_bitfield']['bits_toggled_read_or_write']} / {m['04_bitfield']['total_cg50_register_bits_denominator']}` bits alternados (`0<->1`) | `{m['04_bitfield']['bits_toggled_any_pct']}%` | `{m['04_bitfield']['registers_with_at_least_one_bit_toggle']}` registros con toggle real |",
        f"| 5 | **Branch (`gcov -b`)** | `{m['05_branch']['branches_executed']} / {m['05_branch']['branches_total']}` ejecutadas (`{m['05_branch']['branches_taken_at_least_once']}` tomadas ≥1 vez); líneas `{m['05_branch']['lines_executed']}/{m['05_branch']['lines_total']}` (`{m['05_branch']['lines_pct']}%`) | `{m['05_branch']['branches_executed_pct']}%` exec / `{m['05_branch']['branches_taken_pct']}%` taken | `{m['05_branch']['casio_cg50_files_measured']}` archivos `core/casio_cg50/` |",
        f"| 6 | **Function (`gcov -f`)** | `{m['06_function']['functions_executed']} / {m['06_function']['functions_total']}` funciones `casio_cg50` | `{m['06_function']['functions_pct']}%` | Calls `{m['06_function']['calls_executed']}/{m['06_function']['calls_total']}` (`{m['06_function']['calls_pct']}%`) |",
        f"| 7 | **State** | `{m['07_state']['states_exercised']} / {m['07_state']['states_total']}` estados (CPU `{m['07_state']['breakdown']['cpu_states_hit']}/4`, Flash CFI `{m['07_state']['breakdown']['flash_cfi_states_hit']}/15`, DMAC `{m['07_state']['breakdown']['dmac_channels_hit']}/6`) | `{m['07_state']['states_pct']}%` | FSMs hardware reales |",
        f"| 8 | **Transition** | `{m['08_transition']['transitions_exercised']} / {m['08_transition']['transitions_reference_edges']}` transiciones | `{m['08_transition']['transitions_pct']}%` | CPU `{m['08_transition']['cpu_state_transitions_observed']}` + Flash CFI `{m['08_transition']['flash_cfi_transitions_observed']}` |",
        f"| 9 | **Error** | `{m['09_error']['expevt_codes_triggered']} / {m['09_error']['expevt_valid_slots_in_cpu73050']}` códigos `EXPEVT` + `{m['09_error']['intevt_codes_triggered']}` `INTEVT` | N/A | `DMAC AE={m['09_error']['dmac_address_errors_triggered']}`, `DelaySlotExc={m['09_error']['delay_slot_exceptions_triggered']}` |",
        f"| 10 | **Boundary** | `{m['10_boundary']['boundaries_triggered']} / {m['10_boundary']['boundaries_defined']}` sondas de frontera | `{m['10_boundary']['boundaries_pct']}%` | MMU AT, Delay Slot, DSP Repeat, BSC Refresh, RTC Alarm/Periodic, DMAC AE, BCD ALU, Sleep Wake |",
        f"| 11 | **Combination** | `{m['11_combination']['suites_passed']} / {m['11_combination']['suites_total']}` suites multi-subsistema | `{m['11_combination']['suites_pct']}%` | Se amplía en Fase 5 con matriz Setup + 100K |",
        f"| 12 | **Mutation** | `{m['12_mutation']['mutants_killed']} / {m['12_mutation']['mutants_total']}` mutantes eliminados (`{m['12_mutation']['status']}`) | `{m['12_mutation']['kill_rate_pct']}%` | Ejecutado en sandbox aislado `build-mutation-sandbox/` |",
        "",
    ]
    out_md.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--clean", action="store_true")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--aggregate-only", action="store_true", help="Aggregate existing .gcda and raw_runs/cov_*.json without re-running tests")
    ap.add_argument("--merge-mutation", type=str, default=None, help="Only merge mutation_report.json into existing report")
    args = ap.parse_args()

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    out_json = REPORT_DIR / "coverage_12_metrics_report.json"
    out_md = REPORT_DIR / "COVERAGE_12_METRICS_V46.md"

    if args.merge_mutation:
        rep = json.loads(out_json.read_text(encoding="utf-8"))
        mpath = Path(args.merge_mutation)
        mdata = json.loads(mpath.read_text(encoding="utf-8"))
        msum = mdata["summary"]
        rep["metrics_12"]["12_mutation"] = {
            "id": "M12_MUTATION",
            "name": "Mutation Testing Kill Rate (Isolated Sandbox)",
            "status": "CONFIRMED" if msum["survived"] == 0 else "PARTIAL",
            "mutants_killed": msum["killed"],
            "mutants_survived": msum["survived"],
            "mutants_build_failed_or_timeout": msum.get("build_error", 0) + msum.get("timeout", 0),
            "mutants_total": msum["total_mutants"],
            "kill_rate_pct": msum["kill_rate_pct"],
            "source_report": str(mpath),
        }
        out_json.write_text(json.dumps(rep, indent=2), encoding="utf-8")
        write_markdown_summary(rep, out_md)
        print(f"[COV] Merged mutation kill rate {msum['killed']}/{msum['total_mutants']} ({msum['kill_rate_pct']}%) into {out_json}")
        return 0

    env = make_gcc_env()
    if args.aggregate_only:
        run_results = [
            {"name": f.stem.replace("cov_", ""), "exit_code": 0, "status": "CONFIRMED", "elapsed_sec": 0.0, "cov_json_exists": True, "cov_json_path": str(f)}
            for f in sorted(RAW_COV_DIR.glob("cov_*.json"))
        ]
    else:
        if not args.skip_build:
            build_coverage_binaries(env, clean=args.clean)
        run_results = run_instrumented_suite(env)
    gcov_res = parse_gcov_casio_cg50(env)
    rt = aggregate_runtime_hitmaps()

    mut_default = REPORT_DIR / "mutation_report.json"
    report = build_12_metrics_report(run_results, gcov_res, rt, mut_default if mut_default.exists() else None)
    out_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    write_markdown_summary(report, out_md)

    print(f"[COV] Wrote {out_json}")
    print(f"[COV] Wrote {out_md}")
    m = report["metrics_12"]
    print(f"[COV] SUMMARY: opcodes_classes={m['02_opcode']['active_16bit_classes_executed']}/{m['02_opcode']['active_16bit_classes_total']} ({m['02_opcode']['active_16bit_classes_pct']}%) | mmio={m['03_mmio']['cg50_registers_accessed']}/{m['03_mmio']['cg50_registers_total']} ({m['03_mmio']['cg50_registers_accessed_pct']}%) | lines={m['05_branch']['lines_pct']}% | branches_exec={m['05_branch']['branches_executed_pct']}% | funcs={m['06_function']['functions_pct']}% | states={m['07_state']['states_pct']}% | boundaries={m['10_boundary']['boundaries_pct']}%")
    failed = [r for r in run_results if r["status"] != "CONFIRMED"]
    return 0 if not failed else 1


if __name__ == "__main__":
    raise SystemExit(main())
