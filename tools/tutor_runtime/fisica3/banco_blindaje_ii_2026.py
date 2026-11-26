"""Práctica de blindaje II-2026: desarrollos verificados para las 4 casillas nuevas o sin cobertura.

Cubren exactamente las 4 celdas de `Ecuaciones_Tercer_Parcial_FG3.pdf` detectadas sin desarrollo
en la auditoría forense de los 17 exámenes históricos + 2 prácticas II-2026:
  - P26-1: Esfuerzo térmico (`F1C3`) + Dilatación lineal y volumétrica (`F1C1`, `F1C2`).
  - P26-2: Escala Fahrenheit (`F12C2`) + Kelvin (`F10C3`) + Calorimetría (`F3C2`).
  - P26-3: Teoría cinética con Avogadro (`F5C3`, `F13C2`), Boltzmann (`F5C2`, `F13C1`),
           `v_rms` (`F5C1`), `m_total = nM` (`F4C2`), `K = 3/2 nRT` (`F4C3`) y `1 L = 1000 cm³` (`F14C2`).
"""

from __future__ import annotations

import math

from .ayudas import ATM, KB, NA, R_GAS, T0
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .formato import dice, n, nu, tex
from .registro import leccion

C = r"^\circ\text{C}"
DEG_F = r"^\circ\text{F}"


def _fuente_b(num: int) -> dict:
    return {
        "doc": "blindaje-ii-2026",
        "nombre": "Práctica de blindaje II-2026 (fórmulas nuevas de la hoja)",
        "num": str(num),
        "texto": f"Práctica de blindaje II-2026 · problema {num}",
    }


