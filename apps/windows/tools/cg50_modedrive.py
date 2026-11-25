#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cg50_modedrive.py — Batería funcional 5.6 paso 3+ (G-1) + apps del MAIN MENU (FR9/G-5).

Reutiliza tools/cg50_keydrive.py (v2.0) como módulo: lanza la app GCal, espera
el MAIN MENU (A1, mismo patrón verificado) y conduce los modos con clics reales
sobre el skin. Cada pantalla se verifica con el oráculo de dos vías:
  - tools/cg50_lcd_reader.py (clasificador menu/boot_blanca/texto_denso/...);
  - OCR de Windows sobre el recorte 3x cuando aplica.

Batería G-1 (v1.1, verificada 2026-10-18 PASS 10/10, sin cambios de semántica):
  1. STAT  : badge '2' (si no abre: normalizar a Run-Matrix + DERECHA+EXE)
             -> captura 'stat' -> MENU -> 'menu-s1'
  2. GRAPH : badge '5' (fallback ABAJO+EXE) -> captura 'graph';
             X_THETA, X2, EXE -> 'graph-expr'; F6 (DRAW) -> 'graph-draw'
             -> MENU -> 'menu-s2'
  3. TABLE : badge '7' (fallback ABAJO+DERECHA+DERECHA+EXE) -> captura 'table';
             X_THETA, X2, EXE -> 'table2' -> MENU -> 'menu-s3'

FR9 (v2.0, 2026-10-20) — apps adicionales del MAIN MENU:
  Evidencia de layout (crop 3x del menú en verification/2026-10-18-g1-modos +
  OCR de verification/2026-10-17/verify-gui-rerun): el MAIN MENU de este OS
  es 4 columnas x 3 filas por página con badges: fila 1 = Run-Matrix(1),
  Statistics(2), eActivity(3), Spreadsheet(4); fila 2 = Graph(5), Dyna Graph(6),
  Table(7), Recursion(8); fila 3 = Conic Graphs(9), Equation(A), Program(B),
  Financial(C). Los badges 1-9 abren el icono directamente (verificado 3x con
  2/5/7 en md2). main.cpp NO mapea letras (VK_A..VK_C no existen), así que
  Equation/Program/Financial se alcanzan por flechas; como la semántica del
  cursor tras MENU (¿preserva el icono o resetea a Run-Matrix?) no está
  verificada en corpus, cada app sin badge prueba DOS rutas y registra cuál
  funcionó (evidencia sobre el modelo de navegación). Memory/Link/System no
  tienen evidencia de posición en el corpus (están tras el scroll/del ▼ del
  menú): quedan como etapas EXPLORATORIAS (no abren nada, solo navegan y
  capturan lo que muestra el menú) y SOLO si se piden por --apps.

  Apps nuevas página 1 (grupo menu1 / incluidas en full):
      eactivity   badge '3' (fallback: flechas) -> OCR 'eActivity'
      spreadsheet badge '4' (idem)              -> OCR 'Spreadsheet'
      dyna        badge '6' (idem)              -> OCR 'Dyna Graph'
      recursion   badge '8' (idem)              -> OCR 'Recursion'
      conic       badge '9' (idem)              -> OCR 'Conic Graphs'
      equation    flechas: DERECHA desde Conic (modelo A) o
                  normalizadas ABAJO+ABAJO+DERECHA (modelo B) -> OCR 'Equation'
      program     flechas: DERECHA(/DERECHA) o normaliz.
                  ABAJO+ABAJO+DERECHA+DERECHA -> OCR 'Prog Menu'/'Program'
      financial   flechas: DERECHA o normaliz.
                  ABAJO+ABAJO+DERECHA+DERECHA+DERECHA -> OCR 'Financial'
  Exploratorias (grupo explora / todo):
      memory/link/system -> normalizar + ABAJO x4 (+DERECHA x0/x1/x2): prueba
      la semántica del paso de página; NO presionan EXE (no abren apps
      desconocidas); aserción = la pantalla del menú cambió.

