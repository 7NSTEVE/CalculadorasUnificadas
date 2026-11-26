# Verificación independiente de `Patrones.txt` y del plan de auditoría · III Parcial FI2103 II-2026

**Fecha:** 26 de noviembre de 2026 (según el programa y el cronograma oficial, **hoy es el Tercer Parcial Ordinario**, hora por definir).
**Método:** extracción de texto de los 24 PDFs con PyMuPDF, lectura completa de los 17 exámenes/reposiciones,
las dos prácticas II-2026, Recomendados 3, programa y cronograma; inspección visual del formulario II-2026
(es imagen, 0 caracteres de texto) y del formulario del II parcial II-2026; recálculo a mano / Python de los
resultados numéricos clave. El folleto (138 págs.) se revisó **solo en la parte de calor y termodinámica** y solo
en los puntos señalados; no se leyó completo.

Fuentes que no aportan: `Patrones Gemini.txt` está **vacío (0 bytes)**. `banco-de-examenes_I-2025_..._extra_solución (1).pdf`
es idéntico byte a byte al archivo sin “(1)” (SHA-256 `c7216c6af577…`).

---

## 1. Lo esencial (para estudiar hoy)

1. **El examen cubre todo calor + termodinámica hasta la sección 20.5 de Sears.** El formulario del II parcial II-2026
   fue solo elasticidad y fluidos, así que dilatación, calorimetría, conducción, radiación, gases, primera ley,
   máquinas, refrigeradores y Otto entran en el III parcial. Carnot (20.6) y entropía (20.7) no están en la hoja.
2. **Desarrollo casi seguro:** un **ciclo** (17/17 exámenes históricos tuvieron un problema de procesos/ciclo) y
   **conducción** (16/17; con integración en 7 de los 10 parciales I-2024..I-2026).
3. **Formato más probable (molde I-2026, confirmado en ordinario y extraordinario):**
   Parte I 4×2 pts conceptuales (1 dilatación · 2 calorimetría · 3 proceso isotérmico/adiabático · 4 segunda ley),
   Parte II 3×3 pts de cálculo corto, Parte III 2 desarrollos de 8–9 pts, Parte IV 1 desarrollo de 16–17 pts.
   En I-2026 la dilatación salió **en desarrollo en los dos exámenes** (8 pts cada vez).
4. **Puntos que se pagan y se pierden fácil:** dibujar el diagrama p-V (3–4 pts en 26-I-E, 26-I-R, 25-II-O, 25-II-R);
   decidir motor vs. refrigerador por el **sentido de giro** (antihorario ⇒ refrigerador, se pide *K*, no *e*);
   comprobar si **todo el hielo se funde** antes de suponerlo; en conducción cilíndrica usar **A = 2πrL**.

---

## 2. Verificación de `Patrones.txt`, afirmación por afirmación

### 2.1 Primera respuesta (formularios ↔ preguntas)

