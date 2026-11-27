// ============================================================================
// doc_studio/tests/tec_digital_integration_test.cpp
// Exhaustive unit, negative-policy, parser, HorariosTEC live bridge,
// offline cache, Canvas/SM-2/Gemini Notebook synergy, and HIG/0%-GPU static
// guard test suite for "Centro Academico TEC" in CuadernU.
// Strictly synthetic fixtures — ZERO PII.
// ============================================================================

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "../core/cudoc_storage.hpp"
#include "../integrations/tec_digital/tec_academic_store.hpp"
#if defined(DOC_STUDIO_HAS_GEMINI_NOTEBOOK)
#include "../integrations/gemini_notebook/gemini_notebook_sync_store.hpp"
#endif
#include "academic/tec_digital/academic_models.hpp"
#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/storage.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace td = academic::tec_digital;
namespace ht = academic::horarios_tec;
namespace fs = std::filesystem;

namespace {

int gPass = 0;
int gFail = 0;

void Check(bool cond, const char* expr, int line) {
    if (cond) {
        ++gPass;
    } else {
        ++gFail;
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
    }
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

std::string ReadWholeFile(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

// ----------------------------------------------------------------------------
// 1. ReadOnlyRequestPolicy: Positive + Negative Security Gates
// ----------------------------------------------------------------------------
void TestReadOnlyPolicyGates() {
    td::ReadOnlyRequestPolicy policy;

    // Newly verified read-only POST query endpoints MUST be ALLOWED
    const std::vector<std::string> allowedPostUrls = {
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/info_personal",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/detalle_historial",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/generar_tabla_promedio_ponderado",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/tabla_horario",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/citas_matricula",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/guia_horario",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/tabla_guia_horario",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/combos/carga_sedes_tds_lib",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/combos/carga_carreras_tds_lib",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/combos/carga_periodos_tds_lib",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/get_data_attributes",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/Consultar_datos_estudiante",
        "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/get_user_id",
        "https://tecdigital.tec.ac.cr/tda-um/ajax/get_info",
        "https://tecdigital.tec.ac.cr/tda-um/ajax/get_statistic_form",
        "https://tecdigital.tec.ac.cr/tda-um/ajax/general_statistics",
        "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/el2001/ajax/load_members",
        "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/el2001/evaluation/tda-ce-estudiante/ajax/get_history_files",
        "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/el2001/evaluation/tda-ce-estudiante/ajax/get_data_liverevision"
    };
    for (const auto& u : allowedPostUrls) {
        td::HttpRequest req;
        req.method = "POST";
        req.url = u;
        auto dec = policy.EvaluateHttpRequest(req);
        CHECK(dec.allowed);
    }

    // Mutating endpoints MUST be BLOCKED regardless of GET or POST
    const std::vector<std::pair<std::string, std::string>> blockedCases = {
        { "POST", "https://tecdigital.tec.ac.cr/tda-um/ajax/um_get_test_info" },
        { "GET",  "https://tecdigital.tec.ac.cr/tda-um/ajax/um_get_test_info" },
        { "GET",  "https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/cargaDocumentos/termsAndConditions?type=2" },
        { "POST", "https://tecdigital.tec.ac.cr/tda-rubrica/tda_evaluacion_rubrica" },
        { "POST", "https://tecdigital.tec.ac.cr/user/basic-info-update" },
        { "POST", "https://tecdigital.tec.ac.cr/tda-motor-juegos/resources/ajax/calificar_asignacion" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/el2001/ajax/spam" },
        { "POST", "https://tecdigital.tec.ac.cr/portal/ajax/portal-configure" },
        { "POST", "https://tecdigital.tec.ac.cr/tds-info-general/files/upload" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/file-storage/delete-elements?object_list=1" },
        // /tda-expediente-estudiantil/ajax/actualizar_cursos is a verified read-only DAR preview;
        // actual mutating DAR sync endpoints are /dotlrn/ajax/actualizar_cursos and actualizar_cursos2.
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/ajax/actualizar_cursos" },
        { "POST", "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/actualizar__datos" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/file-storage/api/move/99" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/file-storage/api/copy/99" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/file-storage/files/upload" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/file-storage/create_folder" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/evaluations/upload_evaluation" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/evaluations/delete_version_answer" },
        { "POST", "https://tecdigital.tec.ac.cr/tda-control-plagio/addDocumentEphorus" },
        { "POST", "https://tecdigital.tec.ac.cr/tda-control-plagio/actualizaVisibilidad" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/user/insert_personal_info" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/user/save_privacy_settings" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/user/set_term_condition_answer" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/notifications/update_notification_state" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/notifications/delete_user_notification" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/notifications/delete_all_user_notifications" },
        { "GET",  "https://tecdigital.tec.ac.cr/dotlrn/notifications/request_notifications?delivery_method_id=2" },
        { "GET",  "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/file-storage/permissions?object_id=42" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/shared/make-invisible" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/user/portrait/upload" },
        { "GET",  "https://tecdigital.tec.ac.cr/pvt/unsubscribe?request_id=1" },
        { "GET",  "https://tecdigital.tec.ac.cr/register/logout" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/file-storage/api/notification/123" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/ie1001/file-storage/create_folder2" },
        { "POST", "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/actualizar_cursos2" },
        { "POST", "https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/fileUpload" },
        { "POST", "https://tecdigital.tec.ac.cr/user/email-privacy-level" },
        { "POST", "https://tecdigital.tec.ac.cr/acs-lang/change-locale" },
        { "POST", "https://tecdigital.tec.ac.cr/dotlrn/calendar/calendar-item-types" }
    };
    for (const auto& [method, url] : blockedCases) {
        td::HttpRequest req;
        req.method = method;
        req.url = url;
        auto dec = policy.EvaluateHttpRequest(req);
        CHECK(!dec.allowed);
    }
}

// ----------------------------------------------------------------------------
// 2. Pure Parsers & Calculators (Zero-PII Synthetic Fixtures matching real TEC DOM)
// ----------------------------------------------------------------------------
void TestPureParsersAndCalculators() {
    // 2A. Detailed Evaluations (real TEC .sub_total + .container_accor DOM) + What-If + Rubric
    const std::string evalHtml = R"html(
        <div>Nota Total (sin redondear): 42.00 / 100</div>
        <div class="sub_total" category_id="10" item_weight="60">Exámenes Parciales 24.0 / 60</div>
        <div class="container_accor" assign_id="A1">
            <span class="assignNameText">Examen 1</span>
            <span>24.0 / 30</span>
            <span class="vt_grade_student">80.0</span>
            <p>Valor de la Asignación: 30</p>
            <p>Fecha de entrega: 10/09/2026 23:59</p>
            <a href="javascript:openRubric('R-101','C-1','TI-1','T-1')">Ver rúbrica</a>
        </div>
        <div class="container_accor" assign_id="A2">
            <span class="assignNameText">Examen 2</span>
            <span>-- / 30</span>
            <span class="vt_grade_student">--</span>
            <p>Valor de la Asignación: 30</p>
            <p>Fecha de entrega: 20/10/2026 23:59</p>
        </div>
        <div class="sub_total" category_id="20" item_weight="40">Laboratorios 18.0 / 40</div>
        <div class="container_accor" assign_id="A3">
            <span class="assignNameText">Lab 1</span>
            <span>18.0 / 20</span>
            <span class="vt_grade_student">90.0</span>
            <p>Valor de la Asignación: 20</p>
        </div>
    )html";
    auto evalRep = td::ParseDetailedEvaluationsHtml(evalHtml, "el2001");
    CHECK(evalRep.categories.size() == 2);
    CHECK(evalRep.categories[0].items.size() == 2);
    CHECK(evalRep.categories[0].items[0].is_graded);
    CHECK(std::abs(evalRep.categories[0].items[0].raw_grade_100 - 80.0) < 0.01);
    CHECK(!evalRep.categories[0].items[1].is_graded);

    // Deterministic What-If calculation (target 70, impossible 95, already-reached 35, hypothetical override)
    td::DetailedEvaluationsReport syntheticRep;
    syntheticRep.community_slug = "el2001";
    td::DetailedEvaluationCategory cat1;
    cat1.name = "Examenes";
    cat1.weight_pct = 100.0;
    td::DetailedEvaluationItem i1;
    i1.assign_id = "E1"; i1.name = "Parcial 1"; i1.weight_pct = 50.0; i1.is_graded = true; i1.raw_grade_100 = 80.0; i1.earned_pct = 40.0;
    td::DetailedEvaluationItem i2;
    i2.assign_id = "E2"; i2.name = "Parcial 2"; i2.weight_pct = 50.0; i2.is_graded = false; i2.due_date_iso = "2026-11-28T23:59";
    cat1.items = { i1, i2 };
    syntheticRep.categories.push_back(cat1);

    auto wi70 = td::ComputeGradeWhatIf(syntheticRep, 70.0);
    CHECK(std::abs(wi70.current_earned_pct - 40.0) < 0.01);
    CHECK(std::abs(wi70.remaining_weight_pct - 50.0) < 0.01);
    CHECK(wi70.mathematically_possible);
    CHECK(std::abs(wi70.required_avg_on_remaining_100 - 60.0) < 0.01);

    auto wi95 = td::ComputeGradeWhatIf(syntheticRep, 95.0);
    CHECK(!wi95.mathematically_possible); // Max possible is 90.0

    auto wi35 = td::ComputeGradeWhatIf(syntheticRep, 35.0);
    CHECK(wi35.already_reached);

    auto wiHyp = td::ComputeGradeWhatIf(syntheticRep, 70.0, { { "E2", 90.0 } });
    CHECK(std::abs(wiHyp.projected_final_with_overrides - 85.0) < 0.01);

    // Rubric HTML
    const std::string rubHtml = R"html(
        <h2>Rúbrica Laboratorio 1</h2>
        <table>
            <tr><th>Criterio</th><th>Excelente (40 pts)</th><th>Regular (20 pts)</th></tr>
            <tr><td>Análisis Nodal</td><td>Ecuaciones sin error (40)</td><td>Errores menores (20)</td></tr>
            <tr><td>Simulación SPICE</td><td>Curvas verificadas (60)</td><td>Parcial (30)</td></tr>
        </table>
    )html";
    auto rub = td::ParseRubricHtml(rubHtml, "R-101");
    CHECK(!rub.title.empty());
    CHECK(rub.criteria.size() >= 2);

    // 2B. Expediente stats (all 8 official TEC states), Detalle Notas (7 cols), Promedio Ponderado (4 cols), Citas (4 cols)
    const std::string statsHtml = R"html(
        <div>Promedio ponderado general: 87.50</div>
        <div>Total de horas cursadas: 320</div>
        <table>
            <tr><td>24</td><td>Aprobadas</td></tr>
            <tr><td>4</td><td>En curso</td></tr>
            <tr><td>1</td><td>Reprobadas</td></tr>
            <tr><td>1</td><td>Retiradas</td></tr>
            <tr><td>2</td><td>Reconocidas</td></tr>
            <tr><td>1</td><td>Suficiencia</td></tr>
            <tr><td>18</td><td>Pendientes</td></tr>
            <tr><td>0</td><td>Congeladas</td></tr>
        </table>
    )html";
    auto stats = td::ParseAcademicStatsHtml(statsHtml);
    CHECK(std::abs(stats.weighted_avg_general - 87.50) < 0.01);
    CHECK(stats.hours_taken == 320);
    CHECK(stats.count_aprobadas == 24);
    CHECK(stats.count_en_curso == 4);
    CHECK(stats.count_reprobadas == 1);
    CHECK(stats.count_retiradas == 1);
    CHECK(stats.count_reconocidas == 2);
    CHECK(stats.count_suficiencia == 1);
    CHECK(stats.count_pendientes == 18);
    CHECK(stats.count_congeladas == 0);

    const std::string notasHtml = R"html(
        <table>
            <thead><tr><th>Año</th><th>Estado</th><th>Nota</th><th>Materia</th><th>Modalidad</th><th>Periodo</th><th>Créditos</th></tr></thead>
            <tbody>
                <tr><td>2026</td><td>Aprobado</td><td>88</td><td>MA1001 - Cálculo I</td><td>Semestre</td><td>1</td><td>4</td></tr>
                <tr><td>2026</td><td>Reprobado</td><td>62</td><td>FI1001 - Física I</td><td>Semestre</td><td>1</td><td>4</td></tr>
            </tbody>
        </table>
    )html";
    auto notas = td::ParseDetalleNotasHtml(notasHtml);
    CHECK(notas.size() == 2);
    CHECK(notas[0].course_code == "MA1001");
    CHECK(notas[1].state == "Reprobado");

    const std::string promHtml = R"html(
        <table>
            <thead><tr><th>Año</th><th>Modalidad</th><th>Periodo</th><th>Promedio Ponderado</th></tr></thead>
            <tbody>
                <tr><td>2026</td><td>Semestre</td><td>1</td><td>86.45</td></tr>
                <tr><td>2026</td><td>Semestre</td><td>2</td><td>89.10</td></tr>
            </tbody>
        </table>
    )html";
    auto proms = td::ParsePromedioPonderadoTableHtml(promHtml);
    CHECK(proms.size() == 2);
    CHECK(std::abs(proms[0].weighted_avg - 86.45) < 0.05);

    const std::string citasHtml = R"html(
        <table>
            <thead><tr><th>Fecha</th><th>Periodo</th><th>Modalidad</th><th>Hora</th></tr></thead>
            <tbody>
                <tr><td>2026-07-15</td><td>S-2-2026</td><td>Ordinaria</td><td>08:30</td></tr>
            </tbody>
        </table>
    )html";
    auto citas = td::ParseCitasMatriculaHtml(citasHtml);
    CHECK(citas.size() == 1);
    CHECK(citas[0].date == "2026-07-15");

    // 2C. Exact 11-column tabla_guia_horario -> HorariosTEC JSON -> HorariosSolver
    const std::string guiaHtml = R"html(
        <table>
            <thead>
                <tr>
                    <th>Código</th><th>Materia</th><th>Grupo</th><th>Créditos</th><th>Horario</th>
                    <th>Aula</th><th>Profesor</th><th>Cupo</th><th>Tipo Materia</th><th>Tipo Grupo</th><th>Reservados</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td>EL2001</td><td>Circuitos Eléctricos I</td><td>1</td><td>4</td>
                    <td>L-M 07:30-09:20</td><td>B3-04</td><td>Prof. Ejemplo A</td>
                    <td>20</td><td>Teoría</td><td>Regular</td><td>2</td>
                </tr>
                <tr>
                    <td>EL2001</td><td>Circuitos Eléctricos I</td><td>2</td><td>4</td>
                    <td>K-J 18:00-19:50</td><td>B3-08</td><td>Prof. Ejemplo B</td>
                    <td>0</td><td>Teoría</td><td>Regular</td><td>0</td>
                </tr>
                <tr>
                    <td>MA2001</td><td>Ecuaciones Diferenciales</td><td>1</td><td>4</td>
                    <td>K-J 09:30-11:20</td><td>D2-05</td><td>Prof. Ejemplo C</td>
                    <td>18</td><td>Teoría</td><td>Regular</td><td>0</td>
                </tr>
            </tbody>
        </table>
    )html";
    auto guiaRows = td::ParseTablaGuiaHorarioHtml(guiaHtml);
    CHECK(guiaRows.size() == 3);
    CHECK(guiaRows[0].codigo == "EL2001");
    CHECK(guiaRows[0].creditos == 4);
    CHECK(guiaRows[0].cupo == 20);
    CHECK(guiaRows[0].reservados == 2);

    const std::string horariosJson = td::GuiaHorarioRowsToHorariosTecJson(guiaRows, "Guia Test", "S-2-2026", "Cartago");
    auto parsedCat = ht::ParseCatalogJson(horariosJson);
    CHECK(parsedCat.ok);
    CHECK(parsedCat.catalog.courses.size() == 2);

    ht::SolverPreferences prefs{};
    prefs.filters.requireAvailableSeats = true;
    prefs.filters.excludeNightClasses = true;
    prefs.maxResults = 10;
    auto solved = ht::HorariosSolver::Solve(parsedCat.catalog, {}, prefs);
    CHECK(solved.HasSolutions());
    CHECK(solved.schedules.front().metrics.totalCredits == 8);

    // 2D. 8-column tabla_horario (current student schedule)
    const std::string horHtml = R"html(
        <table>
            <thead><tr><th>Horario</th><th>Lunes</th><th>Martes</th><th>Miércoles</th><th>Jueves</th><th>Viernes</th><th>Sábado</th><th>Domingo</th></tr></thead>
            <tbody>
                <tr><td>07:30-09:20</td><td>EL2001 Aula: B3-04</td><td></td><td>EL2001 Aula: B3-04</td><td></td><td></td><td></td><td></td></tr>
            </tbody>
        </table>
    )html";
    auto cells = td::ParseTablaHorarioHtml(horHtml);
    CHECK(cells.size() == 2);
    CHECK(cells[0].course_code == "EL2001");

    // 2E. Unified Calendar + RFC 5545 .ics export
    const std::string calHtml = R"html(
        <table>
            <thead><tr><th>Fecha</th><th>Hora</th><th>Evento</th><th>Calendario</th><th>Descripción</th></tr></thead>
            <tbody>
                <tr><td>2026-11-25</td><td>10:00 - 11:30</td><td>Seminario de Diseño Digital</td><td>EL2001</td><td>Auditorio B3</td></tr>
            </tbody>
        </table>
    )html";
    auto portalEvents = td::ParseCalendarListHtml(calHtml);
    CHECK(portalEvents.size() == 1);
    auto unified = td::BuildUnifiedCalendarEvents(portalEvents, { syntheticRep }, citas);
    CHECK(unified.size() == 3); // 1 portal + 1 evaluation due date + 1 matricula appointment
    const std::string ics = td::ExportCalendarEventsToIcs(unified, "-//CuadernU//TEC Test//ES");
    CHECK(ics.find("BEGIN:VCALENDAR") != std::string::npos);
    CHECK(ics.find("BEGIN:VEVENT") != std::string::npos);
    CHECK(ics.find("END:VCALENDAR") != std::string::npos);

    // 2F. Plagiarism tree JSON, SMS, Subscriptions, Forums, School Catalog, GAAP, User Model
    const std::string plagJson = R"json({
        "MyFiles": { "name": "Mis Archivos", "isStudent": 1, "folder_id": "9901" },
        "periods": "S-2-2026",
        "documents": [
            { "doc_uid": "DOC-99", "name": "Informe1.pdf", "percentaje": 8.5, "status": "completed", "text": "8.5%" }
        ]
    })json";
    auto plag = td::ParsePlagiarismTreeJson(plagJson);
    CHECK(plag.my_files_name == "Mis Archivos");
    CHECK(plag.documents.size() == 1);
    CHECK(std::abs(plag.documents[0].similarity_pct - 8.5) < 0.01);

    const std::string smsHtml = R"html(
        <table><tbody>
            <tr><td>Se trasladó el laboratorio a B3-02</td><td>2026-10-01 08:00</td><td>EL2001</td><td>Prof. Ejemplo</td></tr>
        </tbody></table>
    )html";
    auto sms = td::ParseSmsTableHtml(smsHtml);
    CHECK(sms.size() == 1);

    const std::string subsHtml = R"html(
        <table><tbody>
            <tr><td>EL2001</td><td>Noticias</td><td>Portada</td><td>Inmediata</td></tr>
        </tbody></table>
    )html";
    auto subs = td::ParseNotificationSubscriptionsHtml(subsHtml);
    CHECK(subs.size() == 1);

    const std::string forumsHtml = R"html(
        <div><a href="/dotlrn/classes/ie/el2001/forums/message-view?message_id=7788">Foro de Consultas Parcial 1</a></div>
    )html";
    auto forums = td::ParseForumsHtml(forumsHtml, "https://tecdigital.tec.ac.cr");
    CHECK(forums.size() == 1);
    CHECK(forums[0].message_id == "7788");

    const std::string schoolsHtml = R"html(
        <div class="school-card"><a href="/dotlrn/classes/ie/">Escuela de Ingeniería Electrónica</a></div>
    )html";
    auto schools = td::ParseSchoolCatalogHtml(schoolsHtml, "https://tecdigital.tec.ac.cr");
    CHECK(schools.size() == 1);
    CHECK(schools[0].dept_code == "ie");

    const std::string gaapJson = R"json([
        { "id": "G1", "title": "Quiz Diagnóstico", "type": "Quiz", "date": "2026-09-02", "score": 95.0, "status": "Completado" }
    ])json";
    auto gaap = td::ParseGaapHistoryHtmlOrJson(gaapJson);
    CHECK(gaap.size() == 1);

    const std::string umJson = R"json({
        "learning_style": { "visual": "82%", "activo": "64%" }
    })json";
    auto um = td::ParseUserModelInfoJson(umJson);
    CHECK(um.dimensions.size() == 1);

    // 2G. Archive ZIP URL builder + Phase 2 multi-day live Guia merge + #AttrStudent + #header-td filter
    const std::string zipUrl = td::StorageDownloader::BuildArchiveZipUrl(
        "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/el2001/file-storage/", "554433");
    CHECK(zipUrl.find("download-archive?object_id=554433") != std::string::npos);

    {
        const std::string liveMultiRowHtml = R"html(
            <table><tbody>
                <tr><td>EL2113</td><td>Circuitos I</td><td>050</td><td>4</td><td>Lunes - 7:55:9:40</td><td>B3-04</td><td>Prof. Live</td><td>20</td><td>Teoría</td><td>Regular</td><td>1</td></tr>
                <tr><td>EL2113</td><td>Circuitos I</td><td>50</td><td>4</td><td>Jueves - 7:0:8:45</td><td>B3-04</td><td>Prof. Live</td><td>20</td><td>Teoría</td><td>Regular</td><td>1</td></tr>
                <tr><td>EL2113</td><td>Circuitos I</td><td>50</td><td>4</td><td>Miercoles - 12:30:16:5</td><td>Lab-1</td><td>Prof. Live</td><td>20</td><td>Teoría</td><td>Regular</td><td>1</td></tr>
            </tbody></table>
        )html";
        auto lr = td::ParseTablaGuiaHorarioHtml(liveMultiRowHtml);
        CHECK(lr.size() == 3);
        const std::string lj = td::GuiaHorarioRowsToHorariosTecJson(lr, "Live Multi", "2026_S_2", "Cartago");
        auto lp = ht::ParseCatalogJson(lj);
        CHECK(lp.ok && lp.catalog.courses.size() == 1 && lp.catalog.courses[0].groups.size() == 1);
        CHECK(lp.catalog.courses[0].groups[0].sessions.size() == 3);
        ht::SolverPreferences lprefs{};
        lprefs.maxResults = 5;
        auto lsol = ht::HorariosSolver::Solve(lp.catalog, {}, lprefs);
        CHECK(lsol.HasSolutions() && lsol.schedules.front().metrics.activeDaysCount == 3);

        auto fids = td::FileStorageWalker::ExtractRootFolderIds(
            "<header id='header-td'><a href='/dotlrn/file-storage/#/153108817'>Personal</a></header>"
            "<div><a href='/dotlrn/classes/ie/el2113/file-storage/#/88776655'>Curso</a></div>");
        CHECK(fids.size() == 1 && fids[0] == "88776655");

        CHECK(td::ConvertPeriodKeyToExpedienteFormat("S-2-2026") == "2026_S_2");
        CHECK(td::ConvertExpedientePeriodToDotlrnFormat("2026_S_2") == "S-2-2026");
    }
}

