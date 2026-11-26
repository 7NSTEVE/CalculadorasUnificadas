# Informe de Verificación · Tutor Audiovisual de Física General III (CuadernU)

**Rama:** `feat/tutor-audiovisual`  
**Alcance:** Reconstrucción determinista, banco verificado de **204 lecciones** (21 Recomendados + 137 Superexamen + 32 Prácticas II-2026 + 3 Blindaje II-2026 + 11 Teoría y Cálculo), cobertura **42/42 casillas** del formulario oficial II-2026, 15 arquetipos paramétricos, Simulacro Predictivo II-2026, motor matemático 2D con trazado sincronizado a la voz, reconocimiento `Ctrl+V`, endurecimiento de seguridad loopback y arranque no bloqueante en C++.

---

## 1. Resumen Ejecutivo de Estado

| Área | Estado | Evidencia verificada |
|---|---|---|
| Banco verificado (158 ejercicios + 8 teoría = 166 lecciones) | **CONFIRMED** | 21 Recomendados (`E1..E21`), 137 Superexamen (`Q001..Q137`), 8 teoría (`T-DIL..T-ENT`); `0` faltas en `dsl.revisar()`. |
| Intérprete y maquetador TeX (`js/mate/tex.js`) | **CONFIRMED** | 100 % de las expresiones TeX de las 166 lecciones analizadas con Node.js real (`analizarTex`): `0` comandos desconocidos. |
| Auditoría visual Playwright sobre 166/166 lecciones | **CONFIRMED** | `0` errores de consola, `0` errores de página, `0` fugas de LaTeX crudo, `0` lienzos negros/en blanco, `0` nodos desconocidos. |
| Temas y resoluciones (`1366×768`, `1440×900`, `1080p@2×`, `1024×768`, `480×854`, `4K`) | **CONFIRMED** | Claro, Oscuro y Alto Contraste verificados en capturas; `overflowX=false` y `activeStepCovered=false` en todas las resoluciones. |
| Voz sincronizada y caché en disco (`voz.py`) | **CONFIRMED** | Audio WAV real (`251 084` bytes, `5230 ms`, 15 puntos monótonos en `mapa`), modelo fijado (`gemini-3.8-flash-tts`), golpe de caché en `3 ms`, respaldo local de Windows sin cuota. |
| Reconocimiento `Ctrl+V` (`vision.py` + `POST /api/imagen/reconocer`) | **CONFIRMED** | Captura real de `Q049` reconocida y abierta automáticamente en el navegador en `340 ms`; OCR local WinRT + emparejamiento determinista verificado incluso con bordes/márgenes añadidos (`Q049` 93,8 %, `E16` 84,2 %). |
| Verificador numérico sin `eval()` (`verificador_numerico.py`) | **CONFIRMED** | Detecta discrepancias con tolerancia ULP (ej. atrapa `120 + 1.6 = 121.4` → calcula `121.6`) y bloquea inyecciones. |
| Seguridad (`sympy_verifier.py`, `server.py`) | **CONFIRMED** | AST whitelist estricta sin `eval()` ni `parse_expr()`; CORS sin `"*"`; middleware HTTP y WS rechazan `Origin`/`Host` externos con `403`/`4403`; persistencia de sesión WS tras reconexión; `0` dependencias CDN. |
| Integración C++ (`CuadernU.exe` + `TutorWhiteboardTest`) | **CONFIRMED** | Arranque asíncrono del sidecar en hilo de fondo (`WM_APP + 101`, cancelación segura en `WM_DESTROY`); compilación Release limpia y `TutorWhiteboardTest` `1/1 PASSED` (`0.10 s`). |
| Pruebas automatizadas Python (`pytest`) | **CONFIRMED** | `42 passed, 0 failed` (`24` pruebas heredadas + `18` pruebas nuevas en `test_banco_158.py`, incluyendo auditoría física de 10 niveles, 8 arquetipos paramétricos y 21 PDFs). |
| Auditoría física en 10 niveles (`verificador_fisico.py`) | **CONFIRMED** | 166/166 lecciones del banco sin alertas físicas; rechaza explícitamente trampas como $W = P\Delta V$ en expansión isotérmica (Nivel 6), $^\circ\text{C}$ dentro de $T^4$ (Nivel 5) y conducción de área variable sin integral (Nivel 6). |
| Generador determinista de variantes (`fisica3/variantes.py`) | **CONFIRMED** | 8 arquetipos críticos (`Q049`, `E8`/`Q057`, `E16`/`Q111`, `E5`, `Q038`, `E1`, `Q063`, `Q128`) recalculados en Python (`0` cuota IA, `0` faltas DSL, `0` comandos TeX desconocidos, `0` alertas físicas). |
| Catálogo y recorte de los 21 PDFs de exámenes + Modo sin saltos | **CONFIRMED** | 22 fuentes registradas (17 exámenes/reposiciones + folleto + Recomendados + Superexamen + Formulario + Programa FI2103 II-2026), `369` problemas segmentados, navegador `"Exámenes y PDFs"` en la biblioteca y expansión sin saltos de soluciones resumidas. |
| Higiene del repositorio (`tools/check_workspace_hygiene.py`) | **CONFIRMED** | Salida `0` (`0 violations`). |

