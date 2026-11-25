"""Opaque-box 4-tier E2E test suite for Calculadora Unificada & Document Studio."""
from __future__ import annotations

import sys
from pathlib import Path

_TESTS_PARENT = str(Path(__file__).resolve().parent.parent)
if _TESTS_PARENT not in sys.path:
    sys.path.insert(0, _TESTS_PARENT)
