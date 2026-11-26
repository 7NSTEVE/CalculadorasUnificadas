"""Preparación exhaustiva en pasos cortos para III Parcial Extraordinario (≤ 20.5) y Reposición (3 Parciales).

Organiza en micro-bloques de 3 a 5 ejercicios (10–15 min cada uno) con calentamiento de 15 s
y recorte quirúrgico `?corte=20.5` que ignora automáticamente incisos de Carnot (20.6) o
Entropía (20.7) manteniendo íntegros todos los incisos ≤ 20.5.
"""

from __future__ import annotations

import copy
import re
from typing import Any, Dict, List, Sequence, Tuple

from . import registro
from .formularios_p1_p2 import mapear_formulario_por_parcial

# ---------------------------------------------------------------------- recorte quirúrgico ≤ 20.5

_RE_FUERA_20_5 = re.compile(r"entrop[íi]a|\\Delta\s*S|dS\s*=|Carnot", re.I)

# (id -> (etiqueta_omitida, patron_enunciado_trim, reemplazo_titulo))
_MIXTOS_20_5: Dict[str, Tuple[str, re.Pattern[str], Tuple[str, str]]] = {
    "Q092": (
        "Inciso D omitido (entropía ΔS · sección 20.7)",
        re.compile(r"\s*D\)\s*[^.]*\.(?:\s*$)?", re.I),
        ("", ""),
    ),
    "Q097": (
        "Inciso E omitido (entropía ΔS · sección 20.7)",
        re.compile(r"\s*E\)\s*[^.]*\.(?:\s*$)?", re.I),
        (" y ΔS", ""),
    ),
    "Q128": (
        "Inciso D omitido (eficiencia de Carnot · sección 20.6)",
        re.compile(r"\s*D\)\s*[^.]*\.(?:\s*$)?", re.I),
        (" y Carnot", ""),
    ),
    "Q127": (
        "Paso auxiliar de entropía omitido (sección 20.7); resuelto solo con Kelvin-Planck (F10C1)",
        re.compile(r"$^"),
        (" y entropía", ""),
    ),
    "Q108": (
        "Inciso D omitido (entropía ΔS · sección 20.7)",
        re.compile(r"\s*D\)\s*[^.]*\.(?:\s*$)?", re.I),
        (", e y ΔS", " y e"),
    ),
}


def filtrar_leccion_corte_20_5(lec_json: Dict[str, Any]) -> Dict[str, Any]:
    """Devuelve una copia limpia de `lec_json` eliminando pasos/incisos de Carnot (20.6) o Entropía (20.7)."""
    out = copy.deepcopy(lec_json)
    lid = str(out.get("id") or "")
    if lid not in _MIXTOS_20_5:
        out["corte_aplicado"] = "20.5"
        return out

    nota_corte, pat_enun, (t_old, t_new) = _MIXTOS_20_5[lid]
    if t_old:
        out["titulo"] = str(out.get("titulo") or "").replace(t_old, t_new)

    pasos_filtrados: List[Dict[str, Any]] = []
    for p in out.get("pasos") or []:
        tit = str(p.get("titulo") or "")
        if _RE_FUERA_20_5.search(tit):
            continue
        pasos_filtrados.append(p)
    for idx, p in enumerate(pasos_filtrados, 1):
        p["n"] = idx
    out["pasos"] = pasos_filtrados

    # Filtrar respuestas fuera de corte
    resp_ok: List[Dict[str, Any]] = []
    for r in out.get("respuestas") or []:
        txt = f"{r.get('inciso', '')} {r.get('tex', '')}"
        if _RE_FUERA_20_5.search(txt) or (lid in ("Q092", "Q128", "Q108") and str(r.get("inciso", "")).upper().startswith(("D", "DS"))):
            continue
        resp_ok.append(r)
    out["respuestas"] = resp_ok

    # Filtrar fórmulas, claves y nota en enunciado para que no aparezcan alertas falsas de fuera_de_hoja
    out["formulas"] = [f for f in (out.get("formulas") or []) if not _RE_FUERA_20_5.search(str(f))]
    out["claves"] = [c for c in (out.get("claves") or []) if not _RE_FUERA_20_5.search(str(c))]

    enun = str(out.get("enunciado") or "")
    if pat_enun.pattern != "$^" and pat_enun.search(enun):
        out["enunciado"] = pat_enun.sub(" [Inciso > 20.5 omitido en esta preparación.]", enun).strip()

    aviso_prev = str(out.get("aviso") or "").strip()
    prefijo = f"Corte ≤ 20.5 activo: {nota_corte}. Los demás incisos se desarrollan completos desde el formulario II-2026."
    if aviso_prev and not _RE_FUERA_20_5.search(aviso_prev) and "20.5" not in aviso_prev and "20.6" not in aviso_prev:
        out["aviso"] = f"{prefijo} {aviso_prev}".strip()
    else:
        out["aviso"] = prefijo
    out["corte_aplicado"] = "20.5"
    out["inciso_omitido"] = nota_corte
    return out


