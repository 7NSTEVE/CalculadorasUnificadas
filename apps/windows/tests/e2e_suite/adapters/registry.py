"""@e2e_case metadata decorator, readiness manifest gating, and JSON result collector."""
from __future__ import annotations

import functools
import json
import os
import time
import unittest
from pathlib import Path
from typing import Any, Callable

from .config import REPO_ROOT, resolve_paths


class PendingFeatureSkip(unittest.SkipTest):
    """Distinct skip subclass so runner records status='PENDING' instead of 'SKIP'."""


_RUNTIME_CONTEXT: dict[str, Any] = {
    "strict_ready": False,
    "target_platform": "both",
    "manifest_path": resolve_paths(REPO_ROOT)["readiness_manifest"],
    "manifest_cache": None,
}


def load_readiness_manifest(manifest_path: Path | None = None) -> dict[str, Any]:
    path = Path(manifest_path) if manifest_path else _RUNTIME_CONTEXT["manifest_path"]
    if not path.exists():
        return {"features": {}}
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def set_runtime_context(
    *,
    strict_ready: bool | None = None,
    target_platform: str | None = None,
    manifest_path: Path | None = None,
) -> None:
    if strict_ready is not None:
        _RUNTIME_CONTEXT["strict_ready"] = bool(strict_ready)
    if target_platform is not None:
        _RUNTIME_CONTEXT["target_platform"] = target_platform
    if manifest_path is not None:
        _RUNTIME_CONTEXT["manifest_path"] = Path(manifest_path)
    _RUNTIME_CONTEXT["manifest_cache"] = None


def get_runtime_context() -> dict[str, Any]:
    if _RUNTIME_CONTEXT["manifest_cache"] is None:
        _RUNTIME_CONTEXT["manifest_cache"] = load_readiness_manifest(_RUNTIME_CONTEXT["manifest_path"])
    strict_env = os.environ.get("E2E_STRICT_READY", "").strip()
    plat_env = os.environ.get("E2E_TARGET_PLATFORM", "").strip().lower()
    return {
        "strict_ready": _RUNTIME_CONTEXT["strict_ready"] or (strict_env == "1"),
        "target_platform": plat_env or _RUNTIME_CONTEXT["target_platform"],
        "manifest": _RUNTIME_CONTEXT["manifest_cache"],
    }


def get_readiness_entry(readiness_key: str, manifest: dict[str, Any] | None = None) -> dict[str, Any]:
    data = manifest if manifest is not None else get_runtime_context()["manifest"]
    features = data.get("features", {})
    if readiness_key in features:
        return dict(features[readiness_key])
    return {
        "status": "READY",
        "milestone": "UNKNOWN",
        "notes": f"Unlisted key '{readiness_key}' defaults to READY.",
    }


def _platform_matches(case_platforms: list[str], target_platform: str) -> bool:
    norm_case = set()
    for p in case_platforms:
        pl = p.lower().strip()
        if pl == "both":
            norm_case.update({"win", "linux"})
        else:
            norm_case.add(pl)
    if target_platform in ("both", "all", ""):
        return True
    return target_platform.lower() in norm_case


def e2e_case(
    *,
    case_id: str,
    tier: int,
    group: str,
    req_ids: list[str],
    apps: list[str],
    platforms: list[str],
    readiness_key: str,
) -> Callable[[Callable[..., Any]], Callable[..., Any]]:
    """Attach opaque-box E2E metadata and enforce readiness & platform gating."""

    meta = {
        "case_id": case_id,
        "tier": int(tier),
        "group": group,
        "req_ids": list(req_ids),
        "apps": list(apps),
        "platforms": list(platforms),
        "readiness_key": readiness_key,
    }

    def decorator(fn: Callable[..., Any]) -> Callable[..., Any]:
        @functools.wraps(fn)
        def wrapper(self: unittest.TestCase, *args: Any, **kwargs: Any) -> Any:
            ctx = get_runtime_context()
            entry = get_readiness_entry(readiness_key, ctx["manifest"])
            status = str(entry.get("status", "READY")).upper()
            notes = str(entry.get("notes", ""))

            if status != "READY":
                if ctx["strict_ready"]:
                    self.fail(f"STRICT_READY: {case_id} blocked by {readiness_key}={status} ({notes})")
                raise PendingFeatureSkip(f"[PENDING:{status}] {readiness_key}: {notes}")

            target_plat = ctx["target_platform"]
            if not _platform_matches(platforms, target_plat):
                raise unittest.SkipTest(
                    f"[PLATFORM_SKIP] {case_id} targets {platforms}, current filter='{target_plat}'"
                )

            return fn(self, *args, **kwargs)

        wrapper.e2e_meta = meta  # type: ignore[attr-defined]
        return wrapper

    return decorator


