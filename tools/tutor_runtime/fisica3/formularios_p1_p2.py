"""Formularios oficiales de Parcial 1 y Parcial 2 de Física General III (FI2103 · II-2026).

Fuentes medidas geométricamente sobre los PDFs oficiales:
  - Parcial 1: `documentos-ii-2026_Ecuaciones_Importantes_Parcial_1.pdf` (37 celdas + g)
  - Parcial 2: `documentos-ii-2026_Ecuaciones_Segundo_Parcial_FG3(1).pdf` (24 celdas + 3 constantes)

Mantiene el mismo contrato de `formulario_ii_2026.mapear_formulario_leccion` para que la
pizarra y el panel de fórmulas rodeen con círculos de color la hoja correcta según `parcial`.
"""

from __future__ import annotations

import re
from typing import Any, Dict, List, Sequence, Tuple

from .formulario_ii_2026 import (
    COLORES_FORMULARIO,
    _normaliza_tex,
    _texto_de_paso,
    catalogo_formulario,
    mapear_formulario_leccion,
)

# ---------------------------------------------------------------------- Parcial 1

CROP_NORM_P1: Tuple[float, float, float, float] = (0.15882, 0.09, 0.84059, 0.74636)
HOJA_ASPECTO_P1: Tuple[int, int] = (1000, 1246)

