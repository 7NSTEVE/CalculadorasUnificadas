#!/usr/bin/env python3
"""
Unit tests for diff_comparator.py — explicitly exercises ALL 5 forensic audit
counterexamples from Finding H-002 plus positive/negative paths for every label.
"""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from diff_comparator import classify_case


class DiffComparatorAuditTests(unittest.TestCase):
    # Counterexample 1 (H-002): PASS without any oracle must NOT return MATCH_BOTH
    def test_pass_without_oracle_returns_no_reference(self):
        cls, _ = classify_case({"case_id": "X1", "status": "PASS", "observed": "42"}, None, None)
        self.assertEqual(cls, "NO_REFERENCE")

    # Counterexample 2 (H-002): PASS with contradictory official Manager PLUS must return CORE_BUG
    def test_pass_with_contradictory_manager_plus_returns_core_bug(self):
        cls, _ = classify_case(
            {"case_id": "X2", "status": "PASS", "observed": "42", "category": "arithmetic", "expected": "42"},
            {"exact_50dps": "42", "display_10sig": "42"},
            {"passed": False, "ocr_text": "43", "normalized_result": "43"},
        )
        self.assertEqual(cls, "CORE_BUG")

    # Counterexample 2b: PASS where Manager PLUS passed flag is True but normalized_result disagrees
    def test_pass_with_value_mismatch_manager_plus_returns_core_bug(self):
        cls, _ = classify_case(
            {"case_id": "X2B", "status": "PASS", "observed": "42", "category": "arithmetic", "expected": "42"},
            {"exact_50dps": "42", "display_10sig": "42"},
            {"passed": True, "normalized_result": "41"},
        )
        self.assertEqual(cls, "CORE_BUG")

    # Counterexample 3 (H-002): Empty test record must return INCOMPLETE_TEST
    def test_empty_record_returns_incomplete_test(self):
        cls, _ = classify_case({}, None, None)
        self.assertEqual(cls, "INCOMPLETE_TEST")

    # Counterexample 4 (H-002): ORACLE_AUDIT without any oracle must NOT return MATCH_CASIO_FIDELITY
    def test_oracle_audit_without_oracle_returns_no_reference(self):
        cls, _ = classify_case(
            {"case_id": "X4", "status": "PASS", "expected": "ORACLE_AUDIT", "observed": "0"},
            None,
            None,
        )
        self.assertEqual(cls, "NO_REFERENCE")

    # Counterexample 5 (H-002): Non-zero FPU opcodes WITH version_diff MUST still be CORE_BUG
    def test_fpu_violation_cannot_be_masked_by_version_diff(self):
        cls, _ = classify_case(
            {"case_id": "X5", "status": "PASS", "observed": "1", "fpu_opcodes": 3, "version_diff": True},
            {"exact_50dps": "1", "display_10sig": "1"},
            {"passed": True},
        )
        self.assertEqual(cls, "CORE_BUG")

    # Positive: BOTH math oracle AND Manager PLUS agree -> MATCH_BOTH
    def test_both_oracles_agree_returns_match_both(self):
        cls, _ = classify_case(
            {"case_id": "P1", "status": "PASS", "observed": "1", "category": "cancellation_bcd", "expected": "1"},
            {"exact_50dps": "1", "display_10sig": "1"},
            {"passed": True, "normalized_result": "1"},
        )
        self.assertEqual(cls, "MATCH_BOTH")

    # Positive: Math oracle only (e.g. parametric 3K case not run individually on Manager PLUS) -> MATCH_MATH_ONLY
    def test_math_oracle_only_returns_match_math_only(self):
        cls, _ = classify_case(
            {"case_id": "P2", "status": "PASS", "observed": "5", "category": "arithmetic", "expected": "5"},
            None,
            None,
        )
        self.assertEqual(cls, "MATCH_MATH_ONLY")

    # Positive: Manager PLUS only (e.g. error dialog) -> MATCH_CASIO_ONLY
    def test_manager_plus_only_returns_match_casio_only(self):
        cls, _ = classify_case(
            {"case_id": "P3", "status": "PASS", "observed": "ERR_DIALOG", "category": "error_handling"},
            None,
            {"passed": True},
        )
        self.assertEqual(cls, "MATCH_CASIO_ONLY")

    # Positive: Casio BCD15 fidelity verified with mpmath oracle -> MATCH_CASIO_FIDELITY
    def test_bcd15_fidelity_with_oracle_returns_match_casio_fidelity(self):
        cls, _ = classify_case(
            {"case_id": "CANCEL-002", "status": "PASS", "observed": "0", "category": "cancellation_bcd"},
            {"exact_50dps": "1", "display_10sig": "1", "casio_bcd15_expected": "0"},
            {"passed": True, "normalized_result": "0"},
        )
        self.assertEqual(cls, "MATCH_CASIO_FIDELITY")


if __name__ == "__main__":
    unittest.main(verbosity=2)
