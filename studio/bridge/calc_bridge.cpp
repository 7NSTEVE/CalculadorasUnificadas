#include "calc_bridge.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace DocStudio::CalcBridge {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

constexpr const char* kSessionTagPrefix = "calc_session:";

std::uint64_t NowUtcMs()
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

std::uint64_t CurrentPidU64()
{
#ifdef _WIN32
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

std::string GetEnvVarUtf8(const char* name)
{
    if (!name) return {};
#ifdef _WIN32
    const DWORD need = GetEnvironmentVariableA(name, nullptr, 0);
    if (need == 0) return {};
    std::string out(need, '\0');
    const DWORD got = GetEnvironmentVariableA(name, out.data(), need);
    if (got == 0 || got >= need) return {};
    out.resize(got);
    return out;
#else
    const char* val = std::getenv(name);
    return val ? std::string(val) : std::string();
#endif
}

std::string ToLowerAscii(std::string s)
{
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::uint64_t Fnv1a64(const std::string& s)
{
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

std::string SanitizeIdentifier(const std::string& raw, const std::string& fallback = "default")
{
    if (raw.empty()) return fallback;
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(raw.size() + 16);
    for (char c : raw) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '-') {
            out.push_back(c);
        } else {
            out.push_back('_');
            out.push_back(kHex[(uc >> 4) & 0x0Fu]);
            out.push_back(kHex[uc & 0x0Fu]);
        }
    }
    if (out.empty()) return fallback;
    if (out.size() > 48) {
        std::ostringstream hashOss;
        hashOss << std::hex << std::setw(16) << std::setfill('0') << Fnv1a64(raw);
        out = out.substr(0, 44) + "__" + hashOss.str();
    }
    return out;
}

Cas::CasEvalOptions CasEvalOptionsForAngleMode(const std::string& angleModeUtf8)
{
    Cas::CasEvalOptions opts;
    const std::string low = ToLowerAscii(angleModeUtf8);
    // Check "grad"/"gra" BEFORE "rad" because "grad" contains "rad".
    if (low.find("grad") != std::string::npos || low.find("gra") != std::string::npos) {
        opts.trigAngleMode = Cas::CasAngleMode::Gra;
        opts.polarAngleMode = Cas::CasAngleMode::Gra;
    } else if (low.find("rad") != std::string::npos) {
        opts.trigAngleMode = Cas::CasAngleMode::Rad;
        opts.polarAngleMode = Cas::CasAngleMode::Rad;
    } else {
        opts.trigAngleMode = Cas::CasAngleMode::Deg;
        opts.polarAngleMode = Cas::CasAngleMode::Deg;
    }
    return opts;
}

std::string ResolveEffectiveAngleMode(const std::string& angleModeUtf8, const std::string& unitAssumptionsUtf8)
{
    const std::string low = ToLowerAscii(angleModeUtf8);
    if (!low.empty() && low != "deg") {
        return angleModeUtf8;
    }
    const std::string uaLow = ToLowerAscii(unitAssumptionsUtf8);
    if (uaLow.find("grad") != std::string::npos) return "GRAD";
    if (uaLow.find("rad") != std::string::npos) return "RAD";
    return angleModeUtf8.empty() ? std::string("DEG") : angleModeUtf8;
}

std::uint32_t Crc32Bytes(const std::uint8_t* data, std::size_t len)
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint32_t>(data[i]);
        for (int b = 0; b < 8; ++b) {
            const std::uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
            crc = (crc >> 1) ^ mask;
        }
    }
    return ~crc;
}

std::uint32_t Adler32Bytes(const std::uint8_t* data, std::size_t len)
{
    std::uint32_t s1 = 1u;
    std::uint32_t s2 = 0u;
    constexpr std::uint32_t kMod = 65521u;
    for (std::size_t i = 0; i < len; ++i) {
        s1 = (s1 + data[i]) % kMod;
        s2 = (s2 + s1) % kMod;
    }
    return (s2 << 16) | s1;
}

void AppendBe32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
}

void AppendLe16(std::vector<std::uint8_t>& out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
}

void AppendPngChunk(
    std::vector<std::uint8_t>& out,
    const char type[4],
    const std::vector<std::uint8_t>& payload)
{
    AppendBe32(out, static_cast<std::uint32_t>(payload.size()));
    const std::size_t typeOff = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), payload.begin(), payload.end());
    const std::uint32_t crc = Crc32Bytes(out.data() + typeOff, 4u + payload.size());
    AppendBe32(out, crc);
}

