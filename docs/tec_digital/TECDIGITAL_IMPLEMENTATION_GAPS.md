# Auditoría de Brechas de Implementación en CuadernU

> **Fecha:** `2026-11-26`  
> Contraste empírico entre la cartografía recursiva (`500` páginas visitadas, `34` espacios, `198` bundles JS de primera parte, `260` contratos API) y la implementación nativa C++ de **CuadernU (Windows)** tras completar el **Super Plan v3**.

---

## 1. Brechas Detectadas y Resueltas de Punta a Punta (Motor → Store → Caché v3 → UI)

1. **Buscador por curso/cátedra, fechas/horas de creación y modificación, historial de revisiones y alertas de carpeta** — **CONFIRMED (RESUELTO)**
   - `FileStorageWalker::ParseFolderJson` y `ParseFileDetailHtml` capturan `creation_date_ansi`, `last_modified_ansi`, `live_revision`, `author_name` y `subscription_active` (`2,234` archivos y `598` carpetas con fechas en la cuenta real).
   - *Cursos → Documentos* incorpora búsqueda instantánea en todo el árbol del espacio, ordenamiento por fecha/tamaño/nombre, historial de versiones y alertas de carpeta por clic directo.
2. **Biblioteca Global Unificada de Todos los Archivos (`3,043` archivos) + gestión de *Mis documentos*** — **CONFIRMED (RESUELTO)**
   - `TecAcademicStore` recorre los árboles de carpetas de **34/34 espacios accesibles** (`2,972` archivos en cursos, cátedras, escuelas, grupos y comunidades + `48` archivos personales + adjuntos de evaluaciones).
   - *Biblioteca → Todos los archivos* filtra por tipo de espacio, señala archivos presentes únicamente en directorio (`Solo directorio`) y permite descarga directa; *Mis documentos* soporta crear carpetas, subir archivos y eliminar elementos.
3. **Radar de cambios, monitor automático (0% CPU en reposo) y notificaciones nativas de Windows** — **CONFIRMED (RESUELTO)**
   - `ComputeSnapshotChanges` detecta archivos nuevos/modificados, carpetas nuevas, evaluaciones nuevas, evaluaciones recién calificadas, rúbricas calificadas, avisos y módulos semanales.
   - `ConfigureAutoMonitor` utiliza `std::condition_variable::wait_for` (cero `SetTimer`, 0% CPU inactivo) con intervalos `Manual / 5 / 15 / 30 / 60 min`, conectado a `Shell_NotifyIconW` para avisos nativos en Windows y al panel de Radar en *Inicio* y *Trámites → Notificaciones y Cuenta*.
4. **Módulos semanales profundos de cátedra (`S2..S16`), infografías GDI+ nativas y portada editorial** — **CONFIRMED (RESUELTO)**
   - `ParseSpacePageHtml` limpia nodos `<i/span class="material-icons">` (`0` `"loupe"`, `0` `"play_arrow"`), resuelve títulos reales de tarjetas (`S2 M.A.S y sus aplicaciones` … `S16 Segunda ley de la termodinámica`), filtra botones `go_back`, y `TecAcademicStore` pre-cachea infografías para renderizado nativo GDI+ (`110` detectadas, `12` cacheadas).
   - *Cursos → Resumen* ofrece portada editorial dedicada para cátedras con avisos recientes, botón `Ver todos` y salto directo a módulos semanales.
5. **Automatización estudiantil máxima legítima (10 grupos en `policy.cpp`, 16 acciones en Store/UI) y soporte nativo GAAP (`tda-motor-juegos`)** — **CONFIRMED (RESUELTO)**
   - Entregas/eliminación de tareas, alertas de carpeta, lectura/borrado de notificaciones, suscripciones, CRUD de *Mis documentos*, sincronización DAR (`actualizar_cursos2`), subida y análisis en Control de Plagio, inclusiones GraphQL, prematrícula, ferias y perfil/consentimientos están completamente cableados en cliente, política, almacén e interfaz (1 clic en acciones reversibles y confirmación breve en acciones destructivas o trámites formales; `MUTATION_MASK=0` estricto en pruebas y sincronización automática).
   - Soporte nativo de `tda-motor-juegos` y conexión interactiva con GAAP (`api.tec.ac.cr:3003`) integrados.
6. **Rúbricas analíticas automáticas, evaluaciones (`52/52` nombres, `18/18` categorías), calendario real, expediente completo y los 6 huecos previos de cableado** — **CONFIRMED (RESUELTO)**
   - Verificados en `16/16` suites CTest (`1,916` aserciones TEC, `0` fallos) y auditoría visual Claro/Oscuro.

---

## 2. Límites Estrictos de Acceso y Seguridad (No son Brechas de Implementación)

- **Límites del servidor y roles no estudiantiles:** CuadernU respeta estrictamente los controles de acceso del servidor (`HTTP 403`, borradores no publicados por docentes y módulos exclusivos de rol docente/coordinador/administrador, documentados en `TECDIGITAL_UNKNOWN_COMPONENTS.md`).
- **Seguridad en pruebas automatizadas (`MUTATION_MASK=0`):** las pruebas y el comando `--sync-real-account-to-cuadernu` mantienen `MUTATION_MASK=0` para no alterar datos reales de la cuenta durante verificaciones automatizadas; las acciones estudiantiles se habilitan exclusivamente ante interacción directa del usuario en la aplicación.
- **Cero stubs, cero mocks en producción, cero WebView2, cero `SetTimer`:** verificado estáticamente (`TestStaticArchitectureAndHigGuards` y `independent_second_audit.py`).
