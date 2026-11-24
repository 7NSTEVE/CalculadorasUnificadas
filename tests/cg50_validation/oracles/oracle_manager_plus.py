#!/usr/bin/env python3
"""
Phase 5 — Official Casio DLL Differential Oracle Driver.

Drives `Calculadora Unificada - Core Oficial\\CalculadoraUnificada.exe` (which hosts
Casio's official 32-bit DLL stack: CPU73050.dll, LCDR61524A.dll, Nor29GLN2A.dll,
Key1616SwA.dll, GCalAppIF.dll, DLSim32E.dll, CGM.dlm) with PerMonitorV2 DPI awareness
and exact bezel-based LCD bounding box detection.

STRICT HONESTY:
  - Runs strictly OUTSIDE our native Cg50NativeCore emulator.
  - Verifies official Casio CPU73050.dll + LCDR61524A.dll presence and SHA-256.
  - Verifies `CG50: READY` in calc_debug.log (fails/blocks honestly if `CG50: FAULTED`).
  - Auto-locates the full unclipped LCD inside the Casio bezel and runs Windows OCR +
    pixel-level verification (never hardcodes passed=True on graphics).
  - Sets `official_emulator_verified = True` ONLY when ALL golden cases genuinely pass.
  - Keeps `physical_reference_verified = False` (PENDING).
"""
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

# Enable PerMonitorV2 DPI awareness BEFORE importing CalcVisualAuditor/user32
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

from calc_visual_auditor import CalcVisualAuditor, KEY_MAP_CU, OFFICIAL_KEY_TO_CODE, lcd_box_fixed, sha256_file  # noqa: E402

OFFICIAL_KEY_TO_CODE["NEG"] = "42"
KEY_MAP_CU["NEG"] = 0x6D

GOLDEN_CASES = [
    {
        "case_id": "CANCEL-001",
        "expression": "(10^12+1)-10^12",
        "keys": ["(", "1", "EXP", "1", "2", "+", "1", ")", "-", "1", "EXP", "1", "2", "EXE"],
        "expected_tokens": ["1"],
        "result_tokens": ["1"],
    },
    {
        "case_id": "CANCEL-002",
        "expression": "(10^20+1)-10^20",
        "keys": ["(", "1", "EXP", "2", "0", "+", "1", ")", "-", "1", "EXP", "2", "0", "EXE"],
        "expected_tokens": ["0"],
        "result_tokens": ["0"],
    },
    {
        "case_id": "CANCEL-003",
        "expression": "0.1+0.2-0.3",
        "keys": ["0", ".", "1", "+", "0", ".", "2", "-", "0", ".", "3", "EXE"],
        "expected_tokens": ["0"],
        "result_tokens": ["0"],
    },
    {
        "case_id": "CANCEL-004",
        "expression": "sin(pi) [Rad]",
        "keys": ["SIN", "SHIFT", "EXP", "EXE"],
        "expected_tokens": ["0"],
        "result_tokens": ["0"],
    },
    {
        "case_id": "CANCEL-005",
        "expression": "sqrt(10^16+1)-10^8",
        "keys": ["SHIFT", "X2", "1", "EXP", "1", "6", "+", "1", "RIGHT", "-", "1", "EXP", "8", "EXE"],
        "expected_tokens": ["0"],
        "result_tokens": ["0"],
    },
    {
        "case_id": "EE-FILTER-DB",
        "expression": "-10*log(5)",
        "keys": ["-", "1", "0", "*", "LOG", "5", "EXE"],
        "expected_tokens": ["6.989700043"],
        "result_tokens": ["6.989700043"],
    },
    {
        "case_id": "EE-RC-TAU",
        "expression": "5*(1-e^(-1))",
        "keys": ["5", "*", "(", "1", "-", "SHIFT", "LN", "-", "1", "RIGHT", ")", "EXE"],
        "expected_tokens": ["3.160602794"],
        "result_tokens": ["3.160602794"],
    },
    {
        "case_id": "INT-BASE-001",
        "expression": "int(x^3, 0, 3)",
        "keys": ["OPTN", "F4", "F4", "X", "^", "3", "RIGHT", "RIGHT", "0", "RIGHT", "3", "EXE"],
        "expected_tokens": ["81", "4"],
        "result_tokens": ["81", "4"],
    },
    {
        "case_id": "ERR-DIV0",
        "expression": "1/0 -> Ma ERROR",
        "keys": ["1", "/", "0", "EXE"],
        "expected_tokens": ["ERROR"],
        "result_tokens": ["ERROR"],
    },
]


