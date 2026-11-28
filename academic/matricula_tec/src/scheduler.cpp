#include "academic/matricula_tec/scheduler.hpp"
#include "academic/matricula_tec/protocol.hpp"
#include "academic/matricula_tec/scenario_engine.hpp"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <random>
#include <set>

namespace academic::matricula_tec {

namespace {

std::string StateToLabel(SchedulerState s) {
    switch (s) {
        case SchedulerState::Idle: return "Inactivo";
        case SchedulerState::WaitingForPreLogin: return "Esperando pre-login (T-2:00)";
        case SchedulerState::PreLoggingIn: return "Iniciando pre-login";
        case SchedulerState::PreLoggedInStandby: return "Pre-sesion lista (standby)";
        case SchedulerState::ZeroHourRefreshing: return "Hora cero: refrescando sesion";
        case SchedulerState::ExecutingPlan: return "Ejecutando escenarios";
        case SchedulerState::SendingInforme: return "Enviando Mi Informe";
        case SchedulerState::Completed: return "Completado";
        case SchedulerState::Failed: return "Fallido";
        case SchedulerState::Cancelled: return "Cancelado";
    }
    return "Desconocido";
}

std::string NowUtcStamp() {
    const auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm_utc{};
    gmtime_s(&tm_utc, &tt);
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%H:%M:%S UTC", &tm_utc);
    return std::string(buf);
}

std::set<std::string> CollectPlanCourseCodes(const ScenarioPlan& plan) {
    std::set<std::string> codes;
    for (const auto& sc : plan.scenarios) {
        if (!sc.enabled) continue;
        for (const auto& t : sc.targets) {
            if (!t.course_code.empty()) codes.insert(t.course_code);
        }
    }
    return codes;
}

} // namespace

AppointmentScheduler::AppointmentScheduler(
    std::shared_ptr<MatriculaClient> client,
    std::shared_ptr<ScraperBridge> scraper)
    : client_(client ? std::move(client) : std::make_shared<MatriculaClient>()),
      scraper_(scraper ? std::move(scraper) : std::make_shared<ScraperBridge>())
{
    status_.state_label = StateToLabel(status_.state);
}

AppointmentScheduler::~AppointmentScheduler() {
    StopMission();
    StopSniper();
    ClearCredentials();
}

bool AppointmentScheduler::SetCredentials(const std::string& username, const std::string& password, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    return vault_.StoreInMemory(username, password, out_err);
}

bool AppointmentScheduler::LoadCredentialsFromDpapi(const std::string& path_utf8, std::string* out_err) {
    const std::string path = path_utf8.empty() ? DefaultVaultPathUtf8() : path_utf8;
    ProtectedCredentials creds;
    if (!ProtectedCredentialVault::LoadFromDpapiFile(path, creds, out_err)) {
        return false;
    }
    return SetCredentials(creds.username, creds.password, out_err);
}

void AppointmentScheduler::ClearCredentials() noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    vault_.ClearMemory();
}

bool AppointmentScheduler::HasCredentials() const noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    return vault_.HasMemoryCredentials();
}

bool AppointmentScheduler::ArmMission(bool armed, const std::string& confirmation_phrase, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!armed) {
        mission_armed_.store(false, std::memory_order_release);
        confirmed_phrase_.clear();
        client_->Disarm();
        status_.armed = false;
        sniper_status_.armed = false;
        return true;
    }
    if (confirmation_phrase != kConfirmationPhrase) {
        mission_armed_.store(false, std::memory_order_release);
        confirmed_phrase_.clear();
        if (out_err) *out_err = std::string("Frase requerida: ") + kConfirmationPhrase;
        return false;
    }
    if (!client_->ArmLiveMutations(true, confirmation_phrase, out_err)) {
        return false;
    }
    mission_armed_.store(true, std::memory_order_release);
    confirmed_phrase_ = confirmation_phrase;
    status_.armed = true;
    sniper_status_.armed = true;
    return true;
}

void AppointmentScheduler::Disarm() noexcept {
    std::string ignored;
    ArmMission(false, "", &ignored);
}

bool AppointmentScheduler::IsArmed() const noexcept {
    return mission_armed_.load(std::memory_order_acquire);
}

