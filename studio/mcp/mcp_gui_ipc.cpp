#include "mcp_gui_ipc.hpp"
#include "mcp_protocol.hpp"
#include "../core/utf_conv.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <sddl.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio::Mcp {

namespace {

std::string NormalizePipeName(const std::string& raw) {
    if (raw.rfind("\\\\.\\pipe\\", 0) == 0) return raw;
    return "\\\\.\\pipe\\" + raw;
}

std::string SanitizeCliToken(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c == '-' || c == '_') out.push_back(static_cast<char>(c));
    }
    return out;
}

#ifdef _WIN32
bool BuildCurrentUserOnlySecurityAttributes(
    SECURITY_ATTRIBUTES& outSa,
    PSECURITY_DESCRIPTOR& outSd)
{
    outSa = {};
    outSd = nullptr;
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return false;

    DWORD needed = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &needed);
    if (needed == 0) {
        CloseHandle(hToken);
        return false;
    }
    std::vector<std::uint8_t> buf(needed);
    if (!GetTokenInformation(hToken, TokenUser, buf.data(), needed, &needed)) {
        CloseHandle(hToken);
        return false;
    }
    CloseHandle(hToken);

    auto* user = reinterpret_cast<TOKEN_USER*>(buf.data());
    LPWSTR sidStr = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sidStr) || !sidStr) {
        return false;
    }
    const std::wstring sddl = L"D:(A;;GA;;;" + std::wstring(sidStr) + L")";
    LocalFree(sidStr);

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &outSd, nullptr)) {
        outSd = nullptr;
        return false;
    }
    outSa.nLength = sizeof(SECURITY_ATTRIBUTES);
    outSa.lpSecurityDescriptor = outSd;
    outSa.bInheritHandle = FALSE;
    return true;
}

bool IsProcessAliveWin32(DWORD pid) {
    if (pid == 0) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code = 0;
    const BOOL ok = GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return ok && code == STILL_ACTIVE;
}

bool WriteAllPipeOverlapped(
    HANDLE hPipe,
    const std::string& data,
    std::chrono::steady_clock::time_point deadline)
{
    HANDLE hEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEv) return false;
    std::size_t off = 0;
    bool ok = true;
    while (off < data.size()) {
        ResetEvent(hEv);
        OVERLAPPED ov{};
        ov.hEvent = hEv;
        const DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(data.size() - off, 65536u));
        DWORD written = 0;
        const BOOL res = WriteFile(hPipe, data.data() + off, chunk, &written, &ov);
        if (!res) {
            const DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) { ok = false; break; }
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) { CancelIoEx(hPipe, &ov); ok = false; break; }
            const auto remMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            const DWORD waitRc = WaitForSingleObject(hEv, static_cast<DWORD>((std::max<long long>)(1ll, remMs)));
            if (waitRc != WAIT_OBJECT_0 || !GetOverlappedResult(hPipe, &ov, &written, FALSE) || written == 0) {
                CancelIoEx(hPipe, &ov);
                DWORD cancelBytes = 0;
                GetOverlappedResult(hPipe, &ov, &cancelBytes, TRUE);
                ok = false;
                break;
            }
        }
        if (written == 0) { ok = false; break; }
        off += written;
    }
    CloseHandle(hEv);
    return ok;
}

