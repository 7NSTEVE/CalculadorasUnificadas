#include "academic/horarios_tec/parser.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace academic::horarios_tec {

namespace {

std::string StripUtf8Bom(const std::string& text) {
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        return text.substr(3);
    }
    return text;
}

std::string TrimAscii(const std::string& input) {
    std::size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }
    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return input.substr(start, end - start);
}

// Normalize UTF-8 Spanish accented vowels (á/Á, é/É, í/Í, ó/Ó, ú/Ú) to uppercase ASCII for day matching.
std::string NormalizeDayToken(const std::string& input) {
    const std::string trimmed = TrimAscii(input);
    std::string out;
    out.reserve(trimmed.size());
    for (std::size_t i = 0; i < trimmed.size(); ++i) {
        const auto b0 = static_cast<unsigned char>(trimmed[i]);
        if (b0 == 0xC3 && i + 1 < trimmed.size()) {
            const auto b1 = static_cast<unsigned char>(trimmed[i + 1]);
            // á=A1, Á=81, é=A9, É=89, í=AD, Í=8D, ó=B3, Ó=93, ú=BA, Ú=9A
            if (b1 == 0xA1 || b1 == 0x81) { out.push_back('A'); ++i; continue; }
            if (b1 == 0xA9 || b1 == 0x89) { out.push_back('E'); ++i; continue; }
            if (b1 == 0xAD || b1 == 0x8D) { out.push_back('I'); ++i; continue; }
            if (b1 == 0xB3 || b1 == 0x93) { out.push_back('O'); ++i; continue; }
            if (b1 == 0xBA || b1 == 0x9A) { out.push_back('U'); ++i; continue; }
        }
        out.push_back(static_cast<char>(std::toupper(b0)));
    }
    return out;
}

const nlohmann::json* FindFirstKey(const nlohmann::json& obj, std::initializer_list<const char*> keys) {
    if (!obj.is_object()) {
        return nullptr;
    }
    for (const char* key : keys) {
        auto it = obj.find(key);
        if (it != obj.end() && !it->is_null()) {
            return &(*it);
        }
    }
    return nullptr;
}

bool TryReadStringOrIntAsString(const nlohmann::json& val, std::string& out) {
    if (val.is_string()) {
        out = val.get<std::string>();
        return true;
    }
    if (val.is_number_unsigned()) {
        out = std::to_string(val.get<std::uint64_t>());
        return true;
    }
    if (val.is_number_integer()) {
        out = std::to_string(val.get<long long>());
        return true;
    }
    return false;
}

