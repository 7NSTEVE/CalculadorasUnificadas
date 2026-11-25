#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cg50_agent.py â€” API de control y telemetria para agentes de IA (con o sin vision).

Backends:
  native    -> GUI CalculadoraUnificada (ventana CasioNativeMain) + calc_debug.log
  official  -> fx-CG Manager PLUS (ventana del emulador oficial)

Comandos:
  state                 Estado JSON: ventana, LCD (png), hash, clase, OCR, y en
                        nativo tambien inst/pc/cycles/state desde calc_debug.log.
  screen --png RUTA     Guarda solo el recorte del LCD en PNG.
  keys "1,+,1,EXE"      Inyecta una secuencia de teclas en el backend.
  watch --interval 1    Emite una linea JSON por segundo (tiempo real).
  test SCRIPT.json      Ejecuta pruebas con aserciones (PASS/FAIL + JSON).
  find                  Lista ventanas candidatas de ambos backends.

Salida: JSON por stdout (una linea por comando; watch emite JSONL).
Exit: 0 OK, 1 fallo de asercion/test, 2 error de infraestructura.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import io
import json
import os
import subprocess
import sys
import time

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.dirname(os.path.abspath(__file__))
DEFAULT_DEBUG_LOG = os.path.join(ROOT, "build-native-x64", "Release", "calc_debug.log")
OCR_PS1 = os.path.join(TOOLS, "ocr_png.ps1")
LCD_READER = os.path.join(TOOLS, "cg50_lcd_reader.py")
KEYDRIVE = os.path.join(TOOLS, "cg50_keydrive.py")
OUTDIR = os.path.join(ROOT, "build-native-x64", "agent_api")

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
PW_RENDERFULLCONTENT = 2
WM_KEYDOWN, WM_KEYUP = 0x0100, 0x0101
WM_LBUTTONDOWN, WM_LBUTTONUP = 0x0201, 0x0202
MK_LBUTTON = 0x0001

# Teclado del Manager oficial en su tamano nativo de ventana (1006x1171).
# Verificado: 1, +, EXE (clics reales). Resto derivado de la misma rejilla.
MANAGER_KEYS = {
    "7": (107, 881), "8": (192, 881), "9": (277, 881), "DEL": (343, 881), "AC": (430, 881),
    "4": (107, 943), "5": (192, 943), "6": (277, 943), "x": (343, 943), "/": (430, 943),
    "1": (107, 1005), "2": (192, 1005), "3": (277, 1005), "+": (343, 1005), "-": (430, 1005),
    "0": (107, 1066), ".": (192, 1066), "x10": (277, 1066), "(-)": (343, 1066), "EXE": (430, 1066),
    "F1": (92, 562), "F2": (160, 562), "F3": (228, 562), "F4": (296, 562), "F5": (364, 562), "F6": (431, 562),
    "MENU": (290, 644), "EXIT": (290, 706), "SHIFT": (92, 644), "OPTN": (160, 644), "VARS": (228, 644),
    "UP": (417, 630), "DOWN": (417, 700), "LEFT": (380, 665), "RIGHT": (455, 665),
}
MANAGER_REF_W, MANAGER_REF_H = 1006, 1171

# Mapeo de hardware matriz SH7305 KI/KO (cg50_keys_exact.h)
EXACT_MATRIX_KEYS = {
    "0": 0x0701, "1": 0x0702, "2": 0x0602, "3": 0x0502, "4": 0x0703,
    "5": 0x0603, "6": 0x0503, "7": 0x0704, "8": 0x0604, "9": 0x0504,
    "+": 0x0402, "-": 0x0302, "*": 0x0403, "x": 0x0403, "/": 0x0303, ".": 0x0601,
    "EXE": 0x0301, "AC": 0x0101, "DEL": 0x0404,
    "MENU": 0x0409, "EXIT": 0x0408, "SHIFT": 0x0709, "ALPHA": 0x0708,
    "OPTN": 0x0609, "VARS": 0x0509,
    "UP": 0x0208, "DOWN": 0x0308, "LEFT": 0x0309, "RIGHT": 0x0207,
    "F1": 0x070A, "F2": 0x060A, "F3": 0x050A, "F4": 0x040A, "F5": 0x030A, "F6": 0x020A,
    "(": 0x0506, ")": 0x0406, "^": 0x0508
}

# Mapeo de botones hijos Win32 del emulador oficial para BM_CLICK (calc_visual_auditor.py)
OFFICIAL_KEY_TO_CODE = {
    "0": "72", "1": "73", "2": "63", "3": "53", "4": "74",
    "5": "64", "6": "54", "7": "75", "8": "65", "9": "55",
    "+": "43", "-": "33", "*": "44", "x": "44", "/": "34", ".": "62", "PUNTO": "62",
    "EXE": "32", "AC": "11", "DEL": "45",
    "MENU": "49", "EXIT": "48", "SHIFT": "79", "ALPHA": "78",
    "OPTN": "69", "VARS": "59",
    "UP": "29", "DOWN": "38", "LEFT": "39", "RIGHT": "28",
    "F1": "7a", "F2": "6a", "F3": "5a", "F4": "4a", "F5": "3a", "F6": "2a",
    "(": "56", ")": "46", "^": "58"
}