| Afirmación | Veredicto | Evidencia |
|---|---|---|
| Formulario II-2026 = 42 casillas, 14 filas × 3 | **Correcto** | Inspección visual del PDF |
| Esfuerzo térmico `F/A = −YαΔT` nuevo (0/10) | **Correcto**, y 0/17 | Ningún formulario 23-II..26-I lo trae. Pero `Y = (F⊥/A)/(Δl/l₀)` sí estaba en las reposiciones 24-I-R y 26-I-R y en la hoja del II parcial II-2026: la fórmula se puede “armar” con elasticidad + dilatación. |
| ΔV = βV₀ΔT nueva (0/10) | **Correcto en su muestra, engañoso en la serie** | Estaba en **4/17**: 23-II-O, 23-II-E, 23-II-R y **24-I-R** (el plan y el doc del repo dicen 3/17; falta 24-I-R). |
| Dilatación lineal 1/10 | **Correcto** (26-I-E) | En la serie completa es **6/17** (23-II ×3, 24-I-R, 26-I-E, 26-I-R); el repo dice 5/17. |
| Fahrenheit nueva (0/10) | **Correcto**, 0/17 | El programa sí la lista (4.3); el folleto no la usa nunca. |
| Fourier diferencial 10/10, radiación 10/10, calorimetría 10/10, gas ideal 10/10 | **Correcto** | — |
| Otto 2/10 | **Correcto** (26-I-O, 26-I-E; también 26-I-R) | Ninguno de esos exámenes preguntó Otto: confirmado. |
| Carnot 10/10 eliminado; entropía 8/10 eliminada; VdW 4/10 ausente | **Correcto** | Entropía faltaba ya en 25-II-O/E. VdW en formulario: 5/17 (24-II ×3, 25-I ×2); el repo dice 6/17. |
| Tabla “examen → problema de desarrollo” (10 filas) | **Correcto en las 10 filas** | Verificado una por una. |
| Ciclos 10/10, conducción 9/10, integración 7/10, dilatación 2/10, calorimetría 1/10, radiación 0/10 | **Correcto** | Matiz: la “calorimetría” de 24-I-E es en realidad un problema de **entropía** de mezcla. |
| “Otto específico 0/10” | **Correcto pero incompleto** | 24-II-E tuvo un **ciclo Diesel** de 18 pts. Otto y Diesel son la misma sección (20.3, motores de combustión interna) ⇒ “motor de combustión interna en desarrollo: 1/10”. |
| “Sospecha particular: podrían cambiar A(x) por k(x)” | **Subestimado: ya pasó** | **25-II-R P3**: `k(y) = 200 + 1000y` (15 pts). Además Recomendados 3 **E8** (`k = k₀/r`) y el folleto 3.3-9 / propuesto con k(r). Patrones excluyó las reposiciones y por eso no lo vio. |
| Estructura 8 + 9 + 16 + 17 = 50 | **Correcto** | Coincide con 26-I-O (III = 9+8, IV = 16) y 26-I-E (III = 8+8, IV = 17). |

### 2.2 Segunda respuesta (las dos prácticas)

| Afirmación | Veredicto | Evidencia |
|---|---|---|
| Profesor: 15 págs., 14 selección + 12 desarrollos; Marcela: 5 págs., 6 desarrollos, “100 pts” | **Correcto** | Pero los incisos de Marcela suman **95**: P2 dice 15 y suma 12; P5 dice 20 y suma 18. |
| Reparto 5 termo / 4 conducción / 3 calorimetría (profesor) y 2/2/2 (Marcela) | **Correcto** | — |
| Dilatación, esfuerzo, radiación solo en selección del profesor | **Correcto** | S3, S13, S10 (y S1 Fahrenheit: es la trampa de **ΔT**, 1800 J/(kg·K) = 1000 J/(kg·°F)). |
| “Dibujar gráfica P–V: profesor P4, P6, P9, P11” | **Incorrecto** | P4, P6 y P9 **traen** el diagrama impreso; P11 no tiene figura ni pide dibujarla. El profesor no entrena el dibujo; Marcela sí (P5, P6). En los exámenes el dibujo vale 3–4 pts. |
| Cono truncado en tres documentos (24-II-E P6, profesor P1, Marcela P4) | **Correcto** | Fórmula verificada: `H = πk r₁r₂ ΔT / L` (24-II-E: 353,4 W ✓; Marcela P4: 96,6 W, 1,04 kg/h de hielo). |
| “25-II-E P6 tubería ↔ profesor P2 y P10” | **Débil** | P2 y P10 son barras con A(x) lineal y exponencial, no tuberías. El paralelo real del profesor P2 es **24-I-O P7B**: `A(x) = 0,01(1 + x/2)` m², la misma forma `A₀(1 + x/L)`. |
| Error en la solución oficial 25-II-E P6 (`A = πr²L`) | **Correcto** | Con `2πrL`: H/L ≈ 223,7 kW/m (no 6202 W/m) y T(6 cm) ≈ 88,6 °C (no 80,0 °C). |
| Ciclos con recta inclinada (profesor P4, Marcela P5) como “novedad” | **Ya tiene historia** | 23-II-O P6 (recta + isoterma) y 25-II-E P7 (C→A recta, área de trapecio). |
| Calorimetría: trampa de “no todo el hielo se funde” | **Correcto** | Profesor D7: 48 600 J < 66 800 J ⇒ queda hielo, T_f = 0 °C. Marcela P2: 94 000 J < 106 984 J ⇒ se funde todo, T_f = 10,7 °C. Profesor D12: se funde todo, T_f ≈ 6,5 °C (no llega a ebullición). |
| Marcela P6 | **Detalle omitido** | Ciclo **antihorario**: W_neto ≈ −1,87 kJ (consume trabajo). |
| Entropía/Carnot ausentes de las prácticas | **Correcto**, con una excepción | Recomendados 3 **E17c** pide la “eficiencia máxima posible” (Carnot sin nombrarlo). |

