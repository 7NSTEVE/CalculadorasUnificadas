#include "academic/tec_digital/policy.hpp"
#include "http_common.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace academic::tec_digital {

namespace {

int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::string UrlDecode(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == '+') {
            out.push_back(' ');
        } else if (sv[i] == '%' && i + 2 < sv.size()) {
            int hi = HexVal(sv[i + 1]);
            int lo = HexVal(sv[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
            out.push_back(sv[i]);
        } else {
            out.push_back(sv[i]);
        }
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> ParseQueryPairs(std::string_view query) {
    std::vector<std::pair<std::string, std::string>> out;
    if (query.empty()) return out;
    size_t pos = 0;
    while (pos <= query.size()) {
        size_t amp = query.find('&', pos);
        if (amp == std::string_view::npos) amp = query.size();
        std::string_view item(query.data() + pos, amp - pos);
        if (!item.empty()) {
            size_t eq = item.find('=');
            std::string k = UrlDecode(eq == std::string_view::npos ? item : item.substr(0, eq));
            std::string v = (eq == std::string_view::npos) ? "" : UrlDecode(item.substr(eq + 1));
            out.emplace_back(std::move(k), std::move(v));
        }
        if (amp == query.size()) break;
        pos = amp + 1;
    }
    return out;
}

bool ContainsCI(std::string_view haystack, std::string_view needle) {
    return AsciiLower(haystack).find(AsciiLower(needle)) != std::string::npos;
}

bool IsPassiveResourceType(std::string_view resource_type) {
    std::string r = AsciiLower(resource_type);
    return r == "script" || r == "stylesheet" || r == "image" || r == "font" || r == "media";
}

bool DecodeAndNormalizePolicyPath(std::string_view raw_path, std::string& out_path) {
    std::string decoded;
    decoded.reserve(raw_path.size());
    for (size_t i = 0; i < raw_path.size(); ++i) {
        char c = raw_path[i];
        if (c == '\\') {
            return false;
        }
        if (c == '%') {
            if (i + 2 >= raw_path.size()) return false;
            int hi = HexVal(raw_path[i + 1]);
            int lo = HexVal(raw_path[i + 2]);
            if (hi < 0 || lo < 0) return false;
            unsigned char b = static_cast<unsigned char>((hi << 4) | lo);
            if (b == 0x00 || b == '/' || b == '\\') return false;
            decoded.push_back(static_cast<char>(b));
            i += 2;
        } else {
            decoded.push_back(c);
        }
    }

    std::vector<std::string_view> stack;
    const bool had_trailing_slash = !decoded.empty() && decoded.back() == '/';
    bool ended_with_dot = false;
    size_t i = 0;
    while (i < decoded.size()) {
        while (i < decoded.size() && decoded[i] == '/') ++i;
        if (i >= decoded.size()) break;
        size_t j = decoded.find('/', i);
        if (j == std::string::npos) j = decoded.size();
        std::string_view seg(decoded.data() + i, j - i);
        ended_with_dot = (seg == "." || seg == "..");
        if (seg == ".") {
            // skip
        } else if (seg == "..") {
            if (!stack.empty()) stack.pop_back();
        } else {
            stack.push_back(seg);
        }
        i = j;
    }

    out_path = "/";
    for (size_t idx = 0; idx < stack.size(); ++idx) {
        if (idx > 0) out_path.push_back('/');
        out_path.append(stack[idx]);
    }
    if ((had_trailing_slash || ended_with_dot) && out_path.size() > 1) {
        out_path.push_back('/');
    }
    return true;
}

// Strip final filename segment ONLY when the already-decoded, dot-normalized path
// genuinely matches /file-storage/(view|download)/[^/]+$ so filenames like
// "tarea_delete.pdf" do not trigger false-positive path-segment blocks.
std::string PathForSegmentCheck(std::string_view norm_path) {
    static const std::regex kGenuineFileStorageLeafRe(
        R"(^.*/file-storage/(?:view|download)/[^/]+$)",
        std::regex::ECMAScript | std::regex::icase);
    std::string path_str(norm_path);
    if (std::regex_match(path_str, kGenuineFileStorageLeafRe)) {
        size_t last_slash = path_str.rfind('/');
        if (last_slash != std::string::npos && last_slash + 1 < path_str.size()) {
            return path_str.substr(0, last_slash + 1);
        }
    }
    return path_str;
}

bool ContainsBlockedUrlPattern(const ParsedUrl& parsed) {
    const std::string full_low_target =
        AsciiLower(parsed.path + (parsed.query.empty() ? "" : "?" + parsed.query));
    static const std::regex kUrlBlockRe(
        R"((/register/logout|/logout\b|/log-out\b|unsubscribe|deregister|configure-element|op=shade|op=unshade|op=delete|op=remove|op=drop|op=subscribe|op=hide|op=show|op=move|op=config|action=op_swap|action=op_move|action=op_hide|action=op_show|/admin/|administer|membership-toggle|toggle-membership|/spam/|/ajax/spam\b|comment-delete|file-delete|folder-delete|news-delete|member-confirm(?:-\d+)?|/member-delete|/member-remove|/cal-item-new\b|request-change-frequency|password-update|cerrar[-_ ]?sesi|/shared/make-invisible|/user/portrait/upload|/user/basic-info-update|/pvt/unsubscribe|/notifications/request-new|/notifications/request-delete|save_browser_notification|/tda-sms-configuration/envio/ajax/register|/portal/ajax/portal-configure|/file-storage/api/(?:move|copy|notification)/|/file-storage/files/upload|/file-storage/create_folder2?|/file-storage/delete-elements|set_live_version=|delivery_method_id=|permissions\?object_id=|/tda-control-plagio/ajax/fileUpload|addDocumentEphorus|actualizaVisibilidad|/dotlrn/ajax/actualizar_cursos\b|actualizar_cursos2\b|actualizar__datos|/tda-um/ajax/um_get_test_info|insert_personal_info|save_privacy_settings|set_term_condition_answer|/tda-consentimiento-informado/ajax/admin/(?:publish_term_condition|delete_term_condition)|update_notification_state|delete_user_notification|delete_all_user_notifications|upload_evaluation|delete_version_answer|request_notifications|/tda-motor-juegos/resources/ajax/(?:synchronization|calificar_asignacion|eliminar_calificacion)|/servicios/gaap_oacs/PL/(?:funcionAsistenteCreacion|funcionAlmacenarCalificaciones|divAgregar[^/]*|divShareActivity)\.php|/tda-rubrica/ajax/(?:accion_rubrica|accion_evaluacion|register_std_grade|guardar[^/]*|ejecutarEliminar[^/]*|accion_permisos)|/tda-prematricula/ajax/(?:guardar_prematricula|actualiza_prematricula|recordRequestR|send_notification|set_additional_students|generateProcess)|/tda-gestor-inscripcion/estudiante/ajax/popup_matricula|/tda-feria-negocios/ajax/(?:ferias/(?:insert_feria|update_feria|delete_feria|add_folder)|registrar_proyectos/(?:add_folder|insert_project|register_project_sede|insertMembers|insertUsers|insertAllContent|record_project|record_project_file|register_notification))|/tds-info-general/files/upload))",
        std::regex::ECMAScript | std::regex::icase);
    return std::regex_search(full_low_target, kUrlBlockRe);
}

void AppendParamValues(std::string_view raw,
                       std::string_view key_lower,
                       std::vector<std::string>& out_vals,
                       bool& parse_error) {
    std::string trimmed = TrimAscii(raw);
    if (trimmed.empty()) return;
    if (trimmed.front() == '{') {
        auto j = nlohmann::json::parse(trimmed, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            parse_error = true;
            return;
        }
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (AsciiLower(it.key()) == key_lower) {
                if (it.value().is_string()) {
                    out_vals.push_back(it.value().get<std::string>());
                } else if (it.value().is_number_integer()) {
                    out_vals.push_back(std::to_string(it.value().get<int64_t>()));
                } else if (it.value().is_number_unsigned()) {
                    out_vals.push_back(std::to_string(it.value().get<uint64_t>()));
                } else {
                    parse_error = true;
                }
            }
        }
        return;
    }
    if (trimmed.find('=') != std::string::npos) {
        for (const auto& kv : ParseQueryPairs(trimmed)) {
            if (AsciiLower(kv.first) == key_lower) {
                out_vals.push_back(kv.second);
            }
        }
    }
}

// Returns std::optional-style pair: {handled, decision}
bool EvaluateConditionalEndpoint(const ParsedUrl& parsed,
                                 std::string_view verb,
                                 const PolicyRequestContext& ctx,
                                 PolicyDecision& out_decision) {
    const std::string low_path = AsciiLower(parsed.path);

    if (low_path == "/tda-control-plagio/ajax/cargadocumentos/termsandconditions") {
        std::vector<std::string> types;
        bool parse_error = false;
        AppendParamValues(parsed.query, "type", types, parse_error);
        AppendParamValues(ctx.post_data, "type", types, parse_error);
        if (parse_error || types.size() != 1 || (types[0] != "1" && types[0] != "3")) {
            out_decision = PolicyDecision::Block("conditional-endpoint-not-proven-read-only");
            return true;
        }
        out_decision = PolicyDecision::Allow();
        return true;
    }

    if (low_path == "/tda-rubrica/tda_evaluacion_rubrica") {
        std::vector<std::string> acciones;
        bool parse_error = false;
        AppendParamValues(parsed.query, "accion", acciones, parse_error);
        AppendParamValues(ctx.post_data, "accion", acciones, parse_error);
        if (parse_error) {
            out_decision = PolicyDecision::Block("conditional-endpoint-not-proven-read-only");
            return true;
        }
        if (acciones.empty()) {
            out_decision = (verb == "POST")
                               ? PolicyDecision::Block("conditional-endpoint-not-proven-read-only")
                               : PolicyDecision::Allow();
            return true;
        }
        for (const auto& a : acciones) {
            if (AsciiLower(TrimAscii(a)) != "ver") {
                out_decision = PolicyDecision::Block("conditional-endpoint-not-proven-read-only");
                return true;
            }
        }
        out_decision = PolicyDecision::Allow();
        return true;
    }

    if (low_path == "/tda-prematricula/ajax/service_courses_student") {
        std::vector<std::string> actions;
        bool parse_error = false;
        AppendParamValues(parsed.query, "action", actions, parse_error);
        AppendParamValues(ctx.post_data, "action", actions, parse_error);
        if (parse_error) {
            out_decision = PolicyDecision::Block("prematricula-operation-not-proven-read-only");
            return true;
        }
        for (const auto& a : actions) {
            if (TrimAscii(a) == "1") {
                out_decision = PolicyDecision::Block("prematricula-operation-not-proven-read-only");
                return true;
            }
        }
        if (verb == "POST") {
            out_decision = PolicyDecision::Block("post-not-proven-read-only");
            return true;
        }
        out_decision = PolicyDecision::Allow();
        return true;
    }

    return false;
}

bool IsMutatingPathOrQuery(const ParsedUrl& parsed) {
    const std::string check_path = PathForSegmentCheck(parsed.path);
    const std::string full_low_path = AsciiLower(parsed.path);

    // Read-only false-positive exclusions before broad pattern checks:
    // - forums/message-view?message_id=...
    // - /evaluation/tda-ce-estudiante/tda-index or evaluation/view
    static const std::regex kMutatingSegmentRe(
        R"((?:^|[/_.-])(?:delete|remove|edit|update|save|submit|publish|upload|move|rename|enroll|deregister|unregister|withdraw|unsubscribe|create|add|approve|reject|grade|send|reply|logout|log[-_]?out|sign[-_]?out|mark[-_]?(?:all[-_]?)?read|borrar|eliminar|editar|actualizar|guardar|subir|mover|renombrar|inscribir|desinscribir|aprobar|rechazar|calificar|responder)(?:$|[/_.-]|\d))",
        std::regex::ECMAScript | std::regex::icase);

    if (std::regex_search(check_path, kMutatingSegmentRe)) {
        return true;
    }

    static const std::regex kExactSegmentBlockRe(
        R"((?:^|/)(?:log_registration|tam-tracking|to-tam-tracking|matricula)(?:/|$))",
        std::regex::ECMAScript | std::regex::icase);

    if (std::regex_search(check_path, kExactSegmentBlockRe)) {
        return true;
    }

    if (ContainsBlockedUrlPattern(parsed)) {
        return true;
    }

    static const std::regex kActionKeyRe(R"(^(?:action|operation|cmd|mode|op)$)",
                                         std::regex::ECMAScript | std::regex::icase);
    static const std::regex kMutatingOpRe(
        R"(^(?:delete|remove|update|save|submit|publish|upload|move|rename|enroll|deregister|unregister|withdraw|unsubscribe|create|add|approve|reject|grade|send|reply|mark_read|shade|unshade|hide|show|drop|subscribe|config|op_swap|op_move|op_hide|op_show|borrar|eliminar)$)",
        std::regex::ECMAScript | std::regex::icase);

    auto qpairs = ParseQueryPairs(parsed.query);
    for (const auto& kv : qpairs) {
        if (std::regex_match(kv.first, kActionKeyRe) &&
            std::regex_match(kv.second, kMutatingOpRe)) {
            return true;
        }
    }

    if (ContainsCI(full_low_path, "/evaluation") || ContainsCI(full_low_path, "/assessment")) {
        static const std::regex kEvalMutateRe(
            R"((?:^|[/_.-])(?:submit|upload|answer|respond|delete|confirm|calificar|entregar|responder|grade-change|qualify)(?:$|[/_.-]))",
            std::regex::ECMAScript | std::regex::icase);
        if (std::regex_search(full_low_path, kEvalMutateRe)) {
            return true;
        }
    }

    return false;
}

}  // namespace

ReadOnlyRequestPolicy::ReadOnlyRequestPolicy(PolicyOptions opts)
    : opts_(std::move(opts)) {
    detail::WarmupRegexLocale();
}

bool ReadOnlyRequestPolicy::IsTecDigitalOrigin(const ParsedUrl& parsed) const {
    if (!parsed.valid) return false;
    if (parsed.scheme == "https" && parsed.host == "tecdigital.tec.ac.cr" && parsed.port == 443) {
        return true;
    }
    for (const auto& test_origin : opts_.allowed_test_origins) {
        ParsedUrl to = ParseHttpUrl(test_origin);
        if (to.valid && IsLoopbackHost(to.host) && parsed.origin == to.origin) {
            return true;
        }
    }
    return false;
}

bool ReadOnlyRequestPolicy::IsGraphQlEndpoint(const ParsedUrl& parsed) const {
    if (!parsed.valid) return false;
    if (parsed.scheme == "https" && parsed.host == "api.tec.ac.cr" &&
        parsed.port == 3005 && parsed.path == "/graphql") {
        return true;
    }
    for (const auto& test_origin : opts_.allowed_test_origins) {
        ParsedUrl to = ParseHttpUrl(test_origin);
        if (to.valid && IsLoopbackHost(to.host) &&
            parsed.origin == to.origin && parsed.path == "/graphql") {
            return true;
        }
    }
    return false;
}

bool ReadOnlyRequestPolicy::IsPublicGuiaHorariosEndpoint(const ParsedUrl& parsed) const {
    if (!parsed.valid) return false;
    const std::string low_path = AsciiLower(parsed.path);
    const bool path_matches = (low_path == "/guiahorarios" ||
                               low_path.compare(0, 14, "/guiahorarios/") == 0);
    if (!path_matches) return false;

    if (parsed.scheme == "https" && parsed.host == "tec-appsext.itcr.ac.cr" && parsed.port == 443) {
        return true;
    }
    for (const auto& test_origin : opts_.allowed_test_origins) {
        ParsedUrl to = ParseHttpUrl(test_origin);
        if (to.valid && IsLoopbackHost(to.host) && parsed.origin == to.origin) {
            return true;
        }
    }
    return false;
}

bool ReadOnlyRequestPolicy::IsGaapApiOrigin(const ParsedUrl& parsed) const {
    if (!parsed.valid) return false;
    if (parsed.scheme == "https" && parsed.host == "api.tec.ac.cr" && parsed.port == 3003) {
        return true;
    }
    return false;
}

bool ReadOnlyRequestPolicy::IsAllowedOrigin(std::string_view url) const {
    ParsedUrl parsed = ParseHttpUrl(url);
    if (!parsed.valid) return false;
    std::string norm_path;
    if (!DecodeAndNormalizePolicyPath(parsed.path, norm_path)) {
        return false;
    }
    parsed.path = std::move(norm_path);
    return IsTecDigitalOrigin(parsed) || IsGraphQlEndpoint(parsed) ||
           IsGaapApiOrigin(parsed) || IsPublicGuiaHorariosEndpoint(parsed);
}

bool ReadOnlyRequestPolicy::IsAllowedStudentGraphQlMutation(std::string_view query,
                                                            uint32_t allowed_mutations_mask) {
    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::ManageInclusions)) == 0u) {
        return false;
    }
    detail::WarmupRegexLocale();
    std::string q(query);
    static const std::regex kStudentInclMutRe(
        R"(\b(?:SetInclusionRequest|DeleteInclusionRequest|ToogleInclusion|ToggleInclusion|SetScholarship)\b)",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kAdminMutRe(
        R"(\b(?:SetPeriodCourses|setCourseParameter|DeleteCourseParameter|SendApprovePreconfiguration|ApprovePreconfiguration|ChangeApplicationsStatusOrComment|ChangeApplicationsToReviewed|SavePriorityConfig|VerifyAlllApplications|VerifyAllApplications|SetAdmInclusionRequest|RemoveInclusionRequest|ToggleStudentException)\b)",
        std::regex::ECMAScript | std::regex::icase);
    return std::regex_search(q, kStudentInclMutRe) && !std::regex_search(q, kAdminMutRe);
}

