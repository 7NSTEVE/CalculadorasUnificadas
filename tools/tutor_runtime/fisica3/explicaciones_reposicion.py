"""Paso «Datos» y «¿Por qué?» de las lecciones del plan «Reposición II-2026».

Dos cosas que pidió la persona para estudiar la reposición:

* **Señalar los datos del enunciado.** Cada lección del plan empieza con un paso «Datos» que
  subraya en el recorte del PDF cada dato, lo escribe en la pizarra y dice para qué sirve (qué
  casilla de la hoja lo usa). Si la lección ya traía su propio resumen «Qué nos dan…», ese paso se
  queda después, como repaso ordenado.
* **Explicar por qué se analiza de esa manera.** Cada paso lleva su «¿Por qué?» (botón Ayuda): por
  qué se usa esa casilla y no otra, qué palabra del enunciado lo decide.

Los textos de cada lección están en DATOS y PORQUE. Los pasos que se repiten en muchos problemas
(isocórico, isobárico, adiabático, recta, revisar opciones) tienen un porqué común en _GENERICOS.
`aplicar(lec)` se llama desde registro.obtener, después de conceptos y desglose; los pasos «(sigue)»
que crea el desglose heredan el porqué de su paso.

Las frases de DATOS se escriben como en el enunciado; antes de mandar la lección, banco_api las
cambia por las palabras exactas del PDF (documentos.ajustar_datos), así «6,0 cm» calza con «6, 0 cm».
En la voz no va LaTeX: los símbolos se escriben [[p_A]] y se narran con dsl.s().
"""

from __future__ import annotations

import re
from typing import Dict, List, Optional, Sequence, Tuple

from .dsl import DATO, M, Leccion, Paso, Tramo, s
from .formato import separa

# (frase tal como sale en el enunciado, renglón para la pizarra, para qué sirve)
# Si la frase va vacía, el dato sale de la figura: se escribe en la pizarra sin subrayar.
_Dato = Tuple[str, str, str]

TITULO_DATOS = "Datos: qué nos dan y para qué sirve cada uno"
_INTRO = "Antes de empezar, subrayamos cada dato del enunciado y decimos para qué sirve."
_MAX_PALABRAS = 75  # el tope de dsl es 90 por paso; se deja margen

_PORQUE_DATOS = (
    "Subrayar primero cada dato evita usar un número equivocado y dice qué casilla de la hoja se va a "
    "necesitar: cada dato tiene su lugar en una fórmula. Un dato que no entra en ninguna fórmula suele "
    "ser un distractor."
)

# Porqués de pasos que se repiten en muchos problemas. Se usan solo si la lección no trae uno propio.
_GENERICOS: Tuple[Tuple[str, str], ...] = (
    (r"^datos:", _PORQUE_DATOS),
    (r"^qu[eé] nos dan",
     "Este paso ordena lo que ya se subrayó: qué se sabe de cada punto y qué hay que encontrar. Tenerlo junto "
     "en una lista evita volver al enunciado a cada rato y deja ver qué falta por calcular."),
    (r"^(revisar las opciones|qu[eé] opci[oó]n es)$",
     "Descartar cada opción con una razón física, y no solo buscar el número, protege de las trampas: "
     "las opciones malas suelen ser el resultado de un error típico (invertir un despeje, olvidar un "
     "cuadrado, confundir el cambio con el valor final)."),
    (r"isoc[oó]ric",
     "En este proceso el volumen no cambia (en el diagrama p-V es una línea vertical). Si el volumen no "
     "cambia, el gas no empuja nada: $W = 0$. Entonces la primera ley de la hoja, $Q = \\Delta U + W$, "
     "queda $Q = \\Delta U = nC_V\\Delta T$."),
    (r"isob[aá]ric|presi[oó]n constante",
     "En este proceso la presión no cambia (línea horizontal en el diagrama p-V). El trabajo es el área "
     "bajo la línea, que aquí es un rectángulo: $W = p\\,\\Delta V$. $\\Delta U$ siempre es "
     "$nC_V\\Delta T$, y el calor sale de la primera ley: $Q = \\Delta U + W$."),
    (r"(compresi[oó]n|expansi[oó]n) adiab[aá]tica",
     "Adiabático quiere decir que no entra ni sale calor: $Q = 0$. Con eso la primera ley queda "
     "$W = -\\Delta U$, y $\\Delta U = nC_V\\Delta T$ solo necesita las temperaturas de la tabla. Por eso "
     "la tabla de estados va primero."),
    (r"\(recta\)",
     "En el diagrama p-V este proceso es una recta inclinada: ni la presión ni el volumen se quedan "
     "fijos, así que no sirve $p\\,\\Delta V$. El trabajo es el área bajo la recta, que es un trapecio: "
     "el promedio de las dos presiones por el cambio de volumen. No hace falta integrar."),
    (r"^temperaturas de los cuatro estados",
     "Los calores y los $\\Delta U$ necesitan temperaturas, pero el problema da presiones y volúmenes. "
     "La casilla $pV = nRT$ es la que pasa de $(p, V)$ a $T$: se despeja $T = pV/(nR)$ en cada estado."),
)


# ============================================================================== datos por lección

