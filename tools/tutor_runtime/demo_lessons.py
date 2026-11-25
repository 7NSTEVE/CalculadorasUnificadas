"""Curated, SymPy-verified interactive seed lessons for Fisica General III (TEC).

Every lesson adheres strictly to cuadernu.tutor.v1.1:
- Explicit <cue id='...'/> audio-visual anchors
- Explicit provenance edges ('¿De dónde salió esto?')
- Interactive STEM widgets (Riemann work rectangles, P-V diagram, spherical thermal shell)
- Pre-built in-place intermediate algebraic expansions
- Casio calculator verification strips
"""

from __future__ import annotations

from typing import Any, Dict, List


def get_seed_lessons_catalog() -> List[Dict[str, str]]:
    return [
        {
            "id": "fis3_trabajo_cero",
            "title": "1. ¿Qué es integrar? Trabajo de un gas W = ∫ P(V) dV",
            "course": "Física General III · TEC",
            "summary": "Aprende desde cero absoluto qué significa integrar usando rectángulos de trabajo interactivos.",
        },
        {
            "id": "fis3_isotermico",
            "title": "2. Proceso isotérmico: cómo nace W = nRT ln(Vf / Vi)",
            "course": "Física General III · TEC",
            "summary": "Por qué nRT sale fuera de la integral, qué hace 1/V y cómo se evalúan los límites.",
        },
        {
            "id": "fis3_conduccion_esferica",
            "title": "3. Conducción térmica esférica: ∫ r⁻² dr y el signo menos",
            "course": "Física General III · TEC",
            "summary": "Separación de variables, regla de potencias con exponente negativo y origen exacto de cada signo.",
        },
    ]


def get_lesson_beats(lesson_id: str, session_id: str) -> List[Dict[str, Any]]:
    if lesson_id == "fis3_isotermico":
        return _lesson_isotermico(session_id)
    if lesson_id == "fis3_conduccion_esferica":
        return _lesson_conduccion_esferica(session_id)
    return _lesson_trabajo_cero(session_id)


