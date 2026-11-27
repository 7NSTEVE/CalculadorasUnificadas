#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace DocStudio::Mcp {

constexpr unsigned int kWmAppMcpDispatch = 0x8000u + 106u; // WM_APP + 106

struct GuiDispatchTask {
    nlohmann::json request;
    nlohmann::json response;
    std::atomic<bool> done{ false };
#ifdef _WIN32
    HANDLE doneEvent = nullptr;
    ~GuiDispatchTask() {
        if (doneEvent) {
            CloseHandle(doneEvent);
            doneEvent = nullptr;
        }
    }
#endif
};

struct LiveGuiInstanceInfo {
    std::uint32_t pid = 0;
    std::string instanceId;
    std::string pipeName;
    std::string mode; // "stealth_background" | "live_desktop"
    std::string titleUtf8;
    std::string profileDirUtf8;
    bool managedByThisServer = false;
    bool alive = false;
};

// Server side embedded inside CuadernU.exe GUI process (both live desktop and --background-gui)
class McpGuiPipeServer {
public:
    using DispatchFn = std::function<nlohmann::json(const nlohmann::json& request)>;

    McpGuiPipeServer() = default;
    ~McpGuiPipeServer();

    McpGuiPipeServer(const McpGuiPipeServer&) = delete;
    McpGuiPipeServer& operator=(const McpGuiPipeServer&) = delete;

    // Reads a 192-bit token line from stdin pipe (for --background-gui children, never argv)
    static std::string ReadAuthTokenFromStdinPipe();

    bool Start(
        const std::string& pipeNameShortOrFull,
        const std::string& authTokenHex,
        DispatchFn dispatcher,
        const std::wstring& registryProfileRootW = L"",
        const std::string& modeLabel = "live_desktop",
        std::string* outErr = nullptr);

    void Stop();

    bool IsRunning() const noexcept { return running_.load(std::memory_order_acquire); }
    const std::string& PipeName() const noexcept { return fullPipeName_; }
    const std::string& AuthToken() const noexcept { return authToken_; }

private:
    std::atomic<bool> running_{ false };
    std::atomic<bool> stopRequested_{ false };
    std::string fullPipeName_;
    std::string authToken_;
    std::wstring registryFileW_;
    DispatchFn dispatcher_;
    std::thread serverThread_;
#ifdef _WIN32
    HANDLE activePipeHandle_ = INVALID_HANDLE_VALUE;
    HANDLE stopEvent_ = nullptr;
    std::mutex handleMtx_;
#endif
};

// Client & process-lifecycle manager inside CuadernU MCP server (Plane 2A + Plane 2B)
class McpGuiInstanceManager {
public:
    McpGuiInstanceManager();
    ~McpGuiInstanceManager();

    McpGuiInstanceManager(const McpGuiInstanceManager&) = delete;
    McpGuiInstanceManager& operator=(const McpGuiInstanceManager&) = delete;

    // Spawn a stealth DWMWA_CLOAK background GUI instance inside a Win32 JobObject (zero orphan processes)
    bool SpawnBackgroundGui(
        const std::wstring& cuadernuExePathW,
        const std::wstring& profileDirW,
        int widthPx,
        int heightPx,
        const std::string& themeUtf8,
        const std::string& initialModeUtf8,
        bool noProfileWrite,
        LiveGuiInstanceInfo& outInfo,
        std::string* outErr = nullptr);

    bool CloseBackgroundGui(const std::string& instanceId = "", std::string* outErr = nullptr);
    void CloseAllBackgroundGuis();

    // Enumerate managed background instances + discover live desktop instances in profileRoot/ipc_instances
    std::vector<LiveGuiInstanceInfo> ListInstances(const std::wstring& profileRootW) const;

    // Attach to an existing live desktop instance by PID or instanceId (or first discovered live instance)
    bool AttachLiveInstance(
        const std::wstring& profileRootW,
        std::uint32_t targetPid,
        LiveGuiInstanceInfo& outInfo,
        std::string* outErr = nullptr);

    void DetachCurrent();

    bool HasActiveConnection() const;
    LiveGuiInstanceInfo ActiveInstanceInfo() const;

    // Send authenticated JSON request to active (or specified) GUI instance over user-DACL Named Pipe
    bool SendRequest(
        const nlohmann::json& request,
        nlohmann::json& outResponse,
        const std::string& optionalInstanceId = "",
        int timeoutMs = 25000,
        std::string* outErr = nullptr);

    // Low-level authenticated pipe roundtrip helper
    static bool SendPipeJson(
        const std::string& fullPipeName,
        const std::string& authToken,
        const nlohmann::json& request,
        nlohmann::json& outResponse,
        int timeoutMs,
        std::string* outErr,
        std::uint32_t expectedPid = 0);

private:
    struct ManagedChild {
        LiveGuiInstanceInfo info;
        std::string authToken;
#ifdef _WIN32
        HANDLE hProcess = nullptr;
#endif
    };

    mutable std::mutex mtx_;
    std::vector<ManagedChild> managed_;
    std::string activeInstanceId_;
    std::string activePipeName_;
    std::string activeAuthToken_;
    LiveGuiInstanceInfo activeInfo_{};
#ifdef _WIN32
    HANDLE hJob_ = nullptr;
#endif
};

} // namespace DocStudio::Mcp
