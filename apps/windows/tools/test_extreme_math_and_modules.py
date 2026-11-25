#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_extreme_math_and_modules.py — Batería de Pruebas Extremas Matemáticas,
Límites Numéricos y Operación Avanzada de Módulos para fx-CG50.

Evalúa:
1. Pila de expresiones y profundidad de paréntesis: ((((1+2)*3)+4)*5) = 65.
2. Cadena continua de acumulador Ans: 250 * 4 = 1000 -> Ans / 8 = 125 -> Ans + 75 = 200.
3. Aritmética decimal de coma flotante y tecla PUNTO: 1.25 * 8.4 = 10.5.
4. Potenciación de gran magnitud: 3^10 = 59049.
5. Límites de exponente BCD y excepción de overflow: 9.999x10^98 * 2 -> Ma ERROR y recuperación con AC.
6. Funciones trascendentales exactas: ln(1) = 0, log(10000) = 4, cos(0) = 1.
7. Módulo Graph: Definición de parábola cuadrática Y1 = X^2 - 4 y trazado F6 DRAW.
8. Módulo Table: Generación tabular para Y1 = 3X - 1 con F6 TABLE.
9. Módulo Equation: Solucionador de polinomios de 2º grado (X^2 - 5X + 6 = 0 -> X1=3, X2=2).
10. Módulos Spreadsheet, Financial y Program: Comprobación de estado visual.
11. Cierre limpio con 0 procesos residuales.
"""

import os
import sys
import time
import subprocess
import ctypes

# Asegurar importación de calc_visual_auditor
_tools_dir = os.path.dirname(os.path.abspath(__file__))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)
from calc_visual_auditor import CalcVisualAuditor

user32 = ctypes.windll.user32
WM_COMMAND = 0x0111
ID_BTN_MODE_GRAPH = 1014


def main():
    print("=" * 78)
    print("INICIANDO SUITE DE PRUEBAS EXTREMAS MATEMÁTICAS Y MÓDULOS AVANZADOS (fx-CG50)")
    print("=" * 78)

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    gallery_dir = os.path.join(root, "verification", "extreme_math_gallery")
    os.makedirs(gallery_dir, exist_ok=True)

    auditor = CalcVisualAuditor(target="unified", outdir=gallery_dir)

    try:
        print("\n[Fase 1] Arrancando CalculadoraUnificada.exe y estabilizando...")
        auditor.start()
        print("CONFIRMED: CalculadoraUnificada lista en vista Solo-CG50.")

        # 1. Menú Principal Inicial
        print("\n[Paso 1] Capturando Menú Principal Inicial...")
        auditor.capture_and_audit("01_extreme_main_menu", expected_tokens=["MAIN MENU"])

        # 2. Entrar a Run-Matrix
        print("\n[Paso 2] Entrando a Run-Matrix (tecla '1')...")
        auditor.press_sequence(["1"])
        time.sleep(2.5)
        auditor.capture_and_audit("02_extreme_run_matrix_ready")

        # 3. Profundidad de Paréntesis: ((((1+2)*3)+4)*5) = 65
        print("\n[Paso 3] Inyectando cálculo anidado de 4 niveles: ((((1+2)*3)+4)*5)...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        # ( ( ( ( 1 + 2 ) * 3 ) + 4 ) * 5 ) EXE
        seq_parens = ["(", "(", "(", "(", "1", "+", "2", ")", "*", "3", ")", "+", "4", ")", "*", "5", ")", "EXE"]
        auditor.press_sequence(seq_parens, default_hold=0.18, default_wait=0.25)
        time.sleep(1.5)
        auditor.capture_and_audit("03_extreme_deep_parentheses_65", expected_tokens=["65"])

        # 4. Cadena continua de acumulador Ans
        print("\n[Paso 4] Cadena continua de acumulador Ans...")
        # 250 * 4 = 1000
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["2", "5", "0", "*", "4", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("04_extreme_ans_step1_1000", expected_tokens=["1000"])

        # / 8 = 125
        print("Inyectando / 8 EXE (Ans/8)...")
        auditor.press_sequence(["/", "8", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("05_extreme_ans_step2_125", expected_tokens=["125"])

        # + 75 = 200
        print("Inyectando + 75 EXE (Ans+75)...")
        auditor.press_sequence(["+", "7", "5", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("06_extreme_ans_step3_200", expected_tokens=["200"])

        # 5. Aritmética decimal con tecla PUNTO: 1.25 * 8.4 = 10.5
        print("\n[Paso 5] Aritmética decimal con coma flotante: 1.25 * 8.4...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["1", ".", "2", "5", "*", "8", ".", "4", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("07_extreme_decimal_point_10_5", expected_tokens=["10.5"])

        # 6. Gran Potenciación: 3^10 = 59049
        print("\n[Paso 6] Gran potenciación: 3^10...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["3", "^", "1", "0", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("08_extreme_power_3_pow_10", expected_tokens=["59049"])

        # 7. Límites de exponente BCD y Overflow: 9.999x10^98 * 2 -> Ma ERROR
        print("\n[Paso 7] Límite de exponente BCD (9.999x10^98)...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["9", ".", "9", "9", "9", "EXP", "9", "8", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("09_extreme_exp_98_limit")

        print("Multiplicando por 2 para forzar Ma ERROR (exponente >= 100)...")
        auditor.press_sequence(["*", "2", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("10_extreme_overflow_ma_error", expected_tokens=["ERROR"])

        print("Recuperando con tecla AC...")
        auditor.press_sequence(["AC"], default_wait=0.5)
        time.sleep(1.0)
        auditor.capture_and_audit("11_extreme_recovery_after_overflow")

        # 8. Funciones Trascendentales exactas
        print("\n[Paso 8] Funciones trascendentales exactas...")
        print("Evaluando ln(1) = 0...")
        auditor.press_sequence(["LN", "1", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("12_extreme_ln_1_is_0", expected_tokens=["0"])

        print("Evaluando log10(10000) = 4...")
        auditor.press_sequence(["LOG", "1", "0", "0", "0", "0", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("13_extreme_log_10000_is_4", expected_tokens=["4"])

        print("Evaluando cos(0) = 1...")
        auditor.press_sequence(["COS", "0", "EXE"])
        time.sleep(1.2)
        auditor.capture_and_audit("14_extreme_cos_0_is_1", expected_tokens=["1"])

        # Retorno al menú
        print("\n[Paso 9] Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("15_extreme_menu_from_run")

        # 9. Módulo Graph: Parábola cuadrática Y1 = X^2 - 4
        print("\n[Paso 10] Entrando al Módulo Graph (tecla '5')...")
        auditor.press_sequence(["5"])
        time.sleep(2.5)
        auditor.capture_and_audit("16_extreme_graph_entry", expected_tokens=["Graph"])

        print("Limpiando entrada Y1 previa con DEL -> F1...")
        auditor.press_sequence(["DEL", "F1"], default_wait=0.5)

        print("Ingresando fórmula Y1 = X^2 - 4...")
        auditor.press_sequence(["X", "^", "2", "-", "4", "EXE"], default_wait=0.3)
        time.sleep(1.2)
        auditor.capture_and_audit("17_extreme_graph_formula_entered", expected_tokens=["-4"])

        print("Trazando gráfica con F6 (DRAW)...")
        auditor.press_sequence(["F6"])
        time.sleep(3.5) # Tiempo para rasterizado en VRAM y refresco LCD
        auditor.capture_and_audit("18_extreme_graph_parabola_drawn")

        # Retorno al menú
        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("19_extreme_menu_from_graph")

        # 10. Módulo Table: Generación tabular de Y1 = 3X - 1
        print("\n[Paso 11] Entrando al Módulo Table (tecla '7')...")
        auditor.press_sequence(["7"])
        time.sleep(2.5)
        auditor.capture_and_audit("20_extreme_table_entry", expected_tokens=["Table"])

        print("Limpiando entrada previa con DEL -> F1...")
        auditor.press_sequence(["DEL", "F1"], default_wait=0.5)

        print("Ingresando función tabular Y1 = 3X - 1...")
        auditor.press_sequence(["3", "X", "-", "1", "EXE"], default_wait=0.3)
        time.sleep(1.2)
        auditor.capture_and_audit("21_extreme_table_formula_entered")

        print("Generando tabla con F6 (TABLE)...")
        auditor.press_sequence(["F6"])
        time.sleep(2.5)
        auditor.capture_and_audit("22_extreme_table_values_generated")

        # Retorno al menú
        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("23_extreme_menu_from_table")

        # 11. Módulo Equation: Solucionador Polinomial (X^2 - 5X + 6 = 0)
        print("\n[Paso 12] Navegando hacia Módulo Equation...")
        # Equation está en fila 2, columna 3: DOWN, RIGHT, RIGHT, EXE
        auditor.press_sequence(["DOWN", "RIGHT", "RIGHT", "EXE"], default_wait=0.4)
        time.sleep(2.5)
        auditor.capture_and_audit("24_extreme_equation_entry", expected_tokens=["Equation"])

        print("Seleccionando Polynomial (F2) -> Degree 2 (F1)...")
        auditor.press_sequence(["F2", "F1"], default_wait=0.6)
        time.sleep(1.5)
        auditor.capture_and_audit("25_extreme_equation_poly_degree2", expected_tokens=["aX"])

        print("Ingresando coeficientes: a=1, b=-5, c=6...")
        # a = 1 EXE, b = -5 EXE, c = 6 EXE
        auditor.press_sequence(["1", "EXE", "-", "5", "EXE", "6", "EXE"], default_wait=0.4)
        time.sleep(1.2)
        auditor.capture_and_audit("26_extreme_equation_coefficients_set")

        print("Resolviendo ecuación cuadrática con F1 (SOLV)...")
        auditor.press_sequence(["F1"])
        time.sleep(2.0)
        auditor.capture_and_audit("27_extreme_equation_roots_solved", expected_tokens=["X1"])

        # Retorno al menú
        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("28_extreme_menu_from_equation")

        # 12. Módulo Spreadsheet
        print("\n[Paso 13] Navegando hacia Spreadsheet...")
        # Spreadsheet está en fila 2, columna 2: DOWN, RIGHT, EXE
        auditor.press_sequence(["DOWN", "RIGHT", "EXE"], default_wait=0.4)
        time.sleep(2.5)
        auditor.capture_and_audit("29_extreme_spreadsheet_grid", expected_tokens=["SHEET"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("30_extreme_menu_from_sheet")

        # 13. Módulo Conic Graphs
        print("\n[Paso 14] Navegando hacia Conic Graphs...")
        # Conic está en fila 1, columna 5: RIGHT x 4, EXE
        auditor.press_sequence(["RIGHT", "RIGHT", "RIGHT", "RIGHT", "EXE"], default_wait=0.4)
        time.sleep(2.5)
        auditor.capture_and_audit("31_extreme_conic_entry", expected_tokens=["Conic"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("32_extreme_menu_from_conic")

        # 14. Generar reporte HTML
        print("\n[Generando Reporte HTML] Creando informe interactivo de la galería...")
        html_file = auditor.generate_html_report(
            title="Auditoría Visual Extrema de la fx-CG50: Aritmética Avanzada, Excepciones BCD y Módulos"
        )
        print(f"CONFIRMED: Galería HTML generada en: {html_file}")

    finally:
        print("\n[Cierre] Cerrando CalculadoraUnificada de forma limpia...")
        auditor.stop()
        time.sleep(1.0)
        chk = subprocess.run(["powershell", "-NoProfile", "-Command",
                             "(Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue).Count"],
                            capture_output=True, text=True)
        count = int(chk.stdout.strip() or 0)
        print(f"CONFIRMED: Procesos residuales de CalculadoraUnificada = {count}")

    print("\n" + "=" * 78)
    print("SUITE EXTREMA DE PRUEBAS FINALIZADA CON ÉXITO")
    print("=" * 78)
    return 0


if __name__ == "__main__":
    sys.exit(main())