void AppointmentScheduler::AppendLogLocked(const std::string& msg) {
    status_.last_message = msg;
    status_.recent_logs.push_back("[" + NowUtcStamp() + "] " + msg);
    if (status_.recent_logs.size() > 200) {
        status_.recent_logs.erase(status_.recent_logs.begin());
    }
}

void AppointmentScheduler::NotifyStatusLocked() {
    status_.state_label = StateToLabel(status_.state);
    status_.server_clock_offset_ms = client_->ServerClockOffsetMs();
    status_.authenticated = client_->IsAuthenticated();
    status_.armed = mission_armed_.load(std::memory_order_acquire);
    if (status_cb_) {
        const auto snapshot = status_;
        const auto cb = status_cb_;
        // Invoke callback with snapshot (avoid recursion on mtx_ by only calling user-safe observers)
        cb(snapshot);
    }
}

void AppointmentScheduler::NotifySniperLocked() {
    sniper_status_.armed = mission_armed_.load(std::memory_order_acquire);
    if (sniper_cb_) {
        sniper_cb_(sniper_status_);
    }
}

bool AppointmentScheduler::PerformLoginWithOptionalRearm(bool mission_armed, std::string* out_err) {
    ProtectedCredentials creds;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!vault_.RetrieveFromMemory(creds, out_err)) {
            return false;
        }
    }
    if (!client_->Login(creds.username, creds.password, out_err)) {
        return false;
    }
    // Preserve user-confirmed armed state safely across autonomous pre-login / re-login
    if (mission_armed) {
        std::string phrase;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            phrase = confirmed_phrase_;
        }
        if (phrase == kConfirmationPhrase) {
            client_->ArmLiveMutations(true, phrase, nullptr);
        }
    }
    return true;
}

bool AppointmentScheduler::StartMission(const AppointmentMissionConfig& config,
                                        StatusCallback on_status,
                                        std::string* out_err)
{
    StopMission();
    if (!HasCredentials()) {
        if (out_err) *out_err = "Debe cargar credenciales en memoria o DPAPI antes de iniciar la mision.";
        return false;
    }
    if (!config.dry_run && !IsArmed()) {
        if (out_err) *out_err = "Modo real bloqueado: arme primero con la frase CONFIRMAR MATRICULA REAL.";
        return false;
    }
    if (config.plan.scenarios.empty()) {
        if (out_err) *out_err = "Configure al menos un escenario (Plan A) antes de iniciar.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_ = AppointmentStatus{};
        status_.dry_run = config.dry_run;
        status_.armed = IsArmed();
        status_.state = SchedulerState::WaitingForPreLogin;
        status_cb_ = std::move(on_status);
        AppendLogLocked("Mision autonoma armada (" + std::string(config.dry_run ? "DRY-RUN" : "REAL") + ").");
        NotifyStatusLocked();
    }

    mission_stop_.store(false, std::memory_order_release);
    mission_thread_ = std::thread(&AppointmentScheduler::RunMissionLoop, this, config);
    return true;
}

void AppointmentScheduler::StopMission() {
    mission_stop_.store(true, std::memory_order_release);
    if (mission_thread_.joinable()) {
        mission_thread_.join();
    }
}

void AppointmentScheduler::WaitForMission() {
    if (mission_thread_.joinable()) {
        mission_thread_.join();
    }
}

AppointmentStatus AppointmentScheduler::CurrentStatus() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return status_;
}

