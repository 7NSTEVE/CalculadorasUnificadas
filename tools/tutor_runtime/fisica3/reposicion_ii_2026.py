"""Reposición II-2026 de Física General III (FI2103): qué viene, con qué hoja y qué estudiar cada día.

Todo sale de leer los PDF oficiales de la cátedra (27-nov-2026):
  * las reposiciones II-2023, I-2024, II-2024, II-2025 y I-2026 (enunciados y soluciones);
  * los formularios de reposición II-2023, I-2024, II-2024, I-2025, II-2025 y I-2026;
  * las hojas de los tres parciales del II-2026 (`formularios_p1_p2.py` y `formulario_ii_2026.py`);
  * el I y el II parcial del II-2026 (ordinario y extraordinario) y el III parcial ordinario.

Al 27-nov-2026 la cátedra no había publicado ni el formulario ni la fecha de la reposición
(revisado en el TEC Digital: «Documentos II - 2026» y noticias de la cátedra y del grupo).
Lo que aquí se predice sale de contar exámenes reales: sirve para ordenar el estudio, no es una garantía.

El plan por días pide lecciones por su ID. Si una lección todavía no existe, el día la muestra
como «en preparación» y sigue funcionando: así este archivo no depende del orden en que se escriben
los bancos nuevos.
"""

from __future__ import annotations

from collections import Counter
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

from . import registro

# ---------------------------------------------------------------------- tipos de problema

# clave -> (nombre corto, parcial)
ARQUETIPOS: Dict[str, Tuple[str, int]] = {
    "resorte_fase": ("Resorte con fase: x(t), período y energía", 1),
    "cuerdas": ("Cuerdas: tensión, μ, frecuencias y armónicos", 1),
    "estacionaria": ("Onda estacionaria: nodos, antinodos y ondas viajeras", 1),
    "pendulo": ("Péndulo: cómo cambia el período", 1),
    "bernoulli": ("Continuidad y Bernoulli: tanque, tubo o Venturi", 2),
    "poiseuille": ("Fluido viscoso: Poiseuille y Reynolds", 2),
    "fuerza_pared": ("Fuerza del agua sobre una pared o compuerta (integral)", 2),
    "flotacion": ("Flotación (Arquímedes)", 2),
    "caudal": ("Caudal y flujo másico", 2),
    "ciclo": ("Ciclo termodinámico: tabla pVT, W y Q por etapa, e o K", 3),
    "conduccion_variable": ("Conducción con k o área que cambian (integral)", 3),
    "conduccion": ("Conducción por una pared: H = kAΔT/L", 3),
    "calor": ("Calor sensible: Q = mcΔT", 3),
}

# ---------------------------------------------------------------------- reposiciones reales

# Cada ítem: (número, parcial, tipo, puntos, arquetipo, lección, resumen)
_Item = Tuple[str, int, str, int, str, str, str]

