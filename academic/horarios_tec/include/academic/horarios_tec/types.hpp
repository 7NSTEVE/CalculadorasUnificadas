#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace academic::horarios_tec {

enum class DayOfWeek : std::uint8_t {
    Monday    = 0, // L (Lunes)
    Tuesday   = 1, // K (Martes)
    Wednesday = 2, // M (Miércoles)
    Thursday  = 3, // J (Jueves)
    Friday    = 4, // V (Viernes)
    Saturday  = 5, // S (Sábado)
    Sunday    = 6  // D (Domingo)
};

struct TimeSlot {
    DayOfWeek day = DayOfWeek::Monday;
    int startMinute = 0; // Minutes since 00:00 [0..1439]
    int endMinute   = 0; // Minutes since 00:00 [1..1440], endMinute > startMinute
    std::string roomUtf8;
    std::string buildingUtf8;
    std::string campusUtf8;

    bool IsValid() const noexcept {
        return static_cast<std::uint8_t>(day) <= 6 &&
               startMinute >= 0 && endMinute <= 1440 && startMinute < endMinute;
    }
};

struct BusySlot {
    DayOfWeek day = DayOfWeek::Monday;
    int startMinute = 0;
    int endMinute   = 0;
    std::string labelUtf8;
    std::string sourceUtf8; // e.g., "calendar", "work", "enrolled"

    bool IsValid() const noexcept {
        return static_cast<std::uint8_t>(day) <= 6 &&
               startMinute >= 0 && endMinute <= 1440 && startMinute < endMinute;
    }
};

struct Group {
    std::string idUtf8;        // e.g. "Gr 50" or "50"
    std::string crnUtf8;       // Optional registration code
    std::string modalityUtf8;  // "Regular", "Semipresencial", "Virtual", etc.
    std::string professorUtf8;
    std::string campusUtf8;
    int seatsAvailable = -1;   // -1 = unknown/unconstrained, >= 0 = known seat count
    int seatsCapacity  = -1;   // -1 = unknown, >= 0 = total capacity
    std::vector<TimeSlot> sessions;
};

struct Course {
    std::string codeUtf8;      // e.g. "EL2113" (alias: "id_course")
    std::string nameUtf8;      // UTF-8 Spanish course title
    std::string colorCssUtf8;  // Optional HorariosTEC gradient/hex string
    std::uint32_t colorRgba = 0x2563EBFFu;
    int credits = 0;
    int hours   = 0;
    std::string curriculumStatusUtf8; // Optional TEC Digital "best_status" ("materiaPendiente", etc.)
    std::vector<std::string> prerequisitesUtf8;
    std::vector<std::string> corequisitesUtf8;
    std::vector<Group> groups;
};

struct CourseCatalog {
    std::string titleUtf8;
    std::string termUtf8;
    std::string campusUtf8;
    std::vector<Course> courses;
};

struct FilterOptions {
    std::vector<std::string> requiredCourseCodes; // Empty => include all courses in catalog
    std::unordered_map<std::string, std::vector<std::string>> allowedGroupIdsByCourse;
    std::unordered_map<std::string, std::vector<std::string>> excludedGroupIdsByCourse;
    std::vector<std::string> allowedModalities;   // Empty => any modality
    std::vector<std::string> allowedCampuses;     // Empty => any campus
    std::vector<std::string> excludedProfessors;  // Case-insensitive match
    bool requireAvailableSeats = false;           // Reject seatsAvailable == 0
    bool allowUnknownSeats     = true;            // Keep seatsAvailable < 0 when requireAvailableSeats == true
    bool excludeNightClasses   = false;           // Reject sessions starting >= nightStartMinute
};

