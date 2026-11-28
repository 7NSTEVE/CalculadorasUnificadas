#include "academic/tec_mail_triage/triage_engine.hpp"
#include "academic/tec_mail_triage/triage_c_api.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>

namespace academic::tec_mail_triage {

namespace {

constexpr std::array<const char*, 21> kRemitentesRuido = {
    "noreply", "no-reply", "donotreply", "do-not-reply", "notificacion",
    "notificaciones", "boletin", "newsletter", "comunicacion", "comunicaciones",
    "marketing", "publicidad", "difusion", "mailer", "bounce", "automatico",
    "informacion", "info", "eventos", "promociones", "promos"
};

constexpr std::array<const char*, 24> kAsuntoRuido = {
    "boletin", "newsletter", "webinar", "feria", "invitamos", "te invitamos",
    "concurso", "promocion", "descuento", "sorteo", "aniversario", "celebracion",
    "campana", "torneo", "no responder", "encuesta de satisfaccion",
    "club", "coro", "banda", "cineforo", "yoga", "zumba", "premio", "ganaste"
};

constexpr std::array<const char*, 20> kAccionFuerte = {
    "entrega", "entregar", "entregable", "tarea", "asignacion", "examen",
    "quiz", "prueba corta", "evaluacion", "proyecto", "laboratorio", "informe",
    "defensa", "matricula", "inscripcion", "convocatoria", "prorroga",
    "retiro de curso", "obligatorio", "requisito"
};

constexpr std::array<const char*, 14> kAccionMedia = {
    "formulario", "presentacion", "reporte", "giras", "gira", "practica",
    "recordatorio", "pendiente", "correccion", "reposicion", "nota",
    "calificacion", "rubrica", "enunciado"
};

constexpr std::array<const char*, 15> kPalabrasFecha = {
    "fecha", "plazo", "vence", "vencimiento", "limite", "hasta el",
    "a mas tardar", "deadline", "reprogramacion", "cambio de fecha",
    "posterga", "aplazado", "se traslada", "se adelanta", "cronograma"
};

constexpr std::array<const char*, 3> kDominiosTec = {
    "itcr.ac.cr", "tec.ac.cr", "estudiantec.cr"
};

constexpr std::array<const char*, 12> kMesesCorto = {
    "ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "set", "oct", "nov", "dic"
};

constexpr int kGraciaPasadoDias = 45;

// Howard Hinnant civil-to-days algorithm (exact proleptic Gregorian day count)
int64_t CivilToDays(int y, int m, int d) noexcept {
    y -= (m <= 2) ? 1 : 0;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

std::string LowerAscii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string UpperAscii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

std::string JoinMax(const std::vector<std::string>& items, size_t max_items) {
    std::string out;
    const size_t n = std::min(items.size(), max_items);
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) out += ", ";
        out += items[i];
    }
    return out;
}

template <size_t N>
std::vector<std::string> CountHits(const std::string& norm_text, const std::array<const char*, N>& terms) {
    std::vector<std::string> hits;
    for (const char* k : terms) {
        if (norm_text.contains(k)) hits.emplace_back(k);
    }
    return hits;
}

std::string LocalPart(std::string_view smtp) {
    const std::string low = LowerAscii(smtp);
    const size_t at = low.find('@');
    return (at == std::string::npos) ? low : low.substr(0, at);
}

std::string DomainPart(std::string_view smtp) {
    const std::string low = LowerAscii(smtp);
    const size_t at = low.find('@');
    return (at == std::string::npos) ? std::string{} : low.substr(at + 1);
}

bool TryMakeDate(int y, int m, int d, LocalDateTime& out) noexcept {
    const int dim = tec_days_in_month(y, m);
    if (dim == 0 || d < 1 || d > dim) return false;
    out = LocalDateTime{ y, m, d, 0, 0 };
    return true;
}

int InferYear(int month, int day, const LocalDateTime& received, std::optional<int> explicit_year) noexcept {
    if (explicit_year.has_value()) {
        const int a = *explicit_year;
        return (a < 100) ? (2000 + a) : a;
    }
    int y = received.year;
    LocalDateTime cand{};
    if (!TryMakeDate(y, month, day, cand)) return y;
    if (cand.ToDayNumber() < received.ToDayNumber() - kGraciaPasadoDias) {
        ++y;
    }
    return y;
}

std::optional<std::pair<int, int>> FindNearbyTime(const std::string& norm, size_t idx) {
    static const std::regex kReHora(R"((\d{1,2}):(\d{2})\s*(a\.?m\.?|p\.?m\.?)?)", std::regex::icase);
    const size_t end = std::min(norm.size(), idx + 90u);
    const std::string window = norm.substr(idx, end - idx);
    std::smatch m;
    if (!std::regex_search(window, m, kReHora)) return std::nullopt;
    int hh = std::stoi(m[1].str());
    int mm = std::stoi(m[2].str());
    if (hh > 23 || mm > 59) return std::nullopt;
    std::string mer = LowerAscii(m[3].str());
    std::erase(mer, '.');
    if (mer == "pm" && hh < 12) hh += 12;
    if (mer == "am" && hh == 12) hh = 0;
    return std::make_pair(hh, mm);
}

std::string MakeContextSnippet(const std::string& raw, size_t idx, size_t len) {
    const size_t safe_idx = std::min(idx, raw.size());
    size_t ini = (safe_idx > 45u) ? (safe_idx - 45u) : 0u;
    size_t fin = std::min(raw.size(), safe_idx + len + 45u);
    if (ini > 0) {
        const size_t sp = raw.find(' ', ini);
        if (sp != std::string::npos && sp < safe_idx) ini = sp + 1;
    }
    if (fin < raw.size()) {
        const size_t sp = raw.rfind(' ', fin);
        if (sp != std::string::npos && sp > safe_idx + len) fin = sp;
    }
    std::string slice = raw.substr(ini, fin - ini);
    std::string collapsed;
    bool prev_sp = true;
    for (char c : slice) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!prev_sp) collapsed.push_back(' ');
            prev_sp = true;
        } else {
            collapsed.push_back(c);
            prev_sp = false;
        }
    }
    while (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
    std::string out;
    if (ini > 0) out += "...";
    out += collapsed;
    if (fin < raw.size()) out += "...";
    return out;
}

} // namespace

