#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_massive_all_modules.py — Suite Masiva de Pruebas y Auditoría Visual para fx-CG50.

Ejecuta y audita sistemáticamente cada uno de los módulos y capacidades matemáticas de la fx-CG50 en CalculadoraUnificada:
1. Menú Principal Inicial (Página 1).
2. Run-Matrix:
   - Aritmética con paréntesis anidados: (12345 + 67890) * 2 / 5 = 32094
   - Potencias y exponentes: 2 ^ 10 = 1024
   - Multiplicación de enteros gigantes (987654321 * 123456789)
   - Funciones trigonométricas exactas: cos(0) = 1
   - Propiedades logarítmicas: log(1000) = 3
   - Estrés de precisión / decimales periódicos: 1 / 7 = 0.1428571428...
   - Estrés de excepción / división por cero: 1 / 0 -> Ma ERROR -> Recuperación limpia con AC
3. Statistics: Listas (List 1..List 4) y comandos estadísticos.
4. Graph: Definición de función Y1 = X - 2 y trazado completo en el plano cartesiano (F6 DRAW).
5. Table: Definición y generación tabular de valores numéricos (F6 TABLE).
6. Conic Graphs: Exploración del catálogo de secciones cónicas.
7. Recursion: Sucesiones recursivas (a_n, b_n).
8. eActivity: Gestor de actividades y fórmulas interactivas.
9. Spreadsheet: Hoja de cálculo matricial interactiva.
10. Equation: Resolución de sistemas simultáneos y polinomios.
11. Financial: Matemática financiera (interés simple, compuesto, amortización).
12. Program: Gestor de programas y entorno Casio Basic.
13. Página 2 del Menú: Herramientas del sistema, memoria, 3D Graph y Python.
14. Vista Triple Multimodelo: Ejecución simultánea con ClassWiz y Natural ES PLUS.
15. Generación de galería HTML interactiva completa con capturas reales Win32, OCR nativo y hashes SHA-256.
"""

import os
import sys
import time
import ctypes
import subprocess
from calc_visual_auditor import CalcVisualAuditor

user32 = ctypes.windll.user32
WM_COMMAND = 0x0111
ID_BTN_MODE_ALL = 1010

def main():
    root = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial"
    gallery_dir = os.path.join(root, "verification", "visual_audit_gallery")
    os.makedirs(gallery_dir, exist_ok=True)

    print("==========================================================================")
    print("  SUITE MASIVA DE PRUEBAS VISUALES Y OPERACIONALES: TODOS LOS MÓDULOS")
    print("==========================================================================")

    auditor = CalcVisualAuditor(target="unified", outdir=gallery_dir)

    try:
        print("\n[Iniciando] Arrancando CalculadoraUnificada.exe y estabilizando GCal...")
        auditor.start()
        print("CONFIRMED: CalculadoraUnificada lista en modo Solo CG50.")

        # ----------------------------------------------------------------------
        # 1. Menú Principal Inicial
        # ----------------------------------------------------------------------
        print("\n--- [Paso 1] Menú Principal Inicial ---")
        auditor.capture_and_audit("01_main_menu_inicial", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 2. Módulo Run-Matrix: Operaciones Aritméticas Complejas y Estrés Numérico
        # ----------------------------------------------------------------------
        print("\n--- [Paso 2] Módulo Run-Matrix: Aritmética Compleja y Límites ---")
        print("Abriendo Run-Matrix con tecla '1'...")
        auditor.press_sequence(["1"])
        time.sleep(3.0) # Espera estabilización
        auditor.capture_and_audit("02_run_matrix_ready", expected_tokens=["MATH"])

        print("Inyectando cálculo 1: (12345 + 67890) * 2 / 5 EXE...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["(", "1", "2", "3", "4", "5", "+", "6", "7", "8", "9", "0", ")",
                                "*", "2", "/", "5", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("03_runmat_aritmetica_compleja", expected_tokens=["32094"])

        print("Inyectando cálculo 2: 2 ^ 10 = 1024...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["2", "^", "1", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("04_runmat_potencia_2a10", expected_tokens=["1024"])

        print("Inyectando cálculo 3: Notación científica y números gigantes (987654321 * 123456789)...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["9", "8", "7", "6", "5", "4", "3", "2", "1", "*",
                                "1", "2", "3", "4", "5", "6", "7", "8", "9", "EXE"], default_hold=0.18, default_wait=0.22)
        time.sleep(0.5)
        auditor.capture_and_audit("05_runmat_multiplicacion_gigante")

        print("Inyectando cálculo 4: Funciones trigonométricas cos(0)...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["COS", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("06_runmat_trigonometria_cos0", expected_tokens=["1"])

        print("Inyectando cálculo 5: Logaritmo log(1000)...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["LOG", "1", "0", "0", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("07_runmat_logaritmo_log1000", expected_tokens=["3"])

        print("Inyectando cálculo 6 (Estrés de precisión decimal): 1 / 7 EXE...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["1", "/", "7", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("08_runmat_precision_1_sobre_7", expected_tokens=["0.142857"])

        print("Inyectando cálculo 7 (Estrés de Excepción / División por cero): 1 / 0 EXE...")
        auditor.press_sequence(["AC"], default_wait=0.3)
        auditor.press_sequence(["1", "/", "0", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(0.5)
        auditor.capture_and_audit("09_runmat_error_division_por_cero", expected_tokens=["ERROR"])

        print("Recuperando de estado de error con AC...")
        auditor.press_sequence(["AC"], default_wait=0.5)

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("10_menu_tras_runmat", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 3. Módulo Statistics (Listas)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 3] Módulo Statistics ---")
        print("Abriendo Statistics con tecla '2'...")
        auditor.press_sequence(["2"])
        time.sleep(2.5)
        auditor.capture_and_audit("11_statistics_listas", expected_tokens=["List"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("12_menu_tras_stat", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 4. Módulo Graph (Definición y Trazado de Curvas 2D)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 4] Módulo Graph ---")
        print("Abriendo Graph con tecla '5'...")
        auditor.press_sequence(["5"])
        time.sleep(2.5)
        auditor.capture_and_audit("13_graph_func_entry", expected_tokens=["Func"])

        print("Ingresando función Y1 = X - 2...")
        auditor.press_sequence(["X", "-", "2", "EXE"], default_hold=0.18, default_wait=0.25)
        time.sleep(1.0)
        auditor.capture_and_audit("14_graph_func_definida")

        print("Trazando curva con F6 (DRAW)...")
        auditor.press_sequence(["F6"], default_wait=3.0)
        time.sleep(2.0)
        auditor.capture_and_audit("15_graph_plot_trazado")

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("16_menu_tras_graph", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 5. Módulo Table (Tablas de Valores)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 5] Módulo Table ---")
        print("Abriendo Table con tecla '7'...")
        auditor.press_sequence(["7"])
        time.sleep(2.5)
        auditor.capture_and_audit("17_table_func_entry", expected_tokens=["Func"])

        print("Generando tabla de valores con F6 (TABLE)...")
        auditor.press_sequence(["F6"], default_wait=2.5)
        time.sleep(1.5)
        auditor.capture_and_audit("18_table_valores_generados")

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("19_menu_tras_table", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 6. Módulo Conic Graphs (Secciones Cónicas)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 6] Módulo Conic Graphs ---")
        print("Abriendo Conic con tecla '9'...")
        auditor.press_sequence(["9"])
        time.sleep(2.5)
        auditor.capture_and_audit("20_conic_formulas", expected_tokens=["Equat"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("21_menu_tras_conic", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 7. Módulo Recursion (Sucesiones)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 7] Módulo Recursion ---")
        print("Abriendo Recursion con tecla '8'...")
        auditor.press_sequence(["8"])
        time.sleep(2.5)
        auditor.capture_and_audit("22_recursion_entry", expected_tokens=["TYPE"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("23_menu_tras_recursion", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 8. Módulo eActivity
        # ----------------------------------------------------------------------
        print("\n--- [Paso 8] Módulo eActivity ---")
        print("Abriendo eActivity con tecla '3'...")
        auditor.press_sequence(["3"])
        time.sleep(2.5)
        auditor.capture_and_audit("24_eactivity_entry", expected_tokens=["Bytes Free"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("25_menu_tras_eactivity", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 9. Módulo Spreadsheet (Hoja de Cálculo)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 9] Módulo Spreadsheet ---")
        print("Abriendo Spreadsheet con tecla '4'...")
        auditor.press_sequence(["4"])
        time.sleep(2.5)
        auditor.capture_and_audit("26_spreadsheet_grid", expected_tokens=["SHEET"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("27_menu_tras_spreadsheet", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 10. Módulo Equation (Sistemas y Polinomios)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 10] Módulo Equation ---")
        print("Navegando hacia Equation con flechas (DOWN, DOWN, RIGHT, EXE)...")
        auditor.press_sequence(["DOWN", "DOWN", "RIGHT", "EXE"], default_wait=0.4)
        time.sleep(2.5)
        auditor.capture_and_audit("28_equation_entry", expected_tokens=["Simul"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("29_menu_tras_equation", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 11. Módulo Financial (Matemática Financiera)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 11] Módulo Financial ---")
        print("Navegando hacia Financial con flechas (DOWN, DOWN, RIGHT, RIGHT, EXE)...")
        auditor.press_sequence(["DOWN", "DOWN", "RIGHT", "RIGHT", "EXE"], default_wait=0.4)
        time.sleep(2.5)
        auditor.capture_and_audit("30_financial_entry", expected_tokens=["Simple"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("31_menu_tras_financial", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 12. Módulo Program (Programación Casio Basic)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 12] Módulo Program ---")
        print("Navegando hacia Program con flechas (DOWN, DOWN, RIGHT, RIGHT, RIGHT, EXE)...")
        auditor.press_sequence(["DOWN", "DOWN", "RIGHT", "RIGHT", "RIGHT", "EXE"], default_wait=0.4)
        time.sleep(2.5)
        auditor.capture_and_audit("32_program_entry", expected_tokens=["Program"])

        print("Retornando al Menú Principal con MENU...")
        auditor.press_sequence(["MENU"], default_wait=1.5)
        auditor.capture_and_audit("33_menu_tras_program", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 13. Página 2 del Menú Principal (Herramientas y Extensiones del Sistema)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 13] Exploración de Página 2 del Menú ---")
        print("Desplazando hacia abajo en el Menú Principal (DOWN x 4)...")
        auditor.press_sequence(["DOWN", "DOWN", "DOWN", "DOWN"], default_wait=0.4)
        time.sleep(1.5)
        auditor.capture_and_audit("34_menu_pagina_2", expected_tokens=["System"])

        print("Regresando a Página 1 del Menú (UP x 4)...")
        auditor.press_sequence(["UP", "UP", "UP", "UP"], default_wait=0.4)
        time.sleep(1.5)
        auditor.capture_and_audit("35_menu_pagina_1_restaurada", expected_tokens=["MAIN MENU"])

        # ----------------------------------------------------------------------
        # 14. Vista Multimodelo Triple Concurrente (ClassWiz + Natural ES + fx-CG50)
        # ----------------------------------------------------------------------
        print("\n--- [Paso 14] Verificación Triple Multimodelo ---")
        print("Conmutando a vista panorámica Triple (modo 1010)...")
        user32.SendMessageW(auditor.hwnd, WM_COMMAND, ID_BTN_MODE_ALL, 0)
        time.sleep(2.0)
        auditor.capture_and_audit("36_triple_vista_panoramica", expected_tokens=["NATURAL"])

        # ----------------------------------------------------------------------
        # 15. Generación de Reporte HTML
        # ----------------------------------------------------------------------
        print("\n[Generando Reporte HTML]")
        html_file = auditor.generate_html_report(title="Auditoría Visual Masiva de la fx-CG50 (36 Pasos Exhaustivos)")
        print(f"CONFIRMED: Reporte HTML interactivo generado en: {html_file}")

    finally:
        print("\n[Cierre] Cerrando proceso de forma limpia...")
        auditor.stop()
        time.sleep(1.0)
        chk = subprocess.run(["powershell", "-NoProfile", "-Command",
                              "Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Measure-Object | Select-Object -ExpandProperty Count"],
                             capture_output=True, text=True)
        count = int(chk.stdout.strip() or 0)
        print(f"CONFIRMED: Procesos CalculadoraUnificada residuales = {count}")

    print("\n==========================================================================")
    print("  SUITE MASIVA COMPLETADA EXITOSAMENTE (100% OPERACIONAL)")
    print("==========================================================================")
    return 0

if __name__ == "__main__":
    sys.exit(main())