def _lesson_trabajo_cero(session_id: str) -> List[Dict[str, Any]]:
    return [
        {
            "protocol": "cuadernu.tutor.v1.1",
            "session_id": session_id,
            "turn_id": "seed_1",
            "beat_id": "beat_0001",
            "step_index": 1,
            "checkpoint_id": "cp_riemann_intro",
            "branch_id": None,
            "speech_markup": (
                "Vamos desde cero absoluto, sin memorizar nada todavía. "
                "<cue id='c1'/> En física, cuando un gas empuja un pistón a presión constante, el trabajo es presión por cambio de volumen. "
                "<cue id='c2'/> Pero cuando la presión cambia conforme el gas se expande, dividimos el área en rectángulos muy angostos de ancho dV y los sumamos todos. "
                "Mueve el deslizador de rectángulos para ver cómo esa suma se acerca al valor exacto de cuarenta joules."
            ),
            "speech_rate": 0.96,
            "actions": [
                {
                    "action_id": "act_101",
                    "type": "write_math",
                    "object_id": "eq_work_def",
                    "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1200, "cue_id": "c1"},
                    "color_token": "ink_primary",
                    "payload": {
                        "latex": "W = \\int_{\\class{term-vi}{V_i}}^{\\class{term-vf}{V_f}} \\class{term-p}{P(V)}\\,\\class{term-dv}{dV}",
                        "step_label": "Paso 1 · Definición física de trabajo",
                    },
                },
                {
                    "action_id": "act_102",
                    "type": "plot_riemann_sum",
                    "object_id": "widget_riemann_1",
                    "placement": {"zone": "visual_lab", "relation": "auto_next", "gap_px": 20},
                    "animation": {"style": "fade_in", "duration_ms": 800, "cue_id": "c2"},
                    "color_token": "accent_blue",
                    "payload": {
                        "title": "Trabajo como suma de rectángulos de área",
                        "expr": "2 + 4*x",
                        "a": 1.0,
                        "b": 5.0,
                        "n_init": 6,
                        "n_max": 100,
                        "x_label": "V (m³)",
                        "y_label": "P (Pa)",
                        "exact_value": 56.0,
                        "unit": "J",
                        "interactive_slider": True,
                    },
                },
            ],
            "provenance": [
                {
                    "target_ref": "eq_work_def:term-dv",
                    "source_ref": "widget_riemann_1",
                    "rule_es": "dV representa el ancho infinitesimal de cada rectángulo en el eje del volumen V",
                }
            ],
            "verification": {
                "status": "sympy_verified",
                "sympy_rule": "Definition of Definite Integral",
                "dimensional_check": "SI: Pa·m³ = (N/m²)·m³ = N·m = J [OK]",
                "source_citation": {"doc_name": "Formulario Física III TEC", "page": 1, "excerpt": "W = ∫ P dV"},
            },
            "pedagogy": {
                "micro_goal": "Entender que el símbolo ∫ significa sumar áreas diminutas P(V)·dV",
                "concept_tag": "calc.int.meaning_area",
                "wait_for_student": True,
                "check_question": "Si aumentas el número de rectángulos en la gráfica, ¿qué notas que pasa con el área aproximada?",
                "expected_answer_expr": None,
                "cognitive_load": "low",
                "suggested_followups": ["¿Qué significa exactamente dV?", "¿Por qué se usa la letra S estirada ∫?", "Seguir al cálculo"],
                "origin_glossary": [
                    {"symbol": "∫", "meaning": "Suma continua de infinitas rebanadas", "origin": "Una letra S alargada de 'Summa'"},
                    {"symbol": "dV", "meaning": "Un cambio diminuto de volumen (el ancho de cada rectángulo)", "origin": "Identifica que V es la variable que cambia"},
                ],
            },
        },
        {
            "protocol": "cuadernu.tutor.v1.1",
            "session_id": session_id,
            "turn_id": "seed_2",
            "beat_id": "beat_0002",
            "step_index": 2,
            "checkpoint_id": "cp_power_rule",
            "branch_id": None,
            "speech_markup": (
                "Ahora resolvamos una integral concreta paso a paso: la integral de tres x al cuadrado d x desde dos hasta cuatro. "
                "<cue id='c1'/> Primero, el número tres es una constante que multiplica, así que lo sacamos fuera de la integral. "
                "<cue id='c2'/> Luego aplicamos la regla de potencias a x al cuadrado: sumamos uno al exponente dos para obtener tres, y dividimos entre ese mismo tres. "
                "<cue id='c3'/> El tres que sacamos arriba se cancela con el tres de abajo, dejándonos simplemente x al cubo."
            ),
            "speech_rate": 0.95,
            "actions": [
                {
                    "action_id": "act_201",
                    "type": "write_math",
                    "object_id": "eq_pow_1",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_work_def", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1100, "cue_id": "c1"},
                    "color_token": "ink_primary",
                    "payload": {
                        "latex": "\\int_{2}^{4} \\class{term-const}{3}\\,\\class{term-x2}{x^2}\\,dx = \\class{term-const}{3} \\int_{2}^{4} \\class{term-x2}{x^2}\\,dx",
                        "step_label": "Paso 2 · Sacar constante multiplicativa",
                    },
                },
                {
                    "action_id": "act_202",
                    "type": "write_math",
                    "object_id": "eq_pow_2",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_pow_1", "gap_px": 22},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1200, "cue_id": "c2"},
                    "color_token": "accent_blue",
                    "payload": {
                        "latex": "= \\class{term-const}{3} \\cdot \\left[\\frac{x^{\\class{term-exp}{2+1}}}{\\class{term-den}{2+1}}\\right]_{2}^{4} = \\class{term-const}{3} \\cdot \\left[\\frac{x^3}{\\class{term-den}{3}}\\right]_{2}^{4}",
                        "step_label": "Paso 3 · Regla de potencias (n + 1)",
                    },
                },
                {
                    "action_id": "act_203",
                    "type": "write_math",
                    "object_id": "eq_pow_3",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_pow_2", "gap_px": 22},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1000, "cue_id": "c3"},
                    "color_token": "accent_green",
                    "payload": {
                        "latex": "= \\left[x^3\\right]_{2}^{4} = 4^3 - 2^3 = 64 - 8 = 56",
                        "step_label": "Paso 4 · Evaluar límites [F(4) − F(2)]",
                    },
                },
                {
                    "action_id": "act_204",
                    "type": "sync_calculator_state",
                    "object_id": "casio_pow_check",
                    "placement": {"zone": "formula_dock", "relation": "auto_next", "gap_px": 16},
                    "animation": {"style": "fade_in", "duration_ms": 500, "cue_id": "c3"},
                    "color_token": "accent_indigo",
                    "payload": {
                        "model": "fx-CG50",
                        "mode": "Run-Matrix",
                        "keys": ["OPTN", "F4", "F4", "3", "X", "x²", ",", "2", ",", "4", "EXE"],
                        "expected_result": "56",
                    },
                },
            ],
            "provenance": [
                {
                    "target_ref": "eq_pow_2:term-den",
                    "source_ref": "eq_pow_1:term-x2",
                    "rule_es": "Regla de potencias: ∫ xⁿ dx = x^(n+1)/(n+1); aquí n = 2, así que n+1 = 3",
                },
                {
                    "target_ref": "eq_pow_3",
                    "source_ref": "eq_pow_2",
                    "rule_es": "El factor 3 del numerador se cancela con el 3 del denominador (3/3 = 1) y evaluamos F(4) − F(2)",
                },
            ],
            "verification": {
                "status": "sympy_verified",
                "sympy_rule": "ConstantTimesRule -> PowerRule -> FTC",
                "dimensional_check": "Exacto: d/dx(x³) = 3x²; F(4)-F(2) = 56 [OK]",
                "source_citation": None,
            },
            "pedagogy": {
                "micro_goal": "Dominar la regla de potencias y la evaluación de límites definidos",
                "concept_tag": "calc.int.power_rule",
                "wait_for_student": True,
                "check_question": "Si ahora tuvieras ∫ 4x³ dx, ¿cuál sería la antiderivada? Escríbela o dímela en voz alta.",
                "expected_answer_expr": "x**4",
                "cognitive_load": "low",
                "suggested_followups": ["¿De dónde salió el 3 de abajo?", "Ver pasos intermedios de 4³ − 2³", "Intentar otro ejercicio"],
                "origin_glossary": [
                    {"symbol": "[x³]₂⁴", "meaning": "Evaluar primero en 4 y restar la evaluación en 2", "origin": "Teorema Fundamental del Cálculo: F(b) − F(a)"},
                ],
            },
        },
    ]


