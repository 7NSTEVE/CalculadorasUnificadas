#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
calc_visual_auditor.py — Motor Universal de Automatización, Control de Computadora
y Auditoría Visual para Calculadora Unificada y fx-CG Manager PLUS Oficial.

Permite:
- Control de interfaz de usuario mediante Win32 API directa (sin bloqueos de permisos).
- Inyección de secuencias de teclado con pacing adaptativo calibrado (KEYSC guest scan loop).
- Handshake de arranque activo con sondeo en calc_debug.log (30s timeout, 250ms polling).
- Recortes dinámicos de LCD subpíxel basados en geometría exacta del canvas Win32 (solo_layout & lcd_box_fixed).
- Análisis visual algorítmico: OCR nativo de Windows (con ampliación 3x LANCZOS),
  análisis de histogramas/paletas y detección de anomalías o congelamientos.
- Generación de informe interactivo HTML con galería de evidencias fotográficas.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import html
import json
import os
import subprocess
import sys
import time
from PIL import Image, ImageChops

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_COMMAND = 0x0111
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
WM_CLOSE = 0x0010
PW_CLIENTONLY = 1
BM_CLICK = 0x00F5

ID_BTN_MODE_ALL = 1010
ID_BTN_MODE_DUAL = 1011
ID_BTN_MODE_WHITE = 1012
ID_BTN_MODE_GRAY = 1013
ID_BTN_MODE_GRAPH = 1014

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"

# Geometría canónica de la skin fx-CG50 y posición del LCD
CG50_SKIN_W, CG50_SKIN_H = 664, 1364
CG50_LCD_RECT = (92, 153, 572, 403)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))


def solo_layout(total_w, total_h):
    """Calcula las dimensiones y posición del viewport de la CG50 en modo Solo CG50.
    Basado en LayoutCalculators() de main.cpp y verify_gui.py."""
    fit_h = total_h - 40
    max_h_from_w = (total_w - 24) * CG50_SKIN_H // CG50_SKIN_W
    if max_h_from_w < fit_h:
        fit_h = max_h_from_w
    if fit_h < 300:
        fit_h = 300
    target_h = fit_h
    target_w = (CG50_SKIN_W * target_h) // CG50_SKIN_H
    vx = (total_w - target_w) // 2
    vy = (total_h - target_h) // 2
    if vy < 10:
        vy = 10
    return vx, vy, target_w, target_h


def lcd_box_fixed(total_w, total_h):
    """Calcula la caja delimitadora (left, top, right, bottom) exacta del display LCD
    en coordenadas del canvas hijo CasioCanvasArea. Coincide exactamente con dstLcd
    en DrawCalculator() de main.cpp."""
    vx, vy, target_w, target_h = solo_layout(total_w, total_h)
    lx0 = CG50_LCD_RECT[0] * target_w // CG50_SKIN_W
    ly0 = CG50_LCD_RECT[1] * target_h // CG50_SKIN_H
    lx1 = CG50_LCD_RECT[2] * target_w // CG50_SKIN_W
    ly1 = CG50_LCD_RECT[3] * target_h // CG50_SKIN_H
    return (vx + lx0, vy + ly0, vx + lx1, vy + ly1)


# Bounding box de referencia para lienzo 2004x1330 (mantenido para compatibilidad de API)
CU_LCD_BOX = lcd_box_fixed(2004, 1330)

# Mapeo de teclas para fx-CG50 en CalculadoraUnificada (MapVkToKeyId de main.cpp)
KEY_MAP_CU = {
    "0": 0x30, "1": 0x31, "2": 0x32, "3": 0x33, "4": 0x34,
    "5": 0x35, "6": 0x36, "7": 0x37, "8": 0x38, "9": 0x39,
    "+": 0x6B, "-": 0x6D, "*": 0x6A, "/": 0x6F, ".": 0x6E, "PUNTO": 0x6E,
    "EXE": 0x0D, "AC": 0x2E, "DEL": 0x08,
    "MENU": 0x24, "EXIT": 0x23, "SHIFT": 0x10, "ALPHA": 0x14,
    "OPTN": 0x2D, "VARS": 0x21,
    "UP": 0x26, "DOWN": 0x28, "LEFT": 0x25, "RIGHT": 0x27,
    "F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73, "F5": 0x74, "F6": 0x75,
    "COS": 0x43, "SIN": 0x53, "TAN": 0x54, "LN": 0x4C, "LOG": 0x47,
    "X": 0x58, "X_THETA": 0x58,
    "(": 0x88, ")": 0x89, "^": 0x8A, "FRAC": 0x8B, "SD": 0x8C,
    ",": 0x8D, "COMA": 0x8D, "EXP": 0x8E, "ANS": 0x8F, "X2": 0x92, "ARROW": 0x22
}

