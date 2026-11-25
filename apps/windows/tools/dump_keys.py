import re

path = r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\cg50_keys_exact.h"
with open(path, "r", encoding="utf-8") as f:
    text = f.read()

pattern = re.compile(r'\{\s*(\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*L"([^"]+)"\s*\}')
for m in pattern.finditer(text):
    print(f"ID={int(m.group(1)):2d}  Matrix={m.group(6)}  Name={m.group(7)}")
