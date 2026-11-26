"""Superexamen Q130–Q137: entropía en procesos reversibles e irreversibles."""

from __future__ import annotations

import math

from .ayudas import R_GAS, T0, revisar_opciones
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .formato import n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion

_AVISO_ENT = "Nota para tu examen: el formulario oficial II-2026 no incluye fórmulas de entropía; revisa estos ejercicios como refuerzo conceptual."


def _sel(q: str, titulo: str, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), tipo="seleccion", fuente=fuente(q), enunciado=enunciado,
                   opciones=opciones, correcta=correcta, aviso=k.pop("aviso", _AVISO_ENT), **k)


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado,
                   aviso=k.pop("aviso", _AVISO_ENT), **k)


# ============================================================================ Q130
@leccion("Q130")
def q130() -> Leccion:
    L = _sel("Q130", "Entropía total en un ciclo reversible",
             "En un ciclo reversible, ¿cuál afirmación describe correctamente el comportamiento de la entropía total del sistema y su entorno?",
             {"A": "La entropía total aumenta.", "B": "La entropía total disminuye.",
              "C": "La entropía total permanece constante y es igual a cero.",
              "D": "La entropía total permanece constante y es diferente de cero."}, "D",
             claves=["ciclo reversible", "entropía total"])
    L.paso("Distinguir S total de ΔS total")\
        .di("En un ciclo reversible el cambio de entropía del universo es cero, lo cual significa que la entropía total se mantiene constante.", DATO("ciclo reversible"),
            F(r"\Delta S_{\text{total}} = 0 \;\Rightarrow\; S_{\text{total}} = \text{constante}"))\
        .di("Ojo con la trampa: que el cambio sea cero no quiere decir que la entropía misma valga cero.",
            OJO("$\\Delta S=0$ significa que $S$ **no cambia**, no que $S=0$."),
            R(r"S_{\text{total}} = \text{constante} \neq 0"))
    revisar_opciones(L, {
        "A": ("La A ocurre solo en procesos irreversibles.", "Reversible tiene $\\Delta S=0$."),
        "B": ("La B violaría la segunda ley de la termodinámica.", "Nunca disminuye."),
        "C": ("La C confunde el cambio delta S con el valor de S.", "$S$ no vale cero a $T>0$ K."),
        "D": ("La D es la correcta: permanece constante y distinta de cero.", "Exacto."),
    })
    return L.oficial("III Parcial extraordinario II-2024", "D").usa(r"\Delta S_{\text{rev}}=0")