Correcciones FR9 a los fallbacks de flechas de v1.1 (nunca ejercitados y mal
calculados: asumían 3 columnas; el layout real es 4): GRAPH = fila 2 col 1 ->
ABAJO+EXE (antes ABAJO+DERECHA+EXE); TABLE = fila 2 col 3 -> ABAJO+DERECHA+
DERECHA+EXE (antes ABAJO+ABAJO+EXE). STAT sin cambio (DERECHA+EXE).

Guardas FR9: toda app nueva cierra con ensure_menu() (hasta 3 MENU con OCR
entre intentos) y al final del flujo se vuelve a exigir MAIN MENU
(B_final_menu); close_app sigue corriendo salvo --keep-open.

CLI nueva (FR9): --apps lista (nombres o grupos stat..financial, memory, link,
system | battery | menu1 | full | todo; sin --apps el default es 'full') y
--time-budget SEC (0 = sin límite; al excederlo las etapas restantes se
registran como apps_skipped_presupuesto, no como FAIL).
Productores de capturas en --out: lcd-full-<tag>.png, lcd-crop-<tag>.png,
lcd-crop-3x-<tag>.png (+ lcd-crop-3x-<tag>-lower.png en reintento OCR),
lcd-<hora>-<ms>.json del lector y lcd-plotink-<tag>.png (graph-draw); el
informe final es modedrive-<ts>.json y el stdout imprime veredicto+aserciones.

Aserciones G-1 (v1.1, sin cambio):
   A1  menú visible (colorido + OCR) antes de tocar nada.
   A2  STAT abre: el crop cambia, el lector no clasifica 'menu' y el OCR lee
       la identidad del modo ('LIST').
   A3  MENU vuelve: OCR lee MAIN MENU.
   A4  GRAPH abre (cambio + no-menu + OCR 'GRAPH FUNC') y tras F6 DRAW la
       pantalla cambia y hay tinta oscura en la zona del plot.
   A5  MENU vuelve tras GRAPH.
   A6  TABLE abre (cambio + no-menu + OCR 'TABLE FUNC'); tras X^2+EXE la
       pantalla cambia y F6 (TABLE) muestra la tabla de valores.
   A7  MENU vuelve tras TABLE.

Aserciones FR9 (v2.0): B_<app>_abierto por app de página 1 ejecutada;
B_explora_<app>_navegacion para las tres exploratorias si se pidieron;
B_final_menu al cierre del flujo.

Uso: python tools/cg50_modedrive.py --out <dir> [--exe <ruta>] [--keep-open]
     [--apps lista|grupo] [--time-budget SEC]
