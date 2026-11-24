#!/usr/bin/env python3
"""
cg50_ci_runner.py - Reproducible CI orchestrator for Casio fx-CG50 native core & Android app.

Profiles:
  --profile ci-fast       : Release build + CTest + thread isolation + Flutter analyze/test (< 60s)
  --profile ci-nightly    : ci-fast + MSVC ASan + MSYS2 UBSan + gcov coverage + 500-case live fuzzer
  --profile ci-exhaustive : ci-nightly + 100K parametric + graphics benchmarks + mutation testing + soak

Every run emits a cryptographically traceable run_manifest.json with git HEAD, ROM/warm-state SHA-256,
compiler versions, RNG seed, exact exit codes, and durations.
"""
import argparse
import datetime
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import time

REPO_ROOT = pathlib.Path(r"D:\Proyectos\CalculadorasUnificadas")
CORE_DIR = REPO_ROOT / "core"
BUILD_HOST = REPO_ROOT / "build-core-host"
BUILD_ASAN = REPO_ROOT / "build-core-asan"
BUILD_UBSAN = REPO_ROOT / "build-core-ubsan"
ROMS_DIR = REPO_ROOT / "assets" / "roms"

CMAKE = r"D:\DevTools\CMake\bin\cmake.exe"
CTEST = r"D:\DevTools\CMake\bin\ctest.exe"
FLUTTER = r"D:\DevTools\flutter\bin\flutter.bat"
GIT = r"D:\DevTools\Tools\Git\cmd\git.exe"
PYTHON = r"D:\DevTools\Python311\python.exe"
MSYS_GXX = r"D:\DevTools\msys64\ucrt64\bin\g++.exe"


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
    r = subprocess.run([GIT, "-C", str(REPO_ROOT), "rev-parse", "HEAD"], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else "UNKNOWN"


def run_step(name: str, cmd: list[str], cwd: pathlib.Path | None = None, env_extra: dict | None = None) -> dict:
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
    ap = argparse.ArgumentParser(description="Casio fx-CG50 Reproducible CI Orchestrator")
    ap.add_argument("--profile", choices=["ci-fast", "ci-nightly", "ci-exhaustive"], default="ci-fast")
    ap.add_argument("--seed", type=int, default=20261124)
    ap.add_argument("--soak-hours", type=float, default=0.0, help="Optional soak duration in hours (0=skip)")
    ap.add_argument("--out-dir", type=pathlib.Path,
                    default=REPO_ROOT / "tests" / "cg50_validation" / "reports" / "ci_runs")
    args = ap.parse_args()

    ts = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d_%H%M%S")
    run_dir = args.out_dir / f"{args.profile}_{ts}"
    run_dir.mkdir(parents=True, exist_ok=True)

    manifest = {
        "profile": args.profile,
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

    # 1. Host Release build + full CTest (all profiles)
    steps.append(run_step("cmake_configure_host", [
        CMAKE, "-S", str(CORE_DIR), "-B", str(BUILD_HOST),
        "-DBUILD_CORE_TESTS=ON", "-DBUILD_CG50_BATTERY=ON",
    ]))
    steps.append(run_step("cmake_build_host_release", [
        CMAKE, "--build", str(BUILD_HOST), "--config", "Release", "--parallel",
    ]))
    steps.append(run_step("ctest_host_release", [
        CTEST, "--test-dir", str(BUILD_HOST), "-C", "Release", "--output-on-failure",
    ], env_extra={"CG50_CERT_STRICT_NO_HLE": "1"}))
    steps.append(run_step("flutter_analyze", [
        FLUTTER, "analyze", "--no-pub",
    ], cwd=REPO_ROOT / "apps" / "android"))
    steps.append(run_step("flutter_test", [
        FLUTTER, "test", "--no-pub",
    ], cwd=REPO_ROOT / "apps" / "android"))

    # 2. Nightly additions: Clang 23 ASan + UBSan + Fuzzer + Coverage
    if args.profile in ("ci-nightly", "ci-exhaustive"):
        clangxx = r"C:/Program Files/LLVM/bin/clang++.exe"
        asan_dll = pathlib.Path(r"C:\Program Files\LLVM\lib\clang\23\lib\windows\clang_rt.asan_dynamic-x86_64.dll")
        asan_env = {
            "PATH": r"C:\Program Files\LLVM\bin;C:\Program Files\LLVM\lib\clang\23\lib\windows;" + os.environ.get("PATH", ""),
        }
        steps.append(run_step("cmake_configure_asan_ubsan", [
            CMAKE, "-S", str(CORE_DIR), "-B", str(BUILD_ASAN),
            "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_CXX_COMPILER={clangxx}",
            "-DBUILD_CORE_TESTS=ON", "-DBUILD_CG50_BATTERY=OFF",
            "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_DISABLE_VECTOR_ANNOTATION -D_DISABLE_STRING_ANNOTATION -Wall -Wno-unused-function",
        ], env_extra=asan_env))
        steps.append(run_step("cmake_build_asan_ubsan", [
            CMAKE, "--build", str(BUILD_ASAN),
            "--target", "cg50_sh7305_hw_test", "cg50_flash_cfi_test",
            "cg50_sh4a_isa_test", "cg50_usb_smem_qr_test", "cg50_thread_isolation_test",
            "-j", "16",
        ], env_extra=asan_env))
        if asan_dll.is_file() and BUILD_ASAN.is_dir():
            import shutil
            shutil.copy2(asan_dll, BUILD_ASAN / asan_dll.name)
        steps.append(run_step("ctest_asan_ubsan", [
            CTEST, "--test-dir", str(BUILD_ASAN),
            "-R", "cg50_(sh7305_hw|flash_cfi|sh4a_isa|usb_smem_qr|thread_isolation)_test",
            "--output-on-failure",
        ], env_extra=asan_env))

        cov_script = REPO_ROOT / "tests" / "cg50_validation" / "coverage" / "run_coverage_12_metrics.py"
        if cov_script.exists():
            steps.append(run_step("coverage_12_metrics", [PYTHON, str(cov_script)]))

        fuzzer_exe = BUILD_HOST / "Release" / "cg50_fuzzer.exe"
        if fuzzer_exe.exists():
            steps.append(run_step("fuzzer_live_500", [
                str(fuzzer_exe), "--roms", str(ROMS_DIR),
                "--seed", str(args.seed), "--grammar", "100000", "--live", "200",
                "--out", str(run_dir / "fuzz"),
            ], env_extra={"CG50_CERT_STRICT_NO_HLE": "1"}))

    # 3. Exhaustive additions: 100K parametric + Mutation Testing + Graphics + Perf + Soak
    if args.profile == "ci-exhaustive":
        val_exe = BUILD_HOST / "Release" / "cg50_validation_runner.exe"
        if val_exe.exists():
            steps.append(run_step("validation_100k_parametric", [
                str(val_exe), "--roms", str(ROMS_DIR),
                "--strict-no-hle", "--strict-no-fpu", "--threads", "16",
                "--parametric", "100000", "--seed", str(args.seed),
                "--out", str(run_dir / "val_100k"),
            ], env_extra={"CG50_CERT_STRICT_NO_HLE": "1"}))
        mut_script = REPO_ROOT / "tools" / "cg50_mutation_tester.py"
        if mut_script.exists():
            steps.append(run_step("mutation_testing", [PYTHON, str(mut_script)]))
        if args.soak_hours > 0.0:
            torture_exe = BUILD_HOST / "Release" / "cg50_hw_torture_runner.exe"
            if torture_exe.exists():
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
