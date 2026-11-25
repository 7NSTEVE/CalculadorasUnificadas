"""Superexamen Q083–Q091: diagramas p-V, trabajo por volumen, energía interna y primera ley."""

from __future__ import annotations

from .ayudas import Gas, revisar_opciones, visual_pv
from .dsl import DATO, F, LISTA, M, OJO, Leccion, R, TABLA, V
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion


def _sel(q: str, titulo: str, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), tipo="seleccion", fuente=fuente(q), enunciado=enunciado,
                   opciones=opciones, correcta=correcta, **k)


# ============================================================================ Q083
@leccion("Q083")
def q083() -> Leccion:
    L = _sel("Q083", "Mayor cambio de energía interna entre isotermas",
             "El diagrama p-V muestra cinco procesos (A, B, C, D, E) sobre tres isotermas $T_1 < T_2 < T_3$: A sube de $T_1$ a $T_2$, B sube de $T_1$ a $T_3$, C va de $T_2$ a $T_3$, D sigue $T_2$ y E sigue $T_1$. ¿Cuál proceso tiene mayor cambio en energía interna?",
             {"A": "Proceso A", "B": "Proceso B", "C": "Proceso C", "D": "Proceso D"}, "B",
             aviso="En el Superexamen aparece una figura cambiada; la figura original de 24-II-E muestra los cinco procesos entre las isotermas T₁ < T₂ < T₃, donde B salta de T₁ hasta T₃.",
             claves=["energía interna", "isotermas", "diagrama p-V"])
    L.paso("La energía interna solo depende de la temperatura")\
        .di("En un gas ideal, el cambio de energía interna solo depende de cuánto cambió la temperatura.", DATO("mayor el cambio en la energía interna"),
            F(r"\Delta U = n C_V\,\Delta T"))\
        .di("Comparamos entre qué isotermas empieza y termina cada flecha.",
            TABLA(["Proceso", "Inicio → Fin", "$\\Delta T$"],
                  [["A", "$T_1 \\to T_2$", "$T_2-T_1$"],
                   ["B", "$T_1 \\to T_3$", "$T_3-T_1$ (máximo)"],
                   ["C", "$T_2 \\to T_3$", "$T_3-T_2$"],
                   ["D", "$T_2 \\to T_2$", "$0$"]], resaltar=[[1, 2]]))\
        .di("El proceso B cruza de la isoterma más baja hasta la más alta: tiene el mayor salto de temperatura.",
            R(r"\Delta U_B = nC_V(T_3-T_1)\ \ (\text{máximo})"))
    revisar_opciones(L, {
        "A": ("La A solo sube de te uno a te dos.", "Salto menor que $T_3-T_1$."),
        "C": ("La C solo sube de te dos a te tres.", "Salto menor que $T_3-T_1$."),
        "D": ("La D se queda sobre la misma isoterma: cambio cero.", "$\\Delta T = 0$."),
        "B": ("La B es la correcta: cruza de te uno hasta te tres.", "Mayor $\\Delta T \\Rightarrow$ mayor $\\Delta U$."),
    })
    return L.oficial("III Parcial extraordinario II-2024", "B").usa(r"\Delta U=nC_V\Delta T")