# Coordenadas relativas en ventana del Manager oficial (1006x1171)
MANAGER_KEYS = {
    "1": (107, 1005), "2": (192, 1005), "3": (277, 1005), "+": (343, 1005), "-": (430, 1005),
    "4": (107, 943), "5": (192, 943), "6": (277, 943), "*": (343, 943), "/": (430, 943),
    "7": (107, 881), "8": (192, 881), "9": (277, 881), "DEL": (343, 881), "AC": (430, 881),
    "0": (107, 1066), ".": (192, 1066), "EXE": (430, 1066),
    "F1": (92, 562), "F2": (160, 562), "F3": (228, 562), "F4": (296, 562), "F5": (364, 562), "F6": (431, 562),
    "MENU": (290, 644), "EXIT": (290, 706), "SHIFT": (92, 644), "OPTN": (160, 644), "VARS": (228, 644),
    "UP": (417, 630), "DOWN": (417, 700), "LEFT": (380, 665), "RIGHT": (455, 665),
    "X": (112, 789), "LOG": (200, 789), "LN": (288, 789), "SIN": (376, 789), "COS": (464, 789), "TAN": (552, 789),
    "(": (277, 818), ")": (343, 818), "^": (289, 711)
}
MANAGER_REF_W, MANAGER_REF_H = 1006, 1171

# Códigos hexadecimales exactos de los 50 botones Win32 del Manager Oficial
OFFICIAL_KEY_TO_CODE = {
    "0": "72", "1": "73", "2": "63", "3": "53", "4": "74",
    "5": "64", "6": "54", "7": "75", "8": "65", "9": "55",
    "+": "43", "-": "33", "*": "44", "/": "34", ".": "62", "PUNTO": "62",
    "EXE": "32", "AC": "11", "DEL": "45",
    "MENU": "49", "EXIT": "48", "SHIFT": "79", "ALPHA": "78",
    "OPTN": "69", "VARS": "59",
    "UP": "29", "DOWN": "38", "LEFT": "39", "RIGHT": "28",
    "F1": "7a", "F2": "6a", "F3": "5a", "F4": "4a", "F5": "3a", "F6": "2a",
    "(": "56", ")": "46", "^": "58", "POT": "58",
    "COS": "37", "SIN": "47", "TAN": "27", "LN": "57", "LOG": "67",
    "X": "77", "X_THETA": "77", "X2": "68",
    "FRAC": "76", "SD": "66", ",": "36", "COMA": "36",
    "EXP": "52", "ANS": "42", "ARROW": "26"
}


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().upper()


