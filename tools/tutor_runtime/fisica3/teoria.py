"""Teoría en cinco minutos para la Ruta del examen (T-DIL a T-ENT).

Cada lección va de lo concreto a la fórmula: qué pasa, qué fórmula del formulario II-2026 lo dice,
qué significa cada letra, un ejemplo con números y la trampa típica. Un movimiento por renglón.
"""

from __future__ import annotations

from .ayudas import Gas, visual_pv
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, T, TABLA, V, s
from .formato import dice, n, nu, tex
from .registro import leccion


def _teo(id: str, titulo: str, tema: str, **k) -> Leccion:
    return Leccion(id, titulo, tema=tema, tipo="teoria", nivel=1, fuente={"texto": "Teoría · formulario oficial II-2026"}, **k)


# ============================================================================ T-DIL
@leccion("T-DIL")
def t_dil() -> Leccion:
    L = _teo("T-DIL", "Teoría rápida: dilatación térmica", "4.4", claves=["dilatación", "alfa", "beta", "hueco"],
             enunciado="Qué le pasa al tamaño de las cosas cuando cambia la temperatura, y cómo se calcula.",
             visual={"tipo": "dilatacion", "caso": "barra", "L0": 2.0, "a": 2.4e-5, "T0": 20, "dT": 80, "nombre": "barra metálica"})
    L.paso("La idea: al calentar, todo crece un poquito")\
        .di("Al calentar un sólido, sus átomos vibran más y se separan. Por eso todas sus medidas crecen.", V(foco="calentar"))\
        .di("Crece más si es más largo y si se calienta más.", T("Más largo **o** más calentamiento → más crecimiento."))\
        .di("Y cada material crece distinto: eso lo dice alfa.".replace("alfa", s("α", "alfa")), M(r"\alpha\ \text{grande}\;\Rightarrow\;\text{se dilata mucho}"))
    L.paso("La fórmula de largo")\
        .di("Es la primera del formulario.", F(r"\Delta L=\alpha L_0\,\Delta T"))\
        .di("Cada letra.", LISTA(["$\\Delta L$: cuánto creció (mismas unidades que $L_0$)", "$\\alpha$: coeficiente del material, en $^\\circ\\text{C}^{-1}$", "$L_0$: largo inicial", "$\\Delta T$: cambio de temperatura, final menos inicial"]))\
        .di("El largo final es el inicial más lo que creció.", M(r"L=L_0+\Delta L=L_0\,(1+\alpha\,\Delta T)", "lf", "azul"))
    L.paso("Un ejemplo con números")\
        .di("Una barra de aluminio de dos metros se calienta ochenta grados.", M(r"L_0=2{,}00\ \text{m}\qquad \alpha=2{,}4\times10^{-5}\qquad \Delta T=80"))\
        .di("Sustituimos.", M(r"\Delta L=(2{,}4\times10^{-5})(2{,}00)(80)"))\
        .di("Crece casi cuatro milímetros.", R(r"\Delta L=3{,}84\times10^{-3}\ \text{m}=3{,}84\ \text{mm}"))\
        .pregunta_rapida("Si la barra midiera el doble, ¿cuánto crecería?", ["El doble", "Lo mismo"], 0, "$\\Delta L$ es proporcional a $L_0$.")
    L.paso("Volumen: la otra fórmula")\
        .di("Para volúmenes se usa beta.".replace("beta", s("β", "beta")), F(r"\Delta V=\beta V_0\,\Delta T"))\
        .di("En un sólido, beta es tres veces alfa, porque crece en tres direcciones.".replace("beta", s("β", "beta")).replace("alfa", s("α", "alfa")), M(r"V=L_0^3(1+\alpha\Delta T)^3\approx V_0(1+3\alpha\Delta T)"), M(r"\beta=3\alpha", "b3", "azul"))\
        .por_que("Al elevar al cubo, $(1+\\alpha\\Delta T)^3=1+3\\alpha\\Delta T+\\dots$; los otros términos son pequeñísimos.")
    L.paso("Las trampas de examen")\
        .di("Un hueco crece como si estuviera lleno del mismo material.", T("**Hueco**: crece con el $\\alpha$ del material que lo rodea."))\
        .di("Un líquido en un recipiente: se derrama la diferencia de lo que crecen.", M(r"V_{\text{derrama}}=(\beta_{\text{líq}}-\beta_{\text{rec}})\,V_0\,\Delta T"))\
        .di("Y un cambio de temperatura vale lo mismo en Celsius que en kelvin.", OJO("$\\Delta T$ en °C es igual a $\\Delta T$ en K: es una resta."))
    return L.usa(r"\Delta L=\alpha L_0\Delta T", r"\Delta V=\beta V_0\Delta T")


