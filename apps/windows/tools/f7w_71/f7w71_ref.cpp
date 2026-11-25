#pragma warning(disable: 4191)

#include <windows.h>
#include <winver.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cwctype>
#include <string>
#include <vector>

typedef void (__thiscall *fn_ctor)(void *);
typedef void (__thiscall *fn_dtor)(void *);
typedef int (__thiscall *fn_u8)(void *, unsigned char);
typedef int (__thiscall *fn_u16)(void *, unsigned short);
typedef int (__thiscall *fn_u32u32)(void *, unsigned int, unsigned int);
typedef int (__thiscall *fn_write)(void *, unsigned int, unsigned int, unsigned char *);
typedef int (__thiscall *fn_void)(void *);
typedef int (__thiscall *fn_getcount)(void *, unsigned int *);
typedef unsigned char (__thiscall *fn_readreg)(void *, unsigned char, unsigned int *);
typedef int (__thiscall *fn_readmem)(void *, unsigned int, unsigned int, unsigned char *);
typedef int (__thiscall *fn_writemem)(void *, unsigned int, unsigned int, unsigned char *);

static const unsigned int kCoreSize = 0x8000;
static const unsigned int kGuardSize = 0x1000;
static const unsigned int kLcdWindow = 0x800;

class Sha256 {
public:
    Sha256() { reset(); }

    void reset() {
        h_[0] = 0x6a09e667u; h_[1] = 0xbb67ae85u; h_[2] = 0x3c6ef372u; h_[3] = 0xa54ff53au;
        h_[4] = 0x510e527fu; h_[5] = 0x9b05688cu; h_[6] = 0x1f83d9abu; h_[7] = 0x5be0cd19u;
        len_ = 0;
        idx_ = 0;
    }

    void update(const void *data, size_t n) {
        if (n == 0) return;
        const unsigned char *p = static_cast<const unsigned char *>(data);
        len_ += static_cast<uint64_t>(n);
        while (n > 0) {
            size_t take = 64 - idx_;
            if (take > n) take = n;
            std::memcpy(buf_ + idx_, p, take);
            idx_ += take;
            p += take;
            n -= take;
            if (idx_ == 64) {
                transform(buf_);
                idx_ = 0;
            }
        }
    }

    void finish(unsigned char out[32]) {
        uint64_t bits = len_ * 8;
        unsigned char one = 0x80;
        unsigned char zero = 0;
        update(&one, 1);
        while (idx_ != 56) update(&zero, 1);
        unsigned char lenbuf[8];
        for (int i = 0; i < 8; ++i) lenbuf[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        update(lenbuf, 8);
        for (int i = 0; i < 8; ++i) {
            out[4 * i + 0] = static_cast<unsigned char>(h_[i] >> 24);
            out[4 * i + 1] = static_cast<unsigned char>(h_[i] >> 16);
            out[4 * i + 2] = static_cast<unsigned char>(h_[i] >> 8);
            out[4 * i + 3] = static_cast<unsigned char>(h_[i]);
        }
    }

private:
    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ ((~x) & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t bs0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static uint32_t bs1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static uint32_t ss0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static uint32_t ss1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform(const unsigned char *p) {
        static const uint32_t k[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
        };
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(p[4 * i]) << 24) |
                   (static_cast<uint32_t>(p[4 * i + 1]) << 16) |
                   (static_cast<uint32_t>(p[4 * i + 2]) << 8) |
                   static_cast<uint32_t>(p[4 * i + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = w[i - 16] + ss0(w[i - 15]) + w[i - 7] + ss1(w[i - 2]);
        }
        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
        uint32_t e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + bs1(e) + ch(e, f, g) + k[i] + w[i];
            uint32_t t2 = bs0(a) + maj(a, b, c);
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
        h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
    }

    uint32_t h_[8];
    uint64_t len_;
    unsigned char buf_[64];
    size_t idx_;
};

struct ModelSpec {
    const char *name;
    unsigned int codeStart;
    unsigned int codeEnd;
    unsigned int dataStart;
    unsigned int dataEnd;
    unsigned int windowStart;
    unsigned int windowEnd;
    unsigned int pdValue;
    unsigned int ioBase;
    unsigned int lcdHashBytes;
};

static const ModelSpec kModelLax = { "lax", 0, 0x3FFFF, 0xD000, 0x8FFFF, 0, 0xCFFF, 0x00, 0x48E00, 2048 };
static const ModelSpec kModelVn = { "vn", 0, 0x2FFFF, 0x8000, 0x8FFFF, 0, 0x7FFF, 0x08, 0x8E00, 512 };

struct Candidate {
    std::wstring path;
    bool exists;
};

struct KeyDef {
    unsigned int code;
    const char *name;
};

static const KeyDef kKnownKeys[] = {
    { 0x00, "1" }, { 0x10, "2" }, { 0x20, "3" }, { 0x01, "4" }, { 0x11, "5" },
    { 0x21, "6" }, { 0x02, "7" }, { 0x12, "8" }, { 0x22, "9" }, { 0x64, "0" },
    { 0x30, "+" }, { 0x40, "-" }, { 0x31, "*" }, { 0x41, "/" }, { 0x60, "=" },
    { 0x32, "DEL" }, { 0x42, "AC" }, { 0x63, "." }, { 0x62, "EXP" }, { 0x61, "ANS" },
    { 0x07, "SHIFT" }, { 0x17, "ALPHA" }, { 0x47, "MENU" }, { 0x46, "INTEGRAL" },
    { 0x56, "VARIABLE" }, { 0x27, "UP" }, { 0x26, "LEFT" }, { 0x37, "RIGHT" },
    { 0x36, "DOWN" }, { 0xFF, "ON" }
};

struct Options {
    std::wstring firmware;
    std::wstring out;
    std::wstring dll;
    std::wstring lcdOut;
    std::wstring lcdDir;
    std::string model;
    std::vector<unsigned int> keys;
    unsigned int steps;
    unsigned int hold;
    unsigned int settle;
    unsigned int poll;
    bool stepsSet;
    bool keyOptionSeen;
    bool keysOptionSeen;
    bool singleKey;
    Options()
        : model("auto"), steps(100), hold(5000), settle(150000), poll(256),
          stepsSet(false), keyOptionSeen(false), keysOptionSeen(false), singleKey(false) {}
};

struct CpuSnapshot {
    unsigned int icount;
    unsigned int pc;
    unsigned int sp;
    bool havePc;
    bool haveSp;
    unsigned int regs[0x30];
    bool regOk[0x30];
};

struct StepResult {
    unsigned int index;
    unsigned int code;
    std::string label;
    bool injected;
    unsigned int waitSteps;
    unsigned int injectedAtInstruction;
    unsigned int holdSteps;
    unsigned int settleSteps;
    unsigned int executedTotal;
    unsigned char cmdAtStart;
    unsigned char cmdAtInjection;
    unsigned char cmdAfterHold;
    unsigned char cmdAfterRelease;
    unsigned char cmdAfterSettle;
    unsigned char kiInjected;
    unsigned char koInjected;
    unsigned char kiAfterSettle;
    unsigned char koAfterSettle;
    int execReturn;
    int lcdReadReturn;
    unsigned int lcdNonZero;
    std::string lcdHash;
    std::wstring lcdFile;
    CpuSnapshot snap;
};

static std::string toUtf8(const std::wstring &w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), NULL, 0, NULL, NULL);
    if (n <= 0) return std::string();
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &s[0], n, NULL, NULL);
    return s;
}