bool TryReadInt(const nlohmann::json& val, int& out) {
    if (val.is_number_unsigned()) {
        const std::uint64_t u = val.get<std::uint64_t>();
        if (u > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        out = static_cast<int>(u);
        return true;
    }
    if (val.is_number_integer()) {
        const long long v = val.get<long long>();
        if (v < static_cast<long long>(std::numeric_limits<int>::min()) ||
            v > static_cast<long long>(std::numeric_limits<int>::max())) {
            return false;
        }
        out = static_cast<int>(v);
        return true;
    }
    if (val.is_string()) {
        const std::string s = TrimAscii(val.get<std::string>());
        if (s.empty()) {
            return false;
        }
        errno = 0;
        char* endPtr = nullptr;
        const long long v = std::strtoll(s.c_str(), &endPtr, 10);
        if (errno == ERANGE || endPtr == s.c_str() || endPtr != s.c_str() + s.size()) {
            return false;
        }
        if (v < static_cast<long long>(std::numeric_limits<int>::min()) ||
            v > static_cast<long long>(std::numeric_limits<int>::max())) {
            return false;
        }
        out = static_cast<int>(v);
        return true;
    }
    return false;
}

bool TryParseHexColor(const std::string& hex, std::uint32_t& outRgba) {
    const std::string trimmed = TrimAscii(hex);
    if (trimmed.empty() || trimmed[0] != '#') {
        return false;
    }
    const std::string digits = trimmed.substr(1);
    if (digits.size() != 6 && digits.size() != 8) {
        return false;
    }
    for (char ch : digits) {
        if (std::isxdigit(static_cast<unsigned char>(ch)) == 0) {
            return false;
        }
    }
    const unsigned long parsed = std::strtoul(digits.c_str(), nullptr, 16);
    if (digits.size() == 6) {
        outRgba = (static_cast<std::uint32_t>(parsed) << 8U) | 0xFFU;
    } else {
        outRgba = static_cast<std::uint32_t>(parsed);
    }
    return true;
}

std::uint32_t WinConsoleColorToRgba(int colorIndex) {
    switch (colorIndex) {
        case 10: return 0x10B981FFU; // Emerald
        case 11: return 0x0EA5E9FFU; // Sky
        case 12: return 0xF43F5EFFU; // Rose
        case 14: return 0xF59E0BFFU; // Amber
        default: return 0x2563EBFFU;
    }
}

bool ParseDayValue(const nlohmann::json& val, DayOfWeek& outDay, std::string& outError) {
    if (val.is_number_unsigned()) {
        const std::uint64_t d = val.get<std::uint64_t>();
        if (d <= 6) {
            outDay = static_cast<DayOfWeek>(static_cast<std::uint8_t>(d));
            return true;
        }
        outError = "Día entero fuera de rango [0..6]: " + std::to_string(d);
        return false;
    }
    if (val.is_number_integer()) {
        const long long d = val.get<long long>();
        if (d >= 0 && d <= 6) {
            outDay = static_cast<DayOfWeek>(static_cast<std::uint8_t>(d));
            return true;
        }
        outError = "Día entero fuera de rango [0..6]: " + std::to_string(d);
        return false;
    }
    if (val.is_string()) {
        const std::string token = val.get<std::string>();
        if (TryParseDay(token, outDay)) {
            return true;
        }
        outError = "Token de día inválido: '" + token + "'";
        return false;
    }
    outError = "El campo 'day' debe ser entero (0..6) o cadena (L/K/M/J/V/S/D).";
    return false;
}

bool ParseTimeBounds(
    const nlohmann::json& obj,
    int& outStartMinute,
    int& outEndMinute,
    std::string& outError) {
    // 1) Explicit integer minutes
    if (const auto* sm = FindFirstKey(obj, {"startMinute", "start_minute"})) {
        const auto* em = FindFirstKey(obj, {"endMinute", "end_minute"});
        if (!em) {
            outError = "Falta 'endMinute' correspondiente a 'startMinute'.";
            return false;
        }
        if (!TryReadInt(*sm, outStartMinute) || !TryReadInt(*em, outEndMinute)) {
            outError = "'startMinute'/'endMinute' deben ser enteros.";
            return false;
        }
        return true;
    }

    // 2) "HH:MM" string fields
    if (const auto* sStr = FindFirstKey(obj, {"start", "startTime", "horaInicio", "inicio"})) {
        const auto* eStr = FindFirstKey(obj, {"end", "endTime", "horaFin", "fin"});
        if (!eStr) {
            outError = "Falta hora de fin ('end'/'horaFin') correspondiente a 'start'.";
            return false;
        }
        if (!sStr->is_string() || !eStr->is_string()) {
            outError = "Los campos 'start'/'end' deben ser cadenas 'HH:MM'.";
            return false;
        }
        const std::string sVal = sStr->get<std::string>();
        const std::string eVal = eStr->get<std::string>();
        if (!TryParseTimeHHMM(sVal, outStartMinute)) {
            outError = "Hora de inicio inválida ('HH:MM'): '" + sVal + "'";
            return false;
        }
        if (!TryParseTimeHHMM(eVal, outEndMinute)) {
            outError = "Hora de fin inválida ('HH:MM'): '" + eVal + "'";
            return false;
        }
        return true;
    }

    // 3) HorariosTEC {startH, startM, endH, endM}
    const auto* sh = FindFirstKey(obj, {"startH", "horaInicioH"});
    const auto* eh = FindFirstKey(obj, {"endH", "horaFinH"});
    if (sh && eh) {
        int startH = 0, startM = 0, endH = 0, endM = 0;
        if (!TryReadInt(*sh, startH) || !TryReadInt(*eh, endH)) {
            outError = "'startH'/'endH' deben ser enteros.";
            return false;
        }
        if (const auto* sm = FindFirstKey(obj, {"startM", "horaInicioM"})) {
            if (!TryReadInt(*sm, startM)) {
                outError = "'startM' debe ser entero.";
                return false;
            }
        }
        if (const auto* em = FindFirstKey(obj, {"endM", "horaFinM"})) {
            if (!TryReadInt(*em, endM)) {
                outError = "'endM' debe ser entero.";
                return false;
            }
        }
        if (startH < 0 || startH > 23 || startM < 0 || startM > 59) {
            outError = "Hora de inicio fuera de rango en 'startH'/'startM'.";
            return false;
        }
        if (endH < 0 || endH > 24 || endM < 0 || endM > 59 || (endH == 24 && endM > 0)) {
            outError = "Hora de fin fuera de rango en 'endH'/'endM'.";
            return false;
        }
        outStartMinute = startH * 60 + startM;
        outEndMinute = endH * 60 + endM;
        return true;
    }

    outError = "Sesión sin horario válido (se esperaba 'startH'/'endH', 'start'/'end', o 'startMinute'/'endMinute').";
    return false;
}

bool ParseSessionObject(
    const nlohmann::json& sObj,
    const std::string& contextLabel,
    TimeSlot& outSlot,
    std::vector<std::string>& warnings,
    std::string& outError) {
    if (!sObj.is_object()) {
        outError = contextLabel + ": cada sesión debe ser un objeto JSON.";
        return false;
    }
    const auto* dayVal = FindFirstKey(sObj, {"day", "dia", "dayOfWeek", "diaSemana"});
    if (!dayVal) {
        outError = contextLabel + ": sesión sin campo 'day'/'dia'.";
        return false;
    }
    std::string dayErr;
    if (!ParseDayValue(*dayVal, outSlot.day, dayErr)) {
        outError = contextLabel + ": " + dayErr;
        return false;
    }

    std::string timeErr;
    if (!ParseTimeBounds(sObj, outSlot.startMinute, outSlot.endMinute, timeErr)) {
        outError = contextLabel + ": " + timeErr;
        return false;
    }

    if (!outSlot.IsValid()) {
        warnings.push_back(contextLabel + ": intervalo de tiempo inválido (" +
                           FormatTimeHHMM(outSlot.startMinute) + " - " +
                           FormatTimeHHMM(outSlot.endMinute) + ").");
    }

    if (const auto* r = FindFirstKey(sObj, {"room", "aula", "classroom"})) {
        TryReadStringOrIntAsString(*r, outSlot.roomUtf8);
    }
    if (const auto* b = FindFirstKey(sObj, {"building", "edificio"})) {
        TryReadStringOrIntAsString(*b, outSlot.buildingUtf8);
    }
    if (const auto* c = FindFirstKey(sObj, {"campus", "sede"})) {
        TryReadStringOrIntAsString(*c, outSlot.campusUtf8);
    }
    return true;
}

void ParseCourseCodeList(const nlohmann::json* arrVal, std::vector<std::string>& outList) {
    if (!arrVal || !arrVal->is_array()) {
        return;
    }
    for (const auto& item : *arrVal) {
        if (item.is_string()) {
            const std::string code = TrimAscii(item.get<std::string>());
            if (!code.empty()) {
                outList.push_back(code);
            }
        } else if (item.is_object()) {
            if (const auto* idVal = FindFirstKey(item, {"id", "id_course", "code"})) {
                std::string code;
                if (TryReadStringOrIntAsString(*idVal, code)) {
                    code = TrimAscii(code);
                    if (!code.empty()) {
                        outList.push_back(code);
                    }
                }
            }
        }
    }
}

bool ParseCourseObject(
    const nlohmann::json& cObj,
    std::size_t courseIdx,
    Course& outCourse,
    std::vector<std::string>& warnings,
    std::string& outError) {
    if (!cObj.is_object()) {
        outError = "El elemento de curso #" + std::to_string(courseIdx) + " debe ser un objeto JSON.";
        return false;
    }

    if (const auto* codeVal = FindFirstKey(cObj, {"code", "id_course", "codigo", "courseCode"})) {
        if (!TryReadStringOrIntAsString(*codeVal, outCourse.codeUtf8)) {
            outError = "Curso #" + std::to_string(courseIdx) + ": el código de curso debe ser cadena o entero.";
            return false;
        }
        outCourse.codeUtf8 = TrimAscii(outCourse.codeUtf8);
    }
    if (outCourse.codeUtf8.empty()) {
        outError = "Curso #" + std::to_string(courseIdx) + ": falta campo 'code' / 'id_course'.";
        return false;
    }

    if (const auto* nameVal = FindFirstKey(cObj, {"name", "nombre", "title", "courseName"})) {
        if (!nameVal->is_string()) {
            outError = "Curso " + outCourse.codeUtf8 + ": el campo 'name'/'title' debe ser una cadena.";
            return false;
        }
        outCourse.nameUtf8 = nameVal->get<std::string>();
    } else {
        outCourse.nameUtf8 = outCourse.codeUtf8;
    }

    if (const auto* colorVal = FindFirstKey(cObj, {"color", "colorCss"})) {
        if (colorVal->is_string()) {
            outCourse.colorCssUtf8 = colorVal->get<std::string>();
            std::uint32_t rgba = 0;
            if (TryParseHexColor(outCourse.colorCssUtf8, rgba)) {
                outCourse.colorRgba = rgba;
            }
        } else if (colorVal->is_number_integer()) {
            int colorIdx = 0;
            if (TryReadInt(*colorVal, colorIdx)) {
                outCourse.colorRgba = WinConsoleColorToRgba(colorIdx);
            }
        }
    }
    if (const auto* rgbaVal = FindFirstKey(cObj, {"colorRgba"})) {
        if (rgbaVal->is_number_unsigned()) {
            const std::uint64_t v = rgbaVal->get<std::uint64_t>();
            if (v <= static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                outCourse.colorRgba = static_cast<std::uint32_t>(v);
            }
        } else if (rgbaVal->is_number_integer()) {
            const long long v = rgbaVal->get<long long>();
            if (v >= 0 && v <= static_cast<long long>(std::numeric_limits<std::uint32_t>::max())) {
                outCourse.colorRgba = static_cast<std::uint32_t>(v);
            }
        }
    }

    if (const auto* credVal = FindFirstKey(cObj, {"credits", "creditos"})) {
        if (!TryReadInt(*credVal, outCourse.credits)) {
            outError = "Curso " + outCourse.codeUtf8 + ": 'credits' inválido.";
            return false;
        }
    }
    if (const auto* hrsVal = FindFirstKey(cObj, {"hours", "horas"})) {
        if (!TryReadInt(*hrsVal, outCourse.hours)) {
            outError = "Curso " + outCourse.codeUtf8 + ": 'hours' inválido.";
            return false;
        }
    }
    if (const auto* statusVal = FindFirstKey(cObj, {"best_status", "curriculumStatus", "estado"})) {
        TryReadStringOrIntAsString(*statusVal, outCourse.curriculumStatusUtf8);
    }

    ParseCourseCodeList(FindFirstKey(cObj, {"prerequisites", "requirements", "requisitos"}), outCourse.prerequisitesUtf8);
    ParseCourseCodeList(FindFirstKey(cObj, {"corequisites", "co_requirements", "correquisitos"}), outCourse.corequisitesUtf8);

    const auto* groupsVal = FindFirstKey(cObj, {"groups", "grupos"});
    if (groupsVal) {
        if (!groupsVal->is_array()) {
            outError = "Curso " + outCourse.codeUtf8 + ": 'groups' debe ser un arreglo JSON.";
            return false;
        }
        for (std::size_t gIdx = 0; gIdx < groupsVal->size(); ++gIdx) {
            const auto& gObj = (*groupsVal)[gIdx];
            if (!gObj.is_object()) {
                outError = "Curso " + outCourse.codeUtf8 + ": el grupo #" + std::to_string(gIdx) + " debe ser un objeto JSON.";
                return false;
            }
            Group group;
            if (const auto* idVal = FindFirstKey(gObj, {"id", "group", "grupo", "number"})) {
                if (!TryReadStringOrIntAsString(*idVal, group.idUtf8)) {
                    outError = "Curso " + outCourse.codeUtf8 + ": 'id'/'group' inválido en grupo #" + std::to_string(gIdx);
                    return false;
                }
                group.idUtf8 = TrimAscii(group.idUtf8);
            }
            if (group.idUtf8.empty()) {
                outError = "Curso " + outCourse.codeUtf8 + ": grupo #" + std::to_string(gIdx) + " sin 'id'.";
                return false;
            }

            if (const auto* crnVal = FindFirstKey(gObj, {"crn", "nrc", "codigoMatricula"})) {
                TryReadStringOrIntAsString(*crnVal, group.crnUtf8);
            }
            if (const auto* modVal = FindFirstKey(gObj, {"modality", "modalidad"})) {
                TryReadStringOrIntAsString(*modVal, group.modalityUtf8);
            }
            if (const auto* profVal = FindFirstKey(gObj, {"professor", "profesor", "docente", "instructor"})) {
                TryReadStringOrIntAsString(*profVal, group.professorUtf8);
            }
            if (const auto* campVal = FindFirstKey(gObj, {"campus", "sede"})) {
                TryReadStringOrIntAsString(*campVal, group.campusUtf8);
            }
            if (const auto* seatsVal = FindFirstKey(gObj, {"seatsAvailable", "cuposDisponibles", "cupos", "availableSeats", "seats"})) {
                if (seatsVal->is_object()) {
                    if (const auto* avail = FindFirstKey(*seatsVal, {"available", "disponibles"})) {
                        if (!TryReadInt(*avail, group.seatsAvailable)) {
                            outError = "Curso " + outCourse.codeUtf8 + " " + group.idUtf8 + ": 'seats.available' inválido.";
                            return false;
                        }
                    }
                    if (const auto* cap = FindFirstKey(*seatsVal, {"capacity", "capacidad", "total"})) {
                        if (!TryReadInt(*cap, group.seatsCapacity)) {
                            outError = "Curso " + outCourse.codeUtf8 + " " + group.idUtf8 + ": 'seats.capacity' inválido.";
                            return false;
                        }
                    }
                } else if (!TryReadInt(*seatsVal, group.seatsAvailable)) {
                    outError = "Curso " + outCourse.codeUtf8 + " " + group.idUtf8 + ": valor de cupos inválido.";
                    return false;
                }
            }
            if (const auto* capVal = FindFirstKey(gObj, {"seatsCapacity", "capacidad", "capacity", "totalSeats"})) {
                if (!TryReadInt(*capVal, group.seatsCapacity)) {
                    outError = "Curso " + outCourse.codeUtf8 + " " + group.idUtf8 + ": 'seatsCapacity' inválido.";
                    return false;
                }
            }

            const auto* sessVal = FindFirstKey(gObj, {"sessions", "schedule", "horario", "bloques"});
            if (sessVal) {
                if (!sessVal->is_array()) {
                    outError = "Curso " + outCourse.codeUtf8 + " " + group.idUtf8 + ": 'sessions'/'schedule' debe ser un arreglo.";
                    return false;
                }
                for (std::size_t sIdx = 0; sIdx < sessVal->size(); ++sIdx) {
                    TimeSlot slot;
                    const std::string ctx = outCourse.codeUtf8 + " " + group.idUtf8 + " sesión #" + std::to_string(sIdx);
                    if (!ParseSessionObject((*sessVal)[sIdx], ctx, slot, warnings, outError)) {
                        return false;
                    }
                    group.sessions.push_back(std::move(slot));
                }
            }

            outCourse.groups.push_back(std::move(group));
        }
    }

    return true;
}

} // namespace