# ============================================================================ Q131
@leccion("Q131")
def q131() -> Leccion:
    m = 0.0100
    ch, ca, cv = 2090.0, 4186.0, 2010.0
    Lf, Lv = 3.33e5, 2.26e6
    Th, Tf0, Tv, Tf = -15.0 + T0, T0, 100.0 + T0, 120.0 + T0
    s1 = m * ch * math.log(Tf0 / Th)
    s2 = m * Lf / Tf0
    s3 = m * ca * math.log(Tv / Tf0)
    s4 = m * Lv / Tv
    s5 = m * cv * math.log(Tf / Tv)
    ds_tot = s1 + s2 + s3 + s4 + s5
    # Hierro C = 3R, M_Fe = 55.847 g/mol
    n_fe = ds_tot / (3.0 * R_GAS * math.log(Tf / Th))
    m_fe_g = n_fe * 55.847
    L = _des("Q131", "Entropía de hielo de −15 °C a vapor a 120 °C y masa de hierro equivalente",
             "Un bloque de hielo de $10{,}0$ g se lleva de $-15{,}0\\ ^\\circ$C hasta vapor a $120{,}0\\ ^\\circ$C. (a) Cambio de entropía del bloque. (b) Masa de hierro ($C=3R$, $M=55{,}847$ g/mol) con igual $\\Delta S$ entre las mismas temperaturas.",
             claves=["cambio de fase", "entropía", "hielo a vapor"],
             visual={"tipo": "entropia", "partes": [
                 {"n": "hielo -15→0", "dS": round(s1, 2)}, {"n": "fusión", "dS": round(s2, 2)},
                 {"n": "agua 0→100", "dS": round(s3, 2)}, {"n": "ebullición", "dS": round(s4, 2)},
                 {"n": "vapor 100→120", "dS": round(s5, 2)}]})
    L.paso("(a) Cinco etapas del hielo hasta vapor")\
        .di("Dividimos el calentamiento en cinco etapas: tres cambios de temperatura con logaritmo y dos cambios de fase a temperatura fija.", DATO("10.0 g"),
            TABLA(["Etapa", "Fórmula", "$\\Delta S$ (J/K)"],
                  [["Hielo $-15\\to 0\\ ^\\circ$C", "$mc_h\\ln(273{,}15/258{,}15)$", f"${tex(s1, 3)}$"],
                   ["Fusión a $0\\ ^\\circ$C", "$mL_f/273{,}15$", f"${tex(s2, 4)}$"],
                   ["Agua $0\\to 100\\ ^\\circ$C", "$mc_a\\ln(373{,}15/273{,}15)$", f"${tex(s3, 4)}$"],
                   ["Ebullición a $100\\ ^\\circ$C", "$mL_v/373{,}15$", f"${tex(s4, 4)}$"],
                   ["Vapor $100\\to 120\\ ^\\circ$C", "$mc_v\\ln(393{,}15/373{,}15)$", f"${tex(s5, 3)}$"]], resaltar=[[3, 2]]))\
        .di(f"Sumamos las cinco aportaciones y obtenemos {nu(ds_tot, 'J/K', 4)}.", V(foco="total"),
            R(rf"\Delta S = {tex(ds_tot, 4, u='J/K')}", "ra"),
            NOTA("El folleto imprimió $T_h=268{,}15$ K en vez de $258{,}15$ K (obteniendo $87{,}24$ J/K); la diferencia es menor al $1\\,\\%$."))
    L.paso("(b) Masa equivalente de hierro")\
        .di("Para el hierro con capacidad molar tres erre, despejamos los moles y multiplicamos por su masa molar.",
            F(r"\Delta S_{\text{Fe}} = n(3R)\ln\left(\frac{T_f}{T_h}\right) \;\Rightarrow\; n = \frac{\Delta S}{3R\ln(393{,}15/258{,}15)}"))\
        .di("Sustituimos el cambio de entropía del inciso a.", M(rf"n = \frac{{{tex(ds_tot, 4)}}}{{3(8{{,}}314)\ln(393{{,}}15/258{{,}}15)}}"),
            M(rf"n = {tex(n_fe, 4, u='mol')}"))\
        .di("La masa es los moles por la masa molar del hierro.", M(rf"m = n M = ({tex(n_fe, 4)})(55{{,}}847)"),
            R(rf"m = {tex(m_fe_g, 4, u='g')}", "rb"))
    L.responde("a", rf"\Delta S={tex(ds_tot, 4)}\ \text{{J/K}}", ds_tot, "J/K")\
     .responde("b", rf"m={tex(m_fe_g, 4)}\ \text{{g}}", m_fe_g, "g")
    return L.oficial("Folleto resuelto 4.1-4", 87.24, 464.6, tol=0.015).usa(r"\Delta S=\int\frac{dQ}{T}")


