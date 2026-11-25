# Manual de Usuario — CuadernU (Cuaderno Digital, Lienzo Infinito y PDF Científico)

**CuadernU** (`CuadernU.exe`, con alias de compatibilidad `DocumentStudio.exe`) es el entorno de documentos técnicos, cuaderno digital de baja latencia, visor/editor nativo de PDF, motor de migración multiformato y laboratorio interactivo de ingeniería integrado con **Calculadora Unificada** en Windows.

---

## 1. Modos de Trabajo

| Modo | Descripción y Uso |
| :--- | :--- |
| **PDF Paginado (`PDF Pág.`)** | Visualización, anotación vectorial, recorte no destructivo, rotación, inserción/reordenamiento de páginas, búsqueda Unicode, OCR offline y firma digital PKCS#7 SHA-256 sobre documentos PDF de cualquier extensión. |
| **Lienzo Infinito (`Lienzo ∞`)** | Espacio bidimensional ilimitado indexado por *Quadtree* con zoom y desplazamiento continuos, minimapa espacial, fondos técnicos configurables (liso, rayado, cuadrícula, punteado, milimetrado) y conexión directa de tarjetas de cálculo, circuitos y gráficas. |
| **Modo Concentración (Foco)** | Oculta paneles laterales para maximizar el área activa de lectura, escritura o resolución. |
| **Modo Presentador + Puntero Láser** | Activa el puntero láser temporal con estela luminosa sin alterar el documento ni mostrar marcos de selección. |

---

## 2. Escritura Digital, Geometría y Edición Selectiva

- **Pluma y Resaltador con Física de Tinta**:
  - Soporte de presión e inclinación de lápiz óptico (muestreo coalescente de alta frecuencia en Windows Pointer API y ejes XInput2/GDK en Linux) con predicción cinemática de sub-fotograma y tres niveles de estabilización (`15%`, `45%`, `85%`).
- **Hold-to-Snap (`Snap`)**:
  - Reconocimiento geométrico determinista de líneas rectas, flechas direccionales, rectángulos y elipses/círculos.
- **Regla Graduada Interactiva (`0° – 360°`)**:
  - Superposición milimetrada con ajuste magnético del trazo al borde activo y rotación angular en pasos de `15°`.
- **Gestos Inteligentes de Tinta**:
  - *Scribble-to-Erase*: tachado en zigzag sobre un trazo para eliminarlo automáticamente respetando objetos bloqueados o tarjetas padre.
  - *Lazo Cerrado (`Lazo`)*: selección simultánea de múltiples trazos, notas y bloques técnicos.
  - *Tinta Anclada*: vincula anotaciones manuscritas a una tarjeta PDF, imagen o bloque STEM para que se desplacen solidariamente.
- **Borrador Selectivo**:
  - Permite alternar entre borrar **Todo**, **Solo Resaltador** (conservando apuntes de pluma y fórmulas intactos) o **Solo Tinta**, además de limpiar todos los resaltados de la vista activa en un clic.

---

## 3. Motor Matemático Dimensional y Modos `#varsub`

El menú **`+STEM ▾`** incorpora un motor determinista de evaluación simbólico-numérica con seguimiento estricto de las 7 dimensiones fundamentales del Sistema Internacional (`kg, m, s, A, K, mol, cd`) y unidades imperiales/técnicas.

### 3.1 Sintaxis de Fórmulas, Unidades y Conversión (`|`)

Ejemplos de expresiones válidas:
- Ley de Ohm y potencia eléctrica:
  - `V_out = 4.7 kOhm * 2 mA | V` → `9.4 V`
  - `P = 12 V * 250 mA | mW` → `3000 mW`
- Presión hidrostática y neumática mixta:
  - `P_tot = 3 bar + 100 psi` → `9.89476 bar`
  - `P_tot_psi = 3 bar + 100 psi | psi` → `143.511 psi`
