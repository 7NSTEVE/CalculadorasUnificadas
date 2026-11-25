#include "tutor_sidecar_bridge.hpp"
#include "../core/lan_remote.hpp"
#include "../core/utf_conv.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace DocStudio::Tutor {

namespace fs = std::filesystem;
using nlohmann::json;

struct TutorSidecarBridge::Impl {
    mutable std::mutex mtx;
    SidecarEndpointInfo endpoint{};
#ifdef _WIN32
    HANDLE hJob = nullptr;
    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
#endif
};

TutorSidecarBridge::TutorSidecarBridge() : impl_(new Impl()) {}

TutorSidecarBridge::~TutorSidecarBridge() {
    Stop();
    delete impl_;
    impl_ = nullptr;
}

SidecarLaunchConfig TutorSidecarBridge::DiscoverDefaultConfig() {
    SidecarLaunchConfig cfg;
    const fs::path preferredPy(L"D:\\DevTools\\Python311\\python.exe");
    std::error_code ec;
    if (fs::is_regular_file(preferredPy, ec)) {
        cfg.pythonExePath = preferredPy.wstring();
    } else {
        cfg.pythonExePath = L"python.exe";
    }

    // Walk upwards from current exe directory and working directory to find tools/tutor_runtime/server.py
    std::vector<fs::path> roots;
#ifdef _WIN32
    wchar_t exeBuf[MAX_PATH * 2]{};
    const DWORD n = GetModuleFileNameW(nullptr, exeBuf, static_cast<DWORD>(std::size(exeBuf)));
    if (n > 0) {
        fs::path cur = fs::path(exeBuf).parent_path();
        for (int i = 0; i < 5 && !cur.empty(); ++i) {
            roots.push_back(cur);
            cur = cur.parent_path();
        }
    }
#endif
    roots.push_back(fs::current_path(ec));
    roots.push_back(fs::path(L"D:\\Proyectos\\CalculadorasUnificadas"));

    for (const auto& r : roots) {
        if (r.empty()) continue;
        const fs::path candidate = r / L"tools" / L"tutor_runtime" / L"server.py";
        if (fs::is_regular_file(candidate, ec)) {
            cfg.repoRootDir = r.wstring();
            cfg.serverScriptPath = candidate.wstring();
            const fs::path webDir = r / L"assets" / L"cuadernu" / L"tutor_web";
            if (fs::is_directory(webDir, ec)) {
                cfg.assetsWebDir = webDir.wstring();
            }
            break;
        }
    }
    return cfg;
}

