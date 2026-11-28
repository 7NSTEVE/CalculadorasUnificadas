#include "academic/matricula_tec/client.hpp"
#include "academic/matricula_tec/matricula_c_api.h"
#include "academic/matricula_tec/protocol.hpp"
#include "academic/matricula_tec/scenario_engine.hpp"
#include "academic/matricula_tec/scheduler.hpp"
#include "academic/matricula_tec/vault.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using namespace academic::matricula_tec;
using json = nlohmann::json;

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        ++g_passed;
        std::cout << "[PASS] " << name << "\n";
    } else {
        ++g_failed;
        std::cerr << "[FAIL] " << name << "\n";
    }
}

// Minimal local WinSock mock ITCR ASP.NET Matricula server for full E2E verification
class MockTecMatriculaServer {
public:
    std::atomic<int> enroll_calls{ 0 };
    std::atomic<int> informe_calls{ 0 };
    std::atomic<int> login_calls{ 0 };
    std::atomic<bool> fail_plan_a_seat{ false };
    std::mutex state_mtx;
    std::vector<std::pair<std::string, int>> enrolled;

    bool Start() {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock_ == INVALID_SOCKET) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
        int len = sizeof(addr);
        getsockname(listen_sock_, reinterpret_cast<sockaddr*>(&addr), &len);
        port_ = ntohs(addr.sin_port);
        if (listen(listen_sock_, 16) != 0) return false;

        running_.store(true);
        th_ = std::thread(&MockTecMatriculaServer::Loop, this);
        return true;
    }

    std::string BaseUrl() const {
        return "http://127.0.0.1:" + std::to_string(port_) + "/Matricula/";
    }

    void Stop() {
        running_.store(false);
        if (listen_sock_ != INVALID_SOCKET) {
            closesocket(listen_sock_);
            listen_sock_ = INVALID_SOCKET;
        }
        if (th_.joinable()) th_.join();
        WSACleanup();
    }

    ~MockTecMatriculaServer() { Stop(); }

