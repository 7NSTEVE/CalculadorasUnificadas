#include "academic/matricula_tec/scenario_engine.hpp"
#include "academic/matricula_tec/protocol.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <set>

namespace academic::matricula_tec {

namespace {

std::string UpperAscii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    return UpperAscii(haystack).contains(UpperAscii(needle));
}

int GroupPreferenceRank(const CourseTarget& target, const CourseGroupLive& g) {
    // Lower is better
    for (size_t i = 0; i < target.preferred_groups.size(); ++i) {
        if (target.preferred_groups[i] == g.grupo) {
            return static_cast<int>(i);
        }
    }
    int bonus = 1000;
    for (const auto& pref_prof : target.filter.preferred_professors) {
        for (const auto& prof : g.profesores) {
            if (ContainsCaseInsensitive(prof, pref_prof)) {
                bonus -= 200;
                break;
            }
        }
    }
    return bonus + g.grupo;
}

} // namespace

bool ScenarioEngine::GroupMatchesFilter(const CourseGroupLive& group, const ScheduleFilterRule& filter) {
    if (filter.require_seats && group.cupo_disponible <= 0) {
        return false;
    }
    if (!filter.allowed_groups.empty() &&
        !std::ranges::contains(filter.allowed_groups, group.grupo)) {
        return false;
    }
    if (std::ranges::contains(filter.excluded_groups, group.grupo)) {
        return false;
    }
    if (!filter.allowed_campuses.empty()) {
        bool campus_ok = false;
        for (const auto& camp : filter.allowed_campuses) {
            if (ContainsCaseInsensitive(group.sede_id, camp) ||
                ContainsCaseInsensitive(group.sede_nombre, camp)) {
                campus_ok = true;
                break;
            }
        }
        if (!campus_ok) return false;
    }
    if (!filter.excluded_professors.empty()) {
        for (const auto& ex_p : filter.excluded_professors) {
            for (const auto& p : group.profesores) {
                if (ContainsCaseInsensitive(p, ex_p)) return false;
            }
        }
    }
    for (const auto& s : group.sesiones) {
        if (s.day_index < 0) continue;
        if (!filter.allowed_days.empty() &&
            !std::ranges::contains(filter.allowed_days, s.day_index)) {
            return false;
        }
        if (std::ranges::contains(filter.excluded_days, s.day_index)) {
            return false;
        }
        if (s.start_minutes >= 0 && s.start_minutes < filter.earliest_start_minutes) {
            return false;
        }
        if (s.end_minutes > 0 && s.end_minutes > filter.latest_end_minutes) {
            return false;
        }
    }
    return true;
}

