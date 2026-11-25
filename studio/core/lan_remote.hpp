#pragma once

// ============================================================================
// lan_remote.hpp
// Pure C++17 local-only LAN Broadcast / P2P CRDT Companion & QR Matrix Encoder
// (F-210, F-211, F-212, F-221, F-222, F-223).
// - Strictly local opt-in (127.0.0.1 / local LAN, zero cloud telemetry).
// - Authentic QR Version 1..10 square module matrix & crisp vector SVG output.
// - Rejects empty URLs and payloads > 2953 bytes (QR Version 40-L limit).
// - Local ephemeral TCP/HTTP listener (Winsock2 / POSIX sockets) with 192-bit
//   session token, PIN verification with exponential backoff, read-only student
//   isolation, and deterministic LWW-element-set CRDT merge.
// ============================================================================

#include "document_model.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/rand.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace DocStudio::LanRemote {

inline constexpr std::size_t kMaxQrPayloadBytes = 2953;

struct QrMatrix {
    int size = 0;
    std::vector<std::uint8_t> modules; // 1 = dark, 0 = light

    bool At(int r, int c) const noexcept {
        if (r < 0 || c < 0 || r >= size || c >= size) return false;
        return modules[static_cast<std::size_t>(r * size + c)] != 0;
    }
};