- Áreas, fuerzas estructurales y momentos:
  - `A = 0.5 m^2 | cm^2` → `5000 cm^2` (`| ft^2` → `5.38196 ft^2`)
  - `F = 10 kip`, `L = 10 ft`, `M = F * L` → `100 kip*ft`
- Fasores en corriente alterna (`r @ θ` o `r ∠ θ°`):
  - `I = (120 @ 0 V) / (30 @ 36.87 Ohm)` → `4∠-36.87° A`

> Si se intenta sumar o igualar magnitudes dimensionalmente incompatibles (por ejemplo `1 Nm + 1 m` o `4.7 kOhm + 2 mA`), el motor rechaza la operación e informa el conflicto dimensional exacto.

### 3.2 Modos de Visualización de Ingeniería
Cada bloque matemático puede alternar entre tres modos de presentación:
1. **`#varsub` (Predeterminado)**: muestra la fórmula simbólica, la sustitución explícita de cada variable con sus unidades y el resultado final verificado.
2. **`#nosub`**: muestra únicamente la fórmula simbólica y el resultado final.
3. **`#novar`**: muestra únicamente el resultado numérico con su unidad.

### 3.3 Sliders de Parámetros, Graficación 2D/3D y Álgebra Matricial
- **Grafo Reactivo (DAG) y Sliders**: al modificar cualquier variable o deslizador de parámetro (`k`, `R_load`, `f`), todas las fórmulas dependientes y gráficas `2D`/`3D` del documento se recalculan en orden topológico.
- **Verificador de Pasos Algebraicos**: comprueba equivalencia simbólico-numérica y consistencia dimensional entre dos pasos consecutivos de una deducción.
- **Tarjeta de Confirmación OCR → Fórmula**: permite revisar y editar texto matemático reconocido por OCR antes de insertarlo como nodo ejecutable, preservando el contenido original si se cancela.
- **Matrices y Sistemas Lineales (SEL)**:
  - Sistemas lineales: `2*x + y = 7; x - y = 2` → `x = 3, y = 1`.
  - Operaciones matriciales: `det([[2,1],[1,3]])`, `inv([[2,1],[1,3]])`, `tr(...)`, `transpose(...)` y producto matricial `[[1,2],[3,4]] * [[5],[6]]`.
- **Gráficas 2D, Regresión CSV, Diagramas Fasoriales y Superficies 3D Accesibles (`CVD`)**:
  - Curvas `y = f(x)` y regresión polinomial por mínimos cuadrados (`OLS`) con coeficiente $R^2$ a partir de pares `x, y`.
  - **Generador de Diagramas Fasoriales**: convierte cada fasor simultáneamente a forma rectangular ($a + jb$), polar ($r\angle\theta^\circ$) y exponencial ($r\cdot e^{j\theta}$), trazando vectores a escala sobre anillos polares de referencia.
  - **Inspector Físico de Variables**: identifica la magnitud física (tensión, corriente, impedancia, presión, fuerza, potencia, energía, frecuencia, capacitancia, inductancia), sus unidades fundamentales SI (`kg, m, s, A, K, mol, cd`), su valor actual y el nodo exacto de origen.
  - Superficies tridimensionales `z = f(x, y)` (ej. `sin(x^2 + y^2)`) renderizadas con paleta perceptual apta para daltonismo (*Color Vision Deficiency*).

---

## 4. Simulador de Circuitos MNA, Osciloscopio Transitorio y Laplace (`s`)

CuadernU resuelve netlists tipo SPICE mediante **Análisis Nodal Modificado (MNA)** tanto en régimen permanente como en el dominio del tiempo y de Laplace.

### 4.1 Componentes Soportados (Nodo `0` = Tierra / Referencia)