DATOS: Dict[str, Tuple[_Dato, ...]] = {
    # ---------------------------------------------------------------- Parcial 3: ciclos y calor
    "Q110": (
        ("2,50 mol de helio", r"n=2{,}50\ \text{mol}",
         "Es el número de moles. Entra en el gas ideal para sacar cada temperatura y en cada cambio de energía interna."),
        ("monoatómico", r"C_V=\tfrac32R\qquad \gamma=\tfrac53",
         "Monoatómico decide las capacidades de la hoja: tres medios de [[R]] a volumen constante, y [[γ]] igual a cinco tercios para la adiabática."),
        ("cinco veces la presión en A", r"p_B=5\,p_A",
         "Fija la presión de B. Junto con la adiabática, sirve para despejar el volumen de B."),
        ("1,26 m3", r"V_A=1{,}26\ \text{m}^3", "Es el volumen del punto A. Ya está en el SI."),
        ("300 K", r"T_A=300\ \text{K}",
         "Es la temperatura de A. Con el volumen y los moles, el gas ideal da la presión de A."),
        ("máquina de refrigeración", r"K=\dfrac{|Q_C|}{|W|}",
         "Dice que es un refrigerador: lo que se pide es el coeficiente de rendimiento, no la eficiencia."),
    ),
    "Q109": (
        ("0,378 mol", r"n=0{,}378\ \text{mol}",
         "Es el número de moles: entra en el gas ideal y en cada cambio de energía interna."),
        ("diatómico", r"C_V=\tfrac52R\qquad \gamma=1{,}40",
         "Diatómico decide las capacidades de la hoja: cinco medios de [[R]], y [[γ]] igual a uno coma cuarenta para la adiabática."),
        ("9,00 L", r"V_A=V_B=9{,}00\ \text{L}",
         "Es el volumen de A, y también el de B, porque de A a B el volumen no cambia. Hay que pasarlo a metros cúbicos."),
        ("4,30 atm", r"p_A=4{,}30\ \text{atm}",
         "Es la presión de A. Para el SI se multiplica por ciento un mil trescientos veinticinco."),
        ("un tercio del volumen", r"V_C=\tfrac13V_A=3{,}00\ \text{L}",
         "Da el volumen de C. Con la adiabática une el punto B con el C."),
        ("expansión isobárica", r"p_C=p_A",
         "De C a A la presión no cambia: C tiene la presión de A. Así C queda completo sin calcular nada."),
    ),
    "O26-2": (
        ("eficiencia", r"e=20\,\%=0{,}20=\tfrac15",
         "La eficiencia es del veinte por ciento. Como fracción es un quinto, y así aparece en las opciones."),
        ("máquina térmica", r"e=\dfrac{W}{Q_H}",
         "Es un motor: su casilla es la eficiencia, trabajo entre calor que entra."),
        ("calor brindado", r"Q_H", "Es el calor que recibe la máquina. Va abajo en la fracción."),
        ("trabajo realizado", r"W", "Es lo que hay que despejar en función del calor brindado."),
    ),
    "Q108": (
        ("Un mol", r"n=1\ \text{mol}", "Es el número de moles: entra en el gas ideal para cada temperatura."),
        ("monoatómico", r"C_V=\tfrac32R",
         "Monoatómico fija la capacidad a volumen constante de la hoja: tres medios de [[R]]."),
        ("la mitad de la presión", r"p_C=\tfrac12\,p_A",
         "Da la presión de C a partir de la de A, que se lee en la figura."),
        ("tres veces mayor", r"V_B=3V_A=0{,}0060\ \text{m}^3",
         "Da el volumen de B. B está a la misma presión que A, en la línea de arriba del triángulo."),
        ("0,0045", r"V_C=0{,}0045\ \text{m}^3", "Es el volumen de C, leído en el eje horizontal de la figura."),
        ("máquina térmica", r"e=\dfrac{W_{neto}}{Q_H}", "Dice que es un motor: al final se pide la eficiencia."),
    ),
    "Q106": (
        ("dos moles", r"n=2\ \text{mol}", "Es el número de moles: entra en el gas ideal para cada temperatura."),
        ("cinco grados de libertad", r"C_V=\tfrac52R",
         "Cinco grados de libertad es un diatómico: la hoja da cinco medios de [[R]] a volumen constante."),
        ("1,22×105", r"p_{baja}=1{,}22\times10^5\ \text{Pa}", "Es la presión de la línea de abajo del rectángulo."),
        ("2,03×105", r"p_{alta}=2{,}03\times10^5\ \text{Pa}", "Es la presión de la línea de arriba."),
        ("7,00×10-2", r"V_{min}=7{,}00\times10^{-2}\ \text{m}^3", "Es el volumen menor, el lado izquierdo del rectángulo."),
        ("9,00×10-2", r"V_{max}=9{,}00\times10^{-2}\ \text{m}^3", "Es el volumen mayor, el lado derecho."),
    ),
    "Q051": (
        ("4,00 m de altura", r"\text{alto}=4{,}00\ \text{m}",
         "Alto de la pared. Con el ancho da el área por donde pasa el calor."),
        ("3,00 m de ancho", r"\text{ancho}=3{,}00\ \text{m}", "Ancho de la pared. El área es alto por ancho."),
        ("20,0 cm de grosor", r"L=20{,}0\ \text{cm}=0{,}200\ \text{m}",
         "Es el grosor, la [[L]] de la casilla de conducción. Va abajo y en metros."),
        ("conductividad térmica", r"k=0{,}800\ \tfrac{\text{W}}{\text{m}\cdot\text{K}}",
         "Es la conductividad del ladrillo, la [[k]] de la casilla."),
        ("30,0 °C", r"T_H=30{,}0\ ^\circ\text{C}", "Temperatura de adentro, el lado caliente."),
        ("10,0 °C", r"T_C=10{,}0\ ^\circ\text{C}",
         "Temperatura de afuera. Solo importa la diferencia: veinte grados, que valen lo mismo en kelvin."),
    ),
    "Q020": (
        ("0,500 kg", r"m=0{,}500\ \text{kg}", "Es la masa del bloque. Va en la casilla del calor sensible."),
        ("20,0 °C", r"T_i=20{,}0\ ^\circ\text{C}", "Es la temperatura de inicio. Al final se le suma el cambio."),
        ("9,00 x 103 J", r"Q=9{,}00\times10^3\ \text{J}", "Es el calor que absorbe. Es positivo porque entra al bloque."),
        ("calor específico del aluminio", r"c=900\ \tfrac{\text{J}}{\text{kg}\cdot\text{K}}",
         "Es la [[c]] de la casilla: cuánto calor necesita un kilogramo para subir un grado."),
    ),
    "O26-6": (
        ("gas diatómico", r"\text{traslación}+\text{rotación}+\text{vibración}",
         "Diatómico: la molécula puede trasladarse, girar y vibrar. Cada forma de moverse que se activa suma a la capacidad calorífica."),
        ("volumen constante", r"C_V", "A volumen constante, la capacidad que se mide es [[C_V]]."),
        ("temperatura muy baja", r"T\ \text{baja}:\ C_V=\tfrac32R",
         "A temperatura muy baja solo hay traslación: tres medios de [[R]]."),
        ("aumenta", r"C_V\ \text{sube}",
         "Aumenta: hay que explicar de dónde salen formas nuevas de guardar energía."),
    ),
    "O26-10": (
        ("térmicamente aislado", r"\textstyle\sum Q=0",
         "Aislado: no entra ni sale calor del recipiente. Por eso la suma de todos los calores es cero."),
        ("kg de agua", r"m_a=0{,}500\ \text{kg}",
         "Masa del agua, que empieza a treinta y ocho coma tres grados y va a ceder calor."),
        ("bloque de aluminio", r"m_{Al}=0{,}300\ \text{kg}",
         "El aluminio está a la misma temperatura que el agua y también cede calor."),
        ("kg de hielo", r"m_h=0{,}150\ \text{kg}\qquad T_h=-15{,}0\ ^\circ\text{C}",
         "El hielo está bajo cero: primero se calienta hasta cero, luego se funde, y después el agua derretida se calienta."),
        ("despreciable", r"C_{recipiente}=0", "El recipiente no absorbe calor: no entra en la suma."),
        ("", r"c_{agua}=4186\quad c_{Al}=900\quad c_{hielo}=2090\quad L_f=3{,}34\times10^5",
         "Al final vienen los calores específicos y el calor latente de fusión, que se usa al derretir el hielo."),
    ),
    # ---------------------------------------------------------------- Parcial 2: fluidos
    "RP-26I-3": (
        ("igual tamaño y del mismo material", r"m_A=m_B\qquad V_A=V_B",
         "Mismo tamaño y mismo material: las dos esferas pesan lo mismo y tienen el mismo volumen. Eso es lo que se va a cancelar."),
        ("40 %", r"V_{hund,A}=0{,}40\,V", "En el fluido A se hunde el cuarenta por ciento del volumen."),
        ("60 %", r"V_{hund,B}=0{,}60\,V",
         "En el fluido B se hunde el sesenta por ciento. Se hunde menos en A, así que A es el más denso."),
    ),
    "RP-26I-4": (
        ("20 kg/s", r"\dfrac{dm}{dt}=20\ \text{kg/s}",
         "Es el flujo de masa, en kilogramos por segundo. Con la densidad se pasa a caudal."),
        ("1040 kg/m3", r"\rho=1040\ \text{kg/m}^3", "Densidad del fluido: convierte masa por segundo en volumen por segundo."),
        ("0,0015 m2", r"A=0{,}0015\ \text{m}^2", "Área de la tubería: el caudal entre el área da la rapidez."),
    ),
    "P2O26-2": (
        ("600 kg/m³", r"\rho_{mad}=600\ \text{kg/m}^3",
         "Densidad de la madera. Es menor que la del agua y la del aceite: por eso flota en los dos."),
        ("1000 kg/m3", r"\rho_{agua}=1000\ \text{kg/m}^3", "Densidad del agua."),
        ("800 kg/m3", r"\rho_{ac}=800\ \text{kg/m}^3", "Densidad del aceite, menor que la del agua."),
        ("también flota", r"B=mg",
         "También flota: en los dos líquidos el empuje iguala al peso. Esa es la clave de la pregunta."),
    ),
    "P2O26-4": (
        ("6,0 cm", r"D_1=6{,}0\ \text{cm}", "Diámetro de la parte ancha."),
        ("2,0 m/s", r"v_1=2{,}0\ \text{m/s}", "Rapidez en la parte ancha."),
        ("3,0 cm", r"D_2=3{,}0\ \text{cm}", "Diámetro de la parte angosta: la mitad del ancho."),
        ("incompresible", r"A_1v_1=A_2v_2",
         "Incompresible: el mismo volumen por segundo pasa por las dos secciones. Es la condición de la continuidad."),
    ),
    "P2E26-1": (
        ("velocidad constante", r"a=0\ \Rightarrow\ \textstyle\sum F=0",
         "Velocidad constante quiere decir aceleración cero: las fuerzas se equilibran."),
        ("permanece sumergida", r"F_B\ \text{hacia arriba}", "Sigue bajo el agua: el empuje sigue empujando hacia arriba."),
        ("la masa m", r"\text{peso}=mg", "Con la masa se escribe el peso, que tira hacia abajo."),
        ("2700 kg/m3", r"\rho_{m\acute{a}rmol}=2700\ \text{kg/m}^3",
         "Densidad del mármol. Aquí es un distractor: la respuesta se pide en letras, con el empuje y el peso."),
    ),
    "P2O26-7": (
        ("1,5 L", r"V_0=1{,}5\ \text{L}",
         "Volumen inicial del cuerpo. Se puede dejar en litros, porque el cambio también se pide en litros."),
        ("5460 MPa", r"B=5460\ \text{MPa}=5{,}46\times10^9\ \text{Pa}",
         "Módulo de compresibilidad: cuánto se resiste a comprimirse. Mega es diez a la seis."),
        ("2000 m", r"h=2000\ \text{m}", "Profundidad: da la presión extra con la hidrostática."),
        ("1025 kg/m3", r"\rho=1025\ \text{kg/m}^3", "Densidad del agua de mar, para la presión extra."),
    ),
    "P2O26-3": (
        ("fluido ideal", r"\text{ideal}\Rightarrow\text{Bernoulli}",
         "Piden las condiciones del fluido ideal, el único en el que vale Bernoulli."),
        ("densidad permanece constante", r"\text{I: incompresible}", "La afirmación uno describe un fluido incompresible."),
        ("constantes con el tiempo", r"\text{II: flujo estable}", "La dos describe un flujo estable."),
        ("rotación local", r"\text{III: ¿irrotacional?}", "La tres permite rotación local: hay que revisarla con cuidado."),
    ),
    "RP-26I-8": (
        ("permanece constante", r"\left(\tfrac{dm}{dt}\right)_{entra}=\left(\tfrac{dm}{dt}\right)_{sale}",
         "Nivel constante: sale lo mismo que entra. Por eso el flujo de entrada sirve para la salida."),
        ("60,0 kg/s", r"\dfrac{dm}{dt}=60{,}0\ \text{kg/s}", "Flujo de masa que entra, igual al que sale."),
        ("1030 kg/m3", r"\rho=1030\ \text{kg/m}^3",
         "Densidad del agua salada: pasa de masa por segundo a volumen por segundo."),
        ("0,14 m", r"D=0{,}140\ \text{m}\ \Rightarrow\ r=0{,}070\ \text{m}",
         "Diámetro del orificio. Para el área se usa el radio, la mitad."),
        ("abierto a la atmósfera", r"p_1=p_2=p_{atm}",
         "Abierto: la superficie y el chorro están a presión atmosférica. En Bernoulli se cancela."),
    ),
    "RP-24II-2": (
        ("0,800 L/s", r"\tfrac{dV}{dt}=0{,}800\ \text{L/s}=8{,}00\times10^{-4}\ \text{m}^3\text{/s}",
         "Caudal: es el mismo en todo el tubo. Un litro es una milésima de metro cúbico."),
        ("4,00 cm", r"D_a=4{,}00\ \text{cm}", "Diámetro en el punto a."),
        ("2,00 cm", r"D_b=2{,}00\ \text{cm}", "Diámetro en el punto be: la mitad. El área es la cuarta parte."),
        ("extremos abiertos a la atmósfera", r"p=p_{atm}+\rho g h",
         "Los tubos verticales abiertos son manómetros: la altura del agua mide la presión del tubo."),
    ),
    "P2O26-5": (
        ("líquido ideal", r"\text{continuidad y Bernoulli}", "Líquido ideal: valen la continuidad y Bernoulli."),
        ("tubería horizontal", r"y_1=y_2",
         "Horizontal: los dos puntos están a la misma altura. En Bernoulli se cancelan los términos de altura."),
        ("disminuye gradualmente", r"A_2<A_1", "La sección se achica: el área dos es menor que la uno."),
    ),
    "P2E26-3": (
        ("orificio en el fondo", r"y_1-y_2=h",
         "La altura entre la superficie y el orificio es la profundidad [[h]]."),
        ("mucho mayor que el área", r"v_1\approx 0",
         "El tanque es mucho más ancho que el orificio: la superficie baja tan despacio que su rapidez se toma como cero."),
        ("presión atmosférica", r"p_1=p_2=p_{atm}",
         "Arriba y en la salida hay presión atmosférica: en Bernoulli se cancela."),
    ),
    "P2E26-6": (
        ("presión estática", r"p=1{,}00\times10^5\ \text{Pa}", "Presión del aire que va con la corriente."),
        ("presión de remanso", r"p_0=1{,}006\times10^5\ \text{Pa}",
         "Presión en la punta del tubo, donde el aire se frena hasta quedar quieto."),
        ("1,20 kg/m3", r"\rho_a=1{,}20\ \text{kg/m}^3", "Densidad del aire: va en el término de rapidez de Bernoulli."),
    ),
    "P2E26-7": (
        ("hacia abajo verticalmente", r"y_A=h\qquad y_B=0", "Baja en vertical: A está más arriba que B, una altura [[h]]."),
        ("de forma estacionaria", r"A_Av_A=A_Bv_B", "Flujo estacionario: vale la continuidad."),
        ("", r"A\ \text{angosta}\qquad B\ \text{ancha}", "En la figura, A está en la parte angosta y B en la ancha."),
    ),
    "P2E26-5": (
        ("fluido ideal", r"\text{continuidad y Bernoulli}", "Fluido ideal: valen la continuidad y Bernoulli."),
        ("sección variable", r"A_{ancha}>A_{angosta}",
         "El tubo cambia de sección. En la figura, el tubo de altura [[H]] está en la parte ancha y el de altura [[L]] en la angosta."),
        ("alturas H y L", r"p-p_{atm}=\rho g\cdot\text{altura}",
         "La altura de cada tubo vertical mide la presión de ese punto."),
    ),
    # ---------------------------------------------------------------- Parcial 2: paredes y manómetros
    "P2E26-8": (
        ("5,45 m", r"H=5{,}45\ \text{m}",
         "Altura del agua sobre el fondo. Da la profundidad de cada punto: cinco coma cuarenta y cinco menos [[y]]."),
        ("abierto a la atmósfera", r"p_{man}=\rho g h",
         "Abierto: arriba hay aire. La presión manométrica es solo la del agua."),
        ("30,0 cm de lado", r"a=0{,}300\ \text{m}", "Lado de la ventana: es el ancho de cada franja y su alto total."),
        ("3,45 m", r"y_{sup}=3{,}45\ \text{m}\qquad y_{inf}=3{,}15\ \text{m}",
         "Altura del borde de arriba sobre el fondo. Restando el lado, el borde de abajo está en tres coma quince."),
        ("10,0 m de lado", r"\text{tanque de }10{,}0\ \text{m}",
         "El tamaño del tanque es un distractor: la presión solo depende de la profundidad."),
        ("30,0 cm de diámetro", r"D=0{,}300\ \text{m}", "Para el último inciso: ventana circular con el mismo centro y menos área."),
    ),
    "RP-24I-2": (
        ("4,00 m", r"H=4{,}00\ \text{m}", "Altura de la compuerta: los límites de la integral van de cero a cuatro metros."),
        ("2,00 m", r"L=2{,}00\ \text{m}\ \Rightarrow\ \tfrac L2=1{,}00\ \text{m}",
         "Largo total de la compuerta: la mitad rectangular tiene un metro de ancho."),
        ("sección rectangular", r"\text{ancho fijo}", "La mitad izquierda es rectangular: todas sus franjas tienen el mismo ancho."),
        ("forma parabólica", r"y=2x^2\ \Rightarrow\ x=\sqrt{y/2}",
         "La mitad derecha sigue la parábola: el ancho de cada franja sale de despejar [[x]]."),
    ),
    "RP-25II-2": (
        ("hasta una altura de 50,0 m", r"H=50{,}0\ \text{m}",
         "Profundidad del agua: los límites de la integral van de cero a cincuenta."),
        ("45,0 m", r"L(0)=45{,}0\ \text{m}", "Ancho de la represa arriba, en la superficie. Sale de la figura."),
        ("30,0 m", r"L(50)=30{,}0\ \text{m}", "Ancho abajo, en el fondo. Entre los dos, el ancho cambia en línea recta."),
        ("presión atmosférica del aire", r"p_{neta}=\rho g h",
         "Del otro lado de la pared hay aire: la atmosférica se cancela y queda solo la presión del agua."),
        ("84,0 cm", r"D_1=0{,}840\ \text{m}", "Diámetro de la primera tubería."),
        ("70,0 cm", r"D_2=0{,}700\ \text{m}", "Diámetro de la salida, más angosta."),
        ("8,00 m por debajo", r"\Delta y=8{,}00\ \text{m}",
         "La salida está ocho metros más abajo que la primera tubería: la caída desde la superficie es de cincuenta y ocho metros."),
    ),
    "SUF26-3": (
        ("3,46 m2 de base", r"A_{base}=3{,}46\ \text{m}^2",
         "Área de la base del cilindro: el volumen entre el área da la altura de cada capa."),
        ("12,87 m3 de agua de mar", r"V_{agua}=12{,}87\ \text{m}^3", "Volumen del agua de mar, que queda abajo por ser más densa."),
        ("1,02 g/cm3", r"\rho_{agua}=1{,}02\ \text{g/cm}^3=1020\ \text{kg/m}^3",
         "Densidad del agua de mar. Para el SI se multiplica por mil."),
        ("8,58 m3 de un aceite", r"V_{aceite}=8{,}58\ \text{m}^3", "Volumen del aceite, que flota encima."),
        ("0,768 g/cm3", r"\rho_{aceite}=768\ \text{kg/m}^3", "Densidad del aceite."),
        ("10,00 cm de ancho", r"b=0{,}1000\ \text{m}", "Ancho de la compuerta: es el ancho de cada franja."),
        ("40,0 cm de alto", r"0\le y\le 0{,}400\ \text{m}", "Alto de la compuerta: son los límites de la integral."),
        ("1,40 cm2", r"A_{orif}=1{,}40\ \text{cm}^2",
         "Área del orificio. Para la rapidez no hace falta: Bernoulli da la rapidez sin el área."),
    ),
    "P2O26-8": (
        ("en reposo", r"\Delta p=\rho g\,\Delta h", "En reposo: dentro de cada líquido la presión solo cambia con la altura."),
        ("64,0 kPa", r"p_A=64{,}0\ \text{kPa}\ \text{(manométrica)}",
         "Presión manométrica en el punto A: es el punto de partida del recorrido."),
        ("2,40 m", r"y_A=2{,}40\ \text{m}", "Altura del punto A."),
        ("0,850 m", r"y_{agua|Hg}=0{,}850\ \text{m}", "Altura donde el agua toca el mercurio."),
        ("1,35 m", r"y_{Hg|gli}=1{,}35\ \text{m}", "Altura donde el mercurio toca la glicerina."),
        ("0,700 m", r"y_{gli|X}=0{,}700\ \text{m}", "Altura donde la glicerina toca el líquido [[X|equis]]."),
        ("2,65 m", r"y_{sup}=2{,}65\ \text{m}\qquad p_{man}=0",
         "Superficie libre del líquido [[X|equis]], abierta: ahí la presión manométrica es cero."),
        ("13600 kg/m3", r"\rho_{Hg}=13\,600\ \text{kg/m}^3",
         "Densidad del mercurio. Las del agua y la glicerina también vienen en el enunciado."),
    ),
    "O26-8": (
        ("dos cortes planos simétricos", r"-L\le y\le L",
         "Los cortes dejan dos caras planas, abajo y arriba, a la misma distancia [[L]] del centro: son los límites de la integral."),
        ("conductividad térmica", r"k\ \text{constante}", "El material tiene una sola conductividad: [[k]] sale de la integral."),
        ("material aislante", r"H\ \text{igual en cada rebanada}",
         "La superficie curva está aislada: el calor no se escapa por los lados, así que el mismo flujo cruza cada rebanada."),
        ("estado estacionario", r"H=\text{constante}", "Estado estacionario: el flujo de calor no cambia con el tiempo."),
        ("integrales indefinidas", r"\int\frac{du}{a^2-u^2}=\frac{1}{2a}\ln\left|\frac{a+u}{a-u}\right|",
         "Dan tres integrales. Hay que elegir la que tiene exactamente la forma que aparece: aquí, la del logaritmo."),
    ),
    # ---------------------------------------------------------------- Parcial 1: MAS y péndulos
    "RP-26I-7": (
        ("4300 N/m", r"k=4300\ \text{N/m}", "Constante del resorte: con la masa da [[ω]]."),
        ("4,53 kg", r"m=4{,}53\ \text{kg}", "Masa del bloque."),
        ("desplazamiento máximo de 6,24 cm", r"A=6{,}24\ \text{cm}=0{,}0624\ \text{m}",
         "Desplazamiento máximo es la amplitud [[A]]. Va en metros."),
        ("3,00 cm", r"x(0)=0{,}0300\ \text{m}", "Posición al empezar: con la amplitud da el coseno de la fase."),
        ("velocidad es positiva", r"v(0)>0", "El signo de la velocidad al empezar decide cuál de los dos ángulos es la fase."),
    ),
    "P1O26-8": (
        ("0,50 kg", r"m=0{,}50\ \text{kg}", "Masa del bloque: con [[k]] da [[ω]]."),
        ("sin fricción", r"E=\text{constante}",
         "Sin fricción: la energía mecánica no cambia. Por eso su línea en la gráfica es plana."),
        ("0,141 m", r"x(0)=0{,}141\ \text{m}", "Posición al empezar. En la gráfica, ahí se cruzan las curvas de [[U]] y [[K]]."),
        ("hacia la izquierda", r"v_x(0)<0", "Hacia la izquierda es velocidad negativa: decide el signo de la fase."),
        ("Energía mecánica E", r"E=2{,}0\ \text{J}", "En la gráfica, la línea plana de la energía total está en dos joules."),
        ("0,1047 s", r"t=0{,}1047\ \text{s}", "Instante del último inciso."),
    ),
    "P1E26-8": (
        ("2,00 m", r"L=2{,}00\ \text{m}", "Largo de los dos péndulos. En el simple es la distancia del pivote a la esfera."),
        ("1,20 kg", r"m=1{,}20\ \text{kg}", "Masa total de cada péndulo."),
        ("masa despreciable", r"\text{A: péndulo simple}",
         "El péndulo A es una esfera pequeña en un alambre sin masa: es un péndulo simple."),
        ("varilla uniforme", r"m_v=m_e=0{,}60\ \text{kg}",
         "El péndulo B es físico: media masa en la varilla y media en la esfera del extremo."),
        ("1,50 m", r"d=1{,}50\ \text{m}", "Distancia del pivote al centro de masa de B: va en la casilla del péndulo físico."),
        ("5 minutos", r"t=300\ \text{s}", "Tiempo para contar oscilaciones: trescientos segundos."),
    ),
    # ---------------------------------------------------------------- Parcial 1: selecciones de MAS y ondas
    "RP-26I-2": (
        ("péndulo simple", r"\omega^2=\dfrac{g}{L}", "Péndulo simple: su casilla es [[ω]] al cuadrado igual a [[g]] entre [[L]]."),
        ("pequeña amplitud", r"T=2\pi\sqrt{L/g}", "Con pequeña amplitud vale la casilla del péndulo simple."),
        ("se cuadruplica", r"L'=4L", "El largo pasa a ser cuatro veces el original."),
        ("disminuye a la mitad", r"m'=\tfrac12m", "La masa baja a la mitad. Es un distractor: la masa no está en la casilla."),
    ),
    "P1O26-1": (
        ("se duplica su ámbito de movimiento", r"A'=2A", "Ámbito de movimiento es la amplitud: se duplica."),
        ("sin alterar su período", r"T'=T\ \Rightarrow\ \omega'=\omega",
         "El período no cambia, así que [[ω]] tampoco: [[ω]] es dos pi entre el período."),
    ),
    "P1O26-3": (
        ("péndulo físico", r"\omega^2=\dfrac{mgd}{I}",
         "Péndulo físico: su casilla tiene la masa, la distancia al centro de gravedad y la inercia."),
        ("misma masa m", r"m'=m", "La masa no cambia."),
        ("mismo momento de inercia", r"I'=I", "La inercia tampoco cambia."),
        ("nuevo centro de gravedad", r"d'=\tfrac d4",
         "Lo único que cambia es la distancia del eje al centro de gravedad: pasa a un cuarto."),
    ),
    "P1O26-7": (
        ("oscila verticalmente entre los puntos 1 y 3", r"\text{extremos: 1 y 3}", "Los puntos uno y tres son los extremos del movimiento."),
        ("equilibrio en el punto 2", r"y=0\ \text{en el punto 2}", "El punto dos es el equilibrio: ahí el desplazamiento es cero."),
        ("la distancia entre los puntos 1 y 2 es la misma", r"|y_1|=|y_3|=A",
         "Los dos extremos están a la misma distancia del equilibrio: los dos están a una amplitud."),
    ),
    "P1E26-4": (
        ("resorte ideal", r"F=-kx", "Resorte ideal: cumple la ley de Hooke de la hoja."),
        ("sin fricción", r"E=\text{constante}", "Sin fricción: es un movimiento armónico simple con amplitud fija."),
        ("amplitud A, constante elástica k y periodo T", r"T=2\pi\sqrt{m/k}",
         "Dan amplitud, constante y período en letras: cada afirmación se revisa con una casilla, sin números."),
    ),
    "P1E26-6": (
        ("amplitud A", r"E=\tfrac12kA^2", "Con la amplitud se escribe la energía total."),
        ("1 2A", r"x=\tfrac12A", "La posición es la mitad de la amplitud."),
        ("energía cinética (K)", r"K=E-U", "La cinética es lo que falta de la energía total."),
    ),
    "P1E26-1": (
        ("masa puntual", r"\omega^2=\dfrac gL", "Masa puntual colgada de una cuerda: es un péndulo simple."),
        ("pequeñas amplitudes", r"T=2\pi\sqrt{L/g}", "Con pequeñas amplitudes vale la casilla del péndulo simple."),
        ("se reduce a una", r"L'=\tfrac14L", "La longitud se reduce a la cuarta parte."),
    ),
    "P1O26-6": (
        ("onda estacionaria", r"\lambda_n=\dfrac{2L}{n}",
         "Onda estacionaria en una cuerda con dos extremos fijos: su casilla da la longitud de onda de cada armónico."),
        ("segundo armónico", r"n=2", "Segundo armónico: [[n]] igual a dos."),
        ("cuerda de violín", r"\text{extremos fijos}", "La cuerda del violín está fija en los dos extremos: ahí siempre hay nodos."),
    ),
    "P2O26-6": (
        ("cae verticalmente", r"\text{peso hacia abajo}",
         "Cae: el peso tira hacia abajo, y la fuerza viscosa se opone al movimiento, hacia arriba."),
        ("líquido viscoso", r"F_v=6\pi\eta r v", "Líquido viscoso con ley de Stokes: la fuerza viscosa es la casilla de Stokes."),
        ("velocidad terminal", r"a=0", "Velocidad terminal: ya no acelera, las fuerzas se equilibran."),
        ("", r"B=\rho_{l\acute{\imath}q}\,g\,V", "Además, como está dentro de un líquido, siempre hay empuje hacia arriba."),
    ),
    "P2E26-2": (
        ("marco de alambre con un lado móvil de longitud l", r"\gamma=\dfrac{F}{2l}",
         "Es el montaje de la casilla de tensión superficial: la fuerza sobre el lado móvil."),
        ("se aplica una fuerza F", r"F_A=F", "Con el líquido A hace falta la fuerza [[F]]."),
        ("doble de tensión superficial", r"\gamma_B=2\gamma_A", "El líquido B tiene el doble de tensión superficial."),
        ("mismo alambre", r"l'=l", "El alambre es el mismo: el largo no cambia."),
    ),
    "O26-3": (
        ("misma longitud", r"L_{0,1}=L_{0,2}=L_0", "Las dos barras empiezan con el mismo largo."),
        ("es el doble", r"\alpha_2=2\alpha_1", "El coeficiente de la barra dos es el doble del de la uno."),
        ("mismo incremento de temperatura", r"\Delta T_1=\Delta T_2", "Las dos se calientan lo mismo."),
    ),
    "O26-4": (
        ("Un mol", r"n=1\ \text{mol}", "Un mol: los moles no cambian."),
        ("gas ideal", r"pV=nRT", "Gas ideal: vale la casilla del gas ideal."),
        ("temperatura de", r"T_1=25\ ^\circ\text{C}=298{,}15\ \text{K}",
         "La temperatura inicial está en grados Celsius: hay que pasarla a kelvin."),
        ("volumen se mantiene constante", r"V_1=V_2", "Volumen constante: en el gas ideal, la presión y la temperatura van juntas."),
        ("presión se duplica", r"p_2=2p_1", "La presión se duplica."),
    ),
    "P1O26-2": (
        ("fuente puntual", r"I=\dfrac{P}{4\pi r^2}",
         "Fuente puntual en todas direcciones: la potencia se reparte en una esfera. Es la casilla nueva de la hoja."),
        ("7,5 m", r"r_1=7{,}5\ \text{m}", "Distancia donde se conoce la intensidad."),
        ("0,11 W/m2", r"I_1=0{,}11\ \text{W/m}^2", "Intensidad a esa distancia."),
        ("1 W/m2", r"I_2=1\ \text{W/m}^2", "Umbral de dolor: la intensidad que se quiere alcanzar."),
    ),
    "P1O26-4": (
        ("misma cuerda", r"v=\sqrt{F/\mu}", "Misma cuerda: misma tensión y misma [[μ]], así que la misma rapidez."),
        ("misma frecuencia", r"v=\lambda f", "La afirmación uno habla de ondas con la misma frecuencia."),
        ("distinta amplitud", r"A\ \text{no está en}\ v", "La dos dice que la amplitud puede cambiar: la amplitud no aparece en la rapidez."),
    ),
    "P1E26-2": (
        ("se propaga hacia la dirección", r"y=A\cos(kx-\omega t)",
         "Viaja hacia las [[x]] positivas: tiene la forma de la casilla de la onda viajera."),
        ("0,025", r"A=0{,}025\ \text{m}", "Amplitud. No hace falta para la longitud de onda ni para la rapidez."),
        ("", r"k=4{,}0\ \text{rad/m}\qquad \omega=50\ \text{rad/s}",
         "El número que multiplica a [[x]] es [[k]], y el que multiplica a [[t]] es [[ω]]."),
        ("están en metros", r"\text{todo en el SI}", "Todo está en el SI: los resultados salen en metros y en metros por segundo."),
    ),
    "P1E26-3": (
        ("cuerdas idénticas", r"\mu_1=\mu_2", "Cuerdas idénticas: la misma [[μ]]."),
        ("misma tensión", r"F_1=F_2", "La misma tensión."),
        ("doble de frecuencia", r"\omega_1=2\omega_2", "La onda uno tiene el doble de frecuencia, así que el doble de [[ω]]."),
        ("mitad de amplitud", r"A_1=\tfrac12A_2", "Y la mitad de amplitud."),
    ),
    "P1E26-5": (
        ("igual forma y amplitud", r"|y_1|=|y_2|", "Los dos pulsos tienen la misma forma y el mismo tamaño."),
        ("direcciones opuestas", r"\leftarrow\quad\rightarrow", "Viajan en sentidos contrarios: se van a cruzar."),
        ("desplazamiento hacia arriba", r"y_1=+y", "Uno sube la cuerda."),
        ("desplazamiento hacia abajo", r"y_2=-y", "El otro la baja."),
    ),
    "RP-26I-1": (
        ("1,056 m", r"L=1{,}056\ \text{m}", "Largo de la cuerda: da [[μ]] y la longitud de onda de la fundamental."),
        ("0,0337 kg", r"m=0{,}0337\ \text{kg}", "Masa de la cuerda: [[μ]] es masa entre largo."),
        ("280 N", r"F=280\ \text{N}", "Tensión: con [[μ]] da la rapidez de la onda."),
        ("frecuencia fundamental", r"n=1", "Fundamental es el primer armónico: [[n]] igual a uno."),
    ),
    # ---------------------------------------------------------------- simulacros y el posible cuarto problema
    "P1E26-9": (
        ("48,0 g", r"m=48{,}0\ \text{g}=0{,}0480\ \text{kg}", "Masa de la cuerda: con el largo da [[μ]]."),
        ("dos extremos", r"\text{nodos en } x=0 \text{ y } x=L",
         "Fija por los dos extremos: ahí hay nodos, y valen las casillas de los armónicos."),
        ("4 antinodos", r"n=4", "Con extremos fijos, el número de antinodos es el número del armónico."),
        ("24,0 cm", r"\tfrac{\lambda}{2}=0{,}240\ \text{m}", "De un nodo al siguiente hay media longitud de onda."),
        ("96,0 N", r"F=96{,}0\ \text{N}", "Tensión de la cuerda: con [[μ]] da la rapidez de la onda."),
        ("1,50 cm", r"A_{OE}=0{,}0150\ \text{m}", "Amplitud en los antinodos: es la amplitud del movimiento de esos puntos."),
        ("44 %", r"F'=1{,}44\,F", "La tensión sube un cuarenta y cuatro por ciento: se multiplica por uno coma cuarenta y cuatro."),
    ),
    "P2O26-9": (
        ("0,860 cm", r"d_1=0{,}860\ \text{cm}\ \Rightarrow\ r_1=4{,}30\ \text{mm}",
         "Diámetro de la tubería principal. Poiseuille usa el radio, a la cuarta."),
        ("6,00 m", r"L_1=6{,}00\ \text{m}", "Largo de la tubería principal."),
        ("0,890 cm", r"d_2=0{,}890\ \text{cm}", "Diámetro de la tubería secundaria."),
        ("10,00 m", r"L_2=10{,}00\ \text{m}", "Largo de la secundaria."),
        ("0,200 m/s", r"v_1=0{,}200\ \text{m/s}", "Rapidez media en la principal: da el caudal y el número de Reynolds."),
        ("0,00100 Pa", r"\eta=1{,}00\times10^{-3}\ \text{Pa}\cdot\text{s}", "Viscosidad del agua: aparece en Reynolds y en Poiseuille."),
        ("misma altura", r"\Delta p_1=\Delta p_2",
         "Las dos salidas están a la misma altura, al aire y después de la misma bomba: tienen la misma caída de presión."),
    ),
    "O26-9": (
        ("moles de un gas ideal", r"n=10{,}0\ \text{mol}", "Diez moles de gas ideal. Se usan al final para comprobar."),
        ("proceso cíclico", r"\textstyle\sum\Delta U=0",
         "Es un ciclo: el gas vuelve a A, así que la suma de los cambios de energía interna es cero."),
        ("es adiabático", r"Q_{AB}=0", "De A a B no entra ni sale calor."),
        ("entrando al sistema", r"Q_{BC}=+328{,}5\ \text{kJ}", "De B a C entra calor: va con signo más."),
        ("proceso isotérmico", r"\Delta U_{CD}=0", "De C a D la temperatura no cambia: su cambio de energía interna es cero."),
        ("saliendo del sistema", r"Q_{DA}=-354{,}6\ \text{kJ}", "De D a A sale calor: va con signo menos. Es el tramo que más sirve."),
    ),
    "Q050": (
        ("4,00 m2", r"A=4{,}00\ \text{m}^2", "Área de la lámina: es la misma en todas las capas."),
        ("2,00 cm de grosor", r"d=0{,}0200\ \text{m}", "Grosor: los límites de la integral van de cero a este valor, en metros."),
        ("300 °C", r"T_1=300\ ^\circ\text{C}\ \text{en}\ y=0", "Temperatura de la cara de abajo, la caliente."),
        ("100 °C", r"T_2=100\ ^\circ\text{C}\ \text{en}\ y=d", "Temperatura de la cara de arriba."),
        ("200 + 1000", r"k(y)=200+1000\,y", "La conductividad cambia con la altura: por eso hay que integrar."),
        ("5 minutos", r"t=300\ \text{s}", "Tiempo para la energía: trescientos segundos."),
        ("mitad del grosor", r"y=\tfrac d2=0{,}0100\ \text{m}", "Punto donde piden la temperatura."),
    ),
    "P2E26-9": (
        ("850 kg/m3", r"\rho=850\ \text{kg/m}^3", "Densidad del aceite: entra en la velocidad terminal, en la diferencia de densidades."),
        ("5,00 m", r"L=5{,}00\ \text{m}", "Largo del tubo: va en Poiseuille."),
        ("2,00 mm", r"R=2{,}00\times10^{-3}\ \text{m}", "Radio del tubo. En Poiseuille va a la cuarta: hay que pasarlo a metros."),
        ("1,50", r"\tfrac{dV}{dt}=1{,}50\times10^{-5}\ \text{m}^3\text{/s}", "Caudal que pasa por el tubo."),
        ("180 kPa", r"\Delta p=1{,}80\times10^5\ \text{Pa}", "Diferencia de presión entre los extremos: con el caudal da la viscosidad."),
        ("1,00 mm", r"r=1{,}00\times10^{-3}\ \text{m}", "Distancia al eje donde piden la velocidad."),
        ("0,500 mm", r"r_e=5{,}00\times10^{-4}\ \text{m}", "Radio de la esfera de teflón."),
        ("2200 kg/m3", r"\rho_e=2200\ \text{kg/m}^3", "Densidad del teflón: mayor que la del aceite, por eso la esfera baja."),
    ),
    "P1E26-7": (
        ("misma longitud", r"L_1=L_2", "Las dos cuerdas tienen el mismo largo. Para la rapidez no importa."),
        ("tensión 4F", r"F_2=4F", "La cuerda dos tiene cuatro veces la tensión."),
        ("densidad lineal 2μ", r"\mu_2=2\mu", "Y el doble de densidad lineal."),
    ),
    "P1O26-5": (
        ("amplitud A", r"E=\tfrac12kA^2", "Con la amplitud se escribe la energía total."),
        ("posición x", r"x=\tfrac A2", "La masa está en la mitad de la amplitud."),
        ("razón entre energía cinética y la energía total", r"\dfrac{K}{E}=\;?",
         "Piden la cinética entre la total, no la cinética entre la potencial."),
    ),
    "P2O26-1": (
        ("recupera su forma", r"\sigma:\ \text{zona elástica}", "Con el primer esfuerzo el material vuelve a su forma: está en la zona elástica."),
        ("deformación 3ε", r"2\sigma\ \to\ 3\varepsilon", "Con el doble de esfuerzo se deforma el triple: ya no es proporcional."),
        ("no recupera completamente", r"2\sigma:\ \text{pasó el límite}", "Con el doble de esfuerzo queda deformado: pasó el límite elástico."),
    ),
    "P2E26-4": (
        ("sección constante", r"v\ \text{igual en todo el tubo}",
         "Sección constante: la rapidez es la misma en todo el tubo, así que no explica ningún cambio de altura."),
        ("viscosidad", r"\eta\neq0", "Hay viscosidad: el aceite roza con las paredes y pierde presión al avanzar."),
        ("régimen laminar", r"\text{vale Poiseuille}", "Laminar: vale la casilla de Poiseuille."),
        ("funcionan como manómetros", r"p=p_{atm}+\rho g h", "La altura en cada tubo mide la presión de ese punto."),
    ),
    "O26-1": (
        ("capacidad calorífica molar a presión constante", r"C_p", "Es [[C_p]]: calor por mol y por grado."),
        ("valor reportado es", r"C_p=16{,}18\ \tfrac{\text{J}}{\text{mol}\cdot{}^\circ\text{F}}",
         "El valor está por grado Fahrenheit: el grado está abajo, en el denominador."),
        ("Sistema Internacional", r"\tfrac{\text{J}}{\text{mol}\cdot\text{K}}", "Lo piden por kelvin."),
        ("nitrógeno", r"\text{N}_2\ \text{diatómico}", "El nitrógeno es diatómico: sirve para comprobar el resultado con la hoja."),
    ),
    "O26-5": (
        ("varias capas de diferentes materiales", r"k_1\neq k_2\neq\cdots",
         "Capas de distintos materiales: cada una tiene su propia conductividad."),
        ("distintas temperaturas", r"T_H>T_C", "Un lado más caliente que el otro: el calor cruza todas las capas."),
        ("régimen estacionario", r"H\ \text{igual en todas las capas}",
         "Estacionario: ninguna capa guarda calor, así que el mismo flujo atraviesa todas."),
    ),
    "P1O26-9": (
        ("0,900 m", r"L_1=0{,}900\ \text{m}", "Largo inicial de la cuerda."),
        ("12,15 g", r"m=12{,}15\ \text{g}=0{,}01215\ \text{kg}", "Masa de la cuerda: con el largo da [[μ]]."),
        ("180 Hz", r"f_1=180\ \text{Hz}", "Frecuencia de la fundamental antes de presionar."),
        ("4,00 %", r"F_2=1{,}04\,F_1", "La tensión sube un cuatro por ciento: se multiplica por uno coma cero cuatro."),
        ("prácticamente constante", r"\mu_2=\mu_1", "[[μ]] no cambia: en el cociente de rapideces se cancela."),
        ("842,4 Hz", r"f_3'=842{,}4\ \text{Hz}", "Frecuencia del tercer armónico después de presionar."),
    ),
    "SUF26-4": (
        ("incompresible", r"\tfrac{dV}{dt}=\text{constante}", "Incompresible: el caudal es el mismo en toda la tubería."),
        ("1180 kg/m3", r"\rho=1180\ \text{kg/m}^3", "Densidad: pasa de caudal a flujo de masa y entra en Bernoulli."),
        ("4,00 cm", r"D_1=4{,}00\ \text{cm}\ \Rightarrow\ r_1=0{,}0200\ \text{m}", "Diámetro arriba: el radio es la mitad."),
        ("2,52 m/s", r"v_1=2{,}52\ \text{m/s}", "Rapidez arriba."),
        ("desciende 2,50 m", r"y_1-y_2=2{,}50\ \text{m}", "La tubería baja dos coma cincuenta metros."),
        ("aumenta 10133 Pa", r"p_2-p_1=10\,133\ \text{Pa}", "Abajo la presión es mayor: esta diferencia entra en Bernoulli."),
    ),
    "Q107": (
        ("gas ideal diatómico", r"C_V=\tfrac52R\qquad C_p=\tfrac72R",
         "Diatómico: la hoja da cinco medios de [[R]] a volumen constante y siete medios a presión constante."),
        ("únicamente en términos de", r"p_0,\ V_0", "Todo se deja con letras: no hay números."),
        ("", r"1(V_0,p_0)\quad 2(V_0,2p_0)\quad 3(2V_0,2p_0)\quad 4(2V_0,p_0)",
         "La figura da los cuatro puntos en términos de [[p_0]] y [[V_0]]."),
        ("eficiencia térmica de la máquina", r"e=\dfrac{W_{neto}}{Q_H}", "Es una máquina: al final se pide la eficiencia."),
    ),
    # ---------------------------------------------------------------- ruta de integrales y pregunta 7 del III parcial
    "O26-7": (
        ("ciclo Otto", r"e_{\text{otto}}=1-\frac{1}{r^{\gamma-1}}",
         "Ciclo Otto: su eficiencia está en la hoja y solo depende de la razón de compresión y de [[γ]]."),
        ("gas diatómico", r"\gamma_A=1{,}4", "El motor A usa un gas diatómico: [[γ]] igual a uno coma cuatro."),
        ("gas monoatómico", r"\gamma_B=1{,}67", "El motor B usa un gas monoatómico: [[γ]] igual a uno coma sesenta y siete."),
        ("misma razón de compresión", r"r_A=r_B=r", "La razón de compresión es la misma: lo único distinto es [[γ]]."),
    ),
    "E2-8": (
        ("850 kg/m3", r"\rho_1=850\ \text{kg/m}^3", "Densidad del aceite, la capa de arriba."),
        ("profundidad de 0,30 m", r"h_{ac}=0{,}30\ \text{m}",
         "El aceite tiene treinta centímetros. Su peso aprieta toda la puerta por igual."),
        ("1000 kg/m3", r"\rho_2=1000\ \text{kg/m}^3", "Densidad del agua, donde está la puerta."),
        ("radio R", r"R=0{,}30\ \text{m}",
         "Radio del semicírculo. También es lo más hondo que llega la puerta, porque el arco baja desde la interfaz."),
        ("borde recto horizontal", r"y=0\ \text{en la interfaz}",
         "El borde recto está en la interfaz: ahí empieza la puerta y desde ahí se mide la profundidad en el agua."),
        ("arco curvo", r"\text{ancho}(y)=2\sqrt{R^2-y^2}",
         "El arco va hacia abajo: el ancho de cada franja sale de la ecuación del círculo."),
    ),
    "Q045": (
        ("flujo radial de calor", r"\text{cascarón: }A=2\pi r L",
         "El calor sale del centro hacia afuera: el pedacito es un cascarón cilíndrico."),
        ("235", r"k=235\ \tfrac{\text{W}}{\text{m}\cdot\text{K}}", "Conductividad del aluminio."),
        ("5,00 m", r"L=5{,}00\ \text{m}", "Largo del tubo: va en el área del cascarón."),
        ("radio interno de 4,00 cm", r"r_1=4{,}00\ \text{cm}", "Radio de adentro: el límite de abajo de la integral."),
        ("radio externo de 8,00 cm", r"r_2=8{,}00\ \text{cm}", "Radio de afuera: el límite de arriba."),
        ("150 °C", r"T_1=150\ ^\circ\text{C}", "Temperatura de adentro, la caliente."),
        ("45 °C", r"T_2=45\ ^\circ\text{C}", "Temperatura de afuera."),
        ("estado estacionario", r"H\ \text{igual en cada cascarón}",
         "Estacionario: el mismo calor por segundo cruza todos los cascarones."),
        ("10,0 min", r"t=600\ \text{s}", "Tiempo para la energía del inciso B."),
        ("6,00 cm", r"r=6{,}00\ \text{cm}", "Radio donde piden la temperatura."),
    ),
}


