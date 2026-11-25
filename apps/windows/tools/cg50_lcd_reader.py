#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cg50_lcd_reader.py — Lector de pantalla CG50 (ítem 5.5 del plan, primera entrega).

Convierte una imagen del LCD de la fx-CG50 (PNG/BMP) en un estado estructurado
JSON verificable por CI.

Clases detectadas:
  menu         — MAIN MENU: grilla de iconos coloridos sobre fondo blanco
  boot_blanca  — pantalla blanca de arranque (monocroma, contenido mínimo)
  texto_denso  — texto denso sobre fondo claro (p.ej. MEM, File System ERROR)
  desconocida  — no coincide con ninguna de las anteriores

Heurísticas (censo GLOBAL de píxeles; umbrales calibrados con la evidencia del
proyecto 2026-10-16/17 — ver verification\\2026-10-17-f5-lector\\INFORME-LECTOR-5-5-2026-10-17.md):

  colorido := max(R,G,B) - min(R,G,B) >= 30   (canales RGB claramente distintos)
  oscuro   := luminancia < 100   (0.299·R + 0.587·G + 0.114·B)
  blanco   := luminancia >= 224

  menu         := r_color >= 0.10  Y  >= 8 componentes coloridos "tipo icono"
                  (área >= 0.5% del total, bbox <= 35% del ancho/alto de la
                  imagen; descarta franjas/biseles alargados)
  boot_blanca  := r_color <= 0.05  Y  r_blanco >= 0.50  Y
                  ( mayor componente oscura >= 20% de la imagen   [captura con
                    bisel/estructura oscura alrededor del LCD blanco ]
                    O (r_oscuro <= 0.08 Y aspecto 1.4..2.6) )    [LCD aislada]
  texto_denso  := r_oscuro >= 0.08  Y  r_blanco >= 0.50   [texto sobre fondo claro]
  desconocida  := resto

Limitación conocida y aceptada en v1: el censo es GLOBAL. Capturas de ventana
completa o recortes mal alineados (fondo de app oscuro, teclado colorido, bisel)
contaminan los ratios y clasifican como "desconocida". El lector clasifica
IMÁGENES DE PANTALLA; una captura que no es la pantalla no es clasificable.

Uso:
  python tools\\cg50_lcd_reader.py <imagen> [--out <dir>] [--ocr]

  --out <dir>  guarda además una copia del JSON en <dir>\\lcd-<ts>.json
               (ts UTC con milisegundos: lcd-20261017-181234-567.json)
  --ocr        solo si clase == menu: ejecuta tools\\ocr_png.ps1 (OCR nativo
               de Windows, Windows.Media.Ocr) y registra términos encontrados.
               Para BMP se convierte a PNG en %TEMP%\\cg50_lcd_reader_ocr.

Salida: JSON por stdout. Códigos de salida:
  0 = imagen procesada (cualquier clase, incluida "desconocida")
  2 = uso incorrecto (argparse)
  3 = imagen no encontrada o ilegible