bool TryParseDay(const std::string& tokenUtf8, DayOfWeek& outDay) noexcept {
    const std::string norm = NormalizeDayToken(tokenUtf8);
    if (norm.empty()) {
        return false;
    }
    if (norm == "L" || norm == "LUN" || norm == "LUNES" || norm == "MON" || norm == "MONDAY" || norm == "0") {
        outDay = DayOfWeek::Monday;
        return true;
    }
    if (norm == "K" || norm == "MA" || norm == "MAR" || norm == "MARTES" || norm == "TUE" || norm == "TUESDAY" || norm == "1") {
        outDay = DayOfWeek::Tuesday;
        return true;
    }
    if (norm == "M" || norm == "MI" || norm == "MIE" || norm == "MIERCOLES" || norm == "WED" || norm == "WEDNESDAY" || norm == "2") {
        outDay = DayOfWeek::Wednesday;
        return true;
    }
    if (norm == "J" || norm == "JUE" || norm == "JUEVES" || norm == "THU" || norm == "THURSDAY" || norm == "3") {
        outDay = DayOfWeek::Thursday;
        return true;
    }
    if (norm == "V" || norm == "VIE" || norm == "VIERNES" || norm == "FRI" || norm == "FRIDAY" || norm == "4") {
        outDay = DayOfWeek::Friday;
        return true;
    }
    if (norm == "S" || norm == "SAB" || norm == "SABADO" || norm == "SAT" || norm == "SATURDAY" || norm == "5") {
        outDay = DayOfWeek::Saturday;
        return true;
    }
    if (norm == "D" || norm == "DOM" || norm == "DOMINGO" || norm == "SUN" || norm == "SUNDAY" || norm == "6") {
        outDay = DayOfWeek::Sunday;
        return true;
    }
    return false;
}

