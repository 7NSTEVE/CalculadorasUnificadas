#include "academic/matricula_tec/client.hpp"
#include "academic/matricula_tec/protocol.hpp"

#include <chrono>
#include <thread>

using json = nlohmann::json;
namespace td = academic::tec_digital;

namespace academic::matricula_tec {

namespace {

std::string NormalizeBaseUrl(std::string url) {
    if (url.empty()) url = kDefaultBaseUrl;
    if (url.back() != '/') url.push_back('/');
    return url;
}

int64_t SteadyNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int64_t SystemNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

MatriculaClient::MatriculaClient(
    std::string base_url,
    std::unique_ptr<td::IHttpTransport> transport)
    : base_url_(NormalizeBaseUrl(std::move(base_url))),
      transport_(transport ? std::move(transport) : td::CreateNativeHttpTransport())
{}

void MatriculaClient::SetBaseUrl(const std::string& base_url) {
    std::lock_guard<std::mutex> lk(mtx_);
    base_url_ = NormalizeBaseUrl(base_url);
}

std::string MatriculaClient::BaseUrl() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return base_url_;
}

bool MatriculaClient::ArmLiveMutations(bool armed, const std::string& confirmation_phrase, std::string* out_err) {
    if (!armed) {
        armed_.store(false, std::memory_order_release);
        return true;
    }
    if (confirmation_phrase != kConfirmationPhrase) {
        armed_.store(false, std::memory_order_release);
        if (out_err) {
            *out_err = std::string("Frase de confirmacion invalida. Se requiere exactamente: ") + kConfirmationPhrase;
        }
        return false;
    }
    armed_.store(true, std::memory_order_release);
    return true;
}

void MatriculaClient::Disarm() noexcept {
    armed_.store(false, std::memory_order_release);
}

bool MatriculaClient::IsArmed() const noexcept {
    return armed_.load(std::memory_order_acquire);
}

bool MatriculaClient::IsAuthenticated() const noexcept {
    return authenticated_.load(std::memory_order_acquire);
}

StudentBootstrap MatriculaClient::Bootstrap() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return bootstrap_;
}

int64_t MatriculaClient::ServerClockOffsetMs() const noexcept {
    return server_offset_ms_.load(std::memory_order_acquire);
}

int64_t MatriculaClient::EstimatedServerEpochSeconds() const noexcept {
    return (SystemNowMs() + server_offset_ms_.load(std::memory_order_acquire)) / 1000;
}

void MatriculaClient::SetPacingMs(int ms) noexcept {
    pacing_ms_.store(ms < 0 ? 0 : ms, std::memory_order_release);
}

void MatriculaClient::EnforcePacing() {
    const int p = pacing_ms_.load(std::memory_order_acquire);
    if (p <= 0) return;
    const int64_t now = SteadyNowMs();
    if (last_req_steady_ms_ > 0) {
        const int64_t elapsed = now - last_req_steady_ms_;
        if (elapsed < p) {
            std::this_thread::sleep_for(std::chrono::milliseconds(p - elapsed));
        }
    }
    last_req_steady_ms_ = SteadyNowMs();
}

void MatriculaClient::UpdateClockOffsetFromResponse(const td::HttpResponse& resp) {
    const std::string date_hdr = resp.GetHeaderCI("Date");
    if (date_hdr.empty()) return;
    int64_t server_sec = 0;
    if (td::CookieJar::ParseHttpDate(date_hdr, server_sec)) {
        const int64_t local_ms = SystemNowMs();
        const int64_t offset_ms = (server_sec * 1000) - local_ms;
        server_offset_ms_.store(offset_ms, std::memory_order_release);
    }
}

td::HttpResponse MatriculaClient::PerformRequest(td::HttpRequest req) {
    EnforcePacing();
    const auto parsed = td::ParseHttpUrl(req.url);
    const std::string cookie_hdr = cookies_.BuildCookieHeader(req.url);
    if (!cookie_hdr.empty()) {
        req.headers["Cookie"] = cookie_hdr;
    }
    if (!req.headers.contains("User-Agent")) {
        req.headers["User-Agent"] =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
    }

    td::HttpResponse resp = transport_->Execute(req);
    if (resp.transport_ok) {
        if (!resp.set_cookies.empty() && parsed.valid) {
            cookies_.ApplySetCookieHeaders(resp.set_cookies, parsed.host, parsed.path);
        }
        UpdateClockOffsetFromResponse(resp);
    }
    return resp;
}

