import json

path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\verification\extreme_math_gallery\audit_log.json"
with open(path, "r", encoding="utf-8") as f:
    steps = json.load(f)

print(f"Total de pasos registrados: {len(steps)}")
for s in steps:
    txt = s['ocr_text'].replace('\n', ' ').strip()
    print(f"{s['step']:36s} | Pass: {str(s['passed']):5s} | OCR: {txt[:45]}")