bool ReadOnlyRequestPolicy::IsReadOnlyGraphQL(std::string_view query) {
    detail::WarmupRegexLocale();
    // Strip #... comments to EOL
    std::string doc;
    doc.reserve(query.size());
    bool in_comment = false;
    for (char c : query) {
        if (in_comment) {
            if (c == '\r' || c == '\n') {
                in_comment = false;
                doc.push_back(' ');
            }
        } else if (c == '#') {
            in_comment = true;
            doc.push_back(' ');
        } else {
            doc.push_back(c);
        }
    }
    doc = TrimAscii(doc);
    if (doc.empty()) return false;

    static const std::regex kStartsWithQueryOrBrace(R"(^(?:query\b|\{))", std::regex::ECMAScript);
    if (!std::regex_search(doc, kStartsWithQueryOrBrace)) {
        return false;
    }

    static const std::regex kHasMutationOrSubscription(
        R"(\b(?:mutation|subscription|SetInclusionRequest|DeleteInclusionRequest|SetScholarship|SetPeriodCourses|setCourseParameter|ToogleInclusion|ToggleInclusion|DeleteCourseParameter|SendApprovePreconfiguration|ApprovePreconfiguration|ChangeApplicationsStatusOrComment|ChangeApplicationsToReviewed|SavePriorityConfig|VerifyAlllApplications|VerifyAllApplications|SetAdmInclusionRequest|RemoveInclusionRequest|ToggleStudentException)\b)",
        std::regex::ECMAScript | std::regex::icase);
    if (std::regex_search(doc, kHasMutationOrSubscription)) {
        return false;
    }
    return true;
}

