#include "academic/tec_digital/http_transport.hpp"
#include "http_common.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace academic::tec_digital {

namespace {

int RemainingTimeoutMs(std::chrono::steady_clock::time_point deadline) {
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return 0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    return static_cast<int>(std::max<int64_t>(1, ms));
}

bool PollFd(int fd, short events, std::chrono::steady_clock::time_point deadline,
            const std::function<bool()>& cancel_check, std::string& out_err) {
    while (true) {
        if (cancel_check && cancel_check()) {
            out_err = "cancelled";
            return false;
        }
        int rem = RemainingTimeoutMs(deadline);
        if (rem <= 0) {
            out_err = "socket timeout";
            return false;
        }
        int slice = cancel_check ? std::min(rem, 200) : rem;
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = events;
        int rc = ::poll(&pfd, 1, slice);
        if (rc > 0) {
            if (pfd.revents & (POLLERR | POLLNVAL)) {
                out_err = "socket poll error";
                return false;
            }
            return true;
        }
        if (rc < 0) {
            if (errno == EINTR) continue;
            out_err = std::string("poll failed: ") + std::strerror(errno);
            return false;
        }
        // rc == 0: check deadline on next iteration
    }
}

struct TransportStream {
    int fd = -1;
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    bool use_tls = false;

    ~TransportStream() {
        if (ssl != nullptr) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        if (ctx != nullptr) {
            SSL_CTX_free(ctx);
        }
        if (fd >= 0) {
            ::close(fd);
        }
    }

    bool WriteAll(const uint8_t* data, size_t len, int timeout_ms,
                  const std::function<bool()>& cancel_check, std::string& out_err) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        size_t sent = 0;
        while (sent < len) {
            if (cancel_check && cancel_check()) {
                out_err = "cancelled";
                return false;
            }
            if (use_tls) {
                int n = SSL_write(ssl, data + sent, static_cast<int>(len - sent));
                if (n > 0) {
                    sent += static_cast<size_t>(n);
                    continue;
                }
                int ssl_err = SSL_get_error(ssl, n);
                if (ssl_err == SSL_ERROR_WANT_WRITE) {
                    if (!PollFd(fd, POLLOUT, deadline, cancel_check, out_err)) return false;
                    continue;
                }
                if (ssl_err == SSL_ERROR_WANT_READ) {
                    if (!PollFd(fd, POLLIN, deadline, cancel_check, out_err)) return false;
                    continue;
                }
                out_err = "SSL_write failed";
                return false;
            } else {
                ssize_t n = ::send(fd, data + sent, len - sent, MSG_NOSIGNAL);
                if (n > 0) {
                    sent += static_cast<size_t>(n);
                    continue;
                }
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                    if (!PollFd(fd, POLLOUT, deadline, cancel_check, out_err)) return false;
                    continue;
                }
                out_err = std::string("send failed: ") + std::strerror(errno);
                return false;
            }
        }
        return true;
    }

    // Returns >0 bytes read, 0 on clean EOF, -1 on error
    ssize_t ReadSome(uint8_t* buf, size_t cap, int timeout_ms,
                     const std::function<bool()>& cancel_check, std::string& out_err) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (true) {
            if (cancel_check && cancel_check()) {
                out_err = "cancelled";
                return -1;
            }
            if (use_tls) {
                int n = SSL_read(ssl, buf, static_cast<int>(cap));
                if (n > 0) return n;
                int ssl_err = SSL_get_error(ssl, n);
                if (ssl_err == SSL_ERROR_ZERO_RETURN) return 0;
                if (ssl_err == SSL_ERROR_WANT_READ) {
                    if (!PollFd(fd, POLLIN, deadline, cancel_check, out_err)) return -1;
                    continue;
                }
                if (ssl_err == SSL_ERROR_WANT_WRITE) {
                    if (!PollFd(fd, POLLOUT, deadline, cancel_check, out_err)) return -1;
                    continue;
                }
                if (n == 0 && ERR_peek_error() == 0) return 0;
                out_err = "SSL_read failed";
                return -1;
            } else {
                ssize_t n = ::recv(fd, buf, cap, 0);
                if (n > 0) return n;
                if (n == 0) return 0;
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    if (!PollFd(fd, POLLIN, deadline, cancel_check, out_err)) return -1;
                    continue;
                }
                out_err = std::string("recv failed: ") + std::strerror(errno);
                return -1;
            }
        }
    }
};

