"""Preguntas conceptuales razonadas desde la hoja, paso a paso.

Las preguntas de selección conceptual (Parte I del examen) se resolvían con una o dos frases. Aquí,
a cada una se le agrega un paso "Desde la hoja, paso a paso" que llega a la opción correcta usando
solo casillas del formulario II-2026 (o despejes de ellas) y conocimiento básico (densidad = m/V).
El paso se inserta antes de "Revisar las opciones". Se aplica al construir la lección (registro).
"""

from __future__ import annotations

from typing import Callable, Dict, List, Tuple

from .dsl import F, M, NOTA, Leccion, Paso
from .formato import dice

Tramo_ = Tuple[str, list]


def _paso(L: Leccion, tramos: List[Tramo_]) -> None:
    """Primero la derivación desde la hoja; lo que la lección ya decía queda después, como resumen."""
    p = Paso("Desde la hoja, paso a paso")
    for voz, acc in tramos:
        p.di(voz, *acc)
    k = 1 if L.pasos and L.pasos[0].titulo.startswith("Qué nos dan") else 0
    fin = next((i for i, q in enumerate(L.pasos) if q.titulo.startswith("Revisar las opciones")), len(L.pasos))
    for q in L.pasos[k:fin]:
        if not q.titulo.startswith("En resumen"):
            q.titulo = f"En resumen: {q.titulo[0].lower()}{q.titulo[1:]}"
    L.pasos.insert(k, p)


def _q062(L: Leccion) -> None:
    _paso(L, [
        ("La hoja trae la dilatación de volumen: al calentar, el volumen crece.",
         [F(r"\Delta V=\beta V_0\Delta T"), M(r"\Delta T>0\;\Rightarrow\;\Delta V>0")]),
        ("La densidad es masa entre volumen, y la masa del agua no cambia al calentarla.",
         [M(r"\rho=\frac{m}{V}"), M(r"m\ \text{igual},\ V\ \text{mayor}\;\Rightarrow\;\rho\ \text{menor}")]),
        ("Lo menos denso flota sobre lo más denso, como el aceite sobre el agua. El agua caliente del fondo sube.",
         [M(r"\rho_{\text{caliente}}<\rho_{\text{fría}}\;\Rightarrow\;\text{sube por flotabilidad (B)}")]),
    ])


def _q065(L: Leccion) -> None:
    _paso(L, [
        ("La hoja dice cuánto radia un cuerpo: depende de la emisividad e.", [F(r"H=Ae\sigma T^4")]),
        ("La emisividad es un número entre cero y uno: cero no radia nada; uno es el cuerpo negro. La tres es verdadera.",
         [M(r"0\le e\le 1\;\Rightarrow\;\text{III verdadera}")]),
        ("Un cuerpo negro absorbe toda la radiación que le llega; por eso se ve negro. La uno dice lo contrario: es falsa.",
         [M(r"\text{cuerpo negro: }e=1,\ \text{absorbe todo}\;\Rightarrow\;\text{I falsa}")]),
        ("Un buen reflector absorbe poco: su emisividad es chica y, por la fórmula, radia poco. La dos es verdadera.",
         [M(r"e\ \text{pequeña}\;\Rightarrow\;H=Ae\sigma T^4\ \text{pequeña}\;\Rightarrow\;\text{II verdadera}")]),
    ])


def _q075(L: Leccion) -> None:
    _paso(L, [
        ("Las casillas de gases de la hoja salen de un modelo: moléculas iguales, siempre en movimiento, que chocan como bolas de billar.",
         [F(r"pV=nRT"), F(r"\frac{1}{2}m\left(v^2\right)_{\text{med}}=\frac{3k_BT}{2}")]),
        ("En esa fórmula hay una sola masa eme: todas las moléculas son iguales. La A es falsa.".replace("eme", dice("m", "eme")),
         [M(r"\text{una sola } m\;\Rightarrow\;\text{A falsa}")]),
        ("Los choques no pierden energía, son elásticos, y las paredes son rígidas. La C y la D son falsas.",
         [M(r"\text{choques elásticos, paredes rígidas}\;\Rightarrow\;\text{C y D falsas}")]),
        ("Lo que sí supone: se mueven sin parar y cumplen las leyes de Newton. Es la B.",
         [M(r"\text{movimiento constante con leyes de Newton}\;\Rightarrow\;\text{B}")]),
    ])