# (id, tex, nombre, es_constante, fila, col, caja)
_CELDAS_P1_DEF: Tuple[Tuple[str, str, str, bool, int, int, Tuple[float, float, float, float]], ...] = (
    # Fila 1 (4 cols)
    ("P1-F1C1", r"F_x = -kx", "Ley de Hooke", False, 1, 1, (0.02114, 0.06994, 0.23555, 0.14612)),
    ("P1-F1C2", r"a = -\frac{k}{m}x", "Aceleración masa-resorte", False, 1, 2, (0.23555, 0.06994, 0.36238, 0.14612)),
    ("P1-F1C3", r"a = -\omega^2 x", "Aceleración del MAS", False, 1, 3, (0.36238, 0.06994, 0.59620, 0.14612)),
    ("P1-F1C4", r"\omega^2 = \frac{k}{m}", "Frecuencia angular masa-resorte", False, 1, 4, (0.59620, 0.06994, 0.97886, 0.14612)),
    # Fila 2 (3 cols)
    ("P1-F2C1", r"\frac{d^2 x}{dt^2} + \omega^2 x = 0", "Ecuación diferencial del MAS", False, 2, 1, (0.02114, 0.14612, 0.23555, 0.22161)),
    ("P1-F2C2", r"x = A\cos(\omega t + \phi)", "Posición en MAS", False, 2, 2, (0.23555, 0.14612, 0.59620, 0.22161)),
    ("P1-F2C3", r"v = -\omega A\sen(\omega t + \phi)", "Velocidad en MAS", False, 2, 3, (0.59620, 0.14612, 0.97886, 0.22161)),
    # Fila 3 (4 cols)
    ("P1-F3C1", r"\omega = 2\pi f", "Frecuencia angular y frecuencia", False, 3, 1, (0.02114, 0.22161, 0.23555, 0.29778)),
    ("P1-F3C2", r"T = \frac{2\pi}{\omega}", "Período y frecuencia angular", False, 3, 2, (0.23555, 0.22161, 0.36238, 0.29778)),
    ("P1-F3C3", r"A = \sqrt{x^2 + \frac{v^2}{\omega^2}}", "Amplitud desde condiciones iniciales", False, 3, 3, (0.36238, 0.22161, 0.59620, 0.29778)),
    ("P1-F3C4", r"\tan(\omega t + \phi) = -\frac{v}{\omega x}", "Fase desde x y v", False, 3, 4, (0.59620, 0.22161, 0.97886, 0.29778)),
    # Fila 4 (4 cols)
    ("P1-F4C1", r"\frac{d^2\theta}{dt^2} \cong -\frac{g}{L}\theta", "Ecuación del péndulo simple", False, 4, 1, (0.02114, 0.29778, 0.23555, 0.37396)),
    ("P1-F4C2", r"\omega^2 = \frac{g}{L}", "Frecuencia angular péndulo simple", False, 4, 2, (0.23555, 0.29778, 0.36238, 0.37396)),
    ("P1-F4C3", r"\frac{d^2\theta}{dt^2} \cong -\frac{mgd}{I}\theta", "Ecuación del péndulo físico", False, 4, 3, (0.36238, 0.29778, 0.59620, 0.37396)),
    ("P1-F4C4", r"\omega^2 = \frac{mgd}{I}", "Frecuencia angular péndulo físico", False, 4, 4, (0.59620, 0.29778, 0.97886, 0.37396)),
    # Fila 5 (4 cols)
    ("P1-F5C1", r"\frac{d^2\theta}{dt^2} = -\frac{\kappa}{I}\theta", "Ecuación del péndulo de torsión", False, 5, 1, (0.02114, 0.37396, 0.23555, 0.45014)),
    ("P1-F5C2", r"\omega^2 = \frac{\kappa}{I}", "Frecuencia angular torsión", False, 5, 2, (0.23555, 0.37396, 0.36238, 0.45014)),
    ("P1-F5C3", r"\theta = \theta_{\text{máx}}\cos(\omega t + \phi)", "Posición angular", False, 5, 3, (0.36238, 0.37396, 0.59620, 0.45014)),
    ("P1-F5C4", r"I_{\text{eje}\parallel} = I_{\text{cm}} + md^2", "Teorema de ejes paralelos", False, 5, 4, (0.59620, 0.37396, 0.97886, 0.45014)),
    # Fila 6 (4 cols)
    ("P1-F6C1", r"I = \frac{P}{4\pi r^2}", "Intensidad de onda esférica", False, 6, 1, (0.02114, 0.45014, 0.23555, 0.52632)),
    ("P1-F6C2", r"\frac{I_1}{I_2} = \frac{r_2^2}{r_1^2}", "Ley del inverso del cuadrado", False, 6, 2, (0.23555, 0.45014, 0.36238, 0.52632)),
    ("P1-F6C3", r"E = \frac{1}{2}kA^2", "Energía mecánica total del MAS", False, 6, 3, (0.36238, 0.45014, 0.59620, 0.52632)),
    ("P1-F6C4", r"E = K + U = \frac{1}{2}mv^2 + \frac{1}{2}kx^2", "Conservación de energía en MAS", False, 6, 4, (0.59620, 0.45014, 0.97886, 0.52632)),
    # Fila 7 (3 cols)
    ("P1-F7C1", r"P_{\text{máx}} = \sqrt{\mu F}\,\omega^2 A^2", "Potencia máxima en cuerda", False, 7, 1, (0.02114, 0.52632, 0.23555, 0.60180)),
    ("P1-F7C2", r"P_{\text{med}} = \frac{1}{2}\sqrt{\mu F}\,\omega^2 A^2", "Potencia media en cuerda", False, 7, 2, (0.23555, 0.52632, 0.59620, 0.60180)),
    ("P1-F7C3", r"P(x,t) = \sqrt{\mu F}\,\omega^2 A^2 \sen^2(kx \pm \omega t)", "Potencia instantánea en cuerda", False, 7, 3, (0.59620, 0.52632, 0.97886, 0.60180)),
    # Fila 8 (3 cols)
    ("P1-F8C1", r"\frac{\partial^2 y}{\partial x^2} = \frac{1}{v^2}\frac{\partial^2 y}{\partial t^2}", "Ecuación de onda", False, 8, 1, (0.02114, 0.60180, 0.23555, 0.67798)),
    ("P1-F8C2", r"y(x,t) = A\cos(kx \pm \omega t)", "Onda viajera sinusoidal", False, 8, 2, (0.23555, 0.60180, 0.59620, 0.67798)),
    ("P1-F8C3", r"y(x,t) = A_{\text{OE}}\sen(kx)\sen(\omega t)", "Onda estacionaria en cuerda", False, 8, 3, (0.59620, 0.60180, 0.97886, 0.67798)),
    # Fila 9 (4 cols)
    ("P1-F9C1", r"v = \lambda f = \frac{\omega}{k}", "Rapidez de onda", False, 9, 1, (0.02114, 0.67798, 0.23555, 0.75416)),
    ("P1-F9C2", r"k = \frac{2\pi}{\lambda}", "Número de onda", False, 9, 2, (0.23555, 0.67798, 0.36238, 0.75416)),
    ("P1-F9C3", r"\omega = 2\pi f", "Frecuencia angular de onda", False, 9, 3, (0.36238, 0.67798, 0.59620, 0.75416)),
    ("P1-F9C4", r"\mu = \frac{m}{L}", "Densidad lineal de masa", False, 9, 4, (0.59620, 0.67798, 0.97886, 0.75416)),
    # Fila 10 (4 cols)
    ("P1-F10C1", r"v = \sqrt{\frac{F}{\mu}}", "Rapidez de onda transversal", False, 10, 1, (0.02114, 0.75416, 0.23555, 0.83033)),
    ("P1-F10C2", r"\lambda_n = \frac{2L}{n}", "Longitudes de onda estacionarias", False, 10, 2, (0.23555, 0.75416, 0.36238, 0.83033)),
    ("P1-F10C3", r"f_n = \frac{v}{\lambda_n}", "Frecuencias normales", False, 10, 3, (0.36238, 0.75416, 0.59620, 0.83033)),
    ("P1-F10C4", r"f_n = n f_1", "Armónicos de cuerda fija", False, 10, 4, (0.59620, 0.75416, 0.97886, 0.83033)),
    # Constante
    ("P1-C1", r"g = 9{,}8\ \text{m/s}^2", "Aceleración de la gravedad", True, 11, 1, (0.09500, 0.94500, 0.28500, 0.99600)),
)

