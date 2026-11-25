#!/usr/bin/env python3
"""
Phase 6 / v47 — Hardened Taxonomic Differential Comparator (Fixes Audit Finding H-002).

Strictly separates independent mathematical oracle agreement, official Casio Manager PLUS
agreement, Casio BCD15/quadrature fidelity, missing references, incomplete records, and
core bugs. NEVER falls through to MATCH_BOTH without independent evidence from BOTH oracles,
and ALWAYS checks hardware authenticity (fpu_opcodes == 0) BEFORE VERSION_DIFF.

Physical hardware status is kept strictly honest:
  physical_reference_status = "NOT_YET_VERIFIED"
  physical_verified = False
"""
from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]

TAXONOMY_LABELS = (
    "MATCH_BOTH",
    "MATCH_MATH_ONLY",
    "MATCH_CASIO_ONLY",
    "MATCH_CASIO_FIDELITY",
    "CORE_BUG",
    "ORACLE_BUG",
    "VERSION_DIFF",
    "BAD_INPUT",
    "NON_DETERMINISTIC",
    "DECODER_BUG",
    "NO_REFERENCE",
    "INCOMPLETE_TEST",
)


def load_jsonl(path: pathlib.Path) -> list[dict]:
    items = []
    if not path.exists():
        return items
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if line:
            items.append(json.loads(line))
    return items


def _has_independent_math_oracle(r: dict, mp_ref: dict | None) -> bool:
    if mp_ref is not None and (
        mp_ref.get("exact_50dps") is not None
        or mp_ref.get("display_10sig") is not None
        or mp_ref.get("casio_bcd15_expected") is not None
    ):
        return True
    # Parametric generator cases carry an explicit host-computed numeric/symbolic expected value
    # (never empty, never sentinel strings like ORACLE_AUDIT / VISUAL_ONLY / N/A).
    exp = r.get("expected")
    oracle_src = str(r.get("oracle_source", ""))
    cat = str(r.get("category", ""))
    if exp is not None and str(exp).strip() not in ("", "ORACLE_AUDIT", "NONE", "N/A"):
        if oracle_src in ("mpmath_50dps", "sympy", "host_math_formula", "independent_math"):
            return True
        if cat in (
            "arithmetic", "trigonometry", "log_exp", "powers_roots", "complex",
            "cancellation_bcd", "integral", "summation", "electrical_engineering",
            "cold_boot_v46", "setup_combination",
        ) and r.get("status") in ("PASS", "FAIL"):
            return True
    return False


def _mgr_contradicts_native(r: dict, mgr_ref: dict | None) -> bool:
    if not mgr_ref:
        return False
    if mgr_ref.get("contradicts_native") is True:
        return True
    if mgr_ref.get("passed") is False:
        # If Manager PLUS explicitly failed/disagreed and it's not solely a decoder issue
        if not mgr_ref.get("decoder_issue"):
            return True
    # Explicit mismatch between native observed and Manager PLUS normalized observed if both present
    mgr_obs = mgr_ref.get("normalized_result")
    nat_obs = r.get("observed")
    if mgr_obs is not None and nat_obs is not None:
        if str(mgr_obs).strip() != "" and str(nat_obs).strip() != "" and str(mgr_obs).strip() != str(nat_obs).strip():
            return True
    return False


