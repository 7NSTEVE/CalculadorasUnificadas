"""Subprocess execution adapters for Windows native and WSL Linux targets."""
from __future__ import annotations

import os
import shlex
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

from .config import is_linux_host, is_windows_host


@dataclass
class RunResult:
    returncode: int
    stdout: str
    stderr: str
    duration_ms: float
    timed_out: bool


def _kill_process_tree_win(pid: int) -> None:
    if not is_windows_host():
        return
    try:
        subprocess.run(
            ["taskkill", "/F", "/T", "/PID", str(pid)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        pass


def run_win(
    cmd: list[str],
    env: dict[str, str] | None = None,
    timeout: int = 30,
    cwd: Path | None = None,
) -> RunResult:
    """Execute a command natively on Windows with timeout and tree cleanup."""
    merged_env = os.environ.copy()
    if env:
        for k, v in env.items():
            if v is None:
                merged_env.pop(k, None)
            else:
                merged_env[k] = v

    creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    t0 = time.perf_counter()
    proc = subprocess.Popen(
        [str(c) for c in cmd],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=str(cwd) if cwd else None,
        env=merged_env,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=creationflags,
    )
    timed_out = False
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        _kill_process_tree_win(proc.pid)
        try:
            proc.kill()
        except OSError:
            pass
        stdout, stderr = proc.communicate(timeout=5)

    dt_ms = (time.perf_counter() - t0) * 1000.0
    rc = -1 if timed_out else int(proc.returncode)
    return RunResult(
        returncode=rc,
        stdout=stdout or "",
        stderr=stderr or "",
        duration_ms=round(dt_ms, 2),
        timed_out=timed_out,
    )


_CLEAN_LINUX_PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"


def sanitize_wsl_env_prefix() -> str:
    """Return a shell command prefix that strips DrvFs (/mnt/...) entries from WSL PATH."""
    return f"export PATH={shlex.quote(_CLEAN_LINUX_PATH)}"


def run_wsl(
    cmd: list[str],
    env: dict[str, str] | None = None,
    timeout: int = 30,
    cwd_linux: str | None = None,
    clean_linux_path: bool = False,
) -> RunResult:
    """Execute a Linux command directly (when on Linux) or via `wsl.exe` (when on Windows)."""
    if is_linux_host():
        merged_env = os.environ.copy()
        if clean_linux_path:
            cur_path = merged_env.get("PATH", _CLEAN_LINUX_PATH)
            clean_parts = [p for p in cur_path.split(":") if p and not p.startswith("/mnt/")]
            merged_env["PATH"] = ":".join(clean_parts) if clean_parts else _CLEAN_LINUX_PATH
        if env:
            for k, v in env.items():
                if v is None:
                    merged_env.pop(k, None)
                else:
                    merged_env[k] = v
        t0 = time.perf_counter()
        proc = subprocess.Popen(
            [str(c) for c in cmd],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=cwd_linux,
            env=merged_env,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        timed_out = False
        try:
            stdout, stderr = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            proc.kill()
            stdout, stderr = proc.communicate(timeout=5)
        dt_ms = (time.perf_counter() - t0) * 1000.0
        rc = -1 if timed_out else int(proc.returncode)
        return RunResult(
            returncode=rc,
            stdout=stdout or "",
            stderr=stderr or "",
            duration_ms=round(dt_ms, 2),
            timed_out=timed_out,
        )

    # Running on Windows -> bridge through wsl.exe bash -lc
    parts: list[str] = []
    if clean_linux_path:
        parts.append(sanitize_wsl_env_prefix())
    if cwd_linux:
        parts.append(f"cd {shlex.quote(cwd_linux)}")

    env_prefix: list[str] = []
    if env:
        env_prefix.append("env")
        for k, v in env.items():
            if v is None:
                env_prefix.append(f"-u {shlex.quote(k)}")
            else:
                env_prefix.append(f"{k}={shlex.quote(v)}")

    quoted_cmd = " ".join(shlex.quote(str(c)) for c in cmd)
    full_exec = (" ".join(env_prefix) + " " + quoted_cmd).strip()
    parts.append(full_exec)
    bash_script = " && ".join(parts)

    t0 = time.perf_counter()
    proc = subprocess.Popen(
        ["wsl", "bash", "-lc", bash_script],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    timed_out = False
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        _kill_process_tree_win(proc.pid)
        try:
            proc.kill()
        except OSError:
            pass
        stdout, stderr = proc.communicate(timeout=5)

    dt_ms = (time.perf_counter() - t0) * 1000.0
    rc = -1 if timed_out else int(proc.returncode)
    return RunResult(
        returncode=rc,
        stdout=stdout or "",
        stderr=stderr or "",
        duration_ms=round(dt_ms, 2),
        timed_out=timed_out,
    )