# ============================================================================ P26-1
@leccion("P26-1")
def p26_1() -> Leccion:
    l0 = 2.00
    area = 4.00e-4
    v0 = l0 * area
    alpha = 1.20e-5
    beta = 3.0 * alpha
    young = 2.00e11
    t0, t1, t2 = 20.0, 70.0, -10.0
    dt1 = t1 - t0
    dt2 = t2 - t0
    dl = alpha * l0 * dt1
    dv = beta * v0 * dt1
    esf1 = -young * alpha * dt1
    f1 = esf1 * area
    esf2 = -young * alpha * dt2
    f2 = esf2 * area
    esf_lim = 1.80e8
    dt_max = esf_lim / (young * alpha)
    t_max = t0 + dt_max

    L = Leccion(
        "P26-1",
        "Dilatación y esfuerzo térmico en una barra rígidamente sujeta",
        tema="4.4",
        tipo="desarrollo",
        nivel=3,
        fuente=_fuente_b(1),
        enunciado=(
            "Una barra de acero ($L_0=2{,}00$ m, sección $A=4{,}00\\times 10^{-4}\\ \\text{m}^2$, "
            "$\\alpha=1{,}20\\times 10^{-5}\\ ^\\circ\\text{C}^{-1}$, módulo de Young $Y=2{,}00\\times 10^{11}$ Pa) "
            "está inicialmente sin esfuerzo a $20{,}0\\ ^\\circ$C. "
            "a) Si quedara libre y se calentara a $70{,}0\\ ^\\circ$C, calcule $\\Delta L$ y $\\Delta V$. "
            "b) Si se sujeta rígidamente entre dos muros y se calienta a $70{,}0\\ ^\\circ$C, halle el esfuerzo térmico $F/A$ y la fuerza $F$. "
            "c) Si estando sujeta se enfría de $20{,}0\\ ^\\circ$C a $-10{,}0\\ ^\\circ$C, calcule el nuevo esfuerzo y su tipo. "
            "d) ¿Cuál es la temperatura máxima admisible si $|F/A|$ no debe superar $1{,}80\\times 10^8$ Pa?"
        ),
        claves=["esfuerzo térmico", "dilatación", "compresión", "tensión", "fórmula nueva II-2026"],
        visual={
            "tipo": "dilatacion",
            "caso": "barras",
            "u": "m",
            "T0": t0,
            "dTmax": dt_max,
            "barras": [{"n": "barra de acero", "L": l0, "a": alpha, "color": "azul"}],
            "pie": "Sujeta entre muros: el esfuerzo térmico impide este alargamiento",
        },
    )

    L.paso("Qué nos dan y qué piden")\
        .di("Esta ecuación de esfuerzo térmico entra nueva en el formulario dos mil veintiséis. Primero ordenamos todos los datos en unidades del sistema internacional.",
            DATO("rígidamente"),
            LISTA([
                "$L_0=2{,}00$ m, $A=4{,}00\\times 10^{-4}\\ \\text{m}^2$, $V_0=L_0 A=8{,}00\\times 10^{-4}\\ \\text{m}^3$",
                "$\\alpha=1{,}20\\times 10^{-5}\\ ^\\circ\\text{C}^{-1}$, $Y=2{,}00\\times 10^{11}$ Pa",
                "$T_0=20{,}0\\ ^\\circ$C; se calienta a $70{,}0\\ ^\\circ$C y luego se enfría a $-10{,}0\\ ^\\circ$C",
            ]))

    L.paso("a) Dilatación lineal y volumétrica libre")\
        .di("Si la barra estuviera libre, su cambio de largo sale directo de la casilla de dilatación lineal de la hoja.",
            V(foco="calentar"), F(r"\Delta L=\alpha L_0\,\Delta T"),
            M(rf"\Delta T_1=70{{,}}0-20{{,}}0=50{{,}}0\ {C}"),
            M(rf"\Delta L=(1{{,}}20\times 10^{{-5}})(2{{,}}00)(50{{,}}0)={tex(dl, 3, u='m')}"),
            R(rf"\Delta L={tex(dl * 1e3, 3)}\ \text{{mm}}", "ra1"))\
        .di("Para el volumen derivamos beta igual a tres alfa desde la dilatación lineal y aplicamos la casilla volumétrica.",
            M(r"V=L_0^3(1+\alpha\,\Delta T)^3\approx V_0(1+3\alpha\,\Delta T)\;\Rightarrow\;\beta=3\alpha=3{,}60\times 10^{-5}\ ^\circ\text{C}^{-1}"),
            F(r"\Delta V=\beta V_0\,\Delta T"),
            M(rf"\Delta V=(3{{,}}60\times 10^{{-5}})(8{{,}}00\times 10^{{-4}})(50{{,}}0)={tex(dv, 3, u='m^3')}"),
            R(rf"\Delta V={tex(dv, 3, u='m^3')}", "ra2"))

    L.paso("b) Sujeta y calentada a 70 °C: compresión")\
        .di("Ahora los muros impiden que la barra se alargue. Usamos la casilla de esfuerzo térmico de la hoja.",
            F(r"\frac{F}{A}=-Y\alpha\,\Delta T"),
            M(rf"\frac{{F}}{{A}}=-(2{{,}}00\times 10^{{11}})(1{{,}}20\times 10^{{-5}})(50{{,}}0)"),
            M(rf"\frac{{F}}{{A}}=-({tex(young * alpha, 4)})(50{{,}}0)={tex(esf1, 3, u='Pa')}"),
            R(rf"\frac{{F}}{{A}}={tex(esf1 / 1e6, 3)}\ \text{{MPa (compresión)}}", "rb1"))\
        .di("Multiplicamos el esfuerzo por el área para obtener la fuerza que hacen los soportes.",
            M(rf"F=\left(\frac{{F}}{{A}}\right)A=({tex(esf1, 3)})(4{{,}}00\times 10^{{-4}})={tex(f1, 3, u='N')}"),
            R(rf"F={tex(f1 / 1e3, 3)}\ \text{{kN}}", "rb2"),
            OJO("El signo menos indica esfuerzo de **compresión**: la barra quiere crecer y los muros la empujan hacia adentro."))

    L.paso("c) Sujeta y enfriada a −10 °C: tensión")\
        .di("Al enfriar la barra desde veinte hasta menos diez grados, el cambio de temperatura es negativo.",
            M(rf"\Delta T_2=-10{{,}}0-20{{,}}0=-30{{,}}0\ {C}"),
            M(rf"\frac{{F}}{{A}}=-(2{{,}}00\times 10^{{11}})(1{{,}}20\times 10^{{-5}})(-30{{,}}0)=+{tex(esf2, 3, u='Pa')}"),
            R(rf"\frac{{F}}{{A}}=+{tex(esf2 / 1e6, 3)}\ \text{{MPa (tensión)}}", "rc1"))\
        .di("Menos por menos da más: ahora el esfuerzo es positivo, de tracción o tensión, porque la barra intenta encogerse.",
            M(rf"F=({tex(esf2, 3)})(4{{,}}00\times 10^{{-4}})={tex(f2, 3, u='N')}"),
            R(rf"F=+{tex(f2 / 1e3, 3)}\ \text{{kN}}", "rc2"))

    L.paso("d) Temperatura máxima admisible")\
        .di("Despejamos el cambio máximo de temperatura en magnitud desde la misma casilla de esfuerzo térmico.",
            M(r"\left|\frac{F}{A}\right|=Y\alpha\,\Delta T_{\text{máx}}\;\Rightarrow\;\Delta T_{\text{máx}}=\frac{|F/A|_{\text{máx}}}{Y\alpha}"),
            M(rf"\Delta T_{{\text{{máx}}}}=\frac{{1{{,}}80\times 10^8}}{{(2{{,}}00\times 10^{{11}})(1{{,}}20\times 10^{{-5}})}}=\frac{{1{{,}}80\times 10^8}}{{{tex(young * alpha, 4)}}}={tex(dt_max, 3)}\ {C}"))\
        .di("Sumamos ese aumento máximo a la temperatura inicial sin esfuerzo.",
            M(rf"T_{{\text{{máx}}}}=20{{,}}0+{tex(dt_max, 3)}={tex(t_max, 3)}\ {C}"),
            R(rf"T_{{\text{{máx}}}}={tex(t_max, 3)}\ {C}", "rd"))

    L.responde("a", rf"\Delta L={tex(dl * 1e3, 3)}\ \text{{mm}}", dl * 1e3, "mm")\
     .responde("b", rf"F/A={tex(esf1 / 1e6, 3)}\ \text{{MPa}}", esf1 / 1e6, "MPa")\
     .responde("c", rf"F/A=+{tex(esf2 / 1e6, 3)}\ \text{{MPa}}", esf2 / 1e6, "MPa")\
     .responde("d", rf"T_{{\text{{máx}}}}={tex(t_max, 3)}\ {C}", t_max, "°C")
    L.te_toca(
        "Si el área de la barra se duplicara ($A=8{,}00\\times 10^{-4}\\ \\text{m}^2$) manteniendo el mismo material y $\\Delta T=+50{,}0\\ ^\\circ\\text{C}$, ¿cuánto vale la magnitud del esfuerzo térmico $|F/A|$ en MPa?",
        120.0,
        "MPa",
        pista="Mira la fórmula $F/A = -Y\\alpha\\Delta T$: el esfuerzo no depende del área (lo que sí se duplica es la fuerza $F$).",
        solucion="$|F/A| = Y\\alpha\\Delta T = 120\\text{ MPa}$ no cambia; la fuerza $F$ sí se duplica a $96{,}0\\text{ kN}$.",
    )
    return L.usa(r"\Delta L=\alpha L_0\Delta T", r"\Delta V=\beta V_0\Delta T", r"\frac{F}{A}=-Y\alpha\Delta T")


