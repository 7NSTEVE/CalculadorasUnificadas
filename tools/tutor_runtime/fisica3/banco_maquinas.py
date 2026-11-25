"""Superexamen Q117–Q129: máquinas térmicas, refrigeradores, Carnot y segunda ley."""

from __future__ import annotations

import math

from .ayudas import ATM, R_GAS, Gas, explica_proceso, revisar_opciones, tabla_estados, tabla_quw, valores_visual, visual_pv
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .formato import n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion


def _sel(q: str, titulo: str, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), tipo="seleccion", fuente=fuente(q), enunciado=enunciado,
                   opciones=opciones, correcta=correcta, **k)


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado, **k)


# ============================================================================ Q117
@leccion("Q117")
def q117() -> Leccion:
    L = _sel("Q117", "Cómo se mide el desempeño de un refrigerador",
             "¿Cómo se estima el desempeño de un refrigerador?",
             {"A": "Mediante una eficiencia que tiene un valor entre 0 y 1.",
              "B": "Mediante un coeficiente de rendimiento que tiene un valor entre 0 y 1.",
              "C": "Mediante una eficiencia que puede ser mayor que 1.",
              "D": "Mediante un coeficiente de rendimiento que puede ser mayor que 1."}, "D",
             claves=["refrigerador", "coeficiente de rendimiento"],
             visual={"tipo": "maquina", "modo": "refrigerador", "QH": 500, "QC": 400, "W": 100})
    L.paso("Coeficiente de rendimiento K")\
        .di("En un refrigerador medimos cuánto calor sacamos del interior frío por cada joule de trabajo eléctrico invertido.", DATO("desempeño de un refrigerador"),
            F(r"K = \frac{|Q_C|}{|W|}"))\
        .di("Como se puede bombear más calor que el trabajo eléctrico usado, ka suele ser mayor que uno.", V(foco="eficiencia"),
            R(r"K = \frac{|Q_C|}{|W|} \quad (\text{puede ser } > 1)"))
    revisar_opciones(L, {
        "A": ("La A describe una máquina térmica, no un refrigerador.", "Eficiencia $e<1$ es para motores."),
        "B": ("La B falla: ka típicamente vale entre tres y seis, mayor que uno.", "No está limitado a $1$."),
        "C": ("La C usa el nombre equivocado: no se llama eficiencia.", "Se llama coeficiente de rendimiento $K$."),
        "D": ("La D es la correcta: coeficiente de rendimiento que puede superar uno.", "Exacto."),
    })
    return L.oficial("II Parcial extraordinario II-2023", "D").usa(r"K=\frac{|Q_C|}{|W|}")


# ============================================================================ Q118
@leccion("Q118")
def q118() -> Leccion:
    L = _sel("Q118", "Efecto de la temperatura ambiente en un refrigerador",
             "¿Cómo afecta la temperatura ambiente al coeficiente de rendimiento de un refrigerador real lleno de alimentos?",
             {"A": "Permanece constante independientemente de la temperatura ambiente.",
              "B": "Primero aumenta y luego disminuye a medida que la temperatura ambiente aumenta.",
              "C": "Disminuye a medida que la temperatura ambiente aumenta.",
              "D": "Aumenta a medida que la temperatura ambiente aumenta."}, "C",
             claves=["refrigerador", "temperatura ambiente", "Carnot"],
             visual={"tipo": "maquina", "modo": "refrigerador", "QH": 500, "QC": 400, "W": 100, "TH": 300, "TC": 270})
    L.paso("Más calor afuera cuesta más trabajo bombear")\
        .di("El límite máximo de rendimiento baja cuando crece la diferencia entre la cocina y el interior frío.", DATO("temperatura ambiente"),
            F(r"K \le K_{\text{Carnot}} = \frac{T_C}{T_H - T_C}"))\
        .di("Si sube la temperatura ambiente te hache, el denominador crece y el rendimiento disminuye.", V(foco="carnot"),
            R(r"T_H \uparrow \;\Rightarrow\; (T_H - T_C) \uparrow \;\Rightarrow\; K \downarrow"))
    revisar_opciones(L, {
        "A": ("La A no: en un día muy caliente el compresor trabaja mucho más.", "Sí depende de $T_H$."),
        "B": ("La B no: la tendencia es siempre decreciente al subir la temperatura ambiente.", "Monótonamente decreciente."),
        "D": ("La D dice al revés: al subir la temperatura ambiente, cuesta más enfriar.", "$K$ baja, no sube."),
        "C": ("La C es la correcta: disminuye al aumentar la temperatura ambiente.", "Mayor salto térmico = menor $K$."),
    })
    return L.oficial("III Parcial ordinario I-2024", "C").usa(r"K=\frac{|Q_C|}{|W|}")