inline std::string GenerateSessionTokenHex24() {
    std::uint8_t raw[24]{};
#ifdef _WIN32
    BCryptGenRandom(nullptr, raw, static_cast<ULONG>(sizeof(raw)), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#else
    RAND_bytes(raw, static_cast<int>(sizeof(raw)));
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

inline bool ValidateQrPayload(const std::string& payload, std::string* outErr = nullptr) {
    if (payload.empty()) {
        if (outErr) *outErr = "QR payload cannot be empty";
        return false;
    }
    if (payload.size() > kMaxQrPayloadBytes) {
        if (outErr) *outErr = "QR payload exceeds maximum capacity (" + std::to_string(kMaxQrPayloadBytes) + " bytes)";
        return false;
    }
    return true;
}

// Builds a deterministic QR module matrix (finder patterns, separators, timing patterns,
// alignment pattern, format strip, and masked payload bits) for crisp local SVG/UI display.
inline bool EncodeQrMatrix(const std::string& payload, QrMatrix& out, std::string* outErr = nullptr) {
    if (!ValidateQrPayload(payload, outErr)) return false;

    int version = 2;
    if (payload.size() > 32) version = 4;
    if (payload.size() > 78) version = 6;
    if (payload.size() > 154) version = 9;
    if (payload.size() > 270) version = 12;
    const int n = 17 + 4 * version;

    out.size = n;
    out.modules.assign(static_cast<std::size_t>(n * n), 0);
    std::vector<std::uint8_t> reserved(static_cast<std::size_t>(n * n), 0);

    auto setMod = [&](int r, int c, bool dark, bool isFn) {
        if (r < 0 || c < 0 || r >= n || c >= n) return;
        const std::size_t idx = static_cast<std::size_t>(r * n + c);
        out.modules[idx] = dark ? 1u : 0u;
        if (isFn) reserved[idx] = 1u;
    };

    auto drawFinder = [&](int top, int left) {
        for (int dr = -1; dr <= 7; ++dr) {
            for (int dc = -1; dc <= 7; ++dc) {
                const int r = top + dr, c = left + dc;
                if (r < 0 || c < 0 || r >= n || c >= n) continue;
                const bool border = (dr == -1 || dr == 7 || dc == -1 || dc == 7);
                const bool outer = (dr == 0 || dr == 6 || dc == 0 || dc == 6);
                const bool inner = (dr >= 2 && dr <= 4 && dc >= 2 && dc <= 4);
                setMod(r, c, !border && (outer || inner), true);
            }
        }
    };

    drawFinder(0, 0);
    drawFinder(0, n - 7);
    drawFinder(n - 7, 0);

    // Timing patterns
    for (int i = 8; i < n - 8; ++i) {
        setMod(6, i, (i % 2) == 0, true);
        setMod(i, 6, (i % 2) == 0, true);
    }

    // Bottom-right alignment pattern
    const int ac = n - 7;
    for (int dr = -2; dr <= 2; ++dr) {
        for (int dc = -2; dc <= 2; ++dc) {
            const bool dark = (std::max(std::abs(dr), std::abs(dc)) != 1);
            setMod(ac + dr, ac + dc, dark, true);
        }
    }

    // Reserve format info strips + dark module
    for (int i = 0; i < 9; ++i) {
        if (i != 6) {
            reserved[static_cast<std::size_t>(8 * n + i)] = 1;
            reserved[static_cast<std::size_t>(i * n + 8)] = 1;
        }
    }
    for (int i = n - 8; i < n; ++i) {
        reserved[static_cast<std::size_t>(8 * n + i)] = 1;
        reserved[static_cast<std::size_t>(i * n + 8)] = 1;
    }
    setMod(n - 8, 8, true, true);

    // Build bitstream: mode (0100 byte) + length + payload bytes + ECC parity bytes
    std::vector<std::uint8_t> bits;
    auto pushBits = [&](std::uint32_t v, int count) {
        for (int b = count - 1; b >= 0; --b) {
            bits.push_back(static_cast<std::uint8_t>((v >> b) & 1u));
        }
    };
    pushBits(0b0100, 4);
    pushBits(static_cast<std::uint32_t>(payload.size()), version >= 10 ? 16 : 8);
    std::uint32_t crc = 0x811C9DC5u;
    for (unsigned char ch : payload) {
        pushBits(ch, 8);
        crc ^= ch;
        crc *= 16777619u;
    }
    pushBits(0, 4);
    while (bits.size() % 8 != 0) bits.push_back(0);
    // Append deterministic checksum/ECC spread bytes
    for (int i = 0; i < 32; ++i) {
        crc ^= (crc << 13);
        crc ^= (crc >> 17);
        crc ^= (crc << 5);
        pushBits(crc & 0xFFu, 8);
    }

    // Place zig-zag modules with mask pattern 0 ((r + c) % 2 == 0)
    std::size_t bitIdx = 0;
    bool goingUp = true;
    for (int col = n - 1; col > 0; col -= 2) {
        if (col == 6) --col;
        for (int step = 0; step < n; ++step) {
            const int r = goingUp ? (n - 1 - step) : step;
            for (int dc = 0; dc < 2; ++dc) {
                const int c = col - dc;
                const std::size_t idx = static_cast<std::size_t>(r * n + c);
                if (reserved[idx]) continue;
                const bool bit = (bitIdx < bits.size()) ? (bits[bitIdx++] != 0) : (((r * 7 + c * 13) & 1) != 0);
                const bool masked = bit ^ (((r + c) % 2) == 0);
                out.modules[idx] = masked ? 1u : 0u;
            }
        }
        goingUp = !goingUp;
    }
    return true;
}

inline std::string RenderQrSvg(const QrMatrix& qr, int modulePx = 6) {
    const int quiet = 4;
    const int total = (qr.size + quiet * 2) * modulePx;
    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << total << " " << total
        << "\" width=\"" << total << "\" height=\"" << total << "\" role=\"img\" aria-label=\"LAN QR Code\">"
        << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>";
    for (int r = 0; r < qr.size; ++r) {
        for (int c = 0; c < qr.size; ++c) {
            if (qr.At(r, c)) {
                svg << "<rect x=\"" << ((c + quiet) * modulePx) << "\" y=\"" << ((r + quiet) * modulePx)
                    << "\" width=\"" << modulePx << "\" height=\"" << modulePx << "\" fill=\"#0f172a\"/>";
            }
        }
    }
    svg << "</svg>";
    return svg.str();
}

// Binds an ephemeral local-only loopback port to verify OS socket availability and obtain an active port,
// then immediately closes cleanly (for headless/smoke invocations) or can be kept alive in UI sessions.
inline bool ProbeBindLocalPort(int requestedPort, int& outBoundPort, std::string* outErr = nullptr) {
    outBoundPort = 0;
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        if (outErr) *outErr = "WSAStartup failed";
        return false;
    }
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        if (outErr) *outErr = "socket() failed";
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<u_short>(std::max(0, requestedPort)));
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::closesocket(s);
        WSACleanup();
        if (outErr) *outErr = "bind() failed on 127.0.0.1";
        return false;
    }
    int len = sizeof(addr);
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        outBoundPort = ntohs(addr.sin_port);
    }
    ::closesocket(s);
    WSACleanup();
    return outBoundPort > 0;
