"""Superexamen Q092–Q105: procesos y ciclos termodinámicos (primera parte)."""

from __future__ import annotations

import math

from .ayudas import ATM, R_GAS, T0, Gas, explica_proceso, revisar_opciones, tabla_estados, tabla_quw, valores_visual, visual_pv
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .formato import n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion


def _sel(q: str, titulo: str, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), tipo="seleccion", fuente=fuente(q), enunciado=enunciado,
                   opciones=opciones, correcta=correcta, **k)


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado, **k)


# ============================================================================ Q092
@leccion("Q092")
def q092() -> Leccion:
    g = Gas(2.50, "dia")
    g.pone("1", p=1.00 * ATM, t=20.0 + T0)
    q12 = 1.52e4
    t2 = g.t("1") + q12 / (g.n * g.cv)
    g.pone("2", v=g.v("1"), t=t2)
    g.pone("3", p=g.p("2"), v=3.0 * g.v("1"))
    r12 = g.proceso("1", "2", "isocorico")
    r23 = g.proceso("2", "3", "isobarico")
    ds12 = g.n * g.cv * math.log(g.t("2") / g.t("1"))
    ds23 = g.n * g.cp * math.log(g.t("3") / g.t("2"))
    res = [("1→2", r12), ("2→3", r23)]
    L = _des("Q092", "Calentamiento isocórico seguido de expansión isobárica",
             "Un gas ideal diatómico ($2{,}50$ mol) inicia a $1{,}00$ atm y $20{,}0\\ ^\\circ$C. Se calienta isocóricamente con $1{,}52\\times 10^4$ J y luego se expande isobáricamente al triple de su volumen. A) Diagrama $p$-$V$. B) Estados $(p,V,T)$. C) $W, Q, \\Delta U$. D) $\\Delta S$ de cada proceso.",
             claves=["isocórico", "isobárico", "gas diatómico"],
             visual=visual_pv(g, [("1", "2", "isocorico"), ("2", "3", "isobarico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("A y B) Hallar presión, volumen y temperatura en los 3 estados")\
        .di("Empezamos en doscientos noventa y tres coma quince kelvin y hallamos el volumen inicial.", DATO("2,50 moles"),
            M(rf"V_1 = \frac{{nRT_1}}{{p_1}} = {tex(g.v('1') * 1e3, 3, u='L')}"))\
        .di("El calor isocórico sube la temperatura hasta quinientos ochenta y cinco coma siete kelvin y duplica la presión; luego triplicar volumen a presión fija triplica la temperatura.",
            M(rf"T_2 = T_1 + \frac{{Q_{{12}}}}{{nC_V}} = {tex(t2, dec=1, u='K')}\qquad T_3 = 3T_2 = {tex(g.t('3'), dec=1, u='K')}"),
            tabla_estados(g, ["1", "2", "3"], up="atm", uv="L", fp=1.0 / ATM, fv=1e3, cp=3, cv=3))
    explica_proceso(L.paso("C1) Calentamiento isocórico 1→2"), g, "1", "2", "isocorico", foco="12")
    explica_proceso(L.paso("C2) Expansión isobárica 2→3"), g, "2", "3", "isobarico", foco="23")
    L.paso("D) Cambio de entropía en cada etapa")\
        .di(f"Usamos la fórmula logarítmica en cada proceso: salen {nu(ds12, 'J/K', 4)} y {nu(ds23, 'J/K', 4)}.",
            M(rf"\Delta S_{{12}} = nC_V\ln\frac{{T_2}}{{T_1}} = {tex(ds12, 4, u='J/K')}"),
            M(rf"\Delta S_{{23}} = nC_p\ln\frac{{T_3}}{{T_2}} = nC_p\ln 3 = {tex(ds23, 4, u='J/K')}"),
            R(rf"\Delta S_{{12}}={tex(ds12, 4, u='J/K')}\qquad \Delta S_{{23}}={tex(ds23, 4, u='J/K')}", "rd"))
    L.responde("T2", rf"T_2={tex(t2, dec=1)}\ \text{{K}}", t2, "K")\
     .responde("Q23", rf"Q_{{23}}={tex(r23['Q'] / 1e3, 4)}\ \text{{kJ}}", r23["Q"], "J")\
     .responde("dS23", rf"\Delta S_{{23}}={tex(ds23, 4)}\ \text{{J/K}}", ds23, "J/K")
    return L.usa(r"pV=nRT", r"Q=nC_V\Delta T", r"Q=nC_p\Delta T")


# ============================================================================ Q093
@leccion("Q093")
def q093() -> Leccion:
    L = _sel("Q093", "Qué pasa con la temperatura en una expansión adiabática",
             "En un gas ideal se da un proceso de expansión adiabática. Según la primera ley, es correcto afirmar que:",
             {"A": "La temperatura aumenta", "B": "La temperatura disminuye",
              "C": "La presión se mantiene constante", "D": "El trabajo realizado por el sistema es negativo"}, "B",
             claves=["expansión adiabática", "primera ley"])
    L.paso("Sin calor entrante, la expansión sale de la energía interna")\
        .di("Adiabático significa calor cero; expansión significa trabajo positivo.", DATO("expansión adiabática"),
            F(r"\Delta U = Q - W = 0 - W = -W < 0"))\
        .di("Como delta U es negativo, la temperatura del gas disminuye.",
            R(r"\Delta U = nC_V\Delta T < 0 \;\Rightarrow\; T\ \text{disminuye}"))
    revisar_opciones(L, {
        "A": ("La A ocurre al comprimir adiabáticamente, no al expandir.", "Al expandir se enfría."),
        "C": ("La C corresponde a un proceso isobárico, no adiabático.", "La presión cae rápidamente."),
        "D": ("La D tiene el signo al revés: al expandirse el trabajo es positivo.", "$W > 0$."),
        "B": ("La B es la correcta: la temperatura disminuye.", "$Q=0, W>0 \\Rightarrow \\Delta T < 0$."),
    })
    return L.oficial("II Parcial ordinario II-2023", "B").usa(r"\Delta U=Q-W")


# ============================================================================ Q094
@leccion("Q094")
def q094() -> Leccion:
    g = Gas(10.0, "dia")
    g.pone("A", p=1.00 * ATM, t=300.0)
    g.pone("B", p=0.500 * ATM, t=300.0)
    rab = g.proceso("A", "B", "recta")
    rba = g.proceso("B", "A", "isotermico")
    res = [("A→B", rab), ("B→A", rba)]
    wnet = rab["W"] + rba["W"]
    qh = rab["Q"]
    qc = rba["Q"]
    ef = wnet / qh
    L = _des("Q094", "Ciclo de dos etapas: línea recta y compresión isotérmica",
             "Diez moles de gas ideal diatómico van de $A$ ($1{,}00$ atm) a $B$ ($0{,}500$ atm, $300$ K) en línea recta en $p$-$V$, y regresan de $B$ a $A$ isotérmicamente. A) Diagrama $p$-$V$. B) $V_A$ y $V_B$. C) $Q, W, \\Delta U$. D) $Q_H, W_{\\text{neto}}, Q_C$ y eficiencia.",
             claves=["línea recta", "isotérmica", "dos etapas"],
             visual=visual_pv(g, [("A", "B", "recta"), ("B", "A", "isotermico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("A y B) Volúmenes en A y en B")\
        .di("Como el regreso B a A es isotérmico, ambos estados están a trescientos kelvin.", DATO("comprime isotérmicamente"),
            M(rf"T_A = T_B = 300\ \text{{K}} \qquad V_A = \frac{{nRT}}{{p_A}} = {tex(g.v('A'), 4, u='m^3')} \qquad V_B = 2V_A = {tex(g.v('B'), 4, u='m^3')}"),
            tabla_estados(g, ["A", "B"], up="atm", uv="m³", fp=1.0 / ATM, cp=3, cv=4))
    explica_proceso(L.paso("C1) Expansión en línea recta A→B"), g, "A", "B", "recta", foco="AB")
    explica_proceso(L.paso("C2) Compresión isotérmica B→A"), g, "B", "A", "isotermico", foco="BA")
    L.paso("D) Calor absorbido, rechazado, trabajo neto y eficiencia")\
        .di(f"El trabajo neto es {nu(wnet, 'J', 4)} y la eficiencia sale {n(ef * 100, 3)} por ciento.", V(foco="ciclo"),
            tabla_quw(res),
            F(r"e = \frac{W_{\text{neto}}}{Q_H}"),
            R(rf"Q_H={tex(qh / 1e3, 4, u='kJ')}\quad Q_C={tex(qc / 1e3, 4, u='kJ')}\quad W_{{\text{{neto}}}}={tex(wnet, 4, u='J')}\quad e={tex(ef * 100, 3)}\,\%", "rd"))
    L.responde("VA", rf"V_A={tex(g.v('A'), 4)}\ \text{{m}}^3", g.v("A"), "m³")\
     .responde("Wnet", rf"W_{{\text{{neto}}}}={tex(wnet, 4)}\ \text{{J}}", wnet, "J")\
     .responde("e", rf"e={tex(ef, 3)}", ef, "")
    return L.usa(r"W=\frac{p_1+p_2}{2}\Delta V", r"W=nRT\ln\frac{V_2}{V_1}", r"e=\frac{W}{Q_H}")


# ============================================================================ Q095
@leccion("Q095")
def q095() -> Leccion:
    g = Gas(1.0, "dia")
    g.pone("0", p=4e5, v=0.01)
    g.pone("D", p=4e5, v=0.025)
    g.pone("C", v=0.025, t=g.t("0"))
    g.pone("B", v=0.025, p=g.p("0") * (0.01 / 0.025) ** g.g)
    g.pone("A", v=0.01, p=1.2e5)
    L = _sel("Q095", "Identificar la curva adiabática entre cuatro procesos en p-V",
             "Cuatro procesos parten del mismo punto sobre la isoterma $T_3$: A baja verticalmente (isocórico), B cae más empinado que $T_3$ hasta $T_4$, C sigue la isoterma $T_3$, y D avanza horizontalmente hasta $T_1$. ¿Cuál es el adiabático?",
             {"A": "Proceso A", "B": "Proceso B", "C": "Proceso C", "D": "Proceso D"}, "B",
             aviso="En el Superexamen se pegó una figura equivocada; en la figura original de 24-I-E, B es la curva más empinada que la isoterma T₃.",
             claves=["adiabática", "pendiente", "isoterma"],
             visual=visual_pv(g, [("0", "D", "isobarico"), ("0", "C", "isotermico"), ("0", "B", "adiabatico"), ("0", "A", "isocorico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3))
    L.paso("Comparar las cuatro trayectorias")\
        .di("Una expansión adiabática se enfría, así que su curva cae por debajo de la isoterma inicial con mayor pendiente.", DATO("adiabático"),
            LISTA(["**A**: vertical ($V=\\text{cte}$, isocórico)",
                   "**B**: cae más empinada que $T_3$ hasta una isoterma más fría $T_4$ (**adiabático**)",
                   "**C**: sigue exactamente la isoterma $T_3$ (isotérmico)",
                   "**D**: horizontal ($p=\\text{cte}$, isobárico)"]),
            R(r"\text{Adiabático} = \text{Proceso B}"))
    revisar_opciones(L, {
        "A": ("La A es vertical: proceso isocórico.", "$V=\\text{cte}$."),
        "C": ("La C va pegada a la isoterma: proceso isotérmico.", "$T=\\text{cte}$."),
        "D": ("La D es horizontal: proceso isobárico.", "$p=\\text{cte}$."),
        "B": ("La B es la correcta: cae más empinada porque pe por ve a la gama es constante con gama mayor que uno.", "Cruza hacia una isoterma más fría."),
    })
    return L.oficial("III Parcial extraordinario I-2024", "B").usa(r"pV^\gamma=\text{cte}")


# ============================================================================ Q096
@leccion("Q096")
def q096() -> Leccion:
    p_lo, p_hi = 1.0 * ATM, 3.0 * ATM
    va, vb, vc, vd = 0.20, 0.09, 0.40, 1.20
    qbc, qda = 100e3, -150e3
    wbc = p_hi * (vc - vb)
    dubc = qbc - wbc
    ducd = 0.0
    wda = p_lo * (va - vd)
    duda = qda - wda
    duab = -(dubc + ducd + duda)
    gamma = math.log(3.0 / 1.0) / math.log(va / vb)
    g = Gas(1.0, cv=R_GAS / (gamma - 1.0))
    g.pone("A", p=p_lo, v=va)
    g.pone("B", p=p_hi, v=vb)
    g.pone("C", p=p_hi, v=vc)
    g.pone("D", p=p_lo, v=vd)
    L = _des("Q096", "Ciclo de cuatro procesos con calores dados y cálculo de γ",
             "Un gas ideal sigue $A(0{,}20\\ \\text{m}^3, 1{,}0\\ \\text{atm})\\to B(0{,}09, 3{,}0)$ adiabático; $B\\to C(0{,}40, 3{,}0)$ isobárico con $Q_{BC}=+100$ kJ; $C\\to D(1{,}20, 1{,}0)$ isotérmico; $D\\to A$ isobárico con $Q_{DA}=-150$ kJ. Halle: A) $\\Delta U_{BC}$. B) $\\Delta U_{CD}$. C) $\\Delta U_{AB}$. D) $\\gamma=C_p/C_V$.",
             claves=["primera ley en ciclos", "adiabático", "gamma"],
             visual=visual_pv(g, [("A", "B", "adiabatico"), ("B", "C", "isobarico"), ("C", "D", "isotermico"), ("D", "A", "isobarico")],
                              uv="m³", up="atm", fp=1.0 / ATM))
    L.paso("A y B) Energía interna en B→C y C→D")\
        .di("En el tramo isobárico de B a C hallamos el trabajo y lo restamos al calor entrante.", V(foco="BC"), DATO("100 kJ"),
            M(rf"W_{{BC}} = p_B(V_C-V_B) = (3{{,}}0\times 101325)(0{{,}}40-0{{,}}09) = {tex(wbc / 1e3, 4, u='kJ')}"),
            R(rf"\Delta U_{{BC}} = Q_{{BC}} - W_{{BC}} = 100 - {tex(wbc / 1e3, 4)} = +{tex(dubc / 1e3, 3, u='kJ')}", "ra"))\
        .di("De C a D es isotérmico, así que el cambio de energía interna es cero.", V(foco="CD"),
            R(r"\Delta U_{CD} = 0", "rb"))
    L.paso("C) Energía interna en A→B cerrando el ciclo")\
        .di("Calculamos el tramo isobárico de D a A.", V(foco="DA"),
            M(rf"W_{{DA}} = (1{{,}}0\times 101325)(0{{,}}20-1{{,}}20) = {tex(wda / 1e3, 4, u='kJ')}"),
            M(rf"\Delta U_{{DA}} = -150 - ({tex(wda / 1e3, 4)}) = {tex(duda / 1e3, 3, u='kJ')}"))\
        .di(f"Como la suma de los cuatro cambios en un ciclo es cero, despejamos el tramo adiabático: da {nu(duab / 1e3, 'kJ', 3)}.", V(foco="AB"),
            F(r"\Delta U_{AB} + \Delta U_{BC} + \Delta U_{CD} + \Delta U_{DA} = 0"),
            R(rf"\Delta U_{{AB}} = +{tex(duab / 1e3, 3, u='kJ')}", "rc"))
    L.paso("D) Razón de capacidades caloríficas γ")\
        .di("Usamos los puntos A y B del proceso adiabático y tomamos logaritmo.",
            F(r"p_A V_A^\gamma = p_B V_B^\gamma \;\Rightarrow\; \gamma = \frac{\ln(p_B/p_A)}{\ln(V_A/V_B)}"),
            M(r"\gamma = \frac{\ln(3{,}0/1{,}0)}{\ln(0{,}20/0{,}09)}"),
            R(rf"\gamma = {tex(gamma, 3)}", "rd"))
    L.responde("A", rf"\Delta U_{{BC}}={tex(dubc / 1e3, 3)}\ \text{{kJ}}", dubc, "J")\
     .responde("B", r"0", 0.0, "J")\
     .responde("C", rf"\Delta U_{{AB}}={tex(duab / 1e3, 3)}\ \text{{kJ}}", duab, "J")\
     .responde("D", rf"\gamma={tex(gamma, 3)}", gamma, "")
    return L.usa(r"\Delta U=Q-W", r"pV^\gamma=\text{cte}")


# ============================================================================ Q097
@leccion("Q097")
def q097() -> Leccion:
    pa, va, ta = 1.00 * ATM, 1.90e-3, 298.0
    n_mol = pa * va / (R_GAS * ta)
    g = Gas(n_mol, "dia")
    g.pone("A", p=pa, v=va, t=ta)
    vb = va / 10.0
    pb = pa * (va / vb) ** g.g
    g.pone("B", p=pb, v=vb)
    vc = 2.0 * vb
    g.pone("C", p=pb, v=vc)
    pd = pb * (vc / va) ** g.g
    g.pone("D", p=pd, v=va)
    r_bc = g.proceso("B", "C", "isobarico")
    r_da = g.proceso("D", "A", "isocorico")
    qh, qc = r_bc["Q"], r_da["Q"]
    ef = (qh + qc) / qh
    ds_bc = g.n * g.cp * math.log(g.t("C") / g.t("B"))
    ds_da = g.n * g.cv * math.log(g.t("A") / g.t("D"))
    L = _des("Q097", "Ciclo Diésel completo (moles, estados, calores, eficiencia y ΔS)",
             "Un motor Diésel (gas ideal diatómico) inicia en $A$ ($1{,}00$ atm, $1{,}90$ L, $298$ K) con $V_A=10V_B$ y $V_C=2V_B$: $A\\to B$ y $C\\to D$ adiabáticos, $B\\to C$ isobárico, $D\\to A$ isocórico. A) $n$. B) Estados $B, C, D$. C) $Q_H$ y $Q_C$. D) Eficiencia. E) Demuestre $\\Delta S_{\\text{ciclo}}=0$.",
             claves=["ciclo Diésel", "adiabático", "eficiencia"],
             visual=visual_pv(g, [("A", "B", "adiabatico"), ("B", "C", "isobarico"), ("C", "D", "adiabatico"), ("D", "A", "isocorico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM))
    L.paso("A y B) Moles y tabla de los cuatro estados")\
        .di(f"Con los datos del estado A hallamos los moles: {nu(n_mol, 'mol', 3)}.", DATO("1,90 L"),
            M(rf"n = \frac{{p_A V_A}}{{RT_A}} = {tex(n_mol, 3, u='mol', sci='si')}"))\
        .di("Aplicamos las relaciones adiabáticas en A a B y en C a D con gama igual a uno coma cuatro.",
            M(rf"p_B = p_C = (1{{,}}00)(10)^{{1{{,}}4}} = {tex(pb / ATM, 3, u='atm')}\qquad p_D = p_C\left(\frac{{0{{,}}38}}{{1{{,}}90}}\right)^{{1{{,}}4}} = {tex(pd / ATM, 3, u='atm')}"),
            tabla_estados(g, ["A", "B", "C", "D"], up="atm", uv="L", fp=1.0 / ATM, fv=1e3, cp=3, cv=3, dt=0))
    L.paso("C y D) Calor suministrado, expulsado y eficiencia")\
        .di("El calor solo entra en el tramo isobárico B a C y solo sale en el isocórico D a A.", V(foco="BC"),
            M(rf"Q_H = nC_p(T_C-T_B) = {tex(qh, 4, u='J')}"),
            M(rf"Q_C = nC_V(T_A-T_D) = {tex(qc, 3, u='J')}"))\
        .di(f"La eficiencia térmica resulta {n(ef * 100, 3)} por ciento.", V(foco="ciclo"),
            F(r"e = 1 - \frac{|Q_C|}{Q_H}"),
            R(rf"Q_H={tex(qh, 4, u='J')}\quad Q_C={tex(qc, 3, u='J')}\quad e={tex(ef, 3)} = {tex(ef * 100, 3)}\,\%", "rcd"))
    L.paso("E) Verificar que ΔS del ciclo es cero")\
        .di("Los dos tramos adiabáticos reversibles tienen entropía cero; los otros dos son opuestos y se cancelan.",
            M(rf"\Delta S_{{BC}} = nC_p\ln\frac{{T_C}}{{T_B}} = +{tex(ds_bc, 3, u='J/K')}"),
            M(rf"\Delta S_{{DA}} = nC_V\ln\frac{{T_A}}{{T_D}} = {tex(ds_da, 3, u='J/K')}"),
            R(r"\Delta S_{\text{ciclo}} = 0 + 1{,}57 + 0 - 1{,}57 = 0", "re"))
    L.responde("A", rf"n={tex(n_mol, 3, sci='si')}\ \text{{mol}}", n_mol, "mol")\
     .responde("QH", rf"Q_H={tex(qh, 4)}\ \text{{J}}", qh, "J")\
     .responde("QC", rf"Q_C={tex(qc, 3)}\ \text{{J}}", qc, "J")\
     .responde("e", rf"e={tex(ef, 3)}", ef, "")
    return L.oficial("III Parcial extraordinario II-2024", 7.77e-2, 1692.0, -789.0, 0.534).usa(r"pV^\gamma=\text{cte}", r"e=1-\frac{|Q_C|}{Q_H}")


# ============================================================================ Q098
@leccion("Q098")
def q098() -> Leccion:
    g = Gas(1.00, "dia")
    g.pone("A", p=25.0 * ATM, t=600.0)
    g.pone("C", p=1.00 * ATM, t=400.0)
    # B on Th=600 connected adiabatically to C: TB VB^(g-1) = TC VC^(g-1)
    vb = g.v("C") * (400.0 / 600.0) ** (1.0 / (g.g - 1.0))
    g.pone("B", v=vb, t=600.0)
    vd = g.v("A") * (600.0 / 400.0) ** (1.0 / (g.g - 1.0))
    g.pone("D", v=vd, t=400.0)
    rab = g.proceso("A", "B", "isotermico")
    rbc = g.proceso("B", "C", "adiabatico")
    rcd = g.proceso("C", "D", "isotermico")
    rda = g.proceso("D", "A", "adiabatico")
    res = [("A→B", rab), ("B→C", rbc), ("C→D", rcd), ("D→A", rda)]
    wnet = sum(r["W"] for _, r in res)
    ef = 1.0 - 400.0 / 600.0
    L = _des("Q098", "Ciclo de Carnot (1,00 mol diatómico entre 600 K y 400 K)",
             "Un mol de gas ideal diatómico recorre un ciclo de Carnot con $A$ ($25{,}0$ atm, $600$ K) y $C$ ($1{,}00$ atm, $400$ K): $A\\to B$ y $C\\to D$ isotérmicos, $B\\to C$ y $D\\to A$ adiabáticos. A) $p$ y $V$ en A, B, C y D. B) Trabajo neto. C) Eficiencia.",
             claves=["Carnot", "adiabático", "isotérmico"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "adiabatico"), ("C", "D", "isotermico"), ("D", "A", "adiabatico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("A) Presiones y volúmenes en los cuatro vértices")\
        .di("Primero sacamos los volúmenes en A y en C con el gas ideal.", DATO("25,0 atm"),
            M(rf"V_A = \frac{{nRT_A}}{{p_A}} = {tex(g.v('A') * 1e3, 3, u='L')}\qquad V_C = \frac{{nRT_C}}{{p_C}} = {tex(g.v('C') * 1e3, 3, u='L')}"))\
        .di("Con la relación adiabática temperatura por volumen a gama menos uno hallamos B y D.",
            F(r"T_1 V_1^{\gamma-1} = T_2 V_2^{\gamma-1}"),
            tabla_estados(g, ["A", "B", "C", "D"], up="atm", uv="L", fp=1.0 / ATM, fv=1e3, cp=3, cv=3, dt=0))
    L.paso("B y C) Trabajo neto y eficiencia de Carnot")\
        .di("Los trabajos adiabáticos de B a C y de D a A se cancelan exactamente; solo quedan los dos isotérmicos.", V(foco="ciclo"),
            tabla_quw(res),
            R(rf"W_{{\text{{neto}}}} = W_{{AB}} + W_{{CD}} = {tex(wnet, 4, u='J')}", "rb"))\
        .di(f"La eficiencia de Carnot entre seiscientos y cuatrocientos kelvin es {n(ef * 100, 3)} por ciento.",
            F(r"e = 1 - \frac{T_C}{T_H} = 1 - \frac{400}{600}"),
            R(rf"e = {tex(ef, 3)} = 33{{,}}3\,\%", "rc"))
    L.responde("Wnet", rf"W_{{\text{{neto}}}}={tex(wnet, 4)}\ \text{{J}}", wnet, "J").responde("e", rf"e={tex(ef, 3)}", ef, "")
    return L.oficial("III Parcial ordinario I-2025", 3024.0, 0.333, tol=0.015).usa(r"TV^{\gamma-1}=\text{cte}", r"e=1-\frac{T_C}{T_H}")


# ============================================================================ Q099
@leccion("Q099")
def q099() -> Leccion:
    g = Gas(0.15, "mono")
    g.pone("A", p=1.0 * ATM, v=9.0e-3)
    g.pone("B", p=1.0 * ATM, v=1.0e-3)
    g.pone("C", p=5.0 * ATM, v=1.0e-3)
    rab = g.proceso("A", "B", "isobarico")
    rbc = g.proceso("B", "C", "isocorico")
    rca = g.proceso("C", "A", "recta")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    du_ac = g.n * g.cv * (g.t("C") - g.t("A"))
    wnet = rab["W"] + rbc["W"] + rca["W"]
    qh = rbc["Q"] + rca["Q"]
    ef = wnet / qh
    L = _des("Q099", "Ciclo triangular de 0,15 mol de gas ideal monoatómico",
             "Con $0{,}15$ mol de gas monoatómico: $A(9\\ \\text{L}, 1\\ \\text{atm})\\to B(1\\ \\text{L}, 1\\ \\text{atm})\\to C(1\\ \\text{L}, 5\\ \\text{atm})\\to A$ (recta). A) Temperaturas en A, B y C. B) $W$ y $Q$ en cada etapa. C) $\\Delta U_{A\\to C}$. D) Eficiencia o rendimiento.",
             aviso="Ojo: el solucionario oficial de 25-II-E sustituyó 1,5 mol en vez de 0,15 mol en las temperaturas (10× menores) y omitió el signo menos en ΔU_AC; los calores, trabajos y la eficiencia 44,45 % sí coinciden porque n se cancela.",
             claves=["ciclo triangular", "monoatómico", "eficiencia"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "isocorico"), ("C", "A", "recta")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("A) Temperaturas en los tres vértices")\
        .di("Con cero coma quince moles hallamos las tres temperaturas desde la ecuación de estado.", DATO("0,15 mol"),
            tabla_estados(g, ["A", "B", "C"], up="atm", uv="L", fp=1.0 / ATM, fv=1e3, cp=2, cv=2, dt=1))
    explica_proceso(L.paso("B1) Compresión isobárica A→B"), g, "A", "B", "isobarico", foco="AB")
    explica_proceso(L.paso("B2) Calentamiento isocórico B→C"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("B3) Expansión en línea recta C→A"), g, "C", "A", "recta", foco="CA")
    L.paso("C y D) ΔU de A a C y eficiencia del motor")\
        .di(f"Como la energía interna es función de estado, solo restamos te C menos te A: da {nu(du_ac, 'J', 4)}.",
            M(rf"\Delta U_{{AC}} = nC_V(T_C - T_A) = {tex(du_ac, 4, u='J')}"),
            R(rf"\Delta U_{{AC}} = {tex(du_ac, 4, u='J')}", "rc"))\
        .di(f"El ciclo gira en sentido horario con trabajo neto positivo: es un motor con eficiencia de {n(ef * 100, 4)} por ciento.", V(foco="ciclo"),
            tabla_quw(res),
            R(rf"e = \frac{{{tex(wnet, 4)}}}{{{tex(qh, 4)}}} = {tex(ef * 100, 4)}\,\%", "rd"))
    L.responde("TA", rf"T_A={tex(g.t('A'), dec=1)}\ \text{{K}}", g.t("A"), "K")\
     .responde("dUAC", rf"\Delta U_{{AC}}={tex(du_ac, 4)}\ \text{{J}}", du_ac, "J")\
     .responde("e", rf"e={tex(ef, 4)}", ef, "")
    return L.oficial("III Parcial extraordinario II-2025", 73.11, 607.8, 0.4445,
                     nota="El solucionario oficial usó n=1,5 mol por error (dando T 10× menor) y omitió el signo negativo en ΔU_AC; Q, W y e=44,45 % sí coinciden.").usa(r"pV=nRT", r"e=\frac{W}{Q_H}")


# ============================================================================ Q100
@leccion("Q100")
def q100() -> Leccion:
    g = Gas(1.50, "mono")
    g.pone("A", v=0.0100, t=400.0)
    g.pone("B", v=0.0300, t=400.0)
    g.pone("C", v=0.0300, p=g.p("A") / 5.0)
    g.pone("D", v=0.0100, t=g.t("C"))
    rab = g.proceso("A", "B", "isotermico")
    rbc = g.proceso("B", "C", "isocorico")
    rcd = g.proceso("C", "D", "isotermico")
    rda = g.proceso("D", "A", "isocorico")
    res = [("A→B", rab), ("B→C", rbc), ("C→D", rcd), ("D→A", rda)]
    wnet = sum(r["W"] for _, r in res)
    qh = rab["Q"] + rda["Q"]
    ef = wnet / qh
    L = _des("Q100", "Ciclo de Stirling (dos isotermas y dos isocoras, 1,50 mol)",
             "Con $1{,}50$ mol de gas monoatómico: $A(0{,}0100\\ \\text{m}^3, 400{,}0\\ \\text{K})\\to B(3V_A)$ isotérmico; $B\\to C$ isocórico hasta $5p_C=p_A$; $C\\to D(V_A)$ isotérmico; $D\\to A$ isocórico. A) Diagrama $p$-$V$. B) $W$ y $Q$ por proceso. C) $W_{\\text{neto}}$ y tipo de dispositivo. D) Eficiencia.",
             claves=["Stirling", "isotermas e isocoras", "monoatómico"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "isocorico"), ("C", "D", "isotermico"), ("D", "A", "isocorico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("A) Tabla de los cuatro estados")\
        .di("Como cinco pe C igual a pe A y el volumen se triplica, la temperatura baja de cuatrocientos a doscientos cuarenta kelvin.", DATO("5PC = PA"),
            tabla_estados(g, ["A", "B", "C", "D"], up="kPa", uv="L", fp=1e-3, fv=1e3, cp=4, cv=3))
    explica_proceso(L.paso("B1) Expansión isotérmica A→B"), g, "A", "B", "isotermico", foco="AB")
    explica_proceso(L.paso("B2) Enfriamiento isocórico B→C"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("B3) Compresión isotérmica C→D"), g, "C", "D", "isotermico", foco="CD")
    explica_proceso(L.paso("B4) Calentamiento isocórico D→A"), g, "D", "A", "isocorico", foco="DA")
    L.paso("C y D) Trabajo neto, tipo de dispositivo y eficiencia")\
        .di(f"El trabajo neto es positivo, {nu(wnet, 'J', 4)}: opera como máquina térmica con eficiencia de {n(ef * 100, 4)} por ciento.", V(foco="ciclo"),
            tabla_quw(res),
            R(rf"W_{{\text{{neto}}}}={tex(wnet, 4, u='J')}\ (\text{{motor}})\qquad e = \frac{{{tex(wnet, 4)}}}{{{tex(qh, 4)}}} = {tex(ef * 100, 4)}\,\%", "rcd"))
    L.responde("Wnet", rf"W_{{\text{{neto}}}}={tex(wnet, 4)}\ \text{{J}}", wnet, "J").responde("e", rf"e={tex(ef, 4)}", ef, "")
    return L.oficial("III Parcial ordinario II-2025", 2192.0, 0.2587).usa(r"W=nRT\ln\frac{V_2}{V_1}", r"e=\frac{W}{Q_H}")


# ============================================================================ Q101
@leccion("Q101")
def q101() -> Leccion:
    L = _sel("Q101", "Compresión adiabática de un gas ideal",
             "Durante la compresión adiabática de un gas ideal en un sistema cilindro-pistón, se puede afirmar que:",
             {"A": "El trabajo realizado por el gas es cero.", "B": "El trabajo realizado por el gas es negativo.",
              "C": "El cambio de energía interna del gas es cero.", "D": "El cambio de energía interna del gas es negativo."}, "B",
             claves=["compresión adiabática", "signo del trabajo"])
    L.paso("Signos en compresión adiabática")\
        .di("Como el volumen disminuye, el trabajo hecho por el gas es negativo; sin intercambio de calor, la energía interna aumenta.", DATO("compresión adiabática"),
            M(r"V_f < V_i \;\Rightarrow\; W < 0"),
            F(r"\Delta U = Q - W = 0 - W = -W > 0"))
    revisar_opciones(L, {
        "A": ("La A sería proceso isocórico, no compresión.", "$W \\neq 0$."),
        "C": ("La C sería proceso isotérmico; aquí el gas se calienta.", "$\\Delta U > 0$."),
        "D": ("La D tiene el signo al revés: al comprimir adiabáticamente, sube la energía interna.", "$\\Delta U = -W > 0$."),
        "B": ("La B es la correcta: el trabajo realizado por el gas es negativo.", "$W < 0$."),
    })
    return L.oficial("III Parcial extraordinario I-2026", "B").usa(r"\Delta U=Q-W")


# ============================================================================ Q102
@leccion("Q102")
def q102() -> Leccion:
    n_mol, T, pf = 2.50, 420.0, 120e3
    vf = n_mol * R_GAS * T / pf
    vi = vf / 3.0
    L = _sel("Q102", "Volumen inicial tras triplicar volumen isotérmicamente",
             "Tras una expansión isotérmica a $420$ K de $2{,}50$ mol de gas ideal, el volumen inicial se triplica y la presión final es $120$ kPa. El volumen inicial es:",
             {"A": "8,08 L", "B": "24,3 L", "C": "72,9 L", "D": "87,3 L"}, "B",
             claves=["isotérmica", "gas ideal"])
    L.paso("Hallar volumen final y dividir entre 3")\
        .di(f"Hallamos el volumen final con la ley de gases ideales: da {nu(vf * 1e3, 'L', 3)}.", DATO("se triplica"),
            F(r"V_f = \frac{nRT}{p_f} = \frac{(2{,}50)(8{,}314)(420)}{120\times 10^3} = 72{,}75\ \text{L}"))\
        .di(f"Como se triplicó, dividimos entre tres y da {nu(vi * 1e3, 'L', 3)}.",
            R(rf"V_i = \frac{{V_f}}{{3}} = {tex(vi * 1e3, 3, u='L')}"))
    revisar_opciones(L, {
        "A": ("La A dividió dos veces entre tres.", "Solo se divide una vez."),
        "C": ("La C es el volumen final, no el inicial.", "Faltó dividir entre $3$."),
        "D": ("La D no corresponde con los datos.", "Cálculo erróneo."),
        "B": ("La B es la correcta: veinticuatro coma tres litros.", "$V_i \\approx 24{,}3$ L."),
    })
    return L.oficial("III Parcial extraordinario I-2026", "B").usa(r"pV=nRT")


# ============================================================================ Q103
@leccion("Q103")
def q103() -> Leccion:
    g = Gas(0.300, "dia")
    g.pone("A", p=1.50e5, t=300.0)
    g.pone("B", p=1.50e5, v=2.50 * g.v("A"))
    g.pone("C", v=g.v("A"), t=g.t("B"))
    rab = g.proceso("A", "B", "isobarico")
    rbc = g.proceso("B", "C", "isotermico")
    rca = g.proceso("C", "A", "isocorico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    wnet = rab["W"] + rbc["W"] + rca["W"]
    qc = rab["Q"]
    K = abs(qc) / abs(wnet)
    L = _des("Q103", "Ciclo refrigerador con 0,300 mol de nitrógeno (N₂)",
             "Con $0{,}300$ mol de $\\text{N}_2$ (diatómico) desde $A$ ($1{,}50\\times 10^5$ Pa, $300$ K): $A\\to B$ expansión isobárica a $2{,}50V_A$; $B\\to C$ compresión isotérmica a $V_A$; $C\\to A$ enfriamiento isocórico. A) Diagrama $p$-$V$. B) Tabla $(p,V,T)$. C) Calor por proceso. D) Trabajo por proceso. E) Eficiencia o rendimiento.",
             claves=["nitrógeno", "refrigerador", "ciclo antihorario"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "isotermico"), ("C", "A", "isocorico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("A y B) Tabla de los tres estados")\
        .di("Hallamos el volumen inicial y multiplicamos volumen y temperatura por dos coma cinco en B; luego B a C mantiene setecientos cincuenta kelvin.", DATO("0,300 mol de nitrógeno"),
            tabla_estados(g, ["A", "B", "C"], up="kPa", uv="L", fp=1e-3, fv=1e3, cp=4, cv=4))
    explica_proceso(L.paso("C1 y D1) Expansión isobárica A→B"), g, "A", "B", "isobarico", foco="AB")
    explica_proceso(L.paso("C2 y D2) Compresión isotérmica B→C"), g, "B", "C", "isotermico", foco="BC")
    explica_proceso(L.paso("C3 y D3) Enfriamiento isocórico C→A"), g, "C", "A", "isocorico", foco="CA")
    L.paso("E) Refrigerador: coeficiente de rendimiento K")\
        .di(f"El ciclo es antihorario y el trabajo neto es negativo ({nu(wnet, 'J', 3)}): es un refrigerador.", V(foco="ciclo"),
            tabla_quw(res),
            F(r"K = \frac{|Q_C|}{|W_{\text{neto}}|}"),
            R(rf"K = \frac{{{tex(qc, 4)}}}{{{tex(abs(wnet), 3)}}} = {tex(K, 3)}", "re"))
    L.responde("QAB", rf"Q_{{AB}}={tex(rab['Q'], 4)}\ \text{{J}}", rab["Q"], "J")\
     .responde("WBC", rf"W_{{BC}}={tex(rbc['W'], 4)}\ \text{{J}}", rbc["W"], "J")\
     .responde("K", rf"K={tex(K, 3)}", K, "")
    return L.oficial("III Parcial extraordinario I-2026", 3928.0, -1713.0, 6.65).usa(r"pV=nRT", r"K=\frac{|Q_C|}{|W|}")


# ============================================================================ Q104
@leccion("Q104")
def q104() -> Leccion:
    L = _sel("Q104", "Expansión isotérmica de un gas ideal",
             "Durante la expansión isotérmica de un gas ideal en un sistema cilindro-pistón, se puede afirmar que:",
             {"A": "El trabajo realizado por el gas es cero.", "B": "El trabajo realizado por el gas es negativo.",
              "C": "El cambio de energía interna del gas es cero.", "D": "El cambio de energía interna del gas es positivo."}, "C",
             claves=["expansión isotérmica", "energía interna"])
    L.paso("Temperatura constante implica ΔU = 0")\
        .di("En un gas ideal la energía interna solo depende de la temperatura: si no cambia la temperatura, delta U es cero.", DATO("expansión isotérmica"),
            F(r"\Delta T = 0 \;\Rightarrow\; \Delta U = nC_V\Delta T = 0"))
    revisar_opciones(L, {
        "A": ("La A es falsa: al expandirse el gas sí hace trabajo positivo.", "$W > 0$."),
        "B": ("La B tiene el signo al revés: en expansión el trabajo es positivo.", "$W > 0$."),
        "D": ("La D falla porque la temperatura no sube.", "$\\Delta T = 0$."),
        "C": ("La C es la correcta: el cambio de energía interna es cero.", "$\\Delta U = 0$."),
    })
    return L.oficial("III Parcial ordinario I-2026", "C").usa(r"\Delta U=nC_V\Delta T")


# ============================================================================ Q105
@leccion("Q105")
def q105() -> Leccion:
    g = Gas(2.00, "mono")
    g.pone("A", p=2.00e5, v=0.0200)
    g.pone("B", p=2.00e5, v=0.0500)
    g.pone("C", p=0.800e5, v=0.0500)
    rab = g.proceso("A", "B", "isobarico")
    rbc = g.proceso("B", "C", "isocorico")
    rca = g.proceso("C", "A", "isotermico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    L = _des("Q105", "Ciclo de 2,00 mol de helio (isobárico, isocórico e isotérmico)",
             "Con $2{,}00$ mol de helio (monoatómico): $A(0{,}0200\\ \\text{m}^3, 2{,}00\\times 10^5\\ \\text{Pa})\\to B(0{,}0500, 2{,}00\\times 10^5)$ isobárico; $B\\to C(0{,}0500, 0{,}800\\times 10^5)$ isocórico; $C\\to A$ isotérmico. A) Temperaturas en A, B y C. B) Trabajo por proceso. C) $\\Delta U$ en cada etapa.",
             claves=["helio", "monoatómico", "ciclo de tres etapas"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "isocorico"), ("C", "A", "isotermico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("A) Temperaturas en los tres puntos")\
        .di("Despejamos la temperatura de la ecuación de estado; como C a A es isotérmico, te C iguala a te A.", DATO("2,00 mol de helio"),
            tabla_estados(g, ["A", "B", "C"], up="kPa", uv="L", fp=1e-3, fv=1e3, cp=3, cv=3, dt=2))
    explica_proceso(L.paso("B1 y C1) Expansión isobárica A→B"), g, "A", "B", "isobarico", foco="AB")
    explica_proceso(L.paso("B2 y C2) Enfriamiento isocórico B→C"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("B3 y C3) Compresión isotérmica C→A"), g, "C", "A", "isotermico", foco="CA")
    L.paso("Resumen del ciclo")\
        .di("El resumen confirma que la suma de los cambios de energía interna vuelve exactamente a cero.", V(foco="ciclo"),
            tabla_quw(res))
    L.responde("TA", rf"T_A={tex(g.t('A'), dec=2)}\ \text{{K}}", g.t("A"), "K")\
     .responde("TB", rf"T_B={tex(g.t('B'), dec=2)}\ \text{{K}}", g.t("B"), "K")\
     .responde("WAB", rf"W_{{AB}}={tex(rab['W'], 4)}\ \text{{J}}", rab["W"], "J")\
     .responde("WCA", rf"W_{{CA}}={tex(rca['W'], 4)}\ \text{{J}}", rca["W"], "J")\
     .responde("dUAB", rf"\Delta U_{{AB}}={tex(rab['U'], 4)}\ \text{{J}}", rab["U"], "J")
    return L.oficial("III Parcial ordinario I-2026", 240.55, 601.40, 6000.0, -3665.0, 9000.0).usa(r"pV=nRT", r"\Delta U=nC_V\Delta T")