| Prefijo | Sintaxis | Descripción |
| :--- | :--- | :--- |
| `R` | `R1 <n+> <n-> <valor>` | Resistor (ej. `R1 1 2 4.7k`) |
| `C` | `C1 <n+> <n-> <valor>` | Capacitor (ej. `C1 2 0 2.2u`) |
| `L` | `L1 <n+> <n-> <valor>` | Inductor (ej. `L1 1 2 10m`) |
| `V` | `V1 <n+> <n-> <mag> [fase]` | Fuente independiente de tensión DC/AC |
| `I` | `I1 <n+> <n-> <mag> [fase]` | Fuente independiente de corriente DC/AC |
| `OPAMP` | `OPAMP U1 <in+> <in-> <out>` | Amplificador operacional ideal |
| `D` | `D1 <ánodo> <cátodo> [Is]` | Diodo no lineal de Shockley (Newton-Raphson con amortiguamiento `pnjlim`) |

### 4.2 Modos de Análisis y Exportación CAD
- **Punto de Operación DC y Fasores AC**: tensiones nodales complejas, corrientes de rama, balance de potencia activa y ecuaciones KCL simbólicas.
- **Simulación Transitoria + Osciloscopio Virtual**: integración temporal con lectura directa de multímetro virtual (`Vmax`, `Vmin`, `Vrms`, `Vdc`) y tarjeta gráfica de forma de onda `V(t)`.
- **Formulación Simbólica en Dominio de Laplace (`s`) y Diagrama de Bode Semilogarítmico**: genera las ecuaciones nodales en `s`, extrae la función de transferencia racional `H(s) = V_out(s) / V_in(s)` junto con su ganancia DC y traza el barrido semilogarítmico $|H(j\omega)|$ en `dB` vs. $\log_{10}(f)$ detectando automáticamente la frecuencia de corte a `-3 dB`.
- **Mapas de Karnaugh (Quine-McCluskey) y Tabla de Verdad**: minimización exacta suma-de-productos (`SOP`) y listado de estados activos para funciones booleanas de 2 a 4 variables.
- **Exportación CAD**: esquemático vectorial `.svg`, plano `.dxf` y netlist compatible con KiCad/SPICE (`.net`).

---

## 5. Puente Bidireccional con Calculadoras Oficiales CASIO y Memoria de Cálculo

- **Emulador → Lienzo (`Enviar a CuadernU`)**:
  - Desde `CalculadoraUnificada.exe`, transfiere en vivo el modelo activo (`fx-991CW`, `fx-991LA X`, `fx-570VN PLUS`, `fx-CG50`), la secuencia auditada de pulsaciones, la expresión matemática en notación 2D natural y la captura de pantalla LCD hacia una **Tarjeta CASIO Interactiva** en el lienzo.
- **Lienzo → Teclas CASIO (`Traducir Ecuación/Matriz/Fasor -> Teclas CASIO`)**:
  - Convierte expresiones con prefijos de ingeniería, sistemas de ecuaciones simultáneas (`SEL`), matrices (`det`, `inv`, multiplicación) o fasores complejos en la macro exacta de botones físicos para `fx-991CW` (`[HOME] -> Equation / Matrix / Complex`) o `fx-991LA X` (`[MENU] -> [A] / [4] / [2]`).
- **Exportación de Memoria de Cálculo / Informe de Laboratorio (`.html` / `.md`)**:
  - Genera en un clic un informe técnico autocontenido con marcadores, tablero de etiquetas, ecuaciones `#varsub`, auditoría de teclas CASIO con LCD incrustado, esquemas vectoriales SVG, funciones de transferencia `H(s)` y gráficas 2D/3D listas para impresión o entrega académica.

---

## 6. Organización Académica, Audio Sincronizado, Capas e Interoperabilidad