ScenarioEvaluation ScenarioEngine::EvaluateScenario(
    const EnrollmentScenario& scenario,
    const std::map<std::string, std::vector<CourseGroupLive>>& live_groups_by_course,
    const std::vector<EnrolledScheduleItem>& currently_enrolled,
    PivotPolicy pivot_policy,
    int min_enrolled_safety_floor,
    int enabled_scenario_count)
{
    ScenarioEvaluation eval{};
    eval.scenario_id = scenario.id;
    eval.scenario_name = scenario.name;

    std::set<std::string> enrolled_codes;
    std::vector<ScheduleSession> fixed_sessions;
    for (const auto& en : currently_enrolled) {
        enrolled_codes.insert(UpperAscii(en.course_code));
        fixed_sessions.insert(fixed_sessions.end(), en.sesiones.begin(), en.sesiones.end());
    }

    struct TargetCandidates {
        CourseTarget target;
        std::vector<CourseGroupLive> candidates;
    };

    std::vector<TargetCandidates> pending;
    for (const auto& t : scenario.targets) {
        const std::string code = UpperAscii(t.course_code);
        if (enrolled_codes.contains(code)) {
            continue; // Already enrolled
        }
        TargetCandidates tc{ t, {} };
        auto it = live_groups_by_course.find(code);
        if (it != live_groups_by_course.end()) {
            for (const auto& g : it->second) {
                if (!GroupMatchesFilter(g, t.filter)) continue;
                if (!t.preferred_groups.empty() && t.filter.allowed_groups.empty()) {
                    // Preferred groups are ordered first, while other filter-matching groups stay eligible
                }
                tc.candidates.push_back(g);
            }
        }
        std::ranges::sort(tc.candidates, [&](const CourseGroupLive& a, const CourseGroupLive& b) {
            const int ra = GroupPreferenceRank(t, a);
            const int rb = GroupPreferenceRank(t, b);
            if (ra != rb) return ra < rb;
            return a.cupo_disponible > b.cupo_disponible;
        });

        if (tc.candidates.empty()) {
            if (!t.optional_course) {
                eval.missing_courses.push_back(code);
            }
        } else {
            pending.push_back(std::move(tc));
        }
    }

    if (!eval.missing_courses.empty()) {
        eval.feasible = false;
        return eval;
    }

    // Backtracking conflict-free solver against fixed_sessions (strictly conservative first)
    auto solve_against = [&](const std::vector<ScheduleSession>& locked_sessions,
                             std::vector<SelectedGroupChoice>& out_choices) -> bool {
        out_choices.clear();
        std::vector<ScheduleSession> acc = locked_sessions;

        auto dfs = [&](auto&& self, size_t idx) -> bool {
            if (idx == pending.size()) return true;
            const auto& tc = pending[idx];
            for (const auto& cand : tc.candidates) {
                if (SessionVectorsOverlap(cand.sesiones, acc)) {
                    continue;
                }
                const size_t prev_sz = acc.size();
                acc.insert(acc.end(), cand.sesiones.begin(), cand.sesiones.end());
                SelectedGroupChoice choice{};
                choice.course_code = UpperAscii(tc.target.course_code);
                choice.grupo = cand.grupo;
                choice.cupo_disponible = cand.cupo_disponible;
                choice.preference_rank = GroupPreferenceRank(tc.target, cand);
                choice.corequisite_code = tc.target.corequisite_code;
                choice.corequisite_group = tc.target.corequisite_group;
                choice.group_data = cand;
                out_choices.push_back(std::move(choice));

                if (self(self, idx + 1)) return true;

                out_choices.pop_back();
                acc.resize(prev_sz);
            }
            if (tc.target.optional_course) {
                return self(self, idx + 1);
            }
            return false;
        };

        return dfs(dfs, 0);
    };

    std::vector<SelectedGroupChoice> chosen;
    if (solve_against(fixed_sessions, chosen)) {
        eval.feasible = true;
    } else if (pivot_policy == PivotPolicy::AllowPriorityScenarioReplace &&
               enabled_scenario_count >= 2 &&
               !currently_enrolled.empty()) {
        // Opt-in replacement: identify ONLY conflicting enrolled courses whose removal preserves
        // min_enrolled_safety_floor (NEVER drop to 0 -> prevents SMaterias!).
        std::vector<SelectedGroupChoice> free_chosen;
        if (solve_against({}, free_chosen) && !free_chosen.empty()) {
            // Verify every target choice has live verified seats > 0
            const bool all_seats_positive = std::ranges::all_of(free_chosen, [](const auto& c) {
                return c.cupo_disponible > 0;
            });
            std::vector<std::string> drops;
            std::vector<ScheduleSession> chosen_sessions;
            for (const auto& c : free_chosen) {
                chosen_sessions.insert(chosen_sessions.end(), c.group_data.sesiones.begin(), c.group_data.sesiones.end());
            }
            for (const auto& en : currently_enrolled) {
                if (SessionVectorsOverlap(en.sesiones, chosen_sessions)) {
                    drops.push_back(UpperAscii(en.course_code));
                }
            }
            const int remaining_after_drop = static_cast<int>(currently_enrolled.size()) - static_cast<int>(drops.size());
            const int safety_floor = std::max(1, min_enrolled_safety_floor);
            if (all_seats_positive && !drops.empty() && remaining_after_drop >= safety_floor) {
                chosen = std::move(free_chosen);
                eval.drop_courses_required = std::move(drops);
                eval.feasible = true;
            } else {
                eval.conflict_notes.push_back(
                    "Reemplazo bloqueado por piso de seguridad SMaterias (min_enrolled_courses_safety_floor=" +
                    std::to_string(safety_floor) + ").");
            }
        }
    } else {
        eval.conflict_notes.push_back("Choque horario con cursos matriculados o entre materias del escenario.");
    }

    if (eval.feasible) {
        // MRV ordering: enroll tightest seat availability first!
        std::ranges::sort(chosen, [](const SelectedGroupChoice& a, const SelectedGroupChoice& b) {
            if (a.cupo_disponible != b.cupo_disponible) return a.cupo_disponible < b.cupo_disponible;
            return a.preference_rank < b.preference_rank;
        });
        eval.choices = std::move(chosen);
    }
    return eval;
}

