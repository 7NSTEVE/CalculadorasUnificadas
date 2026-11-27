# Auditoría e Integración Nativa de TEC Digital en CuadernU

> **Archivos C++ auditados:** `48` | **Endpoints referenciados en C++:** `175` | **Métodos cliente/store:** `92` | **Estructuras de datos:** `101` | **Controles Win32 UI:** `96` | **Dependencia WebView:** `0` (`0% GPU` GDI+ offscreen, `0% CPU` en reposo sin `SetTimer`).

## 1. Arquitectura Modular Nativa en CuadernU

| Capa | Ruta en Repositorio | Responsabilidad |
|---|---|---|
| Transporte y Política Read-Only | `academic/tec_digital/src/policy.cpp`, `http_transport_win.cpp`, `auth_client.cpp`, `vault_win.cpp` | WinHTTP nativo, `ReadOnlyRequestPolicy` (`MUTATION_MASK=0`), bóveda DPAPI cifrada, sanitizador PII |
| Clientes Oficiales | `tda_tds_client.cpp`, `graphql_client.cpp`, `public_guia_client.cpp`, `storage.cpp` | Consumo read-only de dotLRN/AJAX, GraphQL `api.tec.ac.cr:3005`, Guía Pública `tec-appsext.itcr.ac.cr` y descargas SHA-256 |
| Parsers Deterministas | `academic/tec_digital/src/academic_parsers.cpp`, `academic_models.hpp` | Parsers puros sin estado para evaluaciones (base 100 + %), rúbricas analíticas + comentarios, expediente 9-col, citas sin `<tr>`, horario 4-partes `'-----'`, `tda-um` HTML, plagio y ferias |
| Store + Persistencia v2 + FTS5 | `studio/integrations/tec_digital/tec_academic_store.cpp`, `material_indexer.cpp` | Caché offline JSON v2 sin pérdidas (cero `student_id` en disco), sincronización automática de rúbricas, carga bajo demanda de cursos históricos, índice global 4,635+ grupos y búsqueda FTS5 |
| Interfaz guías de interfaz + material translúcido | `studio/app/studio_tec_digital_dialog.cpp`, `studio_tec_digital_views.hpp` | 7 secciones nativas Win32 + GDI+ (`Inicio`, `Cursos`, `Calendario`, `Expediente y Malla`, `HorariosTEC`, `Biblioteca Offline`, `Trámites y Perfil`), Porcelana Claro / Grafito Oscuro / Alto Contraste, DPI 4K `PerMonitorV2` |

## 2. Matriz de Trazabilidad (TEC Digital ↔ Reconstrucción Local ↔ CuadernU)

| Superficie TEC Digital | Reconstrucción Local (`reference_reconstruction/`) | Integración Nativa CuadernU | Estado |
|---|---|---|---|
| Portal Inicio + Notificaciones | Vista `Inicio` + campana | `HubSection::Resumen` (`PaintSummaryCanvas`) | **IMPLEMENTED** |
| Cursos, Cátedras, Escuelas, DOP, Programas (`34` espacios) | Vista `Cursos y Espacios` + pestañas dinámicas | `HubSection::Cursos` (`PaintCourseListCanvas`, `PaintCourseDetailCanvas`) | **IMPLEMENTED** |
| Evaluaciones (Base 100 + %) + What-If + Subida | Subvista `Evaluaciones` + simulador | `Cursos → Evaluaciones` (Base 100 verde/rojo + aporte % + subida RAII confirmada) | **IMPLEMENTED** |
| Rúbricas Analíticas (Pesos, Logrado, Comentarios) | Subvista `Rúbrica Analítica` | Sincronización automática en `SyncAll` y `SyncCourseOnDemand` + matriz GDI+ inline | **IMPLEMENTED** |
| Documentos (`file-storage`) y Mis Documentos | Vista `Documentos` + árbol `GL_FOLDER_ID` | `Cursos → Documentos` + `HubSection::BibliotecaOffline` (FTS5) | **IMPLEMENTED** |
| Calendario Portal + Curso + Citas + Horario | Vista `Calendario (Mes/Semana/Agenda)` | `HubSection::Calendario` (3 vistas + `.ics`) | **IMPLEMENTED** |
| Expediente, 44+ Notas, Promedios 12 sem, RN, Malla | Vista `Expediente y Malla` | `HubSection::ExpedienteYMalla` (gráfico ponderado + 3 sub-tabs + Malla) | **IMPLEMENTED** |
| Guía Pública Horarios (90 escuelas, 5 sedes) | Vista `HorariosTEC` | `HubSection::HorariosVivo` (Buscador global + Planificador) | **IMPLEMENTED** |
| Trámites (DAR, Plagio, `tda-um`, Eval Docente, Ferias, Suscripciones) | Vista `Trámites y Perfil` (5 sub-tabs) | `HubSection::TramitesYPerfil` (`PaintTramitesCanvas` 5 segmentos) | **IMPLEMENTED** |
