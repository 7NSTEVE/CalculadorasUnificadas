#pragma once

#include "academic/matricula_tec/models.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace academic::matricula_tec {

// Out-of-process JSON-Lines stdin/stdout bridge to tools/tec_matricula/matricula_scraper_worker.py.
// Credentials are NEVER passed in command-line arguments.
class ScraperBridge {
public:
    ScraperBridge();
    ~ScraperBridge();

    ScraperBridge(const ScraperBridge&) = delete;
    ScraperBridge& operator=(const ScraperBridge&) = delete;

    void SetWorkerScriptPath(const std::string& path_utf8);
    void SetPythonExePath(const std::string& path_utf8);
    static std::string DiscoverPythonExeUtf8();
    static std::string DiscoverWorkerScriptUtf8();

    bool Start(bool headless = true, std::string* out_err = nullptr);
    void Stop();
    bool IsRunning() const noexcept;

    bool Login(const std::string& base_url,
               const std::string& username,
               const std::string& password,
               const std::string& convocatoria_id,
               const std::string& periodo_id,
               std::string* out_err = nullptr);
    bool FetchLiveGroups(const std::string& course_code,
                         std::vector<CourseGroupLive>& out_groups,
                         std::string* out_err = nullptr);
    EnrollmentResult EnrollGroup(const std::string& course_code,
                                 int grupo,
                                 bool dry_run,
                                 bool armed,
                                 const std::string& confirmation_phrase);
    EnrollmentResult SendInforme(bool dry_run,
                                 bool armed,
                                 const std::string& confirmation_phrase);

private:
    bool SendCommand(const nlohmann::json& cmd, nlohmann::json& out_reply, std::string* out_err);

    mutable std::mutex mtx_;
    std::string python_exe_utf8_;
    std::string worker_script_utf8_;
    void* proc_handle_ = nullptr;
    void* stdin_write_ = nullptr;
    void* stdout_read_ = nullptr;
    bool running_ = false;
};

} // namespace academic::matricula_tec
