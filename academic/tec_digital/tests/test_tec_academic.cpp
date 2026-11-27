#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/canon.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/material_indexer.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/public_guia_client.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/service.hpp"
#include "academic/tec_digital/storage.hpp"
#include "academic/tec_digital/tda_tds_client.hpp"
#include "academic/tec_digital/types.hpp"
#include "academic/tec_digital/vault.hpp"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace academic::tec_digital;

namespace {

int gPasses = 0;
int gFailures = 0;

void Check(bool cond, const char* expr, int line) {
    if (!cond) {
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
        ++gFailures;
    } else {
        ++gPasses;
    }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

struct ScopedTempDir {
    fs::path dir;
    explicit ScopedTempDir(const std::string& tag) {
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        dir = fs::temp_directory_path() / ("tec_academic_m33_" + tag + "_" + std::to_string(ts));
        std::error_code ec;
        fs::create_directories(dir, ec);
    }
    ~ScopedTempDir() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

void SetEnvVar(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value == nullptr || value[0] == '\0') {
        ::unsetenv(name);
    } else {
        ::setenv(name, value, 1);
    }
#endif
}

class MockHttpTransport final : public IHttpTransport {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;
    explicit MockHttpTransport(Handler h) : handler_(std::move(h)) {}

    HttpResponse Execute(const HttpRequest& req) override {
        {
            std::lock_guard<std::mutex> lk(mu_);
            recorded.push_back(req);
        }
        return handler_(req);
    }

    mutable std::mutex mu_;
    std::vector<HttpRequest> recorded;

private:
    Handler handler_;
};

class LoopbackHttpServer {
public:
    using Handler = std::function<std::string(const std::string& method,
                                              const std::string& target,
                                              const std::string& headers_lower,
                                              const std::string& body)>;

    explicit LoopbackHttpServer(Handler h) : handler_(std::move(h)) {}
    ~LoopbackHttpServer() { Stop(); }

    bool Start() {
#ifdef _WIN32
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        wsa_started_ = true;
        listen_sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock_ == INVALID_SOCKET) return false;
#else
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return false;
        int yes = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        listen_fd_.store(fd);
#endif
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
#ifdef _WIN32
        if (::bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
        int alen = sizeof(addr);
        if (::getsockname(listen_sock_, reinterpret_cast<sockaddr*>(&addr), &alen) != 0) return false;
        port_ = ntohs(addr.sin_port);
        if (::listen(listen_sock_, 32) != 0) return false;
#else
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
        socklen_t alen = sizeof(addr);
        if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &alen) != 0) return false;
        port_ = ntohs(addr.sin_port);
        if (::listen(fd, 32) != 0) return false;
#endif
        running_.store(true);
        thread_ = std::thread([this]() { ServeLoop(); });
        return true;
    }

    void Stop() {
        if (!running_.exchange(false)) return;
#ifdef _WIN32
        if (listen_sock_ != INVALID_SOCKET) {
            ::closesocket(listen_sock_);
            listen_sock_ = INVALID_SOCKET;
        }
        if (thread_.joinable()) thread_.join();
        if (wsa_started_) {
            WSACleanup();
            wsa_started_ = false;
        }
#else
        int fd = listen_fd_.exchange(-1);
        if (fd >= 0) {
            ::shutdown(fd, SHUT_RDWR);
        }
        if (thread_.joinable()) thread_.join();
        if (fd >= 0) {
            ::close(fd);
        }
#endif
    }

    std::string BaseUrl() const { return "http://127.0.0.1:" + std::to_string(port_); }

private:
    void ServeLoop() {
        while (running_.load()) {
#ifdef _WIN32
            SOCKET c = ::accept(listen_sock_, nullptr, nullptr);
            if (c == INVALID_SOCKET) break;
            HandleConn(
                [c](char* b, size_t cap) { return ::recv(c, b, static_cast<int>(cap), 0); },
                [c](const char* b, size_t len) { return ::send(c, b, static_cast<int>(len), 0); });
            ::closesocket(c);
#else
            int fd = listen_fd_.load();
            if (fd < 0) break;
            int c = ::accept(fd, nullptr, nullptr);
            if (c < 0) break;
            HandleConn(
                [c](char* b, size_t cap) { return static_cast<int>(::recv(c, b, cap, 0)); },
                [c](const char* b, size_t len) { return static_cast<int>(::send(c, b, len, 0)); });
            ::close(c);
#endif
        }
    }

    template <typename RecvFn, typename SendFn>
    void HandleConn(RecvFn recv_fn, SendFn send_fn) {
        std::string raw;
        char buf[4096];
        size_t hdr_end = std::string::npos;
        while ((hdr_end = raw.find("\r\n\r\n")) == std::string::npos) {
            int n = recv_fn(buf, sizeof(buf));
            if (n <= 0) return;
            raw.append(buf, static_cast<size_t>(n));
            if (raw.size() > 65536) return;
        }
        std::string hdr = raw.substr(0, hdr_end);
        std::string body = raw.substr(hdr_end + 4);
        std::istringstream iss(hdr);
        std::string req_line;
        std::getline(iss, req_line);
        if (!req_line.empty() && req_line.back() == '\r') req_line.pop_back();
        std::istringstream rls(req_line);
        std::string method, target;
        rls >> method >> target;

        size_t cl = 0;
        std::string line, headers_low;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::string low = AsciiLower(line);
            headers_low += low + "\n";
            if (low.rfind("content-length:", 0) == 0) {
                cl = static_cast<size_t>(std::strtoull(line.c_str() + 15, nullptr, 10));
            }
        }
        while (body.size() < cl) {
            int n = recv_fn(buf, sizeof(buf));
            if (n <= 0) break;
            body.append(buf, static_cast<size_t>(n));
        }
        if (body.size() > cl) body.resize(cl);

        std::string resp = handler_(method, target, headers_low, body);
        size_t sent = 0;
        while (sent < resp.size()) {
            int n = send_fn(resp.data() + sent, resp.size() - sent);
            if (n <= 0) break;
            sent += static_cast<size_t>(n);
        }
    }

    Handler handler_;
    uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::thread thread_;
#ifdef _WIN32
    SOCKET listen_sock_ = INVALID_SOCKET;
    bool wsa_started_ = false;
#else
    std::atomic<int> listen_fd_{-1};
#endif
};

// Minimal synthetic ZIP writer using stored (method 0) or bit-3 data descriptor entries
void PushU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
void PushU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

uint32_t Crc32Simple(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
    }
    return ~crc;
}

uint32_t Adler32Simple(const uint8_t* data, size_t len) {
    uint32_t s1 = 1, s2 = 0;
    for (size_t i = 0; i < len; ++i) {
        s1 = (s1 + data[i]) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    return (s2 << 16) | s1;
}

// Wraps payload in a valid RFC 1950 zlib stored block (BTYPE=0)
std::vector<uint8_t> MakeZlibStored(std::string_view text) {
    std::vector<uint8_t> z;
    z.push_back(0x78);
    z.push_back(0x9C);
    // Single BFINAL=1, BTYPE=0 stored block (len <= 65535)
    uint16_t len = static_cast<uint16_t>(text.size());
    uint16_t nlen = static_cast<uint16_t>(~len);
    z.push_back(0x01);
    z.push_back(static_cast<uint8_t>(len & 0xFF));
    z.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    z.push_back(static_cast<uint8_t>(nlen & 0xFF));
    z.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
    z.insert(z.end(), text.begin(), text.end());
    uint32_t adl = Adler32Simple(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    z.push_back(static_cast<uint8_t>((adl >> 24) & 0xFF));
    z.push_back(static_cast<uint8_t>((adl >> 16) & 0xFF));
    z.push_back(static_cast<uint8_t>((adl >> 8) & 0xFF));
    z.push_back(static_cast<uint8_t>(adl & 0xFF));
    return z;
}

struct ZipBuildEntry {
    std::string name;
    std::vector<uint8_t> content;
    uint16_t method = 0;
    bool bit3_data_descriptor = false;
};

std::vector<uint8_t> BuildSyntheticZip(const std::vector<ZipBuildEntry>& entries) {
    std::vector<uint8_t> zip;
    struct CdRec {
        std::string name;
        uint16_t flags = 0;
        uint16_t method = 0;
        uint32_t crc = 0;
        uint32_t comp_sz = 0;
        uint32_t uncomp_sz = 0;
        uint32_t lfh_off = 0;
    };
    std::vector<CdRec> cds;

    for (const auto& e : entries) {
        CdRec cd;
        cd.name = e.name;
        cd.flags = e.bit3_data_descriptor ? 0x0008u : 0u;
        cd.method = e.method;
        cd.crc = Crc32Simple(e.content.data(), e.content.size());
        const auto& payload = e.content;
        cd.comp_sz = static_cast<uint32_t>(payload.size());
        cd.uncomp_sz = static_cast<uint32_t>(e.content.size());
        cd.lfh_off = static_cast<uint32_t>(zip.size());

        // Local File Header
        PushU32(zip, 0x04034B50u);
        PushU16(zip, 20);
        PushU16(zip, cd.flags);
        PushU16(zip, cd.method);
        PushU16(zip, 0);
        PushU16(zip, 0);
        PushU32(zip, e.bit3_data_descriptor ? 0u : cd.crc);
        PushU32(zip, e.bit3_data_descriptor ? 0u : cd.comp_sz);
        PushU32(zip, e.bit3_data_descriptor ? 0u : cd.uncomp_sz);
        PushU16(zip, static_cast<uint16_t>(e.name.size()));
        PushU16(zip, 0);
        zip.insert(zip.end(), e.name.begin(), e.name.end());
        zip.insert(zip.end(), payload.begin(), payload.end());

        if (e.bit3_data_descriptor) {
            PushU32(zip, 0x08074B50u);
            PushU32(zip, cd.crc);
            PushU32(zip, cd.comp_sz);
            PushU32(zip, cd.uncomp_sz);
        }
        cds.push_back(std::move(cd));
    }

    uint32_t cd_off = static_cast<uint32_t>(zip.size());
    for (const auto& cd : cds) {
        PushU32(zip, 0x02014B50u);
        PushU16(zip, 20);
        PushU16(zip, 20);
        PushU16(zip, cd.flags);
        PushU16(zip, cd.method);
        PushU16(zip, 0);
        PushU16(zip, 0);
        PushU32(zip, cd.crc);
        PushU32(zip, cd.comp_sz);
        PushU32(zip, cd.uncomp_sz);
        PushU16(zip, static_cast<uint16_t>(cd.name.size()));
        PushU16(zip, 0);
        PushU16(zip, 0);
        PushU16(zip, 0);
        PushU16(zip, 0);
        PushU32(zip, 0);
        PushU32(zip, cd.lfh_off);
        zip.insert(zip.end(), cd.name.begin(), cd.name.end());
    }
    uint32_t cd_sz = static_cast<uint32_t>(zip.size()) - cd_off;

    // EOCD
    PushU32(zip, 0x06054B50u);
    PushU16(zip, 0);
    PushU16(zip, 0);
    PushU16(zip, static_cast<uint16_t>(cds.size()));
    PushU16(zip, static_cast<uint16_t>(cds.size()));
    PushU32(zip, cd_sz);
    PushU32(zip, cd_off);
    PushU16(zip, 0);
    return zip;
}

// =========================================================================
// 1. M3.1 Hardening Tests
// =========================================================================
void TestM31Hardening() {
    Sanitizer san;

    // Complete bracketed marker enforcement (cannot bypass by prefixing [REDACTED...)
    std::string s1 = san.SanitizeText("Bearer [REDACTED]my_secret_suffix");
    CHECK(s1.find("my_secret_suffix") == std::string::npos);
    CHECK(s1 == "Bearer [REDACTED]");

    std::string s2 = san.SanitizeText("Authorization: Bearer [REDACTED]leaked_tail");
    CHECK(s2.find("leaked_tail") == std::string::npos);

    std::string s3 = san.SanitizeText("Authorization: Bearer [REDACTED:sha256=1234abcd]");
    CHECK(s3 == "Authorization: Bearer [REDACTED:sha256=1234abcd]");

    std::string s4 = san.SanitizeUrl("https://1drv.ms/[REDACTED_SHARE_0123456789ab]");
    CHECK(s4 == "https://1drv.ms/[REDACTED_SHARE_0123456789ab]");

    std::string s5 = san.SanitizeUrl("https://1drv.ms/[REDACTED_SHARE_123]extra_secret");
    CHECK(s5.find("extra_secret") == std::string::npos);

    // ScanReusableTokens must catch crafted [REDACTED...]suffix tokens
    CHECK(!san.ScanReusableTokens("Bearer [REDACTED]actual_token_123456").empty());
    CHECK(san.ScanReusableTokens("Bearer [REDACTED:sha256=abcdef12]").empty());

    // SPA fragment without '?' (#/path/access_token=...)
    std::string spa = san.SanitizeUrl(
        "https://tecdigital.tec.ac.cr/cb#/oauth/callback/access_token=MY_SPA_SECRET&state=ok");
    CHECK(spa.find("MY_SPA_SECRET") == std::string::npos);
    CHECK(spa.find("#/oauth/callback/access_token=[REDACTED") != std::string::npos);
    CHECK(spa.find("state=ok") != std::string::npos);
    CHECK(san.SanitizeUrl(spa) == spa);

    // Strict 1*HEXDIG chunked encoding on loopback server
    LoopbackHttpServer srv([](const std::string&,
                              const std::string& target,
                              const std::string&,
                              const std::string&) -> std::string {
        if (target == "/chunk-0x") {
            return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
                   "0x3\r\nabc\r\n0\r\n\r\n";
        }
        if (target == "/chunk-plus") {
            return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
                   "+3\r\nabc\r\n0\r\n\r\n";
        }
        if (target == "/chunk-neg") {
            return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
                   "-0\r\n\r\n";
        }
        if (target == "/chunk-bws-valid") {
            return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
                   "3 \t;ext=1\r\nabc\r\n0\r\n\r\n";
        }
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    });
    CHECK(srv.Start());
    auto native = CreateNativeHttpTransport();

    {
        HttpRequest req;
        req.url = srv.BaseUrl() + "/chunk-bws-valid";
        HttpResponse resp = native->Execute(req);
        CHECK(resp.transport_ok && resp.status_code == 200);
        CHECK(resp.BodyAsString() == "abc");
    }

#ifndef _WIN32
    for (const char* bad : {"/chunk-0x", "/chunk-plus", "/chunk-neg"}) {
        HttpRequest req;
        req.url = srv.BaseUrl() + bad;
        HttpResponse resp = native->Execute(req);
        CHECK(!resp.transport_ok);
    }
#endif
    srv.Stop();
}

// =========================================================================
// 2. U-07 & U-10: FileStorageWalker, StorageDownloader, CAS _blobs, Manifest
// =========================================================================
void TestStorageWalkerAndDownloader() {
    // 5-tier + SPA HTML root folder extraction (individual tiers + combined deduplication)
    {
        CHECK(FileStorageWalker::ExtractRootFolderIds("<a href='/dotlrn/file-storage/#/153108817'>SPA</a>") ==
              std::vector<std::string>{"153108817"});
        CHECK(FileStorageWalker::ExtractRootFolderIds("<script>var folder_id=445566;</script>") ==
              std::vector<std::string>{"445566"});
        CHECK(FileStorageWalker::ExtractRootFolderIds("<div data-folder-id=\"223344\"></div>") ==
              std::vector<std::string>{"223344"});
        CHECK(FileStorageWalker::ExtractRootFolderIds("<script>var root_folder_id = 334455;</script>") ==
              std::vector<std::string>{"334455"});
        CHECK(FileStorageWalker::ExtractRootFolderIds("<a href='folder-chunk?x=1&folder_id=556677'>C</a>") ==
              std::vector<std::string>{"556677"});
        CHECK(FileStorageWalker::ExtractRootFolderIds("<a href='/dotlrn/file-storage/view?object_id=667788'>V</a>") ==
              std::vector<std::string>{"667788"});

        std::string html =
            "<a href='/dotlrn/file-storage/#/153108817'>SPA</a>"
            "<div data-folder-id=\"223344\"></div>"
            "<script>var root_folder_id = 334455; var folder_id=445566;</script>"
            "<a href='folder-chunk?folder_id=556677'>C</a>"
            "<a href='/dotlrn/file-storage/view?object_id=667788'>V</a>";
        auto ids = FileStorageWalker::ExtractRootFolderIds(html);
        CHECK(ids.size() == 6);
        CHECK(ids[0] == "153108817");
        CHECK(ids[1] == "445566");
        CHECK(ids[2] == "556677");
        CHECK(ids[3] == "223344");
        CHECK(ids[4] == "334455");
        CHECK(ids[5] == "667788");
    }

    // Candidate URL builder never emits %2F and passes ReadOnlyRequestPolicy
    {
        FileRecord fr;
        fr.object_id = "204649355";
        fr.title = "Clase 1 PDF";
        fr.name = "Clase 1.pdf";
        fr.file_url = "../Clases/Semana 1/Clase 1.pdf";
        auto cands = StorageDownloader::BuildCandidateUrls(
            fr, "https://tecdigital.tec.ac.cr",
            "https://tecdigital.tec.ac.cr/dotlrn/classes/E/EL2113/S-2-2026.SC.EL2113.50");
        CHECK(cands.size() >= 4);
        ReadOnlyRequestPolicy policy;
        for (const auto& u : cands) {
            CHECK(u.find("%2F") == std::string::npos);
            CHECK(u.find("%2f") == std::string::npos);
            PolicyRequestContext pctx;
            pctx.url = u;
            pctx.method = "GET";
            CHECK(policy.Evaluate(pctx).allowed);
        }
    }

    // Synthetic valid PDF bytes
    const std::string pdf_bytes_1 =
        "%PDF-1.4\n1 0 obj<<>>endobj\nstream\nBT (Material Uno) Tj ET\nendstream\n%%EOF\n";
    const std::string pdf_bytes_2 =
        "%PDF-1.4\n1 0 obj<<>>endobj\nstream\nBT (Material Dos Resume) Tj ET\nendstream\n%%EOF\n";
    const std::string pdf_bytes_3 =
        "%PDF-1.4\n1 0 obj<<>>endobj\nstream\nBT (Material Tres Colision) Tj ET\nendstream\n%%EOF\n";

    std::atomic<bool> saw_range_zero{false};
    std::atomic<int> retry_503_hits{0};
    std::atomic<bool> portal_alive{true};

    LoopbackHttpServer srv([&](const std::string& method,
                               const std::string& target,
                               const std::string& headers_lower,
                               const std::string&) -> std::string {
        if (headers_lower.find("range: bytes=0-") != std::string::npos) {
            saw_range_zero.store(true);
        }

        if (target == "/dotlrn/") {
            if (portal_alive.load()) {
                return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 42\r\n"
                       "Connection: close\r\n\r\n<html><body>Portal OK Logout</body></html>";
            }
            return "HTTP/1.1 302 Found\r\nLocation: /register/?return_url=/dotlrn/\r\n"
                   "Content-Length: 0\r\nConnection: close\r\n\r\n";
        }

        // Community file-storage HTML
        if (target == "/dotlrn/classes/E/EL2113/S1/file-storage/") {
            std::string h = "<a href='/dotlrn/file-storage/#/100100'>Root</a>";
            return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
                   std::to_string(h.size()) + "\r\nConnection: close\r\n\r\n" + h;
        }

        // Folder tree: root(100100) -> sub(100101) [using "items" object map] -> deep chain > 10 + url link
        if (target == "/dotlrn/file-storage/api/folder/100100") {
            std::string j = R"({
                "object_id": 100100,
                "name": "Root",
                "elements": [
                    {"object_id": 100101, "fs_type": "folder", "name": "Semana 1"},
                    {"object_id": 999, "fs_type": "url", "title": "Enlace Externo", "url": "https://example.org/doc"},
                    {"object_id": 201, "fs_type": "file", "file_upload_name": "guia.pdf", "title": "guia", "content_size": )" +
                            std::to_string(pdf_bytes_1.size()) + R"(},
                    {"object_id": 202, "fs_type": "file", "file_upload_name": "copia.pdf", "title": "copia", "content_size": )" +
                            std::to_string(pdf_bytes_1.size()) + R"(},
                    {"object_id": 203, "fs_type": "file", "file_upload_name": "corrupto.pdf", "title": "corrupto"},
                    {"object_id": 204, "fs_type": "file", "file_upload_name": "perdido.pdf", "title": "perdido"},
                    {"object_id": 205, "fs_type": "file", "file_upload_name": "restringido.pdf", "title": "restringido"}
                ]
            })";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(j.size()) + "\r\nConnection: close\r\n\r\n" + j;
        }

        if (target == "/dotlrn/file-storage/api/folder/100101") {
            // Tests "items" as a JSON object map + self-loop (100101) + cycle (100100) + depth chain
            std::string j = R"({
                "object_id": 100101,
                "name": "Semana 1",
                "items": {
                    "0": {"object_id": 100101, "fs_type": "folder", "name": "Self"},
                    "1": {"object_id": 100100, "fs_type": "folder", "name": "Cycle"},
                    "2": {"object_id": 100102, "fs_type": "folder", "name": "D2"},
                    "3": {"object_id": 206, "fs_type": "file", "file_upload_name": "guia.pdf", "title": "guia_v2"}
                }
            })";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(j.size()) + "\r\nConnection: close\r\n\r\n" + j;
        }

        // Depth 2..12 chain to verify max_depth=10 cutoff
        if (target.rfind("/dotlrn/file-storage/api/folder/1001", 0) == 0) {
            std::string id_str = target.substr(std::strlen("/dotlrn/file-storage/api/folder/"));
            int id_num = std::atoi(id_str.c_str());
            if (id_num >= 100102 && id_num <= 100115) {
                std::string j = R"({"object_id": )" + std::to_string(id_num) +
                                R"(, "name": "D)" + std::to_string(id_num - 100100) +
                                R"(", "contents": [{"object_id": )" + std::to_string(id_num + 1) +
                                R"(, "fs_type": "folder", "name": "Next"}]})";
                return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                       std::to_string(j.size()) + "\r\nConnection: close\r\n\r\n" + j;
            }
        }

        if (target == "/dotlrn/file-storage/api/file/201") {
            std::string j = R"({"object_id":201,"parent_id":100,"mime_type":"application/pdf","name":"guia.pdf","content_size":77,"fs_type":"file","author":{"id":"1","name":"Prof"}})";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(j.size()) + "\r\nConnection: close\r\n\r\n" + j;
        }

        // File 201: 503 on 1st attempt, then 200 OK
        if (target.find("file_id=201") != std::string::npos) {
            if (retry_503_hits.fetch_add(1) == 0) {
                return "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 4\r\nConnection: close\r\n\r\nbusy";
            }
            return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Length: " +
                   std::to_string(pdf_bytes_1.size()) + "\r\nConnection: close\r\n\r\n" + pdf_bytes_1;
        }

        // File 202: identical bytes -> CAS deduplicated
        if (target.find("file_id=202") != std::string::npos) {
            return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Length: " +
                   std::to_string(pdf_bytes_1.size()) + "\r\nConnection: close\r\n\r\n" + pdf_bytes_1;
        }

        // File 203: magic-byte mismatch (.pdf returning plain text garbage -> .bad quarantine)
        if (target.find("file_id=203") != std::string::npos) {
            std::string junk = "NOT A PDF FILE ERROR";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Length: " +
                   std::to_string(junk.size()) + "\r\nConnection: close\r\n\r\n" + junk;
        }

        // File 204: 404 across all candidates -> Orphan404
        if (target.find("file_id=204") != std::string::npos || target == "/dotlrn/file-storage/view/204") {
            return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        }

        // File 205: 403 while portal alive -> Failed (NOT SessionExpired)
        if (target.find("file_id=205") != std::string::npos) {
            return "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\nConnection: close\r\n\r\nForbidden";
        }

        // File 206: resume test! If Range: bytes=8- -> 206 Partial Content; also test 200 OK fallback on /resume-200
        if (target.find("file_id=206") != std::string::npos) {
            if (method == "HEAD") {
                return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(pdf_bytes_2.size()) +
                       "\r\nConnection: close\r\n\r\n";
            }
            if (headers_lower.find("range: bytes=8-") != std::string::npos) {
                std::string tail = pdf_bytes_2.substr(8);
                return "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 8-" +
                       std::to_string(pdf_bytes_2.size() - 1) + "/" +
                       std::to_string(pdf_bytes_2.size()) + "\r\nContent-Length: " +
                       std::to_string(tail.size()) + "\r\nConnection: close\r\n\r\n" + tail;
            }
            return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Length: " +
                   std::to_string(pdf_bytes_2.size()) + "\r\nConnection: close\r\n\r\n" + pdf_bytes_2;
        }

        // 200 OK fallback when start_byte > 0 (must truncate .part rather than appending)
        if (target == "/dotlrn/file-storage/download?file_id=207") {
            if (method == "HEAD") {
                return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(pdf_bytes_3.size()) +
                       "\r\nConnection: close\r\n\r\n";
            }
            return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Length: " +
                   std::to_string(pdf_bytes_3.size()) + "\r\nConnection: close\r\n\r\n" + pdf_bytes_3;
        }

        // Login HTML returned on 200 OK -> delete .part + SessionExpired
        if (target == "/dotlrn/file-storage/download?file_id=208") {
            std::string login = "<!DOCTYPE html><html><body><form id=\"login-form\"></form></body></html>";
            return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
                   std::to_string(login.size()) + "\r\nConnection: close\r\n\r\n" + login;
        }

        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    });
    CHECK(srv.Start());

    PolicyOptions popts;
    popts.allowed_test_origins = {srv.BaseUrl()};
    ReadOnlyRequestPolicy policy(popts);
    auto native = CreateNativeHttpTransport();
    PolicyEnforcingTransport guarded(*native, policy);

    AuthClientOptions aopts;
    aopts.base_url = srv.BaseUrl();
    aopts.perform_prelogin_handshake = false;
    AuthClient auth(guarded, policy, aopts);

    FileStorageWalker walker(guarded, auth, srv.BaseUrl());
    auto roots = walker.DiscoverRootFolderIds("/dotlrn/classes/E/EL2113/S1");
    CHECK(roots.ok && !roots.value.empty() && roots.value[0] == "100100");

    auto fmeta = walker.FetchFileDetails("201");
    CHECK(fmeta.ok && fmeta.value.object_id == 201 && fmeta.value.author_name == "Prof");

    WalkerOptions wopts;
    wopts.max_depth = 10;
    auto walked = walker.WalkFolderTree("100100", "S1", "CLASE", "/dotlrn/classes/E/EL2113/S1", wopts);
    CHECK(walked.ok);
    // Depth 0..10 inclusive = 11 folders (100..110), folder 111 at depth 11 stopped
    CHECK(walked.value.folders.size() == 11);
    CHECK(walked.value.url_links.size() == 1);
    CHECK(walked.value.url_links[0].url == "https://example.org/doc");
    CHECK(walked.value.files.size() == 6);  // 201..206 (url_link NOT queued as file)

    ScopedTempDir tmp("storage");
    DownloaderOptions dopts;
    dopts.min_delay_ms = 0;
    dopts.backoff_ms = {1, 2};
    dopts.max_retries = 2;
    StorageDownloader dl(guarded, auth, srv.BaseUrl(), dopts);

    // Pre-create 8-byte .part for file 206 to verify Range: bytes=8- resume
    fs::create_directories(tmp.dir / "downloads" / "_tmp");
    {
        std::ofstream ofs(tmp.dir / "downloads" / "_tmp" / "206.part", std::ios::binary);
        ofs.write(pdf_bytes_2.data(), 8);
    }

    // Also ensure file 206 collides in logical path if placed in same folder as 201, or test collision explicitly:
    std::map<std::string, DownloadResult> by_id;
    for (auto& fr : walked.value.files) {
        by_id[fr.object_id] = dl.DownloadFile(fr, tmp.dir.u8string(), "/dotlrn/classes/E/EL2113/S1");
    }

    CHECK(!saw_range_zero.load());
    CHECK(by_id["201"].outcome == DownloadOutcome::Downloaded);
    CHECK(by_id["201"].magic_ok);
    CHECK(retry_503_hits.load() >= 2);

    // 202 shares identical bytes with 201 -> deduplicated
    CHECK(by_id["202"].outcome == DownloadOutcome::Deduplicated);
    CHECK(by_id["202"].sha256 == by_id["201"].sha256);

    // 203 fails magic check -> quarantined to .bad, not in _blobs
    CHECK(by_id["203"].outcome == DownloadOutcome::Failed);
    CHECK(!by_id["203"].magic_ok);
    CHECK(!by_id["203"].quarantine_rel_path.empty());
    CHECK(fs::is_regular_file(tmp.dir / fs::u8path(by_id["203"].quarantine_rel_path)));

    // 204 -> Orphan404
    CHECK(by_id["204"].outcome == DownloadOutcome::Orphan404);

    // 205 -> 403 while portal alive -> Failed (session NOT expired)
    CHECK(by_id["205"].outcome == DownloadOutcome::Failed);
    CHECK(!auth.IsSessionExpired());

    // 206 -> Range resumed and intact
    CHECK(by_id["206"].outcome == DownloadOutcome::Downloaded);
    CHECK(by_id["206"].sha256 == Sha256Hex(pdf_bytes_2));

    // Collision suffixing test: download pdf_bytes_2 with same name/folder as 201 (guia.pdf)
    {
        FileRecord col;
        col.object_id = "207";
        col.name = "guia.pdf";
        col.community = "S1";
        col.community_type = "CLASE";
        col.folder_path = "";
        col.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=207"};
        // Pre-seed corrupt 10-byte .part to ALSO verify 200 OK fallback truncates .part cleanly!
        {
            std::ofstream ofs(tmp.dir / "downloads" / "_tmp" / "207.part", std::ios::binary);
            ofs << "GARBAGE_10";
        }
        auto dr207 = dl.DownloadFile(col, tmp.dir.u8string());
        CHECK(dr207.outcome == DownloadOutcome::Downloaded);
        CHECK(dr207.sha256 == Sha256Hex(pdf_bytes_3));
        CHECK(dr207.logical_rel_path.find("_" + dr207.sha256.substr(0, 8) + ".pdf") !=
              std::string::npos);
    }

    // HTML login redirect deletes .part and marks SessionExpired
    {
        FileRecord f208;
        f208.object_id = "208";
        f208.name = "secret.pdf";
        f208.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=208"};
        auto dr208 = dl.DownloadFile(f208, tmp.dir.u8string());
        CHECK(dr208.outcome == DownloadOutcome::SessionExpired);
        CHECK(auth.IsSessionExpired());
        CHECK(!fs::exists(tmp.dir / "downloads" / "_tmp" / "208.part"));
    }

    // Manifest save & load
    MirrorManifestStore mstore;
    CHECK(mstore.Save(tmp.dir.u8string(), walked.value.files, walked.value.folders, {},
                      walked.value.url_links).ok);
    auto loaded = mstore.LoadFilesManifest(tmp.dir.u8string());
    CHECK(loaded.ok && loaded.value.size() == walked.value.files.size());

    // Stall abort preserves .part and retries with configurable backoff
    {
        auth.ResetSessionExpired();
        MockHttpTransport stall_mock([](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.final_url = req.url;
            r.status_code = 200;
            r.has_content_length = true;
            r.content_length = 1000;
            if (req.header_callback) req.header_callback(r);
            const uint8_t partial[16] = {'%', 'P', 'D', 'F', '-', '1', '.', '4', '\n', '0', '1', '2', '3', '4', '5', '6'};
            if (req.chunk_callback && !req.chunk_callback(partial, sizeof(partial))) {
                r.transport_ok = false;
                r.error_message = "aborted by chunk_callback";
            }
            return r;
        });
        PolicyEnforcingTransport stall_guarded(stall_mock, policy);
        DownloaderOptions stall_opts;
        stall_opts.min_delay_ms = 0;
        stall_opts.backoff_ms = {1};
        stall_opts.max_retries = 1;
        stall_opts.stall_grace_ms = 0;
        stall_opts.stall_min_bps = 1e12;
        stall_opts.verify_size_with_head = false;
        StorageDownloader stall_dl(stall_guarded, auth, srv.BaseUrl(), stall_opts);

        FileRecord sf;
        sf.object_id = "209_stall";
        sf.name = "stall.pdf";
        sf.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=209"};
        auto sres = stall_dl.DownloadFile(sf, tmp.dir.u8string());
        CHECK(sres.outcome == DownloadOutcome::Failed);
        CHECK(sres.error_message.find("stall") != std::string::npos);
        CHECK(fs::is_regular_file(tmp.dir / "downloads" / "_tmp" / "209_stall.part"));
        CHECK(stall_mock.recorded.size() == 2);  // initial + 1 retry
    }

    srv.Stop();
}

