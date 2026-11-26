"""Superexamen Q071–Q082: gases ideales, Van der Waals, teoría cinética, equipartición y C_V."""

from __future__ import annotations

import math

from .ayudas import R_GAS, T0, revisar_opciones
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .formato import dice, n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion


def _sel(q: str, titulo: str, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), tipo="seleccion", fuente=fuente(q), enunciado=enunciado,
                   opciones=opciones, correcta=correcta, **k)


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado, **k)


# ============================================================================ Q071
@leccion("Q071")
def q071() -> Leccion:
    L = _sel("Q071", "Propósito de la ecuación de Van der Waals",
             "¿Cuál es el propósito principal de la ecuación de Van der Waals en relación con el modelo molecular de los gases?",
             {"A": "Describir la relación entre la presión y la temperatura de un gas.",
              "B": "Corregir las omisiones en la ecuación del gas ideal, considerando los volúmenes moleculares y las fuerzas de atracción entre moléculas.",
              "C": "Simplificar el cálculo de la densidad de un gas en condiciones de alta presión.",
              "D": "Sustituir a la ecuación del gas ideal en todas las situaciones físicas posibles."}, "B",
             claves=["Van der Waals", "gas real"],
             visual={"tipo": "gas", "modo": "temperatura", "pie": "Van der Waals corrige volumen propio y atracciones"})
    L.paso("Qué corrige Van der Waals")\
        .di("El gas ideal supone moléculas puntuales sin fuerzas entre ellas.", DATO("Van der Waals"),
            F(r"\left(p + \frac{a n^2}{V^2}\right)(V - nb) = nRT"))\
        .di("Van der Waals agrega dos correcciones: el volumen propio be y la atracción intermolecular a.",
            LISTA(["$V - nb$: descuenta el volumen ocupado por las moléculas",
                   "$p + a(n/V)^2$: corrige la caída de presión por atracciones entre moléculas"]))
    revisar_opciones(L, {
        "A": ("La A es demasiado vaga; eso ya lo hace el gas ideal.", "No menciona las correcciones reales."),
        "C": ("La C no: la ecuación de Van der Waals es más compleja, no más simple.", "Es cúbica en $V$."),
        "D": ("La D no: a bajas presiones el gas ideal sigue siendo excelente, y Van der Waals también es aproximada.", "No aplica universalmente."),
        "B": ("La B es la correcta: incluye volumen molecular y fuerzas de atracción.", "Exacto: parámetros $b$ y $a$."),
    })
    return L.oficial("III Parcial ordinario I-2024", "B").usa(r"\left(p+\frac{an^2}{V^2}\right)(V-nb)=nRT")


# ============================================================================ Q072
@leccion("Q072")
def q072() -> Leccion:
    L = _sel("Q072", "Moléculas de un gas de Van der Waals cerca de la pared",
             "En un gas que sigue la ecuación de Van der Waals, ¿qué ocurre con las moléculas cuando se acercan a las paredes del contenedor?",
             {"A": "Se aceleran debido a las fuerzas atractivas entre las moléculas.",
              "B": "Ejercen mayor presión que un gas ideal debido a la repulsión entre moléculas.",
              "C": "Su movimiento se ve afectado por fuerzas atractivas que las devuelven hacia el interior del gas.",
              "D": "Ejercen la misma presión sobre las paredes que un gas ideal."}, "C",
             claves=["Van der Waals", "fuerzas atractivas", "paredes"],
             visual={"tipo": "gas", "modo": "temperatura", "pie": "Las moléculas del interior halan hacia atrás a las que van a chocar"})
    L.paso("Efecto de las fuerzas atractivas junto a la pared")\
        .di("En el interior las atracciones se compensan en todas direcciones.", V(foco="presion"), DATO("acercan a las paredes"),
            LISTA(["Junto a la pared solo hay moléculas **detrás** de ella",
                   "Eso produce una fuerza neta hacia el **interior** que la frena",
                   "Al chocar con menos ímpetu, la presión medida $p$ es menor que la ideal"]))\
        .di("Por eso se suma el término a ene cuadrado entre ve cuadrado para recuperar la presión ideal.",
            F(r"p_{\text{ideal}} = p + \frac{a n^2}{V^2}"))
    revisar_opciones(L, {
        "A": ("La A no: las atracciones hacia adentro frenan a la molécula, no la aceleran.", "La desaceleran."),
        "B": ("La B no: las atracciones reducen la presión sobre la pared.", "La presión real baja."),
        "D": ("La D ignora las interacciones moleculares.", "Solo sería igual en gas ideal."),
        "C": ("La C es la correcta: las fuerzas atractivas las halan hacia el interior.", "Reduce la presión en la pared."),
    })
    return L.oficial("III Parcial ordinario II-2024", "C").usa(r"\left(p+\frac{an^2}{V^2}\right)(V-nb)=nRT")