# ============================================================================ Q132
@leccion("Q132")
def q132() -> Leccion:
    L = _sel("Q132", "Entropía en un proceso adiabático irreversible",
             "En un proceso adiabático irreversible, sobre la entropía del sistema y la del entorno es posible afirmar que:",
             {"A": "no aumenta ninguna de las dos", "B": "aumentan las dos",
              "C": "solo aumenta la del sistema", "D": "solo aumenta la del entorno"}, "C",
             claves=["adiabático irreversible", "entropía"])
    L.paso("Adiabático implica entorno sin cambio de entropía")\
        .di("Al ser adiabático no cruza calor entre el sistema y el entorno, así que la entropía del entorno no cambia.", DATO("adiabático irreversible"),
            M(r"Q = 0 \;\Rightarrow\; \Delta S_{\text{entorno}} = 0"))\
        .di("Pero como es irreversible, la entropía total del universo debe aumentar; todo ese aumento ocurre en el sistema.",
            F(r"\Delta S_{\text{univ}} = \Delta S_{\text{sis}} + 0 > 0 \;\Rightarrow\; \Delta S_{\text{sis}} > 0"),
            R(r"\text{Solo aumenta } S_{\text{sistema}}"))
    revisar_opciones(L, {
        "A": ("La A solo vale si fuera adiabático reversible.", "Aquí es irreversible."),
        "B": ("La B falla: el entorno no recibe calor.", "$\\Delta S_{\\text{ent}}=0$."),
        "D": ("La D dice al revés: el entorno queda igual y el sistema aumenta.", "Sube el sistema."),
        "C": ("La C es la correcta: solo aumenta la del sistema.", "$\\Delta S_{\\text{sis}}>0$, $\\Delta S_{\\text{ent}}=0$."),
    })
    return L.oficial("III Parcial extraordinario I-2024", "C").usa(r"\Delta S_{\text{univ}}>0")


# ============================================================================ Q133
@leccion("Q133")
def q133() -> Leccion:
    L = _sel("Q133", "Segunda ley en términos de entropía",
             "¿Cuál de las siguientes afirmaciones concuerda con la segunda ley de la termodinámica en términos de entropía?",
             {"A": "La entropía total del universo siempre disminuye.",
              "B": "La entropía del universo aumenta en procesos irreversibles.",
              "C": "En los procesos reversibles, la entropía del universo aumenta.",
              "D": "La entropía del universo permanece constante en procesos irreversibles."}, "B",
             claves=["segunda ley", "entropía del universo"])
    L.paso("Regla general de la entropía del universo")\
        .di("En procesos reversibles el cambio total es cero; en todos los procesos reales irreversibles es estrictamente positivo.", DATO("segunda ley"),
            F(r"\Delta S_{\text{universo}} \ge 0"),
            LISTA(["Reversible: $\\Delta S_{\\text{univ}} = 0$", "Irreversible: $\\Delta S_{\\text{univ}} > 0$"]))
    revisar_opciones(L, {
        "A": ("La A es exactamente lo contrario de la segunda ley.", "Nunca disminuye."),
        "C": ("La C falla: en procesos reversibles permanece constante.", "$\\Delta S=0$."),
        "D": ("La D falla: en procesos irreversibles aumenta.", "$\\Delta S > 0$."),
        "B": ("La B es la correcta: aumenta en procesos irreversibles.", "Enunciado de entropía."),
    })
    return L.oficial("III Parcial ordinario I-2026", "B").usa(r"\Delta S_{\text{univ}}\ge 0")