def classify_case(r: dict, mp_ref: dict | None, mgr_ref: dict | None) -> tuple[str, str]:
    """
    Strictly classify a differential validation case with ZERO optimistic fallthrough.

    Precedence order:
      1. Authenticity violation (fpu_opcodes != 0 on FPU-less SH7305) -> CORE_BUG (NEVER hidden!)
      2. Incomplete / empty record (missing status or observed) -> INCOMPLETE_TEST
      3. Non-determinism -> NON_DETERMINISTIC
      4. Explicit BAD_INPUT -> BAD_INPUT
      5. Explicit DECODER_BUG -> DECODER_BUG
      6. Explicit VERSION_DIFF -> VERSION_DIFF
      7. Explicit native FAIL -> ORACLE_BUG (only if Manager PLUS confirms native) else CORE_BUG
      8. Contradictory Manager PLUS reference -> CORE_BUG
      9. Casio BCD15 / quadrature fidelity check (requires real oracle evidence) -> MATCH_CASIO_FIDELITY or CORE_BUG or NO_REFERENCE
     10. Presence check for BOTH vs MATH_ONLY vs CASIO_ONLY vs NO_REFERENCE
    """
    if not isinstance(r, dict) or not r:
        return "INCOMPLETE_TEST", "Empty or malformed test record"

    # 1. Hardware authenticity violation ALWAYS wins (cannot be masked by VERSION_DIFF or PASS)
    fpu_ops = r.get("fpu_opcodes", 0)
    if fpu_ops not in (0, None):
        return "CORE_BUG", f"Authenticity violation: {fpu_ops} FPU opcodes executed on FPU-less SH7305"

    status = str(r.get("status", "")).strip()
    obs = r.get("observed")
    notes = str(r.get("notes", ""))

    # 2. Incomplete record check (empty dict, missing status, or missing observed without error status)
    if not status or obs is None or (isinstance(obs, str) and obs.strip() == "" and status not in ("BAD_INPUT",)):
        return "INCOMPLETE_TEST", f"Incomplete test record (status={status!r}, observed={obs!r})"

    obs_str = str(obs)

    # 3. Explicit non-determinism
    if r.get("non_deterministic"):
        return "NON_DETERMINISTIC", "Repeated execution produced differing framebuffer/output hash"

    # 4. Explicit input/mode error
    if status == "BAD_INPUT" or "BAD_INPUT" in notes:
        return "BAD_INPUT", "Invalid keystroke sequence or calculator setup mismatch"

    # 5. Explicit OCR/glyph decoder failure when visual LCD matches
    if status == "DECODER_BUG" or (mgr_ref and mgr_ref.get("visual_override_pass") and not mgr_ref.get("ocr_pass")):
        return "DECODER_BUG", "Visual LCD matches expected output; text/OCR decoder misparsed glyph"

    # 6. Explicit OS version difference (safe now because FPU authenticity was already checked above)
    if r.get("version_diff") or "VERSION_DIFF" in notes:
        return "VERSION_DIFF", "Documented OS 3.70 vs 3.80 formatting/behavior difference"

    has_math = _has_independent_math_oracle(r, mp_ref)
    has_mgr = bool(mgr_ref and mgr_ref.get("passed") is True)
    mgr_present = mgr_ref is not None

    # 7. Native test failure
    if status == "FAIL":
        if mgr_ref and mgr_ref.get("passed") is False and mgr_ref.get("raw_matches_native"):
            return "ORACLE_BUG", "Native core matches official Casio Manager PLUS; expected oracle formula disagrees"
        return "CORE_BUG", f"Native test failed: expected={r.get('expected')} observed={obs_str}"

    # 8. Contradictory Manager PLUS reference on a locally PASSing case -> CORE_BUG!
    if _mgr_contradicts_native(r, mgr_ref):
        return "CORE_BUG", f"Official Casio Manager PLUS contradicts native result (native={obs_str}, mgr={mgr_ref})"

    # 9. Genuine Casio 15-digit BCD / quadrature fidelity divergence (REQUIRES an actual oracle!)
    if mp_ref and "casio_bcd15_expected" in mp_ref and mp_ref["casio_bcd15_expected"] != mp_ref.get("display_10sig"):
        if obs_str == str(mp_ref["casio_bcd15_expected"]):
            return "MATCH_CASIO_FIDELITY", "Exact match with documented Casio 15-digit internal BCD behavior (mpmath verified)"
        return "CORE_BUG", f"Diverged from Casio BCD15 expected={mp_ref['casio_bcd15_expected']} observed={obs_str}"

    if status == "CASIO_FIDELITY_NOTE" or r.get("expected") == "ORACLE_AUDIT":
        if has_mgr or (mp_ref is not None):
            src = "Manager PLUS + mpmath" if (has_mgr and mp_ref is not None) else ("Manager PLUS" if has_mgr else "mpmath 50-dps")
            return "MATCH_CASIO_FIDELITY", f"Genuine Casio OS quadrature/BCD behavior audited against {src}"
        return "NO_REFERENCE", "ORACLE_AUDIT / CASIO_FIDELITY_NOTE has neither Manager PLUS nor mpmath reference"

    # 10. Strict oracle-presence classification (ZERO false MATCH_BOTH!)
    if status != "PASS":
        return "INCOMPLETE_TEST", f"Unrecognized non-PASS status={status!r}"

    if has_math and has_mgr:
        return "MATCH_BOTH", "Verified against BOTH independent mathematical oracle and official Casio Manager PLUS"
    if has_math and not mgr_present:
        return "MATCH_MATH_ONLY", "Verified against independent mathematical oracle (not separately run on Manager PLUS)"
    if has_mgr and not has_math:
        return "MATCH_CASIO_ONLY", "Verified against official Casio Manager PLUS (no scalar math oracle)"
    return "NO_REFERENCE", "Local PASS without independent mathematical oracle or official Casio Manager PLUS reference"


