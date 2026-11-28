# Agregar problemas de exámenes al tutor, verificados paso a paso (instrucciones para Antigravity)

**Para:** Antigravity (o cualquier agente que agregue lecciones al tutor de CuadernU).
**Escrito:** 27 de noviembre de 2026.
**Va junto con:** `docs/INSTRUCCIONES_AUDITORIA_VISUAL_PASO_A_PASO.md`. Esa guía audita y esta agrega. Toda lección nueva pasa por las dos.

## Qué se pide

Convertir problemas de exámenes en lecciones del tutor con el mismo nivel que las del plan de la reposición:

- cada dato subrayado en el enunciado y explicado;
- ningún paso intermedio saltado;
- cada número y cada letra con su origen a la vista;
- «¿Por qué?» en cada paso;
- todo comprobado con calculadora, contra la solución oficial y **mirando la pantalla**.

El estudiante tiene pasos muy cortos y base cero en cálculo. Si alguien así no puede pasar de un renglón al siguiente con lo que ve y oye, la lección no está terminada.

## 0. Reglas que no se negocian

1. **Todo en `main`.** Prohibido crear ramas, worktrees o copias del proyecto (AGENTS.md, regla 7; Git lo bloquea).
2. **Commits pequeños, solo con tus archivos y solo con el visto bueno del dueño.** Nada de `git add -A` ni `git commit -a`: en el árbol hay trabajo de otras sesiones. Nada de `push` sin permiso. Ojo: el repositorio de GitHub es **público**.
3. **Los PDF nunca van a Git.**
   - Los de Física III están en `Fisica_III/`, que Git ignora.
   - Los de Física General II están en `Física II/`; los copió el dueño el 27 de noviembre.
   - **Antes de abrir cualquier PDF de un curso, comprobá que Git ignore su carpeta:** `git check-ignore -v "Física II/"` tiene que mostrar una regla. Si no muestra nada, pará y avisale al dueño, porque el repositorio es público.
   - No crees carpetas nuevas en la raíz (AGENTS.md, regla 2).
4. **Nada en la raíz.** Lo que generes (inventarios, capturas, informes) va en `build/agregar_problemas/` y lo temporal se borra al terminar.
5. **No toques lo que otra sesión está haciendo:** `tools/tutor_runtime/fisica3/procedencia.py`, `assets/cuadernu/tutor_web/js/leccion/reproductor.js`, `assets/cuadernu/tutor_web/js/ui/asistente.js` y `tools/tutor_runtime/banco_api.py`. Si necesitás cambiarlos, pedilo y explicá por qué.
6. **El progreso del estudiante no se toca.** Cada vez que abras el tutor, respaldá y restaurá `private_local/tutor_progreso.json` como dice la sección 1 de la guía de auditoría.
7. **Ningún número escrito a mano.** Todo resultado se calcula en Python, dentro del archivo de la lección, a partir de los datos.
8. **Informe honesto.** Lo que no se hizo o no se verificó se dice, con nombre.

## 1. Qué hay y qué falta en Física General III (FI2103)

**Ya están en el tutor (328 lecciones):**

- los III parciales (ordinario y extraordinario) de II-2023 a I-2026, en el Superexamen (Q001 a Q137);
- las 5 reposiciones con preguntas;
- todo el II-2026: el I y II parcial ordinario y extraordinario, y el III ordinario con el PDF oficial;
- la suficiencia I-2026, los Recomendados, las tareas de termo, las prácticas y el folleto.

**Faltan los I y II parciales de semestres anteriores** (MAS, ondas y fluidos), que son dos tercios de la materia de la reposición. Del I y II parcial, hoy el tutor solo tiene:

- los de II-2026;
- los Recomendados;
- la suficiencia;
- de una a tres preguntas de cada reposición.

**Inventario del 27 de noviembre de 2026.** Se hizo leyendo el encabezado de cada PDF, porque **el nombre del archivo a veces miente**. Todos traen texto, así que ninguno es escaneado. Las rutas son relativas a `Fisica_III/`.

