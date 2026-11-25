"""Main CLI runner for the 4-tier opaque-box E2E test suite (pure Python 3 stdlib)."""
from __future__ import annotations

import argparse
import datetime
import io
import json
import os
import sys
import unittest
from pathlib import Path
from typing import Any


def _ensure_import_paths() -> Path:
    suite_dir = Path(__file__).resolve().parent
    tests_dir = suite_dir.parent
    repo_root = tests_dir.parent
    if str(tests_dir) not in sys.path:
        sys.path.insert(0, str(tests_dir))
    if str(repo_root) not in sys.path:
        sys.path.insert(1, str(repo_root))
    # Guard against a third-party `tests` package in site-packages shadowing `<repo>/tests`
    try:
        import tests as _tests_pkg  # type: ignore[import-not-found]
        pkg_paths = getattr(_tests_pkg, "__path__", None)
        if pkg_paths is not None and str(tests_dir) not in pkg_paths:
            pkg_paths.insert(0, str(tests_dir))
    except ImportError:
        pass
    return repo_root


_REPO_ROOT = _ensure_import_paths()

from e2e_suite.adapters.config import detect_capabilities, is_linux_host, resolve_paths  # noqa: E402
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks, scrub_secrets  # noqa: E402
from e2e_suite.adapters.registry import (  # noqa: E402
    E2ETestResult,
    get_case_meta,
    get_readiness_entry,
    iter_suite_tests,
    load_readiness_manifest,
    set_runtime_context,
)


def _match_filters(
    meta: dict[str, Any],
    *,
    tier: str,
    groups: set[str],
    substr: str,
) -> bool:
    if tier != "all" and str(meta.get("tier", "")) != tier:
        return False
    if groups and str(meta.get("group", "")).upper() not in groups:
        return False
    if substr:
        haystack = " ".join([
            str(meta.get("case_id", "")),
            str(meta.get("group", "")),
            " ".join(str(r) for r in meta.get("req_ids", [])),
            str(meta.get("readiness_key", "")),
        ]).lower()
        if substr.lower() not in haystack:
            return False
    return True


def _filter_suite(
    suite: unittest.TestSuite,
    *,
    tier: str,
    groups: set[str],
    substr: str,
) -> unittest.TestSuite:
    filtered = unittest.TestSuite()
    for test in iter_suite_tests(suite):
        meta = get_case_meta(test)
        if _match_filters(meta, tier=tier, groups=groups, substr=substr):
            filtered.addTest(test)
    return filtered


