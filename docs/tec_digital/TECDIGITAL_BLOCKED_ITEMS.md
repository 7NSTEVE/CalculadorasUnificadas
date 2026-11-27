# Elementos Bloqueados y Restringidos — Auditoría de Seguridad Solo Lectura

> Durante toda la cartografía recursiva (`500` páginas, `243` bundles JS/CSS) y las sincronizaciones reales, `PolicyEnforcingTransport` mantuvo **`MUTATION_MASK = 0`**. Se interceptaron **373** URLs mutantes o peligrosas antes de enviar un solo byte a la red.

## 1. Resumen de Bloqueos en Vivo por Motivo (`373` URLs)

| Motivo de Política (`policy.cpp`) | Conteo |
|---|---:|
| `mutating-navigation-or-operation` | 373 |

## 2. Operaciones Mutantes vía `GET` Identificadas (Riesgo Crítico Documentado)

El método HTTP `GET` **no** garantiza idempotencia en OpenACS/dotLRN. Las siguientes rutas `GET` modifican estado en el servidor y permanecen bloqueadas:

1. `GET /dotlrn/region=...&action=op_swap` — Reordena portlets del portal (descubierta en esta auditoría; bloqueada explícitamente en `policy.cpp`).
2. `GET /tda-notifications/ajax/update_notification_state?notification_by_user_id=...` — Marca notificación como leída (solo con `ScopedUserMutationGuard::ManageNotifications`).
3. `GET /tda-notifications/ajax/delete_user_notification?notification_by_user_id=...` — Elimina notificación del usuario (solo con confirmación explícita).
4. `GET /tda-notifications/ajax/delete_all_user_notifications` — Borra toda la bandeja (bloqueada permanentemente).
5. `GET /dotlrn/notifications/request_notifications?delivery_method_id=...` — Altera suscripciones de correo.
6. `GET /pvt/unsubscribe?request_id=...` — Cancela suscripción.
7. `GET /register/logout` — Cierra sesión en servidor (CuadernU usa cierre local sin revocar otras sesiones).
8. `GET /dotlrn/configure?op=shade|unshade|hide|show|delete|move` — Modifica layout del portal.
9. `GET /dotlrn/classes/.../file-storage/permissions?object_id=...` — Página de permisos/administración.

## 3. Top 30 Patrones de URL Interceptados

| Patrón de Ruta | Ocurrencias |
|---|---:|
| `https://tecdigital.tec.ac.cr/notifications/request-change-frequency?amp;return_url={val}&request_id={val}` | 79 |
| `https://tecdigital.tec.ac.cr/notifications/request-delete?amp;return_url={val}&request_id={val}` | 79 |
| `https://tecdigital.tec.ac.cr/notifications/request-new?amp;object_id={val}&amp;return_url={val}&amp;type_id={val}&amp;user_id={val}&pretty_name={val}` | 68 |
| `https://tecdigital.tec.ac.cr/shared/send-email?amp;community_id={val}&amp;return_url={val}&sendto={val}` | 27 |
| `https://tecdigital.tec.ac.cr/dotlrn/deregister?amp;amp;referer={val}&community_id={val}` | 23 |
| `https://tecdigital.tec.ac.cr/dotlrn/classes/{dept}/{course}/{group}/calendar/cal-item-new?amp;return_url={val}&amp;view={val}&date={val}` | 17 |
| `https://tecdigital.tec.ac.cr/dotlrn/classes/{dept}/{course}/{group}/deregister-self-confirm?referer={val}` | 17 |
| `https://tecdigital.tec.ac.cr/dotlrn/calendar/cal-item-new?amp;date={val}&start_time={val}` | 16 |
| `https://tecdigital.tec.ac.cr/dotlrn/calendar/cal-item-new?amp;return_url={val}&amp;view={val}&date={val}` | 4 |
| `https://tecdigital.tec.ac.cr/dotlrn/catedras.tec/{catedra_slug}/calendar/cal-item-new?amp;return_url={val}&amp;view={val}&date={val}` | 3 |
| `https://tecdigital.tec.ac.cr/dotlrn/region=1&direction=right&community_id=0&return_url=index&element_id={id}&portal_id={id}&action=op_move` | 2 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/estudiantes.E/estesc/member-confirm-2?referer={val}` | 2 |
| `https://tecdigital.tec.ac.cr/register/logout` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/configure-element?amp;op={val}&element_id={val}` | 1 |
| `https://tecdigital.tec.ac.cr/user/portrait/upload?return_url={val}` | 1 |
| `https://tecdigital.tec.ac.cr/user/password-update` | 1 |
| `https://tecdigital.tec.ac.cr/pvt/unsubscribe` | 1 |
| `https://tecdigital.tec.ac.cr/shared/make-invisible` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/region=2&direction=left&community_id=0&return_url=index&element_id={id}&portal_id={id}&action=op_move` | 1 |
| `https://tecdigital.tec.ac.cr/pvt/../user/password-update` | 1 |
| `https://tecdigital.tec.ac.cr/pvt/../user/portrait/upload?return_url={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/aseteca/member-confirm-2?referer={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/campussc/member-confirm-2?referer={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/estudiantes.E/member-confirm-2?referer={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/calendar/cal-item-new?amp;return_url={val}&amp;view={val}&date={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/estudiantes.E/calendar/cal-item-new?amp;return_url={val}&amp;view={val}&date={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/estudiantes.E/estesc/calendar/cal-item-new?amp;return_url={val}&amp;view={val}&date={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/campussc/calendar/cal-item-new?amp;return_url={val}&amp;view={val}&date={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.E/aseteca/calendar/cal-item-new?amp;return_url={val}&amp;view={val}&date={val}` | 1 |
| `https://tecdigital.tec.ac.cr/dotlrn/escuela/escuela.FI/cursonivelacinautodirigido2/member-confirm-2?referer={val}` | 1 |

## 4. Mutaciones GraphQL Bloqueadas (`16 / 16` en `tda-inclusion-manager`)

Todas las 16 mutaciones extraídas de `index-IE6zhopY.js` (`ApprovePreconfiguration`, `ChangeApplicationsStatusOrComment`, `ChangeApplicationsToReviewed`, `DeleteCourseParameter`, `DeleteInclusionRequest`, `RemoveInclusionRequest`, `SavePriorityConfig`, `SendApprovePreconfiguration`, `SetAdmInclusionRequest`, `SetInclusionRequest`, `SetPeriodCourses`, `SetScholarship`, `ToggleStudentException`, `ToogleInclusion`, `VerifyAlllApplications`, `setCourseParameter`) están bloqueadas por `ReadOnlyRequestPolicy::IsReadOnlyGraphQL`.