bool ConnectSocket(const std::string& host, uint16_t port, int connect_timeout_ms,
                   const std::function<bool()>& cancel_check, int& out_fd, std::string& out_err) {
    out_fd = -1;
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    struct addrinfo* res = nullptr;
    int gai = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (gai != 0 || res == nullptr) {
        out_err = std::string("getaddrinfo failed: ") + gai_strerror(gai);
        return false;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(connect_timeout_ms);
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        int fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        int rc = ::connect(fd, p->ai_addr, p->ai_addrlen);
        if (rc == 0) {
            out_fd = fd;
            ::freeaddrinfo(res);
            return true;
        }
        if (errno == EINPROGRESS) {
            if (PollFd(fd, POLLOUT, deadline, cancel_check, out_err)) {
                int so_err = 0;
                socklen_t len = sizeof(so_err);
                if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &len) == 0 && so_err == 0) {
                    out_fd = fd;
                    ::freeaddrinfo(res);
                    return true;
                }
                out_err = std::string("connect failed: ") + std::strerror(so_err);
            }
        } else {
            out_err = std::string("connect failed: ") + std::strerror(errno);
        }
        ::close(fd);
    }

    ::freeaddrinfo(res);
    if (out_err.empty()) out_err = "failed to connect";
    return false;
}

