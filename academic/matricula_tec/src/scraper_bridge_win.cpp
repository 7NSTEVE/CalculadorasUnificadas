#include "academic/matricula_tec/scraper_bridge.hpp"
#include "academic/matricula_tec/protocol.hpp"

#include <windows.h>

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace academic::matricula_tec {

namespace {

std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string WideToUtf8(std::wstring_view w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

} // namespace

ScraperBridge::ScraperBridge() = default;

ScraperBridge::~ScraperBridge() {
    Stop();
}

void ScraperBridge::SetWorkerScriptPath(const std::string& path_utf8) {
    std::lock_guard<std::mutex> lk(mtx_);
    worker_script_utf8_ = path_utf8;
}

void ScraperBridge::SetPythonExePath(const std::string& path_utf8) {
    std::lock_guard<std::mutex> lk(mtx_);
    python_exe_utf8_ = path_utf8;
}

std::string ScraperBridge::DiscoverPythonExeUtf8() {
    const std::vector<std::string> candidates = {
        "D:/DevTools/Python311/python.exe",
        "D:/Lenguajes de Programación y Herramientas de Desarrollo/Python311/python.exe"
    };
    std::error_code ec;
    for (const auto& c : candidates) {
        if (fs::is_regular_file(fs::u8path(c), ec)) return c;
    }
    return "python.exe";
}

std::string ScraperBridge::DiscoverWorkerScriptUtf8() {
    std::vector<fs::path> candidates;
    wchar_t exe_buf[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe_buf, MAX_PATH) > 0) {
        const fs::path exe_dir = fs::path(exe_buf).parent_path();
        candidates.push_back(exe_dir / "tools" / "tec_matricula" / "matricula_scraper_worker.py");
        candidates.push_back(exe_dir / "matricula_scraper_worker.py");
        // Walk up to monorepo root
        fs::path cur = exe_dir;
        for (int i = 0; i < 6 && cur.has_parent_path(); ++i) {
            candidates.push_back(cur / "tools" / "tec_matricula" / "matricula_scraper_worker.py");
            cur = cur.parent_path();
        }
    }
    candidates.push_back(fs::u8path("D:/Proyectos/CalculadorasUnificadas/tools/tec_matricula/matricula_scraper_worker.py"));

    std::error_code ec;
    for (const auto& p : candidates) {
        if (fs::is_regular_file(p, ec)) {
            return WideToUtf8(p.wstring());
        }
    }
    return {};
}

bool ScraperBridge::Start(bool headless, std::string* out_err) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (running_) return true;

    const std::string py = python_exe_utf8_.empty() ? DiscoverPythonExeUtf8() : python_exe_utf8_;
    const std::string script = worker_script_utf8_.empty() ? DiscoverWorkerScriptUtf8() : worker_script_utf8_;
    if (script.empty()) {
        if (out_err) *out_err = "No se encontro tools/tec_matricula/matricula_scraper_worker.py.";
        return false;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdinRd = nullptr, hStdinWr = nullptr;
    HANDLE hStdoutRd = nullptr, hStdoutWr = nullptr;
    if (!CreatePipe(&hStdinRd, &hStdinWr, &sa, 0) || !CreatePipe(&hStdoutRd, &hStdoutWr, &sa, 0)) {
        if (hStdinRd) CloseHandle(hStdinRd);
        if (hStdinWr) CloseHandle(hStdinWr);
        if (hStdoutRd) CloseHandle(hStdoutRd);
        if (hStdoutWr) CloseHandle(hStdoutWr);
        if (out_err) *out_err = "CreatePipe fallo al iniciar worker Playwright.";
        return false;
    }
    SetHandleInformation(hStdinWr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hStdinRd;
    si.hStdOutput = hStdoutWr;
    si.hStdError = hStdoutWr;

    PROCESS_INFORMATION pi{};
    // NEVER pass credentials on command line; only Python exe + worker script
    std::wstring cmdline = L"\"" + Utf8ToWide(py) + L"\" -u \"" + Utf8ToWide(script) + L"\"";
    std::vector<wchar_t> cmd_mut(cmdline.begin(), cmdline.end());
    cmd_mut.push_back(L'\0');

    const BOOL ok = CreateProcessW(
        nullptr,
        cmd_mut.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    CloseHandle(hStdinRd);
    CloseHandle(hStdoutWr);

    if (!ok) {
        CloseHandle(hStdinWr);
        CloseHandle(hStdoutRd);
        if (out_err) *out_err = "CreateProcessW fallo al iniciar worker Playwright (win32=" + std::to_string(GetLastError()) + ").";
        return false;
    }

    CloseHandle(pi.hThread);
    proc_handle_ = pi.hProcess;
    stdin_write_ = hStdinWr;
    stdout_read_ = hStdoutRd;
    running_ = true;

    json reply;
    if (!SendCommand(json{ { "cmd", "configure" }, { "headless", headless } }, reply, out_err)) {
        Stop();
        return false;
    }
    return true;
}

void ScraperBridge::Stop() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (running_ && stdin_write_) {
        const std::string bye = "{\"cmd\":\"close\"}\n";
        DWORD written = 0;
        WriteFile(static_cast<HANDLE>(stdin_write_), bye.data(), static_cast<DWORD>(bye.size()), &written, nullptr);
    }
    if (stdin_write_) {
        CloseHandle(static_cast<HANDLE>(stdin_write_));
        stdin_write_ = nullptr;
    }
    if (stdout_read_) {
        CloseHandle(static_cast<HANDLE>(stdout_read_));
        stdout_read_ = nullptr;
    }
    if (proc_handle_) {
        WaitForSingleObject(static_cast<HANDLE>(proc_handle_), 2000);
        TerminateProcess(static_cast<HANDLE>(proc_handle_), 0);
        CloseHandle(static_cast<HANDLE>(proc_handle_));
        proc_handle_ = nullptr;
    }
    running_ = false;
}

bool ScraperBridge::IsRunning() const noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    return running_;
}

bool ScraperBridge::SendCommand(const json& cmd, json& out_reply, std::string* out_err) {
    if (!running_ || !stdin_write_ || !stdout_read_) {
        if (out_err) *out_err = "Worker Playwright no iniciado.";
        return false;
    }
    std::string line = cmd.dump() + "\n";
    DWORD written = 0;
    if (!WriteFile(static_cast<HANDLE>(stdin_write_), line.data(), static_cast<DWORD>(line.size()), &written, nullptr)) {
        if (out_err) *out_err = "Fallo escritura stdin del worker Playwright.";
        return false;
    }
    SecureZeroMemory(line.data(), line.size());

    // Read JSON lines until a non-log response arrives
    std::string acc;
    char ch = 0;
    DWORD read = 0;
    while (ReadFile(static_cast<HANDLE>(stdout_read_), &ch, 1, &read, nullptr) && read == 1) {
        if (ch == '\r') continue;
        if (ch == '\n') {
            if (acc.empty()) continue;
            try {
                json parsed = json::parse(acc);
                acc.clear();
                if (parsed.is_object() && parsed.value("event", "") == "log") {
                    continue;
                }
                out_reply = std::move(parsed);
                return true;
            } catch (...) {
                acc.clear();
            }
        } else {
            acc.push_back(ch);
        }
    }
    if (out_err) *out_err = "Worker Playwright cerro stdout inesperadamente.";
    return false;
}

bool ScraperBridge::Login(const std::string& base_url,
                          const std::string& username,
                          const std::string& password,
                          const std::string& convocatoria_id,
                          const std::string& periodo_id,
                          std::string* out_err)
{
    std::lock_guard<std::mutex> lk(mtx_);
    json reply;
    const bool ok = SendCommand(json{
        { "cmd", "login" },
        { "base_url", base_url },
        { "username", username },
        { "password", password },
        { "convocatoria_id", convocatoria_id },
        { "periodo_id", periodo_id }
    }, reply, out_err);
    if (!ok) return false;
    if (!reply.value("ok", false)) {
        if (out_err) *out_err = reply.value("error", "Login Playwright fallido.");
        return false;
    }
    return true;
}

bool ScraperBridge::FetchLiveGroups(const std::string& course_code,
                                    std::vector<CourseGroupLive>& out_groups,
                                    std::string* out_err)
{
    std::lock_guard<std::mutex> lk(mtx_);
    out_groups.clear();
    json reply;
    if (!SendCommand(json{ { "cmd", "fetch_groups" }, { "course_code", course_code } }, reply, out_err)) {
        return false;
    }
    if (!reply.value("ok", false)) {
        if (out_err) *out_err = reply.value("error", "fetch_groups Playwright fallido.");
        return false;
    }
    if (reply.contains("groups")) {
        out_groups = ParseHorariosPayload(reply["groups"], course_code);
    }
    return true;
}

EnrollmentResult ScraperBridge::EnrollGroup(const std::string& course_code,
                                            int grupo,
                                            bool dry_run,
                                            bool armed,
                                            const std::string& confirmation_phrase)
{
    std::lock_guard<std::mutex> lk(mtx_);
    EnrollmentResult r{};
    r.dry_run = dry_run;
    r.transport_used = "scraper";
    r.action = "enroll";
    r.course_code = course_code;
    r.grupo = grupo;

    json reply;
    std::string err;
    if (!SendCommand(json{
            { "cmd", "enroll_group" },
            { "course_code", course_code },
            { "grupo", grupo },
            { "dry_run", dry_run },
            { "armed", armed },
            { "confirmation_phrase", confirmation_phrase }
        }, reply, &err)) {
        r.ok = false;
        r.datic_code = "SCRAPER_ERROR";
        r.message = err;
        return r;
    }
    r.ok = reply.value("ok", false);
    r.datic_code = reply.value("datic_code", r.ok ? "OK" : "ERROR");
    r.message = reply.value("message", reply.value("error", ""));
    return r;
}

EnrollmentResult ScraperBridge::SendInforme(bool dry_run,
                                            bool armed,
                                            const std::string& confirmation_phrase)
{
    std::lock_guard<std::mutex> lk(mtx_);
    EnrollmentResult r{};
    r.dry_run = dry_run;
    r.transport_used = "scraper";
    r.action = "send_informe";

    json reply;
    std::string err;
    if (!SendCommand(json{
            { "cmd", "send_informe" },
            { "dry_run", dry_run },
            { "armed", armed },
            { "confirmation_phrase", confirmation_phrase }
        }, reply, &err)) {
        r.ok = false;
        r.datic_code = "SCRAPER_ERROR";
        r.message = err;
        return r;
    }
    r.ok = reply.value("ok", false);
    r.datic_code = reply.value("datic_code", r.ok ? "OK" : "ERROR");
    r.message = reply.value("message", reply.value("error", ""));
    return r;
}

} // namespace academic::matricula_tec
