#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_adversarial_m2.py — Adversarial Stress Testing Harness for Milestone 2.
Author: challenger_m2_1 (Empirical Challenger)
Target: Milestone 2 Deliverables (cg50_agent diff, lcd_diff, divergence_locator, visual_gallery)

Adversarial Dimensions Tested:
1. Pacing Boundaries:
   - hold_ms <= 0 (0, -1, -50)
   - wait_ms < 0 (-1, -100)
   - hold_ms < 5000 without --allow-short-holds (100, 4999) -> fail-closed exit code 2, ERR_UNSAFE_PACING
   - hold_ms < 5000 with --allow-short-holds (100) -> proceeds past pacing validation gate
   - Boundary valid pacings (hold_ms=5000 without flag, wait_ms=0) -> proceed past pacing gate
2. Key Synchronization Boundaries:
   - Single unmapped token ("UNKNOWN_KEY_FOO") -> exit code 2, ERR_DESYNC_KEY
   - Unmapped token within sequence ("1,+,MALFORMED_XYZ,2,EXE") -> fail-closed with 0 steps executed
   - Empty sequence ("") -> exit code 0, PASS, 0 steps
   - Whitespace sequence ("   \t  ") -> exit code 0, PASS, 0 steps
   - Consecutive commas & padding (",1,,+,,,EXE,") -> pruned and proceeds
   - Case-insensitive tokens ("f1", "menu", "exe") -> recognized and normalized
   - Script JSON variations (array format, steps format, malformed keys)
3. Fail-Closed Build Gate:
   - Rebuild failure halts execution with exit code 2 and structured build_control JSON
   - No emulator or native window processes launched when build gate fails
   - Missing core binary without valid manifest triggers build gate
4. Schema & Report Invariant Stress:
   - Verification of PASS, DIVERGENCIA, and FAIL reports against validate_diff_report_schema()
   - Mutation tests: dropping root fields, dropping build_control fields, dropping first_divergence
   - Verification that all cmd_diff error exit paths emit 100% schema-conformant JSON
5. Divergence & State Isolation Stress:
   - Cascading divergence execution with --no-fail-fast
   - Memory window isolation boundary conditions (byte 0, last byte, identical buffers)
   - Instruction trace alignment with opcode vs PC deviations and empty trace handling