bool SetupTls(TransportStream& stream, const std::string& host, bool allow_insecure,
              int connect_timeout_ms, const std::function<bool()>& cancel_check,
              std::string& out_err) {
    stream.use_tls = true;
    stream.ctx = SSL_CTX_new(TLS_client_method());
    if (stream.ctx == nullptr) {
        out_err = "SSL_CTX_new failed";
        return false;
    }
    SSL_CTX_set_min_proto_version(stream.ctx, TLS1_2_VERSION);

    if (!allow_insecure) {
        SSL_CTX_set_default_verify_paths(stream.ctx);
        SSL_CTX_set_verify(stream.ctx, SSL_VERIFY_PEER, nullptr);
    } else {
        SSL_CTX_set_verify(stream.ctx, SSL_VERIFY_NONE, nullptr);
    }

    stream.ssl = SSL_new(stream.ctx);
    if (stream.ssl == nullptr) {
        out_err = "SSL_new failed";
        return false;
    }
    SSL_set_fd(stream.ssl, stream.fd);
    SSL_set_tlsext_host_name(stream.ssl, host.c_str());

    if (!allow_insecure) {
        struct in_addr ipv4{};
        struct in6_addr ipv6{};
        if (::inet_pton(AF_INET, host.c_str(), &ipv4) == 1 ||
            ::inet_pton(AF_INET6, host.c_str(), &ipv6) == 1) {
            X509_VERIFY_PARAM* param = SSL_get0_param(stream.ssl);
            X509_VERIFY_PARAM_set1_ip_asc(param, host.c_str());
        } else {
            SSL_set1_host(stream.ssl, host.c_str());
        }
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(connect_timeout_ms);
    while (true) {
        if (cancel_check && cancel_check()) {
            out_err = "cancelled during TLS handshake";
            return false;
        }
        int rc = SSL_connect(stream.ssl);
        if (rc == 1) break;
        int ssl_err = SSL_get_error(stream.ssl, rc);
        if (ssl_err == SSL_ERROR_WANT_READ) {
            if (!PollFd(stream.fd, POLLIN, deadline, cancel_check, out_err)) return false;
            continue;
        }
        if (ssl_err == SSL_ERROR_WANT_WRITE) {
            if (!PollFd(stream.fd, POLLOUT, deadline, cancel_check, out_err)) return false;
            continue;
        }
        unsigned long err_code = ERR_get_error();
        char err_buf[256]{};
        ERR_error_string_n(err_code, err_buf, sizeof(err_buf));
        out_err = std::string("TLS handshake failed: ") + err_buf;
        return false;
    }

    if (!allow_insecure) {
        long vr = SSL_get_verify_result(stream.ssl);
        if (vr != X509_V_OK) {
            out_err = std::string("TLS certificate verification failed: ") +
                      X509_verify_cert_error_string(vr);
            return false;
        }
    }
    return true;
}

class GzipStreamDecoder {
public:
    ~GzipStreamDecoder() {
        if (initialized_) inflateEnd(&zs_);
    }

    bool Init(std::string& out_err, bool is_deflate = false) {
        is_deflate_ = is_deflate;
        raw_deflate_mode_ = false;
        emitted_any_output_ = false;
        member_boundary_ = false;
        initial_buf_overflowed_ = false;
        initial_buf_.clear();
        std::memset(&zs_, 0, sizeof(zs_));
        const int window_bits = is_deflate ? MAX_WBITS : (16 + MAX_WBITS);
        if (inflateInit2(&zs_, window_bits) != Z_OK) {
            out_err = "zlib inflateInit2 failed";
            return false;
        }
        initialized_ = true;
        return true;
    }

    bool Feed(const uint8_t* data, size_t len,
              const std::function<bool(const uint8_t*, size_t)>& sink,
              std::string& out_err) {
        if (len == 0) return true;
        if (is_deflate_ && member_boundary_) {
            return true;
        }
        if (!is_deflate_) {
            member_boundary_ = false;
        }

        if (is_deflate_ && !raw_deflate_mode_ && !emitted_any_output_ && !initial_buf_overflowed_) {
            constexpr size_t kMaxInitialBuf = 256ULL * 1024ULL;
            if (initial_buf_.size() + len <= kMaxInitialBuf) {
                initial_buf_.insert(initial_buf_.end(), data, data + len);
            } else {
                initial_buf_overflowed_ = true;
                std::vector<uint8_t>().swap(initial_buf_);
            }
        }

        zs_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
        zs_.avail_in = static_cast<uInt>(len);
        uint8_t out_buf[16384];
        while (zs_.avail_in > 0) {
            zs_.next_out = out_buf;
            zs_.avail_out = sizeof(out_buf);
            int rc = inflate(&zs_, Z_NO_FLUSH);
            if (rc == Z_DATA_ERROR && is_deflate_ && !raw_deflate_mode_ &&
                !emitted_any_output_ && !initial_buf_overflowed_) {
                inflateEnd(&zs_);
                initialized_ = false;
                std::memset(&zs_, 0, sizeof(zs_));
                if (inflateInit2(&zs_, -MAX_WBITS) != Z_OK) {
                    out_err = "zlib inflateInit2 raw deflate failed";
                    return false;
                }
                initialized_ = true;
                raw_deflate_mode_ = true;
                zs_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(initial_buf_.data()));
                zs_.avail_in = static_cast<uInt>(initial_buf_.size());
                continue;
            }
            if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) {
                out_err = "zlib inflate error";
                return false;
            }
            const size_t produced = sizeof(out_buf) - zs_.avail_out;
            if (produced > 0) {
                emitted_any_output_ = true;
                if (!sink(out_buf, produced)) {
                    out_err = "aborted by chunk_callback";
                    return false;
                }
            }
            if (rc == Z_STREAM_END) {
                member_boundary_ = true;
                if (!is_deflate_) {
                    if (inflateReset(&zs_) != Z_OK) {
                        out_err = "zlib inflateReset failed";
                        return false;
                    }
                    if (zs_.avail_in > 0) {
                        member_boundary_ = false;
                        continue;
                    }
                }
                break;
            }
            if (rc == Z_BUF_ERROR && produced == 0) {
                break;
            }
        }
        if ((emitted_any_output_ || member_boundary_ || raw_deflate_mode_) && !initial_buf_.empty()) {
            std::vector<uint8_t>().swap(initial_buf_);
        }
        return true;
    }

    bool Finish() const { return member_boundary_; }

private:
    z_stream zs_{};
    bool initialized_ = false;
    bool is_deflate_ = false;
    bool raw_deflate_mode_ = false;
    bool emitted_any_output_ = false;
    bool member_boundary_ = false;
    bool initial_buf_overflowed_ = false;
    std::vector<uint8_t> initial_buf_;
};

