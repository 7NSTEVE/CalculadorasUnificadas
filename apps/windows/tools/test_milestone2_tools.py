#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_milestone2_tools.py — Comprehensive Unit & Integration Tests for Milestone 2 (M2).
Covers all deliverables for Requirements R2, R3, and R4:
  - tools/lcd_diff.py (LCD normalization, 180° rotation, pixel diff, triptych, classifier)
  - tools/visual_gallery.py (HTML visual gallery, dark theme, failure-first jump link)
  - tools/divergence_locator.py (keystroke bisection, fail-fast, memory windows, trace alignment)
  - tools/cg50_agent.py (diff subcommand, fail-closed build guard, matrix codes, pacing)
  - tools/read-calculator-state.cmd (compatibility CLI forwarder)
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from typing import Dict, Any, List
from PIL import Image, ImageDraw

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

from lcd_diff import (
    CANONICAL_WIDTH,
    CANONICAL_HEIGHT,
    CANONICAL_SIZE,
    TOTAL_PIXELS,
    COLOR_MAGENTA,
    PixelDiffResult,
    normalize_lcd,
    calculate_pixel_diff,
    generate_magenta_diff_image,
    generate_triptych,
    classify_lcd_content,
    detect_blackout,
    FrameCaptureError,
    BlackoutScreenError,
    DimensionMismatchError
)
from visual_gallery import VisualGallery
from divergence_locator import (
    DivergenceLocator,
    DivergenceType,
    MemoryDiscrepancy,
    TraceDiscrepancy,
    FirstDivergence,
    CASIO_MEM_WINDOWS
)
from cg50_agent import (
    EXACT_MATRIX_KEYS,
    OFFICIAL_KEY_TO_CODE,
    KEY_MAP_CU
)


def create_test_lcd(text: str = "TEST", bg: tuple = (255, 255, 255), fg: tuple = (0, 0, 0)) -> Image.Image:
    """Helper to create a synthetic 384x216 LCD image with drawn text."""
    img = Image.new("RGB", CANONICAL_SIZE, bg)
    draw = ImageDraw.Draw(img)
    draw.text((20, 20), text, fill=fg)
    draw.rectangle([5, 5, 379, 211], outline=(0, 0, 150))
    return img


# =========================================================================
# 1. Tests for lcd_diff.py (Requirement R3)
# =========================================================================

