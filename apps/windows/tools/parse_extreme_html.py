import re

path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\extreme_math_gallery\index.html"
with open(path, "r", encoding="utf-8") as f:
    text = f.read()

pattern = re.compile(
    r'<td><strong>#(\d+)</strong></td>\s*'
    r'<td><strong>([^<]+)</strong></td>\s*'
    r'<td><span class="badge ([^"]+)">([^<]+)</span></td>.*?'
    r'<div class="ocr-text">([^<]*)</div>',
    re.DOTALL
)

matches = pattern.findall(text)
print(f"Total pasos encontrados: {len(matches)}")
pass_count = sum(1 for m in matches if m[3] == "PASS")
fail_count = sum(1 for m in matches if m[3] == "FAIL")
print(f"PASS: {pass_count} | FAIL: {fail_count}")
for m in matches:
    step, name, badge_cls, badge, ocr = m
    clean_ocr = ocr.replace('\n', ' ').strip()
    print(f"#{int(step):2d} | {name:38s} | {badge:4s} | OCR: {clean_ocr[:45]}")