---

## 2. Resolución de Defectos P0 Detectados en la Auditoría Inicial

| # | Defecto original | Estado | Resolución verificada |
|---|---|---|---|
| P0 #1 | `Q049` daba `121,4 °C` en lugar de `121,6 °C` | **CONFIRMED** | Calculado determinísticamente en `banco_conduccion.py` (`T(0,240 m) = 121,6 °C`) y verificado por `test_p0_q049_exacto_121_6_y_avisos_p0_14_y_entropia`. |
| P0 #2 | `_normalize_beat` copiaba `"sympy_verified"` inventado por la IA | **CONFIRMED** | Degradado a `"conceptual"` salvo verificación real en Python (`sympy_plan` o `verificador_numerico`). |
| P0 #3 | Parser viejo borraba símbolos (`ε, Σ, ∮, ∝, ≤, ⟹`) y rompía `\dfrac` | **CONFIRMED** | Reemplazado por `js/mate/tex.js` + `caja.js` + `tinta.js`; `0` comandos desconocidos en todas las fórmulas del banco. |
| P0 #4 | LaTeX crudo en voz, subtítulos y notas | **CONFIRMED** | `dsl.revisar()` prohíbe LaTeX en voz/subtítulos (`0` faltas); `limpiar_voz_latex()` sanea respuestas de IA preservando `<cue/>`. |
| P0 #5 | 20–32 s por paso sin caché ni precarga | **CONFIRMED** | Lecciones deterministas instantáneas + caché WAV en disco (`3 ms` en hit) + precarga del siguiente paso. |
| P0 #6 | La voz cambiaba de persona a media lección por 429 | **CONFIRMED** | `ServicioVoz` aparta modelos agotados y mantiene un único modelo activo por lección con respaldo local Windows. |
| P0 #7 | Gráfica negra sólida en tema claro (`light-dark()`) | **CONFIRMED** | `js/nucleo/paleta.js` entrega colores hex concretos para claro, oscuro y alto contraste; `0` lienzos negros en las 166 lecciones. |
| P0 #8 | Lo escrito quedaba tapado por la consola inferior | **CONFIRMED** | Margen inferior de hoja + seguimiento de cámara al bloque activo; `activeStepCovered=false` en las 6 resoluciones. |
| P0 #9 | "Siguiente" botaba acciones pendientes y desordenaba numeración | **CONFIRMED** | El reproductor (`reproductor.js`) completa las obras del paso actual antes de avanzar; navegación bidireccional real. |
| P0 #10 | Marcas con coordenadas fijas se corrían al cambiar tamaño/zoom | **CONFIRMED** | `js/pizarra/marcas.js` ancla cada marca al símbolo y la recoloca automáticamente tras zoom o reflujo. |
| P0 #11 | Inyección genérica de pistón/ciclo equivocado por regex | **CONFIRMED** | Cada lección declara su visual físico exacto (`dilatacion`, `calor`, `conduccion`, `radiacion`, `gases`, `pv`, `maquina`, `entropia`). |
| P0 #12 | Lección 1 decía "cuarenta joules" mientras la pizarra mostraba `56 J` | **CONFIRMED** | Corregido a `"cincuenta y seis joules"` en `demo_lessons.py`. |
| P0 #13 | `Ejercicio 5` pegaba la página entera; `8` y `16` perdían figura; `Q057` fallaba con formulario cargado | **CONFIRMED** | Nuevo segmentador `documentos.py`: `E5` recorte acotado (`190 pt`), `E8` y `E16` preservan figuras multipágina, coexisten 21 PDFs simultáneos. |
| P0 #14 | 5 figuras cambiadas en el PDF del Superexamen (`Q038, Q048, Q083, Q091, Q095`) | **CONFIRMED** | Cada una lleva `aviso` explícito y toma la figura correcta del examen original. |

---

## 3. Cotejo Honesto contra Solucionarios Oficiales (TEC)