class TestLcdDiff(unittest.TestCase):
    """Unit and boundary tests for canonical LCD normalization and pixel diff engine."""

    def test_canonical_normalization_dimensions_and_mode(self):
        """Verifies resizing to 384x216 RGB24 format."""
        raw = Image.new("RGBA", (564, 560), (255, 255, 255, 255))
        norm = normalize_lcd(raw, backend="official", rotate_native=False)
        self.assertEqual(norm.size, CANONICAL_SIZE)
        self.assertEqual(norm.mode, "RGB")

    def test_native_180_rotation_parity(self):
        """Verifies 180° rotation on native capture compensates for inverted mounting."""
        upright = create_test_lcd("UPRIGHT")
        raw_native = upright.transpose(Image.Transpose.ROTATE_180 if hasattr(Image, "Transpose") else Image.ROTATE_180)
        self.assertNotEqual(raw_native.tobytes(), upright.tobytes())

        norm_native = normalize_lcd(raw_native, backend="native", rotate_native=True)
        self.assertEqual(norm_native.tobytes(), upright.tobytes())

    def test_blackout_screen_detection(self):
        """Verifies detect_blackout triggers ERR_BLACK_SCREEN on fully black framebuffers."""
        black = Image.new("RGB", CANONICAL_SIZE, (0, 0, 0))
        res = detect_blackout(black, min_nonzero=1000)
        self.assertTrue(res["blackout"])
        self.assertEqual(res["error"], "ERR_BLACK_SCREEN")
        self.assertEqual(res["exit_code"], 1)

        normal = create_test_lcd("NORMAL")
        res_normal = detect_blackout(normal, min_nonzero=1000)
        self.assertFalse(res_normal["blackout"])
        self.assertEqual(res_normal["exit_code"], 0)

    def test_dimension_mismatch_error(self):
        """Verifies calculate_pixel_diff flags TAMANOS_DISTINTOS when dimensions differ."""
        img1 = Image.new("RGB", (384, 216), (255, 255, 255))
        img2 = Image.new("RGB", (400, 216), (255, 255, 255))
        res = calculate_pixel_diff(img1, img2)
        self.assertEqual(res["error"], "TAMANOS_DISTINTOS")
        self.assertFalse(res["match"])
        self.assertEqual(res["status"], "DIFF")

    def test_identical_framebuffers_zero_diff(self):
        """Verifies identical images produce exactly 0 changed pixels, match=True, and None bbox."""
        img_a = create_test_lcd("MATCH_TEST")
        img_b = create_test_lcd("MATCH_TEST")
        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 0)
        self.assertEqual(res["changed_percent"], 0.0)
        self.assertIsNone(res["diff_bbox"])
        self.assertTrue(res["match"])
        self.assertEqual(res["status"], "MATCH")

    def test_subthreshold_noise_ignored(self):
        """Verifies differences <= threshold are filtered as quantization noise."""
        img_a = Image.new("RGB", CANONICAL_SIZE, (100, 100, 100))
        img_b = Image.new("RGB", CANONICAL_SIZE, (104, 103, 102))  # delta <= 4
        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 0)
        self.assertTrue(res["match"])

    def test_above_threshold_detected_with_bbox(self):
        """Verifies pixel divergence > threshold is counted and bounding box extracted."""
        img_a = Image.new("RGB", CANONICAL_SIZE, (255, 255, 255))
        img_b = img_a.copy()
        draw = ImageDraw.Draw(img_b)
        # Discrepancy rectangle (100, 50, 120, 70) -> width 20, height 20 = 400 pixels
        draw.rectangle([100, 50, 119, 69], fill=(0, 0, 0))

        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 400)
        self.assertFalse(res["match"])
        self.assertEqual(res["status"], "DIFF")
        self.assertEqual(res["diff_bbox"], (100, 50, 120, 70))

    def test_magenta_diff_highlight_artifact(self):
        """Verifies generate_magenta_diff_image turns differing pixels into RGB(255, 0, 255)."""
        img_a = Image.new("RGB", CANONICAL_SIZE, (255, 255, 255))
        img_b = img_a.copy()
        img_b.putpixel((50, 50), (0, 0, 0))

        diff_img = generate_magenta_diff_image(img_a, img_b, threshold=4)
        self.assertEqual(diff_img.size, CANONICAL_SIZE)
        px = diff_img.load()
        self.assertEqual(px[50, 50], COLOR_MAGENTA)
        # Non-divergent pixel retains base image color
        self.assertEqual(px[10, 10], (255, 255, 255))

    def test_triptych_generation(self):
        """Verifies 3-panel triptych creation produces width 1152 and height 216."""
        img1 = create_test_lcd("OFFICIAL")
        img2 = create_test_lcd("NATIVE")
        diff = generate_magenta_diff_image(img1, img2, threshold=4)

        triptych = generate_triptych(img1, img2, diff)
        self.assertEqual(triptych.size, (CANONICAL_WIDTH * 3, CANONICAL_HEIGHT))
        self.assertEqual(triptych.size, (1152, 216))

    def test_screen_content_classifier(self):
        """Verifies heuristic classification into menu, error_dialog, or texto_denso."""
        # Menu with rich colorful icons (>10% colorful pixels)
        menu_img = Image.new("RGB", CANONICAL_SIZE, (255, 255, 255))
        draw_menu = ImageDraw.Draw(menu_img)
        # 8 boxes of 60x45 = 21,600 pixels (>25% of total screen)
        for idx in range(8):
            row = idx // 4
            col = idx % 4
            x0 = 20 + col * 80
            y0 = 30 + row * 60
            color = (0 if idx % 2 == 0 else 230, 120 if idx % 3 == 0 else 20, 215 if idx % 2 == 1 else 10)
            draw_menu.rectangle([x0, y0, x0 + 60, y0 + 45], fill=color)
        self.assertEqual(classify_lcd_content(menu_img), "menu")

        # Error screen with red header/border
        err_img = Image.new("RGB", CANONICAL_SIZE, (255, 255, 255))
        draw_err = ImageDraw.Draw(err_img)
        draw_err.rectangle([10, 10, 370, 30], fill=(220, 20, 20))
        self.assertEqual(classify_lcd_content(err_img), "error_dialog")

        # Standard monochrome calculation text screen
        calc_img = create_test_lcd("1+2=3")
        self.assertEqual(classify_lcd_content(calc_img), "texto_denso")