---

## 3. Hallazgos nuevos (ni `Patrones.txt` ni el plan los mencionan)

1. **La solución oficial de 24-II-O P7 concluye mal (“el ciclo es imposible”).** El ciclo es isobárico + adiabático +
   isocórico de un gas ideal, totalmente reversible: ∮dQ/T = nC_p ln(300/400) + nC_V ln(400/267,2) = **0,000 J/K**
   (calculado). La comparación K = 17,8 > K_Carnot = 2,01 no aplica porque el gas absorbe calor hasta 400 K y lo cede
   entre 400 y 300 K: no hay dos focos separados. **El tutor (`banco_maquinas.py`, lección Q122) repite el error**
   (“viola la 2.ª ley”). Importa porque “¿es posible esta máquina?” es una pregunta conceptual probable.
2. **La cátedra recicla exámenes en los Recomendados:** Recomendados 3 E15 es el mismo enunciado que la reposición
   II-2023 P3 (“dos moles de gas diatómico con cinco grados de libertad…”).
3. **Conductividad variable k(x)/k(r) ya evaluada** (25-II-R P3) y practicada (Recomendados E8, folleto).
4. **Erratas menores adicionales en solucionarios** (no cambian el método):
   26-I-O P9B signo (`(L₀−ΔL)` en vez de `(L₀+ΔL)`); 26-I-O P10 escribe 0,1464 por 0,14464 y “41,125” por 41125;
   25-I-O P6 usa 32,28 L por 32,8 L (W_neto 3024 J, debería rondar 2980 J); 25-I-E rotula “pregunta 7” la 6 y
   muestra R(x) con 0,02 en vez de 0,012; 24-II-E Diesel imprime 7/2 donde calcula con 5/2 y escribe V_D = V_C;
   folleto 3.1-3 usa α_acero = 1,2×10⁻⁵ siendo el dato 1,1×10⁻⁵; folleto 3.1-4 escribe los límites ∫_T^{2T} en el
   tramo [2T, 3T]; 25-II-R P3 además da la energía en MJ cuando son GJ (arrastra el factor 1000).
5. **Erratas del plan que sí pude confirmar:** folleto 3.1-4 (2β por 3β) ✓, folleto 3.2-1 (−88,9 J por −88,9 kJ; y la
   temperatura inicial correcta es ≈826 °C, no 846 °C) ✓, folleto 4.3-3 (T_d = 91,1 K con los datos, no 76 K) ✓,
   25-II-E P7 (usa n = 1,5 en vez de 0,15: solo las temperaturas salen ×10; los calores sí están bien porque nΔT = Δ(pV)/R) ✓,
   25-II-R P3 (falta 1/1000) ✓. **No verifiqué** las del folleto 3.3-9 y 3.4-7.
6. **26-I-E P10 pide “trabajo sobre el gas”** pero la solución reporta el trabajo hecho por el gas (convención de signo).

---

## 4. Estado del tutor frente a esto

- Ya corregidos en el banco (verificado en código): 25-II-E P6 (2πrL, 223,7 kW/m, 88,6 °C), 25-II-R P3 (MW),
  folleto 3.2 (826,1 °C).
- **Q122 corregida** (26-nov): ahora explica por qué el ciclo **sí es posible** y avisa del error del solucionario.
- **Cifras corregidas** en `AUDITORIA_PATRONES_II_2026.md`: ΔV 4/17, ΔL 6/17, VdW 5/17.

---

## 5. Lo que se cambió en el tutor (madrugada del 26-nov)

Pedido: cada solución paso a paso, sin saltos, desde las casillas del formulario II-2026, con lo que no está en la
hoja demostrado por despeje, y explicando derivadas e integrales para alguien que no sabe cálculo.