# ============================================================================ P26-2
@leccion("P26-2")
def p26_2() -> Leccion:
    m = 1.50
    c_si = 2160.0
    c_f = c_si / 1.8
    tf1, tf2 = 68.0, 158.0
    tc1 = (tf1 - 32.0) * 5.0 / 9.0
    tc2 = (tf2 - 32.0) * 5.0 / 9.0
    tk1 = tc1 + T0
    tk2 = tc2 + T0
    dt_f = tf2 - tf1
    dt_c = tc2 - tc1
    q = m * c_si * dt_c

    L = Leccion(
        "P26-2",
        "Escalas Fahrenheit, Celsius y Kelvin aplicadas a calorimetría",
        tema="4.3",
        tipo="desarrollo",
        nivel=2,
        fuente=_fuente_b(2),
        enunciado=(
            "Se calientan $1{,}50$ kg de aceite térmico ($c=2160$ J/(kg·K)) desde $68{,}0\\ ^\\circ$F hasta $158{,}0\\ ^\\circ$F "
            "sin cambio de fase. "
            "a) Convierta ambas temperaturas a grados Celsius y a kelvin usando la hoja oficial. "
            "b) Demuestre la relación entre $\\Delta T_{^\\circ\\text{F}}$ y $\\Delta T_{^\\circ\\text{C}}$ y compruebe por qué el $32$ desaparece en un cambio de temperatura. "
            "c) Exprese el calor específico en J/(kg·°F). "
            "d) Calcule el calor absorbido $Q$ en SI y compruébelo en grados Fahrenheit."
        ),
        claves=["Fahrenheit", "Celsius", "Kelvin", "calor específico", "fórmula nueva II-2026"],
        visual={
            "tipo": "calor",
            "Tf": tc2,
            "componentes": [{"n": "aceite", "m": 1500, "T": tc1, "fase": "agua"}],
            "necesita": [{"n": "calentar de 20 a 70 °C", "Q": q}],
            "disponible": [{"n": "fuente térmica", "Q": q}],
        },
    )

    L.paso("Qué nos dan y qué piden")\
        .di("La conversión a Fahrenheit entra por primera vez en el formulario del tercer parcial. Hay que distinguir siempre entre una temperatura puntual y un cambio de temperatura.",
            DATO("68,0 °F"),
            LISTA([
                "Masa $m=1{,}50$ kg, calor específico $c=2160$ J/(kg·K)",
                "$T_1=68{,}0\\ ^\\circ$F, $T_2=158{,}0\\ ^\\circ$F",
            ]))

    L.paso("a) Convertir T₁ y T₂ a Celsius y kelvin")\
        .di("Partimos de la casilla Fahrenheit de la hoja y despejamos la temperatura en Celsius.",
            F(r"T_{^\circ\text{F}}=\frac{9}{5}T_{^\circ\text{C}}+32"),
            M(r"T_{^\circ\text{C}}=\frac{5}{9}\left(T_{^\circ\text{F}}-32\right)"))\
        .di("Sustituimos sesenta y ocho y ciento cincuenta y ocho grados Fahrenheit.",
            M(rf"T_{{1,^\circ\text{{C}}}}=\frac{{5}}{{9}}(68{{,}}0-32)=\frac{{5}}{{9}}(36{{,}}0)={tex(tc1, 3)}\ {C}"),
            M(rf"T_{{2,^\circ\text{{C}}}}=\frac{{5}}{{9}}(158{{,}}0-32)=\frac{{5}}{{9}}(126{{,}}0)={tex(tc2, 3)}\ {C}"))\
        .di("Ahora aplicamos la casilla de conversión a kelvin de la hoja.",
            F(r"T_K=T_{^\circ\text{C}}+273{,}15"),
            M(rf"T_{{1,K}}={tex(tc1, 3)}+273{{,}}15={tex(tk1, 5)}\ \text{{K}}\qquad T_{{2,K}}={tex(tc2, 3)}+273{{,}}15={tex(tk2, 5)}\ \text{{K}}"),
            R(rf"T_1={tex(tc1, 3)}\ {C}={tex(tk1, 5)}\ \text{{K}}\qquad T_2={tex(tc2, 3)}\ {C}={tex(tk2, 5)}\ \text{{K}}", "ra"))

    L.paso("b) Cambio de temperatura: el 32 se cancela")\
        .di("Restamos las dos temperaturas en Fahrenheit escritas con la ecuación de la hoja.",
            M(r"\Delta T_{^\circ\text{F}}=T_{2,^\circ\text{F}}-T_{1,^\circ\text{F}}=\left(\frac{9}{5}T_{2,^\circ\text{C}}+32\right)-\left(\frac{9}{5}T_{1,^\circ\text{C}}+32\right)"),
            M(r"\Delta T_{^\circ\text{F}}=\frac{9}{5}\left(T_{2,^\circ\text{C}}-T_{1,^\circ\text{C}}\right)=\frac{9}{5}\,\Delta T_{^\circ\text{C}}=1{,}8\,\Delta T_{^\circ\text{C}}"))\
        .di("Comprobamos con los números del aceite: noventa grados Fahrenheit equivalen a cincuenta grados Celsius o cincuenta kelvin.",
            M(rf"\Delta T_{{^\circ\text{{F}}}}=158{{,}}0-68{{,}}0=90{{,}}0\ {DEG_F}\qquad \Delta T_{{^\circ\text{{C}}}}=\Delta T_K=70{{,}}0-20{{,}}0=50{{,}}0\ \text{{K}}"),
            R(rf"\Delta T_{{^\circ\text{{F}}}}=\frac{{9}}{{5}}\,\Delta T_{{^\circ\text{{C}}}}\;\Rightarrow\;90{{,}}0\ {DEG_F}=50{{,}}0\ \text{{K}}", "rb"),
            OJO("Error clásico de examen: nunca sumes ni restes $32$ a un $\\Delta T$; solo multiplica o divide por $1{,}8$."))

    L.paso("c) Calor específico en J/(kg·°F)")\
        .di("Un cambio de un kelvin vale uno coma ocho grados Fahrenheit. Como la temperatura está en el denominador, dividimos entre uno coma ocho.",
            M(r"1\ \text{K}=1{,}8\ ^\circ\text{F}\;\Rightarrow\;c_{^\circ\text{F}}=2160\ \frac{\text{J}}{\text{kg}\cdot\text{K}}\cdot\frac{1\ \text{K}}{1{,}8\ ^\circ\text{F}}=\frac{2160}{1{,}8}"),
            R(rf"c={tex(c_f, 4, sci='no')}\ \text{{J/(kg}}\cdot {DEG_F}\text{{)}}", "rc"))

    L.paso("d) Calor absorbido en SI y comprobación en °F")\
        .di("Aplicamos la casilla de calor sensible de la hoja en el sistema internacional.",
            V(foco="comparar"), F(r"Q=mc\,\Delta T"),
            M(rf"Q=(1{{,}}50)(2160)(50{{,}}0)=({tex(m * c_si, 4, sci='no')})(50{{,}}0)={tex(q, 6, sci='no')}\ \text{{J}}"))\
        .di("Comprobamos usando el calor específico en grados Fahrenheit y el cambio de noventa grados Fahrenheit: sale exactamente la misma energía.",
            M(rf"Q=(1{{,}}50)(1200)(90{{,}}0)={tex(m * c_f * dt_f, 6, sci='no')}\ \text{{J}}"),
            R(rf"Q={tex(q / 1e3, 3)}\ \text{{kJ}}\;({tex(q, 6, sci='no')}\ \text{{J}})", "rd"))

    L.responde("a", rf"T_1={tex(tc1, 3)}\ {C},\ T_2={tex(tc2, 3)}\ {C}", tc2, "°C")\
     .responde("c", rf"c={tex(c_f, 4, sci='no')}\ \text{{J/(kg}}\cdot {DEG_F}\text{{)}}", c_f, "J/(kg·°F)")\
     .responde("d", rf"Q={tex(q / 1e3, 3)}\ \text{{kJ}}", q / 1e3, "kJ")
    L.te_toca(
        "Si un cambio de temperatura es $\\Delta T = 36{,}0\\ ^\\circ\\text{F}$, ¿cuánto vale ese mismo cambio en kelvin?",
        20.0,
        "K",
        pista="Divide entre $1{,}8$ (sin restar $32$).",
        solucion="$\\Delta T_K = \\Delta T_{^\\circ\\text{C}} = 36{,}0 / 1{,}8 = 20{,}0\\text{ K}$.",
    )
    return L.usa(r"T_{^\circ\text{F}}=\frac{9}{5}T_{^\circ\text{C}}+32", r"T_K=T_{^\circ\text{C}}+273{,}15", r"Q=mc\Delta T")


