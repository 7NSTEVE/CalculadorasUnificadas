"""Formulario oficial exacto del III Parcial de Física General III (FI2103 · II-2026).

Fuente verificada visualmente página por página:
  `D:\\Descargas\\documentos-ii-2026_Ecuaciones_Tercer_Parcial_FG3.pdf`
  ("Ecuaciones y constantes importantes", 14 filas × 3 columnas = 42 celdas).

Regla académica del examen:
  Toda solución de desarrollo debe partir exclusivamente de estas 32 ecuaciones base y
  10 constantes/conversiones. Cualquier otra expresión (resistencia térmica, cilindro/esfera
  integrados, ΔA = 2αA₀ΔT, β = 3α, C_p = γR/(γ−1), C_V = R/(γ−1), pM = ρRT, eficiencia de
  Carnot, etc.) debe obtenerse explícitamente igualando, sustituyendo o despejando a partir
  de las ecuaciones base de esta hoja.
"""

from __future__ import annotations

import re
from typing import Any, Dict, List, Tuple

# Las 32 ecuaciones base exactas impresas en la hoja oficial II-2026
ECUACIONES_BASE_II_2026: Tuple[Tuple[str, str], ...] = (
    # Dilatación y esfuerzo térmico (Fila 1)
    ("F1C1", r"\Delta L = \alpha L_0 \Delta T"),
    ("F1C2", r"\Delta V = \beta V_0 \Delta T"),
    ("F1C3", r"\frac{F}{A} = -Y \alpha \Delta T"),
    # Conducción y radiación (Filas 2 y 3)
    ("F2C1", r"H = \frac{dQ}{dt} = kA\frac{T_H - T_C}{L}"),
    ("F2C2", r"H = -kA\frac{dT}{dx}"),
    ("F2C3", r"H = Ae\sigma T^4"),
    ("F3C1", r"H = Ae\sigma (T^4 - T_s^4)"),
    # Calorimetría (Fila 3)
    ("F3C2", r"Q = mc\Delta T = nC\Delta T"),
    ("F3C3", r"Q = \pm mL"),
    # Gases ideales y teoría cinética (Filas 4 y 5)
    ("F4C1", r"pV = nRT"),
    ("F4C2", r"m_{\text{total}} = nM"),
    ("F4C3", r"K = \frac{3}{2}nRT"),
    ("F5C1", r"v_{\text{rms}} = \sqrt{\frac{3k_B T}{m}} = \sqrt{\frac{3RT}{M}}"),
    ("F5C2", r"\frac{1}{2}m(v^2)_{\text{med}} = \frac{3k_B T}{2}"),
    ("F5C3", r"M = N_A m"),
    # Trabajo termodinámico (Fila 6)
    ("F6C1", r"W = \int_{V_1}^{V_2} p\,dV"),
    ("F6C2", r"W = p(V_2 - V_1)"),
    ("F6C3", r"W = nRT\ln\left(\frac{V_2}{V_1}\right)"),
    # Primera ley y calores específicos (Filas 7 y 8)
    ("F7C1", r"\Delta U = Q - W"),
    ("F7C2", r"\Delta U = nC_V\Delta T"),
    ("F7C3", r"C_P = C_V + R"),
    ("F8C1", r"C_{V\text{mono}} = \frac{3}{2}R"),
    ("F8C2", r"C_{V\text{dia}} = \frac{5}{2}R"),
    ("F8C3", r"\gamma = \frac{C_P}{C_V}"),
    # Procesos adiabáticos (Fila 9)
    ("F9C1", r"W = \frac{1}{\gamma - 1}(p_1 V_1 - p_2 V_2)"),
    ("F9C2", r"pV^\gamma = \text{constante}"),
    ("F9C3", r"TV^{\gamma - 1} = \text{constante}"),
    # Máquinas térmicas, refrigeradores, calor diferencial y Otto (Filas 10, 11 y 12)
    ("F10C1", r"e = \frac{W}{Q_H} = 1 - \left|\frac{Q_c}{Q_H}\right|"),
    ("F10C2", r"K = \frac{|Q_c|}{|W|} = \frac{|Q_c|}{|Q_H| - |Q_c|}"),
    ("F10C3", r"T_K = T_{^\circ\text{C}} + 273{,}15"),
    ("F11C1", r"dQ = nC_V dT"),
    ("F11C2", r"dQ = nC_p dT"),
    ("F11C3", r"e_{\text{otto}} = 1 - \frac{1}{r^{\gamma - 1}}"),
    ("F12C1", r"r = \frac{V_{\text{máx}}}{V_{\text{mín}}}"),
    ("F12C2", r"T_{^\circ\text{F}} = \frac{9}{5}T_{^\circ\text{C}} + 32"),
)