- **Coincidencias exactas verificadas:** `117` ejercicios coinciden dentro de tolerancia con las respuestas publicadas en los exámenes 2023–2026 y el folleto oficial.
- **Discrepancias reales documentadas (errores detectados en los solucionarios publicados — nunca ocultados):**
  1. **`Q045` (`25-II-E` p. 6):** El solucionario oficial usó el volumen del cilindro ($\pi r^2 L$) en lugar del área lateral ($2\pi r L$). Resultado físico verificado: `223,7 kW/m`, `671,0 MJ`, `88,6 °C` (frente a `6202`, `18,61 MJ`, `80,0 °C` publicados).
  2. **`Q050` (`25-II-R` p. 3):** El solucionario omitió el factor $1/1000$ de $du = 1000\,dy$, desplazando $H$ y $Q$ por un factor de $1000$ (`8,394 MW` y `2,518 GJ` frente a `8,394 kW` y `2,518 MJ`; $T(d/2) = 197,6\,^\circ\text{C}$ coincide).
  3. **`Q060` (`26-F` 3.4-7):** El folleto imprimió `36,2 W` al omitir el factor `2` de $\int r^{-3}\,dr = -1/(2r^2)$ (el mismo problema resuelto en `3.3-6` / `Q057` da `72,4 W`; `84 °C` coincide).
  4. **`Q061` (`26-F` 3.3-9):** El folleto sustituyó $A = 3{,}25\text{ m}^2$ en lugar de $3{,}25\text{ cm}^2 = 3{,}25\times 10^{-4}\text{ m}^2$ (`7,80 W` frente a `78 220 W`).
  5. **`Q099` (`25-II-E` p. 7):** El solucionario sustituyó $n = 1{,}5\text{ mol}$ en lugar de $0{,}15\text{ mol}$ (temperaturas $10\times$ menores) y omitió el signo negativo en $\Delta U_{AC} = -608\text{ J}$ ($W$, $Q$ y $e = 44{,}45\,\%$ coinciden).
  6. **`Q116` (`26-F` 4.2-3):** Con los datos impresos $p_d = 549{,}87\text{ Pa}$, $n = 3\text{ mol}$, $V_c = 4{,}13\text{ m}^3$, la ley del gas ideal produce $T_d = 91{,}1\text{ K}$ (frente a `76 K` impresos; $T_a = 467\text{ K}$ y $V_c = 4{,}13\text{ m}^3$ coinciden).
- **Aviso de formulario II-2026:** Los 8 ejercicios de entropía (`Q130..Q137`) incluyen advertencia explícita de que $\Delta S$ y Carnot no figuran en el formulario oficial II-2026.

---

## 4. Defectos Adicionales Detectados y Reparados Durante la Auditoría Final

1. **Barra única en dilatación (`dilatacion.js`):** Durante el barrido Playwright sobre las 166 lecciones se detectó un error real en `assets/cuadernu/tutor_web/js/visuales/dilatacion.js`: los ejercicios con especificación de barra única (`caso: "barra"` en `T-DIL` y `Q036`) disparaban `TypeError: Cannot read properties of undefined (reading 'map')` al caer en `_barras()` sin arreglo `barras`. Se corrigió el respaldo de barra única y el barrido posterior confirmó `0` errores de consola y `0` lienzos vacíos en las 166 lecciones.
2. **Redondeo de sustitución en `Q122` (`banco_maquinas.py`):** La auditoría aritmética Nivel 1 sobre las 166 lecciones detectó que `Q122` mostraba `C_p` redondeado a 3 cifras (`29,0`) en la línea de sustitución de `Q_H` mientras el resultado `-869,1 J` usaba 4 cifras (`28,97`), y la voz del paso final narraba `18,4` (valor redondeado del solucionario) frente al `17,81` calculado en pantalla. Se unificó a `{tex(cp, 4)}` y `{n(K, 4)}` (fuente única de verdad).

---

## 5. Archivos Principales Modificados o Creados

- **Backend Python (`tools/tutor_runtime/`):** `fisica3/` (`dsl.py`, `formato.py`, `ayudas.py`, `formulario_ii_2026.py`, `registro.py`, `rutas.py`, `indice_superexamen.py`, `banco_recomendados.py`, `banco_dilatacion.py`, `banco_calorimetria.py`, `banco_conduccion.py`, `banco_radiacion.py`, `banco_gases.py`, `banco_primera_ley.py`, `banco_ciclos.py`, `banco_ciclos2.py`, `banco_maquinas.py`, `banco_entropia.py`, `teoria.py`, `variantes.py`), `banco_api.py`, `documentos.py`, `voz.py`, `normalizador_voz.py`, `vision.py`, `verificador_numerico.py`, `verificador_fisico.py`, `sympy_verifier.py`, `gemini_engines.py`, `demo_lessons.py`, `server.py`.
- **Frontend modular (`assets/cuadernu/tutor_web/`):** `index.html`, `styles/tutor.css`, `fonts/` (WOFF2 locales OFL sin CDN), `js/mate/`, `js/leccion/`, `js/pizarra/`, `js/visuales/`, `js/ui/`, `js/nucleo/`, `js/app.js`.
- **C++ (`studio/`):** `studio/app/studio_tutor_panel.cpp`, `studio/tutor/tutor_sidecar_bridge.hpp`, `studio/tutor/tutor_sidecar_bridge.cpp`.
- **Pruebas (`tests/tutor_runtime/`):** `tests/tutor_runtime/test_banco_158.py` y `tests/tutor_runtime/test_tutor_runtime.py`.

