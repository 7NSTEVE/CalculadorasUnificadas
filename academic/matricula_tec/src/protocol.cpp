#include "academic/matricula_tec/protocol.hpp"
#include "academic/matricula_tec/matricula_c_api.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace academic::matricula_tec {

namespace {

std::string Trim(std::string_view sv) {
    size_t b = 0;
    while (b < sv.size() && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
    size_t e = sv.size();
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string(sv.substr(b, e - b));
}

std::string UpperAscii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

std::string DecodeHtmlEntities(std::string s) {
    auto replace_all = [&](std::string_view from, std::string_view to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replace_all("&amp;", "&");
    replace_all("&quot;", "\"");
    replace_all("&#39;", "'");
    replace_all("&lt;", "<");
    replace_all("&gt;", ">");
    return s;
}

std::string ExtractInputValueByIdOrName(std::string_view html, std::string_view key) {
    // Matches <input ... (id|name)="key" ... value="..." ...> in either attribute order
    const std::string escaped_key(key);
    const std::string html_str(html);
    std::regex tag_re(R"(<input\b[^>]*>)", std::regex::icase);
    std::regex id_re(R"((?:id|name)\s*=\s*["']([^"']+)["'])", std::regex::icase);
    std::regex val_re(R"(value\s*=\s*["']([^"']*)["'])", std::regex::icase);

    for (std::sregex_iterator it(html_str.begin(), html_str.end(), tag_re), end; it != end; ++it) {
        const std::string tag = it->str();
        bool key_match = false;
        for (std::sregex_iterator kit(tag.begin(), tag.end(), id_re); kit != end; ++kit) {
            if (_stricmp((*kit)[1].str().c_str(), escaped_key.c_str()) == 0) {
                key_match = true;
                break;
            }
        }
        if (!key_match) continue;
        std::smatch vm;
        if (std::regex_search(tag, vm, val_re)) {
            return DecodeHtmlEntities(vm[1].str());
        }
        return {};
    }
    return {};
}

std::string JsonString(const json& obj, std::initializer_list<const char*> keys, const std::string& def = "") {
    if (!obj.is_object()) return def;
    for (const char* k : keys) {
        auto it = obj.find(k);
        if (it == obj.end() || it->is_null()) continue;
        if (it->is_string()) return Trim(it->get<std::string>());
        if (it->is_number_integer()) return std::to_string(it->get<int64_t>());
        if (it->is_boolean()) return it->get<bool>() ? "true" : "false";
    }
    return def;
}

int JsonInt(const json& obj, std::initializer_list<const char*> keys, int def = 0) {
    if (!obj.is_object()) return def;
    for (const char* k : keys) {
        auto it = obj.find(k);
        if (it == obj.end() || it->is_null()) continue;
        if (it->is_number_integer()) return it->get<int>();
        if (it->is_number_float()) return static_cast<int>(it->get<double>());
        if (it->is_string()) {
            try { return std::stoi(Trim(it->get<std::string>())); } catch (...) {}
        }
    }
    return def;
}

bool JsonBool(const json& obj, std::initializer_list<const char*> keys, bool def = false) {
    if (!obj.is_object()) return def;
    for (const char* k : keys) {
        auto it = obj.find(k);
        if (it == obj.end() || it->is_null()) continue;
        if (it->is_boolean()) return it->get<bool>();
        if (it->is_number_integer()) return it->get<int>() != 0;
        if (it->is_string()) {
            const std::string v = UpperAscii(Trim(it->get<std::string>()));
            if (v == "1" || v == "TRUE" || v == "S" || v == "SI" || v == "M" || v == "YES") return true;
            if (v == "0" || v == "FALSE" || v == "N" || v == "NO") return false;
        }
    }
    return def;
}

ScheduleSession ParseSessionObj(const json& s) {
    ScheduleSession out{};
    const std::string raw_day = JsonString(s, { "Dia", "dia", "NOM_DIA", "IDE_DIA", "numDia", "NUM_DIA" });
    out.day_index = NormalizeTecDay(raw_day);
    out.day_code = DayIndexToCode(out.day_index);
    out.start_text = JsonString(s, { "Inicio", "inicio", "HOR_INICIO", "HoraInicio" });
    out.end_text = JsonString(s, { "Fin", "fin", "HOR_FIN", "HoraFin" });
    out.start_minutes = ParseTimeToMinutes(out.start_text);
    out.end_minutes = ParseTimeToMinutes(out.end_text);
    out.edificio = JsonString(s, { "Edificio", "edificio", "NOM_EDIFICIO" });
    out.aula = JsonString(s, { "Aula", "aula", "NUM_AULA", "COD_AULA" });
    return out;
}

} // namespace

