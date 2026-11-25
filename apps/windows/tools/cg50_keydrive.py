#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cg50_keydrive.py — Driver de UI de Calculadora Unificada (G-5) + escenario legacy.

MODO G-5 (v2.0): secuencias de teclas físicas sobre la ventana real
`CasioNativeMain` (EnumWindows vía ctypes + SendMessageW WM_KEYDOWN/WM_KEYUP).
La secuencia se define en JSON o por CLI y cada paso lleva vk, hold_ms y wait_ms.
Opcionalmente captura el canvas en pasos clave llamando a tools/verify_gui.py.

  JSON (--sequence): lista de pasos o
    {
      "name": "aritmetica",
      "defaults": {"hold_ms": 5000, "wait_ms": 1000},
      "steps": [
        {"vk": "VK_HOME", "label": "MENU", "capture": "menu"},
        {"vk": "VK_RETURN"},
        {"vk": 0x31, "label": "1"},
        {"vk": "VK_ADD"},
        {"vk": "VK_2"},
        {"vk": "VK_RETURN", "capture": "resultado"}
      ]
    }
  CLI (--keys): "VK_HOME,VK_RETURN:5000:1000,VK_1,VK_ADD,VK_2,VK_RETURN"
    Cada token es vk[:hold_ms[:wait_ms]]; los defaults salen de --hold-ms /
    --wait-ms o de 5000/1000 ms. Acepta nombres VK_*, códigos 0xNN, dígitos
    "0".."9", letras "A".."Z" y alias MENU, EXE, AC, ARRIBA, DERECHA, etc.

Modos sin efectos secundarios:
  --dry-run    valida la secuencia y muestra el plan (VK, tecla lógica, matriz
               CG50 esperada y tiempos) SIN buscar ventana ni enviar nada.
  --self-test  valida el mapeo de main.cpp (extrae MapVkToKeyId con regex),
               la tabla de cg50_keys_exact.h y que cada VK del plan exista.
  Ambos escriben informe JSON en --out si se indica y devuelven 0/1/2.

Ejecución real:
  python tools/cg50_keydrive.py --sequence seq.json --out <dir> [--exe ruta]
  - NO lanza la GUI: exige que CasioNativeMain ya esté corriendo (si no, exit 2).
  - Conmuta a vista Solo-CG50 (WM_COMMAND 1014) salvo --no-cg50-mode.
  - Los holds deben ser largos (>= 5 s; el core emula ~6-12 M instr/s reales y
    las teclas se entregan en ventanas de sleep). Un hold más corto es error
    de validación salvo --allow-short-holds.
  - Capturas: `"capture": "tag"` en el JSON, o --capture-after 1,3,5 / --capture-each.
    Cada captura ejecuta verify_gui.py y guarda PNG/JSON en <out>/captures/.

MODO LEGACY (compatibilidad, sin --sequence/--keys/--dry-run/--self-test):
  escenario 'aritmetica' del paso 2 de la batería 5.6, con clics sintéticos
  WM_LBUTTONDOWN/UP sobre las coordenadas del skin y OCR.

Escenario 'aritmetica' (Run-Matrix):
  A1  MAIN MENU visible (colorful >= 20000) antes de tocar nada.
  A2  EXE abre la app resaltada: el recorte LCD cambia de SHA-256.
  A3  1 + 2: OCR lee '1+2' y la banda derecha del resultado gana tinta
      (EXE evaluó; el dígito pequeño no lo lee el OCR, queda en PNG).
  A4  AC; 5 + 4 EXE -> idem con '5+4' en la banda de la segunda línea.
  A5  MENU -> el OCR encuentra de nuevo 'MAIN MENU'.

Uso: python tools/cg50_keydrive.py --out <dir> [--exe <ruta>] [--keep-open]
Exit 0 = PASS; 1 = FAIL; 2 = error de infraestructura.

v1.1: DPI-aware (la ventana real es física; sin esto el layout y los clics
quedan desplazados/escalados), matching OCR estricto de tokens (evita
falsos positivos tipo "3" dentro de "3D Graph") y guardia de geometría
entre el layout calculado y el LCD detectado antes de enviar clics.
v1.2: A1 exige LCD alineado + colorido + OCR que confirme "MAIN MENU"
antes de enviar cualquier clic (el guest tarda en bootear; con el LCD
gris/vacío la detección de blanco puede confundirse con el teclado).
v1.3: el recorte LCD usa la caja del layout verificado (el detector de
blanco abarca también rótulos del teclado y contamina el bounding box);
la detección queda solo como evidencia en el JSON.
v1.4: tabla de códigos corregida contra gint (((KO-1)<<4)|(8-KI); ver
cg50_keys_exact.h, 2026-10-17); token OCR endurecido para dígitos: rechaza
"F 3:"/"F3:" (etiquetas de softkeys; el falso positivo de kd4) y "3:".
v1.5: el OCR de Windows no lee la fuente pequeña del resultado (probado con
región ampliada, padding y binarizado: solo lee texto grande). A3/A4 combinan
OCR de la expresión ('1+2'/'5+4') + detección de tinta en la banda derecha
del resultado; las capturas PNG conservan el valor (kd5: 3 y 9 visibles).
v2.0 (G-5, 2026-10-19): driver genérico de secuencias VK (JSON/CLI) con
SendMessageW, --dry-run con matriz esperada, --self-test contra main.cpp y
cg50_keys_exact.h, y capturas opcionales vía verify_gui.py. El escenario
legacy del paso 2 se conserva para compatibilidad.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import os
import re
import subprocess
import sys
import time

from PIL import Image, ImageChops

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

user32.SendMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.SendMessageW.restype = wt.LPARAM
user32.IsWindow.argtypes = [wt.HWND]
user32.IsWindow.restype = wt.BOOL
user32.MapVirtualKeyW.argtypes = [wt.UINT, wt.UINT]
user32.MapVirtualKeyW.restype = wt.UINT
user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, wt.INT]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetWindowThreadProcessId.restype = wt.DWORD
user32.PrintWindow.argtypes = [wt.HWND, wt.HDC, wt.UINT]
user32.PrintWindow.restype = wt.BOOL
user32.GetDC.argtypes = [wt.HWND]
user32.GetDC.restype = wt.HDC
user32.ReleaseDC.argtypes = [wt.HWND, wt.HDC]
user32.ReleaseDC.restype = wt.INT
gdi32.CreateCompatibleDC.argtypes = [wt.HDC]
gdi32.CreateCompatibleDC.restype = wt.HDC
gdi32.CreateCompatibleBitmap.argtypes = [wt.HDC, wt.INT, wt.INT]
gdi32.CreateCompatibleBitmap.restype = wt.HBITMAP
gdi32.SelectObject.argtypes = [wt.HDC, wt.HGDIOBJ]
gdi32.SelectObject.restype = wt.HGDIOBJ
gdi32.DeleteObject.argtypes = [wt.HGDIOBJ]
gdi32.DeleteObject.restype = wt.BOOL
gdi32.DeleteDC.argtypes = [wt.HDC]
gdi32.DeleteDC.restype = wt.BOOL
gdi32.GetDIBits.argtypes = [wt.HDC, wt.HBITMAP, wt.UINT, wt.UINT, wt.LPVOID, wt.LPVOID, wt.UINT]
gdi32.GetDIBits.restype = wt.INT

WM_COMMAND = 0x0111
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
BN_CLICKED = 0
ID_BTN_MODE_GRAPH = 1014
PW_CLIENTONLY = 1
BI_RGB = 0
DIB_RGB_COLORS = 0
MAPVK_VK_TO_VSC = 0

MAIN_CLASS = "CasioNativeMain"
CANVAS_CLASS = "CasioCanvasArea"
CG50_SKIN_W, CG50_SKIN_H = 664, 1364

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_EXE = os.path.join(PROJECT_ROOT, "build-verify", "Release", "CalculadoraUnificada.exe")
OCR_PS1 = os.path.join(PROJECT_ROOT, "tools", "ocr_png.ps1")
VERIFY_GUI = os.path.join(PROJECT_ROOT, "tools", "verify_gui.py")
MAIN_CPP = os.path.join(PROJECT_ROOT, "main.cpp")
CG50_KEYS_H = os.path.join(PROJECT_ROOT, "cg50_keys_exact.h")