// =========================================================================
// 3. U-11: Pure-HTTPS Headless BFS Crawler
// =========================================================================
void TestHeadlessCrawler() {
    std::atomic<bool> saw_mutating_hit{false};

    LoopbackHttpServer srv([&](const std::string&,
                               const std::string& target,
                               const std::string&,
                               const std::string&) -> std::string {
        if (target.find("logout") != std::string::npos ||
            target.find("delete") != std::string::npos) {
            saw_mutating_hit.store(true);
        }

        if (target == "/dotlrn/index") {
            std::string h =
                "<html><head><title>Portal TEC</title></head><body>"
                "<a href=\"/dotlrn/classes/E/EL2113/S1/one-community?a=1&amp;b=2\">Curso EL2113</a>"
                "<a href=\"/register/logout\">Salir</a>"
                "<a href=\"/dotlrn/safe-action\">Dar de baja</a>"
                "<a href=\"javascript:void(0)\">JS</a>"
                "<a href=\"/dotlrn/file-storage/view/apuntes.pdf\">Apuntes PDF</a>"
                "<form action=\"/search\" method=\"get\">"
                "  <input type=\"text\" name=\"q\" required>"
                "  <input type=\"password\" name=\"secret_field\" value=\"DO_NOT_SAVE\">"
                "</form>"
                "<script>var u='/dotlrn/file-storage/api/folder/90001'; var s='file-storage/#/90002';</script>"
                "</body></html>";
            return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
                   std::to_string(h.size()) + "\r\nConnection: close\r\n\r\n" + h;
        }
        if (target == "/dotlrn/classes/E/EL2113/S1/one-community?a=1&b=2") {
            std::string h =
                "<html><head><title>EL2113</title></head><body>"
                "<a href=\"/dotlrn/calendar/view?date=2026-09-01&period_days=7\">Cal1</a>"
                "<a href=\"/dotlrn/calendar/view?date=2026-09-15&period_days=30\">Cal2</a>"
                "<a href=\"/dotlrn/calendar/view?date=2026-10-01\">Cal3</a>"
                "<a href=\"/dotlrn/calendar/view?date=2026-11-01\">Cal4</a>"
                "</body></html>";
            return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
                   std::to_string(h.size()) + "\r\nConnection: close\r\n\r\n" + h;
        }
        if (target == "/dotlrn/file-storage/api/folder/90001") {
            std::string j = R"({"object_id":90001,"name":"F90001","elements":[{"object_id":555,"fs_type":"file","file_upload_name":"lab.pdf"}]})";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(j.size()) + "\r\nConnection: close\r\n\r\n" + j;
        }
        if (target == "/dotlrn/file-storage/api/folder/90002") {
            std::string j = R"({"object_id":90002,"name":"F90002","elements":[]})";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(j.size()) + "\r\nConnection: close\r\n\r\n" + j;
        }
        if (target.rfind("/dotlrn/calendar/view", 0) == 0) {
            std::string h = "<html><title>Calendario</title><body>OK</body></html>";
            return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
                   std::to_string(h.size()) + "\r\nConnection: close\r\n\r\n" + h;
        }
        if (target.rfind("/dotlrn/broken-", 0) == 0) {
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 3\r\nConnection: close\r\n\r\nerr";
        }
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    });
    CHECK(srv.Start());

    PolicyOptions popts;
    popts.allowed_test_origins = {srv.BaseUrl()};
    ReadOnlyRequestPolicy policy(popts);
    auto native = CreateNativeHttpTransport();
    PolicyEnforcingTransport guarded(*native, policy);

    AuthClientOptions aopts;
    aopts.base_url = srv.BaseUrl();
    AuthClient auth(guarded, policy, aopts);

    ScopedTempDir tmp("crawler");
    CrawlerOptions copts;
    copts.base_url = srv.BaseUrl();
    copts.delay_ms = 0;
    copts.calendar_date_cap = 2;  // Cal1 & Cal2 collapse via BUG-059; Cal3 hits cap=2; Cal4 skipped!
    copts.download_discovered_files = false;
    copts.seed_urls = {srv.BaseUrl() + "/dotlrn/index"};

    HeadlessCrawler crawler(guarded, auth, copts);
    auto res = crawler.Run(tmp.dir.u8string());
    CHECK(res.ok);
    CHECK(!saw_mutating_hit.load());
    CHECK(res.value.pages_skipped_policy >= 2);
    CHECK(res.value.pages_skipped_cap >= 1);
    CHECK(res.value.discovered_folder_ids.size() >= 2);
    CHECK(res.value.files.size() >= 2);  // apuntes.pdf + lab.pdf
    CHECK(res.value.forms.size() == 1);
    CHECK(res.value.forms[0].fields.size() == 2);
    // Verify form values are NEVER captured
    std::ifstream fifs(tmp.dir / "forms_manifest.json");
    std::string forms_txt((std::istreambuf_iterator<char>(fifs)), std::istreambuf_iterator<char>());
    CHECK(forms_txt.find("DO_NOT_SAVE") == std::string::npos);

    // 5xx circuit breaker check
    CrawlerOptions cb_opts;
    cb_opts.base_url = srv.BaseUrl();
    cb_opts.delay_ms = 0;
    cb_opts.consec_5xx_break = 3;
    cb_opts.seed_urls = {
        srv.BaseUrl() + "/dotlrn/broken-1",
        srv.BaseUrl() + "/dotlrn/broken-2",
        srv.BaseUrl() + "/dotlrn/broken-3",
        srv.BaseUrl() + "/dotlrn/index"
    };
    HeadlessCrawler cb_crawler(guarded, auth, cb_opts);
    auto cb_res = cb_crawler.Run();
    CHECK(cb_res.ok && cb_res.value.circuit_broken);
    CHECK(cb_res.value.pages_visited == 0);

    srv.Stop();
}

// =========================================================================
// 4. U-08: Pure C++17 Zero-Dependency Material Indexer
// =========================================================================
void TestMaterialIndexer() {
    // 4a. InflateCodec: Stored (BTYPE=0), Fixed Huffman (BTYPE=1), Dynamic Huffman (BTYPE=2), Bomb Cap, Adler-32
    {
        auto zstored = MakeZlibStored("Hola Inflate Stored!");
        auto dec = InflateCodec::InflateZlib(zstored.data(), zstored.size());
        CHECK(dec.ok);
        CHECK(std::string(dec.value.begin(), dec.value.end()) == "Hola Inflate Stored!");

        // Corrupt Adler-32 fails
        zstored.back() ^= 0xFF;
        CHECK(!InflateCodec::InflateZlib(zstored.data(), zstored.size()).ok);

        // Bomb cap enforced
        auto zstored2 = MakeZlibStored("0123456789ABCDEF");
        CHECK(!InflateCodec::InflateZlib(zstored2.data(), zstored2.size(), 8).ok);

        // Valid RFC 1952 gzip frame from M3.1 test -> raw DEFLATE block inside (bytes 10 .. len-8)
        static constexpr uint8_t kGzipPayload[] = {
            0x1f, 0x8b, 0x08, 0x00, 0xcb, 0xea, 0xfc, 0x6a, 0x02, 0xff, 0xf3,
            0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x70, 0xaf, 0xca, 0x2c, 0x50, 0x08,
            0x71, 0x75, 0x56, 0x70, 0xc9, 0x4c, 0xcf, 0x2c, 0x49, 0xcc, 0x51,
            0x04, 0x00, 0xf8, 0x39, 0xcf, 0xed, 0x17, 0x00, 0x00, 0x00
        };
        auto raw_dec = InflateCodec::InflateRawDeflate(kGzipPayload + 10, sizeof(kGzipPayload) - 18);
        CHECK(raw_dec.ok);
        CHECK(std::string(raw_dec.value.begin(), raw_dec.value.end()) == "Hello Gzip TEC Digital!");

        // Dynamic Huffman (BTYPE=2) zlib stream
        static constexpr uint8_t kDynamicZlib[] = {
            0x78, 0xda, 0x9d, 0xd4, 0x31, 0x0e, 0xc2, 0x30, 0x10, 0x44, 0xd1, 0x9e, 0x53, 0x2c, 0x37,
            0xf0, 0xac, 0x63, 0x27, 0xa1, 0x45, 0x74, 0x2e, 0xb9, 0x80, 0x95, 0x04, 0x29, 0x12, 0x24,
            0xc8, 0x21, 0x05, 0xb7, 0x07, 0x21, 0x79, 0xe9, 0xe7, 0x00, 0xbf, 0x7b, 0xfa, 0x69, 0x5e,
            0xa6, 0x2c, 0x38, 0xc9, 0x79, 0x2f, 0xdb, 0x2a, 0x97, 0xa4, 0x80, 0x97, 0xf3, 0x5c, 0x86,
            0x7d, 0x7e, 0xad, 0x9b, 0xbc, 0xe5, 0x5a, 0xf2, 0xb2, 0xdd, 0xd6, 0xf2, 0xc8, 0x63, 0x96,
            0x71, 0x92, 0x94, 0x9f, 0xf7, 0x3c, 0x4c, 0xd2, 0x1e, 0x0f, 0xe9, 0x17, 0x2b, 0x11, 0xa3,
            0xa9, 0xb5, 0x27, 0x6a, 0x45, 0xad, 0x1b, 0xa6, 0xee, 0x6a, 0x1d, 0x88, 0xda, 0x87, 0x5a,
            0x47, 0xa2, 0x6e, 0xb4, 0xd6, 0x2d, 0x53, 0xf7, 0xb5, 0xee, 0x88, 0x3a, 0xc4, 0x5a, 0xf7,
            0x44, 0x1d, 0x7d, 0xad, 0xe1, 0x18, 0x2d, 0xce, 0x72, 0x0a, 0x9b, 0x69, 0x03, 0xc3, 0xad,
            0x33, 0x6e, 0x60, 0xbc, 0xf5, 0xe6, 0x0d, 0x0c, 0xb8, 0xde, 0xc0, 0x81, 0x11, 0x07, 0x67,
            0xe4, 0xc0, 0x98, 0x03, 0x0c, 0x1d, 0x5a, 0xaa, 0x37, 0x76, 0x60, 0xdc, 0x41, 0x0d, 0x1e,
            0x18, 0x79, 0xf0, 0x46, 0x4f, 0x1d, 0xf5, 0x1a, 0xb3, 0xa7, 0xa0, 0xfa, 0xff, 0xea, 0xa8,
            0xd7, 0x05, 0xd3, 0xa7, 0x8c, 0x3e, 0x44, 0xe3, 0xa7, 0x0d, 0xd5, 0x7f, 0xfd, 0x7d, 0x00,
            0xa9, 0x09, 0xe4, 0x1b
        };
        auto dyn_dec = InflateCodec::InflateZlib(kDynamicZlib, sizeof(kDynamicZlib));
        CHECK(dyn_dec.ok && dyn_dec.value.size() == 1512);
        std::string dyn_str(dyn_dec.value.begin(), dyn_dec.value.end());
        CHECK(dyn_str.find("Linea 24: Curso EL2113 Circuitos y Transformada de Laplace 168!") !=
              std::string::npos);
    }

    // 4b. OOXML (DOCX, PPTX, XLSX) + Bit-3 Data Descriptor + Generic ZIP + Corrupted .zip
    {
        std::string doc_xml = "<w:document><w:body><w:p><w:r><w:t>Resumen Circuitos</w:t></w:r></w:p></w:body></w:document>";
        std::string hdr_xml = "<w:hdr><w:p><w:r><w:t>Encabezado Curso</w:t></w:r></w:p></w:hdr>";
        std::string fn_xml = "<w:footnotes><w:footnote><w:p><w:r><w:t>Nota Pie</w:t></w:r></w:p></w:footnote></w:footnotes>";
        auto docx_bytes = BuildSyntheticZip({
            {"word/document.xml", {doc_xml.begin(), doc_xml.end()}, 0, true},
            {"word/header1.xml", {hdr_xml.begin(), hdr_xml.end()}, 0, false},
            {"word/footnotes.xml", {fn_xml.begin(), fn_xml.end()}, 0, false}
        });
        auto ex_docx = MaterialExtractor::Extract("apuntes.docx", docx_bytes);
        CHECK(ex_docx.kind == MaterialKind::ZipFamily);
        CHECK(ex_docx.kind_detail == "DOCX");
        CHECK(ex_docx.status == ExtractStatus::Full);
        CHECK(ex_docx.text.has_value() && ex_docx.text->find("Resumen Circuitos") != std::string::npos);
        CHECK(ex_docx.text->find("Encabezado Curso") != std::string::npos);
        CHECK(ex_docx.text->find("Nota Pie") != std::string::npos);

        std::string slide_xml = "<p:sld><a:t>Diapositiva Laplace</a:t></p:sld>";
        std::string notes_xml = "<p:notes><a:t>Nota Profesor</a:t></p:notes>";
        auto pptx_bytes = BuildSyntheticZip({
            {"ppt/slides/slide1.xml", {slide_xml.begin(), slide_xml.end()}, 0, false},
            {"ppt/notesSlides/notesSlide1.xml", {notes_xml.begin(), notes_xml.end()}, 0, false}
        });
        auto ex_pptx = MaterialExtractor::Extract("clase.pptx", pptx_bytes);
        CHECK(ex_pptx.kind_detail == "PPTX" && ex_pptx.status == ExtractStatus::Full);
        CHECK(ex_pptx.text->find("Diapositiva Laplace") != std::string::npos);
        CHECK(ex_pptx.text->find("Nota Profesor") != std::string::npos);

        std::string ss_xml = "<sst><si><t>Nota Final &amp; Promedio</t></si></sst>";
        std::string sh_xml = "<worksheet><sheetData><row><c><v>95</v></c></row></sheetData></worksheet>";
        auto xlsx_bytes = BuildSyntheticZip({
            {"xl/sharedStrings.xml", {ss_xml.begin(), ss_xml.end()}, 0, false},
            {"xl/worksheets/sheet1.xml", {sh_xml.begin(), sh_xml.end()}, 0, false}
        });
        auto ex_xlsx = MaterialExtractor::Extract("notas.xlsx", xlsx_bytes);
        CHECK(ex_xlsx.kind_detail == "XLSX" && ex_xlsx.status == ExtractStatus::Full);
        CHECK(ex_xlsx.text->find("Nota Final & Promedio") != std::string::npos);
        CHECK(ex_xlsx.text->find("95") != std::string::npos);

        // Generic ZIP lists entries -> Partial
        std::string r_txt = "readme";
        auto gen_zip = BuildSyntheticZip({{"src/main.cpp", {r_txt.begin(), r_txt.end()}, 0, false}});
        auto ex_gen = MaterialExtractor::Extract("paquete.zip", gen_zip);
        CHECK(ex_gen.status == ExtractStatus::Partial);
        CHECK(ex_gen.text.has_value() && ex_gen.text->find("src/main.cpp") != std::string::npos);

        // Non-PK .zip -> BinaryMedia -> Skipped
        std::vector<uint8_t> bad_zip = {'N', 'O', 'T', 'Z', 'I', 'P'};
        auto ex_skip = MaterialExtractor::Extract("broken.zip", bad_zip);
        CHECK(ex_skip.kind == MaterialKind::BinaryMedia && ex_skip.status == ExtractStatus::Skipped);
    }

    // 4c. PDF FlateDecode + escapes + octal + UTF-16BE hex + scanned PDF failure
    {
        auto zstream = MakeZlibStored("BT [(Teor\\(1\\) \\100\\101) -120 <FEFF0048006F006C0061>] TJ ET");
        std::vector<uint8_t> pdf;
        std::string hdr = "%PDF-1.4\n1 0 obj\n<< /Length " + std::to_string(zstream.size()) +
                          " /Filter /FlateDecode >>\nstream\n";
        std::string ftr = "\nendstream\nendobj\n%%EOF\n";
        pdf.insert(pdf.end(), hdr.begin(), hdr.end());
        pdf.insert(pdf.end(), zstream.begin(), zstream.end());
        pdf.insert(pdf.end(), ftr.begin(), ftr.end());

        auto ex_pdf = MaterialExtractor::Extract("teoria.pdf", pdf);
        CHECK(ex_pdf.kind == MaterialKind::Pdf && ex_pdf.status == ExtractStatus::Partial);
        CHECK(ex_pdf.text.has_value());
        CHECK(ex_pdf.text->find("Teor(1) @AHola") != std::string::npos);

        // Scanned/empty PDF -> Failed
        std::string scanned = "%PDF-1.4\n1 0 obj<<>>endobj\n%%EOF\n";
        auto ex_scan = MaterialExtractor::Extract("scan.pdf",
                                                  reinterpret_cast<const uint8_t*>(scanned.data()),
                                                  scanned.size());
        CHECK(ex_scan.status == ExtractStatus::Failed && !ex_scan.text.has_value());
    }

    // 4d. OLE ASCII + UTF-16-LE runs & Plain text
    {
        std::vector<uint8_t> ole = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1, 0x00, 0x01};
        const char* ascii_run = "Algebra Lineal";
        ole.insert(ole.end(), ascii_run, ascii_run + std::strlen(ascii_run));
        ole.push_back(0x00);
        ole.push_back(0x00);
        for (char c : std::string("VectorPropio")) {
            ole.push_back(static_cast<uint8_t>(c));
            ole.push_back(0x00);
        }
        auto ex_ole = MaterialExtractor::Extract("guia.doc", ole);
        CHECK(ex_ole.kind == MaterialKind::Ole && ex_ole.status == ExtractStatus::Partial);
        CHECK(ex_ole.text.has_value() && ex_ole.text->find("Algebra Lineal") != std::string::npos);
        CHECK(ex_ole.text->find("VectorPropio") != std::string::npos);

        std::string cpp = "// Ecuación diferencial\nint main() { return 0; }\n";
        auto ex_cpp = MaterialExtractor::Extract("ode.cpp",
                                                 reinterpret_cast<const uint8_t*>(cpp.data()),
                                                 cpp.size());
        CHECK(ex_cpp.kind == MaterialKind::Text && ex_cpp.status == ExtractStatus::Full);
    }

    // 4e. OfflineMaterialIndex: IndexDirectory, missing_blobs, magic_vs_ext_mismatches, Search, Stats
    {
        ScopedTempDir tmp("index");
        fs::create_directories(tmp.dir / "downloads" / "_blobs" / "aa");
        std::string txt_body = "Transformada de Laplace y Circuitos Eléctricos";
        std::string sha_txt = Sha256Hex(txt_body);
        fs::path blob_dir = tmp.dir / "downloads" / "_blobs" / sha_txt.substr(0, 2);
        fs::create_directories(blob_dir);
        {
            std::ofstream ofs(blob_dir / sha_txt, std::ios::binary);
            ofs << txt_body;
        }

        // Write a manifest with 1 valid text doc, 1 magic mismatch (.pdf containing plain text), 1 missing blob
        nlohmann::json man = nlohmann::json::array({
            {
                {"object_id", "1"},
                {"name", "resumen_laplace.txt"},
                {"community", "EL2113"},
                {"course", "Circuitos"},
                {"folder_path", "Semana 1"},
                {"sha256", sha_txt},
                {"blob_rel_path", "downloads/_blobs/" + sha_txt.substr(0, 2) + "/" + sha_txt}
            },
            {
                {"object_id", "2"},
                {"name", "falso.pdf"},
                {"community", "EL2113"},
                {"sha256", sha_txt},
                {"blob_rel_path", "downloads/_blobs/" + sha_txt.substr(0, 2) + "/" + sha_txt}
            },
            {
                {"object_id", "3"},
                {"name", "ausente.pdf"},
                {"community", "EL2113"},
                {"sha256", std::string(64, 'f')},
                {"blob_rel_path", "downloads/_blobs/ff/" + std::string(64, 'f')}
            }
        });
        {
            std::ofstream ofs(tmp.dir / "files_manifest.json", std::ios::binary);
            ofs << man.dump(2);
        }

        OfflineMaterialIndex idx(tmp.dir.u8string());
        auto st_res = idx.IndexDirectory();
        CHECK(st_res.ok);
        CHECK(st_res.value.total == 3);
        CHECK(st_res.value.missing_blobs == 1);
        CHECK(st_res.value.magic_vs_ext_mismatches.size() == 1);
        CHECK(st_res.value.magic_vs_ext_mismatches[0].name == "falso.pdf");

        // Accent-folded search ("electricos" matches "Eléctricos")
        MaterialSearchFilters flt;
        flt.ext = ".txt";
        auto hits = idx.SearchOfflineMaterials("laplace electricos", flt);
        CHECK(hits.size() == 1);
        CHECK(hits[0].document.object_id == "1");
        CHECK(!hits[0].snippet.empty());

        // Reload from disk into a fresh OfflineMaterialIndex instance
        OfflineMaterialIndex idx2;
        CHECK(idx2.LoadFromDisk(tmp.dir.u8string()).ok);
        CHECK(idx2.GetOfflineStats().total == 3);
        CHECK(idx2.SearchOfflineMaterials("laplace").size() == 2);
    }
}