# ============================================================================ T-CAL
@leccion("T-CAL")
def t_cal() -> Leccion:
    L = _teo("T-CAL", "Teoría rápida: calorimetría y cambios de fase", "4.8", claves=["calor específico", "calor latente", "equilibrio"],
             enunciado="Cuánto calor hace falta para calentar algo o cambiarlo de fase, y cómo se resuelve una mezcla.",
             visual={"tipo": "calentamiento", "m": 0.2, "Ti": -20, "Tf": 120})
    L.paso("Dos tipos de calor")\
        .di("Mira la curva: en las rampas sube la temperatura; en los tramos planos cambia la fase.", V(etapa=0))\
        .di("En las rampas.", F(r"Q=mc\,\Delta T"))\
        .di("En los tramos planos la temperatura no cambia.", V(etapa=1), F(r"Q=\pm mL"))\
        .di("Más si se funde o evapora; menos si se congela o condensa.", NOTA("$L_f$: fundir · $L_v$: evaporar. El signo dice si entra o sale."))
    L.paso("Un ejemplo: derretir y calentar hielo")\
        .di("Cien gramos de hielo a cero grados hasta agua a veinte grados. Primero se derrite.", M(r"Q_1=mL_f=(0{,}100)(3{,}34\times10^5)=33\,400\ \text{J}"))\
        .di("Luego esa agua se calienta veinte grados.", M(r"Q_2=mc\,\Delta T=(0{,}100)(4190)(20)=8380\ \text{J}"))\
        .di("Se suman.", R(r"Q=33\,400+8380=41\,780\ \text{J}"))\
        .pregunta_rapida("¿Qué cuesta más?", ["Derretir el hielo", "Calentarlo 20 grados"], 0, "El calor latente es enorme comparado con unos grados de calentamiento.")
    L.paso("Mezclas: lo que uno cede, otro lo gana")\
        .di("En un recipiente aislado, la suma de todos los calores, con su signo, es cero.", F(r"\sum Q=0"))\
        .di("Quien se enfría tiene calor negativo; quien se calienta, positivo.", M(r"Q_{\text{frío}}+Q_{\text{caliente}}=0"))\
        .di("Si todo es la misma sustancia sin cambio de fase, la temperatura final es un promedio pesado por la masa.", M(r"T_f=\frac{m_1T_1+m_2T_2}{m_1+m_2}"))
    L.paso("La trampa: revisar antes de despejar")\
        .di("Con hielo o vapor, primero se compara cuánto calor se pide y cuánto se puede dar.", T("1) ¿Alcanza para fundir **todo** el hielo? 2) ¿Alcanza para condensar **todo** el vapor?"))\
        .di("Si no alcanza, la temperatura final es cero o cien, y lo que se calcula es cuánta masa cambia de fase.", OJO("Si despejas $T_f$ sin revisar, puede salir $-15\\ ^\\circ\\text{C}$ con agua líquida: imposible."))
    return L.usa(r"Q=mc\Delta T", r"Q=\pm mL", r"\sum Q=0")


