#pragma once

#include "academic/tec_digital/academic_models.hpp"
#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/tda_tds_client.hpp"
#include "academic/tec_digital/types.hpp"

#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace academic::tec_digital {

struct InclusionCampaignInfo {
    std::string id;
    std::string name;
    std::string period;
    std::string start_date;
    std::string end_date;
    std::string student_entity;
    bool active = false;
};

struct PendingGradCourse {
    std::string id;
    std::string code;
    std::string name;
    int credits = 0;
};

struct CourseRequisitesCheck {
    std::string course_code;
    bool all_requirements_met = true;
    bool all_corequirements_met = true;
    bool has_overlap = false;
    std::vector<std::string> missing_requirements;
    std::vector<std::string> missing_corequirements;
    std::string message;
};

struct InclusionApplicationRecord {
    std::string id;
    std::string course_code;
    std::string course_name;
    std::string group;
    std::string status;
    std::string comment;
    std::string updated_at;
};

struct ScholarshipStatusInfo {
    std::string campaign_id;
    std::string status;
    std::string scholarship_type;
    std::string detail;
};

struct GraphQlActualCourse {
    std::string id;
    std::string code;
    std::string materia;
    int credits = 0;
    std::vector<std::string> professors;
    std::vector<std::string> schedule;
};

struct StudentRnCourse {
    std::string course_code;
    int rn = 0;
    std::string course_name;
};

struct StudentGeneralInfo {
    std::string career;
    double weighted_average = 0.0;
    int outstanding_courses = 0;
    int available_registration_credits = 0;
    std::vector<StudentRnCourse> rn_courses;
};

struct GraphQlCurriculumBundle {
    CurriculumGraph graph;
    StudentCurriculumProfile profile;
};

class TecGraphQlClient {
public:
    TecGraphQlClient(PolicyEnforcingTransport& transport,
                     AuthClient& auth,
                     std::string base_url = "https://tecdigital.tec.ac.cr",
                     std::string graphql_url = "https://api.tec.ac.cr:3005/graphql",
                     SanitizeOptions sanitize_opts = {});
    ~TecGraphQlClient();

    void SetSanitizeOptions(SanitizeOptions sanitize_opts);
    void ClearSessionToken();
    std::string CachedUserId() const;

    Result<std::string> FetchUserType();
    Result<InclusionCampaignInfo> FetchStudentActiveCampaign(std::string_view request_user_id = "");
    Result<std::vector<PendingGradCourse>> FetchPendingGraduationCourses(
        std::string_view user_id,
        std::string_view carne);
    Result<CourseRequisitesCheck> CheckCourseRequisites(
        std::string_view user_id,
        std::string_view carne,
        std::string_view campaign_id,
        std::string_view course_id,
        std::string_view cod_asignatura,
        std::string_view period);
    Result<std::vector<InclusionApplicationRecord>> FetchStudentApplications(
        std::string_view campaign_id,
        std::string_view carne);
    Result<ScholarshipStatusInfo> FetchScholarshipStatus(std::string_view campaign_id);
    Result<std::vector<GraphQlActualCourse>> FetchActualPeriodCourses(std::string_view carne);
    Result<AcademicStatsSummary> FetchAcademicHistoryFallback(std::string_view carne);
    Result<StudentGeneralInfo> FetchStudentGeneralInfo(std::string_view carne);
    Result<GraphQlCurriculumBundle> FetchStudentCurriculum(std::string_view carne);

private:
    Sanitizer SnapshotSanitizer() const;
    Result<std::string> EnsureTransactionToken(bool force_refresh = false);
    Result<std::string> ExecuteQueryJson(std::string_view operation_name,
                                         std::string_view query,
                                         std::string_view variables_json);

    PolicyEnforcingTransport& transport_;
    AuthClient& auth_;
    std::string base_url_;
    std::string graphql_url_;
    mutable std::mutex sanitizer_mutex_;
    Sanitizer sanitizer_;
    mutable std::mutex token_mutex_;
    std::string cached_jwt_;
    std::string cached_user_id_;
};

}  // namespace academic::tec_digital