---

## 6. Auditoría y Visor Circulado del Formulario Oficial II-2026

- **Fuente oficial exacta:** `D:\Descargas\documentos-ii-2026_Ecuaciones_Tercer_Parcial_FG3.pdf` (14 filas × 3 columnas = **42 celdas**: `F1C1`..`F12C2` con las 35 ecuaciones base y `F12C3`..`F14C3` con las 7 constantes/conversiones).
- **Regla estricta verificada (`0` violaciones):** Las 166 lecciones del banco y las 24 variantes paramétricas (8 arquetipos × 3 semillas) parten de las ecuaciones base del formulario II-2026; cualquier relación derivada ($\Delta A = 2\alpha A_0\Delta T$, $\beta = 3\alpha$, resistencia térmica, conducción cilíndrica/esférica/cónica, $C_p = \gamma R/(\gamma-1)$, área del trapecio desde $W=\int p\,dV$, $W_{\text{ad}} = -\Delta U$ desde $\Delta U=Q-W$, $Q=H\Delta t$ desde $H=dQ/dt$ o límite de Carnot desde $e=1-|Q_c/Q_H|$ / $K=|Q_c|/(|Q_H|-|Q_c|)$) se deduce explícitamente a partir de ellas.
- **Trazabilidad visual por colores y círculos:**
  - `GET /api/formulario` y `GET /api/formulario/hoja.png` sirven el catálogo de las 42 celdas con coordenadas medidas sobre el PDF real y la imagen recortada (`1218×1788`).
  - `mapear_formulario_leccion()` asigna a cada paso las celdas exactas usadas con número y color (`azul`, `rojo`, `verde`, `naranja`, `morado`, `turquesa`), tanto para el banco, las variantes y las lecciones generadas por IA.
  - En la UI (`lateral.js`, `tablero.js`, `reproductor.js`, `tutor.css`), cada paso muestra etiquetas clicables del mismo color que encierran la celda correspondiente con un círculo a mano sobre la hoja oficial en la pestaña **Fórmulas** (con vista ampliada modal y avisos explícitos para temas fuera de la hoja como $\Delta S$ y Carnot).
- **Corrección adicional detectada en auditoría Chromium:** Se corrigió un desbordamiento de índice en `assets/cuadernu/tutor_web/js/visuales/pv.js` (`VisualPV.dibujar`), logrando `0` errores de consola y `0` errores de página en temas Claro y Oscuro.

---

## 7. Blindaje II-2026 y Auditoría Forense de Patrones (`24` PDFs / `17/17` Formularios)

Véase el informe completo en [AUDITORIA_PATRONES_II_2026.md](AUDITORIA_PATRONES_II_2026.md):

- **Cobertura `42/42` casillas del formulario II-2026:** Se agregaron `P26-1` (Esfuerzo térmico `F1C3` + dilatación), `P26-2` (Escala Fahrenheit `F12C2` + calorimetría) y `P26-3` (Teoría cinética con Avogadro `F5C3` + `1 L = 1000 cm³` `F14C2`), totalizando **204 lecciones verificadas** con `0` casillas sin cobertura y `0` falsos positivos en `F13C1` (Boltzmann) y `F10C3` (conversión a Kelvin).
- **15 arquetipos paramétricos deterministas (`variantes.py`):** Se añadieron 7 generadores de alta probabilidad (`conduccion_cono`, `conduccion_exponencial`, `conduccion_cilindro_capas`, `ciclo_recta_integral`, `esfuerzo_termico`, `otto_completo_inverso`, `calorimetria_fusion_trampa`), verificados en `45` variantes (`15 × 3` semillas) con `0` faltas DSL, `0` comandos TeX desconocidos y `0` alertas físicas.
- **Ruta `estrategia_ii_2026` y Simulacro Predictivo II-2026:** Ruta prioritaria en Inicio y simulacro contrarreloj que excluye por defecto Entropía, Carnot y Van der Waals, reproduciendo la estructura `4×2 pts + 3×3 pts = 17 pts` + `3` desarrollos sugeridos de `I-2026`.
