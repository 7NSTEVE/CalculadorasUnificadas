# Auditoría Forense de Patrones y Blindaje II-2026 · Física General III (FI2103)

**Fecha:** 26 de noviembre de 2026 · **Rama:** `feat/tutor-audiovisual`  
**Fuentes auditadas:** `Patrones.txt` + **24 PDFs locales** (`23` únicos + `1` duplicado byte a byte) que abarcan los **17 exámenes/reposiciones históricos** (`II-2023` a `I-2026`), el Folleto FI2103 (`138` págs.), el Programa II-2026 (`13` págs.), los `21` Ejercicios Recomendados, el Formulario oficial II-2026 (`42` casillas) y las dos prácticas oficiales II-2026 (`32` problemas).

---

## 1. Matriz Verificada de Formularios Históricos (`17/17`) frente a II-2026

Cada hoja de ecuaciones de los 12 parciales (`23-II` a `26-I`, ordinario y extraordinario) y 5 reposiciones (`23-II-R` a `26-I-R`) se renderizó e inspeccionó visualmente contra `documentos-ii-2026_Ecuaciones_Tercer_Parcial_FG3.pdf` (`14 × 3 = 42` casillas):

| Casilla / Grupo | 10 Parc. `24-I..26-I` | Serie completa `17/17` (`23-II..26-I` incl. 5 Rep.) | II-2026 | Estado vs. `Patrones.txt` |
|---|---|---|---|---|
| `F1C3` Esfuerzo térmico $F/A = -Y\alpha\Delta T$ | `0/10` | **`0/17`** | **Nueva** | **CONFIRMED** — 100 % inédita en la serie histórica (`0` págs. en el Folleto). Cubierta por `PR2-S13`, `P26-1` y arquetipo `esfuerzo_termico`. |
| `F12C2` Escala Fahrenheit $T_{^\circ\text{F}} = \frac{9}{5}T_{^\circ\text{C}} + 32$ | `0/10` | **`0/17`** | **Nueva** | **CONFIRMED** — 100 % inédita (`0` págs. en el Folleto). Cubierta por `PR2-S1` y `P26-2`. |
| `F1C2` Dilatación volumétrica $\Delta V = \beta V_0\Delta T$ | `0/10` | `4/17` (`23-II-O/E/R`, `24-I-R`) | **Reintroducida** | **CORRECCIÓN A `Patrones.txt`**: sí apareció en II-2023 junto con $\Delta A = 2\alpha A_0\Delta T$ (que **no** volvió en II-2026). |
| `F1C1` Dilatación lineal $\Delta L = \alpha L_0\Delta T$ | `1/10` (`26-I-E`) | `6/17` (`23-II` ×3, `24-I-R`, `26-I-E`, `26-I-R`) | **Sí** | **CONFIRMED** — en `26-I-O` se evaluó en desarrollo (8 pts) antes de imprimirla. |
| `F11C3`, `F12C1` Ciclo Otto ($e_{\text{otto}}, r$) | `2/10` (`26-I-O/E`) | `3/17` (`26-I-O/E/R`) | **Sí** | **CONFIRMED** — fija desde I-2026 (`0` págs. en el Folleto); cubierta por `E21`, `PR2-S7`, `PR2-D9` y `otto_completo_inverso`. |
| `F11C1`, `F11C2` ($dQ = nC_V dT$, $dQ = nC_p dT$) | `2/10` (`26-I-O/E`) | `3/17` (`26-I-O/E/R`) | **Sí** | **OMITIDO en `Patrones.txt`**: introducidas en I-2026 y mantenidas en II-2026. |
| `F14C2` ($1\text{ L}=1000\text{ cm}^3$) y `F14C1` ($101325\text{ Pa}$) | `0/10` | **`0/17`** | **Cambiadas** | **OMITIDO en `Patrones.txt`**: cambio de notación exacto en II-2026; cubiertas por `PR2-D9`, `P26-3` y `otto_completo_inverso`. |
| Carnot ($e_{\text{Carnot}}, K_{\text{Carnot}}$) | `10/10` | **`17/17`** | **Eliminadas** | **CONFIRMED** — retiradas tras 17/17 exámenes previos (corte hasta sección 20.5). |
| Entropía ($\Delta S = \int dQ/T$) | `8/10` (ausente en `25-II-O/E`) | `15/17` | **Eliminada** | **CONFIRMED** — también omitida en `25-II-O/E`. |
| Van der Waals y $C_V = 3R$ (poliatómico) | `4/10` | VdW `5/17`, $C_V=3R$ `5/17` | **Ausentes** | **CONFIRMED** — ausentes en `25-II`, `26-I` y `II-2026`. |

---

## 2. Molde Exacto de 4 Partes en `26-I-O` y `26-I-E` (50 pts)

Ambos exámenes más recientes siguen una estructura ranura por ranura (implementada en `Simulacro Predictivo II-2026` en `assets/cuadernu/tutor_web/js/ui/simulacro.js`):