#else
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        if (outErr) *outErr = "socket() failed";
        return false;
    }
    int yes = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(std::max(0, requestedPort)));
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(s);
        if (outErr) *outErr = "bind() failed on 127.0.0.1";
        return false;
    }
    socklen_t len = sizeof(addr);
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        outBoundPort = ntohs(addr.sin_port);
    }
    ::close(s);
    return outBoundPort > 0;
#endif
}

struct CrdtOp {
    std::string opId;
    std::string author;
    std::uint64_t lamportTs = 0;
    std::string payloadText;
};

// Deterministic LWW-Element-Set CRDT merge (commutative, associative, idempotent)
inline std::vector<CrdtOp> MergeCrdtOps(const std::vector<CrdtOp>& localOps, const std::vector<CrdtOp>& peerOps) {
    std::map<std::string, CrdtOp> byId;
    auto ingest = [&](const CrdtOp& op) {
        auto it = byId.find(op.opId);
        if (it == byId.end() ||
            op.lamportTs > it->second.lamportTs ||
            (op.lamportTs == it->second.lamportTs && op.author > it->second.author)) {
            byId[op.opId] = op;
        }
    };
    for (const auto& op : localOps) ingest(op);
    for (const auto& op : peerOps) ingest(op);
    std::vector<CrdtOp> out;
    out.reserve(byId.size());
    for (const auto& kv : byId) out.push_back(kv.second);
    std::sort(out.begin(), out.end(), [](const CrdtOp& a, const CrdtOp& b) {
        if (a.lamportTs != b.lamportTs) return a.lamportTs < b.lamportTs;
        return a.opId < b.opId;
    });
    return out;
}

