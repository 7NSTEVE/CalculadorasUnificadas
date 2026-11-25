#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_adversarial_m2_stress.py — Empirical Challenger Adversarial Stress Harness for Milestone 2.
Plataforma Integrada de Pruebas Diferenciales (Calculadora Unificada).

Covers exhaustive empirical challenges:
1. LCD diff stress:
   - Subthreshold noise: color delta <= 4 must be ignored (changed_pixels == 0, PASS).
   - Suprathreshold noise: color delta > 4 must be detected as divergence.
   - 180° rotation inversion: compare upright vs upside-down frames; verify rotation compensation.
   - Dimension mismatch: verify safe error handling (TAMANOS_DISTINTOS).
   - Blackout detection: verify ERR_BLACK_SCREEN on all-black images (< 1000 non-zero pixels) vs populated screen.
2. Divergence localization stress:
   - First step divergence vs last step divergence.
   - Fail-fast enforcement (halt immediately on first divergence).
   - Memory window bit-flip isolation: verify exact byte offset and address extraction on memory mismatch.
   - Instruction trace stress: identical, divergent, and empty trace files.
3. Visual Gallery & CLI validation:
   - Failure jump link banner targeting first divergence step.
   - CLI pacing and key validation gates.
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from typing import Tuple, Dict, Any, List
from PIL import Image, ImageChops

# Locate project root and tools
TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(TESTS_DIR)
TOOLS_DIR = os.path.join(PROJECT_ROOT, "tools")
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

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
import cg50_agent


