#include "gemini_notebook_client.hpp"
#include "../../core/cudoc_storage.hpp"
#include "../../core/inline_url_parser.hpp"
#include "../../core/utf_conv.hpp"
#include "../../media/native_video_engine.hpp"

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
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <winhttp.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

namespace DocStudio::GeminiNotebook {

namespace {

using json = nlohmann::json;

std::string ToLowerAscii(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

std::string TrimAscii(const std::string& s) {
    std::size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    std::size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string StripTrailingSlash(const std::string& s) {
    std::string out = s;
    while (!out.empty() && out.back() == '/') out.pop_back();
    return out;
}

std::string UrlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out.push_back(' ');
        } else if (s[i] == '%' && i + 2 < s.size()) {
            const auto hexVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return -1;
            };
            const int hi = hexVal(s[i + 1]);
            const int lo = hexVal(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
            out.push_back(s[i]);
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::int64_t NowEpochSeconds() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

#ifdef _WIN32
void ParseRawHeadersLower(HINTERNET hReq, std::map<std::string, std::string>& outHeadersLower) {
    DWORD hdrBytes = 0;
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
        nullptr, &hdrBytes, WINHTTP_NO_HEADER_INDEX);
    if (hdrBytes == 0 || hdrBytes > 256u * 1024u) return;
    std::wstring wRaw(hdrBytes / sizeof(wchar_t) + 1u, L'\0');
    if (!WinHttpQueryHeaders(hReq, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
            wRaw.data(), &hdrBytes, WINHTTP_NO_HEADER_INDEX)) {
        return;
    }
    const std::string raw = WideToUtf8(wRaw.c_str());
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t col = line.find(':');
        if (col == std::string::npos || col == 0) continue;
        const std::string key = ToLowerAscii(TrimAscii(line.substr(0, col)));
        const std::string val = TrimAscii(line.substr(col + 1));
        if (!key.empty()) outHeadersLower[key] = val;
    }
}

bool ExecuteWinHttpCore(
    const std::string& methodUtf8,
    const std::string& urlUtf8,
    const std::vector<std::pair<std::string, std::string>>& headersUtf8,
    const std::string* jsonBodyUtf8,
    const std::wstring* uploadFilePath,
    int timeoutMs,
    std::function<bool()> cancelCheck,
    NotebookHttpResponse& outResp)
{
    outResp = NotebookHttpResponse{};
    const std::wstring wUrl = Utf8ToWide(urlUtf8);
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostBuf[512]{};
    wchar_t pathBuf[4096]{};
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = static_cast<DWORD>(std::size(hostBuf));
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = static_cast<DWORD>(std::size(pathBuf));
    if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.size()), 0, &uc)) {
        outResp.errorUtf8 = "URL invalida para solicitud HTTP.";
        return false;
    }
    const bool isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    const std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (path.empty()) path = L"/";

    HINTERNET hSession = WinHttpOpen(L"CuadernU/2.0 (Windows; GeminiNotebook)",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        outResp.errorUtf8 = "No se pudo abrir sesion WinHTTP.";
        return false;
    }
    const int effTimeout = (std::max)(3000, timeoutMs);
    WinHttpSetTimeouts(hSession, effTimeout, effTimeout, effTimeout, effTimeout);
    if (isHttps) {
        DWORD secProt = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
        WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secProt, sizeof(secProt));
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        outResp.errorUtf8 = "No se pudo conectar con el servidor remoto.";
        return false;
    }

    const std::wstring wMethod = Utf8ToWide(methodUtf8);
    HINTERNET hReq = WinHttpOpenRequest(hConnect, wMethod.c_str(), path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outResp.errorUtf8 = "No se pudo crear solicitud HTTP.";
        return false;
    }

    for (const auto& kv : headersUtf8) {
        const std::wstring hdr = Utf8ToWide(kv.first + ": " + kv.second + "\r\n");
        WinHttpAddRequestHeaders(hReq, hdr.c_str(), static_cast<DWORD>(hdr.size()),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    BOOL sent = FALSE;
    if (uploadFilePath && !uploadFilePath->empty()) {
        std::error_code ec;
        const std::uint64_t fsz = std::filesystem::file_size(std::filesystem::path(*uploadFilePath), ec);
        if (ec || fsz == 0 || fsz > 0x7FFFFFFFull) {
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            outResp.errorUtf8 = "Archivo invalido o inaccesible para carga binaria.";
            return false;
        }
        std::ifstream ifs(std::filesystem::path(*uploadFilePath), std::ios::binary);
        if (!ifs.is_open()) {
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            outResp.errorUtf8 = "No se pudo abrir archivo local para lectura.";
            return false;
        }
        sent = WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, static_cast<DWORD>(fsz), 0);
        if (sent) {
            std::vector<char> chunk(65536u);
            while (ifs.good()) {
                if (cancelCheck && cancelCheck()) {
                    sent = FALSE;
                    outResp.errorUtf8 = "Carga cancelada por el usuario.";
                    break;
                }
                ifs.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                const std::streamsize got = ifs.gcount();
                if (got <= 0) break;
                DWORD written = 0;
                if (!WinHttpWriteData(hReq, chunk.data(), static_cast<DWORD>(got), &written) ||
                    written != static_cast<DWORD>(got)) {
                    sent = FALSE;
                    outResp.errorUtf8 = "Error de red durante la transmision del archivo.";
                    break;
                }
            }
        }
    } else {
        LPVOID bodyPtr = (jsonBodyUtf8 && !jsonBodyUtf8->empty())
            ? const_cast<char*>(jsonBodyUtf8->data()) : WINHTTP_NO_REQUEST_DATA;
        const DWORD bodyLen = (jsonBodyUtf8 && !jsonBodyUtf8->empty())
            ? static_cast<DWORD>(jsonBodyUtf8->size()) : 0u;
        sent = WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            bodyPtr, bodyLen, bodyLen, 0);
    }

    if (!sent || !WinHttpReceiveResponse(hReq, nullptr)) {
        if (outResp.errorUtf8.empty()) {
            outResp.errorUtf8 = "Error de transporte HTTP (WinHTTP #" + std::to_string(GetLastError()) + ").";
        }
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    outResp.httpStatus = static_cast<int>(statusCode);
    ParseRawHeadersLower(hReq, outResp.headersLower);

    std::string body;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        if (body.size() + avail > 16u * 1024u * 1024u) break;
        const std::size_t oldSz = body.size();
        body.resize(oldSz + avail);
        DWORD got = 0;
        if (!WinHttpReadData(hReq, body.data() + oldSz, avail, &got)) break;
        body.resize(oldSz + got);
        if (got == 0) break;
    }
    outResp.bodyUtf8 = std::move(body);
    outResp.transportOk = true;

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}
#endif

} // namespace

// ---------------- WinHttpNotebookTransport ----------------

NotebookHttpResponse WinHttpNotebookTransport::SendJson(
    const std::string& methodUtf8,
    const std::string& urlUtf8,
    const std::vector<std::pair<std::string, std::string>>& headersUtf8,
    const std::string* bodyUtf8,
    int timeoutMs)
{
    NotebookHttpResponse resp{};
#ifdef _WIN32
    ExecuteWinHttpCore(methodUtf8, urlUtf8, headersUtf8, bodyUtf8, nullptr, timeoutMs, nullptr, resp);
#else
    (void)methodUtf8; (void)urlUtf8; (void)headersUtf8; (void)bodyUtf8; (void)timeoutMs;
    resp.errorUtf8 = "WinHTTP requiere Windows.";
#endif
    return resp;
}

NotebookHttpResponse WinHttpNotebookTransport::UploadFileRaw(
    const std::string& urlUtf8,
    const std::vector<std::pair<std::string, std::string>>& headersUtf8,
    const std::wstring& filePath,
    int timeoutMs,
    std::function<bool()> cancelCheck)
{
    NotebookHttpResponse resp{};
#ifdef _WIN32
    ExecuteWinHttpCore("POST", urlUtf8, headersUtf8, nullptr, &filePath, timeoutMs, std::move(cancelCheck), resp);
#else
    (void)urlUtf8; (void)headersUtf8; (void)filePath; (void)timeoutMs; (void)cancelCheck;
    resp.errorUtf8 = "WinHTTP requiere Windows.";
#endif
    return resp;
}

// ---------------- GoogleOAuthPkceHelper ----------------

