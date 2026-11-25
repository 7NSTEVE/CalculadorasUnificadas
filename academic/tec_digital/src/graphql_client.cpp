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

nlohmann::json ToIntOrStr(std::string_view raw) {
    std::string s = TrimAscii(raw);
    if (!s.empty()) {
        bool all_digits = true;
        for (char c : s) {
            if (c < '0' || c > '9') { all_digits = false; break; }
        }
        if (all_digits && s.size() <= 18) {
            try {
                return static_cast<int64_t>(std::stoll(s));
            } catch (...) {}
        }
    }
    return s;
}

const nlohmann::json* FindDataField(const nlohmann::json& j, std::initializer_list<const char*> names) {
    if (!j.is_object() || !j.contains("data") || !j["data"].is_object()) return nullptr;
    const auto& d = j["data"];
    for (const char* n : names) {
        if (d.contains(n)) return &d[n];
    }
    return nullptr;
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
        vars["requestUserId"] = ToIntOrStr(uid);
    }

    static constexpr const char* kQuery =
        "query GetStudentActiveCampaign($requestUserId: Int) { "
        "getStudentActiveCampaign(requestUserId: $requestUserId) { "
        "id name period start_date end_date student_start_date student_end_date student_entity active "
        "} }";
    auto r = ExecuteQueryJson("GetStudentActiveCampaign", kQuery, vars.dump());
    if (!r.ok) return Result<InclusionCampaignInfo>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* node_ptr = FindDataField(j, {"getStudentActiveCampaign", "GetStudentActiveCampaign"});
    if (j.is_discarded() || !node_ptr) {
        return Result<InclusionCampaignInfo>::Failure("invalid_schema", "missing getStudentActiveCampaign");
    }
    const auto& node = *node_ptr;
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
        {"userId", ToIntOrStr(uid)},
        {"carnet", ToIntOrStr(carne)}
    };
    static constexpr const char* kQuery =
        "query getStudentPendingGraduatedCourses($userId: Int!, $carnet: Int!) { "
        "getStudentPendingGraduatedCourses(userId: $userId, carnet: $carnet) { "
        "id name credits "
        "} }";
    auto r = ExecuteQueryJson("getStudentPendingGraduatedCourses", kQuery, vars.dump());
    if (!r.ok) return Result<std::vector<PendingGradCourse>>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* arr_ptr = FindDataField(j, {"getStudentPendingGraduatedCourses"});
    if (j.is_discarded() || !arr_ptr || !arr_ptr->is_array()) {
        return Result<std::vector<PendingGradCourse>>::Failure(
            "invalid_schema", "missing getStudentPendingGraduatedCourses array");
    }

    const Sanitizer san = SnapshotSanitizer();
    std::vector<PendingGradCourse> out;
    for (const auto& item : *arr_ptr) {
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
        {"userId", ToIntOrStr(uid)},
        {"carnet", ToIntOrStr(carne)},
        {"campaignId", ToIntOrStr(campaign_id)},
        {"courseId", ToIntOrStr(course_id)},
        {"codAsignatura", std::string(cod_asignatura)},
        {"period", std::string(period)}
    };
    static constexpr const char* kQuery =
        "query getStudentRequisitsCorrequisitsByCourse("
        "$userId: Int!, $carnet: Int!, $campaignId: Int!, "
        "$courseId: Int!, $codAsignatura: String!, $period: String!) { "
        "getStudentRequisitsCorrequisitsByCourse("
        "userId: $userId, carnet: $carnet, campaignId: $campaignId, "
        "courseId: $courseId, codAsignatura: $codAsignatura, period: $period) { "
        "allRequirementsMet allCoRequirementsMet hasOverlap missingRequirements missingCoRequirements message "
        "} }";
    auto r = ExecuteQueryJson("getStudentRequisitsCorrequisitsByCourse", kQuery, vars.dump());
    if (!r.ok) return Result<CourseRequisitesCheck>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* obj_ptr = FindDataField(j, {"getStudentRequisitsCorrequisitsByCourse"});
    if (j.is_discarded() || !obj_ptr || !obj_ptr->is_object()) {
        return Result<CourseRequisitesCheck>::Failure(
            "invalid_schema", "missing getStudentRequisitsCorrequisitsByCourse");
    }

    const auto& obj = *obj_ptr;
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
        {"campaignId", ToIntOrStr(campaign_id)},
        {"carnet", ToIntOrStr(carne)}
    };
    static constexpr const char* kQuery =
        "query GetAllStudentApplications($campaignId: Int!, $carnet: Int!) { "
        "getAllStudentApplications(campaignId: $campaignId, carnet: $carnet) { "
        "id course_code course_name group status comment updated_at "
        "} }";
    auto r = ExecuteQueryJson("GetAllStudentApplications", kQuery, vars.dump());
    if (!r.ok) return Result<std::vector<InclusionApplicationRecord>>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* arr_ptr = FindDataField(j, {"getAllStudentApplications", "GetAllStudentApplications"});
    if (j.is_discarded() || !arr_ptr || !arr_ptr->is_array()) {
        return Result<std::vector<InclusionApplicationRecord>>::Failure(
            "invalid_schema", "missing getAllStudentApplications array");
    }

    const Sanitizer san = SnapshotSanitizer();
    std::vector<InclusionApplicationRecord> out;
    for (const auto& item : *arr_ptr) {
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
    nlohmann::json vars = {{"campaignId", ToIntOrStr(campaign_id)}};
    static constexpr const char* kQuery =
        "query getScholarship($campaignId: Int!) { "
        "getScholarship(campaignId: $campaignId) { "
        "campaign_id status scholarship_type detail "
        "} }";
    auto r = ExecuteQueryJson("getScholarship", kQuery, vars.dump());
    if (!r.ok) return Result<ScholarshipStatusInfo>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* node_ptr = FindDataField(j, {"getScholarship"});
    if (j.is_discarded() || !node_ptr) {
        return Result<ScholarshipStatusInfo>::Failure("invalid_schema", "missing getScholarship");
    }
    const auto& node = *node_ptr;
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
    nlohmann::json vars = {{"carnet", ToIntOrStr(carne)}};
    static constexpr const char* kQuery =
        "query getStudentActualPeriod($carnet: Int!) { "
        "getStudentActualPeriod(carnet: $carnet) { "
        "id materia credits professors schedule "
        "} }";
    auto r = ExecuteQueryJson("getStudentActualPeriod", kQuery, vars.dump());
    if (!r.ok) return Result<std::vector<GraphQlActualCourse>>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* arr_ptr = FindDataField(j, {"getStudentActualPeriod"});
    if (j.is_discarded() || !arr_ptr || !arr_ptr->is_array()) {
        return Result<std::vector<GraphQlActualCourse>>::Failure(
            "invalid_schema", "missing getStudentActualPeriod array");
    }

    const Sanitizer san = SnapshotSanitizer();
    std::vector<GraphQlActualCourse> out;
    for (const auto& item : *arr_ptr) {
        if (!item.is_object()) continue;
        GraphQlActualCourse c;
        c.id = san.SanitizeText(JsonStr(item, {"id"}));
        c.code = san.SanitizeText(JsonStr(item, {"code", "cod_asignatura", "id"}));
        c.materia = san.SanitizeText(JsonStr(item, {"materia", "name"}));
        c.credits = JsonInt(item, {"credits", "creditos"});
        if (item.contains("professors")) {
            if (item["professors"].is_array()) {
                for (const auto& p : item["professors"]) {
                    if (p.is_string()) c.professors.push_back(san.SanitizeText(p.get<std::string>()));
                }
            } else if (item["professors"].is_string()) {
                std::string ps = TrimAscii(item["professors"].get<std::string>());
                if (!ps.empty()) c.professors.push_back(san.SanitizeText(ps));
            }
        }
        if (item.contains("schedule")) {
            if (item["schedule"].is_array()) {
                for (const auto& sc : item["schedule"]) {
                    c.schedule.push_back(san.SanitizeText(sc.is_string() ? sc.get<std::string>() : sc.dump()));
                }
            } else if (item["schedule"].is_string()) {
                std::string ss = TrimAscii(item["schedule"].get<std::string>());
                if (!ss.empty()) c.schedule.push_back(san.SanitizeText(ss));
            }
        }
        out.push_back(std::move(c));
    }
    return Result<std::vector<GraphQlActualCourse>>::Success(std::move(out));
}