bool ReadLinePipeOverlapped(
    HANDLE hPipe,
    std::string& outLine,
    std::chrono::steady_clock::time_point deadline,
    DWORD expectedPid = 0,
    std::size_t maxBytes = 64u * 1024u * 1024u)
{
    outLine.clear();
    HANDLE hEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEv) return false;
    char buf[4096];
    bool finished = false;
    while (outLine.size() < maxBytes) {
        ResetEvent(hEv);
        OVERLAPPED ov{};
        ov.hEvent = hEv;
        DWORD got = 0;
        const BOOL res = ReadFile(hPipe, buf, sizeof(buf), &got, &ov);
        if (!res) {
            const DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                bool completed = false;
                while (!completed) {
                    const auto now = std::chrono::steady_clock::now();
                    if (now >= deadline) {
                        CancelIoEx(hPipe, &ov);
                        DWORD cancelBytes = 0;
                        GetOverlappedResult(hPipe, &ov, &cancelBytes, TRUE);
                        break;
                    }
                    if (expectedPid != 0 && !IsProcessAliveWin32(expectedPid)) {
                        CancelIoEx(hPipe, &ov);
                        DWORD cancelBytes = 0;
                        GetOverlappedResult(hPipe, &ov, &cancelBytes, TRUE);
                        break;
                    }
                    const auto remMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
                    const DWORD slice = static_cast<DWORD>((std::min<long long>)(150ll, (std::max<long long>)(1ll, remMs)));
                    const DWORD waitRc = WaitForSingleObject(hEv, slice);
                    if (waitRc == WAIT_OBJECT_0) {
                        if (GetOverlappedResult(hPipe, &ov, &got, FALSE)) {
                            completed = true;
                        }
                        break;
                    }
                }
                if (!completed || got == 0) {
                    finished = !outLine.empty();
                    break;
                }
            } else {
                finished = !outLine.empty();
                break;
            }
        } else if (got == 0) {
            finished = !outLine.empty();
            break;
        }
        for (DWORD i = 0; i < got; ++i) {
            if (buf[i] == '\n') {
                CloseHandle(hEv);
                return true;
            }
            if (buf[i] != '\r') {
                outLine.push_back(buf[i]);
            }
        }
    }
    CloseHandle(hEv);
    return finished;
}
#endif

} // namespace

// ---------------- McpGuiPipeServer ----------------

McpGuiPipeServer::~McpGuiPipeServer() {
    Stop();
}

std::string McpGuiPipeServer::ReadAuthTokenFromStdinPipe() {
#ifdef _WIN32
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (!hIn || hIn == INVALID_HANDLE_VALUE) return {};
    const DWORD ft = GetFileType(hIn);
    if (ft != FILE_TYPE_PIPE && ft != FILE_TYPE_DISK) return {};
    std::string tok;
    char ch = 0;
    DWORD got = 0;
    while (tok.size() < 256 && ReadFile(hIn, &ch, 1, &got, nullptr) && got == 1) {
        if (ch == '\n') break;
        if (ch != '\r' && ch != ' ' && ch != '\t') tok.push_back(ch);
    }
    return tok;
#else
    return {};
#endif
}