VERSION = "2.0 (G-5: secuencias VK JSON/CLI + dry-run + self-test + capturas)"
DEFAULT_HOLD_MS = 5000
DEFAULT_WAIT_MS = 1000
MIN_HOLD_MS = 5000
MAX_STEP_MS = 600000

# Centros exactos del skin (cg50_keys_exact.h); matrixCode solo informativo
# (valores corregidos 2026-10-17: gint ((KO-1)<<4)|(8-KI)).
KEYS = {
    "1": (121, 1120, 0x0703), "2": (226, 1120, 0x0603), "3": (332, 1120, 0x0503),
    "4": (121, 1038, 0x0704), "5": (226, 1038, 0x0604), "6": (332, 1038, 0x0504),
    "7": (121, 956, 0x0705), "8": (226, 956, 0x0605), "9": (332, 956, 0x0505),
    "0": (121, 1202, 0x0702), "PUNTO": (226, 1202, 0x0602),
    "MAS": (437, 1120, 0x0403), "MENOS": (542, 1120, 0x0303),
    "POR": (437, 1038, 0x0404), "ENTRE": (542, 1038, 0x0304),
    "EXE": (542, 1202, 0x0302), "AC": (542, 956, 0x0101),
    "MENU": (376, 642, 0x0409), "EXIT": (376, 711, 0x0408),
}


# ── G-5: tabla VK, parseo de fuentes y planificador ─────────────────────────

VK_CODES = {
    "VK_LBUTTON": 0x01, "VK_RBUTTON": 0x02, "VK_CANCEL": 0x03,
    "VK_MBUTTON": 0x04, "VK_XBUTTON1": 0x05, "VK_XBUTTON2": 0x06,
    "VK_BACK": 0x08, "VK_TAB": 0x09, "VK_CLEAR": 0x0C, "VK_RETURN": 0x0D,
    "VK_SHIFT": 0x10, "VK_CONTROL": 0x11, "VK_MENU": 0x12, "VK_PAUSE": 0x13,
    "VK_CAPITAL": 0x14, "VK_ESCAPE": 0x1B, "VK_SPACE": 0x20,
    "VK_PRIOR": 0x21, "VK_NEXT": 0x22, "VK_END": 0x23, "VK_HOME": 0x24,
    "VK_LEFT": 0x25, "VK_UP": 0x26, "VK_RIGHT": 0x27, "VK_DOWN": 0x28,
    "VK_SELECT": 0x29, "VK_PRINT": 0x2A, "VK_EXECUTE": 0x2B,
    "VK_SNAPSHOT": 0x2C, "VK_INSERT": 0x2D, "VK_DELETE": 0x2E, "VK_HELP": 0x2F,
    "VK_LWIN": 0x5B, "VK_RWIN": 0x5C, "VK_APPS": 0x5D, "VK_SLEEP": 0x5F,
    "VK_MULTIPLY": 0x6A, "VK_ADD": 0x6B, "VK_SEPARATOR": 0x6C,
    "VK_SUBTRACT": 0x6D, "VK_DECIMAL": 0x6E, "VK_DIVIDE": 0x6F,
    "VK_NUMLOCK": 0x90, "VK_SCROLL": 0x91,
    "VK_LSHIFT": 0xA0, "VK_RSHIFT": 0xA1, "VK_LCONTROL": 0xA2,
    "VK_RCONTROL": 0xA3, "VK_LMENU": 0xA4, "VK_RMENU": 0xA5,
    "VK_OEM_1": 0xBA, "VK_OEM_PLUS": 0xBB, "VK_OEM_COMMA": 0xBC,
    "VK_OEM_MINUS": 0xBD, "VK_OEM_PERIOD": 0xBE, "VK_OEM_2": 0xBF,
    "VK_OEM_3": 0xC0, "VK_OEM_4": 0xDB, "VK_OEM_5": 0xDC,
    "VK_OEM_6": 0xDD, "VK_OEM_7": 0xDE, "VK_OEM_8": 0xDF,
    "VK_OEM_102": 0xE2,
}
for _i in range(10):
    VK_CODES[f"VK_{_i}"] = 0x30 + _i
    VK_CODES[f"VK_NUMPAD{_i}"] = 0x60 + _i
for _i in range(26):
    VK_CODES[f"VK_{chr(65 + _i)}"] = 0x41 + _i
for _i in range(1, 25):
    VK_CODES[f"VK_F{_i}"] = 0x6F + _i

VK_ALIASES = {
    "MENU": 0x24, "EXE": 0x0D, "ENTER": 0x0D, "RETURN": 0x0D,
    "AC": 0x1B, "AC/ON": 0x1B, "ON": 0x1B,
    "UP": 0x26, "ARRIBA": 0x26, "LEFT": 0x25, "IZQUIERDA": 0x25,
    "RIGHT": 0x27, "DERECHA": 0x27, "DOWN": 0x28, "ABAJO": 0x28,
    "OPTN": 0x2D, "VARS": 0x21, "REPAG": 0x21, "EXIT": 0x23,
    "FLECHA": 0x22, "AVPAG": 0x22, "ALPHA": 0x14, "SHIFT": 0x10,
    "DEL": 0x2E, "DELETE": 0x2E, "PERIOD": 0xBE, "PUNTO": 0xBE,
}
for _i in range(1, 25):
    VK_ALIASES[f"F{_i}"] = 0x6F + _i

_VK_NAME_CACHE = {}
for _name, _code in VK_CODES.items():
    _VK_NAME_CACHE.setdefault(_code, _name)

EXTENDED_VKS = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                0x2B, 0x2C, 0x2D, 0x2E, 0x5B, 0x5C, 0x5D, 0x6F,
                0x90, 0x91, 0xA3, 0xA5}

CRITICAL_VK_EXPECT = {
    "VK_HOME": 9, "VK_RETURN": 49, "VK_ESCAPE": 34,
    "VK_F1": 0, "VK_F2": 1, "VK_F3": 2, "VK_F4": 3, "VK_F5": 4, "VK_F6": 5,
    "VK_UP": 10, "VK_LEFT": 11, "VK_RIGHT": 12, "VK_DOWN": 13,
    "VK_SHIFT": 6, "VK_CAPITAL": 14, "VK_INSERT": 7, "VK_PRIOR": 8,
    "VK_END": 17, "VK_NEXT": 29,
    "VK_0": 45, "VK_1": 40, "VK_2": 41, "VK_3": 42, "VK_4": 35,
    "VK_5": 36, "VK_6": 37, "VK_7": 30, "VK_8": 31, "VK_9": 32,
    "VK_ADD": 43, "VK_SUBTRACT": 44, "VK_MULTIPLY": 38, "VK_DIVIDE": 39,
    "VK_DECIMAL": 46, "VK_OEM_PERIOD": 46, "VK_BACK": 33, "VK_DELETE": 34,
}

KEYID_NAME_EXPECT = {
    0: "F1", 1: "F2", 2: "F3", 3: "F4", 4: "F5", 5: "F6",
    6: "SHIFT", 7: "OPTN", 8: "VARS", 9: "MENU",
    10: "ARRIBA", 11: "IZQUIERDA", 12: "DERECHA", 13: "ABAJO",
    14: "ALPHA", 17: "EXIT", 29: "FLECHA", 33: "DEL", 34: "AC",
    30: "7", 31: "8", 32: "9", 35: "4", 36: "5", 37: "6",
    40: "1", 41: "2", 42: "3", 43: "MAS", 44: "MENOS",
    45: "0", 46: "PUNTO", 38: "POR", 39: "ENTRE", 49: "EXE",
}

CG50_ONLY_VKS = {0x10, 0x14, 0x24, 0x2D, 0x21, 0x23, 0x22}

DEFAULT_DEMO_STEPS = [
    ("VK_HOME", "MENU", "menu"),
    ("VK_RETURN", "EXE", "run-matrix"),
    ("VK_1", "1", None),
    ("VK_ADD", "+", None),
    ("VK_2", "2", "typed"),
    ("VK_RETURN", "EXE", "result"),
]


def vk_name(vk):
    return _VK_NAME_CACHE.get(int(vk), f"0x{int(vk):02X}")