_REGLAS_P1: Tuple[Tuple[str, re.Pattern[str]], ...] = (
    ("P1-F1C1", re.compile(r"F_x\s*=\s*-k\s*x", re.I)),
    ("P1-F1C2", re.compile(r"a\s*=\s*-\s*\\frac\{k\}\{m\}\s*x", re.I)),
    ("P1-F1C3", re.compile(r"a(?:_x|\(t\))?\s*=\s*-\s*\\omega\^2\s*(?:x|A)|a_\{?(?:\\text\{)?m[áa]x\}?\}?\s*=\s*\\omega\^2", re.I)),
    ("P1-F1C4", re.compile(r"\\omega\^2\s*=\s*\\frac\{k\}\{m\}|\\omega\s*=\s*\\sqrt\{\\frac\{k\}\{m\}\}|k\s*=\s*m\s*\\omega\^2", re.I)),
    ("P1-F2C1", re.compile(r"\\frac\{d\^2\s*x\}\{dt\^2\}", re.I)),
    ("P1-F2C2", re.compile(r"x(?:\(t\)|_0|\(0\))?\s*=\s*A\s*\\cos|\\cos\s*\(\s*\\omega\s*t\s*[+-]\s*\\phi", re.I)),
    ("P1-F2C3", re.compile(r"v(?:\(t\)|_x|_0|\(0\))?\s*=\s*-\s*\\omega\s*A\s*\\(?:sen|sin)|v_\{?(?:\\text\{)?m[áa]x\}?\}?\s*=\s*\\omega\s*A", re.I)),
    ("P1-F3C1", re.compile(r"\\omega\s*=\s*2\\pi\s*f|f\s*=\s*\\frac\{\\omega\}\{2\\pi\}|f\s*=\s*\\frac\{1\}\{T\}", re.I)),
    ("P1-F3C2", re.compile(r"T\s*=\s*\\frac\{2\\pi\}\{\\omega\}|\\omega\s*=\s*\\frac\{2\\pi\}\{T\}|T\s*=\s*2\\pi\\sqrt", re.I)),
    ("P1-F3C3", re.compile(r"A\s*=\s*\\sqrt\{x\^2", re.I)),
    ("P1-F3C4", re.compile(r"\\tan\s*\(\s*(?:\\omega\s*t\s*\+\s*)?\\phi\s*\)\s*=", re.I)),
    ("P1-F4C1", re.compile(r"\\frac\{d\^2\\theta\}\{dt\^2\}\s*(?:\\cong|=)\s*-\s*\\frac\{g\}\{L\}", re.I)),
    ("P1-F4C2", re.compile(r"\\omega\^2\s*=\s*\\frac\{g\}\{L\}|\\omega\s*=\s*\\sqrt\{\\frac\{g\}\{L\}\}|T\s*=\s*2\\pi\\sqrt\{\\frac\{L\}\{g\}\}", re.I)),
    ("P1-F4C3", re.compile(r"\\frac\{mgd\}\{I\}\\theta", re.I)),
    ("P1-F4C4", re.compile(r"\\omega\^2\s*=\s*\\frac\{mgd\}\{I\}", re.I)),
    ("P1-F5C1", re.compile(r"\\frac\{\\kappa\}\{I\}\\theta", re.I)),
    ("P1-F5C2", re.compile(r"\\omega\^2\s*=\s*\\frac\{\\kappa\}\{I\}", re.I)),
    ("P1-F5C3", re.compile(r"\\theta\s*=\s*\\theta_\{?(?:\\text\{)?m[áa]x", re.I)),
    ("P1-F5C4", re.compile(r"I_\{\\text\{cm\}\}\s*\+\s*md\^2", re.I)),
    ("P1-F6C1", re.compile(r"I\s*=\s*\\frac\{P\}\{4\\pi\s*r\^2\}", re.I)),
    ("P1-F6C2", re.compile(r"\\frac\{I_1\}\{I_2\}\s*=\s*\\frac\{r_2\^2\}\{r_1\^2\}", re.I)),
    ("P1-F6C3", re.compile(r"E\s*=\s*\\frac\{1\}\{2\}\s*k\s*A\^2", re.I)),
    ("P1-F6C4", re.compile(r"E\s*=\s*K\s*\+\s*U", re.I)),
    ("P1-F7C1", re.compile(r"P_\{?(?:\\text\{)?m[áa]x\}?\}?\s*=\s*\\sqrt\{\\mu\s*F\}", re.I)),
    ("P1-F7C2", re.compile(r"P_\{?(?:\\text\{)?med\}?\}?\s*=\s*\\frac\{1\}\{2\}\\sqrt\{\\mu\s*F\}", re.I)),
    ("P1-F7C3", re.compile(r"P\(x,t\)\s*=\s*\\sqrt\{\\mu\s*F\}", re.I)),
    ("P1-F8C1", re.compile(r"\\frac\{\\partial\^2\s*y\}\{\\partial\s*x\^2\}", re.I)),
    ("P1-F8C2", re.compile(r"y(?:_1|_2)?\(x,t\)\s*=\s*(?:-?\s*A|4\{,\}25|-4\{,\}25)[^=\n]{0,20}\\cos\s*\(\s*(?:kx|10\\pi)", re.I)),
    ("P1-F8C3", re.compile(r"A_\{?(?:\\text\{)?OE\}?\}?\s*\\(?:sen|sin)|\\(?:sen|sin)\s*\(\s*(?:kx|10\\pi\s*x)\s*\)\s*\\(?:sen|sin)\s*\(\s*(?:\\omega\s*t|240\\pi\s*t)", re.I)),
    ("P1-F9C1", re.compile(r"v(?:_1|_2)?\s*=\s*\\lambda(?:_1|_2)?\s*f|v\s*=\s*\\frac\{\\omega\}\{k\}|\\lambda\s*f\s*=\s*\\frac\{\\omega\}\{k\}", re.I)),
    ("P1-F9C2", re.compile(r"k\s*=\s*\\frac\{2\\pi\}\{\\lambda\}|\\lambda\s*=\s*\\frac\{2\\pi\}\{k\}", re.I)),
    ("P1-F9C4", re.compile(r"\\mu(?:_1|_2)?\s*=\s*\\frac\{m(?:_1|_2)?\}\{L\}|m(?:_1|_2)?\s*=\s*\\mu(?:_1|_2)?\s*L", re.I)),
    ("P1-F10C1", re.compile(r"v(?:_1|_2)?\s*=\s*\\sqrt\{\\frac\{F\}\{\\mu(?:_1|_2)?\}\}|F\s*=\s*\\mu(?:_1|_2)?\s*v(?:_1|_2)?\^2", re.I)),
    ("P1-F10C2", re.compile(r"\\lambda_\{?[n12]\}?\s*=\s*\\frac\{2L|n\s*=\s*\\frac\{2L\}\{\\lambda", re.I)),
    ("P1-F10C3", re.compile(r"f_\{?[n12]'?\}?\s*=\s*\\frac\{v(?:_1|_2)?\}\{\\lambda|f_\{?[12]'?\}?\s*=\s*\\frac\{v(?:_1|_2)?\}\{2L", re.I)),
    ("P1-F10C4", re.compile(r"f_n\s*=\s*n\s*f_1", re.I)),
    ("P1-C1", re.compile(r"g\s*=\s*9\{?,\}?8", re.I)),
)