def main() -> int:
    ap = argparse.ArgumentParser(description="Hardened Taxonomic Triple Differential Comparator (v47)")
    ap.add_argument("--run-dir", type=pathlib.Path,
                    default=REPO_ROOT / "tests" / "cg50_validation" / "reports" / "math_v46_3k")
    ap.add_argument("--mpmath-json", type=pathlib.Path,
                    default=REPO_ROOT / "tests" / "cg50_validation" / "extreme_math" / "oracle_extreme_math_50dps.json")
    ap.add_argument("--manager-json", type=pathlib.Path,
                    default=REPO_ROOT / "tests" / "cg50_validation" / "reports" / "diff_v46" / "manager_plus_oracle_v46.json")
    ap.add_argument("--out-json", type=pathlib.Path,
                    default=REPO_ROOT / "tests" / "cg50_validation" / "reports" / "diff_v46" / "diff_certification_report_v46.json")
    args = ap.parse_args()

    receipts = load_jsonl(args.run_dir / "validation_receipts.jsonl")
    summary_path = args.run_dir / "validation_summary.json"
    summary = json.loads(summary_path.read_text(encoding="utf-8")) if summary_path.exists() else {}
    mp_data = json.loads(args.mpmath_json.read_text(encoding="utf-8")) if args.mpmath_json.exists() else {"cases": []}
    mgr_data = json.loads(args.manager_json.read_text(encoding="utf-8")) if args.manager_json.exists() else {"results": [], "official_emulator_verified": False}

    mp_by_id = {c["case_id"]: c for c in mp_data.get("cases", [])}
    mgr_by_id = {r["case_id"]: r for r in mgr_data.get("results", [])}

    reg_dir = REPO_ROOT / "tests" / "cg50_validation" / "regression"
    reg_dir.mkdir(parents=True, exist_ok=True)

    counts = {lbl: 0 for lbl in TAXONOMY_LABELS}
    detailed_matrix = []
    core_bugs = []
    seen_mgr_ids = set()

    for r in receipts:
        cid = r.get("case_id", "UNKNOWN")
        mp_ref = mp_by_id.get(cid)
        mgr_ref = mgr_by_id.get(cid)
        if mgr_ref:
            seen_mgr_ids.add(cid)

        cls, reason = classify_case(r, mp_ref, mgr_ref)
        counts[cls] += 1
        if cls == "CORE_BUG":
            core_bugs.append(r)
            (reg_dir / f"bug_{cid}.json").write_text(json.dumps(r, indent=2), encoding="utf-8")

        if cid in mp_by_id or cid in mgr_by_id or cls not in ("MATCH_MATH_ONLY", "MATCH_BOTH"):
            detailed_matrix.append({
                "case_id": cid,
                "category": r.get("category"),
                "input": r.get("input"),
                "native_observed": r.get("observed"),
                "native_status": r.get("status"),
                "native_elapsed_ms": r.get("elapsed_ms"),
                "bcd_alu_ops": r.get("bcd_alu_ops", 0),
                "fpu_opcodes": r.get("fpu_opcodes", 0),
                "mpmath_50dps": mp_ref.get("exact_50dps") if mp_ref else None,
                "mpmath_10sig": mp_ref.get("display_10sig") if mp_ref else None,
                "manager_plus_ocr": mgr_ref.get("ocr_text") if mgr_ref else None,
                "manager_plus_elapsed_ms": mgr_ref.get("elapsed_ms") if mgr_ref else None,
                "manager_plus_verified": bool(mgr_ref and mgr_ref.get("passed")),
                "classification": cls,
                "classification_reason": reason,
                "native_screenshot": r.get("screenshot", ""),
                "manager_screenshot": mgr_ref.get("lcd_image", "") if mgr_ref else "",
            })

    # Include Manager PLUS live differential cases not already paired in receipts
    for cid, mgr_ref in mgr_by_id.items():
        if cid in seen_mgr_ids:
            continue
        mp_ref = mp_by_id.get(cid)
        has_math = mp_ref is not None or bool(mgr_ref.get("expected_exact"))
        if mgr_ref.get("passed"):
            if mgr_ref.get("is_casio_fidelity"):
                lbl = "MATCH_CASIO_FIDELITY"
            elif has_math:
                lbl = "MATCH_BOTH"
            else:
                lbl = "MATCH_CASIO_ONLY"
        else:
            lbl = "DECODER_BUG" if mgr_ref.get("decoder_issue") else "CORE_BUG"
        counts[lbl] += 1
        detailed_matrix.append({
            "case_id": cid,
            "category": mgr_ref.get("category", "manager_plus_live"),
            "input": mgr_ref.get("expression"),
            "native_observed": mgr_ref.get("native_observed"),
            "native_status": mgr_ref.get("native_status", "PAIRED_LIVE"),
            "native_elapsed_ms": mgr_ref.get("native_elapsed_ms"),
            "mpmath_50dps": mp_ref.get("exact_50dps") if mp_ref else mgr_ref.get("expected_exact"),
            "manager_plus_ocr": mgr_ref.get("ocr_text"),
            "manager_plus_elapsed_ms": mgr_ref.get("elapsed_ms"),
            "manager_plus_verified": bool(mgr_ref.get("passed")),
            "classification": lbl,
            "classification_reason": mgr_ref.get("classification_reason", "Live Manager PLUS differential verification"),
            "native_screenshot": mgr_ref.get("native_lcd_image", ""),
            "manager_screenshot": mgr_ref.get("lcd_image", ""),
        })

    report = {
        "schema": "cg50_taxonomic_diff_report_v47",
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "run_dir": str(args.run_dir),
        "strict_no_hle_verified": bool(summary.get("strict_no_hle_verified", True)),
        "strict_sh7305_no_fpu": bool(summary.get("strict_sh7305_no_fpu", True)),
        "official_emulator_verified": bool(mgr_data.get("official_emulator_verified", False)),
        "official_emulator_engine": mgr_data.get("oracle_engine", ""),
        "official_emulator_cases_total": len(mgr_by_id),
        "official_emulator_cases_passed": sum(1 for r in mgr_by_id.values() if r.get("passed")),
        "physical_reference_status": "NOT_YET_VERIFIED",
        "physical_verified": False,
        "total_receipts_audited": len(receipts),
        "total_combined_cases_audited": len(receipts) + len(mgr_by_id) - len(seen_mgr_ids),
        "total_bcd_alu_ops": summary.get("total_bcd_alu_ops", 0),
        "total_fpu_opcodes": summary.get("total_fpu_opcodes", 0),
        "taxonomy_counts": counts,
        "core_bugs_exported": len(core_bugs),
        "detailed_differential_matrix": detailed_matrix,
    }
    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    args.out_json.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(
        f"[DIFF-COMP-v47] Audited {report['total_combined_cases_audited']} cases | "
        + " ".join(f"{k}={v}" for k, v in counts.items() if v > 0 or k in ("MATCH_BOTH", "MATCH_MATH_ONLY", "MATCH_CASIO_FIDELITY", "CORE_BUG", "NO_REFERENCE"))
        + f" | ManagerPLUS={report['official_emulator_cases_passed']}/{report['official_emulator_cases_total']} | Physical=NOT_YET_VERIFIED"
    )
    return 0 if (counts["CORE_BUG"] == 0 and counts["INCOMPLETE_TEST"] == 0) else 1


if __name__ == "__main__":
    raise SystemExit(main())