// ============================================================================
// C23-backed codec & schedule helpers
// ============================================================================

std::string StrictPercentEncode(std::string_view input) {
    const size_t needed = matricula_strict_percent_encode(input.data(), input.size(), nullptr, 0);
    std::string out(needed, '\0');
    if (needed > 0) {
        matricula_strict_percent_encode(input.data(), input.size(), out.data(), needed + 1u);
    }
    return out;
}

std::string BuildFormUrlEncoded(const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string out;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) out.push_back('&');
        out += StrictPercentEncode(fields[i].first);
        out.push_back('=');
        out += StrictPercentEncode(fields[i].second);
    }
    return out;
}

int NormalizeTecDay(std::string_view raw_day) {
    return matricula_normalize_tec_day(raw_day.data(), raw_day.size());
}

std::string DayIndexToCode(int day_index) {
    return std::string(matricula_day_index_to_code(day_index));
}

int ParseTimeToMinutes(std::string_view raw_time) {
    return matricula_parse_time_minutes(raw_time.data(), raw_time.size());
}

bool SessionsOverlap(const ScheduleSession& a, const ScheduleSession& b) noexcept {
    return matricula_intervals_overlap(
        a.day_index, a.start_minutes, a.end_minutes,
        b.day_index, b.start_minutes, b.end_minutes);
}

bool SessionVectorsOverlap(const std::vector<ScheduleSession>& a,
                           const std::vector<ScheduleSession>& b) noexcept {
    for (const auto& sa : a) {
        for (const auto& sb : b) {
            if (SessionsOverlap(sa, sb)) return true;
        }
    }
    return false;
}

// ============================================================================
// ASP.NET WebForms & DATIC JSON parsers
// ============================================================================

WebFormsTokens ExtractWebFormsTokens(std::string_view html) {
    WebFormsTokens t{};
    t.view_state = ExtractInputValueByIdOrName(html, "__VIEWSTATE");
    t.view_state_generator = ExtractInputValueByIdOrName(html, "__VIEWSTATEGENERATOR");
    t.event_validation = ExtractInputValueByIdOrName(html, "__EVENTVALIDATION");
    return t;
}

StudentBootstrap ParseFrmMatriculaBootstrap(std::string_view html) {
    StudentBootstrap b{};
    b.carnet = ExtractInputValueByIdOrName(html, "hfCarnet");
    if (b.carnet.empty()) b.carnet = ExtractInputValueByIdOrName(html, "hdnCarnet");
    b.nombre = ExtractInputValueByIdOrName(html, "hfNombre");
    if (b.nombre.empty()) b.nombre = ExtractInputValueByIdOrName(html, "hdnNombre");
    b.carrera_id = ExtractInputValueByIdOrName(html, "hfCarrera");
    b.carrera_nombre = ExtractInputValueByIdOrName(html, "hfNomCarrera");
    b.plan_id = ExtractInputValueByIdOrName(html, "hfPlan");
    b.sede_id = ExtractInputValueByIdOrName(html, "hfSede");
    b.sede_nombre = ExtractInputValueByIdOrName(html, "hfNomSede");
    b.correo = ExtractInputValueByIdOrName(html, "hfCorreo");
    b.periodo_activo = ExtractInputValueByIdOrName(html, "hfPeriodo");
    b.convocatoria_activa = ExtractInputValueByIdOrName(html, "hfConvocatoria");

    // Extract <option value="convId|perId"> or <option value="convId"> from cmbConvocatoria / ddlConvocatoria
    const std::string html_str(html);
    std::regex sel_re(R"(<select\b[^>]*(?:Convocatoria|cmbConvocatoria|ddlConvocatoria)[^>]*>([\s\S]*?)</select>)",
                      std::regex::icase);
    std::smatch sm;
    if (std::regex_search(html_str, sm, sel_re)) {
        const std::string inner = sm[1].str();
        std::regex opt_re(R"(<option\b[^>]*value\s*=\s*["']([^"']+)["'][^>]*>([\s\S]*?)</option>)",
                          std::regex::icase);
        for (std::sregex_iterator it(inner.begin(), inner.end(), opt_re), end; it != end; ++it) {
            const std::string val = DecodeHtmlEntities(Trim((*it)[1].str()));
            const std::string text = DecodeHtmlEntities(Trim((*it)[2].str()));
            if (val.empty() || val == "0" || val == "-1") continue;
            ConvocatoriaInfo ci{};
            const size_t sep = val.find('|');
            if (sep != std::string::npos) {
                ci.id_convocatoria = val.substr(0, sep);
                ci.id_periodo = val.substr(sep + 1);
            } else {
                ci.id_convocatoria = val;
                ci.id_periodo = b.periodo_activo;
            }
            ci.nombre = text;
            b.convocatorias.push_back(std::move(ci));
        }
    }
    if (b.convocatoria_activa.empty() && !b.convocatorias.empty()) {
        b.convocatoria_activa = b.convocatorias.front().id_convocatoria;
        if (b.periodo_activo.empty()) b.periodo_activo = b.convocatorias.front().id_periodo;
    }
    return b;
}