| Semestre | Parcial | Ordinario | Extraordinario |
|---|---|---|---|
| I-2026 | I (MAS y ondas) | `Documentos I-2026/Exámenes/2026I_FG3_Solución_Exam1_Ordinario.pdf` | `Documentos I-2026/Exámenes/2026I_FG3_Solución_Exam1_Extraordinario.pdf` |
| I-2026 | II (fluidos) | `Documentos I-2026/Exámenes/2026I_FG3_Solución_Exam2_Ordinario.pdf` | `Documentos I-2026/Exámenes/2026I_FG3_Solución_Exam2_Extraordinario.pdf` |
| II-2025 | I (MAS y ondas) | `Evaluaciones II-2025/Exámenes/2025II_FG3_Solución_Exam1_Ord_VF.pdf` | `Evaluaciones II-2025/Exámenes/2025II_FG3_Solución_Exam1_ExtraOrd_VF.pdf` |
| II-2025 | II (ondas y fluidos) | `Evaluaciones II-2025/Exámenes/2025II_FG3_Solución_Exam2_Ord.pdf` | `Evaluaciones II-2025/Exámenes/2025II_FG3_Solución_Exam2_Extraord.pdf` |
| I-2025 | I | `Banco de exámenes/I-2025_I_examen_parcial_ordinario_solución.pdf` | `Banco de exámenes/I-2025_I_examen_parcial_extraordinario_solución.pdf` |
| I-2025 | II | `Banco de exámenes/I-_2025_II_examen_parcial_ordinario_soluciónn.pdf` | `Banco de exámenes/II-2025_examen_parcial_extraordinario_solución.pdf` (el nombre dice II-2025, pero es de I-2025) |
| II-2024 | I | `Evaluaciones II-2024/Exámenes/I Examen parcial/II-2024_I_examen_parcial_ordinario_solución_c.pdf` | `Evaluaciones I-2024/I Examen parcial/II-2024_I_examen_parcial_extraordinario_solución.pdf` (está en la carpeta de I-2024) |
| II-2024 | II | `Banco de exámenes/II-2024_II_examen_parcial_ordinario.pdf` | `Banco de exámenes/II-2024_II_examen_parcial_extraord_solución_-_copia.pdf` |
| I-2024 | I | `Banco de exámenes/I-2024_I_examen_parcial_ordinario.pdf` (ver la nota) | `Banco de exámenes/I-2024_I_examen_parcial_extraordinario.pdf` (preguntas con «1)») |
| I-2024 | II | `Banco de exámenes/I-2024_II_examen_parcial_ordinario.pdf` | `Banco de exámenes/I-2024_II_examen_parcial_extraordinario.pdf` |
| II-2023 | I (MAS, ondas y fluidos) | `Banco de exámenes/II-2023_I_examen_parcial_ordinario.pdf` | `Banco de exámenes/II-2023_I_examen_parcial_extraordinario.pdf` |

Notas:

- **Solución.** Casi todos dicen «SOLUCIÓN» en el encabezado, aunque el nombre no lo diga. Cuatro no lo dicen, y hay que abrirlos para ver si traen la resolución:
  - `2026I_FG3_Solución_Exam1_Ordinario.pdf`;
  - `I-_2025_II_examen_parcial_ordinario_soluciónn.pdf`;
  - `II-2025_examen_parcial_extraordinario_solución.pdf`;
  - `II-2023_I_examen_parcial_extraordinario.pdf`.
- **Dos versiones del I parcial ordinario de I-2024.** Hay dos PDF con el mismo encabezado: el del Banco (7 páginas) y `Evaluaciones I-2024/I Examen parcial/solución_I_examen_parcial_Física_III_ordinario.pdf` (11 páginas). Comparalos y usá el más completo.
- **II-2023 tuvo dos parciales.** El I cubre MAS, ondas y fluidos. El II fue de termo y ya está en el tutor, con el nombre «III Parcial II-2023».
- **Para el final:**
  - el modelo del I parcial de I-2026 (`Documentos I-2026/Exámenes/2026I_FG3_Exam1_Modelo.pdf`);
  - las tareas de I-2024 con solución (`Evaluaciones I-2024/Tareas/`);
  - los exámenes de 2018 y 2019 (`Documentos públicos de Cátedra de Física General III/`). En 2018 hubo cuatro parciales, así que revisá el tema de cada pregunta.