def decode_vk(token):
    if isinstance(token, bool):
        raise ValueError("booleano no es un VK válido")
    if isinstance(token, int):
        vk = token
    else:
        text = str(token).strip()
        if not text:
            raise ValueError("VK vacío")
        if re.fullmatch(r"0[xX][0-9A-Fa-f]+", text):
            vk = int(text, 16)
        elif len(text) == 1 and text.isalnum():
            vk = ord(text.upper())
        elif re.fullmatch(r"\d+", text):
            vk = int(text, 10)
        else:
            name = text.upper().replace(" ", "_").replace("-", "_")
            if name in VK_CODES:
                vk = VK_CODES[name]
            elif name in VK_ALIASES:
                vk = VK_ALIASES[name]
            elif len(text) == 1 and text.isalnum():
                vk = ord(text.upper())
            else:
                raise ValueError(f"VK desconocido: {token!r}")
    if not (0 < int(vk) < 256):
        raise ValueError(f"VK fuera de rango 1..255: {vk}")
    return int(vk)


def builtin_keymap():
    mapping = {
        0x30: 45, 0x31: 40, 0x32: 41, 0x33: 42, 0x34: 35,
        0x35: 36, 0x36: 37, 0x37: 30, 0x38: 31, 0x39: 32,
        0x60: 45, 0x61: 40, 0x62: 41, 0x63: 42, 0x64: 35,
        0x65: 36, 0x66: 37, 0x67: 30, 0x68: 31, 0x69: 32,
        0x6B: 43, 0x6D: 44, 0x6A: 38, 0x6F: 39, 0x0D: 49,
        0x6E: 46, 0xBE: 46, 0x08: 33, 0x2E: 34, 0x1B: 34,
        0x26: 10, 0x25: 11, 0x27: 12, 0x28: 13,
        0x10: 6, 0x14: 14, 0x24: 9, 0x2D: 7, 0x21: 8,
        0x23: 17, 0x22: 29,
    }
    for i in range(6):
        mapping[0x70 + i] = i
    return mapping


def load_cg50_key_table(path=CG50_KEYS_H):
    if not os.path.isfile(path):
        return {}, f"tabla de teclas no encontrada: {path}"
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError as exc:
        return {}, f"no se pudo leer {path}: {exc}"
    start = text.find("g_cg50ExactKeys[]")
    if start < 0:
        return {}, "g_cg50ExactKeys[] no encontrada en cg50_keys_exact.h"
    end = text.find("};", start)
    if end < 0:
        return {}, "cierre de g_cg50ExactKeys[] no encontrado"
    pattern = re.compile(
        r'\{\s*(\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*'
        r'(-?\d+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*L"([^"]+)"\s*\}')
    table = {}
    for m in pattern.finditer(text[start:end]):
        kid = int(m.group(1))
        table[kid] = {
            "id": kid, "cx": int(m.group(2)), "cy": int(m.group(3)),
            "w": int(m.group(4)), "h": int(m.group(5)),
            "matrix": int(m.group(6), 16), "name": m.group(7),
        }
    if not table:
        return {}, "no se extrajo ninguna entrada de g_cg50ExactKeys[]"
    return table, None


def _parse_case_label(label):
    text = label.strip()
    if len(text) >= 3 and text[0] == "'" and text[-1] == "'":
        inner = text[1:-1]
        if len(inner) == 1:
            return ord(inner)
        return None
    name = text.upper()
    if name in VK_CODES:
        return VK_CODES[name]
    return None


def load_main_keymap(path=MAIN_CPP):
    if not os.path.isfile(path):
        return {}, set(), f"main.cpp no encontrado: {path}"
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError as exc:
        return {}, set(), f"no se pudo leer main.cpp: {exc}"

    start = text.find("int MapVkToKeyId")
    if start < 0:
        return {}, set(), "MapVkToKeyId no encontrada en main.cpp"
    end = text.find("// Shared physical-key handlers", start)
    if end < 0:
        end = text.find("void HandlePhysicalKeyDown", start)
    if end < 0:
        end = len(text)
    body = text[start:end]
    if "if (calcIdx == 2)" not in body:
        return {}, set(), "bloque 'if (calcIdx == 2)' no encontrado en MapVkToKeyId"

    mapping = {}
    cg50_only = set()
    pending = []
    in_cg50 = False
    for line in body.splitlines():
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        if "if (calcIdx == 2)" in line:
            in_cg50 = True
        for m in re.finditer(r"case\s+([^:]+):", line):
            pending.append(m.group(1).strip())
        kid = None
        m_tern = re.search(r"return\s+calcIdx\s*==\s*2\s*\?\s*(\d+)\s*:\s*(\d+)", line)
        m_ret = re.search(r"return\s+(-?\d+)\s*;", line)
        if m_tern:
            kid = int(m_tern.group(1))
        elif m_ret:
            kid = int(m_ret.group(1))
        if kid is not None:
            for lab in pending:
                vk = _parse_case_label(lab)
                if vk is None:
                    continue
                mapping[vk] = kid
                if in_cg50:
                    cg50_only.add(vk)
            pending = []
    if not mapping:
        return {}, set(), "MapVkToKeyId no produjo ningún par VK->tecla"
    return mapping, cg50_only, None


def sanitize_tag(tag):
    clean = re.sub(r"[^A-Za-z0-9._-]+", "-", str(tag)).strip("-._")
    return clean or "capture"


def _as_int(value, field, errors, default=None):
    if value is None:
        return default
    try:
        if isinstance(value, bool):
            raise ValueError
        return int(value)
    except (TypeError, ValueError):
        errors.append(f"{field} debe ser entero: {value!r}")
        return default


def _normalize_step(raw, defaults, index, errors):
    if not isinstance(raw, dict):
        errors.append(f"paso {index}: debe ser un objeto JSON")
        return None
    unknown = set(raw) - {"vk", "key", "label", "hold_ms", "wait_ms",
                          "capture", "comment", "description"}
    if unknown:
        errors.append(f"paso {index}: claves desconocidas {sorted(unknown)}")
    vk_token = raw.get("vk", raw.get("key"))
    capture = raw.get("capture")
    if isinstance(capture, bool):
        capture = f"step-{index:02d}" if capture else None
    if capture is not None:
        capture = sanitize_tag(capture)
    if vk_token is None and capture is None:
        errors.append(f"paso {index}: necesita 'vk' o 'capture'")
        return None
    hold = _as_int(raw.get("hold_ms"), f"paso {index} hold_ms", errors,
                   defaults["hold_ms"])
    wait = _as_int(raw.get("wait_ms"), f"paso {index} wait_ms", errors,
                   defaults["wait_ms"])
    return {
        "index": index,
        "vk_token": vk_token,
        "label": raw.get("label") or raw.get("description") or "",
        "comment": raw.get("comment") or "",
        "hold_ms": 0 if vk_token is None else hold,
        "wait_ms": 0 if vk_token is None else wait,
        "capture": capture,
    }