json UnwrapAspNetD(const json& envelope) {
    json cur = envelope;
    if (cur.is_object() && cur.contains("d")) {
        cur = cur["d"];
    }
    if (cur.is_string()) {
        const std::string s = Trim(cur.get<std::string>());
        if (!s.empty() && (s.front() == '{' || s.front() == '[')) {
            try {
                return json::parse(s);
            } catch (...) {
                return cur;
            }
        }
    }
    return cur;
}

std::vector<CourseOffer> ParseCursosPayload(const json& payload) {
    std::vector<CourseOffer> out;
    const json d = UnwrapAspNetD(payload);
    const json* arr = nullptr;
    if (d.is_array()) {
        arr = &d;
    } else if (d.is_object()) {
        for (const char* k : { "Cursos", "cursos", "ListaCursos", "materias", "data", "Table" }) {
            if (d.contains(k) && d[k].is_array()) {
                arr = &d[k];
                break;
            }
        }
    }
    if (!arr) return out;
    for (const auto& item : *arr) {
        if (!item.is_object()) continue;
        CourseOffer c{};
        c.codigo = UpperAscii(JsonString(item, { "COD_MATERIA", "CodMateria", "codigo", "Codigo", "IDE_MATERIA" }));
        if (c.codigo.empty()) continue;
        c.nombre = JsonString(item, { "NOM_MATERIA", "NomMateria", "nombre", "Nombre", "DSC_MATERIA" });
        c.creditos = JsonInt(item, { "CAN_CREDITOS", "Creditos", "creditos" }, 0);
        c.tipo = JsonString(item, { "TIP_MATERIA", "Tipo", "tipo" });
        c.modalidad = JsonString(item, { "NOM_MODALIDAD", "Modalidad", "modalidad" });
        c.estado = JsonString(item, { "EST_MATRICULA", "Estado", "estado" });
        c.matriculado = JsonBool(item, { "Matriculado", "matriculado" }, UpperAscii(c.estado) == "M");
        c.grupo_matriculado = JsonInt(item, { "NUM_GRUPO", "GrupoMatriculado", "grupo" }, -1);
        if (item.contains("Correquisitos") && item["Correquisitos"].is_array()) {
            for (const auto& cr : item["Correquisitos"]) {
                if (cr.is_string()) c.correquisitos.push_back(UpperAscii(cr.get<std::string>()));
                else if (cr.is_object()) {
                    const std::string code = UpperAscii(JsonString(cr, { "COD_MATERIA", "Codigo", "codigo" }));
                    if (!code.empty()) c.correquisitos.push_back(code);
                }
            }
        }
        out.push_back(std::move(c));
    }
    return out;
}