# ============================================================================ T-CON
@leccion("T-CON")
def t_con() -> Leccion:
    L = _teo("T-CON", "Teoría rápida: conducción y ley de Fourier", "5.4", claves=["Fourier", "serie", "paralelo", "cilindro", "esfera"],
             enunciado="Cómo pasa el calor a través de un sólido y cómo se calcula en paredes, tubos y esferas.",
             visual={"tipo": "conduccion", "geo": "cilindro", "r1": 0.04, "r2": 0.08, "L": 1.0, "T1": 100, "T2": 20, "k": 50, "uT": "°C"})
    L.paso("La idea: el calor baja de lo caliente a lo frío")\
        .di("La rapidez con que pasa el calor se llama hache, y se mide en watts.".replace("hache", s("H", "hache")), V(foco="barrido"), M(r"H=\frac{dQ}{dt}\quad[\text{W}=\text{J/s}]"))\
        .di("Pasa más si el material conduce bien, si el área es grande y si la diferencia de temperatura es grande.", LISTA(["$k$ grande (metal) → pasa más", "$A$ grande → pasa más", "Pared gruesa → pasa menos"]))
    L.paso("Pared plana: la fórmula directa")\
        .di("Cuando el área y la conductividad no cambian.", F(r"H=kA\,\frac{T_H-T_C}{L}"))\
        .di("Un ejemplo: una pared de ladrillo de doce metros cuadrados, veinte centímetros, diez grados de diferencia.", M(r"H=(0{,}800)(12{,}0)\frac{10}{0{,}200}=480\ \text{W}"))\
        .pregunta_rapida("Si la pared fuera el doble de gruesa, el flujo sería…", ["La mitad", "El doble"], 0, "$L$ está dividiendo.")
    L.paso("Cuando algo cambia: la fórmula de Fourier")\
        .di("Si el área o la conductividad cambian por el camino, se usa la versión con derivada.", V(foco="dr"), F(r"H=-kA\,\frac{dT}{dx}"))\
        .di("La receta siempre es la misma.", LISTA(["Escribir $A$ (y $k$) en función de $x$ o de $r$", "Separar: $H\\,\\frac{dx}{kA}=-dT$", "Integrar los dos lados", "Despejar $H$"]))\
        .di("En un tubo el área es la pared lateral; en una esfera, toda la superficie.", M(r"A_{\text{tubo}}=2\pi rL\qquad A_{\text{esfera}}=4\pi r^2"))
    L.paso("Ojo con las fórmulas de tubo y esfera")\
        .di("Al integrar en un tubo sale un logaritmo, y en una esfera, uno entre erre.".replace("uno entre erre", dice("1/r", "uno entre erre")), M(r"H_{\text{tubo}}=\frac{2\pi kL\,(T_1-T_2)}{\ln(r_2/r_1)}"))\
        .di("Esas fórmulas no vienen en el formulario: en el examen hay que mostrar la integral.", OJO("Escribe $H=-kA\\frac{dT}{dr}$, separa e integra. Mira los ejercicios E8, E9 y E11."))
    L.paso("Serie y paralelo")\
        .di("En serie, una capa tras otra: pasa el mismo flujo por todas.", V(foco="perfil"), M(r"H_1=H_2=H"))\
        .di("Se igualan los flujos de cada capa para hallar la temperatura de la unión.", M(r"k_1A\frac{T_1-T_x}{L_1}=k_2A\frac{T_x-T_2}{L_2}"))\
        .di("En paralelo, una al lado de otra: los flujos se suman.", M(r"H_{\text{total}}=H_1+H_2"))
    return L.usa(r"H=kA\frac{T_H-T_C}{L}", r"H=-kA\frac{dT}{dx}")


# ============================================================================ T-RAD
@leccion("T-RAD")
def t_rad() -> Leccion:
    L = _teo("T-RAD", "Teoría rápida: radiación y Stefan-Boltzmann", "5.9", claves=["radiación", "Stefan-Boltzmann", "emisividad"],
             enunciado="Cómo emite energía un cuerpo caliente aunque no toque nada.",
             visual={"tipo": "radiacion", "T": 400, "Ts": 300, "e": 0.85, "objeto": "radiador"})
    L.paso("La idea: todo cuerpo brilla, aunque no lo veamos")\
        .di("Todo cuerpo emite radiación, y entre más caliente, muchísimo más.", V(foco="calentar"))\
        .di("La fórmula del formulario.", F(r"H=Ae\sigma T^4"))\
        .di("Cada letra.", LISTA(["$A$: área que emite, en m²", "$e$: emisividad, de 0 a 1 (1 = cuerpo negro)", "$\\sigma=5{,}67\\times10^{-8}\\ \\text{W/(m}^2\\text{K}^4)$", "$T$: temperatura **en kelvin**"]))
    L.paso("Por qué la cuarta potencia importa tanto")\
        .di("Si la temperatura absoluta se duplica, la potencia se multiplica por dieciséis.", M(r"(2T)^4=16\,T^4"))\
        .pregunta_rapida("Si T se triplica, H se multiplica por…", ["81", "9"], 0, "$3^4=81$.")\
        .di("Por eso siempre en kelvin: en Celsius la cuarta potencia da cualquier cosa.", OJO("$T(\\text{K})=T(^\\circ\\text{C})+273{,}15$ **antes** de elevar a la cuarta."))
    L.paso("Radiación neta con el ambiente")\
        .di("El cuerpo también recibe radiación del entorno. Lo que pierde de verdad es la diferencia.", F(r"H_{\text{neto}}=Ae\sigma\,(T^4-T_s^4)"))\
        .di("Un ejemplo: un radiador de un metro cuadrado a cuatrocientos kelvin en un cuarto a trescientos.", M(r"H_{\text{neto}}=(1)(0{,}85)(5{,}67\times10^{-8})(400^4-300^4)"))\
        .di("Unos ochocientos cuarenta watts.", R(r"H_{\text{neto}}\approx 843\ \text{W}"))
    return L.usa(r"H=Ae\sigma T^4", r"H=Ae\sigma(T^4-T_s^4)")