struct ScoringWeights {
    double morningWeight        = 30.0;  // Per session starting < afternoonStartMinute (12:00)
    double afternoonWeight      = -10.0; // Per session in [12:00, 17:00)
    double nightWeight          = -50.0; // Per session starting >= 17:00
    double compactnessWeight    = 0.25;  // Penalty per minute of daily span (lastEnd - firstStart)
    double gapPenaltyPerMinute  = 0.25;  // Penalty per idle gap minute between consecutive sessions on same day
    double activeDayPenalty     = 20.0;  // Penalty per distinct active day in week
    int afternoonStartMinute    = 12 * 60; // 720
    int nightStartMinute        = 17 * 60; // 1020
    bool legacySessionScoreOnly = false;   // When true, matches HorariosTEC/WebUI/app.js calculateScore() exactly
};

struct SolverPreferences {
    FilterOptions filters;
    ScoringWeights weights;
    std::size_t maxResults            = 50;      // 0 => return all valid within search budget
    std::size_t maxSearchStates       = 2000000; // Combinatorial safety budget
    int minBreakMinutesBetweenClasses = 0;       // 0 => touching intervals allowed; > 0 enforces break between distinct courses & busySlots (intra-group sessions only forbid strict overlap)
    bool sortDeterministically        = true;    // Tie-break by active days, gaps, span, then group IDs
};

struct ScheduleSelection {
    std::string courseCodeUtf8;
    std::string courseNameUtf8;
    std::string colorCssUtf8;
    std::uint32_t colorRgba = 0x2563EBFFu;
    int credits = 0;
    Group group;
};

struct ScheduleMetrics {
    double totalScore          = 0.0;
    double timeOfDayScore      = 0.0;
    int activeDaysCount        = 0;
    int totalGapMinutes        = 0;
    int totalSpanMinutes       = 0;
    int earliestStartMinute    = 1440;
    int latestEndMinute        = 0;
    int totalCredits           = 0;
    int totalSessionCount      = 0;
};

struct RankedSchedule {
    std::size_t rank = 0; // 1-based rank
    std::vector<ScheduleSelection> selections;
    ScheduleMetrics metrics;
};

struct ConflictDetail {
    std::string courseAUtf8;
    std::string groupAUtf8;
    std::string courseBUtf8; // Or "BUSY:<label>" when blocked by BusySlot
    std::string groupBUtf8;
    DayOfWeek day = DayOfWeek::Monday;
    int overlapStartMinute = 0; // Always <= overlapEndMinute; for buffer-only conflicts, marks the insufficient gap [minEnd, maxStart]
    int overlapEndMinute   = 0;
};

struct CourseFilterDiagnostic {
    std::string courseCodeUtf8;
    std::size_t initialGroupCount       = 0;
    std::size_t afterFilterGroupCount   = 0;
    std::size_t afterBusySlotGroupCount = 0;
    std::string reasonUtf8;
};

enum class SolveStatus : std::uint8_t {
    Ok                     = 0, // >= 1 valid schedule found
    NoConflictFreeSchedule = 1, // Valid input, 0 schedules possible
    InvalidInput           = 2, // Empty course selection, invalid TimeSlot, or bad filter course code
    SearchBudgetExceeded   = 3  // Search budget hit (partial ranked schedules may still be populated)
};

struct SolverDiagnostics {
    std::size_t candidateCoursesCount = 0;
    std::size_t combinationsExplored  = 0;
    std::size_t prunedBranchesCount   = 0;
    std::size_t validSchedulesFound   = 0;
    bool searchBudgetExhausted        = false;
    std::vector<CourseFilterDiagnostic> courseDiagnostics;
    std::vector<ConflictDetail> blockingConflicts;
    std::vector<std::string> warningsUtf8;
    std::vector<std::string> errorsUtf8;
};

struct ScheduleResult {
    bool ok = false; // True when at least one valid schedule was found
    SolveStatus status = SolveStatus::NoConflictFreeSchedule;
    std::vector<RankedSchedule> schedules;
    SolverDiagnostics diagnostics;

    bool HasSolutions() const noexcept { return !schedules.empty(); }
};

} // namespace academic::horarios_tec