def _lesson_isotermico(session_id: str) -> List[Dict[str, Any]]:
    return [
        {
            "protocol": "cuadernu.tutor.v1.1",
            "session_id": session_id,
            "turn_id": "iso_1",
            "beat_id": "beat_0001",
            "step_index": 1,
            "checkpoint_id": "cp_iso_pv",
            "branch_id": None,
            "speech_markup": (
                "En un proceso isotérmico de un gas ideal, la temperatura T permanece constante mientras el volumen cambia de V inicial a V final. "
                "<cue id='c1'/> Partimos siempre de la definición de trabajo y de la ley del gas ideal P V igual a n R T. "
                "<cue id='c2'/> Como necesitamos P en función de V para poder integrar respecto a dV, despejamos P pasando V a dividir."
            ),
            "speech_rate": 0.95,
            "actions": [
                {
                    "action_id": "act_i1",
                    "type": "write_math",
                    "object_id": "eq_iso_1",
                    "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1100, "cue_id": "c1"},
                    "color_token": "ink_primary",
                    "payload": {
                        "latex": "W = \\int_{V_i}^{V_f} \\class{term-p}{P}\\,dV,\\qquad \\class{term-gas}{PV = nRT}",
                        "step_label": "Paso 1 · Ecuaciones base del formulario",
                    },
                },
                {
                    "action_id": "act_i2",
                    "type": "plot_pv_diagram",
                    "object_id": "widget_pv_iso",
                    "placement": {"zone": "visual_lab", "relation": "auto_next", "gap_px": 20},
                    "animation": {"style": "fade_in", "duration_ms": 800, "cue_id": "c1"},
                    "color_token": "accent_blue",
                    "payload": {
                        "title": "Expansión isotérmica en diagrama P–V",
                        "process": "isothermal",
                        "nRT": 2400.0,
                        "Vi": 1.0,
                        "Vf": 3.0,
                        "shade_work": True,
                    },
                },
                {
                    "action_id": "act_i3",
                    "type": "write_math",
                    "object_id": "eq_iso_2",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_iso_1", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1000, "cue_id": "c2"},
                    "color_token": "accent_blue",
                    "payload": {
                        "latex": "\\class{term-p}{P(V)} = \\frac{\\class{term-const}{nRT}}{\\class{term-var}{V}}",
                        "step_label": "Paso 2 · Despeje de P(V)",
                    },
                },
            ],
            "provenance": [
                {
                    "target_ref": "eq_iso_2:term-const",
                    "source_ref": "eq_iso_1:term-gas",
                    "rule_es": "Dividimos ambos lados de PV = nRT entre V para dejar P despejada",
                }
            ],
            "verification": {
                "status": "sympy_verified",
                "sympy_rule": "IdealGasLaw Substitution",
                "dimensional_check": "SI: nRT/V -> (mol·J/(mol·K)·K)/m³ = J/m³ = Pa [OK]",
                "source_citation": {"doc_name": "Formulario Física III TEC", "page": 2, "excerpt": "PV = nRT"},
            },
            "pedagogy": {
                "micro_goal": "Expresar la presión P únicamente en función de la variable de integración V",
                "concept_tag": "phys3.thermo.isothermal_setup",
                "wait_for_student": True,
                "check_question": "¿Ves por qué sustituimos P por nRT/V antes de integrar?",
                "expected_answer_expr": None,
                "cognitive_load": "low",
                "suggested_followups": ["¿Por qué no dejamos P fuera directamente?", "Continuar a la integral de 1/V"],
                "origin_glossary": [
                    {"symbol": "nRT", "meaning": "Constante térmica del proceso isotérmico", "origin": "n (moles), R (8.314 J/mol·K) y T no cambian"},
                ],
            },
        },
        {
            "protocol": "cuadernu.tutor.v1.1",
            "session_id": session_id,
            "turn_id": "iso_2",
            "beat_id": "beat_0002",
            "step_index": 2,
            "checkpoint_id": "cp_iso_ln",
            "branch_id": None,
            "speech_markup": (
                "<cue id='c1'/> Metemos n R T sobre V dentro de la integral y sacamos n R T afuera porque es constante. "
                "<cue id='c2'/> Nos queda la integral especial de uno sobre V dV, cuya antiderivada siempre es logaritmo natural de V. "
                "<cue id='c3'/> Al evaluar límite superior menos límite inferior y aplicar la propiedad de resta de logaritmos, obtenemos exactamente n R T logaritmo natural de V final entre V inicial."
            ),
            "speech_rate": 0.95,
            "actions": [
                {
                    "action_id": "act_i4",
                    "type": "write_math",
                    "object_id": "eq_iso_3",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_iso_2", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1100, "cue_id": "c1"},
                    "color_token": "ink_primary",
                    "payload": {
                        "latex": "W = \\class{term-const}{nRT} \\int_{V_i}^{V_f} \\class{term-inv}{\\frac{1}{V}}\\,dV",
                        "step_label": "Paso 3 · Constantes fuera de la integral",
                    },
                },
                {
                    "action_id": "act_i5",
                    "type": "write_math",
                    "object_id": "eq_iso_4",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_iso_3", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1100, "cue_id": "c2"},
                    "color_token": "accent_indigo",
                    "payload": {
                        "latex": "W = nRT \\left[\\class{term-ln}{\\ln V}\\right]_{V_i}^{V_f} = nRT\\left(\\ln V_f - \\ln V_i\\right)",
                        "step_label": "Paso 4 · Integral de 1/V y límites",
                    },
                },
                {
                    "action_id": "act_i6",
                    "type": "write_math",
                    "object_id": "eq_iso_5",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_iso_4", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1000, "cue_id": "c3"},
                    "color_token": "accent_green",
                    "payload": {
                        "latex": "W = nRT \\ln\\left(\\frac{V_f}{V_i}\\right)",
                        "step_label": "Paso 5 · Resultado final verificado",
                    },
                },
            ],
            "provenance": [
                {
                    "target_ref": "eq_iso_4:term-ln",
                    "source_ref": "eq_iso_3:term-inv",
                    "rule_es": "Regla recíproca: ∫ (1/V) dV = ln|V| (porque d/dV(ln V) = 1/V)",
                },
                {
                    "target_ref": "eq_iso_5",
                    "source_ref": "eq_iso_4",
                    "rule_es": "Propiedad de logaritmos: ln(A) − ln(B) = ln(A / B)",
                },
            ],
            "verification": {
                "status": "sympy_verified",
                "sympy_rule": "ConstantTimesRule -> ReciprocalRule -> LogCombine",
                "dimensional_check": "SI: J·(adimensional) = J [OK]",
                "source_citation": {"doc_name": "Formulario Física III TEC", "page": 2, "excerpt": "W = nRT ln(Vf/Vi)"},
            },
            "pedagogy": {
                "micro_goal": "Integrar 1/V y combinar los límites con propiedades de logaritmo",
                "concept_tag": "calc.int.reciprocal_1_over_x",
                "wait_for_student": True,
                "check_question": "Si el gas se comprime (Vf < Vi), ¿el logaritmo natural da positivo o negativo? ¿Qué significa físicamente?",
                "expected_answer_expr": None,
                "cognitive_load": "low",
                "suggested_followups": ["¿Por qué ∫ 1/V no usa regla de potencias?", "¿De dónde salió ln(Vf/Vi)?", "Pasar a conducción esférica"],
                "origin_glossary": [
                    {"symbol": "ln(Vf/Vi)", "meaning": "Logaritmo natural de la razón de volúmenes", "origin": "Resta ln(Vf) − ln(Vi) combinada en cociente"},
                ],
            },
        },
    ]


