#include "mcp_protocol.hpp"

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <queue>
#include <sstream>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objbase.h>
#include <roapi.h>
#include <bcrypt.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace DocStudio::Mcp {

namespace {

using json = nlohmann::json;

std::string TrimAscii(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string ToLowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void ReplaceAllInPlace(std::string& s, const std::string& needle, const std::string& replacement) {
    if (needle.empty() || s.empty()) return;
    std::size_t pos = 0;
    while ((pos = s.find(needle, pos)) != std::string::npos) {
        s.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
}

void RedactPatternPrefix(std::string& s, const std::string& prefix, std::size_t minTokenTail) {
    if (prefix.empty() || s.size() < prefix.size() + minTokenTail) return;
    std::size_t pos = 0;
    while ((pos = s.find(prefix, pos)) != std::string::npos) {
        std::size_t end = pos + prefix.size();
        while (end < s.size()) {
            const unsigned char ch = static_cast<unsigned char>(s[end]);
            if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.' || ch == '/' || ch == '+' || ch == '=') {
                ++end;
            } else {
                break;
            }
        }
        if (end - (pos + prefix.size()) >= minTokenTail) {
            const std::string repl = "[REDACTED]";
            s.replace(pos, end - pos, repl);
            pos += repl.size();
        } else {
            pos = end;
        }
    }
}

bool WriteAllFd(int fd, const char* data, std::size_t len) {
    std::size_t off = 0;
    while (off < len) {
#ifdef _WIN32
        const int chunk = static_cast<int>((std::min<std::size_t>)(len - off, 65536u));
        const int n = _write(fd, data + off, chunk);
#else
        const ssize_t n = ::write(fd, data + off, len - off);
#endif
        if (n <= 0) return false;
        off += static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace

// ---------------- SecretRedactor ----------------

void SecretRedactor::RegisterSecret(const std::string& secret) {
    if (secret.size() < 6) return;
    std::lock_guard<std::mutex> lk(mtx_);
    if (std::find(secrets_.begin(), secrets_.end(), secret) == secrets_.end()) {
        secrets_.push_back(secret);
    }
}

void SecretRedactor::ClearSecrets() {
    std::lock_guard<std::mutex> lk(mtx_);
    secrets_.clear();
}

std::string SecretRedactor::RedactString(std::string input) const {
    if (input.empty()) return input;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& sec : secrets_) {
            ReplaceAllInPlace(input, sec, "[REDACTED]");
        }
    }
    // Scrub common secret/API-key patterns defensively
    RedactPatternPrefix(input, "AIza", 16);
    RedactPatternPrefix(input, "ya29.", 16);
    RedactPatternPrefix(input, "sk-proj-", 16);
    RedactPatternPrefix(input, "sk-ant-", 16);
    RedactPatternPrefix(input, "sk-", 20);
    RedactPatternPrefix(input, "ghp_", 16);
    RedactPatternPrefix(input, "nvapi-", 16);
    RedactPatternPrefix(input, "gsk_", 16);
    RedactPatternPrefix(input, "xai-", 16);
    RedactPatternPrefix(input, "eyJ", 24);
    RedactPatternPrefix(input, "Bearer ", 12);
    RedactPatternPrefix(input, "bearer ", 12);
    return input;
}

namespace {
bool IsSensitiveJsonKeyName(const std::string& rawKey) {
    std::string norm;
    norm.reserve(rawKey.size());
    for (unsigned char c : rawKey) {
        if (c == '_' || c == '-') continue;
        norm.push_back(static_cast<char>(std::tolower(c)));
    }
    if (norm == "apikey" || norm == "accesstoken" || norm == "refreshtoken" ||
        norm == "idtoken" || norm == "authtoken" || norm == "bearertoken" ||
        norm == "sessiontoken" || norm == "notiontoken" || norm == "oauthtoken" ||
        norm == "password" || norm == "passphrase" || norm == "cookie" ||
        norm == "cookies" || norm == "cookieheader" || norm == "sessioncookie" ||
        norm == "authorization" || norm == "clientsecret" || norm == "privatekey" ||
        norm == "jwt" || norm == "secret" || norm == "xapikey" || norm == "xcuadernutoken") {
        return true;
    }
    auto endsWith = [&](const char* suf) {
        const std::size_t n = std::strlen(suf);
        return norm.size() >= n && norm.compare(norm.size() - n, n, suf) == 0;
    };
    return endsWith("apikey") || endsWith("password") || endsWith("passphrase") ||
           endsWith("clientsecret") || endsWith("privatekey") || endsWith("accesstoken") ||
           endsWith("refreshtoken");
}

bool TryGetStringField(const json& obj, const char* key, std::string& out) {
    out.clear();
    if (!obj.is_object() || !obj.contains(key)) return false;
    if (!obj[key].is_string()) return false;
    out = obj[key].get<std::string>();
    return true;
}

#ifdef _WIN32
bool SendAllSocket(SOCKET sock, const char* data, std::size_t len) {
    std::size_t off = 0;
    while (off < len) {
        const int chunk = static_cast<int>((std::min<std::size_t>)(len - off, 65536u));
        const int n = send(sock, data + off, chunk, 0);
        if (n <= 0) return false;
        off += static_cast<std::size_t>(n);
    }
    return true;
}
#endif
} // namespace

json SecretRedactor::RedactJson(json value) const {
    if (value.is_string()) {
        return RedactString(value.get<std::string>());
    }
    if (value.is_array()) {
        for (auto& el : value) el = RedactJson(std::move(el));
        return value;
    }
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (IsSensitiveJsonKeyName(it.key())) {
                if (it.value().is_string() && !it.value().get<std::string>().empty()) {
                    it.value() = "[REDACTED]";
                }
            } else {
                it.value() = RedactJson(std::move(it.value()));
            }
        }
        return value;
    }
    return value;
}

