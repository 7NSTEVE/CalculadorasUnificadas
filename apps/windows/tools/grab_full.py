#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""grab_full.py — Captura de pantalla completa a PNG.
Uso: python grab_full.py <salida.png>"""
import sys
from PIL import ImageGrab

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if len(sys.argv) < 2:
    print(__doc__)
    sys.exit(1)
ImageGrab.grab().save(sys.argv[1])
print("OK", sys.argv[1])