# ============================================================================ Q084
@leccion("Q084")
def q084() -> Leccion:
    L = _sel("Q084", "Signo del trabajo al comprimir un gas",
             "Un mol de gas está contenido en un cilindro con un pistón móvil y una fuerza externa comprime el gas. Sobre el trabajo realizado por el gas podemos afirmar:",
             {"A": "Es nulo porque el pistón se mueve por efecto de la fuerza externa.",
              "B": "Es positivo porque al comprimir el gas se aumenta la temperatura.",
              "C": "Es negativo debido a que disminuye el volumen del gas.",
              "D": "Se puede calcular como el producto de la presión por el cambio de volumen."}, "C",
             claves=["trabajo", "compresión", "signo"])
    L.paso("Definición y signo del trabajo termodinámico")\
        .di("El trabajo hecho por el gas es la integral de la presión por el cambio de volumen.", DATO("comprimiendo el gas"),
            F(r"W = \int_{V_i}^{V_f} p\,dV"))\
        .di("Como el gas se comprime, el volumen final es menor que el inicial y el diferencial de volumen es negativo.",
            M(r"V_f < V_i \;\Rightarrow\; dV < 0 \;\Rightarrow\; W < 0"),
            OJO("No uses $W=p\\Delta V$ a ciegas: solo vale si la presión es constante, y aquí no lo dicen."))
    revisar_opciones(L, {
        "A": ("La A es falsa: sí hay cambio de volumen, así que hay trabajo.", "$\\Delta V \\neq 0$."),
        "B": ("La B es falsa: el signo del trabajo depende de si el volumen sube o baja, no de la temperatura.", "Compresión $\\Rightarrow W < 0$."),
        "D": ("La D es la trampa: pe por delta ve solo sirve si la presión es constante.", "Aquí no dicen que sea isobárico."),
        "C": ("La C es la correcta: el trabajo del gas es negativo porque disminuye el volumen.", "$V_f < V_i \\Rightarrow W < 0$."),
    })
    return L.oficial("II Parcial ordinario II-2023", "C").usa(r"W=\int p\,dV")


# ============================================================================ Q085
@leccion("Q085")
def q085() -> Leccion:
    L = _sel("Q085", "Trayectoria, trabajo, calor y energía interna en un diagrama p-V",
             "En un diagrama $p$-$V$ se tiene que:",
             {"A": "El calor depende del área bajo la curva, para un estado inicial y final definidos.",
              "B": "El trabajo neto entre los estados inicial y final es independiente de la trayectoria.",
              "C": "El trabajo realizado por el sistema depende solamente de los estados inicial y final.",
              "D": "El cambio de la energía interna entre los estados inicial y final depende de la trayectoria seguida."}, "A",
             claves=["función de estado", "trayectoria", "diagrama p-V"])
    L.paso("Qué depende del camino y qué solo de los extremos")\
        .di("La energía interna es función de estado: entre dos puntos fijos, delta U siempre vale lo mismo.", DATO("diagrama p-V"),
            F(r"Q = \Delta U + W"))\
        .di("El trabajo es el área bajo la curva; como delta U queda fijo, el calor sí depende de esa área.",
            LISTA(["$\\Delta U$: solo depende de inicio y fin", "$W$: área bajo la curva (depende del camino)", "$Q = \\Delta U + W$: depende del área bajo la curva"]))
    revisar_opciones(L, {
        "B": ("La B es falsa: el trabajo sí depende de la trayectoria.", "Distinto camino = distinta área."),
        "C": ("La C repite el mismo error: el trabajo no es función de estado.", "Depende de la curva."),
        "D": ("La D dice lo contrario de la realidad: delta U no depende del camino.", "$U$ es función de estado."),
        "A": ("La A es la correcta: con extremos fijos, el calor cambia con el área bajo la curva.", "$Q = \\Delta U_{\\text{fijo}} + W_{\\text{área}}$."),
    })
    return L.oficial("III Parcial extraordinario II-2025", "A").usa(r"\Delta U=Q-W")