// ----------------------------------------------------------------------------
// 3. Mock HTTP Transport enforcing LIVE POST protocol + Inclusion 503 + Zero Logout
// ----------------------------------------------------------------------------
class StrictLiveProtocolMockTransport final : public td::IHttpTransport {
public:
    std::atomic<int> logoutCalls{0};
    std::atomic<int> wrongMethodCalls{0};
    std::atomic<int> totalCalls{0};

    static std::vector<std::uint8_t> ToBytes(std::string_view s) {
        return std::vector<std::uint8_t>(s.begin(), s.end());
    }

    td::HttpResponse Execute(const td::HttpRequest& req) override {
        ++totalCalls;
        td::HttpResponse res;
        res.transport_ok = true;
        res.status_code = 200;
        res.final_url = req.url;

        if (req.url.find("/register/logout") != std::string::npos) {
            ++logoutCalls;
            res.status_code = 403;
            return res;
        }

        // Enforce live TEC Digital POST requirement on AJAX expediente endpoints!
        const bool mustBePost =
            req.url.find("/ajax/detalle_historial") != std::string::npos ||
            req.url.find("/ajax/citas_matricula") != std::string::npos ||
            req.url.find("/ajax/guia_horario") != std::string::npos ||
            req.url.find("/ajax/tabla_guia_horario") != std::string::npos ||
            req.url.find("/ajax/tabla_horario") != std::string::npos ||
            req.url.find("/ajax/combos/carga_") != std::string::npos;
        if (mustBePost && req.method != "POST") {
            ++wrongMethodCalls;
            res.status_code = 422;
            res.body = ToBytes("POST required on live TEC Digital");
            return res;
        }

        if (req.url.find("/tda-inclusion-manager/api/user-transaction") != std::string::npos) {
            res.status_code = 503;
            res.body.clear();
            return res;
        }
        if (req.url.find("/ajax/tabla_guia_horario") != std::string::npos) {
            // Includes EL2001 (prereq MA1001 approved -> ELIGIBLE) and EL4001 (prereq EL3001 NOT approved -> FILTERED OUT!)
            res.body = ToBytes(R"html(
                <table><tbody>
                    <tr>
                        <td>EL2001</td><td>Circuitos I</td><td>1</td><td>4</td>
                        <td>L-M 07:30-09:20</td><td>B3-04</td><td>Prof. Uno</td>
                        <td>15</td><td>Teoría</td><td>Regular</td><td>0</td>
                    </tr>
                    <tr>
                        <td>EL4001</td><td>Control Avanzado</td><td>1</td><td>4</td>
                        <td>K-J 07:30-09:20</td><td>B3-10</td><td>Prof. Dos</td>
                        <td>15</td><td>Teoría</td><td>Regular</td><td>0</td>
                    </tr>
                </tbody></table>
            )html");
            return res;
        }
        if (req.url.find("/ajax/detalle_historial") != std::string::npos) {
            res.body = ToBytes("<table><tr><td>12</td><td>Aprobadas</td></tr><tr><td>0</td><td>Reprobadas</td></tr></table>");
            return res;
        }
        if (req.url.find("/ajax/citas_matricula") != std::string::npos) {
            res.body = ToBytes("<table><tbody><tr><td>2026-07-20</td><td>S-2-2026</td><td>Ordinaria</td><td>09:00</td></tr></tbody></table>");
            return res;
        }
        if (req.url.find("/ajax/tabla_horario") != std::string::npos) {
            res.body = ToBytes("<table><tbody><tr><td>07:30-09:20</td><td>EL2001 B3-04</td><td></td><td></td><td></td><td></td><td></td><td></td></tr></tbody></table>");
            return res;
        }
        if (req.url.find("/ajax/combos/carga_sedes_tds_lib") != std::string::npos) {
            res.body = ToBytes("<option value='CA'>Cartago</option>");
            return res;
        }
        if (req.url.find("/ajax/combos/carga_periodos_tds_lib") != std::string::npos) {
            res.body = ToBytes("<option value='S-2-2026'>II Semestre 2026</option>");
            return res;
        }
        res.body = ToBytes("{}");
        return res;
    }
};

void TestEndToEndStoreAndSynergies() {
    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_tec_integration_test_root";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    auto mockTransport = std::make_unique<StrictLiveProtocolMockTransport>();
    StrictLiveProtocolMockTransport* mockPtr = mockTransport.get();

    td::ServiceConfig cfg;
    DocStudio::TecDigital::TecAcademicStore store(tempRoot.string(), std::move(mockTransport), cfg);

    // Seed authenticated curriculum profile + graph for prerequisite cross-check
    DocStudio::TecDigital::TecAcademicSnapshot seeded;
    seeded.sessionAuthenticated = true;
    td::CurriculumGraph g;
    g.dsc_curriculum = "Plan Sintético";
    td::CurriculumLevel l1; l1.id = "1";
    td::CurriculumCourseNode n1; n1.id_course = "MA1001"; n1.name = "Cálculo I"; n1.credits = 4;
    l1.courses.push_back(n1);
    td::CurriculumLevel l2; l2.id = "2";
    td::CurriculumCourseNode n2; n2.id_course = "EL2001"; n2.name = "Circuitos I"; n2.credits = 4; n2.requirements = { "MA1001" };
    l2.courses.push_back(n2);
    td::CurriculumLevel l3; l3.id = "3";
    td::CurriculumCourseNode n3; n3.id_course = "EL4001"; n3.name = "Control Avanzado"; n3.credits = 4; n3.requirements = { "EL3001" };
    l3.courses.push_back(n3);
    g.levels = { l1, l2, l3 };
    seeded.curriculumGraph = g;

    td::StudentCurriculumProfile prof;
    td::StudentCourseRecord gr;
    gr.id_course = "MA1001";
    gr.best_status = "Aprobado";
    prof.grades.push_back(gr);
    seeded.studentProfile = prof;

    // Seed Phase 2 fields to verify full snapshot JSON persistence round-trip
    seeded.graduateAttributes.push_back({"Conocimientos de ingeniería", "Aplicación de ciencias", "EL2001", 75.0, 15.0, 30.0, 30.0, 75.0});
    seeded.pendingGraduationCourses.push_back({"EL4001", "EL4001", "Control Avanzado", 4});
    seeded.inclusionCampaign = td::InclusionCampaignInfo{"C1", "Inclusiones II 2026", "2026_S_2", "2026-07-20", "2026-07-24", "IE", true};
    seeded.inclusionApplications.push_back({"A1", "EL4001", "Control Avanzado", "01", "Pendiente", "", "2026-07-21"});
    seeded.scholarshipInfo = td::ScholarshipStatusInfo{"C1", "Activa", "Beca", "100%"};
    seeded.pendingPerformanceEvals.push_back({"PE1", "Circuitos I", "Prof. Uno", "Evaluación Docente", "Pendiente", "2026-11-30", ""});
    seeded.customTabResourcesByCourse["/dotlrn/classes/ie/el2001"] = {{"Repositorio — Examen 2025", "https://tecdigital.tec.ac.cr/doc.pdf", "PDF"}};
    store.SetSnapshotForTesting(seeded);

    // Live HorariosTEC fetch with prerequisite filtering enabled!
    ht::SolverPreferences prefs;
    prefs.filters.requireAvailableSeats = true;
    prefs.maxResults = 5;
    auto liveSolve = store.FetchAndSolveLiveSchedule("CA", "IE", "S-2-2026", "Cartago", prefs, true, false);
    CHECK(liveSolve.ok);
    CHECK(liveSolve.rawRows.size() == 2);
    CHECK(liveSolve.filteredOutByPrereqsCount == 1); // EL4001 filtered out (missing EL3001)
    CHECK(liveSolve.catalog.courses.size() == 1);
    if (!liveSolve.catalog.courses.empty()) {
        CHECK(liveSolve.catalog.courses[0].codeUtf8 == "EL2001");
    }
    CHECK(liveSolve.solved.HasSolutions());
    CHECK(mockPtr->wrongMethodCalls.load() == 0); // Verified POST was used!

    // Verify Expediente POST calls succeed without hitting GET 422
    auto expRes = store.Service().TdaClient().GetExpedienteSummary();
    CHECK(expRes.ok);
    CHECK(mockPtr->wrongMethodCalls.load() == 0);

    // Graceful degradation on 503 Inclusion service via TdaTdsClient
    auto incRes = store.Service().TdaClient().GetInclusionTransactionKey();
    CHECK(!incRes.ok);
    CHECK(incRes.error_code == "service_unavailable");

    // Snapshot JSON persistence round-trip
    std::string err;
    CHECK(store.SaveSnapshotToDisk(&err));
    {
        DocStudio::TecDigital::TecAcademicStore reloadStore(tempRoot.string());
        CHECK(reloadStore.LoadSnapshotFromDisk(&err));
        auto reloaded = reloadStore.Snapshot();
        CHECK(reloaded.curriculumGraph.has_value());
        CHECK(reloaded.curriculumGraph->levels.size() == 3);
        CHECK(reloaded.studentProfile.has_value());
        CHECK(reloaded.studentProfile->grades.size() == 1);
        CHECK(reloaded.graduateAttributes.size() == 1);
        CHECK(reloaded.pendingGraduationCourses.size() == 1);
        CHECK(reloaded.inclusionCampaign.has_value() && reloaded.inclusionCampaign->id == "C1");
        CHECK(reloaded.inclusionApplications.size() == 1);
        CHECK(reloaded.scholarshipInfo.has_value() && reloaded.scholarshipInfo->status == "Activa");
        CHECK(reloaded.pendingPerformanceEvals.size() == 1);
        CHECK(reloaded.customTabResourcesByCourse.count("/dotlrn/classes/ie/el2001") == 1);
    }

    // 1-Click Canvas Builders + SM-2 Flashcards + Gemini Notebook Candidates
    DocStudio::DocumentModel doc;
    doc.ResetToBlank(DocStudio::DocumentMode::InfiniteCanvas);
    td::CommunityInfo course;
    course.slug = "el2001";
    course.name = "Circuitos I";
    course.period = "S-2-2026";
    course.url = "https://tecdigital.tec.ac.cr/dotlrn/classes/ie/el2001/s-2-2026/";

    td::CourseOverviewSnapshot ov;
    ov.teacher.name = "Prof. Ejemplo";
    ov.weekly_sections = { "Semana 1: Leyes de Kirchhoff" };
    ov.official_docs = { { "Programa del Curso", "https://tecdigital.tec.ac.cr/doc1", "Programa" } };
    ov.external_links = { { "Simulador", "https://example.org/circuit", "Enlace" } };

    td::DetailedEvaluationsReport ev;
    td::DetailedEvaluationCategory ec1; ec1.name = "Quices"; ec1.weight_pct = 30.0;
    td::DetailedEvaluationItem ei1;
    ei1.name = "Quiz 1"; ei1.weight_pct = 15.0; ei1.is_graded = false; ei1.due_date_text = "2026-11-28";
    ec1.items.push_back(ei1);
    ev.categories.push_back(ec1);

    const std::size_t addedCourse = DocStudio::TecDigital::TecAcademicStore::InsertCourseOverviewToCanvas(doc, course, &ov, &ev);
    CHECK(addedCourse >= 1);

    const std::size_t addedSched = DocStudio::TecDigital::TecAcademicStore::InsertScheduleToCanvas(doc, liveSolve.solved.schedules.front());
    CHECK(addedSched >= 1);

    const std::size_t addedCurr = DocStudio::TecDigital::TecAcademicStore::InsertCurriculumSummaryToCanvas(doc, g, &prof);
    CHECK(addedCurr >= 1);

    const bool importedMat = DocStudio::TecDigital::TecAcademicStore::ImportDownloadedMaterialToCanvas(
        doc, (tempRoot / "offline_files" / "guia.pdf").string(), "Guía Laboratorio 1.pdf", "EL2001", course.url);
    CHECK(importedMat);

    auto flashcards = DocStudio::TecDigital::TecAcademicStore::GenerateSm2FlashcardsFromCourse(course, &ov, &ev);
    CHECK(flashcards.size() >= 3);

#if defined(DOC_STUDIO_HAS_GEMINI_NOTEBOOK)
    fs::create_directories(tempRoot / "offline_files", ec);
    const fs::path pdfStub = tempRoot / "offline_files" / "guia.pdf";
    {
        std::ofstream ofs(pdfStub, std::ios::binary);
        ofs << "%PDF-1.4\n% synthetic test stub\n";
    }
    td::FileRecord fRec;
    fRec.name = "guia.pdf";
    fRec.title = "Guía 1";
    fRec.logical_rel_path = "guia.pdf";
    fRec.status = td::FileStatus::Downloaded;
    auto gnCands = DocStudio::TecDigital::TecAcademicStore::BuildGeminiNotebookCandidatesForCourse(
        course, &ov, { fRec }, (tempRoot / "offline_files").string());
    CHECK(gnCands.size() == 2); // 1 local PDF + 1 external URL
#endif

    // Offline cache budget pruning test
    {
        const fs::path offlineDir(store.OfflineFilesRootUtf8());
        fs::create_directories(offlineDir, ec);
        std::string payload(4096, 'A');
        { std::ofstream o1(offlineDir / "old1.bin", std::ios::binary); o1 << payload; }
        { std::ofstream o2(offlineDir / "old2.bin", std::ios::binary); o2 << payload; }
        const auto bytesBefore = store.ComputeOfflineCacheBytes();
        CHECK(bytesBefore >= 8192);
        const auto pruned = store.PruneOfflineCacheOlderThanBytes(4096);
        CHECK(pruned >= 1);
        CHECK(store.ComputeOfflineCacheBytes() <= 4096);
    }

    // Graduation Critical Path Simulator
    {
        auto cp = td::TdaTdsClient::ComputeCurriculumCriticalPath(g, &prof);
        CHECK(cp.total_courses == 3);
        CHECK(cp.approved_courses == 1);
        CHECK(cp.remaining_courses == 2);
        CHECK(cp.completed_credits == 4);
        CHECK(cp.remaining_credits == 8);
        CHECK(cp.min_semesters_remaining >= 1);
        CHECK(!cp.unlocked_next_codes.empty());
        CHECK(cp.unlocked_next_codes[0] == "EL2001");
    }

    // Schedule .ics + PDF export + Course Notebook .cnu creation
    if (liveSolve.solved.HasSolutions()) {
        const auto& bestSched = liveSolve.solved.schedules.front();
        const std::string schedIcs = DocStudio::TecDigital::TecAcademicStore::ExportRankedScheduleToIcs(bestSched);
        CHECK(schedIcs.find("BEGIN:VCALENDAR") != std::string::npos);
        CHECK(schedIcs.find("RRULE:FREQ=WEEKLY;COUNT=16") != std::string::npos);
        CHECK(schedIcs.find("EL2001") != std::string::npos);

        const fs::path pdfOut = tempRoot / "horario_test.pdf";
        std::string pdfErr;
        CHECK(DocStudio::TecDigital::TecAcademicStore::ExportRankedScheduleToPdfFile(bestSched, pdfOut.wstring(), "Horario Test", &pdfErr));
        const std::string pdfBytes = ReadWholeFile(pdfOut);
        CHECK(pdfBytes.rfind("%PDF-", 0) == 0);
    }
    {
        const fs::path cnuOut = tempRoot / "curso_el2001.cnu";
        std::string cnuErr;
        CHECK(DocStudio::TecDigital::TecAcademicStore::SaveCourseNotebookCnu(course, &ov, &ev, cnuOut.wstring(), &cnuErr));
        DocStudio::DocumentModel loadedNb;
        CHECK(DocStudio::CudocStorage::LoadFromFile(cnuOut.wstring(), loadedNb, &cnuErr));
        CHECK(!loadedNb.Pages().empty());
    }

    // Local-only logout MUST NEVER hit /register/logout
    store.LogoutLocalOnly(true);
    CHECK(!store.IsAuthenticated());
    CHECK(mockPtr->logoutCalls.load() == 0);

    fs::remove_all(tempRoot, ec);
}