bool McpGuiPipeServer::Start(
    const std::string& pipeNameShortOrFull,
    const std::string& authTokenHex,
    DispatchFn dispatcher,
    const std::wstring& registryProfileRootW,
    const std::string& modeLabel,
    std::string* outErr)
{
    Stop();
#ifdef _WIN32
    if (pipeNameShortOrFull.empty() || !dispatcher) {
        if (outErr) *outErr = "Nombre de pipe o dispatcher invalido.";
        return false;
    }
    fullPipeName_ = NormalizePipeName(pipeNameShortOrFull);
    authToken_ = authTokenHex.empty() ? GenerateTokenHex192() : authTokenHex;
    dispatcher_ = std::move(dispatcher);
    stopRequested_.store(false, std::memory_order_release);

    if (!registryProfileRootW.empty()) {
        std::error_code ec;
        const fs::path regDir = fs::path(registryProfileRootW) / L"ipc_instances";
        fs::create_directories(regDir, ec);
        const DWORD pid = GetCurrentProcessId();
        const fs::path regPath = regDir / (L"pid_" + std::to_wstring(pid) + L".json");
        registryFileW_ = regPath.wstring();
        const json meta = {
            { "pid", static_cast<std::uint32_t>(pid) },
            { "pipe_name", fullPipeName_ },
            { "token", authToken_ },
            { "mode", modeLabel },
            { "profile_dir", WideToUtf8(registryProfileRootW) }
        };
        std::ofstream ofs(regPath, std::ios::binary | std::ios::trunc);
        if (ofs.is_open()) {
            const std::string dumped = meta.dump(2);
            ofs.write(dumped.data(), static_cast<std::streamsize>(dumped.size()));
            ofs.flush();
        }
    }

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_) {
        if (outErr) *outErr = "No se pudo crear evento de cierre para pipe GUI.";
        return false;
    }

    running_.store(true, std::memory_order_release);
    serverThread_ = std::thread([this]() {
        const std::wstring wPipe = Utf8ToWide(fullPipeName_);
        HANDLE hConnEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        while (!stopRequested_.load(std::memory_order_acquire)) {
            SECURITY_ATTRIBUTES sa{};
            PSECURITY_DESCRIPTOR pSd = nullptr;
            const bool hasDacl = BuildCurrentUserOnlySecurityAttributes(sa, pSd);

            HANDLE hPipe = CreateNamedPipeW(
                wPipe.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                PIPE_UNLIMITED_INSTANCES,
                1024 * 1024,
                1024 * 1024,
                5000,
                hasDacl ? &sa : nullptr);
            if (pSd) LocalFree(pSd);

            if (hPipe == INVALID_HANDLE_VALUE) {
                if (stopEvent_) WaitForSingleObject(stopEvent_, 40);
                else Sleep(40);
                continue;
            }
            {
                std::lock_guard<std::mutex> lk(handleMtx_);
                activePipeHandle_ = hPipe;
            }

            BOOL connected = FALSE;
            if (hConnEv) {
                ResetEvent(hConnEv);
                OVERLAPPED connOv{};
                connOv.hEvent = hConnEv;
                if (ConnectNamedPipe(hPipe, &connOv)) {
                    connected = TRUE;
                } else {
                    const DWORD err = GetLastError();
                    if (err == ERROR_PIPE_CONNECTED) {
                        connected = TRUE;
                    } else if (err == ERROR_IO_PENDING) {
                        HANDLE handles[2] = { hConnEv, stopEvent_ };
                        const DWORD wr = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                        if (wr == WAIT_OBJECT_0) {
                            DWORD dummy = 0;
                            connected = GetOverlappedResult(hPipe, &connOv, &dummy, FALSE);
                        } else {
                            CancelIoEx(hPipe, &connOv);
                            DWORD dummy = 0;
                            GetOverlappedResult(hPipe, &connOv, &dummy, TRUE);
                            connected = FALSE;
                        }
                    }
                }
            }

            if (stopRequested_.load(std::memory_order_acquire)) {
                DisconnectNamedPipe(hPipe);
                CloseHandle(hPipe);
                std::lock_guard<std::mutex> lk(handleMtx_);
                activePipeHandle_ = INVALID_HANDLE_VALUE;
                break;
            }

            if (connected) {
                std::string reqLine;
                const auto readDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                if (ReadLinePipeOverlapped(hPipe, reqLine, readDeadline) && !reqLine.empty()) {
                    json resp;
                    try {
                        const json envelope = json::parse(reqLine);
                        const std::string suppliedTok = (envelope.is_object() && envelope.contains("token") && envelope["token"].is_string())
                            ? envelope["token"].get<std::string>() : std::string();
                        if (!ConstantTimeEquals(suppliedTok, authToken_)) {
                            resp = { { "ok", false }, { "error", "Token IPC invalido" } };
                        } else {
                            const json payload = (envelope.contains("request") && envelope["request"].is_object())
                                ? envelope["request"] : json::object();
                            resp = dispatcher_(payload);
                        }
                    } catch (const std::exception& ex) {
                        resp = { { "ok", false }, { "error", std::string("IPC JSON error: ") + ex.what() } };
                    } catch (...) {
                        resp = { { "ok", false }, { "error", "IPC error interno" } };
                    }
                    std::string outWire = resp.dump(-1, ' ', false, json::error_handler_t::replace);
                    outWire.erase(std::remove(outWire.begin(), outWire.end(), '\r'), outWire.end());
                    outWire.erase(std::remove(outWire.begin(), outWire.end(), '\n'), outWire.end());
                    outWire.push_back('\n');
                    const auto writeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                    WriteAllPipeOverlapped(hPipe, outWire, writeDeadline);
                    FlushFileBuffers(hPipe);
                }
                DisconnectNamedPipe(hPipe);
            }

            CloseHandle(hPipe);
            {
                std::lock_guard<std::mutex> lk(handleMtx_);
                activePipeHandle_ = INVALID_HANDLE_VALUE;
            }
        }
        if (hConnEv) CloseHandle(hConnEv);
        running_.store(false, std::memory_order_release);
    });
    return true;
