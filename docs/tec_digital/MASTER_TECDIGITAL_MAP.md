# Mapa Maestro de TEC Digital (2026) para CuadernU

> **Fecha de verificación:** 2026-11-26 | **Política de red:** Estrictamente solo lectura (`MUTATION_MASK=0`) | **Privacidad:** 100% sanitizado (cero credenciales, cookies, JWT, carnés, correos ni notas privadas).

## 1. Resumen Ejecutivo y Alcance Empírico

Este mapa consolida la cartografía recursiva y el análisis estático de bundles de la superficie **observable** de TEC Digital (`https://tecdigital.tec.ac.cr`, `https://api.tec.ac.cr:3005/graphql` y `https://tec-appsext.itcr.ac.cr/guiahorarios/`) para una cuenta estudiantil autenticada:

| Dimensión Cartografiada | Conteo Verificado | Fuente / Evidencia |
|---|---:|---|
| Espacios académicos accesibles (clases, cátedras, escuelas, DOP, programas) | **34** | `MASTER_TECDIGITAL_MAP.json` |
| Páginas concretas visitadas en vivo (Pass 1 + Pass 2) | **500** | `TECDIGITAL_ROUTE_REGISTRY.json` |
| Patrones de ruta únicos consolidados | **235** | `TECDIGITAL_ROUTE_REGISTRY.json` |
| Módulos institucionales catalogados | **32** | `TECDIGITAL_MODULE_REGISTRY.json` |
| Bundles JS de primera parte analizados estáticamente | **198** | `TECDIGITAL_HOOKS_REGISTRY.json` |
| Contratos API totales documentados (HTTP/AJAX + GraphQL) | **260** (`212` HTTP + `48` GraphQL) | `TECDIGITAL_API_REGISTRY.json` |
| Archivos con hooks/eventos/componentes frontend documentados | **140** | `TECDIGITAL_HOOKS_REGISTRY.json` |
| URLs mutantes o restringidas interceptadas por `ReadOnlyRequestPolicy` | **373** | `TECDIGITAL_BLOCKED_ITEMS.md` |
| Servicios externos / terceros referenciados | **116** | `MASTER_TECDIGITAL_MAP.json` |
| Esqueletos DOM sanitizados por módulo | **27** | `evidence/dom_skeletons/` |
| Esquemas JSON observados en vivo (cero valores privados) | **14** | `evidence/schemas/observed_json_schemas.json` |

## 2. Inventario de Módulos Institucionales (32 Módulos)