# ============================================================================ Q073
@leccion("Q073")
def q073() -> Leccion:
    L = _sel("Q073", "Significado físico de a y b en Van der Waals",
             "En la ecuación de Van der Waals, ¿cuál afirmación describe correctamente el significado físico de las constantes $a$ y $b$?",
             {"A": "a y b son constantes universales idénticas para todos los gases; a ajusta el volumen y b la presión.",
              "B": "a corrige el aumento de presión provocado por la interacción entre moléculas y b ajusta el volumen disponible restando el espacio molecular.",
              "C": "a reduce el volumen efectivo al descontar el espacio de las moléculas, y b corrige la presión por atracción intermolecular.",
              "D": "a corrige la reducción de presión debida a las fuerzas de atracción intermoleculares, y b representa el volumen ocupado por un mol de moléculas."}, "D",
             claves=["constantes a y b", "Van der Waals"])
    L.paso("Cada constante en su lugar")\
        .di("Cada gas tiene sus propios valores de a y be.", DATO("constantes a y b"),
            F(r"\left(p + \frac{a n^2}{V^2}\right)(V - nb) = nRT"))\
        .di("A acompaña a la presión y compensa su reducción por atracción; be acompaña al volumen y es el volumen excluido por mol.",
            LISTA(["$a$: corrige la **reducción** de presión por fuerzas atractivas",
                   "$b$: volumen excluido por un mol de moléculas",
                   "Ambas dependen de cada gas específico"]))
    revisar_opciones(L, {
        "A": ("La A falla en todo: no son universales y tiene a y be al revés.", "Dependen del gas."),
        "B": ("La B falla en el signo: las atracciones reducen la presión, no la aumentan.", "Es reducción de presión."),
        "C": ("La C invierte los papeles de a y be.", "$a$ va con $p$, $b$ va con $V$."),
        "D": ("La D es la correcta.", "Describe bien tanto $a$ como $b$."),
    })
    return L.oficial("III Parcial ordinario I-2025", "D").usa(r"\left(p+\frac{an^2}{V^2}\right)(V-nb)=nRT")


# ============================================================================ Q074
@leccion("Q074")
def q074() -> Leccion:
    V_m3, n_mol, T_k = 0.0300, 2.00, 80.0 + T0
    p = n_mol * R_GAS * T_k / V_m3
    L = _sel("Q074", "Presión de 2,00 mol en 30,0 L a 80,0 °C",
             "Un recipiente de $30{,}0$ L contiene $2{,}00$ mol de un gas ideal a $80{,}0\\ ^\\circ$C. La presión en el recipiente es:",
             {"A": "195,7 Pa", "B": "296,5 Pa", "C": "19,57 kPa", "D": "195,7 kPa"}, "D",
             claves=["gas ideal", "pV=nRT"],
             visual={"tipo": "gas", "modo": "temperatura"})
    L.paso("Convertir a SI y despejar p")\
        .di("Pasamos litros a metros cúbicos y Celsius a kelvin.", DATO("30,0 L"),
            LISTA(["$V = 30{,}0\\ \\text{L} = 0{,}0300\\ \\text{m}^3$", "$T = 80{,}0 + 273{,}15 = 353{,}15$ K", "$n = 2{,}00$ mol"]))\
        .di("Despejamos la presión del gas ideal de la hoja.", V(foco="presion"), F(r"pV=nRT\;\Rightarrow\;p = \frac{n R T}{V}"))\
        .di("Sustituimos.", M(rf"p = \frac{{(2{{,}}00)(8{{,}}314)(353{{,}}15)}}{{0{{,}}0300}}"))\
        .di("Multiplicamos arriba y dividimos.", M(rf"p = \frac{{{tex(n_mol * R_GAS * T_k, 5)}}}{{0{{,}}0300}} = {tex(p, 4, u='Pa', sci='si')}"))\
        .di(f"En kilopascales se divide entre mil: {nu(p / 1e3, 'kPa', 4)}.", R(rf"p = {tex(p / 1e3, 4, u='kPa')}"))
    revisar_opciones(L, {
        "A": ("La A olvidó convertir litros a metros cúbicos: falta un factor mil.", "Usó $30$ en vez de $0{,}030$."),
        "B": ("La B ni siquiera coincide en cifras.", "Cálculo erróneo."),
        "C": ("La C está corrida por un factor diez.", "Error de coma decimal."),
        "D": ("La D es la correcta: ciento noventa y cinco coma siete kilopascales.", "$195{,}7$ kPa."),
    })
    return L.oficial("III Parcial ordinario I-2026", "D").usa(r"pV=nRT")


