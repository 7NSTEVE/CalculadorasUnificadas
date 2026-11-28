#pragma once

#include "academic/matricula_tec/models.hpp"
#include "academic/tec_digital/cookie_jar.hpp"
#include "academic/tec_digital/http_transport.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace academic::matricula_tec {

class MatriculaClient {
public:
    explicit MatriculaClient(
        std::string base_url = kDefaultBaseUrl,
        std::unique_ptr<academic::tec_digital::IHttpTransport> transport = nullptr);

    void SetBaseUrl(const std::string& base_url);
    std::string BaseUrl() const;

    // Hard safety lock: mutating endpoints are REFUSED unless armed == true && dry_run == false.
    bool ArmLiveMutations(bool armed, const std::string& confirmation_phrase, std::string* out_err = nullptr);
    void Disarm() noexcept;
    bool IsArmed() const noexcept;

    // Authentication & session bootstrap (resets armed_ = false on login for safety).
    bool Login(const std::string& username, const std::string& password, std::string* out_err = nullptr);
    void LogoutLocal();
    bool IsAuthenticated() const noexcept;
    StudentBootstrap Bootstrap() const;

    // Server clock synchronization from HTTP Date: header (RFC 2822 UTC 24h).
    bool SyncServerClock(int64_t* out_offset_ms = nullptr, std::string* out_err = nullptr);
    int64_t ServerClockOffsetMs() const noexcept;
    int64_t EstimatedServerEpochSeconds() const noexcept;

    // Read-only official WebMethods
    bool FetchCoursesAndBindConvocatoria(const std::string& id_convocatoria,
                                         const std::string& id_periodo,
                                         std::vector<CourseOffer>& out_courses,
                                         std::string* out_err = nullptr);
    bool FetchLiveGroups(const std::string& course_code,
                         std::vector<CourseGroupLive>& out_groups,
                         std::string* out_err = nullptr);
    bool FetchCita(CitaInfo& out_cita, std::string* out_err = nullptr);
    bool FetchMorosidad(MorosidadInfo& out_morosidad, std::string* out_err = nullptr);
    bool FetchEnrolledSchedule(std::vector<EnrolledScheduleItem>& out_enrolled, std::string* out_err = nullptr);
    bool GenerateInforme(const std::string& tipo, InformeSummary& out_informe, std::string* out_err = nullptr);

    // Guarded mutating official WebMethods (enforce dry_run & armed_ safety lock!)
    EnrollmentResult EnrollCourse(const std::string& course_code,
                                  int grupo,
                                  bool dry_run = true,
                                  const std::string& corequisite_code = "",
                                  int corequisite_group = -1);
    EnrollmentResult DropCourse(const std::string& course_code,
                                int grupo,
                                bool dry_run = true);
    EnrollmentResult SendInformeEmail(const std::string& html_informe,
                                      bool dry_run = true);

    // Lightweight TLS reachability check (read-only GET frmAutenticacion.aspx, zero credentials).
    bool ProbePortalReachability(int* out_http_status, bool* out_tls_verified, std::string* out_err = nullptr);

    // Sets polite delay between consecutive network requests (default 150 ms; 0 for local mocks).
    void SetPacingMs(int ms) noexcept;

private:
    academic::tec_digital::HttpResponse PerformRequest(academic::tec_digital::HttpRequest req);
    bool CallWebMethod(const std::string& page_and_method,
                       const nlohmann::json& payload,
                       nlohmann::json& out_unwrapped,
                       int* out_status = nullptr,
                       std::string* out_err = nullptr);
    void UpdateClockOffsetFromResponse(const academic::tec_digital::HttpResponse& resp);
    void EnforcePacing();

    mutable std::mutex mtx_;
    std::string base_url_;
    std::unique_ptr<academic::tec_digital::IHttpTransport> transport_;
    academic::tec_digital::CookieJar cookies_;
    StudentBootstrap bootstrap_;
    std::atomic<bool> authenticated_{ false };
    std::atomic<bool> armed_{ false };
    std::atomic<int64_t> server_offset_ms_{ 0 };
    std::atomic<int> pacing_ms_{ 150 };
    int64_t last_req_steady_ms_ = 0;
    std::string bound_convocatoria_;
    std::string bound_periodo_;
};

} // namespace academic::matricula_tec