# ============================================================================ Q086
@leccion("Q086")
def q086() -> Leccion:
    g = Gas(1.0, "mono")
    g.pone("i", p=3e5, v=0.01)
    g.pone("isob", p=3e5, v=0.025)
    g.pone("isot", v=0.025, t=g.t("i"))
    g.pone("adia", v=0.025, p=g.p("i") * (0.01 / 0.025) ** g.g)
    L = _sel("Q086", "Qué expansión absorbe más calor hasta el mismo volumen final",
             "Un gas ideal se expande desde $(p_i, V_i)$ hasta el mismo volumen final $V_f$ de manera isotérmica, adiabática o isobárica. ¿En qué proceso es mayor el calor transferido al gas?",
             {"A": "Proceso isobárico.", "B": "Proceso adiabático.", "C": "Proceso isotérmico.", "D": "El calor transferido es el mismo en cada proceso."}, "A",
             claves=["isobárico", "isotérmico", "adiabático", "comparación"],
             visual=visual_pv(g, [("i", "isob", "isobarico"), ("i", "isot", "isotermico"), ("i", "adia", "adiabatico")], uv="L", up="kPa", fv=1e3, fp=1e-3))
    L.paso("Comparar trabajo y cambio de energía interna")\
        .di("Por la primera ley, el calor es la suma de delta U más el trabajo.", DATO("mismo volumen final"),
            F(r"Q = \Delta U + W"))\
        .di("En la expansión isobárica la curva va más arriba: tiene la mayor área de trabajo y además la temperatura sube.",
            TABLA(["Proceso", "$W$ (área)", "$\\Delta U$", "$Q = \\Delta U + W$"],
                  [["Isobárico", "Máximo", "$>0$ (se calienta)", "Máximo"],
                   ["Isotérmico", "Intermedio", "$0$", "Intermedio ($Q=W$)"],
                   ["Adiabático", "Mínimo", "$<0$ (se enfría)", "$0$"]], resaltar=[[0, 3]]))\
        .di("Por eso el proceso isobárico absorbe más calor.", R(r"Q_{\text{isob}} > Q_{\text{isot}} > Q_{\text{adiab}} = 0"))
    revisar_opciones(L, {
        "B": ("La B no: en un adiabático el calor es cero.", "$Q=0$."),
        "C": ("La C no: en el isotérmico el área y la temperatura final son menores.", "$\\Delta U = 0$ y menos $W$."),
        "D": ("La D no: el calor depende del camino seguido.", "No es función de estado."),
        "A": ("La A es la correcta: el proceso isobárico.", "Mayor $W$ y $\\Delta U > 0$."),
    })
    return L.oficial("III Parcial ordinario II-2025", "A").usa(r"\Delta U=Q-W")


# ============================================================================ Q087
@leccion("Q087")
def q087() -> Leccion:
    L = _sel("Q087", "Entran 60 kJ de calor y salen 80 kJ de trabajo",
             "En un proceso efectuado por un sistema entran $60$ kJ de calor y el sistema realiza un trabajo neto de $80$ kJ sobre el ambiente. Se puede asegurar que:",
             {"A": "El proceso es imposible", "B": "La energía interna del sistema aumenta",
              "C": "La temperatura del sistema aumenta", "D": "La temperatura del sistema disminuye"}, "D",
             claves=["primera ley", "signos", "energía interna"])
    L.paso("Aplicar la primera ley con sus signos")\
        .di("Calor que entra es positivo; trabajo hecho por el sistema sobre el ambiente es positivo.", DATO("60 kJ"),
            LISTA(["$Q = +60$ kJ", "$W = +80$ kJ"]),
            F(r"\Delta U = Q - W"))\
        .di("Sesenta menos ochenta da menos veinte kilojoules: el sistema gastó de su energía interna y se enfría.",
            M(r"\Delta U = 60\ \text{kJ} - 80\ \text{kJ} = -20\ \text{kJ} < 0"),
            R(r"\Delta U < 0 \;\Rightarrow\; T\ \text{disminuye}"))
    revisar_opciones(L, {
        "A": ("La A no: no es un ciclo; en un proceso abierto sí puedes hacer más trabajo que calor gastando energía interna.", "Es totalmente posible."),
        "B": ("La B tiene el signo al revés: la energía interna disminuye veinte kilojoules.", "$\\Delta U = -20$ kJ."),
        "C": ("La C contradice el signo negativo de delta U.", "Al bajar $U$, baja $T$."),
        "D": ("La D es la correcta: la temperatura disminuye.", "$\\Delta U < 0$."),
    })
    return L.oficial("III Parcial extraordinario I-2024", "D").usa(r"\Delta U=Q-W")