int64_t LocalDateTime::ToDayNumber() const noexcept {
    return CivilToDays(year, month, day);
}

int64_t LocalDateTime::ToMinutesKey() const noexcept {
    return ToDayNumber() * 1440 + static_cast<int64_t>(hour) * 60 + minute;
}

std::string LocalDateTime::FormatYmd() const {
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    return std::string(buf);
}

std::string LocalDateTime::FormatHhmm() const {
    char buf[8]{};
    std::snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    return std::string(buf);
}

std::string MailGroupToString(MailGroup g) {
    switch (g) {
        case MailGroup::Fecha: return "Con fecha";
        case MailGroup::Accion: return "Requiere accion";
        case MailGroup::Info: return "Informativo";
        case MailGroup::Ruido: return "Ruido/masivo";
    }
    return "Ruido/masivo";
}

std::string UrgencyToString(UrgencyLevel u) {
    switch (u) {
        case UrgencyLevel::Vencido: return "Vencido";
        case UrgencyLevel::Urgente: return "Urgente";
        case UrgencyLevel::Pronto: return "Pronto";
        case UrgencyLevel::Ninguna: return "Ninguna";
    }
    return "Ninguna";
}

std::string MailTriageEngine::NormalizeEsAscii(std::string_view input) {
    if (input.empty()) return {};
    std::string out(input.size() + 1u, '\0');
    const size_t written = tec_normalize_es_ascii(input.data(), input.size(), out.data(), out.size());
    out.resize(written);
    return out;
}

bool MailTriageEngine::LooksHumanSender(std::string_view smtp) {
    static const std::regex kReNombreApellido(R"(^[a-z]+\.[a-z]+$)");
    static const std::regex kReUsuarioCorto(R"(^[a-z]{4,12}$)");
    const std::string lp = LocalPart(smtp);
    if (lp.empty()) return false;
    for (const char* r : kRemitentesRuido) {
        if (lp.contains(r)) return false;
    }
    return std::regex_match(lp, kReNombreApellido) || std::regex_match(lp, kReUsuarioCorto);
}

std::vector<std::string> MailTriageEngine::DetectCourseCodes(std::string_view subject, std::string_view body) {
    static const std::regex kReCurso(R"(\b([A-Z]{2})[-\s]?(\d{3,4})\b)");
    std::vector<std::string> cursos;
    for (const std::string src : { UpperAscii(subject), UpperAscii(body) }) {
        for (std::sregex_iterator it(src.begin(), src.end(), kReCurso), end; it != end; ++it) {
            const std::string code = (*it)[1].str() + (*it)[2].str();
            if (!std::ranges::contains(cursos, code)) {
                cursos.push_back(code);
            }
        }
    }
    return cursos;
}

