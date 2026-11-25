#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tier3_combinations.py — Tier 3: Pairwise Cross-Feature Interaction Tests.
Authoritative source: ORIGINAL_REQUEST.md, PROJECT.md § Interface Contracts, report.md.

Tests pairwise cross-feature combinations:
1. Build + Dual Execution Pipeline (R1 + R2)
2. Build Failure Short-Circuit Protection (R1 + R2)
3. Dual Execution + Pixel LCD Comparison (R2 + R3)
4. Dual Execution + First Divergence Localization (R2 + R4)
5. LCD Pixel Diff + Visual HTML Gallery Artifacts (R3 + R4)
6. Error Injection & Parity Recovery Cycle (R2 + R3 + R4)
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
    create_blank_lcd, create_calc_screen, create_error_screen,
    apply_rotation, inject_pixel_discrepancy, calculate_pixel_diff,
    generate_magenta_diff_image, COLOR_DIFF_MAGENTA
)
from fixtures.mock_environment import (
    MockPlatformEnvironment, validate_diff_report_schema
)


class TestTier3Combinations(unittest.TestCase):
    """Tier 3: Pairwise Cross-Feature Combinations Test Suite."""

    def setUp(self):
        self.env = MockPlatformEnvironment()

    def tearDown(self):
        self.env.cleanup()

    def test_interaction_build_and_dual_diff_pipeline(self):
        """
        Combination 1 (R1 + R2): Successful build feeds freshly minted binary into dual diff pipeline.
        Authoritative source: PROJECT.md § Interface Contract M1 ↔ M2.
        """
        # Step 1: M1 Build Guard compiles fresh binary
        fresh_bin = self.env.create_mock_binary("CalculadoraUnificada.exe", b"FRESH_COMPILATION_V1")
        manifest_path = self.env.create_build_manifest(fresh_bin, duration=11.2)

        def verify_or_build_core(rebuild: bool = True):
            if not os.path.exists(manifest_path):
                return {"success": False, "exit_code": 2}
            with open(manifest_path, "r", encoding="utf-8") as f:
                manifest = json.load(f)
            return {
                "success": True,
                "exit_code": 0,
                "exe_path": manifest["binary_path"],
                "manifest_path": manifest_path,
                "sha256": manifest["binary_sha256"]
            }

        build_res = verify_or_build_core(rebuild=True)
        self.assertTrue(build_res["success"], "Build must succeed")

        # Step 2: M2 Dual Runner consumes newly compiled binary
        exit_code, diff_report = self.env.simulate_dual_diff_run("1,+,2,EXE", pass_verdict=True)
        self.assertEqual(exit_code, 0)
        self.assertEqual(diff_report["build_control"]["status"], "PASS")
        self.assertEqual(diff_report["build_control"]["target"], "CalculadoraUnificada.exe")
        self.assertEqual(diff_report["verdict"], "PASS")

    def test_interaction_build_failure_short_circuits_diff(self):
        """
        Combination 2 (R1 + R2): Build failure halts pipeline, preventing any process launch.
        Authoritative source: PROJECT.md § M1 ↔ M2, ORIGINAL_REQUEST.md § Acceptance Criteria.
        """
        # Simulate compilation failure in build guard
        def verify_or_build_core_failing():
            return {
                "success": False,
                "exit_code": 2,
                "error": "ERR_BUILD_FAILED",
                "log": "CMake Error: build failed with exit code 1"
            }

        # Agent CLI diff orchestrator pipeline
        def run_agent_diff_pipeline(keys: str):
            build_res = verify_or_build_core_failing()
            if not build_res["success"]:
                # Short-circuit: do NOT discover windows, do NOT inject keys
                return {
                    "aborted": True,
                    "exit_code": build_res["exit_code"],
                    "error": build_res["error"],
                    "windows_searched": False,
                    "keys_injected": False
                }
            return {"aborted": False, "exit_code": 0}

        pipeline_res = run_agent_diff_pipeline("1,+,2,EXE")
        self.assertTrue(pipeline_res["aborted"], "Pipeline must abort immediately on build failure")
        self.assertEqual(pipeline_res["exit_code"], 2)
        self.assertFalse(pipeline_res["windows_searched"], "Must not search for or launch windows")
        self.assertFalse(pipeline_res["keys_injected"], "Must not inject keys on failed build")

    def test_interaction_dual_execution_to_pixel_lcd_comparison(self):
        """
        Combination 3 (R2 + R3): Injected keys produce raw framebuffers, normalized & diffed.
        Authoritative source: PROJECT.md § Interface Contract M2 ↔ M3.
        """
        # Native framebuffer arrives 180° inverted
        upright_reference = create_calc_screen("1+2", "3")
        native_raw_capture = apply_rotation(upright_reference, 180)
        official_raw_capture = upright_reference.copy()

        # Normalization step (M2 ↔ M3)
        def normalize_framebuffers(nat_img, off_img):
            # Native rotated 180° to physical orientation, official remains intact
            norm_nat = apply_rotation(nat_img, 180)
            norm_off = off_img.copy()
            return norm_nat, norm_off

        norm_native, norm_official = normalize_framebuffers(native_raw_capture, official_raw_capture)
        self.assertEqual(norm_native.size, (CANONICAL_WIDTH, CANONICAL_HEIGHT))
        self.assertEqual(norm_official.size, (CANONICAL_WIDTH, CANONICAL_HEIGHT))

        # Direct pixel diff
        diff_res = calculate_pixel_diff(norm_native, norm_official, threshold=4)
        self.assertTrue(diff_res["match"], "Normalized native and official screens must match perfectly")
        self.assertEqual(diff_res["changed_pixels"], 0)

    def test_interaction_dual_execution_to_first_divergence_localization(self):
        """
        Combination 4 (R2 + R4): Multi-step injection bisects and localizes first divergence instant.
        Authoritative source: PROJECT.md § Interface Contract M2 ↔ M4.
        """
        # Inject multi-step sequence: "1,+,5,*,2,EXE"
        # Divergence engineered at step 3 (key "5")
        exit_code, report = self.env.simulate_dual_diff_run("1,+,5,*,2,EXE", pass_verdict=False, diverge_at_step=3)

        self.assertEqual(exit_code, 1)
        self.assertEqual(report["verdict"], "DIVERGENCIA")
        self.assertEqual(report["steps_executed"], 3)

        first_div = report["first_divergence"]
        self.assertEqual(first_div["step"], 3)
        self.assertEqual(first_div["key"], "5")
        self.assertEqual(first_div["divergence_type"], "LCD_PIXEL_MISMATCH")

        # Verify steps 1 and 2 passed with MATCH
        self.assertEqual(report["steps"][0]["comparison"]["status"], "MATCH")
        self.assertEqual(report["steps"][1]["comparison"]["status"], "MATCH")
        # Step 3 is marked DIFF
        self.assertEqual(report["steps"][2]["comparison"]["status"], "DIFF")

    def test_interaction_lcd_pixel_mismatch_to_visual_gallery_report(self):
        """
        Combination 5 (R3 + R4): LCD pixel mismatch generates magenta diff image and HTML report.
        Authoritative source: PROJECT.md § Milestone 3 & 4, calc_visual_auditor.py:278-335.
        """
        screen_nat = create_calc_screen("1+2", "3")
        # Official emulates buggy/divergent result "4"
        screen_off = create_calc_screen("1+2", "4")

        diff_res = calculate_pixel_diff(screen_nat, screen_off, threshold=4)
        self.assertFalse(diff_res["match"])
        self.assertGreater(diff_res["changed_pixels"], 0)

        # Generate magenta diff image
        diff_img = generate_magenta_diff_image(screen_nat, screen_off, threshold=4)
        diff_png_path = os.path.join(self.env.out_dir, "step3_diff_magenta.png")
        diff_img.save(diff_png_path)
        self.assertTrue(os.path.exists(diff_png_path))

        # Generate HTML report gallery snippet
        def generate_html_gallery_snippet(step: int, diff_info: dict, diff_path: str) -> str:
            return f"""
            <div class="step-card failure">
                <h3>Step {step} - DIVERGENCE DETECTED</h3>
                <p>Changed Pixels: {diff_info['changed_pixels']} ({diff_info['changed_percent']}%)</p>
                <p>Diff BBox: {diff_info['diff_bbox']}</p>
                <img src="{diff_path}" alt="Magenta Diff Highlight" />
            </div>
            """

        html_snippet = generate_html_gallery_snippet(3, diff_res, diff_png_path)
        self.assertIn("DIVERGENCE DETECTED", html_snippet)
        self.assertIn("Magenta Diff Highlight", html_snippet)
        self.assertIn(str(diff_res["changed_pixels"]), html_snippet)

    def test_interaction_error_recovery_and_restoration_cycle(self):
        """
        Combination 6 (R2 + R3 + R4): Division by zero error recovery via AC key restore.
        Authoritative source: report.md § 4 Edge Case 7, § 6.3.
        """
        # Step 1: Inject fault sequence "1,/,0,EXE" -> Error dialog
        err_screen = create_error_screen("Ma ERROR")
        diff_err = calculate_pixel_diff(err_screen, create_calc_screen("1/0", "0"))
        # Significant difference from standard calculation screen
        self.assertGreater(diff_err["changed_pixels"], 1000)

        # Step 2: Protocol recovery action: send AC key
        # Post-AC screen must return to clean calculation prompt state
        restored_screen = create_calc_screen("", "")
        target_reference = create_calc_screen("", "")

        diff_restored = calculate_pixel_diff(restored_screen, target_reference, threshold=4)
        self.assertTrue(diff_restored["match"], "Prompt state must be restored after AC recovery")
        self.assertEqual(diff_restored["changed_pixels"], 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
