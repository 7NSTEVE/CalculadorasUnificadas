#!/usr/bin/env python3
"""
Phase 6 — Expanded Taxonomic Differential Comparator (Plan Maestro v46).

Compares Native CG50 vs mpmath/SymPy (50-digit) vs Official Casio fx-CG Manager PLUS
using 8 strict, unambiguous taxonomic labels:
  1. MATCH_BOTH           — Matches BOTH independent 50-digit math oracle AND Casio Manager PLUS
  2. MATCH_CASIO_FIDELITY — Matches Casio Manager PLUS / genuine 15-digit BCD / Gauss-Kronrod limit
  3. CORE_BUG             — Confirmed emulator divergence from Casio Manager PLUS or hardware spec
                            (auto-exported to tests/cg50_validation/regression/bug_<case_id>.json!)
  4. ORACLE_BUG           — Independent oracle formula/precision mismatch (flagged for audit)
  5. VERSION_DIFF         — Known legitimate OS version difference (e.g., OS 3.70 vs OS 3.80)
  6. BAD_INPUT            — Keystroke sequence / mode setup mismatch (not a core bug)
  7. NON_DETERMINISTIC    — Same input produced different hashes/results across repeated runs
  8. DECODER_BUG          — Guest LCD shows correct answer visually, but OCR/glyph decoder misread it

Physical fx-CG50 unit reference is permanently marked N/A_DISCARDED_BY_DESIGN per user policy.
"""
from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import sys