std::vector<CourseGroupLive> ParseHorariosPayload(const json& payload, const std::string& course_code_hint) {
    std::vector<CourseGroupLive> out;
    const json d = UnwrapAspNetD(payload);
    const json* arr = nullptr;
    if (d.is_array()) {
        arr = &d;
    } else if (d.is_object()) {
        for (const char* k : { "Grupos", "grupos", "Horarios", "horarios", "ListaGrupos", "data", "Table" }) {
            if (d.contains(k) && d[k].is_array()) {
                arr = &d[k];
                break;
            }
        }
    }
    if (!arr) return out;
    for (const auto& item : *arr) {
        if (!item.is_object()) continue;
        CourseGroupLive g{};
        g.course_code = UpperAscii(JsonString(item, { "COD_MATERIA", "CodMateria", "course_code", "codigo" }, course_code_hint));
        g.course_name = JsonString(item, { "NOM_MATERIA", "NomMateria", "course_name", "nombre" });
        g.grupo = JsonInt(item, { "NUM_GRUPO", "Grupo", "grupo", "IDE_GRUPO" }, 0);
        g.cupo_maximo = JsonInt(item, { "CAN_CUPO", "CupoMaximo", "cupo_maximo", "CUPO_MAX" }, 0);
        g.matriculados = JsonInt(item, { "CAN_MATRICULADOS", "Matriculados", "matriculados" }, 0);
        if (item.contains("CupoDisponible") || item.contains("cupo_disponible") ||
            item.contains("CAN_DISPONIBLE") || item.contains("Cupo") || item.contains("cupo")) {
            g.cupo_disponible = JsonInt(item, { "CupoDisponible", "cupo_disponible", "CAN_DISPONIBLE", "Cupo", "cupo" }, 0);
        } else {
            g.cupo_disponible = std::max(0, g.cupo_maximo - g.matriculados);
        }
        g.sede_id = JsonString(item, { "IDE_SEDE", "IdSede", "sede_id", "Sede" });
        g.sede_nombre = JsonString(item, { "NOM_SEDE", "NomSede", "sede_nombre", "Sede" }, g.sede_id);
        g.modalidad = JsonString(item, { "NOM_MODALIDAD", "Modalidad", "modalidad" });
        g.tipo_grupo = JsonString(item, { "TIP_GRUPO", "TipoGrupo", "tipo_grupo" });
        g.estado = JsonString(item, { "EST_GRUPO", "Estado", "estado" });
        g.matriculado = JsonBool(item, { "Matriculado", "matriculado" }, false);

        if (item.contains("Profesores") && item["Profesores"].is_array()) {
            for (const auto& pr : item["Profesores"]) {
                if (pr.is_string()) g.profesores.push_back(Trim(pr.get<std::string>()));
                else if (pr.is_object()) {
                    const std::string nm = JsonString(pr, { "NOM_PROFESOR", "Nombre", "nombre" });
                    if (!nm.empty()) g.profesores.push_back(nm);
                }
            }
        } else {
            const std::string prof = JsonString(item, { "NOM_PROFESOR", "Profesor", "profesor" });
            if (!prof.empty()) g.profesores.push_back(prof);
        }

        const json* sched_arr = nullptr;
        for (const char* sk : { "Horarios", "horarios", "Sesiones", "sesiones", "Horario" }) {
            if (item.contains(sk) && item[sk].is_array()) {
                sched_arr = &item[sk];
                break;
            }
        }
        if (sched_arr) {
            for (const auto& s : *sched_arr) {
                if (!s.is_object()) continue;
                ScheduleSession ss = ParseSessionObj(s);
                if (ss.day_index >= 0 && ss.start_minutes >= 0 && ss.end_minutes > ss.start_minutes) {
                    g.sesiones.push_back(std::move(ss));
                }
            }
        } else if (item.contains("Dia") || item.contains("NOM_DIA")) {
            ScheduleSession ss = ParseSessionObj(item);
            if (ss.day_index >= 0 && ss.start_minutes >= 0 && ss.end_minutes > ss.start_minutes) {
                g.sesiones.push_back(std::move(ss));
            }
        }
        out.push_back(std::move(g));
    }
    return out;
}

CitaInfo ParseCitaPayload(const json& payload) {
    CitaInfo c{};
    const json d = UnwrapAspNetD(payload);
    if (d.is_string()) {
        c.raw_text = Trim(d.get<std::string>());
        c.tiene_cita = !c.raw_text.empty() && !UpperAscii(c.raw_text).contains("NO TIENE");
        return c;
    }
    const json* obj = nullptr;
    if (d.is_object()) {
        obj = &d;
    } else if (d.is_array() && !d.empty() && d[0].is_object()) {
        obj = &d[0];
    }
    if (!obj) return c;

    c.fecha = JsonString(*obj, { "FEC_CITA", "Fecha", "fecha", "FechaCita" });
    c.hora_inicio = JsonString(*obj, { "HOR_INICIO", "HoraInicio", "hora_inicio", "Hora" });
    c.hora_fin = JsonString(*obj, { "HOR_FIN", "HoraFin", "hora_fin" });
    c.convocatoria = JsonString(*obj, { "NOM_CONVOCATORIA", "Convocatoria", "convocatoria" });
    c.sede = JsonString(*obj, { "NOM_SEDE", "Sede", "sede" });
    c.raw_text = obj->dump();
    c.cita_epoch_utc = obj->contains("cita_epoch_utc") && (*obj)["cita_epoch_utc"].is_number_integer()
                           ? (*obj)["cita_epoch_utc"].get<int64_t>()
                           : 0;
    c.cita_activa_ahora = JsonBool(*obj, { "Activa", "cita_activa_ahora", "Habilitada" }, false);
    c.tiene_cita = JsonBool(*obj, { "TieneCita", "tiene_cita" }, !c.fecha.empty() || !c.hora_inicio.empty());
    return c;
}