#else
    (void)pipeNameShortOrFull; (void)authTokenHex; (void)dispatcher; (void)registryProfileRootW; (void)modeLabel;
    if (outErr) *outErr = "Named Pipe IPC solo soportado en Windows.";
    return false;
#endif
}

void McpGuiPipeServer::Stop() {
#ifdef _WIN32
    stopRequested_.store(true, std::memory_order_release);
    if (stopEvent_) {
        SetEvent(stopEvent_);
    }
    {
        std::lock_guard<std::mutex> lk(handleMtx_);
        if (activePipeHandle_ != INVALID_HANDLE_VALUE) {
            CancelIoEx(activePipeHandle_, nullptr);
        }
    }
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
    running_.store(false, std::memory_order_release);
    if (!registryFileW_.empty()) {
        std::error_code ec;
        fs::remove(fs::path(registryFileW_), ec);
        registryFileW_.clear();
    }
#endif
}

// ---------------- McpGuiInstanceManager ----------------

McpGuiInstanceManager::McpGuiInstanceManager() {
#ifdef _WIN32
    hJob_ = CreateJobObjectW(nullptr, nullptr);
    if (hJob_) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
#endif
}

McpGuiInstanceManager::~McpGuiInstanceManager() {
    CloseAllBackgroundGuis();
#ifdef _WIN32
    if (hJob_) {
        CloseHandle(hJob_);
        hJob_ = nullptr;
    }
#endif
}