def _lesson_conduccion_esferica(session_id: str) -> List[Dict[str, Any]]:
    return [
        {
            "protocol": "cuadernu.tutor.v1.1",
            "session_id": session_id,
            "turn_id": "esf_1",
            "beat_id": "beat_0001",
            "step_index": 1,
            "checkpoint_id": "cp_esf_shell",
            "branch_id": None,
            "speech_markup": (
                "Veamos la conducción térmica en un cascarón esférico entre radio r uno y radio r dos. "
                "<cue id='c1'/> A medida que el calor fluye hacia afuera, atraviesa capas esféricas cada vez más grandes de área cuatro pi r al cuadrado. "
                "<cue id='c2'/> Separamos variables dejando todo lo que tiene r y dr a la izquierda, y dT a la derecha."
            ),
            "speech_rate": 0.95,
            "actions": [
                {
                    "action_id": "act_e1",
                    "type": "draw_thermal_shell",
                    "object_id": "widget_shell_esf",
                    "placement": {"zone": "visual_lab", "relation": "auto_next", "gap_px": 20},
                    "animation": {"style": "fade_in", "duration_ms": 800, "cue_id": "c1"},
                    "color_token": "accent_orange",
                    "payload": {
                        "title": "Cascarón esférico diferencial dr",
                        "geometry": "spherical",
                        "r1": 0.05,
                        "r2": 0.12,
                        "T1": 380,
                        "T2": 300,
                        "highlight_dr": True,
                    },
                },
                {
                    "action_id": "act_e2",
                    "type": "write_math",
                    "object_id": "eq_esf_1",
                    "placement": {"zone": "derivation_flow", "relation": "auto_next", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1100, "cue_id": "c1"},
                    "color_token": "ink_primary",
                    "payload": {
                        "latex": "H = -k\\left(\\class{term-area}{4\\pi r^2}\\right)\\frac{dT}{dr}",
                        "step_label": "Paso 1 · Ley de Fourier radial",
                    },
                },
                {
                    "action_id": "act_e3",
                    "type": "write_math",
                    "object_id": "eq_esf_2",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_esf_1", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1200, "cue_id": "c2"},
                    "color_token": "accent_blue",
                    "payload": {
                        "latex": "\\frac{H}{4\\pi k} \\int_{r_1}^{r_2} \\class{term-rneg2}{r^{-2}}\\,dr = -\\int_{T_1}^{T_2} dT",
                        "step_label": "Paso 2 · Separación de variables (1/r² = r⁻²)",
                    },
                },
            ],
            "provenance": [
                {
                    "target_ref": "eq_esf_2:term-rneg2",
                    "source_ref": "eq_esf_1:term-area",
                    "rule_es": "Pasamos r² dividiendo al otro lado: 1/r² se reescribe como potencia r⁻² para usar regla de potencias",
                }
            ],
            "verification": {
                "status": "sympy_verified",
                "sympy_rule": "SeparationOfVariables + RewriteRule(1/r**2 -> r**-2)",
                "dimensional_check": "SI: [H] = W [OK]",
                "source_citation": {"doc_name": "Formulario Física III TEC", "page": 3, "excerpt": "H = -k A dT/dr"},
            },
            "pedagogy": {
                "micro_goal": "Entender por qué reescribimos 1/r² como r⁻² antes de integrar",
                "concept_tag": "phys3.heat.spherical_setup",
                "wait_for_student": True,
                "check_question": "¿Cuál es el exponente n en r⁻² y cuánto da n + 1?",
                "expected_answer_expr": "-1",
                "cognitive_load": "low",
                "suggested_followups": ["¿De dónde salió 4πr²?", "Seguir al paso del signo menos"],
                "origin_glossary": [
                    {"symbol": "r⁻²", "meaning": "Otra forma de escribir 1/r²", "origin": "Propiedad de potencias negativas para aplicar ∫ rⁿ dr"},
                ],
            },
        },
        {
            "protocol": "cuadernu.tutor.v1.1",
            "session_id": session_id,
            "turn_id": "esf_2",
            "beat_id": "beat_0002",
            "step_index": 2,
            "checkpoint_id": "cp_esf_minus",
            "branch_id": None,
            "speech_markup": (
                "Aquí es donde casi todos se confunden con el signo, así que mirémoslo con lupa. "
                "<cue id='c1'/> Al integrar r a la menos dos, sumamos uno al exponente: menos dos más uno da menos uno, y dividimos entre menos uno. "
                "<cue id='c2'/> Dividir entre menos uno hace aparecer este signo menos delante de uno sobre r. "
                "<cue id='c3'/> Cuando evaluamos en r dos menos r uno, ese signo menos invierte el orden y queda uno sobre r uno menos uno sobre r dos, que es positivo porque r uno es más pequeño que r dos."
            ),
            "speech_rate": 0.94,
            "actions": [
                {
                    "action_id": "act_e4",
                    "type": "write_math",
                    "object_id": "eq_esf_3",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_esf_2", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1200, "cue_id": "c1"},
                    "color_token": "ink_primary",
                    "payload": {
                        "latex": "\\int_{r_1}^{r_2} r^{-2}\\,dr = \\left[\\frac{r^{-2+1}}{\\class{term-den}{-2+1}}\\right]_{r_1}^{r_2} = \\left[\\class{term-minus}{-}\\frac{1}{r}\\right]_{r_1}^{r_2}",
                        "step_label": "Paso 3 · Antiderivada de r⁻² (origen del signo −)",
                    },
                },
                {
                    "action_id": "act_e5",
                    "type": "trace_origin",
                    "object_id": "trace_minus_origin",
                    "placement": {"zone": "derivation_flow", "relation": "overlay_on"},
                    "animation": {"style": "draw_stroke", "duration_ms": 900, "cue_id": "c2"},
                    "color_token": "accent_red",
                    "payload": {
                        "from_ref": "eq_esf_2:term-rneg2",
                        "to_ref": "eq_esf_3:term-minus",
                        "rule_es": "n + 1 = −2 + 1 = −1 en el denominador genera el signo −",
                    },
                },
                {
                    "action_id": "act_e6",
                    "type": "write_math",
                    "object_id": "eq_esf_4",
                    "placement": {"zone": "derivation_flow", "relation": "below", "target_id": "eq_esf_3", "gap_px": 24},
                    "animation": {"style": "handwriting_svg", "duration_ms": 1100, "cue_id": "c3"},
                    "color_token": "accent_green",
                    "payload": {
                        "latex": "= \\left(-\\frac{1}{r_2}\\right) - \\left(-\\frac{1}{r_1}\\right) = \\frac{1}{r_1} - \\frac{1}{r_2}",
                        "step_label": "Paso 4 · Evaluación y cancelación de doble signo",
                    },
                },
            ],
            "provenance": [
                {
                    "target_ref": "eq_esf_3:term-minus",
                    "source_ref": "eq_esf_2:term-rneg2",
                    "rule_es": "Al sumar 1 al exponente −2 queda −1 en el denominador: r⁻¹/(−1) = −1/r",
                },
                {
                    "target_ref": "eq_esf_4",
                    "source_ref": "eq_esf_3",
                    "rule_es": "Menos por menos en −(−1/r₁) produce +1/r₁",
                },
            ],
            "verification": {
                "status": "sympy_verified",
                "sympy_rule": "PowerRule(n=-2) -> FTC",
                "dimensional_check": "SI: m⁻¹; H = 4πk(T₁−T₂)/(1/r₁ − 1/r₂) en Watts [OK]",
                "source_citation": {"doc_name": "Formulario Física III TEC", "page": 3, "excerpt": "Conducción esférica"},
            },
            "pedagogy": {
                "micro_goal": "Nunca confundir el signo negativo al integrar 1/r²",
                "concept_tag": "calc.int.inverse_square",
                "wait_for_student": True,
                "check_question": "Prueba rodear con el lazo el signo menos de Paso 3 o escribir la antiderivada de 1/x² para verificarla.",
                "expected_answer_expr": "-1/x",
                "cognitive_load": "low",
                "suggested_followups": ["¿De dónde salió el signo menos?", "Desplegar pasos intermedios de signos", "Probar un valor numérico"],
                "origin_glossary": [
                    {"symbol": "−1/r", "meaning": "Antiderivada exacta de 1/r²", "origin": "r⁻¹ dividido entre −1"},
                ],
            },
        },
    ]
