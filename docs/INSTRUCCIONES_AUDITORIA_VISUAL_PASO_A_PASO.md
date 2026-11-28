# Auditoría visual paso a paso de las lecciones del tutor (instrucciones para Antigravity)

**Para:** Antigravity (o cualquier agente que audite el tutor de CuadernU).
**Escrito:** 27 de noviembre de 2026. Los comandos y la función de la sección 8 se probaron ese día sobre este repositorio.

## Qué se pide

Revisar **todas las lecciones, paso por paso y renglón por renglón**, buscando:

1. cualquier defecto visual (fórmulas mal dibujadas, cosas que se salen o se tapan, subrayados corridos, dibujos en blanco);
2. cualquier **paso o paso intermedio que falte** (dos movimientos de álgebra en un solo renglón, cuentas sin mostrar);
3. cualquier **dato cuyo origen no se señale**: cada número y cada letra tiene que venir de algo visible, subrayado en el enunciado, escrito en un renglón anterior o tomado de una casilla de la hoja de fórmulas;
4. si el tutor **no sabe señalar con la voz y en la pizarra** de dónde salió algo cuando el estudiante lo pide;
5. cualquier defecto visual o de efectos **en el resto de la app**: cada pantalla del tutor, cada panel y cada ventana de CuadernU (sección 9).

El tutor es para un estudiante que parte de **cero en cálculo** y necesita pasos muy cortos. La vara es esta: si alguien que no sabe integrar ni despejar no puede pasar de un renglón al siguiente con lo que está en pantalla y lo que dice la voz, eso es un defecto.

## 0. Reglas que no se negocian

1. **Solo auditar.** No cambies código, lecciones ni textos. Arreglar viene después y solo con el visto bueno del dueño.
2. **Todo en `main`.** Prohibido crear ramas, worktrees o copias del proyecto (AGENTS.md, regla 7; Git lo bloquea).
3. **Nada de commits ni push.** Nada en la raíz del proyecto. Todo lo que generes va en `build/auditoria_visual/`.
4. **Hay otra sesión trabajando en los orígenes de cada dato**: `tools/tutor_runtime/fisica3/procedencia.py`, `assets/cuadernu/tutor_web/js/leccion/reproductor.js`, `assets/cuadernu/tutor_web/js/ui/asistente.js` y `tools/tutor_runtime/banco_api.py`. No toques esos archivos: auditá cómo se comportan hoy y reportalo.
5. No escribas contraseñas ni entres a la cuenta del TEC.
6. **El progreso del estudiante no se toca.** Respaldalo antes y dejalo idéntico al final (secciones 1 y 7). Al abrir lecciones el tutor guarda en qué paso quedó, así que sin el respaldo le cambiarías el progreso real.
7. **Informe honesto.** Si una lección o un control no se revisó, se dice. Nada de «todo bien» sin evidencia.

## 1. Preparar (PowerShell, en `D:\Proyectos\CalculadorasUnificadas`)

```powershell
Set-Location D:\Proyectos\CalculadorasUnificadas
New-Item -ItemType Directory -Force build\auditoria_visual | Out-Null
Copy-Item private_local\tutor_progreso.json build\auditoria_visual\progreso_respaldo.json
(Get-FileHash private_local\tutor_progreso.json -Algorithm SHA256).Hash | Set-Content build\auditoria_visual\progreso_hash.txt
Set-Content -Path build\auditoria_visual\arranque.json -Value '{"session_token":"auditoria"}' -Encoding ascii
$srv = Start-Process -FilePath D:\DevTools\Python311\python.exe -ArgumentList '-m','tools.tutor_runtime.server','--bootstrap-stdin','--port','8766' -RedirectStandardInput build\auditoria_visual\arranque.json -RedirectStandardOutput build\auditoria_visual\servidor.log -RedirectStandardError build\auditoria_visual\servidor_err.log -WorkingDirectory (Get-Location) -WindowStyle Hidden -PassThru
$srv.Id | Set-Content build\auditoria_visual\servidor_pid.txt
```