# ============================================================================ Q119
@leccion("Q119")
def q119() -> Leccion:
    L = _sel("Q119", "Definición de eficiencia térmica de una máquina",
             "¿Cuál de las siguientes opciones describe mejor la eficiencia térmica de una máquina?",
             {"A": "Es la cantidad total de energía generada por la máquina.",
              "B": "Es la cantidad que representa la fracción de calor absorbido que se convierte en trabajo.",
              "C": "Es la diferencia entre el calor absorbido y el calor expulsado por la máquina.",
              "D": "Es la razón entre la cantidad de calor absorbido y la cantidad de calor expulsado."}, "B",
             claves=["eficiencia térmica", "máquina térmica"],
             visual={"tipo": "maquina", "QH": 1000, "QC": 650, "W": 350})
    L.paso("Lo que obtengo entre lo que pago")\
        .di("La eficiencia térmica mide qué fracción del calor absorbido del foco caliente sale como trabajo neto.", DATO("eficiencia térmica"),
            F(r"e = \frac{W}{Q_H}"), V(foco="eficiencia"))\
        .di("Como es una razón de trabajo entre calor entrante, es adimensional y menor que uno.",
            R(r"e = \frac{W}{Q_H} \in (0, 1)"))
    revisar_opciones(L, {
        "A": ("La A habla de energía total, no de una fracción.", "No es energía, es razón."),
        "C": ("La C describe el trabajo neto, no la eficiencia.", "$Q_H - |Q_C| = W$."),
        "D": ("La D da una razón distinta que además sería mayor que uno.", "No es $Q_H/|Q_C|$."),
        "B": ("La B es la correcta: fracción de calor absorbido convertida en trabajo.", "$e = W/Q_H$."),
    })
    return L.oficial("III Parcial ordinario I-2024", "B").usa(r"e=\frac{W}{Q_H}")