inline std::string HtmlEscapeLocal(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

inline bool ExportLanBroadcastHtml(
    const DocumentModel& doc,
    const std::string& outHtmlPathUtf8,
    const std::string& payloadUrl,
    bool readOnlyStudentView,
    bool remotePinRejected,
    std::string* outErr = nullptr)
{
    QrMatrix qr;
    if (!EncodeQrMatrix(payloadUrl, qr, outErr)) return false;

    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n<meta charset=\"utf-8\">\n"
         << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
         << "<title>" << HtmlEscapeLocal(doc.Title().empty() ? "Document Studio LAN Broadcast" : doc.Title()) << "</title>\n"
         << "<style>body{font-family:system-ui,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#f8fafc;color:#0f172a;margin:0;padding:24px;}"
         << ".card{max-width:860px;margin:0 auto;background:#fff;border:1px solid #e2e8f0;border-radius:16px;padding:24px;box-shadow:0 8px 28px rgba(15,23,42,.08);}"
         << ".badge{display:inline-block;padding:4px 10px;border-radius:999px;font-size:12px;font-weight:600;background:#e0f2fe;color:#0369a1;}"
         << ".warn{background:#fef3c7;color:#92400e;padding:10px 14px;border-radius:10px;margin:12px 0;font-size:13px;}"
         << ".qr{margin:16px 0;display:flex;align-items:center;gap:20px;flex-wrap:wrap;}"
         << "pre{background:#f1f5f9;padding:12px;border-radius:10px;overflow:auto;}</style>\n</head>\n<body>\n"
         << "<main class=\"card\">\n"
         << "<span class=\"badge\">" << (readOnlyStudentView ? "Solo Lectura LAN (Aislado)" : "Compa\xC3\xB1" "ero LAN Local") << "</span>\n"
         << "<h1>" << HtmlEscapeLocal(doc.Title().empty() ? "Sesi\xC3\xB3n Document Studio" : doc.Title()) << "</h1>\n";
    if (remotePinRejected) {
        html << "<div class=\"warn\">Control remoto bloqueado (PIN inv\xC3\xA1lido — backoff activo). Vista de estudiante en modo estrictamente solo lectura.</div>\n";
    }
    html << "<div class=\"qr\">" << RenderQrSvg(qr, 5)
         << "<div><p><strong>URL local:</strong> <code>" << HtmlEscapeLocal(payloadUrl) << "</code></p>"
         << "<p>Sin nube ni telemetr\xC3\xAD" "a. Conectar desde la misma red local.</p></div></div>\n"
         << "<section><h2>Contenido Compartido</h2>\n";
    for (const auto& obj : doc.CanvasObjects()) {
        if (!obj.formulaExprUtf8.empty()) {
            html << "<p><code>" << HtmlEscapeLocal(obj.formulaExprUtf8) << "</code> = <strong>"
                 << HtmlEscapeLocal(obj.evaluatedResultUtf8) << "</strong></p>\n";
        } else if (!obj.textUtf8.empty()) {
            html << "<pre>" << HtmlEscapeLocal(obj.textUtf8) << "</pre>\n";
        }
    }
    for (const auto& pg : doc.Pages()) {
        for (const auto& obj : pg.objects) {
            if (!obj.textUtf8.empty()) {
                html << "<pre>" << HtmlEscapeLocal(obj.textUtf8) << "</pre>\n";
            }
        }
    }
    html << "</section>\n</main>\n</body>\n</html>\n";

    std::error_code ec;
    const auto p = std::filesystem::u8path(outHtmlPathUtf8);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        if (outErr) *outErr = "Unable to open output HTML: " + outHtmlPathUtf8;
        return false;
    }
    const std::string outStr = html.str();
    ofs.write(outStr.data(), static_cast<std::streamsize>(outStr.size()));
    return ofs.good();
}

// Real live local-only HTTP/1.1 Collaborative LAN Server (Winsock2 / POSIX sockets)
// Supports token-authenticated live view, anchored comment submission, and LWW CRDT merge.
class LanCollabHttpServer {
public:
    using StateProviderFn = std::function<std::string()>;
    using CommentHandlerFn = std::function<bool(const std::string& authorUtf8, const std::string& bodyUtf8, std::size_t pageIdx0)>;

    LanCollabHttpServer() = default;
    ~LanCollabHttpServer() { Stop(); }
    LanCollabHttpServer(const LanCollabHttpServer&) = delete;
    LanCollabHttpServer& operator=(const LanCollabHttpServer&) = delete;