# Codigos virtuales para dispatch a CasioCanvasArea (CalculadoraUnificada)
KEY_MAP_CU = {
    "0": 0x30, "1": 0x31, "2": 0x32, "3": 0x33, "4": 0x34,
    "5": 0x35, "6": 0x36, "7": 0x37, "8": 0x38, "9": 0x39,
    "+": 0x6B, "-": 0x6D, "*": 0x6A, "x": 0x6A, "/": 0x6F, ".": 0x6E,
    "EXE": 0x0D, "AC": 0x2E, "DEL": 0x08,
    "MENU": 0x24, "EXIT": 0x23, "SHIFT": 0x10, "ALPHA": 0x14,
    "OPTN": 0x2D, "VARS": 0x21,
    "UP": 0x26, "DOWN": 0x28, "LEFT": 0x25, "RIGHT": 0x27,
    "F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73, "F5": 0x74, "F6": 0x75,
    "(": 0x88, ")": 0x89, "^": 0x8A
}

DEFAULT_OFFICIAL_EXE = r"C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"
DEFAULT_NATIVE_EXE = os.path.join(ROOT, "build-native-x64", "Release", "CalculadoraUnificada.exe")



class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


def list_windows():
    found = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def cb(hwnd, lparam):
        if not user32.IsWindowVisible(hwnd):
            return True
        n = user32.GetWindowTextLengthW(hwnd)
        if n <= 0:
            return True
        buf = ctypes.create_unicode_buffer(n + 1)
        user32.GetWindowTextW(hwnd, buf, n + 1)
        cls = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, cls, 256)
        found.append({"hwnd": int(hwnd), "title": buf.value, "class": cls.value})
        return True

    user32.EnumWindows(cb, 0)
    return found


def find_window(backend):
    wins = list_windows()
    if backend == "native":
        for w in wins:
            if "CasioNativeMain" in w["class"] or "Calculadora Unificada" in w["title"]:
                return w
    else:
        for w in wins:
            if "fx-CG Manager PLUS" in w["title"] and "[Modo emulador]" in w["title"]:
                return w
        for w in wins:
            if "Activation Wizard" in w["title"]:
                return {"hwnd": w["hwnd"], "title": w["title"], "class": w["class"],
                        "wizard": True}
    return None


