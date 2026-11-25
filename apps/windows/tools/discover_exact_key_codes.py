#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
discover_exact_key_codes.py — Descubre empíricamente la función exacta de cada
uno de los 50 botones del emulador oficial Casio mediante inyección y OCR/píxeles.
"""

import ctypes
import ctypes.wintypes as wt
import subprocess
import time
import os
import sys

_tools_dir = os.path.dirname(os.path.abspath(__file__))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)
from calc_visual_auditor import CalcVisualAuditor

user32 = ctypes.windll.user32
BM_CLICK = 0x00F5


def main():
    print("=" * 78)
    print("DESCUBRIMIENTO EMPÍRICO DE LOS 50 BOTONES DEL EMULADOR OFICIAL CASIO")
    print("=" * 78)

    auditor = CalcVisualAuditor(target="official", outdir="verification/key_discovery")
    auditor.start()

    try:
        # Enter Run-Matrix with '73' (1)
        print("Entrando a Run-Matrix...")
        auditor.press_key("1")
        time.sleep(2.5)
        auditor.press_key("AC")
        time.sleep(0.5)

        # List all buttons discovered
        buttons = auditor.official_buttons
        print(f"Total botones encontrados: {len(buttons)}")
        sorted_codes = sorted(buttons.keys())
        print("Códigos de botón:", sorted_codes)

        key_map_results = {}

        for code in sorted_codes:
            # Clear line first
            auditor.press_key("AC")
            time.sleep(0.2)

            # Click button
            btn = buttons[code]
            user32.SendMessageW(btn, BM_CLICK, 0, 0)
            time.sleep(0.35)

            # Capture and OCR
            rec = auditor.capture_and_audit(f"btn_{code}")
            ocr_clean = rec["ocr_text"].replace("\n", " ").strip()
            # Remove prompt noise
            for noise in [">MAT/VCT", "MATH", "ET", ">MAT/VC"]:
                ocr_clean = ocr_clean.replace(noise, "")
            ocr_clean = ocr_clean.strip()

            key_map_results[code] = ocr_clean
            print(f"Button '{code}': OCR = '{ocr_clean}'")

        # Save results to JSON
        import json
        with open("verification/key_discovery/official_key_map_empirical.json", "w", encoding="utf-8") as f:
            json.dump(key_map_results, f, indent=2, ensure_ascii=False)
        print("\nMapeo empírico guardado en official_key_map_empirical.json")

    finally:
        auditor.stop()


if __name__ == "__main__":
    main()