bool MatriculaClient::ProbePortalReachability(int* out_http_status, bool* out_tls_verified, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    td::HttpRequest req{};
    req.method = "GET";
    req.url = base_url_ + "frmAutenticacion.aspx";
    req.connect_timeout_ms = 12000;
    req.receive_timeout_ms = 15000;

    const td::HttpResponse resp = PerformRequest(std::move(req));
    if (out_http_status) *out_http_status = resp.status_code;
    if (out_tls_verified) *out_tls_verified = resp.tls_verified;
    if (!resp.transport_ok) {
        if (out_err) *out_err = resp.error_message;
        return false;
    }
    return resp.status_code >= 200 && resp.status_code < 400;
}

bool MatriculaClient::SyncServerClock(int64_t* out_offset_ms, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    td::HttpRequest req{};
    req.method = "HEAD";
    req.url = base_url_ + "frmAutenticacion.aspx";
    td::HttpResponse resp = PerformRequest(req);
    if (!resp.transport_ok || resp.GetHeaderCI("Date").empty()) {
        req.method = "GET";
        resp = PerformRequest(req);
    }
    if (!resp.transport_ok) {
        if (out_err) *out_err = resp.error_message;
        return false;
    }
    if (out_offset_ms) *out_offset_ms = server_offset_ms_.load(std::memory_order_acquire);
    return true;
}

void MatriculaClient::LogoutLocal() {
    std::lock_guard<std::mutex> lk(mtx_);
    cookies_.Clear();
    bootstrap_ = StudentBootstrap{};
    bound_convocatoria_.clear();
    bound_periodo_.clear();
    authenticated_.store(false, std::memory_order_release);
    armed_.store(false, std::memory_order_release);
}

bool MatriculaClient::Login(const std::string& username, const std::string& password, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    // Safety invariant: always reset armed_ = false on login
    armed_.store(false, std::memory_order_release);
    authenticated_.store(false, std::memory_order_release);
    cookies_.Clear();
    bootstrap_ = StudentBootstrap{};
    bound_convocatoria_.clear();
    bound_periodo_.clear();

    if (username.empty() || password.empty()) {
        if (out_err) *out_err = "Usuario y contrasena requeridos.";
        return false;
    }

    // 1. GET frmAutenticacion.aspx to harvest ASP.NET WebForms tokens + initial session cookie
    td::HttpRequest get_req{};
    get_req.method = "GET";
    get_req.url = base_url_ + "frmAutenticacion.aspx";
    td::HttpResponse get_resp = PerformRequest(std::move(get_req));
    if (!get_resp.transport_ok || get_resp.status_code < 200 || get_resp.status_code >= 400) {
        if (out_err) *out_err = "Error GET frmAutenticacion.aspx: HTTP " + std::to_string(get_resp.status_code) + " " + get_resp.error_message;
        return false;
    }

    const WebFormsTokens tokens = ExtractWebFormsTokens(get_resp.BodyAsString());
    if (!tokens.IsComplete()) {
        if (out_err) *out_err = "No se encontraron tokens __VIEWSTATE/__EVENTVALIDATION en frmAutenticacion.aspx.";
        return false;
    }

    // 2. POST frmAutenticacion.aspx with strictly percent-encoded body (+, /, =, $ safe)
    std::vector<std::pair<std::string, std::string>> form_fields = {
        { "__VIEWSTATE", tokens.view_state },
        { "__VIEWSTATEGENERATOR", tokens.view_state_generator },
        { "__EVENTVALIDATION", tokens.event_validation },
        { "txtUsuario", username },
        { "txtPassword", password },
        { "btnIngresar", "Ingresar" }
    };

    const std::string login_url = base_url_ + "frmAutenticacion.aspx";
    td::HttpRequest post_req{};
    post_req.method = "POST";
    post_req.url = login_url;
    post_req.headers["Content-Type"] = "application/x-www-form-urlencoded";
    post_req.headers["Referer"] = login_url;
    post_req.body = BuildFormUrlEncoded(form_fields);

    td::HttpResponse post_resp = PerformRequest(std::move(post_req));
    if (!post_resp.transport_ok) {
        if (out_err) *out_err = "Error POST login: " + post_resp.error_message;
        return false;
    }

    std::string mat_html;
    if (post_resp.status_code == 302 || post_resp.status_code == 301 || post_resp.status_code == 303) {
        const std::string target_url = td::ResolveRedirectLocation(login_url, post_resp.location);
        if (target_url.empty() || target_url.contains("frmAutenticacion")) {
            if (out_err) *out_err = "Credenciales rechazadas por el portal TEC.";
            return false;
        }
        td::HttpRequest mat_req{};
        mat_req.method = "GET";
        mat_req.url = target_url;
        mat_req.headers["Referer"] = base_url_ + "frmAutenticacion.aspx";
        td::HttpResponse mat_resp = PerformRequest(std::move(mat_req));
        if (!mat_resp.transport_ok || mat_resp.status_code != 200) {
            if (out_err) *out_err = "No se pudo cargar frmMatricula.aspx tras login (HTTP " + std::to_string(mat_resp.status_code) + ").";
            return false;
        }
        mat_html = mat_resp.BodyAsString();
    } else if (post_resp.status_code == 200) {
        mat_html = post_resp.BodyAsString();
        if (mat_html.contains("txtPassword") && !mat_html.contains("hfCarnet") && !mat_html.contains("frmMatricula")) {
            if (out_err) *out_err = "Credenciales invalidas o sesion no autorizada.";
            return false;
        }
    } else {
        if (out_err) *out_err = "HTTP inesperado en autenticacion: " + std::to_string(post_resp.status_code);
        return false;
    }

    bootstrap_ = ParseFrmMatriculaBootstrap(mat_html);
    authenticated_.store(true, std::memory_order_release);
    return true;
}