REPOSICIONES: Tuple[Dict[str, Any], ...] = (
    {
        "examen": "23-II-R", "nombre": "Reposición II-2023", "puntos": 43, "minutos": 140,
        "formato": "3 desarrollos",
        "items": (
            ("1", 1, "desarrollo", 17, "estacionaria", "RP-23II-1", "Onda estacionaria: antinodos, viajeras y rapidez transversal máxima"),
            ("2", 2, "desarrollo", 14, "poiseuille", "RP-23II-2", "Aceite que sale por un tubo: Δp, Poiseuille, velocidad y Reynolds"),
            ("3", 3, "desarrollo", 12, "ciclo", "Q106", "Ciclo rectangular de 2 mol diatómico: W neto, Q_H, Q_C y e"),
        ),
    },
    {
        "examen": "24-I-R", "nombre": "Reposición I-2024", "puntos": 60, "minutos": 140,
        "formato": "3 desarrollos de 20 puntos",
        "items": (
            ("1", 1, "desarrollo", 20, "cuerdas", "RP-24I-1", "Instrumento de 2 cuerdas con igual tensión y un traste"),
            ("2", 2, "desarrollo", 20, "fuerza_pared", "RP-24I-2", "Compuerta mitad rectangular y mitad parabólica: dF y F total"),
            ("3", 3, "desarrollo", 20, "ciclo", "Q107", "Ciclo rectangular en términos de p₀ y V₀"),
        ),
    },
    {
        "examen": "24-II-R", "nombre": "Reposición II-2024", "puntos": 40, "minutos": 120,
        "formato": "3 desarrollos",
        "items": (
            ("1", 1, "desarrollo", 12, "resorte_fase", "RP-24II-1", "Masa-resorte: T, k, fase φ = π y desplazamiento"),
            ("2", 2, "desarrollo", 14, "bernoulli", "RP-24II-2", "Tubo Venturi con dos tubos verticales: v, Δp y Δh"),
            ("3", 3, "desarrollo", 14, "ciclo", "Q108", "Ciclo de 3 etapas con trapecios (el inciso de entropía ya no entra)"),
        ),
    },
    {
        "examen": "25-I-R", "nombre": "Reposición I-2025", "puntos": 45, "minutos": 135,
        "formato": "3 desarrollos",
        "items": (),
        "nota": "De este examen solo se tiene el formulario y las instrucciones, no las preguntas.",
    },
    {
        "examen": "25-II-R", "nombre": "Reposición II-2025", "puntos": 60, "minutos": 150,
        "formato": "4 desarrollos de 15 puntos",
        "items": (
            ("1", 1, "desarrollo", 15, "resorte_fase", "RP-25II-1", "MAS leído de la gráfica v(t): T, ω, A, φ y x, v, a"),
            ("2", 2, "desarrollo", 15, "fuerza_pared", "RP-25II-2", "Represa trapezoidal (integral) y tubería con Bernoulli"),
            ("3", 3, "desarrollo", 15, "conduccion_variable", "Q050", "Lámina con k(y) = 200 + 1000y: H, energía y T a la mitad"),
            ("4", 3, "desarrollo", 15, "ciclo", "Q109", "Ciclo isocórico-adiabático-isobárico de 0,378 mol"),
        ),
    },
    {
        "examen": "26-I-R", "nombre": "Reposición I-2026", "puntos": 52, "minutos": 150,
        "formato": "6 selecciones de 3 puntos + 3 desarrollos",
        "items": (
            ("1", 1, "seleccion", 3, "cuerdas", "RP-26I-1", "Frecuencia fundamental de la cuerda de un contrabajo"),
            ("2", 1, "seleccion", 3, "pendulo", "RP-26I-2", "Período de un péndulo si L se cuadruplica"),
            ("3", 2, "seleccion", 3, "flotacion", "RP-26I-3", "Dos esferas iguales hundidas al 40 % y al 60 %"),
            ("4", 2, "seleccion", 3, "caudal", "RP-26I-4", "Rapidez de salida desde el flujo másico"),
            ("5", 3, "seleccion", 3, "conduccion", "Q051", "Calor por una pared de ladrillo"),
            ("6", 3, "seleccion", 3, "calor", "Q020", "Temperatura final de un bloque de aluminio"),
            ("7", 1, "desarrollo", 9, "resorte_fase", "RP-26I-7", "Masa-resorte: T, x(t) con la fase correcta y energía"),
            ("8", 2, "desarrollo", 9, "bernoulli", "RP-26I-8", "Tanque con nivel constante: caudal, rapidez y profundidad"),
            ("9", 3, "desarrollo", 16, "ciclo", "Q110", "Refrigerador de 3 etapas con 2,50 mol de helio"),
        ),
    },
)

# ---------------------------------------------------------------------- formulario esperado

# Casillas que nunca estuvieron en una hoja de reposición (II-2023 a I-2026) y que sí están en las
# hojas del II-2026. «salio_en» dice si ya se preguntaron en un parcial de este semestre.
CASILLAS_NUEVAS: Tuple[Dict[str, str], ...] = (
    {"id": "P1-F6C1", "parcial": "1", "tex": "I = P/(4πr²)",
     "salio_en": "Sí: I parcial ordinario II-2026, selección 2 (el dron que emite sonido)."},
    {"id": "P1-F6C2", "parcial": "1", "tex": "I₁/I₂ = r₂²/r₁²",
     "salio_en": "Sí: la misma selección del dron (a qué distancia la intensidad llega a 1 W/m²)."},
    {"id": "P2-F3C3", "parcial": "2", "tex": "γ = F/(2l)",
     "salio_en": "Sí: II parcial extraordinario II-2026, selección 2 (marco de alambre)."},
    {"id": "P2-F8C3", "parcial": "2", "tex": "F = ηAv/ℓ",
     "salio_en": "Todavía no en ningún examen del II-2026."},
    {"id": "F1C3", "parcial": "3", "tex": "F/A = −YαΔT",
     "salio_en": "Todavía no en ningún examen del II-2026."},
    {"id": "F12C2", "parcial": "3", "tex": "T(°F) = (9/5)·T(°C) + 32",
     "salio_en": "Sí: III parcial ordinario II-2026, selección 1 (C_p en °F)."},
)

