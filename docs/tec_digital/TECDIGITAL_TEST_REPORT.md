# Reporte de Pruebas y Verificación — TEC Digital / CuadernU

> **Fecha de verificación final:** `2026-11-26`  
> **Compilación:** Release x64 (MSVC / CMake, `D:\Proyectos\CalculadorasUnificadas\build\studio-x64`)  
> **Política de seguridad:** `MUTATION_MASK=0` en toda la suite automatizada y en la sincronización real de solo lectura. Cero credenciales, cookies, carnés o datos personales reales en el repositorio o entregables.

---

## 1. Ejecución Completa de CTest (`16/16 PASSED`, `0 FAILED`, `57.54 s`)

| # | Suite CTest | Estado | Duración | Aserciones detalladas |
|---:|---|---|---:|---|
| 1 | `DocumentCoreTest` | **CONFIRMED (Passed)** | 11.59 s | — |
| 2 | `TintaNucleoTest` | **CONFIRMED (Passed)** | 0.09 s | — |
| 3 | `CuadernuInteropTest` | **CONFIRMED (Passed)** | 0.45 s | — |
| 4 | `PrintoutTest` | **CONFIRMED (Passed)** | 30.81 s | — |
| 5 | `KnowledgeGraphAndBasesTest` | **CONFIRMED (Passed)** | 1.00 s | — |
| 6 | `FlexcilSmartScriptAndMathTest` | **CONFIRMED (Passed)** | 5.62 s | — |
| 7 | `AudioCollabSyncAndClipperTest` | **CONFIRMED (Passed)** | 0.45 s | — |
| 8 | `RealE2EClosureTest` | **CONFIRMED (Passed)** | 0.83 s | — |
| 9 | `AiVaultA11yStudyTest` | **CONFIRMED (Passed)** | 0.51 s | — |
| 10 | `PerfAndRichMediaTest` | **CONFIRMED (Passed)** | 2.16 s | — |
| 11 | `GeminiNotebookAndInlineUrlTest` | **CONFIRMED (Passed)** | 1.51 s | — |
| 12 | `TecDigitalIntegrationTest` | **CONFIRMED (Passed)** | 0.74 s | **`453 passed, 0 failed`** |
| 13 | `TutorWhiteboardTest` | **CONFIRMED (Passed)** | 0.05 s | — |
| 14 | `TecDigitalFoundationTest` | **CONFIRMED (Passed)** | 0.53 s | **`518 passed, 0 failed`** |
| 15 | `TecDigitalAcademicTest` | **CONFIRMED (Passed)** | 1.09 s | **`599 passed, 0 failed`** |
| 16 | `academic_horarios_tec_tests` | **CONFIRMED (Passed)** | 0.09 s | **`346 passed, 0 failed`** |
| | **TOTAL CTest / Subsistema TEC** | **16/16 PASSED (0 failed)** | **57.54 s** | **`1,916 passed, 0 failed`** |

---

## 2. Sincronización Real en Vivo — Estrictamente Solo Lectura (`MUTATION_MASK=0`)

Ejecutada mediante `TecDigitalIntegrationTest.exe --sync-real-account-to-cuadernu` con cuenta estudiantil real y máscara de mutación estrictamente en `0`:

| Métrica verificada | Valor observado | Estado |
|---|---|---|
| Autenticación + restauración de bóveda DPAPI | `REAL_SYNC_OK=1`, `SAVED_OK=1`, `VAULT_RESTORE_OK=1` | **CONFIRMED** |
| Máscara de mutación | `MUTATION_MASK=0` (0 escrituras ejecutadas) | **CONFIRMED** |
| Espacios académicos y árboles de carpetas indexados | `34/34` espacios (6 namespaces: clases, cátedras, escuelas, subgrupos, programas, comunidades) | **CONFIRMED** |
| Biblioteca Global Unificada y metadatos de archivos | **`2,972` archivos** (`2,234` con fecha de creación, modificación, revisión y autor) + `718` carpetas (`598` con fechas) + `48` archivos personales = **`3,043` archivos** en Biblioteca Global | **CONFIRMED** |
| Portadas, pestañas dinámicas (`?page_num=N`), subpáginas semanales (`S2..S16`) e infografías | `14` portadas, `17` páginas en cátedra semanal con etiquetas reales `S2..S16`, `0` `"loupe"`, `0` `"play_arrow"`, `110` infografías detectadas (`12` cacheadas localmente para GDI+) | **CONFIRMED** |
| Cursos con evaluaciones / ítems / nombres reales | `6` cursos, `52` ítems, **`52/52` nombres reales** (`assignNameText` sin comillas), `45` notas base 100 | **CONFIRMED** |
| Categorías de evaluación con título y subtotal reales | **`18/18` categorías reales** (`.asign_student` + `.sub_total` numérico) | **CONFIRMED** |
| Sincronización automática de rúbricas (cero registro manual) | **`14` rúbricas sincronizadas**, `13` calificadas, `9` comentarios por criterio (limpios sin `"Copiar comentario"`) | **CONFIRMED** |
| Radar de cambios, notificaciones y suscripciones | `23` eventos en Radar de cambios (`monitorIntervalMinutes=15`, 0% CPU en reposo), `10` notificaciones, `79` suscripciones | **CONFIRMED** |
| Calendario y horario semanal | `90` eventos de calendario + `7` eventos de curso (`0` fechas inválidas, entregas deduplicadas), `6` bloques horarios | **CONFIRMED** |
| Expediente académico + historial + promedios + DAR | `48` registros históricos, `12` promedios ponderados, `67` citas, `3` cursos DAR, `9` dimensiones de Modelo de Usuario, `13` ferias, `100` escuelas, `46` cursos pendientes de graduación, `11` niveles de malla + `6` colores oficiales | **CONFIRMED** |
| Motor GAAP / `tda-motor-juegos` | Detección nativa (`GAAP_URL_PRESENT=1`, `MJ_USER=1`) y cliente autenticado en `policy.cpp` / `tda_tds_client.cpp` | **CONFIRMED** |
| Buscador global público de horarios (`guiahorarios`) | `1,361` cursos, `4,635` grupos, `5` sedes/campus | **CONFIRMED** |

