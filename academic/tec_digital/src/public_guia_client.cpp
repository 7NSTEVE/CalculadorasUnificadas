#include "academic/tec_digital/public_guia_client.hpp"

#include <nlohmann/json.hpp>

namespace academic::tec_digital {

namespace {

std::string JoinGuiaUrl(std::string_view base_url, std::string_view rel_path) {
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();
    std::string p(rel_path);
    if (p.empty() || p.front() != '/') p.insert(p.begin(), '/');
    return b + p;
}

}  // namespace

PublicGuiaHorariosClient::PublicGuiaHorariosClient(
    IHttpTransport& transport,
    std::string base_url,
    SanitizeOptions sanitize_opts)
    : transport_(transport),
      base_url_(base_url.empty() ? "https://tec-appsext.itcr.ac.cr/guiahorarios" : std::move(base_url)),
      sanitizer_(std::move(sanitize_opts)) {
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

void PublicGuiaHorariosClient::SetSanitizeOptions(SanitizeOptions sanitize_opts) {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    sanitizer_ = Sanitizer(std::move(sanitize_opts));
}

Sanitizer PublicGuiaHorariosClient::SnapshotSanitizer() const {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    return sanitizer_;
}

Result<std::string> PublicGuiaHorariosClient::PostWebMethodJson(
    std::string_view rel_path,
    const std::string& json_body) {
    HttpRequest req;
    req.method = "POST";
    req.url = JoinGuiaUrl(base_url_, rel_path);
    req.follow_redirects = false;
    req.body = json_body.empty() ? "{}" : json_body;
    req.headers["Content-Type"] = "application/json; charset=utf-8";
    req.headers["Accept"] = "application/json, text/javascript, */*";
    req.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) CuadernU/2.0";

    HttpResponse resp = transport_.Execute(req);
    const Sanitizer san = SnapshotSanitizer();
    if (!resp.transport_ok) {
        return Result<std::string>::Failure(
            resp.error_message.rfind("read-only-request-guard:", 0) == 0 ? "policy_blocked" : "transport_error",
            san.SanitizeText(resp.error_message));
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        return Result<std::string>::Failure(
            "http_error",
            "Guía de Horarios Pública devolvió HTTP " + std::to_string(resp.status_code));
    }
    return Result<std::string>::Success(resp.BodyAsString());
}

void PublicGuiaHorariosClient::SanitizeRows(
    std::vector<PublicGuiaSessionRow>& rows,
    const Sanitizer& san) {
    for (auto& r : rows) {
        r.dsc_sede = san.SanitizeText(r.dsc_sede);
        r.ide_materia = san.SanitizeText(r.ide_materia);
        r.dsc_materia = san.SanitizeText(r.dsc_materia);
        r.ide_grupo = san.SanitizeText(r.ide_grupo);
        r.dsc_depto = san.SanitizeText(r.dsc_depto);
        r.nom_dia = san.SanitizeText(r.nom_dia);
        r.ide_modalidad = san.SanitizeText(r.ide_modalidad);
        r.ide_per_mod = san.SanitizeText(r.ide_per_mod);
        r.dsc_modalidad = san.SanitizeText(r.dsc_modalidad);
        r.tipo_curso = san.SanitizeText(r.tipo_curso);
        r.hinicio = san.SanitizeText(r.hinicio);
        r.hfin = san.SanitizeText(r.hfin);
        r.nom_profesor = san.SanitizeText(r.nom_profesor);
        r.reserva_sede = san.SanitizeText(r.reserva_sede);
        r.reserva_depto = san.SanitizeText(r.reserva_depto);
        r.reserva_plan = san.SanitizeText(r.reserva_plan);
        r.ide_est_est = san.SanitizeText(r.ide_est_est);
        r.dsc_plan = san.SanitizeText(r.dsc_plan);
        r.num_nivel = san.SanitizeText(r.num_nivel);
    }
}

Result<std::vector<PublicGuiaEscuelaInfo>> PublicGuiaHorariosClient::FetchEscuelas() {
    auto r = PostWebMethodJson("/escuela.aspx/cargaEscuelas", "{}");
    if (!r.ok) return Result<std::vector<PublicGuiaEscuelaInfo>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaEscuelasResponse(r.value);
    if (!parsed.ok) return parsed;
    const Sanitizer san = SnapshotSanitizer();
    for (auto& e : parsed.value) {
        e.ide_depto = san.SanitizeText(e.ide_depto);
        e.dsc_depto = san.SanitizeText(e.dsc_depto);
    }
    return parsed;
}

Result<std::vector<PublicGuiaModalidadInfo>> PublicGuiaHorariosClient::FetchModalidades() {
    auto r = PostWebMethodJson("/escuela.aspx/cargaModalidadPeriodos", "{}");
    if (!r.ok) return Result<std::vector<PublicGuiaModalidadInfo>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaModalidadesResponse(r.value);
    if (!parsed.ok) return parsed;
    const Sanitizer san = SnapshotSanitizer();
    for (auto& m : parsed.value) {
        m.ide_modalidad = san.SanitizeText(m.ide_modalidad);
        m.nombre = san.SanitizeText(m.nombre);
    }
    return parsed;
}

Result<std::vector<PublicGuiaSessionRow>> PublicGuiaHorariosClient::FetchEscuelaByYear(
    std::string_view ide_depto,
    int year) {
    const std::string esc = TrimAscii(ide_depto);
    if (esc.empty()) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Código de escuela vacío");
    }
    if (year < 1990 || year > 2100) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Año fuera de rango");
    }
    nlohmann::json payload = {
        {"escuela", esc},
        {"ano", std::to_string(year)}
    };
    auto r = PostWebMethodJson("/escuela.aspx/getdatosEscuelaAno", payload.dump());
    if (!r.ok) return Result<std::vector<PublicGuiaSessionRow>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaRowsResponse(r.value);
    if (!parsed.ok) return parsed;
    SanitizeRows(parsed.value, SnapshotSanitizer());
    return parsed;
}

Result<std::vector<PublicGuiaSessionRow>> PublicGuiaHorariosClient::FetchVeranoByYear(int year) {
    if (year < 1990 || year > 2100) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Año fuera de rango");
    }
    nlohmann::json payload = {
        {"ano", std::to_string(year)}
    };
    auto r = PostWebMethodJson("/verano.aspx/getdatosVerano", payload.dump());
    if (!r.ok) return Result<std::vector<PublicGuiaSessionRow>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaRowsResponse(r.value);
    if (!parsed.ok) return parsed;
    SanitizeRows(parsed.value, SnapshotSanitizer());
    return parsed;
}

Result<std::vector<PublicGuiaSessionRow>> PublicGuiaHorariosClient::FetchEstudianteByCarnet(
    std::string_view carnet,
    int year) {
    const std::string c = TrimAscii(carnet);
    if (c.empty()) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure("invalid_arg", "Número de carné requerido");
    }
    nlohmann::json payload;
    std::string rel_path;
    if (year >= 1990 && year <= 2100) {
        payload = {{"carnet", c}, {"ano", std::to_string(year)}};
        rel_path = "/estudiante.aspx/getdatosAno";
    } else {
        payload = {{"carnet", c}};
        rel_path = "/estudiante.aspx/getdatos";
    }
    auto r = PostWebMethodJson(rel_path, payload.dump());
    if (!r.ok) return Result<std::vector<PublicGuiaSessionRow>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePublicGuiaRowsResponse(r.value);
    if (!parsed.ok) return parsed;
    SanitizeRows(parsed.value, SnapshotSanitizer());
    return parsed;
}

}  // namespace academic::tec_digital