Result<AcademicStatsSummary> TecGraphQlClient::FetchAcademicHistoryFallback(std::string_view carne) {
    nlohmann::json vars = {{"carnet", ToIntOrStr(carne)}};
    static constexpr const char* kQuery =
        "query getStudentAcademicHistory($carnet: Int!) { "
        "getStudentAcademicHistory(carnet: $carnet) { "
        "statistics { pPonderado tHoras numCursosA numCreditosA numPromedioA numCursosR numCreditosR numPromedioR "
        "mCursadas mMatriculadas mRetiradas mReconocidas mSuficiencia mPendientes mCongelados mConsultadas "
        "tCreditos tCreditosSinNota porcentajeAprobado porcentajeReprobado } "
        "} }";
    auto r = ExecuteQueryJson("getStudentAcademicHistory", kQuery, vars.dump());
    if (!r.ok) return Result<AcademicStatsSummary>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* hist_ptr = FindDataField(j, {"getStudentAcademicHistory"});
    if (j.is_discarded() || !hist_ptr || !hist_ptr->is_object()) {
        return Result<AcademicStatsSummary>::Failure(
            "invalid_schema", "missing getStudentAcademicHistory");
    }

    const auto& hist = *hist_ptr;
    AcademicStatsSummary st;
    if (hist.contains("statistics") && hist["statistics"].is_object()) {
        const auto& s = hist["statistics"];
        st.weighted_avg_general = JsonDbl(s, {"pPonderado", "weighted_avg"});
        st.weighted_avg_passed = JsonDbl(s, {"numPromedioA"});
        st.weighted_avg_failed = JsonDbl(s, {"numPromedioR"});
        st.passed_pct = JsonDbl(s, {"porcentajeAprobado"});
        st.failed_pct = JsonDbl(s, {"porcentajeReprobado"});
        st.hours_taken = JsonInt(s, {"tHoras"});
        st.count_aprobadas = JsonInt(s, {"numCursosA"});
        st.credits_passed = JsonInt(s, {"numCreditosA"});
        st.count_reprobadas = JsonInt(s, {"numCursosR"});
        st.credits_failed = JsonInt(s, {"numCreditosR"});
        st.credits_total = JsonInt(s, {"tCreditos"}, st.credits_passed + st.credits_failed);
        st.credits_no_grade = JsonInt(s, {"tCreditosSinNota"});
        st.subjects_taken = JsonInt(s, {"mCursadas"});
        st.subjects_consulted = JsonInt(s, {"mConsultadas"});
        st.count_en_curso = JsonInt(s, {"mMatriculadas"});
        st.count_retiradas = JsonInt(s, {"mRetiradas"});
        st.count_reconocidas = JsonInt(s, {"mReconocidas"});
        st.count_suficiencia = JsonInt(s, {"mSuficiencia"});
        st.count_pendientes = JsonInt(s, {"mPendientes"});
        st.count_congeladas = JsonInt(s, {"mCongelados"});
    }
    return Result<AcademicStatsSummary>::Success(st);
}