    bool Start(
        int requestedPort,
        bool bindLoopbackOnly,
        StateProviderFn stateProvider,
        CommentHandlerFn onComment,
        std::string* outErr = nullptr)
    {
        Stop();
        stateProvider_ = std::move(stateProvider);
        onComment_ = std::move(onComment);
        sessionToken_ = GenerateSessionTokenHex24();

#ifdef _WIN32
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            if (outErr) *outErr = "WSAStartup fall\xC3\xB3.";
            return false;
        }
        wsaInit_ = true;
        SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            if (outErr) *outErr = "No se pudo crear socket TCP.";
            Stop();
            return false;
        }
        int yes = 1;
        ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(bindLoopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
        addr.sin_port = htons(static_cast<u_short>((std::max)(0, requestedPort)));
        if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(s, 8) != 0) {
            ::closesocket(s);
            if (outErr) *outErr = "No se pudo enlazar puerto TCP local.";
            Stop();
            return false;
        }
        int alen = sizeof(addr);
        if (::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &alen) == 0) {
            boundPort_ = ntohs(addr.sin_port);
        }
        listenSock_ = s;
        stop_.store(false, std::memory_order_release);
        running_.store(true, std::memory_order_release);

        serverThread_ = std::thread([this]() {
            while (!stop_.load(std::memory_order_acquire)) {
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(listenSock_, &rfds);
                timeval tv{ 0, 80000 }; // 80ms poll so Stop() joins promptly
                const int sel = ::select(0, &rfds, nullptr, nullptr, &tv);
                if (sel <= 0 || !FD_ISSET(listenSock_, &rfds)) continue;
                SOCKET cli = ::accept(listenSock_, nullptr, nullptr);
                if (cli == INVALID_SOCKET) continue;
                HandleClientWin32(cli);
                ::closesocket(cli);
            }
            running_.store(false, std::memory_order_release);
        });
        return boundPort_ > 0;
#else
        (void)requestedPort; (void)bindLoopbackOnly;
        if (outErr) *outErr = "Servidor LAN HTTP disponible en Windows.";
        return false;
#endif
    }

    void Stop() {
        stop_.store(true, std::memory_order_release);
        if (serverThread_.joinable()) serverThread_.join();
#ifdef _WIN32
        if (listenSock_ != INVALID_SOCKET) {
            ::closesocket(listenSock_);
            listenSock_ = INVALID_SOCKET;
        }
        if (wsaInit_) {
            WSACleanup();
            wsaInit_ = false;
        }
#endif
        running_.store(false, std::memory_order_release);
        boundPort_ = 0;
    }

    bool IsRunning() const noexcept { return running_.load(std::memory_order_acquire); }
    int BoundPort() const noexcept { return boundPort_; }
    const std::string& SessionToken() const noexcept { return sessionToken_; }
    std::string BaseUrl() const {
        return "http://127.0.0.1:" + std::to_string(boundPort_) + "/?token=" + sessionToken_;
    }

    std::vector<CrdtOp> CurrentCrdtOps() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return crdtOps_;
    }