# ---------------------------------------------------------------------- calentamientos de 15 s

# Plantillas por tema/tipo para preguntas de 15 s con 3 opciones claras de fórmula base
_CALENTAMIENTOS_TEMA: Dict[str, Dict[str, Any]] = {
    "dilatacion": {
        "pregunta": "¿Con qué casilla de la hoja arrancas un problema de dilatación térmica?",
        "opciones": ["ΔL = α L₀ ΔT (o ΔV = β V₀ ΔT)", "H = kA(T_H − T_C)/L", "Q = mcΔT"],
        "correcta": 0,
        "explica": "Si cambia longitud, área o volumen con ΔT, partes de F1C1 o F1C2.",
    },
    "calorimetria": {
        "pregunta": "¿Qué casillas gobiernan mezclas térmicas o cambios de fase?",
        "opciones": ["Q = mcΔT y Q = ±mL", "W = p(V₂ − V₁)", "pV = nRT"],
        "correcta": 0,
        "explica": "Calor sensible (F3C2) cuando cambia T y calor latente (F3C3) cuando cambia fase.",
    },
    "conduccion": {
        "pregunta": "¿Desde qué ecuación base arranca toda conducción de calor?",
        "opciones": ["H = kA(T_H − T_C)/L o H = −kA dT/dx", "H = AeσT⁴", "ΔU = Q − W"],
        "correcta": 0,
        "explica": "Pared plana uniforme usa F2C1; área o conductividad variable integra F2C2.",
    },
    "radiacion": {
        "pregunta": "¿Qué casilla aplica a radiación térmica y qué unidad exige T?",
        "opciones": ["H = AeσT⁴ (o neta) con T en Kelvin", "H = kAΔT/L con T en °C", "Q = mcΔT con T en °C"],
        "correcta": 0,
        "explica": "Stefan-Boltzmann (F2C3 / F3C1) siempre exige temperatura absoluta en Kelvin.",
    },
    "gases": {
        "pregunta": "¿Qué base usas para gas ideal y energía cinética molecular?",
        "opciones": ["pV = nRT y ½m(v²)_med = 3k_B T/2", "ΔL = α L₀ ΔT", "e = W/Q_H"],
        "correcta": 0,
        "explica": "Estados de gas parten de F4C1 y rapidez/energía molecular de F5C1–F5C2.",
    },
    "primera_ley": {
        "pregunta": "¿Qué par de casillas resuelve energía interna y balance de un proceso?",
        "opciones": ["ΔU = Q − W y ΔU = nC_V ΔT", "H = −kA dT/dx", "Q = ±mL"],
        "correcta": 0,
        "explica": "La primera ley (F7C1) y ΔU = nC_V ΔT (F7C2) valen para cualquier proceso de gas ideal.",
    },
    "ciclos": {
        "pregunta": "En un ciclo p-V, ¿cómo hallas trabajo y eficiencia sin saltarte pasos?",
        "opciones": ["W = ∫ p dV por tramo y e = W/Q_H (o K = |Q_c|/|W|)", "Usar W = pΔV en todos los tramos", "Usar e = 1 − T_C/T_H directo"],
        "correcta": 0,
        "explica": "Cada proceso usa su casilla de trabajo (F6C1–F6C3, F9C1) y al cerrar el ciclo aplica F10C1/F10C2.",
    },
    "maquinas": {
        "pregunta": "¿Qué casilla oficial rige máquinas térmicas y refrigeradores hasta 20.5?",
        "opciones": ["e = W/Q_H = 1 − |Q_c/Q_H| y K = |Q_c|/|W|", "e = 1 − T_C/T_H", "pV^γ = constante"],
        "correcta": 0,
        "explica": "Hasta la sección 20.5 siempre partes de F10C1 (motor) o F10C2 (refrigerador).",
    },
    "ondas": {
        "pregunta": "¿De qué hoja oficial salen las fórmulas de oscilaciones y ondas?",
        "opciones": ["Hoja Parcial 1: MAS (ω²=k/m, x=A cos) y cuerdas (v=√(F/μ), λ_n=2L/n)", "Hoja Parcial 2: Bernoulli y Poiseuille", "Hoja Parcial 3: pV = nRT"],
        "correcta": 0,
        "explica": "Todo problema de reposición de Parcial 1 parte directo de la hoja oficial del I Parcial.",
    },
    "fluidos": {
        "pregunta": "¿Qué ecuación de la Hoja Parcial 2 conecta presión, altura y rapidez de un fluido ideal?",
        "opciones": ["Bernoulli (p₁+ρgy₁+½ρv₁² = cte) y continuidad (A₁v₁ = A₂v₂)", "Ley de Hooke F_x = −kx", "Primera ley ΔU = Q − W"],
        "correcta": 0,
        "explica": "Fluidos ideales usan continuidad/Bernoulli; viscosos usan Poiseuille/Reynolds y paredes usan ∫p dA.",
    },
}