| ID | Módulo | Ruta Base | Páginas | APIs | Estado CuadernU | Pantalla CuadernU |
|---|---|---|---:|---:|---|---|
| `MOD-01` | **Inicio y Portal Principal (dotLRN)** | `/dotlrn/index` | 12 | 10 | `IMPLEMENTED` | Inicio (HubSection::Resumen) |
| `MOD-02` | **Cursos y Clases Matriculadas (Actuales e Históricos)** | `/dotlrn/classes/{dept}/{course}/{group}/` | 94 | 0 | `IMPLEMENTED` | Cursos (HubSection::Cursos) |
| `MOD-03` | **Cátedras Institucionales** | `/dotlrn/catedras.tec/{slug}/` | 20 | 0 | `IMPLEMENTED` | Cursos (filtro Cátedras) |
| `MOD-04` | **Escuelas, Asociaciones y Subcomunidades** | `/dotlrn/escuela/{escuela}/[{subgrupo}/]` | 57 | 0 | `IMPLEMENTED` | Cursos (filtro Comunidades) |
| `MOD-05` | **DOP y Bienestar Estudiantil (PSED-NE)** | `/dotlrn/dop/[psed/]` | 15 | 0 | `IMPLEMENTED` | Cursos (filtro Comunidades) |
| `MOD-06` | **Programas Institucionales (PAySE / MATEC)** | `/dotlrn/programas.tec/{programa}/` | 13 | 0 | `IMPLEMENTED` | Cursos (filtro Comunidades) |
| `MOD-07` | **Espacios Estudiantiles por Campus** | `/dotlrn/estudiantes.tec/[estudiantes.{sede}/]` | 17 | 0 | `IMPLEMENTED` | Cursos (filtro Comunidades) |
| `MOD-08` | **Documentos y Mis Documentos (file-storage)** | `/dotlrn/file-storage/ y <espacio>/file-storage/` | 35 | 21 | `IMPLEMENTED` | Cursos → Documentos + Biblioteca Offline |
| `MOD-09` | **Evaluaciones, Calificaciones y Entregas (tda-ce-estudiante)** | `<curso>/evaluation/tda-ce-estudiante/tda-index` | 17 | 7 | `IMPLEMENTED` | Cursos → Evaluaciones (Nota base 100 + What-If + Subida confirmada) |
| `MOD-10` | **Rúbricas Analíticas de Evaluación (tda-rubrica)** | `/tda-rubrica/tda_evaluacion_rubrica` | 1 | 20 | `IMPLEMENTED` | Cursos → Evaluaciones (Sincronización automática + Matriz guías de interfaz) |
| `MOD-11` | **Expediente Estudiantil, Notas, Promedios y Citas** | `/tda-expediente-estudiantil/` | 2 | 24 | `IMPLEMENTED` | Expediente y Malla (HubSection::ExpedienteYMalla) |
| `MOD-12` | **Malla Curricular Interactiva (tds-curriculum-exp)** | `/tds-curriculum-exp/ajax/*` | 0 | 15 | `IMPLEMENTED` | Expediente → Malla Curricular |
| `MOD-13` | **Guía Pública de Horarios de Todo el TEC** | `https://tec-appsext.itcr.ac.cr/guiahorarios/` | 4 | 0 | `IMPLEMENTED` | HorariosTEC (HubSection::HorariosVivo — Buscador Global + Planificador) |
| `MOD-14` | **Calendario Unificado (Portal y por Curso)** | `/dotlrn/calendar/view y <curso>/calendar/view` | 58 | 0 | `IMPLEMENTED` | Calendario (HubSection::Calendario — Mes / Semana / Agenda + ICS) |
| `MOD-15` | **Notificaciones y Suscripciones** | `/tda-notifications/ y /notifications/manage` | 3 | 5 | `IMPLEMENTED` | Inicio + Trámites → Notificaciones y Cuenta |
| `MOD-16` | **Noticias y Archivo Histórico de Noticias** | `<espacio>/news/ y <espacio>/news/?view=archive` | 78 | 0 | `IMPLEMENTED` | Cursos → Novedades |
| `MOD-17` | **Foros de Discusión** | `<curso>/forums/` | 17 | 0 | `IMPLEMENTED` | Cursos → Actividades |
| `MOD-18` | **Mensajería SMS de Curso y Portal** | `<curso>/tda-sms/ y /tda-sms-configuration/` | 17 | 3 | `IMPLEMENTED` | Cursos → Actividades |
| `MOD-19` | **Integrantes y Equipo Docente** | `<curso>/members y /dotlrn/.../ajax/load_members` | 17 | 0 | `IMPLEMENTED` | Cursos → Novedades y Personas |
| `MOD-20` | **Control de Plagio (Ephorus / Turnitin)** | `/tda-control-plagio/` | 2 | 18 | `PARTIAL_BEING_COMPLETED` | Trámites → Control de Plagio (corrigiendo rutas reales cargaMisDocs/cargaDocsCurso en Paso 7) |
| `MOD-21` | **Gestor de Inclusiones y Becas (SPA + GraphQL)** | `/tda-inclusion-manager/ + api.tec.ac.cr:3005/graphql` | 1 | 51 | `IMPLEMENTED` | Trámites → Resumen y DAR + Expediente |
| `MOD-22` | **Prematrícula** | `/tda-prematricula/` | 2 | 28 | `IMPLEMENTED` | Trámites → Resumen y DAR |
| `MOD-23` | **Gestor de Inscripción de Eventos y Talleres** | `/tda-gestor-inscripcion/` | 2 | 5 | `IMPLEMENTED` | Trámites → Resumen y DAR |
| `MOD-24` | **Modelo de Usuario (tda-um)** | `/tda-um/info` | 3 | 11 | `IMPLEMENTED` | Trámites → Modelo de Usuario |
| `MOD-25` | **Feria de Ideas de Negocios** | `/tda-feria-negocios/registro_proyectos` | 2 | 23 | `IMPLEMENTED` | Trámites → Eval. Docente y Ferias |
| `MOD-26` | **Evaluación de Desempeño Docente** | `/tda-performance-evaluation/api/*` | 1 | 0 | `IMPLEMENTED` | Trámites → Eval. Docente y Ferias |
| `MOD-27` | **Sincronización de Cursos desde DAR** | `/tda-expediente-estudiantil/ajax/actualizar_cursos y /dotlrn/ajax/actualizar_cursos` | 0 | 1 | `IMPLEMENTED` | Trámites → Resumen y DAR |
| `MOD-28` | **Motor de Juegos y Actividades GAAP** | `/tda-motor-juegos/ y /servicios/gaap_oacs/` | 1 | 14 | `PARTIAL` | Cursos → Actividades (histórico y enlace seguro; motor interactivo externo en navegador) |
| `MOD-29` | **Consentimiento Informado** | `/tda-consentimiento-informado/` | 1 | 2 | `PARTIAL` | Solo lectura en policy/cliente; sin modal de firma automática por seguridad |
| `MOD-30` | **Mi Cuenta, Privacidad y Preferencias** | `/dotlrn/control-panel, /pvt/home, /user/` | 5 | 0 | `PARTIAL` | Expediente → Información Académica + Trámites → Notificaciones y Cuenta |
| `MOD-31` | **Ayuda, Guías de Uso y Accesibilidad** | `/dotlrn/help, /servicios/capacitacion/guia_profesores/` | 0 | 1 | `IMPLEMENTED` | Trámites → Enlaces Institucionales Seguros |
| `MOD-32` | **Librería UI Transversal (tds-lib / theme-td)** | `/resources/tds-lib/ y /resources/theme-td/` | 0 | 0 | `IMPLEMENTED` | Reconstruido nativamente con guías de interfaz + material translúcido GDI+ (0% GPU) |