# Casillas que regresan (estuvieron en II-2023 e I-2024 pero no en I-2026)
CASILLAS_QUE_VUELVEN: Tuple[Dict[str, str], ...] = (
    {"id": "F1C2", "parcial": "3", "tex": "ΔV = βV₀ΔT",
     "salio_en": "Estuvo en las hojas de II-2023 e I-2024; no en I-2026. Todavía no se pregunta en el II-2026."},
)

FUERA_DE_LA_HOJA: Tuple[str, ...] = (
    "Ciclo de Carnot (sección 20.6): e = 1 − T_C/T_H y K de Carnot.",
    "Entropía (sección 20.7): ΔS = ∫dQ/T.",
    "Van der Waals y C_V = 3R (ya no estaban en I-2026).",
)

REGLAS: Tuple[str, ...] = (
    "La hoja de la reposición es la de los 3 parciales del semestre, pegadas una tras otra. En I-2026 solo agregó 3 líneas (dilatación lineal y dU = dQ − dW) y quitó 1.",
    "Siempre hay al menos un problema de cada parcial.",
    "El problema del Parcial 3 siempre es un ciclo (salió en 5 de 5) y es el que más vale.",
    "Lo que no está en la hoja hay que demostrarlo desde la hoja (por ejemplo, Torricelli desde Bernoulli).",
    "Lo que sacaron de la hoja no se pregunta: Carnot y entropía ya no entran.",
    "Las casillas nuevas del semestre se preguntaron en selecciones de los parciales: repasarlas sale barato.",
    "La reposición recicla las preguntas de los parciales del mismo semestre: en I-2026, 9 de 9 (selecciones casi textuales, desarrollos con otros números). Por eso el plan trae las 45 preguntas de los parciales II-2026.",
    "La hoja de termo de la reposición I-2026 fue la del III extraordinario. Cuando salga la solución del III extraordinario II-2026, va primero.",
)


def _catalogos() -> Dict[int, List[Dict[str, Any]]]:
    from .formulario_ii_2026 import catalogo_formulario
    from .formularios_p1_p2 import catalogo_formulario_parcial

    return {
        1: list(catalogo_formulario_parcial(1)["celdas"]),
        2: list(catalogo_formulario_parcial(2)["celdas"]),
        3: list(catalogo_formulario()["celdas"]),
    }


def hoja_predicha() -> Dict[str, Any]:
    """La hoja que se espera para la reposición: las tres hojas del II-2026, en orden."""
    nuevas = {c["id"]: c for c in CASILLAS_NUEVAS}
    vuelven = {c["id"]: c for c in CASILLAS_QUE_VUELVEN}
    partes = []
    total = 0
    for parcial, celdas in _catalogos().items():
        lista = []
        for c in celdas:
            marca = "nueva" if c["id"] in nuevas else "vuelve" if c["id"] in vuelven else ""
            lista.append({
                "id": c["id"], "nombre": c["nombre"], "tex": c["tex"],
                "es_constante": bool(c.get("es_constante")), "marca": marca,
            })
        total += len(lista)
        partes.append({"parcial": parcial, "casillas": len(lista), "celdas": lista})
    return {
        "estado": "Predicción: todavía no está publicada (revisado en el TEC Digital el 27-nov-2026).",
        "total": total,
        "partes": partes,
    }


# ---------------------------------------------------------------------- recetas fijas