std::string GoogleOAuthPkceHelper::Base64UrlEncode(const std::uint8_t* data, std::size_t len) {
    static constexpr char kTbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve(((len + 2u) / 3u) * 4u);
    std::size_t i = 0;
    while (i + 2 < len) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16u) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8u) |
                                static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kTbl[(v >> 18u) & 0x3Fu]);
        out.push_back(kTbl[(v >> 12u) & 0x3Fu]);
        out.push_back(kTbl[(v >> 6u) & 0x3Fu]);
        out.push_back(kTbl[v & 0x3Fu]);
        i += 3;
    }
    const std::size_t rem = len - i;
    if (rem == 1) {
        const std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16u;
        out.push_back(kTbl[(v >> 18u) & 0x3Fu]);
        out.push_back(kTbl[(v >> 12u) & 0x3Fu]);
    } else if (rem == 2) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16u) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8u);
        out.push_back(kTbl[(v >> 18u) & 0x3Fu]);
        out.push_back(kTbl[(v >> 12u) & 0x3Fu]);
        out.push_back(kTbl[(v >> 6u) & 0x3Fu]);
    }
    return out;
}

std::string GoogleOAuthPkceHelper::UrlEncode(const std::string& raw) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(raw.size() + 16u);
    for (unsigned char c : raw) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4u) & 0x0Fu]);
            out.push_back(kHex[c & 0x0Fu]);
        }
    }
    return out;
}

std::string GoogleOAuthPkceHelper::GenerateCodeVerifier(std::size_t length) {
    static constexpr char kUnreserved[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    const std::size_t n = std::clamp<std::size_t>(length, 43u, 128u);
    std::vector<std::uint8_t> rnd(n, 0u);
#ifdef _WIN32
    BCryptGenRandom(nullptr, rnd.data(), static_cast<ULONG>(rnd.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#endif
    std::string out;
    out.reserve(n);
    for (std::uint8_t b : rnd) {
        out.push_back(kUnreserved[b % 66u]);
    }
    return out;
}

std::string GoogleOAuthPkceHelper::ComputeCodeChallengeS256(const std::string& codeVerifier) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::array<std::uint8_t, 32> digest{};
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0) {
            if (!codeVerifier.empty()) {
                BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(codeVerifier.data())),
                    static_cast<ULONG>(codeVerifier.size()), 0);
            }
            BCryptFinishHash(hHash, digest.data(), static_cast<ULONG>(digest.size()), 0);
            BCryptDestroyHash(hHash);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return Base64UrlEncode(digest.data(), digest.size());
    }
#endif
    return {};
}

std::string GoogleOAuthPkceHelper::BuildAuthorizeUrl(
    const GeminiNotebookConfig& cfg,
    const std::string& redirectUriUtf8,
    const std::string& codeChallengeS256,
    const std::string& stateToken)
{
    std::ostringstream oss;
    oss << cfg.oauthAuthorizeEndpointUtf8
        << "?client_id=" << UrlEncode(cfg.oauthClientIdUtf8)
        << "&redirect_uri=" << UrlEncode(redirectUriUtf8)
        << "&response_type=code"
        << "&scope=" << UrlEncode(cfg.oauthScopeUtf8 + " email")
        << "&code_challenge=" << UrlEncode(codeChallengeS256)
        << "&code_challenge_method=S256"
        << "&access_type=offline"
        << "&prompt=consent"
        << "&state=" << UrlEncode(stateToken);
    return oss.str();
}

bool GoogleOAuthPkceHelper::IsWorkspaceAccountRejection(int httpStatus, const std::string& responseBodyUtf8) noexcept {
    if (httpStatus != 400 && httpStatus != 403) return false;
    const std::string low = ToLowerAscii(responseBodyUtf8);
    return low.find("workspace") != std::string::npos ||
           low.find("personal account") != std::string::npos ||
           low.find("consumer account") != std::string::npos ||
           low.find("enterprise") != std::string::npos ||
           low.find("not supported for organization") != std::string::npos ||
           low.find("failed_precondition") != std::string::npos;
}

bool GoogleOAuthPkceHelper::ExchangeAuthorizationCode(
    const GeminiNotebookConfig& cfg,
    INotebookHttpTransport& transport,
    const std::string& codeUtf8,
    const std::string& redirectUriUtf8,
    const std::string& codeVerifierUtf8,
    GeminiNotebookAuthTokens& outTokens,
    std::string* outErr)
{
    std::ostringstream form;
    form << "grant_type=authorization_code"
         << "&code=" << UrlEncode(codeUtf8)
         << "&client_id=" << UrlEncode(cfg.oauthClientIdUtf8)
         << "&redirect_uri=" << UrlEncode(redirectUriUtf8)
         << "&code_verifier=" << UrlEncode(codeVerifierUtf8);
    if (!cfg.oauthClientSecretUtf8.empty()) {
        form << "&client_secret=" << UrlEncode(cfg.oauthClientSecretUtf8);
    }
    const std::string body = form.str();
    const std::vector<std::pair<std::string, std::string>> hdrs = {
        { "Content-Type", "application/x-www-form-urlencoded" }
    };
    const NotebookHttpResponse resp = transport.SendJson("POST", cfg.oauthTokenEndpointUtf8, hdrs, &body, cfg.httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (outErr) {
            *outErr = IsWorkspaceAccountRejection(resp.httpStatus, resp.bodyUtf8)
                ? "Google rechazo la cuenta institucional/Workspace. Usa una cuenta personal de Google para Gemini Notebook."
                : ("No se pudo completar el intercambio OAuth (HTTP " + std::to_string(resp.httpStatus) + ").");
        }
        return false;
    }
    try {
        const json j = json::parse(resp.bodyUtf8);
        outTokens.accessTokenUtf8 = j.value("access_token", "");
        if (j.contains("refresh_token") && j["refresh_token"].is_string()) {
            outTokens.refreshTokenUtf8 = j["refresh_token"].get<std::string>();
        }
        outTokens.oauthClientSecretUtf8 = cfg.oauthClientSecretUtf8;
        const int expIn = j.value("expires_in", 3600);
        outTokens.expiresAtEpochSec = NowEpochSeconds() + expIn;
        outTokens.workspaceRejectionSeen = false;
        if (outTokens.accessTokenUtf8.empty()) {
            if (outErr) *outErr = "Respuesta OAuth sin token de acceso.";
            return false;
        }
        return true;
    } catch (...) {
        if (outErr) *outErr = "Respuesta OAuth con JSON invalido.";
        return false;
    }
}

bool GoogleOAuthPkceHelper::RefreshAccessToken(
    const GeminiNotebookConfig& cfg,
    INotebookHttpTransport& transport,
    GeminiNotebookAuthTokens& inOutTokens,
    std::string* outErr)
{
    if (inOutTokens.refreshTokenUtf8.empty() || cfg.oauthClientIdUtf8.empty()) {
        if (outErr) *outErr = "No hay token de renovacion OAuth configurado.";
        return false;
    }
    std::ostringstream form;
    form << "grant_type=refresh_token"
         << "&refresh_token=" << UrlEncode(inOutTokens.refreshTokenUtf8)
         << "&client_id=" << UrlEncode(cfg.oauthClientIdUtf8);
    const std::string sec = !inOutTokens.oauthClientSecretUtf8.empty()
        ? inOutTokens.oauthClientSecretUtf8 : cfg.oauthClientSecretUtf8;
    if (!sec.empty()) form << "&client_secret=" << UrlEncode(sec);

    const std::string body = form.str();
    const std::vector<std::pair<std::string, std::string>> hdrs = {
        { "Content-Type", "application/x-www-form-urlencoded" }
    };
    const NotebookHttpResponse resp = transport.SendJson("POST", cfg.oauthTokenEndpointUtf8, hdrs, &body, cfg.httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (outErr) *outErr = "No se pudo renovar el token OAuth (HTTP " + std::to_string(resp.httpStatus) + ").";
        return false;
    }
    try {
        const json j = json::parse(resp.bodyUtf8);
        const std::string fresh = j.value("access_token", "");
        if (fresh.empty()) return false;
        inOutTokens.accessTokenUtf8 = fresh;
        inOutTokens.expiresAtEpochSec = NowEpochSeconds() + j.value("expires_in", 3600);
        return true;
    } catch (...) {
        if (outErr) *outErr = "Respuesta de renovacion OAuth invalida.";
        return false;
    }
}