def load_sequence(args):
    result = {
        "name": "secuencia",
        "source": "cli",
        "steps": [],
        "defaults": {
            "hold_ms": DEFAULT_HOLD_MS if args.hold_ms is None else args.hold_ms,
            "wait_ms": DEFAULT_WAIT_MS if args.wait_ms is None else args.wait_ms,
        },
        "errors": [],
        "warnings": [],
    }
    errors = result["errors"]

    if args.sequence:
        path = args.sequence
        result["source"] = f"json:{path}"
        try:
            with open(path, "r", encoding="utf-8") as f:
                doc = json.load(f)
        except (OSError, json.JSONDecodeError) as exc:
            errors.append(f"no se pudo leer la secuencia JSON: {exc}")
            return result
        if isinstance(doc, list):
            doc = {"steps": doc}
        if not isinstance(doc, dict):
            errors.append("la raíz del JSON debe ser un objeto o una lista de pasos")
            return result
        result["name"] = str(doc.get("name") or os.path.splitext(os.path.basename(path))[0])
        defaults = doc.get("defaults") or {}
        if not isinstance(defaults, dict):
            errors.append("'defaults' debe ser un objeto")
            defaults = {}
        if args.hold_ms is None and defaults.get("hold_ms") is not None:
            result["defaults"]["hold_ms"] = _as_int(
                defaults.get("hold_ms"), "defaults.hold_ms", errors, DEFAULT_HOLD_MS)
        if args.wait_ms is None and defaults.get("wait_ms") is not None:
            result["defaults"]["wait_ms"] = _as_int(
                defaults.get("wait_ms"), "defaults.wait_ms", errors, DEFAULT_WAIT_MS)
        raw_steps = doc.get("steps")
        if not isinstance(raw_steps, list) or not raw_steps:
            errors.append("'steps' debe ser una lista no vacía")
            return result
    elif args.keys:
        result["source"] = "cli"
        raw_steps = []
        parts = [p.strip() for p in re.split(r"[,;\n]+", args.keys) if p.strip()]
        for part in parts:
            chunks = part.split(":")
            raw = {"vk": chunks[0]}
            if len(chunks) > 1 and chunks[1].strip():
                raw["hold_ms"] = chunks[1].strip()
            if len(chunks) > 2 and chunks[2].strip():
                raw["wait_ms"] = chunks[2].strip()
            if len(chunks) > 3:
                errors.append(f"--keys: token con demasiados campos: {part!r}")
            raw_steps.append(raw)
    else:
        result["source"] = "demo"
        result["name"] = "demo-aritmetica"
        raw_steps = [{"vk": vk, "label": label, "capture": cap}
                     for vk, label, cap in DEFAULT_DEMO_STEPS]

    for idx, raw in enumerate(raw_steps, start=1):
        step = _normalize_step(raw, result["defaults"], idx, errors)
        if step is not None:
            result["steps"].append(step)

    capture_after = set()
    if args.capture_after:
        for token in re.split(r"[,\s]+", args.capture_after.strip()):
            if not token:
                continue
            try:
                n = int(token)
            except ValueError:
                errors.append(f"--capture-after: índice no entero {token!r}")
                continue
            if n < 1 or n > len(result["steps"]):
                errors.append(f"--capture-after: índice {n} fuera de 1..{len(result['steps'])}")
            else:
                capture_after.add(n)
    for step in result["steps"]:
        if args.capture_each and step["index"] in capture_after:
            errors.append(f"paso {step['index']}: --capture-each y --capture-after se solapan")
        if args.capture_each or step["index"] in capture_after:
            if not step["capture"]:
                vk_part = str(step["vk_token"]).lower().replace("_", "")
                step["capture"] = sanitize_tag(f"step-{step['index']:02d}-{vk_part}")

    seen = set()
    for step in result["steps"]:
        if step["capture"]:
            if step["capture"] in seen:
                errors.append(f"captura duplicada: {step['capture']}")
            seen.add(step["capture"])
    return result


def enrich_plan(sequence, keymap, cg50_only, key_table, allow_short_holds):
    errors = []
    warnings = []
    plan = []
    cumulative = 0
    seen_cg50_only = False
    for step in sequence["steps"]:
        entry = dict(step)
        entry["vk"] = None
        entry["vk_hex"] = None
        entry["vk_name"] = None
        entry["key_id"] = None
        entry["key_name"] = None
        entry["matrix"] = None
        entry["matrix_hex"] = None
        vk_token = step["vk_token"]
        if vk_token is not None:
            try:
                vk = decode_vk(vk_token)
            except ValueError as exc:
                errors.append(f"paso {step['index']}: {exc}")
                vk = None
            if vk is not None:
                entry["vk"] = vk
                entry["vk_hex"] = f"0x{vk:02X}"
                entry["vk_name"] = vk_name(vk)
                kid = keymap.get(vk)
                if kid is None:
                    errors.append(
                        f"paso {step['index']}: {vk_name(vk)} (0x{vk:02X}) no está "
                        f"mapeado en MapVkToKeyId")
                else:
                    entry["key_id"] = kid
                    kdef = key_table.get(kid)
                    if not kdef:
                        errors.append(
                            f"paso {step['index']}: keyID {kid} no existe en "
                            f"cg50_keys_exact.h")
                    else:
                        entry["key_name"] = kdef["name"]
                        entry["matrix"] = kdef["matrix"]
                        entry["matrix_hex"] = f"0x{kdef['matrix']:04X}"
                if vk in cg50_only and not seen_cg50_only:
                    seen_cg50_only = True
                    warnings.append(
                        f"{vk_name(vk)} solo se mapea en el bloque calcIdx==2; "
                        f"se activará la vista Solo-CG50")
            hold = step["hold_ms"]
            wait = step["wait_ms"]
            if hold < 0 or wait < 0:
                errors.append(f"paso {step['index']}: tiempos negativos")
            if hold > MAX_STEP_MS or wait > MAX_STEP_MS:
                errors.append(f"paso {step['index']}: tiempo > {MAX_STEP_MS} ms")
            if hold < MIN_HOLD_MS:
                msg = (f"paso {step['index']}: hold {hold} ms < {MIN_HOLD_MS} ms "
                       f"(el core necesita holds largos)")
                if allow_short_holds:
                    warnings.append(msg + " [permitido por --allow-short-holds]")
                else:
                    errors.append(msg + "; usa --allow-short-holds para forzarlo")
            cumulative += max(0, hold) + max(0, wait)
        entry["cumulative_ms"] = cumulative
        plan.append(entry)
    if not any(step["vk"] is not None for step in plan):
        errors.append("la secuencia no contiene ningún paso con VK")
    return plan, errors, warnings, cumulative


def format_plan(sequence, plan, total_ms):
    lines = [
        f"Secuencia: {sequence['name']}  (fuente: {sequence['source']})",
        "idx  VK              code  keyID  tecla        matriz  hold_ms  wait_ms  acum_ms  captura",
        "---  --------------  ----  -----  -----------  ------  -------  -------  -------  -------",
    ]
    for e in plan:
        if e["vk"] is None:
            lines.append(
                f"{e['index']:>3}  {'(solo captura)':<14}  {'-':<4}  {'-':<5}  "
                f"{'-':<11}  {'-':<6}  {0:>7}  {0:>7}  {e['cumulative_ms']:>7}  "
                f"{e['capture'] or '-'}")
        else:
            lines.append(
                f"{e['index']:>3}  {e['vk_name']:<14}  {e['vk_hex']:<4}  "
                f"{str(e['key_id']):<5}  {(e['key_name'] or '?'):<11}  "
                f"{(e['matrix_hex'] or '?'):<6}  {e['hold_ms']:>7}  "
                f"{e['wait_ms']:>7}  {e['cumulative_ms']:>7}  {e['capture'] or '-'}")
    lines.append(f"Total previsto: {total_ms} ms ({total_ms / 1000.0:.1f} s) + overhead de SendMessageW")
    return "\n".join(lines)


def write_report(out_dir, prefix, payload):
    os.makedirs(out_dir, exist_ok=True)
    ts = time.strftime("%Y%m%d-%H%M%S")
    path = os.path.join(out_dir, f"{prefix}-{ts}.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, indent=2)
    return path


def send_vk_message(hwnd, vk, hold_ms, wait_ms):
    scan = int(user32.MapVirtualKeyW(int(vk), MAPVK_VK_TO_VSC)) & 0xFF
    flags = 1 | (scan << 16)
    if vk in EXTENDED_VKS:
        flags |= 1 << 24
    down_lparam = flags
    up_lparam = flags | 0xC0000000
    t0 = time.perf_counter()
    r_down = user32.SendMessageW(hwnd, WM_KEYDOWN, vk, down_lparam)
    time.sleep(max(0, hold_ms) / 1000.0)
    r_up = user32.SendMessageW(hwnd, WM_KEYUP, vk, up_lparam)
    time.sleep(max(0, wait_ms) / 1000.0)
    return {
        "vk": vk, "vk_name": vk_name(vk), "scan_code": scan,
        "lparam_down": f"0x{down_lparam:08X}",
        "lparam_up": f"0x{up_lparam & 0xFFFFFFFF:08X}",
        "result_down": int(r_down), "result_up": int(r_up),
        "elapsed_ms": int((time.perf_counter() - t0) * 1000),
    }


def capture_with_verify_gui(exe, out_root, tag, ocr=False):
    if not os.path.isfile(VERIFY_GUI):
        return {"tag": tag, "ok": False, "error": f"verify_gui.py no encontrado: {VERIFY_GUI}"}
    cap_dir = os.path.join(out_root, "captures",
                           f"{time.strftime('%Y%m%d-%H%M%S')}-{sanitize_tag(tag)}")
    os.makedirs(cap_dir, exist_ok=True)
    cmd = [sys.executable, VERIFY_GUI, "--out", cap_dir, "--exe", exe]
    if ocr:
        cmd.append("--ocr")
    info = {"tag": tag, "out_dir": cap_dir, "cmd": cmd}
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=180, cwd=PROJECT_ROOT)
    except Exception as exc:
        info.update({"ok": False, "error": repr(exc)})
        return info
    info["exit_code"] = proc.returncode
    info["ok"] = proc.returncode == 0
    try:
        parsed = json.loads(proc.stdout)
        info["verify"] = parsed
        if parsed.get("status") == "FAIL_EMPTY":
            info["ok"] = False
    except (ValueError, TypeError):
        info["stdout_tail"] = (proc.stdout or "")[-800:].strip()
    if proc.stderr:
        info["stderr_tail"] = (proc.stderr or "")[-800:].strip()
    return info


