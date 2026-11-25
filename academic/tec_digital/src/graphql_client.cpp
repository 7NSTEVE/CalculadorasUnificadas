#include "academic/tec_digital/graphql_client.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "http_common.hpp"
#include "session_http.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>

namespace academic::tec_digital {

namespace {

std::string JoinUrl(std::string_view base_url, std::string_view path) {
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();
    std::string p(path);
    if (p.empty() || p.front() != '/') p.insert(p.begin(), '/');
    return b + p;
}

std::string JsonStr(const nlohmann::json& j, std::initializer_list<const char*> keys) {
    if (!j.is_object()) return {};
    for (const char* k : keys) {
        if (!j.contains(k)) continue;
        const auto& v = j[k];
        if (v.is_string()) return v.get<std::string>();
        if (v.is_number_integer()) return std::to_string(v.get<int64_t>());
        if (v.is_number_unsigned()) return std::to_string(v.get<uint64_t>());
        if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    }
    return {};
}

bool JsonBool(const nlohmann::json& j, const char* key, bool fallback = false) {
    if (!j.is_object() || !j.contains(key)) return fallback;
    const auto& v = j[key];
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number_integer()) return v.get<int64_t>() != 0;
    if (v.is_string()) {
        std::string s = AsciiLower(v.get<std::string>());
        if (s == "true" || s == "1") return true;
        if (s == "false" || s == "0") return false;
    }
    return fallback;
}

int JsonInt(const nlohmann::json& j, std::initializer_list<const char*> keys, int fallback = 0) {
    if (!j.is_object()) return fallback;
    for (const char* k : keys) {
        if (!j.contains(k)) continue;
        const auto& v = j[k];
        if (v.is_number_integer()) return static_cast<int>(v.get<int64_t>());
        if (v.is_number_float()) return static_cast<int>(v.get<double>());
        if (v.is_string()) return std::atoi(v.get<std::string>().c_str());
    }
    return fallback;
}

double JsonDbl(const nlohmann::json& j, std::initializer_list<const char*> keys, double fallback = 0.0) {
    if (!j.is_object()) return fallback;
    for (const char* k : keys) {
        if (!j.contains(k)) continue;
        const auto& v = j[k];
        if (v.is_number()) return v.get<double>();
        if (v.is_string()) return std::atof(v.get<std::string>().c_str());
    }
    return fallback;
}

std::string ExtractUserIdFromJwtPayload(std::string_view jwt) {
    size_t d1 = jwt.find('.');
    if (d1 == std::string_view::npos) return {};
    size_t d2 = jwt.find('.', d1 + 1);
    if (d2 == std::string_view::npos || d2 <= d1 + 1) return {};
    std::string b64(jwt.substr(d1 + 1, d2 - d1 - 1));
    std::replace(b64.begin(), b64.end(), '-', '+');
    std::replace(b64.begin(), b64.end(), '_', '/');
    while (b64.size() % 4 != 0) b64.push_back('=');
    std::vector<uint8_t> raw;
    if (!Base64Decode(b64, raw)) return {};
    std::string payload(raw.begin(), raw.end());
    auto j = nlohmann::json::parse(payload, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return {};
    return JsonStr(j, {"userId", "user_id", "id"});
}

}  // namespace

TecGraphQlClient::TecGraphQlClient(PolicyEnforcingTransport& transport,
                                   AuthClient& auth,
                                   std::string base_url,
                                   std::string graphql_url,
                                   SanitizeOptions sanitize_opts)
    : transport_(transport),
      auth_(auth),
      base_url_(std::move(base_url)),
      graphql_url_(std::move(graphql_url)),
      sanitizer_(std::move(sanitize_opts)) {
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

TecGraphQlClient::~TecGraphQlClient() {
    ClearSessionToken();
}

void TecGraphQlClient::SetSanitizeOptions(SanitizeOptions sanitize_opts) {
    Sanitizer next(std::move(sanitize_opts));
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    sanitizer_ = std::move(next);
}

Sanitizer TecGraphQlClient::SnapshotSanitizer() const {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    return sanitizer_;
}

void TecGraphQlClient::ClearSessionToken() {
    std::lock_guard<std::mutex> lk(token_mutex_);
    SecureZeroString(cached_jwt_);
    SecureZeroString(cached_user_id_);
}

std::string TecGraphQlClient::CachedUserId() const {
    std::lock_guard<std::mutex> lk(token_mutex_);
    return cached_user_id_;
}

Result<std::string> TecGraphQlClient::EnsureTransactionToken(bool force_refresh) {
    if (!force_refresh) {
        std::lock_guard<std::mutex> lk(token_mutex_);
        if (!cached_jwt_.empty()) {
            return Result<std::string>::Success(cached_jwt_);
        }
    }

    HttpRequest req;
    req.method = "GET";
    req.url = JoinUrl(base_url_, "/tda-inclusion-manager/api/user-transaction");
    req.follow_redirects = true;
    req.headers["Accept"] = "application/json, text/plain, */*";
    req.headers["Referer"] = JoinUrl(base_url_, "/tda-inclusion-manager/");

    HttpResponse resp = detail::ExecuteSessionRequest(transport_, auth_, std::move(req));
    auto val = auth_.ValidateResponse(resp, req.url, true);
    if (val == ResponseValidity::SessionExpired) {
        return Result<std::string>::Failure("session_expired", "session expired fetching transaction key");
    }
    if (!resp.transport_ok || resp.status_code < 200 || resp.status_code >= 300) {
        return Result<std::string>::Failure(
            "service_unavailable",
            "Inclusiones no disponible (HTTP " + std::to_string(resp.status_code) + ")");
    }

    auto j = nlohmann::json::parse(resp.BodyAsString(), nullptr, false);
    if (j.is_discarded() || !j.is_object() ||
        !j.contains("transaction_key") || !j["transaction_key"].is_string()) {
        return Result<std::string>::Failure("service_unavailable", "Missing transaction_key");
    }
    std::string raw_jwt = TrimAscii(j["transaction_key"].get<std::string>());
    if (raw_jwt.empty()) {
        return Result<std::string>::Failure("service_unavailable", "Empty transaction_key");
    }

    std::string uid = ExtractUserIdFromJwtPayload(raw_jwt);
    {
        std::lock_guard<std::mutex> lk(token_mutex_);
        cached_jwt_ = raw_jwt;
        if (!uid.empty()) cached_user_id_ = uid;
    }
    return Result<std::string>::Success(std::move(raw_jwt));
}

Result<std::string> TecGraphQlClient::ExecuteQueryJson(std::string_view operation_name,
                                                       std::string_view query,
                                                       std::string_view variables_json) {
    nlohmann::json vars = nlohmann::json::object();
    if (!variables_json.empty()) {
        auto parsed_v = nlohmann::json::parse(variables_json, nullptr, false);
        if (!parsed_v.is_discarded() && parsed_v.is_object()) {
            vars = std::move(parsed_v);
        }
    }

    nlohmann::json payload = {
        {"operationName", std::string(operation_name)},
        {"variables", std::move(vars)},
        {"query", std::string(query)}
    };
    const std::string body = payload.dump();

    for (int attempt = 0; attempt < 2; ++attempt) {
        auto tok = EnsureTransactionToken(attempt > 0);
        if (!tok.ok) {
            return Result<std::string>::Failure(tok.error_code, tok.error_message);
        }

        HttpRequest req;
        req.method = "POST";
        req.url = graphql_url_;
        req.follow_redirects = false;
        req.body = body;
        req.headers["Content-Type"] = "application/json";
        req.headers["Accept"] = "application/json";
        // TEC GraphQL requires exact "token: <jwt>" header without "Bearer "
        req.headers["token"] = tok.value;

        HttpResponse resp = transport_.Execute(req);
        if (!resp.transport_ok) {
            return Result<std::string>::Failure(
                resp.error_message.rfind("read-only-request-guard:", 0) == 0 ? "policy_blocked" : "transport_error",
                SnapshotSanitizer().SanitizeText(resp.error_message));
        }

        const std::string resp_body = resp.BodyAsString();
        if (resp.status_code == 401 || resp.status_code == 403 ||
            (attempt == 0 && AsciiLower(resp_body).find("token") != std::string::npos &&
             AsciiLower(resp_body).find("errors") != std::string::npos)) {
            if (attempt == 0) {
                ClearSessionToken();
                continue;
            }
            return Result<std::string>::Failure("unauthorized", "GraphQL token rejected");
        }

        if (resp.status_code < 200 || resp.status_code >= 300) {
            return Result<std::string>::Failure(
                "http_" + std::to_string(resp.status_code),
                "HTTP " + std::to_string(resp.status_code) + " on GraphQL");
        }
        return Result<std::string>::Success(resp_body);
    }

    return Result<std::string>::Failure("unauthorized", "GraphQL request failed after token refresh");
}

Result<std::string> TecGraphQlClient::FetchUserType() {
    static constexpr const char* kQuery =
        "query getUserType { getUserType { user_role } }";
    auto r = ExecuteQueryJson("getUserType", kQuery, "{}");
    if (!r.ok) return Result<std::string>::Failure(r.error_code, r.error_message);
    auto j = nlohmann::json::parse(r.value, nullptr, false);
    if (j.is_discarded() || !j.contains("data") || !j["data"].contains("getUserType")) {
        return Result<std::string>::Failure("invalid_schema", "missing getUserType");
    }
    std::string role = JsonStr(j["data"]["getUserType"], {"user_role", "role"});
    return Result<std::string>::Success(SnapshotSanitizer().SanitizeText(role));
}

Result<InclusionCampaignInfo> TecGraphQlClient::FetchStudentActiveCampaign(
    std::string_view request_user_id) {
    std::string uid = TrimAscii(request_user_id);
    if (uid.empty()) uid = CachedUserId();
    if (uid.empty()) {
        auto tok = EnsureTransactionToken(false);
        if (!tok.ok) return Result<InclusionCampaignInfo>::Failure(tok.error_code, tok.error_message);
        uid = CachedUserId();
    }

    nlohmann::json vars = nlohmann::json::object();
    if (!uid.empty()) {
        vars["requestUserId"] = uid;
    }

    static constexpr const char* kQuery =
        "query GetStudentActiveCampaign($requestUserId: String) { "
        "GetStudentActiveCampaign(requestUserId: $requestUserId) { "
        "id name period start_date end_date student_entity active "
        "} }";
    auto r = ExecuteQueryJson("GetStudentActiveCampaign", kQuery, vars.dump());
    if (!r.ok) return Result<InclusionCampaignInfo>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    if (j.is_discarded() || !j.contains("data") || !j["data"].contains("GetStudentActiveCampaign")) {
        return Result<InclusionCampaignInfo>::Failure("invalid_schema", "missing GetStudentActiveCampaign");
    }
    const auto& node = j["data"]["GetStudentActiveCampaign"];
    const nlohmann::json* obj = nullptr;
    if (node.is_object()) obj = &node;
    else if (node.is_array() && !node.empty() && node[0].is_object()) obj = &node[0];
    if (obj == nullptr) {
        return Result<InclusionCampaignInfo>::Failure("not_found", "no active campaign");
    }

    const Sanitizer san = SnapshotSanitizer();
    InclusionCampaignInfo info;
    info.id = san.SanitizeText(JsonStr(*obj, {"id", "campaign_id"}));
    info.name = san.SanitizeText(JsonStr(*obj, {"name", "title"}));
    info.period = san.SanitizeText(JsonStr(*obj, {"period", "periodo"}));
    info.start_date = san.SanitizeText(JsonStr(*obj, {"start_date", "startDate", "student_start_date"}));
    info.end_date = san.SanitizeText(JsonStr(*obj, {"end_date", "endDate", "student_end_date"}));
    info.student_entity = san.SanitizeText(JsonStr(*obj, {"student_entity", "entity", "career_name"}));
    info.active = JsonBool(*obj, "active", !info.id.empty());
    return Result<InclusionCampaignInfo>::Success(std::move(info));
}

Result<std::vector<PendingGradCourse>> TecGraphQlClient::FetchPendingGraduationCourses(
    std::string_view user_id,
    std::string_view carne) {
    std::string uid = TrimAscii(user_id);
    if (uid.empty()) uid = CachedUserId();

    nlohmann::json vars = {
        {"userId", uid},
        {"carnet", std::string(carne)}
    };
    static constexpr const char* kQuery =
        "query getStudentPendingGraduatedCourses($userId: String, $carnet: String) { "
        "getStudentPendingGraduatedCourses(userId: $userId, carnet: $carnet) { "
        "id code name credits "
        "} }";
    auto r = ExecuteQueryJson("getStudentPendingGraduatedCourses", kQuery, vars.dump());
    if (!r.ok) return Result<std::vector<PendingGradCourse>>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    if (j.is_discarded() || !j.contains("data") ||
        !j["data"].contains("getStudentPendingGraduatedCourses") ||
        !j["data"]["getStudentPendingGraduatedCourses"].is_array()) {
        return Result<std::vector<PendingGradCourse>>::Failure(
            "invalid_schema", "missing getStudentPendingGraduatedCourses array");
    }

    const Sanitizer san = SnapshotSanitizer();
    std::vector<PendingGradCourse> out;
    for (const auto& item : j["data"]["getStudentPendingGraduatedCourses"]) {
        if (!item.is_object()) continue;
        PendingGradCourse c;
        c.id = san.SanitizeText(JsonStr(item, {"id"}));
        c.code = san.SanitizeText(JsonStr(item, {"code", "cod_asignatura", "id"}));
        c.name = san.SanitizeText(JsonStr(item, {"name", "materia"}));
        c.credits = JsonInt(item, {"credits", "creditos"});
        if (!c.code.empty() || !c.name.empty()) {
            out.push_back(std::move(c));
        }
    }
    return Result<std::vector<PendingGradCourse>>::Success(std::move(out));
}

Result<CourseRequisitesCheck> TecGraphQlClient::CheckCourseRequisites(
    std::string_view user_id,
    std::string_view carne,
    std::string_view campaign_id,
    std::string_view course_id,
    std::string_view cod_asignatura,
    std::string_view period) {
    std::string uid = TrimAscii(user_id);
    if (uid.empty()) uid = CachedUserId();

    nlohmann::json vars = {
        {"userId", uid},
        {"carnet", std::string(carne)},
        {"campaignId", std::string(campaign_id)},
        {"courseId", std::string(course_id)},
        {"codAsignatura", std::string(cod_asignatura)},
        {"period", std::string(period)}
    };
    static constexpr const char* kQuery =
        "query getStudentRequisitsCorrequisitsByCourse("
        "$userId: String, $carnet: String, $campaignId: String, "
        "$courseId: String, $codAsignatura: String, $period: String) { "
        "getStudentRequisitsCorrequisitsByCourse("
        "userId: $userId, carnet: $carnet, campaignId: $campaignId, "
        "courseId: $courseId, codAsignatura: $codAsignatura, period: $period) { "
        "allRequirementsMet allCoRequirementsMet hasOverlap missingRequirements missingCoRequirements message "
        "} }";
    auto r = ExecuteQueryJson("getStudentRequisitsCorrequisitsByCourse", kQuery, vars.dump());
    if (!r.ok) return Result<CourseRequisitesCheck>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    if (j.is_discarded() || !j.contains("data") ||
        !j["data"].contains("getStudentRequisitsCorrequisitsByCourse") ||
        !j["data"]["getStudentRequisitsCorrequisitsByCourse"].is_object()) {
        return Result<CourseRequisitesCheck>::Failure(
            "invalid_schema", "missing getStudentRequisitsCorrequisitsByCourse");
    }

    const auto& obj = j["data"]["getStudentRequisitsCorrequisitsByCourse"];
    const Sanitizer san = SnapshotSanitizer();
    CourseRequisitesCheck chk;
    chk.course_code = san.SanitizeText(std::string(cod_asignatura));
    chk.all_requirements_met = JsonBool(obj, "allRequirementsMet", true);
    chk.all_corequirements_met = JsonBool(obj, "allCoRequirementsMet", true);
    chk.has_overlap = JsonBool(obj, "hasOverlap", false);
    chk.message = san.SanitizeText(JsonStr(obj, {"message", "detail"}));

    auto pull_list = [&](const char* k, std::vector<std::string>& dst) {
        if (!obj.contains(k) || !obj[k].is_array()) return;
        for (const auto& elem : obj[k]) {
            if (elem.is_string()) dst.push_back(san.SanitizeText(elem.get<std::string>()));
            else if (elem.is_object()) {
                std::string s = JsonStr(elem, {"code", "id", "name"});
                if (!s.empty()) dst.push_back(san.SanitizeText(s));
            }
        }
    };
    pull_list("missingRequirements", chk.missing_requirements);
    pull_list("missingCoRequirements", chk.missing_corequirements);
    return Result<CourseRequisitesCheck>::Success(std::move(chk));
}

Result<std::vector<InclusionApplicationRecord>> TecGraphQlClient::FetchStudentApplications(
    std::string_view campaign_id,
    std::string_view carne) {
    nlohmann::json vars = {
        {"campaignId", std::string(campaign_id)},
        {"carnet", std::string(carne)}
    };
    static constexpr const char* kQuery =
        "query GetAllStudentApplications($campaignId: String, $carnet: String) { "
        "GetAllStudentApplications(campaignId: $campaignId, carnet: $carnet) { "
        "id course_code course_name group status comment updated_at "
        "} }";
    auto r = ExecuteQueryJson("GetAllStudentApplications", kQuery, vars.dump());
    if (!r.ok) return Result<std::vector<InclusionApplicationRecord>>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    if (j.is_discarded() || !j.contains("data") ||
        !j["data"].contains("GetAllStudentApplications") ||
        !j["data"]["GetAllStudentApplications"].is_array()) {
        return Result<std::vector<InclusionApplicationRecord>>::Failure(
            "invalid_schema", "missing GetAllStudentApplications array");
    }

    const Sanitizer san = SnapshotSanitizer();
    std::vector<InclusionApplicationRecord> out;
    for (const auto& item : j["data"]["GetAllStudentApplications"]) {
        if (!item.is_object()) continue;
        InclusionApplicationRecord rec;
        rec.id = san.SanitizeText(JsonStr(item, {"id", "application_id"}));
        rec.course_code = san.SanitizeText(JsonStr(item, {"course_code", "cod_asignatura", "code"}));
        rec.course_name = san.SanitizeText(JsonStr(item, {"course_name", "materia", "name"}));
        rec.group = san.SanitizeText(JsonStr(item, {"group", "grupo"}));
        rec.status = san.SanitizeText(JsonStr(item, {"status", "estado"}));
        rec.comment = san.SanitizeText(JsonStr(item, {"comment", "observation", "comentario"}));
        rec.updated_at = san.SanitizeText(JsonStr(item, {"updated_at", "created_at"}));
        out.push_back(std::move(rec));
    }
    return Result<std::vector<InclusionApplicationRecord>>::Success(std::move(out));
}

Result<ScholarshipStatusInfo> TecGraphQlClient::FetchScholarshipStatus(std::string_view campaign_id) {
    nlohmann::json vars = {{"campaignId", std::string(campaign_id)}};
    static constexpr const char* kQuery =
        "query getScholarship($campaignId: String) { "
        "getScholarship(campaignId: $campaignId) { "
        "campaign_id status scholarship_type detail "
        "} }";
    auto r = ExecuteQueryJson("getScholarship", kQuery, vars.dump());
    if (!r.ok) return Result<ScholarshipStatusInfo>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    if (j.is_discarded() || !j.contains("data") || !j["data"].contains("getScholarship")) {
        return Result<ScholarshipStatusInfo>::Failure("invalid_schema", "missing getScholarship");
    }
    const auto& node = j["data"]["getScholarship"];
    const nlohmann::json* obj = node.is_object() ? &node
                                                : (node.is_array() && !node.empty() && node[0].is_object()
                                                       ? &node[0]
                                                       : nullptr);
    if (!obj) {
        return Result<ScholarshipStatusInfo>::Failure("not_found", "no scholarship data");
    }

    const Sanitizer san = SnapshotSanitizer();
    ScholarshipStatusInfo s;
    s.campaign_id = san.SanitizeText(JsonStr(*obj, {"campaign_id", "campaignId"}));
    if (s.campaign_id.empty()) s.campaign_id = std::string(campaign_id);
    s.status = san.SanitizeText(JsonStr(*obj, {"status", "estado"}));
    s.scholarship_type = san.SanitizeText(JsonStr(*obj, {"scholarship_type", "type", "tipo"}));
    s.detail = san.SanitizeText(JsonStr(*obj, {"detail", "description", "observacion"}));
    return Result<ScholarshipStatusInfo>::Success(std::move(s));
}

Result<std::vector<GraphQlActualCourse>> TecGraphQlClient::FetchActualPeriodCourses(
    std::string_view carne) {
    nlohmann::json vars = {{"carnet", std::string(carne)}};
    static constexpr const char* kQuery =
        "query getStudentActualPeriod($carnet: String) { "
        "getStudentActualPeriod(carnet: $carnet) { "
        "id code materia credits professors schedule "
        "} }";
    auto r = ExecuteQueryJson("getStudentActualPeriod", kQuery, vars.dump());
    if (!r.ok) return Result<std::vector<GraphQlActualCourse>>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    if (j.is_discarded() || !j.contains("data") ||
        !j["data"].contains("getStudentActualPeriod") ||
        !j["data"]["getStudentActualPeriod"].is_array()) {
        return Result<std::vector<GraphQlActualCourse>>::Failure(
            "invalid_schema", "missing getStudentActualPeriod array");
    }

    const Sanitizer san = SnapshotSanitizer();
    std::vector<GraphQlActualCourse> out;
    for (const auto& item : j["data"]["getStudentActualPeriod"]) {
        if (!item.is_object()) continue;
        GraphQlActualCourse c;
        c.id = san.SanitizeText(JsonStr(item, {"id"}));
        c.code = san.SanitizeText(JsonStr(item, {"code", "cod_asignatura", "id"}));
        c.materia = san.SanitizeText(JsonStr(item, {"materia", "name"}));
        c.credits = JsonInt(item, {"credits", "creditos"});
        if (item.contains("professors") && item["professors"].is_array()) {
            for (const auto& p : item["professors"]) {
                if (p.is_string()) c.professors.push_back(san.SanitizeText(p.get<std::string>()));
            }
        }
        if (item.contains("schedule") && item["schedule"].is_array()) {
            for (const auto& sc : item["schedule"]) {
                c.schedule.push_back(san.SanitizeText(sc.is_string() ? sc.get<std::string>() : sc.dump()));
            }
        }
        out.push_back(std::move(c));
    }
    return Result<std::vector<GraphQlActualCourse>>::Success(std::move(out));
}

Result<AcademicStatsSummary> TecGraphQlClient::FetchAcademicHistoryFallback(std::string_view carne) {
    nlohmann::json vars = {{"carnet", std::string(carne)}};
    static constexpr const char* kQuery =
        "query getStudentAcademicHistory($carnet: String) { "
        "getStudentAcademicHistory(carnet: $carnet) { "
        "statistics { pPonderado tHoras numCursosA numCreditosA numPromedioA numCursosR numCreditosR } "
        "} }";
    auto r = ExecuteQueryJson("getStudentAcademicHistory", kQuery, vars.dump());
    if (!r.ok) return Result<AcademicStatsSummary>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    if (j.is_discarded() || !j.contains("data") ||
        !j["data"].contains("getStudentAcademicHistory") ||
        !j["data"]["getStudentAcademicHistory"].is_object()) {
        return Result<AcademicStatsSummary>::Failure(
            "invalid_schema", "missing getStudentAcademicHistory");
    }

    const auto& hist = j["data"]["getStudentAcademicHistory"];
    AcademicStatsSummary st;
    if (hist.contains("statistics") && hist["statistics"].is_object()) {
        const auto& s = hist["statistics"];
        st.weighted_avg_general = JsonDbl(s, {"pPonderado", "weighted_avg"});
        st.weighted_avg_passed = JsonDbl(s, {"numPromedioA"});
        st.weighted_avg_failed = JsonDbl(s, {"numPromedioR"});
        st.hours_taken = JsonInt(s, {"tHoras"});
        st.count_aprobadas = JsonInt(s, {"numCursosA"});
        st.credits_passed = JsonInt(s, {"numCreditosA"});
        st.count_reprobadas = JsonInt(s, {"numCursosR"});
        st.credits_failed = JsonInt(s, {"numCreditosR"});
        st.credits_total = st.credits_passed + st.credits_failed;
    }
    return Result<AcademicStatsSummary>::Success(st);
}

}  // namespace academic::tec_digital