# ============================================================================ Q120
@leccion("Q120")
def q120() -> Leccion:
    n_mol = 7.107e-2
    v1, v2, p3 = 3.50e-3, 1.40e-3, 1.013e5
    g = Gas(n_mol, "mono")
    # 3->1 isobaric => p1 = p3
    g.pone("1", p=p3, v=v1)
    g.pone("2", v=v2, t=g.t("1"))
    # 2->3 adiabatic to p3: p2 V2^g = p3 V3^g
    v3 = v2 * (g.p("2") / p3) ** (1.0 / g.g)
    g.pone("3", p=p3, v=v3)
    r12 = g.proceso("1", "2", "isotermico")
    r23 = g.proceso("2", "3", "adiabatico")
    r31 = g.proceso("3", "1", "isobarico")
    res = [("1→2", r12), ("2→3", r23), ("3→1", r31)]
    wnet = r12["W"] + r23["W"] + r31["W"]
    qc = r31["Q"]
    K = abs(qc) / abs(wnet)
    L = _des("Q120", "Refrigerador de gas ideal monoatómico (3 etapas)",
             "Un refrigerador opera con $7{,}107\\times 10^{-2}$ mol de gas ideal monoatómico: $1\\to 2$ compresión isotérmica de $V_1=3{,}50\\times 10^{-3}\\ \\text{m}^3$ a $V_2=1{,}40\\times 10^{-3}\\ \\text{m}^3$; $2\\to 3$ expansión adiabática hasta $p_3=1{,}013\\times 10^5$ Pa; $3\\to 1$ expansión isobárica cerrando el ciclo. A) Diagrama $p$-$V$. B) $T_1, p_2, T_3, V_3$. C) $W, Q, \\Delta U$. D) $K$.",
             claves=["refrigerador", "ciclo antihorario", "monoatómico"],
             visual=visual_pv(g, [("1", "2", "isotermico"), ("2", "3", "adiabatico"), ("3", "1", "isobarico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("A y B) Estados del ciclo")\
        .di("Como de tres a uno es isobárico, la presión en uno es igual a la presión en tres.", DATO("expande isobáricamente"),
            M(rf"T_1 = T_2 = \frac{{p_1 V_1}}{{nR}} = \frac{{(1{{,}}013\times 10^5)(3{{,}}50\times 10^{{-3}})}}{{(7{{,}}107\times 10^{{-2}})(8{{,}}314)}} = {tex(g.t('1'), dec=1, u='K')}"),
            M(rf"p_2 = p_1\frac{{V_1}}{{V_2}} = (1{{,}}013\times 10^5)\frac{{3{{,}}50}}{{1{{,}}40}} = {tex(g.p('2'), 4, u='Pa', sci='si')}"))\
        .di("Con la relación adiabática de un gas monoatómico hallamos el estado tres.",
            M(rf"V_3 = V_2\left(\frac{{p_2}}{{p_3}}\right)^{{3/5}} = {tex(v3, 4, u='m^3', sci='si')}\qquad T_3 = \frac{{p_3 V_3}}{{nR}} = {tex(g.t('3'), dec=1, u='K')}"),
            tabla_estados(g, ["1", "2", "3"], up="kPa", uv="L", fp=1e-3, fv=1e3, cp=4, cv=3))
    explica_proceso(L.paso("C1) Compresión isotérmica 1→2"), g, "1", "2", "isotermico", foco="12")
    explica_proceso(L.paso("C2) Expansión adiabática 2→3"), g, "2", "3", "adiabatico", foco="23")
    explica_proceso(L.paso("C3) Expansión isobárica 3→1"), g, "3", "1", "isobarico", foco="31")
    L.paso("D) Coeficiente de rendimiento del refrigerador")\
        .di("El trabajo neto es negativo porque es un refrigerador; el calor absorbido del foco frío es el de la etapa tres a uno.", V(foco="ciclo"),
            tabla_quw(res),
            M(rf"W_{{\text{{neto}}}} = {tex(wnet, 4, u='J')}\qquad Q_C = Q_{{31}} = {tex(qc, 4, u='J')}"))\
        .di(f"Dividimos el calor extraído entre la magnitud del trabajo neto: da {n(K, 3)}.",
            F(r"K = \frac{|Q_C|}{|W_{\text{neto}}|}"),
            R(rf"K = \frac{{{tex(qc, 4)}}}{{{tex(abs(wnet), 3)}}} = {tex(K, 3)}", "rd"))
    L.responde("T1", rf"T_1={tex(g.t('1'), dec=1)}\ \text{{K}}", g.t("1"), "K")\
     .responde("Wnet", rf"W_{{\text{{neto}}}}={tex(wnet, 3)}\ \text{{J}}", wnet, "J")\
     .responde("K", rf"K={tex(K, 3)}", K, "")
    return L.usa(r"pV=nRT", r"pV^\gamma=\text{cte}", r"K=\frac{|Q_C|}{|W|}")


# ============================================================================ Q121
@leccion("Q121")
def q121() -> Leccion:
    L = _sel("Q121", "Propiedades de un ciclo de máquina térmica",
             "Para un ciclo termodinámico que representa una máquina térmica: I. El trabajo neto es positivo. II. El ciclo en p-V se recorre en sentido antihorario. III. El cambio total de energía interna es cero. ¿Cuáles son correctas?",
             {"A": "I y II", "B": "II y III", "C": "I y III", "D": "I, II y III"}, "C",
             claves=["máquina térmica", "sentido horario", "ciclo"])
    L.paso("Revisar las tres propiedades")\
        .di("Un motor entrega trabajo positivo, gira en sentido horario en el diagrama p-V y, al volver al estado inicial, su cambio total de energía interna es cero.", DATO("máquina térmica"),
            LISTA(["**I. Correcta**: un motor produce trabajo neto ($W_{\\text{neto}} > 0$)",
                   "**II. Incorrecta**: el sentido horario es motor; antihorario es refrigerador",
                   "**III. Correcta**: en todo ciclo cerrado $\\Delta U_{\\text{ciclo}} = 0$"]))\
        .di("Son correctas la uno y la tres.", R(r"\text{I y III}"))
    revisar_opciones(L, {
        "A": ("La A incluye la dos, pero antihorario es un refrigerador.", "II es falsa."),
        "B": ("La B también incluye la dos.", "Motor = sentido horario."),
        "D": ("La D incluye la dos.", "Solo I y III."),
        "C": ("La C es la correcta: uno y tres.", "$W>0$ y $\\Delta U_{\\text{ciclo}}=0$."),
    })
    return L.oficial("III Parcial extraordinario II-2024", "C").usa(r"\Delta U_{\text{ciclo}}=0")


# ============================================================================ Q122
@leccion("Q122")
def q122() -> Leccion:
    n_mol, ta, tb, tc, pa = 0.300, 400.0, 300.0, 267.2, 3.6e6
    # p_C = (T_C/T_A)*p_A -> gamma = ln(T_C/T_A)/ln(0.75) (método oficial)
    pc = (tc / ta) * pa
    gamma = math.log(tc / ta) / math.log(0.75)
    cp = gamma * R_GAS / (gamma - 1.0)
    cv = cp - R_GAS
    qc = n_mol * cv * (ta - tc)
    qh = n_mol * cp * (tb - ta)
    K = abs(qc) / (abs(qh) - abs(qc))
    K_carnot = tc / (ta - tc)
    g = Gas(n_mol, cv=cv)
    g.pone("A", p=pa, t=ta)
    g.pone("B", p=pa, t=tb)
    g.pone("C", v=g.v("A"), t=tc)
    L = _des("Q122", "Ciclo de compresión-expansión: hallar γ, C_p, K y límite de Carnot",
             "Un ciclo con $0{,}300$ mol de gas ideal: $A\\to B$ compresión isobárica de $400$ K a $300$ K ($V_B=0{,}75V_A$, $p_A=3{,}6\\times 10^6$ Pa); $B\\to C$ expansión adiabática hasta $V_A$ y $267{,}2$ K; $C\\to A$ calentamiento isocórico. A) $\\gamma$. B) $C_p$. C) $K$ del ciclo. D) $K_{\\text{Carnot}}$. E) ¿Es posible operar entre esos dos focos?",
             claves=["gamma", "refrigerador", "Carnot", "imposibilidad"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "adiabatico"), ("C", "A", "isocorico")],
                              uv="cm³", up="MPa", fv=1e6, fp=1e-6))
    L.paso("A y B) Hallar γ y C_p")\
        .di("En el tramo isocórico la presión es proporcional a la temperatura; luego aplicamos la relación adiabática de B a C.", DATO("75 % de su volumen"),
            M(rf"p_C = \frac{{T_C}}{{T_A}}\,p_A = 0{{,}}668\,p_A = {tex(pc, 3, u='Pa', sci='si')}"),
            M(rf"(0{{,}}75)^\gamma = 0{{,}}668 \;\Rightarrow\; \gamma = \frac{{\ln(0{{,}}668)}}{{\ln(0{{,}}75)}} = {tex(gamma, 3)}"))\
        .di(f"Despejamos ce pe con gama igual a uno coma cuatro: da {nu(cp, 'J/(mol·K)', 3)}.",
            F(r"C_p = \frac{\gamma R}{\gamma - 1}"),
            R(rf"\gamma = {tex(gamma, 3)}\qquad C_p = {tex(cp, 3, u='J/(mol·K)')}", "rab"))
    L.paso("C) Coeficiente de rendimiento del ciclo")\
        .di("El calor que entra ocurre en el tramo isocórico y el que sale en el isobárico.",
            M(rf"Q_C = Q_{{CA}} = nC_V(T_A-T_C) = (0{{,}}300)({tex(cv, 4)})(400-267{{,}}2) = {tex(qc, 4, u='J')}"),
            M(rf"Q_H = Q_{{AB}} = nC_p(T_B-T_A) = (0{{,}}300)({tex(cp, 3)})(300-400) = {tex(qh, 4, u='J')}"))\
        .di(f"Calculamos el rendimiento: da {n(K, 4)}.",
            F(r"K = \frac{|Q_C|}{|Q_H|-|Q_C|}"),
            R(rf"K = \frac{{{tex(qc, 4)}}}{{{tex(abs(qh), 4)}-{tex(qc, 4)}}} = {tex(K, 4)}", "rc"))
    L.paso("D y E) Límite de Carnot y comparación")\
        .di(f"Para un refrigerador de Carnot entre las temperaturas extremas, el máximo es {n(K_carnot, 3)}.",
            F(r"K_{\text{Carnot}} = \frac{T_C}{T_H - T_C} = \frac{267{,}2}{400 - 267{,}2}"),
            R(rf"K_{{\text{{Carnot}}}} = {tex(K_carnot, 3)}", "rd"))\
        .di("Como dieciocho coma cuatro supera el límite de Carnot, este ciclo no puede operar entre dos focos fijos a esas temperaturas.",
            R(r"K > K_{\text{Carnot}} \;\Rightarrow\; \text{no es posible (viola la 2.ª ley)}"))
    L.responde("A", rf"\gamma={tex(gamma, 3)}", gamma, "")\
     .responde("B", rf"C_p={tex(cp, 3)}\ \text{{J/(mol}}\cdot\text{{K)}}", cp, "J/(mol·K)")\
     .responde("C", rf"K={tex(K, 4)}", K, "")\
     .responde("D", rf"K_{{\text{{Carnot}}}}={tex(K_carnot, 3)}", K_carnot, "")
    return L.oficial("III Parcial ordinario II-2024", 1.40, 29.1, 18.44, 2.01, tol=0.04,
                     nota="El oficial redondeó γ=1,402 a 1,40 (Cp=29,10 → K=18,44); sin redondear da K=17,83 (ambos muy por encima de K_Carnot=2,01).").usa(r"pV^\gamma=\text{cte}", r"K=\frac{|Q_C|}{|W|}")