// ---------------- Encoding & Token Helpers ----------------

std::string Base64Encode(const std::uint8_t* data, std::size_t len) {
    static constexpr const char* kAlpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    if (!data || len == 0) return out;
    out.reserve(((len + 2u) / 3u) * 4u);
    std::size_t i = 0;
    while (i + 2u < len) {
        const std::uint32_t t = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kAlpha[(t >> 18) & 0x3Fu]);
        out.push_back(kAlpha[(t >> 12) & 0x3Fu]);
        out.push_back(kAlpha[(t >> 6) & 0x3Fu]);
        out.push_back(kAlpha[t & 0x3Fu]);
        i += 3u;
    }
    if (i < len) {
        std::uint32_t t = static_cast<std::uint32_t>(data[i]) << 16;
        if (i + 1u < len) t |= static_cast<std::uint32_t>(data[i + 1]) << 8;
        out.push_back(kAlpha[(t >> 18) & 0x3Fu]);
        out.push_back(kAlpha[(t >> 12) & 0x3Fu]);
        out.push_back((i + 1u < len) ? kAlpha[(t >> 6) & 0x3Fu] : '=');
        out.push_back('=');
    }
    return out;
}

std::string Base64Encode(const std::vector<std::uint8_t>& data) {
    return Base64Encode(data.data(), data.size());
}

std::vector<std::uint8_t> Base64Decode(const std::string& b64) {
    auto valOf = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<std::uint8_t> out;
    out.reserve((b64.size() / 4u) * 3u);
    std::uint32_t buf = 0;
    int bits = -8;
    for (unsigned char c : b64) {
        if (c == '=') break;
        const int v = valOf(c);
        if (v < 0) continue;
        buf = (buf << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<std::uint8_t>((buf >> bits) & 0xFFu));
            bits -= 8;
        }
    }
    return out;
}