RECETAS: Tuple[Dict[str, Any], ...] = (
    {
        "arquetipo": "ciclo",
        "titulo": "Ciclo (Parcial 3)",
        "casillas": ["F4C1", "F9C2", "F6C2", "F7C2", "F7C1", "F10C1", "F10C2"],
        "pasos": [
            "Dibujá el ciclo p-V con flechas. Horario = motor (pide e). Antihorario = refrigerador (pide K).",
            "Tabla de estados: p, V y T en cada punto con pV = nRT. En una adiabática usá pV^γ = constante o TV^(γ−1) = constante.",
            "Trabajo de cada etapa con su casilla: isobárica p(V₂ − V₁); isocórica 0; isotérmica nRT ln(V₂/V₁); adiabática (p₁V₁ − p₂V₂)/(γ − 1); recta: área del trapecio.",
            "ΔU = nC_VΔT en cada etapa y después Q = ΔU + W.",
            "Q_H es la suma de los Q positivos. Motor: e = W_neto/Q_H. Refrigerador: K = |Q_C|/|W|.",
        ],
    },
    {
        "arquetipo": "bernoulli",
        "titulo": "Tanque o tubo (Parcial 2)",
        "casillas": ["P2-F5C3", "P2-F3C1", "P2-F6C1", "P2-F6C2"],
        "pasos": [
            "Pasá todo a metros y diámetros a radios. Área de un círculo: A = πr².",
            "Caudal dV/dt = Av. Si dan kg/s: dm/dt = ρ·dV/dt.",
            "En otra sección del mismo tubo: A₁v₁ = A₂v₂.",
            "Bernoulli entre dos puntos. Tachá lo que es igual: misma altura, p_atm en los dos, v ≈ 0 en la superficie de un tanque grande.",
            "Despejá lo que piden. Si usás v = √(2gh), primero demostralo desde Bernoulli.",
        ],
    },
    {
        "arquetipo": "fuerza_pared",
        "titulo": "Fuerza sobre una pared (Parcial 2, con integral)",
        "casillas": ["P2-F5C1", "P2-F2C3", "P2-F4C1"],
        "pasos": [
            "Dibujá la pared y una franja horizontal delgada a profundidad h; su alto es dh.",
            "Ancho de la franja L(h): número fijo (rectángulo), recta (trapecio) o despejado de la curva (parábola).",
            "Presión en la franja: p = ρgh. Si del otro lado hay aire, p₀ se cancela.",
            "Fuerza en la franja: dF = p·dA = ρgh·L(h)·dh.",
            "Sumá todas las franjas: F = ∫dF desde arriba hasta abajo, con la regla de la potencia.",
        ],
    },
    {
        "arquetipo": "resorte_fase",
        "titulo": "Resorte con fase (Parcial 1)",
        "casillas": ["P1-F1C4", "P1-F3C2", "P1-F3C3", "P1-F2C2", "P1-F2C3", "P1-F6C3"],
        "pasos": [
            "ω = √(k/m) y T = 2π/ω.",
            "Amplitud: dato, o A = √(x₀² + v₀²/ω²).",
            "Fase: x(0) = A cos φ da dos ángulos. El signo de v(0) = −ωA sen φ elige uno.",
            "Escribí x(t) = A cos(ωt + φ) con los números en unidades del SI.",
            "Energía: E = ½kA².",
        ],
    },
    {
        "arquetipo": "cuerdas",
        "titulo": "Cuerdas y ondas estacionarias (Parcial 1)",
        "casillas": ["P1-F9C4", "P1-F10C1", "P1-F10C2", "P1-F10C3", "P1-F8C3", "P1-F9C1"],
        "pasos": [
            "μ = m/L y v = √(F/μ).",
            "Armónico n: λ_n = 2L/n y f_n = v/λ_n = n·f₁.",
            "Si dan y(x,t) = A_OE sen(kx) sen(ωt), leé k y ω. Después v = ω/k y λ = 2π/k.",
            "Cada onda viajera tiene la mitad de la amplitud (hay que demostrarlo).",
            "Rapidez transversal máxima en un antinodo: A_OE·ω.",
        ],
    },
    {
        "arquetipo": "seleccion",
        "titulo": "Selecciones (las 6 de 3 puntos)",
        "casillas": [],
        "pasos": [
            "Subrayá qué cambia y qué se mantiene igual.",
            "Escribí la casilla de la hoja que une esas cantidades.",
            "Tachá lo que se cancela y quedate con una razón (por ejemplo, T₂/T₁).",
            "Calculá con números redondos y buscá la letra.",
            "Si dudás entre dos, probá cada una con la casilla: no adivines.",
        ],
    },
)

# ---------------------------------------------------------------------- calendario

# (id, día, fecha ISO, título, subtítulo, es_simulacro, items[(examen, número, lección, meta)])
_Dia = Tuple[str, int, str, str, str, bool, Tuple[Tuple[str, str, str, str], ...]]