# ============================================================================ Q123
@leccion("Q123")
def q123() -> Leccion:
    L = _sel("Q123", "Interpretación del coeficiente de rendimiento K",
             "¿Cuál afirmación sobre el coeficiente de rendimiento $K$ de un refrigerador es correcta?",
             {"A": "K siempre es menor que 1, ya que no puede realizarse más trabajo que el calor extraído.",
              "B": "Un valor de K = 4 significa que consume 4 J de trabajo para extraer 1 J de calor.",
              "C": "Un valor alto de K indica que el refrigerador es más eficiente, pues extrae más calor por unidad de trabajo invertido.",
              "D": "K depende únicamente de la temperatura del espacio frío y no de la temperatura exterior."}, "C",
             claves=["coeficiente de rendimiento", "refrigerador"])
    L.paso("Leer la definición de K")\
        .di("Ka es el calor extraído del espacio frío dividido entre el trabajo invertido.", DATO("coeficiente de rendimiento"),
            F(r"K = \frac{|Q_C|}{|W|}"))\
        .di("Por eso, un ka grande significa que sacamos muchos joules de calor con muy poco trabajo.",
            R(r"K\ \text{alto} \;\Rightarrow\; \text{más }|Q_C|\text{ por cada joule de }|W|"))
    revisar_opciones(L, {
        "A": ("La A confunde refrigerador con motor: ka suele ser mayor que uno.", "$K$ suele ser $>1$."),
        "B": ("La B está al revés: ka igual a cuatro significa extraer cuatro joules por cada joule de trabajo.", "$4 = |Q_C|/|W|$."),
        "D": ("La D falla: también depende de la temperatura exterior te hache.", "$K_{\\text{Carnot}}=T_C/(T_H-T_C)$."),
        "C": ("La C es la correcta: mayor ka implica más calor extraído por unidad de trabajo.", "Definición directa de $K$."),
    })
    return L.oficial("III Parcial extraordinario I-2025", "C").usa(r"K=\frac{|Q_C|}{|W|}")