# Las 7 constantes y conversiones impresas al pie de la hoja oficial II-2026 (completan las 42 celdas)
CONSTANTES_II_2026: Tuple[Tuple[str, str], ...] = (
    ("F12C3", r"R = 8{,}314\ \frac{\text{J}}{\text{mol}\cdot\text{K}}"),
    ("F13C1", r"k_B = 1{,}38\times 10^{-23}\ \frac{\text{J}}{\text{K}}"),
    ("F13C2", r"N_A = 6{,}022\times 10^{23}\ \text{mol}^{-1}"),
    ("F13C3", r"\sigma = 5{,}67\times 10^{-8}\ \frac{\text{W}}{\text{m}^2\cdot\text{K}^4}"),
    ("F14C1", r"1\ \text{atm} = 101325\ \text{Pa}"),
    ("F14C2", r"1\ \text{L} = 1000\ \text{cm}^3"),
    ("F14C3", r"R = 0{,}08206\ \frac{\text{L}\cdot\text{atm}}{\text{mol}\cdot\text{K}}"),
)

FORMULARIO_PROMPT_TXT = """REGLA OBLIGATORIA DEL FORMULARIO OFICIAL III PARCIAL FI2103 II-2026 (Ecuaciones_Tercer_Parcial_FG3.pdf):
En problemas de desarrollo de Física General III, SOLO puedes partir de las ecuaciones base impresas en el formulario oficial:
- Dilatación/esfuerzo: ΔL = α L₀ ΔT ; ΔV = β V₀ ΔT ; F/A = −Y α ΔT
- Conducción/radiación: H = dQ/dt = kA(T_H − T_C)/L ; H = −kA dT/dx ; H = AeσT⁴ ; H = Aeσ(T⁴ − T_s⁴)
- Calorimetría: Q = mcΔT = nCΔT ; Q = ±mL
- Gases ideales y cinética: pV = nRT ; m_total = nM ; K = (3/2)nRT ; v_rms = √(3k_B T/m) = √(3RT/M) ; (1/2)m(v²)_med = 3k_B T/2 ; M = N_A m
- Trabajo y 1.ª ley: W = ∫ p dV ; W = p(V₂ − V₁) ; W = nRT ln(V₂/V₁) ; ΔU = Q − W ; ΔU = n C_V ΔT ; C_P = C_V + R ; C_Vmono = (3/2)R ; C_Vdia = (5/2)R ; γ = C_P/C_V ; dQ = n C_V dT ; dQ = n C_p dT
- Adiabáticos: W = (1/(γ − 1))(p₁V₁ − p₂V₂) ; p V^γ = cte ; T V^(γ−1) = cte
- Máquinas/Otto: e = W/Q_H = 1 − |Q_c/Q_H| ; K = |Q_c|/|W| = |Q_c|/(|Q_H| − |Q_c|) ; e_otto = 1 − 1/r^(γ−1) ; r = V_máx/V_mín
- Temperaturas y constantes: T_K = T_°C + 273,15 ; T_°F = (9/5)T_°C + 32 ; R = 8,314 J/(mol·K) = 0,08206 L·atm/(mol·K) ; k_B = 1,38×10⁻²³ J/K ; N_A = 6,022×10²³ mol⁻¹ ; σ = 5,67×10⁻⁸ W/(m²·K⁴) ; 1 atm = 101325 Pa ; 1 L = 1000 cm³.

ESTÁ PROHIBIDO sacar de la nada atajos que NO están en esta hoja sin derivarlos antes desde las ecuaciones base:
1) Resistencia térmica (R_th = L/(kA), R_cil, R_esf): parte de H = kA(T_H − T_C)/L o H = −kA dT/dr, despeja ΔT o iguala H₁ = H₂ y suma caídas.
2) Conducción cilíndrica/esférica/cónica/área variable: parte SIEMPRE de H = −kA dT/dx (o dT/dr), sustituye A(r) o A(x), separa variables e integra.
3) ΔA = 2α A₀ ΔT y β = 3α: parte de ΔL = α L₀ ΔT (L = L₀(1 + αΔT)) y eleva al cuadrado o al cubo despreciando (αΔT)².
4) C_p = γR/(γ − 1) y C_V = R/(γ − 1): despeja a partir de C_P = C_V + R y γ = C_P/C_V.
5) ρ = pM/(RT): despeja a partir de pV = nRT y m_total = nM.
6) W = nRT ln(p₁/p₂): parte de W = nRT ln(V₂/V₁) y sustituye V₂/V₁ = p₁/p₂ desde pV = nRT.
7) Eficiencia o K de Carnot (1 − T_C/T_H, T_C/(T_H − T_C)): NO están impresas; parte de e = 1 − |Q_c/Q_H| o K = |Q_c|/(|Q_H| − |Q_c|) y deduce |Q_c/Q_H| = T_C/T_H con las isotérmicas y adiabáticas (o indica explícitamente que Carnot no figura en el formulario II-2026)."""