# ============================================================================ Q134
@leccion("Q134")
def q134() -> Leccion:
    m, c = 0.250, 4186.0
    Ti, Tf = 90.0 + T0, 20.0 + T0
    ds_a = m * c * math.log(Tf / Ti)
    q_oc = m * c * (Ti - Tf)
    ds_o = q_oc / Tf
    ds_t = ds_a + ds_o
    L = _des("Q134", "Verter 250 g de agua caliente a 90 °C en el océano a 20 °C",
             "Se vierten $250$ g de agua a $90{,}0\\ ^\\circ$C en el océano a $20{,}0\\ ^\\circ$C (sistema aislado). (a) $\\Delta S$ del agua vertida. (b) $\\Delta S$ del océano. (c) ¿Es reversible o irreversible?",
             claves=["entropía", "océano", "irreversible"],
             visual={"tipo": "entropia", "partes": [{"n": "agua 90→20 °C", "dS": round(ds_a, 1)}, {"n": "océano (20 °C)", "dS": round(ds_o, 1)}]})
    L.paso("(a) Entropía del agua que se enfría")\
        .di("El agua baja de trescientos sesenta y tres coma quince a doscientos noventa y tres coma quince kelvin.", V(foco=0), DATO("250 g"),
            F(r"\Delta S_{\text{agua}} = m c\ln\left(\frac{T_f}{T_i}\right)"),
            M(rf"\Delta S_{{\text{{agua}}}} = (0{{,}}250)(4186)\ln\left(\frac{{293{{,}}15}}{{363{{,}}15}}\right)"),
            R(rf"\Delta S_{{\text{{agua}}}} = {tex(ds_a, 4, u='J/K')}", "ra"))
    L.paso("(b) y (c) Entropía del océano y total")\
        .di("El océano es tan grande que absorbe el calor sin cambiar de temperatura.", V(foco=1),
            M(rf"Q_{{\text{{oc}}}} = mc(90{{,}}0-20{{,}}0) = {tex(q_oc, 5, u='J', sci='no')}"),
            R(rf"\Delta S_{{\text{{océano}}}} = \frac{{{tex(q_oc, 5, sci='no')}}}{{293{{,}}15}} = +{tex(ds_o, 4, u='J/K')}", "rb"))\
        .di(f"La suma da {nu(ds_t, 'J/K', 3)}, positiva: el proceso es irreversible.", V(foco="total"),
            R(rf"\Delta S_{{\text{{total}}}} = {tex(ds_a, 4)} + {tex(ds_o, 4)} = +{tex(ds_t, 3, u='J/K')} > 0 \;\Rightarrow\; \text{{irreversible}}", "rc"))
    L.responde("a", rf"{tex(ds_a, 4)}\ \text{{J/K}}", ds_a, "J/K")\
     .responde("b", rf"{tex(ds_o, 4)}\ \text{{J/K}}", ds_o, "J/K")\
     .responde("c", rf"+{tex(ds_t, 3)}\ \text{{J/K}}", ds_t, "J/K")
    return L.oficial("Folleto resuelto 4.1-5", -224.1, 249.9, 25.8).usa(r"\Delta S=mc\ln\frac{T_f}{T_i}", r"\Delta S=\frac{Q}{T}")


# ============================================================================ Q135
@leccion("Q135")
def q135() -> Leccion:
    mh, ma = 0.0600, 0.500
    ch, ca, Lf = 2090.0, 4190.0, 3.33e5
    Th, T0k, Ta, Tf = -20.0 + T0, T0, 45.0 + T0, 30.6 + T0
    s_h = mh * (ch * math.log(T0k / Th) + Lf / T0k + ca * math.log(Tf / T0k))
    s_a = ma * ca * math.log(Tf / Ta)
    s_tot = s_h + s_a
    L = _des("Q135", "Entropía al mezclar hielo a −20 °C con agua a 45 °C",
             "Se colocan $0{,}0600$ kg de hielo a $-20{,}0\\ ^\\circ$C en $0{,}500$ kg de agua a $45{,}0\\ ^\\circ$C en recipiente aislado; llegan a $30{,}6\\ ^\\circ$C. (a) $\\Delta S$ del sistema. (b) ¿Reversible o irreversible?",
             claves=["entropía", "hielo y agua", "mezcla"],
             visual={"tipo": "entropia", "partes": [{"n": "hielo→agua 30,6 °C", "dS": round(s_h, 1)}, {"n": "agua 45→30,6 °C", "dS": round(s_a, 1)}]})
    L.paso("(a) Entropía del hielo y del agua")\
        .di("El hielo pasa por tres tramos: calentar a cero, fundirse y calentar hasta treinta coma seis grados.", V(foco=0), DATO("30.6"),
            M(rf"\Delta S_h = 0{{,}}0600\left[2090\ln\frac{{273{{,}}15}}{{253{{,}}15}} + \frac{{3{{,}}33\times 10^5}}{{273{{,}}15}} + 4190\ln\frac{{303{{,}}75}}{{273{{,}}15}}\right] = +{tex(s_h, 4, u='J/K')}"))\
        .di("El agua caliente solo se enfría.", V(foco=1),
            M(rf"\Delta S_a = (0{{,}}500)(4190)\ln\left(\frac{{303{{,}}75}}{{318{{,}}15}}\right) = {tex(s_a, 4, u='J/K')}"))\
        .di(f"Sumamos ambos: sale {nu(s_tot, 'J/K', 3)}, positivo e irreversible.", V(foco="total"),
            R(rf"\Delta S_{{\text{{sis}}}} = {tex(s_h, 4)} + ({tex(s_a, 4)}) = +{tex(s_tot, 3, u='J/K')} > 0 \;\Rightarrow\; \text{{irreversible}}", "ra"))
    L.responde("a", rf"+{tex(s_tot, 3)}\ \text{{J/K}}", s_tot, "J/K")
    return L.oficial("Folleto propuesto 4.2-4", 12.3).usa(r"\Delta S=mc\ln\frac{T_f}{T_i}", r"\Delta S=\frac{mL}{T}")