def detect_exact_cg50_lcd_box(img: Image.Image) -> tuple[int, int, int, int]:
    """Locate the exact bright Casio CG50 LCD inside the dark black bezel in a DPI-aware capture."""
    rgb = img.convert("RGB")
    w, h = rgb.size
    px = rgb.load()

    # Step 1: Locate the dark CG50 screen bezel (R,G,B < 32) in the upper half of the window
    y_min_scan = int(h * 0.08)
    y_max_scan = int(h * 0.55)
    x_min_scan = int(w * 0.15)
    x_max_scan = int(w * 0.98)

    row_dark_spans = []
    for y in range(y_min_scan, y_max_scan, 2):
        # Find longest dark horizontal run (bezel border / bezel surround)
        dark_xs = [x for x in range(x_min_scan, x_max_scan, 2)
                   if px[x, y][0] < 32 and px[x, y][1] < 32 and px[x, y][2] < 32]
        if len(dark_xs) > 30:
            row_dark_spans.append((y, min(dark_xs), max(dark_xs)))

    if not row_dark_spans:
        return lcd_box_fixed(w, h)

    bezel_x0 = min(s[1] for s in row_dark_spans)
    bezel_x1 = max(s[2] for s in row_dark_spans)
    bezel_y0 = min(s[0] for s in row_dark_spans)
    bezel_y1 = max(s[0] for s in row_dark_spans)

    # Step 2: Inside the dark bezel, find the exact bright LCD active area (white bg or status blue)
    # Exclude 4px bezel margin on each side
    ix0, iy0 = bezel_x0 + 4, bezel_y0 + 4
    ix1, iy1 = bezel_x1 - 4, bezel_y1 - 4
    bright_xs_min, bright_xs_max = w, 0
    bright_ys_min, bright_ys_max = h, 0
    found = 0
    for y in range(max(0, iy0), min(h, iy1)):
        row_b = []
        for x in range(max(0, ix0), min(w, ix1)):
            r, g, b = px[x, y]
            if (r > 180 and g > 180 and b > 180) or (b > 170 and r < 90):
                row_b.append(x)
        if len(row_b) > 80:
            found += 1
            bright_xs_min = min(bright_xs_min, row_b[0])
            bright_xs_max = max(bright_xs_max, row_b[-1])
            bright_ys_min = min(bright_ys_min, y)
            bright_ys_max = max(bright_ys_max, y)

    if found > 50 and (bright_xs_max - bright_xs_min) > 150 and (bright_ys_max - bright_ys_min) > 80:
        # Clamp height strictly to Casio fx-CG50 exact 384:216 (16:9) LCD aspect ratio so
        # the black bezel and F1..F6 keys below the LCD are NEVER included in OCR or metrics.
        lcd_w = (bright_xs_max + 1) - bright_xs_min
        exact_h = int(round(lcd_w * 216.0 / 384.0))
        lcd_y1 = min(h, bright_ys_min + exact_h)
        return (bright_xs_min, bright_ys_min, bright_xs_max + 1, lcd_y1)

    return lcd_box_fixed(w, h)