CALENDARIO: Tuple[_Dia, ...] = (
    ("D1", 1, "2026-11-27", "Día 1 · vie 27-nov · El ciclo, el problema que más vale",
     "La reposición siempre trae un ciclo. Hoy: tabla pVT, W y Q por etapa, e o K.", False, (
         ("26-I-R", "9", "Q110", "Refrigerador de 2,50 mol de helio: tabla pVT, W, Q y K (salió en I-2026, 16 pts)."),
         ("25-II-R", "4", "Q109", "Ciclo isocórico, adiabático e isobárico de 0,378 mol diatómico."),
         ("26-II-O", "2", "O26-2", "Selección P3: con 20 % de eficiencia, ¿qué relación hay entre W y Q_H?"),
         ("Teoría", "1", "T-MAT1", "Integrales 1 de 9: qué es dT/dx y qué es una integral, sin saber nada antes."),
     )),
    ("D2", 2, "2026-11-28", "Día 2 · sáb 28-nov · Más ciclos y las selecciones del Parcial 3",
     "Dos ciclos más, las 2 selecciones de termo de I-2026 y dos preguntas del III parcial.", False, (
         ("24-II-R", "3", "Q108", "Ciclo de 3 etapas con trapecios (el inciso de entropía ya no entra)."),
         ("23-II-R", "3", "Q106", "Ciclo rectangular de 2 mol diatómico: W neto, Q_H, Q_C y e."),
         ("26-I-R", "5", "Q051", "Selección: calor por una pared de ladrillo con H = kAΔT/L."),
         ("26-I-R", "6", "Q020", "Selección: temperatura final de un bloque con Q = mcΔT."),
         ("26-II-O", "6", "O26-6", "Selección P3: por qué C_V de un diatómico sube al calentarlo."),
         ("26-II-O", "10", "O26-10", "Desarrollo P3: agua y aluminio a 38,3 °C con hielo a −15,0 °C."),
         ("26-II-O", "7", "O26-7", "Selección P3: dos motores Otto con la misma razón de compresión (pregunta 7)."),
         ("Teoría", "2", "T-INT0", "Integrales 2 de 9: integrar es sumar franjas; con más franjas la suma se acerca al número exacto."),
     )),
    ("D3", 3, "2026-11-29", "Día 3 · dom 29-nov · Fluidos 1: presión, flotación y continuidad",
     "Selecciones cortas: casi todas se resuelven con una sola casilla.", False, (
         ("26-I-R", "3", "RP-26I-3", "Dos esferas iguales hundidas al 40 % y al 60 %: ¿qué fluido es más denso?"),
         ("26-I-R", "4", "RP-26I-4", "Rapidez de salida a partir de 20 kg/s."),
         ("26-II-2O", "2", "P2O26-2", "Madera que flota en agua y luego en aceite: empuje y volumen hundido."),
         ("26-II-2O", "4", "P2O26-4", "Tubería que se angosta de 6 cm a 3 cm: ¿cuánto sube la rapidez?"),
         ("26-II-2E", "1", "P2E26-1", "Escultura sacada del mar: tensión del cable con el empuje."),
         ("26-II-2O", "7", "P2O26-7", "Cuerpo a 2000 m de profundidad: cuánto se comprime con B."),
         ("26-II-2O", "3", "P2O26-3", "Selección: qué supone el modelo de fluido ideal."),
         ("Teoría", "3", "T-INT1", "Integrales 3 de 9: una sola regla, ∫hⁿ dh = hⁿ⁺¹/(n+1), y «arriba menos abajo»."),
     )),
    ("D4", 4, "2026-11-30", "Día 4 · lun 30-nov · Fluidos 2: Bernoulli en tanques y tubos",
     "El desarrollo de fluidos de I-2026 fue un tanque con Bernoulli.", False, (
         ("26-I-R", "8", "RP-26I-8", "Tanque con nivel constante: caudal, rapidez de salida y profundidad (9 pts)."),
         ("24-II-R", "2", "RP-24II-2", "Tubo Venturi con dos tubos verticales: v, Δp y Δh."),
         ("26-II-2O", "5", "P2O26-5", "Tubería que se angosta: qué pasa con la rapidez y con la presión."),
         ("26-II-2E", "3", "P2E26-3", "Tanque con un orificio: de qué depende la rapidez de salida."),
         ("26-II-2E", "6", "P2E26-6", "Tubo de Pitot: rapidez del aire desde dos presiones."),
         ("26-II-2E", "7", "P2E26-7", "Agua que baja por un tubo que se ensancha: P_A frente a P_B."),
         ("26-II-2E", "5", "P2E26-5", "Tubo que se angosta con dos tubos verticales: diferencia de alturas."),
         ("Teoría", "4", "T-PLANTEO", "Integrales 4 de 9: cómo se plantea, con 5 preguntas y 3 pedacitos (franja, disco y cascarón)."),
     )),
    ("D5", 5, "2026-12-01", "Día 5 · mar 1-dic · Integrales: la franja en paredes y compuertas",
     "Las cinco integrales del curso y la franja con todas sus formas: rectángulo, trapecio, ventana y semicírculo.", False, (
         ("Teoría", "5", "T-MAT2", "Integrales 5 de 9: las cinco integrales que salen en el curso, con un ejemplo cada una."),
         ("Teoría", "6", "T-PARED", "Armar la fuerza sobre una pared en 5 pasos: franja, ancho, presión, dF y suma."),
         ("26-II-2E", "8", "P2E26-8", "Ventana cuadrada en un tanque: la integral más corta que pueden pedir."),
         ("Rec-26", "8", "E2-8", "Puerta semicircular: el ancho de la franja sale de la ecuación del círculo."),
     )),
    ("D6", 6, "2026-12-02", "Día 6 · mié 2-dic · Integrales: compuertas, represas y la esfera cortada",
     "Las dos integrales que ya salieron en reposición, presión con varios líquidos y la esfera cortada del III parcial.", False, (
         ("24-I-R", "2", "RP-24I-2", "Compuerta mitad rectángulo y mitad parábola: dF y fuerza total."),
         ("25-II-R", "2", "RP-25II-2", "Represa trapezoidal con integral y tubería con Bernoulli."),
         ("26-I-SUF", "3", "SUF26-3", "Tanque con agua de mar y aceite: compuerta y chorro por un orificio."),
         ("26-II-2O", "8", "P2O26-8", "Manómetro con agua, mercurio, glicerina y un líquido X (20 pts)."),
         ("26-II-O", "8", "O26-8", "Desarrollo P3: conducción en una esfera cortada por dos planos."),
     )),
    ("D7", 7, "2026-12-03", "Día 7 · jue 3-dic · MAS y péndulos (Parcial 1)",
     "En I-2026 la reposición repitió el resorte con fase y el péndulo de sus parciales.", False, (
         ("26-I-R", "7", "RP-26I-7", "Masa-resorte: T, x(t) con la fase correcta y energía (9 pts)."),
         ("26-II-1O", "8", "P1O26-8", "Bloque en MAS leído de las curvas de energía (18 pts)."),
         ("26-II-1E", "8", "P1E26-8", "Péndulo simple contra péndulo físico de 2,00 m."),
         ("26-I-R", "2", "RP-26I-2", "Selección: período de un péndulo si L se cuadruplica."),
         ("26-II-1O", "1", "P1O26-1", "Selección: MAS con el doble de amplitud y el mismo período."),
         ("26-II-1O", "3", "P1O26-3", "Selección: péndulo físico con d/4, qué pasa con el período."),
         ("26-II-1O", "7", "P1O26-7", "Selección: resorte vertical, dónde es máxima la energía potencial."),
         ("26-II-1E", "4", "P1E26-4", "Selección: qué es cierto en un oscilador masa-resorte."),
         ("26-II-1E", "6", "P1E26-6", "Selección: en x = A/2, relación entre K y U."),
         ("Teoría", "7", "T-MAT3", "Integrales 7 de 9: logaritmos y separar variables en la ley de Fourier."),
     )),
    ("D8", 8, "2026-12-04", "Día 8 · vie 4-dic · Simulacro A (2 h 30 min, como I-2026)",
     "De corrido y sin mirar soluciones: 6 selecciones y 3 desarrollos. Después revisá con el tutor.", True, (
         ("26-II-1E", "1", "P1E26-1", "Selección P1: péndulo simple con la cuerda más corta."),
         ("26-II-1O", "6", "P1O26-6", "Selección P1: nodos de una onda estacionaria en el 2.º armónico."),
         ("26-II-2O", "6", "P2O26-6", "Selección P2: esfera con rapidez terminal (Stokes)."),
         ("26-II-2E", "2", "P2E26-2", "Selección P2: tensión superficial con el doble de γ (casilla nueva)."),
         ("26-II-O", "3", "O26-3", "Selección P3: si α se duplica con igual L₀ y ΔT, ¿cuánto cambia ΔL?"),
         ("26-II-O", "4", "O26-4", "Selección P3: gas a volumen constante que duplica su presión."),
         ("26-II-1E", "9", "P1E26-9", "Desarrollo P1: cuerda de instrumento en el banco de pruebas."),
         ("26-II-2O", "9", "P2O26-9", "Desarrollo P2: fuente con dos tuberías (Reynolds y Poiseuille)."),
         ("26-II-O", "9", "O26-9", "Desarrollo P3: el ciclo del III parcial ordinario II-2026."),
     )),
    ("D9", 9, "2026-12-05", "Día 9 · sáb 5-dic · Ondas y cuerdas, y el posible 4.º problema",
     "Las selecciones de ondas del Parcial 1. En II-2025 hubo un cuarto problema: conducción con k que cambia.", False, (
         ("26-II-1O", "2", "P1O26-2", "Selección: el dron y la intensidad del sonido (casilla nueva)."),
         ("26-II-1O", "4", "P1O26-4", "Selección: dos ondas en la misma cuerda."),
         ("26-II-1E", "2", "P1E26-2", "Selección: leer λ y v de y = 0,025 cos(4,0x − 50t)."),
         ("26-II-1E", "3", "P1E26-3", "Selección: potencia media con el doble de f y la mitad de A."),
         ("26-II-1E", "5", "P1E26-5", "Selección: dos pulsos opuestos que se cruzan."),
         ("26-I-R", "1", "RP-26I-1", "Selección: frecuencia fundamental de una cuerda."),
         ("Teoría", "4", "T-INT2", "La integral del logaritmo para conducción con k(y) = a + by."),
         ("25-II-R", "3", "Q050", "Lámina con k(y) = 200 + 1000y: H, energía en 5 min y T a la mitad."),
         ("26-II-2E", "9", "P2E26-9", "Aceite mineral: viscosidad, v máxima, v terminal y fuerza de Stokes."),
     )),
    ("D10", 10, "2026-12-06", "Día 10 · dom 6-dic · Simulacro B y repaso de recetas",
     "Otro simulacro igual de largo. Lo que salga mal, repasalo con la receta de su tipo.", True, (
         ("26-II-1E", "7", "P1E26-7", "Selección P1: dos cuerdas con 4F y 2μ, razón de rapideces."),
         ("26-II-1O", "5", "P1O26-5", "Selección P1: en x = A/2, ¿qué fracción de la energía es cinética?"),
         ("26-II-2O", "1", "P2O26-1", "Selección P2: módulo de Young y límite elástico."),
         ("26-II-2E", "4", "P2E26-4", "Selección P2: alturas en los tubos con un aceite viscoso."),
         ("26-II-O", "1", "O26-1", "Selección P3: C_p del N₂ en grados Fahrenheit (casilla nueva)."),
         ("26-II-O", "5", "O26-5", "Selección P3: pared compuesta en régimen estacionario."),
         ("26-II-1O", "9", "P1O26-9", "Desarrollo P1: cuerda de instrumento que se presiona contra un traste."),
         ("26-I-SUF", "4", "SUF26-4", "Desarrollo P2: tubería que baja 2,50 m y cambia de diámetro."),
         ("24-I-R", "3", "Q107", "Desarrollo P3: ciclo rectangular en términos de p₀ y V₀."),
     )),
    ("D11", 11, "2026-12-07", "Día 11 · lun 7-dic · Repaso final: el cono y el tubo",
     "Mañana es la reposición (martes 8-dic). Dos integrales de conducción y las recetas de la hoja resumen; nada nuevo pesado.", False, (
         ("Teoría", "8", "T-CONO", "Integrales 8 de 9: el tronco de cono desde cero, con disco de radio que crece en línea recta."),
         ("25-II-E", "6", "Q045", "Integrales 9 de 9: tubo de aluminio, cascarón cilíndrico, flujo y temperatura a 6 cm."),
     )),
)

