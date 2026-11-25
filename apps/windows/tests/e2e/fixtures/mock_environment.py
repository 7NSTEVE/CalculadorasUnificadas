#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mock_environment.py — Test environment, mock runner, and schema validation fixtures.
Provides:
- Isolated temporary directories for builds, logs, and artifacts.
- Deterministic simulation of platform CLI invocations.
- Structured JSON schema validation against report.md specifications.
- Matrix key code and official button code lookup verification.
"""

import hashlib
import json
import os
import shutil
import tempfile
import time
from typing import Dict, Any, List, Optional, Tuple


# Key codes according to cg50_keys_exact.h and calc_visual_auditor.py
EXACT_MATRIX_KEYS = {
    "0": 0x0701, "1": 0x0702, "2": 0x0602, "3": 0x0502, "4": 0x0703,
    "5": 0x0603, "6": 0x0503, "7": 0x0704, "8": 0x0604, "9": 0x0504,
    "+": 0x0402, "-": 0x0302, "*": 0x0403, "/": 0x0303, ".": 0x0601,
    "EXE": 0x0301, "AC": 0x0101, "DEL": 0x0404,
    "MENU": 0x0409, "EXIT": 0x0408, "SHIFT": 0x0709, "ALPHA": 0x0708,
    "OPTN": 0x0609, "VARS": 0x0509,
    "UP": 0x0208, "DOWN": 0x0308, "LEFT": 0x0309, "RIGHT": 0x0207,
    "F1": 0x070A, "F2": 0x060A, "F3": 0x050A, "F4": 0x040A, "F5": 0x030A, "F6": 0x020A,
    "(": 0x0506, ")": 0x0406, "^": 0x0508
}

OFFICIAL_BUTTON_CODES = {
    "0": "72", "1": "73", "2": "63", "3": "53", "4": "74",
    "5": "64", "6": "54", "7": "75", "8": "65", "9": "55",
    "+": "43", "-": "33", "*": "44", "/": "34", ".": "62",
    "EXE": "32", "AC": "11", "DEL": "45",
    "MENU": "49", "EXIT": "48", "SHIFT": "79", "ALPHA": "78",
    "OPTN": "69", "VARS": "59",
    "UP": "29", "DOWN": "38", "LEFT": "39", "RIGHT": "28",
    "F1": "7a", "F2": "6a", "F3": "5a", "F4": "4a", "F5": "3a", "F6": "2a",
    "(": "56", ")": "46", "^": "58"
}


class MockPlatformEnvironment:
    """Manages isolated workspace and simulates platform commands."""

    def __init__(self):
        self.temp_dir = tempfile.mkdtemp(prefix="cg50_e2e_")
        self.build_dir = os.path.join(self.temp_dir, "build-verify")
        self.release_dir = os.path.join(self.build_dir, "Release")
        self.out_dir = os.path.join(self.temp_dir, "diff_out")
        os.makedirs(self.release_dir, exist_ok=True)
        os.makedirs(self.out_dir, exist_ok=True)

    def cleanup(self):
        """Clean up temporary directory."""
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def create_mock_binary(self, name: str = "CalculadoraUnificada.exe", content: bytes = b"MZ_MOCK_PE") -> str:
        """Create a mock executable binary."""
        path = os.path.join(self.release_dir, name)
        with open(path, "wb") as f:
            f.write(content)
        return path

    def create_build_manifest(self, binary_path: str, compiler: str = "MSVC 14.44.35207",
                              duration: float = 12.5) -> str:
        """Create a valid build_manifest.json with real SHA-256."""
        h = hashlib.sha256()
        with open(binary_path, "rb") as f:
            h.update(f.read())
        sha = h.hexdigest()

        manifest = {
            "status": "PASS",
            "compiler": compiler,
            "target": os.path.basename(binary_path),
            "binary_path": binary_path,
            "binary_sha256": sha,
            "timestamp": time.time(),
            "iso_timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "duration_seconds": duration
        }
        manifest_path = os.path.join(self.build_dir, "build_manifest.json")
        with open(manifest_path, "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2)
        return manifest_path

    def simulate_dual_diff_run(self, keys: str, pass_verdict: bool = True,
                               diverge_at_step: Optional[int] = None) -> Tuple[int, Dict[str, Any]]:
        """
        Simulate the execution and output of `cg50_agent diff` according to report.md schema.
        Returns: (exit_code, json_report)
        """
        key_list = [k.strip() for k in keys.split(",") if k.strip()]
        steps = []
        verdict = "PASS" if pass_verdict else "DIVERGENCIA"
        first_div = None

        for idx, key in enumerate(key_list):
            step_num = idx + 1
            is_divergent = (diverge_at_step is not None and step_num == diverge_at_step)

            step_data = {
                "step": step_num,
                "key": key,
                "native": {
                    "pc": "0x0801382A" if not is_divergent else "0x080206FC",
                    "inst": "214140110",
                    "cyc": "226799261",
                    "state": "2",
                    "lcd_sha256": f"SHA_{step_num}_NATIVE",
                    "ocr_text": key,
                    "class": "texto_denso"
                },
                "official": {
                    "pc": "0x0801382A" if not is_divergent else "0x08020710",
                    "lcd_sha256": f"SHA_{step_num}_OFFICIAL" if not is_divergent else f"SHA_{step_num}_DIV",
                    "ocr_text": key,
                    "class": "texto_denso"
                },
                "comparison": {
                    "status": "MATCH" if not is_divergent else "DIFF",
                    "pixel_diff": {
                        "changed_pixels": 0 if not is_divergent else 1420,
                        "changed_percent": 0.0 if not is_divergent else 1.60,
                        "diff_bbox": None if not is_divergent else [180, 45, 320, 80],
                        "diff_png": None if not is_divergent else f"step{step_num}_diff.png"
                    },
                    "state_diff": {
                        "pc_match": not is_divergent,
                        "ocr_match": not is_divergent
                    }
                }
            }
            steps.append(step_data)

            if is_divergent and first_div is None:
                first_div = {
                    "step": step_num,
                    "key": key,
                    "divergence_type": "LCD_PIXEL_MISMATCH",
                    "details": "Pixel mismatch exceeding tolerance (1420 pixels changed, 1.60%)",
                    "diff_bbox": [180, 45, 320, 80],
                    "changed_pixels": 1420,
                    "changed_percent": 1.60,
                    "diff_image": f"step{step_num}_diff_heatmap.png",
                    "native_pc": "0x080206FC",
                    "official_pc": "0x08020710"
                }
                break  # Stop on first divergence (fail-fast)

        report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "build_control": {
                "status": "PASS",
                "compiler": "MSVC 14.44.35207",
                "target": "CalculadoraUnificada.exe",
                "duration_seconds": 12.5,
                "binary_path": os.path.join(self.release_dir, "CalculadoraUnificada.exe"),
                "binary_sha256": "3B8C112233445566778899AABBCCDDEEFF"
            },
            "verdict": verdict,
            "total_steps": len(key_list),
            "steps_executed": len(steps),
            "first_divergence": first_div,
            "steps": steps
        }

        exit_code = 0 if verdict == "PASS" else 1
        return exit_code, report


def validate_diff_report_schema(report: Dict[str, Any]) -> Tuple[bool, List[str]]:
    """
    Validate that a JSON report conforms strictly to the specification in report.md § 5.2.
    Returns (is_valid, error_list).
    """
    errors = []
    required_root = ["platform", "version", "timestamp", "build_control", "verdict",
                     "total_steps", "steps_executed", "first_divergence", "steps"]
    for field in required_root:
        if field not in report:
            errors.append(f"Missing root field: {field}")

    if "verdict" in report and report["verdict"] not in ("PASS", "FAIL", "DIVERGENCIA"):
        errors.append(f"Invalid verdict value: {report['verdict']}")

    if "build_control" in report:
        bc = report["build_control"]
        for bc_field in ["status", "compiler", "target", "binary_path", "binary_sha256"]:
            if bc_field not in bc:
                errors.append(f"Missing build_control field: {bc_field}")

    if "steps" in report and isinstance(report["steps"], list):
        for s in report["steps"]:
            for s_field in ["step", "key", "native", "official", "comparison"]:
                if s_field not in s:
                    errors.append(f"Step missing field: {s_field}")

    if report.get("verdict") == "DIVERGENCIA":
        if not report.get("first_divergence"):
            errors.append("Verdict is DIVERGENCIA but first_divergence is null/empty")
        else:
            fd = report["first_divergence"]
            for fd_field in ["step", "key", "divergence_type", "details"]:
                if fd_field not in fd:
                    errors.append(f"first_divergence missing field: {fd_field}")

    return len(errors) == 0, errors
