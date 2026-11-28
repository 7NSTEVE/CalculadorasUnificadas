#pragma once

#include "academic/matricula_tec/client.hpp"
#include "academic/matricula_tec/models.hpp"
#include "academic/matricula_tec/scraper_bridge.hpp"
#include "academic/matricula_tec/vault.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace academic::matricula_tec {

using StatusCallback = std::function<void(const AppointmentStatus&)>;
using SniperCallback = std::function<void(const SniperStatus&)>;

// Autonomous Appointment Orchestrator (T - 2:00 pre-login, clock sync, keep-alive,
// T - 0:00 refresh/re-login, Plan A -> B -> C -> Z execution, Playwright failover,
// Mi Informe -> Enviar) + Background Seat Sniper.
class AppointmentScheduler {
public:
    explicit AppointmentScheduler(
        std::shared_ptr<MatriculaClient> client = nullptr,
        std::shared_ptr<ScraperBridge> scraper = nullptr);
    ~AppointmentScheduler();

    AppointmentScheduler(const AppointmentScheduler&) = delete;
    AppointmentScheduler& operator=(const AppointmentScheduler&) = delete;

    // Stores credentials in RAM-protected vault for autonomous pre-login / re-login.
    bool SetCredentials(const std::string& username, const std::string& password, std::string* out_err = nullptr);
    bool LoadCredentialsFromDpapi(const std::string& path_utf8 = "", std::string* out_err = nullptr);
    void ClearCredentials() noexcept;
    bool HasCredentials() const noexcept;

    // Explicit live arming gate (required when config.dry_run == false).
    bool ArmMission(bool armed, const std::string& confirmation_phrase, std::string* out_err = nullptr);
    void Disarm() noexcept;
    bool IsArmed() const noexcept;

    // Starts/stops the autonomous appointment mission on a dedicated worker thread.
    bool StartMission(const AppointmentMissionConfig& config,
                      StatusCallback on_status = nullptr,
                      std::string* out_err = nullptr);
    void StopMission();
    void WaitForMission();
    AppointmentStatus CurrentStatus() const;

    // Background Seat Sniper during the enrollment period.
    bool StartSniper(const SniperConfig& config,
                     SniperCallback on_status = nullptr,
                     std::string* out_err = nullptr);
    void StopSniper();
    SniperStatus CurrentSniperStatus() const;

    std::shared_ptr<MatriculaClient> Client() const noexcept { return client_; }

private:
    void RunMissionLoop(AppointmentMissionConfig cfg);
    void RunSniperLoop(SniperConfig cfg);
    void AppendLogLocked(const std::string& msg);
    void NotifyStatusLocked();
    void NotifySniperLocked();
    bool PerformLoginWithOptionalRearm(bool mission_armed, std::string* out_err);

    std::shared_ptr<MatriculaClient> client_;
    std::shared_ptr<ScraperBridge> scraper_;
    ProtectedCredentialVault vault_;

    mutable std::mutex mtx_;
    std::atomic<bool> mission_armed_{ false };
    std::string confirmed_phrase_;

    std::atomic<bool> mission_stop_{ false };
    std::thread mission_thread_;
    AppointmentStatus status_;
    StatusCallback status_cb_;

    std::atomic<bool> sniper_stop_{ false };
    std::thread sniper_thread_;
    SniperStatus sniper_status_;
    SniperCallback sniper_cb_;
};

} // namespace academic::matricula_tec