# Reglas de verificación: (nombre, regex_atajo_no_en_hoja, regex_derivacion_desde_hoja)
_ATAJOS_SIN_DERIVAR: Tuple[Tuple[str, re.Pattern[str], re.Pattern[str]], ...] = (
    (
        "Resistencia térmica (R_th / R_cil / R_esf) usada sin partir de H = kAΔT/L o H = −kA dT/dr",
        re.compile(r"R_\{?(?:th|t|cil|esf|eq|tot)\}?|\\frac\{L_?\d?\}\{k_?\d?A"),
        re.compile(r"H\s*=\s*-?\s*k\s*A|H\s*=\s*k_?\d?\s*A|H_1\s*=\s*H_2|\\frac\{dT\}\{d[xr]\}"),
    ),
    (
        "Fórmula cilíndrica 2πkLΔT/ln(r₂/r₁) sin derivar integrando H = −kA dT/dr",
        re.compile(r"2\\pi\s*k\s*L.*\\ln"),
        re.compile(r"\\int|\\frac\{dT\}\{dr\}"),
    ),
    (
        "Fórmula esférica 4πkr₁r₂ΔT/(r₂−r₁) sin derivar integrando H = −kA dT/dr",
        re.compile(r"4\\pi\s*k.*(?:\\frac\{1\}\{r|r_1\s*r_2)"),
        re.compile(r"\\int|\\frac\{dT\}\{dr\}"),
    ),
    (
        "Cono truncado πkR₀R₁ΔT/L sin derivar integrando H = −kA(x) dT/dx",
        re.compile(r"\\pi\s*k_?[A-Za-z]?\s*R_0\s*R_1"),
        re.compile(r"\\int|\\frac\{dT\}\{dx\}"),
    ),
    (
        "C_p = γR/(γ−1) o C_V = R/(γ−1) sin despejar desde C_P = C_V + R y γ = C_P/C_V",
        re.compile(r"C_[pP]\s*=\s*\\frac\{\\gamma\s*R\}\{\\gamma\s*[-−]\s*1\}|C_V\s*=\s*\\frac\{R\}\{\\gamma\s*[-−]\s*1\}"),
        re.compile(r"C_[pP]\s*=\s*C_V\s*\+\s*R|C_[pP]\s*[-−]\s*C_V\s*=\s*R"),
    ),
    (
        "ΔA = 2αA₀ΔT usada sin derivarla desde ΔL = αL₀ΔT",
        re.compile(r"\\Delta\s*A\s*=\s*2\\alpha"),
        re.compile(r"\(1\s*\+\s*\\alpha\s*\\,?\s*\\Delta\s*T\)\^2|\\Delta\s*L\s*=\s*\\alpha\s*L_0"),
    ),
    (
        "β = 3α usada sin derivarla desde ΔL = αL₀ΔT",
        re.compile(r"\\beta\s*=\s*3\\alpha"),
        re.compile(r"\(1\s*\+\s*\\alpha\s*\\,?\s*\\Delta\s*T\)\^3|L_0\^3|\\Delta\s*L\s*=\s*\\alpha\s*L_0"),
    ),
    (
        "ρ = pM/(RT) usada sin despejar desde pV = nRT y m_total = nM",
        re.compile(r"\\rho\s*=\s*\\frac\{p\s*M\}\{R\s*T\}|p\s*M\s*=\s*\\rho\s*R\s*T"),
        re.compile(r"p\s*V\s*=\s*n\s*R\s*T"),
    ),
    (
        "W = nRT ln(p₁/p₂) sin pasar por W = nRT ln(V₂/V₁) y pV = nRT",
        re.compile(r"n\s*R\s*T\s*\\ln\s*(?:\\left)?\(\s*\\frac\{p_1\}\{p_2\}"),
        re.compile(r"\\frac\{V_2\}\{V_1\}|p_1\s*V_1\s*=\s*p_2\s*V_2"),
    ),
)


def verificar_origen_formulario(lec: Dict[str, Any]) -> List[str]:
    """Comprueba que toda lección de desarrollo parta de las ecuaciones base del formulario II-2026."""
    if lec.get("tipo") in ("seleccion", "teoria"):
        return []
    mates: List[str] = []
    for p in lec.get("pasos") or []:
        for tr in p.get("tramos") or []:
            for acc in tr.get("acc") or []:
                if isinstance(acc, dict) and acc.get("a") == "mate" and acc.get("tex"):
                    mates.append(str(acc["tex"]))
        for l in p.get("lento") or []:
            if isinstance(l, dict) and l.get("tex"):
                mates.append(str(l["tex"]))
    if not mates:
        return []
    blob = "\n".join(mates)
    faltas: List[str] = []
    for desc, pat_atajo, pat_base in _ATAJOS_SIN_DERIVAR:
        if pat_atajo.search(blob) and not pat_base.search(blob):
            faltas.append(desc)
    return faltas