std::vector<DetectedDate> MailTriageEngine::ExtractSpanishDates(std::string_view text, const LocalDateTime& received) {
    static const std::regex kReTextual(
        R"(\b(\d{1,2})\s*(?:de\s+|-|/)?\s*(enero|ene|febrero|feb|marzo|mar|abril|abr|mayo|may|junio|jun|julio|jul|agosto|ago|septiembre|sep|sept|setiembre|set|octubre|oct|noviembre|nov|diciembre|dic)(?:\s*(?:de|del|-|/)\s*(\d{4}))?)");
    static const std::regex kReNumerica(R"(\b(\d{1,2})[/\-](\d{1,2})(?:[/\-](\d{2,4}))?\b)");

    const std::string raw(text);
    const std::string norm = NormalizeEsAscii(raw);
    std::set<std::string> seen;
    std::vector<DetectedDate> out;

    auto add_date = [&](const LocalDateTime& dt, size_t idx, size_t len, bool has_time) {
        const std::string ymd = dt.FormatYmd();
        const std::string hhmm = has_time ? dt.FormatHhmm() : "";
        const std::string key = ymd + hhmm;
        if (!seen.insert(key).second) return;
        out.push_back(DetectedDate{
            dt,
            has_time,
            ymd,
            hhmm,
            MakeContextSnippet(raw, idx, len)
        });
    };

    for (std::sregex_iterator it(norm.begin(), norm.end(), kReTextual), end; it != end; ++it) {
        const int dia = std::stoi((*it)[1].str());
        const std::string mtok = (*it)[2].str();
        const int mes = tec_parse_spanish_month(mtok.data(), mtok.size());
        std::optional<int> exp_y;
        if ((*it)[3].matched) exp_y = std::stoi((*it)[3].str());
        const int anio = InferYear(mes, dia, received, exp_y);
        LocalDateTime dt{};
        if (!TryMakeDate(anio, mes, dia, dt)) continue;
        const auto tm = FindNearbyTime(norm, static_cast<size_t>(it->position()));
        if (tm.has_value()) {
            dt.hour = tm->first;
            dt.minute = tm->second;
        }
        add_date(dt, static_cast<size_t>(it->position()), static_cast<size_t>(it->length()), tm.has_value());
    }

    for (std::sregex_iterator it(norm.begin(), norm.end(), kReNumerica), end; it != end; ++it) {
        const int dia = std::stoi((*it)[1].str());
        const int mes = std::stoi((*it)[2].str());
        if (mes < 1 || mes > 12) continue;
        std::optional<int> exp_y;
        if ((*it)[3].matched) exp_y = std::stoi((*it)[3].str());
        const int anio = InferYear(mes, dia, received, exp_y);
        LocalDateTime dt{};
        if (!TryMakeDate(anio, mes, dia, dt)) continue;
        const auto tm = FindNearbyTime(norm, static_cast<size_t>(it->position()));
        if (tm.has_value()) {
            dt.hour = tm->first;
            dt.minute = tm->second;
        }
        add_date(dt, static_cast<size_t>(it->position()), static_cast<size_t>(it->length()), tm.has_value());
    }

    std::ranges::sort(out, [](const DetectedDate& a, const DetectedDate& b) {
        return a.instant.ToMinutesKey() < b.instant.ToMinutesKey();
    });
    return out;
}