// =========================================================================
// 5. U-09: 42+ TDA/TDS Endpoints & TecDigitalService Facade
// =========================================================================
void TestTdaTdsAndService() {
    // 5a. Static parser checks (incl. auth_probe.py sedes fix, numeric/string planes, trucatedName, prematricula)
    {
        auto sedes = TdaTdsClient::ParseSedesJson(
            R"({"sedes":[{"key":"CA","data":"Cartago"},{"key":"SC","data":"San Carlos"}]})");
        CHECK(sedes.ok && sedes.value.size() == 2);
        CHECK(sedes.value[0].key == "CA" && sedes.value[0].name == "Cartago");

        // Numeric & string plan key coercion
        auto planes = TdaTdsClient::ParsePlanesJson(
            R"({"planes":[{"key":809,"data":"Plan 809"},{"key":"412","data":"Plan 412"}]})");
        CHECK(planes.ok && planes.value.size() == 2);
        CHECK(planes.value[0].key == "809" && planes.value[1].key == "412");

        // trucatedName typo & mixed {id}/string equivalents
        auto graph = TdaTdsClient::ParseCurriculumGraphJson(R"({
            "dsc_curriculum":"Ingenieria Electronica","id_curriculum":809,"modality":"Semestral",
            "academic_degree":"Licenciatura","first_level":1,
            "levels":[{"id":"1","courses":[{
                "id_course":"EL2113","name":"Circuitos Electricos I","trucatedName":"Circ. Elec I",
                "credits":4,"hours":4,"requirements":[{"id":"MA1102"}],"co_requirements":["MA2104"],
                "required_by":["EL2114"],"equivalent":[{"id":"EL2101"}],"tEquiv":"1"
            }]}]
        })");
        CHECK(graph.ok && graph.value.levels.size() == 1);
        CHECK(graph.value.levels[0].courses[0].truncated_name == "Circ. Elec I");
        CHECK(graph.value.levels[0].courses[0].requirements[0] == "MA1102");
        CHECK(graph.value.levels[0].courses[0].equivalent[0] == "EL2101");

        // Prematricula tipo=1 & tipo=2 & cursos_deshabilitados
        int max_cr = 0;
        std::string rn;
        auto p1 = TdaTdsClient::ParsePrematriculaTipo1("18~RN01~EL2113~1~NA~NA~MA2104~2~EL1~Optativa", &max_cr, &rn);
        CHECK(max_cr == 18 && rn == "RN01" && p1.size() == 2 && p1[1].course_code == "MA2104");
        CHECK(TdaTdsClient::ParsePrematriculaTipo1("-1").empty());

        auto p2 = TdaTdsClient::ParsePrematriculaTipo2("EL3212`Choque horario~MA2104`Levantamiento");
        CHECK(p2.size() == 2 && p2[0].course_code == "EL3212" && p2[0].observation == "Choque horario");

        auto dis = TdaTdsClient::ParseDisabledCourses("E.EL4701 CA.CA3101");
        CHECK(dis.size() == 2 && dis[0] == "EL4701" && dis[1] == "CA3101");
    }

    // 5b. Full TecDigitalService lifecycle with MockHttpTransport covering all 42+ endpoints
    std::atomic<bool> saw_actualizar_cursos{false};
    std::atomic<bool> saw_mutating_post{false};

    auto mock = std::make_unique<MockHttpTransport>([&](const HttpRequest& req) -> HttpResponse {
        HttpResponse r;
        r.transport_ok = true;
        r.final_url = req.url;
        ParsedUrl pu = ParseHttpUrl(req.url);
        const std::string& p = pu.path;

        if (p == "/dotlrn/ajax/actualizar_cursos") saw_actualizar_cursos.store(true);
        if (p.find("delete_") != std::string::npos || p.find("log_registration") != std::string::npos) {
            saw_mutating_post.store(true);
        }

        auto ok_json = [&](std::string body, std::string ct = "application/json") {
            r.status_code = 200;
            r.content_type = std::move(ct);
            r.body.assign(body.begin(), body.end());
            return r;
        };
        auto ok_html = [&](std::string body) {
            return ok_json(std::move(body), "text/html; charset=utf-8");
        };

        if (p == "/") {
            r.set_cookies.push_back("AlteonP=sticky_1; Path=/");
            return ok_html("<html><body>Login</body></html>");
        }
        if (p == "/api/login/new-form/" && req.method == "POST") {
            r.set_cookies.push_back("ad_session_id=FAKE_SVC_SID; Path=/; HttpOnly");
            return ok_json(R"({"status":"ok","returnUrl":"/dotlrn/"})");
        }
        if (p == "/dotlrn/") {
            return ok_html("<html><body>Portal <a href='/register/logout'>Out</a></body></html>");
        }
        if (p == "/dotlrn/index" || p == "/dotlrn/my-communities") {
            return ok_html(
                "<a href='/dotlrn/classes/E/EL2113/S-2-2026.SC.EL2113.50/one-community'>EL2113 Circuitos</a>"
                "<a href='/dotlrn/catedras.tec/mate-general/one-community'>Catedra Mate</a>");
        }
        if (p == "/dotlrn/classes/" || p == "/dotlrn/catedras.tec/") {
            return ok_html("<html><body>Catalog</body></html>");
        }
        if (p == "/theme-td/ajax/get_date_server") return ok_html("1760000000000");
        if (p == "/tda-um/ajax/get_gaap_server") return ok_json(R"({"gaap_server":"https://gaap.example"})");
        if (p == "/tds-curriculum-exp/ajax/carga_sedes_json") {
            return ok_json(R"({"sedes":[{"key":"SC","data":"San Carlos"}]})");
        }
        if (p == "/tds-curriculum-exp/ajax/carga_carreras_json") {
            return ok_json(R"({"carreras":[{"key":"E","data":"Electronica"}]})");
        }
        if (p == "/tds-curriculum-exp/ajax/carga_planes_json") {
            return ok_json(R"({"planes":[{"key":809,"data":"Plan 809"}]})");
        }
        if (p == "/tds-curriculum-exp/ajax/color_codes_json") {
            return ok_json(R"({"title":"Estados","color_codes":[{"label":"Aprobada","css_class":"ok"}],"aditional_text":"Nota"})");
        }
        if (p == "/tds-curriculum-exp/ajax/obtener_codigo_aplicacion.tcl") return ok_json(R"({"code":2732})");
        if (p == "/tds-curriculum-exp/ajax/validar_permisos_modificacion") return ok_html("false");
        if (p == "/tds-curriculum-exp/ajax/obtener_datos_carrera_estudiante") {
            return ok_json(R"({"nombre":"Estudiante Demo","sede":"SC","carrera":"E","plan":809})");
        }
        if (p == "/tds-curriculum-exp/ajax/json_draw_angular") {
            return ok_json(R"({"dsc_curriculum":"Electronica","id_curriculum":809,"levels":[{"id":"1","courses":[{"id_course":"EL2113","name":"Circuitos","trucatedName":"Circ.","credits":4,"hours":4,"requirements":[],"co_requirements":[],"required_by":[],"equivalent":[]}]}]})");
        }
        if (p == "/tds-curriculum-exp/ajax/json_student_profile_angular") {
            return ok_json(R"({"grades":[{"id_course":"EL2113","best_status":"1","grades_record":[{"year":2026,"period":2,"modality":"S","state":1,"grade":92.5,"group":50,"schedule":[]}]}]})");
        }
        if (p == "/tda-expediente-estudiantil/ajax/i18n") return ok_json(R"({"PackageId":99,"lbl":"Notas"})");
        if (p == "/tda-expediente-estudiantil/ajax/i18n_validation") return ok_json(R"({"just_numbers":"Solo numeros"})");
        if (p == "/tda-expediente-estudiantil/ajax/detalle_notas") return ok_html("<table><tr><td>EL2113</td><td>92</td></tr></table>");
        if (p == "/tda-expediente-estudiantil/ajax/detalle_promedio_ponderado") {
            return ok_html("<div id='nota'>91.50</div><div id='porcentaje_aprobado'>85%</div>");
        }
        if (p == "/tda-expediente-estudiantil/ajax/generar_tabla_promedio_ponderado") return ok_html("<table></table>");
        if (p == "/tda-expediente-estudiantil/ajax/combos/carga_sedes_tds_lib") {
            return ok_html("<span value='SC'>San Carlos</span>");
        }
        if (p == "/tda-expediente-estudiantil/ajax/combos/carga_periodos_tds_lib") {
            // JSON served with text/html Content-Type quirk!
            return ok_html(R"([{"key":"2026-2","data":"II Semestre 2026"}])");
        }
        if (p == "/tda-notifications/ajax/get_user_notifications") {
            return ok_json(R"({"category_sort":true,"notifications":[{"notification_id":"n1","origin":1,"title":"Aviso","text":"Examen","state":"U"}]})");
        }
        if (p == "/tda-notifications/ajax/has_unread_notifications") return ok_json(R"({"quantity":1})");
        if (p == "/tda-inclusion-manager/api/user-transaction") return ok_json(R"({"transaction_key":"TX_SAFE_01"})");
        if (p == "/tda-prematricula/ajax/prematricula_est") {
            if (req.body.find("tipo=1") != std::string::npos) return ok_html("18~RN1~EL2113~1~NA~NA");
            return ok_html("EL2113`Solicitado");
        }
        if (p == "/tda-prematricula/ajax/cursos_deshabilitados") return ok_html("E.EL4900");
        if (p == "/tda-prematricula/ajax/validate_elective") return ok_html("2");
        if (p == "/tda-prematricula/ajax/Internationalization") return ok_json(R"({"title":"Prematricula"})");
        if (p == "/tda-gestor-inscripcion/estudiante/ajax/refresca_selector") return ok_html("<option value='1'>2026</option>");
        if (p == "/tda-gestor-inscripcion/estudiante/ajax/carga_inscripciones") return ok_json("[]");
        if (p == "/tda-feria-negocios/ajax/internationalization") return ok_json(R"({"title":"Feria"})");
        if (p == "/tda-feria-negocios/ajax/registrar_proyectos/getStudentLocation") {
            return ok_html(R"({"identificador":"SC","nombre":"San Carlos"})");
        }
        if (p == "/tda-feria-negocios/ajax/categorias/load_categorias") {
            return ok_html(R"([{"idOp":1,"nameOp":"Software"}])");
        }
        if (p == "/tda-feria-negocios/ajax/ferias/get_ids") {
            return ok_html(R"({"user":1,"community":2,"pack":3})");
        }
        if (p.find("/evaluation/tda-ce-estudiante/tda-index") != std::string::npos) {
            return ok_html(
                "<div id='studentFinal'>90.00 / 100.00</div>"
                "<div class='sub_total' item_weight='30%'>Examenes</div>"
                "<div class='container_accor' assign_id='a1'><span class='assignNameText'>Parcial 1</span><span class='vt_grade_student'>90</span></div>");
        }
        if (p.find("/tda-performance-evaluation/api/forms/") != std::string::npos) {
            return ok_json(R"([{"id":10,"name":"Rubrica","instructions":"Eval","sections":[{"id":1,"name":"S1","order":1,"items":[{"id":2,"description":"Claridad","order":1}]}]}])");
        }
        if (p.find("/tda-performance-evaluation/api/evaluations/") != std::string::npos) return ok_json("[]");
        if (p.find("/news/rss/rss.xml") != std::string::npos) {
            return ok_json(
                "<rss><channel><item><title><![CDATA[Aviso &amp; Cambio]]></title>"
                "<link>https://tecdigital.tec.ac.cr/n1</link><guid>g1</guid>"
                "<description>Clase en laboratorio</description><pubDate>Mon, 01 Sep 2026</pubDate></item></channel></rss>",
                "application/xml");
        }
        if (p.find("/ajax/load_members") != std::string::npos) {
            return ok_json(R"({"draw":1,"recordsTotal":1,"data":[{"checkcCol":"u1","firstName":"Estudiante","lastName":"Demo","emailCol":"demo@example.invalid","rolCol":"Estudiante"}]})");
        }
        r.status_code = 200;
        return r;
    });

    ScopedTempDir tmp("svc");
    ServiceConfig cfg;
    cfg.offline_root = tmp.dir.u8string();
    cfg.vault_config.mode = VaultPersistenceMode::EncryptedVault;
    cfg.vault_config.vault_path = (tmp.dir / "vault.tdv").u8string();
    cfg.vault_config.session_meta_path = (tmp.dir / "session_meta.json").u8string();
    cfg.vault_config.key_override = "unit-test-key";

    TecDigitalService svc(cfg, std::move(mock));

    // Login via ephemeral env vars
    SetEnvVar("TEC_EMAIL", "student.demo@example.invalid");
    SetEnvVar("TEC_PASSWORD", "Demo$Secret#2026");
    AuthResult auth_res = svc.LoginWithEnvOrVault(true);
    SetEnvVar("TEC_EMAIL", "");
    SetEnvVar("TEC_PASSWORD", "");

    CHECK(auth_res.ok);
    CHECK(auth_res.status == AuthStatus::Ok);
    // RAII ScopedAllowLogin MUST have reset allow_login_ to false!
    CHECK(!svc.GuardedTransport().AllowLogin());

    // Mutating endpoints remain blocked through GuardedTransport
    {
        HttpRequest mut_req;
        mut_req.method = "POST";
        mut_req.url = "https://tecdigital.tec.ac.cr/tda-notifications/ajax/delete_all_user_notifications";
        auto mut_resp = svc.GuardedTransport().Execute(mut_req);
        CHECK(!mut_resp.transport_ok);
        CHECK(mut_resp.error_message.rfind("read-only-request-guard:", 0) == 0);
    }

    auto comms = svc.FetchCoursesAndCommunities();
    CHECK(comms.ok && comms.value.size() == 2);
    CHECK(!saw_actualizar_cursos.load());
    CHECK(!saw_mutating_post.load());

    TdaTdsCatalogOptions copts;
    copts.fetch_prematricula = true;
    copts.fetch_gestor_inscripcion = true;
    copts.fetch_feria_negocios = true;
    copts.fetch_members = true;
    copts.pre_enrollment_id = "1001";
    copts.id_plan = "809";
    copts.community_paths = {comms.value[0].url};

    auto cat = svc.FetchTdaTdsCatalog(copts);
    CHECK(cat.ok);
    CHECK(cat.value.server_epoch_ms == 1760000000000LL);
    CHECK(cat.value.sedes.size() == 1 && cat.value.sedes[0].key == "SC");
    CHECK(cat.value.curriculum_graph.has_value() &&
          cat.value.curriculum_graph->levels[0].courses[0].truncated_name == "Circ.");
    CHECK(cat.value.expediente.has_value() &&
          cat.value.expediente->weighted_average_text == "91.50" &&
          cat.value.expediente->periodos_combo.size() == 1);
    CHECK(cat.value.notifications.has_value() && cat.value.notifications->unread_quantity == 1);
    CHECK(cat.value.prematricula.has_value() && cat.value.prematricula->courses.size() == 1 &&
          cat.value.prematricula->disabled_courses.size() == 1);
    CHECK(cat.value.feria_negocios.has_value() &&
          cat.value.feria_negocios->student_location.id == "SC");
    CHECK(cat.value.inclusion_transaction_key == "TX_SAFE_01");
    CHECK(cat.value.evaluations.size() == 1 &&
          cat.value.evaluations[0].final_grade_text == "90.00 / 100.00");
    CHECK(cat.value.news_rss_by_community.size() == 1);
    CHECK(cat.value.members_by_community.size() == 1);

    // Ensure session_meta.json & logs contain zero credential leaks
    auto leak_scan = svc.GetSanitizer().ScanDirectoryForLeaks(tmp.dir.u8string());
    CHECK(leak_scan.passed);
}