bool McpGuiInstanceManager::SendPipeJson(
    const std::string& fullPipeName,
    const std::string& authToken,
    const json& request,
    json& outResponse,
    int timeoutMs,
    std::string* outErr,
    std::uint32_t expectedPid)
{
#ifdef _WIN32
    outResponse = json::object();
    if (expectedPid != 0 && !IsProcessAliveWin32(static_cast<DWORD>(expectedPid))) {
        if (outErr) *outErr = "La instancia GUI de CuadernU (PID " + std::to_string(expectedPid) + ") ya no esta en ejecucion.";
        return false;
    }
    const std::wstring wPipe = Utf8ToWide(NormalizePipeName(fullPipeName));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((std::max)(200, timeoutMs));

    HANDLE hPipe = INVALID_HANDLE_VALUE;
    while (std::chrono::steady_clock::now() < deadline) {
        if (expectedPid != 0 && !IsProcessAliveWin32(static_cast<DWORD>(expectedPid))) {
            if (outErr) *outErr = "La instancia GUI de CuadernU (PID " + std::to_string(expectedPid) + ") termino inesperadamente.";
            return false;
        }
        hPipe = CreateFileW(
            wPipe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (hPipe != INVALID_HANDLE_VALUE) break;
        const DWORD err = GetLastError();
        if (err == ERROR_PIPE_BUSY) {
            WaitNamedPipeW(wPipe.c_str(), 100);
        } else {
            Sleep(35);
        }
    }
    if (hPipe == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo conectar al pipe GUI de CuadernU (" + fullPipeName + ").";
        return false;
    }

    const json envelope = {
        { "token", authToken },
        { "request", request }
    };
    std::string wire = envelope.dump(-1, ' ', false, json::error_handler_t::replace);
    wire.erase(std::remove(wire.begin(), wire.end(), '\r'), wire.end());
    wire.erase(std::remove(wire.begin(), wire.end(), '\n'), wire.end());
    wire.push_back('\n');

    if (!WriteAllPipeOverlapped(hPipe, wire, deadline)) {
        CloseHandle(hPipe);
        if (outErr) *outErr = "Error enviando peticion al pipe GUI.";
        return false;
    }

    std::string respLine;
    if (!ReadLinePipeOverlapped(hPipe, respLine, deadline, static_cast<DWORD>(expectedPid)) || respLine.empty()) {
        CloseHandle(hPipe);
        if (outErr) *outErr = "Sin respuesta (o timeout) desde la instancia GUI.";
        return false;
    }
    CloseHandle(hPipe);

    try {
        outResponse = json::parse(respLine);
        return true;
    } catch (...) {
        if (outErr) *outErr = "Respuesta JSON invalida desde la instancia GUI.";
        return false;
    }
#else
    (void)fullPipeName; (void)authToken; (void)request; (void)outResponse; (void)timeoutMs; (void)expectedPid;
    if (outErr) *outErr = "IPC GUI solo disponible en Windows.";
    return false;
#endif
}

bool McpGuiInstanceManager::SpawnBackgroundGui(
    const std::wstring& cuadernuExePathW,
    const std::wstring& profileDirW,
    int widthPx,
    int heightPx,
    const std::string& themeUtf8,
    const std::string& initialModeUtf8,
    bool noProfileWrite,
    LiveGuiInstanceInfo& outInfo,
    std::string* outErr)
{
#ifdef _WIN32
    if (cuadernuExePathW.empty() || !fs::exists(fs::path(cuadernuExePathW))) {
        if (outErr) *outErr = "Ejecutable CuadernU.exe no encontrado para lanzar GUI en segundo plano.";
        return false;
    }

    static std::atomic<std::uint64_t> sSeq{ 0 };
    const std::uint64_t seq = ++sSeq;
    const std::string pipeShort = "CuadernU_Bg_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(seq);
    const std::string fullPipe = NormalizePipeName(pipeShort);
    const std::string tokenHex = GenerateTokenHex192();

    // Anonymous stdin pipe for secure 192-bit token handoff (never on CLI argv)
    SECURITY_ATTRIBUTES saPipe{};
    saPipe.nLength = sizeof(saPipe);
    saPipe.bInheritHandle = TRUE;
    HANDLE hStdinRd = nullptr, hStdinWr = nullptr;
    if (!CreatePipe(&hStdinRd, &hStdinWr, &saPipe, 0)) {
        if (outErr) *outErr = "No se pudo crear pipe stdin para token de instancia GUI.";
        return false;
    }
    SetHandleInformation(hStdinWr, HANDLE_FLAG_INHERIT, 0);

    std::wostringstream cmd;
    cmd << L"\"" << cuadernuExePathW << L"\" --background-gui --mcp-pipe " << Utf8ToWide(pipeShort);
    if (noProfileWrite) cmd << L" --no-profile-write";
    if (!profileDirW.empty()) cmd << L" --profile-dir \"" << profileDirW << L"\"";
    if (widthPx >= 640 && heightPx >= 480) {
        cmd << L" --window-size " << widthPx << L"x" << heightPx;
    }
    const std::string safeTheme = SanitizeCliToken(themeUtf8);
    if (!safeTheme.empty()) {
        cmd << L" --theme " << Utf8ToWide(safeTheme);
    }
    const std::string safeMode = SanitizeCliToken(initialModeUtf8);
    if (!safeMode.empty()) {
        cmd << L" --mode " << Utf8ToWide(safeMode);
    }

    std::wstring cmdStr = cmd.str();
    std::vector<wchar_t> cmdBuf(cmdStr.begin(), cmdStr.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdinRd;
    si.hStdOutput = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_SHOWNOACTIVATE;

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        cuadernuExePathW.c_str(),
        cmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_SUSPENDED,
        nullptr,
        nullptr,
        &si,
        &pi);
    CloseHandle(hStdinRd);

    if (!ok) {
        CloseHandle(hStdinWr);
        if (outErr) *outErr = "CreateProcessW fallo al iniciar CuadernU.exe --background-gui.";
        return false;
    }

    if (hJob_) {
        AssignProcessToJobObject(hJob_, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    // Deliver 192-bit token over stdin pipe and close write end
    const std::string tokLine = tokenHex + "\n";
    DWORD wr = 0;
    WriteFile(hStdinWr, tokLine.data(), static_cast<DWORD>(tokLine.size()), &wr, nullptr);
    CloseHandle(hStdinWr);

    // Wait for GUI message loop & Named Pipe to answer ping
    json pingResp;
    std::string pingErr;
    bool ready = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!IsProcessAliveWin32(pi.dwProcessId)) {
            pingErr = "La instancia GUI oculta termino inesperadamente durante el arranque.";
            break;
        }
        if (SendPipeJson(fullPipe, tokenHex, { { "action", "ping" } }, pingResp, 400, &pingErr, static_cast<std::uint32_t>(pi.dwProcessId)) &&
            pingResp.value("ok", false)) {
            ready = true;
            break;
        }
        Sleep(60);
    }

    if (!ready) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        if (outErr) *outErr = pingErr.empty() ? "Tiempo de espera agotado esperando instancia GUI en segundo plano." : pingErr;
        return false;
    }

    ManagedChild child{};
    child.info.pid = static_cast<std::uint32_t>(pi.dwProcessId);
    child.info.instanceId = "bg_" + std::to_string(pi.dwProcessId);
    child.info.pipeName = fullPipe;
    child.info.mode = "stealth_background";
    child.info.titleUtf8 = pingResp.value("title", "CuadernU (Segundo Plano)");
    child.info.profileDirUtf8 = WideToUtf8(profileDirW);
    child.info.managedByThisServer = true;
    child.info.alive = true;
    child.authToken = tokenHex;
    child.hProcess = pi.hProcess;

    outInfo = child.info;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        managed_.push_back(std::move(child));
        activeInstanceId_ = outInfo.instanceId;
        activePipeName_ = outInfo.pipeName;
        activeAuthToken_ = tokenHex;
        activeInfo_ = outInfo;
    }
    return true;
