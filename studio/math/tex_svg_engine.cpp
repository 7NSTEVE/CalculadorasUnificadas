#include "tex_svg_engine.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace DocStudio::Math {

namespace {

#ifdef _WIN32
std::wstring Utf8ToW(std::string_view s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

bool RunProcessWithTimeout(const std::wstring& cmdline, uint32_t timeout_ms) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mut(cmdline.begin(), cmdline.end());
    mut.push_back(L'\0');

    // Attach to a Win32 Job Object so child process trees terminate reliably on timeout
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    const BOOL ok = CreateProcessW(
        nullptr, mut.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        if (job) CloseHandle(job);
        return false;
    }

    if (job) AssignProcessToJobObject(job, pi.hProcess);
    const DWORD wait_rc = WaitForSingleObject(pi.hProcess, timeout_ms);
    DWORD exit_code = 1;
    if (wait_rc == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    } else {
        TerminateProcess(pi.hProcess, 1);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (job) CloseHandle(job);
    return wait_rc == WAIT_OBJECT_0 && exit_code == 0;
}
#endif

uint64_t Fnv1a64(std::string_view sv) noexcept {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : sv) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

std::string StripDelimiters(std::string_view raw) {
    size_t b = 0, e = raw.size();
    while (b < e && std::isspace(static_cast<unsigned char>(raw[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(raw[e - 1]))) --e;
    std::string s(raw.substr(b, e - b));
    if (s.size() >= 4 && s.starts_with("$$") && s.ends_with("$$")) s = s.substr(2, s.size() - 4);
    else if (s.size() >= 4 && s.starts_with("\\[") && s.ends_with("\\]")) s = s.substr(2, s.size() - 4);
    else if (s.size() >= 2 && s.starts_with("$") && s.ends_with("$")) s = s.substr(1, s.size() - 2);
    else if (s.size() >= 4 && s.starts_with("\\(") && s.ends_with("\\)")) s = s.substr(2, s.size() - 4);
    return s;
}

} // namespace

TexSvgEngine& TexSvgEngine::Instance() {
    static TexSvgEngine inst;
    return inst;
}

TexSvgEngine::TexSvgEngine() {
    LocateTexLive();
}

bool TexSvgEngine::IsMathContent(std::string_view text) noexcept {
    if (text.empty()) return false;
    if (text.contains('\\') || text.contains('_') || text.contains('^') || text.contains('$')) {
        return true;
    }
    // Check common UTF-8 mathematical Unicode symbols (Greek/calculus/operators)
    for (const std::string_view sym : { "∫", "∑", "∏", "√", "∞", "∇", "∂", "±", "×", "÷", "≤", "≥", "≠", "≈", "α", "β", "γ", "θ", "π", "ω", "Ω", "Δ" }) {
        if (text.contains(sym)) return true;
    }
    return false;
}

void TexSvgEngine::LocateTexLive() {
    std::lock_guard<std::mutex> lk(mtx_);
    latex_bin_.clear();
    dvisvgm_bin_.clear();
    has_texlive_ = false;

    const std::vector<std::string> latex_cands = {
        "E:/texlive/2024/bin/windows/latex.exe",
        "E:/texlive/bin/windows/latex.exe",
        "C:/texlive/2024/bin/windows/latex.exe"
    };
    const std::vector<std::string> dvi_cands = {
        "E:/texlive/2024/bin/windows/dvisvgm.exe",
        "E:/texlive/bin/windows/dvisvgm.exe",
        "C:/texlive/2024/bin/windows/dvisvgm.exe"
    };

    std::error_code ec;
    for (const auto& c : latex_cands) {
        if (fs::is_regular_file(c, ec)) { latex_bin_ = c; break; }
    }
    for (const auto& c : dvi_cands) {
        if (fs::is_regular_file(c, ec)) { dvisvgm_bin_ = c; break; }
    }
    has_texlive_ = (!latex_bin_.empty() && !dvisvgm_bin_.empty());
}

bool TexSvgEngine::IsTexLiveAvailable() const noexcept {
    std::lock_guard<std::mutex> lk(mtx_);
    return has_texlive_;
}

std::string TexSvgEngine::LatexBinPath() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return latex_bin_;
}

std::string TexSvgEngine::DvisvgmBinPath() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return dvisvgm_bin_;
}

std::string TexSvgEngine::ComputeKey(std::string_view latex, uint8_t r, uint8_t g, uint8_t b) {
    std::string raw(latex);
    raw += "|" + std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b);
    const uint64_t h1 = Fnv1a64(raw);
    const uint64_t h2 = Fnv1a64(std::string_view(raw.data(), raw.size() / 2u));
    char hex[40]{};
    std::snprintf(hex, sizeof(hex), "%016llx%016llx",
                  static_cast<unsigned long long>(h1),
                  static_cast<unsigned long long>(h2));
    return std::string(hex);
}

void TexSvgEngine::CachePutLocked(const std::string& key, const std::string& svg) {
    if (auto it = lru_map_.find(key); it != lru_map_.end()) {
        lru_list_.erase(it->second);
        lru_map_.erase(it);
    }
    lru_list_.emplace_front(key, svg);
    lru_map_[key] = lru_list_.begin();
    while (lru_list_.size() > kMaxCacheEntries) {
        const auto& last = lru_list_.back();
        lru_map_.erase(last.first);
        lru_list_.pop_back();
    }
}

