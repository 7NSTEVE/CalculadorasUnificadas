#pragma once

#include "academic/matricula_tec/models.hpp"

#include <map>
#include <string>
#include <vector>

namespace academic::matricula_tec {

class ScenarioEngine {
public:
    // Checks whether a live group satisfies a dynamic ScheduleFilterRule.
    static bool GroupMatchesFilter(const CourseGroupLive& group, const ScheduleFilterRule& filter);

    // Evaluates a single scenario against live group availability and already-enrolled schedule.
    // Returns MRV-ordered conflict-free choices when feasible.
    static ScenarioEvaluation EvaluateScenario(
        const EnrollmentScenario& scenario,
        const std::map<std::string, std::vector<CourseGroupLive>>& live_groups_by_course,
        const std::vector<EnrolledScheduleItem>& currently_enrolled,
        PivotPolicy pivot_policy,
        int min_enrolled_safety_floor,
        int enabled_scenario_count);

    // Evaluates Plan A -> Plan B -> Plan C (sorted by priority asc), falling back to Plan Z
    // (maximum feasible conflict-free subset across all targets) when enabled.
    static ScenarioEvaluation SelectBestPlan(
        const ScenarioPlan& plan,
        const std::map<std::string, std::vector<CourseGroupLive>>& live_groups_by_course,
        const std::vector<EnrolledScheduleItem>& currently_enrolled);
};

} // namespace academic::matricula_tec
