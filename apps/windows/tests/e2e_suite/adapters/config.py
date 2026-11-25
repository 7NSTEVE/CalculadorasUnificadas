"""Dual-platform path resolution, timeouts, and capability probing."""
from __future__ import annotations

import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


REPO_ROOT: Path = Path(__file__).resolve().parents[3]

DEFAULT_TIMEOUTS: dict[str, int] = {
    "cli_smoke": 30,
    "ctest_keepdir": 120,
    "gui_probe": 15,
    "cpp_build": 120,
    "network": 15,
}


def is_linux_host() -> bool:
    return sys.platform.startswith("linux")


def is_windows_host() -> bool:
    return os.name == "nt"


def win_to_wsl_path(path: str | Path) -> str:
    """Convert a Windows path (e.g. D:\\foo\\bar) to a WSL /mnt/<drive>/... path."""
    raw = str(path)
    if raw.startswith("/"):
        return raw.replace("\\", "/")
    m = re.match(r"^([A-Za-z]):[\\/](.*)$", raw)
    if m:
        drive = m.group(1).lower()
        rest = m.group(2).replace("\\", "/")
        return f"/mnt/{drive}/{rest}"
    return raw.replace("\\", "/")


def wsl_to_win_path(wsl_path: str) -> Path:
    """Convert a /mnt/<drive>/... path back to a Windows Path."""
    m = re.match(r"^/mnt/([a-zA-Z])/(.*)$", wsl_path)
    if m:
        drive = m.group(1).upper()
        rest = m.group(2).replace("/", "\\")
        return Path(f"{drive}:\\{rest}")
    return Path(wsl_path)


def _first_existing(candidates: list[Path]) -> Path:
    for cand in candidates:
        if cand.exists():
            return cand
    return candidates[0]


def resolve_paths(repo_root: Path | None = None) -> dict[str, Path]:
    """Resolve authoritative binaries and test directories for Windows and WSL Linux.

    Strictly prefers `build-studio-x64/Release/DocumentStudio.exe` over any stale
    copy under `build-verify/`.
    """
    root = Path(repo_root).resolve() if repo_root else REPO_ROOT

    win_calc = root / "build-verify" / "Release" / "CalculadoraUnificada.exe"
    if not win_calc.exists() and (root / "CalculadoraUnificada.exe").exists():
        win_calc = root / "CalculadoraUnificada.exe"

    acad = root / "academic"
    win_tec_candidates = [
        root / "build-studio-x64" / "academic_build" / "tec_digital" / "Release" / "tec_digital_cli.exe",
    ] + [
        acad / f"build-{m}-{arch}" / sub / "tec_digital_cli.exe"
        for m in ("m35", "m34", "m33", "m32", "m31")
        for arch in ("x64", "win32")
        for sub in ("tec_digital/Release", "Release", "")
    ]
    linux_tec_candidates = [
        acad / f"build-{m}-linux" / sub / "tec_digital_cli"
        for m in ("m35", "m34", "m33", "m32", "m31")
        for sub in ("tec_digital", "")
    ]
    win_horarios_candidates = [
        root / "build-studio-x64" / "academic_build" / "horarios_tec" / "Release" / "horarios_tec_cli.exe",
    ] + [
        acad / f"build-{m}-{arch}" / sub / "horarios_tec_cli.exe"
        for m in ("m35", "m34", "m33", "m32", "m31")
        for arch in ("x64", "win32")
        for sub in ("horarios_tec/Release", "Release")
    ] + [
        acad / "horarios_tec" / f"build-{arch}" / "Release" / "horarios_tec_cli.exe"
        for arch in ("x64", "win32")
    ]
    linux_horarios_candidates = [
        acad / f"build-{m}-linux" / "horarios_tec" / "horarios_tec_cli"
        for m in ("m35", "m34", "m33", "m32", "m31")
    ] + [
        acad / "horarios_tec" / "build-linux" / "horarios_tec_cli",
    ]

    return {
        "repo_root": root,
        "manuales_dir": root / "manuales",
        "suite_root": root / "tests" / "e2e_suite",
        "readiness_manifest": root / "tests" / "e2e_suite" / "readiness_manifest.json",
        "cpp_probe_dir": root / "tests" / "e2e_suite" / "cpp_probe",
        "ocr_ps1": root / "tools" / "ocr_png.ps1",
        # Windows authoritative binaries (never stale build-verify/Release/DocumentStudio.exe)
        "win_cuadernu": root / "build-studio-x64" / "Release" / "CuadernU.exe",
        "win_document_studio": root / "build-studio-x64" / "Release" / "DocumentStudio.exe",
        "win_document_core_test": root / "build-studio-x64" / "Release" / "DocumentCoreTest.exe",
        "win_document_core_lib": root / "build-studio-x64" / "Release" / "DocumentCore.lib",
        "win_calculadora": win_calc,
        "win_inspect_gui": root / "build-verify" / "Release" / "InspectGui.exe",
        "win_window_command": root / "build-verify" / "Release" / "WindowCommand.exe",
        "win_cg50_contract_test": root / "build-verify" / "Release" / "Cg50BackendContractTest.exe",
        "win_ui_core_test": root / "build-verify" / "Release" / "UiCoreModulesTest.exe",
        "win_pdf_stress_test": root / "build-verify" / "Release" / "PdfViewerStressTest.exe",
        # Linux authoritative binaries / wrappers
        "linux_run_document_studio": root / "linux" / "dist-linux" / "run_document_studio.sh",
        "linux_run_calculadora": root / "linux" / "dist-linux" / "run_calculadora.sh",
        "linux_document_studio_bin": root / "linux" / "dist-linux" / "bin" / "document-studio",
        "linux_calculadora_bin": root / "linux" / "dist-linux" / "bin" / "calculadora-unificada",
        "linux_document_core_test": root / "linux" / "build-release" / "doc_studio_linux" / "document_core_linux_test",
        "linux_document_core_lib": root / "linux" / "build-release" / "doc_studio_linux" / "libDocumentCoreLinux.a",
        "linux_engines_test": root / "linux" / "build-release" / "engines" / "test_linux_engines",
        "linux_engines_lib": root / "linux" / "build-release" / "engines" / "libCasioLinuxEngines.a",
        # Optional M3 academic module (strict CLI discovery; never test_tec_foundation)
        "academic_dir": acad,
        "win_tec_digital_cli": _first_existing(win_tec_candidates),
        "linux_tec_digital_cli": _first_existing(linux_tec_candidates),
        "win_horarios_tec_cli": _first_existing(win_horarios_candidates),
        "linux_horarios_tec_cli": _first_existing(linux_horarios_candidates),
    }