def _discover_tier_suite(tiers_dir: Path) -> unittest.TestSuite:
    loader = unittest.TestLoader()
    if not tiers_dir.exists():
        return unittest.TestSuite()
    return loader.discover(
        start_dir=str(tiers_dir),
        pattern="tier*.py",
        top_level_dir=str(tiers_dir.parents[1]),
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Calculadora Unificada & Document Studio 4-Tier E2E Runner"
    )
    parser.add_argument(
        "--tier",
        choices=["1", "2", "3", "4", "all"],
        default="all",
        help="Tier to inspect or execute",
    )
    parser.add_argument(
        "--platform",
        choices=["win", "linux", "both"],
        default="linux" if is_linux_host() else "win",
        help="Target platform filter",
    )
    parser.add_argument(
        "--group",
        default="",
        help="Optional group filter (e.g. G01 or G18,G19)",
    )
    parser.add_argument(
        "--filter",
        default="",
        help="Substring filter across case_id, req_ids, readiness_key",
    )
    parser.add_argument(
        "--inspect",
        "--dry-run",
        dest="inspect",
        action="store_true",
        help="Discover and print case inventory without executing tests",
    )
    parser.add_argument(
        "--strict-ready",
        action="store_true",
        help="Treat any PENDING feature in readiness_manifest.json as a hard failure (exit 2)",
    )
    parser.add_argument(
        "--out-json",
        default="",
        help="Optional output path for machine-readable JSON summary",
    )
    parser.add_argument(
        "--keep-artifacts",
        action="store_true",
        help="Preserve keepDir artifacts created during this session",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Verbose per-case output",
    )

    args = parser.parse_args(argv)

    if args.keep_artifacts:
        os.environ["E2E_KEEP_ARTIFACTS"] = "1"
    if args.strict_ready:
        os.environ["E2E_STRICT_READY"] = "1"
    os.environ["E2E_TARGET_PLATFORM"] = args.platform

    paths = resolve_paths(_REPO_ROOT)
    set_runtime_context(
        strict_ready=args.strict_ready,
        target_platform=args.platform,
        manifest_path=paths["readiness_manifest"],
    )
    manifest = load_readiness_manifest(paths["readiness_manifest"])
    groups = {g.strip().upper() for g in args.group.split(",") if g.strip()}

    raw_suite = _discover_tier_suite(paths["suite_root"] / "tiers")
    suite = _filter_suite(raw_suite, tier=args.tier, groups=groups, substr=args.filter)
    tests = iter_suite_tests(suite)

    if args.inspect:
        caps = detect_capabilities(_REPO_ROOT, probe_wsl=False)
        inspected_cases: list[dict[str, Any]] = []
        tier_counts: dict[str, int] = {}
        readiness_counts: dict[str, int] = {}
        for t in tests:
            meta = get_case_meta(t)
            entry = get_readiness_entry(meta["readiness_key"], manifest)
            rstatus = str(entry.get("status", "READY"))
            tier_key = str(meta.get("tier", 0))
            tier_counts[tier_key] = tier_counts.get(tier_key, 0) + 1
            readiness_counts[rstatus] = readiness_counts.get(rstatus, 0) + 1
            inspected_cases.append({**meta, "readiness_status": rstatus})

        report = {
            "schema_version": 1,
            "mode": "inspect",
            "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "host_os": caps["host_os"],
            "target_platform": args.platform,
            "strict_ready": args.strict_ready,
            "capabilities": caps,
            "summary": {
                "total_discovered": len(inspected_cases),
                "by_tier": tier_counts,
                "by_readiness": readiness_counts,
            },
            "cases": inspected_cases,
        }
        print(
            f"[E2E INSPECT] host={caps['host_os']} target={args.platform} "
            f"discovered={len(inspected_cases)} by_tier={tier_counts} by_readiness={readiness_counts}"
        )
        if args.out_json:
            out_p = Path(args.out_json)
            out_p.parent.mkdir(parents=True, exist_ok=True)
            out_p.write_text(scrub_secrets(json.dumps(report, indent=2)), encoding="utf-8")
        return 0

    caps = detect_capabilities(_REPO_ROOT, probe_wsl=(args.platform in ("linux", "both")))
    log_buf = io.StringIO()

    class _TeeStream:
        def write(self, s: str) -> int:
            sys.stdout.write(s)
            log_buf.write(s)
            return len(s)

        def flush(self) -> None:
            sys.stdout.flush()

    result = E2ETestResult(stream=_TeeStream(), verbosity=2 if args.verbose else 1)
    suite.run(result)
    if not args.verbose:
        sys.stdout.write("\n")

    passed = sum(1 for r in result.records if r["status"] == "PASS")
    failed = sum(1 for r in result.records if r["status"] == "FAIL")
    pending = sum(1 for r in result.records if r["status"] == "PENDING")
    skipped = sum(1 for r in result.records if r["status"] == "SKIP")

    # Scrub and scan captured stdout + messages for credential/PII leaks
    captured_text = log_buf.getvalue() + "\n" + "\n".join(r["message"] for r in result.records)
    leaks = scan_text_for_leaks(captured_text)

    for r in result.records:
        r["message"] = scrub_secrets(r["message"])

    report = {
        "schema_version": 1,
        "mode": "execute",
        "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "host_os": caps["host_os"],
        "target_platform": args.platform,
        "strict_ready": args.strict_ready,
        "capabilities": caps,
        "summary": {
            "total": len(result.records),
            "passed": passed,
            "failed": failed,
            "pending": pending,
            "skipped": skipped,
            "privacy_leaks": len(leaks),
        },
        "privacy_findings": leaks,
        "cases": result.records,
    }

    if args.out_json:
        out_p = Path(args.out_json)
        out_p.parent.mkdir(parents=True, exist_ok=True)
        out_p.write_text(scrub_secrets(json.dumps(report, indent=2)), encoding="utf-8")

    print(
        f"[E2E SUMMARY] total={len(result.records)} passed={passed} failed={failed} "
        f"pending={pending} skipped={skipped} privacy_leaks={len(leaks)}"
    )
    if leaks:
        print(f"[PRIVACY_GUARD FAIL] {leaks}")
        return 1
    if failed > 0:
        if args.strict_ready and any("STRICT_READY:" in r["message"] for r in result.records if r["status"] == "FAIL"):
            return 2
        return 1
    if args.strict_ready and pending > 0:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
