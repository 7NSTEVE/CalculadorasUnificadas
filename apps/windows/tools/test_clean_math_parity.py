#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_clean_math_parity.py — Verificación Robusta de Aritmética Avanzada,
Paréntesis Anidados, Ans, Coma Flotante y Recuperación de Excepciones.
"""

import os
import sys
import time
import subprocess
import ctypes

_tools_dir = os.path.dirname(os.path.abspath(__file__))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)
from calc_visual_auditor import CalcVisualAuditor


def main():
    print("=" * 78)
    print("VERIFICACIÓN ROBUSTA DE ARITMÉTICA AVANZADA Y PRECISIÓN (fx-CG50)")
    print("=" * 78)

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    gallery_dir = os.path.join(root, "verification", "clean_math_gallery")
    os.makedirs(gallery_dir, exist_ok=True)

    auditor = CalcVisualAuditor(target="unified", outdir=gallery_dir)

    try:
        print("\n[Arranque] Iniciando CalculadoraUnificada...")
        auditor.start()

        # 1. Menú Principal
        auditor.capture_and_audit("01_menu", expected_tokens=["MAIN MENU"])

        # 2. Run-Matrix
        print("Entrando a Run-Matrix...")
        auditor.press_sequence(["1"])
        time.sleep(2.5)
        auditor.press_sequence(["EXIT", "AC"], default_wait=0.4)

        # 3. Paréntesis anidados con operadores intercalados: ((12 + 34) * (56 - 78)) / 2 = -506
        print("Prueba 1: ((12 + 34) * (56 - 78)) / 2 = -506...")
        seq1 = ["(", "(", "1", "2", "+", "3", "4", ")", "*", "(", "5", "6", "-", "7", "8", ")", ")", "/", "2", "EXE"]
        auditor.press_sequence(seq1, default_hold=0.18, default_wait=0.30)
        time.sleep(1.5)
        r1 = auditor.capture_and_audit("02_nested_parens_506", expected_tokens=["506"])
        print(f"Resultado 1: Passed={r1['passed']}, OCR={r1['ocr_text'].strip()[:40]}")

        # 4. Coma flotante con punto decimal: 1.5 * 2.4 = 3.6
        print("\nPrueba 2: Coma flotante 1.5 * 2.4 = 3.6...")
        auditor.press_sequence(["EXIT", "AC"], default_wait=0.4)
        seq2 = ["1", ".", "5", "*", "2", ".", "4", "EXE"]
        auditor.press_sequence(seq2, default_hold=0.18, default_wait=0.25)
        time.sleep(1.5)
        r2 = auditor.capture_and_audit("03_decimal_float_3_6", expected_tokens=["3.6"])
        print(f"Resultado 2: Passed={r2['passed']}, OCR={r2['ocr_text'].strip()[:40]}")

        # 5. Potenciación: 3 ^ 5 = 243
        print("\nPrueba 3: Potenciación 3 ^ 5 = 243...")
        auditor.press_sequence(["EXIT", "AC"], default_wait=0.4)
        seq3 = ["3", "^", "5", "EXE"]
        auditor.press_sequence(seq3, default_hold=0.18, default_wait=0.25)
        time.sleep(1.5)
        r3 = auditor.capture_and_audit("04_power_3_pow_5", expected_tokens=["243"])
        print(f"Resultado 3: Passed={r3['passed']}, OCR={r3['ocr_text'].strip()[:40]}")

        # 6. Registro acumulador Ans: Ans * 2 = 486
        print("\nPrueba 4: Ans * 2 = 486...")
        seq4 = ["*", "2", "EXE"]
        auditor.press_sequence(seq4, default_hold=0.18, default_wait=0.25)
        time.sleep(1.5)
        r4 = auditor.capture_and_audit("05_ans_times_2", expected_tokens=["486"])
        print(f"Resultado 4: Passed={r4['passed']}, OCR={r4['ocr_text'].strip()[:40]}")

        # 7. Funciones trascendentales: ln(1) = 0 y cos(0) = 1
        print("\nPrueba 5: ln(1) = 0...")
        auditor.press_sequence(["EXIT", "AC"], default_wait=0.4)
        auditor.press_sequence(["LN", "1", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(1.5)
        r5 = auditor.capture_and_audit("06_ln_1_is_0", expected_tokens=["0"])
        print(f"Resultado 5: Passed={r5['passed']}, OCR={r5['ocr_text'].strip()[:40]}")

        print("\nPrueba 6: cos(0) = 1...")
        auditor.press_sequence(["EXIT", "AC"], default_wait=0.4)
        auditor.press_sequence(["COS", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(1.5)
        r6 = auditor.capture_and_audit("07_cos_0_is_1", expected_tokens=["1"])
        print(f"Resultado 6: Passed={r6['passed']}, OCR={r6['ocr_text'].strip()[:40]}")

        # 8. Excepción Ma ERROR con 1 / 0 y recuperación con EXIT
        print("\nPrueba 7: División por cero 1 / 0 -> Ma ERROR...")
        auditor.press_sequence(["EXIT", "AC"], default_wait=0.4)
        auditor.press_sequence(["1", "/", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(1.5)
        r7 = auditor.capture_and_audit("08_div_by_zero_error", expected_tokens=["ERROR"])
        print(f"Resultado 7: Passed={r7['passed']}, OCR={r7['ocr_text'].strip()[:40]}")

        print("Recuperando con tecla EXIT...")
        auditor.press_sequence(["EXIT"], default_wait=0.5)
        time.sleep(1.0)
        auditor.press_sequence(["AC"], default_wait=0.5)
        r8 = auditor.capture_and_audit("09_clean_recovery")
        print(f"Resultado 8: Passed={r8['passed']}")

        # 9. Retorno al Menú
        print("\nRetorno al Menú Principal...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        r9 = auditor.capture_and_audit("10_menu_restored", expected_tokens=["MAIN MENU"])
        print(f"Resultado 9: Passed={r9['passed']}")

        auditor.generate_html_report(title="Verificación Robusta de Aritmética y Precisión")

    finally:
        auditor.stop()
        time.sleep(1.0)
        chk = subprocess.run(["powershell", "-NoProfile", "-Command",
                             "(Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue).Count"],
                            capture_output=True, text=True)
        print(f"Procesos residuales = {chk.stdout.strip()}")


if __name__ == "__main__":
    sys.exit(main())
