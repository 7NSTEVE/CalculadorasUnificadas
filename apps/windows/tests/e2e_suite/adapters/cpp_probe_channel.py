"""Centralized out-of-tree C++17 black-box probe builder & runner with ABI freshness guard."""
from __future__ import annotations

import atexit
import datetime
import os
import shutil
import tempfile
from pathlib import Path

from .cli_runner import RunResult, run_win, run_wsl
from .config import DEFAULT_TIMEOUTS, REPO_ROOT, resolve_paths, win_to_wsl_path


_PROBE_CACHE: dict[str, Path] = {}
_PROBE_BUILD_ERROR: dict[str, str] = {}
_PROBE_TMP_DIRS: list[Path] = []
_HEADER_SUBDIRS = ("core", "math", "circuit", "bridge", "pdf")


def _cleanup_probe_dirs() -> None:
    for d in _PROBE_TMP_DIRS:
        shutil.rmtree(d, ignore_errors=True)
    _PROBE_TMP_DIRS.clear()


atexit.register(_cleanup_probe_dirs)


def _fmt_utc(ts: float) -> str:
    return datetime.datetime.fromtimestamp(ts, tz=datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def check_document_core_freshness(plat: str, paths: dict[str, Path] | None = None) -> None:
    """Fail loudly if doc_studio headers are newer than the compiled DocumentCore archive."""
    resolved = paths or resolve_paths(REPO_ROOT)
    lib_key = "win_document_core_lib" if plat == "win" else "linux_document_core_lib"
    lib_path = resolved[lib_key]
    if not lib_path.exists():
        msg = (
            f"EXTERNAL_M1_ABI_SKEW ({plat}): missing {lib_path}; "
            f"M1 must build DocumentCore before cpp_probe can link/run."
        )
        _PROBE_BUILD_ERROR[plat] = msg
        raise RuntimeError(msg)

    lib_mtime = lib_path.stat().st_mtime
    doc_studio_root = resolved["repo_root"] / "doc_studio"
    newest_hdr: Path | None = None
    max_hdr_mtime = 0.0
    for sub in _HEADER_SUBDIRS:
        sub_dir = doc_studio_root / sub
        if not sub_dir.exists():
            continue
        for ext in ("*.h", "*.hpp"):
            for hdr in sub_dir.glob(ext):
                try:
                    mt = hdr.stat().st_mtime
                except OSError:
                    continue
                if mt > max_hdr_mtime:
                    max_hdr_mtime = mt
                    newest_hdr = hdr

    if newest_hdr is not None and max_hdr_mtime > lib_mtime + 1e-6:
        try:
            rel_hdr = newest_hdr.relative_to(resolved["repo_root"])
        except ValueError:
            rel_hdr = newest_hdr
        msg = (
            f"EXTERNAL_M1_ABI_SKEW ({plat}): doc_studio header {rel_hdr} "
            f"({_fmt_utc(max_hdr_mtime)}) is newer than {lib_path.name} "
            f"({_fmt_utc(lib_mtime)}); M1 must rebuild DocumentCore before cpp_probe can link/run."
        )
        _PROBE_BUILD_ERROR[plat] = msg
        raise RuntimeError(msg)


def clear_cpp_probe_caches() -> None:
    """Clear in-memory probe binary and failure caches (primarily for isolated self-tests)."""
    _PROBE_CACHE.clear()
    _PROBE_BUILD_ERROR.clear()


def ensure_cpp_probe(plat: str) -> Path:
    """Ensure out-of-tree `e2e_cpp_probe` is built for `plat` ('win' or 'linux').

    Enforces header-vs-library freshness BEFORE configuring/linking/running and caches
    both successful binaries and build/skew errors per platform.
    """
    plat_norm = plat.lower().strip()
    if plat_norm not in ("win", "linux"):
        raise ValueError(f"Unsupported cpp_probe platform: {plat!r}")

    if plat_norm in _PROBE_BUILD_ERROR:
        raise RuntimeError(_PROBE_BUILD_ERROR[plat_norm])

    paths = resolve_paths(REPO_ROOT)
    check_document_core_freshness(plat_norm, paths)

    if plat_norm in _PROBE_CACHE and _PROBE_CACHE[plat_norm].exists():
        return _PROBE_CACHE[plat_norm]

    env_probe = os.environ.get(f"E2E_CPP_PROBE_{plat_norm.upper()}", "").strip()
    if env_probe and Path(env_probe).exists():
        _PROBE_CACHE[plat_norm] = Path(env_probe)
        return _PROBE_CACHE[plat_norm]

    src_dir = paths["cpp_probe_dir"]
    build_dir = Path(tempfile.mkdtemp(prefix=f"e2e_probe_{plat_norm}_"))
    _PROBE_TMP_DIRS.append(build_dir)
    build_timeout = int(DEFAULT_TIMEOUTS.get("cpp_build", 120))

    if plat_norm == "win":
        cmake = "D:/DevTools/CMake/bin/cmake.exe"
        if not Path(cmake).exists():
            cmake = "cmake"
        cfg = run_win(
            [
                cmake,
                "-S",
                str(src_dir),
                "-B",
                str(build_dir),
                "-G",
                "Visual Studio 17 2022",
                "-A",
                "x64",
                "-DCMAKE_TOOLCHAIN_FILE=D:/DevTools/vcpkg/scripts/buildsystems/vcpkg.cmake",
            ],
            timeout=build_timeout,
            cwd=REPO_ROOT,
        )
        if cfg.returncode != 0:
            err = f"cpp_probe win configure failed (rc={cfg.returncode}): {cfg.stderr}\n{cfg.stdout}"
            _PROBE_BUILD_ERROR[plat_norm] = err
            raise RuntimeError(err)

        bld = run_win(
            [cmake, "--build", str(build_dir), "--config", "Release"],
            timeout=build_timeout,
            cwd=REPO_ROOT,
        )
        if bld.returncode != 0:
            err = f"cpp_probe win build failed (rc={bld.returncode}): {bld.stderr}\n{bld.stdout}"
            _PROBE_BUILD_ERROR[plat_norm] = err
            raise RuntimeError(err)
        exe = build_dir / "Release" / "e2e_cpp_probe.exe"
    else:
        src_wsl = win_to_wsl_path(src_dir)
        bld_wsl = win_to_wsl_path(build_dir)
        repo_wsl = win_to_wsl_path(REPO_ROOT)
        cfg = run_wsl(
            [
                "cmake",
                "-S",
                src_wsl,
                "-B",
                bld_wsl,
                "-G",
                "Ninja",
                "-DCMAKE_BUILD_TYPE=Release",
            ],
            timeout=max(build_timeout, 180),
            cwd_linux=repo_wsl,
            clean_linux_path=True,
        )
        if cfg.returncode != 0:
            err = f"cpp_probe linux configure failed (rc={cfg.returncode}): {cfg.stderr}\n{cfg.stdout}"
            _PROBE_BUILD_ERROR[plat_norm] = err
            raise RuntimeError(err)

        bld = run_wsl(
            ["cmake", "--build", bld_wsl],
            timeout=max(build_timeout, 180),
            cwd_linux=repo_wsl,
            clean_linux_path=True,
        )
        if bld.returncode != 0:
            err = f"cpp_probe linux build failed (rc={bld.returncode}): {bld.stderr}\n{bld.stdout}"
            _PROBE_BUILD_ERROR[plat_norm] = err
            raise RuntimeError(err)
        exe = build_dir / "e2e_cpp_probe"

    if not exe.exists():
        err = f"Missing built cpp_probe executable: {exe}"
        _PROBE_BUILD_ERROR[plat_norm] = err
        raise RuntimeError(err)

    _PROBE_CACHE[plat_norm] = exe
    os.environ[f"E2E_CPP_PROBE_{plat_norm.upper()}"] = str(exe)
    return exe


def run_cpp_probe(plat: str, args: list[str], timeout: int = 45) -> RunResult:
    """Build (if needed) and invoke `e2e_cpp_probe` on `plat` with `args`."""
    plat_norm = plat.lower().strip()
    exe = ensure_cpp_probe(plat_norm)
    if plat_norm == "win":
        return run_win([str(exe), *args], timeout=timeout, cwd=REPO_ROOT)
    return run_wsl([win_to_wsl_path(exe), *args], timeout=timeout, cwd_linux=win_to_wsl_path(REPO_ROOT))