# ---------------------------------------------------------------------- Parcial 2

CROP_NORM_P2: Tuple[float, float, float, float] = (0.16941, 0.09, 0.81235, 0.80818)
HOJA_ASPECTO_P2: Tuple[int, int] = (1000, 1446)

_CELDAS_P2_DEF: Tuple[Tuple[str, str, str, bool, int, int, Tuple[float, float, float, float]], ...] = (
    # Fila 1
    ("P2-F1C1", r"Y = \frac{F_\perp / A}{\Delta l / l_0}", "Módulo de Young", False, 1, 1, (0.04986, 0.06392, 0.23559, 0.15949)),
    ("P2-F1C2", r"B = -\frac{F_\perp / A}{\Delta V / V_0}", "Módulo de volumen", False, 1, 2, (0.23559, 0.06392, 0.71912, 0.15949)),
    ("P2-F1C3", r"S = \frac{F_\parallel / A}{x / h}", "Módulo de corte", False, 1, 3, (0.71912, 0.06392, 0.97758, 0.15949)),
    # Fila 2
    ("P2-F2C1", r"\rho = \frac{m}{V}", "Definición de densidad", False, 2, 1, (0.04986, 0.15949, 0.23559, 0.25443)),
    ("P2-F2C2", r"\rho_r = \frac{\rho}{\rho_{\text{agua}}}", "Densidad relativa", False, 2, 2, (0.23559, 0.15949, 0.71912, 0.25443)),
    ("P2-F2C3", r"p = \frac{dF_\perp}{dA}", "Definición de presión", False, 2, 3, (0.71912, 0.15949, 0.97758, 0.25443)),
    # Fila 3
    ("P2-F3C1", r"\frac{dm}{dt} = \rho \frac{dV}{dt}", "Flujo másico y caudal", False, 3, 1, (0.04986, 0.25443, 0.23559, 0.34430)),
    ("P2-F3C2", r"\text{módulo elástico} = \frac{\text{esfuerzo}}{\text{deformación}}", "Módulo elástico general", False, 3, 2, (0.23559, 0.25443, 0.71912, 0.34430)),
    ("P2-F3C3", r"\gamma = \frac{F}{2l}", "Tensión superficial", False, 3, 3, (0.71912, 0.25443, 0.97758, 0.34430)),
    # Fila 4
    ("P2-F4C1", r"F_\perp = \int p\,dA", "Fuerza hidrostática (integral)", False, 4, 1, (0.04986, 0.34430, 0.23559, 0.43671)),
    ("P2-F4C2", r"d\tau = r\sen\theta\,dF_\perp = r\sen\theta\,p\,dA", "Torque hidrostático diferencial", False, 4, 2, (0.23559, 0.34430, 0.71912, 0.43671)),
    ("P2-F4C3", r"p_{\text{man}} = p_{\text{abs}} - p_{\text{atm}}", "Presión manométrica", False, 4, 3, (0.71912, 0.34430, 0.97758, 0.43671)),
    # Fila 5
    ("P2-F5C1", r"p = p_0 + \rho gh", "Presión hidrostática en profundidad", False, 5, 1, (0.04986, 0.43671, 0.23559, 0.52595)),
    ("P2-F5C2", r"p_2 - p_1 = -\rho g(y_2 - y_1)", "Variación vertical de presión", False, 5, 2, (0.23559, 0.43671, 0.71912, 0.52595)),
    ("P2-F5C3", r"\frac{dV}{dt} = Av", "Caudal volumétrico", False, 5, 3, (0.71912, 0.43671, 0.97758, 0.52595)),
    # Fila 6
    ("P2-F6C1", r"A_1 v_1 = A_2 v_2", "Ecuación de continuidad", False, 6, 1, (0.04986, 0.52595, 0.23559, 0.61392)),
    ("P2-F6C2", r"p_1 + \rho g y_1 + \frac{1}{2}\rho v_1^2 = p_2 + \rho g y_2 + \frac{1}{2}\rho v_2^2", "Ecuación de Bernoulli", False, 6, 2, (0.23559, 0.52595, 0.71912, 0.61392)),
    ("P2-F6C3", r"F_B = \rho g V", "Empuje de Arquímedes", False, 6, 3, (0.71912, 0.52595, 0.97758, 0.61392)),
    # Fila 7
    ("P2-F7C1", r"F_S = 6\pi r \eta v", "Ley de Stokes", False, 7, 1, (0.04986, 0.61392, 0.23559, 0.70886)),
    ("P2-F7C2", r"v(r) = \frac{p_1 - p_2}{4\eta L}(R^2 - r^2)", "Perfil parabólico de velocidad", False, 7, 2, (0.23559, 0.61392, 0.71912, 0.70886)),
    ("P2-F7C3", r"v_T = \frac{2}{9}\frac{r^2 g}{\eta}(\rho - \rho')", "Rapidez terminal viscosa", False, 7, 3, (0.71912, 0.61392, 0.97758, 0.70886)),
    # Fila 8
    ("P2-F8C1", r"N_R = \frac{\rho v D}{\eta}", "Número de Reynolds", False, 8, 1, (0.04986, 0.70886, 0.23559, 0.80380)),
    ("P2-F8C2", r"\frac{dV}{dt} = \frac{(p_1 - p_2)\pi R^4}{8\eta L}", "Ley de Poiseuille", False, 8, 2, (0.23559, 0.70886, 0.71912, 0.80380)),
    ("P2-F8C3", r"F = \eta A \frac{v}{\ell}", "Fuerza viscosa laminar", False, 8, 3, (0.71912, 0.70886, 0.97758, 0.80380)),
    # Constantes
    ("P2-C1", r"g = 9{,}80\ \text{m/s}^2", "Aceleración de la gravedad", True, 9, 1, (0.02500, 0.91800, 0.28500, 0.99200)),
    ("P2-C2", r"\rho_{\text{agua}} = 1000\ \text{kg/m}^3", "Densidad del agua", True, 9, 2, (0.31000, 0.91800, 0.62000, 0.99200)),
    ("P2-C3", r"1\ \text{atm} = 1{,}0132\times 10^5\ \text{Pa}", "Presión atmosférica estándar", True, 9, 3, (0.64000, 0.91800, 0.93500, 0.99200)),
)