# ============================================================================ Q075
@leccion("Q075")
def q075() -> Leccion:
    L = _sel("Q075", "Supuestos de la teoría cinética molecular",
             "Dentro de los supuestos de la teoría cinética molecular se encuentra:",
             {"A": "Un recipiente con volumen V contiene un número muy grande N de moléculas de masas diferentes.",
              "B": "Las moléculas están en constante movimiento y obedecen las leyes del movimiento de Newton.",
              "C": "Las moléculas chocan ocasionalmente con las paredes y tales choques son perfectamente inelásticos.",
              "D": "Las paredes del recipiente no son perfectamente rígidas y tienen masa infinita; se deforman."}, "B",
             claves=["teoría cinética", "supuestos"],
             visual={"tipo": "gas", "modo": "temperatura"})
    L.paso("Revisar los postulados del modelo")\
        .di("En un gas ideal puro todas las moléculas tienen la misma masa y chocan elásticamente contra paredes rígidas.", DATO("teoría cinética molecular"),
            LISTA(["Gran número $N$ de moléculas **idénticas** (misma masa)",
                   "Movimiento aleatorio continuo que obedece las **leyes de Newton**",
                   "Choques **perfectamente elásticos** con paredes rígidas"]))
    revisar_opciones(L, {
        "A": ("La A falla: para un gas puro se asumen moléculas idénticas de igual masa.", "Masas iguales, no diferentes."),
        "C": ("La C falla: los choques son perfectamente elásticos, no inelásticos.", "Se conserva energía cinética."),
        "D": ("La D falla: las paredes se suponen perfectamente rígidas.", "No se deforman."),
        "B": ("La B es la correcta: movimiento constante bajo las leyes de Newton.", "Postulado oficial del modelo."),
    })
    return L.oficial("II Parcial extraordinario II-2023", "B")