# ============================================================================ Q124
@leccion("Q124")
def q124() -> Leccion:
    L = _sel("Q124", "Dejar abierta la puerta del refrigerador en la cocina",
             "Si se deja abierta la puerta de un refrigerador doméstico dentro de la cocina, ¿qué ocurrirá con la temperatura de la habitación?",
             {"A": "Permanecerá igual, pues al estar en equilibrio no habrá flujo neto de calor.",
              "B": "La cocina se calentará, pues el frigorífico expulsa al ambiente tanto el calor extraído de su interior como el calor generado por el compresor.",
              "C": "Solo se enfriará si se usa una hielera con hielo, no con aparato mecánico.",
              "D": "La cocina se enfriará, porque el refrigerador extrae calor del aire interior."}, "B",
             claves=["refrigerador abierto", "balance de energía"],
             visual={"tipo": "maquina", "modo": "refrigerador", "QH": 500, "QC": 400, "W": 100})
    L.paso("Balance de energía dentro de la cocina")\
        .di("El refrigerador toma calor del aire del frente y lo bota por la parrilla trasera, sumado al trabajo eléctrico del compresor.", DATO("puerta de un refrigerador"),
            F(r"|Q_H| = |Q_C| + |W|"), V(foco="balance"))\
        .di("Como el frente y la parte trasera están en el mismo cuarto, el efecto neto es que entra energía eléctrica como calor: la cocina se calienta.",
            R(r"\Delta E_{\text{cocina}} = |Q_H| - |Q_C| = +|W| > 0"))
    revisar_opciones(L, {
        "A": ("La A olvida el trabajo eléctrico que entra al compresor.", "Entra potencia eléctrica $W > 0$."),
        "C": ("La C no responde qué pasa con la cocina al dejar la puerta abierta.", "No explica el calentamiento."),
        "D": ("La D ignora la parrilla trasera que bota más calor del que absorbe el frente.", "$|Q_H| > |Q_C|$."),
        "B": ("La B es la correcta: la cocina se calienta.", "Expulsa $|Q_C| + |W|$ al cuarto."),
    })
    return L.oficial("III Parcial ordinario I-2025", "B").usa(r"|Q_H|=|Q_C|+|W|")