_REGLAS_P2: Tuple[Tuple[str, re.Pattern[str]], ...] = (
    ("P2-F1C1", re.compile(r"Y\s*=\s*\\frac\{F_\\perp", re.I)),
    ("P2-F1C2", re.compile(r"B\s*=\s*-\s*\\frac\{F_\\perp", re.I)),
    ("P2-F1C3", re.compile(r"S\s*=\s*\\frac\{F_\\parallel", re.I)),
    ("P2-F2C1", re.compile(r"\\rho(?:_[AB])?\s*=\s*\\frac\{m(?:_[AB])?\}\{V|m(?:_[AB])?\s*=\s*\\rho(?:_[AB])?\s*V", re.I)),
    ("P2-F2C2", re.compile(r"\\rho_r\s*=\s*\\frac\{\\rho\}", re.I)),
    ("P2-F2C3", re.compile(r"p\s*=\s*\\frac\{dF_\\perp\}\{dA\}|dF(?:_\\perp|_\{?[a-z]+\}?)?\s*=\s*p\s*(?:\\,)?dA", re.I)),
    ("P2-F3C1", re.compile(r"\\frac\{dm\}\{dt\}\s*=\s*\\rho\s*\\frac\{dV\}\{dt\}|\\dot\{m\}\s*=\s*\\rho", re.I)),
    ("P2-F3C2", re.compile(r"m[óo]dulo\s+el[áa]stico", re.I)),
    ("P2-F3C3", re.compile(r"\\gamma\s*=\s*\\frac\{F\}\{2l\}", re.I)),
    ("P2-F4C1", re.compile(r"F(?:_\\perp|_\{?[a-z]+\}?)?\s*=\s*\\int", re.I)),
    ("P2-F4C2", re.compile(r"d\\tau\s*=\s*r\\(?:sen|sin)", re.I)),
    ("P2-F4C3", re.compile(r"p_\{?(?:\\text\{)?man\}?\}?\s*=\s*p_\{?(?:\\text\{)?abs\}?\}?\s*[-−]\s*p_\{?(?:\\text\{)?atm\}?\}?", re.I)),
    ("P2-F5C1", re.compile(r"p(?:\([yh]\))?\s*=\s*p_0\s*\+\s*\\rho\s*g\s*h|\\Delta\s*p\s*=\s*\\rho\s*g\s*h|p_a\s*[-−]\s*p_b\s*=\s*\\rho\s*g|p\s*=\s*\\rho\s*g\s*\(H\s*[-−]\s*y\)|p\s*=\s*\\rho\s*g\s*h", re.I)),
    ("P2-F5C2", re.compile(r"p_2\s*[-−]\s*p_1\s*=\s*-\s*\\rho\s*g", re.I)),
    ("P2-F5C3", re.compile(r"\\frac\{dV\}\{dt\}\s*=\s*A\s*v|Q\s*=\s*A(?:_[ab12])?\s*v(?:_[ab12])?|v(?:_[ab12])?\s*=\s*\\frac\{Q\}\{A|v\s*=\s*\\frac\{dV/dt\}\{A\}", re.I)),
    ("P2-F6C1", re.compile(r"A_1\s*v_1\s*=\s*A_2\s*v_2|A_a\s*v_a\s*=\s*A_b\s*v_b", re.I)),
    ("P2-F6C2", re.compile(r"p_1\s*\+\s*\\rho\s*g\s*y_1|p_a\s*\+\s*\\rho\s*g\s*y_a|p_a\s*\+\s*\\frac\{1\}\{2\}\\rho\s*v_a\^2|Bernoulli", re.I)),
    ("P2-F6C3", re.compile(r"F_B\s*=\s*\\rho(?:_\{?[a-z]+\}?)?\s*g\s*V", re.I)),
    ("P2-F7C1", re.compile(r"F_S\s*=\s*6\\pi", re.I)),
    ("P2-F7C2", re.compile(r"v\(r\)\s*=\s*\\frac\{p_1\s*[-−]\s*p_2\}\{4\\eta", re.I)),
    ("P2-F7C3", re.compile(r"v_T\s*=\s*\\frac\{2\}\{9\}", re.I)),
    ("P2-F8C1", re.compile(r"N_R\s*=\s*\\frac\{\\rho\s*v\s*D\}\{\\eta\}", re.I)),
    ("P2-F8C2", re.compile(r"\\frac\{\(p_1\s*[-−]\s*p_2\)\\pi\s*[Rr]\^4\}\{8\\eta\s*L\}|\\frac\{\\Delta\s*p\s*(?:\\,)?\\pi\s*[Rr]\^4\}\{8\\eta\s*L\}|Poiseuille", re.I)),
    ("P2-F8C3", re.compile(r"F\s*=\s*\\eta\s*A\s*\\frac\{v\}\{\\ell\}", re.I)),
    ("P2-C1", re.compile(r"g\s*=\s*9\{?,\}?80?", re.I)),
    ("P2-C2", re.compile(r"\\rho_\{?(?:\\text\{)?agua\}?\}?\s*=\s*1\\,?000|1000\s*(?:\\ )?\\text\{kg/m\}\^3", re.I)),
    ("P2-C3", re.compile(r"1\{?,\}?0132\s*\\times\s*10\^5", re.I)),
)