# Calentamientos específicos más precisos para desarrollos clave
_CALENTAMIENTO_POR_ID: Dict[str, Dict[str, Any]] = {
    "Q041": {
        "pregunta": "En un cono truncado el área A(x) cambia con x. ¿Cuál casilla eliges?",
        "opciones": ["H = −kA dT/dx (separar e integrar)", "H = kA(T_H−T_C)/L con área promedio", "H = AeσT⁴"],
        "correcta": 0,
        "explica": "Área variable obliga a partir de Fourier diferencial (F2C2) e integrar.",
    },
    "Q043": {
        "pregunta": "Dos barras en paralelo conectan los mismos focos. ¿Cómo se combinan H_A y H_B?",
        "opciones": ["H_total = H_A + H_B (cónica con F2C2, cilíndrica con F2C1)", "H_A = H_B (flujo único en serie)", "Se promedian las conductividades"],
        "correcta": 0,
        "explica": "En paralelo los flujos se suman; cada barra aporta según su geometría.",
    },
    "Q045": {
        "pregunta": "En una tubería cilíndrica con flujo radial, ¿qué área entra en H = −kA dT/dr?",
        "opciones": ["Área lateral del cilindro A = 2πrL", "Volumen del cilindro πr²L", "Área transversal πr²"],
        "correcta": 0,
        "explica": "El calor cruza la pared lateral 2πrL (¡el solucionario oficial cometió el error de usar πr²L!).",
    },
    "Q050": {
        "pregunta": "Aquí la conductividad varía k(y) = 200 + 1000y. Al integrar con u = 200 + 1000y, ¿qué sale de dy?",
        "opciones": ["dy = du / 1000 (¡no olvidar el 1/1000!)", "dy = du directamente", "k se evalúa en y = 0 y sale de la integral"],
        "correcta": 0,
        "explica": "du = 1000 dy → dy = du/1000; olvidarlo produce un error de factor 1000.",
    },
    "Q097": {
        "pregunta": "En el ciclo Diésel, ¿qué casilla conecta dos estados de una etapa adiabática?",
        "opciones": ["pV^γ = cte y TV^(γ−1) = cte", "W = nRT ln(V₂/V₁)", "p₁/T₁ = p₂/T₂"],
        "correcta": 0,
        "explica": "Las etapas adiabáticas B→A y C→D salen de F9C2 y F9C3.",
    },
    "Q128": {
        "pregunta": "En este ciclo isotérmico-isobárico-isocórico (corte ≤ 20.5), ¿qué inciso se ignora?",
        "opciones": ["Inciso D (Carnot · sección 20.6); resolvemos A, B y C", "Inciso A (trabajo isotérmico)", "Inciso B (calor absorbido)"],
        "correcta": 0,
        "explica": "El corte llega a 20.5, así que Carnot se omite automáticamente y practicas A, B y C al 100 %.",
    },
    "RP-23II-1": {
        "pregunta": "Dada y(x,t) = 8,50 mm sen(10πx) sen(240πt), ¿cómo sacas v de las ondas viajeras?",
        "opciones": ["v = ω/k = 240π / 10π", "v = A_OE · ω", "v = √(k/m)"],
        "correcta": 0,
        "explica": "De P1-F9C1: la rapidez de propagación es ω/k; en cambio A_OE·ω es la rapidez transversal máxima.",
    },
    "RP-23II-2": {
        "pregunta": "El aceite tiene viscosidad η = 0,3 Pa·s y sale por un tubo angosto. ¿Qué ley aplica?",
        "opciones": ["Poiseuille: dV/dt = (p₁−p₂)πR⁴/(8ηL)", "Bernoulli sin viscosidad", "Ley de Stokes F_S = 6πrηv"],
        "correcta": 0,
        "explica": "Fluido viscoso en tubo cilíndrico usa Poiseuille (P2-F8C2) y se valida con Reynolds (P2-F8C1).",
    },
    "RP-24I-2": {
        "pregunta": "Para una compuerta con borde parabólico y = 2x², ¿cómo planteas el diferencial dA?",
        "opciones": ["Franja horizontal dA = x(y) dy = √(y/2) dy en F = ∫ p dA", "Multiplicar presión máxima por área total", "Usar Bernoulli entre fondo y superficie"],
        "correcta": 0,
        "explica": "La presión solo depende de la profundidad (H − y), por eso cada franja horizontal tiene presión uniforme.",
    },
    "RP-25II-2": {
        "pregunta": "Una pared trapezoidal cambia de ancho con la profundidad h. ¿Qué integral usas?",
        "opciones": ["F_neta = ∫ ρgh L(h) dh desde F_⊥ = ∫ p dA", "F = (p₀ + ρgH)·A_total", "F = ρgV"],
        "correcta": 0,
        "explica": "p₀ se cancela con el aire exterior y se integra ρgh·L(h) dh de 0 a 50 m.",
    },
    "RP-26I-7": {
        "pregunta": "Si x(0) > 0 y v(0) > 0 en x = A cos(ωt + φ), ¿qué signo debe tener φ?",
        "opciones": ["φ negativo (o 4.º cuadrante), porque v = −ωA sen(φ) > 0", "φ positivo en el 1.er cuadrante", "φ = 0 siempre"],
        "correcta": 0,
        "explica": "El signo menos en v = −ωA sen(ωt+φ) exige sen(φ) < 0 cuando v(0) > 0.",
    },
}