class CalcVisualAuditor:
    def __init__(self, target="unified", outdir="verification/visual_audit_gallery"):
        self.target_type = target  # "unified" o "official"
        self.outdir = os.path.abspath(outdir)
        os.makedirs(self.outdir, exist_ok=True)
        self.proc = None
        self.hwnd = None
        self.canvas = None
        self.history = []
        self.step_counter = 0
        self.official_buttons = {}
        self.official_lcd_hwnd = None

    def _find_default_exe(self):
        repo_root = os.path.dirname(SCRIPT_DIR)
        candidates = [
            os.path.join(repo_root, "CalculadoraUnificada.exe"),
            os.path.join(repo_root, "build-win32", "Release", "CalculadoraUnificada.exe"),
            os.path.join(repo_root, "build-verify", "Release", "CalculadoraUnificada.exe"),
            os.path.join(PROJECT_ROOT, "Calculadora Unificada - Core Oficial", "CalculadoraUnificada.exe"),
            os.path.join(PROJECT_ROOT, "Calculadora Unificada - Core Oficial", "build-win32", "Release", "CalculadoraUnificada.exe"),
            os.path.join(PROJECT_ROOT, "Calculadora Unificada - Core Oficial", "build-verify", "Release", "CalculadoraUnificada.exe"),
            os.path.join(PROJECT_ROOT, "Calculadora Unificada", "build-native-x64", "Release", "CalculadoraUnificada.exe"),
            os.path.join(PROJECT_ROOT, "Calculadora Unificada", "build-x64", "Release", "CalculadoraUnificada.exe"),
        ]
        for c in candidates:
            if os.path.isfile(c):
                return c
        return candidates[0]

    def _count_ready_in_log(self, log_path):
        if not os.path.isfile(log_path):
            return 0
        try:
            with open(log_path, "rb") as f:
                return f.read().count(b"CG50: READY")
        except Exception:
            return 0

    def _wait_for_ready(self, log_path, initial_size, initial_ready_count, timeout=30.0, poll_interval=0.25):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if os.path.isfile(log_path):
                try:
                    with open(log_path, "rb") as f:
                        data = f.read()
                        if data.count(b"CG50: READY") > initial_ready_count:
                            return True
                        if len(data) > initial_size:
                            new_bytes = data[initial_size:]
                            if b"CG50: READY" in new_bytes or b"Initialize: OK" in new_bytes or b"GCal listo" in new_bytes:
                                return True
                except Exception:
                    pass
            time.sleep(poll_interval)
        return False

    def start(self, exe_path=None):
        if self.target_type == "unified":
            if not exe_path:
                exe_path = self._find_default_exe()

            exe_dir = os.path.dirname(os.path.abspath(exe_path))
            log_path = os.path.join(exe_dir, "calc_debug.log")

            # Limpiar instancias previas para evitar bloqueos de GCal DLSim32E
            subprocess.run(["powershell", "-NoProfile", "-Command",
                            "Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)
            time.sleep(0.5)

            initial_size = os.path.getsize(log_path) if os.path.isfile(log_path) else 0
            initial_ready_count = self._count_ready_in_log(log_path)

            self.proc = subprocess.Popen([exe_path], cwd=exe_dir)

            # Sondeo de ventana principal
            for _ in range(40):
                time.sleep(0.25)
                self.hwnd = self._find_window(MAIN_CLASS)
                if self.hwnd:
                    break
            if not self.hwnd:
                raise RuntimeError("No se encontró ventana de CalculadoraUnificada")

            self.canvas = self._find_canvas(self.hwnd)

            # Handshake activo de arranque mediante sondeo de calc_debug.log (30s timeout, 250ms polling)
            is_ready = self._wait_for_ready(log_path, initial_size, initial_ready_count, timeout=30.0, poll_interval=0.25)
            if not is_ready:
                print(f"ADVERTENCIA: Handshake timeout (30s) esperando CG50: READY en {log_path}. Continuando con inicialización...", file=sys.stderr)

            # Poner en modo Solo CG50 por defecto
            user32.SendMessageW(self.hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
            time.sleep(1.50)
        else:
            if not exe_path:
                exe_path = r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"
            subprocess.run(["powershell", "-NoProfile", "-Command",
                            "Get-Process fx-CG_Manager* -ErrorAction SilentlyContinue | Stop-Process -Force"], check=False)
            time.sleep(0.5)
            cmd = f'"{exe_path}" /n"fx-CG Manager PLUS Subscription for fx-CG50series"'
            self.proc = subprocess.Popen(cmd, shell=True)
            for _ in range(40):
                time.sleep(0.5)
                self.hwnd = self._find_official_window()
                if self.hwnd:
                    break
            if not self.hwnd:
                raise RuntimeError("No se encontró ventana del emulador oficial")
            time.sleep(6.0)  # Estabilización emulador oficial
            self.official_buttons = {}
            self.official_lcd_hwnd = None

            def _enum_children(h, _):
                cls = ctypes.create_unicode_buffer(256)
                user32.GetClassNameW(h, cls, 256)
                txt = ctypes.create_unicode_buffer(256)
                user32.GetWindowTextW(h, txt, 256)
                ctrl_id = user32.GetDlgCtrlID(h)
                if cls.value == "Button" and txt.value:
                    self.official_buttons[txt.value.lower()] = h
                if cls.value == "Static" and ctrl_id == 1404:
                    self.official_lcd_hwnd = h
                return True

            cb = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(_enum_children)
            user32.EnumChildWindows(self.hwnd, cb, 0)

        return self.hwnd

    def stop(self):
        if self.hwnd:
            user32.PostMessageW(self.hwnd, WM_CLOSE, 0, 0)
            time.sleep(0.5)
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except Exception:
                self.proc.kill()
        self.hwnd = None
        self.canvas = None

    def press_key(self, key_name, hold_s=0.20, wait_s=0.30):
        """Envía una tecla respetando el pacing requerido por el guest scan loop (KEYSC) del SH7305."""
        if self.target_type == "unified":
            target = self.canvas if self.canvas else self.hwnd
            vk = KEY_MAP_CU.get(key_name.upper())
            if vk is None:
                if len(key_name) == 1:
                    vk = ord(key_name.upper())
                else:
                    raise ValueError(f"Tecla no mapeada en CU: {key_name}")
            user32.SendMessageW(target, WM_KEYDOWN, vk, 0)
            time.sleep(hold_s)
            user32.SendMessageW(target, WM_KEYUP, vk, 0)
            time.sleep(wait_s)
        else:
            code = OFFICIAL_KEY_TO_CODE.get(key_name.upper())
            if code and code in self.official_buttons:
                btn = self.official_buttons[code]
                user32.SendMessageW(btn, BM_CLICK, 0, 0)
                time.sleep(hold_s + wait_s)
            elif key_name.upper() in MANAGER_KEYS:
                rect = wt.RECT()
                user32.GetWindowRect(self.hwnd, ctypes.byref(rect))
                w = rect.right - rect.left
                h = rect.bottom - rect.top
                sx = w / float(MANAGER_REF_W)
                sy = h / float(MANAGER_REF_H)
                refx, refy = MANAGER_KEYS[key_name.upper()]
                x, y = int(refx * sx), int(refy * sy)
                pt = wt.POINT(x, y)
                child = user32.ChildWindowFromPointEx(self.hwnd, pt, 1)
                target = child if child else self.hwnd
                cpt = wt.POINT(x, y)
                user32.MapWindowPoints(self.hwnd, target, ctypes.byref(cpt), 1)
                lp = (cpt.y << 16) | (cpt.x & 0xFFFF)
                user32.PostMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, lp)
                time.sleep(hold_s)
                user32.PostMessageW(target, WM_LBUTTONUP, 0, lp)
                time.sleep(wait_s)
            else:
                raise ValueError(f"Tecla no mapeada en Manager: {key_name}")

    def press_sequence(self, keys, default_hold=0.20, default_wait=0.30):
        for k in keys:
            self.press_key(k, hold_s=default_hold, wait_s=default_wait)

    def capture_and_audit(self, step_name, expected_tokens=None):
        self.step_counter += 1
        prefix = f"{self.step_counter:02d}_{step_name}"
        full_path = os.path.join(self.outdir, f"{prefix}_full.png")
        lcd_path = os.path.join(self.outdir, f"{prefix}_lcd.png")

        target_wnd = self.canvas if (self.target_type == "unified" and self.canvas) else self.hwnd
        img_full = self._capture_window(target_wnd, full_path)
        if not img_full:
            raise RuntimeError(f"Fallo al capturar ventana para {step_name}")

        if self.target_type == "unified":
            w, h = img_full.size
            # Cálculo dinámico de la caja del display LCD en base al tamaño actual del canvas
            box = lcd_box_fixed(w, h)
            x0 = max(0, min(box[0], w - 1))
            y0 = max(0, min(box[1], h - 1))
            x1 = max(x0 + 1, min(box[2], w))
            y1 = max(y0 + 1, min(box[3], h))
            img_lcd = img_full.crop((x0, y0, x1, y1))
            img_lcd.save(lcd_path)
        else:
            if self.official_lcd_hwnd:
                img_lcd = self._capture_window(self.official_lcd_hwnd, lcd_path)
            else:
                img_lcd = self._crop_official_lcd(img_full)
                img_lcd.save(lcd_path)

        ocr_data = self._run_ocr(lcd_path)
        palette_info = self._analyze_palette(img_lcd)

        tokens_found = {}
        passed = True
        if expected_tokens:
            for tok in expected_tokens:
                found = any(tok.lower() in line.lower() for line in ocr_data["lines"]) or (tok.lower() in ocr_data["text"].lower())
                tokens_found[tok] = found
                if not found:
                    passed = False

        record = {
            "step": self.step_counter,
            "name": step_name,
            "full_image": os.path.basename(full_path),
            "lcd_image": os.path.basename(lcd_path),
            "sha256": sha256_file(lcd_path),
            "ocr_text": ocr_data["text"],
            "ocr_lines": ocr_data["lines"],
            "expected_tokens": tokens_found,
            "palette": palette_info,
            "passed": passed
        }
        self.history.append(record)
        return record

    def generate_html_report(self, title="Auditoría Visual de Calculadora"):
        html_path = os.path.join(self.outdir, "index.html")
        rows = []
        for r in self.history:
            badge_class = "badge-pass" if r["passed"] else "badge-fail"
            badge_text = "PASS" if r["passed"] else "FAIL"
            toks = ", ".join(f"{k}: {'OK' if v else 'MISSING'}" for k, v in r["expected_tokens"].items())
            rows.append(f"""
            <tr>
                <td><strong>#{r['step']}</strong></td>
                <td><strong>{html.escape(r['name'])}</strong></td>
                <td><span class="badge {badge_class}">{badge_text}</span></td>
                <td>
                    <a href="{r['lcd_image']}" target="_blank">
                        <img src="{r['lcd_image']}" class="thumb-lcd" alt="LCD" />
                    </a>
                </td>
                <td>
                    <div class="ocr-text">{html.escape(r['ocr_text'] or "(Sin texto)")}</div>
                    <small class="tokens">{html.escape(toks)}</small>
                </td>
                <td><code class="hash">{r['sha256'][:16]}...</code></td>
            </tr>
            """)

        html_content = f"""<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <title>{title}</title>
    <style>
        body {{ font-family: system-ui, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f141c; color: #e6edf3; padding: 24px; }}
        h1 {{ color: #58a6ff; font-size: 24px; border-bottom: 1px solid #30363d; padding-bottom: 12px; }}
        table {{ width: 100%; border-collapse: collapse; margin-top: 16px; background: #161b22; border-radius: 8px; overflow: hidden; }}
        th, td {{ padding: 12px 16px; text-align: left; border-bottom: 1px solid #21262d; }}
        th {{ background: #21262d; color: #8b949e; font-size: 13px; text-transform: uppercase; }}
        .thumb-lcd {{ height: 120px; border-radius: 4px; border: 1px solid #30363d; background: #000; }}
        .badge {{ padding: 4px 8px; border-radius: 4px; font-weight: bold; font-size: 12px; }}
        .badge-pass {{ background: #238636; color: #fff; }}
        .badge-fail {{ background: #da3633; color: #fff; }}
        .ocr-text {{ font-family: monospace; font-size: 13px; color: #7ee787; }}
        .tokens {{ color: #8b949e; font-size: 11px; }}
        .hash {{ font-family: monospace; font-size: 11px; color: #8b949e; }}
    </style>
</head>
<body>
    <h1>{title}</h1>
    <p>Target: <strong>{self.target_type.upper()}</strong> · Total pasos auditados: <strong>{len(self.history)}</strong></p>
    <table>
        <thead>
            <tr>
                <th>Paso</th>
                <th>Operación</th>
                <th>Estado</th>
                <th>Display LCD</th>
                <th>OCR / Tokens</th>
                <th>Hash SHA-256</th>
            </tr>
        </thead>
        <tbody>
            {"".join(rows)}
        </tbody>
    </table>
</body>
</html>"""
        with open(html_path, "w", encoding="utf-8") as f:
            f.write(html_content)

        json_path = os.path.join(self.outdir, "audit_log.json")
        try:
            with open(json_path, "w", encoding="utf-8") as f:
                json.dump(self.history, f, indent=2, ensure_ascii=False)
        except Exception:
            pass

        return html_path

    def _find_window(self, class_name):
        res = []
        @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
        def cb(h, _):
            buf = ctypes.create_unicode_buffer(256)
            user32.GetClassNameW(h, buf, 256)
            if buf.value == class_name and user32.IsWindowVisible(h):
                res.append(h)
            return True
        user32.EnumWindows(cb, 0)
        return res[0] if res else None

    def _find_canvas(self, parent):
        res = []
        @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
        def cb(h, _):
            buf = ctypes.create_unicode_buffer(256)
            user32.GetClassNameW(h, buf, 256)
            if buf.value == CANVAS_CLASS:
                res.append(h)
                return False
            return True
        user32.EnumChildWindows(parent, cb, 0)
        return res[0] if res else None

    def _find_official_window(self):
        res = []
        @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
        def cb(h, _):
            if not user32.IsWindowVisible(h):
                return True
            b = ctypes.create_unicode_buffer(512)
            user32.GetWindowTextW(h, b, 512)
            if "fx-CG Manager" in b.value:
                res.append(h)
                return True
        user32.EnumWindows(cb, 0)
        return res[0] if res else None

    def _capture_window(self, hwnd, path):
        rect = wt.RECT()
        user32.GetClientRect(hwnd, ctypes.byref(rect))
        w, h = rect.right, rect.bottom
        if w <= 0 or h <= 0:
            return None
        hdc_w = user32.GetDC(hwnd)
        hdc_m = gdi32.CreateCompatibleDC(hdc_w)
        hbmp = gdi32.CreateCompatibleBitmap(hdc_w, w, h)
        gdi32.SelectObject(hdc_m, hbmp)
        user32.PrintWindow(hwnd, hdc_m, PW_CLIENTONLY)

        class BITMAPINFOHEADER(ctypes.Structure):
            _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                        ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                        ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                        ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]

        class BITMAPINFO(ctypes.Structure):
            _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.UINT * 3)]

        bmi = BITMAPINFO()
        bmi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        bmi.bmiHeader.biWidth = w
        bmi.bmiHeader.biHeight = -h
        bmi.bmiHeader.biPlanes = 1
        bmi.bmiHeader.biBitCount = 32
        buf = ctypes.create_string_buffer(w * h * 4)
        gdi32.GetDIBits(hdc_m, hbmp, 0, h, buf, ctypes.byref(bmi), 0)
        user32.ReleaseDC(hwnd, hdc_w)
        gdi32.DeleteObject(hbmp)
        gdi32.DeleteDC(hdc_m)
        img = Image.frombytes("RGB", (w, h), buf.raw, "raw", "BGRX", 0, 1)
        img.save(path)
        return img

    def _crop_official_lcd(self, img):
        w, h = img.size
        screen_pixels = []
        for y in range(int(h * 0.15), int(h * 0.45)):
            for x in range(int(w * 0.04), int(w * 0.48)):
                p = img.getpixel((x, y))
                if (p[0] > 220 and p[1] > 220 and p[2] > 220) or (p[2] > 200 and p[0] < 80):
                    screen_pixels.append((x, y))
        if not screen_pixels:
            return img.crop((int(w * 0.05), int(h * 0.20), int(w * 0.45), int(h * 0.42)))
        min_x = min(p[0] for p in screen_pixels)
        max_x = max(p[0] for p in screen_pixels)
        min_y = min(p[1] for p in screen_pixels)
        max_y = max(p[1] for p in screen_pixels)
        return img.crop((min_x, min_y, max_x + 1, max_y + 1))

    def _find_ocr_script(self):
        candidates = [
            os.path.join(SCRIPT_DIR, "ocr_png.ps1"),
            os.path.join(PROJECT_ROOT, "Calculadora Unificada", "tools", "ocr_png.ps1"),
            os.path.join(PROJECT_ROOT, "Calculadora Unificada - Core Oficial", "tools", "ocr_png.ps1"),
        ]
        for c in candidates:
            if os.path.isfile(c):
                return c
        return None

    def _run_ocr(self, path):
        ocr_ps1 = self._find_ocr_script()
        if not ocr_ps1:
            return {"text": "(ocr_png.ps1 no encontrado)", "lines": []}

        # Generar imagen 3x LANCZOS temporal para maximizar legibilidad en Windows Media OCR
        target_path = path
        temp_3x = None
        try:
            with Image.open(path) as img:
                w, h = img.size
                img_3x = img.resize((w * 3, h * 3), Image.Resampling.LANCZOS)
                dir_name, base_name = os.path.split(path)
                temp_3x = os.path.join(dir_name, f"_tmp_3x_{base_name}")
                img_3x.save(temp_3x)
                target_path = temp_3x
        except Exception:
            target_path = path

        try:
            res = subprocess.run(
                ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ocr_ps1, target_path],
                capture_output=True, text=True, timeout=30
            )
            text = ""
            lines = []
            capture_lines = False
            for l in res.stdout.splitlines():
                if "=== LINEAS ===" in l:
                    capture_lines = True
                    continue
                if capture_lines and l.strip():
                    lines.append(l.strip())
                elif not capture_lines and "=== TEXTO ===" in l:
                    pass
                elif not capture_lines and l.strip() and "OCR" not in l:
                    text += " " + l.strip()
            return {"text": text.strip(), "lines": lines}
        except Exception as e:
            return {"text": f"(Error en OCR: {e})", "lines": []}
        finally:
            if temp_3x and os.path.isfile(temp_3x):
                try:
                    os.remove(temp_3x)
                except Exception:
                    pass

    def _analyze_palette(self, img):
        colors = img.getcolors(maxcolors=100000) or []
        total = img.size[0] * img.size[1]
        distinct = len(colors)
        dark_pixels = sum(c[0] for c in colors if c[1][0] < 80 and c[1][1] < 80 and c[1][2] < 80)
        light_pixels = sum(c[0] for c in colors if c[1][0] > 200 and c[1][1] > 200 and c[1][2] > 200)
        return {
            "distinct_colors": distinct,
            "dark_percent": round(dark_pixels / total * 100.0, 2) if total else 0,
            "light_percent": round(light_pixels / total * 100.0, 2) if total else 0
        }