bool ReadOnlyRequestPolicy::IsPrematriculaReadOnly(std::string_view body) {
    std::string trimmed = TrimAscii(body);
    if (trimmed.empty()) return false;

    static const std::unordered_set<std::string> kPermittedKeys = {
        "tipo", "pre_enrollment_id", "curriculum_id", "curriculum_id_current"
    };

    if (trimmed.front() == '{') {
        try {
            auto j = nlohmann::json::parse(trimmed);
            if (!j.is_object() || j.empty()) return false;
            std::string tipo_val;
            bool has_tipo = false;
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (kPermittedKeys.count(it.key()) == 0) return false;
                if (it.key() == "tipo") {
                    has_tipo = true;
                    if (it.value().is_string()) {
                        tipo_val = it.value().get<std::string>();
                    } else if (it.value().is_number_integer()) {
                        tipo_val = std::to_string(it.value().get<int64_t>());
                    } else {
                        return false;
                    }
                }
            }
            return has_tipo && (tipo_val == "1" || tipo_val == "2" || tipo_val == "5");
        } catch (...) {
            return false;
        }
    }

    if (trimmed.find('=') != std::string::npos) {
        auto pairs = ParseQueryPairs(trimmed);
        if (pairs.empty()) return false;
        bool has_tipo = false;
        std::string tipo_val;
        for (const auto& kv : pairs) {
            if (kPermittedKeys.count(kv.first) == 0) return false;
            if (kv.first == "tipo") {
                if (has_tipo) return false;  // reject duplicate tipo ambiguity
                has_tipo = true;
                tipo_val = kv.second;
            }
        }
        return has_tipo && (tipo_val == "1" || tipo_val == "2" || tipo_val == "5");
    }

    return false;
}