# ============================================================================ Q136
@leccion("Q136")
def q136() -> Leccion:
    m = 1.500
    ch, ca, Lf = 2090.0, 4186.0, 3.33e5
    Th, T0k, Tf = -15.0 + T0, T0, 27.0 + T0
    q_h = m * (ch * 15.0 + Lf + ca * 27.0)
    ds_dep = -q_h / Tf
    ds_h = m * (ch * math.log(T0k / Th) + Lf / T0k + ca * math.log(Tf / T0k))
    ds_tot = ds_dep + ds_h
    L = _des("Q136", "Hielo sobre un gran depósito térmico a 27 °C",
             "Sobre un gran depósito a $27{,}0\\ ^\\circ$C se colocan $1{,}500$ kg de hielo a $-15\\ ^\\circ$C que se funden y llegan a $27{,}0\\ ^\\circ$C. (a) $\\Delta S$ del depósito. (b) $\\Delta S$ del hielo. (c) $\\Delta S$ total.",
             claves=["entropía", "reservorio", "hielo"],
             visual={"tipo": "entropia", "partes": [{"n": "depósito (27 °C)", "dS": round(ds_dep, 1)}, {"n": "hielo→agua 27 °C", "dS": round(ds_h, 1)}]})
    L.paso("(a) Calor absorbido y entropía del depósito")\
        .di("El depósito entrega todo el calor necesario para calentar, fundir y llevar el hielo hasta veintisiete grados.", V(foco=0), DATO("1,500 kg"),
            M(rf"Q = 1{{,}}500\,[2090(15)+3{{,}}33\times 10^5+4186(27)] = {tex(q_h, 5, u='J', sci='si')}"))\
        .di(f"Como el depósito pierde calor a temperatura fija trescientos coma quince kelvin, da {nu(ds_dep, 'J/K', 5)}.",
            R(rf"\Delta S_{{\text{{dep}}}} = -\frac{{{tex(q_h, 5, sci='si')}}}{{300{{,}}15}} = {tex(ds_dep, 5, u='J/K', sci='no')}", "ra"))
    L.paso("(b) y (c) Entropía del hielo y total")\
        .di(f"Sumamos los tres tramos de entropía del hielo: da {nu(ds_h, 'J/K', 5)}.", V(foco=1),
            M(rf"\Delta S_h = 1{{,}}500\left[2090\ln\frac{{273{{,}}15}}{{258{{,}}15}}+\frac{{3{{,}}33\times 10^5}}{{273{{,}}15}}+4186\ln\frac{{300{{,}}15}}{{273{{,}}15}}\right]"),
            R(rf"\Delta S_h = +{tex(ds_h, 5, u='J/K', sci='no')}", "rb"))\
        .di(f"La suma total sale {nu(ds_tot, 'J/K', 3)}.", V(foco="total"),
            R(rf"\Delta S_{{\text{{total}}}} = +{tex(ds_tot, 3, u='J/K')}", "rc"))
    L.responde("a", rf"{tex(ds_dep, 5, sci='no')}\ \text{{J/K}}", ds_dep, "J/K")\
     .responde("b", rf"+{tex(ds_h, 5, sci='no')}\ \text{{J/K}}", ds_h, "J/K")\
     .responde("c", rf"+{tex(ds_tot, 3)}\ \text{{J/K}}", ds_tot, "J/K")
    return L.oficial("Folleto propuesto 4.2-5", -2386.2, 2598.2, 212.0).usa(r"\Delta S=mc\ln\frac{T_f}{T_i}", r"\Delta S=\frac{Q}{T}")