void AppointmentScheduler::RunMissionLoop(AppointmentMissionConfig cfg) {
    // Prevent Windows sleep during autonomous appointment window
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    struct SleepGuard {
        ~SleepGuard() { SetThreadExecutionState(ES_CONTINUOUS); }
    } sleep_guard;

    client_->SetBaseUrl(cfg.base_url);
    const bool live_armed = (!cfg.dry_run) && IsArmed();

    auto sleep_interruptible = [&](int ms) {
        const int slice = 50;
        for (int waited = 0; waited < ms && !mission_stop_.load(std::memory_order_acquire); waited += slice) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(slice, ms - waited)));
        }
    };

    // 1. Sync clock initially
    int64_t offset_ms = 0;
    std::string err;
    client_->SyncServerClock(&offset_ms, &err);

    // 2. Wait until T - pre_login_lead_seconds
    while (!mission_stop_.load(std::memory_order_acquire)) {
        const int64_t now_srv = client_->EstimatedServerEpochSeconds();
        const int64_t rem = (cfg.cita_epoch_utc > 0) ? (cfg.cita_epoch_utc - now_srv) : 0;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            status_.seconds_to_cita = std::max<int64_t>(0, rem);
            NotifyStatusLocked();
        }
        if (rem <= cfg.pre_login_lead_seconds) break;
        sleep_interruptible(250);
    }
    if (mission_stop_.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lk(mtx_);
        status_.state = SchedulerState::Cancelled;
        AppendLogLocked("Mision cancelada por el usuario.");
        NotifyStatusLocked();
        return;
    }

    // 3. Pre-login at T - 2:00
    bool use_scraper_fallback = (cfg.transport_mode == TransportMode::ScrapingOnly);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_.state = SchedulerState::PreLoggingIn;
        AppendLogLocked("Iniciando pre-login T-" + std::to_string(cfg.pre_login_lead_seconds) + "s...");
        NotifyStatusLocked();
    }

    if (!use_scraper_fallback) {
        if (!PerformLoginWithOptionalRearm(live_armed, &err)) {
            if (cfg.transport_mode == TransportMode::ApiWithScrapingFallback) {
                use_scraper_fallback = true;
                std::lock_guard<std::mutex> lk(mtx_);
                status_.fallback_scraper_active = true;
                AppendLogLocked("Pre-login API fallo (" + err + "); activando fallback Playwright.");
                NotifyStatusLocked();
            } else {
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    status_.state = SchedulerState::Failed;
                    AppendLogLocked("Pre-login API fallido: " + err);
                    NotifyStatusLocked();
                }
                Disarm();
                return;
            }
        } else {
            std::vector<CourseOffer> offers;
            client_->FetchCoursesAndBindConvocatoria(cfg.plan.convocatoria_id, cfg.plan.periodo_id, offers, nullptr);
            std::lock_guard<std::mutex> lk(mtx_);
            status_.state = SchedulerState::PreLoggedInStandby;
            AppendLogLocked("Pre-login exitoso (" + std::to_string(offers.size()) + " cursos en convocatoria).");
            NotifyStatusLocked();
        }
    }

    // 4. Standby with keep-alive until zero-hour refresh window
    int64_t last_ka = client_->EstimatedServerEpochSeconds();
    while (!mission_stop_.load(std::memory_order_acquire)) {
        const int64_t now_srv = client_->EstimatedServerEpochSeconds();
        const int64_t rem = (cfg.cita_epoch_utc > 0) ? (cfg.cita_epoch_utc - now_srv) : 0;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            status_.seconds_to_cita = std::max<int64_t>(0, rem);
            NotifyStatusLocked();
        }
        if (rem <= cfg.re_login_before_seconds) break;
        if (!use_scraper_fallback && (now_srv - last_ka) >= std::max(5, cfg.keep_alive_interval_seconds)) {
            CitaInfo ci;
            client_->FetchCita(ci, nullptr);
            last_ka = now_srv;
        }
        sleep_interruptible(200);
    }
    if (mission_stop_.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lk(mtx_);
        status_.state = SchedulerState::Cancelled;
        AppendLogLocked("Mision cancelada durante standby.");
        NotifyStatusLocked();
        return;
    }

    // 5. Zero-hour refresh / clean re-login
    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_.state = SchedulerState::ZeroHourRefreshing;
        AppendLogLocked("Hora cero alcanzada: re-autenticando y consultando cupos en vivo...");
        NotifyStatusLocked();
    }

    if (!use_scraper_fallback) {
        if (!PerformLoginWithOptionalRearm(live_armed, &err)) {
            if (cfg.transport_mode == TransportMode::ApiWithScrapingFallback) {
                use_scraper_fallback = true;
                std::lock_guard<std::mutex> lk(mtx_);
                status_.fallback_scraper_active = true;
                AppendLogLocked("Re-login API fallo (" + err + "); cambiando a Playwright DOM.");
                NotifyStatusLocked();
            } else {
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    status_.state = SchedulerState::Failed;
                    AppendLogLocked("Re-login en hora cero fallo: " + err);
                    NotifyStatusLocked();
                }
                Disarm();
                return;
            }
        } else {
            std::vector<CourseOffer> offers;
            client_->FetchCoursesAndBindConvocatoria(cfg.plan.convocatoria_id, cfg.plan.periodo_id, offers, nullptr);
        }
    }

    // If scraper fallback is active, start Playwright worker and login
    if (use_scraper_fallback) {
        ProtectedCredentials creds;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            vault_.RetrieveFromMemory(creds, &err);
        }
        if (!scraper_->Start(true, &err) ||
            !scraper_->Login(cfg.base_url, creds.username, creds.password,
                             cfg.plan.convocatoria_id, cfg.plan.periodo_id, &err)) {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                status_.state = SchedulerState::Failed;
                AppendLogLocked("Fallback Playwright fallo: " + err);
                NotifyStatusLocked();
            }
            Disarm();
            return;
        }
    }

    // 6. Execute hierarchical Plan A -> Plan B -> Plan C -> Plan Z with mid-flight pivot
    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_.state = SchedulerState::ExecutingPlan;
        NotifyStatusLocked();
    }

    const auto all_codes = CollectPlanCourseCodes(cfg.plan);
    std::vector<EnrollmentResult> all_results;
    int enrolled_success = 0;

    for (int attempt = 0; attempt < 3 && !mission_stop_.load(std::memory_order_acquire); ++attempt) {
        std::vector<EnrolledScheduleItem> enrolled;
        if (!use_scraper_fallback) {
            client_->FetchEnrolledSchedule(enrolled, nullptr);
        }

        std::map<std::string, std::vector<CourseGroupLive>> live_map;
        for (const auto& code : all_codes) {
            std::vector<CourseGroupLive> groups;
            bool ok = false;
            if (!use_scraper_fallback) {
                ok = client_->FetchLiveGroups(code, groups, &err);
            } else {
                ok = scraper_->FetchLiveGroups(code, groups, &err);
            }
            if (ok) live_map[code] = std::move(groups);
        }

        const ScenarioEvaluation eval = ScenarioEngine::SelectBestPlan(cfg.plan, live_map, enrolled);
        {
            std::lock_guard<std::mutex> lk(mtx_);
            status_.active_scenario = eval.scenario_name;
            AppendLogLocked("Escenario seleccionado (ronda " + std::to_string(attempt + 1) + "): " +
                            eval.scenario_name + " (" + std::to_string(eval.choices.size()) + " cursos).");
            NotifyStatusLocked();
        }

        if (!eval.feasible || eval.choices.empty()) {
            break;
        }

        // Optional replacement drops ONLY when ScenarioEngine authorized them (never under StrictlyConservative)
        for (const auto& drop_code : eval.drop_courses_required) {
            int drop_grp = 0;
            for (const auto& en : enrolled) {
                if (en.course_code == drop_code) { drop_grp = en.grupo; break; }
            }
            if (!use_scraper_fallback) {
                auto dr = client_->DropCourse(drop_code, drop_grp, cfg.dry_run);
                all_results.push_back(dr);
            }
        }

        bool need_pivot = false;
        std::string phrase_copy;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            phrase_copy = confirmed_phrase_;
        }

        for (const auto& choice : eval.choices) {
            if (mission_stop_.load(std::memory_order_acquire)) break;

            EnrollmentResult er{};
            if (cfg.dry_run) {
                er = SimulateDryRunEnroll(choice.group_data, enrolled);
            } else if (!use_scraper_fallback) {
                er = client_->EnrollCourse(choice.course_code, choice.grupo, false,
                                           choice.corequisite_code, choice.corequisite_group);
                // On network/HTTP error with fallback enabled, transparently retry via Playwright
                if (!er.ok && er.datic_code == "HTTP_ERROR" &&
                    cfg.transport_mode == TransportMode::ApiWithScrapingFallback) {
                    ProtectedCredentials creds;
                    {
                        std::lock_guard<std::mutex> lk(mtx_);
                        vault_.RetrieveFromMemory(creds, nullptr);
                    }
                    if (scraper_->Start(true, nullptr) &&
                        scraper_->Login(cfg.base_url, creds.username, creds.password,
                                        cfg.plan.convocatoria_id, cfg.plan.periodo_id, nullptr)) {
                        use_scraper_fallback = true;
                        std::lock_guard<std::mutex> lk(mtx_);
                        status_.fallback_scraper_active = true;
                        er = scraper_->EnrollGroup(choice.course_code, choice.grupo, false, live_armed, phrase_copy);
                    }
                }
            } else {
                er = scraper_->EnrollGroup(choice.course_code, choice.grupo, false, live_armed, phrase_copy);
            }

            all_results.push_back(er);
            {
                std::lock_guard<std::mutex> lk(mtx_);
                status_.results.push_back(er);
                AppendLogLocked((er.ok ? "OK: " : "RECHAZO (" + er.datic_code + "): ") +
                                er.course_code + " G" + std::to_string(er.grupo) + " - " + er.message);
                NotifyStatusLocked();
            }

            if (er.ok) {
                ++enrolled_success;
                EnrolledScheduleItem added{};
                added.course_code = choice.course_code;
                added.grupo = choice.grupo;
                added.sesiones = choice.group_data.sesiones;
                enrolled.push_back(std::move(added));
            } else if (er.datic_code == "SCupo" || er.datic_code == "CChoque") {
                // Seat vanished mid-flight! Re-query live availability and pivot to Plan B / C.
                need_pivot = true;
                break;
            }
        }

        if (!need_pivot) break;
    }

    // 7. Send Mi Informe -> Enviar receipt email if requested and at least one course enrolled
    bool informe_sent = false;
    if (cfg.send_informe_after_enroll && enrolled_success > 0 && !mission_stop_.load(std::memory_order_acquire)) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            status_.state = SchedulerState::SendingInforme;
            AppendLogLocked("Enviando comprobante oficial (Mi Informe -> Enviar)...");
            NotifyStatusLocked();
        }
        EnrollmentResult inf_res{};
        std::string phrase_copy;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            phrase_copy = confirmed_phrase_;
        }
        if (!use_scraper_fallback) {
            inf_res = client_->SendInformeEmail("", cfg.dry_run);
        } else {
            inf_res = scraper_->SendInforme(cfg.dry_run, live_armed, phrase_copy);
        }
        informe_sent = inf_res.ok;
        std::lock_guard<std::mutex> lk(mtx_);
        status_.results.push_back(inf_res);
        status_.informe_sent = informe_sent;
        AppendLogLocked(informe_sent ? "Mi Informe enviado correctamente." : ("Error enviando Mi Informe: " + inf_res.message));
        NotifyStatusLocked();
    }

    // 8. Finalize and automatically disarm for safety
    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_.enrolled_count = enrolled_success;
        status_.informe_sent = informe_sent;
        status_.state = (enrolled_success > 0 || cfg.dry_run) ? SchedulerState::Completed : SchedulerState::Failed;
        AppendLogLocked("Mision finalizada. Cursos procesados: " + std::to_string(enrolled_success) +
                        " | Informe: " + std::string(informe_sent ? "ENVIADO" : "NO"));
        NotifyStatusLocked();
    }
    if (scraper_->IsRunning()) {
        scraper_->Stop();
    }
    Disarm();
}

