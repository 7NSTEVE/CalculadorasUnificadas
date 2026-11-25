#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/canon.hpp"
#include "academic/tec_digital/cookie_jar.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/types.hpp"
#include "academic/tec_digital/vault.hpp"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace academic::tec_digital;

namespace {

int gFailures = 0;
int gPasses = 0;

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
        dir = fs::temp_directory_path() / ("tec_digital_m31_" + tag + "_" + std::to_string(ts));
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

    explicit MockHttpTransport(Handler handler) : handler_(std::move(handler)) {}

    HttpResponse Execute(const HttpRequest& req) override {
        recorded_requests.push_back(req);
        return handler_(req);
    }

    std::vector<HttpRequest> recorded_requests;

private:
    Handler handler_;
};

// Minimal real 127.0.0.1 HTTP/1.1 loopback test server
class LoopbackHttpServer {
public:
    using Handler = std::function<std::string(const std::string& method,
                                              const std::string& target,
                                              const std::string& headers_lower,
                                              const std::string& body)>;

    explicit LoopbackHttpServer(Handler handler) : handler_(std::move(handler)) {}

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
        int addrlen = sizeof(addr);
        if (::getsockname(listen_sock_, reinterpret_cast<sockaddr*>(&addr), &addrlen) != 0) return false;
        port_ = ntohs(addr.sin_port);
        if (::listen(listen_sock_, 16) != 0) return false;
#else
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
        socklen_t addrlen = sizeof(addr);
        if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addrlen) != 0) return false;
        port_ = ntohs(addr.sin_port);
        if (::listen(fd, 16) != 0) return false;
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

    uint16_t Port() const { return port_; }
    std::string BaseUrl() const { return "http://127.0.0.1:" + std::to_string(port_); }

private:
    void ServeLoop() {
        while (running_.load()) {
#ifdef _WIN32
            SOCKET client = ::accept(listen_sock_, nullptr, nullptr);
            if (client == INVALID_SOCKET) break;
            HandleClientWin(client);
            ::closesocket(client);
#else
            int fd = listen_fd_.load();
            if (fd < 0) break;
            int client = ::accept(fd, nullptr, nullptr);
            if (client < 0) break;
            HandleClientPosix(client);
            ::close(client);
#endif
        }
    }

    template <typename RecvFn, typename SendFn>
    void HandleConnection(RecvFn recv_fn, SendFn send_fn) {
        std::string raw;
        char buf[4096];
        size_t hdr_end = std::string::npos;
        while ((hdr_end = raw.find("\r\n\r\n")) == std::string::npos) {
            int n = recv_fn(buf, sizeof(buf));
            if (n <= 0) return;
            raw.append(buf, static_cast<size_t>(n));
            if (raw.size() > 65536) return;
        }

        std::string hdr_block = raw.substr(0, hdr_end);
        std::string body = raw.substr(hdr_end + 4);

        std::istringstream iss(hdr_block);
        std::string req_line;
        std::getline(iss, req_line);
        if (!req_line.empty() && req_line.back() == '\r') req_line.pop_back();
        std::istringstream rls(req_line);
        std::string method, target;
        rls >> method >> target;

        size_t content_len = 0;
        std::string line;
        std::string headers_lower;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            headers_lower += AsciiLower(line) + "\n";
            if (AsciiLower(line).rfind("content-length:", 0) == 0) {
                content_len = static_cast<size_t>(std::strtoull(line.c_str() + 15, nullptr, 10));
            }
        }

        while (body.size() < content_len) {
            int n = recv_fn(buf, sizeof(buf));
            if (n <= 0) break;
            body.append(buf, static_cast<size_t>(n));
        }
        if (body.size() > content_len) body.resize(content_len);

        std::string resp = handler_(method, target, headers_lower, body);
        size_t sent = 0;
        while (sent < resp.size()) {
            int n = send_fn(resp.data() + sent, resp.size() - sent);
            if (n <= 0) break;
            sent += static_cast<size_t>(n);
        }
    }

#ifdef _WIN32
    void HandleClientWin(SOCKET s) {
        HandleConnection(
            [s](char* b, size_t cap) { return ::recv(s, b, static_cast<int>(cap), 0); },
            [s](const char* b, size_t len) { return ::send(s, b, static_cast<int>(len), 0); });
    }
    SOCKET listen_sock_ = INVALID_SOCKET;
    bool wsa_started_ = false;
#else
    void HandleClientPosix(int fd) {
        HandleConnection(
            [fd](char* b, size_t cap) { return static_cast<int>(::recv(fd, b, cap, 0)); },
            [fd](const char* b, size_t len) { return static_cast<int>(::send(fd, b, len, 0)); });
    }
    std::atomic<int> listen_fd_{-1};
#endif

    Handler handler_;
    uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