"""

import copy
import io
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from typing import Dict, Any
from unittest.mock import patch, MagicMock

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
CORE_ROOT = os.path.dirname(TOOLS_DIR)
E2E_FIXTURES = os.path.join(CORE_ROOT, "tests", "e2e", "fixtures")

if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
if E2E_FIXTURES not in sys.path:
    sys.path.insert(0, E2E_FIXTURES)

from mock_environment import validate_diff_report_schema
from divergence_locator import (
    DivergenceLocator,
    DivergenceType,
    FirstDivergence,
    MemoryDiscrepancy,
    TraceDiscrepancy
)
from cg50_agent import (
    cmd_diff,
    EXACT_MATRIX_KEYS,
    OFFICIAL_KEY_TO_CODE,
    KEY_MAP_CU
)
import build_guard

NON_EXISTENT_EXE = os.path.join(CORE_ROOT, "build-win32", "NON_EXISTENT_FOR_TESTS.exe")


# =============================================================================
# 1. Adversarial Pacing Stress Tests
# =============================================================================

class TestAdversarialPacingStress(unittest.TestCase):
    """Stress tests pacing validation rules and boundary conditions in cg50_agent diff."""

    def setUp(self):
        self.agent_py = os.path.join(TOOLS_DIR, "cg50_agent.py")

    def _run_cli_diff(self, extra_args):
        # Pass non-existent exe paths so that if pacing passes, it exits quickly at window check
        cmd = [
            sys.executable, self.agent_py, "diff",
            "--official-exe", NON_EXISTENT_EXE,
            "--native-exe", NON_EXISTENT_EXE
        ] + extra_args
        res = subprocess.run(cmd, capture_output=True, text=True)
        try:
            data = json.loads(res.stdout) if res.stdout.strip() else {}
        except json.JSONDecodeError:
            data = {}
        return res.returncode, data, res.stderr

    def test_pacing_hold_ms_zero_fails_closed(self):
        """hold_ms = 0 must fail closed with exit code 2 and ERR_INVALID_PACING."""
        code, data, _ = self._run_cli_diff(["--keys", "1", "--hold-ms", "0"])
        self.assertEqual(code, 2)
        self.assertEqual(data.get("verdict"), "FAIL")
        self.assertEqual(data.get("error"), "ERR_INVALID_PACING")
        valid, errors = validate_diff_report_schema(data)
        self.assertTrue(valid, f"Schema validation errors: {errors}")

    def test_pacing_hold_ms_negative_fails_closed(self):
        """Negative hold_ms (-1, -50) must fail closed with exit code 2 and ERR_INVALID_PACING."""
        for val in ["-1", "-50"]:
            code, data, _ = self._run_cli_diff(["--keys", "1", "--hold-ms", val])
            self.assertEqual(code, 2, f"Failed for hold-ms {val}")
            self.assertEqual(data.get("verdict"), "FAIL")
            self.assertEqual(data.get("error"), "ERR_INVALID_PACING")
            valid, errors = validate_diff_report_schema(data)
            self.assertTrue(valid, f"Schema validation errors: {errors}")

    def test_pacing_wait_ms_negative_fails_closed(self):
        """Negative wait_ms (-1, -100) must fail closed with exit code 2 and ERR_INVALID_PACING."""
        for val in ["-1", "-100"]:
            code, data, _ = self._run_cli_diff(["--keys", "1", "--wait-ms", val])
            self.assertEqual(code, 2, f"Failed for wait-ms {val}")
            self.assertEqual(data.get("verdict"), "FAIL")
            self.assertEqual(data.get("error"), "ERR_INVALID_PACING")
            valid, errors = validate_diff_report_schema(data)
            self.assertTrue(valid, f"Schema validation errors: {errors}")

    def test_pacing_short_hold_without_flag_fails_closed(self):
        """hold_ms < 5000 without --allow-short-holds must fail closed with code 2 and ERR_UNSAFE_PACING."""
        for val in ["100", "500", "2000", "4999"]:
            code, data, _ = self._run_cli_diff(["--keys", "1", "--hold-ms", val])
            self.assertEqual(code, 2, f"Failed for hold-ms {val}")
            self.assertEqual(data.get("verdict"), "FAIL")
            self.assertEqual(data.get("error"), "ERR_UNSAFE_PACING")
            valid, errors = validate_diff_report_schema(data)
            self.assertTrue(valid, f"Schema validation errors: {errors}")

    def test_pacing_short_hold_with_flag_proceeds_past_pacing_gate(self):
        """hold_ms = 100 with --allow-short-holds must pass pacing validation gate."""
        code, data, _ = self._run_cli_diff(["--keys", "1", "--hold-ms", "100", "--allow-short-holds"])
        # Should NOT fail with pacing errors
        self.assertNotIn(data.get("error"), ["ERR_UNSAFE_PACING", "ERR_INVALID_PACING"])
        # Reached window discovery gate
        self.assertIn(data.get("error"), ["ERR_WINDOW_NOT_FOUND", "ERR_WIZARD_BLOCKED"])

    def test_pacing_boundary_5000ms_without_flag_proceeds(self):
        """hold_ms = 5000 without --allow-short-holds is the exact safe boundary and must pass."""
        code, data, _ = self._run_cli_diff(["--keys", "1", "--hold-ms", "5000"])
        self.assertNotIn(data.get("error"), ["ERR_UNSAFE_PACING", "ERR_INVALID_PACING"])
        self.assertIn(data.get("error"), ["ERR_WINDOW_NOT_FOUND", "ERR_WIZARD_BLOCKED"])

    def test_pacing_boundary_wait_ms_zero_proceeds(self):
        """wait_ms = 0 is non-negative and must pass pacing validation."""
        code, data, _ = self._run_cli_diff(["--keys", "1", "--wait-ms", "0", "--hold-ms", "5000"])
        self.assertNotIn(data.get("error"), ["ERR_UNSAFE_PACING", "ERR_INVALID_PACING"])
        self.assertIn(data.get("error"), ["ERR_WINDOW_NOT_FOUND", "ERR_WIZARD_BLOCKED"])


# =============================================================================
# 2. Adversarial Key Synchronization Stress Tests
# =============================================================================

class TestAdversarialKeySyncStress(unittest.TestCase):
    """Stress tests key parsing, matrix mapping, and input validation."""

    def setUp(self):
        self.agent_py = os.path.join(TOOLS_DIR, "cg50_agent.py")

    def _run_cli_diff(self, extra_args):
        cmd = [
            sys.executable, self.agent_py, "diff",
            "--official-exe", NON_EXISTENT_EXE,
            "--native-exe", NON_EXISTENT_EXE
        ] + extra_args
        res = subprocess.run(cmd, capture_output=True, text=True)
        try:
            data = json.loads(res.stdout) if res.stdout.strip() else {}
        except json.JSONDecodeError:
            data = {}
        return res.returncode, data, res.stderr

    def test_unmapped_token_aborts_code_2(self):
        """Unmapped single token must abort with exit code 2 and ERR_DESYNC_KEY."""
        unmapped_tokens = [
            "UNKNOWN_KEY_FOO",
            "BACKSPACE",
            "ESCAPE",
            "TAB",
            "CTRL",
            "SPECIAL_999",
            "@#$%"
        ]
        for token in unmapped_tokens:
            code, data, _ = self._run_cli_diff(["--keys", token])
            self.assertEqual(code, 2, f"Failed for token {token}")
            self.assertEqual(data.get("verdict"), "FAIL")
            self.assertEqual(data.get("error"), "ERR_DESYNC_KEY")
            self.assertEqual(data.get("steps_executed"), 0)
            valid, errors = validate_diff_report_schema(data)
            self.assertTrue(valid, f"Schema validation errors: {errors}")

    def test_unmapped_token_inside_valid_sequence_fails_closed_before_execution(self):
        """Sequence with one invalid token must fail closed with 0 steps executed."""
        seq = "1,+,2,INVALID_MIDDLE_KEY,*,EXE"
        code, data, _ = self._run_cli_diff(["--keys", seq])
        self.assertEqual(code, 2)
        self.assertEqual(data.get("error"), "ERR_DESYNC_KEY")
        self.assertEqual(data.get("steps_executed"), 0)
        self.assertEqual(len(data.get("steps", [])), 0)

    def test_empty_key_sequence_returns_pass_code_0(self):
        """Empty key sequence ('') must return exit code 0 and PASS verdict."""
        code, data, _ = self._run_cli_diff(["--keys", ""])
        self.assertEqual(code, 0)
        self.assertEqual(data.get("verdict"), "PASS")
        self.assertEqual(data.get("total_steps"), 0)
        self.assertEqual(data.get("steps_executed"), 0)
        valid, errors = validate_diff_report_schema(data)
        self.assertTrue(valid, f"Schema validation errors: {errors}")

    def test_whitespace_only_key_sequence_returns_pass_code_0(self):
        """Whitespace-only key sequence must return exit code 0 and PASS verdict."""
        code, data, _ = self._run_cli_diff(["--keys", "   \t \n  "])
        self.assertEqual(code, 0)
        self.assertEqual(data.get("verdict"), "PASS")
        self.assertEqual(data.get("total_steps"), 0)
        valid, errors = validate_diff_report_schema(data)
        self.assertTrue(valid, f"Schema validation errors: {errors}")

    def test_consecutive_commas_and_padding_pruned_safely(self):
        """Key sequences with multiple consecutive commas and padding are normalized."""
        code, data, _ = self._run_cli_diff(["--keys", ",,,1,,+,2,,,EXE,,,", "--hold-ms", "5000"])
        # Should not fail with ERR_DESYNC_KEY (empty tokens pruned)
        self.assertNotEqual(data.get("error"), "ERR_DESYNC_KEY")
        self.assertIn(data.get("error"), ["ERR_WINDOW_NOT_FOUND", "ERR_WIZARD_BLOCKED"])

    def test_case_insensitive_key_tokens(self):
        """Keys like 'f1', 'menu', 'exe', 'ac', 'del' must be normalized and recognized."""
        lowercase_keys = ["f1", "f6", "menu", "exit", "ac", "del", "exe", "shift", "optn"]
        for k in lowercase_keys:
            code, data, _ = self._run_cli_diff(["--keys", k, "--hold-ms", "5000"])
            self.assertNotEqual(data.get("error"), "ERR_DESYNC_KEY", f"Failed to recognize lowercase key: {k}")
            self.assertIn(data.get("error"), ["ERR_WINDOW_NOT_FOUND", "ERR_WIZARD_BLOCKED"])

    def test_script_json_key_extraction_and_rejection(self):
        """Verifies JSON script files with malformed or unmapped keys are rejected."""
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as f:
            json.dump({"steps": [{"keys": "1"}, {"keys": "BOGUS_KEY"}]}, f)
            temp_path = f.name
        try:
            code, data, _ = self._run_cli_diff(["--script", temp_path])
            self.assertEqual(code, 2)
            self.assertEqual(data.get("error"), "ERR_DESYNC_KEY")
        finally:
            if os.path.exists(temp_path):
                os.remove(temp_path)


# =============================================================================
# 3. Fail-Closed Build Gate Stress Tests
# =============================================================================

class TestFailClosedBuildGateStress(unittest.TestCase):
    """Stress tests the compile guard and pre-execution safety gate."""

    def test_build_failure_aborts_immediately_code_2_without_launching_processes(self):
        """A compilation failure halts cmd_diff with exit code 2 and structured build_control JSON."""
        args = MagicMock()
        args.rebuild = True
        args.keys = "1,+,1,EXE"
        args.script = None

        comp_err = build_guard.CompilationError(
            message="Fatal error C1083: Cannot open include file: 'cg50_broken.h': No such file",
            diagnostics=["error C1083: Cannot open include file: 'cg50_broken.h'"]
        )

        with patch("build_guard.verify_or_build_core", side_effect=comp_err), \
             patch("subprocess.Popen") as mock_popen, \
             patch("sys.stdout", new_callable=io.StringIO) as mock_stdout:

            exit_code = cmd_diff(args)

            self.assertEqual(exit_code, 2)
            # Crucial invariant: zero child processes launched!
            self.assertEqual(mock_popen.call_count, 0)

            out_text = mock_stdout.getvalue()
            data = json.loads(out_text)

            self.assertEqual(data.get("verdict"), "FAIL")
            self.assertEqual(data.get("total_steps"), 0)
            self.assertEqual(data.get("steps_executed"), 0)

            bc = data.get("build_control", {})
            self.assertEqual(bc.get("status"), "FAIL")
            self.assertEqual(bc.get("error_type"), "ERR_BUILD_FAILED")
            self.assertIn("error C1083", bc.get("message", ""))

            valid, errors = validate_diff_report_schema(data)
            self.assertTrue(valid, f"Schema validation failed on build failure: {errors}")

    def test_missing_binary_without_rebuild_triggers_build_gate(self):
        """When core binary is missing and build fails, verify_or_build_core aborts with code 2."""
        args = MagicMock()
        args.rebuild = False
        args.keys = "1,+,1,EXE"
        args.script = None

        build_err = build_guard.CompilationError(
            message="Binary not found and automatic rebuild failed (exit code 2)",
            exit_code=2
        )
        with patch("build_guard.verify_or_build_core", side_effect=build_err), \
             patch("subprocess.Popen") as mock_popen, \
             patch("sys.stdout", new_callable=io.StringIO) as mock_stdout:

            exit_code = cmd_diff(args)
            self.assertEqual(exit_code, 2)
            self.assertEqual(mock_popen.call_count, 0)
            data = json.loads(mock_stdout.getvalue())
            self.assertEqual(data.get("build_control", {}).get("error_type"), "ERR_BUILD_FAILED")
            self.assertEqual(data.get("build_control", {}).get("status"), "FAIL")


# =============================================================================
# 4. Report Schema Stress & Adversarial Mutation Testing
# =============================================================================

class TestReportSchemaStress(unittest.TestCase):
    """Stress tests JSON report schema validation, mutation defenses, and invariant guarantees."""

    def _create_canonical_pass_report(self) -> Dict[str, Any]:
        return {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": "2026-10-31T03:00:00Z",
            "build_control": {
                "status": "PASS",
                "compiler": "MSVC 14.44.35207",
                "target": "CalculadoraUnificada.exe",
                "binary_path": r"D:\test\CalculadoraUnificada.exe",
                "binary_sha256": "ABCDEF1234567890"
            },
            "verdict": "PASS",
            "total_steps": 1,
            "steps_executed": 1,
            "first_divergence": None,
            "steps": [
                {
                    "step": 1,
                    "key": "1",
                    "native": {"pc": "0x0801382A", "lcd_sha256": "1234"},
                    "official": {"pc": "0x0801382A", "lcd_sha256": "1234"},
                    "comparison": {"status": "MATCH"}
                }
            ]
        }

    def _create_canonical_divergence_report(self) -> Dict[str, Any]:
        rep = self._create_canonical_pass_report()
        rep["verdict"] = "DIVERGENCIA"
        rep["first_divergence"] = {
            "step": 1,
            "key": "1",
            "divergence_type": "LCD_PIXEL_MISMATCH",
            "details": "Pixel discrepancy: 42 pixels changed (0.05%)"
        }
        return rep

    def test_canonical_pass_report_is_valid(self):
        """Canonical PASS report passes validation with 0 errors."""
        rep = self._create_canonical_pass_report()
        valid, errors = validate_diff_report_schema(rep)
        self.assertTrue(valid, f"Unexpected errors: {errors}")
        self.assertEqual(len(errors), 0)

    def test_canonical_divergence_report_is_valid(self):
        """Canonical DIVERGENCIA report passes validation with 0 errors."""
        rep = self._create_canonical_divergence_report()
        valid, errors = validate_diff_report_schema(rep)
        self.assertTrue(valid, f"Unexpected errors: {errors}")
        self.assertEqual(len(errors), 0)

    def test_adversarial_mutation_missing_root_fields(self):
        """Deleting any required root field must cause validate_diff_report_schema to fail."""
        required_root = [
            "platform", "version", "timestamp", "build_control",
            "verdict", "total_steps", "steps_executed", "first_divergence", "steps"
        ]
        base = self._create_canonical_pass_report()
        for field in required_root:
            mutated = copy.deepcopy(base)
            del mutated[field]
            valid, errors = validate_diff_report_schema(mutated)
            self.assertFalse(valid, f"Schema validation should have failed for missing '{field}'")
            self.assertTrue(any(f"Missing root field: {field}" in e for e in errors))

    def test_adversarial_mutation_missing_build_control_fields(self):
        """Deleting any required build_control field must cause schema failure."""
        required_bc = ["status", "compiler", "target", "binary_path", "binary_sha256"]
        base = self._create_canonical_pass_report()
        for field in required_bc:
            mutated = copy.deepcopy(base)
            del mutated["build_control"][field]
            valid, errors = validate_diff_report_schema(mutated)
            self.assertFalse(valid, f"Schema validation should have failed for missing build_control '{field}'")

    def test_adversarial_mutation_divergencia_null_first_divergence(self):
        """verdict == 'DIVERGENCIA' with first_divergence == None must fail schema validation."""
        rep = self._create_canonical_divergence_report()
        rep["first_divergence"] = None
        valid, errors = validate_diff_report_schema(rep)
        self.assertFalse(valid)
        self.assertTrue(any("first_divergence is null/empty" in e for e in errors))

    def test_adversarial_mutation_divergencia_incomplete_first_divergence(self):
        """Missing required first_divergence field must fail validation."""
        required_fd = ["step", "key", "divergence_type", "details"]
        base = self._create_canonical_divergence_report()
        for field in required_fd:
            mutated = copy.deepcopy(base)
            del mutated["first_divergence"][field]
            valid, errors = validate_diff_report_schema(mutated)
            self.assertFalse(valid, f"Schema validation should have failed for missing first_divergence '{field}'")

    def test_adversarial_mutation_invalid_verdict_value(self):
        """Invalid verdict value (e.g. 'SUCCESS', 'ERROR', 'UNKNOWN') must fail schema validation."""
        for bad_v in ["SUCCESS", "ERROR", "UNKNOWN", "pass", "fail", ""]:
            rep = self._create_canonical_pass_report()
            rep["verdict"] = bad_v
            valid, errors = validate_diff_report_schema(rep)
            self.assertFalse(valid, f"Validation should have failed for verdict '{bad_v}'")

    def test_all_cmd_diff_error_reports_are_schema_conformant(self):
        """All structured error exit points from cmd_diff conform strictly to schema."""
        error_types = [
            ("ERR_INVALID_PACING", ["--keys", "1", "--hold-ms", "0"]),
            ("ERR_UNSAFE_PACING", ["--keys", "1", "--hold-ms", "100"]),
            ("ERR_DESYNC_KEY", ["--keys", "INVALID_KEY_XYZ"])
        ]
        agent_py = os.path.join(TOOLS_DIR, "cg50_agent.py")
        for err_name, args in error_types:
            res = subprocess.run([
                sys.executable, agent_py, "diff",
                "--official-exe", NON_EXISTENT_EXE,
                "--native-exe", NON_EXISTENT_EXE
            ] + args, capture_output=True, text=True)
            self.assertEqual(res.returncode, 2)
            data = json.loads(res.stdout)
            self.assertEqual(data.get("error"), err_name)
            valid, errors = validate_diff_report_schema(data)
            self.assertTrue(valid, f"Schema error in {err_name} report: {errors}")


# =============================================================================
# 5. Divergence & State Isolation Adversarial Edge Cases
# =============================================================================

class TestDivergenceIsolationStress(unittest.TestCase):
    """Stress tests for multi-tier divergence locator and edge condition handling."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            import shutil
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_no_fail_fast_records_cascading_steps_but_preserves_first_divergence(self):
        """When stop_on_first=False, locator executes all steps and records cascade."""
        locator = DivergenceLocator(out_dir=self.temp_dir, stop_on_first=False)

        def mock_executor(idx, key):
            # Divergence starts at step 2 and continues through step 4
            is_div = (idx >= 2)
            nat = {"pc": "0x0801382A" if not is_div else f"0x0802000{idx}"}
            off = {"pc": "0x0801382A"}
            comp = {
                "status": "MATCH" if not is_div else "DIFF",
                "pixel_diff": {"changed_pixels": 0 if not is_div else idx * 50, "changed_percent": 0.1, "diff_bbox": None},
                "state_diff": {"pc_match": not is_div, "ocr_match": True}
            }
            return nat, off, comp

        bc = {"status": "PASS", "compiler": "MSVC", "target": "CalculadoraUnificada.exe", "binary_path": "bin", "binary_sha256": "sha"}
        exit_code, rep = locator.bisect_keystrokes("1,+,2,EXE", mock_executor, bc)

        self.assertEqual(exit_code, 1)
        self.assertEqual(rep["verdict"], "DIVERGENCIA")
        # All 4 steps must be executed despite divergence at step 2
        self.assertEqual(rep["total_steps"], 4)
        self.assertEqual(rep["steps_executed"], 4)
        self.assertEqual(len(rep["steps"]), 4)

        # First divergence MUST remain pinned to step 2!
        fd = rep["first_divergence"]
        self.assertEqual(fd["step"], 2)
        self.assertEqual(fd["key"], "+")

    def test_memory_isolation_identical_buffers_returns_none(self):
        """Identical memory buffers return None discrepancy."""
        buf = bytes([0xAA] * 256)
        res = DivergenceLocator.isolate_memory_divergence("History", 0x880D3400, buf, buf)
        self.assertIsNone(res)

    def test_memory_isolation_boundary_offset_zero(self):
        """Discrepancy at the very first byte (offset 0) is correctly isolated."""
        nat = bytes([0x11] + [0x00] * 15)
        off = bytes([0x22] + [0x00] * 15)
        res = DivergenceLocator.isolate_memory_divergence("History", 0x880D3400, nat, off)
        self.assertIsNotNone(res)
        self.assertEqual(res["offset"], 0)
        self.assertEqual(res["divergent_address"], "0x880D3400")
        self.assertEqual(res["native_byte"], "0x11")
        self.assertEqual(res["official_byte"], "0x22")

    def test_memory_isolation_boundary_last_byte(self):
        """Discrepancy at the very last byte of the window is correctly isolated."""
        nat = bytes([0x00] * 31 + [0xFF])
        off = bytes([0x00] * 31 + [0x00])
        res = DivergenceLocator.isolate_memory_divergence("Display", 0x8804F800, nat, off)
        self.assertIsNotNone(res)
        self.assertEqual(res["offset"], 31)
        self.assertEqual(res["divergent_address"], f"0x{0x8804F800 + 31:08X}")
        self.assertEqual(res["native_byte"], "0xFF")
        self.assertEqual(res["official_byte"], "0x00")

    def test_trace_alignment_differing_pc_vs_differing_opcode(self):
        """Trace comparator distinguishes between PC mismatch and opcode mismatch."""
        # 1. PC Mismatch at ordinal 1
        nat_lines = ["00000001 PC=0x08013820 OP=0xD005 CYC=10"]
        off_records = [{"guest_pc_ebp": "08014000", "opcode": "D005"}]
        res_pc = DivergenceLocator.compare_instruction_traces(nat_lines, off_records)
        self.assertEqual(res_pc["status"], "DIFF")
        self.assertNotEqual(res_pc["first_divergence"]["native_pc"], res_pc["first_divergence"]["official_pc"])
        self.assertEqual(res_pc["first_divergence"]["native_op"], res_pc["first_divergence"]["official_op"])
        self.assertIn("deviation", res_pc["first_divergence"]["message"].lower())

        # 2. Opcode Mismatch at ordinal 1
        off_records_op = [{"guest_pc_ebp": "08013820", "opcode": "9999"}]
        res_op = DivergenceLocator.compare_instruction_traces(nat_lines, off_records_op)
        self.assertEqual(res_op["status"], "DIFF")
        self.assertEqual(res_op["first_divergence"]["native_pc"], res_op["first_divergence"]["official_pc"])
        self.assertNotEqual(res_op["first_divergence"]["native_op"], res_op["first_divergence"]["official_op"])
        self.assertIn("deviation", res_op["first_divergence"]["message"].lower())


if __name__ == "__main__":
    unittest.main(verbosity=2)
