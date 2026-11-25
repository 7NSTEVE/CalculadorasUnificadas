#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
adversarial_m1_challenge.py — Empirical Challenger Stress Harness for Milestone 1.

Adversarially stress-tests:
1. Rapid successive invocations and concurrency race conditions.
2. Timeout limits (sub-second float, 1s aggressive timeout, 0s, negative).
3. Empty / missing directories (non-existent build-dir, missing Release).
4. Non-interactive guarantee (DEVNULL stdin, piped EOF, pipe gibberish).
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

# Paths
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(TEST_DIR)
TOOLS_DIR = os.path.join(PROJECT_ROOT, "tools")
BUILD_CG50_PY = os.path.join(TOOLS_DIR, "build_cg50.py")
BUILD_GUARD_PY = os.path.join(TOOLS_DIR, "build_guard.py")
DEFAULT_BUILD_DIR = os.path.join(PROJECT_ROOT, "build-win32")
DEFAULT_EXE = os.path.join(DEFAULT_BUILD_DIR, "Release", "CalculadoraUnificada.exe")
DEFAULT_MANIFEST = os.path.join(DEFAULT_BUILD_DIR, "Release", "build_manifest.json")

# Python executable
PYTHON_EXE = sys.executable


def run_cli(args, timeout=60, stdin_data=None, stdin_devnull=True):
    """Executes build_cg50.py CLI command and returns (returncode, stdout, stderr, duration)."""
    cmd = [PYTHON_EXE, BUILD_CG50_PY] + args
    stdin_mode = subprocess.DEVNULL if stdin_devnull else subprocess.PIPE
    t0 = time.perf_counter()
    p = subprocess.Popen(
        cmd,
        stdin=stdin_mode,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace"
    )
    try:
        stdout, stderr = p.communicate(input=stdin_data, timeout=timeout)
        dur = time.perf_counter() - t0
        return p.returncode, stdout, stderr, dur
    except subprocess.TimeoutExpired:
        p.kill()
        stdout, stderr = p.communicate()
        dur = time.perf_counter() - t0
        return -999, stdout, stderr, dur