bool ReadOnlyRequestPolicy::IsMutatingUiLabel(std::string_view label) {
    detail::WarmupRegexLocale();
    std::string trimmed = TrimAscii(label);
    if (trimmed.empty()) return false;

    static const std::regex kMutatingLabelRe(
        R"((?:dar de baja|darse de baja|cancelar inscripci|retirar curso|desinscribirse|cerrar sesi))",
        std::regex::ECMAScript | std::regex::icase);
    if (std::regex_search(trimmed, kMutatingLabelRe)) return true;

    static const std::regex kConfirmBtnRe(
        R"(^(?:aceptar|confirmar|si|sí|yes|ok|enviar|guardar|aplicar)(?:\s|$))",
        std::regex::ECMAScript | std::regex::icase);
    return std::regex_search(trimmed, kConfirmBtnRe);
}

bool ReadOnlyRequestPolicy::IsReadOnlyPostPath(std::string_view pathname) {
    detail::WarmupRegexLocale();
    static const std::unordered_set<std::string> kExactPaths = {
        "/notifications/ajax/request_id",
        "/tda-consentimiento-informado/ajax/client/get_user_term_condition",
        "/tda-expediente-estudiantil/ajax/i18n",
        "/tda-expediente-estudiantil/ajax/i18n_validation",
        "/tda-expediente-estudiantil/ajax/info_personal",
        "/tda-expediente-estudiantil/ajax/detalle_notas",
        "/tda-expediente-estudiantil/ajax/detalle_promedio_ponderado",
        "/tda-expediente-estudiantil/ajax/detalle_historial",
        "/tda-expediente-estudiantil/ajax/generar_tabla_promedio_ponderado",
        "/tda-expediente-estudiantil/ajax/tabla_horario",
        "/tda-expediente-estudiantil/ajax/citas_matricula",
        "/tda-expediente-estudiantil/ajax/guia_horario",
        "/tda-expediente-estudiantil/ajax/tabla_guia_horario",
        "/tda-expediente-estudiantil/ajax/get_data_attributes",
        "/tda-expediente-estudiantil/ajax/actualizar_cursos",
        "/tda-expediente-estudiantil/ajax/Consultar_datos_estudiante",
        "/tda-expediente-estudiantil/ajax/get_user_id",
        "/tda-expediente-estudiantil/ajax/combos/carga_sedes",
        "/tda-expediente-estudiantil/ajax/combos/carga_carreras",
        "/tda-expediente-estudiantil/ajax/combos/carga_periodos",
        "/tda-expediente-estudiantil/ajax/combos/carga_sedes_tds_lib",
        "/tda-expediente-estudiantil/ajax/combos/carga_carreras_tds_lib",
        "/tda-expediente-estudiantil/ajax/combos/carga_periodos_tds_lib",
        "/tda-feria-negocios/ajax/internationalization",
        "/tda-feria-negocios/ajax/ferias/get_ferias",
        "/tda-feria-negocios/ajax/categorias/load_categorias",
        "/tda-um/ajax/get_gaap_server",
        "/tda-um/ajax/internacionalization",
        "/tda-um/ajax/get_info",
        "/tda-um/ajax/get_statistic_form",
        "/tda-um/ajax/general_statistics",
        "/tda-um/ajax/get_gaap_server",
        "/tda-motor-juegos/api/user/",
        "/tda-motor-juegos/api/activities/",
        "/tda-motor-juegos/api/community_details/",
        "/tda-motor-juegos/api/get_evaluation_item_id/",
        "/tda-motor-juegos/api/get_is_grade_published/",
        "/tda-motor-juegos/api/get_list_is_grade_published/",
        "/tda-motor-juegos/api/user/photo/",
        "/tda-motor-juegos/api/user/photo/list",
        "/servicios/gaap_oacs/PL/funcionVisualizadorActividades.php",
        "/servicios/gaap_oacs/PL/funcionVisualizadorHistoricos.php"
    };
    std::string pstr(pathname);
    if (kExactPaths.count(pstr) > 0) {
        return true;
    }
    static const std::regex kLoadMembersRe(
        R"(^/dotlrn/(?:[^/]+/)*ajax/(?:load_members|load_members_parent|load_pending)$)",
        std::regex::ECMAScript);
    if (std::regex_match(pstr, kLoadMembersRe)) {
        return true;
    }
    static const std::regex kEvalAjaxReadRe(
        R"(^/dotlrn/(?:[^/]+/)*evaluation/tda-ce-estudiante/ajax/(?:Internationalization|get_history_files|get_data_liverevision)$)",
        std::regex::ECMAScript);
    return std::regex_match(pstr, kEvalAjaxReadRe);
}