bool MatriculaClient::CallWebMethod(const std::string& page_and_method,
                                    const json& payload,
                                    json& out_unwrapped,
                                    int* out_status,
                                    std::string* out_err)
{
    if (!authenticated_.load(std::memory_order_acquire)) {
        if (out_err) *out_err = "No autenticado en el portal de Matricula TEC.";
        return false;
    }

    td::HttpRequest req{};
    req.method = "POST";
    req.url = base_url_ + page_and_method;
    req.headers["Content-Type"] = "application/json; charset=utf-8";
    req.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    req.headers["X-Requested-With"] = "XMLHttpRequest";
    req.headers["Referer"] = base_url_ + "frmMatricula.aspx";
    req.body = payload.dump();

    td::HttpResponse resp = PerformRequest(std::move(req));
    if (out_status) *out_status = resp.status_code;
    if (!resp.transport_ok) {
        if (out_err) *out_err = resp.error_message;
        return false;
    }
    if (resp.status_code == 302 || resp.status_code == 401 || resp.status_code == 403) {
        authenticated_.store(false, std::memory_order_release);
        if (out_err) *out_err = "Sesion expirada o redirigida al login (HTTP " + std::to_string(resp.status_code) + ").";
        return false;
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        if (out_err) *out_err = "HTTP " + std::to_string(resp.status_code) + " en " + page_and_method;
        return false;
    }
    try {
        json parsed = json::parse(resp.BodyAsString());
        out_unwrapped = UnwrapAspNetD(parsed);
        return true;
    } catch (const std::exception& ex) {
        if (out_err) *out_err = std::string("JSON invalido en ") + page_and_method + ": " + ex.what();
        return false;
    }
}

bool MatriculaClient::FetchCoursesAndBindConvocatoria(const std::string& id_convocatoria,
                                                      const std::string& id_periodo,
                                                      std::vector<CourseOffer>& out_courses,
                                                      std::string* out_err)
{
    std::lock_guard<std::mutex> lk(mtx_);
    out_courses.clear();
    const std::string conv = id_convocatoria.empty() ? bootstrap_.convocatoria_activa : id_convocatoria;
    const std::string per = id_periodo.empty() ? bootstrap_.periodo_activo : id_periodo;

    json payload = {
        { "idConvocatoria", conv },
        { "idPeriodo", per }
    };
    json unwrapped;
    if (!CallWebMethod("frmMatricula.aspx/ObtieneCursos", payload, unwrapped, nullptr, out_err)) {
        return false;
    }
    bound_convocatoria_ = conv;
    bound_periodo_ = per;
    out_courses = ParseCursosPayload(unwrapped);
    return true;
}

