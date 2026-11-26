"""Guía pedagógica y forense de las 42 casillas del formulario oficial FI2103 II-2026.

Fuente académica y forense:
  - Hoja oficial: `documentos-ii-2026_Ecuaciones_Tercer_Parcial_FG3.pdf` (14 × 3 = 42 celdas).
  - Auditoría verificada sobre los 17 exámenes/reposiciones (II-2023 a I-2026), las 2 prácticas
    II-2026 y el banco de 207 lecciones (`docs/VERIFICACION_PATRONES_II_2026.md`).

Cada una de las 42 casillas documenta:
  * qué es en lenguaje físico claro,
  * cada letra y sus unidades SI (apoyado en `simbolos.py`),
  * dónde y cuándo SÍ se usa,
  * cuándo NO se debe usar,
  * errores típicos, cuidados y confusiones con casillas gemelas,
  * derivaciones permitidas desde esa casilla base,
  * pistas del enunciado e historial real de patrones de examen,
  * enlaces automáticos a lecciones verificadas del banco para practicar en la pizarra.
"""

from __future__ import annotations

import threading
from typing import Any, Dict, List, Tuple

from . import registro, simbolos
from .formulario_ii_2026 import catalogo_formulario, mapear_formulario_leccion

# Familias temáticas en el orden visual de la hoja (de arriba hacia abajo)
BLOQUES_FORMULARIO: Tuple[Dict[str, str], ...] = (
    {"id": "dilatacion", "nombre": "Dilatación y esfuerzo térmico", "color": "azul"},
    {"id": "conduccion", "nombre": "Conducción de calor", "color": "rojo"},
    {"id": "radiacion", "nombre": "Radiación térmica", "color": "naranja"},
    {"id": "calorimetria", "nombre": "Calorimetría y cambios de fase", "color": "turquesa"},
    {"id": "gases", "nombre": "Gases ideales y teoría cinética", "color": "morado"},
    {"id": "trabajo", "nombre": "Trabajo termodinámico", "color": "verde"},
    {"id": "primera_ley", "nombre": "Primera ley y calores molares", "color": "azul"},
    {"id": "adiabaticos", "nombre": "Procesos adiabáticos", "color": "rojo"},
    {"id": "maquinas", "nombre": "Máquinas, refrigeradores y Otto", "color": "naranja"},
    {"id": "escalas", "nombre": "Escalas de temperatura", "color": "turquesa"},
    {"id": "constantes", "nombre": "Constantes y conversiones", "color": "morado"},
)

# Símbolos explícitos para las 7 constantes/conversiones (F12C3..F14C3) donde las letras son fijas
_SIMBOLOS_CONSTANTES: Dict[str, List[List[str]]] = {
    "F12C3": [["R", "constante universal de los gases ideales en SI", "J/(mol·K)"]],
    "F13C1": [["k_B", "constante de Boltzmann (R / N_A, por molécula)", "J/K"]],
    "F13C2": [["N_A", "número de Avogadro (entidades por cada mol)", "mol⁻¹"]],
    "F13C3": [["σ", "constante de Stefan–Boltzmann para radiación térmica", "W/(m²·K⁴)"]],
    "F14C1": [["atm", "atmósfera estándar de presión", "1 atm = 101 325 Pa"]],
    "F14C2": [["L", "litro de volumen (equivale a 10⁻³ m³)", "1 L = 1000 cm³"]],
    "F14C3": [["R", "constante de los gases solo si p está en atm y V en L", "L·atm/(mol·K)"]],
}


def _c(
    *,
    bloque: str,
    estado: str,
    prioridad: str,
    que_es: str,
    cuando_si: List[str],
    cuando_no: List[str],
    unidades: str,
    errores: List[str],
    confundir_con: List[Tuple[str, str]],
    se_deriva: List[Tuple[str, str]],
    pistas: List[str],
    en_formularios: str,
    parte_examen: str,
    historial: str,
) -> Dict[str, Any]:
    return {
        "bloque": bloque,
        "estado_ii_2026": estado,
        "prioridad": prioridad,
        "que_es": que_es,
        "cuando_si": cuando_si,
        "cuando_no": cuando_no,
        "unidades": unidades,
        "errores": errores,
        "confundir_con": [{"id": cid, "motivo": mot} for cid, mot in confundir_con],
        "se_deriva": [{"tex": tx, "como": cm} for tx, cm in se_deriva],
        "pistas_enunciado": pistas,
        "patron_examen": {
            "en_formularios": en_formularios,
            "parte": parte_examen,
            "historial": historial,
        },
    }