# ============================================================================ T-GAS
@leccion("T-GAS")
def t_gas() -> Leccion:
    L = _teo("T-GAS", "Teoría rápida: gas ideal, teoría cinética y C_V", "6.3", claves=["gas ideal", "rapidez rms", "grados de libertad"],
             enunciado="La ecuación del gas ideal, qué mide la temperatura y los valores de C_V que hay que saberse.",
             visual={"tipo": "gas", "modo": "temperatura"})
    L.paso("La ecuación del gas ideal")\
        .di("Une presión, volumen, moles y temperatura.", F(r"pV=nRT"))\
        .di("Ojo con las unidades: hay dos valores de erre.".replace("erre", s("R", "erre")), M(r"R=8{,}314\ \frac{\text{J}}{\text{mol·K}}\ \ (\text{Pa, m}^3)\qquad R=0{,}08206\ \frac{\text{L·atm}}{\text{mol·K}}"))\
        .di("Y la temperatura siempre en kelvin.", OJO("Con $R=8{,}314$: presión en Pa y volumen en m³. Con $0{,}08206$: atm y litros."))
    L.paso("Qué mide la temperatura")\
        .di("La temperatura mide qué tan rápido se mueven las moléculas, en promedio.", V(foco="presion"), F(r"\frac{1}{2}m\,(v^2)_{\text{med}}=\frac{3}{2}k_BT"))\
        .di("De ahí sale la rapidez cuadrática media.", F(r"v_{\text{rms}}=\sqrt{\frac{3RT}{M}}"))\
        .di(f"Más temperatura, más rapidez; más masa molar, menos rapidez.", NOTA("$M$ en **kg/mol**: oxígeno $0{,}032$, no 32."))
    L.paso("Promedio y rms con un ejemplo")\
        .di("Dos partículas: una a dos y otra a cuatro metros por segundo.", M(r"v_{\text{med}}=\frac{2+4}{2}=3\ \text{m/s}"))\
        .di("La rms eleva al cuadrado, promedia y saca raíz.", M(r"v_{\text{rms}}=\sqrt{\frac{2^2+4^2}{2}}=\sqrt{10}=3{,}16\ \text{m/s}"))\
        .di("La rms siempre es mayor o igual que el promedio.", M(r"v_{\text{rms}}\ge v_{\text{med}}"))
    L.paso("Ce ve: los valores que hay que saber".replace("Ce ve", "Cᵥ"))\
        .di("Vienen en el formulario y salen en casi todos los ciclos.", TABLA(["Gas", "$C_V$", "$C_p=C_V+R$", "$\\gamma=C_p/C_V$"],
                                                                               [["Monoatómico (He, Ar)", "$\\tfrac{3}{2}R$", "$\\tfrac{5}{2}R$", "$\\tfrac{5}{3}\\approx1{,}67$"],
                                                                                ["Diatómico (N₂, O₂, aire)", "$\\tfrac{5}{2}R$", "$\\tfrac{7}{2}R$", "$1{,}40$"]], resaltar=[[0, 3], [1, 3]]))\
        .pregunta_rapida("El helio es…", ["Monoatómico: γ = 1,67", "Diatómico: γ = 1,40"], 0, "El helio es un gas noble: átomos sueltos.")
    return L.usa(r"pV=nRT", r"v_{\text{rms}}=\sqrt{\frac{3RT}{M}}", r"C_p=C_V+R")