#else
    (void)cuadernuExePathW; (void)profileDirW; (void)widthPx; (void)heightPx;
    (void)themeUtf8; (void)initialModeUtf8; (void)noProfileWrite; (void)outInfo;
    if (outErr) *outErr = "GUI oculta en segundo plano solo soportada en Windows.";
    return false;
#endif
}

bool McpGuiInstanceManager::CloseBackgroundGui(const std::string& instanceId, std::string* outErr) {
#ifdef _WIN32
    std::string targetId = instanceId;
    ManagedChild victim{};
    bool found = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (targetId.empty()) targetId = activeInstanceId_;
        for (auto it = managed_.begin(); it != managed_.end(); ++it) {
            if (targetId.empty() || it->info.instanceId == targetId) {
                victim = std::move(*it);
                managed_.erase(it);
                found = true;
                if (activeInstanceId_ == victim.info.instanceId) {
                    activeInstanceId_.clear();
                    activePipeName_.clear();
                    activeAuthToken_.clear();
                    activeInfo_ = {};
                }
                break;
            }
        }
    }
    if (!found) {
        if (outErr) *outErr = "No se encontro instancia GUI administrada para cerrar.";
        return false;
    }

    if (IsProcessAliveWin32(static_cast<DWORD>(victim.info.pid))) {
        json resp;
        SendPipeJson(victim.info.pipeName, victim.authToken, { { "action", "close_window" } }, resp, 2000, nullptr, victim.info.pid);
    }
    if (victim.hProcess) {
        if (WaitForSingleObject(victim.hProcess, 3500) != WAIT_OBJECT_0) {
            TerminateProcess(victim.hProcess, 0);
            WaitForSingleObject(victim.hProcess, 1500);
        }
        CloseHandle(victim.hProcess);
    }
    return true;
#else
    (void)instanceId;
    if (outErr) *outErr = "No soportado.";
    return false;
#endif
}