private:
    void Loop() {
        while (running_.load()) {
            SOCKET client = accept(listen_sock_, nullptr, nullptr);
            if (client == INVALID_SOCKET) break;
            HandleClient(client);
            closesocket(client);
        }
    }

    void HandleClient(SOCKET s) {
        std::string req;
        char buf[4096];
        size_t header_end = std::string::npos;
        size_t content_len = 0;
        while (true) {
            int n = recv(s, buf, sizeof(buf), 0);
            if (n <= 0) break;
            req.append(buf, static_cast<size_t>(n));
            if (header_end == std::string::npos) {
                header_end = req.find("\r\n\r\n");
                if (header_end != std::string::npos) {
                    const std::string hdrs = req.substr(0, header_end);
                    std::istringstream iss(hdrs);
                    std::string ln;
                    while (std::getline(iss, ln)) {
                        if (_strnicmp(ln.c_str(), "Content-Length:", 15) == 0) {
                            content_len = static_cast<size_t>(std::stoul(ln.substr(15)));
                        }
                    }
                }
            }
            if (header_end != std::string::npos && req.size() >= header_end + 4 + content_len) break;
        }
        if (req.empty()) return;

        const size_t sp1 = req.find(' ');
        const size_t sp2 = (sp1 != std::string::npos) ? req.find(' ', sp1 + 1) : std::string::npos;
        if (sp1 == std::string::npos || sp2 == std::string::npos) return;
        const std::string method = req.substr(0, sp1);
        const std::string path = req.substr(sp1 + 1, sp2 - sp1 - 1);
        const std::string body = (header_end != std::string::npos) ? req.substr(header_end + 4) : "";

        std::string status = "200 OK";
        std::string ctype = "application/json; charset=utf-8";
        std::string extra_hdrs;
        std::string resp_body;

        if (path == "/Matricula/frmAutenticacion.aspx" && (method == "GET" || method == "HEAD")) {
            ctype = "text/html; charset=utf-8";
            extra_hdrs = "Set-Cookie: ASP.NET_SessionId=mock_sess_123; path=/; HttpOnly\r\n";
            if (method == "GET") {
                resp_body = "<html><body><form>"
                            "<input id=\"__VIEWSTATE\" name=\"__VIEWSTATE\" value=\"AA+BB/CC==\" />"
                            "<input id=\"__VIEWSTATEGENERATOR\" name=\"__VIEWSTATEGENERATOR\" value=\"1234ABCD\" />"
                            "<input id=\"__EVENTVALIDATION\" name=\"__EVENTVALIDATION\" value=\"EV+99/==\" />"
                            "</form></body></html>";
            }
        } else if (path == "/Matricula/frmAutenticacion.aspx" && method == "POST") {
            ++login_calls;
            // Verify strict percent-encoding of '+' (%2B) and '$' (%24)
            if (body.contains("%2B") && body.contains("%24")) {
                status = "302 Found";
                ctype = "text/html; charset=utf-8";
                extra_hdrs = "Location: /Matricula/frmMatricula.aspx\r\n"
                             "Set-Cookie: .ASPXAUTH=mock_auth_tok; path=/; HttpOnly\r\n";
                resp_body = "Redirecting";
            } else {
                status = "200 OK";
                ctype = "text/html; charset=utf-8";
                resp_body = "<html><input name=\"txtPassword\" />Invalid encoding</html>";
            }
        } else if (path == "/Matricula/frmMatricula.aspx" && method == "GET") {
            ctype = "text/html; charset=utf-8";
            resp_body = "<html><body>"
                        "<input id=\"hfCarnet\" value=\"2024000001\" />"
                        "<input id=\"hfNombre\" value=\"ESTUDIANTE PRUEBA\" />"
                        "<input id=\"hfCarrera\" value=\"EL\" />"
                        "<input id=\"hfPeriodo\" value=\"2026-2\" />"
                        "<input id=\"hfConvocatoria\" value=\"C1\" />"
                        "<select id=\"cmbConvocatoria\"><option value=\"C1|2026-2\">Convocatoria 1</option></select>"
                        "</body></html>";
        } else if (path == "/Matricula/frmMatricula.aspx/ObtieneCursos") {
            resp_body = json{ { "d", json::array({
                { { "COD_MATERIA", "FI1204" }, { "NOM_MATERIA", "FISICA III" }, { "CAN_CREDITOS", 3 } },
                { { "COD_MATERIA", "EL3307" }, { "NOM_MATERIA", "DISENO ELECTRICO" }, { "CAN_CREDITOS", 4 } }
            }) } }.dump();
        } else if (path == "/Matricula/frmMatricula.aspx/ConsultaCitaMatriculaEstudiante") {
            resp_body = json{ { "d", {
                { "TieneCita", true },
                { "Fecha", "2026-11-27" },
                { "HoraInicio", "08:00" },
                { "Activa", true }
            } } }.dump();
        } else if (path == "/Matricula/frmMatricula.aspx/ConsultaMorosidad") {
            resp_body = json{ { "d", { { "TieneMorosidad", false } } } }.dump();
        } else if (path == "/Matricula/frmMatricula.aspx/ConsultaHorarios") {
            json req_j = json::parse(body);
            const std::string cod = req_j.value("codMateria", "");
            if (cod == "FI1204") {
                const int g1_seats = fail_plan_a_seat.load() ? 0 : 2;
                resp_body = json{ { "d", json::array({
                    {
                        { "COD_MATERIA", "FI1204" }, { "NUM_GRUPO", 1 }, { "CupoDisponible", g1_seats },
                        { "NOM_SEDE", "Cartago" },
                        { "Horarios", json::array({ { { "Dia", "L" }, { "Inicio", "07:30" }, { "Fin", "09:20" } } }) }
                    },
                    {
                        { "COD_MATERIA", "FI1204" }, { "NUM_GRUPO", 2 }, { "CupoDisponible", 5 },
                        { "NOM_SEDE", "Cartago" },
                        { "Horarios", json::array({ { { "Dia", "K" }, { "Inicio", "09:30" }, { "Fin", "11:20" } } }) }
                    }
                }) } }.dump();
            } else {
                resp_body = json{ { "d", json::array({
                    {
                        { "COD_MATERIA", cod }, { "NUM_GRUPO", 1 }, { "CupoDisponible", 4 },
                        { "NOM_SEDE", "Cartago" },
                        { "Horarios", json::array({ { { "Dia", "M" }, { "Inicio", "13:00" }, { "Fin", "14:50" } } }) }
                    }
                }) } }.dump();
            }
        } else if (path == "/Matricula/frmMatricula.aspx/ConsultaHorarioMatriculado") {
            json arr = json::array();
            std::lock_guard<std::mutex> lk(state_mtx);
            for (const auto& [c, g] : enrolled) {
                arr.push_back({ { "COD_MATERIA", c }, { "NUM_GRUPO", g } });
            }
            resp_body = json{ { "d", arr } }.dump();
        } else if (path == "/Matricula/frmMatricula.aspx/AgregarMatricula") {
            ++enroll_calls;
            json req_j = json::parse(body);
            const std::string cod = req_j.value("codMateria", "");
            const int grp = req_j.value("numGrupo", 0);
            if (cod == "FI1204" && grp == 1 && fail_plan_a_seat.load()) {
                resp_body = json{ { "d", "SCupo" } }.dump();
            } else {
                {
                    std::lock_guard<std::mutex> lk(state_mtx);
                    enrolled.push_back({ cod, grp });
                }
                resp_body = json{ { "d", "OK" } }.dump();
            }
        } else if (path == "/Matricula/frmInformeMatricula.aspx/GeneraInforme") {
            resp_body = json{ { "d", "<html><body>Comprobante Oficial TEC</body></html>" } }.dump();
        } else if (path == "/Matricula/frmInformeMatricula.aspx/EnviaInforme") {
            ++informe_calls;
            resp_body = json{ { "d", "OK" } }.dump();
        } else {
            status = "404 Not Found";
            resp_body = "{\"error\":\"not_found\"}";
        }

        std::ostringstream oss;
        oss << "HTTP/1.1 " << status << "\r\n"
            << "Date: Fri, 27 Nov 2026 22:00:00 GMT\r\n"
            << "Content-Type: " << ctype << "\r\n"
            << "Content-Length: " << resp_body.size() << "\r\n"
            << extra_hdrs
            << "Connection: close\r\n\r\n"
            << resp_body;
        const std::string wire = oss.str();
        send(s, wire.data(), static_cast<int>(wire.size()), 0);
    }

    SOCKET listen_sock_ = INVALID_SOCKET;
    uint16_t port_ = 0;
    std::atomic<bool> running_{ false };
    std::thread th_;
};

} // namespace

