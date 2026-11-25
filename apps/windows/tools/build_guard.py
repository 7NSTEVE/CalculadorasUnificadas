#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_guard.py — Fail-Closed Compilation & Artifact Integrity Engine for Calculadora Unificada.
Milestone 1 (Requirement R1: Control estricto de compilación y corte ante fallos).

Authoritative specification:
- ORIGINAL_REQUEST.md (Requirement R1)
- PROJECT.md (M1 Scope, Features 1 & 2, Interface Contracts M1 <-> M2)
- Explorer reports M1-1, M1-2, M1-3
"""

import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import time
import uuid
from dataclasses import dataclass, asdict, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional, List, Dict, Any, Tuple, Union

# Default directories and binary names
TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_PROJECT_ROOT = os.path.dirname(TOOLS_DIR)
DEFAULT_BUILD_DIR = os.path.join(DEFAULT_PROJECT_ROOT, "build-win32")
DEFAULT_TARGET = "CalculadoraUnificada"
DEFAULT_TARGET_EXE = "CalculadoraUnificada.exe"
DEFAULT_COMPILER_NAME = "MSVC 14.44.35207"

# PE Machine Architecture constants
PE_MACHINE_I386 = 0x014c
PE_MACHINE_AMD64 = 0x8664

# Binary search candidates
CMAKE_BIN_CANDIDATES = [
    r"D:\DevTools\CMake\bin\cmake.exe",
    shutil.which("cmake") or "cmake"
]

GIT_BIN_CANDIDATES = [
    r"D:\DevTools\Tools\Git\cmd\git.exe",
    shutil.which("git") or "git"
]


# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class BuildResult:
    """Represents the structured result of a build execution or verification."""
    success: bool
    exe_path: str
    manifest_path: Optional[str] = None
    sha256: Optional[str] = None
    timestamp: float = 0.0
    iso_timestamp: str = ""
    duration_seconds: float = 0.0
    exit_code: int = 0
    error_message: Optional[str] = None
    diagnostics: Optional[List[str]] = None
    stage: str = "complete"
    target: str = DEFAULT_TARGET
    config: str = "Release"
    size_bytes: int = 0
    pe_machine: Optional[str] = None
    compiler_version: Optional[str] = None
    git_commit: Optional[str] = None
    error: Optional[str] = None
    stdout_tail: Optional[str] = None
    stderr_tail: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        d = asdict(self)
        d["status"] = "CONFIRMED" if self.success else "FAILED"
        return d


@dataclass
class VerificationResult:
    """Represents the outcome of manifest and binary integrity verification."""
    is_valid: bool
    status: str  # "CONFIRMED", "FAILED", "EXPIRED", "CORRUPTED", "NOT_FOUND"
    reason: str
    sha256: str = ""
    manifest_sha256: str = ""
    binary_path: str = ""
    manifest_path: str = ""
    details: Dict[str, Any] = field(default_factory=dict)


# =============================================================================
# Exception Hierarchy
# =============================================================================

class BuildGateError(Exception):
    """Base exception for all build guard failures."""
    def __init__(self, message: str, exit_code: int = 2, stage: str = "unknown",
                 diagnostics: Optional[List[str]] = None):
        super().__init__(message)
        self.message = message
        self.exit_code = exit_code
        self.stage = stage
        self.diagnostics = diagnostics or []


class ConfigError(BuildGateError):
    """Raised when CMake configuration phase fails (exit code 1)."""
    def __init__(self, message: str, diagnostics: Optional[List[str]] = None):
        super().__init__(message, exit_code=1, stage="cmake_configure", diagnostics=diagnostics)


class CompilationError(BuildGateError):
    """
    Raised when compilation / linking fails or build guard aborts (exit code 2).
    Supports multiple calling conventions for interoperability.
    """
    def __init__(self, *args, **kwargs):
        # Support CompilationError(exit_code: int, log_output: str, stage: str, diagnostics: list)
        # and CompilationError(message: str, diagnostics: list, stdout_tail: str, stderr_tail: str)
        if args and isinstance(args[0], int):
            exit_code = args[0]
            log_output = args[1] if len(args) > 1 else ""
            stage = args[2] if len(args) > 2 else kwargs.get("stage", "compilation")
            diagnostics = args[3] if len(args) > 3 else kwargs.get("diagnostics", [])
            message = f"Compilation failed at stage '{stage}' (exit code {exit_code})"
            stdout_tail = kwargs.get("stdout_tail", "")
            stderr_tail = kwargs.get("stderr_tail", "")
        else:
            message = args[0] if args else kwargs.get("message", "Compilation failed")
            exit_code = kwargs.get("exit_code", 2)
            stage = kwargs.get("stage", "compilation")
            diagnostics = kwargs.get("diagnostics", [])
            log_output = kwargs.get("log_output", "")
            stdout_tail = kwargs.get("stdout_tail", "")
            stderr_tail = kwargs.get("stderr_tail", "")

        super().__init__(message, exit_code=exit_code, stage=stage, diagnostics=diagnostics)
        self.log_output = log_output
        self.stdout_tail = stdout_tail
        self.stderr_tail = stderr_tail


class ManifestVerificationError(BuildGateError):
    """Raised when artifact manifest verification fails (exit code 3)."""
    def __init__(self, message: str, diagnostics: Optional[List[str]] = None):
        super().__init__(message, exit_code=3, stage="manifest_verification", diagnostics=diagnostics)


class ProcessLockError(BuildGateError):
    """Raised when process locking prevents file removal/build (exit code 4)."""
    def __init__(self, message: str, pids: Optional[List[int]] = None):
        super().__init__(message, exit_code=4, stage="process_locking", diagnostics=[message])
        self.pids = pids or []


# =============================================================================
# Helper Resolvers & Utilities
# =============================================================================

def get_cmake_executable() -> str:
    """Finds available CMake binary in authorized paths or PATH."""
    for cand in CMAKE_BIN_CANDIDATES:
        if cand and os.path.exists(cand):
            return cand
    cmd = shutil.which("cmake")
    if cmd:
        return cmd
    return "cmake"


def get_git_executable() -> str:
    """Finds available Git binary in authorized paths or PATH."""
    for cand in GIT_BIN_CANDIDATES:
        if cand and os.path.exists(cand):
            return cand
    cmd = shutil.which("git")
    if cmd:
        return cmd
    return "git"


def extract_diagnostics(log_text: str) -> List[str]:
    """Extracts compiler / linker error and warning lines from log text."""
    diags = []
    markers = [
        ": error C", ": fatal error C",
        ": error LNK", ": fatal error LNK",
        "MSBUILD : error", "CMake Error at"
    ]
    for line in log_text.splitlines():
        line_s = line.strip()
        if any(m in line_s for m in markers):
            if line_s not in diags:
                diags.append(line_s)
    return diags


def compute_sha256(filepath: Union[str, Path], chunk_size: int = 65536) -> str:
    """Computes SHA-256 hash of a file reading in chunks, returns uppercase hex."""
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(chunk_size):
            h.update(chunk)
    return h.hexdigest().upper()


def verify_pe_header(exe_path: Union[str, Path]) -> Tuple[int, str]:
    """
    Parses PE header to verify architecture.
    Returns (machine_int, machine_name_str).
    IMAGE_FILE_MACHINE_I386 = 0x014c (32-bit x86, required for GCalAppIF).
    """
    with open(exe_path, "rb") as f:
        dos_hdr = f.read(64)
        if len(dos_hdr) < 64 or dos_hdr[:2] != b"MZ":
            raise ValueError(f"File '{exe_path}' is not a valid MZ executable.")
        pe_offset = struct.unpack("<I", dos_hdr[0x3C:0x40])[0]
        f.seek(pe_offset)
        sig = f.read(4)
        if sig != b"PE\x00\x00":
            raise ValueError(f"Invalid PE signature in '{exe_path}'.")
        machine = struct.unpack("<H", f.read(2))[0]

    if machine == PE_MACHINE_I386:
        name = "IMAGE_FILE_MACHINE_I386"
    elif machine == PE_MACHINE_AMD64:
        name = "IMAGE_FILE_MACHINE_AMD64"
    else:
        name = f"UNKNOWN_MACHINE_{hex(machine)}"
    return machine, name


def get_git_metadata(repo_dir: str) -> Dict[str, Any]:
    """Retrieves Git repository commit, branch, and dirty status."""
    git_bin = get_git_executable()
    meta = {
        "commit_hash": "UNKNOWN",
        "commit_short": "UNKNOWN",
        "branch": "UNKNOWN",
        "dirty": False,
        "commit_timestamp_utc": None
    }
    try:
        r_hash = subprocess.run([git_bin, "rev-parse", "HEAD"],
                                cwd=repo_dir, capture_output=True, text=True, timeout=5)
        if r_hash.returncode == 0:
            meta["commit_hash"] = r_hash.stdout.strip()
            meta["commit_short"] = meta["commit_hash"][:7]

        r_branch = subprocess.run([git_bin, "rev-parse", "--abbrev-ref", "HEAD"],
                                  cwd=repo_dir, capture_output=True, text=True, timeout=5)
        if r_branch.returncode == 0:
            meta["branch"] = r_branch.stdout.strip()

        r_status = subprocess.run([git_bin, "status", "--porcelain"],
                                  cwd=repo_dir, capture_output=True, text=True, timeout=5)
        if r_status.returncode == 0:
            meta["dirty"] = bool(r_status.stdout.strip())

        r_time = subprocess.run([git_bin, "log", "-1", "--format=%cI"],
                                cwd=repo_dir, capture_output=True, text=True, timeout=5)
        if r_time.returncode == 0:
            meta["commit_timestamp_utc"] = r_time.stdout.strip()
    except Exception:
        pass
    return meta


def write_error_artifacts(build_dir: str, stage: str, exit_code: int,
                          log_text: str, target: str) -> Tuple[str, str]:
    """Writes build_error.log and structured build_error.json in build_dir."""
    os.makedirs(build_dir, exist_ok=True)
    error_log = os.path.join(build_dir, "build_error.log")
    try:
        with open(error_log, "w", encoding="utf-8") as f:
            f.write(log_text)
    except Exception:
        pass

    diags = extract_diagnostics(log_text)
    error_json = os.path.join(build_dir, "build_error.json")
    data = {
        "status": "FAIL",
        "exit_code": exit_code,
        "stage": stage,
        "target": target,
        "timestamp": time.time(),
        "iso_timestamp": datetime.now(timezone.utc).isoformat(),
        "error_type": "ERR_BUILD_FAILED",
        "diagnostics": diags,
        "log_file": error_log
    }
    try:
        with open(error_json, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
    except Exception:
        pass
    return error_json, error_log


# =============================================================================
# Core Pipeline Functions
# =============================================================================

def kill_running_instances(image_name: str = "CalculadoraUnificada.exe",
                           timeout_sec: float = 3.0) -> int:
    """
    Cleanly and deterministically terminates any running instance of image_name
    to prevent file locks (Sharing Violation / WinError 32).
    Returns number of termination attempts made.
    """
    if not image_name.endswith(".exe"):
        image_name = f"{image_name}.exe"

    targets = [image_name]
    if image_name == "CalculadoraUnificada.exe":
        targets.extend(["GCalHostProbe.exe", "InspectGui.exe", "WindowCommand.exe"])

    attempts = 0
    for target in targets:
        try:
            res = subprocess.run(
                ["taskkill", "/F", "/IM", target, "/T"],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
                timeout=5
            )
            if res.returncode == 0:
                attempts += 1
        except Exception:
            pass

    # Wait briefly for OS handles to release
    time.sleep(0.3)
    return attempts


def invalidate_stale_artifacts(release_dir: str,
                               target_name: str = "CalculadoraUnificada") -> None:
    """
    Physically removes old target binary, manifests, and error logs before building.
    Ensures that if the subsequent build fails, no stale executable can be invoked.
    """
    target_exe = target_name if target_name.endswith(".exe") else f"{target_name}.exe"
    exe_path = os.path.join(release_dir, target_exe)
    build_dir = os.path.dirname(os.path.abspath(release_dir))
    project_root = os.path.dirname(build_dir)

    # Terminate running instances that could lock the file
    kill_running_instances(target_exe)

    # List of artifacts to invalidate
    paths_to_remove = [
        exe_path,
        os.path.join(release_dir, "build_manifest.json"),
        os.path.join(build_dir, "build_manifest.json"),
        os.path.join(project_root, "build_manifest.json"),
        os.path.join(build_dir, "build_error.json"),
        os.path.join(build_dir, "build_error.log")
    ]

    for p in paths_to_remove:
        if os.path.exists(p):
            try:
                os.remove(p)
            except OSError as e:
                # If deletion failed due to a lock, retry killing and deleting once more
                kill_running_instances(target_exe)
                time.sleep(0.5)
                try:
                    os.remove(p)
                except OSError as e2:
                    raise ProcessLockError(
                        f"Cannot remove stale artifact '{p}' due to file lock: {e2}"
                    )

    # Strictly assert that the binary is gone
    if os.path.exists(exe_path):
        raise ProcessLockError(
            f"Stale binary '{exe_path}' is still present after invalidation attempt."
        )


def run_static_contract_guard(source_dir: str) -> None:
    """
    Verifies source contracts before compilation:
    - CMakeLists.txt exists and is non-empty.
    - main.cpp does not contain unthrottled while(true) loops without Sleep.
    - Required ClassWiz and Natural ES ROMs exist in roms/.
    - Required runtime DLLs exist in runtime/.
    """
    cmake_lists = os.path.join(source_dir, "CMakeLists.txt")
    if not os.path.isfile(cmake_lists) or os.path.getsize(cmake_lists) == 0:
        raise CompilationError(
            2,
            f"Source contract violation: CMakeLists.txt missing or empty in '{source_dir}'",
            stage="contract_guard"
        )

    # Inspect main.cpp if present
    main_cpp = os.path.join(source_dir, "main.cpp")
    if os.path.isfile(main_cpp):
        try:
            with open(main_cpp, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
            if "while(true)" in content and "Sleep" not in content:
                raise CompilationError(
                    2,
                    "Source contract violation: Unthrottled while(true) loop without Sleep in main.cpp",
                    stage="contract_guard"
                )
        except CompilationError:
            raise
        except Exception as e:
            raise CompilationError(2, f"Failed to inspect main.cpp: {e}", stage="contract_guard")

    # Verify base ROMs
    roms_dir = os.path.join(source_dir, "roms")
    rom_cw = os.path.join(roms_dir, "fx991lax.rom")
    rom_es = os.path.join(roms_dir, "fx570vnplus.rom")
    has_cw = os.path.isfile(rom_cw) or os.path.isfile(rom_cw.replace(".rom", ".bin"))
    has_es = os.path.isfile(rom_es) or os.path.isfile(rom_es.replace(".rom", ".bin"))
    if not has_cw:
        raise CompilationError(
            2,
            f"Source contract violation: Required ClassWiz ROM missing in '{roms_dir}'",
            stage="contract_guard"
        )
    if not has_es:
        raise CompilationError(
            2,
            f"Source contract violation: Required Natural ES ROM missing in '{roms_dir}'",
            stage="contract_guard"
        )

    # Verify GCal runtime DLLs
    runtime_dir = os.path.join(source_dir, "runtime")
    gcal_dll = os.path.join(runtime_dir, "GCalAppIF.dll")
    if not os.path.isfile(gcal_dll):
        raise CompilationError(
            2,
            f"Source contract violation: Required runtime DLL missing: '{gcal_dll}'",
            stage="contract_guard"
        )


def run_cmake_build(project_dir: str, build_dir: str, config: str = "Release",
                    target: str = DEFAULT_TARGET, timeout_sec: int = 300,
                    clean_first: bool = False) -> Tuple[int, str, str]:
    """
    Invokes non-interactive CMake with stdin=subprocess.DEVNULL.
    Configures if necessary, then builds the specified target.
    Returns (exit_code, stdout, stderr).
    """
    cmake_bin = get_cmake_executable()
    os.makedirs(build_dir, exist_ok=True)

    # Configure step if CMakeCache.txt does not exist
    cache_file = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache_file):
        config_cmd = [
            cmake_bin,
            "-S", project_dir,
            "-B", build_dir,
            "-G", "Visual Studio 17 2022",
            "-A", "Win32"
        ]
        res_cfg = subprocess.run(
            config_cmd,
            stdin=subprocess.DEVNULL,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=120
        )
        if res_cfg.returncode != 0:
            full_log = (f"=== CMAKE CONFIGURE STDOUT ===\n{res_cfg.stdout}\n"
                        f"=== CMAKE CONFIGURE STDERR ===\n{res_cfg.stderr}")
            write_error_artifacts(build_dir, "cmake_configure", 1, full_log, target)
            raise ConfigError(
                f"CMake configuration failed with exit code {res_cfg.returncode}",
                diagnostics=extract_diagnostics(full_log)
            )

    # Build step
    build_cmd = [
        cmake_bin,
        "--build", build_dir,
        "--config", config,
        "--target", target
    ]
    if clean_first:
        build_cmd.append("--clean-first")

    res_bld = subprocess.run(
        build_cmd,
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout_sec
    )

    return res_bld.returncode, res_bld.stdout, res_bld.stderr


def generate_manifest(binary_path: str, manifest_path: str, build_duration: float,
                      compiler_info: Optional[Union[str, Dict[str, Any]]] = None,
                      target_name: Optional[str] = None,
                      config: str = "Release") -> Dict[str, Any]:
    """
    Computes SHA-256 (64KB chunks), verifies PE header machine 0x14c,
    records ISO 8601 UTC timestamp, epoch, git commit, and status CONFIRMED.
    Writes JSON to manifest_path and returns manifest dict.
    """
    if not os.path.isfile(binary_path):
        raise FileNotFoundError(f"Binary not found for manifest generation: {binary_path}")

    file_size = os.path.getsize(binary_path)
    if file_size == 0:
        raise ValueError(f"Binary file is empty (0 bytes): {binary_path}")

    file_mtime = os.path.getmtime(binary_path)
    sha256_hex = compute_sha256(binary_path)
    pe_machine, pe_machine_name = verify_pe_header(binary_path)

    if pe_machine != PE_MACHINE_I386:
        raise ValueError(f"PE machine type {hex(pe_machine)} is not Win32 i386 (0x014c).")

    # Compiler string handling
    if isinstance(compiler_info, dict):
        compiler_str = compiler_info.get("banner") or compiler_info.get("version") or DEFAULT_COMPILER_NAME
    elif isinstance(compiler_info, str) and compiler_info:
        compiler_str = compiler_info
    else:
        compiler_str = DEFAULT_COMPILER_NAME

    # Git metadata
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(manifest_path)))
    git_meta = get_git_metadata(project_root)

    t_now = datetime.now(timezone.utc)
    iso_ts = t_now.strftime("%Y-%m-%dT%H:%M:%SZ")
    target_str = target_name or os.path.basename(binary_path)
    if not target_str.endswith(".exe"):
        target_str = f"{target_str}.exe"

    manifest_data = {
        "schema_version": "1.0.0",
        "manifest_id": str(uuid.uuid4()),
        "status": "CONFIRMED",
        "compiler": compiler_str,
        "target": target_str,
        "binary_path": os.path.abspath(binary_path),
        "binary_sha256": sha256_hex,
        "timestamp": file_mtime,
        "iso_timestamp": iso_ts,
        "duration_seconds": round(build_duration, 3),
        "build_timestamp_utc": iso_ts,
        "build_epoch_utc": file_mtime,
        "compile_duration_seconds": round(build_duration, 3),
        "file_size": file_size,
        "pe_machine": hex(pe_machine),
        "pe_machine_name": pe_machine_name,
        "config": config,
        "arch": "Win32",
        "git": git_meta,
        "binary": {
            "name": os.path.basename(binary_path),
            "relative_path": os.path.relpath(binary_path, project_root).replace("\\", "/"),
            "absolute_path": os.path.abspath(binary_path),
            "size_bytes": file_size,
            "sha256": sha256_hex,
            "pe_machine": pe_machine_name,
            "last_modified_utc": datetime.fromtimestamp(file_mtime, timezone.utc).isoformat()
        }
    }

    # Ensure parent directory exists and write
    os.makedirs(os.path.dirname(os.path.abspath(manifest_path)), exist_ok=True)
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest_data, f, indent=2)

    # Mirror manifest to project root and build root for convenient access
    try:
        build_dir = os.path.dirname(os.path.dirname(os.path.abspath(manifest_path)))
        root_dir = os.path.dirname(build_dir)
        mirror_build = os.path.join(build_dir, "build_manifest.json")
        mirror_root = os.path.join(root_dir, "build_manifest.json")
        if mirror_build != os.path.abspath(manifest_path):
            with open(mirror_build, "w", encoding="utf-8") as f:
                json.dump(manifest_data, f, indent=2)
        if mirror_root != os.path.abspath(manifest_path):
            with open(mirror_root, "w", encoding="utf-8") as f:
                json.dump(manifest_data, f, indent=2)
    except Exception:
        pass

    return manifest_data


def verify_manifest(binary_path: Union[str, Path],
                    manifest_path: Optional[Union[str, Path]] = None,
                    max_age_seconds: Optional[float] = None,
                    expected_pe_machine: Optional[str] = "IMAGE_FILE_MACHINE_I386",
                    raise_on_failure: bool = False) -> VerificationResult:
    """
    Validates existence, schema, matching SHA-256, and freshness.
    Performs 8 gates of structural and cryptographic certification.
    """
    bin_p = os.path.abspath(str(binary_path))
    if manifest_path is None:
        manifest_path = os.path.join(os.path.dirname(bin_p), "build_manifest.json")
    man_p = os.path.abspath(str(manifest_path))

    details = {"binary_path": bin_p, "manifest_path": man_p, "checks": {}}

    # Gate 1: Manifest exists
    if not os.path.isfile(man_p):
        res = VerificationResult(False, "NOT_FOUND", f"Manifest not found: {man_p}", details=details)
        if raise_on_failure:
            raise ManifestVerificationError(res.reason)
        return res
    details["checks"]["manifest_exists"] = True

    # Gate 2: Binary exists
    if not os.path.isfile(bin_p):
        res = VerificationResult(False, "NOT_FOUND", f"Binary not found: {bin_p}", details=details)
        if raise_on_failure:
            raise ManifestVerificationError(res.reason)
        return res
    details["checks"]["binary_exists"] = True

    # Gate 3: Parse JSON
    try:
        with open(man_p, "r", encoding="utf-8") as f:
            manifest = json.load(f)
    except Exception as e:
        res = VerificationResult(False, "CORRUPTED", f"Manifest JSON parse error: {e}", details=details)
        if raise_on_failure:
            raise ManifestVerificationError(res.reason)
        return res
    details["manifest_data"] = manifest

    # Gate 4: Status CONFIRMED or PASS
    status = manifest.get("status")
    if status not in ("CONFIRMED", "PASS"):
        res = VerificationResult(False, status or "FAILED",
                                 f"Manifest status is not CONFIRMED/PASS (got: {status})",
                                 details=details)
        if raise_on_failure:
            raise ManifestVerificationError(res.reason)
        return res
    details["checks"]["status_confirmed"] = True

    # Gate 5: File size
    actual_size = os.path.getsize(bin_p)
    if actual_size == 0:
        res = VerificationResult(False, "CORRUPTED", "Binary is empty (0 bytes)", details=details)
        if raise_on_failure:
            raise ManifestVerificationError(res.reason)
        return res
    expected_size = manifest.get("file_size") or manifest.get("binary", {}).get("size_bytes")
    if expected_size is not None and actual_size != expected_size:
        res = VerificationResult(False, "CORRUPTED",
                                 f"Size mismatch: expected {expected_size}, actual {actual_size}",
                                 details=details)
        if raise_on_failure:
            raise ManifestVerificationError(res.reason)
        return res
    details["checks"]["size_match"] = True

    # Gate 6: SHA-256 hash match
    actual_sha = compute_sha256(bin_p)
    expected_sha = (manifest.get("binary_sha256") or
                    manifest.get("binary", {}).get("sha256") or "").upper()
    if not expected_sha or actual_sha != expected_sha:
        res = VerificationResult(False, "CORRUPTED",
                                 f"SHA-256 mismatch: expected {expected_sha}, actual {actual_sha}",
                                 sha256=actual_sha, manifest_sha256=expected_sha, details=details)
        if raise_on_failure:
            raise ManifestVerificationError(res.reason)
        return res
    details["checks"]["sha256_match"] = True

    # Gate 7: PE Machine header
    if expected_pe_machine:
        try:
            _, actual_machine_name = verify_pe_header(bin_p)
            if actual_machine_name != expected_pe_machine:
                res = VerificationResult(False, "CORRUPTED",
                                         f"PE machine mismatch: expected {expected_pe_machine}, got {actual_machine_name}",
                                         details=details)
                if raise_on_failure:
                    raise ManifestVerificationError(res.reason)
                return res
        except Exception as e:
            res = VerificationResult(False, "CORRUPTED", f"PE header parse failure: {e}", details=details)
            if raise_on_failure:
                raise ManifestVerificationError(res.reason)
            return res
        details["checks"]["pe_machine_match"] = True

    # Gate 8: Freshness / max age
    if max_age_seconds is not None:
        build_epoch = manifest.get("build_epoch_utc") or manifest.get("timestamp")
        if build_epoch is not None:
            age = time.time() - float(build_epoch)
            if age > max_age_seconds:
                res = VerificationResult(False, "EXPIRED",
                                         f"Manifest expired: age {age:.1f}s exceeds limit {max_age_seconds}s",
                                         details=details)
                if raise_on_failure:
                    raise ManifestVerificationError(res.reason)
                return res
        details["checks"]["age_valid"] = True

    return VerificationResult(
        is_valid=True,
        status="CONFIRMED",
        reason="CONFIRMED: Binary cryptographically and structurally certified by manifest",
        sha256=actual_sha,
        manifest_sha256=expected_sha,
        binary_path=bin_p,
        manifest_path=man_p,
        details=details
    )


def verify_or_build_core(rebuild: bool = True,
                         config: str = "Release",
                         target: str = DEFAULT_TARGET,
                         project_root: Optional[str] = None,
                         build_dir: Optional[str] = None,
                         timeout: int = 300,
                         clean_first: bool = False,
                         force_kill_locking: bool = True) -> BuildResult:
    """
    Primary API exported for downstream tools (`cg50_agent diff`, tests).
    - If rebuild=False and a valid manifest exists matching the binary SHA-256,
      returns existing certified BuildResult immediately.
    - If rebuild=True (or verification fails):
      1. Invalidates stale artifacts (kills locks, deletes old .exe and manifest).
      2. Runs static contract assertion guard.
      3. Invokes non-interactive CMake and MSVC.
      4. If build fails: writes error artifacts, deletes any partial binary, and
         immediately aborts raising CompilationError(exit_code=2).
      5. If build succeeds: certifies binary (size, PE machine 0x014c, SHA-256),
         generates build_manifest.json, and returns BuildResult.
    """
    if project_root is None:
        project_root = DEFAULT_PROJECT_ROOT
    if build_dir is None:
        build_dir = DEFAULT_BUILD_DIR

    target_exe = target if target.endswith(".exe") else f"{target}.exe"
    release_dir = os.path.join(build_dir, config)
    exe_path = os.path.join(release_dir, target_exe)
    manifest_path = os.path.join(release_dir, "build_manifest.json")

    # Fast-path: Reuse existing build if rebuild=False and manifest verifies
    if not rebuild and os.path.isfile(exe_path) and os.path.isfile(manifest_path):
        v = verify_manifest(exe_path, manifest_path)
        if v.is_valid:
            try:
                with open(manifest_path, "r", encoding="utf-8") as f:
                    mdata = json.load(f)
                return BuildResult(
                    success=True,
                    exe_path=exe_path,
                    manifest_path=manifest_path,
                    sha256=v.sha256,
                    timestamp=mdata.get("timestamp", os.path.getmtime(exe_path)),
                    iso_timestamp=mdata.get("iso_timestamp", ""),
                    duration_seconds=mdata.get("duration_seconds", 0.0),
                    exit_code=0,
                    stage="complete",
                    target=target,
                    config=config,
                    size_bytes=mdata.get("file_size", os.path.getsize(exe_path)),
                    pe_machine=mdata.get("pe_machine_name", "IMAGE_FILE_MACHINE_I386"),
                    compiler_version=mdata.get("compiler", DEFAULT_COMPILER_NAME),
                    git_commit=mdata.get("git", {}).get("commit_hash")
                )
            except Exception:
                pass  # Fall through to clean rebuild if parsing fails

    t_start = time.perf_counter()

    try:
        # Step 1: Invalidate stale artifacts
        if force_kill_locking:
            kill_running_instances(target_exe)
        invalidate_stale_artifacts(release_dir, target)

        # Step 2: Static Contract Guard
        run_static_contract_guard(project_root)

        # Step 3: CMake Configure and Build
        retcode, stdout, stderr = run_cmake_build(
            project_dir=project_root,
            build_dir=build_dir,
            config=config,
            target=target,
            timeout_sec=timeout,
            clean_first=clean_first
        )

        full_output = f"=== BUILD STDOUT ===\n{stdout}\n=== BUILD STDERR ===\n{stderr}"

        if retcode != 0:
            # Clean up any partial or corrupted binary
            if os.path.exists(exe_path):
                try:
                    os.remove(exe_path)
                except Exception:
                    pass
            write_error_artifacts(build_dir, "cmake_build", 2, full_output, target)
            diags = extract_diagnostics(full_output)
            raise CompilationError(
                2,
                full_output,
                stage="cmake_build",
                diagnostics=diags
            )

        # Step 4: Post-Build Physical Verification
        if not os.path.isfile(exe_path):
            msg = f"Build succeeded (exit 0) but target binary was not produced at: {exe_path}"
            write_error_artifacts(build_dir, "post_verification", 2, msg, target)
            raise CompilationError(2, msg, stage="post_verification")

        size = os.path.getsize(exe_path)
        if size == 0:
            msg = f"Target binary '{exe_path}' is empty (0 bytes)."
            write_error_artifacts(build_dir, "post_verification", 2, msg, target)
            try:
                os.remove(exe_path)
            except Exception:
                pass
            raise CompilationError(2, msg, stage="post_verification")

        pe_machine, pe_name = verify_pe_header(exe_path)
        if pe_machine != PE_MACHINE_I386:
            msg = f"Target binary PE machine {hex(pe_machine)} is not Win32 i386 (0x014c)."
            write_error_artifacts(build_dir, "post_verification", 2, msg, target)
            raise CompilationError(2, msg, stage="post_verification")

        duration = time.perf_counter() - t_start

        # Step 5: Manifest Generation
        manifest_data = generate_manifest(
            binary_path=exe_path,
            manifest_path=manifest_path,
            build_duration=duration,
            compiler_info=DEFAULT_COMPILER_NAME,
            target_name=target,
            config=config
        )

        return BuildResult(
            success=True,
            exe_path=exe_path,
            manifest_path=manifest_path,
            sha256=manifest_data["binary_sha256"],
            timestamp=manifest_data["timestamp"],
            iso_timestamp=manifest_data["iso_timestamp"],
            duration_seconds=manifest_data["duration_seconds"],
            exit_code=0,
            stage="complete",
            target=target,
            config=config,
            size_bytes=manifest_data["file_size"],
            pe_machine=pe_name,
            compiler_version=manifest_data["compiler"],
            git_commit=manifest_data["git"].get("commit_hash")
        )

    except (BuildGateError, KeyboardInterrupt):
        raise
    except Exception as ex:
        err_msg = f"Unexpected build guard exception: {str(ex)}"
        write_error_artifacts(build_dir, "internal_error", 2, err_msg, target)
        raise CompilationError(2, err_msg, stage="internal_error")


# =============================================================================
# High-Level BuildGuard OOP Wrapper
# =============================================================================

class BuildGuard:
    """Object-oriented wrapper around the build guard engine."""
    def __init__(self, root_dir: Optional[str] = None, build_dir: Optional[str] = None):
        self.root_dir = root_dir or DEFAULT_PROJECT_ROOT
        self.build_dir = build_dir or os.path.join(self.root_dir, "build-win32")

    def build_and_certify(self, target: str = DEFAULT_TARGET,
                           config: str = "Release",
                           rebuild: bool = True,
                           clean: bool = False,
                           timeout: int = 300) -> BuildResult:
        return verify_or_build_core(
            rebuild=rebuild,
            config=config,
            target=target,
            project_root=self.root_dir,
            build_dir=self.build_dir,
            timeout=timeout,
            clean_first=clean
        )

    def verify(self, target: str = DEFAULT_TARGET,
               config: str = "Release",
               max_age: Optional[float] = None) -> VerificationResult:
        target_exe = target if target.endswith(".exe") else f"{target}.exe"
        exe_path = os.path.join(self.build_dir, config, target_exe)
        manifest_path = os.path.join(self.build_dir, config, "build_manifest.json")
        return verify_manifest(exe_path, manifest_path, max_age_seconds=max_age)
