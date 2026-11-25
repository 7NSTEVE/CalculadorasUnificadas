#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_adversarial_m1.py — Empirical Challenger Stress Harness for Milestone 1.

Adversarially tests and challenges:
1. Binary byte tampering (single-byte in-place flip, truncation, zero padding, zero-byte file)
2. Manifest tampering (SHA-256 flip, status downgrade, corrupted JSON, missing fields, expiration)
3. PE header architecture tampering (AMD64 spoofing, invalid signature)
4. Process locking (taskkill on live binary vs unkillable file handle lock WinError 32 -> code 4)
5. Stale binary prevention under forced compilation abort (purge before build, no stale reuse)
6. Static contract guard enforcement (missing CMakeLists, missing ROMs, missing DLL, unthrottled loop)
7. CLI exit codes: 0 (SUCCESS), 1 (CONFIG_ERR), 2 (BUILD/ABORT), 3 (MANIFEST_ERR), 4 (LOCK_ERR)
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
from typing import Tuple, Dict, Any, Optional

# Locate tools
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
TOOLS_DIR = os.path.join(PROJECT_ROOT, "tools")
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

from build_guard import (
    compute_sha256,
    verify_pe_header,
    generate_manifest,
    verify_manifest,
    invalidate_stale_artifacts,
    run_static_contract_guard,
    extract_diagnostics,
    verify_or_build_core,
    BuildGateError,
    ConfigError,
    CompilationError,
    ManifestVerificationError,
    ProcessLockError,
    PE_MACHINE_I386,
    PE_MACHINE_AMD64,
)

BUILD_CG50_PY = os.path.join(TOOLS_DIR, "build_cg50.py")