bool TutorSidecarBridge::Start(
    const SidecarLaunchConfig& cfg,
    const AiProviderVault* optionalVault,
    std::string* outErr)
{
    Stop();
#ifdef _WIN32
    std::error_code ec;
    if (cfg.serverScriptPath.empty() || !fs::is_regular_file(cfg.serverScriptPath, ec)) {
        const std::string msg = "No se encontr\xC3\xB3 tools/tutor_runtime/server.py.";
        if (outErr) *outErr = msg;
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->endpoint.lastErrorUtf8 = msg;
        return false;
    }

    const std::string token = LanRemote::GenerateSessionTokenHex24();

    // Build RAM-only bootstrap payload (never exposed in argv)
    json boot;
    boot["session_token"] = token;
    boot["visual_model"] = cfg.visualModelUtf8;
    boot["live_audio_model"] = cfg.liveAudioModelUtf8;
    boot["bind_host"] = cfg.bindLan ? "0.0.0.0" : "127.0.0.1";
    boot["repo_root"] = WideToUtf8(cfg.repoRootDir);
    boot["assets_web_dir"] = WideToUtf8(cfg.assetsWebDir);
    if (optionalVault) {
        if (const auto* gp = optionalVault->FindProviderByKind(AiProviderKind::GoogleGemini); gp && !gp->apiKeyUtf8.empty()) {
            boot["vault_gemini_key"] = gp->apiKeyUtf8;
        }
        if (const auto* ol = optionalVault->FindProviderByKind(AiProviderKind::OllamaLocal); ol) {
            boot["ollama_endpoint"] = ol->endpointUrlUtf8;
            boot["ollama_model"] = ol->modelNameUtf8;
        }
    }
    const std::string bootPayload = boot.dump() + "\n";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdinRead = nullptr, hStdinWrite = nullptr;
    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0) ||
        !CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        if (hStdinRead) CloseHandle(hStdinRead);
        if (hStdinWrite) CloseHandle(hStdinWrite);
        if (hStdoutRead) CloseHandle(hStdoutRead);
        if (hStdoutWrite) CloseHandle(hStdoutWrite);
        if (outErr) *outErr = "CreatePipe fall\xC3\xB3 al iniciar el runtime del tutor.";
        return false;
    }
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    std::wstring cmdLine = L"\"" + cfg.pythonExePath + L"\" -u \"" + cfg.serverScriptPath + L"\" --bootstrap-stdin";
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi{};
    const wchar_t* workDir = cfg.repoRootDir.empty() ? nullptr : cfg.repoRootDir.c_str();
    const BOOL created = CreateProcessW(
        nullptr,
        cmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED,
        nullptr,
        workDir,
        &si,
        &pi);

    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    if (!created) {
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        if (hJob) CloseHandle(hJob);
        const std::string msg = "No se pudo lanzar Python Tutor Runtime (CreateProcessW fall\xC3\xB3).";
        if (outErr) *outErr = msg;
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->endpoint.lastErrorUtf8 = msg;
        return false;
    }

    if (hJob) {
        AssignProcessToJobObject(hJob, pi.hProcess);
    }
    ResumeThread(pi.hThread);

    // Deliver bootstrap JSON via stdin pipe and immediately close stdin write end
    DWORD written = 0;
    WriteFile(hStdinWrite, bootPayload.data(), static_cast<DWORD>(bootPayload.size()), &written, nullptr);
    CloseHandle(hStdinWrite);

    // Poll stdout pipe until first JSON ready line or timeout
    std::string lineBuf;
    int boundPort = 0;
    std::string readyErr;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(2000, cfg.startupTimeoutMs));
    while (std::chrono::steady_clock::now() < deadline) {
        DWORD avail = 0;
        if (PeekNamedPipe(hStdoutRead, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            char tmp[1024];
            DWORD got = 0;
            if (ReadFile(hStdoutRead, tmp, static_cast<DWORD>(std::min<DWORD>(avail, sizeof(tmp))), &got, nullptr) && got > 0) {
                lineBuf.append(tmp, got);
                std::size_t nl = 0;
                while ((nl = lineBuf.find('\n')) != std::string::npos) {
                    const std::string line = lineBuf.substr(0, nl);
                    lineBuf.erase(0, nl + 1);
                    try {
                        const auto j = json::parse(line);
                        if (j.is_object() && j.value("ready", false)) {
                            boundPort = j.value("port", 0);
                            break;
                        }
                        if (j.is_object() && j.contains("error")) {
                            readyErr = j.value("error", "Error de arranque del sidecar");
                            break;
                        }
                    } catch (...) {
                        // Ignore non-JSON banner lines
                    }
                }
                if (boundPort > 0 || !readyErr.empty()) break;
            }
        }
        DWORD exitCode = 0;
        if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            readyErr = "El proceso Python Tutor Runtime termin\xC3\xB3 prematuramente (exit=" + std::to_string(exitCode) + ").";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    CloseHandle(hStdoutRead);

    if (boundPort <= 0) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (hJob) CloseHandle(hJob);
        const std::string msg = readyErr.empty() ? "Timeout esperando arranque del Tutor Runtime." : readyErr;
        if (outErr) *outErr = msg;
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->endpoint.lastErrorUtf8 = msg;
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->hJob = hJob;
        impl_->hProcess = pi.hProcess;
        impl_->hThread = pi.hThread;
        impl_->endpoint.running = true;
        impl_->endpoint.port = boundPort;
        impl_->endpoint.sessionTokenHex = token;
        impl_->endpoint.baseHttpUrl = "http://127.0.0.1:" + std::to_string(boundPort);
        impl_->endpoint.wsUrl = "ws://127.0.0.1:" + std::to_string(boundPort) + "/ws/tutor?token=" + token;
        impl_->endpoint.lastErrorUtf8.clear();
    }
    return true;
#else
    (void)cfg; (void)optionalVault;
    if (outErr) *outErr = "TutorSidecarBridge Win32 solo est\xC3\xA1 habilitado en Windows.";
    return false;
#endif
}

void TutorSidecarBridge::Stop() {
#ifdef _WIN32
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->hJob) {
        CloseHandle(impl_->hJob); // Automatically terminates process tree via KILL_ON_JOB_CLOSE
        impl_->hJob = nullptr;
    }
    if (impl_->hProcess) {
        DWORD ec = 0;
        if (GetExitCodeProcess(impl_->hProcess, &ec) && ec == STILL_ACTIVE) {
            TerminateProcess(impl_->hProcess, 0);
            WaitForSingleObject(impl_->hProcess, 500);
        }
        CloseHandle(impl_->hProcess);
        impl_->hProcess = nullptr;
    }
    if (impl_->hThread) {
        CloseHandle(impl_->hThread);
        impl_->hThread = nullptr;
    }
    impl_->endpoint.running = false;
    impl_->endpoint.port = 0;
#endif
}

bool TutorSidecarBridge::IsRunning() const noexcept {
#ifdef _WIN32
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (!impl_->endpoint.running || !impl_->hProcess) return false;
    DWORD ec = 0;
    return GetExitCodeProcess(impl_->hProcess, &ec) && ec == STILL_ACTIVE;
#else
    return false;
#endif
}

SidecarEndpointInfo TutorSidecarBridge::Endpoint() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->endpoint;
}

} // namespace DocStudio::Tutor