# ============================================================================ Q076
@leccion("Q076")
def q076() -> Leccion:
    m_he, m_o2 = 4.0026, 31.9988
    razon_v = math.sqrt(m_he / m_o2)
    L = _des("Q076", "Comparación entre helio (monoatómico) y oxígeno (diatómico)",
             "Helio (monoatómico, $M=4{,}0026$ g/mol) y oxígeno (diatómico, $M=31{,}9988$ g/mol) como gases ideales. A) En equilibrio térmico, $v_{\\text{rms}}(\\text{O}_2)$ es ___ que $v_{\\text{rms}}(\\text{He})$. B) En cilindro sellado de He al bajar $p$, $K_{\\text{tr}}$ ___. C) Con igual $K_{\\text{tr}}$ y misma fuga, la presión baja más rápido en ___. D) Con igual calor y $1{,}00$ mol, aumenta más $T$ el ___.",
             claves=["rapidez rms", "helio y oxígeno", "grados de libertad"],
             visual={"tipo": "gas", "modo": "temperatura", "pie": "A igual T, el gas más liviano (He) es más veloz"})
    L.paso("A) Rapidez eficaz en equilibrio térmico")\
        .di("En equilibrio térmico ambos gases tienen la misma temperatura.", DATO("equilibrio térmico"),
            F(r"v_{\text{rms}} = \sqrt{\frac{3RT}{M}}"))\
        .di("Dividimos la de oxígeno entre la de helio: tres, erre y te se cancelan, y la masa molar del helio queda arriba.".replace("erre y te", dice("R y T", "erre y te")),
            M(r"\frac{v_{\text{rms,O}_2}}{v_{\text{rms,He}}}=\frac{\sqrt{3RT/M_{\text{O}_2}}}{\sqrt{3RT/M_{\text{He}}}}=\sqrt{\frac{M_{\text{He}}}{M_{\text{O}_2}}}"))\
        .di(f"El oxígeno pesa ocho veces más que el helio, así que su rapidez eficaz es menor: un {n(razon_v * 100, 3)} por ciento de la del helio.",
            M(rf"\frac{{v_{{\text{{rms,O}}_2}}}}{{v_{{\text{{rms,He}}}}}} = \sqrt{{\frac{{4{{,}}0026}}{{31{{,}}9988}}}} = \sqrt{{{tex(4.0026 / 31.9988, 4)}}} = {tex(razon_v, 3)} < 1"),
            R(r"v_{\text{rms}}(\text{O}_2)\ \text{es menor que}\ v_{\text{rms}}(\text{He})", "ra"))
    L.paso("B) Cilindro sellado al disminuir la presión")\
        .di("La energía cinética de traslación de la hoja; y del gas ideal, ene erre te es pe ve.".replace("ene erre te es pe ve", dice("nRT = pV", "ene erre te es pe ve")),
            F(r"K_{\text{tr}} = \frac{3}{2}nRT"), F(r"pV=nRT"), M(r"K_{\text{tr}} = \frac{3}{2}nRT = \frac{3}{2}pV"))\
        .di("Con volumen y moles fijos, si baja la presión, baja la temperatura y baja la energía cinética traslacional.",
            M(r"V\ \text{fijo},\ p\downarrow\;\Rightarrow\;K_{\text{tr}}=\tfrac{3}{2}pV\downarrow"),
            R(r"K_{\text{tr}}\ \text{disminuye}", "rb"))
    L.paso("C y D) Fuga más rápida y mayor aumento de temperatura")\
        .di("Con igual energía cinética media tienen igual temperatura. La rapidez eficaz de la hoja depende de la masa molar.",
            F(r"v_{\text{rms}}=\sqrt{\frac{3RT}{M}}"))\
        .di("Misma temperatura y el helio tiene masa molar mucho menor: su rapidez es mayor y escapa antes por el agujero.",
            M(r"M_{\text{He}}<M_{\text{O}_2}\;\Rightarrow\;v_{\text{rms,He}}>v_{\text{rms,O}_2}"),
            R(r"\text{C) Baja más rápido en: helio}", "rc"))\
        .di("El helio es monoatómico y tiene menor capacidad calorífica que el oxígeno diatómico, así que con el mismo calor se calienta más.",
            M(r"\Delta T = \frac{Q}{nC_V}\qquad C_{V,\text{He}}=\frac{3}{2}R < C_{V,\text{O}_2}=\frac{5}{2}R"),
            R(r"\text{D) Aumenta más su temperatura: helio}", "rd"))
    L.responde("A", r"\text{menor que}", razon_v, "").responde("B", r"\text{disminuye}", None, "")\
     .responde("C", r"\text{helio}", None, "").responde("D", r"\text{helio}", None, "")
    return L.usa(r"v_{\text{rms}}=\sqrt{\frac{3RT}{M}}", r"K_{\text{tr}}=\frac{3}{2}nRT")


# ============================================================================ Q077
@leccion("Q077")
def q077() -> Leccion:
    L = _sel("Q077", "Por qué el CO₂ tiene mayor C_V que el helio",
             "Según el modelo cinético-molecular, ¿por qué el $\\text{CO}_2$ (poliatómico, $40$ g/mol) tiene mayor $C_V$ que el helio (monoatómico, $4$ g/mol) a la misma temperatura?",
             {"A": "Porque el CO₂ tiene mayor masa molar.",
              "B": "Porque el CO₂ tiene más grados de libertad para almacenar energía interna.",
              "C": "Porque el CO₂ ejerce mayor presión sobre las paredes del recipiente.",
              "D": "Porque las moléculas de CO₂ se mueven más rápidamente."}, "B",
             claves=["grados de libertad", "capacidad calorífica", "poliatómico"])
    L.paso("Equipartición y grados de libertad")\
        .di("Generalizando las dos casillas de la hoja: cada forma activa de moverse aporta medio erre.", DATO("poliatómico"),
            M(r"C_V = \frac{f}{2}\,R\qquad (f:\ \text{formas de moverse})"))\
        .di("El helio solo traslada energía; el dióxido de carbono además rota y vibra, así que absorbe más energía por grado.",
            LISTA(["He monoatómico: solo traslación ($f=3$, $C_V=\\tfrac{3}{2}R$)",
                   "$\\text{CO}_2$ poliatómico: traslación + rotación + vibración ($f>3$)"]),
            R(r"C_{V,\text{CO}_2} > C_{V,\text{He}}"))
    revisar_opciones(L, {
        "A": ("La A no: la masa molar no aparece en ce ve molar.", "$C_V$ depende de $f$, no de $M$."),
        "C": ("La C no: a igual número de moles, volumen y temperatura ejercen la misma presión.", "$pV=nRT$ no depende del tipo de gas."),
        "D": ("La D es falsa: al ser más pesadas, las moléculas de CO₂ van más lento.", "$v_{\\text{rms}}\\propto 1/\\sqrt{M}$."),
        "B": ("La B es la correcta: tiene más grados de libertad.", "Almacena energía también en rotación y vibración."),
    })
    return L.oficial("III Parcial extraordinario I-2025", "B").usa(r"C_V=\frac{f}{2}R")