MorosidadInfo ParseMorosidadPayload(const json& payload) {
    MorosidadInfo m{};
    const json d = UnwrapAspNetD(payload);
    if (d.is_array()) {
        for (const auto& item : d) {
            if (item.is_string()) {
                const std::string s = Trim(item.get<std::string>());
                if (!s.empty()) m.detalles.push_back(s);
            } else if (item.is_object()) {
                const std::string desc = JsonString(item, { "DSC_MOROSIDAD", "Descripcion", "Detalle", "mensaje" });
                if (!desc.empty()) m.detalles.push_back(desc);
            }
        }
        m.tiene_morosidad = !m.detalles.empty();
        return m;
    }
    if (d.is_object()) {
        m.tiene_morosidad = JsonBool(d, { "TieneMorosidad", "tiene_morosidad", "Moroso" }, false);
        if (d.contains("Detalles") && d["Detalles"].is_array()) {
            for (const auto& x : d["Detalles"]) {
                if (x.is_string()) m.detalles.push_back(x.get<std::string>());
            }
        }
        if (!m.detalles.empty()) m.tiene_morosidad = true;
    } else if (d.is_string()) {
        const std::string s = Trim(d.get<std::string>());
        if (!s.empty() && !UpperAscii(s).contains("SIN MOROSIDAD") && UpperAscii(s) != "OK") {
            m.tiene_morosidad = true;
            m.detalles.push_back(s);
        }
    }
    return m;
}

std::vector<EnrolledScheduleItem> ParseHorarioMatriculadoPayload(const json& payload) {
    std::vector<EnrolledScheduleItem> out;
    const json d = UnwrapAspNetD(payload);
    const json* arr = nullptr;
    if (d.is_array()) arr = &d;
    else if (d.is_object()) {
        for (const char* k : { "Horario", "horario", "Cursos", "cursos", "Matriculados", "data" }) {
            if (d.contains(k) && d[k].is_array()) {
                arr = &d[k];
                break;
            }
        }
    }
    if (!arr) return out;
    for (const auto& item : *arr) {
        if (!item.is_object()) continue;
        EnrolledScheduleItem e{};
        e.course_code = UpperAscii(JsonString(item, { "COD_MATERIA", "CodMateria", "course_code", "codigo" }));
        e.course_name = JsonString(item, { "NOM_MATERIA", "NomMateria", "course_name", "nombre" });
        e.grupo = JsonInt(item, { "NUM_GRUPO", "Grupo", "grupo" }, 0);
        e.creditos = JsonInt(item, { "CAN_CREDITOS", "Creditos", "creditos" }, 0);
        e.sede = JsonString(item, { "NOM_SEDE", "Sede", "sede" });
        if (item.contains("Sesiones") && item["Sesiones"].is_array()) {
            for (const auto& s : item["Sesiones"]) e.sesiones.push_back(ParseSessionObj(s));
        } else if (item.contains("Horarios") && item["Horarios"].is_array()) {
            for (const auto& s : item["Horarios"]) e.sesiones.push_back(ParseSessionObj(s));
        } else if (item.contains("Dia") || item.contains("NUM_DIA") || item.contains("IDE_DIA")) {
            e.sesiones.push_back(ParseSessionObj(item));
        }
        if (!e.course_code.empty()) out.push_back(std::move(e));
    }
    return out;
}

InformeSummary ParseGeneraInformePayload(const json& payload) {
    InformeSummary inf{};
    const json d = UnwrapAspNetD(payload);
    if (d.is_string()) {
        inf.html_informe = d.get<std::string>();
        inf.ok = !inf.html_informe.empty();
        return inf;
    }
    if (d.is_object()) {
        inf.html_informe = JsonString(d, { "htmlInforme", "HtmlInforme", "html", "Informe", "informe" });
        inf.correo_destino = JsonString(d, { "correo", "Correo", "email" });
        inf.cursos_count = JsonInt(d, { "cursos_count", "CantidadCursos" }, 0);
        inf.creditos_total = JsonInt(d, { "creditos_total", "TotalCreditos" }, 0);
        inf.ok = !inf.html_informe.empty();
    }
    return inf;
}