HttpResponse ExecuteSingleLinuxHttp(const HttpRequest& req) {
    HttpResponse resp;
    resp.final_url = req.url;

    if (req.cancel_check && req.cancel_check()) {
        resp.error_message = "cancelled before request";
        return resp;
    }

    ParsedUrl parsed = ParseHttpUrl(req.url);
    if (!parsed.valid) {
        resp.error_message = "invalid URL";
        return resp;
    }

    const bool is_https = (parsed.scheme == "https");
    if (!is_https && !IsLoopbackHost(parsed.host)) {
        resp.error_message = "plaintext HTTP refused for non-loopback host";
        return resp;
    }

    TransportStream stream;
    if (!ConnectSocket(parsed.host, parsed.port, req.connect_timeout_ms,
                       req.cancel_check, stream.fd, resp.error_message)) {
        return resp;
    }

    if (is_https) {
        if (!SetupTls(stream, parsed.host, req.allow_insecure_tls, req.connect_timeout_ms,
                      req.cancel_check, resp.error_message)) {
            return resp;
        }
    }

    std::string method_upper = req.method.empty() ? "GET" : req.method;
    for (char& c : method_upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    const std::string range_val = FormatRangeHeader(req.range_start, req.range_end);

    std::ostringstream oss;
    oss << method_upper << " " << parsed.target << " HTTP/1.1\r\n";
    oss << "Host: " << parsed.host_port << "\r\n";
    oss << "Connection: close\r\n";

    bool has_ua = false;
    bool has_accept_encoding = false;
    bool has_content_length = false;

    for (const auto& kv : req.headers) {
        std::string low = AsciiLower(kv.first);
        if (low == "host" || low == "connection") continue;
        if (low == "range" && TrimAscii(kv.second) == "bytes=0-") continue;
        if (!range_val.empty() && low == "range") continue;
        if (low == "user-agent") has_ua = true;
        if (low == "accept-encoding") has_accept_encoding = true;
        if (low == "content-length") has_content_length = true;
        oss << kv.first << ": " << kv.second << "\r\n";
    }

    if (!has_ua) {
        oss << "User-Agent: CalculadoraUnificada-TecDigital/1.0\r\n";
    }
    if (!range_val.empty()) {
        oss << "Range: " << range_val << "\r\n";
        if (!has_accept_encoding) {
            oss << "Accept-Encoding: identity\r\n";
        }
    } else if (!has_accept_encoding) {
        oss << "Accept-Encoding: gzip\r\n";
    }
    if (!req.body.empty() && !has_content_length) {
        oss << "Content-Length: " << req.body.size() << "\r\n";
    }
    oss << "\r\n";

    std::string hdr_str = oss.str();
    if (!stream.WriteAll(reinterpret_cast<const uint8_t*>(hdr_str.data()), hdr_str.size(),
                         req.send_timeout_ms, req.cancel_check, resp.error_message)) {
        return resp;
    }
    if (!req.body.empty()) {
        if (!stream.WriteAll(reinterpret_cast<const uint8_t*>(req.body.data()), req.body.size(),
                             req.send_timeout_ms, req.cancel_check, resp.error_message)) {
            return resp;
        }
    }

    // Read headers
    std::string recv_buf;
    size_t header_end = std::string::npos;
    uint8_t tmp[8192];
    while ((header_end = recv_buf.find("\r\n\r\n")) == std::string::npos) {
        if (recv_buf.size() > 256 * 1024) {
            resp.error_message = "HTTP response header too large";
            return resp;
        }
        ssize_t got = stream.ReadSome(tmp, sizeof(tmp), req.receive_timeout_ms,
                                      req.cancel_check, resp.error_message);
        if (got < 0) return resp;
        if (got == 0) {
            resp.error_message = "EOF before HTTP headers complete";
            return resp;
        }
        recv_buf.append(reinterpret_cast<const char*>(tmp), static_cast<size_t>(got));
    }

    std::string raw_headers = recv_buf.substr(0, header_end);
    std::string leftover = recv_buf.substr(header_end + 4);

    bool is_chunked = false;
    bool is_gzip = false;
    bool is_deflate = false;

    std::istringstream hiss(raw_headers);
    std::string line;
    if (!std::getline(hiss, line)) {
        resp.error_message = "empty HTTP status line";
        return resp;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    {
        size_t sp1 = line.find(' ');
        if (sp1 == std::string::npos) {
            resp.error_message = "malformed HTTP status line";
            return resp;
        }
        std::string rest = TrimAscii(line.substr(sp1 + 1));
        size_t sp2 = rest.find(' ');
        std::string code_str = (sp2 == std::string::npos) ? rest : rest.substr(0, sp2);
        resp.status_code = std::atoi(code_str.c_str());
    }

    while (std::getline(hiss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = TrimAscii(line.substr(0, colon));
        std::string val = TrimAscii(line.substr(colon + 1));
        std::string low = AsciiLower(name);
        resp.headers.emplace_back(name, val);
        if (low == "set-cookie") {
            resp.set_cookies.push_back(val);
        } else if (low == "content-type") {
            resp.content_type = val;
        } else if (low == "location") {
            resp.location = val;
        } else if (low == "content-range") {
            resp.content_range = val;
        } else if (low == "content-length") {
            char* endptr = nullptr;
            unsigned long long cl = std::strtoull(val.c_str(), &endptr, 10);
            if (endptr != val.c_str()) {
                resp.content_length = static_cast<uint64_t>(cl);
                resp.has_content_length = true;
            }
        } else if (low == "transfer-encoding") {
            if (AsciiLower(val).find("chunked") != std::string::npos) {
                is_chunked = true;
            }
        } else if (low == "content-encoding") {
            std::string ce_low = AsciiLower(val);
            if (ce_low.find("gzip") != std::string::npos) {
                is_gzip = true;
            } else if (ce_low.find("deflate") != std::string::npos) {
                is_deflate = true;
            }
        }
    }

    if (req.header_callback) {
        if (!req.header_callback(resp)) {
            resp.error_message = "aborted by header_callback";
            return resp;
        }
    }

    auto emit_plain = [&](const uint8_t* ptr, size_t len) -> bool {
        if (len == 0) return true;
        if (req.chunk_callback) {
            if (!req.chunk_callback(ptr, len)) {
                resp.error_message = "aborted by chunk_callback";
                return false;
            }
        } else {
            resp.body.insert(resp.body.end(), ptr, ptr + len);
        }
        return true;
    };

    GzipStreamDecoder gz;
    const bool is_compressed = is_gzip || is_deflate;
    if (is_compressed) {
        if (!gz.Init(resp.error_message, is_deflate)) return resp;
    }

    bool fed_gzip_bytes = false;
    auto emit_wire = [&](const uint8_t* ptr, size_t len) -> bool {
        if (len == 0) return true;
        if (is_compressed) {
            fed_gzip_bytes = true;
            return gz.Feed(ptr, len, emit_plain, resp.error_message);
        }
        return emit_plain(ptr, len);
    };

    const bool no_body =
        (method_upper == "HEAD" || resp.status_code == 204 || resp.status_code == 304 ||
         (resp.status_code >= 100 && resp.status_code < 200));

    if (!no_body) {
        if (is_chunked) {
            constexpr unsigned long long kMaxChunkBytes = 256ULL * 1024ULL * 1024ULL;
            std::string cbuf = std::move(leftover);
            auto ensure_bytes = [&](size_t need) -> bool {
                while (cbuf.size() < need) {
                    ssize_t got = stream.ReadSome(tmp, sizeof(tmp), req.receive_timeout_ms,
                                                  req.cancel_check, resp.error_message);
                    if (got < 0) return false;
                    if (got == 0) {
                        resp.error_message = "unexpected EOF in chunked body";
                        return false;
                    }
                    cbuf.append(reinterpret_cast<const char*>(tmp), static_cast<size_t>(got));
                }
                return true;
            };
            while (true) {
                size_t crlf = std::string::npos;
                while ((crlf = cbuf.find("\r\n")) == std::string::npos) {
                    if (cbuf.size() > 64 * 1024) {
                        resp.error_message = "chunk header line too large";
                        return resp;
                    }
                    ssize_t got = stream.ReadSome(tmp, sizeof(tmp), req.receive_timeout_ms,
                                                  req.cancel_check, resp.error_message);
                    if (got < 0) return resp;
                    if (got == 0) {
                        resp.error_message = "unexpected EOF reading chunk header";
                        return resp;
                    }
                    cbuf.append(reinterpret_cast<const char*>(tmp), static_cast<size_t>(got));
                }
                std::string size_line = cbuf.substr(0, crlf);
                cbuf.erase(0, crlf + 2);
                size_t semi = size_line.find(';');
                if (semi != std::string::npos) size_line = size_line.substr(0, semi);
                while (!size_line.empty() &&
                       (size_line.back() == ' ' || size_line.back() == '\t')) {
                    size_line.pop_back();
                }
                if (size_line.empty()) {
                    resp.error_message = "malformed chunk size";
                    return resp;
                }
                for (unsigned char ch : size_line) {
                    if (!((ch >= '0' && ch <= '9') ||
                          (ch >= 'a' && ch <= 'f') ||
                          (ch >= 'A' && ch <= 'F'))) {
                        resp.error_message = "invalid hex chunk size";
                        return resp;
                    }
                }
                errno = 0;
                char* endptr = nullptr;
                unsigned long long chunk_len = std::strtoull(size_line.c_str(), &endptr, 16);
                if (errno == ERANGE || endptr == size_line.c_str() || *endptr != '\0') {
                    resp.error_message = "invalid hex chunk size";
                    return resp;
                }
                if (chunk_len > kMaxChunkBytes ||
                    chunk_len > static_cast<unsigned long long>(std::numeric_limits<size_t>::max() - 2)) {
                    resp.error_message = "chunk size exceeds maximum limit";
                    return resp;
                }
                if (chunk_len == 0) {
                    while (true) {
                        size_t tcrlf = std::string::npos;
                        while ((tcrlf = cbuf.find("\r\n")) == std::string::npos) {
                            if (cbuf.size() > 64 * 1024) {
                                resp.error_message = "chunked trailer too large";
                                return resp;
                            }
                            ssize_t got = stream.ReadSome(tmp, sizeof(tmp), req.receive_timeout_ms,
                                                          req.cancel_check, resp.error_message);
                            if (got < 0) return resp;
                            if (got == 0) {
                                resp.error_message = "unexpected EOF in chunked trailers";
                                return resp;
                            }
                            cbuf.append(reinterpret_cast<const char*>(tmp), static_cast<size_t>(got));
                        }
                        if (tcrlf == 0) {
                            cbuf.erase(0, 2);
                            break;
                        }
                        cbuf.erase(0, tcrlf + 2);
                    }
                    break;
                }
                const size_t clen = static_cast<size_t>(chunk_len);
                if (!ensure_bytes(clen + 2)) {
                    return resp;
                }
                if (cbuf[clen] != '\r' || cbuf[clen + 1] != '\n') {
                    resp.error_message = "malformed chunk terminator";
                    return resp;
                }
                if (!emit_wire(reinterpret_cast<const uint8_t*>(cbuf.data()), clen)) {
                    return resp;
                }
                cbuf.erase(0, clen + 2);
            }
        } else if (resp.has_content_length) {
            uint64_t remaining = resp.content_length;
            if (!leftover.empty()) {
                size_t take = static_cast<size_t>(
                    std::min<uint64_t>(remaining, static_cast<uint64_t>(leftover.size())));
                if (!emit_wire(reinterpret_cast<const uint8_t*>(leftover.data()), take)) {
                    return resp;
                }
                remaining -= take;
            }
            while (remaining > 0) {
                size_t to_read = static_cast<size_t>(
                    std::min<uint64_t>(remaining, static_cast<uint64_t>(sizeof(tmp))));
                ssize_t got = stream.ReadSome(tmp, to_read, req.receive_timeout_ms,
                                              req.cancel_check, resp.error_message);
                if (got < 0) return resp;
                if (got == 0) {
                    resp.error_message = "unexpected EOF before Content-Length satisfied";
                    return resp;
                }
                if (!emit_wire(tmp, static_cast<size_t>(got))) {
                    return resp;
                }
                remaining -= static_cast<uint64_t>(got);
            }
        } else {
            if (!leftover.empty()) {
                if (!emit_wire(reinterpret_cast<const uint8_t*>(leftover.data()), leftover.size())) {
                    return resp;
                }
            }
            while (true) {
                ssize_t got = stream.ReadSome(tmp, sizeof(tmp), req.receive_timeout_ms,
                                              req.cancel_check, resp.error_message);
                if (got < 0) return resp;
                if (got == 0) break;
                if (!emit_wire(tmp, static_cast<size_t>(got))) {
                    return resp;
                }
            }
        }
    }

    if (is_compressed && !no_body && fed_gzip_bytes && !gz.Finish()) {
        resp.error_message = "truncated compressed stream";
        return resp;
    }

    resp.transport_ok = true;
    resp.tls_verified = is_https && !req.allow_insecure_tls;
    return resp;
}

class LinuxHttpTransport final : public IHttpTransport {
public:
    HttpResponse Execute(const HttpRequest& req) override {
        return detail::ExecuteWithSameOriginRedirects(req, ExecuteSingleLinuxHttp);
    }
};

}  // namespace

std::unique_ptr<IHttpTransport> CreateNativeHttpTransport() {
    return std::make_unique<LinuxHttpTransport>();
}

}  // namespace academic::tec_digital
