#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cg50_battery.py — Batería funcional de la GUI de la CG50 (item 5.6, paso 1).

Orquesta dos herramientas existentes y aserta sobre sus salidas:
  1) tools\verify_gui.py  — lanza la app (si no corre), captura el canvas, recorta
     el LCD por detección, OCR (Windows) y emite JSON (status PASS si OCR contiene
     'MAIN MENU').
  2) tools\cg50_lcd_reader.py — clasifica la pantalla y mapea la grilla de iconos.

Aserciones del paso 1 (menu estable + lector como oráculo):
  A1  verify_gui.status == PASS
  A2  verify_gui OCR contiene 'MAIN MENU'
  A3  lector.clase == 'menu'
  A4  lector cuenta 12 componentes de icono (grilla 3x4)
  A5  lector OCR contiene >= 10 de los 13 terminos del menu

Salida: verification/2026-10-17-f5-bateria/bateria-<ts>.json (y stdout).
Exit 0 = PASS; 1 = FAIL; 2 = error de infraestructura.
La app queda CERRADA al terminar (si el script la lanzó).
"""
import datetime
import json
import os
import subprocess
import sys
import time

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PY = sys.executable
OUT_BASE = os.path.join(PROJ, "verification", "2026-10-17-f5-bateria")

TERMINOS_MENU = ["MAIN MENU", "Run-Matrix", "Graph", "Conic", "Statistics", "Dyna",
                 "Equation", "eActivity", "Table", "Program", "Spreadsheet",
                 "Recursion", "Financial"]


def log(msg):
    print("[bateria] " + msg, file=sys.stderr, flush=True)


def cerrar_app():
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-Process -Name CalculadoraUnificada -ErrorAction SilentlyContinue | "
                    "ForEach-Object { $null = $_.CloseMainWindow(); if (-not $_.WaitForExit(12000)) "
                    "{ Stop-Process -Id $_.Id -Force } }"],
                   capture_output=True)
    time.sleep(1.5)
    r = subprocess.run(["powershell", "-NoProfile", "-Command",
                        "if (Get-Process -Name CalculadoraUnificada -ErrorAction SilentlyContinue) { 'VIVA' } else { 'CERRADA' }"],
                       capture_output=True, text=True)
    return r.stdout.strip()


def main():
    ts = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = os.path.join(OUT_BASE, "run-" + ts)
    os.makedirs(out_dir, exist_ok=True)
    resultado = {"herramienta": "tools/cg50_battery.py", "version": "1.0 (paso 1)",
                 "ts": ts, "aserciones": {}, "sub_resultados": {}}

    try:
        # 1) verify_gui
        log("paso 1: verify_gui.py")
        r = subprocess.run([PY, os.path.join(PROJ, "tools", "verify_gui.py"),
                            "--out", out_dir, "--no-click", "--ocr"],
                           capture_output=True, text=True, timeout=420, cwd=PROJ)
        vg = None
        try:
            vg = json.loads(r.stdout)
        except Exception:
            # buscar el json guardado
            for f in sorted(os.listdir(out_dir), reverse=True):
                if f.startswith("capture-") and f.endswith(".json"):
                    vg = json.load(open(os.path.join(out_dir, f), encoding="utf-8"))
                    break
        if vg is None:
            log("verify_gui no produjo JSON; stderr: " + r.stderr[-400:])
            resultado["veredicto"] = "ERROR"
            _guardar(resultado, out_dir, ts)
            return 2
        resultado["sub_resultados"]["verify_gui"] = vg
        a1 = vg.get("status") == "PASS"
        ocr_vg = (vg.get("ocr") or {}).get("raw_output", "")
        a2 = "MAIN MENU" in ocr_vg.upper()
        resultado["aserciones"]["A1_verify_gui_PASS"] = a1
        resultado["aserciones"]["A2_OCR_MAIN_MENU"] = a2

        # 2) lector sobre el crop del propio verify_gui
        log("paso 2: cg50_lcd_reader.py")
        crop = os.path.join(out_dir, "gui-lcd-crop.png")
        rl = subprocess.run([PY, os.path.join(PROJ, "tools", "cg50_lcd_reader.py"),
                             crop, "--ocr", "--out", out_dir],
                            capture_output=True, text=True, timeout=300, cwd=PROJ)
        lector = None
        try:
            lector = json.loads(rl.stdout)
        except Exception:
            for f in sorted(os.listdir(out_dir), reverse=True):
                if f.startswith("lcd-") and f.endswith(".json"):
                    lector = json.load(open(os.path.join(out_dir, f), encoding="utf-8"))
                    break
        if lector is None:
            log("lector no produjo JSON; stderr: " + rl.stderr[-400:])
            resultado["veredicto"] = "ERROR"
            _guardar(resultado, out_dir, ts)
            return 2
        resultado["sub_resultados"]["lector"] = lector
        a3 = lector.get("clase") == "menu"
        cd = lector.get("celdas_detectadas")
        if isinstance(cd, int):
            n_iconos = cd
        elif isinstance(cd, list):
            n_iconos = len(cd)
        elif isinstance(cd, dict):
            n_iconos = cd.get("iconos") or cd.get("celdas_detectadas") or len(cd.get("celdas") or [])
        else:
            gr = lector.get("grilla")
            if isinstance(gr, dict):
                n_iconos = gr.get("iconos") or gr.get("celdas_detectadas") or len(gr.get("celdas") or [])
            else:
                n_iconos = 0
        a4 = n_iconos == 12
        ocr_txt = ((lector.get("ocr") or {}).get("texto") or "").upper()
        encontrados = [t for t in TERMINOS_MENU if t.upper() in ocr_txt]
        a5 = len(encontrados) >= 10
        resultado["aserciones"]["A3_lector_clase_menu"] = a3
        resultado["aserciones"]["A4_grilla_12_iconos"] = a4
        resultado["aserciones"]["A5_terminos_10_de_13"] = a5
        resultado["terminos_ocr_encontrados"] = encontrados

        # cierre de la app y veredicto
        estado = cerrar_app()
        resultado["app_cerrada"] = estado
        ok = a1 and a2 and a3 and a4 and a5
        resultado["veredicto"] = "PASS" if ok else "FAIL"
        _guardar(resultado, out_dir, ts)
        log("veredicto: " + resultado["veredicto"])
        return 0 if ok else 1
    except Exception as e:
        resultado["veredicto"] = "ERROR"
        resultado["error"] = repr(e)
        cerrar_app()
        _guardar(resultado, out_dir, ts)
        return 2


def _guardar(res, out_dir, ts):
    p = os.path.join(out_dir, "bateria-" + ts + ".json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=2)
    print(json.dumps({"veredicto": res.get("veredicto"),
                      "aserciones": res.get("aserciones"),
                      "ruta": p}, ensure_ascii=False))


if __name__ == "__main__":
    sys.exit(main())