EnrollmentResult ParseMutationPayload(const json& payload,
                                      const std::string& action,
                                      const std::string& course_code,
                                      int grupo)
{
    EnrollmentResult r{};
    r.dry_run = false;
    r.transport_used = "api";
    r.action = action;
    r.course_code = UpperAscii(course_code);
    r.grupo = grupo;

    const json d = UnwrapAspNetD(payload);
    std::string raw;
    if (d.is_string()) {
        raw = Trim(d.get<std::string>());
    } else if (d.is_boolean()) {
        r.ok = d.get<bool>();
        r.datic_code = r.ok ? "OK" : "ERROR";
        r.message = r.ok ? "Operacion confirmada por el servidor TEC." : "El servidor rechazo la operacion.";
        return r;
    } else if (d.is_object()) {
        raw = JsonString(d, { "Codigo", "codigo", "Estado", "estado", "Resultado", "resultado", "Message", "message", "datic_code" });
        r.message = JsonString(d, { "Mensaje", "mensaje", "Message", "message", "Detalle" }, raw);
        if (d.contains("ok") && d["ok"].is_boolean()) {
            r.ok = d["ok"].get<bool>();
            r.datic_code = raw.empty() ? (r.ok ? "OK" : "ERROR") : raw;
            return r;
        }
    } else {
        raw = d.dump();
    }

    const std::string up = UpperAscii(raw);
    r.datic_code = raw.empty() ? "UNKNOWN" : raw;
    if (r.message.empty()) r.message = raw;

    if (up == "OK" || up == "EXITO" || up == "TRUE" || up == "1" || up.contains("MATRICULADO") || up.contains("ENVIADO")) {
        r.ok = true;
        if (r.datic_code.empty()) r.datic_code = "OK";
    } else if (up.contains("SCUPO") || up.contains("SIN CUPO")) {
        r.ok = false;
        r.datic_code = "SCupo";
    } else if (up.contains("CCHOQUE") || up.contains("CHOQUE")) {
        r.ok = false;
        r.datic_code = "CChoque";
    } else if (up.contains("SMATERIAS")) {
        r.ok = false;
        r.datic_code = "SMaterias";
    } else if (up.contains("COREQUISITO") || up.contains("CORREQUISITO")) {
        r.ok = false;
        r.requires_corequisite = true;
        r.datic_code = "Corequisito";
    } else if (up.contains("OTRASEDE") || up.contains("OTRA SEDE")) {
        r.ok = false;
        r.requires_campus_cancel = true;
        r.datic_code = "OtraSede";
    } else {
        r.ok = false;
    }
    return r;
}

EnrollmentResult SimulateDryRunEnroll(const CourseGroupLive& candidate,
                                      const std::vector<EnrolledScheduleItem>& currently_enrolled)
{
    EnrollmentResult r{};
    r.dry_run = true;
    r.transport_used = "dry_run";
    r.action = "enroll";
    r.course_code = UpperAscii(candidate.course_code);
    r.grupo = candidate.grupo;

    if (candidate.cupo_disponible <= 0) {
        r.ok = false;
        r.datic_code = "SCupo";
        r.message = "[DRY-RUN] Sin cupo disponible en " + r.course_code + " grupo " + std::to_string(r.grupo);
        return r;
    }
    for (const auto& en : currently_enrolled) {
        if (UpperAscii(en.course_code) == r.course_code) {
            r.ok = false;
            r.datic_code = "YaMatriculado";
            r.message = "[DRY-RUN] Curso " + r.course_code + " ya matriculado en grupo " + std::to_string(en.grupo);
            return r;
        }
        if (SessionVectorsOverlap(candidate.sesiones, en.sesiones)) {
            r.ok = false;
            r.datic_code = "CChoque";
            r.message = "[DRY-RUN] Choque horario detectado con " + en.course_code + " grupo " + std::to_string(en.grupo);
            return r;
        }
    }
    r.ok = true;
    r.datic_code = "DRY_RUN_OK";
    r.message = "[DRY-RUN] Preflight valido para " + r.course_code + " G" + std::to_string(r.grupo) +
                " (cupos=" + std::to_string(candidate.cupo_disponible) + ")";
    return r;
}

