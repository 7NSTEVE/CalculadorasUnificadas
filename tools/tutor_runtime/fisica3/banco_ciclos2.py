"""Superexamen Q106–Q116: ciclos de reposición y del folleto."""

from __future__ import annotations

import math

from .ayudas import ATM, R_GAS, Gas, explica_proceso, tabla_estados, tabla_quw, valores_visual, visual_pv, estado_T, estado_V, estado_p, cierre_ciclo, deriva_carnot, deriva_entropia
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, T, TABLA, V, s
from .formato import dice, n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado, **k)


# ============================================================================ Q106
@leccion("Q106")
def q106() -> Leccion:
    from .banco_recomendados import _rectangulo

    L = _rectangulo("Q106", fuente("Q106"))
    L.titulo = "Ciclo rectangular diatómico (2 mol)"
    return L.oficial("Reposición II-2023", 1620.0, 28381.0, -26762.0, 5.71, tol=0.02, nota="Es el mismo ejercicio 15 de los Recomendados.")

# ============================================================================ Q107
@leccion("Q107")
def q107() -> Leccion:
    g = Gas(1.0, "dia")
    g.pone("1", p=1.0, v=1.0)
    g.pone("2", p=2.0, v=1.0)
    g.pone("3", p=2.0, v=2.0)
    g.pone("4", p=1.0, v=2.0)
    ef = 2.0 / 19.0
    PV = r"p_0V_0"
    L = _des("Q107", "Ciclo rectangular en términos de p₀ y V₀",
             "Un gas ideal diatómico recorre el ciclo $1\\to2\\to3\\to4\\to1$ de la figura: $1(V_0,p_0)$, $2(V_0,2p_0)$, $3(2V_0,2p_0)$, $4(2V_0,p_0)$. A) Trabajo y calor de cada proceso en términos de $p_0$ y $V_0$. B) $Q_C$ y $Q_H$. C) Eficiencia.",
             claves=["ciclo simbólico", "p0 V0", "diatómico"],
             visual=visual_pv(g, [("1", "2", "isocorico"), ("2", "3", "isobarico"), ("3", "4", "isocorico"), ("4", "1", "isobarico")], uv="V₀", up="p₀"))
    L.paso("Qué nos dan y qué piden")\
        .di("No hay números: todo se deja en función de pe cero y ve cero.".replace("pe cero y ve cero", dice("p₀ y V₀", "pe cero y ve cero")), DATO("p0"),
            LISTA(["$1$: $(V_0,\\ p_0)$ y $2$: $(V_0,\\ 2p_0)$", "$3$: $(2V_0,\\ 2p_0)$ y $4$: $(2V_0,\\ p_0)$", "Diatómico: $C_V=\\frac{5}{2}R$, $C_p=\\frac{7}{2}R$"]))\
        .di("El truco: no hace falta la temperatura, porque ene erre te es pe por ve.".replace("ene erre te es pe por ve", dice("nRT = pV", "ene erre te es pe ve")), F(r"pV=nRT"), M(r"nRT_1=p_0V_0\quad nRT_2=2p_0V_0\quad nRT_3=4p_0V_0\quad nRT_4=2p_0V_0", "nrt", "azul"))
    L.paso("La energía interna en términos de pV")\
        .di("La energía interna con ce ve.".replace("ce ve", s("C_V")), F(r"\Delta U=nC_V\,\Delta T"), M(r"\Delta U=n\left(\frac{5}{2}R\right)\Delta T=\frac{5}{2}\,\Delta(nRT)"))\
        .di("Y ene erre te es pe ve.".replace("ene erre te es pe ve", dice("nRT=pV", "ene erre te es pe ve")), M(r"\Delta U=\frac{5}{2}\,\Delta(pV)", "du", "azul"))
    L.paso("A) Proceso 1→2 (volumen constante)")\
        .di("Sin cambio de volumen no hay trabajo.", V(foco="12"), M(r"W_{12}=0"))\
        .di("La energía interna.", M(rf"\Delta U_{{12}}=\frac{{5}}{{2}}(2{PV}-{PV})=\frac{{5}}{{2}}{PV}"))\
        .di("El calor, por la primera ley.", F(r"\Delta U=Q-W"), R(rf"Q_{{12}}=\Delta U_{{12}}+W_{{12}}=+\frac{{5}}{{2}}{PV}", "q12"))
    L.paso("A) Proceso 2→3 (presión constante 2p₀)")\
        .di("Trabajo a presión constante.", V(foco="23"), F(r"W=p(V_2-V_1)"), M(rf"W_{{23}}=2p_0(2V_0-V_0)=+2{PV}"))\
        .di("Energía interna.", M(rf"\Delta U_{{23}}=\frac{{5}}{{2}}(4{PV}-2{PV})=+5{PV}"))\
        .di("Calor.", R(rf"Q_{{23}}=5{PV}+2{PV}=+7{PV}", "q23"))
    L.paso("A) Proceso 3→4 (volumen constante 2V₀)")\
        .di("Otra vez sin trabajo.", V(foco="34"), M(r"W_{34}=0"))\
        .di("La presión baja a la mitad.", M(rf"\Delta U_{{34}}=\frac{{5}}{{2}}(2{PV}-4{PV})=-5{PV}"))\
        .di("Calor que sale.", R(rf"Q_{{34}}=-5{PV}", "q34"))
    L.paso("A) Proceso 4→1 (presión constante p₀)")\
        .di("Se comprime: trabajo negativo.", V(foco="41"), M(rf"W_{{41}}=p_0(V_0-2V_0)=-{PV}"))\
        .di("Energía interna.", M(rf"\Delta U_{{41}}=\frac{{5}}{{2}}({PV}-2{PV})=-\frac{{5}}{{2}}{PV}"))\
        .di("Calor.", R(rf"Q_{{41}}=-\frac{{5}}{{2}}{PV}-{PV}=-\frac{{7}}{{2}}{PV}", "q41"))
    L.paso("B) Calor absorbido y calor expulsado")\
        .di("Ordenamos todo en una tabla.", TABLA(["Proceso", "$W$", "$\\Delta U$", "$Q$"],
                                                  [["$1\\to2$", "$0$", "$+\\tfrac{5}{2}p_0V_0$", "$+\\tfrac{5}{2}p_0V_0$"], ["$2\\to3$", "$+2p_0V_0$", "$+5p_0V_0$", "$+7p_0V_0$"],
                                                   ["$3\\to4$", "$0$", "$-5p_0V_0$", "$-5p_0V_0$"], ["$4\\to1$", "$-p_0V_0$", "$-\\tfrac{5}{2}p_0V_0$", "$-\\tfrac{7}{2}p_0V_0$"]]), V(foco="ciclo"))\
        .di("Absorbido: los calores positivos.", R(rf"Q_H=\frac{{5}}{{2}}{PV}+7{PV}=\frac{{19}}{{2}}{PV}", "qh"))\
        .di("Expulsado: los negativos.", R(rf"Q_C=-5{PV}-\frac{{7}}{{2}}{PV}=-\frac{{17}}{{2}}{PV}", "qc"))
    L.paso("C) Eficiencia")\
        .di("El trabajo neto es la suma de los trabajos: el área del rectángulo.", M(rf"W_{{\text{{neto}}}}=0+2{PV}+0-{PV}={PV}"))\
        .di("Eficiencia.", F(r"e=\frac{W}{Q_H}"), M(rf"e=\frac{{{PV}}}{{\frac{{19}}{{2}}{PV}}}=\frac{{2}}{{19}}"))\
        .di(f"Diez coma cinco por ciento.", R(rf"e=\frac{{2}}{{19}}={tex(ef * 100, 3)}\,\%", "re"))
    L.responde("B", r"Q_H=\frac{19}{2}p_0V_0", 9.5, "p0V0").responde("B", r"Q_C=-\frac{17}{2}p_0V_0", -8.5, "p0V0").responde("C", r"e=\frac{2}{19}", ef, "")
    return L.oficial("Reposición I-2024", 9.5, -8.5, 0.105, tol=0.01).usa(r"pV=nRT", r"W=p(V_2-V_1)", r"\Delta U=nC_V\Delta T", r"e=\frac{W}{Q_H}")

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
    wnet = sum(r["W"] for _, r in res)
    qh = rab["Q"]
    ef = wnet / qh
    ds_ab = g.n * g.cp * math.log(g.t("B") / g.t("A"))
    L = _des("Q108", "Ciclo triangular de 1 mol monoatómico (trabajo, Q_H, e y ΔS)",
             "Un mol de gas monoatómico recorre el ciclo de la figura. $A$: $2{,}0\\times10^5$ Pa y $0{,}0020$ m³; $p_C=p_A/2$ y $V_C=0{,}0045$ m³; $V_B=3V_A$ a la presión de A. A) Trabajo de cada proceso. B) Calor que entra. C) Eficiencia. D) $\\Delta S_{A\\to B}$.",
             claves=["triángulo", "monoatómico", "trapecio", "entropía"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "recta"), ("C", "A", "recta")], uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Un triángulo: arriba una línea horizontal y dos rectas inclinadas.", DATO("tres veces"),
            LISTA(["$A$: $p_A=2{,}0\\times10^5$ Pa, $V_A=0{,}0020$ m³", "$B$: misma presión, $V_B=3V_A=0{,}0060$ m³", "$C$: $p_C=\\frac{p_A}{2}=1{,}0\\times10^5$ Pa, $V_C=0{,}0045$ m³", "$n=1$ mol, monoatómico: $C_V=\\frac{3}{2}R$"]))\
        .di("Primero las temperaturas, con el gas ideal.", F(r"T=\frac{pV}{nR}"), tabla_estados(g, ["A", "B", "C"], up="Pa", uv="m³", cp=2, cv=2, dt=1))
    explica_proceso(L.paso("A) Proceso A→B (presión constante)"), g, "A", "B", "isobarico", foco="AB")
    explica_proceso(L.paso("A) Proceso B→C (recta)"), g, "B", "C", "recta", foco="BC")
    explica_proceso(L.paso("A) Proceso C→A (recta)"), g, "C", "A", "recta", foco="CA")
    L.paso("B) Calor que entra")\
        .di("Revisamos los signos de los tres calores.", tabla_quw(res), V(foco="ciclo"))\
        .di("Solo es positivo el de A a B: es el único calor que entra.", R(rf"Q_H=Q_{{AB}}={tex(qh, 4, u='J')}", "qh"))\
        .di("La cátedra pone mil novecientos noventa y nueve coma cinco por redondear las temperaturas.", NOTA("La oficial da $1999{,}5$ J: es el mismo valor redondeado."))
    L.paso("C) Eficiencia")\
        .di("El trabajo neto es la suma de los tres.", M(rf"W_{{\text{{neto}}}}={tex(rab['W'], 3)}+({tex(rbc['W'], 3)})+({tex(rca['W'], 3)})={tex(wnet, 3, u='J')}"))\
        .di("Entre el calor que entra.", F(r"e=\frac{W}{Q_H}"), M(rf"e=\frac{{{tex(wnet, 3)}}}{{{tex(qh, 4)}}}"), R(rf"e={tex(ef, 2)}={tex(ef * 100, 2)}\,\%", "re"))
    L.paso("D) Entropía de A a B")\
        .di("La entropía se arma con calor entre temperatura.", M(r"dS=\frac{dQ}{T}"), F(r"dQ=nC_p\,dT"))\
        .di("A presión constante, se integra y sale un logaritmo.", M(r"\Delta S_{AB}=\int_{T_A}^{T_B}\frac{nC_p\,dT}{T}=nC_p\ln\frac{T_B}{T_A}"))\
        .di("Como el volumen se triplica a presión constante, la temperatura también se triplica.", M(r"\frac{T_B}{T_A}=\frac{V_B}{V_A}=3"), M(r"C_p=C_V+R=\frac{5}{2}R"))\
        .di("Sustituimos.", M(r"\Delta S_{AB}=(1)\left(\frac{5}{2}\right)(8{,}314)\ln 3"), R(rf"\Delta S_{{AB}}={tex(ds_ab, 3, u='J/K')}", "rd"))
    L.responde("A", rf"W_{{AB}}={tex(rab['W'], 3)}\ \text{{J}}", rab["W"], "J").responde("A", rf"W_{{BC}}={tex(rbc['W'], 3)}\ \text{{J}}", rbc["W"], "J").responde("A", rf"W_{{CA}}={tex(rca['W'], 3)}\ \text{{J}}", rca["W"], "J")\
        .responde("B", rf"Q_H={tex(qh, 4)}\ \text{{J}}", qh, "J").responde("C", rf"e={tex(ef, 2)}", ef, "").responde("D", rf"\Delta S={tex(ds_ab, 3)}\ \text{{J/K}}", ds_ab, "J/K")
    return L.oficial("Reposición II-2024", 800.0, -225.0, -375.0, 1999.5, 0.10, 22.8).usa(r"W=\int_{V_1}^{V_2} p\,dV", r"W=p(V_2-V_1)", r"e=\frac{W}{Q_H}")

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
    L = _des("Q109", "Ciclo de 0,378 mol diatómico (isocórico, adiabático, isobárico)",
             "Un gas diatómico ($0{,}378$ mol) parte en $A$ ($4{,}30$ atm, $9{,}00$ L): $A\\to B$ enfriamiento isocórico; $B\\to C$ compresión adiabática a un tercio del volumen ($3{,}00$ L); $C\\to A$ expansión isobárica. A) Diagrama. B) Tabla $(p,V,T)$. C) $Q$ y $W$. D) Eficiencia.",
             claves=["diatómico", "adiabático", "eficiencia"],
             visual=visual_pv(g, [("A", "B", "isocorico"), ("B", "C", "adiabatico"), ("C", "A", "isobarico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Tres procesos: enfriar a volumen constante, comprimir sin calor y expandir a presión constante.", DATO("un tercio del volumen"),
            LISTA(["$n=0{,}378$ mol, diatómico: $\\gamma=1{,}40$", "$A$: $4{,}30$ atm y $9{,}00$ L", "$C$: misma presión que $A$ (proceso $C\\to A$ isobárico) y $3{,}00$ L", "$B$: volumen de $A$, sobre la adiabática que llega a $C$"]))\
        .di("En unidades del SI.", M(r"p_A=p_C=(4{,}30)(101\,325)=4{,}357\times10^5\ \text{Pa}"), M(r"V_A=9{,}00\times10^{-3}\ \text{m}^3\qquad V_C=3{,}00\times10^{-3}\ \text{m}^3"))
    p = L.paso("B) Temperaturas de A y C")
    estado_T(p, g, "A", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en A.", dec=0)
    estado_T(p, g, "C", dec=0)
    L.paso("B) Estado B con la adiabática")\
        .di("B tiene el volumen de A y está en la misma adiabática que C. La hoja da la relación de presión y volumen.", V(foco="BC"), F(r"pV^\gamma=\text{cte}"))\
        .di("La igualamos entre B y C y despejamos pe be.".replace("pe be", dice("p_B", "pe be")), M(r"p_BV_B^{\gamma}=p_CV_C^{\gamma}\;\Rightarrow\;p_B=p_C\left(\frac{V_C}{V_B}\right)^{\gamma}"))\
        .di("Sustituimos en atmósferas: la razón de volúmenes es un tercio. Guardamos seis cifras, porque luego se multiplica por un número grande.",
            M(rf"p_B=4{{,}}30\left(\frac{{3{{,}}00}}{{9{{,}}00}}\right)^{{1{{,}}40}}=4{{,}}30\,({tex((1.0 / 3.0) ** 1.4, 6)})={tex(pb / ATM, 6, u='atm')}"))\
        .di("En pascales.", M(rf"p_B=({tex(pb / ATM, 6)})(101\,325)={tex(pb, 4, u='Pa')}"))
    estado_T(L.paso("B) Temperatura de B"), g, "B", voz="Y la temperatura de B con el gas ideal.", dec=0)
    explica_proceso(L.paso("C1) Enfriamiento isocórico A→B"), g, "A", "B", "isocorico", foco="AB", dec_T=0)
    explica_proceso(L.paso("C2) Compresión adiabática B→C"), g, "B", "C", "adiabatico", foco="BC", dec_T=0)
    explica_proceso(L.paso("C3) Expansión isobárica C→A"), g, "C", "A", "isobarico", foco="CA", dec_T=0)
    c = cierre_ciclo(L, res, titulo="D) Eficiencia del motor", id_e="rd")
    L.responde("TA", rf"T_A={tex(g.t('A'), dec=0)}\ \text{{K}}", g.t("A"), "K")\
     .responde("TB", rf"T_B={tex(g.t('B'), dec=0)}\ \text{{K}}", g.t("B"), "K")\
     .responde("TC", rf"T_C={tex(g.t('C'), dec=0)}\ \text{{K}}", g.t("C"), "K")\
     .responde("e", rf"e={tex(c['e'], 3)}", c["e"], "")
    return L.oficial("Reposición II-2025", 1248.0, 268.0, 416.0, 0.158).usa(r"pV=nRT", r"pV^\gamma=\text{cte}", r"e=\frac{W}{Q_H}")

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
    v5 = g.v("1") * (g.t("1") / 256.0) ** (1.0 / (g.g - 1.0))
    g.pone("5", v=v5, t=256.0)
    T1, T2, T4 = g.t("1"), g.t("2"), 256.0
    V3, V4, V5 = g.v("3") * 1e3, g.v("4") * 1e3, v5 * 1e3
    p5 = g.p("5")
    r12 = g.proceso("1", "2", "isocorico")
    r23 = g.proceso("2", "3", "isotermico")
    r34 = g.proceso("3", "4", "isobarico")
    r45 = g.proceso("4", "5", "isotermico")
    r51 = g.proceso("5", "1", "adiabatico")
    res = [("1→2", r12), ("2→3", r23), ("3→4", r34), ("4→5", r45), ("5→1", r51)]
    L = _des("Q111", "Ciclo de cinco etapas (1 mol diatómico)",
             "Un mol de gas ideal diatómico parte de $1$: $1{,}50$ atm y $3{,}50$ L. $1\\to2$ isocórico hasta $3{,}00$ atm; $2\\to3$ expansión isotérmica hasta $2{,}00$ atm; $3\\to4$ expansión isobárica hasta $256$ K; $4\\to5$ compresión isotérmica; $5\\to1$ expansión adiabática. (a) $p$, $V$, $T$ de cada estado. (b) $\\Delta U$, $W$ y $Q$ de cada proceso.",
             claves=["cinco etapas", "adiabático", "folleto", "isotérmico"],
             visual=visual_pv(g, [("1", "2", "isocorico"), ("2", "3", "isotermico"), ("3", "4", "isobarico"), ("4", "5", "isotermico"), ("5", "1", "adiabatico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Cinco procesos seguidos que vuelven al inicio. Lo primero es conocer los cinco estados.", DATO("3.50 L"),
            LISTA(["$n=1{,}00$ mol, diatómico: $\\gamma=1{,}40$", "$1$: $1{,}50$ atm y $3{,}50$ L", "$2$: $3{,}00$ atm (mismo volumen)", "$3$: $2{,}00$ atm (misma temperatura que 2)", "$4$: $2{,}00$ atm y $256$ K", "$5$: en la isoterma de 256 K y en la adiabática que llega a 1"]))\
        .di("Las presiones en pascales: una atmósfera son ciento un mil trescientos veinticinco pascales.", M(r"p_1=1{,}50\ \text{atm}=1{,}520\times10^5\ \text{Pa}"), M(r"p_2=3{,}00\ \text{atm}=3{,}040\times10^5\ \text{Pa}"), M(r"p_3=2{,}00\ \text{atm}=2{,}027\times10^5\ \text{Pa}"))
    L.paso("(a) Estados 1 y 2")\
        .di("Temperatura de 1 con el gas ideal.", F(r"pV=nRT"), M(rf"T_1=\frac{{p_1V_1}}{{nR}}=\frac{{(1{{,}}520\times10^5)(3{{,}}50\times10^{{-3}})}}{{(1)(8{{,}}314)}}={tex(T1, 3, u='K')}"))\
        .di("De 1 a 2 el volumen no cambia y la presión se duplica: la temperatura también se duplica.", M(r"V_2=V_1=3{,}50\ \text{L}"), M(rf"T_2=T_1\,\frac{{p_2}}{{p_1}}=2T_1={tex(T2, 3, u='K')}"))
    L.paso("(a) Estado 3: expansión isotérmica")\
        .di("La temperatura se queda igual que en 2.", V(foco="23"), M(rf"T_3=T_2={tex(T2, 3, u='K')}"))\
        .di("A temperatura constante, pe por ve no cambia.".replace("pe por ve", dice("pV", "pe ve")), M(r"p_2V_2=p_3V_3\;\Rightarrow\;V_3=V_2\,\frac{p_2}{p_3}"), M(rf"V_3=(3{{,}}50)\left(\frac{{3{{,}}00}}{{2{{,}}00}}\right)={tex(V3, 3, u='L')}"))
    L.paso("(a) Estado 4: expansión isobárica")\
        .di("La presión se queda en dos atmósferas y la temperatura llega a doscientos cincuenta y seis.", V(foco="34"), M(r"p_4=p_3=2{,}00\ \text{atm}\qquad T_4=256\ \text{K}"))\
        .di("A presión constante, volumen y temperatura van juntos.", M(r"\frac{V_4}{T_4}=\frac{V_3}{T_3}\;\Rightarrow\;V_4=V_3\,\frac{T_4}{T_3}"), M(rf"V_4=({tex(V3, 3)})\left(\frac{{256}}{{{tex(T2, 3)}}}\right)={tex(V4, 3, u='L')}"))
    L.paso("(a) Estado 5: el cruce de una isoterma y una adiabática")\
        .di("El cinco está en la isoterma de doscientos cincuenta y seis kelvin.", V(foco="45"), M(r"T_5=T_4=256\ \text{K}"))\
        .di("Y en la adiabática que llega al estado uno.", F(r"TV^{\gamma-1}=\text{cte}"), M(r"T_5V_5^{\gamma-1}=T_1V_1^{\gamma-1}\;\Rightarrow\;V_5=V_1\left(\frac{T_1}{T_5}\right)^{\frac{1}{\gamma-1}}"))\
        .di("El exponente es dos coma cinco.", M(rf"V_5=(3{{,}}50)\left(\frac{{{tex(T1, 3)}}}{{256}}\right)^{{2{{,}}5}}={tex(V5, 3, u='L')}"))\
        .di("Y su presión, con el gas ideal: enorme, porque el volumen es pequeñísimo.", M(rf"p_5=\frac{{nRT_5}}{{V_5}}=\frac{{(1)(8{{,}}314)(256)}}{{{tex(v5, 3)}}}={tex(p5, 3, u='Pa')}"))
    L.paso("(a) Los cinco estados")\
        .di("Juntamos todo en la tabla.", tabla_estados(g, ["1", "2", "3", "4", "5"], up="×10⁵ Pa", uv="L", fp=1e-5, fv=1e3, cp=4, cv=3, dt=0), V(foco="ciclo"))\
        .di("Coincide con la tabla del folleto.", NOTA("Folleto: $V_5=0{,}109$ L y $p_5=195\\times10^5$ Pa."))
    explica_proceso(L.paso("(b) Proceso 1→2 (isocórico)"), g, "1", "2", "isocorico", foco="12")
    explica_proceso(L.paso("(b) Proceso 2→3 (isotérmico)"), g, "2", "3", "isotermico", foco="23")
    explica_proceso(L.paso("(b) Proceso 3→4 (isobárico)"), g, "3", "4", "isobarico", foco="34")
    explica_proceso(L.paso("(b) Proceso 4→5 (isotérmico)"), g, "4", "5", "isotermico", foco="45")
    explica_proceso(L.paso("(b) Proceso 5→1 (adiabático)"), g, "5", "1", "adiabatico", foco="51")
    L.paso("(b) Resumen y revisión")\
        .di("Ponemos los tres valores de cada proceso. Los cambios de energía interna deben sumar cero: es la revisión.", tabla_quw(res), V(foco="ciclo"))\
        .di("Suman cero: el gas volvió al estado uno.", NOTA("Si $\\sum\\Delta U\\neq0$, hay una temperatura mal calculada."))
    L.responde("Q12", rf"Q_{{12}}={tex(r12['Q'], 4)}\ \text{{J}}", r12["Q"], "J")\
     .responde("W23", rf"W_{{23}}={tex(r23['W'], 4)}\ \text{{J}}", r23["W"], "J")\
     .responde("Q34", rf"Q_{{34}}={tex(r34['Q'], 4)}\ \text{{J}}", r34["Q"], "J")\
     .responde("W45", rf"W_{{45}}={tex(r45['W'], 4)}\ \text{{J}}", r45["W"], "J")\
     .responde("W51", rf"W_{{51}}={tex(r51['W'], 4)}\ \text{{J}}", r51["W"], "J")
    return L.oficial("Folleto resuelto 4.1-1", 1330.0, 431.5, 3724.0, -9722.0, 3990.0).usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}", r"W=nRT\ln\left(\frac{V_2}{V_1}\right)", r"\Delta U=nC_V\Delta T")

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
    L = _des("Q112", "Ciclo triangular de 50 mol monoatómico",
             "Cincuenta moles de gas ideal monoatómico recorren el triángulo $a(0{,}500\\ \\text{m}^3, 3{,}0\\times 10^5\\ \\text{Pa})\\to b(0{,}800, 3{,}0\\times 10^5)\\to c(0{,}800, 1{,}0\\times 10^5)\\to a$ (recta). (a) Calor de cada etapa. (b) Eficiencia.",
             claves=["triángulo", "50 mol", "folleto"],
             visual=visual_pv(g, [("a", "b", "isobarico"), ("b", "c", "isocorico"), ("c", "a", "recta")],
                              uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Un triángulo en el diagrama pe ve: arriba una horizontal, a la derecha una vertical y de regreso una recta inclinada.".replace("pe ve", dice("p-V", "pe ve")), DATO("Cincuenta moles"),
            LISTA(["$a$: $3{,}0\\times10^5$ Pa y $0{,}500$ m³", "$b$: $3{,}0\\times10^5$ Pa y $0{,}800$ m³", "$c$: $1{,}0\\times10^5$ Pa y $0{,}800$ m³", "$n=50$ mol, monoatómico"]))\
        .di("Gas monoatómico: de la hoja, la capacidad a volumen constante es tres medios de erre.", F(r"C_V=\frac{3}{2}R"),
            M(rf"C_V=\frac{{3}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))
    p = L.paso("Temperaturas de los tres estados")
    estado_T(p, g, "a", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en a.")
    estado_T(p, g, "b")
    estado_T(p, g, "c")
    explica_proceso(L.paso("(a) Proceso a→b (presión constante)"), g, "a", "b", "isobarico", foco="ab")
    explica_proceso(L.paso("(a) Proceso b→c (volumen constante)"), g, "b", "c", "isocorico", foco="bc")
    explica_proceso(L.paso("(a) Proceso c→a (recta)"), g, "c", "a", "recta", foco="ca")
    c = cierre_ciclo(L, res, titulo="(b) Eficiencia del ciclo", id_e="rb")
    L.responde("Qab", rf"Q_{{ab}}={tex(rab['Q'] / 1e3, 3)}\ \text{{kJ}}", rab["Q"], "J")\
     .responde("Qbc", rf"Q_{{bc}}={tex(rbc['Q'] / 1e3, 3)}\ \text{{kJ}}", rbc["Q"], "J")\
     .responde("Qca", rf"Q_{{ca}}={tex(rca['Q'] / 1e3, 2)}\ \text{{kJ}}", rca["Q"], "J")\
     .responde("e", rf"e={tex(c['e'], 3)}", c["e"], "")
    return L.oficial("Folleto resuelto 4.1-2", 226e3, -240e3, 45e3, 0.111).usa(r"pV=nRT", r"W=\int_{V_1}^{V_2} p\,dV", r"W=p(V_2-V_1)", r"\Delta U=nC_V\Delta T", r"e=\frac{W}{Q_H}")

# ============================================================================ Q113
@leccion("Q113")
def q113() -> Leccion:
    g = Gas(1.00, "dia")
    g.pone("A", v=0.024, t=900.0)
    g.pone("B", v=0.224, t=900.0)
    tc = 900.0 * (0.024 / 0.224) ** (g.g - 1.0)
    g.pone("C", v=0.224, t=tc)
    rab = g.proceso("A", "B", "isotermico")
    rbc = g.proceso("B", "C", "isocorico")
    rca = g.proceso("C", "A", "adiabatico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    ds_ab = rab["Q"] / 900.0
    ds_bc = g.n * g.cv * math.log(tc / 900.0)
    L = _des("Q113", "Ciclo isotérmico-isocórico-adiabático (1 mol diatómico)",
             "Un mol de gas diatómico recorre $A\\to B$ isotérmico ($V_A=0{,}024\\ \\text{m}^3$, $V_B=0{,}224\\ \\text{m}^3$, $T_A=900$ K), $B\\to C$ isocórico y $C\\to A$ adiabático. (a) Estados y diagrama $p$-$V$. (b) $\\Delta U, W, Q$. (c) Eficiencia. (d) $\\Delta S$ por etapa.",
             claves=["isotérmico", "adiabático", "entropía"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "isocorico"), ("C", "A", "adiabatico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Tres procesos: una isoterma a novecientos kelvin, un enfriamiento a volumen constante y una adiabática que regresa al inicio.", DATO("TA = 900 K"),
            LISTA(["$n=1{,}00$ mol, diatómico", "$A$: $V_A=0{,}024$ m³, $T_A=900$ K", "$B$: $V_B=0{,}224$ m³, $T_B=T_A=900$ K", "$C$: $V_C=V_B$ y sobre la adiabática que llega a $A$"]))\
        .di("Diatómico: de la hoja salen ce ve y gama.".replace("ce ve", dice("C_V", "ce ve")), F(r"C_V=\frac{5}{2}R"), F(r"\gamma=\frac{C_P}{C_V}=\frac{7/2}{5/2}=1{,}40"))
    p = L.paso("(a) Presiones de A y B")
    estado_p(p, g, "A", primero=True, voz="Despejamos la presión del gas ideal y la calculamos en A.")
    estado_p(p, g, "B")
    L.paso("(a) Estado C: la adiabática C→A")\
        .di("C está a la derecha, con el mismo volumen que B.", V(foco="BC"), M(r"V_C=V_B=0{,}224\ \text{m}^3"))\
        .di("Y C está en la misma adiabática que A. La hoja da la relación de temperatura y volumen.", V(foco="CA"), F(r"TV^{\gamma-1}=\text{cte}"))\
        .di("La igualamos entre C y A y despejamos te C.".replace("te C", dice("T_C", "te ce")), M(r"T_CV_C^{\gamma-1}=T_AV_A^{\gamma-1}\;\Rightarrow\;T_C=T_A\left(\frac{V_A}{V_C}\right)^{\gamma-1}"))\
        .di("Sustituimos con gama menos uno igual a cero coma cuarenta.", M(rf"T_C=900\left(\frac{{0{{,}}024}}{{0{{,}}224}}\right)^{{0{{,}}40}}=900\,({tex(0.024 / 0.224, 4)})^{{0{{,}}40}}=900\,({tex((0.024 / 0.224) ** 0.4, 4)})"))\
        .di(f"Te ce vale {nu(tc, 'K', 4)}.".replace("Te ce", dice("T_C", "Te ce")), M(rf"T_C={tex(tc, 4, u='K')}", "tc", "verde"))
    estado_p(L.paso("(a) Presión de C"), g, "C", voz="Con el gas ideal sacamos la presión de C.")
    explica_proceso(L.paso("(b) Proceso A→B (isotérmico)"), g, "A", "B", "isotermico", foco="AB")
    explica_proceso(L.paso("(b) Proceso B→C (isocórico)"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("(b) Proceso C→A (adiabático)"), g, "C", "A", "adiabatico", foco="CA")
    c = cierre_ciclo(L, res, titulo="(c) Trabajo neto, calores y eficiencia", id_e="rc")
    deriva_entropia(L.paso("(d) Entropía: de dónde salen las fórmulas"), "iso", "cv", "adi")
    p = L.paso("(d) Cambio de entropía en cada etapa")
    p.di("Isoterma A a B: el calor entre la temperatura.", V(foco="AB"),
         M(rf"\Delta S_{{AB}}=\frac{{Q_{{AB}}}}{{T_A}}=\frac{{{tex(rab['Q'], 4)}}}{{900}}={tex(ds_ab, 3, u='J/K')}"))
    p.di("Isocora B a C, con ce ve.".replace("ce ve", dice("C_V", "ce ve")), V(foco="BC"),
         M(rf"\Delta S_{{BC}}=nC_V\ln\left(\frac{{T_C}}{{T_B}}\right)=(1)({tex(g.cv, 5)})\ln\left(\frac{{{tex(tc, 4)}}}{{900}}\right)={tex(ds_bc, 3, u='J/K')}"))
    p.di("Adiabática reversible: cero.", V(foco="CA"), M(r"\Delta S_{CA}=0"))
    p.di("Revisión: en un ciclo la entropía del gas vuelve a su valor, la suma da cero.",
         R(rf"\Delta S_{{AB}}=+{tex(ds_ab, 3, u='J/K')}\quad \Delta S_{{BC}}={tex(ds_bc, 3, u='J/K')}\quad \Delta S_{{CA}}=0", "rd"))
    L.responde("QAB", rf"Q_{{AB}}={tex(rab['Q'] / 1e3, 3)}\ \text{{kJ}}", rab["Q"], "J")\
     .responde("QBC", rf"Q_{{BC}}={tex(rbc['Q'] / 1e3, 3)}\ \text{{kJ}}", rbc["Q"], "J")\
     .responde("e", rf"e={tex(c['e'], 2)}", c["e"], "")\
     .responde("dSAB", rf"\Delta S_{{AB}}={tex(ds_ab, 3)}\ \text{{J/K}}", ds_ab, "J/K")
    return L.oficial("Folleto resuelto 4.1-3", 16700.0, -11000.0, 0.34, 18.6).usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}", r"W=nRT\ln\left(\frac{V_2}{V_1}\right)", r"e=1-\frac{|Q_C|}{Q_H}")

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
    L.paso("Qué nos dan y qué piden")\
        .di("Tres procesos que vuelven al inicio: comprimir a presión constante, expandir sin calor y calentar a volumen constante.", DATO("mitad de su volumen"),
            LISTA(["$n=0{,}400$ mol de $\\text{N}_2$, diatómico", "$1$: $2{,}0\\times10^5$ Pa y $300$ K", "$2$: misma presión, $V_2=\\frac{V_1}{2}$", "$3$: $V_3=V_1$, sobre la adiabática que sale de $2$"]))\
        .di("Diatómico: de la hoja salen ce ve y gama.".replace("ce ve", dice("C_V", "ce ve")), F(r"C_V=\frac{5}{2}R"), F(r"\gamma=\frac{C_P}{C_V}=1{,}40"))
    p = L.paso("(a) Estado 1")
    estado_V(p, g, "1", primero=True, voz="Despejamos el volumen del gas ideal y lo calculamos en 1.", dec_T=0)
    L.paso("(a) Estado 2: compresión a presión constante")\
        .di("El volumen baja a la mitad y la presión no cambia.", V(foco="12"), M(rf"V_2=\frac{{V_1}}{{2}}=\frac{{{tex(g.v('1'), 4)}}}{{2}}={tex(g.v('2'), 4, u='m^3')}"), M(r"p_2=p_1=2{,}00\times10^5\ \text{Pa}"))\
        .di("Del gas ideal, a presión constante el volumen y la temperatura van juntos.", F(r"pV=nRT"), M(r"\frac{V_2}{T_2}=\frac{V_1}{T_1}\;\Rightarrow\;T_2=T_1\,\frac{V_2}{V_1}"))\
        .di("La temperatura también baja a la mitad.", M(rf"T_2=300\left(\frac{{1}}{{2}}\right)={tex(g.t('2'), 4, u='K')}"))
    L.paso("(a) Estado 3: expansión adiabática")\
        .di("De 2 a 3 no hay calor. La hoja da la relación de presión y volumen en la adiabática.", V(foco="23"), F(r"pV^\gamma=\text{cte}"))\
        .di("La igualamos entre 2 y 3 y despejamos pe tres.".replace("pe tres", dice("p_3", "pe tres")), M(r"p_3V_3^{\gamma}=p_2V_2^{\gamma}\;\Rightarrow\;p_3=p_2\left(\frac{V_2}{V_3}\right)^{\gamma}"))\
        .di("Como ve tres es ve uno, la razón de volúmenes es un medio.".replace("ve tres es ve uno", dice("V₃ = V₁", "ve tres es ve uno")), M(rf"p_3=(2{{,}}00\times10^5)\left(\frac{{1}}{{2}}\right)^{{1{{,}}40}}=(2{{,}}00\times10^5)({tex(0.5 ** g.g, 5)})"))\
        .di(f"La presión de 3 es {nu(p3, 'Pa', 3)}.", M(rf"p_3={tex(p3, 3, u='Pa')}", "p3", "verde"))
    estado_T(L.paso("(a) Temperatura de 3"), g, "3", voz="Y con el gas ideal, la temperatura de 3.")
    explica_proceso(L.paso("(b) Proceso 1→2 (isobárico)"), g, "1", "2", "isobarico", foco="12", dec_T=1)
    explica_proceso(L.paso("(b) Proceso 2→3 (adiabático)"), g, "2", "3", "adiabatico", foco="23", dec_T=1)
    explica_proceso(L.paso("(b) Proceso 3→1 (isocórico)"), g, "3", "1", "isocorico", foco="31", dec_T=1)
    cierre_ciclo(L, res, titulo="(b) Resumen de calores y revisión", solo_totales=True)
    L.responde("Q12", rf"Q_{{12}}={tex(r12['Q'], 4)}\ \text{{J}}", r12["Q"], "J")\
     .responde("Q31", rf"Q_{{31}}={tex(r31['Q'], 4)}\ \text{{J}}", r31["Q"], "J")
    return L.oficial("Folleto propuesto 4.2-1", -1746.0, 1544.0).usa(r"pV=nRT", r"pV^\gamma=\text{cte}", r"W=p(V_2-V_1)", r"\Delta U=nC_V\Delta T")

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
    L.paso("Qué nos dan y qué piden")\
        .di("Cuatro procesos seguidos que vuelven al inicio. Primero hay que conocer los cuatro estados.", DATO("Ta = 293 K"),
            LISTA(["$n=2{,}00$ mol, monoatómico", "$a$: $V_a=1$ m³, $T_a=293$ K", "$b$: misma presión que $a$, $T_b=879$ K", "$c$: mismo volumen que $b$, $T_c=700$ K", "$d$: en la adiabática desde $c$ y en la isoterma de $293$ K"]))\
        .di("Monoatómico: de la hoja salen ce ve, ce pe y gama.".replace("ce ve, ce pe", dice("C_V, C_P", "ce ve, ce pe")), F(r"C_V=\frac{3}{2}R"), F(r"C_P=C_V+R=\frac{5}{2}R"), M(r"\gamma=\frac{C_P}{C_V}=\frac{5}{3}"))
    p = L.paso("(a) Estados a y b")
    estado_p(p, g, "a", primero=True, voz="Despejamos la presión del gas ideal y la calculamos en a.", dec_T=0)
    p.di("De a a b la presión no cambia: el volumen crece en la misma proporción que la temperatura.", V(foco="ab"),
         M(r"\frac{V_b}{T_b}=\frac{V_a}{T_a}\;\Rightarrow\;V_b=V_a\,\frac{T_b}{T_a}"),
         M(rf"V_b=(1)\left(\frac{{879}}{{293}}\right)={tex(g.v('b'), 4, u='m^3')}"))
    p = L.paso("(a) Estado c")
    p.di("De b a c el volumen no cambia.", V(foco="bc"), M(rf"V_c=V_b={tex(g.v('c'), 4, u='m^3')}"))
    estado_p(p, g, "c", dec_T=0)
    L.paso("(a) Estado d: la adiabática c→d")\
        .di("De ce a de no hay calor. La hoja da la relación de temperatura y volumen.".replace("ce a de", dice("c a d", "ce a de")), V(foco="cd"), F(r"TV^{\gamma-1}=\text{cte}"))\
        .di("Y de vuelve a la isoterma de doscientos noventa y tres kelvin.".replace("Y de vuelve", dice("Y d vuelve", "Y de vuelve")), M(r"T_d=T_a=293\ \text{K}"))\
        .di("Igualamos entre c y d y despejamos el volumen de d.", M(r"T_dV_d^{\gamma-1}=T_cV_c^{\gamma-1}\;\Rightarrow\;V_d=V_c\left(\frac{T_c}{T_d}\right)^{\frac{1}{\gamma-1}}"))\
        .di("El exponente es uno entre dos tercios: uno coma cinco.", M(r"\frac{1}{\gamma-1}=\frac{1}{5/3-1}=\frac{1}{2/3}=\frac{3}{2}=1{,}5"))\
        .di("Sustituimos.", M(rf"V_d=(3)\left(\frac{{700}}{{293}}\right)^{{1{{,}}5}}=(3)({tex((700.0 / 293.0) ** 1.5, 4)})"), M(rf"V_d={tex(vd, 4, u='m^3')}", "vd", "verde"))
    estado_p(L.paso("(a) Presión de d"), g, "d", voz="Y la presión de d con el gas ideal.", dec_T=0)
    explica_proceso(L.paso("(a) Proceso a→b (isobárico)"), g, "a", "b", "isobarico", foco="ab", dec_T=0)
    explica_proceso(L.paso("(a) Proceso b→c (isocórico)"), g, "b", "c", "isocorico", foco="bc", dec_T=0)
    explica_proceso(L.paso("(a) Proceso c→d (adiabático)"), g, "c", "d", "adiabatico", foco="cd", dec_T=0)
    explica_proceso(L.paso("(a) Proceso d→a (isotérmico)"), g, "d", "a", "isotermico", foco="da", dec_T=0)
    cierre_ciclo(L, res, titulo="(a) Totales del ciclo", solo_totales=True)
    deriva_entropia(L.paso("(b) Entropía: de dónde salen las fórmulas"), "cp", "cv", "adi", "iso")
    p = L.paso("(b) Cambio de entropía en cada proceso")
    p.di("De a a b, a presión constante.", V(foco="ab"),
         M(rf"\Delta S_{{ab}}=nC_p\ln\left(\frac{{T_b}}{{T_a}}\right)=(2)({tex(g.cp, 5)})\ln\left(\frac{{879}}{{293}}\right)=(41{{,}}57)({tex(math.log(3.0), 4)})={tex(ds_ab, 3, u='J/K')}"))
    p.di("De b a c, a volumen constante.", V(foco="bc"),
         M(rf"\Delta S_{{bc}}=nC_V\ln\left(\frac{{T_c}}{{T_b}}\right)=(2)({tex(g.cv, 5)})\ln\left(\frac{{700}}{{879}}\right)=(24{{,}}94)({tex(math.log(700.0 / 879.0), 4)})={tex(ds_bc, 3, u='J/K')}"))
    p.di("De c a d, adiabática reversible.", V(foco="cd"), M(r"\Delta S_{cd}=0"))
    p.di("De d a a, la isoterma: su calor entre la temperatura.", V(foco="da"),
         M(rf"\Delta S_{{da}}=\frac{{Q_{{da}}}}{{T_a}}=\frac{{{tex(rda['Q'], 5, sci='no')}}}{{293}}={tex(ds_da, 3, u='J/K')}"))
    p.di("Revisión: el ciclo es reversible, la suma tiene que dar cero.",
         M(rf"\sum\Delta S=({tex(ds_ab, 3)})+({tex(ds_bc, 3)})+0+({tex(ds_da, 3)})\approx 0"),
         R(rf"\Delta S_{{ab}}=+{tex(ds_ab, 3, u='J/K')}\quad \Delta S_{{bc}}={tex(ds_bc, 3, u='J/K')}\quad \Delta S_{{cd}}=0\quad \Delta S_{{da}}={tex(ds_da, 3, u='J/K')}", "rb"))
    L.responde("Qab", rf"{tex(rab['Q'], 5, sci='no')}\ \text{{J}}", rab["Q"], "J")\
     .responde("Qbc", rf"{tex(rbc['Q'], 4)}\ \text{{J}}", rbc["Q"], "J")\
     .responde("Wcd", rf"{tex(rcd['W'], 5, sci='no')}\ \text{{J}}", rcd["W"], "J")\
     .responde("Wda", rf"{tex(rda['W'], 5, sci='no')}\ \text{{J}}", rda["W"], "J")
    return L.oficial("Folleto propuesto 4.2-2", 24360.0, -4465.0, 10151.0, -11727.0).usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}", r"dQ=nC_p\,dT", r"dQ=nC_V\,dT")

# ============================================================================ Q116
@leccion("Q116")
def q116() -> Leccion:
    g = Gas(3.00, "dia")
    # a->b isobárico: Va=1.0, Vb=1.5, Tb=700 => Ta = 700/1.5 = 466,666...
    # Se usa 466,67 K, como se escribe: n·C_V·ΔT con el valor exacto cae justo en ...,5 J y la última cifra
    # escrita no saldría con calculadora
    ta = round(700.0 * (1.0 / 1.5), 2)
    g.pone("a", v=1.0, t=ta)
    g.pone("b", v=1.5, t=700.0)
    vc = 1.5 * (700.0 / ta) ** (1.0 / (g.g - 1.0))
    g.pone("c", v=vc, t=ta)
    g.pone("d", p=549.87, v=vc)
    g.pone("e", v=1.0, t=g.t("d"))
    rab = g.proceso("a", "b", "isobarico")
    rbc = g.proceso("b", "c", "adiabatico")
    rcd = g.proceso("c", "d", "isocorico")
    rde = g.proceso("d", "e", "isotermico")
    rea = g.proceso("e", "a", "isocorico")
    res = [("a→b", rab), ("b→c", rbc), ("c→d", rcd), ("d→e", rde), ("e→a", rea)]
    td = g.t("d")
    ecar = 1.0 - td / 700.0
    L = _des("Q116", "Ciclo de cinco procesos con 3 mol de oxígeno (O₂)",
             "Tres moles de oxígeno ($\\text{O}_2$, diatómico): $a(1\\ \\text{m}^3)\\to b(1{,}5\\ \\text{m}^3, 700\\ \\text{K})$ isobárico; $b\\to c$ adiabático con $T_c=T_a$; $c\\to d$ isocórico hasta $p_d=549{,}87$ Pa; $d\\to e(1\\ \\text{m}^3)$ isotérmico; $e\\to a$ isocórico. Calcule $T_a, T_d, V_c$, trabajos, $\\Delta U$, $Q$, eficiencia y Carnot.",
             aviso="Con los datos impresos (n=3 mol, Vc=4,13 m³, pd=549,87 Pa), la ley de gas ideal da Td = 91,1 K; el folleto reporta 76 K y eso desplaza también Q_cd, W_de, Q_ea y las eficiencias. Aquí todo se calcula con los datos impresos.",
             claves=["oxígeno", "cinco etapas", "eficiencia y Carnot"],
             visual=visual_pv(g, [("a", "b", "isobarico"), ("b", "c", "adiabatico"), ("c", "d", "isocorico"), ("d", "e", "isotermico"), ("e", "a", "isocorico")],
                              uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Cinco procesos que vuelven al inicio. Primero los estados, después cada proceso y al final las eficiencias.", DATO("Tb = 700 K"),
            LISTA(["$n=3{,}00$ mol de $\\text{O}_2$, diatómico", "$a$: $V_a=1$ m³ · $b$: $V_b=1{,}5$ m³, $T_b=700$ K", "$c$: sobre la adiabática desde $b$, con $T_c=T_a$", "$d$: $V_d=V_c$, $p_d=549{,}87$ Pa · $e$: $V_e=1$ m³, $T_e=T_d$"]))\
        .di("Diatómico: de la hoja salen ce ve y gama.".replace("ce ve", dice("C_V", "ce ve")), F(r"C_V=\frac{5}{2}R"), F(r"\gamma=\frac{C_P}{C_V}=1{,}40"))
    p = L.paso("(a) Temperatura de a")
    p.di("De a a b la presión no cambia. Del gas ideal, volumen y temperatura van en la misma proporción.", V(foco="ab"), F(r"pV=nRT"),
         M(r"\frac{V_a}{T_a}=\frac{V_b}{T_b}\;\Rightarrow\;T_a=T_b\,\frac{V_a}{V_b}"))
    p.di(f"Sustituimos: te a es {nu(ta, 'K', 4)}.".replace("te a", dice("T_a", "te a")), M(rf"T_a=700\left(\frac{{1}}{{1{{,}}5}}\right)={tex(ta, 4, u='K')}", "ta", "verde"))
    estado_p(p, g, "a", voz="Y la presión de a, que también es la de b.")
    L.paso("(b) Volumen de c con la adiabática")\
        .di("De be a ce no hay calor. La hoja da la relación de temperatura y volumen.".replace("be a ce", dice("b a c", "be a ce")), V(foco="bc"), F(r"TV^{\gamma-1}=\text{cte}"))\
        .di("La igualamos entre b y c, y despejamos el volumen de c.", M(r"T_cV_c^{\gamma-1}=T_bV_b^{\gamma-1}\;\Rightarrow\;V_c=V_b\left(\frac{T_b}{T_c}\right)^{\frac{1}{\gamma-1}}"))\
        .di("Con gama igual a uno coma cuatro, el exponente es dos coma cinco; y te ce es te a.".replace("te ce es te a", dice("T_c = T_a", "te ce es te a")), M(r"\frac{1}{\gamma-1}=\frac{1}{0{,}40}=2{,}5"))\
        .di("Sustituimos.", M(rf"V_c=1{{,}}5\left(\frac{{700}}{{{tex(ta, 4)}}}\right)^{{2{{,}}5}}=1{{,}}5\,(1{{,}}5)^{{2{{,}}5}}=1{{,}}5\,({tex(1.5 ** 2.5, 4)})"), M(rf"V_c={tex(vc, 3, u='m^3')}", "vc", "verde"))
    p = L.paso("(a) Temperatura de d")
    p.di("De c a d el volumen no cambia, y la presión de d es dato.", V(foco="cd"), M(rf"V_d=V_c={tex(vc, 4, u='m^3')}\qquad p_d=549{{,}}87\ \text{{Pa}}"))
    estado_T(p, g, "d", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en d.")
    p.di("El folleto escribe setenta y seis kelvin; con sus propios datos sale noventa y uno coma uno.", OJO("Con $p_d=549{,}87$ Pa y $V_c=4{,}13$ m³ el gas ideal da $T_d=91{,}1$ K, no $76$ K."))
    p = L.paso("Estado e")
    p.di("De d a e la temperatura no cambia y el volumen vuelve a un metro cúbico.", V(foco="de"), M(rf"T_e=T_d={tex(td, 4, u='K')}\qquad V_e=1\ \text{{m}}^3"))
    estado_p(p, g, "e")
    explica_proceso(L.paso("(c) Proceso a→b (isobárico)"), g, "a", "b", "isobarico", foco="ab")
    explica_proceso(L.paso("(c) Proceso b→c (adiabático)"), g, "b", "c", "adiabatico", foco="bc")
    explica_proceso(L.paso("(c) Proceso c→d (isocórico)"), g, "c", "d", "isocorico", foco="cd")
    explica_proceso(L.paso("(c) Proceso d→e (isotérmico)"), g, "d", "e", "isotermico", foco="de")
    explica_proceso(L.paso("(c) Proceso e→a (isocórico)"), g, "e", "a", "isocorico", foco="ea")
    c = cierre_ciclo(L, res, titulo="(d) Trabajo neto, calores y eficiencia", id_e="re")
    deriva_carnot(L.paso("(e) Carnot: de dónde sale su eficiencia"))
    p = L.paso("(e) Eficiencia de Carnot entre las temperaturas extremas")
    p.di("Las temperaturas extremas del ciclo: la mayor es la de b y la menor la de d y e.", V(foco="ciclo"), M(rf"T_H=700\ \text{{K}}\qquad T_C={tex(td, 4, u='K')}"))
    p.di("Sustituimos.", M(rf"e_{{\text{{Carnot}}}}=1-\frac{{{tex(td, 4)}}}{{700}}=1-{tex(td / 700.0, 4)}"), R(rf"e_{{\text{{Carnot}}}}={tex(ecar, 3)}", "rcar"))
    p.di("La eficiencia real es mucho menor que la de Carnot, como debe ser.", M(rf"e={tex(c['e'], 3)}<e_{{\text{{Carnot}}}}={tex(ecar, 3)}"))
    L.responde("Ta", rf"T_a={tex(ta, dec=0)}\ \text{{K}}", ta, "K")\
     .responde("Td", rf"T_d={tex(td, dec=1)}\ \text{{K}}", td, "K")\
     .responde("Vc", rf"V_c={tex(vc, 3)}\ \text{{m}}^3", vc, "m³")\
     .responde("e", rf"e={tex(c['e'], 2)}", c["e"], "")\
     .responde("eCar", rf"e_{{\text{{Carnot}}}}={tex(ecar, 2)}", ecar, "")
    return L.oficial("Folleto propuesto 4.2-3", 467.0, 76.0, 4.13, 0.40, 0.89,
                     nota="Ta=467 K y Vc=4,13 m³ coinciden exacto; con pd=549,87 Pa y n=3 mol resulta Td=91,1 K (en vez de 76 K impreso en el folleto), lo que ajusta e y e_Carnot.").usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}", r"e=\frac{W}{Q_H}=1-\left|\frac{Q_c}{Q_H}\right|")
