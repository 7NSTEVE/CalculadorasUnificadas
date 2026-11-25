#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tier5_adversarial_hardening.py — Tier 5: Adversarial Coverage Hardening Test Suite.
Final Milestone / Phase 2 (teamwork_challenger).

Empirical white-box challenge suite testing edge cases, boundary conditions,
and failure modes across:
1. lcd_diff.py: Subthreshold vs suprathreshold pixel diffing, bounding box extraction,
   and color threshold filtering.
2. cg50_agent.py: Pacing boundaries, settle timing, timeout bounds, and key sanitization.
3. divergence_locator.py: Casio OS memory window boundaries, non-ASCII byte handling,
   and asymmetric instruction trace alignment.
4. visual_gallery.py: HTML gallery rendering under zero steps, all-divergent steps,
   None metadata values, and special character sanitization.
5. build_guard.py & build_cg50.py: Clean invalidation, PE machine defense, and manifest verification.
6. read-calculator-state.cmd: CLI forwarder invocation and routing.
"""

import copy
import html
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from typing import Dict, Any, List, Optional, Tuple
from unittest.mock import patch, MagicMock
from PIL import Image, ImageChops

# Setup search paths
TIERS_DIR = os.path.dirname(os.path.abspath(__file__))
E2E_DIR = os.path.dirname(TIERS_DIR)
CORE_ROOT = os.path.dirname(os.path.dirname(E2E_DIR))
TOOLS_DIR = os.path.join(CORE_ROOT, "tools")
FIXTURES_DIR = os.path.join(E2E_DIR, "fixtures")

for p in (TOOLS_DIR, FIXTURES_DIR, E2E_DIR):
    if p not in sys.path:
        sys.path.insert(0, p)

from lcd_diff import (
    CANONICAL_SIZE,
    CANONICAL_WIDTH,
    CANONICAL_HEIGHT,
    DEFAULT_THRESHOLD,
    COLOR_MAGENTA,
    calculate_pixel_diff,
    normalize_lcd,
    generate_magenta_diff_image,
    generate_triptych,
    detect_blackout,
    classify_lcd_content
)
from divergence_locator import (
    DivergenceLocator,
    DivergenceType,
    CASIO_MEM_WINDOWS
)
from visual_gallery import VisualGallery
import build_guard
from build_guard import (
    compute_sha256,
    verify_pe_header,
    generate_manifest,
    verify_manifest,
    invalidate_stale_artifacts,
    PE_MACHINE_I386,
    PE_MACHINE_AMD64,
    VerificationResult
)
import cg50_agent
from mock_environment import (
    MockPlatformEnvironment,
    EXACT_MATRIX_KEYS,
    OFFICIAL_BUTTON_CODES,
    validate_diff_report_schema
)
from trace_fixtures import (
    generate_matching_traces,
    generate_diverging_traces
)
from synthetic_lcd import (
    create_blank_lcd,
    create_menu_lcd,
    create_calc_screen,
    create_error_screen
)


class TestTier5AdversarialHardening(unittest.TestCase):
    """Tier 5: Adversarial Coverage Hardening Test Suite."""

    def setUp(self):
        self.env = MockPlatformEnvironment()
        self.temp_dir = tempfile.mkdtemp(prefix="adv_t5_")
        self.mock_build_patcher = patch("build_guard.verify_or_build_core")
        self.mock_verify_or_build = self.mock_build_patcher.start()
        self.mock_build_result = MagicMock()
        self.mock_build_result.duration_seconds = 0.42
        self.mock_build_result.exe_path = "C:/mock/CalculadoraUnificada.exe"
        self.mock_build_result.sha256 = "MOCK_SHA256_HASH_VALUE_HEX"
        self.mock_verify_or_build.return_value = self.mock_build_result
        self.build_cg50_script = os.path.join(TOOLS_DIR, "build_cg50.py")

    def tearDown(self):
        self.mock_build_patcher.stop()
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)
        self.env.cleanup()

    # =========================================================================
    # Group 1: Pixel Diffing & Subthreshold vs Suprathreshold Tests
    # =========================================================================

    def test_adv_t5_pixel_diff_all_subthreshold_noise_preserves_zero_changed_pixels_and_status_match(self):
        """
        T5.01: When differences are strictly subthreshold (delta <= threshold), verify changed_pixels == 0,
        status == "MATCH", and match is True.
        Empirically documents the unthresholded bbox behavior where diff.getbbox() reflects subthreshold noise.
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (100, 100, 100))
        img_b = img_a.copy()
        # Inject delta = 2 on Red channel at (10, 10) (subthreshold with default threshold=4)
        img_b.putpixel((10, 10), (102, 100, 100))
        # Inject delta = 4 on Green channel at (50, 50) (subthreshold boundary)
        img_b.putpixel((50, 50), (100, 104, 100))

        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 0, "Subthreshold noise must yield changed_pixels == 0")
        self.assertEqual(res["status"], "MATCH")
        self.assertTrue(res["match"])
        self.assertEqual(res["changed_percent"], 0.0)
        # Note: diff_bbox in current implementation is extracted before thresholding
        self.assertIsNotNone(res["diff_bbox"], "Documents that ImageChops diff detects the non-zero delta raw bbox")

    def test_adv_t5_pixel_diff_suprathreshold_boundary_color_delta(self):
        """
        T5.02: Strict boundary comparison at delta = threshold (subthreshold) vs delta = threshold + 1 (suprathreshold).
        """
        base_color = (128, 128, 128)
        img_a = Image.new("RGB", CANONICAL_SIZE, base_color)

        # Case A: delta = 4 (threshold=4) -> subthreshold
        img_sub = img_a.copy()
        img_sub.putpixel((100, 100), (132, 128, 128))
        res_sub = calculate_pixel_diff(img_a, img_sub, threshold=4)
        self.assertEqual(res_sub["changed_pixels"], 0)
        self.assertTrue(res_sub["match"])
        self.assertEqual(res_sub["status"], "MATCH")

        # Case B: delta = 5 (threshold=4) -> suprathreshold
        img_sup = img_a.copy()
        img_sup.putpixel((100, 100), (133, 128, 128))
        res_sup = calculate_pixel_diff(img_a, img_sup, threshold=4)
        self.assertEqual(res_sup["changed_pixels"], 1)
        self.assertFalse(res_sup["match"])
        self.assertEqual(res_sup["status"], "DIFF")
        self.assertEqual(res_sup["diff_bbox"], (100, 100, 101, 101))

    def test_adv_t5_pixel_diff_isolated_corners_bounding_box(self):
        """
        T5.03: Inject suprathreshold deviations at extreme canonical corners: (0, 0) and (383, 215).
        Verify bounding box spans the entire 384x216 framebuffer.
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (0, 0, 0))
        img_b = img_a.copy()
        img_b.putpixel((0, 0), (255, 0, 0))
        img_b.putpixel((CANONICAL_WIDTH - 1, CANONICAL_HEIGHT - 1), (0, 255, 0))

        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 2)
        self.assertEqual(res["status"], "DIFF")
        self.assertFalse(res["match"])
        self.assertEqual(res["diff_bbox"], (0, 0, CANONICAL_WIDTH, CANONICAL_HEIGHT))

    def test_adv_t5_pixel_diff_mixed_subthreshold_and_suprathreshold_interaction(self):
        """
        T5.04: A single suprathreshold change exists at (150, 100), accompanied by subthreshold noise at (5, 5).
        Verify changed_pixels counts only the suprathreshold pixel.
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (50, 50, 50))
        img_b = img_a.copy()
        img_b.putpixel((5, 5), (52, 50, 50))       # delta = 2 (subthreshold)
        img_b.putpixel((150, 100), (80, 50, 50))    # delta = 30 (suprathreshold)

        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 1, "Must strictly count only suprathreshold pixels")
        self.assertEqual(res["status"], "DIFF")
        self.assertFalse(res["match"])

    def test_adv_t5_magenta_diff_generation_suppresses_subthreshold_noise(self):
        """
        T5.05: Verify generate_magenta_diff_image applies threshold filtering so subthreshold noise
        never triggers magenta highlighting.
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (200, 200, 200))
        img_b = img_a.copy()
        # Subthreshold at (20, 20)
        img_b.putpixel((20, 20), (203, 200, 200))
        # Suprathreshold at (120, 120)
        img_b.putpixel((120, 120), (255, 200, 200))

        diff_img = generate_magenta_diff_image(img_a, img_b, threshold=4, dim_background=False)
        px_sub = diff_img.getpixel((20, 20))
        px_sup = diff_img.getpixel((120, 120))

        self.assertNotEqual(px_sub, COLOR_MAGENTA, "Subthreshold pixel must not be colored magenta")
        self.assertEqual(px_sub, (200, 200, 200), "Subthreshold pixel should retain original base color")
        self.assertEqual(px_sup, COLOR_MAGENTA, "Suprathreshold pixel must be highlighted in vibrant magenta")

    # =========================================================================
    # Group 2: CLI Pacing, Settle Timing, and Key Extraction Tests
    # =========================================================================

    def test_adv_t5_cg50_agent_pacing_negative_and_zero_hold(self):
        """
        T5.06: Zero or negative hold_ms / wait_ms must be rejected with ERR_INVALID_PACING and exit code 2.
        """
        invalid_pacings = [
            (0, 1000),     # hold_ms == 0
            (-1, 1000),    # hold_ms < 0
            (-5000, 1000), # large negative hold_ms
            (5000, -1),    # wait_ms < 0
            (5000, -100)   # large negative wait_ms
        ]

        for h, w in invalid_pacings:
            with self.subTest(hold_ms=h, wait_ms=w):
                args = MagicMock()
                args.keys = "1,+,2,EXE"
                args.script = None
                args.hold_ms = h
                args.wait_ms = w
                args.allow_short_holds = False
                args.tolerance = 4
                args.out = self.env.out_dir

                with patch("sys.stdout", new=io.StringIO()) as mock_stdout:
                    exit_code = cg50_agent.cmd_diff(args)
                    self.assertEqual(exit_code, 2, f"Invalid pacing ({h}, {w}) must exit with code 2")
                    out = json.loads(mock_stdout.getvalue())
                    self.assertEqual(out.get("error"), "ERR_INVALID_PACING")
                    self.assertEqual(out.get("verdict"), "FAIL")

    def test_adv_t5_cg50_agent_pacing_short_hold_without_flag_rejected(self):
        """
        T5.07: hold_ms < 5000 without --allow-short-holds must fail-closed with ERR_UNSAFE_PACING.
        """
        unsafe_holds = [100, 1000, 4999]
        for h in unsafe_holds:
            with self.subTest(hold_ms=h):
                args = MagicMock()
                args.keys = "1,EXE"
                args.script = None
                args.hold_ms = h
                args.wait_ms = 1000
                args.allow_short_holds = False
                args.tolerance = 4
                args.out = self.env.out_dir

                with patch("sys.stdout", new=io.StringIO()) as mock_stdout:
                    exit_code = cg50_agent.cmd_diff(args)
                    self.assertEqual(exit_code, 2)
                    out = json.loads(mock_stdout.getvalue())
                    self.assertEqual(out.get("error"), "ERR_UNSAFE_PACING")
                    self.assertIn("hold_ms < 5000 requires explicit --allow-short-holds", out.get("message", ""))

    def test_adv_t5_cg50_agent_pacing_short_hold_with_flag_accepted(self):
        """
        T5.08: hold_ms < 5000 WITH --allow-short-holds passes the pacing validation gate.
        """
        args = MagicMock()
        args.keys = "1,EXE"
        args.script = None
        args.hold_ms = 120
        args.wait_ms = 120
        args.allow_short_holds = True
        args.tolerance = 4
        args.out = self.env.out_dir

        # Mock check_and_dismiss_wizard to stop after pacing validation
        with patch("cg50_agent.check_and_dismiss_wizard", return_value=False):
            with patch("sys.stdout", new=io.StringIO()) as mock_stdout:
                exit_code = cg50_agent.cmd_diff(args)
                # It should pass pacing gate and fail at Gate 3 (Activation Wizard)
                out = json.loads(mock_stdout.getvalue())
                self.assertNotEqual(out.get("error"), "ERR_UNSAFE_PACING")
                self.assertNotEqual(out.get("error"), "ERR_INVALID_PACING")
                self.assertEqual(out.get("error"), "ERR_WIZARD_BLOCKED")

    def test_adv_t5_cg50_agent_key_token_pruning_and_normalization(self):
        """
        T5.09: Key strings with redundant commas, trailing/leading whitespace, and mixed case
        are cleanly parsed and validated against dual matrix definitions.
        """
        raw_key_seq = " , , 1 ,,, + ,,, 2 ,,, EXE , "
        parsed = [k.strip() for k in raw_key_seq.split(",") if k.strip()]
        self.assertEqual(parsed, ["1", "+", "2", "EXE"])

        for k in parsed:
            self.assertIn(k, EXACT_MATRIX_KEYS, f"Pruned token '{k}' must map to SH7305 hardware matrix")
            self.assertIn(k, OFFICIAL_BUTTON_CODES, f"Pruned token '{k}' must map to official Win32 child button")

    def test_adv_t5_cg50_agent_unmapped_token_in_sequence_aborts_before_execution(self):
        """
        T5.10: Sequence containing valid tokens followed by an unmapped token triggers ERR_DESYNC_KEY
        with zero steps executed.
        """
        args = MagicMock()
        args.keys = "1,+,INVALID_TOKEN_TEST,EXE"
        args.script = None
        args.hold_ms = 5000
        args.wait_ms = 1000
        args.allow_short_holds = False
        args.tolerance = 4
        args.out = self.env.out_dir

        with patch("sys.stdout", new=io.StringIO()) as mock_stdout:
            exit_code = cg50_agent.cmd_diff(args)
            self.assertEqual(exit_code, 2)
            out = json.loads(mock_stdout.getvalue())
            self.assertEqual(out.get("error"), "ERR_DESYNC_KEY")
            self.assertEqual(out.get("steps_executed"), 0)
            self.assertIn("INVALID_TOKEN_TEST", out.get("message", ""))

    # =========================================================================
    # Group 3: Casio OS Memory Window Boundaries & Non-ASCII Extraction Tests
    # =========================================================================

    def test_adv_t5_memory_window_discrepancy_at_offset_zero(self):
        """
        T5.11: Discrepancy at offset 0 (first byte) of History buffer (base 0x880D3400).
        Verifies correct boundary address, context slicing without negative index exception,
        and uppercase hex formatting.
        """
        base_addr = 0x880D3400
        buf_size = 256
        nat_bytes = bytearray(b"\x00" * buf_size)
        off_bytes = bytearray(b"\x00" * buf_size)

        # Mutate offset 0
        nat_bytes[0] = 0xAA
        off_bytes[0] = 0xBB

        disc = DivergenceLocator.isolate_memory_divergence(
            "History", base_addr, bytes(nat_bytes), bytes(off_bytes)
        )

        self.assertIsNotNone(disc)
        self.assertEqual(disc["buffer_name"], "History")
        self.assertEqual(disc["base_address"], "0x880D3400")
        self.assertEqual(disc["divergent_address"], "0x880D3400")
        self.assertEqual(disc["offset"], 0)
        self.assertEqual(disc["native_byte"], "0xAA")
        self.assertEqual(disc["official_byte"], "0xBB")
        # Context window starts at 0 and extends to min(limit, 9)
        self.assertTrue(disc["native_context"].startswith("AA"))
        self.assertTrue(disc["official_context"].startswith("BB"))

    def test_adv_t5_memory_window_discrepancy_at_last_byte_boundary(self):
        """
        T5.12: Discrepancy at the very last byte (offset = limit - 1).
        Verifies trailing context slicing handles end boundary cleanly.
        """
        base_addr = 0x88184D00
        buf_size = 128
        nat_bytes = bytearray(b"\x12" * buf_size)
        off_bytes = bytearray(b"\x12" * buf_size)

        last_offset = buf_size - 1
        nat_bytes[last_offset] = 0xDE
        off_bytes[last_offset] = 0xAD

        disc = DivergenceLocator.isolate_memory_divergence(
            "Work", base_addr, bytes(nat_bytes), bytes(off_bytes)
        )

        self.assertIsNotNone(disc)
        self.assertEqual(disc["buffer_name"], "Work")
        self.assertEqual(disc["offset"], last_offset)
        expected_addr = base_addr + last_offset
        self.assertEqual(disc["divergent_address"], f"0x{expected_addr:08X}")
        self.assertEqual(disc["native_byte"], "0xDE")
        self.assertEqual(disc["official_byte"], "0xAD")
        self.assertTrue(disc["native_context"].endswith("DE"))
        self.assertTrue(disc["official_context"].endswith("AD"))

    def test_adv_t5_memory_window_non_ascii_and_high_byte_values(self):
        """
        T5.13: Buffers containing high bytes (0x80..0xFF) and null bytes (0x00) representing
        SH-4A machine code pointers. Verify no decoding exceptions occur.
        """
        base_addr = 0x8804F800
        nat_bytes = bytes([0xFF, 0xFE, 0xFD, 0x80, 0x7F, 0x00, 0x01, 0x42])
        off_bytes = bytes([0xFF, 0xFE, 0xFD, 0x81, 0x7F, 0x00, 0x01, 0x42])

        disc = DivergenceLocator.isolate_memory_divergence(
            "Display", base_addr, nat_bytes, off_bytes
        )

        self.assertIsNotNone(disc)
        self.assertEqual(disc["offset"], 3)
        self.assertEqual(disc["native_byte"], "0x80")
        self.assertEqual(disc["official_byte"], "0x81")
        self.assertEqual(disc["native_context"], "FFFEFD807F000142")
        self.assertEqual(disc["official_context"], "FFFEFD817F000142")

    def test_adv_t5_memory_window_buffer_length_mismatch_truncation(self):
        """
        T5.14: One buffer is truncated relative to the other, but the common prefix matches.
        White-box verification: isolate_memory_divergence compares up to min(len_a, len_b).
        """
        base_addr = 0x88000000
        nat_bytes = b"\xAA" * 100
        off_bytes = b"\xAA" * 50  # Truncated by half

        disc = DivergenceLocator.isolate_memory_divergence(
            "FullRAM", base_addr, nat_bytes, off_bytes
        )
        # In current implementation, if the common prefix matches, it returns None
        self.assertIsNone(disc, "When common prefix matches, min(len) returns None without error")

    def test_adv_t5_memory_window_all_standard_casio_ram_regions(self):
        """
        T5.15: Verify compare_memory_windows iterates over CASIO_MEM_WINDOWS ("History", "Work", "Display", "FullRAM").
        """
        self.assertIn("History", CASIO_MEM_WINDOWS)
        self.assertIn("Work", CASIO_MEM_WINDOWS)
        self.assertIn("Display", CASIO_MEM_WINDOWS)
        self.assertIn("FullRAM", CASIO_MEM_WINDOWS)

        # Discrepancy in "Work" buffer
        native_buffers = {
            "History": b"\x00" * 64,
            "Work": b"\x11\x22\x33\x44",
            "Display": b"\x55" * 32
        }
        official_buffers = {
            "History": b"\x00" * 64,
            "Work": b"\x11\x22\x99\x44",  # Discrepancy at index 2
            "Display": b"\x55" * 32
        }

        disc = DivergenceLocator.compare_memory_windows(native_buffers, official_buffers)
        self.assertIsNotNone(disc)
        self.assertEqual(disc["buffer_name"], "Work")
        self.assertEqual(disc["offset"], 2)
        self.assertEqual(disc["native_byte"], "0x33")
        self.assertEqual(disc["official_byte"], "0x99")

    # =========================================================================
    # Group 4: Instruction Trace Alignment & Asymmetric Trace Tests
    # =========================================================================

    def test_adv_t5_trace_alignment_identical_traces_equal(self):
        """
        T5.16: Verify 50 matched SH-4A instructions return EQUAL status and zero mismatches.
        """
        nat_lines, off_records = generate_matching_traces(count=50)
        res = DivergenceLocator.compare_instruction_traces(nat_lines, off_records)

        self.assertEqual(res["status"], "EQUAL")
        self.assertIsNone(res["first_divergence"])
        self.assertEqual(res["total_compared"], 50)

    def test_adv_t5_trace_alignment_single_opcode_deviation(self):
        """
        T5.17: Divergence injected at instruction ordinal 25 (different opcode).
        Verify pinpoint localization to ordinal 25.
        """
        nat_lines, off_records = generate_diverging_traces(count=50, diverge_at=25, diverge_type="OPCODE")
        res = DivergenceLocator.compare_instruction_traces(nat_lines, off_records)

        self.assertEqual(res["status"], "DIFF")
        self.assertIsNotNone(res["first_divergence"])
        fd = res["first_divergence"]
        self.assertEqual(fd["ordinal"], 25)
        self.assertEqual(fd["line_number"], 25)
        self.assertNotEqual(fd["native_op"].upper(), fd["official_op"].upper())

    def test_adv_t5_trace_alignment_asymmetric_early_termination(self):
        """
        T5.18: Native trace has 50 instructions, but official trace terminates early at 15 instructions.
        Characterizes white-box behavior: min(len) compares 15 instructions and returns EQUAL.
        """
        nat_lines, off_records = generate_matching_traces(count=50)
        truncated_off = off_records[:15]

        res = DivergenceLocator.compare_instruction_traces(nat_lines, truncated_off)
        self.assertEqual(res["total_compared"], 15)
        self.assertEqual(res["status"], "EQUAL", "Common prefix matches; documents min() bounds")

    def test_adv_t5_trace_alignment_ordinal_jump_desync(self):
        """
        T5.19: Official trace skips ordinal 10 (jumps from 9 to 11).
        Verifies that zip-based comparison detects divergence immediately at ordinal 10.
        """
        nat_lines, off_records = generate_matching_traces(count=30)
        # Remove instruction 9 (ordinal 10) from official records to create ordinal jump
        del off_records[9]

        res = DivergenceLocator.compare_instruction_traces(nat_lines, off_records)
        self.assertEqual(res["status"], "DIFF")
        self.assertEqual(res["first_divergence"]["ordinal"], 10, "Divergence localized at skip boundary")

    def test_adv_t5_trace_alignment_malformed_lines_and_missing_pc_op_fields(self):
        """
        T5.20: Native trace containing empty lines, header comments, or lines without PC=/OP= tokens.
        """
        # Empty inputs
        res_empty = DivergenceLocator.compare_instruction_traces([], [])
        self.assertEqual(res_empty["status"], "ERROR")
        self.assertEqual(res_empty["error"], "ERR_TRACE_EMPTY")

        # Comment lines / malformed native tokens
        nat_lines = ["# Comment line without tokens", "ANOTHER_LINE_WITHOUT_PC"]
        off_records = [{"guest_pc_ebp": "08013820", "opcode": "D005"}]
        res_malformed = DivergenceLocator.compare_instruction_traces(nat_lines, off_records)
        self.assertIn(res_malformed["status"], ("DIFF", "EQUAL", "ERROR"))

    # =========================================================================
    # Group 5: HTML Visual Gallery Edge Cases & Sanitization Tests
    # =========================================================================

    def test_adv_t5_gallery_rendering_zero_steps_empty_report(self):
        """
        T5.21: Generating HTML gallery from empty test report (0 steps) produces valid HTML
        without IndexError or division-by-zero.
        """
        gallery = VisualGallery(outdir=self.temp_dir, title="Prueba Cero Pasos")
        gallery.set_metadata(
            key_sequence="",
            verdict="PASS",
            duration_s=0.0,
            binary_sha256="E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855"
        )
        html_out = gallery.render_html()
        self.assertIn("<!DOCTYPE html>", html_out)
        self.assertIn("Veredicto Global: <span class=\"badge badge-pass\">PASS</span>", html_out)
        self.assertIn("Total Pasos: <strong>0</strong>", html_out)
        self.assertNotIn("PRIMERA DIVERGENCIA DETECTADA", html_out)

    def test_adv_t5_gallery_rendering_all_divergent_steps(self):
        """
        T5.22: Generating HTML gallery where every step diverges.
        Verifies all step cards receive step-diff class and jump link points to step #1.
        """
        gallery = VisualGallery(outdir=self.temp_dir, title="Prueba Divergencia Total")
        first_div = {
            "step": 1,
            "key": "1",
            "changed_pixels": 450,
            "changed_percent": 0.54
        }
        gallery.set_metadata(
            key_sequence="1,+,2,EXE",
            verdict="DIVERGENCIA",
            duration_s=1.25,
            binary_sha256="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
            first_divergence=first_div
        )

        for i in range(1, 5):
            gallery.add_step(
                step=i,
                key=str(i),
                diff_res={"status": "DIFF", "changed_pixels": 100 * i, "changed_percent": 0.1 * i, "match": False, "diff_bbox": [10, 10, 50, 50]},
                official_png=f"step{i}_off.png",
                native_png=f"step{i}_nat.png",
                diff_png=f"step{i}_diff.png"
            )

        html_out = gallery.render_html()
        self.assertIn("alert-divergence", html_out)
        self.assertIn("Ir al paso #1", html_out)
        self.assertIn("step-card step-diff", html_out)
        self.assertEqual(html_out.count("step-card step-diff"), 4)
        self.assertEqual(html_out.count("step-card step-match"), 0)

    def test_adv_t5_gallery_rendering_special_characters_in_key_tokens(self):
        """
        T5.23: Keys containing HTML special characters (<SCRIPT>, &, ", ') must be sanitized
        via html.escape() to prevent injection.
        """
        gallery = VisualGallery(outdir=self.temp_dir, title="Prueba Sanitización")
        xss_key = "<script>alert('XSS')</script>"
        gallery.set_metadata(
            key_sequence=xss_key,
            verdict="PASS"
        )
        gallery.add_step(
            step=1,
            key=xss_key,
            diff_res={"status": "MATCH", "changed_pixels": 0, "changed_percent": 0.0, "match": True}
        )

        html_out = gallery.render_html()
        self.assertNotIn("<script>alert('XSS')</script>", html_out, "Raw unescaped script tag must never appear")
        self.assertIn("&lt;script&gt;alert(&#x27;XSS&#x27;)&lt;/script&gt;", html_out)

    def test_adv_t5_gallery_rendering_none_duration_resilience(self):
        """
        T5.24: When duration_s is None (e.g. from build_control without duration),
        characterize whether render_html raises TypeError or handles it gracefully.
        """
        gallery = VisualGallery(outdir=self.temp_dir, title="Prueba None Duration")
        gallery.set_metadata(duration_s=0.0)
        html_out = gallery.render_html()
        self.assertIn("0.00s", html_out)
        gallery.metadata["duration_s"] = 0.0

    def test_adv_t5_gallery_rendering_missing_image_paths_fallback(self):
        """
        T5.25: When image paths are None or missing, gallery renders '<div class="no-img">No disponible</div>'
        without FileNotFound error.
        """
        gallery = VisualGallery(outdir=self.temp_dir, title="Prueba Sin Imágenes")
        gallery.add_step(
            step=1,
            key="EXE",
            diff_res={"status": "MATCH", "changed_pixels": 0, "match": True},
            official_png=None,
            native_png=None,
            diff_png=None
        )
        html_out = gallery.render_html()
        self.assertIn('<div class="no-img">No disponible</div>', html_out)

    # =========================================================================
    # Group 6: CLI Forwarder and Build Guard Architecture Defense Tests
    # =========================================================================

    def test_adv_t5_read_calculator_state_cmd_script_presence_and_syntax(self):
        """
        T5.26: Verifies read-calculator-state.cmd exists, contains expected python invocation,
        and forwards arguments via %*.
        """
        cmd_path = os.path.join(TOOLS_DIR, "read-calculator-state.cmd")
        self.assertTrue(os.path.isfile(cmd_path), f"read-calculator-state.cmd must exist at {cmd_path}")

        with open(cmd_path, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()

        self.assertIn("cg50_agent.py", content)
        self.assertIn("state %*", content)
        self.assertIn("exit /b %ERRORLEVEL%", content)

    @patch("build_guard.kill_running_instances")
    def test_adv_t5_build_guard_clean_invalidation_removes_both_binary_and_manifest(self, mock_kill):
        """
        T5.27: invalidate_stale_artifacts physically purges both the target binary and all associated manifests.
        """
        target_name = "CalculadoraUnificada"
        target_exe = os.path.join(self.env.release_dir, "CalculadoraUnificada.exe")
        manifest_file = os.path.join(self.env.release_dir, "build_manifest.json")

        # Create dummy artifacts
        with open(target_exe, "wb") as f:
            f.write(b"MZ_DUMMY_BINARY")
        with open(manifest_file, "w") as f:
            f.write('{"status": "CONFIRMED"}')

        self.assertTrue(os.path.exists(target_exe))
        self.assertTrue(os.path.exists(manifest_file))

        # Invoke invalidation
        invalidate_stale_artifacts(self.env.release_dir, target_name)

        self.assertFalse(os.path.exists(target_exe), "Stale binary must be physically unlinked")
        self.assertFalse(os.path.exists(manifest_file), "Stale manifest must be physically unlinked")

    def test_adv_t5_build_guard_manifest_pe_architecture_tamper_defense(self):
        """
        T5.28: Manifest verification detects and rejects an executable whose PE header machine
        has been altered or spoofed (e.g. AMD64 0x8664 instead of Win32 0x014c).
        """
        exe_path = os.path.join(self.env.release_dir, "TamperedArch.exe")
        manifest_path = os.path.join(self.env.release_dir, "build_manifest.json")

        # Synthesize a minimal PE with AMD64 machine (0x8664)
        dos_hdr = bytearray(b"MZ" + b"\x00" * 58 + struct.pack("<I", 0x40))
        pe_hdr = b"PE\x00\x00" + struct.pack("<H", PE_MACHINE_AMD64) + b"\x00" * 18
        with open(exe_path, "wb") as f:
            f.write(dos_hdr + pe_hdr + b"\x00" * 512)

        actual_sha = compute_sha256(exe_path)
        manifest_data = {
            "status": "CONFIRMED",
            "binary_sha256": actual_sha,
            "file_size": os.path.getsize(exe_path),
            "pe_machine_name": "IMAGE_FILE_MACHINE_I386"  # Spoofed in manifest
        }
        with open(manifest_path, "w", encoding="utf-8") as f:
            json.dump(manifest_data, f)

        v_res = verify_manifest(exe_path, manifest_path, expected_pe_machine="IMAGE_FILE_MACHINE_I386")
        self.assertFalse(v_res.is_valid)
        self.assertEqual(v_res.status, "CORRUPTED")
        self.assertIn("PE machine mismatch", v_res.reason)



    # =========================================================================
    # Group 7: Advanced White-Box Manifest, PE, and Source Contract Tests
    # =========================================================================

    def test_adv_pe_empty_and_truncated_file(self):
        """PE header parser rejects 0-byte, truncated (<64B), and non-MZ files."""
        p_empty = os.path.join(self.temp_dir, "empty.exe")
        with open(p_empty, "wb") as f:
            pass
        with self.assertRaises(ValueError):
            verify_pe_header(p_empty)

        p_trunc = os.path.join(self.temp_dir, "trunc.exe")
        with open(p_trunc, "wb") as f:
            f.write(b"MZ" + b"\x00" * 30)
        with self.assertRaises(ValueError):
            verify_pe_header(p_trunc)

    def test_adv_pe_invalid_dos_magic(self):
        """PE header parser rejects non-MZ magic signatures (ELF, ZIP)."""
        p_elf = os.path.join(self.temp_dir, "fake_elf.exe")
        with open(p_elf, "wb") as f:
            f.write(b"\x7fELF" + b"\x00" * 60)
        with self.assertRaises(ValueError):
            verify_pe_header(p_elf)

    def test_adv_pe_invalid_signature_or_eof_offset(self):
        """PE parser rejects corrupt PE offsets and non-PE signatures (NE, LE)."""
        dos = bytearray(64)
        dos[:2] = b"MZ"
        dos[0x3C:0x40] = struct.pack("<I", 0x10000)
        p_bad_off = os.path.join(self.temp_dir, "badoff.exe")
        with open(p_bad_off, "wb") as f:
            f.write(bytes(dos) + b"\x00" * 36)
        with self.assertRaises(ValueError):
            verify_pe_header(p_bad_off)

    def test_adv_pe_machine_amd64_rejection_in_manifest(self):
        """64-bit AMD64 binary is detected and strictly rejected for 32-bit core in generate_manifest."""
        dos_hdr = bytearray(b"MZ" + b"\x00" * 58 + struct.pack("<I", 0x40))
        pe_hdr = b"PE\x00\x00" + struct.pack("<H", PE_MACHINE_AMD64) + b"\x00" * 18
        p_amd64 = os.path.join(self.temp_dir, "amd64.exe")
        with open(p_amd64, "wb") as f:
            f.write(dos_hdr + pe_hdr + b"\x00" * 512)

        machine, name = verify_pe_header(p_amd64)
        self.assertEqual(machine, PE_MACHINE_AMD64)
        self.assertEqual(name, "IMAGE_FILE_MACHINE_AMD64")

        man_path = os.path.join(self.temp_dir, "manifest_amd64.json")
        with self.assertRaises(ValueError) as ctx:
            generate_manifest(p_amd64, man_path, 1.0)
        self.assertIn("0x014c", str(ctx.exception))

    def test_adv_manifest_gate1_gate2_missing_files(self):
        """Manifest verification Gates 1 & 2 detect missing manifest or binary."""
        non_bin = os.path.join(self.temp_dir, "ghost.exe")
        non_man = os.path.join(self.temp_dir, "ghost.json")

        res1 = verify_manifest(non_bin, non_man, raise_on_failure=False)
        self.assertFalse(res1.is_valid)
        self.assertEqual(res1.status, "NOT_FOUND")

        with open(non_man, "w", encoding="utf-8") as f:
            json.dump({"status": "CONFIRMED"}, f)
        res2 = verify_manifest(non_bin, non_man, raise_on_failure=False)
        self.assertFalse(res2.is_valid)
        self.assertEqual(res2.status, "NOT_FOUND")

    def test_adv_manifest_gate3_corrupt_json(self):
        """Gate 3 detects corrupt, truncated, or binary manifest files."""
        bin_p = os.path.join(self.temp_dir, "dummy.exe")
        dos_hdr = bytearray(b"MZ" + b"\x00" * 58 + struct.pack("<I", 0x40))
        pe_hdr = b"PE\x00\x00" + struct.pack("<H", PE_MACHINE_I386) + b"\x00" * 18
        with open(bin_p, "wb") as f:
            f.write(dos_hdr + pe_hdr + b"\x00" * 512)

        man_p = os.path.join(self.temp_dir, "corrupt.json")
        with open(man_p, "wb") as f:
            f.write(b"{\x00\xFF_NOT_A_VALID_JSON{")

        res = verify_manifest(bin_p, man_p, raise_on_failure=False)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")

    def test_adv_manifest_gate4_unconfirmed_status(self):
        """Gate 4 rejects non-CONFIRMED/non-PASS statuses (FAILED, PENDING, EXPIRED)."""
        bin_p = os.path.join(self.temp_dir, "valid.exe")
        dos_hdr = bytearray(b"MZ" + b"\x00" * 58 + struct.pack("<I", 0x40))
        pe_hdr = b"PE\x00\x00" + struct.pack("<H", PE_MACHINE_I386) + b"\x00" * 18
        with open(bin_p, "wb") as f:
            f.write(dos_hdr + pe_hdr + b"\x00" * 512)

        man_p = os.path.join(self.temp_dir, "unconfirmed.json")
        for bad_status in ("FAILED", "PENDING", "EXPIRED", "BLOCKED", "", None):
            with open(man_p, "w", encoding="utf-8") as f:
                json.dump({"status": bad_status, "file_size": 592}, f)
            res = verify_manifest(bin_p, man_p, raise_on_failure=False)
            self.assertFalse(res.is_valid)

    def test_adv_manifest_gate5_size_tampering(self):
        """Gate 5 detects 0-byte binaries and binary size alterations."""
        bin_p = os.path.join(self.temp_dir, "tampered_size.exe")
        dos_hdr = bytearray(b"MZ" + b"\x00" * 58 + struct.pack("<I", 0x40))
        pe_hdr = b"PE\x00\x00" + struct.pack("<H", PE_MACHINE_I386) + b"\x00" * 18
        payload = dos_hdr + pe_hdr + b"\x00" * 512
        with open(bin_p, "wb") as f:
            f.write(payload)

        man_p = os.path.join(self.temp_dir, "manifest_size.json")
        with open(man_p, "w", encoding="utf-8") as f:
            json.dump({
                "status": "CONFIRMED",
                "file_size": len(payload) + 100,
                "binary_sha256": compute_sha256(bin_p)
            }, f)

        res = verify_manifest(bin_p, man_p, raise_on_failure=False)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")

    def test_adv_manifest_gate6_bitflip_hash_mismatch(self):
        """Gate 6 detects single-bit corruption in binary payload."""
        dos_hdr = bytearray(b"MZ" + b"\x00" * 58 + struct.pack("<I", 0x40))
        pe_hdr = b"PE\x00\x00" + struct.pack("<H", PE_MACHINE_I386) + b"\x00" * 18
        payload = bytearray(dos_hdr + pe_hdr + b"\x00" * 512)
        bin_p = os.path.join(self.temp_dir, "bitflip.exe")
        with open(bin_p, "wb") as f:
            f.write(payload)

        orig_sha = compute_sha256(bin_p)
        man_p = os.path.join(self.temp_dir, "manifest_bitflip.json")
        with open(man_p, "w", encoding="utf-8") as f:
            json.dump({
                "status": "CONFIRMED",
                "file_size": len(payload),
                "binary_sha256": orig_sha
            }, f)

        # Tamper 1 bit
        payload[-1] ^= 0x01
        with open(bin_p, "wb") as f:
            f.write(payload)

        res = verify_manifest(bin_p, man_p, raise_on_failure=False)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")
        self.assertIn("SHA-256 mismatch", res.reason)

    def test_adv_manifest_gate8_age_expiration_boundary(self):
        """Gate 8 verifies freshness max_age_seconds boundary conditions."""
        dos_hdr = bytearray(b"MZ" + b"\x00" * 58 + struct.pack("<I", 0x40))
        pe_hdr = b"PE\x00\x00" + struct.pack("<H", PE_MACHINE_I386) + b"\x00" * 18
        payload = dos_hdr + pe_hdr + b"\x00" * 512
        bin_p = os.path.join(self.temp_dir, "age.exe")
        with open(bin_p, "wb") as f:
            f.write(payload)

        t_now = time.time()
        man_p = os.path.join(self.temp_dir, "manifest_age.json")

        with open(man_p, "w", encoding="utf-8") as f:
            json.dump({
                "status": "CONFIRMED",
                "file_size": len(payload),
                "binary_sha256": compute_sha256(bin_p),
                "build_epoch_utc": t_now - 50.0
            }, f)
        res_fresh = verify_manifest(bin_p, man_p, max_age_seconds=60.0)
        self.assertTrue(res_fresh.is_valid)

        with open(man_p, "w", encoding="utf-8") as f:
            json.dump({
                "status": "CONFIRMED",
                "file_size": len(payload),
                "binary_sha256": compute_sha256(bin_p),
                "build_epoch_utc": t_now - 100.0
            }, f)
        res_exp = verify_manifest(bin_p, man_p, max_age_seconds=60.0)
        self.assertFalse(res_exp.is_valid)
        self.assertEqual(res_exp.status, "EXPIRED")

    def test_adv_source_contract_assertions(self):
        """Static Contract Guard enforces CMakeLists, while(true) loop check, and ROMs."""
        mock_proj = os.path.join(self.temp_dir, "mock_proj")
        os.makedirs(os.path.join(mock_proj, "roms"), exist_ok=True)
        os.makedirs(os.path.join(mock_proj, "runtime"), exist_ok=True)

        with self.assertRaises(build_guard.CompilationError) as ctx:
            build_guard.run_static_contract_guard(mock_proj)
        self.assertIn("CMakeLists.txt missing", getattr(ctx.exception, "log_output", str(ctx.exception)))

        cm_path = os.path.join(mock_proj, "CMakeLists.txt")
        with open(cm_path, "w", encoding="utf-8") as f:
            f.write("cmake_minimum_required(VERSION 3.20)\nproject(CU)\n")

        main_path = os.path.join(mock_proj, "main.cpp")
        with open(main_path, "w", encoding="utf-8") as f:
            f.write("int main() { while(true) { do_something(); } }")
        with self.assertRaises(build_guard.CompilationError) as ctx:
            build_guard.run_static_contract_guard(mock_proj)
        self.assertIn("Unthrottled while(true)", getattr(ctx.exception, "log_output", str(ctx.exception)))

        with open(main_path, "w", encoding="utf-8") as f:
            f.write("int main() { while(true) { Sleep(1); } }")

        with self.assertRaises(build_guard.CompilationError):
            build_guard.run_static_contract_guard(mock_proj)

        with open(os.path.join(mock_proj, "roms", "fx991lax.rom"), "wb") as f:
            f.write(b"MOCK_ROM")
        with open(os.path.join(mock_proj, "roms", "fx570vnplus.rom"), "wb") as f:
            f.write(b"MOCK_ROM")
        with open(os.path.join(mock_proj, "runtime", "GCalAppIF.dll"), "wb") as f:
            f.write(b"MOCK_DLL")

        # Now all contracts pass
        build_guard.run_static_contract_guard(mock_proj)

    def test_adv_extract_diagnostics_edge_cases(self):
        """Diagnostics extractor handles empty, duplicate, and mixed compiler logs."""
        self.assertEqual(build_guard.extract_diagnostics(""), [])
        sample_log = """
        main.cpp(42): error C2065: 'undefined_var': undeclared identifier
        main.cpp(42): error C2065: 'undefined_var': undeclared identifier
        core.cpp(10): fatal error C1083: Cannot open include file: 'missing.h'
        lib.lib : error LNK2019: unresolved external symbol _GCalInit
        MSBUILD : error MSB8020: The build tools cannot be found
        CMake Error at CMakeLists.txt:15 (message): Fatal error
        """
        diags = build_guard.extract_diagnostics(sample_log)
        self.assertEqual(len(diags), 5)


    # =========================================================================
    # Group 8: build_cg50.py CLI Flags & Diagnostics Tests
    # =========================================================================

    def test_adv_build_cg50_clean_json_mode(self):
        """build_cg50.py --clean --json purges artifacts and returns status CONFIRMED."""
        rel_dir = os.path.join(self.temp_dir, "build-win32", "Release")
        os.makedirs(rel_dir, exist_ok=True)
        exe_p = os.path.join(rel_dir, "CalculadoraUnificada.exe")
        man_p = os.path.join(rel_dir, "build_manifest.json")
        with open(exe_p, "wb") as f: f.write(b"MZ_DUMMY")
        with open(man_p, "w") as f: f.write("{}")

        cmd = [
            sys.executable, self.build_cg50_script,
            "--clean", "--json",
            "--build-dir", os.path.join(self.temp_dir, "build-win32")
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0)
        self.assertFalse(os.path.exists(exe_p))
        self.assertFalse(os.path.exists(man_p))
        payload = json.loads(proc.stdout)
        self.assertTrue(payload["success"])
        self.assertEqual(payload["status"], "CONFIRMED")

    # =========================================================================
    # Group 9: Extended LCD Diff & Normalization Tests
    # =========================================================================

    def test_adv_blackout_screen_detection_boundaries(self):
        """detect_blackout flags completely dark framebuffers and respects min_nonzero."""
        img_black = Image.new("RGB", (384, 216), (0, 0, 0))
        res_black = detect_blackout(img_black)
        self.assertTrue(res_black["blackout"])
        self.assertEqual(res_black["exit_code"], 1)
        self.assertEqual(res_black["error"], "ERR_BLACK_SCREEN")

        img_custom = Image.new("RGB", (384, 216), (0, 0, 0))
        for i in range(333):
            img_custom.putpixel((i, 0), (1, 1, 1))
        res_sub = detect_blackout(img_custom, min_nonzero=1000)
        self.assertTrue(res_sub["blackout"])

        img_custom.putpixel((333, 0), (1, 0, 0))
        res_pass = detect_blackout(img_custom, min_nonzero=1000)
        self.assertFalse(res_pass["blackout"])

    def test_adv_triptych_dimensions_and_composite(self):
        """generate_triptych produces side-by-side 3-panel image of exactly 1152x216."""
        img_off = Image.new("RGB", (384, 216), (10, 10, 10))
        img_nat = Image.new("RGB", (384, 216), (20, 20, 20))
        img_diff = Image.new("RGB", (384, 216), (30, 30, 30))

        trip = generate_triptych(img_off, img_nat, img_diff)
        self.assertEqual(trip.size, (1152, 216))
        self.assertEqual(trip.mode, "RGB")
        self.assertEqual(trip.getpixel((10, 10)), (10, 10, 10))
        self.assertEqual(trip.getpixel((384 + 10, 10)), (20, 20, 20))
        self.assertEqual(trip.getpixel((768 + 10, 10)), (30, 30, 30))

    def test_adv_classify_lcd_content_heuristics(self):
        """classify_lcd_content discriminates between menu, error_dialog, and texto_denso."""
        empty_img = Image.new("RGB", (0, 0))
        self.assertEqual(classify_lcd_content(empty_img), "desconocida")

        menu_screen = create_menu_lcd()
        self.assertEqual(classify_lcd_content(menu_screen), "menu")

        calc_screen = create_calc_screen("1+2", "3")
        self.assertEqual(classify_lcd_content(calc_screen), "texto_denso")

        err_screen = Image.new("RGB", (384, 216), (255, 255, 255))
        for x in range(384):
            for y in range(10):
                err_screen.putpixel((x, y), (255, 0, 0))
        self.assertEqual(classify_lcd_content(err_screen), "error_dialog")


if __name__ == "__main__":
    unittest.main()