# ============================================================================ Q125
@leccion("Q125")
def q125() -> Leccion:
    L = _sel("Q125", "Máquina reversible A frente a irreversible B entre 700 K y 350 K",
             "Dos máquinas A (reversible) y B (irreversible) operan entre $700$ K y $350$ K recibiendo el mismo calor $Q_H$. Según la Segunda Ley, ¿cuál afirmación es correcta?",
             {"A": "B tiene mayor eficiencia que A, porque las máquinas reales disipan menos calor.",
              "B": "A tiene mayor eficiencia que B, ya que ninguna máquina puede superar la eficiencia de una máquina reversible entre los mismos focos.",
              "C": "Ambas tienen la misma eficiencia, porque operan entre los mismos focos.",
              "D": "Como motor A es mejor, pero como refrigerador B tiene mejor rendimiento."}, "B",
             claves=["teorema de Carnot", "reversible", "segunda ley"],
             visual={"tipo": "maquina", "QH": 1000, "QC": 500, "W": 500, "TH": 700, "TC": 350})
    L.paso("Teorema de Carnot")\
        .di("Entre dos focos fijos, ninguna máquina real o irreversible puede superar la eficiencia de una máquina reversible.", DATO("reversible"),
            F(r"e_{\text{irrev}} < e_{\text{rev}} = 1 - \frac{T_C}{T_H} = 1 - \frac{350}{700} = 50\,\%"), V(foco="carnot"))\
        .di("Eso vale tanto para motores como para refrigeradores: la máquina reversible A gana siempre.",
            R(r"e_A > e_B"))
    revisar_opciones(L, {
        "A": ("La A dice al revés: las irreversibilidades siempre hacen perder eficiencia.", "Las reales disipan **más** calor."),
        "C": ("La C falla: solo dos máquinas **reversibles** empatan entre los mismos focos.", "B es irreversible."),
        "D": ("La D falla: como refrigerador también gana la reversible.", "$K_{\\text{irrev}} < K_{\\text{rev}}$."),
        "B": ("La B es la correcta: teorema de Carnot.", "$e_A > e_B$."),
    })
    return L.oficial("III Parcial extraordinario II-2025", "B").usa(r"e_{\text{Carnot}}=1-\frac{T_C}{T_H}")


# ============================================================================ Q126
@leccion("Q126")
def q126() -> Leccion:
    L = _sel("Q126", "Por qué es imposible una máquina térmica con 100 % de eficiencia",
             "Un estudiante propone construir sin fricción una máquina térmica con eficiencia del $100\\,\\%$ entre $T_H$ y $T_C$. ¿Por qué es fundamentalmente imposible?",
             {"A": "El trabajo neto en un ciclo siempre es cero.",
              "B": "La temperatura del depósito caliente siempre disminuye.",
              "C": "La primera ley impide transformar energía interna en trabajo.",
              "D": "La segunda ley establece que una parte del calor absorbido debe necesariamente transferirse al foco frío."}, "D",
             claves=["Kelvin-Planck", "eficiencia 100%", "segunda ley"])
    L.paso("Enunciado de Kelvin-Planck")\
        .di("Incluso sin fricción, para cerrar un ciclo es obligatorio expulsar una parte del calor al foco frío.", DATO("eficiencia del 100 %"),
            F(r"e = 1 - \frac{|Q_C|}{Q_H} < 1 \quad\text{porque } |Q_C| > 0"))\
        .di("Esa imposibilidad no es tecnológica, sino una ley fundamental: la segunda ley de la termodinámica.",
            R(r"|Q_C| > 0 \;\Rightarrow\; e < 100\,\%"))
    revisar_opciones(L, {
        "A": ("La A es falsa: en una máquina el trabajo neto es positivo; lo que es cero es delta U.", "$W_{\\text{neto}} > 0$."),
        "B": ("La B contradice el enunciado: los depósitos tienen temperaturas constantes.", "$T_H$ es constante."),
        "C": ("La C es falsa: la primera ley sí permite que todo el calor se vuelva trabajo; la que lo prohíbe es la segunda.", "Es la 2.ª ley, no la 1.ª."),
        "D": ("La D es la correcta: siempre debe cederse calor al foco frío.", "Enunciado de Kelvin-Planck."),
    })
    return L.oficial("III Parcial ordinario II-2025", "D").usa(r"e=1-\frac{|Q_C|}{Q_H}")