def _mapping_for_plan():
    mapping, cg50_only, err = load_main_keymap()
    if err:
        return builtin_keymap(), set(CG50_ONLY_VKS), f"builtin ({err})"
    return mapping, cg50_only, "main.cpp"


def run_dry_run(args):
    sequence = load_sequence(args)
    keymap, cg50_only, map_source = _mapping_for_plan()
    key_table, table_err = load_cg50_key_table()
    plan, plan_errors, plan_warnings, total_ms = enrich_plan(
        sequence, keymap, cg50_only, key_table, args.allow_short_holds)
    errors = list(sequence["errors"]) + list(plan_errors)
    warnings = list(sequence["warnings"]) + list(plan_warnings)
    if table_err:
        errors.append(table_err)
    if map_source.startswith("builtin"):
        warnings.append(map_source)
    else:
        warnings.append("mapeo VK cargado desde main.cpp")

    print("== cg50_keydrive --dry-run (G-5) ==")
    print(f"main.cpp: {MAIN_CPP}")
    print(f"cg50_keys_exact.h: {CG50_KEYS_H}")
    print(format_plan(sequence, plan, total_ms))
    if warnings:
        print("\nAvisos:")
        for w in warnings:
            print("  - " + w)
    if errors:
        print("\nErrores de validación:")
        for e in errors:
            print("  - " + e)
    verdict = "PASS" if not errors else "FAIL"
    print(f"\nVeredicto dry-run: {verdict} (pasos={len(plan)}, errores={len(errors)}, avisos={len(warnings)})")

    if args.out:
        payload = {
            "herramienta": "tools/cg50_keydrive.py",
            "version": VERSION,
            "modo": "dry-run",
            "veredicto": verdict,
            "secuencia": sequence["name"],
            "fuente_secuencia": sequence["source"],
            "mapeo_fuente": map_source,
            "main_cpp": MAIN_CPP,
            "cg50_keys_h": CG50_KEYS_H,
            "plan": plan,
            "total_ms": total_ms,
            "errores": errors,
            "avisos": warnings,
            "enviado": False,
        }
        path = write_report(args.out, "dryrun", payload)
        print(f"Informe: {path}")
    return 0 if not errors else 1


def run_self_test(args):
    checks = []
    infra_error = None

    def add(name, ok, detail=""):
        checks.append({"check": name, "ok": bool(ok), "detail": detail})

    main_exists = os.path.isfile(MAIN_CPP)
    add("main.cpp localizado", main_exists, MAIN_CPP)
    mapping, cg50_only, map_err = load_main_keymap()
    add("MapVkToKeyId extraído con regex", not map_err and len(mapping) >= 30,
        map_err or f"{len(mapping)} pares VK->keyID (solo-CG50: {len(cg50_only)})")
    if map_err and not main_exists:
        infra_error = map_err

    if not map_err:
        for name, expected in CRITICAL_VK_EXPECT.items():
            vk = VK_CODES[name]
            got = mapping.get(vk)
            add(f"main.cpp {name} -> keyID {expected}", got == expected,
                f"obtenido={got}")
        add("bloque condicional calcIdx==2 presente", len(cg50_only) > 0,
            f"VKs exclusivos CG50: {sorted(vk_name(v) for v in cg50_only)}")

    key_table, table_err = load_cg50_key_table()
    add("cg50_keys_exact.h: 50 teclas extraídas", len(key_table) == 50,
        table_err or f"entradas={len(key_table)}")
    if key_table:
        matrices = [k["matrix"] for k in key_table.values()]
        add("matrices CG50 únicas", len(matrices) == len(set(matrices)),
            f"matrices={len(matrices)} únicas={len(set(matrices))}")
        for kid, expected_name in KEYID_NAME_EXPECT.items():
            got = key_table.get(kid, {}).get("name")
            add(f"cg50_keys_exact.h keyID {kid} = {expected_name}", got == expected_name,
                f"obtenido={got}")
        add("keyID 34 es AC (0x0101)",
            key_table.get(34, {}).get("matrix") == 0x0101,
            f"matriz={key_table.get(34, {}).get('matrix')}")

    if not key_table and table_err:
        infra_error = infra_error or table_err

    sequence = load_sequence(args)
    plan, plan_errors, plan_warnings, total_ms = enrich_plan(
        sequence, mapping if not map_err else builtin_keymap(),
        cg50_only if not map_err else set(CG50_ONLY_VKS),
        key_table, args.allow_short_holds)
    add("secuencia del plan válida", not sequence["errors"] and not plan_errors,
        f"pasos={len(plan)} errores={len(sequence['errors']) + len(plan_errors)}")
    for entry in plan:
        if entry["vk"] is None:
            continue
        add(f"plan VK {entry['vk_name']} existe en main.cpp",
            entry["vk"] in mapping,
            f"0x{entry['vk']:02X} -> keyID {mapping.get(entry['vk'])}")
        if entry["key_id"] is not None:
            add(f"plan VK {entry['vk_name']} -> matriz CG50",
                entry["key_id"] in key_table and entry["matrix"] is not None,
                f"keyID={entry['key_id']} tecla={entry['key_name']} "
                f"matriz={entry['matrix_hex']}")
    expected_total = sum(e["hold_ms"] + e["wait_ms"] for e in plan)
    add("tiempos acumulados consistentes", expected_total == total_ms,
        f"suma={expected_total} acumulado={total_ms}")

    print("== cg50_keydrive --self-test (G-5) ==")
    for c in checks:
        print(f"[{'PASS' if c['ok'] else 'FAIL'}] {c['check']}"
              + (f"  ({c['detail']})" if c["detail"] else ""))
    if plan_warnings:
        print("\nAvisos del plan:")
        for w in plan_warnings:
            print("  - " + w)
    if sequence["errors"] or plan_errors:
        print("\nErrores del plan:")
        for e in list(sequence["errors"]) + list(plan_errors):
            print("  - " + e)
    failed = [c for c in checks if not c["ok"]]
    verdict = "PASS" if not failed and not infra_error else "FAIL"
    print(f"\nVeredicto self-test: {verdict} "
          f"(checks={len(checks)}, fallos={len(failed)})")

    if args.out:
        payload = {
            "herramienta": "tools/cg50_keydrive.py",
            "version": VERSION,
            "modo": "self-test",
            "veredicto": verdict,
            "checks": checks,
            "fallos": len(failed),
            "secuencia": sequence["name"],
            "fuente_secuencia": sequence["source"],
            "plan": plan,
            "plan_errores": plan_errors,
            "plan_avisos": plan_warnings,
            "infra_error": infra_error,
            "total_ms": total_ms,
        }
        path = write_report(args.out, "selftest", payload)
        print(f"Informe: {path}")
    if infra_error and not checks:
        return 2
    return 0 if verdict == "PASS" else 1


