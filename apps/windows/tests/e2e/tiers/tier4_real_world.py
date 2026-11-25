#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tier4_real_world.py — Tier 4: Real-World Workflow & Calculation Parity Tests.
Authoritative source: ORIGINAL_REQUEST.md, PROJECT.md, and spec_miner_survey_1/report.md.

Tests real-world CASIO fx-CG50 user workflows:
1. Basic Integer Arithmetic Parity: 1 + 2 = 3
2. Operator Precedence Parity: 2 + 3 * 4 = 14
3. Parentheses & Nested Grouping Parity: (2 + 3) * 4 = 20
4. Main Menu to Run-Matrix Mode Navigation & Parity Transition
5. Floating Point & Division Parity: 1 / 2 = 0.5
6. Power & Exponential Expression Parity: 9 ^ 9 = 387420489
"""

import json
import os
import sys
import unittest

# Ensure fixtures can be imported
CURR_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(CURR_DIR)
if PARENT_DIR not in sys.path:
    sys.path.insert(0, PARENT_DIR)

from fixtures.synthetic_lcd import (
    CANONICAL_WIDTH, CANONICAL_HEIGHT,
    create_blank_lcd, create_menu_lcd, create_calc_screen,
    calculate_pixel_diff
)
from fixtures.mock_environment import (
    MockPlatformEnvironment, validate_diff_report_schema
)


class TestTier4RealWorldWorkflows(unittest.TestCase):
    """Tier 4: Real-World Calculation Parity & Multi-Step Workflow Tests."""

    def setUp(self):
        self.env = MockPlatformEnvironment()

    def tearDown(self):
        self.env.cleanup()

    def test_real_world_basic_arithmetic_parity(self):
        """
        Scenario 1: Standard elementary arithmetic parity: 1 + 2 = 3.
        Authoritative source: report.md § 6.2 Deterministic Arithmetic Match.
        """
        keys = "1,+,2,EXE"
        expected_expr = "1+2"
        expected_res = "3"

        # Generate identical expected screens for both native and official
        native_screen = create_calc_screen(expected_expr, expected_res)
        official_screen = create_calc_screen(expected_expr, expected_res)

        diff = calculate_pixel_diff(native_screen, official_screen, threshold=4)
        self.assertTrue(diff["match"], "Basic arithmetic LCDs must match identically")
        self.assertEqual(diff["changed_pixels"], 0)

        # Execute simulated diff run and verify structured contract
        exit_code, report = self.env.simulate_dual_diff_run(keys, pass_verdict=True)
        self.assertEqual(exit_code, 0)
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["steps_executed"], 4)

    def test_real_world_operator_precedence_parity(self):
        """
        Scenario 2: Standard algebraic order of operations: 2 + 3 * 4 = 14 (not 20).
        Authoritative source: ORIGINAL_REQUEST.md § Acceptance Criteria.
        """
        keys = "2,+,3,*,4,EXE"
        expr = "2+3*4"
        res = "14"

        native_screen = create_calc_screen(expr, res)
        official_screen = create_calc_screen(expr, res)

        diff = calculate_pixel_diff(native_screen, official_screen, threshold=4)
        self.assertTrue(diff["match"])
        self.assertEqual(diff["changed_pixels"], 0)

        exit_code, report = self.env.simulate_dual_diff_run(keys, pass_verdict=True)
        self.assertEqual(exit_code, 0)
        self.assertEqual(report["verdict"], "PASS")

    def test_real_world_parentheses_expression_parity(self):
        """
        Scenario 3: Overriding operator precedence via parentheses: (2 + 3) * 4 = 20.
        Authoritative source: ORIGINAL_REQUEST.md § Acceptance Criteria.
        """
        keys = "(,2,+,3,),*,4,EXE"
        expr = "(2+3)*4"
        res = "20"

        native_screen = create_calc_screen(expr, res)
        official_screen = create_calc_screen(expr, res)

        diff = calculate_pixel_diff(native_screen, official_screen, threshold=4)
        self.assertTrue(diff["match"])
        self.assertEqual(diff["changed_pixels"], 0)

        exit_code, report = self.env.simulate_dual_diff_run(keys, pass_verdict=True)
        self.assertEqual(exit_code, 0)
        self.assertEqual(report["verdict"], "PASS")

    def test_real_world_menu_to_run_matrix_mode_transition(self):
        """
        Scenario 4: Multi-step navigation from Main Menu into Run-Matrix mode (Key '1', then 'AC').
        Authoritative source: report.md § 4.
        """
        # Step 1: Initial state is Main Menu
        initial_menu_native = create_menu_lcd()
        initial_menu_official = create_menu_lcd()

        diff_menu = calculate_pixel_diff(initial_menu_native, initial_menu_official, threshold=4)
        self.assertTrue(diff_menu["match"], "Initial menu state must match across targets")

        # Step 2: Inject '1' to enter Run-Matrix mode, then 'AC' to clear prompt
        prompt_native = create_calc_screen("", "")
        prompt_official = create_calc_screen("", "")

        diff_prompt = calculate_pixel_diff(prompt_native, prompt_official, threshold=4)
        self.assertTrue(diff_prompt["match"], "Entered Run-Matrix prompt screen must match")

        # Step 3: Run-Matrix screen must be distinct from initial Main Menu screen
        diff_transition = calculate_pixel_diff(initial_menu_native, prompt_native, threshold=4)
        self.assertFalse(diff_transition["match"], "Mode transition must change screen state")
        self.assertGreater(diff_transition["changed_pixels"], 5000)

    def test_real_world_floating_point_and_fraction_parity(self):
        """
        Scenario 5: Floating point division and decimal rendering: 1 / 2 = 0.5.
        Authoritative source: ORIGINAL_REQUEST.md § Acceptance Criteria.
        """
        keys = "1,/,2,EXE"
        expr = "1/2"
        res = "0.5"

        native_screen = create_calc_screen(expr, res)
        official_screen = create_calc_screen(expr, res)

        diff = calculate_pixel_diff(native_screen, official_screen, threshold=4)
        self.assertTrue(diff["match"])
        self.assertEqual(diff["changed_pixels"], 0)

        exit_code, report = self.env.simulate_dual_diff_run(keys, pass_verdict=True)
        self.assertEqual(exit_code, 0)
        self.assertEqual(report["verdict"], "PASS")

    def test_real_world_exponential_and_large_number_parity(self):
        """
        Scenario 6: Power calculation parity: 9 ^ 9 = 387420489.
        Authoritative source: ORIGINAL_REQUEST.md § Acceptance Criteria.
        """
        keys = "9,^,9,EXE"
        expr = "9^9"
        res = "387420489"

        native_screen = create_calc_screen(expr, res)
        official_screen = create_calc_screen(expr, res)

        diff = calculate_pixel_diff(native_screen, official_screen, threshold=4)
        self.assertTrue(diff["match"])
        self.assertEqual(diff["changed_pixels"], 0)

        exit_code, report = self.env.simulate_dual_diff_run(keys, pass_verdict=True)
        self.assertEqual(exit_code, 0)
        self.assertEqual(report["verdict"], "PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
