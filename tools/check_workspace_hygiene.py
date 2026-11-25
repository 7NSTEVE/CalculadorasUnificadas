#!/usr/bin/env python3
"""Workspace Hygiene Validator for CalculadorasUnificadas.

Enforces the 8 Permanent Anti-Chaos Rules documented in AGENTS.md:
  1. Root allowlist (zero stray files/folders in repo root).
  2. Zero forbidden backup/build/temp files (*.bak*, *.obj, *.pdb, *.tlog, *.exe, *.dll, etc.)
     tracked in Git or present outside build/ and private_local/.
  3. Single Source of Truth for native C++ cores (exact count = 1 for each core anchor file).
  4. git check-ignore verification on private_local/ and build/.
  5. Zero stray build-* / out_test / .agents directories inside the repo.

Usage:
  python tools/check_workspace_hygiene.py            # full workspace check
  python tools/check_workspace_hygiene.py --staged   # pre-commit check on staged files + root
"""

from __future__ import annotations

import argparse
import fnmatch
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

ALLOWED_ROOT_FILES = {
    "README.md",
    "AGENTS.md",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    ".gitignore",
    ".gitattributes",
    ".editorconfig",
    "CMakeLists.txt",
}

ALLOWED_ROOT_DIRS = {
    ".git",
    ".github",
    ".githooks",
    "core",
    "studio",
    "academic",
    "ui_glass",
    "apps",
    "assets",
    "docs",
    "tests",
    "tools",
    "private_local",
    "build",
}

FORBIDDEN_PATTERNS = [
    "*.bak",
    "*.bak_*",
    "*.orig",
    "*.obj",
    "*.pdb",
    "*.ilk",
    "*.tlog",
    "*.exe",
    "*.dll",
    "*.apk",
    "*.ipa",
    "dma_frame_*.raw",
    "vram*.bmp",
]

FORBIDDEN_DIRS = {
    "build-verify",
    "build-native-x64",
    "build-win32",
    "build-probe32",
    "build-studio-x64",
    "build-cuadernu-codex",
    "build-cuadernu-portable-codex",
    "out_test",
    ".agents",
}

SKIP_WALK_DIRS = {
    ".git",
    "build",
    "private_local",
    ".dart_tool",
    ".gradle",
    "node_modules",
    "__pycache__",
}

# Single Source of Truth anchors: each must exist at most ONCE (and ideally exactly ONCE)
UNIQUE_CORE_ANCHORS = {
    "cg50_native_core.cpp": "core/casio_cg50/cg50_native_core.cpp",
    "casio_cw2.cpp": "core/casio_cw2/src/casio_cw2.cpp",
    "calcemu.cpp": "core/casio_nx/src/calcemu.cpp",
    "casio_ffi.cpp": "core/casio_ffi/casio_ffi.cpp",
}


def run_git(args: list[str]) -> tuple[int, str]:
    r = subprocess.run(
        ["git", "-C", str(ROOT)] + args,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return r.returncode, r.stdout.strip()


def matches_forbidden(name: str) -> bool:
    low = name.lower()
    return any(fnmatch.fnmatch(low, pat.lower()) for pat in FORBIDDEN_PATTERNS)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--staged", action="store_true", help="Check staged Git paths + root rules")
    args = ap.parse_args()

    errors: list[str] = []

    # 1. Root allowlist check
    for entry in sorted(ROOT.iterdir(), key=lambda p: p.name.lower()):
        if entry.is_dir():
            if entry.name not in ALLOWED_ROOT_DIRS:
                errors.append(f"[ROOT_DIR_DISALLOWED] Unexpected root directory: {entry.name}/")
        else:
            if entry.name not in ALLOWED_ROOT_FILES:
                errors.append(f"[ROOT_FILE_DISALLOWED] Unexpected loose file in root: {entry.name}")

    # 2. git check-ignore verification on private_local/ and build/
    for ign in ("private_local/test.bin", "build/test.obj"):
        rc, _ = run_git(["check-ignore", "-q", ign])
        if rc != 0:
            errors.append(f"[IGNORE_MISSING] Git does NOT ignore '{ign}' (check .gitignore)")

    # 3. Check Git staged or tracked files for forbidden artifacts
    git_cmd = ["diff", "--cached", "--name-only"] if args.staged else ["ls-files"]
    rc, listed = run_git(git_cmd)
    if rc == 0:
        for rel in listed.splitlines():
            if not rel:
                continue
            parts = pathlib.PurePosixPath(rel).parts
            if parts and parts[0] in ("private_local", "build"):
                errors.append(f"[PRIVATE_OR_BUILD_TRACKED] '{rel}' must NEVER be tracked in Git")
            if matches_forbidden(parts[-1]):
                errors.append(f"[FORBIDDEN_TRACKED] Forbidden file tracked/staged in Git: {rel}")

    # 4. Full workspace walk (when not --staged)
    if not args.staged:
        anchor_hits: dict[str, list[str]] = {k: [] for k in UNIQUE_CORE_ANCHORS}
        for cur, dirs, files in os.walk(ROOT):
            rel_dir = os.path.relpath(cur, ROOT).replace("\\", "/")
            # Flag forbidden directories and prune ignored trees
            kept_dirs = []
            for d in dirs:
                if d in FORBIDDEN_DIRS:
                    bad = f"{rel_dir}/{d}" if rel_dir != "." else d
                    errors.append(f"[FORBIDDEN_DIR] Stray build/scratch directory outside build/: {bad}")
                    continue
                if d in SKIP_WALK_DIRS:
                    continue
                if rel_dir.startswith("apps/android/build") or rel_dir.startswith("apps/flutter/build"):
                    continue
                kept_dirs.append(d)
            dirs[:] = kept_dirs

            for f in files:
                rel_file = f"{rel_dir}/{f}" if rel_dir != "." else f
                if f in anchor_hits:
                    anchor_hits[f].append(rel_file)
                if matches_forbidden(f):
                    # Allow expected Flutter/Gradle outputs only under apps/*/build/ (already skipped)
                    errors.append(f"[FORBIDDEN_FILE] Forbidden backup/binary file in tree: {rel_file}")

        for fname, expected_rel in UNIQUE_CORE_ANCHORS.items():
            hits = anchor_hits[fname]
            if hits != [expected_rel]:
                errors.append(
                    f"[DUPLICATE_OR_MISSING_CORE] '{fname}' expected ONLY at '{expected_rel}', found: {hits}"
                )

    if errors:
        print(f"HYGIENE CHECK FAILED ({len(errors)} violation(s)):")
        for e in errors[:60]:
            print("  -", e)
        if len(errors) > 60:
            print(f"  ... +{len(errors) - 60} more")
        return 1

    print("HYGIENE CHECK PASSED (0 violations).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