# Calentamientos de 15 s para las lecciones nuevas (los demás usan los de preparacion_examenes)
CALENTAMIENTOS: Dict[str, Dict[str, Any]] = {
    "T-INT0": {
        "pregunta": "¿Qué significa el símbolo ∫ en una fórmula de la hoja?",
        "opciones": ["Sumar muchos pedacitos pequeños", "Multiplicar por dos", "Sacar la raíz"],
        "correcta": 0,
        "explica": "La ∫ es una S alargada: suma de pedacitos. F = ∫p dA suma la fuerza de cada franja.",
    },
    "T-INT1": {
        "pregunta": "Con la regla de la potencia, ¿cuánto da ∫h dh?",
        "opciones": ["h²/2", "2h", "h"],
        "correcta": 0,
        "explica": "h es h¹: se suma 1 al exponente (queda 2) y se divide entre ese 2.",
    },
    "T-PARED": {
        "pregunta": "En una pared, ¿por qué se usan franjas horizontales?",
        "opciones": ["Porque en una franja horizontal toda el agua está a la misma profundidad",
                     "Porque son más fáciles de dibujar", "Porque la presión es igual en toda la pared"],
        "correcta": 0,
        "explica": "La presión depende solo de la profundidad h; en una franja horizontal h es la misma.",
    },
    "T-INT2": {
        "pregunta": "Si u = 200 + 1000y, ¿cuánto vale dy?",
        "opciones": ["dy = du/1000", "dy = du", "dy = 1000 du"],
        "correcta": 0,
        "explica": "du = 1000 dy, entonces dy = du/1000. Olvidar ese 1/1000 es el error típico.",
    },
}