- **Listas de aulas.** En esas carpetas también hay listas de aulas con nombres de estudiantes. Son datos personales de otras personas: no se abren ni se copian.

**Prioridad**, porque la reposición es el martes 8 de diciembre:

1. los extraordinarios de I-2026 y II-2025 (4 exámenes): son los más recientes y los más difíciles;
2. los ordinarios de I-2026 y II-2025 (4);
3. I-2025 y II-2024 (8);
4. I-2024 y II-2023 (6).

Son 22 exámenes. **Antes del 8 de diciembre valen más 4 exámenes terminados según la sección 2.6 que 20 a medias.**

## 2. Receta para agregar un examen

Es la misma que se usó para el II-2026. Mirá `tools/tutor_runtime/fisica3/banco_parciales_ii_2026_p1.py` y `..._p2.py` como modelo, y `tests/tutor_runtime/test_reposicion_ii_2026.py` para las pruebas.

### 2.1 Inventario (sin tocar código)

La tabla de la sección 1 ya dice qué PDF hay. Antes de tocar código, confirmala **pregunta por pregunta** en `build/agregar_problemas/inventario.csv`, con una fila por pregunta y estas columnas:

- `fuente` (el id de 2.2), `pregunta`, `tipo` (selección o desarrollo), `tema`;
- `trae_solucion`: sí si el PDF muestra la resolución de esa pregunta, no solo la letra;
- `figura`: sí o no;
- `ya_en_tutor`: algunas preguntas se repiten entre semestres, y a veces el Superexamen o una reposición ya las tiene.

**Si un PDF resulta escaneado** (sin texto), el tutor no lo puede recortar solo. Anotalo y preguntá antes de seguir con ese.

### 2.2 Registrar el PDF

En `tools/tutor_runtime/documentos.py`:

1. **Agregá una fuente a `FUENTES`.**
   - **El id** sigue el esquema `"<año>-<semestre>-<parcial><O|E>"`, como los de II-2026 (`"26-II-1O"`). Por ejemplo, `"26-I-1E"` es el I parcial extraordinario de I-2026, y `"25-II-2O"` el II parcial ordinario de II-2025.
   - **El nombre del archivo** va sin tildes y en minúsculas. Si el PDF está en una subcarpeta de `Fisica_III/`, va con la ruta desde ahí, como ya está registrada la tarea `"24-I-T4"`. Por ejemplo, `"documentos i-2026/examenes/2026i_fg3_solucion_exam1_extraordinario.pdf"`.
   - **El nombre para mostrar**, por ejemplo «I parcial extraordinario I-2026».
   - **El segmentador:**
     - `"examen"` si las preguntas empiezan con «1.»;
     - `"examen26"` si empiezan con «1)» (PDF hechos en LaTeX).
2. **Comprobá que el tutor encuentre el archivo:** `Documentos(pathlib.Path(".")).rutas()["26-I-1E"]` tiene que devolver la ruta correcta.
3. **Enlazá la fuente con sus lecciones** en `PARCIALES_REPO26`, para que `leccion_de` encuentre la lección de cada pregunta.
   - Por ejemplo, `"26-I-1E": "P1E-26I"`, y las lecciones se llaman `P1E-26I-1`, `P1E-26I-2`, etc.
   - Agregá el patrón nuevo (`P[12][OE]-`) a la expresión de `assets/cuadernu/tutor_web/js/ui/biblioteca.js`, línea 188. Si no, la Biblioteca muestra «·» en lugar del código de la lección.
4. **Comprobá el recorte antes de escribir una sola lección:**
   - `Documentos(pathlib.Path(".")).problemas("26-I-1E")` tiene que devolver exactamente las preguntas del examen, en orden;
   - cada `recorte("26-I-1E", "n")` tiene que mostrar el enunciado completo, con su figura y **sin la respuesta**. En los PDF de solución eso importa: la resolución viene pegada al enunciado.

### 2.3 Escribir las lecciones

