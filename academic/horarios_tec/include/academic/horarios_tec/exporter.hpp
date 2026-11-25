#pragma once

#include "types.hpp"
#include <string>

namespace academic::horarios_tec {

struct JsonExportOptions {
    int indentSpaces        = 2;    // -1 for compact single-line JSON
    bool includeMetrics     = true;
    bool includeDiagnostics = true;
};

struct CsvExportOptions {
    bool includeUtf8Bom    = true; // UTF-8 BOM for Excel compatibility with Spanish accents
    char delimiter         = ',';
    bool includeRankColumn = true;
};

// Exports full ScheduleResult (all ranked schedules + metrics + diagnostics) to UTF-8 JSON.
std::string ExportJson(const ScheduleResult& result, const JsonExportOptions& opts = {});

// Exports a single RankedSchedule to UTF-8 JSON.
std::string ExportJson(const RankedSchedule& schedule, const JsonExportOptions& opts = {});

// Exports all ranked schedules in ScheduleResult to RFC-4180 UTF-8 CSV.
std::string ExportCsv(const ScheduleResult& result, const CsvExportOptions& opts = {});

// Exports a single RankedSchedule to RFC-4180 UTF-8 CSV.
std::string ExportCsv(const RankedSchedule& schedule, const CsvExportOptions& opts = {});

} // namespace academic::horarios_tec