std::string RedactSecrets(std::string text, const std::vector<std::string>& secrets) {
    for (const auto& sec : secrets) {
        if (sec.size() < 3) continue;
        size_t pos = 0;
        while ((pos = text.find(sec, pos)) != std::string::npos) {
            text.replace(pos, sec.size(), "<REDACTED>");
            pos += 10;
        }
    }
    return text;
}

// ============================================================================
// Models JSON persistence (compatible with %LOCALAPPDATA%\CuadernU\matricula_scenarios.json)
// ============================================================================

json ScheduleFilterToJson(const ScheduleFilterRule& f) {
    return json{
        { "allowed_groups", f.allowed_groups },
        { "excluded_groups", f.excluded_groups },
        { "allowed_days", f.allowed_days },
        { "excluded_days", f.excluded_days },
        { "earliest_start_minutes", f.earliest_start_minutes },
        { "latest_end_minutes", f.latest_end_minutes },
        { "allowed_campuses", f.allowed_campuses },
        { "preferred_professors", f.preferred_professors },
        { "excluded_professors", f.excluded_professors },
        { "require_seats", f.require_seats }
    };
}

ScheduleFilterRule ScheduleFilterFromJson(const json& j) {
    ScheduleFilterRule f{};
    if (!j.is_object()) return f;
    if (j.contains("allowed_groups") && j["allowed_groups"].is_array()) f.allowed_groups = j["allowed_groups"].get<std::vector<int>>();
    if (j.contains("excluded_groups") && j["excluded_groups"].is_array()) f.excluded_groups = j["excluded_groups"].get<std::vector<int>>();
    if (j.contains("allowed_days") && j["allowed_days"].is_array()) {
        for (const auto& d : j["allowed_days"]) {
            if (d.is_number_integer()) f.allowed_days.push_back(d.get<int>());
            else if (d.is_string()) {
                int idx = NormalizeTecDay(d.get<std::string>());
                if (idx >= 0) f.allowed_days.push_back(idx);
            }
        }
    }
    if (j.contains("excluded_days") && j["excluded_days"].is_array()) {
        for (const auto& d : j["excluded_days"]) {
            if (d.is_number_integer()) f.excluded_days.push_back(d.get<int>());
            else if (d.is_string()) {
                int idx = NormalizeTecDay(d.get<std::string>());
                if (idx >= 0) f.excluded_days.push_back(idx);
            }
        }
    }
    f.earliest_start_minutes = j.value("earliest_start_minutes", 0);
    f.latest_end_minutes = j.value("latest_end_minutes", 23 * 60 + 59);
    if (j.contains("allowed_campuses") && j["allowed_campuses"].is_array()) f.allowed_campuses = j["allowed_campuses"].get<std::vector<std::string>>();
    if (j.contains("preferred_professors") && j["preferred_professors"].is_array()) f.preferred_professors = j["preferred_professors"].get<std::vector<std::string>>();
    if (j.contains("excluded_professors") && j["excluded_professors"].is_array()) f.excluded_professors = j["excluded_professors"].get<std::vector<std::string>>();
    f.require_seats = j.value("require_seats", true);
    return f;
}

json ScenarioPlanToJson(const ScenarioPlan& plan) {
    json sc_arr = json::array();
    for (const auto& sc : plan.scenarios) {
        json t_arr = json::array();
        for (const auto& t : sc.targets) {
            t_arr.push_back(json{
                { "course_code", t.course_code },
                { "course_name", t.course_name },
                { "preferred_groups", t.preferred_groups },
                { "filter", ScheduleFilterToJson(t.filter) },
                { "optional_course", t.optional_course },
                { "corequisite_code", t.corequisite_code },
                { "corequisite_group", t.corequisite_group }
            });
        }
        sc_arr.push_back(json{
            { "id", sc.id },
            { "name", sc.name },
            { "priority", sc.priority },
            { "enabled", sc.enabled },
            { "targets", std::move(t_arr) }
        });
    }
    return json{
        { "convocatoria_id", plan.convocatoria_id },
        { "periodo_id", plan.periodo_id },
        { "sede_id", plan.sede_id },
        { "enable_plan_z_rescue", plan.enable_plan_z_rescue },
        { "pivot_policy", plan.pivot_policy == PivotPolicy::AllowPriorityScenarioReplace ? "AllowPriorityScenarioReplace" : "StrictlyConservative" },
        { "min_enrolled_courses_safety_floor", plan.min_enrolled_courses_safety_floor },
        { "auto_send_informe_on_success", plan.auto_send_informe_on_success },
        { "scenarios", std::move(sc_arr) }
    };
}