def _q077(L: Leccion) -> None:
    _paso(L, [
        ("La hoja trae dos capacidades: la del gas monoatómico y la del diatómico.",
         [F(r"C_{V\text{mono}}=\frac{3}{2}R"), F(r"C_{V\text{dia}}=\frac{5}{2}R")]),
        ("Cada forma de moverse aporta medio erre. El helio solo se traslada en tres direcciones: tres medios erre.".replace("medio erre", dice("½R", "medio erre")),
         [M(r"\text{mono: }3\cdot\tfrac{1}{2}R=\tfrac{3}{2}R")]),
        ("Una molécula de dos átomos además gira de dos maneras: cinco medios erre. El dióxido de carbono tiene más formas de moverse.",
         [M(r"\text{dia: }(3+2)\cdot\tfrac{1}{2}R=\tfrac{5}{2}R"),
          M(r"\text{más grados de libertad}\;\Rightarrow\;C_V\ \text{mayor}\;\Rightarrow\;\text{B}")]),
    ])


def _q079(L: Leccion) -> None:
    _paso(L, [
        ("La hoja da la energía cinética media de traslación de una molécula: tres medios por ka be por te.",
         [F(r"\frac{1}{2}m\left(v^2\right)_{\text{med}}=\frac{3k_BT}{2}")]),
        ("La velocidad tiene tres componentes, equis, ye y zeta, y en promedio se reparten igual. Cada una lleva medio ka be te: la uno es verdadera.",
         [M(r"\frac{3}{2}k_BT\div 3=\frac{1}{2}k_BT\ \text{por componente}\;\Rightarrow\;\text{I verdadera}")]),
        ("Lo mismo vale para cada forma de moverse, o grado de libertad: la dos es verdadera.",
         [M(r"\text{cada grado de libertad: }\tfrac{1}{2}k_BT\;\Rightarrow\;\text{II verdadera}")]),
        ("Pero un gas monoatómico no gira: su capacidad de la hoja es solo de traslación. La tres no siempre vale.",
         [F(r"C_{V\text{mono}}=\frac{3}{2}R"), M(r"\text{sin rotación}\;\Rightarrow\;\text{III falsa}\;\Rightarrow\;\text{A (solo I y II)}")]),
    ])


def _q081(L: Leccion) -> None:
    _paso(L, [
        ("La energía cinética media de la hoja depende solo de la temperatura absoluta.",
         [F(r"\frac{1}{2}m\left(v^2\right)_{\text{med}}=\frac{3k_BT}{2}")]),
        ("Ponemos dos te en lugar de te: el dos sale como factor.".replace("dos te", dice("2T", "dos te")).replace("de te:", "de " + dice("T", "te") + ":"),
         [M(r"K'_{\text{med}}=\frac{3k_B(2T)}{2}=2\cdot\frac{3k_BT}{2}=2K_{\text{med}}")]),
        ("No aparece la presión, ni el volumen, ni la masa: solo la temperatura. Es la B.",
         [M(r"K_{\text{med}}\ \text{depende solo de }T\;\Rightarrow\;\text{B}")]),
    ])


def _q082(L: Leccion) -> None:
    _paso(L, [
        ("La hoja da cinco medios erre para un diatómico: tres traslaciones y dos rotaciones.",
         [F(r"C_{V\text{dia}}=\frac{5}{2}R"), M(r"(3+2)\cdot\tfrac{1}{2}R=\tfrac{5}{2}R")]),
        ("A temperaturas altas los átomos empiezan a vibrar: son dos formas más de guardar energía.",
         [M(r"+2\cdot\tfrac{1}{2}R\;\Rightarrow\;C_V\to\tfrac{7}{2}R")]),
        ("Por eso la capacidad crece al calentar: se activan los modos de vibración. Es la A.",
         [M(r"\text{vibraciones activas}\;\Rightarrow\;C_V\ \text{aumenta}\;\Rightarrow\;\text{A}")]),
    ])