void McpGuiInstanceManager::CloseAllBackgroundGuis() {
#ifdef _WIN32
    std::vector<ManagedChild> toClose;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        toClose = std::move(managed_);
        managed_.clear();
        activeInstanceId_.clear();
        activePipeName_.clear();
        activeAuthToken_.clear();
        activeInfo_ = {};
    }
    for (auto& c : toClose) {
        if (IsProcessAliveWin32(static_cast<DWORD>(c.info.pid))) {
            json resp;
            SendPipeJson(c.info.pipeName, c.authToken, { { "action", "close_window" } }, resp, 1200, nullptr, c.info.pid);
        }
        if (c.hProcess) {
            if (WaitForSingleObject(c.hProcess, 2500) != WAIT_OBJECT_0) {
                TerminateProcess(c.hProcess, 0);
                WaitForSingleObject(c.hProcess, 1000);
            }
            CloseHandle(c.hProcess);
        }
    }
#endif
}

std::vector<LiveGuiInstanceInfo> McpGuiInstanceManager::ListInstances(const std::wstring& profileRootW) const {
    std::vector<LiveGuiInstanceInfo> out;
#ifdef _WIN32
    std::vector<std::uint32_t> seenPids;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& m : managed_) {
            LiveGuiInstanceInfo info = m.info;
            info.alive = IsProcessAliveWin32(info.pid);
            if (info.alive) {
                seenPids.push_back(info.pid);
                out.push_back(std::move(info));
            }
        }
    }
    if (!profileRootW.empty()) {
        const fs::path regDir = fs::path(profileRootW) / L"ipc_instances";
        std::error_code ec;
        if (fs::is_directory(regDir, ec)) {
            for (const auto& entry : fs::directory_iterator(regDir, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                if (entry.path().extension() != L".json") continue;
                std::ifstream ifs(entry.path(), std::ios::binary);
                if (!ifs.is_open()) continue;
                try {
                    const json j = json::parse(ifs);
                    const std::uint32_t pid = j.value("pid", 0u);
                    if (!IsProcessAliveWin32(pid)) {
                        ifs.close();
                        fs::remove(entry.path(), ec);
                        continue;
                    }
                    if (std::find(seenPids.begin(), seenPids.end(), pid) != seenPids.end()) continue;
                    LiveGuiInstanceInfo info{};
                    info.pid = pid;
                    info.instanceId = "live_" + std::to_string(pid);
                    info.pipeName = j.value("pipe_name", "");
                    info.mode = j.value("mode", "live_desktop");
                    info.profileDirUtf8 = j.value("profile_dir", "");
                    info.managedByThisServer = false;
                    info.alive = true;
                    out.push_back(std::move(info));
                } catch (...) {}
            }
        }
    }
#else
    (void)profileRootW;
#endif
    return out;
}

bool McpGuiInstanceManager::AttachLiveInstance(
    const std::wstring& profileRootW,
    std::uint32_t targetPid,
    LiveGuiInstanceInfo& outInfo,
    std::string* outErr)
{
#ifdef _WIN32
    // First check managed background instances
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& m : managed_) {
            if (targetPid != 0 && m.info.pid == targetPid && IsProcessAliveWin32(m.info.pid)) {
                activeInstanceId_ = m.info.instanceId;
                activePipeName_ = m.info.pipeName;
                activeAuthToken_ = m.authToken;
                activeInfo_ = m.info;
                outInfo = m.info;
                return true;
            }
        }
    }

    const fs::path regDir = fs::path(profileRootW) / L"ipc_instances";
    std::error_code ec;
    if (!fs::is_directory(regDir, ec)) {
        if (outErr) *outErr = "No hay instancias vivas de CuadernU registradas en este perfil.";
        return false;
    }

    for (const auto& entry : fs::directory_iterator(regDir, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != L".json") continue;
        std::ifstream ifs(entry.path(), std::ios::binary);
        if (!ifs.is_open()) continue;
        try {
            const json j = json::parse(ifs);
            const std::uint32_t pid = j.value("pid", 0u);
            if (!IsProcessAliveWin32(pid)) {
                ifs.close();
                fs::remove(entry.path(), ec);
                continue;
            }
            if (targetPid != 0 && pid != targetPid) continue;
            const std::string pipeName = j.value("pipe_name", "");
            const std::string token = j.value("token", "");
            if (pipeName.empty() || token.empty()) continue;

            json pingResp;
            std::string pingErr;
            if (!SendPipeJson(pipeName, token, { { "action", "ping" } }, pingResp, 2000, &pingErr, pid) ||
                !pingResp.value("ok", false)) {
                continue;
            }

            LiveGuiInstanceInfo info{};
            info.pid = pid;
            info.instanceId = "live_" + std::to_string(pid);
            info.pipeName = pipeName;
            info.mode = j.value("mode", "live_desktop");
            info.titleUtf8 = pingResp.value("title", "CuadernU");
            info.profileDirUtf8 = j.value("profile_dir", "");
            info.managedByThisServer = false;
            info.alive = true;

            {
                std::lock_guard<std::mutex> lk(mtx_);
                activeInstanceId_ = info.instanceId;
                activePipeName_ = pipeName;
                activeAuthToken_ = token;
                activeInfo_ = info;
            }
            outInfo = info;
            return true;
        } catch (...) {}
    }

    if (outErr) *outErr = "No se encontro ninguna instancia viva accesible de CuadernU para conectar.";
    return false;