bool GoogleOAuthPkceHelper::RunLoopbackAuthorization(
    const GeminiNotebookConfig& cfg,
    INotebookHttpTransport& transport,
    GeminiNotebookAuthTokens& outTokens,
    int timeoutSeconds,
    std::string* outErr,
    std::function<bool(const std::string& authorizeUrl)> browserLauncherOverride)
{
    if (cfg.oauthClientIdUtf8.empty()) {
        if (outErr) *outErr = "Configura primero tu Client ID de escritorio de Google Cloud.";
        return false;
    }
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        if (outErr) *outErr = "No se pudo inicializar socket local para autenticacion.";
        return false;
    }
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) {
        WSACleanup();
        if (outErr) *outErr = "No se pudo crear socket local.";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // Ephemeral OS-assigned loopback port
    if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        listen(srv, 1) == SOCKET_ERROR) {
        closesocket(srv);
        WSACleanup();
        if (outErr) *outErr = "No se pudo enlazar puerto local temporal en 127.0.0.1.";
        return false;
    }
    int addrLen = sizeof(addr);
    getsockname(srv, reinterpret_cast<sockaddr*>(&addr), &addrLen);
    const std::uint16_t port = ntohs(addr.sin_port);

    const std::string redirectUri = "http://127.0.0.1:" + std::to_string(port) + "/callback";
    const std::string verifier = GenerateCodeVerifier(64u);
    const std::string challenge = ComputeCodeChallengeS256(verifier);
    const std::string state = GenerateCodeVerifier(43u);
    const std::string authUrl = BuildAuthorizeUrl(cfg, redirectUri, challenge, state);

    bool launched = false;
    if (browserLauncherOverride) {
        launched = browserLauncherOverride(authUrl);
    } else {
        const HINSTANCE res = ShellExecuteW(nullptr, L"open", Utf8ToWide(authUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        launched = reinterpret_cast<INT_PTR>(res) > 32;
    }
    if (!launched) {
        closesocket(srv);
        WSACleanup();
        if (outErr) *outErr = "No se pudo abrir el navegador del sistema para iniciar sesion.";
        return false;
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(srv, &rfds);
    timeval tv{};
    tv.tv_sec = (std::max)(5, timeoutSeconds);
    tv.tv_usec = 0;
    const int sel = select(0, &rfds, nullptr, nullptr, &tv);
    if (sel <= 0) {
        closesocket(srv);
        WSACleanup();
        if (outErr) *outErr = "Tiempo agotado esperando la confirmacion en el navegador.";
        return false;
    }

    SOCKET cli = accept(srv, nullptr, nullptr);
    std::string reqText;
    if (cli != INVALID_SOCKET) {
        char buf[4096]{};
        const int got = recv(cli, buf, sizeof(buf) - 1, 0);
        if (got > 0) reqText.assign(buf, static_cast<std::size_t>(got));

        const std::string html =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
            "<!DOCTYPE html><html><body style='font-family:Segoe UI,sans-serif;max-width:520px;margin:60px auto;color:#161a22'>"
            "<h2>CuadernU conectado</h2><p>La autorizacion se recibio correctamente. Ya puedes cerrar esta pestana y volver a CuadernU.</p>"
            "</body></html>";
        send(cli, html.data(), static_cast<int>(html.size()), 0);
        closesocket(cli);
    }
    closesocket(srv);
    WSACleanup();

    // Parse GET /callback?code=...&state=... HTTP/1.1
    const std::size_t qPos = reqText.find('?');
    const std::size_t spPos = (qPos != std::string::npos) ? reqText.find(' ', qPos) : std::string::npos;
    if (qPos == std::string::npos || spPos == std::string::npos) {
        if (outErr) *outErr = "Callback OAuth incompleto.";
        return false;
    }
    const std::string query = reqText.substr(qPos + 1, spPos - (qPos + 1));
    std::string gotCode, gotState, gotErr;
    std::istringstream qss(query);
    std::string pair;
    while (std::getline(qss, pair, '&')) {
        const std::size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = pair.substr(0, eq);
        const std::string v = UrlDecode(pair.substr(eq + 1));
        if (k == "code") gotCode = v;
        else if (k == "state") gotState = v;
        else if (k == "error") gotErr = v;
    }
    if (!gotErr.empty()) {
        if (outErr) *outErr = "Autorizacion cancelada o rechazada (" + gotErr + ").";
        return false;
    }
    if (gotState != state || gotCode.empty()) {
        if (outErr) *outErr = "Validacion de estado de seguridad OAuth invalida.";
        return false;
    }
    return ExchangeAuthorizationCode(cfg, transport, gotCode, redirectUri, verifier, outTokens, outErr);
#else
    (void)cfg; (void)transport; (void)outTokens; (void)timeoutSeconds; (void)browserLauncherOverride;
    if (outErr) *outErr = "OAuth loopback requiere Windows.";
    return false;
#endif
}

bool GoogleOAuthPkceHelper::TryImportGcloudAdcToken(std::string& outAccessTokenUtf8, std::string* outErr) {
    outAccessTokenUtf8.clear();
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"cmd.exe /C gcloud auth application-default print-access-token";
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) {
        CloseHandle(hRead);
        if (outErr) *outErr = "No se encontro gcloud instalado en el sistema.";
        return false;
    }
    WaitForSingleObject(pi.hProcess, 8000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    std::string out;
    char buf[512]{};
    DWORD got = 0;
    while (ReadFile(hRead, buf, sizeof(buf), &got, nullptr) && got > 0) {
        out.append(buf, got);
    }
    CloseHandle(hRead);

    out = TrimAscii(out);
    if (exitCode == 0 && out.size() > 20 && out.find(' ') == std::string::npos) {
        outAccessTokenUtf8 = out;
        return true;
    }
    if (outErr) *outErr = "No se pudo obtener un token activo desde gcloud.";
    return false;
#else
    if (outErr) *outErr = "No compatible en esta plataforma.";
    return false;
#endif
}

// ---------------- GeminiNotebookRestClient ----------------

GeminiNotebookRestClient::GeminiNotebookRestClient(
    GeminiNotebookSyncStore& store,
    INotebookHttpTransport* transport,
    UploadRateLimiter* rateLimiter)
    : store_(store)
{
    if (transport) {
        transport_ = transport;
    } else {
        ownedTransport_ = std::make_unique<WinHttpNotebookTransport>();
        transport_ = ownedTransport_.get();
    }
    if (rateLimiter) {
        limiter_ = rateLimiter;
    } else {
        ownedLimiter_ = std::make_unique<UploadRateLimiter>(store.Config().maxUploadsPerMinute);
        limiter_ = ownedLimiter_.get();
    }
}

std::vector<std::pair<std::string, std::string>> GeminiNotebookRestClient::BuildAuthHeaders(bool jsonContentType) const {
    std::vector<std::pair<std::string, std::string>> h;
    if (!store_.Tokens().accessTokenUtf8.empty()) {
        h.emplace_back("Authorization", "Bearer " + store_.Tokens().accessTokenUtf8);
    }
    if (!store_.Config().projectIdUtf8.empty()) {
        h.emplace_back("x-goog-user-project", store_.Config().projectIdUtf8);
    }
    if (jsonContentType) {
        h.emplace_back("Content-Type", "application/json; charset=utf-8");
    }
    return h;
}

std::uint64_t GeminiNotebookRestClient::ParseRetryAfterMs(const NotebookHttpResponse& resp) {
    auto it = resp.headersLower.find("retry-after");
    if (it == resp.headersLower.end() || it->second.empty()) return 3000ull;
    const long sec = std::strtol(it->second.c_str(), nullptr, 10);
    if (sec <= 0) return 3000ull;
    return static_cast<std::uint64_t>((std::min<long>)(sec, 120l)) * 1000ull;
}

bool GeminiNotebookRestClient::ParseNotebookJson(const std::string& bodyUtf8, RemoteNotebookRecord& outNb) {
    try {
        const json j = json::parse(bodyUtf8);
        outNb = RemoteNotebookRecord{};
        outNb.resourceNameUtf8 = j.value("name", "");
        outNb.displayNameUtf8 = j.value("display_name", j.value("displayName", ""));
        outNb.createTimeUtf8 = j.value("create_time", j.value("createTime", ""));
        outNb.updateTimeUtf8 = j.value("update_time", j.value("updateTime", ""));
        outNb.webUrlUtf8 = BuildWebNotebookUrl(outNb.resourceNameUtf8);
        return !outNb.resourceNameUtf8.empty();
    } catch (...) {
        return false;
    }
}

bool GeminiNotebookRestClient::ParseSourceJson(const std::string& bodyUtf8, RemoteSourceRecord& outSrc) {
    try {
        const json j = json::parse(bodyUtf8);
        outSrc = RemoteSourceRecord{};
        outSrc.resourceNameUtf8 = j.value("name", "");
        outSrc.displayNameUtf8 = j.value("display_name", j.value("displayName", ""));
        outSrc.mimeTypeUtf8 = j.value("mime_type", j.value("mimeType", ""));
        outSrc.stateUtf8 = j.value("state", "ACTIVE");
        outSrc.createTimeUtf8 = j.value("create_time", j.value("createTime", ""));
        if (j.contains("web_content") && j["web_content"].is_object()) {
            outSrc.sourceUrlUtf8 = j["web_content"].value("url", "");
        }
        return !outSrc.resourceNameUtf8.empty();
    } catch (...) {
        return false;
    }
}

bool GeminiNotebookRestClient::EnsureValidAccessToken(std::string* outErr) {
    auto& tok = store_.MutableTokens();
    const std::int64_t now = NowEpochSeconds();
    if (tok.HasUsableAccessToken(now)) return true;
    if (!tok.refreshTokenUtf8.empty()) {
        if (GoogleOAuthPkceHelper::RefreshAccessToken(store_.Config(), *transport_, tok, outErr)) {
            store_.SaveTokensDpapi(nullptr);
            return true;
        }
    }
    if (outErr && outErr->empty()) {
        *outErr = "Conecta primero tu cuenta personal de Google para usar Gemini Notebook.";
    }
    return false;
}

bool GeminiNotebookRestClient::ListNotebooks(
    std::vector<RemoteNotebookRecord>& outNotebooks,
    std::string* outErr,
    bool* outWorkspaceRejected)
{
    outNotebooks.clear();
    if (outWorkspaceRejected) *outWorkspaceRejected = false;
    if (!EnsureValidAccessToken(outErr)) return false;

    const std::string url = StripTrailingSlash(store_.Config().apiBaseUrlUtf8) + "/notebooks";
    const NotebookHttpResponse resp = transport_->SendJson("GET", url, BuildAuthHeaders(false), nullptr, store_.Config().httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (GoogleOAuthPkceHelper::IsWorkspaceAccountRejection(resp.httpStatus, resp.bodyUtf8)) {
            store_.MutableTokens().workspaceRejectionSeen = true;
            if (outWorkspaceRejected) *outWorkspaceRejected = true;
            if (outErr) *outErr = "Tu cuenta Google Workspace/TEC no es compatible con la version preliminar. Usa una cuenta personal de Google.";
        } else if (outErr) {
            *outErr = SanitizeSecretFromText("Error listando cuadernos (HTTP " + std::to_string(resp.httpStatus) + "): " + resp.errorUtf8, store_.Tokens());
        }
        return false;
    }

    try {
        const json j = json::parse(resp.bodyUtf8);
        if (j.contains("notebooks") && j["notebooks"].is_array()) {
            for (const auto& item : j["notebooks"]) {
                RemoteNotebookRecord nb{};
                if (ParseNotebookJson(item.dump(), nb)) {
                    outNotebooks.push_back(std::move(nb));
                }
            }
        }
        return true;
    } catch (...) {
        if (outErr) *outErr = "Respuesta JSON invalida al listar cuadernos.";
        return false;
    }
}

bool GeminiNotebookRestClient::CreateNotebook(
    const std::string& displayNameUtf8,
    RemoteNotebookRecord& outNotebook,
    std::string* outErr,
    bool* outWorkspaceRejected)
{
    if (outWorkspaceRejected) *outWorkspaceRejected = false;
    if (!EnsureValidAccessToken(outErr)) return false;

    const std::string url = StripTrailingSlash(store_.Config().apiBaseUrlUtf8) + "/notebooks";
    const std::string body = json({ { "display_name", displayNameUtf8.empty() ? "Cuaderno CuadernU" : displayNameUtf8 } }).dump();
    const NotebookHttpResponse resp = transport_->SendJson("POST", url, BuildAuthHeaders(true), &body, store_.Config().httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (GoogleOAuthPkceHelper::IsWorkspaceAccountRejection(resp.httpStatus, resp.bodyUtf8)) {
            store_.MutableTokens().workspaceRejectionSeen = true;
            if (outWorkspaceRejected) *outWorkspaceRejected = true;
            if (outErr) *outErr = "Cuenta Google Workspace no admitida en Public Preview. Usa una cuenta personal de Google.";
        } else if (outErr) {
            *outErr = SanitizeSecretFromText("No se pudo crear el cuaderno (HTTP " + std::to_string(resp.httpStatus) + ").", store_.Tokens());
        }
        return false;
    }
    if (!ParseNotebookJson(resp.bodyUtf8, outNotebook)) {
        if (outErr) *outErr = "No se pudo interpretar el cuaderno creado.";
        return false;
    }
    return true;
}

bool GeminiNotebookRestClient::GetNotebook(
    const std::string& resourceNameUtf8,
    RemoteNotebookRecord& outNotebook,
    std::string* outErr)
{
    if (!EnsureValidAccessToken(outErr)) return false;
    const std::string url = StripTrailingSlash(store_.Config().apiBaseUrlUtf8) + "/" + resourceNameUtf8;
    const NotebookHttpResponse resp = transport_->SendJson("GET", url, BuildAuthHeaders(false), nullptr, store_.Config().httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (outErr) *outErr = "No se pudo consultar el cuaderno (HTTP " + std::to_string(resp.httpStatus) + ").";
        return false;
    }
    return ParseNotebookJson(resp.bodyUtf8, outNotebook);
}

bool GeminiNotebookRestClient::RenameNotebook(
    const std::string& resourceNameUtf8,
    const std::string& newDisplayNameUtf8,
    RemoteNotebookRecord& outNotebook,
    std::string* outErr)
{
    if (!EnsureValidAccessToken(outErr)) return false;
    const std::string url = StripTrailingSlash(store_.Config().apiBaseUrlUtf8) + "/" + resourceNameUtf8 + "?updateMask=display_name";
    const std::string body = json({ { "display_name", newDisplayNameUtf8 } }).dump();
    const NotebookHttpResponse resp = transport_->SendJson("PATCH", url, BuildAuthHeaders(true), &body, store_.Config().httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (outErr) *outErr = "No se pudo renombrar el cuaderno (HTTP " + std::to_string(resp.httpStatus) + ").";
        return false;
    }
    return ParseNotebookJson(resp.bodyUtf8, outNotebook);
}

bool GeminiNotebookRestClient::ListSources(
    const std::string& notebookResourceNameUtf8,
    std::vector<RemoteSourceRecord>& outSources,
    std::string* outErr)
{
    outSources.clear();
    if (!EnsureValidAccessToken(outErr)) return false;
    const std::string url = StripTrailingSlash(store_.Config().apiBaseUrlUtf8) + "/" + notebookResourceNameUtf8 + "/sources";
    const NotebookHttpResponse resp = transport_->SendJson("GET", url, BuildAuthHeaders(false), nullptr, store_.Config().httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (outErr) *outErr = "No se pudieron listar las fuentes del cuaderno (HTTP " + std::to_string(resp.httpStatus) + ").";
        return false;
    }
    try {
        const json j = json::parse(resp.bodyUtf8);
        if (j.contains("sources") && j["sources"].is_array()) {
            for (const auto& item : j["sources"]) {
                RemoteSourceRecord src{};
                if (ParseSourceJson(item.dump(), src)) outSources.push_back(std::move(src));
            }
        }
        return true;
    } catch (...) {
        if (outErr) *outErr = "Respuesta invalida al listar fuentes.";
        return false;
    }
}

bool GeminiNotebookRestClient::GetSource(
    const std::string& sourceResourceNameUtf8,
    RemoteSourceRecord& outSource,
    std::string* outErr)
{
    if (!EnsureValidAccessToken(outErr)) return false;
    const std::string url = StripTrailingSlash(store_.Config().apiBaseUrlUtf8) + "/" + sourceResourceNameUtf8;
    const NotebookHttpResponse resp = transport_->SendJson("GET", url, BuildAuthHeaders(false), nullptr, store_.Config().httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (outErr) *outErr = "No se pudo consultar el estado de la fuente (HTTP " + std::to_string(resp.httpStatus) + ").";
        return false;
    }
    return ParseSourceJson(resp.bodyUtf8, outSource);
}

bool GeminiNotebookRestClient::CreateWebOrYouTubeSource(
    const std::string& notebookResourceNameUtf8,
    const std::string& displayNameUtf8,
    const std::string& urlUtf8,
    RemoteSourceRecord& outSource,
    std::string* outErr,
    bool* outWorkspaceRejected)
{
    if (outWorkspaceRejected) *outWorkspaceRejected = false;
    if (!EnsureValidAccessToken(outErr)) return false;
    if (!IsSafeExternalLinkUrl(urlUtf8)) {
        if (outErr) *outErr = "URL bloqueada por seguridad (solo http/https).";
        return false;
    }
    const std::string url = StripTrailingSlash(store_.Config().apiBaseUrlUtf8) + "/" + notebookResourceNameUtf8 + "/sources";
    const json payload = {
        { "display_name", displayNameUtf8.empty() ? ExtractUrlDomainUtf8(urlUtf8) : displayNameUtf8 },
        { "web_content", { { "url", urlUtf8 } } }
    };
    const std::string body = payload.dump();
    const NotebookHttpResponse resp = transport_->SendJson("POST", url, BuildAuthHeaders(true), &body, store_.Config().httpTimeoutMs);
    if (resp.httpStatus == 429) {
        limiter_->NoteRetryAfterMs(ParseRetryAfterMs(resp));
        if (outErr) *outErr = "Limite de velocidad alcanzado (HTTP 429). Reintentando respetando cuota.";
        return false;
    }
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (GoogleOAuthPkceHelper::IsWorkspaceAccountRejection(resp.httpStatus, resp.bodyUtf8)) {
            if (outWorkspaceRejected) *outWorkspaceRejected = true;
            if (outErr) *outErr = "Cuenta Workspace rechazada; requiere cuenta personal de Google.";
        } else if (outErr) {
            *outErr = SanitizeSecretFromText("Error agregando enlace/YouTube (HTTP " + std::to_string(resp.httpStatus) + ").", store_.Tokens());
        }
        return false;
    }
    return ParseSourceJson(resp.bodyUtf8, outSource);
}

bool GeminiNotebookRestClient::CreateTextOrMarkdownSource(
    const std::string& notebookResourceNameUtf8,
    const std::string& displayNameUtf8,
    const std::string& textUtf8,
    const std::string& mimeTypeUtf8,
    RemoteSourceRecord& outSource,
    std::string* outErr,
    bool* outWorkspaceRejected)
{
    if (outWorkspaceRejected) *outWorkspaceRejected = false;
    if (!EnsureValidAccessToken(outErr)) return false;
    const std::string url = StripTrailingSlash(store_.Config().apiBaseUrlUtf8) + "/" + notebookResourceNameUtf8 + "/sources";
    const json payload = {
        { "display_name", displayNameUtf8.empty() ? "Notas CuadernU.md" : displayNameUtf8 },
        { "mime_type", mimeTypeUtf8.empty() ? "text/markdown" : mimeTypeUtf8 },
        { "text_content", { { "content", textUtf8 } } }
    };
    const std::string body = payload.dump();
    const NotebookHttpResponse resp = transport_->SendJson("POST", url, BuildAuthHeaders(true), &body, store_.Config().httpTimeoutMs);
    if (resp.httpStatus == 429) {
        limiter_->NoteRetryAfterMs(ParseRetryAfterMs(resp));
        if (outErr) *outErr = "Limite de velocidad alcanzado (HTTP 429).";
        return false;
    }
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (GoogleOAuthPkceHelper::IsWorkspaceAccountRejection(resp.httpStatus, resp.bodyUtf8)) {
            if (outWorkspaceRejected) *outWorkspaceRejected = true;
            if (outErr) *outErr = "Cuenta Workspace rechazada; requiere cuenta personal de Google.";
        } else if (outErr) {
            *outErr = SanitizeSecretFromText("Error cargando texto/Markdown (HTTP " + std::to_string(resp.httpStatus) + ").", store_.Tokens());
        }
        return false;
    }
    return ParseSourceJson(resp.bodyUtf8, outSource);
}