# =========================================================================
# 2. Tests for visual_gallery.py (Requirement R3)
# =========================================================================

class TestVisualGallery(unittest.TestCase):
    """Unit tests for HTML visual gallery generation."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="gallery_test_")

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_gallery_rendering_pass_workflow(self):
        """Verifies HTML report generation for a passing test run."""
        gallery = VisualGallery(outdir=self.temp_dir, title="Test Gallery Pass")
        gallery.set_metadata(
            key_sequence="1,+,2,EXE",
            verdict="PASS",
            duration_s=2.5,
            binary_sha256="AABBCCDDEEFF0011223344"
        )
        diff_res = {
            "status": "MATCH",
            "changed_pixels": 0,
            "changed_percent": 0.0,
            "diff_bbox": None,
            "match": True
        }
        gallery.add_step(
            step=1,
            key="1",
            diff_res=diff_res,
            ocr_official="1",
            ocr_native="1"
        )
        html_text = gallery.render_html()
        self.assertIn("Test Gallery Pass", html_text)
        self.assertIn("PASS", html_text)
        self.assertIn("1,+,2,EXE", html_text)
        self.assertIn("Paso #1", html_text)
        self.assertIn("MATCH", html_text)

    def test_gallery_rendering_divergence_jump_banner(self):
        """Verifies divergence generates failure jump banner pointing to divergent step."""
        gallery = VisualGallery(outdir=self.temp_dir, title="Test Gallery Divergence")
        first_div = {
            "step": 3,
            "key": "*",
            "changed_pixels": 450,
            "changed_percent": 0.54,
            "divergence_type": "LCD_PIXEL_MISMATCH"
        }
        gallery.set_metadata(
            key_sequence="1,+,2,*,3,EXE",
            verdict="DIVERGENCIA",
            first_divergence=first_div
        )
        diff_res = {
            "status": "DIFF",
            "changed_pixels": 450,
            "changed_percent": 0.54,
            "diff_bbox": (100, 50, 150, 80),
            "match": False
        }
        gallery.add_step(step=3, key="*", diff_res=diff_res)
        html_text = gallery.render_html()
        self.assertIn("PRIMERA DIVERGENCIA DETECTADA", html_text)
        self.assertIn('href="#step-3"', html_text)
        self.assertIn("450", html_text)
        self.assertIn("DIFF", html_text)

    def test_save_html_file(self):
        """Verifies saving gallery to disk creates valid index.html."""
        gallery = VisualGallery(outdir=self.temp_dir)
        path = gallery.save_html("index.html")
        self.assertTrue(os.path.exists(path))
        self.assertGreater(os.path.getsize(path), 1000)

    def test_generate_from_report_class_method(self):
        """Verifies generate_from_report creates gallery from standard report dictionary."""
        mock_report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": "2026-10-31T03:00:00Z",
            "build_control": {
                "status": "PASS",
                "compiler": "MSVC",
                "target": "CalculadoraUnificada.exe",
                "duration_seconds": 10.0,
                "binary_path": "CalculadoraUnificada.exe",
                "binary_sha256": "3B8C1122"
            },
            "verdict": "PASS",
            "total_steps": 1,
            "steps_executed": 1,
            "first_divergence": None,
            "steps": [
                {
                    "step": 1,
                    "key": "1",
                    "native": {"pc": "0x0801382A", "ocr_text": "1"},
                    "official": {"pc": "0x0801382A", "ocr_text": "1"},
                    "comparison": {
                        "status": "MATCH",
                        "pixel_diff": {"changed_pixels": 0, "changed_percent": 0.0, "diff_bbox": None},
                        "state_diff": {"pc_match": True, "ocr_match": True}
                    }
                }
            ]
        }
        out_file = VisualGallery.generate_from_report(mock_report, outdir=self.temp_dir)
        self.assertTrue(os.path.exists(out_file))


# =========================================================================
# 3. Tests for divergence_locator.py (Requirement R4)
# =========================================================================

class TestDivergenceLocator(unittest.TestCase):
    """Unit tests for multi-tier divergence localization engine."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="div_test_")

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_bisection_pass_workflow(self):
        """Verifies bisection executes all steps and returns exit code 0 when all steps match."""
        locator = DivergenceLocator(out_dir=self.temp_dir, stop_on_first=True)

        def mock_executor(idx, key):
            nat = {"pc": "0x0801382A", "ocr_text": key}
            off = {"pc": "0x0801382A", "ocr_text": key}
            comp = {
                "status": "MATCH",
                "pixel_diff": {"changed_pixels": 0, "changed_percent": 0.0, "diff_bbox": None},
                "state_diff": {"pc_match": True, "ocr_match": True}
            }
            return nat, off, comp

        build_ctrl = {"status": "PASS", "compiler": "MSVC", "target": "CalculadoraUnificada.exe", "binary_path": "bin", "binary_sha256": "sha"}
        exit_code, report = locator.bisect_keystrokes("1,+,2,EXE", mock_executor, build_ctrl)
        self.assertEqual(exit_code, 0)
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["total_steps"], 4)
        self.assertEqual(report["steps_executed"], 4)
        self.assertIsNone(report["first_divergence"])

    def test_bisection_fail_fast_stop_on_first(self):
        """Verifies --stop-on-first-divergence halts immediately at first divergence."""
        locator = DivergenceLocator(out_dir=self.temp_dir, stop_on_first=True)

        def mock_executor(idx, key):
            is_div = (idx == 2)
            nat = {"pc": "0x0801382A" if not is_div else "0x08020000", "ocr_text": key}
            off = {"pc": "0x0801382A", "ocr_text": key}
            comp = {
                "status": "MATCH" if not is_div else "DIFF",
                "pixel_diff": {
                    "changed_pixels": 0 if not is_div else 150,
                    "changed_percent": 0.0 if not is_div else 0.18,
                    "diff_bbox": None if not is_div else (50, 50, 70, 70)
                },
                "state_diff": {"pc_match": not is_div, "ocr_match": True}
            }
            return nat, off, comp

        build_ctrl = {"status": "PASS", "compiler": "MSVC", "target": "CalculadoraUnificada.exe", "binary_path": "bin", "binary_sha256": "sha"}
        # 5-step sequence, divergence at step 2
        exit_code, report = locator.bisect_keystrokes("1,+,2,*,EXE", mock_executor, build_ctrl)
        self.assertEqual(exit_code, 1)
        self.assertEqual(report["verdict"], "DIVERGENCIA")
        self.assertEqual(report["total_steps"], 5)
        # Must halt at step 2
        self.assertEqual(report["steps_executed"], 2)
        self.assertEqual(len(report["steps"]), 2)
        fd = report["first_divergence"]
        self.assertIsNotNone(fd)
        self.assertEqual(fd["step"], 2)
        self.assertEqual(fd["key"], "+")

    def test_memory_window_isolation(self):
        """Verifies byte-by-byte memory buffer comparison isolates exact address and hex context."""
        # Work buffer base is 0x88184D00
        nat_work = bytearray(b"\x00" * 100)
        off_work = bytearray(b"\x00" * 100)
        # Introduce difference at offset 24
        nat_work[24] = 0xAA
        off_work[24] = 0xBB

        disc = DivergenceLocator.isolate_memory_divergence("Work", 0x88184D00, bytes(nat_work), bytes(off_work))
        self.assertIsNotNone(disc)
        self.assertEqual(disc["buffer_name"], "Work")
        self.assertEqual(disc["offset"], 24)
        self.assertEqual(disc["divergent_address"], f"0x{0x88184D00 + 24:08X}")
        self.assertEqual(disc["native_byte"], "0xAA")
        self.assertEqual(disc["official_byte"], "0xBB")
        self.assertIn("AA", disc["native_context"])

    def test_instruction_trace_ordinal_alignment(self):
        """Verifies instruction trace alignment pinpoints exact ordinal and PC mismatch."""
        nat_lines = [
            "00000001 PC=0x08013820 OP=0xD005 CYC=200000000",
            "00000002 PC=0x08013822 OP=0x4010 CYC=200000002",
            "00000003 PC=0x08013824 OP=0x0009 CYC=200000004"
        ]
        off_records = [
            {"guest_pc_ebp": "08013820", "opcode": "D005"},
            {"guest_pc_ebp": "08013822", "opcode": "4010"},
            {"guest_pc_ebp": "08013824", "opcode": "9999"}  # Mismatched opcode at ordinal 3
        ]
        res = DivergenceLocator.compare_instruction_traces(nat_lines, off_records)
        self.assertEqual(res["status"], "DIFF")
        fd = res["first_divergence"]
        self.assertIsNotNone(fd)
        self.assertEqual(fd["ordinal"], 3)
        self.assertEqual(fd["native_op"], "0x0009")
        self.assertEqual(fd["official_op"], "0x9999")

    def test_instruction_trace_empty_safe_handling(self):
        """Verifies empty trace input returns structured ERR_TRACE_EMPTY safely."""
        res = DivergenceLocator.compare_instruction_traces([], [])
        self.assertEqual(res["status"], "ERROR")
        self.assertEqual(res["exit_code"], 2)
        self.assertEqual(res["error"], "ERR_TRACE_EMPTY")

    def test_console_report_formatting(self):
        """Verifies format_console_report renders a clean terminal summary."""
        mock_report = {
            "verdict": "DIVERGENCIA",
            "total_steps": 3,
            "steps_executed": 2,
            "build_control": {"status": "PASS", "compiler": "MSVC", "target": "CalculadoraUnificada.exe", "binary_sha256": "ABCD"},
            "first_divergence": {
                "step": 2,
                "key": "+",
                "divergence_type": "LCD_PIXEL_MISMATCH",
                "details": "Pixel mismatch",
                "changed_pixels": 120,
                "changed_percent": 0.14,
                "diff_bbox": [10, 10, 30, 30]
            }
        }
        text = DivergenceLocator.format_console_report(mock_report)
        self.assertIn("DIVERGENCE REPORT", text)
        self.assertIn("Step Number      : #2", text)
        self.assertIn("Triggering Key   : '+'", text)
        self.assertIn("Verdict          : DIVERGENCIA", text)