# ---------------------------------------------------------------------- definición de micro-bloques

# (bloque_id, titulo, subtítulo_corto, items: [(examen, num, id, meta_1_linea)])
_BLOQUES_EXTRA: Tuple[Tuple[str, str, str, Tuple[Tuple[str, str, str, str], ...]], ...] = (
    ("E-B1", "Bloque 1 · Dilatación y cambios de fase", "5 preguntas rápidas de selección para entrar en ritmo", (
        ("23-II-E", "1", "Q002", "Identificar cómo cambia el volumen total al fundir dos cilindros."),
        ("25-II-E", "1", "Q003", "Comparar qué tramo se dilata más en una barra compuesta."),
        ("26-I-E", "1", "Q005", "Relacionar dos barras con igual ΔL pero distinto ΔT."),
        ("23-II-E", "2", "Q021", "Distinguir fusión, condensación y signo del calor latente."),
        ("25-II-E", "2", "Q025", "Explicar por qué T se mantiene constante durante un cambio de fase."),
    )),
    ("E-B2", "Bloque 2 · Calorimetría, conducción y radiación", "5 selecciones de transferencia de calor y mezclas", (
        ("26-I-E", "2", "Q027", "Decidir si sirve el promedio simple al mezclar hielo y agua."),
        ("26-I-E", "6", "Q028", "Estimar y calcular la T final al mezclar masas distintas de agua."),
        ("24-II-E", "1", "Q040", "Ordenar 4 placas en serie por conductividad según sus ΔT."),
        ("25-I-E", "4", "Q062", "Explicar por qué sube el agua caliente usando ΔV y densidad."),
        ("23-II-E", "3", "Q063", "Ver cuánto crece la potencia radiada al duplicar T en Kelvin."),
    )),
    ("E-B3", "Bloque 3 · Radiación y teoría cinética", "5 selecciones sobre Stefan-Boltzmann y gases ideales", (
        ("25-I-E", "5", "Q066", "Comparar radiación de un cuerpo negro frente a cobre pulido."),
        ("26-I-E", "7", "Q068", "Calcular radiación neta pasando 97 °C y 27 °C a Kelvin."),
        ("23-II-E", "4", "Q075", "Reconocer los supuestos reales de la teoría cinética molecular."),
        ("25-I-E", "3", "Q077", "Explicar por qué el CO₂ tiene mayor C_V que el helio."),
        ("25-II-E", "3", "Q078", "Identificar de qué depende la presión microscópica de un gas."),
    )),
    ("E-B4", "Bloque 4 · Primera ley y procesos p-V", "5 selecciones clave de ΔU, Q y W", (
        ("24-I-E", "4", "Q079", "Aplicar el teorema de equipartición a cada grado de libertad."),
        ("24-II-E", "5", "Q083", "Ubicar qué trayectoria entre isotermas tiene mayor ΔU."),
        ("25-II-E", "4", "Q085", "Distinguir funciones de estado (ΔU) de cantidades de trayectoria (Q, W)."),
        ("24-I-E", "1", "Q087", "Aplicar ΔU = Q − W cuidando signos de calor y trabajo."),
        ("24-II-E", "2", "Q090", "Comprobar por qué ΔU = nC_V ΔT incluso en un proceso isobárico."),
    )),
    ("E-B5", "Bloque 5 · Adiabáticas, isotermas y ciclos", "5 selecciones de curvas p-V y máquinas térmicas", (
        ("24-I-E", "5", "Q095", "Distinguir la curva adiabática de la isoterma por su pendiente."),
        ("26-I-E", "3", "Q101", "Determinar signos de W y ΔU en una compresión adiabática."),
        ("26-I-E", "5", "Q102", "Despejar volumen inicial en una expansión isotérmica."),
        ("24-I-E", "3", "Q088", "Filtrar ciclos posibles e imposibles con 1.ª y 2.ª ley."),
        ("23-II-E", "5", "Q117", "Identificar por qué el desempeño K de un refrigerador puede superar 1."),
    )),
    ("E-B6", "Bloque 6 · Segunda Ley ≤ 20.5 y refrigeradores", "4 selecciones de cierre conceptual (sin Carnot ni ΔS)", (
        ("24-II-E", "4", "Q121", "Verificar propiedades fundamentales de un ciclo de máquina térmica."),
        ("25-I-E", "2", "Q123", "Interpretar físicamente el valor numérico del coeficiente K."),
        ("25-I-E", "1", "Q127", "Explicar por qué Kelvin-Planck prohíbe 100 % de eficiencia."),
        ("26-I-E", "4", "Q129", "Reconocer el enunciado correcto de la segunda ley (Kelvin-Planck)."),
    )),
    ("E-B7", "Bloque 7 · Desarrollos I: dilatación, calorimetría y serie", "4 desarrollos completos paso a paso desde el formulario", (
        ("26-I-E", "9", "Q006", "Ajustar temperatura para encajar una barra en un anillo de hierro."),
        ("23-II-E", "7", "Q022", "Balance completo de hielo en calorímetro de hierro con vapor."),
        ("24-I-E", "6", "Q038", "Comparar conducción en barras de cobre y plata en serie vs paralelo."),
        ("26-I-E", "8", "Q047", "Hallar T de frontera, k del policarbonato y gradientes en ventana."),
    )),
    ("E-B8", "Bloque 8 · Desarrollos II: conducción con integrales", "3 desarrollos clásicos de Fourier diferencial (F2C2)", (
        ("24-II-E", "6", "Q041", "Integrar H = −kA(x) dT/dx en un cono truncado sólido."),
        ("25-I-E", "6", "Q043", "Sumar flujos en paralelo: barra cónica (integral) + cilíndrica."),
        ("25-II-E", "6", "Q045", "Integrar flujo radial en tubería cilíndrica con A = 2πrL."),
    )),
    ("E-B9", "Bloque 9 · Desarrollos III: ciclos térmicos", "3 desarrollos de ciclos p-V (corte ≤ 20.5 aplicado)", (
        ("23-II-E", "6", "Q092", "Calentamiento isocórico + expansión isobárica (A, B y C; ignora D)."),
        ("24-I-E", "8", "Q096", "Ciclo de 4 etapas con calores dados y despeje de γ con F9C1."),
        ("25-II-E", "7", "Q099", "Ciclo triangular de 0,15 mol monoatómico con trapecios y e."),
    )),
    ("E-B10", "Bloque 10 · Desarrollos IV: Diésel, ciclos mixtos y refrigerador", "3 desarrollos de cierre (Carnot y ΔS recortados)", (
        ("24-II-E", "7", "Q097", "Ciclo Diésel completo: moles, estados, Q_H, Q_C y e (ignora E)."),
        ("25-I-E", "7", "Q128", "Ciclo isotérmico-isobárico-isocórico (A, B y C; ignora Carnot D)."),
        ("26-I-E", "10", "Q103", "Ciclo refrigerador con 0,300 mol de N₂ y coeficiente K."),
    )),
)

