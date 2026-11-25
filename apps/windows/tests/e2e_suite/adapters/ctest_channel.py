"""Channel B: CTest & DocumentCoreTest / document_core_linux_test keepDir harvester."""
from __future__ import annotations

import atexit
import os
import shutil
import tempfile
from pathlib import Path
from typing import Any

from .cli_runner import RunResult, run_win, run_wsl
from .config import DEFAULT_TIMEOUTS, REPO_ROOT, resolve_paths, win_to_wsl_path


_SESSION_CACHE: dict[tuple[str, str], dict[str, Any]] = {}
_TEMP_KEEP_DIRS: list[Path] = []


def _cleanup_temp_keep_dirs() -> None:
    if os.environ.get("E2E_KEEP_ARTIFACTS", "").strip() == "1":
        return
    for d in _TEMP_KEEP_DIRS:
        shutil.rmtree(d, ignore_errors=True)
    _TEMP_KEEP_DIRS.clear()


atexit.register(_cleanup_temp_keep_dirs)


def create_session_keepdir(prefix: str = "e2e_doccore_") -> Path:
    """Create a temp keepDir automatically cleaned at process exit unless E2E_KEEP_ARTIFACTS=1."""
    d = Path(tempfile.mkdtemp(prefix=prefix))
    _TEMP_KEEP_DIRS.append(d)
    return d


def index_artifacts(keep_dir: Path) -> dict[str, list[Path]]:
    """Index all files inside `keep_dir` by lowercase extension (e.g. '.pdf', '.cudoc')."""
    indexed: dict[str, list[Path]] = {}
    if not keep_dir.exists():
        return indexed
    for p in sorted(keep_dir.rglob("*")):
        if p.is_file():
            ext = p.suffix.lower() or "<noext>"
            indexed.setdefault(ext, []).append(p)
    return indexed


def run_doc_core_keepdir(
    platform: str,
    keep_dir: Path | None = None,
    timeout: int | None = None,
    force_rerun: bool = False,
) -> dict[str, Any]:
    """Run DocumentCoreTest.exe (win) or document_core_linux_test (linux) with manualsDir & keepDir.

    Caches per (platform, resolved_keep_dir) in-process so multiple tests can inspect
    emitted .cudoc/.pdf/.docx/.pptx/.svg/.dxf/.kicad_sch/.wav/.html artifacts without
    re-running the binary redundantly. If a cached run timed out under a smaller
    `timeout`, a subsequent call with a strictly larger `timeout` re-executes rather
    than returning the stale timeout failure.
    """
    plat = platform.lower().strip()
    if plat not in ("win", "linux"):
        raise ValueError(f"Unsupported platform '{platform}'; expected 'win' or 'linux'.")

    target_keep = Path(keep_dir) if keep_dir is not None else create_session_keepdir(f"e2e_keep_{plat}_")
    target_keep.mkdir(parents=True, exist_ok=True)
    cache_key = (plat, str(target_keep.resolve()))

    eff_timeout = int(timeout) if timeout is not None else DEFAULT_TIMEOUTS["ctest_keepdir"]
    if eff_timeout <= 0:
        raise ValueError(f"timeout must be a positive integer, got {eff_timeout}")

    if not force_rerun and cache_key in _SESSION_CACHE:
        cached = _SESSION_CACHE[cache_key]
        if not cached.get("timed_out", False) or int(cached.get("timeout_s", 0)) >= eff_timeout:
            return cached

    paths = resolve_paths(REPO_ROOT)

    if plat == "win":
        exe = paths["win_document_core_test"]
        if not exe.exists():
            res = RunResult(
                returncode=-1,
                stdout="",
                stderr=f"Missing Windows binary: {exe}",
                duration_ms=0.0,
                timed_out=False,
            )
        else:
            res = run_win(
                [str(exe), str(paths["manuales_dir"]), str(target_keep)],
                timeout=eff_timeout,
                cwd=paths["repo_root"],
            )
    else:
        exe = paths["linux_document_core_test"]
        if not exe.exists():
            res = RunResult(
                returncode=-1,
                stdout="",
                stderr=f"Missing Linux binary: {exe}",
                duration_ms=0.0,
                timed_out=False,
            )
        else:
            res = run_wsl(
                [
                    win_to_wsl_path(exe),
                    win_to_wsl_path(paths["manuales_dir"]),
                    win_to_wsl_path(target_keep),
                ],
                timeout=eff_timeout,
                cwd_linux=win_to_wsl_path(paths["repo_root"]),
            )

    payload = {
        "platform": plat,
        "returncode": res.returncode,
        "duration_ms": res.duration_ms,
        "timed_out": res.timed_out,
        "timeout_s": eff_timeout,
        "stdout": res.stdout,
        "stderr": res.stderr,
        "keep_dir": target_keep,
        "artifacts": index_artifacts(target_keep),
    }
    _SESSION_CACHE[cache_key] = payload
    return payload


def run_ctest_target(
    platform: str,
    target_regex: str | None = None,
    timeout: int = 120,
) -> RunResult:
    """Run existing CTest targets in `build-verify` (win) or `linux/build-release` (linux).

    Invokes `ctest` with `--no-tests=error` on both platforms and explicitly rejects
    `"No tests were found"` output (forcing `returncode = -2` if `ctest` ever exits `0`)
    so zero-match regexes can never produce silent false passes.

    `timeout` is the wall-clock subprocess timeout in seconds enforced by `run_win` /
    `run_wsl` (returns `RunResult(returncode=-1, timed_out=True, ...)` on expiry).
    """
    plat = platform.lower().strip()
    if plat not in ("win", "linux"):
        raise ValueError(f"Unsupported platform '{platform}'")
    eff_timeout = int(timeout) if timeout is not None else 120
    if eff_timeout <= 0:
        raise ValueError(f"timeout must be a positive integer, got {eff_timeout}")

    paths = resolve_paths(REPO_ROOT)

    if plat == "win":
        ctest_exe = "D:/DevTools/CMake/bin/ctest.exe"
        if not Path(ctest_exe).exists():
            ctest_exe = "ctest"
        cmd = [
            ctest_exe,
            "--test-dir",
            str(paths["repo_root"] / "build-verify"),
            "-C",
            "Release",
            "--output-on-failure",
            "--no-tests=error",
        ]
        if target_regex:
            cmd.extend(["-R", target_regex])
        res = run_win(cmd, timeout=eff_timeout, cwd=paths["repo_root"])
    else:
        build_dir_wsl = win_to_wsl_path(paths["repo_root"] / "linux" / "build-release")
        cmd = [
            "ctest",
            "--test-dir",
            build_dir_wsl,
            "--output-on-failure",
            "--no-tests=error",
        ]
        if target_regex:
            cmd.extend(["-R", target_regex])
        res = run_wsl(cmd, timeout=eff_timeout, cwd_linux=win_to_wsl_path(paths["repo_root"]))

    combined_out = f"{res.stdout}\n{res.stderr}".lower()
    if "no tests were found" in combined_out and res.returncode == 0:
        err_msg = "CTest matched zero tests ('No tests were found')."
        new_stderr = f"{res.stderr.rstrip()}\n{err_msg}".lstrip() if res.stderr else err_msg
        return RunResult(
            returncode=-2,
            stdout=res.stdout,
            stderr=new_stderr,
            duration_ms=res.duration_ms,
            timed_out=res.timed_out,
        )
    return res
