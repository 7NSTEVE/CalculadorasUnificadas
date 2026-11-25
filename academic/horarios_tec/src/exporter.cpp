#include "academic/horarios_tec/exporter.hpp"
#include "academic/horarios_tec/parser.hpp"

#include <nlohmann/json.hpp>

#include <locale>
#include <sstream>
#include <string>

namespace academic::horarios_tec {

namespace {

const char* SolveStatusToString(SolveStatus status) noexcept {
    switch (status) {
        case SolveStatus::Ok:                     return "Ok";
        case SolveStatus::NoConflictFreeSchedule: return "NoConflictFreeSchedule";
        case SolveStatus::InvalidInput:           return "InvalidInput";
        case SolveStatus::SearchBudgetExceeded:   return "SearchBudgetExceeded";
    }
    return "Unknown";
}

nlohmann::json ScheduleToJson(const RankedSchedule& schedule, bool includeMetrics) {
    nlohmann::json j = nlohmann::json::object();
    j["rank"] = schedule.rank;

    nlohmann::json selArr = nlohmann::json::array();
    for (const auto& sel : schedule.selections) {
        nlohmann::json sJson = nlohmann::json::object();
        sJson["courseCode"] = sel.courseCodeUtf8;
        sJson["courseName"] = sel.courseNameUtf8;
        sJson["colorCss"] = sel.colorCssUtf8;
        sJson["colorRgba"] = sel.colorRgba;
        sJson["credits"] = sel.credits;

        nlohmann::json grpJson = nlohmann::json::object();
        grpJson["id"] = sel.group.idUtf8;
        grpJson["crn"] = sel.group.crnUtf8;
        grpJson["modality"] = sel.group.modalityUtf8;
        grpJson["professor"] = sel.group.professorUtf8;
        grpJson["campus"] = sel.group.campusUtf8;
        grpJson["seatsAvailable"] = sel.group.seatsAvailable;
        grpJson["seatsCapacity"] = sel.group.seatsCapacity;

        nlohmann::json sessArr = nlohmann::json::array();
        for (const auto& sess : sel.group.sessions) {
            nlohmann::json tJson = nlohmann::json::object();
            tJson["day"] = DayToShortEs(sess.day);
            tJson["dayName"] = DayToSpanishName(sess.day);
            tJson["dayIndex"] = static_cast<int>(sess.day);
            tJson["start"] = FormatTimeHHMM(sess.startMinute);
            tJson["end"] = FormatTimeHHMM(sess.endMinute);
            tJson["startMinute"] = sess.startMinute;
            tJson["endMinute"] = sess.endMinute;
            tJson["room"] = sess.roomUtf8;
            tJson["building"] = sess.buildingUtf8;
            tJson["campus"] = sess.campusUtf8;
            sessArr.push_back(std::move(tJson));
        }
        grpJson["sessions"] = std::move(sessArr);
        sJson["group"] = std::move(grpJson);
        selArr.push_back(std::move(sJson));
    }
    j["selections"] = std::move(selArr);

    if (includeMetrics) {
        j["metrics"] = {
            {"totalScore", schedule.metrics.totalScore},
            {"timeOfDayScore", schedule.metrics.timeOfDayScore},
            {"activeDaysCount", schedule.metrics.activeDaysCount},
            {"totalGapMinutes", schedule.metrics.totalGapMinutes},
            {"totalSpanMinutes", schedule.metrics.totalSpanMinutes},
            {"earliestStartMinute", schedule.metrics.earliestStartMinute},
            {"latestEndMinute", schedule.metrics.latestEndMinute},
            {"totalCredits", schedule.metrics.totalCredits},
            {"totalSessionCount", schedule.metrics.totalSessionCount}
        };
    }
    return j;
}

std::string CsvEscape(const std::string& field, char delimiter, bool sanitizeFormula = false) {
    std::string safe = field;
    if (sanitizeFormula && !safe.empty()) {
        const char c0 = safe.front();
        if (c0 == '=' || c0 == '+' || c0 == '-' || c0 == '@' || c0 == '\t' || c0 == '\r') {
            safe.insert(safe.begin(), '\'');
        }
    }
    bool needsQuotes = false;
    for (char ch : safe) {
        if (ch == delimiter || ch == '"' || ch == '\r' || ch == '\n') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return safe;
    }
    std::string out;
    out.reserve(safe.size() + 4);
    out.push_back('"');
    for (char ch : safe) {
        if (ch == '"') {
            out.push_back('"');
        }
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

void AppendCsvHeader(std::string& out, const CsvExportOptions& opts) {
    const char d = opts.delimiter;
    auto addCol = [&](const char* name, bool first) {
        if (!first) {
            out.push_back(d);
        }
        out += CsvEscape(name, d);
    };
    bool first = true;
    if (opts.includeRankColumn) {
        addCol("Horario", first);
        first = false;
    }
    addCol("Puntaje", first);
    addCol("Días Activos", false);
    addCol("Huecos (min)", false);
    addCol("Código", false);
    addCol("Curso", false);
    addCol("Grupo", false);
    addCol("Modalidad", false);
    addCol("Profesor", false);
    addCol("Sede", false);
    addCol("Cupos", false);
    addCol("Día", false);
    addCol("Hora Inicio", false);
    addCol("Hora Fin", false);
    addCol("Aula", false);
    out += "\r\n";
}

std::string FormatScore(double score) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << score;
    return oss.str();
}

void AppendScheduleCsvRows(std::string& out, const RankedSchedule& schedule, const CsvExportOptions& opts) {
    const char d = opts.delimiter;
    const std::string rankStr = std::to_string(schedule.rank);
    const std::string scoreStr = FormatScore(schedule.metrics.totalScore);
    const std::string daysStr = std::to_string(schedule.metrics.activeDaysCount);
    const std::string gapsStr = std::to_string(schedule.metrics.totalGapMinutes);

    auto emitRow = [&](const ScheduleSelection& sel,
                       const std::string& dayStr,
                       const std::string& startStr,
                       const std::string& endStr,
                       const std::string& roomStr,
                       const std::string& campusStr) {
        const std::string seatsStr = (sel.group.seatsAvailable < 0)
            ? std::string("N/D")
            : std::to_string(sel.group.seatsAvailable);

        if (opts.includeRankColumn) {
            out += CsvEscape(rankStr, d);
            out.push_back(d);
        }
        out += CsvEscape(scoreStr, d);
        out.push_back(d);
        out += CsvEscape(daysStr, d);
        out.push_back(d);
        out += CsvEscape(gapsStr, d);
        out.push_back(d);
        out += CsvEscape(sel.courseCodeUtf8, d, true);
        out.push_back(d);
        out += CsvEscape(sel.courseNameUtf8, d, true);
        out.push_back(d);
        out += CsvEscape(sel.group.idUtf8, d, true);
        out.push_back(d);
        out += CsvEscape(sel.group.modalityUtf8, d, true);
        out.push_back(d);
        out += CsvEscape(sel.group.professorUtf8, d, true);
        out.push_back(d);
        out += CsvEscape(campusStr, d, true);
        out.push_back(d);
        out += CsvEscape(seatsStr, d, false);
        out.push_back(d);
        out += CsvEscape(dayStr, d, true);
        out.push_back(d);
        out += CsvEscape(startStr, d, false);
        out.push_back(d);
        out += CsvEscape(endStr, d, false);
        out.push_back(d);
        out += CsvEscape(roomStr, d, true);
        out += "\r\n";
    };

    for (const auto& sel : schedule.selections) {
        if (sel.group.sessions.empty()) {
            emitRow(sel, "", "", "", "", sel.group.campusUtf8);
            continue;
        }
        for (const auto& sess : sel.group.sessions) {
            const std::string campus = !sess.campusUtf8.empty() ? sess.campusUtf8 : sel.group.campusUtf8;
            emitRow(sel,
                    DayToSpanishName(sess.day),
                    FormatTimeHHMM(sess.startMinute),
                    FormatTimeHHMM(sess.endMinute),
                    sess.roomUtf8,
                    campus);
        }
    }
}

} // namespace

std::string ExportJson(const ScheduleResult& result, const JsonExportOptions& opts) {
    nlohmann::json root = nlohmann::json::object();
    root["ok"] = result.ok;
    root["status"] = SolveStatusToString(result.status);
    root["scheduleCount"] = result.schedules.size();

    nlohmann::json schedArr = nlohmann::json::array();
    for (const auto& sched : result.schedules) {
        schedArr.push_back(ScheduleToJson(sched, opts.includeMetrics));
    }
    root["schedules"] = std::move(schedArr);

    if (opts.includeDiagnostics) {
        nlohmann::json diagJson = nlohmann::json::object();
        diagJson["candidateCoursesCount"] = result.diagnostics.candidateCoursesCount;
        diagJson["combinationsExplored"] = result.diagnostics.combinationsExplored;
        diagJson["prunedBranchesCount"] = result.diagnostics.prunedBranchesCount;
        diagJson["validSchedulesFound"] = result.diagnostics.validSchedulesFound;
        diagJson["searchBudgetExhausted"] = result.diagnostics.searchBudgetExhausted;
        diagJson["warnings"] = result.diagnostics.warningsUtf8;
        diagJson["errors"] = result.diagnostics.errorsUtf8;

        nlohmann::json courseDiags = nlohmann::json::array();
        for (const auto& cd : result.diagnostics.courseDiagnostics) {
            courseDiags.push_back({
                {"courseCode", cd.courseCodeUtf8},
                {"initialGroupCount", cd.initialGroupCount},
                {"afterFilterGroupCount", cd.afterFilterGroupCount},
                {"afterBusySlotGroupCount", cd.afterBusySlotGroupCount},
                {"reason", cd.reasonUtf8}
            });
        }
        diagJson["courseDiagnostics"] = std::move(courseDiags);

        nlohmann::json conflicts = nlohmann::json::array();
        for (const auto& c : result.diagnostics.blockingConflicts) {
            conflicts.push_back({
                {"courseA", c.courseAUtf8},
                {"groupA", c.groupAUtf8},
                {"courseB", c.courseBUtf8},
                {"groupB", c.groupBUtf8},
                {"day", DayToShortEs(c.day)},
                {"dayName", DayToSpanishName(c.day)},
                {"overlapStart", FormatTimeHHMM(c.overlapStartMinute)},
                {"overlapEnd", FormatTimeHHMM(c.overlapEndMinute)}
            });
        }
        diagJson["blockingConflicts"] = std::move(conflicts);
        root["diagnostics"] = std::move(diagJson);
    }

    return root.dump(opts.indentSpaces, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string ExportJson(const RankedSchedule& schedule, const JsonExportOptions& opts) {
    return ScheduleToJson(schedule, opts.includeMetrics).dump(
        opts.indentSpaces, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string ExportCsv(const ScheduleResult& result, const CsvExportOptions& opts) {
    std::string out;
    if (opts.includeUtf8Bom) {
        out += "\xEF\xBB\xBF";
    }
    AppendCsvHeader(out, opts);
    for (const auto& sched : result.schedules) {
        AppendScheduleCsvRows(out, sched, opts);
    }
    return out;
}

std::string ExportCsv(const RankedSchedule& schedule, const CsvExportOptions& opts) {
    std::string out;
    if (opts.includeUtf8Bom) {
        out += "\xEF\xBB\xBF";
    }
    AppendCsvHeader(out, opts);
    AppendScheduleCsvRows(out, schedule, opts);
    return out;
}

} // namespace academic::horarios_tec