// =========================================================================
// 6. R2 Remediations & Remaining TDA Helper Coverage
// =========================================================================
void TestR2Remediations() {
    // 6a. [Fix 1 & 2 & 5] HeadlessCrawler: no cap double-counting (kAnchorRe+kBareHrefRe),
    //     JSON subfolder BFS enqueueing, and SanitizeOptions forwarded to MirrorManifestStore.
    {
        std::atomic<bool> saw_c1{false};
        std::atomic<bool> saw_c2{false};
        std::atomic<bool> saw_c3{false};
        LoopbackHttpServer srv([&](const std::string&,
                                   const std::string& target,
                                   const std::string&,
                                   const std::string&) -> std::string {
            auto ok = [](const std::string& body, const std::string& ct) {
                return "HTTP/1.1 200 OK\r\nContent-Type: " + ct +
                       "\r\nContent-Length: " + std::to_string(body.size()) +
                       "\r\nConnection: close\r\n\r\n" + body;
            };
            if (target == "/dotlrn/") {
                return ok(
                    "<html><body><a href='/register/logout'>Out</a>"
                    "<a href='/dotlrn/calendar/view?date=2026-09-01'>C1</a>"
                    "<a href='/dotlrn/calendar/view?date=2026-10-01'>C2</a>"
                    "<a href='/dotlrn/calendar/view?date=2026-11-01'>C3</a>"
                    "<form action='/dotlrn/search?pwd=R2_CRAWL_SECRET_99'></form>"
                    "</body></html>",
                    "text/html");
            }
            if (target.rfind("/dotlrn/calendar/view?date=2026-09", 0) == 0) {
                saw_c1.store(true);
                return ok("<html><body>C1</body></html>", "text/html");
            }
            if (target.rfind("/dotlrn/calendar/view?date=2026-10", 0) == 0) {
                saw_c2.store(true);
                return ok("<html><body>C2</body></html>", "text/html");
            }
            if (target.rfind("/dotlrn/calendar/view?date=2026-11", 0) == 0) {
                saw_c3.store(true);
                return ok("<html><body>C3</body></html>", "text/html");
            }
            if (target == "/dotlrn/file-storage/api/folder/7001") {
                return ok(
                    R"({"object_id":7001,"name":"Root7001","elements":[{"object_id":7002,"fs_type":"folder","name":"Sub7002"}]})",
                    "application/json");
            }
            if (target == "/dotlrn/file-storage/api/folder/7002") {
                return ok(
                    R"({"object_id":7002,"name":"Sub7002","elements":[{"object_id":9901,"fs_type":"file","file_upload_name":"notas.pdf","title":"Notas"}]})",
                    "application/json");
            }
            return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        });
        CHECK(srv.Start());

        PolicyOptions popts;
        popts.allowed_test_origins = {srv.BaseUrl()};
        ReadOnlyRequestPolicy policy(popts);
        auto native = CreateNativeHttpTransport();
        PolicyEnforcingTransport guarded(*native, policy);

        AuthClientOptions aopts;
        aopts.base_url = srv.BaseUrl();
        aopts.perform_prelogin_handshake = false;
        AuthClient auth(guarded, policy, aopts);

        CrawlerOptions copts;
        copts.base_url = srv.BaseUrl();
        copts.delay_ms = 0;
        copts.calendar_date_cap = 2;
        copts.max_depth = 4;
        copts.max_folders = 10;
        copts.seed_urls = {srv.BaseUrl() + "/dotlrn/"};
        copts.seed_folder_ids = {"7001"};

        SanitizeOptions sopts;
        sopts.runtime_password = "R2_CRAWL_SECRET_99";

        ScopedTempDir tmp("r2_crawl");
        HeadlessCrawler crawler(guarded, auth, copts, sopts);
        auto cr = crawler.Run(tmp.dir.u8string());
        CHECK(cr.ok);
        CHECK(saw_c1.load());
        CHECK(saw_c2.load());
        CHECK(!saw_c3.load());
        CHECK(cr.value.pages_skipped_cap == 1);

        bool found_9901 = false;
        for (const auto& fr : cr.value.files) {
            if (fr.object_id == "9901") found_9901 = true;
        }
        CHECK(found_9901);

        // MirrorManifestStore received sopts and scrubbed runtime_password
        Sanitizer check_san(sopts);
        CHECK(check_san.ScanDirectoryForLeaks(tmp.dir.u8string()).passed);
    }

    // 6b. [Fix 3, 4, 5] StorageDownloader: 200 OK Range-fallback max_file_bytes uses post-truncation
    //     offset 0, HTTP 416 removes stale .part and retries without Range, and saw_range_header_omitted_on_zero.
    {
        const std::string pdf_76 =
            "%PDF-1.4\n1 0 obj<<>>endobj\nstream\nBT (76-Byte PDF Payload!) Tj ET\nendstream\n";
        const std::string pdf_76_b =
            "%PDF-1.4\n1 0 obj<<>>endobj\nstream\nBT (76-Byte PDF 416 Ok!!) Tj ET\nendstream\n";
        CHECK(pdf_76.size() == 76 && pdf_76.size() <= 81);
        CHECK(pdf_76_b.size() == 76 && pdf_76_b.size() <= 81);
        std::atomic<int> hits_416{0};
        std::atomic<bool> saw_range_on_retry_after_416{false};

        LoopbackHttpServer srv([&](const std::string& method,
                                   const std::string& target,
                                   const std::string& headers_lower,
                                   const std::string&) -> std::string {
            if (target == "/dotlrn/file-storage/download?file_id=301") {
                if (method == "HEAD") {
                    return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                }
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Length: " +
                       std::to_string(pdf_76.size()) + "\r\nConnection: close\r\n\r\n" + pdf_76;
            }
            if (target == "/dotlrn/file-storage/download?file_id=302") {
                if (method == "HEAD") {
                    return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                }
                if (headers_lower.find("range:") != std::string::npos) {
                    if (hits_416.fetch_add(1) == 0) {
                        return "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    }
                    saw_range_on_retry_after_416.store(true);
                }
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Length: " +
                       std::to_string(pdf_76_b.size()) + "\r\nConnection: close\r\n\r\n" + pdf_76_b;
            }
            return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        });
        CHECK(srv.Start());

        PolicyOptions popts;
        popts.allowed_test_origins = {srv.BaseUrl()};
        ReadOnlyRequestPolicy policy(popts);
        auto native = CreateNativeHttpTransport();
        PolicyEnforcingTransport guarded(*native, policy);

        AuthClientOptions aopts;
        aopts.base_url = srv.BaseUrl();
        aopts.perform_prelogin_handshake = false;
        AuthClient auth(guarded, policy, aopts);

        DownloaderOptions dopts;
        dopts.min_delay_ms = 0;
        dopts.backoff_ms = {1};
        dopts.max_retries = 1;
        dopts.max_file_bytes = 81;
        StorageDownloader dl(guarded, auth, srv.BaseUrl(), dopts);

        ScopedTempDir tmp("r2_dl");
        fs::create_directories(tmp.dir / "downloads" / "_tmp");

        // 200 OK fallback after 20-byte .part with max_file_bytes=81 and 76-byte PDF must succeed
        {
            std::ofstream ofs(tmp.dir / "downloads" / "_tmp" / "301.part", std::ios::binary);
            ofs << std::string(20, 'X');
        }
        FileRecord f301;
        f301.object_id = "301";
        f301.name = "fit.pdf";
        f301.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=301"};
        auto r301 = dl.DownloadFile(f301, tmp.dir.u8string());
        CHECK(r301.outcome == DownloadOutcome::Downloaded);
        CHECK(r301.sha256 == Sha256Hex(pdf_76));

        // 416 Range Not Satisfiable on stale 120-byte .part must reset .part and retry without Range
        {
            std::ofstream ofs(tmp.dir / "downloads" / "_tmp" / "302.part", std::ios::binary);
            ofs << std::string(120, 'Z');
        }
        FileRecord f302;
        f302.object_id = "302";
        f302.name = "recover416.pdf";
        f302.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=302"};
        auto r302 = dl.DownloadFile(f302, tmp.dir.u8string());
        CHECK(r302.outcome == DownloadOutcome::Downloaded);
        CHECK(r302.sha256 == Sha256Hex(pdf_76_b));
        CHECK(r302.saw_range_header_omitted_on_zero);
        CHECK(hits_416.load() == 1);
        CHECK(!saw_range_on_retry_after_416.load());
    }

    // 6c. [Fix 6, 7, 8] MaterialIndexer: PDF UTF-16BE & Latin-1 literal strings -> valid UTF-8,
    //     OLE accented UTF-16-LE runs, and OfflineMaterialIndex::IndexBlob re-index deduplication.
    {
        // PDF literal UTF-16BE BOM + octal Latin-1 \341
        std::string pdf_lit =
            "%PDF-1.4\n1 0 obj<<>>endobj\nstream\n"
            "BT (\\376\\377\\000H\\000o\\000l\\000a) Tj (C\\341lculo \\(I\\)) Tj ET\n"
            "endstream\n%%EOF\n";
        auto ex_pdf = MaterialExtractor::Extract(
            "lit.pdf", reinterpret_cast<const uint8_t*>(pdf_lit.data()), pdf_lit.size());
        CHECK(ex_pdf.text.has_value());
        CHECK(ex_pdf.text->find("Hola") != std::string::npos);
        CHECK(ex_pdf.text->find("C\xC3\xA1lculo (I)") != std::string::npos);
        // Must serialize cleanly with nlohmann::json::dump() (valid UTF-8)
        bool json_ok = true;
        try {
            nlohmann::json j = {{"text", *ex_pdf.text}};
            (void)j.dump();
        } catch (...) {
            json_ok = false;
        }
        CHECK(json_ok);

        // OLE accented UTF-16-LE run ("Matemática")
        std::vector<uint8_t> ole = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1, 0x00, 0x01};
        const uint16_t utf16_word[] = {'M', 'a', 't', 'e', 'm', 0x00E1, 't', 'i', 'c', 'a'};
        for (uint16_t u : utf16_word) {
            ole.push_back(static_cast<uint8_t>(u & 0xFFu));
            ole.push_back(static_cast<uint8_t>((u >> 8) & 0xFFu));
        }
        auto ex_ole = MaterialExtractor::Extract("mate.doc", ole);
        CHECK(ex_ole.text.has_value());
        CHECK(ex_ole.text->find("Matem\xC3\xA1tica") != std::string::npos);

        // Re-index deduplication for mismatches_ & missing_blobs_count_
        ScopedTempDir tmp("r2_reindex");
        OfflineMaterialIndex idx(tmp.dir.u8string());
        IndexedBlobInput mm_in;
        mm_in.object_id = "m1";
        mm_in.name = "falso.pdf";
        std::string txt = "NOT A PDF";
        mm_in.inline_bytes.assign(txt.begin(), txt.end());
        CHECK(idx.IndexBlob(mm_in).ok);
        CHECK(idx.IndexBlob(mm_in).ok);
        CHECK(idx.Documents().size() == 1);
        CHECK(idx.GetOfflineStats().magic_vs_ext_mismatches.size() == 1);

        IndexedBlobInput miss_in;
        miss_in.object_id = "miss1";
        miss_in.name = "missing.pdf";
        miss_in.sha256 = std::string(64, 'a');
        CHECK(idx.IndexBlob(miss_in).ok);
        CHECK(idx.IndexBlob(miss_in).ok);
        CHECK(idx.Documents().size() == 2);
        CHECK(idx.GetOfflineStats().missing_blobs == 1);

        // Providing bytes on re-index decrements missing_blobs cleanly
        miss_in.inline_bytes.assign(pdf_lit.begin(), pdf_lit.end());
        CHECK(idx.IndexBlob(miss_in).ok);
        CHECK(idx.GetOfflineStats().missing_blobs == 0);
    }

    // 6d. [Fix 12 & 13] ReadOnlyRequestPolicy GET+POST blocks for log_registration, tam-tracking,
    //     to-tam-tracking, matricula (while allowing prematricula/citas_matricula/comprobante_matricula)
    //     and Sanitizer::ScanReusableTokens #/path/access_token=... detection.
    {
        ReadOnlyRequestPolicy policy;
        auto eval = [&](const char* path, const char* method) {
            PolicyRequestContext ctx;
            ctx.url = std::string("https://tecdigital.tec.ac.cr") + path;
            ctx.method = method;
            return policy.Evaluate(ctx).allowed;
        };
        CHECK(!eval("/tda-prematricula/ajax/log_registration", "GET"));
        CHECK(!eval("/tda-prematricula/ajax/log_registration", "POST"));
        CHECK(!eval("/tda-tam/tam-tracking", "GET"));
        CHECK(!eval("/tda-tam/tam-tracking", "POST"));
        CHECK(!eval("/to-tam-tracking", "GET"));
        CHECK(!eval("/tda-matricula/matricula", "GET"));
        CHECK(!eval("/tda-matricula/matricula", "POST"));

        CHECK(eval("/tda-prematricula/", "GET"));
        CHECK(eval("/tda-expediente-estudiantil/ajax/citas_matricula", "GET"));
        CHECK(eval("/tda-gestor-inscripcion/estudiante/ajax/comprobante_matricula", "GET"));
        PolicyRequestContext pm_ctx;
        pm_ctx.url = "https://tecdigital.tec.ac.cr/tda-prematricula/ajax/prematricula_est";
        pm_ctx.method = "POST";
        pm_ctx.post_data = "tipo=1&pre_enrollment_id=1&curriculum_id=2";
        CHECK(policy.Evaluate(pm_ctx).allowed);

        Sanitizer san;
        auto toks = san.ScanReusableTokens("https://example.com/cb#/oauth/access_token=SUPERSECRET123&state=ok");
        CHECK(!toks.empty());
        CHECK(san.ScanReusableTokens("https://example.com/cb#/oauth/access_token=[REDACTED]&state=ok").empty());
    }

    // 6e. [Fix 14] RSS XML prefix collision, CDATA </description>, HtmlUnescape >= 128 & named entities,
    //     nested <span> in .vt_grade_student.
    {
        auto rss = TdaTdsClient::ParseRssXml(
            "<rss><channel><item>"
            "<title_extra>NOISE</title_extra>"
            "<title>Evaluaci&#243;n &aacute;gil &#xF3;ptima</title>"
            "<description><![CDATA[Example </description> inside CDATA]]></description>"
            "</item></channel></rss>");
        CHECK(rss.ok && rss.value.size() == 1);
        CHECK(rss.value[0].title == "Evaluaci\xC3\xB3n \xC3\xA1gil \xC3\xB3ptima");
        CHECK(rss.value[0].description == "Example </description> inside CDATA");

        auto cats = TdaTdsClient::ParseEvaluationsHtml(
            "<div class='sub_total' item_weight='20%'>Quices</div>"
            "<div assign_id='q1'><span class='assignNameText'>Quiz 1</span>"
            "<span class='vt_grade_student'><span>--</span> / 20.00</span></div>");
        CHECK(cats.size() == 1 && cats[0].items.size() == 1);
        CHECK(cats[0].items[0].grade_text == "-- / 20.00");
    }

    // 6f. [Fix 9, 10, 11, 14] Login needle propagation to material_index_ & tda_client_,
    //     FetchTdaTdsCatalog full payload scrubbing, auto-discovered plan for prematricula,
    //     and remaining minor TDA helper methods.
    {
        const std::string runtime_pwd = "R2#SuperSecretPwd!2026";
        const std::string runtime_mail = "r2.student@example.invalid";

        auto mock = std::make_unique<MockHttpTransport>([&](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.final_url = req.url;
            ParsedUrl pu = ParseHttpUrl(req.url);
            const std::string& p = pu.path;

            auto ok_json = [&](std::string body, std::string ct = "application/json") {
                r.status_code = 200;
                r.content_type = std::move(ct);
                r.body.assign(body.begin(), body.end());
                return r;
            };
            auto ok_html = [&](std::string body) {
                return ok_json(std::move(body), "text/html; charset=utf-8");
            };

            if (p == "/") {
                r.set_cookies.push_back("AlteonP=sticky_r2; Path=/");
                return ok_html("<html>Login</html>");
            }
            if (p == "/api/login/new-form/" && req.method == "POST") {
                r.set_cookies.push_back("ad_session_id=FAKE_R2_SID; Path=/; HttpOnly");
                return ok_json(R"({"status":"ok","returnUrl":"/dotlrn/"})");
            }
            if (p == "/dotlrn/") return ok_html("<html>Portal Logout</html>");
            if (p == "/theme-td/ajax/get_date_server") {
                r.transport_ok = false;
                r.error_message = "upstream failure containing " + runtime_pwd;
                return r;
            }
            if (p == "/tda-um/ajax/get_gaap_server") return ok_json(R"({"gaap_server":"https://gaap.example/?pwd=GAAP_SECRET"})");
            if (p == "/tda-um/ajax/internacionalization") return ok_json(R"({"hello":"Hola"})");
            if (p == "/tda-consentimiento-informado/ajax/client/get_user_term_condition") return ok_json(R"({"accepted":true})");
            if (p == "/tda-sms-configuration/ajax/internationalization") return ok_json(R"({"sms":"OK"})");
            if (p == "/servicios/gaap_oacs/PL/resources/js/json/tam_gaap.json") return ok_json(R"({"tam":"v1"})");
            if (p == "/notifications/ajax/request_id") return ok_json(R"({"requestID":"REQ_77"})");
            if (p == "/dotlrn/file-storage/view/folder-chunk") return ok_html("<div>FolderChunk</div>");
            if (p == "/dotlrn/file-storage/ajax/internationalization/internationalization") return ok_json(R"({"fs":"Archivo"})");
            if (p == "/tds-curriculum-exp/pg_angular/templates/main.html") return ok_html("<div>Tpl</div>");
            if (p == "/tds-curriculum/ajax/internationalization") return ok_json(R"({"cur":"Plan"})");
            if (p == "/tds-curriculum/display_3") return ok_html("<div>LegacyDisplay</div>");
            if (p == "/tds-curriculum-exp/ajax/obtener_nombre_estudiante") return ok_json(R"({"nombre":"Estudiante R2"})");
            if (p.find("/members") != std::string::npos) return ok_html("name,email\nDemo,demo@example.invalid\n");

            if (p == "/tds-curriculum-exp/ajax/carga_sedes_json") return ok_json(R"({"sedes":[{"key":"SC","data":"San Carlos"}]})");
            if (p == "/tds-curriculum-exp/ajax/carga_carreras_json") return ok_json(R"({"carreras":[{"key":"E","data":"Electronica"}]})");
            if (p == "/tds-curriculum-exp/ajax/carga_planes_json") return ok_json(R"({"planes":[{"key":809,"data":"Plan 809"}]})");
            if (p == "/tds-curriculum-exp/ajax/obtener_datos_carrera_estudiante") {
                return ok_json(R"({"nombre":")" + runtime_mail + R"(","sede":"SC","carrera":"E","plan":809})");
            }
            if (p == "/tda-notifications/ajax/get_user_notifications") {
                return ok_json(R"({"category_sort":true,"notifications":[{"notification_id":"n1","origin":1,"title":"Zoom","text":"Bearer LEAKED_BEARER_TOKEN_XYZ","url":"https://zoom.us/j/123?pwd=ZOOM_SECRET_99","state":"U"}]})");
            }
            if (p == "/tda-notifications/ajax/has_unread_notifications") return ok_json(R"({"quantity":1})");
            if (p == "/tda-inclusion-manager/api/user-transaction") return ok_json(R"({"transaction_key":"Bearer LEAKED_INCLUSION_TOK"})");
            if (p == "/tda-prematricula/ajax/prematricula_est") {
                if (req.body.find("tipo=1") != std::string::npos) return ok_html("18~RN1~EL2113~1~NA~NA");
                return ok_html("EL2113`Obs other@example.invalid");
            }
            if (p == "/tda-prematricula/ajax/cursos_deshabilitados") return ok_html("E.EL4900");
            if (p.find("/evaluation/tda-ce-estudiante/tda-index") != std::string::npos) {
                return ok_html("<div id='studentFinal'>90</div><div class='sub_total'>Cat</div><div assign_id='a1'><span class='assignNameText'>Bearer LEAKED_EVAL_TOK</span><span class='vt_grade_student'><span>--</span> / 20.00</span></div>");
            }
            if (p.find("/news/rss/rss.xml") != std::string::npos) {
                return ok_json("<rss><channel><item><title>News</title><link>https://zoom.us/j/9?pwd=RSS_ZOOM_SECRET</link><description>mail@example.invalid</description></item></channel></rss>", "application/xml");
            }
            if (p.find("/ajax/load_members") != std::string::npos) {
                return ok_json(R"({"data":[{"checkcCol":"u1","firstName":"A","lastName":"B","emailCol":"member.pii@example.invalid","rolCol":"Est"}]})");
            }
            if (p == "/dotlrn/file-storage/api/folder/8801") {
                return ok_json(R"({"object_id":8801,"name":"R2Folder","elements":[{"object_id":8802,"fs_type":"file","file_upload_name":"leak_note.txt","title":"leak_note"}]})");
            }
            if (req.url.find("file_id=8802") != std::string::npos || p == "/dotlrn/file-storage/view/8802") {
                return ok_json("Nota con secreto: " + runtime_pwd + " fin.", "text/plain");
            }
            return ok_json("{}");
        });

        ScopedTempDir tmp("r2_svc");
        ServiceConfig cfg;
        cfg.offline_root = tmp.dir.u8string();
        TecDigitalService svc(cfg, std::move(mock));

        Credentials creds;
        creds.email = runtime_mail;
        creds.password = runtime_pwd;
        CHECK(svc.LoginWithCredentials(creds, false).ok);

        // TdaClient error message after login must scrub runtime_pwd
        auto date_res = svc.TdaClient().GetServerDateMs();
        CHECK(!date_res.ok);
        CHECK(date_res.error_message.find(runtime_pwd) == std::string::npos);

        // Auto-discovered plan (id_plan = "") still fetches prematricula, and all catalog fields scrubbed
        TdaTdsCatalogOptions copts;
        copts.fetch_prematricula = true;
        copts.fetch_members = true;
        copts.pre_enrollment_id = "1001";
        copts.id_plan = "";
        copts.community_paths = {"/dotlrn/classes/E/EL2113/S1"};
        auto cat = svc.FetchTdaTdsCatalog(copts);
        CHECK(cat.ok);
        CHECK(cat.value.prematricula.has_value() && cat.value.prematricula->courses.size() == 1);
        CHECK(cat.value.gaap_server.find("GAAP_SECRET") == std::string::npos);
        CHECK(cat.value.inclusion_transaction_key.find("LEAKED_INCLUSION_TOK") == std::string::npos);
        CHECK(cat.value.student_career.has_value() &&
              cat.value.student_career->nombre.find(runtime_mail) == std::string::npos);
        CHECK(cat.value.notifications.has_value() &&
              cat.value.notifications->notifications[0].url.find("ZOOM_SECRET_99") == std::string::npos &&
              cat.value.notifications->notifications[0].text.find("LEAKED_BEARER_TOKEN_XYZ") == std::string::npos);
        CHECK(cat.value.evaluations.size() == 1 &&
              cat.value.evaluations[0].categories[0].items[0].name.find("LEAKED_EVAL_TOK") == std::string::npos);
        CHECK(cat.value.news_rss_by_community.begin()->second[0].link.find("RSS_ZOOM_SECRET") == std::string::npos &&
              cat.value.news_rss_by_community.begin()->second[0].description.find("mail@example.invalid") == std::string::npos);
        CHECK(cat.value.members_by_community.begin()->second[0].email.find("member.pii@example.invalid") == std::string::npos);

        // Remaining TDA helpers
        CHECK(svc.TdaClient().GetNotificationRequestId().value == "REQ_77");
        CHECK(svc.TdaClient().GetUmInternationalization().value["hello"] == "Hola");
        CHECK(svc.TdaClient().GetUserTermConditionJson().ok);
        CHECK(svc.TdaClient().GetSmsInternationalization().value["sms"] == "OK");
        CHECK(svc.TdaClient().GetTamGaapJson().ok);
        CHECK(svc.TdaClient().GetFileStorageFolderChunkHtml("1").value.find("FolderChunk") != std::string::npos);
        CHECK(svc.TdaClient().GetFileStorageInternationalization().value["fs"] == "Archivo");
        CHECK(svc.TdaClient().GetCurriculumAngularTemplate("main.html").value.find("Tpl") != std::string::npos);
        CHECK(svc.TdaClient().GetLegacyCurriculumInternationalization().value["cur"] == "Plan");
        CHECK(svc.TdaClient().GetLegacyCurriculumDisplayHtml("809", "E").value.find("LegacyDisplay") != std::string::npos);
        CHECK(svc.TdaClient().GetStudentOfficialName().value == "Estudiante R2");
        CHECK(svc.TdaClient().GetCommunityMembersCsv("/dotlrn/classes/E/EL2113/S1").value.find("demo@example.invalid") == std::string::npos);

        // SyncFolderStorage downloads .txt containing runtime_pwd -> material_index/text/*.txt must NOT leak runtime_pwd
        SyncFolderStorageOptions sopts;
        sopts.seed_folder_ids = {"8801"};
        auto sr = svc.SyncFolderStorage(sopts);
        CHECK(sr.ok && sr.value.files_downloaded == 1);
        CHECK(sr.value.index_stats.total == 1);
        CHECK(svc.GetSanitizer().ScanDirectoryForLeaks((tmp.dir / "material_index").u8string()).passed);
        for (const auto& mname : {"files_manifest.json", "folders_manifest.json", "forms_manifest.json", "manifest.json"}) {
            std::ifstream mfs(tmp.dir / mname, std::ios::binary);
            std::ostringstream mss;
            mss << mfs.rdbuf();
            CHECK(svc.GetSanitizer().ScanForLeaks(mss.str()).passed);
        }
    }
}

uint32_t TestCrc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t TestAdler32(const uint8_t* data, size_t len) {
    uint32_t s1 = 1, s2 = 0;
    for (size_t i = 0; i < len; ++i) {
        s1 = (s1 + data[i]) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    return (s2 << 16) | s1;
}

std::string BuildStoredGzip(std::string_view payload, const std::string& fname = "") {
    std::string out;
    out.push_back(static_cast<char>(0x1F));
    out.push_back(static_cast<char>(0x8B));
    out.push_back(static_cast<char>(0x08));
    out.push_back(static_cast<char>(fname.empty() ? 0x00 : 0x08));
    for (int i = 0; i < 4; ++i) out.push_back(0x00);
    out.push_back(0x00);
    out.push_back(static_cast<char>(0xFF));
    if (!fname.empty()) {
        out.append(fname);
        out.push_back('\0');
    }
    // Single stored DEFLATE block (payload <= 65535)
    const uint16_t plen = static_cast<uint16_t>(payload.size());
    const uint16_t nlen = static_cast<uint16_t>(~plen);
    out.push_back(0x01);  // BFINAL=1, BTYPE=00
    out.push_back(static_cast<char>(plen & 0xFFu));
    out.push_back(static_cast<char>((plen >> 8) & 0xFFu));
    out.push_back(static_cast<char>(nlen & 0xFFu));
    out.push_back(static_cast<char>((nlen >> 8) & 0xFFu));
    out.append(payload);

    const uint32_t crc = TestCrc32(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    const uint32_t isz = static_cast<uint32_t>(payload.size() & 0xFFFFFFFFu);
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((crc >> (8 * i)) & 0xFFu));
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((isz >> (8 * i)) & 0xFFu));
    return out;
}

std::string BuildStoredZlib(std::string_view payload) {
    std::string out;
    out.push_back(static_cast<char>(0x78));
    out.push_back(static_cast<char>(0x9C));
    const uint16_t plen = static_cast<uint16_t>(payload.size());
    const uint16_t nlen = static_cast<uint16_t>(~plen);
    out.push_back(0x01);
    out.push_back(static_cast<char>(plen & 0xFFu));
    out.push_back(static_cast<char>((plen >> 8) & 0xFFu));
    out.push_back(static_cast<char>(nlen & 0xFFu));
    out.push_back(static_cast<char>((nlen >> 8) & 0xFFu));
    out.append(payload);
    const uint32_t adl = TestAdler32(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    out.push_back(static_cast<char>((adl >> 24) & 0xFFu));
    out.push_back(static_cast<char>((adl >> 16) & 0xFFu));
    out.push_back(static_cast<char>((adl >> 8) & 0xFFu));
    out.push_back(static_cast<char>(adl & 0xFFu));
    return out;
}

std::string BuildStoredRawDeflate(std::string_view payload) {
    std::string out;
    const uint16_t plen = static_cast<uint16_t>(payload.size());
    const uint16_t nlen = static_cast<uint16_t>(~plen);
    out.push_back(0x01);  // BFINAL=1, BTYPE=00
    out.push_back(static_cast<char>(plen & 0xFFu));
    out.push_back(static_cast<char>((plen >> 8) & 0xFFu));
    out.push_back(static_cast<char>(nlen & 0xFFu));
    out.push_back(static_cast<char>((nlen >> 8) & 0xFFu));
    out.append(payload);
    return out;
}

std::string ToHttpChunked(std::string_view body, size_t split_at) {
    auto hex_len = [](size_t n) {
        std::ostringstream ss;
        ss << std::hex << n;
        return ss.str();
    };
    std::string out;
    if (split_at > 0 && split_at < body.size()) {
        std::string_view p1 = body.substr(0, split_at);
        std::string_view p2 = body.substr(split_at);
        out += hex_len(p1.size()) + "\r\n";
        out.append(p1);
        out += "\r\n";
        out += hex_len(p2.size()) + "\r\n";
        out.append(p2);
        out += "\r\n";
    } else if (!body.empty()) {
        out += hex_len(body.size()) + "\r\n";
        out.append(body);
        out += "\r\n";
    }
    out += "0\r\n\r\n";
    return out;
}

void TestR3Remediations() {
    // 7a. [R3-1 & R3-2] IndexBlob transition deduplication & SaveToDisk filename scrubbing
    {
        ScopedTempDir tmp("r3_idx");
        SanitizeOptions sopts;
        sopts.runtime_password = "R3#FilePwd!99";
        sopts.runtime_email = "r3.owner@estudiantec.cr";

        OfflineMaterialIndex idx(tmp.dir.u8string(), sopts);

        // 1) Missing blob with NO sha256 -> total=1, missing=1
        IndexedBlobInput step1;
        step1.object_id = "obj_501";
        step1.name = "tema_" + sopts.runtime_password + "_" + sopts.runtime_email + ".pdf";
        CHECK(idx.IndexBlob(step1).ok);
        CHECK(idx.Documents().size() == 1);
        CHECK(idx.GetOfflineStats().missing_blobs == 1);
        CHECK(idx.GetOfflineStats().magic_vs_ext_mismatches.empty());

        // 2) Same object_id transitions missing(no sha) -> mismatch(sha1) -> total=1, missing=0, mm=1
        IndexedBlobInput step2 = step1;
        std::string bad_txt = "OLD MISMATCH TEXT TOKEN_R3_OLD";
        step2.inline_bytes.assign(bad_txt.begin(), bad_txt.end());
        CHECK(idx.IndexBlob(step2).ok);
        CHECK(idx.Documents().size() == 1);
        CHECK(idx.GetOfflineStats().missing_blobs == 0);
        CHECK(idx.GetOfflineStats().magic_vs_ext_mismatches.size() == 1);

        // 3) Same object_id transitions mismatch(sha1) -> valid(sha2) -> total=1, missing=0, mm=0
        IndexedBlobInput step3 = step1;
        std::string valid_pdf =
            "%PDF-1.4\n1 0 obj<<>>endobj\nstream\nBT (TOKEN_R3_NEW) Tj ET\nendstream\n%%EOF\n";
        step3.inline_bytes.assign(valid_pdf.begin(), valid_pdf.end());
        CHECK(idx.IndexBlob(step3).ok);
        CHECK(idx.Documents().size() == 1);
        CHECK(idx.GetOfflineStats().missing_blobs == 0);
        CHECK(idx.GetOfflineStats().magic_vs_ext_mismatches.empty());
        CHECK(idx.SearchOfflineMaterials("TOKEN_R3_OLD").empty());
        CHECK(idx.SearchOfflineMaterials("TOKEN_R3_NEW").size() == 1);

        // 4) SaveToDisk scrubs filename in both documents.json and stats.json (including mismatches)
        IndexedBlobInput mm_doc;
        mm_doc.object_id = "obj_502";
        mm_doc.name = "leak_" + sopts.runtime_password + "_" + sopts.runtime_email + ".pdf";
        mm_doc.inline_bytes.assign(bad_txt.begin(), bad_txt.end());
        CHECK(idx.IndexBlob(mm_doc).ok);
        CHECK(idx.SaveToDisk().ok);

        Sanitizer check_san(sopts);
        for (const char* fname : {"documents.json", "stats.json"}) {
            std::ifstream ifs(tmp.dir / "material_index" / fname, std::ios::binary);
            std::ostringstream ss;
            ss << ifs.rdbuf();
            std::string content = ss.str();
            CHECK(!content.empty());
            CHECK(check_san.ScanForLeaks(content).passed);
            CHECK(content.find(sopts.runtime_password) == std::string::npos);
            CHECK(content.find(sopts.runtime_email) == std::string::npos);
        }
    }

    // 7b. [R3-3 & R3-4] InflateGzip + Loopback 200 OK gzip (tiny CL & Range-fallback) + deflate + partial retry != Orphan404
    {
        const std::string pdf_tiny = "%PDF-1.4\n1 0 obj<<>>endobj\n%%EOF\n";
        const std::string pdf_g3 = "%PDF-1.4\n2 0 obj<<>>endobj\n%%EOF\n";
        const std::string pdf_deflate = "%PDF-1.4\n3 0 obj<<>>endobj\n%%EOF\n";
        const std::string gz_tiny = BuildStoredGzip(pdf_tiny, "notas.pdf");
        const std::string gz_g3 = BuildStoredGzip(pdf_g3, "g3.pdf");
        const std::string zlib_tiny = BuildStoredZlib(pdf_deflate);
        CHECK(gz_tiny.size() > pdf_tiny.size());

        // InflateCodec::InflateGzip unit checks (FNAME + multi-member + CRC rejection)
        auto dec1 = InflateCodec::InflateGzip(
            reinterpret_cast<const uint8_t*>(gz_tiny.data()), gz_tiny.size());
        CHECK(dec1.ok && std::string(dec1.value.begin(), dec1.value.end()) == pdf_tiny);
        std::string multi_gz = gz_tiny + BuildStoredGzip("EXTRA");
        auto dec2 = InflateCodec::InflateGzip(
            reinterpret_cast<const uint8_t*>(multi_gz.data()), multi_gz.size());
        CHECK(dec2.ok && std::string(dec2.value.begin(), dec2.value.end()) == (pdf_tiny + "EXTRA"));
        std::string bad_crc_gz = gz_tiny;
        bad_crc_gz[bad_crc_gz.size() - 5] ^= 0xFF;
        CHECK(!InflateCodec::InflateGzip(
                   reinterpret_cast<const uint8_t*>(bad_crc_gz.data()), bad_crc_gz.size())
                   .ok);

        std::atomic<int> g2_hits{0};
        std::atomic<int> g3_hits{0};
        std::atomic<bool> g3_saw_range{false};
        std::atomic<bool> g3_saw_identity{false};
        std::atomic<int> partial_hits{0};

        LoopbackHttpServer srv([&](const std::string& method,
                                   const std::string& target,
                                   const std::string& headers_lower,
                                   const std::string&) -> std::string {
            if (method == "HEAD") {
                return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            }
            if (target == "/dotlrn/file-storage/download?file_id=901") {
                g2_hits.fetch_add(1);
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: gzip\r\nContent-Length: " +
                       std::to_string(gz_tiny.size()) + "\r\nConnection: close\r\n\r\n" + gz_tiny;
            }
            if (target == "/dotlrn/file-storage/download?file_id=902") {
                g3_hits.fetch_add(1);
                if (headers_lower.find("range: bytes=10-") != std::string::npos) {
                    g3_saw_range.store(true);
                }
                if (headers_lower.find("accept-encoding: identity") != std::string::npos) {
                    g3_saw_identity.store(true);
                }
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: gzip\r\nContent-Length: " +
                       std::to_string(gz_g3.size()) + "\r\nConnection: close\r\n\r\n" + gz_g3;
            }
            if (target == "/dotlrn/file-storage/download?file_id=903") {
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: deflate\r\nContent-Length: " +
                       std::to_string(zlib_tiny.size()) + "\r\nConnection: close\r\n\r\n" + zlib_tiny;
            }
            if (target == "/dotlrn/file-storage/download?file_id=904") {
                partial_hits.fetch_add(1);
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Length: 200\r\nConnection: close\r\n\r\n" +
                       pdf_tiny;
            }
            return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        });
        CHECK(srv.Start());

        PolicyOptions popts;
        popts.allowed_test_origins = {srv.BaseUrl()};
        ReadOnlyRequestPolicy policy(popts);
        auto native = CreateNativeHttpTransport();
        PolicyEnforcingTransport guarded(*native, policy);

        AuthClientOptions aopts;
        aopts.base_url = srv.BaseUrl();
        aopts.perform_prelogin_handshake = false;
        AuthClient auth(guarded, policy, aopts);

        DownloaderOptions dopts;
        dopts.min_delay_ms = 0;
        dopts.backoff_ms = {1, 1};
        dopts.max_retries = 2;
        StorageDownloader dl(guarded, auth, srv.BaseUrl(), dopts);

        ScopedTempDir tmp("r3_gzip");
        fs::create_directories(tmp.dir / "downloads" / "_tmp");

        // G2: tiny gzip where compressed Content-Length > decompressed size succeeds in 1 hit
        FileRecord f901;
        f901.object_id = "901";
        f901.name = "tiny.pdf";
        f901.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=901"};
        auto r901 = dl.DownloadFile(f901, tmp.dir.u8string());
        CHECK(r901.outcome == DownloadOutcome::Downloaded);
        CHECK(r901.sha256 == Sha256Hex(pdf_tiny));
        CHECK(g2_hits.load() == 1);

        // G3: stale .part -> Range sent + Accept-Encoding: identity -> server replies 200 OK gzip -> Downloaded
        {
            std::ofstream ofs(tmp.dir / "downloads" / "_tmp" / "902.part", std::ios::binary);
            ofs << std::string(10, 'S');
        }
        FileRecord f902;
        f902.object_id = "902";
        f902.name = "fallback_gzip.pdf";
        f902.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=902"};
        auto r902 = dl.DownloadFile(f902, tmp.dir.u8string());
        CHECK(r902.outcome == DownloadOutcome::Downloaded);
        CHECK(r902.sha256 == Sha256Hex(pdf_g3));
        CHECK(r902.quarantine_rel_path.empty());
        CHECK(g3_hits.load() == 1);
        CHECK(g3_saw_range.load());
        CHECK(g3_saw_identity.load());

        // Deflate fallback on stale .part
        {
            std::ofstream ofs(tmp.dir / "downloads" / "_tmp" / "903.part", std::ios::binary);
            ofs << std::string(10, 'D');
        }
        FileRecord f903;
        f903.object_id = "903";
        f903.name = "fallback_deflate.pdf";
        f903.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=903"};
        auto r903 = dl.DownloadFile(f903, tmp.dir.u8string());
        CHECK(r903.outcome == DownloadOutcome::Downloaded);
        CHECK(r903.sha256 == Sha256Hex(pdf_deflate));

        // Partial response retry exhaustion returns Failed, NOT Orphan404
        FileRecord f904;
        f904.object_id = "904";
        f904.name = "partial_fail.pdf";
        f904.candidate_urls = {srv.BaseUrl() + "/dotlrn/file-storage/download?file_id=904"};
        auto r904 = dl.DownloadFile(f904, tmp.dir.u8string());
        CHECK(r904.outcome == DownloadOutcome::Failed);
        CHECK(partial_hits.load() == 3);
    }

    // 7c. [R3-5] Concurrent ScopedAllowLogin never latches AllowLogin() to true
    {
        auto mock = std::make_unique<MockHttpTransport>([&](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.final_url = req.url;
            ParsedUrl pu = ParseHttpUrl(req.url);
            if (pu.path == "/") {
                r.status_code = 200;
                r.set_cookies.push_back("AlteonP=sticky_r3; Path=/");
                r.body.assign(1, ' ');
                return r;
            }
            if (pu.path == "/api/login/new-form/" || pu.path == "/api/login/scanning/") {
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
                r.status_code = 200;
                r.set_cookies.push_back("ad_session_id=FAKE_R3_SID; Path=/; HttpOnly");
                std::string b = R"({"status":"ok","returnUrl":"/dotlrn/"})";
                r.body.assign(b.begin(), b.end());
                return r;
            }
            r.status_code = 200;
            std::string h = "<html>Portal Logout</html>";
            r.body.assign(h.begin(), h.end());
            return r;
        });

        ServiceConfig cfg;
        TecDigitalService svc(cfg, std::move(mock));

        std::vector<std::thread> workers;
        for (int i = 0; i < 6; ++i) {
            workers.emplace_back([&, i]() {
                if ((i % 2) == 0) {
                    Credentials c("user" + std::to_string(i) + "@example.invalid", "R3#ConcurrentPwd");
                    (void)svc.LoginWithCredentials(c, false);
                } else {
                    (void)svc.AttemptQrScanning("user" + std::to_string(i) + "@example.invalid", "");
                }
            });
        }
        for (auto& th : workers) th.join();

        CHECK(!svc.GuardedTransport().AllowLogin());
        HttpRequest blocked_login;
        blocked_login.method = "POST";
        blocked_login.url = "https://tecdigital.tec.ac.cr/api/login/new-form/";
        blocked_login.body = "email=x&password=y";
        HttpResponse blocked_resp = svc.GuardedTransport().Execute(blocked_login);
        CHECK(!blocked_resp.transport_ok);
        CHECK(blocked_resp.error_message.find("login-outside-authentication-phase") != std::string::npos);
    }

    // 7d. [R3-6] ScanReusableTokens kQueryRe <> boundary on sanitized HTML/XML
    {
        Sanitizer san;
        CHECK(san.ScanReusableTokens("<div>https://zoom.us/j/1?pwd=[REDACTED:sha256=22fd1d54]</div>").empty());
        auto raw_hits = san.ScanReusableTokens("<div>https://zoom.us/j/1?pwd=UNREDACTED_SECRET</div>");
        CHECK(!raw_hits.empty());
    }

    // 7e. [R3-7 & R3-8] Direct TdaTdsClient structured-output sanitization & ParseRssXml item boundary/CDATA
    {
        auto rss = TdaTdsClient::ParseRssXml(
            "<rss><channel>"
            "<item_extra><title>NO</title></item_extra>"
            "<item attr=\"a>b\"><title>OK</title>"
            "<description><![CDATA[</item> inside cdata]]></description>"
            "<link>https://x/1</link></item>"
            "</channel></rss>");
        CHECK(rss.ok && rss.value.size() == 1);
        CHECK(rss.value[0].title == "OK");
        CHECK(rss.value[0].link == "https://x/1");
        CHECK(rss.value[0].description == "</item> inside cdata");

        const std::string pwd = "R3#DirectTdaPwd!";
        const std::string self_mail = "r3.self@estudiantec.cr";
        const std::string peer_mail = "r3.peer@estudiantec.cr";

        auto mock = std::make_unique<MockHttpTransport>([&](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.final_url = req.url;
            ParsedUrl pu = ParseHttpUrl(req.url);
            const std::string& p = pu.path;
            auto ok_json = [&](std::string body, std::string ct = "application/json") {
                r.status_code = 200;
                r.content_type = std::move(ct);
                r.body.assign(body.begin(), body.end());
                return r;
            };
            auto ok_html = [&](std::string body) {
                return ok_json(std::move(body), "text/html; charset=utf-8");
            };

            if (p == "/") {
                r.set_cookies.push_back("AlteonP=sticky_r3_tda; Path=/");
                return ok_html("<html>Login</html>");
            }
            if (p == "/api/login/new-form/") {
                r.set_cookies.push_back("ad_session_id=FAKE_R3_TDA_SID; Path=/; HttpOnly");
                return ok_json(R"({"status":"ok","returnUrl":"/dotlrn/"})");
            }
            if (p == "/dotlrn/" || p == "/dotlrn/index") {
                return ok_html("<html><a href='/dotlrn/classes/E/EL2113/S1'>Curso " + peer_mail + "</a></html>");
            }
            if (p == "/tda-notifications/ajax/get_user_notifications") {
                return ok_json(R"({"category_sort":true,"notifications":[{"notification_id":"n1","origin":1,"title":"T","text":"Join Bearer DIRECT_BEARER_SECRET_999 user )" +
                               peer_mail + R"(","url":"https://tec-cr.zoom.us/j/888?pwd=Z00MR3DIRECT","state":"U"}]})");
            }
            if (p == "/tda-notifications/ajax/has_unread_notifications") return ok_json(R"({"quantity":1})");
            if (p.find("/news/rss/rss.xml") != std::string::npos) {
                return ok_json("<rss><channel><item><title>" + pwd + "</title><link>https://zoom.us/j/2?pwd=RSSR3SEC</link><description>" +
                               peer_mail + "</description></item></channel></rss>", "application/xml");
            }
            if (p.find("/ajax/load_members") != std::string::npos) {
                return ok_json(R"({"data":[{"checkcCol":"u1","firstName":"A","lastName":"B","emailCol":")" + peer_mail + R"(","rolCol":"Est"}]})");
            }
            if (p.find("/evaluation/tda-ce-estudiante/tda-index") != std::string::npos) {
                return ok_html("<div id='studentFinal'>95</div><div class='sub_total'>Cat</div><div assign_id='a1'><span class='assignNameText'>Bearer DIRECT_EVAL_BEARER</span><span class='vt_grade_student'>19 / 20</span></div>");
            }
            if (p == "/tda-expediente-estudiantil/ajax/detalle_notas") {
                return ok_html("<div>Notas " + pwd + " " + peer_mail + " https://zoom.us/j/1?pwd=EXP_PWD</div>");
            }
            if (p == "/tda-prematricula/ajax/prematricula_est") {
                return ok_html("18~" + pwd + "~EL2113~1~NA~" + peer_mail);
            }
            if (p == "/tds-curriculum-exp/ajax/obtener_datos_carrera_estudiante") {
                return ok_json(R"({"nombre":")" + self_mail + R"(","sede":"SC","carrera":"E","plan":809})");
            }
            if (p == "/tda-feria-negocios/ajax/registrar_proyectos/getInfoEstudiante") {
                return ok_json(R"({"correo":")" + peer_mail + R"(","token":"FERIA_TOK_99"})");
            }
            return ok_json("{}");
        });

        ServiceConfig cfg;
        cfg.sanitize_options.shareable_mode = true;
        TecDigitalService svc(cfg, std::move(mock));
        Credentials creds(self_mail, pwd);
        CHECK(svc.LoginWithCredentials(creds, false).ok);

        const auto& san = svc.GetSanitizer();

        auto nf = svc.TdaClient().GetUserNotifications();
        CHECK(nf.ok && nf.value.notifications.size() == 1);
        CHECK(san.ScanForLeaks(nf.value.notifications[0].text).passed);
        CHECK(san.ScanForLeaks(nf.value.notifications[0].url).passed);

        auto rss_r = svc.TdaClient().GetCommunityNewsRss("/dotlrn/classes/E/EL2113/S1");
        CHECK(rss_r.ok && rss_r.value.size() == 1);
        CHECK(san.ScanForLeaks(rss_r.value[0].title).passed);
        CHECK(san.ScanForLeaks(rss_r.value[0].link).passed);
        CHECK(san.ScanForLeaks(rss_r.value[0].description).passed);

        auto mem_r = svc.TdaClient().GetCommunityMembers("/dotlrn/classes/E/EL2113/S1");
        CHECK(mem_r.ok && mem_r.value.size() == 1);
        CHECK(san.ScanForLeaks(mem_r.value[0].email).passed);

        auto ev_r = svc.TdaClient().GetCourseEvaluations("/dotlrn/classes/E/EL2113/S1");
        CHECK(ev_r.ok && ev_r.value.categories.size() == 1);
        CHECK(san.ScanForLeaks(ev_r.value.categories[0].items[0].name).passed);

        auto exp_r = svc.TdaClient().GetExpedienteSummary();
        CHECK(exp_r.ok);
        CHECK(san.ScanForLeaks(exp_r.value.detalle_notas_html).passed);

        auto pm_r = svc.TdaClient().GetPrematriculaSnapshot("1", "809");
        CHECK(pm_r.ok && pm_r.value.courses.size() == 1);
        CHECK(san.ScanForLeaks(pm_r.value.rn_student).passed);
        CHECK(san.ScanForLeaks(pm_r.value.courses[0].name_elective).passed);

        auto car_r = svc.TdaClient().GetStudentCareerData();
        CHECK(car_r.ok);
        CHECK(san.ScanForLeaks(car_r.value.nombre).passed);

        auto fer_r = svc.TdaClient().GetFeriaNegociosSnapshot();
        CHECK(fer_r.ok);
        CHECK(san.ScanForLeaks(fer_r.value.info_estudiante_raw_json).passed);

        auto comm_r = svc.TdaClient().FetchCoursesAndCommunities(false);
        CHECK(comm_r.ok && comm_r.value.size() == 1);
        CHECK(san.ScanForLeaks(comm_r.value[0].name).passed);
    }
}

