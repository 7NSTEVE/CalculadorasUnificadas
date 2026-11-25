#pragma once

#include "../core/ai_provider_vault.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace DocStudio::Tutor {

struct SidecarLaunchConfig {
    std::wstring pythonExePath;
    std::wstring serverScriptPath;
    std::wstring assetsWebDir;
    std::wstring repoRootDir;
    std::string visualModelUtf8 = "gemini-2.5-flash";
    std::string liveAudioModelUtf8 = "gemini-3.8-live";
    bool bindLan = false;
    int startupTimeoutMs = 15000;
};

struct SidecarEndpointInfo {
    bool running = false;
    int port = 0;
    std::string sessionTokenHex;
    std::string baseHttpUrl;
    std::string wsUrl;
    std::string lastErrorUtf8;
};

struct SidecarVaultSnapshot {
    std::string geminiApiKeyUtf8;
    std::string ollamaEndpointUtf8;
    std::string ollamaModelUtf8;

    static SidecarVaultSnapshot Capture(const AiProviderVault* optionalVault);
};

// Manages the local decoupled Python Tutor Runtime safely:
// - Win32 JobObject with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE (zero orphan processes).
// - Ephemeral 192-bit token & optional DPAPI-decrypted vault info delivered strictly
//   through an anonymous stdin pipe in RAM (never in CLI argv or plaintext files).
// - Loopback 127.0.0.1 binding by default.
class TutorSidecarBridge {
public:
    TutorSidecarBridge();
    ~TutorSidecarBridge();

    TutorSidecarBridge(const TutorSidecarBridge&) = delete;
    TutorSidecarBridge& operator=(const TutorSidecarBridge&) = delete;

    static SidecarLaunchConfig DiscoverDefaultConfig();

    bool Start(
        const SidecarLaunchConfig& cfg,
        const AiProviderVault* optionalVault = nullptr,
        std::string* outErr = nullptr);

    bool StartWithSnapshot(
        const SidecarLaunchConfig& cfg,
        const SidecarVaultSnapshot& vaultSnapshot,
        std::atomic<bool>* cancelFlag = nullptr,
        std::string* outErr = nullptr);

    void Stop();

    bool IsRunning() const noexcept;
    SidecarEndpointInfo Endpoint() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace DocStudio::Tutor
