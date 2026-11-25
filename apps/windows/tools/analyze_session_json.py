#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
analyze_session_json.py — Analiza un export de sesión (opencode JSON) y resume
los hallazgos clave: mensajes de usuario, archivos tocados, conclusiones.

Uso: python analyze_session_json.py <sesion.json> [salida.txt]
Herramienta de investigación del proyecto Calculadora Unificada (2026-10-16).
"""
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
msgs = data["messages"]
print(f"SESION: {data['info'].get('slug')} / {data['info'].get('title')}")
print(f"MENSAJES: {len(msgs)}")

user_texts = []
file_writes = set()
assistant_summaries = []
for m in msgs:
    role = m.get("info", {}).get("role", "?")
    for part in m.get("parts", []):
        ptype = part.get("type")
        text = part.get("text", "") or ""
        if role == "user" and ptype == "text" and len(text) > 10:
            user_texts.append(text[:600].replace("\n", " "))
        if ptype == "tool":
            state = part.get("state", {})
            inp = json.dumps(state.get("input", {}), ensure_ascii=False)
            for match in re.findall(r'[A-Za-z]:\\[^\\"\n]+', inp):
                file_writes.add(match[:120])
        if ptype == "step-start":
            pass

print("\n=== MENSAJES DEL USUARIO (únicos) ===")
seen = set()
for t in user_texts:
    key = t[:80]
    if key in seen:
        continue
    seen.add(key)
    print(" *", t[:400])

print(f"\n=== RUTAS MENCIONADAS EN LLAMADAS A HERRAMIENTAS ({len(file_writes)}) ===")
for f in sorted(file_writes):
    print(" -", f)

# últimos mensajes sustantivos del asistente
print("\n=== ULTIMOS TEXTOS DEL ASISTENTE ===")
count = 0
for m in reversed(msgs):
    if count >= 4:
        break
    if m.get("info", {}).get("role") != "assistant":
        continue
    for part in m.get("parts", []):
        if part.get("type") == "text" and len(part.get("text", "")) > 200:
            print("-----")
            print(part["text"][:2500])
            count += 1
            break
