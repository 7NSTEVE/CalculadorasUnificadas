#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lcd_diff.py — Canonical LCD Normalization & Pixel-by-Pixel Differential Engine.
Requirement R3 (Plataforma Integrada de Pruebas Diferenciales).

Core responsibilities:
- Canonical normalization: 384x216 pixels, 24-bit RGB grid.
- 180° rotation compensation for native core VRAM hardware mounting.
- Pixel-by-pixel differential calculation using color thresholding (default thr=4).
- Difference bounding box extraction and magenta (#FF00FF) discrepancy highlight.
- 3-panel composite triptych generation.
- Strict decoupling from OCR: pixel difference is the authoritative equality criterion.
"""

from dataclasses import dataclass, asdict
import os
import sys
from typing import Tuple, Optional, Dict, Any, List
from PIL import Image, ImageChops

CANONICAL_WIDTH = 384
CANONICAL_HEIGHT = 216
CANONICAL_SIZE = (CANONICAL_WIDTH, CANONICAL_HEIGHT)
TOTAL_PIXELS = CANONICAL_WIDTH * CANONICAL_HEIGHT  # 82,944
COLOR_MAGENTA = (255, 0, 255)  # RGB(255, 0, 255)
COLOR_DIFF_MAGENTA = COLOR_MAGENTA
DEFAULT_THRESHOLD = 4

# Safe resampling enum compatible with Pillow < 9.1 and >= 9.1
try:
    RESAMPLING_NEAREST = Image.Resampling.NEAREST
    TRANSPOSE_ROTATE_180 = Image.Transpose.ROTATE_180
except AttributeError:
    RESAMPLING_NEAREST = Image.NEAREST
    TRANSPOSE_ROTATE_180 = Image.ROTATE_180


class FrameCaptureError(Exception):
    """Raised when window capture fails or produces invalid dimensions."""
    pass


class BlackoutScreenError(FrameCaptureError):
    """Raised when a captured framebuffer is completely black."""
    pass


class DimensionMismatchError(ValueError):
    """Raised when image dimensions cannot be reconciled."""
    pass


@dataclass
class PixelDiffResult:
    """Structured pixel difference result."""
    status: str            # "MATCH" or "DIFF"
    changed_pixels: int    # Number of differing pixels
    total_pixels: int      # Total pixel count (82,944)
    changed_percent: float # Percentage of differing pixels
    diff_bbox: Optional[Tuple[int, int, int, int]]  # (x0, y0, x1, y1) or None
    match: bool            # True if changed_pixels == 0

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)

    def __getitem__(self, item):
        return asdict(self)[item]

    def get(self, key, default=None):
        return asdict(self).get(key, default)


def detect_blackout(img: Image.Image, min_nonzero: int = 1000) -> Dict[str, Any]:
    """
    Detect if the framebuffer is completely or almost entirely black.
    Returns status dictionary conforming to ERR_BLACK_SCREEN test contract.
    """
    raw_bytes = img.convert("RGB").tobytes()
    nonzero = sum(1 for b in raw_bytes if b > 0)
    if nonzero < min_nonzero:
        return {
            "blackout": True,
            "exit_code": 1,
            "error": "ERR_BLACK_SCREEN",
            "nonzero_pixels": nonzero
        }
    return {"blackout": False, "exit_code": 0, "nonzero_pixels": nonzero}


def normalize_lcd(img: Image.Image, backend: str = "native", rotate_native: bool = True) -> Image.Image:
    """
    Normalizes an LCD image to canonical 384x216 RGB24 format.
    Applies 180° rotation if backend is 'native' and rotate_native is True.
    """
    if not isinstance(img, Image.Image):
        raise TypeError(f"Expected PIL.Image.Image, got {type(img)}")

    out = img.copy()

    # 180° rotation compensation for native hardware mounting
    if backend == "native" and rotate_native:
        out = out.transpose(TRANSPOSE_ROTATE_180)

    # Resample to canonical 384x216 grid using nearest-neighbor to preserve exact pixels
    if out.size != CANONICAL_SIZE:
        out = out.resize(CANONICAL_SIZE, resample=RESAMPLING_NEAREST)

    return out.convert("RGB")


def calculate_pixel_diff(img_a: Image.Image, img_b: Image.Image, threshold: int = DEFAULT_THRESHOLD) -> Dict[str, Any]:
    """
    Direct pixel-by-pixel difference using ImageChops.difference.
    Conforms to img_diff.py and synthetic_lcd.py test specifications:
    - Verifies identical dimensions (returns error TAMANOS_DISTINTOS on mismatch)
    - Counts pixels where max(|R1-R2|, |G1-G2|, |B1-B2|) > threshold
    - Extracts difference bounding box
    """
    if img_a.size != img_b.size:
        return {
            "error": "TAMANOS_DISTINTOS",
            "size_a": img_a.size,
            "size_b": img_b.size,
            "match": False,
            "status": "DIFF",
            "changed_pixels": -1,
            "changed_percent": -1.0,
            "diff_bbox": None
        }

    rgb_a = img_a.convert("RGB")
    rgb_b = img_b.convert("RGB")
    diff = ImageChops.difference(rgb_a, rgb_b)
    bbox = diff.getbbox()
    raw_bytes = diff.tobytes()
    total_pixels = len(raw_bytes) // 3

    changed = 0
    # Threshold filtering to suppress subthreshold quantization / DAC noise
    for i in range(0, len(raw_bytes), 3):
        if max(raw_bytes[i], raw_bytes[i + 1], raw_bytes[i + 2]) > threshold:
            changed += 1

    pct = (changed / total_pixels) * 100.0 if total_pixels > 0 else 0.0

    return {
        "status": "MATCH" if changed == 0 else "DIFF",
        "changed_pixels": changed,
        "total_pixels": total_pixels,
        "changed_percent": round(pct, 4),
        "diff_bbox": bbox,
        "match": (changed == 0)
    }


def generate_magenta_diff_image(img_a: Image.Image, img_b: Image.Image,
                                threshold: int = DEFAULT_THRESHOLD,
                                dim_background: bool = False) -> Image.Image:
    """
    Generates a visual differential artifact:
    Base image with all differing pixels highlighted in vibrant magenta RGB(255, 0, 255).
    Non-differing pixels remain as original base image (or dimmed grayscale if dim_background is True).
    """
    rgb_a = img_a.convert("RGB")
    rgb_b = img_b.convert("RGB")
    diff = ImageChops.difference(rgb_a, rgb_b)

    out = rgb_a.copy()
    px_diff = diff.load()
    px_a = rgb_a.load()
    px_out = out.load()
    w, h = out.size

    for y in range(h):
        for x in range(w):
            r_d, g_d, b_d = px_diff[x, y]
            if max(r_d, g_d, b_d) > threshold:
                px_out[x, y] = COLOR_MAGENTA
            else:
                if dim_background:
                    ra, ga, ba = px_a[x, y]
                    gray = int(0.299 * ra + 0.587 * ga + 0.114 * ba) // 3
                    px_out[x, y] = (gray, gray, gray)
                else:
                    px_out[x, y] = px_a[x, y]

    return out


def generate_triptych(img_official: Image.Image, img_native: Image.Image, img_diff: Image.Image) -> Image.Image:
    """
    Combines Official Reference, Native Core, and Magenta Diff Highlight into a
    single side-by-side 3-panel triptych image (1152 x 216).
    """
    off_norm = normalize_lcd(img_official, backend="official")
    nat_norm = normalize_lcd(img_native, backend="native", rotate_native=False)
    diff_norm = normalize_lcd(img_diff, backend="diff", rotate_native=False)

    w, h = CANONICAL_WIDTH, CANONICAL_HEIGHT
    triptych = Image.new("RGB", (w * 3, h), (20, 20, 20))
    triptych.paste(off_norm, (0, 0))
    triptych.paste(nat_norm, (w, 0))
    triptych.paste(diff_norm, (w * 2, 0))
    return triptych


def classify_lcd_content(img: Image.Image) -> str:
    """
    Heuristic screen classifier (Feature 12):
    - 'menu': Rich variety of colorful icons (color_ratio > 0.10)
    - 'error_dialog': Mostly monochrome with red border/title (red_ratio > 0.005)
    - 'texto_denso': Standard monochrome/low-color calculation or text screen
    """
    raw_bytes = img.convert("RGB").tobytes()
    total_pixels = len(raw_bytes) // 3
    if total_pixels == 0:
        return "desconocida"

    colorful = 0
    reddish = 0
    for i in range(0, len(raw_bytes), 3):
        r, g, b = raw_bytes[i], raw_bytes[i + 1], raw_bytes[i + 2]
        if (max(r, g, b) - min(r, g, b)) > 50:
            colorful += 1
        if r > 180 and g < 50 and b < 50:
            reddish += 1

    color_ratio = colorful / total_pixels
    red_ratio = reddish / total_pixels

    if color_ratio > 0.10:
        return "menu"
    elif red_ratio > 0.005:
        return "error_dialog"
    else:
        return "texto_denso"


def capture_window_client_image(hwnd: int) -> Image.Image:
    """
    Offscreen client area capture using Win32 PrintWindow.
    """
    import ctypes
    import ctypes.wintypes as wt
    user32 = ctypes.windll.user32
    gdi32 = ctypes.windll.gdi32

    if not user32.IsWindow(hwnd):
        raise FrameCaptureError(f"Invalid window HWND: {hwnd}")

    if user32.IsIconic(hwnd):
        raise FrameCaptureError(f"Window is minimized: HWND {hwnd}")

    rect = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top

    if w <= 0 or h <= 0:
        raise FrameCaptureError(f"Window client area has invalid dimensions {w}x{h}: HWND {hwnd}")

    hdc_src = user32.GetDC(hwnd)
    if not hdc_src:
        raise FrameCaptureError(f"GetDC failed for HWND {hwnd}")

    hdc_mem = gdi32.CreateCompatibleDC(hdc_src)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_src, w, h)
    old_bmp = gdi32.SelectObject(hdc_mem, hbmp)

    # PW_CLIENTONLY = 1
    pw_ok = user32.PrintWindow(hwnd, hdc_mem, 1)

    import io
    # Extract DIB bits
    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ("biSize", wt.DWORD),
            ("biWidth", wt.LONG),
            ("biHeight", wt.LONG),
            ("biPlanes", wt.WORD),
            ("biBitCount", wt.WORD),
            ("biCompression", wt.DWORD),
            ("biSizeImage", wt.DWORD),
            ("biXPelsPerMeter", wt.LONG),
            ("biYPelsPerMeter", wt.LONG),
            ("biClrUsed", wt.DWORD),
            ("biClrImportant", wt.DWORD)
        ]

    bih = BITMAPINFOHEADER()
    bih.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bih.biWidth = w
    bih.biHeight = -h  # top-down
    bih.biPlanes = 1
    bih.biBitCount = 32
    bih.biCompression = 0  # BI_RGB

    buf_len = w * h * 4
    buf = ctypes.create_string_buffer(buf_len)
    gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bih), 0)

    # Cleanup GDI handles
    gdi32.SelectObject(hdc_mem, old_bmp)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(hwnd, hdc_src)

    raw_img = Image.frombytes("RGBA", (w, h), bytes(buf), "raw", "BGRA")
    return raw_img.convert("RGB")


def capture_normalized_framebuffers(native_hwnd: int, official_hwnd: int) -> Tuple[Image.Image, Image.Image]:
    """
    Contract M2 ↔ M3:
    Input: HWNDs of native and official emulator windows.
    Output: Tuple of two PIL.Image objects, both normalized to 384x216, RGB 24-bit, upright orientation.
    """
    img_native_raw = capture_window_client_image(native_hwnd)
    img_official_raw = capture_window_client_image(official_hwnd)

    norm_native = normalize_lcd(img_native_raw, backend="native", rotate_native=True)
    norm_official = normalize_lcd(img_official_raw, backend="official", rotate_native=False)

    return norm_native, norm_official


def main(argv=None):
    import argparse
    import json

    parser = argparse.ArgumentParser(description="Canonical LCD Normalization & Pixel-by-Pixel Differential Engine")
    parser.add_argument("image_a", help="Path to first image (e.g. official reference)")
    parser.add_argument("image_b", help="Path to second image (e.g. native capture)")
    parser.add_argument("--threshold", "-t", type=int, default=DEFAULT_THRESHOLD, help="Max channel delta threshold")
    parser.add_argument("--out-diff", "-o", help="Path to save magenta differential highlight image")
    parser.add_argument("--triptych", help="Path to save 3-panel composite triptych")
    parser.add_argument("--rotate-b", action="store_true", help="Rotate second image 180° before comparison")
    parser.add_argument("--json", action="store_true", default=True, help="Output structured JSON summary")

    args = parser.parse_args(argv)

    if not os.path.exists(args.image_a):
        print(f"Error: image_a not found: {args.image_a}", file=sys.stderr)
        return 2
    if not os.path.exists(args.image_b):
        print(f"Error: image_b not found: {args.image_b}", file=sys.stderr)
        return 2

    img_a = Image.open(args.image_a)
    img_b = Image.open(args.image_b)

    norm_a = normalize_lcd(img_a, backend="official", rotate_native=False)
    norm_b = normalize_lcd(img_b, backend="native" if args.rotate_b else "official", rotate_native=args.rotate_b)

    res = calculate_pixel_diff(norm_a, norm_b, threshold=args.threshold)

    if args.out_diff:
        os.makedirs(os.path.dirname(os.path.abspath(args.out_diff)), exist_ok=True)
        diff_img = generate_magenta_diff_image(norm_a, norm_b, threshold=args.threshold)
        diff_img.save(args.out_diff)
        res["diff_image"] = args.out_diff

    if args.triptych:
        os.makedirs(os.path.dirname(os.path.abspath(args.triptych)), exist_ok=True)
        diff_img = generate_magenta_diff_image(norm_a, norm_b, threshold=args.threshold)
        trip_img = generate_triptych(norm_a, norm_b, diff_img)
        trip_img.save(args.triptych)
        res["triptych_image"] = args.triptych

    print(json.dumps(res, indent=2))
    return 0 if res.get("match") else 1


if __name__ == "__main__":
    sys.exit(main())