# ============================================================================ Q078
@leccion("Q078")
def q078() -> Leccion:
    L = _sel("Q078", "De qué depende la presión microscópica de un gas ideal",
             "Según la Teoría Cinética de los Gases, la presión de un gas monoatómico en un recipiente es directamente proporcional a:",
             {"A": "La masa molar de las moléculas del gas.", "B": "La energía cinética media de las moléculas del gas.",
              "C": "La rapidez cuadrática media de las moléculas del gas.", "D": "Todas las anteriores son correctas."}, "B",
             claves=["presión microscópica", "energía cinética media"],
             visual={"tipo": "gas", "modo": "temperatura"})
    L.paso("Presión y energía cinética, desde la hoja")\
        .di("Partimos de dos casillas de la hoja: el gas ideal y la energía cinética total.", V(foco="presion"), DATO("directamente proporcional"),
            F(r"pV=nRT"), F(r"K=\frac{3}{2}nRT"))\
        .di("De la segunda despejamos ene erre te: el tres medios pasa al otro lado como dos tercios.".replace("ene erre te", dice("nRT", "ene erre te")),
            M(r"nRT=\frac{2}{3}K"))\
        .di("Lo ponemos en el gas ideal en lugar de ene erre te.".replace("ene erre te", dice("nRT", "ene erre te")), M(r"pV=\frac{2}{3}K"))\
        .di("La energía total es el número de moléculas por la energía media de cada una.", M(r"K=N\,K_{\text{med}}\;\Rightarrow\;pV=\frac{2}{3}N\,K_{\text{med}}"))\
        .di("Dividimos entre el volumen: la presión es directamente proporcional a la energía cinética media.", M(r"p=\frac{2}{3}\,\frac{N}{V}\,K_{\text{med}}"))
    L.paso("¿Y la rapidez eficaz?")\
        .di("La hoja da la energía media de una molécula y la rapidez eficaz.", F(r"\frac{1}{2}m(v^2)_{\text{med}}=\frac{3k_B T}{2}"), F(r"v_{\text{rms}}=\sqrt{\frac{3k_BT}{m}}"))\
        .di("Despejando las dos, el promedio de la rapidez al cuadrado es la rapidez eficaz al cuadrado.",
            M(r"(v^2)_{\text{med}}=\frac{3k_BT}{m}=v_{\text{rms}}^2\;\Rightarrow\;K_{\text{med}}=\frac{1}{2}m\,v_{\text{rms}}^2"))\
        .di("Lo metemos en la presión: aparece la rapidez al cuadrado, no la rapidez sola.",
            M(r"p=\frac{2}{3}\,\frac{N}{V}\left(\frac{1}{2}m\,v_{\text{rms}}^2\right)=\frac{1}{3}\,\frac{N m}{V}\,v_{\text{rms}}^2"))\
        .di("Por eso la presión va con la energía media, y con el cuadrado de la rapidez eficaz.",
            M(r"p \propto K_{\text{med}}\qquad p \propto v_{\text{rms}}^2"))
    revisar_opciones(L, {
        "A": ("La A no: a igual temperatura, la presión no depende de la masa molar.", "$pV=nRT$."),
        "C": ("La C es la trampa: es proporcional al cuadrado de la rapidez eficaz, no a ella sola.", "$p\\propto v_{\\text{rms}}^2$, no $v_{\\text{rms}}$."),
        "D": ("La D falla porque A y C son falsas.", "Solo B es proporcionalidad directa."),
        "B": ("La B es la correcta: directamente proporcional a la energía cinética media.", "$p = \\frac{2N}{3V}K_{\\text{med}}$."),
    })
    return L.oficial("III Parcial extraordinario II-2025", "B").usa(r"pV=nRT", r"K=\frac{3}{2}nRT")


