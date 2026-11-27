# Backlog de CuadernU

> **Origen:** [`INVESTIGACION_COMPETENCIA_2026.md`](INVESTIGACION_COMPETENCIA_2026.md) (27-nov-2026).
> **Alcance:** Windows con lápiz + iPad con lápiz del iPad, uso personal.
> **Estados de una tarea:**
> - **Pendiente:** no empezó.
> - **En curso:** alguien la está haciendo, siempre en `main` (no hay ramas).
> - **Hecha:** compilada, probada y vista funcionando. Si no se verificó, no está hecha.
>
> **IDs de la hoja:** la columna `tarea_backlog` de [`competencia_2026/evidencias/revision_codigo_2026-11-27.csv`](competencia_2026/evidencias/revision_codigo_2026-11-27.csv) dice qué requisitos cubre cada tarea.
> **Diseño para la concentración:** las tareas T2 a T5 ya tienen su plan en [`AUDITORIA_ATENCION_APP_COMPLETA.md`](AUDITORIA_ATENCION_APP_COMPLETA.md); aquí solo se enlazan.

## Nivel 0. Que no dañe ni mienta (bloque 1)

### N0-1. Que no cambie tu letra sola · Pendiente · 16 IDs

- **Qué:**
  - Formas automáticas apagadas por defecto. Si se encienden, solo actúan con una pausa real de al menos 500 ms al final del trazo.
  - Tachar para borrar con interruptor, apagado por defecto, y que nunca borre imágenes ni páginas.
  - El doble toque ya no convierte la página.
  - La ortografía manuscrita y el modo gestos Flexcil pasan a «Experimental».
- **Hecha cuando:** con los valores por defecto, un zigzag (resistencia), «m», «w», «:», «l» y «0» no se convierten ni borran nada. Lo comprueba una prueba automática y lo confirmas tú con el lápiz.
- **Archivos:**
  - `studio/app/main_studio.cpp` (397, 7930-7963);
  - `studio/core/ink_engine.cpp` (216, 334, 425);
  - `studio/app/studio_actions.hpp` (277-300);
  - `studio/core/smart_script_reflow.cpp` (560-608);
  - `studio/pdf/flexcil_study_overlay.cpp` (118-185).

### N0-2. Resaltador translúcido de verdad · Pendiente · 9 IDs

- **Qué:** cada trazo translúcido se dibuja una sola vez:
  - en pantalla, con geometría de trazo único o una capa D2D con la opacidad;
  - en el PDF, con una sola ruta y `/CA`.
- **Hecha cuando:** sobre fondo blanco, un trazo de resaltador da el color de una sola capa (±2 por canal), en un render fuera de pantalla y en el PDF exportado. Y el texto de un PDF resaltado se sigue leyendo.
- **Archivos:** `studio/render/d2d_renderer.cpp` (1052-1082) y `studio/pdf/pdf_writer.cpp` (1206-1230).

### N0-3. Puerta «Experimental» y etiquetas honestas · Pendiente · 29 IDs

- **Qué:**
  - Archivo nuevo `studio/app/studio_funciones_experimentales.hpp` con la lista de comandos simulados.
  - Casilla «Mostrar funciones experimentales» en el perfil, apagada por defecto.
  - Filtro en los menús, en Explorar (Ctrl+K), en la barra y en `ExecuteToolbarCommand`.
  - Lista inicial: kanban, calendario de bases, velocidad de audio, diarización, actas, QR en red local, enlaces compartidos, traducción sin conexión, biblioteca «global», portadas, firma con trazo fijo, reemplazar texto, rectificar escaneo, «PDF editable» y desplazamiento horizontal.
  - Etiquetas honestas:
    - «Tapar con rectángulo (el texto sigue en el archivo)»;
    - «Bloquear nota» avisa que no cifra;
    - «Cambiar × ÷ − por * / -»;
    - DOCX y PPTX «como imágenes, primeras 12 páginas»;
    - importar .one dice «Parcial»;
    - el resumen sin IA ya no se marca como verificado.
- **Hecha cuando:** una prueba recorre los menús y Explorar sin encontrar ningún comando de la lista con la casilla apagada, y las capturas lo muestran.

### N0-4. Sin datos inventados · Pendiente · 15 IDs

- **Qué:** quitar las notas «Resumen» y «Formulario» del grafo vacío, los registros «Tarea 1» de las bases vacías y el tono sintético cuando no hay micrófono.
- **Hecha cuando:** el grafo y las bases vacíos quedan vacíos, con un mensaje claro, y grabar sin micrófono dice «No encontré un micrófono» sin guardar audio.
- **Archivos:** `studio/app/main_studio.cpp` (5105-5111, 6047-6050, 5300-5311, 5339-5350) y `studio/core/audio_timeline.cpp` (485-507).

## Nivel 1. Lo diario