# ============================================================================ T-PV
@leccion("T-PV")
def t_pv() -> Leccion:
    g = Gas(1.0, "dia")
    g.pone("A", p=3e5, v=0.01)
    g.pone("B", p=3e5, v=0.02)
    g.pone("C", p=1.2e5, v=0.02)
    L = _teo("T-PV", "Teoría rápida: primera ley y procesos en p-V", "6.11", claves=["primera ley", "procesos", "diagrama p-V"],
             enunciado="La primera ley y cómo se calculan W, ΔU y Q en los cuatro procesos que salen en el examen.",
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "isocorico"), ("C", "A", "recta")], uv="L", up="kPa", fv=1e3, fp=1e-3))
    L.paso("La primera ley")\
        .di("El calor que entra se reparte: una parte sube la energía interna y otra se va como trabajo.", F(r"\Delta U=Q-W"))\
        .di("Los signos.", LISTA(["$Q>0$: entra calor · $Q<0$: sale", "$W>0$: el gas se expande · $W<0$: lo comprimen", "$\\Delta U$ depende solo de la temperatura"]))\
        .di("Y la energía interna siempre con ce ve, en cualquier proceso.".replace("ce ve", s("C_V")), F(r"\Delta U=nC_V\,\Delta T"))
    L.paso("El trabajo es el área bajo la curva")\
        .di("En el diagrama pe ve, el trabajo es el área entre la curva y el eje del volumen.".replace("pe ve", dice("p-V", "pe ve")), V(foco="AB"), F(r"W=\int_{V_1}^{V_2}p\,dV"))\
        .di("A presión constante es un rectángulo.", F(r"W=p\,(V_2-V_1)"))\
        .di("Si es una recta inclinada, es un trapecio.", M(r"W=\frac{p_1+p_2}{2}\,(V_2-V_1)"))
    L.paso("Los cuatro procesos")\
        .di("Cada uno tiene algo constante, y eso simplifica.", TABLA(["Proceso", "Constante", "$W$", "$\\Delta U$", "$Q$"],
                                                                        [["Isocórico", "$V$", "$0$", "$nC_V\\Delta T$", "$=\\Delta U$"],
                                                                         ["Isobárico", "$p$", "$p\\Delta V$", "$nC_V\\Delta T$", "$\\Delta U+W$"],
                                                                         ["Isotérmico", "$T$", "$nRT\\ln\\frac{V_2}{V_1}$", "$0$", "$=W$"],
                                                                         ["Adiabático", "$Q=0$", "$-\\Delta U$", "$nC_V\\Delta T$", "$0$"]]), V(foco="ciclo"))\
        .di("En la adiabática, además, se cumple pe ve a la gama constante.".replace("pe ve a la gama", dice("pVᵞ", "pe ve a la gama")), F(r"pV^\gamma=\text{cte}\qquad TV^{\gamma-1}=\text{cte}"))
    L.paso("Ciclos: la revisión que salva notas")\
        .di("En un ciclo el gas vuelve al inicio, así que la energía interna total no cambia.", M(r"\sum\Delta U=0\;\Rightarrow\;W_{\text{neto}}=Q_{\text{neto}}", "cic", "azul"))\
        .di("Si al sumar los delta U no da cero, hay un error.".replace("delta U", s("ΔU")), OJO("Haz siempre la tabla $Q$, $W$, $\\Delta U$ y revisa que $\\sum\\Delta U=0$."))\
        .pregunta_rapida("En una isoterma, ΔU vale…", ["0", "Q"], 0, "La energía interna depende solo de $T$, y $T$ no cambia.")
    return L.usa(r"\Delta U=Q-W", r"\Delta U=nC_V\Delta T", r"W=\int p\,dV", r"W=nRT\ln\left(\frac{V_2}{V_1}\right)")


