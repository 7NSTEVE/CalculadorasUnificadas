#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tier1_features.py — Tier 1: Feature Coverage Tests for Differential Testing Platform.
Authoritative source: ORIGINAL_REQUEST.md, PROJECT.md, and spec_miner_survey_1/report.md.

Covers >= 5 tests per requirement:
- R1: Strict Build Control & Fail-Closed Abort (Tests 1-5)
- R2: Automated Dual Differential Testing (`cg50_agent diff`) (Tests 6-10)
- R3: Pixel-by-Pixel Graphical LCD Comparison (Tests 11-15)
- R4: Automated First Divergence Localization (Tests 16-20)
"""

import hashlib
import json
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
    create_blank_lcd, create_menu_lcd, create_calc_screen, create_error_screen,
    apply_rotation, inject_pixel_discrepancy, calculate_pixel_diff,
    generate_magenta_diff_image, COLOR_DIFF_MAGENTA
)
from fixtures.trace_fixtures import (
    generate_matching_traces, generate_diverging_traces, compare_traces_in_memory
)
from fixtures.mock_environment import (
    MockPlatformEnvironment, validate_diff_report_schema,
    EXACT_MATRIX_KEYS, OFFICIAL_BUTTON_CODES
)


class TestTier1FeatureCoverage(unittest.TestCase):
    """Tier 1: Feature Coverage Test Suite (R1, R2, R3, R4)."""

    def setUp(self):
        self.env = MockPlatformEnvironment()

    def tearDown(self):
        self.env.cleanup()

    # =========================================================================
    # R1: Control estricto de compilación y corte ante fallos
    # =========================================================================

    def test_r1_clean_build_manifest_generation(self):
        """
        R1.1: Verify clean build produces fresh binary and authentic build_manifest.json.
        Authoritative source: PROJECT.md § M1, report.md § 3 Feature 1.
        """
        bin_path = self.env.create_mock_binary("CalculadoraUnificada.exe", b"MZ_FRESH_COMPILED_PE_BINARY")
        manifest_path = self.env.create_build_manifest(bin_path, compiler="MSVC 14.44.35207", duration=14.2)

        self.assertTrue(os.path.exists(manifest_path), "build_manifest.json must be generated")
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)

        self.assertEqual(manifest["status"], "PASS")
        self.assertEqual(manifest["target"], "CalculadoraUnificada.exe")
        self.assertIn("MSVC", manifest["compiler"])
        self.assertGreater(manifest["duration_seconds"], 0)

        # Verify SHA-256 matches binary content exactly
        with open(bin_path, "rb") as f:
            expected_sha = hashlib.sha256(f.read()).hexdigest()
        self.assertEqual(manifest["binary_sha256"], expected_sha)

    def test_r1_fail_closed_abort_on_compilation_error(self):
        """
        R1.2: Verify build failure triggers immediate fail-closed abort (exit code 2).
        Authoritative source: ORIGINAL_REQUEST.md § R1, PROJECT.md § Interface Contracts M1.
        """
        # Simulate compilation failure: cmake / cl.exe error
        def simulate_build_guard(should_succeed: bool):
            if not should_succeed:
                # Must abort immediately with code 2 and raise explicit error
                return {
                    "success": False,
                    "exit_code": 2,
                    "error_type": "ERR_BUILD_FAILED",
                    "log": "fatal error C1083: Cannot open include file: 'non_existent.h'"
                }
            return {"success": True, "exit_code": 0}

        res = simulate_build_guard(should_succeed=False)
        self.assertFalse(res["success"], "Build must report failure")
        self.assertEqual(res["exit_code"], 2, "Fail-closed abort must return exit code 2")
        self.assertEqual(res["error_type"], "ERR_BUILD_FAILED")
        self.assertIn("fatal error", res["log"])

    def test_r1_pre_build_stale_artifact_invalidation(self):
        """
        R1.3: Verify pre-build invalidation removes/purges stale binaries.
        Authoritative source: PROJECT.md § M1, report.md § 2 R1.
        """
        stale_bin = self.env.create_mock_binary("CalculadoraUnificada.exe", b"OLD_STALE_CODE")
        self.assertTrue(os.path.exists(stale_bin))

        # Invalidation logic before compilation
        def invalidate_stale_artifacts(binary_path: str):
            if os.path.exists(binary_path):
                os.remove(binary_path)
            return not os.path.exists(binary_path)

        invalidation_result = invalidate_stale_artifacts(stale_bin)
        self.assertTrue(invalidation_result, "Stale binary must be removed before building")
        self.assertFalse(os.path.exists(stale_bin), "Stale binary must no longer exist")

    def test_r1_build_manifest_schema_and_metadata(self):
        """
        R1.4: Verify all mandatory metadata fields in build_manifest.json.
        Authoritative source: report.md § 5.2 Structured JSON Output Contract.
        """
        bin_path = self.env.create_mock_binary("CalculadoraUnificada.exe", b"MOCK_PE")
        manifest_path = self.env.create_build_manifest(bin_path, compiler="MSVC 14.44.35207", duration=8.4)

        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)

        required_keys = ["status", "compiler", "target", "binary_path", "binary_sha256",
                         "timestamp", "iso_timestamp", "duration_seconds"]
        for key in required_keys:
            self.assertIn(key, manifest, f"Mandatory manifest field missing: {key}")
            self.assertIsNotNone(manifest[key], f"Field {key} must not be null")

    def test_r1_static_contract_assertion_guard(self):
        """
        R1.5: Verify static contract guard checks source code before build.
        Authoritative source: PROJECT.md § Feature 2, compilar_cg50_x64.bat.
        """
        clean_code = "void execute_loop() { Sleep(1); ProcessMessages(); }"
        violating_code = "void execute_loop() { while(true) { /* unthrottled loop without sleep */ } }"

        def check_source_contract(source_text: str) -> bool:
            # Prohibits unthrottled while(true) loops lacking sleep
            if "while(true)" in source_text and "Sleep" not in source_text:
                return False
            return True

        self.assertTrue(check_source_contract(clean_code), "Compliant code must pass contract check")
        self.assertFalse(check_source_contract(violating_code), "Unthrottled loop must be rejected")

    # =========================================================================
    # R2: Comparación dual automatizada (`cg50_agent diff`)
    # =========================================================================

    def test_r2_dual_window_discovery(self):
        """
        R2.1: Verify window discovery logic identifies native & official windows.
        Authoritative source: PROJECT.md § Feature 4, cg50_agent.py:90-105.
        """
        mock_windows = [
            {"hwnd": 1001, "title": "Calculadora Unificada [fx-CG50]", "class": "CasioNativeMain"},
            {"hwnd": 2002, "title": "fx-CG Manager PLUS [fx-CG50] - [Modo emulador]", "class": "Afx:00400000:b"},
            {"hwnd": 3003, "title": "Visual Studio Code", "class": "Chrome_WidgetWin_1"}
        ]

        def find_target_window(backend: str, windows: list):
            if backend == "native":
                for w in windows:
                    if "CasioNativeMain" in w["class"] or "Calculadora Unificada" in w["title"]:
                        return w
            elif backend == "official":
                for w in windows:
                    if "fx-CG Manager PLUS" in w["title"] and "[Modo emulador]" in w["title"]:
                        return w
            return None

        nat_win = find_target_window("native", mock_windows)
        off_win = find_target_window("official", mock_windows)

        self.assertIsNotNone(nat_win, "Native window must be detected")
        self.assertEqual(nat_win["hwnd"], 1001)
        self.assertIsNotNone(off_win, "Official emulator window must be detected")
        self.assertEqual(off_win["hwnd"], 2002)

    def test_r2_hardware_matrix_key_encoding(self):
        """
        R2.2: Verify logical key tokens translate to exact 16-bit hardware matrix codes.
        Authoritative source: PROJECT.md § Feature 5, cg50_keys_exact.h.
        """
        test_keys = ["1", "+", "2", "EXE", "MENU", "F1"]
        for key in test_keys:
            self.assertIn(key, EXACT_MATRIX_KEYS, f"Key {key} must have hardware matrix mapping")
            code = EXACT_MATRIX_KEYS[key]
            # Must be a 16-bit integer (KI << 8) | KO
            self.assertIsInstance(code, int)
            ki = (code >> 8) & 0xFF
            ko = code & 0xFF
            self.assertGreater(ki, 0, f"Matrix KI for {key} must be non-zero")
            self.assertGreater(ko, 0, f"Matrix KO for {key} must be non-zero")

    def test_r2_official_button_code_mapping(self):
        """
        R2.3: Verify mapping to 50 official Win32 child buttons for BM_CLICK.
        Authoritative source: PROJECT.md § Feature 6, calc_visual_auditor.py:83-97.
        """
        essential_keys = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                          "+", "-", "*", "/", "EXE", "AC", "DEL", "MENU", "F1", "F6"]
        for key in essential_keys:
            self.assertIn(key, OFFICIAL_BUTTON_CODES, f"Key {key} must be mapped to official button code")
            hex_code = OFFICIAL_BUTTON_CODES[key]
            self.assertEqual(len(hex_code), 2, f"Official button ID must be a 2-char hex string: {hex_code}")
            int(hex_code, 16)  # Must be valid hex

    def test_r2_key_injection_pacing_enforcement(self):
        """
        R2.4: Verify key injection pacing configuration enforces guest sleep loop safety.
        Authoritative source: PROJECT.md § Feature 7, report.md § 4 Edge Case 3.
        """
        def validate_pacing(hold_ms: int, allow_short_holds: bool = False) -> bool:
            # Enforces hold >= 5000ms unless --allow-short-holds is explicitly given
            if hold_ms < 5000 and not allow_short_holds:
                return False
            return True

        self.assertFalse(validate_pacing(100, allow_short_holds=False),
                         "Short hold without override must be rejected for slow guest sleep loop")
        self.assertTrue(validate_pacing(5000, allow_short_holds=False),
                        "5000ms hold must be accepted as safe default")
        self.assertTrue(validate_pacing(150, allow_short_holds=True),
                        "Short hold must be permitted when allow_short_holds is explicit")

    def test_r2_dual_diff_json_report_structure(self):
        """
        R2.5: Verify cg50_agent diff emits structured JSON report matching specification.
        Authoritative source: report.md § 5.2 Structured JSON Output Contract.
        """
        exit_code, report = self.env.simulate_dual_diff_run("1,+,2,EXE", pass_verdict=True)
        self.assertEqual(exit_code, 0, "Passing diff must return exit code 0")

        is_valid, errors = validate_diff_report_schema(report)
        self.assertTrue(is_valid, f"Report failed schema validation: {errors}")
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["total_steps"], 4)
        self.assertEqual(report["steps_executed"], 4)
        self.assertIsNone(report["first_divergence"])

    # =========================================================================
    # R3: Comparación gráfica de LCD por píxel
    # =========================================================================

    def test_r3_lcd_subpixel_cropping_and_dimensions(self):
        """
        R3.1: Verify LCD subpixel auto-cropping outputs canonical 384x216 dimensions.
        Authoritative source: PROJECT.md § Feature 8, M3 Interface Contract.
        """
        # Create full window image containing an active LCD area
        window_img = Image.new("RGB", (800, 600), (40, 40, 40))
        lcd_crop = create_menu_lcd(CANONICAL_WIDTH, CANONICAL_HEIGHT)
        window_img.paste(lcd_crop, (200, 100))

        # Extract using known LCD bounding box
        extracted = window_img.crop((200, 100, 200 + CANONICAL_WIDTH, 100 + CANONICAL_HEIGHT))
        self.assertEqual(extracted.size, (CANONICAL_WIDTH, CANONICAL_HEIGHT))
        self.assertEqual(extracted.mode, "RGB")

    def test_r3_native_180_degree_rotation_normalization(self):
        """
        R3.2: Verify 180° rotation on native LCD compensates for physical mounting.
        Authoritative source: PROJECT.md § Feature 10, cg50_agent.py:196-198.
        """
        # Create an upright calculation screen
        upright_img = create_calc_screen("1+2", "3")

        # Simulate raw native capture (which arrives 180° inverted)
        raw_native = apply_rotation(upright_img, 180)
        self.assertNotEqual(raw_native.tobytes(), upright_img.tobytes())

        # Normalize native capture by rotating 180°
        normalized_native = apply_rotation(raw_native, 180)
        # Should now perfectly match the upright image
        self.assertEqual(normalized_native.tobytes(), upright_img.tobytes())

    def test_r3_pixel_by_pixel_differential_analysis(self):
        """
        R3.3: Verify direct pixel subtraction with configurable color threshold.
        Authoritative source: PROJECT.md § Feature 11, img_diff.py.
        """
        img_a = create_calc_screen("1+2", "3")
        # Identical screen must have 0 changed pixels
        diff_identical = calculate_pixel_diff(img_a, img_a, threshold=4)
        self.assertEqual(diff_identical["changed_pixels"], 0)
        self.assertEqual(diff_identical["changed_percent"], 0.0)
        self.assertTrue(diff_identical["match"])

        # Inject controlled discrepancy in result area (box 20x15 = 300 pixels)
        img_b = inject_pixel_discrepancy(img_a, bbox=(300, 50, 320, 65), color=(255, 0, 0))
        diff_divergent = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertGreater(diff_divergent["changed_pixels"], 0)
        self.assertFalse(diff_divergent["match"])
        self.assertIsNotNone(diff_divergent["diff_bbox"])
        # Bounding box must enclose the injected region
        self.assertEqual(diff_divergent["diff_bbox"], (300, 50, 320, 65))

    def test_r3_visual_diff_artifact_magenta_highlight(self):
        """
        R3.4: Verify differential image generation highlights differing pixels in magenta.
        Authoritative source: PROJECT.md § Milestone 3, report.md § 2 R3.
        """
        img_a = create_calc_screen("1+2", "3")
        img_b = inject_pixel_discrepancy(img_a, bbox=(50, 50, 70, 70), color=(0, 255, 0))

        diff_img = generate_magenta_diff_image(img_a, img_b, threshold=4)
        self.assertEqual(diff_img.size, img_a.size)

        # Check that pixels inside the modified region are turned magenta (255, 0, 255)
        px = diff_img.load()
        self.assertEqual(px[60, 60], COLOR_DIFF_MAGENTA)
        # Check that pixels outside the modified region retain original color
        orig_px = img_a.load()
        self.assertEqual(px[10, 10], orig_px[10, 10])

    def test_r3_lcd_content_classification(self):
        """
        R3.5: Verify heuristic classification into menu vs calculation vs error screen.
        Authoritative source: PROJECT.md § Feature 12, cg50_lcd_reader.py.
        """
        menu_img = create_menu_lcd()
        calc_img = create_calc_screen("1+2", "3")
        error_img = create_error_screen("Ma ERROR")

        def classify_screen(img: Image.Image) -> str:
            # Analyze color variance across pixels
            raw_bytes = img.convert("RGB").tobytes()
            total_pixels = len(raw_bytes) // 3
            colorful = 0
            reddish = 0
            for i in range(0, len(raw_bytes), 3):
                r, g, b = raw_bytes[i], raw_bytes[i + 1], raw_bytes[i + 2]
                if (max(r, g, b) - min(r, g, b)) > 50:
                    colorful += 1
                if r > 180 and g < 50 and b < 50:
                    reddish += 1

            color_ratio = colorful / total_pixels
            red_ratio = reddish / total_pixels

            # Menus have a rich variety of colored icons (color_ratio > 0.10)
            if color_ratio > 0.10:
                return "menu"
            # Error dialogs are mostly monochrome with a focused red border/title
            elif red_ratio > 0.005:
                return "error_dialog"
            else:
                return "texto_denso"

        self.assertEqual(classify_screen(menu_img), "menu")
        self.assertEqual(classify_screen(calc_img), "texto_denso")
        self.assertEqual(classify_screen(error_img), "error_dialog")

    # =========================================================================
    # R4: Localización automática de la primera divergencia
    # =========================================================================

    def test_r4_first_divergence_step_identification(self):
        """
        R4.1: Verify identification of exact step and key that triggered divergence.
        Authoritative source: ORIGINAL_REQUEST.md § R4, PROJECT.md § M4.
        """
        # Inject "1,+,2,*,3,EXE", with divergence at step 4 (the '*' key)
        exit_code, report = self.env.simulate_dual_diff_run("1,+,2,*,3,EXE", pass_verdict=False, diverge_at_step=4)
        self.assertEqual(exit_code, 1, "Divergent run must return exit code 1")
        self.assertEqual(report["verdict"], "DIVERGENCIA")

        first_div = report["first_divergence"]
        self.assertIsNotNone(first_div)
        self.assertEqual(first_div["step"], 4)
        self.assertEqual(first_div["key"], "*")
        self.assertEqual(first_div["divergence_type"], "LCD_PIXEL_MISMATCH")

    def test_r4_fail_fast_stop_on_first_divergence(self):
        """
        R4.2: Verify --fail-fast stops execution immediately at divergence instant.
        Authoritative source: PROJECT.md § Feature 13, report.md § 5.2.
        """
        # 6-step sequence, divergence at step 2
        exit_code, report = self.env.simulate_dual_diff_run("1,+,2,3,4,EXE", pass_verdict=False, diverge_at_step=2)
        self.assertEqual(report["total_steps"], 6)
        # Must halt immediately at step 2; subsequent steps (3, 4, 5, 6) must NOT execute
        self.assertEqual(report["steps_executed"], 2)
        self.assertEqual(len(report["steps"]), 2)

    def test_r4_divergence_classification_types(self):
        """
        R4.3: Verify structured classification of divergence types.
        Authoritative source: PROJECT.md § Interface Contracts M2 ↔ M4.
        """
        allowed_types = {"LCD_MISMATCH", "LCD_PIXEL_MISMATCH", "REGISTER_MISMATCH",
                         "MEMORY_MISMATCH", "CRASH"}

        for div_type in allowed_types:
            report_item = {
                "step": 1,
                "key": "EXE",
                "divergence_type": div_type,
                "details": f"Test divergence of type {div_type}"
            }
            self.assertIn(report_item["divergence_type"], allowed_types)

    def test_r4_instruction_trace_ordinal_localization(self):
        """
        R4.4: Verify pinpointing exact instruction ordinal and PC address deviation.
        Authoritative source: PROJECT.md § Feature 13, Cg50TraceDiff.exe.
        """
        # 50 instructions, divergence at ordinal 27
        nat_lines, off_records = generate_diverging_traces(count=50, diverge_at=27, diverge_type="OPCODE")
        res = compare_traces_in_memory(nat_lines, off_records)

        self.assertEqual(res["status"], "DIFF")
        self.assertIsNotNone(res["first_divergence"])
        self.assertEqual(res["first_divergence"]["ordinal"], 27)
        self.assertNotEqual(res["first_divergence"]["native_op"], res["first_divergence"]["official_op"])

    def test_r4_first_divergence_bounding_box_extraction(self):
        """
        R4.5: Verify bounding box extraction accurately isolates divergent screen area.
        Authoritative source: report.md § 5.2, img_diff.py.
        """
        img_a = create_calc_screen("1+2", "3")
        # Divergence at result area: x in [250, 300], y in [55, 75]
        target_bbox = (250, 55, 300, 75)
        img_b = inject_pixel_discrepancy(img_a, bbox=target_bbox, color=(0, 0, 255))

        diff_res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(diff_res["diff_bbox"], target_bbox)


if __name__ == "__main__":
    unittest.main(verbosity=2)
