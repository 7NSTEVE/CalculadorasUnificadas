"""Superexamen Q092–Q105: procesos y ciclos termodinámicos (primera parte)."""

from __future__ import annotations

import math

from .ayudas import ATM, R_GAS, R_LATM, T0, Gas, explica_proceso, revisar_opciones, tabla_estados, tabla_quw, valores_visual, visual_pv, estado_T, estado_V, estado_p, cierre_ciclo, deriva_entropia, _par_T
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, T, TABLA, V, s
from .formato import dice, n, nu, tex
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
             aviso="El inciso D (entropía) no entra en el III parcial: el corte llega hasta la sección 20.5. Los incisos A, B y C sí entran.",
             claves=["isocórico", "isobárico", "gas diatómico"],
             visual=visual_pv(g, [("1", "2", "isocorico"), ("2", "3", "isobarico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Dos procesos: primero se calienta sin cambiar el volumen y después se expande a presión constante.", DATO("2,50 moles"),
            LISTA(["$n=2{,}50$ mol, diatómico", "$1$: $1{,}00$ atm $=101\\,325$ Pa, $20{,}0\\ ^\\circ$C $=293{,}15$ K", "$1\\to2$: $V$ constante, $Q_{12}=1{,}52\\times10^4$ J", "$2\\to3$: $p$ constante, $V_3=3V_1$"]))\
        .di("Diatómico: de la hoja, la capacidad a volumen constante.", F(r"C_V=\frac{5}{2}R"), M(rf"C_V=\frac{{5}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))
    estado_V(L.paso("B) Estado 1"), g, "1", primero=True, voz="Despejamos el volumen del gas ideal y lo calculamos en 1.", dec_T=2)
    L.paso("B) Estado 2: el calor a volumen constante")\
        .di("A volumen constante, el calor de la hoja va con ce ve.".replace("ce ve", dice("C_V", "ce ve")), V(foco="12"), F(r"Q=nC_V\,\Delta T"))\
        .di("Despejamos la temperatura final.", M(r"Q_{12}=nC_V(T_2-T_1)\;\Rightarrow\;T_2=T_1+\frac{Q_{12}}{nC_V}"))\
        .di("Sustituimos.", M(rf"T_2=293{{,}}15+\frac{{1{{,}}52\times10^4}}{{(2{{,}}50)({tex(g.cv, 5)})}}=293{{,}}15+{tex(q12 / (g.n * g.cv), 4)}"))\
        .di(f"Te dos vale {nu(t2, 'K', 4)}.".replace("Te dos", dice("T₂", "Te dos")), M(rf"T_2={tex(t2, 4, u='K')}", "t2", "verde"))\
        .di("El volumen no cambia.", M(rf"V_2=V_1={tex(g.v('1'), 4, u='m^3')}"))
    estado_p(L.paso("B) Presión de 2"), g, "2", voz="Con el gas ideal, la presión de 2.", primero=True)
    L.paso("B) Estado 3: expansión a presión constante")\
        .di("La presión se queda y el volumen se triplica.", V(foco="23"), M(rf"p_3=p_2={tex(g.p('2'), 4, u='Pa')}\qquad V_3=3V_1={tex(g.v('3'), 4, u='m^3')}"))\
        .di("Del gas ideal, a presión constante volumen y temperatura van juntos.", F(r"pV=nRT"), M(r"\frac{V_3}{T_3}=\frac{V_2}{T_2}\;\Rightarrow\;T_3=T_2\,\frac{V_3}{V_2}=3T_2"))\
        .di("Sustituimos.", M(rf"T_3=3({tex(t2, 4)})={tex(g.t('3'), 4, u='K')}"))
    explica_proceso(L.paso("C) Calentamiento isocórico 1→2"), g, "1", "2", "isocorico", foco="12", dec_T=2)
    explica_proceso(L.paso("C) Expansión isobárica 2→3"), g, "2", "3", "isobarico", foco="23", dec_T=1)
    cierre_ciclo(L, res, titulo="C) Resumen de los dos procesos", solo_totales=True)
    deriva_entropia(L.paso("D) Entropía: de dónde salen las fórmulas (no entra en el parcial)"), "cv", "cp")
    L.paso("D) Cambio de entropía en cada etapa")\
        .di("A volumen constante.", V(foco="12"), M(rf"\Delta S_{{12}} = nC_V\ln\left(\frac{{T_2}}{{T_1}}\right)=(2{{,}}50)({tex(g.cv, 5)})\ln\left(\frac{{{tex(t2, 4)}}}{{293{{,}}15}}\right)"))\
        .di("Calculamos.", M(rf"\Delta S_{{12}}=({tex(g.n * g.cv, 4)})({tex(math.log(t2 / g.t('1')), 4)})={tex(ds12, 4, u='J/K')}"))\
        .di("A presión constante, con la temperatura que se triplica.", V(foco="23"), M(rf"\Delta S_{{23}} = nC_p\ln\left(\frac{{T_3}}{{T_2}}\right)=(2{{,}}50)({tex(g.cp, 5)})\ln 3"))\
        .di("Calculamos.", M(rf"\Delta S_{{23}}=({tex(g.n * g.cp, 4)})({tex(math.log(3.0), 4)})={tex(ds23, 4, u='J/K')}"),
            R(rf"\Delta S_{{12}}={tex(ds12, 4, u='J/K')}\qquad \Delta S_{{23}}={tex(ds23, 4, u='J/K')}", "rd"))
    L.responde("T2", rf"T_2={tex(t2, dec=1)}\ \text{{K}}", t2, "K")\
     .responde("Q23", rf"Q_{{23}}={tex(r23['Q'] / 1e3, 4)}\ \text{{kJ}}", r23["Q"], "J")\
     .responde("dS23", rf"\Delta S_{{23}}={tex(ds23, 4)}\ \text{{J/K}}", ds23, "J/K")
    return L.usa(r"pV=nRT", r"Q=nC_V\Delta T", r"W=p(V_2-V_1)")

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
    L = _des("Q094", "Ciclo de dos etapas: línea recta y compresión isotérmica",
             "Diez moles de gas ideal diatómico van de $A$ ($1{,}00$ atm) a $B$ ($0{,}500$ atm, $300$ K) en línea recta en $p$-$V$, y regresan de $B$ a $A$ isotérmicamente. A) Diagrama $p$-$V$. B) $V_A$ y $V_B$. C) $Q, W, \\Delta U$. D) $Q_H, W_{\\text{neto}}, Q_C$ y eficiencia.",
             claves=["línea recta", "isotérmica", "dos etapas"],
             visual=visual_pv(g, [("A", "B", "recta"), ("B", "A", "isotermico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Dos etapas: de A a B en línea recta, y de B de regreso a A sin cambiar la temperatura.", DATO("comprime isotérmicamente"),
            LISTA(["$n=10{,}0$ mol, diatómico",
                   "$A$: $p_A=1{,}00$ atm $=101\\,325$ Pa",
                   "$B$: $p_B=0{,}500$ atm $=50\\,662{,}5$ Pa y $T_B=300$ K",
                   "$A\\to B$: recta en $p$-$V$ · $B\\to A$: isotérmico"]))\
        .di("Como de B a A la temperatura no cambia, A está a la misma temperatura que B.", V(foco="BA"), M(r"T_A=T_B=300\ \text{K}"))\
        .di("Diatómico: de la hoja, la capacidad a volumen constante.", F(r"C_V=\frac{5}{2}R"),
            M(rf"C_V=\frac{{5}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))
    p = L.paso("B) Volúmenes en A y en B")
    estado_V(p, g, "A", primero=True, voz="Despejamos el volumen del gas ideal y lo calculamos en A.", dec_T=0)
    estado_V(p, g, "B", dec_T=0)
    p.di("Comprobación: a la misma temperatura, la mitad de la presión da el doble del volumen.",
         M(r"p_AV_A=p_BV_B\;\Rightarrow\;V_B=V_A\,\frac{p_A}{p_B}=2V_A"),
         M(rf"V_B=2\,({tex(g.v('A'), 5)})={tex(g.v('B'), 4, u='m^3')}"))
    explica_proceso(L.paso("C1) Expansión en línea recta A→B"), g, "A", "B", "recta", foco="AB")
    explica_proceso(L.paso("C2) Compresión isotérmica B→A"), g, "B", "A", "isotermico", foco="BA")
    L.paso("D) ¿Motor o refrigerador?")\
        .di("De A a B el gas baja por la recta hacia la derecha y regresa por la curva de abajo hacia la izquierda: gira en sentido horario.", V(foco="ciclo"))\
        .di("En sentido horario el trabajo neto es positivo: es un motor, y lo que se pide es su eficiencia.",
            M(r"\text{horario}\;\Rightarrow\;W_{\text{neto}}>0\;\Rightarrow\;\text{motor}"))
    c = cierre_ciclo(L, res, titulo="D) Calor absorbido, rechazado, trabajo neto y eficiencia", id_e="rd")
    L.responde("VA", rf"V_A={tex(g.v('A'), 4)}\ \text{{m}}^3", g.v("A"), "m³")\
     .responde("Wnet", rf"W_{{\text{{neto}}}}={tex(c['W'], 4)}\ \text{{J}}", c["W"], "J")\
     .responde("e", rf"e={tex(c['e'], 3)}", c["e"], "")
    return L.usa(r"pV=nRT", r"W=\int_{V_1}^{V_2} p\,dV", r"W=nRT\ln\left(\frac{V_2}{V_1}\right)", r"e=\frac{W}{Q_H}")

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
    pa_atm, pb_atm = 1.0, 3.0
    va, vb, vc, vd = 0.20, 0.09, 0.40, 1.20
    pa, pb = pa_atm * ATM, pb_atm * ATM
    qbc, qda = 100e3, -150e3
    wbc = pb * (vc - vb)
    dubc = qbc - wbc
    wda = pa * (va - vd)
    duda = qda - wda
    duab = -(dubc + 0.0 + duda)
    wab = -duab
    num = pa * va - pb * vb
    gamma = 1.0 + num / wab
    gamma_pv = math.log(pb_atm / pa_atm) / math.log(va / vb)
    g = Gas(1.0, cv=R_GAS / (gamma - 1.0))
    g.pone("A", p=pa, v=va)
    g.pone("B", p=pb, v=vb)
    g.pone("C", p=pb, v=vc)
    g.pone("D", p=pa, v=vd)
    L = _des("Q096", "Ciclo de cuatro procesos con calores dados y cálculo de γ",
             "Un gas ideal recorre $A(0{,}20\\ \\text{m}^3;\\ 1{,}0\\ \\text{atm})\\to B(0{,}09;\\ 3{,}0)$ adiabático, $B\\to C(0{,}40;\\ 3{,}0)$ isobárico con $Q_{BC}=+100$ kJ, $C\\to D(1{,}20;\\ 1{,}0)$ isotérmico y $D\\to A$ isobárico con $Q_{DA}=-150$ kJ. A) $\\Delta U_{BC}$. B) $\\Delta U_{CD}$. C) $\\Delta U_{AB}$. D) $\\gamma=C_P/C_V$.",
             claves=["primera ley en ciclos", "adiabático", "gamma"],
             visual=visual_pv(g, [("A", "B", "adiabatico"), ("B", "C", "isobarico"), ("C", "D", "isotermico"), ("D", "A", "isobarico")], uv="m³", up="atm", fp=1.0 / ATM))
    L.paso("Qué nos dan y qué piden")\
        .di("No sabemos los moles ni qué gas es. Por eso todo se hace con la primera ley y con los datos de la figura.", DATO("100 kJ"),
            LISTA(["$A$: $0{,}20$ m³ y $1{,}0$ atm; $B$: $0{,}09$ m³ y $3{,}0$ atm", "$C$: $0{,}40$ m³ y $3{,}0$ atm; $D$: $1{,}20$ m³ y $1{,}0$ atm", "$Q_{BC}=+100$ kJ (entra), $Q_{DA}=-150$ kJ (sale)"]))\
        .di("Las presiones en atmósferas se pasan a pascales.", M(r"1{,}0\ \text{atm}=101\,325\ \text{Pa}\qquad 3{,}0\ \text{atm}=303\,975\ \text{Pa}"))
    L.paso("A) Trabajo en B→C (presión constante)")\
        .di("A presión constante, el trabajo es presión por cambio de volumen.", V(foco="BC"), F(r"W=p(V_2-V_1)"))\
        .di("Sustituimos.", M(r"W_{BC}=(303\,975)(0{,}40-0{,}09)"))\
        .di("Son unos noventa y cuatro kilojoules.", M(rf"W_{{BC}}={tex(wbc, 4, u='J')}", "wbc"))
    L.paso("A) ΔU en B→C con la primera ley")\
        .di("La primera ley.", F(r"\Delta U=Q-W"))\
        .di("Entran cien kilojoules y el gas entrega casi todo como trabajo.", M(rf"\Delta U_{{BC}}=100\,000-{tex(wbc, 5, sci='no')}"))\
        .di(f"Queda {nu(dubc / 1e3, 'kJ', 2)}.", R(rf"\Delta U_{{BC}}={tex(dubc / 1e3, 2, u='kJ')}", "ra"))
    L.paso("B) ΔU en C→D")\
        .di("De C a D la temperatura no cambia, y la energía interna depende solo de la temperatura.", V(foco="CD"), F(r"\Delta U=nC_V\,\Delta T"))\
        .di("Delta te es cero, así que todo es cero.".replace("Delta te", s("ΔT")), R(r"\Delta U_{CD}=nC_V(0)=0", "rb"))
    L.paso("C) ΔU en D→A")\
        .di("Primero el trabajo, otra vez a presión constante.", V(foco="DA"), M(r"W_{DA}=p_A(V_A-V_D)=(101\,325)(0{,}20-1{,}20)"), M(rf"W_{{DA}}={tex(wda, 4, u='J')}", "wda"))\
        .di("Primera ley, con el calor que sale.", M(rf"\Delta U_{{DA}}=Q_{{DA}}-W_{{DA}}=-150\,000-({tex(wda, 5, sci='no')})"), M(rf"\Delta U_{{DA}}={tex(duda / 1e3, 3, u='kJ')}", "dda"))
    L.paso("C) ΔU en A→B: el ciclo suma cero")\
        .di("En un ciclo completo el gas vuelve a su estado inicial: los cuatro cambios de energía interna suman cero.", V(foco="ciclo"), M(r"\Delta U_{AB}+\Delta U_{BC}+\Delta U_{CD}+\Delta U_{DA}=0", "cic", "azul"))\
        .di("Despejamos el que falta.", M(r"\Delta U_{AB}=-(\Delta U_{BC}+\Delta U_{CD}+\Delta U_{DA})"))\
        .di("Sustituimos, en kilojoules.", M(rf"\Delta U_{{AB}}=-({tex(dubc / 1e3, 3)}+0{tex(duda / 1e3, 3, signo=True)})"))\
        .di(f"Da {nu(duab / 1e3, 'kJ', 3)}.", R(rf"\Delta U_{{AB}}=+{tex(duab / 1e3, 3, u='kJ')}", "rc"))\
        .por_que("La energía interna es función de estado: depende solo de dónde está el gas, no del camino. Al volver a A, el total es cero.")
    L.paso("D) Trabajo en la adiabática A→B")\
        .di("En la adiabática no hay calor, así que el trabajo es menos el cambio de energía interna.", V(foco="AB"), F(r"\Delta U=Q-W"), M(r"\Delta U_{AB}=0-W_{AB}\;\Rightarrow\;W_{AB}=-\Delta U_{AB}"))\
        .di("El gas se comprime: el trabajo sale negativo.", M(rf"W_{{AB}}={tex(wab, 4, u='J')}", "wab"))
    L.paso("D) Despejar γ con el trabajo adiabático")\
        .di("El formulario da el trabajo adiabático con gama.", F(r"W=\frac{1}{\gamma-1}\left(p_1V_1-p_2V_2\right)"))\
        .di("Primero el paréntesis, con A como uno y B como dos.", M(rf"p_AV_A-p_BV_B=(101\,325)(0{{,}}20)-(303\,975)(0{{,}}09)={tex(num, 4, u='J')}"))\
        .di("Despejamos gama menos uno.".replace("gama menos uno", dice("γ−1", "gama menos uno")), M(r"\gamma-1=\frac{p_AV_A-p_BV_B}{W_{AB}}"), M(rf"\gamma-1=\frac{{{tex(num, 4)}}}{{{tex(wab, 4)}}}={tex(gamma - 1, 3)}"))\
        .di(f"Gama vale {n(gamma, 3)}.".replace("Gama", s("γ", "gama")), R(rf"\gamma={tex(gamma, 3)}", "rd"))
    L.paso("Ojo con un atajo que no sirve aquí")\
        .di("Si se usara pe por ve a la gama constante con los números redondeados de la figura, saldría otro valor.".replace("pe por ve a la gama", dice("pVᵞ", "pe ve a la gama")),
            M(rf"\gamma=\frac{{\ln(3{{,}}0/1{{,}}0)}}{{\ln(0{{,}}20/0{{,}}09)}}={tex(gamma_pv, 3)}"))\
        .di("La cátedra espera uno coma dieciséis, que sale de la energía de los incisos anteriores.", OJO("Los puntos de la figura no caen exactamente sobre una adiabática. La respuesta oficial, $\\gamma=1{,}16$, usa $W_{AB}=-\\Delta U_{AB}$."))
    L.responde("A", rf"\Delta U_{{BC}}={tex(dubc / 1e3, 2)}\ \text{{kJ}}", dubc / 1e3, "kJ").responde("B", r"\Delta U_{CD}=0", 0.0, "kJ")\
        .responde("C", rf"\Delta U_{{AB}}={tex(duab / 1e3, 3)}\ \text{{kJ}}", duab / 1e3, "kJ").responde("D", rf"\gamma={tex(gamma, 3)}", gamma, "")
    return L.oficial("III Parcial extraordinario I-2024", 5.8, 0.0, 42.9, 1.16, tol=0.02).usa(r"\Delta U=Q-W", r"W=p(V_2-V_1)", r"W=\frac{1}{\gamma-1}(p_1V_1-p_2V_2)")

# ============================================================================ Q097
@leccion("Q097")
def q097() -> Leccion:
    pa_atm, va_l, ta = 1.00, 1.90, 298.0
    nm = pa_atm * va_l / (R_LATM * ta)
    g = Gas(nm, "dia")
    gm = g.g
    vb_l, vc_l = va_l / 10.0, 2.0 * va_l / 10.0
    pb_atm = pa_atm * 10.0 ** gm
    tb = ta * 10.0 ** (gm - 1.0)
    tc = 2.0 * tb
    pd_atm = pb_atm * (vc_l / va_l) ** gm
    td = tc * (vc_l / va_l) ** (gm - 1.0)
    g.pone("A", p=pa_atm * ATM, v=va_l * 1e-3, t=ta)
    g.pone("B", p=pb_atm * ATM, v=vb_l * 1e-3, t=tb)
    g.pone("C", p=pb_atm * ATM, v=vc_l * 1e-3, t=tc)
    g.pone("D", p=pd_atm * ATM, v=va_l * 1e-3, t=td)
    qh = nm * g.cp * (tc - tb)
    qc = nm * g.cv * (ta - td)
    ef = 1.0 - abs(qc) / qh
    ds_bc = nm * g.cp * math.log(tc / tb)
    ds_da = nm * g.cv * math.log(ta / td)
    L = _des("Q097", "Ciclo Diésel completo (moles, estados, calores, eficiencia y ΔS)",
             "Motor Diésel con gas ideal diatómico. $A$: $1{,}00$ atm, $1{,}90$ L, $298$ K. $V_A=10V_B$ y $V_C=2V_B$. $A\\to B$ y $C\\to D$ adiabáticos, $B\\to C$ isobárico, $D\\to A$ isocórico. A) $n$. B) $p$, $V$, $T$ en B, C y D. C) Calor suministrado y expulsado. D) Eficiencia. E) Demuestre que $\\Delta S_{\\text{ciclo}}=0$.",
             claves=["ciclo Diésel", "adiabático", "eficiencia", "entropía"],
             visual=visual_pv(g, [("A", "B", "adiabatico"), ("B", "C", "isobarico"), ("C", "D", "adiabatico"), ("D", "A", "isocorico")], uv="L", up="atm", fv=1e3, fp=1.0 / ATM))
    L.paso("Qué nos dan y qué piden")\
        .di("Un ciclo Diésel: comprime, quema a presión constante, se expande y suelta calor a volumen constante.", DATO("1,90 L"),
            LISTA(["$A$: $p_A=1{,}00$ atm, $V_A=1{,}90$ L, $T_A=298$ K", "$V_A=10V_B$ y $V_C=2V_B$", "Diatómico: $\\gamma=1{,}40$", "$A\\to B$, $C\\to D$ adiabáticos; $B\\to C$ isobárico; $D\\to A$ isocórico"]))\
        .di("Primero todos los volúmenes, que salen directo de las relaciones.", M(r"V_B=\frac{V_A}{10}=0{,}190\ \text{L}\qquad V_C=2V_B=0{,}380\ \text{L}"), M(r"V_D=V_A=1{,}90\ \text{L}\quad(\text{D}\to\text{A es isocórico})", "vd", "azul"))
    L.paso("A) Número de moles")\
        .di("Gas ideal en el estado A, con erre en litros atmósfera.".replace("erre", s("R")), F(r"pV=nRT"), M(r"n=\frac{p_AV_A}{RT_A}=\frac{(1{,}00)(1{,}90)}{(0{,}08206)(298)}"))\
        .di(f"Son {nu(nm, 'mol', 3)}.", R(rf"n={tex(nm, 3, u='mol')}", "ra"))
    L.paso("B) Estado B: compresión adiabática")\
        .di("En la adiabática se cumple pe ve a la gama constante.".replace("pe ve a la gama", dice("pVᵞ", "pe ve a la gama")), V(foco="AB"), F(r"pV^\gamma=\text{cte}"), M(r"p_BV_B^\gamma=p_AV_A^\gamma\;\Rightarrow\;p_B=p_A\left(\frac{V_A}{V_B}\right)^\gamma"))\
        .di("El cociente de volúmenes es diez.", M(rf"p_B=(1{{,}}00)(10)^{{1{{,}}40}}={tex(pb_atm, 3, u='atm')}", "pb"))\
        .di("Y la temperatura con te ve a la gama menos uno.".replace("te ve a la gama menos uno", dice("TVᵞ⁻¹", "te ve a la gama menos uno")), F(r"TV^{\gamma-1}=\text{cte}"), M(rf"T_B=T_A\left(\frac{{V_A}}{{V_B}}\right)^{{\gamma-1}}=(298)(10)^{{0{{,}}40}}={tex(tb, 4, u='K')}", "tb"))
    L.paso("B) Estado C: se calienta a presión constante")\
        .di("De B a C la presión no cambia.", V(foco="BC"), M(rf"p_C=p_B={tex(pb_atm, 3, u='atm')}"))\
        .di("A presión constante, volumen y temperatura van juntos: si el volumen se duplica, la temperatura también.", F(r"pV=nRT"), M(r"\frac{V_C}{T_C}=\frac{V_B}{T_B}\;\Rightarrow\;T_C=T_B\,\frac{V_C}{V_B}=2T_B"), M(rf"T_C=2({tex(tb, 4)})={tex(tc, 4, u='K')}", "tc"))
    L.paso("B) Estado D: expansión adiabática")\
        .di("De C a D otra vez adiabático, hasta el volumen de A.", V(foco="CD"), M(r"p_D=p_C\left(\frac{V_C}{V_D}\right)^\gamma=(25{,}1)\left(\frac{0{,}380}{1{,}90}\right)^{1{,}40}"), M(rf"p_D={tex(pd_atm, 3, u='atm')}", "pd"))\
        .di("Y su temperatura.", M(rf"T_D=T_C\left(\frac{{V_C}}{{V_D}}\right)^{{\gamma-1}}=({tex(tc, 4)})(0{{,}}200)^{{0{{,}}40}}={tex(td, 3, u='K')}", "td"))
    L.paso("B) Los cuatro estados")\
        .di("Resumen en una tabla.", tabla_estados(g, ["A", "B", "C", "D"], up="atm", uv="L", fp=1.0 / ATM, fv=1e3, cp=3, cv=3, dt=0), V(foco="ciclo"))
    L.paso("C) Calor suministrado (B→C)")\
        .di("El calor entra solo al quemar el combustible, a presión constante.", V(foco="BC"), F(r"Q=nC_p\,\Delta T"), M(r"C_p=C_V+R=\frac{5}{2}R+R=\frac{7}{2}R"))\
        .di("Sustituimos.", M(rf"Q_H=({tex(nm, 3)})\left(\frac{{7}}{{2}}\right)(8{{,}}314)({tex(tc, 4)}-{tex(tb, 4)})"))\
        .di(f"Entran {nu(qh, 'J', 4)}.", R(rf"Q_H={tex(qh, 4, u='J')}", "rch"))
    L.paso("C) Calor expulsado (D→A)")\
        .di("El calor sale solo de D a A, a volumen constante, donde no hay trabajo.", V(foco="DA"), F(r"Q=nC_V\,\Delta T"), M(rf"Q_C=({tex(nm, 3)})\left(\frac{{5}}{{2}}\right)(8{{,}}314)(298-{tex(td, 3)})"))\
        .di(f"Salen {nu(abs(qc), 'J', 3)}: por eso el signo menos.", R(rf"Q_C={tex(qc, 3, u='J')}", "rcc"))\
        .di("En las dos adiabáticas no hay calor.", M(r"Q_{AB}=Q_{CD}=0"))
    L.paso("D) Eficiencia")\
        .di("Eficiencia: uno menos lo que se bota entre lo que entra.", F(r"e=1-\left|\frac{Q_c}{Q_H}\right|"), M(rf"e=1-\frac{{{tex(abs(qc), 3)}}}{{{tex(qh, 4)}}}"))\
        .di(f"Casi cincuenta y tres por ciento.", R(rf"e={tex(ef, 3)}={tex(ef * 100, 3)}\,\%", "rd"))
    L.paso("E) Entropía: de dónde sale la fórmula")\
        .di("La entropía cambia con el calor dividido entre la temperatura a la que entra.", M(r"dS=\frac{dQ}{T}", "ds", "azul"))\
        .di("En las adiabáticas reversibles no hay calor: no cambia la entropía.", M(r"\Delta S_{AB}=\Delta S_{CD}=0"))\
        .di("A presión constante, de cu igual a ene ce pe de te; al integrar sale un logaritmo.".replace("de cu igual a ene ce pe de te", dice("dQ=nC_p dT", "de cu igual a ene ce pe de te")), F(r"dQ=nC_p\,dT"), M(r"\Delta S=\int_{T_1}^{T_2}\frac{nC_p\,dT}{T}=nC_p\ln\frac{T_2}{T_1}"))\
        .por_que("La fórmula de entropía no está en el formulario: se arma con $dS=dQ/T$ y el $dQ$ que sí está impreso.")
    L.paso("E) Sumar las entropías del ciclo")\
        .di("De B a C, a presión constante.", M(rf"\Delta S_{{BC}}=nC_p\ln\frac{{T_C}}{{T_B}}=({tex(nm, 3)})(29{{,}}10)\ln 2={tex(ds_bc, 3, signo=True, u='J/K')}"))\
        .di("De D a A, a volumen constante: igual pero con ce ve.".replace("ce ve", s("C_V")), M(rf"\Delta S_{{DA}}=nC_V\ln\frac{{T_A}}{{T_D}}=({tex(nm, 3)})(20{{,}}79)\ln\frac{{298}}{{{tex(td, 3)}}}={tex(ds_da, 3, u='J/K')}"))\
        .di("La suma da cero, como debe ser en un ciclo.", R(rf"\Delta S_{{\text{{ciclo}}}}=0+({tex(ds_bc, 3)})+0+({tex(ds_da, 3)})=0", "re"))
    L.responde("A", rf"n={tex(nm, 3)}\ \text{{mol}}", nm, "mol").responde("C", rf"Q_H={tex(qh, 4)}\ \text{{J}}", qh, "J").responde("C", rf"Q_C={tex(qc, 3)}\ \text{{J}}", qc, "J").responde("D", rf"e={tex(ef, 3)}", ef, "")
    return L.oficial("III Parcial extraordinario II-2024", 7.77e-2, 1692.0, -789.0, 0.53, tol=0.02).usa(r"pV=nRT", r"pV^\gamma=\text{cte}", r"TV^{\gamma-1}=\text{cte}", r"e=1-\left|\frac{Q_c}{Q_H}\right|")

# ============================================================================ Q098
@leccion("Q098")
def q098() -> Leccion:
    TH, TC = 600.0, 400.0
    g = Gas(1.00, "dia")
    g.pone("A", p=25.0 * ATM, t=TH)
    g.pone("C", p=1.00 * ATM, t=TC)
    k = 1.0 / (g.g - 1.0)
    vb = g.v("C") * (TC / TH) ** k
    g.pone("B", v=vb, t=TH)
    vd = g.v("A") * (TH / TC) ** k
    g.pone("D", v=vd, t=TC)
    VA, VB, VC, VD = (g.v(x) * 1e3 for x in "ABCD")
    pB, pD = g.p("B") / ATM, g.p("D") / ATM
    rab = g.proceso("A", "B", "isotermico")
    rbc = g.proceso("B", "C", "adiabatico")
    rcd = g.proceso("C", "D", "isotermico")
    rda = g.proceso("D", "A", "adiabatico")
    res = [("A→B", rab), ("B→C", rbc), ("C→D", rcd), ("D→A", rda)]
    wnet = sum(r["W"] for _, r in res)
    qh = rab["Q"]
    ef = wnet / qh
    L = _des("Q098", "Ciclo de Carnot (1,00 mol diatómico entre 600 K y 400 K)",
             "Un mol de gas ideal diatómico recorre un ciclo de Carnot. $A$: $25{,}0$ atm y $600$ K; $C$: $1{,}00$ atm y $400$ K. $A\\to B$ expansión isotérmica, $B\\to C$ expansión adiabática, $C\\to D$ compresión isotérmica, $D\\to A$ compresión adiabática. A) $p$ y $V$ en A, B, C y D. B) Trabajo neto. C) Eficiencia.",
             claves=["Carnot", "adiabático", "isotérmico"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "adiabatico"), ("C", "D", "isotermico"), ("D", "A", "adiabatico")], uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Dos isotermas, a seiscientos y a cuatrocientos kelvin, unidas por dos adiabáticas.", DATO("25,0 atm"),
            LISTA(["$n=1{,}00$ mol, **diatómico**: $\\gamma=1{,}40$", "$A$: $25{,}0$ atm, $600$ K", "$C$: $1{,}00$ atm, $400$ K"]))\
        .di("En una isoterma la temperatura no cambia: B está a seiscientos y D a cuatrocientos.", M(r"T_B=T_A=600\ \text{K}\qquad T_D=T_C=400\ \text{K}", "tt", "azul"))\
        .sale_de(("$T_B=600$ K", "A→B es isotérmico"), ("$T_D=400$ K", "C→D es isotérmico"))
    L.paso("A) Volúmenes en A y en C")\
        .di("Gas ideal. Con erre en litros atmósfera, el volumen sale en litros.".replace("erre", s("R")), F(r"pV=nRT"), M(r"V_A=\frac{nRT_A}{p_A}=\frac{(1{,}00)(0{,}08206)(600)}{25{,}0}"), M(rf"V_A={tex(VA, 3, u='L')}", "va"))\
        .di("Igual para C.", M(r"V_C=\frac{(1{,}00)(0{,}08206)(400)}{1{,}00}"), M(rf"V_C={tex(VC, 3, u='L')}", "vc"))
    L.paso("A) Volumen en B: B y C están en la misma adiabática")\
        .di("En una adiabática, te por ve a la gama menos uno se mantiene.".replace("te por ve a la gama menos uno", dice("TVᵞ⁻¹", "te ve a la gama menos uno")), V(foco="BC"), F(r"TV^{\gamma-1}=\text{cte}"), M(r"T_BV_B^{\gamma-1}=T_CV_C^{\gamma-1}"))\
        .di("Despejamos ve be: sacamos la raíz con exponente uno entre gama menos uno.".replace("ve be", s("V_B")), M(r"V_B=V_C\left(\frac{T_C}{T_B}\right)^{\frac{1}{\gamma-1}}"), M(r"\frac{1}{\gamma-1}=\frac{1}{0{,}40}=2{,}5"))\
        .di("Sustituimos.", M(rf"V_B=({tex(VC, 3)})\left(\frac{{400}}{{600}}\right)^{{2{{,}}5}}=({tex(VC, 3)})({tex((TC / TH) ** k, 4)})"), M(rf"V_B={tex(VB, 4, u='L')}", "vb"))\
        .despacio((r"T_BV_B^{0{,}4}=T_CV_C^{0{,}4}", "con $\\gamma-1=0{,}4$"), (r"V_B^{0{,}4}=V_C^{0{,}4}\,\frac{T_C}{T_B}", "se divide entre $T_B$"), (r"V_B=\left(V_C^{0{,}4}\,\frac{T_C}{T_B}\right)^{1/0{,}4}=V_C\left(\frac{T_C}{T_B}\right)^{2{,}5}", "se eleva todo a $1/0{,}4=2{,}5$"))
    L.paso("A) Presión en B")\
        .di("Gas ideal en B.", M(rf"p_B=\frac{{nRT_B}}{{V_B}}=\frac{{(1{{,}}00)(0{{,}}08206)(600)}}{{{tex(VB, 4)}}}"), R(rf"p_B={tex(pB, 3, u='atm')}", "pb"))
    L.paso("A) Volumen y presión en D: D y A están en la otra adiabática")\
        .di("Ahora entre D y A.", V(foco="DA"), M(r"T_DV_D^{\gamma-1}=T_AV_A^{\gamma-1}\;\Rightarrow\;V_D=V_A\left(\frac{T_A}{T_D}\right)^{2{,}5}"))\
        .di("Sustituimos.", M(rf"V_D=({tex(VA, 3)})\left(\frac{{600}}{{400}}\right)^{{2{{,}}5}}=({tex(VA, 3)})({tex((TH / TC) ** k, 4)})={tex(VD, 3, u='L')}", "vd"))\
        .di("Y su presión.", M(rf"p_D=\frac{{(1{{,}}00)(0{{,}}08206)(400)}}{{{tex(VD, 3)}}}"), R(rf"p_D={tex(pD, 3, u='atm')}", "pd"))
    L.paso("A) Los cuatro estados")\
        .di("Juntamos todo.", tabla_estados(g, ["A", "B", "C", "D"], up="atm", uv="L", fp=1.0 / ATM, fv=1e3, cp=3, cv=3, dt=0), V(foco="ciclo"))\
        .di("La solución oficial redondea un poquito distinto: cuatro coma quince y seis coma cero uno atmósferas.", NOTA("La oficial usa $R=0{,}082$; por eso da $4{,}15$ atm y $6{,}01$ atm."))
    explica_proceso(L.paso("B) Expansión isotérmica A→B"), g, "A", "B", "isotermico", foco="AB")
    explica_proceso(L.paso("B) Expansión adiabática B→C"), g, "B", "C", "adiabatico", foco="BC")
    explica_proceso(L.paso("B) Compresión isotérmica C→D"), g, "C", "D", "isotermico", foco="CD")
    explica_proceso(L.paso("B) Compresión adiabática D→A"), g, "D", "A", "adiabatico", foco="DA")
    L.paso("B) Trabajo neto")\
        .di("Sumamos los cuatro trabajos. Los de las adiabáticas son iguales y opuestos: se cancelan.", tabla_quw(res), V(foco="ciclo"))\
        .di("Queda la suma de los dos isotérmicos.", M(rf"W_{{\text{{neto}}}}={tex(rab['W'], 4)}+({tex(rcd['W'], 4)})"), R(rf"W_{{\text{{neto}}}}={tex(wnet, 4, u='J')}", "rw"))\
        .di("La oficial da tres mil veinticuatro porque redondeó antes; la diferencia es de uno por ciento.", NOTA("La oficial obtiene $3024$ J con $R=0{,}082$ y volúmenes redondeados. El valor exacto es $2993$ J."))
    L.paso("C) Eficiencia")\
        .di("El calor solo entra en la isoterma caliente, de A a B.", F(r"e=\frac{W}{Q_H}"), M(rf"Q_H=Q_{{AB}}={tex(qh, 4, u='J')}"))\
        .di("Dividimos.", M(rf"e=\frac{{{tex(wnet, 4)}}}{{{tex(qh, 4)}}}"), R(rf"e={tex(ef, 3)}={tex(ef * 100, 3)}\,\%", "re"))\
        .di("Da justo uno menos cuatrocientos entre seiscientos: así es siempre en un ciclo de Carnot.", NOTA("Comprobación: $1-\\frac{T_C}{T_H}=1-\\frac{400}{600}=0{,}333$. Esa fórmula no viene en el formulario: sale de $e=W/Q_H$ como aquí."))
    L.responde("A", rf"p_B={tex(pB, 3)}\ \text{{atm}}", pB, "atm").responde("A", rf"V_B={tex(VB, 4)}\ \text{{L}}", VB, "L").responde("A", rf"p_D={tex(pD, 3)}\ \text{{atm}}", pD, "atm").responde("A", rf"V_D={tex(VD, 3)}\ \text{{L}}", VD, "L")\
        .responde("B", rf"W_{{\text{{neto}}}}={tex(wnet, 4)}\ \text{{J}}", wnet, "J").responde("C", rf"e={tex(ef, 3)}", ef, "")
    return L.oficial("III Parcial ordinario I-2025", 4.15, 11.87, 6.01, 5.45, 3024.0, 0.333, tol=0.015, nota="La oficial redondea R y los volúmenes; las diferencias son menores al 1,5 %.")\
        .usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}", r"W=nRT\ln\left(\frac{V_2}{V_1}\right)", r"e=\frac{W}{Q_H}")

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
    sTa, sTc = _par_T(g.t("A"), g.t("C"), 1)
    dT = round(g.t("C"), 1) - round(g.t("A"), 1)
    L = _des("Q099", "Ciclo triangular de 0,15 mol de gas ideal monoatómico",
             "Con $0{,}15$ mol de gas monoatómico: $A(9\\ \\text{L}, 1\\ \\text{atm})\\to B(1\\ \\text{L}, 1\\ \\text{atm})\\to C(1\\ \\text{L}, 5\\ \\text{atm})\\to A$ (recta). A) Temperaturas en A, B y C. B) $W$ y $Q$ en cada etapa. C) $\\Delta U_{A\\to C}$. D) Eficiencia o rendimiento.",
             aviso="Ojo: el solucionario oficial de 25-II-E sustituyó 1,5 mol en vez de 0,15 mol en las temperaturas (10× menores) y omitió el signo menos en ΔU_AC; los calores, trabajos y la eficiencia 44,45 % sí coinciden porque n se cancela.",
             claves=["ciclo triangular", "monoatómico", "eficiencia"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "isocorico"), ("C", "A", "recta")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Tres etapas: comprimir a presión constante, calentar a volumen constante y volver en línea recta.", DATO("0,15 mol"),
            LISTA(["$n=0{,}15$ mol, monoatómico",
                   "$A$: $9$ L y $1$ atm · $B$: $1$ L y $1$ atm · $C$: $1$ L y $5$ atm",
                   "$A\\to B$: $p$ constante · $B\\to C$: $V$ constante · $C\\to A$: recta"]))\
        .di("Pasamos a unidades del SI: un litro es una milésima de metro cúbico.",
            M(r"1\ \text{L}=10^{-3}\ \text{m}^3"), M(r"1\ \text{atm}=101\,325\ \text{Pa}"),
            M(r"V_A=9{,}00\times10^{-3}\ \text{m}^3\qquad V_B=V_C=1{,}00\times10^{-3}\ \text{m}^3"),
            M(r"p_A=p_B=101\,325\ \text{Pa}\qquad p_C=5\,(101\,325)=506\,625\ \text{Pa}"))\
        .di("Monoatómico: de la hoja, la capacidad a volumen constante.", F(r"C_V=\frac{3}{2}R"),
            M(rf"C_V=\frac{{3}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))
    p = L.paso("A) Temperaturas en los tres vértices")
    estado_T(p, g, "A", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en A.")
    estado_T(p, g, "B")
    estado_T(p, g, "C")
    explica_proceso(L.paso("B1) Compresión isobárica A→B"), g, "A", "B", "isobarico", foco="AB")
    explica_proceso(L.paso("B2) Calentamiento isocórico B→C"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("B3) Expansión en línea recta C→A"), g, "C", "A", "recta", foco="CA")
    L.paso("C) ΔU de A a C")\
        .di("La energía interna solo depende de la temperatura: basta restar la temperatura de C menos la de A, sin importar el camino.",
            F(r"\Delta U=nC_V\,\Delta T"), M(r"\Delta U_{AC}=nC_V\,(T_C-T_A)"))\
        .di("Sustituimos.", M(rf"\Delta U_{{AC}}=({tex(g.n, 4, recorta=True)})({tex(g.cv, 5)})({sTc}-{sTa})"))\
        .di("Multiplicamos los dos primeros y restamos las temperaturas.", M(rf"\Delta U_{{AC}}=({tex(g.n * g.cv, 5)})({tex(dT, dec=1)})"))\
        .di(f"Da {nu(du_ac, 'J', 4)}: negativo, porque C está más frío que A.", R(rf"\Delta U_{{AC}}={tex(du_ac, 4, u='J')}", "rc"))
    L.paso("D) ¿Motor o refrigerador?")\
        .di("De A a B va hacia la izquierda por abajo, sube de B a C y baja en diagonal hasta A: gira en sentido horario.", V(foco="ciclo"))\
        .di("En sentido horario el trabajo neto es positivo: es un motor, y lo que se pide es su eficiencia.",
            M(r"\text{horario}\;\Rightarrow\;W_{\text{neto}}>0\;\Rightarrow\;\text{motor}"))
    c = cierre_ciclo(L, res, titulo="D) Trabajo neto, calores y eficiencia", id_e="rd")
    L.responde("TA", rf"T_A={tex(g.t('A'), dec=1)}\ \text{{K}}", g.t("A"), "K")\
     .responde("dUAC", rf"\Delta U_{{AC}}={tex(du_ac, 4)}\ \text{{J}}", du_ac, "J")\
     .responde("e", rf"e={tex(c['e'], 4)}", c["e"], "")
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
    L = _des("Q100", "Ciclo de Stirling (dos isotermas y dos isocoras, 1,50 mol)",
             "Con $1{,}50$ mol de gas monoatómico: $A(0{,}0100\\ \\text{m}^3, 400{,}0\\ \\text{K})\\to B(3V_A)$ isotérmico; $B\\to C$ isocórico hasta $5p_C=p_A$; $C\\to D(V_A)$ isotérmico; $D\\to A$ isocórico. A) Diagrama $p$-$V$. B) $W$ y $Q$ por proceso. C) $W_{\\text{neto}}$ y tipo de dispositivo. D) Eficiencia.",
             claves=["Stirling", "isotermas e isocoras", "monoatómico"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "isocorico"), ("C", "D", "isotermico"), ("D", "A", "isocorico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Dos isotermas y dos isocoras: es un ciclo de Stirling.", DATO("5PC = PA"),
            LISTA(["$n=1{,}50$ mol, monoatómico: $C_V=\\frac{3}{2}R$", "$A$: $V_A=0{,}0100$ m³, $T_A=400{,}0$ K", "$B$: $V_B=3V_A=0{,}0300$ m³, misma temperatura", "$C$: $V_C=V_B$, $p_C=\\frac{p_A}{5}$ · $D$: $V_D=V_A$, $T_D=T_C$"]))
    p = L.paso("A) Estados A y B")
    estado_p(p, g, "A", primero=True, voz="Despejamos la presión del gas ideal y la calculamos en A.")
    p.di("De A a B la temperatura no cambia: pe por ve se conserva.".replace("pe por ve", dice("pV", "pe por ve")), V(foco="AB"),
         M(r"p_AV_A=p_BV_B\;\Rightarrow\;p_B=p_A\,\frac{V_A}{V_B}=\frac{p_A}{3}"), M(rf"p_B=\frac{{{tex(g.p('A'), 4)}}}{{3}}={tex(g.p('B'), 4, u='Pa')}"))
    p = L.paso("A) Estados C y D")
    p.di("La presión de C es la quinta parte de la de A.", V(foco="BC"), M(rf"p_C=\frac{{p_A}}{{5}}=\frac{{{tex(g.p('A'), 4)}}}{{5}}={tex(g.p('C'), 4, u='Pa')}"))
    estado_T(p, g, "C", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en C.")
    p.di("D está en la misma isoterma que C, con el volumen de A.", V(foco="CD"), M(rf"T_D=T_C={tex(g.t('D'), 4, u='K')}\qquad V_D=V_A=0{{,}}0100\ \text{{m}}^3"))
    estado_p(p, g, "D")
    explica_proceso(L.paso("B1) Expansión isotérmica A→B"), g, "A", "B", "isotermico", foco="AB")
    explica_proceso(L.paso("B2) Enfriamiento isocórico B→C"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("B3) Compresión isotérmica C→D"), g, "C", "D", "isotermico", foco="CD")
    explica_proceso(L.paso("B4) Calentamiento isocórico D→A"), g, "D", "A", "isocorico", foco="DA")
    c = cierre_ciclo(L, res, titulo="C y D) Trabajo neto, tipo de dispositivo y eficiencia", id_e="rcd")
    L.paso("C) ¿Motor o refrigerador?")\
        .di("El trabajo neto es positivo y el ciclo va en sentido horario: el gas entrega trabajo, es un motor.", V(foco="ciclo"),
            R(rf"W_{{\text{{neto}}}}={tex(c['W'], 4, u='J')}>0\;\Rightarrow\;\text{{motor}}", "rc"))
    L.responde("Wnet", rf"W_{{\text{{neto}}}}={tex(c['W'], 4)}\ \text{{J}}", c["W"], "J").responde("e", rf"e={tex(c['e'], 4)}", c["e"], "")
    return L.oficial("III Parcial ordinario II-2025", 2192.0, 0.2587).usa(r"pV=nRT", r"W=nRT\ln\frac{V_2}{V_1}", r"e=\frac{W}{Q_H}")

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
    num = n_mol * R_GAS * T
    vf = num / pf
    vi = vf / 3.0
    L = _sel("Q102", "Volumen inicial tras triplicar volumen isotérmicamente",
             "Tras una expansión isotérmica a $420$ K de $2{,}50$ mol de gas ideal, el volumen inicial se triplica y la presión final es $120$ kPa. El volumen inicial es:",
             {"A": "8,08 L", "B": "24,3 L", "C": "72,9 L", "D": "87,3 L"}, "B",
             claves=["isotérmica", "gas ideal"])
    L.paso("Volumen final con el gas ideal")\
        .di("Del estado final conocemos todo menos el volumen. Despejamos de la ley del gas ideal de la hoja.", DATO("se triplica"),
            F(r"pV=nRT\;\Rightarrow\;V=\frac{nRT}{p}"))\
        .di("Sustituimos con la presión en pascales.", M(r"V_f = \frac{(2{,}50)(8{,}314)(420)}{120\times 10^3}"),
            M(rf"V_f=\frac{{{tex(num, 5)}}}{{120\,000}}={tex(vf, 4, u='m^3')}"))\
        .di("En litros: mil litros por metro cúbico.", M(rf"V_f=({tex(vf, 4)})(1000)={tex(vf * 1e3, 4, u='L')}"))
    L.paso("Volumen inicial")\
        .di("El volumen final es el triple del inicial: despejamos.", M(r"V_f=3V_i\;\Rightarrow\;V_i=\frac{V_f}{3}"))\
        .di(f"Dividimos entre tres: {nu(vi * 1e3, 'L', 3)}.", M(rf"V_i=\frac{{{tex(vf * 1e3, 4)}}}{{3}}"),
            R(rf"V_i = {tex(vi * 1e3, 3, u='L')}"))
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
    L = _des("Q103", "Ciclo refrigerador con 0,300 mol de nitrógeno (N₂)",
             "Con $0{,}300$ mol de $\\text{N}_2$ (diatómico) desde $A$ ($1{,}50\\times 10^5$ Pa, $300$ K): $A\\to B$ expansión isobárica a $2{,}50V_A$; $B\\to C$ compresión isotérmica a $V_A$; $C\\to A$ enfriamiento isocórico. A) Diagrama $p$-$V$. B) Tabla $(p,V,T)$. C) Calor por proceso. D) Trabajo por proceso. E) Eficiencia o rendimiento.",
             claves=["nitrógeno", "refrigerador", "ciclo antihorario"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "isotermico"), ("C", "A", "isocorico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Tres etapas: expandir a presión constante, comprimir sin cambiar la temperatura y enfriar a volumen constante.", DATO("0,300 mol de nitrógeno"),
            LISTA(["$n=0{,}300$ mol de $\\text{N}_2$, diatómico",
                   "$A$: $p_A=1{,}50\\times10^5$ Pa y $T_A=300$ K",
                   "$A\\to B$: $p$ constante hasta $V_B=2{,}50\\,V_A$",
                   "$B\\to C$: $T$ constante hasta $V_C=V_A$ · $C\\to A$: $V$ constante"]))\
        .di("Diatómico: de la hoja, la capacidad a volumen constante.", F(r"C_V=\frac{5}{2}R"),
            M(rf"C_V=\frac{{5}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))
    estado_V(L.paso("B) Estado A"), g, "A", primero=True, voz="Despejamos el volumen del gas ideal y lo calculamos en A.", dec_T=0)
    L.paso("B) Estado B: presión constante")\
        .di("La presión no cambia y el volumen se multiplica por dos coma cinco.", V(foco="AB"),
            M(r"p_B=p_A=1{,}50\times10^5\ \text{Pa}"),
            M(rf"V_B=2{{,}}50\,V_A=2{{,}}50\,({tex(g.v('A'), 5)})={tex(g.v('B'), 4, u='m^3')}"))\
        .di("Del gas ideal, a presión constante el volumen y la temperatura van juntos.", F(r"pV=nRT"),
            M(r"\frac{V_B}{T_B}=\frac{V_A}{T_A}\;\Rightarrow\;T_B=T_A\,\frac{V_B}{V_A}=2{,}50\,T_A"))\
        .di("La temperatura también se multiplica por dos coma cinco.", M(rf"T_B=2{{,}}50\,(300)={tex(g.t('B'), dec=0, u='K')}"))
    p = L.paso("B) Estado C: misma temperatura que B")
    p.di("De B a C la temperatura no cambia, y el volumen vuelve al de A.", V(foco="BC"),
         M(rf"T_C=T_B={tex(g.t('C'), dec=0, u='K')}\qquad V_C=V_A={tex(g.v('C'), 4, u='m^3')}"))
    estado_p(p, g, "C", primero=True, voz="Despejamos la presión del gas ideal y la calculamos en C.", dec_T=0)
    p.di("La tabla con los tres estados.", tabla_estados(g, ["A", "B", "C"], up="kPa", uv="L", fp=1e-3, fv=1e3, cp=4, cv=4))
    explica_proceso(L.paso("C1 y D1) Expansión isobárica A→B"), g, "A", "B", "isobarico", foco="AB")
    explica_proceso(L.paso("C2 y D2) Compresión isotérmica B→C"), g, "B", "C", "isotermico", foco="BC")
    explica_proceso(L.paso("C3 y D3) Enfriamiento isocórico C→A"), g, "C", "A", "isocorico", foco="CA")
    L.paso("E) ¿Motor o refrigerador?")\
        .di("De A a B va hacia la derecha por abajo, regresa por la curva de arriba hasta C y baja de C a A: gira en sentido antihorario.", V(foco="ciclo"))\
        .di("En sentido antihorario el trabajo neto es negativo: es un refrigerador, y lo que se pide es su coeficiente de rendimiento.",
            M(r"\text{antihorario}\;\Rightarrow\;W_{\text{neto}}<0\;\Rightarrow\;\text{refrigerador}"))
    c = cierre_ciclo(L, res, refri=True, titulo="E) Coeficiente de rendimiento K", id_e="re")
    L.responde("QAB", rf"Q_{{AB}}={tex(rab['Q'], 4)}\ \text{{J}}", rab["Q"], "J")\
     .responde("WBC", rf"W_{{BC}}={tex(rbc['W'], 4)}\ \text{{J}}", rbc["W"], "J")\
     .responde("K", rf"K={tex(c['K'], 3)}", c["K"], "")
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
    L.paso("Qué nos dan y qué piden")\
        .di("Tres etapas: expandir a presión constante, enfriar a volumen constante y volver sin cambiar la temperatura.", DATO("2,00 mol de helio"),
            LISTA(["$n=2{,}00$ mol de helio, monoatómico",
                   "$A$: $0{,}0200$ m³ y $2{,}00\\times10^5$ Pa · $B$: $0{,}0500$ m³ y $2{,}00\\times10^5$ Pa",
                   "$C$: $0{,}0500$ m³ y $0{,}800\\times10^5$ Pa",
                   "$A\\to B$: $p$ constante · $B\\to C$: $V$ constante · $C\\to A$: $T$ constante"]))\
        .di("Monoatómico: de la hoja, la capacidad a volumen constante.", F(r"C_V=\frac{3}{2}R"),
            M(rf"C_V=\frac{{3}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))
    p = L.paso("A) Temperaturas en los tres puntos")
    estado_T(p, g, "A", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en A.", dec=2)
    estado_T(p, g, "B", dec=2)
    estado_T(p, g, "C", dec=2)
    p.di("La de C sale igual a la de A, como debe ser: de C a A la temperatura no cambia.", V(foco="CA"),
         M(rf"T_C=T_A={tex(g.t('A'), dec=2, u='K')}"))
    explica_proceso(L.paso("B1 y C1) Expansión isobárica A→B"), g, "A", "B", "isobarico", foco="AB")
    explica_proceso(L.paso("B2 y C2) Enfriamiento isocórico B→C"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("B3 y C3) Compresión isotérmica C→A"), g, "C", "A", "isotermico", foco="CA")
    L.paso("Resumen del ciclo")\
        .di("La tabla de los tres procesos.", V(foco="ciclo"), tabla_quw(res))\
        .di("Los cambios de energía interna suman cero: el gas vuelve a su temperatura inicial.",
            M(r"\Delta U_{AB}+\Delta U_{BC}+\Delta U_{CA}=" + f"({tex(rab['U'], 4)})+({tex(rbc['U'], 4)})+(0)=0"))
    L.responde("TA", rf"T_A={tex(g.t('A'), dec=2)}\ \text{{K}}", g.t("A"), "K")\
     .responde("TB", rf"T_B={tex(g.t('B'), dec=2)}\ \text{{K}}", g.t("B"), "K")\
     .responde("WAB", rf"W_{{AB}}={tex(rab['W'], 4)}\ \text{{J}}", rab["W"], "J")\
     .responde("WCA", rf"W_{{CA}}={tex(rca['W'], 4)}\ \text{{J}}", rca["W"], "J")\
     .responde("dUAB", rf"\Delta U_{{AB}}={tex(rab['U'], 4)}\ \text{{J}}", rab["U"], "J")
    return L.oficial("III Parcial ordinario I-2026", 240.55, 601.40, 6000.0, -3665.0, 9000.0).usa(r"pV=nRT", r"\Delta U=nC_V\Delta T")