class TestAdversarialBinaryTampering(unittest.TestCase):
    """Stress-tests file tampering and corruption detection."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="adv_tamper_")
        self.release_dir = os.path.join(self.temp_dir, "Release")
        os.makedirs(self.release_dir, exist_ok=True)
        self.exe_path = os.path.join(self.release_dir, "CalculadoraUnificada.exe")
        self.manifest_path = os.path.join(self.release_dir, "build_manifest.json")
        self._create_valid_certified_target()

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_valid_certified_target(self, payload: bytes = b"EXACT_CODE_PAYLOAD_12345678"):
        """Creates a valid i386 PE binary and generates authentic build_manifest.json."""
        dos = bytearray(64)
        dos[:2] = b"MZ"
        struct.pack_into("<I", dos, 0x3C, 64)
        pe = bytearray(24)
        pe[:4] = b"PE\x00\x00"
        struct.pack_into("<H", pe, 4, PE_MACHINE_I386)
        with open(self.exe_path, "wb") as f:
            f.write(dos)
            f.write(pe)
            f.write(payload)

        generate_manifest(
            binary_path=self.exe_path,
            manifest_path=self.manifest_path,
            build_duration=1.23,
            compiler_info="MSVC 14.44.35207",
            target_name="CalculadoraUnificada"
        )

    def test_adv_01_single_byte_flip_detected(self):
        """ATTACK: Invert a single bit in the binary payload without changing size.
        VERIFY: verify_manifest rejects with CORRUPTED, SHA-256 mismatch."""
        with open(self.exe_path, "r+b") as f:
            f.seek(80)
            orig = f.read(1)
            flipped = bytes([orig[0] ^ 0xFF])
            f.seek(80)
            f.write(flipped)

        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")
        self.assertIn("SHA-256 mismatch", res.reason)

        cmd = [sys.executable, BUILD_CG50_PY, "--build-dir", self.temp_dir, "--verify-only", "--json"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 3, "CLI must exit with code 3 on corrupted binary")
        out = json.loads(proc.stdout)
        self.assertFalse(out["success"])
        self.assertEqual(out["status"], "CORRUPTED")

    def test_adv_02_binary_zero_padding_detected(self):
        """ATTACK: Append 1024 zero bytes to binary.
        VERIFY: verify_manifest catches size mismatch before hash check."""
        with open(self.exe_path, "ab") as f:
            f.write(b"\x00" * 1024)

        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")
        self.assertIn("Size mismatch", res.reason)

    def test_adv_03_binary_truncation_detected(self):
        """ATTACK: Truncate binary by 16 bytes.
        VERIFY: verify_manifest catches size mismatch."""
        size = os.path.getsize(self.exe_path)
        with open(self.exe_path, "r+b") as f:
            f.truncate(size - 16)

        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")
        self.assertIn("Size mismatch", res.reason)

    def test_adv_04_binary_empty_file_detected(self):
        """ATTACK: Truncate binary to 0 bytes.
        VERIFY: verify_manifest rejects with 'empty (0 bytes)'."""
        with open(self.exe_path, "wb") as f:
            pass

        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")
        self.assertIn("empty (0 bytes)", res.reason)

    def test_adv_05_pe_machine_amd64_spoofing_detected(self):
        """ATTACK: Change PE machine to AMD64 (0x8664) and forged manifest hash.
        VERIFY: Gate 7 catches PE machine mismatch despite valid SHA-256."""
        dos = bytearray(64)
        dos[:2] = b"MZ"
        struct.pack_into("<I", dos, 0x3C, 64)
        pe = bytearray(24)
        pe[:4] = b"PE\x00\x00"
        struct.pack_into("<H", pe, 4, PE_MACHINE_AMD64)
        with open(self.exe_path, "wb") as f:
            f.write(dos)
            f.write(pe)
            f.write(b"AMD64_PAYLOAD")

        actual_sha = compute_sha256(self.exe_path)
        size = os.path.getsize(self.exe_path)
        with open(self.manifest_path, "r", encoding="utf-8") as f:
            m = json.load(f)
        m["binary_sha256"] = actual_sha
        m["file_size"] = size
        m["binary"]["sha256"] = actual_sha
        m["binary"]["size_bytes"] = size
        with open(self.manifest_path, "w", encoding="utf-8") as f:
            json.dump(m, f)

        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")
        self.assertIn("PE machine mismatch", res.reason)
        self.assertIn("IMAGE_FILE_MACHINE_AMD64", res.reason)

    def test_adv_06_missing_binary_detected(self):
        """ATTACK: Remove binary file while leaving manifest intact.
        VERIFY: verify_manifest returns NOT_FOUND, CLI returns exit code 3."""
        os.remove(self.exe_path)
        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "NOT_FOUND")

        cmd = [sys.executable, BUILD_CG50_PY, "--build-dir", self.temp_dir, "--verify-only", "--json"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 3)
        out = json.loads(proc.stdout)
        self.assertEqual(out["status"], "NOT_FOUND")


class TestAdversarialManifestTampering(unittest.TestCase):
    """Stress-tests manifest integrity and tampering detection."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="adv_manifest_")
        self.release_dir = os.path.join(self.temp_dir, "Release")
        os.makedirs(self.release_dir, exist_ok=True)
        self.exe_path = os.path.join(self.release_dir, "CalculadoraUnificada.exe")
        self.manifest_path = os.path.join(self.release_dir, "build_manifest.json")
        self._create_valid_certified_target()

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_valid_certified_target(self):
        dos = bytearray(64)
        dos[:2] = b"MZ"
        struct.pack_into("<I", dos, 0x3C, 64)
        pe = bytearray(24)
        pe[:4] = b"PE\x00\x00"
        struct.pack_into("<H", pe, 4, PE_MACHINE_I386)
        with open(self.exe_path, "wb") as f:
            f.write(dos)
            f.write(pe)
            f.write(b"CODE")
        generate_manifest(self.exe_path, self.manifest_path, build_duration=1.0)

    def test_adv_07_tamper_manifest_sha256(self):
        """ATTACK: Flip one hex character of binary_sha256 in build_manifest.json.
        VERIFY: verify_manifest detects hash mismatch and rejects with CORRUPTED."""
        with open(self.manifest_path, "r", encoding="utf-8") as f:
            m = json.load(f)
        orig_sha = m["binary_sha256"]
        tampered_sha = ("0" if orig_sha[0] != "0" else "1") + orig_sha[1:]
        m["binary_sha256"] = tampered_sha
        m["binary"]["sha256"] = tampered_sha
        with open(self.manifest_path, "w", encoding="utf-8") as f:
            json.dump(m, f)

        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")
        self.assertIn("SHA-256 mismatch", res.reason)

    def test_adv_08_tamper_manifest_status_downgrade(self):
        """ATTACK: Change manifest status from CONFIRMED to FAILED.
        VERIFY: verify_manifest rejects with FAILED."""
        with open(self.manifest_path, "r", encoding="utf-8") as f:
            m = json.load(f)
        m["status"] = "FAILED"
        with open(self.manifest_path, "w", encoding="utf-8") as f:
            json.dump(m, f)

        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "FAILED")

    def test_adv_09_corrupted_json_syntax(self):
        """ATTACK: Corrupt JSON syntax in manifest file (truncated JSON).
        VERIFY: verify_manifest handles parse error cleanly without unhandled exception."""
        with open(self.manifest_path, "w", encoding="utf-8") as f:
            f.write('{"status": "CONFIRMED", "binary_sha256": "ABCDEF')

        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "CORRUPTED")
        self.assertIn("JSON parse error", res.reason)

        cmd = [sys.executable, BUILD_CG50_PY, "--build-dir", self.temp_dir, "--verify-only", "--json"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 3)

    def test_adv_10_missing_manifest_file(self):
        """ATTACK: Remove build_manifest.json completely.
        VERIFY: verify_manifest reports NOT_FOUND, CLI returns exit code 3."""
        os.remove(self.manifest_path)
        res = verify_manifest(self.exe_path, self.manifest_path)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "NOT_FOUND")

        cmd = [sys.executable, BUILD_CG50_PY, "--build-dir", self.temp_dir, "--verify-only", "--json"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 3)

    def test_adv_11_manifest_expiration_window(self):
        """ATTACK: Manifest timestamp is older than max_age_seconds.
        VERIFY: verify_manifest rejects with EXPIRED."""
        with open(self.manifest_path, "r", encoding="utf-8") as f:
            m = json.load(f)
        m["build_epoch_utc"] = time.time() - 3600
        m["timestamp"] = m["build_epoch_utc"]
        with open(self.manifest_path, "w", encoding="utf-8") as f:
            json.dump(m, f)

        res = verify_manifest(self.exe_path, self.manifest_path, max_age_seconds=30.0)
        self.assertFalse(res.is_valid)
        self.assertEqual(res.status, "EXPIRED")
        self.assertIn("Manifest expired", res.reason)