Herramienta del proyecto Calculadora Unificada (2026-10-17). Solo stdlib + PIL.
"""

import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    print("ERROR: PIL no disponible (pip install pillow)", file=sys.stderr)
    sys.exit(3)

VERSION = "1.0.0"
LCD_CG50 = {"ancho": 396, "alto": 224}  # LCD nativo fx-CG50 (VRAM dumpeada: 384×216)

# ── Umbrales (documentados arriba; calibración en el INFORME 5.5) ──────────
UMBRAL_COLORIDO_DELTA = 30     # diferencia máx-mín de canales RGB
UMBRAL_OSCURO_LUM = 100        # luminancia < 100  (especificado en 5.5)
UMBRAL_BLANCO_LUM = 224        # luminancia >= 224

MENU_R_COLOR_MIN = 0.10        # >=10% de píxeles coloridos
MENU_COMPS_MIN = 8             # >=8 componentes tipo icono
MENU_COMP_AREA_FRAC = 0.005    # área de componente >= 0.5% del total
MENU_COMP_BBOX_FRAC = 0.35     # bbox del componente <= 35% de ancho/alto

BOOT_R_COLOR_MAX = 0.05        # pantalla monocroma: <=5% coloridos
BOOT_R_BLANCO_MIN = 0.50       # fondo claro dominante
BOOT_MAYOR_COMP_OSCURA_MIN = 0.20  # bisel oscuro conectado >= 20% de la imagen
BOOT_R_OSCURO_MAX_LIMPIO = 0.08    # LCD aislada: contenido oscuro mínimo
BOOT_ASPECTO_MIN = 1.4         # tolerancia de aspecto alrededor de 396/224≈1.77
BOOT_ASPECTO_MAX = 2.6

TEXTO_R_OSCURO_MIN = 0.08      # densidad de texto mínima
TEXTO_R_BLANCO_MIN = 0.50      # fondo claro dominante

# Términos esperados del MAIN MENU según OCR previo del proyecto (es-MX)
TERMINOS_MENU = [
    "MAIN MENU", "CASIO",
    "Run-Matrix", "Graph", "Conic Graphs", "Statistics", "Dyna Graph",
    "Equation", "eActivity", "Table", "Program", "Spreadsheet",
    "Recursion", "Financial",
]


def _acotar(x, lo=0.0, hi=1.0):
    return max(lo, min(hi, x))


# ── Censo global de píxeles ────────────────────────────────────────────────
def censo(rgb):
    """Un solo pase sobre la imagen. Devuelve conteos, máscaras y filas oscuras."""
    w, h = rgb.size
    total = w * h
    buf = rgb.tobytes()
    mask_col = bytearray(total)
    mask_osc = bytearray(total)
    n_col = n_osc = n_blan = 0
    suma_lum = 0
    filas_osc = [0] * h
    for y in range(h):
        base = y * w * 3
        fila = buf[base:base + w * 3]
        dr = 0
        fila3 = fila
        for x in range(w):
            r = fila3[x * 3]
            g = fila3[x * 3 + 1]
            b = fila3[x * 3 + 2]
            mx = r if r > g else g
            if b > mx:
                mx = b
            mn = r if r < g else g
            if b < mn:
                mn = b
            lum = (299 * r + 587 * g + 114 * b) // 1000
            suma_lum += lum
            if lum < UMBRAL_OSCURO_LUM:
                n_osc += 1
                dr += 1
                mask_osc[y * w + x] = 1
            elif lum >= UMBRAL_BLANCO_LUM:
                n_blan += 1
            if mx - mn >= UMBRAL_COLORIDO_DELTA:
                n_col += 1
                mask_col[y * w + x] = 1
        filas_osc[y] = dr
    return {
        "w": w, "h": h, "total": total, "buf": buf,
        "n_col": n_col, "n_osc": n_osc, "n_blan": n_blan,
        "suma_lum": suma_lum, "filas_osc": filas_osc,
        "mask_col": mask_col, "mask_osc": mask_osc,
    }


# ── Componentes conexas (8-vecinos) sobre una máscara ──────────────────────
def componentes(mask, w, h, min_area=1):
    total = w * h
    visit = bytearray(total)
    comps = []
    for i in range(total):
        if mask[i] and not visit[i]:
            x0 = x1 = i % w
            y0 = y1 = i // w
            area = 0
            pila = [i]
            visit[i] = 1
            while pila:
                p = pila.pop()
                area += 1
                py, px = divmod(p, w)
                if px < x0:
                    x0 = px
                elif px > x1:
                    x1 = px
                if py < y0:
                    y0 = py
                elif py > y1:
                    y1 = py
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        nx = px + dx
                        ny = py + dy
                        if 0 <= nx < w and 0 <= ny < h:
                            j = ny * w + nx
                            if mask[j] and not visit[j]:
                                visit[j] = 1
                                pila.append(j)
            if area >= min_area:
                comps.append({
                    "area": area,
                    "bbox": (x0, y0, x1, y1),
                    "centro": ((x0 + x1) // 2, (y0 + y1) // 2),
                })
    comps.sort(key=lambda c: -c["area"])
    return comps


def componentes_icono(comps, w, h, total):
    """Filtra componentes coloridos 'tipo icono': grandes y compactos."""
    area_min = MENU_COMP_AREA_FRAC * total
    bw_max = MENU_COMP_BBOX_FRAC * w
    bh_max = MENU_COMP_BBOX_FRAC * h
    out = []
    for c in comps:
        x0, y0, x1, y1 = c["bbox"]
        if c["area"] >= area_min and (x1 - x0) <= bw_max and (y1 - y0) <= bh_max:
            out.append(c)
    return out


# ── Bandas 1D (filas con contenido) ────────────────────────────────────────
def bandas(hist, minimo, hueco_min):
    bandas = []
    ini = None
    hueco = 0
    for i, v in enumerate(hist):
        if v >= minimo:
            if ini is None:
                ini = i
            hueco = 0
        elif ini is not None:
            hueco += 1
            if hueco >= hueco_min:
                bandas.append([ini, i - hueco])
                ini = None
                hueco = 0
    if ini is not None:
        bandas.append([ini, len(hist) - 1])
    return bandas


def agrupar(vals, sep):
    """Agrupa valores 1D ordenados separando donde el salto > sep."""
    grupos = []
    if not vals:
        return grupos
    vals = sorted(vals)
    g = [vals[0]]
    for v in vals[1:]:
        if v - g[-1] > sep:
            grupos.append(g)
            g = [v]
        else:
            g.append(v)
    grupos.append(g)
    return grupos


# ── Mapeo de la grilla del MAIN MENU ───────────────────────────────────────
def mapear_grilla(iconos, w, h, buf, filas_osc):
    if not iconos:
        return None
    anchos = [c["bbox"][2] - c["bbox"][0] + 1 for c in iconos]
    altos = [c["bbox"][3] - c["bbox"][1] + 1 for c in iconos]
    med_anch = statistics.median(anchos)
    med_alt = statistics.median(altos)
    sep_cols = max(12, int(0.75 * med_anch))
    sep_filas = max(12, int(0.75 * med_alt))

    grupos_y = agrupar([c["centro"][1] for c in iconos], sep_filas)
    grupos_x = agrupar([c["centro"][0] for c in iconos], sep_cols)
    centros_y = [sum(g) // len(g) for g in grupos_y]
    centros_x = [sum(g) // len(g) for g in grupos_x]

    celdas = []
    for c in iconos:
        fila = min(range(len(centros_y)), key=lambda i: abs(centros_y[i] - c["centro"][1]))
        col = min(range(len(centros_x)), key=lambda i: abs(centros_x[i] - c["centro"][0]))
        x0, y0, x1, y1 = c["bbox"]
        celdas.append({
            "fila": fila + 1,
            "columna": col + 1,
            "bbox": [x0, y0, x1, y1],
            "centro": [c["centro"][0], c["centro"][1]],
            "area": c["area"],
        })
    celdas.sort(key=lambda c: (c["fila"], c["columna"]))

    gx0 = min(c["bbox"][0] for c in iconos)
    gy0 = min(c["bbox"][1] for c in iconos)
    gx1 = max(c["bbox"][2] for c in iconos)
    gy1 = max(c["bbox"][3] for c in iconos)
    ancho_grilla = gx1 - gx0 + 1

    # Bandas de texto (filas con oscuros, tipo texto) fuera de la grilla,
    # contando oscuros solo dentro del rango X de la grilla.
    def filas_texto_rango(y_desde, y_hasta):
        hist = []
        for y in range(y_desde, y_hasta):
            dr = 0
            base = y * w * 3
            for x in range(gx0, gx1 + 1):
                r = buf[base + x * 3]
                g = buf[base + x * 3 + 1]
                b = buf[base + x * 3 + 2]
                if (299 * r + 587 * g + 114 * b) // 1000 < UMBRAL_OSCURO_LUM:
                    dr += 1
            hist.append(dr)
        minimo = max(2, int(0.005 * ancho_grilla))
        tope_texto = 0.6 * ancho_grilla  # fila tipo texto, no bloque sólido
        candidatas = [v if (minimo <= v <= tope_texto) else 0 for v in hist]
        return [[y_desde + a, y_desde + b] for a, b in bandas(candidatas, 1, 2)]

    texto_arriba = filas_texto_rango(0, gy0)
    texto_abajo = filas_texto_rango(gy1 + 1, h)

    return {
        "filas": len(grupos_y),
        "columnas": len(grupos_x),
        "iconos": len(iconos),
        "celdas_esperadas": len(grupos_y) * len(grupos_x),
        "celdas_vacias": len(grupos_y) * len(grupos_x) - len(iconos),
        "bbox_grilla": {"x0": gx0, "y0": gy0, "x1": gx1, "y1": gy1},
        "celdas": celdas,
        "fila_texto": {
            "arriba": texto_arriba,
            "abajo": texto_abajo,
            "n_bandas": len(texto_arriba) + len(texto_abajo),
        },
    }


# ── OCR opcional (solo clase menu) vía tools/ocr_png.ps1 ───────────────────
def ocr_imagen(ruta):
    resultado = {"ejecutado": True, "herramienta": "tools/ocr_png.ps1"}
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ocr_png.ps1")
    if not os.path.isfile(script):
        return {"ejecutado": False, "error": "no se encontró tools/ocr_png.ps1"}
    ruta_ocr = ruta
    try:
        if ruta.lower().endswith(".bmp"):
            dir_conv = os.path.join(tempfile.gettempdir(), "cg50_lcd_reader_ocr")
            os.makedirs(dir_conv, exist_ok=True)
            ruta_ocr = os.path.join(
                dir_conv, os.path.splitext(os.path.basename(ruta))[0] + ".png")
            Image.open(ruta).convert("RGB").save(ruta_ocr, "PNG")
            resultado["imagen_ocr"] = ruta_ocr
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
               "-File", script, "-PngPath", os.path.abspath(ruta_ocr)]
        proc = subprocess.run(cmd, capture_output=True, timeout=120)
        resultado["salida"] = proc.returncode
        try:
            texto = proc.stdout.decode("utf-8")
        except UnicodeDecodeError:
            texto = proc.stdout.decode("latin-1", errors="replace")
        resultado["salida_stdout"] = texto.strip()
        if proc.returncode != 0 or "NO OCR ENGINE" in texto:
            resultado["error"] = ("sin motor OCR (idioma de perfil sin paquete) "
                                  if "NO OCR ENGINE" in texto
                                  else "fallo del proceso OCR")
            return resultado
        lineas_raw = texto.replace("\r\n", "\n").split("\n")
        lenguaje = None
        texto_completo = None
        ocr_lineas = []
        estado = None
        for ln in lineas_raw:
            ln = ln.strip()
            if ln.startswith("OCR lenguaje:"):
                lenguaje = ln.split(":", 1)[1].strip()
            elif ln == "=== TEXTO ===":
                estado = "texto"
                texto_completo = ""
            elif ln == "=== LINEAS ===":
                estado = "lineas"
            elif estado == "texto":
                texto_completo += ln + "\n"
            elif estado == "lineas" and ln:
                ocr_lineas.append(ln)
        resultado["lenguaje"] = lenguaje
        resultado["texto"] = (texto_completo or "").strip()
        resultado["lineas"] = ocr_lineas
        bajos = [ln.lower() for ln in ocr_lineas]
        terminos = [t for t in TERMINOS_MENU
                    if any(t.lower() in bl for bl in bajos)]
        resultado["terminos_menu"] = terminos
        resultado["n_terminos_menu"] = len(terminos)
        resultado["contiene_main_menu"] = any("main menu" in bl for bl in bajos)
    except subprocess.TimeoutExpired:
        resultado["error"] = "timeout (120 s) del proceso OCR"
    except Exception as exc:  # cualquier fallo de OCR no invalida la lectura
        resultado["error"] = f"excepción OCR: {exc}"
    return resultado


# ── Clasificación ───────────────────────────────────────────────────────────
def clasificar(c, comps_icono, mayor_comp_oscura):
    """c: censo. Devuelve (clase, confianza, nota)."""
    w, h, total = c["w"], c["h"], c["total"]
    r_color = c["n_col"] / total
    r_osc = c["n_osc"] / total
    r_blan = c["n_blan"] / total
    lum_media = c["suma_lum"] / total
    aspecto = w / h
    n_iconos = len(comps_icono)

    # 1) menu
    if r_color >= MENU_R_COLOR_MIN and n_iconos >= MENU_COMPS_MIN:
        conf = (0.6 * _acotar((r_color - MENU_R_COLOR_MIN) / 0.15)
                + 0.4 * _acotar((n_iconos - MENU_COMPS_MIN) / 8.0))
        return "menu", round(conf, 2), None

    # 2) boot_blanca
    if (r_color <= BOOT_R_COLOR_MAX and r_blan >= BOOT_R_BLANCO_MIN
            and (mayor_comp_oscura is not None)):
        frac_mayor = mayor_comp_oscura / total
        r_osc_fuera = max(0.0, r_osc - frac_mayor)
        con_bisel = frac_mayor >= BOOT_MAYOR_COMP_OSCURA_MIN
        limpia = (r_osc <= BOOT_R_OSCURO_MAX_LIMPIO
                  and BOOT_ASPECTO_MIN <= aspecto <= BOOT_ASPECTO_MAX)
        if con_bisel or limpia:
            conf = (0.5 * _acotar((r_blan - BOOT_R_BLANCO_MIN) / 0.30)
                    + 0.5 * _acotar(1.0 - r_osc_fuera / BOOT_R_OSCURO_MAX_LIMPIO))
            nota = None
            if r_osc > 0.15:
                nota = ("captura con bisel/estructura oscura alrededor del LCD "
                        "blanco; el censo global incluye el marco")
            return "boot_blanca", round(conf, 2), nota

    # 3) texto_denso
    if r_osc >= TEXTO_R_OSCURO_MIN and r_blan >= TEXTO_R_BLANCO_MIN:
        conf = (0.5 * _acotar((r_osc - TEXTO_R_OSCURO_MIN) / 0.12)
                + 0.5 * _acotar((r_blan - TEXTO_R_BLANCO_MIN) / 0.30))
        return "texto_denso", round(conf, 2), None

    # 4) desconocida + notas de diagnóstico
    if r_blan < 0.05 and lum_media < 40:
        nota = ("imagen casi negra (¿LCD apagado o captura sin pantalla "
                "iluminada?)")
    elif r_osc > 0.60:
        nota = ("imagen dominada por fondo oscuro (¿captura de ventana/canvas "
                "completa en vez del LCD aislado?)")
    elif aspecto > 4.0 or aspecto < 1.2:
        nota = (f"aspecto {aspecto:.2f} muy distinto al LCD 396×224 (≈1.77); "
                "¿recorte parcial de la pantalla?")
    else:
        nota = ("sin rasgos de menú/blanca/texto con los umbrales actuales")
    return "desconocida", 0.25, nota


# ── Principal ───────────────────────────────────────────────────────────────
def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="cg50_lcd_reader.py",
        description="Lector de pantalla CG50: imagen del LCD → JSON estructurado.",
        epilog=("Clases: menu | boot_blanca | texto_denso | desconocida. "
                "Umbrales y limitaciones en la cabecera del script."))
    parser.add_argument("imagen", help="ruta de la imagen PNG/BMP del LCD")
    parser.add_argument("--out", metavar="DIR",
                        help="guarda copia del JSON en DIR\\lcd-<ts>.json")
    parser.add_argument("--ocr", action="store_true",
                        help="OCR (Windows.Media.Ocr) si clase == menu")
    args = parser.parse_args(argv)

    ruta = args.imagen
    if not os.path.isfile(ruta):
        print(f"ERROR: no se encontró la imagen: {ruta}", file=sys.stderr)
        return 3
    try:
        img = Image.open(ruta)
        formato = img.format or "?"
        modo_origen = img.mode
        rgb = img.convert("RGB")
    except Exception as exc:
        print(f"ERROR: no se pudo cargar la imagen: {exc}", file=sys.stderr)
        return 3

    w, h = rgb.size
    c = censo(rgb)
    total = c["total"]
    r_color = c["n_col"] / total
    r_osc = c["n_osc"] / total
    r_blan = c["n_blan"] / total
    lum_media = c["suma_lum"] / total
    aspecto = w / h

    # Componentes coloridos: solo si hay censo colorido alto (candidato menu)
    comps_col = []
    comps_icono = []
    if r_color >= MENU_R_COLOR_MIN:
        comps_col = componentes(c["mask_col"], w, h, min_area=1)
        comps_icono = componentes_icono(comps_col, w, h, total)

    # Componente oscura mayor: solo si hay candidato boot_blanca
    mayor_comp_oscura = None
    if r_color <= BOOT_R_COLOR_MAX and r_blan >= BOOT_R_BLANCO_MIN:
        comps_osc = componentes(c["mask_osc"], w, h, min_area=1)
        if comps_osc:
            mayor_comp_oscura = comps_osc[0]["area"]

    clase, confianza, nota = clasificar(c, comps_icono, mayor_comp_oscura)

    celdas = None
    if clase == "menu":
        celdas = mapear_grilla(comps_icono, w, h, c["buf"], c["filas_osc"])

    ocr = None
    if args.ocr and clase == "menu":
        ocr = ocr_imagen(ruta)
    elif args.ocr:
        ocr = {"ejecutado": False,
               "motivo": "OCR solo se ejecuta para clase == menu"}

    ahora = datetime.now(timezone.utc)
    ts = ahora.strftime("%Y-%m-%dT%H:%M:%SZ")
    ts_archivo = ahora.strftime("%Y%m%d-%H%M%S-") + f"{ahora.microsecond // 1000:03d}"

    salida = {
        "archivo": ruta,
        "ruta_absoluta": os.path.abspath(ruta),
        "formato": formato,
        "modo_origen": modo_origen,
        "tamano": {"ancho": w, "alto": h},
        "clase": clase,
        "confianza": confianza,
        "pixeles": {
            "total": total,
            "coloridos": c["n_col"],
            "oscuros": c["n_osc"],
            "blancos": c["n_blan"],
        },
        "ratios": {
            "coloridos": round(r_color, 4),
            "oscuros": round(r_osc, 4),
            "blancos": round(r_blan, 4),
        },
        "celdas_detectadas": celdas,
        "ocr": ocr,
        "nota": nota,
        "detalle": {
            "lum_media": round(lum_media, 1),
            "aspecto": round(aspecto, 3),
            "aspecto_cg50_esperado": round(LCD_CG50["ancho"] / LCD_CG50["alto"], 3),
            "componentes_coloridos": len(comps_col) if comps_col else None,
            "componentes_icono": len(comps_icono) if comps_col else None,
            "mayor_comp_oscura_frac": (round(mayor_comp_oscura / total, 4)
                                       if mayor_comp_oscura is not None else None),
            "bandas_texto_global": len(bandas(c["filas_osc"],
                                               max(2, int(0.005 * w)), 2)),
            "umbrales": {
                "colorido_delta_rgb": UMBRAL_COLORIDO_DELTA,
                "oscuro_lum": UMBRAL_OSCURO_LUM,
                "blanco_lum": UMBRAL_BLANCO_LUM,
                "menu_r_color_min": MENU_R_COLOR_MIN,
                "menu_comps_min": MENU_COMPS_MIN,
                "boot_r_color_max": BOOT_R_COLOR_MAX,
                "boot_r_blanco_min": BOOT_R_BLANCO_MIN,
                "boot_mayor_comp_oscura_min": BOOT_MAYOR_COMP_OSCURA_MIN,
                "texto_r_oscuro_min": TEXTO_R_OSCURO_MIN,
                "texto_r_blanco_min": TEXTO_R_BLANCO_MIN,
            },
        },
        "ts": ts,
        "version": VERSION,
        "herramienta": "tools/cg50_lcd_reader.py",
    }

    texto_json = json.dumps(salida, indent=2, ensure_ascii=True)
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    print(texto_json)

    if args.out:
        try:
            os.makedirs(args.out, exist_ok=True)
            destino = os.path.join(args.out, f"lcd-{ts_archivo}.json")
            with open(destino, "w", encoding="utf-8") as f:
                f.write(texto_json + "\n")
            print(f"JSON guardado en: {destino}", file=sys.stderr)
        except Exception as exc:
            print(f"ERROR: no se pudo guardar --out: {exc}", file=sys.stderr)
            return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