const char* DayToShortEs(DayOfWeek day) noexcept {
    switch (day) {
        case DayOfWeek::Monday:    return "L";
        case DayOfWeek::Tuesday:   return "K";
        case DayOfWeek::Wednesday: return "M";
        case DayOfWeek::Thursday:  return "J";
        case DayOfWeek::Friday:    return "V";
        case DayOfWeek::Saturday:  return "S";
        case DayOfWeek::Sunday:    return "D";
    }
    return "?";
}

const char* DayToSpanishName(DayOfWeek day) noexcept {
    switch (day) {
        case DayOfWeek::Monday:    return "Lunes";
        case DayOfWeek::Tuesday:   return "Martes";
        case DayOfWeek::Wednesday: return "Miércoles";
        case DayOfWeek::Thursday:  return "Jueves";
        case DayOfWeek::Friday:    return "Viernes";
        case DayOfWeek::Saturday:  return "Sábado";
        case DayOfWeek::Sunday:    return "Domingo";
    }
    return "Desconocido";
}

bool TryParseTimeHHMM(const std::string& hhmmUtf8, int& outMinutes) noexcept {
    const std::string s = TrimAscii(hhmmUtf8);
    const std::size_t colon = s.find(':');
    if (colon == std::string::npos || colon == 0 || colon > 2) {
        return false;
    }
    const std::string hhStr = s.substr(0, colon);
    const std::string mmStr = s.substr(colon + 1);
    if (mmStr.size() != 2) {
        return false;
    }
    for (char ch : hhStr) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            return false;
        }
    }
    for (char ch : mmStr) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            return false;
        }
    }
    const int hh = std::stoi(hhStr);
    const int mm = std::stoi(mmStr);
    if (hh < 0 || hh > 24 || mm < 0 || mm > 59 || (hh == 24 && mm > 0)) {
        return false;
    }
    outMinutes = hh * 60 + mm;
    return true;
}