- **Sistema de Capas y Capa Privada Docente**: creación de múltiples capas con control individual de visibilidad, bloqueo de edición, opacidad y exclusión selectiva; las capas marcadas como privadas docente permanecen visibles durante la preparación de clase y se ocultan automáticamente en **Modo Presentador** y al exportar a PDF.
- **Cinta de Estudio (*Study Tape*) y Repetición Espaciada (`SM-2`)**: oculta respuestas o fórmulas para autoevaluación con un clic y genera mazos de repaso programado con algoritmo SuperMemo-2.
- **Etiquetas (`#tags`) y Tablero Resumen**: clasificación transversal (`#examen`, `#importante`, `#duda`) con índice consolidado por página o lienzo.
- **Ink & Node Replay + Audio PCM WAV (`16-bit`)**: grabación de notas de voz sincronizadas milisegundo a milisegundo con la reproducción temporal, atenuando al `20%` de opacidad los trazos y nodos futuros hasta el instante exacto en que fueron creados.
- **Enlaces Bidireccionales `PDF ↔ Lienzo` (*InkLinks*) y Colapso de Páginas**: navegación cruzada entre páginas de manuales PDF y nodos del Lienzo Infinito, más plegado temporal de rangos de páginas.
- **Persistencia Atómica `.cudoc`, WAL y Fusión sin Pérdida**:
  - Guardado transaccional ZIP/JSON con respaldo automático, registro *Write-Ahead Log* (`WAL`), fronteras seguras de Deshacer (`Undo Boundary`), historial de instantáneas por página y resolución de ediciones offline concurrentes mediante páginas de conflicto lado a lado.
- **Motor de Interoperabilidad, Migración e Importación Multiformato de Alta Fidelidad**:
  - Migración nativa con informe previo de compatibilidad en 3 niveles (**Visual**, **Estructural** y **Editable**) desde **Goodnotes 6 (`.goodnotes`)**, **Notability (`.note`)**, **Microsoft OneNote (`.one` / `.onepkg` con descompresión nativa CAB `NONE`/`MSZIP`/`LZX`)**, **CuadernU Clásico (`.tinta`)**, **Microsoft Word (`.docx` con encabezados, tablas, imágenes y ecuaciones OMML)**, **Microsoft PowerPoint (`.pptx` con diapositivas, formas, tablas, imágenes y tinta vectorial)** y **PDF (`.pdf` con `/Outlines`, enlaces, formularios AcroForm, adjuntos y censura permanente)**.
  - Exportación bidireccional a `.cudoc` (y cifrado AES-256-GCM), `.pdf` (vectorial o firmado PKCS#7 SHA-256), `.docx`, `.pptx`, `.csv`, tutorial sincronizado `.html` y paquetes de examen LMS `.zip`.
- **Apariencia material translúcido 2.0 y Accesibilidad WCAG AAA**:
  - Popover de apariencia en vivo con modos **Claro**, **Oscuro** y **Alto Contraste** (bordes nítidos de `1 px`), deslizador continuo de opacidad del material vítreo (`Diseño` / `Equilibrado` / `Esmerilado`) y conmutador de *Reducir Transparencia*.
- **Módulos Académicos Universitarios (Solo Lectura + HorariosTEC)**:
  - Inspector Académico integrado con consulta de solo lectura a TEC Digital (cursos, evaluaciones, anuncios y sincronización local de materiales con búsqueda de texto completo) y generador automático de horarios sin traslapes (**HorariosTEC**).

---

## 7. Línea de Comandos (`CuadernU.exe`)

```bash
# Abrir un documento PDF o proyecto .cudoc en una página específica
CuadernU.exe --open manuales/fx-570_991LA_X_ES.pdf --page 5

# Migrar un cuaderno de Goodnotes 6, Notability, OneNote, Word, PowerPoint o .tinta
CuadernU.exe --migrate apuntes.goodnotes --out-cudoc apuntes.cudoc --out-report reporte.json

# Iniciar directamente en Lienzo Infinito con una tarjeta CASIO evaluada
CuadernU.exe --mode canvas --calc-card "fx-991CW|4.7 kOhm * 2 mA|V_out = 4.7 kOhm * 2 mA | V"

# Cargar escena de demostración STEM completa (3D CVD, MNA Transitorio, #varsub, Regla y Minimapa)
CuadernU.exe --showcase-stem
```