### N1-1. Tinta rápida en Windows · Pendiente · 37 IDs

- **Qué:**
  - Medir primero: banco de latencia con marcas de tiempo por muestra.
  - Después:
    - GPU por defecto;
    - capa de tinta «húmeda» (DirectComposition o `InkPresenterDesktop`);
    - repintar solo lo que cambia y suavizar en vivo;
    - pellizco y paneo con dos dedos, y que el dedo no dibuje si hay lápiz.
- **Hecha cuando:** latencia p95 menor que un fotograma a 60 Hz en tu equipo, medida, y la apruebas con el lápiz.
- **Archivos:** `studio/render/d2d_renderer.cpp` (3352-3370), `studio/app/main_studio.cpp` (7886-8908) y `studio/core/ink_engine.cpp`.

### N1-2. Un formato y sincronización Windows ↔ iPad · Pendiente · 16 IDs

- **Qué:**
  - El iPad lee y escribe .cudoc en una carpeta visible desde Archivos.
  - Windows sincroniza la carpeta de cuadernos, no la del perfil.
  - Fusión con estado base: nunca se pisa lo más nuevo y los conflictos se guardan como copia.
- **Hecha cuando:** un cuaderno editado en Windows aparece en el iPad y al revés, con texto, tinta, imágenes y audio. Y una edición en los dos lados a la vez no pierde nada (prueba automática con dos carpetas).

### N1-3. La app de iPad en tu iPad · Pendiente · 11 IDs

- **Qué:**
  - Build Release firmada, con Sideloadly y iPadOS ID gratis (volver a firmar cada 7 días) o con cuenta de desarrollador y TestFlight desde el CI.
  - Las 651 pruebas compilando en el CI.
  - Quitar del `Info.plist` lo que promete y no hace.
- **Hecha cuando:** la app se instala en tu iPad, abre un cuaderno y escribe con el Pencil, y el CI corre las pruebas del iPad.

### N1-4. PDF nítido y con texto real · Pendiente · 64 IDs

- **Qué:**
  - Dibujo por mosaicos, con vista previa de baja resolución.
  - Extracción de texto fuera del hilo de la interfaz.
  - Un motor de PDF probado (por ejemplo PDFium, con licencia BSD; revisar el aviso en `THIRD_PARTY_NOTICES.md`) para el texto con posiciones, los enlaces, los destinos con nombre y los marcadores.
  - Exportar con `/Annots`, enlaces y marcadores.
- **Hecha cuando:**
  - un PDF de 500 páginas abre en menos de 2 s y se ve nítido al 800 %;
  - seleccionar texto cae sobre las letras en un corpus de prueba;
  - los marcadores de un PDF hecho con LaTeX llevan a la página correcta.

### N1-5. Biblioteca visual y búsqueda global · Pendiente · 105 IDs

- **Qué:**
  - Cuadrícula de cuadernos con portadas y miniaturas, agrupada por curso.
  - Índice persistente de todos los cuadernos: texto, PDF y manuscrito, con reconocimiento en segundo plano y el idioma de la nota.
  - Duplicar y fusionar de verdad.
- **Hecha cuando:** una palabra escrita a mano en cualquier cuaderno aparece al buscar, incluso de cuadernos que no se abrieron en esta sesión.

### N1-6. Traer de verdad tus cuadernos de Goodnotes, OneNote y Notability · Pendiente · 6 IDs