bool ReadOnlyRequestPolicy::IsPublicGuiaHorariosReadOnlyPostPath(std::string_view pathname) {
    static const std::unordered_set<std::string> kPublicGuiaPostPaths = {
        "/guiahorarios/escuela.aspx/cargaescuelas",
        "/guiahorarios/escuela.aspx/cargamodalidadperiodos",
        "/guiahorarios/escuela.aspx/getdatosescuelaano",
        "/guiahorarios/verano.aspx/getdatosverano",
        "/guiahorarios/estudiante.aspx/getdatos",
        "/guiahorarios/estudiante.aspx/getdatosano"
    };
    return kPublicGuiaPostPaths.count(AsciiLower(pathname)) > 0;
}

bool ReadOnlyRequestPolicy::IsAllowedUserMutationRequest(std::string_view method,
                                                         std::string_view norm_path,
                                                         uint32_t allowed_mutations_mask) {
    if (allowed_mutations_mask == 0u) return false;
    detail::WarmupRegexLocale();
    std::string verb(method);
    for (char& c : verb) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    std::string low_path = AsciiLower(norm_path);

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::UploadEvaluation)) != 0u &&
        verb == "POST") {
        static const std::regex kUploadEvalRe(
            R"(^/dotlrn/(?:[^/]+/)*evaluation/tda-ce-estudiante/ajax/upload_evaluation$)",
            std::regex::ECMAScript);
        if (std::regex_match(low_path, kUploadEvalRe)) return true;
    }

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::DeleteEvaluationRevision)) != 0u &&
        verb == "POST") {
        static const std::regex kDeleteRevRe(
            R"(^/dotlrn/(?:[^/]+/)*evaluation/tda-ce-estudiante/ajax/delete_version_answer$)",
            std::regex::ECMAScript);
        if (std::regex_match(low_path, kDeleteRevRe)) return true;
    }

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::ManageNotifications)) != 0u) {
        if (verb == "GET" && (low_path == "/tda-notifications/ajax/update_notification_state" ||
                              low_path == "/tda-notifications/ajax/delete_user_notification" ||
                              low_path == "/tda-notifications/ajax/delete_all_user_notifications" ||
                              low_path == "/dotlrn/ajax/save_browser_notification")) {
            return true;
        }
        if (verb == "POST") {
            if (low_path == "/dotlrn/file-storage/api/notification/") return true;
            static const std::regex kReqEvalNotifRe(
                R"(^/dotlrn/(?:[^/]+/)*evaluation/tda-ce-estudiante/ajax/request_notifications$)",
                std::regex::ECMAScript);
            if (std::regex_match(low_path, kReqEvalNotifRe)) return true;
        }
    }

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::RefreshEnrolledCourses)) != 0u &&
        verb == "POST") {
        if (low_path == "/dotlrn/ajax/actualizar_cursos" ||
            low_path == "/tda-expediente-estudiantil/ajax/actualizar_cursos2" ||
            low_path == "/tda-expediente-estudiantil/ajax/actualizar__datos") {
            return true;
        }
    }

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::ManagePlagiarism)) != 0u) {
        if (verb == "POST" && low_path == "/tda-control-plagio/ajax/fileupload") return true;
        if (verb == "GET" && (low_path == "/tda-control-plagio/ajax/adddocumentephorus" ||
                              low_path == "/tda-control-plagio/ajax/actualizavisibilidad")) {
            return true;
        }
    }

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::ManagePrematricula)) != 0u) {
        if ((verb == "GET" || verb == "POST") &&
            (low_path == "/tda-prematricula/ajax/guardar_prematricula" ||
             low_path == "/tda-prematricula/ajax/recordrequestr" ||
             low_path == "/tda-prematricula/ajax/service_courses_student" ||
             low_path == "/tda-prematricula/ajax/prematricula_est")) {
            return true;
        }
    }

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::ManageFairs)) != 0u) {
        if ((verb == "POST" || verb == "GET") &&
            (low_path.rfind("/tda-feria-negocios/ajax/registrar_proyectos/", 0) == 0 ||
             low_path == "/tds-info-general/files/upload")) {
            return true;
        }
    }

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::ManageProfileAndConsent)) != 0u) {
        if (verb == "GET" && (low_path == "/tda-um/ajax/insert_personal_info" ||
                              low_path == "/tda-um/ajax/save_privacy_settings")) {
            return true;
        }
        if (verb == "POST" && low_path == "/tda-consentimiento-informado/ajax/client/set_term_condition_answer") {
            return true;
        }
    }

    if ((allowed_mutations_mask & MutationMask(AllowedStudentMutation::ManagePersonalFiles)) != 0u) {
        if ((verb == "POST" || verb == "PUT" || verb == "DELETE") &&
            (low_path.rfind("/dotlrn/file-storage/api/folder/", 0) == 0 ||
             low_path.rfind("/dotlrn/file-storage/api/file/", 0) == 0 ||
             low_path.rfind("/dotlrn/file-storage/api/move/", 0) == 0 ||
             low_path.rfind("/dotlrn/file-storage/api/copy/", 0) == 0 ||
             low_path == "/dotlrn/file-storage/delete-elements")) {
            return true;
        }
    }

    return false;
}