_EXCLUIDOS_EXTRA: Tuple[Dict[str, str], ...] = (
    {
        "id": "Q132",
        "examen": "24-I-E",
        "num": "2",
        "tipo": "seleccion",
        "seccion": "20.7 · Entropía",
        "titulo": "Cambio de entropía en una expansión adiabática reversible",
        "motivo": "Evalúa únicamente entropía (ΔS), fuera del corte ≤ 20.5.",
    },
    {
        "id": "Q023",
        "examen": "24-I-E",
        "num": "7",
        "tipo": "desarrollo",
        "seccion": "20.7 · Entropía",
        "titulo": "Cambio de entropía al derretir hielo y calentar agua",
        "motivo": "Desarrollo íntegro de entropía (ΔS = ∫ dQ/T), fuera del corte ≤ 20.5.",
    },
    {
        "id": "Q130",
        "examen": "24-II-E",
        "num": "3",
        "tipo": "seleccion",
        "seccion": "20.7 · Entropía",
        "titulo": "Cambio de entropía neta del universo en ciclo reversible",
        "motivo": "Pregunta exclusiva de entropía (sección 20.7), fuera del corte ≤ 20.5.",
    },
    {
        "id": "Q125",
        "examen": "25-II-E",
        "num": "5",
        "tipo": "seleccion",
        "seccion": "20.6 · Ciclo de Carnot",
        "titulo": "Teorema de Carnot entre focos de 700 K y 350 K",
        "motivo": "Evalúa directamente eficiencia de Carnot (sección 20.6), fuera del corte ≤ 20.5.",
    },
)