def _q083(L: Leccion) -> None:
    _paso(L, [
        ("La energía interna de un gas ideal depende solo del cambio de temperatura.", [F(r"\Delta U=nC_V\Delta T")]),
        ("Escribimos el salto de temperatura de cada proceso.",
         [M(r"\Delta U_A=nC_V(T_2-T_1)"), M(r"\Delta U_B=nC_V(T_3-T_1)"), M(r"\Delta U_C=nC_V(T_3-T_2)")]),
        ("El salto más grande es de te uno a te tres: es el proceso B.".replace("te uno a te tres", dice("T₁ a T₃", "te uno a te tres")),
         [M(r"T_3-T_1\ \text{es el mayor}\;\Rightarrow\;\text{B}")]),
    ])


def _q084(L: Leccion) -> None:
    _paso(L, [
        ("Si la presión casi no cambia, la hoja da el trabajo como presión por cambio de volumen.", [F(r"W=p\left(V_2-V_1\right)")]),
        ("Al comprimir, el volumen final es menor que el inicial: la resta da negativa. La presión siempre es positiva.",
         [M(r"V_2<V_1\;\Rightarrow\;V_2-V_1<0"), M(r"p>0\;\Rightarrow\;W=p\,(V_2-V_1)<0")]),
        ("El trabajo que hace el gas es negativo porque disminuye su volumen. Es la C.",
         [M(r"W<0\;\Rightarrow\;\text{C}")]),
    ])


def _q085(L: Leccion) -> None:
    _paso(L, [
        ("La energía interna solo depende de la temperatura: entre los mismos estados es la misma por cualquier camino. La D es falsa.",
         [F(r"\Delta U=nC_V\Delta T"), M(r"\Delta U\ \text{no depende del camino}\;\Rightarrow\;\text{D falsa}")]),
        ("El trabajo es el área bajo la curva: si cambia el camino, cambia el área. La B y la C son falsas.",
         [F(r"W=\int_{V_1}^{V_2}p\,dV"), M(r"W\ \text{depende del camino}\;\Rightarrow\;\text{B y C falsas}")]),
        ("Despejamos el calor de la primera ley: es delta U, fijo, más el trabajo, que depende del área. El calor depende del camino: A.".replace("delta U", dice("ΔU", "delta U")),
         [F(r"\Delta U=Q-W"), M(r"Q=\Delta U+W\;\Rightarrow\;\text{A}")]),
    ])


def _q086(L: Leccion) -> None:
    _paso(L, [
        ("Despejamos el calor de la primera ley de la hoja: delta U más el trabajo.".replace("delta U", dice("ΔU", "delta U")),
         [F(r"\Delta U=Q-W"), M(r"Q=\Delta U+W")]),
        ("Adiabático: por definición no entra calor.", [M(r"Q_{\text{adiab}}=0")]),
        ("Isotérmico: la temperatura no cambia, así que delta U es cero y el calor es el trabajo isotérmico.".replace("delta U", dice("ΔU", "delta U")),
         [F(r"W=nRT\ln\left(\frac{V_2}{V_1}\right)"), M(r"\Delta U=0\;\Rightarrow\;Q_{\text{isot}}=W_{\text{isot}}>0")]),
        ("Isobárico: la presión no baja, así que su área es mayor que la de la isoterma; y la temperatura sube, así que delta U es positivo.".replace("delta U", dice("ΔU", "delta U")),
         [F(r"W=p\left(V_2-V_1\right)"), M(r"W_{\text{isob}}>W_{\text{isot}}\qquad \Delta U_{\text{isob}}>0")]),
        ("Juntando: el isobárico absorbe más calor. Es la A.",
         [M(r"Q_{\text{isob}}=\Delta U_{\text{isob}}+W_{\text{isob}}>Q_{\text{isot}}>Q_{\text{adiab}}=0\;\Rightarrow\;\text{A}")]),
    ])