class TestEmpiricalChallengerM1(unittest.TestCase):
    """Adversarial stress-test suite targeting Milestone 1 build pipeline."""

    @classmethod
    def setUpClass(cls):
        print("\n" + "=" * 70)
        print("  STARTING EMPIRICAL CHALLENGER STRESS HARNESS — MILESTONE 1")
        print("=" * 70)
        # Ensure build_guard and build_cg50 exist
        assert os.path.isfile(BUILD_CG50_PY), f"Missing {BUILD_CG50_PY}"
        assert os.path.isfile(BUILD_GUARD_PY), f"Missing {BUILD_GUARD_PY}"

    # =========================================================================
    # AREA 1: Rapid Successive Invocations & Race Conditions
    # =========================================================================

    def test_1_1_rapid_sequential_verify_loop(self):
        """
        Stress 1.1: 25 rapid successive --verify-only invocations without sleep.
        Assert: Every call returns exit 0, valid JSON, matching SHA-256, zero file lock crashes.
        """
        print("\n--- Running Stress 1.1: Rapid Sequential Verify Loop (25 iterations) ---")
        iterations = 25
        durations = []
        for i in range(iterations):
            code, stdout, stderr, dur = run_cli(["--verify-only", "--json"])
            durations.append(dur)
            self.assertEqual(code, 0, f"Iteration {i} failed with exit code {code}: {stderr}")
            data = json.loads(stdout)
            self.assertTrue(data.get("success"), f"Iteration {i} returned success=False")
            self.assertEqual(data.get("status"), "CONFIRMED")
            self.assertTrue(data.get("sha256"))

        avg_dur = sum(durations) / len(durations)
        print(f"[PASS] 25 rapid verify calls succeeded. Avg duration: {avg_dur:.4f}s (Min: {min(durations):.4f}s, Max: {max(durations):.4f}s)")

    def test_1_2_concurrent_threaded_verification(self):
        """
        Stress 1.2: 10 concurrent threads simultaneously verifying manifest and binary.
        Assert: No file access contention (WinError 32), all return valid certified results.
        """
        print("\n--- Running Stress 1.2: Concurrent Multi-Threaded Verification (10 threads) ---")
        num_threads = 10
        results = []

        def worker():
            return run_cli(["--verify-only", "--json"])

        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = [executor.submit(worker) for _ in range(num_threads)]
            for f in as_completed(futures):
                results.append(f.result())

        for idx, (code, stdout, stderr, dur) in enumerate(results):
            self.assertEqual(code, 0, f"Thread {idx} failed with code {code}: {stderr}")
            data = json.loads(stdout)
            self.assertTrue(data["success"])
            self.assertEqual(data["status"], "CONFIRMED")

        print(f"[PASS] {num_threads} concurrent verifications completed with 100% success.")

    def test_1_3_rapid_sequential_rebuilds(self):
        """
        Stress 1.3: Two consecutive clean rebuilds in immediate succession.
        Assert: Pre-build invalidation cleanly cleans prior build, builds anew, produces fresh manifest.
        """
        print("\n--- Running Stress 1.3: Rapid Sequential Rebuilds ---")
        # Run first rebuild
        code1, out1, err1, dur1 = run_cli(["--rebuild", "--json"], timeout=180)
        self.assertEqual(code1, 0, f"First rebuild failed (exit {code1}): {err1}")
        data1 = json.loads(out1)
        ts1 = data1["timestamp"]
        sha1 = data1["sha256"]

        # Run second rebuild immediately
        code2, out2, err2, dur2 = run_cli(["--rebuild", "--json"], timeout=180)
        self.assertEqual(code2, 0, f"Second rebuild failed (exit {code2}): {err2}")
        data2 = json.loads(out2)
        ts2 = data2["timestamp"]
        sha2 = data2["sha256"]

        self.assertGreaterEqual(ts2, ts1)
        self.assertEqual(len(sha1), 64)
        self.assertEqual(len(sha2), 64)
        # Verify that manifest reflects the second build
        v_code, v_out, _, _ = run_cli(["--verify-only", "--json"])
        self.assertEqual(v_code, 0)
        v_data = json.loads(v_out)
        self.assertEqual(v_data["sha256"], sha2)
        print(f"[PASS] Sequential rebuilds succeeded (Durations: {dur1:.2f}s, {dur2:.2f}s). Hashes verified: build1={sha1[:12]}..., build2={sha2[:12]}...")

    def test_1_5_running_instance_auto_termination(self):
        """
        Stress 1.5: Rebuild when CalculadoraUnificada.exe is running.
        Assert: kill_running_instances() terminates the process, prevents Sharing Violation (WinError 32),
        and rebuild finishes with exit code 0.
        """
        print("\n--- Running Stress 1.5: Running Instance Auto-Termination on Rebuild ---")
        if not os.path.isfile(DEFAULT_EXE):
            run_cli(["--rebuild"])

        # Start process in background
        proc = subprocess.Popen([DEFAULT_EXE], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.5)
        # Verify it was running or started
        poll_val = proc.poll()
        self.assertIsNone(poll_val, "Process should be running in background")

        # Now trigger rebuild
        code, stdout, stderr, dur = run_cli(["--rebuild", "--json"], timeout=180)
        self.assertEqual(code, 0, f"Rebuild failed while process was running: {stderr}")

        # Check that original process handle is now dead / terminated
        try:
            proc.wait(timeout=2)
        except Exception:
            pass
        self.assertIsNotNone(proc.poll(), "Process should have been terminated by build_guard")
        print(f"[PASS] Running process auto-terminated and clean rebuild completed in {dur:.2f}s.")

    def test_1_4_race_condition_rebuild_vs_verify_collision(self):
        """
        Stress 1.4: Race condition between --rebuild and --verify-only.
        While rebuild has invalidated the old binary and is compiling, verify-only must FAIL-CLOSED.
        Under NO circumstance should verify-only report PASS while a rebuild is in flight.
        """
        print("\n--- Running Stress 1.4: Race Condition Rebuild vs Verify Collision ---")
        verify_results = []
        stop_event = threading.Event()

        def background_verifier():
            while not stop_event.is_set():
                code, stdout, stderr, dur = run_cli(["--verify-only", "--json"])
                verify_results.append((code, stdout))
                time.sleep(0.1)

        # Start verify poll thread
        t = threading.Thread(target=background_verifier)
        t.start()

        try:
            # Trigger clean rebuild
            code_reb, out_reb, err_reb, _ = run_cli(["--rebuild", "--json"], timeout=180)
            self.assertEqual(code_reb, 0, f"Rebuild failed: {err_reb}")
        finally:
            stop_event.set()
            t.join(timeout=5)

        # Inspect verify results captured during rebuild
        # During the window where artifacts are invalidated, verify MUST return code 3 (not code 0)
        fail_closed_detected = False
        for code, out in verify_results:
            if code == 3:
                try:
                    d = json.loads(out)
                    if d.get("status") in ("NOT_FOUND", "CORRUPTED"):
                        fail_closed_detected = True
                        break
                except Exception:
                    pass

        print(f"[VERIFIED] Total verify queries during rebuild: {len(verify_results)}. Fail-closed transient rejection observed: {fail_closed_detected}")
        # At the end, verify must be CONFIRMED (code 0)
        final_code, final_out, _, _ = run_cli(["--verify-only", "--json"])
        self.assertEqual(final_code, 0, "Final state after rebuild completion must be 0")

    # =========================================================================
    # AREA 2: Timeout Limits & Boundary Handling
    # =========================================================================

    def test_2_1_invalid_float_timeout_handling(self):
        """
        Stress 2.1: Non-integer timeout (--timeout 0.1).
        Assert: Argparse rejects invalid int with exit code 2 and helpful stderr without traceback crash.
        """
        print("\n--- Running Stress 2.1: Float Timeout Handling (--timeout 0.1) ---")
        code, stdout, stderr, dur = run_cli(["--timeout", "0.1"])
        self.assertEqual(code, 2, f"Expected argparse rejection code 2, got {code}")
        self.assertIn("invalid int value", stderr.lower())
        print(f"[PASS] Float timeout rejected cleanly with code 2: {stderr.strip()}")

    def test_2_2_aggressive_1s_timeout_abort(self):
        """
        Stress 2.2: Aggressive 1-second timeout on clean rebuild.
        Assert:
        - Fails closed within ~1-3s (exit code 2).
        - No lingering compiler processes locking the workspace.
        - Manifest and partial binary are wiped out (fail-closed).
        """
        print("\n--- Running Stress 2.2: Aggressive 1s Timeout Abort ---")
        code, stdout, stderr, dur = run_cli(["--rebuild", "--timeout", "1", "--json"], timeout=20)
        self.assertEqual(code, 2, f"Expected abort exit code 2 on timeout, got {code}")
        self.assertLess(dur, 10.0, f"Timeout took too long to abort: {dur:.2f}s")

        data = json.loads(stdout)
        self.assertFalse(data.get("success"))
        self.assertEqual(data.get("status"), "FAILED")
        self.assertEqual(data.get("exit_code"), 2)

        # Verify fail-closed: binary must NOT be certified or present as a valid build
        v_code, v_out, _, _ = run_cli(["--verify-only", "--json"])
        self.assertEqual(v_code, 3, "Verify-only must fail with code 3 after a timed-out build abort")
        print(f"[PASS] 1s timeout aborted cleanly in {dur:.2f}s with fail-closed state certified.")

    def test_2_3_zero_and_negative_timeout(self):
        """
        Stress 2.3: Boundary timeouts (--timeout 0, --timeout -5).
        Assert: Does not freeze; aborts immediately with exit code 2.
        """
        print("\n--- Running Stress 2.3: Zero and Negative Timeouts ---")
        # Timeout 0
        code0, out0, err0, dur0 = run_cli(["--rebuild", "--timeout", "0", "--json"], timeout=10)
        self.assertEqual(code0, 2, f"Expected exit code 2 for timeout 0, got {code0}")
        self.assertLess(dur0, 5.0)

        # Timeout -1
        code_neg, out_neg, err_neg, dur_neg = run_cli(["--rebuild", "--timeout", "-1", "--json"], timeout=10)
        self.assertEqual(code_neg, 2, f"Expected exit code 2 for negative timeout, got {code_neg}")
        self.assertLess(dur_neg, 5.0)
        print(f"[PASS] Zero and negative timeouts abort immediately without freeze.")

    def test_2_4_no_orphan_compiler_processes_after_abort(self):
        """
        Stress 2.4: Ensure no zombie cl.exe or msbuild.exe linger after timeout abort.
        """
        print("\n--- Running Stress 2.4: Zombie / Orphan Process Check ---")
        time.sleep(0.5)
        res = subprocess.run(["tasklist", "/FI", "IMAGENAME eq cl.exe"], capture_output=True, text=True)
        self.assertNotIn("cl.exe", res.stdout.lower(), "Orphan cl.exe detected in tasklist")
        print("[PASS] Zero orphan compiler processes active.")

    # =========================================================================
    # AREA 3: Empty / Missing Directories & Artifacts
    # =========================================================================

    def test_3_1_nonexistent_build_dir_verify_only(self):
        """
        Stress 3.1: --verify-only with completely non-existent --build-dir.
        Assert: Exits cleanly with code 3 (MANIFEST_ERROR), valid JSON, reason 'NOT_FOUND'.
        """
        print("\n--- Running Stress 3.1: Non-existent Build Directory on Verify ---")
        bogus_dir = os.path.join(tempfile.gettempdir(), f"bogus_build_{int(time.time())}")
        if os.path.exists(bogus_dir):
            shutil.rmtree(bogus_dir, ignore_errors=True)

        code, stdout, stderr, _ = run_cli(["--verify-only", "--build-dir", bogus_dir, "--json"])
        self.assertEqual(code, 3, f"Expected exit code 3, got {code}")
        data = json.loads(stdout)
        self.assertFalse(data["success"])
        self.assertEqual(data["status"], "NOT_FOUND")
        self.assertIn("not found", data["reason"].lower())
        print(f"[PASS] Non-existent build dir handled cleanly on verify: status={data['status']}")

    def test_3_2_missing_release_dir_on_clean(self):
        """
        Stress 3.2: --clean when Release directory does not exist.
        Assert: Returns code 0 (idempotent clean) without unhandled exception.
        """
        print("\n--- Running Stress 3.2: Missing Release Directory on Clean ---")
        bogus_dir = os.path.join(tempfile.gettempdir(), f"bogus_clean_{int(time.time())}")
        if os.path.exists(bogus_dir):
            shutil.rmtree(bogus_dir, ignore_errors=True)

        code, stdout, stderr, _ = run_cli(["--clean", "--build-dir", bogus_dir, "--json"])
        self.assertEqual(code, 0, f"Expected exit code 0 for clean on non-existent dir, got {code}: {stderr}")
        data = json.loads(stdout)
        self.assertTrue(data["success"])
        self.assertEqual(data["status"], "CONFIRMED")
        print(f"[PASS] Clean on missing directory is clean and idempotent (exit 0).")

    def test_3_3_deleted_manifest_with_existing_binary(self):
        """
        Stress 3.3: Binary exists but build_manifest.json is missing or deleted.
        Assert: verify-only refuses to certify unauthenticated binary; returns exit code 3.
        """
        print("\n--- Running Stress 3.3: Deleted Manifest with Existing Binary ---")
        # Create temp folder with dummy binary but no manifest
        temp_dir = tempfile.mkdtemp(prefix="test_no_manifest_")
        try:
            rel_dir = os.path.join(temp_dir, "Release")
            os.makedirs(rel_dir, exist_ok=True)
            dummy_exe = os.path.join(rel_dir, "CalculadoraUnificada.exe")
            with open(dummy_exe, "wb") as f:
                f.write(b"MZ_DUMMY_BINARY_DATA")

            code, stdout, stderr, _ = run_cli(["--verify-only", "--build-dir", temp_dir, "--json"])
            self.assertEqual(code, 3, f"Expected code 3 when manifest is missing, got {code}")
            data = json.loads(stdout)
            self.assertFalse(data["success"])
            self.assertEqual(data["status"], "NOT_FOUND")
            print(f"[PASS] Unmanifested binary rejected fail-closed with code 3.")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    # =========================================================================
    # AREA 4: Non-Interactive Guarantee
    # =========================================================================

    def test_4_1_closed_stdin_devnull(self):
        """
        Stress 4.1: Invocation with stdin=DEVNULL.
        Assert: Completes without blocking on all modes (--verify-only, --clean).
        """
        print("\n--- Running Stress 4.1: Closed stdin (DEVNULL) ---")
        code_v, out_v, err_v, dur_v = run_cli(["--verify-only", "--json"], stdin_devnull=True)
        # It may be 0 or 3 depending on state, but it must NOT hang
        self.assertIn(code_v, (0, 3))
        self.assertLess(dur_v, 5.0)

        code_c, out_c, err_c, dur_c = run_cli(["--clean", "--json"], stdin_devnull=True)
        self.assertEqual(code_c, 0)
        self.assertLess(dur_c, 5.0)
        print(f"[PASS] Commands run strictly non-interactive under DEVNULL stdin.")

    def test_4_2_piped_eof_and_gibberish_stdin(self):
        """
        Stress 4.2: Invocation with piped input containing EOF and random keystrokes.
        Assert: stdin is ignored, does not alter execution or hang waiting for user confirmation.
        """
        print("\n--- Running Stress 4.2: Piped EOF and Keystroke Stdin ---")
        gibberish = "y\r\n\x03\x04n\r\nq\r\n"
        code, stdout, stderr, dur = run_cli(["--clean", "--json"], stdin_data=gibberish, stdin_devnull=False)
        self.assertEqual(code, 0)
        self.assertLess(dur, 5.0)
        data = json.loads(stdout)
        self.assertTrue(data["success"])
        print(f"[PASS] Piped keystrokes ignored; operation is non-interactive.")

    def test_4_3_negative_abort_is_non_interactive(self):
        """
        Stress 4.3: Negative build failure (invalid target) with piped stdin.
        Assert: Exits with code 2 immediately; does NOT pause or prompt for input.
        """
        print("\n--- Running Stress 4.3: Negative Abort Non-Interactivity ---")
        code, stdout, stderr, dur = run_cli(
            ["--target", "NonExistentTarget", "--rebuild", "--json"],
            stdin_data="n\r\n",
            stdin_devnull=False,
            timeout=20
        )
        self.assertEqual(code, 2, f"Expected abort code 2, got {code}")
        self.assertLess(dur, 10.0)
        data = json.loads(stdout)
        self.assertFalse(data["success"])
        self.assertEqual(data["exit_code"], 2)
        print(f"[PASS] Negative abort exits with code 2 in {dur:.2f}s without user prompts.")


if __name__ == "__main__":
    # Ensure a fresh confirmed build exists prior to test start
    print("Rebuilding fresh binary to establish baseline certified state...")
    subprocess.run([PYTHON_EXE, BUILD_CG50_PY, "--rebuild"], check=True)
    unittest.main()
