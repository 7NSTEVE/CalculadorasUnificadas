"""Superexamen Q106–Q116: ciclos de reposición y del folleto."""

from __future__ import annotations

import math

from .ayudas import ATM, R_GAS, Gas, explica_proceso, tabla_estados, tabla_quw, valores_visual, visual_pv
from .dsl import DATO, F, LISTA, M, NOTA, Leccion, R, TABLA, V
from .formato import n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado, **k)


# ============================================================================ Q106
@leccion("Q106")
def q106() -> Leccion:
    g = Gas(2.00, "dia")
    v1, v2, p1, p2 = 7.00e-2, 9.00e-2, 1.22e5, 2.03e5
    g.pone("B", p=p1, v=v1)
    g.pone("C", p=p2, v=v1)
    g.pone("D", p=p2, v=v2)
    g.pone("A", p=p1, v=v2)
    rbc = g.proceso("B", "C", "isocorico")
    rcd = g.proceso("C", "D", "isobarico")
    rda = g.proceso("D", "A", "isocorico")
    rab = g.proceso("A", "B", "isobarico")
    res = [("B→C", rbc), ("C→D", rcd), ("D→A", rda), ("A→B", rab)]
    wnet = sum(r["W"] for _, r in res)
    qh = rbc["Q"] + rcd["Q"]
    qc = rda["Q"] + rab["Q"]
    ef = wnet / qh
    L = _des("Q106", "Ciclo rectangular diatómico (2 mol)",
             "Dos moles de gas ideal diatómico recorren el rectángulo horario $B\\to C\\to D\\to A\\to B$ entre $V=7{,}00\\times 10^{-2}$ y $9{,}00\\times 10^{-2}\\ \\text{m}^3$, y $p=1{,}22\\times 10^5$ y $2{,}03\\times 10^5$ Pa. A) Trabajo neto. B) $Q_H$ y $Q_C$. C) Eficiencia.",
             claves=["ciclo rectangular", "diatómico", "eficiencia"],
             visual=visual_pv(g, [("B", "C", "isocorico"), ("C", "D", "isobarico"), ("D", "A", "isocorico"), ("A", "B", "isobarico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("A) Trabajo neto como área del rectángulo")\
        .di(f"El trabajo neto de un ciclo rectangular horario es simplemente base por altura: {nu(wnet, 'J', 4)}.", V(foco="ciclo"), DATO("ciclo mostrado"),
            M(r"W_{\text{neto}} = (p_2-p_1)(V_2-V_1) = (2{,}03-1{,}22)\times 10^5(9{,}00-7{,}00)\times 10^{-2}"),
            R(rf"W_{{\text{{neto}}}} = {tex(wnet, 4, u='J')}", "ra"))
    L.paso("B y C) Calores absorbido y cedido, y eficiencia")\
        .di("Como es diatómico, ce ve vale cinco medios de erre y ce pe siete medios de erre; entra calor al subir y expandir, sale al bajar y comprimir.",
            tabla_quw(res),
            M(rf"Q_H = Q_{{BC}}+Q_{{CD}} = {tex(qh, 5, u='J', sci='no')}\qquad |Q_C| = |Q_{{DA}}+Q_{{AB}}| = {tex(abs(qc), 5, u='J', sci='no')}"))\
        .di(f"La eficiencia es trabajo neto entre calor absorbido: {n(ef * 100, 3)} por ciento.",
            R(rf"e = \frac{{{tex(wnet, 4)}}}{{{tex(qh, 5, sci='no')}}} = {tex(ef * 100, 3)}\,\%\ \ (\approx 6\,\%)", "rbc"))
    L.responde("A", rf"W_{{\text{{neto}}}}={tex(wnet, 4)}\ \text{{J}}", wnet, "J")\
     .responde("QH", rf"Q_H={tex(qh, 5, sci='no')}\ \text{{J}}", qh, "J")\
     .responde("QC", rf"|Q_C|={tex(abs(qc), 5, sci='no')}\ \text{{J}}", abs(qc), "J")\
     .responde("e", rf"e={tex(ef, 3)}", ef, "")
    return L.oficial("Reposición II-2023", 1620.0, 28381.0, 26762.0, 0.0571).usa(r"W_{\text{neto}}=\Delta p\,\Delta V", r"e=\frac{W}{Q_H}")


# ============================================================================ Q107
@leccion("Q107")
def q107() -> Leccion:
    g = Gas(1.0, "dia")
    g.pone("1", p=1.0, v=1.0)
    g.pone("2", p=2.0, v=1.0)
    g.pone("3", p=2.0, v=2.0)
    g.pone("4", p=1.0, v=2.0)
    ef = 2.0 / 19.0
    L = _des("Q107", "Ciclo rectangular simbólico (p₀, V₀) con gas diatómico",
             "Un gas ideal diatómico recorre el ciclo rectangular $1(V_0,p_0)\\to 2(V_0,2p_0)\\to 3(2V_0,2p_0)\\to 4(2V_0,p_0)\\to 1$. A) Trabajo y calor por proceso en términos de $p_0 V_0$. B) $Q_C$ y $Q_H$. C) Eficiencia.",
             claves=["ciclo simbólico", "p0 V0", "diatómico"],
             visual=visual_pv(g, [("1", "2", "isocorico"), ("2", "3", "isobarico"), ("3", "4", "isocorico"), ("4", "1", "isobarico")],
                              uv="V₀", up="p₀"))
    L.paso("A) Trabajo y calor por etapa en función de p₀V₀")\
        .di("Como ene erre delta te es delta de pe por ve, sustituimos ce ve igual a cinco medios y ce pe siete medios.", DATO("p0 y V0"),
            TABLA(["Etapa", "$W$", "$\\Delta U = \\tfrac{5}{2}\\Delta(pV)$", "$Q = \\Delta U + W$"],
                  [["$1\\to 2$ (isoc.)", "$0$", "$+\\tfrac{5}{2}p_0V_0$", "$+\\tfrac{5}{2}p_0V_0$"],
                   ["$2\\to 3$ (isob.)", "$+2p_0V_0$", "$+5p_0V_0$", "$+7p_0V_0$"],
                   ["$3\\to 4$ (isoc.)", "$0$", "$-5p_0V_0$", "$-5p_0V_0$"],
                   ["$4\\to 1$ (isob.)", "$-p_0V_0$", "$-\\tfrac{5}{2}p_0V_0$", "$-\\tfrac{7}{2}p_0V_0$"]], resaltar=[[1, 3], [0, 3]]))
    L.paso("B y C) Calores totales y eficiencia")\
        .di("Sumamos los calores positivos para cu hache y los negativos para cu ce.", V(foco="ciclo"),
            M(r"Q_H = \frac{5}{2}p_0V_0 + 7p_0V_0 = \frac{19}{2}\,p_0V_0 = 9{,}5\,p_0V_0"),
            M(r"Q_C = -5p_0V_0 - \frac{7}{2}p_0V_0 = -\frac{17}{2}\,p_0V_0 = -8{,}5\,p_0V_0"))\
        .di(f"El trabajo neto es un pe cero ve cero, así que la eficiencia da dos diecinueveavos, {n(ef * 100, 3)} por ciento.",
            R(rf"e = \frac{{p_0V_0}}{{\frac{{19}}{{2}}p_0V_0}} = \frac{{2}}{{19}} \approx {tex(ef * 100, 3)}\,\%", "rc"))
    L.responde("QH", r"\frac{19}{2}p_0V_0", 9.5, "p0V0").responde("QC", r"-\frac{17}{2}p_0V_0", -8.5, "p0V0").responde("e", r"\frac{2}{19}", ef, "")
    return L.usa(r"pV=nRT", r"e=\frac{W}{Q_H}")


# ============================================================================ Q108
@leccion("Q108")
def q108() -> Leccion:
    g = Gas(1.00, "mono")
    g.pone("A", p=2.0e5, v=0.0020)
    g.pone("B", p=2.0e5, v=0.0060)
    g.pone("C", p=1.0e5, v=0.0045)
    rab = g.proceso("A", "B", "isobarico")
    rbc = g.proceso("B", "C", "recta")
    rca = g.proceso("C", "A", "recta")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    wnet = rab["W"] + rbc["W"] + rca["W"]
    qh = rab["Q"]
    ef = wnet / qh
    ds_ab = g.n * g.cp * math.log(g.t("B") / g.t("A"))
    L = _des("Q108", "Ciclo triangular de 1 mol monoatómico (trabajo, Q_H, e y ΔS)",
             "Un mol de gas monoatómico recorre el triángulo horario $A(0{,}0020\\ \\text{m}^3, 2{,}0\\times 10^5\\ \\text{Pa})\\to B(0{,}0060, 2{,}0\\times 10^5)\\to C(0{,}0045, 1{,}0\\times 10^5)\\to A$. A) Trabajo por proceso. B) Calor entrante $Q_H$. C) Eficiencia. D) $\\Delta S_{A\\to B}$.",
             claves=["triángulo", "monoatómico", "trapecio"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "recta"), ("C", "A", "recta")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("A) Trabajo de los tres procesos")\
        .di("De A a B es rectángulo; de B a C y de C a A son trapecios negativos porque el volumen baja.", V(foco="AB"), DATO("tres veces mayor"),
            M(rf"W_{{AB}} = (2{{,}}0\times 10^5)(0{{,}}0060-0{{,}}0020) = +{tex(rab['W'], 3, u='J')}"),
            M(rf"W_{{BC}} = \frac{{(2{{,}}0+1{{,}}0)\times 10^5}}{{2}}(0{{,}}0045-0{{,}}0060) = {tex(rbc['W'], 3, u='J')}"),
            M(rf"W_{{CA}} = \frac{{(1{{,}}0+2{{,}}0)\times 10^5}}{{2}}(0{{,}}0020-0{{,}}0045) = {tex(rca['W'], 3, u='J')}"))
    L.paso("B, C y D) Calor entrante, eficiencia y entropía A→B")\
        .di("Solo entra calor en la expansión isobárica A a B; en los otros dos tramos el calor sale.", V(foco="ciclo"),
            tabla_quw(res),
            R(rf"Q_H = Q_{{AB}} = {tex(qh, 4, u='J')}\qquad e = \frac{{{tex(wnet, 3)}}}{{{tex(qh, 4)}}} = {tex(ef * 100, 3)}\,\%", "rbc"))\
        .di(f"Como el volumen se triplica a presión constante, la temperatura se triplica y la entropía sube {nu(ds_ab, 'J/K', 3)}.",
            M(rf"\Delta S_{{AB}} = nC_p\ln\frac{{T_B}}{{T_A}} = \frac{{5}}{{2}}(8{{,}}314)\ln 3"),
            R(rf"\Delta S_{{AB}} = {tex(ds_ab, 3, u='J/K')}", "rd"))
    L.responde("WAB", rf"{tex(rab['W'], 3)}\ \text{{J}}", rab["W"], "J")\
     .responde("WBC", rf"{tex(rbc['W'], 3)}\ \text{{J}}", rbc["W"], "J")\
     .responde("WCA", rf"{tex(rca['W'], 3)}\ \text{{J}}", rca["W"], "J")\
     .responde("QH", rf"{tex(qh, 4)}\ \text{{J}}", qh, "J")\
     .responde("e", rf"{tex(ef, 3)}", ef, "")\
     .responde("dS", rf"{tex(ds_ab, 3)}\ \text{{J/K}}", ds_ab, "J/K")
    return L.oficial("Reposición II-2024", 800.0, -225.0, -375.0, 1999.5, 0.10, 22.8).usa(r"W=\frac{p_1+p_2}{2}\Delta V", r"e=\frac{W}{Q_H}")


# ============================================================================ Q109
@leccion("Q109")
def q109() -> Leccion:
    g = Gas(0.378, "dia")
    g.pone("A", p=4.30 * ATM, v=9.00e-3)
    g.pone("C", p=4.30 * ATM, v=3.00e-3)
    pb = g.p("C") * (g.v("C") / g.v("A")) ** g.g
    g.pone("B", p=pb, v=9.00e-3)
    rab = g.proceso("A", "B", "isocorico")
    rbc = g.proceso("B", "C", "adiabatico")
    rca = g.proceso("C", "A", "isobarico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    wnet = rab["W"] + rbc["W"] + rca["W"]
    ef = wnet / rca["Q"]
    L = _des("Q109", "Ciclo de 0,378 mol diatómico (isocórico, adiabático, isobárico)",
             "Un gas diatómico ($0{,}378$ mol) parte en $A$ ($4{,}30$ atm, $9{,}00$ L): $A\\to B$ enfriamiento isocórico; $B\\to C$ compresión adiabática a un tercio del volumen ($3{,}00$ L); $C\\to A$ expansión isobárica. A) Diagrama. B) Tabla $(p,V,T)$. C) $Q$ y $W$. D) Eficiencia.",
             claves=["diatómico", "adiabático", "eficiencia"],
             visual=visual_pv(g, [("A", "B", "isocorico"), ("B", "C", "adiabatico"), ("C", "A", "isobarico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("A y B) Estados del ciclo")\
        .di("Como C a A es isobárico, pe C vale cuatro coma treinta atmósferas; con la curva adiabática B a C sacamos pe B.", DATO("un tercio del volumen"),
            M(rf"p_B = p_C\left(\frac{{V_C}}{{V_B}}\right)^{{1{{,}}4}} = 4{{,}}30\left(\frac{{3}}{{9}}\right)^{{1{{,}}4}} = {tex(pb / ATM, 3, u='atm')}"),
            tabla_estados(g, ["A", "B", "C"], up="atm", uv="L", fp=1.0 / ATM, fv=1e3, cp=3, cv=3, dt=0))
    explica_proceso(L.paso("C1) Enfriamiento isocórico A→B"), g, "A", "B", "isocorico", foco="AB", dec_T=0)
    explica_proceso(L.paso("C2) Compresión adiabática B→C"), g, "B", "C", "adiabatico", foco="BC", dec_T=0)
    explica_proceso(L.paso("C3) Expansión isobárica C→A"), g, "C", "A", "isobarico", foco="CA", dec_T=0)
    L.paso("D) Eficiencia del motor")\
        .di(f"El trabajo neto es positivo ({nu(wnet, 'J', 4)}), así que funciona como motor con eficiencia de {n(ef * 100, 3)} por ciento.", V(foco="ciclo"),
            tabla_quw(res),
            R(rf"e = \frac{{{tex(wnet, 4)}}}{{{tex(rca['Q'], 4)}}} = {tex(ef * 100, 3)}\,\%", "rd"))
    L.responde("TA", rf"T_A={tex(g.t('A'), dec=0)}\ \text{{K}}", g.t("A"), "K")\
     .responde("TB", rf"T_B={tex(g.t('B'), dec=0)}\ \text{{K}}", g.t("B"), "K")\
     .responde("TC", rf"T_C={tex(g.t('C'), dec=0)}\ \text{{K}}", g.t("C"), "K")\
     .responde("e", rf"e={tex(ef, 3)}", ef, "")
    return L.oficial("Reposición II-2025", 1248.0, 268.0, 416.0, 0.158).usa(r"pV^\gamma=\text{cte}", r"e=\frac{W}{Q_H}")


# ============================================================================ Q110
@leccion("Q110")
def q110() -> Leccion:
    g = Gas(2.50, "mono")
    g.pone("A", v=1.26, t=300.0)
    pb = 5.0 * g.p("A")
    vb = g.v("A") * (1.0 / 5.0) ** (1.0 / g.g)
    g.pone("B", p=pb, v=vb)
    g.pone("C", p=g.p("A"), v=vb)
    rab = g.proceso("A", "B", "adiabatico")
    rbc = g.proceso("B", "C", "isocorico")
    rca = g.proceso("C", "A", "isobarico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    wnet = rab["W"] + rbc["W"] + rca["W"]
    K = rca["Q"] / abs(wnet)
    L = _des("Q110", "Refrigerador de 2,50 mol de helio (adiabática, isocórica, isobárica)",
             "Con $2{,}50$ mol de helio (monoatómico) desde $A$ ($1{,}26\\ \\text{m}^3$, $300$ K): $A\\to B$ compresión adiabática hasta $p_B=5p_A$; $B\\to C$ enfriamiento isocórico hasta $p_A$; $C\\to A$ expansión isobárica. A) Diagrama. B) Tabla SI. C) $W$ y $Q$. D) Rendimiento $K$.",
             claves=["helio", "refrigerador", "adiabático"],
             visual=visual_pv(g, [("A", "B", "adiabatico"), ("B", "C", "isocorico"), ("C", "A", "isobarico")],
                              uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("A y B) Tabla de estados en SI")\
        .di("Hallamos pe A por gas ideal y despejamos ve B de la adiabática con gama igual a cinco tercios.", DATO("cinco veces"),
            M(rf"V_B = V_A\left(\frac{{1}}{{5}}\right)^{{3/5}} = {tex(vb, 4, u='m^3')}"),
            tabla_estados(g, ["A", "B", "C"], up="Pa", uv="m³", cp=5, cv=4, dt=1))
    explica_proceso(L.paso("C1) Compresión adiabática A→B"), g, "A", "B", "adiabatico", foco="AB")
    explica_proceso(L.paso("C2) Enfriamiento isocórico B→C"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("C3) Expansión isobárica C→A"), g, "C", "A", "isobarico", foco="CA")
    L.paso("D) Coeficiente de rendimiento del refrigerador")\
        .di(f"El calor absorbido del foco frío es el del tramo C a A; dividiendo entre el trabajo neto sale {n(K, 4)}.", V(foco="ciclo"),
            tabla_quw(res),
            F(r"K = \frac{|Q_C|}{|W_{\text{neto}}|}"),
            R(rf"K = \frac{{{tex(rca['Q'], 4)}}}{{{tex(abs(wnet), 4)}}} = {tex(K, 4)}", "rd"))
    L.responde("WAB", rf"W_{{AB}}={tex(rab['W'], 4)}\ \text{{J}}", rab["W"], "J")\
     .responde("QBC", rf"Q_{{BC}}={tex(rbc['Q'], 5, sci='no')}\ \text{{J}}", rbc["Q"], "J")\
     .responde("QCA", rf"Q_{{CA}}={tex(rca['Q'], 4)}\ \text{{J}}", rca["Q"], "J")\
     .responde("K", rf"K={tex(K, 4)}", K, "")
    return L.oficial("Reposición I-2026", -8449.0, -14248.0, 9660.0, 2.106).usa(r"pV^\gamma=\text{cte}", r"K=\frac{|Q_C|}{|W|}")


# ============================================================================ Q111
@leccion("Q111")
def q111() -> Leccion:
    g = Gas(1.00, "dia")
    g.pone("1", p=1.50 * ATM, v=3.50e-3)
    g.pone("2", p=3.00 * ATM, v=3.50e-3)
    g.pone("3", p=2.00 * ATM, t=g.t("2"))
    g.pone("4", p=2.00 * ATM, t=256.0)
    # 4->5 isotermico (T5=256) y 5->1 adiabatico: T5 V5^(g-1) = T1 V1^(g-1)
    v5 = g.v("1") * (g.t("1") / 256.0) ** (1.0 / (g.g - 1.0))
    g.pone("5", v=v5, t=256.0)
    r12 = g.proceso("1", "2", "isocorico")
    r23 = g.proceso("2", "3", "isotermico")
    r34 = g.proceso("3", "4", "isobarico")
    r45 = g.proceso("4", "5", "isotermico")
    r51 = g.proceso("5", "1", "adiabatico")
    res = [("1→2", r12), ("2→3", r23), ("3→4", r34), ("4→5", r45), ("5→1", r51)]
    L = _des("Q111", "Ciclo de cinco etapas (1 mol diatómico)",
             "Un mol de gas diatómico parte en $1$ ($1{,}50$ atm, $3{,}50$ L): $1\\to 2$ isocórico a $3{,}00$ atm; $2\\to 3$ isotérmico a $2{,}00$ atm; $3\\to 4$ isobárico a $256$ K; $4\\to 5$ isotérmico; $5\\to 1$ adiabático. (a) Estados $(p,V,T)$. (b) $\\Delta U, W, Q$ por etapa.",
             claves=["cinco etapas", "adiabático", "folleto"],
             visual=visual_pv(g, [("1", "2", "isocorico"), ("2", "3", "isotermico"), ("3", "4", "isobarico"), ("4", "5", "isotermico"), ("5", "1", "adiabatico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("(a) Estados del ciclo de cinco puntos")\
        .di("Hallamos los cuatro primeros estados con la ley de gases ideales y el quinto enlazando la isoterma de cuatro con la adiabática hacia uno.", DATO("3.50 L"),
            M(rf"V_5 = V_1\left(\frac{{T_1}}{{T_5}}\right)^{{1/(\gamma-1)}} = 3{{,}}50\left(\frac{{64}}{{256}}\right)^{{2{{,}}5}} = {tex(v5 * 1e3, 3, u='L')}"),
            tabla_estados(g, ["1", "2", "3", "4", "5"], up="×10⁵ Pa", uv="L", fp=1e-5, fv=1e3, cp=4, cv=3, dt=0))
    L.paso("(b) Energía interna, trabajo y calor de cada proceso")\
        .di("Calculamos las tres cantidades en cada etapa y comprobamos que la suma de delta U cierra en cero.", V(foco="ciclo"),
            tabla_quw(res))
    L.responde("Q12", rf"Q_{{12}}={tex(r12['Q'], 4)}\ \text{{J}}", r12["Q"], "J")\
     .responde("W23", rf"W_{{23}}={tex(r23['W'], 4)}\ \text{{J}}", r23["W"], "J")\
     .responde("Q34", rf"Q_{{34}}={tex(r34['Q'], 4)}\ \text{{J}}", r34["Q"], "J")\
     .responde("W45", rf"W_{{45}}={tex(r45['W'], 4)}\ \text{{J}}", r45["W"], "J")\
     .responde("W51", rf"W_{{51}}={tex(r51['W'], 4)}\ \text{{J}}", r51["W"], "J")
    return L.oficial("Folleto resuelto 4.1-1", 1330.0, 431.5, 3724.0, -9722.0, 3990.0).usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}")


# ============================================================================ Q112
@leccion("Q112")
def q112() -> Leccion:
    g = Gas(50.0, "mono")
    g.pone("a", p=3.00e5, v=0.500)
    g.pone("b", p=3.00e5, v=0.800)
    g.pone("c", p=1.00e5, v=0.800)
    rab = g.proceso("a", "b", "isobarico")
    rbc = g.proceso("b", "c", "isocorico")
    rca = g.proceso("c", "a", "recta")
    res = [("a→b", rab), ("b→c", rbc), ("c→a", rca)]
    wnet = rab["W"] + rbc["W"] + rca["W"]
    qh = rab["Q"] + rca["Q"]
    ef = wnet / qh
    L = _des("Q112", "Ciclo triangular de 50 mol monoatómico",
             "Cincuenta moles de gas ideal monoatómico recorren el triángulo $a(0{,}500\\ \\text{m}^3, 3{,}0\\times 10^5\\ \\text{Pa})\\to b(0{,}800, 3{,}0\\times 10^5)\\to c(0{,}800, 1{,}0\\times 10^5)\\to a$ (recta). (a) Calor de cada etapa. (b) Eficiencia.",
             claves=["triángulo", "50 mol", "folleto"],
             visual=visual_pv(g, [("a", "b", "isobarico"), ("b", "c", "isocorico"), ("c", "a", "recta")],
                              uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("(a) Estados y calores en cada etapa")\
        .di("Hallamos las temperaturas y aplicamos las fórmulas de isobárico, isocórico y recta.", DATO("Cincuenta moles"),
            tabla_estados(g, ["a", "b", "c"], up="kPa", uv="m³", fp=1e-3, cp=3, cv=3, dt=0),
            tabla_quw(res, u="kJ", f=1e-3))\
        .di("Absorbe calor en a hacia b y en c hacia a; cede calor en b hacia c.",
            R(rf"Q_{{ab}}=+{tex(rab['Q'] / 1e3, 3, u='kJ')}\quad Q_{{bc}}={tex(rbc['Q'] / 1e3, 3, u='kJ')}\quad Q_{{ca}}=+{tex(rca['Q'] / 1e3, 2, u='kJ')}", "ra"))
    L.paso("(b) Eficiencia del ciclo")\
        .di(f"Sumamos los dos calores entrantes y hallamos la eficiencia: {n(ef * 100, 3)} por ciento.", V(foco="ciclo"),
            F(r"e = 1 - \frac{|Q_C|}{Q_H} = 1 - \frac{240}{225 + 45}"),
            R(rf"e = {tex(ef, 3)} = {tex(ef * 100, 3)}\,\%", "rb"))
    L.responde("Qab", rf"Q_{{ab}}={tex(rab['Q'] / 1e3, 3)}\ \text{{kJ}}", rab["Q"], "J")\
     .responde("Qbc", rf"Q_{{bc}}={tex(rbc['Q'] / 1e3, 3)}\ \text{{kJ}}", rbc["Q"], "J")\
     .responde("Qca", rf"Q_{{ca}}={tex(rca['Q'] / 1e3, 2)}\ \text{{kJ}}", rca["Q"], "J")\
     .responde("e", rf"e={tex(ef, 3)}", ef, "")
    return L.oficial("Folleto resuelto 4.1-2", 226e3, -240e3, 45e3, 0.111).usa(r"W=\frac{p_1+p_2}{2}\Delta V", r"e=1-\frac{|Q_C|}{Q_H}")


# ============================================================================ Q113
@leccion("Q113")
def q113() -> Leccion:
    g = Gas(1.00, "dia")
    g.pone("A", v=0.024, t=900.0)
    g.pone("B", v=0.224, t=900.0)
    pc = g.p("A") * (0.024 / 0.224) ** g.g
    g.pone("C", p=pc, v=0.224)
    rab = g.proceso("A", "B", "isotermico")
    rbc = g.proceso("B", "C", "isocorico")
    rca = g.proceso("C", "A", "adiabatico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    ef = (rab["Q"] + rbc["Q"]) / rab["Q"]
    ds_ab = rab["Q"] / g.t("A")
    ds_bc = g.n * g.cv * math.log(g.t("C") / g.t("B"))
    L = _des("Q113", "Ciclo isotérmico-isocórico-adiabático (1 mol diatómico)",
             "Un mol de gas diatómico recorre $A\\to B$ isotérmico ($V_A=0{,}024\\ \\text{m}^3$, $V_B=0{,}224\\ \\text{m}^3$, $T_A=900$ K), $B\\to C$ isocórico y $C\\to A$ adiabático. (a) Estados y diagrama $p$-$V$. (b) $\\Delta U, W, Q$. (c) Eficiencia. (d) $\\Delta S$ por etapa.",
             claves=["isotérmico", "adiabático", "entropía"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "isocorico"), ("C", "A", "adiabatico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("(a) Presiones, volúmenes y temperaturas")\
        .di("Hallamos pe A y pe B por gas ideal, y pe C con la relación adiabática hacia A.", DATO("TA = 900 K"),
            M(rf"p_C = p_A\left(\frac{{V_A}}{{V_C}}\right)^{{1{{,}}40}} = {tex(pc / 1e5, 3)}\times 10^5\ \text{{Pa}}"),
            tabla_estados(g, ["A", "B", "C"], up="×10⁵ Pa", uv="m³", fp=1e-5, cp=3, cv=3, dt=0))
    L.paso("(b) y (c) Energía, trabajo, calor y eficiencia")\
        .di(f"Entran {nu(rab['Q'] / 1e3, 'kJ', 3)} en la isoterma y salen {nu(abs(rbc['Q']) / 1e3, 'kJ', 3)} en la isocora; la eficiencia es {n(ef * 100, 2)} por ciento.", V(foco="ciclo"),
            tabla_quw(res, u="kJ", f=1e-3),
            R(rf"e = 1 - \frac{{{tex(abs(rbc['Q']) / 1e3, 3)}}}{{{tex(rab['Q'] / 1e3, 3)}}} = {tex(ef * 100, 2)}\,\%", "rc"))
    L.paso("(d) Cambio de entropía en cada etapa")\
        .di("La entropía que entra en la isoterma es exactamente la que sale en la isocora, y en la adiabática es cero.",
            R(rf"\Delta S_{{AB}}=+{tex(ds_ab, 3, u='J/K')}\quad \Delta S_{{BC}}={tex(ds_bc, 3, u='J/K')}\quad \Delta S_{{CA}}=0", "rd"))
    L.responde("QAB", rf"Q_{{AB}}={tex(rab['Q'] / 1e3, 3)}\ \text{{kJ}}", rab["Q"], "J")\
     .responde("QBC", rf"Q_{{BC}}={tex(rbc['Q'] / 1e3, 3)}\ \text{{kJ}}", rbc["Q"], "J")\
     .responde("e", rf"e={tex(ef, 2)}", ef, "")\
     .responde("dSAB", rf"\Delta S_{{AB}}={tex(ds_ab, 3)}\ \text{{J/K}}", ds_ab, "J/K")
    return L.oficial("Folleto resuelto 4.1-3", 16700.0, -11000.0, 0.34, 18.6).usa(r"pV^\gamma=\text{cte}", r"e=1-\frac{|Q_C|}{Q_H}")


# ============================================================================ Q114
@leccion("Q114")
def q114() -> Leccion:
    g = Gas(0.400, "dia")
    g.pone("1", p=2.00e5, t=300.0)
    g.pone("2", p=2.00e5, v=0.5 * g.v("1"))
    p3 = g.p("2") * (g.v("2") / g.v("1")) ** g.g
    g.pone("3", p=p3, v=g.v("1"))
    r12 = g.proceso("1", "2", "isobarico")
    r23 = g.proceso("2", "3", "adiabatico")
    r31 = g.proceso("3", "1", "isocorico")
    res = [("1→2", r12), ("2→3", r23), ("3→1", r31)]
    L = _des("Q114", "Ciclo de 0,400 mol de nitrógeno (isobárica, adiabática, isocórica)",
             "Un cilindro contiene $0{,}400$ mol de $\\text{N}_2$ (diatómico) a $2{,}0\\times 10^5$ Pa y $300$ K: compresión isobárica a la mitad del volumen, expansión adiabática al volumen inicial y calentamiento isocórico a la presión inicial. (a) Estados $(p,V)$. (b) Calor de cada etapa.",
             claves=["nitrógeno", "isobárica", "adiabática"],
             visual=visual_pv(g, [("1", "2", "isobarico"), ("2", "3", "adiabatico"), ("3", "1", "isocorico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("(a) Estados principales del ciclo")\
        .di("El volumen inicial es cinco litros, baja a dos coma cinco litros y la expansión adiabática baja la presión a cero coma setenta y seis por diez a la cinco pascales.", DATO("mitad de su volumen"),
            M(rf"p_3 = (2{{,}}0\times 10^5)(0{{,}}5)^{{1{{,}}4}} = {tex(p3 / 1e5, 3)}\times 10^5\ \text{{Pa}}"),
            tabla_estados(g, ["1", "2", "3"], up="×10⁵ Pa", uv="L", fp=1e-5, fv=1e3, cp=3, cv=3, dt=1))
    L.paso("(b) Calor en cada etapa")\
        .di(f"Emite {nu(abs(r12['Q']), 'J', 4)} en la compresión isobárica, cero en la adiabática y absorbe {nu(r31['Q'], 'J', 4)} en el calentamiento isocórico.", V(foco="ciclo"),
            tabla_quw(res),
            R(rf"Q_{{12}}={tex(r12['Q'], 4, u='J')}\ (\text{{emite}})\quad Q_{{23}}=0\quad Q_{{31}}=+{tex(r31['Q'], 4, u='J')}\ (\text{{absorbe}})", "rb"))
    L.responde("Q12", rf"Q_{{12}}={tex(r12['Q'], 4)}\ \text{{J}}", r12["Q"], "J")\
     .responde("Q31", rf"Q_{{31}}={tex(r31['Q'], 4)}\ \text{{J}}", r31["Q"], "J")
    return L.oficial("Folleto propuesto 4.2-1", -1746.0, 1544.0).usa(r"pV^\gamma=\text{cte}", r"Q=nC_p\Delta T")


# ============================================================================ Q115
@leccion("Q115")
def q115() -> Leccion:
    g = Gas(2.00, "mono")
    g.pone("a", v=1.0, t=293.0)
    g.pone("b", p=g.p("a"), t=879.0)
    g.pone("c", v=g.v("b"), t=700.0)
    vd = g.v("c") * (700.0 / 293.0) ** (1.0 / (g.g - 1.0))
    g.pone("d", v=vd, t=293.0)
    rab = g.proceso("a", "b", "isobarico")
    rbc = g.proceso("b", "c", "isocorico")
    rcd = g.proceso("c", "d", "adiabatico")
    rda = g.proceso("d", "a", "isotermico")
    res = [("a→b", rab), ("b→c", rbc), ("c→d", rcd), ("d→a", rda)]
    ds_ab = g.n * g.cp * math.log(879.0 / 293.0)
    ds_bc = g.n * g.cv * math.log(700.0 / 879.0)
    ds_da = rda["Q"] / 293.0
    L = _des("Q115", "Ciclo de cuatro procesos con 2 mol de gas monoatómico",
             "Dos moles de gas monoatómico siguen $a\\to b$ isobárico ($V_a=1\\ \\text{m}^3$, $T_a=293$ K, $T_b=879$ K), $b\\to c$ isocórico ($T_c=700$ K), $c\\to d$ adiabático y $d\\to a$ isotérmico. (a) $\\Delta U, Q, W$ por proceso y del ciclo. (b) $\\Delta S$ por proceso.",
             claves=["cuatro procesos", "monoatómico", "entropía"],
             visual=visual_pv(g, [("a", "b", "isobarico"), ("b", "c", "isocorico"), ("c", "d", "adiabatico"), ("d", "a", "isotermico")],
                              uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("(a) Estados, energía interna, trabajo y calor")\
        .di("Como te be es el triple de te a, ve be vale tres metros cúbicos; con la adiabática sacamos ve de.", DATO("Ta = 293 K"),
            tabla_estados(g, ["a", "b", "c", "d"], up="kPa", uv="m³", fp=1e-3, cp=4, cv=3, dt=0),
            tabla_quw(res))
    L.paso("(b) Cambio de entropía en cada proceso")\
        .di("Sumando los cuatro cambios de entropía comprobamos que el ciclo reversible cierra en cero.", V(foco="ciclo"),
            R(rf"\Delta S_{{ab}}=+{tex(ds_ab, 3, u='J/K')}\quad \Delta S_{{bc}}={tex(ds_bc, 3, u='J/K')}\quad \Delta S_{{cd}}=0\quad \Delta S_{{da}}={tex(ds_da, 3, u='J/K')}", "rb"))
    L.responde("Qab", rf"{tex(rab['Q'], 5, sci='no')}\ \text{{J}}", rab["Q"], "J")\
     .responde("Qbc", rf"{tex(rbc['Q'], 4)}\ \text{{J}}", rbc["Q"], "J")\
     .responde("Wcd", rf"{tex(rcd['W'], 5, sci='no')}\ \text{{J}}", rcd["W"], "J")\
     .responde("Wda", rf"{tex(rda['W'], 5, sci='no')}\ \text{{J}}", rda["W"], "J")
    return L.oficial("Folleto propuesto 4.2-2", 24360.0, -4465.0, 10151.0, -11727.0).usa(r"TV^{\gamma-1}=\text{cte}", r"\Delta S=nC\ln\frac{T_2}{T_1}")


# ============================================================================ Q116
@leccion("Q116")
def q116() -> Leccion:
    g = Gas(3.00, "dia")
    # a->b isobarico: Va=1.0, Vb=1.5, Tb=700 => Ta = 700/1.5 = 466.67 K
    ta = 700.0 * (1.0 / 1.5)
    g.pone("a", v=1.0, t=ta)
    g.pone("b", v=1.5, t=700.0)
    # b->c adiabatico con Tc = Ta: Vc = Vb * (Tb/Tc)^(1/(g-1))
    vc = 1.5 * (700.0 / ta) ** (1.0 / (g.g - 1.0))
    g.pone("c", v=vc, t=ta)
    # c->d isocorico hasta pd = 549.87 Pa
    g.pone("d", p=549.87, v=vc)
    # d->e isotermico hasta Ve = Va = 1.0
    g.pone("e", v=1.0, t=g.t("d"))
    rab = g.proceso("a", "b", "isobarico")
    rbc = g.proceso("b", "c", "adiabatico")
    rcd = g.proceso("c", "d", "isocorico")
    rde = g.proceso("d", "e", "isotermico")
    rea = g.proceso("e", "a", "isocorico")
    res = [("a→b", rab), ("b→c", rbc), ("c→d", rcd), ("d→e", rde), ("e→a", rea)]
    wnet = sum(r["W"] for _, r in res)
    qh = rab["Q"] + rea["Q"]
    ef = wnet / qh
    ecar = 1.0 - g.t("d") / 700.0
    L = _des("Q116", "Ciclo de cinco procesos con 3 mol de oxígeno (O₂)",
             "Tres moles de oxígeno ($\\text{O}_2$, diatómico): $a(1\\ \\text{m}^3)\\to b(1{,}5\\ \\text{m}^3, 700\\ \\text{K})$ isobárico; $b\\to c$ adiabático con $T_c=T_a$; $c\\to d$ isocórico hasta $p_d=549{,}87$ Pa; $d\\to e(1\\ \\text{m}^3)$ isotérmico; $e\\to a$ isocórico. Calcule $T_a, T_d, V_c$, trabajos, $\\Delta U$, $Q$, eficiencia y Carnot.",
             aviso="Con los datos impresos (n=3 mol, Vc=4,13 m³, pd=549,87 Pa), la ley de gas ideal da Td = 91,1 K (el folleto impreso reporta 76 K porque el cálculo editorial usó un factor distinto en Td, desplazando también Q_cd, W_de y Q_ea). Aquí calculamos todos los valores exactos con n=3 mol.",
             claves=["oxígeno", "cinco etapas", "eficiencia y Carnot"],
             visual=visual_pv(g, [("a", "b", "isobarico"), ("b", "c", "adiabatico"), ("c", "d", "isocorico"), ("d", "e", "isotermico"), ("e", "a", "isocorico")],
                              uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("(a) y (b) Temperaturas Ta, Td y volumen Vc")\
        .di("De la expansión isobárica sacamos te a, y de la curva adiabática sacamos ve c.", DATO("Tb = 700 K"),
            M(rf"T_a = T_b\frac{{V_a}}{{V_b}} = 700\frac{{1}}{{1{{,}}5}} = {tex(ta, dec=1, u='K')}"),
            M(rf"V_c = 1{{,}}5\left(\frac{{700}}{{{tex(ta, dec=1)}}}\right)^{{2{{,}}5}} = {tex(vc, 3, u='m^3')}"),
            M(rf"T_d = \frac{{p_d V_c}}{{nR}} = \frac{{(549{{,}}87)({tex(vc, 3)})}}{{(3)(8{{,}}314)}} = {tex(g.t('d'), dec=1, u='K')}"),
            tabla_estados(g, ["a", "b", "c", "d", "e"], up="kPa", uv="m³", fp=1e-3, cp=3, cv=3, dt=1))
    L.paso("(c)–(g) Trabajos, energía interna, calores, eficiencia y Carnot")\
        .di("Calculamos las cinco etapas en la tabla y sacamos la eficiencia real frente a la de Carnot.", V(foco="ciclo"),
            tabla_quw(res),
            R(rf"e = \frac{{{tex(wnet, 4)}}}{{{tex(qh, 5, sci='no')}}} = {tex(ef, 2)} = {tex(ef * 100, 3)}\,\%\qquad e_{{\text{{Carnot}}}} = 1 - \frac{{{tex(g.t('d'), dec=1)}}}{{700}} = {tex(ecar, 2)}", "rfg"))
    L.responde("Ta", rf"T_a={tex(ta, dec=0)}\ \text{{K}}", ta, "K")\
     .responde("Td", rf"T_d={tex(g.t('d'), dec=1)}\ \text{{K}}", g.t("d"), "K")\
     .responde("Vc", rf"V_c={tex(vc, 3)}\ \text{{m}}^3", vc, "m³")\
     .responde("e", rf"e={tex(ef, 2)}", ef, "")\
     .responde("eCar", rf"e_{{\text{{Carnot}}}}={tex(ecar, 2)}", ecar, "")
    return L.oficial("Folleto propuesto 4.2-3", 467.0, 76.0, 4.13, 0.40, 0.89,
                     nota="Ta=467 K y Vc=4,13 m³ coinciden exacto; con pd=549,87 Pa y n=3 mol resulta Td=91,1 K (en vez de 76 K impreso en el folleto), lo que ajusta e=0,39 y e_Carnot=0,87.").usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}")