# ============================================================================ Q127
@leccion("Q127")
def q127() -> Leccion:
    L = _sel("Q127", "Segunda ley y entropía al prohibir el 100 % de eficiencia",
             "¿Por qué, según la segunda ley de la termodinámica, es imposible convertir el $100\\,\\%$ del calor absorbido en trabajo útil en un ciclo sin ceder calor a un foco frío?",
             {"A": "Porque violaría la conservación de la energía (primera ley).",
              "B": "Porque siempre existe fricción en los componentes mecánicos.",
              "C": "Porque el calor es una forma de energía menos útil que el trabajo mecánico.",
              "D": "Porque convertir todo el calor en trabajo reduciría la entropía del universo, lo cual no ocurre espontáneamente."}, "D",
             claves=["segunda ley", "entropía del universo", "Kelvin-Planck"])
    L.paso("Verlo con entropía")\
        .di("En un ciclo el gas vuelve a su estado inicial, así que su entropía no cambia.", DATO("segunda ley"),
            M(r"\Delta S_{\text{máquina}} = 0 \qquad \Delta S_{\text{foco caliente}} = -\frac{Q_H}{T_H} < 0"))\
        .di("Si no se entregara calor a un foco frío, la entropía total del universo bajaría, violando la segunda ley.",
            F(r"\Delta S_{\text{universo}} = -\frac{Q_H}{T_H} < 0 \quad (\text{imposible})"))
    revisar_opciones(L, {
        "A": ("La A es falsa: convertir todo en trabajo sí conservaría energía.", "No viola la 1.ª ley."),
        "B": ("La B falla: incluso sin fricción sigue siendo imposible.", "No es un problema de fricción."),
        "C": ("La C es una frase vaga que no enuncia la razón física fundamental.", "Falta el principio de entropía."),
        "D": ("La D es la correcta: disminuiría la entropía del universo.", "$\\Delta S_{\\text{univ}} \\ge 0$."),
    })
    return L.oficial("III Parcial extraordinario I-2025", "D").usa(r"\Delta S_{\text{univ}}\ge 0")


