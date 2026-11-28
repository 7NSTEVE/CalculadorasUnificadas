"""Generador determinista de variantes paramétricas para los arquetipos críticos del III Parcial.

Cambia los valores numéricos y condiciones conservando la estructura física, recalcula TODOS
los pasos en Python (cero improvisación de IA, cero cuota), cumple dsl.revisar() y pasa
la auditoría física de 10 niveles.
"""

from __future__ import annotations

import math
import random
from typing import Callable, Dict, Optional

from . import registro
from .ayudas import Gas, visual_pv
from .dsl import Leccion

SIGMA = 5.670374419e-8
R_GAS = 8.314


def fmt(x: float, dec: int = 2) -> str:
    """Formatea un número en TeX con coma decimal ({,}) y sin -0."""
    v = 0.0 if abs(x) < 10.0 ** (-(dec + 3)) else x
    if dec <= 0:
        return f"{round(v):d}"
    return f"{v:.{dec}f}".replace(".", "{,}")


def fmt_txt(x: float, dec: int = 2) -> str:
    return fmt(x, dec).replace("{,}", ",")


def _v_conduccion_area(rng: random.Random) -> Leccion:
    """Arquetipo crítico tipo Q049: barra piramidal con área transversal variable A(y) = A0 - b y."""
    mat, k = rng.choice([("aluminio", 235.0), ("cobre", 385.0), ("latón", 109.0), ("acero", 50.2)])
    T1 = float(rng.choice([160, 180, 200, 220, 250]))
    T2 = float(rng.choice([15, 20, 25, 30]))
    L = rng.choice([0.36, 0.40, 0.48, 0.50, 0.60])
    A0 = rng.choice([0.160, 0.200, 0.240, 0.250])
    A_top = round(A0 * rng.choice([0.55, 0.60, 0.65]), 4)
    b = round((A0 - A_top) / L, 4)
    A_top = A0 - b * L

    s_inf = math.sqrt(A0)
    s_sup = math.sqrt(A_top)
    I_tot = (1.0 / b) * math.log(A0 / A_top)
    H = k * (T1 - T2) / I_tot
    y_mid = L / 2.0
    A_mid = A0 - b * y_mid
    I_mid = (1.0 / b) * math.log(A0 / A_mid)
    T_mid = T1 - (H / k) * I_mid

    pts = []
    for i in range(41):
        yi = L * i / 40.0
        Ai = A0 - b * yi
        Ii = (1.0 / b) * math.log(A0 / Ai)
        Ti = T1 - (H / k) * Ii
        pts.append([round(yi, 4), round(Ti, 2)])
    c_forma = round(b * L / A0, 4)

    lec = Leccion(
        f"VAR-AREA-{rng.randint(100,999)}",
        f"Variante · Barra de {mat} con área variable A(y)",
        tema="conduccion",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Dos reservorios térmicos, uno a ${fmt(T1,1)}\\,^\\circ\\text{{C}}$ y otro a ${fmt(T2,1)}\\,^\\circ\\text{{C}}$, "
            f"se conectan mediante una barra de {mat} ($k = {fmt(k,1)}\\text{{ W/(m\\cdot K)}}$) de altura $L = {fmt(L,2)}\\text{{ m}}$ "
            f"y área transversal cuadrada $A(y) = {fmt(A0,3)} - {fmt(b,4)}y$ (en $\\text{{m}}^2$). "
            f"Calcule: A) los lados de la base y el tope, B) el flujo de calor $H$, y C) la temperatura a la mitad ($y = {fmt(y_mid,2)}\\text{{ m}}$)."
        ),
        visual={
            "tipo": "conduccion",
            "geo": "pared",
            "L": L,
            "T1": T1,
            "T2": T2,
            "eje": "y",
            "uT": "°C",
            "forma": f"Math.sqrt(1 - {c_forma} * u)",
            "pts": pts,
            "xm": y_mid,
            "etA": f"{fmt_txt(A0,3)}−{fmt_txt(b,4)}y",
        },
    ).usa(r"H = -k A(y)\frac{dT}{dy}", r"\int \frac{dy}{A_0 - b y} = -\frac{1}{b}\ln(A_0 - b y)")

    (
        lec.paso("A) Lados de la base y el tope")
        .por_que("El área es cuadrada ($A = s^2$), así que evaluamos $A(0)$ y $A(L)$ y sacamos raíz cuadrada.")
        .di(
            "En la base, y vale cero, así que el área es A subcero y su lado es la raíz cuadrada.",
            {"a": "mate", "tex": rf"A(0) = {fmt(A0,3)}\,\text{{m}}^2 \implies s_{{\text{{base}}}} = \sqrt{{{fmt(A0,3)}}} = {fmt(s_inf,3)}\,\text{{m}}", "color": "azul"},
        )
        .di(
            f"En el tope, y vale {fmt_txt(L,2)} metros, sustituimos en A de y y sacamos la raíz.",
            {"a": "mate", "tex": rf"A({fmt(L,2)}) = {fmt(A0,3)} - {fmt(b,4)}({fmt(L,2)}) = {fmt(A_top,4)}\,\text{{m}}^2 \implies s_{{\text{{tope}}}} = {fmt(s_sup,3)}\,\text{{m}}", "color": "verde"},
        )
        .sale_de((f"$A_0 = {fmt(A0,3)}$", "del enunciado en $y=0$"), (f"$b = {fmt(b,4)}$", "coeficiente de $y$ en $A(y)$"))
    )
    (
        lec.paso("B) Separar variables e integrar para hallar H")
        .por_que("Como el área cambia con $y$, no aplica $H = kA\\Delta T/L$ constante; hay que separar variables e integrar.")
        .di(
            "En estado estacionario el flujo H es constante a lo largo de la barra. Separamos y y la temperatura.",
            {"a": "mate", "tex": r"H\int_0^L \frac{dy}{A_0 - b y} = -k\int_{T_1}^{T_2} dT = k(T_1 - T_2)", "color": "azul"},
        )
        .di(
            "Usamos cambio de variable u igual a A subcero menos b por y para evaluar la integral geométrica.",
            {"a": "mate", "tex": rf"I = \frac{{1}}{{{fmt(b,4)}}}\ln\!\left(\frac{{{fmt(A0,3)}}}{{{fmt(A_top,4)}}}\right) = {fmt(I_tot,3)}\,\text{{m}}^{{-1}}"},
            {"a": "mate", "tex": rf"H = \frac{{({fmt(k,1)})({fmt(T1,1)} - {fmt(T2,1)})}}{{{fmt(I_tot,3)}}} = {fmt(H/1000,3)}\,\text{{kW}}\;({fmt(H,0)}\,\text{{W}})", "color": "verde", "rol": "resultado"},
        )
        .despacio(
            (r"u = A_0 - b y \implies du = -b\,dy \implies dy = -\frac{du}{b}", "Cambio de variable paso a paso sin saltos"),
            (r"\int_0^L \frac{dy}{A_0 - b y} = -\frac{1}{b}\left[\ln(A_0 - b y)\right]_0^L = \frac{1}{b}\ln\!\left(\frac{A_0}{A(L)}\right)", "El signo menos invierte los límites del logaritmo"),
        )
    )
    (
        lec.paso(f"C) Temperatura a la mitad (y = {fmt_txt(y_mid,2)} m)")
        .por_que("Integramos la misma ecuación de Fourier solo desde la base hasta la mitad de la barra.")
        .di(
            "Calculamos primero el área y la integral geométrica hasta la mitad.",
            {"a": "mate", "tex": rf"A({fmt(y_mid,2)}) = {fmt(A0,3)} - {fmt(b,4)}({fmt(y_mid,2)}) = {fmt(A_mid,4)}\,\text{{m}}^2"},
            {"a": "mate", "tex": rf"I_{{1/2}} = \frac{{1}}{{{fmt(b,4)}}}\ln\!\left(\frac{{{fmt(A0,3)}}}{{{fmt(A_mid,4)}}}\right) = {fmt(I_mid,3)}\,\text{{m}}^{{-1}}"},
        )
        .di(
            "Despejamos la temperatura en la mitad; queda más cerca de la cara ancha.",
            {"a": "mate", "tex": rf"T({fmt(y_mid,2)}\,\text{{m}}) = {fmt(T1,1)} - \frac{{{fmt(H,0)}}}{{{fmt(k,1)}}}({fmt(I_mid,3)}) = {fmt(T_mid,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"s_{{\text{{base}}}} = {fmt(s_inf,3)}\,\text{{m}},\; s_{{\text{{tope}}}} = {fmt(s_sup,3)}\,\text{{m}}", valor=round(s_inf, 3))
    lec.responde("B)", rf"H = {fmt(H/1000,3)}\,\text{{kW}}", valor=round(H / 1000, 3))
    lec.responde("C)", rf"T({fmt(y_mid,2)}\,\text{{m}}) = {fmt(T_mid,1)}\,^\circ\text{{C}}", valor=round(T_mid, 1))
    lec.te_toca(
        f"Si en lugar de {mat} ($k={fmt(k,1)}$) la conductividad se duplicara, ¿cuánto valdría el flujo $H$ en kW?",
        round(2 * H / 1000, 2),
        "kW",
        pista="H es directamente proporcional a k cuando la geometría y las temperaturas no cambian.",
        solucion=f"Al duplicar $k$, el flujo se duplica: $2 \\times {fmt(H/1000,2)} = {fmt(2*H/1000,2)}\\text{{ kW}}$.",
    )
    return lec


def _v_conduccion_k_radial(rng: random.Random) -> Leccion:
    """Arquetipo crítico tipo E8 / Q057: tubo cilíndrico con conductividad k(r) = k0 / r."""
    a = rng.choice([0.04, 0.05, 0.06, 0.08])
    b = round(a * rng.choice([2.0, 2.5, 3.0]), 3)
    L = rng.choice([1.0, 1.5, 2.0, 2.5])
    k0 = rng.choice([0.40, 0.60, 0.80, 1.20])
    Tint = float(rng.choice([90, 100, 120, 140]))
    Text = float(rng.choice([15, 20, 25, 30]))

    H = 2.0 * math.pi * k0 * L * (Tint - Text) / (b - a)
    r_mid = 0.5 * (a + b)
    T_mid = Tint - (Tint - Text) * (r_mid - a) / (b - a)

    lec = Leccion(
        f"VAR-KRAD-{rng.randint(100,999)}",
        "Variante · Tubo cilíndrico con conductividad k(r) = k₀/r",
        tema="conduccion",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un tubo de largo $L = {fmt(L,1)}\\text{{ m}}$, radio interno $a = {fmt(a,3)}\\text{{ m}}$ y radio externo $b = {fmt(b,3)}\\text{{ m}}$ "
            f"tiene conductividad variable $k(r) = k_0/r$ con $k_0 = {fmt(k0,2)}\\text{{ W/K}}$. "
            f"Si $T_{{\\text{{int}}}} = {fmt(Tint,1)}\\,^\\circ\\text{{C}}$ y $T_{{\\text{{ext}}}} = {fmt(Text,1)}\\,^\\circ\\text{{C}}$, "
            f"calcule el flujo radial de calor $H$ y la temperatura en $r = {fmt(r_mid,3)}\\text{{ m}}$."
        ),
        visual={"tipo": "conduccion", "caso": "cilindro", "r1": a, "r2": b, "T1": Tint, "T2": Text, "k0": k0, "L": L},
    ).usa(r"H = -k(r) A(r)\frac{dT}{dr}", r"A(r) = 2\pi r L")

    (
        lec.paso("Sustituir A(r) y k(r) en la ley de Fourier")
        .por_que("El radio $r$ se cancela exactamente al multiplicar $k_0/r$ por $2\\pi r L$.")
        .di(
            "El área lateral de un cilindro de radio r y largo L es dos pi por r por L. Al multiplicar, r se cancela.",
            {"a": "mate", "tex": r"H = -\left(\frac{k_0}{r}\right)(2\pi r L)\frac{dT}{dr} = -2\pi k_0 L \frac{dT}{dr}", "color": "azul"},
        )
        .despacio((r"\frac{k_0}{r}\cdot 2\pi r L = 2\pi k_0 L", "Se simplifica r arriba y abajo sin ningún salto"))
    )
    (
        lec.paso("Integrar entre a y b para hallar H")
        .por_que("Como $r$ se canceló, la integral de $dr$ es simplemente $b - a$.")
        .di(
            "Separamos variables e integramos desde el borde interno hasta el externo.",
            {"a": "mate", "tex": r"H\int_a^b dr = -2\pi k_0 L \int_{T_{\text{int}}}^{T_{\text{ext}}} dT \implies H(b-a) = 2\pi k_0 L(T_{\text{int}} - T_{\text{ext}})"},
            {"a": "mate", "tex": rf"H = \frac{{2\pi({fmt(k0,2)})({fmt(L,1)})({fmt(Tint,1)} - {fmt(Text,1)})}}{{{fmt(b,3)} - {fmt(a,3)}}} = {fmt(H,1)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso(f"Temperatura en el punto medio (r = {fmt_txt(r_mid,3)} m)")
        .por_que("Como $dT/dr$ es constante, la temperatura baja linealmente con el radio.")
        .di(
            "Integramos solo desde a hasta el radio medio y despejamos la temperatura.",
            {"a": "mate", "tex": rf"T({fmt(r_mid,3)}\,\text{{m}}) = {fmt(Tint,1)} - \frac{{{fmt(H,1)}({fmt(r_mid-a,3)})}}{{2\pi({fmt(k0,2)})({fmt(L,1)})}} = {fmt(T_mid,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"H = {fmt(H,1)}\,\text{{W}}", valor=round(H, 1))
    lec.responde("B)", rf"T({fmt(r_mid,3)}\,\text{{m}}) = {fmt(T_mid,1)}\,^\circ\text{{C}}", valor=round(T_mid, 1))
    return lec


def _v_ciclo_pv(rng: random.Random) -> Leccion:
    """Arquetipo crítico tipo E16 / Q111: ciclo isotérmica + isobárica + isocórica (100% energía conservada)."""
    n = rng.choice([1.5, 2.0, 2.5, 3.0])
    es_diat = rng.choice([True, False])
    tipo_gas = "diatómico" if es_diat else "monoatómico"
    cv_coef = 2.5 if es_diat else 1.5
    cp_coef = cv_coef + 1.0
    cv_tex = r"\frac{5}{2}R" if es_diat else r"\frac{3}{2}R"
    cp_tex = r"\frac{7}{2}R" if es_diat else r"\frac{5}{2}R"

    TA = float(rng.choice([400, 480, 540, 600]))
    VA = rng.choice([0.020, 0.030, 0.040])
    factor_V = rng.choice([2.0, 2.5, 3.0])
    VB = VA * factor_V
    TB = TA
    pA = n * R_GAS * TA / VA
    pB = n * R_GAS * TB / VB
    pC = pB
    VC = VA
    TC = TB * (VC / VB)

    W_AB = n * R_GAS * TA * math.log(VB / VA)
    Q_AB = W_AB
    W_BC = pB * (VC - VB)
    dU_BC = n * cv_coef * R_GAS * (TC - TB)
    Q_BC = n * cp_coef * R_GAS * (TC - TB)
    dU_CA = n * cv_coef * R_GAS * (TA - TC)
    Q_CA = dU_CA

    W_neto = W_AB + W_BC
    Q_H = Q_AB + Q_CA
    efic = W_neto / Q_H

    lec = Leccion(
        f"VAR-PV-{rng.randint(100,999)}",
        f"Variante · Ciclo P–V (isotérmica · isobárica · isocórica, gas {tipo_gas})",
        tema="ciclos",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un gas ideal {tipo_gas} ($n = {fmt(n,1)}\\text{{ mol}}$) parte del estado A con $T_A = {fmt(TA,0)}\\text{{ K}}$ y "
            f"$V_A = {fmt(VA,3)}\\text{{ m}}^3$. Realiza: A→B expansión isotérmica hasta $V_B = {fmt(VB,3)}\\text{{ m}}^3$, "
            f"B→C compresión isobárica hasta $V_C = V_A$, y C→A calentamiento isocórico hasta A. "
            f"Calcule el trabajo de cada proceso, el trabajo neto $W_{{\\text{{neto}}}}$ y la eficiencia $e$."
        ),
        visual={
            "tipo": "pv",
            "estados": [
                {"n": "A", "p": round(pA), "V": VA, "T": TA},
                {"n": "B", "p": round(pB), "V": VB, "T": TB},
                {"n": "C", "p": round(pC), "V": VC, "T": TC},
            ],
            "tramos": [
                {"de": 0, "a": 1, "tipo": "isotermico", "Q": round(Q_AB), "W": round(W_AB), "dU": 0},
                {"de": 1, "a": 2, "tipo": "isobarico", "Q": round(Q_BC), "W": round(W_BC), "dU": round(dU_BC)},
                {"de": 2, "a": 0, "tipo": "isocorico", "Q": round(Q_CA), "W": 0, "dU": round(dU_CA)},
            ],
        },
    ).usa(r"pV = nRT", r"W_{\text{iso}} = nRT\ln\!\left(\frac{V_f}{V_i}\right)", r"W_{\text{isob}} = p\Delta V", r"e = \frac{W_{\text{neto}}}{Q_H}")

    (
        lec.paso("Estados A, B y C con la ley del gas ideal")
        .por_que("Primero fijamos presión, volumen y temperatura en los tres vértices en SI.")
        .di(
            "Despejamos la presión en A y en B con la temperatura en Kelvin.",
            {"a": "mate", "tex": rf"p_A = \frac{{nRT_A}}{{V_A}} = {fmt(pA/1000,1)}\,\text{{kPa}},\quad p_B = p_C = {fmt(pB/1000,1)}\,\text{{kPa}}", "color": "azul"},
        )
        .di(
            "Como B a C es a presión constante, la temperatura baja en la misma proporción que el volumen.",
            {"a": "mate", "tex": rf"T_C = T_B\frac{{V_C}}{{V_B}} = {fmt(TB,0)}\left(\frac{{{fmt(VC,3)}}}{{{fmt(VB,3)}}}\right) = {fmt(TC,1)}\,\text{{K}}"},
        )
    )
    (
        lec.paso("Proceso A → B (expansión isotérmica)")
        .por_que("La presión cambia todo el camino: NO se usa $p\\Delta V$, sino la integral $nRT\\ln(V_B/V_A)$.")
        .di(
            "A temperatura constante la energía interna no cambia, así que el calor absorbido es igual al trabajo.",
            {"a": "mate", "tex": rf"W_{{AB}} = Q_{{AB}} = ({fmt(n,1)})(8{{,}}314)({fmt(TA,0)})\ln\!\left(\frac{{{fmt(VB,3)}}}{{{fmt(VA,3)}}}\right) = {fmt(W_AB,0)}\,\text{{J}}", "color": "verde"},
        )
        .despacio((r"W_{AB} = \int_{V_A}^{V_B} \frac{nRT_A}{V}\,dV = nRT_A \ln\!\left(\frac{V_B}{V_A}\right)", "Por qué aparece el logaritmo natural"))
    )
    (
        lec.paso("Procesos B → C (isobárica) y C → A (isocórica)")
        .por_que("En B→C la presión es constante y el volumen baja (trabajo negativo); en C→A el volumen no cambia ($W = 0$).")
        .di(
            "En la compresión isobárica el trabajo es presión por cambio de volumen y sale negativo.",
            {"a": "mate", "tex": rf"W_{{BC}} = p_B(V_C - V_B) = ({fmt(pB,0)})({fmt(VC-VB,3)}) = {fmt(W_BC,0)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"Q_{{BC}} = n\left({cp_tex}\right)(T_C - T_B) = {fmt(Q_BC,0)}\,\text{{J}}"},
        )
        .di(
            "Al cerrar el ciclo a volumen constante no hay trabajo y entra calor.",
            {"a": "mate", "tex": rf"W_{{CA}} = 0\,\text{{J}},\quad Q_{{CA}} = n\left({cv_tex}\right)(T_A - T_C) = {fmt(Q_CA,0)}\,\text{{J}}"},
        )
    )
    (
        lec.paso("Trabajo neto y eficiencia térmica")
        .por_que("Solo los calores positivos forman $Q_H$; además $\\Delta U_{{\\text{{ciclo}}}} = 0$ garantiza $W_{{\\text{{neto}}}} = Q_{{\\text{{neto}}}}$.")
        .di(
            "Sumamos los trabajos y dividimos entre el calor total absorbido.",
            {"a": "mate", "tex": rf"W_{{\text{{neto}}}} = {fmt(W_AB,0)} + ({fmt(W_BC,0)}) + 0 = {fmt(W_neto,0)}\,\text{{J}}", "color": "azul"},
            {"a": "mate", "tex": rf"Q_H = Q_{{AB}} + Q_{{CA}} = {fmt(Q_AB,0)} + {fmt(Q_CA,0)} = {fmt(Q_H,0)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"e = \frac{{{fmt(W_neto,0)}}}{{{fmt(Q_H,0)}}} = {fmt(efic*100,1)}\,\%", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("W", rf"W_{{\text{{neto}}}} = {fmt(W_neto,0)}\,\text{{J}}", valor=round(W_neto))
    lec.responde("e", rf"e = {fmt(efic*100,1)}\,\%", valor=round(efic * 100, 1))
    return lec


def _v_calorimetria_fase(rng: random.Random) -> Leccion:
    """Arquetipo alto tipo E5: metal caliente en mezcla de agua y hielo con chequeo de fusión."""
    m_agua = rng.choice([0.20, 0.25, 0.30, 0.40])
    m_hielo = rng.choice([0.04, 0.05, 0.06])
    m_metal = rng.choice([0.25, 0.35, 0.45, 0.50])
    mat, c_m = rng.choice([("cobre", 387.0), ("aluminio", 900.0), ("hierro", 448.0)])
    T_m = float(rng.choice([180, 220, 260, 300]))
    c_w = 4186.0
    Lf = 3.34e5

    Q_fundir = m_hielo * Lf
    Q_metal_max = m_metal * c_m * (T_m - 0.0)
    if Q_metal_max > Q_fundir:
        Tf = (Q_metal_max - Q_fundir) / ((m_agua + m_hielo) * c_w + m_metal * c_m)
        res_tex = rf"T_f = {fmt(Tf,1)}\,^\circ\text{{C}}"
        val_final = round(Tf, 1)
        nota_f = "Todo el hielo se derrite y la mezcla queda líquida sobre cero grados."
    else:
        m_derr = Q_metal_max / Lf
        Tf = 0.0
        res_tex = rf"T_f = 0{{,}}0\,^\circ\text{{C}},\; m_{{\text{{derretida}}}} = {fmt(m_derr*1000,1)}\,\text{{g}}"
        val_final = 0.0
        nota_f = "El metal no alcanza para derretir todo el hielo; el equilibrio queda en cero grados."

    lec = Leccion(
        f"VAR-CAL-{rng.randint(100,999)}",
        f"Variante · Bloque de {mat} en agua con hielo",
        tema="calorimetria",
        tipo="desarrollo",
        nivel=3,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un calorímetro aislado contiene ${fmt(m_agua,2)}\\text{{ kg}}$ de agua y ${fmt(m_hielo,2)}\\text{{ kg}}$ de hielo en equilibrio a $0{{,}}0\\,^\\circ\\text{{C}}$. "
            f"Se introduce un bloque de {mat} ($c = {fmt(c_m,0)}\\text{{ J/(kg\\cdot K)}}$) de masa ${fmt(m_metal,2)}\\text{{ kg}}$ a ${fmt(T_m,0)}\\,^\\circ\\text{{C}}$. "
            f"Determine la temperatura final de equilibrio."
        ),
        visual={"tipo": "calor", "caso": "curva", "T0": 0, "Tf": Tf},
    ).usa(r"\sum Q = 0", r"Q = m c \Delta T", r"Q_{\text{fusión}} = m L_f")

    (
        lec.paso("Comparar el calor máximo del metal con el calor para fundir el hielo")
        .por_que("Antes de despejar $T_f$ hay que comprobar si el hielo se derrite por completo o no.")
        .di(
            "Calculamos cuánto calor pide derretir todo el hielo y cuánto puede ceder el metal al enfriarse hasta cero.",
            {"a": "mate", "tex": rf"Q_{{\text{{fundir}}}} = m_h L_f = ({fmt(m_hielo,2)})(3{{,}}34\times 10^5) = {fmt(Q_fundir,0)}\,\text{{J}}", "color": "azul"},
            {"a": "mate", "tex": rf"Q_{{\text{{metal, máx}}}} = ({fmt(m_metal,2)})({fmt(c_m,0)})({fmt(T_m,0)} - 0) = {fmt(Q_metal_max,0)}\,\text{{J}}"},
        )
    )
    (
        lec.paso("Balance de energía y temperatura final")
        .por_que(nota_f)
        .di(nota_f, {"a": "mate", "tex": res_tex, "color": "verde", "rol": "resultado"})
    )
    lec.responde("", res_tex, valor=val_final)
    return lec


def _v_conduccion_serie_paralelo(rng: random.Random) -> Leccion:
    """Arquetipo muy alto tipo Q038: dos barras en serie (temperatura de unión y flujo)."""
    L1 = rng.choice([0.20, 0.25, 0.30])
    L2 = rng.choice([0.15, 0.20, 0.30])
    A = rng.choice([0.0020, 0.0040, 0.0050])
    k1, n1 = 385.0, "cobre"
    k2, n2 = rng.choice([(50.2, "acero"), (109.0, "latón"), (235.0, "aluminio")])
    T1 = float(rng.choice([100, 120, 150]))
    T2 = float(rng.choice([0, 10, 20]))

    R1 = L1 / (k1 * A)
    R2 = L2 / (k2 * A)
    H = (T1 - T2) / (R1 + R2)
    Tu = T1 - H * R1

    lec = Leccion(
        f"VAR-SER-{rng.randint(100,999)}",
        f"Variante · Barras de {n1} y {n2} en serie",
        tema="conduccion",
        tipo="desarrollo",
        nivel=3,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una barra de {n1} ($L_1 = {fmt(L1,2)}\\text{{ m}}, k_1 = {fmt(k1,1)}\\text{{ W/(m\\cdot K)}}$) se une en serie con una de {n2} "
            f"($L_2 = {fmt(L2,2)}\\text{{ m}}, k_2 = {fmt(k2,1)}\\text{{ W/(m\\cdot K)}}$), ambas de área $A = {fmt(A,4)}\\text{{ m}}^2$. "
            f"Los extremos se mantienen a ${fmt(T1,0)}\\,^\\circ\\text{{C}}$ y ${fmt(T2,0)}\\,^\\circ\\text{{C}}$. Calcule el flujo $H$ y la temperatura de la unión."
        ),
        visual={"tipo": "conduccion", "caso": "serie", "T1": T1, "T2": T2, "L1": L1, "L2": L2, "k1": k1, "k2": k2},
    ).usa(r"H = kA\frac{T_H - T_C}{L}")

    (
        lec.paso("Partir de Fourier en serie y despejar el flujo H")
        .por_que("El formulario oficial solo trae $H = kA(T_H - T_C)/L$. En serie el mismo flujo $H$ cruza ambas barras; despejando cada caída de temperatura y sumándolas eliminamos $T_u$.")
        .di(
            "Partimos de la ley de Fourier del formulario en cada barra, despejamos ambas caídas de temperatura y las sumamos.",
            {"a": "mate", "tex": r"H = k_1 A\frac{T_1 - T_u}{L_1} = k_2 A\frac{T_u - T_2}{L_2}", "color": "azul"},
            {"a": "mate", "tex": r"T_1 - T_2 = H\left(\frac{L_1}{k_1 A} + \frac{L_2}{k_2 A}\right)"},
            {"a": "mate", "tex": rf"\frac{{L_1}}{{k_1 A}} = \frac{{{fmt(L1,2)}}}{{({fmt(k1,1)})({fmt(A,4)})}} = {fmt(R1,4)}\,\text{{K/W}},\quad \frac{{L_2}}{{k_2 A}} = {fmt(R2,4)}\,\text{{K/W}}"},
            {"a": "mate", "tex": rf"H = \frac{{{fmt(T1,0)} - {fmt(T2,0)}}}{{{fmt(R1,4)} + {fmt(R2,4)}}} \approx {fmt(H,1)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso("Despejar la temperatura de la unión")
        .por_que("Volvemos a la primera barra $H = k_1 A(T_1 - T_u)/L_1$ y despejamos $T_u$.")
        .di(
            "Despejamos la temperatura de la unión desde la ecuación base de la primera barra.",
            {"a": "mate", "tex": rf"T_u = T_1 - H\frac{{L_1}}{{k_1 A}} = {fmt(T1,0)} - ({fmt(H,1)})({fmt(R1,4)}) \approx {fmt(Tu,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"H = {fmt(H,1)}\,\text{{W}}", valor=round(H, 1))
    lec.responde("B)", rf"T_u = {fmt(Tu,1)}\,^\circ\text{{C}}", valor=round(Tu, 1))
    return lec


def _v_dilatacion_barras(rng: random.Random) -> Leccion:
    """Arquetipo tipo E1: dos barras metálicas que mantienen diferencia constante de largo."""
    dif = rng.choice([4.0, 5.0, 6.0, 8.0])
    a1, n1 = 1.7e-5, "cobre"
    a2, n2 = rng.choice([(1.1e-5, "acero"), (1.2e-5, "hierro")])
    L1 = dif * a2 / (a1 - a2)
    L2 = L1 + dif

    lec = Leccion(
        f"VAR-DIL-{rng.randint(100,999)}",
        f"Variante · Dos barras ({n1} y {n2}) con diferencia fija de {fmt_txt(dif,1)} cm",
        tema="dilatacion",
        tipo="desarrollo",
        nivel=2,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una barra de {n1} ($\\alpha_1 = 1{{,}}7\\times 10^{{-5}}\\,^\\circ\\text{{C}}^{{-1}}$) y otra de {n2} "
            f"($\\alpha_2 = {fmt(a2*1e5,1)}\\times 10^{{-5}}\\,^\\circ\\text{{C}}^{{-1}}$) mantienen siempre una diferencia de largo de ${fmt(dif,1)}\\text{{ cm}}$ a cualquier temperatura. "
            f"Calcule el largo inicial de cada barra."
        ),
        visual={"tipo": "dilatacion", "caso": "barras", "u": "cm", "T0": 20, "dTmax": 250, "barras": [{"n": n1, "L": round(L1, 2), "a": a1, "color": "naranja"}, {"n": n2, "L": round(L2, 2), "a": a2, "color": "azul"}]},
    ).usa(r"\Delta L = \alpha L_0 \Delta T")

    (
        lec.paso("Igualar las dilataciones y despejar los largos")
        .por_que("Para que $L_2 - L_1$ nunca cambie, ambas barras deben crecer exactamente lo mismo.")
        .di(
            "Igualamos ambos crecimientos de largo y cancelamos el cambio de temperatura.",
            {"a": "mate", "tex": r"\alpha_1 L_1 \Delta T = \alpha_2 L_2 \Delta T \implies \alpha_1 L_1 = \alpha_2 (L_1 + d)", "color": "azul"},
            {"a": "mate", "tex": rf"L_1 = \frac{{\alpha_2 d}}{{\alpha_1 - \alpha_2}} = {fmt(L1,2)}\,\text{{cm}},\quad L_2 = {fmt(L2,2)}\,\text{{cm}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("", rf"L_1 = {fmt(L1,2)}\,\text{{cm}},\; L_2 = {fmt(L2,2)}\,\text{{cm}}", valor=round(L1, 2))
    return lec


def _v_radiacion_stefan(rng: random.Random) -> Leccion:
    """Arquetipo alto tipo E10 / Q063: radiación neta de Stefan-Boltzmann."""
    A = rng.choice([0.08, 0.12, 0.15, 0.20])
    eps = rng.choice([0.65, 0.75, 0.85, 0.90])
    T_c = float(rng.choice([127, 227, 327, 427]))
    Tamb_c = float(rng.choice([17, 20, 27]))
    T_K = T_c + 273.15
    Tamb_K = Tamb_c + 273.15
    P_neta = eps * SIGMA * A * (T_K**4 - Tamb_K**4)

    lec = Leccion(
        f"VAR-RAD-{rng.randint(100,999)}",
        "Variante · Radiación neta por ley de Stefan–Boltzmann",
        tema="radiacion",
        tipo="desarrollo",
        nivel=2,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un objeto de área $A = {fmt(A,2)}\\text{{ m}}^2$ y emisividad $\\varepsilon = {fmt(eps,2)}$ está a ${fmt(T_c,0)}\\,^\\circ\\text{{C}}$ "
            f"dentro de un cuarto a ${fmt(Tamb_c,0)}\\,^\\circ\\text{{C}}$. Calcule la potencia neta irradiada."
        ),
        visual={"tipo": "radiacion", "T": T_K, "e": eps},
    ).usa(r"P_{\text{neta}} = \varepsilon \sigma A (T^4 - T_{\text{amb}}^4)")

    (
        lec.paso("Convertir temperaturas a Kelvin y aplicar Stefan–Boltzmann")
        .por_que("En $T^4$ es obligatorio usar Kelvin; sustituir Celsius da un resultado totalmente erróneo.")
        .di(
            "Primero sumamos 273,15 a cada temperatura para pasar a Kelvin.",
            {"a": "mate", "tex": rf"T = {fmt(T_K,2)}\,\text{{K}},\quad T_{{\text{{amb}}}} = {fmt(Tamb_K,2)}\,\text{{K}}", "color": "azul"},
        )
        .di(
            "Sustituimos en la ley de Stefan Boltzmann con la constante sigma.",
            {"a": "mate", "tex": rf"P_{{\text{{neta}}}} = ({fmt(eps,2)})(5{{,}}67\times 10^{{-8}})({fmt(A,2)})({fmt(T_K,2)}^4 - {fmt(Tamb_K,2)}^4) = {fmt(P_neta,1)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("", rf"P_{{\text{{neta}}}} = {fmt(P_neta,1)}\,\text{{W}}", valor=round(P_neta, 1))
    return lec


def _v_maquina_otto(rng: random.Random) -> Leccion:
    """Arquetipo alto tipo E17 / Q128: eficiencia de un ciclo Otto y potencia térmica."""
    r = rng.choice([7.5, 8.0, 8.5, 9.0, 9.5, 10.0])
    gamma = rng.choice([1.40, 1.35])
    QH = float(rng.choice([1200, 1500, 1800, 2400]))
    e = 1.0 - 1.0 / (r ** (gamma - 1.0))
    W = e * QH
    QC = QH - W

    lec = Leccion(
        f"VAR-OTTO-{rng.randint(100,999)}",
        f"Variante · Motor de ciclo Otto (r = {fmt_txt(r,1)})",
        tema="maquinas",
        tipo="desarrollo",
        nivel=3,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un motor de gasolina idealizado como ciclo Otto tiene razón de compresión $r = {fmt(r,1)}$ y $\\gamma = {fmt(gamma,2)}$. "
            f"Si en cada ciclo absorbe $Q_H = {fmt(QH,0)}\\text{{ J}}$, calcule la eficiencia $e$, el trabajo $W$ y el calor desechado $|Q_C|$."
        ),
        visual={"tipo": "maquina", "modo": "motor", "QH": QH, "W": round(W), "QC": round(QC)},
    ).usa(r"e_{\text{Otto}} = 1 - \frac{1}{r^{\gamma - 1}}", r"W = e Q_H", r"|Q_C| = Q_H - W")

    (
        lec.paso("Eficiencia del ciclo Otto y balance de calor")
        .por_que("La eficiencia de Otto depende solo de la razón de compresión $r$ y $\\gamma$.")
        .di(
            "Evaluamos la eficiencia del ciclo Otto.",
            {"a": "mate", "tex": rf"e = 1 - \frac{{1}}{{{fmt(r,1)}^{{{fmt(gamma-1,2)}}}}} = {fmt(e*100,1)}\,\%", "color": "azul"},
        )
        .di(
            "Por conservación de la energía, el calor que no sale como trabajo se expulsa al foco frío.",
            {"a": "mate", "tex": rf"W = ({fmt(e,3)})({fmt(QH,0)}) = {fmt(W,0)}\,\text{{J}},\quad |Q_C| = {fmt(QH,0)} - {fmt(W,0)} = {fmt(QC,0)}\,\text{{J}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("e", rf"e = {fmt(e*100,1)}\,\%", valor=round(e * 100, 1))
    lec.responde("W", rf"W = {fmt(W,0)}\,\text{{J}},\; |Q_C| = {fmt(QC,0)}\,\text{{J}}", valor=round(W))
    return lec


def _v_conduccion_cono(rng: random.Random) -> Leccion:
    """Arquetipo PR1-4 / PR2-D1 / Q041: tronco de cono con fusión de hielo y posición de T media."""
    mat, k = rng.choice([("aluminio", 205.0), ("cobre", 385.0), ("latón", 109.0), ("acero", 50.2)])
    L = rng.choice([0.16, 0.20, 0.24, 0.30])
    r1 = rng.choice([0.010, 0.012, 0.015])
    r2 = round(r1 * rng.choice([2.5, 3.0, 3.5]), 4)
    T1 = float(rng.choice([100, 120, 140]))
    T2 = 0.0
    Lf = 3.34e5
    b = (r2 - r1) / L
    H = math.pi * k * r1 * r2 * (T1 - T2) / L
    Q_h = H * 3600.0
    m_h = Q_h / Lf
    fac = H / (math.pi * k * b)
    rmid = r1 + b * (L / 2.0)
    Tmid = T1 - fac * (1.0 / r1 - 1.0 / rmid)
    Tobj = 0.5 * (T1 + T2)
    r_obj = 1.0 / (1.0 / r1 - (T1 - Tobj) / fac)
    x_obj = (r_obj - r1) / b
    pts = [[round(L * i / 40.0, 4), round(T1 - fac * (1.0 / r1 - 1.0 / (r1 + b * L * i / 40.0)), 2)] for i in range(41)]
    c0 = round(r1 / r2, 3)

    lec = Leccion(
        f"VAR-CONO-{rng.randint(100,999)}",
        f"Variante · Tronco de cono de {mat} y fusión de hielo",
        tema="conduccion",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una barra de {mat} ($k={fmt(k,1)}$ W/(m·K)) en tronco de cono mide $L={fmt(L*100,1)}$ cm; "
            f"su radio crece linealmente de $r_1={fmt(r1*100,2)}$ cm en $x=0$ ($T_1={fmt(T1,0)}\\,^\\circ\\text{{C}}$) "
            f"a $r_2={fmt(r2*100,2)}$ cm en $x=L$ (hielo a $0\\,^\\circ\\text{{C}}$). "
            f"Calcule: A) flujo $H$, B) masa de hielo fundida por hora, C) $T(L/2)$ y posición $x$ donde $T={fmt(Tobj,0)}\\,^\\circ\\text{{C}}$."
        ),
        visual={"tipo": "conduccion", "geo": "pared", "L": L, "T1": T1, "T2": T2, "eje": "x", "uT": "°C",
                "forma": f"{c0} + {round(1.0 - c0, 3)} * u", "pts": pts, "xm": L / 2.0, "etA": "π r(x)²"},
    ).usa(r"H = -kA\frac{dT}{dx}", r"H = \frac{dQ}{dt}", r"Q = \pm mL")

    (
        lec.paso("A) Separar variables e integrar desde Fourier")
        .por_que("Como el radio cambia linealmente, partimos de $H=-kA(x)\\,dT/dx$ con $A(x)=\\pi(r_1+bx)^2$.")
        .di(
            "Escribimos el radio lineal, sustituimos el área circular en la ley de Fourier e integramos toda la barra.",
            {"a": "mate", "tex": r"H = -kA\frac{dT}{dx} = -k\pi(r_1+bx)^2\frac{dT}{dx}", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": r"\frac{H}{\pi}\int_0^L \frac{dx}{(r_1+bx)^2} = -k\int_{T_1}^{T_2} dT \;\Rightarrow\; \frac{H L}{\pi r_1 r_2} = k(T_1 - T_2)"},
            {"a": "mate", "tex": rf"H = \frac{{\pi({fmt(k,1)})({fmt(r1,4)})({fmt(r2,4)})({fmt(T1-T2,1)})}}{{{fmt(L,2)}}} = {fmt(H,2)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso("B) Masa de hielo fundida por hora")
        .por_que("Usamos la definición de corriente de calor y el calor latente de fusión del formulario.")
        .di(
            "Multiplicamos el flujo por tres mil seiscientos segundos y dividimos entre el calor latente del hielo.",
            {"a": "mate", "tex": r"H = \frac{dQ}{dt} \;\Rightarrow\; Q = H\,\Delta t", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"Q = ({fmt(H,2)})(3600) = {fmt(Q_h,0)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"m = \frac{{Q}}{{L_f}} = \frac{{{fmt(Q_h,0)}}}{{3{{,}}34\times 10^5}} = {fmt(m_h,3)}\,\text{{kg/h}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso(f"C) Temperatura a la mitad y punto de {fmt_txt(Tobj,0)} °C")
        .por_que("La caída de temperatura se concentra en el extremo angosto donde la sección es menor.")
        .di(
            "Integramos solo hasta la mitad de la barra para hallar la temperatura media geométrica y luego despejamos la posición.",
            {"a": "mate", "tex": rf"T(x) = T_1 - \frac{{H}}{{\pi k b}}\left(\frac{{1}}{{r_1}} - \frac{{1}}{{r(x)}}\right),\quad \frac{{H}}{{\pi k b}} = {fmt(fac,4)}\,\text{{K}}\cdot\text{{m}}"},
            {"a": "mate", "tex": rf"T(L/2) = {fmt(T1,0)} - ({fmt(fac,5)})\left(\frac{{1}}{{{fmt(r1,5)}}} - \frac{{1}}{{{fmt(rmid,5)}}}\right) = {fmt(Tmid,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"x = \frac{{{fmt(r_obj,5)} - {fmt(r1,5)}}}{{{fmt(b,5)}}} = {fmt(x_obj*100,2)}\,\text{{cm}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"H = {fmt(H,2)}\,\text{{W}}", valor=round(H, 2))
    lec.responde("B)", rf"m = {fmt(m_h,3)}\,\text{{kg/h}}", valor=round(m_h, 3))
    lec.responde("C)", rf"T(L/2) = {fmt(Tmid,1)}\,^\circ\text{{C}},\; x = {fmt(x_obj*100,2)}\,\text{{cm}}", valor=round(Tmid, 1))
    return lec


def _v_conduccion_exponencial(rng: random.Random) -> Leccion:
    """Arquetipo PR2-D2 / PR2-D10: barra de área variable lineal A₀(1+x/L) o exponencial A₀e^(-x/L)."""
    es_exp = rng.choice([True, False])
    L = rng.choice([0.50, 0.60, 0.80, 1.00])
    k = rng.choice([120.0, 180.0, 205.0, 385.0])
    A0 = rng.choice([0.0020, 0.0025, 0.0040])
    T1 = float(rng.choice([120, 150, 180]))
    T2 = float(rng.choice([20, 30, 40]))
    g0 = L / (k * A0)

    if es_exp:
        R_th = g0 * (math.e - 1.0)
        H = (T1 - T2) / R_th
        Tmid = T1 - H * g0 * (math.exp(0.5) - 1.0)
        gr0 = H / (k * A0)
        grL = H / (k * A0 * math.exp(-1.0))
        pts = [[round(L * i / 40.0, 4), round(T1 - (T1 - T2) * (math.exp(i / 40.0) - 1.0) / (math.e - 1.0), 2)] for i in range(41)]
        forma = "exp(-u)"
        etA = "A₀ e^(−x/L)"
        tit = "Variante · Barra con área exponencial A(x) = A₀ e^(−x/L)"
        enun_a = r"A(x) = A_0 e^{-x/L}"
        int_tex = r"\int_0^L \frac{dx}{A_0 e^{-x/L}} = \frac{1}{A_0}\int_0^L e^{x/L}\,dx = \frac{L}{A_0}(e - 1)"
        r_formula = rf"R = \frac{{L(e-1)}}{{k A_0}} = ({fmt(g0,4)})({fmt(math.e-1,4)}) = {fmt(R_th,4)}\,\text{{K/W}}"
        tmid_tex = rf"T(L/2) = {fmt(T1,0)} - ({fmt(H,2)})({fmt(g0,4)})(e^{{0{{,}}5}} - 1) = {fmt(Tmid,1)}\,^\circ\text{{C}}"
        mayor = "x = L (extremo angosto)"
    else:
        R_th = g0 * math.log(2.0)
        H = (T1 - T2) / R_th
        Tmid = T1 - H * g0 * math.log(1.5)
        gr0 = H / (k * A0)
        grL = H / (k * 2.0 * A0)
        pts = [[round(L * i / 40.0, 4), round(T1 - H * g0 * math.log(1.0 + i / 40.0), 2)] for i in range(41)]
        forma = "0.5 + 0.5 * u"
        etA = "A₀(1 + x/L)"
        tit = "Variante · Barra con área lineal A(x) = A₀(1 + x/L)"
        enun_a = r"A(x) = A_0(1 + x/L)"
        int_tex = r"\int_0^L \frac{dx}{A_0(1+x/L)} = \frac{L}{A_0}\ln 2"
        r_formula = rf"R = \frac{{L\ln 2}}{{k A_0}} = ({fmt(g0,4)})({fmt(math.log(2.0),4)}) = {fmt(R_th,4)}\,\text{{K/W}}"
        tmid_tex = rf"T(L/2) = {fmt(T1,0)} - ({fmt(H,2)})({fmt(g0,4)})\ln(1{{,}}5) = {fmt(Tmid,1)}\,^\circ\text{{C}}"
        mayor = "x = 0 (extremo angosto)"

    lec = Leccion(
        f"VAR-AVAR-{rng.randint(100,999)}",
        tit,
        tema="conduccion",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una barra de longitud $L={fmt(L,2)}$ m y conductividad $k={fmt(k,0)}$ W/(m·K) tiene área transversal ${enun_a}$ "
            f"con $A_0={fmt(A0,4)}\\,\\text{{m}}^2$. Sus extremos están a ${fmt(T1,0)}\\,^\\circ\\text{{C}}$ en $x=0$ y "
            f"${fmt(T2,0)}\\,^\\circ\\text{{C}}$ en $x=L$. Calcule: A) resistencia térmica total y flujo $H$, B) $T(L/2)$ y C) gradientes extremos."
        ),
        visual={"tipo": "conduccion", "geo": "pared", "L": L, "T1": T1, "T2": T2, "eje": "x", "uT": "°C",
                "forma": forma, "pts": pts, "xm": L / 2.0, "etA": etA},
    ).usa(r"H = -kA\frac{dT}{dx}")

    (
        lec.paso("A) Integrar Fourier para resistencia térmica y flujo H")
        .por_que("Separamos variables desde $H=-kA(x)\\,dT/dx$ porque el área depende de $x$.")
        .di(
            "Partimos de la ley de Fourier de la hoja, separamos variables e integramos a lo largo de la barra.",
            {"a": "mate", "tex": r"H = -kA\frac{dT}{dx} \;\Rightarrow\; T_1 - T_2 = H\int_0^L \frac{dx}{kA(x)}", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": int_tex},
            {"a": "mate", "tex": r_formula},
            {"a": "mate", "tex": rf"H = \frac{{{fmt(T1,0)} - {fmt(T2,0)}}}{{{fmt(R_th,4)}}} = {fmt(H,2)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso("B) Temperatura en el punto medio y gradientes")
        .por_que("El gradiente $|dT/dx|=H/(kA(x))$ es siempre mayor donde la sección transversal es menor.")
        .di(
            "Integramos solo hasta la mitad y comparamos el gradiente de temperatura en ambos extremos.",
            {"a": "mate", "tex": tmid_tex, "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"\left|\frac{{dT}}{{dx}}\right|_{{0}} = {fmt(gr0,1)}\,\text{{K/m}},\quad \left|\frac{{dT}}{{dx}}\right|_{{L}} = {fmt(grL,1)}\,\text{{K/m}}"},
            {"a": "texto", "txt": f"Mayor gradiente en {mayor}.", "clase": "nota"},
        )
    )
    lec.responde("A)", rf"H = {fmt(H,2)}\,\text{{W}}", valor=round(H, 2))
    lec.responde("B)", rf"T(L/2) = {fmt(Tmid,1)}\,^\circ\text{{C}}", valor=round(Tmid, 1))
    return lec


def _v_conduccion_cilindro_capas(rng: random.Random) -> Leccion:
    """Arquetipo PR1-3: tubería cilíndrica concéntrica de 2 capas aislantes + condensación de vapor."""
    L = rng.choice([6.0, 8.0, 10.0, 12.0])
    r1 = rng.choice([0.020, 0.025, 0.030])
    r2 = round(r1 + rng.choice([0.015, 0.020]), 4)
    r3 = round(r2 + rng.choice([0.010, 0.015]), 4)
    k1 = rng.choice([0.040, 0.050, 0.060])
    k2 = rng.choice([0.16, 0.20, 0.25])
    T1 = float(rng.choice([120, 140, 150, 160]))
    T3 = float(rng.choice([20, 25, 30]))
    Lv = 2.256e6

    R1 = math.log(r2 / r1) / (2.0 * math.pi * k1 * L)
    R2 = math.log(r3 / r2) / (2.0 * math.pi * k2 * L)
    H = (T1 - T3) / (R1 + R2)
    Ti = T1 - H * R1
    rm = 0.5 * (r1 + r2)
    Tm = T1 - (H / (2.0 * math.pi * k1 * L)) * math.log(rm / r1)
    Q_h = H * 3600.0
    m_v = Q_h / Lv

    lec = Leccion(
        f"VAR-CIL2-{rng.randint(100,999)}",
        "Variante · Tubería de vapor con dos capas cilíndricas",
        tema="conduccion",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una tubería de $L={fmt(L,1)}$ m y radio $r_1={fmt(r1*100,2)}$ cm a ${fmt(T1,0)}\\,^\\circ\\text{{C}}$ "
            f"tiene dos capas concéntricas: capa 1 hasta $r_2={fmt(r2*100,2)}$ cm ($k_1={fmt(k1,3)}$ W/(m·K)) y capa 2 hasta "
            f"$r_3={fmt(r3*100,2)}$ cm ($k_2={fmt(k2,2)}$ W/(m·K)). Afuera hay ${fmt(T3,0)}\\,^\\circ\\text{{C}}$. "
            f"Calcule: A) flujo $H$ y temperatura de interfaz $T_i$, B) $T$ en $r={fmt(rm*100,2)}$ cm, C) vapor condensado por hora."
        ),
        visual={"tipo": "conduccion", "geo": "cilindro2", "r1": r1, "r2": r2, "r3": r3, "L": L,
                "T1": T1, "T2": round(Ti, 2), "T3": T3, "k1": k1, "k2": k2, "n1": "aislante", "n2": "cubierta", "r": rm, "uT": "°C"},
    ).usa(r"H = -kA\frac{dT}{dx}", r"H = \frac{dQ}{dt}", r"Q = \pm mL")

    (
        lec.paso("A) Derivar la resistencia cilíndrica desde Fourier y hallar H")
        .por_que("En geometría cilíndrica el área crece como $A(r)=2\\pi rL$; integramos $dr/r$ en cada capa.")
        .di(
            "Partimos de la ley de Fourier radial, separamos variables e integramos para obtener la caída de cada capa.",
            {"a": "mate", "tex": r"H = -k(2\pi r L)\frac{dT}{dr} \;\Rightarrow\; H\int_{r_{\text{int}}}^{r_{\text{ext}}} \frac{dr}{r} = -2\pi k L\int_{T_{\text{int}}}^{T_{\text{ext}}} dT", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"R_1 = \frac{{\ln({fmt(r2/r1,5)})}}{{2\pi({fmt(k1,3)})({fmt(L,1)})}} = {fmt(R1,5)}\,\text{{K/W}},\quad R_2 = {fmt(R2,5)}\,\text{{K/W}}"},
            {"a": "mate", "tex": rf"H = \frac{{{fmt(T1,0)} - {fmt(T3,0)}}}{{{fmt(R1,5)} + {fmt(R2,5)}}} = {fmt(H,2)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"T_i = {fmt(T1,0)} - ({fmt(H,2)})({fmt(R1,5)}) = {fmt(Ti,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso("B) Perfil logarítmico y vapor condensado por hora")
        .por_que("Como $dT/dr \\propto 1/r$, el perfil de temperatura es logarítmico, no lineal.")
        .di(
            "Evaluamos la temperatura logarítmica en la mitad de la capa uno y luego el vapor que se condensa en una hora.",
            {"a": "mate", "tex": rf"T({fmt(rm*100,2)}\,\text{{cm}}) = {fmt(T1,0)} - \frac{{{fmt(H,2)}}}{{2\pi({fmt(k1,3)})({fmt(L,1)})}}\ln\left(\frac{{{fmt(rm,4)}}}{{{fmt(r1,4)}}}\right) = {fmt(Tm,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": r"H = \frac{dQ}{dt} \;\Rightarrow\; Q = H\,\Delta t", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"m = \frac{{({fmt(H,2)})(3600)}}{{2{{,}}256\times 10^6}} = {fmt(m_v,3)}\,\text{{kg/h}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"H = {fmt(H,1)}\,\text{{W}},\; T_i = {fmt(Ti,1)}\,^\circ\text{{C}}", valor=round(H, 1))
    lec.responde("B)", rf"T = {fmt(Tm,1)}\,^\circ\text{{C}}", valor=round(Tm, 1))
    lec.responde("C)", rf"m = {fmt(m_v,3)}\,\text{{kg/h}}", valor=round(m_v, 3))
    return lec


def _v_ciclo_recta_integral(rng: random.Random) -> Leccion:
    """Arquetipo PR1-5 / PR2-D4: ciclo triangular con recta inclinada P = αV calculado por integral."""
    n = rng.choice([0.15, 0.20, 0.25, 0.40])
    gas_tipo = rng.choice(["mono", "dia"])
    g = Gas(n, gas_tipo)
    pA = float(rng.choice([100e3, 120e3, 150e3]))
    vA = rng.choice([0.0040, 0.0050, 0.0060])
    f = rng.choice([2.0, 2.5, 3.0])
    pB, vB = pA * f, vA * f
    alfa = pA / vA
    g.pone("A", p=pA, v=vA)
    g.pone("B", p=pB, v=vB)
    g.pone("C", p=pA, v=vB)
    rab = g.proceso("A", "B", "recta")
    rbc = g.proceso("B", "C", "isocorico")
    rca = g.proceso("C", "A", "isobarico")
    W_neto = rab["W"] + rbc["W"] + rca["W"]
    Q_H = rab["Q"]
    efic = W_neto / Q_H
    cv_tex = r"\frac{3}{2}R" if gas_tipo == "mono" else r"\frac{5}{2}R"
    nom_gas = "monoatómico" if gas_tipo == "mono" else "diatómico"

    lec = Leccion(
        f"VAR-RECTA-{rng.randint(100,999)}",
        f"Variante · Ciclo P = αV con integral (gas {nom_gas})",
        tema="ciclos",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un gas ideal {nom_gas} ($n={fmt(n,2)}$ mol) recorre el ciclo: $A\\to B$ en recta $P=\\alpha V$ desde "
            f"$A$ (${fmt(pA/1e3,0)}$ kPa, ${fmt(vA*1e3,2)}$ L) hasta $B$ (${fmt(pB/1e3,0)}$ kPa, ${fmt(vB*1e3,2)}$ L), "
            f"$B\\to C$ isocórico hasta ${fmt(pA/1e3,0)}$ kPa, y $C\\to A$ isobárico. "
            f"Calcule: A) $\\alpha$ y temperaturas, B) $W_{{AB}}$ por integral, C) $W_{{\\text{{neto}}}}$ y eficiencia $e$."
        ),
        visual=visual_pv(g, [("A", "B", "recta"), ("B", "C", "isocorico"), ("C", "A", "isobarico")], uv="L", up="kPa", fv=1e3, fp=1e-3),
    ).usa(r"pV = nRT", r"W = \int_{V_1}^{V_2} p\,dV", r"\Delta U = nC_V\Delta T", r"e = \frac{W}{Q_H}")

    (
        lec.paso("A) Constante α y temperaturas en los vértices")
        .por_que("Como $P=\\alpha V$ pasa por el origen, $\\alpha=P_A/V_A$ y cada temperatura sale de $pV=nRT$.")
        .di(
            "Calculamos alfa y despejamos las tres temperaturas desde la ley del gas ideal.",
            {"a": "mate", "tex": rf"\alpha = \frac{{P_A}}{{V_A}} = \frac{{{fmt(pA,0)}}}{{{fmt(vA,4)}}} = {fmt(alfa/1e7,3)}\times 10^7\,\text{{Pa/m}}^3"},
            {"a": "mate", "tex": r"pV = nRT \;\Rightarrow\; T = \frac{pV}{nR}", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"T_A = {fmt(g.t('A'),1)}\,\text{{K}},\quad T_B = {fmt(g.t('B'),1)}\,\text{{K}},\quad T_C = {fmt(g.t('C'),1)}\,\text{{K}}"},
        )
    )
    (
        lec.paso("B) Trabajo de A a B integrando P = αV")
        .por_que("Partimos de la integral $W=\\int p\\,dV$ del formulario y evaluamos la potencia.")
        .di(
            "Sustituimos presión igual a alfa por volumen en la integral del trabajo y evaluamos en los límites.",
            {"a": "mate", "tex": r"W = \int_{V_1}^{V_2} p\,dV", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": r"W_{AB} = \int_{V_A}^{V_B} \alpha V\,dV = \frac{\alpha}{2}\left(V_B^2 - V_A^2\right)"},
            {"a": "mate", "tex": rf"W_{{AB}} = \frac{{{fmt(pA,0)} + {fmt(pB,0)}}}{{2}}({fmt(vB,4)} - {fmt(vA,4)}) = {fmt(rab['W'],1)}\,\text{{J}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso("C) Balance del ciclo, trabajo neto y eficiencia")
        .por_que("Solo $A\\to B$ absorbe calor ($Q_H=Q_{AB}$); en $C\\to A$ el trabajo isobárico es negativo.")
        .di(
            "Calculamos la energía interna y el calor de A a B con la primera ley, el trabajo isobárico de C a A y la eficiencia.",
            {"a": "mate", "tex": rf"\Delta U_{{AB}} = n\left({cv_tex}\right)(T_B - T_A) = {fmt(rab['U'],1)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"Q_H = Q_{{AB}} = \Delta U_{{AB}} + W_{{AB}} = {fmt(rab['U'],1)} + {fmt(rab['W'],1)} = {fmt(Q_H,1)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"W_{{CA}} = P_A(V_A - V_B) = {fmt(rca['W'],1)}\,\text{{J}} \;\Rightarrow\; W_{{\text{{neto}}}} = {fmt(rab['W'],1)} + ({fmt(rca['W'],1)}) = {fmt(W_neto,1)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"e = \frac{{W_{{\text{{neto}}}}}}{{Q_H}} = \frac{{{fmt(W_neto,1)}}}{{{fmt(Q_H,1)}}} = {fmt(efic*100,1)}\,\%", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("B)", rf"W_{{AB}} = {fmt(rab['W'],1)}\,\text{{J}}", valor=round(rab["W"], 1))
    lec.responde("C)", rf"W_{{\text{{neto}}}} = {fmt(W_neto,1)}\,\text{{J}},\; e = {fmt(efic*100,1)}\,\%", valor=round(efic * 100, 1))
    return lec


def _v_esfuerzo_termico(rng: random.Random) -> Leccion:
    """Arquetipo P26-1 / PR2-S13: dilatación libre + esfuerzo térmico F/A = -Y α ΔT y ΔT límite."""
    mat, alpha, Y = rng.choice([
        ("acero", 1.2e-5, 2.0e11),
        ("cobre", 1.7e-5, 1.1e11),
        ("aluminio", 2.4e-5, 7.0e10),
        ("latón", 2.0e-5, 9.0e10),
    ])
    L0 = rng.choice([1.50, 2.00, 2.50, 3.00])
    A_cm2 = rng.choice([3.00, 4.00, 5.00, 6.00])
    A = A_cm2 * 1e-4
    T0 = 20.0
    dT = float(rng.choice([-40, -25, 30, 45, 60]))
    T1 = T0 + dT
    dL = alpha * L0 * dT
    esf = -Y * alpha * dT
    fuerza = esf * A
    esf_lim = rng.choice([1.50e8, 1.80e8, 2.00e8])
    dT_max = esf_lim / (Y * alpha)
    tipo_esf = "compresión" if esf < 0 else "tensión"

    lec = Leccion(
        f"VAR-ESF-{rng.randint(100,999)}",
        f"Variante · Esfuerzo térmico en barra de {mat} ({tipo_esf})",
        tema="dilatacion",
        tipo="desarrollo",
        nivel=3,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una barra de {mat} ($L_0={fmt(L0,2)}$ m, $A={fmt(A_cm2,2)}\\,\\text{{cm}}^2$, "
            f"$\\alpha={fmt(alpha*1e5,1)}\\times 10^{{-5}}\\,^\\circ\\text{{C}}^{{-1}}$, $Y={fmt(Y/1e10,1)}\\times 10^{{10}}$ Pa) "
            f"está sin esfuerzo a $20{{,}}0\\,^\\circ\\text{{C}}$ y cambia a ${fmt(T1,1)}\\,^\\circ\\text{{C}}$. "
            f"Calcule: A) $\\Delta L$ si fuera libre, B) esfuerzo $F/A$ y fuerza $F$ si está rígidamente sujeta, C) $\\Delta T_{{\\text{{máx}}}}$ para $|F/A|={fmt(esf_lim/1e6,0)}$ MPa."
        ),
        visual={"tipo": "dilatacion", "caso": "barras", "u": "m", "T0": T0, "dTmax": max(abs(dT), dT_max),
                "barras": [{"n": mat, "L": L0, "a": alpha, "color": "azul"}]},
    ).usa(r"\Delta L = \alpha L_0 \Delta T", r"\frac{F}{A} = -Y\alpha\Delta T")

    (
        lec.paso("A) Dilatación libre")
        .por_que("Primero calculamos cuánto querría cambiar de largo la barra sin muros.")
        .di(
            "Aplicamos la casilla de dilatación lineal de la hoja.",
            {"a": "mate", "tex": r"\Delta L = \alpha L_0\,\Delta T", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"\Delta L = ({fmt(alpha*1e5,1)}\times 10^{{-5}})({fmt(L0,2)})({fmt(dT,1)}) = {fmt(dL*1e3,2)}\,\text{{mm}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso(f"B) Esfuerzo térmico ({tipo_esf}) y fuerza")
        .por_que("Cuando se impide el cambio de longitud aparece el esfuerzo térmico de la hoja II-2026.")
        .di(
            f"Sustituimos en la casilla de esfuerzo térmico; el signo indica {tipo_esf}.",
            {"a": "mate", "tex": r"\frac{F}{A} = -Y\alpha\,\Delta T", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"\frac{{F}}{{A}} = -({fmt(Y/1e10,1)}\times 10^{{10}})({fmt(alpha*1e5,1)}\times 10^{{-5}})({fmt(dT,1)}) = {fmt(esf/1e6,1)}\,\text{{MPa}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"F = \left(\frac{{F}}{{A}}\right)A = ({fmt(esf/1e6,1)}\times 10^6)({fmt(A_cm2,2)}\times 10^{{-4}}) = {fmt(fuerza/1e3,1)}\,\text{{kN}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso("C) Cambio máximo de temperatura admisible")
        .por_que("Despejamos $\\Delta T_{\\text{máx}}$ de la magnitud del esfuerzo térmico.")
        .di(
            "Dividimos el esfuerzo límite entre el módulo de Young por alfa.",
            {"a": "mate", "tex": rf"\Delta T_{{\text{{máx}}}} = \frac{{{fmt(esf_lim/1e6,0)}\times 10^6}}{{({fmt(Y/1e10,1)}\times 10^{{10}})({fmt(alpha*1e5,1)}\times 10^{{-5}})}} = {fmt(dT_max,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"\Delta L = {fmt(dL*1e3,2)}\,\text{{mm}}", valor=round(dL * 1e3, 2))
    lec.responde("B)", rf"F/A = {fmt(esf/1e6,1)}\,\text{{MPa}}\;({tipo_esf})", valor=round(esf / 1e6, 1))
    lec.responde("C)", rf"\Delta T_{{\text{{máx}}}} = {fmt(dT_max,1)}\,^\circ\text{{C}}", valor=round(dT_max, 1))
    return lec


def _v_otto_completo_inverso(rng: random.Random) -> Leccion:
    """Arquetipo PR2-D9: ciclo Otto completo con cm³ -> L y despeje inverso de razón de compresión r."""
    n = rng.choice([0.020, 0.025, 0.030, 0.040])
    gam = 1.40
    cv = 2.5 * R_GAS
    T1 = float(rng.choice([290, 300, 310]))
    V1_cm3 = float(rng.choice([450, 500, 600, 720]))
    V1 = V1_cm3 * 1e-6
    r = rng.choice([8.0, 9.0, 10.0, 11.0])
    QH = float(rng.choice([500, 650, 800, 950]))

    Vmin_cm3 = V1_cm3 / r
    fac = r ** (gam - 1.0)
    T2 = T1 * fac
    T3 = T2 + QH / (n * cv)
    T4 = T3 / fac
    QC = n * cv * (T1 - T4)
    W = QH + QC
    e = 1.0 - 1.0 / fac
    e_obj = min(0.68, round(e + 0.05, 2))
    r_obj = (1.0 / (1.0 - e_obj)) ** (1.0 / (gam - 1.0))

    g = Gas(n, cv=cv)
    g.pone("1", v=V1, t=T1)
    g.pone("2", v=V1 / r, t=T2)
    g.pone("3", v=V1 / r, t=T3)
    g.pone("4", v=V1, t=T4)

    lec = Leccion(
        f"VAR-OTTO2-{rng.randint(100,999)}",
        f"Variante · Ciclo Otto completo y despeje inverso de r (r = {fmt_txt(r,1)})",
        tema="maquinas",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un motor de ciclo Otto con $n={fmt(n,3)}$ mol de gas diatómico ($\\gamma=1{{,}}40$) parte de $T_1={fmt(T1,0)}$ K "
            f"y $V_1={fmt(V1_cm3,0)}\\,\\text{{cm}}^3$, con razón de compresión $r={fmt(r,1)}$ y $Q_H={fmt(QH,0)}$ J. "
            f"Calcule: A) $V_{{\\text{{mín}}}}$, $T_2$, $T_3$, $T_4$, B) $W_{{\\text{{neto}}}}$ y $e_{{\\text{{otto}}}}$, "
            f"C) ¿qué razón de compresión $r'$ logra $e={fmt(e_obj*100,0)}\\,\\%$?"
        ),
        visual=visual_pv(g, [("1", "2", "adiabatico"), ("2", "3", "isocorico"), ("3", "4", "adiabatico"), ("4", "1", "isocorico")],
                         uv="cm³", up="kPa", fv=1e6, fp=1e-3),
    ).usa(r"r = \frac{V_{\text{máx}}}{V_{\text{mín}}}", r"TV^{\gamma-1} = \text{cte}", r"C_{V\text{dia}} = \frac{5}{2}R", r"e_{\text{otto}} = 1 - \frac{1}{r^{\gamma-1}}")

    (
        lec.paso("A) Volumen mínimo y temperaturas del ciclo")
        .por_que("Las compresiones y expansiones son adiabáticas ($TV^{\\gamma-1}=\\text{cte}$) y el calor entra a volumen constante.")
        .di(
            "Usamos la razón de compresión y la equivalencia de litros a centímetros cúbicos de la hoja.",
            {"a": "mate", "tex": rf"1\,\text{{L}} = 1000\,\text{{cm}}^3 \;\Rightarrow\; V_1 = {fmt(V1_cm3/1000,3)}\,\text{{L}},\quad V_{{\text{{mín}}}} = \frac{{{fmt(V1_cm3,0)}}}{{{fmt(r,1)}}} = {fmt(Vmin_cm3,1)}\,\text{{cm}}^3"},
            {"a": "mate", "tex": r"TV^{\gamma - 1} = \text{cte} \;\Rightarrow\; T_2 = T_1 r^{\gamma - 1}", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"T_2 = ({fmt(T1,0)})({fmt(r,1)})^{{0{{,}}40}} = {fmt(T2,1)}\,\text{{K}}"},
            {"a": "mate", "tex": rf"C_{{V\text{{dia}}}} = \frac{{5}}{{2}}R = {fmt(cv,3)}\,\frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}\;\Rightarrow\; T_3 = {fmt(T2,1)} + \frac{{{fmt(QH,0)}}}{{({fmt(n,3)})({fmt(cv,3)})}} = {fmt(T3,1)}\,\text{{K}}"},
            {"a": "mate", "tex": rf"T_4 = \frac{{T_3}}{{r^{{0{{,}}40}}}} = {fmt(T4,1)}\,\text{{K}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso("B) Trabajo neto y eficiencia Otto")
        .por_que("En un ciclo cerrado $W=Q_H+Q_C$ y la eficiencia coincide con $1-1/r^{\\gamma-1}$.")
        .di(
            "Calculamos el calor rechazado en el enfriamiento isocórico, el trabajo neto y comprobamos la eficiencia de Otto.",
            {"a": "mate", "tex": rf"Q_C = n C_V (T_1 - T_4) = {fmt(QC,1)}\,\text{{J}} \;\Rightarrow\; W = {fmt(QH,0)} + ({fmt(QC,1)}) = {fmt(W,1)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"e_{{\text{{otto}}}} = 1 - \frac{{1}}{{{fmt(r,1)}^{{0{{,}}40}}}} = {fmt(e*100,1)}\,\%", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso(f"C) Despeje inverso de r' para e = {fmt_txt(e_obj*100,0)} %")
        .por_que("Esta es la trampa típica de examen: hay que elevar a $1/(\\gamma-1)$, no dividir entre $\\gamma-1$.")
        .di(
            "Despejamos la potencia y elevamos ambos lados al inverso de gama menos uno.",
            {"a": "mate", "tex": r"1 - e = \frac{1}{(r')^{\gamma - 1}} \;\Rightarrow\; r' = \left(\frac{1}{1 - e}\right)^{1/(\gamma - 1)}"},
            {"a": "mate", "tex": rf"r' = \left(\frac{{1}}{{1 - {fmt(e_obj,2)}}}\right)^{{2{{,}}5}} = {fmt(r_obj,2)}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"T_3 = {fmt(T3,1)}\,\text{{K}},\; T_4 = {fmt(T4,1)}\,\text{{K}}", valor=round(T3, 1))
    lec.responde("B)", rf"W = {fmt(W,1)}\,\text{{J}},\; e = {fmt(e*100,1)}\,\%", valor=round(e * 100, 1))
    lec.responde("C)", rf"r' = {fmt(r_obj,2)}", valor=round(r_obj, 2))
    return lec


def _v_calorimetria_fusion_trampa(rng: random.Random) -> Leccion:
    """Arquetipo PR1-2 / PR2-D7 / PR2-D12: bloque metálico en agua+hielo con chequeo automático de fusión parcial o total."""
    m_h = rng.choice([0.150, 0.200, 0.250])
    m_w = rng.choice([0.300, 0.400, 0.500])
    C_cal = rng.choice([100.0, 120.0, 150.0])
    mat, c_m = rng.choice([("hierro", 450.0), ("cobre", 387.0), ("aluminio", 900.0)])
    m_m = rng.choice([0.400, 0.600, 0.800])
    T_m = float(rng.choice([140, 180, 260, 340]))
    c_w = 4186.0
    Lf = 3.34e5

    Q_fund = m_h * Lf
    Q_max = m_m * c_m * (T_m - 0.0)
    parcial = Q_max < Q_fund

    if parcial:
        m_derr = Q_max / Lf
        m_h_fin = m_h - m_derr
        m_w_fin = m_w + m_derr
        Tf = 0.0
        comp_tex = rf"{fmt(Q_max,0)}\,\text{{J}} < {fmt(Q_fund,0)}\,\text{{J}} \;\Rightarrow\; \text{{fusión parcial}}"
        paso2_nota = "Como no alcanza para fundir todo el hielo, coexisten hielo y agua en equilibrio a cero grados."
        paso2_eq1 = rf"m_{{\text{{fundida}}}} = \frac{{{fmt(Q_max,0)}}}{{3{{,}}34\times 10^5}} = {fmt(m_derr,3)}\,\text{{kg}}"
        res_tex = rf"T_f = 0{{,}}0\,^\circ\text{{C}},\quad m_{{\text{{hielo}}}} = {fmt(m_h_fin,3)}\,\text{{kg}},\quad m_{{\text{{agua}}}} = {fmt(m_w_fin,3)}\,\text{{kg}}"
        val_r = 0.0
    else:
        sobra = Q_max - Q_fund
        cap_tot = (m_w + m_h) * c_w + C_cal + m_m * c_m
        Tf = sobra / cap_tot
        m_h_fin = 0.0
        m_w_fin = m_w + m_h
        comp_tex = rf"{fmt(Q_max,0)}\,\text{{J}} > {fmt(Q_fund,0)}\,\text{{J}} \;\Rightarrow\; \text{{se funde todo el hielo}}"
        paso2_nota = "Sobra energía después de fundir todo el hielo; esa energía calienta toda el agua, el calorímetro y el metal."
        paso2_eq1 = rf"T_f = \frac{{{fmt(Q_max,0)} - {fmt(Q_fund,0)}}}{{{fmt(cap_tot,1)}}} = {fmt(Tf,1)}\,^\circ\text{{C}}"
        res_tex = rf"T_f = {fmt(Tf,1)}\,^\circ\text{{C}},\quad m_{{\text{{hielo}}}} = 0\,\text{{kg}},\quad m_{{\text{{agua}}}} = {fmt(m_w_fin,3)}\,\text{{kg}}"
        val_r = round(Tf, 1)

    lec = Leccion(
        f"VAR-FUS-{rng.randint(100,999)}",
        f"Variante · Trampa de fusión ({mat} en calorímetro con hielo y agua)",
        tema="calorimetria",
        tipo="desarrollo",
        nivel=3,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un calorímetro ($C_{{\\text{{cal}}}}={fmt(C_cal,0)}$ J/K) contiene ${fmt(m_w,3)}$ kg de agua y ${fmt(m_h,3)}$ kg de hielo "
            f"en equilibrio a $0\\,^\\circ\\text{{C}}$. Se introduce un bloque de {mat} ($c={fmt(c_m,0)}$ J/(kg·K)) de ${fmt(m_m,3)}$ kg "
            f"a ${fmt(T_m,0)}\\,^\\circ\\text{{C}}$. Determine si se funde todo el hielo, $T_f$ y las masas finales de agua y hielo."
        ),
        visual={"tipo": "calor", "Tf": round(Tf, 1),
                "componentes": [{"n": mat, "m": round(m_m * 1000), "T": T_m, "fase": "caliente"},
                                {"n": "hielo", "m": round(m_h * 1000), "T": 0, "fase": "hielo"},
                                {"n": "agua", "m": round(m_w * 1000), "T": 0, "fase": "agua"}],
                "necesita": [{"n": "fundir todo el hielo", "Q": Q_fund}],
                "disponible": [{"n": f"{mat} hasta 0 °C", "Q": Q_max}]},
    ).usa(r"Q = mc\Delta T", r"Q = \pm mL")

    (
        lec.paso("A) Comprobar si alcanza para fundir todo el hielo")
        .por_que("Nunca plantees una ecuación para $T_f$ antes de comprobar si el hielo se funde por completo.")
        .di(
            "Comparamos el calor latente necesario para fundir todo el hielo contra lo máximo que cede el metal al bajar a cero.",
            {"a": "mate", "tex": r"Q = \pm mL", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"Q_{{\text{{fundir}}}} = ({fmt(m_h,3)})(3{{,}}34\times 10^5) = {fmt(Q_fund,0)}\,\text{{J}}"},
            {"a": "mate", "tex": r"Q = mc\,\Delta T", "color": "azul", "rol": "formula"},
            {"a": "mate", "tex": rf"Q_{{\text{{máx}}}} = ({fmt(m_m,3)})({fmt(c_m,0)})({fmt(T_m,0)} - 0) = {fmt(Q_max,0)}\,\text{{J}}"},
            {"a": "mate", "tex": comp_tex, "color": "azul"},
        )
    )
    (
        lec.paso("B) Temperatura final y masas de cada fase")
        .por_que(paso2_nota)
        .di(
            paso2_nota,
            {"a": "mate", "tex": paso2_eq1},
            {"a": "mate", "tex": res_tex, "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("Tf", res_tex, valor=val_r)
    return lec


def _v_resorte_fase(rng: random.Random) -> Leccion:
    """Parcial 1: Masa-resorte con fase φ, período T y energía total."""
    m = rng.choice([0.50, 1.00, 2.00, 4.00])
    k = rng.choice([200.0, 400.0, 800.0, 1600.0])
    A = rng.choice([0.08, 0.10, 0.12, 0.16])
    sentido = rng.choice([-1, 1])
    x0 = A / 2.0
    w = math.sqrt(k / m)
    T = 2.0 * math.pi / w
    phi = -math.pi / 3.0 if sentido > 0 else math.pi / 3.0
    E = 0.5 * k * A * A
    v0 = -w * A * math.sin(phi)
    signo_txt = "positiva" if sentido > 0 else "negativa"

    lec = Leccion(
        f"VAR-MAS-{rng.randint(100,999)}",
        "Variante · Masa-resorte con fase φ y energía",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un bloque de masa $m = {fmt(m,2)}\\,\\text{{kg}}$ unido a un resorte de constante $k = {fmt(k,1)}\\,\\text{{N/m}}$ "
            f"oscila en MAS con amplitud $A = {fmt(A,2)}\\,\\text{{m}}$. En $t = 0$ pasa por $x_0 = {fmt(x0,3)}\\,\\text{{m}}$ "
            f"con velocidad {signo_txt}. Calcule: A) $\\omega$ y período $T$, B) la fase $\\phi$ en radianes y C) la energía total $E$."
        ),
    ).usa(r"\omega^2 = \frac{k}{m}", r"T = \frac{2\pi}{\omega}", r"x = A\cos(\omega t + \phi)", r"E = \frac{1}{2}kA^2")

    lec.paso("A) Frecuencia angular ω y período T")\
        .por_que("En un resorte ideal, ω = √(k/m) y T = 2π/ω salen directo de m y k.")\
        .di("Calculamos omega y el período con las casillas del formulario del Parcial 1.",
            {"a": "mate", "tex": rf"\omega = \sqrt{{\frac{{{fmt(k,1)}}}{{{fmt(m,2)}}}}} = {fmt(w,2)}\,\text{{rad/s}}", "color": "azul"},
            {"a": "mate", "tex": rf"T = \frac{{2\pi}}{{{fmt(w,2)}}} = {fmt(T,3)}\,\text{{s}}", "color": "verde", "rol": "resultado"})
    lec.paso("B) Elegir la fase φ correcta con el signo de v(0)")\
        .por_que("cos(φ) = x₀/A = 1/2 da ±π/3; el signo de v(0) = −ωA sen(φ) decide cuál cuadrante es el verdadero.")\
        .di("De la posición inicial despejamos el coseno de fi y usamos el signo de la velocidad inicial.",
            {"a": "mate", "tex": r"\cos\phi = \frac{x_0}{A} = \frac{1}{2} \implies \phi = \pm\frac{\pi}{3}", "color": "azul"},
            {"a": "mate", "tex": rf"v(0) = {fmt(v0,2)}\,\text{{m/s}} \implies \phi = {fmt(phi,3)}\,\text{{rad}}", "color": "verde", "rol": "resultado"})
    lec.paso("C) Energía mecánica total E")\
        .por_que("La energía total de un oscilador armónico es ½ k A².")\
        .di("Sustituimos k y la amplitud A en la fórmula de energía.",
            {"a": "mate", "tex": rf"E = \frac{{1}}{{2}}({fmt(k,1)})({fmt(A,2)})^2 = {fmt(E,3)}\,\text{{J}}", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"T = {fmt(T,3)}\,\text{{s}}", valor=round(T, 3), unidad="s")
    lec.responde("B)", rf"\phi = {fmt(phi,3)}\,\text{{rad}}", valor=round(phi, 3), unidad="rad")
    lec.responde("C)", rf"E = {fmt(E,3)}\,\text{{J}}", valor=round(E, 3), unidad="J")
    lec.te_toca("Si la amplitud $A$ se duplicara, ¿cuánto valdría la energía total $E$ en joules?", round(4 * E, 3), "J",
                pista="E es proporcional a A²: duplicar A multiplica E por 4.", solucion=f"E' = 4({fmt_txt(E,3)}) = {fmt_txt(4*E,3)} J.")
    return lec


def _v_pendulo_simple_fisico(rng: random.Random) -> Leccion:
    """Parcial 1: Péndulo simple y físico (varilla uniforme colgada de un extremo)."""
    L = rng.choice([0.80, 1.20, 1.50, 2.00, 2.40])
    g = 9.80
    T_simp = 2.0 * math.pi * math.sqrt(L / g)
    T_fis = 2.0 * math.pi * math.sqrt((2.0 * L) / (3.0 * g))
    lec = Leccion(
        f"VAR-PEND-{rng.randint(100,999)}",
        "Variante · Péndulo simple contra varilla rígida",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Compare el período de: A) un péndulo simple de longitud $L = {fmt(L,2)}\\,\\text{{m}}$ y "
            f"B) una varilla uniforme del mismo largo $L = {fmt(L,2)}\\,\\text{{m}}$ que oscila desde un extremo "
            f"($I = \\tfrac{{1}}{{3}}mL^2$, $d = L/2$)."
        ),
    ).usa(r"\omega^2 = \frac{g}{L}", r"\omega^2 = \frac{mgd}{I}", r"T = \frac{2\pi}{\omega}")
    lec.paso("A) Período del péndulo simple")\
        .por_que("En el péndulo simple ω = √(g/L) y T = 2π√(L/g).")\
        .di("Sustituimos el largo y la gravedad.",
            {"a": "mate", "tex": rf"T_{{\text{{simple}}}} = 2\pi\sqrt{{\frac{{{fmt(L,2)}}}{{9{{,}}80}}}} = {fmt(T_simp,3)}\,\text{{s}}", "color": "verde", "rol": "resultado"})
    lec.paso("B) Período de la varilla (péndulo físico)")\
        .por_que("Con I = (1/3)mL² y d = L/2, la masa se cancela y queda I/(mgd) = 2L/(3g).")\
        .di("Usamos la casilla de péndulo físico y simplificamos la masa.",
            {"a": "mate", "tex": rf"T_{{\text{{varilla}}}} = 2\pi\sqrt{{\frac{{2({fmt(L,2)})}}{{3(9{{,}}80)}}}} = {fmt(T_fis,3)}\,\text{{s}}", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"T_{{\text{{simple}}}} = {fmt(T_simp,3)}\,\text{{s}}", valor=round(T_simp, 3), unidad="s")
    lec.responde("B)", rf"T_{{\text{{varilla}}}} = {fmt(T_fis,3)}\,\text{{s}}", valor=round(T_fis, 3), unidad="s")
    return lec


def _v_cuerda_armonicos(rng: random.Random) -> Leccion:
    """Parcial 1: Cuerda tensada con densidad lineal μ, rapidez v y armónico n."""
    L = rng.choice([0.60, 0.80, 1.00, 1.20])
    m_g = rng.choice([4.0, 6.0, 8.0, 10.0])
    m = m_g / 1000.0
    F_ten = rng.choice([64.0, 100.0, 144.0, 196.0])
    n_arm = rng.choice([2, 3, 4])
    mu = m / L
    v = math.sqrt(F_ten / mu)
    f1 = v / (2.0 * L)
    fn = n_arm * f1
    lec = Leccion(
        f"VAR-CUERDA-{rng.randint(100,999)}",
        f"Variante · Cuerda tensada y {n_arm}.º armónico",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una cuerda de largo $L = {fmt(L,2)}\\,\\text{{m}}$ y masa $m = {fmt(m_g,1)}\\,\\text{{g}}$ está sometida a una tensión "
            f"$F = {fmt(F_ten,1)}\\,\\text{{N}}$. Calcule: A) la rapidez de onda $v$, B) la frecuencia fundamental $f_1$ y "
            f"C) la frecuencia del armónico $n = {n_arm}$."
        ),
    ).usa(r"\mu = \frac{m}{L}", r"v = \sqrt{\frac{F}{\mu}}", r"\lambda_n = \frac{2L}{n}", r"f_n = n f_1")
    lec.paso("A) Densidad lineal μ y rapidez v")\
        .por_que("Primero se pasa la masa a kilogramos para hallar μ = m/L y luego v = √(F/μ).")\
        .di("Pasamos la masa a kilogramos y hallamos mu y la rapidez.",
            {"a": "mate", "tex": rf"\mu = \frac{{{fmt(m,4)}}}{{{fmt(L,2)}}} = {fmt(mu,5)}\,\text{{kg/m}}", "color": "azul"},
            {"a": "mate", "tex": rf"v = \sqrt{{\frac{{{fmt(F_ten,1)}}}{{{fmt(mu,5)}}}}} = {fmt(v,2)}\,\text{{m/s}}", "color": "verde", "rol": "resultado"})
    lec.paso(f"B) Fundamental f₁ y armónico n = {n_arm}")\
        .por_que("Para cuerda fija en ambos extremos, λ₁ = 2L, f₁ = v/(2L) y f_n = n·f₁.")\
        .di("Calculamos la frecuencia fundamental y la multiplicamos por ene.",
            {"a": "mate", "tex": rf"f_1 = \frac{{{fmt(v,2)}}}{{2({fmt(L,2)})}} = {fmt(f1,2)}\,\text{{Hz}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"f_{n_arm} = {n_arm}({fmt(f1,2)}) = {fmt(fn,2)}\,\text{{Hz}}", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"v = {fmt(v,2)}\,\text{{m/s}}", valor=round(v, 2), unidad="m/s")
    lec.responde("B)", rf"f_1 = {fmt(f1,2)}\,\text{{Hz}}", valor=round(f1, 2), unidad="Hz")
    lec.responde("C)", rf"f_{n_arm} = {fmt(fn,2)}\,\text{{Hz}}", valor=round(fn, 2), unidad="Hz")
    return lec


def _v_onda_estacionaria(rng: random.Random) -> Leccion:
    """Parcial 1: Onda estacionaria y(x,t) = A_OE sen(kx) sen(ωt)."""
    A_oe = rng.choice([0.040, 0.050, 0.060, 0.080])
    k_pi = rng.choice([4, 6, 8, 10])
    w_pi = rng.choice([120, 160, 200, 240])
    lam = 2.0 / k_pi
    v = w_pi / k_pi
    v_trans = (w_pi * math.pi) * A_oe
    lec = Leccion(
        f"VAR-OE-{rng.randint(100,999)}",
        "Variante · Onda estacionaria: λ, v y rapidez transversal máxima",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una onda estacionaria en una cuerda se describe por $y(x,t) = {fmt(A_oe,3)}\\sen({k_pi}\\pi x)\\sen({w_pi}\\pi t)$ "
            f"(en metros y segundos). Calcule: A) longitud de onda $\\lambda$, B) rapidez de propagación $v$ y C) rapidez transversal máxima en un antinodo."
        ),
    ).usa(r"y(x,t) = A_{\text{OE}}\sen(kx)\sen(\omega t)", r"k = \frac{2\pi}{\lambda}", r"v = \lambda f = \frac{\omega}{k}")
    lec.paso("A) Leer k y ω y calcular λ y v")\
        .por_que("k multiplica a x y ω multiplica a t; de ahí salen λ = 2π/k y v = ω/k.")\
        .di("Leemos k y omega directamente de la ecuación.",
            {"a": "mate", "tex": rf"\lambda = \frac{{2\pi}}{{{k_pi}\pi}} = {fmt(lam,3)}\,\text{{m}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"v = \frac{{{w_pi}\pi}}{{{k_pi}\pi}} = {fmt(v,2)}\,\text{{m/s}}", "color": "verde", "rol": "resultado"})
    lec.paso("C) Rapidez transversal máxima")\
        .por_que("En un antinodo sen(kx) = ±1, así que la rapidez máxima transversal es ω·A_OE.")\
        .di("Multiplicamos omega por la amplitud de la estacionaria.",
            {"a": "mate", "tex": rf"v_{{y,\text{{máx}}}} = ({w_pi}\pi)({fmt(A_oe,3)}) = {fmt(v_trans,2)}\,\text{{m/s}}", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"\lambda = {fmt(lam,3)}\,\text{{m}}", valor=round(lam, 3), unidad="m")
    lec.responde("B)", rf"v = {fmt(v,2)}\,\text{{m/s}}", valor=round(v, 2), unidad="m/s")
    lec.responde("C)", rf"v_{{y,\text{{máx}}}} = {fmt(v_trans,2)}\,\text{{m/s}}", valor=round(v_trans, 2), unidad="m/s")
    return lec


def _v_sonido_intensidad(rng: random.Random) -> Leccion:
    """Parcial 1: Intensidad de onda sonora esférica I = P/(4πr²) (casilla nueva II-2026)."""
    r1 = rng.choice([5.0, 6.0, 8.0, 10.0])
    I1 = rng.choice([0.09, 0.16, 0.25, 0.36])
    I2 = 1.0
    r2 = r1 * math.sqrt(I1 / I2)
    P = I1 * 4.0 * math.pi * r1 * r1
    lec = Leccion(
        f"VAR-SON-{rng.randint(100,999)}",
        "Variante · Fuente sonora puntual e intensidad",
        tema="ondas", tipo="desarrollo", nivel=2, parcial=1,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una fuente puntual emite sonido uniforme. A $r_1 = {fmt(r1,1)}\\,\\text{{m}}$ la intensidad es "
            f"$I_1 = {fmt(I1,2)}\\,\\text{{W/m}}^2$. Calcule: A) la potencia sonora $P$ y B) la distancia $r_2$ donde $I_2 = 1{{,}}00\\,\\text{{W/m}}^2$."
        ),
    ).usa(r"I = \frac{P}{4\pi r^2}", r"\frac{I_1}{I_2} = \frac{r_2^2}{r_1^2}")
    lec.paso("A) Potencia sonora P y B) distancia r₂")\
        .por_que("La misma potencia P se reparte en esferas 4πr², así que I₁/I₂ = r₂²/r₁².")\
        .di("Despejamos P de la intensidad y luego r dos de la razón de intensidades.",
            {"a": "mate", "tex": rf"P = 4\pi r_1^2 I_1 = 4\pi({fmt(r1,1)})^2({fmt(I1,2)}) = {fmt(P,2)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"r_2 = r_1\sqrt{{\frac{{I_1}}{{I_2}}}} = {fmt(r1,1)}\sqrt{{{fmt(I1,2)}}} = {fmt(r2,2)}\,\text{{m}}", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"P = {fmt(P,2)}\,\text{{W}}", valor=round(P, 2), unidad="W")
    lec.responde("B)", rf"r_2 = {fmt(r2,2)}\,\text{{m}}", valor=round(r2, 2), unidad="m")
    return lec


def _v_flotacion_arquimedes(rng: random.Random) -> Leccion:
    """Parcial 2: Bloque que flota en agua y aceite (Arquímedes)."""
    rho_obj = rng.choice([550.0, 600.0, 650.0, 720.0])
    rho_ac = rng.choice([800.0, 850.0, 900.0])
    V = rng.choice([0.020, 0.040, 0.050])
    g = 9.80
    FB = rho_obj * V * g
    frac_agua = (rho_obj / 1000.0) * 100.0
    frac_ac = (rho_obj / rho_ac) * 100.0
    lec = Leccion(
        f"VAR-FLOT-{rng.randint(100,999)}",
        "Variante · Flotación en agua y en aceite",
        tema="fluidos", tipo="desarrollo", nivel=2, parcial=2,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un bloque de volumen $V = {fmt(V,3)}\\,\\text{{m}}^3$ y densidad $\\rho = {fmt(rho_obj,0)}\\,\\text{{kg/m}}^3$ "
            f"flota primero en agua ($1000\\,\\text{{kg/m}}^3$) y luego en aceite ($\\rho_{{\\text{{ac}}}} = {fmt(rho_ac,0)}\\,\\text{{kg/m}}^3$). "
            f"Calcule: A) el empuje $F_B$, B) el porcentaje sumergido en agua y C) el porcentaje sumergido en aceite."
        ),
    ).usa(r"F_B = \rho g V", r"\rho = \frac{m}{V}")
    lec.paso("A) Empuje igual al peso y porcentajes sumergidos")\
        .por_que("Cuando el cuerpo flota en equilibrio, F_B = mg y la fracción sumergida es ρ_obj / ρ_líq.")\
        .di("Como flota, el empuje es igual al peso del bloque y la fracción sumergida es el cociente de densidades.",
            {"a": "mate", "tex": rf"F_B = \rho_{{\text{{obj}}}} V g = ({fmt(rho_obj,0)})({fmt(V,3)})(9{{,}}80) = {fmt(FB,2)}\,\text{{N}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"\%_{{\text{{agua}}}} = \frac{{{fmt(rho_obj,0)}}}{{1000}} = {fmt(frac_agua,1)}\,\%\qquad \%_{{\text{{aceite}}}} = \frac{{{fmt(rho_obj,0)}}}{{{fmt(rho_ac,0)}}} = {fmt(frac_ac,1)}\,\%", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"F_B = {fmt(FB,2)}\,\text{{N}}", valor=round(FB, 2), unidad="N")
    lec.responde("B)", rf"{fmt(frac_agua,1)}\,\%", valor=round(frac_agua, 1), unidad="%")
    lec.responde("C)", rf"{fmt(frac_ac,1)}\,\%", valor=round(frac_ac, 1), unidad="%")
    return lec


def _v_venturi_bernoulli(rng: random.Random) -> Leccion:
    """Parcial 2: Tubo Venturi horizontal con caudal, continuidad y Bernoulli."""
    r1 = rng.choice([0.030, 0.040, 0.050])
    r2 = r1 / 2.0
    v1 = rng.choice([1.0, 1.5, 2.0])
    rho = 1000.0
    g = 9.80
    v2 = v1 * (r1 / r2) ** 2
    Q = math.pi * r1 * r1 * v1
    dp = 0.5 * rho * (v2 * v2 - v1 * v1)
    dh = dp / (rho * g)
    lec = Leccion(
        f"VAR-VENT-{rng.randint(100,999)}",
        "Variante · Tubo Venturi horizontal (continuidad y Bernoulli)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Por un tubo Venturi horizontal con agua ($\\rho = 1000\\,\\text{{kg/m}}^3$), el radio pasa de $r_1 = {fmt(r1*100,1)}\\,\\text{{cm}}$ "
            f"a $r_2 = {fmt(r2*100,1)}\\,\\text{{cm}}$. Si $v_1 = {fmt(v1,2)}\\,\\text{{m/s}}$, calcule: A) $v_2$, "
            f"B) la diferencia de presión $p_1 - p_2$ en kPa y C) la diferencia de alturas $\\Delta h$ en los tubos piezométricos."
        ),
    ).usa(r"A_1 v_1 = A_2 v_2", r"p_1 + \rho g y_1 + \frac{1}{2}\rho v_1^2 = p_2 + \rho g y_2 + \frac{1}{2}\rho v_2^2", r"p = p_0 + \rho gh")
    lec.paso("A) Continuidad para hallar v₂")\
        .por_que("A = πr²: al reducirse el radio a la mitad, el área se reduce 4 veces y v₂ = 4·v₁.")\
        .di("Aplicamos continuidad entre las dos secciones.",
            {"a": "mate", "tex": rf"v_2 = v_1\left(\frac{{r_1}}{{r_2}}\right)^2 = ({fmt(v1,2)})(4) = {fmt(v2,2)}\,\text{{m/s}}", "color": "verde", "rol": "resultado"})
    lec.paso("B) Bernoulli y C) diferencia de altura Δh")\
        .por_que("Como el tubo es horizontal, y₁ = y₂ y Δp = ½ρ(v₂² − v₁²); en los tubos verticales Δp = ρgΔh.")\
        .di("Calculamos delta p con Bernoulli y luego delta hache.",
            {"a": "mate", "tex": rf"p_1 - p_2 = \frac{{1}}{{2}}(1000)({fmt(v2,2)}^2 - {fmt(v1,2)}^2) = {fmt(dp/1000,2)}\,\text{{kPa}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"\Delta h = \frac{{p_1 - p_2}}{{\rho g}} = {fmt(dh,3)}\,\text{{m}}", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"v_2 = {fmt(v2,2)}\,\text{{m/s}}", valor=round(v2, 2), unidad="m/s")
    lec.responde("B)", rf"\Delta p = {fmt(dp/1000,2)}\,\text{{kPa}}", valor=round(dp / 1000, 2), unidad="kPa")
    lec.responde("C)", rf"\Delta h = {fmt(dh,3)}\,\text{{m}}", valor=round(dh, 3), unidad="m")
    return lec


def _v_poiseuille_reynolds(rng: random.Random) -> Leccion:
    """Parcial 2: Flujo laminar viscoso (Poiseuille y número de Reynolds)."""
    R_mm = rng.choice([4.0, 5.0, 6.0])
    R = R_mm / 1000.0
    L = rng.choice([2.0, 4.0, 5.0])
    eta = rng.choice([0.050, 0.080, 0.100])
    rho = 900.0
    dp_kpa = rng.choice([8.0, 12.0, 16.0])
    dp = dp_kpa * 1000.0
    Q = (dp * math.pi * (R ** 4)) / (8.0 * eta * L)
    v_med = Q / (math.pi * R * R)
    Re = (rho * v_med * (2.0 * R)) / eta
    lec = Leccion(
        f"VAR-POIS-{rng.randint(100,999)}",
        "Variante · Aceite en tubería: Poiseuille y Reynolds",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Aceite ($\\rho = 900\\,\\text{{kg/m}}^3$, $\\eta = {fmt(eta,3)}\\,\\text{{Pa}}\\cdot\\text{{s}}$) fluye por un tubo "
            f"de radio $R = {fmt(R_mm,1)}\\,\\text{{mm}}$ y largo $L = {fmt(L,1)}\\,\\text{{m}}$ bajo $\\Delta p = {fmt(dp_kpa,1)}\\,\\text{{kPa}}$. "
            f"Calcule: A) caudal $dV/dt$ en $\\text{{cm}}^3/\\text{{s}}$, B) rapidez media $v$ y C) número de Reynolds $N_R$."
        ),
    ).usa(r"\frac{dV}{dt} = \frac{(p_1 - p_2)\pi R^4}{8\eta L}", r"\frac{dV}{dt} = Av", r"N_R = \frac{\rho v D}{\eta}")
    lec.paso("A) Caudal con Poiseuille, B) rapidez media y C) Reynolds")\
        .por_que("Con viscosidad η aplica Poiseuille (usando radio R en metros a la cuarta potencia).")\
        .di("Sustituimos todo en unidades del SI y evaluamos Reynolds con el diámetro D igual a dos R.",
            {"a": "mate", "tex": rf"\frac{{dV}}{{dt}} = \frac{{({fmt(dp,0)})\pi({fmt(R,4)})^4}}{{8({fmt(eta,3)})({fmt(L,1)})}} = {fmt(Q*1e6,2)}\,\text{{cm}}^3/\text{{s}}", "color": "verde", "rol": "resultado"},
            {"a": "mate", "tex": rf"v = \frac{{Q}}{{\pi R^2}} = {fmt(v_med,3)}\,\text{{m/s}}\qquad N_R = \frac{{\rho v (2R)}}{{\eta}} = {fmt(Re,1)}", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"Q = {fmt(Q*1e6,2)}\,\text{{cm}}^3/\text{{s}}", valor=round(Q * 1e6, 2), unidad="cm³/s")
    lec.responde("B)", rf"v = {fmt(v_med,3)}\,\text{{m/s}}", valor=round(v_med, 3), unidad="m/s")
    lec.responde("C)", rf"N_R = {fmt(Re,1)}", valor=round(Re, 1), unidad="")
    return lec


def _v_represa_integral(rng: random.Random) -> Leccion:
    """Parcial 2: Fuerza hidrostática sobre una represa trapezoidal mediante integral."""
    H = rng.choice([10.0, 20.0, 30.0, 40.0])
    b_sup = rng.choice([30.0, 40.0, 50.0])
    b_inf = b_sup - rng.choice([10.0, 15.0, 20.0])
    rho, g = 1000.0, 9.80
    # L(h) = b_sup - m*h, con m = (b_sup - b_inf)/H
    m = (b_sup - b_inf) / H
    # F = rho*g * integral_0^H (b_sup*h - m*h^2) dh = rho*g * (b_sup*H^2/2 - m*H^3/3)
    F_tot = rho * g * (b_sup * H * H / 2.0 - m * (H ** 3) / 3.0)
    F_mn = F_tot / 1e6
    lec = Leccion(
        f"VAR-PRES-{rng.randint(100,999)}",
        "Variante · Fuerza del agua sobre represa trapezoidal (integral)",
        tema="fluidos", tipo="desarrollo", nivel=4, parcial=2,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una represa trapezoidal retiene agua hasta una profundidad $H = {fmt(H,1)}\\,\\text{{m}}$. "
            f"Mide ${fmt(b_sup,1)}\\,\\text{{m}}$ de ancho en la superficie y ${fmt(b_inf,1)}\\,\\text{{m}}$ en el fondo. "
            f"Plantee el diferencial $dF$ y calcule la fuerza total del agua $F$ en MN."
        ),
        visual={"tipo": "pared", "forma": "trapecio", "H": H, "ancho_sup": b_sup, "ancho_inf": b_inf, "titulo": "Represa trapezoidal"},
    ).usa(r"p = p_0 + \rho gh", r"F_\perp = \int p\,dA")
    lec.paso("1) Ancho L(h) y diferencial de fuerza dF")\
        .por_que("En una franja horizontal a profundidad h, la presión es p = ρgh y el área dA = L(h)dh.")\
        .di("El ancho baja linealmente desde la superficie hasta el fondo.",
            {"a": "mate", "tex": rf"L(h) = {fmt(b_sup,1)} - {fmt(m,3)}\,h", "color": "azul"},
            {"a": "mate", "tex": rf"dF = \rho g h\,L(h)\,dh = \rho g\left({fmt(b_sup,1)}\,h - {fmt(m,3)}\,h^2\right)dh", "color": "azul"})
    lec.paso("2) Integrar de h = 0 a H")\
        .por_que("Aplicamos la regla de la potencia a h y a h² y evaluamos arriba menos abajo.")\
        .di("Integramos término por término desde cero hasta H.",
            {"a": "mate", "tex": rf"F = \rho g\left[{fmt(b_sup/2,2)}\,h^2 - \frac{{{fmt(m,3)}}}{{3}}\,h^3\right]_0^{{{fmt(H,1)}}} = {fmt(F_mn,2)}\,\text{{MN}}", "color": "verde", "rol": "resultado"})
    lec.responde("A)", rf"F = {fmt(F_mn,2)}\,\text{{MN}}", valor=round(F_mn, 2), unidad="MN")
    return lec


ARQUETIPOS: Dict[str, Callable[[random.Random], Leccion]] = {
    "conduccion_area": _v_conduccion_area,
    "conduccion_k_radial": _v_conduccion_k_radial,
    "ciclo_pv": _v_ciclo_pv,
    "calorimetria_fase": _v_calorimetria_fase,
    "conduccion_serie_paralelo": _v_conduccion_serie_paralelo,
    "dilatacion_barras": _v_dilatacion_barras,
    "radiacion_stefan": _v_radiacion_stefan,
    "maquina_otto_refrig": _v_maquina_otto,
    "conduccion_cono": _v_conduccion_cono,
    "conduccion_exponencial": _v_conduccion_exponencial,
    "conduccion_cilindro_capas": _v_conduccion_cilindro_capas,
    "ciclo_recta_integral": _v_ciclo_recta_integral,
    "esfuerzo_termico": _v_esfuerzo_termico,
    "otto_completo_inverso": _v_otto_completo_inverso,
    "calorimetria_fusion_trampa": _v_calorimetria_fusion_trampa,
    # Parcial 1 (5 arquetipos de Reposición)
    "resorte_fase": _v_resorte_fase,
    "pendulo_simple_fisico": _v_pendulo_simple_fisico,
    "cuerda_armonicos": _v_cuerda_armonicos,
    "onda_estacionaria": _v_onda_estacionaria,
    "sonido_intensidad": _v_sonido_intensidad,
    # Parcial 2 (4 arquetipos de Reposición)
    "flotacion_arquimedes": _v_flotacion_arquimedes,
    "venturi_bernoulli": _v_venturi_bernoulli,
    "poiseuille_reynolds": _v_poiseuille_reynolds,
    "represa_integral": _v_represa_integral,
}

_MAPA_DIRECTO: Dict[str, str] = {
    "Q049": "conduccion_area",
    "Q050": "conduccion_area",
    "E8": "conduccion_k_radial",
    "Q057": "conduccion_k_radial",
    "Q060": "conduccion_k_radial",
    "E16": "ciclo_pv",
    "Q111": "ciclo_pv",
    "Q110": "ciclo_pv",
    "Q109": "ciclo_pv",
    "Q108": "ciclo_recta_integral",
    "Q106": "ciclo_pv",
    "Q107": "ciclo_pv",
    "E5": "calorimetria_fase",
    "Q038": "conduccion_serie_paralelo",
    "E1": "dilatacion_barras",
    "Q063": "radiacion_stefan",
    "E10": "radiacion_stefan",
    "E17": "maquina_otto_refrig",
    "E21": "maquina_otto_refrig",
    "Q117": "maquina_otto_refrig",
    "Q128": "maquina_otto_refrig",
    "PR1-4": "conduccion_cono",
    "PR2-D1": "conduccion_cono",
    "Q041": "conduccion_cono",
    "T-CONO": "conduccion_cono",
    "PR2-D2": "conduccion_exponencial",
    "PR2-D10": "conduccion_exponencial",
    "PR1-3": "conduccion_cilindro_capas",
    "Q045": "conduccion_cilindro_capas",
    "PR1-5": "ciclo_recta_integral",
    "PR2-D4": "ciclo_recta_integral",
    "P26-1": "esfuerzo_termico",
    "PR2-S13": "esfuerzo_termico",
    "PR2-D9": "otto_completo_inverso",
    "PR1-2": "calorimetria_fusion_trampa",
    "PR2-D7": "calorimetria_fusion_trampa",
    "PR2-D12": "calorimetria_fusion_trampa",
    # Mapeo directo P1 y P2
    "RP-26I-7": "resorte_fase",
    "RP-24II-1": "resorte_fase",
    "RP-25II-1": "resorte_fase",
    "P1O26-8": "resorte_fase",
    "T-MAS": "resorte_fase",
    "P1E26-8": "pendulo_simple_fisico",
    "RP-26I-2": "pendulo_simple_fisico",
    "T-PEND": "pendulo_simple_fisico",
    "P1O26-9": "cuerda_armonicos",
    "P1E26-9": "cuerda_armonicos",
    "RP-24I-1": "cuerda_armonicos",
    "RP-26I-1": "cuerda_armonicos",
    "T-ONDA": "cuerda_armonicos",
    "RP-23II-1": "onda_estacionaria",
    "P1O26-6": "onda_estacionaria",
    "P1O26-2": "sonido_intensidad",
    "T-SON": "sonido_intensidad",
    "RP-26I-3": "flotacion_arquimedes",
    "P2O26-2": "flotacion_arquimedes",
    "P2E26-1": "flotacion_arquimedes",
    "T-FLOT": "flotacion_arquimedes",
    "RP-24II-2": "venturi_bernoulli",
    "RP-26I-8": "venturi_bernoulli",
    "SUF26-4": "venturi_bernoulli",
    "T-BERN": "venturi_bernoulli",
    "RP-23II-2": "poiseuille_reynolds",
    "P2O26-9": "poiseuille_reynolds",
    "P2E26-9": "poiseuille_reynolds",
    "T-VISC": "poiseuille_reynolds",
    "RP-24I-2": "represa_integral",
    "RP-25II-2": "represa_integral",
    "P2E26-8": "represa_integral",
    "E2-8": "represa_integral",
    "T-PARED": "represa_integral",
}

_MAPA_TEMA: Dict[str, str] = {
    "dilatacion": "dilatacion_barras",
    "calorimetria": "calorimetria_fase",
    "conduccion": "conduccion_area",
    "radiacion": "radiacion_stefan",
    "gases": "ciclo_pv",
    "primera_ley": "ciclo_pv",
    "ciclos": "ciclo_pv",
    "maquinas": "maquina_otto_refrig",
    "entropia": "maquina_otto_refrig",
    "ondas": "resorte_fase",
    "fluidos": "venturi_bernoulli",
}


def arquetipo_de(id_o_tema: str) -> str:
    clave = (id_o_tema or "").strip()
    if clave in ARQUETIPOS:
        return clave
    up = clave.upper()
    if up in _MAPA_DIRECTO:
        return _MAPA_DIRECTO[up]
    if registro.existe(up):
        lec = registro.obtener(up)
        en = (lec.enunciado or "").lower()
        if lec.tema == "conduccion":
            if "k(" in en or "k_0" in en or "esfera" in en or "cilindr" in en:
                return "conduccion_k_radial"
            if "serie" in en or "paralelo" in en:
                return "conduccion_serie_paralelo"
            return "conduccion_area"
        return _MAPA_TEMA.get(lec.tema, "ciclo_pv")
    return _MAPA_TEMA.get(clave.lower(), "conduccion_area")


def generar_variante(id_o_tema: str, semilla: Optional[int] = None) -> Leccion:
    """Genera una variante paramétrica determinista lista para estudiar."""
    arq = arquetipo_de(id_o_tema)
    rng = random.Random(semilla) if semilla is not None else random.Random()
    lec = ARQUETIPOS[arq](rng)
    # Igual que en el banco: cada cuenta de varias operaciones se desglosa renglón por renglón
    from . import desglose
    desglose.aplicar(lec)
    return lec
