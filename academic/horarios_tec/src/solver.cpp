#include "academic/horarios_tec/solver.hpp"
#include "academic/horarios_tec/parser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace academic::horarios_tec {

namespace {

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

std::string ToLowerUtf8Spanish(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto b0 = static_cast<unsigned char>(input[i]);
        if (b0 >= static_cast<unsigned char>('A') && b0 <= static_cast<unsigned char>('Z')) {
            out.push_back(static_cast<char>(b0 + ('a' - 'A')));
            continue;
        }
        if (b0 == 0xC3u && i + 1 < input.size()) {
            const auto b1 = static_cast<unsigned char>(input[i + 1]);
            switch (b1) {
                case 0x81u: // Á -> á
                case 0x89u: // É -> é
                case 0x8Du: // Í -> í
                case 0x91u: // Ñ -> ñ
                case 0x93u: // Ó -> ó
                case 0x9Au: // Ú -> ú
                case 0x9Cu: // Ü -> ü
                    out.push_back(static_cast<char>(0xC3u));
                    out.push_back(static_cast<char>(b1 + 0x20u));
                    ++i;
                    continue;
                default:
                    break;
            }
        }
        out.push_back(static_cast<char>(b0));
    }
    return out;
}

bool ContainsCaseInsensitive(const std::vector<std::string>& list, const std::string& value) {
    const std::string needle = ToLowerUtf8Spanish(TrimAscii(value));
    for (const auto& item : list) {
        if (ToLowerUtf8Spanish(TrimAscii(item)) == needle) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> CollectGroupFilterList(
    const std::unordered_map<std::string, std::vector<std::string>>& map,
    const std::string& courseCode) {
    const std::string lowerCode = ToLowerUtf8Spanish(TrimAscii(courseCode));
    std::vector<std::pair<std::string, const std::vector<std::string>*>> matches;
    for (const auto& kv : map) {
        if (ToLowerUtf8Spanish(TrimAscii(kv.first)) == lowerCode) {
            matches.emplace_back(kv.first, &kv.second);
        }
    }
    if (matches.empty()) {
        return {};
    }
    std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    std::vector<std::string> merged;
    for (const auto& match : matches) {
        merged.insert(merged.end(), match.second->begin(), match.second->end());
    }
    return merged;
}

bool GroupMatchesCampusFilter(
    const Group& group,
    const std::string& catalogCampus,
    const std::vector<std::string>& allowedCampuses) {
    if (allowedCampuses.empty()) {
        return true;
    }
    const std::string& groupFallback = !TrimAscii(group.campusUtf8).empty()
        ? group.campusUtf8
        : catalogCampus;
    if (group.sessions.empty()) {
        return !TrimAscii(groupFallback).empty() &&
               ContainsCaseInsensitive(allowedCampuses, groupFallback);
    }
    for (const auto& s : group.sessions) {
        const std::string& effectiveCampus = !TrimAscii(s.campusUtf8).empty()
            ? s.campusUtf8
            : groupFallback;
        if (TrimAscii(effectiveCampus).empty() ||
            !ContainsCaseInsensitive(allowedCampuses, effectiveCampus)) {
            return false;
        }
    }
    return true;
}

void ComputeConflictSpan(
    int startA,
    int endA,
    int startB,
    int endB,
    int& outStartMinute,
    int& outEndMinute) noexcept {
    const int rawOverlapStart = std::max(startA, startB);
    const int rawOverlapEnd = std::min(endA, endB);
    if (rawOverlapStart < rawOverlapEnd) {
        outStartMinute = rawOverlapStart;
        outEndMinute = rawOverlapEnd;
    } else {
        // Triggered by minBufferMinutes > 0 without direct overlap:
        // record the insufficient gap interval [minEnd, maxStart] so start <= end always holds.
        outStartMinute = rawOverlapEnd;
        outEndMinute = rawOverlapStart;
    }
}

bool GroupsConflict(
    const Group& a,
    const Group& b,
    int minBufferMinutes,
    const std::string& courseA,
    const std::string& courseB,
    ConflictDetail* outConflict) {
    for (const auto& sa : a.sessions) {
        for (const auto& sb : b.sessions) {
            if (TimeSlotsConflict(sa.day, sa.startMinute, sa.endMinute,
                                  sb.day, sb.startMinute, sb.endMinute,
                                  minBufferMinutes)) {
                if (outConflict) {
                    outConflict->courseAUtf8 = courseA;
                    outConflict->groupAUtf8 = a.idUtf8;
                    outConflict->courseBUtf8 = courseB;
                    outConflict->groupBUtf8 = b.idUtf8;
                    outConflict->day = sa.day;
                    ComputeConflictSpan(
                        sa.startMinute, sa.endMinute,
                        sb.startMinute, sb.endMinute,
                        outConflict->overlapStartMinute,
                        outConflict->overlapEndMinute);
                }
                return true;
            }
        }
    }
    return false;
}

ScheduleMetrics ComputeScheduleMetrics(
    const std::vector<ScheduleSelection>& selections,
    const ScoringWeights& weights) {
    ScheduleMetrics metrics;
    std::vector<std::pair<int, int>> byDay[7];

    for (const auto& sel : selections) {
        metrics.totalCredits += sel.credits;
        for (const auto& s : sel.group.sessions) {
            ++metrics.totalSessionCount;
            if (s.startMinute < weights.afternoonStartMinute) {
                metrics.timeOfDayScore += weights.morningWeight;
            } else if (s.startMinute < weights.nightStartMinute) {
                metrics.timeOfDayScore += weights.afternoonWeight;
            } else {
                metrics.timeOfDayScore += weights.nightWeight;
            }
            const auto dayIdx = static_cast<std::size_t>(s.day);
            if (dayIdx < 7) {
                byDay[dayIdx].emplace_back(s.startMinute, s.endMinute);
            }
        }
    }

    if (metrics.totalSessionCount == 0) {
        metrics.earliestStartMinute = 0;
        metrics.latestEndMinute = 0;
    } else {
        for (auto& daySlots : byDay) {
            if (daySlots.empty()) {
                continue;
            }
            ++metrics.activeDaysCount;
            std::sort(daySlots.begin(), daySlots.end());
            const int firstStart = daySlots.front().first;
            int currentEnd = daySlots.front().second;
            int lastEnd = currentEnd;
            for (std::size_t i = 1; i < daySlots.size(); ++i) {
                if (daySlots[i].first > currentEnd) {
                    metrics.totalGapMinutes += (daySlots[i].first - currentEnd);
                }
                currentEnd = std::max(currentEnd, daySlots[i].second);
                lastEnd = std::max(lastEnd, daySlots[i].second);
            }
            metrics.totalSpanMinutes += (lastEnd - firstStart);
            metrics.earliestStartMinute = std::min(metrics.earliestStartMinute, firstStart);
            metrics.latestEndMinute = std::max(metrics.latestEndMinute, lastEnd);
        }
    }

    if (weights.legacySessionScoreOnly) {
        metrics.totalScore = metrics.timeOfDayScore;
    } else {
        metrics.totalScore = metrics.timeOfDayScore
            - weights.compactnessWeight * static_cast<double>(metrics.totalSpanMinutes)
            - weights.gapPenaltyPerMinute * static_cast<double>(metrics.totalGapMinutes)
            - weights.activeDayPenalty * static_cast<double>(metrics.activeDaysCount);
    }
    return metrics;
}

// Returns true if 'a' is strictly better (should appear earlier) than 'b'.
// Uses exact transitive comparison to satisfy C++ StrictWeakOrdering for std::sort.
bool IsBetterSchedule(
    const RankedSchedule& a,
    const RankedSchedule& b,
    bool deterministicTieBreak) {
    const bool aNan = std::isnan(a.metrics.totalScore);
    const bool bNan = std::isnan(b.metrics.totalScore);
    if (aNan != bNan) {
        return !aNan;
    }
    if (!aNan && a.metrics.totalScore != b.metrics.totalScore) {
        return a.metrics.totalScore > b.metrics.totalScore;
    }
    if (!deterministicTieBreak) {
        return false;
    }
    if (a.metrics.activeDaysCount != b.metrics.activeDaysCount) {
        return a.metrics.activeDaysCount < b.metrics.activeDaysCount;
    }
    if (a.metrics.totalGapMinutes != b.metrics.totalGapMinutes) {
        return a.metrics.totalGapMinutes < b.metrics.totalGapMinutes;
    }
    if (a.metrics.totalSpanMinutes != b.metrics.totalSpanMinutes) {
        return a.metrics.totalSpanMinutes < b.metrics.totalSpanMinutes;
    }
    const std::size_t count = std::min(a.selections.size(), b.selections.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (a.selections[i].courseCodeUtf8 != b.selections[i].courseCodeUtf8) {
            return a.selections[i].courseCodeUtf8 < b.selections[i].courseCodeUtf8;
        }
        if (a.selections[i].group.idUtf8 != b.selections[i].group.idUtf8) {
            return a.selections[i].group.idUtf8 < b.selections[i].group.idUtf8;
        }
    }
    return a.selections.size() < b.selections.size();
}

void RecordBlockingConflict(
    std::vector<ConflictDetail>& conflicts,
    const ConflictDetail& detail,
    std::size_t maxEntries = 32) {
    if (conflicts.size() >= maxEntries) {
        return;
    }
    for (const auto& existing : conflicts) {
        if (existing.courseAUtf8 == detail.courseAUtf8 &&
            existing.groupAUtf8 == detail.groupAUtf8 &&
            existing.courseBUtf8 == detail.courseBUtf8 &&
            existing.groupBUtf8 == detail.groupBUtf8 &&
            existing.day == detail.day &&
            existing.overlapStartMinute == detail.overlapStartMinute &&
            existing.overlapEndMinute == detail.overlapEndMinute) {
            return;
        }
    }
    conflicts.push_back(detail);
}

struct CandidateCourse {
    const Course* course = nullptr;
    std::vector<const Group*> groups;
};

} // namespace

bool TimeSlotsConflict(
    DayOfWeek dayA, int startA, int endA,
    DayOfWeek dayB, int startB, int endB,
    int minBufferMinutes) noexcept {
    if (dayA != dayB) {
        return false;
    }
    const long long buffer = (minBufferMinutes > 0) ? static_cast<long long>(minBufferMinutes) : 0LL;
    const long long sA = static_cast<long long>(startA);
    const long long eA = static_cast<long long>(endA);
    const long long sB = static_cast<long long>(startB);
    const long long eB = static_cast<long long>(endB);
    return (sA < (eB + buffer)) && (sB < (eA + buffer));
}

ScheduleResult HorariosSolver::Solve(
    const CourseCatalog& catalog,
    const std::vector<BusySlot>& busySlots,
    const SolverPreferences& prefs) {
    ScheduleResult result;

    if (prefs.minBreakMinutesBetweenClasses < 0) {
        result.status = SolveStatus::InvalidInput;
        result.diagnostics.errorsUtf8.push_back("'minBreakMinutesBetweenClasses' no puede ser negativo.");
        return result;
    }

    if (!std::isfinite(prefs.weights.morningWeight) ||
        !std::isfinite(prefs.weights.afternoonWeight) ||
        !std::isfinite(prefs.weights.nightWeight) ||
        !std::isfinite(prefs.weights.compactnessWeight) ||
        !std::isfinite(prefs.weights.gapPenaltyPerMinute) ||
        !std::isfinite(prefs.weights.activeDayPenalty)) {
        result.status = SolveStatus::InvalidInput;
        result.diagnostics.errorsUtf8.push_back("Los pesos de puntuación ('ScoringWeights') deben ser números finitos.");
        return result;
    }

    for (std::size_t i = 0; i < busySlots.size(); ++i) {
        if (!busySlots[i].IsValid()) {
            result.status = SolveStatus::InvalidInput;
            result.diagnostics.errorsUtf8.push_back(
                "Bloque ocupado #" + std::to_string(i) + " inválido (" +
                FormatTimeHHMM(busySlots[i].startMinute) + " - " +
                FormatTimeHHMM(busySlots[i].endMinute) + ").");
            return result;
        }
    }

    if (catalog.courses.empty()) {
        result.status = SolveStatus::InvalidInput;
        result.diagnostics.errorsUtf8.push_back("El catálogo de cursos está vacío.");
        return result;
    }

    std::unordered_set<std::string> seenCourseCodes;
    seenCourseCodes.reserve(catalog.courses.size());
    for (const auto& course : catalog.courses) {
        const std::string normCode = ToLowerUtf8Spanish(TrimAscii(course.codeUtf8));
        if (normCode.empty()) {
            result.status = SolveStatus::InvalidInput;
            result.diagnostics.errorsUtf8.push_back("Existe un curso con código vacío en el catálogo.");
            return result;
        }
        if (!seenCourseCodes.insert(normCode).second) {
            result.status = SolveStatus::InvalidInput;
            result.diagnostics.errorsUtf8.push_back(
                "Código de curso duplicado en el catálogo: " + course.codeUtf8);
            return result;
        }
        for (const auto& group : course.groups) {
            for (const auto& session : group.sessions) {
                if (!session.IsValid()) {
                    result.status = SolveStatus::InvalidInput;
                    result.diagnostics.errorsUtf8.push_back(
                        "Sesión inválida en " + course.codeUtf8 + " (" + group.idUtf8 + "): " +
                        FormatTimeHHMM(session.startMinute) + " - " + FormatTimeHHMM(session.endMinute) + ".");
                    return result;
                }
            }
            // Enforce strict non-overlap inside a single group (buffer = 0);
            // minBreakMinutesBetweenClasses governs breaks between distinct courses and busySlots.
            for (std::size_t si = 0; si < group.sessions.size(); ++si) {
                const auto& sa = group.sessions[si];
                for (std::size_t sj = si + 1; sj < group.sessions.size(); ++sj) {
                    const auto& sb = group.sessions[sj];
                    if (TimeSlotsConflict(sa.day, sa.startMinute, sa.endMinute,
                                          sb.day, sb.startMinute, sb.endMinute, 0)) {
                        result.status = SolveStatus::InvalidInput;
                        result.diagnostics.errorsUtf8.push_back(
                            "Sesiones solapadas o duplicadas dentro del mismo grupo en " +
                            course.codeUtf8 + " (" + group.idUtf8 + ").");
                        return result;
                    }
                }
            }
        }
    }

    // Resolve target course list
    std::vector<const Course*> selectedCourses;
    if (prefs.filters.requiredCourseCodes.empty()) {
        selectedCourses.reserve(catalog.courses.size());
        for (const auto& course : catalog.courses) {
            selectedCourses.push_back(&course);
        }
    } else {
        std::unordered_set<std::string> seenRequested;
        for (const auto& reqCode : prefs.filters.requiredCourseCodes) {
            const std::string normReq = ToLowerUtf8Spanish(TrimAscii(reqCode));
            if (normReq.empty()) {
                result.status = SolveStatus::InvalidInput;
                result.diagnostics.errorsUtf8.push_back("Código de curso vacío en 'requiredCourseCodes'.");
                return result;
            }
            if (!seenRequested.insert(normReq).second) {
                continue;
            }
            const Course* found = nullptr;
            for (const auto& course : catalog.courses) {
                if (ToLowerUtf8Spanish(TrimAscii(course.codeUtf8)) == normReq) {
                    found = &course;
                    break;
                }
            }
            if (!found) {
                result.status = SolveStatus::InvalidInput;
                result.diagnostics.errorsUtf8.push_back("Curso requerido no encontrado en el catálogo: " + reqCode);
                return result;
            }
            selectedCourses.push_back(found);
        }
    }

    result.diagnostics.candidateCoursesCount = selectedCourses.size();

    // Filter groups and check against busySlots
    std::vector<CandidateCourse> candidates;
    candidates.reserve(selectedCourses.size());
    bool anyCourseEmpty = false;

    for (const Course* course : selectedCourses) {
        CourseFilterDiagnostic cDiag;
        cDiag.courseCodeUtf8 = course->codeUtf8;
        cDiag.initialGroupCount = course->groups.size();

        const std::vector<std::string> allowedGroups =
            CollectGroupFilterList(prefs.filters.allowedGroupIdsByCourse, course->codeUtf8);
        const std::vector<std::string> excludedGroups =
            CollectGroupFilterList(prefs.filters.excludedGroupIdsByCourse, course->codeUtf8);

        std::vector<const Group*> afterFilter;
        for (const auto& group : course->groups) {
            if (!allowedGroups.empty() && !ContainsCaseInsensitive(allowedGroups, group.idUtf8)) {
                continue;
            }
            if (!excludedGroups.empty() && ContainsCaseInsensitive(excludedGroups, group.idUtf8)) {
                continue;
            }
            if (!prefs.filters.allowedModalities.empty() &&
                !ContainsCaseInsensitive(prefs.filters.allowedModalities, group.modalityUtf8)) {
                continue;
            }
            if (!GroupMatchesCampusFilter(group, catalog.campusUtf8, prefs.filters.allowedCampuses)) {
                continue;
            }
            if (!prefs.filters.excludedProfessors.empty() &&
                !TrimAscii(group.professorUtf8).empty() &&
                ContainsCaseInsensitive(prefs.filters.excludedProfessors, group.professorUtf8)) {
                continue;
            }
            if (prefs.filters.requireAvailableSeats) {
                if (group.seatsAvailable == 0) {
                    continue;
                }
                if (group.seatsAvailable < 0 && !prefs.filters.allowUnknownSeats) {
                    continue;
                }
            }
            if (prefs.filters.excludeNightClasses) {
                bool hasNight = false;
                for (const auto& s : group.sessions) {
                    if (s.startMinute >= prefs.weights.nightStartMinute) {
                        hasNight = true;
                        break;
                    }
                }
                if (hasNight) {
                    continue;
                }
            }
            afterFilter.push_back(&group);
        }
        cDiag.afterFilterGroupCount = afterFilter.size();

        std::vector<const Group*> afterBusy;
        for (const Group* group : afterFilter) {
            bool hitsBusy = false;
            for (const auto& s : group->sessions) {
                for (const auto& b : busySlots) {
                    if (TimeSlotsConflict(s.day, s.startMinute, s.endMinute,
                                          b.day, b.startMinute, b.endMinute,
                                          prefs.minBreakMinutesBetweenClasses)) {
                        hitsBusy = true;
                        ConflictDetail detail;
                        detail.courseAUtf8 = course->codeUtf8;
                        detail.groupAUtf8 = group->idUtf8;
                        detail.courseBUtf8 = std::string("BUSY:") + (b.labelUtf8.empty() ? "Bloqueado" : b.labelUtf8);
                        detail.groupBUtf8 = b.sourceUtf8;
                        detail.day = s.day;
                        ComputeConflictSpan(
                            s.startMinute, s.endMinute,
                            b.startMinute, b.endMinute,
                            detail.overlapStartMinute,
                            detail.overlapEndMinute);
                        RecordBlockingConflict(result.diagnostics.blockingConflicts, detail);
                        break;
                    }
                }
                if (hitsBusy) {
                    break;
                }
            }
            if (!hitsBusy) {
                afterBusy.push_back(group);
            }
        }
        cDiag.afterBusySlotGroupCount = afterBusy.size();

        if (cDiag.initialGroupCount == 0) {
            cDiag.reasonUtf8 = "El curso no tiene grupos registrados en el catálogo.";
            anyCourseEmpty = true;
        } else if (cDiag.afterFilterGroupCount == 0) {
            cDiag.reasonUtf8 = "Todos los grupos fueron descartados por los filtros activos.";
            anyCourseEmpty = true;
        } else if (cDiag.afterBusySlotGroupCount == 0) {
            cDiag.reasonUtf8 = "Todos los grupos restantes chocan con bloques ocupados (busySlots).";
            anyCourseEmpty = true;
        }

        result.diagnostics.courseDiagnostics.push_back(std::move(cDiag));
        CandidateCourse cand;
        cand.course = course;
        cand.groups = std::move(afterBusy);
        candidates.push_back(std::move(cand));
    }

    if (anyCourseEmpty) {
        result.ok = false;
        result.status = SolveStatus::NoConflictFreeSchedule;
        result.diagnostics.warningsUtf8.push_back("No fue posible armar un horario sin choques con las restricciones actuales.");
        return result;
    }

    // Order search indices by MRV (fewest groups first) while keeping output selections in original order
    const std::size_t nCourses = candidates.size();
    std::vector<std::size_t> searchOrder(nCourses);
    std::iota(searchOrder.begin(), searchOrder.end(), 0);
    std::stable_sort(searchOrder.begin(), searchOrder.end(), [&](std::size_t a, std::size_t b) {
        return candidates[a].groups.size() < candidates[b].groups.size();
    });

    std::vector<const Group*> chosenByCourse(nCourses, nullptr);
    std::vector<std::size_t> chosenOrderHistory;
    chosenOrderHistory.reserve(nCourses);

    std::vector<RankedSchedule> collected;
    constexpr std::size_t kMaxSafeResults = std::numeric_limits<std::size_t>::max() / 8;
    const std::size_t trimThreshold =
        (prefs.maxResults == 0 || prefs.maxResults > kMaxSafeResults)
            ? 0
            : std::max<std::size_t>(prefs.maxResults * 8, 4096);

    auto comparator = [&](const RankedSchedule& a, const RankedSchedule& b) {
        return IsBetterSchedule(a, b, prefs.sortDeterministically);
    };

    const int minBreak = prefs.minBreakMinutesBetweenClasses;
    const std::size_t maxStates = prefs.maxSearchStates;

    auto dfs = [&](auto& self, std::size_t depth) -> void {
        if (result.diagnostics.searchBudgetExhausted) {
            return;
        }
        if (depth == nCourses) {
            ++result.diagnostics.validSchedulesFound;
            RankedSchedule ranked;
            ranked.selections.reserve(nCourses);
            for (std::size_t i = 0; i < nCourses; ++i) {
                ScheduleSelection sel;
                sel.courseCodeUtf8 = candidates[i].course->codeUtf8;
                sel.courseNameUtf8 = candidates[i].course->nameUtf8;
                sel.colorCssUtf8 = candidates[i].course->colorCssUtf8;
                sel.colorRgba = candidates[i].course->colorRgba;
                sel.credits = candidates[i].course->credits;
                sel.group = *chosenByCourse[i];
                ranked.selections.push_back(std::move(sel));
            }
            ranked.metrics = ComputeScheduleMetrics(ranked.selections, prefs.weights);
            collected.push_back(std::move(ranked));

            if (trimThreshold > 0 && collected.size() >= trimThreshold) {
                std::sort(collected.begin(), collected.end(), comparator);
                if (collected.size() > prefs.maxResults) {
                    collected.resize(prefs.maxResults);
                }
            }
            return;
        }

        const std::size_t cIdx = searchOrder[depth];
        const CandidateCourse& cand = candidates[cIdx];

        for (const Group* grp : cand.groups) {
            if (maxStates > 0 && result.diagnostics.combinationsExplored >= maxStates) {
                result.diagnostics.searchBudgetExhausted = true;
                return;
            }
            ++result.diagnostics.combinationsExplored;

            bool conflictFound = false;
            ConflictDetail conflictDetail;
            for (std::size_t prevIdx : chosenOrderHistory) {
                if (GroupsConflict(*grp, *chosenByCourse[prevIdx], minBreak,
                                   cand.course->codeUtf8, candidates[prevIdx].course->codeUtf8,
                                   &conflictDetail)) {
                    conflictFound = true;
                    break;
                }
            }
            if (conflictFound) {
                ++result.diagnostics.prunedBranchesCount;
                RecordBlockingConflict(result.diagnostics.blockingConflicts, conflictDetail);
                continue;
            }

            chosenByCourse[cIdx] = grp;
            chosenOrderHistory.push_back(cIdx);
            self(self, depth + 1);
            chosenOrderHistory.pop_back();
            chosenByCourse[cIdx] = nullptr;

            if (result.diagnostics.searchBudgetExhausted) {
                return;
            }
        }
    };

    dfs(dfs, 0);

    std::sort(collected.begin(), collected.end(), comparator);
    if (prefs.maxResults > 0 && collected.size() > prefs.maxResults) {
        collected.resize(prefs.maxResults);
    }
    for (std::size_t i = 0; i < collected.size(); ++i) {
        collected[i].rank = i + 1;
    }

    result.schedules = std::move(collected);
    if (result.diagnostics.searchBudgetExhausted) {
        result.status = SolveStatus::SearchBudgetExceeded;
        result.ok = !result.schedules.empty();
        result.diagnostics.warningsUtf8.push_back("Se alcanzó el límite máximo de estados de búsqueda (maxSearchStates).");
    } else if (result.schedules.empty()) {
        result.status = SolveStatus::NoConflictFreeSchedule;
        result.ok = false;
        result.diagnostics.warningsUtf8.push_back("No existen combinaciones libres de choques entre los grupos seleccionados.");
    } else {
        result.status = SolveStatus::Ok;
        result.ok = true;
        // Clear intermediate pruning conflicts when valid schedules exist
        result.diagnostics.blockingConflicts.clear();
    }

    return result;
}

ScheduleResult HorariosSolver::SolveFromJson(
    const std::string& catalogJsonUtf8,
    const std::string& busySlotsJsonUtf8,
    const SolverPreferences& prefs) {
    CatalogParseResult catRes = ParseCatalogJson(catalogJsonUtf8);
    if (!catRes.ok) {
        ScheduleResult err;
        err.ok = false;
        err.status = SolveStatus::InvalidInput;
        err.diagnostics.errorsUtf8.push_back(catRes.errorUtf8);
        return err;
    }

    std::vector<BusySlot> busySlots;
    std::vector<std::string> parseWarnings = std::move(catRes.warningsUtf8);

    if (!TrimAscii(busySlotsJsonUtf8).empty()) {
        BusySlotsParseResult busyRes = ParseBusySlotsJson(busySlotsJsonUtf8);
        if (!busyRes.ok) {
            ScheduleResult err;
            err.ok = false;
            err.status = SolveStatus::InvalidInput;
            err.diagnostics.errorsUtf8.push_back(busyRes.errorUtf8);
            return err;
        }
        busySlots = std::move(busyRes.busySlots);
        parseWarnings.insert(parseWarnings.end(), busyRes.warningsUtf8.begin(), busyRes.warningsUtf8.end());
    }

    ScheduleResult solved = Solve(catRes.catalog, busySlots, prefs);
    solved.diagnostics.warningsUtf8.insert(
        solved.diagnostics.warningsUtf8.begin(),
        parseWarnings.begin(),
        parseWarnings.end());
    return solved;
}

ScheduleResult SolveSchedules(
    const CourseCatalog& catalog,
    const std::vector<BusySlot>& busySlots,
    const SolverPreferences& prefs) {
    return HorariosSolver::Solve(catalog, busySlots, prefs);
}

} // namespace academic::horarios_tec