# Reposición: 3 Pilares · 6 micro-bloques · 22 problemas totales
_BLOQUES_REPOS: Tuple[Tuple[str, int, str, str, Tuple[Tuple[str, str, str, str], ...]], ...] = (
    ("R-P1A", 1, "Pilar 1 · Bloque A: Cuerdas vibrantes y péndulo", "Parcial 1 · 2 selecciones + 2 desarrollos de ondas", (
        ("26-I-R", "1", "RP-26I-1", "Calcular frecuencia fundamental de cuerda de contrabajo desde μ y F."),
        ("26-I-R", "2", "RP-26I-2", "Ver cómo escala el período de un péndulo simple al cuadruplicar L."),
        ("23-II-R", "1", "RP-23II-1", "Descomponer onda estacionaria en antinodos, viajeras y v_y máx."),
        ("24-I-R", "1", "RP-24I-1", "Diseñar instrumento de 2 cuerdas bajo igual tensión y posición de traste."),
    )),
    ("R-P1B", 1, "Pilar 1 · Bloque B: Oscilador masa-resorte y gráficas MAS", "Parcial 1 · 3 desarrollos completos de MAS", (
        ("24-II-R", "1", "RP-24II-1", "Hallar T, k, fase φ = π y desplazamiento Δx en masa-resorte."),
        ("25-II-R", "1", "RP-25II-1", "Extraer T, ω, A, φ y ecuaciones x, v, a desde gráfica v(t)."),
        ("26-I-R", "7", "RP-26I-7", "Determinar T, fase correcta con signo de v(0) > 0 y energía E."),
    )),
    ("R-P2A", 2, "Pilar 2 · Bloque A: Flotación, caudal y Poiseuille", "Parcial 2 · 2 selecciones + 2 desarrollos de fluidos", (
        ("26-I-R", "3", "RP-26I-3", "Comparar densidades de dos fluidos según porcentaje sumergido."),
        ("26-I-R", "4", "RP-26I-4", "Hallar rapidez de salida combinando flujo másico y caudal."),
        ("23-II-R", "2", "RP-23II-2", "Calcular Δp, caudal de Poiseuille, velocidad y Reynolds en aceite."),
        ("26-I-R", "8", "RP-26I-8", "Tanque en estado estacionario: caudal, rapidez y profundidad."),
    )),
    ("R-P2B", 2, "Pilar 2 · Bloque B: Venturi e integrales hidrostáticas", "Parcial 2 · 3 desarrollos clásicos con Bernoulli e integrales", (
        ("24-II-R", "2", "RP-24II-2", "Tubo Venturi con manómetros verticales: v_a, v_b, Δp y Δh."),
        ("24-I-R", "2", "RP-24I-2", "Integrar fuerza hidrostática en compuerta rectangular + parabólica."),
        ("25-II-R", "2", "RP-25II-2", "Represa trapezoidal con integral ∫ρgh L(h)dh + tubería con Bernoulli."),
    )),
    ("R-P3A", 3, "Pilar 3 · Bloque A: Calor, conducción y ciclo rectangular", "Parcial 3 · 2 selecciones + 2 desarrollos térmicos", (
        ("26-I-R", "5", "Q051", "Aplicar conducción estacionaria a una pared plana de ladrillo."),
        ("26-I-R", "6", "Q020", "Despejar temperatura final de aluminio con Q = mcΔT."),
        ("25-II-R", "3", "Q050", "Integrar conductividad variable k(y) = 200 + 1000y (alerta errata)."),
        ("23-II-R", "3", "Q106", "Ciclo rectangular de 2 mol diatómico: W_neto, Q_H, Q_C y e."),
    )),
    ("R-P3B", 3, "Pilar 3 · Bloque B: Ciclos simbólicos, triángulos y refrigerador", "Parcial 3 · 4 desarrollos de ciclos ≤ 20.5", (
        ("24-I-R", "3", "Q107", "Ciclo rectangular puramente simbólico en términos de p₀ y V₀."),
        ("24-II-R", "3", "Q108", "Ciclo triangular monoatómico (A, B y C; ignora entropía D)."),
        ("25-II-R", "4", "Q109", "Ciclo isocórico-adiabático-isobárico de 0,378 mol diatómico."),
        ("26-I-R", "9", "Q110", "Refrigerador de 2,50 mol de helio: tabla pVT, W, Q y K."),
    )),
)