bool GeminiNotebookRestClient::UploadBinaryFileSource(
    const std::string& notebookResourceNameUtf8,
    const std::string& displayNameUtf8,
    const std::wstring& filePath,
    const std::string& mimeTypeUtf8,
    RemoteSourceRecord& outSource,
    std::string* outErr,
    bool* outWorkspaceRejected,
    std::function<bool()> cancelCheck)
{
    if (outWorkspaceRejected) *outWorkspaceRejected = false;
    if (!EnsureValidAccessToken(outErr)) return false;

    const std::string mime = mimeTypeUtf8.empty() ? DetectSupportedMimeType(filePath) : mimeTypeUtf8;
    if (mime.empty()) {
        if (outErr) *outErr = "Tipo de archivo no admitido para carga.";
        return false;
    }

    const std::string url = StripTrailingSlash(store_.Config().uploadBaseUrlUtf8) + "/" +
        notebookResourceNameUtf8 + "/sources?uploadType=media";
    auto hdrs = BuildAuthHeaders(false);
    hdrs.emplace_back("X-Goog-Upload-Protocol", "raw");
    hdrs.emplace_back("X-Goog-Upload-File-Name", displayNameUtf8.empty()
        ? std::filesystem::path(filePath).filename().u8string() : displayNameUtf8);
    hdrs.emplace_back("Content-Type", mime);

    const NotebookHttpResponse resp = transport_->UploadFileRaw(url, hdrs, filePath, store_.Config().httpTimeoutMs, std::move(cancelCheck));
    if (resp.httpStatus == 429) {
        limiter_->NoteRetryAfterMs(ParseRetryAfterMs(resp));
        if (outErr) *outErr = "Limite de 20 cargas/minuto alcanzado (HTTP 429).";
        return false;
    }
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (GoogleOAuthPkceHelper::IsWorkspaceAccountRejection(resp.httpStatus, resp.bodyUtf8)) {
            if (outWorkspaceRejected) *outWorkspaceRejected = true;
            if (outErr) *outErr = "Cuenta Workspace rechazada; requiere cuenta personal de Google.";
        } else if (outErr) {
            *outErr = SanitizeSecretFromText("Error subiendo archivo (HTTP " + std::to_string(resp.httpStatus) + "): " + resp.errorUtf8, store_.Tokens());
        }
        return false;
    }
    return ParseSourceJson(resp.bodyUtf8, outSource);
}