void TestR4Remediations() {
    // 8a. [R4-1, R4-2, R4-3] Native Win & Linux HTTP transports:
    //     - raw RFC 1951 Content-Encoding: deflate (with & without Range)
    //     - multi-member Content-Encoding: gzip (with & without Range, plus cross-chunk boundary)
    //     - corrupted gzip CRC32 trailer & trailing garbage (with & without Range -> transport_ok == false)
    {
        const std::string pdf_raw_deflate = "%PDF-1.4\n%RAW_RFC1951_DEFLATE_OK\n";
        const std::string raw_deflate_wire = BuildStoredRawDeflate(pdf_raw_deflate);

        const std::string part1 = "%PDF-1.4\n%MM_PART1_";  // 19 bytes
        const std::string part2 = "MM_PART2_OK32";        // 13 bytes -> 32 bytes total
        const std::string expected_multi = part1 + part2;
        CHECK(expected_multi.size() == 32);
        const std::string gz_m1 = BuildStoredGzip(part1);
        const std::string gz_m2 = BuildStoredGzip(part2);
        const std::string multi_gz_wire = gz_m1 + gz_m2;
        const std::string bad_crc_gz = [&]() {
            std::string s = BuildStoredGzip(pdf_raw_deflate);
            s[s.size() - 5] ^= 0xFF;  // corrupt CRC32 trailer
            return s;
        }();
        const std::string trailing_garbage_gz = gz_m1 + "TRAILING_GARBAGE";
        const std::string trunc_raw_deflate = raw_deflate_wire.substr(0, raw_deflate_wire.size() - 3);

        LoopbackHttpServer srv([&](const std::string& method,
                                   const std::string& target,
                                   const std::string&,
                                   const std::string&) -> std::string {
            if (method == "HEAD") {
                return "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            }
            if (target == "/raw-deflate") {
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: deflate\r\nContent-Length: " +
                       std::to_string(raw_deflate_wire.size()) + "\r\nConnection: close\r\n\r\n" + raw_deflate_wire;
            }
            if (target == "/raw-deflate-trunc") {
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: deflate\r\nContent-Length: " +
                       std::to_string(trunc_raw_deflate.size()) + "\r\nConnection: close\r\n\r\n" + trunc_raw_deflate;
            }
            if (target == "/multi-gzip") {
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: gzip\r\nContent-Length: " +
                       std::to_string(multi_gz_wire.size()) + "\r\nConnection: close\r\n\r\n" + multi_gz_wire;
            }
            if (target == "/multi-gzip-chunked") {
                // Split mid-way through member 2 so member boundary and member 2 span chunks
                const size_t split_pos = gz_m1.size() + 7;
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: x-gzip\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n" +
                       ToHttpChunked(multi_gz_wire, split_pos);
            }
            if (target == "/bad-crc-gzip") {
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: gzip\r\nContent-Length: " +
                       std::to_string(bad_crc_gz.size()) + "\r\nConnection: close\r\n\r\n" + bad_crc_gz;
            }
            if (target == "/trailing-garbage-gzip") {
                const size_t split_pos = gz_m1.size();  // exact boundary between chunk 1 and trailing garbage
                return "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nContent-Encoding: gzip\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n" +
                       ToHttpChunked(trailing_garbage_gz, split_pos);
            }
            return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        });
        CHECK(srv.Start());

        auto native = CreateNativeHttpTransport();
        CHECK(native != nullptr);

        // 1. Raw RFC 1951 deflate: without Range and with Range
        for (uint64_t rstart : {uint64_t{0}, uint64_t{10}}) {
            HttpRequest req;
            req.url = srv.BaseUrl() + "/raw-deflate";
            req.range_start = rstart;
            HttpResponse resp = native->Execute(req);
            CHECK(resp.transport_ok && resp.status_code == 200);
            CHECK(resp.BodyAsString() == pdf_raw_deflate);
        }
        {
            HttpRequest req;
            req.url = srv.BaseUrl() + "/raw-deflate-trunc";
            HttpResponse resp = native->Execute(req);
            CHECK(!resp.transport_ok);
        }

        // 2. Multi-member gzip (Content-Length & cross-chunk Transfer-Encoding: chunked): without Range and with Range
        for (const char* path : {"/multi-gzip", "/multi-gzip-chunked"}) {
            for (uint64_t rstart : {uint64_t{0}, uint64_t{10}}) {
                HttpRequest req;
                req.url = srv.BaseUrl() + path;
                req.range_start = rstart;
                HttpResponse resp = native->Execute(req);
                CHECK(resp.transport_ok && resp.status_code == 200);
                CHECK(resp.BodyAsString() == expected_multi);
            }
        }

        // 3. Corrupt gzip CRC32 trailer & trailing garbage: without Range and with Range -> transport_ok == false
        for (const char* path : {"/bad-crc-gzip", "/trailing-garbage-gzip"}) {
            for (uint64_t rstart : {uint64_t{0}, uint64_t{10}}) {
                HttpRequest req;
                req.url = srv.BaseUrl() + path;
                req.range_start = rstart;
                HttpResponse resp = native->Execute(req);
                CHECK(!resp.transport_ok);
            }
        }

        // StorageDownloader stale .part + Range fallback with raw deflate & multi-member gzip
        PolicyOptions popts;
        popts.allowed_test_origins = {srv.BaseUrl()};
        ReadOnlyRequestPolicy policy(popts);
        AuthClientOptions aopts;
        aopts.base_url = srv.BaseUrl();
        aopts.perform_prelogin_handshake = false;
        PolicyEnforcingTransport guarded(*native, policy);
        AuthClient auth(guarded, policy, aopts);
        DownloaderOptions dopts;
        dopts.min_delay_ms = 0;
        dopts.backoff_ms = {1};
        dopts.max_retries = 1;
        StorageDownloader dl(guarded, auth, srv.BaseUrl(), dopts);

        ScopedTempDir tmp("r4_dl");
        fs::create_directories(tmp.dir / "downloads" / "_tmp");
        {
            std::ofstream ofs(tmp.dir / "downloads" / "_tmp" / "951.part", std::ios::binary);
            ofs << std::string(10, 'R');
        }
        FileRecord f951;
        f951.object_id = "951";
        f951.name = "raw_deflate.pdf";
        f951.candidate_urls = {srv.BaseUrl() + "/raw-deflate"};
        auto r951 = dl.DownloadFile(f951, tmp.dir.u8string());
        CHECK(r951.outcome == DownloadOutcome::Downloaded);
        CHECK(r951.sha256 == Sha256Hex(pdf_raw_deflate));

        {
            std::ofstream ofs(tmp.dir / "downloads" / "_tmp" / "952.part", std::ios::binary);
            ofs << std::string(10, 'M');
        }
        FileRecord f952;
        f952.object_id = "952";
        f952.name = "multi_gz.pdf";
        f952.candidate_urls = {srv.BaseUrl() + "/multi-gzip-chunked"};
        auto r952 = dl.DownloadFile(f952, tmp.dir.u8string());
        CHECK(r952.outcome == DownloadOutcome::Downloaded);
        CHECK(r952.sha256 == Sha256Hex(expected_multi));
    }

    // 8b. [R4-4] Cross-component sanitizer_ thread safety across TecDigitalService, TdaTdsClient,
    //     and OfflineMaterialIndex, plus prior needle preservation across overlapping logins
    {
        ScopedTempDir tmp("r4_tsan");
        const std::string pwd_a = "R4#AlphaPwd!91";
        const std::string mail_a = "r4.alpha@estudiantec.cr";
        const std::string pwd_b = "R4#BetaPwd!92";
        const std::string mail_b = "r4.beta@estudiantec.cr";
        const std::string peer_mail = "r4.peer@estudiantec.cr";

        auto mock = std::make_unique<MockHttpTransport>([&](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.final_url = req.url;
            ParsedUrl pu = ParseHttpUrl(req.url);
            const std::string& p = pu.path;
            auto ok_json = [&](std::string body, std::string ct = "application/json") {
                r.status_code = 200;
                r.content_type = std::move(ct);
                r.body.assign(body.begin(), body.end());
                return r;
            };
            auto ok_html = [&](std::string body) {
                return ok_json(std::move(body), "text/html; charset=utf-8");
            };

            if (p == "/") {
                r.set_cookies.push_back("AlteonP=sticky_r4; Path=/");
                return ok_html("<html>Login</html>");
            }
            if (p == "/api/login/new-form/" || p == "/api/login/scanning/") {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                r.set_cookies.push_back("ad_session_id=FAKE_R4_SID; Path=/; HttpOnly");
                return ok_json(R"({"status":"ok","returnUrl":"/dotlrn/"})");
            }
            if (p == "/dotlrn/" || p == "/dotlrn/index") {
                return ok_html("<html>Portal Logout <a href='/dotlrn/classes/E/EL2113/S1'>Curso " +
                               peer_mail + " " + pwd_a + "</a></html>");
            }
            if (p == "/tda-notifications/ajax/get_user_notifications") {
                return ok_json(R"({"category_sort":false,"notifications":[{"notification_id":"n1","origin":1,"title":"N","text":"Bearer R4_BEARER_SECRET_999 )" +
                               mail_a + " " + pwd_b + R"(","url":"https://zoom.us/j/1?pwd=R4Z00MPWD","state":"U"}]})");
            }
            if (p == "/tda-notifications/ajax/has_unread_notifications") return ok_json(R"({"quantity":1})");
            if (p == "/theme-td/ajax/get_date_server") return ok_json("1700000000000");
            if (p == "/tda-um/ajax/get_gaap_server") return ok_json(R"({"gaap_server":"https://gaap.tec.ac.cr/?token=GAAPTOK"})");
            if (p == "/tds-curriculum-exp/ajax/carga_sedes_json") return ok_json(R"([{"key":"1","data":"Cartago"}])");
            if (p == "/tds-curriculum-exp/ajax/obtener_datos_carrera_estudiante") {
                return ok_json(R"({"nombre":")" + mail_a + R"(","sede":"Cartago","carrera":"EL","plan":412})");
            }
            if (p == "/tda-inclusion-manager/api/user-transaction") {
                return ok_json(R"({"transaction_key":"TX_R4_KEY"})");
            }
            return ok_json("{}");
        });

        ServiceConfig cfg;
        cfg.offline_root = tmp.dir.u8string();
        cfg.sanitize_options.shareable_mode = true;
        TecDigitalService svc(cfg, std::move(mock));

        // Login with Account A then Account B -> verify prior Account A needles preserved
        CHECK(svc.LoginWithCredentials(Credentials(mail_a, pwd_a), false).ok);
        CHECK(svc.LoginWithCredentials(Credentials(mail_b, pwd_b), false).ok);
        {
            Sanitizer snap = svc.SnapshotSanitizer();
            std::string mixed = "A=" + pwd_a + " " + mail_a + " B=" + pwd_b + " " + mail_b;
            std::string scrubbed = snap.SanitizeText(mixed);
            CHECK(scrubbed.find(pwd_a) == std::string::npos);
            CHECK(scrubbed.find(mail_a) == std::string::npos);
            CHECK(scrubbed.find(pwd_b) == std::string::npos);
            CHECK(scrubbed.find(mail_b) == std::string::npos);
        }

        // Concurrent stress: logins + TdaClient().GetUserNotifications() + FetchCoursesAndCommunities() +
        // FetchTdaTdsCatalog() + OfflineMaterialIndex::IndexBlob/SaveToDisk
        std::atomic<bool> any_leak{false};
        std::atomic<bool> any_error{false};
        std::vector<std::thread> workers;

        for (int t = 0; t < 2; ++t) {
            workers.emplace_back([&, t]() {
                for (int i = 0; i < 12; ++i) {
                    if ((i + t) % 2 == 0) {
                        auto ar = svc.LoginWithCredentials(
                            Credentials((t == 0) ? mail_a : mail_b, (t == 0) ? pwd_a : pwd_b), false);
                        if (!ar.ok) any_error.store(true);
                    } else {
                        auto ar = svc.AttemptQrScanning((t == 0) ? mail_a : mail_b, "");
                        if (!ar.ok) any_error.store(true);
                    }
                }
            });
        }

        workers.emplace_back([&]() {
            Sanitizer check_a(SanitizeOptions{true, true, false, mail_a, pwd_a, {peer_mail}, 8000});
            Sanitizer check_b(SanitizeOptions{true, true, false, mail_b, pwd_b, {peer_mail}, 8000});
            for (int i = 0; i < 15; ++i) {
                auto nf = svc.TdaClient().GetUserNotifications();
                if (!nf.ok || nf.value.notifications.empty()) {
                    any_error.store(true);
                    continue;
                }
                const auto& n = nf.value.notifications[0];
                if (!check_a.ScanForLeaks(n.text).passed || !check_b.ScanForLeaks(n.text).passed ||
                    !check_a.ScanForLeaks(n.url).passed || !check_b.ScanForLeaks(n.url).passed ||
                    !check_a.ScanReusableTokens(n.text).empty() ||
                    !check_a.ScanReusableTokens(n.url).empty()) {
                    any_leak.store(true);
                }
            }
        });

        workers.emplace_back([&]() {
            Sanitizer check_a(SanitizeOptions{true, true, false, mail_a, pwd_a, {peer_mail}, 8000});
            for (int i = 0; i < 12; ++i) {
                auto cr = svc.FetchCoursesAndCommunities(false);
                if (!cr.ok || cr.value.empty()) {
                    any_error.store(true);
                    continue;
                }
                if (!check_a.ScanForLeaks(cr.value[0].name).passed) {
                    any_leak.store(true);
                }
            }
        });

        workers.emplace_back([&]() {
            Sanitizer check_a(SanitizeOptions{true, true, false, mail_a, pwd_a, {peer_mail}, 8000});
            Sanitizer check_b(SanitizeOptions{true, true, false, mail_b, pwd_b, {peer_mail}, 8000});
            TdaTdsCatalogOptions copts;
            copts.fetch_expediente = false;
            copts.fetch_evaluations = false;
            copts.fetch_news_rss = false;
            for (int i = 0; i < 10; ++i) {
                auto cat = svc.FetchTdaTdsCatalog(copts);
                if (!cat.ok) {
                    any_error.store(true);
                    continue;
                }
                if (cat.value.student_career.has_value() &&
                    !check_a.ScanForLeaks(cat.value.student_career->nombre).passed) {
                    any_leak.store(true);
                }
                if (cat.value.notifications.has_value() && !cat.value.notifications->notifications.empty()) {
                    const auto& nt = cat.value.notifications->notifications[0];
                    if (!check_b.ScanForLeaks(nt.text).passed || !check_b.ScanForLeaks(nt.url).passed) {
                        any_leak.store(true);
                    }
                }
            }
        });

        workers.emplace_back([&]() {
            for (int i = 0; i < 10; ++i) {
                IndexedBlobInput in;
                in.object_id = "r4_obj_" + std::to_string(i % 3);
                in.name = "doc_" + pwd_a + "_" + mail_b + ".txt";
                std::string txt = "Texto " + pwd_a + " " + pwd_b + " " + mail_a + " " + mail_b;
                in.inline_bytes.assign(txt.begin(), txt.end());
                if (!svc.MaterialIndex().IndexBlob(in).ok || !svc.MaterialIndex().SaveToDisk().ok) {
                    any_error.store(true);
                }
            }
        });

        for (auto& w : workers) w.join();

        CHECK(!any_error.load());
        CHECK(!any_leak.load());
        CHECK(!svc.GuardedTransport().AllowLogin());

        Sanitizer check_a(SanitizeOptions{true, true, false, mail_a, pwd_a, {}, 8000});
        Sanitizer check_b(SanitizeOptions{true, true, false, mail_b, pwd_b, {}, 8000});
        CHECK(check_a.ScanDirectoryForLeaks((tmp.dir / "material_index").u8string()).passed);
        CHECK(check_b.ScanDirectoryForLeaks((tmp.dir / "material_index").u8string()).passed);

        HttpRequest blocked_login;
        blocked_login.method = "POST";
        blocked_login.url = "https://tecdigital.tec.ac.cr/api/login/new-form/";
        blocked_login.body = "email=x&password=y";
        HttpResponse blocked_resp = svc.GuardedTransport().Execute(blocked_login);
        CHECK(!blocked_resp.transport_ok);
        CHECK(blocked_resp.error_message.find("login-outside-authentication-phase") != std::string::npos);
    }
}