PolicyDecision ReadOnlyRequestPolicy::Evaluate(const PolicyRequestContext& ctx) const {
    ParsedUrl parsed = ParseHttpUrl(ctx.url);
    if (!parsed.valid) {
        return PolicyDecision::Block("unsupported-url");
    }

    std::string norm_path;
    if (!DecodeAndNormalizePolicyPath(parsed.path, norm_path)) {
        return PolicyDecision::Block("malformed-url-path");
    }
    parsed.path = std::move(norm_path);

    std::string verb = ctx.method.empty() ? "GET" : ctx.method;
    for (char& c : verb) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Branch A: GraphQL endpoint
    if (IsGraphQlEndpoint(parsed)) {
        if (verb == "HEAD" || verb == "OPTIONS") {
            return PolicyDecision::Allow();
        }
        std::string gql_query;
        if (verb == "GET") {
            bool found_query = false;
            for (const auto& kv : ParseQueryPairs(parsed.query)) {
                if (kv.first == "query") {
                    if (found_query) {
                        return PolicyDecision::Block("graphql-not-proven-query");
                    }
                    found_query = true;
                    gql_query = kv.second;
                } else if (AsciiLower(kv.first) == "query") {
                    return PolicyDecision::Block("graphql-not-proven-query");
                }
            }
        } else if (verb == "POST") {
            std::string body = TrimAscii(ctx.post_data);
            if (!body.empty() && body.front() == '{') {
                try {
                    auto j = nlohmann::json::parse(body);
                    if (j.is_object() && j.contains("query") && j["query"].is_string()) {
                        gql_query = j["query"].get<std::string>();
                    }
                } catch (...) {
                    gql_query.clear();
                }
            } else if (body.find('=') != std::string::npos) {
                bool found_query = false;
                for (const auto& kv : ParseQueryPairs(body)) {
                    if (kv.first == "query") {
                        if (found_query) {
                            return PolicyDecision::Block("graphql-not-proven-query");
                        }
                        found_query = true;
                        gql_query = kv.second;
                    } else if (AsciiLower(kv.first) == "query") {
                        return PolicyDecision::Block("graphql-not-proven-query");
                    }
                }
            }
        } else {
            return PolicyDecision::Block("graphql-not-proven-query");
        }
        return (IsReadOnlyGraphQL(gql_query) ||
                IsAllowedStudentGraphQlMutation(gql_query, ctx.allowed_mutations_mask))
                   ? PolicyDecision::Allow()
                   : PolicyDecision::Block("graphql-not-proven-query");
    }

    // Branch A1b: Official GAAP API (https://api.tec.ac.cr:3003) read-only student queries
    if (IsGaapApiOrigin(parsed)) {
        if (verb == "GET" || verb == "HEAD" || verb == "OPTIONS") {
            return PolicyDecision::Allow();
        }
        return PolicyDecision::Block("post-not-proven-read-only");
    }

    // Branch A2: Public Guia de Horarios TEC (https://tec-appsext.itcr.ac.cr/guiahorarios/)
    if (IsPublicGuiaHorariosEndpoint(parsed)) {
        const std::string low_path = AsciiLower(parsed.path);
        if (verb == "GET" || verb == "HEAD" || verb == "OPTIONS") {
            if (!IsPassiveResourceType(ctx.resource_type)) {
                if (IsMutatingPathOrQuery(parsed) || IsMutatingUiLabel(ctx.link_label)) {
                    return PolicyDecision::Block("mutating-navigation-or-operation");
                }
                const bool safe_page = (low_path == "/guiahorarios" ||
                                        low_path == "/guiahorarios/" ||
                                        low_path == "/guiahorarios/estudiante.aspx" ||
                                        low_path == "/guiahorarios/verano.aspx" ||
                                        low_path == "/guiahorarios/escuela.aspx");
                if (!safe_page) {
                    return PolicyDecision::Block("external-origin");
                }
            }
            return PolicyDecision::Allow();
        }
        if (verb == "POST") {
            if (ContainsBlockedUrlPattern(parsed) || IsMutatingUiLabel(ctx.link_label)) {
                return PolicyDecision::Block("mutating-navigation-or-operation");
            }
            if (IsPublicGuiaHorariosReadOnlyPostPath(parsed.path)) {
                return PolicyDecision::Allow();
            }
            return PolicyDecision::Block("post-not-proven-read-only");
        }
        return PolicyDecision::Block("unreviewed-state-changing-method");
    }

    // Branch B: GET / HEAD / OPTIONS
    if (verb == "GET" || verb == "HEAD" || verb == "OPTIONS") {
        if (!IsTecDigitalOrigin(parsed)) {
            return PolicyDecision::Block("external-origin");
        }
        if (IsAllowedUserMutationRequest(verb, parsed.path, ctx.allowed_mutations_mask)) {
            return PolicyDecision::Allow();
        }
        if (!IsPassiveResourceType(ctx.resource_type)) {
            if (IsMutatingPathOrQuery(parsed) || IsMutatingUiLabel(ctx.link_label)) {
                return PolicyDecision::Block("mutating-navigation-or-operation");
            }
            PolicyDecision cond_decision;
            if (EvaluateConditionalEndpoint(parsed, verb, ctx, cond_decision)) {
                return cond_decision;
            }
        }
        return PolicyDecision::Allow();
    }

    // Branch C: Non-GET/HEAD/OPTIONS
    if (!IsTecDigitalOrigin(parsed)) {
        return PolicyDecision::Block("unreviewed-state-changing-method");
    }
    if (IsAllowedUserMutationRequest(verb, parsed.path, ctx.allowed_mutations_mask)) {
        return PolicyDecision::Allow();
    }
    if (verb != "POST") {
        return PolicyDecision::Block("unreviewed-state-changing-method");
    }

    if (parsed.path == "/api/login/new-form/" || parsed.path == "/api/login/scanning/") {
        if (ctx.allow_login) {
            return PolicyDecision::Allow();
        }
        return PolicyDecision::Block("login-outside-authentication-phase");
    }

    if (ContainsBlockedUrlPattern(parsed) || IsMutatingUiLabel(ctx.link_label)) {
        return PolicyDecision::Block("mutating-navigation-or-operation");
    }

    PolicyDecision cond_decision;
    if (EvaluateConditionalEndpoint(parsed, verb, ctx, cond_decision)) {
        return cond_decision;
    }

    if (parsed.path == "/tda-prematricula/ajax/prematricula_est") {
        if (IsPrematriculaReadOnly(ctx.post_data)) {
            return PolicyDecision::Allow();
        }
        return PolicyDecision::Block("prematricula-operation-not-proven-read-only");
    }

    if (IsReadOnlyPostPath(parsed.path)) {
        return PolicyDecision::Allow();
    }

    return PolicyDecision::Block("post-not-proven-read-only");
}