SyncExecutionSummary GeminiNotebookRestClient::ExecuteSyncPlan(
    const SyncDiffPlan& plan,
    std::function<void(std::size_t, std::size_t, const std::string&)> onProgress,
    std::function<bool()> cancelCheck,
    bool sleepOnRateLimit)
{
    SyncExecutionSummary sum{};
    if (!EnsureValidAccessToken(&sum.errorUtf8)) {
        return sum;
    }

    // 1. Ensure remote notebook exists (create if missing)
    std::string remoteNb = plan.existingRemoteNotebookNameUtf8;
    if (remoteNb.empty()) {
        RemoteNotebookRecord created{};
        bool wsRej = false;
        if (!CreateNotebook(plan.targetNotebookDisplayNameUtf8, created, &sum.errorUtf8, &wsRej)) {
            sum.workspaceAccountRejected = wsRej;
            return sum;
        }
        remoteNb = created.resourceNameUtf8;
        LocalNotebookMapping mapRec{};
        mapRec.localKeyUtf8 = plan.targetLocalNotebookKeyUtf8;
        mapRec.displayNameUtf8 = plan.targetNotebookDisplayNameUtf8;
        mapRec.remoteNotebookNameUtf8 = remoteNb;
        mapRec.webUrlUtf8 = created.webUrlUtf8;
        mapRec.lastSyncUtc = CurrentUtcIso8601();
        store_.UpsertNotebookMapping(mapRec);
        store_.SaveManifest(nullptr);
    }
    sum.remoteNotebookNameUtf8 = remoteNb;
    sum.remoteWebUrlUtf8 = BuildWebNotebookUrl(remoteNb);

    const std::size_t totalItems = plan.items.size();
    for (std::size_t i = 0; i < totalItems; ++i) {
        if (cancelCheck && cancelCheck()) {
            sum.cancelled = true;
            break;
        }
        const auto& item = plan.items[i];
        if (onProgress) {
            onProgress(i, totalItems, "Procesando: " + item.targetDisplayNameUtf8);
        }

        if (item.action == SyncDiffAction::SkipUnchanged) {
            sum.skippedCount++;
            continue;
        }
        if (item.action == SyncDiffAction::BlockedByPreFlight) {
            sum.failedCount++;
            sum.notesUtf8.push_back("Bloqueado: " + item.targetDisplayNameUtf8 + " (" + item.reasonUtf8 + ")");
            continue;
        }

        // Enforce 20 uploads/min token bucket before network call
        for (int attempt = 0; attempt < 3; ++attempt) {
            const std::uint64_t waitMs = limiter_->TryAcquireOrWaitMs();
            if (waitMs > 0) {
                if (!sleepOnRateLimit) {
                    sum.notesUtf8.push_back("Pausado por limite de 20 cargas/minuto (" + std::to_string(waitMs) + " ms).");
                    break;
                }
                const std::uint64_t stepWait = (std::min<std::uint64_t>)(waitMs, 5000ull);
                std::this_thread::sleep_for(std::chrono::milliseconds(stepWait));
                if (cancelCheck && cancelCheck()) {
                    sum.cancelled = true;
                    break;
                }
                continue;
            }

            RemoteSourceRecord uploaded{};
            std::string err;
            bool wsRej = false;
            bool ok = false;
            if (item.candidate.inputKind == SourceInputKind::WebUrl || item.candidate.inputKind == SourceInputKind::YouTubeUrl) {
                ok = CreateWebOrYouTubeSource(remoteNb, item.targetDisplayNameUtf8, item.candidate.sourceUrlUtf8, uploaded, &err, &wsRej);
            } else if (item.candidate.inputKind == SourceInputKind::InlineTextOrMarkdown) {
                ok = CreateTextOrMarkdownSource(remoteNb, item.targetDisplayNameUtf8,
                    item.candidate.inlineMarkdownUtf8, item.preflight.detectedMimeTypeUtf8, uploaded, &err, &wsRej);
            } else {
                ok = UploadBinaryFileSource(remoteNb, item.targetDisplayNameUtf8,
                    Utf8ToWide(item.candidate.localFilePathUtf8), item.preflight.detectedMimeTypeUtf8, uploaded, &err, &wsRej, cancelCheck);
            }

            if (wsRej) {
                sum.workspaceAccountRejected = true;
                sum.errorUtf8 = err;
                sum.failedCount++;
                return sum;
            }
            if (ok) {
                const std::string origin = !item.candidate.localFilePathUtf8.empty()
                    ? item.candidate.localFilePathUtf8 : item.candidate.sourceUrlUtf8;
                if (item.action == SyncDiffAction::UploadVersionedCopy) {
                    store_.MarkPriorSourcesSuperseded(plan.targetLocalNotebookKeyUtf8, origin, item.preflight.sha256Hex);
                    sum.versionedCount++;
                } else {
                    sum.uploadedCount++;
                }
                LocalSyncSourceEntry entry{};
                entry.localIdUtf8 = plan.targetLocalNotebookKeyUtf8 + ":" + item.preflight.sha256Hex.substr(0, 16);
                entry.notebookKeyUtf8 = plan.targetLocalNotebookKeyUtf8;
                entry.remoteNotebookNameUtf8 = remoteNb;
                entry.remoteSourceNameUtf8 = uploaded.resourceNameUtf8;
                entry.displayNameUtf8 = item.targetDisplayNameUtf8;
                entry.inputKind = item.candidate.inputKind;
                entry.localFilePathUtf8 = item.candidate.localFilePathUtf8;
                entry.sourceUrlUtf8 = item.candidate.sourceUrlUtf8;
                entry.mimeTypeUtf8 = item.preflight.detectedMimeTypeUtf8;
                entry.sha256Hex = item.preflight.sha256Hex;
                entry.sizeBytes = item.preflight.sizeBytes;
                entry.estimatedWords = item.preflight.estimatedWords;
                entry.tecCourseUtf8 = item.candidate.tecCourseUtf8;
                entry.chapterLabelUtf8 = ExtractChapterLabelUtf8(item.candidate.folderPathUtf8, item.candidate.displayNameUtf8);
                entry.materialCategoryUtf8 = DetectMaterialCategoryUtf8(item.candidate.folderPathUtf8, item.candidate.displayNameUtf8);
                entry.state = (uploaded.stateUtf8 == "PROCESSING")
                    ? SyncSourceState::ProcessingRemote : SyncSourceState::ActiveSynced;
                entry.versionNumber = item.nextVersionNumber;
                entry.syncedAtUtc = CurrentUtcIso8601();
                store_.UpsertSourceEntry(entry);
                store_.SaveManifest(nullptr);
                break;
            }

            // Retry only if rate limited (429); otherwise record failure immediately
            if (err.find("429") != std::string::npos && attempt < 2) {
                continue;
            }
            sum.failedCount++;
            sum.notesUtf8.push_back("Error en " + item.targetDisplayNameUtf8 + ": " + err);
            break;
        }
    }

    if (onProgress) onProgress(totalItems, totalItems, "Sincronizacion finalizada");

    if (auto* existingNb = const_cast<LocalNotebookMapping*>(store_.FindNotebookByLocalKey(plan.targetLocalNotebookKeyUtf8))) {
        existingNb->lastSyncUtc = CurrentUtcIso8601();
    }
    SyncHistoryRecord hist{};
    hist.timestampUtc = CurrentUtcIso8601();
    hist.notebookDisplayNameUtf8 = plan.targetNotebookDisplayNameUtf8;
    hist.uploadedCount = sum.uploadedCount;
    hist.skippedUnchangedCount = sum.skippedCount;
    hist.versionedUpdateCount = sum.versionedCount;
    hist.failedCount = sum.failedCount;
    hist.summaryUtf8 = std::to_string(sum.uploadedCount) + " nuevas, " +
        std::to_string(sum.versionedCount) + " versionadas, " +
        std::to_string(sum.skippedCount) + " sin cambios, " +
        std::to_string(sum.failedCount) + " fallos";
    store_.AppendHistory(hist);
    store_.SaveManifest(nullptr);

    sum.ok = (sum.failedCount == 0 && !sum.cancelled);
    return sum;
}