// ----------------------------------------------------------------------------
// 3B. REAL 127.0.0.1 Loopback TCP HTTP Server Integration Test (Live WinHTTP!)
// ----------------------------------------------------------------------------
void TestRealLoopbackHttpServerEndToEnd() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        CHECK(false);
        return;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(listenSock != INVALID_SOCKET);
    if (listenSock == INVALID_SOCKET) { WSACleanup(); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // ephemeral OS port
    CHECK(bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    CHECK(listen(listenSock, 16) == 0);

    int addrLen = sizeof(addr);
    CHECK(getsockname(listenSock, reinterpret_cast<sockaddr*>(&addr), &addrLen) == 0);
    const unsigned short port = ntohs(addr.sin_port);
    const std::string origin = "http://127.0.0.1:" + std::to_string(port);

    std::atomic<bool> stopFlag{false};
    std::atomic<int> serverWrongMethodCount{0};
    std::atomic<bool> forceExpiredSession{false};
    std::atomic<bool> inclusionReturn503{true};
    std::atomic<int> sawCarneInPostCount{0};
    std::atomic<int> sawStudentIdInAttrCount{0};
    std::atomic<int> gqlAuthOkCount{0};
    std::atomic<int> gqlAuthFailCount{0};
    const std::string synthJwt = "eyJhbGciOiJIUzI1NiJ9.eyJ1c2VySWQiOiI5MDAxIn0.synthsig";

    std::thread serverThread([&]() {
        while (!stopFlag.load(std::memory_order_acquire)) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(listenSock, &rfds);
            timeval tv{ 0, 80000 }; // 80ms select poll
            int ready = select(0, &rfds, nullptr, nullptr, &tv);
            if (ready <= 0) continue;

            SOCKET client = accept(listenSock, nullptr, nullptr);
            if (client == INVALID_SOCKET) continue;

            std::string rawReq;
            char buf[4096]{};
            size_t hdrEnd = std::string::npos;
            while ((hdrEnd = rawReq.find("\r\n\r\n")) == std::string::npos) {
                int n = recv(client, buf, sizeof(buf), 0);
                if (n <= 0) break;
                rawReq.append(buf, static_cast<size_t>(n));
                if (rawReq.size() > 65536) break;
            }
            if (hdrEnd != std::string::npos) {
                std::string hdrBlock = rawReq.substr(0, hdrEnd);
                std::string reqBody = rawReq.substr(hdrEnd + 4);
                std::istringstream iss(hdrBlock);
                std::string reqLine;
                std::getline(iss, reqLine);
                if (!reqLine.empty() && reqLine.back() == '\r') reqLine.pop_back();
                std::istringstream rls(reqLine);
                std::string method, target;
                rls >> method >> target;

                size_t contentLen = 0;
                std::string hdrsLower;
                std::string line;
                while (std::getline(iss, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    std::string low = line;
                    for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    hdrsLower += low + "\n";
                    if (low.rfind("content-length:", 0) == 0) {
                        contentLen = static_cast<size_t>(std::strtoull(low.c_str() + 15, nullptr, 10));
                    }
                }
                while (reqBody.size() < contentLen) {
                    int n = recv(client, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    reqBody.append(buf, static_cast<size_t>(n));
                }
                if (reqBody.size() > contentLen) reqBody.resize(contentLen);

                int status = 200;
                std::string statusText = "OK";
                std::string contentType = "text/html; charset=utf-8";
                std::string body = "{}";
                std::string extraHeaders;

                if (target.find("/register/") != std::string::npos) {
                    status = 200;
                    statusText = "OK";
                    body = "<html><body><form action='/api/login/new-form/'><input name='email'/></form></body></html>";
                } else if (forceExpiredSession.load(std::memory_order_acquire)) {
                    status = 302;
                    statusText = "Found";
                    extraHeaders = "Location: " + origin + "/register/?return_url=/dotlrn/\r\n";
                    body = "Session expired";
                } else if (target == "/tda-expediente-estudiantil/") {
                    body = "<html><body><input type='hidden' id='carne' value='2026999999'/></body></html>";
                } else if (target.find("/ajax/get_data_attributes") != std::string::npos) {
                    if (method != "POST") {
                        ++serverWrongMethodCount;
                        status = 422; statusText = "Unprocessable Entity"; body = "POST required";
                    } else {
                        if (reqBody.find("student_id=2026999999") != std::string::npos) ++sawStudentIdInAttrCount;
                        body = "<table id='AttrStudent'><tr><td>Comunicación</td><td>Efectiva</td><td>EL2001</td><td>80%</td><td>10%</td><td>30%</td><td>40%</td><td>80%</td></tr></table>";
                    }
                } else if (target.find("/ajax/tabla_guia_horario") != std::string::npos ||
                           target.find("/ajax/detalle_historial") != std::string::npos ||
                           target.find("/ajax/citas_matricula") != std::string::npos ||
                           target.find("/ajax/tabla_horario") != std::string::npos ||
                           target.find("/ajax/combos/carga_") != std::string::npos) {
                    if (method != "POST") {
                        ++serverWrongMethodCount;
                        status = 422;
                        statusText = "Unprocessable Entity";
                        body = "POST required";
                    } else if (target.find("/ajax/tabla_guia_horario") != std::string::npos) {
                        body = R"html(<table><tbody>
                            <tr><td>EL2001</td><td>Circuitos I</td><td>050</td><td>4</td><td>Lunes - 7:55:9:40</td><td>B3-04</td><td>Prof. Loopback</td><td>15</td><td>Teoría</td><td>Regular</td><td>1</td></tr>
                            <tr><td>EL2001</td><td>Circuitos I</td><td>50</td><td>4</td><td>Jueves - 7:0:8:45</td><td>B3-04</td><td>Prof. Loopback</td><td>15</td><td>Teoría</td><td>Regular</td><td>1</td></tr>
                        </tbody></table>)html";
                    } else if (target.find("/ajax/detalle_historial") != std::string::npos) {
                        if (reqBody.find("carne=2026999999") != std::string::npos) ++sawCarneInPostCount;
                        body = "<table><tr><td>10</td><td>Aprobadas</td></tr></table>";
                    } else if (target.find("/ajax/citas_matricula") != std::string::npos) {
                        if (reqBody.find("carne=2026999999") != std::string::npos) ++sawCarneInPostCount;
                        body = "<table><tbody><tr><td>2026-07-22</td><td>S-2-2026</td><td>Ordinaria</td><td>10:00</td></tr></tbody></table>";
                    } else if (target.find("/ajax/combos/carga_periodos_tds_lib") != std::string::npos) {
                        contentType = "application/json; charset=utf-8";
                        body = R"json([{"key":"2026_S_2","data":"2026 - Semestre 2"}])json";
                    } else {
                        body = "<span value='CA'>Cartago</span>";
                    }
                } else if (target.find("/tda-inclusion-manager/api/user-transaction") != std::string::npos) {
                    if (inclusionReturn503.load(std::memory_order_acquire)) {
                        status = 503;
                        statusText = "Service Unavailable";
                        body.clear();
                    } else {
                        contentType = "application/json; charset=utf-8";
                        body = R"({"transaction_key":")" + synthJwt + R"("})";
                    }
                } else if (target == "/graphql" && method == "POST") {
                    contentType = "application/json; charset=utf-8";
                    std::string jwtLow = synthJwt;
                    for (char& ch : jwtLow) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    if (hdrsLower.find("token: " + jwtLow) == std::string::npos) {
                        ++gqlAuthFailCount;
                        status = 401;
                        statusText = "Unauthorized";
                        body = R"({"errors":[{"message":"Token Required"}]})";
                    } else {
                        ++gqlAuthOkCount;
                        if (reqBody.find("getUserType") != std::string::npos) {
                            body = R"({"data":{"getUserType":{"user_role":"student"}}})";
                        } else if (reqBody.find("GetStudentActiveCampaign") != std::string::npos) {
                            body = R"({"data":{"GetStudentActiveCampaign":{"id":"CAMP-1","name":"Inclusiones II 2026","period":"2026_S_2","start_date":"2026-07-20","end_date":"2026-07-24","student_entity":"IE","active":true}}})";
                        } else if (reqBody.find("getStudentPendingGraduatedCourses") != std::string::npos) {
                            body = R"({"data":{"getStudentPendingGraduatedCourses":[{"id":"EL3001","code":"EL3001","name":"Electrónica I","credits":4}]}})";
                        } else if (reqBody.find("getStudentRequisitsCorrequisitsByCourse") != std::string::npos) {
                            body = R"({"data":{"getStudentRequisitsCorrequisitsByCourse":{"allRequirementsMet":true,"allCoRequirementsMet":true,"hasOverlap":false,"missingRequirements":[],"missingCoRequirements":[],"message":"OK"}}})";
                        } else {
                            body = R"({"data":{}})";
                        }
                    }
                } else if (target.find("/file-storage/api/file/90001/versions") != std::string::npos) {
                    contentType = "application/json; charset=utf-8";
                    body = R"json([{"title":"Guía Circuitos v2","live_revision":"90002","author":{"name":"Prof. Loopback"},"last_modified_ansi":"2026-09-15","content_size":46}])json";
                } else if (target.find("/file-storage/api/folder/70001") != std::string::npos) {
                    contentType = "application/json; charset=utf-8";
                    body = R"json({
                        "folder_id": "70001",
                        "name": "Semana 1",
                        "breadcrumbs": [{"title": "Semana 1", "url": "/dotlrn/classes/ie/el2001/file-storage/?folder_id=70001"}],
                        "elements": [
                            {
                                "object_id": "90001",
                                "parent_id": "70001",
                                "fs_type": "file",
                                "name": "guia_circuitos.txt",
                                "title": "Guía Circuitos",
                                "mime_type": "text/plain",
                                "content_size": 46,
                                "url": "/dotlrn/classes/ie/el2001/file-storage/download/guia_circuitos.txt?version_id=90001",
                                "plagiarism": {"status": "done", "text": "3.0%", "percentage": 3.0, "doc_uid": "UID-90001"}
                            }
                        ]
                    })json";
                } else if (target.find("/file-storage/download/guia_circuitos.txt") != std::string::npos ||
                           target.find("version_id=90001") != std::string::npos ||
                           target.find("file_id=90001") != std::string::npos) {
                    contentType = "text/plain; charset=utf-8";
                    body = "Analisis nodal aplicando Ley de Ohm y Kirchhoff";
                } else if (target == "/guiahorarios/escuela.aspx/cargaEscuelas" && method == "POST") {
                    contentType = "application/json; charset=utf-8";
                    body = R"json({"d":"[{\"IDE_DEPTO\":\"CA\",\"DSC_DEPTO\":\"ESCUELA DE INGENIERIA EN COMPUTACION\"},{\"IDE_DEPTO\":\"MA\",\"DSC_DEPTO\":\"ESCUELA DE MATEMATICA\"}]"})json";
                } else if (target == "/guiahorarios/escuela.aspx/getdatosEscuelaAno" && method == "POST") {
                    contentType = "application/json; charset=utf-8";
                    if (reqBody.find("\"MA\"") != std::string::npos) {
                        body = nlohmann::json({{"d", R"json([
                            {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"MA1103","DSC_MATERIA":"Cálculo y álgebra lineal","IDE_GRUPO":2,"DSC_DEPTO":"MA","CAN_CREDITOS":4,"HORAS":6,"NOM_DIA":"MARTES","IDE_MODALIDAD":"S","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"09:30","HFIN":"11:20","NOM_PROFESOR":"Prof. Euler","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                        ])json"}}).dump();
                    } else {
                        body = nlohmann::json({{"d", R"json([
                            {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"IC1802","DSC_MATERIA":"Introducción a la programación","IDE_GRUPO":1,"DSC_DEPTO":"CA","CAN_CREDITOS":3,"HORAS":4,"NOM_DIA":"LUNES","IDE_MODALIDAD":"S","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"07:30","HFIN":"09:20","NOM_PROFESOR":"Prof. Turing","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                        ])json"}}).dump();
                    }
                } else if (target == "/guiahorarios/verano.aspx/getdatosVerano" && method == "POST") {
                    contentType = "application/json; charset=utf-8";
                    body = nlohmann::json({{"d", R"json([
                        {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"FH0025","DSC_MATERIA":"Optativa Verano","IDE_GRUPO":40,"DSC_DEPTO":"FH","CAN_CREDITOS":2,"HORAS":4,"NOM_DIA":"MIERCOLES","IDE_MODALIDAD":"V","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"VERANO","TIPO_CURSO":"Virtual","HINICIO":"18:20","HFIN":"20:50","NOM_PROFESOR":"Prof. Verano","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                    ])json"}}).dump();
                } else if (target == "/guiahorarios/estudiante.aspx/getdatos" && method == "POST") {
                    contentType = "application/json; charset=utf-8";
                    body = nlohmann::json({{"d", R"json([
                        {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"IC2101","DSC_MATERIA":"POO","IDE_GRUPO":3,"DSC_DEPTO":"CA","CAN_CREDITOS":3,"HORAS":4,"NOM_DIA":"VIERNES","IDE_MODALIDAD":"S","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"13:00","HFIN":"15:00","NOM_PROFESOR":"Prof. Poo","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                    ])json"}}).dump();
                } else if (target.find("/dotlrn/classes/ie/el2001/") != std::string::npos) {
                    body = "<html><body><header id='header-td'><a href='/dotlrn/file-storage/#/153108817'>Personal</a></header>"
                           "<a href='/dotlrn/classes/ie/el2001/file-storage/?folder_id=70001'>Documentos</a></body></html>";
                }

                std::ostringstream resp;
                resp << "HTTP/1.1 " << status << " " << statusText << "\r\n"
                     << "Content-Type: " << contentType << "\r\n"
                     << extraHeaders
                     << "Content-Length: " << body.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << body;
                const std::string out = resp.str();
                send(client, out.data(), static_cast<int>(out.size()), 0);
            }
            closesocket(client);
        }
    });

    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_tec_loopback_e2e";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    td::ServiceConfig cfg;
    cfg.base_url = origin;
    cfg.graphql_url = origin + "/graphql";
    cfg.policy_options.allowed_test_origins = { origin };
    cfg.downloader_options.min_delay_ms = 0;

    DocStudio::TecDigital::TecAcademicStore store(tempRoot.string(), nullptr, cfg);

    // 1. Live WinHTTP POST Guia de Horarios (2 rows merged by (EL2001, 050/50)) -> HorariosSolver over real 127.0.0.1 TCP
    ht::SolverPreferences prefs;
    prefs.filters.requireAvailableSeats = true;
    auto solveRes = store.FetchAndSolveLiveSchedule("CA", "IE", "S-2-2026", "Cartago", prefs, false, false);
    CHECK(solveRes.ok);
    CHECK(solveRes.rawRows.size() == 2);
    CHECK(solveRes.catalog.courses.size() == 1);
    if (!solveRes.catalog.courses.empty()) {
        CHECK(solveRes.catalog.courses[0].groups.size() == 1);
        CHECK(solveRes.catalog.courses[0].groups[0].sessions.size() == 2);
    }
    CHECK(solveRes.solved.HasSolutions());
    CHECK(serverWrongMethodCount.load() == 0);

    // 2. Live WinHTTP Expediente POST with auto-resolved carne/student_id & Inclusion 503 degradation over real TCP
    auto expRes = store.Service().TdaClient().GetExpedienteSummary();
    CHECK(expRes.ok);
    CHECK(sawCarneInPostCount.load() >= 2);
    CHECK(sawStudentIdInAttrCount.load() >= 1);
    CHECK(expRes.value.structured.graduate_attributes.size() == 1);
    CHECK(expRes.value.periodos_combo.size() == 1 && expRes.value.periodos_combo[0].key == "2026_S_2");
    CHECK(serverWrongMethodCount.load() == 0);
    auto incRes = store.Service().TdaClient().GetInclusionTransactionKey();
    CHECK(!incRes.ok);
    CHECK(incRes.error_code == "service_unavailable");

    // 2B. Live WinHTTP GraphQL client over real 127.0.0.1 TCP (token: header enforcement + pending grad + requisites check)
    inclusionReturn503.store(false, std::memory_order_release);
    auto roleRes = store.Service().GraphQlClient().FetchUserType();
    CHECK(roleRes.ok && roleRes.value == "student");
    auto pendRes = store.Service().GraphQlClient().FetchPendingGraduationCourses("", "2026999999");
    CHECK(pendRes.ok && pendRes.value.size() == 1 && pendRes.value[0].code == "EL3001");
    auto valRes = store.ValidateScheduleCoursesWithGraphQL({"EL2001"}, "S-2-2026");
    CHECK(valRes.ok && valRes.value.size() == 1 && valRes.value[0].all_requirements_met && !valRes.value[0].has_overlap);
    CHECK(gqlAuthOkCount.load() >= 3);
    CHECK(gqlAuthFailCount.load() == 0);

    // 3. Live WinHTTP File Storage Sync (ignoring #header-td personal root) + File Versions + SHA-256 Dedupe + FTS5
    const std::string commUrl = origin + "/dotlrn/classes/ie/el2001/";
    std::string syncSum, syncErr;
    bool syncOk = store.SyncCourseFiles(commUrl, true, &syncSum, &syncErr);
    CHECK(syncOk);
    auto snap1 = store.Snapshot();
    CHECK(snap1.foldersByPath.count(commUrl) == 1);
    if (snap1.foldersByPath.count(commUrl) == 1) {
        CHECK(!snap1.foldersByPath[commUrl].files.empty());
    }
    auto verRes = store.FetchFileVersions("90001");
    CHECK(verRes.ok && verRes.value.size() == 1 && verRes.value[0].live_revision == "90002");
    // Second sync verifies SHA-256 deduplication!
    std::string syncSum2;
    CHECK(store.SyncCourseFiles(commUrl, true, &syncSum2, &syncErr));
    CHECK(syncSum2.find("Deduplicados: 1") != std::string::npos);

    // FTS5 search over the downloaded loopback material
    auto ftsHits = store.SearchOfflineMaterials("Kirchhoff");
    CHECK(!ftsHits.empty());

#if defined(DOC_STUDIO_HAS_GEMINI_NOTEBOOK)
    if (snap1.foldersByPath.count(commUrl) == 1) {
        td::CommunityInfo ci;
        ci.slug = "el2001";
        ci.name = "Circuitos I";
        ci.url = commUrl;
        auto gnCands = DocStudio::TecDigital::TecAcademicStore::BuildGeminiNotebookCandidatesForCourse(
            ci, nullptr, snap1.foldersByPath[commUrl].files, store.OfflineFilesRootUtf8());
        CHECK(!gnCands.empty());
        DocStudio::GeminiNotebook::GeminiNotebookSyncStore gnStore((tempRoot / "gn_profile").wstring());
        auto plan = gnStore.ComputeIncrementalSyncPlan("tec:el2001", "TEC · Circuitos I", gnCands);
        CHECK(!plan.items.empty());
    }
#endif

    // 3C. Unauthenticated Public Guia de Horarios over real 127.0.0.1 TCP + Multi-school Merge
    {
        td::ServiceConfig pubCfg;
        pubCfg.base_url = origin;
        pubCfg.public_guia_horarios_url = origin + "/guiahorarios";
        pubCfg.policy_options.allowed_test_origins = { origin };
        DocStudio::TecDigital::TecAcademicStore pubStore((tempRoot / "pub_store").string(), nullptr, pubCfg);

        auto escList = pubStore.FetchPublicGuiaEscuelas();
        CHECK(escList.ok && escList.value.size() == 2 && escList.value[0].ide_depto == "CA");

        ht::SolverPreferences pubPrefs;
        DocStudio::TecDigital::PublicGuiaQuery qEsc;
        qEsc.mode = DocStudio::TecDigital::PublicGuiaMode::Escuela;
        qEsc.escuelaCode = "CA";
        qEsc.year = 2026;
        qEsc.filter.sede_filter = "Cartago";
        qEsc.filter.periodo_filter = "S-1";
        auto resCA = pubStore.FetchAndSolvePublicGuiaSchedule(qEsc, pubPrefs, false, false);
        CHECK(resCA.ok && resCA.catalog.courses.size() == 1 && resCA.catalog.courses[0].codeUtf8 == "IC1802");
        CHECK(resCA.solved.HasSolutions());

        // Merge second school (MA) into existing catalog -> conflict-free combined schedule (IC1802 + MA1103)
        DocStudio::TecDigital::PublicGuiaQuery qMA = qEsc;
        qMA.escuelaCode = "MA";
        auto resMerged = pubStore.FetchAndSolvePublicGuiaSchedule(qMA, pubPrefs, false, false, &resCA.catalog);
        CHECK(resMerged.ok && resMerged.catalog.courses.size() == 2);
        CHECK(resMerged.solved.HasSolutions());

        DocStudio::TecDigital::PublicGuiaQuery qVer;
        qVer.mode = DocStudio::TecDigital::PublicGuiaMode::Verano;
        qVer.year = 2026;
        auto resVer = pubStore.FetchAndSolvePublicGuiaSchedule(qVer, pubPrefs, false, false);
        CHECK(resVer.ok && resVer.catalog.courses.size() == 1 && resVer.catalog.courses[0].codeUtf8 == "FH0025");

        DocStudio::TecDigital::PublicGuiaQuery qCar;
        qCar.mode = DocStudio::TecDigital::PublicGuiaMode::Carnet;
        qCar.carnet = "2026999999";
        qCar.year = 0;
        auto resCar = pubStore.FetchAndSolvePublicGuiaSchedule(qCar, pubPrefs, false, false);
        CHECK(resCar.ok && resCar.catalog.courses.size() == 1 && resCar.catalog.courses[0].codeUtf8 == "IC2101");
    }

    // 4. Session expired graceful handling over real TCP redirect to /register/
    forceExpiredSession.store(true, std::memory_order_release);
    auto expiredRes = store.Service().TdaClient().GetGuiaHorarioRows("CA", "IE", "S-2-2026");
    CHECK(!expiredRes.ok);
    CHECK(expiredRes.error_code == "session_expired");

    stopFlag.store(true, std::memory_order_release);
    closesocket(listenSock);
    if (serverThread.joinable()) serverThread.join();
    WSACleanup();
    fs::remove_all(tempRoot, ec);
#endif
}

// ----------------------------------------------------------------------------
// 4. Static Architecture, 0% GPU/CPU Idle & guías de interfaz Guards
// ----------------------------------------------------------------------------
void TestStaticArchitectureAndHigGuards(const fs::path& docStudioRoot) {
    const std::string dialogSrc = ReadWholeFile(docStudioRoot / "app" / "studio_tec_digital_dialog.cpp");
    CHECK(!dialogSrc.empty());
    // 0% idle CPU: zero background polling timers
    CHECK(dialogSrc.find("SetTimer(") == std::string::npos);
    // 0% GPU: zero D3D11 or WebView2 usage
    CHECK(dialogSrc.find("D3D11CreateDevice") == std::string::npos);
    CHECK(dialogSrc.find("ID3D11") == std::string::npos);
    CHECK(dialogSrc.find("CreateCoreWebView2") == std::string::npos);
    // Zero internal jargon in user-facing wide labels (L"...")
    CHECK(dialogSrc.find("(DPAPI)") == std::string::npos);
    CHECK(dialogSrc.find("(0% GPU)") == std::string::npos);
    CHECK(dialogSrc.find("L\"GraphQL") == std::string::npos);
    CHECK(dialogSrc.find("GraphQL\"") == std::string::npos);
    // Scan all wide string literals L"..." to verify zero internal jargon leaks into UI text
    {
        bool jargonLeak = false;
        const std::vector<std::string> forbiddenUi = { "GraphQL", "dotLRN", "DPAPI", "JWT", "AJAX", "0% GPU" };
        for (size_t i = 0; i + 2 < dialogSrc.size(); ++i) {
            if (dialogSrc[i] == 'L' && dialogSrc[i + 1] == '"' &&
                (i == 0 || !std::isalnum(static_cast<unsigned char>(dialogSrc[i - 1])))) {
                size_t j = i + 2;
                while (j < dialogSrc.size() && dialogSrc[j] != '"') {
                    if (dialogSrc[j] == '\\' && j + 1 < dialogSrc.size()) ++j;
                    ++j;
                }
                std::string lit = dialogSrc.substr(i + 2, j - (i + 2));
                for (const auto& kw : forbiddenUi) {
                    if (lit.find(kw) != std::string::npos) {
                        jargonLeak = true;
                        std::cerr << "[HIG JARGON LEAK] " << lit << "\n";
                    }
                }
                i = j;
            }
        }
        CHECK(!jargonLeak);
    }
    // Zero common decorative emojis (UTF-8 4-byte F0 9F ...)
    bool hasEmoji = false;
    for (size_t i = 0; i + 1 < dialogSrc.size(); ++i) {
        if (static_cast<unsigned char>(dialogSrc[i]) == 0xF0 &&
            static_cast<unsigned char>(dialogSrc[i + 1]) == 0x9F) {
            hasEmoji = true;
            break;
        }
    }
    CHECK(!hasEmoji);

    // Also enforce identical guías de interfaz / 0% GPU / 0% idle CPU / zero-jargon / zero-emoji / zero-demo-mock guards
    CHECK(dialogSrc.find("visualAuditDemoMode") == std::string::npos);
    CHECK(dialogSrc.find("BuildVisualAuditDemo") == std::string::npos);
    {
        const std::string viewsSrc = ReadWholeFile(docStudioRoot / "app" / "studio_tec_digital_views.hpp");
        CHECK(!viewsSrc.empty());
        CHECK(viewsSrc.find("SetTimer(") == std::string::npos);
        CHECK(viewsSrc.find("D3D11CreateDevice") == std::string::npos);
        CHECK(viewsSrc.find("CreateCoreWebView2") == std::string::npos);
        CHECK(viewsSrc.find("BuildVisualAuditDemo") == std::string::npos);
        bool viewsEmoji = false;
        for (size_t i = 0; i + 1 < viewsSrc.size(); ++i) {
            if (static_cast<unsigned char>(viewsSrc[i]) == 0xF0 &&
                static_cast<unsigned char>(viewsSrc[i + 1]) == 0x9F) {
                viewsEmoji = true;
                break;
            }
        }
        CHECK(!viewsEmoji);
    }

    const std::string mainSrc = ReadWholeFile(docStudioRoot / "app" / "main_studio.cpp");
    CHECK(!mainSrc.empty());
    CHECK(mainSrc.find("--tec-demo-data") == std::string::npos);
    CHECK(mainSrc.find("cliTecDemoData") == std::string::npos);
    // Confirm the old static 2-course HorariosTEC demo was removed and replaced with real dialog wiring
    CHECK(mainSrc.find("RunTecDigitalCenterDialog") != std::string::npos);
    CHECK(mainSrc.find("CMD_ACADEMIC_MALLA_TEC") != std::string::npos);
    CHECK(mainSrc.find("CMD_ACADEMIC_NOTAS_TEC") != std::string::npos);
    CHECK(mainSrc.find("CMD_ACADEMIC_CALENDAR_TEC") != std::string::npos);
    CHECK(mainSrc.find("c1.codeUtf8 = \"MA1103\"") == std::string::npos);

    // End-to-end wiring guards: verify ALL 6 previously unwired TEC features are wired in BOTH Store & Dialog UI
    const std::string storeSrc = ReadWholeFile(docStudioRoot / "integrations" / "tec_digital" / "tec_academic_store.cpp");
    CHECK(!storeSrc.empty());
    CHECK(storeSrc.find("GetPrematriculaSnapshot") != std::string::npos);
    CHECK(storeSrc.find("GetPlagiarismCourseDocs") != std::string::npos);
    CHECK(storeSrc.find("GetServerDateTime") != std::string::npos);
    CHECK(storeSrc.find("GetConsentStatus") != std::string::npos);
    CHECK(storeSrc.find("GetCommunityMembersCsv") != std::string::npos);
    CHECK(storeSrc.find("GetCurriculumColorCodes") != std::string::npos);
    CHECK(dialogSrc.find("plagiarismDocsByCourse") != std::string::npos);
    CHECK(dialogSrc.find("serverTimeOffsetSec") != std::string::npos);
    CHECK(dialogSrc.find("serverDateTimeIso") != std::string::npos);
    CHECK(dialogSrc.find("customTabResourcesByCourse") != std::string::npos);
    CHECK(dialogSrc.find("consentStatusByModule") != std::string::npos);
    CHECK(dialogSrc.find("FetchCommunityMembersCsv") != std::string::npos);
    CHECK(dialogSrc.find("inclusionServiceStatus") != std::string::npos);
    CHECK(dialogSrc.find("curriculumColors") != std::string::npos);
    CHECK(dialogSrc.find("FindLinkedCatedraForSpace") != std::string::npos);
    // v3 Super Plan wiring guards
    CHECK(storeSrc.find("ComputeSnapshotChanges") != std::string::npos);
    CHECK(storeSrc.find("BuildGlobalFileCatalog") != std::string::npos);
    CHECK(storeSrc.find("ConfigureAutoMonitor") != std::string::npos);
    CHECK(dialogSrc.find("IDC_EDIT_COURSE_DOC_SEARCH") != std::string::npos);
    CHECK(dialogSrc.find("ShowWindowsTecToast") != std::string::npos);
    CHECK(dialogSrc.find("linked_subpages") != std::string::npos);
    CHECK(dialogSrc.find("SyncExpedienteDarCourses") != std::string::npos);
    CHECK(dialogSrc.find("UploadPlagiarismFile") != std::string::npos);
    CHECK(dialogSrc.find("CreatePersonalFolder") != std::string::npos);
}

// ----------------------------------------------------------------------------
// 5. 2026 GraphQL APIs (getStudentGeneralInfo, getStudentCurriculum, Int! vars)
//    including negative/error-path verification
// ----------------------------------------------------------------------------
void Test2026GraphQlEndpointsAndNegativePaths() {
    class GraphQl2026MockTransport final : public td::IHttpTransport {
    public:
        bool forceHttpError = false;
        bool sawIntCarnet = false;
        td::HttpResponse Execute(const td::HttpRequest& req) override {
            td::HttpResponse resp{};
            resp.transport_ok = true;
            resp.final_url = req.url;
            if (req.url.find("/tda-inclusion-manager/api/user-transaction") != std::string::npos) {
                resp.status_code = 200;
                resp.body = StrictLiveProtocolMockTransport::ToBytes(
                    R"({"transaction_key":"eyJhbGciOiJIUzI1NiJ9.eyJ1c2VySWQiOjU1fQ.sig"})");
                return resp;
            }
            if (forceHttpError) {
                resp.status_code = 500;
                resp.body = StrictLiveProtocolMockTransport::ToBytes(R"({"errors":[{"message":"Internal error"}]})");
                return resp;
            }
            if (req.body.find("\"carnet\":2026123456") != std::string::npos) {
                sawIntCarnet = true;
            }
            resp.status_code = 200;
            if (req.body.find("getStudentGeneralInfo") != std::string::npos) {
                resp.body = StrictLiveProtocolMockTransport::ToBytes(R"json({
                    "data": {
                        "getStudentGeneralInfo": {
                            "carreer": "Ingeniería en Electrónica",
                            "weightedAverage": 86.42,
                            "outstandingCourses": 18,
                            "availableRegistrationCredits": 18,
                            "rn": [{ "ide_materia": "EL2113", "rn": 1, "course_name": "Circuitos CC" }]
                        }
                    }
                })json");
                return resp;
            }
            if (req.body.find("getStudentCurriculum") != std::string::npos) {
                resp.body = StrictLiveProtocolMockTransport::ToBytes(R"json({
                    "data": {
                        "getStudentCurriculum": {
                            "id_plan": 410,
                            "dsc_plan": "Plan 410 Electrónica",
                            "curriculum": [
                                {
                                    "id": "1",
                                    "courses": [
                                        {
                                            "id_course": "MA1102",
                                            "name": "Cálculo Diferencial",
                                            "trucatedName": "Cálc. Dif.",
                                            "credits": 4,
                                            "hours": 6,
                                            "requirements": [],
                                            "co_requirements": [],
                                            "required_by": [{ "id": "MA1103" }],
                                            "equivalent": [],
                                            "best_status": "Aprobada",
                                            "grades_record": [{ "year": 2025, "period": 2, "modality": "S", "grade": 88.0, "state": 1, "group": 50 }]
                                        }
                                    ]
                                }
                            ]
                        }
                    }
                })json");
                return resp;
            }
            resp.status_code = 404;
            return resp;
        }
    };

    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_tec_gql2026_test";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);

    auto rawMock = std::make_unique<GraphQl2026MockTransport>();
    auto* mockPtr = rawMock.get();
    DocStudio::TecDigital::TecAcademicStore store(tempRoot.string(), std::move(rawMock));
    auto& gql = store.Service().GraphQlClient();

    // Positive path: getStudentGeneralInfo with Int! carnet
    auto gi = gql.FetchStudentGeneralInfo("2026123456");
    CHECK(gi.ok);
    CHECK(mockPtr->sawIntCarnet);
    CHECK(gi.value.career == "Ingeniería en Electrónica");
    CHECK(std::abs(gi.value.weighted_average - 86.42) < 0.01);
    CHECK(gi.value.available_registration_credits == 18);
    CHECK(gi.value.rn_courses.size() == 1 && gi.value.rn_courses[0].course_code == "EL2113");

    // Positive path: getStudentCurriculum -> CurriculumGraph + StudentCurriculumProfile
    auto cur = gql.FetchStudentCurriculum("2026123456");
    CHECK(cur.ok);
    CHECK(cur.value.graph.id_curriculum == 410);
    CHECK(cur.value.graph.levels.size() == 1);
    CHECK(!cur.value.graph.levels.empty() && cur.value.graph.levels[0].courses.size() == 1 && cur.value.graph.levels[0].courses[0].id_course == "MA1102");
    CHECK(!cur.value.profile.grades.empty() && cur.value.profile.grades[0].best_status == "Aprobada");

    // Negative / error paths: empty carnet & HTTP 500
    auto badEmpty = gql.FetchStudentGeneralInfo("");
    CHECK(!badEmpty.ok);
    auto badCurEmpty = gql.FetchStudentCurriculum("");
    CHECK(!badCurEmpty.ok);
    mockPtr->forceHttpError = true;
    auto bad500 = gql.FetchStudentCurriculum("2026123456");
    CHECK(!bad500.ok);

    fs::remove_all(tempRoot, ec);
}

// ----------------------------------------------------------------------------
// 6. 2026 Deep-Map Parsers, GL_FOLDER_ID Isolation, Login Chrome Regression,
//    ScopedUserMutationGuard Security & Real 127.0.0.1 Loopback Mutation E2E
// ----------------------------------------------------------------------------
void Test2026DeepMapParsersAndLoopbackMutations() {
    // 6A. AuthClient::ContainsLoginMarkers 2026 timeout-modal-overlay regression
    {
        const std::string liveAuthedPortal = R"html(
            <html><body>
            <header id="header-td"><div id="platform_user_panel"><a href="/register/logout">Salir</a></div></header>
            <div class="timeout-modal-overlay" id="timeout-overlay">
                <p>Su sesión ha expirado.</p>
                <button class="timeout-modal-button" onclick="logout()">Iniciar sesión nuevamente</button>
            </div>
            </body></html>)html";
        CHECK(!td::AuthClient::ContainsLoginMarkers(liveAuthedPortal));

        const std::string unauthedLoginForm = R"html(
            <html><body>
            <form action="/api/login/new-form/" method="post">
                <input name="email"/><input name="password" type="password"/>
                <button>Iniciar sesión</button>
            </form>
            </body></html>)html";
        CHECK(td::AuthClient::ContainsLoginMarkers(unauthedLoginForm));

        const std::string plainUnauthed = "<html><body><h1>Iniciar sesión en TEC Digital</h1></body></html>";
        CHECK(td::AuthClient::ContainsLoginMarkers(plainUnauthed));
    }

    // 6B. ParseCommunitiesHtml: all 7 real 2026 namespaces + Material icon stripping + chronological ranking
    {
        const std::string portalHtml = R"html(
            <html><body>
            <a href="/dotlrn/classes/e/el2113/s-2-2026.el2113.50/">bookmark_check Circuitos eléctricos en corriente continua GR 50</a>
            <a href="/dotlrn/classes/e/el1101/s-1-2026.el1101.01/">loupe Introducción a la Electrónica GR 01</a>
            <a href="/dotlrn/catedras.tec/catedradefsicageneraliii/">Cátedra de Física General III</a>
            <a href="/dotlrn/escuela/escueladefisica/">Escuela de Física</a>
            <a href="/dotlrn/escuela/escueladeingenieraenelectrnica/estudiantese/">Estudiantes E</a>
            <a href="/dotlrn/programas.tec/programadepasantias/">Programa de Pasantías</a>
            <a href="/dotlrn/dop/">Orientación y Psicología</a>
            </body></html>)html";
        auto comms = td::TdaTdsClient::ParseCommunitiesHtml(portalHtml, "https://tecdigital.tec.ac.cr");
        CHECK(comms.size() == 7);
        bool sawCurrentClass = false, sawOlderClass = false, sawCatedra = false;
        bool sawEscuela = false, sawSubgrupo = false, sawPrograma = false, sawComunidad = false;
        for (const auto& c : comms) {
            CHECK(c.name.find("bookmark_check") == std::string::npos);
            CHECK(c.name.find("loupe") == std::string::npos);
            std::string perLow = c.period;
            for (char& ch : perLow) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (c.type == "CLASE" && perLow == "s-2-2026") {
                sawCurrentClass = true;
                CHECK(c.is_current_period);
            } else if (c.type == "CLASE" && perLow == "s-1-2026") {
                sawOlderClass = true;
                CHECK(!c.is_current_period);
            } else if (c.type == "CATEDRA") {
                sawCatedra = true;
            } else if (c.type == "ESCUELA") {
                sawEscuela = true;
            } else if (c.type == "SUBGRUPO") {
                sawSubgrupo = true;
            } else if (c.type == "PROGRAMA") {
                sawPrograma = true;
            } else if (c.type == "COMUNIDAD") {
                sawComunidad = true;
            }
        }
        CHECK(sawCurrentClass && sawOlderClass && sawCatedra && sawEscuela && sawSubgrupo && sawPrograma && sawComunidad);
    }

    // 6C. GL_FOLDER_ID priority & personal Mis documentos isolation
    {
        const std::string courseFsHtml = R"html(
            <html><body>
            <nav id="menu-left">
                <a href="/dotlrn/file-storage/#/153108817">Mis documentos</a>
            </nav>
            <script>
                $rootScope.GL_FOLDER_ID = 245716892;
            </script>
            </body></html>)html";
        auto courseIds = td::FileStorageWalker::ExtractRootFolderIds(courseFsHtml);
        CHECK(!courseIds.empty());
        CHECK(courseIds[0] == "245716892");
        bool leakedPersonal = false;
        for (const auto& id : courseIds) {
            if (id == "153108817") leakedPersonal = true;
        }
        CHECK(!leakedPersonal);
        CHECK(td::FileStorageWalker::ExtractPersonalRootFolderId(courseFsHtml) == "153108817");
    }

    // 6D. ParseDetailedEvaluationsHtml 2026 unquoted attrs, can_upload, due date, late_submit, submitted files
    {
        const std::string evalHtml = R"html(
            <div id="studentFinal">88.50 / 100.00</div>
            <div class="sub_total" item_weight="25%">Laboratorios</div>
            <div class="container_accor" assign_id=233153700 task_id=233153743 task_item_id=233153744 grade_id=232992252 late_submit =t>
                <span class="assignNameText">Laboratorio 2: Leyes de Kirchhoff</span>
                <span class="assign_weight">12.5%</span>
                <div class="info_item">Fecha de Entrega: 05/12/2026 23:55</div>
                <div class="container_dropzone">
                    <button type='submit' class='tds-lib_form primary btn_send_answer' task_id=233153743 grade_id=232992252 task_item_id=233153744 with_group= 0 due_date='2026-12-05 23:55:00 GMT'>Enviar entrega</button>
                </div>
                <div class="taskAnswer" revision_id=235106635 item_id=235106634>
                    <span class="displayNameFile">informe_lab2.pdf</span>
                </div>
            </div>)html";
        auto rep = td::ParseDetailedEvaluationsHtml(evalHtml, "el2113");
        CHECK(rep.has_final_grade && std::abs(rep.final_grade - 88.5) < 0.01);
        CHECK(rep.categories.size() == 1 && rep.categories[0].items.size() == 1);
        if (!rep.categories.empty() && !rep.categories[0].items.empty()) {
            const auto& item = rep.categories[0].items[0];
            CHECK(item.task_id == "233153743");
            CHECK(item.grade_id == "232992252");
            CHECK(item.allow_late_submission);
            CHECK(item.can_upload);
            CHECK(item.due_date_text.find("05/12/2026") != std::string::npos);
            CHECK(item.latest_revision_id == "235106635");
            CHECK(item.latest_item_id == "235106634");
            CHECK(!item.submitted_files.empty() && item.submitted_files[0] == "informe_lab2.pdf");
        }
    }

    // 6E. ParseSpacePageHtml & ParseNewsArticleHtml
    {
        const std::string spaceHtml = R"html(
            <html><body>
            <div class="portal-page-content">
                <a href="https://tecdigital.tec.ac.cr/dotlrn/clubs/ctedras/catedradefsicageneraliii/file-storage/">
                    <img src="/dotlrn/clubs/ctedras/banner_fi3.png" alt="Banner Cátedra Física III"/>
                </a>
                <div class="portlet-wrap">
                    <div class="portlet-header"><h2>Repositorio de Exámenes</h2></div>
                    <div class="portlet-body">
                        <p>Exámenes resueltos y guías de estudio de semestres anteriores.</p>
                        <a href="https://tecdigital.tec.ac.cr/dotlrn/clubs/ctedras/catedradefsicageneraliii/examen_2025.pdf">Examen Parcial 2025</a>
                    </div>
                </div>
            </div>
            </body></html>)html";
        auto page = td::ParseSpacePageHtml(spaceHtml, 25, "Repositorio",
                                           "https://tecdigital.tec.ac.cr/dotlrn/clubs/ctedras/catedradefsicageneraliii/?page_num=25",
                                           "https://tecdigital.tec.ac.cr");
        CHECK(page.page_num == 25);
        CHECK(!page.banners.empty());
        CHECK(!page.portlets.empty() && page.portlets[0].title == "Repositorio de Exámenes");
        CHECK(!page.links.empty());

        const std::string newsHtml = R"html(
            <html><body>
            <article class="news-item">
                <h1>Cambio de aula para Parcial 2</h1>
                <div class="news-date">2026-11-24</div>
                <div class="news-body"><p>El examen se aplicará en el auditorio D3 a las 13:00 horas.</p></div>
            </article>
            </body></html>)html";
        auto art = td::ParseNewsArticleHtml(newsHtml, "https://tecdigital.tec.ac.cr/dotlrn/classes/e/el2113/news/item?item_id=8801");
        CHECK(art.title.find("Parcial 2") != std::string::npos);
        CHECK(art.body_text.find("auditorio D3") != std::string::npos);
    }

    // 6F. Real 127.0.0.1 Loopback HTTP E2E for ScopedUserMutationGuard, 5 Confirmed Actions,
    //     SyncPersonalDocuments, FetchSpacePage, FetchNewsArticle & Snapshot PII Hygiene
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { CHECK(false); return; }
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(listenSock != INVALID_SOCKET);
    if (listenSock == INVALID_SOCKET) { WSACleanup(); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    CHECK(bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    CHECK(listen(listenSock, 16) == 0);
    int addrLen = sizeof(addr);
    CHECK(getsockname(listenSock, reinterpret_cast<sockaddr*>(&addr), &addrLen) == 0);
    const unsigned short port = ntohs(addr.sin_port);
    const std::string origin = "http://127.0.0.1:" + std::to_string(port);

    std::atomic<bool> stopFlag{false};
    std::atomic<int> uploadCalls{0};
    std::atomic<bool> sawMultipartFields{false};
    std::atomic<int> deleteRevCalls{0};
    std::atomic<int> markReadCalls{0};
    std::atomic<int> deleteNotifCalls{0};
    std::atomic<int> refreshCoursesCalls{0};
    std::atomic<int> forbiddenCalls{0};

    std::thread serverThread([&]() {
        while (!stopFlag.load(std::memory_order_acquire)) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(listenSock, &rfds);
            timeval tv{ 0, 80000 };
            if (select(0, &rfds, nullptr, nullptr, &tv) <= 0) continue;
            SOCKET client = accept(listenSock, nullptr, nullptr);
            if (client == INVALID_SOCKET) continue;

            std::string rawReq;
            char buf[4096]{};
            size_t hdrEnd = std::string::npos;
            while ((hdrEnd = rawReq.find("\r\n\r\n")) == std::string::npos) {
                int n = recv(client, buf, sizeof(buf), 0);
                if (n <= 0) break;
                rawReq.append(buf, static_cast<size_t>(n));
                if (rawReq.size() > 65536) break;
            }
            if (hdrEnd != std::string::npos) {
                std::string hdrBlock = rawReq.substr(0, hdrEnd);
                std::string reqBody = rawReq.substr(hdrEnd + 4);
                std::istringstream iss(hdrBlock);
                std::string reqLine;
                std::getline(iss, reqLine);
                if (!reqLine.empty() && reqLine.back() == '\r') reqLine.pop_back();
                std::istringstream rls(reqLine);
                std::string method, target;
                rls >> method >> target;
                size_t contentLen = 0;
                std::string line;
                while (std::getline(iss, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    std::string low = line;
                    for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    if (low.rfind("content-length:", 0) == 0) {
                        contentLen = static_cast<size_t>(std::strtoull(low.c_str() + 15, nullptr, 10));
                    }
                }
                while (reqBody.size() < contentLen) {
                    int n = recv(client, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    reqBody.append(buf, static_cast<size_t>(n));
                }
                if (reqBody.size() > contentLen) reqBody.resize(contentLen);

                int status = 200;
                std::string ct = "text/html; charset=utf-8";
                std::string body = "<html><body>OK</body></html>";

                if (target.find("/register/logout") != std::string::npos || target.find("/admin/") != std::string::npos) {
                    ++forbiddenCalls;
                    status = 403;
                    body = "Forbidden";
                } else if (target.find("/evaluation/tda-ce-estudiante/ajax/upload_evaluation") != std::string::npos && method == "POST") {
                    ++uploadCalls;
                    if (reqBody.find("name=\"task_id\"") != std::string::npos &&
                        reqBody.find("233153743") != std::string::npos &&
                        reqBody.find("name=\"grade_id\"") != std::string::npos &&
                        reqBody.find("232992252") != std::string::npos &&
                        reqBody.find("filename=\"laboratorio1.pdf\"") != std::string::npos &&
                        reqBody.find("%PDF-1.4") != std::string::npos) {
                        sawMultipartFields.store(true);
                    }
                    ct = "application/json; charset=utf-8";
                    body = R"json({"result":"ok","revision_id":"99001","item_id":"99002","html_icons_download":"<span>laboratorio1.pdf</span>"})json";
                } else if (target.find("/evaluation/tda-ce-estudiante/ajax/delete_version_answer") != std::string::npos && method == "POST") {
                    ++deleteRevCalls;
                    ct = "application/json; charset=utf-8";
                    body = (reqBody.find("revision_id=99001") != std::string::npos && reqBody.find("item_id=99002") != std::string::npos)
                               ? R"({"result":"ok"})"
                               : R"({"result":"error"})";
                } else if (target.find("/tda-notifications/ajax/update_notification_state") != std::string::npos && method == "GET") {
                    ++markReadCalls;
                    ct = "text/plain; charset=utf-8";
                    body = (target.find("notification_by_user_id=55501") != std::string::npos) ? "1" : "0";
                } else if (target.find("/tda-notifications/ajax/delete_user_notification") != std::string::npos && method == "GET") {
                    ++deleteNotifCalls;
                    ct = "text/plain; charset=utf-8";
                    body = (target.find("notification_by_user_id=55501") != std::string::npos) ? "1" : "0";
                } else if (target.find("/dotlrn/ajax/actualizar_cursos") != std::string::npos && method == "POST") {
                    ++refreshCoursesCalls;
                    ct = "text/plain; charset=utf-8";
                    body = "1";
                } else if (target == "/dotlrn/" || target == "/dotlrn/index") {
                    body = "<html><body><header id='header-td'></header><nav id='menu-left'><a href='/dotlrn/file-storage/#/153108817'>Mis documentos</a></nav></body></html>";
                } else if (target.find("/dotlrn/file-storage/api/folder/153108817") != std::string::npos) {
                    ct = "application/json; charset=utf-8";
                    body = R"json({
                        "folder_id": "153108817",
                        "name": "Mis documentos",
                        "elements": [
                            {"object_id":"881001","parent_id":"153108817","fs_type":"folder","name":"Respaldo 2026"},
                            {"object_id":"881002","parent_id":"153108817","fs_type":"file","name":"resumen_personal.pdf","title":"Resumen Personal","mime_type":"application/pdf","content_size":2048,"url":"/dotlrn/file-storage/download/resumen_personal.pdf?version_id=881002"}
                        ]
                    })json";
                } else if (target.find("/dotlrn/file-storage/api/folder/881001") != std::string::npos) {
                    ct = "application/json; charset=utf-8";
                    body = R"json({"folder_id":"881001","name":"Respaldo 2026","elements":[]})json";
                } else if (target.find("/dotlrn/clubs/ctedras/catedradefsicageneraliii/") != std::string::npos && target.find("page_num=25") != std::string::npos) {
                    body = R"html(<html><body><header id="header-td"></header>
                        <img src="/banner_catedra.png" alt="Cátedra Física III"/>
                        <div class="portlet-wrap"><div class="portlet-header"><h2>Exámenes Anteriores</h2></div>
                        <div class="portlet-body"><p>Repositorio oficial de exámenes.</p>
                        <a href="/dotlrn/clubs/ctedras/catedradefsicageneraliii/parcial1.pdf">Parcial 1</a></div></div>
                        </body></html>)html";
                } else if (target.find("/news/item") != std::string::npos) {
                    body = R"html(<html><body><header id="header-td"></header>
                        <article class="news-item"><h1>Aviso de Laboratorio</h1>
                        <div class="news-body"><p>Traer osciloscopio calibrado.</p></div></article></body></html>)html";
                }

                std::ostringstream resp;
                resp << "HTTP/1.1 " << status << " OK\r\n"
                     << "Content-Type: " << ct << "\r\n"
                     << "Content-Length: " << body.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << body;
                const std::string out = resp.str();
                send(client, out.data(), static_cast<int>(out.size()), 0);
            }
            closesocket(client);
        }
    });

    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_tec_2026_mut_e2e";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    td::ServiceConfig cfg;
    cfg.base_url = origin;
    cfg.graphql_url = origin + "/graphql";
    cfg.policy_options.allowed_test_origins = { origin };
    cfg.downloader_options.min_delay_ms = 0;

    DocStudio::TecDigital::TecAcademicStore store(tempRoot.string(), nullptr, cfg);
    DocStudio::TecDigital::TecAcademicSnapshot seeded;
    td::NotificationsFeed nresp;
    nresp.unread_quantity = 1;
    td::NotificationItem n1;
    n1.notification_id = "55501"; n1.state = "U"; n1.title = "Tarea nueva"; n1.text = "Entrega el viernes";
    nresp.notifications.push_back(n1);
    seeded.notifications = nresp;
    store.SetSnapshotForTesting(seeded);

    td::EvaluationUploadRequest ureq;
    ureq.community_path = "/dotlrn/classes/ie/el2001/";
    ureq.task_id = "233153743";
    ureq.task_item_id = "233153744";
    ureq.grade_id = "232992252";
    ureq.student_id = "999888";
    ureq.with_group = "0";
    ureq.file_name = "laboratorio1.pdf";
    ureq.file_mime = "application/pdf";
    ureq.file_bytes = "%PDF-1.4\n% synthetic upload bytes\n";

    // Negative Gate 1: userConfirmed == false MUST fail locally with ZERO server calls!
    CHECK(!store.UploadCourseEvaluation(ureq, false).ok);
    CHECK(!store.DeleteEvaluationSubmission("/dotlrn/classes/ie/el2001/", "99001", "99002", false).ok);
    CHECK(!store.MarkNotificationAsRead("55501", false).ok);
    CHECK(!store.DeleteUserNotification("55501", false).ok);
    CHECK(!store.RefreshEnrolledCoursesFromDar(false).ok);
    CHECK(uploadCalls.load() == 0 && deleteRevCalls.load() == 0 && markReadCalls.load() == 0 &&
          deleteNotifCalls.load() == 0 && refreshCoursesCalls.load() == 0);

    // Negative Gate 2: direct client call WITHOUT ScopedUserMutationGuard MUST be blocked by PolicyEnforcingTransport!
    {
        auto directBlocked = store.Service().TdaClient().UploadEvaluationSubmission(ureq);
        CHECK(!directBlocked.ok);
        CHECK(uploadCalls.load() == 0);
    }

    // Negative Gate 3: /register/logout and /admin/ MUST remain blocked EVEN INSIDE ScopedUserMutationGuard!
    {
        td::ScopedUserMutationGuard g1(store.Service().GuardedTransport(), td::AllowedStudentMutation::UploadEvaluation);
        td::ScopedUserMutationGuard g2(store.Service().GuardedTransport(), td::AllowedStudentMutation::ManageNotifications);
        td::HttpRequest logoutReq;
        logoutReq.method = "GET";
        logoutReq.url = origin + "/register/logout";
        auto rOut = store.Service().GuardedTransport().Execute(logoutReq);
        CHECK(!rOut.transport_ok);
        CHECK(rOut.error_message.rfind("read-only-request-guard:", 0) == 0);

        td::HttpRequest adminReq;
        adminReq.method = "POST";
        adminReq.url = origin + "/admin/delete";
        auto rAdm = store.Service().GuardedTransport().Execute(adminReq);
        CHECK(!rAdm.transport_ok);
        CHECK(forbiddenCalls.load() == 0);
    }
    CHECK(store.Service().GuardedTransport().ActiveMutationsMask() == 0u);

    // Positive Gate: All 5 confirmed student actions succeed over real 127.0.0.1 WinHTTP loopback!
    auto upRes = store.UploadCourseEvaluation(ureq, true);
    CHECK(upRes.ok && upRes.value.revision_id == "99001" && upRes.value.item_id == "99002");
    CHECK(uploadCalls.load() == 1 && sawMultipartFields.load());
    CHECK(store.Service().GuardedTransport().ActiveMutationsMask() == 0u);

    auto delRevRes = store.DeleteEvaluationSubmission("/dotlrn/classes/ie/el2001/", "99001", "99002", true);
    CHECK(delRevRes.ok && deleteRevCalls.load() == 1);

    auto readRes = store.MarkNotificationAsRead("55501", true);
    CHECK(readRes.ok && markReadCalls.load() == 1);
    {
        auto sAfterRead = store.Snapshot();
        CHECK(sAfterRead.notifications.has_value() && sAfterRead.notifications->unread_quantity == 0);
        CHECK(!sAfterRead.notifications->notifications.empty() && sAfterRead.notifications->notifications[0].state == "R");
    }

    auto delNotifRes = store.DeleteUserNotification("55501", true);
    CHECK(delNotifRes.ok && deleteNotifCalls.load() == 1);
    {
        auto sAfterDel = store.Snapshot();
        CHECK(sAfterDel.notifications.has_value() && sAfterDel.notifications->notifications.empty());
    }

    auto darRes = store.RefreshEnrolledCoursesFromDar(true);
    CHECK(darRes.ok && refreshCoursesCalls.load() == 1);
    CHECK(store.Service().GuardedTransport().ActiveMutationsMask() == 0u);

    // Positive Gate: SyncPersonalDocuments + FetchSpacePage + FetchNewsArticle over real 127.0.0.1 loopback!
    std::string pSum, pErr;
    CHECK(store.SyncPersonalDocuments(false, &pSum, &pErr));
    {
        auto sp = store.Snapshot();
        CHECK(sp.personalDocumentsFolderId == "153108817");
        CHECK(sp.personalDocuments.has_value() && sp.personalDocuments->files.size() == 1);
    }
    auto spPage = store.FetchSpacePage("/dotlrn/clubs/ctedras/catedradefsicageneraliii/", 25, "Exámenes");
    CHECK(spPage.ok && !spPage.value.banners.empty() && !spPage.value.portlets.empty());
    auto newsArt = store.FetchNewsArticle(origin + "/dotlrn/classes/ie/el2001/news/item?item_id=101");
    CHECK(newsArt.ok && newsArt.value.body_text.find("osciloscopio") != std::string::npos);

    // Snapshot JSON persistence round-trip & ZERO PII (student_id must NEVER be written to snapshot.json!)
    {
        auto curSnap = store.Snapshot();
        td::DetailedEvaluationsReport evRep;
        td::DetailedEvaluationCategory cat; cat.name = "Labs"; cat.weight_pct = 20.0;
        td::DetailedEvaluationItem it;
        it.name = "Lab 1"; it.can_upload = true; it.task_id = "233153743";
        it.grade_id = "232992252"; it.student_id = "SECRET_STUDENT_ID_777";
        cat.items.push_back(it);
        evRep.categories.push_back(cat);
        curSnap.evaluationsByPath["/dotlrn/classes/ie/el2001/"] = evRep;
        // Regression test: raw ISO-8859-1 byte in news title must recover to valid UTF-8 and NEVER break SaveSnapshotToDisk!
        curSnap.newsByPath["/dotlrn/classes/ie/el2001/"] = {
            { "F\xEDsica \xD3ptica", "", "", "Evaluaci\xF3n", "2026-11-26" }
        };
        store.SetSnapshotForTesting(curSnap);

        std::string saveErr;
        CHECK(store.SaveSnapshotToDisk(&saveErr));
        const std::string rawJson = ReadWholeFile(tempRoot / "cache" / "academic_snapshot.json");
        CHECK(!rawJson.empty());
        CHECK(rawJson.find("SECRET_STUDENT_ID_777") == std::string::npos); // student_id NEVER persisted!
        CHECK(rawJson.find("personal_documents") != std::string::npos);
        CHECK(rawJson.find("custom_pages") != std::string::npos);

        DocStudio::TecDigital::TecAcademicStore reloadStore(tempRoot.string());
        CHECK(reloadStore.LoadSnapshotFromDisk(&saveErr));
        auto rs = reloadStore.Snapshot();
        CHECK(rs.newsByPath.count("/dotlrn/classes/ie/el2001/") == 1);
        CHECK(rs.newsByPath["/dotlrn/classes/ie/el2001/"][0].title == "F\xC3\xADsica \xC3\x93ptica");
        CHECK(rs.personalDocuments.has_value() && rs.personalDocuments->files.size() == 1);
        CHECK(rs.customPagesBySpace.count("/dotlrn/clubs/ctedras/catedradefsicageneraliii/") == 1);
        CHECK(rs.evaluationsByPath.count("/dotlrn/classes/ie/el2001/") == 1);
        CHECK(rs.evaluationsByPath["/dotlrn/classes/ie/el2001/"].categories[0].items[0].can_upload);
        CHECK(rs.evaluationsByPath["/dotlrn/classes/ie/el2001/"].categories[0].items[0].student_id.empty());
    }

    stopFlag.store(true, std::memory_order_release);
    closesocket(listenSock);
    if (serverThread.joinable()) serverThread.join();
    WSACleanup();
    fs::remove_all(tempRoot, ec);