def _q088(L: Leccion) -> None:
    _paso(L, [
        ("En un ciclo la energía interna vuelve a su valor: delta U es cero, así que la energía que entra es la que sale.".replace("delta U", dice("ΔU", "delta U")),
         [F(r"\Delta U=Q-W"), M(r"\Delta U_{\text{ciclo}}=0\;\Rightarrow\;\text{entra}=\text{sale}")]),
        ("Uno: entran cinco joules y salen tres más uno, que son cuatro. No cuadra: viola la primera ley.",
         [M(r"\text{I: } 5\neq 3+1=4\;\Rightarrow\;\text{imposible}")]),
        ("Dos: cinco igual a tres más dos, y bota calor al foco frío. La eficiencia es menor que uno: es posible.",
         [M(r"\text{II: } 5=3+2\ \checkmark"), F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_c}{Q_H}\right|"), M(r"e=\frac{3}{5}=0{,}6<1\ \checkmark")]),
        ("Tres: todo el calor se vuelve trabajo, sin botar nada. La eficiencia sería uno: la segunda ley lo prohíbe. Solo la dos: C.",
         [M(r"\text{III: } e=\frac{5}{5}=1\;\Rightarrow\;|Q_c|=0\;\Rightarrow\;\text{imposible}\;\Rightarrow\;\text{C}")]),
    ])


def _q090(L: Leccion) -> None:
    _paso(L, [
        ("A presión constante, la hoja da el calor con ce pe. Eso es el calor, no el cambio de energía interna.".replace("ce pe", dice("Cₚ", "ce pe")),
         [F(r"dQ=nC_pdT"), M(r"Q=nC_p\Delta T")]),
        ("El trabajo a presión constante, y con el gas ideal pe delta ve es ene erre delta te.".replace("pe delta ve es ene erre delta te", dice("pΔV = nRΔT", "pe delta ve es ene erre delta te")),
         [F(r"W=p\left(V_2-V_1\right)"), F(r"pV=nRT"), M(r"W=p\,\Delta V=nR\,\Delta T")]),
        ("Primera ley: delta U es el calor menos el trabajo.".replace("delta U", dice("ΔU", "delta U")),
         [F(r"\Delta U=Q-W"), M(r"\Delta U=nC_p\Delta T-nR\Delta T=n\left(C_p-R\right)\Delta T")]),
        ("Y de la hoja, ce pe menos erre es ce ve. Queda la casilla de energía interna: es la A.".replace("ce pe menos erre es ce ve", dice("Cₚ − R = C_V", "ce pe menos erre es ce ve")),
         [F(r"C_P=C_V+R"), M(r"C_p-R=C_V\;\Rightarrow\;\Delta U=nC_V\Delta T\;\Rightarrow\;\text{A}")]),
    ])


def _q093(L: Leccion) -> None:
    _paso(L, [
        ("Adiabático quiere decir que no entra ni sale calor. Expansión quiere decir que el gas hace trabajo positivo.",
         [M(r"Q=0\qquad W>0")]),
        ("Primera ley de la hoja, con calor cero.", [F(r"\Delta U=Q-W"), M(r"\Delta U=0-W=-W<0")]),
        ("Con la otra casilla de la hoja, despejamos el cambio de temperatura: dividimos entre ene ce ve.".replace("ene ce ve", dice("nC_V", "ene ce ve")),
         [F(r"\Delta U=nC_V\Delta T"), M(r"\Delta T=\frac{\Delta U}{nC_V}<0")]),
        ("Se ve también con la adiabática de la hoja: si ve crece, te tiene que bajar para que el producto no cambie. Es la B.".replace("si ve crece, te", "si " + dice("V", "ve") + " crece, " + dice("T", "te")),
         [F(r"TV^{\gamma-1}=\text{constante}"), M(r"V\uparrow\;\Rightarrow\;T\downarrow\;\Rightarrow\;\text{B}")]),
    ])


def _q095(L: Leccion) -> None:
    _paso(L, [
        ("En una isoterma, del gas ideal, la presión va como uno entre ve.".replace("uno entre ve", dice("1/V", "uno entre ve")),
         [F(r"pV=nRT"), M(r"\text{isoterma: }p=\frac{nRT}{V}")]),
        ("En una adiabática, de la hoja, va como uno entre ve a la gama.".replace("uno entre ve a la gama", dice("1/Vᵞ", "uno entre ve a la gama")),
         [F(r"pV^{\gamma}=\text{constante}"), M(r"\text{adiabática: }p=\frac{\text{cte}}{V^{\gamma}}")]),
        ("Si el volumen se duplica, en la isoterma la presión baja a la mitad; en la adiabática baja más, porque gama es mayor que uno.".replace("gama", dice("γ", "gama")),
         [M(r"V\to 2V:\quad p_{\text{iso}}\to\frac{p}{2}\qquad p_{\text{adi}}\to\frac{p}{2^{\gamma}}<\frac{p}{2}")]),
        ("La adiabática cae más empinada que la isoterma y llega a una isoterma más fría: es el proceso B.",
         [M(r"\text{más empinada que la isoterma}\;\Rightarrow\;\text{B}")]),
    ])


def _q101(L: Leccion) -> None:
    _paso(L, [
        ("Adiabático: no entra calor. Compresión: el volumen baja, así que el trabajo del gas es negativo.",
         [M(r"Q=0\qquad V_2<V_1\;\Rightarrow\;W<0")]),
        ("Primera ley de la hoja: delta U es menos el trabajo, y menos por menos da más.".replace("delta U", dice("ΔU", "delta U")),
         [F(r"\Delta U=Q-W"), M(r"\Delta U=0-W=-W>0")]),
        ("El trabajo del gas es negativo y la energía interna sube. Es la B.",
         [M(r"W<0,\ \Delta U>0\;\Rightarrow\;\text{B}")]),
    ])


def _q104(L: Leccion) -> None:
    _paso(L, [
        ("Isotérmico: la temperatura no cambia. Con la casilla de energía interna, delta U es cero.".replace("delta U", dice("ΔU", "delta U")),
         [F(r"\Delta U=nC_V\Delta T"), M(r"\Delta T=0\;\Rightarrow\;\Delta U=0\;\Rightarrow\;\text{C}")]),
        ("El trabajo isotérmico de la hoja: al expandirse, ve dos es mayor que ve uno y el logaritmo es positivo.".replace("ve dos es mayor que ve uno", dice("V₂ > V₁", "ve dos es mayor que ve uno")),
         [F(r"W=nRT\ln\left(\frac{V_2}{V_1}\right)"), M(r"\frac{V_2}{V_1}>1\;\Rightarrow\;\ln\frac{V_2}{V_1}>0\;\Rightarrow\;W>0")]),
        ("El trabajo es positivo: la A y la B son falsas. Y delta U no es positivo: la D es falsa.".replace("delta U", dice("ΔU", "delta U")),
         [M(r"W>0,\ \Delta U=0\;\Rightarrow\;\text{C}")]),
    ])


def _q117(L: Leccion) -> None:
    _paso(L, [
        ("Para un refrigerador la hoja da el coeficiente de rendimiento: lo que se saca del frío entre el trabajo.",
         [F(r"K=\frac{|Q_c|}{|W|}=\frac{|Q_c|}{|Q_H|-|Q_c|}")]),
        ("Un ejemplo: saca novecientos joules con trescientos de trabajo.",
         [M(r"K=\frac{900}{300}=3")]),
        ("Da tres, mayor que uno. Se llama coeficiente, no eficiencia, y puede pasar de uno. Es la D.",
         [M(r"K>1\ \text{es normal}\;\Rightarrow\;\text{D}")]),
    ])


def _q119(L: Leccion) -> None:
    _paso(L, [
        ("La hoja define la eficiencia como el trabajo entre el calor que entra.", [F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_c}{Q_H}\right|")]),
        ("Es una fracción: qué parte del calor que entra se vuelve trabajo. Por ejemplo, treinta de cien es cero coma tres.",
         [M(r"e=\frac{30}{100}=0{,}3\ \text{(30 % del calor se vuelve trabajo)}")]),
        ("La C es una resta, no una fracción; la D está al revés. Es la B.",
         [M(r"e=\frac{W}{Q_H}\;\Rightarrow\;\text{B}")]),
    ])


def _q123(L: Leccion) -> None:
    _paso(L, [
        ("La hoja: calor sacado del frío entre trabajo.", [F(r"K=\frac{|Q_c|}{|W|}=\frac{|Q_c|}{|Q_H|-|Q_c|}")]),
        ("Si ka vale cuatro, se sacan cuatro joules por cada joule de trabajo. La B lo dice al revés.".replace("ka vale", dice("K", "ka") + " vale"),
         [M(r"K=4\;\Rightarrow\;|Q_c|=4\,|W|")]),
        ("Más ka es más calor sacado por cada joule pagado: mejor refrigerador. Es la C.".replace("Más ka", "Más " + dice("K", "ka")),
         [M(r"K\ \text{alto}\;\Rightarrow\;\text{más }|Q_c|\text{ por joule}\;\Rightarrow\;\text{C}")]),
    ])


def _q124(L: Leccion) -> None:
    _paso(L, [
        ("De la casilla del refrigerador sale cuánto trabajo se paga: el calor que bota menos el que saca.",
         [F(r"K=\frac{|Q_c|}{|W|}=\frac{|Q_c|}{|Q_H|-|Q_c|}"), M(r"|W|=|Q_H|-|Q_c|")]),
        ("Pasamos el calor que saca sumando: lo que bota es lo que saca más el trabajo.",
         [M(r"|Q_H|=|Q_c|+|W|")]),
        ("Con la puerta abierta, saca calor de la cocina y lo devuelve a la cocina, más el trabajo del motor.",
         [M(r"\text{neto para la cocina}=|Q_H|-|Q_c|=|W|>0")]),
        ("La cocina gana energía: se calienta. Es la B.", [M(r"|W|>0\;\Rightarrow\;\text{se calienta}\;\Rightarrow\;\text{B}")]),
    ])


def _q126(L: Leccion) -> None:
    _paso(L, [
        ("La eficiencia de la hoja.", [F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_c}{Q_H}\right|")]),
        ("Para que sea cien por ciento, el calor que se bota al foco frío tendría que ser cero.",
         [M(r"e=1\;\Rightarrow\;\left|\frac{Q_c}{Q_H}\right|=0\;\Rightarrow\;|Q_c|=0")]),
        ("La segunda ley dice que siempre se bota algo al foco frío. Entonces la eficiencia siempre es menor que uno. Es la D.",
         [M(r"|Q_c|>0\;\Rightarrow\;e<1\;\Rightarrow\;\text{D}")]),
    ])


def _q129(L: Leccion) -> None:
    _paso(L, [
        ("Con la eficiencia de la hoja, el trabajo es lo que entra menos lo que se bota.",
         [F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_c}{Q_H}\right|"), M(r"W=Q_H-|Q_c|")]),
        ("Como siempre se bota algo, el trabajo es menor que el calor que entra: ninguna máquina convierte todo en trabajo. Es la A.",
         [M(r"|Q_c|>0\;\Rightarrow\;W<Q_H\;\Rightarrow\;\text{A}")]),
    ])


# ---------------------------------------------------------------- fuera del corte (Carnot, Van der Waals)
# No entran en el III parcial II-2026, pero igual se razonan desde la hoja: lo que no está impreso
# (Carnot, la ecuación de Van der Waals) se deriva o se presenta como corrección y se dice que no está en la hoja.
# Entropía: el usuario indicó que no entra en el examen; esas lecciones no se tocan.


def _a_resumen_y_poner(L: Leccion, p: Paso) -> None:
    k = 1 if L.pasos and L.pasos[0].titulo.startswith("Qué nos dan") else 0
    fin = next((i for i, q in enumerate(L.pasos) if q.titulo.startswith("Revisar las opciones")), len(L.pasos))
    for q in L.pasos[k:fin]:
        if not q.titulo.startswith("En resumen"):
            q.titulo = f"En resumen: {q.titulo[0].lower()}{q.titulo[1:]}"
    L.pasos.insert(k, p)


def _q125(L: Leccion) -> None:
    from .ayudas import deriva_carnot
    p = Paso("Desde la hoja, paso a paso")
    deriva_carnot(p)
    p.di("Con los focos del problema, en kelvin.", M(r"e_{\text{Carnot}}=1-\frac{350}{700}=1-0{,}5=0{,}50"))
    p.di("La máquina reversible A es la de Carnot: nadie entre esos focos la supera. La irreversible B tiene menos eficiencia. Es la B.",
         M(r"e_B<e_A=0{,}50\;\Rightarrow\;\text{B}"))
    _a_resumen_y_poner(L, p)


def _q118(L: Leccion) -> None:
    from .ayudas import deriva_carnot
    p = Paso("Desde la hoja, paso a paso")
    p.di("El rendimiento de un refrigerador, de la hoja: calor sacado del frío entre el trabajo.",
         F(r"K=\frac{|Q_c|}{|W|}=\frac{|Q_c|}{|Q_H|-|Q_c|}"))
    deriva_carnot(p, refri=True)
    p.di("Si la cocina está más caliente, sube la temperatura del foco caliente y el denominador crece: el rendimiento máximo baja.",
         M(r"T_H\uparrow\;\Rightarrow\;T_H-T_C\uparrow\;\Rightarrow\;K_{\text{Carnot}}\downarrow"))
    p.di("Un refrigerador real sigue la misma tendencia: cuesta más trabajo sacar el mismo calor. Es la C.",
         M(r"T_{\text{ambiente}}\uparrow\;\Rightarrow\;K\downarrow\;\Rightarrow\;\text{C}"))
    _a_resumen_y_poner(L, p)


def _vdw_base() -> List[Tramo_]:
    return [
        ("La ecuación del gas ideal de la hoja supone moléculas sin tamaño y que no se atraen.", [F(r"pV=nRT")]),
        ("Van der Waals, que no viene en la hoja, corrige esas dos cosas sobre la misma ecuación.",
         [M(r"\left(p+\frac{an^2}{V^2}\right)(V-nb)=nRT"), NOTA("Ecuación de Van der Waals: no está en la hoja II-2026 y no entra en el III parcial.")]),
        ("Al volumen se le resta el espacio que ocupan las moléculas: eso es b, el volumen de un mol de moléculas.",
         [M(r"V\to V-nb\quad(b:\ \text{volumen propio de las moléculas})")]),
        ("A la presión se le suma un término con a: la atracción entre moléculas hace que choquen menos fuerte contra la pared.",
         [M(r"p\to p+\frac{an^2}{V^2}\quad(a:\ \text{atracción entre moléculas})")]),
    ]


def _q071(L: Leccion) -> None:
    _paso(L, _vdw_base() + [("Su propósito es corregir lo que el gas ideal omite: el tamaño de las moléculas y su atracción. Es la B.",
                             [M(r"\text{corrige } pV=nRT\;\Rightarrow\;\text{B}")])])


def _q072(L: Leccion) -> None:
    _paso(L, _vdw_base() + [("Junto a la pared, una molécula solo tiene vecinas hacia adentro: la atracción la jala hacia el interior y choca con menos fuerza. Es la C.",
                             [M(r"p_{\text{real}}=p_{\text{ideal}}-\frac{an^2}{V^2}<p_{\text{ideal}}\;\Rightarrow\;\text{C}")])])


def _q073(L: Leccion) -> None:
    _paso(L, _vdw_base() + [("Entonces a corrige la presión que baja por la atracción, y b es el volumen de un mol de moléculas. Es la D.",
                             [M(r"a:\ \text{presión (atracción)}\qquad b:\ \text{volumen molecular}\;\Rightarrow\;\text{D}")])])


AGREGADOS: Dict[str, Callable[[Leccion], None]] = {
    "Q062": _q062, "Q065": _q065, "Q075": _q075, "Q077": _q077, "Q079": _q079, "Q081": _q081, "Q082": _q082,
    "Q083": _q083, "Q084": _q084, "Q085": _q085, "Q086": _q086, "Q088": _q088, "Q090": _q090, "Q093": _q093,
    "Q095": _q095, "Q101": _q101, "Q104": _q104, "Q117": _q117, "Q119": _q119, "Q123": _q123, "Q124": _q124,
    "Q126": _q126, "Q129": _q129,
    # Fuera del corte, razonadas igual desde la hoja (Carnot y Van der Waals)
    "Q125": _q125, "Q118": _q118, "Q071": _q071, "Q072": _q072, "Q073": _q073,
}


def aplicar(lec: Leccion) -> bool:
    fn = AGREGADOS.get(lec.id)
    if fn is None or getattr(lec, "_concepto", False):
        return False
    fn(lec)
    lec._concepto = True  # type: ignore[attr-defined]
    return True