bool GeminiNotebookRestClient::QueryNotebookRemote(
    const std::string&, const std::string&, std::string& outAnswerUtf8, std::string* outErr)
{
    outAnswerUtf8.clear();
    if (!caps_.queryNotebookApi) {
        if (outErr) *outErr = "Pendiente en la API oficial de Gemini Notebook. Usa la consulta IA local/cloud de CuadernU sobre las fuentes sincronizadas.";
        return false;
    }
    return false;
}

bool GeminiNotebookRestClient::GenerateArtifactRemote(
    const std::string&, const std::string&, std::string& outArtifactJsonUtf8, std::string* outErr)
{
    outArtifactJsonUtf8.clear();
    if (!caps_.generateArtifactsApi) {
        if (outErr) *outErr = "La generacion de artefactos por API esta pendiente en la hoja de ruta oficial de Google. Usa el boton Abrir en la web o las herramientas de estudio de CuadernU.";
        return false;
    }
    return false;
}

bool GeminiNotebookRestClient::DeleteRemoteSource(const std::string&, std::string* outErr) {
    if (!caps_.deleteNotebookOrSourceApi) {
        if (outErr) *outErr = "La eliminacion remota de fuentes aun no esta habilitada en la version preliminar oficial.";
        return false;
    }
    return false;
}

// ---------------- GeminiNotebookMcpClient ----------------

GeminiNotebookMcpClient::GeminiNotebookMcpClient(GeminiNotebookSyncStore& store, INotebookHttpTransport* transport)
    : store_(store)
{
    if (transport) {
        transport_ = transport;
    } else {
        ownedTransport_ = std::make_unique<WinHttpNotebookTransport>();
        transport_ = ownedTransport_.get();
    }
}