def run_sequence(args):
    sequence = load_sequence(args)
    keymap, cg50_only, map_source = _mapping_for_plan()
    key_table, table_err = load_cg50_key_table()
    plan, plan_errors, plan_warnings, total_ms = enrich_plan(
        sequence, keymap, cg50_only, key_table, args.allow_short_holds)
    errors = list(sequence["errors"]) + list(plan_errors)
    warnings = list(sequence["warnings"]) + list(plan_warnings)
    if table_err:
        errors.append(table_err)
    if map_source.startswith("builtin"):
        warnings.append(map_source)
    if errors:
        print("== cg50_keydrive (G-5): secuencia inválida ==")
        print(format_plan(sequence, plan, total_ms))
        for e in errors:
            print("  - " + e)
        return 1
    for w in warnings:
        log(w)

    out_dir = args.out or os.path.join(
        PROJECT_ROOT, "verification", time.strftime("%Y-%m-%d") + "-g5-keydrive")
    os.makedirs(out_dir, exist_ok=True)
    report = {
        "herramienta": "tools/cg50_keydrive.py",
        "version": VERSION,
        "modo": "run",
        "secuencia": sequence["name"],
        "fuente_secuencia": sequence["source"],
        "mapeo_fuente": map_source,
        "plan": plan,
        "total_previsto_ms": total_ms,
        "avisos": warnings,
        "pasos": [],
        "capturas": [],
        "veredicto": "ERROR",
    }

    hwnds = find_visible_windows(MAIN_CLASS)
    if not hwnds:
        report["error"] = ("ventana CasioNativeMain no encontrada; el driver no "
                           "lanza la GUI (arráncala manualmente)")
        path = write_report(out_dir, "run", report)
        print(f"[keydrive] ERROR: {report['error']}")
        print(f"Informe: {path}")
        return 2
    hwnd = hwnds[0]
    report["hwnd"] = f"0x{hwnd:X}"
    log(f"ventana encontrada: 0x{hwnd:X}")
    if not args.no_cg50_mode:
        user32.SendMessageW(hwnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0)
        report["vista_solo_cg50"] = True
        log("vista Solo-CG50 activada (WM_COMMAND 1014)")
        time.sleep(1.5)

    ok = True
    for entry in plan:
        if not user32.IsWindow(hwnd):
            report["error"] = "la ventana desapareció durante la secuencia"
            ok = False
            break
        if entry["vk"] is not None:
            log(f"paso {entry['index']}: {entry['vk_name']} "
                f"(keyID {entry['key_id']} {entry['key_name']}, "
                f"matriz {entry['matrix_hex']}) hold={entry['hold_ms']} ms "
                f"wait={entry['wait_ms']} ms")
            sent = send_vk_message(hwnd, entry["vk"], entry["hold_ms"], entry["wait_ms"])
            sent["step"] = entry["index"]
            report["pasos"].append(sent)
        if entry["capture"]:
            log(f"captura '{entry['capture']}' via verify_gui.py")
            cap = capture_with_verify_gui(args.exe, out_dir, entry["capture"],
                                          args.capture_ocr)
            cap["step"] = entry["index"]
            report["capturas"].append(cap)
            if not cap.get("ok"):
                ok = False
    report["veredicto"] = "PASS" if ok else "FAIL"
    path = write_report(out_dir, "run", report)
    print(f"[keydrive] veredicto: {report['veredicto']}; pasos={len(report['pasos'])}; "
          f"capturas={len(report['capturas'])}")
    print(f"Informe: {path}")
    return 0 if ok else 1


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wt.UINT), ("biWidth", wt.LONG),
        ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
        ("biBitCount", wt.WORD), ("biCompression", wt.UINT),
        ("biSizeImage", wt.UINT), ("biXPelsPerMeter", wt.LONG),
        ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.UINT),
        ("biClrImportant", wt.UINT)
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.UINT * 3)]


def log(msg):
    print("[keydrive] " + msg, file=sys.stderr, flush=True)


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest().upper()


def find_visible_windows(cls):
    result = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == cls and user32.IsWindowVisible(hwnd):
            result.append(hwnd)
        return True

    user32.EnumWindows(cb, 0)
    return result


def find_child(parent, cls):
    result = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, 256)
        if buf.value == cls:
            result.append(hwnd)
            return False
        return True

    user32.EnumChildWindows(parent, cb, 0)
    return result[0] if result else None


def render_window(hwnd, w, h):
    hdc_window = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_window)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_window, w, h)
    gdi32.SelectObject(hdc_mem, hbmp)
    ok = user32.PrintWindow(hwnd, hdc_mem, PW_CLIENTONLY)
    bmi = BITMAPINFO()
    bmi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.bmiHeader.biWidth = w
    bmi.bmiHeader.biHeight = -h
    bmi.bmiHeader.biPlanes = 1
    bmi.bmiHeader.biBitCount = 32
    bmi.bmiHeader.biCompression = BI_RGB
    buf = ctypes.create_string_buffer(w * h * 4)
    got = gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), DIB_RGB_COLORS)
    user32.ReleaseDC(hwnd, hdc_window)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    if not ok or got != h:
        return None
    return Image.frombytes("RGB", (w, h), buf.raw, "raw", "BGRX", 0, 1)


def solo_layout(total_w, total_h):
    fit_h = total_h - 40
    max_h_from_w = (total_w - 24) * CG50_SKIN_H // CG50_SKIN_W
    if max_h_from_w < fit_h:
        fit_h = max_h_from_w
    if fit_h < 300:
        fit_h = 300
    target_h = fit_h
    target_w = CG50_SKIN_W * target_h // CG50_SKIN_H
    vx = (total_w - target_w) // 2
    vy = max(10, (total_h - target_h) // 2)
    return vx, vy, target_w, target_h


def enable_dpi_awareness():
    try:
        shcore = ctypes.WinDLL("shcore", use_last_error=True)
        shcore.SetProcessDpiAwareness.argtypes = [ctypes.c_int]
        shcore.SetProcessDpiAwareness.restype = ctypes.c_long
        if shcore.SetProcessDpiAwareness(2) == 0:  # PROCESS_PER_MONITOR_DPI_AWARE
            return "per_monitor_v2"
    except Exception:
        pass
    try:
        if user32.SetProcessDPIAware():
            return "system"
    except Exception:
        pass
    return "none"


def expected_lcd_box(layout):
    vx, vy, tw, th = layout
    lx0 = vx + 92 * tw // CG50_SKIN_W
    ly0 = vy + 153 * th // CG50_SKIN_H
    lx1 = vx + 572 * tw // CG50_SKIN_W
    ly1 = vy + 403 * th // CG50_SKIN_H
    return (lx0, ly0, lx1, ly1)


def center_offset(bbox, layout):
    exp = expected_lcd_box(layout)
    dx = ((bbox[0] + bbox[2]) - (exp[0] + exp[2])) // 2
    dy = ((bbox[1] + bbox[3]) - (exp[1] + exp[3])) // 2
    return dx, dy


def detect_clear_bbox(img, thr=236, x_min=None, min_row_px=300, min_h=60):
    w, h = img.size
    if x_min is None:
        x_min = w // 3
    x_min = min(x_min, max(0, w - 1))
    region = img.crop((x_min, 0, w, h))
    r, g, b = region.split()
    mask = ImageChops.darker(ImageChops.darker(r, g), b).point(
        lambda v: 255 if v >= thr else 0)
    mw, mh = mask.size
    data = mask.tobytes()
    rows = [y for y in range(mh)
            if data[y * mw:(y + 1) * mw].count(255) >= min_row_px]
    if not rows or (rows[-1] - rows[0] + 1) < min_h:
        return None
    xa, xb = mw, -1
    for y in rows:
        row = data[y * mw:(y + 1) * mw]
        f = row.find(255)
        if 0 <= f < xa:
            xa = f
        l = row.rfind(255)
        if l > xb:
            xb = l
    if xb < 0:
        return None
    return (x_min + xa, rows[0], x_min + xb + 1, rows[-1] + 1)


def count_ready_in_log(log_path):
    if not os.path.isfile(log_path):
        return 0
    try:
        with open(log_path, "rb") as f:
            return f.read().count(b"CG50: READY")
    except Exception:
        return 0


def run_ocr(png_path):
    if not os.path.isfile(OCR_PS1):
        return {"error": "ocr_script_missing"}
    cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
           "-File", OCR_PS1, "-PngPath", png_path]
    try:
        proc = subprocess.run(cmd, capture_output=True, timeout=45)
        raw = proc.stdout.decode("utf-8", errors="replace")
        text = "\n".join(line.strip() for line in raw.splitlines() if line.strip())
        return {"exit_code": proc.returncode, "raw_output": text}
    except Exception as e:
        return {"error": str(e)}


def close_app():
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-Process -Name CalculadoraUnificada -ErrorAction SilentlyContinue | "
                    "ForEach-Object { $null = $_.CloseMainWindow(); if (-not $_.WaitForExit(12000)) "
                    "{ Stop-Process -Id $_.Id -Force } }"],
                   capture_output=True)
    time.sleep(1.0)