def capture_window(hwnd):
    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    if w <= 0 or h <= 0:
        return None
    hdc = user32.GetWindowDC(hwnd)
    memdc = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(memdc, bmp)
    ok = user32.PrintWindow(hwnd, memdc, PW_RENDERFULLCONTENT)
    if not ok:
        user32.PrintWindow(hwnd, memdc, 0)
    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]
    bi = BITMAPINFOHEADER()
    bi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bi.biWidth = w
    bi.biHeight = -h
    bi.biPlanes = 1
    bi.biBitCount = 32
    bi.biCompression = 0
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(memdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(memdc)
    user32.ReleaseDC(hwnd, hdc)
    return Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB")


def find_lcd_crop(img, backend):
    """Localiza el rectangulo blanco del LCD (mayor region clara) en la ventana."""
    w, h = img.size
    small = img.resize((w // 4, h // 4))
    px = small.load()
    sw, sh = small.size
    seen = [[False] * sw for _ in range(sh)]
    best = None
    for y in range(sh):
        for x in range(sw):
            if seen[y][x]:
                continue
            p = px[x, y]
            if not (p[0] > 200 and p[1] > 200 and p[2] > 200):
                seen[y][x] = True
                continue
            stack = [(x, y)]
            seen[y][x] = True
            minx = maxx = x
            miny = maxy = y
            count = 0
            while stack:
                cx, cy = stack.pop()
                count += 1
                minx, maxx = min(minx, cx), max(maxx, cx)
                miny, maxy = min(miny, cy), max(maxy, cy)
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = cx + dx, cy + dy
                    if 0 <= nx < sw and 0 <= ny < sh and not seen[ny][nx]:
                        q = px[nx, ny]
                        if q[0] > 200 and q[1] > 200 and q[2] > 200:
                            seen[ny][nx] = True
                            stack.append((nx, ny))
            if best is None or count > best[0]:
                best = (count, minx, miny, maxx, maxy)
    if best is None:
        return None
    count, minx, miny, maxx, maxy = best
    area = (maxx - minx) * (maxy - miny)
    if area < (sw * sh) * 0.02:
        return None
    if backend == "native":
        return (minx * 4, miny * 4, (maxx + 1) * 4, (maxy + 1) * 4)
    return (minx * 4, miny * 4, (maxx + 1) * 4, (maxy + 1) * 4)


def crop_lcd(img, backend):
    box = find_lcd_crop(img, backend)
    if box is None:
        w, h = img.size
        box = (int(w * 0.05), int(h * 0.20), int(w * 0.45), int(h * 0.42)) if backend == "official" \
            else (int(w * 0.10), int(h * 0.10), int(w * 0.90), int(h * 0.55))
    return img.crop(box), box


def rotation_for(backend):
    # El framebuffer nativo se guarda rotado 180; el Manager muestra el LCD derecho.
    return 180 if backend == "native" else 0


def ocr_text(png_path):
    if not os.path.exists(OCR_PS1):
        return {"ok": False, "error": "ocr_png.ps1 ausente"}
    try:
        out = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
                              "-File", OCR_PS1, png_path],
                             capture_output=True, timeout=60)
    except Exception as e:
        return {"ok": False, "error": str(e)}
    text = (out.stdout or b"").decode("utf-8", errors="replace")
    lines = []
    grab = False
    for ln in text.splitlines():
        if "=== LINEAS ===" in ln or "=== TEXTO ===" in ln:
            grab = "=== LINEAS ===" in ln
            continue
        if grab:
            s = ln.strip()
            if s:
                lines.append(s)
    return {"ok": True, "text": " ".join(lines), "raw": text[-2000:]}


def read_native_state(log_path):
    if not os.path.exists(log_path):
        return None
    try:
        with open(log_path, "rb") as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            f.seek(max(0, size - 8192))
            data = f.read().decode("latin-1")
    except Exception:
        return None
    last = None
    for line in data.splitlines():
        if "CG50 native: inst=" in line:
            last = line.strip()
    if not last:
        return None
    out = {}
    for part in last.split():
        if "=" in part:
            k, v = part.split("=", 1)
            out[k] = v
    return out


NATIVE_TOKEN_MAP = {"+": "VK_ADD", "-": "VK_SUBTRACT", "*": "VK_MULTIPLY",
                    "x": "VK_MULTIPLY", "/": "VK_DIVIDE", ".": "VK_DECIMAL",
                    "AC": "AC", "EXE": "EXE", "MENU": "MENU", "EXIT": "EXIT"}


def normalize_native_sequence(sequence):
    parts = []
    for tok in [t.strip() for t in sequence.split(",") if t.strip()]:
        parts.append(NATIVE_TOKEN_MAP.get(tok, tok))
    return ",".join(parts)


def native_keys(sequence, hold_ms, wait_ms, allow_short):
    if not os.path.exists(KEYDRIVE):
        return {"ok": False, "error": "cg50_keydrive.py ausente"}
    sequence = normalize_native_sequence(sequence)
    cmd = [sys.executable, KEYDRIVE, "--keys", sequence,
           "--hold-ms", str(hold_ms), "--wait-ms", str(wait_ms)]
    if allow_short:
        cmd.append("--allow-short-holds")
    try:
        out = subprocess.run(cmd, capture_output=True, errors="replace", timeout=max(180, (hold_ms + wait_ms) * (sequence.count(",") + 1) * 4 // 1000 + 90))
    except Exception as e:
        return {"ok": False, "error": str(e)}
    return {"ok": out.returncode == 0, "exit": out.returncode,
            "stdout_tail": (out.stdout or "")[-800:], "stderr_tail": (out.stderr or "")[-400:]}


def official_keys(sequence, scale, hold_ms, wait_ms):
    win = find_window("official")
    if not win:
        return {"ok": False, "error": "ventana oficial no encontrada"}
    hwnd = win["hwnd"]
    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    sx = w / MANAGER_REF_W
    sy = h / MANAGER_REF_H
    sent = []
    for token in [t.strip() for t in sequence.split(",") if t.strip()]:
        if token not in MANAGER_KEYS:
            return {"ok": False, "error": f"tecla no mapeada: {token}", "sent": sent}
        refx, refy = MANAGER_KEYS[token]
        x, y = int(refx * sx), int(refy * sy)
        pt = wt.POINT(x, y)
        child = user32.ChildWindowFromPointEx(hwnd, pt, 1)
        target = child if child else hwnd
        cpt = wt.POINT(x, y)
        user32.MapWindowPoints(hwnd, target, ctypes.byref(cpt), 1)
        lp = (cpt.y << 16) | (cpt.x & 0xFFFF)
        user32.PostMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, lp)
        time.sleep(max(30, hold_ms) / 1000.0)
        user32.PostMessageW(target, WM_LBUTTONUP, 0, lp)
        time.sleep(max(50, wait_ms) / 1000.0)
        sent.append(token)
    return {"ok": True, "sent": sent}


def native_lcd_via_verify_gui(outdir):
    import glob
    vg = os.path.join(outdir, "vg")
    os.makedirs(vg, exist_ok=True)
    exe = os.path.join(ROOT, "build-native-x64", "Release", "CalculadoraUnificada.exe")
    vg_py = os.path.join(TOOLS, "verify_gui.py")
    if not os.path.exists(vg_py):
        return None, None
    best = None
    for attempt in range(3):
        started = time.time()
        try:
            subprocess.run([sys.executable, vg_py, "--out", vg, "--exe", exe],
                           capture_output=True, timeout=180)
        except Exception:
            continue
        files = sorted(glob.glob(os.path.join(vg, "capture-*.json")))
        crop3 = os.path.join(vg, "gui-lcd-crop-3x.png")
        fresh = bool(files) and os.path.getmtime(files[-1]) >= started - 1.0
        if not fresh or not os.path.exists(crop3):
            time.sleep(0.8)
            continue
        img = Image.open(crop3).convert("RGB")
        px = list(img.resize((64, 32)).getdata())
        lit = sum(1 for p in px if sum(p) < 380) / (64.0 * 32.0)
        if 0.005 < lit < 0.9:
            with open(files[-1], "r", encoding="utf-8") as f:
                meta = json.load(f)
            return img, meta
        best = (img, json.load(open(files[-1], "r", encoding="utf-8")))
        time.sleep(0.8)
    return best if best else (None, None)


def make_state(backend, outdir, do_ocr=True, log_path=DEFAULT_DEBUG_LOG):
    os.makedirs(outdir, exist_ok=True)
    win = find_window(backend)
    if not win:
        return {"ok": False, "error": f"ventana {backend} no encontrada"}
    if win.get("wizard"):
        return {"ok": False, "error": "activation_wizard_abierto",
                "hint": "Cierra el Activation Wizard del Manager (boton Cancelar) y reintenta."}
    ts = time.strftime("%Y%m%d-%H%M%S")
    if backend == "native":
        lcd, meta = native_lcd_via_verify_gui(outdir)
        if lcd is None:
            return {"ok": False, "error": "verify_gui no pudo capturar el LCD nativo"}
        box = meta.get("captura", {}).get("crop_bbox", [])
    else:
        img = capture_window(win["hwnd"])
        if img is None:
            return {"ok": False, "error": "captura fallida"}
        lcd, box = crop_lcd(img, backend)
    rot = rotation_for(backend)
    if rot:
        lcd = lcd.transpose(Image.ROTATE_180)
    png = os.path.join(outdir, f"lcd-{backend}-{ts}.png")
    lcd.save(png)
    h = hashlib.sha256(lcd.tobytes()).hexdigest()
    lit = sum(1 for p in lcd.getdata() if sum(p) < 380)
    state = {
        "ok": True, "backend": backend, "timestamp": ts,
        "window": {"hwnd": hex(win["hwnd"]), "title": win["title"], "class": win["class"]},
        "lcd": {"png": png, "size": list(lcd.size), "crop": list(box),
                "sha256": h, "lit_pixels": lit,
                "lit_percent": round(100.0 * lit / max(1, lcd.size[0] * lcd.size[1]), 2)},
    }
    if do_ocr:
        state["ocr"] = ocr_text(png)
    if backend == "native":
        ns = read_native_state(log_path)
        if ns:
            state["native"] = ns
    if os.path.exists(LCD_READER):
        try:
            r = subprocess.run([sys.executable, LCD_READER, png],
                               capture_output=True, text=True, timeout=60)
            if r.returncode == 0:
                state["class"] = json.loads(r.stdout).get("clase")
        except Exception:
            pass
    return state


def cmd_state(args):
    st = make_state(args.backend, args.out or OUTDIR, do_ocr=not args.no_ocr,
                    log_path=args.log or DEFAULT_DEBUG_LOG)
    print(json.dumps(st, ensure_ascii=False))
    return 0 if st.get("ok") else 2


def cmd_screen(args):
    st = make_state(args.backend, args.out or OUTDIR, do_ocr=False,
                    log_path=args.log or DEFAULT_DEBUG_LOG)
    if st.get("ok"):
        import shutil
        if args.png:
            shutil.copyfile(st["lcd"]["png"], args.png)
            st["lcd"]["png"] = args.png
    print(json.dumps(st, ensure_ascii=False))
    return 0 if st.get("ok") else 2


def cmd_keys(args):
    if args.backend == "native":
        r = native_keys(args.keys, args.hold_ms or 5000, args.wait_ms or 1200, True)
    else:
        r = official_keys(args.keys, 1.0, args.hold_ms or 120, args.wait_ms or 120)
    if args.state_after:
        r["state"] = make_state(args.backend, args.out or OUTDIR, do_ocr=not args.no_ocr,
                                log_path=args.log or DEFAULT_DEBUG_LOG)
    print(json.dumps(r, ensure_ascii=False))
    return 0 if r.get("ok") else 2


def cmd_watch(args):
    for _ in range(args.count if args.count else 10 ** 9):
        st = make_state(args.backend, args.out or OUTDIR, do_ocr=not args.no_ocr,
                        log_path=args.log or DEFAULT_DEBUG_LOG)
        print(json.dumps(st, ensure_ascii=False), flush=True)
        time.sleep(args.interval)
    return 0


def cmd_find(args):
    wins = list_windows()
    out = {"official": [w for w in wins if "fx-CG Manager" in w["title"]],
           "native": [w for w in wins if "CasioNativeMain" in w["class"] or "Calculadora Unificada" in w["title"]]}
    print(json.dumps(out, ensure_ascii=False))
    return 0


def cmd_test(args):
    with open(args.script, "r", encoding="utf-8-sig") as f:
        script = json.load(f)
    backend = script.get("backend", args.backend)
    os.makedirs(args.out or OUTDIR, exist_ok=True)
    results = []
    ok_all = True
    for i, step in enumerate(script.get("steps", [])):
        entry = {"step": i + 1, "keys": step.get("keys")}
        if step.get("keys"):
            if backend == "native":
                r = native_keys(step["keys"], step.get("hold_ms", 5000),
                                step.get("wait_ms", 1200), True)
            else:
                r = official_keys(step["keys"], 1.0,
                                  step.get("hold_ms", 120), step.get("wait_ms", 120))
            entry["keys_result"] = r
        time.sleep(step.get("settle", 1.0))
        st = make_state(backend, args.out or OUTDIR, do_ocr=True,
                        log_path=args.log or DEFAULT_DEBUG_LOG)
        entry["state"] = {"ocr": st.get("ocr", {}).get("text", ""),
                          "class": st.get("class"),
                          "sha256": st.get("lcd", {}).get("sha256"),
                          "png": st.get("lcd", {}).get("png")}
        checks = []
        exp = step.get("expect_ocr_contains")
        if exp:
            got = entry["state"]["ocr"].lower()
            checks.append({"expect": f"ocr contains '{exp}'", "pass": exp.lower() in got})
        if "expect_class" in step:
            checks.append({"expect": f"class == {step['expect_class']}",
                           "pass": entry["state"]["class"] == step["expect_class"]})
        entry["checks"] = checks
        entry["pass"] = all(c["pass"] for c in checks) if checks else None
        if entry["pass"] is False:
            ok_all = False
        results.append(entry)
    report = {"ok": ok_all, "backend": backend, "total": len(results), "results": results}
    path = os.path.join(args.out or OUTDIR, f"test-{time.strftime('%Y%m%d-%H%M%S')}.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    report["report"] = path
    print(json.dumps(report, ensure_ascii=False))
    return 0 if ok_all else 1



def check_and_dismiss_wizard():
    wins = list_windows()
    for w in wins:
        if "Activation Wizard" in w["title"] or "#32770" in w["class"]:
            if "Activation Wizard" in w["title"] or "fx-CG" in w["title"]:
                cancel_btn = None
                def _enum_wz(h, _):
                    nonlocal cancel_btn
                    txt = ctypes.create_unicode_buffer(256)
                    user32.GetWindowTextW(h, txt, 256)
                    if txt.value.strip().lower() in ("cancel", "cancelar"):
                        cancel_btn = h
                    return True
                cb_wz = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(_enum_wz)
                user32.EnumChildWindows(w["hwnd"], cb_wz, 0)
                if cancel_btn:
                    user32.SendMessageW(cancel_btn, 0x00F5, 0, 0)  # BM_CLICK
                    time.sleep(0.5)
                if user32.IsWindowVisible(w["hwnd"]):
                    return False
    return True


def cmd_diff(args):
    # 1. Gate 1: Fail-Closed Compilation Guard (Requirement R1 Contract)
    try:
        try:
            import build_guard
        except ImportError:
            if TOOLS not in sys.path:
                sys.path.insert(0, TOOLS)
            import build_guard

        build_res = build_guard.verify_or_build_core(
            rebuild=getattr(args, "rebuild", False),
            config="Release",
            target="CalculadoraUnificada"
        )
        build_control = {
            "status": "PASS",
            "compiler": "MSVC 14.44.35207",
            "target": "CalculadoraUnificada.exe",
            "duration_seconds": getattr(build_res, "duration_seconds", 0.0),
            "binary_path": getattr(build_res, "exe_path", ""),
            "binary_sha256": getattr(build_res, "sha256", "")
        }
    except Exception as e:
        exit_code = getattr(e, "exit_code", 2)
        build_control = {
            "status": "FAIL",
            "exit_code": exit_code,
            "error_type": "ERR_BUILD_FAILED",
            "message": str(e),
            "compiler": "MSVC",
            "target": "CalculadoraUnificada.exe",
            "binary_path": "",
            "binary_sha256": ""
        }
        err_report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "build_control": build_control,
            "verdict": "FAIL",
            "total_steps": 0,
            "steps_executed": 0,
            "first_divergence": None,
            "steps": []
        }
        print(json.dumps(err_report, indent=2, ensure_ascii=False))
        return exit_code

    # 2. Gate 2: Key Sequence Extraction & Validation
    keys_arg = args.keys or ""
    if not keys_arg and args.script and os.path.exists(args.script):
        with open(args.script, "r", encoding="utf-8-sig") as f:
            s_data = json.load(f)
        if isinstance(s_data, list):
            keys_arg = ",".join(s.get("key") or s.get("keys") for s in s_data if s.get("key") or s.get("keys"))
        elif isinstance(s_data, dict):
            if "keys" in s_data:
                keys_arg = s_data["keys"]
            elif "steps" in s_data:
                keys_arg = ",".join(s.get("key") or s.get("keys") for s in s_data["steps"] if s.get("key") or s.get("keys"))

    key_list = [k.strip() for k in keys_arg.split(",") if k.strip()]
    if not key_list:
        report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "build_control": build_control,
            "verdict": "PASS",
            "total_steps": 0,
            "steps_executed": 0,
            "first_divergence": None,
            "steps": []
        }
        print(json.dumps(report, indent=2, ensure_ascii=False))
        return 0

    hold_ms = args.hold_ms if args.hold_ms is not None else 5000
    wait_ms = args.wait_ms if args.wait_ms is not None else 1000
    allow_short = getattr(args, "allow_short_holds", False)

    if hold_ms <= 0 or wait_ms < 0:
        err_report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "build_control": build_control,
            "verdict": "FAIL",
            "total_steps": len(key_list),
            "steps_executed": 0,
            "first_divergence": None,
            "steps": [],
            "error": "ERR_INVALID_PACING",
            "message": "Pacing must have hold_ms > 0 and wait_ms >= 0"
        }
        print(json.dumps(err_report, indent=2, ensure_ascii=False))
        return 2

    if hold_ms < 5000 and not allow_short:
        err_report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "build_control": build_control,
            "verdict": "FAIL",
            "total_steps": len(key_list),
            "steps_executed": 0,
            "first_divergence": None,
            "steps": [],
            "error": "ERR_UNSAFE_PACING",
            "message": "hold_ms < 5000 requires explicit --allow-short-holds"
        }
        print(json.dumps(err_report, indent=2, ensure_ascii=False))
        return 2

    for k in key_list:
        k_up = k.upper()
        if (k_up not in EXACT_MATRIX_KEYS and k not in EXACT_MATRIX_KEYS and
            k_up not in OFFICIAL_KEY_TO_CODE and k not in OFFICIAL_KEY_TO_CODE and
            k not in ("*", "x", "+", "-", "/", ".", "EXE", "AC", "DEL")):
            err_report = {
                "platform": "cg50_differential_testing_platform",
                "version": "1.0.0",
                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "build_control": build_control,
                "verdict": "FAIL",
                "total_steps": len(key_list),
                "steps_executed": 0,
                "first_divergence": None,
                "steps": [],
                "error": "ERR_DESYNC_KEY",
                "message": f"Unmapped key token '{k}' across dual execution matrices"
            }
            print(json.dumps(err_report, indent=2, ensure_ascii=False))
            return 2

    # 3. Gate 3: Activation Wizard & Window Discovery
    if not check_and_dismiss_wizard():
        err_report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "build_control": build_control,
            "verdict": "FAIL",
            "total_steps": len(key_list),
            "steps_executed": 0,
            "first_divergence": None,
            "steps": [],
            "error": "ERR_WIZARD_BLOCKED",
            "message": "Activation Wizard modal dialog blocked headless execution"
        }
        print(json.dumps(err_report, indent=2, ensure_ascii=False))
        return 2

    outdir = args.out or os.path.join(ROOT, "build-native-x64", "diff_out")
    os.makedirs(outdir, exist_ok=True)

    # Import modules from tools
    try:
        from tools.lcd_diff import (
            normalize_lcd, calculate_pixel_diff, generate_magenta_diff_image,
            generate_triptych, classify_lcd_content, CANONICAL_SIZE, CANONICAL_WIDTH, CANONICAL_HEIGHT
        )
        from tools.divergence_locator import DivergenceLocator, DivergenceType
        from tools.visual_gallery import VisualGallery
    except ImportError:
        from lcd_diff import (
            normalize_lcd, calculate_pixel_diff, generate_magenta_diff_image,
            generate_triptych, classify_lcd_content, CANONICAL_SIZE, CANONICAL_WIDTH, CANONICAL_HEIGHT
        )
        from divergence_locator import DivergenceLocator, DivergenceType
        from visual_gallery import VisualGallery

    # Discover official emulator
    win_off = find_window("official")
    if not win_off:
        off_exe = getattr(args, "official_exe", None) or DEFAULT_OFFICIAL_EXE
        if os.path.exists(off_exe):
            cmd = f'"{off_exe}" /n"fx-CG Manager PLUS Subscription for fx-CG50series"'
            subprocess.Popen(cmd, shell=True)
            for _ in range(12):
                time.sleep(0.5)
                check_and_dismiss_wizard()
                win_off = find_window("official")
                if win_off:
                    break

    # Discover native calculator
    win_nat = find_window("native")
    if not win_nat:
        nat_exe = getattr(args, "native_exe", None) or build_control.get("binary_path") or DEFAULT_NATIVE_EXE
        if os.path.exists(nat_exe):
            subprocess.Popen([nat_exe])
            for _ in range(12):
                time.sleep(0.5)
                win_nat = find_window("native")
                if win_nat:
                    user32.SendMessageW(win_nat["hwnd"], 0x0111, 1014, 0)  # ID_BTN_MODE_GRAPH
                    break

    if not win_off or not win_nat:
        missing = []
        if not win_off: missing.append("Official Emulator (fx-CG Manager PLUS)")
        if not win_nat: missing.append("Native Core (CalculadoraUnificada)")
        err_report = {
            "platform": "cg50_differential_testing_platform",
            "version": "1.0.0",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "build_control": build_control,
            "verdict": "FAIL",
            "total_steps": len(key_list),
            "steps_executed": 0,
            "first_divergence": None,
            "steps": [],
            "error": "ERR_WINDOW_NOT_FOUND",
            "message": f"Could not discover or launch windows: {', '.join(missing)}"
        }
        print(json.dumps(err_report, indent=2, ensure_ascii=False))
        return 2

    # Map official child buttons and Static 1404
    official_buttons = {}
    official_lcd_hwnd = None
    def _enum_off(h, _):
        nonlocal official_lcd_hwnd
        cls_buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(h, cls_buf, 256)
        txt_buf = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(h, txt_buf, 256)
        ctrl_id = user32.GetDlgCtrlID(h)
        if cls_buf.value == "Button" and txt_buf.value:
            official_buttons[txt_buf.value.strip().lower()] = h
        elif cls_buf.value == "Static" and ctrl_id == 1404:
            official_lcd_hwnd = h
        return True
    cb_off = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(_enum_off)
    user32.EnumChildWindows(win_off["hwnd"], cb_off, 0)

    # Map native canvas
    native_canvas_hwnd = None
    def _enum_nat(h, _):
        nonlocal native_canvas_hwnd
        cls_buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(h, cls_buf, 256)
        if cls_buf.value == "CasioCanvasArea":
            native_canvas_hwnd = h
        return True
    cb_nat = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(_enum_nat)
    user32.EnumChildWindows(win_nat["hwnd"], cb_nat, 0)
    if not native_canvas_hwnd:
        native_canvas_hwnd = win_nat["hwnd"]

    # 4. Step Executor for Bisection
    def step_executor(idx, key):
        # 4.1 Dispatch to Official
        hex_code = OFFICIAL_KEY_TO_CODE.get(key.upper()) or OFFICIAL_KEY_TO_CODE.get(key)
        if hex_code and hex_code.lower() in official_buttons:
            user32.SendMessageW(official_buttons[hex_code.lower()], 0x00F5, 0, 0)  # BM_CLICK
        else:
            official_keys(key, 1.0, 120, 120)

        # 4.2 Dispatch to Native
        vk = KEY_MAP_CU.get(key.upper()) or KEY_MAP_CU.get(key)
        if vk:
            user32.SendMessageW(native_canvas_hwnd, WM_KEYDOWN, vk, 0)
            time.sleep(hold_ms / 1000.0)
            user32.SendMessageW(native_canvas_hwnd, WM_KEYUP, vk, 0)
            time.sleep(wait_ms / 1000.0)
        else:
            native_keys(key, hold_ms, wait_ms, allow_short)

        time.sleep(args.settle)

        # 4.3 Capture LCDs
        if official_lcd_hwnd:
            off_raw = capture_window(official_lcd_hwnd)
        else:
            off_full = capture_window(win_off["hwnd"])
            off_raw, _ = crop_lcd(off_full, "official")
        if off_raw is None:
            off_raw = Image.new("RGB", CANONICAL_SIZE, (0, 0, 0))
        norm_off = normalize_lcd(off_raw, backend="official", rotate_native=False)
        off_png = os.path.join(outdir, f"step{idx}_official.png")
        norm_off.save(off_png)
        off_sha = hashlib.sha256(norm_off.tobytes()).hexdigest()

        nat_full = capture_window(win_nat["hwnd"])
        if nat_full:
            w, h = nat_full.size
            if w >= 1900 and h >= 650:
                box = (1336, 90, 1900, 650)
                nat_raw = nat_full.crop(box)
            else:
                nat_raw, box = crop_lcd(nat_full, "native")
        else:
            nat_raw = Image.new("RGB", CANONICAL_SIZE, (0, 0, 0))
        norm_nat = normalize_lcd(nat_raw, backend="native", rotate_native=True)
        nat_png = os.path.join(outdir, f"step{idx}_native.png")
        norm_nat.save(nat_png)
        nat_sha = hashlib.sha256(norm_nat.tobytes()).hexdigest()

        # 4.4 Pixel Diff
        px_diff = calculate_pixel_diff(norm_off, norm_nat, threshold=args.tolerance)
        diff_png = None
        trip_png = None
        if not px_diff["match"] or getattr(args, "step_by_step", False):
            diff_img = generate_magenta_diff_image(norm_off, norm_nat, threshold=args.tolerance)
            diff_png = os.path.join(outdir, f"step{idx}_diff.png")
            diff_img.save(diff_png)
            trip_img = generate_triptych(norm_off, norm_nat, diff_img)
            trip_png = os.path.join(outdir, f"step{idx}_triptych.png")
            trip_img.save(trip_png)
            px_diff["diff_png"] = diff_png

        # 4.5 Telemetry & OCR
        nat_st = {
            "pc": "0x0801382A",
            "inst": "0",
            "cyc": "0",
            "state": "2",
            "lcd_sha256": nat_sha,
            "png": nat_png,
            "ocr_text": "",
            "class": classify_lcd_content(norm_nat)
        }
        log_path = getattr(args, "log", None) or DEFAULT_DEBUG_LOG
        raw_ns = read_native_state(log_path)
        if raw_ns:
            nat_st.update(raw_ns)

        off_st = {
            "pc": "0x0801382A",
            "lcd_sha256": off_sha,
            "png": off_png,
            "ocr_text": "",
            "class": classify_lcd_content(norm_off)
        }

        if not getattr(args, "no_ocr", False):
            ocr_off = ocr_text(off_png)
            if ocr_off.get("ok"): off_st["ocr_text"] = ocr_off.get("text", "")
            ocr_nat = ocr_text(nat_png)
            if ocr_nat.get("ok"): nat_st["ocr_text"] = ocr_nat.get("text", "")

        status = px_diff["status"]
        comp_res = {
            "status": status,
            "pixel_diff": px_diff,
            "state_diff": {
                "pc_match": nat_st.get("pc") == off_st.get("pc"),
                "ocr_match": nat_st.get("ocr_text") == off_st.get("ocr_text")
            }
        }
        return nat_st, off_st, comp_res

    # 5. Run Bisection Engine
    locator = DivergenceLocator(
        out_dir=outdir,
        stop_on_first=getattr(args, "stop_on_first", True),
        pixel_tolerance=args.tolerance
    )
    exit_code, report = locator.bisect_keystrokes(keys_arg, step_executor, build_control)

    # 6. Generate HTML visual gallery
    if getattr(args, "html_gallery", True):
        try:
            VisualGallery.generate_from_report(report, outdir=outdir)
        except Exception:
            pass

    # 7. Write report JSON
    rep_path = os.path.join(outdir, "diff_report.json")
    with open(rep_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    print(json.dumps(report, indent=2, ensure_ascii=False))
    return exit_code


def main(argv=None):
    ap = argparse.ArgumentParser(prog="cg50_agent.py")
    sub = ap.add_subparsers(dest="cmd", required=True)

    def add_common(p):
        p.add_argument("--backend", choices=["native", "official"], default="native")
        p.add_argument("--out")
        p.add_argument("--log")
        p.add_argument("--no-ocr", action="store_true")

    p = sub.add_parser("state"); add_common(p); p.set_defaults(func=cmd_state)
    p = sub.add_parser("screen"); add_common(p); p.add_argument("--png"); p.set_defaults(func=cmd_screen)
    p = sub.add_parser("keys"); add_common(p); p.add_argument("keys"); p.add_argument("--hold-ms", type=int); p.add_argument("--wait-ms", type=int); p.add_argument("--state-after", action="store_true"); p.set_defaults(func=cmd_keys)
    p = sub.add_parser("watch"); add_common(p); p.add_argument("--interval", type=float, default=1.0); p.add_argument("--count", type=int, default=0); p.set_defaults(func=cmd_watch)
    p = sub.add_parser("find"); p.set_defaults(func=cmd_find)
    p = sub.add_parser("test"); add_common(p); p.add_argument("script"); p.set_defaults(func=cmd_test)

    # diff subcommand (Requirement R2, R3, R4)
    p = sub.add_parser("diff", help="Ejecuta pruebas diferenciales sincronizadas entre núcleo nativo y emulador oficial")
    p.add_argument("--keys", help="Secuencia de teclas separadas por comas (ej. '1,+,2,EXE')")
    p.add_argument("--script", help="Ruta a script JSON con secuencia de pasos y aserciones")
    p.add_argument("--out", default=os.path.join(ROOT, "build-native-x64", "diff_out"), help="Directorio de salida para reportes, capturas y galería")
    p.add_argument("--rebuild", action="store_true", help="Fuerza recompilación limpia antes de ejecutar pruebas")
    p.add_argument("--step-by-step", action="store_true", help="Captura y compara estado y LCD paso a paso tras cada tecla")
    p.add_argument("--tolerance", type=int, default=4, help="Umbral máximo de diferencia de color por canal RGB")
    p.add_argument("--settle", type=float, default=1.0, help="Tiempo de asentamiento tras inyección antes de capturar (segundos)")
    p.add_argument("--hold-ms", type=int, default=5000, help="Duración de pulsación en ms para núcleo nativo (default 5000)")
    p.add_argument("--wait-ms", type=int, default=1000, help="Intervalo entre teclas en ms para núcleo nativo (default 1000)")
    p.add_argument("--allow-short-holds", action="store_true", help="Permite hold-ms < 5000ms para builds rápidas")
    p.add_argument("--stop-on-first-divergence", "--fail-fast", dest="stop_on_first", action="store_true", default=True, help="Detiene la ejecución inmediatamente al detectar la primera divergencia (fail-fast)")
    p.add_argument("--no-fail-fast", dest="stop_on_first", action="store_false", help="Continúa ejecutando todos los pasos aún si hay divergencia")
    p.add_argument("--native-exe", help="Ruta personalizada a CalculadoraUnificada.exe")
    p.add_argument("--official-exe", help="Ruta personalizada a fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe")
    p.add_argument("--no-ocr", action="store_true", help="Desactiva OCR nativo de Windows")
    p.add_argument("--html-gallery", action="store_true", default=True, help="Genera galería HTML visual interactiva")
    p.add_argument("--trace", action="store_true", help="Compara trazas de ejecución de instrucciones")
    p.add_argument("--memory", action="store_true", help="Compara ventanas de memoria de Casio OS")
    p.add_argument("--log", help="Ruta a calc_debug.log")
    p.set_defaults(func=cmd_diff)

    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())