Exit 0 = PASS; 1 = FAIL; 2 = error de infraestructura.
"""

import argparse
import ctypes
import json
import os
import subprocess
import sys
import time

import cg50_keydrive as kd
from PIL import Image

PROJ = kd.PROJECT_ROOT

KEYS_EXTRA = {
    "F1": (112, 559, 0x070A), "F2": (200, 559, 0x060A), "F3": (288, 559, 0x050A),
    "F4": (376, 559, 0x040A), "F5": (464, 559, 0x030A), "F6": (552, 559, 0x020A),
    "ARRIBA": (512, 628, 0x0209), "IZQUIERDA": (460, 676, 0x0309),
    "DERECHA": (564, 676, 0x0208), "ABAJO": (512, 724, 0x0308),
    "X_THETA": (112, 789, 0x0707), "X2": (202, 711, 0x0608),
}

NORMALIZAR = ["IZQUIERDA", "IZQUIERDA", "IZQUIERDA", "ARRIBA", "ARRIBA", "ARRIBA"]

PLOT_INK_MIN = 150

# ---- FR9: apps del MAIN MENU. pos = (fila, columna) 1-based (evidencia:
# crop md2 + OCR 2026-10-17). estado: g1 = cubierto por la batería v1.1
# (responden en --apps también); nuevo = FR9, badge directo; nuevo-flechas =
# FR9, sin VK para la letra del badge; exploratorio = FR9, sin evidencia.
APPS = {
    "stat":        {"badge": "2", "pos": (1, 2), "tokens": ["LIST"], "estado": "g1"},
    "eactivity":   {"badge": "3", "pos": (1, 3), "tokens": ["EACTIVITY", "ACTIVITY"], "estado": "nuevo"},
    "spreadsheet": {"badge": "4", "pos": (1, 4), "tokens": ["SPREADSHEET"], "estado": "nuevo"},
    "graph":       {"badge": "5", "pos": (2, 1), "tokens": ["GRAPH FUNC"], "estado": "g1"},
    "dyna":        {"badge": "6", "pos": (2, 2), "tokens": ["DYNA", "DYNAGRAPH"], "estado": "nuevo"},
    "table":       {"badge": "7", "pos": (2, 3), "tokens": ["TABLE FUNC"], "estado": "g1"},
    "recursion":   {"badge": "8", "pos": (2, 4), "tokens": ["RECURSION"], "estado": "nuevo"},
    "conic":       {"badge": "9", "pos": (3, 1), "tokens": ["CONIC"], "estado": "nuevo"},
    "equation":    {"badge": None, "pos": (3, 2), "tokens": ["EQUATION"], "estado": "nuevo-flechas"},
    "program":     {"badge": None, "pos": (3, 3), "tokens": ["PROG", "PROGRAM"], "estado": "nuevo-flechas"},
    "financial":   {"badge": None, "pos": (3, 4), "tokens": ["FINANCIAL", "FINANCE"], "estado": "nuevo-flechas"},
    "memory":      {"badge": None, "pos": None, "tokens": ["MEMORY"], "estado": "exploratorio"},
    "link":        {"badge": None, "pos": None, "tokens": ["LINK"], "estado": "exploratorio"},
    "system":      {"badge": None, "pos": None, "tokens": ["SYSTEM"], "estado": "exploratorio"},
}

CANONICAL = ["stat", "graph", "table",
             "eactivity", "spreadsheet", "dyna", "recursion", "conic",
             "equation", "program", "financial",
             "memory", "link", "system"]

APP_GROUPS = {
    "battery": ["stat", "graph", "table"],
    "menu1": ["eactivity", "spreadsheet", "dyna", "recursion", "conic",
              "equation", "program", "financial"],
    "explora": ["memory", "link", "system"],
}
APP_GROUPS["full"] = APP_GROUPS["battery"] + APP_GROUPS["menu1"]
APP_GROUPS["todo"] = APP_GROUPS["full"] + APP_GROUPS["explora"]


def parse_apps(spec):
    tokens = [t.strip().lower() for t in (spec or "").split(",") if t.strip()]
    if not tokens:
        tokens = ["full"]
    names = []
    for t in tokens:
        if t in APP_GROUPS:
            names.extend(APP_GROUPS[t])
        elif t in APPS:
            names.append(t)
        else:
            raise ValueError(
                "--apps: identificador desconocido: %r (grupos: %s)"
                % (t, ", ".join(sorted(APP_GROUPS))))
    sel = set(names)
    return [a for a in CANONICAL if a in sel]


def log(msg):
    print("[modedrive] " + msg, file=sys.stderr, flush=True)


def run_reader(crop_png, out_dir, tag, res):
    cmd = [sys.executable, os.path.join(PROJ, "tools", "cg50_lcd_reader.py"),
           crop_png, "--out", out_dir]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=180, cwd=PROJ)
        try:
            info = json.loads(r.stdout)
        except Exception:
            info = {"parse_error": True, "stdout_tail": (r.stdout or "")[-400:],
                    "stderr_tail": (r.stderr or "")[-400:]}
    except Exception as e:
        info = {"error": repr(e)}
    res["lector"][tag] = info
    return info


def clase(info):
    if not isinstance(info, dict):
        return None
    c = info.get("clase")
    if c:
        return c
    gr = info.get("grilla")
    return gr.get("clase") if isinstance(gr, dict) else None


def plot_ink(out_dir, tag, crop_png, res):
    img = Image.open(crop_png).convert("RGB")
    w, h = img.size
    region = img.crop((int(w * 0.10), int(h * 0.12), int(w * 0.85), int(h * 0.55)))
    png = os.path.join(out_dir, f"lcd-plotink-{tag}.png")
    region.save(png)
    dark = sum(1 for p in region.getdata() if min(p) < 100)
    res["plot_ink"] = res.get("plot_ink", {})
    res["plot_ink"][tag] = {"dark_pixels": dark, "min": PLOT_INK_MIN, "png": png}
    return dark


def _abort_run(res, args, ts, error):
    res["veredicto"] = "FAIL"
    res["error"] = error
    _save(res, args.out, ts)
    if not args.keep_open:
        kd.close_app()
        res["app_cerrada"] = True
    return 1


def main():
    dpi = kd.enable_dpi_awareness()
    ap = argparse.ArgumentParser(
        description="Paso 3+ de la batería 5.6 (modos) + apps del MAIN MENU (FR9)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--exe", default=kd.DEFAULT_EXE)
    ap.add_argument("--keep-open", action="store_true")
    ap.add_argument("--apps", default="",
                    help="lista separada por coma de nombres de app o grupos: "
                         "stat,graph,table,eactivity,spreadsheet,dyna,recursion,"
                         "conic,equation,program,financial | memory,link,system | "
                         "battery | menu1 | full | todo (default: full)")
    ap.add_argument("--time-budget", type=float, default=0.0,
                    help="límite de segundos para las etapas que aún no arrancan "
                         "(0 = sin límite; lo saltado queda en apps_skipped_presupuesto)")
    args = ap.parse_args()

    try:
        selected = parse_apps(args.apps)
    except ValueError as exc:
        print("[modedrive] " + str(exc), file=sys.stderr, flush=True)
        return 2
    log("apps seleccionadas: " + (", ".join(selected) or "(ninguna)"))

    kd.KEYS.update(KEYS_EXTRA)
    os.makedirs(args.out, exist_ok=True)
    ts = time.strftime("%Y%m%d-%H%M%S")
    log_path = os.path.join(os.path.dirname(args.exe), "calc_debug.log")
    res = {"herramienta": "tools/cg50_modedrive.py",
           "version": "2.0 (FR9: apps del MAIN MENU con badges 1-9/flechas por "
                      "layout 4x3; corrección de fallbacks v1.1; exploratorias "
                      "memory/link/system; --apps/--time-budget; reusa cg50_keydrive v2.0)",
           "ts": ts, "exe": args.exe, "dpi_awareness": dpi,
           "apps_seleccionadas": selected,
           "apps_skipped_presupuesto": [],
           "apps_plan": {},
           "aserciones": {}, "capturas": {}, "ocr": {}, "lector": {},
           "clave_enviada": [], "navegacion": []}

    kd.close_app()
    n0 = kd.count_ready_in_log(log_path)
    subprocess.Popen([args.exe], cwd=os.path.dirname(args.exe))
    hwnds = []
    for _ in range(60):
        time.sleep(0.5)
        hwnds = kd.find_visible_windows(kd.MAIN_CLASS)
        if hwnds:
            break
    if not hwnds:
        res["veredicto"] = "ERROR"
        res["error"] = "ventana CasioNativeMain no apareció"
        _save(res, args.out, ts)
        return 2
    hwnd = hwnds[0]
    for _ in range(80):
        time.sleep(0.5)
        if kd.count_ready_in_log(log_path) > n0:
            break
    canvas = kd.find_child(hwnd, kd.CANVAS_CLASS)
    if not canvas:
        res["veredicto"] = "ERROR"
        res["error"] = "canvas CasioCanvasArea no encontrado"
        _save(res, args.out, ts)
        return 2
    kd.user32.SendMessageW(hwnd, kd.WM_COMMAND,
                           (kd.BN_CLICKED << 16) | kd.ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.5)

    rect = kd.wt.RECT()
    kd.user32.GetClientRect(canvas, ctypes.byref(rect))
    cw, ch = rect.right, rect.bottom
    layout = kd.solo_layout(cw, ch)
    res["canvas"] = {"w": cw, "h": ch, "layout": list(layout)}
    lcd_box = kd.expected_lcd_box(layout)

    # ---- A1: MAIN MENU visible (patrón de keydrive v1.2) ----
    # FR9 recuperación (ESCRITO NO CORRIDO 2026-10-20): la única corrida FR9
    # encontró el boot del guest bloqueado en "MAIN MEMORY CLEARED !
    # Press [MENU] KEY" (evidencia run-fr9/lcd-crop-3x-menu.png; el OS no
    # saca el MAIN MENU hasta que se presione [MENU]). Detección por OCR cada
    # ~5 iteraciones del bucle mientras colorido sea falso; si la pantalla de
    # memory-cleared es la que hay, se presiona [MENU] una vez por detección.
    menu_png = None
    menu_info = None
    menu_ok = False
    probe_i = 0
    deadline = time.time() + 40.0
    while time.time() < deadline:
        png_i, info_i = kd.capture_lcd(canvas, cw, ch, args.out, "menu", lcd_box)
        menu_png, menu_info = png_i, info_i
        colorido = bool(info_i and info_i.get("colorful_pixels", 0) >= 20000)
        log(f"esperando MAIN MENU: colorido={colorido}")
        if colorido:
            found = kd.ocr_tokens(args.out, "menu", png_i,
                                  ["MAIN MENU", "Run-Matrix"], res["ocr"])
            if found.get("MAIN MENU") or found.get("Run-Matrix"):
                menu_ok = True
                break
        else:
            probe_i += 1
            if probe_i % 5 == 0:
                probed = kd.ocr_tokens(args.out, "menu", png_i,
                                       ["MEMORY CLEARED"], res["ocr"])
                if probed.get("MEMORY CLEARED"):
                    log("pantalla 'MAIN MEMORY CLEARED' detectada; presiono [MENU] y sigo esperando")
                    res["clave_enviada"].append("MENU")
                    kd.click_key(canvas, layout, "MENU")
                    time.sleep(2.0)
        time.sleep(1.0)
    res["capturas"]["menu"] = menu_info
    res["aserciones"]["A1_menu_visible"] = bool(menu_ok)
    if not menu_ok:
        res["veredicto"] = "FAIL"
        res["error"] = "MAIN MENU no alcanzado (sin clics enviados)"
        _save(res, args.out, ts)
        return 1

    def sha_of(info):
        return info.get("crop_sha256") if info else None

    def press(k, wait=0.5):
        kd.click_key(canvas, layout, k)
        res["clave_enviada"].append(k)
        time.sleep(wait)

    def snap(tag):
        png, info = kd.capture_lcd(canvas, cw, ch, args.out, tag, lcd_box)
        res["capturas"][tag] = info
        return png, info

    def wait_change(base_sha, timeout):
        dl = time.time() + timeout
        while time.time() < dl:
            probe = kd.render_window(canvas, cw, ch)
            if probe is not None:
                psha = kd.sha256_bytes(probe.crop(lcd_box).tobytes())
                if psha != base_sha:
                    return True
            time.sleep(0.5)
        return False

    def snap_reader(tag):
        png, info = snap(tag)
        lec = run_reader(os.path.join(args.out, f"lcd-crop-{tag}.png"),
                         args.out, tag, res)
        return png, info, lec

    def cur_sha():
        probe = kd.render_window(canvas, cw, ch)
        if probe is None:
            return None
        return kd.sha256_bytes(probe.crop(lcd_box).tobytes())

    def ensure_menu(tag):
        for i in range(3):
            press("MENU", 2.0)
            png, info = snap(f"{tag}-m{i}")
            found = kd.ocr_tokens(args.out, f"{tag}-m{i}", png,
                                  ["MAIN MENU", "Run-Matrix"], res["ocr"])
            if found.get("MAIN MENU") or found.get("Run-Matrix"):
                return True
            time.sleep(1.0)
        return False

    def open_mode(tag, badge, arrows, base_sha, tokens):
        """Etapas G-1 (v1.1 sin cambios de semántica): badge primero; si no
        abre, normalización a Run-Matrix + flechas + EXE (arrows INCLUYE EXE)."""
        via = "badge"
        press(badge, 1.5)
        wait_change(base_sha, 8.0)
        time.sleep(1.0)
        png, info, lec = snap_reader(tag)
        if sha_of(info) == base_sha or clase(lec) == "menu":
            via = "flechas"
            log(f"{tag}: badge sin efecto; normalizo y navego {arrows}")
            for k in NORMALIZAR:
                press(k, 0.35)
            for k in arrows:
                press(k, 0.5)
            wait_change(base_sha, 8.0)
            time.sleep(1.2)
            png, info, lec = snap_reader(tag)
        found = kd.ocr_tokens(args.out, tag, png, tokens, res["ocr"])
        res["navegacion"].append({"modo": tag, "via": via, "badge": badge,
                                  "flechas": arrows, "crop_sha256": sha_of(info),
                                  "clase_lector": clase(lec), "tokens": tokens,
                                  "tokens_encontrados": found})
        opened = bool(sha_of(info) != base_sha and clase(lec) != "menu"
                      and found.get(tokens[0]))
        return png, info, lec, opened

    def return_menu(tag):
        press("MENU", 2.0)
        png, info, lec = snap_reader(tag)
        found = kd.ocr_tokens(args.out, tag, png, ["MAIN MENU", "Run-Matrix"], res["ocr"])
        ok = bool(found.get("MAIN MENU") or found.get("Run-Matrix"))
        res["navegacion"].append({"modo": tag, "via": "MENU", "clase_lector": clase(lec),
                                  "ocr_menu": ok, "crop_sha256": sha_of(info)})
        return ok, info

    # FR9: etapas de apps (página 1) — badge absoluto + dos rutas por flechas.
    # cur_pos = hipótesis del icono resaltado tras el último MENU (modelo A:
    # el menú preserva el icono; tras boot = Run-Matrix, evidencia kd legacy
    # A2). pos None = desconocido -> solo la ruta (modelo B) se prueba.
    cur_pos = {"pos": (1, 1), "modelo": "boot (evidencia kd legacy: EXE abre Run-Matrix)"}

    def open_new_app(app):
        spec = APPS[app]
        title = APPS[app]["pos"]
        st = {"app": app, "estado": spec["estado"], "intentos": [], "abierta": False}
        res["apps_plan"][app] = st
        attempts = []
        if spec["badge"]:
            attempts.append({"via": "badge", "keys": [spec["badge"]], "normalizar_antes": False})
        actual = cur_pos["pos"]
        if actual is not None and actual[0] == title[0] and title[1] > actual[1]:
            attempts.append({"via": "flechas(modeloA-preserve)",
                             "keys": ["DERECHA"] * (title[1] - actual[1]) + ["EXE"],
                             "normalizar_antes": False})
        attempts.append({"via": "flechas(modeloB-reset)",
                         "keys": list(NORMALIZAR) + ["ABAJO"] * (title[0] - 1)
                                 + ["DERECHA"] * (title[1] - 1) + ["EXE"],
                         "normalizar_antes": True})
        opened = False
        last = (None, None)
        for i, att in enumerate(attempts):
            tag = f"{app}-a{i}"
            if att["normalizar_antes"]:
                okm = ensure_menu(f"{app}-pre{i}")
                if not okm:
                    st["intentos"].append({"via": "ensure_menu-pre", "ok": False})
                    break
            base = cur_sha()
            for k in att["keys"]:
                press(k, 0.5)
            changed = wait_change(base, 8.0)
            time.sleep(1.0)
            png, info, lec = snap_reader(tag)
            found = kd.ocr_tokens(args.out, tag, png, spec["tokens"], res["ocr"])
            rec = {"via": att["via"], "keys": att["keys"], "sha_antes": base,
                   "crop_sha256": sha_of(info), "pantalla_cambio": changed,
                   "clase_lector": clase(lec), "tokens": found,
                   "identidad": any(found.values())}
            ok = bool(changed and base and sha_of(info) != base
                      and clase(lec) != "menu" and rec["identidad"])
            rec["ok"] = ok
            st["intentos"].append(rec)
            if ok:
                opened = True
                last = (info, lec)
                break
        st["abierta"] = opened
        return opened, last[0], last[1]

    menu_sha = sha_of(menu_info)
    last_menu_sha = menu_sha
    sel = set(selected)
    sel_order = [a for a in CANONICAL if a in sel]
    done = []
    t_phase = time.time()

    def budget_hit():
        if args.time_budget > 0 and (time.time() - t_phase) > args.time_budget:
            missing = [a for a in sel_order if a not in done]
            res["apps_skipped_presupuesto"] = missing
            log("presupuesto agotado; etapas saltadas: "
                + (", ".join(missing) or "(ninguna)"))
            return True
        return False

    # ================= Batería G-1 (v1.1, semántica intacta) =================
    if "stat" in sel:
        _, stat_info, stat_lec, stat_ok = open_mode(
            "stat", "2", ["DERECHA", "EXE"], menu_sha, ["LIST"])
        res["aserciones"]["A2_stat_abierto"] = stat_ok
        done.append("stat")
        cur_pos = {"pos": (1, 2) if stat_ok else None, "modelo": "hipótesis=modeloA"}

        menu_ok2, menu_info2 = return_menu("menu-s1")
        last_menu_sha = sha_of(menu_info2)
        res["aserciones"]["A3_menu_tras_stat"] = menu_ok2
        res["menu_s1_identico_al_inicial"] = sha_of(menu_info2) == menu_sha
        if not menu_ok2:
            return _abort_run(res, args, ts, "no se pudo volver al MAIN MENU tras STAT")

    if "graph" in sel:
        _, graph_info, graph_lec, graph_ok = open_mode(
            "graph", "5", ["ABAJO", "EXE"], last_menu_sha, ["GRAPH FUNC"])
        res["aserciones"]["A4a_graph_abierto"] = graph_ok
        cur_pos = {"pos": (2, 1) if graph_ok else None, "modelo": "hipótesis=modeloA"}

        for k in ("X_THETA", "X2", "EXE"):
            press(k, 0.6)
        time.sleep(1.0)
        _, expr_info, expr_lec = snap_reader("graph-expr")

        press("F6", 0.6)
        time.sleep(3.0)
        draw_png, draw_info, draw_lec = snap_reader("graph-draw")
        ink = plot_ink(args.out, "graph-draw",
                       os.path.join(args.out, "lcd-crop-graph-draw.png"), res)
        a4b = bool(sha_of(draw_info) != sha_of(expr_info) and ink >= PLOT_INK_MIN)
        res["aserciones"]["A4b_graph_dibujo"] = a4b

        menu_ok3, menu_info3 = return_menu("menu-s2")
        last_menu_sha = sha_of(menu_info3)
        res["aserciones"]["A5_menu_tras_graph"] = menu_ok3
        if not menu_ok3:
            return _abort_run(res, args, ts, "no se pudo volver al MAIN MENU tras GRAPH")
        done.append("graph")

    if "table" in sel:
        _, table_info, table_lec, table_ok = open_mode(
            "table", "7", ["ABAJO", "DERECHA", "DERECHA", "EXE"], last_menu_sha,
            ["TABLE FUNC"])
        res["aserciones"]["A6a_table_abierto"] = table_ok
        cur_pos = {"pos": (2, 3) if table_ok else None, "modelo": "hipótesis=modeloA"}

        for k in ("X_THETA", "X2", "EXE"):
            press(k, 0.6)
        time.sleep(1.5)
        png3x, table2_info, table2_lec = snap_reader("table2")
        found_t2 = kd.ocr_tokens(args.out, "table2", png3x, ["TABLE FUNC"], res["ocr"])
        a6b = bool(sha_of(table2_info) != sha_of(table_info)
                   and clase(table2_lec) != "menu" and found_t2.get("TABLE FUNC"))
        res["aserciones"]["A6b_table_expresion"] = a6b

        press("F6", 0.6)
        time.sleep(2.5)
        _, tval_info, tval_lec = snap_reader("table-valores")
        a6c = bool(sha_of(tval_info) != sha_of(table2_info) and clase(tval_lec) != "menu")
        res["aserciones"]["A6c_table_valores"] = a6c

        menu_ok4, menu_info4 = return_menu("menu-s3")
        last_menu_sha = sha_of(menu_info4)
        res["aserciones"]["A7_menu_tras_table"] = menu_ok4
        if not menu_ok4:
            return _abort_run(res, args, ts, "no se pudo volver al MAIN MENU tras TABLE")
        done.append("table")

    # ================= Apps nuevas FR9 (página 1) =================
    for app in ["eactivity", "spreadsheet", "dyna", "recursion", "conic",
                "equation", "program", "financial"]:
        if app not in sel:
            continue
        if budget_hit():
            break
        done.append(app)
        opened, info, lec = open_new_app(app)
        res["aserciones"][f"B_{app}_abierto"] = bool(opened)
        if opened:
            cur_pos = {"pos": APPS[app]["pos"], "modelo": "hipótesis=modeloA"}
            bk, info_bk = return_menu(f"{app}-menu")
            res["apps_plan"][app]["menu_tras_abrir"] = bk
            if not bk:
                okm = ensure_menu(f"{app}-post")
                res["apps_plan"][app]["menu_recuperado"] = okm
                if not okm:
                    return _abort_run(
                        res, args, ts,
                        f"{app} abrió pero la guarda de salida no recuperó el MAIN MENU")
        else:
            cur_pos = {"pos": None, "modelo": "desconocido (solo ruta modeloB)"}
            okm = ensure_menu(f"{app}-post")
            res["apps_plan"][app]["menu_recuperado"] = okm
            if not okm:
                return _abort_run(
                    res, args, ts,
                    f"{app} no abrió y la guarda de salida no recuperó el MAIN MENU")

    # ================= Exploratorias FR9 (memory/link/system) =================
    for app in ["memory", "link", "system"]:
        if app not in sel:
            continue
        if budget_hit():
            break
        done.append(app)
        spec = APPS[app]
        st = {"app": app, "estado": spec["estado"], "intentos": [], "abierta": False}
        res["apps_plan"][app] = st
        okm = ensure_menu(f"{app}-sweep")
        if not okm:
            res["aserciones"][f"B_explora_{app}_navegacion"] = False
            return _abort_run(res, args, ts,
                              f"{app}: menú inaccesible antes del barrido exploratorio")
        for k in NORMALIZAR:
            press(k, 0.35)
        base = cur_sha()
        steps_down = 4
        rights = {"memory": 1, "link": 0, "system": 2}[app]
        keys = ["ABAJO"] * steps_down + ["DERECHA"] * rights
        for k in keys:
            press(k, 0.6)
        png, info = snap(f"{app}-nav")
        found = kd.ocr_tokens(args.out, f"{app}-nav", png, spec["tokens"], res["ocr"])
        moved = bool(base and sha_of(info) and sha_of(info) != base)
        st["intentos"].append({"via": "exploratoria (sin EXE)", "keys": keys,
                               "sha_antes": base, "crop_sha256": sha_of(info),
                               "navego": moved, "tokens": found})
        res["aserciones"][f"B_explora_{app}_navegacion"] = moved
        okm2 = ensure_menu(f"{app}-back")
        st["menu_post"] = okm2
        if not okm2:
            return _abort_run(res, args, ts,
                              f"{app}: la guarda de salida no recuperó el MAIN MENU")

    # ---- Guarda final de salida (FR9) ----
    okm = ensure_menu("final")
    res["aserciones"]["B_final_menu"] = bool(okm)
    if not okm:
        return _abort_run(res, args, ts, "guarda final: MAIN MENU no confirmado al cierre")

    if not args.keep_open:
        kd.close_app()
        res["app_cerrada"] = True

    ok = all(res["aserciones"].values())
    res["veredicto"] = "PASS" if ok else "FAIL"
    _save(res, args.out, ts)
    log("veredicto: " + res["veredicto"])
    return 0 if ok else 1


def _save(res, out_dir, ts):
    p = os.path.join(out_dir, f"modedrive-{ts}.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=2)
    print(json.dumps({"veredicto": res.get("veredicto"),
                      "aserciones": res.get("aserciones"),
                      "ruta": p}, ensure_ascii=False))


if __name__ == "__main__":
    sys.exit(main())