---

## 3. Auditoría Visual Completa de Interfaz guías de interfaz (`CuadernU.exe`, Porcelana Claro y Grafito Oscuro)

Se capturaron e inspeccionaron visualmente **`32` + `27` capturas previas y `16` capturas v3 + `8` capturas de re-verificación** en temas **Porcelana Claro** y **Grafito Oscuro**, validando todos los módulos del Super Plan v3:

1. ***Inicio* (Claro y Oscuro):** tarjeta *Radar de cambios y avisos* con contador, píldoras por tipo de cambio, reloj oficial sin truncamiento (`Reloj TEC: DD/MM/AAAA HH:MM · Monitor: 15 min`) y salto directo.
2. ***Cursos → Documentos* (Claro y Oscuro):** buscador instantáneo de archivos y carpetas por espacio, ordenamiento (`Recientes`, `Nombre`, `Creación`, `Tamaño`), marcas de fecha/hora y autor, historial de revisiones y alertas de carpeta.
3. ***Cursos → Resumen (Cátedras y Módulos Semanales)* (Claro y Oscuro):** portada editorial con *Avisos y comunicados recientes de cátedra*, chips de salto rápido a módulos semanales, subpáginas `S2..S16` con nombres reales (`0` `"ver másloupe"`, `0` `"play_arrow"`, `0` fugas HTML) e infografías renderizadas nativamente con GDI+.
4. ***Biblioteca → Todos los archivos (`3,043`) y Mis documentos (`48`)* (Claro y Oscuro):** catálogo global unificado con filtros por espacio, indicador `Solo directorio`, fechas/autores, y gestión completa de carpetas/archivos personales.
5. ***Trámites* (Claro y Oscuro):** *Resumen y DAR* (`+ Solicitar inclusión`, `Sincronizar DAR`), *Control de Plagio* (`Subir a Plagio...`, `Analizar`, `Ver reporte`) y *Notificaciones y Cuenta* (*Radar de cambios*, selector de frecuencia `Manual / 5 / 15 / 30 / 60 min`, `Buscar cambios ahora`, `Marcar todo leído`, sin duplicación de nombre de espacio).

---

## 4. Matriz de las 26 Categorías de Prueba (Sección 12 + Super Plan v3)