PolicyDecision ReadOnlyRequestPolicy::EvaluateHttpRequest(const HttpRequest& req,
                                                          bool allow_login,
                                                          uint32_t allowed_mutations_mask) const {
    PolicyRequestContext ctx;
    ctx.url = req.url;
    ctx.method = req.method;
    ctx.resource_type = "document";
    ctx.post_data = req.body;
    ctx.allow_login = allow_login;
    ctx.allowed_mutations_mask = allowed_mutations_mask;
    return Evaluate(ctx);
}

PolicyEnforcingTransport::PolicyEnforcingTransport(IHttpTransport& inner,
                                                   ReadOnlyRequestPolicy policy)
    : inner_(inner), policy_(std::move(policy)) {}

void PolicyEnforcingTransport::AcquireMutation(AllowedStudentMutation m) {
    switch (m) {
        case AllowedStudentMutation::UploadEvaluation:
            mut_upload_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::DeleteEvaluationRevision:
            mut_delete_rev_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::ManageNotifications:
            mut_notif_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::RefreshEnrolledCourses:
            mut_refresh_courses_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::ManagePlagiarism:
            mut_plagiarism_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::ManagePrematricula:
            mut_prematricula_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::ManageInclusions:
            mut_inclusions_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::ManageFairs:
            mut_fairs_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::ManageProfileAndConsent:
            mut_profile_consent_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::ManagePersonalFiles:
            mut_personal_files_depth_.fetch_add(1, std::memory_order_acq_rel);
            break;
        case AllowedStudentMutation::None:
            break;
    }
}