ScenarioPlan ScenarioPlanFromJson(const json& j) {
    ScenarioPlan p{};
    if (!j.is_object()) return p;
    p.convocatoria_id = j.value("convocatoria_id", "");
    p.periodo_id = j.value("periodo_id", "");
    p.sede_id = j.value("sede_id", "");
    p.enable_plan_z_rescue = j.value("enable_plan_z_rescue", true);
    const std::string pol = j.value("pivot_policy", "StrictlyConservative");
    p.pivot_policy = (pol == "AllowPriorityScenarioReplace")
                         ? PivotPolicy::AllowPriorityScenarioReplace
                         : PivotPolicy::StrictlyConservative;
    p.min_enrolled_courses_safety_floor = std::max(1, j.value("min_enrolled_courses_safety_floor", 1));
    p.auto_send_informe_on_success = j.value("auto_send_informe_on_success", true);

    if (j.contains("scenarios") && j["scenarios"].is_array()) {
        for (const auto& scj : j["scenarios"]) {
            if (!scj.is_object()) continue;
            EnrollmentScenario sc{};
            sc.id = scj.value("id", "");
            sc.name = scj.value("name", sc.id);
            sc.priority = scj.value("priority", static_cast<int>(p.scenarios.size() + 1));
            sc.enabled = scj.value("enabled", true);
            if (scj.contains("targets") && scj["targets"].is_array()) {
                for (const auto& tj : scj["targets"]) {
                    if (!tj.is_object()) continue;
                    CourseTarget t{};
                    t.course_code = UpperAscii(tj.value("course_code", ""));
                    t.course_name = tj.value("course_name", "");
                    if (tj.contains("preferred_groups") && tj["preferred_groups"].is_array()) {
                        t.preferred_groups = tj["preferred_groups"].get<std::vector<int>>();
                    }
                    if (tj.contains("filter")) t.filter = ScheduleFilterFromJson(tj["filter"]);
                    t.optional_course = tj.value("optional_course", false);
                    t.corequisite_code = UpperAscii(tj.value("corequisite_code", ""));
                    t.corequisite_group = tj.value("corequisite_group", -1);
                    if (!t.course_code.empty()) sc.targets.push_back(std::move(t));
                }
            }
            p.scenarios.push_back(std::move(sc));
        }
    }
    return p;
}

bool SaveScenarioPlanToFile(const ScenarioPlan& plan, const std::string& path_utf8, std::string* out_err) {
    try {
        const fs::path p = fs::u8path(path_utf8);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            if (out_err) *out_err = "No se pudo abrir archivo para guardar escenarios.";
            return false;
        }
        const std::string text = ScenarioPlanToJson(plan).dump(2);
        ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(ofs);
    } catch (const std::exception& ex) {
        if (out_err) *out_err = ex.what();
        return false;
    }
}

bool LoadScenarioPlanFromFile(const std::string& path_utf8, ScenarioPlan& out_plan, std::string* out_err) {
    try {
        const fs::path p = fs::u8path(path_utf8);
        std::ifstream ifs(p, std::ios::binary);
        if (!ifs) {
            if (out_err) *out_err = "Archivo de escenarios no encontrado.";
            return false;
        }
        std::ostringstream ss;
        ss << ifs.rdbuf();
        out_plan = ScenarioPlanFromJson(json::parse(ss.str()));
        return true;
    } catch (const std::exception& ex) {
        if (out_err) *out_err = ex.what();
        return false;
    }
}

std::string DefaultCuadernuDataDirUtf8() {
    auto read_env = [](const char* name) -> std::string {
#ifdef _WIN32
        char* buf = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr) {
            std::string val(buf);
            std::free(buf);
            return val;
        }
        return {};
#else
        const char* v = std::getenv(name);
        return v ? std::string(v) : std::string{};
#endif
    };
    if (auto la = read_env("LOCALAPPDATA"); !la.empty()) {
        return (fs::path(la) / "CuadernU").string();
    }
    if (auto up = read_env("USERPROFILE"); !up.empty()) {
        return (fs::path(up) / "AppData" / "Local" / "CuadernU").string();
    }
    return ".";
}

std::string DefaultVaultPathUtf8() {
    return (fs::path(DefaultCuadernuDataDirUtf8()) / "matricula_vault.dpapi").string();
}

std::string DefaultScenariosPathUtf8() {
    return (fs::path(DefaultCuadernuDataDirUtf8()) / "matricula_scenarios.json").string();
}

} // namespace academic::matricula_tec
