"""Superexamen Q117–Q129: máquinas térmicas, refrigeradores, Carnot y segunda ley."""

from __future__ import annotations

import math

from .ayudas import ATM, R_GAS, Gas, explica_proceso, revisar_opciones, tabla_estados, tabla_quw, valores_visual, visual_pv, estado_T, estado_p, cierre_ciclo, deriva_carnot
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .formato import dice, n, nu, tex
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
             aviso="El ciclo de Carnot (sección 20.6) no entra en el III parcial; aquí solo sirve para ver la tendencia. Lo que sí entra es K = |Q_C|/|W| de la hoja.",
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
    # 3->1 isobárico => p1 = p3
    g.pone("1", p=p3, v=v1)
    g.pone("2", v=v2, t=g.t("1"))
    # 2->3 adiabático hasta p3: p2 V2^g = p3 V3^g
    f35 = (g.p("2") / p3) ** (1.0 / g.g)
    v3 = v2 * f35
    g.pone("3", p=p3, v=v3)
    r12 = g.proceso("1", "2", "isotermico")
    r23 = g.proceso("2", "3", "adiabatico")
    r31 = g.proceso("3", "1", "isobarico")
    res = [("1→2", r12), ("2→3", r23), ("3→1", r31)]
    L = _des("Q120", "Refrigerador de gas ideal monoatómico (3 etapas)",
             "Un refrigerador opera con $7{,}107\\times 10^{-2}$ mol de gas ideal monoatómico: $1\\to 2$ compresión isotérmica de $V_1=3{,}50\\times 10^{-3}\\ \\text{m}^3$ a $V_2=1{,}40\\times 10^{-3}\\ \\text{m}^3$; $2\\to 3$ expansión adiabática hasta $p_3=1{,}013\\times 10^5$ Pa; $3\\to 1$ expansión isobárica cerrando el ciclo. A) Diagrama $p$-$V$. B) $T_1, p_2, T_3, V_3$. C) $W, Q, \\Delta U$. D) $K$.",
             claves=["refrigerador", "ciclo antihorario", "monoatómico"],
             visual=visual_pv(g, [("1", "2", "isotermico"), ("2", "3", "adiabatico"), ("3", "1", "isobarico")],
                              uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Tres etapas: comprimir sin cambiar la temperatura, expandir sin calor y expandir a presión constante hasta cerrar.", DATO("expande isobáricamente"),
            LISTA(["$n=7{,}107\\times10^{-2}$ mol, monoatómico",
                   "$1\\to2$: $T$ constante, de $V_1=3{,}50\\times10^{-3}$ m³ a $V_2=1{,}40\\times10^{-3}$ m³",
                   "$2\\to3$: adiabático hasta $p_3=1{,}013\\times10^5$ Pa",
                   "$3\\to1$: $p$ constante, así que $p_1=p_3$"]))\
        .di("Monoatómico: de la hoja salen la capacidad a volumen constante, la de presión constante y gama.",
            F(r"C_V=\frac{3}{2}R"), F(r"C_P=C_V+R"), F(r"\gamma=\frac{C_P}{C_V}"))\
        .di("Sustituimos: gama vale cinco tercios.",
            M(rf"C_V=\frac{{3}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"),
            M(r"C_P=\frac{3}{2}R+R=\frac{5}{2}R\qquad \gamma=\frac{\frac{5}{2}R}{\frac{3}{2}R}=\frac{5}{3}"))
    p = L.paso("B) Estado 1 y temperatura de 2")
    p.di("Como de tres a uno la presión no cambia, la presión de uno es la de tres.", V(foco="31"), M(r"p_1=p_3=1{,}013\times10^5\ \text{Pa}"))
    estado_T(p, g, "1", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en 1.")
    p.di("De uno a dos la temperatura no cambia.", V(foco="12"), M(rf"T_2=T_1={tex(g.t('1'), dec=1, u='K')}"))
    L.paso("B) Presión de 2")\
        .di("A temperatura constante, del gas ideal se conserva el producto de presión por volumen.", F(r"pV=nRT"),
            M(r"p_1V_1=p_2V_2\;\Rightarrow\;p_2=p_1\,\frac{V_1}{V_2}"))\
        .di("Sustituimos: la razón de volúmenes da dos coma cinco.",
            M(r"p_2=(1{,}013\times10^5)\,\frac{3{,}50\times10^{-3}}{1{,}40\times10^{-3}}=(1{,}013\times10^5)(2{,}5)"))\
        .di("Ya tenemos la presión de 2.", M(rf"p_2={tex(g.p('2'), 5, u='Pa')}", "p2", "verde"))
    L.paso("B) Volumen de 3: despeje con la adiabática")\
        .di("De dos a tres no hay calor. La hoja da la relación de presión y volumen en la adiabática.", V(foco="23"), F(r"pV^\gamma=\text{cte}"))\
        .di("La igualamos entre 2 y 3.", M(r"p_3V_3^{\gamma}=p_2V_2^{\gamma}"))\
        .di("Dividimos los dos lados entre la presión de 3, para dejar solo lo que tiene el volumen de 3.",
            M(r"V_3^{\gamma}=V_2^{\gamma}\,\frac{p_2}{p_3}"))\
        .di("Para quitar el exponente gama, elevamos los dos lados a uno sobre gama: al multiplicar los exponentes queda uno.",
            M(r"\left(V_3^{\gamma}\right)^{1/\gamma}=V_3^{\gamma\cdot\frac{1}{\gamma}}=V_3^{1}=V_3"),
            M(r"V_3=V_2\left(\frac{p_2}{p_3}\right)^{1/\gamma}"))
    L.paso("B) Volumen de 3: los números")\
        .di("Como gama es cinco tercios, uno sobre gama es tres quintos, o sea cero coma seis.",
            M(r"\frac{1}{\gamma}=\frac{3}{5}=0{,}6"),
            M(r"\frac{p_2}{p_3}=\frac{2{,}5325\times10^5}{1{,}013\times10^5}=2{,}5"))\
        .di("En la calculadora: dos coma cinco elevado a cero coma seis.",
            M(rf"V_3=(1{{,}}40\times10^{{-3}})(2{{,}}5)^{{0{{,}}6}}=(1{{,}}40\times10^{{-3}})({tex(f35, 5)})"))\
        .di("El volumen de 3.", M(rf"V_3={tex(v3, 4, u='m^3')}", "v3", "verde"))
    estado_T(L.paso("B) Temperatura de 3"), g, "3", voz="Y con el gas ideal, la temperatura de 3.")
    explica_proceso(L.paso("C1) Compresión isotérmica 1→2"), g, "1", "2", "isotermico", foco="12")
    explica_proceso(L.paso("C2) Expansión adiabática 2→3"), g, "2", "3", "adiabatico", foco="23")
    explica_proceso(L.paso("C3) Expansión isobárica 3→1"), g, "3", "1", "isobarico", foco="31")
    L.paso("D) ¿Motor o refrigerador?")\
        .di("De 1 a 2 sube hacia la izquierda por la isoterma, baja de 2 a 3 por la adiabática, que va por debajo, y vuelve por abajo hasta 1: gira en sentido antihorario.", V(foco="ciclo"))\
        .di("En sentido antihorario el trabajo neto es negativo: es un refrigerador.",
            M(r"\text{antihorario}\;\Rightarrow\;W_{\text{neto}}<0\;\Rightarrow\;\text{refrigerador}"))
    c = cierre_ciclo(L, res, refri=True, titulo="D) Coeficiente de rendimiento del refrigerador", id_e="rd")
    L.responde("T1", rf"T_1={tex(g.t('1'), dec=1)}\ \text{{K}}", g.t("1"), "K")\
     .responde("Wnet", rf"W_{{\text{{neto}}}}={tex(c['W'], 3)}\ \text{{J}}", c["W"], "J")\
     .responde("K", rf"K={tex(c['K'], 3)}", c["K"], "")
    return L.usa(r"pV=nRT", r"pV^\gamma=\text{cte}", r"K=\frac{|Q_C|}{|W|}")

# ============================================================================ Q121
@leccion("Q121")
def q121() -> Leccion:
    L = _sel("Q121", "Propiedades de un ciclo de máquina térmica",
             "Para un ciclo termodinámico que representa una máquina térmica: I. El trabajo neto es positivo. II. El ciclo en p-V se recorre en sentido antihorario. III. El cambio total de energía interna es cero. ¿Cuáles son correctas?",
             {"A": "I y II", "B": "II y III", "C": "I y III", "D": "I, II y III"}, "C",
             claves=["máquina térmica", "sentido horario", "ciclo"])
    L.paso("Revisar las tres propiedades")\
        .di("Con la primera ley y el trabajo como área en el diagrama p-V revisamos cada afirmación del ciclo.", DATO("máquina térmica"),
            F(r"\Delta U=Q-W"), F(r"W=\int_{V_1}^{V_2}p\,dV"), F(r"e=\frac{W}{Q_H}"))\
        .di("Un motor entrega trabajo positivo, gira en sentido horario y, al cerrar el ciclo, su cambio de energía interna es cero.",
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
    return L.oficial("III Parcial extraordinario II-2024", "C").usa(r"\Delta U=Q-W", r"W=\int_{V_1}^{V_2}p\,dV", r"e=\frac{W}{Q_H}")


# ============================================================================ Q122
@leccion("Q122")
def q122() -> Leccion:
    n_mol, ta, tb, tc, pa = 0.300, 400.0, 300.0, 267.2, 3.6e6
    rt = tc / tb
    gm1 = math.log(rt) / math.log(0.75)
    gamma = 1.0 + gm1
    cv = R_GAS / gm1
    cp = cv + R_GAS
    K_carnot = tc / (ta - tc)
    g = Gas(n_mol, cv=cv)
    g.pone("A", p=pa, t=ta)
    g.pone("B", p=pa, t=tb)
    g.pone("C", v=g.v("A"), t=tc)
    rab = g.proceso("A", "B", "isobarico")
    rbc = g.proceso("B", "C", "adiabatico")
    rca = g.proceso("C", "A", "isocorico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    L = _des("Q122", "Ciclo de compresión-expansión: hallar γ, C_p, K y límite de Carnot",
             "Un ciclo con $0{,}300$ mol de gas ideal: $A\\to B$ compresión isobárica de $400$ K a $300$ K ($V_B=0{,}75V_A$, $p_A=3{,}6\\times 10^6$ Pa); $B\\to C$ expansión adiabática hasta $V_A$ y $267{,}2$ K; $C\\to A$ calentamiento isocórico. A) $\\gamma$. B) $C_p$. C) $K$ del ciclo. D) $K_{\\text{Carnot}}$. E) ¿Es posible operar entre esos dos focos?",
             aviso="Los incisos D y E usan el refrigerador de Carnot (sección 20.6), que no entra en el III parcial. A, B y C sí entran. "
                   "Ojo: la conclusión E del solucionario oficial («el ciclo es imposible») es un error; aquí se explica por qué el ciclo sí es posible.",
             claves=["gamma", "refrigerador", "Carnot", "posible", "segunda ley"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "adiabatico"), ("C", "A", "isocorico")],
                              uv="cm³", up="MPa", fv=1e6, fp=1e-6, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("No dicen si el gas es mono o diatómico: gama hay que sacarla de la adiabática.".replace("gama", dice("γ", "gama")), DATO("75 % de su volumen"),
            LISTA(["$n=0{,}300$ mol, $p_A=3{,}6\\times10^6$ Pa", "$A$: $400$ K · $B$: $300$ K, $V_B=0{,}75V_A$", "$C$: $267{,}2$ K, $V_C=V_A$"]))
    L.paso("A) γ con la adiabática B→C")\
        .di("La hoja da la relación de temperatura y volumen en la adiabática.", V(foco="BC"), F(r"TV^{\gamma-1}=\text{cte}"))\
        .di("La igualamos entre B y C, y juntamos las razones.", M(r"T_BV_B^{\gamma-1}=T_CV_C^{\gamma-1}\;\Rightarrow\;\left(\frac{V_B}{V_C}\right)^{\gamma-1}=\frac{T_C}{T_B}"))\
        .di("Como ve ce es ve a, la razón de volúmenes es cero coma setenta y cinco.".replace("ve ce es ve a", dice("V_C = V_A", "ve ce es ve a")), M(rf"(0{{,}}75)^{{\gamma-1}}=\frac{{267{{,}}2}}{{300}}={tex(rt, 6)}"))\
        .di("Sacamos logaritmo a los dos lados para bajar el exponente. Con seis cifras: gama queda justo entre dos redondeos.".replace("gama", dice("γ", "gama")),
            M(rf"(\gamma-1)\ln(0{{,}}75)=\ln({tex(rt, 6)})\;\Rightarrow\;\gamma-1=\frac{{{tex(math.log(rt), 6)}}}{{{tex(math.log(0.75), 6)}}}={tex(gm1, 5)}"))\
        .di(f"Gama vale {n(gamma, 3)}: es un gas diatómico.".replace("Gama", dice("γ", "Gama")), R(rf"\gamma=1+{tex(gm1, 5)}={tex(gamma, 4)}", "ra"))
    L.paso("A) Otra forma: con las presiones (como el solucionario)")\
        .di("De C a A el volumen no cambia: del gas ideal, la presión va en proporción a la temperatura.", V(foco="CA"), F(r"pV=nRT"),
            M(r"\frac{p_C}{T_C}=\frac{p_A}{T_A}\;\Rightarrow\;p_C=p_A\,\frac{T_C}{T_A}=p_A\,\frac{267{,}2}{400}=0{,}668\,p_A"))\
        .di("En la adiabática de B a C usamos la relación de presión y volumen; pe be es pe a.".replace("pe be es pe a", dice("p_B = p_A", "pe be es pe a")), F(r"pV^\gamma=\text{cte}"),
            M(r"p_BV_B^{\gamma}=p_CV_C^{\gamma}\;\Rightarrow\;\left(\frac{V_B}{V_C}\right)^{\gamma}=\frac{p_C}{p_B}\;\Rightarrow\;(0{,}75)^{\gamma}=0{,}668"))\
        .di("Con logaritmos sale la misma gama.".replace("gama", dice("γ", "gama")), M(rf"\gamma=\frac{{\ln(0{{,}}668)}}{{\ln(0{{,}}75)}}=\frac{{{tex(math.log(tc / ta), 6)}}}{{{tex(math.log(0.75), 6)}}}={tex(math.log(tc / ta) / math.log(0.75), 4)}"))
    L.paso("B) C_p desde las dos casillas de la hoja")\
        .di("La hoja trae dos relaciones entre las capacidades.", F(r"C_P=C_V+R"), F(r"\gamma=\frac{C_P}{C_V}"))\
        .di("Ponemos ce pe igual a gama ce ve en la primera.".replace("ce pe", dice("C_P", "ce pe")).replace("gama ce ve", dice("γC_V", "gama ce ve")), M(r"\gamma C_V=C_V+R\;\Rightarrow\;C_V(\gamma-1)=R\;\Rightarrow\;C_V=\frac{R}{\gamma-1}"))\
        .di("Sustituimos.", M(rf"C_V=\frac{{8{{,}}314}}{{{tex(gm1, 5)}}}={tex(cv, 4)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))\
        .di("Y ce pe es ce ve más erre.".replace("ce pe es ce ve más erre", dice("C_P = C_V + R", "ce pe es ce ve más erre")), M(rf"C_P={tex(cv, 4)}+8{{,}}314"),
            R(rf"C_p={tex(cp, 4)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}", "rb"))
    explica_proceso(L.paso("C1) Compresión isobárica A→B"), g, "A", "B", "isobarico", foco="AB")
    explica_proceso(L.paso("C2) Expansión adiabática B→C"), g, "B", "C", "adiabatico", foco="BC")
    explica_proceso(L.paso("C3) Calentamiento isocórico C→A"), g, "C", "A", "isocorico", foco="CA")
    c = cierre_ciclo(L, res, refri=True, titulo="C) Coeficiente de rendimiento del ciclo", id_e="rc")
    deriva_carnot(L.paso("D) Carnot: de dónde sale su rendimiento (no entra en el parcial)"), refri=True)
    L.paso("D) El número de Carnot que pide el solucionario")\
        .di("El solucionario toma la temperatura más baja y la más alta del ciclo como si fueran dos focos fijos.", M(r"K_{\text{Carnot}} = \frac{T_C}{T_H - T_C} = \frac{267{,}2}{400 - 267{,}2}=\frac{267{,}2}{132{,}8}"),
            R(rf"K_{{\text{{Carnot}}}} = {tex(K_carnot, 3)}", "rd"))
    L.paso("E) ¿Es imposible el ciclo? No: esa comparación no aplica")\
        .di(f"El ciclo da {n(c['K'], 3)}, más que {n(K_carnot, 3)}. El solucionario concluye que es imposible. Esa conclusión es un error.",
            OJO("El solucionario oficial dice «imposible». Está mal: el ciclo sí se puede hacer."))\
        .di("El límite de Carnot vale cuando el gas toma calor de un foco frío y lo bota en un foco caliente, cada uno a una sola temperatura.",
            M(r"K\le\frac{T_C}{T_H-T_C}\quad\text{solo si todo el calor entra a }T_C\text{ y sale a }T_H"))\
        .di("Aquí no pasa eso. El gas toma calor de C a A mientras se calienta hasta cuatrocientos kelvin.",
            M(r"Q_{CA}>0\quad\text{entra entre }267{,}2\text{ K y }400\text{ K}"))\
        .di("Y lo bota de A a B mientras se enfría de cuatrocientos a trescientos kelvin.",
            M(r"Q_{AB}<0\quad\text{sale entre }400\text{ K y }300\text{ K}"))
    L.paso("E) Conclusión correcta")\
        .di("Toma calor más caliente que parte del que bota: no hay un foco frío y uno caliente separados. Comparar con Carnot no tiene sentido.")\
        .di("Además, los tres procesos son reversibles y posibles para un gas ideal. Un ciclo hecho con ellos siempre se puede hacer.",
            R(r"\text{El ciclo sí es posible: no viola la segunda ley}", "re"),
            NOTA("Comprobación con entropía (fuera del parcial): sumar todos los pedacitos $\\frac{dQ}{T}$ del ciclo da "
                 "$nC_p\\ln\\frac{300}{400}+nC_V\\ln\\frac{400}{267{,}2}=0$. Un ciclo imposible daría un número mayor que cero."))
    L.responde("A", rf"\gamma={tex(gamma, 3)}", gamma, "")\
     .responde("B", rf"C_p={tex(cp, 3)}\ \text{{J/(mol}}\cdot\text{{K)}}", cp, "J/(mol·K)")\
     .responde("C", rf"K={tex(c['K'], 4)}", c["K"], "")\
     .responde("D", rf"K_{{\text{{Carnot}}}}={tex(K_carnot, 3)}", K_carnot, "")\
     .responde("E", r"\text{sí es posible (la comparación con Carnot no aplica)}")
    return L.oficial("III Parcial ordinario II-2024", 1.40, 29.1, 18.44, 2.01, tol=0.04,
                     nota="El oficial redondeó γ=1,402 a 1,40 (Cp=29,10 → K=18,44); sin redondear da K≈17,9. "
                          "En E el oficial concluye «imposible»: es un error, porque el ciclo es reversible y no tiene dos focos separados.").usa(r"TV^{\gamma-1}=\text{cte}", r"C_P=C_V+R", r"K=\frac{|Q_C|}{|W|}")

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
             aviso="Comparar una máquina reversible con una irreversible usa el ciclo de Carnot (sección 20.6), que no entra en el III parcial: el corte llega hasta la 20.5.",
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
             aviso="La entropía (sección 20.7) no entra en el III parcial. Lo que sí entra es la segunda ley de Kelvin-Planck (20.5), que es el primer paso.",
             claves=["segunda ley", "entropía del universo", "Kelvin-Planck"])
    L.paso("Primero con la hoja: Kelvin-Planck")\
        .di("La eficiencia de la hoja. Si no se cede calor al foco frío, el calor que sale es cero.", DATO("segunda ley"),
            F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_c}{Q_H}\right|"), M(r"|Q_c|=0\;\Rightarrow\;e=1\;\Rightarrow\;W=Q_H"))\
        .di("La primera ley no lo prohíbe: la energía se conservaría. Lo prohíbe la segunda ley: ninguna máquina cíclica convierte todo el calor en trabajo.",
            M(r"\text{segunda ley: }|Q_c|>0\;\Rightarrow\;e<1"))
    L.paso("Verlo con entropía (no entra en el III parcial)")\
        .di("El cambio de entropía es el calor que entra entre la temperatura. El foco caliente no cambia de temperatura, así que la te sale de la suma.".replace("la te", "la " + dice("T", "te")),
            M(r"dS=\frac{dQ}{T}\;\Rightarrow\;\Delta S=\int\frac{dQ}{T}=\frac{1}{T}\int dQ=\frac{Q}{T}"))\
        .di("El foco caliente pierde ese calor; el gas da la vuelta completa y su entropía no cambia.",
            M(r"\Delta S_{\text{foco caliente}} = -\frac{Q_H}{T_H} < 0 \qquad \Delta S_{\text{máquina}} = 0"))\
        .di("Sin foco frío que reciba calor, nada lo compensa: la entropía del universo bajaría, y eso no ocurre solo. Es la D.",
            M(r"\Delta S_{\text{universo}} = -\frac{Q_H}{T_H} < 0 \quad (\text{imposible})"))
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
    ecar = 1.0 - g.t("C") / g.t("A")
    L = _des("Q128", "Ciclo isotérmico-isobárico-isocórico (1 mol monoatómico) y Carnot",
             "Una muestra de $1{,}00$ mol de gas ideal monoatómico recorre: $A(5\\ \\text{atm}, 10\\ \\text{L})\\to B(1\\ \\text{atm}, 50\\ \\text{L})$ isotérmico; $B\\to C(1\\ \\text{atm}, 10\\ \\text{L})$ isobárico; $C\\to A$ isocórico. A) Trabajo neto. B) Calor absorbido $Q_H$. C) Calor expulsado $Q_C$. D) Eficiencia de Carnot entre las temperaturas extremas.",
             aviso="El inciso D es Carnot (sección 20.6): no entra en el III parcial. A, B y C sí entran.",
             claves=["ciclo", "monoatómico", "Carnot"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "isobarico"), ("C", "A", "isocorico")],
                              uv="L", up="atm", fv=1e3, fp=1.0 / ATM, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Tres procesos: una isoterma, una compresión a presión constante y un calentamiento a volumen constante.", DATO("expansión isotérmica"),
            LISTA(["$n=1{,}00$ mol, monoatómico: $C_V=\\frac{3}{2}R$", "$A$: $5$ atm y $10$ L", "$B$: $1$ atm y $50$ L", "$C$: $1$ atm y $10$ L"]))\
        .di("En unidades del SI.", M(r"5\ \text{atm}=5(101\,325)=5{,}066\times10^5\ \text{Pa}"), M(r"1\ \text{atm}=1{,}013\times10^5\ \text{Pa}"), M(r"10\ \text{L}=0{,}010\ \text{m}^3"), M(r"50\ \text{L}=0{,}050\ \text{m}^3"))
    p = L.paso("Temperaturas de los tres estados")
    estado_T(p, g, "A", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en A.")
    estado_T(p, g, "B", voz="En B sale la misma: A y B están en la misma isoterma.")
    estado_T(p, g, "C", voz="Y en C, cinco veces más frío.")
    explica_proceso(L.paso("A1) Expansión isotérmica A→B"), g, "A", "B", "isotermico", foco="AB")
    explica_proceso(L.paso("A2) Compresión isobárica B→C"), g, "B", "C", "isobarico", foco="BC")
    explica_proceso(L.paso("A3) Calentamiento isocórico C→A"), g, "C", "A", "isocorico", foco="CA")
    c = cierre_ciclo(L, res, titulo="A, B y C) Trabajo neto y calores", id_e="re")
    deriva_carnot(L.paso("D) Carnot: de dónde sale su eficiencia (no entra en el parcial)"))
    L.paso("D) Eficiencia de Carnot entre las temperaturas extremas")\
        .di("La temperatura más alta es la de A y la más baja la de C.", V(foco="ciclo"), M(rf"T_H=T_A={tex(g.t('A'), 4, u='K')}\qquad T_C={tex(g.t('C'), 4, u='K')}"))\
        .di("Sustituimos.", M(rf"e_{{\text{{Carnot}}}}=1-\frac{{{tex(g.t('C'), 4)}}}{{{tex(g.t('A'), 4)}}}=1-0{{,}}200"),
            R(rf"e_{{\text{{Carnot}}}} = {tex(ecar, dec=2)} = 80\,\%", "rd"))\
        .di("La real es menor, como debe ser.", M(rf"e={tex(c['e'], 3)}<e_{{\text{{Carnot}}}}={tex(ecar, dec=2)}"))
    L.responde("A", rf"W_{{\text{{neto}}}}={tex(c['W'], 4)}\ \text{{J}}", c["W"], "J")\
     .responde("B", rf"Q_H={tex(c['QH'], 5, sci='no')}\ \text{{J}}", c["QH"], "J")\
     .responde("C", rf"Q_C={tex(c['QC'], 5, sci='no')}\ \text{{J}}", c["QC"], "J")\
     .responde("D", rf"e_{{\text{{Carnot}}}}={tex(ecar, dec=2)}", ecar, "")
    return L.oficial("III Parcial extraordinario I-2025", 4099.0, 14229.0, -10130.0, 0.80).usa(r"pV=nRT", r"W=nRT\ln\left(\frac{V_2}{V_1}\right)", r"e=1-\left|\frac{Q_c}{Q_H}\right|")

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