def click_key(canvas, layout, keyname):
    vx, vy, tw, th = layout
    cx, cy, matrix = KEYS[keyname]
    x = vx + cx * tw // CG50_SKIN_W
    y = vy + cy * th // CG50_SKIN_H
    lparam = (y << 16) | (x & 0xFFFF)
    user32.SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lparam)
    time.sleep(0.12)
    user32.SendMessageW(canvas, WM_LBUTTONUP, 0, lparam)
    log(f"clic {keyname} (matrix 0x{matrix:04X}) en cliente ({x},{y})")
    time.sleep(0.40)


def capture_lcd(canvas, cw, ch, out_dir, tag, lcd_box):
    img = render_window(canvas, cw, ch)
    if img is None:
        return None, {}
    full_png = os.path.join(out_dir, f"lcd-full-{tag}.png")
    img.save(full_png)
    det = detect_clear_bbox(img)
    bbox = tuple(lcd_box)
    crop = img.crop(bbox)
    crop_png = os.path.join(out_dir, f"lcd-crop-{tag}.png")
    crop.save(crop_png)
    crop3x = crop.resize((crop.width * 3, crop.height * 3), Image.LANCZOS)
    crop3x_png = os.path.join(out_dir, f"lcd-crop-3x-{tag}.png")
    crop3x.save(crop3x_png)
    pixels = list(crop.getdata())
    total = len(pixels)
    lit = sum(1 for p in pixels if max(p) > 24)
    colorful = sum(1 for p in pixels if (max(p) - min(p)) > 40)
    info = {
        "tag": tag,
        "bbox": list(bbox), "crop_source": "geometria_layout",
        "bbox_detectado": [int(v) for v in det] if det else None,
        "crop_sha256": sha256_bytes(crop.tobytes()),
        "lit_pixels": lit, "colorful_pixels": colorful,
        "total_pixels": total,
    }
    return crop3x_png, info


def token_found(text, token):
    # Rechaza etiquetas de softkeys ("F 3:", "F3:", "3:") y digitos pegados a
    # letras/numeros (falso positivo A3 de kd4 con "F 3: Import/Export").
    pat = (r"(?<!F )(?<!F  )(?<![0-9A-Z])" + re.escape(token.upper())
           + r"(?![0-9A-Z:])")
    return re.search(pat, text) is not None