_HOJAS_PARCIAL: Dict[int, Dict[str, Any]] = {
    1: {
        "parcial": 1,
        "titulo": "Formulario oficial · I Parcial FI2103 II-2026",
        "hoja_url": "/api/formulario/p1/hoja.png",
        "crop_norm": CROP_NORM_P1,
        "aspecto": HOJA_ASPECTO_P1,
        "celdas": _CELDAS_P1_DEF,
        "reglas": _REGLAS_P1,
    },
    2: {
        "parcial": 2,
        "titulo": "Formulario oficial · II Parcial FI2103 II-2026",
        "hoja_url": "/api/formulario/p2/hoja.png",
        "crop_norm": CROP_NORM_P2,
        "aspecto": HOJA_ASPECTO_P2,
        "celdas": _CELDAS_P2_DEF,
        "reglas": _REGLAS_P2,
    },
}


def _celdas_en_texto_parcial(blob: str, reglas: Sequence[Tuple[str, re.Pattern[str]]]) -> List[str]:
    texto = blob + "\n" + _normaliza_tex(blob)
    out: List[str] = []
    for cid, pat in reglas:
        if pat.search(texto):
            out.append(cid)
    return out


def catalogo_formulario_parcial(parcial: int) -> Dict[str, Any]:
    """Devuelve el catálogo completo de la hoja oficial del Parcial 1, 2 o 3."""
    if parcial == 3:
        cat = catalogo_formulario()
        cat["parcial"] = 3
        return cat
    info = _HOJAS_PARCIAL[parcial]
    celdas = [
        {
            "id": cid,
            "fila": fila,
            "col": col,
            "etiqueta": f"Constante {col}" if es_c else f"Fila {fila} · Col {col}",
            "nombre": nombre,
            "tex": tex,
            "es_constante": es_c,
            "caja": list(caja),
        }
        for cid, tex, nombre, es_c, fila, col, caja in info["celdas"]
    ]
    return {
        "parcial": parcial,
        "titulo": info["titulo"],
        "hoja_url": info["hoja_url"],
        "aspecto": list(info["aspecto"]),
        "total": len(celdas),
        "celdas": celdas,
    }


