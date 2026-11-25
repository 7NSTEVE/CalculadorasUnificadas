#include "academic/tec_digital/http_transport.hpp"
#include "http_common.hpp"
#include "inflate.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace academic::tec_digital {

namespace {

std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring w(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), needed);
    return w;
}

std::string WideToUtf8(std::wstring_view w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string s(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        s.data(), needed, nullptr, nullptr);
    return s;
}

std::string FormatWinHttpError(const char* prefix, DWORD err) {
    return std::string(prefix) + " (win32_error=" + std::to_string(err) + ")";
}

HttpResponse ExecuteSingleWinHttp(const HttpRequest& req) {
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

    std::wstring ua = L"CalculadoraUnificada-TecDigital/1.0";
    for (const auto& kv : req.headers) {
        if (AsciiLower(kv.first) == "user-agent" && !kv.second.empty()) {
            ua = Utf8ToWide(kv.second);
            break;
        }
    }

    const DWORD access_type = IsLoopbackHost(parsed.host)
                                  ? WINHTTP_ACCESS_TYPE_NO_PROXY
                                  : WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
    HINTERNET hSession = WinHttpOpen(ua.c_str(), access_type,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession == nullptr) {
        resp.error_message = FormatWinHttpError("WinHttpOpen failed", GetLastError());
        return resp;
    }

    std::wstring whost = Utf8ToWide(parsed.host);
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(),
                                        static_cast<INTERNET_PORT>(parsed.port), 0);
    if (hConnect == nullptr) {
        resp.error_message = FormatWinHttpError("WinHttpConnect failed", GetLastError());
        WinHttpCloseHandle(hSession);
        return resp;
    }

    std::string method_upper = req.method.empty() ? "GET" : req.method;
    for (char& c : method_upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    std::wstring wmethod = Utf8ToWide(method_upper);
    std::wstring wtarget = Utf8ToWide(parsed.target);
    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), wtarget.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (hRequest == nullptr) {
        resp.error_message = FormatWinHttpError("WinHttpOpenRequest failed", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    WinHttpSetTimeouts(hRequest, req.connect_timeout_ms, req.connect_timeout_ms,
                       req.send_timeout_ms, req.receive_timeout_ms);

    // Disable automatic redirects so caller/wrapper can inspect Location and enforce same-origin.
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));

    const std::string range_val = FormatRangeHeader(req.range_start, req.range_end);
    bool has_accept_encoding = false;
    for (const auto& kv : req.headers) {
        if (AsciiLower(kv.first) == "accept-encoding") {
            has_accept_encoding = true;
            break;
        }
    }

    if (is_https && req.allow_insecure_tls) {
        DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                          SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
    }

    for (const auto& kv : req.headers) {
        std::string key_low = AsciiLower(kv.first);
        if (key_low == "user-agent" || key_low == "host" || key_low == "content-length") {
            continue;
        }
        if (key_low == "range" && TrimAscii(kv.second) == "bytes=0-") {
            continue;
        }
        if (!range_val.empty() && key_low == "range") {
            continue;
        }
        std::wstring hline = Utf8ToWide(kv.first + ": " + kv.second);
        WinHttpAddRequestHeaders(hRequest, hline.c_str(), static_cast<DWORD>(-1),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    if (!range_val.empty()) {
        std::wstring rline = Utf8ToWide("Range: " + range_val);
        WinHttpAddRequestHeaders(hRequest, rline.c_str(), static_cast<DWORD>(-1),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        if (!has_accept_encoding) {
            std::wstring aeline = L"Accept-Encoding: identity";
            WinHttpAddRequestHeaders(hRequest, aeline.c_str(), static_cast<DWORD>(-1),
                                     WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }
    } else if (!has_accept_encoding) {
        std::wstring aeline = L"Accept-Encoding: gzip, deflate";
        WinHttpAddRequestHeaders(hRequest, aeline.c_str(), static_cast<DWORD>(-1),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    LPVOID body_ptr = req.body.empty() ? WINHTTP_NO_REQUEST_DATA
                                       : const_cast<char*>(req.body.data());
    DWORD body_len = static_cast<DWORD>(req.body.size());

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            body_ptr, body_len, body_len, 0)) {
        resp.error_message = FormatWinHttpError("WinHttpSendRequest failed", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        resp.error_message = FormatWinHttpError("WinHttpReceiveResponse failed", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status_code, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        resp.status_code = static_cast<int>(status_code);
    }

    // Read all raw headers in exact arrival order (preserves multiple Set-Cookie headers!)
    std::string resp_content_encoding;
    DWORD raw_size = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &raw_size, WINHTTP_NO_HEADER_INDEX);
    if (raw_size > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::wstring raw_w(raw_size / sizeof(wchar_t), L'\0');
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                raw_w.data(), &raw_size, WINHTTP_NO_HEADER_INDEX)) {
            std::string raw_utf8 = WideToUtf8(raw_w);
            std::istringstream iss(raw_utf8);
            std::string line;
            bool first_line = true;
            while (std::getline(iss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (first_line) {
                    first_line = false;
                    continue;
                }
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
                } else if (low == "content-encoding") {
                    if (!resp_content_encoding.empty()) resp_content_encoding += ",";
                    resp_content_encoding += AsciiLower(val);
                } else if (low == "content-length") {
                    char* endptr = nullptr;
                    unsigned long long cl = std::strtoull(val.c_str(), &endptr, 10);
                    if (endptr != val.c_str()) {
                        resp.content_length = static_cast<uint64_t>(cl);
                        resp.has_content_length = true;
                    }
                }
            }
        }
    }

    if (req.header_callback) {
        if (!req.header_callback(resp)) {
            resp.error_message = "aborted by header_callback";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return resp;
        }
    }

    enum class ManualDecomp { None, Gzip, Deflate };
    ManualDecomp manual_decomp = ManualDecomp::None;
    if (method_upper != "HEAD" && resp.status_code == 200) {
        if (resp_content_encoding.find("gzip") != std::string::npos) {
            manual_decomp = ManualDecomp::Gzip;
        } else if (resp_content_encoding.find("deflate") != std::string::npos) {
            manual_decomp = ManualDecomp::Deflate;
        }
    }

    constexpr size_t kMaxManualDecompressBytes = 512ULL * 1024ULL * 1024ULL;
    std::vector<uint8_t> compressed_buf;
    bool read_ok = true;
    while (method_upper != "HEAD" && resp.status_code != 204 && resp.status_code != 304) {
        if (req.cancel_check && req.cancel_check()) {
            resp.error_message = "cancelled during response body";
            read_ok = false;
            break;
        }
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
            resp.error_message = FormatWinHttpError("WinHttpQueryDataAvailable failed", GetLastError());
            read_ok = false;
            break;
        }
        if (avail == 0) {
            break;
        }
        std::vector<uint8_t> buf(avail);
        DWORD read_bytes = 0;
        if (!WinHttpReadData(hRequest, buf.data(), avail, &read_bytes)) {
            resp.error_message = FormatWinHttpError("WinHttpReadData failed", GetLastError());
            read_ok = false;
            break;
        }
        if (read_bytes == 0) break;
        if (manual_decomp != ManualDecomp::None) {
            if (compressed_buf.size() + read_bytes > kMaxManualDecompressBytes) {
                resp.error_message = "compressed response exceeds maximum limit";
                read_ok = false;
                break;
            }
            compressed_buf.insert(compressed_buf.end(), buf.begin(), buf.begin() + read_bytes);
        } else if (req.chunk_callback) {
            if (!req.chunk_callback(buf.data(), read_bytes)) {
                resp.error_message = "aborted by chunk_callback";
                read_ok = false;
                break;
            }
        } else {
            resp.body.insert(resp.body.end(), buf.begin(), buf.begin() + read_bytes);
        }
    }

    if (read_ok && manual_decomp != ManualDecomp::None) {
        Result<std::vector<uint8_t>> dec =
            (manual_decomp == ManualDecomp::Gzip)
                ? detail::InflateGzipImpl(
                      compressed_buf.data(), compressed_buf.size(), kMaxManualDecompressBytes)
                : [&]() {
                      auto zr = detail::InflateZlibImpl(
                          compressed_buf.data(), compressed_buf.size(), kMaxManualDecompressBytes);
                      if (!zr.ok && (zr.error_code == "invalid_zlib" ||
                                     zr.error_code == "unsupported_zlib")) {
                          return detail::InflateRawDeflateImpl(
                              compressed_buf.data(), compressed_buf.size(), kMaxManualDecompressBytes);
                      }
                      return zr;
                  }();
        std::vector<uint8_t>().swap(compressed_buf);
        if (!dec.ok) {
            resp.error_message = "decompression failed: " + dec.error_message;
            read_ok = false;
        } else if (req.chunk_callback) {
            constexpr size_t kEmitChunk = 16384;
            size_t off = 0;
            while (off < dec.value.size()) {
                size_t n = std::min(kEmitChunk, dec.value.size() - off);
                if (!req.chunk_callback(dec.value.data() + off, n)) {
                    resp.error_message = "aborted by chunk_callback";
                    read_ok = false;
                    break;
                }
                off += n;
            }
        } else {
            resp.body = std::move(dec.value);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    resp.transport_ok = read_ok;
    resp.tls_verified = is_https && !req.allow_insecure_tls && read_ok;
    return resp;
}

class WinHttpTransport final : public IHttpTransport {
public:
    HttpResponse Execute(const HttpRequest& req) override {
        return detail::ExecuteWithSameOriginRedirects(req, ExecuteSingleWinHttp);
    }
};

}  // namespace

std::unique_ptr<IHttpTransport> CreateNativeHttpTransport() {
    return std::make_unique<WinHttpTransport>();
}

}  // namespace academic::tec_digital