void TestPhase2LiveProtocolAndGraphQl() {
    // 1. Live Guia de Horarios parser: "Dia - H:M:H:M" + 1-digit minutes + accents + (codigo, grupo) merge
    {
        const std::string liveGuiaHtml = R"html(
            <table id="tguiaHorario">
                <thead><tr>
                    <th>Código</th><th>Materia</th><th>Grupo</th><th>Créditos</th><th>Horario</th>
                    <th>Aula</th><th>Profesor</th><th>Cupo</th><th>Tipo Materia</th><th>Tipo Grupo</th><th>Reservados</th>
                </tr></thead>
                <tbody>
                    <tr><td>EL2113</td><td>Circuitos Eléctricos I</td><td>050</td><td>4</td><td>Lunes - 7:55:9:40</td><td>B3-04</td><td>Prof. Sintético A</td><td>25</td><td>Teoría</td><td>Regular</td><td>3</td></tr>
                    <tr><td>EL2113</td><td>Circuitos Eléctricos I</td><td>50</td><td>4</td><td>Jueves - 7:0:8:45</td><td>B3-04</td><td>Prof. Sintético A</td><td>25</td><td>Teoría</td><td>Regular</td><td>3</td></tr>
                    <tr><td>EL2113</td><td>Circuitos Eléctricos I</td><td>50</td><td>4</td><td>Miércoles - 12:30:16:5</td><td>Lab-02</td><td>Prof. Sintético A</td><td>25</td><td>Teoría</td><td>Regular</td><td>3</td></tr>
                    <tr><td>FI1102</td><td>Física General II</td><td>01</td><td>4</td><td>Miercoles - 12:30:16:5</td><td>D2-01</td><td>Prof. Sintético B</td><td>30</td><td>Teoría</td><td>Regular</td><td>0</td></tr>
                    <tr><td>FI1102</td><td>Física General II</td><td>1</td><td>4</td><td>Sábado - 8:0:11:30</td><td>D2-01</td><td>Prof. Sintético B</td><td>30</td><td>Teoría</td><td>Regular</td><td>0</td></tr>
                </tbody>
            </table>
        )html";
        auto rows = ParseTablaGuiaHorarioHtml(liveGuiaHtml);
        CHECK(rows.size() == 5);
        std::string jsonStr = GuiaHorarioRowsToHorariosTecJson(rows, "Guia Live", "2026_S_2", "Cartago");
        auto j = nlohmann::json::parse(jsonStr);
        CHECK(j["courses"].size() == 2);
        CHECK(j["courses"][0]["code"] == "EL2113");
        CHECK(j["courses"][0]["groups"].size() == 1);  // 050 and 50 merged!
        CHECK(j["courses"][0]["groups"][0]["sessions"].size() == 3);
        CHECK(j["courses"][0]["groups"][0]["sessions"][0]["day"] == "L");
        CHECK(j["courses"][0]["groups"][0]["sessions"][0]["start"] == "07:55");
        CHECK(j["courses"][0]["groups"][0]["sessions"][0]["end"] == "09:40");
        CHECK(j["courses"][0]["groups"][0]["sessions"][1]["day"] == "J");
        CHECK(j["courses"][0]["groups"][0]["sessions"][1]["start"] == "07:00");
        CHECK(j["courses"][0]["groups"][0]["sessions"][1]["end"] == "08:45");
        CHECK(j["courses"][0]["groups"][0]["sessions"][2]["day"] == "M");
        CHECK(j["courses"][0]["groups"][0]["sessions"][2]["start"] == "12:30");
        CHECK(j["courses"][0]["groups"][0]["sessions"][2]["end"] == "16:05");
        CHECK(j["courses"][1]["groups"].size() == 1);
        CHECK(j["courses"][1]["groups"][0]["sessions"].size() == 2);
        CHECK(j["courses"][1]["groups"][0]["sessions"][1]["day"] == "S");
        CHECK(j["courses"][1]["groups"][0]["sessions"][1]["start"] == "08:00");
        CHECK(j["courses"][1]["groups"][0]["sessions"][1]["end"] == "11:30");
    }

    // 2. Period format bidirectional conversion & JSON/<span value>/<option value> combo parser
    {
        CHECK(ConvertPeriodKeyToExpedienteFormat("S-2-2026") == "2026_S_2");
        CHECK(ConvertPeriodKeyToExpedienteFormat("2026_S_2") == "2026_S_2");
        CHECK(ConvertExpedientePeriodToDotlrnFormat("2026_S_2") == "S-2-2026");

        auto cJson = ParseExpedienteComboJsonOrHtml(R"([{"key":"2026_S_2","data":"2026 - Semestre 2"},{"key":"2026_S_1","data":"2026 - Semestre 1"}])");
        CHECK(cJson.size() == 2 && cJson[0].first == "2026_S_2" && cJson[0].second == "2026 - Semestre 2");

        auto cSpan = ParseExpedienteComboJsonOrHtml("<span value='SC'>San Carlos</span><span value='CA'>Cartago</span>");
        CHECK(cSpan.size() == 2 && cSpan[0].first == "SC" && cSpan[1].second == "Cartago");

        auto cOpt = ParseExpedienteComboJsonOrHtml("<option value='E'>Electrónica</option>");
        CHECK(cOpt.size() == 1 && cOpt[0].first == "E");
    }

    // 3. FileStorageWalker::ExtractRootFolderIds strips #header-td and platform_user_panel personal root
    {
        const std::string portalHtml = R"html(
            <html><body>
                <header id="header-td">
                    <nav><a href="/dotlrn/file-storage/#/153108817">Mis documentos</a></nav>
                </header>
                <div id="platform_user_panel">
                    <a href="/dotlrn/file-storage/?folder_id=153108817">Panel</a>
                </div>
                <main>
                    <a href="/dotlrn/estudiantes.tec/file-storage/#/116796971">Documentos Comunidad</a>
                </main>
            </body></html>
        )html";
        auto ids = FileStorageWalker::ExtractRootFolderIds(portalHtml);
        CHECK(ids.size() == 1);
        CHECK(ids[0] == "116796971");
    }

    // 4. Pure Phase 2 parsers: #AttrStudent (11 rows), FileVersionsJson, EvalHistory/Live, PlagioMisDocs, Forum, Performance, Custom Tabs
    {
        std::ostringstream attrHtml;
        attrHtml << "<table id='AttrStudent'><tr><th>Atributo</th><th>Descripción</th><th>Cursos</th>"
                    "<th>Porcentaje evaluado</th><th>Inicial</th><th>Medio</th><th>Avanzado</th><th>Total</th></tr>";
        for (int i = 1; i <= 11; ++i) {
            attrHtml << "<tr><td>Atributo " << i << "</td><td>Desc " << i << "</td><td>EL2113, FI1102</td>"
                     << "<td>" << (50 + i) << "%</td><td>20%</td><td>35%</td><td>45%</td><td>100%</td></tr>";
        }
        attrHtml << "</table>";
        auto attrs = ParseGraduateAttributesHtml(attrHtml.str());
        CHECK(attrs.size() == 11);
        CHECK(attrs[0].attribute_name == "Atributo 1");
        CHECK(attrs[0].evaluated_pct == 51.0);
        CHECK(attrs[10].advanced_pct == 45.0);

        const std::string verJson = R"json([
            {"title":"Guia_v2.pdf","live_revision":"9902","author":{"name":"Docente A"},"mime_type":"application/pdf",
             "type":"content_revision","pretty_type":"Archivo","last_modified_ansi":"2026-09-12 10:00:00",
             "content_size":20480,"description":"Corrección ejercicio 3","fs_type":"file","fs_sort_relevance":1,
             "privileges":{"read_p":1}},
            {"title":"Guia_v1.pdf","live_revision":"9901","author":"Docente A","mime_type":"application/pdf",
             "type":"content_revision","pretty_type":"Archivo","last_modified_ansi":"2026-09-01 08:00:00",
             "content_size":18432,"description":"Versión inicial","fs_type":"file","fs_sort_relevance":2,
             "privileges":{"read_p":true}}
        ])json";
        auto vers = ParseFileVersionsJson(verJson);
        CHECK(vers.size() == 2 && vers[0].live_revision == "9902" && vers[0].content_size == 20480 && vers[1].author == "Docente A");

        const std::string evalHistHtml = R"html(
            <div class="lineFiles"><a href="/dotlrn/classes/E/EL2113/S1/evaluation/view/tarea1.pdf?revision_id=501">tarea1.pdf</a> 10/09/2026 21:30</div>
            <div class="lineFiles"><a href="/dotlrn/classes/E/EL2113/S1/evaluation/view/tarea1_v2.pdf?revision_id=502">tarea1_v2.pdf</a> 11/09/2026 22:15</div>
        )html";
        auto hist = ParseEvalHistoryHtml(evalHistHtml, "https://tecdigital.tec.ac.cr");
        CHECK(hist.size() == 2 && hist[0].revision_id == "501" && hist[1].file_name == "tarea1_v2.pdf");

        const std::string evalLiveHtml = R"html(
            <div><a href="/dotlrn/classes/E/EL2113/S1/evaluation/view/tarea1_final.pdf?revision_id=503">tarea1_final.pdf</a>
            <p>Día de entrega: 12/09/2026</p><p>Hora de entrega: 23:40</p></div>
        )html";
        auto live = ParseEvalLiveRevisionHtml(evalLiveHtml, "https://tecdigital.tec.ac.cr");
        CHECK(live.has_value() && live->is_live_revision && live->revision_id == "503" && live->delivered_date == "12/09/2026");

        const std::string misDocsHtml = R"html(
            <span class='documento' check_format='1' item_id='7701' id='DOC77' fecha='2026-09-10' hora='14:20' status='Completado' visibilidad='Privado' porcentaje='12.5' package_from='myfilestorage' community_id='10'>Informe_Lab1.pdf</span>
        )html";
        auto misDocs = ParsePlagiarismMisDocsHtml(misDocsHtml);
        CHECK(misDocs.size() == 1 && misDocs[0].item_id == "7701" && misDocs[0].file_name == "Informe_Lab1.pdf" && misDocs[0].similarity_pct == 12.5);

        const std::string forumHtml = R"html(
            <header id="header-td"><div class="message_header">Ignore Header</div></header>
            <div class="message_header"><a href="message-view?message_id=8801"></a><span class="message_subject">Consulta Lab 2</span><span class="forum_user_name">Estudiante A</span> 2026-09-14 09:00<div class="content forum_message">¿Cuál resistencia usar?</div></div>
            <div class="message_header"><a href="message-view?message_id=8802"></a><span class="message_subject">Re: Consulta Lab 2</span><span class="forum_user_name">Docente B</span> 2026-09-14 10:15<div class="content forum_message">Use 1 kOhm.</div></div>
        )html";
        auto posts = ParseForumThreadMessagesHtml(forumHtml);
        CHECK(posts.size() == 2 && posts[0].author == "Estudiante A" && posts[1].body_text.find("1 kOhm") != std::string::npos);

        const std::string perfJson = R"json([{"id":"PE-1","course_name":"Circuitos I","teacher_name":"Docente A","form_name":"Evaluación Semestral","status":"Pendiente","deadline":"2026-11-30"}])json";
        auto perfs = ParsePerformanceEvaluationsJson(perfJson);
        CHECK(perfs.size() == 1 && perfs[0].course_name == "Circuitos I" && perfs[0].status == "Pendiente");

        const std::string catedraHtml = R"html(
            <html><head><title>Cátedra Física II</title></head><body>
                <header id="header-td"><a href="/dotlrn/one-community?page_num=99">Portal Ignorar</a></header>
                <nav>
                    <a href="/dotlrn/catedras.tec/fisica2/one-community?page_num=0&loc=">Portada</a>
                    <a href="/dotlrn/catedras.tec/fisica2/one-community?page_num=55&loc=">Repositorio de exámenes</a>
                </nav>
            </body></html>
        )html";
        auto ov = ParseCourseOverviewHtml(catedraHtml, "/dotlrn/catedras.tec/fisica2", "https://tecdigital.tec.ac.cr");
        CHECK(ov.tabs.size() == 2);
        CHECK(ov.tabs[1].page_num == 55 && ov.tabs[1].is_custom && ov.tabs[1].label == "Repositorio de exámenes");

        const std::string tab55Html = R"html(
            <div><a href="/dotlrn/catedras.tec/fisica2/file-storage/view/examen_2025.pdf">Examen I Parcial 2025</a></div>
        )html";
        auto tlinks = ExtractCustomTabLinksHtml(tab55Html, "Repositorio de exámenes", "https://tecdigital.tec.ac.cr");
        CHECK(tlinks.size() == 1 && tlinks[0].category == "PDF");
    }

    // 5. Real 127.0.0.1 loopback server testing TecGraphQlClient + TdaTdsClient Phase 2 protocol
    {
        // Synthetic unsigned JWT carrying {"userId":"9001"} in base64url payload (zero PII)
        const std::string synthJwt = "eyJhbGciOiJIUzI1NiJ9.eyJ1c2VySWQiOiI5MDAxIn0.synthsig";
        std::atomic<int> gqlCalls{0};
        std::atomic<int> gqlRejectedMissingToken{0};
        std::atomic<int> sawStudentIdInAttr{0};
        std::atomic<int> sawCarneInHorario{0};
        std::atomic<int> sawMembersPost{0};

        LoopbackHttpServer srv([&](const std::string& method,
                                   const std::string& target,
                                   const std::string& headers_low,
                                   const std::string& body) -> std::string {
            auto reply = [](int code, const std::string& ct, const std::string& b) {
                return "HTTP/1.1 " + std::to_string(code) + (code == 200 ? " OK" : " Unauthorized") +
                       "\r\nContent-Type: " + ct + "\r\nContent-Length: " + std::to_string(b.size()) +
                       "\r\nConnection: close\r\n\r\n" + b;
            };

            if (target == "/tda-expediente-estudiantil/") {
                return reply(200, "text/html", "<input id='carne' value='2026999999'/>");
            }
            if (target == "/tda-expediente-estudiantil/ajax/get_data_attributes" && method == "POST") {
                if (body.find("student_id=2026999999") != std::string::npos) sawStudentIdInAttr.fetch_add(1);
                return reply(200, "text/html",
                             "<table id='AttrStudent'><tr><td>Comunicación</td><td>Oral y escrita</td><td>EL2113</td>"
                             "<td>80%</td><td>10%</td><td>30%</td><td>40%</td><td>80%</td></tr></table>");
            }
            if (target == "/tda-expediente-estudiantil/ajax/tabla_horario" && method == "POST") {
                if (body.find("carne=2026999999") != std::string::npos &&
                    body.find("periodo=2026_S_2") != std::string::npos) {
                    sawCarneInHorario.fetch_add(1);
                }
                return reply(200, "text/html",
                             "<table><tr><td>07:30-09:20</td><td>EL2113 Aula: B3-04</td><td></td><td></td><td></td><td></td><td></td><td></td></tr></table>");
            }
            if (target == "/dotlrn/classes/E/EL2113/S1/ajax/load_members" && method == "POST") {
                if (body.find("community_id=CID1") != std::string::npos &&
                    body.find("orderCol=2") != std::string::npos) {
                    sawMembersPost.fetch_add(1);
                }
                return reply(200, "application/json",
                             R"({"draw":1,"recordsTotal":1,"recordsFiltered":1,"data":[{"checkcCol":"u1","firstName":"Ana","lastName":"Ejemplo","emailCol":"ana@example.invalid","rolCol":"Estudiante"}]})");
            }
            if (target == "/tda-inclusion-manager/api/user-transaction") {
                return reply(200, "application/json", R"({"transaction_key":")" + synthJwt + R"("})");
            }
            if (target == "/graphql" && method == "POST") {
                gqlCalls.fetch_add(1);
                if (headers_low.find("token: " + AsciiLower(synthJwt)) == std::string::npos) {
                    gqlRejectedMissingToken.fetch_add(1);
                    return reply(401, "application/json", R"({"errors":[{"message":"Token Required"}]})");
                }
                if (body.find("getUserType") != std::string::npos) {
                    return reply(200, "application/json", R"({"data":{"getUserType":{"user_role":"student"}}})");
                }
                if (body.find("GetStudentActiveCampaign") != std::string::npos) {
                    return reply(200, "application/json",
                                 R"({"data":{"GetStudentActiveCampaign":{"id":"CAMP-2026-2","name":"Inclusiones II 2026","period":"2026_S_2","start_date":"2026-07-20","end_date":"2026-07-24","student_entity":"Ingeniería Electrónica","active":true}}})");
                }
                if (body.find("getStudentPendingGraduatedCourses") != std::string::npos) {
                    return reply(200, "application/json",
                                 R"({"data":{"getStudentPendingGraduatedCourses":[{"id":"EL3212","code":"EL3212","name":"Circuitos Digitales","credits":4}]}})");
                }
                if (body.find("getStudentRequisitsCorrequisitsByCourse") != std::string::npos) {
                    return reply(200, "application/json",
                                 R"({"data":{"getStudentRequisitsCorrequisitsByCourse":{"allRequirementsMet":true,"allCoRequirementsMet":true,"hasOverlap":false,"missingRequirements":[],"missingCoRequirements":[],"message":"Elegible"}}})");
                }
                if (body.find("GetAllStudentApplications") != std::string::npos) {
                    return reply(200, "application/json",
                                 R"({"data":{"GetAllStudentApplications":[{"id":"APP-1","course_code":"EL3212","course_name":"Circuitos Digitales","group":"01","status":"En revisión","comment":"Cupo","updated_at":"2026-07-21"}]}})");
                }
                if (body.find("getScholarship") != std::string::npos) {
                    return reply(200, "application/json",
                                 R"({"data":{"getScholarship":{"campaign_id":"CAMP-2026-2","status":"Activa","scholarship_type":"Mauricio Campos","detail":"100%"}}})");
                }
                return reply(200, "application/json", R"({"data":{}})");
            }
            if (target == "/dotlrn/file-storage/api/file/5501/versions") {
                return reply(200, "application/json", R"([{"title":"Plan.pdf","live_revision":"12","content_size":4096}])");
            }
            if (target == "/tda-performance-evaluation/api/forms/") {
                return reply(200, "application/json", R"([{"id":1,"name":"Form Docente","instructions":"Leer","sections":[]}])");
            }
            if (target == "/tda-performance-evaluation/api/evaluations/") {
                return reply(200, "application/json", R"([{"id":"E1","course_name":"Circuitos I","teacher_name":"Prof. A","status":"Pendiente"}])");
            }
            return reply(404, "text/plain", "not found");
        });
        CHECK(srv.Start());

        ServiceConfig cfg;
        cfg.base_url = srv.BaseUrl();
        cfg.graphql_url = srv.BaseUrl() + "/graphql";
        cfg.policy_options.allowed_test_origins = {srv.BaseUrl()};
        TecDigitalService svc(cfg);

        // Carne resolution & injection
        CHECK(svc.TdaClient().ResolveStudentCarne() == "2026999999");
        auto gaRes = svc.TdaClient().GetGraduateAttributes();
        CHECK(gaRes.ok && gaRes.value.size() == 1 && sawStudentIdInAttr.load() == 1);

        auto schRes = svc.TdaClient().GetCurrentPeriodSchedule("S-2-2026");
        CHECK(schRes.ok && schRes.value.size() == 1 && sawCarneInHorario.load() == 1);

        auto memRes = svc.TdaClient().GetCommunityMembers("/dotlrn/classes/E/EL2113/S1", "CID1", 100);
        CHECK(memRes.ok && memRes.value.size() == 1 && sawMembersPost.load() == 1);

        auto perfRes = svc.TdaClient().GetPerformanceEvaluationStatus();
        CHECK(perfRes.ok && perfRes.value.forms.size() == 1 && perfRes.value.pending_evaluations.size() == 1);

        FileStorageWalker walker(svc.GuardedTransport(), svc.Auth(), srv.BaseUrl());
        auto verRes = walker.FetchFileVersions("5501");
        CHECK(verRes.ok && verRes.value.size() == 1 && verRes.value[0].live_revision == "12");

        // GraphQL client end-to-end with token header enforcement
        auto roleRes = svc.GraphQlClient().FetchUserType();
        CHECK(roleRes.ok && roleRes.value == "student");
        CHECK(svc.GraphQlClient().CachedUserId() == "9001");

        auto campRes = svc.GraphQlClient().FetchStudentActiveCampaign();
        CHECK(campRes.ok && campRes.value.id == "CAMP-2026-2" && campRes.value.active);

        auto pendRes = svc.GraphQlClient().FetchPendingGraduationCourses("9001", "2026999999");
        CHECK(pendRes.ok && pendRes.value.size() == 1 && pendRes.value[0].code == "EL3212");

        auto reqRes = svc.GraphQlClient().CheckCourseRequisites("9001", "2026999999", "CAMP-2026-2", "EL3212", "EL3212", "2026_S_2");
        CHECK(reqRes.ok && reqRes.value.all_requirements_met && !reqRes.value.has_overlap);

        auto appRes = svc.GraphQlClient().FetchStudentApplications("CAMP-2026-2", "2026999999");
        CHECK(appRes.ok && appRes.value.size() == 1 && appRes.value[0].course_code == "EL3212");

        auto becaRes = svc.GraphQlClient().FetchScholarshipStatus("CAMP-2026-2");
        CHECK(becaRes.ok && becaRes.value.status == "Activa");

        CHECK(gqlCalls.load() >= 6);
        CHECK(gqlRejectedMissingToken.load() == 0);

        svc.GraphQlClient().ClearSessionToken();
        CHECK(svc.GraphQlClient().CachedUserId().empty());
        svc.TdaClient().ClearCachedCarne();
        srv.Stop();
    }
}