std::string GenerateTokenHex192() {
    std::uint8_t raw[24]{};
#ifdef _WIN32
    BCryptGenRandom(nullptr, raw, static_cast<ULONG>(sizeof(raw)), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
    for (std::size_t i = 0; i < sizeof(raw); ++i) raw[i] = static_cast<std::uint8_t>((i * 37u + 19u) & 0xFFu);
#endif
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(48);
    for (std::uint8_t b : raw) {
        out.push_back(kHex[(b >> 4) & 0xFu]);
        out.push_back(kHex[b & 0xFu]);
    }
    return out;
}

bool ConstantTimeEquals(const std::string& a, const std::string& b) noexcept {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

// ---------------- McpProtocolRouter ----------------

McpProtocolRouter::McpProtocolRouter(std::string serverName, std::string serverVersion)
    : serverName_(std::move(serverName)), serverVersion_(std::move(serverVersion)) {}

void McpProtocolRouter::RegisterTool(McpToolDef def) {
    if (def.name.empty()) return;
    auto it = toolIndex_.find(def.name);
    if (it != toolIndex_.end()) {
        tools_[it->second] = std::move(def);
    } else {
        toolIndex_[def.name] = tools_.size();
        tools_.push_back(std::move(def));
    }
}

void McpProtocolRouter::RegisterResource(McpResourceDef def) {
    resources_.push_back(std::move(def));
}

void McpProtocolRouter::RegisterResourceTemplate(McpResourceTemplateDef def) {
    resourceTemplates_.push_back(std::move(def));
}

void McpProtocolRouter::SetResourceReader(McpResourceReader reader) {
    resourceReader_ = std::move(reader);
}

void McpProtocolRouter::RegisterPrompt(McpPromptDef def) {
    if (def.name.empty()) return;
    auto it = promptIndex_.find(def.name);
    if (it != promptIndex_.end()) {
        prompts_[it->second] = std::move(def);
    } else {
        promptIndex_[def.name] = prompts_.size();
        prompts_.push_back(std::move(def));
    }
}

McpToolResult McpProtocolRouter::CallToolDirect(
    const std::string& toolName,
    const json& args,
    const McpRequestContext* externalCtx)
{
    auto it = toolIndex_.find(toolName);
    if (it == toolIndex_.end()) {
        return McpToolResult::Err("Herramienta no encontrada: " + toolName);
    }
    McpRequestContext localCtx;
    const McpRequestContext& ctx = externalCtx ? *externalCtx : localCtx;
    try {
        McpToolResult res = tools_[it->second].handler(args.is_object() ? args : json::object(), ctx);
        res.data = redactor_.RedactJson(std::move(res.data));
        res.errorMessageUtf8 = redactor_.RedactString(std::move(res.errorMessageUtf8));
        return res;
    } catch (const std::exception& ex) {
        return McpToolResult::Err(redactor_.RedactString(std::string("Excepcion en herramienta: ") + ex.what()));
    } catch (...) {
        return McpToolResult::Err("Excepcion interna desconocida en herramienta.");
    }
}

std::string McpProtocolRouter::HandleJsonRpcLine(
    const std::string& lineUtf8,
    const McpRequestContext* externalCtx,
    std::function<void(const std::string& notificationJsonLine)> emitNotification)
{
    const std::string trimmed = TrimAscii(lineUtf8);
    if (trimmed.empty()) return {};

    json req;
    try {
        req = json::parse(trimmed);
    } catch (...) {
        return json({
            { "jsonrpc", "2.0" },
            { "id", nullptr },
            { "error", { { "code", -32700 }, { "message", "Parse error" } } }
        }).dump(-1, ' ', false, json::error_handler_t::replace);
    }

    if (!req.is_object()) {
        return json({
            { "jsonrpc", "2.0" },
            { "id", nullptr },
            { "error", { { "code", -32600 }, { "message", "Invalid Request" } } }
        }).dump(-1, ' ', false, json::error_handler_t::replace);
    }

    const bool hasId = req.contains("id") && !req["id"].is_null();
    std::string method;
    const bool hasValidMethod = TryGetStringField(req, "method", method) && !method.empty();

    // Strict JSON-RPC 2.0: ANY message without "id" is a notification and MUST NOT produce any response.
    if (!hasId) {
        return {};
    }

    const json idVal = req["id"];
    const json params = (req.contains("params") && req["params"].is_object()) ? req["params"] : json::object();

    auto okResp = [&](const json& result) -> std::string {
        return json({ { "jsonrpc", "2.0" }, { "id", idVal }, { "result", result } })
            .dump(-1, ' ', false, json::error_handler_t::replace);
    };
    auto errResp = [&](int code, const std::string& msg) -> std::string {
        return json({
            { "jsonrpc", "2.0" },
            { "id", idVal },
            { "error", { { "code", code }, { "message", redactor_.RedactString(msg) } } }
        }).dump(-1, ' ', false, json::error_handler_t::replace);
    };

    if (!hasValidMethod) {
        return errResp(-32600, "Invalid Request: 'method' must be a non-empty string");
    }

    try {
    if (method == "ping") {
        return okResp(json::object());
    }

    if (method == "initialize") {
        std::string clientProto;
        TryGetStringField(params, "protocolVersion", clientProto);
        std::string chosenProto = "2025-03-26";
        if (clientProto == "2025-06-18" || clientProto == "2025-03-26" || clientProto == "2024-11-05") {
            chosenProto = clientProto;
        }
        return okResp({
            { "protocolVersion", chosenProto },
            { "capabilities", {
                { "tools", { { "listChanged", false } } },
                { "resources", { { "subscribe", false }, { "listChanged", false } } },
                { "prompts", { { "listChanged", false } } },
                { "logging", json::object() }
            } },
            { "serverInfo", {
                { "name", serverName_ },
                { "version", serverVersion_ }
            } },
            { "instructions", "Servidor MCP integral de CuadernU. Usa cuadernu_capabilities para autodescubrimiento de las 35 herramientas, 324+ comandos, capturas offscreen/stealth GUI y sesiones multi-documento." }
        });
    }

    if (method == "logging/setLevel") {
        std::string lvl;
        if (TryGetStringField(params, "level", lvl) && !lvl.empty()) {
            std::lock_guard<std::mutex> lk(logLevelMtx_);
            logLevel_ = lvl;
        }
        return okResp(json::object());
    }

    if (method == "tools/list") {
        json arr = json::array();
        for (const auto& t : tools_) {
            arr.push_back({
                { "name", t.name },
                { "description", t.description },
                { "inputSchema", t.inputSchema.is_object() ? t.inputSchema : json({ { "type", "object" }, { "properties", json::object() } }) }
            });
        }
        return okResp({ { "tools", std::move(arr) } });
    }

    if (method == "tools/call") {
        std::string toolName;
        if (!TryGetStringField(params, "name", toolName) || toolName.empty()) {
            return errResp(-32602, "Invalid params: 'name' debe ser una cadena no vacia");
        }
        const json args = (params.contains("arguments") && params["arguments"].is_object())
            ? params["arguments"] : json::object();

        auto it = toolIndex_.find(toolName);
        if (it == toolIndex_.end()) {
            return errResp(-32601, "Herramienta no encontrada: " + toolName);
        }

        McpRequestContext ctx;
        if (externalCtx) {
            ctx.cancelled = externalCtx->cancelled;
        }
        if (params.contains("_meta") && params["_meta"].is_object() && params["_meta"].contains("progressToken")) {
            ctx.progressToken = params["_meta"]["progressToken"];
            if (emitNotification) {
                ctx.reportProgress = [emitNotification, tok = ctx.progressToken](double progress, double total, const std::string& message) {
                    json notif = {
                        { "jsonrpc", "2.0" },
                        { "method", "notifications/progress" },
                        { "params", {
                            { "progressToken", tok },
                            { "progress", progress },
                            { "total", total }
                        } }
                    };
                    if (!message.empty()) notif["params"]["message"] = message;
                    emitNotification(notif.dump(-1, ' ', false, json::error_handler_t::replace));
                };
            }
        }

        McpToolResult res = CallToolDirect(toolName, args, &ctx);
        json content = json::array();
        if (res.isError) {
            json errObj = res.data.is_object() ? res.data : json::object();
            errObj["error"] = res.errorMessageUtf8.empty() ? "Error desconocido" : res.errorMessageUtf8;
            content.push_back({
                { "type", "text" },
                { "text", errObj.dump(2, ' ', false, json::error_handler_t::replace) }
            });
            return okResp({ { "content", std::move(content) }, { "isError", true } });
        }

        content.push_back({
            { "type", "text" },
            { "text", res.data.dump(2, ' ', false, json::error_handler_t::replace) }
        });
        for (const auto& img : res.images) {
            if (img.bytes.empty()) continue;
            content.push_back({
                { "type", "image" },
                { "mimeType", img.mimeTypeUtf8.empty() ? "image/png" : img.mimeTypeUtf8 },
                { "data", Base64Encode(img.bytes) }
            });
        }
        return okResp({ { "content", std::move(content) }, { "isError", false } });
    }

    if (method == "resources/list") {
        json arr = json::array();
        for (const auto& r : resources_) {
            arr.push_back({
                { "uri", r.uri },
                { "name", r.name },
                { "description", r.description },
                { "mimeType", r.mimeType }
            });
        }
        return okResp({ { "resources", std::move(arr) } });
    }

    if (method == "resources/templates/list") {
        json arr = json::array();
        for (const auto& rt : resourceTemplates_) {
            arr.push_back({
                { "uriTemplate", rt.uriTemplate },
                { "name", rt.name },
                { "description", rt.description },
                { "mimeType", rt.mimeType }
            });
        }
        return okResp({ { "resourceTemplates", std::move(arr) } });
    }

    if (method == "resources/read") {
        std::string uri;
        if (!TryGetStringField(params, "uri", uri) || uri.empty()) {
            return errResp(-32602, "Parametro uri requerido (string)");
        }
        if (!resourceReader_) return errResp(-32601, "Lector de recursos no configurado");
        McpResourceContent out;
        std::string err;
        if (!resourceReader_(uri, out, err)) {
            return errResp(-32002, err.empty() ? ("Recurso no encontrado: " + uri) : err);
        }
        json entry = {
            { "uri", out.uri.empty() ? uri : out.uri },
            { "mimeType", out.mimeType }
        };
        if (out.isBlob) {
            entry["blob"] = Base64Encode(out.blobBytes);
        } else {
            entry["text"] = redactor_.RedactString(out.textUtf8);
        }
        return okResp({ { "contents", json::array({ std::move(entry) }) } });
    }

    if (method == "prompts/list") {
        json arr = json::array();
        for (const auto& p : prompts_) {
            json pArgs = json::array();
            for (const auto& a : p.arguments) {
                pArgs.push_back({
                    { "name", a.name },
                    { "description", a.description },
                    { "required", a.required }
                });
            }
            arr.push_back({
                { "name", p.name },
                { "description", p.description },
                { "arguments", std::move(pArgs) }
            });
        }
        return okResp({ { "prompts", std::move(arr) } });
    }

    if (method == "prompts/get") {
        std::string pName;
        if (!TryGetStringField(params, "name", pName) || pName.empty()) {
            return errResp(-32602, "Parametro name requerido (string)");
        }
        auto it = promptIndex_.find(pName);
        if (it == promptIndex_.end()) {
            return errResp(-32602, "Prompt no encontrado: " + pName);
        }
        const json pArgs = (params.contains("arguments") && params["arguments"].is_object())
            ? params["arguments"] : json::object();
        json built = prompts_[it->second].builder ? prompts_[it->second].builder(pArgs) : json::object();
        built = redactor_.RedactJson(std::move(built));
        return okResp(built);
    }

    return errResp(-32601, "Metodo no soportado: " + method);
    } catch (const json::exception& ex) {
        return errResp(-32602, std::string("Invalid params: ") + ex.what());
    } catch (const std::exception& ex) {
        return errResp(-32603, std::string("Internal error: ") + ex.what());
    } catch (...) {
        return errResp(-32603, "Internal error");
    }
}

// ---------------- Binary LF Stdio Server (Reader + Async Worker) ----------------

int McpProtocolRouter::RunStdioServer() {
    // Isolate protocol stdout from stray std::cout/printf writes and force binary LF framing on Windows
    int mcpOutFd = 1;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    std::fflush(stdout);
    const int dupOut = _dup(_fileno(stdout));
    if (dupOut >= 0) {
        mcpOutFd = dupOut;
        _setmode(mcpOutFd, _O_BINARY);
        if (HANDLE hOut = reinterpret_cast<HANDLE>(_get_osfhandle(mcpOutFd)); hOut && hOut != INVALID_HANDLE_VALUE) {
            SetHandleInformation(hOut, HANDLE_FLAG_INHERIT, 0);
        }
        _dup2(_fileno(stderr), _fileno(stdout));
    }
    if (HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE); hIn && hIn != INVALID_HANDLE_VALUE) {
        SetHandleInformation(hIn, HANDLE_FLAG_INHERIT, 0);
    }
#endif

    std::mutex writeMtx;
    auto writeFrame = [&](const std::string& jsonLine) {
        if (jsonLine.empty()) return;
        std::string frame = jsonLine;
        // Ensure strict single-line LF framing (zero \r, zero embedded \n)
        frame.erase(std::remove(frame.begin(), frame.end(), '\r'), frame.end());
        frame.erase(std::remove(frame.begin(), frame.end(), '\n'), frame.end());
        frame.push_back('\n');
        std::lock_guard<std::mutex> lk(writeMtx);
        WriteAllFd(mcpOutFd, frame.data(), frame.size());
    };

    struct QueuedTask {
        std::string line;
        std::string idKey;
        std::shared_ptr<std::atomic<bool>> cancelFlag;
    };

    std::mutex qMtx;
    std::condition_variable qCv;
    std::deque<QueuedTask> queue;
    std::unordered_map<std::string, std::shared_ptr<std::atomic<bool>>> inflightCancels;
    bool eofReached = false;

    std::thread worker([&]() {
#ifdef _WIN32
        struct ScopedThreadComWinRt {
            bool comInit = false;
            bool roInit = false;
            ScopedThreadComWinRt() {
                const HRESULT hrC = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                comInit = (hrC == S_OK || hrC == S_FALSE);
                const HRESULT hrR = RoInitialize(RO_INIT_SINGLETHREADED);
                roInit = (hrR == S_OK || hrR == S_FALSE);
            }
            ~ScopedThreadComWinRt() {
                if (roInit) RoUninitialize();
                if (comInit) CoUninitialize();
            }
        } threadComGuard;
#endif
        while (true) {
            QueuedTask task;
            {
                std::unique_lock<std::mutex> lk(qMtx);
                qCv.wait(lk, [&]() { return !queue.empty() || eofReached; });
                if (queue.empty() && eofReached) break;
                task = std::move(queue.front());
                queue.pop_front();
            }
            if (!task.cancelFlag || !task.cancelFlag->load(std::memory_order_acquire)) {
                McpRequestContext ctx;
                ctx.cancelled = task.cancelFlag;
                const std::string resp = HandleJsonRpcLine(task.line, &ctx, writeFrame);
                if (!resp.empty() && (!task.cancelFlag || !task.cancelFlag->load(std::memory_order_acquire))) {
                    writeFrame(resp);
                }
            }
            if (!task.idKey.empty()) {
                std::lock_guard<std::mutex> lk(qMtx);
                inflightCancels.erase(task.idKey);
            }
        }
    });

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string trimmed = TrimAscii(line);
        if (trimmed.empty()) continue;

        bool handledInline = false;
        try {
            const json probe = json::parse(trimmed);
            if (probe.is_object()) {
                const bool hasId = probe.contains("id") && !probe["id"].is_null();
                std::string method;
                const bool validMethod = TryGetStringField(probe, "method", method);

                if (!hasId) {
                    if (validMethod && method == "notifications/cancelled" && probe.contains("params") && probe["params"].is_object()) {
                        if (probe["params"].contains("requestId")) {
                            const std::string rk = probe["params"]["requestId"].dump();
                            std::lock_guard<std::mutex> lk(qMtx);
                            auto it = inflightCancels.find(rk);
                            if (it != inflightCancels.end() && it->second) {
                                it->second->store(true, std::memory_order_release);
                            }
                        }
                    }
                    handledInline = true; // Never reply to notifications
                } else if (!validMethod || method == "ping" || method == "initialize" || method == "logging/setLevel") {
                    const std::string resp = HandleJsonRpcLine(trimmed, nullptr, writeFrame);
                    if (!resp.empty()) writeFrame(resp);
                    handledInline = true;
                } else {
                    const std::string idKey = probe["id"].dump();
                    auto flag = std::make_shared<std::atomic<bool>>(false);
                    {
                        std::lock_guard<std::mutex> lk(qMtx);
                        inflightCancels[idKey] = flag;
                        queue.push_back({ trimmed, idKey, flag });
                    }
                    qCv.notify_one();
                    handledInline = true;
                }
            }
        } catch (...) {
            // Malformed JSON -> handle inline with -32700 Parse error
            const std::string resp = HandleJsonRpcLine(trimmed, nullptr, writeFrame);
            if (!resp.empty()) writeFrame(resp);
            handledInline = true;
        }

        if (!handledInline) {
            const std::string resp = HandleJsonRpcLine(trimmed, nullptr, writeFrame);
            if (!resp.empty()) writeFrame(resp);
        }
    }

    {
        std::lock_guard<std::mutex> lk(qMtx);
        eofReached = true;
    }
    qCv.notify_all();
    if (worker.joinable()) worker.join();

#ifdef _WIN32
    if (mcpOutFd != 1) {
        _close(mcpOutFd);
    }
#endif
    return 0;
}