- Esperá a que `build\auditoria_visual\servidor.log` tenga la línea `{"ready": true, "host": "127.0.0.1", "port": 8766}`.
- Abrí `http://127.0.0.1:8766/?token=auditoria` en una **ventana visible**. Si la ventana está oculta, el navegador frena los temporizadores y la voz, y los tiempos no sirven.
- No uses el puerto 8765: puede ser el CuadernU del estudiante.

### Primero, lo automático

```powershell
D:\DevTools\Python311\python.exe -m pytest tests/tutor_runtime -q -p no:cacheprovider
```

En el navegador, guardá en `build/auditoria_visual/automatico/` lo que devuelven estas dos rutas:

- `/api/banco-revision?token=auditoria`: faltas de calidad de cada lección.
- `/api/banco-auditoria?token=auditoria`: auditoría física de 10 niveles.

Lo que esos controles ya marcan va al informe como «ya detectado». **Tu trabajo visual es encontrar lo que ellos no ven.**

## 2. Qué lecciones y en qué orden

1. **Primero las 76 del plan «Reposición II-2026»**, en el orden del calendario. El examen es el martes 8 de diciembre, así que son las urgentes.
   ```js
   (await fetch('/api/preparacion?token=auditoria').then((r) => r.json()))
     .reposicion_ii_2026.bloques.flatMap((b) => b.items.map((i) => i.id))
   ```
2. **Después el resto del banco** (328 lecciones en total):
   ```js
   (await fetch('/api/banco?token=auditoria').then((r) => r.json())).lecciones.map((l) => l.id)
   ```
3. Trabajá **en lotes de 10 lecciones**. Al cerrar cada lote, actualizá `build/auditoria_visual/avance.json` (cada id con «revisada» o «pendiente») y escribí sus hallazgos. Si te cortan, se retoma desde ahí.
4. **La recorrida de toda la app (sección 9)** se hace dos veces:
   - al principio, porque es rápida y encuentra defectos que afectan a todas las lecciones;
   - al final, para confirmar que siguen ahí o que ya no.

## 3. Cómo recorrer una lección

En la consola del navegador está `window.__tutor`:

| Para | Usar |
|---|---|
| Abrir una lección | `await __tutor.acciones.abrir('O26-8')` |
| Ir a un paso (desde 0) | `__tutor.rep.irA(n)` |
| Saber si el paso terminó | `__tutor.rep.estado`: «hablando» mientras escribe; «espera» o «fin» cuando terminó |
| Ver la lección completa en JSON | `__tutor.rep.lec`. Tiene pasos → tramos → acciones: `mate` (renglón), `dato` (subrayado), `texto`, `lista`, `tabla`, `visual`, `opcion` y `fuente` (señalar un origen) |
| Ver el dibujo, el enunciado y las fórmulas | Botón «Apoyo», arriba a la derecha. El modo enfoque (tecla F) lo esconde |
| Ayuda del paso | Tecla A abre el menú. I: «¿En qué iba?», D: «Más despacio», O: «¿De dónde salió?», P: «¿Por qué?» |
| Preguntar por algo puntual | Herramienta «Encerrar» (barra izquierda): rodeás algo de la pizarra y el menú ofrece «¿De dónde salió…?». Esa respuesta la da la IA |

- **Mirá cada paso completo, con la voz.** Las primeras 10 lecciones, a velocidad normal. Después podés subir el ritmo en Ajustes → Ritmo, pero volvé a la velocidad normal en cualquier paso donde encuentres algo.
- **Control automático por lección:** pegá en la consola la función de la sección 8 y corré `lanzarAuditoria('Q020')`. Corre sola mientras mirás, y el resultado se consulta con `__aud`. Por paso, da:
  - datos que debían subrayarse y no se subrayaron;
  - fórmulas que la pizarra no entiende;
  - renglones que se salen del ancho;
  - LaTeX crudo a la vista;
  - si el paso tiene «¿Por qué?» y «¿De dónde salió?».

  Es una red de seguridad: **no reemplaza mirar**.