# =========================================================================
# 4. Tests for cg50_agent.py (Requirements R2 & Contracts)
# =========================================================================

class TestCg50AgentDiff(unittest.TestCase):
    """Unit tests for cg50_agent diff CLI and key synchronization dictionaries."""

    def test_hardware_matrix_exact_keys_coverage(self):
        """Verifies 16-bit matrix mapping (KI << 8) | KO for required keys."""
        required = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                    "+", "-", "*", "/", "EXE", "AC", "DEL", "MENU", "F1", "F6"]
        for k in required:
            self.assertIn(k, EXACT_MATRIX_KEYS, f"Matrix code missing for {k}")
            code = EXACT_MATRIX_KEYS[k]
            ki = (code >> 8) & 0xFF
            ko = code & 0xFF
            self.assertGreater(ki, 0)
            self.assertGreater(ko, 0)

    def test_official_button_codes_coverage(self):
        """Verifies official 50 child button hex IDs for BM_CLICK dispatch."""
        required = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                    "+", "-", "*", "/", "EXE", "AC", "DEL", "MENU", "F1", "F6"]
        for k in required:
            self.assertIn(k, OFFICIAL_KEY_TO_CODE, f"Official button hex missing for {k}")
            hex_id = OFFICIAL_KEY_TO_CODE[k]
            self.assertEqual(len(hex_id), 2)
            int(hex_id, 16)  # Must parse as hex

    def test_virtual_keys_for_native_canvas(self):
        """Verifies virtual key mapping KEY_MAP_CU for native canvas dispatch."""
        required = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                    "+", "-", "*", "/", "EXE", "AC", "DEL", "MENU"]
        for k in required:
            self.assertIn(k, KEY_MAP_CU, f"Virtual key missing for {k}")
            self.assertIsInstance(KEY_MAP_CU[k], int)

    def test_diff_subcommand_cli_help(self):
        """Verifies python cg50_agent.py diff --help returns exit code 0 and lists flags."""
        agent_py = os.path.join(TOOLS_DIR, "cg50_agent.py")
        cmd = [sys.executable, agent_py, "diff", "--help"]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("--keys", res.stdout)
        self.assertIn("--tolerance", res.stdout)
        self.assertIn("--stop-on-first-divergence", res.stdout)
        self.assertIn("--rebuild", res.stdout)
        self.assertIn("--hold-ms", res.stdout)

    def test_diff_cli_invalid_key_aborts_code_2(self):
        """Verifies invalid key token aborts with exit code 2 and ERR_DESYNC_KEY."""
        agent_py = os.path.join(TOOLS_DIR, "cg50_agent.py")
        cmd = [sys.executable, agent_py, "diff", "--keys", "UNKNOWN_TOKEN_ABC"]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 2)
        data = json.loads(res.stdout)
        self.assertEqual(data.get("error"), "ERR_DESYNC_KEY")
        self.assertEqual(data.get("verdict"), "FAIL")

    def test_diff_cli_unsafe_pacing_aborts_code_2(self):
        """Verifies hold-ms < 5000 without --allow-short-holds aborts with exit code 2 and ERR_UNSAFE_PACING."""
        agent_py = os.path.join(TOOLS_DIR, "cg50_agent.py")
        cmd = [sys.executable, agent_py, "diff", "--keys", "1", "--hold-ms", "100"]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 2)
        data = json.loads(res.stdout)
        self.assertEqual(data.get("error"), "ERR_UNSAFE_PACING")

    def test_diff_cli_empty_keys_returns_pass_code_0(self):
        """Verifies empty key sequence returns PASS with exit code 0."""
        agent_py = os.path.join(TOOLS_DIR, "cg50_agent.py")
        cmd = [sys.executable, agent_py, "diff", "--keys", "  "]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        data = json.loads(res.stdout)
        self.assertEqual(data.get("verdict"), "PASS")
        self.assertEqual(data.get("total_steps"), 0)
        self.assertEqual(data.get("steps_executed"), 0)



# =========================================================================
# 5. Tests for read-calculator-state.cmd Forwarder
# =========================================================================

class TestCompatibilityForwarder(unittest.TestCase):
    """Verifies legacy read-calculator-state.cmd forwards arguments to cg50_agent.py state."""

    def test_forwarder_invocation_help(self):
        cmd_path = os.path.join(TOOLS_DIR, "read-calculator-state.cmd")
        self.assertTrue(os.path.exists(cmd_path), f"Forwarder script missing: {cmd_path}")

        res = subprocess.run(["cmd.exe", "/c", cmd_path, "--help"],
                             capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("cg50_agent.py state", res.stdout)
        self.assertIn("--backend", res.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