// ============================================================================
// Background Seat Sniper
// ============================================================================

bool AppointmentScheduler::StartSniper(const SniperConfig& config, SniperCallback on_status, std::string* out_err) {
    StopSniper();
    if (!HasCredentials()) {
        if (out_err) *out_err = "Cargue credenciales antes de iniciar el vigilante de cupos.";
        return false;
    }
    if (config.targets.empty()) {
        if (out_err) *out_err = "Agregue al menos un curso objetivo para vigilar.";
        return false;
    }
    if (config.auto_enroll && !config.dry_run && !IsArmed()) {
        if (out_err) *out_err = "auto_enroll real requiere armar primero con CONFIRMAR MATRICULA REAL.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(mtx_);
        sniper_status_ = SniperStatus{};
        sniper_status_.running = true;
        sniper_status_.armed = IsArmed();
        sniper_status_.auto_enroll = config.auto_enroll;
        sniper_status_.dry_run = config.dry_run;
        sniper_status_.last_message = "Vigilante de cupos iniciado.";
        sniper_cb_ = std::move(on_status);
        NotifySniperLocked();
    }

    sniper_stop_.store(false, std::memory_order_release);
    sniper_thread_ = std::thread(&AppointmentScheduler::RunSniperLoop, this, config);
    return true;
}