- **Qué:**
  - **OneNote:** lector de .one y .onepkg según [MS-ONESTORE] y [MS-ONE], las especificaciones públicas de Microsoft. Trae la jerarquía (cuaderno → sección → página), el texto, las imágenes y la tinta con presión. Las secciones con contraseña se abren con tu contraseña. Sirven de referencia [onenote_parser](https://docs.rs/crate/onenote_parser/1.1.1) y Apache Tika; antes de reutilizar código se revisa su licencia.
  - **Goodnotes:** ingeniería inversa de caja negra con tus propios archivos. Es un ZIP con .pb con prefijo de longitud y bloques LZ4.
    - Crear cuadernos de prueba con trazos conocidos y comparar bytes.
    - Documentar el esquema en `docs/`.
    - No descompilar la app ni saltarse ningún cifrado.
    - Plan B mientras tanto: importar el PDF que exporta Goodnotes y volver editable su tinta vectorial.
  - **Notability:** varias páginas y audio .m4a.
  - **Pruebas:** con archivos tuyos reales en `private_local/` (Git lo ignora), nunca en el repo público.
- **Hecha cuando:** un cuaderno real tuyo de cada app se importa con su estructura, su texto y su tinta editable, y una prueba compara contra lo que se ve en la app original.

## Nivel 2. Igualar las herramientas de estudio

### N2-1. Audio ligado a cada trazo · Pendiente · 46 IDs

- **Qué:**
  - Cada trazo guarda qué grabación y qué instante le corresponden.
  - Compresión (AAC u Opus con Media Foundation).
  - La pausa no desincroniza.
  - Transcripción incluida (whisper.cpp dentro de la app) en segundo plano.
  - Importar m4a y mp3.
- **Hecha cuando:** tocar un trazo reproduce el audio de ese momento en la grabación correcta, y una hora de clase ocupa menos de 30 MB.

### N2-2. IA que no congela y ve tu letra · Pendiente · 28 IDs

- **Qué:**
  - Llamadas en segundo plano, que se pueden cancelar.
  - Enviar la imagen de la página a modelos con visión para resumir, hacer tarjetas y cuestionarios.
- **Hecha cuando:** la ventana sigue respondiendo durante una consulta y un resumen de una página manuscrita cita lo que dice.

### N2-3. Páginas, lazo y deshacer completos · Pendiente · 48 IDs

- **Qué:**
  - Reordenar y duplicar páginas desde la interfaz.
  - Miniaturas con tinta, que se pueden arrastrar.
  - Lazo que escala, rota y recolorea varios objetos.
  - Regla que se arrastra.
  - Las capas también se pueden deshacer.
- **Hecha cuando:** cada acción tiene una prueba y se deshace.

### N2-4. Limpiar la letra (restyle) · Pendiente · 5 IDs

- **Qué:** enderezar y limpiar tu letra sin cambiar palabras ni estilo. Es lo nuevo de Goodnotes en 2026.

### N2-5. Reconocer ecuaciones manuscritas · Pendiente · 17 IDs

- **Qué:** reconocimiento 2D (fracciones, exponentes, raíces, integrales) que termina en LaTeX y en el CAS.
- **Hecha cuando:** un corpus de 50 ecuaciones de tus apuntes da al menos un 90 % correctas.

### N2-6. Unir la bóveda Markdown con los cuadernos · Pendiente · 28 IDs

- **Qué:**
  - Los [[ ]] escritos en un cuaderno crean enlaces y backlinks.
  - Editor Markdown.
  - Elegir la carpeta de la bóveda.
  - Las notas diarias son notas de verdad.

## Nivel 3. Donde puedes ganar

### N3-1. La calculadora Casio viva dentro de la página · Pendiente

- **Hoy:** la calculadora envía la secuencia como tarjeta, que el CAS recalcula (`apps/windows/main.cpp:1889-1935`).
- **Qué:** el emulador real dentro de la página, que reproduce las teclas y la pantalla.

### N3-2. Tutor de física unido a los apuntes · Pendiente · 2 IDs

- **Qué:** tocar un problema del PDF y ver los pasos con tu formulario oficial.

### N3-3. Unidades, chequeo dimensional y circuitos en la página · Pendiente · 12 IDs

### N3-4. TEC Digital: un cuaderno por curso · Pendiente

## Diseño para la concentración (plan en `AUDITORIA_ATENCION_APP_COMPLETA.md`)

- **T2:** modo simple de CuadernU · Pendiente · 6 IDs.
- **T3:** «Hoy», cuenta regresiva y reloj de estudio · Pendiente.
- **T4:** mismo idioma y estilo en todo · Pendiente · 5 IDs.
- **T5:** accesibilidad técnica (UIA) · Pendiente · 1 ID.

## Mantenimiento y riesgos

- **M-1. Respaldo.** La fase 1 de concentración y el tutor ya están en GitHub (repo público) desde el 27-nov. Los commits nuevos de `main` siguen solo en este PC: hay que decidir si se suben y si se borra de GitHub la rama `feat/tutor-reposicion-ii-2026`.
- **M-2. Trabajo sin revisar.** Los 24 archivos del 26-nov (MCP, TEC Digital, tutor, WebView2) quedaron guardados tal cual en `main`; hay que revisarlos y probarlos.
- **M-3. Pruebas en rojo.** PrintoutTest y RealE2EClosureTest fallaban en la última corrida (26-nov, 23:24).
- **M-4. Pruebas que no prueban.**
  - `Full987MatrixComplianceTest` solo valida el CSV; hay que renombrarla o decirlo en su salida.
  - Quitar el respaldo «HIO» de `studio/tests/knowledge_graph_and_bases_test.cpp:900`.
  - Las pruebas de rendimiento buscan texto en el código (`studio/tests/perf_and_rich_media_test.cpp:208-218`).
- **M-5. CI.** El CI no compila CuadernU para Windows (`CMakeLists.txt:13`).
- **M-6. Tutor portable.** El tutor busca Python en `D:\DevTools` (`studio/tutor/tutor_sidecar_bridge.cpp:47-73`).

[MS-ONESTORE]: https://msopenspecs.microsoft.com/files/MS-ONESTORE/%5bMS-ONESTORE%5d-180619.pdf
[MS-ONE]: https://learn.microsoft.com/en-us/openspecs/office_file_formats/ms-one/