# ============================================================================ Q088
@leccion("Q088")
def q088() -> Leccion:
    L = _sel("Q088", "Procesos cíclicos permitidos por primera y segunda ley",
             "En procesos cíclicos: I. Entran $5$ J de calor y salen $3$ J de trabajo más $1$ J de radiación. II. Entran $5$ J de calor y salen $3$ J de trabajo más $2$ J de calor. III. Entran $5$ J de calor y salen $5$ J de trabajo. ¿Cuáles son posibles?",
             {"A": "Solo I y II", "B": "Solo I", "C": "Solo II", "D": "Todas"}, "C",
             claves=["proceso cíclico", "primera y segunda ley"])
    L.paso("Revisar conservación y segunda ley en cada ciclo")\
        .di("En un ciclo cerrado delta U es cero: toda la energía que entra debe salir, y no se puede convertir todo el calor en trabajo.", DATO("procesos cíclicos"),
            LISTA(["**I. Imposible**: entran $5$ J pero solo salen $3+1=4$ J (viola la primera ley)",
                   "**II. Posible**: entran $5$ J y salen $3+2=5$ J, con calor cedido al foco frío",
                   "**III. Imposible**: convierte el $100\\,\\%$ del calor en trabajo (viola la segunda ley)"]))\
        .di("Solo la afirmación dos respeta ambas leyes.", R(r"\text{Solo II}"))
    revisar_opciones(L, {
        "A": ("La A incluye la uno, que pierde un joule de la nada.", "I viola la 1.ª ley."),
        "B": ("La B elige justo la que viola conservación de energía.", "$5 \\neq 4$."),
        "D": ("La D acepta violar tanto la primera como la segunda ley.", "I y III son imposibles."),
        "C": ("La C es la correcta: solo la dos.", "$5=3+2$ y cede calor al foco frío."),
    })
    return L.oficial("III Parcial extraordinario I-2024", "C").usa(r"\Delta U_{\text{ciclo}}=0", r"Q_H=W+Q_C")


# ============================================================================ Q089
@leccion("Q089")
def q089() -> Leccion:
    L = _sel("Q089", "Dos trayectorias entre A y B en un diagrama p-V",
             "Dos procesos 1 y 2 inician en A y terminan en B (en expansión); la curva 1 va por encima de la curva 2 en el diagrama $p$-$V$. ¿Cuál relación entre $\\Delta U$ y $Q$ es correcta?",
             {"A": "ΔU₁ > ΔU₂, Q₁ > Q₂", "B": "ΔU₁ < ΔU₂, Q₁ > Q₂", "C": "ΔU₁ = ΔU₂, Q₁ = Q₂", "D": "ΔU₁ = ΔU₂, Q₁ > Q₂"}, "D",
             claves=["trayectorias", "función de estado", "área bajo la curva"])
    L.paso("Comparar delta U y Q entre las dos curvas")\
        .di("Como ambos procesos arrancan en A y llegan al mismo punto B, el cambio de energía interna es idéntico.", DATO("inician en A y acaban en B"),
            M(r"\Delta U_1 = U_B - U_A = \Delta U_2"))\
        .di("La curva uno pasa por presiones más altas, así que encierra mayor área bajo la curva: hace más trabajo.",
            M(r"W_1 > W_2"))\
        .di("Con la primera ley, si delta U es igual y doble ve uno es mayor, entonces cu uno es mayor.",
            F(r"Q = \Delta U + W \;\Rightarrow\; Q_1 > Q_2"),
            R(r"\Delta U_1 = \Delta U_2 \quad\text{y}\quad Q_1 > Q_2"))
    revisar_opciones(L, {
        "A": ("La A falla: la energía interna no depende de la trayectoria.", "$\\Delta U_1 = \\Delta U_2$."),
        "B": ("La B también supone erróneamente distinto delta U.", "Mismos extremos $\\Rightarrow$ igual $\\Delta U$."),
        "C": ("La C olvida que las áreas bajo las dos curvas son distintas.", "$W_1 > W_2 \\Rightarrow Q_1 > Q_2$."),
        "D": ("La D es la correcta: igual delta U y mayor calor en la curva superior.", "Exacto."),
    })
    return L.oficial("III Parcial ordinario I-2024", "D").usa(r"\Delta U=Q-W")


