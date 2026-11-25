#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_differential_official.py — Auditoría Visual y de Operaciones en el Emulador Oficial de CASIO.

Ejecuta exactamente la misma batería de pruebas en el emulador oficial
(fx-CG Manager PLUS Subscription for fx-CG50series.exe) para servir como Oráculo Diferencial:
1. Menú Principal Inicial.
2. Módulo Run-Matrix:
   - Aritmética compleja: (12345 + 67890) * 2 / 5 = 32094
   - Potencias: 2 ^ 10 = 1024
   - Multiplicación gigante: 987654321 * 123456789
   - Trigonometría: cos(0) = 1
   - Logaritmos: log(1000) = 3
   - Precisión decimal BCD: 1 / 7 = 0.1428571429
   - Excepción matemática: 1 / 0 -> Ma ERROR -> Recuperación con AC
3. Módulos Statistics, Graph y Table.
4. Generación de galería HTML del Oráculo Oficial en verification/official_manager_gallery/.
"""

import os
import sys
import time
import subprocess
from calc_visual_auditor import CalcVisualAuditor

def main():
    root = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial"
    gallery_dir = os.path.join(root, "verification", "official_manager_gallery")
    os.makedirs(gallery_dir, exist_ok=True)

    print("==========================================================================")
    print("  ORÁCULO DIFERENCIAL: AUDITORÍA DEL EMULADOR OFICIAL DE CASIO")
    print("==========================================================================")

    auditor = CalcVisualAuditor(target="official", outdir=gallery_dir)

    try:
        print("\n[Iniciando] Arrancando fx-CG Manager PLUS oficial...")
        auditor.start()
        print("CONFIRMED: Emulador oficial iniciado y estabilizado.")

        # 1. Menú Principal Inicial
        print("\n--- [Paso 1] Menú Principal Oficial ---")
        auditor.capture_and_audit("01_oficial_main_menu_inicial", expected_tokens=["MAIN MENU"])

        # 2. Run-Matrix
        print("\n--- [Paso 2] Módulo Run-Matrix Oficial ---")
        auditor.press_sequence(["1"])
        time.sleep(3.0)
        auditor.capture_and_audit("02_oficial_runmat_ready")

        print("Inyectando cálculo 1: (12345 + 67890) * 2 / 5 EXE...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["(", "1", "2", "3", "4", "5", "+", "6", "7", "8", "9", "0", ")",
                                "*", "2", "/", "5", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("03_oficial_runmat_aritmetica_compleja", expected_tokens=["32094"])

        print("Inyectando cálculo 2: 2 ^ 10 = 1024...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["2", "^", "1", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("04_oficial_runmat_potencia_2a10", expected_tokens=["1024"])

        print("Inyectando cálculo 3: Notación científica y números gigantes (987654321 * 123456789)...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["9", "8", "7", "6", "5", "4", "3", "2", "1", "*",
                                "1", "2", "3", "4", "5", "6", "7", "8", "9", "EXE"], default_hold=0.18, default_wait=0.22)
        time.sleep(0.5)
        auditor.capture_and_audit("05_oficial_runmat_multiplicacion_gigante")

        print("Inyectando cálculo 4: Funciones trigonométricas cos(0)...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["COS", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("06_oficial_runmat_trigonometria_cos0", expected_tokens=["1"])

        print("Inyectando cálculo 5: Logaritmo log(1000)...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["LOG", "1", "0", "0", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("07_oficial_runmat_logaritmo_log1000", expected_tokens=["3"])

        print("Inyectando cálculo 6 (Precisión decimal): 1 / 7 EXE...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["1", "/", "7", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("08_oficial_runmat_precision_1_sobre_7", expected_tokens=["0.142857"])

        print("Inyectando cálculo 7 (Excepción / División por cero): 1 / 0 EXE...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["1", "/", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("09_oficial_runmat_error_division_por_cero", expected_tokens=["ERROR"])

        print("Recuperando con AC...")
        auditor.press_sequence(["AC"], default_wait=0.5)

        print("Retornando al Menú Principal...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("10_oficial_menu_tras_runmat", expected_tokens=["MAIN MENU"])

        # 3. Statistics
        print("\n--- [Paso 3] Módulo Statistics Oficial ---")
        auditor.press_sequence(["2"])
        time.sleep(2.5)
        auditor.capture_and_audit("11_oficial_statistics_listas", expected_tokens=["List"])
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("12_oficial_menu_tras_stat", expected_tokens=["MAIN MENU"])

        # 4. Graph
        print("\n--- [Paso 4] Módulo Graph Oficial ---")
        auditor.press_sequence(["5"])
        time.sleep(2.5)
        auditor.capture_and_audit("13_oficial_graph_func_entry", expected_tokens=["Func"])
        auditor.press_sequence(["X", "-", "2", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(1.0)
        auditor.capture_and_audit("14_oficial_graph_func_definida")
        auditor.press_sequence(["F6"], default_wait=3.0)
        time.sleep(2.0)
        auditor.capture_and_audit("15_oficial_graph_plot_trazado")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("16_oficial_menu_tras_graph", expected_tokens=["MAIN MENU"])

        # Reporte
        print("\n[Generando Reporte HTML del Oráculo Oficial]")
        html_file = auditor.generate_html_report(title="Auditoría Visual del Emulador Oficial Casio (Oráculo)")
        print(f"CONFIRMED: Reporte HTML oficial generado en: {html_file}")

    finally:
        print("\n[Cierre] Cerrando emulador oficial...")
        auditor.stop()
        time.sleep(1.0)
        chk = subprocess.run(["powershell", "-NoProfile", "-Command",
                              "Get-Process fx-CG_Manager* -ErrorAction SilentlyContinue | Measure-Object | Select-Object -ExpandProperty Count"],
                             capture_output=True, text=True)
        count = int(chk.stdout.strip() or 0)
        print(f"CONFIRMED: Procesos oficiales residuales = {count}")

    return 0

if __name__ == "__main__":
    sys.exit(main())