bool MatriculaClient::FetchLiveGroups(const std::string& course_code,
                                      std::vector<CourseGroupLive>& out_groups,
                                      std::string* out_err)
{
    std::lock_guard<std::mutex> lk(mtx_);
    out_groups.clear();
    json payload = {
        { "codMateria", course_code },
        { "idConvocatoria", bound_convocatoria_.empty() ? bootstrap_.convocatoria_activa : bound_convocatoria_ },
        { "idPeriodo", bound_periodo_.empty() ? bootstrap_.periodo_activo : bound_periodo_ }
    };
    json unwrapped;
    if (!CallWebMethod("frmMatricula.aspx/ConsultaHorarios", payload, unwrapped, nullptr, out_err)) {
        return false;
    }
    out_groups = ParseHorariosPayload(unwrapped, course_code);
    return true;
}

bool MatriculaClient::FetchCita(CitaInfo& out_cita, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    json unwrapped;
    if (!CallWebMethod("frmMatricula.aspx/ConsultaCitaMatriculaEstudiante", json::object(), unwrapped, nullptr, out_err)) {
        return false;
    }
    out_cita = ParseCitaPayload(unwrapped);
    return true;
}

bool MatriculaClient::FetchMorosidad(MorosidadInfo& out_morosidad, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    json unwrapped;
    if (!CallWebMethod("frmMatricula.aspx/ConsultaMorosidad", json::object(), unwrapped, nullptr, out_err)) {
        return false;
    }
    out_morosidad = ParseMorosidadPayload(unwrapped);
    return true;
}

bool MatriculaClient::FetchEnrolledSchedule(std::vector<EnrolledScheduleItem>& out_enrolled, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    out_enrolled.clear();
    json unwrapped;
    if (!CallWebMethod("frmMatricula.aspx/ConsultaHorarioMatriculado", json::object(), unwrapped, nullptr, out_err)) {
        return false;
    }
    out_enrolled = ParseHorarioMatriculadoPayload(unwrapped);
    return true;
}

bool MatriculaClient::GenerateInforme(const std::string& tipo, InformeSummary& out_informe, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    json payload = { { "tipo", tipo.empty() ? "E" : tipo } };
    json unwrapped;
    if (!CallWebMethod("frmInformeMatricula.aspx/GeneraInforme", payload, unwrapped, nullptr, out_err)) {
        return false;
    }
    out_informe = ParseGeneraInformePayload(unwrapped);
    return out_informe.ok;
}

EnrollmentResult MatriculaClient::EnrollCourse(const std::string& course_code,
                                               int grupo,
                                               bool dry_run,
                                               const std::string& corequisite_code,
                                               int corequisite_group)
{
    if (dry_run) {
        EnrollmentResult r{};
        r.ok = true;
        r.dry_run = true;
        r.transport_used = "dry_run";
        r.action = "enroll";
        r.course_code = course_code;
        r.grupo = grupo;
        r.datic_code = "DRY_RUN_OK";
        r.message = "[DRY-RUN] Simulacion sin envio real para " + course_code + " G" + std::to_string(grupo);
        return r;
    }

    if (!armed_.load(std::memory_order_acquire)) {
        EnrollmentResult r{};
        r.ok = false;
        r.dry_run = false;
        r.transport_used = "blocked";
        r.action = "enroll";
        r.course_code = course_code;
        r.grupo = grupo;
        r.datic_code = "SAFETY_LOCK";
        r.message = "Bloqueado: el candado de matricula real NO esta armado (requiere CONFIRMAR MATRICULA REAL).";
        return r;
    }

    std::lock_guard<std::mutex> lk(mtx_);
    const bool has_coreq = !corequisite_code.empty() && corequisite_group > 0;
    const std::string method = has_coreq
                                   ? "frmMatricula.aspx/AgregarMatricularCorequisito"
                                   : "frmMatricula.aspx/AgregarMatricula";
    json payload = {
        { "codMateria", course_code },
        { "numGrupo", grupo }
    };
    if (has_coreq) {
        payload["codCorequisito"] = corequisite_code;
        payload["numGrupoCorequisito"] = corequisite_group;
    }

    json unwrapped;
    int status = 0;
    std::string err;
    if (!CallWebMethod(method, payload, unwrapped, &status, &err)) {
        EnrollmentResult r{};
        r.ok = false;
        r.dry_run = false;
        r.transport_used = "api";
        r.action = "enroll";
        r.course_code = course_code;
        r.grupo = grupo;
        r.status_code = status;
        r.datic_code = "HTTP_ERROR";
        r.message = err;
        return r;
    }

    EnrollmentResult r = ParseMutationPayload(unwrapped, "enroll", course_code, grupo);
    r.status_code = status;
    return r;
}