class TestAdversarialProcessLocking(unittest.TestCase):
    """Stress-tests behavior under process locking and concurrency."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="adv_lock_")
        self.release_dir = os.path.join(self.temp_dir, "Release")
        os.makedirs(self.release_dir, exist_ok=True)
        self.exe_path = os.path.join(self.release_dir, "CalculadoraUnificada.exe")
        self.manifest_path = os.path.join(self.release_dir, "build_manifest.json")
        self._create_dummy_target()

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_dummy_target(self):
        dos = bytearray(64)
        dos[:2] = b"MZ"
        struct.pack_into("<I", dos, 0x3C, 64)
        pe = bytearray(24)
        pe[:4] = b"PE\x00\x00"
        struct.pack_into("<H", pe, 4, PE_MACHINE_I386)
        with open(self.exe_path, "wb") as f:
            f.write(dos)
            f.write(pe)
            f.write(b"CODE")
        with open(self.manifest_path, "w", encoding="utf-8") as f:
            f.write("{}")

    def test_adv_12_exclusive_file_lock_causes_code_4_fail_closed(self):
        """ATTACK: Hold an exclusive file lock on the target executable from Python.
        VERIFY: invalidate_stale_artifacts raises ProcessLockError with exit code 4.
        VERIFY: build_cg50.py fails-closed and exits with code 4 (LOCK_OR_ENV_ERROR)."""
        with open(self.exe_path, "r+b") as lock_handle:
            with self.assertRaises(ProcessLockError) as ctx:
                invalidate_stale_artifacts(self.release_dir, "CalculadoraUnificada")

            self.assertEqual(ctx.exception.exit_code, 4)
            self.assertEqual(ctx.exception.stage, "process_locking")
            self.assertIn("file lock", str(ctx.exception))

            cmd = [
                sys.executable, BUILD_CG50_PY,
                "--build-dir", self.temp_dir,
                "--clean",
                "--json"
            ]
            proc = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(proc.returncode, 4, "CLI must exit with code 4 when file is locked")
            out = json.loads(proc.stdout)
            self.assertFalse(out["success"])
            self.assertEqual(out["status"], "FAILED")
            self.assertEqual(out["action"], "clean")
            self.assertIn("file lock", out["error"])

        # Once lock is released, invalidate_stale_artifacts must succeed
        invalidate_stale_artifacts(self.release_dir, "CalculadoraUnificada")
        self.assertFalse(os.path.exists(self.exe_path))
        self.assertFalse(os.path.exists(self.manifest_path))


class TestAdversarialStaleBinaryPrevention(unittest.TestCase):
    """Stress-tests fail-closed abort and anti-stale-binary guarantee."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="adv_stale_")
        self.release_dir = os.path.join(self.temp_dir, "Release")
        os.makedirs(self.release_dir, exist_ok=True)
        self.exe_path = os.path.join(self.release_dir, "CalculadoraUnificada.exe")
        self.manifest_path = os.path.join(self.release_dir, "build_manifest.json")

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_adv_13_stale_binary_purged_on_failed_build_attempt(self):
        """ATTACK: Plant a stale pre-existing binary for target CalculadoraUnificada.
        Then attempt a rebuild where compilation fails (e.g. missing CMakeLists).
        VERIFY:
        1. Pre-invalidation purges the stale binary BEFORE compilation attempt.
        2. Compilation fails with exit code 2.
        3. Stale binary does NOT survive or get reused.
        4. verify-only fails with exit code 3 (NOT_FOUND)."""
        # 1. Plant stale binary
        with open(self.exe_path, "wb") as f:
            f.write(b"STALE_OBSOLETE_BINARY_FROM_LAST_WEEK")
        with open(self.manifest_path, "w", encoding="utf-8") as f:
            f.write('{"status": "CONFIRMED", "stale": true}')

        self.assertTrue(os.path.exists(self.exe_path))

        # 2. Invoke rebuild on target CalculadoraUnificada in temp_dir (no CMakeLists.txt -> fails)
        cmd = [
            sys.executable, BUILD_CG50_PY,
            "--build-dir", self.temp_dir,
            "--target", "CalculadoraUnificada",
            "--rebuild",
            "--json"
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 2, "Must exit with code 2 on build failure")
        out = json.loads(proc.stdout)
        self.assertFalse(out["success"])
        self.assertEqual(out["status"], "FAILED")
        self.assertEqual(out["exit_code"], 2)

        # 3. Strictly assert the stale binary is GONE
        self.assertFalse(
            os.path.exists(self.exe_path),
            "CRITICAL: Stale binary must be deleted and not remain after failed build attempt!"
        )
        self.assertFalse(
            os.path.exists(self.manifest_path),
            "CRITICAL: Stale manifest must be deleted!"
        )

        # 4. verify-only must fail with code 3 (NOT_FOUND)
        cmd_v = [
            sys.executable, BUILD_CG50_PY,
            "--build-dir", self.temp_dir,
            "--target", "CalculadoraUnificada",
            "--verify-only",
            "--json"
        ]
        proc_v = subprocess.run(cmd_v, capture_output=True, text=True)
        self.assertEqual(proc_v.returncode, 3)
        out_v = json.loads(proc_v.stdout)
        self.assertFalse(out_v["success"])
        self.assertEqual(out_v["status"], "NOT_FOUND")

    def test_adv_13b_named_target_stale_binary_purged(self):
        """ATTACK: Plant a stale binary for a custom named target, then build with invalid target.
        VERIFY: The custom target executable is also purged in pre-invalidation."""
        target_name = "CustomTargetX"
        target_exe = os.path.join(self.release_dir, f"{target_name}.exe")
        with open(target_exe, "wb") as f:
            f.write(b"STALE_CUSTOM_TARGET")

        self.assertTrue(os.path.exists(target_exe))

        cmd = [
            sys.executable, BUILD_CG50_PY,
            "--build-dir", self.temp_dir,
            "--target", target_name,
            "--rebuild",
            "--json"
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 2)
        self.assertFalse(os.path.exists(target_exe), "Custom target stale binary must be purged")

    def test_adv_13c_cmake_build_failure_writes_error_artifacts(self):
        """ATTACK: Invoke invalid target on real project to trigger CMake/MSBuild error.
        VERIFY: build_error.json and build_error.log are written with compiler diagnostics."""
        build_dir = os.path.join(PROJECT_ROOT, "build-win32")
        cmd = [
            sys.executable, BUILD_CG50_PY,
            "--build-dir", build_dir,
            "--target", "NonExistentTarget_DiagnosticsProof",
            "--rebuild",
            "--json"
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 2)
        out = json.loads(proc.stdout)
        self.assertFalse(out["success"])
        self.assertEqual(out["exit_code"], 2)
        self.assertIn("NonExistentTarget_DiagnosticsProof", proc.stdout)

        error_json = os.path.join(build_dir, "build_error.json")
        error_log = os.path.join(build_dir, "build_error.log")
        self.assertTrue(os.path.exists(error_json))
        self.assertTrue(os.path.exists(error_log))

        with open(error_json, "r", encoding="utf-8") as f:
            err_data = json.load(f)
        self.assertEqual(err_data["status"], "FAIL")
        self.assertEqual(err_data["exit_code"], 2)