def ocr_tokens(out_dir, tag, crop3x_png, tokens, results):
    ocr = run_ocr(crop3x_png)
    text = (ocr.get("raw_output") or "").upper()
    found = {t: token_found(text, t) for t in tokens}
    if tokens and not any(found.values()):
        # reintento sobre la mitad inferior (zona de resultado)
        img = Image.open(crop3x_png)
        lower = img.crop((0, img.height // 2, img.width, img.height))
        lower_png = os.path.join(out_dir, f"lcd-crop-3x-{tag}-lower.png")
        lower.save(lower_png)
        ocr2 = run_ocr(lower_png)
        text2 = (ocr2.get("raw_output") or "").upper()
        for t in tokens:
            if token_found(text2, t):
                found[t] = True
        ocr["retry_lower"] = ocr2
    results[tag] = {"tokens": tokens, "found": found, "ocr": ocr}
    return found


RESULT_INK_MIN = 120


def result_ink(out_dir, tag, base_crop_png, y0, y1, results):
    """Cuenta píxeles oscuros en la banda derecha donde el guest dibuja el
    resultado (fuente pequeña que el OCR de Windows ignora: verificado
    2026-10-17 con región ampliada, padding y binarizado)."""
    img = Image.open(base_crop_png).convert("RGB")
    w, h = img.size
    band = img.crop((int(w * 0.80), int(h * y0), int(w * 0.965), int(h * y1)))
    png = os.path.join(out_dir, f"lcd-result-{tag}.png")
    band.save(png)
    dark = sum(1 for p in band.getdata() if min(p) < 100)
    results[f"result-{tag}"] = {
        "banda_y": [y0, y1], "dark_pixels": dark,
        "dark_pixels_min": RESULT_INK_MIN, "png": png,
    }
    return dark


def run_legacy_scenario(args):
    """Escenario 'aritmetica' (paso 2 batería 5.6), conservado sin cambios.

    Lanza la app, conduce clics sobre el skin y verifica con OCR. Se activa
    cuando no hay --sequence/--keys/--dry-run/--self-test/--legacy y se pasó
    --out (compatibilidad con la invocación histórica del paso 2).
    """
    dpi_mode = enable_dpi_awareness()
    os.makedirs(args.out, exist_ok=True)
    ts = time.strftime("%Y%m%d-%H%M%S")
    log_path = os.path.join(os.path.dirname(args.exe), "calc_debug.log")
    resultado = {"herramienta": "tools/cg50_keydrive.py", "version": "1.5 (paso 2, DPI + OCR + crop por layout + result-ink)",
                 "ts": ts, "exe": args.exe, "dpi_awareness": dpi_mode,
                 "aserciones": {}, "capturas": {}, "ocr": {}, "clave_enviada": []}

    close_app()
    n0 = count_ready_in_log(log_path)
    subprocess.Popen([args.exe], cwd=os.path.dirname(args.exe))
    hwnds = []
    for _ in range(60):
        time.sleep(0.5)
        hwnds = find_visible_windows(MAIN_CLASS)
        if hwnds:
            break
    if not hwnds:
        resultado["veredicto"] = "ERROR"
        resultado["error"] = "ventana CasioNativeMain no apareció"
        _save(resultado, args.out, ts)
        return 2
    hwnd = hwnds[0]
    for _ in range(80):
        time.sleep(0.5)
        if count_ready_in_log(log_path) > n0:
            break
    canvas = find_child(hwnd, CANVAS_CLASS)
    if not canvas:
        resultado["veredicto"] = "ERROR"
        resultado["error"] = "canvas CasioCanvasArea no encontrado"
        _save(resultado, args.out, ts)
        return 2
    user32.SendMessageW(hwnd, WM_COMMAND, (BN_CLICKED << 16) | ID_BTN_MODE_GRAPH, 0)
    time.sleep(1.5)

    rect = wt.RECT()
    user32.GetClientRect(canvas, ctypes.byref(rect))
    cw, ch = rect.right, rect.bottom
    layout = solo_layout(cw, ch)
    resultado["canvas"] = {"w": cw, "h": ch, "layout": list(layout)}

    # A1: MAIN MENU visible — recorte por caja LCD del layout + colorido + OCR
    lcd_box = expected_lcd_box(layout)
    menu_png, menu_info = None, None
    menu_ok = False
    deadline = time.time() + 40.0
    while time.time() < deadline:
        png_i, info_i = capture_lcd(canvas, cw, ch, args.out, "menu", lcd_box)
        menu_png, menu_info = png_i, info_i
        colorido = bool(info_i and info_i.get("colorful_pixels", 0) >= 20000)
        log(f"esperando MAIN MENU: colorido={colorido} "
            f"detectado={info_i.get('bbox_detectado') if info_i else None}")
        if colorido:
            found = ocr_tokens(args.out, "menu", png_i,
                               ["MAIN MENU", "Run-Matrix"], resultado["ocr"])
            if found.get("MAIN MENU") or found.get("Run-Matrix"):
                menu_ok = True
                break
        time.sleep(1.0)
    resultado["capturas"]["menu"] = menu_info
    resultado["aserciones"]["A1_menu_visible"] = bool(menu_ok)
    if menu_info and menu_info.get("bbox_detectado"):
        dx, dy = center_offset(menu_info["bbox_detectado"], layout)
        resultado["geometria"] = {"esperado": list(lcd_box),
                                  "detectado": menu_info["bbox_detectado"],
                                  "dx": dx, "dy": dy}
    if not menu_ok:
        resultado["veredicto"] = "FAIL"
        resultado["error"] = ("MAIN MENU no alcanzado/confirmado dentro del plazo "
                              "(sin clics enviados)")
        _save(resultado, args.out, ts)
        return 1

    # A2: EXE abre la app resaltada (cambio de pantalla, con espera real)
    click_key(canvas, layout, "EXE")
    resultado["clave_enviada"].append("EXE")
    change_deadline = time.time() + 12.0
    while time.time() < change_deadline:
        probe = render_window(canvas, cw, ch)
        if probe is not None and menu_info:
            psha = sha256_bytes(probe.crop(lcd_box).tobytes())
            if psha != menu_info["crop_sha256"]:
                break
        time.sleep(0.5)
    time.sleep(0.8)
    app_png, app_info = capture_lcd(canvas, cw, ch, args.out, "app", lcd_box)
    resultado["capturas"]["app"] = app_info
    a2 = bool(menu_ok and app_info and app_info["crop_sha256"] != menu_info["crop_sha256"])
    resultado["aserciones"]["A2_app_abierta_cambio_pantalla"] = a2

    # A3: 1 + 2 EXE -> OCR lee '1+2' y la banda derecha gana tinta (resultado).
    for k in ("1", "MAS", "2"):
        click_key(canvas, layout, k)
        resultado["clave_enviada"].append(k)
    typed1_png, typed1_info = capture_lcd(canvas, cw, ch, args.out, "typed1", lcd_box)
    resultado["capturas"]["typed1"] = typed1_info
    ft1 = ocr_tokens(args.out, "typed1", typed1_png, ["1+2"], resultado["ocr"])
    click_key(canvas, layout, "EXE")
    resultado["clave_enviada"].append("EXE")
    time.sleep(1.5)
    expr1_png, expr1_info = capture_lcd(canvas, cw, ch, args.out, "expr1", lcd_box)
    resultado["capturas"]["expr1"] = expr1_info
    f1 = ocr_tokens(args.out, "expr1", expr1_png, ["1+2"], resultado["ocr"])
    ink1_prev = result_ink(args.out, "typed1",
                           os.path.join(args.out, "lcd-crop-typed1.png"),
                           0.20, 0.33, resultado["ocr"])
    ink1 = result_ink(args.out, "expr1",
                      os.path.join(args.out, "lcd-crop-expr1.png"),
                      0.20, 0.33, resultado["ocr"])
    a3 = bool(ft1.get("1+2") and f1.get("1+2") and
              ink1 >= RESULT_INK_MIN and ink1 > ink1_prev)
    resultado["aserciones"]["A3_1mas2_igual_3"] = a3

    # A4: AC; 5 + 4 EXE -> OCR lee '5+4' y la banda de la segunda línea gana
    # tinta (el 9). La línea 1 conserva su 3 (fuera de la banda elegida).
    click_key(canvas, layout, "AC")
    resultado["clave_enviada"].append("AC")
    time.sleep(0.5)
    for k in ("5", "MAS", "4"):
        click_key(canvas, layout, k)
        resultado["clave_enviada"].append(k)
    typed2_png, typed2_info = capture_lcd(canvas, cw, ch, args.out, "typed2", lcd_box)
    resultado["capturas"]["typed2"] = typed2_info
    ft2 = ocr_tokens(args.out, "typed2", typed2_png, ["5+4"], resultado["ocr"])
    click_key(canvas, layout, "EXE")
    resultado["clave_enviada"].append("EXE")
    time.sleep(1.5)
    expr2_png, expr2_info = capture_lcd(canvas, cw, ch, args.out, "expr2", lcd_box)
    resultado["capturas"]["expr2"] = expr2_info
    f2 = ocr_tokens(args.out, "expr2", expr2_png, ["5+4"], resultado["ocr"])
    ink2_prev = result_ink(args.out, "typed2",
                           os.path.join(args.out, "lcd-crop-typed2.png"),
                           0.38, 0.52, resultado["ocr"])
    ink2 = result_ink(args.out, "expr2",
                      os.path.join(args.out, "lcd-crop-expr2.png"),
                      0.38, 0.52, resultado["ocr"])
    a4 = bool(ft2.get("5+4") and f2.get("5+4") and
              ink2 >= RESULT_INK_MIN and ink2 > ink2_prev)
    resultado["aserciones"]["A4_5mas4_igual_9"] = a4

    # A5: MENU -> de vuelta al MAIN MENU
    click_key(canvas, layout, "MENU")
    resultado["clave_enviada"].append("MENU")
    time.sleep(2.0)
    menu2_png, menu2_info = capture_lcd(canvas, cw, ch, args.out, "menu2", lcd_box)
    resultado["capturas"]["menu2"] = menu2_info
    f3 = ocr_tokens(args.out, "menu2", menu2_png, ["MAIN MENU", "Run-Matrix"], resultado["ocr"])
    a5 = bool(f3.get("MAIN MENU") or f3.get("Run-Matrix"))
    resultado["aserciones"]["A5_vuelta_al_menu"] = a5

    if not args.keep_open:
        close_app()
        resultado["app_cerrada"] = True

    ok = all(resultado["aserciones"].values())
    resultado["veredicto"] = "PASS" if ok else "FAIL"
    _save(resultado, args.out, ts)
    log("veredicto: " + resultado["veredicto"])
    return 0 if ok else 1


def _save(res, out_dir, ts):
    p = os.path.join(out_dir, f"keydrive-{ts}.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=2)
    print(json.dumps({"veredicto": res.get("veredicto"),
                      "aserciones": res.get("aserciones"),
                      "ruta": p}, ensure_ascii=False))


def build_arg_parser():
    ap = argparse.ArgumentParser(
        prog="cg50_keydrive.py",
        description="Driver de UI de Calculadora Unificada: secuencias VK (G-5) "
                    "y escenario legacy del paso 2.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Ejemplos:\n"
            "  python tools/cg50_keydrive.py --dry-run --out verification\\<dir>\n"
            "  python tools/cg50_keydrive.py --self-test --out verification\\<dir>\n"
            "  python tools/cg50_keydrive.py --keys \"VK_HOME,VK_RETURN,VK_1,VK_ADD,VK_2,VK_RETURN\" \\\n"
            "         --capture-after 1,6 --out verification\\<dir>\n"
            "  python tools/cg50_keydrive.py --sequence mi_seq.json --capture-ocr --out verification\\<dir>\n"
            "  python tools/cg50_keydrive.py --out verification\\<dir>   (escenario legacy)\n"))
    ap.add_argument("--sequence", "--json", dest="sequence", default=None,
                    help="Secuencia en JSON (lista de pasos u objeto con 'steps')")
    ap.add_argument("--keys", default=None,
                    help="Secuencia CLI: vk[:hold_ms[:wait_ms]] separados por comas")
    ap.add_argument("--dry-run", action="store_true",
                    help="Valida y muestra el plan sin buscar ventana ni enviar nada")
    ap.add_argument("--self-test", action="store_true",
                    help="Valida el mapeo VK de main.cpp y el plan contra cg50_keys_exact.h")
    ap.add_argument("--out", default=None,
                    help="Directorio de informes/capturas (obligatorio en modo legacy)")
    ap.add_argument("--exe", default=DEFAULT_EXE, help="Ruta al ejecutable (capturas/legacy)")
    ap.add_argument("--hold-ms", type=int, default=None,
                    help=f"hold por defecto en ms (mínimo {MIN_HOLD_MS} salvo --allow-short-holds)")
    ap.add_argument("--wait-ms", type=int, default=None,
                    help=f"espera por defecto tras el KEYUP en ms (defecto {DEFAULT_WAIT_MS})")
    ap.add_argument("--allow-short-holds", action="store_true",
                    help="Permite holds < 5000 ms (solo para pruebas; riesgo de tecla perdida)")
    ap.add_argument("--capture-after", default="",
                    help="Índices 1-based de pasos a capturar con verify_gui.py (p.ej. 1,4,6)")
    ap.add_argument("--capture-each", action="store_true",
                    help="Captura con verify_gui.py tras cada paso")
    ap.add_argument("--capture-ocr", action="store_true",
                    help="Pasa --ocr a verify_gui.py en las capturas")
    ap.add_argument("--no-cg50-mode", action="store_true",
                    help="No conmutar a vista Solo-CG50 antes de enviar la secuencia")
    ap.add_argument("--keep-open", action="store_true",
                    help="[legacy] no cerrar la app al terminar el escenario")
    ap.add_argument("--legacy", action="store_true",
                    help="Forzar el escenario legacy del paso 2 (necesita --out)")
    return ap


def main(argv=None):
    args = build_arg_parser().parse_args(argv)

    if args.self_test:
        return run_self_test(args)
    if args.dry_run:
        return run_dry_run(args)
    if args.sequence or args.keys:
        return run_sequence(args)
    if args.legacy or args.out:
        if not args.out:
            print("ERROR: el escenario legacy requiere --out", file=sys.stderr)
            return 2
        print("[keydrive] modo legacy (paso 2); usa --sequence/--keys para G-5",
              file=sys.stderr)
        return run_legacy_scenario(args)
    build_arg_parser().print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
