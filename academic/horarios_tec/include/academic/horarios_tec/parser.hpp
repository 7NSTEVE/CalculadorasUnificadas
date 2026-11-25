#pragma once

#include "types.hpp"
#include <string>
#include <vector>

namespace academic::horarios_tec {

struct CatalogParseResult {
    bool ok = false;
    CourseCatalog catalog;
    std::vector<std::string> warningsUtf8;
    std::string errorUtf8;
};

struct BusySlotsParseResult {
    bool ok = false;
    std::vector<BusySlot> busySlots;
    std::vector<std::string> warningsUtf8;
    std::string errorUtf8;
};

// Accepts HorariosTEC/WebUI/app.js array or object { courses: [...] }, plus TEC Digital
// aliases (id_course, group int/string, schedule, levels[].courses[], "HH:MM", "L/K/M/J/V/S/D").
CatalogParseResult ParseCatalogJson(const std::string& jsonUtf8);

// Accepts JSON array of busy slots or { busySlots: [...] } / { busy_slots: [...] }.
BusySlotsParseResult ParseBusySlotsJson(const std::string& jsonUtf8);

// Helpers
bool TryParseDay(const std::string& tokenUtf8, DayOfWeek& outDay) noexcept;
const char* DayToShortEs(DayOfWeek day) noexcept;     // "L","K","M","J","V","S","D"
const char* DayToSpanishName(DayOfWeek day) noexcept; // "Lunes", "Martes", ...
bool TryParseTimeHHMM(const std::string& hhmmUtf8, int& outMinutes) noexcept;
std::string FormatTimeHHMM(int minutes);

} // namespace academic::horarios_tec