def _mapear_parcial_12(lec: Dict[str, Any], parcial: int) -> Dict[str, Any]:
    info = _HOJAS_PARCIAL[parcial]
    por_id = {cid: (tex, nombre, es_c, fila, col, caja) for cid, tex, nombre, es_c, fila, col, caja in info["celdas"]}
    reglas = info["reglas"]
    pasos = lec.get("pasos") or []

    por_paso: List[List[str]] = []
    pasos_por_celda: Dict[str, List[int]] = {}
    for idx, p in enumerate(pasos):
        cids = _celdas_en_texto_parcial(_texto_de_paso(p), reglas)
        por_paso.append(cids)
        for cid in cids:
            pasos_por_celda.setdefault(cid, []).append(idx)

    formulas_blob = "\n".join(str(x) for x in (lec.get("formulas") or []))
    for cid in _celdas_en_texto_parcial(formulas_blob, reglas):
        if cid not in pasos_por_celda:
            pasos_por_celda[cid] = [0] if pasos else []
            if por_paso and cid not in por_paso[0]:
                por_paso[0].append(cid)

    ordenadas = sorted(
        pasos_por_celda.keys(),
        key=lambda cid: (
            1 if por_id[cid][2] else 0,
            min(pasos_por_celda[cid]) if pasos_por_celda[cid] else 999,
            por_id[cid][3],
            por_id[cid][4],
        ),
    )

    celdas_out: List[Dict[str, Any]] = []
    for pos, cid in enumerate(ordenadas):
        tex, nombre, es_c, fila, col, caja = por_id[cid]
        celdas_out.append({
            "id": cid,
            "num": pos + 1,
            "color": COLORES_FORMULARIO[pos % len(COLORES_FORMULARIO)],
            "fila": fila,
            "col": col,
            "etiqueta": f"Constante · P{parcial}" if es_c else f"Fila {fila} · Col {col}",
            "nombre": nombre,
            "tex": tex,
            "es_constante": es_c,
            "caja": list(caja),
            "pasos": sorted(set(pasos_por_celda[cid])),
            "derivado": "",
        })

    orden_num = {c["id"]: c["num"] for c in celdas_out}
    por_paso_ord = [sorted(set(cids), key=lambda cid: orden_num.get(cid, 999)) for cids in por_paso]

    por_accion: Dict[str, Dict[str, Any]] = {}
    ultima: List[str] = []
    for p in pasos:
        for tr in p.get("tramos") or []:
            for acc in tr.get("acc") or []:
                if not isinstance(acc, dict) or acc.get("a") != "mate" or not acc.get("id"):
                    continue
                cids = [c for c in _celdas_en_texto_parcial(str(acc.get("tex") or ""), reglas) if c in orden_num]
                if not cids:
                    if ultima:
                        por_accion[str(acc["id"])] = {"celdas": list(ultima), "formula": False, "hereda": True}
                    continue
                cids.sort(key=lambda c: (1 if por_id[c][2] else 0, orden_num[c]))
                por_accion[str(acc["id"])] = {"celdas": cids, "formula": acc.get("rol") == "formula"}
                base = [c for c in cids if not por_id[c][2]]
                if base:
                    ultima = base

    return {
        "parcial": parcial,
        "titulo": info["titulo"],
        "hoja_url": info["hoja_url"],
        "aspecto": list(info["aspecto"]),
        "celdas": celdas_out,
        "por_paso": por_paso_ord,
        "por_accion": por_accion,
        "fuera_de_hoja": [],
    }


def mapear_formulario_por_parcial(lec: Dict[str, Any]) -> Dict[str, Any]:
    """Enruta una lección a su hoja oficial correspondiente (Parcial 1, 2 o 3)."""
    parcial = int(lec.get("parcial") or (lec.get("fuente") or {}).get("parcial") or 3)
    if parcial in (1, 2):
        return _mapear_parcial_12(lec, parcial)
    mapa = mapear_formulario_leccion(lec)
    mapa["parcial"] = 3
    return mapa


def verificar_origen_formulario_parcial(lec: Dict[str, Any]) -> List[str]:
    """Comprueba que las lecciones de Parcial 1 y 2 partan de casillas oficiales de su hoja."""
    parcial = int(lec.get("parcial") or (lec.get("fuente") or {}).get("parcial") or 3)
    if parcial not in (1, 2):
        return []
    mapa = _mapear_parcial_12(lec, parcial)
    base = [c for c in mapa["celdas"] if not c["es_constante"]]
    if not base:
        return [f"No se detectó ninguna ecuación base del formulario oficial del Parcial {parcial}"]
    return []
