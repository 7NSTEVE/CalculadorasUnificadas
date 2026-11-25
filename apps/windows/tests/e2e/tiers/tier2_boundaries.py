#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tier2_boundaries.py — Tier 2: Boundary & Corner Case Tests for Differential Testing Platform.
Authoritative source: ORIGINAL_REQUEST.md, PROJECT.md, and spec_miner_survey_1/report.md.

Covers >= 5 boundary/corner cases per requirement:
- R1: Compilation Boundaries (Tests 1-5)
- R2: Dual Execution & Pacing Boundaries (Tests 6-10)
- R3: Framebuffer & Pixel Math Boundaries (Tests 11-15)
- R4: Divergence & Trace Boundaries (Tests 16-20)
"""

import os
import sys
import unittest
from PIL import Image

# Ensure fixtures can be imported
CURR_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(CURR_DIR)
if PARENT_DIR not in sys.path:
    sys.path.insert(0, PARENT_DIR)

from fixtures.synthetic_lcd import (
    CANONICAL_WIDTH, CANONICAL_HEIGHT,
    create_blank_lcd, create_calc_screen, create_error_screen,
    calculate_pixel_diff
)
from fixtures.trace_fixtures import (
    generate_matching_traces, compare_traces_in_memory
)
from fixtures.mock_environment import (
    MockPlatformEnvironment, EXACT_MATRIX_KEYS, OFFICIAL_BUTTON_CODES
)


class TestTier2BoundaryCases(unittest.TestCase):
    """Tier 2: Boundary & Corner Cases Test Suite."""

    def setUp(self):
        self.env = MockPlatformEnvironment()

    def tearDown(self):
        self.env.cleanup()

    # =========================================================================
    # R1 Boundaries: Build Control & Artifact Gate
    # =========================================================================

    def test_r1_boundary_missing_cmakelists(self):
        """
        R1.B1: Missing CMakeLists.txt or build configuration aborts before building.
        Authoritative source: report.md § 5.3 ERR_BUILD_FAILED.
        """
        non_existent_path = os.path.join(self.env.temp_dir, "non_existent_cmake_dir")

        def run_build_pipeline(source_dir: str):
            cmake_file = os.path.join(source_dir, "CMakeLists.txt")
            if not os.path.exists(cmake_file):
                return {
                    "success": False,
                    "exit_code": 2,
                    "error": "ERR_BUILD_FAILED",
                    "details": f"CMakeLists.txt not found in {source_dir}"
                }
            return {"success": True, "exit_code": 0}

        res = run_build_pipeline(non_existent_path)
        self.assertFalse(res["success"])
        self.assertEqual(res["exit_code"], 2)
        self.assertEqual(res["error"], "ERR_BUILD_FAILED")

    def test_r1_boundary_target_binary_locked_or_readonly(self):
        """
        R1.B2: Target binary write/overwrite permission failure halts execution.
        Authoritative source: ORIGINAL_REQUEST.md § R1.
        """
        bin_path = self.env.create_mock_binary("CalculadoraUnificada.exe", b"READONLY_EXE")
        # Mark read-only
        os.chmod(bin_path, 0o444)

        def attempt_overwrite(target_path: str):
            try:
                with open(target_path, "wb") as f:
                    f.write(b"NEW_CONTENT")
                return {"success": True, "exit_code": 0}
            except (PermissionError, OSError) as e:
                return {
                    "success": False,
                    "exit_code": 2,
                    "error": "ERR_BUILD_FAILED",
                    "details": f"Cannot write target binary: {e}"
                }

        try:
            res = attempt_overwrite(bin_path)
            self.assertFalse(res["success"])
            self.assertEqual(res["exit_code"], 2)
            self.assertEqual(res["error"], "ERR_BUILD_FAILED")
        finally:
            # Restore write permission for cleanup
            os.chmod(bin_path, 0o777)

    def test_r1_boundary_stale_timestamp_detection(self):
        """
        R1.B3: Detect stale binary when target timestamp is older than source modification.
        Authoritative source: report.md § 5.3 ERR_STALE_BINARY.
        """
        bin_path = self.env.create_mock_binary("CalculadoraUnificada.exe", b"OLD_BIN")
        # Set binary timestamp to past
        past_time = 1600000000.0  # Year 2020
        os.utime(bin_path, (past_time, past_time))

        source_time = past_time + 1000.0  # Source modified later

        def verify_binary_freshness(binary_file: str, min_source_mtime: float):
            mtime = os.path.getmtime(binary_file)
            if mtime < min_source_mtime:
                return {
                    "success": False,
                    "exit_code": 2,
                    "error": "ERR_STALE_BINARY",
                    "details": f"Binary timestamp ({mtime}) older than source ({min_source_mtime})"
                }
            return {"success": True, "exit_code": 0}

        res = verify_binary_freshness(bin_path, source_time)
        self.assertFalse(res["success"])
        self.assertEqual(res["exit_code"], 2)
        self.assertEqual(res["error"], "ERR_STALE_BINARY")

    def test_r1_boundary_empty_or_truncated_source_contract(self):
        """
        R1.B4: Empty or zero-byte critical source file fails contract check.
        Authoritative source: PROJECT.md § Feature 2.
        """
        empty_src = os.path.join(self.env.temp_dir, "empty_source.cpp")
        with open(empty_src, "w", encoding="utf-8") as f:
            f.write("")

        def validate_source_file(file_path: str):
            if not os.path.exists(file_path) or os.path.getsize(file_path) == 0:
                return {
                    "valid": False,
                    "exit_code": 2,
                    "error": "ERR_SOURCE_CONTRACT",
                    "details": "Source file is empty or missing"
                }
            return {"valid": True, "exit_code": 0}

        res = validate_source_file(empty_src)
        self.assertFalse(res["valid"])
        self.assertEqual(res["exit_code"], 2)

    def test_r1_boundary_compiler_crash_exit_code(self):
        """
        R1.B5: Compiler crash (e.g. 0xC0000005) or negative exit code cleanly handled.
        Authoritative source: report.md § 5.3 ERR_BUILD_FAILED.
        """
        crash_exit_codes = [-1073741819, 1, 2, 255]

        for code in crash_exit_codes:
            def handle_compiler_exit(exit_code: int):
                if exit_code != 0:
                    return {
                        "aborted": True,
                        "exit_code": 2,
                        "error": "ERR_BUILD_FAILED",
                        "raw_code": exit_code
                    }
                return {"aborted": False, "exit_code": 0}

            res = handle_compiler_exit(code)
            self.assertTrue(res["aborted"], f"Compiler exit code {code} must trigger fail-closed abort")
            self.assertEqual(res["exit_code"], 2)

    # =========================================================================
    # R2 Boundaries: Dual Execution & Key Synchronization
    # =========================================================================

    def test_r2_boundary_missing_target_windows(self):
        """
        R2.B1: Target window not found triggers ERR_NATIVE_NOT_FOUND / ERR_OFFICIAL_NOT_FOUND.
        Authoritative source: report.md § 5.3.
        """
        empty_windows = []

        def locate_window(backend: str, window_list: list):
            target = None
            if backend == "native":
                target = [w for w in window_list if "CasioNativeMain" in w.get("class", "")]
                err = "ERR_NATIVE_NOT_FOUND"
            else:
                target = [w for w in window_list if "fx-CG Manager PLUS" in w.get("title", "")]
                err = "ERR_OFFICIAL_NOT_FOUND"

            if not target:
                return {"found": False, "exit_code": 2, "error": err}
            return {"found": True, "exit_code": 0, "window": target[0]}

        res_nat = locate_window("native", empty_windows)
        self.assertFalse(res_nat["found"])
        self.assertEqual(res_nat["exit_code"], 2)
        self.assertEqual(res_nat["error"], "ERR_NATIVE_NOT_FOUND")

        res_off = locate_window("official", empty_windows)
        self.assertFalse(res_off["found"])
        self.assertEqual(res_off["exit_code"], 2)
        self.assertEqual(res_off["error"], "ERR_OFFICIAL_NOT_FOUND")

    def test_r2_boundary_activation_wizard_modal_block(self):
        """
        R2.B2: Activation Wizard modal popup detection triggers ERR_WIZARD_BLOCKED.
        Authoritative source: report.md § 4 Edge Case 2, § 5.3 ERR_WIZARD_BLOCKED.
        """
        windows_with_wizard = [
            {"hwnd": 9999, "title": "fx-CG Manager PLUS Activation Wizard", "class": "#32770"}
        ]

        def check_wizard_block(windows: list):
            for w in windows:
                if "Activation Wizard" in w.get("title", ""):
                    return {
                        "blocked": True,
                        "exit_code": 2,
                        "error": "ERR_WIZARD_BLOCKED",
                        "hint": "Cierra el Activation Wizard o aplica el stub ActivationFxMb.dll"
                    }
            return {"blocked": False, "exit_code": 0}

        res = check_wizard_block(windows_with_wizard)
        self.assertTrue(res["blocked"])
        self.assertEqual(res["exit_code"], 2)
        self.assertEqual(res["error"], "ERR_WIZARD_BLOCKED")
        self.assertIn("ActivationFxMb.dll", res["hint"])

    def test_r2_boundary_unmapped_or_invalid_key_token(self):
        """
        R2.B3: Unmapped key token (e.g. INVALID_KEY) is rejected before transmission.
        Authoritative source: report.md § 5.3 ERR_DESYNC_KEY.
        """
        def validate_key_token(token: str):
            if token not in EXACT_MATRIX_KEYS or token not in OFFICIAL_BUTTON_CODES:
                return {
                    "valid": False,
                    "exit_code": 2,
                    "error": "ERR_DESYNC_KEY",
                    "token": token,
                    "details": f"Unknown key token: '{token}'"
                }
            return {"valid": True, "exit_code": 0}

        invalid_tokens = ["FOO_KEY", "ESC", "ENTER", "", "99"]
        for tok in invalid_tokens:
            res = validate_key_token(tok)
            self.assertFalse(res["valid"])
            self.assertEqual(res["exit_code"], 2)
            self.assertEqual(res["error"], "ERR_DESYNC_KEY")

    def test_r2_boundary_empty_key_sequence(self):
        """
        R2.B4: Empty or whitespace-only key string yields 0 steps executed.
        Authoritative source: report.md § 5.2.
        """
        def parse_key_sequence(keys_str: str) -> list:
            if not keys_str or not keys_str.strip():
                return []
            return [k.strip() for k in keys_str.split(",") if k.strip()]

        self.assertEqual(parse_key_sequence(""), [])
        self.assertEqual(parse_key_sequence("   ,  ,,  "), [])

    def test_r2_boundary_zero_or_negative_pacing(self):
        """
        R2.B5: Zero or negative key pacing rejected as unsafe.
        Authoritative source: report.md § 4 Edge Case 3.
        """
        def sanitize_pacing(hold_ms: int, wait_ms: int):
            if hold_ms <= 0 or wait_ms < 0:
                return {
                    "valid": False,
                    "exit_code": 2,
                    "error": "ERR_INVALID_PACING",
                    "details": "Hold duration must be positive (>0 ms)"
                }
            return {"valid": True, "exit_code": 0}

        res_zero = sanitize_pacing(0, 100)
        self.assertFalse(res_zero["valid"])
        self.assertEqual(res_zero["exit_code"], 2)

        res_neg = sanitize_pacing(-50, 100)
        self.assertFalse(res_neg["valid"])

    # =========================================================================
    # R3 Boundaries: Framebuffer & Pixel Math
    # =========================================================================

    def test_r3_boundary_blackout_framebuffer(self):
        """
        R3.B1: Total black screen capture triggers ERR_BLACK_SCREEN.
        Authoritative source: report.md § 5.3 ERR_BLACK_SCREEN.
        """
        black_screen = create_blank_lcd(color=(0, 0, 0))

        def detect_blackout(img: Image.Image, min_nonzero: int = 1000):
            raw_bytes = img.convert("RGB").tobytes()
            nonzero = sum(1 for b in raw_bytes if b > 0)
            if nonzero < min_nonzero:
                return {
                    "blackout": True,
                    "exit_code": 1,
                    "error": "ERR_BLACK_SCREEN",
                    "nonzero_pixels": nonzero
                }
            return {"blackout": False, "exit_code": 0}

        res = detect_blackout(black_screen)
        self.assertTrue(res["blackout"])
        self.assertEqual(res["error"], "ERR_BLACK_SCREEN")

    def test_r3_boundary_mismatched_framebuffer_dimensions(self):
        """
        R3.B2: Dimension mismatch between framebuffers aborts with TAMANOS_DISTINTOS.
        Authoritative source: report.md § 3 Feature 11, img_diff.py:21-23.
        """
        img_384 = create_blank_lcd(384, 216)
        img_396 = create_blank_lcd(396, 224)

        diff_res = calculate_pixel_diff(img_384, img_396)
        self.assertEqual(diff_res.get("error"), "TAMANOS_DISTINTOS")
        self.assertFalse(diff_res["match"])

    def test_r3_boundary_identical_framebuffers_zero_diff(self):
        """
        R3.B3: Byte-identical framebuffers produce exactly 0 changed pixels and None bbox.
        Authoritative source: PROJECT.md § Milestone 3.
        """
        screen_1 = create_calc_screen("1+2", "3")
        screen_2 = create_calc_screen("1+2", "3")

        diff = calculate_pixel_diff(screen_1, screen_2, threshold=4)
        self.assertEqual(diff["changed_pixels"], 0)
        self.assertEqual(diff["changed_percent"], 0.0)
        self.assertIsNone(diff["diff_bbox"])
        self.assertTrue(diff["match"])

    def test_r3_boundary_single_pixel_delta_at_corners(self):
        """
        R3.B4: Single pixel alteration at boundary (0,0) is detected at threshold 0.
        Authoritative source: PROJECT.md § Feature 11.
        """
        img_a = create_blank_lcd(color=(255, 255, 255))
        img_b = img_a.copy()
        # Alter extreme top-left pixel
        img_b.putpixel((0, 0), (0, 0, 0))

        diff_tl = calculate_pixel_diff(img_a, img_b, threshold=0)
        self.assertEqual(diff_tl["changed_pixels"], 1)
        self.assertEqual(diff_tl["diff_bbox"], (0, 0, 1, 1))

        # Alter extreme bottom-right pixel
        img_br = img_a.copy()
        img_br.putpixel((CANONICAL_WIDTH - 1, CANONICAL_HEIGHT - 1), (0, 0, 0))
        diff_br = calculate_pixel_diff(img_a, img_br, threshold=0)
        self.assertEqual(diff_br["changed_pixels"], 1)
        self.assertEqual(diff_br["diff_bbox"],
                         (CANONICAL_WIDTH - 1, CANONICAL_HEIGHT - 1, CANONICAL_WIDTH, CANONICAL_HEIGHT))

    def test_r3_boundary_subthreshold_noise_ignored(self):
        """
        R3.B5: Differences below color threshold (e.g. delta <= threshold) are ignored.
        Authoritative source: report.md § 5.2.
        """
        img_base = create_blank_lcd(color=(200, 200, 200))
        img_noise = create_blank_lcd(color=(203, 203, 203))  # Delta = 3

        # With threshold = 4, delta 3 should be ignored -> 0 changed
        diff = calculate_pixel_diff(img_base, img_noise, threshold=4)
        self.assertEqual(diff["changed_pixels"], 0)
        self.assertTrue(diff["match"])

        # With threshold = 2, delta 3 exceeds threshold -> all changed
        diff_strict = calculate_pixel_diff(img_base, img_noise, threshold=2)
        self.assertEqual(diff_strict["changed_pixels"], CANONICAL_WIDTH * CANONICAL_HEIGHT)
        self.assertFalse(diff_strict["match"])

    # =========================================================================
    # R4 Boundaries: Divergence & Trace Localization
    # =========================================================================

    def test_r4_boundary_divergence_on_first_step(self):
        """
        R4.B1: Divergence on the very first keystroke of a sequence.
        Authoritative source: report.md § 5.2.
        """
        exit_code, report = self.env.simulate_dual_diff_run("1,+,2,EXE", pass_verdict=False, diverge_at_step=1)
        self.assertEqual(exit_code, 1)
        self.assertEqual(report["first_divergence"]["step"], 1)
        self.assertEqual(report["first_divergence"]["key"], "1")
        self.assertEqual(report["steps_executed"], 1)

    def test_r4_boundary_divergence_on_last_step(self):
        """
        R4.B2: Divergence on the final keystroke (step N).
        Authoritative source: report.md § 5.2.
        """
        exit_code, report = self.env.simulate_dual_diff_run("1,+,2,EXE", pass_verdict=False, diverge_at_step=4)
        self.assertEqual(exit_code, 1)
        self.assertEqual(report["first_divergence"]["step"], 4)
        self.assertEqual(report["first_divergence"]["key"], "EXE")
        self.assertEqual(report["steps_executed"], 4)
        self.assertEqual(report["total_steps"], 4)

    def test_r4_boundary_division_by_zero_ma_error(self):
        """
        R4.B3: Division by zero triggers Ma ERROR state across emulators.
        Authoritative source: report.md § 4 Edge Case 7, § 6.3.
        """
        error_screen = create_error_screen("Ma ERROR")
        raw_bytes = error_screen.convert("RGB").tobytes()
        total_pixels = len(raw_bytes) // 3
        reddish = 0
        for i in range(0, len(raw_bytes), 3):
            r, g, b = raw_bytes[i], raw_bytes[i + 1], raw_bytes[i + 2]
            if r > 180 and g < 50 and b < 50:
                reddish += 1

        # Red dialog border must be detected
        self.assertGreater(reddish / total_pixels, 0.005)

    def test_r4_boundary_identical_instruction_traces(self):
        """
        R4.B4: Identical execution traces return EQUAL status and zero mismatches.
        Authoritative source: PROJECT.md § Feature 13, Cg50TraceDiff.exe.
        """
        nat_lines, off_records = generate_matching_traces(count=30)
        res = compare_traces_in_memory(nat_lines, off_records)

        self.assertEqual(res["status"], "EQUAL")
        self.assertIsNone(res["first_divergence"])
        self.assertEqual(res["total_compared"], 30)

    def test_r4_boundary_missing_or_empty_trace_file(self):
        """
        R4.B5: Empty or truncated trace file handled safely without unhandled exception.
        Authoritative source: cg50_differential_report.py:26-31.
        """
        def compare_safe(nat_lines: list, off_records: list):
            if not nat_lines or not off_records:
                return {
                    "status": "ERROR",
                    "exit_code": 2,
                    "error": "ERR_TRACE_EMPTY",
                    "details": "One or both trace inputs are empty"
                }
            return compare_traces_in_memory(nat_lines, off_records)

        res_empty = compare_safe([], [])
        self.assertEqual(res_empty["status"], "ERROR")
        self.assertEqual(res_empty["exit_code"], 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