# ============================================================================ P26-3
@leccion("P26-3")
def p26_3() -> Leccion:
    v_cm3 = 500.0
    v_l = v_cm3 / 1000.0
    v_m3 = v_l * 1e-3
    p_atm = 2.00
    p_pa = p_atm * ATM
    tc = 27.0
    tk = tc + T0
    M_mol = 0.0280
    n_mol = p_pa * v_m3 / (R_GAS * tk)
    m_tot = n_mol * M_mol
    m_molec = M_mol / NA
    n_molec = n_mol * NA
    v_rms_M = math.sqrt(3.0 * R_GAS * tk / M_mol)
    v_rms_m = math.sqrt(3.0 * KB * tk / m_molec)
    # La comprobación con Boltzmann se hace con lo que se ve escrito: k_B = 1,38e-23 y m con cuatro cifras
    v_rms_kb_vis = math.sqrt(3.0 * 1.38e-23 * tk / float(f"{m_molec:.3e}"))
    k_med = 1.5 * KB * tk
    k_tot = 1.5 * n_mol * R_GAS * tk

    L = Leccion(
        "P26-3",
        "Teoría cinética completa con Avogadro y volumen en cm³",
        tema="6.3",
        tipo="desarrollo",
        nivel=3,
        fuente=_fuente_b(3),
        enunciado=(
            "Un recipiente rígido de $500\\ \\text{cm}^3$ contiene nitrógeno gaseoso ($M=0{,}0280$ kg/mol) a $2{,}00$ atm y $27{,}0\\ ^\\circ$C. "
            "a) Convierta $V$, $p$ y $T$ al SI con las constantes de la hoja y halle los moles $n$ y la masa total $m_{\\text{total}}$. "
            "b) Calcule la masa $m$ de una sola molécula y el número total de moléculas $N$ usando $M=N_A m$. "
            "c) Calcule la rapidez cuadrática media $v_{\\text{rms}}$ por ambas formas de la hoja. "
            "d) Halle la energía cinética traslacional media por molécula y la energía cinética traslacional total $K$."
        ),
        claves=["teoría cinética", "Avogadro", "Boltzmann", "v_rms", "cm³", "fórmula nueva II-2026"],
        visual={"tipo": "gas", "modo": "temperatura", "pie": "N₂ en 500 cm³ a 2,00 atm y 27,0 °C"},
    )

    L.paso("a) Conversiones al SI con las constantes de la hoja")\
        .di("Empezamos convirtiendo el volumen a metros cúbicos y la presión a pascales con las equivalencias impresas al pie del formulario.",
            DATO("500 cm³"),
            M(r"1\ \text{L}=1000\ \text{cm}^3\;\Rightarrow\;V=\frac{500\ \text{cm}^3}{1000}=0{,}500\ \text{L}=5{,}00\times 10^{-4}\ \text{m}^3"),
            M(rf"1\ \text{{atm}}=101325\ \text{{Pa}}\;\Rightarrow\;p=(2{{,}}00)(101325)={tex(p_pa, 5, sci='no')}\ \text{{Pa}}"))\
        .di("Y pasamos la temperatura de grados Celsius a kelvin sumando doscientos setenta y tres coma quince.",
            F(r"T_K=T_{^\circ\text{C}}+273{,}15"),
            M(rf"T=27{{,}}0+273{{,}}15={tex(tk, 5)}\ \text{{K}}"))

    L.paso("a) Moles y masa total del gas")\
        .di("Despejamos el número de moles de la ley del gas ideal de la hoja.",
            V(foco="presion"), F(r"pV=nRT\;\Rightarrow\;n=\frac{pV}{RT}"),
            M(rf"n=\frac{{({tex(p_pa, 5, sci='no')})(5{{,}}00\times 10^{{-4}})}}{{(8{{,}}314)({tex(tk, 5)})}}=\frac{{{tex(p_pa * v_m3, 4)}}}{{{tex(R_GAS * tk, 4)}}}={tex(n_mol, 4)}\ \text{{mol}}"))\
        .di("Con la casilla de masa total y moles de la hoja multiplicamos por la masa molar.",
            F(r"m_{\text{total}}=nM"),
            M(rf"m_{{\text{{total}}}}=({tex(n_mol, 4)})(0{{,}}0280)={tex(m_tot, 4, u='kg')}"),
            R(rf"n={tex(n_mol, 3)}\ \text{{mol}}\qquad m_{{\text{{total}}}}={tex(m_tot * 1e3, 3)}\ \text{{g}}", "ra"))

    L.paso("b) Masa de una molécula y número total N")\
        .di("Usamos la casilla de masa molar y número de Avogadro de la hoja.",
            F(r"M=N_A m\;\Rightarrow\;m=\frac{M}{N_A}"),
            M(rf"m=\frac{{0{{,}}0280}}{{6{{,}}022\times 10^{{23}}}}={tex(m_molec, 4, u='kg')}"))\
        .di("Y el total de moléculas es moles por número de Avogadro.",
            M(rf"N=nN_A=({tex(n_mol, 5)})(6{{,}}022\times 10^{{23}})={tex(n_molec, 3)}"),
            R(rf"m={tex(m_molec, 3, u='kg')}\qquad N={tex(n_molec, 3)}\ \text{{moléculas}}", "rb"))

    L.paso("c) Rapidez cuadrática media por ambas formas")\
        .di("La hoja trae dos expresiones equivalentes para la rapidez cuadrática media: con erre y masa molar, o con Boltzmann y masa molecular.",
            F(r"v_{\text{rms}}=\sqrt{\frac{3k_B T}{m}}=\sqrt{\frac{3RT}{M}}"),
            M(rf"v_{{\text{{rms}}}}=\sqrt{{\frac{{3(8{{,}}314)({tex(tk, 5)})}}{{0{{,}}0280}}}}=\sqrt{{{tex(3 * R_GAS * tk / M_mol, 5, sci='no')}}}={tex(v_rms_M, 4)}\ \text{{m/s}}"))\
        .di("Comprobamos con la constante de Boltzmann de la hoja.",
            M(rf"v_{{\text{{rms}}}}=\sqrt{{\frac{{3(1{{,}}38\times 10^{{-23}})({tex(tk, 5)})}}{{{tex(m_molec, 4)}}}}}"),
            M(rf"v_{{\text{{rms}}}}={tex(v_rms_kb_vis, 4)}\ \text{{m/s}}"))\
        .di("Sale casi lo mismo. La décima de diferencia viene de que la hoja redondea la constante de Boltzmann a tres cifras. Con tres cifras, las dos formas dan lo mismo.",
            R(rf"v_{{\text{{rms}}}}={tex(v_rms_M, 3)}\ \text{{m/s}}", "rc"))

    L.paso("d) Energía cinética molecular y total")\
        .di("La energía cinética traslacional media por molécula sale directo de la casilla con la constante de Boltzmann.",
            F(r"\frac{1}{2}m(v^2)_{\text{med}}=\frac{3k_B T}{2}"),
            M(rf"K_{{\text{{med}}}}=\frac{{3}}{{2}}(1{{,}}38\times 10^{{-23}})({tex(tk, 5)})={tex(k_med, 3, u='J')}"),
            R(rf"K_{{\text{{med}}}}={tex(k_med, 3, u='J')}", "rd1"))\
        .di("Y la energía cinética traslacional total del gas sale de la casilla tres medios de ene erre te.",
            F(r"K=\frac{3}{2}nRT"),
            M(rf"K=\frac{{3}}{{2}}({tex(n_mol, 4)})(8{{,}}314)({tex(tk, 5)})={tex(k_tot, 4, u='J')}"),
            R(rf"K={tex(k_tot, 3, u='J')}", "rd2"),
            NOTA(f"Comprobación microscópica: $N K_{{\\text{{med}}}} = ({tex(n_molec, 3)})({tex(k_med, 3)}) \\approx {tex(k_tot, 3)}$ J."))

    L.responde("a", rf"n={tex(n_mol, 3)}\ \text{{mol}}", n_mol, "mol")\
     .responde("b", rf"m={tex(m_molec, 3, u='kg')}", m_molec, "kg")\
     .responde("c", rf"v_{{\text{{rms}}}}={tex(v_rms_M, 3)}\ \text{{m/s}}", v_rms_M, "m/s")\
     .responde("d", rf"K={tex(k_tot, 3, u='J')}", k_tot, "J")
    return L.usa(
        r"pV=nRT",
        r"m_{\text{total}}=nM",
        r"K=\frac{3}{2}nRT",
        r"v_{\text{rms}}=\sqrt{\frac{3k_B T}{m}}=\sqrt{\frac{3RT}{M}}",
        r"\frac{1}{2}m(v^2)_{\text{med}}=\frac{3k_B T}{2}",
        r"M=N_A m",
        r"T_K=T_{^\circ\text{C}}+273{,}15",
    )