## 3. Índice de los 20 Entregables Obligatorios

1. `MASTER_TECDIGITAL_MAP.md` (este documento)
2. `MASTER_TECDIGITAL_MAP.json`
3. `TECDIGITAL_ROUTE_REGISTRY.json`
4. `TECDIGITAL_API_REGISTRY.json`
5. `TECDIGITAL_MODULE_REGISTRY.json`
6. `TECDIGITAL_HOOKS_REGISTRY.json`
7. `TECDIGITAL_UI_COMPONENTS.json`
8. `TECDIGITAL_DATA_MODELS.json`
9. `TECDIGITAL_PERMISSIONS_MATRIX.json`
10. `TECDIGITAL_COVERAGE_MATRIX.csv`
11. `TECDIGITAL_COVERAGE_REPORT.md`
12. `TECDIGITAL_ARCHITECTURE.md`
13. `TECDIGITAL_DATA_FLOW.md`
14. `TECDIGITAL_DISCOVERY_QUEUE.json`
15. `TECDIGITAL_BLOCKED_ITEMS.md`
16. `TECDIGITAL_UNKNOWN_COMPONENTS.md`
17. `TECDIGITAL_TEST_REPORT.md`
18. `TECDIGITAL_CUADERNU_INTEGRATION.md`
19. `TECDIGITAL_IMPLEMENTATION_GAPS.md`
20. `TECDIGITAL_SESSION_CHECKPOINT.md`
- Reconstrucción funcional local (Sección 8): `reference_reconstruction/index.html`
- Evidencias sanitizadas: `evidence/dom_skeletons/`, `evidence/schemas/`, `evidence/fixtures/`, `evidence/diagrams/`