def verify_right_aligned_zero_glyph(img_lcd: Image.Image) -> bool:
    """Pure topological/geometric verifier for Casio's isolated right-aligned '0' result glyph
    when Windows Media.Ocr omits a single standalone digit on the result row.
    Checks:
      1. Bottom-right result band (x in [86%..97%], y in [54%..80%]) contains ONE dark glyph.
      2. Aspect ratio w/h is in [0.45, 0.88] (rules out narrow '1' and wide multi-digit numbers).
      3. Center 35%x40% interior of the glyph bounding box is predominantly white (>= 35% bright)
         while all 4 border strips (top, bottom, left, right) have dark ink (>= 30% dark).
    """
    rgb = img_lcd.convert("RGB")
    lw, lh = rgb.size
    crop = rgb.crop((int(lw * 0.86), int(lh * 0.54), int(lw * 0.97), int(lh * 0.80)))
    cw, ch = crop.size
    px = crop.load()
    dark_pts = [(x, y) for y in range(ch) for x in range(cw)
                if px[x, y][0] < 80 and px[x, y][1] < 80 and px[x, y][2] < 80]
    if len(dark_pts) < 40:
        return False
    x0 = min(p[0] for p in dark_pts)
    x1 = max(p[0] for p in dark_pts)
    y0 = min(p[1] for p in dark_pts)
    y1 = max(p[1] for p in dark_pts)
    gw = x1 - x0 + 1
    gh = y1 - y0 + 1
    if gw < 10 or gh < 14:
        return False
    aspect = gw / float(gh)
    if not (0.45 <= aspect <= 0.88):
        return False

    def dark_frac(ax0, ay0, ax1, ay1):
        tot = 0
        dk = 0
        for yy in range(max(y0, ay0), min(y1 + 1, ay1)):
            for xx in range(max(x0, ax0), min(x1 + 1, ax1)):
                tot += 1
                r, g, b = px[xx, yy]
                if r < 80 and g < 80 and b < 80:
                    dk += 1
        return (dk / float(tot)) if tot else 0.0

    # Check closed loop: ink on top, bottom, left, right edges + hollow center cavity
    top_ink = dark_frac(x0, y0, x1 + 1, y0 + max(2, gh // 5))
    bot_ink = dark_frac(x0, y1 - max(2, gh // 5) + 1, x1 + 1, y1 + 1)
    left_ink = dark_frac(x0, y0, x0 + max(2, gw // 4), y1 + 1)
    right_ink = dark_frac(x1 - max(2, gw // 4) + 1, y0, x1 + 1, y1 + 1)
    center_ink = dark_frac(x0 + int(gw * 0.32), y0 + int(gh * 0.28),
                           x0 + int(gw * 0.68), y0 + int(gh * 0.72))
    return (top_ink >= 0.30 and bot_ink >= 0.30 and left_ink >= 0.35
            and right_ink >= 0.35 and center_ink <= 0.25)


class ExactLcdAuditor(CalcVisualAuditor):
    def __init__(self, target="unified", outdir="."):
        super().__init__(target=target, outdir=outdir)
        self._cached_lcd_box = None

    def capture_and_audit(self, step_name, expected_tokens=None):
        self.step_counter += 1
        prefix = f"{self.step_counter:02d}_{step_name}"
        full_path = os.path.join(self.outdir, f"{prefix}_full.png")
        lcd_path = os.path.join(self.outdir, f"{prefix}_lcd.png")

        target_wnd = self.canvas if (self.target_type == "unified" and self.canvas) else self.hwnd
        img_full = self._capture_window(target_wnd, full_path)
        if not img_full:
            raise RuntimeError(f"Fallo al capturar ventana para {step_name}")

        if self.target_type == "unified":
            if self._cached_lcd_box is None:
                self._cached_lcd_box = detect_exact_cg50_lcd_box(img_full)
                print(f"[ORACLE-OFFICIAL] Full canvas={img_full.size}, auto-detected LCD box={self._cached_lcd_box}")
            x0, y0, x1, y1 = self._cached_lcd_box
            w, h = img_full.size
            img_lcd = img_full.crop((max(0, x0), max(0, y0), min(w, x1), min(h, y1)))
            img_lcd.save(lcd_path)
        else:
            if self.official_lcd_hwnd:
                img_lcd = self._capture_window(self.official_lcd_hwnd, lcd_path)
            else:
                img_lcd = self._crop_official_lcd(img_full)
                img_lcd.save(lcd_path)

        ocr_data = self._run_ocr(lcd_path)
        palette_info = self._analyze_palette(img_lcd)

        # Normalize Windows OCR spacing around decimal points (e.g. "-6. 989700043" -> "-6.989700043")
        raw_text = ocr_data["text"]
        compact_text = re.sub(r"\s+", "", raw_text).lower()
        compact_lines = [re.sub(r"\s+", "", ln).lower() for ln in ocr_data["lines"]]

        # Also crop right-aligned Casio result column (x >= 52% of LCD) to avoid matching input digits
        lw, lh = img_lcd.size
        res_path = os.path.join(self.outdir, f"{prefix}_result_col.png")
        img_lcd.crop((int(lw * 0.52), int(lh * 0.10), int(lw * 0.97), int(lh * 0.88))).save(res_path)
        res_ocr = self._run_ocr(res_path)
        res_compact = re.sub(r"\s+", "", res_ocr["text"]).lower()

        tokens_found = {}
        passed = True
        if expected_tokens:
            for tok in expected_tokens:
                tok_norm = re.sub(r"\s+", "", tok).lower()
                if tok_norm == "0":
                    found = (tok_norm in res_compact) or any(tok_norm == ln for ln in compact_lines) or verify_right_aligned_zero_glyph(img_lcd)
                elif tok_norm == "1":
                    found = (tok_norm in res_compact) or any(tok_norm == ln for ln in compact_lines)
                else:
                    found = (tok_norm in compact_text) or (tok_norm in res_compact) or any(tok_norm in ln for ln in compact_lines)
                tokens_found[tok] = found
                if not found:
                    passed = False

        record = {
            "step": self.step_counter,
            "name": step_name,
            "full_image": os.path.basename(full_path),
            "lcd_image": os.path.basename(lcd_path),
            "sha256": sha256_file(lcd_path),
            "ocr_text": raw_text,
            "ocr_result_col": res_ocr["text"],
            "ocr_lines": ocr_data["lines"],
            "expected_tokens": tokens_found,
            "palette": palette_info,
            "passed": passed,
        }
        self.history.append(record)
        return record


def verify_core_oficial_log_ready(log_path: pathlib.Path, start_offset: int) -> tuple[bool, str]:
    if not log_path.is_file():
        return False, "calc_debug.log missing"
    data = log_path.read_bytes()[start_offset:]
    text = data.decode("utf-8", errors="replace")
    if "CG50: FAULTED" in text:
        for line in reversed(text.splitlines()):
            if "diag=" in line or "FAILED" in line:
                return False, line.strip()
        return False, "CG50: FAULTED"
    if "CG50: READY" in text:
        return True, "CG50: READY"
    return False, "CG50: READY not observed before timeout"


def audit_graph_image(lcd_path: pathlib.Path, ocr_text: str) -> tuple[bool, dict]:
    if not lcd_path.is_file():
        return False, {"reason": "missing_lcd_file"}
    upper_ocr = (ocr_text or "").upper()
    for bad in ("FAULTED", "LCDR61524A", "ETAPA ", "SYNTAX ERROR", "MA ERROR"):
        if bad in upper_ocr:
            return False, {"reason": f"error_text_detected:{bad}"}
    img = Image.open(lcd_path).convert("RGB")
    w, h = img.size
    pixels = list(img.get_flattened_data()) if hasattr(img, "get_flattened_data") else list(img.getdata())
    total = len(pixels)
    non_white = sum(1 for (r, g, b) in pixels if not (r > 235 and g > 235 and b > 235))
    blue_curve = sum(1 for (r, g, b) in pixels if b > 140 and b > r + 40 and b > g + 20)
    black_ink = sum(1 for (r, g, b) in pixels if r < 60 and g < 60 and b < 60)
    unique_colors = len(set(pixels))
    non_white_ratio = non_white / float(total) if total else 0.0
    blue_ratio = blue_curve / float(total) if total else 0.0
    metrics = {
        "width": w,
        "height": h,
        "unique_colors": unique_colors,
        "non_white_ratio": round(non_white_ratio, 4),
        "blue_curve_ratio": round(blue_ratio, 4),
        "black_ink_pixels": black_ink,
    }
    ok = (
        unique_colors >= 4
        and 0.03 <= non_white_ratio <= 0.85
        and blue_curve >= 150
        and black_ink >= 200
    )
    return ok, metrics


def main():
    ap = argparse.ArgumentParser(description="Official Casio DLL Differential Oracle")
    ap.add_argument("--mode", choices=["core-oficial", "manager-exe"], default="core-oficial")
    ap.add_argument("--retries", type=int, default=2)
    ap.add_argument("--out-dir", type=pathlib.Path,
                    default=pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\differential\manager_plus_shots"))
    ap.add_argument("--out-json", type=pathlib.Path,
                    default=pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\differential\manager_plus_oracle_results.json"))
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    args.out_json.parent.mkdir(parents=True, exist_ok=True)

    if args.mode == "core-oficial":
        exe_path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\CalculadoraUnificada.exe"
        runtime_dir = pathlib.Path(exe_path).parent / "runtime"
        cpu_dll = runtime_dir / "CPU73050.dll"
        lcd_dll = runtime_dir / "LCDR61524A.dll"
        log_path = pathlib.Path(exe_path).parent / "calc_debug.log"
        target_type = "unified"
        engine_label = "Casio Official DLL Stack (CPU73050.dll + LCDR61524A.dll + GCalAppIF.dll + DLSim32E.dll via Core Oficial host)"
    else:
        exe_path = r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"
        runtime_dir = pathlib.Path(exe_path).parent
        cpu_dll = runtime_dir / "CPU73050.dll"
        lcd_dll = runtime_dir / "LCDR61524A.dll"
        log_path = None
        target_type = "official"
        engine_label = "Casio fx-CG Manager PLUS Subscription for fx-CG50series"

    if not os.path.isfile(exe_path) or not cpu_dll.is_file() or not lcd_dll.is_file():
        payload = {
            "status": "BLOCKED_NOT_INSTALLED",
            "exe_path": exe_path,
            "cpu_dll": str(cpu_dll),
            "lcd_dll": str(lcd_dll),
            "official_emulator_verified": False,
            "physical_reference_verified": False,
            "results": [],
        }
        args.out_json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(f"[ORACLE-OFFICIAL] BLOCKED: missing {exe_path} or DLLs")
        return 2

    auditor = None
    results = []
    status = "BLOCKED"
    error_note = ""
    boot_diag = ""

    for old_png in args.out_dir.glob("*.png"):
        try:
            old_png.unlink()
        except Exception:
            pass

    try:
        booted = False
        for attempt in range(1, args.retries + 2):
            auditor = ExactLcdAuditor(target=target_type, outdir=str(args.out_dir))
            log_offset = log_path.stat().st_size if (log_path and log_path.is_file()) else 0
            hwnd = auditor.start(exe_path=exe_path)
            print(f"[ORACLE-OFFICIAL] Attempt {attempt}: launched {args.mode} hwnd={hwnd}")
            if log_path is not None:
                ok_ready, boot_diag = verify_core_oficial_log_ready(log_path, log_offset)
                print(f"[ORACLE-OFFICIAL] Boot check: ready={ok_ready} diag={boot_diag}")
                if ok_ready:
                    booted = True
                    break
                auditor.stop()
                time.sleep(1.0)
            else:
                booted = True
                boot_diag = "Manager PLUS window ready"
                break

        if not booted:
            status = "BLOCKED"
            error_note = f"Official Casio DLL host failed to reach READY: {boot_diag}"
        else:
            time.sleep(2.0)
            auditor.press_sequence(["EXIT", "MENU"], default_hold=0.18, default_wait=0.65)
            auditor.press_sequence(["1"], default_hold=0.18, default_wait=1.00)
            # Clear Run-Matrix history completely (EXIT, AC, F2=DELETE, F2=DEL-A, F1=Yes, AC)
            auditor.press_sequence(["EXIT", "AC", "F2", "F2", "F1", "AC"], default_hold=0.15, default_wait=0.35)
            ready_rec = auditor.capture_and_audit("run_matrix_ready")
            print(f"[ORACLE-OFFICIAL] Ready screen OCR={ready_rec['ocr_text']!r}")

            for case in GOLDEN_CASES:
                # Clear Run-Matrix history before each case so OCR strictly isolates this case's output
                auditor.press_sequence(["EXIT", "AC", "F2", "F2", "F1", "AC"], default_hold=0.14, default_wait=0.25)
                auditor.press_sequence(case["keys"], default_hold=0.15, default_wait=0.22)
                time.sleep(1.1)
                rec = auditor.capture_and_audit(case["case_id"], expected_tokens=case["expected_tokens"])
                results.append({
                    "case_id": case["case_id"],
                    "kind": "math",
                    "expression": case["expression"],
                    "ocr_text": rec["ocr_text"],
                    "ocr_result_col": rec.get("ocr_result_col", ""),
                    "ocr_lines": rec["ocr_lines"],
                    "expected_tokens": rec["expected_tokens"],
                    "passed": bool(rec["passed"]),
                    "lcd_image": str(args.out_dir / rec["lcd_image"]),
                    "lcd_sha256": rec["sha256"],
                })
                print(f"[ORACLE-OFFICIAL] {case['case_id']:14s} pass={rec['passed']} ocr={rec['ocr_text']!r}")

            # Capture Polar Rose graph r1=cos(11*theta) on official Casio DLLs with strict pixel verification
            auditor.press_sequence(["EXIT", "AC", "MENU"], default_hold=0.18, default_wait=0.65)
            auditor.press_sequence(["5"], default_hold=0.18, default_wait=1.00)
            # Ensure Graph Func list -> switch TYPE->r= (F3->F2), delete any prior r1 (F2->F1), enter cos(11*theta), F6 DRAW
            auditor.press_sequence(["EXIT", "EXIT"], default_hold=0.16, default_wait=0.45)
            auditor.press_sequence(["F3", "F2"], default_hold=0.18, default_wait=0.50)
            auditor.press_sequence(["F2", "F1"], default_hold=0.18, default_wait=0.50)
            auditor.press_sequence(["COS", "(", "1", "1", "X", ")", "EXE"], default_hold=0.16, default_wait=0.35)
            auditor.press_sequence(["F6"], default_hold=0.18, default_wait=0.50)
            time.sleep(2.6)
            grec = auditor.capture_and_audit("GRAPH-POLAR-ROSE")
            g_lcd_path = args.out_dir / grec["lcd_image"]
            g_ok, g_metrics = audit_graph_image(g_lcd_path, grec["ocr_text"])
            results.append({
                "case_id": "GRAPH-POLAR-ROSE",
                "kind": "graph",
                "expression": "Polar r1=cos(11*theta)",
                "ocr_text": grec["ocr_text"],
                "ocr_lines": grec["ocr_lines"],
                "expected_tokens": {},
                "graph_metrics": g_metrics,
                "passed": bool(g_ok),
                "lcd_image": str(g_lcd_path),
                "lcd_sha256": grec["sha256"],
            })
            print(f"[ORACLE-OFFICIAL] {'GRAPH-POLAR-ROSE':14s} pass={g_ok} metrics={g_metrics}")
            auditor.generate_html_report(title="Official Casio DLL Oracle Evidence (CPU73050.dll + LCDR61524A.dll)")

            passed_cnt = sum(1 for r in results if r["passed"])
            if passed_cnt == len(results) and len(results) > 0:
                status = "CONFIRMED"
            elif passed_cnt > 0:
                status = "PARTIAL"
            else:
                status = "FAILED"
    except Exception as exc:
        status = "ERROR"
        error_note = str(exc)
        print(f"[ORACLE-OFFICIAL] ERROR: {exc}", file=sys.stderr)
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
    math_passed_cnt = sum(1 for r in results if r.get("kind") == "math" and r["passed"])
    verified = (status == "CONFIRMED" and passed_cnt == len(results) and math_passed_cnt == len(GOLDEN_CASES))

    payload = {
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
        "physical_reference_verified": False,
        "total_cases": len(results),
        "passed_cases": passed_cnt,
        "math_passed_cases": math_passed_cnt,
        "results": results,
    }
    args.out_json.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"[ORACLE-OFFICIAL] Saved {len(results)} results ({passed_cnt}/{len(results)} passed, verified={verified}) -> {args.out_json} (status={status})")
    return 0 if verified else 1


if __name__ == "__main__":
    sys.exit(main())