std::string Base64Encode(const std::vector<std::uint8_t>& data)
{
    static constexpr const char* kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 2 < data.size()) {
        const std::uint32_t t =
            (static_cast<std::uint32_t>(data[i]) << 16) |
            (static_cast<std::uint32_t>(data[i + 1]) << 8) |
            static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kAlphabet[(t >> 18) & 0x3F]);
        out.push_back(kAlphabet[(t >> 12) & 0x3F]);
        out.push_back(kAlphabet[(t >> 6) & 0x3F]);
        out.push_back(kAlphabet[t & 0x3F]);
        i += 3;
    }
    if (i < data.size()) {
        std::uint32_t t = static_cast<std::uint32_t>(data[i]) << 16;
        if (i + 1 < data.size()) t |= static_cast<std::uint32_t>(data[i + 1]) << 8;
        out.push_back(kAlphabet[(t >> 18) & 0x3F]);
        out.push_back(kAlphabet[(t >> 12) & 0x3F]);
        out.push_back((i + 1 < data.size()) ? kAlphabet[(t >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

std::vector<std::uint8_t> Base64Decode(const std::string& b64)
{
    auto valOf = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<std::uint8_t> out;
    out.reserve((b64.size() / 4) * 3);
    std::uint32_t buf = 0;
    int bits = -8;
    for (unsigned char c : b64) {
        if (c == '=') break;
        const int d = valOf(c);
        if (d < 0) continue;
        buf = (buf << 6) | static_cast<std::uint32_t>(d);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<std::uint8_t>((buf >> bits) & 0xFFu));
            bits -= 8;
        }
    }
    return out;
}

std::mutex& LiveSessionMutex()
{
    static std::mutex m;
    return m;
}

std::unordered_map<std::string, CalcSessionState>& LiveSessionRegistry()
{
    static std::unordered_map<std::string, CalcSessionState> reg;
    return reg;
}

std::string NumberToKeyTokens(const std::string& numTok, CasioModel model)
{
    std::string out;
    const char* expTok = (model == CasioModel::Fx570VnPlus || model == CasioModel::FxCg50)
        ? "[EXP]"
        : "[x10^x]";
    for (std::size_t i = 0; i < numTok.size(); ++i) {
        const char c = numTok[i];
        const bool afterExp = (i > 0 && (numTok[i - 1] == 'e' || numTok[i - 1] == 'E'));
        if (c == '+' && afterExp) {
            continue;
        }
        if (!out.empty()) out += " ";
        if (c == '-' && (i == 0 || afterExp)) out += "[(-)]";
        else if (c == 'e' || c == 'E') out += expTok;
        else out.push_back(c);
    }
    return out;
}

CasioKeyStep MakeStep(
    std::size_t idx,
    const std::string& modelId,
    const std::string& keyName,
    const std::string& caption,
    KeyLayerKind layer,
    const std::string& menuCtx,
    const std::string& note = {})
{
    CasioKeyStep s;
    s.stepIndex = idx;
    s.modelIdUtf8 = modelId;
    s.keyNameUtf8 = keyName;
    s.displayCaptionUtf8 = caption;
    s.layer = layer;
    s.badge = MakeShiftAlphaBadge(layer, modelId);
    s.menuContextUtf8 = menuCtx;
    s.noteUtf8 = note;
    return s;
}

std::string FormatCasioDisplayValue(double val, bool useComma)
{
    if (!std::isfinite(val)) return "Math ERROR";
    if (val == 0.0 || std::abs(val) < 1e-100) return "0";
    std::string s;
    if (std::abs(std::round(val)) >= 1.0 &&
        std::abs(val - std::round(val)) < 1e-10 &&
        std::abs(val) < 1e12) {
        s = std::to_string(static_cast<long long>(std::llround(val)));
    } else {
        std::ostringstream oss;
        oss << std::setprecision(10) << val;
        s = oss.str();
    }
    if (useComma) {
        for (char& c : s) {
            if (c == '.') c = ',';
        }
    }
    return s;
}

std::mutex& IpcSessionRmwMutex()
{
    static std::mutex m;
    return m;
}

class ScopedIpcSessionLock {
public:
    explicit ScopedIpcSessionLock(const fs::path& lockFilePath)
        : lockGuard_(IpcSessionRmwMutex()), lockPath_(lockFilePath)
    {
        std::error_code ec;
        fs::create_directories(lockPath_.parent_path(), ec);
        const auto start = std::chrono::steady_clock::now();
        const auto deadline = start + std::chrono::milliseconds(400);
        auto nextStaleCheck = start + std::chrono::milliseconds(100);
        while (true) {
            if (TryCreateLockFile()) {
                acquiredFile_ = true;
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            // Clear stale lock files older than 5 seconds
            if (now >= nextStaleCheck) {
                nextStaleCheck = now + std::chrono::milliseconds(100);
                if (fs::exists(lockPath_, ec)) {
                    const auto ftime = fs::last_write_time(lockPath_, ec);
                    if (!ec && (fs::file_time_type::clock::now() - ftime) > std::chrono::seconds(5)) {
                        fs::remove(lockPath_, ec);
                        continue;
                    }
                }
            }
            if (now >= deadline) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    ~ScopedIpcSessionLock()
    {
        if (acquiredFile_) {
            std::error_code ec;
            fs::remove(lockPath_, ec);
        }
    }

    bool Acquired() const noexcept
    {
        return acquiredFile_;
    }

    ScopedIpcSessionLock(const ScopedIpcSessionLock&) = delete;
    ScopedIpcSessionLock& operator=(const ScopedIpcSessionLock&) = delete;

private:
    bool TryCreateLockFile() const
    {
#ifdef _WIN32
        const std::wstring wpath = lockPath_.wstring();
        HANDLE h = CreateFileW(
            wpath.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            return false;
        }
        CloseHandle(h);
        return true;
#else
        const int fd = ::open(lockPath_.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
        if (fd < 0) {
            return false;
        }
        ::close(fd);
        return true;
#endif
    }

    std::unique_lock<std::mutex> lockGuard_;
    fs::path lockPath_;
    bool acquiredFile_ = false;
};

bool WriteFileAtomic(const fs::path& dst, const std::string& content, std::string* outErr)
{
    static std::atomic<std::uint64_t> s_tmpSeq{1};
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    const fs::path tmp = dst.parent_path() /
        (dst.filename().string() + ".tmp." + std::to_string(CurrentPidU64()) + "." +
         std::to_string(s_tmpSeq.fetch_add(1, std::memory_order_relaxed)));

    {
        std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            if (outErr) *outErr = "No se pudo abrir archivo temporal IPC: " + tmp.string();
            return false;
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.flush();
        if (!ofs.good()) {
            if (outErr) *outErr = "Fallo escritura IPC en: " + tmp.string();
            return false;
        }
    }

#ifdef _WIN32
    const std::wstring tmpW = tmp.wstring();
    const std::wstring dstW = dst.wstring();
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (MoveFileExW(tmpW.c_str(), dstW.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        const DWORD err = GetLastError();
        if (err != ERROR_SHARING_VIOLATION && err != ERROR_ACCESS_DENIED &&
            err != ERROR_LOCK_VIOLATION && err != ERROR_DELETE_PENDING) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
#else
    for (int attempt = 0; attempt < 10; ++attempt) {
        fs::rename(tmp, dst, ec);
        if (!ec) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
#endif
    fs::remove(tmp, ec);
    if (outErr) *outErr = "No se pudo renombrar archivo IPC a: " + dst.string();
    return false;
}

bool ReadFileString(const fs::path& p, std::string& out, std::string* outErr)
{
#ifdef _WIN32
    const std::wstring wpath = p.wstring();
    for (int attempt = 0; attempt < 40; ++attempt) {
        HANDLE h = CreateFileW(
            wpath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            const DWORD err = GetLastError();
            if (err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED ||
                err == ERROR_LOCK_VIOLATION || err == ERROR_DELETE_PENDING) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (outErr) *outErr = "No se pudo leer archivo IPC: " + p.string();
            return false;
        }
        LARGE_INTEGER fileSize{};
        if (!GetFileSizeEx(h, &fileSize) || fileSize.QuadPart < 0 ||
            fileSize.QuadPart > static_cast<LONGLONG>(64ULL * 1024ULL * 1024ULL)) {
            CloseHandle(h);
            if (outErr) *outErr = "Tamano invalido al leer archivo IPC: " + p.string();
            return false;
        }
        const std::size_t total = static_cast<std::size_t>(fileSize.QuadPart);
        out.resize(total);
        std::size_t got = 0;
        bool readOk = true;
        while (got < total) {
            DWORD chunk = 0;
            const DWORD toRead = static_cast<DWORD>((std::min<std::size_t>)(65536u, total - got));
            if (!ReadFile(h, out.data() + got, toRead, &chunk, nullptr) || chunk == 0) {
                readOk = false;
                break;
            }
            got += chunk;
        }
        CloseHandle(h);
        if (!readOk || got != total) {
            out.clear();
            if (outErr) *outErr = "Lectura incompleta de archivo IPC: " + p.string();
            return false;
        }
        return true;
    }
    if (outErr) *outErr = "Timeout leyendo archivo IPC: " + p.string();
    return false;
#else
    std::ifstream ifs(p, std::ios::binary | std::ios::ate);
    if (!ifs) {
        if (outErr) *outErr = "No se pudo leer archivo IPC: " + p.string();
        return false;
    }
    const std::streamsize sz = ifs.tellg();
    if (sz < 0) return false;
    ifs.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(sz));
    if (sz > 0 && !ifs.read(out.data(), sz)) {
        out.clear();
        return false;
    }
    return true;
#endif
}

json FrameToJson(const CalcDisplayFrame& f)
{
    json j;
    j["timestampMs"] = f.timestampMs;
    j["stepIndex"] = f.stepIndex;
    j["keyName"] = f.keyNameUtf8;
    j["lcdText"] = f.lcdTextUtf8;
    j["result"] = f.resultUtf8;
    j["indicatorsSummary"] = f.indicatorsSummaryUtf8;
    j["pixelWidth"] = f.pixelWidth;
    j["pixelHeight"] = f.pixelHeight;
    j["shift"] = f.shiftAlphaState.shiftActive;
    j["alpha"] = f.shiftAlphaState.alphaActive;
    j["sto"] = f.shiftAlphaState.stoActive;
    j["rcl"] = f.shiftAlphaState.rclActive;
    j["angleMode"] = f.shiftAlphaState.angleModeUtf8;
    j["displayFormat"] = f.shiftAlphaState.displayFormatUtf8;
    std::vector<std::uint8_t> png = f.lcdPngBytes;
    if (png.empty() && f.pixelWidth > 0 && f.pixelHeight > 0 &&
        f.rgbaPixels.size() >= static_cast<std::size_t>(f.pixelWidth) * f.pixelHeight * 4u) {
        png = EncodeRgbaToPng(f.pixelWidth, f.pixelHeight, f.rgbaPixels);
    }
    if (!png.empty()) {
        j["lcdPngBase64"] = Base64Encode(png);
    }
    return j;
}

CalcDisplayFrame FrameFromJson(const json& j)
{
    CalcDisplayFrame f;
    f.timestampMs = j.value("timestampMs", 0ULL);
    f.stepIndex = j.value("stepIndex", static_cast<std::size_t>(0));
    f.keyNameUtf8 = j.value("keyName", std::string());
    f.lcdTextUtf8 = j.value("lcdText", std::string());
    f.resultUtf8 = j.value("result", std::string());
    f.indicatorsSummaryUtf8 = j.value("indicatorsSummary", std::string());
    f.pixelWidth = j.value("pixelWidth", 0);
    f.pixelHeight = j.value("pixelHeight", 0);
    f.shiftAlphaState.shiftActive = j.value("shift", false);
    f.shiftAlphaState.alphaActive = j.value("alpha", false);
    f.shiftAlphaState.stoActive = j.value("sto", false);
    f.shiftAlphaState.rclActive = j.value("rcl", false);
    f.shiftAlphaState.angleModeUtf8 = j.value("angleMode", std::string("DEG"));
    f.shiftAlphaState.displayFormatUtf8 = j.value("displayFormat", std::string("Norm"));
    f.shiftAlphaState = NormalizeShiftAlphaState(f.shiftAlphaState);
    if (j.contains("lcdPngBase64") && j["lcdPngBase64"].is_string()) {
        f.lcdPngBytes = Base64Decode(j["lcdPngBase64"].get<std::string>());
    }
    return f;
}

} // namespace

CasioModel ParseCasioModel(const std::string& modelIdUtf8) noexcept
{
    const std::string low = ToLowerAscii(modelIdUtf8);
    if (low.find("cg50") != std::string::npos || low.find("cg-50") != std::string::npos ||
        low.find("prizm") != std::string::npos) {
        return CasioModel::FxCg50;
    }
    if (low.find("570vn") != std::string::npos || low.find("vn") != std::string::npos ||
        low.find("es") != std::string::npos || low.find("gris") != std::string::npos) {
        return CasioModel::Fx570VnPlus;
    }
    if (low.find("lax") != std::string::npos || low.find("la x") != std::string::npos ||
        low.find("blanca") != std::string::npos || low.find("991la") != std::string::npos ||
        low.find("570la") != std::string::npos) {
        return CasioModel::Fx570LaX;
    }
    return CasioModel::Fx991Cw;
}

std::string CanonicalModelIdUtf8(CasioModel model)
{
    switch (model) {
    case CasioModel::Fx570LaX:    return "fx-570LA X";
    case CasioModel::Fx570VnPlus: return "fx-570VN PLUS";
    case CasioModel::FxCg50:      return "fx-CG50";
    case CasioModel::Fx991Cw:     return "fx-991CW";
    }
    return "fx-991CW";
}

ShiftAlphaBadgeInfo MakeShiftAlphaBadge(KeyLayerKind layer, const std::string& modelIdUtf8)
{
    (void)modelIdUtf8;
    ShiftAlphaBadgeInfo b;
    b.layer = layer;
    switch (layer) {
    case KeyLayerKind::Normal:
        b.badgeTextUtf8 = "[N] NORMAL";
        b.colorTokenUtf8 = "neutral";
        b.rgbaColor = 0x1A73E8FFu;
        break;
    case KeyLayerKind::Shift:
        b.badgeTextUtf8 = "[S] SHIFT";
        b.colorTokenUtf8 = "amber_shift";
        b.rgbaColor = 0xD97706FFu;
        break;
    case KeyLayerKind::Alpha:
        b.badgeTextUtf8 = "[A] ALPHA";
        b.colorTokenUtf8 = "rose_alpha";
        b.rgbaColor = 0xE11D48FFu;
        break;
    case KeyLayerKind::ShiftAlpha:
        b.badgeTextUtf8 = "[S+A] SHIFT+ALPHA";
        b.colorTokenUtf8 = "violet_shift_alpha";
        b.rgbaColor = 0x7C3AEDFFu;
        break;
    }
    return b;
}

ShiftAlphaState NormalizeShiftAlphaState(ShiftAlphaState state, const std::string& modelIdUtf8)
{
    KeyLayerKind layer = KeyLayerKind::Normal;
    if (state.shiftActive && state.alphaActive) layer = KeyLayerKind::ShiftAlpha;
    else if (state.shiftActive) layer = KeyLayerKind::Shift;
    else if (state.alphaActive) layer = KeyLayerKind::Alpha;
    if (state.angleModeUtf8.empty()) state.angleModeUtf8 = "DEG";
    if (state.displayFormatUtf8.empty()) state.displayFormatUtf8 = "Norm";
    state.badge = MakeShiftAlphaBadge(layer, modelIdUtf8);
    return state;
}

std::string FormatShiftAlphaCalloutUtf8(const ShiftAlphaState& state)
{
    std::ostringstream oss;
    oss << state.badge.badgeTextUtf8
        << " | " << (state.angleModeUtf8.empty() ? "DEG" : state.angleModeUtf8)
        << " | " << (state.displayFormatUtf8.empty() ? "Norm" : state.displayFormatUtf8);
    if (state.stoActive) oss << " | [STO]";
    if (state.rclActive) oss << " | [RCL]";
    return oss.str();
}

std::vector<std::uint8_t> EncodeRgbaToPng(int width, int height, const std::vector<std::uint8_t>& rgbaPixels)
{
    if (width <= 0 || height <= 0) return {};
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
    if (rgbaPixels.size() < rowBytes * static_cast<std::size_t>(height)) return {};

    // Uncompressed scanlines (filter type 0 per row)
    std::vector<std::uint8_t> raw;
    raw.reserve((1u + rowBytes) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0u);
        const std::uint8_t* src = rgbaPixels.data() + static_cast<std::size_t>(y) * rowBytes;
        raw.insert(raw.end(), src, src + rowBytes);
    }

    // Zlib stream with uncompressed DEFLATE stored blocks (<= 65535 bytes each)
    std::vector<std::uint8_t> zlibData;
    zlibData.reserve(raw.size() + 32u);
    zlibData.push_back(0x78u); // CMF: deflate, 32K window
    zlibData.push_back(0x01u); // FLG: no preset dict, FCHECK valid (0x7801 % 31 == 0)

    std::size_t offset = 0;
    while (offset < raw.size()) {
        const std::size_t chunk = (std::min<std::size_t>)(65535u, raw.size() - offset);
        const bool isFinal = (offset + chunk >= raw.size());
        zlibData.push_back(isFinal ? 0x01u : 0x00u);
        const std::uint16_t len16 = static_cast<std::uint16_t>(chunk);
        const std::uint16_t nlen16 = static_cast<std::uint16_t>(~len16);
        AppendLe16(zlibData, len16);
        AppendLe16(zlibData, nlen16);
        zlibData.insert(zlibData.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                        raw.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
        offset += chunk;
    }
    AppendBe32(zlibData, Adler32Bytes(raw.data(), raw.size()));

    std::vector<std::uint8_t> png;
    png.reserve(zlibData.size() + 64u);
    static constexpr std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    png.insert(png.end(), kSig, kSig + 8);

    std::vector<std::uint8_t> ihdr;
    ihdr.reserve(13);
    AppendBe32(ihdr, static_cast<std::uint32_t>(width));
    AppendBe32(ihdr, static_cast<std::uint32_t>(height));
    ihdr.push_back(8u); // bit depth
    ihdr.push_back(6u); // color type RGBA
    ihdr.push_back(0u); // compression
    ihdr.push_back(0u); // filter
    ihdr.push_back(0u); // interlace
    AppendPngChunk(png, "IHDR", ihdr);
    AppendPngChunk(png, "IDAT", zlibData);
    AppendPngChunk(png, "IEND", {});
    return png;
}

std::string ExpressionToLatexUtf8(const std::string& exprUtf8, const std::string& resultUtf8) {
    std::string ltx = exprUtf8;
    auto repl = [&](const std::string& from, const std::string& to) {
        std::size_t pos = 0;
        while ((pos = ltx.find(from, pos)) != std::string::npos) {
            ltx.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    repl("*", "\\cdot ");
    repl("sqrt(", "\\sqrt{");
    repl("pi", "\\pi ");
    repl("\xE2\x88\xA0", "\\angle ");
    if (!resultUtf8.empty()) {
        return "$" + ltx + " = " + resultUtf8 + "$";
    }
    return "$" + ltx + "$";
}

bool BindCardToSession(AnnotationObject& obj, const std::string& calcSessionIdUtf8)
{
    if (obj.kind != ObjectKind::CalcSessionCard || calcSessionIdUtf8.empty()) return false;
    for (auto& tg : obj.tags) {
        if (tg.rfind(kSessionTagPrefix, 0) == 0) {
            tg = std::string(kSessionTagPrefix) + calcSessionIdUtf8;
            return true;
        }
    }
    obj.tags.push_back(std::string(kSessionTagPrefix) + calcSessionIdUtf8);
    return true;
}

std::string ExtractSessionIdFromCard(const AnnotationObject& obj)
{
    for (const auto& tg : obj.tags) {
        if (tg.rfind(kSessionTagPrefix, 0) == 0) {
            return tg.substr(std::strlen(kSessionTagPrefix));
        }
    }
    return {};
}

std::string KeySequenceToExpressionUtf8(const std::string& keySeqUtf8)
{
    if (keySeqUtf8.empty()) return {};

    // Strip leading navigation prefix if present, or trailing format/option/shift toggle after "->"
    std::string body = keySeqUtf8;
    std::size_t cutPos = std::string::npos;
    for (const char* marker : {"-> [FORMAT]", "-> [OPTN]", "-> [SHIFT]"}) {
        const std::size_t p = body.find(marker);
        if (p != std::string::npos && p < cutPos) {
            cutPos = p;
        }
    }
    if (cutPos != std::string::npos) {
        body = body.substr(0, cutPos);
    }

    // Tokenize by whitespace / commas
    std::vector<std::string> toks;
    std::string cur;
    for (char c : body) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
            if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) toks.push_back(cur);

    std::string expr;
    int openParens = 0;
    std::size_t idx = 0;

    // Skip leading mode entry prefix ([HOME] Calculate [OK], [MENU] [1] (Calcular), [MODE] [1] (COMP))
    auto isSingleBracketModeKey = [](const std::string& tok) -> bool {
        if (tok.size() != 3 || tok.front() != '[' || tok.back() != ']') return false;
        const unsigned char c = static_cast<unsigned char>(tok[1]);
        return std::isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    };
    auto isModeLabelParen = [](const std::string& tok) -> bool {
        if (tok.size() < 4 || tok.front() != '(' || tok.back() != ')') return false;
        if (tok == "(Ans)" || tok == "(ans)") return false;
        const std::string inner = tok.substr(1, tok.size() - 2);
        if (!std::isalpha(static_cast<unsigned char>(inner.front()))) return false;
        int alphaRun = 0;
        int maxAlphaRun = 0;
        for (std::size_t k = 0; k < inner.size(); ++k) {
            const unsigned char c = static_cast<unsigned char>(inner[k]);
            if (std::isdigit(c) || c == '+' || c == '*' || c == '^' || c == '=' ||
                c == '@' || c == '<' || c == '>' || c == '(' || c == ')') {
                return false;
            }
            if (c == '-' || c == '/') {
                // Mode labels like Run-Matrix or Ecuacion/Func have multi-letter words on both sides,
                // whereas math like (a-b) or (x/y) has single-letter operands.
                if (alphaRun < 2) return false;
                alphaRun = 0;
                continue;
            }
            if (std::isalpha(c) || c >= 0x80) {
                ++alphaRun;
                if (alphaRun > maxAlphaRun) maxAlphaRun = alphaRun;
            } else {
                alphaRun = 0;
            }
        }
        return maxAlphaRun >= 3 &&
               ((inner.find('-') == std::string::npos && inner.find('/') == std::string::npos) || alphaRun >= 2);
    };

    if (idx < toks.size()) {
        if (toks[idx] == "[HOME]") {
            std::size_t scan = idx + 1;
            std::size_t steps = 0;
            bool foundOk = false;
            while (scan < toks.size() && steps < 6) {
                if (toks[scan] == "[OK]") {
                    idx = scan + 1;
                    foundOk = true;
                    break;
                }
                const std::string& s = toks[scan];
                if (s != "->" && !std::isalpha(static_cast<unsigned char>(s.front()))) {
                    break;
                }
                ++scan;
                ++steps;
            }
            if (!foundOk) {
                ++idx;
                if (idx < toks.size() && isModeLabelParen(toks[idx])) {
                    ++idx;
                }
            }
        } else if (toks[idx] == "[MENU]" || toks[idx] == "[MODE]") {
            ++idx;
            if (idx < toks.size() && isSingleBracketModeKey(toks[idx])) {
                ++idx;
            }
            if (idx < toks.size() && isModeLabelParen(toks[idx])) {
                ++idx;
            }
        }
    }

    auto consumeOptionalOpenParen = [&]() {
        if (idx + 1 < toks.size() &&
            (toks[idx + 1] == "[(]" || toks[idx + 1] == "PAREN_ABRE" || toks[idx + 1] == "(")) {
            ++idx;
        }
    };

    bool shiftPending = false;
    for (; idx < toks.size(); ++idx) {
        const std::string& t = toks[idx];
        if (t == "[EXE]" || t == "[=]" || t == "EXE" || t == "IGUAL" || t == "=" ||
            t == "AC" || t == "[AC]" || t == "ON") {
            shiftPending = false;
            continue;
        }
        if (t == "[SHIFT]" || t == "SHIFT") {
            shiftPending = true;
            continue;
        }

        if (shiftPending) {
            shiftPending = false;
            if (t == "[sin]" || t == "SIN") {
                expr += "asin(";
                ++openParens;
                consumeOptionalOpenParen();
                continue;
            }
            if (t == "[cos]" || t == "COS") {
                expr += "acos(";
                ++openParens;
                consumeOptionalOpenParen();
                continue;
            }
            if (t == "[tan]" || t == "TAN") {
                expr += "atan(";
                ++openParens;
                consumeOptionalOpenParen();
                continue;
            }
            if (t == "[x^2]" || t == "CUADRADO") {
                expr += "sqrt(";
                ++openParens;
                consumeOptionalOpenParen();
                continue;
            }
            if (t == "[sqrt]" || t == "RAIZ") {
                expr += "^2";
                continue;
            }
            if (t == "[log]" || t == "LOG_B" || t == "LOG") {
                expr += "10^";
                continue;
            }
            if (t == "[ln]" || t == "LN") {
                expr += "exp(";
                ++openParens;
                consumeOptionalOpenParen();
                continue;
            }
            if (t == "[pi]" || t == "[EXP]" || t == "EXP" || t == "[x10^x]" || t == "x10^x" || t == "pi") {
                if (!expr.empty()) {
                    const char prev = expr.back();
                    if (std::isdigit(static_cast<unsigned char>(prev)) || prev == ')' || prev == 'i') {
                        expr.push_back('*');
                    }
                }
                expr += "pi";
                continue;
            }
            if (t == "[(-)]" || t == "[VARS]") {
                expr.push_back('@');
                if (idx + 1 < toks.size() && toks[idx + 1] == "(<)") {
                    ++idx;
                }
                continue;
            }
            if (t == "[0:i]" || t == "[0]" || t == "0" || t == "[i]" || t == "i") {
                expr.push_back('i');
                continue;
            }
        }

        if (t.size() == 1 && (std::isdigit(static_cast<unsigned char>(t[0])) || t[0] == '.')) {
            expr.push_back(t[0]);
        } else if (t == "PUNTO") {
            expr.push_back('.');
        } else if (t == "[+]" || t == "MAS" || t == "+") {
            expr.push_back('+');
        } else if (t == "[-]" || t == "MENOS" || t == "-") {
            expr.push_back('-');
        } else if (t == "[(-)]" || t == "NEGATIVO") {
            expr.push_back('-');
        } else if (t == "[x]" || t == "[*]" || t == "POR" || t == "*") {
            expr.push_back('*');
        } else if (t == "[/]" || t == "ENTRE" || t == "FRACCION" || t == "/") {
            expr.push_back('/');
        } else if (t == "[x^]" || t == "POTENCIA" || t == "POT" || t == "^") {
            expr.push_back('^');
        } else if (t == "[x^2]" || t == "CUADRADO") {
            expr += "^2";
        } else if (t == "[x10^x]" || t == "[EXP]" || t == "EXP") {
            // Casio x10^x / EXP binds tighter than * and / (part of scientific literal).
            std::size_t mantStart = expr.size();
            if (!expr.empty()) {
                if (expr.back() == ')') {
                    int depth = 0;
                    std::size_t p = expr.size();
                    while (p > 0) {
                        --p;
                        if (expr[p] == ')') ++depth;
                        else if (expr[p] == '(') {
                            --depth;
                            if (depth == 0) { mantStart = p; break; }
                        }
                    }
                    // If preceded by a function identifier (e.g., sin, cos, sqrt, log10), include it in the mantissa.
                    while (mantStart > 0 &&
                           (std::isalnum(static_cast<unsigned char>(expr[mantStart - 1])) ||
                            expr[mantStart - 1] == '_')) {
                        --mantStart;
                    }
                } else {
                    std::size_t p = expr.size();
                    while (p > 0 && (std::isdigit(static_cast<unsigned char>(expr[p - 1])) || expr[p - 1] == '.')) {
                        --p;
                    }
                    mantStart = p;
                }
            }
            std::size_t j = idx + 1;
            bool expNeg = false;
            if (j < toks.size() &&
                (toks[j] == "[(-)]" || toks[j] == "NEGATIVO" || toks[j] == "[-]" || toks[j] == "MENOS" || toks[j] == "-")) {
                expNeg = true;
                ++j;
            } else if (j < toks.size() && (toks[j] == "[+]" || toks[j] == "MAS" || toks[j] == "+")) {
                ++j;
            }
            std::string expDigits;
            while (j < toks.size() && toks[j].size() == 1 &&
                   std::isdigit(static_cast<unsigned char>(toks[j][0]))) {
                expDigits.push_back(toks[j][0]);
                ++j;
            }
            if (mantStart < expr.size() && !expDigits.empty()) {
                expr.insert(mantStart, "(");
                expr += "*10^(";
                if (expNeg) expr.push_back('-');
                expr += expDigits;
                expr += "))";
                idx = j - 1;
            } else {
                expr += "*10^";
            }
        } else if (t == "[(]" || t == "PAREN_ABRE" || t == "(") {
            expr.push_back('(');
            ++openParens;
        } else if (t == "[)]" || t == "PAREN_CIERRA" || t == ")") {
            expr.push_back(')');
            if (openParens > 0) --openParens;
        } else if (t == "[sqrt]" || t == "RAIZ") {
            expr += "sqrt(";
            ++openParens;
            consumeOptionalOpenParen();
        } else if (t == "[sin]" || t == "SIN") {
            expr += "sin(";
            ++openParens;
            consumeOptionalOpenParen();
        } else if (t == "[cos]" || t == "COS") {
            expr += "cos(";
            ++openParens;
            consumeOptionalOpenParen();
        } else if (t == "[tan]" || t == "TAN") {
            expr += "tan(";
            ++openParens;
            consumeOptionalOpenParen();
        } else if (t == "[ln]" || t == "LN") {
            expr += "ln(";
            ++openParens;
            consumeOptionalOpenParen();
        } else if (t == "[log]" || t == "LOG_B" || t == "LOG") {
            expr += "log10(";
            ++openParens;
            consumeOptionalOpenParen();
        } else if (t == "[exp]") {
            expr += "exp(";
            ++openParens;
            consumeOptionalOpenParen();
        } else if (t == "[abs]") {
            expr += "abs(";
            ++openParens;
            consumeOptionalOpenParen();
        } else if (t == "[SHIFT][pi]" || t == "[pi]" || t == "pi") {
            if (!expr.empty()) {
                const char prev = expr.back();
                if (std::isdigit(static_cast<unsigned char>(prev)) || prev == ')' || prev == 'i') {
                    expr.push_back('*');
                }
            }
            expr += "pi";
        } else if (t == "[i]" || t == "[ENG:i]" || t == "[0:i]" || t == "[SHIFT][0:i]" || t == "i") {
            expr += "i";
        } else if (t.find("\xE2\x88\xA0") != std::string::npos || t == "@" || t == "(<)") {
            expr.push_back('@');
        } else if (t == "ANS" || t == "[ANS]") {
            expr += "Ans";
        } else if (!t.empty() && t.front() != '[' && t.back() != ']') {
            bool isLiteralMath = true;
            bool hasValueChar = false;
            for (char ch : t) {
                const unsigned char uch = static_cast<unsigned char>(ch);
                if (std::isdigit(uch) || ch == '(' || ch == 'i' || ch == 'j') {
                    hasValueChar = true;
                } else if (ch == '.' || ch == '+' || ch == '-' || ch == '*' ||
                           ch == '/' || ch == '^' || ch == ')' || ch == '@') {
                } else {
                    isLiteralMath = false;
                    break;
                }
            }
            if (isLiteralMath && hasValueChar) {
                expr += t;
            }
        }
    }
    while (openParens > 0) {
        expr.push_back(')');
        --openParens;
    }
    return expr;
}

bool ReplaySessionKeySequence(CalcSessionState& session, const Cas::SymbolEnv& env)
{
    std::string expr = session.formulaSourceUtf8;
    if (expr.empty() && !session.inputSequenceUtf8.empty()) {
        expr = KeySequenceToExpressionUtf8(session.inputSequenceUtf8);
    }
    if (expr.empty()) return false;

    if (expr.find('[') != std::string::npos) {
        const auto mres = Cas::EvaluateMatrixExpression(expr);
        session.appModeUtf8 = "Matrix";
        session.resultUtf8 = mres.ok ? mres.formattedUtf8 : mres.errorUtf8;
        session.latexUtf8 = mres.ok ? mres.latexUtf8 : expr;
        session.verificationStatus = mres.ok ? VerificationStatus::VerifiedCasMatch : VerificationStatus::EvaluationError;
        session.updatedUtcMs = NowUtcMs();
        return mres.ok;
    }
    if (expr.find(';') != std::string::npos && expr.find('=') != std::string::npos) {
        const auto sres = Cas::SolveLinearSystemEquations(expr);
        session.appModeUtf8 = "Equation (Simultan)";
        session.resultUtf8 = sres.ok ? sres.summaryUtf8 : sres.errorUtf8;
        session.latexUtf8 = ExpressionToLatexUtf8(expr, session.resultUtf8);
        session.verificationStatus = sres.ok ? VerificationStatus::VerifiedCasMatch : VerificationStatus::EvaluationError;
        session.updatedUtcMs = NowUtcMs();
        return sres.ok;
    }

    const std::string effectiveAngle = ResolveEffectiveAngleMode(
        session.shiftAlphaState.angleModeUtf8, session.unitAssumptionsUtf8);
    const auto casOpts = CasEvalOptionsForAngleMode(effectiveAngle);
    const auto ev = Cas::EvaluateExpression(expr, env, casOpts);
    session.updatedUtcMs = NowUtcMs();
    if (!ev.ok) {
        session.resultUtf8 = "ERR: " + ev.errorUtf8;
        session.latexUtf8 = ExpressionToLatexUtf8(expr, session.resultUtf8);
        session.verificationStatus = (ev.errorUtf8.find("Dim") != std::string::npos ||
                                      ev.errorUtf8.find("dim") != std::string::npos)
            ? VerificationStatus::DimensionError
            : VerificationStatus::EvaluationError;
        return false;
    }
    session.resultUtf8 = ev.formattedResultUtf8;
    session.latexUtf8 = ExpressionToLatexUtf8(expr, session.resultUtf8);
    session.verificationStatus = VerificationStatus::VerifiedCasMatch;
    return true;
}

void RegisterLiveSessionState(const CalcSessionState& state)
{
    if (state.calcSessionIdUtf8.empty()) return;
    std::lock_guard<std::mutex> lock(LiveSessionMutex());
    LiveSessionRegistry()[state.calcSessionIdUtf8] = state;
}

bool LookupLiveSessionState(const std::string& calcSessionIdUtf8, CalcSessionState& outState)
{
    if (calcSessionIdUtf8.empty()) return false;
    std::lock_guard<std::mutex> lock(LiveSessionMutex());
    auto it = LiveSessionRegistry().find(calcSessionIdUtf8);
    if (it == LiveSessionRegistry().end()) return false;
    outState = it->second;
    return true;
}

void UnregisterLiveSessionState(const std::string& calcSessionIdUtf8)
{
    if (calcSessionIdUtf8.empty()) return;
    std::lock_guard<std::mutex> lock(LiveSessionMutex());
    LiveSessionRegistry().erase(calcSessionIdUtf8);
}

AnnotationObject CalcSessionCard::ToAnnotationObject(const Cas::SymbolEnv& env) const
{
    AnnotationObject obj = BuildCalcSessionCard(session, bounds, env);
    if (objectId != 0) obj.id = objectId;
    return obj;
}

CalcSessionCard CalcSessionCard::FromAnnotationObject(const AnnotationObject& obj)
{
    CalcSessionCard card;
    card.objectId = obj.id;
    card.bounds = obj.bounds;
    card.session.calcSessionIdUtf8 = ExtractSessionIdFromCard(obj);
    card.liveBound = !card.session.calcSessionIdUtf8.empty();
    card.session.modelIdUtf8 = obj.calcModelIdUtf8.empty() ? "fx-991CW" : obj.calcModelIdUtf8;
    card.session.inputSequenceUtf8 = obj.calcKeySeqUtf8;
    card.session.formulaSourceUtf8 = obj.formulaExprUtf8;
    card.session.resultUtf8 = obj.evaluatedResultUtf8;
    card.session.latexUtf8 = obj.calcLatexUtf8;
    if (!obj.pngBytes.empty()) {
        CalcDisplayFrame f;
        f.lcdPngBytes = obj.pngBytes;
        f.resultUtf8 = obj.evaluatedResultUtf8;
        card.session.displayFrames.push_back(std::move(f));
    }
    return card;
}

AnnotationObject BuildCalcSessionCard(
    const CalcStepRecord& rec,
    const DocRect& bounds,
    const Cas::SymbolEnv& env)
{
    AnnotationObject obj;
    obj.kind = ObjectKind::CalcSessionCard;
    obj.bounds = bounds;
    obj.strokeColor = DocColor::RGBA(15, 23, 42, 255);
    obj.fillColor = DocColor::RGBA(240, 253, 244, 255);
    obj.calcModelIdUtf8 = rec.modelIdUtf8.empty() ? "fx-991LAX" : rec.modelIdUtf8;
    obj.calcKeySeqUtf8 = rec.keySeqUtf8;
    obj.formulaVarNameUtf8 = rec.assignVarUtf8;
    obj.formulaExprUtf8 = !rec.expressionUtf8.empty() ? rec.expressionUtf8 : rec.formulaSourceUtf8;
    obj.pngBytes = rec.lcdPngBytes;
    if (obj.pngBytes.empty() && !rec.displayFrames.empty()) {
        const auto& last = rec.displayFrames.back();
        if (!last.lcdPngBytes.empty()) {
            obj.pngBytes = last.lcdPngBytes;
        } else if (last.pixelWidth > 0 && last.pixelHeight > 0 && !last.rgbaPixels.empty()) {
            obj.pngBytes = EncodeRgbaToPng(last.pixelWidth, last.pixelHeight, last.rgbaPixels);
        }
    }
    obj.captionUtf8 = "CASIO " + obj.calcModelIdUtf8;

    if (!rec.resultUtf8.empty()) {
        obj.evaluatedResultUtf8 = rec.resultUtf8;
    } else if (!obj.formulaExprUtf8.empty()) {
        if (obj.formulaExprUtf8.find('[') != std::string::npos) {
            const auto mres = Cas::EvaluateMatrixExpression(obj.formulaExprUtf8);
            obj.evaluatedResultUtf8 = mres.ok ? mres.formattedUtf8 : mres.errorUtf8;
        } else if (obj.formulaExprUtf8.find(';') != std::string::npos &&
                   obj.formulaExprUtf8.find('=') != std::string::npos) {
            const auto sres = Cas::SolveLinearSystemEquations(obj.formulaExprUtf8);
            obj.evaluatedResultUtf8 = sres.ok ? sres.summaryUtf8 : sres.errorUtf8;
        } else {
            const std::string effectiveAngle = ResolveEffectiveAngleMode(
                rec.shiftAlphaState.angleModeUtf8, rec.unitAssumptionsUtf8);
            const auto casOpts = CasEvalOptionsForAngleMode(effectiveAngle);
            const auto ev = Cas::EvaluateExpression(obj.formulaExprUtf8, env, casOpts);
            obj.evaluatedResultUtf8 = ev.ok ? ev.formattedResultUtf8 : ev.errorUtf8;
        }
    }
    obj.calcLatexUtf8 = !rec.latexUtf8.empty()
        ? rec.latexUtf8
        : ExpressionToLatexUtf8(obj.formulaExprUtf8, obj.evaluatedResultUtf8);
    obj.textUtf8 = obj.captionUtf8 + " | " + obj.calcKeySeqUtf8 + " -> " + obj.evaluatedResultUtf8;
    if (!rec.calcSessionIdUtf8.empty()) {
        BindCardToSession(obj, rec.calcSessionIdUtf8);
    }
    return obj;
}

AnnotationObject BuildCalcSessionCard(
    const CalcSessionState& state,
    const DocRect& bounds,
    const Cas::SymbolEnv& env)
{
    CalcStepRecord rec;
    rec.modelIdUtf8 = state.modelIdUtf8;
    rec.keySeqUtf8 = state.inputSequenceUtf8;
    rec.expressionUtf8 = state.formulaSourceUtf8;
    rec.latexUtf8 = state.latexUtf8;
    rec.resultUtf8 = state.resultUtf8;
    rec.calcSessionIdUtf8 = state.calcSessionIdUtf8;
    rec.formulaSourceUtf8 = state.formulaSourceUtf8;
    rec.unitAssumptionsUtf8 = state.unitAssumptionsUtf8;
    rec.shiftAlphaState = state.shiftAlphaState;
    rec.displayFrames = state.displayFrames;
    if (!state.displayFrames.empty()) {
        const auto& last = state.displayFrames.back();
        if (!last.lcdPngBytes.empty()) {
            rec.lcdPngBytes = last.lcdPngBytes;
        } else if (last.pixelWidth > 0 && last.pixelHeight > 0 && !last.rgbaPixels.empty()) {
            rec.lcdPngBytes = EncodeRgbaToPng(last.pixelWidth, last.pixelHeight, last.rgbaPixels);
        }
    }
    return BuildCalcSessionCard(rec, bounds, env);
}

CalcSessionCard CreateSessionCard(
    const CalcSessionState& state,
    const DocRect& bounds,
    const Cas::SymbolEnv& env)
{
    CalcSessionCard card;
    card.bounds = bounds;
    card.session = state;
    card.liveBound = !state.calcSessionIdUtf8.empty();
    if (!card.session.displayFrames.empty()) {
        card.activeFrameIndex = card.session.displayFrames.size() - 1u;
    }
    RefreshCalcSessionCard(card, env);
    return card;
}

bool RefreshCalcSessionCard(CalcSessionCard& card, const Cas::SymbolEnv& env)
{
    if (!card.session.calcSessionIdUtf8.empty()) {
        CalcSessionState live;
        if (LookupLiveSessionState(card.session.calcSessionIdUtf8, live)) {
            card.session = std::move(live);
            card.liveBound = true;
        }
    }
    if (card.session.formulaSourceUtf8.empty() && !card.session.inputSequenceUtf8.empty()) {
        card.session.formulaSourceUtf8 = KeySequenceToExpressionUtf8(card.session.inputSequenceUtf8);
    }
    for (auto& f : card.session.displayFrames) {
        if (f.lcdPngBytes.empty() && f.pixelWidth > 0 && f.pixelHeight > 0 &&
            f.rgbaPixels.size() >= static_cast<std::size_t>(f.pixelWidth) * f.pixelHeight * 4u) {
            f.lcdPngBytes = EncodeRgbaToPng(f.pixelWidth, f.pixelHeight, f.rgbaPixels);
        }
    }
    if (!card.session.displayFrames.empty() &&
        card.activeFrameIndex >= card.session.displayFrames.size()) {
        card.activeFrameIndex = card.session.displayFrames.size() - 1u;
    }
    return ReplaySessionKeySequence(card.session, env);
}

bool RefreshCalcSessionCard(AnnotationObject& obj, const Cas::SymbolEnv& env) {
    if (obj.kind != ObjectKind::CalcSessionCard) return false;

    std::string effectiveAngle = "DEG";
    const std::string sid = ExtractSessionIdFromCard(obj);
    if (!sid.empty()) {
        CalcSessionState live;
        if (LookupLiveSessionState(sid, live)) {
            if (!live.modelIdUtf8.empty()) obj.calcModelIdUtf8 = live.modelIdUtf8;
            if (!live.inputSequenceUtf8.empty()) obj.calcKeySeqUtf8 = live.inputSequenceUtf8;
            if (!live.formulaSourceUtf8.empty()) obj.formulaExprUtf8 = live.formulaSourceUtf8;
            if (!live.resultUtf8.empty()) obj.evaluatedResultUtf8 = live.resultUtf8;
            effectiveAngle = ResolveEffectiveAngleMode(
                live.shiftAlphaState.angleModeUtf8, live.unitAssumptionsUtf8);
            if (!live.displayFrames.empty()) {
                const auto& last = live.displayFrames.back();
                if (!last.lcdPngBytes.empty()) {
                    obj.pngBytes = last.lcdPngBytes;
                } else if (last.pixelWidth > 0 && last.pixelHeight > 0 && !last.rgbaPixels.empty()) {
                    obj.pngBytes = EncodeRgbaToPng(last.pixelWidth, last.pixelHeight, last.rgbaPixels);
                }
            }
        }
    }

    std::string exprToEval = obj.formulaExprUtf8;
    if (exprToEval.empty() && !obj.calcKeySeqUtf8.empty()) {
        exprToEval = KeySequenceToExpressionUtf8(obj.calcKeySeqUtf8);
    }

    bool ok = true;
    if (!exprToEval.empty()) {
        if (exprToEval.find('[') != std::string::npos) {
            const auto mres = Cas::EvaluateMatrixExpression(exprToEval);
            if (!mres.ok) {
                obj.evaluatedResultUtf8 = "ERR: " + mres.errorUtf8;
                ok = false;
            } else {
                obj.evaluatedResultUtf8 = mres.formattedUtf8;
                obj.calcLatexUtf8 = mres.latexUtf8;
            }
        } else if (exprToEval.find(';') != std::string::npos &&
                   exprToEval.find('=') != std::string::npos) {
            const auto sres = Cas::SolveLinearSystemEquations(exprToEval);
            if (!sres.ok) {
                obj.evaluatedResultUtf8 = "ERR: " + sres.errorUtf8;
                ok = false;
            } else {
                obj.evaluatedResultUtf8 = sres.summaryUtf8;
                obj.calcLatexUtf8 = ExpressionToLatexUtf8(exprToEval, obj.evaluatedResultUtf8);
            }
        } else {
            const auto casOpts = CasEvalOptionsForAngleMode(effectiveAngle);
            const auto ev = Cas::EvaluateExpression(exprToEval, env, casOpts);
            if (!ev.ok) {
                obj.evaluatedResultUtf8 = "ERR: " + ev.errorUtf8;
                ok = false;
            } else {
                obj.evaluatedResultUtf8 = ev.formattedResultUtf8;
                obj.calcLatexUtf8 = ExpressionToLatexUtf8(exprToEval, obj.evaluatedResultUtf8);
            }
        }
    } else {
        obj.calcLatexUtf8 = ExpressionToLatexUtf8(obj.formulaExprUtf8, obj.evaluatedResultUtf8);
    }

    if (obj.calcLatexUtf8.empty()) {
        obj.calcLatexUtf8 = ExpressionToLatexUtf8(exprToEval, obj.evaluatedResultUtf8);
    }
    obj.captionUtf8 = "CASIO " + (obj.calcModelIdUtf8.empty() ? "fx-991CW" : obj.calcModelIdUtf8);
    obj.textUtf8 = obj.captionUtf8 + " | " + obj.calcKeySeqUtf8 + " -> " + obj.evaluatedResultUtf8;
    return ok;
}

CasioKeystrokePlan TranslateExpressionToCasioKeys(
    const std::string& exprUtf8,
    const std::string& modelIdUtf8,
    const Cas::SymbolEnv& env)
{
    CasioKeystrokePlan plan{};
    plan.model = ParseCasioModel(modelIdUtf8);
    plan.modelIdUtf8 = modelIdUtf8.empty() ? CanonicalModelIdUtf8(plan.model) : modelIdUtf8;

    const bool isCw2 = (plan.model == CasioModel::Fx991Cw);
    const bool isCg50 = (plan.model == CasioModel::FxCg50);
    const bool isVnPlus = (plan.model == CasioModel::Fx570VnPlus);
    const std::string exeBtn = (isCw2 || isCg50) ? "[EXE]" : "[=]";

    if (exprUtf8.empty()) return plan;

    std::size_t stepIdx = 0;
    auto pushStep = [&](const std::string& keyName, const std::string& caption,
                        KeyLayerKind layer, const std::string& ctx, const std::string& note = {}) {
        plan.steps.push_back(MakeStep(stepIdx++, plan.modelIdUtf8, keyName, caption, layer, ctx, note));
    };

    // Case 1: Matrix expression ([[...]])
    if (exprUtf8.find('[') != std::string::npos) {
        const auto mres = Cas::EvaluateMatrixExpression(exprUtf8);
        plan.appModeUtf8 = "Matrix";
        if (isCw2) {
            plan.keySequenceUtf8 = "[HOME] [Right]x3 (Matrix) [OK] -> [TOOLS] Define MatA -> Filas/Cols [OK] -> Ingresar coeficientes " + exeBtn;
            pushStep("HOME", "HOME", KeyLayerKind::Normal, "Matrix");
            pushStep("OK", "Matrix", KeyLayerKind::Normal, "Matrix");
            pushStep("TOOLS", "Define MatA", KeyLayerKind::Normal, "Matrix");
            if (exprUtf8.find("det") != std::string::npos) {
                plan.keySequenceUtf8 += " -> [CATALOG] Matrix -> Determinant -> MatA [)] [EXE]";
                pushStep("CATALOG", "Determinant", KeyLayerKind::Normal, "Matrix");
            } else if (exprUtf8.find("inv") != std::string::npos) {
                plan.keySequenceUtf8 += " -> [CATALOG] Matrix -> MatA [x^-1] [EXE]";
                pushStep("CATALOG", "Inverse", KeyLayerKind::Normal, "Matrix");
            } else {
                plan.keySequenceUtf8 += " -> [CATALOG] Matrix -> MatA [x] MatB [EXE]";
                pushStep("CATALOG", "Matrix Op", KeyLayerKind::Normal, "Matrix");
            }
            pushStep("EXE", "=", KeyLayerKind::Normal, "Matrix");
        } else if (isVnPlus) {
            plan.keySequenceUtf8 = "[MODE] [6] (MATRIX) -> [1] (MatA) -> Dim -> Coeficientes [=] -> [SHIFT] [4] (MATRIX) [=]";
            pushStep("MODE", "MODE", KeyLayerKind::Normal, "Matrix");
            pushStep("6", "MATRIX", KeyLayerKind::Normal, "Matrix");
            pushStep("SHIFT", "SHIFT", KeyLayerKind::Shift, "Matrix");
            pushStep("4", "MATRIX", KeyLayerKind::Shift, "Matrix");
            pushStep("=", "=", KeyLayerKind::Normal, "Matrix");
        } else if (isCg50) {
            plan.keySequenceUtf8 = "[MENU] [1] (Run-Matrix) -> [F3] (>MAT/VCT) -> [F1] (Mat A) -> Dim [EXE] -> Coeficientes [EXE] -> [OPTN] [F2] (MAT) [EXE]";
            pushStep("MENU", "MENU", KeyLayerKind::Normal, "Matrix");
            pushStep("1", "Run-Matrix", KeyLayerKind::Normal, "Matrix");
            pushStep("F3", ">MAT/VCT", KeyLayerKind::Normal, "Matrix");
            pushStep("OPTN", "MAT", KeyLayerKind::Normal, "Matrix");
            pushStep("EXE", "EXE", KeyLayerKind::Normal, "Matrix");
        } else {
            plan.keySequenceUtf8 = "[MENU] [4] (Matriz) -> [1] (MatA) -> Filas/Cols -> Ingresar coeficientes [=] -> [OPTN] Matriz [=]";
            pushStep("MENU", "MENU", KeyLayerKind::Normal, "Matrix");
            pushStep("4", "Matriz", KeyLayerKind::Normal, "Matrix");
            pushStep("OPTN", "OPTN", KeyLayerKind::Normal, "Matrix");
            pushStep("=", "=", KeyLayerKind::Normal, "Matrix");
        }
        plan.evaluatedUtf8 = mres.ok ? mres.formattedUtf8 : mres.errorUtf8;
        plan.latexUtf8 = mres.ok ? mres.latexUtf8 : exprUtf8;
        plan.ok = mres.ok;
        return plan;
    }

    // Case 2: Simultaneous Linear System (contains ';' and '=')
    if (exprUtf8.find(';') != std::string::npos && exprUtf8.find('=') != std::string::npos) {
        const auto sres = Cas::SolveLinearSystemEquations(exprUtf8);
        plan.appModeUtf8 = "Equation (Simultan)";
        const std::size_t nVars = (std::max<std::size_t>)(2, sres.varNames.size());
        if (isCw2) {
            plan.keySequenceUtf8 = "[HOME] -> Equation [OK] -> Simultan Equation [OK] -> " +
                std::to_string(nVars) + " Unknowns [OK] -> Coeficientes " + exeBtn + " " + exeBtn;
            pushStep("HOME", "HOME", KeyLayerKind::Normal, "Equation");
            pushStep("OK", "Simultan Equation", KeyLayerKind::Normal, "Equation");
            pushStep(std::to_string(nVars), std::to_string(nVars) + " Unknowns", KeyLayerKind::Normal, "Equation");
            pushStep("EXE", "=", KeyLayerKind::Normal, "Equation");
        } else if (isVnPlus) {
            const char* subChoice = (nVars == 2) ? "[1] (anX+bnY=cn)" : "[2] (anX+bnY+cnZ=dn)";
            plan.keySequenceUtf8 = std::string("[MODE] [5] (EQN) -> ") + subChoice + " -> Coeficientes [=] [=]";
            pushStep("MODE", "MODE", KeyLayerKind::Normal, "Equation");
            pushStep("5", "EQN", KeyLayerKind::Normal, "Equation");
            pushStep(nVars == 2 ? "1" : "2", "Simul", KeyLayerKind::Normal, "Equation");
            pushStep("=", "=", KeyLayerKind::Normal, "Equation");
            if (nVars > 3) {
                plan.warningsUtf8.push_back("fx-570VN PLUS solo soporta hasta 3 incognitas en modo EQN.");
            }
        } else if (isCg50) {
            plan.keySequenceUtf8 = "[MENU] [A] (Equation) -> [F1] (Simultaneous) -> [F" +
                std::to_string((std::min<std::size_t>)(nVars - 1, 5)) + "] (" + std::to_string(nVars) +
                " Unknowns) -> Coeficientes [F1:SOLVE]";
            pushStep("MENU", "MENU", KeyLayerKind::Normal, "Equation");
            pushStep("ALPHA", "A:Equation", KeyLayerKind::Alpha, "Equation");
            pushStep("F1", "Simultaneous", KeyLayerKind::Normal, "Equation");
            pushStep("F1", "SOLVE", KeyLayerKind::Normal, "Equation");
        } else {
            plan.keySequenceUtf8 = "[MENU] [A] (Ecuaci\xC3\xB3n/Func) -> [1] (Simul Ecuaci\xC3\xB3n) -> [" +
                std::to_string(nVars) + "] -> Coeficientes [=] [=]";
            pushStep("MENU", "MENU", KeyLayerKind::Normal, "Equation");
            pushStep("A", "Ecuacion/Func", KeyLayerKind::Alpha, "Equation");
            pushStep("1", "Simul", KeyLayerKind::Normal, "Equation");
            pushStep("=", "=", KeyLayerKind::Normal, "Equation");
        }
        plan.evaluatedUtf8 = sres.ok ? sres.summaryUtf8 : sres.errorUtf8;
        plan.latexUtf8 = ExpressionToLatexUtf8(exprUtf8, plan.evaluatedUtf8);
        plan.ok = sres.ok;
        return plan;
    }

    // Case 3: Phasor / Complex or Engineering Scalar Expression
    std::string rhs = exprUtf8;
    if (const auto eq = rhs.find('='); eq != std::string::npos) {
        rhs = rhs.substr(eq + 1);
    }

    bool hasPipeTarget = false;
    double targetUnitScale = 1.0;
    double targetUnitOffset = 0.0;
    bool hasTargetUnitTransform = false;
    {
        std::size_t pipePos = std::string::npos;
        for (std::size_t p = rhs.size(); p > 0; --p) {
            const std::size_t idx = p - 1;
            if (rhs[idx] == '|') {
                const bool prevPipe = (idx > 0 && rhs[idx - 1] == '|');
                const bool nextPipe = (idx + 1 < rhs.size() && rhs[idx + 1] == '|');
                if (!prevPipe && !nextPipe) {
                    pipePos = idx;
                    break;
                }
            }
        }
        if (pipePos != std::string::npos) {
            hasPipeTarget = true;
            std::string targetUnit = rhs.substr(pipePos + 1);
            const auto firstNonWs = targetUnit.find_first_not_of(" \t\r\n");
            const auto lastNonWs = targetUnit.find_last_not_of(" \t\r\n");
            targetUnit = (firstNonWs == std::string::npos)
                ? std::string()
                : targetUnit.substr(firstNonWs, lastNonWs - firstNonWs + 1);
            rhs = rhs.substr(0, pipePos);
            plan.warningsUtf8.push_back("Unidades SI convertidas a potencia de 10 manual para calculadora fisica.");

            Cas::UnitDim tDim{};
            double tScale = 1.0;
            double tOffset = 0.0;
            if (!targetUnit.empty() &&
                Cas::TryParseUnitExpression(targetUnit, tDim, tScale, tOffset) &&
                tScale > 0.0 &&
                (tOffset != 0.0 || std::abs(tScale - 1.0) > 1e-12)) {
                targetUnitScale = tScale;
                targetUnitOffset = tOffset;
                hasTargetUnitTransform = true;
            }
        }
    }

    const bool hasArithmeticOps = (rhs.find_first_of("+-*/^") != std::string::npos);
    const bool isPhasor = (exprUtf8.find('@') != std::string::npos || exprUtf8.find("\xE2\x88\xA0") != std::string::npos);
    plan.appModeUtf8 = isPhasor ? "Complex" : "Calculate";

    std::ostringstream keys;
    if (isCw2) {
        keys << (isPhasor ? "[HOME] Complex [OK] " : "[HOME] Calculate [OK] ");
        pushStep("HOME", "HOME", KeyLayerKind::Normal, plan.appModeUtf8);
        pushStep("OK", plan.appModeUtf8, KeyLayerKind::Normal, plan.appModeUtf8);
    } else if (isVnPlus) {
        keys << (isPhasor ? "[MODE] [2] (CMPLX) " : "[MODE] [1] (COMP) ");
        pushStep("MODE", "MODE", KeyLayerKind::Normal, plan.appModeUtf8);
        pushStep(isPhasor ? "2" : "1", plan.appModeUtf8, KeyLayerKind::Normal, plan.appModeUtf8);
    } else if (isCg50) {
        keys << "[MENU] [1] (Run-Matrix) ";
        pushStep("MENU", "MENU", KeyLayerKind::Normal, plan.appModeUtf8);
        pushStep("1", "Run-Matrix", KeyLayerKind::Normal, plan.appModeUtf8);
    } else {
        keys << (isPhasor ? "[MENU] [2] (Complejos) " : "[MENU] [1] (Calcular) ");
        pushStep("MENU", "MENU", KeyLayerKind::Normal, plan.appModeUtf8);
        pushStep(isPhasor ? "2" : "1", plan.appModeUtf8, KeyLayerKind::Normal, plan.appModeUtf8);
    }

    const char* angleToken = isCw2 ? "[CATALOG:\xE2\x88\xA0] "
                           : (isVnPlus ? "[SHIFT] [(-)] (<) "
                           : (isCg50 ? "[SHIFT] [VARS] (<) " : "[OPTN][1:\xE2\x88\xA0] "));
    const char* imagToken = isCw2 ? "[i] " : (isVnPlus ? "[ENG:i] " : (isCg50 ? "[SHIFT] [0:i] " : "[ENG:i] "));
    const char* expPrefixToken = (isVnPlus || isCg50) ? "[EXP] " : "[x10^x] ";

    std::string exprKeysStr;
    bool expectOperand = true;
    int parenDepth = 0;
    bool inDenomFactor = false;
    bool denomWrapped = false;
    bool denomHasContent = false;
    int denomParenDepth = 0;
    std::size_t denomStartPos = 0;
    std::size_t factorStartPos = 0;
    bool factorPendingUnary = false;

    auto closeDenomIfNeeded = [&]() {
        if (inDenomFactor) {
            if (denomWrapped) {
                exprKeysStr += "[)] ";
                pushStep("PAREN_CIERRA", ")", KeyLayerKind::Normal, plan.appModeUtf8);
            }
            inDenomFactor = false;
            denomWrapped = false;
            denomHasContent = false;
        }
    };

    std::size_t i = 0;
    while (i < rhs.size()) {
        const unsigned char uc = static_cast<unsigned char>(rhs[i]);
        if (std::isspace(uc)) { ++i; continue; }
        if (std::isdigit(uc) || rhs[i] == '.') {
            if (expectOperand && !factorPendingUnary) {
                factorStartPos = exprKeysStr.size();
            }
            factorPendingUnary = false;
            std::size_t st = i;
            while (i < rhs.size() && (std::isdigit(static_cast<unsigned char>(rhs[i])) || rhs[i] == '.')) ++i;
            if (i < rhs.size() && (rhs[i] == 'e' || rhs[i] == 'E')) {
                std::size_t expPos = i + 1;
                if (expPos < rhs.size() && (rhs[expPos] == '+' || rhs[expPos] == '-')) {
                    ++expPos;
                }
                if (expPos < rhs.size() && std::isdigit(static_cast<unsigned char>(rhs[expPos]))) {
                    i = expPos;
                    while (i < rhs.size() && std::isdigit(static_cast<unsigned char>(rhs[i]))) ++i;
                }
            }
            const std::string num = rhs.substr(st, i - st);
            exprKeysStr += NumberToKeyTokens(num, plan.model) + " ";
            pushStep(num, num, KeyLayerKind::Normal, plan.appModeUtf8);
            if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
            expectOperand = false;
            continue;
        }
        if (rhs.compare(i, 3, "\xE2\x88\xA0") == 0) {
            if (parenDepth == denomParenDepth) closeDenomIfNeeded();
            factorPendingUnary = false;
            exprKeysStr += angleToken;
            pushStep("ANGLE", "\xE2\x88\xA0", (isVnPlus || isCg50) ? KeyLayerKind::Shift : KeyLayerKind::Normal, plan.appModeUtf8);
            expectOperand = true;
            i += 3;
            continue;
        }
        if (std::isalpha(uc)) {
            if (expectOperand && !factorPendingUnary) {
                factorStartPos = exprKeysStr.size();
            }
            factorPendingUnary = false;
            std::size_t st = i;
            while (i < rhs.size() && (std::isalnum(static_cast<unsigned char>(rhs[i])) || rhs[i] == '_')) ++i;
            const std::string word = rhs.substr(st, i - st);
            if (word == "sin" || word == "sind") { exprKeysStr += "[sin] "; pushStep("SIN", "sin(", KeyLayerKind::Normal, plan.appModeUtf8); expectOperand = true; }
            else if (word == "cos" || word == "cosd") { exprKeysStr += "[cos] "; pushStep("COS", "cos(", KeyLayerKind::Normal, plan.appModeUtf8); expectOperand = true; }
            else if (word == "tan" || word == "tand") { exprKeysStr += "[tan] "; pushStep("TAN", "tan(", KeyLayerKind::Normal, plan.appModeUtf8); expectOperand = true; }
            else if (word == "asin") { exprKeysStr += "[SHIFT] [sin] "; pushStep("SIN", "sin^-1(", KeyLayerKind::Shift, plan.appModeUtf8); expectOperand = true; }
            else if (word == "acos") { exprKeysStr += "[SHIFT] [cos] "; pushStep("COS", "cos^-1(", KeyLayerKind::Shift, plan.appModeUtf8); expectOperand = true; }
            else if (word == "atan") { exprKeysStr += "[SHIFT] [tan] "; pushStep("TAN", "tan^-1(", KeyLayerKind::Shift, plan.appModeUtf8); expectOperand = true; }
            else if (word == "sqrt") { exprKeysStr += "[sqrt] "; pushStep("RAIZ", "sqrt(", KeyLayerKind::Normal, plan.appModeUtf8); expectOperand = true; }
            else if (word == "ln") { exprKeysStr += "[ln] "; pushStep("LN", "ln(", KeyLayerKind::Normal, plan.appModeUtf8); expectOperand = true; }
            else if (word == "log10" || word == "log") { exprKeysStr += "[log] "; pushStep("LOG", "log(", KeyLayerKind::Normal, plan.appModeUtf8); expectOperand = true; }
            else if (word == "exp") { exprKeysStr += "[SHIFT] [ln] "; pushStep("LN", "e^(", KeyLayerKind::Shift, plan.appModeUtf8); expectOperand = true; }
            else if (word == "abs") { exprKeysStr += "[abs] "; pushStep("ABS", "abs(", KeyLayerKind::Shift, plan.appModeUtf8); expectOperand = true; }
            else if (word == "pi") { exprKeysStr += "[SHIFT][pi] "; pushStep("EXP", "pi", KeyLayerKind::Shift, plan.appModeUtf8); expectOperand = false; }
            else if (word == "Ans" || word == "ans") { exprKeysStr += "[ANS] "; pushStep("ANS", "Ans", KeyLayerKind::Normal, plan.appModeUtf8); expectOperand = false; }
            else if (word == "j" || word == "i") { exprKeysStr += imagToken; pushStep("i", "i", isCg50 ? KeyLayerKind::Shift : KeyLayerKind::Normal, plan.appModeUtf8); expectOperand = false; }
            else if (word[0] == 'k' && (word == "kOhm" || word == "kV" || word == "kW" || word == "kHz" || word == "kPa" || word == "kN")) {
                if (expectOperand) exprKeysStr += "1 ";
                exprKeysStr += std::string(expPrefixToken) + "3 ";
                pushStep("EXP", "x10^3 (" + word + ")", KeyLayerKind::Normal, plan.appModeUtf8);
                plan.warningsUtf8.push_back("Prefijo " + word + " traducido a x10^3.");
                if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                expectOperand = false;
            } else if (word[0] == 'm' && (word == "mA" || word == "mV" || word == "mW" || word == "mH" || word == "ms" || word == "mm")) {
                if (expectOperand) exprKeysStr += "1 ";
                exprKeysStr += std::string(expPrefixToken) + "[(-)] 3 ";
                pushStep("EXP", "x10^-3 (" + word + ")", KeyLayerKind::Normal, plan.appModeUtf8);
                plan.warningsUtf8.push_back("Prefijo " + word + " traducido a x10^-3.");
                if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                expectOperand = false;
            } else if (word[0] == 'u' && (word == "uF" || word == "uA" || word == "uV" || word == "uH")) {
                if (expectOperand) exprKeysStr += "1 ";
                exprKeysStr += std::string(expPrefixToken) + "[(-)] 6 ";
                pushStep("EXP", "x10^-6 (" + word + ")", KeyLayerKind::Normal, plan.appModeUtf8);
                plan.warningsUtf8.push_back("Prefijo " + word + " traducido a x10^-6.");
                if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                expectOperand = false;
            } else if (word[0] == 'n' && (word == "nF" || word == "nA")) {
                if (expectOperand) exprKeysStr += "1 ";
                exprKeysStr += std::string(expPrefixToken) + "[(-)] 9 ";
                pushStep("EXP", "x10^-9 (" + word + ")", KeyLayerKind::Normal, plan.appModeUtf8);
                if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                expectOperand = false;
            } else if (word[0] == 'M' && (word == "MOhm" || word == "MHz" || word == "MPa" || word == "MW")) {
                if (expectOperand) exprKeysStr += "1 ";
                exprKeysStr += std::string(expPrefixToken) + "6 ";
                pushStep("EXP", "x10^6 (" + word + ")", KeyLayerKind::Normal, plan.appModeUtf8);
                if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                expectOperand = false;
            } else {
                const auto envIt = env.find(word);
                Cas::UnitDim uDim{};
                double uScale = 1.0;
                double uOffset = 0.0;
                if (envIt != env.end() && envIt->second.IsReal() && std::isfinite(envIt->second.value.real())) {
                    const std::string valStr = FormatCasioDisplayValue(envIt->second.value.real(), false);
                    exprKeysStr += NumberToKeyTokens(valStr, plan.model) + " ";
                    pushStep(word, valStr, KeyLayerKind::Normal, plan.appModeUtf8);
                    if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                    expectOperand = false;
                } else if (Cas::TryParseUnitExpression(word, uDim, uScale, uOffset) && uScale > 0.0) {
                    if (uOffset != 0.0) {
                        if (hasPipeTarget || hasArithmeticOps) {
                            if (!expectOperand) {
                                const std::size_t insertPos = (factorStartPos <= exprKeysStr.size()) ? factorStartPos : 0;
                                exprKeysStr.insert(insertPos, "[(] ");
                                if (inDenomFactor && denomStartPos > insertPos) {
                                    denomStartPos += 4;
                                }
                                if (std::abs(uScale - 1.0) > 1e-12) {
                                    exprKeysStr += "[x] " + NumberToKeyTokens(FormatCasioDisplayValue(uScale, false), plan.model) + " ";
                                }
                                exprKeysStr += "[+] " + NumberToKeyTokens(FormatCasioDisplayValue(uOffset, false), plan.model) + " [)] ";
                            } else {
                                exprKeysStr += NumberToKeyTokens(FormatCasioDisplayValue(uOffset, false), plan.model) + " ";
                            }
                            plan.warningsUtf8.push_back("Unidad " + word + " escalada a SI.");
                        }
                        if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                        expectOperand = false;
                    } else {
                        if (std::abs(uScale - 1.0) > 1e-12) {
                            const double lg = std::log10(uScale);
                            const long long exp10 = std::llround(lg);
                            if (std::abs(lg - static_cast<double>(exp10)) < 1e-9) {
                                if (expectOperand) exprKeysStr += "1 ";
                                exprKeysStr += std::string(expPrefixToken) + (exp10 < 0 ? "[(-)] " : "") +
                                               std::to_string(std::abs(exp10)) + " ";
                            } else {
                                if (expectOperand) {
                                    exprKeysStr += NumberToKeyTokens(FormatCasioDisplayValue(uScale, false), plan.model) + " ";
                                } else {
                                    if (inDenomFactor && parenDepth == denomParenDepth && denomHasContent) {
                                        if (!denomWrapped) {
                                            exprKeysStr.insert(denomStartPos, "[(] ");
                                            if (factorStartPos >= denomStartPos) factorStartPos += 4;
                                            denomWrapped = true;
                                        }
                                    }
                                    exprKeysStr += "[x] " + NumberToKeyTokens(FormatCasioDisplayValue(uScale, false), plan.model) + " ";
                                }
                            }
                            plan.warningsUtf8.push_back("Unidad " + word + " escalada a SI.");
                        } else {
                            // Scale-1 SI base/derived unit (m, s, kg, A, K, V, Ohm, etc.)
                            std::size_t nextNonWs = i;
                            while (nextNonWs < rhs.size() && std::isspace(static_cast<unsigned char>(rhs[nextNonWs]))) {
                                ++nextNonWs;
                            }
                            const bool followedByPow = (nextNonWs < rhs.size() && rhs[nextNonWs] == '^');
                            if (expectOperand) {
                                exprKeysStr += "1 ";
                                pushStep("1", "1 (" + word + ")", KeyLayerKind::Normal, plan.appModeUtf8);
                            } else if (followedByPow) {
                                if (inDenomFactor && parenDepth == denomParenDepth && denomHasContent && !denomWrapped) {
                                    exprKeysStr.insert(denomStartPos, "[(] ");
                                    if (factorStartPos >= denomStartPos) factorStartPos += 4;
                                    denomWrapped = true;
                                }
                                exprKeysStr += "[x] 1 ";
                                pushStep("POR", "*", KeyLayerKind::Normal, plan.appModeUtf8);
                                pushStep("1", "1 (" + word + ")", KeyLayerKind::Normal, plan.appModeUtf8);
                            }
                        }
                        if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                        expectOperand = false;
                    }
                } else {
                    exprKeysStr += word + " ";
                    pushStep(word, word, KeyLayerKind::Normal, plan.appModeUtf8, "Identificador sin tecla directa");
                    plan.warningsUtf8.push_back("Identificador o funcion no soportada en teclado fisico: " + word);
                    if (inDenomFactor && parenDepth == denomParenDepth) denomHasContent = true;
                    expectOperand = false;
                }
            }
            continue;
        }
        switch (rhs[i]) {
        case '+':
            if (!expectOperand) {
                if (parenDepth == denomParenDepth) closeDenomIfNeeded();
                factorPendingUnary = false;
                exprKeysStr += "[+] ";
                pushStep("MAS", "+", KeyLayerKind::Normal, plan.appModeUtf8);
                expectOperand = true;
            }
            break;
        case '-':
            if (expectOperand) {
                if (!factorPendingUnary) {
                    factorStartPos = exprKeysStr.size();
                    factorPendingUnary = true;
                }
                exprKeysStr += "[(-)] ";
                pushStep("NEGATIVO", "(-)", KeyLayerKind::Normal, plan.appModeUtf8);
            } else {
                if (parenDepth == denomParenDepth) closeDenomIfNeeded();
                factorPendingUnary = false;
                exprKeysStr += "[-] ";
                pushStep("MENOS", "-", KeyLayerKind::Normal, plan.appModeUtf8);
            }
            expectOperand = true;
            break;
        case '*':
            if (parenDepth == denomParenDepth) closeDenomIfNeeded();
            factorPendingUnary = false;
            exprKeysStr += "[x] ";
            pushStep("POR", "*", KeyLayerKind::Normal, plan.appModeUtf8);
            expectOperand = true;
            break;
        case '/':
            if (parenDepth == denomParenDepth) closeDenomIfNeeded();
            factorPendingUnary = false;
            exprKeysStr += "[/] ";
            pushStep("ENTRE", "/", KeyLayerKind::Normal, plan.appModeUtf8);
            inDenomFactor = true;
            denomWrapped = false;
            denomHasContent = false;
            denomParenDepth = parenDepth;
            denomStartPos = exprKeysStr.size();
            expectOperand = true;
            break;
        case '^':
            factorPendingUnary = false;
            exprKeysStr += "[x^] ";
            pushStep("POTENCIA", "^", KeyLayerKind::Normal, plan.appModeUtf8);
            expectOperand = true;
            break;
        case '(':
            if (expectOperand && !factorPendingUnary) {
                factorStartPos = exprKeysStr.size();
            }
            factorPendingUnary = false;
            ++parenDepth;
            exprKeysStr += "[(] ";
            pushStep("PAREN_ABRE", "(", KeyLayerKind::Normal, plan.appModeUtf8);
            expectOperand = true;
            break;
        case ')':
            if (inDenomFactor && parenDepth == denomParenDepth) closeDenomIfNeeded();
            if (parenDepth > 0) --parenDepth;
            factorPendingUnary = false;
            exprKeysStr += "[)] ";
            pushStep("PAREN_CIERRA", ")", KeyLayerKind::Normal, plan.appModeUtf8);
            expectOperand = false;
            break;
        case '@':
            if (parenDepth == denomParenDepth) closeDenomIfNeeded();
            factorPendingUnary = false;
            exprKeysStr += angleToken;
            pushStep("ANGLE", "\xE2\x88\xA0", (isVnPlus || isCg50) ? KeyLayerKind::Shift : KeyLayerKind::Normal, plan.appModeUtf8);
            expectOperand = true;
            break;
        default: break;
        }
        ++i;
    }
    closeDenomIfNeeded();

    if (hasTargetUnitTransform) {
        if (targetUnitOffset != 0.0) {
            const std::string offStr = FormatCasioDisplayValue(targetUnitOffset, false);
            keys << "[(] " << exprKeysStr << "[-] " << NumberToKeyTokens(offStr, plan.model) << " [)] ";
            pushStep("MENOS", "-" + offStr, KeyLayerKind::Normal, plan.appModeUtf8);
            if (std::abs(targetUnitScale - 1.0) > 1e-12) {
                const std::string sStr = FormatCasioDisplayValue(targetUnitScale, false);
                keys << "[/] " << NumberToKeyTokens(sStr, plan.model) << " ";
                pushStep("ENTRE", "/" + sStr, KeyLayerKind::Normal, plan.appModeUtf8);
            }
        } else {
            const double invScale = 1.0 / targetUnitScale;
            const double lg = std::log10(invScale);
            const long long exp10 = std::llround(lg);
            keys << "[(] " << exprKeysStr << "[)] ";
            if (std::abs(lg - static_cast<double>(exp10)) < 1e-9 && exp10 != 0) {
                keys << "[x] 1 " << expPrefixToken << (exp10 < 0 ? "[(-)] " : "")
                     << std::abs(exp10) << " ";
                pushStep("EXP", "x10^" + std::to_string(exp10), KeyLayerKind::Normal, plan.appModeUtf8);
            } else {
                const std::string sStr = FormatCasioDisplayValue(targetUnitScale, false);
                keys << "[/] " << NumberToKeyTokens(sStr, plan.model) << " ";
                pushStep("ENTRE", "/" + sStr, KeyLayerKind::Normal, plan.appModeUtf8);
            }
        }
    } else {
        keys << exprKeysStr;
    }

    keys << exeBtn;
    pushStep((isCw2 || isCg50) ? "EXE" : "IGUAL", "=", KeyLayerKind::Normal, plan.appModeUtf8);
    if (isPhasor) {
        if (isCw2) keys << " -> [FORMAT] Polar Coord [OK]";
        else if (isVnPlus) keys << " -> [SHIFT] [2] (CMPLX) [3:r\xE2\x88\xA0\xCE\xB8] [=]";
        else if (isCg50) keys << " -> [OPTN] [F3:CPLX] [F6] [F3:r\xE2\x88\xA0\xCE\xB8] [EXE]";
        else keys << " -> [OPTN] [Down] [1:r\xE2\x88\xA0\xCE\xB8] [=]";
    }

    plan.keySequenceUtf8 = keys.str();
    const auto casOpts = CasEvalOptionsForAngleMode("DEG");
    const auto ev = Cas::EvaluateExpression(exprUtf8, env, casOpts);
    plan.ok = ev.ok;
    plan.evaluatedUtf8 = ev.ok ? ev.formattedResultUtf8 : ev.errorUtf8;
    plan.latexUtf8 = ExpressionToLatexUtf8(exprUtf8, plan.evaluatedUtf8);
    return plan;
}

std::string FormatPhasorAllFormsUtf8(std::complex<double> z, const std::string& unitUtf8) {
    constexpr double kPi = 3.14159265358979323846;
    const double mag = std::abs(z);
    const double deg = std::atan2(z.imag(), z.real()) * (180.0 / kPi);
    const double rad = std::atan2(z.imag(), z.real());
    const std::string u = unitUtf8.empty() ? "" : (" " + unitUtf8);

    std::ostringstream oss;
    oss.precision(4);
    oss << "Rect: (" << z.real() << (z.imag() >= 0.0 ? " + j" : " - j") << std::abs(z.imag()) << ")" << u
        << " | Polar: " << mag << "\xE2\x88\xA0" << deg << "\xC2\xB0" << u
        << " | Exp: " << mag << "\xC2\xB7" "e^(j" << rad << " rad)" << u;
    return oss.str();
}

// ---- JSON Serialization ----

std::string SerializeSessionStateToJson(const CalcSessionState& state)
{
    json j;
    j["schemaVersion"] = 1;
    j["calcSessionId"] = state.calcSessionIdUtf8;
    j["modelId"] = state.modelIdUtf8;
    j["appMode"] = state.appModeUtf8;
    j["inputSequence"] = state.inputSequenceUtf8;
    j["formulaSource"] = state.formulaSourceUtf8;
    j["unitAssumptions"] = state.unitAssumptionsUtf8;
    j["result"] = state.resultUtf8;
    j["latex"] = state.latexUtf8;
    j["verificationStatus"] = static_cast<int>(state.verificationStatus);
    j["verificationNote"] = state.verificationNoteUtf8;
    j["updatedUtcMs"] = state.updatedUtcMs;

    json sa;
    sa["shift"] = state.shiftAlphaState.shiftActive;
    sa["alpha"] = state.shiftAlphaState.alphaActive;
    sa["sto"] = state.shiftAlphaState.stoActive;
    sa["rcl"] = state.shiftAlphaState.rclActive;
    sa["angleMode"] = state.shiftAlphaState.angleModeUtf8;
    sa["displayFormat"] = state.shiftAlphaState.displayFormatUtf8;
    j["shiftAlphaState"] = sa;

    json stepsArr = json::array();
    for (const auto& st : state.steps) {
        json sj;
        sj["stepIndex"] = st.stepIndex;
        sj["modelId"] = st.modelIdUtf8;
        sj["keyName"] = st.keyNameUtf8;
        sj["displayCaption"] = st.displayCaptionUtf8;
        sj["layer"] = static_cast<int>(st.layer);
        sj["menuContext"] = st.menuContextUtf8;
        sj["expectedLcdText"] = st.expectedLcdTextUtf8;
        sj["note"] = st.noteUtf8;
        sj["timestampMs"] = st.timestampMs;
        stepsArr.push_back(std::move(sj));
    }
    j["steps"] = std::move(stepsArr);

    json framesArr = json::array();
    for (const auto& f : state.displayFrames) {
        framesArr.push_back(FrameToJson(f));
    }
    j["displayFrames"] = std::move(framesArr);
    return j.dump(2);
}

bool DeserializeSessionStateFromJson(const std::string& jsonUtf8, CalcSessionState& outState, std::string* outErr)
{
    try {
        json j = json::parse(jsonUtf8);
        if (!j.is_object()) {
            if (outErr) *outErr = "JSON de sesion no es objeto";
            return false;
        }
        CalcSessionState st;
        st.calcSessionIdUtf8 = j.value("calcSessionId", std::string());
        st.modelIdUtf8 = j.value("modelId", std::string("fx-991CW"));
        st.appModeUtf8 = j.value("appMode", std::string("Calculate"));
        st.inputSequenceUtf8 = j.value("inputSequence", std::string());
        st.formulaSourceUtf8 = j.value("formulaSource", std::string());
        st.unitAssumptionsUtf8 = j.value("unitAssumptions", std::string("DEG; SI"));
        st.resultUtf8 = j.value("result", std::string());
        st.latexUtf8 = j.value("latex", std::string());
        const int vs = j.value("verificationStatus", 0);
        st.verificationStatus = (vs >= 0 && vs <= 4)
            ? static_cast<VerificationStatus>(vs)
            : VerificationStatus::Unverified;
        st.verificationNoteUtf8 = j.value("verificationNote", std::string());
        st.updatedUtcMs = j.value("updatedUtcMs", 0ULL);

        if (j.contains("shiftAlphaState") && j["shiftAlphaState"].is_object()) {
            const auto& sa = j["shiftAlphaState"];
            st.shiftAlphaState.shiftActive = sa.value("shift", false);
            st.shiftAlphaState.alphaActive = sa.value("alpha", false);
            st.shiftAlphaState.stoActive = sa.value("sto", false);
            st.shiftAlphaState.rclActive = sa.value("rcl", false);
            st.shiftAlphaState.angleModeUtf8 = sa.value("angleMode", std::string("DEG"));
            st.shiftAlphaState.displayFormatUtf8 = sa.value("displayFormat", std::string("Norm"));
        }
        st.shiftAlphaState = NormalizeShiftAlphaState(st.shiftAlphaState, st.modelIdUtf8);

        if (j.contains("steps") && j["steps"].is_array()) {
            for (const auto& sj : j["steps"]) {
                CasioKeyStep ks;
                ks.stepIndex = sj.value("stepIndex", static_cast<std::size_t>(0));
                ks.modelIdUtf8 = sj.value("modelId", st.modelIdUtf8);
                ks.keyNameUtf8 = sj.value("keyName", std::string());
                ks.displayCaptionUtf8 = sj.value("displayCaption", std::string());
                const int ly = sj.value("layer", 0);
                ks.layer = (ly >= 0 && ly <= 3) ? static_cast<KeyLayerKind>(ly) : KeyLayerKind::Normal;
                ks.badge = MakeShiftAlphaBadge(ks.layer, ks.modelIdUtf8);
                ks.menuContextUtf8 = sj.value("menuContext", std::string());
                ks.expectedLcdTextUtf8 = sj.value("expectedLcdText", std::string());
                ks.noteUtf8 = sj.value("note", std::string());
                ks.timestampMs = sj.value("timestampMs", 0ULL);
                st.steps.push_back(std::move(ks));
            }
        }
        if (j.contains("displayFrames") && j["displayFrames"].is_array()) {
            for (const auto& fj : j["displayFrames"]) {
                st.displayFrames.push_back(FrameFromJson(fj));
            }
        }
        outState = std::move(st);
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

// ---- CalcIpcChannel (Local-Only Zero-Network IPC) ----

struct CalcIpcChannel::Impl {
    std::atomic<bool> open{false};
    CalcIpcConfig cfg{};
    fs::path rootDir;
    fs::path sessionsDir;
    fs::path toStudioDir;
    fs::path toCalcDir;
    std::string nativePipeName;
    std::atomic<std::uint64_t> nextSeq{1};

    std::mutex pipeMutex;
    std::condition_variable pipeCv;
    std::deque<std::string> pipeQueue;
    std::thread pipeServerThread;
    bool pipeServerRunning = false;
    std::atomic<bool> pipeStopRequested{false};
#ifdef _WIN32
    HANDLE hPipeStopEvent = nullptr;
#else
    int listenFd = -1;
    int wakePipe[2] = {-1, -1};
    std::string sockPath;
#endif

    void CloseNativePipe()
    {
#ifdef _WIN32
        HANDLE hStopCopy = nullptr;
        {
            std::lock_guard<std::mutex> lk(pipeMutex);
            pipeStopRequested.store(true);
            pipeServerRunning = false;
            hStopCopy = hPipeStopEvent;
        }
        pipeCv.notify_all();
        if (hStopCopy != nullptr) {
            SetEvent(hStopCopy);
        }
        if (pipeServerThread.joinable()) {
            pipeServerThread.join();
        }
        {
            std::lock_guard<std::mutex> lk(pipeMutex);
            if (hPipeStopEvent != nullptr) {
                CloseHandle(hPipeStopEvent);
                hPipeStopEvent = nullptr;
            }
            pipeQueue.clear();
            pipeStopRequested.store(false);
        }
#else
        int wakeWriteFd = -1;
        {
            std::lock_guard<std::mutex> lk(pipeMutex);
            pipeStopRequested.store(true);
            pipeServerRunning = false;
            wakeWriteFd = wakePipe[1];
        }
        pipeCv.notify_all();
        if (wakeWriteFd >= 0) {
            const char b = 1;
            if (::write(wakeWriteFd, &b, 1) < 0) {
            }
        }
        if (pipeServerThread.joinable()) {
            pipeServerThread.join();
        }
        {
            std::lock_guard<std::mutex> lk(pipeMutex);
            if (listenFd >= 0) {
                ::close(listenFd);
                listenFd = -1;
            }
            if (wakePipe[0] >= 0) {
                ::close(wakePipe[0]);
                wakePipe[0] = -1;
            }
            if (wakePipe[1] >= 0) {
                ::close(wakePipe[1]);
                wakePipe[1] = -1;
            }
            if (!sockPath.empty()) {
                ::unlink(sockPath.c_str());
                sockPath.clear();
            }
            pipeQueue.clear();
            pipeStopRequested.store(false);
        }
#endif
    }
};

CalcIpcChannel::CalcIpcChannel()
    : impl_(std::make_unique<Impl>())
{
}

CalcIpcChannel::~CalcIpcChannel()
{
    Close();
}

bool CalcIpcChannel::Open(const CalcIpcConfig& cfg, std::string* outErr)
{
    Close();
    impl_->cfg = cfg;
    const std::string cleanName = SanitizeIdentifier(cfg.channelNameUtf8, "default");

    fs::path baseDir;
    if (!cfg.directoryOverrideUtf8.empty()) {
        baseDir = fs::path(cfg.directoryOverrideUtf8);
    } else {
        std::error_code ec;
        const std::string xdg = GetEnvVarUtf8("XDG_RUNTIME_DIR");
        const std::string localApp = GetEnvVarUtf8("LOCALAPPDATA");
        if (!localApp.empty()) {
            baseDir = fs::path(localApp) / "CalculadoraUnificada" / "ipc" / cleanName;
        } else if (!xdg.empty()) {
            baseDir = fs::path(xdg) / "calculadora_unificada_ipc" / cleanName;
        } else {
            baseDir = fs::temp_directory_path(ec) / "calculadora_unificada_ipc" / cleanName;
        }
    }

    std::error_code ec;
    impl_->rootDir = baseDir;
    impl_->sessionsDir = baseDir / "sessions";
    impl_->toStudioDir = baseDir / "to_studio";
    impl_->toCalcDir = baseDir / "to_calc";
    fs::create_directories(impl_->sessionsDir, ec);
    fs::create_directories(impl_->toStudioDir, ec);
    fs::create_directories(impl_->toCalcDir, ec);
    if (ec) {
        if (outErr) *outErr = "No se pudo crear directorio IPC local: " + baseDir.string();
        return false;
    }

    const std::uint64_t nameHash = Fnv1a64(baseDir.string() + ":" + cleanName);
    std::ostringstream hashHex;
    hashHex << std::hex << std::setw(8) << std::setfill('0') << (nameHash & 0xFFFFFFFFULL);
#ifdef _WIN32
    impl_->nativePipeName = "\\\\.\\pipe\\CalcUnifiedIpc_" + cleanName + "_" + hashHex.str();
#else
    std::string runtimeRoot = GetEnvVarUtf8("XDG_RUNTIME_DIR");
    if (runtimeRoot.empty() || runtimeRoot.size() > 50) {
        runtimeRoot = "/tmp";
    }
    impl_->nativePipeName = runtimeRoot + "/cu_ipc_" + cleanName.substr(0, 16) + "_" + hashHex.str() + ".sock";
#endif

    impl_->open.store(true, std::memory_order_release);
    return true;
}

void CalcIpcChannel::Close()
{
    if (!impl_) return;
    impl_->open.store(false, std::memory_order_release);
    impl_->CloseNativePipe();
}

bool CalcIpcChannel::IsOpen() const noexcept
{
    return impl_ && impl_->open.load(std::memory_order_acquire);
}

std::string CalcIpcChannel::ChannelDirectoryUtf8() const
{
    return IsOpen() ? impl_->rootDir.string() : std::string();
}

std::string CalcIpcChannel::NativePipeNameUtf8() const
{
    return IsOpen() ? impl_->nativePipeName : std::string();
}

bool CalcIpcChannel::Send(const CalcIpcMessage& msg, std::string* outErr)
{
    if (!IsOpen()) {
        if (outErr) *outErr = "Canal IPC cerrado";
        return false;
    }
    const std::uint64_t seq = msg.seq != 0
        ? msg.seq
        : impl_->nextSeq.fetch_add(1, std::memory_order_relaxed);
    const std::uint64_t ts = msg.timestampUtcMs != 0 ? msg.timestampUtcMs : NowUtcMs();

    json j;
    j["seq"] = seq;
    j["timestampUtcMs"] = ts;
    j["sender"] = static_cast<int>(msg.sender);
    j["type"] = static_cast<int>(msg.type);
    j["sessionId"] = msg.sessionIdUtf8;
    j["modelId"] = msg.modelIdUtf8;
    j["keySeq"] = msg.keySeqUtf8;
    j["expression"] = msg.expressionUtf8;
    j["result"] = msg.resultUtf8;
    j["payloadJson"] = msg.payloadJsonUtf8;

    const fs::path targetDir = (msg.sender == CalcIpcRole::CalculatorHost)
        ? impl_->toStudioDir
        : impl_->toCalcDir;
    std::ostringstream fname;
    fname << std::setw(16) << std::setfill('0') << ts << "_"
          << std::setw(8) << std::setfill('0') << seq << "_"
          << CurrentPidU64() << ".json";
    return WriteFileAtomic(targetDir / fname.str(), j.dump(2), outErr);
}

std::vector<CalcIpcMessage> CalcIpcChannel::Poll(std::size_t maxMessages, std::string* outErr)
{
    std::vector<CalcIpcMessage> out;
    if (!IsOpen()) {
        if (outErr) *outErr = "Canal IPC cerrado";
        return out;
    }
    const fs::path inboxDir = (impl_->cfg.role == CalcIpcRole::DocumentStudio)
        ? impl_->toStudioDir
        : impl_->toCalcDir;

    std::error_code ec;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(inboxDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& p : files) {
        if (out.size() >= maxMessages) break;
        std::string raw;
        if (!ReadFileString(p, raw, nullptr)) continue;
        try {
            json j = json::parse(raw);
            CalcIpcMessage m;
            m.seq = j.value("seq", 0ULL);
            m.timestampUtcMs = j.value("timestampUtcMs", 0ULL);
            const int sRole = j.value("sender", 0);
            m.sender = (sRole == 1) ? CalcIpcRole::DocumentStudio : CalcIpcRole::CalculatorHost;
            const int tp = j.value("type", 1);
            m.type = (tp >= 0 && tp <= 6)
                ? static_cast<CalcIpcMessageType>(tp)
                : CalcIpcMessageType::SessionStateSync;
            m.sessionIdUtf8 = j.value("sessionId", std::string());
            m.modelIdUtf8 = j.value("modelId", std::string());
            m.keySeqUtf8 = j.value("keySeq", std::string());
            m.expressionUtf8 = j.value("expression", std::string());
            m.resultUtf8 = j.value("result", std::string());
            m.payloadJsonUtf8 = j.value("payloadJson", std::string());
            out.push_back(std::move(m));
            fs::remove(p, ec);
        } catch (...) {
            // Leave malformed or in-flight files untouched on parse race
        }
    }
    return out;
}

bool CalcIpcChannel::PublishSession(const CalcSessionState& state, std::string* outErr)
{
    if (!IsOpen()) {
        if (outErr) *outErr = "Canal IPC cerrado";
        return false;
    }
    if (state.calcSessionIdUtf8.empty()) {
        if (outErr) *outErr = "calcSessionIdUtf8 vacio";
        return false;
    }
    const std::string payload = SerializeSessionStateToJson(state);
    const std::string cleanId = SanitizeIdentifier(state.calcSessionIdUtf8, "session");
    {
        ScopedIpcSessionLock lock(impl_->sessionsDir / (cleanId + ".lock"));
        if (!lock.Acquired()) {
            if (outErr) *outErr = "Timeout esperando lock IPC de sesion";
            return false;
        }
        if (!WriteFileAtomic(impl_->sessionsDir / (cleanId + ".json"), payload, outErr)) {
            return false;
        }
        RegisterLiveSessionState(state);
    }

    CalcIpcMessage msg;
    msg.sender = impl_->cfg.role;
    msg.type = CalcIpcMessageType::SessionStateSync;
    msg.sessionIdUtf8 = state.calcSessionIdUtf8;
    msg.modelIdUtf8 = state.modelIdUtf8;
    msg.keySeqUtf8 = state.inputSequenceUtf8;
    msg.expressionUtf8 = state.formulaSourceUtf8;
    msg.resultUtf8 = state.resultUtf8;
    msg.payloadJsonUtf8 = payload;
    return Send(msg, outErr);
}

bool CalcIpcChannel::ReadSession(const std::string& sessionIdUtf8, CalcSessionState& outState, std::string* outErr) const
{
    if (!IsOpen()) {
        if (outErr) *outErr = "Canal IPC cerrado";
        return false;
    }
    const std::string cleanId = SanitizeIdentifier(sessionIdUtf8, "session");
    const fs::path p = impl_->sessionsDir / (cleanId + ".json");
    std::string raw;
    {
        std::lock_guard<std::mutex> rmwLk(IpcSessionRmwMutex());
        if (!ReadFileString(p, raw, outErr)) return false;
    }
    return DeserializeSessionStateFromJson(raw, outState, outErr);
}

std::vector<std::string> CalcIpcChannel::ListActiveSessions() const
{
    std::vector<std::string> ids;
    if (!IsOpen()) return ids;
    std::lock_guard<std::mutex> rmwLk(IpcSessionRmwMutex());
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(impl_->sessionsDir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
            std::string raw;
            CalcSessionState st;
            if (ReadFileString(entry.path(), raw, nullptr) &&
                DeserializeSessionStateFromJson(raw, st, nullptr) &&
                !st.calcSessionIdUtf8.empty()) {
                ids.push_back(st.calcSessionIdUtf8);
            }
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool CalcIpcChannel::PushLcdFrame(const std::string& sessionIdUtf8, const CalcDisplayFrame& frame, std::string* outErr)
{
    if (!IsOpen()) {
        if (outErr) *outErr = "Canal IPC cerrado";
        return false;
    }
    CalcDisplayFrame copy = frame;
    if (copy.lcdPngBytes.empty() && copy.pixelWidth > 0 && copy.pixelHeight > 0 && !copy.rgbaPixels.empty()) {
        copy.lcdPngBytes = EncodeRgbaToPng(copy.pixelWidth, copy.pixelHeight, copy.rgbaPixels);
    }

    const std::string cleanId = SanitizeIdentifier(sessionIdUtf8, "session");
    CalcSessionState st;
    {
        ScopedIpcSessionLock lock(impl_->sessionsDir / (cleanId + ".lock"));
        if (!lock.Acquired()) {
            if (outErr) *outErr = "Timeout esperando lock IPC de sesion";
            return false;
        }
        std::string rawExisting;
        if (!ReadFileString(impl_->sessionsDir / (cleanId + ".json"), rawExisting, nullptr) ||
            !DeserializeSessionStateFromJson(rawExisting, st, nullptr)) {
            st = CalcSessionState{};
            st.calcSessionIdUtf8 = sessionIdUtf8;
        }
        st.displayFrames.push_back(copy);
        if (!copy.resultUtf8.empty()) st.resultUtf8 = copy.resultUtf8;
        st.shiftAlphaState = copy.shiftAlphaState;
        st.updatedUtcMs = NowUtcMs();

        const std::string payload = SerializeSessionStateToJson(st);
        if (!WriteFileAtomic(impl_->sessionsDir / (cleanId + ".json"), payload, outErr)) {
            return false;
        }
        RegisterLiveSessionState(st);
    }

    CalcIpcMessage msg;
    msg.sender = impl_->cfg.role;
    msg.type = CalcIpcMessageType::LcdFrameCapture;
    msg.sessionIdUtf8 = sessionIdUtf8;
    msg.modelIdUtf8 = st.modelIdUtf8;
    msg.resultUtf8 = copy.resultUtf8;
    msg.payloadJsonUtf8 = FrameToJson(copy).dump();
    return Send(msg, outErr);
}

bool CalcIpcChannel::RequestKeystrokeReplay(
    const std::string& sessionIdUtf8,
    const std::string& modelIdUtf8,
    const std::string& keySeqUtf8,
    const std::string& expressionUtf8,
    std::string* outErr)
{
    CalcIpcMessage msg;
    msg.sender = impl_->cfg.role;
    msg.type = CalcIpcMessageType::KeystrokeReplay;
    msg.sessionIdUtf8 = sessionIdUtf8;
    msg.modelIdUtf8 = modelIdUtf8;
    msg.keySeqUtf8 = keySeqUtf8;
    msg.expressionUtf8 = expressionUtf8;
    return Send(msg, outErr);
}

bool CalcIpcChannel::PullIntoCard(AnnotationObject& cardObj, const Cas::SymbolEnv& env, std::string* outErr) const
{
    if (cardObj.kind != ObjectKind::CalcSessionCard) {
        if (outErr) *outErr = "AnnotationObject no es CalcSessionCard";
        return false;
    }
    const std::string sid = ExtractSessionIdFromCard(cardObj);
    if (sid.empty()) {
        if (outErr) *outErr = "Tarjeta sin calcSessionId vinculado";
        return false;
    }
    CalcSessionState st;
    if (!ReadSession(sid, st, outErr)) return false;

    cardObj.calcModelIdUtf8 = st.modelIdUtf8;
    if (!st.inputSequenceUtf8.empty()) cardObj.calcKeySeqUtf8 = st.inputSequenceUtf8;
    if (!st.formulaSourceUtf8.empty()) cardObj.formulaExprUtf8 = st.formulaSourceUtf8;
    if (!st.displayFrames.empty()) {
        const auto& last = st.displayFrames.back();
        if (!last.lcdPngBytes.empty()) {
            cardObj.pngBytes = last.lcdPngBytes;
        } else if (last.pixelWidth > 0 && last.pixelHeight > 0 && !last.rgbaPixels.empty()) {
            cardObj.pngBytes = EncodeRgbaToPng(last.pixelWidth, last.pixelHeight, last.rgbaPixels);
        }
    }
    RegisterLiveSessionState(st);
    const bool ok = RefreshCalcSessionCard(cardObj, env);
    if (!st.resultUtf8.empty() && cardObj.evaluatedResultUtf8.empty()) {
        cardObj.evaluatedResultUtf8 = st.resultUtf8;
    }
    return ok;
}

bool CalcIpcChannel::StartNativePipeServer(std::string* outErr)
{
    if (!IsOpen()) {
        if (outErr) *outErr = "Canal IPC cerrado";
        return false;
    }
    impl_->CloseNativePipe();

#ifdef _WIN32
    constexpr std::size_t kPoolSize = 16;
    const std::wstring wName(impl_->nativePipeName.begin(), impl_->nativePipeName.end());

    struct PipeListenSlot {
        HANDLE pipe = INVALID_HANDLE_VALUE;
        HANDLE connEv = nullptr;
        std::unique_ptr<OVERLAPPED> ov;
        bool pending = false;
        bool connected = false;
    };

    auto armSlot = [wName](PipeListenSlot& slot, DWORD& outErrCode) -> bool {
        if (slot.connEv == nullptr) {
            slot.connEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (slot.connEv == nullptr) {
                outErrCode = GetLastError();
                return false;
            }
        }
        ResetEvent(slot.connEv);
        slot.pipe = CreateNamedPipeW(
            wName.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536,
            65536,
            0,
            nullptr);
        if (slot.pipe == INVALID_HANDLE_VALUE) {
            outErrCode = GetLastError();
            return false;
        }
        slot.ov = std::make_unique<OVERLAPPED>();
        slot.ov->hEvent = slot.connEv;
        slot.pending = false;
        slot.connected = false;
        if (ConnectNamedPipe(slot.pipe, slot.ov.get())) {
            slot.connected = true;
            SetEvent(slot.connEv);
            return true;
        }
        const DWORD e = GetLastError();
        if (e == ERROR_PIPE_CONNECTED) {
            slot.connected = true;
            SetEvent(slot.connEv);
            return true;
        }
        if (e == ERROR_IO_PENDING) {
            slot.pending = true;
            return true;
        }
        outErrCode = e;
        CloseHandle(slot.pipe);
        slot.pipe = INVALID_HANDLE_VALUE;
        slot.ov.reset();
        return false;
    };

    auto cleanupSlots = [](std::vector<PipeListenSlot>& slots) {
        for (auto& s : slots) {
            if (s.pipe != INVALID_HANDLE_VALUE) {
                if (s.pending && s.ov) {
                    CancelIoEx(s.pipe, s.ov.get());
                    DWORD unused = 0;
                    GetOverlappedResult(s.pipe, s.ov.get(), &unused, TRUE);
                }
                CloseHandle(s.pipe);
                s.pipe = INVALID_HANDLE_VALUE;
            }
            s.ov.reset();
            if (s.connEv != nullptr) {
                CloseHandle(s.connEv);
                s.connEv = nullptr;
            }
            s.pending = false;
            s.connected = false;
        }
    };

    HANDLE hStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (hStop == nullptr) {
        const DWORD err = GetLastError();
        if (outErr) *outErr = "CreateEventW fallo con codigo " + std::to_string(err);
        return false;
    }

    auto slotsPtr = std::make_shared<std::vector<PipeListenSlot>>(kPoolSize);
    for (std::size_t k = 0; k < kPoolSize; ++k) {
        DWORD armErr = 0;
        if (!armSlot((*slotsPtr)[k], armErr)) {
            cleanupSlots(*slotsPtr);
            CloseHandle(hStop);
            if (outErr) *outErr = "CreateNamedPipeW/ConnectNamedPipe fallo con codigo " + std::to_string(armErr);
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lk(impl_->pipeMutex);
        impl_->hPipeStopEvent = hStop;
        impl_->pipeStopRequested.store(false);
        impl_->pipeServerRunning = true;
    }

    Impl* implPtr = impl_.get();
    impl_->pipeServerThread = std::thread([implPtr, slotsPtr, armSlot, cleanupSlots]() mutable {
        struct ServerExitGuard {
            Impl* impl;
            ~ServerExitGuard() {
                {
                    std::lock_guard<std::mutex> lk(impl->pipeMutex);
                    impl->pipeServerRunning = false;
                }
                impl->pipeCv.notify_all();
            }
        } exitGuard{implPtr};

        struct WorkerEntry {
            std::thread th;
            std::atomic<bool> done{false};
        };
        std::vector<std::shared_ptr<WorkerEntry>> workers;

        struct PipeOrderState {
            std::uint64_t nextDeliverSeq = 0;
            std::map<std::uint64_t, std::pair<bool, std::string>> staged;
        };
        auto orderState = std::make_shared<PipeOrderState>();
        auto deliverResult = [implPtr, orderState](std::uint64_t seq, bool ok, std::string payload) {
            bool pushedAny = false;
            {
                std::lock_guard<std::mutex> lk(implPtr->pipeMutex);
                orderState->staged.emplace(seq, std::make_pair(ok, std::move(payload)));
                while (true) {
                    auto it = orderState->staged.find(orderState->nextDeliverSeq);
                    if (it == orderState->staged.end()) break;
                    if (it->second.first) {
                        implPtr->pipeQueue.push_back(std::move(it->second.second));
                        pushedAny = true;
                    }
                    orderState->staged.erase(it);
                    ++orderState->nextDeliverSeq;
                }
            }
            if (pushedAny) {
                implPtr->pipeCv.notify_all();
            }
        };
        std::uint64_t nextAcceptSeq = 0;

        auto reapDoneWorkers = [&workers]() {
            auto it = workers.begin();
            while (it != workers.end()) {
                if ((*it)->done.load()) {
                    if ((*it)->th.joinable()) (*it)->th.join();
                    it = workers.erase(it);
                } else {
                    ++it;
                }
            }
        };

        auto spawnClientWorker = [&](HANDLE hClientPipe, std::uint64_t seq) {
            reapDoneWorkers();
            auto entry = std::make_shared<WorkerEntry>();
            entry->th = std::thread([implPtr, hClientPipe, seq, deliverResult, entry]() {
                struct DoneGuard {
                    std::shared_ptr<WorkerEntry> e;
                    ~DoneGuard() { e->done.store(true); }
                } doneGuard{entry};
                struct PipeCloser {
                    HANDLE h;
                    ~PipeCloser() {
                        if (h != INVALID_HANDLE_VALUE) {
                            DisconnectNamedPipe(h);
                            CloseHandle(h);
                        }
                    }
                } pipeCloser{hClientPipe};
                bool delivered = false;
                std::string finalPayload;
                struct DeliveryGuard {
                    std::uint64_t seq;
                    bool& ok;
                    std::string& payload;
                    const decltype(deliverResult)& deliver;
                    ~DeliveryGuard() {
                        deliver(seq, ok, std::move(payload));
                    }
                } deliveryGuard{seq, delivered, finalPayload, deliverResult};

                HANDLE hReadEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (hReadEv == nullptr) return;
                struct EventCloser {
                    HANDLE h;
                    ~EventCloser() { if (h != nullptr) CloseHandle(h); }
                } evCloser{hReadEv};

                auto readExact = [&](void* dst, DWORD need) -> bool {
                    DWORD got = 0;
                    while (got < need) {
                        if (implPtr->pipeStopRequested.load()) return false;
                        OVERLAPPED rOv{};
                        rOv.hEvent = hReadEv;
                        ResetEvent(hReadEv);
                        DWORD chunk = 0;
                        const DWORD toRead = (std::min<DWORD>)(65536u, need - got);
                        BOOL ok = ReadFile(hClientPipe, static_cast<std::uint8_t*>(dst) + got, toRead, &chunk, &rOv);
                        if (!ok) {
                            const DWORD err = GetLastError();
                            if (err == ERROR_IO_PENDING) {
                                HANDLE waits[2] = { implPtr->hPipeStopEvent, hReadEv };
                                const DWORD wr = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
                                if (wr != WAIT_OBJECT_0 + 1) {
                                    CancelIoEx(hClientPipe, &rOv);
                                    DWORD cancelledBytes = 0;
                                    GetOverlappedResult(hClientPipe, &rOv, &cancelledBytes, TRUE);
                                    return false;
                                }
                                if (!GetOverlappedResult(hClientPipe, &rOv, &chunk, FALSE) || chunk == 0) {
                                    return false;
                                }
                            } else {
                                return false;
                            }
                        }
                        if (chunk == 0) return false;
                        got += chunk;
                    }
                    return true;
                };

                std::uint8_t hdr[4]{};
                if (readExact(hdr, 4u)) {
                    const std::uint32_t len = static_cast<std::uint32_t>(hdr[0]) |
                        (static_cast<std::uint32_t>(hdr[1]) << 8) |
                        (static_cast<std::uint32_t>(hdr[2]) << 16) |
                        (static_cast<std::uint32_t>(hdr[3]) << 24);
                    if (len <= (16u * 1024u * 1024u)) {
                        std::string payload(len, '\0');
                        if (len == 0 || readExact(payload.data(), len)) {
                            finalPayload = std::move(payload);
                            delivered = true;
                        }
                    }
                }
            });
            workers.push_back(std::move(entry));
        };

        auto& slots = *slotsPtr;
        HANDLE hFastEv = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        auto tryReadSmallInline = [&](HANDLE hClientPipe, std::uint64_t seq) -> bool {
            if (hFastEv == nullptr) return false;
            std::uint8_t hdr[4]{};
            DWORD peekRead = 0;
            DWORD totalAvail = 0;
            if (!PeekNamedPipe(hClientPipe, hdr, 4, &peekRead, &totalAvail, nullptr) || peekRead < 4) {
                return false;
            }
            const std::uint32_t len = static_cast<std::uint32_t>(hdr[0]) |
                (static_cast<std::uint32_t>(hdr[1]) << 8) |
                (static_cast<std::uint32_t>(hdr[2]) << 16) |
                (static_cast<std::uint32_t>(hdr[3]) << 24);
            if (len > 65536u || totalAvail < 4u + len) {
                return false;
            }
            std::vector<std::uint8_t> raw(4u + len);
            std::size_t got = 0;
            while (got < raw.size()) {
                OVERLAPPED ov{};
                ov.hEvent = hFastEv;
                ResetEvent(hFastEv);
                DWORD chunk = 0;
                const DWORD toRead = static_cast<DWORD>(raw.size() - got);
                BOOL ok = ReadFile(hClientPipe, raw.data() + got, toRead, &chunk, &ov);
                if (!ok) {
                    if (GetLastError() == ERROR_IO_PENDING) {
                        if (!GetOverlappedResult(hClientPipe, &ov, &chunk, TRUE) || chunk == 0) {
                            deliverResult(seq, false, std::string());
                            DisconnectNamedPipe(hClientPipe);
                            CloseHandle(hClientPipe);
                            return true;
                        }
                    } else {
                        deliverResult(seq, false, std::string());
                        DisconnectNamedPipe(hClientPipe);
                        CloseHandle(hClientPipe);
                        return true;
                    }
                }
                if (chunk == 0) {
                    deliverResult(seq, false, std::string());
                    DisconnectNamedPipe(hClientPipe);
                    CloseHandle(hClientPipe);
                    return true;
                }
                got += chunk;
            }
            std::string payload(reinterpret_cast<const char*>(raw.data() + 4), len);
            deliverResult(seq, true, std::move(payload));
            DisconnectNamedPipe(hClientPipe);
            CloseHandle(hClientPipe);
            return true;
        };

        auto dispatchClient = [&](HANDLE hClientPipe) {
            const std::uint64_t seq = nextAcceptSeq++;
            if (!tryReadSmallInline(hClientPipe, seq)) {
                spawnClientWorker(hClientPipe, seq);
            }
        };

        while (!implPtr->pipeStopRequested.load()) {
            bool dispatchedReady = false;
            for (std::size_t k = 0; k < slots.size(); ++k) {
                if (slots[k].pipe != INVALID_HANDLE_VALUE && slots[k].connected) {
                    HANDLE hConn = slots[k].pipe;
                    slots[k].pipe = INVALID_HANDLE_VALUE;
                    slots[k].ov.reset();
                    slots[k].pending = false;
                    slots[k].connected = false;
                    DWORD armErr = 0;
                    if (!implPtr->pipeStopRequested.load()) {
                        armSlot(slots[k], armErr);
                    }
                    dispatchClient(hConn);
                    dispatchedReady = true;
                }
            }
            if (dispatchedReady) continue;

            HANDLE waits[1 + kPoolSize]{};
            std::size_t slotMap[kPoolSize]{};
            waits[0] = implPtr->hPipeStopEvent;
            DWORD waitCount = 1;
            for (std::size_t k = 0; k < slots.size(); ++k) {
                if (slots[k].pipe != INVALID_HANDLE_VALUE && slots[k].connEv != nullptr) {
                    slotMap[waitCount - 1] = k;
                    waits[waitCount] = slots[k].connEv;
                    ++waitCount;
                }
            }
            if (waitCount == 1) break;

            const DWORD wr = WaitForMultipleObjects(waitCount, waits, FALSE, INFINITE);
            if (wr == WAIT_OBJECT_0 || wr == WAIT_FAILED || wr >= WAIT_OBJECT_0 + waitCount) {
                break;
            }
            const std::size_t idx = slotMap[wr - (WAIT_OBJECT_0 + 1)];
            auto& slot = slots[idx];
            DWORD unused = 0;
            bool connOk = !slot.pending || GetOverlappedResult(slot.pipe, slot.ov.get(), &unused, FALSE);
            HANDLE hClientPipe = slot.pipe;
            slot.pipe = INVALID_HANDLE_VALUE;
            slot.ov.reset();
            slot.pending = false;
            slot.connected = false;
            if (!implPtr->pipeStopRequested.load()) {
                DWORD armErr = 0;
                armSlot(slot, armErr);
            }
            if (connOk && hClientPipe != INVALID_HANDLE_VALUE) {
                dispatchClient(hClientPipe);
            } else if (hClientPipe != INVALID_HANDLE_VALUE) {
                CloseHandle(hClientPipe);
            }
        }

        cleanupSlots(slots);
        if (hFastEv != nullptr) CloseHandle(hFastEv);
        for (auto& w : workers) {
            if (w->th.joinable()) w->th.join();
        }
        workers.clear();
    });
    return true;
#else
    if (impl_->nativePipeName.size() >= sizeof(sockaddr_un::sun_path)) {
        if (outErr) *outErr = "Ruta de socket AF_UNIX excede sun_path";
        return false;
    }
    auto setNonBlock = [](int fd) {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    };
    ::unlink(impl_->nativePipeName.c_str());
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        if (outErr) *outErr = "socket(AF_UNIX) fallo";
        return false;
    }
    setNonBlock(fd);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, impl_->nativePipeName.c_str(), impl_->nativePipeName.size() + 1);
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (outErr) *outErr = "bind(AF_UNIX) fallo: " + std::string(std::strerror(errno));
        ::close(fd);
        return false;
    }
    if (::listen(fd, 64) != 0) {
        if (outErr) *outErr = "listen(AF_UNIX) fallo";
        ::close(fd);
        ::unlink(impl_->nativePipeName.c_str());
        return false;
    }
    int wp[2] = {-1, -1};
    if (::pipe(wp) != 0) {
        if (outErr) *outErr = "pipe() fallo para AF_UNIX server";
        ::close(fd);
        ::unlink(impl_->nativePipeName.c_str());
        return false;
    }
    setNonBlock(wp[0]);
    setNonBlock(wp[1]);

    {
        std::lock_guard<std::mutex> lk(impl_->pipeMutex);
        impl_->listenFd = fd;
        impl_->wakePipe[0] = wp[0];
        impl_->wakePipe[1] = wp[1];
        impl_->sockPath = impl_->nativePipeName;
        impl_->pipeStopRequested.store(false);
        impl_->pipeServerRunning = true;
    }

    Impl* implPtr = impl_.get();
    const int wakeReadFd = wp[0];
    impl_->pipeServerThread = std::thread([implPtr, fd, wakeReadFd, setNonBlock]() {
        struct ServerExitGuard {
            Impl* impl;
            ~ServerExitGuard() {
                {
                    std::lock_guard<std::mutex> lk(impl->pipeMutex);
                    impl->pipeServerRunning = false;
                }
                impl->pipeCv.notify_all();
            }
        } exitGuard{implPtr};

        struct ClientConn {
            int cfd = -1;
            std::uint64_t acceptSeq = 0;
            std::uint8_t hdr[4]{};
            std::size_t hGot = 0;
            bool hasLen = false;
            std::uint32_t len = 0;
            std::string payload;
            std::size_t pGot = 0;
            bool delivered = false;
        };
        std::vector<ClientConn> clients;
        std::uint64_t nextAcceptSeq = 0;
        std::uint64_t nextDeliverSeq = 0;
        std::map<std::uint64_t, std::pair<bool, std::string>> staged;

        auto deliverResult = [&](std::uint64_t seq, bool ok, std::string payload) {
            bool pushedAny = false;
            {
                std::lock_guard<std::mutex> lk(implPtr->pipeMutex);
                staged.emplace(seq, std::make_pair(ok, std::move(payload)));
                while (true) {
                    auto it = staged.find(nextDeliverSeq);
                    if (it == staged.end()) break;
                    if (it->second.first) {
                        implPtr->pipeQueue.push_back(std::move(it->second.second));
                        pushedAny = true;
                    }
                    staged.erase(it);
                    ++nextDeliverSeq;
                }
            }
            if (pushedAny) {
                implPtr->pipeCv.notify_all();
            }
        };

        // Returns true if this connection is finished (either delivered or errored) and should be closed+removed.
        auto pumpClient = [&](ClientConn& c) -> bool {
            while (!implPtr->pipeStopRequested.load()) {
                if (!c.hasLen) {
                    const ssize_t n = ::recv(c.cfd, c.hdr + c.hGot, 4u - c.hGot, 0);
                    if (n < 0) {
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
                        if (!c.delivered) { c.delivered = true; deliverResult(c.acceptSeq, false, std::string()); }
                        return true;
                    }
                    if (n == 0) {
                        if (!c.delivered) { c.delivered = true; deliverResult(c.acceptSeq, false, std::string()); }
                        return true;
                    }
                    c.hGot += static_cast<std::size_t>(n);
                    if (c.hGot == 4u) {
                        c.len = static_cast<std::uint32_t>(c.hdr[0]) |
                            (static_cast<std::uint32_t>(c.hdr[1]) << 8) |
                            (static_cast<std::uint32_t>(c.hdr[2]) << 16) |
                            (static_cast<std::uint32_t>(c.hdr[3]) << 24);
                        if (c.len > (16u * 1024u * 1024u)) {
                            if (!c.delivered) { c.delivered = true; deliverResult(c.acceptSeq, false, std::string()); }
                            return true;
                        }
                        c.hasLen = true;
                        c.payload.resize(c.len);
                        if (c.len == 0) {
                            if (!c.delivered) { c.delivered = true; deliverResult(c.acceptSeq, true, std::string()); }
                            return true;
                        }
                    }
                } else {
                    const ssize_t n = ::recv(c.cfd, c.payload.data() + c.pGot, c.len - c.pGot, 0);
                    if (n < 0) {
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
                        if (!c.delivered) { c.delivered = true; deliverResult(c.acceptSeq, false, std::string()); }
                        return true;
                    }
                    if (n == 0) {
                        if (!c.delivered) { c.delivered = true; deliverResult(c.acceptSeq, false, std::string()); }
                        return true;
                    }
                    c.pGot += static_cast<std::size_t>(n);
                    if (c.pGot == c.len) {
                        if (!c.delivered) { c.delivered = true; deliverResult(c.acceptSeq, true, std::move(c.payload)); }
                        return true;
                    }
                }
            }
            if (!c.delivered) { c.delivered = true; deliverResult(c.acceptSeq, false, std::string()); }
            return true;
        };

        while (!implPtr->pipeStopRequested.load()) {
            std::vector<pollfd> pfds;
            pfds.reserve(2u + clients.size());
            pfds.push_back(pollfd{wakeReadFd, POLLIN, 0});
            pfds.push_back(pollfd{fd, POLLIN, 0});
            for (const auto& c : clients) {
                pfds.push_back(pollfd{c.cfd, POLLIN, 0});
            }

            const int pr = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), -1);
            if (pr < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (implPtr->pipeStopRequested.load() || (pfds[0].revents & POLLIN) != 0) {
                break;
            }

            // Pump existing clients first, removing finished ones
            std::size_t writeIdx = 0;
            for (std::size_t i = 0; i < clients.size(); ++i) {
                bool done = false;
                if ((pfds[2u + i].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) != 0) {
                    done = pumpClient(clients[i]);
                }
                if (done) {
                    ::close(clients[i].cfd);
                } else {
                    if (writeIdx != i) clients[writeIdx] = std::move(clients[i]);
                    ++writeIdx;
                }
            }
            clients.resize(writeIdx);

            if ((pfds[1].revents & POLLIN) != 0) {
                while (!implPtr->pipeStopRequested.load()) {
                    const int cfd = ::accept(fd, nullptr, nullptr);
                    if (cfd < 0) {
                        if (errno == EINTR) continue;
                        break;
                    }
                    setNonBlock(cfd);
                    ClientConn cc;
                    cc.cfd = cfd;
                    cc.acceptSeq = nextAcceptSeq++;
                    if (pumpClient(cc)) {
                        ::close(cfd);
                    } else {
                        clients.push_back(std::move(cc));
                    }
                }
            } else if ((pfds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                break;
            }
        }

        for (auto& c : clients) {
            if (c.cfd >= 0) ::close(c.cfd);
        }
        clients.clear();
    });
    return true;
#endif
}

bool CalcIpcChannel::SendNativePipeBytes(const std::string& payload, std::string* outErr) const
{
    if (!IsOpen()) {
        if (outErr) *outErr = "Canal IPC cerrado";
        return false;
    }
    if (payload.size() > (16u * 1024u * 1024u)) {
        if (outErr) *outErr = "Tamano de paquete IPC excede limite de 16 MiB";
        return false;
    }
    const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
    std::vector<std::uint8_t> pkt(4u + len);
    pkt[0] = static_cast<std::uint8_t>(len & 0xFFu);
    pkt[1] = static_cast<std::uint8_t>((len >> 8) & 0xFFu);
    pkt[2] = static_cast<std::uint8_t>((len >> 16) & 0xFFu);
    pkt[3] = static_cast<std::uint8_t>((len >> 24) & 0xFFu);
    if (len > 0) std::memcpy(pkt.data() + 4, payload.data(), len);

#ifdef _WIN32
    const std::wstring wName(impl_->nativePipeName.begin(), impl_->nativePipeName.end());
    HANDLE hClient = INVALID_HANDLE_VALUE;
    DWORD lastErr = 0;
    for (int attempt = 0; attempt < 50; ++attempt) {
        hClient = CreateFileW(
            wName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
        if (hClient != INVALID_HANDLE_VALUE) break;
        lastErr = GetLastError();
        if (lastErr == ERROR_PIPE_BUSY) {
            WaitNamedPipeW(wName.c_str(), 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(1 + (attempt % 4)));
            continue;
        }
        break;
    }
    if (hClient == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "CreateFileW pipe client fallo: " + std::to_string(lastErr);
        return false;
    }
    std::size_t totalWritten = 0;
    while (totalWritten < pkt.size()) {
        DWORD chunk = 0;
        const DWORD toWrite = static_cast<DWORD>(
            (std::min<std::size_t>)(65536u, pkt.size() - totalWritten));
        if (!WriteFile(hClient, pkt.data() + totalWritten, toWrite, &chunk, nullptr) || chunk == 0) {
            CloseHandle(hClient);
            if (outErr) *outErr = "WriteFile pipe client incompleto";
            return false;
        }
        totalWritten += chunk;
    }
    CloseHandle(hClient);
    return true;
#else
    if (impl_->nativePipeName.size() >= sizeof(sockaddr_un::sun_path)) {
        if (outErr) *outErr = "Ruta de socket AF_UNIX excede sun_path";
        return false;
    }
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        if (outErr) *outErr = "socket(AF_UNIX) client fallo";
        return false;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, impl_->nativePipeName.c_str(), impl_->nativePipeName.size() + 1);
    bool connected = false;
    int lastErrno = 0;
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0) {
            connected = true;
            break;
        }
        lastErrno = errno;
        if (lastErrno == EINTR || lastErrno == EAGAIN || lastErrno == EWOULDBLOCK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        break;
    }
    if (!connected) {
        if (outErr) *outErr = "connect(AF_UNIX) fallo: " + std::string(std::strerror(lastErrno));
        ::close(fd);
        return false;
    }
#ifdef MSG_NOSIGNAL
    const int sendFlags = MSG_NOSIGNAL;
#else
    const int sendFlags = 0;
#endif
    std::size_t sent = 0;
    while (sent < pkt.size()) {
        const ssize_t n = ::send(fd, pkt.data() + sent, pkt.size() - sent, sendFlags);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (outErr) *outErr = "send(AF_UNIX) fallo";
            ::close(fd);
            return false;
        }
        if (n == 0) {
            if (outErr) *outErr = "send(AF_UNIX) fallo";
            ::close(fd);
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    ::close(fd);
    return true;
#endif
}

bool CalcIpcChannel::RecvNativePipeBytes(std::string& outPayload, std::uint32_t timeoutMs, std::string* outErr)
{
    outPayload.clear();
    if (!IsOpen()) {
        if (outErr) *outErr = "Canal IPC cerrado";
        return false;
    }

    std::unique_lock<std::mutex> lk(impl_->pipeMutex);
    if (!impl_->pipeServerRunning && impl_->pipeQueue.empty()) {
        if (outErr) *outErr = "Named Pipe / AF_UNIX server no iniciado";
        return false;
    }
    const bool ready = impl_->pipeCv.wait_for(
        lk,
        std::chrono::milliseconds(timeoutMs),
        [this]() { return !impl_->pipeQueue.empty() || !impl_->pipeServerRunning; });
    if (!ready || impl_->pipeQueue.empty()) {
        if (outErr) {
            *outErr = impl_->pipeServerRunning
                ? "Timeout esperando datos en Named Pipe / AF_UNIX"
                : "Named Pipe / AF_UNIX cerrado";
        }
        return false;
    }
    outPayload = std::move(impl_->pipeQueue.front());
    impl_->pipeQueue.pop_front();
    return true;
}

// ---- 4. Differential Model Comparison (F-206, F-248) ----

ModelComparisonReport CompareCalculatorModels(
    const std::string& exprUtf8,
    const Cas::SymbolEnv& env,
    const std::vector<std::string>& modelIds)
{
    ModelComparisonReport rep;
    rep.expressionUtf8 = exprUtf8;
    if (exprUtf8.empty()) return rep;

    std::vector<std::string> targets = modelIds;
    if (targets.empty()) {
        targets = {"fx-570LA X", "fx-570VN PLUS", "fx-CG50", "fx-991CW"};
    }

    const bool isMatrix = (exprUtf8.find('[') != std::string::npos);
    const bool isSel = (!isMatrix && exprUtf8.find(';') != std::string::npos &&
                        exprUtf8.find('=') != std::string::npos);

    std::size_t selVars = 0;
    if (isMatrix) {
        const auto mres = Cas::EvaluateMatrixExpression(exprUtf8);
        rep.ok = mres.ok;
        rep.casReferenceResultUtf8 = mres.ok ? mres.formattedUtf8 : mres.errorUtf8;
        if (mres.ok && mres.isScalar) {
            rep.hasCasReferenceNumeric = true;
            rep.casReferenceNumeric = mres.scalarValue;
        }
    } else if (isSel) {
        const auto sres = Cas::SolveLinearSystemEquations(exprUtf8);
        rep.ok = sres.ok;
        rep.casReferenceResultUtf8 = sres.ok ? sres.summaryUtf8 : sres.errorUtf8;
        selVars = sres.varNames.size();
    } else {
        Cas::CasEvalOptions casOpts;
        casOpts.trigAngleMode = Cas::CasAngleMode::Deg;
        casOpts.polarAngleMode = Cas::CasAngleMode::Deg;
        const auto ev = Cas::EvaluateExpression(exprUtf8, env, casOpts);
        rep.ok = ev.ok;
        rep.casReferenceResultUtf8 = ev.ok ? ev.formattedResultUtf8 : ev.errorUtf8;
        if (ev.ok && ev.result.IsReal() && std::isfinite(ev.result.value.real())) {
            const double scale = (ev.result.unitScaleToSi != 0.0) ? ev.result.unitScaleToSi : 1.0;
            const double targetVal = (ev.result.value.real() - ev.result.unitOffsetToSi) / scale;
            if (std::isfinite(targetVal)) {
                rep.hasCasReferenceNumeric = true;
                rep.casReferenceNumeric = targetVal;
            }
        }
    }

    for (const auto& mid : targets) {
        const auto plan = TranslateExpressionToCasioKeys(exprUtf8, mid, env);
        ModelComparisonEntry entry;
        entry.model = plan.model;
        entry.modelIdUtf8 = CanonicalModelIdUtf8(plan.model);
        entry.appModeUtf8 = plan.appModeUtf8;
        entry.keySequenceUtf8 = plan.keySequenceUtf8;
        entry.steps = plan.steps;
        entry.displayDigits = 10;
        entry.internalDigits = 15;

        // Check hardware model limits
        const bool vnSelUnsupported = (isSel && entry.model == CasioModel::Fx570VnPlus && selVars > 3);
        const bool cwLaxSelUnsupported = (isSel && (entry.model == CasioModel::Fx570LaX || entry.model == CasioModel::Fx991Cw) && selVars > 4);
        const bool cgSelUnsupported = (isSel && entry.model == CasioModel::FxCg50 && selVars > 6);
        if (vnSelUnsupported || cwLaxSelUnsupported || cgSelUnsupported) {
            entry.supported = false;
            entry.displayResultUtf8 = vnSelUnsupported
                ? "No soportado (>3 incognitas)"
                : (cgSelUnsupported ? "No soportado (>6 incognitas)" : "No soportado (>4 incognitas)");
            entry.diffNoteUtf8 = vnSelUnsupported
                ? "fx-570VN PLUS limita EQN simultaneo a 2 o 3 incognitas."
                : "Numero de incognitas excede el limite de EQN simultaneo del modelo.";
            rep.allModelsSupported = false;
            rep.allResultsAgreeWithinTol = false;
        } else {
            entry.supported = plan.ok;
            if (!plan.ok) {
                rep.allModelsSupported = false;
            }
            if (rep.hasCasReferenceNumeric) {
                entry.hasNumericValue = true;
                // Round to 10 significant digits to mirror Casio 10-digit mantissa LCD
                const bool useComma = (entry.model == CasioModel::Fx570LaX);
                const std::string disp = FormatCasioDisplayValue(rep.casReferenceNumeric, useComma);
                entry.displayResultUtf8 = disp;
                std::string dotStr = disp;
                for (char& c : dotStr) if (c == ',') c = '.';
                try {
                    entry.numericValue = std::stod(dotStr);
                } catch (...) {
                    entry.numericValue = rep.casReferenceNumeric;
                }
                entry.deltaFromCas = std::abs(entry.numericValue - rep.casReferenceNumeric);
                rep.maxDeltaAcrossModels = (std::max)(rep.maxDeltaAcrossModels, entry.deltaFromCas);
                if (entry.deltaFromCas > 1e-7) {
                    rep.allResultsAgreeWithinTol = false;
                }
                entry.roundingNoteUtf8 = useComma
                    ? "10 cifras significativas (coma decimal LATAM)"
                    : "10 cifras significativas (punto decimal)";
            } else {
                entry.displayResultUtf8 = plan.evaluatedUtf8;
                entry.roundingNoteUtf8 = "Evaluacion simbolica/matricial";
                auto normStr = [](const std::string& s) {
                    std::string out;
                    out.reserve(s.size());
                    for (char c : s) {
                        if (!std::isspace(static_cast<unsigned char>(c))) out.push_back(c);
                    }
                    return out;
                };
                if (!entry.supported ||
                    normStr(entry.displayResultUtf8) != normStr(rep.casReferenceResultUtf8)) {
                    rep.allResultsAgreeWithinTol = false;
                }
            }
        }

        if (!plan.warningsUtf8.empty()) {
            if (!entry.diffNoteUtf8.empty()) entry.diffNoteUtf8 += " ";
            entry.diffNoteUtf8 += plan.warningsUtf8.front();
        }
        rep.models.push_back(std::move(entry));
    }

    for (std::size_t i = 1; i < rep.models.size(); ++i) {
        if (rep.models[i].keySequenceUtf8 != rep.models[0].keySequenceUtf8) {
            rep.hasKeySequenceDifferences = true;
            break;
        }
    }

    std::ostringstream sum;
    sum << "CAS=" << rep.casReferenceResultUtf8 << " | Modelos=" << rep.models.size()
        << " | TodosSoportados=" << (rep.allModelsSupported ? "SI" : "NO")
        << " | DeltaMax=" << rep.maxDeltaAcrossModels;
    rep.diffSummaryUtf8 = sum.str();
    return rep;
}

// ---- 5. KeyLog Presenter Controller Model (F-205, F-207) ----

KeyLogPresenterState BuildKeyLogPresenterFromSession(const CalcSessionState& session)
{
    KeyLogPresenterState st;
    st.calcSessionIdUtf8 = session.calcSessionIdUtf8;
    st.modelIdUtf8 = session.modelIdUtf8;
    st.steps = session.steps;
    st.frames = session.displayFrames;
    st.cursorIndex = 0;
    return st;
}

KeyLogPresenterState BuildKeyLogPresenterFromPlan(const CasioKeystrokePlan& plan)
{
    KeyLogPresenterState st;
    st.modelIdUtf8 = plan.modelIdUtf8;
    st.steps = plan.steps;
    st.cursorIndex = 0;
    return st;
}

bool StepPresenterFirst(KeyLogPresenterState& st) noexcept
{
    if (st.steps.empty()) return false;
    st.cursorIndex = 0;
    return true;
}

bool StepPresenterPrev(KeyLogPresenterState& st) noexcept
{
    if (st.steps.empty() || st.cursorIndex == 0) return false;
    --st.cursorIndex;
    return true;
}

bool StepPresenterNext(KeyLogPresenterState& st) noexcept
{
    if (st.steps.empty() || st.cursorIndex >= st.steps.size() - 1u) return false;
    ++st.cursorIndex;
    return true;
}

bool StepPresenterLast(KeyLogPresenterState& st) noexcept
{
    if (st.steps.empty()) return false;
    st.cursorIndex = st.steps.size() - 1u;
    return true;
}

bool JumpPresenterToStep(KeyLogPresenterState& st, std::size_t stepIdx) noexcept
{
    if (stepIdx >= st.steps.size()) return false;
    st.cursorIndex = stepIdx;
    return true;
}

const CasioKeyStep* CurrentPresenterStep(const KeyLogPresenterState& st) noexcept
{
    if (st.cursorIndex >= st.steps.size()) return nullptr;
    return &st.steps[st.cursorIndex];
}

const CalcDisplayFrame* CurrentPresenterFrame(const KeyLogPresenterState& st) noexcept
{
    if (st.frames.empty()) return nullptr;
    if (st.cursorIndex < st.frames.size()) return &st.frames[st.cursorIndex];
    return &st.frames.back();
}

std::string FormatPresenterStepCalloutUtf8(const KeyLogPresenterState& st)
{
    const CasioKeyStep* step = CurrentPresenterStep(st);
    if (!step) return {};
    std::ostringstream oss;
    oss << "[" << (st.cursorIndex + 1) << "/" << st.steps.size() << "] "
        << step->badge.badgeTextUtf8 << " " << step->keyNameUtf8
        << " (" << step->displayCaptionUtf8 << ")";
    if (!st.presenterCleanMode && !step->noteUtf8.empty()) {
        oss << " — " << step->noteUtf8;
    }
    return oss.str();
}

} // namespace DocStudio::CalcBridge