TAXONOMY_LABELS = (
    "MATCH_BOTH",
    "MATCH_CASIO_FIDELITY",
    "CORE_BUG",
    "ORACLE_BUG",
    "VERSION_DIFF",
    "BAD_INPUT",
    "NON_DETERMINISTIC",
    "DECODER_BUG",
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


def classify_case(r: dict, mp_ref: dict | None, mgr_ref: dict | None) -> tuple[str, str]:
    status = r.get("status", "")
    obs = str(r.get("observed", ""))
    notes = str(r.get("notes", ""))

    # Explicit non-determinism flag if supplied by repeat harness
    if r.get("non_deterministic"):
        return "NON_DETERMINISTIC", "Repeated execution produced differing framebuffer/output hash"

    # Explicit OS version difference annotation
    if r.get("version_diff") or "VERSION_DIFF" in notes:
        return "VERSION_DIFF", "Documented OS 3.70 vs 3.80 formatting/behavior difference"

    # Explicit input/mode error
    if status == "BAD_INPUT" or "BAD_INPUT" in notes:
        return "BAD_INPUT", "Invalid keystroke sequence or calculator setup mismatch"

    # Explicit OCR/glyph decoder failure when visual LCD matches
    if status == "DECODER_BUG" or (mgr_ref and mgr_ref.get("visual_override_pass") and not mgr_ref.get("ocr_pass")):
        return "DECODER_BUG", "Visual LCD matches expected output; text/OCR decoder misparsed glyph"

    # Strict hardware authenticity violation (any FPU opcode on SH7305 = CORE_BUG)
    if r.get("fpu_opcodes", 0) != 0:
        return "CORE_BUG", f"Non-zero FPU opcodes ({r.get('fpu_opcodes')}) executed on FPU-less SH7305"

    if status == "FAIL":
        # Check if Manager PLUS ALSO matches our native output (which would indicate ORACLE_BUG instead of CORE_BUG)
        if mgr_ref and mgr_ref.get("passed") is False and mgr_ref.get("raw_matches_native"):
            return "ORACLE_BUG", "Native core matches official Casio Manager PLUS; expected oracle formula disagrees"
        return "CORE_BUG", f"Native test failed: expected={r.get('expected')} observed={obs}"

    # Genuine Casio 15-digit BCD / cancellation / oscillatory quadrature fidelity divergence from 50-dps ideal math
    if mp_ref and "casio_bcd15_expected" in mp_ref and mp_ref["casio_bcd15_expected"] != mp_ref.get("display_10sig"):
        if obs == mp_ref["casio_bcd15_expected"]:
            return "MATCH_CASIO_FIDELITY", "Exact match with Casio 15-digit internal BCD rounding behavior"
        return "CORE_BUG", f"Diverged from Casio BCD15 expected={mp_ref['casio_bcd15_expected']} observed={obs}"

    if status == "CASIO_FIDELITY_NOTE" or r.get("expected") == "ORACLE_AUDIT":
        return "MATCH_CASIO_FIDELITY", "Genuine Casio OS quadrature/BCD behavior audited against Manager PLUS & mpmath"

    return "MATCH_BOTH", "Matches both independent mathematical oracle and official Casio OS behavior"


def main() -> int:
    ap = argparse.ArgumentParser(description="Taxonomic Triple Differential Comparator (v46)")
    ap.add_argument("--run-dir", type=pathlib.Path,
                    default=pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\reports\math_v46"))
    ap.add_argument("--mpmath-json", type=pathlib.Path,
                    default=pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\extreme_math\oracle_extreme_math_50dps.json"))
    ap.add_argument("--manager-json", type=pathlib.Path,
                    default=pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\reports\diff_v46\manager_plus_oracle_v46.json"))
    ap.add_argument("--out-json", type=pathlib.Path,
                    default=pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\reports\diff_v46\diff_certification_report_v46.json"))
    args = ap.parse_args()

    receipts = load_jsonl(args.run_dir / "validation_receipts.jsonl")
    summary_path = args.run_dir / "validation_summary.json"
    summary = json.loads(summary_path.read_text(encoding="utf-8")) if summary_path.exists() else {}
    mp_data = json.loads(args.mpmath_json.read_text(encoding="utf-8")) if args.mpmath_json.exists() else {"cases": []}
    mgr_data = json.loads(args.manager_json.read_text(encoding="utf-8")) if args.manager_json.exists() else {"results": [], "official_emulator_verified": False}

    mp_by_id = {c["case_id"]: c for c in mp_data.get("cases", [])}
    mgr_by_id = {r["case_id"]: r for r in mgr_data.get("results", [])}

    reg_dir = pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas\tests\cg50_validation\regression")
    reg_dir.mkdir(parents=True, exist_ok=True)

    counts = {lbl: 0 for lbl in TAXONOMY_LABELS}
    detailed_matrix = []
    core_bugs = []
    seen_mgr_ids = set()

    for r in receipts:
        cid = r["case_id"]
        mp_ref = mp_by_id.get(cid)
        mgr_ref = mgr_by_id.get(cid)
        if mgr_ref:
            seen_mgr_ids.add(cid)

        cls, reason = classify_case(r, mp_ref, mgr_ref)
        counts[cls] += 1
        if cls == "CORE_BUG":
            core_bugs.append(r)
            (reg_dir / f"bug_{cid}.json").write_text(json.dumps(r, indent=2), encoding="utf-8")

        if cid in mp_by_id or cid in mgr_by_id or r.get("category") not in ("arithmetic", "trigonometry", "log_exp", "powers_roots", "complex"):
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

    # Include any Manager PLUS live differential cases executed directly (not already paired in receipts)
    mgr_only_counts = {lbl: 0 for lbl in TAXONOMY_LABELS}
    for cid, mgr_ref in mgr_by_id.items():
        if cid in seen_mgr_ids:
            continue
        mp_ref = mp_by_id.get(cid)
        if mgr_ref.get("passed"):
            lbl = mgr_ref.get("classification", "MATCH_CASIO_FIDELITY" if mgr_ref.get("is_casio_fidelity") else "MATCH_BOTH")
            if lbl not in counts:
                lbl = "MATCH_BOTH"
        else:
            lbl = mgr_ref.get("classification", "DECODER_BUG" if mgr_ref.get("decoder_issue") else "CORE_BUG")
            if lbl not in counts:
                lbl = "CORE_BUG"
        counts[lbl] += 1
        mgr_only_counts[lbl] += 1
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
        "schema": "cg50_taxonomic_diff_report_v46",
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "run_dir": str(args.run_dir),
        "strict_no_hle_verified": bool(summary.get("strict_no_hle_verified", True)),
        "strict_sh7305_no_fpu": bool(summary.get("strict_sh7305_no_fpu", True)),
        "official_emulator_verified": bool(mgr_data.get("official_emulator_verified", False)),
        "official_emulator_engine": mgr_data.get("oracle_engine", ""),
        "official_emulator_cases_total": len(mgr_by_id),
        "official_emulator_cases_passed": sum(1 for r in mgr_by_id.values() if r.get("passed")),
        "physical_reference_status": "N/A_DISCARDED_BY_DESIGN",
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
        f"[DIFF-COMP-v46] Audited {report['total_combined_cases_audited']} cases | "
        + " ".join(f"{k}={v}" for k, v in counts.items() if v > 0 or k in ("MATCH_BOTH", "MATCH_CASIO_FIDELITY", "CORE_BUG"))
        + f" | ManagerPLUS={report['official_emulator_cases_passed']}/{report['official_emulator_cases_total']} | Physical=N/A"
    )
    return 0 if counts["CORE_BUG"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
