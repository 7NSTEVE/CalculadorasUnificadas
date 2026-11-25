#pragma once

#include "types.hpp"
#include <string>
#include <vector>

namespace academic::horarios_tec {

// Exact interval overlap test (touching boundaries return false when minBufferMinutes == 0;
// minBufferMinutes > 0 requires at least that many minutes between sessions).
bool TimeSlotsConflict(
    DayOfWeek dayA, int startA, int endA,
    DayOfWeek dayB, int startB, int endB,
    int minBufferMinutes = 0) noexcept;

class HorariosSolver {
public:
    static ScheduleResult Solve(
        const CourseCatalog& catalog,
        const std::vector<BusySlot>& busySlots = {},
        const SolverPreferences& prefs = {});

    static ScheduleResult SolveFromJson(
        const std::string& catalogJsonUtf8,
        const std::string& busySlotsJsonUtf8 = {},
        const SolverPreferences& prefs = {});
};

// Canonical free-function entry point per PROJECT.md & SCOPE.md interface contract.
ScheduleResult SolveSchedules(
    const CourseCatalog& catalog,
    const std::vector<BusySlot>& busySlots = {},
    const SolverPreferences& prefs = {});

} // namespace academic::horarios_tec
