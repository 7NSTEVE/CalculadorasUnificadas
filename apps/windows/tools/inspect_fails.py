import json
import re

path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\extreme_math_gallery\index.html"
with open(path, "r", encoding="utf-8") as f:
    text = f.read()

pattern = re.compile(
    r'<td><strong>#(\d+)</strong></td>\s*'
    r'<td><strong>([^<]+)</strong></td>\s*'
    r'<td><span class="badge ([^"]+)">([^<]+)</span></td>.*?'
    r'<div class="ocr-text">([^<]*)</div>\s*'
    r'<small class="tokens">([^<]*)</small>',
    re.DOTALL
)

matches = pattern.findall(text)
for m in matches:
    step, name, badge_cls, badge, ocr, tokens = m
    if badge == "FAIL":
        print(f"=== FAIL #{step} {name} ===")
        print(f"Expected Tokens: {tokens}")
        print(f"OCR Full Text:\n{ocr.strip()}")
        print("-" * 50)