void AppointmentScheduler::StopSniper() {
    sniper_stop_.store(true, std::memory_order_release);
    if (sniper_thread_.joinable()) {
        sniper_thread_.join();
    }
    std::lock_guard<std::mutex> lk(mtx_);
    sniper_status_.running = false;
}

SniperStatus AppointmentScheduler::CurrentSniperStatus() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return sniper_status_;
}

void AppointmentScheduler::RunSniperLoop(SniperConfig cfg) {
    client_->SetBaseUrl(cfg.base_url);
    const bool live_armed = cfg.auto_enroll && !cfg.dry_run && IsArmed();
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> jitter_dist(0, std::max(0, cfg.jitter_seconds));

    std::set<std::string> done_courses;
    std::string err;

    while (!sniper_stop_.load(std::memory_order_acquire) && done_courses.size() < cfg.targets.size()) {
        if (!client_->IsAuthenticated()) {
            if (!PerformLoginWithOptionalRearm(live_armed, &err)) {
                std::lock_guard<std::mutex> lk(mtx_);
                sniper_status_.last_message = "Re-login sniper fallo: " + err;
                NotifySniperLocked();
            } else {
                std::vector<CourseOffer> offers;
                client_->FetchCoursesAndBindConvocatoria(cfg.convocatoria_id, cfg.periodo_id, offers, nullptr);
            }
        }

        if (client_->IsAuthenticated()) {
            std::vector<EnrolledScheduleItem> enrolled;
            client_->FetchEnrolledSchedule(enrolled, nullptr);

            for (const auto& t : cfg.targets) {
                if (sniper_stop_.load(std::memory_order_acquire)) break;
                if (done_courses.contains(t.course_code)) continue;

                std::vector<CourseGroupLive> groups;
                if (!client_->FetchLiveGroups(t.course_code, groups, &err)) {
                    continue;
                }
                for (const auto& g : groups) {
                    if (g.cupo_disponible <= 0) continue;
                    if (!t.allowed_groups.empty() && !std::ranges::contains(t.allowed_groups, g.grupo)) continue;
                    if (!ScenarioEngine::GroupMatchesFilter(g, t.filter)) continue;
                    if (SessionVectorsOverlap(g.sesiones, [&] {
                            std::vector<ScheduleSession> all;
                            for (const auto& e : enrolled) all.insert(all.end(), e.sesiones.begin(), e.sesiones.end());
                            return all;
                        }())) {
                        continue;
                    }

                    {
                        std::lock_guard<std::mutex> lk(mtx_);
                        ++sniper_status_.seats_detected_count;
                        sniper_status_.last_message = "Cupo detectado: " + t.course_code + " G" +
                                                      std::to_string(g.grupo) + " (cupos=" + std::to_string(g.cupo_disponible) + ")";
                        NotifySniperLocked();
                    }

                    if (cfg.auto_enroll) {
                        EnrollmentResult er = cfg.dry_run
                                                  ? SimulateDryRunEnroll(g, enrolled)
                                                  : client_->EnrollCourse(t.course_code, g.grupo, false);
                        {
                            std::lock_guard<std::mutex> lk(mtx_);
                            sniper_status_.sniped_results.push_back(er);
                            if (er.ok) ++sniper_status_.enrolled_count;
                            NotifySniperLocked();
                        }
                        if (er.ok) {
                            done_courses.insert(t.course_code);
                            if (cfg.send_informe_on_snipe) {
                                auto inf = client_->SendInformeEmail("", cfg.dry_run);
                                std::lock_guard<std::mutex> lk(mtx_);
                                sniper_status_.informe_sent = inf.ok;
                                sniper_status_.sniped_results.push_back(inf);
                                NotifySniperLocked();
                            }
                            break;
                        }
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lk(mtx_);
            ++sniper_status_.poll_count;
            sniper_status_.last_poll_utc = NowUtcStamp();
            NotifySniperLocked();
        }

        if (done_courses.size() >= cfg.targets.size()) break;
        const int wait_s = std::max(1, cfg.poll_interval_seconds) + jitter_dist(rng);
        for (int i = 0; i < wait_s * 10 && !sniper_stop_.load(std::memory_order_acquire); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::lock_guard<std::mutex> lk(mtx_);
    sniper_status_.running = false;
    sniper_status_.last_message = "Vigilante de cupos finalizado.";
    NotifySniperLocked();
}

} // namespace academic::matricula_tec