MailMessage MailTriageEngine::Classify(MailMessage c, const LocalDateTime& now) {
    const std::string nAsunto = NormalizeEsAscii(c.subject);
    const std::string nCuerpo = NormalizeEsAscii(c.body);
    const std::string nTodo = nAsunto + " \n " + nCuerpo;
    const std::string smtp = LowerAscii(c.sender_smtp);
    const std::string dominio = DomainPart(smtp);
    const std::string lp = LocalPart(smtp);

    int puntaje = 0;
    std::vector<std::string> razones;

    if (c.has_list_unsubscribe) {
        puntaje -= 45;
        razones.emplace_back("Trae List-Unsubscribe: es correo masivo");
    }

    std::vector<std::string> rem_ruido;
    for (const char* r : kRemitentesRuido) {
        if (lp.contains(r)) rem_ruido.emplace_back(r);
    }
    if (!rem_ruido.empty()) {
        puntaje -= 30;
        razones.push_back("Remitente automatico (" + rem_ruido.front() + ")");
    }

    const auto asunto_ruido = CountHits(nAsunto, kAsuntoRuido);
    if (!asunto_ruido.empty()) {
        puntaje -= 12 * static_cast<int>(std::min<size_t>(asunto_ruido.size(), 2u));
        razones.push_back("Asunto de difusion (" + JoinMax(asunto_ruido, 2) + ")");
    }

    const auto fuertes = CountHits(nTodo, kAccionFuerte);
    if (!fuertes.empty()) {
        puntaje += std::min(30, 15 * static_cast<int>(fuertes.size()));
        razones.push_back("Palabras de accion (" + JoinMax(fuertes, 3) + ")");
    }

    const auto medias = CountHits(nTodo, kAccionMedia);
    if (!medias.empty()) {
        puntaje += std::min(15, 6 * static_cast<int>(medias.size()));
        razones.push_back("Terminos de curso (" + JoinMax(medias, 3) + ")");
    }

    const auto pal_fecha = CountHits(nTodo, kPalabrasFecha);
    if (!pal_fecha.empty()) {
        puntaje += 18;
        razones.push_back("Menciona plazo (" + JoinMax(pal_fecha, 2) + ")");
    }

    const auto cursos = DetectCourseCodes(c.subject, c.body);
    if (!cursos.empty()) {
        puntaje += 22;
        razones.push_back("Codigo de curso: " + JoinMax(cursos, cursos.size()));
    }

    const bool is_tec_domain = std::ranges::any_of(kDominiosTec, [&](const char* d) { return dominio == d; });
    if (is_tec_domain && LooksHumanSender(smtp) && !c.has_list_unsubscribe) {
        puntaje += 22;
        razones.emplace_back("Escrito por una persona del TEC");
    }

    if (c.is_meeting) {
        puntaje += 15;
        razones.emplace_back("Es una invitacion de calendario");
    }
    if (c.has_attachment) puntaje += 5;
    if (c.unread) puntaje += 4;

    const LocalDateTime ref = c.received.value_or(now);
    const auto all_dates = ExtractSpanishDates(c.subject + "\n" + c.body, ref);
    const int64_t cutoff_min = now.ToMinutesKey() - 12 * 60;
    std::vector<DetectedDate> futuras;
    for (const auto& d : all_dates) {
        if (d.instant.ToMinutesKey() >= cutoff_min) futuras.push_back(d);
    }

    MailGroup grupo = MailGroup::Ruido;
    if (!futuras.empty() && puntaje >= 20) grupo = MailGroup::Fecha;
    else if (puntaje >= 38) grupo = MailGroup::Accion;
    else if (puntaje >= 8) grupo = MailGroup::Info;
    else grupo = MailGroup::Ruido;

    // Being in Junk/Spam NEVER reduces score; important messages are rescued transparently.
    const bool rescatado = c.in_spam && (grupo == MailGroup::Fecha || grupo == MailGroup::Accion);
    if (c.in_spam) {
        razones.emplace_back(rescatado
            ? "Estaba en Correo no deseado y parece importante: rescatado"
            : "Estaba en Correo no deseado");
    }

    c.group = grupo;
    c.rescued_from_spam = rescatado;
    c.score = puntaje;
    c.reasons = std::move(razones);
    c.courses = cursos;
    c.dates = std::move(futuras);
    return c;
}

int MailTriageEngine::DaysUntil(const LocalDateTime& target, const LocalDateTime& now) noexcept {
    return static_cast<int>(target.ToDayNumber() - now.ToDayNumber());
}

UrgencyLevel MailTriageEngine::UrgencyOf(const LocalDateTime& target, const LocalDateTime& now) noexcept {
    const int d = DaysUntil(target, now);
    if (d < 0) return UrgencyLevel::Vencido;
    if (d <= 3) return UrgencyLevel::Urgente;
    if (d <= 7) return UrgencyLevel::Pronto;
    return UrgencyLevel::Ninguna;
}

std::string MailTriageEngine::FormatWhen(const DetectedDate& d, const LocalDateTime& now) {
    const int n = DaysUntil(d.instant, now);
    const std::string hora = d.has_time ? (" · " + d.hhmm) : "";
    const int m_idx = std::clamp(d.instant.month - 1, 0, 11);
    const std::string dia = std::to_string(d.instant.day) + " " + kMesesCorto[static_cast<size_t>(m_idx)];
    if (n < 0) return "hace " + std::to_string(-n) + " d";
    if (n == 0) return "HOY" + hora;
    if (n == 1) return "MANANA" + hora;
    if (n <= 7) return "en " + std::to_string(n) + " dias · " + dia + hora;
    return dia + hora;
}

std::string MailTriageEngine::FormatShortDate(const std::optional<LocalDateTime>& d) {
    if (!d.has_value()) return {};
    const int m_idx = std::clamp(d->month - 1, 0, 11);
    return std::to_string(d->day) + " " + kMesesCorto[static_cast<size_t>(m_idx)];
}

} // namespace academic::tec_mail_triage