# ============================================================================ Q128
@leccion("Q128")
def q128() -> Leccion:
    g = Gas(1.00, "mono")
    g.pone("A", p=5.0 * ATM, v=0.010)
    g.pone("B", p=1.0 * ATM, v=0.050)
    g.pone("C", p=1.0 * ATM, v=0.010)
    rab = g.proceso("A", "B", "isotermico")
    rbc = g.proceso("B", "C", "isobarico")
    rca = g.proceso("C", "A", "isocorico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    wnet = rab["W"] + rbc["W"] + rca["W"]
    qh = rab["Q"] + rca["Q"]
    qc = rbc["Q"]
    ecar = 1.0 - g.t("C") / g.t("A")
    L = _des("Q128", "Ciclo isotérmico-isobárico-isocórico (1 mol monoatómico) y Carnot",
             "Una muestra de $1{,}00$ mol de gas ideal monoatómico recorre: $A(5\\ \\text{atm}, 10\\ \\text{L})\\to B(1\\ \\text{atm}, 50\\ \\text{L})$ isotérmico; $B\\to C(1\\ \\text{atm}, 10\\ \\text{L})$ isobárico; $C\\to A$ isocórico. A) Trabajo neto. B) Calor absorbido $Q_H$. C) Calor expulsado $Q_C$. D) Eficiencia de Carnot entre las temperaturas extremas.",
             claves=["ciclo", "monoatómico", "Carnot"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "isobarico"), ("C", "A", "isocorico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("Estados y temperaturas del ciclo")\
        .di("Como pe por ve vale igual en A y en B, ambos están en la misma isoterma caliente; C está cinco veces más frío.", DATO("expansión isotérmica"),
            tabla_estados(g, ["A", "B", "C"], up="atm", uv="L", fp=1.0 / ATM, fv=1e3, cp=3, cv=3))
    explica_proceso(L.paso("A1) Expansión isotérmica A→B"), g, "A", "B", "isotermico", foco="AB")
    explica_proceso(L.paso("A2) Compresión isobárica B→C"), g, "B", "C", "isobarico", foco="BC")
    explica_proceso(L.paso("A3) Calentamiento isocórico C→A"), g, "C", "A", "isocorico", foco="CA")
    L.paso("A, B, C y D) Totales del ciclo y Carnot")\
        .di(f"Sumamos trabajos y calores: trabajo neto {nu(wnet, 'J', 4)}, calor entrante {nu(qh, 'J', 5)} y calor cedido {nu(qc, 'J', 5)}.", V(foco="ciclo"),
            tabla_quw(res),
            R(rf"W_{{\text{{neto}}}} = {tex(wnet, 4, u='J')}\quad Q_H = {tex(qh, 5, u='J', sci='no')}\quad Q_C = {tex(qc, 5, u='J', sci='no')}", "rabc"))\
        .di("La eficiencia de Carnot entre la temperatura mínima en C y la máxima en A da exactamente ochenta por ciento.",
            F(r"e_{\text{Carnot}} = 1 - \frac{T_C}{T_A} = 1 - \frac{p_C}{p_A} = 1 - \frac{1}{5}"),
            R(rf"e_{{\text{{Carnot}}}} = {tex(ecar, dec=2)} = 80\,\%", "rd"))
    L.responde("A", rf"W_{{\text{{neto}}}}={tex(wnet, 4)}\ \text{{J}}", wnet, "J")\
     .responde("B", rf"Q_H={tex(qh, 5, sci='no')}\ \text{{J}}", qh, "J")\
     .responde("C", rf"Q_C={tex(qc, 5, sci='no')}\ \text{{J}}", qc, "J")\
     .responde("D", rf"e_{{\text{{Carnot}}}}={tex(ecar, dec=2)}", ecar, "")
    return L.oficial("III Parcial extraordinario I-2025", 4099.0, 14229.0, -10130.0, 0.80).usa(r"W=nRT\ln\frac{V_2}{V_1}", r"e_{\text{Carnot}}=1-\frac{T_C}{T_H}")


# ============================================================================ Q129
@leccion("Q129")
def q129() -> Leccion:
    L = _sel("Q129", "Enunciado correcto de la segunda ley (Kelvin-Planck)",
             "La segunda ley de la termodinámica se puede enunciar de la siguiente manera:",
             {"A": "No hay máquinas térmicas que transformen todo el calor absorbido en trabajo.",
              "B": "No hay máquinas térmicas en donde el calor cedido sea superior al calor absorbido.",
              "C": "No hay máquinas térmicas que transformen todo el calor absorbido en calor cedido.",
              "D": "No hay máquinas térmicas en donde el trabajo realizado sea superior al calor cedido."}, "A",
             claves=["Kelvin-Planck", "segunda ley"])
    L.paso("Identificar el enunciado de Kelvin-Planck")\
        .di("El enunciado de Kelvin-Planck afirma que es imposible una máquina cíclica cuyo único efecto sea absorber calor y convertirlo todo en trabajo.", DATO("segunda ley"),
            R(r"\text{Siempre } |Q_C| > 0 \;\Rightarrow\; W < Q_H"))
    revisar_opciones(L, {
        "B": ("La B habla de balance de energía en un motor, pero un refrigerador sí expulsa más calor del que absorbe del foco frío.", "Confunde máquina y refrigerador."),
        "C": ("La C es falsa: conducir calor directo de caliente a frío convierte todo el calor absorbido en calor cedido.", "Sí ocurre espontáneamente."),
        "D": ("La D es falsa: cualquier motor con eficiencia mayor al cincuenta por ciento produce más trabajo que calor cedido.", "Si $e>0{,}5$, $W > |Q_C|$."),
        "A": ("La A es la correcta: enunciado de Kelvin-Planck.", "Imposible eficiencia del $100\\,\\%$."),
    })
    return L.oficial("III Parcial extraordinario I-2026", "A").usa(r"e=\frac{W}{Q_H}<1")