GUIA_42_CELDAS: Dict[str, Dict[str, Any]] = {
    # ========================================================== FILA 1: DILATACIÓN Y ESFUERZO
    "F1C1": _c(
        bloque="dilatacion",
        estado="fija",
        prioridad="alta",
        que_es="Cambio de longitud ΔL de una barra, riel, diámetro o agujero en un sólido al variar su temperatura.",
        cuando_si=[
            "Barras, cables, rieles, cintas métricas o agujeros/anillos en sólidos isotrópicos que se calientan o enfrían.",
            "Cualquier problema de área (ΔA) o volumen de sólido (β = 3α) cuando debas derivarlo desde la hoja.",
            "Sistemas con dos barras que mantienen una diferencia constante de largo o que se pandean.",
        ],
        cuando_no=[
            "NO usar directamente para líquidos (no tienen largo propio; usa F1C2 con β del líquido).",
            "NO usar como si fuera esfuerzo mecánico cuando la barra está bloqueada entre muros (usa F1C3).",
            "NO usar con cambios extremos si α(T) depende de la temperatura sin integrar dL = α(T) L₀ dT.",
        ],
        unidades="ΔL y L₀ en la misma unidad (m, cm o mm); α en °C⁻¹ o K⁻¹; ΔT en °C o K (una resta vale igual en ambas).",
        errores=[
            "Un agujero o anillo se dilata IGUAL que el material macizo que lo rodea (al calentar, el hueco crece, NO se encoge).",
            "En cintas métricas metálicas, si la cinta también se dilata, cambia el tamaño de cada división.",
            "No sacar ΔA = 2αA₀ΔT ni β = 3α de la nada en desarrollo: dedúcelas elevando L = L₀(1 + αΔT) al cuadrado o al cubo.",
        ],
        confundir_con=[
            ("F1C2", "F1C2 es para volumen (3D, usa β); F1C1 es para una sola dimensión (largo, radio o diámetro, usa α)."),
            ("F1C3", "Si los extremos están rígidamente sujetos e impiden el cambio ΔL, aparece esfuerzo térmico (F1C3)."),
        ],
        se_deriva=[
            (r"\Delta A \approx 2\alpha A_0\Delta T", "Eleva L = L₀(1 + αΔT) al cuadrado y desprecia (αΔT)²."),
            (r"\beta = 3\alpha", "Eleva L = L₀(1 + αΔT) al cubo y desprecia términos cuadráticos y cúbicos."),
        ],
        pistas=[
            "barra / riel / cinta métrica / esfera pasa por un anillo",
            "diferencia constante de longitudes a cualquier temperatura",
            "coeficiente de expansión lineal α",
        ],
        en_formularios="6/17 previos (23-II ×3, 24-I-R, 26-I-E, 26-I-R) · Fija en II-2026",
        parte_examen="Parte I #1 (conceptual 2 pts) o Parte III (desarrollo 8 pts)",
        historial="En I-2026 salió en desarrollo de 8 pts tanto en ordinario como en extraordinario; además abre casi siempre la Parte I conceptual.",
    ),
    "F1C2": _c(
        bloque="dilatacion",
        estado="reintroducida",
        prioridad="alta",
        que_es="Cambio de volumen ΔV de un líquido o de un sólido (o cavidad interior) al cambiar su temperatura.",
        cuando_si=[
            "Líquidos dentro de recipientes (derrame de combustible, mercurio en vidrio, lata llena que se calienta).",
            "Expansión volumétrica de sólidos cuando te dan β directamente o tras derivar β = 3α desde F1C1.",
        ],
        cuando_no=[
            "NO usar para gases ideales (en gases p, V y T dependen entre sí por F4C1: pV = nRT).",
            "NO olvidar que el recipiente TAMBIÉN se dilata: el derrame es ΔV_líquido − ΔV_recipiente.",
        ],
        unidades="ΔV y V₀ en la misma unidad (m³, L, mL o cm³); β en °C⁻¹ o K⁻¹; ΔT en °C o K.",
        errores=[
            "Olvidar restar la expansión del recipiente: si solo calculas ΔV del líquido, sobreestimas cuánto se derrama.",
            "Confundir α con β del recipiente: para un sólido isotrópico β_recipiente = 3α (y debes mostrar esa derivación desde F1C1).",
            "En agua cerca de 0–4 °C la expansión es anómala (se contrae al subir de 0 a 4 °C).",
        ],
        confundir_con=[
            ("F1C1", "F1C1 mide longitud o diámetro (α); F1C2 mide volumen (β = 3α en sólidos)."),
            ("F4C1", "Para gases ideales nunca uses ΔV = βV₀ΔT; usa la ley del gas ideal pV = nRT."),
        ],
        se_deriva=[
            (r"\Delta V_{\text{derrame}} = (\beta_{\text{líq}} - 3\alpha_{\text{rec}})V_0\Delta T", "Resta el cambio de volumen del recipiente al del líquido."),
        ],
        pistas=[
            "recipiente lleno hasta el borde / cuánto líquido se derrama",
            "coeficiente de expansión volumétrica β",
            "tanque de gasolina / frasco de vidrio con mercurio o agua",
        ],
        en_formularios="4/17 previos (23-II ×3, 24-I-R) · Reintroducida en II-2026",
        parte_examen="Parte I #1 (conceptual 2 pts), Parte II (cálculo 3 pts) o Parte III (8 pts)",
        historial="Volvió a la hoja tras 4 semestres ausente en parciales ordinarios (sin ΔA = 2αA₀ΔT). Alta señal de que puede evaluarse combinada con F1C1.",
    ),
    "F1C3": _c(
        bloque="dilatacion",
        estado="nueva",
        prioridad="critica",
        que_es="Esfuerzo mecánico F/A (presión interna de compresión o tensión) que aparece cuando los soportes impiden que una barra se dilate o contraiga.",
        cuando_si=[
            "Barra, viga, riel o tornillo rígidamente sujeto en ambos extremos (sin holgura) que cambia de temperatura.",
            "Cuando piden fuerza sobre muros, tipo de esfuerzo (tensión vs. compresión) o ΔT máximo antes de superar un esfuerzo límite.",
        ],
        cuando_no=[
            "NO usar si al menos un extremo está libre para moverse: sin restricción, ΔL ocurre libremente y el esfuerzo térmico es cero.",
            "NO usar si la barra tiene un espacio libre (holgura) mayor que αL₀ΔT (primero hay que cerrar la holgura).",
        ],
        unidades="Y en Pa (N/m²), α en °C⁻¹ o K⁻¹, ΔT en °C o K → F/A sale en Pa; para hallar fuerza F = (F/A)·A convierte A de cm² a m² (×10⁻⁴).",
        errores=[
            "Signo clave: al CALENTAR (ΔT > 0) la barra intenta crecer y los muros la empujan → F/A < 0 (COMPRESIÓN). Al ENFRIAR (ΔT < 0) intenta encogerse y los muros la jalan → F/A > 0 (TENSIÓN).",
            "Creer que el esfuerzo F/A depende del área o del largo: F/A solo depende de Y, α y ΔT; la FUERZA F = (F/A)·A sí depende del área A.",
            "Olvidar convertir cm² a m² (1 cm² = 10⁻⁴ m²) al multiplicar por A para sacar F.",
        ],
        confundir_con=[
            ("F1C1", "F1C1 da cuánto crecería libremente (ΔL); F1C3 da el esfuerzo cuando se impide ese ΔL."),
        ],
        se_deriva=[
            (r"F = -Y\alpha A\,\Delta T", "Multiplica ambos lados de F/A = −YαΔT por el área transversal A."),
            (r"\Delta T_{\text{máx}} = \frac{|F/A|_{\text{máx}}}{Y\alpha}", "Despeja el cambio máximo de temperatura antes de fractura o pandeo."),
        ],
        pistas=[
            "rígidamente sujeta en ambos extremos / entre dos muros fijos",
            "módulo de Young Y / esfuerzo de compresión o tensión",
            "sin cambiar de longitud / temperatura máxima admisible",
        ],
        en_formularios="0/17 previos · 100 % NUEVA en II-2026",
        parte_examen="Parte I #1 (conceptual 2 pts, como PR2-S13) o Parte II/III (cálculo 3–8 pts)",
        historial="Inédita en los 17 exámenes 2023–2026 y ausente del Folleto, pero agregada deliberadamente en la hoja II-2026 y ya incluida en la práctica del profesor (PR2-S13).",
    ),
    # ========================================================== FILA 2: CONDUCCIÓN Y RADIACIÓN
    "F2C1": _c(
        bloque="conduccion",
        estado="fija",
        prioridad="critica",
        que_es="Corriente o flujo de calor H (en watts = J/s) por conducción a través de una pared plana o barra uniforme en estado estacionario.",
        cuando_si=[
            "Pared plana, ventana o barra recta con área A y conductividad k CONSTANTES de extremo a extremo.",
            "Capas planas en serie (mismo H, sumas caídas ΔT = H·L/(kA)) o en paralelo (misma ΔT, sumas H_total = H₁ + H₂).",
            "Relacionar potencia H con energía total transferida en un tiempo Δt: Q = H·Δt (por ejemplo para fundir hielo o condensar vapor).",
        ],
        cuando_no=[
            "PROHIBIDO en cilindros/tuberías (A = 2πrL varía con r), esferas (A = 4πr²), troncos de cono (A = πr(x)²) o barras con A(x) o k(x) variable → usa F2C2 e integra.",
            "NO usar sin derivar la resistencia térmica R_th = L/(kA): parte siempre de F2C1 y despeja T_H − T_C = H·L/(kA).",
        ],
        unidades="k en W/(m·K), A en m², L en m, T_H − T_C en K o °C → H en W (J/s). Tiempo Δt SIEMPRE en segundos (1 h = 3600 s).",
        errores=[
            "Error del folleto 3.3-9: sustituir cm² como si fueran m² (divide entre 10⁴, NO entre 100).",
            "Confundir serie con paralelo: en SERIE el calor cruza una capa tras otra (mismo H, se suman L/(kA)); en PARALELO el calor se divide por dos caminos (misma ΔT, se suman H₁ + H₂).",
            "Olvidar pasar horas o minutos a segundos al usar Q = H·Δt.",
        ],
        confundir_con=[
            ("F2C2", "Si el área A o la conductividad k cambian con la posición (cono, cilindro, esfera, A(x), k(x)), F2C1 falla: usa F2C2."),
            ("F2C3", "F2C1 es conducción a través de un material en contacto; F2C3 es radiación electromagnética (no necesita medio)."),
        ],
        se_deriva=[
            (r"Q = H\,\Delta t", "Con flujo estacionario constante, integra H = dQ/dt en el intervalo Δt."),
            (r"T_H - T_C = H\left(\frac{L_1}{k_1 A} + \frac{L_2}{k_2 A}\right)", "Despeja cada caída de temperatura desde F2C1 y suma (capas planas en serie)."),
        ],
        pistas=[
            "pared compuesta de dos o tres capas planas / temperatura de interfaz",
            "dos barras en serie o en paralelo / energía transferida en horas",
            "flujo de calor H constante en estado estacionario",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (cálculo 3 pts) o Parte III (desarrollo 8–9 pts, ej. PR2-D3)",
        historial="Conducción apareció en 16/17 exámenes históricos. Cuando no pide integral, aparece como pared de capas planas o barras en serie/paralelo.",
    ),
    "F2C2": _c(
        bloque="conduccion",
        estado="fija",
        prioridad="critica",
        que_es="Ley de Fourier en forma diferencial: conecta el flujo de calor H con el gradiente local de temperatura dT/dx (o dT/dr) en cualquier geometría.",
        cuando_si=[
            "Geometrías donde el área cambia con la posición: tronco de cono (A = πr(x)²), cilindro/tubería (A = 2πrL), esfera (A = 4πr²), barra piramidal o exponencial A(x).",
            "Materiales con conductividad variable k(x), k(y) o k(r).",
            "Cuando preguntan dónde es mayor el gradiente |dT/dx| = H/(kA(x)) o piden el perfil de temperatura T(x) o T(r).",
        ],
        cuando_no=[
            "NO hace falta integrar si la pared es plana con A y k constantes (ahí F2C2 se reduce directamente a F2C1).",
            "PROHIBIDO escribir de memoria las fórmulas cilíndrica, esférica o de cono truncado sin partir primero de F2C2, separar variables e integrar.",
        ],
        unidades="k en W/(m·K), A en m², dx o dr en m, dT en K o °C → H en W. Gradiente |dT/dx| en K/m o °C/m.",
        errores=[
            "Error real del solucionario 25-II-E P6: en un cilindro el área que atraviesa el calor radial es el área LATERAL A = 2πrL, JAMÁS el volumen πr²L.",
            "Olvidar el signo menos: como el calor fluye hacia donde baja T, dT/dx < 0, y el menos hace H > 0.",
            "Para hallar T(x) en un punto intermedio (como x = L/2), repite la integral solo de 0 a x (NO promedies (T₁+T₂)/2; solo es lineal si A y k son constantes).",
            "Donde el área A(x) es MÁS ANGOSTA, el gradiente |dT/dx| = H/(kA) es MÁS GRANDE (ahí cae más rápido la temperatura).",
        ],
        confundir_con=[
            ("F2C1", "F2C1 solo sirve con A y k constantes; F2C2 es obligatoria en cualquier barra variable, cilindro, esfera o cono."),
        ],
        se_deriva=[
            (r"H = \frac{2\pi k L (T_1 - T_2)}{\ln(r_2/r_1)}", "Sustituye A(r) = 2πrL en F2C2, separa dr/r e integra de r₁ a r₂ (cilindro)."),
            (r"H = \frac{4\pi k r_1 r_2 (T_1 - T_2)}{r_2 - r_1}", "Sustituye A(r) = 4πr² en F2C2, separa dr/r² e integra de r₁ a r₂ (esfera)."),
            (r"H = \frac{\pi k r_1 r_2 (T_1 - T_2)}{L}", "Sustituye A(x) = π(r₁ + bx)² en F2C2, haz u = r₁ + bx e integra (cono truncado)."),
        ],
        pistas=[
            "tronco de cono / barra piramidal / A(x) = A₀(1+x/L) / A₀ e^(−x/L)",
            "tubería cilíndrica con capas aislantes / cascarón esférico",
            "conductividad variable k(r) o k(y) / demuestre H / perfil T(x) / mayor gradiente",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte III o IV (desarrollo con integral, 9–16 pts)",
        historial="Conducción con integración apareció en 7 de los últimos 10 parciales (I-2024..I-2026), más 25-II-R (k(y)) y en ambas prácticas II-2026 (PR1-3, PR1-4, PR2-D1, PR2-D2, PR2-D10).",
    ),
    "F2C3": _c(
        bloque="radiacion",
        estado="fija",
        prioridad="alta",
        que_es="Potencia total de radiación electromagnética emitida por una superficie a temperatura absoluta T (ley de Stefan–Boltzmann).",
        cuando_si=[
            "El enunciado pide únicamente la potencia EMITIDA o IRRADIADA por un cuerpo (estrella, filamento, cuerpo negro aislado) sin mencionar temperatura ambiente T_s.",
            "Comparaciones de razón de potencia al cambiar T (si T se duplica en K, H se multiplica por 2⁴ = 16, como PR2-S10).",
        ],
        cuando_no=[
            "NO usar cuando el objeto intercambia radiación con un recinto o ambiente a temperatura T_s conocida y piden potencia NETA o rapidez de enfriamiento (usa F3C1).",
            "PROHIBIDO sustituir T en °C: T DEBE estar en kelvin (T_K = T_°C + 273,15).",
        ],
        unidades="A en m² (esfera: 4πr², cilindro lateral: 2πrL), e adimensional (0 a 1; cuerpo negro e = 1), σ = 5,67×10⁻⁸ W/(m²·K⁴), T SIEMPRE en K → H en W.",
        errores=[
            "Metida de pata #1: sustituir grados Celsius en T⁴ (por ejemplo 100⁴ en vez de 373,15⁴). Duplicar °C NO multiplica H por 16.",
            "Confundir la letra e de emisividad (0 ≤ e ≤ 1) con eficiencia térmica de F10C1 o el número de Euler 2,718.",
            "Olvidar el área superficial completa: una esfera tiene A = 4πr² (no πr²); una placa delgada con dos caras tiene 2A.",
        ],
        confundir_con=[
            ("F3C1", "F2C3 es solo lo que el cuerpo EMITE; F3C1 es la potencia NETA restando lo que absorbe del ambiente T_s."),
            ("F10C1", "No confundas emisividad e (radiación) con eficiencia térmica e (máquinas)."),
        ],
        se_deriva=[
            (r"\frac{H_2}{H_1} = \left(\frac{T_2}{T_1}\right)^4", "Divide F2C3 en dos estados con igual área A y emisividad e."),
            (r"T = \left(\frac{H}{Ae\sigma}\right)^{1/4}", "Despeja T sacando raíz cuarta (en calculadora: potencia 0,25)."),
        ],
        pistas=[
            "cuerpo negro / emisividad e / filamento / estrella",
            "potencia irradiada o emitida / si la temperatura en K se duplica",
            "constante de Stefan–Boltzmann",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I/II (selección 2–3 pts, ej. PR2-S10) o Parte III (desarrollo anunciado por el profe)",
        historial="El profesor anunció explícitamente que en II-2026 el desarrollo corto será de calorimetría o radiación; domina tanto F2C3 como el despeje con raíz cuarta.",
    ),
    # ========================================================== FILA 3: RADIACIÓN NETA Y CALORIMETRÍA
    "F3C1": _c(
        bloque="radiacion",
        estado="fija",
        prioridad="critica",
        que_es="Potencia neta de radiación intercambiada entre un cuerpo a temperatura T y sus alrededores a temperatura T_s.",
        cuando_si=[
            "Objeto caliente o frío dentro de una habitación, horno o recinto a temperatura ambiente T_s.",
            "Cálculo de energía neta perdida en un tiempo Δt (Q = H·Δt), rapidez inicial de enfriamiento o despeje de T con raíz cuarta (P26-4, P26-5).",
        ],
        cuando_no=[
            "NO usar (T − T_s)⁴: eso es matemáticamente FALSO; se resta T⁴ menos T_s⁴.",
            "PROHIBIDO sustituir T o T_s en °C (aunque sea una resta, elevar a la cuarta exige kelvin obligatoriamente).",
        ],
        unidades="A en m², e sin unidades (0–1), σ = 5,67×10⁻⁸ W/(m²·K⁴), T y T_s SIEMPRE en K → H en W.",
        errores=[
            "Error gravísimo: calcular (T − T_s)⁴ en vez de (T⁴ − T_s⁴). Por ejemplo 370⁴ − 300⁴ ≈ 1,06×10¹⁰, mientras que (370 − 300)⁴ = 2,4×10⁷ (¡400 veces mal!).",
            "Al despejar T desde H = Aeσ(T⁴ − T_s⁴), primero pasa T_s⁴ SUMANDO y al final saca raíz cuarta: T = (H/(Aeσ) + T_s⁴)^(1/4).",
            "Si T < T_s, H sale negativo (el cuerpo absorbe más radiación de la que emite).",
        ],
        confundir_con=[
            ("F2C3", "Usa F3C1 siempre que el enunciado te dé la temperatura del entorno/ambiente T_s."),
        ],
        se_deriva=[
            (r"T = \left(\frac{H}{Ae\sigma} + T_s^4\right)^{1/4}", "Divide por Aeσ, suma T_s⁴ y aplica raíz cuarta (exponente 0,25)."),
        ],
        pistas=[
            "dentro de un cuarto / recinto / paredes a temperatura ambiente T_s",
            "tasa neta de radiación / pérdida neta de calor por radiación",
            "enfriamiento radiativo / radiación que funde hielo",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (cálculo 3 pts, ej. Q068 en I-2026) o Parte III (desarrollo 8–9 pts, ej. P26-4, P26-5)",
        historial="Presente en I-2026 (Q068) y reforzada para II-2026 tras el aviso del profesor (calorimetría o radiación en desarrollo).",
    ),
    "F3C2": _c(
        bloque="calorimetria",
        estado="fija",
        prioridad="critica",
        que_es="Calor sensible Q absorbido o cedido por un cuerpo cuando cambia su temperatura SIN cambiar de fase.",
        cuando_si=[
            "Calentar o enfriar un sólido, un líquido o un gas dentro de una misma fase (sin fusión ni ebullición).",
            "Mezclas en calorímetro aislado aplicando conservación de energía: ΣQ = 0 con ΔT = T_final − T_inicial.",
            "Cuerpos cuya capacidad calorífica total C_cal (en J/K) ya viene dada: Q = C_cal·ΔT.",
        ],
        cuando_no=[
            "NO usar durante un cambio de fase (hielo derritiéndose a 0 °C o agua hirviendo a 100 °C → ahí ΔT = 0 y se usa F3C3).",
            "NO plantear Q_ganado + Q_cedido = 0 asumiendo una T_f final sin antes verificar si se completó el cambio de fase (trampa de fusión parcial PR2-D7).",
        ],
        unidades="m en kg, c en J/(kg·K) o J/(kg·°C), ΔT = T_f − T_i en K o °C → Q en J. Si dan n en moles, C en J/(mol·K).",
        errores=[
            "Siempre escribe ΔT = T_f − T_i (final menos inicial): así los cuerpos que se enfrían tienen Q < 0 automáticamente y ΣQ = 0 funciona sin adivinar signos.",
            "Confundir calor específico c [J/(kg·K)] con capacidad calorífica total C_cal [J/K] (cuando te dan C_cal = 150 J/K, NO la multipliques por la masa).",
            "Olvidar la masa del hielo ya derretido o del vapor ya condensado al calentar/enfriar el agua final.",
        ],
        confundir_con=[
            ("F3C3", "F3C2 cambia temperatura (tramo inclinado); F3C3 cambia fase a temperatura fija (tramo horizontal)."),
            ("F11C1", "En gases ideales el calor molar depende de la trayectoria: usa C_V (F11C1) a V constante o C_p (F11C2) a p constante."),
        ],
        se_deriva=[
            (r"\sum Q = 0 \;\Rightarrow\; T_f = \frac{\sum m_i c_i T_i}{\sum m_i c_i}", "Sin cambios de fase en sistema aislado, despeja T_f de Σ m_i c_i (T_f − T_i) = 0."),
        ],
        pistas=[
            "calorímetro aislado / recipiente de capacidad calorífica C",
            "temperatura final de equilibrio / mezcla de metal caliente con agua",
            "calor específico c / sin cambio de fase",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #2 (conceptual 2 pts), Parte II (3 pts) o Parte III (desarrollo 8–9 pts)",
        historial="Fija en la Parte I conceptual #2 de I-2026 y candidata directa a desarrollo en II-2026 (anunciada por el profesor; 5 problemas entre ambas prácticas).",
    ),
    "F3C3": _c(
        bloque="calorimetria",
        estado="fija",
        prioridad="critica",
        que_es="Calor latente Q absorbido (+mL) o liberado (−mL) al cambiar de fase (fusión/congelación o vaporización/condensación) a temperatura constante.",
        cuando_si=[
            "Fusión de hielo (+m·L_f) o congelación de agua (−m·L_f) a 0 °C.",
            "Vaporización de agua (+m·L_v) o condensación de vapor (−m·L_v) a 100 °C.",
            "Comprobar si la energía disponible alcanza para fundir todo el hielo o condensar todo el vapor.",
        ],
        cuando_no=[
            "NO usar cuando el cuerpo solo sube o baja de temperatura sin cambiar de estado (usa F3C2).",
            "NO asumir que toda la masa m cambia de fase sin comprobar antes los topes de energía (si solo alcanza para una fracción, m_fundida = Q_disponible / L_f y T_f queda en 0 °C).",
        ],
        unidades="m en kg, L_f o L_v en J/kg (agua: L_f ≈ 3,34×10⁵ J/kg, L_v ≈ 2,256×10⁶ J/kg) → Q en J.",
        errores=[
            "TRAMPA CLÁSICA (PR2-D7 vs PR1-2): compara SIEMPRE Q_necesario = m_hielo·L_f contra Q_máx_disponible antes de despejar T_f. Si Q_máx < Q_necesario, queda hielo y T_f = 0 °C.",
            "Signo en ΣQ = 0: fundir y evaporar ABSORBEN calor (+mL); congelar y condensar CEDEN calor (−mL).",
            "Después de condensar vapor a 100 °C, esa masa ahora es AGUA líquida a 100 °C que también cede calor al bajar hasta T_f.",
        ],
        confundir_con=[
            ("F3C2", "F3C3 no lleva ΔT porque la temperatura no cambia durante el cambio de fase."),
        ],
        se_deriva=[
            (r"m_{\text{fundida}} = \frac{Q_{\text{disponible}}}{L_f}", "En fusión parcial, despeja cuánta masa se funde; el resto queda como hielo a 0 °C."),
        ],
        pistas=[
            "hielo a 0 °C o −20 °C / vapor a 100 °C / ¿se funde todo el hielo?",
            "masas finales de hielo, agua y vapor / calor latente L_f o L_v",
            "masa de hielo fundida por hora / vapor condensado por hora",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #2 (conceptual 2 pts) o Parte III (desarrollo 8–9 pts: PR1-2, PR2-D7, PR2-D8, PR2-D12)",
        historial="Aparece en las 2 prácticas II-2026 y en la ruta anunciada por el profesor; además se acopla con conducción/radiación (Q = H·Δt = mL).",
    ),
    # ========================================================== FILA 4: GASES IDEALES
    "F4C1": _c(
        bloque="gases",
        estado="fija",
        prioridad="critica",
        que_es="Ecuación de estado del gas ideal: enlaza presión p, volumen V, moles n y temperatura absoluta T en cualquier estado de equilibrio.",
        cuando_si=[
            "Hallar la variable faltante (p, V, T o n) en cada vértice A, B, C, D de un diagrama p–V.",
            "Relacionar dos estados del mismo gas (p₁V₁/T₁ = p₂V₂/T₂): isobárico V/T = cte, isocórico p/T = cte, isotérmico pV = cte.",
            "Reemplazar nRT por pV dentro de energía interna, trabajo isotérmico o adiabatico.",
        ],
        cuando_no=[
            "PROHIBIDO meter T en °C: T SIEMPRE va en kelvin (F10C3).",
            "NO mezclar unidades de R: si usas R = 8,314 (F12C3), p va en Pa y V en m³; si p va en atm y V en L, usa R = 0,08206 (F14C3).",
        ],
        unidades="SI recomendado: p en Pa, V en m³ (1 L = 10⁻³ m³), n en mol, R = 8,314 J/(mol·K), T en K.",
        errores=[
            "Olvidar convertir litros a m³ (multiplica por 10⁻³) o cm³ a m³ (multiplica por 10⁻⁶) cuando usas Pa y R = 8,314.",
            "Usar presión manométrica en vez de presión absoluta (si dan manométrica, suma 1 atm = 101 325 Pa).",
            "Confundir estado (un punto: pV = nRT) con proceso (un cambio entre dos puntos).",
        ],
        confundir_con=[
            ("F9C2", "pV = cte solo vale en ISOTERMA; en ADIABÁTICA es pV^γ = cte (F9C2)."),
            ("F12C3", "Usa F12C3 (8,314) en SI (Pa, m³) y F14C3 (0,08206) solo en atm y L."),
        ],
        se_deriva=[
            (r"\rho = \frac{pM}{RT}", "Combina F4C1 con F4C2 (n = m_total/M) y ρ = m_total/V."),
            (r"\frac{p_1 V_1}{T_1} = \frac{p_2 V_2}{T_2}", "Divide pV/T = nR entre dos estados con la misma cantidad de moles n."),
        ],
        pistas=[
            "calcule las temperaturas / presiones / volúmenes en los estados A, B, C",
            "un recipiente de V litros contiene n moles de gas ideal",
            "densidad de un gas / gas ideal monoatómico o diatómico",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (3 pts, ej. Q074) y Parte IV (100 % de los ciclos, 17/17 exámenes)",
        historial="Ecuación más utilizada del examen junto con la primera ley: aparece en todos los desarrollos de ciclos (17/17).",
    ),
    "F4C2": _c(
        bloque="gases",
        estado="fija",
        prioridad="media",
        que_es="Relación entre la masa total de una muestra de gas, el número de moles n y su masa molar M.",
        cuando_si=[
            "El enunciado te da la masa del gas en gramos o kilogramos en lugar de darte los moles n.",
            "Te piden hallar la masa total del gas o derivar la densidad ρ = pM/(RT).",
        ],
        cuando_no=[
            "NO confundir masa total de la muestra m_total con masa de una sola molécula m (para una molécula usa F5C3: M = N_A m).",
        ],
        unidades="m_total en kg y M en kg/mol (¡cuidado! si M viene en g/mol, divide entre 1000: N₂ = 28 g/mol = 0,028 kg/mol) → n en mol.",
        errores=[
            "Error más común en gases: usar M en g/mol (ej. 28) junto con m en kg o dentro de v_rms = √(3RT/M) → te da v_rms mal por factor √1000 ≈ 31,6.",
            "Si m_total está en gramos y M en g/mol, n = m/M sí sale en mol, pero para F5C1 (v_rms) SIEMPRE pasa M a kg/mol.",
        ],
        confundir_con=[
            ("F5C3", "F4C2 relaciona masa TOTAL con moles (n); F5C3 relaciona masa de UNA MOLÉCULA con Avogadro (N_A)."),
        ],
        se_deriva=[
            (r"n = \frac{m_{\text{total}}}{M}", "Despeja los moles para entrar a pV = nRT o ΔU = nC_VΔT."),
            (r"\rho = \frac{m_{\text{total}}}{V} = \frac{pM}{RT}", "Sustituye n = m_total/M en F4C1 y despeja m_total/V."),
        ],
        pistas=[
            "masa molar M (g/mol o kg/mol) / muestra de m gramos de oxígeno o helio",
            "calcule la masa de gas en el tanque / densidad del gas",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (cálculo 3 pts) o paso inicial en gases/ciclos",
        historial="Aparece como puente obligado cuando el problema da gramos en vez de moles (ej. Q075, P26-3).",
    ),
    "F4C3": _c(
        bloque="gases",
        estado="fija",
        prioridad="alta",
        que_es="Energía cinética traslacional TOTAL de los n moles de un gas ideal a temperatura absoluta T.",
        cuando_si=[
            "Piden la energía cinética traslacional total del gas (en Joules).",
            "Para un gas MONOATÓMICO ideal, coincide con su energía interna total U = (3/2)nRT.",
            "En preguntas conceptuales sobre la relación entre presión p y energía cinética (con F4C1: K = (3/2)pV, como Q076 y Q078).",
        ],
        cuando_no=[
            "NO usar para energía cinética de UNA sola molécula (usa F5C2: (3/2)k_B T).",
            "NO confundir con energía interna total U de un gas DIATÓMICO: en diatómico hay rotación (U = (5/2)nRT), aunque la parte traslacional sigue siendo (3/2)nRT.",
            "PROHIBIDO poner T en °C: T va siempre en kelvin.",
        ],
        unidades="n en mol, R = 8,314 J/(mol·K), T en K → K en J.",
        errores=[
            "Olvidar que K_traslacional depende SOLO de n y T, NO del tipo de gas (a igual n y T, helio y oxígeno tienen la misma K_tr, aunque distintas rapideces).",
            "Usar R = 0,08206 en vez de 8,314: el resultado NO queda en Joules.",
        ],
        confundir_con=[
            ("F5C2", "F4C3 es la energía de TODO el gas (lleva nR); F5C2 es la de UNA molécula (lleva k_B)."),
            ("F7C2", "F4C3 da energía cinética traslacional (3/2 nRT); F7C2 da el cambio de energía interna total ΔU = nC_VΔT."),
        ],
        se_deriva=[
            (r"K = \frac{3}{2}pV", "Sustituye nRT = pV desde F4C1; útil cuando p y V son conocidos pero no T."),
        ],
        pistas=[
            "energía cinética traslacional total del gas",
            "cilindro sellado donde baja la presión / relación p con K",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I (conceptual 2 pts, ej. Q078) o Parte II (cálculo 3 pts)",
        historial="Muy rentable en selección conceptual y preguntas cortas de teoría cinética (Q076, Q078, P26-3).",
    ),
    # ========================================================== FILA 5: TEORÍA CINÉTICA
    "F5C1": _c(
        bloque="gases",
        estado="fija",
        prioridad="alta",
        que_es="Rapidez cuadrática media (v_rms) de las moléculas de un gas ideal en función de T y la masa molecular m o molar M.",
        cuando_si=[
            "Calcular qué tan rápido se mueven las moléculas de un gas (en m/s) a una temperatura T.",
            "Comparar velocidades de dos gases a igual temperatura (v_rms ∝ 1/√M: el gas más liviano es más rápido y escapa antes, como Q076 y PR2-S6).",
            "Hallar la temperatura T necesaria para que un gas alcance cierta rapidez v_rms.",
        ],
        cuando_no=[
            "PROHIBIDO sustituir M en g/mol: DEBE estar en kg/mol (ej. O₂ = 0,032 kg/mol, He = 0,004 kg/mol).",
            "PROHIBIDO sustituir T en °C (siempre en K).",
            "NO usar la fórmula para rapidez más probable (√2RT/M) ni media (√8RT/πM) si no están en la hoja.",
        ],
        unidades="R = 8,314 J/(mol·K), M en kg/mol, k_B = 1,38×10⁻²³ J/K, m en kg, T en K → v_rms en m/s.",
        errores=[
            "Olvidar pasar M de g/mol a kg/mol (error de factor √1000 ≈ 31,6).",
            "Creer que si T se duplica, v_rms se duplica: NO, v_rms ∝ √T (se multiplica por √2; para duplicar v_rms hay que CUADRUPLICAR T en kelvin).",
            "Mezclar k_B con M (incorrecto) o R con m (incorrecto): k_B va con m (1 molécula) y R va con M (1 mol).",
        ],
        confundir_con=[
            ("F5C2", "F5C1 da rapidez (m/s, depende de la masa); F5C2 da energía cinética media (J, NO depende de la masa)."),
        ],
        se_deriva=[
            (r"\frac{v_{\text{rms},1}}{v_{\text{rms},2}} = \sqrt{\frac{T_1 M_2}{T_2 M_1}}", "Divide F5C1 para dos gases o dos temperaturas; se cancelan 3 y R."),
        ],
        pistas=[
            "rapidez cuadrática media / rapidez eficaz / moléculas más rápidas",
            "dos gases (helio y oxígeno) a la misma temperatura / fuga por un agujero",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I/II (selección 2–3 pts, ej. PR2-S6, Q076, E13)",
        historial="Frecuente en selección única conceptual y cálculo rápido (PR2-S6, Q076, E13, P26-3).",
    ),
    "F5C2": _c(
        bloque="gases",
        estado="fija",
        prioridad="alta",
        que_es="Energía cinética traslacional media de UNA sola molécula de gas ideal: depende únicamente de la temperatura absoluta T.",
        cuando_si=[
            "Piden energía cinética promedio POR MOLÉCULA (órdenes de 10⁻²¹ J).",
            "Preguntas conceptuales sobre qué hace cambiar la energía cinética media (solo T en K; ej. Q081, Q025, Q078).",
        ],
        cuando_no=[
            "NO usar para la energía cinética total del gas (falta multiplicar por N; para eso usa F4C3: (3/2)nRT).",
            "PROHIBIDO poner T en °C: para duplicar K_med hay que duplicar T en KELVIN, no en °C (trampa clásica de Q081).",
        ],
        unidades="m en kg, v en m/s, k_B = 1,38×10⁻²³ J/K, T en K → resultado en J.",
        errores=[
            "Trampa de Q081: pasar de 20 °C a 40 °C NO duplica la energía cinética media; 20 °C = 293,15 K, así que duplicarla exige 586,3 K = 313,15 °C.",
            "Pensar que una molécula más pesada tiene más energía cinética media a igual T: FALSO, ambos gases tienen la MISMA (3/2)k_B T (la pesada solo va más lento).",
        ],
        confundir_con=[
            ("F4C3", "F5C2 es por molécula (lleva k_B); F4C3 es total del gas (lleva nR)."),
            ("F5C1", "De F5C2 se despeja F5C1: √((v²)_med) = √(3k_B T/m)."),
        ],
        se_deriva=[
            (r"K_{\text{total}} = N\cdot\frac{3}{2}k_B T = \frac{3}{2}nRT", "Multiplica F5C2 por N = nN_A usando R = N_A k_B."),
        ],
        pistas=[
            "energía cinética media por molécula / cómo duplicar la energía cinética media",
            "dos gases a la misma temperatura / equipartición (1/2 k_B T por grado)",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I (conceptual 2 pts, ej. Q078, Q079, Q081)",
        historial="Clásica de preguntas conceptuales de teoría cinética (Q025, Q078, Q079, Q081).",
    ),
    "F5C3": _c(
        bloque="gases",
        estado="fija",
        prioridad="apoyo",
        que_es="Conexión entre la masa molar M (masa de un mol) y la masa individual m de una sola molécula mediante el número de Avogadro N_A.",
        cuando_si=[
            "Necesitas la masa m de una molécula individual a partir de la masa molar M (m = M/N_A) para usar F5C1 o F5C2.",
            "Necesitas pasar entre número de moléculas N y moles n (N = n·N_A).",
        ],
        cuando_no=[
            "NO confundir m (masa de 1 molécula, ~10⁻²⁶ kg) con m_total (masa de toda la muestra en F4C2).",
        ],
        unidades="M en kg/mol, N_A = 6,022×10²³ mol⁻¹ → m en kg (siempre del orden de 10⁻²⁷ a 10⁻²⁵ kg).",
        errores=[
            "Si metes M en g/mol sin dividir por 1000, m te sale en gramos en vez de kg y rompe cualquier cuenta en Joules o m/s.",
            "Multiplicar por N_A en vez de dividir: una molécula pesa muchísimo MENOS que un mol, así que m = M / N_A.",
        ],
        confundir_con=[
            ("F4C2", "F4C2 es m_total = nM (toda la muestra); F5C3 es M = N_A m (una sola molécula)."),
        ],
        se_deriva=[
            (r"m = \frac{M}{N_A}", "Despeja la masa de una molécula dividiendo masa molar entre Avogadro."),
            (r"N = n N_A", "Número total de moléculas a partir de los moles n."),
        ],
        pistas=[
            "masa de una molécula / número total de moléculas N",
            "número de Avogadro N_A / comprobar v_rms con k_B",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (cálculo corto 3 pts) o apoyo en teoría cinética (P26-3)",
        historial="Poco frecuente en solitario, pero clave para conectar el mundo molar (R, M) con el molecular (k_B, m).",
    ),
    # ========================================================== FILA 6: TRABAJO TERMODINÁMICO
    "F6C1": _c(
        bloque="trabajo",
        estado="fija",
        prioridad="critica",
        que_es="Definición general del trabajo W hecho POR el gas en un proceso cuasiestático: es el área con signo bajo la curva en el diagrama p–V.",
        cuando_si=[
            "Procesos en línea recta inclinada (P = αV o trapecio entre dos puntos: área = ½(p₁+p₂)(V₂−V₁), como PR1-5 y PR2-D4).",
            "Procesos con p(V) dada como función cualquiera (polinomio, etc.) donde debes integrar.",
            "Cuando piden demostrar las fórmulas de trabajo isotérmico (F6C3) o adiabático (F9C1) integrando (como PR1-6).",
        ],
        cuando_no=[
            "En procesos isocóricos (V = cte): dV = 0 ⇒ W = 0 directo, sin integrar.",
            "En isobáricos, isotérmicos o adiabáticos puedes ir directo a F6C2, F6C3 o F9C1 salvo que el enunciado pida 'demuestre integrando'.",
        ],
        unidades="p en Pa (1 kPa = 10³ Pa, 1 atm = 101 325 Pa) y V en m³ (1 L = 10⁻³ m³) → W en J.",
        errores=[
            "Signo del trabajo: expansión (V₂ > V₁, flecha a la derecha) → W > 0; compresión (V₂ < V₁, flecha a la izquierda) → W < 0.",
            "CUIDADO con la redacción: en 26-I-E P10 preguntaron 'trabajo SOBRE el gas' (= −W_por_el_gas). Lee si piden 'hecho por' o 'sobre'.",
            "En un ciclo cerrado: sentido HORARIO → W_neto > 0 (motor); sentido ANTIHORARIO → W_neto < 0 (refrigerador, ej. PR1-6).",
        ],
        confundir_con=[
            ("F6C2", "F6C2 solo sirve cuando la presión es constante (rectángulo); F6C1 vale para CUALQUIER curva p(V)."),
        ],
        se_deriva=[
            (r"W_{\text{recta}} = \frac{p_1 + p_2}{2}(V_2 - V_1)", "Área del trapecio bajo una recta inclinada en el plano p–V."),
            (r"W_{P=\alpha V} = \frac{\alpha}{2}(V_2^2 - V_1^2)", "Sustituye p = αV en F6C1 e integra V dV = V²/2 (PR1-5)."),
        ],
        pistas=[
            "proceso en línea recta / P = αV / demuestre el trabajo integrando",
            "área bajo la curva / ciclo triangular / trabajo neto del ciclo",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #3 (conceptual 2 pts) y Parte IV (ciclos integradores: PR1-5, PR1-6, PR2-D4, 25-II-E, 23-II-O)",
        historial="Ciclos aparecen en 17/17 exámenes. Recta inclinada + demostración por integral son favoritas de ambas prácticas II-2026.",
    ),
    "F6C2": _c(
        bloque="trabajo",
        estado="fija",
        prioridad="critica",
        que_es="Trabajo hecho por un gas durante un proceso ISOBÁRICO (presión constante): área de un rectángulo en el diagrama p–V.",
        cuando_si=[
            "Tramos horizontales del diagrama p–V donde p₁ = p₂ = p constante.",
            "También equivale a nR(T₂ − T₁) combinándola con F4C1.",
        ],
        cuando_no=[
            "PROHIBIDO en expansión/compresión isotérmica (ahí p baja mientras V sube → usa F6C3).",
            "PROHIBIDO en procesos adiabáticos (usa F9C1 o −ΔU) ni en rectas inclinadas (usa F6C1).",
        ],
        unidades="p en Pa, V₁ y V₂ en m³ → W en J. (Si p está en kPa y V en L, kPa × L = J directamente, pero pasar todo a SI evita errores).",
        errores=[
            "Invertir la resta: es SIEMPRE V_final − V_inicial (V₂ − V₁). Si se comprime, W DEBE quedar negativo.",
            "Usar p en atm y V en L sin convertir a Pa y m³: atm·L NO son Joules (1 atm·L = 101,325 J).",
        ],
        confundir_con=[
            ("F6C1", "Si la presión cambia en recta inclinada, falta el promedio (p₁+p₂)/2 de F6C1."),
            ("F6C3", "En isoterma p NO es constante; usar pΔV es la trampa más penalizada."),
        ],
        se_deriva=[
            (r"W_{\text{isob}} = nR(T_2 - T_1)", "Sustituye pV₂ = nRT₂ y pV₁ = nRT₁ desde F4C1."),
        ],
        pistas=[
            "a presión constante / proceso isobárico / línea horizontal en p–V",
            "pistón libre con presión atmosférica constante",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (3 pts) y Parte IV (casi todos los ciclos p–V: PR1-5, PR2-D4, PR2-D6, PR2-D11)",
        historial="Presente en prácticamente todos los desarrollos de ciclos del banco (17/17 exámenes).",
    ),
    "F6C3": _c(
        bloque="trabajo",
        estado="fija",
        prioridad="critica",
        que_es="Trabajo hecho por un gas ideal en un proceso ISOTÉRMICO (temperatura constante); por la primera ley también es igual al calor Q.",
        cuando_si=[
            "Expansión o compresión a temperatura constante (ΔT = 0, curva hiperbólica pV = cte).",
            "Cuando conoces n, T y los volúmenes V₁, V₂ (o la razón de presiones p₁/p₂ = V₂/V₁ desde F4C1).",
        ],
        cuando_no=[
            "PROHIBIDO si la temperatura cambia (aunque sea poco) o en una adiabática (la adiabática es más empinada que la isoterma; usa F9C1).",
            "NO usar logaritmo base 10 (log): es logaritmo NATURAL (tecla ln).",
        ],
        unidades="n en mol, R = 8,314 J/(mol·K), T SIEMPRE en K; V₂ y V₁ en la misma unidad (se cancelan en el cociente) → W en J.",
        errores=[
            "Usar tecla log (base 10) en vez de ln (logaritmo natural): da un error de factor 2,303.",
            "Invertir el cociente: es V_final / V_inicial. Al comprimir (V₂ < V₁), ln(V₂/V₁) < 0 y W sale negativo.",
            "Si reemplazas V₂/V₁ por p₁/p₂, muestra primero p₁V₁ = p₂V₂ desde F4C1 (recuerda que las presiones van invertidas: p₁/p₂).",
        ],
        confundir_con=[
            ("F6C2", "En isoterma la presión NO es constante; nunca uses p(V₂ − V₁)."),
            ("F9C1", "En el diagrama p–V, la adiabática cae más rápido que la isoterma y tiene Q = 0 (en isoterma ΔU = 0 y Q = W)."),
        ],
        se_deriva=[
            (r"Q_{\text{iso}} = W_{\text{iso}} = nRT\ln\left(\frac{V_2}{V_1}\right)", "Como ΔT = 0 ⇒ ΔU = 0 (F7C2) ⇒ Q = W (F7C1)."),
            (r"W_{\text{iso}} = p_1 V_1\ln\left(\frac{p_1}{p_2}\right)", "Sustituye nRT = p₁V₁ y V₂/V₁ = p₁/p₂ desde F4C1."),
        ],
        pistas=[
            "temperatura constante / isotérmico / p₁V₁ = p₂V₂",
            "expansión lenta en contacto con un depósito térmico",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #3 (conceptual 2 pts) y Parte IV (ciclos: PR1-6, PR2-D6, Q105, Q100, Q111)",
        historial="Fija en los ciclos de examen y en la Parte I conceptual comparando isoterma vs. adiabática.",
    ),
    # ========================================================== FILAS 7 Y 8: PRIMERA LEY Y CALORES MOLARES
    "F7C1": _c(
        bloque="primera_ley",
        estado="fija",
        prioridad="critica",
        que_es="Primera ley de la termodinámica (conservación de energía): el cambio de energía interna ΔU es el calor que entra Q menos el trabajo que hace el gas W.",
        cuando_si=[
            "SIEMPRE válida para cualquier proceso (isobárico, isocórico, isotérmico, adiabático, recta inclinada o trayectoria arbitraria).",
            "Para hallar Q = ΔU + W en procesos generales, o para verificar un ciclo completo: ΔU_ciclo = 0 ⇒ W_neto = Q_neto.",
        ],
        cuando_no=[
            "Nunca falla físicamente; el único peligro es mezclar convenciones de signo (la hoja usa W > 0 cuando el gas SE EXPANDE: ΔU = Q − W).",
        ],
        unidades="ΔU, Q y W TODOS en Joules (J) o todos en kJ.",
        errores=[
            "Confundir signos: Q > 0 entra calor al gas; W > 0 el gas se expande (hace trabajo). Si Q = 0 (adiabática), W = −ΔU.",
            "Olvidar la comprobación de oro en un ciclo: al cerrar la tabla Q, W, ΔU, la suma de ΔU DEBE dar 0 y ΣQ DEBE igualar a ΣW.",
            "Creer que Q o W dependen solo de los puntos inicial y final: NO, solo ΔU es función de estado; Q y W dependen de la trayectoria.",
        ],
        confundir_con=[
            ("F7C2", "F7C1 relaciona ΔU con Q y W en cualquier sistema; F7C2 calcula ΔU directamente desde ΔT para un gas ideal."),
        ],
        se_deriva=[
            (r"Q = \Delta U + W", "Despeje directo para hallar el calor en rectas inclinadas o procesos isobáricos."),
            (r"\Delta U_{\text{ciclo}} = 0 \;\Rightarrow\; W_{\text{neto}} = Q_{\text{neto}}", "Al volver al estado inicial, el trabajo neto iguala al calor neto."),
        ],
        pistas=[
            "tabla de Q, W y ΔU / conservación de la energía en un gas",
            "dos trayectorias entre A y B / ciclo completo / expansión adiabática",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #3 (conceptual 2 pts), Parte II (3 pts) y Parte IV (100 % de los ciclos, 17/17)",
        historial="Eje absoluto de la termodinámica en el III Parcial: presente en el 100 % de los ciclos y en selección conceptual (PR2-S8, PR2-S12).",
    ),
    "F7C2": _c(
        bloque="primera_ley",
        estado="fija",
        prioridad="critica",
        que_es="Cambio de energía interna ΔU de un gas ideal: depende SOLO de la cantidad de moles, C_V y el cambio de temperatura ΔT.",
        cuando_si=[
            "EN CUALQUIER PROCESO de un gas ideal: isocórico, isobárico, adiabático, recta inclinada P = αV o curva desconocida.",
            "Para hallar trabajo adiabático rápidamente: como Q = 0, W_ad = −ΔU = −nC_VΔT.",
        ],
        cuando_no=[
            "NO usar para sólidos ni líquidos (solo gases ideales).",
            "NO cambiar C_V por C_P en procesos isobáricos: ΔU SIEMPRE lleva C_V, incluso a presión constante.",
        ],
        unidades="n en mol, C_V en J/(mol·K), ΔT = T_f − T_i en K (o °C) → ΔU en J.",
        errores=[
            "MITO FATAL #1: pensar que ΔU = nC_VΔT solo sirve a volumen constante. ¡FALSO! Como U depende solo de T, sirve en TODOS los procesos de un gas ideal.",
            "Poner C_P en vez de C_V cuando el proceso es isobárico (C_P va en Q_isob = nC_PΔT, jamás en ΔU).",
            "Olvidar que en una isoterma (ΔT = 0) da ΔU = 0 automáticamente.",
        ],
        confundir_con=[
            ("F11C1", "ΔU = nC_VΔT vale en CUALQUIER proceso; Q = nC_VΔT (F11C1) solo vale a VOLUMEN constante."),
            ("F11C2", "En un proceso isobárico, ΔU usa C_V (F7C2) pero Q usa C_P (F11C2)."),
        ],
        se_deriva=[
            (r"\Delta U = \frac{C_V}{R}(p_2 V_2 - p_1 V_1)", "Sustituye nΔT = (p₂V₂ − p₁V₁)/R desde F4C1 (útil si no calculaste T)."),
        ],
        pistas=[
            "energía interna / tabla Q, W, ΔU / trabajo en expansión adiabática",
            "dos trayectorias distintas entre los mismos estados A y B",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #3 (conceptual 2 pts, ej. PR2-S8) y Parte IV (17/17 ciclos)",
        historial="Se utiliza en cada uno de los tramos de todo desarrollo de ciclo termodinámico.",
    ),
    "F7C3": _c(
        bloque="primera_ley",
        estado="fija",
        prioridad="alta",
        que_es="Relación de Mayer para todo gas ideal: calentar un mol a presión constante cuesta exactamente R (8,314 J/(mol·K)) más que a volumen constante, porque el gas realiza trabajo de expansión.",
        cuando_si=[
            "Calcular C_P a partir de C_V (monoatómico: 3/2 R + R = 5/2 R; diatómico: 5/2 R + R = 7/2 R).",
            "Despejar C_V y C_P cuando conoces γ (combinando F7C3 con F8C3) en problemas de información incompleta (PR2-D5, PR2-D11, Q122).",
        ],
        cuando_no=[
            "NO usar para sólidos ni líquidos (solo gases ideales).",
            "NO sacar C_V = R/(γ−1) ni C_P = γR/(γ−1) de memoria sin mostrar antes F7C3 + F8C3.",
        ],
        unidades="C_P, C_V y R = 8,314 todos en J/(mol·K).",
        errores=[
            "Invertir el signo y escribir C_V = C_P + R: siempre C_P > C_V porque a presión constante parte del calor se va en trabajo pΔV.",
            "Usar R = 0,08206 en lugar de 8,314 J/(mol·K).",
        ],
        confundir_con=[
            ("F8C3", "F7C3 es la DIFERENCIA (C_P − C_V = R); F8C3 es el COCIENTE (γ = C_P / C_V). Juntas despejan ambos."),
        ],
        se_deriva=[
            (r"C_V = \frac{R}{\gamma - 1}", "Sustituye C_P = γC_V (F8C3) en F7C3 y despeja C_V."),
            (r"C_P = \frac{\gamma R}{\gamma - 1}", "Multiplica C_V = R/(γ−1) por γ."),
        ],
        pistas=[
            "no indican si el gas es monoatómico o diatómico / halle C_P y C_V",
            "por qué C_P es mayor que C_V / halle γ",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (3 pts) y Parte IV (ciclos con información incompleta: PR2-D5, PR2-D11, Q122)",
        historial="Indispensable cada vez que el problema oculta el tipo de gas y pide identificarlo desde Q_isob o una adiabática.",
    ),
    "F8C1": _c(
        bloque="primera_ley",
        estado="fija",
        prioridad="critica",
        que_es="Capacidad calorífica molar a volumen constante de un gas ideal MONOATÓMICO (solo 3 grados de libertad traslacionales).",
        cuando_si=[
            "El enunciado dice gas monoatómico o nombra un gas noble: helio (He), argón (Ar), neón (Ne).",
            "Para reconocer el tipo de gas cuando tu cálculo da C_V ≈ 12,47 J/(mol·K) o γ ≈ 1,67.",
        ],
        cuando_no=[
            "PROHIBIDO en gases diatómicos (O₂, N₂, H₂, aire, CO → usa F8C2 = 5/2 R).",
            "NO usar como si fuera C_P (para monoatómico, C_P = 3/2 R + R = 5/2 R).",
        ],
        unidades="C_Vmono = (3/2)(8,314) = 12,47 J/(mol·K).",
        errores=[
            "Confundir C_P del monoatómico (5/2 R) con C_V del diatómico (5/2 R): tienen el mismo número, pero significados distintos.",
            "Con igual calor Q a volumen constante, un monoatómico sube MÁS su temperatura que un diatómico (PR2-S11) porque tiene menor C_V.",
        ],
        confundir_con=[
            ("F8C2", "Monoatómico = 3/2 R (γ = 5/3 ≈ 1,67); diatómico = 5/2 R (γ = 7/5 = 1,40)."),
        ],
        se_deriva=[
            (r"C_{P\text{mono}} = \frac{5}{2}R,\quad \gamma_{\text{mono}} = \frac{5}{3} \approx 1{,}67", "Combina F8C1 con F7C3 y F8C3."),
        ],
        pistas=[
            "gas monoatómico / helio (He) / argón (Ar) / neón (Ne)",
            "C_V ≈ 12,5 J/(mol·K) o γ ≈ 1,67",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I/II (PR2-S11) y Parte IV (ciclos con helio/monoatómico: Q105, Q110, PR1-5, PR2-D4)",
        historial="En I-2026 ordinario y reposición (Q105, Q110) los desarrollos largos usaron helio (monoatómico).",
    ),
    "F8C2": _c(
        bloque="primera_ley",
        estado="fija",
        prioridad="critica",
        que_es="Capacidad calorífica molar a volumen constante de un gas ideal DIATÓMICO a temperatura moderada (3 traslacionales + 2 rotacionales = 5 grados de libertad).",
        cuando_si=[
            "El enunciado dice gas diatómico o nombra N₂, O₂, H₂, CO o aire (sin vibraciones activas).",
            "Ciclo Otto con aire / gas diatómico (γ = 1,40, C_V = 20,79 J/(mol·K), como PR2-D9).",
        ],
        cuando_no=[
            "PROHIBIDO en gases monoatómicos (He, Ar, Ne → usa F8C1 = 3/2 R).",
            "NO usar como si fuera C_P (para diatómico, C_P = 5/2 R + R = 7/2 R = 29,10 J/(mol·K)).",
        ],
        unidades="C_Vdia = (5/2)(8,314) = 20,79 J/(mol·K).",
        errores=[
            "Usar 5/2 R para C_P de un diatómico (el C_P correcto es 7/2 R = 29,10 J/(mol·K)).",
            "Si el problema dice 'no asuma el tipo de gas' o te da γ = 1,35, NO impongas 5/2 R: despeja C_V = R/(γ−1) desde F7C3 y F8C3.",
        ],
        confundir_con=[
            ("F8C1", "Diatómico = 5/2 R (γ = 1,40); monoatómico = 3/2 R (γ = 1,67)."),
        ],
        se_deriva=[
            (r"C_{P\text{dia}} = \frac{7}{2}R,\quad \gamma_{\text{dia}} = \frac{7}{5} = 1{,}40", "Combina F8C2 con F7C3 y F8C3."),
        ],
        pistas=[
            "gas diatómico / nitrógeno (N₂) / oxígeno (O₂) / aire / γ = 1,40",
            "ciclo Otto / C_V ≈ 20,8 J/(mol·K)",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I/II (PR2-S11) y Parte IV (ciclos: Q103, PR1-6, PR2-D5, PR2-D6, PR2-D9, PR2-D11)",
        historial="El tipo de gas más frecuente en los ciclos de examen y prácticas (Q103 en I-2026 E, PR1-6, PR2-D5, D6, D9, D11).",
    ),
    "F8C3": _c(
        bloque="primera_ley",
        estado="fija",
        prioridad="critica",
        que_es="Razón de capacidades caloríficas γ (exponente adiabático): siempre mayor que 1 porque C_P > C_V.",
        cuando_si=[
            "Calcular γ de un gas monoatómico (5/3 ≈ 1,67) o diatómico (7/5 = 1,40) para usarlo en adiabáticas (F9C1–F9C3) y Otto (F11C3).",
            "Identificar qué tipo de gas recorre un ciclo tras hallar γ con logaritmos o desde C_P y C_V (PR2-D5, PR2-D11, Q122).",
        ],
        cuando_no=[
            "NO invertir el cociente: γ = C_P / C_V > 1 siempre (si te da < 1, lo pusiste al revés).",
        ],
        unidades="Adimensional (sin unidades); siempre > 1 (1,67 mono; 1,40 dia).",
        errores=[
            "Invertir C_V/C_P y obtener γ < 1.",
            "Confundir γ con γ − 1 en las ecuaciones adiabáticas (por ejemplo, para diatómico γ = 1,40 pero γ − 1 = 0,40).",
        ],
        confundir_con=[
            ("F7C3", "Combina F8C3 con F7C3 para despejar C_V = R/(γ−1) y C_P = γR/(γ−1)."),
        ],
        se_deriva=[
            (r"C_V = \frac{R}{\gamma - 1},\quad C_P = \frac{\gamma R}{\gamma - 1}", "Despeje directo junto con F7C3."),
        ],
        pistas=[
            "razón adiabática γ / identifique qué tipo de gas es",
            "expansión o compresión adiabática / ciclo Otto",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte IV (ciclos con adiabáticas e información incompleta: PR2-D5, PR2-D9, PR2-D11, Q122)",
        historial="Siempre aparece en pareja con F7C3 cuando el examen evalúa procesos adiabáticos o identificación del gas.",
    ),
    # ========================================================== FILA 9: PROCESOS ADIABÁTICOS
    "F9C1": _c(
        bloque="adiabaticos",
        estado="fija",
        prioridad="alta",
        que_es="Trabajo hecho por un gas ideal durante un proceso ADIABÁTICO reversible (Q = 0) expresado en presiones y volúmenes.",
        cuando_si=[
            "Expansión o compresión adiabática cuando conoces (p₁, V₁) y (p₂, V₂) y γ.",
            "Para comprobar el resultado obtenido por W = −ΔU = −nC_V(T₂ − T₁) (ambas dan exactamente lo mismo).",
            "Cuando el problema pide demostrarla integrando W = ∫p dV con pV^γ = cte (como PR1-6).",
        ],
        cuando_no=[
            "PROHIBIDO en procesos que NO son adiabáticos (si Q ≠ 0, esta fórmula es falsa).",
            "NO invertir el orden: es (p₁V₁ − p₂V₂) / (γ − 1), inicial menos final (al revés que ΔT).",
        ],
        unidades="p en Pa, V en m³, γ sin unidades → W en J.",
        errores=[
            "Invertir p₁V₁ − p₂V₂ y cambiarle el signo al trabajo: si se EXPANDE adiabáticamente, W > 0 (y p₁V₁ > p₂V₂ porque T baja); si se COMPRIME, W < 0.",
            "Dividir entre γ en vez de (γ − 1).",
        ],
        confundir_con=[
            ("F6C3", "F6C3 es isotérmica (T cte, Q = W); F9C1 es adiabática (Q = 0, W = −ΔU)."),
            ("F7C2", "F9C1 es algebraicamente idéntica a −nC_V(T₂ − T₁) usando pV = nRT y C_V = R/(γ−1)."),
        ],
        se_deriva=[
            (r"W_{\text{ad}} = \frac{nR(T_1 - T_2)}{\gamma - 1} = -nC_V(T_2 - T_1) = -\Delta U", "Sustituye pV = nRT (F4C1) y C_V = R/(γ−1)."),
        ],
        pistas=[
            "expansión o compresión adiabática reversible / sin intercambio de calor",
            "demuestre W = (p₁V₁ − p₂V₂)/(γ − 1)",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte IV (ciclos con tramo adiabático: PR1-6, PR2-D5, Q103, Q122)",
        historial="Fija en desarrollos con adiabáticas; excelente verificación cruzada contra W = −ΔU.",
    ),
    "F9C2": _c(
        bloque="adiabaticos",
        estado="fija",
        prioridad="critica",
        que_es="Ley adiabática en presión y volumen para un gas ideal en un proceso cuasiestático sin transferencia de calor (Q = 0).",
        cuando_si=[
            "Encontrar p₂ o V₂ al final de una expansión/compresión adiabática.",
            "Expresar p(V) = (p₁V₁^γ)·V^(−γ) para integrar el trabajo adiabático (PR1-6).",
            "Despejar γ con logaritmos si conoces presiones y volúmenes en los extremos.",
        ],
        cuando_no=[
            "PROHIBIDO en procesos isotérmicos (ahí es p₁V₁ = p₂V₂ sin exponente γ).",
            "NO usar exponente (γ − 1) aquí: con presión y volumen el exponente es γ completo.",
        ],
        unidades="p₁ y p₂ en la misma unidad (Pa o atm o kPa); V₁ y V₂ en la misma unidad (m³ o L o cm³); γ sin unidades.",
        errores=[
            "Confundir exponente γ (de pV^γ) con γ − 1 (de TV^(γ−1)).",
            "Al despejar V₂ desde p₁V₁^γ = p₂V₂^γ, elevar (p₁/p₂) a 1/γ (NO dividir entre γ).",
            "En el diagrama p–V, como γ > 1, la curva adiabática cae MÁS EMPINADA que una isoterma.",
        ],
        confundir_con=[
            ("F9C3", "F9C2 usa p y V con exponente γ; F9C3 usa T y V con exponente (γ − 1)."),
            ("F4C1", "En isoterma se cumple pV = cte; en adiabática pV^γ = cte."),
        ],
        se_deriva=[
            (r"V_2 = V_1\left(\frac{p_1}{p_2}\right)^{1/\gamma}", "Divide por p₂ y eleva ambos lados a 1/γ."),
            (r"\gamma = \frac{\ln(p_1/p_2)}{\ln(V_2/V_1)}", "Aplica ln a ambos lados para bajar el exponente γ."),
        ],
        pistas=[
            "adiabático / térmicamente aislado / compresión rápida sin calor",
            "halle la presión final o el volumen final de la adiabática",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #3 (conceptual 2 pts) y Parte IV (ciclos con adiabática: PR1-6, Q103, Q122)",
        historial="Fija en los ciclos de examen (incluyendo Q103 en I-2026 E) y en preguntas conceptuales sobre pendiente de curvas p–V.",
    ),
    "F9C3": _c(
        bloque="adiabaticos",
        estado="fija",
        prioridad="critica",
        que_es="Ley adiabática en temperatura absoluta y volumen para un gas ideal con Q = 0.",
        cuando_si=[
            "Hallar T₂ al terminar una compresión o expansión adiabática (en particular en el ciclo Otto: T₂ = T₁·r^(γ−1), T₄ = T₃/r^(γ−1), PR2-D9).",
            "Hallar γ desconocida tomando logaritmo natural cuando conoces T₁, T₂ y la razón de volúmenes (PR2-D5, Q122).",
        ],
        cuando_no=[
            "PROHIBIDO sustituir T en °C: T₁ y T₂ DEBEN estar en kelvin.",
            "NO usar exponente γ completo: aquí es (γ − 1).",
        ],
        unidades="T₁ y T₂ SIEMPRE en K; V₁ y V₂ en la misma unidad (se cancelan en el cociente); γ sin unidades.",
        errores=[
            "Meter temperaturas en °C (ej. 27 °C en vez de 300,15 K): destruye el resultado.",
            "Usar exponente γ = 1,40 en vez de γ − 1 = 0,40.",
            "Al despejar γ con logaritmos: γ − 1 = ln(T₂/T₁) / ln(V₁/V₂); no olvides sumar +1 al final.",
        ],
        confundir_con=[
            ("F9C2", "F9C3 conecta T y V (exponente γ − 1); F9C2 conecta p y V (exponente γ)."),
            ("F11C3", "De aplicar F9C3 en los dos tramos adiabáticos del ciclo Otto nace directamente F11C3."),
        ],
        se_deriva=[
            (r"\gamma - 1 = \frac{\ln(T_2/T_1)}{\ln(V_1/V_2)}", "Saca ln a ambos lados para despejar γ en problemas de información incompleta (PR2-D5, Q122)."),
            (r"T_2 = T_1\,r^{\gamma - 1}", "Con r = V₁/V₂ (F12C1) en la compresión adiabática de un motor Otto (PR2-D9)."),
        ],
        pistas=[
            "expansión adiabática hasta duplicar el volumen, alcanzando T₂",
            "halle γ / ciclo Otto temperaturas T₂ y T₄",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (3 pts) y Parte IV (PR1-6, PR2-D5, PR2-D9, Q122)",
        historial="Herramienta clave tanto en ciclo Otto como en ciclos con gas desconocido (PR2-D5, Q122).",
    ),
    # ========================================================== FILAS 10–12: MÁQUINAS, REFRIGERADORES, CALOR DIFERENCIAL, OTTO Y ESCALAS
    "F10C1": _c(
        bloque="maquinas",
        estado="fija",
        prioridad="critica",
        que_es="Eficiencia térmica e de una MÁQUINA TÉRMICA (motor): fracción del calor absorbido Q_H que se convierte en trabajo neto útil W.",
        cuando_si=[
            "Ciclos recorridos en sentido HORARIO (W_neto > 0) que operan como motor térmico.",
            "Problemas de máquinas con Q_H, Q_c y W donde piden eficiencia (o W = e·Q_H).",
        ],
        cuando_no=[
            "PROHIBIDO en ciclos ANTIHORARIOS (W_neto < 0, ej. PR1-6, Q103, Q110): esos consumen trabajo y son refrigeradores → usa F10C2 (K), no e.",
            "NO meter calores negativos en Q_H: Q_H es ÚNICAMENTE la suma de los calores POSITIVOS (los que entran al gas).",
            "NO usar e = 1 − T_c/T_H (Carnot) en máquinas reales ni sin advertir que Carnot (secc. 20.6) NO está en la hoja II-2026.",
        ],
        unidades="W, Q_H y Q_c en la misma unidad (J o kJ o W de potencia) → e es adimensional, SIEMPRE entre 0 y 1 (0 % < e < 100 %).",
        errores=[
            "Error #1 en ciclos: sumar todos los Q (incluso los negativos) para ponerlos en el denominador en vez de sumar SOLO los Q > 0 (Q_H). Si metes Q_neto abajo te da e = 100 %, imposible por la 2.ª ley.",
            "Dividir |Q_c|/Q_H y reportar eso como e sin hacer 1 − |Q_c|/Q_H (trampa de PR2-S2).",
            "Aplicar e = W/Q_H a un ciclo antihorario sin revisar el signo de W_neto.",
        ],
        confundir_con=[
            ("F10C2", "Motor (horario, W > 0) → eficiencia e < 1 (F10C1). Refrigerador (antihorario, W < 0) → rendimiento K, puede ser > 1 (F10C2)."),
            ("F11C3", "F10C1 vale para CUALQUIER motor; F11C3 vale SOLO para el ciclo Otto ideal."),
        ],
        se_deriva=[
            (r"W = e\,Q_H,\qquad |Q_c| = Q_H - W", "Despeje directo del balance energético en un ciclo motor."),
        ],
        pistas=[
            "eficiencia térmica / máquina térmica / motor / ciclo horario",
            "libera al depósito frío el X % del calor recibido (PR2-S2)",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #4 (2.ª ley 2 pts), Parte II (3 pts, PR2-S2) y Parte IV (cierre de casi todo ciclo motor)",
        historial="Aparece al final de casi todos los desarrollos de ciclos motores (PR1-5, PR2-D4, PR2-D6, PR2-D9, PR2-D11, Q105).",
    ),
    "F10C2": _c(
        bloque="maquinas",
        estado="fija",
        prioridad="critica",
        que_es="Coeficiente de rendimiento K de un REFRIGERADOR: razón entre el calor extraído del espacio frío |Q_c| y el trabajo invertido |W|.",
        cuando_si=[
            "Refrigeradores, congeladores o aires acondicionados que extraen calor |Q_c| de un foco frío gastando trabajo |W|.",
            "Ciclos p–V recorridos en sentido ANTIHORARIO (W_neto < 0: Q103, Q110, Q122, PR1-6).",
        ],
        cuando_no=[
            "NO usar para motores (ciclos horarios con W > 0 → usa F10C1).",
            "NO usar K_Carnot = T_c/(T_H − T_c): Carnot (secc. 20.6) no viene en la hoja II-2026.",
        ],
        unidades="|Q_c|, |Q_H| y |W| en la misma unidad (J, kJ o W de potencia) → K es adimensional y suele ser MAYOR que 1 (ej. 2 a 6).",
        errores=[
            "Confundir |Q_H| (calor expulsado a la habitación, el más grande: |Q_H| = |Q_c| + |W|) con |Q_c| (calor sacado de la nevera, el que va arriba en K).",
            "Creer que K debe ser menor que 1 (trampa conceptual de Q123): e < 1 siempre, pero K normalmente es > 1.",
            "Dividir al revés |W|/|Q_c| (trampa A de PR2-S14).",
        ],
        confundir_con=[
            ("F10C1", "F10C1 es motor (horario, e < 1); F10C2 es refrigerador (antihorario, K puede ser > 1)."),
        ],
        se_deriva=[
            (r"|Q_H| = |Q_c| + |W|", "Balance de energía de un refrigerador: el calor cedido al exterior suma lo extraído + el trabajo."),
        ],
        pistas=[
            "refrigerador / extrae calor del espacio frío / coeficiente de rendimiento K",
            "ciclo en sentido antihorario / consume trabajo (W_neto < 0)",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I #4 (conceptual 2 pts, Q123), Parte II (3 pts, PR2-S14) y Parte IV (I-2026 E Q103 y I-2026 R Q110)",
        historial="En I-2026 tanto el extraordinario (Q103) como la reposición (Q110) pusieron un ciclo antihorario (refrigerador) en desarrollo largo.",
    ),
    "F10C3": _c(
        bloque="escalas",
        estado="fija",
        prioridad="critica",
        que_es="Conversión de temperatura puntual de grados Celsius a la escala absoluta Kelvin.",
        cuando_si=[
            "OBLIGATORIA en toda fórmula donde aparece T sola (no ΔT): gas ideal (F4C1), radiación T⁴ (F2C3, F3C1), energía cinética (F4C3, F5C2), v_rms (F5C1), trabajo isotérmico (F6C3) y adiabática T–V (F9C3).",
        ],
        cuando_no=[
            "NO sumar 273,15 a un CAMBIO de temperatura ΔT: como 273,15 se cancela en la resta, ΔT_K = ΔT_°C exactamente (en dilatación, conducción y calorimetría puedes restar directo en °C).",
        ],
        unidades="T_°C en °C → T_K en K (nunca se escribe °K, solo K).",
        errores=[
            "Error clásico #1: sumar 273,15 a un ΔT (ej. creer que ΔT = 20 °C son 293,15 K; ¡NO, son 20 K!).",
            "Error clásico #2: olvidar sumar 273,15 antes de meter T en T⁴, pV = nRT, v_rms o TV^(γ−1).",
        ],
        confundir_con=[
            ("F12C2", "En Kelvin el grado mide igual que en Celsius (ΔT_K = ΔT_°C); en Fahrenheit un grado es más pequeño (ΔT_°F = 1,8·ΔT_°C)."),
        ],
        se_deriva=[
            (r"\Delta T_K = \Delta T_{^\circ\text{C}}", "Resta T_K2 − T_K1: la constante 273,15 se cancela."),
        ],
        pistas=[
            "temperaturas dadas en °C dentro de problemas de gases, radiación o ciclos",
            "conversión de escalas térmicas",
        ],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Transversal en todas las partes (radiación, gases, ciclos y escalas)",
        historial="Fuente de errores evitables #1 en radiación y gases ideales si se omite en T absoluta o se aplica mal a ΔT.",
    ),
    "F11C1": _c(
        bloque="primera_ley",
        estado="reciente",
        prioridad="alta",
        que_es="Calor elemental dQ absorbido o cedido por un gas ideal durante un proceso a VOLUMEN CONSTANTE (isocórico).",
        cuando_si=[
            "Tramos verticales del diagrama p–V (V = cte, W = 0): al integrar con C_V constante da Q = nC_VΔT = ΔU.",
            "Cálculo diferencial de calor en procesos isocóricos o cuando C_V varía con T.",
        ],
        cuando_no=[
            "PROHIBIDO usar Q = nC_VΔT en procesos isobáricos (usa F11C2 con C_p), isotérmicos (ΔT = 0 pero Q = W ≠ 0) ni rectas inclinadas (usa Q = ΔU + W).",
        ],
        unidades="n en mol, C_V en J/(mol·K), dT o ΔT en K (o °C) → Q en J.",
        errores=[
            "Confundir ΔU = nC_VΔT (que vale SIEMPRE) con Q = nC_VΔT (que vale SOLO cuando V es constante).",
            "En una recta inclinada P = αV (PR1-5), Q NO es nC_VΔT ni nC_pΔT: se calcula con Q = ΔU + W.",
        ],
        confundir_con=[
            ("F11C2", "F11C1 usa C_V a VOLUMEN constante (vertical); F11C2 usa C_p a PRESIÓN constante (horizontal)."),
            ("F7C2", "F7C2 da ΔU para cualquier proceso; F11C1 da Q solo para procesos isocóricos."),
        ],
        se_deriva=[
            (r"Q_{V=\text{cte}} = nC_V\Delta T = \Delta U", "Integra F11C1 con C_V constante; coincide con ΔU porque W = 0."),
        ],
        pistas=[
            "a volumen constante / isocórico / línea vertical en p–V / calentamiento en ciclo Otto",
        ],
        en_formularios="3/17 previos (desde I-2026 O/E/R) · Fija en II-2026",
        parte_examen="Parte I/II (ej. PR2-S11) y Parte IV (todo ciclo con tramos verticales, incluido Otto PR2-D9)",
        historial="Introducida en I-2026 junto con F11C2 y mantenida en II-2026.",
    ),
    "F11C2": _c(
        bloque="primera_ley",
        estado="reciente",
        prioridad="alta",
        que_es="Calor elemental dQ absorbido o cedido por un gas ideal durante un proceso a PRESIÓN CONSTANTE (isobárico).",
        cuando_si=[
            "Tramos horizontales del diagrama p–V (p = cte): al integrar da Q = nC_pΔT.",
            "Despejar C_p desconocido cuando te dan el calor absorbido Q_AB en una expansión isobárica (ej. PR2-D11).",
        ],
        cuando_no=[
            "PROHIBIDO usar C_p en procesos isocóricos (usa F11C1), isotérmicos (usa F6C3) ni para calcular ΔU (ΔU siempre lleva C_V, F7C2).",
        ],
        unidades="n en mol, C_p = C_V + R en J/(mol·K), dT o ΔT en K (o °C) → Q en J.",
        errores=[
            "Usar C_V en lugar de C_p al calcular el calor de un tramo isobárico (te falta sumar el trabajo pΔV).",
            "Para comprobar tus cuentas en un tramo isobárico: nC_pΔT DEBE coincidir exactamente con ΔU + W = nC_VΔT + pΔV.",
        ],
        confundir_con=[
            ("F11C1", "F11C2 es a PRESIÓN constante (horizontal, C_p); F11C1 es a VOLUMEN constante (vertical, C_V)."),
            ("F7C2", "A presión constante, Q usa C_p (F11C2) pero ΔU usa C_V (F7C2)."),
        ],
        se_deriva=[
            (r"Q_{p=\text{cte}} = nC_p\Delta T = \Delta U + p\Delta V", "Integra F11C2 con C_p constante; equivale a F7C2 + F6C2."),
        ],
        pistas=[
            "a presión constante / isobárico / línea horizontal en p–V",
            "en la expansión isobárica absorbe Q kJ → despejar C_p (PR2-D11)",
        ],
        en_formularios="3/17 previos (desde I-2026 O/E/R) · Fija en II-2026",
        parte_examen="Parte II (3 pts) y Parte IV (ciclos con tramo horizontal: PR2-D11, PR2-D4, PR2-D6)",
        historial="Introducida en I-2026 junto con F11C1 y muy útil en ciclos de información incompleta (PR2-D11).",
    ),
    "F11C3": _c(
        bloque="maquinas",
        estado="reciente",
        prioridad="alta",
        que_es="Eficiencia térmica del ciclo OTTO ideal (motor de gasolina: 2 adiabáticas + 2 isocóricas) en función únicamente de la razón de compresión r y γ.",
        cuando_si=[
            "Motores de gasolina o ciclos Otto ideales (2 adiabáticas reversibles + 2 procesos a volumen constante).",
            "Calcular e_otto dado r y γ (PR2-S7, PR2-D9, E21) o despejar r necesaria para una eficiencia objetivo.",
        ],
        cuando_no=[
            "PROHIBIDO en ciclos que NO sean Otto (no aplica a ciclos triangulares, rectangulares, Diesel ni isotérmicos: ahí usa F10C1).",
            "NO usar exponente γ en lugar de (γ − 1).",
        ],
        unidades="r y γ sin unidades → e_otto adimensional entre 0 y 1 (multiplicar ×100 para %).",
        errores=[
            "TRAMPA DE DESPEJE INVERSO: si te dan e_otto y piden r, despeja 1/r^(γ−1) = 1 − e ⇒ r = (1 / (1 − e))^(1/(γ−1)). ¡Eleva a 1/(γ−1), NO dividas por γ−1!",
            "Para diatómico (γ = 1,40), γ − 1 = 0,40 y 1/(γ−1) = 2,5.",
            "Recordar que e_otto NO depende de Q_H ni de T₁: depende solo de r y γ.",
        ],
        confundir_con=[
            ("F10C1", "F10C1 vale para cualquier motor; en un ciclo Otto completo (PR2-D9), W/Q_H (F10C1) debe dar igual que F11C3."),
            ("F12C1", "F12C1 define r = V_máx/V_mín que entra en el denominador de F11C3."),
        ],
        se_deriva=[
            (r"r = \left(\frac{1}{1 - e_{\text{otto}}}\right)^{\frac{1}{\gamma - 1}}", "Despeje inverso de la razón de compresión dada la eficiencia deseada."),
        ],
        pistas=[
            "ciclo Otto / motor de gasolina / razón de compresión r",
            "dos adiabáticas y dos isocóricas / ¿qué razón r da eficiencia X %?",
        ],
        en_formularios="3/17 previos (desde I-2026 O/E/R) · Mantenida en II-2026",
        parte_examen="Parte II (cálculo 3 pts, ej. PR2-S7) o Parte IV (desarrollo completo, ej. PR2-D9, E21)",
        historial="Fija en la hoja desde I-2026 aunque todavía no había salido en examen I-2026; sí está en Recomendados (E21) y en la práctica del profesor (PR2-S7 y PR2-D9). Recuerda que 24-II-E tuvo un ciclo Diesel de 18 pts.",
    ),
    "F12C1": _c(
        bloque="maquinas",
        estado="reciente",
        prioridad="alta",
        que_es="Definición de la razón de compresión r de un motor: cociente entre el volumen máximo (antes de comprimir) y el volumen mínimo (al final de la compresión).",
        cuando_si=[
            "Hallar V_mín = V_máx / r (como en PR2-D9 inciso a) o calcular r a partir de dos volúmenes para entrar a F11C3 y F9C3.",
        ],
        cuando_no=[
            "NO invertir el cociente: r siempre es MAYOR que 1 (típicamente 7 a 12 en motores Otto).",
        ],
        unidades="V_máx y V_mín en la misma unidad (cm³, L o m³) → r es adimensional (> 1).",
        errores=[
            "Poner V_mín arriba y obtener r < 1 (si metes r < 1 en F11C3, la eficiencia te saldría negativa).",
            "Si V_máx viene en cm³ y luego necesitas pV = nRT, convierte con F14C2 (1 L = 1000 cm³, 1 m³ = 10⁶ cm³).",
        ],
        confundir_con=[
            ("F11C3", "Primero obtienes o usas r con F12C1; luego evalúas la eficiencia con F11C3 y las temperaturas adiabáticas con F9C3."),
        ],
        se_deriva=[
            (r"V_{\text{mín}} = \frac{V_{\text{máx}}}{r}", "Despeje directo del volumen de la cámara al final de la compresión."),
        ],
        pistas=[
            "razón de compresión r / volumen inicial 500 cm³ y r = 9,00",
            "volumen mínimo / ciclo Otto",
        ],
        en_formularios="3/17 previos (desde I-2026 O/E/R) · Mantenida en II-2026",
        parte_examen="Parte II (3 pts, PR2-S7) o Parte IV inciso a) (PR2-D9)",
        historial="Va siempre en pareja con F11C3 (ciclo Otto).",
    ),
    "F12C2": _c(
        bloque="escalas",
        estado="nueva",
        prioridad="critica",
        que_es="Conversión de una temperatura puntual de grados Celsius a grados Fahrenheit.",
        cuando_si=[
            "Convertir una lectura de temperatura entre °C y °F (despejando T_°C = (5/9)(T_°F − 32)).",
            "Deducir la relación de INTERVALOS ΔT_°F = (9/5)ΔT_°C = 1,8·ΔT_°C para convertir calores específicos o dilataciones (PR2-S1, P26-2).",
        ],
        cuando_no=[
            "PROHIBIDO sumar o restar 32 cuando conviertes un CAMBIO de temperatura ΔT o un calor específico c (en una resta el +32 se cancela siempre).",
        ],
        unidades="T_°C en °C, T_°F en °F. Para cambios: 1 K = 1 °C = 1,8 °F.",
        errores=[
            "TRAMPA DE EXAMEN (PR2-S1): para pasar c de J/(kg·K) a J/(kg·°F), se DIVIDE entre 1,8 (NO se multiplica por 1,8 y JAMÁS se resta 32): 1800 / 1,8 = 1000 J/(kg·°F).",
            "Al invertir la fórmula hacia Celsius, primero RESTA 32 y LUEGO multiplica por 5/9 (no al revés).",
        ],
        confundir_con=[
            ("F10C3", "En Kelvin (F10C3) el tamaño del grado es 1:1 con Celsius; en Fahrenheit (F12C2) hay factor 9/5 = 1,8."),
        ],
        se_deriva=[
            (r"T_{^\circ\text{C}} = \frac{5}{9}\left(T_{^\circ\text{F}} - 32\right)", "Resta 32 y multiplica por 5/9."),
            (r"\Delta T_{^\circ\text{F}} = \frac{9}{5}\,\Delta T_{^\circ\text{C}} = 1{,}8\,\Delta T_K", "Resta dos temperaturas en °F: el +32 desaparece."),
        ],
        pistas=[
            "grados Fahrenheit (°F) / calor específico en J/(kg·°F)",
            "diferencia de temperatura en °F / conversión de escalas",
        ],
        en_formularios="0/17 previos · 100 % NUEVA en II-2026",
        parte_examen="Parte II (cálculo 3 pts, exactamente como PR2-S1) o paso inicial de calorimetría (P26-2)",
        historial="Nunca estuvo en los 17 formularios anteriores ni en el Folleto, pero entró nueva en II-2026 y ya abrió la práctica del profesor (PR2-S1).",
    ),
    # ========================================================== FILAS 12–14: CONSTANTES Y CONVERSIONES
    "F12C3": _c(
        bloque="constantes",
        estado="fija",
        prioridad="critica",
        que_es="Constante universal de los gases ideales R en unidades del Sistema Internacional (SI).",
        cuando_si=[
            "SIEMPRE que el resultado salga en Joules (W, Q, ΔU, K), en m/s (v_rms), en J/(mol·K) (C_V, C_P) o en Pa y m³ (pV = nRT).",
        ],
        cuando_no=[
            "NO usar si p está en atm y V está en litros sin convertir antes a Pa y m³ (si dejas atm y L en pV = nRT, usa F14C3 = 0,08206).",
        ],
        unidades="8,314 J/(mol·K) = 8,314 Pa·m³/(mol·K).",
        errores=[
            "Regla de oro: convierte p a Pa y V a m³ desde el principio y usa SIEMPRE R = 8,314 en todo el examen. Así nunca mezclas unidades.",
        ],
        confundir_con=[
            ("F14C3", "Usa F12C3 (8,314) para TODO cálculo energético en SI; F14C3 (0,08206) sirve solo para pV=nRT en atm·L."),
            ("F13C1", "R = N_A·k_B es por MOL; k_B (F13C1) es por MOLÉCULA."),
        ],
        se_deriva=[],
        pistas=["todo problema de gases, ciclos p–V, C_V, C_P, trabajo o energía interna en SI"],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Usada en 100 % de los ciclos (Parte IV) y cálculos de gases (Parte II)",
        historial="La constante más usada del examen; estándar en todas las lecciones del banco.",
    ),
    "F13C1": _c(
        bloque="constantes",
        estado="fija",
        prioridad="apoyo",
        que_es="Constante de Boltzmann k_B = R / N_A: equivale a la constante de los gases por cada molécula individual.",
        cuando_si=[
            "Cálculos microscópicos por molécula: energía cinética molecular (F5C2) y v_rms con masa de 1 molécula m (F5C1).",
        ],
        cuando_no=[
            "PROHIBIDO confundir k_B (constante de Boltzmann) con la conductividad térmica k o k_B de una capa B en conducción (como PR2-D3).",
            "NO usar con moles n ni masa molar M (con moles usa R = 8,314).",
        ],
        unidades="1,38×10⁻²³ J/K (exige T en K y m en kg).",
        errores=[
            "Confundir k_B (1,38×10⁻²³ J/K) con k de conductividad térmica en W/(m·K).",
            "Usar k_B junto con la masa molar M en vez de la masa de una molécula m.",
        ],
        confundir_con=[
            ("F12C3", "k_B va con N moléculas y masa m; R (F12C3) va con n moles y masa molar M."),
            ("F2C1", "En conducción, k es conductividad del material (ej. capa B: k_B), NO Boltzmann."),
        ],
        se_deriva=[],
        pistas=["energía cinética por molécula / rapidez con masa de una molécula / equipartición"],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte I/II (teoría cinética: Q078–Q081, PR2-S6, P26-3)",
        historial="Constante de apoyo en teoría cinética molecular.",
    ),
    "F13C2": _c(
        bloque="constantes",
        estado="fija",
        prioridad="apoyo",
        que_es="Número de Avogadro N_A: cantidad exacta de moléculas (o átomos) contenidas en 1 mol de cualquier sustancia.",
        cuando_si=[
            "Pasar de masa molar M a masa de una molécula (m = M/N_A, F5C3).",
            "Pasar de moles n a número total de moléculas (N = n·N_A).",
        ],
        cuando_no=[
            "No necesitas N_A si toda tu ecuación trabaja en moles (n, R, M).",
        ],
        unidades="6,022×10²³ mol⁻¹.",
        errores=[
            "Invertir el signo del exponente (es +23, no −23; el −23 es k_B).",
        ],
        confundir_con=[
            ("F13C1", "N_A tiene exponente +23 (enorme); k_B tiene exponente −23 (diminuto). Su producto da R ≈ 8,31."),
        ],
        se_deriva=[],
        pistas=["cuántas moléculas hay / masa de una sola molécula / Avogadro"],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II o apoyo en teoría cinética (P26-3)",
        historial="Constante de enlace entre moles y moléculas individuales.",
    ),
    "F13C3": _c(
        bloque="constantes",
        estado="fija",
        prioridad="alta",
        que_es="Constante universal de Stefan–Boltzmann σ para radiación electromagnética de cuerpos a temperatura T.",
        cuando_si=[
            "En TODO cálculo numérico de radiación emitida (F2C3) o radiación neta (F3C1).",
        ],
        cuando_no=[
            "No la necesitas si el problema pide solo una razón H₂/H₁ = (T₂/T₁)⁴ porque σ se cancela arriba y abajo.",
        ],
        unidades="5,67×10⁻⁸ W/(m²·K⁴) → obliga a usar A en m² y T en K.",
        errores=[
            "Olvidar el signo menos del exponente (−8) o sustituir A en cm² en vez de m².",
            "No confundir σ (Stefan–Boltzmann) con esfuerzo mecánico.",
        ],
        confundir_con=[
            ("F2C3", "Se usa exclusivamente junto con F2C3 y F3C1."),
        ],
        se_deriva=[],
        pistas=["radiación / Stefan–Boltzmann / potencia irradiada en watts"],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Parte II (Q068) o Parte III (desarrollo de radiación: P26-4, P26-5)",
        historial="Imprescindible si sale radiación en el desarrollo corto anunciado por el profesor.",
    ),
    "F14C1": _c(
        bloque="constantes",
        estado="nueva",
        prioridad="alta",
        que_es="Equivalencia exacta entre atmósferas (atm) y pascales (Pa = N/m²) en el Sistema Internacional.",
        cuando_si=[
            "Siempre que un problema de gases o ciclos dé la presión en atm y necesites trabajar en SI (Joules, Pa, m³, R = 8,314).",
        ],
        cuando_no=[
            "Si la presión ya viene en kPa, solo multiplica por 10³ (NO multipliques por 101 325).",
        ],
        unidades="1 atm = 101 325 Pa = 101,325 kPa.",
        errores=[
            "Dejar la presión en atm al calcular trabajo W = pΔV y reportar el número como si fueran Joules.",
            "Confundir atm con kPa (1 atm son 101,325 kPa, no 1 kPa).",
        ],
        confundir_con=[
            ("F14C3", "Si conviertes atm → Pa con F14C1 y L → m³, usas R = 8,314 (F12C3) en todo el problema sin riesgo."),
        ],
        se_deriva=[],
        pistas=["presión dada en atm (ej. 1,00 atm, 2,50 atm) / presión atmosférica estándar"],
        en_formularios="Notación exacta 101325 Pa nueva en II-2026 (antes 1,013×10⁵)",
        parte_examen="Transversal en gases y ciclos (ej. Q105, Q111, P26-3)",
        historial="Muy común en exámenes que mezclan estados en atm y litros pero piden W, Q y ΔU en Joules.",
    ),
    "F14C2": _c(
        bloque="constantes",
        estado="nueva",
        prioridad="alta",
        que_es="Equivalencia exacta entre litros (L) y centímetros cúbicos (cm³ = mL), puente hacia metros cúbicos (m³).",
        cuando_si=[
            "Problemas donde el volumen viene en cm³ (como motores Otto: 500 cm³ en PR2-D9, o tanques en P26-3) y necesitas pasarlo a litros o m³.",
        ],
        cuando_no=[
            "NO confundir 1 L = 1000 cm³ con 1 m³ = 1000 L: para pasar de cm³ a m³ divides entre 10⁶ (1000 × 1000), NO entre 1000.",
        ],
        unidades="1 L = 1000 cm³ = 10⁻³ m³  ⇒  1 cm³ = 10⁻⁶ m³.",
        errores=[
            "Error clásico de factor 1000: creer que 500 cm³ son 0,5 m³. ¡NO! 500 cm³ = 0,500 L = 5,00×10⁻⁴ m³.",
            "Para pasar cm² (área) a m² se divide entre 10⁴; para pasar cm³ (volumen) a m³ se divide entre 10⁶.",
        ],
        confundir_con=[
            ("F14C1", "F14C1 convierte presión (atm → Pa); F14C2 convierte volumen (cm³ ↔ L → m³)."),
        ],
        se_deriva=[
            (r"1\ \text{m}^3 = 1000\ \text{L} = 10^6\ \text{cm}^3", "Encadena 1 L = 10⁻³ m³ con F14C2."),
        ],
        pistas=[
            "volumen en cm³ (ej. 500 cm³ en ciclo Otto PR2-D9) o en litros (L)",
        ],
        en_formularios="0/17 previos · Nueva equivalencia explícita en II-2026",
        parte_examen="Parte II/IV en gases y ciclo Otto (PR2-D9, P26-3)",
        historial="Añadida en II-2026 específicamente para conversiones de volumen en gases y cilindros de motor Otto.",
    ),
    "F14C3": _c(
        bloque="constantes",
        estado="fija",
        prioridad="apoyo",
        que_es="Constante de los gases ideales R expresada en litros·atmósfera por mol·kelvin.",
        cuando_si=[
            "ÚNICAMENTE en pV = nRT (F4C1) cuando p está en atm, V en litros y NO necesitas todavía resultados en Joules.",
        ],
        cuando_no=[
            "PROHIBIDO meter 0,08206 en W, Q, ΔU, K, v_rms, C_V o C_P: esas cantidades van en SI (Joules, m/s) y exigen R = 8,314 (F12C3).",
        ],
        unidades="0,08206 L·atm/(mol·K) — exige p en atm, V en L y T en K.",
        errores=[
            "Usar 0,08206 para calcular C_V = (5/2)R o W = nRT ln(V₂/V₁) y escribir 'J' al final: da un número ~101 veces demasiado pequeño.",
        ],
        confundir_con=[
            ("F12C3", "Ante la duda, convierte todo a Pa y m³ y usa SIEMPRE R = 8,314 (F12C3)."),
        ],
        se_deriva=[],
        pistas=["p en atm y V en L cuando solo piden T, n, p o V con pV = nRT"],
        en_formularios="17/17 previos · Fija en II-2026",
        parte_examen="Apoyo rápido en Parte II si p está en atm y V en L",
        historial="Útil como atajo de verificación en pV = nRT, pero peligrosa si se arrastra a cálculos de energía.",
    ),
}

# ========================================================== GUÍA RÁPIDA: "¿QUÉ FÓRMULA USO?"
DECISIONES_RAPIDAS: Tuple[Dict[str, Any], ...] = (
    {
        "sintoma": "Barra rígidamente sujeta entre muros que se calienta o enfría",
        "celdas": ["F1C3", "F1C1"],
        "trampa": "Calentar (ΔT > 0) produce compresión (F/A < 0); enfriar (ΔT < 0) produce tensión (F/A > 0). El esfuerzo no depende del área, la fuerza sí.",
        "leccion": "P26-1",
    },
    {
        "sintoma": "Líquido en un recipiente que se llena hasta el borde y se calienta",
        "celdas": ["F1C2", "F1C1"],
        "trampa": "El recipiente también se dilata: deriva β_rec = 3α desde F1C1 y resta ΔV_líq − ΔV_rec.",
        "leccion": "E2",
    },
    {
        "sintoma": "Calor específico en °F o diferencia de temperatura en °F",
        "celdas": ["F12C2", "F10C3"],
        "trampa": "En un ΔT o en un calor específico el +32 se cancela: solo usa factor 1,8 (divide entre 1,8 para pasar J/(kg·K) a J/(kg·°F)).",
        "leccion": "PR2-S1",
    },
    {
        "sintoma": "Mezcla con hielo y metal caliente o vapor: ¿cuál es la temperatura final?",
        "celdas": ["F3C3", "F3C2"],
        "trampa": "Comprueba SIEMPRE primero si Q_disponible alcanza para fundir todo el hielo (m_h·L_f). Si no alcanza, T_f = 0 °C y queda hielo.",
        "leccion": "PR2-D7",
    },
    {
        "sintoma": "Pared plana compuesta de varias capas en serie o dos barras en paralelo",
        "celdas": ["F2C1"],
        "trampa": "No saques R_th de la nada: despeja T_H − T_C = H·L/(kA) desde F2C1 y suma caídas (serie) o flujos (paralelo).",
        "leccion": "PR2-D3",
    },
    {
        "sintoma": "Tronco de cono, cilindro, esfera o barra con A(x) o k(x) variable",
        "celdas": ["F2C2", "F2C1"],
        "trampa": "Prohibido usar F2C1 directa. Parte de F2C2, sustituye A(x) o A(r) (cilindro = 2πrL, nunca πr²L), separa variables e integra.",
        "leccion": "PR1-4",
    },
    {
        "sintoma": "Objeto radiando calor dentro de un cuarto o ambiente a T_s",
        "celdas": ["F3C1", "F13C3", "F10C3"],
        "trampa": "T y T_s SIEMPRE en kelvin y calcula T⁴ − T_s⁴, NUNCA (T − T_s)⁴. Para despejar T suma T_s⁴ antes de sacar raíz cuarta.",
        "leccion": "P26-4",
    },
    {
        "sintoma": "Comparar velocidades o energía cinética media de dos gases (He vs O₂)",
        "celdas": ["F5C1", "F5C2", "F4C3"],
        "trampa": "A igual T tienen la MISMA K_med = (3/2)k_B T, pero el más liviano tiene mayor v_rms (∝ 1/√M, con M en kg/mol).",
        "leccion": "PR2-S6",
    },
    {
        "sintoma": "Ciclo p–V con recta inclinada (P = αV o trapecio)",
        "celdas": ["F6C1", "F4C1", "F7C1", "F7C2"],
        "trampa": "El trabajo es ∫p dV (área del trapecio), NO pΔV. El calor se saca por Q = ΔU + W, NO con C_V ni C_p.",
        "leccion": "PR1-5",
    },
    {
        "sintoma": "Ciclo p–V recorrido en sentido antihorario (W_neto < 0)",
        "celdas": ["F10C2", "F7C1"],
        "trampa": "Consume trabajo: es un REFRIGERADOR o bomba de calor. Se evalúa con K = |Q_c|/|W| (puede ser > 1), NO con eficiencia e.",
        "leccion": "PR1-6",
    },
    {
        "sintoma": "Ciclo con tramo isotérmico y tramo adiabático",
        "celdas": ["F6C3", "F9C1", "F9C2", "F9C3"],
        "trampa": "Isoterma: ΔU = 0, Q = W = nRT ln(V₂/V₁). Adiabática: Q = 0, W = −ΔU = (p₁V₁−p₂V₂)/(γ−1) y pV^γ = cte.",
        "leccion": "Q103",
    },
    {
        "sintoma": "No dicen si el gas es monoatómico o diatómico (información incompleta)",
        "celdas": ["F9C3", "F11C2", "F7C3", "F8C3"],
        "trampa": "Saca γ con logaritmos desde TV^(γ−1) = cte (o C_p desde Q_isob = nC_pΔT) y usa C_P = C_V + R + γ = C_P/C_V para identificar el gas.",
        "leccion": "PR2-D5",
    },
    {
        "sintoma": "Motor de gasolina / ciclo Otto / volumen en cm³ / despeje de r",
        "celdas": ["F11C3", "F12C1", "F9C3", "F14C2"],
        "trampa": "Convierte cm³ con 1 L = 1000 cm³. Si te dan e y piden r: r = (1/(1−e))^(1/(γ−1)), elevando a 1/(γ−1).",
        "leccion": "PR2-D9",
    },
    {
        "sintoma": "Calcular eficiencia térmica de un motor cualquiera (ciclo horario)",
        "celdas": ["F10C1", "F7C1"],
        "trampa": "En el denominador Q_H entran ÚNICAMENTE los procesos con Q > 0 (calor absorbido), nunca Q_neto.",
        "leccion": "Q105",
    },
)

FUERA_DE_HOJA_II_2026: Tuple[Dict[str, str], ...] = (
    {
        "tema": "Ciclo de Carnot (e_Carnot = 1 − T_c/T_H y K_Carnot = T_c/(T_H − T_c))",
        "historial": "Estuvo en 17/17 formularios previos · ELIMINADA en II-2026",
        "nota": "Corresponde a la sección 20.6, fuera del corte II-2026 (hasta 20.5). Si practicas un examen viejo (ej. E17c o Q122), dedúcela desde F10C1/F10C2 con isotermas y adiabáticas.",
    },
    {
        "tema": "Entropía (ΔS = ∫ dQ/T)",
        "historial": "Estuvo en 15/17 formularios previos (ausente ya en 25-II-O/E) · ELIMINADA en II-2026",
        "nota": "Corresponde a la sección 20.7, fuera del III parcial II-2026. No busques ΔS en la hoja del examen.",
    },
    {
        "tema": "Ecuación de Van der Waals y gases poliatómicos (C_V = 3R)",
        "historial": "Estuvo en 5/17 formularios previos · AUSENTE desde 25-II",
        "nota": "El formulario II-2026 incluye solo gas ideal monoatómico (F8C1) y diatómico (F8C2).",
    },
    {
        "tema": "Atajos derivados no impresos (R_th, ΔA = 2αA₀ΔT, β = 3α, cilindro/esfera/cono, C_V = R/(γ−1))",
        "historial": "Regla de desarrollo FI2103 II-2026",
        "nota": "SÍ entran en el examen, pero NO están impresos: debes escribir primero la casilla base de la hoja y mostrar el despeje o la integral.",
    },
)

# ========================================================== ENTRENADOR ACTIVO (TOCA LA CASILLA EN LA HOJA)
ESCENARIOS_ENTRENADOR: Tuple[Dict[str, Any], ...] = (
    {
        "id": "ENT-01",
        "categoria": "nuevas",
        "enunciado": "Una barra de acero inicialmente sin esfuerzo, rígidamente sujeta entre dos muros, se enfría 30 °C sin cambiar de longitud. ¿Qué casilla te da el esfuerzo y su signo?",
        "celdas_validas": ["F1C3"],
        "distractor": "F1C1",
        "explicacion": "F1C3 (F/A = −YαΔT) da el esfuerzo térmico cuando se impide el cambio de largo. Como ΔT < 0, F/A > 0 (tensión). F1C1 solo vale si la barra está libre.",
        "leccion": "PR2-S13",
    },
    {
        "id": "ENT-02",
        "categoria": "nuevas",
        "enunciado": "Un líquido tiene calor específico c = 1800 J/(kg·K) y te piden su valor en J/(kg·°F). ¿De qué casilla partes para relacionar ΔT en °F y °C?",
        "celdas_validas": ["F12C2"],
        "distractor": "F10C3",
        "explicacion": "Desde F12C2 (T_°F = (9/5)T_°C + 32) se obtiene ΔT_°F = 1,8·ΔT_°C (el 32 se cancela) y se divide 1800 / 1,8 = 1000 J/(kg·°F).",
        "leccion": "PR2-S1",
    },
    {
        "id": "ENT-03",
        "categoria": "trampas",
        "enunciado": "Una barra en forma de tronco de cono tiene radio r(x) = r₁ + bx y conduce calor estacionariamente. ¿De qué casilla DEBES partir?",
        "celdas_validas": ["F2C2"],
        "distractor": "F2C1",
        "explicacion": "Como el área A(x) = πr(x)² varía con x, F2C1 es inválida: debes partir de F2C2 (H = −kA dT/dx), separar variables e integrar.",
        "leccion": "PR1-4",
    },
    {
        "id": "ENT-04",
        "categoria": "trampas",
        "enunciado": "Una tubería cilíndrica con dos capas concéntricas de aislante pierde calor radialmente hacia el exterior. ¿Cuál es la ecuación base obligatoria?",
        "celdas_validas": ["F2C2"],
        "distractor": "F2C1",
        "explicacion": "El área radial A(r) = 2πrL cambia con el radio r: parte de F2C2 e integra dr/r para obtener el logaritmo ln(r_ext/r_int).",
        "leccion": "PR1-3",
    },
    {
        "id": "ENT-05",
        "categoria": "general",
        "enunciado": "Una pared plana de 2,50 m² tiene tres capas A, B y C en serie, cada una con espesor y conductividad constantes. ¿Qué casilla usas en cada capa?",
        "celdas_validas": ["F2C1"],
        "distractor": "F2C2",
        "explicacion": "Al ser capas planas con A y k constantes, parte directamente de F2C1 (H = kA(T_H − T_C)/L) y despeja la caída de temperatura de cada capa.",
        "leccion": "PR2-D3",
    },
    {
        "id": "ENT-06",
        "categoria": "trampas",
        "enunciado": "Una esfera metálica a 97 °C está dentro de una habitación cuyas paredes están a 27 °C. Piden la potencia NETA que pierde por radiación. ¿Cuál casilla usas?",
        "celdas_validas": ["F3C1"],
        "distractor": "F2C3",
        "explicacion": "Al haber entorno a T_s = 300,15 K, la pérdida neta sale de F3C1: H = Aeσ(T⁴ − T_s⁴) en kelvin. F2C3 solo da la emitida sin restar lo absorbido.",
        "leccion": "P26-4",
    },
    {
        "id": "ENT-07",
        "categoria": "general",
        "enunciado": "Un objeto negro duplica su temperatura absoluta de 300 K a 600 K y te preguntan por cuánto se multiplica la potencia que emite.",
        "celdas_validas": ["F2C3"],
        "distractor": "F3C1",
        "explicacion": "Con F2C3 (H = AeσT⁴), al duplicar T en kelvin la potencia emitida aumenta por 2⁴ = 16.",
        "leccion": "PR2-S10",
    },
    {
        "id": "ENT-08",
        "categoria": "trampas",
        "enunciado": "Se mete hierro caliente en un calorímetro con agua y hielo a 0 °C. Antes de calcular T_f, ¿qué casilla te dice cuánto calor cuesta fundir TODO el hielo?",
        "celdas_validas": ["F3C3"],
        "distractor": "F3C2",
        "explicacion": "F3C3 (Q = ±mL) da el calor latente de fusión m_h·L_f. Compararlo contra Q_máx del hierro evita caer en la trampa de fusión parcial (PR2-D7).",
        "leccion": "PR2-D7",
    },
    {
        "id": "ENT-09",
        "categoria": "general",
        "enunciado": "Cuatro cuerpos (aluminio, agua, hierro y cobre) se mezclan en un recipiente aislado sin cambios de fase. ¿Qué ecuación aplicas a cada cuerpo?",
        "celdas_validas": ["F3C2"],
        "distractor": "F3C3",
        "explicacion": "Sin cambios de fase, cada cuerpo aporta Q = mcΔT (F3C2) a la suma ΣQ = 0.",
        "leccion": "PR1-1",
    },
    {
        "id": "ENT-10",
        "categoria": "trampas",
        "enunciado": "Un ciclo p–V se recorre en sentido ANTIHORARIO (W_neto = −1869 J). ¿Con qué casilla medirías su desempeño?",
        "celdas_validas": ["F10C2"],
        "distractor": "F10C1",
        "explicacion": "Un ciclo antihorario consume trabajo (es un refrigerador o bomba de calor): se evalúa con K = |Q_c|/|W| (F10C2), NUNCA con eficiencia e = W/Q_H.",
        "leccion": "PR1-6",
    },
    {
        "id": "ENT-11",
        "categoria": "general",
        "enunciado": "Una máquina térmica libera al depósito frío el 65 % del calor que recibe del foco caliente. ¿Qué casilla da su eficiencia al instante?",
        "celdas_validas": ["F10C1"],
        "distractor": "F10C2",
        "explicacion": "F10C1 trae directamente e = 1 − |Q_c/Q_H| = 1 − 0,65 = 0,35 (35 %).",
        "leccion": "PR2-S2",
    },
    {
        "id": "ENT-12",
        "categoria": "nuevas",
        "enunciado": "Un motor de ciclo Otto con gas diatómico tiene razón de compresión r = 9,00 y te piden su eficiencia ideal (o la r necesaria para subirla).",
        "celdas_validas": ["F11C3"],
        "distractor": "F10C1",
        "explicacion": "F11C3 (e_otto = 1 − 1/r^(γ−1)) da la eficiencia directo de r y γ, y se invierte como r = (1/(1−e))^(1/(γ−1)).",
        "leccion": "PR2-D9",
    },
    {
        "id": "ENT-13",
        "categoria": "nuevas",
        "enunciado": "Un motor Otto parte de V₁ = 500 cm³ con razón de compresión r = 9,00 y piden el volumen mínimo al final de la compresión.",
        "celdas_validas": ["F12C1"],
        "distractor": "F11C3",
        "explicacion": "F12C1 (r = V_máx/V_mín) despeja directamente V_mín = V₁/r = 500/9 = 55,6 cm³.",
        "leccion": "PR2-D9",
    },
    {
        "id": "ENT-14",
        "categoria": "trampas",
        "enunciado": "Un gas ideal recorre una recta inclinada P = αV desde A hasta B. ¿De qué casilla debes partir para calcular el trabajo W_AB?",
        "celdas_validas": ["F6C1"],
        "distractor": "F6C2",
        "explicacion": "Como la presión cambia a lo largo del proceso, F6C2 (pΔV) es falsa: parte de F6C1 (W = ∫p dV) para integrar αV dV o hallar el área del trapecio.",
        "leccion": "PR1-5",
    },
    {
        "id": "ENT-15",
        "categoria": "trampas",
        "enunciado": "Un gas ideal se expande a temperatura constante desde 10 L hasta 30 L. ¿Qué casilla da el trabajo de la expansión?",
        "celdas_validas": ["F6C3"],
        "distractor": "F6C2",
        "explicacion": "En una expansión isotérmica la presión cae como 1/V: usa F6C3 (W = nRT ln(V₂/V₁)), nunca p(V₂ − V₁).",
        "leccion": "PR1-6",
    },
    {
        "id": "ENT-16",
        "categoria": "general",
        "enunciado": "En un proceso a presión constante de 200 kPa, el volumen crece de 0,030 m³ a 0,075 m³. ¿Qué casilla da el trabajo?",
        "celdas_validas": ["F6C2"],
        "distractor": "F6C3",
        "explicacion": "Al ser isobárico (p constante), el trabajo es directamente F6C2: W = p(V₂ − V₁).",
        "leccion": "PR2-D11",
    },
    {
        "id": "ENT-17",
        "categoria": "trampas",
        "enunciado": "Un gas ideal se expande a PRESIÓN CONSTANTE desde T_A hasta T_B. ¿Qué casilla calcula su cambio de energía interna ΔU?",
        "celdas_validas": ["F7C2"],
        "distractor": "F11C2",
        "explicacion": "¡Trampa clásica! En un gas ideal, ΔU = nC_VΔT (F7C2) vale en TODOS los procesos, incluso a presión constante. C_p solo se usa para Q_isob (F11C2).",
        "leccion": "PR2-D4",
    },
    {
        "id": "ENT-18",
        "categoria": "general",
        "enunciado": "En una expansión isobárica de 3 mol de gas desconocido conoces ΔT y el calor absorbido Q = 31,5 kJ. ¿Qué casilla te deja despejar C_p?",
        "celdas_validas": ["F11C2", "F3C2"],
        "distractor": "F11C1",
        "explicacion": "A presión constante el calor es Q = nC_pΔT (F11C2 / F3C2), lo que permite despejar C_p y luego hallar C_V y γ.",
        "leccion": "PR2-D11",
    },
    {
        "id": "ENT-19",
        "categoria": "general",
        "enunciado": "Tres moles de un gas ideal desconocido se expanden adiabáticamente al doble de volumen bajando de 300 K a 227,4 K. ¿Qué casilla te permite hallar γ con logaritmos?",
        "celdas_validas": ["F9C3"],
        "distractor": "F9C2",
        "explicacion": "Como conoces temperaturas T₁, T₂ y razón de volúmenes V₂/V₁, usa F9C3 (TV^(γ−1) = cte) y aplica ln para despejar γ − 1.",
        "leccion": "PR2-D5",
    },
    {
        "id": "ENT-20",
        "categoria": "general",
        "enunciado": "Conoces p_B, V_B y V_C en una compresión adiabática con γ = 1,40 y necesitas la presión final p_C. ¿Qué casilla usas?",
        "celdas_validas": ["F9C2"],
        "distractor": "F9C3",
        "explicacion": "Para enlazar presión y volumen en un tramo adiabático se usa F9C2: pV^γ = cte (con exponente γ, no γ − 1).",
        "leccion": "PR1-6",
    },
    {
        "id": "ENT-21",
        "categoria": "general",
        "enunciado": "Ya despejaste γ = 1,40 de una adiabática y necesitas encontrar C_V y C_P sin sacar atajos de la nada. ¿Qué par de casillas combinas? (Toca una de ellas)",
        "celdas_validas": ["F7C3", "F8C3"],
        "distractor": "F8C1",
        "explicacion": "Combinando F7C3 (C_P = C_V + R) con F8C3 (γ = C_P/C_V) se demuestra C_V = R/(γ−1) y C_P = C_V + R.",
        "leccion": "PR2-D5",
    },
    {
        "id": "ENT-22",
        "categoria": "trampas",
        "enunciado": "A igual cantidad de moles y mismo calor suministrado a volumen constante, ¿por qué un gas monoatómico se calienta más que uno diatómico? Toca el C_V del monoatómico.",
        "celdas_validas": ["F8C1"],
        "distractor": "F8C2",
        "explicacion": "F8C1 (C_Vmono = 3/2 R) es menor que F8C2 (C_Vdia = 5/2 R): con igual Q, ΔT = Q/(nC_V) resulta 5/3 mayor en el monoatómico.",
        "leccion": "PR2-S11",
    },
    {
        "id": "ENT-23",
        "categoria": "general",
        "enunciado": "Tienes dos gases (helio y oxígeno) a la misma temperatura y te preguntan cuál tiene moléculas con mayor rapidez cuadrática media.",
        "celdas_validas": ["F5C1"],
        "distractor": "F5C2",
        "explicacion": "F5C1 (v_rms = √(3RT/M)) muestra que a igual T el gas de menor masa molar M (helio) es más rápido. F5C2 da energía cinética media, que es igual para ambos.",
        "leccion": "PR2-S6",
    },
    {
        "id": "ENT-24",
        "categoria": "trampas",
        "enunciado": "Un gas ideal está a 20 °C y preguntan a qué temperatura se duplica la energía cinética traslacional media por molécula. ¿Qué casilla relaciona K_med con T?",
        "celdas_validas": ["F5C2"],
        "distractor": "F5C1",
        "explicacion": "F5C2 ((1/2)m(v²)_med = (3/2)k_B T) dice que K_med es proporcional a T en KELVIN (hay que duplicar 293,15 K → 586,3 K = 313 °C, no 40 °C).",
        "leccion": "Q081",
    },
    {
        "id": "ENT-25",
        "categoria": "nuevas",
        "enunciado": "Un frasco de vidrio lleno hasta el borde con glicerina se calienta 40 °C y te piden cuánto líquido se derrama. ¿Qué casilla describe la expansión del volumen?",
        "celdas_validas": ["F1C2"],
        "distractor": "F1C3",
        "explicacion": "F1C2 (ΔV = βV₀ΔT) reintroducida en II-2026 da la expansión volumétrica del líquido y del recipiente (con β_rec = 3α desde F1C1).",
        "leccion": "E2",
    },
    {
        "id": "ENT-26",
        "categoria": "nuevas",
        "enunciado": "Un cilindro de motor tiene volumen inicial V₁ = 500 cm³ y necesitas pasarlo a litros (y luego a m³). ¿Qué casilla de equivalencia viene nueva al pie de la hoja?",
        "celdas_validas": ["F14C2"],
        "distractor": "F14C1",
        "explicacion": "F14C2 (1 L = 1000 cm³) da 500 cm³ = 0,500 L = 5,00×10⁻⁴ m³ (recuerda: 1 cm³ = 10⁻⁶ m³).",
        "leccion": "P26-3",
    },
    {
        "id": "ENT-27",
        "categoria": "nuevas",
        "enunciado": "Te dan la masa molar M = 0,0280 kg/mol de N₂ y necesitas la masa individual m de UNA sola molécula. ¿Qué casilla de la hoja usas?",
        "celdas_validas": ["F5C3"],
        "distractor": "F4C2",
        "explicacion": "F5C3 (M = N_A m) despeja m = M/N_A. F4C2 (m_total = nM) es para la masa total de la muestra.",
        "leccion": "P26-3",
    },
    {
        "id": "ENT-28",
        "categoria": "trampas",
        "enunciado": "Vas a calcular el trabajo W = nRT ln(V₂/V₁) en Joules para un gas cuya presión inicial venía en atmósferas. ¿Cuál de las dos constantes R DEBES usar?",
        "celdas_validas": ["F12C3"],
        "distractor": "F14C3",
        "explicacion": "Para obtener Joules usa SIEMPRE F12C3 (R = 8,314 J/(mol·K)). Meter F14C3 (0,08206) en una fórmula de energía es un error fatal de unidades.",
        "leccion": "Q105",
    },
    {
        "id": "ENT-29",
        "categoria": "general",
        "enunciado": "En un ciclo cerrado un gas ideal absorbe calor neto positivo Q_neto > 0. ¿Qué casilla demuestra inmediatamente que W_neto = Q_neto > 0?",
        "celdas_validas": ["F7C1"],
        "distractor": "F10C1",
        "explicacion": "Con la primera ley F7C1 (ΔU = Q − W), al cerrar un ciclo ΔU_ciclo = 0, de modo que W_neto = Q_neto.",
        "leccion": "PR2-S12",
    },
    {
        "id": "ENT-30",
        "categoria": "general",
        "enunciado": "Un gas ideal en un recipiente rígido de 30,0 L a 80,0 °C tiene n = 2,00 mol y piden su presión en kPa. ¿Qué ecuación de estado usas?",
        "celdas_validas": ["F4C1"],
        "distractor": "F4C3",
        "explicacion": "F4C1 (pV = nRT) despeja p = nRT/V, convirtiendo primero 30,0 L = 0,0300 m³ y 80,0 °C = 353,15 K.",
        "leccion": "Q074",
    },
    {
        "id": "ENT-31",
        "categoria": "general",
        "enunciado": "Te piden demostrar que el trabajo en una compresión adiabática vale (p₁V₁ − p₂V₂)/(γ − 1) y luego evaluarlo directamente con presiones y volúmenes.",
        "celdas_validas": ["F9C1"],
        "distractor": "F6C2",
        "explicacion": "F9C1 da el trabajo adiabático directo desde presiones y volúmenes extremos (equivale a −ΔU porque Q = 0).",
        "leccion": "PR1-6",
    },
    {
        "id": "ENT-32",
        "categoria": "trampas",
        "enunciado": "Piden demostrar por qué la presión microscópica de un gas monoatómico es proporcional a su energía cinética media (K = (3/2)pV). ¿Qué casilla enlaza K con nRT?",
        "celdas_validas": ["F4C3"],
        "distractor": "F7C1",
        "explicacion": "Combinando F4C3 (K = (3/2)nRT) con F4C1 (pV = nRT) resulta pV = (2/3)K = (2/3)N·K_med.",
        "leccion": "Q078",
    },
)


_CANDADO = threading.Lock()
_CACHE_GUIA: Dict[str, Any] | None = None


def _prioridad_ejemplo(lid: str, tipo: str) -> Tuple[int, str]:
    """Ordena ejemplos priorizando prácticas II-2026, blindaje P26-* y clásicos de examen."""
    if lid.startswith("P26-"):
        return (0, lid)
    if lid.startswith("PR1-") or lid.startswith("PR2-D"):
        return (1, lid)
    if lid.startswith("PR2-S"):
        return (2, lid)
    if lid in ("Q049", "Q105", "Q103", "Q110", "Q111", "Q122", "Q068", "Q022", "E21", "E16", "E8", "E2"):
        return (3, lid)
    if tipo == "desarrollo":
        return (4, lid)
    if tipo == "seleccion":
        return (5, lid)
    return (6, lid)


def _recopilar_ejemplos() -> Dict[str, List[Dict[str, str]]]:
    por_celda: Dict[str, List[Tuple[Tuple[int, str], Dict[str, str]]]] = {}
    for lid in registro.ids():
        lec = registro.obtener(lid)
        if lec.tipo == "teoria":
            continue
        j = lec.a_json()
        mapa = mapear_formulario_leccion(j)
        prio = _prioridad_ejemplo(lid, lec.tipo)
        item = {"id": lid, "titulo": lec.titulo, "tipo": lec.tipo}
        for c in mapa.get("celdas") or []:
            por_celda.setdefault(c["id"], []).append((prio, item))
    salida: Dict[str, List[Dict[str, str]]] = {}
    for cid, lista in por_celda.items():
        lista.sort(key=lambda x: x[0])
        salida[cid] = [it for _, it in lista[:5]]
    return salida


def guia_completa() -> Dict[str, Any]:
    """Construye y cachea el catálogo completo de estudio de las 42 casillas II-2026."""
    global _CACHE_GUIA
    with _CANDADO:
        if _CACHE_GUIA is not None:
            return _CACHE_GUIA

        cat = catalogo_formulario()
        ejemplos_por_celda = _recopilar_ejemplos()
        bloque_por_id = {b["id"]: b for b in BLOQUES_FORMULARIO}

        celdas_out: List[Dict[str, Any]] = []
        for c in cat["celdas"]:
            cid = c["id"]
            info = GUIA_42_CELDAS[cid]
            blk = bloque_por_id[info["bloque"]]
            if cid in _SIMBOLOS_CONSTANTES:
                sims = _SIMBOLOS_CONSTANTES[cid]
            else:
                sims = simbolos.simbolos_de(c["tex"], simbolos.grupo_celda(cid))
            celdas_out.append({
                **c,
                "bloque": info["bloque"],
                "bloque_nombre": blk["nombre"],
                "color": blk["color"],
                "estado_ii_2026": info["estado_ii_2026"],
                "prioridad": info["prioridad"],
                "que_es": info["que_es"],
                "cuando_si": list(info["cuando_si"]),
                "cuando_no": list(info["cuando_no"]),
                "unidades": info["unidades"],
                "errores": list(info["errores"]),
                "confundir_con": list(info["confundir_con"]),
                "se_deriva": list(info["se_deriva"]),
                "pistas_enunciado": list(info["pistas_enunciado"]),
                "patron_examen": dict(info["patron_examen"]),
                "simbolos": sims,
                "ejemplos": ejemplos_por_celda.get(cid, []),
            })

        _CACHE_GUIA = {
            "titulo": "Estudio del formulario oficial · III Parcial FI2103 II-2026",
            "hoja_url": cat["hoja_url"],
            "aspecto": cat["aspecto"],
            "total": len(celdas_out),
            "bloques": list(BLOQUES_FORMULARIO),
            "celdas": celdas_out,
            "decisiones": list(DECISIONES_RAPIDAS),
            "fuera_de_hoja": list(FUERA_DE_HOJA_II_2026),
            "entrenador": list(ESCENARIOS_ENTRENADOR),
        }
        return _CACHE_GUIA