int main() {
    // 1. Strict C23 codec & day/time normalization
    Check(StrictPercentEncode("A+B/C=D$E") == "A%2BB%2FC%3DD%24E", "c23_strict_percent_encode");
    Check(NormalizeTecDay("K") == 1 && NormalizeTecDay("2") == 1 && NormalizeTecDay("Martes") == 1, "c23_day_martes_K_and_2");
    Check(NormalizeTecDay("M") == 2 && NormalizeTecDay("Miércoles") == 2, "c23_day_miercoles_utf8");
    Check(ParseTimeToMinutes("07:30") == 450 && ParseTimeToMinutes("13:45") == 825, "c23_parse_time");
    Check(matricula_intervals_overlap(1, 450, 560, 1, 500, 620), "c23_overlap_true");
    Check(!matricula_intervals_overlap(1, 450, 560, 1, 560, 660), "c23_overlap_touching_false");
    Check(!matricula_intervals_overlap(1, 450, 560, 2, 450, 560), "c23_overlap_diff_day_false");

    // 2. Scenario Engine: Plan A -> Plan B fallback, MRV ordering, SMaterias safety floor
    {
        CourseGroupLive fi_g1{};
        fi_g1.course_code = "FI1204"; fi_g1.grupo = 1; fi_g1.cupo_disponible = 0; // Full!
        fi_g1.sesiones.push_back({ 0, "L", 450, 560, "07:30", "09:20", "D3", "4" });

        CourseGroupLive fi_g2{};
        fi_g2.course_code = "FI1204"; fi_g2.grupo = 2; fi_g2.cupo_disponible = 2;
        fi_g2.sesiones.push_back({ 1, "K", 570, 680, "09:30", "11:20", "D3", "5" });

        CourseGroupLive el_g1{};
        el_g1.course_code = "EL3307"; el_g1.grupo = 1; el_g1.cupo_disponible = 10;
        el_g1.sesiones.push_back({ 2, "M", 780, 890, "13:00", "14:50", "E1", "2" });

        std::map<std::string, std::vector<CourseGroupLive>> live = {
            { "FI1204", { fi_g1, fi_g2 } },
            { "EL3307", { el_g1 } }
        };

        ScenarioPlan plan{};
        EnrollmentScenario plan_a{};
        plan_a.id = "plan_a"; plan_a.name = "Plan A"; plan_a.priority = 1;
        CourseTarget t_a{}; t_a.course_code = "FI1204"; t_a.filter.allowed_groups = { 1 };
        plan_a.targets.push_back(t_a);

        EnrollmentScenario plan_b{};
        plan_b.id = "plan_b"; plan_b.name = "Plan B"; plan_b.priority = 2;
        CourseTarget t_b1{}; t_b1.course_code = "EL3307"; t_b1.filter.allowed_groups = { 1 };
        CourseTarget t_b2{}; t_b2.course_code = "FI1204"; t_b2.filter.allowed_groups = { 2 };
        plan_b.targets.push_back(t_b1);
        plan_b.targets.push_back(t_b2);

        plan.scenarios = { plan_a, plan_b };
        const auto ev = ScenarioEngine::SelectBestPlan(plan, live, {});
        Check(ev.feasible && ev.scenario_id == "plan_b", "scenario_pivot_plan_a_to_b");
        Check(ev.choices.size() == 2 && ev.choices[0].course_code == "FI1204", "scenario_mrv_tightest_seat_first");

        // StrictlyConservative never drops conflicting enrolled course
        EnrolledScheduleItem existing{};
        existing.course_code = "MA2104"; existing.grupo = 3;
        existing.sesiones.push_back({ 1, "K", 570, 680, "09:30", "11:20", "", "" });
        plan.enable_plan_z_rescue = false;
        const auto ev_strict = ScenarioEngine::SelectBestPlan(plan, live, { existing });
        Check(!ev_strict.feasible && ev_strict.drop_courses_required.empty(), "strictly_conservative_never_drops");

        // AllowPriorityScenarioReplace respects SMaterias floor (cannot drop last 1 enrolled course!)
        plan.pivot_policy = PivotPolicy::AllowPriorityScenarioReplace;
        plan.min_enrolled_courses_safety_floor = 1;
        const auto ev_floor = ScenarioEngine::SelectBestPlan(plan, live, { existing });
        Check(!ev_floor.feasible, "smaterias_floor_blocks_drop_to_zero");

        // With 2 enrolled courses, dropping 1 conflicting course keeps >= 1 (safe)
        EnrolledScheduleItem existing2{};
        existing2.course_code = "CI1230"; existing2.grupo = 1;
        existing2.sesiones.push_back({ 4, "V", 900, 1010, "15:00", "16:50", "", "" });
        const auto ev_replace = ScenarioEngine::SelectBestPlan(plan, live, { existing, existing2 });
        Check(ev_replace.feasible && ev_replace.drop_courses_required.size() == 1 &&
                  ev_replace.drop_courses_required[0] == "MA2104",
              "opt_in_replace_respects_safety_floor");
    }

    // 3. Protected RAM Vault + Windows DPAPI file round-trip
    {
        ProtectedCredentialVault v;
        Check(v.StoreInMemory("user_test@estudiantec.cr", "P@ss$1234+/", nullptr), "ram_vault_store");
        ProtectedCredentials out;
        Check(v.RetrieveFromMemory(out, nullptr) && out.username == "user_test@estudiantec.cr" &&
                  out.password == "P@ss$1234+/",
              "ram_vault_retrieve");

        const auto tmp_path = (std::filesystem::temp_directory_path() / "cuadernu_test_vault_c23.dpapi").string();
        Check(ProtectedCredentialVault::SaveToDpapiFile(tmp_path, "user_dpapi@estudiantec.cr", "Secr3t$Key!", nullptr),
              "dpapi_save");
        ProtectedCredentials dp_out;
        Check(ProtectedCredentialVault::LoadFromDpapiFile(tmp_path, dp_out, nullptr) &&
                  dp_out.username == "user_dpapi@estudiantec.cr" && dp_out.password == "Secr3t$Key!",
              "dpapi_load_roundtrip");
        Check(ProtectedCredentialVault::DeleteDpapiFile(tmp_path, nullptr), "dpapi_delete_secure");
    }

    // 4. Safety lock negative tests + full loopback Mock ITCR Server E2E (incl. Scheduler + EnviaInforme)
    {
        MockTecMatriculaServer srv;
        Check(srv.Start(), "mock_server_start");

        auto client = std::make_shared<MatriculaClient>(srv.BaseUrl());
        client->SetPacingMs(0);

        // Negative: refuse wrong phrase & refuse live mutation when unarmed
        std::string err;
        Check(!client->ArmLiveMutations(true, "frase incorrecta", &err), "safety_refuses_wrong_phrase");
        const auto blocked = client->EnrollCourse("FI1204", 1, false);
        Check(!blocked.ok && blocked.datic_code == "SAFETY_LOCK", "safety_blocks_unarmed_enroll");
        const auto blocked_inf = client->SendInformeEmail("<html></html>", false);
        Check(!blocked_inf.ok && blocked_inf.datic_code == "SAFETY_LOCK", "safety_blocks_unarmed_informe");

        // Login with password containing '$' and '+' (verifies C23 strict percent-encoding!)
        Check(client->Login("student@estudiantec.cr", "Abc$123+", &err), "mock_login_strict_encoding");
        Check(client->Bootstrap().carnet == "2024000001", "mock_bootstrap_carnet");

        std::vector<CourseOffer> courses;
        Check(client->FetchCoursesAndBindConvocatoria("C1", "2026-2", courses, &err) && courses.size() == 2,
              "mock_obtiene_cursos");

        CitaInfo cita{};
        Check(client->FetchCita(cita, &err) && cita.tiene_cita, "mock_consulta_cita");
        MorosidadInfo mor{};
        Check(client->FetchMorosidad(mor, &err) && !mor.tiene_morosidad, "mock_consulta_morosidad");

        // Autonomous Scheduler E2E: Plan A fails (seat 0), pivots to Plan B, enrolls & calls EnviaInforme!
        srv.fail_plan_a_seat.store(true);
        AppointmentScheduler sched(client);
        Check(sched.SetCredentials("student@estudiantec.cr", "Abc$123+", &err), "sched_set_creds");
        Check(sched.ArmMission(true, kConfirmationPhrase, &err), "sched_arm_live");

        AppointmentMissionConfig cfg{};
        cfg.base_url = srv.BaseUrl();
        cfg.cita_epoch_utc = 0; // immediate execution
        cfg.pre_login_lead_seconds = 120;
        cfg.re_login_before_seconds = 5;
        cfg.dry_run = false;
        cfg.transport_mode = TransportMode::ApiOnly;
        cfg.send_informe_after_enroll = true;
        cfg.plan.convocatoria_id = "C1";
        cfg.plan.periodo_id = "2026-2";

        EnrollmentScenario sc_a{ "plan_a", "Plan A", 1, true, {} };
        CourseTarget ta{}; ta.course_code = "FI1204"; ta.filter.allowed_groups = { 1 };
        sc_a.targets.push_back(ta);

        EnrollmentScenario sc_b{ "plan_b", "Plan B", 2, true, {} };
        CourseTarget tb{}; tb.course_code = "FI1204"; tb.filter.allowed_groups = { 2 };
        sc_b.targets.push_back(tb);

        cfg.plan.scenarios = { sc_a, sc_b };
        Check(sched.StartMission(cfg, nullptr, &err), "sched_start_mission");
        sched.WaitForMission();

        const auto st = sched.CurrentStatus();
        Check(st.state == SchedulerState::Completed, "sched_completed");
        Check(st.enrolled_count == 1, "sched_enrolled_plan_b");
        Check(st.informe_sent && srv.informe_calls.load() == 1, "sched_sent_envia_informe");
        Check(!sched.IsArmed(), "sched_auto_disarmed_on_finish");

        srv.Stop();
    }

    std::cout << "SUMMARY: passed=" << g_passed << " failed=" << g_failed << "\n";
    if (g_failed == 0) {
        std::cout << "MATRICULA_TEC_TESTS_PASS\n";
        return 0;
    }
    return 1;
}