Un archivo por examen: `tools/tutor_runtime/fisica3/banco_parciales_<semestre>_p<1|2>.py`. Registralo en `MODULOS` de `tools/tutor_runtime/fisica3/registro.py`.

Cada lección cumple **todo** esto:

- **Fuente:** `fuente={"doc": "26-I-1E", "nombre": "I parcial extraordinario I-2026", "num": "3", "parcial": 1}`. Es lo que arma `_f(...)` en `banco_parciales_ii_2026_p1.py`.
- **Tema:** `tema="ondas"` para MAS y ondas, y `tema="fluidos"` para fluidos, igual que en las lecciones de II-2026. Así la Biblioteca las agrupa con las demás.
- **Hoja de fórmulas:** solo casillas de la hoja oficial del parcial que usa la reposición II-2026. Están en `formularios_p1_p2.py` (Parcial 1 y 2) y `formulario_ii_2026.py` (Parcial 3). Van con `F(...)`, que las dibuja en azul. Lo que no está en la hoja se **demuestra** en renglones, como exige el examen; por ejemplo, Torricelli desde Bernoulli.
- **Primer paso «Datos: qué nos dan y para qué sirve cada uno»:** por cada dato, `DATO("frase exacta del enunciado")`, más `M(r"símbolo = valor con unidades")`, más una frase hablada que diga para qué sirve. Los distractores se dicen como distractores.
- **Un solo movimiento por renglón** (regla del lápiz): sustituir, una operación, despejar un término o pasar unidades. El desglose automático separa la aritmética, pero **el álgebra la tenés que escribir vos**.
- **Origen de cada valor:**
  - cada valor nuevo sale de un dato subrayado, de un renglón anterior, de una casilla de la hoja o de una conversión escrita;
  - usá `.sale_de(("valor", "de dónde sale"))` en los pasos donde aparece algo que viene de lejos, que es lo que muestra «¿De dónde salió?»;
  - si un valor viene de más de un paso atrás, recordalo con la voz.
- **«¿Por qué?» en cada paso** con `.por_que("…")`: por qué se usa esa casilla y no otra, nombrando la palabra del enunciado que lo decide. Sin LaTeX en la voz; en el texto del porqué las fórmulas van entre `$…$`.
- **Integrales:** las 5 preguntas de la lección T-PLANTEO, cada una en su renglón: qué cambia, hacia dónde, el pedacito, su tamaño sacado del dibujo y la suma con los límites del dibujo.
- **Selección única:** `revisar_opciones(L, {...})` con un motivo concreto para descartar cada opción, y la correcta marcada.
- **Resultados:** `R(...)` recuadrado con unidades, `responde(...)` con el valor calculado y `oficial("fuente", ...)` con la clave o los números de la solución publicada.
  - Si la clave oficial está mal o es de otra versión, no la uses para cotejar: poné un `aviso` que lo explique, como en `banco_parciales_ii_2026_p1.py` (`_CLAVE_OTRA`).
- **Práctica:** `te_toca(...)` con un ejercicio corto parecido.
- **Dibujo** (`visual=` o `VISUAL(...)`), solo con los tipos que existen: `pv`, `conduccion`, `dilatacion`, `calor`, `calentamiento`, `gas`, `distribucion`, `radiacion`, `maquina`, `otto`, `entropia`, `funcion` y `pared`. **Si hace falta otro, proponelo; no lo inventes a medias.**