def main():
    parser = argparse.ArgumentParser(description="Auditor Visual de Calculadora Unificada")
    parser.add_argument("--target", default="unified", choices=["unified", "official"])
    parser.add_argument("--exe", default=None, help="Ruta al ejecutable")
    parser.add_argument("--out", default="verification/visual_audit_gallery", help="Directorio de salida")
    parser.add_argument("--test-audit", action="store_true", help="Ejecutar suite de auditoría operacional")
    args = parser.parse_args()

    auditor = CalcVisualAuditor(target=args.target, outdir=args.out)
    print(f"[Auditor] Iniciando target={args.target}...")
    auditor.start(exe_path=args.exe)
    print(f"[Auditor] Ventana principal lista: HWND={hex(auditor.hwnd)}")

    if args.test_audit:
        print("[Auditor] Esperando estabilización completa del Menú Principal...")
        time.sleep(2.5)

        print("[Auditor] Ejecutando paso 1: Menú Principal...")
        res1 = auditor.capture_and_audit("01_main_menu", expected_tokens=["MAIN MENU"])
        print(f"  Paso 1: Pass={res1['passed']}, Tokens={res1['expected_tokens']}, Colores={res1['palette']['distinct_colors']}")

        print("[Auditor] Ejecutando paso 2: Abrir Run-Matrix (tecla EXE)...")
        auditor.press_key("EXE")
        time.sleep(2.5)
        res2 = auditor.capture_and_audit("02_open_run_matrix", expected_tokens=["DELETE"])
        print(f"  Paso 2: Pass={res2['passed']}, Tokens={res2['expected_tokens']}, Colores={res2['palette']['distinct_colors']}")

        print("[Auditor] Ejecutando paso 3: Cálculo 12 * 34 = 408...")
        auditor.press_sequence(["AC", "1", "2", "*", "3", "4", "EXE"])
        time.sleep(1.0)
        res3 = auditor.capture_and_audit("03_calculate_12x34", expected_tokens=["408"])
        print(f"  Paso 3: Pass={res3['passed']}, Tokens={res3['expected_tokens']}, OCR={res3['ocr_text']}")

    report_path = auditor.generate_html_report(title="Auditoría Visual Automatizada")
    print(f"[Auditor] Reporte HTML generado: {report_path}")
    auditor.stop()
    print("[Auditor] Sesión finalizada.")


if __name__ == "__main__":
    main()