ScenarioEvaluation ScenarioEngine::SelectBestPlan(
    const ScenarioPlan& plan,
    const std::map<std::string, std::vector<CourseGroupLive>>& live_groups_by_course,
    const std::vector<EnrolledScheduleItem>& currently_enrolled)
{
    std::vector<EnrollmentScenario> enabled_scenarios;
    for (const auto& sc : plan.scenarios) {
        if (sc.enabled) enabled_scenarios.push_back(sc);
    }
    std::ranges::sort(enabled_scenarios, [](const auto& a, const auto& b) {
        return a.priority < b.priority;
    });

    const int enabled_count = static_cast<int>(enabled_scenarios.size());
    for (const auto& sc : enabled_scenarios) {
        ScenarioEvaluation ev = EvaluateScenario(
            sc,
            live_groups_by_course,
            currently_enrolled,
            plan.pivot_policy,
            plan.min_enrolled_courses_safety_floor,
            enabled_count);
        if (ev.feasible) {
            return ev;
        }
    }

    // Optional Plan Z Rescue: maximum conflict-free subset across all targets in priority order
    if (plan.enable_plan_z_rescue && !enabled_scenarios.empty()) {
        ScenarioEvaluation z{};
        z.scenario_id = "plan_z_rescue";
        z.scenario_name = "Plan Z (Rescate parcial sin choques)";
        z.is_plan_z = true;

        std::set<std::string> seen_courses;
        std::vector<ScheduleSession> acc;
        for (const auto& en : currently_enrolled) {
            seen_courses.insert(UpperAscii(en.course_code));
            acc.insert(acc.end(), en.sesiones.begin(), en.sesiones.end());
        }

        for (const auto& sc : enabled_scenarios) {
            for (const auto& t : sc.targets) {
                const std::string code = UpperAscii(t.course_code);
                if (seen_courses.contains(code)) continue;
                auto it = live_groups_by_course.find(code);
                if (it == live_groups_by_course.end()) continue;

                std::vector<CourseGroupLive> cands;
                for (const auto& g : it->second) {
                    if (GroupMatchesFilter(g, t.filter) && !SessionVectorsOverlap(g.sesiones, acc)) {
                        cands.push_back(g);
                    }
                }
                if (cands.empty()) continue;
                std::ranges::sort(cands, [&](const auto& a, const auto& b) {
                    return GroupPreferenceRank(t, a) < GroupPreferenceRank(t, b);
                });

                const auto& best = cands.front();
                SelectedGroupChoice ch{};
                ch.course_code = code;
                ch.grupo = best.grupo;
                ch.cupo_disponible = best.cupo_disponible;
                ch.preference_rank = GroupPreferenceRank(t, best);
                ch.corequisite_code = t.corequisite_code;
                ch.corequisite_group = t.corequisite_group;
                ch.group_data = best;
                z.choices.push_back(std::move(ch));
                acc.insert(acc.end(), best.sesiones.begin(), best.sesiones.end());
                seen_courses.insert(code);
            }
        }

        if (!z.choices.empty()) {
            std::ranges::sort(z.choices, [](const auto& a, const auto& b) {
                return a.cupo_disponible < b.cupo_disponible;
            });
            z.feasible = true;
            return z;
        }
    }

    ScenarioEvaluation none{};
    none.feasible = false;
    none.scenario_name = "Sin escenario viable";
    none.conflict_notes.push_back("Ningun escenario tiene cupo disponible sin choques.");
    return none;
}

} // namespace academic::matricula_tec