# ============================================================================ Q079
@leccion("Q079")
def q079() -> Leccion:
    L = _sel("Q079", "Teorema de equipartición de la energía",
             "Sobre la equipartición de la energía: I. Cada componente de velocidad tiene en promedio $\\tfrac{1}{2}k_BT$ por molécula. II. Cada grado de libertad posible contribuye con $\\tfrac{1}{2}k_BT$. III. En un gas ideal, los grados traslacionales y rotacionales contribuyen significativamente a la energía interna. Son verdaderas:",
             {"A": "Solo I y II", "B": "Todas", "C": "Solo II", "D": "Solo I"}, "A",
             claves=["equipartición", "grados de libertad"])
    L.paso("Analizar cada enunciado")\
        .di("Uno y dos expresan el principio de equipartición: un medio de ka be te por componente o grado de libertad.", DATO("equipartición"),
            F(r"\langle E_{\text{grado}}\rangle = \frac{1}{2}k_B T"),
            LISTA(["**I. Verdadera**: cada componente $v_x, v_y, v_z$ aporta $\\tfrac{1}{2}k_B T$ por molécula",
                   "**II. Verdadera**: enunciado estándar de equipartición (por molécula)",
                   "**III. Falsa**: en un gas ideal **monoatómico** no hay energía rotacional significativa"]))\
        .di("Por tanto, solo uno y dos son verdaderas.", R(r"\text{Solo I y II}"))
    revisar_opciones(L, {
        "B": ("La B incluye la tres, pero un gas ideal monoatómico no rota.", "III falla para monoatómicos."),
        "C": ("La C deja fuera la uno, que es cierta.", "I también es verdadera."),
        "D": ("La D deja fuera la dos.", "I y II van juntas en el teorema."),
        "A": ("La A es la correcta: solo uno y dos.", "I y II verdaderas; III no vale para gases monoatómicos."),
    })
    return L.oficial("III Parcial extraordinario I-2024", "A").usa(r"K_{\text{med}}=\frac{3}{2}k_BT")


# ============================================================================ Q080
@leccion("Q080")
def q080() -> Leccion:
    L = _sel("Q080", "Dos gases a igual temperatura con distinta masa molar",
             "Dos gases ideales A y B están a la misma temperatura absoluta (igual número de moles), con $M_A > M_B$. ¿Cuál afirmación es correcta?",
             {"A": "La energía cinética media es menor en A que en B, y la velocidad eficaz es mayor en B.",
              "B": "La energía cinética media es mayor en A que en B, y la velocidad eficaz es menor en A.",
              "C": "La energía cinética media y la velocidad eficaz son iguales en A y B.",
              "D": "La energía cinética media es igual en A y B, y la velocidad eficaz es mayor en B."}, "D",
             claves=["energía cinética media", "rapidez eficaz", "masa molar"],
             visual={"tipo": "gas", "modo": "temperatura"})
    L.paso("Comparar energía cinética y rapidez eficaz")\
        .di("La energía cinética media traslacional solo depende de la temperatura: como es la misma, son iguales.", DATO("misma temperatura absoluta"),
            F(r"K_{\text{med}} = \frac{3}{2}k_B T \;\Rightarrow\; K_{\text{med},A} = K_{\text{med},B}"))\
        .di("La rapidez eficaz divide por la raíz de la masa molar: el gas más liviano se mueve más rápido.",
            F(r"v_{\text{rms}} = \sqrt{\frac{3RT}{M}}"),
            M(r"M_A > M_B \;\Rightarrow\; v_{\text{rms},B} > v_{\text{rms},A}"))
    revisar_opciones(L, {
        "A": ("La A falla en la energía cinética: a igual temperatura son iguales.", "$K_{\\text{med}}$ no depende de $M$."),
        "B": ("La B también falla en la energía cinética.", "Igual $T \\Rightarrow$ igual $K_{\\text{med}}$."),
        "C": ("La C falla en la rapidez: moléculas más livianas van más rápido.", "$v_{\\text{rms},B} > v_{\\text{rms},A}$."),
        "D": ("La D es la correcta: igual energía cinética media y mayor rapidez en B.", "Más liviano = más rápido."),
    })
    return L.oficial("III Parcial ordinario I-2025", "D").usa(r"K_{\text{med}}=\frac{3}{2}k_BT", r"v_{\text{rms}}=\sqrt{\frac{3RT}{M}}")