Result<StudentGeneralInfo> TecGraphQlClient::FetchStudentGeneralInfo(std::string_view carne) {
    nlohmann::json vars = {{"carnet", ToIntOrStr(carne)}};
    static constexpr const char* kQuery =
        "query getStudentGeneralInfo($carnet: Int!) { "
        "getStudentGeneralInfo(carnet: $carnet) { "
        "carreer weightedAverage outstandingCourses availableRegistrationCredits "
        "rn { ide_materia rn course_name } "
        "} }";
    auto r = ExecuteQueryJson("getStudentGeneralInfo", kQuery, vars.dump());
    if (!r.ok) return Result<StudentGeneralInfo>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* obj_ptr = FindDataField(j, {"getStudentGeneralInfo"});
    if (j.is_discarded() || !obj_ptr || !obj_ptr->is_object()) {
        return Result<StudentGeneralInfo>::Failure("invalid_schema", "missing getStudentGeneralInfo");
    }

    const auto& obj = *obj_ptr;
    const Sanitizer san = SnapshotSanitizer();
    StudentGeneralInfo info;
    info.career = san.SanitizeText(JsonStr(obj, {"carreer", "career"}));
    info.weighted_average = JsonDbl(obj, {"weightedAverage", "weighted_average"});
    info.outstanding_courses = JsonInt(obj, {"outstandingCourses", "outstanding_courses"});
    info.available_registration_credits = JsonInt(obj, {"availableRegistrationCredits", "available_registration_credits"});
    if (obj.contains("rn") && obj["rn"].is_array()) {
        for (const auto& rni : obj["rn"]) {
            if (!rni.is_object()) continue;
            StudentRnCourse rc;
            rc.course_code = san.SanitizeText(JsonStr(rni, {"ide_materia", "code"}));
            rc.rn = JsonInt(rni, {"rn"});
            rc.course_name = san.SanitizeText(JsonStr(rni, {"course_name", "name"}));
            if (!rc.course_code.empty() || !rc.course_name.empty()) {
                info.rn_courses.push_back(std::move(rc));
            }
        }
    }
    return Result<StudentGeneralInfo>::Success(std::move(info));
}