| Pieza | Qué hace | Alcance medido |
|---|---|---|
| `fisica3/desglose.py` | Cada cuenta con varias operaciones se escribe de adentro hacia afuera, **un renglón por operación**, con su frase hablada («Dividimos 15 396 entre 235: da 65,51»). Ajusta las cifras cuando hace falta (por ejemplo, logaritmos de números cercanos a 1). Parte los pasos que quedan largos. | ~400 cuentas en ~100 lecciones; la prueba confirma que ya no queda ninguna cuenta desglosable |
| `fisica3/despeje.py` | Despejes de dos o más operaciones en renglones separados («multiplicamos los dos lados por…», «pasamos sumando…»). Solo inserta pasos si llega **exactamente** al mismo resultado que escribió el autor. | 22 despejes en 18 lecciones (los demás ya eran de una sola operación) |
| `fisica3/calculo.py` | La primera vez que una lección usa una regla de cálculo, la explica ahí mismo (qué es dT/dx, de dónde sale du = b·dx, ∫du/u = ln u, la potencia, «arriba menos abajo», resta de logaritmos, exponencial, trabajo como área). | ~100 explicaciones en 37 lecciones |
| `fisica3/conceptos.py` | 23 preguntas conceptuales razonadas **desde casillas de la hoja**, paso a paso, antes de revisar las opciones. | 23 lecciones |
| Lección **T-CONO** | El tronco de cono desde cero: qué es, el radio como rampa (tabla y gráfica), el área de cada rebanada, por qué el mismo H cruza todo, la receta de 6 pasos, la integral con cambio de variable sin saltos, truco de memoria (r·r → r₁·r₂) y los 6 renglones del examen. | Nueva; enlazada desde PR1-4, PR2-D1, Q041 y Q043 |
| PR1-4 | La integral del cono con su cambio de variable completo (antes aplicaba la regla directo). | — |
| **P26-4, P26-5** | Problemas nuevos de radiación (lo que anunció el profesor): radiación neta + enfriamiento; despejar T con raíz cuarta + fundir hielo. Resultados verificados con cálculo independiente. | 2 lecciones |
| Ruta «Lo que dijo el profe» | Primera en el inicio: calorimetría, radiación y ciclos, del más directo al más largo. | 26 lecciones |
| Buscador | Escribir un id exacto (T-CONO, PR1-4, P26-4) abre la lección. | — |

**Verificación:** 62 pruebas pasan (`tests/tutor_runtime`, incluidas 15 nuevas en `test_paso_a_paso.py`); el
analizador TeX real (Node) no encuentra comandos desconocidos; auditoría física sin alertas. T-CONO y P26-5 se
revisaron en el navegador (pasos, gráficas y raíz cuarta) sin errores de consola.

**Lo que no se hizo o queda pendiente:**
- El auditor heurístico todavía marca 15 lecciones, **todas fuera del corte** (entropía Q130–Q137, Q091, Q132, Q133;
  Carnot Q125, Q127, Q118; Van der Waals Q071–Q073).
- No se revisó a ojo cada una de las 207 lecciones: los motores son automáticos y conservadores; donde no pueden leer
  un renglón, lo dejan como estaba.
- El tutor que estaba abierto (puerto 8765) sigue con el código anterior hasta reiniciarlo.

---

## 6. Folleto (138 págs.): auditoría de los capítulos del III parcial

- Estructura: cap. 1 MAS y ondas (págs. 5–48), cap. 2 fluidos (49–90), **cap. 3 calor (91–120)**, **cap. 4 termodinámica (121–138)**.
  Los capítulos 1 y 2 son materia del I y II parcial: se inventariaron, **no se auditaron numéricamente**.
- Capítulos 3 y 4: los **37 problemas** (resueltos y propuestos) tienen lección con cálculo independiente en el tutor.
  33 coinciden o no publican número; **3 difieren por errata del folleto**: 3.3-9 (área en m² en vez de cm², error de 10⁴),
  3.4-7 (falta el factor 2 de ∫r⁻³dr: 36,2 W en vez de 72,4 W), 4.2-3 (T_d = 91,1 K, no 76 K). Erratas menores
  documentadas en la lección: 3.1-3 (α del acero 1,2 por 1,1), 3.1-4 (límites y 2β por 3β), 3.2-1 (J por kJ; 826 °C),
  3.2-3 (26 890,5 por 26 850,5 J), 3.3-5 (238,0 por 73 en la sustitución). Detalle por problema: cotejo de cada lección.
- Entropía (4.1-4, 4.1-5, 4.2-4…4.2-6): por indicación del usuario no entra en el examen y no se amplió.