void TestPublicGuiaHorarios() {
    // 1. Escuelas & Modalidades ASP.NET {"d": "..."} parsers
    const std::string escRaw = R"json({"d":"[{\"IDE_DEPTO\":\"CA\",\"DSC_DEPTO\":\"ESCUELA DE INGENIERIA EN COMPUTACION\"},{\"IDE_DEPTO\":\"MA\",\"DSC_DEPTO\":\"ESCUELA DE MATEMATICA\"}]"})json";
    auto escRes = ParsePublicGuiaEscuelasResponse(escRaw);
    CHECK(escRes.ok && escRes.value.size() == 2 && escRes.value[0].ide_depto == "CA");

    const std::string modRaw = R"json({"d":"[{\"IDE_MODALIDAD\":\"S\",\"NOMBRE\":\"SEMESTRE\",\"CANT_PERIODOS\":2},{\"IDE_MODALIDAD\":\"V\",\"NOMBRE\":\"VERANO\",\"CANT_PERIODOS\":1}]"})json";
    auto modRes = ParsePublicGuiaModalidadesResponse(modRaw);
    CHECK(modRes.ok && modRes.value.size() == 2 && modRes.value[0].cant_periodos == 2);

    // 2. Sentinels ("NO DATOS", "ESTUDIANTE SIN HORARIOS", "false")
    auto empty1 = ParsePublicGuiaRowsResponse(R"({"d":"NO DATOS"})");
    CHECK(empty1.ok && empty1.value.empty());
    auto empty2 = ParsePublicGuiaRowsResponse(R"({"d":"ESTUDIANTE SIN HORARIOS"})");
    CHECK(empty2.ok && empty2.value.empty());
    auto errFalse = ParsePublicGuiaRowsResponse(R"({"d":"false"})");
    CHECK(!errFalse.ok && errFalse.error_code == "server_false");

    // 3. Multi-day, multi-campus, multi-period, summer & intra-group overlap rows
    const std::string innerRows = R"json([
        {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"IC1802","DSC_MATERIA":"Introducción a la programación","IDE_GRUPO":1,"DSC_DEPTO":"CA","CAN_CREDITOS":3,"HORAS":4,"NOM_DIA":"LUNES","IDE_MODALIDAD":"S","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"07:30","HFIN":"09:20","NOM_PROFESOR":"Prof. Cartago A","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""},
        {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"IC1802","DSC_MATERIA":"Introducción a la programación","IDE_GRUPO":1,"DSC_DEPTO":"CA","CAN_CREDITOS":3,"HORAS":4,"NOM_DIA":"JUEVES","IDE_MODALIDAD":"S","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"07:30","HFIN":"09:20","NOM_PROFESOR":"Prof. Cartago A","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""},
        {"DSC_SEDE":"CAMPUS TECNOLOGICO CENTRAL CARTAGO","IDE_MATERIA":"IC1802","DSC_MATERIA":"Introducción a la programación","IDE_GRUPO":1,"DSC_DEPTO":"CA","CAN_CREDITOS":3,"HORAS":4,"NOM_DIA":"JUEVES","IDE_MODALIDAD":"S","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Regular","HINICIO":"08:00","HFIN":"09:00","NOM_PROFESOR":"Prof. Cartago A","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""},
        {"DSC_SEDE":"CAMPUS TECNOLOGICO LOCAL SAN JOSE","IDE_MATERIA":"IC1802","DSC_MATERIA":"Introducción a la programación","IDE_GRUPO":1,"DSC_DEPTO":"CA","CAN_CREDITOS":3,"HORAS":4,"NOM_DIA":"MARTES","IDE_MODALIDAD":"S","IDE_PER_MOD":2,"NUM_ANO":2026,"DSC_MODALIDAD":"SEMESTRE","TIPO_CURSO":"Virtual","HINICIO":"18:00","HFIN":"20:50","NOM_PROFESOR":"Prof. SJ B","RESERVA_SEDE":"SJ","RESERVA_DEPTO":"","RESERVA_PLAN":""},
        {"DSC_SEDE":"CAMPUS TECNOLOGICO LOCAL SAN JOSE","IDE_MATERIA":"FH0025","DSC_MATERIA":"Sexualidad humana","IDE_GRUPO":40,"DSC_DEPTO":"FH","CAN_CREDITOS":0,"HORAS":2,"NOM_DIA":"MIERCOLES","IDE_MODALIDAD":"V","IDE_PER_MOD":1,"NUM_ANO":2026,"DSC_MODALIDAD":"VERANO","TIPO_CURSO":"Virtual","HINICIO":"18:20","HFIN":"21:55","NOM_PROFESOR":"Prof. Verano","RESERVA_SEDE":"","RESERVA_DEPTO":"","RESERVA_PLAN":""}
    ])json";
    const nlohmann::json wrapped = {{"d", innerRows}};
    auto rowsRes = ParsePublicGuiaRowsResponse(wrapped.dump());
    CHECK(rowsRes.ok && rowsRes.value.size() == 5);

    // Filter tests: Sede accent-insensitive ("San José" -> "SAN JOSE"), Período ("S-1"), exclude_reserved
    PublicGuiaFilterOptions f1;
    f1.sede_filter = "San José";
    CHECK(FilterPublicGuiaRows(rowsRes.value, f1).size() == 2);

    PublicGuiaFilterOptions f2;
    f2.periodo_filter = "S-1-2026";
    auto filteredS1 = FilterPublicGuiaRows(rowsRes.value, f2);
    CHECK(filteredS1.size() == 3);

    PublicGuiaFilterOptions f3;
    f3.exclude_reserved = true;
    CHECK(FilterPublicGuiaRows(rowsRes.value, f3).size() == 4);

    // Conversion to HorariosTEC JSON: cross-campus + cross-period disambiguation & overlap pruning
    const std::string catJsonAll = PublicGuiaRowsToHorariosTecJson(rowsRes.value, {});
    auto cj = nlohmann::json::parse(catJsonAll);
    CHECK(cj["courses"].size() == 2);
    CHECK(cj["courses"][0]["code"] == "IC1802");
    CHECK(cj["courses"][0]["groups"].size() == 2);
    // Group "1" exists in CA S-1 and SJ S-2 -> disambiguated IDs & 2 sessions on CA (overlapping Jueves 08:00-09:00 dropped!)
    CHECK(cj["courses"][0]["groups"][0]["id"] != cj["courses"][0]["groups"][1]["id"]);
    CHECK(cj["courses"][0]["groups"][0]["sessions"].size() == 2);
    CHECK(!cj["courses"][0]["groups"][0].contains("seatsAvailable"));

    auto ghRows = PublicGuiaRowsToGuiaHorarioRows(rowsRes.value, f2);
    CHECK(ghRows.size() == 3 && ghRows[0].cupo == -1);

    // 4. Mock IHttpTransport test asserting anonymous JSON POSTs (zero Cookie header)
    struct MockPublicTransport final : public IHttpTransport {
        std::vector<HttpRequest> seen;
        HttpResponse Execute(const HttpRequest& req) override {
            seen.push_back(req);
            HttpResponse r;
            r.transport_ok = true;
            r.status_code = 200;
            std::string bodyStr = R"({"d":"[]"})";
            if (req.url.find("cargaEscuelas") != std::string::npos) {
                bodyStr = R"({"d":"[{\"IDE_DEPTO\":\"CA\",\"DSC_DEPTO\":\"Computación\"}]"})";
            } else if (req.url.find("cargaModalidadPeriodos") != std::string::npos) {
                bodyStr = R"({"d":"[{\"IDE_MODALIDAD\":\"S\",\"NOMBRE\":\"SEMESTRE\",\"CANT_PERIODOS\":2}]"})";
            }
            r.body.assign(bodyStr.begin(), bodyStr.end());
            return r;
        }
    };
    MockPublicTransport mockTr;
    PublicGuiaHorariosClient cli(mockTr, "https://tec-appsext.itcr.ac.cr/guiahorarios");
    auto ce = cli.FetchEscuelas();
    auto cm = cli.FetchModalidades();
    auto cy = cli.FetchEscuelaByYear("CA", 2026);
    auto cv = cli.FetchVeranoByYear(2026);
    auto cc = cli.FetchEstudianteByCarnet("2026999999", 0);
    CHECK(ce.ok && ce.value.size() == 1);
    CHECK(cm.ok && cm.value.size() == 1);
    CHECK(cy.ok && cv.ok && cc.ok);
    CHECK(mockTr.seen.size() == 5);
    for (const auto& req : mockTr.seen) {
        CHECK(req.method == "POST");
        CHECK(req.headers.count("Cookie") == 0 && req.headers.count("cookie") == 0);
        CHECK(req.headers.at("Content-Type").find("application/json") != std::string::npos);
    }
}

void TestLive2026ParsersAndContracts() {
    // 1. Live TEC Evaluation grade scale (earned/weight in .vt_grade_student + Nota obtenida / 100)
    {
        const std::string evalHtml = R"html(
            <div class='sub_total' category_id='c1' item_weight='30'>Exámenes 11.4 / 30</div>
            <div class='container_accor' assign_id='a101' task_id='t10' task_item_id='ti10'>
              <div class='head_subaccord'>
                <span class='assignNameText'>Examen Parcial 1</span>
                <span class='text_right vt_grade_student' title='11.4 / 15'> 11.4 / 15</span>
              </div>
              <div class='content_delivery'>
                <p class='title_subsection'>Descripción :</p><div class='body_style'>Evaluación sobre nodos y mallas</div>
                <p class='title_subsection'>Valor de la Asignación:</p><div class='body_style'>15</div>
                <p class='title_subsection'>Nota obtenida:</p><div class='body_style'>76.0 / 100</div>
                <p class='title_subsection'>Comentarios:</p><div class='body_style'>Buen desarrollo en mallas</div>
                <p class='title_subsection'>Rúbrica:</p>
                <input type='radio' disabled><label>Sí</label>
                <input type='radio' disabled checked><label>No</label>
                <p>Enunciado: <a href='/dotlrn/classes/E/EL2113/S-2-2026/evaluation/view/p1.pdf'>p1.pdf</a></p>
                <p>Solución: <a href='/dotlrn/classes/E/EL2113/S-2-2026/evaluation/view/sol1.pdf'>sol1.pdf</a></p>
              </div>
            </div>
            <div class='container_accor' assign_id='a102'>
              <div class='head_subaccord'>
                <span class=assignNameText>Examen Parcial 2</span>
                <span class='text_right vt_grade_student' title='12.75 / 15'> 12.75 / 15</span>
              </div>
              <div class='content_delivery'>
                <p class='title_subsection'>Valor de la Asignación:</p><div class='body_style'>15</div>
                <button class='seeRubric' data-rubric_id='555' data-course_id='888' data-student_id='777' data-task_item_id='666' data-task_id='444'>Ver</button>
              </div>
            </div>
            <div class='container_accor' assign_id='a103'>
              <div class='head_subaccord'>
                <span class='assignNameText'>Examen Parcial 3</span>
                <span class='text_right vt_grade_student' title='-- / 20'> -- / 20</span>
              </div>
              <div class='content_delivery'>
                <p class='title_subsection'>Valor de la Asignación:</p><div class='body_style'>20</div>
              </div>
            </div>
        )html";
        auto rep = ParseDetailedEvaluationsHtml(evalHtml, "el2113");
        CHECK(rep.categories.size() == 1);
        CHECK(rep.categories[0].items.size() == 3);
        const auto& i0 = rep.categories[0].items[0];
        CHECK(i0.is_graded);
        CHECK(std::abs(i0.earned_pct - 11.4) < 0.01);
        CHECK(std::abs(i0.weight_pct - 15.0) < 0.01);
        CHECK(std::abs(i0.raw_grade_100 - 76.0) < 0.01);
        CHECK(std::abs(ComputeEvaluationGrade100(i0) - 76.0) < 0.01);
        CHECK(!i0.has_rubric);
        CHECK(i0.description == "Evaluación sobre nodos y mallas");
        CHECK(i0.teacher_comment == "Buen desarrollo en mallas");
        CHECK(i0.attachment_url.find("p1.pdf") != std::string::npos);
        CHECK(i0.solution_url.find("sol1.pdf") != std::string::npos);

        // Item 2 has unquoted class=assignNameText and NO "Nota obtenida" -> derive 85.0/100 from 12.75 / 15
        const auto& i1 = rep.categories[0].items[1];
        CHECK(i1.name == "Examen Parcial 2");
        CHECK(i1.is_graded);
        CHECK(std::abs(i1.earned_pct - 12.75) < 0.01);
        CHECK(std::abs(i1.weight_pct - 15.0) < 0.01);
        CHECK(std::abs(ComputeEvaluationGrade100(i1) - 85.0) < 0.05);
        CHECK(i1.has_rubric && i1.rubric_id == "555" && i1.community_id == "888" && i1.student_id == "777");

        // Item 3 is ungraded (-- / 20)
        const auto& i2 = rep.categories[0].items[2];
        CHECK(!i2.is_graded && ComputeEvaluationGrade100(i2) < 0.0 && std::abs(i2.weight_pct - 20.0) < 0.01);

        // Accumulated course grade base 100 = (11.4 + 12.75) / (15 + 15) * 100 = 80.5
        CHECK(std::abs(ComputeCourseAccumulatedGrade100(rep) - 80.5) < 0.05);

        // 1a-bis. Live TEC enclosing .asign_student category header + numeric-only .sub_total
        const std::string liveCatHtml = R"html(
            <div id="h_245716934" class="asign_student title_acor_grade iconOpenGA row">
              <i class="material-icons">add_circle_outline</i>
              <span>Proyecto de Física</span>
              <div class="title_right sub_total hasGradeClass" item_weight="15">6.562</div>
            </div>
            <div class="container_accor" assign_id="a201">
              <div class="head_subaccord"><span class=assignNameText>II Fase</span><span class="vt_grade_student">3.25 / 5</span></div>
              <div class="content_delivery"><p class="title_subsection">Valor de la Asignación:</p><div class="body_style">5</div></div>
            </div>
        )html";
        auto liveRep = ParseDetailedEvaluationsHtml(liveCatHtml, "fi2103");
        CHECK(liveRep.categories.size() == 1);
        CHECK(liveRep.categories[0].name == "Proyecto de Física");
        CHECK(liveRep.categories[0].category_id == "245716934");
        CHECK(std::abs(liveRep.categories[0].weight_pct - 15.0) < 0.01);
        CHECK(std::abs(liveRep.categories[0].earned_pct - 6.562) < 0.01);
        CHECK(liveRep.categories[0].items.size() == 1 && liveRep.categories[0].items[0].name == "II Fase");

        // 1b. Live TEC analytic rubric table (#table_body_evaluation, Peso %, criterion_selected, tr[comment], calculate_score)
        const std::string rubHtml = R"html(
            <div id='container_table'>
              <table id='table_body_evaluation' levels_org='4'>
                <thead>
                  <tr class='header'>
                    <th id='th_name'>
                      <div class='right_note'>Evaluar Grupo: Grupo 5</div>
                      <div class='right_note'>Puntuación: <label id='lbl_score'>0</label></div>
                      <div class='name_rubric'>2S-2026- Fase II Proyecto</div>
                    </th>
                    <th level_col='0' id='level0' wlevel='3'>Excelente<div><span id='weight0'>3</span> Puntos</div></th>
                    <th level_col='1' id='level1' wlevel='2'>Satisfactorio<div><span id='weight1'>2</span> Puntos</div></th>
                    <th level_col='2' id='level2' wlevel='1'>En desarrollo<div><span id='weight2'>1</span> Puntos</div></th>
                    <th level_col='3' id='level3' wlevel='0'>Insuficiente<div><span id='weight3'>0</span> Puntos</div></th>
                  </tr>
                </thead>
                <tr class='category' row='0' category_id='10' comment='Faltó aclarar condiciones del modelo.'>
                  <td><div id='div_1_0'>Fundamento físico</div><div><span>Peso <span id='category_weight0'>95</span>%</span></div></td>
                  <td class='criterion_padding' row='0' level='0'>Completo</td>
                  <td class='criterion_padding criterion_selected' row='0' level='1'>Correcto con pequeñas omisiones<span class='tooltip_box'>Faltó aclarar condiciones del modelo.</span></td>
                  <td class='criterion_padding' row='0' level='2'>Parcial</td>
                  <td class='criterion_padding' row='0' level='3'>Sin fundamento</td>
                </tr>
                <tr class='category' row='1' category_id='11' comment='Incluir bitácora completa.'>
                  <td><div id='div_1_1'>Presentación</div><div><span>Peso <span id='category_weight1'>5</span>%</span></div></td>
                  <td class='criterion_padding' row='1' level='0'>Excelente</td>
                  <td class='criterion_padding' row='1' level='1'>Adecuado</td>
                  <td class='criterion_padding criterion_selected' row='1' level='2'>Incompleto</td>
                  <td class='criterion_padding' row='1' level='3'>Deficiente</td>
                </tr>
              </table>
            </div>
            <div id='div_info_rubric' max_weight='3'></div>
        )html";
        auto rb = ParseRubricHtml(rubHtml, "23678");
        CHECK(rb.title == "2S-2026- Fase II Proyecto");
        CHECK(rb.evaluated_target == "Grupo 5");
        CHECK(rb.is_graded);
        CHECK(std::abs(rb.max_level_points - 3.0) < 0.01);
        CHECK(std::abs(rb.max_score - 100.0) < 0.01);
        // 95 * (2/3) + 5 * (1/3) = 63.3333 + 1.6667 = 65.0
        CHECK(std::abs(rb.total_score - 65.0) < 0.05);
        CHECK(rb.criteria.size() == 2);
        CHECK(rb.criteria[0].category_name == "Fundamento físico");
        CHECK(std::abs(rb.criteria[0].weight_pct - 95.0) < 0.01);
        CHECK(rb.criteria[0].comment.find("condiciones del modelo") != std::string::npos);
        CHECK(rb.criteria[0].levels.size() == 4 && rb.criteria[0].levels[1].achieved && !rb.criteria[0].levels[0].achieved);
        CHECK(rb.criteria[0].levels[1].description == "Correcto con pequeñas omisiones");
        CHECK(rb.criteria[1].levels[2].achieved && rb.criteria[1].comment.find("bitácora") != std::string::npos);
    }

    // 2. Live 9-column detalle_notas HTML (2 empty spacer columns)
    {
        const std::string notas9 = R"html(
            <table id='tRegistro'>
              <thead><tr><th>Año</th><th>Estado</th><th>Nota</th><th></th><th>Materia</th><th></th><th>Modalidad</th><th>Periodo</th><th>Créditos</th></tr></thead>
              <tbody>
                <tr><td>2026</td><td>Aprobada</td><td>88</td><td></td><td>EL2113 - Circuitos CC</td><td></td><td>Semestre</td><td>1</td><td>4</td></tr>
                <tr><td>2026</td><td>En curso</td><td>--</td><td></td><td>FI1102 - Física II</td><td></td><td>Semestre</td><td>2</td><td>4</td></tr>
              </tbody>
            </table>
        )html";
        auto grades = ParseDetalleNotasHtml(notas9);
        CHECK(grades.size() == 2);
        CHECK(grades[0].year == 2026 && grades[0].state == "Aprobada" && std::abs(grades[0].grade - 88.0) < 0.01);
        CHECK(grades[0].course_code == "EL2113" && grades[0].course_name == "Circuitos CC" && grades[0].credits == 4);
        CHECK(grades[1].course_code == "FI1102" && grades[1].period == "2");
    }

    // 3. Live detalle_historial HTML (#nota BEFORE label + #hCursadas + #porcentaje_aprobado)
    {
        const std::string histHtml = R"html(
            <div id='general'>
              <span id='nota' class='tds-lib_text_detail'>83.45</span>
              <span class='tds-lib_text_marker'>Promedio ponderado general</span>
              <span id='hCursadas'>114 Horas cursadas</span>
              <span id='porcentaje_aprobado'>91.5%</span>
              <span id='porcentaje_reprobado'>8.5%</span>
            </div>
            <table>
              <tr><td>Cursos</td><td>Aprobados</td><td>22</td></tr>
              <tr><td>Cursos</td><td>Matriculados</td><td>3</td></tr>
              <tr><td>Cursos</td><td>Retirados</td><td>2</td></tr>
            </table>
        )html";
        auto st = ParseAcademicStatsHtml(histHtml);
        CHECK(std::abs(st.weighted_avg_general - 83.45) < 0.01);
        CHECK(st.hours_taken == 114);
        CHECK(std::abs(st.passed_pct - 91.5) < 0.05);
        CHECK(std::abs(st.failed_pct - 8.5) < 0.05);
        CHECK(st.count_aprobadas == 22 && st.count_en_curso == 3 && st.count_retiradas == 2);
    }

    // 4. Live malformed <tr>-less citas_matricula HTML
    {
        const std::string citasMalformed =
            "<table id='tcitas_Matricula'><thead><th>Fecha</th><th>Periodo</th><th>Modalidad</th><th>Hora</th></thead>"
            "<tbody><td>2026-07-15</td><td>2</td><td>Semestral Ordinaria</td><td>08:15:00</td></tr>"
            "<td>2026-01-20</td><td>1</td><td>Semestral Ordinaria</td><td>14:00:00</td></tr></tbody></table>";
        auto citas = ParseCitasMatriculaHtml(citasMalformed);
        CHECK(citas.size() == 2);
        CHECK(citas[0].date == "2026-07-15" && citas[0].time == "08:15:00" && citas[0].modality == "Semestral Ordinaria");
        CHECK(citas[1].date == "2026-01-20" && citas[1].period == "1");
    }

    // 5. Live 4-part '-----'-delimited tabla_horario + space period converter
    {
        CHECK(ConvertPeriodKeyToTablaHorarioFormat("S-2-2026") == "S 2 2026");
        CHECK(ConvertPeriodKeyToTablaHorarioFormat("2026_S_2") == "S 2 2026");
        const std::string horarioPayload =
            "<table></table>-----16 10 17 55 03|09 45 11 30 05-----"
            "Circuitos*1*2*3*EL2113*PROF UNO*03-E|Fisica*1*2*3*FI1102*PROF DOS*02-A-----"
            "IDE_MATERIA*EL2113*DSC_MATERIA*Circuitos CC*CAN_CREDITOS*4*IDE_GRUPO*50*NOM_PROFESOR*RODRIGUEZ PABLO*"
            "COD_DIA*3*NOM_DIA*MIERCOLES*IDE_EDIFICIO*E*IDE_AULA*03*NUM_HORA_INICIO*16:10*NUM_HORA_FIN*17:55*celdas*16 10 17 55 03|"
            "IDE_MATERIA*FI1102*DSC_MATERIA*Física II*CAN_CREDITOS*4*IDE_GRUPO*50*NOM_PROFESOR*ALVARADO ROMMEL*"
            "COD_DIA*5*NOM_DIA*VIERNES*IDE_EDIFICIO*A*IDE_AULA*02*NUM_HORA_INICIO*09:45*NUM_HORA_FIN*11:30*celdas*09 45 11 30 05";
        auto cells = ParseTablaHorarioHtml(horarioPayload);
        CHECK(cells.size() == 2);
        CHECK(cells[0].course_code == "EL2113" && cells[0].day_index_0_mon == 2 && cells[0].room == "E-03");
        CHECK(cells[0].professor == "RODRIGUEZ PABLO" && cells[0].start_minute == 16 * 60 + 10 && cells[0].end_minute == 17 * 60 + 55);
        CHECK(cells[1].course_code == "FI1102" && cells[1].day_index_0_mon == 4 && cells[1].room == "A-02");
    }

    // 6. User Model HTML, Personal Academic Info HTML, DAR Preview HTML, Feria Table HTML
    {
        auto dim = ParseUserModelSectionHtml(
            "#learning_style_info", "Estilo de aprendizaje",
            "<section class='test_description'><h1>Felder</h1><p>Basado en el test de Felder.</p>"
            "<table><tr><td>Visual / Verbal</td><td>Visual (7)</td></tr></table></section>");
        CHECK(dim.dimension_key == "#learning_style_info" && !dim.metrics.empty());

        auto pi = ParsePersonalAcademicInfoHtml(
            "<div id='InformacionAcademica_lib'><h2 class='labelInf'>Carrera</h2><span class='detailInf'>Ingeniería Electrónica</span>"
            "<h2 class='labelInf'>Sede</h2><span class='detailInf'>Cartago</span></div>");
        CHECK(pi.career == "Ingeniería Electrónica" && pi.campus == "Cartago");

        auto dar = ParseDarActualizarCursosHtml(
            "<div class=course estado=1><div class=courseName>Circuitos CC</div><div>Miembro</div></div>"
            "<div class=course estado=1><div class=courseName>Física general Ii</div><div>Miembro</div></div>"
            "<div class=course estado=1><div class=courseName>Física general Iii</div><div>Miembro</div></div>");
        CHECK(dar.size() == 3 && dar[0].course_name == "Circuitos CC" && dar[0].status_text == "Miembro");
        CHECK(dar[1].course_name == "Física general II" && dar[2].course_name == "Física general III");

        auto ferias = ParseFeriaTableHtml(
            "<table id=tferias><tr><td>Feria 2026</td><td>01/08/2026</td><td>30/09/2026</td><td><a href='/bases.pdf'>Ver</a></td></tr></table>");
        CHECK(ferias.size() == 1 && ferias[0].name == "Feria 2026" && ferias[0].bases_url.find("bases.pdf") != std::string::npos);
    }

    // 7. Policy guards: read-only preview allowed, mutating DAR / actualizar_cursos2 blocked
    {
        ReadOnlyRequestPolicy pol;
        PolicyRequestContext pPreview;
        pPreview.url = "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/actualizar_cursos";
        pPreview.method = "POST";
        CHECK(pol.Evaluate(pPreview).allowed);

        PolicyRequestContext pNotas;
        pNotas.url = "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/detalle_notas";
        pNotas.method = "POST";
        CHECK(pol.Evaluate(pNotas).allowed);

        PolicyRequestContext pMut1;
        pMut1.url = "https://tecdigital.tec.ac.cr/dotlrn/ajax/actualizar_cursos";
        pMut1.method = "POST";
        CHECK(!pol.Evaluate(pMut1).allowed);

        PolicyRequestContext pMut2;
        pMut2.url = "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/actualizar_cursos2";
        pMut2.method = "POST";
        CHECK(!pol.Evaluate(pMut2).allowed);

        // Mutating GET discovered in live recursive crawl (op_swap reorders portal portlets) MUST be blocked
        PolicyRequestContext pOpSwap;
        pOpSwap.url = "https://tecdigital.tec.ac.cr/dotlrn/region=1&page_id=10&direction=down&action=op_swap";
        pOpSwap.method = "GET";
        CHECK(!pol.Evaluate(pOpSwap).allowed);

        // Read-only plagiarism docs & server clock endpoints discovered in JS bundles MUST be allowed
        PolicyRequestContext pPlag;
        pPlag.url = "https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/cargaDocumentos/cargaDocsCurso?community_id=12345";
        pPlag.method = "GET";
        CHECK(pol.Evaluate(pPlag).allowed);

        PolicyRequestContext pClock;
        pClock.url = "https://tecdigital.tec.ac.cr/theme-td/ajax/get_date_server";
        pClock.method = "GET";
        CHECK(pol.Evaluate(pClock).allowed);
        // 8. Calendar parser ignores #at-a-glance mini-calendar ("Lun"/"Mié"/"28") and parses real list-table Spanish dates
        const std::string liveCalHtml = R"html(
            <table id="at-a-glance">
              <thead><tr><th>Lun</th><th>Mar</th><th>Mié</th><th>Jue</th><th>Vie</th><th>Sáb</th><th>Dom</th></tr></thead>
              <tbody><tr><td>26</td><td>27</td><td>28</td><td>29</td><td>30</td><td>1</td><td>2</td></tr></tbody>
            </table>
            <table class="list-table" summary="Datos para items-list">
              <thead><tr><th>Fecha</th><th>Hora</th><th>Evento</th><th>Calendario</th><th>Descripción</th></tr></thead>
              <tbody>
                <tr><td>25 noviembre 2026</td><td>08:30 - 11:00</td><td>Tercer Examen Parcial</td><td>Cátedra de Física General II</td><td>Aula Cartago</td></tr>
                <tr><td></td><td>14:00</td><td>Entrega Proyecto</td><td>FI2103</td><td>Fase final</td></tr>
                <tr><td>Sin eventos</td><td></td><td>No hay elementos</td><td></td><td></td></tr>
              </tbody>
            </table>
        )html";
        auto calEvs = ParseCalendarListHtml(liveCalHtml);
        CHECK(calEvs.size() == 2);
        CHECK(calEvs[0].date_iso == "2026-11-25" && calEvs[0].start_time == "08:30" && calEvs[0].end_time == "11:00");
        CHECK(calEvs[0].title == "Tercer Examen Parcial" && calEvs[0].calendar_name == "Cátedra de Física General II");
        CHECK(calEvs[1].date_iso == "2026-11-25" && calEvs[1].title == "Entrega Proyecto");

        // 9. Course overview strips timeout-overlay chrome & ignores in-portlet "ver más" ?page_num= links
        const std::string ovHtml = R"html(
            <div id="timeout-overlay">Límite de tiempo de sesión close Por políticas de seguridad</div>
            <section id="sub-navigation"><ul id="selectable_subnavbar">
              <li><a href="?page_num=0">Portada</a></li>
              <li><a href="?page_num=1">Contenidos</a></li>
              <li><a href="?page_num=4">Cronograma</a></li>
              <li><a href="?page_num=6">Documentos</a></li>
              <li><a href="?page_num=7">Evaluaciones</a></li>
            </ul></section>
            <div id="main-content">
              <div class="portlet-wrap"><div class="portlet-title">Semana 1 y 2</div><img alt="Módulo de cinemática y oscilaciones" src="/img/s1.png"/></div>
              <a href="?page_num=10">ver más</a>
              <a href="?page_num=11">ver más</a>
            </div>
        )html";
        auto ov = ParseCourseOverviewHtml(ovHtml, "/dotlrn/classes/ECE/FI2103/S-2-2026.SC.FI2103.51", "https://tecdigital.tec.ac.cr");
        CHECK(ov.tabs.size() == 5);
        int customCnt = 0;
        for (const auto& tb : ov.tabs) {
            if (tb.label == "Documentos" || tb.label == "Evaluaciones") CHECK(!tb.is_custom);
            if (tb.is_custom) ++customCnt;
        }
        CHECK(customCnt == 2);  // Only Contenidos & Cronograma
        CHECK(ov.description.find("Límite de tiempo de sesión") == std::string::npos);
        CHECK(ov.portlets.size() == 1 && ov.portlets[0].title == "Semana 1 y 2");
        CHECK(ov.portlets[0].body_preview.find("oscilaciones") != std::string::npos);

        // Dedup "Fecha de entrega para PIM" vs "Entrega: PIM" on same date
        std::vector<CalendarEventEntry> portEvs;
        CalendarEventEntry ce;
        ce.date_iso = "2026-11-23";
        ce.start_time = "23:29";
        ce.title = "Fecha de entrega para PIM";
        ce.source_kind = "calendar";
        portEvs.push_back(ce);
        DetailedEvaluationsReport rep;
        rep.community_slug = "FI2103";
        decltype(rep.categories)::value_type cat;
        cat.name = "Proyectos";
        decltype(cat.items)::value_type it;
        it.assign_id = "101";
        it.name = "PIM";
        it.due_date_iso = "2026-11-23 23:59";
        it.weight_pct = 15.0;
        it.submission_status = "Calificada";
        cat.items.push_back(it);
        rep.categories.push_back(cat);
        auto unified = BuildUnifiedCalendarEvents(portEvs, { rep }, {});
        CHECK(unified.size() == 1);
        CHECK(unified[0].source_kind == "evaluation");
        CHECK(unified[0].title == "Entrega: PIM");

        // Short RSS news descriptions (<800 chars) must ALWAYS strip HTML tags
        const std::string rssXml = R"xml(<?xml version="1.0"?>
            <rss version="2.0"><channel><item>
                <title>Aviso de Cátedra</title>
                <link>https://tecdigital.tec.ac.cr/news/item?item_id=1</link>
                <description>&lt;p&gt;Estimables estudiantes.&lt;br&gt;&lt;div id="TextareaHtml"&gt;Nueva guía publicada.&lt;/div&gt;&lt;/p&gt;</description>
                <pubDate>Tue, 24 Nov 2026 03:38:01 GMT</pubDate>
            </item></channel></rss>)xml";
        auto rssRes = TdaTdsClient::ParseRssXml(rssXml);
        CHECK(rssRes.ok && rssRes.value.size() == 1);
        CHECK(rssRes.value[0].description.find("<p>") == std::string::npos);
        CHECK(rssRes.value[0].description.find("<div") == std::string::npos);
        CHECK(rssRes.value[0].description.find("Nueva guía publicada") != std::string::npos);

        // Rubric parser strips trailing "Copiar comentario" UI button text
        const std::string rubHtml = R"html(
            <div id="div_info_rubric" max_weight="3"></div>
            <div class="name_rubric">Rúbrica Fase II</div>
            <table><thead><tr><th>Cat</th><th level_col="0" wlevel="3">Excelente</th></tr></thead>
            <tbody id="table_body_evaluation">
              <tr class="category" row="0" comment="Buen análisis del modelo físico. Copiar comentario">
                <td><div id="div_1_0">Fundamento</div><span id="category_weight0">100</span></td>
                <td level="0" class="criterion_selected">Explicación clara</td>
              </tr>
            </tbody></table>)html";
        auto rb = ParseRubricHtml(rubHtml, "99");
        CHECK(rb.criteria.size() == 1);
        CHECK(rb.criteria[0].comment == "Buen análisis del modelo físico.");
    }
}