## 4. Lista de control de cada paso

Revisá todo esto en cada lección, cada paso y cada renglón. Cada problema encontrado es un hallazgo con su código.

### A. Enunciado y recorte

- **A1.** El recorte es del problema correcto (examen y número) y se lee completo, con su figura.
- **A2.** El recorte no deja ver la respuesta (resaltado del solucionario, letra marcada).
- **A3.** Si no hay PDF, el enunciado escrito está completo y sus fórmulas se ven dibujadas, sin `$` ni `\`.

### B. Paso «Datos»

- **B1.** Cada número o condición que se usa después está subrayado en el enunciado. Si no hay PDF, en el texto escrito.
- **B2.** El subrayado cae sobre las palabras correctas, no corrido ni sobre otra cosa. Compará con la `frase` de cada acción `dato`.
- **B3.** Cada dato se escribe en la pizarra con su símbolo y sus unidades, y la voz dice para qué sirve.
- **B4.** Los datos que sobran (distractores) se dicen como tales.

### C. De dónde sale cada valor (lo más importante)

Para **cada número y cada letra que aparece por primera vez** en un renglón, tiene que cumplirse una de estas cuatro:

1. está subrayado en el enunciado (paso «Datos») y el valor coincide;
2. sale de un renglón anterior que se ve, con el mismo valor (o un redondeo que se dice);
3. es una casilla o constante de la hoja (la fórmula en azul, g, R, σ, 101 325 Pa) y la pizarra lo muestra;
4. es una conversión de unidades, y la conversión está escrita en un renglón.

- **C1. Dato huérfano:** no cumple ninguna de las cuatro.
- **C2. Valor que cambió sin aviso:** el número no coincide con su origen.
- **C3. Origen lejano sin señalar:** sale de más de un paso atrás y nadie lo recuerda.

Probá también **la ayuda con voz y pizarra**, que es lo que el estudiante usa cuando no entiende:

- **C4.** Ayuda → «¿De dónde salió?» (tecla O) tiene que señalar en azul la casilla de la hoja, el dato del enunciado o el renglón anterior correcto, y decirlo con la voz. Es defecto si aparece «No pude señalar esta referencia en la pizarra», si señala otra cosa o si no dice nada.
- **C5.** Encerrá ese número → «¿De dónde salió…?». Anotá si responde, si la respuesta es correcta y cuánto tarda. Si no hay conexión, anotalo como «no verificable».
- **C6.** El recuadro «Qué es cada letra», que sale al escribir una fórmula: cada letra nueva tiene que tener su significado y sus unidades correctos.

### D. Pasos intermedios

- **D1. Regla del lápiz:** entre un renglón y el siguiente hay **un solo movimiento**: sustituir, hacer una operación, despejar un término o pasar unidades. Si hay que hacer dos o más cosas de cabeza, es «paso saltado».
- **D2.** Toda cuenta de calculadora muestra la operación y el resultado. Comprobá cada resultado con calculadora; se acepta hasta media unidad de la última cifra.
- **D3. Integrales:** cada cosa en su propio renglón, escrita: el pedacito (franja, disco o cascarón), su tamaño sacado del dibujo, la fórmula para un pedacito, los límites y el «arriba menos abajo». La lección T-PLANTEO es la referencia.
- **D4.** No se despeja más de una cosa por renglón.
- **D5.** Los pasos «(sigue)» continúan donde terminó el anterior, sin repetir ni saltar.
- **D6.** «Más despacio» (tecla D) existe donde el paso es denso y agrega renglones de verdad.

### E. Pizarra

- **E1.** Ninguna fórmula como texto crudo (`\frac`, `$`, `^{}`) y ningún cuadro vacío por un símbolo que falta.
- **E2.** Nada se sale del ancho, nada queda tapado por los paneles y nada se encima.
- **E3.** Los colores tienen sentido: azul es fórmula de la hoja, verde con recuadro es resultado, naranja es dato.
- **E4.** Se lee bien en modo claro y oscuro, y con la escala de Windows al 100 % y al 200 % (4K).
- **E5.** El resultado final está recuadrado y con unidades.

### F. Voz y subtítulos

- **F1.** Lo que se dice corresponde a lo que se escribe en ese momento, sin desfase.
- **F2.** La voz no lee símbolos raros ni LaTeX, y los números se dicen bien.
- **F3.** No hay silencios largos ni frases cortadas.

### G. Ayudas del paso

- **G1.** «¿Por qué?» (tecla P) existe en cada paso y explica por qué se hace así, nombrando lo del enunciado que decide el método. No repite lo que ya dijo la voz.
- **G2.** «¿En qué iba?» (tecla I) resume bien el paso actual.

### H. Dibujo (panel «Apoyo»)

- **H1.** Corresponde al problema y a sus números, cambia en el paso correcto, tiene etiquetas legibles y no queda en blanco.

Si el panel muestra la **escena 3D** (con los botones 3D, Frontal, Lateral, Superior y Corte), además:

- **H2. Números del enunciado.** Cada número que escribe la escena (alturas, radios, temperaturas, masas, presiones) es el del enunciado o sale de él con una cuenta. Si el enunciado es con letras o la lección es de teoría, la línea de abajo de la escena empieza con «Valores de ejemplo». La prueba `tests/tutor_runtime/test_tutor_3d_plan_escenas.py` revisa los números escritos a mano; esta revisión es para lo que se ve.
- **H3. Girar y volver.** Arrastrar con el mouse, el lápiz o un dedo gira la escena **aunque esté en pausa**; no se da vuelta (la inclinación tiene tope). Doble toque, doble clic o la tecla Inicio vuelve a la vista inicial, que es la del dibujo del enunciado.
- **H4. Vistas y zoom.** Frontal, Lateral, Superior y Corte cambian de verdad la imagen. La rueda, las teclas + y − y el pellizco con dos dedos acercan y alejan.
- **H5. Profundidad.** Se entiende qué está delante y qué está detrás. Nada se ve a través de una pieza que debería taparlo, salvo en «Corte».
- **H6. Paso «Datos».** En ese paso la escena muestra, a la izquierda y en naranja, cada dato con su símbolo y su valor (n = 1 mol, V_C = 0,0045 m³), y a la derecha, en morado, lo que piden por inciso. Sin LaTeX crudo (nada de `$`, `\frac`, `\text`). Al pasar al primer paso que no es «Datos», esas etiquetas desaparecen.
- **H7. Integrales.** En paredes, compuertas y conducción, el pedacito (franja dh, disco dy, cascarón dr) barre de un límite del dibujo al otro con su medida escrita, y la barra «Suma de…» crece hasta llegar al resultado de la lección (por ejemplo, T-INT0: F = 156,8 kN de 156,8 kN).
- **H8. Movimiento, temas y tamaño.** «Animar» mueve y «Pausar» detiene; con «menos movimiento» de Windows no hay animación y el botón dice «Avanzar paso». La escena se ve bien en Claro, Oscuro y Contraste, al 200 % de zoom y en «Ampliar». Sin WebGL aparece el dibujo 2D de la lección.

### I. Selección única

- **I1.** Cada opción se descarta con un motivo concreto, y la correcta queda marcada.
- **I2.** La respuesta coincide con la clave oficial (`cotejo` en el JSON). Si la lección avisa que la clave publicada es de otra versión, comprobá que el aviso esté.

### J. Cierre

- **J1.** Las respuestas finales coinciden con la solución oficial cuando existe.
- **J2.** La lección llega al final (estado «fin») sin errores en la consola.

## 5. Gravedad

- **G1. Bloquea entender:** dato huérfano, paso saltado, número equivocado, fórmula ilegible, «¿De dónde salió?» que no funciona.
- **G2. Confunde:** origen lejano sin señalar, voz desfasada, subrayado corrido, «¿Por qué?» flojo.
- **G3. Cosmético:** espacios, alineación, colores.

## 6. Cómo anotar cada hallazgo

Una línea JSON por hallazgo en `build/auditoria_visual/hallazgos.jsonl`:

```json
{"leccion": "O26-8", "paso": 6, "titulo_paso": "Paso 3 · Separar variables", "renglon": "texto o TeX del renglón", "codigo": "C1", "gravedad": "G1", "que_pasa": "El 2L del límite de arriba aparece sin decir de dónde sale.", "como_reproducir": "Abrir O26-8, paso 6, tercer renglón", "evidencia": "build/auditoria_visual/capturas/O26-8_p6.png", "propuesta": "Agregar el renglón «y va de −L a +L, las dos caras planas» antes de integrar."}
```

- **Capturas:** solo de los hallazgos, en `build/auditoria_visual/capturas/<ID>_p<n>.png`.
- **Propuesta:** concreta. Qué renglón falta y qué diría, qué frase hay que subrayar, qué porqué falta.

## 7. Entrega y cierre

1. **`build/auditoria_visual/INFORME.md`:**
   - **Resumen arriba, de 15 líneas como máximo:** cuántas lecciones se revisaron de verdad (y cuáles no), cuántos hallazgos hay por gravedad y por código, y las 10 lecciones peores.
   - Detalle por lección, en el orden del plan.
   - Qué no se pudo verificar y por qué (por ejemplo, «Encerrar» sin conexión).
2. **No arregles nada.** Si un arreglo es obvio, va como propuesta dentro del hallazgo.
3. **Cerrar el servidor y restaurar el progreso del estudiante:**
   ```powershell
   Stop-Process -Id (Get-Content build\auditoria_visual\servidor_pid.txt) -Confirm:$false
   Copy-Item build\auditoria_visual\progreso_respaldo.json private_local\tutor_progreso.json -Force
   (Get-FileHash private_local\tutor_progreso.json -Algorithm SHA256).Hash -eq (Get-Content build\auditoria_visual\progreso_hash.txt)
   ```
   La última línea tiene que dar `True`. Si da `False`, avisá: el progreso no quedó igual.
4. **Borrar los temporales:** `arranque.json`, `servidor.log`, `servidor_err.log`, `servidor_pid.txt`, `progreso_respaldo.json` y `progreso_hash.txt`. Borrá estos dos últimos solo después de que el paso 3 haya dado `True`. Quedan `INFORME.md`, `hallazgos.jsonl`, `avance.json`, `automatico/` y `capturas/`.
5. **Puerta de cierre:** `D:\DevTools\Python311\python.exe tools/check_workspace_hygiene.py` tiene que dar código 0.

## 8. Función de control por lección (pegar en la consola)

Corre sola mientras mirás la lección. `lanzarAuditoria('ID')` la arranca, y `__aud` muestra cómo va y el resultado. Cada paso espera hasta 90 segundos a que termine de escribirse.

```js
window.auditarLeccion = async function auditarLeccion(id, { esperaMax = 90000 } = {}) {
  const T = window.__tutor;
  const tok = new URLSearchParams(location.search).get('token');
  const j = await fetch(`/api/banco/${encodeURIComponent(id)}?token=${tok}`).then((r) => r.json());
  await T.acciones.abrir(id);
  const pausa = (ms) => new Promise((r) => setTimeout(r, ms));
  // Cada intento de subrayar un dato queda registrado, con si encontró la frase o no
  const intentos = [];
  const enun = T.rep.enun;
  if (enun && !enun.__vigilado) {
    const original = enun.marcarDato.bind(enun);
    enun.marcarDato = (a, ...resto) => { const m = original(a, ...resto); intentos.push({ frase: typeof a === 'string' ? a : a?.frase, ok: Boolean(m) }); return m; };
    enun.__vigilado = true;
  }
  const out = { id, titulo: j.titulo, recorte: Boolean(j.recorte), pasos: [] };
  for (let i = 0; i < j.pasos.length; i += 1) {
    const p = j.pasos[i];
    const desde = intentos.length;
    T.rep.irA(i);
    const t0 = Date.now();
    while (Date.now() - t0 < esperaMax) { await pausa(400); if (['espera', 'fin', 'pausado'].includes(T.rep.estado)) break; }
    out.pasos.push({
      n: i + 1, titulo: p.titulo, termino: T.rep.estado, segundos: Math.round((Date.now() - t0) / 1000),
      porque: Boolean(p.porque), origen: (p.origen || []).length, despacio: (p.lento || []).length,
      datos: p.tramos.flatMap((t) => t.acc.filter((a) => a.a === 'dato')).length,
      subrayadoFallido: intentos.slice(desde).filter((x) => !x.ok).map((x) => x.frase),
      desconocidos: [...document.querySelectorAll('[data-desconocidos]')].map((e) => e.dataset.desconocidos),
      desbordes: [...document.querySelectorAll('.linea-mate')].filter((e) => e.scrollWidth > e.clientWidth + 2).length,
      crudo: [...document.querySelectorAll('.linea-texto, .lista-mano, .tabla-caja')]
        .map((e) => e.textContent).filter((s) => /\\[a-zA-Z]{2,}|\$/.test(s)).slice(0, 3),
    });
  }
  return out;
};
window.lanzarAuditoria = (id) => {
  window.__aud = { id, estado: 'corriendo', inicio: Date.now() };
  window.auditarLeccion(id)
    .then((r) => { window.__aud = { id, estado: 'listo', r }; })
    .catch((e) => { window.__aud = { id, estado: 'error', e: String(e) }; });
};
```

**Cómo leer el resultado**

- `subrayadoFallido` con frases: esas frases no se encontraron en el enunciado y no se subrayaron (B1/B2). Que se subraye no garantiza que caiga en el lugar correcto: eso se mira en pantalla (B2).
- `desconocidos` con algo: hay una fórmula que la pizarra no sabe dibujar (E1).
- `desbordes` mayor que 0: un renglón se sale del ancho (E2).
- `crudo` con algo: LaTeX a la vista (E1).
- `porque: false`: falta el «¿Por qué?» (G1).
- `origen: 0` en un paso que trae valores de otro lado: hay que revisar C4 a mano.
- `termino` distinto de «espera» o «fin»: el paso no terminó en 90 s (J2). Si pasa en **todos** los pasos y cada uno tarda justo 90 s, el problema no es la lección: la ventana del navegador está oculta o minimizada y el tutor no termina de escribir aunque el audio ya acabó. Hacela visible y repetí la lección.

## 9. Toda la app: cada pantalla y cada ventana

Lo mismo que con las lecciones, pero en el resto de la app. Cada defecto es un hallazgo con un código K, con la gravedad de la sección 5 y el formato de la sección 6. En vez de la lección, se anota la pantalla:

```json
{"leccion": "-", "pantalla": "Biblioteca", "tema": "dark", "zoom": 200, "codigo": "K4", "gravedad": "G2", "que_pasa": "El nombre de cada grupo es gris sobre gris y casi no se lee.", "como_reproducir": "__tutor.acciones.biblioteca(), tema oscuro, zoom 200 %", "evidencia": "build/auditoria_visual/capturas/app_biblioteca_dark_200.png", "propuesta": "Usar el color de texto principal del tema para los títulos de grupo."}
```

### 9.1 Cuidados

Esta parte toca cosas que el estudiante usa todos los días:

- **No guardes, no exportes, no borres y no subas nada.** Cada ventana se abre, se revisa y se cierra con Cancelar o Esc.
- **Archivos y micrófono.** El botón de adjuntar (`btn-adjuntar`) abre el cuadro de archivos: cancelalo. No concedas el micrófono (`btn-mic`). Lo que dependa de eso va como «no verificable».
- **Claves y cuentas.** Si una pantalla pide una clave de API o una cuenta, no escribas nada: anotalo.
- **Ajustes.** Anotá cómo estaban y dejalos igual al terminar. Para probar los temas, usá `__tutor.aplicarTema(...)`, que cambia el tema sin guardarlo.

### 9.2 El tutor, pantalla por pantalla

Con el servidor de la sección 1 y la ventana visible:

| Pantalla | Cómo abrirla | Qué probar, además de la lista K |
|---|---|---|
| Inicio («Hoy») | `__tutor.acciones.inicio()` | Que el ejercicio que propone sea el que toca según el calendario del plan. |
| Preparación | `__tutor.acciones.preparacion()` | Los 11 días con sus fechas, y que cada ejercicio abra su lección. |
| Biblioteca | `__tutor.acciones.biblioteca()` | Cada filtro (Recomendados, Superexamen, Exámenes y PDFs, Me faltan), los grupos por tema y el código de cada lección. Que «Exámenes y PDFs» liste los documentos y abra sus recortes. |
| Estudio del formulario | `__tutor.acciones.estudiarFormulario()` | Cada casilla de la hoja dibujada como fórmula, sin LaTeX crudo. |
| Simulacro | `__tutor.acciones.simulacro()` | Que se pueda empezar, responder, terminar y salir. |
| Lección | `await __tutor.acciones.abrir('O26-8')` | **Pizarra:** la barra de abajo (`btn-anterior`, `btn-repetir`, `btn-pausa`, `btn-siguiente`, `btn-ayuda`, `btn-mic`, la caja de preguntar) y los puntos de progreso. **Barra izquierda:** lápiz, Encerrar, borrar el lápiz (`btn-borrar-lapiz`) y el zoom (`btn-menos`, `btn-mas`). **Además:** el panel Apoyo (`btn-apoyo`), el modo enfoque (`btn-enfoque` o la tecla F), el menú Ayuda (tecla A), el menú de Encerrar, los avisos y la pantalla de cierre al terminar. |
| Ajustes | botón `btn-ajustes` | Voz, letra, tema y ritmo: cada control hace lo que dice. Probá cada estilo de letra en una lección. |

**Hacé la recorrida completa en cada combinación:**

- los tres temas: `__tutor.aplicarTema('light')`, `('dark')` y `('high-contrast')`;
- la ventana a 1280 × 800 y a pantalla completa;
- el zoom del navegador (Ctrl + y Ctrl −) al 100 %, 150 % y 200 %. Así se ve qué pasa con letra grande y con la escala de Windows alta.

### 9.3 CuadernU nativo

El ejecutable es `build/studio-x64/Release/CuadernU.exe`.

- **Si no existe,** no lo compiles sin permiso. CuadernU se compila solo en `build/studio-x64` (AGENTS.md, regla 5).
- **Si el estudiante ya lo tiene abierto,** no lo cierres: preguntá.
- **El tutor se abre** desde el botón «Estudio» de la barra → «Pizarra Tutor Audiovisual (Voz + Lazo + PDF)…». En modo examen ese botón puede estar desactivado.
- **Dentro de CuadernU está el progreso real.** El tutor es la misma web, dentro de WebView2, pero trabaja con el progreso real. Hacé el respaldo y el hash de la sección 1 antes de abrirlo.
- **Al terminar,** cerrá CuadernU si lo abriste vos, y recién después restaurá el progreso (sección 7): al cerrarse, puede volver a guardarlo.

Recorré:

- la ventana principal: barra, menús, lienzo y paneles;
- **cada ventana o diálogo que se abra desde los menús** (configuración, exportar, ayuda, estudio, historial), con la lista K;
- el tutor dentro de CuadernU: lo mismo de 9.2, por lo menos en una lección completa;
- si hay un monitor 4K, la escala de Windows al 150 % y al 200 %. Si no lo hay, anotalo como no verificado.

Las calculadoras CASIO son otro programa y no entran, salvo que el dueño lo pida.

### 9.4 Lista de control K

- **K1. Se ve roto:**
  - texto cortado, encimado o que se sale;
  - barra de desplazamiento horizontal;
  - algo tapado por otro panel;
  - iconos en blanco o cuadros vacíos por un símbolo que falta.
- **K2. Código a la vista:** `$`, `\frac`, `{}`, `undefined`, `NaN`, `[object Object]`, ids internos o mensajes de depuración.
- **K3. Botón o tecla que falla:**
  - no hace lo que dice, o no hace nada;
  - no hay forma de volver;
  - Tab, Enter o Esc no funcionan donde deberían.
- **K4. Tema:**
  - algo ilegible en claro, oscuro o contraste;
  - gris sobre gris;
  - colores sin sentido;
  - el foco del teclado no se ve.
- **K5. Escala:**
  - al 150 % o 200 % algo se rompe, se corta o queda diminuto;
  - grandes vacíos con controles chicos.
- **K6. Inconsistencia entre pantallas:**
  - la misma acción con nombres distintos;
  - botones, radios, espaciados o letra que cambian sin razón;
  - una ventana que no sigue el tema.
- **K7. Estética de IA** (los 18 antipatrones de AGENTS.md):
  - emojis como iconos;
  - jerga interna («LaTeX», «Core oficial», «Registro visual»);
  - demasiados botones a la vista;
  - neón o resplandores;
  - tarjetas dentro de tarjetas;
  - todo en píldora;
  - paneles de métricas;
  - decoración sin función;
  - controles repetidos.
- **K8. Animaciones y efectos:**
  - parpadeos;
  - cosas que saltan de lugar al cargar;
  - animaciones que no terminan;
  - transiciones que tapan contenido;
  - el lápiz que deja rastros;
  - transparencias o desenfoques que vuelven ilegible el texto.
- **K9. Mensajes y errores:**
  - errores en la consola (F12) o en `build/auditoria_visual/servidor_err.log` mientras usás la pantalla;
  - avisos que no se entienden;
  - avisos que no dicen la verdad, como «guardado» cuando no se guardó.
- **K10. Matemática como texto plano:** fórmulas, unidades o letras griegas escritas como texto común en vez de notación matemática (AGENTS.md, «Tipografía matemática»).

### 9.5 Evidencia obligatoria

- **Una captura por pantalla en cada tema,** y otra en tema claro con zoom al 200 %, en `build/auditoria_visual/capturas/app_<pantalla>_<tema>_<zoom>.png`. Lo mismo para cada ventana de CuadernU.
- **En el `INFORME.md`, una tabla de pantallas por temas** que diga en cada casilla «revisada» o «no revisada», y por qué.

Sin esa tabla completa y sus capturas, la recorrida no cuenta como hecha.

## 10. Texto corto para pegar en Antigravity

> Leé `docs/INSTRUCCIONES_AUDITORIA_VISUAL_PASO_A_PASO.md` y seguilo al pie de la letra. Es una auditoría: no cambies código, no hagas commits, no crees ramas y no toques `procedencia.py`, `reproductor.js`, `asistente.js` ni `banco_api.py`, que los está trabajando otra sesión. Respaldá el progreso del estudiante antes de abrir el tutor o CuadernU y dejalo idéntico al final. Primero hacé la recorrida de toda la app de la sección 9: cada pantalla del tutor y cada ventana de CuadernU, en tema claro, oscuro y contraste, y con zoom al 100, 150 y 200 %, con una captura de cada una. Después seguí con las 76 lecciones del plan «Reposición II-2026» en el orden del calendario, en lotes de 10. En cada paso buscá defectos visuales, pasos intermedios que falten y cualquier número o letra cuyo origen no se señale (subrayado en el enunciado, renglón anterior o casilla de la hoja). Probá «¿De dónde salió?» (tecla O) y «Encerrar» en cada valor dudoso. No guardes, no exportes ni borres nada en la app. Entregá `build/auditoria_visual/INFORME.md`, con un resumen de 15 líneas arriba y la tabla de pantallas por temas, y `hallazgos.jsonl`, con una línea por hallazgo y una propuesta concreta de arreglo. Decí con honestidad qué no revisaste.