std::string FormatTimeHHMM(int minutes) {
    const bool negative = (minutes < 0);
    const long long mag = negative
        ? -static_cast<long long>(minutes)
        : static_cast<long long>(minutes);
    const long long hh = mag / 60;
    const int mm = static_cast<int>(mag % 60);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s%02lld:%02d", negative ? "-" : "", hh, mm);
    return std::string(buf);
}

CatalogParseResult ParseCatalogJson(const std::string& jsonUtf8) {
    CatalogParseResult res;
    const std::string cleaned = TrimAscii(StripUtf8Bom(jsonUtf8));
    if (cleaned.empty()) {
        res.errorUtf8 = "El JSON del catálogo está vacío.";
        return res;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(cleaned);
    } catch (const nlohmann::json::exception& ex) {
        res.errorUtf8 = std::string("JSON del catálogo malformado: ") + ex.what();
        return res;
    }

    std::vector<const nlohmann::json*> courseArrays;
    if (root.is_array()) {
        courseArrays.push_back(&root);
    } else if (root.is_object()) {
        if (const auto* t = FindFirstKey(root, {"title", "titulo", "dsc_curriculum"})) {
            TryReadStringOrIntAsString(*t, res.catalog.titleUtf8);
        }
        if (const auto* term = FindFirstKey(root, {"term", "periodo", "semestre"})) {
            TryReadStringOrIntAsString(*term, res.catalog.termUtf8);
        }
        if (const auto* camp = FindFirstKey(root, {"campus", "sede"})) {
            TryReadStringOrIntAsString(*camp, res.catalog.campusUtf8);
        }

        if (const auto* coursesArr = FindFirstKey(root, {"courses", "cursos", "materias"})) {
            if (!coursesArr->is_array()) {
                res.errorUtf8 = "El campo 'courses' debe ser un arreglo JSON.";
                return res;
            }
            courseArrays.push_back(coursesArr);
        } else if (const auto* levelsArr = FindFirstKey(root, {"levels", "niveles"})) {
            if (!levelsArr->is_array()) {
                res.errorUtf8 = "El campo 'levels' debe ser un arreglo JSON.";
                return res;
            }
            for (const auto& lvl : *levelsArr) {
                if (!lvl.is_object()) {
                    res.errorUtf8 = "Cada elemento en 'levels' debe ser un objeto JSON.";
                    return res;
                }
                if (const auto* lvlCourses = FindFirstKey(lvl, {"courses", "cursos"})) {
                    if (!lvlCourses->is_array()) {
                        res.errorUtf8 = "El campo 'levels[].courses' debe ser un arreglo JSON.";
                        return res;
                    }
                    courseArrays.push_back(lvlCourses);
                }
            }
        } else {
            res.errorUtf8 = "El objeto JSON del catálogo debe contener 'courses' o 'levels'.";
            return res;
        }
    } else {
        res.errorUtf8 = "La raíz del JSON del catálogo debe ser un arreglo u objeto.";
        return res;
    }

    std::size_t globalIdx = 0;
    for (const nlohmann::json* arr : courseArrays) {
        for (const auto& cObj : *arr) {
            Course course;
            if (!ParseCourseObject(cObj, globalIdx, course, res.warningsUtf8, res.errorUtf8)) {
                return res;
            }
            res.catalog.courses.push_back(std::move(course));
            ++globalIdx;
        }
    }

    if (res.catalog.courses.empty()) {
        res.warningsUtf8.push_back("El catálogo no contiene cursos.");
    }

    res.ok = true;
    return res;
}