def _probe_cmd(cmd: list[str], timeout: int = 8) -> bool:
    try:
        res = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
        return res.returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def detect_capabilities(repo_root: Path | None = None, probe_wsl: bool = True) -> dict[str, Any]:
    """Probe host OS, binaries, and external test tools (cmake, ctest, xvfb-run, tesseract)."""
    paths = resolve_paths(repo_root)
    host_os = "linux" if is_linux_host() else ("windows" if is_windows_host() else sys.platform)

    caps: dict[str, Any] = {
        "host_os": host_os,
        "python_version": platform.python_version(),
        "win_binaries": {
            "DocumentStudio": paths["win_document_studio"].exists(),
            "DocumentCoreTest": paths["win_document_core_test"].exists(),
            "CalculadoraUnificada": paths["win_calculadora"].exists(),
            "InspectGui": paths["win_inspect_gui"].exists(),
        },
        "linux_binaries": {
            "run_document_studio": paths["linux_run_document_studio"].exists(),
            "run_calculadora": paths["linux_run_calculadora"].exists(),
            "document_studio_bin": paths["linux_document_studio_bin"].exists(),
            "calculadora_bin": paths["linux_calculadora_bin"].exists(),
            "document_core_linux_test": paths["linux_document_core_test"].exists(),
            "test_linux_engines": paths["linux_engines_test"].exists(),
        },
        "academic_present": paths["academic_dir"].exists(),
        "tools": {
            "cmake": shutil.which("cmake") is not None or Path("D:/DevTools/CMake/bin/cmake.exe").exists(),
            "ctest": shutil.which("ctest") is not None or Path("D:/DevTools/CMake/bin/ctest.exe").exists(),
            "powershell": shutil.which("powershell") is not None or shutil.which("pwsh") is not None,
            "wsl": False,
            "wsl_python3": False,
            "xvfb_run": False,
            "tesseract": False,
            "xwininfo": False,
            "openssl": shutil.which("openssl") is not None,
        },
    }

    if is_linux_host():
        caps["tools"]["xvfb_run"] = shutil.which("xvfb-run") is not None
        caps["tools"]["tesseract"] = shutil.which("tesseract") is not None
        caps["tools"]["xwininfo"] = shutil.which("xwininfo") is not None
        caps["tools"]["openssl"] = shutil.which("openssl") is not None
        caps["tools"]["wsl_python3"] = True
    elif is_windows_host() and probe_wsl and shutil.which("wsl") is not None:
        caps["tools"]["wsl"] = True
        caps["tools"]["wsl_python3"] = _probe_cmd(["wsl", "python3", "--version"], timeout=10)
        caps["tools"]["xvfb_run"] = _probe_cmd(["wsl", "which", "xvfb-run"], timeout=10)
        caps["tools"]["tesseract"] = _probe_cmd(["wsl", "which", "tesseract"], timeout=10)
        caps["tools"]["xwininfo"] = _probe_cmd(["wsl", "which", "xwininfo"], timeout=10)
        if not caps["tools"]["openssl"]:
            caps["tools"]["openssl"] = _probe_cmd(["wsl", "which", "openssl"], timeout=10)

    return caps