bool GeminiNotebookMcpClient::SendRpc(
    const std::string& methodUtf8,
    const std::string& paramsJsonUtf8,
    std::string& outResultJsonUtf8,
    std::string* outErr)
{
    outResultJsonUtf8.clear();
    {
        auto& tok = store_.MutableTokens();
        const std::int64_t now = NowEpochSeconds();
        if (!tok.HasUsableAccessToken(now) && !tok.refreshTokenUtf8.empty()) {
            if (GoogleOAuthPkceHelper::RefreshAccessToken(store_.Config(), *transport_, tok, outErr)) {
                store_.SaveTokensDpapi(nullptr);
            }
        }
    }
    if (store_.Tokens().accessTokenUtf8.empty()) {
        if (outErr) *outErr = "No hay token OAuth activo para conectar al servidor MCP de Google.";
        return false;
    }
    json req = {
        { "jsonrpc", "2.0" },
        { "id", nextId_++ },
        { "method", methodUtf8 }
    };
    if (!paramsJsonUtf8.empty()) {
        try { req["params"] = json::parse(paramsJsonUtf8); }
        catch (...) { req["params"] = json::object(); }
    } else {
        req["params"] = json::object();
    }

    std::vector<std::pair<std::string, std::string>> hdrs = {
        { "Authorization", "Bearer " + store_.Tokens().accessTokenUtf8 },
        { "Content-Type", "application/json" },
        { "Accept", "application/json, text/event-stream" }
    };
    if (!store_.Config().projectIdUtf8.empty()) {
        hdrs.emplace_back("x-goog-user-project", store_.Config().projectIdUtf8);
    }
    if (!sessionId_.empty()) {
        hdrs.emplace_back("Mcp-Session-Id", sessionId_);
    }

    const std::string body = req.dump();
    const NotebookHttpResponse resp = transport_->SendJson("POST", store_.Config().mcpEndpointUrlUtf8, hdrs, &body, store_.Config().httpTimeoutMs);
    if (!resp.transportOk || resp.httpStatus < 200 || resp.httpStatus >= 300) {
        if (outErr) {
            *outErr = SanitizeSecretFromText("Error MCP (HTTP " + std::to_string(resp.httpStatus) + ").", store_.Tokens());
        }
        return false;
    }
    auto sidIt = resp.headersLower.find("mcp-session-id");
    if (sidIt != resp.headersLower.end() && !sidIt->second.empty()) {
        sessionId_ = sidIt->second;
    }

    // Parse either direct application/json or SSE ("data: {...}")
    std::string jsonPayload = TrimAscii(resp.bodyUtf8);
    if (jsonPayload.rfind("data:", 0) == 0 || jsonPayload.find("\ndata:") != std::string::npos) {
        std::istringstream iss(jsonPayload);
        std::string line, lastData;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("data:", 0) == 0) {
                const std::string chunk = TrimAscii(line.substr(5));
                if (!chunk.empty() && chunk != "[DONE]") lastData = chunk;
            }
        }
        if (!lastData.empty()) jsonPayload = lastData;
    }

    try {
        const json j = json::parse(jsonPayload);
        if (j.contains("error") && j["error"].is_object()) {
            if (outErr) *outErr = j["error"].value("message", "Error JSON-RPC MCP.");
            return false;
        }
        if (j.contains("result")) {
            outResultJsonUtf8 = j["result"].dump();
            return true;
        }
        outResultJsonUtf8 = j.dump();
        return true;
    } catch (...) {
        if (outErr) *outErr = "Respuesta MCP no interpretable.";
        return false;
    }
}

bool GeminiNotebookMcpClient::Initialize(std::string* outServerInfoUtf8, std::string* outErr) {
    const json params = {
        { "protocolVersion", "2025-03-26" },
        { "capabilities", json::object() },
        { "clientInfo", { { "name", "CuadernU" }, { "version", "2.0.0" } } }
    };
    std::string res;
    if (!SendRpc("initialize", params.dump(), res, outErr)) return false;
    if (outServerInfoUtf8) *outServerInfoUtf8 = res;
    return true;
}

bool GeminiNotebookMcpClient::DiscoverToolsAndCapabilities(GeminiNotebookCapabilities& inOutCaps, std::string* outErr) {
    std::string res;
    if (!SendRpc("tools/list", "{}", res, outErr)) return false;
    try {
        const json j = json::parse(res);
        inOutCaps.discoveredMcpTools.clear();
        if (j.contains("tools") && j["tools"].is_array()) {
            for (const auto& t : j["tools"]) {
                const std::string name = t.value("name", "");
                if (name.empty()) continue;
                inOutCaps.discoveredMcpTools.push_back(name);
                const std::string low = ToLowerAscii(name);
                if (low.find("query") != std::string::npos || low.find("ask") != std::string::npos) {
                    inOutCaps.queryNotebookApi = true;
                }
                if (low.find("artifact") != std::string::npos || low.find("audio_overview") != std::string::npos ||
                    low.find("summary") != std::string::npos) {
                    inOutCaps.generateArtifactsApi = true;
                }
                if (low.find("delete_source") != std::string::npos || low.find("delete_notebook") != std::string::npos) {
                    inOutCaps.deleteNotebookOrSourceApi = true;
                }
            }
        }
        return true;
    } catch (...) {
        if (outErr) *outErr = "No se pudo leer la lista de herramientas MCP.";
        return false;
    }
}

bool GeminiNotebookMcpClient::CallTool(
    const std::string& toolNameUtf8,
    const std::string& argumentsJsonUtf8,
    std::string& outResultJsonUtf8,
    std::string* outErr)
{
    json args = json::object();
    if (!argumentsJsonUtf8.empty()) {
        try { args = json::parse(argumentsJsonUtf8); } catch (...) {}
    }
    const json params = {
        { "name", toolNameUtf8 },
        { "arguments", args }
    };
    return SendRpc("tools/call", params.dump(), outResultJsonUtf8, outErr);
}

// ---------------- CuadernUMcpBridge ----------------

std::string CuadernUMcpBridge::GenerateAgentMcpConfigJson(
    const std::wstring& cuadernuExePath,
    const GeminiNotebookConfig& cfg)
{
    const std::string exeUtf8 = WideToUtf8(cuadernuExePath);
    json j = {
        { "mcpServers", {
            { "cuadernu-notebook-bridge", {
                { "command", exeUtf8.empty() ? "CuadernU.exe" : exeUtf8 },
                { "args", json::array({ "--mcp-stdio" }) }
            } },
            { "google-gemini-notebook", {
                { "url", cfg.mcpEndpointUrlUtf8.empty() ? "https://notebook.googleapis.com/mcp" : cfg.mcpEndpointUrlUtf8 },
                { "transport", "streamable-http" }
            } }
        } }
    };
    return j.dump(2);
}

