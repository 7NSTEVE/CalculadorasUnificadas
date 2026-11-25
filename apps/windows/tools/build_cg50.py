#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_cg50.py — Standalone CLI driver for Calculadora Unificada Fail-Closed Build Engine.
Milestone 1 (Requirement R1: Control estricto de compilación y corte ante fallos).

Standardized Exit Codes:
  0: SUCCESS (Fresh build certified by manifest or successful verification)
  1: CONFIG_ERROR (CMake configure failed)
  2: BUILD_ERROR / ABORT (Compilation/Link error or fail-closed abort)
  3: MANIFEST_ERROR (Verification failed, hash mismatch, or stale manifest)
  4: LOCK_OR_ENV_ERROR (File locked by running process, toolchain missing)

Usage:
  python build_cg50.py [--rebuild] [--clean] [--verify-only]
                       [--target TARGET] [--build-dir BUILD_DIR]
                       [--config {Release,Debug,RelWithDebInfo}]
                       [--json] [--quiet] [--verbose] [--timeout SECONDS]
"""

import argparse
import json
import os
import sys

# Ensure build_guard can be imported from the current directory
TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

from build_guard import (
    verify_or_build_core,
    verify_manifest,
    invalidate_stale_artifacts,
    DEFAULT_PROJECT_ROOT,
    DEFAULT_BUILD_DIR,
    DEFAULT_TARGET,
    BuildGateError,
    ConfigError,
    CompilationError,
    ManifestVerificationError,
    ProcessLockError,
    BuildResult,
    VerificationResult
)


def print_banner(text: str, file=sys.stdout):
    line = "=" * 65
    print(line, file=file)
    print(f"  {text}", file=file)
    print(line, file=file)


def main():
    parser = argparse.ArgumentParser(
        description="Driver no interactivo de compilación y certificación de artefactos (R1)."
    )
    parser.add_argument(
        "--rebuild", dest="rebuild", action="store_true", default=True,
        help="Forzar invalidación previa y recompilación limpia (predeterminado: True)."
    )
    parser.add_argument(
        "--no-rebuild", dest="rebuild", action="store_false",
        help="Reutilizar binario existente si el manifiesto SHA-256 es válido."
    )
    parser.add_argument(
        "--clean", action="store_true", default=False,
        help="Eliminar el ejecutable del target y su build_manifest.json, luego terminar."
    )
    parser.add_argument(
        "--verify-only", action="store_true", default=False,
        help="Solo verificar integridad y autenticidad del binario contra el manifiesto sin compilar."
    )
    parser.add_argument(
        "--target", default=DEFAULT_TARGET,
        help=f"Target CMake a construir o verificar (predeterminado: '{DEFAULT_TARGET}')."
    )
    parser.add_argument(
        "--build-dir", default=DEFAULT_BUILD_DIR,
        help=f"Directorio de build (predeterminado: '{DEFAULT_BUILD_DIR}')."
    )
    parser.add_argument(
        "--config", default="Release", choices=["Release", "Debug", "RelWithDebInfo"],
        help="Configuración de compilación (predeterminado: 'Release')."
    )
    parser.add_argument(
        "--json", action="store_true", default=False,
        help="Emitir salida estructurada en formato JSON exclusivamente por stdout."
    )
    parser.add_argument(
        "--quiet", "-q", action="store_true", default=False,
        help="Suprimir mensajes informativos de progreso (solo errores y resultado final)."
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", default=False,
        help="Imprimir salida detallada de herramientas subyacentes."
    )
    parser.add_argument(
        "--timeout", type=int, default=300,
        help="Tiempo límite de compilación en segundos (predeterminado: 300)."
    )

    args = parser.parse_args()

    # Normalize build_dir and project_root
    build_dir = os.path.abspath(args.build_dir)
    project_root = os.path.dirname(build_dir)
    target_exe = args.target if args.target.endswith(".exe") else f"{args.target}.exe"
    release_dir = os.path.join(build_dir, args.config)
    exe_path = os.path.join(release_dir, target_exe)
    manifest_path = os.path.join(release_dir, "build_manifest.json")

    # -------------------------------------------------------------------------
    # Mode 1: --clean
    # -------------------------------------------------------------------------
    if args.clean:
        try:
            invalidate_stale_artifacts(release_dir, args.target)
            if args.json:
                out = {
                    "success": True,
                    "status": "CONFIRMED",
                    "action": "clean",
                    "target": args.target,
                    "cleaned_paths": [exe_path, manifest_path]
                }
                print(json.dumps(out, indent=2))
            elif not args.quiet:
                print(f"[CLEAN] Artefactos eliminados exitosamente para target '{args.target}' en {release_dir}")
            sys.exit(0)
        except Exception as e:
            if args.json:
                out = {"success": False, "status": "FAILED", "action": "clean", "error": str(e)}
                print(json.dumps(out, indent=2))
            else:
                print(f"[ERROR] Error al limpiar artefactos: {e}", file=sys.stderr)
            sys.exit(4)

    # -------------------------------------------------------------------------
    # Mode 2: --verify-only
    # -------------------------------------------------------------------------
    if args.verify_only:
        v_res = verify_manifest(exe_path, manifest_path)
        if args.json:
            payload = {
                "success": v_res.is_valid,
                "status": v_res.status,
                "action": "verify_only",
                "target": args.target,
                "config": args.config,
                "exe_path": exe_path,
                "manifest_path": manifest_path,
                "sha256": v_res.sha256 or None,
                "reason": v_res.reason,
                "details": v_res.details
            }
            print(json.dumps(payload, indent=2))
        else:
            if v_res.is_valid:
                if not args.quiet:
                    print_banner("ARTIFACT VERIFICATION: CONFIRMED (PASS)")
                    print(f"  Target:     {args.target}")
                    print(f"  Binary:     {exe_path}")
                    print(f"  SHA-256:    {v_res.sha256}")
                    print(f"  Manifest:   {manifest_path}")
                    print("=" * 65)
            else:
                print_banner(f"ARTIFACT VERIFICATION FAILED ({v_res.status})", file=sys.stderr)
                print(f"  Reason:     {v_res.reason}", file=sys.stderr)
                print(f"  Binary:     {exe_path}", file=sys.stderr)
                print(f"  Manifest:   {manifest_path}", file=sys.stderr)
                print("=" * 65, file=sys.stderr)

        sys.exit(0 if v_res.is_valid else 3)

    # -------------------------------------------------------------------------
    # Mode 3: Build / Rebuild Execution
    # -------------------------------------------------------------------------
    if not args.json and not args.quiet:
        action_desc = "Recompilación limpia forzada" if args.rebuild else "Compilación incremental / Verificación"
        print(f"[BUILD_CG50] Iniciando {action_desc} para target '{args.target}' ({args.config})...")

    try:
        build_result = verify_or_build_core(
            rebuild=args.rebuild,
            config=args.config,
            target=args.target,
            project_root=project_root,
            build_dir=build_dir,
            timeout=args.timeout,
            clean_first=False
        )

        if args.json:
            print(json.dumps(build_result.to_dict(), indent=2))
        elif not args.quiet:
            print_banner("BUILD GUARD CERTIFICATION: SUCCESS (PASS)")
            print(f"  Target:     {build_result.target}")
            print(f"  Config:     {build_result.config} (Win32 x86)")
            print(f"  Binary:     {build_result.exe_path}")
            print(f"  Size:       {build_result.size_bytes:,} bytes")
            print(f"  SHA-256:    {build_result.sha256}")
            print(f"  Duration:   {build_result.duration_seconds:.3f}s")
            print(f"  Manifest:   {build_result.manifest_path}")
            print("=" * 65)

        sys.exit(0)

    except ConfigError as ce:
        if args.json:
            out = {
                "success": False,
                "status": "FAILED",
                "exit_code": 1,
                "stage": ce.stage,
                "target": args.target,
                "error": ce.message,
                "diagnostics": ce.diagnostics
            }
            print(json.dumps(out, indent=2))
        else:
            print_banner("BUILD GUARD CONFIGURATION ERROR (EXIT CODE 1)", file=sys.stderr)
            print(f"  Stage:       {ce.stage}", file=sys.stderr)
            print(f"  Error:       {ce.message}", file=sys.stderr)
            if ce.diagnostics:
                print("  Diagnostics:", file=sys.stderr)
                for d in ce.diagnostics[:10]:
                    print(f"    - {d}", file=sys.stderr)
            print("=" * 65, file=sys.stderr)
        sys.exit(1)

    except CompilationError as ce:
        if args.json:
            out = {
                "success": False,
                "status": "FAILED",
                "exit_code": 2,
                "stage": ce.stage,
                "target": args.target,
                "error": ce.message,
                "diagnostics": ce.diagnostics
            }
            print(json.dumps(out, indent=2))
        else:
            print_banner("BUILD GUARD ABORT: FAIL-CLOSED (EXIT CODE 2)", file=sys.stderr)
            print(f"  Stage:       {ce.stage}", file=sys.stderr)
            print(f"  Error:       {ce.message}", file=sys.stderr)
            if ce.diagnostics:
                print("  Diagnostics:", file=sys.stderr)
                for d in ce.diagnostics[:15]:
                    print(f"    - {d}", file=sys.stderr)
            print("  FAIL-CLOSED: Los binarios antiguos fueron eliminados.", file=sys.stderr)
            print("  Se bloquea el paso a pruebas para impedir ejecución de código obsoleto.", file=sys.stderr)
            print("=" * 65, file=sys.stderr)
        sys.exit(2)

    except ManifestVerificationError as me:
        if args.json:
            out = {
                "success": False,
                "status": "FAILED",
                "exit_code": 3,
                "stage": me.stage,
                "target": args.target,
                "error": me.message,
                "diagnostics": me.diagnostics
            }
            print(json.dumps(out, indent=2))
        else:
            print_banner("BUILD GUARD MANIFEST ERROR (EXIT CODE 3)", file=sys.stderr)
            print(f"  Error:       {me.message}", file=sys.stderr)
            print("=" * 65, file=sys.stderr)
        sys.exit(3)

    except ProcessLockError as pe:
        if args.json:
            out = {
                "success": False,
                "status": "FAILED",
                "exit_code": 4,
                "stage": pe.stage,
                "target": args.target,
                "error": pe.message,
                "diagnostics": pe.diagnostics
            }
            print(json.dumps(out, indent=2))
        else:
            print_banner("BUILD GUARD PROCESS LOCK ERROR (EXIT CODE 4)", file=sys.stderr)
            print(f"  Error:       {pe.message}", file=sys.stderr)
            print("=" * 65, file=sys.stderr)
        sys.exit(4)

    except Exception as ex:
        if args.json:
            out = {
                "success": False,
                "status": "FAILED",
                "exit_code": 2,
                "stage": "unexpected_error",
                "target": args.target,
                "error": str(ex)
            }
            print(json.dumps(out, indent=2))
        else:
            print_banner("BUILD GUARD UNEXPECTED ERROR (EXIT CODE 2)", file=sys.stderr)
            print(f"  Error:       {ex}", file=sys.stderr)
            print("=" * 65, file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