# ============================================================================ Q137
@leccion("Q137")
def q137() -> Leccion:
    m_taz, m_ag, m_cil, m_vap = 0.1460, 0.2230, 0.3140, 0.0047
    c_cu, c_ag, Lv = 387.0, 4186.0, 2.26e6
    T1, Tf, Tc = 21.0 + T0, 100.0 + T0, 826.1 + T0
    ds_ag = m_ag * c_ag * math.log(Tf / T1) + m_vap * Lv / Tf
    ds_taz = m_taz * c_cu * math.log(Tf / T1)
    ds_cil = m_cil * c_cu * math.log(Tf / Tc)
    ds_tot = ds_ag + ds_taz + ds_cil
    L = _des("Q137", "Tazón de cobre con agua y cilindro caliente a 826,1 °C",
             "Un tazón de cobre ($146{,}0$ g) con $223{,}0$ g de agua a $21{,}0\\ ^\\circ$C recibe un cilindro de cobre ($314{,}0$ g) a $826{,}1\\ ^\\circ$C; llegan a $100\\ ^\\circ$C y hierven $4{,}7$ g de agua. Calcule $\\Delta S$ de: (a) agua+vapor, (b) tazón, (c) cilindro, (d) universo.",
             claves=["entropía", "ebullición", "cobre y agua"],
             visual={"tipo": "entropia", "partes": [
                 {"n": "agua + vapor", "dS": round(ds_ag, 1)},
                 {"n": "tazón Cu", "dS": round(ds_taz, 1)},
                 {"n": "cilindro Cu", "dS": round(ds_cil, 1)}]})
    L.paso("(a) y (b) Entropía del agua + vapor y del tazón")\
        .di("El agua se calienta hasta cien grados y cuatro coma siete gramos se evaporan.", V(foco=0), DATO("4.7 g"),
            M(rf"\Delta S_{{\text{{agua}}}} = (0{{,}}2230)(4186)\ln\frac{{373{{,}}15}}{{294{{,}}15}} + \frac{{(0{{,}}0047)(2{{,}}26\times 10^6)}}{{373{{,}}15}}"),
            R(rf"\Delta S_{{\text{{agua}}}} = +{tex(ds_ag, 4, u='J/K')}", "ra"))\
        .di("El tazón de cobre solo sube de veintiuno a cien grados.", V(foco=1),
            M(rf"\Delta S_{{\text{{tazón}}}} = (0{{,}}1460)(387)\ln\frac{{373{{,}}15}}{{294{{,}}15}}"),
            R(rf"\Delta S_{{\text{{tazón}}}} = +{tex(ds_taz, 3, u='J/K')}", "rb"))
    L.paso("(c) y (d) Entropía del cilindro y del universo")\
        .di("El cilindro se enfría desde mil noventa y nueve coma veinticinco kelvin hasta trescientos setenta y tres coma quince.", V(foco=2),
            M(rf"\Delta S_{{\text{{cil}}}} = (0{{,}}3140)(387)\ln\frac{{373{{,}}15}}{{1099{{,}}25}}"),
            R(rf"\Delta S_{{\text{{cil}}}} = {tex(ds_cil, 4, u='J/K')}", "rc"))\
        .di(f"Sumamos las tres partes: da {nu(ds_tot, 'J/K', 3)}, claramente irreversible.", V(foco="total"),
            R(rf"\Delta S_{{\text{{univ}}}} = +{tex(ds_tot, 3, u='J/K')} > 0 \;\Rightarrow\; \text{{irreversible}}", "rd"))
    L.responde("a", rf"+{tex(ds_ag, 4)}\ \text{{J/K}}", ds_ag, "J/K")\
     .responde("b", rf"+{tex(ds_taz, 3)}\ \text{{J/K}}", ds_taz, "J/K")\
     .responde("c", rf"{tex(ds_cil, 4)}\ \text{{J/K}}", ds_cil, "J/K")\
     .responde("d", rf"+{tex(ds_tot, 3)}\ \text{{J/K}}", ds_tot, "J/K")
    return L.oficial("Folleto propuesto 4.2-6", 250.8, 13.5, -132.3, 132.0).usa(r"\Delta S=mc\ln\frac{T_f}{T_i}", r"\Delta S=\frac{mL_v}{T}")
