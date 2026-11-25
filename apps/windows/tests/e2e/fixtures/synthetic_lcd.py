#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
synthetic_lcd.py — Synthetic LCD image generators and helpers for E2E tests.
Generates pixel-accurate representations of Casio fx-CG50 LCD states:
- Blank / boot screens
- Main menu screens
- Calculation screens with expression & result
- Ma ERROR dialogs
- Inverted (180°) native framebuffers
- Synthetic pixel discrepancies for differential testing
"""

import io
from typing import Tuple, Optional
from PIL import Image, ImageDraw, ImageFont, ImageChops

# Canonical Casio fx-CG50 display dimensions
CANONICAL_WIDTH = 384
CANONICAL_HEIGHT = 216
ALT_WIDTH = 396
ALT_HEIGHT = 224

# Characteristic Casio fx-CG50 UI Colors
COLOR_WHITE = (255, 255, 255)
COLOR_BLACK = (0, 0, 0)
COLOR_STATUS_BG = (230, 230, 230)
COLOR_STATUS_TEXT = (40, 40, 40)
COLOR_HEADER_BLUE = (0, 102, 204)
COLOR_ERROR_BG = (255, 255, 255)
COLOR_ERROR_BORDER = (200, 0, 0)
COLOR_DIFF_MAGENTA = (255, 0, 255)


def create_blank_lcd(width: int = CANONICAL_WIDTH, height: int = CANONICAL_HEIGHT,
                     color: Tuple[int, int, int] = COLOR_WHITE) -> Image.Image:
    """Create a solid color LCD image."""
    return Image.new("RGB", (width, height), color)


def create_menu_lcd(width: int = CANONICAL_WIDTH, height: int = CANONICAL_HEIGHT) -> Image.Image:
    """
    Create a synthetic representation of the fx-CG50 main icon menu:
    - Status bar on top
    - Grid of colored app icons (Run-Matrix, Statistics, eActivity, etc.)
    - Function key bar at bottom
    """
    img = Image.new("RGB", (width, height), COLOR_WHITE)
    draw = ImageDraw.Draw(img)

    # Status Bar
    draw.rectangle([0, 0, width, 18], fill=COLOR_STATUS_BG)
    draw.text((10, 3), "MAIN MENU", fill=COLOR_STATUS_TEXT)
    draw.text((width - 50, 3), "DEG", fill=COLOR_STATUS_TEXT)

    # App icons grid (4 columns, 3 rows)
    icons = [
        ("Run-Matrix", (0, 120, 215)),
        ("Statistics", (232, 17, 35)),
        ("eActivity", (0, 153, 188)),
        ("Spreadsheet", (16, 124, 65)),
        ("Graph", (122, 117, 116)),
        ("Dyna Graph", (104, 33, 122)),
        ("Table", (0, 120, 215)),
        ("Recursion", (16, 124, 65)),
    ]

    icon_w, icon_h = 60, 45
    start_x, start_y = 20, 30
    x_spacing, y_spacing = 80, 55

    for idx, (name, icon_col) in enumerate(icons):
        row = idx // 4
        col = idx % 4
        x0 = start_x + col * x_spacing
        y0 = start_y + row * y_spacing
        draw.rectangle([x0, y0, x0 + icon_w, y0 + icon_h], fill=icon_col, outline=COLOR_BLACK)
        draw.text((x0 + 4, y0 + icon_h + 2), name[:8], fill=COLOR_BLACK)

    # Function key guide bar
    draw.rectangle([0, height - 16, width, height], fill=COLOR_STATUS_BG)
    draw.text((20, height - 14), "F1", fill=COLOR_STATUS_TEXT)
    draw.text((80, height - 14), "F2", fill=COLOR_STATUS_TEXT)
    draw.text((140, height - 14), "F3", fill=COLOR_STATUS_TEXT)

    return img


def create_calc_screen(expression: str = "1+2", result: str = "3",
                       width: int = CANONICAL_WIDTH, height: int = CANONICAL_HEIGHT) -> Image.Image:
    """
    Create a synthetic fx-CG50 calculation screen (Run-Matrix mode):
    - Top status bar
    - Left-aligned input expression
    - Right-aligned calculation result
    """
    img = Image.new("RGB", (width, height), COLOR_WHITE)
    draw = ImageDraw.Draw(img)

    # Status Bar
    draw.rectangle([0, 0, width, 18], fill=COLOR_STATUS_BG)
    draw.text((10, 3), "Rad  Norm1  d/c  Real", fill=COLOR_STATUS_TEXT)

    # Expression line (upper section, left aligned)
    draw.text((15, 35), expression, fill=COLOR_BLACK)

    # Result line (right-ish aligned)
    # Estimate width per char ~7px
    res_x = width - 30 - (len(result) * 10)
    draw.text((res_x, 60), result, fill=COLOR_BLACK)

    # Soft prompt cursor line
    draw.line([0, height - 20, width, height - 20], fill=(210, 210, 210))

    return img


def create_error_screen(error_type: str = "Ma ERROR",
                        width: int = CANONICAL_WIDTH, height: int = CANONICAL_HEIGHT) -> Image.Image:
    """
    Create a synthetic fx-CG50 error dialog box (e.g., Ma ERROR for division by zero):
    - Darkened / dimmed background
    - Centered white dialog box with red border
    - Error title and prompt: [AC] :Cancel  [EXIT]:Goto
    """
    img = Image.new("RGB", (width, height), (220, 220, 220))
    draw = ImageDraw.Draw(img)

    # Centered Error Box
    box_w, box_h = 240, 100
    x0 = (width - box_w) // 2
    y0 = (height - box_h) // 2
    x1, y1 = x0 + box_w, y0 + box_h

    # Dialog background with double red border
    draw.rectangle([x0, y0, x1, y1], fill=COLOR_WHITE, outline=COLOR_ERROR_BORDER, width=2)
    draw.rectangle([x0 + 3, y0 + 3, x1 - 3, y1 - 3], outline=COLOR_ERROR_BORDER, width=1)

    # Error text
    draw.text((x0 + 75, y0 + 15), error_type, fill=COLOR_ERROR_BORDER)
    draw.text((x0 + 20, y0 + 50), "[AC]   :Cancel", fill=COLOR_BLACK)
    draw.text((x0 + 20, y0 + 70), "[EXIT] :Goto", fill=COLOR_BLACK)

    return img


def apply_rotation(img: Image.Image, degrees: int = 180) -> Image.Image:
    """Rotate image by specified angle (180 for native LCD orientation normalization)."""
    if degrees == 180:
        return img.transpose(Image.Transpose.ROTATE_180)
    elif degrees == 90:
        return img.transpose(Image.Transpose.ROTATE_90)
    elif degrees == 270:
        return img.transpose(Image.Transpose.ROTATE_270)
    return img


def inject_pixel_discrepancy(img: Image.Image,
                             bbox: Tuple[int, int, int, int] = (180, 45, 240, 75),
                             color: Tuple[int, int, int] = (255, 0, 0)) -> Image.Image:
    """
    Return a copy of the image with a defined region filled with differing pixels.
    Used to generate controlled differential test cases.
    """
    modified = img.copy()
    draw = ImageDraw.Draw(modified)
    x0, y0, x1, y1 = bbox
    # In Pillow, draw.rectangle includes both endpoints. To make getbbox() == bbox,
    # draw up to x1 - 1, y1 - 1.
    draw.rectangle([x0, y0, max(x0, x1 - 1), max(y0, y1 - 1)], fill=color)
    return modified


def calculate_pixel_diff(img_a: Image.Image, img_b: Image.Image, threshold: int = 4) -> dict:
    """
    Calculate pixel-by-pixel difference matching img_diff.py specifications:
    - Verifies identical dimensions
    - Counts pixels with max color channel delta > threshold
    - Computes bounding box of differences
    - Returns structured summary
    """
    if img_a.size != img_b.size:
        return {
            "error": "TAMANOS_DISTINTOS",
            "size_a": img_a.size,
            "size_b": img_b.size,
            "match": False
        }

    rgb_a = img_a.convert("RGB")
    rgb_b = img_b.convert("RGB")
    diff = ImageChops.difference(rgb_a, rgb_b)
    bbox = diff.getbbox()
    raw_bytes = diff.tobytes()
    total_pixels = len(raw_bytes) // 3
    changed = 0
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


def generate_magenta_diff_image(img_a: Image.Image, img_b: Image.Image, threshold: int = 4) -> Image.Image:
    """
    Generate visual differential artifact:
    Base image with all differing pixels highlighted in bright magenta (255, 0, 255).
    """
    rgb_a = img_a.convert("RGB")
    rgb_b = img_b.convert("RGB")
    diff = ImageChops.difference(rgb_a, rgb_b)
    
    out = rgb_a.copy()
    px_diff = diff.load()
    px_out = out.load()
    w, h = out.size

    for y in range(h):
        for x in range(w):
            r, g, b = px_diff[x, y]
            if max(r, g, b) > threshold:
                px_out[x, y] = COLOR_DIFF_MAGENTA

    return out