static std::string jsonEscape(const std::string &s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\b': r += "\\b"; break;
        case '\f': r += "\\f"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                std::snprintf(b, sizeof(b), "\\u%04x", static_cast<unsigned int>(c));
                r += b;
            } else {
                r += static_cast<char>(c);
            }
            break;
        }
    }
    return r;
}

static std::string jsonWide(const std::wstring &w) {
    return "\"" + jsonEscape(toUtf8(w)) + "\"";
}

static std::string jsonText(const std::string &s) {
    return "\"" + jsonEscape(s) + "\"";
}

static std::string hexLower(const unsigned char *p, size_t n) {
    static const char *d = "0123456789abcdef";
    std::string s;
    s.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s[2 * i] = d[p[i] >> 4];
        s[2 * i + 1] = d[p[i] & 0x0F];
    }
    return s;
}

static std::string hex32(unsigned int v) {
    char b[16];
    std::snprintf(b, sizeof(b), "0x%08X", v);
    return b;
}

static std::string hex8(unsigned int v) {
    char b[8];
    std::snprintf(b, sizeof(b), "0x%02X", v);
    return b;
}

static std::string keyLabel(unsigned int code) {
    for (size_t i = 0; i < sizeof(kKnownKeys) / sizeof(kKnownKeys[0]); ++i) {
        if (kKnownKeys[i].code == code) return kKnownKeys[i].name;
    }
    char b[16];
    std::snprintf(b, sizeof(b), "key_%02X", code);
    return b;
}

static std::string utcNow() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    char b[32];
    std::snprintf(b, sizeof(b), "%04u-%02u-%02uT%02u:%02u:%02uZ",
                  static_cast<unsigned int>(st.wYear), static_cast<unsigned int>(st.wMonth),
                  static_cast<unsigned int>(st.wDay), static_cast<unsigned int>(st.wHour),
                  static_cast<unsigned int>(st.wMinute), static_cast<unsigned int>(st.wSecond));
    return b;
}

static std::wstring exeDir() {
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        DWORD n = GetModuleFileNameW(NULL, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return L".";
        if (n + 1 < buf.size()) {
            std::wstring p(buf.data(), n);
            size_t pos = p.find_last_of(L"\\/");
            return pos == std::wstring::npos ? L"." : p.substr(0, pos);
        }
        buf.resize(buf.size() * 2);
    }
}

static std::wstring currentDir() {
    DWORD n = GetCurrentDirectoryW(0, NULL);
    if (n == 0) return L".";
    std::vector<wchar_t> buf(n);
    DWORD m = GetCurrentDirectoryW(n, buf.data());
    return std::wstring(buf.data(), m);
}

static std::wstring parentDir(const std::wstring &p) {
    size_t pos = p.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return p;
    if (pos == 2 && p.size() >= 3 && p[1] == L':') return p.substr(0, 3);
    return p.substr(0, pos);
}

static std::wstring absolutePath(const std::wstring &p) {
    DWORD n = GetFullPathNameW(p.c_str(), 0, NULL, NULL);
    if (n == 0) return p;
    std::vector<wchar_t> buf(n);
    DWORD m = GetFullPathNameW(p.c_str(), n, buf.data(), NULL);
    if (m == 0) return p;
    return std::wstring(buf.data(), m);
}

static bool fileExists(const std::wstring &p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool getFileSize(const std::wstring &p, unsigned long long &size) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fad)) return false;
    size = (static_cast<unsigned long long>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
    return true;
}

static std::string fileVersionString(const std::wstring &p) {
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(p.c_str(), &handle);
    if (size == 0) return "unknown";
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(p.c_str(), 0, size, data.data())) return "unknown";
    VS_FIXEDFILEINFO *info = NULL;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID *>(&info), &len) || !info) return "unknown";
    char b[64];
    std::snprintf(b, sizeof(b), "%u.%u.%u.%u",
                  static_cast<unsigned int>(info->dwFileVersionMS >> 16),
                  static_cast<unsigned int>(info->dwFileVersionMS & 0xFFFF),
                  static_cast<unsigned int>(info->dwFileVersionLS >> 16),
                  static_cast<unsigned int>(info->dwFileVersionLS & 0xFFFF));
    return b;
}

static bool readFile(const std::wstring &p, std::vector<unsigned char> &out, std::string &err) {
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        err = "CreateFileW failed";
        return false;
    }
    LARGE_INTEGER li;
    if (!GetFileSizeEx(h, &li)) {
        CloseHandle(h);
        err = "GetFileSizeEx failed";
        return false;
    }
    if (li.QuadPart < 0) {
        CloseHandle(h);
        err = "negative file size";
        return false;
    }
    out.resize(static_cast<size_t>(li.QuadPart));
    size_t total = 0;
    while (total < out.size()) {
        DWORD chunk = static_cast<DWORD>(out.size() - total > 0x100000 ? 0x100000 : out.size() - total);
        DWORD got = 0;
        if (!ReadFile(h, out.data() + total, chunk, &got, NULL)) {
            CloseHandle(h);
            err = "ReadFile failed";
            return false;
        }
        if (got == 0) break;
        total += got;
    }
    CloseHandle(h);
    if (total != out.size()) {
        out.resize(total);
        err = "short read";
        return false;
    }
    return true;
}

static std::string sha256Bytes(const unsigned char *p, size_t n) {
    Sha256 s;
    s.update(p, n);
    unsigned char d[32];
    s.finish(d);
    return hexLower(d, sizeof(d));
}

static std::string sha256File(const std::wstring &p, bool &ok) {
    std::vector<unsigned char> data;
    std::string err;
    ok = readFile(p, data, err);
    if (!ok) return std::string();
    return sha256Bytes(data.empty() ? NULL : data.data(), data.size());
}