std::string CuadernUMcpBridge::HandleJsonRpcRequestLine(
    const std::string& lineUtf8,
    GeminiNotebookSyncStore& store,
    INotebookHttpTransport* transportOverride)
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
        }).dump();
    }

    const std::string method = req.value("method", "");
    const bool hasId = req.contains("id") && !req["id"].is_null();
    const json idVal = hasId ? req["id"] : json(nullptr);

    if (method == "notifications/initialized" || method == "notifications/cancelled") {
        return {}; // JSON-RPC notification -> no response
    }

    auto okResp = [&](const json& result) -> std::string {
        return json({ { "jsonrpc", "2.0" }, { "id", idVal }, { "result", result } }).dump();
    };
    auto errResp = [&](int code, const std::string& msg) -> std::string {
        return json({ { "jsonrpc", "2.0" }, { "id", idVal }, { "error", { { "code", code }, { "message", msg } } } }).dump();
    };

    if (method == "initialize") {
        return okResp({
            { "protocolVersion", "2025-03-26" },
            { "capabilities", { { "tools", { { "listChanged", false } } } } },
            { "serverInfo", { { "name", "cuadernu-notebook-bridge" }, { "version", "2.0.0" } } }
        });
    }

    if (method == "tools/list") {
        json tools = json::array({
            {
                { "name", "cuadernu_notebook_status" },
                { "description", "Consulta el estado local de Gemini Notebook en CuadernU (cuadernos mapeados, fuentes, cuotas y autenticacion)." },
                { "inputSchema", { { "type", "object" }, { "properties", json::object() } } }
            },
            {
                { "name", "cuadernu_list_sync_inventory" },
                { "description", "Lista los cuadernos y fuentes sincronizadas en el almacen local independiente de CuadernU." },
                { "inputSchema", { { "type", "object" }, { "properties", { { "notebook_key", { { "type", "string" } } } } } } }
            },
            {
                { "name", "cuadernu_plan_tec_course_sync" },
                { "description", "Clasifica y genera un plan incremental (dry-run SHA-256) para materiales locales de un curso antes de subir a Gemini Notebook." },
                { "inputSchema", {
                    { "type", "object" },
                    { "properties", {
                        { "course_name", { { "type", "string" } } },
                        { "directory_path", { { "type", "string" } } },
                        { "bundle_small_notes", { { "type", "boolean" } } }
                    } },
                    { "required", json::array({ "directory_path" }) }
                } }
            },
            {
                { "name", "cuadernu_sync_sources" },
                { "description", "Sube archivos locales (PDF, DOCX, PPTX, MD, audio) o enlaces web/YouTube a Gemini Notebook respetando 200 MiB, 500k palabras y 20 cargas/min." },
                { "inputSchema", {
                    { "type", "object" },
                    { "properties", {
                        { "notebook_key", { { "type", "string" } } },
                        { "notebook_title", { { "type", "string" } } },
                        { "file_paths", { { "type", "array" }, { "items", { { "type", "string" } } } } },
                        { "urls", { { "type", "array" }, { "items", { { "type", "string" } } } } }
                    } },
                    { "required", json::array({ "notebook_key", "notebook_title" }) }
                } }
            },
            {
                { "name", "cuadernu_export_cudoc_to_markdown" },
                { "description", "Convierte un archivo .cudoc de CuadernU a Markdown limpio listo para sincronizar como fuente." },
                { "inputSchema", {
                    { "type", "object" },
                    { "properties", { { "cudoc_path", { { "type", "string" } } } } },
                    { "required", json::array({ "cudoc_path" }) }
                } }
            }
        });
        return okResp({ { "tools", std::move(tools) } });
    }

    if (method == "tools/call") {
        const json params = req.contains("params") && req["params"].is_object() ? req["params"] : json::object();
        const std::string toolName = params.value("name", "");
        const json args = params.contains("arguments") && params["arguments"].is_object() ? params["arguments"] : json::object();

        auto toolOk = [&](const json& payload) -> std::string {
            return okResp({
                { "content", json::array({ { { "type", "text" }, { "text", payload.dump(2) } } }) },
                { "isError", false }
            });
        };
        auto toolErr = [&](const std::string& msg) -> std::string {
            return okResp({
                { "content", json::array({ { { "type", "text" }, { "text", msg } } }) },
                { "isError", true }
            });
        };

        if (toolName == "cuadernu_notebook_status") {
            return toolOk({
                { "enabled", store.Config().enabled },
                { "authenticated", store.Tokens().HasAnyCredential() },
                { "workspace_warning", store.Tokens().workspaceRejectionSeen },
                { "mapped_notebooks", store.NotebookMappings().size() },
                { "tracked_sources", store.SyncedSources().size() },
                { "max_upload_bytes", store.Config().maxUploadBytes },
                { "max_words_per_source", store.Config().maxWordsPerSource },
                { "max_uploads_per_minute", store.Config().maxUploadsPerMinute }
            });
        }

        if (toolName == "cuadernu_list_sync_inventory") {
            const std::string nbFilter = args.value("notebook_key", "");
            json nbs = json::array();
            for (const auto& nb : store.NotebookMappings()) {
                if (!nbFilter.empty() && nb.localKeyUtf8 != nbFilter) continue;
                nbs.push_back({
                    { "local_key", nb.localKeyUtf8 },
                    { "display_name", nb.displayNameUtf8 },
                    { "remote_name", nb.remoteNotebookNameUtf8 },
                    { "web_url", nb.webUrlUtf8 }
                });
            }
            json srcs = json::array();
            for (const auto& s : store.SyncedSources()) {
                if (!nbFilter.empty() && s.notebookKeyUtf8 != nbFilter) continue;
                srcs.push_back({
                    { "display_name", s.displayNameUtf8 },
                    { "notebook_key", s.notebookKeyUtf8 },
                    { "sha256", s.sha256Hex },
                    { "state", SyncSourceStateToString(s.state) },
                    { "version", s.versionNumber },
                    { "chapter", s.chapterLabelUtf8 }
                });
            }
            return toolOk({ { "notebooks", nbs }, { "sources", srcs } });
        }

        if (toolName == "cuadernu_plan_tec_course_sync") {
            const std::string dirUtf8 = args.value("directory_path", "");
            const std::string courseName = args.value("course_name", "Curso TEC");
            if (dirUtf8.empty()) return toolErr("directory_path requerido.");
            const std::filesystem::path root(Utf8ToWide(dirUtf8));
            std::error_code ec;
            if (!std::filesystem::exists(root, ec)) return toolErr("Carpeta no encontrada.");

            std::vector<AcademicMaterialCandidate> cands;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                const std::string mime = DetectSupportedMimeType(entry.path().wstring());
                if (mime.empty()) continue;
                AcademicMaterialCandidate c{};
                c.inputKind = SourceInputKind::LocalFile;
                c.localFilePathUtf8 = entry.path().u8string();
                c.displayNameUtf8 = entry.path().filename().u8string();
                c.tecCourseUtf8 = courseName;
                c.folderPathUtf8 = std::filesystem::relative(entry.path().parent_path(), root, ec).u8string();
                cands.push_back(std::move(c));
            }
            const auto groups = ClassifyAcademicMaterials(cands, store.Config().recommendedMaxSourcesPerNotebook);
            const auto plan = store.ComputeIncrementalSyncPlan("tec:" + courseName, courseName, cands);
            json jGroups = json::array();
            for (const auto& g : groups) {
                jGroups.push_back({
                    { "chapter", g.chapterLabelUtf8 },
                    { "suggested_notebook", g.suggestedNotebookTitleUtf8 },
                    { "count", g.items.size() },
                    { "exceeds_50_cap", g.exceedsRecommendedCap }
                });
            }
            return toolOk({
                { "course", courseName },
                { "candidates", cands.size() },
                { "new", plan.countNew },
                { "unchanged", plan.countUnchanged },
                { "versioned", plan.countVersioned },
                { "blocked", plan.countBlocked },
                { "quota_warning_50", plan.quotaWarningExceeds50 },
                { "chapters", jGroups }
            });
        }

        if (toolName == "cuadernu_sync_sources") {
            const std::string nbKey = args.value("notebook_key", "");
            const std::string nbTitle = args.value("notebook_title", "Cuaderno CuadernU");
            if (nbKey.empty()) return toolErr("notebook_key requerido.");

            std::vector<AcademicMaterialCandidate> cands;
            if (args.contains("file_paths") && args["file_paths"].is_array()) {
                for (const auto& fp : args["file_paths"]) {
                    if (!fp.is_string()) continue;
                    AcademicMaterialCandidate c{};
                    c.inputKind = SourceInputKind::LocalFile;
                    c.localFilePathUtf8 = fp.get<std::string>();
                    c.displayNameUtf8 = std::filesystem::path(Utf8ToWide(c.localFilePathUtf8)).filename().u8string();
                    cands.push_back(std::move(c));
                }
            }
            if (args.contains("urls") && args["urls"].is_array()) {
                for (const auto& u : args["urls"]) {
                    if (!u.is_string()) continue;
                    AcademicMaterialCandidate c{};
                    c.sourceUrlUtf8 = u.get<std::string>();
                    c.inputKind = IsYouTubeUrlUtf8(c.sourceUrlUtf8) ? SourceInputKind::YouTubeUrl : SourceInputKind::WebUrl;
                    c.displayNameUtf8 = ExtractUrlDomainUtf8(c.sourceUrlUtf8);
                    cands.push_back(std::move(c));
                }
            }
            const auto plan = store.ComputeIncrementalSyncPlan(nbKey, nbTitle, cands);
            GeminiNotebookRestClient client(store, transportOverride);
            const auto sum = client.ExecuteSyncPlan(plan, nullptr, nullptr, false);
            return toolOk({
                { "ok", sum.ok },
                { "remote_notebook", sum.remoteNotebookNameUtf8 },
                { "web_url", sum.remoteWebUrlUtf8 },
                { "uploaded", sum.uploadedCount },
                { "versioned", sum.versionedCount },
                { "skipped", sum.skippedCount },
                { "failed", sum.failedCount },
                { "workspace_rejected", sum.workspaceAccountRejected },
                { "error", sum.errorUtf8 }
            });
        }

        if (toolName == "cuadernu_export_cudoc_to_markdown") {
            const std::string pUtf8 = args.value("cudoc_path", "");
            if (pUtf8.empty()) return toolErr("cudoc_path requerido.");
            DocumentModel doc;
            std::string loadErr;
            if (!CudocStorage::LoadFromFile(Utf8ToWide(pUtf8), doc, &loadErr)) {
                return toolErr("No se pudo abrir .cudoc: " + loadErr);
            }
            const std::string md = ExportDocumentToSyncMarkdown(doc);
            return toolOk({
                { "title", doc.Title() },
                { "estimated_words", EstimateWordCountFromUtf8(md) },
                { "sha256", ComputeSha256HexString(md) },
                { "markdown", md }
            });
        }

        return errResp(-32601, "Herramienta no encontrada: " + toolName);
    }

    return errResp(-32601, "Metodo no soportado: " + method);
}

int CuadernUMcpBridge::RunStdioServer(GeminiNotebookSyncStore& store) {
    store.LoadAll(nullptr);
    std::string line;
    while (std::getline(std::cin, line)) {
        const std::string resp = HandleJsonRpcRequestLine(line, store, nullptr);
        if (!resp.empty()) {
            std::cout << resp << "\n" << std::flush;
        }
    }
    return 0;
}

} // namespace DocStudio::GeminiNotebook