class TestAdversarialLcdNoise(unittest.TestCase):
    """Stress-tests LCD differential threshold boundaries and noise tolerance."""

    def test_adv_01_subthreshold_noise_individual_channels_ignored(self):
        """
        ATTACK: Inject color deltas of 1, 2, 3, 4 into individual R, G, B channels
        across various background colors (white, black, mid-gray, primary colors).
        VERIFY: calculate_pixel_diff ignores all deltas <= 4 (changed_pixels == 0, PASS).
        """
        base_colors = [
            (0, 0, 0),        # black
            (255, 255, 255),  # white
            (128, 128, 128),  # gray
            (200, 50, 50),    # red-ish
            (50, 200, 50),    # green-ish
            (50, 50, 200),    # blue-ish
        ]

        for base in base_colors:
            img_a = Image.new("RGB", CANONICAL_SIZE, base)
            # Test deltas 1, 2, 3, 4
            for delta in [1, 2, 3, 4]:
                # Test positive delta if space permits, else negative
                r_mod = base[0] + delta if base[0] + delta <= 255 else base[0] - delta
                g_mod = base[1] + delta if base[1] + delta <= 255 else base[1] - delta
                b_mod = base[2] + delta if base[2] + delta <= 255 else base[2] - delta

                for mod_color in [(r_mod, base[1], base[2]),
                                  (base[0], g_mod, base[2]),
                                  (base[0], base[1], b_mod)]:
                    img_b = img_a.copy()
                    # Modify 500 scattered pixels
                    px = img_b.load()
                    for i in range(500):
                        px[i % CANONICAL_WIDTH, (i * 7) % CANONICAL_HEIGHT] = mod_color

                    res = calculate_pixel_diff(img_a, img_b, threshold=4)
                    self.assertEqual(res["changed_pixels"], 0,
                                     f"Delta {delta} on base {base} was incorrectly flagged as divergence: {res}")
                    self.assertEqual(res["status"], "MATCH")
                    self.assertTrue(res["match"])

    def test_adv_02_subthreshold_noise_multi_channel_ignored(self):
        """
        ATTACK: Inject simultaneous deltas across all 3 channels where max(dR, dG, dB) <= 4.
        VERIFY: Must be completely ignored as DAC/quantization noise (changed_pixels == 0, MATCH).
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (100, 100, 100))
        img_b = img_a.copy()
        px = img_b.load()

        # Modify a 50x50 block with max channel delta == 4
        for y in range(20, 70):
            for x in range(30, 80):
                px[x, y] = (104, 96, 103)  # deltas: (+4, -4, +3), max delta is 4

        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 0)
        self.assertEqual(res["status"], "MATCH")
        self.assertTrue(res["match"])

    def test_adv_FINDING_diff_bbox_unthresholded_leakage(self):
        """
        EMPIRICAL CHALLENGE FINDING (calculate_pixel_diff):
        Demonstrates that diff_bbox is extracted from diff.getbbox() BEFORE applying threshold.
        Case 1: When only subthreshold noise is present (delta <= 4), changed_pixels is 0,
        but diff_bbox is NOT None (it returns bounding box of noise).
        Case 2: When 1 pixel diverges at (100, 100) and delta=1 noise is at (0, 0),
        diff_bbox expands to (0, 0, 101, 101) instead of (100, 100, 101, 101).
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (0, 0, 0))
        img_b = img_a.copy()
        img_b.putpixel((0, 0), (1, 0, 0))  # Subthreshold delta = 1
        res_sub = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res_sub["changed_pixels"], 0)
        self.assertEqual(res_sub["status"], "MATCH")
        # Defect demonstration: diff_bbox is not None even though changed_pixels == 0
        self.assertEqual(res_sub["diff_bbox"], (0, 0, 1, 1))

        # Add suprathreshold divergence at (100, 100)
        img_b.putpixel((100, 100), (10, 0, 0))  # Suprathreshold delta = 10
        res_both = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res_both["changed_pixels"], 1)
        self.assertEqual(res_both["status"], "DIFF")
        # Defect demonstration: diff_bbox encompasses subthreshold noise at (0,0)
        self.assertEqual(res_both["diff_bbox"], (0, 0, 101, 101))

    def test_adv_03_suprathreshold_noise_minimal_divergence_detected(self):
        """
        ATTACK: Inject minimal suprathreshold delta = 5 into a single pixel at (120, 80).
        VERIFY: calculate_pixel_diff strictly detects exactly 1 changed pixel,
        returns status DIFF, match=False, and exact bounding box (120, 80, 121, 81).
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (100, 100, 100))
        img_b = img_a.copy()
        img_b.putpixel((120, 80), (105, 100, 100))  # delta = 5 on Red channel

        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 1)
        self.assertEqual(res["status"], "DIFF")
        self.assertFalse(res["match"])
        self.assertEqual(res["diff_bbox"], (120, 80, 121, 81))

    def test_adv_04_suprathreshold_noise_spread_and_bbox_accuracy(self):
        """
        ATTACK: Inject deltas 6, 10, 50, 255 into 4 boundary pixels:
        (10, 20), (300, 20), (10, 180), (300, 180).
        VERIFY: Changed pixels == 4, and bounding box spans exactly (10, 20, 301, 181).
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (0, 0, 0))
        img_b = img_a.copy()
        img_b.putpixel((10, 20), (6, 0, 0))     # delta = 6
        img_b.putpixel((300, 20), (0, 10, 0))    # delta = 10
        img_b.putpixel((10, 180), (0, 0, 50))    # delta = 50
        img_b.putpixel((300, 180), (255, 255, 255)) # delta = 255

        res = calculate_pixel_diff(img_a, img_b, threshold=4)
        self.assertEqual(res["changed_pixels"], 4)
        self.assertEqual(res["status"], "DIFF")
        self.assertFalse(res["match"])
        self.assertEqual(res["diff_bbox"], (10, 20, 301, 181))

    def test_adv_05_magenta_diff_highlight_and_dimming(self):
        """
        ATTACK: Verify generate_magenta_diff_image paints differing pixels with exact
        RGB(255, 0, 255) while keeping non-differing pixels intact (or dimmed grayscale).
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (200, 150, 100))
        img_b = img_a.copy()
        # Invert a 10x10 block
        for y in range(10, 20):
            for x in range(10, 20):
                img_b.putpixel((x, y), (0, 0, 0))

        diff_img = generate_magenta_diff_image(img_a, img_b, threshold=4, dim_background=False)
        px_diff = diff_img.load()

        # Differing region must be pure magenta
        for y in range(10, 20):
            for x in range(10, 20):
                self.assertEqual(px_diff[x, y], COLOR_MAGENTA)

        # Unmodified region must match img_a
        self.assertEqual(px_diff[0, 0], (200, 150, 100))

        # Test dim_background=True
        diff_dim = generate_magenta_diff_image(img_a, img_b, threshold=4, dim_background=True)
        px_dim = diff_dim.load()
        self.assertEqual(px_dim[15, 15], COLOR_MAGENTA)
        # Background pixel should be monochrome gray
        gray_val = px_dim[0, 0]
        self.assertEqual(gray_val[0], gray_val[1])
        self.assertEqual(gray_val[1], gray_val[2])

    def test_adv_06_custom_threshold_boundaries(self):
        """
        ATTACK: Pass custom threshold (e.g. threshold = 10).
        VERIFY: Delta 10 is ignored, Delta 11 triggers divergence.
        """
        img_a = Image.new("RGB", CANONICAL_SIZE, (100, 100, 100))
        img_b = img_a.copy()
        img_b.putpixel((50, 50), (110, 100, 100))  # delta = 10

        res_10 = calculate_pixel_diff(img_a, img_b, threshold=10)
        self.assertEqual(res_10["changed_pixels"], 0)
        self.assertEqual(res_10["status"], "MATCH")

        img_b.putpixel((50, 50), (111, 100, 100))  # delta = 11
        res_11 = calculate_pixel_diff(img_a, img_b, threshold=10)
        self.assertEqual(res_11["changed_pixels"], 1)
        self.assertEqual(res_11["status"], "DIFF")


class TestAdversarialLcdRotationAndGeometry(unittest.TestCase):
    """Stress-tests 180° rotation inversion and dimension mismatch handling."""

    def test_adv_07_asymmetric_180_rotation_diverges_without_compensation(self):
        """
        ATTACK: Create an asymmetric LCD image (colored block in top-left, blank in bottom-right).
        Generate an inverted (upside-down) version.
        VERIFY: Without rotation compensation, calculate_pixel_diff reports severe divergence.
        """
        img_upright = Image.new("RGB", CANONICAL_SIZE, (255, 255, 255))
        # Top-left colored patch
        for y in range(0, 50):
            for x in range(0, 100):
                img_upright.putpixel((x, y), (0, 0, 255))

        img_upside_down = img_upright.transpose(Image.Transpose.ROTATE_180)

        # Direct comparison without rotation compensation
        res = calculate_pixel_diff(img_upright, img_upside_down, threshold=4)
        self.assertGreater(res["changed_pixels"], 5000)
        self.assertEqual(res["status"], "DIFF")
        self.assertFalse(res["match"])

    def test_adv_08_asymmetric_180_rotation_passes_with_compensation(self):
        """
        ATTACK: Normalize the upside-down image using normalize_lcd with backend='native' and rotate_native=True.
        VERIFY: The normalized native frame matches the upright official frame with 0 changed pixels.
        """
        img_upright = Image.new("RGB", CANONICAL_SIZE, (255, 255, 255))
        for y in range(0, 50):
            for x in range(0, 100):
                img_upright.putpixel((x, y), (0, 0, 255))

        img_upside_down = img_upright.transpose(Image.Transpose.ROTATE_180)

        norm_official = normalize_lcd(img_upright, backend="official", rotate_native=False)
        norm_native = normalize_lcd(img_upside_down, backend="native", rotate_native=True)

        res = calculate_pixel_diff(norm_official, norm_native, threshold=4)
        self.assertEqual(res["changed_pixels"], 0)
        self.assertEqual(res["status"], "MATCH")
        self.assertTrue(res["match"])
        self.assertIsNone(res["diff_bbox"])

    def test_adv_09_dimension_mismatch_safe_error_handling(self):
        """
        ATTACK: Compare images of incompatible sizes (384x216 vs 384x215, 383x216, 500x300).
        VERIFY: calculate_pixel_diff returns structured TAMANOS_DISTINTOS dictionary safely
        without raising uncaught exceptions.
        """
        mismatched_pairs = [
            ((384, 216), (384, 215)),
            ((384, 216), (383, 216)),
            ((384, 216), (500, 300)),
            ((100, 100), (200, 200)),
        ]

        for sz_a, sz_b in mismatched_pairs:
            img_a = Image.new("RGB", sz_a, (128, 128, 128))
            img_b = Image.new("RGB", sz_b, (128, 128, 128))

            res = calculate_pixel_diff(img_a, img_b)
            self.assertEqual(res.get("error"), "TAMANOS_DISTINTOS")
            self.assertEqual(res.get("status"), "DIFF")
            self.assertFalse(res.get("match"))
            self.assertEqual(res.get("size_a"), sz_a)
            self.assertEqual(res.get("size_b"), sz_b)

    def test_adv_10_normalization_resizes_arbitrary_inputs_to_canonical(self):
        """
        ATTACK: Pass non-canonical resolutions (800x480, 1920x1080, 50x50, RGBA) to normalize_lcd.
        VERIFY: Output image is strictly CANONICAL_SIZE (384, 216) and mode RGB.
        """
        test_inputs = [
            Image.new("RGBA", (800, 480), (255, 0, 0, 255)),
            Image.new("L", (1920, 1080), 128),
            Image.new("RGB", (50, 50), (0, 255, 0)),
        ]

        for inp in test_inputs:
            norm = normalize_lcd(inp, backend="official")
            self.assertEqual(norm.size, CANONICAL_SIZE)
            self.assertEqual(norm.mode, "RGB")


class TestAdversarialBlackoutDetection(unittest.TestCase):
    """Stress-tests blackout screen detection and edge boundaries."""

    def test_adv_11_all_black_screen_triggers_err_black_screen(self):
        """
        ATTACK: Provide fully black framebuffer (all 0x00 bytes).
        VERIFY: detect_blackout returns blackout=True, exit_code=1, error='ERR_BLACK_SCREEN'.
        """
        black_img = Image.new("RGB", CANONICAL_SIZE, (0, 0, 0))
        res = detect_blackout(black_img, min_nonzero=1000)
        self.assertTrue(res["blackout"])
        self.assertEqual(res["exit_code"], 1)
        self.assertEqual(res["error"], "ERR_BLACK_SCREEN")
        self.assertEqual(res["nonzero_pixels"], 0)

    def test_adv_12_near_black_screens_below_threshold(self):
        """
        ATTACK: Test near-black images with 10, 500, and 999 non-zero bytes (< 1000 threshold).
        VERIFY: detect_blackout identifies all as blackouts.
        """
        for count in [10, 500, 999]:
            img = Image.new("RGB", CANONICAL_SIZE, (0, 0, 0))
            raw = bytearray(CANONICAL_WIDTH * CANONICAL_HEIGHT * 3)
            for i in range(count):
                raw[i] = 128
            img = Image.frombytes("RGB", CANONICAL_SIZE, bytes(raw))

            res = detect_blackout(img, min_nonzero=1000)
            self.assertTrue(res["blackout"], f"Failed to detect blackout with {count} nonzero bytes")
            self.assertEqual(res["exit_code"], 1)
            self.assertEqual(res["error"], "ERR_BLACK_SCREEN")
            self.assertEqual(res["nonzero_pixels"], count)

    def test_adv_13_exact_threshold_and_above_boundary(self):
        """
        ATTACK: Test boundary at exactly 1000 and 1001 non-zero bytes.
        VERIFY: At exactly 1000 and above, blackout is False (exit_code=0).
        """
        # 1000 non-zero bytes
        raw_1000 = bytearray(CANONICAL_WIDTH * CANONICAL_HEIGHT * 3)
        for i in range(1000):
            raw_1000[i] = 200
        img_1000 = Image.frombytes("RGB", CANONICAL_SIZE, bytes(raw_1000))
        res_1000 = detect_blackout(img_1000, min_nonzero=1000)
        self.assertFalse(res_1000["blackout"])
        self.assertEqual(res_1000["exit_code"], 0)
        self.assertEqual(res_1000["nonzero_pixels"], 1000)

        # 1001 non-zero bytes
        raw_1001 = bytearray(CANONICAL_WIDTH * CANONICAL_HEIGHT * 3)
        for i in range(1001):
            raw_1001[i] = 200
        img_1001 = Image.frombytes("RGB", CANONICAL_SIZE, bytes(raw_1001))
        res_1001 = detect_blackout(img_1001, min_nonzero=1000)
        self.assertFalse(res_1001["blackout"])
        self.assertEqual(res_1001["exit_code"], 0)
        self.assertEqual(res_1001["nonzero_pixels"], 1001)

    def test_adv_14_populated_casio_screen_not_blackout(self):
        """
        ATTACK: Normal Casio UI framebuffer (thousands of non-zero pixels).
        VERIFY: blackout is False, exit_code is 0.
        """
        img = Image.new("RGB", CANONICAL_SIZE, (255, 255, 255))
        # Draw status bar
        px = img.load()
        for x in range(CANONICAL_WIDTH):
            for y in range(20):
                px[x, y] = (0, 0, 150)

        res = detect_blackout(img)
        self.assertFalse(res["blackout"])
        self.assertEqual(res["exit_code"], 0)


class TestAdversarialKeystrokeBisectionAndFailFast(unittest.TestCase):
    """Stress-tests step-by-step bisection, first vs last step divergence, and fail-fast."""

    def test_adv_15_first_step_divergence_halts_immediately(self):
        """
        ATTACK: Inject divergence at step 1 of 5 keys: ['1', '+', '2', '=', 'AC'].
        Configure DivergenceLocator with stop_on_first=True.
        VERIFY: Fail-fast halts execution immediately (steps_executed == 1),
        first_divergence['step'] == 1, verdict is DIVERGENCIA, exit code is 1.
        """
        keys = "1, +, 2, =, AC"
        executed_steps = []

        def mock_executor(step_idx, key):
            executed_steps.append((step_idx, key))
            if step_idx == 1:
                return (
                    {"pc": "0x80010000", "crashed": False},
                    {"pc": "0x80010000", "crashed": False},
                    {
                        "status": "DIFF",
                        "pixel_diff": {
                            "changed_pixels": 45,
                            "changed_percent": 0.054,
                            "diff_bbox": [10, 10, 30, 30]
                        }
                    }
                )
            return (
                {"pc": "0x80010000", "crashed": False},
                {"pc": "0x80010000", "crashed": False},
                {"status": "MATCH", "pixel_diff": {"changed_pixels": 0}}
            )

        locator = DivergenceLocator(stop_on_first=True)
        code, report = locator.bisect_keystrokes(keys, mock_executor, {"status": "SUCCESS"})

        self.assertEqual(code, 1)
        self.assertEqual(report["verdict"], "DIVERGENCIA")
        self.assertEqual(report["total_steps"], 5)
        self.assertEqual(report["steps_executed"], 1, "Fail-fast did not halt at step 1!")
        self.assertEqual(len(executed_steps), 1)
        self.assertIsNotNone(report["first_divergence"])
        self.assertEqual(report["first_divergence"]["step"], 1)
        self.assertEqual(report["first_divergence"]["key"], "1")
        self.assertEqual(report["first_divergence"]["changed_pixels"], 45)

    def test_adv_16_last_step_divergence_completes_all_preceding(self):
        """
        ATTACK: Steps 1..4 match, step 5 of 5 diverges.
        VERIFY: steps_executed == 5, first_divergence['step'] == 5, triggering key == 'AC'.
        """
        keys = "1, +, 2, =, AC"
        executed_steps = []

        def mock_executor(step_idx, key):
            executed_steps.append((step_idx, key))
            if step_idx == 5:
                return (
                    {"pc": "0x80010050", "crashed": False},
                    {"pc": "0x80010050", "crashed": False},
                    {
                        "status": "DIFF",
                        "pixel_diff": {"changed_pixels": 12, "diff_bbox": [5, 5, 10, 10]}
                    }
                )
            return (
                {"pc": "0x80010000", "crashed": False},
                {"pc": "0x80010000", "crashed": False},
                {"status": "MATCH", "pixel_diff": {"changed_pixels": 0}}
            )

        locator = DivergenceLocator(stop_on_first=True)
        code, report = locator.bisect_keystrokes(keys, mock_executor, {"status": "SUCCESS"})

        self.assertEqual(code, 1)
        self.assertEqual(report["verdict"], "DIVERGENCIA")
        self.assertEqual(report["steps_executed"], 5)
        self.assertEqual(len(executed_steps), 5)
        self.assertEqual(report["first_divergence"]["step"], 5)
        self.assertEqual(report["first_divergence"]["key"], "AC")

    def test_adv_17_no_fail_fast_records_all_steps_and_first_divergence(self):
        """
        ATTACK: Step 2 diverges, with stop_on_first=False.
        VERIFY: All 5 steps execute (steps_executed == 5), but first_divergence still
        strictly isolates step 2.
        """
        keys = "1, +, 2, =, AC"
        executed_steps = []

        def mock_executor(step_idx, key):
            executed_steps.append((step_idx, key))
            if step_idx in (2, 4):
                return (
                    {"pc": "0x80010000", "crashed": False},
                    {"pc": "0x80010000", "crashed": False},
                    {
                        "status": "DIFF",
                        "pixel_diff": {"changed_pixels": 20}
                    }
                )
            return (
                {"pc": "0x80010000", "crashed": False},
                {"pc": "0x80010000", "crashed": False},
                {"status": "MATCH", "pixel_diff": {"changed_pixels": 0}}
            )

        locator = DivergenceLocator(stop_on_first=False)
        code, report = locator.bisect_keystrokes(keys, mock_executor, {"status": "SUCCESS"})

        self.assertEqual(code, 1)
        self.assertEqual(report["steps_executed"], 5)
        self.assertEqual(len(executed_steps), 5)
        self.assertEqual(len(report["steps"]), 5)
        self.assertEqual(report["first_divergence"]["step"], 2)
        self.assertEqual(report["first_divergence"]["key"], "+")

    def test_adv_18_empty_key_sequence_returns_pass_code_0(self):
        """
        ATTACK: Key sequence is empty or whitespace/commas only.
        VERIFY: steps_executed == 0, report verdict is PASS, exit code is 0.
        """
        for empty_seq in ["", "   ", " , , ,  "]:
            locator = DivergenceLocator()
            code, report = locator.bisect_keystrokes(empty_seq, lambda s, k: ({}, {}, {}), {})
            self.assertEqual(code, 0)
            self.assertEqual(report["verdict"], "PASS")
            self.assertEqual(report["steps_executed"], 0)
            self.assertIsNone(report["first_divergence"])

    def test_adv_19_divergence_taxonomy_classification_hierarchy(self):
        """
        ATTACK: Verify divergence type classification order:
        1. CRASH -> DivergenceType.CRASH
        2. pc_match=False -> DivergenceType.REGISTER_MISMATCH
        3. memory_discrepancy -> DivergenceType.MEMORY_MISMATCH
        4. default -> DivergenceType.LCD_PIXEL_MISMATCH
        """
        locator = DivergenceLocator()

        # 1. Crash
        _, rep_crash = locator.bisect_keystrokes(
            "1",
            lambda s, k: ({"crashed": True}, {}, {"status": "DIFF"}),
            {}
        )
        self.assertEqual(rep_crash["first_divergence"]["divergence_type"], DivergenceType.CRASH.value)

        # 2. Register mismatch
        _, rep_reg = locator.bisect_keystrokes(
            "1",
            lambda s, k: ({"pc": "0x100"}, {"pc": "0x200"}, {"status": "DIFF", "state_diff": {"pc_match": False}}),
            {}
        )
        self.assertEqual(rep_reg["first_divergence"]["divergence_type"], DivergenceType.REGISTER_MISMATCH.value)

        # 3. Memory mismatch
        _, rep_mem = locator.bisect_keystrokes(
            "1",
            lambda s, k: (
                {"pc": "0x100"}, {"pc": "0x100"},
                {"status": "DIFF", "state_diff": {"pc_match": True}, "memory_discrepancy": {"offset": 10}}
            ),
            {}
        )
        self.assertEqual(rep_mem["first_divergence"]["divergence_type"], DivergenceType.MEMORY_MISMATCH.value)

        # 4. LCD pixel mismatch
        _, rep_lcd = locator.bisect_keystrokes(
            "1",
            lambda s, k: (
                {"pc": "0x100"}, {"pc": "0x100"},
                {"status": "DIFF", "state_diff": {"pc_match": True}, "pixel_diff": {"changed_pixels": 8}}
            ),
            {}
        )
        self.assertEqual(rep_lcd["first_divergence"]["divergence_type"], DivergenceType.LCD_PIXEL_MISMATCH.value)


class TestAdversarialMemoryWindowBitFlipIsolation(unittest.TestCase):
    """Stress-tests memory window bit-flip isolation across History, Work, Display buffers."""

    def test_adv_20_history_buffer_single_bit_flip_at_offset_0(self):
        """
        ATTACK: Invert a single bit at byte offset 0 of History buffer (base 0x880D3400).
        VERIFY: isolate_memory_divergence isolates offset=0, address=0x880D3400,
        exact native vs official byte, and valid hex context.
        """
        base_addr, size = CASIO_MEM_WINDOWS["History"]
        nat = bytearray(b"\xAA" * size)
        off = bytearray(b"\xAA" * size)
        # Flip bit 0 of first byte
        nat[0] = nat[0] ^ 0x01  # 0xAA -> 0xAB

        disc = DivergenceLocator.isolate_memory_divergence("History", base_addr, bytes(nat), bytes(off))
        self.assertIsNotNone(disc)
        self.assertEqual(disc["buffer_name"], "History")
        self.assertEqual(disc["base_address"], "0x880D3400")
        self.assertEqual(disc["divergent_address"], "0x880D3400")
        self.assertEqual(disc["offset"], 0)
        self.assertEqual(disc["native_byte"], "0xAB")
        self.assertEqual(disc["official_byte"], "0xAA")
        self.assertEqual(disc["total_bytes_compared"], size)

    def test_adv_21_work_buffer_single_bit_flip_at_middle_offset(self):
        """
        ATTACK: Invert a single bit at offset 1024 of Work buffer (base 0x88184D00).
        VERIFY: offset=1024, divergent_address=0x88185100.
        """
        base_addr, size = CASIO_MEM_WINDOWS["Work"]
        nat = bytearray(b"\x00" * size)
        off = bytearray(b"\x00" * size)
        nat[1024] = 0x42

        disc = DivergenceLocator.isolate_memory_divergence("Work", base_addr, bytes(nat), bytes(off))
        self.assertIsNotNone(disc)
        self.assertEqual(disc["offset"], 1024)
        self.assertEqual(disc["divergent_address"], f"0x{base_addr + 1024:08X}")
        self.assertEqual(disc["native_byte"], "0x42")
        self.assertEqual(disc["official_byte"], "0x00")

    def test_adv_22_display_buffer_single_bit_flip_at_last_byte(self):
        """
        ATTACK: Invert a single bit at last byte (offset 1023) of Display buffer (base 0x8804F800).
        VERIFY: offset=1023, divergent_address=0x8804FBFF.
        """
        base_addr, size = CASIO_MEM_WINDOWS["Display"]
        nat = bytearray(b"\x20" * size)
        off = bytearray(b"\x20" * size)
        last_idx = size - 1
        nat[last_idx] = 0x21

        disc = DivergenceLocator.isolate_memory_divergence("Display", base_addr, bytes(nat), bytes(off))
        self.assertIsNotNone(disc)
        self.assertEqual(disc["offset"], last_idx)
        self.assertEqual(disc["divergent_address"], f"0x{base_addr + last_idx:08X}")
        self.assertEqual(disc["native_byte"], "0x21")
        self.assertEqual(disc["official_byte"], "0x20")

    def test_adv_23_multi_window_search_skips_matching_buffers(self):
        """
        ATTACK: Pass matching History buffer and corrupted Work buffer to compare_memory_windows.
        VERIFY: Locator skips History and correctly pinpoints the Work buffer discrepancy.
        """
        hist_size = CASIO_MEM_WINDOWS["History"][1]
        work_size = CASIO_MEM_WINDOWS["Work"][1]

        nat_buffers = {
            "History": b"\x00" * hist_size,
            "Work": b"\x00" * 50 + b"\xFF" + b"\x00" * (work_size - 51)
        }
        off_buffers = {
            "History": b"\x00" * hist_size,
            "Work": b"\x00" * work_size
        }

        disc = DivergenceLocator.compare_memory_windows(nat_buffers, off_buffers)
        self.assertIsNotNone(disc)
        self.assertEqual(disc["buffer_name"], "Work")
        self.assertEqual(disc["offset"], 50)
        self.assertEqual(disc["divergent_address"], f"0x{CASIO_MEM_WINDOWS['Work'][0] + 50:08X}")

    def test_adv_24_identical_buffers_return_none(self):
        """
        ATTACK: Pass identical buffers across all windows.
        VERIFY: Returns None.
        """
        buffers = {
            k: b"\x00" * size for k, (addr, size) in CASIO_MEM_WINDOWS.items()
        }
        disc = DivergenceLocator.compare_memory_windows(buffers, buffers)
        self.assertIsNone(disc)


class TestAdversarialInstructionTraceStress(unittest.TestCase):
    """Stress-tests instruction trace alignment, PC/Opcode discrepancies, and empty inputs."""

    def test_adv_25_identical_instruction_traces_pass(self):
        """
        ATTACK: Compare 10 identical instruction records.
        VERIFY: status is EQUAL, first_divergence is None, total_compared == 10.
        """
        nat_lines = [f"PC=0x8800{i:04X} OP=0x0009" for i in range(10)]
        off_records = [{"guest_pc_ebp": f"0x8800{i:04X}", "opcode": "0x0009"} for i in range(10)]

        res = DivergenceLocator.compare_instruction_traces(nat_lines, off_records)
        self.assertEqual(res["status"], "EQUAL")
        self.assertIsNone(res["first_divergence"])
        self.assertEqual(res["total_compared"], 10)

    def test_adv_26_pc_mismatch_at_first_instruction(self):
        """
        ATTACK: Instruction #1 has PC discrepancy.
        VERIFY: status is DIFF, ordinal is 1, exact PCs captured.
        """
        nat = ["PC=0x88000100 OP=0x0009", "PC=0x88000102 OP=0x0009"]
        off = [
            {"guest_pc_ebp": "0x88000104", "opcode": "0x0009"},
            {"guest_pc_ebp": "0x88000102", "opcode": "0x0009"}
        ]

        res = DivergenceLocator.compare_instruction_traces(nat, off)
        self.assertEqual(res["status"], "DIFF")
        self.assertEqual(res["first_divergence"]["ordinal"], 1)
        self.assertEqual(res["first_divergence"]["native_pc"], "0x88000100")
        self.assertEqual(res["first_divergence"]["official_pc"], "0x88000104")

    def test_adv_27_opcode_mismatch_with_identical_pc(self):
        """
        ATTACK: PC matches, but opcode deviates at instruction #3.
        VERIFY: status is DIFF, ordinal is 3, exact opcodes captured.
        """
        nat = [
            "PC=0x88000100 OP=0x0009",
            "PC=0x88000102 OP=0x0009",
            "PC=0x88000104 OP=0x310C"
        ]
        off = [
            {"guest_pc_ebp": "0x88000100", "opcode": "0x0009"},
            {"guest_pc_ebp": "0x88000102", "opcode": "0x0009"},
            {"guest_pc_ebp": "0x88000104", "opcode": "0x310D"}
        ]

        res = DivergenceLocator.compare_instruction_traces(nat, off)
        self.assertEqual(res["status"], "DIFF")
        self.assertEqual(res["first_divergence"]["ordinal"], 3)
        self.assertEqual(res["first_divergence"]["native_op"], "0x310C")
        self.assertEqual(res["first_divergence"]["official_op"], "0x310D")

    def test_adv_28_hex_casing_and_prefix_resilience(self):
        """
        ATTACK: Native trace uses lowercase '0x88000abc', official uses uppercase without prefix '88000ABC'.
        VERIFY: Comparator normalizes hex format and does NOT flag false divergence.
        """
        nat = ["PC=0x88000abc OP=0x00ff"]
        off = [{"guest_pc_ebp": "88000ABC", "opcode": "00FF"}]

        res = DivergenceLocator.compare_instruction_traces(nat, off)
        self.assertEqual(res["status"], "EQUAL")
        self.assertIsNone(res["first_divergence"])

    def test_adv_29_empty_traces_safe_handling(self):
        """
        ATTACK: Test empty native lines, empty official records, and both empty.
        VERIFY: Gracefully returns structured ERR_TRACE_EMPTY with exit_code=2.
        """
        test_cases = [
            ([], [{"guest_pc_ebp": "0x100", "opcode": "0x0"}]),
            (["PC=0x100 OP=0x0"], []),
            ([], [])
        ]

        for nat, off in test_cases:
            res = DivergenceLocator.compare_instruction_traces(nat, off)
            self.assertEqual(res["status"], "ERROR")
            self.assertEqual(res["exit_code"], 2)
            self.assertEqual(res["error"], "ERR_TRACE_EMPTY")


class TestAdversarialVisualGalleryAndPacingCLI(unittest.TestCase):
    """Stress-tests Visual Gallery failure jump links and CLI pacing gates."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="adv_gallery_")

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_adv_30_gallery_jump_banner_first_step_anchor(self):
        """
        ATTACK: Render gallery where first_divergence is step 1.
        VERIFY: Jump banner exists and points to '#step-1'.
        """
        gallery = VisualGallery(self.temp_dir)
        gallery.set_metadata(
            verdict="DIVERGENCIA",
            first_divergence={"step": 1, "key": "1", "changed_pixels": 50, "changed_percent": 0.06}
        )
        gallery.add_step(1, "1", {"status": "DIFF", "changed_pixels": 50, "match": False})
        html_code = gallery.render_html()

        self.assertIn("PRIMERA DIVERGENCIA DETECTADA", html_code)
        self.assertIn('href="#step-1"', html_code)

    def test_adv_31_gallery_jump_banner_last_step_anchor(self):
        """
        ATTACK: Render gallery where first_divergence is step 5.
        VERIFY: Jump banner points to '#step-5'.
        """
        gallery = VisualGallery(self.temp_dir)
        gallery.set_metadata(
            verdict="DIVERGENCIA",
            first_divergence={"step": 5, "key": "AC", "changed_pixels": 30, "changed_percent": 0.04}
        )
        for i in range(1, 5):
            gallery.add_step(i, str(i), {"status": "MATCH", "changed_pixels": 0, "match": True})
        gallery.add_step(5, "AC", {"status": "DIFF", "changed_pixels": 30, "match": False})
        html_code = gallery.render_html()

        self.assertIn("PRIMERA DIVERGENCIA DETECTADA", html_code)
        self.assertIn('href="#step-5"', html_code)

    def test_adv_32_gallery_pass_omits_jump_banner(self):
        """
        ATTACK: Render gallery for a passing test run (verdict=PASS, first_divergence=None).
        VERIFY: No divergence alert or jump banner is present in the rendered HTML.
        """
        gallery = VisualGallery(self.temp_dir)
        gallery.set_metadata(verdict="PASS", first_divergence=None)
        gallery.add_step(1, "1", {"status": "MATCH", "changed_pixels": 0, "match": True})
        html_code = gallery.render_html()

        self.assertNotIn("PRIMERA DIVERGENCIA DETECTADA", html_code)
        self.assertNotIn('<div class="alert alert-divergence">', html_code)
        self.assertNotIn('class="btn-jump"', html_code)

    def test_adv_33_cli_rejects_invalid_key_token_code_2(self):
        """
        ATTACK: Invoke cg50_agent.py diff with unmapped key token 'INVALID_TOKEN'.
        VERIFY: Aborts immediately with exit code 2 and structured ERR_DESYNC_KEY report.
        """
        cg50_py = os.path.join(TOOLS_DIR, "cg50_agent.py")
        cmd = [sys.executable, cg50_py, "diff", "--keys", "INVALID_TOKEN", "--rebuild"]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        self.assertEqual(proc.returncode, 2)
        data = json.loads(proc.stdout)
        self.assertEqual(data.get("error"), "ERR_DESYNC_KEY")
        self.assertEqual(data.get("verdict"), "FAIL")

    def test_adv_34_cli_rejects_unsafe_pacing_code_2(self):
        """
        ATTACK: Invoke cg50_agent.py diff with hold_ms=100 without --allow-short-holds.
        VERIFY: Aborts immediately with exit code 2 and structured ERR_UNSAFE_PACING report.
        """
        cg50_py = os.path.join(TOOLS_DIR, "cg50_agent.py")
        cmd = [sys.executable, cg50_py, "diff", "--keys", "1", "--hold-ms", "100"]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        self.assertEqual(proc.returncode, 2)
        data = json.loads(proc.stdout)
        self.assertEqual(data.get("error"), "ERR_UNSAFE_PACING")
        self.assertEqual(data.get("verdict"), "FAIL")

    def test_adv_35_cli_rejects_invalid_pacing_values_code_2(self):
        """
        ATTACK: Invoke cg50_agent.py diff with hold_ms <= 0 or wait_ms < 0.
        VERIFY: Aborts immediately with exit code 2 and structured ERR_INVALID_PACING report.
        """
        cg50_py = os.path.join(TOOLS_DIR, "cg50_agent.py")
        for bad_pacing in [["--hold-ms", "0"], ["--hold-ms", "-10"], ["--wait-ms", "-5"]]:
            cmd = [sys.executable, cg50_py, "diff", "--keys", "1"] + bad_pacing
            proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

            self.assertEqual(proc.returncode, 2, f"Failed on {bad_pacing}")
            data = json.loads(proc.stdout)
            self.assertEqual(data.get("error"), "ERR_INVALID_PACING")


if __name__ == "__main__":
    unittest.main(verbosity=2)