BusySlotsParseResult ParseBusySlotsJson(const std::string& jsonUtf8) {
    BusySlotsParseResult res;
    const std::string cleaned = TrimAscii(StripUtf8Bom(jsonUtf8));
    if (cleaned.empty()) {
        res.errorUtf8 = "El JSON de bloques ocupados está vacío.";
        return res;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(cleaned);
    } catch (const nlohmann::json::exception& ex) {
        res.errorUtf8 = std::string("JSON de bloques ocupados malformado: ") + ex.what();
        return res;
    }

    const nlohmann::json* slotsArr = nullptr;
    if (root.is_array()) {
        slotsArr = &root;
    } else if (root.is_object()) {
        slotsArr = FindFirstKey(root, {"busySlots", "busy_slots", "slots", "bloques"});
        if (!slotsArr || !slotsArr->is_array()) {
            res.errorUtf8 = "El objeto JSON de bloques ocupados debe contener el arreglo 'busySlots' o 'busy_slots'.";
            return res;
        }
    } else {
        res.errorUtf8 = "La raíz del JSON de bloques ocupados debe ser un arreglo u objeto.";
        return res;
    }

    for (std::size_t i = 0; i < slotsArr->size(); ++i) {
        const auto& item = (*slotsArr)[i];
        if (!item.is_object()) {
            res.errorUtf8 = "Bloque ocupado #" + std::to_string(i) + " debe ser un objeto JSON.";
            return res;
        }
        const auto* dayVal = FindFirstKey(item, {"day", "dia", "dayOfWeek", "diaSemana"});
        if (!dayVal) {
            res.errorUtf8 = "Bloque ocupado #" + std::to_string(i) + " sin campo 'day'/'dia'.";
            return res;
        }
        BusySlot busy;
        std::string dayErr;
        if (!ParseDayValue(*dayVal, busy.day, dayErr)) {
            res.errorUtf8 = "Bloque ocupado #" + std::to_string(i) + ": " + dayErr;
            return res;
        }
        std::string timeErr;
        if (!ParseTimeBounds(item, busy.startMinute, busy.endMinute, timeErr)) {
            res.errorUtf8 = "Bloque ocupado #" + std::to_string(i) + ": " + timeErr;
            return res;
        }
        if (const auto* lbl = FindFirstKey(item, {"label", "etiqueta", "title", "name", "nombre"})) {
            TryReadStringOrIntAsString(*lbl, busy.labelUtf8);
        }
        if (const auto* src = FindFirstKey(item, {"source", "fuente"})) {
            TryReadStringOrIntAsString(*src, busy.sourceUtf8);
        }
        if (!busy.IsValid()) {
            res.warningsUtf8.push_back("Bloque ocupado #" + std::to_string(i) + " tiene intervalo inválido (" +
                                       FormatTimeHHMM(busy.startMinute) + " - " +
                                       FormatTimeHHMM(busy.endMinute) + ").");
        }
        res.busySlots.push_back(std::move(busy));
    }

    res.ok = true;
    return res;
}

} // namespace academic::horarios_tec