EnrollmentResult MatriculaClient::DropCourse(const std::string& course_code, int grupo, bool dry_run) {
    if (dry_run) {
        EnrollmentResult r{};
        r.ok = true;
        r.dry_run = true;
        r.transport_used = "dry_run";
        r.action = "drop";
        r.course_code = course_code;
        r.grupo = grupo;
        r.datic_code = "DRY_RUN_OK";
        r.message = "[DRY-RUN] Simulacion de retiro sin envio real para " + course_code + " G" + std::to_string(grupo);
        return r;
    }

    if (!armed_.load(std::memory_order_acquire)) {
        EnrollmentResult r{};
        r.ok = false;
        r.dry_run = false;
        r.transport_used = "blocked";
        r.action = "drop";
        r.course_code = course_code;
        r.grupo = grupo;
        r.datic_code = "SAFETY_LOCK";
        r.message = "Bloqueado: candado de matricula real NO armado.";
        return r;
    }

    std::lock_guard<std::mutex> lk(mtx_);
    json payload = {
        { "codMateria", course_code },
        { "numGrupo", grupo }
    };
    json unwrapped;
    int status = 0;
    std::string err;
    if (!CallWebMethod("frmMatricula.aspx/EliminarMatricula", payload, unwrapped, &status, &err)) {
        EnrollmentResult r{};
        r.ok = false;
        r.dry_run = false;
        r.transport_used = "api";
        r.action = "drop";
        r.course_code = course_code;
        r.grupo = grupo;
        r.status_code = status;
        r.datic_code = "HTTP_ERROR";
        r.message = err;
        return r;
    }
    EnrollmentResult r = ParseMutationPayload(unwrapped, "drop", course_code, grupo);
    r.status_code = status;
    return r;
}

EnrollmentResult MatriculaClient::SendInformeEmail(const std::string& html_informe, bool dry_run) {
    if (dry_run) {
        EnrollmentResult r{};
        r.ok = true;
        r.dry_run = true;
        r.transport_used = "dry_run";
        r.action = "send_informe";
        r.datic_code = "DRY_RUN_OK";
        r.message = "[DRY-RUN] Mi Informe -> Enviar simulado (sin correo real).";
        return r;
    }

    if (!armed_.load(std::memory_order_acquire)) {
        EnrollmentResult r{};
        r.ok = false;
        r.dry_run = false;
        r.transport_used = "blocked";
        r.action = "send_informe";
        r.datic_code = "SAFETY_LOCK";
        r.message = "Bloqueado: EnviaInforme requiere candado armado (CONFIRMAR MATRICULA REAL).";
        return r;
    }

    std::lock_guard<std::mutex> lk(mtx_);
    std::string html_to_send = html_informe;
    if (html_to_send.empty()) {
        json gen_unwrapped;
        std::string gerr;
        if (CallWebMethod("frmInformeMatricula.aspx/GeneraInforme", json{ { "tipo", "E" } }, gen_unwrapped, nullptr, &gerr)) {
            html_to_send = ParseGeneraInformePayload(gen_unwrapped).html_informe;
        }
    }

    json payload = { { "htmlInforme", html_to_send } };
    json unwrapped;
    int status = 0;
    std::string err;
    if (!CallWebMethod("frmInformeMatricula.aspx/EnviaInforme", payload, unwrapped, &status, &err)) {
        EnrollmentResult r{};
        r.ok = false;
        r.dry_run = false;
        r.transport_used = "api";
        r.action = "send_informe";
        r.status_code = status;
        r.datic_code = "HTTP_ERROR";
        r.message = err;
        return r;
    }
    EnrollmentResult r = ParseMutationPayload(unwrapped, "send_informe", "", 0);
    r.status_code = status;
    return r;
}

} // namespace academic::matricula_tec