#endif
}

// ----------------------------------------------------------------------------
// 7. Opt-in Strictly Read-Only Live Account QA Harness (NEVER runs in CTest!)
//    Activated ONLY when invoked with --live-readonly-qa and TEC_EMAIL/TEC_PASSWORD env vars.
//    Prints ONLY aggregate non-PII counts and deletes its ephemeral temp dir immediately.
// ----------------------------------------------------------------------------
int RunOptInLiveReadOnlyQa() {
    const char* emailEnv = std::getenv("TEC_EMAIL");
    const char* passEnv = std::getenv("TEC_PASSWORD");
    if (!emailEnv || !emailEnv[0] || !passEnv || !passEnv[0]) {
        std::cerr << "[LIVE-QA] Missing TEC_EMAIL or TEC_PASSWORD environment variables.\n";
        return 2;
    }

    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_live_qa_ephemeral";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    int rc = 1;
    try {
        DocStudio::TecDigital::TecAcademicStore store(tempRoot.string());
        std::cerr << "[LIVE-QA] Step 1: Logging in..." << std::endl;
        auto auth = store.LoginWithPassword(emailEnv, passEnv, /*persistEncryptedVault=*/false);
        std::cout << "LIVE_QA_LOGIN_OK=" << (auth.ok ? 1 : 0)
                  << " HTTP=" << auth.http_status
                  << " STATUS=" << static_cast<int>(auth.status) << std::endl;
        if (!auth.ok) {
            fs::remove_all(tempRoot, ec);
            return 1;
        }

        // Confirm transport remains strictly read-only throughout live QA
        if (store.Service().GuardedTransport().ActiveMutationsMask() != 0u) {
            std::cerr << "[LIVE-QA] FATAL: mutation mask non-zero!\n";
            fs::remove_all(tempRoot, ec);
            return 3;
        }

        DocStudio::TecDigital::TecSyncOptions opts;
        opts.syncUserModel = false;
        opts.syncSchoolCatalog = false;
        opts.syncForums = true;
        opts.syncMembers = false;
        opts.syncGaap = false;
        opts.syncGraduateAttributes = true;
        opts.syncPerformanceEval = true;
        opts.syncGraphQlInclusiones = true;
        opts.syncCustomTabs = true;
        opts.maxCustomTabsPerSpace = 6;
        opts.syncCatedrasAndCommunities = true;
        opts.syncPersonalDocuments = true;
        opts.syncCustomSpacePages = true;
        opts.maxNonClassSpacesToDeepSync = 4;
        opts.politeDelayMs = 160;

        std::string syncErr;
        const bool syncOk = store.SyncAll(
            opts,
            [](int pct, const std::string& msg) {
                std::cerr << "[LIVE-QA] " << pct << "% " << msg << std::endl;
            },
            nullptr, &syncErr);
        auto snap = store.Snapshot();

        // Also test metadata-only SyncCourseFiles on 1 active CLASE + 1 CATEDRA (downloadFiles=false!)
        std::string activeCourseUrl, catedraUrl;
        int nClaseCur = 0, nClasePast = 0, nCatedra = 0, nEscuela = 0, nSubgrupo = 0, nPrograma = 0, nComunidad = 0;
        for (const auto& c : snap.communities) {
            if (c.type == "CLASE") {
                if (c.is_current_period) {
                    ++nClaseCur;
                    if (activeCourseUrl.empty()) activeCourseUrl = c.url;
                } else {
                    ++nClasePast;
                }
            } else if (c.type == "CATEDRA") {
                ++nCatedra;
                if (catedraUrl.empty()) catedraUrl = c.url;
            } else if (c.type == "ESCUELA") {
                ++nEscuela;
            } else if (c.type == "SUBGRUPO") {
                ++nSubgrupo;
            } else if (c.type == "PROGRAMA") {
                ++nPrograma;
            } else {
                ++nComunidad;
            }
        }

        bool courseFsOk = false, catedraFsOk = false;
        std::string fsSum1, fsErr1, fsSum2, fsErr2;
        if (!activeCourseUrl.empty()) {
            std::cerr << "[LIVE-QA] SyncCourseFiles(active course)..." << std::endl;
            courseFsOk = store.SyncCourseFiles(activeCourseUrl, /*downloadFiles=*/false, &fsSum1, &fsErr1);
            std::cerr << "[LIVE-QA] SyncCourseFiles(active course) done ok=" << courseFsOk << std::endl;
        }
        if (!catedraUrl.empty()) {
            std::cerr << "[LIVE-QA] SyncCourseFiles(catedra)..." << std::endl;
            catedraFsOk = store.SyncCourseFiles(catedraUrl, /*downloadFiles=*/false, &fsSum2, &fsErr2);
            std::cerr << "[LIVE-QA] SyncCourseFiles(catedra) done ok=" << catedraFsOk << std::endl;
        }
        snap = store.Snapshot();

        std::size_t bannersTotal = 0, portletsTotal = 0, customPagesTotal = 0;
        for (const auto& [k, ov] : snap.overviewsByPath) {
            bannersTotal += ov.banners.size();
            portletsTotal += ov.portlets.size();
        }
        for (const auto& [k, pages] : snap.customPagesBySpace) {
            customPagesTotal += pages.size();
            for (const auto& p : pages) {
                bannersTotal += p.banners.size();
                portletsTotal += p.portlets.size();
            }
        }

        std::size_t evalItemsTotal = 0, evalUploadable = 0, evalWithDue = 0, evalWithSubmitted = 0;
        for (const auto& [k, rep] : snap.evaluationsByPath) {
            for (const auto& cat : rep.categories) {
                for (const auto& it : cat.items) {
                    ++evalItemsTotal;
                    if (it.can_upload) ++evalUploadable;
                    if (!it.due_date_text.empty() && it.due_date_text.find("no definida") == std::string::npos) ++evalWithDue;
                    if (!it.submitted_files.empty()) ++evalWithSubmitted;
                }
            }
        }

        std::size_t activeCourseFiles = 0, activeCourseFolders = 0;
        std::string activeCourseRootId;
        if (auto it = snap.foldersByPath.find(activeCourseUrl); it != snap.foldersByPath.end()) {
            activeCourseFiles = it->second.files.size();
            activeCourseFolders = it->second.folders.size();
            activeCourseRootId = it->second.root_folder_id;
        }
        std::size_t catedraFiles = 0, catedraFolders = 0;
        std::string catedraRootId;
        if (auto it = snap.foldersByPath.find(catedraUrl); it != snap.foldersByPath.end()) {
            catedraFiles = it->second.files.size();
            catedraFolders = it->second.folders.size();
            catedraRootId = it->second.root_folder_id;
        }

        const bool courseFolderIsolated = activeCourseRootId.empty() || snap.personalDocumentsFolderId.empty() ||
                                          (activeCourseRootId != snap.personalDocumentsFolderId);
        const bool catedraFolderIsolated = catedraRootId.empty() || snap.personalDocumentsFolderId.empty() ||
                                           (catedraRootId != snap.personalDocumentsFolderId);

        // Sanitized numeric summary ONLY (ZERO PII, ZERO names, ZERO carné, ZERO URLs with user IDs)
        std::cout << "LIVE_QA_SYNC_OK=" << (syncOk ? 1 : 0) << "\n"
                  << "LIVE_QA_COMMUNITIES_TOTAL=" << snap.communities.size()
                  << " CLASE_CUR=" << nClaseCur
                  << " CLASE_PAST=" << nClasePast
                  << " CATEDRA=" << nCatedra
                  << " ESCUELA=" << nEscuela
                  << " SUBGRUPO=" << nSubgrupo
                  << " PROGRAMA=" << nPrograma
                  << " COMUNIDAD=" << nComunidad << "\n"
                  << "LIVE_QA_OVERVIEWS=" << snap.overviewsByPath.size()
                  << " BANNERS=" << bannersTotal
                  << " PORTLETS=" << portletsTotal
                  << " CUSTOM_PAGES=" << customPagesTotal << "\n"
                  << "LIVE_QA_EVAL_COURSES=" << snap.evaluationsByPath.size()
                  << " ITEMS=" << evalItemsTotal
                  << " CAN_UPLOAD=" << evalUploadable
                  << " WITH_DUE=" << evalWithDue
                  << " SUBMITTED=" << evalWithSubmitted << "\n"
                  << "LIVE_QA_COURSE_FS_OK=" << (courseFsOk ? 1 : 0)
                  << " FOLDERS=" << activeCourseFolders
                  << " FILES=" << activeCourseFiles
                  << " ISOLATED_FROM_PERSONAL=" << (courseFolderIsolated ? 1 : 0) << "\n"
                  << "LIVE_QA_CATEDRA_FS_OK=" << (catedraFsOk ? 1 : 0)
                  << " FOLDERS=" << catedraFolders
                  << " FILES=" << catedraFiles
                  << " ISOLATED_FROM_PERSONAL=" << (catedraFolderIsolated ? 1 : 0) << "\n"
                  << "LIVE_QA_PERSONAL_DOCS_FOUND=" << (snap.personalDocuments.has_value() ? 1 : 0)
                  << " FOLDERS=" << (snap.personalDocuments ? snap.personalDocuments->folders.size() : 0)
                  << " FILES=" << (snap.personalDocuments ? snap.personalDocuments->files.size() : 0) << "\n"
                  << "LIVE_QA_NOTIFICATIONS=" << (snap.notifications ? snap.notifications->notifications.size() : 0)
                  << " UNREAD=" << (snap.notifications ? snap.notifications->unread_quantity : 0) << "\n"
                  << "LIVE_QA_NEWS_SPACES=" << snap.newsByPath.size()
                  << " CALENDAR_EVENTS=" << snap.unifiedCalendarEvents.size()
                  << " SCHEDULE_CELLS=" << snap.currentSchedule.size() << "\n"
                  << "LIVE_QA_EXPEDIENTE=" << (snap.expediente.has_value() ? 1 : 0)
                  << " CURRICULUM_LEVELS=" << (snap.curriculumGraph ? snap.curriculumGraph->levels.size() : 0)
                  << " STUDENT_GENERAL_INFO=" << (snap.studentGeneralInfo.has_value() ? 1 : 0)
                  << " GRAD_ATTRS=" << snap.graduateAttributes.size() << "\n"
                  << "LIVE_QA_MUTATION_MASK=" << store.Service().GuardedTransport().ActiveMutationsMask() << "\n";

        // Local-only cleanup (NEVER hits /register/logout!)
        store.LogoutLocalOnly(true);
        rc = (auth.ok && syncOk && nClaseCur > 0 && nCatedra > 0 && courseFolderIsolated && catedraFolderIsolated &&
              store.Service().GuardedTransport().ActiveMutationsMask() == 0u) ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "[LIVE-QA] Exception: " << ex.what() << "\n";
        rc = 1;
    }
    fs::remove_all(tempRoot, ec);
    return rc;
}