| # | Categoría de prueba | Cobertura / Evidencia | Estado |
|---:|---|---|---|
| 1 | Autenticación y gestión de sesión (`auth_client`, cookies, timeout modal 2026) | `TecDigitalFoundationTest` + `TecDigitalIntegrationTest` + live login | **CONFIRMED** |
| 2 | Navegación por espacios y 6 namespaces `/dotlrn/` | `TecDigitalAcademicTest` + crawl 500 páginas / 34 espacios | **CONFIRMED** |
| 3 | Pestañas dinámicas `?page_num=N`, subpáginas `S1..S16`, infografías GDI+ y limpieza Material Icons | `TestV3SuperPlanParsersAndPolicy` + live `17` páginas (`LOUPE_COUNT=0`) | **CONFIRMED** |
| 4 | Árbol jerárquico de Documentos (34/34 espacios), buscador local, fechas/horas, revisiones y alertas | `TestV3SuperPlanParsersAndPolicy` + live `2,972` archivos (`2,234` con fechas) | **CONFIRMED** |
| 5 | Biblioteca Global Unificada (`BuildGlobalFileCatalog`) + CRUD de *Mis documentos* | `TestV3RadarGlobalCatalogAndPersistence` + live `3,043` archivos | **CONFIRMED** |
| 6 | Evaluaciones detalladas (`assignNameText`, `.asign_student`, base 100) y simulador What-If | `TestLive2026ParsersAndContracts` + live `52/52` nombres / `18/18` categorías | **CONFIRMED** |
| 7 | Rúbricas analíticas automáticas (`#table_body_evaluation`, `Peso %`, comentarios limpios) | `TestLive2026ParsersAndContracts` + live `14` rúbricas / `13` calificadas / `9` comentarios | **CONFIRMED** |
| 8 | Radar diferencial (`ComputeSnapshotChanges`), monitor CV (0% CPU) y avisos nativos Windows | `TestV3RadarGlobalCatalogAndPersistence` + `TestStaticArchitectureAndHigGuards` | **CONFIRMED** |
| 9 | Expediente académico (`detalle_notas`, `detalle_historial`, GraphQL merge 48 filas) y Citas (67) | `TestLive2026ParsersAndContracts` + live `48` filas / `12` promedios / `67` citas | **CONFIRMED** |
| 10 | Malla curricular, prerrequisitos, `curriculumColors` y pendientes de graduación | `TestPhase2LiveProtocolAndGraphQl` + live `11` niveles / `46` pendientes / `6` colores | **CONFIRMED** |
| 11 | Horario personal (`tabla_horario`) y Buscador global público (`guiahorarios`) | `academic_horarios_tec_tests` (`346/346`) + live `1,361` cursos / `4,635` grupos | **CONFIRMED** |
| 12 | Calendario unificado (list-table sin `#at-a-glance`, fechas en español, deduplicación entregas) | `TecDigitalAcademicTest` + `TecDigitalIntegrationTest` | **CONFIRMED** |
| 13 | Noticias RSS, archivo histórico y Portada Editorial de Cátedra | `TecDigitalAcademicTest` + `TecDigitalIntegrationTest` + auditoría visual | **CONFIRMED** |
| 14 | Notificaciones, suscripciones, consentimiento informado y exportación CSV | `TecDigitalIntegrationTest` + live `10` notifs / `79` suscripciones | **CONFIRMED** |
| 15 | Control de Plagio (listado por curso, subida, generación de reporte, estados legibles) | `policy.cpp` + `tda_tds_client.cpp` + `TecDigitalIntegrationTest` | **CONFIRMED** |
| 16 | Reloj oficial del servidor (`GET /theme-td/ajax/get_date_server`) | `GetServerDateTime()` + `TecAcademicStore` + auditoría visual | **CONFIRMED** |
| 17 | Trámites DAR, Prematrícula, Modelo de Usuario, Ferias de Negocios e Inclusiones GraphQL | `policy.cpp` + `graphql_client.cpp` + `TecDigitalIntegrationTest` | **CONFIRMED** |
| 18 | Motor interactivo GAAP (`api.tec.ac.cr:3003`) y `tda-motor-juegos` | `policy.cpp` + `tda_tds_client.cpp` + `TestV3SuperPlanParsersAndPolicy` | **CONFIRMED** |
| 19 | Política granular de 10 mutaciones estudiantiles (`MUTATION_MASK=0` en pruebas/sync) | `policy.cpp` + `TestV3SuperPlanParsersAndPolicy` + `373` bloqueos en crawl | **CONFIRMED** |
| 20 | Persistencia offline v3 (`academic_snapshot.json` con Radar, fechas y monitor) y bóveda DPAPI | `TestV3RadarGlobalCatalogAndPersistence` + live `SAVED_OK=1 VAULT_RESTORE_OK=1` | **CONFIRMED** |
| 21 | Privacidad cero-PII (`student_id` nunca en disco, 0 fugas en entregables/Git) | `TecDigitalIntegrationTest` + `independent_second_audit.py` | **CONFIRMED** |
| 22 | Reconstrucción funcional local sanitizada (`reference_reconstruction/`) | `node --check` + verificación estructural en auditoría independiente | **CONFIRMED** |
| 23 | Rendimiento y arquitectura UI (0% GPU GDI+, cero `SetTimer`, cero WebView2, cero emojis) | `TestStaticArchitectureAndHigGuards` (`TecDigitalIntegrationTest`) | **CONFIRMED** |
| 24 | Auditoría visual integral Claro/Oscuro y cumplimiento guías de interfaz | `16` capturas v3 + `8` re-verificaciones; 0 defectos abiertos | **CONFIRMED** |
| 25 | Higiene del repositorio (`tools/check_workspace_hygiene.py`) | Exit code `0` verificado | **CONFIRMED** |
| 26 | Segunda auditoría independiente de cobertura y consistencia | `independent_second_audit.py` PASSED, `0` fallos, `0` fugas PII | **CONFIRMED** |