# ============================================================================== porqué por lección

PORQUE: Dict[str, Dict[str, str]] = {
    "Q110": {
        "A y B) Tabla de estados":
            "Todo lo demás ($W$, $Q$, $\\Delta U$) se calcula con $p$, $V$ y $T$ de cada punto, por eso la tabla va "
            "primero. A sale del gas ideal con sus datos; B sale de la adiabática $pV^\\gamma$ = constante, porque el "
            "enunciado dice «compresión adiabática» y da $p_B = 5p_A$; C tiene el volumen de B (isocórico) y la presión "
            "de A (isobárico).",
        "D) Coeficiente de rendimiento":
            "El enunciado dice «máquina de refrigeración» y el ciclo va en sentido antihorario: se pide "
            "$K = |Q_C|/|W|$. $Q_C$ es el calor que el gas saca del foco frío, el único positivo (el de C→A), y $W$ es "
            "el trabajo neto que hay que meterle.",
    },
    "Q109": {
        "B) Temperaturas de A y C":
            "A y C son los puntos de los que se conocen presión y volumen: A por el enunciado, y C porque tiene la "
            "presión de A y un tercio del volumen. Con esos dos números, $T = pV/(nR)$ da la temperatura directo.",
        "B) Estado B con la adiabática":
            "De B solo se sabe que tiene el volumen de A (isocórico) y que está en la misma adiabática que C. La casilla "
            "$pV^\\gamma$ = constante es la que une dos puntos de una adiabática; por eso se iguala entre B y C para "
            "despejar $p_B$.",
        "B) Temperatura de B":
            "Con $p_B$ recién hallada y $V_B = V_A$, el gas ideal da $T_B$. Hace falta para el $\\Delta U$ de los dos "
            "procesos que tocan B.",
        "D) Eficiencia del motor":
            "El ciclo va en sentido horario, así que es un motor: se pide $e = W_{neto}/Q_H$. $Q_H$ es solo la suma de "
            "los calores positivos (los que entran), no el calor neto; la tabla con los signos evita mezclarlos.",
    },
    "O26-2": {
        "Partir de la casilla de eficiencia":
            "La única casilla de la hoja que une trabajo, calor recibido y eficiencia es $e = W/Q_H$. El 20 % del "
            "enunciado es justo esa $e$, así que se empieza ahí.",
        "Despejar el trabajo W":
            "Las opciones están escritas como «W = …», por eso se despeja $W$: $Q_H$ pasa multiplicando y queda "
            "$W = 0{,}20\\,Q_H = Q_H/5$.",
    },
    "Q108": {
        "B) Calor que entra":
            "El calor que entra, $Q_H$, es la suma de los calores positivos. Por eso se revisan los signos de los tres: "
            "solo el de A→B es positivo (el gas se expande y se calienta a presión constante).",
        "C) Eficiencia":
            "El enunciado dice «máquina térmica», así que se usa $e = W_{neto}/Q_H$. $W_{neto}$ es la suma de los tres "
            "trabajos con su signo, que también es el área del triángulo.",
        "D) Entropía de A a B":
            "Este inciso es de entropía (sección 20.7), que ya no entra en la reposición II-2026. Se deja para entender "
            "la idea: $dS = dQ/T$, y a presión constante $dQ = nC_p\\,dT$; por eso sale un logaritmo.",
    },
    "Q106": {
        "a) Trabajo neto":
            "En un ciclo, el trabajo neto es el área encerrada en el diagrama p-V. Aquí el ciclo es un rectángulo, así "
            "que esa área es base por altura: $(V_{max}-V_{min})(p_{alta}-p_{baja})$. No hace falta ir proceso por proceso.",
        "Juntar todo en una tabla":
            "La tabla sirve de control: en un ciclo el gas vuelve al mismo estado, así que la suma de los $\\Delta U$ "
            "tiene que dar cero y la suma de los $Q$ tiene que igualar la de los $W$. Si no da, hay un signo mal.",
        "b) y c) Calor absorbido y liberado":
            "Absorbido es lo que entra: los $Q$ positivos. Liberado es lo que sale: los $Q$ negativos. Se separan "
            "porque la eficiencia usa solo el absorbido.",
        "d) Eficiencia":
            "Es una máquina térmica, así que $e = W_{neto}/Q_{abs}$. Se divide entre el calor absorbido, no entre el "
            "neto ni entre el liberado: ese es el error más común.",
    },
    "Q051": {
        "Aplicar Fourier":
            "Piden la tasa de calor por conducción a través de una pared en régimen estable: esa es la casilla "
            "$H = kA\\,\\Delta T/L$. El área es la cara por donde pasa el calor (alto por ancho), $L$ es el grosor en "
            "metros, y $\\Delta T$ en °C vale lo mismo que en K.",
    },
    "Q020": {
        "Despejar el cambio de temperatura":
            "El bloque solo se calienta, sin fundirse, así que no hay cambio de fase y vale $Q = mc\\,\\Delta T$. Lo "
            "que piden está escondido en $\\Delta T$, por eso se despeja primero.",
        "Temperatura final":
            "$\\Delta T$ dice cuánto subió, no dónde quedó. La temperatura final es la inicial más el cambio: "
            "$T_f = T_i + \\Delta T$.",
    },
    "O26-6": {
        "Escalones cuánticos":
            "La hoja trae dos valores de $C_V$ para un diatómico según qué movimientos participan. Pasar de uno a otro "
            "solo se explica si al calentar se activan la rotación y luego la vibración, que a baja temperatura están "
            "«congeladas»: cada modo que se activa guarda energía y sube $C_V$.",
    },
    "O26-10": {
        "Qué nos dan y regla de etapas":
            "Con hielo bajo cero no se puede plantear de una vez $\\sum Q = 0$ con una sola $T_f$: no se sabe si el "
            "hielo se derrite todo. Por eso primero se compara el calor que pueden ceder el agua y el aluminio hasta "
            "0 °C con el que necesita el hielo para llegar a 0 °C y fundirse.",
        "Paso 1":
            "Es lo máximo que pueden dar el agua y el aluminio sin bajar de 0 °C. Si ese calor no alcanza para fundir "
            "el hielo, la mezcla queda en 0 °C; si sobra, todo se funde y $T_f > 0$.",
        "Paso 2":
            "El hielo pasa por dos etapas con casillas distintas: calentarse como sólido ($mc_{hielo}\\Delta T$) y "
            "fundirse a 0 °C sin cambiar de temperatura ($mL_f$). La suma es lo que necesita para quedar como agua a 0 °C.",
        "Paso 3":
            "Como sobra energía, todo el hielo se funde y la temperatura final queda sobre 0 °C. Ahora sí se plantea "
            "$\\sum Q = 0$ con una sola $T_f$, incluyendo el agua que salió del hielo, que se calienta desde 0 °C.",
    },
    "RP-26I-3": {
        "Equilibrio de flotación":
            "Las esferas flotan quietas, así que en cada fluido el empuje iguala al peso. El peso es el mismo en los "
            "dos casos, entonces los dos empujes son iguales; y el empuje de la hoja, $B = \\rho_{fluido}\\,g\\,V_{hundido}$, "
            "es el que conecta la densidad con lo que se hunde.",
        "Despejar la razón de densidades":
            "Al igualar los dos empujes se cancelan $g$ y el volumen de la esfera, y queda una razón: "
            "$\\rho_A/\\rho_B = 0{,}60/0{,}40 = 1{,}5$. Las opciones están en «% mayor», así que 1,5 se lee como 50 % mayor.",
    },
    "RP-26I-4": {
        "Combinar flujo másico y caudal":
            "El dato viene en kg/s, masa por segundo, pero la rapidez está en la casilla del caudal, $dV/dt = Av$. La "
            "densidad une las dos: $dm/dt = \\rho\\,dV/dt = \\rho A v$, y de ahí se despeja $v$.",
        "Sustituir y comprobar opciones":
            "Al sustituir, $v = (dm/dt)/(\\rho A)$. Las opciones cercanas (13,3 y 12,8) salen de usar mal la densidad: "
            "por eso se hace la cuenta con 1040 y no con 1000.",
    },
    "P2O26-2": {
        "Si flota, el empuje es igual al peso":
            "La pregunta compara dos situaciones en las que el bloque flota quieto. Flotar quieto es fuerza neta cero: "
            "empuje igual al peso. El peso no cambia al cambiar de líquido, así que el empuje tampoco.",
        "Cuánto se hunde en cada líquido":
            "El empuje es igual, pero $B = \\rho_{líq}\\,g\\,V_{hund}$: si la densidad del líquido baja (aceite), el "
            "volumen hundido tiene que subir para dar el mismo empuje. La fracción hundida es $\\rho_{mad}/\\rho_{líq}$.",
    },
    "P2O26-4": {
        "Continuidad":
            "Piden una rapidez en otra sección del mismo tubo y el agua es incompresible: eso es la continuidad de la "
            "hoja, $A_1v_1 = A_2v_2$. No hace falta Bernoulli porque no piden presiones.",
        "La razón de áreas":
            "El área depende del cuadrado del radio. Al dividir dos áreas se cancelan $\\pi$ y el «entre 2», y queda "
            "$(D_1/D_2)^2$. Por eso la mitad de diámetro no duplica la rapidez: la cuadruplica.",
        "Sustituir y elegir":
            "Con $(6{,}0/3{,}0)^2 = 4$, la rapidez en la parte angosta es $4 \\times 2{,}0 = 8{,}0$ m/s. La opción de "
            "4,0 m/s es la trampa de olvidar el cuadrado.",
    },
    "P2E26-1": {
        "Velocidad constante":
            "El enunciado dice «a velocidad constante»: por la segunda ley de Newton eso es aceleración cero, así que "
            "las fuerzas suman cero. Hacia arriba tiran la tensión y el empuje; hacia abajo, el peso.",
        "Despejar la tensión":
            "Las opciones dan $T$ en términos de $F_B$, $m$ y $g$, por eso se despeja $T$ de $T + F_B - mg = 0$. Queda "
            "$T = mg - F_B$: el cable pone solo lo que el empuje no alcanza a sostener.",
    },
    "P2O26-7": {
        "Cuánta presión de más":
            "El cuerpo ya estaba a presión atmosférica: lo que lo comprime es solo la presión que se agrega al bajar. "
            "Por eso se usa $\\Delta p = \\rho g h$ y no la presión absoluta.",
        "Módulo de volumen":
            "La casilla del módulo de volumen, $B = -\\Delta p/(\\Delta V/V_0)$, es la que une presión y cambio de "
            "volumen. Se despeja $\\Delta V = -V_0\\,\\Delta p/B$. Si $V_0$ queda en litros, $\\Delta V$ sale en litros.",
    },
    "P2O26-3": {
        "Las cuatro condiciones":
            "Para decidir qué afirmaciones son hipótesis hace falta la lista completa: incompresible, no viscoso, flujo "
            "estable e irrotacional. Son las que permiten usar Bernoulli de la hoja.",
        "Revisar cada afirmación":
            "Cada afirmación se compara con una condición. La III dice que las partículas pueden girar: eso contradice "
            "«irrotacional», que no permite rotación local. Por eso quedan solo I y II.",
    },
    "RP-26I-8": {
        "A) Caudal":
            "El dato está en masa por segundo y piden volumen por segundo. Como el nivel no cambia, lo que sale es igual "
            "a lo que entra, y la densidad convierte: $dV/dt = (dm/dt)/\\rho$.",
        "B) Velocidad":
            "La casilla del caudal, $dV/dt = Av$, une el caudal del inciso A con la rapidez. El área del orificio es "
            "circular, $A = \\pi r^2$, con el radio (la mitad del diámetro).",
        "C) Profundidad":
            "Piden una profundidad y ya se conoce la rapidez de salida: Bernoulli entre la superficie y el chorro las "
            "une. Se tacha lo que es igual: presión atmosférica en los dos puntos, y rapidez casi cero arriba porque el "
            "tanque es grande. Queda $v^2 = 2gh$.",
    },
    "RP-24II-2": {
        "Conversión del caudal y radios":
            "Las casillas trabajan en el SI: el caudal tiene que estar en m³/s y el área se calcula con el radio, no con "
            "el diámetro. Convertir primero evita errores de potencias de diez.",
        "A) Rapidez":
            "El caudal es el mismo en todo el tubo (el agua es incompresible), así que $v = (dV/dt)/A$ en cada sección. "
            "Como $D_b$ es la mitad de $D_a$, el área es la cuarta parte y la rapidez es cuatro veces mayor.",
        "B) Diferencia de presiones":
            "Piden $p_a - p_b$ y ya se tienen las rapideces: eso es Bernoulli entre a y b. El tubo es horizontal, los "
            "términos $\\rho g y$ se cancelan, y la diferencia de presiones queda solo por la diferencia de rapideces.",
        "C) Diferencia de alturas":
            "Cada tubo vertical abierto mide la presión de su punto: $p = p_{atm} + \\rho g h$. Al restar los dos, "
            "$p_{atm}$ se cancela y queda $p_a - p_b = \\rho g (h_a - h_b)$; con la diferencia de presiones de B se "
            "despeja la de alturas.",
    },
    "P2O26-5": {
        "Rapidez: continuidad":
            "Piden comparar rapideces en dos secciones del mismo tubo: eso lo decide la continuidad, $A_1v_1 = A_2v_2$. "
            "Si el área baja, la rapidez sube.",
        "Presión: Bernoulli":
            "Para comparar presiones hace falta Bernoulli. Como la tubería es horizontal, las alturas se cancelan y "
            "queda $p_1 - p_2 = \\tfrac12\\rho(v_2^2 - v_1^2)$, que es positivo porque $v_2 > v_1$: más rapidez, menos presión.",
    },
    "P2E26-3": {
        "Bernoulli entre la superficie y el orificio":
            "Torricelli no está en la hoja, así que hay que sacarla de Bernoulli. Se eligen los dos puntos donde se sabe "
            "más: la superficie (presión atmosférica y rapidez casi cero por el dato de las áreas) y el orificio "
            "(presión atmosférica).",
        "Tachar y despejar":
            "Lo que es igual en los dos puntos se tacha: la presión atmosférica. La densidad está en todos los términos "
            "y también se va. Queda $v = \\sqrt{2gh}$: solo depende de la profundidad, la opción D.",
    },
    "P2E26-6": {
        "Bernoulli entre la corriente y la punta":
            "El tubo de Pitot funciona con Bernoulli: en la punta el aire se detiene ($v = 0$) y su presión sube a la "
            "de remanso. Las dos posiciones están a la misma altura, así que quedan solo presión y rapidez: "
            "$p + \\tfrac12\\rho v^2 = p_0$.",
        "Despejar la rapidez":
            "Lo único desconocido es $v$, así que se despeja: $v = \\sqrt{2(p_0 - p)/\\rho}$. La diferencia de presiones "
            "es chica (600 Pa), pero el aire es muy liviano; por eso la rapidez sale grande.",
    },
    "P2E26-7": {
        "Rapideces: continuidad":
            "Para comparar presiones con Bernoulli primero hay que saber qué rapidez es mayor. La continuidad lo dice: "
            "A está en la parte angosta, así que $v_A > v_B$.",
        "Bernoulli con B como altura cero":
            "Se pone la altura cero en B para que quede un término menos. Al despejar $p_B$ aparecen dos términos "
            "positivos que se suman a $p_A$: el de la altura ($\\rho g h$, A está más arriba) y el de la rapidez (A va "
            "más rápido). Por eso $p_B > p_A$, y la D queda incompleta.",
    },
    "P2E26-5": {
        "Dónde va más rápido":
            "Bernoulli compara presiones a partir de rapideces, así que primero hay que saber dónde va más rápido. "
            "Por continuidad, en la parte angosta.",
        "Bernoulli: más rapidez, menos presión":
            "Los dos puntos están a la misma altura, así que Bernoulli queda presión más término de rapidez igual a una "
            "constante: donde va más rápido, la presión es menor. Cada tubo vertical sube hasta que $\\rho g$ por su "
            "altura iguala esa presión, así que el tubo del lado ancho sube más: $H > L$.",
    },
    # ---------------------------------------------------------------- integrales desde cero (teoría)
    "T-INT0": {
        "1. El problema":
            "Es el mismo problema de las compuertas y represas de las reposiciones: la casilla $F = pA$ solo sirve si "
            "la presión es una sola en toda el área. En una pared la presión crece con la profundidad, así que hace "
            "falta otra forma.",
        "2. La idea":
            "Si la franja es delgada, todos sus puntos están casi a la misma profundidad, y entonces sí se puede usar "
            "$F = pA$ en cada franja. Las franjas son horizontales porque la presión solo depende de la profundidad.",
        "3. Sumar 4 franjas":
            "Con pocas franjas se ve el método con números. Cada franja usa la presión de su borde de arriba, que es "
            "un poco menor que la real dentro de la franja; por eso la suma se queda corta.",
        "4. Franjas más finas":
            "Al hacer las franjas más delgadas, el error de usar una sola presión por franja se achica. La suma se "
            "acerca a un número fijo: ese número es la fuerza exacta.",
        "5. Eso es la integral":
            "La integral es el nombre y el símbolo de esa suma con franjas cada vez más finas. Por eso $F = \\int p\\,dA$ "
            "de la hoja se lee: sumar presión por área de cada franja, desde arriba hasta abajo.",
    },
    "T-INT1": {
        "1. La regla de la potencia":
            "En las paredes y compuertas todo lo que se integra son potencias de $h$ o de $y$ ($h$, $h^2$, raíz de $y$). "
            "Con esta sola regla alcanzan todos los problemas que salieron en las reposiciones.",
        "2. Más ejemplos":
            "La raíz aparece en la compuerta con parábola, donde el ancho es $\\sqrt{y/2}$. Escribirla como potencia "
            "$y^{1/2}$ deja usar la misma regla, sin aprender otra.",
        "3. Los números fijos salen afuera":
            "Un número fijo multiplica a todas las franjas por igual, así que sale de la suma y multiplica al final. "
            "Dentro de la integral queda solo lo que cambia con la profundidad.",
        "4. Con límites":
            "La integral con límites da un número: el resultado en el límite de arriba menos el resultado en el de "
            "abajo. Los límites dicen de dónde a dónde va la pared.",
        "5. La pared de la lección anterior":
            "Se usa la pared de la lección anterior para comprobar que la regla da el mismo número al que se acercaba "
            "la suma de franjas: la integral y la suma son lo mismo.",
    },
    "T-PARED": {
        "La receta de 5 pasos":
            "Las paredes de las reposiciones (rectángulo, trapecio, parábola) solo se diferencian en el ancho de la "
            "franja. Con la receta fija no hay que pensar desde cero: cambia un renglón y el resto se repite.",
        "Trapecio, paso 2":
            "El ancho del trapecio cambia en línea recta con la profundidad, de 45 m arriba a 30 m abajo. Por eso se "
            "escribe como una recta, $L(h) = 45{,}0 - 0{,}30h$, que da 45 en $h = 0$ y 30 en $h = 50$.",
        "Trapecio, pasos 3 y 4":
            "Del otro lado de la pared hay aire: la atmosférica empuja igual por los dos lados y se cancela, y queda "
            "$p = \\rho g h$. Se multiplica $h$ por el paréntesis para tener potencias sueltas, que es lo que integra la regla.",
        "Trapecio, paso 5":
            "Cada potencia se integra con la regla y se evalúa de 0 a 50. En $h = 0$ todo vale cero, así que solo queda "
            "el valor en el fondo.",
        "Parábola: el ancho sale de la curva":
            "En la parábola el ancho de la franja no es fijo: es la $x$ de la curva a esa altura. Por eso se despeja $x$ "
            "de $y = 2x^2$. La profundidad es $4 - y$ porque $y$ se mide desde el fondo y el agua llega a 4 m.",
        "Parábola: potencias sueltas":
            "El producto $(4 - y)\\sqrt{y/2}$ se abre en potencias sueltas ($y^{1/2}$ y $y^{3/2}$) para usar la regla de "
            "la potencia en cada una.",
    },
    # ---------------------------------------------------------------- Parcial 2: paredes y manómetros
    "P2E26-8": {
        "Dibujo y medidas":
            "Las alturas vienen medidas desde el fondo, pero la presión depende de la profundidad desde la superficie. "
            "Poner todo en el mismo eje ($y$ desde el fondo) evita confundir altura con profundidad: profundidad "
            "$= 5{,}45 - y$.",
        "a) Presión manométrica en el fondo":
            "Manométrica quiere decir sin contar la atmosférica, así que la casilla queda $p = \\rho g h$. En el fondo "
            "la profundidad es toda la columna: 5,45 m.",
        "b) Presión en el centro":
            "La presión cambia con la profundidad, así que hay que ubicar el centro: medio lado (0,15 m) por debajo del "
            "borde de arriba, en $y = 3{,}30$ m. Su profundidad es $5{,}45 - 3{,}30$.",
        "c) Una franja horizontal":
            "En la ventana la presión no es una sola (abajo es mayor), así que $F = pA$ no sirve directo. En una franja "
            "horizontal delgada sí: todos sus puntos están a la misma profundidad. Su área es el ancho por $dy$.",
        "c) Sumar todas las franjas":
            "La fuerza total es la suma de las fuerzas de todas las franjas, de borde a borde de la ventana ($y$ de 3,15 "
            "a 3,45): esa suma es la integral. $\\rho$, $g$ y el ancho no cambian y salen afuera.",
        "c) Integrar 5,45":
            "Dentro de la integral queda $5{,}45 - y$: un número y una potencia de $y$. Cada uno se integra con la regla "
            "de la potencia.",
        "c) Arriba menos abajo":
            "La integral con límites es el resultado evaluado en el borde de arriba (3,45) menos el resultado en el de "
            "abajo (3,15).",
        "Comprobación rápida":
            "Como la presión crece en línea recta con la profundidad, el promedio sobre la ventana es la presión de su "
            "centro. Por eso $p_{centro}\\,A$ tiene que dar lo mismo que la integral; si no, hay un error.",
        "d) Ventana circular":
            "Piden solo argumentar. Con el centro a la misma profundidad, la presión media es la misma; entonces la "
            "fuerza depende del área, y un círculo de 30 cm de diámetro tiene menos área que un cuadrado de 30 cm de lado.",
    },
    "RP-24I-2": {
        "Planteo":
            "La presión cambia con la profundidad, así que hay que trabajar con franjas horizontales. Se pone $y$ hacia "
            "arriba desde el fondo porque la parábola $y = 2x^2$ está escrita así; entonces la profundidad de una franja "
            "es $H - y$.",
        "A) Diferencial de fuerza":
            "La casilla $dF = p\\,dA$ se aplica a una franja: $p = \\rho g(H - y)$ y su área es ancho por alto, "
            "$dA = (L/2)\\,dy$. En la mitad rectangular el ancho no cambia.",
        "B) Diferencial de fuerza":
            "Es la misma casilla $dF = p\\,dA$, pero ahora el ancho de la franja es la $x$ de la parábola a esa altura: "
            "$x = \\sqrt{y/2}$. Por eso se despeja $x$ de la curva.",
        "C) Fuerza en la mitad rectangular":
            "Para la fuerza total se suman todas las franjas, de $y = 0$ a $y = H$: eso es integrar el $dF$ del inciso A. "
            "Lo fijo ($\\rho$, $g$, $L/2$) sale afuera.",
        "C) Fuerza en la mitad parabólica":
            "Se integra el $dF$ del inciso B de 0 a 4 m. La raíz se escribe como potencia y el producto se abre en "
            "potencias sueltas, para usar la regla de la potencia.",
        "C) Fuerza total":
            "Las dos mitades empujan en la misma dirección, perpendicular a la compuerta, así que la fuerza total es la "
            "suma de las dos.",
    },
    "RP-25II-2": {
        "A) Ancho lineal":
            "La pared es un trapecio: el ancho de la franja cambia con la profundidad. Como cambia en línea recta entre "
            "45 m arriba y 30 m abajo, se escribe $L(h) = 45{,}0 - 0{,}30h$. Y como afuera hay aire, la atmosférica se "
            "cancela: $p = \\rho g h$.",
        "A) La fuerza de una franja":
            "En una franja horizontal la profundidad es una sola, así que $dF = p\\,dA = \\rho g h\\,L(h)\\,dh$. Se "
            "multiplica $h$ por el paréntesis para dejar potencias sueltas.",
        "A) Sumar las franjas":
            "La fuerza total es la suma de todas las franjas, de la superficie ($h = 0$) al fondo ($h = 50$): la integral. "
            "Cada potencia se integra con la regla y se evalúa arriba menos abajo.",
        "B) Rapidez de salida":
            "Bernoulli entre la superficie del embalse y la salida final, porque ahí se sabe todo menos $v_2$: los dos "
            "puntos están a presión atmosférica y la superficie casi no se mueve. La caída total es 50 + 8 = 58 m.",
        "C) Rapidez v1 y caudal":
            "$v_1$ está en otra sección de la misma tubería: eso es continuidad, $A_1v_1 = A_2v_2$. Con áreas circulares "
            "se cancela $\\pi/4$ y queda la razón de diámetros al cuadrado. El caudal sale de $Av$ en cualquiera de las "
            "dos secciones.",
    },
    "SUF26-3": {
        "A) Altura de cada capa":
            "Los volúmenes vienen dados y el depósito es un cilindro, así que cada capa es un cilindro más bajo: "
            "$V = A_{base}\\,h$. Al despejar $h$ sale la altura de cada líquido.",
        "B) Presión en la interfaz":
            "Piden presión absoluta: se parte de la atmosférica en la superficie del aceite y se baja por toda la capa de "
            "aceite, $p = p_{atm} + \\rho_{ac} g h_{ac}$. Las densidades tienen que estar en kg/m³.",
        "C) Presión manométrica en el fondo":
            "Afuera de la compuerta hay aire: la atmosférica empuja igual por los dos lados y se cancela. Por eso la "
            "fuerza neta se calcula con presiones manométricas. En el fondo pesan las dos capas.",
        "C) Una franja de la compuerta":
            "La compuerta está toda en el agua, pero la presión cambia con la altura: hay que usar franjas. Con $y$ "
            "desde el fondo, la presión de una franja es la del fondo menos $\\rho_{agua} g y$ (subir resta). Su área es "
            "10 cm por $dy$.",
        "C) Sumar las franjas":
            "La fuerza neta es la suma de las franjas de $y = 0$ a $y = 0{,}40$ m: la integral. Como la presión cambia en "
            "línea recta, la presión del centro por el área tiene que dar lo mismo, y sirve de control.",
        "D) Rapidez de salida":
            "Bernoulli vale dentro de un solo líquido, por eso se usa del lado del agua: dentro, la presión del agua en "
            "el centro de la compuerta (ya calculada) con el agua quieta; afuera, el chorro al aire. Queda "
            "$v = \\sqrt{2p_{man}/\\rho_{agua}}$.",
    },
    "P2O26-8": {
        "La regla de oro":
            "En un líquido en reposo, la presión solo depende de la altura: bajar suma $\\rho g\\,\\Delta h$ y subir resta. "
            "En una interfaz los dos líquidos tienen la misma presión, así que se puede caminar de interfaz en interfaz. "
            "Se usan manométricas porque el camino termina en una superficie abierta, donde valen cero.",
        "a) Interfaz agua-mercurio":
            "Se parte de A, el único punto con presión conocida, y se baja dentro del agua hasta tocar el mercurio. Bajar "
            "suma $\\rho_{agua} g\\,\\Delta h$, con $\\Delta h = 2{,}40 - 0{,}850$.",
        "a) Interfaz mercurio-glicerina":
            "En la interfaz agua-mercurio los dos tienen la misma presión, así que se sigue por el mercurio. Para llegar "
            "a la glicerina hay que subir, y subir resta $\\rho_{Hg} g\\,\\Delta h$.",
        "b) Interfaz glicerina-X":
            "En la glicerina el camino sube y baja, pero la presión solo depende de la altura final: se usa la diferencia "
            "entre 1,35 m y 0,700 m. Bajar suma.",
        "b) Columna de X":
            "La densidad de X es la única incógnita que queda. Se sube por X hasta la superficie abierta, donde la "
            "manométrica es cero; eso da una ecuación con $\\rho_X$ sola, que se despeja.",
        "c) Punto B":
            "B está dentro del mercurio, debajo de la interfaz con el agua: se baja desde esa interfaz. Piden absoluta, "
            "así que al final se suma la atmosférica (casilla de presión manométrica).",
        "d) Punto C":
            "C está en la glicerina, por encima de la interfaz con el mercurio: se parte de esa interfaz y subir resta. "
            "Piden manométrica, así que no se suma la atmosférica.",
    },
    "O26-8": {
        "Paso 2":
            "$H = kA\\,\\Delta T/L$ solo sirve si el área es la misma en todo el camino del calor. Aquí el área de cada "
            "rebanada cambia con $y$, así que se usa la forma diferencial de la hoja, $H = -kA\\,dT/dy$, que vale en "
            "cada rebanada.",
        "Paso 3":
            "$H$ es el mismo en todas las rebanadas (estado estacionario y lados aislados), así que puede quedar afuera. "
            "Se separa: lo que depende de $y$ con $dy$ a un lado y $dT$ al otro, y se integra de una cara a la otra. De "
            "las integrales provistas se elige la de forma $\\frac{1}{a^2-u^2}$, con $a = R$.",
        "Paso 4":
            "Cada lado se evalúa entre sus límites: $y$ de $-L$ a $+L$ (las dos caras) y $T$ de $T_1$ a $T_2$. El valor "
            "absoluto del logaritmo se quita porque $L < R$ deja los cocientes positivos.",
        "Paso 5":
            "Se despeja $H$ porque es lo que piden. El caso del corte delgado sirve de control: si $L$ es mucho menor que "
            "$R$, la pieza es casi un disco de área $\\pi R^2$ y tiene que salir la casilla de una placa, "
            "$H = k\\pi R^2(T_1-T_2)/(2L)$.",
    },
    # ---------------------------------------------------------------- Parcial 1: MAS y péndulos
    "RP-26I-7": {
        "A) Frecuencia angular y período":
            "El período sale de $\\omega$, y $\\omega$ sale de los dos datos del resorte: $\\omega = \\sqrt{k/m}$. Después "
            "$T = 2\\pi/\\omega$. No depende de la amplitud.",
        "B) Fase inicial":
            "$x(t) = A\\cos(\\omega t + \\phi)$ ya tiene $A$ y $\\omega$; falta $\\phi$. En $t = 0$ queda "
            "$x(0) = A\\cos\\phi$, que da dos ángulos. El enunciado dice «velocidad positiva»: como "
            "$v(0) = -\\omega A\\,\\mathrm{sen}\\,\\phi$, el seno tiene que ser negativo, y eso elige el ángulo negativo.",
        "C) Energía mecánica":
            "En el extremo toda la energía es del resorte, así que $E = \\tfrac12 kA^2$ con la amplitud en metros. Es la "
            "casilla más corta: no hace falta la velocidad.",
    },
    "P1O26-8": {
        "a) Leer la gráfica":
            "El problema da casi todo en la gráfica. La energía total es la línea plana (no cambia porque no hay "
            "fricción). Donde $U$ y $K$ se cruzan valen lo mismo, y como suman $E$, cada una es la mitad.",
        "a) Constante del resorte":
            "En el cruce se conocen $x$ y $U$, y la casilla $U = \\tfrac12 kx^2$ los une con $k$. Es la única incógnita, "
            "así que se despeja.",
        "a) Amplitud":
            "En el extremo ($x = A$) el bloque se detiene: $K = 0$ y toda la energía es potencial. Por eso "
            "$E = \\tfrac12 kA^2$, y con $E$ y $k$ se despeja $A$.",
        "b) Frecuencia angular":
            "Con $k$ ya hallada y la masa del enunciado, la casilla del resorte da $\\omega = \\sqrt{k/m}$.",
        "b) Fase: dos ángulos":
            "En $t = 0$, $x(0) = A\\cos\\phi$. Al despejar el coseno salen dos ángulos con el mismo coseno, uno positivo y "
            "otro negativo: la posición sola no alcanza para elegir.",
        "b) El signo de la velocidad":
            "El enunciado dice que se mueve a la izquierda: $v_x(0) < 0$. Como $v_x(0) = -\\omega A\\,\\mathrm{sen}\\,\\phi$, "
            "el seno tiene que ser positivo, y eso deja $\\phi = +\\pi/4$.",
        "c) Velocidad y aceleración":
            "Piden las funciones con números: se toman las casillas $v_x = -\\omega A\\,\\mathrm{sen}(\\omega t + \\phi)$ y "
            "$a_x = -\\omega^2 A\\cos(\\omega t + \\phi)$ y se sustituyen $A$, $\\omega$ y $\\phi$.",
        "d) En t":
            "Se evalúa en el instante dado. El ángulo $\\omega t + \\phi$ está en radianes, así que la calculadora tiene "
            "que estar en radianes; en grados da otro número.",
    },
    "P1E26-8": {
        "a) Inercia de la varilla":
            "El enunciado da la inercia de la varilla respecto a su centro, pero el péndulo gira alrededor del pivote, "
            "en un extremo. El teorema de ejes paralelos de la hoja la traslada: $I = I_{cm} + m d^2$, con $d = 1$ m "
            "(media varilla).",
        "a) Inercia de la esfera y total":
            "La esfera es pequeña, así que es una masa puntual a 2,00 m del pivote: $I = mL^2$. La inercia de B es la "
            "suma de las dos partes, porque giran juntas alrededor del mismo pivote.",
        "b) Período del péndulo simple":
            "A es un péndulo simple (esfera pequeña y alambre sin masa), así que vale $\\omega^2 = g/L$ y "
            "$T = 2\\pi/\\omega$. No depende de la masa.",
        "c) Período del péndulo físico":
            "B no es simple: su masa está repartida. Se usa la casilla del péndulo físico, $\\omega^2 = mgd/I$, con la "
            "masa total, la distancia al centro de masa y la inercia del inciso a.",
        "d) Oscilaciones completas":
            "Cada oscilación dura un período, así que en 300 s caben $300/T$. Piden oscilaciones completas: se toma la "
            "parte entera, sin redondear hacia arriba.",
        "e) Largo del péndulo simple":
            "Mismo período quiere decir misma $\\omega$. Se igualan las dos casillas, $g/L = mgd/I$, y se despeja $L$; la "
            "$g$ se cancela.",
    },
    # ---------------------------------------------------------------- Parcial 1: selecciones de MAS y ondas
    "RP-26I-2": {
        "Obtener T en función de L":
            "Para ver qué pasa con el período hay que tenerlo escrito con $L$ y $m$ a la vista. Juntando $T = 2\\pi/\\omega$ "
            "con $\\omega^2 = g/L$ queda $T = 2\\pi\\sqrt{L/g}$: ahí se ve que la masa no aparece.",
        "Sustituir L":
            "Se compara con una razón: $T'/T = \\sqrt{4L/L} = 2$. Trabajar con la razón evita necesitar números que el "
            "enunciado no da.",
    },
    "P1O26-1": {
        "Qué cambia y qué no":
            "La pregunta se resuelve viendo qué letras de $x(t) = A\\cos(\\omega t + \\phi)$ cambian. Con el mismo período, "
            "$\\omega = 2\\pi/T$ no cambia; solo cambia $A$, que multiplica a toda la función.",
        "La fuerza máxima solo se duplica":
            "La opción C dice «se cuadruplica», así que se revisa con la casilla: la fuerza máxima es $kA$, proporcional a "
            "$A$, no a $A^2$. Con el doble de $A$, el doble de fuerza.",
    },
    "P1O26-3": {
        "El período del péndulo físico":
            "Se juntan las dos casillas para tener $T = 2\\pi\\sqrt{I/(mgd)}$ con todas las letras a la vista. Así se ve "
            "dónde está $d$ (abajo, dentro de la raíz) antes de cambiarla.",
        "Cambiar d por d/4":
            "Con una razón: $m$ e $I$ no cambian y se cancelan, y queda $T'/T = \\sqrt{d/(d/4)} = \\sqrt4 = 2$. Como $d$ está "
            "abajo, achicarla agranda el período.",
    },
    "P1O26-7": {
        "La energía potencial del MAS":
            "El enunciado mide $y$ desde el equilibrio y pide la energía potencial «asociada al movimiento oscilatorio»: "
            "esa es $U = \\tfrac12 k y^2$ de la hoja, que ya incluye el efecto del peso. Por eso no se mira la altura "
            "sobre el suelo.",
        "Dónde es máxima":
            "$U$ depende de $y^2$, así que solo importa el tamaño del desplazamiento, no el signo. Los dos extremos están a "
            "la misma distancia del equilibrio: en 1 y en 3 la energía potencial es la misma y es la máxima.",
    },
    "P1E26-4": {
        "Revisar cada afirmación con la hoja":
            "Es una pregunta de afirmaciones, así que cada una se contrasta con una casilla: $a = -\\omega^2 x$ (en $x = 0$ "
            "la aceleración es cero, no máxima) y $T = 2\\pi\\sqrt{m/k}$ (no tiene la amplitud). Así se descartan A y B "
            "sin adivinar.",
        "La ley de Hooke":
            "La D es la ley de Hooke, $F = -kx$: el tamaño de la fuerza es proporcional al desplazamiento y el signo menos "
            "dice que apunta al revés. La C falla porque al alejarse del equilibrio la velocidad y la aceleración apuntan "
            "en sentidos opuestos.",
    },
    "P1E26-6": {
        "U en A/2":
            "La potencial depende de $x^2$: $U = \\tfrac12 kx^2$. Con $x = A/2$ queda un cuarto de $\\tfrac12 kA^2$, es "
            "decir, un cuarto de la energía total. Comparar con $E$ evita necesitar números.",
        "K es el resto":
            "Sin fricción la energía total se reparte entre $K$ y $U$: $K = E - U = \\tfrac34E$. Dividiendo, $K/U = 3$.",
    },
    "P1E26-1": {
        "El período del péndulo simple":
            "Juntando $T = 2\\pi/\\omega$ con $\\omega^2 = g/L$ queda $T = 2\\pi\\sqrt{L/g}$: el período depende de la raíz "
            "del largo. Eso hay que tenerlo a la vista antes de cambiar $L$.",
        "Cambiar L por L/4":
            "Con una razón: $T'/T = \\sqrt{(L/4)/L} = \\tfrac12$. Como $L$ está arriba dentro de la raíz, achicarla a la "
            "cuarta parte achica el período a la mitad.",
    },
    "P1O26-6": {
        "La longitud de onda del segundo armónico":
            "Para contar nodos hay que saber cuántas medias ondas caben en la cuerda. La casilla $\\lambda_n = 2L/n$ con "
            "$n = 2$ da $\\lambda = L$: cabe una onda completa, o sea dos medias ondas.",
        "Contar los nodos":
            "Los nodos están cada media longitud de onda, empezando en un extremo fijo y terminando en el otro. Con dos "
            "medias ondas hay nodos en 0, en $L/2$ y en $L$: tres. En general, $n + 1$.",
    },
    "P2O26-6": {
        "Qué fuerzas actúan":
            "Para relacionar fuerzas hay que tenerlas todas. Dentro de un líquido siempre hay empuje, hacia arriba; al "
            "moverse en un líquido viscoso aparece la fuerza de Stokes, opuesta al movimiento (hacia arriba porque baja); "
            "y el peso, hacia abajo.",
        "Velocidad terminal":
            "Velocidad terminal es velocidad constante, aceleración cero: por Newton, lo de abajo iguala a lo de arriba, "
            "$mg = B + F_v$. La A olvida el empuje y la C contradice la aceleración nula.",
    },
    "P2E26-2": {
        "La casilla de tensión superficial":
            "La única casilla que une fuerza, largo y tensión superficial es $\\gamma = F/(2l)$. El 2 está porque la "
            "película tiene dos caras y cada una tira del alambre.",
        "Mismo alambre":
            "Se despeja $F = 2\\gamma l$. Con el mismo alambre, $l$ no cambia, así que la fuerza es proporcional a "
            "$\\gamma$: el doble de $\\gamma$, el doble de fuerza.",
    },
    "O26-3": {
        "Comparar ambas barras":
            "La casilla de dilatación lineal, $\\Delta L = \\alpha L_0\\Delta T$, tiene las tres cosas que compara el "
            "enunciado. Como $L_0$ y $\\Delta T$ son iguales en las dos barras, solo cambia $\\alpha$, y $\\Delta L$ es "
            "proporcional a $\\alpha$.",
    },
    "O26-4": {
        "Pasar primero a temperatura absoluta":
            "En $pV = nRT$ la temperatura tiene que estar en kelvin: la proporción entre $p$ y $T$ solo vale desde el cero "
            "absoluto. Duplicar 25 °C daría 50 °C, que es la trampa de la opción C.",
        "Duplicar la presión":
            "Con $V$ y $n$ fijos, el gas ideal queda $p/T$ = constante, así que $p_2/T_2 = p_1/T_1$. Si la presión se "
            "duplica, la temperatura absoluta también: $T_2 = 2T_1$.",
        "Volver a grados Celsius":
            "La pregunta pide la temperatura en °C, así que al final se resta 273,15. Si no, quedaría 596, que es la opción "
            "trampa B.",
    },
    "P1O26-2": {
        "La intensidad baja con la distancia":
            "La casilla $I = P/(4\\pi r^2)$ vale para una fuente puntual que emite en todas direcciones, que es lo que dice "
            "el enunciado. La potencia del dron no cambia, así que $I_1r_1^2 = I_2r_2^2$: no hace falta calcular $P$.",
        "Despejar la distancia nueva":
            "Se despeja $r_2 = r_1\\sqrt{I_1/I_2}$. Como la intensidad buscada es mayor, hay que acercarse: $r_2$ tiene que "
            "salir menor que 7,5 m, y eso sirve de control.",
    },
    "P1O26-4": {
        "La rapidez depende solo de la cuerda":
            "La casilla $v = \\sqrt{F/\\mu}$ solo tiene la tensión y la densidad lineal, que son de la cuerda, no de la "
            "onda. Por eso dos ondas en la misma cuerda van igual de rápido, tengan la amplitud que tengan: la II es "
            "verdadera.",
        "Con igual frecuencia":
            "Con $v = \\lambda f$: si $v$ es la misma (misma cuerda) y $f$ también, $\\lambda$ tiene que ser la misma. La I "
            "dice lo contrario, así que es falsa.",
    },
    "P1E26-2": {
        "Comparar con la casilla de la onda viajera":
            "Se compara la función del enunciado con la casilla $y = A\\cos(kx - \\omega t)$ término por término: así se "
            "leen $k = 4{,}0$ y $\\omega = 50$ sin calcular nada.",
        "Longitud de onda y rapidez":
            "Con $k$ y $\\omega$, las casillas dan lo que piden las opciones: $\\lambda = 2\\pi/k$ y $v = \\omega/k$. Hay que "
            "hacer las dos cuentas: la C tiene bien $\\lambda$ y mal $v$, y la D al revés.",
    },
    "P1E26-3": {
        "La casilla de potencia media":
            "La potencia media de una onda en una cuerda es $P = \\tfrac12\\sqrt{\\mu F}\\,\\omega^2A^2$. Como las cuerdas y "
            "la tensión son iguales, $\\sqrt{\\mu F}$ se cancela al comparar: solo importa $\\omega^2A^2$.",
        "Doble de frecuencia y mitad de amplitud":
            "Con una razón: $P_1/P_2 = 2^2\\cdot(\\tfrac12)^2 = 1$. Los dos cambios se compensan exactamente porque los dos "
            "van al cuadrado.",
    },
    "P1E26-5": {
        "Superposición":
            "Cuando dos pulsos se cruzan, la cuerda toma la suma de los dos desplazamientos. Con igual forma y signos "
            "opuestos, la suma da cero en el instante en que coinciden. Cada pulso sigue con la rapidez que le da la "
            "cuerda, $v = \\sqrt{F/\\mu}$.",
        "Qué opción es":
            "La energía no se pierde: en ese instante la cuerda está plana pero se está moviendo, la energía es cinética. "
            "Por eso los pulsos reaparecen y siguen; la B, la C y la D contradicen eso.",
    },
    "RP-26I-1": {
        "Densidad lineal y rapidez transversal":
            "La frecuencia sale de $f = v/\\lambda$, así que primero hace falta la rapidez. La casilla $v = \\sqrt{F/\\mu}$ "
            "necesita $\\mu$, que es masa entre largo.",
        "Frecuencia fundamental":
            "En la fundamental cabe media onda en la cuerda: $\\lambda_1 = 2L$, así que $f_1 = v/(2L)$.",
    },
    # ---------------------------------------------------------------- simulacros y el posible cuarto problema
    "P1E26-9": {
        "a) Qué armónico y qué largo":
            "Con los dos extremos fijos, cada antinodo es media onda, así que 4 antinodos es el armónico $n = 4$. La "
            "distancia entre nodos vecinos es $\\lambda/2$, y el largo tiene $n$ medias ondas: $L = 4 \\times 0{,}240$ m.",
        "b) Densidad lineal y rapidez":
            "La rapidez en una cuerda sale de $v = \\sqrt{F/\\mu}$, y $\\mu$ es masa entre largo. Por eso primero $\\mu$, con "
            "la masa en kg y el largo del inciso a.",
        "c) Aceleración máxima":
            "Cada punto de un antinodo sube y baja en MAS con la frecuencia de la onda, y su amplitud es la del antinodo. "
            "La aceleración máxima de un MAS es $\\omega^2 A$; por eso primero se calculan $f = v/\\lambda$ y $\\omega = 2\\pi f$.",
        "d) Tensión 44 % mayor":
            "La rapidez va con la raíz de la tensión: $\\sqrt{1{,}44} = 1{,}20$, así que $v$ sube un 20 %. La fundamental usa "
            "$\\lambda_1 = 2L$ (el largo no cambia) y $f_1 = v'/(2L)$.",
    },
    "P2O26-9": {
        "a) Número de Reynolds":
            "Antes de usar Poiseuille hay que saber si el flujo es laminar, porque Poiseuille solo vale ahí. La casilla de "
            "Reynolds lo decide: $Re = \\rho v d/\\eta$; si da menos de 2000, es laminar.",
        "b) Caudal":
            "El caudal es área por rapidez media, $dV/dt = Av$. El área es circular, $\\pi d^2/4$, porque el radio es medio "
            "diámetro.",
        "c) Caída de presión":
            "La casilla de Poiseuille une el caudal con la diferencia de presión en un tubo viscoso. Se despeja "
            "$\\Delta p = 8\\eta L\\,(dV/dt)/(\\pi r^4)$. El radio va a la cuarta, así que tiene que estar bien en metros.",
        "d) Rapidez en la tubería secundaria":
            "La secundaria sale de la misma bomba y descarga al aire a la misma altura, así que tiene la misma caída de "
            "presión que la principal. Se usa Poiseuille con esa $\\Delta p$ y se cambia el caudal por $\\pi r^2 v$ para "
            "despejar $v_2$.",
    },
    "O26-9": {
        "Qué nos dan y el atajo":
            "Los tramos curvos (adiabático e isotérmico) tienen trabajos difíciles. La energía interna es función de estado: "
            "solo depende del punto, no del camino. Por eso $\\Delta U_{AC}$ se puede encontrar cerrando el ciclo por los "
            "tramos fáciles, sin calcular esos trabajos.",
        "a) Cerrar el ciclo":
            "En un ciclo $\\sum\\Delta U = 0$. El isotérmico C→D tiene $\\Delta U = 0$, así que $\\Delta U_{AC}$ (de A a C "
            "pasando por B) compensa al único que queda, D→A: $\\Delta U_{AC} = -\\Delta U_{DA}$.",
        "a) Trabajo y primera ley":
            "D→A es isobárico y da su calor: es el tramo donde la primera ley se puede usar entera. $W = p\\,\\Delta V$ por "
            "ser a presión constante, y $\\Delta U = Q - W$ con $Q$ negativo porque sale.",
        "b) Deducción de γ":
            "$\\gamma = C_p/C_V$. En un isobárico el calor es $Q = nC_p\\Delta T$ y $\\Delta U = nC_V\\Delta T$, con el mismo "
            "$\\Delta T$. Al dividir se cancelan $n$ y $\\Delta T$: $\\gamma = Q_{DA}/\\Delta U_{DA}$, sin conocer las temperaturas.",
        "b) Sustitución y verificación":
            "Se sustituyen los dos números del tramo D→A. La comprobación con los 10,0 moles (temperaturas de A y D con el "
            "gas ideal) llega al mismo valor por otro camino: si coinciden, no hay error de signo.",
    },
    "T-INT2": {
        "1. Fourier de la hoja con k que cambia":
            "$H = kA\\,\\Delta T/L$ supone $k$ constante. Si $k$ cambia con $y$, hay que usar la forma diferencial "
            "$H = -kA\\,dT/dy$ en cada capa delgada y separar: lo de $y$ con $dy$, lo de $T$ con $dT$.",
        "2. La regla del logaritmo":
            "Queda $\\int dy/(200 + 1000y)$: uno entre algo. La regla de la potencia no sirve ahí (daría dividir entre cero), "
            "por eso se usa $\\int du/u = \\ln u$, y para eso se le pone nombre $u$ al paréntesis.",
        "3. Cambian los límites":
            "Al cambiar a $u$ también cambian los límites: cuando $y$ va de 0 a 0,02, $u$ va de 200 a 220. Y como "
            "$du = 1000\\,dy$, aparece el $1/1000$ que se suele olvidar.",
        "4. Despejar el flujo":
            "El lado de la temperatura es la integral más simple: $\\int dT$ es el cambio total, de 300 a 100 °C. Igualando "
            "los dos lados se despeja $H$.",
    },
    "Q050": {
        "A) Separar variables":
            "$H$ es el mismo en todas las capas (estado estacionario), así que puede quedar afuera. Se separa: lo que "
            "depende de $y$ con $dy$ a un lado y $dT$ al otro, para integrar cada lado por su cuenta.",
        "A) La integral de la izquierda":
            "Queda $\\int dy/(200 + 1000y)$, uno entre algo: es la integral del logaritmo. Se le pone nombre $u$ al "
            "paréntesis; como $du = 1000\\,dy$, aparece $1/1000$, y los límites pasan a 200 y 220.",
        "A) Despejar el flujo":
            "Con las dos integrales hechas, al igualarlas queda una ecuación con $H$ sola, que se despeja.",
        "B) Energía":
            "La tasa de flujo es energía por segundo: $H = Q/t$. Por eso $Q = Ht$, con los 5 minutos pasados a segundos.",
        "C) Temperatura a la mitad":
            "Es la misma integral pero solo hasta $y = d/2$, donde $u$ llega a 210. Con el $H$ del inciso A se despeja la "
            "temperatura en ese punto. Sale por debajo del promedio porque la mitad de abajo conduce peor y ahí la "
            "temperatura cae más.",
    },
    "P2E26-9": {
        "a) Viscosidad":
            "La casilla de Poiseuille une el caudal, la diferencia de presión y la viscosidad. Como se conocen el caudal y "
            "$\\Delta p$, la única incógnita es $\\eta$, y se despeja. El radio va a la cuarta: tiene que estar en metros.",
        "b) Velocidad máxima":
            "La casilla del perfil, $v(r) = \\frac{\\Delta p}{4\\eta L}(R^2 - r^2)$, da la rapidez a cada distancia del eje. "
            "Es máxima en el eje ($r = 0$), el punto más lejos de las paredes que frenan al aceite.",
        "b) Velocidad a 1,00 mm":
            "Es la misma casilla con $r = 1{,}00$ mm. Como $R^2 - r^2$ queda en $\\tfrac34R^2$, la velocidad tiene que salir "
            "tres cuartos de la máxima: sirve de control.",
        "c) Velocidad terminal":
            "La esfera cae hasta que peso, empuje y Stokes se equilibran; la casilla de velocidad terminal ya trae ese "
            "equilibrio resuelto: $v_t = \\frac29\\,r^2 g(\\rho_e - \\rho)/\\eta$. Usa el radio de la esfera y la viscosidad "
            "del inciso a.",
        "d) Fuerza viscosa":
            "A la velocidad terminal, la fuerza de Stokes es $F = 6\\pi\\eta r v_t$. Se calcula con el radio de la esfera, no "
            "con el del tubo.",
    },
    "P1E26-7": {
        "Cociente de rapideces":
            "La rapidez en una cuerda solo depende de la tensión y de $\\mu$: $v = \\sqrt{F/\\mu}$. Al dividir las dos, "
            "$v_2/v_1 = \\sqrt{(4F/2\\mu)/(F/\\mu)} = \\sqrt2$. El largo no entra, por eso «misma longitud» no cambia nada.",
    },
    "P1O26-5": {
        "Energía total y energía potencial":
            "La razón $K/E$ se arma con las casillas de energía: $E = \\tfrac12kA^2$ y $U = \\tfrac12kx^2$. Con $x = A/2$, "
            "$U$ es un cuarto de $E$; así no hacen falta $k$ ni $A$ con números.",
        "La cinética es lo que falta":
            "Sin fricción, $K = E - U = \\tfrac34E$, así que $K/E = 0{,}75$. La trampa es contestar $U/E = 0{,}25$.",
    },
    "P2O26-1": {
        "Qué dice la hoja del módulo elástico":
            "El módulo de Young de la hoja es esfuerzo entre deformación, pero solo en la zona elástica, donde el material "
            "vuelve a su forma y la deformación es proporcional al esfuerzo. Por eso primero hay que ver en qué zona está "
            "cada dato.",
        "Con sigma el material vuelve":
            "Con $\\sigma$ recupera su forma: zona elástica, así que $Y = \\sigma/\\varepsilon$. Con $2\\sigma$ no la recupera: "
            "ya pasó el límite elástico y $2\\sigma/(3\\varepsilon)$ no es el módulo. El límite quedó entre $\\sigma$ y "
            "$2\\sigma$, lo que descarta C y D.",
    },
    "P2E26-4": {
        "Para que un fluido viscoso avance":
            "En un fluido viscoso no alcanza con Bernoulli: el roce hace perder presión. Poiseuille dice que el caudal es "
            "proporcional a $p_1 - p_2$; si el aceite avanza, la presión tiene que ir bajando en la dirección del flujo.",
        "La altura en cada tubo":
            "Cada tubo vertical abierto sube hasta que $\\rho g h$ iguala la presión manométrica de su punto. Si la presión "
            "baja a lo largo del tubo, las alturas también: son decrecientes, la opción B.",
    },
    "O26-1": {
        "Relación entre intervalos":
            "En una capacidad calorífica el grado está en el denominador y es un cambio de temperatura, no una lectura del "
            "termómetro. Al restar dos temperaturas se cancela el 32 de la escala Fahrenheit: queda "
            "$\\Delta T_F = 1{,}8\\,\\Delta T_C$, y $\\Delta T_C = \\Delta T_K$.",
        "Factor de conversión":
            "Un kelvin es un cambio más grande que un grado Fahrenheit (1 K equivale a 1,8 °F). Por eso, por kelvin, el número "
            "sube: se multiplica por 1,8. La comprobación con $\\tfrac72R$ del diatómico confirma que da cerca de 29.",
    },
    "O26-5": {
        "Qué significa régimen estacionario":
            "Estacionario quiere decir que las temperaturas ya no cambian con el tiempo: ninguna capa acumula energía, y todo "
            "el calor que entra por una cara sale por la otra. Por eso $H$ es el mismo en todas las capas. Con "
            "$H = kA\\,\\Delta T/L$, si $k$ cambia, la caída de temperatura de cada capa también cambia.",
    },
    "P1O26-9": {
        "a) Densidad lineal y rapidez inicial":
            "$\\mu$ es masa entre largo, con la masa en kg. La rapidez sale de la fundamental: ahí $\\lambda_1 = 2L$, y "
            "$v = \\lambda f$. Así no hace falta conocer la tensión.",
        "b) La tensión sube 4 %":
            "La casilla $v = \\sqrt{F/\\mu}$ con $\\mu$ igual: al dividir las dos rapideces, $\\mu$ se cancela y queda "
            "$v_2 = v_1\\sqrt{1{,}04}$. Por eso no hace falta la tensión en newtons.",
        "c) Nueva fundamental":
            "El tercer armónico es tres veces la fundamental, así que $f_1' = 842{,}4/3$. Con la rapidez nueva y "
            "$f_1' = v_2/(2L_2)$ se despeja el largo nuevo.",
        "d) Dónde está el traste":
            "El traste corta la cuerda: la parte que ya no vibra es lo que se acortó, $\\Delta L = L_1 - L_2$.",
        "e) Longitud de onda":
            "Con el largo nuevo y $n = 3$, la casilla de los armónicos da $\\lambda_3' = 2L_2/3$.",
    },
    "SUF26-4": {
        "A) Caudal y flujo másico":
            "El caudal es área por rapidez, $dV/dt = Av$, con el área circular de la parte alta, donde se conocen el diámetro "
            "y la rapidez. El flujo de masa es la densidad por el caudal.",
        "B) Bernoulli":
            "Entre la parte alta y la baja cambian la altura y la presión, y no se conoce $v_2$: eso es Bernoulli con todos "
            "sus términos. La altura cero va abajo para quitar un término.",
        "C) Diámetro":
            "El caudal es el mismo arriba y abajo (incompresible), así que el área de abajo es el caudal entre $v_2$. Del "
            "área circular se despeja el diámetro.",
    },
    "Q107": {
        "La energía interna en términos de pV":
            "No hay temperaturas, solo $p_0$ y $V_0$. Como $nRT = pV$ en un gas ideal, $\\Delta U = nC_V\\Delta T = "
            "\\tfrac52\\Delta(pV)$: así cada $\\Delta U$ sale con los valores de $p$ y $V$ de la figura.",
        "A) Proceso 1→2":
            "De 1 a 2 el volumen no cambia: $W = 0$ y $Q = \\Delta U$. Con la relación del paso anterior, "
            "$\\Delta U = \\tfrac52(2p_0V_0 - p_0V_0)$.",
        "A) Proceso 3→4":
            "De 3 a 4 el volumen no cambia ($2V_0$): $W = 0$ y todo el cambio de energía interna sale como calor. Como la "
            "presión baja, $\\Delta U$ y $Q$ son negativos.",
        "B) Calor absorbido y calor expulsado":
            "$Q_H$ es la suma de los calores positivos y $Q_C$ la de los negativos. La tabla con los cuatro procesos deja "
            "ver los signos de un vistazo.",
        "C) Eficiencia":
            "Es una máquina, así que $e = W_{neto}/Q_H$. El trabajo neto es el área del rectángulo, $p_0V_0$, y $Q_H$ es lo "
            "que entra, no el calor neto.",
    },
    # ---------------------------------------------------------------- ruta de integrales y pregunta 7 del III parcial
    "O26-7": {
        "La casilla del ciclo Otto":
            "La pregunta compara eficiencias de dos ciclos Otto y la hoja trae justo esa casilla: "
            "$e = 1 - 1/r^{\\gamma-1}$. Como $r$ es igual en los dos, la comparación depende solo del exponente $\\gamma - 1$.",
        "Probar con un número":
            "La respuesta no puede depender del valor de $r$, así que basta probar con uno cualquiera mayor que 1. Con "
            "más exponente, $r^{\\gamma-1}$ crece, la fracción $1/r^{\\gamma-1}$ se achica y la eficiencia sube.",
    },
    "E2-8": {
        "Presión manométrica y ancho de franja":
            "La presión cambia con la profundidad y el ancho de la puerta también, así que hay que usar franjas. Con $y$ "
            "hacia abajo desde la interfaz, la presión de una franja es la del aceite más $\\rho_2 g y$, y el ancho sale del "
            "círculo: $x^2 + y^2 = R^2$ da medio ancho $\\sqrt{R^2-y^2}$.",
        "Separar la integral en dos aportes":
            "La presión tiene dos partes: la del aceite, que es la misma en toda la puerta, y la del agua, que crece con $y$. "
            "Separarlas deja una cuenta fácil (presión fija por el área del semicírculo, $\\pi R^2/2$) y una integral que se "
            "resuelve con $u = R^2 - y^2$.",
        "Suma total":
            "Las dos partes empujan la misma puerta en la misma dirección, así que la fuerza total es su suma.",
    },
    "Q045": {
        "A) Fourier en un cascarón":
            "El calor sale del centro hacia afuera y el área por donde pasa crece con el radio, así que $H = kA\\,\\Delta T/L$ "
            "no sirve. Se usa la forma diferencial en un cascarón delgado, donde el área es una sola: la pared del "
            "cilindro, $2\\pi r L$.",
        "A) Separar variables":
            "$H$ es el mismo en todos los cascarones (estado estacionario), así que se separa: lo que depende de $r$ con "
            "$dr$ a un lado y $dT$ al otro, para integrar cada lado por su cuenta.",
        "A) Resolver las dos integrales":
            "La de la izquierda es uno entre $r$, que da logaritmo; la de la derecha es la suma de los $dT$, que da el "
            "cambio total. Los límites salen del dibujo: de $r_1$ a $r_2$ y de $T_1$ a $T_2$.",
        "A) Despejar el flujo":
            "Con las dos integrales hechas, la única incógnita es $H$: se despeja "
            "$H = 2\\pi k L(T_1-T_2)/\\ln(r_2/r_1)$.",
        "A) Por cada metro":
            "Piden el flujo «por unidad de longitud», así que se divide entre el largo del tubo. Es lo mismo que poner "
            "$L = 1$ m en la fórmula.",
        "B) Energía":
            "La tasa de flujo es energía por segundo: $H = Q/t$. Por eso $Q = Ht$, con los 10,0 min pasados a segundos.",
        "C) Temperatura":
            "Para la temperatura en un radio intermedio se repite la misma integral, pero solo hasta ese radio: de $r_1$ "
            "a $r$ y de $T_1$ a $T(r)$. Con el $H$ del inciso A, la única incógnita es $T(r)$.",
    },
    "T-PLANTEO": {
        "1. Por qué aparece una integral":
            "Entender por qué aparece la integral evita memorizar casos: siempre es porque algo de la fórmula de la hoja "
            "cambia de un punto a otro.",
        "2. Las 5 preguntas":
            "Un orden fijo le quita al planteo la parte de adivinar: cada pregunta tiene una respuesta concreta en el "
            "dibujo, y la siguiente sale de la anterior.",
        "3. Solo hay tres pedacitos":
            "Casi todos los problemas del curso usan uno de estos tres cortes. Elegir bien el pedacito es lo que hace que "
            "adentro nada cambie.",
        "4. Ejemplo con franja":
            "La pared es el caso más simple: el ancho no cambia, así que se ve solo la idea (la presión crece con $h$) "
            "sin geometría complicada.",
        "5. Ejemplo con disco":
            "Es la pregunta 8 del III parcial (16 puntos). El área cambia con la altura y Pitágoras da el radio de cada "
            "disco: ahí está la geometría.",
        "6. La esfera cortada":
            "Con el área del disco escrita, el resto es la receta de siempre: Fourier, separar, integrar con los límites "
            "del dibujo y despejar.",
        "7. Ejemplo con cascarón":
            "En un tubo el calor no va de una cara a otra sino del centro hacia afuera, así que el pedacito cambia a "
            "cascarón. Su área, $2\\pi r L$, es el largo de la vuelta por el largo del tubo.",
        "8. Lo que se escribe en el examen":
            "El examen pide el procedimiento completo. Escribir estos renglones en orden muestra el planteo aunque la "
            "cuenta final tenga un error.",
    },
    "T-MAT1": {
        "La pendiente":
            "Antes de integrar hay que entender qué mide $dT/dx$, porque aparece en la ley de Fourier de la hoja. La "
            "pendiente es lo más concreto: cuánto cambia la temperatura por cada metro.",
        "dT/dx":
            "En los problemas la temperatura no cambia parejo, así que hace falta la pendiente en cada punto. $dT/dx$ es "
            "eso: la pendiente con una distancia pequeñísima.",
        "Pedacitos":
            "Pensar $dx$ y $dT$ como pedacitos permite pasarlos de un lado al otro como números. Eso es lo que después "
            "se llama separar variables.",
        "La integral":
            "Si se suman todos los pedacitos de cambio, sale el cambio total. Esa suma es la integral; los límites dicen "
            "desde dónde y hasta dónde se suma.",
        "Calcular una integral":
            "Para no sumar pedacito por pedacito, se busca una función cuya resta entre los dos límites da la suma "
            "completa. Por eso se evalúa «arriba menos abajo».",
        "Los números constantes":
            "Lo que no cambia ($k$, $\\pi$, un área fija) multiplica a todos los pedacitos por igual, así que sale de la "
            "suma. Adentro queda solo lo que cambia.",
        "Derivada de una potencia":
            "La derivada aparece pocas veces en el curso. Basta con la regla de la potencia, que es la idea de la "
            "integral al revés.",
    },
    "T-MAT2": {
        "Regla 1 · Potencia":
            "Es la regla que más se usa: paredes, compuertas y conos terminan en potencias de la variable. Por eso va primero.",
        "Regla 1 con exponentes negativos":
            "En las esferas el área es $4\\pi r^2$ y al separar queda $1/r^2$. Escrito como $r^{-2}$ es la misma regla de "
            "la potencia, sin aprender nada nuevo.",
        "Regla 2":
            "Con exponente $-1$ la regla de la potencia dividiría entre cero, así que esa integral es distinta: da "
            "logaritmo. Aparece en los tubos ($A = 2\\pi r L$) y con $k$ variable.",
        "Regla 3 · Cambio":
            "Cuando abajo hay un paréntesis $a + bx$, ponerle nombre $u$ convierte la integral en una de las reglas 1 o 2. "
            "El precio es el factor $1/b$ y los límites nuevos.",
        "Regla 3: para qué sirve":
            "Esa forma aparece cuando el radio o el área crecen en línea recta (conos truncados, barras de área lineal). "
            "Reconocerla en el enunciado dice qué regla usar.",
        "Regla 4":
            "Algunas tareas tienen áreas o coeficientes exponenciales. La exponencial casi no cambia al integrar: solo se "
            "divide entre lo que multiplica a $x$ en el exponente.",
        "La tabla completa":
            "Tener todas las reglas juntas evita buscar: cada integral del curso es una de estas, y las lecciones las "
            "nombran por número.",
    },
    "T-MAT3": {
        "Qué es un logaritmo":
            "Las integrales de tubos y de $k$ variable dan logaritmos, y para despejar hay que saber qué son. El logaritmo "
            "natural responde a qué potencia hay que elevar $e$.",
        "Las tres reglas":
            "Estas reglas juntan o separan logaritmos: la resta $\\ln r_2 - \\ln r_1$ se vuelve $\\ln(r_2/r_1)$, que es como "
            "aparece en las respuestas.",
        "Despejar un exponente":
            "Cuando la incógnita está en un exponente (como $\\gamma$ en $p_AV_A^\\gamma = p_BV_B^\\gamma$), el logaritmo la "
            "baja a multiplicar y se puede despejar.",
        "Separar variables":
            "La ley de Fourier mezcla $T$ y $x$ en $dT/dx$. Separar deja cada variable con su pedacito, para integrar cada "
            "lado por su cuenta.",
        "Integrar los dos lados":
            "Los dos lados describen el mismo camino, de un extremo al otro, así que los límites son los mismos dos "
            "puntos: posición y temperatura del inicio y del final.",
        "Cuando el área cambia":
            "Si el área depende de $x$ no puede salir de la integral: se escribe $A(x)$ y se elige la regla según su forma. "
            "Esa es la diferencia entre una pared y un cono o un tubo.",
    },
    "T-CONO": {
        "1. Qué es un tronco de cono":
            "Sin ver la forma no se puede plantear nada. Lo clave es la dirección del calor: va a lo largo del cono, así "
            "que el pedacito es una rebanada perpendicular a esa dirección, un disco.",
        "2. El radio crece parejo":
            "El área de cada rebanada depende del radio, así que primero hay que saber cómo cambia el radio. La tabla "
            "muestra que crece lo mismo por cada centímetro: es una recta.",
        "3. La fórmula del radio":
            "Para meter el radio en la integral hace falta una fórmula, no una tabla. Como crece parejo, es una recta, "
            "$r(x) = r_1 + bx$. Revisar los extremos confirma que está bien.",
        "4. El área de cada rebanada":
            "Cada rebanada es un círculo, así que su área es $\\pi r(x)^2$. Es la pregunta 4 de las 5: el tamaño del "
            "pedacito escrito con la variable.",
        "5. La idea que destraba todo":
            "En estado estacionario $H$ es el mismo en todas las rebanadas, así que es una constante que puede salir de la "
            "integral. Eso es lo que permite separar.",
        "6. La receta de 6 pasos":
            "La receta fija evita pensar desde cero en el examen: solo cambia la fórmula del área. Es la misma idea de las "
            "5 preguntas, escrita para conducción.",
        "7. Pasos 3 y 4":
            "Fourier con el área del cono da una ecuación con $T$ y $x$ mezclados. Separar deja lo de $x$, con el área, a "
            "un lado y $dT$ al otro.",
        "8. Paso 5":
            "Integrar los dos lados es sumar todas las rebanadas, de una cara a la otra. Los límites salen del dibujo: $x$ "
            "de 0 a $L$ y $T$ de $T_1$ a $T_2$.",
        "9. La integral de la izquierda (1)":
            "Con el radio como variable la integral queda como una potencia simple, $1/u^2$. El cambio de $x$ a $u$ trae "
            "el factor $1/b$ y cambia los límites a los dos radios.",
        "10. La integral de la izquierda (2)":
            "$1/u^2$ es $u^{-2}$: la regla de la potencia sirve tal cual. Después se evalúa arriba menos abajo con los radios.",
        "11. Simplificar":
            "Simplificar deja el resultado en su forma más corta, $L/(\\pi r_1 r_2)$, que es fácil de reconocer y de revisar.",
        "12. Paso 6":
            "Es lo que piden: con la integral resuelta, $H$ es la única incógnita.",
        "13. Truco para recordarlo":
            "Comparar con la pared de la hoja es un control rápido: si los dos radios son iguales, el cono es un cilindro "
            "y tiene que salir $H = kA\\,\\Delta T/L$.",
        "14. Con números":
            "Los números van al final y en metros, para no arrastrar errores de unidades en el álgebra.",
        "15. Lo que se escribe en el examen":
            "En el examen se evalúa el procedimiento: estos renglones, en orden, muestran cada paso del planteo sin "
            "saltarse nada.",
    },
}


