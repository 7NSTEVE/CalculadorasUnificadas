#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_build_guard.py — Comprehensive Unit & Integration Tests for Milestone 1 (M1).
Tests behavior and edge cases of build_guard.py and build_cg50.py.
"""

import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from typing import Tuple, Dict, Any, Optional, List

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

from build_guard import (
    BuildGuard,
    BuildResult,
    VerificationResult,
    BuildGateError,
    ConfigError,
    CompilationError,
    ManifestVerificationError,
    ProcessLockError,
    compute_sha256,
    verify_pe_header,
    invalidate_stale_artifacts,
    generate_manifest,
    verify_manifest,
    run_static_contract_guard,
    extract_diagnostics,
    PE_MACHINE_I386,
    PE_MACHINE_AMD64
)


class TestBuildGuardUnit(unittest.TestCase):
    """Unit tests for build_guard helper functions and edge cases."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="bg_test_")
        self.mock_release = os.path.join(self.temp_dir, "Release")
        os.makedirs(self.mock_release, exist_ok=True)

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_synthetic_pe(self, machine: int = PE_MACHINE_I386, payload: bytes = b"CODE") -> str:
        """Constructs a minimal valid PE binary with specified machine architecture."""
        path = os.path.join(self.mock_release, "CalculadoraUnificada.exe")
        dos_header = bytearray(64)
        dos_header[:2] = b"MZ"
        struct.pack_into("<I", dos_header, 0x3C, 64)  # PE offset at byte 64

        pe_header = bytearray(24)
        pe_header[:4] = b"PE\x00\x00"
        struct.pack_into("<H", pe_header, 4, machine)

        with open(path, "wb") as f:
            f.write(dos_header)
            f.write(pe_header)
            f.write(payload)
        return path

    def test_compute_sha256_known_bytes(self):
        """Verifies compute_sha256 matches hashlib output in uppercase hex."""
        sample_path = os.path.join(self.temp_dir, "sample.bin")
        content = b"CALCULADORA_UNIFICADA_M1_TEST"
        with open(sample_path, "wb") as f:
            f.write(content)

        expected = hashlib.sha256(content).hexdigest().upper()
        actual = compute_sha256(sample_path)
        self.assertEqual(actual, expected)

    def test_verify_pe_header_valid_i386(self):
        """Verifies PE parser detects IMAGE_FILE_MACHINE_I386 (0x014c)."""
        pe_path = self._create_synthetic_pe(machine=PE_MACHINE_I386)
        machine, name = verify_pe_header(pe_path)
        self.assertEqual(machine, 0x014c)
        self.assertEqual(name, "IMAGE_FILE_MACHINE_I386")

    def test_verify_pe_header_amd64_detected(self):
        """Verifies PE parser detects IMAGE_FILE_MACHINE_AMD64 (0x8664)."""
        pe_path = self._create_synthetic_pe(machine=PE_MACHINE_AMD64)
        machine, name = verify_pe_header(pe_path)
        self.assertEqual(machine, 0x8664)
        self.assertEqual(name, "IMAGE_FILE_MACHINE_AMD64")

    def test_verify_pe_header_invalid_file_raises_error(self):
        """Verifies non-PE binary raises ValueError."""
        invalid_path = os.path.join(self.temp_dir, "not_pe.bin")
        with open(invalid_path, "wb") as f:
            f.write(b"NOT_A_PE_HEADER_FILE")

        with self.assertRaises(ValueError):
            verify_pe_header(invalid_path)

    def test_invalidate_stale_artifacts_purges_files(self):
        """Verifies pre-build invalidation deletes executable and manifest files."""
        exe_path = self._create_synthetic_pe()
        manifest_path = os.path.join(self.mock_release, "build_manifest.json")
        with open(manifest_path, "w") as f:
            f.write("{}")

        self.assertTrue(os.path.exists(exe_path))
        self.assertTrue(os.path.exists(manifest_path))

        invalidate_stale_artifacts(self.mock_release, "CalculadoraUnificada")

        self.assertFalse(os.path.exists(exe_path), "Target exe must be removed")
        self.assertFalse(os.path.exists(manifest_path), "Manifest must be removed")

    def test_generate_and_verify_manifest_success_flow(self):
        """Verifies round-trip manifest generation and cryptographic certification."""
        exe_path = self._create_synthetic_pe()
        manifest_path = os.path.join(self.mock_release, "build_manifest.json")

        m_data = generate_manifest(
            binary_path=exe_path,
            manifest_path=manifest_path,
            build_duration=5.25,
            compiler_info="MSVC 14.44.35207",
            target_name="CalculadoraUnificada"
        )

        self.assertEqual(m_data["status"], "CONFIRMED")
        self.assertEqual(m_data["target"], "CalculadoraUnificada.exe")
        self.assertEqual(m_data["binary_sha256"], compute_sha256(exe_path))

        # Verify manifest
        v_res = verify_manifest(exe_path, manifest_path)
        self.assertTrue(v_res.is_valid)
        self.assertEqual(v_res.status, "CONFIRMED")
        self.assertEqual(v_res.sha256, m_data["binary_sha256"])

    def test_verify_manifest_detects_sha256_tampering(self):
        """Verifies that altering binary bytes causes verify_manifest to reject."""
        exe_path = self._create_synthetic_pe()
        manifest_path = os.path.join(self.mock_release, "build_manifest.json")

        generate_manifest(exe_path, manifest_path, build_duration=3.0)

        # Tamper with the binary in-place without changing size
        with open(exe_path, "r+b") as f:
            f.seek(80)
            f.write(b"XXXX")

        v_res = verify_manifest(exe_path, manifest_path)
        self.assertFalse(v_res.is_valid)
        self.assertEqual(v_res.status, "CORRUPTED")
        self.assertIn("SHA-256 mismatch", v_res.reason)

    def test_verify_manifest_detects_size_mismatch(self):
        """Verifies that changing binary size causes verify_manifest to reject with CORRUPTED."""
        exe_path = self._create_synthetic_pe()
        manifest_path = os.path.join(self.mock_release, "build_manifest.json")
        generate_manifest(exe_path, manifest_path, build_duration=3.0)

        with open(exe_path, "ab") as f:
            f.write(b"EXTRA_BYTES")

        v_res = verify_manifest(exe_path, manifest_path)
        self.assertFalse(v_res.is_valid)
        self.assertEqual(v_res.status, "CORRUPTED")
        self.assertIn("Size mismatch", v_res.reason)

    def test_verify_manifest_detects_missing_binary(self):
        """Verifies rejection when binary is missing."""
        exe_path = self._create_synthetic_pe()
        manifest_path = os.path.join(self.mock_release, "build_manifest.json")
        generate_manifest(exe_path, manifest_path, build_duration=1.0)

        os.remove(exe_path)
        v_res = verify_manifest(exe_path, manifest_path)
        self.assertFalse(v_res.is_valid)
        self.assertEqual(v_res.status, "NOT_FOUND")

    def test_verify_manifest_detects_expired_manifest(self):
        """Verifies max_age_seconds rejection when manifest is older than allowed."""
        exe_path = self._create_synthetic_pe()
        manifest_path = os.path.join(self.mock_release, "build_manifest.json")
        generate_manifest(exe_path, manifest_path, build_duration=1.0)

        # Force manifest epoch to 100 seconds in the past
        with open(manifest_path, "r", encoding="utf-8") as f:
            m = json.load(f)
        m["build_epoch_utc"] = time.time() - 100
        m["timestamp"] = m["build_epoch_utc"]
        with open(manifest_path, "w", encoding="utf-8") as f:
            json.dump(m, f)

        v_res = verify_manifest(exe_path, manifest_path, max_age_seconds=10.0)
        self.assertFalse(v_res.is_valid)
        self.assertEqual(v_res.status, "EXPIRED")

    def test_static_contract_guard_detects_unthrottled_loop(self):
        """Verifies static guard detects while(true) loop without Sleep."""
        # Create minimal repo structure
        with open(os.path.join(self.temp_dir, "CMakeLists.txt"), "w") as f:
            f.write("cmake_minimum_required(VERSION 3.20)\n")
        with open(os.path.join(self.temp_dir, "main.cpp"), "w") as f:
            f.write("void loop() { while(true) { /* no sleep */ } }\n")

        with self.assertRaises(CompilationError) as ctx:
            run_static_contract_guard(self.temp_dir)
        self.assertEqual(ctx.exception.exit_code, 2)
        self.assertEqual(ctx.exception.stage, "contract_guard")

    def test_extract_diagnostics_filters_compiler_errors(self):
        """Verifies extract_diagnostics isolates C2xxx and LNKxxx compiler errors."""
        raw_log = """
Microsoft (R) Build Engine version 17.14.51
main.cpp(142): error C2065: 'undefined_var': undeclared identifier
cg50_backend.cpp(88): error C2660: 'GCAI_Init': function does not take 2 arguments
CalculadoraUnificada.obj : fatal error LNK1120: 2 unresolved externals
Normal informational message
Done building project "CalculadoraUnificada.vcxproj" -- FAILED.
"""
        diags = extract_diagnostics(raw_log)
        self.assertEqual(len(diags), 3)
        self.assertIn("error C2065", diags[0])
        self.assertIn("error C2660", diags[1])
        self.assertIn("fatal error LNK1120", diags[2])


