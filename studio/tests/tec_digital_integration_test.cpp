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
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/storage.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
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
        { "POST", "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/actualizar_cursos" },
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

    const std::string mainSrc = ReadWholeFile(docStudioRoot / "app" / "main_studio.cpp");
    CHECK(!mainSrc.empty());
    // Confirm the old static 2-course HorariosTEC demo was removed and replaced with real dialog wiring
    CHECK(mainSrc.find("RunTecDigitalCenterDialog") != std::string::npos);
    CHECK(mainSrc.find("CMD_ACADEMIC_MALLA_TEC") != std::string::npos);
    CHECK(mainSrc.find("CMD_ACADEMIC_NOTAS_TEC") != std::string::npos);
    CHECK(mainSrc.find("CMD_ACADEMIC_CALENDAR_TEC") != std::string::npos);
    CHECK(mainSrc.find("c1.codeUtf8 = \"MA1103\"") == std::string::npos);
}

}  // namespace

int main(int argc, char** argv) {
    const fs::path docStudioRoot = (argc > 1) ? fs::path(argv[1]) : fs::current_path();
    TestReadOnlyPolicyGates();
    TestPureParsersAndCalculators();
    TestEndToEndStoreAndSynergies();
    TestRealLoopbackHttpServerEndToEnd();
    TestStaticArchitectureAndHigGuards(docStudioRoot);

    std::cout << "TecDigitalIntegrationTest: " << gPass << " passed, " << gFail << " failed.\n";
    return (gFail == 0) ? 0 : 1;
}