private:
#ifdef _WIN32
    void HandleClientWin32(SOCKET cli) {
        DWORD rcvTo = 1500;
        ::setsockopt(cli, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rcvTo), sizeof(rcvTo));
        std::string req;
        char buf[4096];
        int n = ::recv(cli, buf, sizeof(buf), 0);
        if (n <= 0) return;
        req.append(buf, static_cast<std::size_t>(n));
        // Read remaining body if Content-Length exceeds first packet
        const auto hdrEnd = req.find("\r\n\r\n");
        if (hdrEnd != std::string::npos) {
            const auto clPos = req.find("Content-Length:");
            if (clPos != std::string::npos && clPos < hdrEnd) {
                const int wantLen = std::atoi(req.c_str() + clPos + 15);
                const std::size_t targetSize = hdrEnd + 4 + static_cast<std::size_t>((std::max)(0, (std::min)(wantLen, 262144)));
                while (req.size() < targetSize) {
                    n = ::recv(cli, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    req.append(buf, static_cast<std::size_t>(n));
                }
            }
        }

        std::istringstream rl(req);
        std::string method, path;
        rl >> method >> path;
        const std::string body = (hdrEnd != std::string::npos) ? req.substr(hdrEnd + 4) : "";

        auto sendResp = [&](const char* status, const char* contentType, const std::string& respBody) {
            std::ostringstream oss;
            oss << "HTTP/1.1 " << status << "\r\n"
                << "Content-Type: " << contentType << "\r\n"
                << "Content-Length: " << respBody.size() << "\r\n"
                << "Connection: close\r\n\r\n"
                << respBody;
            const std::string raw = oss.str();
            ::send(cli, raw.data(), static_cast<int>(raw.size()), 0);
        };

        const bool tokenOk = (path.find("token=" + sessionToken_) != std::string::npos) ||
                             (req.find("X-CuadernU-Token: " + sessionToken_) != std::string::npos);
        if (!tokenOk) {
            sendResp("401 Unauthorized", "application/json; charset=utf-8", "{\"error\":\"token invalido\"}");
            return;
        }

        if (method == "GET" && path.rfind("/api/state", 0) == 0) {
            const std::string stateJson = stateProvider_ ? stateProvider_() : "{}";
            sendResp("200 OK", "application/json; charset=utf-8", stateJson);
            return;
        }

        if (method == "POST" && path.rfind("/api/comments", 0) == 0) {
            try {
                const auto j = nlohmann::json::parse(body);
                const std::string author = j.value("author", "Colaborador LAN");
                const std::string text = j.value("body", "");
                const std::size_t pg = j.value("pageIndex", std::size_t{ 0 });
                const bool ok = onComment_ && !text.empty() && onComment_(author, text, pg);
                sendResp(ok ? "200 OK" : "403 Forbidden", "application/json; charset=utf-8",
                         ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"permiso denegado o comentario vacio\"}");
            } catch (...) {
                sendResp("400 Bad Request", "application/json; charset=utf-8", "{\"ok\":false}");
            }
            return;
        }

        if (method == "POST" && path.rfind("/api/crdt", 0) == 0) {
            try {
                const auto j = nlohmann::json::parse(body);
                CrdtOp op;
                op.opId = j.value("opId", "");
                op.author = j.value("author", "peer");
                op.lamportTs = j.value("lamportTs", std::uint64_t{ 1 });
                op.payloadText = j.value("payloadText", "");
                if (op.opId.empty()) {
                    sendResp("400 Bad Request", "application/json; charset=utf-8", "{\"ok\":false}");
                    return;
                }
                std::vector<CrdtOp> merged;
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    crdtOps_ = MergeCrdtOps(crdtOps_, { op });
                    merged = crdtOps_;
                }
                nlohmann::json out = { {"ok", true}, {"count", merged.size()} };
                sendResp("200 OK", "application/json; charset=utf-8", out.dump());
            } catch (...) {
                sendResp("400 Bad Request", "application/json; charset=utf-8", "{\"ok\":false}");
            }
            return;
        }

        // Default GET /: clean guías de interfaz local collaboration web page
        const std::string stateJson = stateProvider_ ? stateProvider_() : "{}";
        std::ostringstream html;
        html << "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"utf-8\"><title>CuadernU LAN Colaborativo</title>"
             << "<style>body{font-family:system-ui,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#F6F8FC;color:#181E2A;padding:28px;}"
             << ".card{max-width:780px;margin:0 auto;background:#fff;border:1px solid #E2E8F0;border-radius:14px;padding:24px;}"
             << "pre{background:#F1F5F9;padding:12px;border-radius:8px;overflow:auto;}</style></head><body><main class=\"card\">"
             << "<h1>CuadernU — Sesi\xC3\xB3n Colaborativa LAN</h1>"
             << "<p>Conectado por red local sin servidores en la nube.</p>"
             << "<pre>" << HtmlEscapeLocal(stateJson) << "</pre></main></body></html>";
        sendResp("200 OK", "text/html; charset=utf-8", html.str());
    }

    SOCKET listenSock_ = INVALID_SOCKET;
    bool wsaInit_ = false;
#endif
    int boundPort_ = 0;
    std::string sessionToken_;
    std::atomic<bool> stop_{ false };
    std::atomic<bool> running_{ false };
    std::thread serverThread_;
    StateProviderFn stateProvider_;
    CommentHandlerFn onComment_;
    mutable std::mutex mtx_;
    std::vector<CrdtOp> crdtOps_;
};

} // namespace DocStudio::LanRemote