1. **Parte I · Selección conceptual (8 pts = 4 × 2 pts):** #1 Dilatación / esfuerzo térmico, #2 Calorimetría / cambio de fase, #3 Procesos / gases / 1.ª ley, #4 Segunda ley / máquinas / transferencia.
2. **Parte II · Selección de cálculo (9 pts = 3 × 3 pts):** 3 cálculos directos (escalas/calor/dilatación, gases/1.ª ley, Otto/máquinas/refrigerador).
3. **Parte III · 2 desarrollos cortos (16–17 pts = 8–9 pts c/u):** calorimetría (fusión parcial/total) o fórmula nueva II-2026 + conducción con integral.
4. **Parte IV · 1 desarrollo largo integrador (16–17 pts):** ciclo termodinámico completo (recta $P=\alpha V$, ciclo antihorario, información incompleta o ciclo Otto).

---

## 3. Duplicado, Anomalías Editoriales y 7 Erratas Oficiales Documentadas

- **Archivo duplicado byte a byte en `D:\Descargas`:** `banco-de-examenes_I-2025_III_examen_parcial_extra_solución (1).pdf` es idéntico (`SHA-256: c7216c6af577`) a `banco-de-examenes_I-2025_III_examen_parcial_extra_solución.pdf`.
- **Anomalías en `Práctica del Parcial 3_IIS2026.pdf` (avisadas en las lecciones):**
  - `PR1-2`: encabezado `(15 pts)`, incisos suman `6 + 4 + 2 = 12 pts` (todo el hielo se funde: $T_f = 10{,}7\,^\circ\text{C}$, opuesto a `PR2-D7` con fusión parcial $T_f = 0\,^\circ\text{C}$).
  - `PR1-5`: encabezado `(20 pts)`, incisos suman `3 + 4 + 6 + 2 + 3 = 18 pts`.
  - `PR1-6`: ciclo **antihorario** ($W_{\text{neto}} = -1869\text{ J} < 0$, consume trabajo; no aplica $e = W/Q_H$).
- **7 erratas confirmadas en solucionarios/folleto** (6 adicionales a `25-II-E` P6 citada en `Patrones.txt`):
  1. `25-II-E` P6 (`Q045`): usa volumen $\pi r^2 L$ en lugar de área lateral $2\pi r L$.
  2. `25-II-E` P7 (`Q099`): usa $n=1{,}5$ en vez de $0{,}15$ mol y omite signo en $\Delta U_{AC}$.
  3. `25-II-R` P3 (`Q050`): omite el factor $1/1000$ de $du = 1000\,dy$.
  4. Folleto `3.1` (`Q012`): confunde $2\beta$ vs. $3\beta$.
  5. Folleto `3.2` (`Q029`): imprime `-88,9 J` en vez de `kJ`.
  6. Folleto `3.3-9` (`Q061`): sustituye `m²` en vez de `cm²`.
  7. Folleto `3.4-7` (`Q060`, omite factor `2` en $\int r^{-3}dr$) y `4.2-3` (`Q116`, imprime `76 K` en vez de `91,1 K`).

---

## 4. Correcciones Aplicadas al Tutor CuadernU

1. **Precisión del mapeo de las 42 casillas (`formulario_ii_2026.py`):**
   - Eliminado el falso positivo `F13C1` (Boltzmann) en conducción (`PR2-D3`, donde `k_B` es la conductividad de la capa B).
   - Eliminados los `17` falsos positivos `F10C3` (conversión a Kelvin) en lecciones que solo mencionan la palabra `kelvin` sin sumar/restar `273,15`.
   - Corregidas las detecciones de `F8C2` (`PR2-D6`, `PR2-D9`) y `F14C2` (`PR2-D9`, `P26-3`), y enlazadas las casillas oficiales en las selecciones conceptuales `Q078`, `Q027` y `Q121`.
2. **Cobertura `42/42` casillas (`banco_blindaje_ii_2026.py` + `banco_practica.py`):**
   - Agregadas `P26-1` (Dilatación + Esfuerzo térmico `F1C3`), `P26-2` (Escala Fahrenheit `F12C2` + calorimetría) y `P26-3` (Teoría cinética con Avogadro `F5C3` + `1 L = 1000 cm³` `F14C2`), llevando el banco a **204 lecciones** con cobertura **42/42** casillas y desarrollos completos en las 4 casillas antes huérfanas.
   - Añadida la trampa de despeje inverso $r = (1/(1-e))^{1/(\gamma-1)}$ en `PR2-D9` y eliminadas las faltas heurísticas en `PR1-6` y `PR2-D7`.
3. **15 arquetipos paramétricos (`variantes.py`) y Ruta Estratégica (`rutas.py`):**
   - Añadidos `7` generadores nuevos (`conduccion_cono`, `conduccion_exponencial`, `conduccion_cilindro_capas`, `ciclo_recta_integral`, `esfuerzo_termico`, `otto_completo_inverso`, `calorimetria_fusion_trampa`).
   - Publicada la ruta `estrategia_ii_2026` (*Top 12 Estrategia II-2026 · Patrones + Fórmulas nuevas*).
4. **Simulacro Predictivo II-2026 (`simulacro.js`):**
   - Excluye por defecto Entropía, Carnot (`Q118`, `Q125`) y Van der Waals (`Q071..Q073`), ofrece el formato exacto de I-2026 (`4×2 pts + 3×3 pts = 17 pts` + `3` desarrollos sugeridos con enlace directo a la pizarra) y una casilla opt-in para incluir el histórico completo.


> **Corrección (26-nov-2026, verificación independiente):** las cifras de ΔV (4/17), ΔL (6/17) y Van der Waals (5/17) se corrigieron tras releer los 17 formularios; el detalle está en `docs/VERIFICACION_PATRONES_II_2026.md`.