static void ensureParentDir(const std::wstring &p) {
    size_t pos = p.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return;
    std::wstring dir = p.substr(0, pos);
    if (dir.empty()) return;
    std::wstring cur;
    for (size_t i = 0; i < dir.size(); ++i) {
        cur += dir[i];
        bool last = (i + 1 == dir.size());
        if (dir[i] == L'\\' || dir[i] == L'/' || last) {
            if (cur.size() == 2 && cur[1] == L':') continue;
            if (cur.size() == 3 && cur[1] == L':' && (cur[2] == L'\\' || cur[2] == L'/')) continue;
            CreateDirectoryW(cur.c_str(), NULL);
        }
    }
}

static bool writeText(const std::wstring &p, const std::string &text) {
    ensureParentDir(p);
    FILE *f = NULL;
    if (_wfopen_s(&f, p.c_str(), L"wb") != 0 || !f) return false;
    size_t n = text.empty() ? 0 : fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return n == text.size();
}

static bool writeBytes(const std::wstring &p, const unsigned char *data, size_t size) {
    ensureParentDir(p);
    FILE *f = NULL;
    if (_wfopen_s(&f, p.c_str(), L"wb") != 0 || !f) return false;
    size_t n = size == 0 ? 0 : fwrite(data, 1, size, f);
    fclose(f);
    return n == size;
}

template <typename T>
static T getProc(HMODULE dll, const char *name) {
    return reinterpret_cast<T>(GetProcAddress(dll, name));
}

static std::vector<Candidate> dllCandidates(const std::wstring &explicitPath) {
    std::vector<std::wstring> paths;
    if (!explicitPath.empty()) {
        paths.push_back(absolutePath(explicitPath));
    } else {
        wchar_t buf[32768];
        DWORD n = GetEnvironmentVariableW(L"F7W71_SIMU8ENGINE", buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
        if (n > 0 && n < sizeof(buf) / sizeof(buf[0])) paths.push_back(absolutePath(buf));
        n = GetEnvironmentVariableW(L"SIMU8_DIR", buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
        if (n > 0 && n < sizeof(buf) / sizeof(buf[0])) {
            std::wstring dir(buf, n);
            paths.push_back(absolutePath(dir + L"\\SimU8engine.dll"));
        }
        std::wstring root = parentDir(parentDir(parentDir(exeDir())));
        std::wstring cwd = currentDir();
        paths.push_back(absolutePath(root + L"\\build-verify\\Release\\runtime\\SimU8engine.dll"));
        paths.push_back(absolutePath(root + L"\\..\\Calculadora blanca\\SimU8engine.dll"));
        paths.push_back(absolutePath(root + L"\\..\\Calculadora fx 570 es\\SimU8engine.dll"));
        paths.push_back(absolutePath(cwd + L"\\SimU8engine.dll"));
        paths.push_back(absolutePath(cwd + L"\\build-verify\\Release\\runtime\\SimU8engine.dll"));
        paths.push_back(absolutePath(cwd + L"\\..\\Calculadora blanca\\SimU8engine.dll"));
        paths.push_back(absolutePath(cwd + L"\\..\\Calculadora fx 570 es\\SimU8engine.dll"));
    }
    std::vector<Candidate> out;
    for (size_t i = 0; i < paths.size(); ++i) {
        Candidate c;
        c.path = paths[i];
        c.exists = fileExists(paths[i]);
        out.push_back(c);
        if (c.exists) break;
    }
    return out;
}

static const ModelSpec *pickModel(const std::wstring &firmware, const std::string &requested, std::string &why) {
    if (requested == "lax") return &kModelLax;
    if (requested == "vn") return &kModelVn;
    if (requested != "auto") {
        why = "invalid --model (expected auto, lax or vn)";
        return NULL;
    }
    std::wstring lower = firmware;
    for (size_t i = 0; i < lower.size(); ++i) lower[i] = static_cast<wchar_t>(std::towlower(lower[i]));
    if (lower.find(L"570vn") != std::wstring::npos || lower.find(L"vnplus") != std::wstring::npos) return &kModelVn;
    if (lower.find(L"991lax") != std::wstring::npos || lower.find(L"570la") != std::wstring::npos) return &kModelLax;
    unsigned long long size = 0;
    if (getFileSize(firmware, size)) {
        if (size == 131072ULL) return &kModelVn;
        if (size == 262144ULL) return &kModelLax;
    }
    why = "cannot autodetect model from firmware name/size; pass --model lax or --model vn";
    return NULL;
}

static bool parseUnsigned(const wchar_t *s, unsigned int &value) {
    wchar_t *end = NULL;
    unsigned long v = wcstoul(s, &end, 0);
    if (end == s || *end != L'\0' || v > 0xFFFFFFFFUL) return false;
    value = static_cast<unsigned int>(v);
    return true;
}

static bool parseKeyList(const std::wstring &text, std::vector<unsigned int> &keys, std::string &err) {
    size_t start = 0;
    for (;;) {
        size_t comma = text.find(L',', start);
        std::wstring token = (comma == std::wstring::npos) ? text.substr(start) : text.substr(start, comma - start);
        size_t b = 0, e = token.size();
        while (b < e && std::iswspace(token[b]) != 0) ++b;
        while (e > b && std::iswspace(token[e - 1]) != 0) --e;
        token = token.substr(b, e - b);
        if (!token.empty()) {
            wchar_t *end = NULL;
            unsigned long v = wcstoul(token.c_str(), &end, 16);
            if (end == token.c_str() || *end != L'\0' || v > 0xFF) {
                err = "invalid key in --keys: " + toUtf8(token);
                return false;
            }
            keys.push_back(static_cast<unsigned int>(v));
        }
        if (comma == std::wstring::npos) break;
        start = comma + 1;
    }
    if (keys.empty()) { err = "--keys produced an empty list"; return false; }
    if (keys.size() > 64) { err = "too many keys in --keys (max 64)"; return false; }
    return true;
}

static bool parseOptions(int argc, wchar_t **argv, Options &o, std::string &err) {
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--firmware" && i + 1 < argc) {
            o.firmware = argv[++i];
        } else if (a == L"--steps" && i + 1 < argc) {
            if (!parseUnsigned(argv[++i], o.steps) || o.steps == 0) { err = "invalid --steps"; return false; }
            o.stepsSet = true;
        } else if (a == L"--out" && i + 1 < argc) {
            o.out = argv[++i];
        } else if (a == L"--dll" && i + 1 < argc) {
            o.dll = argv[++i];
        } else if (a == L"--lcd-out" && i + 1 < argc) {
            o.lcdOut = argv[++i];
        } else if (a == L"--lcd-dir" && i + 1 < argc) {
            o.lcdDir = argv[++i];
        } else if (a == L"--model" && i + 1 < argc) {
            std::wstring m = argv[++i];
            o.model = toUtf8(m);
        } else if (a == L"--key" && i + 1 < argc) {
            wchar_t *end = NULL;
            unsigned long v = wcstoul(argv[++i], &end, 16);
            if (end == argv[i] || *end != L'\0' || v > 0xFF) { err = "invalid --key (expected 00..FF hex)"; return false; }
            o.keys.clear();
            o.keys.push_back(static_cast<unsigned int>(v));
            o.keyOptionSeen = true;
            o.singleKey = true;
        } else if (a == L"--keys" && i + 1 < argc) {
            if (o.keysOptionSeen) { err = "--keys specified more than once"; return false; }
            o.keys.clear();
            if (!parseKeyList(argv[++i], o.keys, err)) return false;
            o.keysOptionSeen = true;
            o.singleKey = false;
        } else if (a == L"--hold" && i + 1 < argc) {
            if (!parseUnsigned(argv[++i], o.hold)) { err = "invalid --hold"; return false; }
        } else if (a == L"--settle" && i + 1 < argc) {
            if (!parseUnsigned(argv[++i], o.settle)) { err = "invalid --settle"; return false; }
        } else if (a == L"--poll" && i + 1 < argc) {
            if (!parseUnsigned(argv[++i], o.poll) || o.poll == 0) { err = "invalid --poll"; return false; }
        } else if (a == L"--help" || a == L"-h") {
            err = "help";
            return false;
        } else {
            err = "unknown or incomplete argument: " + toUtf8(a);
            return false;
        }
    }
    if (o.firmware.empty()) { err = "--firmware is required"; return false; }
    if (o.keyOptionSeen && o.keysOptionSeen) { err = "--key and --keys are mutually exclusive"; return false; }
    if (o.out.empty()) o.out = L"f7w71_result.json";
    if (!o.keys.empty() && !o.stepsSet) o.steps = 5000000;
    return true;
}