#else
    (void)profileRootW; (void)targetPid; (void)outInfo;
    if (outErr) *outErr = "No soportado.";
    return false;
#endif
}

void McpGuiInstanceManager::DetachCurrent() {
    std::lock_guard<std::mutex> lk(mtx_);
    activeInstanceId_.clear();
    activePipeName_.clear();
    activeAuthToken_.clear();
    activeInfo_ = {};
}

bool McpGuiInstanceManager::HasActiveConnection() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (activePipeName_.empty() || activeInfo_.pid == 0) return false;
#ifdef _WIN32
    return IsProcessAliveWin32(activeInfo_.pid);
#else
    return false;
#endif
}

LiveGuiInstanceInfo McpGuiInstanceManager::ActiveInstanceInfo() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return activeInfo_;
}

bool McpGuiInstanceManager::SendRequest(
    const json& request,
    json& outResponse,
    const std::string& optionalInstanceId,
    int timeoutMs,
    std::string* outErr)
{
    std::string pipeName;
    std::string token;
    std::uint32_t targetPid = 0;
    std::string resolvedInstanceId;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!optionalInstanceId.empty()) {
            for (const auto& m : managed_) {
                if (m.info.instanceId == optionalInstanceId) {
                    pipeName = m.info.pipeName;
                    token = m.authToken;
                    targetPid = m.info.pid;
                    resolvedInstanceId = m.info.instanceId;
                    break;
                }
            }
            if (pipeName.empty() && activeInstanceId_ == optionalInstanceId) {
                pipeName = activePipeName_;
                token = activeAuthToken_;
                targetPid = activeInfo_.pid;
                resolvedInstanceId = activeInstanceId_;
            }
        } else {
            pipeName = activePipeName_;
            token = activeAuthToken_;
            targetPid = activeInfo_.pid;
            resolvedInstanceId = activeInstanceId_;
        }
    }
    if (pipeName.empty() || token.empty()) {
        if (outErr) *outErr = "No hay instancia GUI conectada. Usa cuadernu_gui(action='spawn_background') o cuadernu_gui(action='attach_live').";
        return false;
    }
#ifdef _WIN32
    if (targetPid != 0 && !IsProcessAliveWin32(static_cast<DWORD>(targetPid))) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (activeInstanceId_ == resolvedInstanceId) {
            activeInstanceId_.clear();
            activePipeName_.clear();
            activeAuthToken_.clear();
            activeInfo_ = {};
        }
        managed_.erase(
            std::remove_if(managed_.begin(), managed_.end(), [&](ManagedChild& m) {
                if (m.info.instanceId == resolvedInstanceId) {
                    if (m.hProcess) { CloseHandle(m.hProcess); m.hProcess = nullptr; }
                    return true;
                }
                return false;
            }),
            managed_.end());
        if (outErr) *outErr = "La instancia GUI conectada (PID " + std::to_string(targetPid) + ") se cerro o termino inesperadamente.";
        return false;
    }
#endif
    return SendPipeJson(pipeName, token, request, outResponse, timeoutMs, outErr, targetPid);
}

} // namespace DocStudio::Mcp