def _resumen_arquetipos() -> List[Dict[str, Any]]:
    """Cuántas reposiciones trajeron cada tipo de problema (en desarrollo o en selección)."""
    con_items = [r for r in REPOSICIONES if r["items"]]
    veces: Counter = Counter()
    lecciones: Dict[str, List[str]] = {}
    for r in con_items:
        vistos = set()
        for num, parcial, tipo, pts, arq, lid, res in r["items"]:
            lecciones.setdefault(arq, []).append(lid)
            if arq not in vistos:
                veces[arq] += 1
                vistos.add(arq)
    out = []
    for arq, (nombre, parcial) in ARQUETIPOS.items():
        if not veces.get(arq):
            continue
        out.append({"clave": arq, "nombre": nombre, "parcial": parcial, "veces": veces[arq],
                    "de": len(con_items), "lecciones": lecciones.get(arq, [])})
    out.sort(key=lambda a: (a["parcial"], -a["veces"]))
    return out


def prediccion() -> Dict[str, Any]:
    puntos = [(r["nombre"], r["puntos"], r["minutos"], r["formato"]) for r in REPOSICIONES]
    return {
        "formato": {
            "titulo": "Lo más probable: como la reposición de I-2026",
            "puntos": 52,
            "minutos": 150,
            "partes": [
                {"nombre": "Parte I · 6 selecciones", "puntos": 18, "detalle": "2 de cada parcial, 3 puntos cada una; solo cuenta la letra."},
                {"nombre": "Parte II · 3 desarrollos", "puntos": 34, "detalle": "Parcial 1 ≈ 9 · Parcial 2 ≈ 9 · ciclo ≈ 16."},
            ],
            "otra_posibilidad": "Como II-2025: 4 desarrollos de 15 puntos; el cuarto fue conducción con k que cambia.",
        },
        "historial": [{"nombre": n, "puntos": p, "minutos": m, "formato": f} for n, p, m, f in puntos],
        "arquetipos": _resumen_arquetipos(),
        "reglas": list(REGLAS),
        "recetas": [dict(r) for r in RECETAS],
        "casillas_nuevas": [dict(c) for c in CASILLAS_NUEVAS],
        "casillas_que_vuelven": [dict(c) for c in CASILLAS_QUE_VUELVEN],
        "fuera": list(FUERA_DE_LA_HOJA),
        "hoja": {
            "estado": "Todavía no la publican (revisado en el TEC Digital el 27-nov-2026).",
            "prediccion": "Las hojas del II-2026 del Parcial 1, el Parcial 2 y el Parcial 3, una tras otra.",
        },
    }


