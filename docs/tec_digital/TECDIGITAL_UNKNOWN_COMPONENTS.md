# Componentes Desconocidos, Restringidos o No Observables desde Cuenta Estudiantil

> Fecha: 2026-11-26. En cumplimiento estricto de la Sección 15, este registro documenta explícitamente lo que **NO** puede verificarse desde una cuenta estudiantil ni inferirse del cliente.

1. **Vistas y APIs de Rol Profesor / Coordinador / Administrador:**
   - Queries GraphQL de gestión en `tda-inclusion-manager`: `GetCampaignResults`, `ExportInclusionsReport`, `GetStudentsForReport`, `GetCoursesForReport`, `GetGroupsForReport`, `SearchStudent`, `getPreconfStepOneCourses`, `getPreconfStepTwoCourses`, `getCourseParameters`, `GetSavedPriorities`, `GetEntityAppCourses`, `GerUserEntities`.
   - Administración de rúbricas (`/tda-rubrica/ajax/accion_rubrica`, `accion_evaluacion`, `register_std_grade`, `accion_permisos`).
   - Administración de consentimientos (`/tda-consentimiento-informado/ajax/admin/*`).
   - Administración de prematrícula (`/tda-prematricula/ajax/generateProcess`, `set_additional_students`, `search_students`).
2. **Microservicio Externo GAAP (`https://api.tec.ac.cr:3003`):**
   - Devuelve `HTTP 401` con las cookies estándar de dotLRN; utiliza flujo de token propio del SPA `tda-motor-juegos` (no se intenta ningún bypass; CuadernU ofrece enlace seguro al navegador).
3. **Atributos de Graduación (`/tda-expediente-estudiantil/ajax/get_data_attributes`):**
   - El servidor institucional agota tiempo de espera (>20s) de forma intermitente; CuadernU aplica timeout acotado (`8s`) y muestra estado honesto sin colgar la sincronización.
4. **Módulos Opcionales No Montados en Cursos Específicos (`HTTP 404`):**
   - `16 / 17` cursos visitados devolvieron `HTTP 404` en `/forums/` (solo 1 curso histórico tenía foro activo).
   - `17 / 17` cursos devolvieron `HTTP 404` en `/tda-sms/` raíz (el endpoint real es `/tda-sms/view` o el portlet embebido en `/dotlrn/index`).
   - `/tda-evaluacion-desempeno/` raíz devolvió `HTTP 404`; el módulo real opera vía REST en `/tda-performance-evaluation/api/forms/` y `/tda-performance-evaluation/api/evaluations/` (`HTTP 200`).
5. **Internos No Observables del Servidor:**
   - Esquemas de base de datos PostgreSQL/Oracle, procedimientos Tcl/OpenACS, colas internas DAR y reglas de negocio de matrícula del lado servidor no son observables desde el cliente y no se especulan.
