"""Mini-lecciones de teoría animada para la Ruta del examen (T-DIL a T-ENT)."""

from __future__ import annotations

from .ayudas import Gas, visual_pv
from .dsl import F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .registro import leccion


def _teo(id: str, titulo: str, tema: str, **k) -> Leccion:
    return Leccion(id, titulo, tema=tema, tipo="teoria", nivel=1, fuente={"documento": "Formulario oficial II-2026"}, **k)


@leccion("T-DIL")
def t_dil() -> Leccion:
    L = _teo("T-DIL", "Teoría rápida: dilatación térmica", "4.4", claves=["dilatación", "alfa", "beta"],
             visual={"tipo": "dilatacion", "caso": "barra", "L0": 2.0, "a": 2.4e-5, "T0": 20, "dT": 80, "nombre": "barra metálica"})
    L.paso("Lineal, superficial y volumétrica")\
        .di("Al calentar un sólido o líquido, todas sus dimensiones crecen en proporción al tamaño inicial y al cambio de temperatura.", V(foco="calentar"),
            F(r"\Delta L = \alpha L_0\,\Delta T"),
            F(r"\Delta A = 2\alpha A_0\,\Delta T"),
            F(r"\Delta V = \beta V_0\,\Delta T \qquad (\beta \approx 3\alpha\ \text{en sólidos isotrópicos})"))
    L.paso("Dos trampas típicas de examen")\
        .di("Un agujero en una placa se agranda al calentarse como si estuviera relleno del mismo material.",
            LISTA(["Un **hueco** crece con el mismo $\\alpha$ del metal que lo rodea",
                   "Un **líquido en recipiente** desborda según $(\\beta_{\\text{liq}} - \\beta_{\\text{rec}})V_0\\Delta T$",
                   "En $\\Delta T$ da igual usar °C o K porque es una resta"]),
            OJO("Nunca restes temperaturas en Celsius y luego sumes 273: $\\Delta T(\\text{K}) = \\Delta T(^\\circ\\text{C})$."))
    return L.usa(r"\Delta L=\alpha L_0\Delta T", r"\Delta V=\beta V_0\Delta T")


@leccion("T-CAL")
def t_cal() -> Leccion:
    L = _teo("T-CAL", "Teoría rápida: calorimetría y cambios de fase", "4.8", claves=["calor específico", "calor latente", "equilibrio"],
             visual={"tipo": "calentamiento", "m": 0.2, "T0": -20, "T1": 120})
    L.paso("Calor sensible vs calor latente")\
        .di("Cuando cambia la temperatura usamos eme ce delta te; cuando cambia la fase a temperatura fija usamos eme por ele.",
            F(r"Q_{\text{sensible}} = m c\,\Delta T"),
            F(r"Q_{\text{fase}} = \pm m L \qquad (L_f\ \text{fusión},\; L_v\ \text{vaporización})"))
    L.paso("Balance en un calorímetro aislado")\
        .di("Si nada se escapa al ambiente, la suma de todos los calores con signo vale cero.",
            F(r"\sum Q = 0"),
            OJO("Antes de despejar $T_f$ con hielo o vapor, **comprobá primero** si alcanza el calor disponible para fundir o condensar todo."))
    return L.usa(r"Q=mc\Delta T", r"Q=\pm mL", r"\sum Q=0")


@leccion("T-CON")
def t_con() -> Leccion:
    L = _teo("T-CON", "Teoría rápida: conducción y ley de Fourier", "5.4", claves=["Fourier", "serie", "paralelo", "cilindro", "esfera"],
             visual={"tipo": "conduccion", "geo": "cilindro", "r1": 0.04, "r2": 0.08, "L": 1.0, "T1": 100, "T2": 20, "k": 50, "uT": "°C"})
    L.paso("De la pared plana a geometrías con integral")\
        .di("En estado estacionario la corriente de calor es constante en toda sección transversal.", V(foco="dr"),
            F(r"H = -k A\,\frac{dT}{dx}"),
            TABLA(["Geometría", "Área $A$", "Flujo $H$"],
                  [["Pared plana", "$A$ constante", "$kA(T_1-T_2)/L$"],
                   ["Cilindro radial", "$2\\pi r L$", "$2\\pi kL(T_1-T_2)/\\ln(r_2/r_1)$"],
                   ["Esfera radial", "$4\\pi r^2$", "$4\\pi k(T_1-T_2)/(1/r_1-1/r_2)$"]]))
    L.paso("Serie, paralelo y cuando k o A varían")\
        .di("En serie pasa el mismo flujo y se suman las resistencias; en paralelo se suman los flujos.", V(foco="perfil"),
            LISTA(["**Serie**: mismo $H$, $\\Delta T$ mayor donde $k$ es menor",
                   "**Paralelo**: mismo $\\Delta T$, $H_{\\text{total}} = H_1 + H_2$",
                   "**$A(x)$ o $k(x)$ variable**: separa variables e integra $H\\int\\frac{dx}{kA}=-\\int dT$"]))
    return L.usa(r"H=-kA\frac{dT}{dx}")


@leccion("T-RAD")
def t_rad() -> Leccion:
    L = _teo("T-RAD", "Teoría rápida: radiación y Stefan-Boltzmann", "5.9", claves=["radiación", "Stefan-Boltzmann", "emisividad"],
             visual={"tipo": "radiacion", "T": 400, "Ts": 300, "e": 0.85, "objeto": "radiador"})
    L.paso("Potencia radiada y potencia neta")\
        .di("Todo cuerpo emite radiación proporcional a la cuarta potencia de su temperatura absoluta en kelvin.",
            F(r"H = A e \sigma T^4 \qquad \sigma = 5{,}67\times 10^{-8}\ \frac{\text{W}}{\text{m}^2\cdot\text{K}^4}"),
            F(r"H_{\text{neto}} = A e \sigma\,(T^4 - T_s^4)"),
            OJO("Siempre convierte $T$ y $T_s$ a **kelvin** antes de elevar a la cuarta potencia."))
    return L.usa(r"H=Ae\sigma T^4", r"H_{\text{neto}}=Ae\sigma(T^4-T_s^4)")