# ============================================================================ T-MAQ
@leccion("T-MAQ")
def t_maq() -> Leccion:
    L = _teo("T-MAQ", "Teoría rápida: máquinas, refrigeradores y Otto", "6.14", claves=["eficiencia", "refrigerador", "Otto"],
             enunciado="Cómo se mide qué tan buena es una máquina térmica o un refrigerador.",
             visual={"tipo": "maquina", "QH": 1000, "QC": 650, "W": 350, "TH": 600, "TC": 300})
    L.paso("Una máquina térmica")\
        .di("Toma calor de lo caliente, saca trabajo y bota el resto a lo frío.", V(foco="balance"), M(r"Q_H=W+|Q_C|"))\
        .di("La eficiencia: lo que se aprovecha entre lo que se paga.", F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_C}{Q_H}\right|"))\
        .di("Un ejemplo: entran mil joules y salen trescientos cincuenta de trabajo.", M(r"e=\frac{350}{1000}=0{,}35=35\,\%"), V(foco="eficiencia"))
    L.paso("En un ciclo de varias etapas")\
        .di("Ce hache es la suma de todos los calores que entran, los positivos.".replace("Ce hache", s("Q_H", "cu hache")), M(r"Q_H=\sum Q_{\text{positivos}}"))\
        .di("Y el trabajo neto es la suma de todos los trabajos.", M(r"W_{\text{neto}}=\sum W"))\
        .di("Error típico: dividir entre el calor neto.", OJO("Abajo va solo el calor que **entra**, no el neto."))
    L.paso("Un refrigerador")\
        .di("Al revés: gasta trabajo para sacar calor de lo frío.", V(foco="eficiencia"), F(r"K=\frac{|Q_C|}{|W|}"))\
        .di("Ka puede ser mayor que uno: saca más calor que el trabajo que gasta.".replace("Ka", s("K", "ka")), M(r"|Q_C|=600\ \text{J},\ |W|=200\ \text{J}\;\Rightarrow\;K=3"))\
        .pregunta_rapida("¿Puede K valer 3?", ["Sí", "No, debe ser menor que 1"], 0, "$K$ no es una eficiencia en porcentaje: puede ser mayor que 1.")
    L.paso("El ciclo Otto (motor de gasolina)")\
        .di("Su eficiencia depende solo de cuánto se comprime la mezcla.", F(r"e_{\text{Otto}}=1-\frac{1}{r^{\gamma-1}}"), F(r"r=\frac{V_{\text{máx}}}{V_{\text{mín}}}"))\
        .di("Un ejemplo con aire y compresión ocho.", M(r"e=1-\frac{1}{8^{0{,}40}}=1-0{,}435=0{,}565"))\
        .di("Más compresión, más eficiencia, pero cada vez gana menos.", NOTA("Por eso los motores de alta compresión necesitan gasolina de más octanaje (E21)."))
    return L.usa(r"e=\frac{W}{Q_H}", r"K=\frac{|Q_C|}{|W|}", r"e_{\text{Otto}}=1-\frac{1}{r^{\gamma-1}}")


# ============================================================================ T-ENT
@leccion("T-ENT")
def t_ent() -> Leccion:
    L = _teo("T-ENT", "Teoría rápida: entropía y segunda ley", "6.16", claves=["entropía", "reversible", "irreversible"],
             aviso="El formulario II-2026 no trae fórmulas de entropía: si aparece, hay que armarla desde dS = dQ/T.",
             enunciado="Qué es la entropía, cómo se calcula y qué dice la segunda ley.",
             visual={"tipo": "entropia", "partes": [{"n": "caliente cede", "dS": -20}, {"n": "frío recibe", "dS": 30}]})
    L.paso("La definición")\
        .di("La entropía cambia con el calor dividido entre la temperatura a la que entra.", F(r"dS=\frac{dQ}{T}"))\
        .di("Temperatura en kelvin, siempre.", OJO("$T$ en kelvin: dividir entre $0\\ ^\\circ\\text{C}$ no tiene sentido."))
    L.paso("Dos casos que salen siempre")\
        .di("Si la temperatura no cambia, en un cambio de fase o un depósito grande, es solo dividir.", M(r"\Delta S=\frac{Q}{T}"))\
        .di("Si la temperatura cambia, se integra con de cu igual a eme ce de te.".replace("de cu igual a eme ce de te", dice("dQ = mc dT", "de cu igual a eme ce de te")), M(r"\Delta S=\int_{T_i}^{T_f}\frac{mc\,dT}{T}=mc\ln\frac{T_f}{T_i}"))\
        .di("En un gas: con ce ve a volumen constante, con ce pe a presión constante.".replace("ce ve", s("C_V")).replace("ce pe", s("C_p")), M(r"\Delta S=nC_V\ln\frac{T_2}{T_1}\qquad \Delta S=nC_p\ln\frac{T_2}{T_1}"))
    L.paso("La segunda ley")\
        .di("La entropía del universo nunca baja.", V(foco="total"), M(r"\Delta S_{\text{universo}}\ge 0", "seg", "azul"))\
        .di("Cero en un proceso reversible; mayor que cero en uno real.", LISTA(["$=0$: reversible (ideal)", "$>0$: irreversible (todo lo real: mezclar, frenar, enfriar)"]))\
        .pregunta_rapida("Al mezclar agua caliente y fría, ΔS del universo es…", ["Mayor que cero", "Cero"], 0, "Mezclar es irreversible: no se separan solas.")
    return L.usa(r"\Delta S=\int\frac{dQ}{T}")