# ============================================================================== mecánica

_RE_SIGUE = re.compile(r"\s*\(sigue\)\s*$")
_RE_SIM = re.compile(r"\[\[([^\]|]+)(?:\|([^\]]+))?\]\]")


def _voz(txt: str) -> str:
    """[[p_A]] -> símbolo narrable de dsl.s(); el resto queda igual."""
    return _RE_SIM.sub(lambda m: s(m.group(1), m.group(2)), txt)


def _palabras_habladas(texto: str) -> int:
    return len(separa(texto)[1].split())


def base_del_titulo(titulo: str) -> str:
    """Título sin los «(sigue)» que agrega el desglose."""
    t = titulo
    while True:
        u = _RE_SIGUE.sub("", t)
        if u == t:
            return t.strip()
        t = u


def pasos_de_datos(datos: Sequence[_Dato]) -> List[Paso]:
    """El paso «Datos» (partido en «(sigue)» si se pasa de largo)."""
    tramos = [Tramo(_INTRO, [])]
    for frase, tex, para in datos:
        acc = ([DATO(frase)] if frase else []) + [M(tex)]
        tramos.append(Tramo(" ".join(_voz(para).split()), acc))
    pasos: List[Paso] = []
    actual: Optional[Paso] = None
    cuenta = 0
    for t in tramos:
        n = _palabras_habladas(t.texto)
        if actual is None or (cuenta + n > _MAX_PALABRAS and actual.tramos):
            actual = Paso(TITULO_DATOS if not pasos else f"{TITULO_DATOS} (sigue)")
            pasos.append(actual)
            cuenta = 0
        actual.tramos.append(t)
        cuenta += n
    return pasos


def porque_de(lid: str, titulo: str) -> str:
    """El porqué de un paso: el propio de la lección (por prefijo del título) o el común."""
    base = base_del_titulo(titulo)
    propios = PORQUE.get(lid, {})
    if base in propios:
        return propios[base]
    mejor = ""
    for clave in propios:
        if base.startswith(clave) and len(clave) > len(mejor):
            mejor = clave
    if mejor:
        return propios[mejor]
    for patron, txt in _GENERICOS:
        if re.search(patron, base, re.IGNORECASE):
            return txt
    return ""


def aplicar(lec: Leccion) -> None:
    """Agrega el paso «Datos» y completa el «¿Por qué?» de cada paso. No toca los porqués que ya había."""
    datos = DATOS.get(lec.id)
    if datos and not any(p.titulo.startswith(TITULO_DATOS) for p in lec.pasos):
        lec.pasos[0:0] = pasos_de_datos(datos)
    if lec.id not in DATOS and lec.id not in PORQUE:
        return
    for p in lec.pasos:
        if not p.porque:
            p.porque = porque_de(lec.id, p.titulo)


def lecciones_con_explicaciones() -> List[str]:
    return sorted(set(DATOS) | set(PORQUE))
