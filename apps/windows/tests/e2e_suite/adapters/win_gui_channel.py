"""Channel A/C (Windows): DocumentStudio.exe CLI smoke & CalculadoraUnificada.exe Win32 probe."""
from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path
from typing import Any

from .cli_runner import RunResult, _kill_process_tree_win, run_win
from .config import DEFAULT_TIMEOUTS, REPO_ROOT, resolve_paths


def smoke_document_studio(
    platform: str,
    mode: str,
    out_png: Path,
    open_path: Path | None = None,
    page: int | None = None,
    calc_card: str | None = None,
    showcase_stem: bool = False,
    timeout: int | None = None,
) -> RunResult:
    """Invoke DocumentStudio (--smoke-test) on Windows or Linux and capture screenshot PNG."""
    plat = platform.lower().strip()
    if plat == "linux":
        from .linux_gui_channel import smoke_document_studio as _linux_smoke

        return _linux_smoke(
            platform="linux",
            mode=mode,
            out_png=out_png,
            open_path=open_path,
            page=page,
            calc_card=calc_card,
            showcase_stem=showcase_stem,
            timeout=timeout,
        )

    paths = resolve_paths(REPO_ROOT)
    exe = paths["win_document_studio"]
    if not exe.exists():
        return RunResult(
            returncode=-1,
            stdout="",
            stderr=f"Authoritative DocumentStudio.exe missing: {exe}",
            duration_ms=0.0,
            timed_out=False,
        )

    out_png = Path(out_png)
    out_png.parent.mkdir(parents=True, exist_ok=True)

    cmd: list[str] = [
        str(exe),
        "--smoke-test",
        "--mode",
        mode,
        "--screenshot-out",
        str(out_png),
    ]
    if open_path is not None:
        cmd.extend(["--open", str(open_path)])
    if page is not None:
        cmd.extend(["--page", str(int(page))])
    if calc_card is not None:
        cmd.extend(["--calc-card", calc_card])
    if showcase_stem:
        cmd.append("--showcase-stem")

    eff_timeout = timeout if timeout is not None else DEFAULT_TIMEOUTS["cli_smoke"]
    return run_win(cmd, timeout=eff_timeout, cwd=paths["repo_root"])


def probe_calculadora_win(
    args: list[str],
    out_png: Path | None = None,
    timeout_s: float = 8.0,
) -> dict[str, Any]:
    """Launch CalculadoraUnificada.exe briefly, optionally capture via InspectGui.exe, and clean up PID."""
    paths = resolve_paths(REPO_ROOT)
    exe = paths["win_calculadora"]
    inspect_exe = paths["win_inspect_gui"]

    result: dict[str, Any] = {
        "pid": None,
        "started": False,
        "alive_after_wait": False,
        "window_captured": False,
        "out_png": out_png,
        "inspect_stdout": "",
        "returncode": None,
        "error": None,
    }

    if not exe.exists():
        result["error"] = f"CalculadoraUnificada.exe missing: {exe}"
        return result

    env = os.environ.copy()
    env["CALCULADORA_CONSOLA"] = "1"

    cmd = [str(exe), *[str(a) for a in args]]
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=str(paths["repo_root"]),
        env=env,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    result["pid"] = proc.pid
    result["started"] = True

    try:
        deadline = time.monotonic() + max(1.0, float(timeout_s))
        # Give the Win32 message loop & ROM init time to create CasioNativeMain
        time.sleep(min(1.5, max(0.4, timeout_s * 0.25)))
        result["alive_after_wait"] = proc.poll() is None

        if out_png is not None and inspect_exe.exists() and result["alive_after_wait"]:
            out_png = Path(out_png)
            out_png.parent.mkdir(parents=True, exist_ok=True)
            while time.monotonic() < deadline:
                cap = run_win(
                    [str(inspect_exe), str(proc.pid), "capture", str(out_png)],
                    timeout=5,
                    cwd=paths["repo_root"],
                )
                result["inspect_stdout"] = cap.stdout.strip()
                if out_png.exists() and out_png.stat().st_size > 100:
                    result["window_captured"] = True
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.5)
        elif result["alive_after_wait"]:
            time.sleep(min(0.8, max(0.2, deadline - time.monotonic())))
            result["alive_after_wait"] = proc.poll() is None

        if proc.poll() is not None:
            result["returncode"] = proc.returncode
    finally:
        if proc.poll() is None:
            _kill_process_tree_win(proc.pid)
            try:
                proc.terminate()
                proc.wait(timeout=3)
            except (OSError, subprocess.SubprocessError):
                try:
                    proc.kill()
                    proc.wait(timeout=2)
                except (OSError, subprocess.SubprocessError):
                    pass
        if result["returncode"] is None:
            result["returncode"] = proc.poll()
        try:
            if proc.stdout:
                proc.stdout.close()
            if proc.stderr:
                proc.stderr.close()
        except OSError:
            pass

    return result


def run_winrt_ocr(png_path: Path, timeout: int = 20) -> str:
    """Invoke tools/ocr_png.ps1 on Windows to extract OCR text via Windows.Media.Ocr."""
    paths = resolve_paths(REPO_ROOT)
    script = paths["ocr_ps1"]
    if not script.exists():
        return ""
    res = run_win(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script), str(png_path)],
        timeout=timeout,
        cwd=paths["repo_root"],
    )
    return res.stdout.strip() if res.returncode == 0 else ""