static void printUsage() {
    std::fprintf(stderr,
        "usage: f7w71_ref.exe --firmware <rom> [--steps N] [--out file.json]\n"
        "                      [--model auto|lax|vn] [--dll path]\n"
        "                      [--key XX | --keys XX,XX,...]\n"
        "                      [--hold N] [--settle N] [--poll N]\n"
        "                      [--lcd-out file.bin] [--lcd-dir dir]\n"
        "\n"
        "  --key XX         deliver one matrix key (hex 00..FF)\n"
        "  --keys a,b,c     deliver a sequence; --steps is the per-key wait budget\n"
        "  --lcd-dir DIR    write the LCD dump after every delivered key\n");
}

static void takeSnapshot(void *core, fn_getcount getCount, fn_readreg readReg, CpuSnapshot &s) {
    s.icount = 0;
    s.pc = 0;
    s.sp = 0;
    s.havePc = false;
    s.haveSp = false;
    if (getCount) getCount(core, &s.icount);
    if (readReg) {
        s.havePc = readReg(core, 0x10, &s.pc) == 0;
        s.haveSp = readReg(core, 0x1E, &s.sp) == 0;
    }
    for (unsigned int i = 0; i < 0x30; ++i) {
        unsigned int v = 0;
        s.regOk[i] = readReg ? (readReg(core, static_cast<unsigned char>(i), &v) == 0) : false;
        s.regs[i] = v;
    }
}

static std::string snapshotJson(const CpuSnapshot &s) {
    std::string j = "{";
    j += "\"icount\": " + std::to_string(s.icount);
    j += ", \"pc\": " + (s.havePc ? jsonText(hex32(s.pc)) : std::string("null"));
    j += ", \"sp\": " + (s.haveSp ? jsonText(hex32(s.sp)) : std::string("null"));
    j += "}";
    return j;
}

static std::string registersJson(const CpuSnapshot &s) {
    std::string j = "{\n";
    bool first = true;
    for (unsigned int i = 0; i < 0x30; ++i) {
        if (!s.regOk[i]) continue;
        if (!first) j += ",\n";
        first = false;
        j += "    " + jsonText(hex8(i)) + ": " + jsonText(hex32(s.regs[i]));
    }
    j += "\n  }";
    return j;
}

static std::string registersJsonCompact(const CpuSnapshot &s) {
    std::string j = "{";
    bool first = true;
    for (unsigned int i = 0; i < 0x30; ++i) {
        if (!s.regOk[i]) continue;
        if (!first) j += ", ";
        first = false;
        j += jsonText(hex8(i)) + ": " + jsonText(hex32(s.regs[i]));
    }
    j += "}";
    return j;
}

static std::string searchJson(const std::vector<Candidate> &search) {
    std::string j = "[\n";
    for (size_t i = 0; i < search.size(); ++i) {
        j += "    {\"path\": " + jsonWide(search[i].path) + ", \"exists\": " + (search[i].exists ? "true" : "false") + "}";
        if (i + 1 < search.size()) j += ",";
        j += "\n";
    }
    j += "  ]";
    return j;
}

static std::string codesJson(const std::vector<unsigned int> &keys) {
    std::string j = "[";
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) j += ", ";
        j += jsonText(hex8(keys[i]));
    }
    j += "]";
    return j;
}

static std::string labelsJson(const std::vector<unsigned int> &keys) {
    std::string j = "[";
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) j += ", ";
        j += jsonText(keyLabel(keys[i]));
    }
    j += "]";
    return j;
}

static int writeError(const Options &o, const std::string &message, const std::vector<Candidate> &search, int code) {
    std::string j = "{\n";
    j += "  \"schema\": \"f7w71-s1/1\",\n";
    j += "  \"status\": \"error\",\n";
    j += "  \"generated_utc\": " + jsonText(utcNow()) + ",\n";
    j += "  \"error\": " + jsonText(message) + ",\n";
    j += "  \"dll_search\": " + searchJson(search) + "\n";
    j += "}\n";
    writeText(o.out, j);
    std::fprintf(stderr, "ERROR: %s\n", message.c_str());
    return code;
}

static unsigned char readByte(void *core, fn_readmem readMem, unsigned int addr) {
    unsigned char v = 0;
    if (readMem) readMem(core, addr, 1, &v);
    return v;
}

static int readLcdSnapshot(void *core, fn_readmem readMem, const ModelSpec *model,
                           std::vector<unsigned char> &lcd, std::string &hash, unsigned int &nonZero) {
    lcd.assign(kLcdWindow, 0);
    int ret = readMem(core, 0xF800, kLcdWindow, lcd.data());
    hash = sha256Bytes(lcd.data(), model->lcdHashBytes);
    nonZero = 0;
    for (unsigned int i = 0; i < model->lcdHashBytes; ++i) {
        if (lcd[i] != 0) ++nonZero;
    }
    return ret;
}