# ============================================================================ Q090
@leccion("Q090")
def q090() -> Leccion:
    L = _sel("Q090", "Energía interna en una expansión isobárica",
             "En una expansión isobárica de un gas ideal, el cambio de energía interna se puede calcular mediante:",
             {"A": "ΔU = nC_V ΔT", "B": "ΔU = nC_p ΔT", "C": "ΔU = |Q| + |W|", "D": "ΔU = nRT ln(V₂/V₁)"}, "A",
             claves=["energía interna", "isobárica", "C_V"])
    L.paso("La regla de oro para delta U en gases ideales")\
        .di("En un gas ideal, la energía interna solo depende de la temperatura, así que siempre lleva ce ve.", DATO("expansión isobárica"),
            F(r"\Delta U = n C_V\,\Delta T"),
            OJO("Incluso a presión constante: $Q = nC_p\\Delta T$, pero $\\Delta U = nC_V\\Delta T$ **siempre**."))
    revisar_opciones(L, {
        "B": ("La B es el calor isobárico, no el cambio de energía interna.", "$nC_p\\Delta T = Q_{\\text{isob}}$."),
        "C": ("La C suma en vez de restar: la primera ley es cu menos doble ve.", "$\\Delta U = Q - W$."),
        "D": ("La D es el trabajo de un proceso isotérmico.", "Eso es $W_{\\text{isot}}$."),
        "A": ("La A es la correcta: siempre ene ce ve delta te.", "Válida para cualquier proceso de gas ideal."),
    })
    return L.oficial("III Parcial extraordinario II-2024", "A").usa(r"\Delta U=nC_V\Delta T")


# ============================================================================ Q091
@leccion("Q091")
def q091() -> Leccion:
    L = _sel("Q091", "Enfriamiento a volumen constante: signos de ΔS, ΔU, W y Q",
             "En un proceso donde un gas en un recipiente de volumen constante experimenta una disminución en su temperatura, se cumple que:",
             {"A": "ΔS ≠ 0, ΔU ≠ 0, W = 0, Q ≠ 0", "B": "ΔS = 0, ΔU ≠ 0, W = 0, Q ≠ 0",
              "C": "ΔS ≠ 0, ΔU = 0, W = 0, Q ≠ 0", "D": "ΔS ≠ 0, ΔU ≠ 0, W ≠ 0, Q ≠ 0"}, "A",
             aviso="En el Superexamen se pegó por error una figura de otra pregunta; este ejercicio es puramente conceptual sobre un enfriamiento isocórico.",
             claves=["isocórico", "primera ley", "entropía"])
    L.paso("Revisar las cuatro variables a volumen constante")\
        .di("Si el volumen no cambia, el gas no se expande ni se comprime: el trabajo es exactamente cero.", DATO("volumen constante"),
            LISTA(["$V=\\text{cte} \\Rightarrow W = 0$",
                   "$\\Delta T < 0 \\Rightarrow \\Delta U = nC_V\\Delta T < 0 \\;(\\neq 0)$",
                   "$Q = \\Delta U + W = \\Delta U < 0 \\;(\\neq 0)$",
                   "$\\Delta S = nC_V\\ln(T_f/T_i) < 0 \\;(\\neq 0)$"]))\
        .di("Solo el trabajo es cero; las otras tres cantidades cambian.",
            R(r"\Delta S \neq 0,\;\Delta U \neq 0,\;W = 0,\;Q \neq 0"))
    revisar_opciones(L, {
        "B": ("La B falla: al enfriarse y ceder calor, la entropía del gas disminuye, no es cero.", "$\\Delta S < 0$."),
        "C": ("La C falla: si baja la temperatura, la energía interna disminuye.", "$\\Delta U < 0$."),
        "D": ("La D falla: a volumen constante el trabajo siempre es cero.", "$W = 0$."),
        "A": ("La A es la correcta: solo el trabajo es cero.", "Exacto para un proceso isocórico."),
    })
    return L.oficial("III Parcial ordinario II-2024", "A").usa(r"\Delta U=Q-W", r"\Delta U=nC_V\Delta T")