class TestAdversarialStaticContracts(unittest.TestCase):
    """Stress-tests static contract guard enforcement."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="adv_contract_")

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_adv_14_missing_cmakelists_aborts_with_code_2(self):
        """ATTACK: Directory without CMakeLists.txt.
        VERIFY: run_static_contract_guard raises CompilationError(exit_code=2, stage='contract_guard')."""
        with self.assertRaises(CompilationError) as ctx:
            run_static_contract_guard(self.temp_dir)
        self.assertEqual(ctx.exception.exit_code, 2)
        self.assertEqual(ctx.exception.stage, "contract_guard")
        self.assertIn("CMakeLists.txt missing", ctx.exception.log_output)

    def test_adv_15_missing_classwiz_rom_aborts_with_code_2(self):
        """ATTACK: CMakeLists exists but roms/fx991lax.rom is missing.
        VERIFY: run_static_contract_guard raises CompilationError(exit_code=2)."""
        with open(os.path.join(self.temp_dir, "CMakeLists.txt"), "w") as f:
            f.write("cmake_minimum_required(VERSION 3.20)\n")
        os.makedirs(os.path.join(self.temp_dir, "roms"), exist_ok=True)

        with self.assertRaises(CompilationError) as ctx:
            run_static_contract_guard(self.temp_dir)
        self.assertEqual(ctx.exception.exit_code, 2)
        self.assertEqual(ctx.exception.stage, "contract_guard")
        self.assertIn("ClassWiz ROM missing", ctx.exception.log_output)

    def test_adv_16_missing_runtime_gcal_dll_aborts_with_code_2(self):
        """ATTACK: ROMs exist but runtime/GCalAppIF.dll is missing.
        VERIFY: run_static_contract_guard raises CompilationError(exit_code=2)."""
        with open(os.path.join(self.temp_dir, "CMakeLists.txt"), "w") as f:
            f.write("cmake_minimum_required(VERSION 3.20)\n")
        roms = os.path.join(self.temp_dir, "roms")
        os.makedirs(roms, exist_ok=True)
        with open(os.path.join(roms, "fx991lax.rom"), "wb") as f:
            f.write(b"ROM_CW")
        with open(os.path.join(roms, "fx570vnplus.rom"), "wb") as f:
            f.write(b"ROM_ES")
        os.makedirs(os.path.join(self.temp_dir, "runtime"), exist_ok=True)

        with self.assertRaises(CompilationError) as ctx:
            run_static_contract_guard(self.temp_dir)
        self.assertEqual(ctx.exception.exit_code, 2)
        self.assertEqual(ctx.exception.stage, "contract_guard")
        self.assertIn("GCalAppIF.dll", ctx.exception.log_output)


class TestAdversarialCliBehavior(unittest.TestCase):
    """Stress-tests CLI driver flag combinations and edge cases."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="adv_cli_")
        self.release_dir = os.path.join(self.temp_dir, "Release")
        os.makedirs(self.release_dir, exist_ok=True)
        self.exe_path = os.path.join(self.release_dir, "CalculadoraUnificada.exe")
        self.manifest_path = os.path.join(self.release_dir, "build_manifest.json")

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_adv_17_clean_flag_purges_and_returns_code_0(self):
        """ATTACK/EDGE: Call --clean on an existing target.
        VERIFY: Artifacts are deleted and exit code 0 is returned."""
        with open(self.exe_path, "wb") as f:
            f.write(b"SAMPLE")
        with open(self.manifest_path, "w") as f:
            f.write("{}")

        cmd = [sys.executable, BUILD_CG50_PY, "--build-dir", self.temp_dir, "--clean", "--json"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0)
        out = json.loads(proc.stdout)
        self.assertTrue(out["success"])
        self.assertEqual(out["action"], "clean")
        self.assertFalse(os.path.exists(self.exe_path))
        self.assertFalse(os.path.exists(self.manifest_path))

    def test_adv_18_verify_only_on_empty_dir_exits_code_3(self):
        """EDGE: Call --verify-only on an empty directory without manifest.
        VERIFY: Exit code 3, NOT_FOUND."""
        cmd = [sys.executable, BUILD_CG50_PY, "--build-dir", self.temp_dir, "--verify-only", "--json"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 3)
        out = json.loads(proc.stdout)
        self.assertFalse(out["success"])
        self.assertEqual(out["status"], "NOT_FOUND")

    def test_adv_19_no_rebuild_with_corrupted_manifest_falls_back_or_aborts(self):
        """ATTACK: Call --no-rebuild when manifest is corrupted or invalid.
        VERIFY: build_cg50.py does NOT blindly reuse the corrupted binary;
        it falls through to rebuild (or aborts if build cannot succeed in mock dir)."""
        with open(self.exe_path, "wb") as f:
            f.write(b"CORRUPTED_TARGET")
        with open(self.manifest_path, "w") as f:
            f.write('{"status": "CORRUPTED"}')

        cmd = [
            sys.executable, BUILD_CG50_PY,
            "--build-dir", self.temp_dir,
            "--target", "CalculadoraUnificada",
            "--no-rebuild",
            "--json"
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        self.assertNotEqual(proc.returncode, 0, "--no-rebuild must not certify corrupted artifact")
        self.assertEqual(proc.returncode, 2, "Must abort fail-closed with code 2")


if __name__ == "__main__":
    unittest.main(verbosity=2)