static std::wstring stepLcdPath(const Options &o, const ModelSpec *model, const StepResult &st) {
    if (o.lcdDir.empty()) return std::wstring();
    char name[96];
    std::snprintf(name, sizeof(name), "lcd_%s_step%u_%02X.bin",
                  model->name, st.index, st.code & 0xFF);
    std::wstring p = o.lcdDir;
    if (!p.empty() && p[p.size() - 1] != L'\\' && p[p.size() - 1] != L'/') p += L'\\';
    p += std::wstring(name, name + std::strlen(name));
    return absolutePath(p);
}

static std::string stepJson(const StepResult &st) {
    std::string j = "    {\n";
    j += "      \"step\": " + std::to_string(st.index) + ",\n";
    j += "      \"key\": {\"code\": " + jsonText(hex8(st.code)) + ", \"label\": " + jsonText(st.label) + "},\n";
    j += "      \"injected\": " + std::string(st.injected ? "true" : "false") + ",\n";
    j += "      \"wait_steps\": " + std::to_string(st.waitSteps) + ",\n";
    j += "      \"injected_at_instruction\": ";
    if (st.injected) j += std::to_string(st.injectedAtInstruction);
    else j += "null";
    j += ",\n";
    j += "      \"hold_steps\": " + std::to_string(st.holdSteps) + ",\n";
    j += "      \"settle_steps\": " + std::to_string(st.settleSteps) + ",\n";
    j += "      \"executed_total\": " + std::to_string(st.executedTotal) + ",\n";
    j += "      \"exec_return\": " + std::to_string(st.execReturn) + ",\n";
    j += "      \"io\": {\n";
    j += "        \"cmd_at_start\": " + jsonText(hex8(st.cmdAtStart)) + ",\n";
    j += "        \"cmd_at_injection\": " + jsonText(hex8(st.cmdAtInjection)) + ",\n";
    j += "        \"cmd_after_hold\": " + jsonText(hex8(st.cmdAfterHold)) + ",\n";
    j += "        \"cmd_after_release\": " + jsonText(hex8(st.cmdAfterRelease)) + ",\n";
    j += "        \"cmd_after_settle\": " + jsonText(hex8(st.cmdAfterSettle)) + ",\n";
    j += "        \"ki_injected\": " + jsonText(hex8(st.kiInjected)) + ",\n";
    j += "        \"ko_injected\": " + jsonText(hex8(st.koInjected)) + ",\n";
    j += "        \"ki_after_settle\": " + jsonText(hex8(st.kiAfterSettle)) + ",\n";
    j += "        \"ko_after_settle\": " + jsonText(hex8(st.koAfterSettle)) + "\n";
    j += "      },\n";
    j += "      \"cpu\": " + snapshotJson(st.snap) + ",\n";
    j += "      \"registers\": " + registersJsonCompact(st.snap) + ",\n";
    j += "      \"lcd\": {\n";
    j += "        \"read_return\": " + std::to_string(st.lcdReadReturn) + ",\n";
    j += "        \"sha256\": " + jsonText(st.lcdHash) + ",\n";
    j += "        \"non_zero_bytes\": " + std::to_string(st.lcdNonZero) + ",\n";
    j += "        \"dump_file\": " + (st.lcdFile.empty() ? std::string("null") : jsonWide(st.lcdFile)) + "\n";
    j += "      }\n";
    j += "    }";
    return j;
}