def _arma_item(examen: str, num: str, lid: str, meta: str) -> Dict[str, Any]:
    lec = registro.obtener(lid)
    j = lec.a_json()
    parcial = int(j.get("parcial") or 3)
    if lid in _MIXTOS_20_5:
        j = filtrar_leccion_corte_20_5(j)
    mapa = mapear_formulario_por_parcial(j)
    celdas_clave = [
        {"id": c["id"], "nombre": c["nombre"], "etiqueta": c["etiqueta"], "color": c["color"]}
        for c in (mapa.get("celdas") or [])
        if not c.get("es_constante")
    ][:3]
    tema = registro.grupo(lec.tema)
    cal = _CALENTAMIENTO_POR_ID.get(lid) or _CALENTAMIENTOS_TEMA.get(tema) or _CALENTAMIENTOS_TEMA["ciclos"]
    return {
        "id": lid,
        "examen": examen,
        "num": num,
        "parcial": parcial,
        "tipo": lec.tipo,
        "tema": tema,
        "titulo": j.get("titulo") or lec.titulo,
        "minutos": registro.minutos(lec),
        "pasos": len(j.get("pasos") or []),
        "meta_1_linea": meta,
        "inciso_omitido": _MIXTOS_20_5[lid][0] if lid in _MIXTOS_20_5 else "",
        "aviso": bool(lec.aviso),
        "celdas_clave": celdas_clave,
        "calentamiento": cal,
    }


