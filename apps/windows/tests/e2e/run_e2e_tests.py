#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_e2e_tests.py — Main E2E Test Suite Runner for Differential Testing Platform.
Authoritative source: ORIGINAL_REQUEST.md, PROJECT.md, and spec_miner_survey_1/report.md.

Executes and reports opaque-box E2E tests across Tiers 1-4:
- Tier 1: Feature Coverage (R1, R2, R3, R4) [20 tests]
- Tier 2: Boundary & Corner Cases (R1, R2, R3, R4) [20 tests]
- Tier 3: Pairwise Cross-Feature Combinations [6 tests]
- Tier 4: Real-World Workflow & Parity Scenarios [6 tests]

CLI Flags:
  --tier {1,2,3,4,all}   Execute specific tier or all (default: all)
  --inspect, --dry-run   List all discoverable tests with descriptions without running
  --verbose, -v          Verbose execution output
  --out-json <file>      Export execution results to a JSON file
  --help, -h             Show help message

Exit codes:
  0: All executed tests passed (100% pass rate)
  1: One or more tests failed or encountered errors
"""

import argparse
import io
import json
import os
import sys
import time
import unittest
from typing import Dict, Any, List

# Setup module search paths
CURR_DIR = os.path.dirname(os.path.abspath(__file__))
if CURR_DIR not in sys.path:
    sys.path.insert(0, CURR_DIR)

from tiers.tier1_features import TestTier1FeatureCoverage
from tiers.tier2_boundaries import TestTier2BoundaryCases
from tiers.tier3_combinations import TestTier3Combinations
from tiers.tier4_real_world import TestTier4RealWorldWorkflows
from tiers.tier5_adversarial_hardening import TestTier5AdversarialHardening

TIER_MAP = {
    "1": ("Tier 1: Feature Coverage (R1, R2, R3, R4)", TestTier1FeatureCoverage),
    "2": ("Tier 2: Boundary & Corner Cases (R1, R2, R3, R4)", TestTier2BoundaryCases),
    "3": ("Tier 3: Pairwise Cross-Feature Combinations", TestTier3Combinations),
    "4": ("Tier 4: Real-World Workflows & Parity", TestTier4RealWorldWorkflows),
    "5": ("Tier 5: Adversarial Coverage Hardening", TestTier5AdversarialHardening),
}


def build_suite(selected_tier: str) -> unittest.TestSuite:
    """Build a TestSuite based on the requested tier."""
    suite = unittest.TestSuite()
    loader = unittest.TestLoader()
    norm_tier = str(selected_tier).strip().lower()
    if norm_tier.startswith("tier"):
        norm_tier = norm_tier[4:]

    if norm_tier == "all":
        for _, (_, test_class) in TIER_MAP.items():
            suite.addTests(loader.loadTestsFromTestCase(test_class))
    elif norm_tier in TIER_MAP:
        _, test_class = TIER_MAP[norm_tier]
        suite.addTests(loader.loadTestsFromTestCase(test_class))
    else:
        raise ValueError(f"Invalid tier selection: {selected_tier}")

    return suite


def inspect_tests(selected_tier: str):
    """Print an inventory of all test cases with their requirements and docstrings."""
    print("=" * 80)
    print("  E2E TEST SUITE INVENTORY — DIFFERENTIAL TESTING PLATFORM")
    print("=" * 80)

    norm_tier = str(selected_tier).strip().lower()
    if norm_tier.startswith("tier"):
        norm_tier = norm_tier[4:]

    tiers_to_inspect = TIER_MAP.keys() if norm_tier == "all" else [norm_tier]
    loader = unittest.TestLoader()
    total_count = 0

    for t_id in tiers_to_inspect:
        title, test_class = TIER_MAP[t_id]
        tests = loader.loadTestsFromTestCase(test_class)
        print(f"\n[Tier {t_id}] {title} ({tests.countTestCases()} tests)")
        print("-" * 80)

        for test in tests:
            total_count += 1
            method_name = test._testMethodName
            method = getattr(test, method_name)
            doc = (method.__doc__ or "").strip().split("\n")[0]
            print(f"  * {method_name:<50} | {doc}")

    print("\n" + "=" * 80)
    print(f"Total tests cataloged: {total_count}")
    print("=" * 80)


def run_tests(selected_tier: str, verbose: bool = False, json_out: str = None) -> int:
    """Run the selected tests and return process exit code (0 = pass, 1 = fail)."""
    start_time = time.time()
    print("=" * 80)
    print("  DIFFERENTIAL TESTING PLATFORM — E2E TEST SUITE")
    print(f"  Target: {selected_tier.upper()} | Start: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 80 + "\n")

    suite = build_suite(selected_tier)
    verbosity_level = 2 if verbose else 1

    stream = io.StringIO()
    runner = unittest.TextTestRunner(stream=stream, verbosity=verbosity_level)
    result = runner.run(suite)
    duration = time.time() - start_time

    # Output test details to console
    print(stream.getvalue())

    # Summary calculations
    total = result.testsRun
    failed = len(result.failures)
    errors = len(result.errors)
    skipped = len(result.skipped)
    passed = total - (failed + errors + skipped)
    pass_rate = (passed / total * 100.0) if total > 0 else 0.0

    print("=" * 80)
    print("  TEST EXECUTION SUMMARY")
    print("=" * 80)
    print(f"  Total Tests  : {total}")
    print(f"  Passed       : {passed}")
    print(f"  Failed       : {failed}")
    print(f"  Errors       : {errors}")
    print(f"  Skipped      : {skipped}")
    print(f"  Pass Rate    : {pass_rate:.1f}%")
    print(f"  Duration     : {duration:.3f} seconds")
    print("-" * 80)

    if failed == 0 and errors == 0:
        print("  VERDICT: [PASS] All tests executed and passed successfully.")
        exit_code = 0
    else:
        print("  VERDICT: [FAIL] Test failures encountered.")
        exit_code = 1
    print("=" * 80 + "\n")

    if json_out:
        summary_data = {
            "platform": "cg50_differential_testing_platform",
            "tier_selected": selected_tier,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "total_tests": total,
            "passed": passed,
            "failed": failed,
            "errors": errors,
            "skipped": skipped,
            "pass_rate_percent": round(pass_rate, 2),
            "duration_seconds": round(duration, 3),
            "verdict": "PASS" if exit_code == 0 else "FAIL",
            "failures": [
                {"test": str(test), "traceback": trace} for test, trace in result.failures
            ],
            "errors_list": [
                {"test": str(test), "traceback": trace} for test, trace in result.errors
            ]
        }
        out_dir = os.path.dirname(os.path.abspath(json_out))
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        with open(json_out, "w", encoding="utf-8") as f:
            json.dump(summary_data, f, indent=2)
        print(f"Execution report written to: {json_out}")

    return exit_code


def main() -> int:
    parser = argparse.ArgumentParser(
        description="E2E Test Runner for Differential Testing Platform (Tiers 1-4)"
    )
    parser.add_argument(
        "--tier",
        choices=["1", "2", "3", "4", "5", "tier1", "tier2", "tier3", "tier4", "tier5", "all"],
        default="all",
        help="Select test tier to execute (1/tier1: Features, 2/tier2: Boundaries, 3/tier3: Combinations, 4/tier4: Real-World, 5/tier5: Adversarial, all: All tiers)"
    )
    parser.add_argument(
        "--inspect", "--dry-run",
        dest="inspect",
        action="store_true",
        help="Inspect/list all test cases without executing"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable detailed verbose output for every test case"
    )
    parser.add_argument(
        "--out-json",
        metavar="PATH",
        help="Save test results summary to a JSON file"
    )

    args = parser.parse_args()

    if args.inspect:
        inspect_tests(args.tier)
        return 0

    return run_tests(selected_tier=args.tier, verbose=args.verbose, json_out=args.out_json)


if __name__ == "__main__":
    sys.exit(main())