void TestCrypto() {
    CHECK(Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha1Hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");

    auto rnd1 = RandomBytes(32);
    auto rnd2 = RandomBytes(32);
    CHECK(rnd1.size() == 32);
    CHECK(rnd2.size() == 32);
    CHECK(rnd1 != rnd2);

    std::string b64 = Base64Encode("TecDigital-C++17!");
    std::vector<uint8_t> decoded;
    CHECK(Base64Decode(b64, decoded));
    CHECK(std::string(decoded.begin(), decoded.end()) == "TecDigital-C++17!");
    CHECK(!Base64Decode("%%%invalid%%%", decoded));

    ScopedTempDir tmp("crypto");
    fs::path fp = tmp.dir / "hash_test.bin";
    {
        std::ofstream ofs(fp, std::ios::binary);
        ofs << "abc";
    }
    std::string err;
    CHECK(Sha256FileHex(fp.string(), err) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(err.empty());

    std::string secret = "FAKE_SECRET_1234";
    SecureZeroString(secret);
    CHECK(secret.empty());
}

void TestCookieJar() {
    int64_t t1980 = 0, t2035 = 0;
    CHECK(CookieJar::ParseHttpDate("Fri, 01-Jan-1980 01:00:00 GMT", t1980));
    CHECK(CookieJar::ParseHttpDate("Fri, 01-Jan-2035 01:00:00 GMT", t2035));
    CHECK(t1980 < t2035);
    int64_t t_rfc1123 = 0;
    CHECK(CookieJar::ParseHttpDate("Fri, 01 Jan 2035 01:00:00 GMT", t_rfc1123));
    CHECK(t_rfc1123 == t2035);

    const int64_t now = 1700000000;  // 2023-11-14
    CookieJar jar;

    // Pre-login GET / seeds AlteonP on Path=/
    jar.ApplySetCookieHeaders({"AlteonP=sticky_v1; Path=/"}, "tecdigital.tec.ac.cr", "/", now);

    // Ordered Set-Cookie delete + set (matches HAR [17])
    jar.ApplySetCookieHeaders(
        {
            "AlteonP=sticky_v2;",
            "ad_user_login=old_user; Expires=Fri, 01-Jan-1980 01:00:00 GMT; Path=/; HttpOnly",
            "ad_secure_token=dead_tok; Expires=Fri, 01-Jan-1980 01:00:00 GMT; Path=/; Secure; HttpOnly",
            "ad_user_login_secure=dead_sec; Expires=Fri, 01-Jan-1980 01:00:00 GMT; Path=/; Secure; HttpOnly",
            "ad_user_login=FAKE_NEW_USER_LOGIN; Expires=Fri, 01-Jan-2035 01:00:00 GMT; Path=/; HttpOnly",
            "ad_session_id=FAKE_SESSION_12345; Expires=Fri, 01-Jan-2035 01:00:00 GMT; Path=/; Discard; HttpOnly",
            "_ga=GA1.2.999; Domain=.tec.ac.cr; Path=/; Expires=Fri, 01-Jan-2035 01:00:00 GMT"
        },
        "tecdigital.tec.ac.cr", "/api/login/new-form/", now);

    CHECK(jar.HasCookie("ad_session_id", "tecdigital.tec.ac.cr", "/dotlrn/", true, now));
    auto ulogin = jar.GetCookie("ad_user_login", "tecdigital.tec.ac.cr", "/dotlrn/", true, now);
    CHECK(ulogin.has_value() && ulogin->value == "FAKE_NEW_USER_LOGIN");
    CHECK(!jar.HasCookie("ad_secure_token", "tecdigital.tec.ac.cr", "/", true, now));
    CHECK(!jar.HasCookie("ad_user_login_secure", "tecdigital.tec.ac.cr", "/", true, now));

    // AlteonP from GET / is still sent on /dotlrn/ even though POST /api/login/new-form/ omitted Path=
    std::string dotlrn_hdr = jar.BuildCookieHeader("https://tecdigital.tec.ac.cr/dotlrn/", now);
    CHECK(dotlrn_hdr.find("AlteonP=sticky_v1") != std::string::npos);
    CHECK(dotlrn_hdr.find("ad_session_id=FAKE_SESSION_12345") != std::string::npos);
    CHECK(dotlrn_hdr.find("ad_user_login=FAKE_NEW_USER_LOGIN") != std::string::npos);
    CHECK(dotlrn_hdr.find("_ga=GA1.2.999") != std::string::npos);
    CHECK(dotlrn_hdr.find("ad_secure_token") == std::string::npos);

    // Secure cookie omitted over http://, Max-Age=0 deletes cookie
    jar.ApplySetCookieHeaders(
        {"sec_only=FAKE_SEC_VAL; Path=/; Secure", "temp_ck=abc; Path=/; Max-Age=100"},
        "tecdigital.tec.ac.cr", "/", now);
    CHECK(jar.BuildCookieHeader("https://tecdigital.tec.ac.cr/", now).find("sec_only=FAKE_SEC_VAL") !=
          std::string::npos);
    CHECK(jar.BuildCookieHeader("http://tecdigital.tec.ac.cr/", now).find("sec_only=") ==
          std::string::npos);
    jar.ApplySetCookieHeaders({"temp_ck=bye; Path=/; Max-Age=0"}, "tecdigital.tec.ac.cr", "/", now);
    CHECK(!jar.HasCookie("temp_ck", "tecdigital.tec.ac.cr", "/", true, now));

    // Sanitized metadata must never expose raw cookie values
    std::string meta_json = jar.ExportSanitizedMetadataJson("2026-11-17T00:00:00Z", now);
    CHECK(meta_json.find("FAKE_SESSION_12345") == std::string::npos);
    CHECK(meta_json.find("FAKE_NEW_USER_LOGIN") == std::string::npos);
    CHECK(meta_json.find(Sha256Hex("FAKE_SESSION_12345").substr(0, 8)) != std::string::npos);

    // JSON round-trip
    std::string ser = jar.SerializeToJson();
    CookieJar jar2;
    CHECK(jar2.DeserializeFromJson(ser));
    CHECK(jar2.HasCookie("ad_session_id", "tecdigital.tec.ac.cr", "/dotlrn/", true, now));

    // Item 9: Huge Max-Age must NOT overflow signed int64 and delete cookie
    jar.ApplySetCookieHeaders({"huge_age=alive; Path=/; Max-Age=99999999999999999999"},
                              "tecdigital.tec.ac.cr", "/", now);
    CHECK(jar.HasCookie("huge_age", "tecdigital.tec.ac.cr", "/", true, now));
    jar.ApplySetCookieHeaders({"huge_neg=dead; Path=/; Max-Age=-99999999999999999999"},
                              "tecdigital.tec.ac.cr", "/", now);
    CHECK(!jar.HasCookie("huge_neg", "tecdigital.tec.ac.cr", "/", true, now));

    // Item 9: RFC 6265 IPv4/IPv6 Domain= suffix injection must be rejected
    jar.ApplySetCookieHeaders({"ip_inj=1; Domain=0.0.1; Path=/"}, "127.0.0.1", "/", now);
    CHECK(!jar.HasCookie("ip_inj", "127.0.0.1", "/", false, now));
    jar.ApplySetCookieHeaders({"ip6_inj=1; Domain=1; Path=/"}, "::1", "/", now);
    CHECK(!jar.HasCookie("ip6_inj", "::1", "/", false, now));
    jar.ApplySetCookieHeaders({"ip_exact=1; Domain=127.0.0.1; Path=/"}, "127.0.0.1", "/", now);
    CHECK(jar.HasCookie("ip_exact", "127.0.0.1", "/", false, now));
}

void TestAuthClientMocked() {
    // 1. Successful login: email lowercasing, special chars in password, AlteonP handshake, /dotlrn/ check
    {
        MockHttpTransport mock([](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.final_url = req.url;
            if (req.method == "GET" && req.url == "https://tecdigital.tec.ac.cr/") {
                r.status_code = 200;
                r.set_cookies.push_back("AlteonP=lb_root; Path=/");
                return r;
            }
            if (req.method == "POST" && req.url == "https://tecdigital.tec.ac.cr/api/login/new-form/") {
                auto j = nlohmann::json::parse(req.body);
                CHECK(j["email"] == "user.name@estudiantec.cr");
                CHECK(j["password"] == "Pass\"With\\Slash\nAndUTF8_\xc3\xb1");
                CHECK(j["returnUrl"] == "/dotlrn/");
                CHECK(req.headers.count("X-Content-Type-Options") > 0 &&
                      req.headers.at("X-Content-Type-Options") == "nosniff");
                r.status_code = 200;
                r.content_type = "application/json; charset=utf-8";
                r.set_cookies = {
                    "ad_user_login=x; Expires=Fri, 01-Jan-1980 01:00:00 GMT; Path=/",
                    "ad_user_login=FAKE_USER_TOKEN; Expires=Fri, 01-Jan-2035 01:00:00 GMT; Path=/; HttpOnly",
                    "ad_session_id=FAKE_SID_999; Expires=Fri, 01-Jan-2035 01:00:00 GMT; Path=/; Discard; HttpOnly"
                };
                std::string b = R"({"status":"ok","returnUrl":"/dotlrn/index"})";
                r.body.assign(b.begin(), b.end());
                return r;
            }
            if (req.method == "GET" && req.url == "https://tecdigital.tec.ac.cr/dotlrn/") {
                CHECK(req.headers.count("Cookie") > 0);
                CHECK(req.headers.at("Cookie").find("AlteonP=lb_root") != std::string::npos);
                CHECK(req.headers.at("Cookie").find("ad_session_id=FAKE_SID_999") != std::string::npos);
                r.status_code = 200;
                r.content_type = "text/html";
                std::string html = "<html><body><h1>DotLRN</h1><a href=\"/register/logout\">Salir</a></body></html>";
                r.body.assign(html.begin(), html.end());
                return r;
            }
            r.status_code = 404;
            return r;
        });

        AuthClient client(mock);
        AuthResult res = client.LoginWithPassword("  USER.Name@EstudianTEC.CR ", "Pass\"With\\Slash\nAndUTF8_\xc3\xb1");
        CHECK(res.ok);
        CHECK(res.status == AuthStatus::Ok);
        CHECK(res.ad_session_id_present);
        CHECK(!client.IsSessionExpired());
        CHECK(mock.recorded_requests.size() == 3);
    }

    // 2. HTTP 200 with no_account -> fail closed, zero auto-retry
    {
        MockHttpTransport mock([](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.status_code = 200;
            r.final_url = req.url;
            std::string b = R"({"status":"no_account","returnUrl":"/register"})";
            r.body.assign(b.begin(), b.end());
            return r;
        });
        AuthClientOptions opts;
        opts.perform_prelogin_handshake = false;
        AuthClient client(mock, ReadOnlyRequestPolicy{}, opts);
        AuthResult res = client.LoginWithPassword("test@example.invalid", "FAKE_PWD");
        CHECK(!res.ok);
        CHECK(res.status == AuthStatus::NoAccount);
        CHECK(res.error_code == 5);
        CHECK(mock.recorded_requests.size() == 1);
    }

    // 3. HTTP 200 ok but missing ad_session_id -> MissingSessionCookie
    {
        MockHttpTransport mock([](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.status_code = 200;
            r.final_url = req.url;
            std::string b = R"({"status":"ok","returnUrl":"/dotlrn/"})";
            r.body.assign(b.begin(), b.end());
            return r;
        });
        AuthClientOptions opts;
        opts.perform_prelogin_handshake = false;
        AuthClient client(mock, ReadOnlyRequestPolicy{}, opts);
        AuthResult res = client.LoginWithPassword("test@example.invalid", "FAKE_PWD");
        CHECK(!res.ok);
        CHECK(res.status == AuthStatus::MissingSessionCookie);
    }

    // 4. All 19+ status codes & zero auto-retries on bad_password / muchos_intentos
    {
        struct Case {
            const char* raw;
            AuthStatus expected;
            const char* msg;
        };
        const Case cases[] = {
            {"no_account", AuthStatus::NoAccount, ""},
            {"bad_password", AuthStatus::BadPassword, ""},
            {"empty_email_or_password", AuthStatus::EmptyCredentials, ""},
            {"invalid_retoken", AuthStatus::InvalidRetoken, ""},
            {"login_expired", AuthStatus::LoginExpired, ""},
            {"invalid_captcha", AuthStatus::InvalidCaptcha, ""},
            {"error_ws", AuthStatus::BackendWsError, ""},
            {"muchos_intentos", AuthStatus::TooManyAttempts, ""},
            {"error_desconocido", AuthStatus::UnknownServerError, ""},
            {"datic_auth_error", AuthStatus::DaticAuthError, "DATIC err msg"},
            {"datic_bad_password", AuthStatus::DaticBadPassword, "Bad DATIC pwd"},
            {"datic_ws_communication_error", AuthStatus::DaticWsError, "WS timeout"},
            {"account_url", AuthStatus::ActionRedirect, ""},
            {"account_message", AuthStatus::AccountMessage, ""},
            {"register-account-closed", AuthStatus::AccountClosed, ""},
            {"survey-vida", AuthStatus::InterstitialRedirect, ""},
            {"survey-vida-profesor", AuthStatus::InterstitialRedirect, ""},
            {"insurance-policy", AuthStatus::InterstitialRedirect, ""},
            {"verificacion-correo-evaluaciondocente", AuthStatus::InterstitialRedirect, ""},
            {"totally_unseen_status", AuthStatus::UnknownError, ""}
        };
        for (const auto& c : cases) {
            CHECK(AuthClient::ParseAuthStatus(c.raw) == c.expected);
            MockHttpTransport mock([&](const HttpRequest& req) -> HttpResponse {
                HttpResponse r;
                r.transport_ok = true;
                r.status_code = 200;
                r.final_url = req.url;
                nlohmann::json j = {{"status", c.raw}};
                if (c.msg[0] != '\0') j["message"] = c.msg;
                std::string b = j.dump();
                r.body.assign(b.begin(), b.end());
                return r;
            });
            AuthClientOptions opts;
            opts.perform_prelogin_handshake = false;
            AuthClient client(mock, ReadOnlyRequestPolicy{}, opts);
            AuthResult res = client.LoginWithPassword("test@example.invalid", "FAKE_PWD");
            CHECK(!res.ok);
            CHECK(res.status == c.expected);
            if (c.msg[0] != '\0') CHECK(res.message == c.msg);
            CHECK(mock.recorded_requests.size() == 1);
        }
    }

    // 5. QR Scanning endpoint + empty email claim
    {
        MockHttpTransport mock([](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.final_url = req.url;
            if (req.url == "https://tecdigital.tec.ac.cr/api/login/scanning/") {
                auto j = nlohmann::json::parse(req.body);
                CHECK(j["email"] == "test@example.invalid");
                r.status_code = 200;
                r.set_cookies = {"ad_session_id=FAKE_QR_SID; Path=/; HttpOnly"};
                std::string b = R"({"status":"ok","returnUrl":"/dotlrn/index"})";
                r.body.assign(b.begin(), b.end());
                return r;
            }
            r.status_code = 200;
            std::string b = "<html><body>Portal</body></html>";
            r.body.assign(b.begin(), b.end());
            return r;
        });
        AuthClient client(mock);
        AuthResult empty_qr = client.AttemptQrScanning("  ");
        CHECK(!empty_qr.ok);
        CHECK(empty_qr.status == AuthStatus::MissingQrEmailClaim);
        CHECK(empty_qr.error_code == 6);

        AuthResult qr_ok = client.AttemptQrScanning("TEST@EXAMPLE.INVALID");
        CHECK(qr_ok.ok);
        CHECK(qr_ok.status == AuthStatus::Ok);
    }

    // 6. Session expiry detection & 403 Permission vs Expiry with 120s TTL cache
    {
        int dotlrn_probes = 0;
        bool portal_alive = true;
        MockHttpTransport mock([&](const HttpRequest& req) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.final_url = req.url;
            if (req.url == "https://tecdigital.tec.ac.cr/dotlrn/") {
                ++dotlrn_probes;
                if (portal_alive) {
                    r.status_code = 200;
                    r.content_type = "text/html";
                    std::string html = "<html><body><a href=\"/register/logout\">Logout</a></body></html>";
                    r.body.assign(html.begin(), html.end());
                } else {
                    r.status_code = 302;
                    r.location = "https://tecdigital.tec.ac.cr/register/?return_url=/dotlrn/";
                }
                return r;
            }
            r.status_code = 200;
            return r;
        });
        AuthClient client(mock);

        // 403 while portal is healthy -> PermissionDenied, session_expired_ stays false
        HttpResponse r403;
        r403.transport_ok = true;
        r403.status_code = 403;
        CHECK(client.ValidateResponse(r403, "https://tecdigital.tec.ac.cr/dotlrn/restricted") ==
              ResponseValidity::PermissionDenied);
        CHECK(!client.IsSessionExpired());
        CHECK(dotlrn_probes == 1);

        // Second 403 within 120s TTL hits cached probe without extra network call
        CHECK(client.ValidateResponse(r403, "https://tecdigital.tec.ac.cr/dotlrn/restricted2") ==
              ResponseValidity::PermissionDenied);
        CHECK(dotlrn_probes == 1);

        // 302 redirect to /register/ trips session_expired_
        HttpResponse r302;
        r302.transport_ok = true;
        r302.status_code = 302;
        r302.location = "https://tecdigital.tec.ac.cr/register/?return_url=/dotlrn/";
        CHECK(client.ValidateResponse(r302, "https://tecdigital.tec.ac.cr/dotlrn/file") ==
              ResponseValidity::SessionExpired);
        CHECK(client.IsSessionExpired());

        // HTML loginForm / userPassword marker trips session_expired_
        client.ResetSessionExpired();
        HttpResponse rhtml;
        rhtml.transport_ok = true;
        rhtml.status_code = 200;
        rhtml.content_type = "text/html";
        std::string login_html = "<form id=\"login-form\"><input name=\"userPassword\"></form>";
        rhtml.body.assign(login_html.begin(), login_html.end());
        CHECK(client.ValidateResponse(rhtml, "https://tecdigital.tec.ac.cr/dotlrn/file") ==
              ResponseValidity::SessionExpired);
        CHECK(client.IsSessionExpired());

        // 403 when portal redirects to /register/ -> SessionExpired
        client.ResetSessionExpired();
        portal_alive = false;
        CHECK(client.ValidateResponse(r403, "https://tecdigital.tec.ac.cr/dotlrn/file") ==
              ResponseValidity::SessionExpired);
        CHECK(client.IsSessionExpired());

        // Item 6: Stale 120s cache must be invalidated when session_expired_ latches (403 -> /register/ 302 -> 403)
        client.ResetSessionExpired();
        portal_alive = true;
        int probes_before = dotlrn_probes;
        CHECK(client.ValidateResponse(r403, "https://tecdigital.tec.ac.cr/dotlrn/restricted") ==
              ResponseValidity::PermissionDenied);
        CHECK(dotlrn_probes == probes_before + 1);
        CHECK(client.ValidateResponse(r302, "https://tecdigital.tec.ac.cr/dotlrn/file") ==
              ResponseValidity::SessionExpired);
        CHECK(client.IsSessionExpired());
        CHECK(client.ValidateResponse(r403, "https://tecdigital.tec.ac.cr/dotlrn/restricted_again") ==
              ResponseValidity::SessionExpired);

        // Item 6: HTML loginForm with >64B leading whitespace & non-HTML Content-Type detected as SessionExpired
        client.ResetSessionExpired();
        HttpResponse r_ws_html;
        r_ws_html.transport_ok = true;
        r_ws_html.status_code = 200;
        r_ws_html.content_type = "application/octet-stream";
        std::string padded_html = std::string(100, ' ') +
                                  "<!DOCTYPE html><html><body><form id=\"login-form\"></form></body></html>";
        r_ws_html.body.assign(padded_html.begin(), padded_html.end());
        CHECK(client.ValidateResponse(r_ws_html, "https://tecdigital.tec.ac.cr/dotlrn/file") ==
              ResponseValidity::SessionExpired);
        CHECK(client.IsSessionExpired());
    }
}

void TestVault() {
    // Env resolution
    SetEnvVar("TEC_EMAIL", "");
    SetEnvVar("TEC_PASSWORD", "");
    std::string env_err;
    CHECK(!CredentialVault::ResolveFromEnv(&env_err).has_value());

    SetEnvVar("TEC_EMAIL", "test@example.invalid");
    SetEnvVar("TEC_PASSWORD", "FAKE_ENV_PWD_99");
    auto creds_opt = CredentialVault::ResolveFromEnv(&env_err);
    CHECK(creds_opt.has_value());
    CHECK(creds_opt->email == "test@example.invalid");
    CHECK(creds_opt->password == "FAKE_ENV_PWD_99");
    SetEnvVar("TEC_EMAIL", "");
    SetEnvVar("TEC_PASSWORD", "");

    ScopedTempDir tmp("vault");
    fs::path vpath = tmp.dir / "vault.tdv";
    Credentials creds("test@example.invalid", "FAKE_VAULT_PWD!@#");
    Cookie c;
    c.name = "ad_session_id";
    c.value = "FAKE_COOKIE_VAL";
    c.domain = "tecdigital.tec.ac.cr";
    c.path = "/";
    c.http_only = true;

#ifdef _WIN32
    auto save_res = CredentialVault::SaveEncrypted(vpath.string(), creds, {c}, true);
    CHECK(save_res.ok);
    auto load_res = CredentialVault::LoadEncrypted(vpath.string());
    CHECK(load_res.ok);
    CHECK(load_res.value.credentials.email == "test@example.invalid");
    CHECK(load_res.value.credentials.password == "FAKE_VAULT_PWD!@#");
    CHECK(load_res.value.cookies.size() == 1);
    CHECK(load_res.value.cookies[0].value == "FAKE_COOKIE_VAL");

    // Flip 1 byte in ciphertext and verify fail-closed
    std::vector<uint8_t> raw;
    {
        std::ifstream ifs(vpath, std::ios::binary);
        raw.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }
    CHECK(raw.size() > 20);
    raw[raw.size() - 5] ^= 0x01;
    {
        std::ofstream ofs(vpath, std::ios::binary | std::ios::trunc);
        ofs.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    }
    CHECK(!CredentialVault::LoadEncrypted(vpath.string()).ok);

    // Truncated header fails closed
    raw.resize(8);
    {
        std::ofstream ofs(vpath, std::ios::binary | std::ios::trunc);
        ofs.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    }
    CHECK(!CredentialVault::LoadEncrypted(vpath.string()).ok);
#else
    auto save_res = CredentialVault::SaveEncrypted(vpath.string(), creds, {c}, true, "machine-A");
    CHECK(save_res.ok);

    struct stat st{};
    CHECK(::stat(vpath.c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);

    auto load_res = CredentialVault::LoadEncrypted(vpath.string(), "machine-A");
    CHECK(load_res.ok);
    CHECK(load_res.value.credentials.email == "test@example.invalid");
    CHECK(load_res.value.credentials.password == "FAKE_VAULT_PWD!@#");
    CHECK(load_res.value.cookies.size() == 1);
    CHECK(load_res.value.cookies[0].value == "FAKE_COOKIE_VAL");

    // Wrong machine/key_override fails closed
    CHECK(!CredentialVault::LoadEncrypted(vpath.string(), "machine-B").ok);

    // Permissive mode 0644 fails closed
    CHECK(::chmod(vpath.c_str(), 0644) == 0);
    auto perm_fail = CredentialVault::LoadEncrypted(vpath.string(), "machine-A");
    CHECK(!perm_fail.ok);
    CHECK(perm_fail.error_code == "permissive_mode");
    CHECK(::chmod(vpath.c_str(), 0600) == 0);

    // Symlink fails closed
    fs::path sym = tmp.dir / "vault_link.tdv";
    fs::create_symlink(vpath, sym);
    auto sym_fail = CredentialVault::LoadEncrypted(sym.string(), "machine-A");
    CHECK(!sym_fail.ok);
    CHECK(sym_fail.error_code == "symlink_rejected");

    // 1-bit tamper in tag/ciphertext fails closed
    std::vector<uint8_t> raw;
    {
        std::ifstream ifs(vpath, std::ios::binary);
        raw.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }
    CHECK(raw.size() > 65);
    raw[raw.size() - 3] ^= 0x01;
    {
        std::ofstream ofs(vpath, std::ios::binary | std::ios::trunc);
        ofs.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    }
    CHECK(!CredentialVault::LoadEncrypted(vpath.string(), "machine-A").ok);
#endif

    // Sanitized session_meta.json contains zero raw secrets
    CookieJar jar;
    jar.SetCookies({c});
    fs::path meta_path = tmp.dir / "session_meta.json";
    CHECK(CredentialVault::WriteSanitizedSessionMeta(meta_path.string(), jar, "2026-11-17T00:00:00Z").ok);
    std::ifstream mifs(meta_path);
    std::string meta_txt((std::istreambuf_iterator<char>(mifs)), std::istreambuf_iterator<char>());
    CHECK(meta_txt.find("FAKE_COOKIE_VAL") == std::string::npos);
    CHECK(meta_txt.find(Sha256Hex("FAKE_COOKIE_VAL").substr(0, 8)) != std::string::npos);

    // Item 10: UTF-8 non-ASCII path handling in SaveEncrypted/LoadEncrypted/WriteSanitizedSessionMeta
    {
        fs::path utf8_dir = tmp.dir / fs::u8path("t\xc3\xa9st_\xc3\xb1_dir");
        std::string utf8_vpath = (utf8_dir / "vault.tdv").u8string();
        std::string utf8_mpath = (utf8_dir / "session_meta.json").u8string();
        CHECK(CredentialVault::SaveEncrypted(utf8_vpath, creds, {c}, true, "machine-utf8").ok);
        auto uload = CredentialVault::LoadEncrypted(utf8_vpath, "machine-utf8");
        CHECK(uload.ok && uload.value.credentials.password == "FAKE_VAULT_PWD!@#");
        CHECK(CredentialVault::WriteSanitizedSessionMeta(utf8_mpath, jar, "2026-11-17T00:00:00Z").ok);
    }
}

void TestReadOnlyPolicy() {
    ReadOnlyRequestPolicy policy;
    auto Eval = [&](std::string url,
                    std::string method = "GET",
                    std::string resource_type = "document",
                    std::string post_data = "",
                    bool allow_login = false,
                    std::string link_label = "") {
        PolicyRequestContext ctx;
        ctx.url = std::move(url);
        ctx.method = std::move(method);
        ctx.resource_type = std::move(resource_type);
        ctx.post_data = std::move(post_data);
        ctx.allow_login = allow_login;
        ctx.link_label = std::move(link_label);
        return policy.Evaluate(ctx);
    };

    // Allowed safe GETs including false-positive exclusions
    CHECK(Eval("https://tecdigital.tec.ac.cr/dotlrn/index", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/dotlrn/file-storage/api/folder/123", "HEAD").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/dotlrn/classes/E/EL2113/S-2-2026.SC.EL2113.50/forums/message-view?message_id=1", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/dotlrn/file-storage/view/examen_1.pdf", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/dotlrn/file-storage/download/tarea_delete.pdf", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/dotlrn/evaluation/tda-ce-estudiante/tda-index", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/resources/delete-icon.png", "GET", "image").allowed);

    // Unsupported schemes / external origins / unsafe verbs
    CHECK(!Eval("javascript:alert(1)", "GET").allowed);
    CHECK(!Eval("mailto:test@example.invalid", "GET").allowed);
    CHECK(!Eval("https://evil.example/dotlrn/", "GET").allowed);
    CHECK(!Eval("http://tecdigital.tec.ac.cr/dotlrn/", "GET").allowed);
    for (const char* verb : {"PUT", "PATCH", "DELETE", "TRACE"}) {
        CHECK(!Eval("https://tecdigital.tec.ac.cr/dotlrn/", verb).allowed);
    }

    // Login POST phase guard & register-login-method block
    auto login_blocked = Eval("https://tecdigital.tec.ac.cr/api/login/new-form/", "POST", "document", "{}", false);
    CHECK(!login_blocked.allowed && login_blocked.reason == "login-outside-authentication-phase");
    CHECK(Eval("https://tecdigital.tec.ac.cr/api/login/new-form/", "POST", "document", "{}", true).allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/api/login/register-login-method/", "POST", "document", "{}", true).allowed);

    // All 7 exact READ_ONLY_POST_PATHS + dynamic load_members
    const char* allowed_posts[] = {
        "/notifications/ajax/request_id",
        "/tda-consentimiento-informado/ajax/client/get_user_term_condition",
        "/tda-expediente-estudiantil/ajax/i18n",
        "/tda-expediente-estudiantil/ajax/i18n_validation",
        "/tda-feria-negocios/ajax/internationalization",
        "/tda-um/ajax/get_gaap_server",
        "/tda-um/ajax/internacionalization",
        "/dotlrn/estudiantes.tec/ajax/load_members"
    };
    for (const char* p : allowed_posts) {
        CHECK(Eval(std::string("https://tecdigital.tec.ac.cr") + p, "POST").allowed);
    }

    // Blocked POSTs
    CHECK(!Eval("https://tecdigital.tec.ac.cr/dotlrn/estudiantes.tec/ajax/add_members", "POST").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-notifications/ajax/delete_all_user_notifications", "POST").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/profile/save", "POST").allowed);
    CHECK(!Eval("https://evil.example/notifications/ajax/request_id", "POST").allowed);

    // prematricula_est guard (form & JSON)
    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/prematricula_est", "POST", "document",
               "tipo=1&pre_enrollment_id=123&curriculum_id=456&curriculum_id_current=456").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/prematricula_est", "POST", "document",
               R"({"tipo":"2","curriculum_id":10})").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/prematricula_est", "POST", "document",
                "tipo=3&pre_enrollment_id=123").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/prematricula_est", "POST", "document",
                "pre_enrollment_id=123").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/prematricula_est", "POST", "document",
                "tipo=1&delete=true").allowed);

    // GraphQL guard
    CHECK(Eval("https://api.tec.ac.cr:3005/graphql", "OPTIONS").allowed);
    CHECK(Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document",
               R"({"query":"query Courses { courses { name } }"})").allowed);
    CHECK(Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document",
               R"({"query":"{ courses { name } }"})").allowed);
    CHECK(Eval("https://api.tec.ac.cr:3005/graphql?query=%7Bme%7Bname%7D%7D", "GET").allowed);
    CHECK(!Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document",
                R"({"query":"mutation Drop { drop }"})").allowed);
    CHECK(!Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document",
                R"({"query":"subscription S { events }"})").allowed);
    CHECK(!Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document",
                R"({"query":"query Read { me { name } } mutation Write { save }"})").allowed);
    CHECK(!Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document",
                "{\"query\":\"# query\\nmutation Write { save }\"}").allowed);
    const std::string gql_literal_mut = R"({"query":"{ search(q: \"mutation\") }"})";
    CHECK(!Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document", gql_literal_mut).allowed);
    CHECK(!Eval("https://example.com/graphql", "POST", "document",
                R"({"query":"query Read { me { name } }"})").allowed);

    // Mutating GET routes blocked
    const char* blocked_gets[] = {
        "/register/logout",
        "/file-storage/delete?file_id=1",
        "/dotlrn/deregister?community_id=123",
        "/dotlrn/x/member-confirm-2",
        "/dotlrn/calendar/cal-item-new?date=2026-08-04",
        "/dotlrn/x?op=shade",
        "/dotlrn/archivos/borrar_xls?nombre=x",
        "/notifications/ajax/mark-all-read",
        "/dotlrn/x?action=delete"
    };
    for (const char* g : blocked_gets) {
        CHECK(!Eval(std::string("https://tecdigital.tec.ac.cr") + g, "GET").allowed);
    }

    // Mutating UI labels blocked
    CHECK(!Eval("https://tecdigital.tec.ac.cr/dotlrn/index", "GET", "document", "", false, "Dar de baja").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/dotlrn/index", "GET", "document", "", false, "Confirmar").allowed);

    // PolicyEnforcingTransport blocks off-origin redirect even with follow_redirects=true
    {
        MockHttpTransport mock([](const HttpRequest&) -> HttpResponse {
            HttpResponse r;
            r.transport_ok = true;
            r.status_code = 302;
            r.location = "https://evil.example/steal";
            return r;
        });
        PolicyEnforcingTransport enforcing(mock, policy);
        HttpRequest rreq;
        rreq.url = "https://tecdigital.tec.ac.cr/dotlrn/index";
        rreq.follow_redirects = true;
        HttpResponse rresp = enforcing.Execute(rreq);
        CHECK(!rresp.transport_ok);
        CHECK(rresp.error_message.find("OffOriginRedirect") != std::string::npos);
    }

    // Item 2: Percent-encoded GET bypasses, dot-segment traversal & PathForSegmentCheck bypasses blocked
    const char* blocked_bypass_gets[] = {
        "/register/%6cogout",
        "/dotlrn/archivos/%62orrar_xls?nombre=x",
        "/file-storage/%64elete?file_id=1",
        "/dotlrn/calendar/%63al-item-new/",
        "/dotlrn/x/member%2dconfirm-2",
        "/dotlrn/file-storage/view/../../register/logout",
        "/dotlrn/file-storage/view/../../delete?file_id=1",
        "/dotlrn/file-storage/download/../../../dotlrn/archivos/borrar_xls",
        "/dotlrn/file-storage/view/x/../../evaluation/submit",
        "/dotlrn/file-storage/download/../delete",
        "/dotlrn/file-storage/download/../../profile/delete",
        "/dotlrn/file-storage/view/sub/delete",
        "/dotlrn/file-storage/view/%2e%2e/delete",
        "/dotlrn/file-storage/view/foo%2fdelete",
        "/dotlrn/file-storage/view/foo%5cdelete",
        "/dotlrn/file-storage/view/foo%00bar",
        "/dotlrn/file-storage/view/bad%ZZpct"
    };
    for (const char* bg : blocked_bypass_gets) {
        CHECK(!Eval(std::string("https://tecdigital.tec.ac.cr") + bg, "GET").allowed);
    }
    // Legitimate percent-encoded filename under /file-storage/view/ still allowed
    CHECK(Eval("https://tecdigital.tec.ac.cr/dotlrn/file-storage/view/examen%5F1.pdf", "GET").allowed);

    // Item 2: PolicyEnforcingTransport blocks 302 Location: /register/%6cogout on hop 1
    {
        int seen_hops = 0;
        MockHttpTransport mock([&](const HttpRequest&) -> HttpResponse {
            ++seen_hops;
            HttpResponse r;
            r.transport_ok = true;
            r.status_code = 302;
            r.location = "/register/%6cogout";
            return r;
        });
        PolicyEnforcingTransport enforcing(mock, policy);
        HttpRequest rreq;
        rreq.url = "https://tecdigital.tec.ac.cr/dotlrn/index";
        rreq.follow_redirects = true;
        HttpResponse rresp = enforcing.Execute(rreq);
        CHECK(!rresp.transport_ok);
        CHECK(seen_hops == 1);
        CHECK(rresp.error_message.rfind("read-only-request-guard:", 0) == 0);
    }

    // Item 4: GraphQL duplicate query parameter bypass blocked (GET and form POST)
    CHECK(!Eval("https://api.tec.ac.cr:3005/graphql?query=query%7Bx%7D&query=mutation%7Bdrop%7D", "GET").allowed);
    CHECK(!Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document",
                "query=query%7Bx%7D&query=mutation%7Bdrop%7D").allowed);

    // Phase 2 Security Hardening: um_get_test_info permanently blocked (GET & POST)
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-um/ajax/um_get_test_info", "POST").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-um/ajax/um_get_test_info", "GET").allowed);

    // Phase 2 Conditional endpoints: termsAndConditions, tda_evaluacion_rubrica, prematricula_est, service_courses_student
    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/cargaDocumentos/termsAndConditions?type=1", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/cargaDocumentos/termsAndConditions?type=3", "GET").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/cargaDocumentos/termsAndConditions?type=2", "GET").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/cargaDocumentos/termsAndConditions", "POST", "document", "type=2").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-control-plagio/ajax/cargaDocumentos/termsAndConditions", "GET").allowed);

    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-rubrica/tda_evaluacion_rubrica?rubric_id=1&accion=ver", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-rubrica/tda_evaluacion_rubrica", "POST", "document", "rubric_id=1&accion=ver").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-rubrica/tda_evaluacion_rubrica", "POST", "document", "rubric_id=1").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-rubrica/tda_evaluacion_rubrica", "POST", "document", "rubric_id=1&accion=evaluation").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-rubrica/tda_evaluacion_rubrica?rubric_id=1&accion=delete_task", "GET").allowed);

    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/prematricula_est", "POST", "document", "tipo=5&curriculum_id=809").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/prematricula_est", "POST", "document", "tipo=4&curriculum_id=809").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/service_courses_student?action=2", "GET").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/service_courses_student?action=1", "GET").allowed);
    CHECK(!Eval("https://tecdigital.tec.ac.cr/tda-prematricula/ajax/service_courses_student", "POST", "document", "action=1").allowed);

    // Phase 2: All 16 GraphQL mutations blocked even inside query/alias wrappers
    const char* gql_mutations[] = {
        "SetInclusionRequest", "DeleteInclusionRequest", "SetScholarship", "SetPeriodCourses",
        "setCourseParameter", "ToogleInclusion", "ToggleInclusion", "DeleteCourseParameter",
        "SendApprovePreconfiguration", "ApprovePreconfiguration", "ChangeApplicationsStatusOrComment",
        "ChangeApplicationsToReviewed", "SavePriorityConfig", "VerifyAlllApplications",
        "VerifyAllApplications", "SetAdmInclusionRequest", "RemoveInclusionRequest", "ToggleStudentException"
    };
    for (const char* mut : gql_mutations) {
        std::string payload = std::string(R"({"query":"query Sneaky { x: )") + mut + R"( { id } }"})";
        CHECK(!Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document", payload).allowed);
    }

    // Phase 2: Read-only GraphQL queries allowed
    const char* gql_queries[] = {
        R"({"query":"query getUserType { getUserType { user_role } }"})",
        R"({"query":"query GetStudentActiveCampaign { GetStudentActiveCampaign(requestUserId: \"1\") { id } }"})",
        R"({"query":"query getStudentPendingGraduatedCourses { getStudentPendingGraduatedCourses(userId: \"1\", carnet: \"2026\") { id name } }"})",
        R"({"query":"query getStudentRequisitsCorrequisitsByCourse { getStudentRequisitsCorrequisitsByCourse(codAsignatura: \"EL2113\") { allRequirementsMet } }"})"
    };
    for (const char* q : gql_queries) {
        CHECK(Eval("https://api.tec.ac.cr:3005/graphql", "POST", "document", q).allowed);
    }

    // Phase 2: 25+ newly discovered mutating routes blocked
    const char* phase2_blocked[] = {
        "/user/basic-info-update",
        "/notifications/request-new",
        "/notifications/request-delete",
        "/dotlrn/ajax/save_browser_notification",
        "/tda-sms-configuration/envio/ajax/register",
        "/dotlrn/classes/E/EL2113/S1/ajax/spam",
        "/portal/ajax/portal-configure",
        "/dotlrn/file-storage/delete-elements?object_list=1",
        "/dotlrn/file-storage/api/file/100?set_live_version=2",
        "/tda-motor-juegos/resources/ajax/synchronization",
        "/tda-motor-juegos/resources/ajax/calificar_asignacion",
        "/tda-motor-juegos/resources/ajax/eliminar_calificacion",
        "/servicios/gaap_oacs/PL/funcionAsistenteCreacion.php",
        "/servicios/gaap_oacs/PL/funcionAlmacenarCalificaciones.php",
        "/servicios/gaap_oacs/PL/divAgregarActividad.php",
        "/servicios/gaap_oacs/PL/divShareActivity.php",
        "/tda-prematricula/ajax/guardar_prematricula",
        "/tda-prematricula/ajax/actualiza_prematricula",
        "/tda-prematricula/ajax/recordRequestR",
        "/tda-prematricula/ajax/send_notification",
        "/tda-prematricula/ajax/set_additional_students",
        "/tda-prematricula/ajax/generateProcess",
        "/tda-gestor-inscripcion/estudiante/ajax/popup_matricula",
        "/tda-feria-negocios/ajax/ferias/insert_feria",
        "/tda-feria-negocios/ajax/ferias/update_feria",
        "/tda-feria-negocios/ajax/ferias/delete_feria",
        "/tda-feria-negocios/ajax/ferias/add_folder",
        "/tda-feria-negocios/ajax/registrar_proyectos/insert_project",
        "/tda-feria-negocios/ajax/registrar_proyectos/record_project_file",
        "/tda-feria-negocios/ajax/registrar_proyectos/register_notification",
        "/tds-info-general/files/upload",
        "/tda-consentimiento-informado/ajax/admin/publish_term_condition",
        "/tda-consentimiento-informado/ajax/admin/delete_term_condition"
    };
    for (const char* bp : phase2_blocked) {
        CHECK(!Eval(std::string("https://tecdigital.tec.ac.cr") + bp, "POST").allowed);
        CHECK(!Eval(std::string("https://tecdigital.tec.ac.cr") + bp, "GET").allowed);
    }

    // Phase 2: Verified live read-only POST & GET routes allowed
    const char* phase2_allowed_posts[] = {
        "/dotlrn/classes/E/EL2113/S1/evaluation/tda-ce-estudiante/ajax/Internationalization",
        "/dotlrn/classes/E/EL2113/S1/evaluation/tda-ce-estudiante/ajax/get_history_files",
        "/dotlrn/classes/E/EL2113/S1/evaluation/tda-ce-estudiante/ajax/get_data_liverevision",
        "/dotlrn/classes/E/EL2113/S1/ajax/load_members",
        "/dotlrn/classes/E/EL2113/S1/ajax/load_members_parent",
        "/dotlrn/classes/E/EL2113/S1/ajax/load_pending",
        "/tda-expediente-estudiantil/ajax/get_data_attributes",
        "/tda-expediente-estudiantil/ajax/Consultar_datos_estudiante",
        "/tda-expediente-estudiantil/ajax/get_user_id",
        "/tda-expediente-estudiantil/ajax/combos/carga_sedes",
        "/tda-expediente-estudiantil/ajax/combos/carga_carreras",
        "/tda-expediente-estudiantil/ajax/combos/carga_periodos",
        "/tda-um/ajax/get_statistic_form",
        "/tda-um/ajax/general_statistics",
        "/servicios/gaap_oacs/PL/funcionVisualizadorActividades.php",
        "/servicios/gaap_oacs/PL/funcionVisualizadorHistoricos.php"
    };
    for (const char* ap : phase2_allowed_posts) {
        CHECK(Eval(std::string("https://tecdigital.tec.ac.cr") + ap, "POST").allowed);
    }
    CHECK(Eval("https://tecdigital.tec.ac.cr/dotlrn/file-storage/api/file/12345/versions", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-performance-evaluation/api/forms/", "GET").allowed);
    CHECK(Eval("https://tecdigital.tec.ac.cr/tda-performance-evaluation/api/evaluations/", "GET").allowed);

    // Public Guia de Horarios (https://tec-appsext.itcr.ac.cr/guiahorarios/) read-only gate
    const char* public_guia_allowed_posts[] = {
        "/guiahorarios/escuela.aspx/cargaEscuelas",
        "/guiahorarios/escuela.aspx/cargaModalidadPeriodos",
        "/guiahorarios/escuela.aspx/getdatosEscuelaAno",
        "/guiahorarios/verano.aspx/getdatosVerano",
        "/guiahorarios/estudiante.aspx/getdatos",
        "/guiahorarios/estudiante.aspx/getdatosAno"
    };
    for (const char* gp : public_guia_allowed_posts) {
        CHECK(Eval(std::string("https://tec-appsext.itcr.ac.cr") + gp, "POST", "document", "{}").allowed);
    }
    const char* public_guia_allowed_gets[] = {
        "/guiahorarios",
        "/guiahorarios/",
        "/guiahorarios/estudiante.aspx",
        "/guiahorarios/verano.aspx",
        "/guiahorarios/escuela.aspx"
    };
    for (const char* gg : public_guia_allowed_gets) {
        CHECK(Eval(std::string("https://tec-appsext.itcr.ac.cr") + gg, "GET").allowed);
    }
    CHECK(!Eval("http://tec-appsext.itcr.ac.cr/guiahorarios/escuela.aspx/cargaEscuelas", "POST", "document", "{}").allowed);
    CHECK(!Eval("http://tec-appsext.itcr.ac.cr/guiahorarios/", "GET").allowed);
    CHECK(!Eval("https://tec-appsext.itcr.ac.cr/other/", "GET").allowed);
    CHECK(!Eval("https://tec-appsext.itcr.ac.cr/guiahorarios/other/page", "GET").allowed);
    CHECK(!Eval("https://tec-appsext.itcr.ac.cr/guiahorarios/escuela.aspx", "POST", "document", "{}").allowed);
    CHECK(!Eval("https://tec-appsext.itcr.ac.cr/guiahorarios/admin/save", "POST", "document", "{}").allowed);
    CHECK(!Eval("https://tec-appsext.itcr.ac.cr/guiahorarios/escuela.aspx?op=delete", "GET").allowed);
    for (const char* verb : {"PUT", "DELETE", "PATCH"}) {
        CHECK(!Eval("https://tec-appsext.itcr.ac.cr/guiahorarios/escuela.aspx/cargaEscuelas", verb).allowed);
    }
}

void TestCanonicalizer() {
    auto r1 = UrlCanonicalizer::Canonicalize("https://TECDIGITAL.TEC.AC.CR:443/dotlrn/./file-storage/../calendar/view#tab");
    CHECK(r1.ok && r1.value == "https://tecdigital.tec.ac.cr/dotlrn/calendar/view");

    auto r2 = UrlCanonicalizer::Canonicalize("https://api.tec.ac.cr:3005/graphql");
    CHECK(r2.ok && r2.value == "https://api.tec.ac.cr:3005/graphql");

    auto r3 = UrlCanonicalizer::Canonicalize("https://tecdigital.tec.ac.cr");
    CHECK(r3.ok && r3.value == "https://tecdigital.tec.ac.cr/");

    // Query sorting, avoid_cache / utm_* stripping, blank values preserved
    auto r4 = UrlCanonicalizer::Canonicalize(
        "https://tecdigital.tec.ac.cr/dotlrn/one-community?avoid_cache=2&utm_source=x&b=2&loc=&a=1");
    CHECK(r4.ok && r4.value == "https://tecdigital.tec.ac.cr/dotlrn/one-community?a=1&b=2&loc=");

    // BUG-059 /calendar/view collapse
    auto c1 = UrlCanonicalizer::Canonicalize(
        "https://tecdigital.tec.ac.cr/dotlrn/calendar/view?date=2026-09-02&view=list&period_days=30");
    auto c2 = UrlCanonicalizer::Canonicalize(
        "https://tecdigital.tec.ac.cr/dotlrn/calendar/view?date=2026-09-28&view=list&period_days=7");
    CHECK(c1.ok && c2.ok && c1.value == c2.value);
    CHECK(c1.value == "https://tecdigital.tec.ac.cr/dotlrn/calendar/view?date=2026-09&view=list");

    // BUG-059 /one-community with vs without date
    auto oc1 = UrlCanonicalizer::Canonicalize(
        "https://tecdigital.tec.ac.cr/dotlrn/one-community?date=2024-09-25&page_num=1&period_days=7");
    CHECK(oc1.ok && oc1.value == "https://tecdigital.tec.ac.cr/dotlrn/one-community?date=2024-09&page_num=1");
    auto oc_p1 = UrlCanonicalizer::Canonicalize("https://tecdigital.tec.ac.cr/dotlrn/one-community?page_num=1");
    auto oc_p2 = UrlCanonicalizer::Canonicalize("https://tecdigital.tec.ac.cr/dotlrn/one-community?page_num=2");
    CHECK(oc_p1.ok && oc_p2.ok && oc_p1.value != oc_p2.value);

    // Non-calendar route keeps date & period_days
    auto nc1 = UrlCanonicalizer::Canonicalize("https://tecdigital.tec.ac.cr/dotlrn/file-storage/view/x?date=2026-09-02&period_days=30");
    auto nc2 = UrlCanonicalizer::Canonicalize("https://tecdigital.tec.ac.cr/dotlrn/file-storage/view/x?date=2026-09-02&period_days=7");
    CHECK(nc1.ok && nc2.ok && nc1.value != nc2.value);

    // Relative resolution & non-HTTP rejection
    auto rel = UrlCanonicalizer::Canonicalize("../file-storage/", "https://tecdigital.tec.ac.cr/dotlrn/classes/x");
    CHECK(rel.ok && rel.value == "https://tecdigital.tec.ac.cr/dotlrn/file-storage/");
    CHECK(!UrlCanonicalizer::Canonicalize("javascript:void(0)").ok);

    // page_key lengths & determinism
    std::string k1 = UrlCanonicalizer::PageKeySha1(c1.value);
    std::string k256 = UrlCanonicalizer::PageKeySha256(c1.value);
    std::string oid = UrlCanonicalizer::UrlObjectId(c1.value);
    CHECK(k1.size() == 12 && k1 == Sha1Hex(c1.value).substr(0, 12));
    CHECK(k256.size() == 12 && k256 == Sha256Hex(c1.value).substr(0, 12));
    CHECK(oid == "url:" + Sha256Hex(c1.value).substr(0, 32));

    // Item 8: Relative URL containing :// in query/fragment resolves cleanly against base_url
    auto rel_scheme_q = UrlCanonicalizer::Canonicalize(
        "/redirect?next=https://tecdigital.tec.ac.cr/dotlrn/",
        "https://tecdigital.tec.ac.cr/");
    CHECK(rel_scheme_q.ok &&
          rel_scheme_q.value ==
              "https://tecdigital.tec.ac.cr/redirect?next=https%3A%2F%2Ftecdigital.tec.ac.cr%2Fdotlrn%2F");
}

void TestSanitizer() {
    SanitizeOptions opts;
    opts.runtime_email = "test@example.invalid";
    opts.runtime_password = "Fake$Pwd\"2026";
    opts.pii_needles = {"Nombre Ejemplo", "2026999999"};
    Sanitizer san(opts);

    // Cookie & Set-Cookie redaction (including unknown cookie language=es)
    std::string ck = san.SanitizeHeaderValue("Cookie", "language=es; ad_session_id=FAKE_SID_ABC");
    CHECK(ck.find("language=[REDACTED:sha256=") != std::string::npos);
    CHECK(ck.find("ad_session_id=[REDACTED:sha256=") != std::string::npos);
    CHECK(ck.find("FAKE_SID_ABC") == std::string::npos);

    std::string sck = san.SanitizeHeaderValue(
        "Set-Cookie", "ad_session_id=FAKE_SID_ABC; Path=/; HttpOnly; SameSite=Lax");
    CHECK(sck.find("FAKE_SID_ABC") == std::string::npos);
    CHECK(sck.find("Path=/") != std::string::npos);
    CHECK(sck.find("HttpOnly") != std::string::npos);
    CHECK(sck.find("SameSite=Lax") != std::string::npos);

    // Zoom ?pwd= + SharePoint + idempotency
    std::string zoom = san.SanitizeUrl("https://zoom.us/j/123456789?pwd=SECRET_PASSCODE&foo=bar");
    CHECK(zoom.find("SECRET_PASSCODE") == std::string::npos);
    CHECK(zoom.find("foo=bar") != std::string::npos);
    CHECK(san.SanitizeUrl(zoom) == zoom);

    std::string sp = san.SanitizeUrl("https://contoso.sharepoint.com/:w:/g/personal/user_example/Ev12345");
    CHECK(sp.find("user_example") == std::string::npos);
    CHECK(sp.find("[REDACTED_SHARE_") != std::string::npos);
    CHECK(san.SanitizeUrl(sp) == sp);

    std::string od = san.SanitizeUrl("https://1drv.ms/b/s!AkFakeShare");
    CHECK(od.find("AkFakeShare") == std::string::npos);
    CHECK(san.SanitizeUrl(od) == od);

    // HTML / DOM / <script>
    std::string html =
        "<form><input type=\"password\" value=\"FAKE_INPUT_PWD\">"
        "<input name=\"retoken\" value=\"FAKE_RETOKEN\">"
        "<meta name=\"csrf-token\" content=\"FAKE_CSRF\">"
        "<textarea name=\"csrf\">FAKE_TA</textarea>"
        "<script>const csrf_token=\"FAKE_JS\";document.cookie=\"ad_session_id=FAKE_CK\";</script></form>";
    std::string shtml = san.SanitizeHtml(html);
    CHECK(shtml.find("FAKE_INPUT_PWD") == std::string::npos);
    CHECK(shtml.find("FAKE_RETOKEN") == std::string::npos);
    CHECK(shtml.find("FAKE_CSRF") == std::string::npos);
    CHECK(shtml.find("FAKE_TA") == std::string::npos);
    CHECK(shtml.find("FAKE_JS") == std::string::npos);
    CHECK(shtml.find("FAKE_CK") == std::string::npos);
    CHECK(san.SanitizeHtml(shtml) == shtml);

    // JSON & Form-urlencoded & PII
    std::string j_in = R"({"password":"Fake$Pwd\"2026","firstName":"Nombre Ejemplo","emailCol":"test@example.invalid","course":"EL2113"})";
    std::string j_out = san.SanitizeJsonString(j_in);
    CHECK(j_out.find("Fake$Pwd") == std::string::npos);
    CHECK(j_out.find("Nombre Ejemplo") == std::string::npos);
    CHECK(j_out.find("test@example.invalid") == std::string::npos);
    CHECK(j_out.find("EL2113") != std::string::npos);

    std::string f_out = san.SanitizeFormUrlEncoded("email=test@example.invalid&password=FAKE_PWD&view=list");
    CHECK(f_out.find("FAKE_PWD") == std::string::npos);
    CHECK(f_out.find("test@example.invalid") == std::string::npos);
    CHECK(f_out.find("view=list") != std::string::npos);

    // Split-chunk 1MB + 4096B boundary token scan
    std::string big(1024 * 1024 - 10, ' ');
    big += "Bearer FAKE_SPLIT_TOKEN_ACROSS_CHUNK_BOUNDARY";
    big += std::string(100, ' ');
    auto cats = san.ScanReusableTokens(big);
    CHECK(!cats.empty() && cats[0] == "bearer_like_value");

    // Redacted placeholder is ignored by reusable token scanner
    CHECK(san.ScanReusableTokens("Bearer [REDACTED] ad_session_id=[REDACTED:sha256=12345678]").empty());

    // Runtime needle detection across URL-encoded & JSON-escaped variants
    CHECK(san.ScanForLeaks("pwd=Fake%24Pwd%222026").has_password_leak);
    CHECK(san.ScanForLeaks("{\"p\":\"Fake$Pwd\\\"2026\"}").has_password_leak);
    CHECK(san.ScanForLeaks("user=TEST%40EXAMPLE.INVALID").has_email_leak);
    CHECK(san.ScanForLeaks("student Nombre Ejemplo").has_pii_leak);

    // Private originals mode: allows email, still fails on password
    SanitizeOptions priv_opts = opts;
    priv_opts.allow_personal_email = true;
    Sanitizer priv_san(priv_opts);
    auto priv_email = priv_san.ScanForLeaks("owner test@example.invalid");
    CHECK(priv_email.passed && priv_email.personal_data_detected && !priv_email.has_email_leak);
    CHECK(!priv_san.ScanForLeaks("pwd Fake$Pwd\"2026").passed);

    // BUG-069 directory named trampa.html/ does not crash directory scan
    ScopedTempDir tmp("sanitizer");
    fs::create_directories(tmp.dir / "pages" / "trampa.html");
    {
        std::ofstream ofs(tmp.dir / "pages" / "trampa.html" / "clean.html");
        ofs << "<html><body>Clean page</body></html>";
    }
    CHECK(san.ScanReusableTokensFile((tmp.dir / "pages" / "trampa.html").string()).empty());
    CHECK(san.ScanDirectoryForLeaks(tmp.dir.string()).passed);

    {
        std::ofstream ofs(tmp.dir / "pages" / "leak.txt");
        ofs << "leaked Fake$Pwd\"2026 here";
    }
    CHECK(san.ScanDirectoryForLeaks(tmp.dir.string()).has_password_leak);

    // SafeDiagnosticPath scrubs needles (including uppercase & URL-encoded email)
    std::string dpath = san.SafeDiagnosticPath("/tmp/test@example.invalid/Fake$Pwd\"2026/log.txt");
    CHECK(dpath.find("test@example.invalid") == std::string::npos);
    CHECK(dpath.find("Fake$Pwd") == std::string::npos);

    SanitizeOptions up_opts;
    up_opts.runtime_email = "student.user@example.invalid";
    Sanitizer up_san(up_opts);
    std::string up_dpath = up_san.SafeDiagnosticPath("C:/logs/STUDENT.USER@EXAMPLE.INVALID/out.txt");
    CHECK(up_dpath.find("STUDENT.USER@EXAMPLE.INVALID") == std::string::npos);
    CHECK(up_dpath.find("[REDACTED_EMAIL]") != std::string::npos);

    // Item 5: SanitizeText ordering with whitespace/quote/backslash runtime_password (zero partial suffix leak)
    {
        SanitizeOptions sp_opts;
        sp_opts.runtime_password = "my secret pass";
        sp_opts.runtime_email = "student.user@example.invalid";
        Sanitizer sp_san(sp_opts);
        std::string st = sp_san.SanitizeText("password=my secret pass");
        CHECK(st == "password=[REDACTED]");
        CHECK(st.find("secret pass") == std::string::npos);
        CHECK(sp_san.ScanForLeaks(st).passed);

        // + form-space variant in ScanForLeaks, SanitizeText, SafeDiagnosticPath
        CHECK(sp_san.ScanForLeaks("password=my+secret+pass").has_password_leak);
        CHECK(sp_san.SanitizeText("body=my+secret+pass").find("my+secret+pass") == std::string::npos);
        CHECK(sp_san.SafeDiagnosticPath("/tmp/my+secret+pass/a").find("my+secret+pass") == std::string::npos);

        // Sample truncation honors max_sample_chars while SanitizeText stays untruncated
        SanitizeOptions samp_opts = sp_opts;
        samp_opts.max_sample_chars = 24;
        Sanitizer samp_san(samp_opts);
        std::string long_in = "password=my secret pass and extra_tail_0123456789abcdef";
        std::string full_out = samp_san.SanitizeText(long_in);
        std::string samp_out = samp_san.SanitizeSample(long_in);
        CHECK(full_out.size() > 24);
        CHECK(samp_out.size() == 24);
        CHECK(samp_san.ScanForLeaks(samp_out).passed);
    }
    {
        SanitizeOptions q_opts;
        q_opts.runtime_password = R"(p@ss"w\ord#1)";
        Sanitizer q_san(q_opts);
        std::string st = q_san.SanitizeText(R"(password=p@ss"w\ord#1)");
        CHECK(st == "password=[REDACTED]");
        CHECK(st.find(R"(w\ord#1)") == std::string::npos);
        CHECK(q_san.ScanForLeaks(st).passed);
    }

    // Item 5: Header preservation & quoted multi-word secrets in SanitizeText
    {
        std::string sc = san.SanitizeText(
            "Set-Cookie: ad_session_id=FAKE_SESSION_98765432; Path=/; HttpOnly");
        CHECK(sc.find("ad_session_id=[REDACTED:sha256=") != std::string::npos);
        CHECK(sc.find("Path=/; HttpOnly") != std::string::npos);

        std::string ck_hdr = san.SanitizeText(
            "Cookie: language=es; ad_session_id=FAKE_SESSION_98765432");
        CHECK(ck_hdr.find("language=[REDACTED:sha256=") != std::string::npos);
        CHECK(ck_hdr.find("ad_session_id=[REDACTED:sha256=") != std::string::npos);

        std::string auth_hdr = san.SanitizeText(
            "Authorization: Bearer FAKE_BEARER_TOKEN_123456");
        CHECK(auth_hdr.find("Authorization: Bearer [REDACTED:sha256=") != std::string::npos);

        std::string qsec = san.SanitizeText(R"(password="secret with spaces")");
        CHECK(qsec == R"(password="[REDACTED]")");
    }

    // Item 7: Numeric PII JSON keys redacted under shareable_mode
    {
        std::string j_num = san.SanitizeJsonString(
            R"({"carneOficial":2026123456,"studentId":998877,"course":"EL2113"})");
        CHECK(j_num.find("2026123456") == std::string::npos);
        CHECK(j_num.find("998877") == std::string::npos);
        CHECK(j_num.find("[REDACTED_PII]") != std::string::npos);
        CHECK(j_num.find("EL2113") != std::string::npos);
    }

    // Item 10: SanitizeUrl redacts secrets inside #fragment
    {
        std::string u1 = san.SanitizeUrl("https://zoom.us/j/123#/join?pwd=SECRET");
        CHECK(u1.find("SECRET") == std::string::npos);
        CHECK(san.SanitizeUrl(u1) == u1);

        std::string u2 = san.SanitizeUrl(
            "https://tecdigital.tec.ac.cr/cb#access_token=MY_OAUTH_FRAGMENT_SECRET");
        CHECK(u2.find("MY_OAUTH_FRAGMENT_SECRET") == std::string::npos);
        CHECK(san.SanitizeUrl(u2) == u2);
    }
}

void TestLoopbackNativeTransport() {
    // Valid RFC 1952 gzip frame for "Hello Gzip TEC Digital!"
    static constexpr uint8_t kGzipPayload[] = {
        0x1f, 0x8b, 0x08, 0x00, 0xcb, 0xea, 0xfc, 0x6a, 0x02, 0xff, 0xf3,
        0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x70, 0xaf, 0xca, 0x2c, 0x50, 0x08,
        0x71, 0x75, 0x56, 0x70, 0xc9, 0x4c, 0xcf, 0x2c, 0x49, 0xcc, 0x51,
        0x04, 0x00, 0xf8, 0x39, 0xcf, 0xed, 0x17, 0x00, 0x00, 0x00
    };

    std::atomic<bool> saw_range_zero{false};
    std::atomic<bool> saw_range_10{false};
    std::atomic<bool> saw_cookie_on_dotlrn{false};

    LoopbackHttpServer server([&](const std::string& method,
                                  const std::string& target,
                                  const std::string& headers_lower,
                                  const std::string& body) -> std::string {
        if (headers_lower.find("range: bytes=0-") != std::string::npos) {
            saw_range_zero.store(true);
        }

        if (method == "GET" && target == "/") {
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Length: 2\r\n"
                   "Set-Cookie: AlteonP=lb_real; Path=/\r\n"
                   "Connection: close\r\n\r\nOK";
        }
        if (method == "POST" && target == "/api/login/new-form/") {
            auto j = nlohmann::json::parse(body);
            if (j["email"] == "test@example.invalid" && j["password"] == "FAKE_LOOPBACK_PWD") {
                std::string json_out = R"({"status":"ok","returnUrl":"/dotlrn/index"})";
                return "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(json_out.size()) + "\r\n"
                       "Set-Cookie: ad_user_login=x; Expires=Fri, 01-Jan-1980 01:00:00 GMT; Path=/\r\n"
                       "Set-Cookie: ad_user_login=FAKE_LB_USER; Expires=Fri, 01-Jan-2035 01:00:00 GMT; Path=/; HttpOnly\r\n"
                       "Set-Cookie: ad_session_id=FAKE_LB_SID; Expires=Fri, 01-Jan-2035 01:00:00 GMT; Path=/; Discard; HttpOnly\r\n"
                       "Connection: close\r\n\r\n" + json_out;
            }
            std::string bad = R"({"status":"bad_password"})";
            return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(bad.size()) +
                   "\r\nConnection: close\r\n\r\n" + bad;
        }
        if (method == "GET" && target == "/dotlrn/") {
            if (headers_lower.find("ad_session_id=fake_lb_sid") != std::string::npos &&
                headers_lower.find("alteonp=lb_real") != std::string::npos) {
                saw_cookie_on_dotlrn.store(true);
            }
            std::string html = "<html><body>Portal OK <a href=\"/register/logout\">Exit</a></body></html>";
            return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
                   std::to_string(html.size()) + "\r\nConnection: close\r\n\r\n" + html;
        }
        if (method == "GET" && target == "/range-check") {
            if (headers_lower.find("range: bytes=10-") != std::string::npos) {
                saw_range_10.store(true);
            }
            return "HTTP/1.1 206 Partial Content\r\n"
                   "Content-Range: bytes 10-14/15\r\n"
                   "Content-Length: 5\r\n"
                   "Connection: close\r\n\r\n12345";
        }
        if (method == "GET" && target == "/redirect-register") {
            return "HTTP/1.1 302 Found\r\n"
                   "Location: /register/?return_url=/dotlrn/\r\n"
                   "Content-Length: 0\r\nConnection: close\r\n\r\n";
        }
        if (method == "GET" && target == "/redirect-evil") {
            return "HTTP/1.1 302 Found\r\n"
                   "Location: https://evil.example/steal\r\n"
                   "Content-Length: 0\r\nConnection: close\r\n\r\n";
        }
        if (method == "GET" && target == "/gzip-test") {
            std::string resp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Encoding: gzip\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(sizeof(kGzipPayload)) + "\r\n"
                "Connection: close\r\n\r\n";
            resp.append(reinterpret_cast<const char*>(kGzipPayload), sizeof(kGzipPayload));
            return resp;
        }
        if (method == "GET" && target == "/gzip-truncated-eof") {
            std::string resp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Encoding: gzip\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n\r\n";
            resp.append(reinterpret_cast<const char*>(kGzipPayload), 16);
            return resp;
        }
        if (method == "GET" && target == "/chunked-overflow") {
            return "HTTP/1.1 200 OK\r\n"
                   "Transfer-Encoding: chunked\r\n"
                   "Connection: close\r\n\r\n"
                   "fffffffffffffffe\r\nabc\r\n0\r\n\r\n";
        }
        if (method == "GET" && target == "/chunked-trailer") {
            return "HTTP/1.1 200 OK\r\n"
                   "Transfer-Encoding: chunked\r\n"
                   "Connection: close\r\n\r\n"
                   "3;ext=1\r\nabc\r\n0\r\nX-Trailer: done\r\n\r\n";
        }
        if (method == "GET" && target == "/redirect-query-scheme") {
            return "HTTP/1.1 302 Found\r\n"
                   "Location: /target?next=https://evil.example/a\r\n"
                   "Content-Length: 0\r\nConnection: close\r\n\r\n";
        }
        if (method == "GET" && target == "/target?next=https://evil.example/a") {
            return "HTTP/1.1 200 OK\r\nContent-Length: 9\r\nConnection: close\r\n\r\ntarget-ok";
        }
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    });

    CHECK(server.Start());
    auto native = CreateNativeHttpTransport();
    CHECK(native != nullptr);

    // Native transport refuses plaintext http:// on non-loopback host without network I/O
    {
        HttpRequest bad_http;
        bad_http.url = "http://tecdigital.tec.ac.cr/dotlrn/";
        HttpResponse bad_resp = native->Execute(bad_http);
        CHECK(!bad_resp.transport_ok);
    }

    PolicyOptions popts;
    popts.allowed_test_origins = {server.BaseUrl()};
    ReadOnlyRequestPolicy policy(popts);

    AuthClientOptions aopts;
    aopts.base_url = server.BaseUrl();
    AuthClient client(*native, policy, aopts);

    AuthResult login_ok = client.LoginWithPassword("TEST@EXAMPLE.INVALID", "FAKE_LOOPBACK_PWD");
    CHECK(login_ok.ok);
    CHECK(login_ok.status == AuthStatus::Ok);
    CHECK(saw_cookie_on_dotlrn.load());
    CHECK(!saw_range_zero.load());

    // Range resume header check
    {
        HttpRequest rreq;
        rreq.url = server.BaseUrl() + "/range-check";
        rreq.range_start = 10;
        HttpResponse rresp = native->Execute(rreq);
        CHECK(rresp.transport_ok && rresp.status_code == 206);
        CHECK(rresp.BodyAsString() == "12345");
        CHECK(saw_range_10.load());
    }

    // 302 /register/ detection over real loopback wire
    {
        HttpRequest rreq;
        rreq.url = server.BaseUrl() + "/redirect-register";
        rreq.follow_redirects = false;
        HttpResponse rresp = native->Execute(rreq);
        CHECK(rresp.transport_ok && rresp.status_code == 302);
        CHECK(client.ValidateResponse(rresp, rreq.url) == ResponseValidity::SessionExpired);
        CHECK(client.IsSessionExpired());
    }

    // Cross-origin 302 blocked when follow_redirects = true
    {
        HttpRequest rreq;
        rreq.url = server.BaseUrl() + "/redirect-evil";
        rreq.follow_redirects = true;
        HttpResponse rresp = native->Execute(rreq);
        CHECK(!rresp.transport_ok);
        CHECK(rresp.error_message.find("OffOriginRedirect") != std::string::npos);
    }

    // Gzip response decompression over real wire (both default and explicit Accept-Encoding: gzip)
    {
        HttpRequest greq;
        greq.url = server.BaseUrl() + "/gzip-test";
        HttpResponse gresp = native->Execute(greq);
        CHECK(gresp.transport_ok && gresp.status_code == 200);
        CHECK(gresp.BodyAsString() == "Hello Gzip TEC Digital!");

        HttpRequest greq_explicit;
        greq_explicit.url = server.BaseUrl() + "/gzip-test";
        greq_explicit.headers["Accept-Encoding"] = "gzip";
        HttpResponse gresp2 = native->Execute(greq_explicit);
        CHECK(gresp2.transport_ok && gresp2.status_code == 200);
        CHECK(gresp2.BodyAsString() == "Hello Gzip TEC Digital!");
    }

    // Item 1: Malicious chunked overflow fails cleanly without crash; valid chunked + trailer succeeds
    {
        HttpRequest creq;
        creq.url = server.BaseUrl() + "/chunked-overflow";
        HttpResponse cresp = native->Execute(creq);
        CHECK(!cresp.transport_ok);

        HttpRequest treq;
        treq.url = server.BaseUrl() + "/chunked-trailer";
        HttpResponse tresp = native->Execute(treq);
        CHECK(tresp.transport_ok && tresp.status_code == 200);
        CHECK(tresp.BodyAsString() == "abc");
    }

#ifndef _WIN32
    // Item 3: Truncated gzip EOF fails closed
    {
        HttpRequest tgreq;
        tgreq.url = server.BaseUrl() + "/gzip-truncated-eof";
        HttpResponse tgresp = native->Execute(tgreq);
        CHECK(!tgresp.transport_ok);
        CHECK(!tgresp.error_message.empty());
    }
#endif

    // Item 8: Relative Location containing :// in query follows same-origin cleanly
    {
        HttpRequest rreq;
        rreq.url = server.BaseUrl() + "/redirect-query-scheme";
        rreq.follow_redirects = true;
        HttpResponse rresp = native->Execute(rreq);
        CHECK(rresp.transport_ok && rresp.status_code == 200);
        CHECK(rresp.BodyAsString() == "target-ok");
    }

    server.Stop();
}

#ifndef _WIN32
void TestLoopbackTlsLinux() {
    // Ephemeral in-memory EC P-256 key & self-signed X.509 cert (zero temp files)
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    CHECK(pctx != nullptr);
    EVP_PKEY* pkey = nullptr;
    CHECK(EVP_PKEY_keygen_init(pctx) == 1);
    CHECK(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) == 1);
    CHECK(EVP_PKEY_keygen(pctx, &pkey) == 1);
    EVP_PKEY_CTX_free(pctx);

    X509* cert = X509_new();
    CHECK(cert != nullptr);
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_getm_notBefore(cert), -60);
    X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
    X509_set_pubkey(cert, pkey);
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>("127.0.0.1"), -1, -1, 0);
    X509_set_issuer_name(cert, name);
    X509V3_CTX v3ctx;
    X509V3_set_ctx_nodb(&v3ctx);
    X509V3_set_ctx(&v3ctx, cert, cert, nullptr, nullptr, 0);
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &v3ctx, NID_subject_alt_name,
                                              const_cast<char*>("IP:127.0.0.1"));
    if (ext != nullptr) {
        X509_add_ext(cert, ext, -1);
        X509_EXTENSION_free(ext);
    }
    CHECK(X509_sign(cert, pkey, EVP_sha256()) > 0);

    SSL_CTX* srv_ctx = SSL_CTX_new(TLS_server_method());
    CHECK(srv_ctx != nullptr);
    CHECK(SSL_CTX_use_certificate(srv_ctx, cert) == 1);
    CHECK(SSL_CTX_use_PrivateKey(srv_ctx, pkey) == 1);

    int lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    CHECK(lfd >= 0);
    int yes = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    CHECK(::bind(lfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    socklen_t alen = sizeof(addr);
    CHECK(::getsockname(lfd, reinterpret_cast<sockaddr*>(&addr), &alen) == 0);
    uint16_t port = ntohs(addr.sin_port);
    CHECK(::listen(lfd, 4) == 0);

    std::atomic<bool> stop_srv{false};
    std::thread srv_thread([&]() {
        while (!stop_srv.load()) {
            int cfd = ::accept(lfd, nullptr, nullptr);
            if (cfd < 0) break;
            SSL* ssl = SSL_new(srv_ctx);
            SSL_set_fd(ssl, cfd);
            if (SSL_accept(ssl) == 1) {
                char buf[2048];
                SSL_read(ssl, buf, sizeof(buf));
                const char reply[] =
                    "HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\ntls-ok";
                SSL_write(ssl, reply, static_cast<int>(sizeof(reply) - 1));
                SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            ::close(cfd);
        }
    });

    auto native = CreateNativeHttpTransport();
    std::string url = "https://127.0.0.1:" + std::to_string(port) + "/ping";

    // Untrusted self-signed cert rejected when allow_insecure_tls = false
    HttpRequest strict_req;
    strict_req.url = url;
    strict_req.allow_insecure_tls = false;
    HttpResponse strict_resp = native->Execute(strict_req);
    CHECK(!strict_resp.transport_ok);
    CHECK(!strict_resp.tls_verified);
    CHECK(!strict_resp.error_message.empty());

    // Succeeds when allow_insecure_tls = true (with tls_verified == false)
    HttpRequest ins_req;
    ins_req.url = url;
    ins_req.allow_insecure_tls = true;
    HttpResponse ins_resp = native->Execute(ins_req);
    CHECK(ins_resp.transport_ok);
    CHECK(ins_resp.status_code == 200);
    CHECK(!ins_resp.tls_verified);
    CHECK(ins_resp.BodyAsString() == "tls-ok");

    stop_srv.store(true);
    ::shutdown(lfd, SHUT_RDWR);
    ::close(lfd);
    srv_thread.join();

    SSL_CTX_free(srv_ctx);
    X509_free(cert);
    EVP_PKEY_free(pkey);
}
#endif

}  // namespace

int main() {
    TestCrypto();
    TestCookieJar();
    TestAuthClientMocked();
    TestVault();
    TestReadOnlyPolicy();
    TestCanonicalizer();
    TestSanitizer();
    TestLoopbackNativeTransport();
#ifndef _WIN32
    TestLoopbackTlsLinux();
#endif

    std::cout << "TecDigitalFoundationTest: " << gPasses << " passed, " << gFailures << " failed.\n";
    return (gFailures == 0) ? 0 : 1;
}