_CACHE_PREP: Dict[str, Any] = {}


def preparacion_completa() -> Dict[str, Any]:
    """Construye y cachea los dos módulos completos (Extraordinario 42 + Reposición 22)."""
    if _CACHE_PREP:
        return _CACHE_PREP

    bloques_extra: List[Dict[str, Any]] = []
    for bid, tit, sub, items in _BLOQUES_EXTRA:
        lista = [_arma_item(ex, num, lid, meta) for ex, num, lid, meta in items]
        bloques_extra.append({
            "id": bid,
            "titulo": tit,
            "subtitulo": sub,
            "minutos": sum(x["minutos"] for x in lista),
            "items": lista,
        })

    bloques_repos: List[Dict[str, Any]] = []
    for bid, parc, tit, sub, items in _BLOQUES_REPOS:
        lista = [_arma_item(ex, num, lid, meta) for ex, num, lid, meta in items]
        bloques_repos.append({
            "id": bid,
            "parcial": parc,
            "titulo": tit,
            "subtitulo": sub,
            "minutos": sum(x["minutos"] for x in lista),
            "items": lista,
        })

    total_extra = sum(len(b["items"]) for b in bloques_extra)
    total_repos = sum(len(b["items"]) for b in bloques_repos)

    _CACHE_PREP.update({
        "corte": "20.5",
        "extraordinario": {
            "titulo": "III Parcial Extraordinario · Corte ≤ 20.5",
            "descripcion": (
                "Los 42 problemas reales de los 6 exámenes extraordinarios (II-2023 a I-2026) "
                "resueltos paso a paso desde el formulario oficial II-2026. "
                "Carnot (20.6) y Entropía (20.7) quedan filtrados automáticamente."
            ),
            "examenes": ["23-II-E", "24-I-E", "24-II-E", "25-I-E", "25-II-E", "26-I-E"],
            "total_historico": total_extra + len(_EXCLUIDOS_EXTRA),
            "total_activos": total_extra,
            "total_excluidos": len(_EXCLUIDOS_EXTRA),
            "bloques": bloques_extra,
            "excluidos_corte": list(_EXCLUIDOS_EXTRA),
            "mixtos_recortados": [
                {"id": lid, "nota": info[0]}
                for lid, info in _MIXTOS_20_5.items()
                if lid != "Q108"
            ],
        },
        "reposicion": {
            "titulo": "Exámenes de Reposición · Curso Completo (3 Parciales)",
            "descripcion": (
                "Los 22 problemas reales de los 5 exámenes de reposición (II-2023 a I-2026), "
                "enlazados a las 3 hojas oficiales de fórmulas (Parcial 1, Parcial 2 y Parcial 3 ≤ 20.5)."
            ),
            "examenes": ["23-II-R", "24-I-R", "24-II-R", "25-II-R", "26-I-R"],
            "total": total_repos,
            "pilares": [
                {"parcial": 1, "nombre": "Parcial 1 · Oscilaciones y ondas", "hoja_url": "/api/formulario/p1/hoja.png", "total": 7},
                {"parcial": 2, "nombre": "Parcial 2 · Fluidos e hidrodinámica", "hoja_url": "/api/formulario/p2/hoja.png", "total": 7},
                {"parcial": 3, "nombre": "Parcial 3 · Termodinámica ≤ 20.5", "hoja_url": "/api/formulario/hoja.png", "total": 8},
            ],
            "bloques": bloques_repos,
            "mixtos_recortados": [{"id": "Q108", "nota": _MIXTOS_20_5["Q108"][0]}],
        },
    })
    return _CACHE_PREP