# ============================================================================ Q081
@leccion("Q081")
def q081() -> Leccion:
    L = _sel("Q081", "Cómo duplicar la energía cinética media de un gas ideal",
             "La energía cinética media de las moléculas de un gas ideal se multiplica por dos al duplicarse:",
             {"A": "la presión absoluta.", "B": "la temperatura absoluta.", "C": "el volumen del recipiente.", "D": "la masa de las moléculas de gas."}, "B",
             claves=["energía cinética media", "temperatura absoluta"],
             visual={"tipo": "gas", "modo": "temperatura"})
    L.paso("Solo depende de la temperatura absoluta")\
        .di("La energía cinética media es directamente proporcional a la temperatura en kelvin.", DATO("multiplica por dos"),
            F(r"K_{\text{med}} = \frac{3}{2}k_B T"))\
        .di("Si se duplica la temperatura absoluta, la energía cinética media se duplica.",
            R(r"T' = 2T \;\Rightarrow\; K'_{\text{med}} = 2K_{\text{med}}"))
    revisar_opciones(L, {
        "A": ("La A no siempre: se puede duplicar la presión isotérmicamente bajando el volumen.", "No garantiza duplicar $T$."),
        "C": ("La C tampoco: el volumen puede cambiar a temperatura constante.", "No fija $T$."),
        "D": ("La D no: la masa no aparece en tres medios de ka be te.", "Independiente de $m$."),
        "B": ("La B es la correcta: al duplicar la temperatura absoluta.", "$K_{\\text{med}} \\propto T$."),
    })
    return L.oficial("III Parcial ordinario II-2025", "B").usa(r"K_{\text{med}}=\frac{3}{2}k_BT")


# ============================================================================ Q082
@leccion("Q082")
def q082() -> Leccion:
    L = _sel("Q082", "Por qué aumenta C_V de un gas diatómico entre 100 K y 1000 K",
             "Al calentar un gas diatómico a volumen constante desde $100$ K hasta $1000$ K, su $C_V$ aumenta. ¿Qué explica cualitativamente este aumento?",
             {"A": "Activación de modos vibracionales que demandan energía adicional.",
              "B": "Aumento de la masa molar efectiva del gas al elevar la temperatura.",
              "C": "Cambio en el número de grados de libertad traslacionales.",
              "D": "Disociación parcial de las moléculas diatómicas."}, "A",
             claves=["vibración", "C_V", "gas diatómico"])
    L.paso("Escalones cuánticos de C_V con la temperatura")\
        .di("A temperaturas bajas solo hay traslación; al subir la temperatura se despiertan primero la rotación y luego la vibración.", DATO("100 K hasta 1000 K"),
            TABLA(["Rango típico", "Modos activos", "$C_V$"],
                  [["Baja ($<100$ K)", "Traslación (3)", "$\\tfrac{3}{2}R$"],
                   ["Ambiente ($\\sim 300$ K)", "+ Rotación (5)", "$\\tfrac{5}{2}R$"],
                   ["Alta ($\\sim 1000$ K)", "+ Vibración (7)", "$\\tfrac{7}{2}R$"]], resaltar=[[2, 1], [2, 2]]))\
        .di("Al activarse los modos vibracionales, hace falta más calor por cada kelvin de subida.",
            R(r"\frac{3}{2}R \;\to\; \frac{5}{2}R \;\to\; \frac{7}{2}R"))
    revisar_opciones(L, {
        "B": ("La B es falsa: calentar no cambia la masa de las moléculas.", "La masa molar es constante."),
        "C": ("La C es falsa: las direcciones espaciales siempre son tres.", "Siempre hay 3 modos traslacionales."),
        "D": ("La D no explica el escalón cuántico clásico entre cien y mil kelvin.", "Ocurre por vibración."),
        "A": ("La A es la correcta: activación de los modos vibracionales.", "Aporta energía cinética y potencial vibracional."),
    })
    return L.oficial("III Parcial ordinario I-2025", "A").usa(r"C_V=\frac{f}{2}R")