class TestBuildCg50Cli(unittest.TestCase):
    """End-to-end integration tests for build_cg50.py CLI."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="bg_cli_test_")
        self.build_dir = os.path.join(self.temp_dir, "build-win32")
        self.release_dir = os.path.join(self.build_dir, "Release")
        os.makedirs(self.release_dir, exist_ok=True)

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_mock_environment(self) -> Tuple[str, str]:
        dos = bytearray(64)
        dos[:2] = b"MZ"
        struct.pack_into("<I", dos, 0x3C, 64)
        pe = bytearray(24)
        pe[:4] = b"PE\x00\x00"
        struct.pack_into("<H", pe, 4, PE_MACHINE_I386)
        exe_path = os.path.join(self.release_dir, "CalculadoraUnificada.exe")
        with open(exe_path, "wb") as f:
            f.write(dos)
            f.write(pe)
            f.write(b"MOCK_CODE")
        manifest_path = os.path.join(self.release_dir, "build_manifest.json")
        generate_manifest(exe_path, manifest_path, build_duration=2.5, compiler_info="MSVC 14.44.35207")
        return exe_path, manifest_path

    def test_cli_verify_only_json(self):
        """Verifies build_cg50.py --verify-only --json outputs valid JSON and exit 0."""
        self._create_mock_environment()
        cmd = [sys.executable, os.path.join(TOOLS_DIR, "build_cg50.py"),
               "--build-dir", self.build_dir, "--verify-only", "--json"]
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
        self.assertEqual(res.returncode, 0)
        out = json.loads(res.stdout)
        self.assertTrue(out["success"])
        self.assertEqual(out["status"], "CONFIRMED")
        self.assertIsNotNone(out["sha256"])

    def test_cli_negative_invalid_target_exits_code_2(self):
        """Verifies build_cg50.py on invalid target aborts with exit code 2 and structured failure JSON."""
        cmd = [sys.executable, os.path.join(TOOLS_DIR, "build_cg50.py"),
               "--build-dir", self.build_dir,
               "--target", "InvalidTargetXYZ", "--rebuild", "--json"]
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=20)
        self.assertEqual(res.returncode, 2, f"Expected exit code 2, got {res.returncode}")
        out = json.loads(res.stdout)
        self.assertFalse(out["success"])
        self.assertEqual(out["exit_code"], 2)
        self.assertEqual(out["status"], "FAILED")


if __name__ == "__main__":
    unittest.main(verbosity=2)