Result<GraphQlCurriculumBundle> TecGraphQlClient::FetchStudentCurriculum(std::string_view carne) {
    nlohmann::json vars = {{"carnet", ToIntOrStr(carne)}};
    static constexpr const char* kQuery =
        "query getStudentCurriculum($carnet: Int!) { "
        "getStudentCurriculum(carnet: $carnet) { "
        "id_plan dsc_plan "
        "curriculum { "
        "id courses { "
        "id_course name trucatedName credits hours "
        "requirements { id } co_requirements { id } required_by { id } equivalent { id } tEquiv "
        "best_status grades_record { year period modality state grade group } "
        "} } } }";
    auto r = ExecuteQueryJson("getStudentCurriculum", kQuery, vars.dump());
    if (!r.ok) return Result<GraphQlCurriculumBundle>::Failure(r.error_code, r.error_message);

    auto j = nlohmann::json::parse(r.value, nullptr, false);
    const nlohmann::json* obj_ptr = FindDataField(j, {"getStudentCurriculum"});
    if (j.is_discarded() || !obj_ptr || !obj_ptr->is_object()) {
        return Result<GraphQlCurriculumBundle>::Failure("invalid_schema", "missing getStudentCurriculum");
    }

    const auto& obj = *obj_ptr;
    const Sanitizer san = SnapshotSanitizer();
    GraphQlCurriculumBundle bundle;
    bundle.graph.id_curriculum = JsonInt(obj, {"id_plan", "id_curriculum"});
    bundle.graph.dsc_curriculum = san.SanitizeText(JsonStr(obj, {"dsc_plan", "dsc_curriculum"}));

    auto extract_ids = [&](const nlohmann::json& c, const char* key, std::vector<std::string>& dst) {
        if (!c.contains(key) || !c[key].is_array()) return;
        for (const auto& el : c[key]) {
            if (el.is_string()) {
                std::string s = san.SanitizeText(el.get<std::string>());
                if (!s.empty()) dst.push_back(std::move(s));
            } else if (el.is_object()) {
                std::string s = san.SanitizeText(JsonStr(el, {"id", "id_course", "code"}));
                if (!s.empty()) dst.push_back(std::move(s));
            }
        }
    };

    if (obj.contains("curriculum") && obj["curriculum"].is_array()) {
        for (const auto& lv : obj["curriculum"]) {
            if (!lv.is_object()) continue;
            CurriculumLevel level;
            level.id = san.SanitizeText(JsonStr(lv, {"id"}));
            if (lv.contains("courses") && lv["courses"].is_array()) {
                for (const auto& cj : lv["courses"]) {
                    if (!cj.is_object()) continue;
                    CurriculumCourseNode node;
                    node.id_course = san.SanitizeText(JsonStr(cj, {"id_course", "id"}));
                    node.name = san.SanitizeText(JsonStr(cj, {"name"}));
                    node.truncated_name = san.SanitizeText(JsonStr(cj, {"trucatedName", "truncatedName"}));
                    node.credits = JsonInt(cj, {"credits"});
                    node.hours = JsonInt(cj, {"hours"});
                    node.t_equiv = san.SanitizeText(JsonStr(cj, {"tEquiv"}));
                    extract_ids(cj, "requirements", node.requirements);
                    extract_ids(cj, "co_requirements", node.co_requirements);
                    extract_ids(cj, "required_by", node.required_by);
                    extract_ids(cj, "equivalent", node.equivalent);

                    StudentCourseRecord rec;
                    rec.id_course = node.id_course;
                    rec.best_status = san.SanitizeText(JsonStr(cj, {"best_status"}));
                    if (cj.contains("grades_record") && cj["grades_record"].is_array()) {
                        for (const auto& gr : cj["grades_record"]) {
                            if (!gr.is_object()) continue;
                            CourseGradeAttempt att;
                            att.year = JsonInt(gr, {"year"});
                            att.period = JsonInt(gr, {"period"});
                            att.modality = san.SanitizeText(JsonStr(gr, {"modality"}));
                            att.state = JsonInt(gr, {"state"});
                            att.grade = JsonDbl(gr, {"grade"});
                            att.group = JsonInt(gr, {"group"});
                            rec.grades_record.push_back(std::move(att));
                        }
                    }
                    if (!rec.id_course.empty()) {
                        bundle.profile.grades.push_back(std::move(rec));
                    }
                    level.courses.push_back(std::move(node));
                }
            }
            bundle.graph.levels.push_back(std::move(level));
        }
    }
    return Result<GraphQlCurriculumBundle>::Success(std::move(bundle));
}

}  // namespace academic::tec_digital