def ids_del_calendario() -> List[str]:
    return [lid for d in CALENDARIO for _, _, lid, _ in d[6]]


def construir_ruta(arma_item: Callable[[str, str, str, str], Dict[str, Any]]) -> Dict[str, Any]:
    """Arma el módulo «Reposición II-2026» para /api/preparacion.

    `arma_item` es `preparacion_examenes._arma_item` (se recibe como parámetro para no crear una
    importación circular). Las lecciones que todavía no existen van a `pendientes` del día.
    """
    bloques: List[Dict[str, Any]] = []
    pendientes_tot: List[str] = []
    examenes: List[str] = []
    for bid, dia, fecha, titulo, sub, simulacro, items in CALENDARIO:
        lista: List[Dict[str, Any]] = []
        pend: List[Dict[str, str]] = []
        for examen, num, lid, meta in items:
            if not registro.existe(lid):
                pend.append({"id": lid, "examen": examen, "num": num, "meta_1_linea": meta})
                pendientes_tot.append(lid)
                continue
            it = arma_item(examen, num, lid, meta)
            if lid in CALENTAMIENTOS:
                it["calentamiento"] = CALENTAMIENTOS[lid]
            lista.append(it)
            if examen not in examenes:
                examenes.append(examen)
        bloques.append({
            "id": bid,
            "dia": dia,
            "fecha": fecha,
            "titulo": titulo,
            "subtitulo": sub,
            "simulacro": simulacro,
            "minutos": 150 if simulacro else sum(x["minutos"] for x in lista),
            "items": lista,
            "pendientes": pend,
        })
    total = sum(len(b["items"]) for b in bloques)
    return {
        "titulo": "Reposición II-2026 · plan hasta el examen (martes 8-dic)",
        "descripcion": (
            "Todo el curso hasta la sección 20.5, ordenado por lo que más vale en la reposición: "
            "el ciclo, fluidos, integrales desde cero (una por día), MAS y dos simulacros. "
            "El examen es el martes 8 de diciembre de 2026."
        ),
        "examenes": examenes,
        "total": total,
        "pendientes": pendientes_tot,
        "prediccion": prediccion(),
        "bloques": bloques,
        "mixtos_recortados": [{"id": "Q108", "nota": "Inciso D omitido (entropía ΔS · sección 20.7)"}],
    }