# ============================================================================ P26-4
@leccion("P26-4")
def p26_4() -> Leccion:
    """Radiación neta + calorimetría: una esfera negra que se enfría (lo que el profe anunció: calorimetría o radiación)."""
    from .ayudas import SIGMA
    r, e, m, c = 0.0500, 0.900, 1.41, 900.0
    tc, tsc, t = 200.0, 25.0, 60.0
    A = 4.0 * math.pi * r ** 2
    T, Ts = tc + T0, tsc + T0
    pre = A * e * SIGMA
    He, Ha = pre * T ** 4, pre * Ts ** 4
    Hn = He - Ha
    Q = Hn * t
    dT = Q / (m * c)
    L = Leccion(
        "P26-4", "Radiación neta y calorimetría: una esfera negra que se enfría", tema="5.9", tipo="desarrollo", nivel=3,
        fuente=_fuente_b(4),
        enunciado=(
            "Una esfera de aluminio pintada de negro ($e=0{,}900$), de radio $5{,}00$ cm y masa $1{,}41$ kg "
            "($c=900$ J/(kg·K)), está a $200\\ ^\\circ$C dentro de una habitación cuyas paredes están a $25{,}0\\ ^\\circ$C. "
            "a) Área de la esfera. b) Potencia que emite. c) Potencia que absorbe de las paredes. d) Potencia neta. "
            "e) Si esa potencia neta se mantuviera, ¿cuánto bajaría la temperatura de la esfera en $1{,}00$ minuto?"),
        claves=["radiación neta", "Stefan-Boltzmann", "calorimetría", "enfriamiento", "lo que dijo el profe"],
        visual={"tipo": "radiacion", "T": round(T, 2), "Ts": round(Ts, 2), "e": e, "A": round(A, 5), "H": round(Hn, 1), "objeto": "esfera"},
    )
    L.paso("Qué nos dan y qué piden")\
        .di("Una esfera caliente en un cuarto más frío: pierde energía por radiación, y esa energía sale de su calor.",
            DATO("pintada de negro"),
            LISTA(["$r=0{,}0500$ m, $e=0{,}900$, $m=1{,}41$ kg, $c=900$ J/(kg·K)",
                   "Esfera a $200\\ ^\\circ$C; paredes a $25{,}0\\ ^\\circ$C",
                   "Tiempo: $1{,}00$ min $=60{,}0$ s"]))
    L.paso("a) Área de la esfera")\
        .di("El área de una esfera es cuatro pi por el radio al cuadrado. Es geometría de colegio.", M(r"A=4\pi r^2"))\
        .di("El radio en metros: cinco centímetros son cero coma cero cinco metros.", M(r"A=4\pi(0{,}0500)^2"))\
        .di(f"Da {nu(A, 'm2', 4)}.", R(rf"A={tex(A, 5)}\ \text{{m}}^2", "ra"))
    L.paso("b) Antes de elevar a la cuarta: kelvin")\
        .di("En radiación las temperaturas van siempre en kelvin. La hoja da la conversión.", F(r"T_K=T_{^\circ\text{C}}+273{,}15"))\
        .di("La esfera y las paredes.", M(r"T=200+273{,}15=473{,}15\ \text{K}"), M(r"T_s=25{,}0+273{,}15=298{,}15\ \text{K}"))\
        .di("Si se usan grados Celsius dentro de la cuarta potencia, el resultado sale completamente mal.",
            OJO("Nunca $(200)^4$: siempre $(473{,}15)^4$. Y nunca restar antes de elevar: $(473{,}15-298{,}15)^4$ está mal."))
    L.paso("b) Potencia que emite la esfera")\
        .di("La hoja da la potencia que emite un cuerpo a temperatura T.", F(r"H=Ae\sigma T^4"))\
        .di("Sustituimos el área, la emisividad, sigma de la hoja y la temperatura en kelvin.".replace("sigma", dice("σ", "sigma")),
            M(rf"H_{{\text{{emite}}}}=({tex(A, 5)})(0{{,}}900)(5{{,}}67\times 10^{{-8}})(473{{,}}15)^4"),
            NOTA("Elevar a la cuarta es elevar al cuadrado dos veces: $473{,}15^2=223\\,871$ y luego $223\\,871^2$."))\
        .di(f"Emite {nu(He, 'W', 4)}.", R(rf"H_{{\text{{emite}}}}={tex(He, 4)}\ \text{{W}}", "rb"))
    L.paso("c) Potencia que absorbe de las paredes")\
        .di("Las paredes también radian hacia la esfera, y la esfera absorbe con la misma emisividad. Misma fórmula, con la temperatura de las paredes.",
            M(rf"H_{{\text{{absorbe}}}}=Ae\sigma T_s^4=({tex(A, 5)})(0{{,}}900)(5{{,}}67\times 10^{{-8}})(298{{,}}15)^4"))\
        .di(f"Absorbe {nu(Ha, 'W', 4)}.", R(rf"H_{{\text{{absorbe}}}}={tex(Ha, 4)}\ \text{{W}}", "rc"))
    L.paso("d) Potencia neta")\
        .di("Lo que de verdad pierde es lo que emite menos lo que absorbe.", M(rf"H_{{\text{{neto}}}}={tex(He, 4)}-{tex(Ha, 4)}"))\
        .di(f"Pierde {nu(Hn, 'W', 4)} cada segundo.", R(rf"H_{{\text{{neto}}}}={tex(Hn, 4)}\ \text{{W}}", "rd"))\
        .di("Es exactamente la casilla de radiación neta de la hoja, con el área, la emisividad y sigma como factor común.".replace("sigma", dice("σ", "sigma")),
            F(r"H=Ae\sigma\left(T^4-T_s^4\right)"), M(r"Ae\sigma T^4-Ae\sigma T_s^4=Ae\sigma\left(T^4-T_s^4\right)"))
    L.paso("e) Energía que pierde en un minuto")\
        .di("Hache es energía por segundo. La hoja lo dice: hache es de Q entre de te.".replace("Hache es", dice("H", "Hache") + " es").replace("hache es de Q entre de te", dice("H = dQ/dt", "hache es de Q entre de te")),
            F(r"H=\frac{dQ}{dt}"))\
        .di("Si la potencia casi no cambia en un minuto, la energía es potencia por tiempo.", M(r"Q=H\,\Delta t"),
            M(rf"Q=({tex(Hn, 4)})(60{{,}}0)"))\
        .di(f"Pierde {nu(Q, 'J', 4)}.", R(rf"Q={tex(Q, 4)}\ \text{{J}}", "re1"))
    L.paso("e) Cuánto baja su temperatura")\
        .di("Esa energía sale del calor sensible de la esfera. Casilla de la hoja.", F(r"Q=mc\Delta T"))\
        .di("Despejamos delta te: dividimos los dos lados entre eme por ce.".replace("delta te", dice("ΔT", "delta te")).replace("eme por ce", dice("m·c", "eme por ce")),
            M(r"\Delta T=\frac{Q}{mc}"))\
        .di("Sustituimos.", M(rf"\Delta T=\frac{{{tex(Q, 4)}}}{{(1{{,}}41)(900)}}"))\
        .di(f"Baja unos {nu(dT, '°C', 3)} en un minuto.", R(rf"\Delta T={tex(dT, 3)}\ ^\circ\text{{C}}\ \text{{(se enfría)}}", "re2"),
            NOTA("Es una buena aproximación: en un minuto la temperatura cambia poco, así que la potencia neta casi no cambia."))
    L.responde("a", rf"A={tex(A, 5)}\ \text{{m}}^2", A, "m2")\
     .responde("b", rf"H_{{\text{{emite}}}}={tex(He, 4)}\ \text{{W}}", He, "W")\
     .responde("c", rf"H_{{\text{{absorbe}}}}={tex(Ha, 4)}\ \text{{W}}", Ha, "W")\
     .responde("d", rf"H_{{\text{{neto}}}}={tex(Hn, 4)}\ \text{{W}}", Hn, "W")\
     .responde("e", rf"\Delta T={tex(dT, 3)}\ ^\circ\text{{C}}", dT, "°C")
    return L.usa(r"H=Ae\sigma T^4", r"H=Ae\sigma(T^4-T_s^4)", r"T_K=T_{^\circ\text{C}}+273{,}15", r"H=\frac{dQ}{dt}", r"Q=mc\Delta T")