int wmain(int argc, wchar_t **argv) {
    Options o;
    std::string parseErr;
    if (!parseOptions(argc, argv, o, parseErr)) {
        if (parseErr != "help") std::fprintf(stderr, "ERROR: %s\n", parseErr.c_str());
        printUsage();
        return 2;
    }

    std::vector<Candidate> search = dllCandidates(o.dll);
    std::wstring dllPath;
    for (size_t i = 0; i < search.size(); ++i) {
        if (search[i].exists) { dllPath = search[i].path; break; }
    }
    if (dllPath.empty()) {
        return writeError(o, "SimU8engine.dll not found in any search location", search, 3);
    }

    std::vector<unsigned char> firmware;
    std::string readErr;
    if (!readFile(o.firmware, firmware, readErr)) {
        return writeError(o, "cannot read firmware: " + readErr + " (" + toUtf8(o.firmware) + ")", search, 4);
    }

    std::string modelWhy;
    const ModelSpec *model = pickModel(o.firmware, o.model, modelWhy);
    if (!model) {
        return writeError(o, modelWhy, search, 5);
    }

    HMODULE dll = LoadLibraryExW(dllPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) {
        char b[128];
        std::snprintf(b, sizeof(b), "LoadLibraryExW failed with GetLastError=%lu", static_cast<unsigned long>(GetLastError()));
        return writeError(o, b, search, 6);
    }

    std::vector<std::string> missing;
    fn_ctor ctor = getProc<fn_ctor>(dll, "??0CSimU8core@@QAE@XZ");
    fn_dtor dtor = getProc<fn_dtor>(dll, "??1CSimU8core@@UAE@XZ");
    fn_u8 setModel = getProc<fn_u8>(dll, "?m_SetMemoryModel@CSimU8core@@QAEHE@Z");
    fn_u32u32 setCode = getProc<fn_u32u32>(dll, "?m_SetCodeMemorySize@CSimU8core@@QAEHII@Z");
    fn_u32u32 setData = getProc<fn_u32u32>(dll, "?m_SetDataMemorySize@CSimU8core@@QAEHII@Z");
    fn_u32u32 setWindow = getProc<fn_u32u32>(dll, "?m_SetRomWindowSize@CSimU8core@@QAEHII@Z");
    fn_u16 setDefault = getProc<fn_u16>(dll, "?m_SetCodeMemoryDefaultCode@CSimU8core@@QAEHG@Z");
    fn_write writeCode = getProc<fn_write>(dll, "?m_WriteCodeMemory@CSimU8core@@QAEHIIPAE@Z");
    fn_void simReset = getProc<fn_void>(dll, "?m_SimReset@CSimU8core@@QAEHXZ");
    fn_void exec = getProc<fn_void>(dll, "?m_Execute@CSimU8core@@QAEHXZ");
    fn_getcount getCount = getProc<fn_getcount>(dll, "?m_GetCount@CSimU8core@@QAEHPAI@Z");
    fn_readreg readReg = getProc<fn_readreg>(dll, "?m_ReadReg@CSimU8core@@QAEHEPAI@Z");
    fn_readmem readMem = getProc<fn_readmem>(dll, "?m_ReadDataMemory@CSimU8core@@QAEHIIPAE@Z");
    fn_writemem writeMem = getProc<fn_writemem>(dll, "?m_WriteDataMemory@CSimU8core@@QAEHIIPAE@Z");

    if (!ctor) missing.push_back("??0CSimU8core@@QAE@XZ");
    if (!setModel) missing.push_back("?m_SetMemoryModel@CSimU8core@@QAEHE@Z");
    if (!setCode) missing.push_back("?m_SetCodeMemorySize@CSimU8core@@QAEHII@Z");
    if (!setData) missing.push_back("?m_SetDataMemorySize@CSimU8core@@QAEHII@Z");
    if (!setWindow) missing.push_back("?m_SetRomWindowSize@CSimU8core@@QAEHII@Z");
    if (!setDefault) missing.push_back("?m_SetCodeMemoryDefaultCode@CSimU8core@@QAEHG@Z");
    if (!writeCode) missing.push_back("?m_WriteCodeMemory@CSimU8core@@QAEHIIPAE@Z");
    if (!simReset) missing.push_back("?m_SimReset@CSimU8core@@QAEHXZ");
    if (!exec) missing.push_back("?m_Execute@CSimU8core@@QAEHXZ");
    if (!getCount) missing.push_back("?m_GetCount@CSimU8core@@QAEHPAI@Z");
    if (!readReg) missing.push_back("?m_ReadReg@CSimU8core@@QAEHEPAI@Z");
    if (!readMem) missing.push_back("?m_ReadDataMemory@CSimU8core@@QAEHIIPAE@Z");
    if (!writeMem) missing.push_back("?m_WriteDataMemory@CSimU8core@@QAEHIIPAE@Z");

    if (!missing.empty()) {
        std::string m = "missing required exports:";
        for (size_t i = 0; i < missing.size(); ++i) m += " " + missing[i];
        FreeLibrary(dll);
        return writeError(o, m, search, 7);
    }

    std::vector<unsigned char> storage(kGuardSize + kCoreSize + kGuardSize, 0xCD);
    unsigned char *core = storage.data() + kGuardSize;
    std::memset(core, 0, kCoreSize);
    ctor(core);

    int initModel = setModel(core, 1);
    int initCode = setCode(core, model->codeStart, model->codeEnd);
    int initData = setData(core, model->dataStart, model->dataEnd);
    int initWindow = setWindow(core, model->windowStart, model->windowEnd);
    int initDefault = setDefault(core, 0);
    int romWrite = writeCode(core, 0, static_cast<unsigned int>(firmware.size()), firmware.data());
    int resetRet = simReset(core);

    unsigned char pd = static_cast<unsigned char>(model->pdValue);
    int pdWrite = writeMem(core, 0xF050, 1, &pd);

    CpuSnapshot resetSnap;
    takeSnapshot(core, getCount, readReg, resetSnap);

    unsigned int totalExecuted = 0;
    int lastExecRet = 0;
    bool stopped = false;
    std::vector<StepResult> steps;

    if (o.keys.empty()) {
        for (; totalExecuted < o.steps; ++totalExecuted) {
            lastExecRet = exec(core);
            if (lastExecRet != 0) { stopped = true; break; }
        }
    } else {
        unsigned char cmd = readByte(core, readMem, model->ioBase);
        for (size_t k = 0; k < o.keys.size(); ++k) {
            StepResult st;
            st.index = static_cast<unsigned int>(k);
            st.code = o.keys[k];
            st.label = keyLabel(st.code);
            st.injected = false;
            st.waitSteps = 0;
            st.injectedAtInstruction = 0;
            st.holdSteps = o.hold;
            st.settleSteps = o.settle;
            st.executedTotal = totalExecuted;
            st.cmdAtStart = cmd;
            st.cmdAtInjection = 0;
            st.cmdAfterHold = 0;
            st.cmdAfterRelease = 0;
            st.cmdAfterSettle = 0;
            st.kiInjected = 0;
            st.koInjected = 0;
            st.kiAfterSettle = 0;
            st.koAfterSettle = 0;
            st.execReturn = 0;
            st.lcdReadReturn = 0;
            st.lcdNonZero = 0;
            std::memset(&st.snap, 0, sizeof(st.snap));

            while (cmd != 1 && st.waitSteps < o.steps && !stopped) {
                lastExecRet = exec(core);
                if (lastExecRet != 0) { stopped = true; break; }
                ++totalExecuted;
                ++st.waitSteps;
                if ((st.waitSteps % o.poll) == 0 || st.waitSteps == o.steps) {
                    cmd = readByte(core, readMem, model->ioBase);
                }
            }
            st.cmdAtInjection = cmd;

            if (!stopped && cmd == 1) {
                st.injected = true;
                st.injectedAtInstruction = totalExecuted;
                st.kiInjected = static_cast<unsigned char>(1u << (st.code & 0x0F));
                st.koInjected = static_cast<unsigned char>(1u << ((st.code >> 4) & 0x0F));
                writeMem(core, model->ioBase + 1, 1, &st.kiInjected);
                writeMem(core, model->ioBase + 2, 1, &st.koInjected);

                for (unsigned int i = 0; i < o.hold && !stopped; ++i) {
                    lastExecRet = exec(core);
                    if (lastExecRet != 0) { stopped = true; break; }
                    ++totalExecuted;
                }
                st.cmdAfterHold = readByte(core, readMem, model->ioBase);

                unsigned char zero = 0;
                writeMem(core, model->ioBase + 1, 1, &zero);
                writeMem(core, model->ioBase + 2, 1, &zero);
                st.cmdAfterRelease = readByte(core, readMem, model->ioBase);

                for (unsigned int i = 0; i < o.settle && !stopped; ++i) {
                    lastExecRet = exec(core);
                    if (lastExecRet != 0) { stopped = true; break; }
                    ++totalExecuted;
                }
            }

            st.execReturn = lastExecRet;
            st.executedTotal = totalExecuted;
            st.cmdAfterSettle = readByte(core, readMem, model->ioBase);
            st.kiAfterSettle = readByte(core, readMem, model->ioBase + 1);
            st.koAfterSettle = readByte(core, readMem, model->ioBase + 2);
            takeSnapshot(core, getCount, readReg, st.snap);

            std::vector<unsigned char> lcd;
            st.lcdReadReturn = readLcdSnapshot(core, readMem, model, lcd, st.lcdHash, st.lcdNonZero);
            st.lcdFile = stepLcdPath(o, model, st);
            if (!st.lcdFile.empty()) {
                writeBytes(st.lcdFile, lcd.data(), model->lcdHashBytes);
            }

            steps.push_back(st);
            cmd = st.cmdAfterSettle;
            if (stopped || !st.injected) break;
        }
    }

    CpuSnapshot finalSnap;
    takeSnapshot(core, getCount, readReg, finalSnap);

    unsigned char cmd = 0, ki = 0, ko = 0;
    int cmdRet = readMem(core, model->ioBase, 1, &cmd);
    int kiRet = readMem(core, model->ioBase + 1, 1, &ki);
    int koRet = readMem(core, model->ioBase + 2, 1, &ko);

    std::vector<unsigned char> finalLcd;
    std::string finalLcdHash;
    unsigned int finalLcdNonZero = 0;
    int lcdRet = readLcdSnapshot(core, readMem, model, finalLcd, finalLcdHash, finalLcdNonZero);
    std::wstring finalLcdFile;
    if (!o.lcdOut.empty()) {
        finalLcdFile = absolutePath(o.lcdOut);
        writeBytes(o.lcdOut, finalLcd.data(), model->lcdHashBytes);
    } else if (!steps.empty() && !steps.back().lcdFile.empty()) {
        finalLcdFile = steps.back().lcdFile;
    }

    bool dtorCalled = false;
    bool dtorOk = false;
    if (dtor && dtor != NULL) {
        dtorCalled = true;
        dtor(core);
        dtorOk = true;
    }

    bool guardOk = true;
    unsigned int guardMismatch = 0;
    for (unsigned int i = 0; i < kGuardSize; ++i) {
        if (storage[i] != 0xCD) { guardOk = false; ++guardMismatch; }
        if (storage[kGuardSize + kCoreSize + i] != 0xCD) { guardOk = false; ++guardMismatch; }
    }

    bool dllHashOk = false;
    std::string dllHash = sha256File(dllPath, dllHashOk);
    bool fwHashOk = false;
    std::string fwHash = sha256File(o.firmware, fwHashOk);

    unsigned long long dllSize = 0, fwSize = 0;
    getFileSize(dllPath, dllSize);
    getFileSize(o.firmware, fwSize);

    bool anyStepLcdError = false;
    bool anyStepMissing = false;
    for (size_t i = 0; i < steps.size(); ++i) {
        if (!steps[i].injected) anyStepMissing = true;
        if (steps[i].lcdReadReturn != 0) anyStepLcdError = true;
    }

    std::vector<std::string> warnings;
    if (initModel != 0 || initCode != 0 || initData != 0 || initWindow != 0 || initDefault != 0)
        warnings.push_back("init_return_nonzero");
    if (romWrite != 0) warnings.push_back("write_code_memory_nonzero");
    if (resetRet != 0) warnings.push_back("sim_reset_nonzero");
    if (pdWrite != 0) warnings.push_back("write_pd_nonzero");
    if (lcdRet != 0) warnings.push_back("lcd_read_nonzero");
    if (anyStepLcdError) warnings.push_back("per_step_lcd_read_nonzero");
    if (!dtorCalled) warnings.push_back("destructor_export_missing");
    if (dtorCalled && !dtorOk) warnings.push_back("destructor_failed");
    if (!guardOk) warnings.push_back("core_buffer_guard_mismatch");
    if (!dllHashOk) warnings.push_back("dll_hash_failed");
    if (!fwHashOk) warnings.push_back("firmware_hash_failed");
    if (anyStepMissing) warnings.push_back("key_request_timeout");
    if (stopped) warnings.push_back("execution_stopped_early");

    std::string status = "ok";
    if (!resetSnap.havePc || !resetSnap.haveSp || !finalSnap.havePc || !finalSnap.haveSp ||
        lcdRet != 0 || !guardOk || stopped)
        status = "error";
    if (!o.keys.empty()) {
        if (steps.size() != o.keys.size() || anyStepMissing || anyStepLcdError) status = "error";
    }

    std::string mode = "steps";
    if (o.singleKey) mode = "single-key";
    else if (!o.keys.empty()) mode = "sequence";

    std::string j = "{\n";
    j += "  \"schema\": \"f7w71-s1/1\",\n";
    j += "  \"status\": " + jsonText(status) + ",\n";
    j += "  \"mode\": " + jsonText(mode) + ",\n";
    j += "  \"generated_utc\": " + jsonText(utcNow()) + ",\n";
    j += "  \"model\": " + jsonText(model->name) + ",\n";
    j += "  \"firmware\": {\n";
    j += "    \"path\": " + jsonWide(absolutePath(o.firmware)) + ",\n";
    j += "    \"size\": " + std::to_string(fwSize) + ",\n";
    j += "    \"sha256\": " + jsonText(fwHash) + ",\n";
    j += "    \"hash_ok\": " + std::string(fwHashOk ? "true" : "false") + "\n";
    j += "  },\n";
    j += "  \"dll\": {\n";
    j += "    \"path\": " + jsonWide(dllPath) + ",\n";
    j += "    \"size\": " + std::to_string(dllSize) + ",\n";
    j += "    \"sha256\": " + jsonText(dllHash) + ",\n";
    j += "    \"hash_ok\": " + std::string(dllHashOk ? "true" : "false") + ",\n";
    j += "    \"file_version\": " + jsonText(fileVersionString(dllPath)) + ",\n";
    j += "    \"search\": " + searchJson(search) + "\n";
    j += "  },\n";
    j += "  \"init\": {\n";
    j += "    \"memory_model\": 1,\n";
    j += "    \"code_start\": " + jsonText(hex32(model->codeStart)) + ",\n";
    j += "    \"code_end\": " + jsonText(hex32(model->codeEnd)) + ",\n";
    j += "    \"data_start\": " + jsonText(hex32(model->dataStart)) + ",\n";
    j += "    \"data_end\": " + jsonText(hex32(model->dataEnd)) + ",\n";
    j += "    \"rom_window_start\": " + jsonText(hex32(model->windowStart)) + ",\n";
    j += "    \"rom_window_end\": " + jsonText(hex32(model->windowEnd)) + ",\n";
    j += "    \"code_default\": 0,\n";
    j += "    \"pd_value\": " + jsonText(hex8(model->pdValue)) + ",\n";
    j += "    \"returns\": {\"set_memory_model\": " + std::to_string(initModel) +
         ", \"set_code_memory_size\": " + std::to_string(initCode) +
         ", \"set_data_memory_size\": " + std::to_string(initData) +
         ", \"set_rom_window_size\": " + std::to_string(initWindow) +
         ", \"set_code_memory_default_code\": " + std::to_string(initDefault) +
         ", \"write_code_memory\": " + std::to_string(romWrite) +
         ", \"sim_reset\": " + std::to_string(resetRet) +
         ", \"write_pd\": " + std::to_string(pdWrite) + "}\n";
    j += "  },\n";
    j += "  \"reset\": " + snapshotJson(resetSnap) + ",\n";
    j += "  \"run\": {\n";
    j += "    \"steps_requested\": " + std::to_string(o.steps) + ",\n";
    j += "    \"budget_per_key\": " + std::to_string(o.steps) + ",\n";
    j += "    \"steps_executed\": " + std::to_string(totalExecuted) + ",\n";
    j += "    \"exec_return\": " + std::to_string(lastExecRet) + ",\n";
    j += "    \"stopped\": " + std::string(stopped ? "true" : "false") + "\n";
    j += "  },\n";
    j += "  \"sequence\": {\n";
    j += "    \"codes\": " + codesJson(o.keys) + ",\n";
    j += "    \"labels\": " + labelsJson(o.keys) + ",\n";
    j += "    \"hold\": " + std::to_string(o.hold) + ",\n";
    j += "    \"settle\": " + std::to_string(o.settle) + ",\n";
    j += "    \"poll\": " + std::to_string(o.poll) + ",\n";
    j += "    \"budget_per_key\": " + std::to_string(o.steps) + "\n";
    j += "  },\n";
    j += "  \"steps\": [";
    if (!steps.empty()) {
        j += "\n";
        for (size_t i = 0; i < steps.size(); ++i) {
            j += stepJson(steps[i]);
            if (i + 1 < steps.size()) j += ",";
            j += "\n";
        }
        j += "  ";
    }
    j += "],\n";
    j += "  \"key\": ";
    if (o.singleKey && steps.size() == 1) {
        const StepResult &st = steps[0];
        j += "{\n";
        j += "    \"requested\": true,\n";
        j += "    \"injected\": " + std::string(st.injected ? "true" : "false") + ",\n";
        j += "    \"code\": " + jsonText(hex8(st.code)) + ",\n";
        j += "    \"ki\": " + jsonText(hex8(st.kiInjected)) + ",\n";
        j += "    \"ko\": " + jsonText(hex8(st.koInjected)) + ",\n";
        j += "    \"boot_attempts\": " + std::to_string(st.waitSteps + (st.injected ? 1u : 0u)) + ",\n";
        j += "    \"injected_at_instruction\": " + std::to_string(st.injectedAtInstruction) + ",\n";
        j += "    \"hold_steps\": " + std::to_string(st.holdSteps) + ",\n";
        j += "    \"settle_steps\": " + std::to_string(st.settleSteps) + ",\n";
        j += "    \"poll_interval\": " + std::to_string(o.poll) + ",\n";
        j += "    \"exec_return\": " + std::to_string(st.execReturn) + "\n";
        j += "  },\n";
    } else {
        j += "null,\n";
    }
    j += "  \"io\": {\n";
    j += "    \"base\": " + jsonText(hex32(model->ioBase)) + ",\n";
    j += "    \"cmd\": " + jsonText(hex8(cmd)) + ",\n";
    j += "    \"cmd_read_return\": " + std::to_string(cmdRet) + ",\n";
    j += "    \"ki\": " + jsonText(hex8(ki)) + ",\n";
    j += "    \"ki_read_return\": " + std::to_string(kiRet) + ",\n";
    j += "    \"ko\": " + jsonText(hex8(ko)) + ",\n";
    j += "    \"ko_read_return\": " + std::to_string(koRet) + "\n";
    j += "  },\n";
    j += "  \"final\": " + snapshotJson(finalSnap) + ",\n";
    j += "  \"registers\": " + registersJson(finalSnap) + ",\n";
    j += "  \"lcd\": {\n";
    j += "    \"address\": \"0xF800\",\n";
    j += "    \"read_size\": " + std::to_string(kLcdWindow) + ",\n";
    j += "    \"hashed_size\": " + std::to_string(model->lcdHashBytes) + ",\n";
    j += "    \"read_return\": " + std::to_string(lcdRet) + ",\n";
    j += "    \"sha256\": " + jsonText(finalLcdHash) + ",\n";
    j += "    \"non_zero_bytes\": " + std::to_string(finalLcdNonZero) + ",\n";
    j += "    \"dump_file\": " + (finalLcdFile.empty() ? std::string("null") : jsonWide(finalLcdFile)) + "\n";
    j += "  },\n";
    j += "  \"output_sha256\": " + jsonText(finalLcdHash) + ",\n";
    j += "  \"guards\": {\"ok\": " + std::string(guardOk ? "true" : "false") + ", \"mismatches\": " + std::to_string(guardMismatch) + "},\n";
    j += "  \"destructor\": {\"called\": " + std::string(dtorCalled ? "true" : "false") + ", \"ok\": " + std::string(dtorOk ? "true" : "false") + "},\n";
    j += "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) j += ", ";
        j += jsonText(warnings[i]);
    }
    j += "],\n";
    j += "  \"notes\": [";
    const char *notes[] = {
        "interrupt_table_not_set",
        "no_rtc_or_timer_host_service",
        "readreg_map_only_pc_and_sp_confirmed",
        "key_sequence_uses_fixed_hold_settle_per_step",
        "simu8_spy_not_available"
    };
    for (size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i) {
        if (i) j += ", ";
        j += jsonText(notes[i]);
    }
    j += "]\n";
    j += "}\n";

    bool wrote = writeText(o.out, j);
    FreeLibrary(dll);

    if (!wrote) {
        std::fprintf(stderr, "ERROR: could not write %s\n", toUtf8(o.out).c_str());
        return 8;
    }

    if (steps.empty()) {
        std::printf("model=%s mode=%s steps=%u icount=%u pc=%s lcd_sha256=%s status=%s\n",
                    model->name, mode.c_str(), totalExecuted, finalSnap.icount,
                    finalSnap.havePc ? hex32(finalSnap.pc).c_str() : "unavailable",
                    finalLcdHash.c_str(), status.c_str());
    } else {
        std::printf("model=%s mode=%s keys=%u steps=%u icount=%u pc=%s lcd_sha256=%s status=%s",
                    model->name, mode.c_str(), static_cast<unsigned int>(steps.size()), totalExecuted,
                    finalSnap.icount, finalSnap.havePc ? hex32(finalSnap.pc).c_str() : "unavailable",
                    finalLcdHash.c_str(), status.c_str());
        for (size_t i = 0; i < steps.size(); ++i) {
            std::printf(" | %s:%s", steps[i].label.c_str(), steps[i].lcdHash.c_str());
        }
        std::printf("\n");
    }
    return status == "ok" ? 0 : 9;
}