void TestV3SuperPlanParsersAndPolicy() {
    // 1. ParseFolderJson extracts creation_date_ansi, last_modified_ansi, author_name, subscription_active on folders & files
    {
        const std::string fJson = R"json({
            "folder_id": "5001",
            "name": "Raíz",
            "elements": [
                {
                    "object_id": "5010",
                    "parent_id": "5001",
                    "fs_type": "folder",
                    "name": "Semana 1",
                    "creation_date_ansi": "2026-08-03 09:15:00",
                    "last_modified_ansi": "2026-08-05 14:22:00",
                    "author_name": "Prof. Física",
                    "subscribed_p": "t",
                    "content_size": 3
                },
                {
                    "object_id": "5020",
                    "parent_id": "5001",
                    "fs_type": "file",
                    "name": "guia_oscilaciones.pdf",
                    "title": "Guía Oscilaciones",
                    "mime_type": "application/pdf",
                    "content_size": 245760,
                    "live_revision": "3",
                    "creation_date_ansi": "2026-08-04 10:00:00",
                    "last_modified_ansi": "2026-08-19 16:45:00",
                    "author": "Prof. Física",
                    "url": "/dotlrn/file-storage/download/guia_oscilaciones.pdf?version_id=5020"
                }
            ]
        })json";
        auto parsed = FileStorageWalker::ParseFolderJson(fJson, "5001");
        CHECK(parsed.ok);
        CHECK(parsed.value.folders.size() == 2); // root 5001 + subfolder 5010
        if (parsed.value.folders.size() == 2) {
            const auto& sf = parsed.value.folders[1];
            CHECK(sf.creation_date_ansi == "2026-08-03 09:15:00");
            CHECK(sf.last_modified_ansi == "2026-08-05 14:22:00");
            CHECK(sf.author_name == "Prof. Física");
            CHECK(sf.subscription_active);
        }
        CHECK(parsed.value.files.size() == 1);
        if (parsed.value.files.size() == 1) {
            const auto& f = parsed.value.files[0];
            CHECK(f.creation_date_ansi == "2026-08-04 10:00:00");
            CHECK(f.last_modified_ansi == "2026-08-19 16:45:00");
            CHECK(f.live_revision == "3");
            CHECK(f.author_name == "Prof. Física");
            CHECK(f.mime_type == "application/pdf");
        }
    }

    // 2. ParseSpacePageHtml captures weekly linked_subpages (S1, S2), embedded infographics, module_kind & cleans "article"
    {
        const std::string wkHtml = R"html(
            <html><body>
            <div class="portlet-wrap">
                <div class="portlet-header"><h2>Semana 1 y 2 — Oscilaciones</h2></div>
                <div class="portlet-body">
                    <p>Contenidos semanales y guía gráfica.</p>
                    <a href="/dotlrn/clubs/ctedras/fisica3/?page_num=3">S1</a>
                    <a href="/dotlrn/clubs/ctedras/fisica3/?page_num=4">S2</a>
                    <a href="/dotlrn/clubs/ctedras/fisica3/?page_num=99">ver más</a>
                    <a href="/dotlrn/file-storage/download/guia_s1.pdf"><i class="material-icons">article</i> Guía Semana 1</a>
                    <a href="https://tecdigital.tec.ac.cr/dotlrn/clubs/ctedras/fisica3/infografia_s1.png">
                        <img src="/dotlrn/clubs/ctedras/fisica3/infografia_s1.png" alt="Infografía Semana 1"/>
                    </a>
                </div>
            </div>
            <div class="portlet-wrap">
                <div class="portlet-header"><h2>Repositorio de Exámenes</h2></div>
                <div class="portlet-body"><p>Parciales anteriores.</p></div>
            </div>
            </body></html>)html";
        auto pg = ParseSpacePageHtml(wkHtml, 1, "Contenidos",
                                     "https://tecdigital.tec.ac.cr/dotlrn/clubs/ctedras/fisica3/?page_num=1",
                                     "https://tecdigital.tec.ac.cr");
        CHECK(pg.portlets.size() == 2);
        if (pg.portlets.size() == 2) {
            const auto& p0 = pg.portlets[0];
            CHECK(p0.module_kind == "weekly");
            CHECK(p0.linked_subpages.size() == 2);
            CHECK(p0.linked_subpages[0].page_num == 3 && p0.linked_subpages[0].label == "S1");
            CHECK(p0.linked_subpages[1].page_num == 4 && p0.linked_subpages[1].label == "S2");
            CHECK(!p0.images.empty() && p0.images[0].image_url.find("infografia_s1.png") != std::string::npos);
            CHECK(!p0.links.empty());
            for (const auto& lk : p0.links) {
                CHECK(lk.label.find("article") == std::string::npos);
            }
            CHECK(pg.portlets[1].module_kind == "exam_notice");
        }
    }

    // 2b. ParseSpacePageHtml resolves real S1..S2 card headings behind Material-icon "ver más<i class='material-icons'>loupe</i>" buttons & strips play_arrow/go_back
    {
        const std::string catHtml = R"html(
            <html><body>
            <div class="portlet-wrap">
                <div class="portlet-header"><h2>Contenidos</h2></div>
                <div class="portlet-body">
                    <div class="card"><span>S1 Movimiento periódico</span><a href="/dotlrn/catedras.tec/fisica3/one-community?page_num=11">ver más<i class="material-icons">loupe</i></a></div>
                    <div class="card"><div style="margin:0;padding:12px;border:1px solid #ccc;background:#fff;width:100%;box-sizing:border-box;"><span style="color: #002856ff; font-size: 16pt;">S2 M.A.S y sus aplicaciones</span><p style="font-size:11pt;color:#444;">Contenido detallado del módulo semanal con recursos, ejercicios y videos.</p></div><a href="/dotlrn/catedras.tec/fisica3/one-community?page_num=12">ver más<i class="material-icons">loupe</i></a></div>
                    <a href="/dotlrn/catedras.tec/fisica3/one-community?page_num=0"><img src="/repositorio/go_back.png" alt="regresar al portal"/></a>
                    <a href="/dotlrn/catedras.tec/fisica3/one-community?page_num=25"><i class="material-icons">play_arrow</i>Exámenes de otros semestres</a>
                </div>
            </div>
            </body></html>)html";
        auto pg = ParseSpacePageHtml(catHtml, 1, "Contenidos",
                                     "https://tecdigital.tec.ac.cr/dotlrn/catedras.tec/fisica3/one-community?page_num=1",
                                     "https://tecdigital.tec.ac.cr");
        CHECK(pg.portlets.size() == 1);
        if (pg.portlets.size() == 1) {
            const auto& subs = pg.portlets[0].linked_subpages;
            CHECK(subs.size() == 3);
            if (subs.size() == 3) {
                CHECK(subs[0].page_num == 11 && subs[0].label == "S1 Movimiento periódico");
                CHECK(subs[1].page_num == 12 && subs[1].label.rfind("S2 M.A.S y sus aplicaciones", 0) == 0);
                CHECK(subs[1].label.find("color:") == std::string::npos && subs[1].label.find('>') == std::string::npos);
                CHECK(subs[2].page_num == 25 && subs[2].label == "Exámenes de otros semestres");
            }
            for (const auto& s : subs) {
                CHECK(s.label.find("loupe") == std::string::npos);
                CHECK(s.label.find("play_arrow") == std::string::npos);
                CHECK(s.page_num > 0);
            }
        }
        CHECK(pg.plain_excerpt.find("loupe") == std::string::npos);
        CHECK(pg.plain_excerpt.find("play_arrow") == std::string::npos);
    }

    // 3. Policy matrix: MUTATION_MASK=0 blocks ALL 10 mutation groups while allowing read-only GAAP/Motor-Juegos
    {
        PolicyOptions popts;
        ReadOnlyRequestPolicy pol(popts);

        // Read-only GAAP origin & tda-motor-juegos read-only POST allowed under MUTATION_MASK=0
        PolicyRequestContext pGaap;
        pGaap.url = "https://api.tec.ac.cr:3003/activities/list";
        pGaap.method = "GET";
        CHECK(pol.Evaluate(pGaap).allowed);

        PolicyRequestContext pMj;
        pMj.url = "https://tecdigital.tec.ac.cr/tda-motor-juegos/api/activities/";
        pMj.method = "POST";
        CHECK(pol.Evaluate(pMj).allowed);

        // All new student mutations MUST be blocked by default (MUTATION_MASK=0)
        const std::vector<std::pair<std::string, std::string>> mutReqs = {
            {"POST", "https://tecdigital.tec.ac.cr/tda-notifications/ajax/delete_all_user_notifications"},
            {"GET",  "https://tecdigital.tec.ac.cr/dotlrn/file-storage/api/notification/subscribe/5010"},
            {"POST", "https://tecdigital.tec.ac.cr/tda-expediente-estudiantil/ajax/actualizar_cursos2"},
            {"POST", "https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/fileUpload"},
            {"POST", "https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/addDocumentEphorus"},
            {"POST", "https://tecdigital.tec.ac.cr/tda-prematricula/ajax/guardar_prematricula"},
            {"POST", "https://tecdigital.tec.ac.cr/tda-feria-negocios/ajax/record_project"},
            {"POST", "https://tecdigital.tec.ac.cr/tda-um/ajax/save_privacy_settings"},
            {"POST", "https://tecdigital.tec.ac.cr/dotlrn/file-storage/api/folder-create"},
            {"DELETE", "https://tecdigital.tec.ac.cr/dotlrn/file-storage/api/file/5020"}
        };
        for (const auto& [m, u] : mutReqs) {
            PolicyRequestContext ctx;
            ctx.method = m;
            ctx.url = u;
            CHECK(!pol.Evaluate(ctx).allowed);
        }

        // GraphQL student inclusion mutation blocked by default, allowed ONLY with ManageInclusions bit
        PolicyRequestContext gqlMut;
        gqlMut.method = "POST";
        gqlMut.url = "https://api.tec.ac.cr:3005/graphql";
        gqlMut.post_data = R"({"query":"mutation SetInclusionRequest { setInclusionRequest(course:\"FI2103\") { ok } }"})";
        CHECK(!pol.Evaluate(gqlMut).allowed);
        {
            const uint32_t incMask = static_cast<uint32_t>(AllowedStudentMutation::ManageInclusions);
            gqlMut.allowed_mutations_mask = incMask;
            CHECK(pol.Evaluate(gqlMut).allowed);
            // Unrelated plagiarism upload MUST remain blocked under ManageInclusions
            PolicyRequestContext pPlagUp;
            pPlagUp.method = "POST";
            pPlagUp.url = "https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/fileUpload";
            pPlagUp.allowed_mutations_mask = incMask;
            CHECK(!pol.Evaluate(pPlagUp).allowed);
            // Admin GraphQL mutation MUST remain blocked even with ManageInclusions
            PolicyRequestContext gqlAdmin;
            gqlAdmin.method = "POST";
            gqlAdmin.url = "https://api.tec.ac.cr:3005/graphql";
            gqlAdmin.post_data = R"({"query":"mutation DropEverything { deleteAllUsers }"})";
            gqlAdmin.allowed_mutations_mask = incMask;
            CHECK(!pol.Evaluate(gqlAdmin).allowed);
        }
        gqlMut.allowed_mutations_mask = 0u;
        CHECK(!pol.Evaluate(gqlMut).allowed);

        // Personal files mask enables folder creation & DELETE while keeping logout permanently blocked
        {
            const uint32_t pfMask = static_cast<uint32_t>(AllowedStudentMutation::ManagePersonalFiles);
            PolicyRequestContext mkf;
            mkf.method = "POST";
            mkf.url = "https://tecdigital.tec.ac.cr/dotlrn/file-storage/api/folder/5001";
            mkf.allowed_mutations_mask = pfMask;
            CHECK(pol.Evaluate(mkf).allowed);
            PolicyRequestContext delF;
            delF.method = "DELETE";
            delF.url = "https://tecdigital.tec.ac.cr/dotlrn/file-storage/api/file/5020";
            delF.allowed_mutations_mask = pfMask;
            CHECK(pol.Evaluate(delF).allowed);
            PolicyRequestContext logOut;
            logOut.method = "GET";
            logOut.url = "https://tecdigital.tec.ac.cr/register/logout";
            logOut.allowed_mutations_mask = pfMask;
            CHECK(!pol.Evaluate(logOut).allowed);
        }
    }
}

}  // namespace

int main() {
    TestM31Hardening();
    TestStorageWalkerAndDownloader();
    TestHeadlessCrawler();
    TestMaterialIndexer();
    TestTdaTdsAndService();
    TestR2Remediations();
    TestR3Remediations();
    TestR4Remediations();
    TestPhase2LiveProtocolAndGraphQl();
    TestPublicGuiaHorarios();
    TestLive2026ParsersAndContracts();
    TestV3SuperPlanParsersAndPolicy();

    std::cout << "TecDigitalAcademicTest: " << gPasses << " passed, " << gFailures << " failed.\n";
    return (gFailures == 0) ? 0 : 1;
}