@leccion("T-GAS")
def t_gas() -> Leccion:
    L = _teo("T-GAS", "Teoría rápida: gas ideal, teoría cinética y C_V", "6.3", claves=["gas ideal", "rapidez rms", "grados de libertad"],
             visual={"tipo": "gas", "modo": "temperatura"})
    L.paso("Ecuación de estado y energía cinética")\
        .di("La temperatura absoluta mide directamente la energía cinética traslacional media por molécula.", V(foco="presion"),
            F(r"pV = nRT \qquad K_{\text{med}} = \frac{3}{2}k_B T \qquad v_{\text{rms}} = \sqrt{\frac{3RT}{M}}"))
    L.paso("Capacidades caloríficas según el tipo de gas")\
        .di("Memoriza los dos casos del examen: monoatómico y diatómico.",
            TABLA(["Gas ideal", "$C_V$", "$C_p = C_V+R$", "$\\gamma = C_p/C_V$"],
                  [["Monoatómico (He, Ar)", "$\\tfrac{3}{2}R$", "$\\tfrac{5}{2}R$", "$\\tfrac{5}{3} \\approx 1{,}67$"],
                   ["Diatómico ($\\text{N}_2, \\text{O}_2$)", "$\\tfrac{5}{2}R$", "$\\tfrac{7}{2}R$", "$\\tfrac{7}{5} = 1{,}40$"]], resaltar=[[0, 3], [1, 3]]))
    return L.usa(r"pV=nRT", r"v_{\text{rms}}=\sqrt{\frac{3RT}{M}}", r"C_p=C_V+R")


@leccion("T-PV")
def t_pv() -> Leccion:
    g = Gas(1.0, "dia")
    g.pone("A", p=3e5, v=0.01)
    g.pone("B", p=3e5, v=0.02)
    g.pone("C", p=1.2e5, v=0.02)
    L = _teo("T-PV", "Teoría rápida: primera ley y procesos en p-V", "6.11", claves=["primera ley", "procesos", "diagrama p-V"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "isocorico"), ("C", "A", "recta")], uv="L", up="kPa", fv=1e3, fp=1e-3))
    L.paso("Primera ley y los 4 procesos clave")\
        .di("En cualquier proceso de un gas ideal la energía interna usa ce ve, y el trabajo es el área bajo la curva.",
            F(r"\Delta U = Q - W \qquad \Delta U = nC_V\Delta T"),
            TABLA(["Proceso", "Constante", "$W$", "$Q$"],
                  [["Isocórico", "$V$", "$0$", "$nC_V\\Delta T$"],
                   ["Isobárico", "$p$", "$p\\Delta V$", "$nC_p\\Delta T$"],
                   ["Isotérmico", "$T$", "$nRT\\ln(V_2/V_1)$", "$Q = W$"],
                   ["Adiabático", "$Q=0$", "$-\\Delta U$", "$0$"]]))
    return L.usa(r"\Delta U=Q-W", r"\Delta U=nC_V\Delta T")


@leccion("T-MAQ")
def t_maq() -> Leccion:
    L = _teo("T-MAQ", "Teoría rápida: máquinas, refrigeradores y Otto", "6.14", claves=["eficiencia", "refrigerador", "Otto"],
             visual={"tipo": "maquina", "QH": 1000, "QC": 650, "W": 350, "TH": 600, "TC": 300})
    L.paso("Motor vs refrigerador y ciclo Otto")\
        .di("En un diagrama p-V, el recorrido horario entrega trabajo (motor) y el antihorario consume trabajo (refrigerador).", V(foco="eficiencia"),
            F(r"e = \frac{W_{\text{neto}}}{Q_H} = 1 - \frac{|Q_C|}{Q_H} \qquad K = \frac{|Q_C|}{|W_{\text{neto}}|}"),
            F(r"e_{\text{Otto}} = 1 - \frac{1}{r^{\gamma-1}}"),
            NOTA("Si un ciclo tiene varias etapas donde entra calor, $Q_H$ es la **suma de todos los calores positivos**."))
    return L.usa(r"e=\frac{W}{Q_H}", r"K=\frac{|Q_C|}{|W|}", r"e_{\text{Otto}}=1-\frac{1}{r^{\gamma-1}}")


@leccion("T-ENT")
def t_ent() -> Leccion:
    L = _teo("T-ENT", "Teoría rápida: entropía y segunda ley", "6.16", claves=["entropía", "reversible", "irreversible"],
             aviso="El formulario oficial II-2026 no incluye fórmulas de entropía; úsalo solo como respaldo conceptual.",
             visual={"tipo": "entropia", "partes": [{"n": "caliente cede", "dS": -20}, {"n": "frío recibe", "dS": 30}]})
    L.paso("Definición y signo en el universo")\
        .di("La entropía cambia según calor reversible dividido entre temperatura absoluta en kelvin.", V(foco="total"),
            F(r"\Delta S = \int \frac{dQ_{\text{rev}}}{T}"),
            LISTA(["A temperatura fija (fase o reservorio): $\\Delta S = Q/T$",
                   "Calentando sólido o líquido: $\\Delta S = mc\\ln(T_f/T_i)$",
                   "Segunda ley: $\\Delta S_{\\text{universo}} \\ge 0$ ($0$ reversible, $>0$ irreversible)"]))
    return L.usa(r"\Delta S=\int\frac{dQ}{T}")