TexSvgResult TexSvgEngine::CompileViaTexLive(std::string_view latex_code,
                                             uint8_t r, uint8_t g, uint8_t b,
                                             const std::string& key,
                                             uint32_t timeout_ms) const
{
    TexSvgResult res{};
#ifndef _WIN32
    (void)latex_code; (void)r; (void)g; (void)b; (void)key; (void)timeout_ms;
    res.error_utf8 = "TeX Live Win32 pipeline solo disponible en Windows.";
    return res;
#else
    std::string lbin, dbin;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!has_texlive_) {
            res.error_utf8 = "TeX Live (latex.exe / dvisvgm.exe) no disponible en el sistema.";
            return res;
        }
        lbin = latex_bin_;
        dbin = dvisvgm_bin_;
    }

    const std::string body = StripDelimiters(latex_code);
    if (body.empty()) {
        res.error_utf8 = "Expresion matematica vacia.";
        return res;
    }

    std::error_code ec;
    const fs::path tmp_dir = fs::temp_directory_path(ec) / "cuadernu_tex_svg";
    fs::create_directories(tmp_dir, ec);

    const fs::path tex_p = tmp_dir / (key + ".tex");
    const fs::path dvi_p = tmp_dir / (key + ".dvi");
    const fs::path svg_p = tmp_dir / (key + ".svg");
    const fs::path aux_p = tmp_dir / (key + ".aux");
    const fs::path log_p = tmp_dir / (key + ".log");

    {
        std::ofstream ofs(tex_p, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            res.error_utf8 = "No se pudo crear archivo temporal .tex.";
            return res;
        }
        ofs << "\\documentclass[preview,border=4pt,varwidth]{standalone}\n"
            << "\\usepackage{amsmath,amssymb,amsfonts}\n"
            << "\\usepackage{xcolor}\n"
            << "\\definecolor{mathCol}{RGB}{" << static_cast<int>(r) << "," << static_cast<int>(g) << "," << static_cast<int>(b) << "}\n"
            << "\\color{mathCol}\n"
            << "\\begin{document}\n\\[\n" << body << "\n\\]\n\\end{document}\n";
    }

    // Strict sandbox: -no-shell-escape -interaction=nonstopmode
    const std::wstring cmd_latex = L"\"" + Utf8ToW(lbin) +
        L"\" -no-shell-escape -interaction=nonstopmode \"-output-directory=" +
        tmp_dir.wstring() + L"\" \"" + tex_p.wstring() + L"\"";
    RunProcessWithTimeout(cmd_latex, timeout_ms);

    if (fs::is_regular_file(dvi_p, ec)) {
        const std::wstring cmd_svg = L"\"" + Utf8ToW(dbin) +
            L"\" --no-fonts --exact-bbox -o \"" + svg_p.wstring() + L"\" \"" + dvi_p.wstring() + L"\"";
        RunProcessWithTimeout(cmd_svg, timeout_ms);
        if (fs::is_regular_file(svg_p, ec)) {
            std::ifstream ifs(svg_p, std::ios::binary);
            std::ostringstream ss;
            ss << ifs.rdbuf();
            res.svg_utf8 = ss.str();
            res.ok = res.svg_utf8.contains("<svg");
        }
    }

    if (!res.ok && res.error_utf8.empty()) {
        res.error_utf8 = "Compilacion TeX Live -> dvisvgm no produjo SVG valido.";
    }

    // Always clean temporary files
    fs::remove(tex_p, ec);
    fs::remove(dvi_p, ec);
    fs::remove(svg_p, ec);
    fs::remove(aux_p, ec);
    fs::remove(log_p, ec);
    return res;
#endif
}

TexSvgResult TexSvgEngine::RenderSvgSync(std::string_view latex_code,
                                         uint8_t r, uint8_t g, uint8_t b,
                                         uint32_t timeout_ms)
{
    const std::string key = ComputeKey(latex_code, r, g, b);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (auto it = lru_map_.find(key); it != lru_map_.end()) {
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
            return TexSvgResult{ true, true, it->second->second, "" };
        }
    }
    TexSvgResult res = CompileViaTexLive(latex_code, r, g, b, key, timeout_ms);
    if (res.ok) {
        std::lock_guard<std::mutex> lk(mtx_);
        CachePutLocked(key, res.svg_utf8);
    }
    return res;
}

void TexSvgEngine::RenderSvgAsync(std::string latex_code,
                                  uint8_t r, uint8_t g, uint8_t b,
                                  std::function<void(TexSvgResult)> on_done,
                                  uint32_t timeout_ms)
{
    const std::string key = ComputeKey(latex_code, r, g, b);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (auto it = lru_map_.find(key); it != lru_map_.end()) {
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
            TexSvgResult cached{ true, true, it->second->second, "" };
            if (on_done) on_done(std::move(cached));
            return;
        }
        auto& waiters = in_flight_[key];
        const bool already_running = !waiters.empty();
        waiters.push_back(std::move(on_done));
        if (already_running) return;
    }

    std::thread([this, latex = std::move(latex_code), r, g, b, key, timeout_ms]() {
        TexSvgResult res = CompileViaTexLive(latex, r, g, b, key, timeout_ms);
        std::vector<std::function<void(TexSvgResult)>> cbs;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (res.ok) CachePutLocked(key, res.svg_utf8);
            if (auto it = in_flight_.find(key); it != in_flight_.end()) {
                cbs = std::move(it->second);
                in_flight_.erase(it);
            }
        }
        for (auto& cb : cbs) {
            if (cb) cb(res);
        }
    }).detach();
}

size_t TexSvgEngine::CacheSize() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return lru_list_.size();
}

void TexSvgEngine::ClearCache() {
    std::lock_guard<std::mutex> lk(mtx_);
    lru_list_.clear();
    lru_map_.clear();
}

} // namespace DocStudio::Math
