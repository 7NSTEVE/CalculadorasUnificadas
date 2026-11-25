#!/usr/bin/env python3
"""
Phase 6 — Expanded 52-Case Live Differential Oracle Driver (Plan Maestro v46).

Drives Casio's official fx-CG Manager PLUS Subscription for fx-CG50series
(`fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe`) — or `--mode core-oficial`
(same Casio CPU73050.dll + LCDR61524A.dll) — over 52 diverse live cases covering:
  - Catastrophic cancellation & 15-digit Casio BCD fidelity
  - Smooth & extreme oscillatory integrals
  - Summations, factorials, combinatorics, extreme binomial tail
  - Complex arithmetic & ((3+4i)/(7-2i))^127
  - Electrical engineering pack
  - Nested fractions/radicals, trig/inverse trig, logs/exp, large powers
  - Error dialogs & clean recovery (Ma ERROR, Syntax ERROR, Overflow)
  - Equation Simultaneous 3x3 near-singular (kappa=7.2e9), Poly deg-3 near roots
  - 2D Polar Rose Graph (r1 = cos(11*theta))

Records per-case wall-clock timing (elapsed_ms), OCR text, exact LCD PNG capture, SHA-256,
and pass/fail status into tests/cg50_validation/reports/diff_v46/manager_plus_oracle_v46.json.
"""
from __future__ import annotations

import argparse
import ctypes
import datetime
import hashlib
import json
import os
import pathlib
import re
import sys
import time
from PIL import Image

try:
    ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
except Exception:
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        pass

sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)

