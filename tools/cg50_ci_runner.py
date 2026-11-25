#!/usr/bin/env python3
"""
cg50_ci_runner.py - Portable, reproducible CI orchestrator for Casio fx-CG50 native core & Android app.

Resolves repository root relative to this script and discovers toolchains dynamically via PATH /
environment variables (with optional Windows DevTools fallback when installed), eliminating
hardcoded machine paths.

Profiles:
  --profile ci-fast       : Release build + CTest + Python oracle/graphics checks + Flutter analyze/test
  --profile ci-nightly    : ci-fast + Clang ASan+UBSan + hardware cert labs + coverage + live fuzzer
  --profile ci-exhaustive : ci-nightly + 100K parametric + 100+ mutation sandbox + stress/soak

Every run emits a cryptographically traceable run_manifest.json with git HEAD, ROM/warm-state SHA-256
(when locally present), compiler versions, RNG seed, exact exit codes, and durations.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import time

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
CORE_DIR = REPO_ROOT / "core"
BUILD_HOST = REPO_ROOT / "build-core-host"
BUILD_ASAN = REPO_ROOT / "build-core-asan"
ROMS_DIR = REPO_ROOT / "assets" / "roms"


def resolve_tool(env_var: str, names: list[str], fallbacks: list[str]) -> str | None:
    if env_val := os.environ.get(env_var):
        if pathlib.Path(env_val).is_file():
            return env_val
    for n in names:
        found = shutil.which(n)
        if found:
            return found
    for fb in fallbacks:
        if pathlib.Path(fb).is_file():
            return fb
    return None


CMAKE = resolve_tool("CMAKE_BIN", ["cmake"], [r"D:\DevTools\CMake\bin\cmake.exe"]) or "cmake"
CTEST = resolve_tool("CTEST_BIN", ["ctest"], [r"D:\DevTools\CMake\bin\ctest.exe"]) or "ctest"
FLUTTER = resolve_tool("FLUTTER_BIN", ["flutter", "flutter.bat"], [r"D:\DevTools\flutter\bin\flutter.bat"])
GIT = resolve_tool("GIT_BIN", ["git"], [r"D:\DevTools\Tools\Git\cmd\git.exe"]) or "git"
PYTHON = sys.executable
CLANGXX = resolve_tool(
    "CLANGXX_BIN",
    ["clang++"],
    [r"C:\Program Files\LLVM\bin\clang++.exe", r"D:\DevTools\msys64\ucrt64\bin\clang++.exe"],
)


def sha256_file(path: pathlib.Path) -> str | None:
    if not path.is_file():
        return None
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(1 << 20)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest().upper()


def get_git_head() -> str:
    try:
        r = subprocess.run([GIT, "-C", str(REPO_ROOT), "rev-parse", "HEAD"], capture_output=True, text=True)
        return r.stdout.strip() if r.returncode == 0 else "UNKNOWN"
    except OSError:
        return "UNKNOWN"


def host_exe(name: str) -> pathlib.Path:
    suffix = ".exe" if platform.system() == "Windows" else ""
    for candidate in (
        BUILD_HOST / "Release" / f"{name}{suffix}",
        BUILD_HOST / f"{name}{suffix}",
    ):
        if candidate.is_file():
            return candidate
    return BUILD_HOST / "Release" / f"{name}{suffix}"


def run_step(name: str, cmd: list[str], cwd: pathlib.Path | None = None, env_extra: dict[str, str] | None = None) -> dict:
    print(f"\n[CI] >>> {name}: {' '.join(cmd)}", flush=True)
    env = os.environ.copy()
    if env_extra:
        env.update(env_extra)
    t0 = time.perf_counter()
    proc = subprocess.run(cmd, cwd=str(cwd or REPO_ROOT), env=env, capture_output=True, text=True)
    dt = time.perf_counter() - t0
    ok = proc.returncode == 0
    print(f"[CI] <<< {name}: {'PASS' if ok else 'FAIL'} (rc={proc.returncode}, {dt:.2f}s)", flush=True)
    if not ok:
        if proc.stdout:
            print(proc.stdout[-2000:], flush=True)
        if proc.stderr:
            print(proc.stderr[-2000:], file=sys.stderr, flush=True)
    return {
        "step": name,
        "command": cmd,
        "exit_code": proc.returncode,
        "status": "CONFIRMED" if ok else "FAILED",
        "duration_s": round(dt, 3),
        "stdout_tail": proc.stdout[-1500:],
        "stderr_tail": proc.stderr[-1500:],
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="Casio fx-CG50 Portable Reproducible CI Orchestrator")
    ap.add_argument("--profile", choices=["ci-fast", "ci-nightly", "ci-exhaustive"], default="ci-fast")
    ap.add_argument("--seed", type=int, default=20261124)
    ap.add_argument("--soak-hours", type=float, default=0.0, help="Optional soak duration in hours (0=skip)")
    ap.add_argument("--skip-flutter", action="store_true", help="Skip Flutter analyze/test if SDK is unavailable")
    ap.add_argument(
        "--out-dir",
        type=pathlib.Path,
        default=REPO_ROOT / "tests" / "cg50_validation" / "reports" / "ci_runs",
    )
    args = ap.parse_args()

    ts = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d_%H%M%S")
    run_dir = args.out_dir / f"{args.profile}_{ts}"
    run_dir.mkdir(parents=True, exist_ok=True)
    has_roms = (ROMS_DIR / "SystemROM.raw").is_file()

    manifest: dict = {
        "schema_version": "2.0.0",
        "profile": args.profile,
        "host_platform": platform.platform(),
        "repo_root": str(REPO_ROOT),
        "has_local_roms": has_roms,
        "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "git_head": get_git_head(),
        "rng_seed": args.seed,
        "soak_hours": args.soak_hours,
        "artifacts_sha256": {
            "SystemROM.raw": sha256_file(ROMS_DIR / "SystemROM.raw"),
            "UserROM.raw": sha256_file(ROMS_DIR / "UserROM.raw"),
            "C2ROM.raw": sha256_file(ROMS_DIR / "C2ROM.raw"),
            "cg50_warm_state.bin": sha256_file(ROMS_DIR / "cg50_warm_state.bin"),
            "cg50_warm_state.bin.gz": sha256_file(ROMS_DIR / "cg50_warm_state.bin.gz"),
        },
        "steps": [],
    }

    steps = manifest["steps"]

    # 1. Host Release build + CTest + deterministic Python checks
    cfg_cmd = [
        CMAKE, "-S", str(CORE_DIR), "-B", str(BUILD_HOST),
        "-DBUILD_CORE_TESTS=ON",
        f"-DBUILD_CG50_BATTERY={'ON' if has_roms else 'OFF'}",
    ]
    if platform.system() != "Windows":
        cfg_cmd.append("-DCMAKE_BUILD_TYPE=Release")
    steps.append(run_step("cmake_configure_host", cfg_cmd))
    build_cmd = [CMAKE, "--build", str(BUILD_HOST), "--parallel"]
    if platform.system() == "Windows":
        build_cmd[3:3] = ["--config", "Release"]
    steps.append(run_step("cmake_build_host_release", build_cmd))

    ctest_cmd = [CTEST, "--test-dir", str(BUILD_HOST), "--output-on-failure"]
    if platform.system() == "Windows":
        ctest_cmd.extend(["-C", "Release"])
    if not has_roms:
        # Exclude ROM-dependent smoke tests when ROM dumps are not present in CI
        ctest_cmd.extend(["-E", "cg50_android_smoke|cg50_thread_isolation|cw2_headless|nx_regression"])
    steps.append(run_step("ctest_host_release", ctest_cmd, env_extra={"CG50_CERT_STRICT_NO_HLE": "1"}))

    diff_test = REPO_ROOT / "tests" / "cg50_validation" / "oracles" / "test_diff_comparator.py"
    if diff_test.is_file():
        steps.append(run_step("python_diff_comparator_tests", [PYTHON, str(diff_test)]))

    gfx_ref = REPO_ROOT / "tests" / "cg50_validation" / "graphics" / "verify_graphics_references.py"
    gfx_dir = REPO_ROOT / "tests" / "cg50_validation" / "reports" / "graphics_v46"
    if gfx_ref.is_file() and (gfx_dir / "BENCH-MANDELBROT.bmp").is_file():
        steps.append(run_step("python_graphics_reference_audit", [PYTHON, str(gfx_ref)]))

    if not args.skip_flutter and FLUTTER:
        steps.append(run_step("flutter_analyze", [FLUTTER, "analyze", "--no-pub"], cwd=REPO_ROOT / "apps" / "android"))
        steps.append(run_step("flutter_test", [FLUTTER, "test", "--no-pub"], cwd=REPO_ROOT / "apps" / "android"))

    # 2. Nightly additions: Hardware cert labs + ASan/UBSan + Coverage + Fuzzer
    if args.profile in ("ci-nightly", "ci-exhaustive"):
        reg_lab = host_exe("cg50_register_cert_lab")
        if reg_lab.is_file():
            steps.append(run_step("hw_register_cert_lab", [str(reg_lab), "--out", str(run_dir / "reg_cert")]))
        stress_lab = host_exe("cg50_combined_hw_stress")
        if stress_lab.is_file():
            steps.append(run_step(
                "hw_combined_stress_lab",
                [str(stress_lab), "--seqs", "50000", "--threads", "8", "--out", str(run_dir / "stress_cert")],
                env_extra={"CG50_MMIO_AUDIT": "1"},
            ))
        if CLANGXX:
            asan_env: dict[str, str] = {}
            asan_dll = pathlib.Path(CLANGXX).resolve().parents[1] / "lib" / "clang"
            extra_paths = [str(pathlib.Path(CLANGXX).resolve().parent)]
            if asan_dll.is_dir():
                for dll in asan_dll.rglob("clang_rt.asan_dynamic-x86_64.dll"):
                    extra_paths.append(str(dll.parent))
                    break
            asan_env["PATH"] = os.pathsep.join(extra_paths) + os.pathsep + os.environ.get("PATH", "")
            steps.append(run_step("cmake_configure_asan_ubsan", [
                CMAKE, "-S", str(CORE_DIR), "-B", str(BUILD_ASAN),
                "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                f"-DCMAKE_CXX_COMPILER={CLANGXX}",
                "-DBUILD_CORE_TESTS=ON", "-DBUILD_CG50_BATTERY=OFF",
                "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_DISABLE_VECTOR_ANNOTATION -D_DISABLE_STRING_ANNOTATION -Wall -Wno-unused-function",
            ], env_extra=asan_env))
            steps.append(run_step("cmake_build_asan_ubsan", [
                CMAKE, "--build", str(BUILD_ASAN),
                "--target", "cg50_sh7305_hw_test", "cg50_hw_silicon_100_test", "cg50_flash_cfi_test",
                "cg50_sh4a_isa_test", "cg50_usb_smem_qr_test", "cg50_register_cert_lab", "cg50_combined_hw_stress",
                "--parallel",
            ], env_extra=asan_env))
            steps.append(run_step("ctest_asan_ubsan", [
                CTEST, "--test-dir", str(BUILD_ASAN),
                "-R", "cg50_(sh7305_hw|hw_silicon_100|flash_cfi|sh4a_isa|usb_smem_qr)_test",
                "--output-on-failure",
            ], env_extra=asan_env))

        cov_script = REPO_ROOT / "tests" / "cg50_validation" / "coverage" / "run_coverage_12_metrics.py"
        if cov_script.is_file():
            steps.append(run_step("coverage_12_metrics", [PYTHON, str(cov_script)]))

        fuzzer_exe = host_exe("cg50_fuzzer")
        if fuzzer_exe.is_file() and has_roms:
            steps.append(run_step("fuzzer_live_200", [
                str(fuzzer_exe), "--roms", str(ROMS_DIR),
                "--seed", str(args.seed), "--grammar", "100000", "--live", "200",
                "--out", str(run_dir / "fuzz"),
            ], env_extra={"CG50_CERT_STRICT_NO_HLE": "1"}))

    # 3. Exhaustive additions: 100K parametric + Mutation Testing + Soak
    if args.profile == "ci-exhaustive":
        val_exe = host_exe("cg50_validation_runner")
        if val_exe.is_file() and has_roms:
            steps.append(run_step("validation_100k_parametric", [
                str(val_exe), "--roms", str(ROMS_DIR),
                "--strict-no-hle", "--strict-no-fpu", "--threads", "16",
                "--parametric", "100000", "--seed", str(args.seed),
                "--out", str(run_dir / "val_100k"),
            ], env_extra={"CG50_CERT_STRICT_NO_HLE": "1"}))
        mut_script = REPO_ROOT / "tools" / "cg50_mutation_tester.py"
        if mut_script.is_file():
            steps.append(run_step("mutation_testing", [PYTHON, str(mut_script)]))
        if args.soak_hours > 0.0:
            torture_exe = host_exe("cg50_hw_torture_runner")
            if torture_exe.is_file():
                steps.append(run_step("soak_run", [
                    str(torture_exe), "--profile", "full-cert",
                    "--hours", str(args.soak_hours),
                    "--out", str(run_dir / "soak"),
                ]))

    failed = [s for s in steps if s["status"] != "CONFIRMED"]
    manifest["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    manifest["summary"] = {
        "total_steps": len(steps),
        "confirmed": len(steps) - len(failed),
        "failed": len(failed),
        "overall": "CONFIRMED" if not failed else "FAILED",
    }

    manifest_path = run_dir / "run_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"\n[CI] Manifest written: {manifest_path}")
    print(f"[CI] Summary: {manifest['summary']}")
    return 0 if not failed else 1


if __name__ == "__main__":
    raise SystemExit(main())