# ============================================================================ P26-5
@leccion("P26-5")
def p26_5() -> Leccion:
    """Radiación: despejar la temperatura (raíz cuarta) y fundir hielo con esa potencia."""
    from .ayudas import SIGMA
    r, lb, e, H = 0.0100, 0.300, 0.850, 1500.0
    tsc = 20.0
    mh, lf = 2.00, 3.34e5
    A = 2.0 * math.pi * r * lb
    pre = A * e * SIGMA
    x = H / pre
    Ts = tsc + T0
    ts4 = Ts ** 4
    t4 = x + ts4
    raiz2 = math.sqrt(t4)
    T = math.sqrt(raiz2)
    tcel = T - T0
    Q = mh * lf
    tiempo = Q / H
    L = Leccion(
        "P26-5", "Calentador radiante: despejar la temperatura y fundir hielo", tema="5.9", tipo="desarrollo", nivel=3,
        fuente=_fuente_b(5),
        enunciado=(
            "Un calentador radiante es una barra cilíndrica de $30{,}0$ cm de largo y $1{,}00$ cm de radio, con $e=0{,}850$; "
            "radia solo por su superficie lateral. Debe entregar una potencia neta de $1500$ W a una habitación cuyas paredes están a $20{,}0\\ ^\\circ$C. "
            "a) Área que radia. b) ¿A qué temperatura debe trabajar la barra? c) Si toda esa potencia se usara para fundir "
            "$2{,}00$ kg de hielo a $0\\ ^\\circ$C ($L_f=3{,}34\\times 10^5$ J/kg), ¿cuánto tiempo tardaría? "
            "d) Si la temperatura absoluta de la barra se duplicara, ¿por cuánto se multiplicaría la potencia que emite?"),
        claves=["radiación neta", "raíz cuarta", "despejar temperatura", "calor latente", "lo que dijo el profe"],
        visual={"tipo": "radiacion", "T": round(T, 1), "Ts": round(Ts, 2), "e": e, "A": round(A, 5), "H": H, "objeto": "calentador"},
    )
    L.paso("Qué nos dan y qué piden")\
        .di("Aquí la incógnita es la temperatura: hay que despejarla de la radiación neta.", DATO("potencia neta de 1500 W"),
            LISTA(["Barra: $L=0{,}300$ m, $r=0{,}0100$ m, $e=0{,}850$", "$H_{\\text{neto}}=1500$ W; paredes a $20{,}0\\ ^\\circ$C",
                   "Hielo: $m=2{,}00$ kg a $0\\ ^\\circ$C, $L_f=3{,}34\\times 10^5$ J/kg"]))
    L.paso("a) Área que radia: la superficie lateral")\
        .di("Si se desenrolla la superficie lateral de un cilindro, queda un rectángulo: de alto el largo, y de ancho la vuelta completa, dos pi erre.".replace("dos pi erre", dice("2πr", "dos pi erre")),
            M(r"A=(2\pi r)\,L"))\
        .di("Sustituimos en metros.", M(r"A=2\pi(0{,}0100)(0{,}300)"))\
        .di(f"Da {nu(A, 'm2', 4)}.", R(rf"A={tex(A, 4)}\ \text{{m}}^2", "ra"))
    L.paso("b) La fórmula de la hoja y qué se despeja")\
        .di("La potencia neta es la casilla de radiación neta. La incógnita es te, la temperatura de la barra.".replace("te, la", dice("T", "te") + ", la"),
            F(r"H=Ae\sigma\left(T^4-T_s^4\right)"))\
        .di("Las paredes, en kelvin.", F(r"T_K=T_{^\circ\text{C}}+273{,}15"), M(r"T_s=20{,}0+273{,}15=293{,}15\ \text{K}"))
    L.paso("b) Despejar T sin saltos")\
        .di("Primero dividimos los dos lados entre a, e, sigma: queda sola la resta de cuartas potencias.".replace("sigma", dice("σ", "sigma")),
            M(r"\frac{H}{Ae\sigma}=T^4-T_s^4"))\
        .di("Ahora pasamos te ese a la cuarta sumando al otro lado.".replace("te ese a la cuarta", dice("Tₛ⁴", "te ese a la cuarta")),
            M(r"T^4=\frac{H}{Ae\sigma}+T_s^4"))\
        .di("Y para quitar la cuarta potencia se saca raíz cuarta. Es lo mismo que raíz cuadrada dos veces.",
            M(r"T=\sqrt[4]{\frac{H}{Ae\sigma}+T_s^4}=\sqrt{\sqrt{\frac{H}{Ae\sigma}+T_s^4}}"))
    L.paso("b) Con números, una cuenta a la vez")\
        .di("Primero los tres factores de abajo.", M(rf"Ae\sigma=({tex(A, 4)})(0{{,}}850)(5{{,}}67\times 10^{{-8}})"))\
        .di(f"Dan {n(pre, 4)}.", M(rf"Ae\sigma={tex(pre, 4)}\ \text{{W/K}}^4"))\
        .di("Dividimos la potencia entre ese número.", M(rf"\frac{{H}}{{Ae\sigma}}=\frac{{1500}}{{{tex(pre, 4)}}}"))\
        .di(f"Da {n(x, 4)}.", M(rf"\frac{{H}}{{Ae\sigma}}={tex(x, 4)}\ \text{{K}}^4"))\
        .di("La temperatura de las paredes a la cuarta.", M(r"T_s^4=(293{,}15)^4"), M(rf"T_s^4={tex(ts4, 4)}\ \text{{K}}^4"))
    L.paso("b) Sumar y sacar la raíz cuarta")\
        .di("Sumamos.", M(rf"T^4={tex(x, 4)}+{tex(ts4, 4)}"), M(rf"T^4={tex(t4, 4)}\ \text{{K}}^4"))\
        .di("Primera raíz cuadrada.", M(rf"\sqrt{{{tex(t4, 4)}}}={tex(raiz2, 4)}"))\
        .di("Segunda raíz cuadrada: esa es la temperatura en kelvin.", M(rf"T=\sqrt{{{tex(raiz2, 4)}}}"), R(rf"T={tex(T, 4)}\ \text{{K}}", "rb1"))\
        .di("En grados Celsius restamos doscientos setenta y tres coma quince.", M(rf"T_{{^\circ\text{{C}}}}={tex(T, 4)}-273{{,}}15"),
            R(rf"T={tex(tcel, 3)}\ ^\circ\text{{C}}", "rb2"), NOTA("Al rojo vivo: así se ven los calentadores de barra."))
    L.paso("c) Tiempo para fundir el hielo")\
        .di("El hielo ya está a cero grados: solo hay que fundirlo. Calor latente de la hoja.", F(r"Q=\pm mL"),
            M(r"Q=(2{,}00)(3{,}34\times 10^5)"), M(rf"Q={tex(Q, 4)}\ \text{{J}}"))\
        .di("Hache es energía por segundo; despejamos el tiempo: dividimos la energía entre la potencia.".replace("Hache", dice("H", "Hache")),
            F(r"H=\frac{dQ}{dt}"), M(r"H=\frac{Q}{\Delta t}\;\Rightarrow\;\Delta t=\frac{Q}{H}"))\
        .di("Sustituimos.", M(rf"\Delta t=\frac{{{tex(Q, 4)}}}{{1500}}"))\
        .di(f"Son {nu(tiempo, 's', 4)}, unos siete minutos y medio.", R(rf"\Delta t={tex(tiempo, 4)}\ \text{{s}}={tex(tiempo / 60.0, 3)}\ \text{{min}}", "rc"))
    L.paso("d) Si la temperatura absoluta se duplica")\
        .di("La potencia que emite va con la cuarta potencia de la temperatura en kelvin.", F(r"H=Ae\sigma T^4"))\
        .di("Ponemos dos te en vez de te. El dos también se eleva a la cuarta.".replace("dos te", dice("2T", "dos te")).replace("de te.", "de " + dice("T", "te") + "."),
            M(r"H'=Ae\sigma\,(2T)^4=Ae\sigma\,(2^4)\,T^4"))\
        .di("Dos a la cuarta es dieciséis.", M(r"2^4=2\cdot 2\cdot 2\cdot 2=16"), R(r"H'=16\,H", "rd"))\
        .pregunta_rapida("Si la temperatura absoluta se triplica, la potencia emitida se multiplica por…", ["81", "9", "3"], 0,
                         "$3^4=81$: la cuarta potencia hace que la radiación crezca muchísimo con la temperatura.")
    L.responde("a", rf"A={tex(A, 4)}\ \text{{m}}^2", A, "m2")\
     .responde("b", rf"T={tex(T, 4)}\ \text{{K}}={tex(tcel, 3)}\ ^\circ\text{{C}}", T, "K")\
     .responde("c", rf"\Delta t={tex(tiempo, 4)}\ \text{{s}}", tiempo, "s")\
     .responde("d", r"H'=16\,H", 16.0, "")
    return L.usa(r"H=Ae\sigma(T^4-T_s^4)", r"H=Ae\sigma T^4", r"T_K=T_{^\circ\text{C}}+273{,}15", r"Q=\pm mL", r"H=\frac{dQ}{dt}")