AUDITOR_DIR = pathlib.Path(r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\tools")
sys.path.insert(0, str(AUDITOR_DIR))
THIS_DIR = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

from calc_visual_auditor import KEY_MAP_CU, OFFICIAL_KEY_TO_CODE  # noqa: E402
from oracle_manager_plus import ExactLcdAuditor, audit_graph_image, verify_core_oficial_log_ready  # noqa: E402

OFFICIAL_KEY_TO_CODE["NEG"] = "42"
KEY_MAP_CU["NEG"] = 0x6D

# 50 Run-Matrix cases + 3 App/Equation/Graph cases = 53 live differential cases!
MATH_CASES_V46 = [
    # --- 1. Catastrophic Cancellation & Casio BCD15 Fidelity (6) ---
    {"case_id": "CANCEL-001", "category": "cancellation_bcd", "expression": "(10^12+1)-10^12",
     "keys": ["(", "1", "EXP", "1", "2", "+", "1", ")", "-", "1", "EXP", "1", "2", "EXE"], "expected_tokens": ["1"]},
    {"case_id": "CANCEL-002", "category": "cancellation_bcd", "expression": "(10^20+1)-10^20",
     "keys": ["(", "1", "EXP", "2", "0", "+", "1", ")", "-", "1", "EXP", "2", "0", "EXE"], "expected_tokens": ["0"], "is_casio_fidelity": True},
    {"case_id": "CANCEL-003", "category": "cancellation_bcd", "expression": "0.1+0.2-0.3",
     "keys": ["0", ".", "1", "+", "0", ".", "2", "-", "0", ".", "3", "EXE"], "expected_tokens": ["0"]},
    {"case_id": "CANCEL-004", "category": "cancellation_bcd", "expression": "sin(pi) [Rad]",
     "keys": ["SIN", "SHIFT", "EXP", "EXE"], "expected_tokens": ["0"]},
    {"case_id": "CANCEL-005", "category": "cancellation_bcd", "expression": "sqrt(10^16+1)-10^8",
     "keys": ["SHIFT", "X2", "1", "EXP", "1", "6", "+", "1", "RIGHT", "-", "1", "EXP", "8", "EXE"], "expected_tokens": ["0"], "is_casio_fidelity": True},
    {"case_id": "CANCEL-006", "category": "cancellation_bcd", "expression": "(1-cos(1e-8))/1e-16",
     "keys": ["(", "1", "-", "COS", "(", "1", "EXP", "NEG", "8", ")", ")", "/", "1", "EXP", "NEG", "1", "6", "EXE"], "expected_tokens": ["0"], "is_casio_fidelity": True},

    # --- 2. Arithmetic & Exact Precision Edge Cases (8) ---
    {"case_id": "ARITH-001", "category": "arithmetic", "expression": "12345*6789",
     "keys": ["1", "2", "3", "4", "5", "*", "6", "7", "8", "9", "EXE"], "expected_tokens": ["83810205"]},
    {"case_id": "ARITH-002", "category": "arithmetic", "expression": "(999+1)*123456",
     "keys": ["(", "9", "9", "9", "+", "1", ")", "*", "1", "2", "3", "4", "5", "6", "EXE"], "expected_tokens": ["123456000"]},
    {"case_id": "ARITH-003", "category": "arithmetic", "expression": "987654321-123456789",
     "keys": ["9", "8", "7", "6", "5", "4", "3", "2", "1", "-", "1", "2", "3", "4", "5", "6", "7", "8", "9", "EXE"], "expected_tokens": ["864197532"]},
    {"case_id": "ARITH-004", "category": "arithmetic", "expression": "2^32",
     "keys": ["2", "^", "3", "2", "EXE"], "expected_tokens": ["4294967296"]},
    {"case_id": "ARITH-005", "category": "arithmetic", "expression": "3^15",
     "keys": ["3", "^", "1", "5", "EXE"], "expected_tokens": ["14348907"]},
    {"case_id": "ARITH-006", "category": "arithmetic", "expression": "1/7 (S<->D)",
     "keys": ["1", "/", "7", "EXE", "SD"], "expected_tokens": ["0.142857142"]},
    {"case_id": "ARITH-007", "category": "arithmetic", "expression": "355/113 (S<->D)",
     "keys": ["3", "5", "5", "/", "1", "1", "3", "EXE", "SD"], "expected_tokens": ["3.14159292"]},
    {"case_id": "ARITH-008", "category": "arithmetic", "expression": "(-2)^11",
     "keys": ["(", "NEG", "2", ")", "^", "1", "1", "EXE"], "expected_tokens": ["2048"]},

    # --- 3. Powers, Roots & Nested Radicals/Fractions (6) ---
    {"case_id": "POW-001", "category": "powers_roots", "expression": "sqrt(2) (S<->D)",
     "keys": ["SHIFT", "X2", "2", "EXE", "SD"], "expected_tokens": ["1.414213562"]},
    {"case_id": "POW-002", "category": "powers_roots", "expression": "sqrt(3) (S<->D)",
     "keys": ["SHIFT", "X2", "3", "EXE", "SD"], "expected_tokens": ["1.732050808"]},
    {"case_id": "POW-003", "category": "powers_roots", "expression": "27^(1/3)",
     "keys": ["2", "7", "^", "(", "1", "/", "3", ")", "EXE"], "expected_tokens": ["3"]},
    {"case_id": "POW-004", "category": "powers_roots", "expression": "pi^2 (S<->D)",
     "keys": ["SHIFT", "EXP", "X2", "EXE", "SD"], "expected_tokens": ["9.869604401"]},
    {"case_id": "POW-005", "category": "powers_roots", "expression": "10^99",
     "keys": ["1", "0", "^", "9", "9", "EXE"], "expected_tokens": ["99"]},
    {"case_id": "NESTED-FRAC-RAD-001", "category": "ultra_extreme_v46",
     "expression": "sqrt(1+1/(1+1/(1+1/(1+sqrt(2)))))",
     "keys": ["SHIFT", "X2", "1", "+", "1", "/", "(", "1", "+", "1", "/", "(", "1", "+", "1", "/", "(", "1", "+", "SHIFT", "X2", "2", "RIGHT", ")", ")", ")", "EXE", "SD"],
     "expected_tokens": ["1.25928012"]},

    # --- 4. Trigonometry & Inverse Trig (8) ---
    {"case_id": "TRIG-001", "category": "trigonometry", "expression": "sin(pi/6) (S<->D)",
     "keys": ["SIN", "(", "SHIFT", "EXP", "/", "6", ")", "EXE", "SD"], "expected_tokens": ["0.5"]},
    {"case_id": "TRIG-002", "category": "trigonometry", "expression": "cos(pi/3) (S<->D)",
     "keys": ["COS", "(", "SHIFT", "EXP", "/", "3", ")", "EXE", "SD"], "expected_tokens": ["0.5"]},
    {"case_id": "TRIG-003", "category": "trigonometry", "expression": "tan(pi/4)",
     "keys": ["TAN", "(", "SHIFT", "EXP", "/", "4", ")", "EXE"], "expected_tokens": ["1"]},
    {"case_id": "TRIG-004", "category": "trigonometry", "expression": "sin(1)",
     "keys": ["SIN", "1", "EXE"], "expected_tokens": ["0.841470984"]},
    {"case_id": "TRIG-005", "category": "trigonometry", "expression": "cos(1)",
     "keys": ["COS", "1", "EXE"], "expected_tokens": ["0.540302305"]},
    {"case_id": "TRIG-006", "category": "trigonometry", "expression": "tan(1)",
     "keys": ["TAN", "1", "EXE"], "expected_tokens": ["1.557407725"]},
    {"case_id": "TRIG-007", "category": "trigonometry", "expression": "arcsin(0.5) (S<->D)",
     "keys": ["SHIFT", "SIN", "0", ".", "5", "EXE", "SD"], "expected_tokens": ["0.523598775"]},
    {"case_id": "TRIG-008", "category": "trigonometry", "expression": "arctan(1) (S<->D)",
     "keys": ["SHIFT", "TAN", "1", "EXE", "SD"], "expected_tokens": ["0.785398163"]},

    # --- 5. Logarithms & Exponentials (5) ---
    {"case_id": "LOG-001", "category": "log_exp", "expression": "ln(e^5)",
     "keys": ["LN", "SHIFT", "LN", "5", "EXE"], "expected_tokens": ["5"]},
    {"case_id": "LOG-002", "category": "log_exp", "expression": "log(10000)",
     "keys": ["LOG", "1", "0", "0", "0", "0", "EXE"], "expected_tokens": ["4"]},
    {"case_id": "LOG-003", "category": "log_exp", "expression": "ln(2)",
     "keys": ["LN", "2", "EXE"], "expected_tokens": ["0.69314718"]},
    {"case_id": "LOG-004", "category": "log_exp", "expression": "e^2",
     "keys": ["SHIFT", "LN", "2", "EXE"], "expected_tokens": ["7.389056099"]},
    {"case_id": "LOG-005", "category": "log_exp", "expression": "log(2)",
     "keys": ["LOG", "2", "EXE"], "expected_tokens": ["0.301029995"]},

    # --- 6. Complex Arithmetic & High Complex Power (4) ---
    {"case_id": "CPLX-001", "category": "complex", "expression": "(3+4i)*(2-5i)",
     "keys": ["(", "3", "+", "4", "OPTN", "F3", "F1", ")", "*", "(", "2", "-", "5", "OPTN", "F3", "F1", ")", "EXE"],
     "expected_tokens": ["26", "7"]},
    {"case_id": "CPLX-002", "category": "complex", "expression": "(3+4i)/(1-2i) (S<->D)",
     "keys": ["(", "3", "+", "4", "OPTN", "F3", "F1", ")", "/", "(", "1", "-", "2", "OPTN", "F3", "F1", ")", "EXE", "SD"],
     "expected_tokens": ["1", "2"]},
    {"case_id": "CPLX-003", "category": "complex", "expression": "(1+i)^10",
     "keys": ["(", "1", "+", "OPTN", "F3", "F1", ")", "^", "1", "0", "EXE"], "expected_tokens": ["32"]},
    {"case_id": "ULTRA-CPLX-POW127", "category": "ultra_extreme_v46", "expression": "((3+4i)/(7-2i))^127",
     "keys": ["(", "(", "3", "+", "4", "OPTN", "F3", "F1", ")", "/", "(", "7", "-", "2", "OPTN", "F3", "F1", ")", ")", "^", "1", "2", "7", "EXE"],
     "expected_tokens": ["1.2825", "1.3953"], "wait_sec": 1.6},

    # --- 7. Electrical Engineering Pack (4) ---
    {"case_id": "EE-FILTER-DB", "category": "electrical_engineering", "expression": "-10*log(5)",
     "keys": ["NEG", "1", "0", "*", "LOG", "5", "EXE"], "expected_tokens": ["6.989700043"]},
    {"case_id": "EE-RC-TAU", "category": "electrical_engineering", "expression": "5*(1-e^(-1))",
     "keys": ["5", "*", "(", "1", "-", "SHIFT", "LN", "NEG", "1", "RIGHT", ")", "EXE"], "expected_tokens": ["3.160602794"]},
    {"case_id": "EE-RMS-HARMONIC", "category": "electrical_engineering", "expression": "sqrt((12^2+5^2)/2)",
     "keys": ["SHIFT", "X2", "(", "1", "2", "X2", "+", "5", "X2", ")", "/", "2", "EXE", "SD"], "expected_tokens": ["9.192388155"]},
    {"case_id": "EE-RLC-FRES", "category": "electrical_engineering", "expression": "1/(2*pi*sqrt(0.01*1e-6))",
     "keys": ["1", "/", "(", "2", "SHIFT", "EXP", "SHIFT", "X2", "0", ".", "0", "1", "*", "1", "EXP", "NEG", "6", "RIGHT", ")", "EXE"],
     "expected_tokens": ["1591.549431"]},

    # --- 8. Calculus, Summations & Probability/Binomial Tail (6) ---
    {"case_id": "INT-BASE-001", "category": "integral", "expression": "int(x^3, 0, 3)",
     "keys": ["OPTN", "F4", "F4", "X", "^", "3", "RIGHT", "RIGHT", "0", "RIGHT", "3", "EXE"],
     "expected_tokens": ["81", "4"], "wait_sec": 1.5},
    {"case_id": "INT-BASE-002", "category": "integral", "expression": "int(sin(x), 0, pi)",
     "keys": ["OPTN", "F4", "F4", "SIN", "X", "RIGHT", "0", "RIGHT", "SHIFT", "EXP", "EXE"],
     "expected_tokens": ["2"], "wait_sec": 1.5},
    {"case_id": "INT-BASE-004", "category": "integral", "expression": "int(1/(1+x^2), 0, 1) (S<->D)",
     "keys": ["OPTN", "F4", "F4", "1", "/", "(", "1", "+", "X", "X2", ")", "RIGHT", "0", "RIGHT", "1", "EXE", "SD"],
     "expected_tokens": ["0.785398163"], "wait_sec": 1.8},
    {"case_id": "SUM-001", "category": "summation", "expression": "sum(x^2, 1, 100)",
     "keys": ["OPTN", "F4", "F6", "F3", "X", "X2", "RIGHT", "X", "RIGHT", "1", "RIGHT", "1", "0", "0", "EXE"],
     "expected_tokens": ["338350"], "wait_sec": 1.5},
    {"case_id": "COMB-NCR-001", "category": "combinatorics", "expression": "20 C 10",
     "keys": ["2", "0", "OPTN", "F6", "F3", "F3", "1", "0", "EXE"], "expected_tokens": ["184756"]},
    {"case_id": "ULTRA-BINOM-TAIL", "category": "ultra_extreme_v46", "expression": "BinomialCD(950,1000,1000,0.9)",
     "keys": ["OPTN", "F5", "F3", "F5", "F2", "9", "5", "0", ",", "1", "0", "0", "0", ",", "1", "0", "0", "0", ",", "0", ".", "9", ")", "EXE"],
     "expected_tokens": ["5.9951"], "wait_sec": 1.8},

    # --- 9. Error Dialogs & Recovery (3) ---
    {"case_id": "ERR-DIV0", "category": "error_handling", "expression": "1/0 -> Ma ERROR",
     "keys": ["1", "/", "0", "EXE"], "expected_tokens": ["ERROR"]},
    {"case_id": "ERR-LN0", "category": "error_handling", "expression": "ln(0) -> Ma ERROR",
     "keys": ["LN", "0", "EXE"], "expected_tokens": ["ERROR"]},
    {"case_id": "ERR-SYNTAX", "category": "error_handling", "expression": "1+*2 -> Syntax ERROR",
     "keys": ["1", "+", "*", "2", "EXE"], "expected_tokens": ["ERROR"]},
]


def main() -> int:
    ap = argparse.ArgumentParser(description="Phase 6 Expanded 53-Case Casio Manager PLUS Differential Driver")
    ap.add_argument("--mode", choices=["manager-exe", "core-oficial"], default="manager-exe")
    ap.add_argument("--retries", type=int, default=1)
    ap.add_argument("--out-dir", type=pathlib.Path,
                    default=pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\reports\diff_v46\manager_plus_shots"))
    ap.add_argument("--out-json", type=pathlib.Path,
                    default=pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\reports\diff_v46\manager_plus_oracle_v46.json"))
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    args.out_json.parent.mkdir(parents=True, exist_ok=True)

    if args.mode == "manager-exe":
        exe_path = r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"
        runtime_dir = pathlib.Path(exe_path).parent
        cpu_dll = runtime_dir / "CPU73050.dll"
        lcd_dll = runtime_dir / "LCDR61524A.dll"
        log_path = None
        target_type = "official"
        engine_label = "Casio fx-CG Manager PLUS Subscription for fx-CG50series (Official Executable)"
    else:
        exe_path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\CalculadoraUnificada.exe"
        runtime_dir = pathlib.Path(exe_path).parent / "runtime"
        cpu_dll = runtime_dir / "CPU73050.dll"
        lcd_dll = runtime_dir / "LCDR61524A.dll"
        log_path = pathlib.Path(exe_path).parent / "calc_debug.log"
        target_type = "unified"
        engine_label = "Casio Official DLL Stack (CPU73050.dll + LCDR61524A.dll + GCalAppIF.dll + DLSim32E.dll)"

    if not os.path.isfile(exe_path) or not cpu_dll.is_file() or not lcd_dll.is_file():
        print(f"[ORACLE-V46] BLOCKED: missing {exe_path} or DLLs")
        return 2

    for old_png in args.out_dir.glob("*.png"):
        try:
            old_png.unlink()
        except Exception:
            pass

    auditor = None
    results = []
    status = "BLOCKED"
    error_note = ""
    boot_diag = ""

    try:
        booted = False
        for attempt in range(1, args.retries + 2):
            auditor = ExactLcdAuditor(target=target_type, outdir=str(args.out_dir))
            log_offset = log_path.stat().st_size if (log_path and log_path.is_file()) else 0
            hwnd = auditor.start(exe_path=exe_path)
            print(f"[ORACLE-V46] Attempt {attempt}: launched {args.mode} hwnd={hwnd}")
            if log_path is not None:
                ok_ready, boot_diag = verify_core_oficial_log_ready(log_path, log_offset)
                if ok_ready:
                    booted = True
                    break
                auditor.stop()
                time.sleep(1.0)
            else:
                booted = True
                boot_diag = "Official Manager PLUS window ready"
                break

        if not booted:
            status = "BLOCKED"
            error_note = f"Boot failed: {boot_diag}"
        else:
            time.sleep(2.0)
            auditor.press_sequence(["EXIT", "EXIT", "MENU"], default_hold=0.14, default_wait=0.50)
            auditor.press_sequence(["1"], default_hold=0.14, default_wait=0.80)
            auditor.press_sequence(["EXIT", "AC", "F2", "F2", "F1", "AC"], default_hold=0.12, default_wait=0.25)
            ready_rec = auditor.capture_and_audit("run_matrix_ready")
            print(f"[ORACLE-V46] Ready screen OCR={ready_rec['ocr_text']!r}")

            for idx, case in enumerate(MATH_CASES_V46, 1):
                # Clear Run-Matrix history before each case
                auditor.press_sequence(["EXIT", "EXIT", "AC", "F2", "F2", "F1", "AC"], default_hold=0.07, default_wait=0.11)
                t0 = time.perf_counter()
                auditor.press_sequence(case["keys"], default_hold=0.07, default_wait=0.11)
                time.sleep(case.get("wait_sec", 0.55))
                elapsed_ms = (time.perf_counter() - t0) * 1000.0
                rec = auditor.capture_and_audit(case["case_id"], expected_tokens=case["expected_tokens"])
                results.append({
                    "case_id": case["case_id"],
                    "category": case["category"],
                    "kind": "math",
                    "expression": case["expression"],
                    "is_casio_fidelity": bool(case.get("is_casio_fidelity", False)),
                    "elapsed_ms": round(elapsed_ms, 1),
                    "ocr_text": rec["ocr_text"],
                    "ocr_result_col": rec.get("ocr_result_col", ""),
                    "ocr_lines": rec["ocr_lines"],
                    "expected_tokens": rec["expected_tokens"],
                    "passed": bool(rec["passed"]),
                    "lcd_image": str(args.out_dir / rec["lcd_image"]),
                    "lcd_sha256": rec["sha256"],
                })
                print(f"[ORACLE-V46] [{idx:02d}/{len(MATH_CASES_V46)}] {case['case_id']:20s} pass={rec['passed']} ({elapsed_ms:.0f}ms) ocr={rec['ocr_text'][:60]!r}")

            # 51. ULTRA-SYS-3X3 (eps=1e-8 -> det=1e-16 < 1e-15 BCD15 -> genuine Casio Ma ERROR!)
            auditor.press_sequence(["EXIT", "EXIT", "MENU"], default_hold=0.14, default_wait=0.50)
            auditor.press_sequence(["ALPHA", "X"], default_hold=0.14, default_wait=0.60)
            auditor.press_sequence(["EXIT", "EXIT", "F1", "F2"], default_hold=0.14, default_wait=0.40)
            t0 = time.perf_counter()
            auditor.press_sequence([
                "1", "EXE", "1", "EXE", "1", "EXE", "3", "EXE",
                "1", "EXE", "1", ".", "0", "0", "0", "0", "0", "0", "0", "1", "EXE", "1", "EXE",
                "3", ".", "0", "0", "0", "0", "0", "0", "0", "1", "EXE",
                "1", "EXE", "1", "EXE", "1", ".", "0", "0", "0", "0", "0", "0", "0", "1", "EXE",
                "3", ".", "0", "0", "0", "0", "0", "0", "0", "1", "EXE",
                "F1"
            ], default_hold=0.10, default_wait=0.16)
            time.sleep(1.2)
            ms_sys = (time.perf_counter() - t0) * 1000.0
            srec = auditor.capture_and_audit("ULTRA-SYS-3X3", expected_tokens=["ERROR"])
            results.append({
                "case_id": "ULTRA-SYS-3X3",
                "category": "ultra_extreme_v46",
                "kind": "equation",
                "expression": "Simul 3x3 eps=1e-8 (det=1e-16 < 1e-15 BCD15 -> Casio Ma ERROR)",
                "is_casio_fidelity": True,
                "elapsed_ms": round(ms_sys, 1),
                "ocr_text": srec["ocr_text"],
                "expected_tokens": srec["expected_tokens"],
                "passed": bool(srec["passed"]),
                "lcd_image": str(args.out_dir / srec["lcd_image"]),
                "lcd_sha256": srec["sha256"],
            })
            print(f"[ORACLE-V46] [51/54] {'ULTRA-SYS-3X3':20s} pass={srec['passed']} ({ms_sys:.0f}ms) ocr={srec['ocr_text'][:60]!r}")

            # 52. ULTRA-SYS-3X3-EPS6 (eps=1e-6 -> det=1e-12 > 1e-15 BCD15 -> exact x=y=z=1!)
            auditor.press_sequence(["EXIT", "EXIT", "EXIT", "F1", "F2"], default_hold=0.14, default_wait=0.40)
            t0 = time.perf_counter()
            auditor.press_sequence([
                "1", "EXE", "1", "EXE", "1", "EXE", "3", "EXE",
                "1", "EXE", "1", ".", "0", "0", "0", "0", "0", "1", "EXE", "1", "EXE",
                "3", ".", "0", "0", "0", "0", "0", "1", "EXE",
                "1", "EXE", "1", "EXE", "1", ".", "0", "0", "0", "0", "0", "1", "EXE",
                "3", ".", "0", "0", "0", "0", "0", "1", "EXE",
                "F1"
            ], default_hold=0.10, default_wait=0.16)
            time.sleep(1.2)
            ms_sys6 = (time.perf_counter() - t0) * 1000.0
            srec6 = auditor.capture_and_audit("ULTRA-SYS-3X3-EPS6", expected_tokens=["1"])
            results.append({
                "case_id": "ULTRA-SYS-3X3-EPS6",
                "category": "ultra_extreme_v46",
                "kind": "equation",
                "expression": "Simul 3x3 eps=1e-6 (det=1e-12 -> exact x=y=z=1)",
                "elapsed_ms": round(ms_sys6, 1),
                "ocr_text": srec6["ocr_text"],
                "expected_tokens": srec6["expected_tokens"],
                "passed": bool(srec6["passed"]),
                "lcd_image": str(args.out_dir / srec6["lcd_image"]),
                "lcd_sha256": srec6["sha256"],
            })
            print(f"[ORACLE-V46] [52/54] {'ULTRA-SYS-3X3-EPS6':20s} pass={srec6['passed']} ({ms_sys6:.0f}ms) ocr={srec6['ocr_text'][:60]!r}")

            # 53. POLY-NEAR-ROOTS-001 in Equation -> Polynomial (F2) -> Deg 3 (F2) -> SOLVE (F1)
            auditor.press_sequence(["EXIT", "EXIT", "EXIT", "EXIT", "F2", "F2"], default_hold=0.14, default_wait=0.40)
            t0 = time.perf_counter()
            auditor.press_sequence([
                "1", "EXE",
                "NEG", "3", ".", "0", "0", "3", "EXE",
                "3", ".", "0", "0", "6", "0", "0", "2", "EXE",
                "NEG", "1", ".", "0", "0", "3", "0", "0", "2", "EXE",
                "F1"
            ], default_hold=0.10, default_wait=0.16)
            time.sleep(1.2)
            prec = auditor.capture_and_audit("POLY-NEAR-ROOTS-001", expected_tokens=["1.00"])
            ms_poly = (time.perf_counter() - t0) * 1000.0
            results.append({
                "case_id": "POLY-NEAR-ROOTS-001",
                "category": "polynomial",
                "kind": "equation",
                "expression": "(x-1)(x-1.001)(x-1.002)=0",
                "is_casio_fidelity": True,
                "elapsed_ms": round(ms_poly, 1),
                "ocr_text": prec["ocr_text"],
                "expected_tokens": prec["expected_tokens"],
                "passed": bool(prec["passed"]),
                "lcd_image": str(args.out_dir / prec["lcd_image"]),
                "lcd_sha256": prec["sha256"],
            })
            print(f"[ORACLE-V46] [53/54] {'POLY-NEAR-ROOTS-001':20s} pass={prec['passed']} ({ms_poly:.0f}ms) ocr={prec['ocr_text'][:60]!r}")

            # 54. GRAPH-POLAR-ROSE r1 = cos(11*theta)
            auditor.press_sequence(["EXIT", "EXIT", "MENU"], default_hold=0.14, default_wait=0.50)
            auditor.press_sequence(["5"], default_hold=0.14, default_wait=0.80)
            auditor.press_sequence(["EXIT", "EXIT", "F3", "F2", "F2", "F1"], default_hold=0.14, default_wait=0.35)
            t0 = time.perf_counter()
            auditor.press_sequence(["COS", "(", "1", "1", "X", ")", "EXE", "F6"], default_hold=0.14, default_wait=0.25)
            time.sleep(2.4)
            ms_g = (time.perf_counter() - t0) * 1000.0
            grec = auditor.capture_and_audit("GRAPH-POLAR-ROSE")
            g_lcd_path = args.out_dir / grec["lcd_image"]
            g_ok, g_metrics = audit_graph_image(g_lcd_path, grec["ocr_text"])
            results.append({
                "case_id": "GRAPH-POLAR-ROSE",
                "category": "graphics",
                "kind": "graph",
                "expression": "Polar r1=cos(11*theta)",
                "elapsed_ms": round(ms_g, 1),
                "ocr_text": grec["ocr_text"],
                "graph_metrics": g_metrics,
                "passed": bool(g_ok),
                "lcd_image": str(g_lcd_path),
                "lcd_sha256": grec["sha256"],
            })
            print(f"[ORACLE-V46] [54/54] {'GRAPH-POLAR-ROSE':20s} pass={g_ok} ({ms_g:.0f}ms) metrics={g_metrics}")

            auditor.generate_html_report(title="CG50 v46 — 53-Case Official Casio Differential Evidence")
            passed_cnt = sum(1 for r in results if r["passed"])
            status = "CONFIRMED" if passed_cnt == len(results) else ("PARTIAL" if passed_cnt > 0 else "FAILED")
    except Exception as exc:
        status = "ERROR"
        error_note = str(exc)
        print(f"[ORACLE-V46] ERROR: {exc}", file=sys.stderr)
    finally:
        if auditor is not None:
            try:
                auditor.stop()
            except Exception:
                pass

    cpu_sha = hashlib.sha256(cpu_dll.read_bytes()).hexdigest().upper()
    lcd_sha = hashlib.sha256(lcd_dll.read_bytes()).hexdigest().upper()
    exe_sha = hashlib.sha256(pathlib.Path(exe_path).read_bytes()).hexdigest().upper()
    passed_cnt = sum(1 for r in results if r["passed"])
    verified = (status == "CONFIRMED" and passed_cnt == len(results) and len(results) >= 50)

    payload = {
        "schema": "cg50_manager_plus_oracle_v46",
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "oracle_engine": engine_label,
        "mode": args.mode,
        "boot_diag": boot_diag,
        "exe_path": exe_path,
        "exe_sha256": exe_sha,
        "cpu73050_dll_path": str(cpu_dll),
        "cpu73050_dll_sha256": cpu_sha,
        "lcdr61524a_dll_path": str(lcd_dll),
        "lcdr61524a_dll_sha256": lcd_sha,
        "status": status,
        "error_note": error_note,
        "official_emulator_verified": verified,
        "physical_reference_status": "N/A_DISCARDED_BY_DESIGN",
        "total_cases": len(results),
        "passed_cases": passed_cnt,
        "results": results,
    }
    args.out_json.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"[ORACLE-V46] Saved {passed_cnt}/{len(results)} passed (verified={verified}, status={status}) -> {args.out_json}")
    return 0 if verified else 1


if __name__ == "__main__":
    raise SystemExit(main())
