"""Channel A/C (Linux/WSL): document-studio & calculadora-unificada CLI/Xvfb/OCR adapter."""
from __future__ import annotations

import os
import shlex
import tempfile
from pathlib import Path

from .cli_runner import RunResult, run_wsl
from .config import DEFAULT_TIMEOUTS, REPO_ROOT, resolve_paths, win_to_wsl_path


def smoke_document_studio(
    platform: str,
    mode: str,
    out_png: Path,
    open_path: Path | None = None,
    page: int | None = None,
    calc_card: str | None = None,
    showcase_stem: bool = False,
    timeout: int | None = None,
    use_xvfb: bool = False,
) -> RunResult:
    """Invoke Linux `document-studio` via `run_document_studio.sh` in offscreen or Xvfb mode."""
    plat = platform.lower().strip()
    if plat == "win":
        from .win_gui_channel import smoke_document_studio as _win_smoke

        return _win_smoke(
            platform="win",
            mode=mode,
            out_png=out_png,
            open_path=open_path,
            page=page,
            calc_card=calc_card,
            showcase_stem=showcase_stem,
            timeout=timeout,
        )

    paths = resolve_paths(REPO_ROOT)
    runner_sh = paths["linux_run_document_studio"]
    if not runner_sh.exists():
        return RunResult(
            returncode=-1,
            stdout="",
            stderr=f"Missing Linux wrapper: {runner_sh}",
            duration_ms=0.0,
            timed_out=False,
        )

    out_png = Path(out_png)
    out_png.parent.mkdir(parents=True, exist_ok=True)

    # Isolate XDG_CONFIG_HOME in /tmp so tests never pollute user config
    xdg_tmp = "/tmp/cu_e2e_xdg_" + str(os.getpid())
    cmd: list[str] = []
    if use_xvfb:
        cmd.extend(["xvfb-run", "-a", "-s", "-screen 0 1440x960x24"])

    cmd.extend([
        win_to_wsl_path(runner_sh),
        "--smoke-test",
        "--mode",
        mode,
        "--screenshot-out",
        win_to_wsl_path(out_png),
    ])
    if open_path is not None:
        cmd.extend(["--open", win_to_wsl_path(open_path)])
    if page is not None:
        cmd.extend(["--page", str(int(page))])
    if calc_card is not None:
        cmd.extend(["--calc-card", calc_card])
    if showcase_stem:
        cmd.append("--showcase-stem")

    env: dict[str, str] = {"XDG_CONFIG_HOME": xdg_tmp}
    if not use_xvfb:
        env["DISPLAY"] = ""

    eff_timeout = timeout if timeout is not None else DEFAULT_TIMEOUTS["cli_smoke"]
    return run_wsl(
        cmd,
        env=env,
        timeout=eff_timeout,
        cwd_linux=win_to_wsl_path(paths["repo_root"]),
    )


def smoke_calculadora_linux(
    args: list[str],
    out_png: Path | None = None,
    use_xvfb: bool = True,
    timeout: int | None = None,
) -> RunResult:
    """Invoke Linux `calculadora-unificada` via `run_calculadora.sh` with optional Xvfb & screenshot."""
    paths = resolve_paths(REPO_ROOT)
    runner_sh = paths["linux_run_calculadora"]
    if not runner_sh.exists():
        return RunResult(
            returncode=-1,
            stdout="",
            stderr=f"Missing Linux wrapper: {runner_sh}",
            duration_ms=0.0,
            timed_out=False,
        )

    xdg_tmp = "/tmp/cu_e2e_calc_xdg_" + str(os.getpid())
    cmd: list[str] = []
    if use_xvfb:
        cmd.extend(["xvfb-run", "-a", "-s", "-screen 0 1440x960x24"])

    cmd.append(win_to_wsl_path(runner_sh))
    cmd.extend(str(a) for a in args)

    if out_png is not None:
        out_png = Path(out_png)
        out_png.parent.mkdir(parents=True, exist_ok=True)
        cmd.extend(["--screenshot-out", win_to_wsl_path(out_png)])

    env: dict[str, str] = {"XDG_CONFIG_HOME": xdg_tmp}
    if not use_xvfb:
        env["DISPLAY"] = ""

    eff_timeout = timeout if timeout is not None else DEFAULT_TIMEOUTS["cli_smoke"]
    return run_wsl(
        cmd,
        env=env,
        timeout=eff_timeout,
        cwd_linux=win_to_wsl_path(paths["repo_root"]),
    )


def run_tesseract_ocr(png_path: Path, lang: str = "eng", timeout: int = 20) -> str:
    """Run `tesseract <png> stdout` on Linux/WSL and return extracted UTF-8 text."""
    wsl_png = win_to_wsl_path(png_path)
    res = run_wsl(["tesseract", wsl_png, "stdout", "-l", lang], timeout=timeout)
    return res.stdout.strip() if res.returncode == 0 else ""


def check_xvfb_window(cmd_args: list[str], timeout: int = 15) -> RunResult:
    """Launch a Linux binary briefly under Xvfb and verify exit or window presence via xwininfo."""
    paths = resolve_paths(REPO_ROOT)
    script = (
        f"xvfb-run -a -s '-screen 0 1440x960x24' bash -lc "
        f"{shlex.quote(' '.join(shlex.quote(a) for a in cmd_args))}"
    )
    return run_wsl(["bash", "-lc", script], timeout=timeout, cwd_linux=win_to_wsl_path(paths["repo_root"]))