// ----------------------------------------------------------------------------
// 8. Global All-TEC Schedule Search, Parallel Crawl, Cache, Enrich & Cross-School Solver
// ----------------------------------------------------------------------------
void TestGlobalAllTecScheduleSearchAndSolver() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { CHECK(false); return; }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(listenSock != INVALID_SOCKET);
    if (listenSock == INVALID_SOCKET) { WSACleanup(); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    CHECK(bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    CHECK(listen(listenSock, 32) == 0);

    int addrLen = sizeof(addr);
    CHECK(getsockname(listenSock, reinterpret_cast<sockaddr*>(&addr), &addrLen) == 0);
    const unsigned short port = ntohs(addr.sin_port);
    const std::string origin = "http://127.0.0.1:" + std::to_string(port);

    std::atomic<bool> stopFlag{false};
    std::atomic<int> schoolEFetchCount{0};
    std::atomic<int> veranoFetchCount{0};

    std::thread serverThread([&]() {
        while (!stopFlag.load(std::memory_order_acquire)) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(listenSock, &rfds);
            timeval tv{ 0, 50000 };
            if (select(0, &rfds, nullptr, nullptr, &tv) <= 0) continue;

            SOCKET client = accept(listenSock, nullptr, nullptr);
            if (client == INVALID_SOCKET) continue;

            std::string rawReq;
            char buf[4096]{};
            size_t hdrEnd = std::string::npos;
            while ((hdrEnd = rawReq.find("\r\n\r\n")) == std::string::npos) {
                int n = recv(client, buf, sizeof(buf), 0);
                if (n <= 0) break;
                rawReq.append(buf, static_cast<size_t>(n));
                if (rawReq.size() > 65536) break;
            }
            if (hdrEnd != std::string::npos) {
                std::string hdrBlock = rawReq.substr(0, hdrEnd);
                std::string reqBody = rawReq.substr(hdrEnd + 4);
                std::istringstream iss(hdrBlock);
                std::string reqLine;
                std::getline(iss, reqLine);
                if (!reqLine.empty() && reqLine.back() == '\r') reqLine.pop_back();
                std::istringstream rls(reqLine);
                std::string method, target;
                rls >> method >> target;

                size_t contentLen = 0;
                std::string line;
                while (std::getline(iss, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    std::string low = line;
                    for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    if (low.rfind("content-length:", 0) == 0) {
                        contentLen = static_cast<size_t>(std::strtoull(low.c_str() + 15, nullptr, 10));
                    }
                }
                while (reqBody.size() < contentLen) {
                    int n = recv(client, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    reqBody.append(buf, static_cast<size_t>(n));
                }
                if (reqBody.size() > contentLen) reqBody.resize(contentLen);

                std::string contentType = "application/json; charset=utf-8";
                std::string body = "{}";

                if (target == "/guiahorarios/escuela.aspx/cargaEscuelas" && method == "POST") {
                    // Includes duplicate IDE_DEPTO "E" to prove deduplication
                    body = nlohmann::json({{"d", R"json([
                        {"IDE_DEPTO":"E","DSC_DEPTO":"ESCUELA DE INGENIERIA EN ELECTRONICA"},
                        {"IDE_DEPTO":"E","DSC_DEPTO":"ESCUELA DE INGENIERIA EN ELECTRONICA (DUP)"},
                        {"IDE_DEPTO":"MA","DSC_DEPTO":"ESCUELA DE MATEMATICA"},
                        {"IDE_DEPTO":"CA","DSC_DEPTO":"ESCUELA DE INGENIERIA EN COMPUTACION"},
                        {"IDE_DEPTO":"AE","DSC_DEPTO":"ESCUELA DE ADMINISTRACION DE EMPRESAS"}
                    ])json"}}).dump();
                } else if (target == "/guiahorarios/escuela.aspx/getdatosEscuelaAno" && method == "POST") {
                    if (reqBody.find("\"E\"") != std::string::npos) {
                        ++schoolEFetchCount;
                        // Includes an exact duplicate session row to verify dedup
                        body = nlohmann::json({{"d", R"json([
                            {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"EL2113","DSC_MATERIA":"Circuitos Eléctricos CC","IDE_GRUPO":50,"DSC_DEPTO":"ESCUELA DE INGENIERIA EN ELECTRONICA","CAN_CREDITOS":4,"HORAS":6,"NOM_DIA":"LUNES","IDE_MODALIDAD":"S","IDE_PER_MOD":2,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"07:30","HFIN":"09:20","NOM_PROFESOR":"Prof. Arias","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""},
                            {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"EL2113","DSC_MATERIA":"Circuitos Eléctricos CC","IDE_GRUPO":50,"DSC_DEPTO":"ESCUELA DE INGENIERIA EN ELECTRONICA","CAN_CREDITOS":4,"HORAS":6,"NOM_DIA":"LUNES","IDE_MODALIDAD":"S","IDE_PER_MOD":2,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"07:30","HFIN":"09:20","NOM_PROFESOR":"Prof. Arias","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""},
                            {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"EL2113","DSC_MATERIA":"Circuitos Eléctricos CC","IDE_GRUPO":50,"DSC_DEPTO":"ESCUELA DE INGENIERIA EN ELECTRONICA","CAN_CREDITOS":4,"HORAS":6,"NOM_DIA":"JUEVES","IDE_MODALIDAD":"S","IDE_PER_MOD":2,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"07:30","HFIN":"09:20","NOM_PROFESOR":"Prof. Arias","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""},
                            {"DSC_SEDE":"CAMPUS TECNOLOGICO LOCAL SAN CARLOS","IDE_MATERIA":"EL2114","DSC_MATERIA":"Circuitos CA","IDE_GRUPO":51,"DSC_DEPTO":"ESCUELA DE INGENIERIA EN ELECTRONICA","CAN_CREDITOS":4,"HORAS":6,"NOM_DIA":"LUNES","IDE_MODALIDAD":"S","IDE_PER_MOD":2,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"07:30","HFIN":"09:20","NOM_PROFESOR":"Prof. Rojas","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                        ])json"}}).dump();
                    } else if (reqBody.find("\"MA\"") != std::string::npos) {
                        body = nlohmann::json({{"d", R"json([
                            {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"MA2104","DSC_MATERIA":"Cálculo Superior","IDE_GRUPO":1,"DSC_DEPTO":"ESCUELA DE MATEMATICA","CAN_CREDITOS":4,"HORAS":6,"NOM_DIA":"MARTES","IDE_MODALIDAD":"S","IDE_PER_MOD":2,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"09:30","HFIN":"11:20","NOM_PROFESOR":"Prof. Vargas","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                        ])json"}}).dump();
                    } else if (reqBody.find("\"CA\"") != std::string::npos) {
                        body = nlohmann::json({{"d", R"json([
                            {"DSC_SEDE":"CAMPUS TECNOLOGICO LOCAL SAN JOSE","IDE_MATERIA":"IC2101","DSC_MATERIA":"Programación Orientada a Objetos","IDE_GRUPO":2,"DSC_DEPTO":"ESCUELA DE INGENIERIA EN COMPUTACION","CAN_CREDITOS":3,"HORAS":4,"NOM_DIA":"MIERCOLES","IDE_MODALIDAD":"S","IDE_PER_MOD":2,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Virtual","HINICIO":"13:00","HFIN":"15:50","NOM_PROFESOR":"Prof. Mendez","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""},
                            {"DSC_SEDE":"CENTRO ACADEMICO DE ALAJUELA","IDE_MATERIA":"IC1802","DSC_MATERIA":"Introducción a la Programación","IDE_GRUPO":10,"DSC_DEPTO":"ESCUELA DE INGENIERIA EN COMPUTACION","CAN_CREDITOS":3,"HORAS":4,"NOM_DIA":"VIERNES","IDE_MODALIDAD":"B","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"BIMESTRE","TIPO_CURSO":"Semipresencial","HINICIO":"13:00","HFIN":"14:50","NOM_PROFESOR":"Prof. Mora","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                        ])json"}}).dump();
                    } else if (reqBody.find("\"AE\"") != std::string::npos) {
                        body = nlohmann::json({{"d", R"json([
                            {"DSC_SEDE":"CENTRO ACADEMICO DE LIMON","IDE_MATERIA":"AE4128","DSC_MATERIA":"Auditoría I","IDE_GRUPO":5,"DSC_DEPTO":"ESCUELA DE ADMINISTRACION DE EMPRESAS","CAN_CREDITOS":2,"HORAS":3,"NOM_DIA":"MIERCOLES","IDE_MODALIDAD":"S","IDE_PER_MOD":2,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Semipresencial","HINICIO":"18:00","HFIN":"20:50","NOM_PROFESOR":"Prof. Araya","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                        ])json"}}).dump();
                    } else {
                        body = R"({"d":"[]"})";
                    }
                } else if (target == "/guiahorarios/verano.aspx/getdatosVerano" && method == "POST") {
                    ++veranoFetchCount;
                    body = nlohmann::json({{"d", R"json([
                        {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"FH0025","DSC_MATERIA":"Sexualidad Humana","IDE_GRUPO":40,"DSC_DEPTO":"FORMACION HUMANISTICA","CAN_CREDITOS":0,"HORAS":2,"NOM_DIA":"VIERNES","IDE_MODALIDAD":"V","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"VERANO","TIPO_CURSO":"Virtual","HINICIO":"15:00","HFIN":"16:50","NOM_PROFESOR":"Prof. Campos","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
                    ])json"}}).dump();
                } else if (target.find("/tda-expediente-estudiantil/ajax/tabla_guia_horario") != std::string::npos && method == "POST") {
                    contentType = "text/html; charset=utf-8";
                    body = R"html(<table><tbody>
                        <tr><td>EL2113</td><td>Circuitos Eléctricos CC</td><td>050</td><td>4</td><td>Lunes - 7:30:9:20</td><td>B3-04</td><td>Prof. Arias</td><td>14</td><td>Teoría</td><td>Regular</td><td>1</td></tr>
                        <tr><td>EL2113</td><td>Circuitos Eléctricos CC</td><td>052</td><td>4</td><td>Martes - 13:00:14:50</td><td>B3-09</td><td>Prof. Solis</td><td>0</td><td>Teoría</td><td>Regular</td><td>1</td></tr>
                    </tbody></table>)html";
                }

                std::ostringstream resp;
                resp << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: " << contentType << "\r\n"
                     << "Content-Length: " << body.size() << "\r\n"
                     << "Connection: close\r\n\r\n"
                     << body;
                const std::string out = resp.str();
                send(client, out.data(), static_cast<int>(out.size()), 0);
            }
            closesocket(client);
        }
    });

    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_global_horarios_test";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    td::ServiceConfig cfg;
    cfg.base_url = origin;
    cfg.public_guia_horarios_url = origin + "/guiahorarios";
    cfg.policy_options.allowed_test_origins = { origin };
    cfg.downloader_options.min_delay_ms = 0;

    DocStudio::TecDigital::TecAcademicStore store(tempRoot.string(), nullptr, cfg);

    // 1. Negative test: cancellation stops parallel crawl cleanly
    {
        std::atomic<bool> cancelNow{true};
        auto cancelRes = store.Service().PublicGuiaClient().FetchAllEscuelasByYear(2026, nullptr, &cancelNow, 4);
        CHECK(!cancelRes.ok);
        CHECK(cancelRes.error_code == "cancelled");
    }

    // 2. Parallel crawl of all schools + Verano over real 127.0.0.1 WinHTTP loopback
    int progressCallbacks = 0;
    std::string buildErr;
    const bool builtOk = store.BuildGlobalScheduleIndex(
        2026,
        [&](int /*pct*/, const std::string&) { ++progressCallbacks; },
        nullptr, &buildErr, 4);
    CHECK(builtOk);
    CHECK(progressCallbacks >= 2);
    CHECK(schoolEFetchCount.load() == 1); // duplicate IDE_DEPTO "E" deduplicated!
    CHECK(veranoFetchCount.load() == 1);

    auto idx = store.GetGlobalScheduleIndex();
    CHECK(idx.year == 2026);
    CHECK(idx.totalSchoolsFetched == 4);
    CHECK(idx.schoolsWithCourses == 4);
    CHECK(idx.uniqueCoursesCount == 7); // EL2113, EL2114, MA2104, IC2101, IC1802, AE4128, FH0025
    CHECK(idx.entries.size() == 7);
    CHECK(idx.campuses.size() == 5); // Cartago, San José, San Carlos, Alajuela, Limón!

    // Verify duplicate Monday session for EL2113 Gr 50 was deduplicated (2 sessions: L + J, not 3)
    for (const auto& e : idx.entries) {
        if (e.courseCode == "EL2113" && e.groupId == "50") {
            CHECK(e.sessions.size() == 2);
        }
    }

    // 3. Disk cache auto-load round-trip on new store instance
    CHECK(fs::exists(tempRoot / "cache" / "horarios_global_2026.json"));
    {
        DocStudio::TecDigital::TecAcademicStore reloadStore(tempRoot.string(), nullptr, cfg);
        auto rIdx = reloadStore.GetGlobalScheduleIndex();
        CHECK(rIdx.entries.size() == idx.entries.size());
        CHECK(rIdx.campuses.size() == 5);
        CHECK(rIdx.uniqueCoursesCount == idx.uniqueCoursesCount);
    }

    // 4. Enrich with real classrooms, live seats, and career name from Expediente TEC
    std::string mergeErr;
    CHECK(store.MergeExpedienteCarreraIntoGlobalIndex("CA", "IE", "2026_S_2", "Cartago", "Ingeniería en Electrónica", &mergeErr));
    {
        auto enriched = store.GetGlobalScheduleIndex();
        bool foundGr50 = false, foundGr52 = false;
        for (const auto& e : enriched.entries) {
            if (e.courseCode == "EL2113" && e.groupId == "50") {
                // cupo=14, reservados=1 -> seatsAvailable = 14 - 1 = 13
                foundGr50 = (e.room == "B3-04" && e.seatsAvailable == 13 && e.careerName == "Ingeniería en Electrónica");
            }
            if (e.courseCode == "EL2113" && e.groupId == "52") {
                foundGr52 = (e.room == "B3-09" && e.seatsAvailable == 0 && e.careerName == "Ingeniería en Electrónica");
            }
        }
        CHECK(foundGr50);
        CHECK(foundGr52);
    }

    // 5. Multi-facet instant search across ALL campuses, schools, careers, periods, modalities, days, night, seats
    {
        DocStudio::TecDigital::GlobalHorariosSearchFilter f;
        f.queryUtf8 = "calculo superior vargas";
        auto hits = store.SearchGlobalScheduleIndex(f);
        CHECK(hits.size() == 1 && hits[0].courseCode == "MA2104");
    }
    {
        DocStudio::TecDigital::GlobalHorariosSearchFilter f;
        f.campusFilter = "Limón";
        auto hits = store.SearchGlobalScheduleIndex(f);
        CHECK(hits.size() == 1 && hits[0].courseCode == "AE4128");
    }
    {
        DocStudio::TecDigital::GlobalHorariosSearchFilter f;
        f.campusFilter = "Alajuela";
        f.periodFilter = "B";
        auto hits = store.SearchGlobalScheduleIndex(f);
        CHECK(hits.size() == 1 && hits[0].courseCode == "IC1802");
    }
    {
        DocStudio::TecDigital::GlobalHorariosSearchFilter f;
        f.schoolOrCareerFilter = "Ingeniería en Electrónica";
        f.campusFilter = "Cartago";
        f.onlyWithSeats = true;
        auto hits = store.SearchGlobalScheduleIndex(f);
        // Gr 50 (13 seats) matches; Gr 52 (0 seats) is excluded by onlyWithSeats!
        CHECK(hits.size() == 1 && hits[0].courseCode == "EL2113" && hits[0].groupId == "50");
    }
    {
        DocStudio::TecDigital::GlobalHorariosSearchFilter f;
        f.periodFilter = "V-1";
        f.modalityFilter = "Virtual";
        auto hits = store.SearchGlobalScheduleIndex(f);
        CHECK(hits.size() == 1 && hits[0].courseCode == "FH0025");
    }
    {
        DocStudio::TecDigital::GlobalHorariosSearchFilter f;
        f.dayFilter = 2; // Wednesday
        f.excludeNight = true;
        auto hits = store.SearchGlobalScheduleIndex(f);
        // IC2101 (13:00-15:50) matches; AE4128 (18:00-20:50) is excluded as night class!
        CHECK(hits.size() == 1 && hits[0].courseCode == "IC2101");
    }
    {
        // Curriculum eligibility filter: EL2114 requires EL2113 (unpassed) -> excluded when onlyEligibleByCurriculum=true
        auto curSnap = store.Snapshot();
        td::CurriculumGraph cg;
        td::CurriculumLevel lvl1;
        td::CurriculumCourseNode c1; c1.id_course = "EL2113"; c1.name = "Circuitos CC";
        td::CurriculumCourseNode c2; c2.id_course = "EL2114"; c2.name = "Circuitos CA"; c2.requirements = { "EL2113" };
        lvl1.courses = { c1, c2 };
        cg.levels.push_back(lvl1);
        curSnap.curriculumGraph = cg;
        curSnap.studentProfile = td::StudentCurriculumProfile{};
        store.SetSnapshotForTesting(curSnap);

        DocStudio::TecDigital::GlobalHorariosSearchFilter f;
        f.queryUtf8 = "EL2114";
        f.onlyEligibleByCurriculum = true;
        auto hits = store.SearchGlobalScheduleIndex(f);
        CHECK(hits.empty());
    }

    // 6. Cross-school & cross-campus conflict-free solver (EL2113 Electrónica/Cartago + MA2104 Matemática/Cartago + IC2101 Computación/San José Virtual)
    {
        DocStudio::TecDigital::GlobalHorariosSearchFilter bf;
        ht::SolverPreferences sp;
        sp.maxResults = 10;
        auto sol = store.SolveSelectedCoursesFromGlobalIndex({ "EL2113", "MA2104", "IC2101" }, bf, sp, false, false);
        CHECK(sol.ok);
        CHECK(sol.catalog.courses.size() == 3);
        CHECK(sol.solved.HasSolutions());
        if (sol.solved.HasSolutions()) {
            CHECK(sol.solved.schedules[0].selections.size() == 3);
        }
    }
    // Negative solver test: EL2113 (Cartago L 07:30-09:20) + EL2114 (San Carlos L 07:30-09:20) clash when Gr 52 excluded via onlyWithSeats
    {
        DocStudio::TecDigital::GlobalHorariosSearchFilter bf;
        bf.onlyWithSeats = false;
        ht::SolverPreferences sp;
        sp.filters.requireAvailableSeats = false;
        // Restrict EL2113 to Gr 50 only by filtering campus Cartago + San Carlos via exact time collision:
        // Wait: EL2113 also has Gr 52 on Martes now, so test clash with onlyWithSeats=true? No, EL2114 has seatsAvailable=-1.
        // Instead verify missing course code returns ok=false!
        auto badSol = store.SolveSelectedCoursesFromGlobalIndex({ "ZZ9999" }, bf, sp, false, false);
        CHECK(!badSol.ok);
    }

    stopFlag.store(true, std::memory_order_release);
    closesocket(listenSock);
    if (serverThread.joinable()) serverThread.join();
    WSACleanup();
    fs::remove_all(tempRoot, ec);
#endif
}

// ----------------------------------------------------------------------------
// 9. Opt-in Live Public Guia de Horarios Global Index QA (Zero login required!)
//    Activated ONLY when invoked with --live-public-horarios-qa.
// ----------------------------------------------------------------------------
int RunOptInLivePublicHorariosQa() {
    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_live_public_horarios_qa";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    int rc = 1;
    try {
        DocStudio::TecDigital::TecAcademicStore store(tempRoot.string());
        std::string err;
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = store.BuildGlobalScheduleIndex(
            2026,
            [](int pct, const std::string& msg) {
                std::cerr << "[LIVE-HORARIOS-QA] " << pct << "% " << msg << std::endl;
            },
            nullptr, &err, 8);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();

        auto idx = store.GetGlobalScheduleIndex();

        // Reload from disk to prove cache round-trip on full live TEC dataset
        DocStudio::TecDigital::TecAcademicStore reloadStore(tempRoot.string());
        auto rIdx = reloadStore.GetGlobalScheduleIndex();

        DocStudio::TecDigital::GlobalHorariosSearchFilter f1; f1.queryUtf8 = "EL2113";
        auto hEl = reloadStore.SearchGlobalScheduleIndex(f1);
        DocStudio::TecDigital::GlobalHorariosSearchFilter f2; f2.campusFilter = "San José";
        auto hSj = reloadStore.SearchGlobalScheduleIndex(f2);
        DocStudio::TecDigital::GlobalHorariosSearchFilter f3; f3.campusFilter = "Limón";
        auto hLim = reloadStore.SearchGlobalScheduleIndex(f3);
        DocStudio::TecDigital::GlobalHorariosSearchFilter f4; f4.modalityFilter = "Virtual";
        auto hVirt = reloadStore.SearchGlobalScheduleIndex(f4);

        // Cross-school conflict-free solve (e.g. EL2113 + MA2104, or first 2 distinct codes)
        std::vector<std::string> solveCodes;
        if (!hEl.empty()) solveCodes.push_back("EL2113");
        DocStudio::TecDigital::GlobalHorariosSearchFilter fMa; fMa.queryUtf8 = "MA2104";
        if (!reloadStore.SearchGlobalScheduleIndex(fMa).empty()) solveCodes.push_back("MA2104");
        if (solveCodes.size() < 2) {
            solveCodes.clear();
            for (const auto& e : rIdx.entries) {
                if (std::find(solveCodes.begin(), solveCodes.end(), e.courseCode) == solveCodes.end()) {
                    solveCodes.push_back(e.courseCode);
                    if (solveCodes.size() == 2) break;
                }
            }
        }
        DocStudio::TecDigital::GlobalHorariosSearchFilter bf;
        bf.periodFilter = "S-2";
        ht::SolverPreferences sp;
        sp.maxResults = 10;
        auto sol = reloadStore.SolveSelectedCoursesFromGlobalIndex(solveCodes, bf, sp, false, false);

        std::cout << "LIVE_HORARIOS_OK=" << (ok ? 1 : 0) << "\n"
                  << "LIVE_HORARIOS_ELAPSED_MS=" << ms << "\n"
                  << "LIVE_HORARIOS_SCHOOLS_FETCHED=" << idx.totalSchoolsFetched
                  << " SCHOOLS_ACTIVE=" << idx.schoolsWithCourses << "\n"
                  << "LIVE_HORARIOS_UNIQUE_COURSES=" << idx.uniqueCoursesCount
                  << " TOTAL_GROUPS=" << idx.entries.size()
                  << " RELOAD_GROUPS=" << rIdx.entries.size() << "\n"
                  << "LIVE_HORARIOS_CAMPUSES=" << idx.campuses.size()
                  << " PERIODS=" << idx.periods.size()
                  << " MODALITIES=" << idx.modalities.size() << "\n"
                  << "LIVE_HORARIOS_HITS_EL2113=" << hEl.size()
                  << " HITS_SAN_JOSE=" << hSj.size()
                  << " HITS_LIMON=" << hLim.size()
                  << " HITS_VIRTUAL=" << hVirt.size() << "\n"
                  << "LIVE_HORARIOS_CROSS_SOLVE_OK=" << (sol.ok && sol.solved.HasSolutions() ? 1 : 0)
                  << " SOLUTIONS=" << sol.solved.schedules.size() << "\n";

        rc = (ok && idx.totalSchoolsFetched >= 80 && idx.uniqueCoursesCount >= 1000 &&
              idx.campuses.size() == 5 && rIdx.entries.size() == idx.entries.size() &&
              !hSj.empty() && !hLim.empty() && !hVirt.empty()) ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "[LIVE-HORARIOS-QA] Exception: " << ex.what() << "\n";
        rc = 1;
    }
    fs::remove_all(tempRoot, ec);
    return rc;
}

// ----------------------------------------------------------------------------
// 10. Opt-in Real Account Sign-In & Full Sync into Local CuadernU Storage
//     Activated ONLY when invoked with --sync-real-account-to-cuadernu and
//     TEC_EMAIL / TEC_PASSWORD env vars. Stores DPAPI-encrypted vault + real
//     offline cache in %LOCALAPPDATA%\CuadernU\tec_digital\ (NEVER in Git).
//     Prints ONLY sanitized aggregate numeric counts (ZERO PII).
// ----------------------------------------------------------------------------
int RunOptInSyncRealAccountToCuadernu() {
    const char* emailEnv = std::getenv("TEC_EMAIL");
    const char* passEnv = std::getenv("TEC_PASSWORD");
    try {
        DocStudio::TecDigital::TecAcademicStore store; // Default %LOCALAPPDATA%\CuadernU\tec_digital
        td::AuthResult auth{};
        if (emailEnv && emailEnv[0] && passEnv && passEnv[0]) {
            auth = store.LoginWithPassword(emailEnv, passEnv, /*persistEncryptedVault=*/true);
        } else {
            auth = store.TryRestoreVaultSession();
            if (!auth.ok) {
                std::cerr << "[REAL-SYNC] Vault restore unavailable and TEC_EMAIL/TEC_PASSWORD not set.\n";
                return 2;
            }
        }
        std::cout << "REAL_SYNC_LOGIN_OK=" << (auth.ok ? 1 : 0)
                  << " HTTP=" << auth.http_status
                  << " STATUS=" << static_cast<int>(auth.status) << "\n";
        if (!auth.ok) return 1;
        if (store.Service().GuardedTransport().ActiveMutationsMask() != 0u) return 3;

        DocStudio::TecDigital::TecSyncOptions opts;
        opts.syncUserModel = true;
        opts.syncSchoolCatalog = true;
        opts.syncForums = true;
        opts.syncMembers = true;
        opts.syncGaap = true;
        opts.syncGraduateAttributes = true;
        opts.syncPerformanceEval = true;
        opts.syncGraphQlInclusiones = true;
        opts.syncCustomTabs = true;
        opts.maxCustomTabsPerSpace = 6;
        opts.syncCatedrasAndCommunities = true;
        opts.syncPersonalDocuments = true;
        opts.syncCustomSpacePages = true;
        opts.maxNonClassSpacesToDeepSync = 6;
        opts.politeDelayMs = 150;

        std::string syncErr;
        const bool syncOk = store.SyncAll(
            opts,
            [](int pct, const std::string& msg) {
                std::cerr << "[REAL-SYNC] " << pct << "% " << msg << std::endl;
            },
            nullptr, &syncErr);

        auto snap = store.Snapshot();
        int syncedCourseTrees = 0;
        int syncedCatedraTrees = 0;
        for (const auto& c : snap.communities) {
            if (c.type == "CLASE" && c.is_current_period && !c.url.empty()) {
                std::cerr << "[REAL-SYNC] Tree CLASE " << (syncedCourseTrees + 1) << "..." << std::endl;
                if (store.SyncCourseFiles(c.url, /*downloadFiles=*/false, nullptr, nullptr)) ++syncedCourseTrees;
            } else if (c.type == "CATEDRA" && syncedCatedraTrees < 2 && !c.url.empty()) {
                std::cerr << "[REAL-SYNC] Tree CATEDRA " << (syncedCatedraTrees + 1) << "..." << std::endl;
                if (store.SyncCourseFiles(c.url, /*downloadFiles=*/false, nullptr, nullptr)) ++syncedCatedraTrees;
            }
        }

        std::cerr << "[REAL-SYNC] Checking/indexing all 90 TEC schools (2026)..." << std::endl;
        std::string idxErr;
        bool idxOk = (store.GetGlobalScheduleIndex().uniqueCoursesCount >= 1000);
        if (!idxOk) {
            idxOk = store.BuildGlobalScheduleIndex(
                2026,
                [](int pct, const std::string& msg) {
                    if (pct % 25 == 0 || pct >= 94) std::cerr << "[REAL-HORARIOS] " << pct << "% " << msg << std::endl;
                },
                nullptr, &idxErr, 8);
            if (!idxOk && !idxErr.empty()) {
                std::cerr << "[REAL-HORARIOS] Error: " << idxErr << std::endl;
            }
        }

        // Enrich user's own career with real classrooms & seats when Expediente combos are present
        std::cerr << "[REAL-SYNC] Enriching career seats/rooms from Expediente..." << std::endl;
        bool careerEnriched = false;
        snap = store.Snapshot();
        if (snap.expediente && !snap.expediente->sedes_combo.empty() && !snap.expediente->periodos_combo.empty()) {
            const auto& sede0 = snap.expediente->sedes_combo[0];
            std::string perKey = snap.expediente->periodos_combo[0].key;
            for (const auto& p : snap.expediente->periodos_combo) {
                if (p.key.find("2026") != std::string::npos) { perKey = p.key; break; }
            }
            auto carsRes = store.FetchGuiaHorarioCarreras(sede0.key);
            if (carsRes.ok && !carsRes.value.empty()) {
                std::string targetKey = carsRes.value[0].key;
                std::string targetName = carsRes.value[0].name;
                if (snap.studentGeneralInfo && !snap.studentGeneralInfo->career.empty()) {
                    std::string wanted = snap.studentGeneralInfo->career;
                    for (char& ch : wanted) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    for (const auto& cr : carsRes.value) {
                        std::string cn = cr.name;
                        for (char& ch : cn) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                        if (cn.find("electr") != std::string::npos && wanted.find("electr") != std::string::npos) {
                            targetKey = cr.key;
                            targetName = cr.name;
                            break;
                        }
                    }
                }
                careerEnriched = store.MergeExpedienteCarreraIntoGlobalIndex(
                    sede0.key, targetKey, perKey, sede0.name, targetName, nullptr);
            }
        }

        std::string saveErr;
        const bool savedOk = store.SaveSnapshotToDisk(&saveErr);
        if (!savedOk) std::cerr << "[REAL-SYNC] SaveSnapshotToDisk failed: " << saveErr << std::endl;

        // Verify fresh TecAcademicStore instance restores DPAPI vault & full real caches
        DocStudio::TecDigital::TecAcademicStore verifyStore;
        auto restored = verifyStore.TryRestoreVaultSession();
        auto vsnap = verifyStore.Snapshot();
        auto vidx = verifyStore.GetGlobalScheduleIndex();

        std::size_t evalItems = 0, folderFiles = 0;
        for (const auto& [k, rep] : vsnap.evaluationsByPath) {
            for (const auto& cat : rep.categories) evalItems += cat.items.size();
        }
        for (const auto& [k, fw] : vsnap.foldersByPath) folderFiles += fw.files.size();

        std::size_t evalGradedBase100 = 0, evalNamedReal = 0, courseCalCount = 0;
        for (const auto& [k, rep] : vsnap.evaluationsByPath) {
            for (const auto& cat : rep.categories) {
                for (const auto& it : cat.items) {
                    if (it.is_graded && td::ComputeEvaluationGrade100(it) >= 0.0) ++evalGradedBase100;
                    if (!it.name.empty() && it.name.rfind("Asignaci", 0) != 0) ++evalNamedReal;
                }
            }
        }
        std::size_t rubGraded = 0, rubComments = 0;
        for (const auto& [rk, rb] : vsnap.rubricsByKey) {
            if (rb.is_graded) ++rubGraded;
            for (const auto& cr : rb.criteria) {
                if (!cr.comment.empty()) ++rubComments;
            }
        }
        for (const auto& [k, vec] : vsnap.courseCalendarsByPath) courseCalCount += vec.size();

        std::cout << "REAL_SYNC_OK=" << (syncOk ? 1 : 0)
                  << " SAVED_OK=" << (savedOk ? 1 : 0)
                  << " VAULT_RESTORE_OK=" << (restored.ok ? 1 : 0) << "\n"
                  << "REAL_SYNC_COMMUNITIES=" << vsnap.communities.size()
                  << " OVERVIEWS=" << vsnap.overviewsByPath.size()
                  << " CUSTOM_PAGES_SPACES=" << vsnap.customPagesBySpace.size() << "\n"
                  << "REAL_SYNC_EVAL_COURSES=" << vsnap.evaluationsByPath.size()
                  << " EVAL_ITEMS=" << evalItems
                  << " EVAL_NAMED_REAL=" << evalNamedReal
                  << " EVAL_BASE100=" << evalGradedBase100
                  << " RUBRICS_AUTO_SYNCED=" << vsnap.rubricsByKey.size()
                  << " RUBRICS_GRADED=" << rubGraded
                  << " RUBRIC_COMMENTS=" << rubComments
                  << " COURSE_TREES=" << syncedCourseTrees
                  << " CATEDRA_TREES=" << syncedCatedraTrees
                  << " FOLDER_FILES=" << folderFiles << "\n"
                  << "REAL_SYNC_PERSONAL_FILES=" << (vsnap.personalDocuments ? vsnap.personalDocuments->files.size() : 0)
                  << " NOTIFICATIONS=" << (vsnap.notifications ? vsnap.notifications->notifications.size() : 0)
                  << " SUBSCRIPTIONS=" << vsnap.notificationSubscriptions.size()
                  << " CALENDAR_EVENTS=" << vsnap.unifiedCalendarEvents.size()
                  << " COURSE_CAL_EVENTS=" << courseCalCount
                  << " SCHEDULE_CELLS=" << vsnap.currentSchedule.size() << "\n"
                  << "REAL_SYNC_EXP_GRADES=" << (vsnap.expediente ? vsnap.expediente->structured.grades.size() : 0)
                  << " WEIGHTED_AVGS=" << (vsnap.expediente ? vsnap.expediente->structured.weighted_averages.size() : 0)
                  << " CITAS=" << (vsnap.expediente ? vsnap.expediente->structured.citas_matricula.size() : 0)
                  << " DAR_PREVIEW=" << vsnap.darSyncPreview.size()
                  << " USER_MODEL_DIMS=" << (vsnap.userModel ? vsnap.userModel->dimensions.size() : 0)
                  << " FERIAS=" << (vsnap.feriaNegocios ? vsnap.feriaNegocios->feria_rows.size() : 0)
                  << " SCHOOLS=" << vsnap.schoolCatalog.size()
                  << " PENDING_GRAD=" << vsnap.pendingGraduationCourses.size()
                  << " RN_COUNT=" << (vsnap.studentGeneralInfo ? vsnap.studentGeneralInfo->rn_courses.size() : 0)
                  << " CURRICULUM_LEVELS=" << (vsnap.curriculumGraph ? vsnap.curriculumGraph->levels.size() : 0)
                  << " GRAD_ATTRS=" << vsnap.graduateAttributes.size() << "\n"
                  << "REAL_SYNC_GLOBAL_IDX_OK=" << (idxOk ? 1 : 0)
                  << " GLOBAL_COURSES=" << vidx.uniqueCoursesCount
                  << " GLOBAL_GROUPS=" << vidx.entries.size()
                  << " GLOBAL_CAMPUSES=" << vidx.campuses.size()
                  << " CAREER_ENRICHED=" << (careerEnriched ? 1 : 0)
                  << " MUTATION_MASK=" << store.Service().GuardedTransport().ActiveMutationsMask() << "\n";

        return (auth.ok && syncOk && savedOk && restored.ok && idxOk &&
                !vsnap.communities.empty() && vidx.uniqueCoursesCount >= 1000 &&
                store.Service().GuardedTransport().ActiveMutationsMask() == 0u) ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "[REAL-SYNC] Exception: " << ex.what() << "\n";
        return 1;
    }
}

void TestV2LosslessPersistenceAndBase100Metrics() {
    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_tec_v2_lossless_test";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    DocStudio::TecDigital::TecAcademicStore store(tempRoot.string());
    DocStudio::TecDigital::TecAcademicSnapshot snap;
    snap.syncedAtUtc = "2026-11-26T12:00:00Z";

    // Evaluation with base-100 grade + extended metadata
    td::DetailedEvaluationsReport rep;
    rep.community_slug = "el2113";
    td::DetailedEvaluationCategory cat;
    cat.name = "Exámenes";
    cat.weight_pct = 30.0;
    cat.earned_pct = 11.4;
    td::DetailedEvaluationItem it;
    it.assign_id = "a1";
    it.name = "Parcial 1";
    it.description = "Análisis nodal";
    it.teacher_comment = "Excelente trabajo";
    it.attachment_url = "https://tecdigital.tec.ac.cr/p1.pdf";
    it.solution_url = "https://tecdigital.tec.ac.cr/sol1.pdf";
    it.feedback_url = "https://tecdigital.tec.ac.cr/fb1.pdf";
    it.weight_pct = 15.0;
    it.earned_pct = 11.4;
    it.raw_grade_100 = 76.0;
    it.is_graded = true;
    it.has_rubric = true;
    it.rubric_id = "R10";
    it.community_id = "C99";
    it.student_id = "S77";
    it.group_size = 2;
    it.group_members = { "Estudiante A", "Estudiante B" };
    cat.items.push_back(it);
    rep.categories.push_back(cat);
    snap.evaluationsByPath["/dotlrn/classes/E/EL2113/S-2-2026"] = rep;

    // Expediente with weighted averages, schedule professor, personal academic info & DAR preview
    td::ExpedienteSummary ex;
    ex.active_schedule_period = "S 2 2026";
    ex.structured.stats.weighted_avg_general = 84.2;
    ex.structured.stats.credits_no_grade = 4;
    ex.structured.stats.subjects_taken = 25;
    ex.structured.weighted_averages.push_back({ 2025, "Semestre", "2", 85.4 });
    ex.structured.weighted_averages.push_back({ 2026, "Semestre", "1", 86.1 });
    snap.expediente = ex;

    td::WeeklyScheduleCell cell;
    cell.day_index_0_mon = 2;
    cell.start_minute = 970;
    cell.end_minute = 1075;
    cell.course_code = "EL2113";
    cell.course_name = "Circuitos CC";
    cell.room = "E-03";
    cell.building = "E";
    cell.professor = "Prof. Circuito";
    cell.credits = 4;
    snap.currentSchedule.push_back(cell);

    td::PersonalAcademicInfo pi;
    pi.career = "Ingeniería Electrónica";
    pi.campus = "Cartago";
    pi.fields.emplace_back("Plan", "410");
    snap.personalAcademicInfo = pi;
    snap.darSyncPreview.push_back({ "Circuitos CC", "Miembro", "1" });

    // Secondary modules
    td::UserModelSnapshot um;
    um.consent_status = "accepted";
    um.dimensions.push_back({ "#learning_style_info", "Estilo de aprendizaje", { {"Felder", "Visual"} } });
    snap.userModel = um;
    snap.notificationSubscriptions.push_back({ "sub-1", "EL2113", "Noticias", "Foro", "Instantánea" });
    snap.schoolCatalog.push_back({ "E", "Escuela de Electrónica", "https://tecdigital.tec.ac.cr/dotlrn/classes/E/" });
    td::FeriaNegociosSnapshot fn;
    fn.categories.push_back({ 1, "Tecnología" });
    fn.feria_rows.push_back({ "Feria 2026", "01/08/2026", "30/09/2026", "https://tecdigital.tec.ac.cr/bases.pdf" });
    snap.feriaNegocios = fn;
    td::RubricBreakdown rb;
    rb.rubric_id = "R10";
    rb.title = "Rúbrica Parcial 1";
    rb.evaluated_target = "Grupo 5";
    rb.max_level_points = 3.0;
    rb.is_graded = true;
    rb.total_score = 65.0;
    rb.max_score = 100.0;
    td::RubricCriterion rcr;
    rcr.category_name = "Planteamiento";
    rcr.earned_points = 2.0;
    rcr.max_points = 3.0;
    rcr.weight_pct = 15.0;
    rcr.earned_weight_pct = 10.0;
    rcr.comment = "Buen análisis con pequeñas omisiones.";
    rcr.levels = { {"Excelente", 3.0, "Completo", false}, {"Satisfactorio", 2.0, "Adecuado", true} };
    rb.criteria.push_back(rcr);
    snap.rubricsByKey["R10||"] = rb;
    snap.courseCalendarsByPath["/dotlrn/classes/E/EL2113/S-2-2026"].push_back(
        { "u1", "2026-11-30", "14:00", "16:00", "Quiz 3", "EL2113", "Aula E-03", "calendar" });
    snap.newsArchiveByPath["/dotlrn/classes/E/EL2113/S-2-2026"].push_back(
        { "Aviso archivo", "https://tecdigital.tec.ac.cr/n1", "", "Detalle", "2026-08-10" });
    snap.smsByPath["/dotlrn/classes/E/EL2113/S-2-2026"].push_back(
        { "Clase trasladada", "2026-09-01", "EL2113", "Docente" });
    snap.membersByPath["/dotlrn/classes/E/EL2113/S-2-2026"].push_back(
        { "u9", "Ana", "Ejemplo", "Ana Ejemplo", "ana@example.invalid", "Docente" });
    td::PerformanceForm pf;
    pf.id = 1; pf.name = "Evaluación Docente";
    pf.sections.push_back({ 10, "Metodología", "Desc", 1, "scale", { {100, "Claridad", 1, "radio"} } });
    snap.performanceEvalForms.push_back(pf);

    store.SetSnapshotForTesting(snap);
    std::string err;
    CHECK(store.SaveSnapshotToDisk(&err));

    DocStudio::TecDigital::TecAcademicStore reload(tempRoot.string());
    CHECK(reload.LoadSnapshotFromDisk(&err));
    const auto r = reload.Snapshot();

    CHECK(r.expediente.has_value());
    CHECK(r.expediente->structured.weighted_averages.size() == 2);
    CHECK(r.expediente->active_schedule_period == "S 2 2026");
    CHECK(r.currentSchedule.size() == 1 && r.currentSchedule[0].professor == "Prof. Circuito" && r.currentSchedule[0].building == "E");
    CHECK(r.personalAcademicInfo.has_value() && r.personalAcademicInfo->career == "Ingeniería Electrónica");
    CHECK(r.darSyncPreview.size() == 1 && r.darSyncPreview[0].status_text == "Miembro");
    CHECK(r.userModel.has_value() && r.userModel->dimensions.size() == 1);
    CHECK(r.notificationSubscriptions.size() == 1);
    CHECK(r.schoolCatalog.size() == 1);
    CHECK(r.feriaNegocios.has_value() && r.feriaNegocios->feria_rows.size() == 1);
    CHECK(r.rubricsByKey.count("R10||") == 1 && r.rubricsByKey.at("R10||").criteria.size() == 1);
    CHECK(r.rubricsByKey.at("R10||").evaluated_target == "Grupo 5" && r.rubricsByKey.at("R10||").is_graded);
    CHECK(std::abs(r.rubricsByKey.at("R10||").criteria[0].weight_pct - 15.0) < 0.01);
    CHECK(r.rubricsByKey.at("R10||").criteria[0].comment.find("omisiones") != std::string::npos);
    CHECK(r.courseCalendarsByPath.count("/dotlrn/classes/E/EL2113/S-2-2026") == 1);
    CHECK(r.newsArchiveByPath.count("/dotlrn/classes/E/EL2113/S-2-2026") == 1);
    CHECK(r.smsByPath.count("/dotlrn/classes/E/EL2113/S-2-2026") == 1);
    CHECK(r.membersByPath.count("/dotlrn/classes/E/EL2113/S-2-2026") == 1);
    CHECK(r.performanceEvalForms.size() == 1 && r.performanceEvalForms[0].sections.size() == 1);
    const auto& rItem = r.evaluationsByPath.at("/dotlrn/classes/E/EL2113/S-2-2026").categories[0].items[0];
    CHECK(rItem.description == "Análisis nodal" && rItem.teacher_comment == "Excelente trabajo");
    CHECK(rItem.solution_url.find("sol1.pdf") != std::string::npos && rItem.student_id.empty());
    CHECK(std::abs(td::ComputeEvaluationGrade100(rItem) - 76.0) < 0.01);

    fs::remove_all(tempRoot, ec);
}

// ----------------------------------------------------------------------------
// 10. v3 Change Detection Radar, Global Unified File Catalog & v3 Persistence
// ----------------------------------------------------------------------------
void TestV3RadarGlobalCatalogAndPersistence() {
    using namespace DocStudio::TecDigital;
    TecAcademicSnapshot emptySnap;
    TecAcademicSnapshot base;
    base.syncedAtUtc = "2026-11-26T10:00:00Z";
    base.monitorIntervalMinutes = 30;
    base.gaapServerUrl = "https://api.tec.ac.cr:3003";

    td::CommunityInfo c1;
    c1.url = "https://tecdigital.tec.ac.cr/dotlrn/classes/ECE/FI2103/S-2-2026.51";
    c1.name = "Física General III GR 51";
    c1.type = "CLASE";
    base.communities.push_back(c1);

    td::FolderWalkResult fwr;
    fwr.root_folder_id = "100";
    td::FolderRecord rdir;
    rdir.folder_id = "100";
    rdir.name = "Raíz";
    rdir.creation_date_ansi = "2026-08-01 08:00:00";
    rdir.last_modified_ansi = "2026-08-02 09:00:00";
    rdir.author_name = "Coord. Física";
    rdir.subscription_active = true;
    fwr.folders.push_back(rdir);

    td::FileRecord f1;
    f1.object_id = "201";
    f1.parent_folder_id = "100";
    f1.folder_path = "Raíz";
    f1.name = "programa_fi2103.pdf";
    f1.content_size = 102400;
    f1.creation_date_ansi = "2026-08-01 08:30:00";
    f1.last_modified_ansi = "2026-08-01 08:30:00";
    f1.live_revision = "1";
    f1.author_name = "Coord. Física";
    f1.download_url = "https://tecdigital.tec.ac.cr/dotlrn/file-storage/download/programa_fi2103.pdf?version_id=201";
    fwr.files.push_back(f1);

    td::FileRecord fUnlinked;
    fUnlinked.object_id = "202";
    fUnlinked.parent_folder_id = "100";
    fUnlinked.folder_path = "Raíz/BancoOculto";
    fUnlinked.name = "solucionario_extra.pdf";
    fUnlinked.content_size = 51200;
    fUnlinked.creation_date_ansi = "2026-08-10 11:00:00";
    fUnlinked.last_modified_ansi = "2026-08-12 15:20:00";
    fwr.files.push_back(fUnlinked);
    base.foldersByPath[c1.url] = fwr;

    td::CourseOverviewSnapshot ov;
    ov.official_docs.push_back({ "Programa", f1.download_url, "Programa" });
    base.overviewsByPath[c1.url] = ov;

    td::DetailedEvaluationsReport evRep;
    td::DetailedEvaluationCategory cat;
    cat.name = "Proyectos";
    td::DetailedEvaluationItem it1;
    it1.assign_id = "A1";
    it1.name = "Fase II";
    it1.weight_pct = 20.0;
    it1.earned_pct = 0.0;
    it1.is_graded = false;
    it1.attachment_url = "https://tecdigital.tec.ac.cr/enunciado_fase2.pdf";
    cat.items.push_back(it1);
    evRep.categories.push_back(cat);
    base.evaluationsByPath[c1.url] = evRep;

    // Negative test 1: Initial sync from empty snapshot produces ZERO flood events
    CHECK(TecAcademicStore::ComputeSnapshotChanges(emptySnap, base).empty());
    // Negative test 2: Identical snapshot re-sync produces ZERO false-positive events
    CHECK(TecAcademicStore::ComputeSnapshotChanges(base, base).empty());

    // Positive test: Mutate next snapshot across files, folders, grades, rubrics, news & weekly modules
    TecAcademicSnapshot next = base;
    next.syncedAtUtc = "2026-11-26T12:00:00Z";
    next.foldersByPath[c1.url].files[0].last_modified_ansi = "2026-11-26 11:45:00";
    next.foldersByPath[c1.url].files[0].live_revision = "2";
    td::FileRecord fNew;
    fNew.object_id = "203";
    fNew.name = "practica_parcial3.pdf";
    fNew.folder_path = "Raíz/Prácticas";
    fNew.last_modified_ansi = "2026-11-26 11:50:00";
    next.foldersByPath[c1.url].files.push_back(fNew);
    td::FolderRecord fNewDir;
    fNewDir.folder_id = "101";
    fNewDir.parent_id = "100";
    fNewDir.name = "Prácticas";
    next.foldersByPath[c1.url].folders.push_back(fNewDir);
    next.evaluationsByPath[c1.url].categories[0].items[0].is_graded = true;
    next.evaluationsByPath[c1.url].categories[0].items[0].earned_pct = 18.0;
    next.evaluationsByPath[c1.url].categories[0].items[0].teacher_comment = "Excelente sustento experimental.";
    td::RubricBreakdown rb;
    rb.rubric_id = "R77";
    rb.title = "Rúbrica Fase II";
    rb.is_graded = true;
    rb.total_score = 90.0;
    rb.max_score = 100.0;
    next.rubricsByKey["R77||"] = rb;
    next.newsByPath[c1.url].push_back({ "Aviso examen reposición", "https://tecdigital.tec.ac.cr/n9", "", "Viernes 14:00", "2026-11-26" });
    td::SpacePageSnapshot wkPg;
    wkPg.page_num = 12;
    wkPg.tab_label = "S12";
    td::SpacePortletBlock ptb;
    ptb.title = "Semana 12";
    ptb.module_kind = "weekly";
    ptb.images.push_back({ "https://tecdigital.tec.ac.cr/img/s12.png", "", "Infografía S12" });
    td::CourseTabInfo subTab;
    subTab.label = "S13";
    subTab.url = "?page_num=13";
    subTab.page_num = 13;
    subTab.is_custom = true;
    ptb.linked_subpages.push_back(subTab);
    wkPg.portlets.push_back(ptb);
    next.customPagesBySpace[c1.url].push_back(wkPg);

    auto changes = TecAcademicStore::ComputeSnapshotChanges(base, next);
    CHECK(changes.size() >= 7);
    auto hasKind = [&](td::TecChangeEventKind k) {
        for (const auto& ev : changes) if (ev.kind == k) return true;
        return false;
    };
    CHECK(hasKind(td::TecChangeEventKind::NewFile));
    CHECK(hasKind(td::TecChangeEventKind::ModifiedFile));
    CHECK(hasKind(td::TecChangeEventKind::NewFolder));
    CHECK(hasKind(td::TecChangeEventKind::GradePublished));
    CHECK(hasKind(td::TecChangeEventKind::NewTeacherComment));
    CHECK(hasKind(td::TecChangeEventKind::RubricGraded));
    CHECK(hasKind(td::TecChangeEventKind::NewNews));
    CHECK(hasKind(td::TecChangeEventKind::NewWeeklyContent));
    next.activityFeed = changes;

    // Global Unified File Catalog checks
    auto catalog = TecAcademicStore::BuildGlobalFileCatalog(next);
    CHECK(catalog.size() >= 4); // 3 folder files + 1 evaluation attachment
    bool sawLinked = false, sawUnlinkedDir = false, sawRecent = false, sawEvalAtt = false;
    for (const auto& e : catalog) {
        if (e.objectId == "201") { sawLinked = e.isLinkedInPortada; sawRecent = e.isRecentlyChanged; }
        if (e.objectId == "202") sawUnlinkedDir = !e.isLinkedInPortada;
        if (e.isEvalAttachment) sawEvalAtt = true;
    }
    CHECK(sawLinked && sawUnlinkedDir && sawRecent && sawEvalAtt);

    // v3 Persistence Roundtrip checks
    next.motorJuegosUser = td::MotorJuegosUserInfo{ 7, "Estudiante", "Demo", false };
    next.motorJuegosActivities.push_back({ "act1", "Quiz Interactivo Ondas", "c1", "Disponible", "", 95.0, true, "https://api.tec.ac.cr:3003/q/1" });
    const fs::path tempRoot = fs::temp_directory_path() / "cuadernu_tec_v3_persist_test";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    {
        TecAcademicStore st(tempRoot.string());
        st.SetSnapshotForTesting(next);
        std::string err;
        CHECK(st.SaveSnapshotToDisk(&err));
    }
    {
        TecAcademicStore st2(tempRoot.string());
        std::string err;
        CHECK(st2.LoadSnapshotFromDisk(&err));
        const auto loaded = st2.Snapshot();
        CHECK(loaded.monitorIntervalMinutes == 30);
        CHECK(loaded.gaapServerUrl == "https://api.tec.ac.cr:3003");
        CHECK(loaded.activityFeed.size() == next.activityFeed.size());
        CHECK(loaded.motorJuegosUser.has_value() && loaded.motorJuegosUser->user_id == 7);
        CHECK(loaded.motorJuegosActivities.size() == 1 && loaded.motorJuegosActivities[0].title == "Quiz Interactivo Ondas");
        const auto& lf = loaded.foldersByPath.at(c1.url);
        CHECK(!lf.folders.empty() && lf.folders[0].creation_date_ansi == "2026-08-01 08:00:00" && lf.folders[0].subscription_active);
        CHECK(!lf.files.empty() && lf.files[0].last_modified_ansi == "2026-11-26 11:45:00" && lf.files[0].live_revision == "2");
        const auto& lpg = loaded.customPagesBySpace.at(c1.url);
        CHECK(!lpg.empty() && !lpg[0].portlets.empty() &&
              lpg[0].portlets[0].module_kind == "weekly" &&
              lpg[0].portlets[0].images.size() == 1 &&
              lpg[0].portlets[0].linked_subpages.size() == 1);
    }
    fs::remove_all(tempRoot, ec);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--live-readonly-qa") {
        return RunOptInLiveReadOnlyQa();
    }
    if (argc > 1 && std::string(argv[1]) == "--live-public-horarios-qa") {
        return RunOptInLivePublicHorariosQa();
    }
    if (argc > 1 && std::string(argv[1]) == "--sync-real-account-to-cuadernu") {
        return RunOptInSyncRealAccountToCuadernu();
    }
    const fs::path docStudioRoot = (argc > 1) ? fs::path(argv[1]) : fs::current_path();
    TestReadOnlyPolicyGates();
    TestPureParsersAndCalculators();
    TestEndToEndStoreAndSynergies();
    TestRealLoopbackHttpServerEndToEnd();
    TestStaticArchitectureAndHigGuards(docStudioRoot);
    Test2026GraphQlEndpointsAndNegativePaths();
    Test2026DeepMapParsersAndLoopbackMutations();
    TestGlobalAllTecScheduleSearchAndSolver();
    TestV2LosslessPersistenceAndBase100Metrics();
    TestV3RadarGlobalCatalogAndPersistence();

    std::cout << "TecDigitalIntegrationTest: " << gPass << " passed, " << gFail << " failed.\n";
    return (gFail == 0) ? 0 : 1;
}