// ---------------- Optional 127.0.0.1 HTTP Transport ----------------

int McpProtocolRouter::RunHttpServer(
    std::uint16_t port,
    const std::string& bearerTokenUtf8,
    std::atomic<bool>* stopFlag)
{
#ifdef _WIN32
    struct ScopedHttpComWinRt {
        bool comInit = false;
        bool roInit = false;
        ScopedHttpComWinRt() {
            const HRESULT hrC = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            comInit = (hrC == S_OK || hrC == S_FALSE);
            const HRESULT hrR = RoInitialize(RO_INIT_SINGLETHREADED);
            roInit = (hrR == S_OK || hrR == S_FALSE);
        }
        ~ScopedHttpComWinRt() {
            if (roInit) RoUninitialize();
            if (comInit) CoUninitialize();
        }
    } httpComGuard;
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        listen(listenSock, 16) == SOCKET_ERROR) {
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    const std::string expectedAuth = "Bearer " + bearerTokenUtf8;
    std::mutex dispatchMtx;

    while (!stopFlag || !stopFlag->load(std::memory_order_acquire)) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listenSock, &rfds);
        timeval tv{ 0, 200000 }; // 200ms poll for clean shutdown
        const int sel = select(0, &rfds, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        DWORD rcvTimeout = 5000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rcvTimeout), sizeof(rcvTimeout));

        constexpr std::size_t kMaxHttpRequestBytes = 32u * 1024u * 1024u;
        std::string rawReq;
        char buf[4096];
        std::size_t contentLen = 0;
        std::size_t headerEnd = std::string::npos;
        bool tooLarge = false;
        while (rawReq.size() < kMaxHttpRequestBytes) {
            const int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0) break;
            rawReq.append(buf, static_cast<std::size_t>(n));
            if (headerEnd == std::string::npos) {
                headerEnd = rawReq.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    const std::string hdrLow = ToLowerAscii(rawReq.substr(0, headerEnd));
                    const std::size_t clPos = hdrLow.find("content-length:");
                    if (clPos != std::string::npos) {
                        const std::size_t eol = hdrLow.find("\r\n", clPos);
                        const std::string val = TrimAscii(hdrLow.substr(clPos + 15, eol == std::string::npos ? std::string::npos : eol - (clPos + 15)));
                        contentLen = static_cast<std::size_t>(std::strtoull(val.c_str(), nullptr, 10));
                        if (contentLen > kMaxHttpRequestBytes - 65536u) {
                            tooLarge = true;
                            break;
                        }
                    }
                }
            }
            if (headerEnd != std::string::npos && rawReq.size() >= headerEnd + 4u + contentLen) break;
        }

        auto sendHttp = [&](int code, const char* statusText, const std::string& body, const char* contentType = "application/json") {
            std::ostringstream oss;
            oss << "HTTP/1.1 " << code << " " << statusText << "\r\n"
                << "Content-Type: " << contentType << "\r\n"
                << "Content-Length: " << body.size() << "\r\n"
                << "Cache-Control: no-store\r\n"
                << "Connection: close\r\n\r\n"
                << body;
            const std::string wire = oss.str();
            SendAllSocket(client, wire.data(), wire.size());
        };

        if (tooLarge) {
            sendHttp(413, "Payload Too Large", R"({"error":"payload_too_large"})");
            closesocket(client);
            continue;
        }

        if (headerEnd == std::string::npos) {
            sendHttp(400, "Bad Request", R"({"error":"malformed_http"})");
            closesocket(client);
            continue;
        }

        const std::string headersPart = rawReq.substr(0, headerEnd);
        const std::string bodyPart = rawReq.substr(headerEnd + 4u, contentLen);
        std::istringstream hss(headersPart);
        std::string reqLine;
        std::getline(hss, reqLine);
        if (!reqLine.empty() && reqLine.back() == '\r') reqLine.pop_back();

        std::string httpMethod, httpPath;
        {
            std::istringstream rl(reqLine);
            rl >> httpMethod >> httpPath;
        }

        std::string authHeader;
        std::string hLine;
        while (std::getline(hss, hLine)) {
            if (!hLine.empty() && hLine.back() == '\r') hLine.pop_back();
            const std::size_t col = hLine.find(':');
            if (col == std::string::npos) continue;
            if (ToLowerAscii(TrimAscii(hLine.substr(0, col))) == "authorization") {
                authHeader = TrimAscii(hLine.substr(col + 1));
            }
        }

        if (httpMethod == "GET" && httpPath == "/health") {
            sendHttp(200, "OK", json({ { "ok", true }, { "server", serverName_ }, { "version", serverVersion_ } }).dump());
            closesocket(client);
            continue;
        }

        if (!bearerTokenUtf8.empty() && !ConstantTimeEquals(authHeader, expectedAuth)) {
            sendHttp(401, "Unauthorized", R"({"error":"unauthorized"})");
            closesocket(client);
            continue;
        }

        if (httpMethod == "POST" && (httpPath == "/mcp" || httpPath == "/")) {
            std::string respJson;
            {
                std::lock_guard<std::mutex> lk(dispatchMtx);
                respJson = HandleJsonRpcLine(bodyPart);
            }
            if (respJson.empty()) {
                sendHttp(202, "Accepted", "");
            } else {
                sendHttp(200, "OK", respJson);
            }
        } else {
            sendHttp(404, "Not Found", R"({"error":"not_found"})");
        }
        closesocket(client);
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
#else
    (void)port; (void)bearerTokenUtf8; (void)stopFlag;
    return 1;
#endif
}

} // namespace DocStudio::Mcp