Cuidado al generar código desde la consola: una barra `\` simple dentro de un string de Python cambia el texto. Por ejemplo, `"\neq"` se vuelve un salto de línea y `"\frac"` un salto de página. En strings normales escribí `\\`, o usá strings `r"…"`.

### 2.4 Pruebas

1. **Un archivo de pruebas por examen**, `tests/tutor_runtime/test_<banco>.py`, copiando la forma de `test_reposicion_ii_2026.py`:
   - que las lecciones existen;
   - cero faltas en `dsl.revisar`, cero alertas en `verificador_fisico.auditar_leccion`, y cero faltas y cero avisos en `auditoria_pasos.auditar`;
   - el cotejo con la solución oficial da «coincide» (o el aviso existe);
   - el TeX se analiza bien con `assets/cuadernu/tutor_web/js/mate/tex.js` en Node;
   - el recorte de cada pregunta existe;
   - ningún texto trae caracteres de control escondidos (copiá `test_sin_caracteres_de_control_escondidos` de `test_explicaciones_reposicion.py`).
2. **Actualizá el total** de `test_banco_158.py`.
3. **La suite completa en verde:** `D:\DevTools\Python311\python.exe -m pytest tests/tutor_runtime -q -p no:cacheprovider`.

### 2.5 Auditoría visual de cada lección nueva

Seguí `docs/INSTRUCCIONES_AUDITORIA_VISUAL_PASO_A_PASO.md` con cada lección nueva, paso por paso, con la voz y en ventana visible:

- **Preparación:** la de su sección 1, tal cual, incluido el respaldo del progreso.
- **Revisión:** todos los códigos de su sección 4 (A a J). Si cambiaste algo de la pantalla, también la sección 9.
- **Evidencia de estas lecciones:** capturas, control y hallazgos van en `build/agregar_problemas/`, no en `build/auditoria_visual/`.
- **Acá sí corregís.** La regla 1 de esa guía («solo auditar») no vale para las lecciones que vos mismo agregaste. Las demás reglas sí.

Corregí todo hallazgo G1 y G2, y volvé a auditar la lección completa hasta que no quede ninguno. Al terminar, cerrá el servidor y restaurá el progreso como dice su sección 7.

### 2.6 Cuándo una lección cuenta como terminada

Una lección está terminada **solo** si cumple todo esto. No hay excepciones ni «casi»:

1. Pasa las pruebas de 2.4 y la suite completa está en verde.
2. El cotejo con la solución oficial da «coincide», o hay un aviso que explica por qué no se coteja.
3. **Hay una captura por cada paso**, tomada después de que el paso terminó de escribirse, en `build/agregar_problemas/capturas/<ID>_p<n>.png`.
4. **Hay un resultado de la función de control** (sección 8 de la guía de auditoría) en `build/agregar_problemas/control/<ID>.json`. Es lo que devuelve `JSON.stringify(__aud.r, null, 2)` en la consola cuando `__aud.estado` es «listo». Tiene que salir:
   - sin subrayados fallidos;
   - sin fórmulas desconocidas;
   - sin desbordes ni LaTeX crudo;
   - con «¿Por qué?» en todos los pasos.
5. **No quedan hallazgos G1 ni G2 abiertos** de esa lección en `build/agregar_problemas/hallazgos.jsonl`.

Para comprobar los puntos 3 y 4:

1. Escribí los ids de las lecciones nuevas, uno por línea, en `build/agregar_problemas/ids_nuevos.txt`.
2. Desde `D:\Proyectos\CalculadorasUnificadas`, corré esto y pegá la salida en el informe:

```powershell
D:\DevTools\Python311\python.exe -c "import json,pathlib; from tools.tutor_runtime.fisica3 import registro; b=pathlib.Path('build/agregar_problemas'); ids=[x.strip() for x in (b/'ids_nuevos.txt').read_text(encoding='utf-8-sig').splitlines() if x.strip()]; ctl=lambda i: json.loads((b/'control'/(i+'.json')).read_text(encoding='utf-8-sig')) if (b/'control'/(i+'.json')).exists() else None; [print(i, 'pasos', len(registro.obtener(i).pasos), 'en_control', len(ctl(i)['pasos']) if ctl(i) else 'FALTA', 'capturas', len(list((b/'capturas').glob(i+'_p*.png')))) for i in ids]"
```

Cada línea sale así: `P1E-26I-3 pasos 12 en_control 12 capturas 12`. La lección **no está terminada** si pasa cualquiera de estas cosas:

- dice `FALTA`;
- los tres números no son iguales.

### 2.7 Entrega

Escribí `build/agregar_problemas/INFORME.md`:

- **arriba, un resumen de 15 líneas como máximo:** qué exámenes se agregaron, cuántas lecciones, cuáles quedaron terminadas según 2.6 y cuáles no, y por qué;
- la salida del comando de 2.6;
- lo que no se pudo verificar (PDF escaneados, soluciones que no existen, claves oficiales equivocadas).

Después:

- corré la puerta de cierre: `D:\DevTools\Python311\python.exe tools/check_workspace_hygiene.py` tiene que dar 0;
- pedile al dueño el visto bueno para el commit (solo tus archivos).

## 3. Otro curso (por ejemplo, Física General II)

**Hoy el tutor está hecho solo para Física General III.** Agregar otro curso no es «meter problemas»: el paquete `tools/tutor_runtime/fisica3/` depende de cosas propias de FI2103:

- los temas y grupos del curso;
- las hojas de fórmulas, con sus casillas (cada paso de una lección sale de una casilla);
- las reglas del verificador físico (termo, fluidos y ondas);
- los dibujos disponibles;
- el plan de estudio.

El material está en `Física II/`: son 100 archivos, 97 de ellos PDF. Se hace por etapas, y **no se pasa a la siguiente sin el visto bueno del dueño**.

### Etapa 0. Investigar y proponer (sin tocar código)

1. **Revisá el material de `Física II/`,** después de comprobar que Git lo ignora (sección 0, regla 3):
   - **Exámenes:** `Exámenes Anteriores/`, con las carpetas `Examen I`, `Examen II`, `Examen III` y `Exámenes de Reposición`. Adentro hay una carpeta por semestre, de I-2023 a I-2026: 53 PDF en total.
   - **Hojas de fórmulas oficiales:** en `Documentos públicos de Cátedra de Física General II/`, en las carpetas «Instrucciones Generales y Formulario…».
   - **Lo demás:** `Capítulos del libro`, `Presentaciones`, `Tareas` e `Información examenes`. Los capítulos del libro sirven para entender los temas, pero no se copian a las lecciones.
   - **Si falta el programa del curso,** pedíselo al dueño.
2. **Hacé el inventario de 2.1 para ese curso.** Leé el encabezado de cada PDF, porque el nombre del archivo puede mentir.
3. **Escribí `build/agregar_problemas/fisica_general_ii/PROPUESTA.md`** con:
   - los temas;
   - las casillas de cada hoja;
   - qué dibujos harían falta (por ejemplo circuitos o campos, que hoy no existen);
   - qué reglas del verificador habría que agregar;
   - cómo elegiría el estudiante el curso en la pantalla;
   - **cómo generalizar el código sin copiarlo:** prohibido duplicar `fisica3/` en otra carpeta (AGENTS.md, reglas 1 y 3).
4. Esperá el visto bueno.

### Etapa 1. Piloto

Cinco problemas, uno por tema, de punta a punta con todo lo de la sección 2, incluida la definición de terminado de 2.6. El dueño los revisa antes de seguir.

### Etapa 2. Escalar

En lotes de 10 lecciones, con las mismas reglas. Cada lote se cierra con su informe y su commit, siempre con visto bueno.

## 4. Texto corto para pegar en Antigravity

> Leé `docs/INSTRUCCIONES_AGREGAR_PROBLEMAS_AL_TUTOR.md` y `docs/INSTRUCCIONES_AUDITORIA_VISUAL_PASO_A_PASO.md`, y seguilos al pie de la letra. Trabajá solo en `main`, sin ramas ni copias. No subas PDF a Git y no hagas commits ni push sin mi visto bueno. Empezá por la tabla de la sección 1: primero los I y II parciales extraordinarios de I-2026 y II-2025, después sus ordinarios, y así en el orden de prioridad. Cada lección lleva el paso «Datos» con cada dato subrayado, un solo movimiento por renglón, el origen de cada valor a la vista, «¿Por qué?» en cada paso, y los números calculados en Python y cotejados con la solución oficial. Una lección solo cuenta como terminada si cumple la sección 2.6: pruebas en verde, una captura por paso, el control de la consola limpio y cero hallazgos G1 o G2 abiertos. Entregá el informe de la sección 2.7 y decí con honestidad qué no terminaste. De Física General II hacé solo la Etapa 0 de la sección 3, y esperá mi visto bueno antes de seguir.