def get_case_meta(test_obj: unittest.TestCase) -> dict[str, Any]:
    method_name = getattr(test_obj, "_testMethodName", "")
    method = getattr(test_obj, method_name, None) if method_name else None
    meta = getattr(method, "e2e_meta", None)
    if isinstance(meta, dict):
        return dict(meta)
    return {
        "case_id": test_obj.id(),
        "tier": 0,
        "group": "UNASSIGNED",
        "req_ids": [],
        "apps": [],
        "platforms": ["both"],
        "readiness_key": "unlisted",
    }


def iter_suite_tests(suite: unittest.TestSuite) -> list[unittest.TestCase]:
    out: list[unittest.TestCase] = []
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            out.extend(iter_suite_tests(item))
        else:
            out.append(item)
    return out


class E2ETestResult(unittest.TestResult):
    """Structured unittest TestResult recording PASS/FAIL/PENDING/SKIP and timings."""

    def __init__(self, stream: Any = None, verbosity: int = 1) -> None:
        super().__init__()
        self.stream = stream
        self.verbosity = verbosity
        self.records: list[dict[str, Any]] = []
        self._start_times: dict[str, float] = {}

    def startTest(self, test: unittest.TestCase) -> None:
        super().startTest(test)
        self._start_times[test.id()] = time.perf_counter()

    def _elapsed_ms(self, test: unittest.TestCase) -> float:
        t0 = self._start_times.pop(test.id(), None)
        if t0 is None:
            return 0.0
        return round((time.perf_counter() - t0) * 1000.0, 2)

    def _record(
        self,
        test: unittest.TestCase,
        status: str,
        message: str = "",
    ) -> None:
        meta = get_case_meta(test)
        rec = {
            "case_id": meta["case_id"],
            "test_id": test.id(),
            "tier": meta["tier"],
            "group": meta["group"],
            "req_ids": meta["req_ids"],
            "apps": meta["apps"],
            "platforms": meta["platforms"],
            "readiness_key": meta["readiness_key"],
            "status": status,
            "duration_ms": self._elapsed_ms(test),
            "message": message,
        }
        self.records.append(rec)
        if self.stream is not None and self.verbosity >= 1:
            if self.verbosity >= 2:
                self.stream.write(f"[{status}] {meta['case_id']} ({rec['duration_ms']} ms)")
                if message and status != "PASS":
                    first_line = message.splitlines()[0]
                    self.stream.write(f" -- {first_line}")
                self.stream.write("\n")
            else:
                ch = {"PASS": ".", "FAIL": "F", "PENDING": "P", "SKIP": "s"}.get(status, "?")
                self.stream.write(ch)
            self.stream.flush()

    def addSuccess(self, test: unittest.TestCase) -> None:
        super().addSuccess(test)
        self._record(test, "PASS", "")

    def addFailure(self, test: unittest.TestCase, err: Any) -> None:
        super().addFailure(test, err)
        msg = self._exc_info_to_string(err, test)
        self._record(test, "FAIL", msg)

    def addError(self, test: unittest.TestCase, err: Any) -> None:
        super().addError(test, err)
        msg = self._exc_info_to_string(err, test)
        self._record(test, "FAIL", msg)

    def addSkip(self, test: unittest.TestCase, reason: str) -> None:
        super().addSkip(test, reason)
        if reason.startswith("[PENDING:"):
            self._record(test, "PENDING", reason)
        else:
            self._record(test, "SKIP", reason)

    def addExpectedFailure(self, test: unittest.TestCase, err: Any) -> None:
        super().addExpectedFailure(test, err)
        self._record(test, "PASS", "expected_failure")

    def addUnexpectedSuccess(self, test: unittest.TestCase) -> None:
        super().addUnexpectedSuccess(test)
        self._record(test, "FAIL", "unexpected_success")