void PolicyEnforcingTransport::ReleaseMutation(AllowedStudentMutation m) {
    auto dec_nonneg = [](std::atomic<int>& counter) {
        int prev = counter.load(std::memory_order_acquire);
        while (prev > 0 &&
               !counter.compare_exchange_weak(
                   prev, prev - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
        }
    };
    switch (m) {
        case AllowedStudentMutation::UploadEvaluation:
            dec_nonneg(mut_upload_depth_);
            break;
        case AllowedStudentMutation::DeleteEvaluationRevision:
            dec_nonneg(mut_delete_rev_depth_);
            break;
        case AllowedStudentMutation::ManageNotifications:
            dec_nonneg(mut_notif_depth_);
            break;
        case AllowedStudentMutation::RefreshEnrolledCourses:
            dec_nonneg(mut_refresh_courses_depth_);
            break;
        case AllowedStudentMutation::ManagePlagiarism:
            dec_nonneg(mut_plagiarism_depth_);
            break;
        case AllowedStudentMutation::ManagePrematricula:
            dec_nonneg(mut_prematricula_depth_);
            break;
        case AllowedStudentMutation::ManageInclusions:
            dec_nonneg(mut_inclusions_depth_);
            break;
        case AllowedStudentMutation::ManageFairs:
            dec_nonneg(mut_fairs_depth_);
            break;
        case AllowedStudentMutation::ManageProfileAndConsent:
            dec_nonneg(mut_profile_consent_depth_);
            break;
        case AllowedStudentMutation::ManagePersonalFiles:
            dec_nonneg(mut_personal_files_depth_);
            break;
        case AllowedStudentMutation::None:
            break;
    }
}

uint32_t PolicyEnforcingTransport::ActiveMutationsMask() const noexcept {
    uint32_t mask = 0u;
    if (mut_upload_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::UploadEvaluation);
    }
    if (mut_delete_rev_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::DeleteEvaluationRevision);
    }
    if (mut_notif_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::ManageNotifications);
    }
    if (mut_refresh_courses_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::RefreshEnrolledCourses);
    }
    if (mut_plagiarism_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::ManagePlagiarism);
    }
    if (mut_prematricula_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::ManagePrematricula);
    }
    if (mut_inclusions_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::ManageInclusions);
    }
    if (mut_fairs_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::ManageFairs);
    }
    if (mut_profile_consent_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::ManageProfileAndConsent);
    }
    if (mut_personal_files_depth_.load(std::memory_order_acquire) > 0) {
        mask |= MutationMask(AllowedStudentMutation::ManagePersonalFiles);
    }
    return mask;
}

HttpResponse PolicyEnforcingTransport::Execute(const HttpRequest& req) {
    HttpRequest current = req;
    const bool follow = req.follow_redirects;
    const int max_hops = (req.max_redirects > 0) ? req.max_redirects : 5;
    current.follow_redirects = false;

    for (int hop = 0; hop <= (follow ? max_hops : 0); ++hop) {
        PolicyDecision decision =
            policy_.EvaluateHttpRequest(current, AllowLogin(), ActiveMutationsMask());
        if (!decision.allowed) {
            HttpResponse blocked;
            blocked.transport_ok = false;
            blocked.final_url = current.url;
            blocked.error_message = "read-only-request-guard:" + decision.reason;
            return blocked;
        }

        HttpResponse resp = inner_.Execute(current);
        if (!follow || !resp.transport_ok) {
            return resp;
        }

        const int code = resp.status_code;
        const bool is_redirect =
            (code == 301 || code == 302 || code == 303 || code == 307 || code == 308);
        if (!is_redirect || resp.location.empty()) {
            return resp;
        }

        std::string next_url = ResolveRedirectLocation(current.url, resp.location);
        if (next_url.empty()) {
            resp.transport_ok = false;
            resp.error_message = "invalid redirect location";
            return resp;
        }
        if (!IsSameOrigin(current.url, next_url) || !policy_.IsAllowedOrigin(next_url)) {
            resp.transport_ok = false;
            resp.error_message = "OffOriginRedirect: blocked cross-origin redirect";
            return resp;
        }
        if (hop == max_hops) {
            resp.transport_ok = false;
            resp.error_message = "TooManyRedirects";
            return resp;
        }
        if (code == 301 || code == 302 || code == 303) {
            if (current.method != "GET" && current.method != "HEAD") {
                current.method = "GET";
                current.body.clear();
            }
        }
        current.url = std::move(next_url);
    }

    HttpResponse err;
    err.transport_ok = false;
    err.error_message = "TooManyRedirects";
    return err;
}

}  // namespace academic::tec_digital
