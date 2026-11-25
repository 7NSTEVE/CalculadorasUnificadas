// ============================================================================
// doc_studio/tests/gemini_notebook_and_inline_url_test.cpp
// Exhaustive automated test suite (positive, negative, security, real loopback
// WinHTTP REST+MCP+OAuth, real CuadernU.exe --mcp-stdio, and DirectWrite
// inline TextBox URL rendering & hit-testing).
// ============================================================================

#include "../core/inline_url_parser.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/utf_conv.hpp"
#include "../integrations/gemini_notebook/gemini_notebook_client.hpp"
#include "../integrations/gemini_notebook/gemini_notebook_sync_store.hpp"
#include "../render/d2d_renderer.hpp"

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
#include <objbase.h>
#include <roapi.h>
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace DocStudio;
using namespace DocStudio::GeminiNotebook;
using json = nlohmann::json;

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* msg) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "[FAIL] " << msg << "\n";
    }
}

std::string ReadWholeFile(const std::filesystem::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) return {};
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

bool ContainsUtf8FourByteEmoji(const std::string& s) {
    for (std::size_t i = 0; i + 3 < s.size(); ++i) {
        if (static_cast<unsigned char>(s[i]) == 0xF0u && static_cast<unsigned char>(s[i + 1]) == 0x9Fu) {
            return true;
        }
    }
    return false;
}

std::size_t CountBlueAccentPixels(const RasterImageBgra32& img) {
    if (!img.Valid()) return 0;
    std::size_t count = 0;
    for (std::size_t i = 0; i + 3 < img.pixels.size(); i += 4) {
        const int b = img.pixels[i + 0];
        const int g = img.pixels[i + 1];
        const int r = img.pixels[i + 2];
        if (b > 140 && b > r + 45 && b >= g) ++count;
    }
    return count;
}

#ifdef _WIN32
// Real ephemeral HTTP/1.1 loopback test server for WinHTTP REST + Upload + MCP + OAuth verification
class LoopbackMockGoogleServer {
public:
    bool Start() {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        wsaStarted_ = true;
        srv_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (srv_ == INVALID_SOCKET) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (bind(srv_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
            listen(srv_, 8) == SOCKET_ERROR) {
            return false;
        }
        int len = sizeof(addr);
        getsockname(srv_, reinterpret_cast<sockaddr*>(&addr), &len);
        port_ = ntohs(addr.sin_port);
        stop_.store(false);
        th_ = std::thread([this]() { ServeLoop(); });
        return true;
    }

    void Stop() {
        stop_.store(true);
        if (srv_ != INVALID_SOCKET) {
            closesocket(srv_);
            srv_ = INVALID_SOCKET;
        }
        if (th_.joinable()) th_.join();
        if (wsaStarted_) {
            WSACleanup();
            wsaStarted_ = false;
        }
    }

    ~LoopbackMockGoogleServer() { Stop(); }

    std::string BaseUrl() const { return "http://127.0.0.1:" + std::to_string(port_); }

    std::atomic<int> uploadAttempts{ 0 };
    std::atomic<bool> sawProjectHeader{ false };
    std::atomic<bool> sawUploadProtocolRaw{ false };
    std::atomic<bool> sawUploadFileName{ false };
    std::atomic<std::size_t> lastUploadBytes{ 0 };
    std::atomic<bool> sawMcpSessionOnFollowup{ false };

private:
    void ServeLoop() {
        while (!stop_.load()) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(srv_, &rfds);
            timeval tv{ 0, 100000 };
            if (select(0, &rfds, nullptr, nullptr, &tv) <= 0) continue;
            SOCKET cli = accept(srv_, nullptr, nullptr);
            if (cli == INVALID_SOCKET) continue;
            HandleClient(cli);
            closesocket(cli);
        }
    }

    void HandleClient(SOCKET cli) {
        std::string raw;
        char buf[4096];
        std::size_t hdrEnd = std::string::npos;
        std::size_t contentLen = 0;
        while (true) {
            const int got = recv(cli, buf, sizeof(buf), 0);
            if (got <= 0) break;
            raw.append(buf, static_cast<std::size_t>(got));
            if (hdrEnd == std::string::npos) {
                hdrEnd = raw.find("\r\n\r\n");
                if (hdrEnd != std::string::npos) {
                    const std::string hdrs = raw.substr(0, hdrEnd);
                    std::istringstream hss(hdrs);
                    std::string line;
                    while (std::getline(hss, line)) {
                        std::string low = line;
                        for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (low.rfind("content-length:", 0) == 0) {
                            contentLen = static_cast<std::size_t>(std::strtoull(low.c_str() + 15, nullptr, 10));
                        }
                        if (low.find("x-goog-user-project: proj-tec-demo") != std::string::npos) {
                            sawProjectHeader.store(true);
                        }
                        if (low.find("x-goog-upload-protocol: raw") != std::string::npos) {
                            sawUploadProtocolRaw.store(true);
                        }
                        if (low.find("x-goog-upload-file-name:") != std::string::npos) {
                            sawUploadFileName.store(true);
                        }
                        if (low.find("mcp-session-id: sess-42") != std::string::npos) {
                            sawMcpSessionOnFollowup.store(true);
                        }
                    }
                }
            }
            if (hdrEnd != std::string::npos && raw.size() >= hdrEnd + 4 + contentLen) break;
        }
        if (hdrEnd == std::string::npos) return;

        const std::string firstLine = raw.substr(0, raw.find("\r\n"));
        const std::string body = raw.substr(hdrEnd + 4);
        std::string method, path;
        {
            std::istringstream lss(firstLine);
            lss >> method >> path;
        }

        int code = 200;
        std::string status = "OK";
        std::string extraHdrs;
        std::string respBody = "{}";

        if (method == "POST" && path == "/oauth/token") {
            respBody = R"({"access_token":"ya29.test_access_secret_98765","refresh_token":"1//test_refresh_secret_54321","expires_in":3600})";
        } else if (method == "POST" && path == "/v1/notebooks") {
            respBody = R"({"name":"notebooks/nb-fisica-01","display_name":"Fisica I","create_time":"2026-11-21T14:00:00Z"})";
        } else if (method == "GET" && path == "/v1/notebooks") {
            respBody = R"({"notebooks":[{"name":"notebooks/nb-fisica-01","display_name":"Fisica I"}]})";
        } else if (method == "POST" && path == "/v1/notebooks/nb-fisica-01/sources") {
            respBody = R"({"name":"notebooks/nb-fisica-01/sources/src-web-1","display_name":"Desmos","state":"ACTIVE"})";
        } else if (method == "GET" && path == "/v1/notebooks/nb-fisica-01/sources") {
            respBody = R"({"sources":[{"name":"notebooks/nb-fisica-01/sources/src-web-1","display_name":"Desmos","state":"ACTIVE"}]})";
        } else if (method == "POST" && path.rfind("/upload/v1/notebooks/nb-fisica-01/sources", 0) == 0) {
            const int att = ++uploadAttempts;
            lastUploadBytes.store(body.size());
            if (att == 1) {
                code = 429;
                status = "Too Many Requests";
                extraHdrs = "Retry-After: 1\r\n";
                respBody = R"({"error":{"code":429,"message":"Rate limit 20/min"}})";
            } else {
                respBody = R"({"name":"notebooks/nb-fisica-01/sources/src-pdf-1","display_name":"Guia_Cap1.pdf","mime_type":"application/pdf","state":"ACTIVE"})";
            }
        } else if (method == "POST" && path == "/mcp") {
            try {
                const json req = json::parse(body);
                const std::string m = req.value("method", "");
                const json id = req.value("id", 1);
                if (m == "initialize") {
                    extraHdrs = "Mcp-Session-Id: sess-42\r\n";
                    respBody = json({
                        { "jsonrpc", "2.0" }, { "id", id },
                        { "result", { { "protocolVersion", "2025-03-26" }, { "serverInfo", { { "name", "google-notebook-mcp" } } } } }
                    }).dump();
                } else if (m == "tools/list") {
                    respBody = json({
                        { "jsonrpc", "2.0" }, { "id", id },
                        { "result", { { "tools", json::array({
                            { { "name", "create_notebook" } },
                            { { "name", "list_sources" } },
                            { { "name", "query_notebook" } }
                        }) } } }
                    }).dump();
                } else if (m == "tools/call") {
                    respBody = json({
                        { "jsonrpc", "2.0" }, { "id", id },
                        { "result", { { "content", json::array({ { { "type", "text" }, { "text", "ok_mcp_call" } } }) } } }
                    }).dump();
                }
            } catch (...) {
                code = 400; status = "Bad Request";
            }
        } else {
            code = 404; status = "Not Found";
        }

        std::ostringstream out;
        out << "HTTP/1.1 " << code << " " << status << "\r\n"
            << "Content-Type: application/json\r\n"
            << extraHdrs
            << "Content-Length: " << respBody.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << respBody;
        const std::string wire = out.str();
        send(cli, wire.data(), static_cast<int>(wire.size()), 0);
    }

    SOCKET srv_ = INVALID_SOCKET;
    std::uint16_t port_ = 0;
    std::atomic<bool> stop_{ false };
    std::thread th_;
    bool wsaStarted_ = false;
};

bool RunChildStdioProcess(const std::wstring& cmdLine, const std::string& stdinData, std::string& outStdout) {
    outStdout.clear();
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE inRd = nullptr, inWr = nullptr, outRd = nullptr, outWr = nullptr;
    if (!CreatePipe(&inRd, &inWr, &sa, 0) || !CreatePipe(&outRd, &outWr, &sa, 0)) return false;
    SetHandleInformation(inWr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(outRd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = inRd;
    si.hStdOutput = outWr;
    si.hStdError = outWr;

    PROCESS_INFORMATION pi{};
    std::wstring mutCmd = cmdLine;
    const BOOL ok = CreateProcessW(nullptr, mutCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(inRd);
    CloseHandle(outWr);
    if (!ok) {
        CloseHandle(inWr);
        CloseHandle(outRd);
        return false;
    }
    DWORD written = 0;
    if (!stdinData.empty()) {
        WriteFile(inWr, stdinData.data(), static_cast<DWORD>(stdinData.size()), &written, nullptr);
    }
    CloseHandle(inWr); // Signal EOF to child stdin

    char buf[1024]{};
    DWORD got = 0;
    while (ReadFile(outRd, buf, sizeof(buf), &got, nullptr) && got > 0) {
        outStdout.append(buf, got);
    }
    WaitForSingleObject(pi.hProcess, 8000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(outRd);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exitCode == 0;
}
#endif

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RoInitialize(RO_INIT_SINGLETHREADED);
#endif
    const std::filesystem::path srcRoot = (argc >= 2)
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();
    const std::wstring cuadernuExePath = (argc >= 3) ? Utf8ToWide(argv[2]) : L"";

    std::error_code ec;
    const std::filesystem::path tmpDir = std::filesystem::temp_directory_path(ec) / L"CuadernU_GeminiNotebookTest";
    std::filesystem::remove_all(tmpDir, ec);
    std::filesystem::create_directories(tmpDir, ec);

    // =========================================================================
    // 1. INLINE CLICKABLE URLS IN TEXTBOX (PARSER + SECURITY + D2D + HIT-TEST)
    // =========================================================================
    {
        const std::string para =
            "F\xC3\xADsica \xCE\xB1: ver (https://es.wikipedia.org/wiki/F%C3%ADsica_(ciencia)), "
            "simular en www.desmos.com/calculator. Video: https://youtu.be/fNk_zzaMoSs! "
            "Contacto: mailto:docente@tec.ac.cr; inseguros: javascript:alert(1) "
            "javascript:https://evil.test file:///C:/Windows/cmd.exe vbscript:msg data:text/html,hi";

        const auto spans = ExtractSafeInlineUrls(para);
        Check(spans.size() == 4u, "ExtractSafeInlineUrls extracts exactly 4 safe URLs and rejects all 5 unsafe schemes");
        if (spans.size() == 4u) {
            Check(spans[0].normalizedUrlUtf8 == "https://es.wikipedia.org/wiki/F%C3%ADsica_(ciencia)",
                  "Preserves balanced parentheses while trimming trailing '), '");
            Check(spans[1].normalizedUrlUtf8 == "https://www.desmos.com/calculator",
                  "Normalizes boundary-delimited www. to https:// and trims trailing '.'");
            Check(spans[2].normalizedUrlUtf8 == "https://youtu.be/fNk_zzaMoSs" && spans[2].isYouTube,
                  "Detects YouTube short URL and trims trailing '!'");
            Check(spans[3].normalizedUrlUtf8 == "mailto:docente@tec.ac.cr",
                  "Accepts safe mailto: and trims trailing ';'");
            const std::wstring wPara = Utf8ToWide(para);
            Check(wPara.substr(spans[0].utf16Start, spans[0].utf16Len) == Utf8ToWide(spans[0].rawTextUtf8),
                  "Exact UTF-16 offset matches raw URL slice with Spanish/Greek Unicode prefix");
        }

        // Direct2D visual rendering + DirectWrite HitTestTextBoxInlineUrl verification
        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject tb{};
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 20.0, 20.0, 340.0, 90.0 };
        tb.textUtf8 = "Texto normal sin enlace\nhttps://www.desmos.com/calculator";
        tb.strokeColor = DocColor::RGBA(20, 24, 32, 255);
        tb.fillColor = DocColor::RGBA(255, 255, 255, 255);
        tb.fontSizePt = 14.0;
        doc.AddCanvasObject(tb);

        D2DRenderer ren;
        Camera2D cam{};
        RasterImageBgra32 imgLight, imgHc;
        ViewportOverlayState ovLight{};
        Check(ren.RenderHwndViewportToOffscreen(doc, cam, 400, 140, nullptr, imgLight, &ovLight),
              "Render TextBox with inline URL offscreen (Light)");
        Check(CountBlueAccentPixels(imgLight) > 40u, "Rendered TextBox shows accessible blue underlined inline URL pixels");

        ViewportOverlayState ovHc{}; ovHc.highContrast = true;
        Check(ren.RenderHwndViewportToOffscreen(doc, cam, 400, 140, nullptr, imgHc, &ovHc),
              "Render TextBox with inline URL offscreen (HighContrast)");
        Check(CountBlueAccentPixels(imgHc) > 40u, "Rendered TextBox shows AAA high-contrast inline URL pixels");

        InlineUrlSpan hit{};
        const bool hitLine1 = ren.HitTestTextBoxInlineUrl(tb, 60.0, 28.0, 1.0, false, hit);
        Check(!hitLine1, "HitTestTextBoxInlineUrl returns false on plain text line 1");
        const bool hitLine2 = ren.HitTestTextBoxInlineUrl(tb, 95.0, 48.0, 1.0, false, hit);
        Check(hitLine2 && hit.normalizedUrlUtf8 == "https://www.desmos.com/calculator",
              "HitTestTextBoxInlineUrl accurately hits inline URL on line 2");
    }

    // =========================================================================
    // 2. INDEPENDENT LOCAL STORE + SHA-256 + DPAPI + PRE-FLIGHT + CLASSIFIER
    // =========================================================================
    {
        Check(ComputeSha256HexString("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
              "BCrypt SHA-256 matches official NIST test vector for 'abc'");

        const std::filesystem::path courseDir = tmpDir / L"Fisica_I";
        std::filesystem::create_directories(courseDir / L"Capitulo 1", ec);
        std::filesystem::create_directories(courseDir / L"Laboratorio 2", ec);

        const std::filesystem::path pdfFile = courseDir / L"Capitulo 1" / L"Guia_Cap1.pdf";
        const std::filesystem::path mdFile = courseDir / L"Capitulo 1" / L"Resumen_Cap1.md";
        const std::filesystem::path emptyFile = courseDir / L"Capitulo 1" / L"vacio.pdf";
        const std::filesystem::path badExtFile = courseDir / L"Laboratorio 2" / L"script.exe";
        {
            std::ofstream f1(pdfFile, std::ios::binary); f1 << "%PDF-1.4\n1 0 obj<<>>endobj\n%%EOF\n";
            std::ofstream f2(mdFile, std::ios::binary);  f2 << "# Cinematica\nVelocidad media v = dx/dt.\n";
            std::ofstream f3(emptyFile, std::ios::binary); // 0 bytes
            std::ofstream f4(badExtFile, std::ios::binary); f4 << "MZ_NOT_ALLOWED";
        }

        GeminiNotebookSyncStore store((tmpDir / L"gn_store").wstring());
        store.LoadAll(nullptr);
        store.MutableConfig().projectIdUtf8 = "proj-tec-demo";
        store.MutableConfig().oauthClientIdUtf8 = "client-id-demo.apps.googleusercontent.com";
        store.MutableConfig().oauthClientSecretUtf8 = "TOP_SECRET_OAUTH_CLIENT_999";
        store.MutableTokens().accessTokenUtf8 = "ya29.TOP_SECRET_ACCESS_TOKEN_111";
        store.MutableTokens().refreshTokenUtf8 = "1//TOP_SECRET_REFRESH_TOKEN_222";
        store.MutableTokens().accountEmailUtf8 = "estudiante.personal@gmail.com";

        Check(store.SaveConfig(nullptr), "SaveConfig writes config.json atomically");
        Check(store.SaveTokensDpapi(nullptr), "SaveTokensDpapi encrypts OAuth tokens via DPAPI");
        Check(store.SaveManifest(nullptr), "SaveManifest writes sync_manifest.json atomically");

        const std::string cfgDisk = ReadWholeFile(tmpDir / L"gn_store" / L"config.json");
        const std::string manDisk = ReadWholeFile(tmpDir / L"gn_store" / L"sync_manifest.json");
        const std::string dpapiDisk = ReadWholeFile(tmpDir / L"gn_store" / L"oauth_tokens.dpapi");
        Check(cfgDisk.find("TOP_SECRET") == std::string::npos &&
              manDisk.find("TOP_SECRET") == std::string::npos &&
              dpapiDisk.find("TOP_SECRET") == std::string::npos,
              "Zero plaintext OAuth tokens or client_secret on disk across all store files");

        GeminiNotebookSyncStore reloaded((tmpDir / L"gn_store").wstring());
        Check(reloaded.LoadAll(nullptr), "Reload GeminiNotebookSyncStore from disk");
        Check(reloaded.Tokens().accessTokenUtf8 == "ya29.TOP_SECRET_ACCESS_TOKEN_111" &&
              reloaded.Tokens().refreshTokenUtf8 == "1//TOP_SECRET_REFRESH_TOKEN_222" &&
              reloaded.Config().oauthClientSecretUtf8 == "TOP_SECRET_OAUTH_CLIENT_999",
              "DPAPI roundtrip restores OAuth tokens and client secret accurately");

        const std::string scrubbed = SanitizeSecretFromText("Fail with ya29.TOP_SECRET_ACCESS_TOKEN_111", reloaded.Tokens());
        Check(scrubbed.find("TOP_SECRET") == std::string::npos && scrubbed.find("[REDACTED]") != std::string::npos,
              "SanitizeSecretFromText scrubs tokens from error messages");

        // Pre-flight negative checks (0 bytes, >200 MiB simulated cap, >500k words simulated cap, unsupported .exe, unsafe URL)
        AcademicMaterialCandidate cEmpty{}; cEmpty.localFilePathUtf8 = emptyFile.u8string();
        Check(!ValidateSourceForUpload(cEmpty, store.Config()).ok, "Pre-flight rejects 0-byte file");

        AcademicMaterialCandidate cExe{}; cExe.localFilePathUtf8 = badExtFile.u8string();
        Check(!ValidateSourceForUpload(cExe, store.Config()).ok, "Pre-flight rejects unsupported .exe extension");

        GeminiNotebookConfig tightCfg = store.Config();
        tightCfg.maxUploadBytes = 10u;
        AcademicMaterialCandidate cBig{}; cBig.localFilePathUtf8 = pdfFile.u8string();
        Check(!ValidateSourceForUpload(cBig, tightCfg).ok, "Pre-flight rejects file exceeding byte limit");

        tightCfg.maxUploadBytes = kOfficialMaxUploadBytes;
        tightCfg.maxWordsPerSource = 2u;
        AcademicMaterialCandidate cWords{};
        cWords.inputKind = SourceInputKind::InlineTextOrMarkdown;
        cWords.inlineMarkdownUtf8 = "uno dos tres cuatro cinco";
        Check(!ValidateSourceForUpload(cWords, tightCfg).ok, "Pre-flight rejects source exceeding word limit");

        AcademicMaterialCandidate cBadUrl{};
        cBadUrl.inputKind = SourceInputKind::WebUrl;
        cBadUrl.sourceUrlUtf8 = "javascript:alert(1)";
        Check(!ValidateSourceForUpload(cBadUrl, store.Config()).ok, "Pre-flight rejects javascript: URL");

        // Academic classifier + Chapter Markdown bundle + .cudoc exporter
        AcademicMaterialCandidate cPdf{};
        cPdf.inputKind = SourceInputKind::LocalFile;
        cPdf.localFilePathUtf8 = pdfFile.u8string();
        cPdf.displayNameUtf8 = "Guia_Cap1.pdf";
        cPdf.tecCourseUtf8 = "FI-1101 Fisica I";
        cPdf.folderPathUtf8 = "Capitulo 1";

        AcademicMaterialCandidate cMd{};
        cMd.inputKind = SourceInputKind::LocalFile;
        cMd.localFilePathUtf8 = mdFile.u8string();
        cMd.displayNameUtf8 = "Resumen_Cap1.md";
        cMd.tecCourseUtf8 = "FI-1101 Fisica I";
        cMd.folderPathUtf8 = "Capitulo 1";

        const auto groups = ClassifyAcademicMaterials({ cPdf, cMd });
        Check(groups.size() == 1u && groups[0].chapterLabelUtf8 == "Capitulo 1" && groups[0].items.size() == 2u,
              "ClassifyAcademicMaterials groups TEC course materials by chapter");

        const auto bundle = BuildChapterMarkdownBundle("FI-1101 Fisica I", "Capitulo 1", { cPdf, cMd });
        Check(bundle.ok && bundle.bundledFileCount == 1u && bundle.markdownUtf8.find("Cinematica") != std::string::npos,
              "BuildChapterMarkdownBundle compiles Markdown/text files with SHA-256");

        DocumentModel sampleDoc;
        sampleDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        sampleDoc.SetTitle("Apuntes Electromagnetismo");
        AnnotationObject fObj{}; fObj.kind = ObjectKind::MathBlock; fObj.formulaExprUtf8 = "F = q*E"; fObj.evaluatedResultUtf8 = "N";
        sampleDoc.AddCanvasObject(fObj);
        const std::string exportedMd = ExportDocumentToSyncMarkdown(sampleDoc);
        Check(exportedMd.find("Apuntes Electromagnetismo") != std::string::npos &&
              exportedMd.find("F = q*E") != std::string::npos,
              "ExportDocumentToSyncMarkdown exports CuadernU document cleanly");
    }

    // =========================================================================
    // 3. RATE LIMITER (<= 20/MIN) + OAUTH PKCE (RFC 7636) + WORKSPACE DETECTION
    // =========================================================================
    {
        // RFC 7636 Appendix B official test vector
        const std::string challenge = GoogleOAuthPkceHelper::ComputeCodeChallengeS256("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
        Check(challenge == "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM",
              "OAuth PKCE S256 code_challenge matches RFC 7636 official vector");

        Check(GoogleOAuthPkceHelper::IsWorkspaceAccountRejection(403, R"({"error":{"status":"FAILED_PRECONDITION","message":"Google Workspace accounts not supported"}})"),
              "Detects Google Workspace account rejection accurately");
        Check(!GoogleOAuthPkceHelper::IsWorkspaceAccountRejection(200, "ok"),
              "Does not false-positive Workspace detection on HTTP 200");

        std::uint64_t fakeClockMs = 100000ull;
        UploadRateLimiter limiter(20u, [&]() { return fakeClockMs; });
        bool all20Immediate = true;
        for (int i = 0; i < 20; ++i) {
            if (limiter.TryAcquireOrWaitMs() != 0ull) all20Immediate = false;
            fakeClockMs += 100ull;
        }
        Check(all20Immediate, "UploadRateLimiter grants 20 uploads within 60s window");
        const std::uint64_t wait21 = limiter.TryAcquireOrWaitMs();
        Check(wait21 >= 57000ull && wait21 <= 60000ull, "UploadRateLimiter throttles 21st upload accurately");
        fakeClockMs += 60001ull;
        Check(limiter.TryAcquireOrWaitMs() == 0ull, "UploadRateLimiter re-opens slot after 60s window expires");
        limiter.NoteRetryAfterMs(5000ull);
        Check(limiter.TryAcquireOrWaitMs() == 5000ull, "UploadRateLimiter honors HTTP 429 Retry-After cooldown");
    }

    // =========================================================================
    // 4. REAL WINHTTP LOOPBACK E2E (REST v1 + 429 RETRY + UPLOAD + MCP + OAUTH)
    // =========================================================================
#ifdef _WIN32
    {
        LoopbackMockGoogleServer srv;
        Check(srv.Start(), "Start real local HTTP/MCP loopback server");

        GeminiNotebookSyncStore store((tmpDir / L"e2e_store").wstring());
        store.LoadAll(nullptr);
        store.MutableConfig().apiBaseUrlUtf8 = srv.BaseUrl() + "/v1";
        store.MutableConfig().uploadBaseUrlUtf8 = srv.BaseUrl() + "/upload/v1";
        store.MutableConfig().mcpEndpointUrlUtf8 = srv.BaseUrl() + "/mcp";
        store.MutableConfig().oauthTokenEndpointUtf8 = srv.BaseUrl() + "/oauth/token";
        store.MutableConfig().projectIdUtf8 = "proj-tec-demo";
        store.MutableConfig().oauthClientIdUtf8 = "test-desktop-client";

        // 4A. Real Loopback OAuth 2.0 PKCE callback + token exchange
        WinHttpNotebookTransport winHttp;
        GeminiNotebookAuthTokens oauthTok{};
        std::string oauthErr;
        const bool oauthOk = GoogleOAuthPkceHelper::RunLoopbackAuthorization(
            store.Config(), winHttp, oauthTok, 10, &oauthErr,
            [&](const std::string& authUrl) -> bool {
                // Extract redirect_uri and state from authorize URL and simulate browser callback
                auto extractParam = [&](const std::string& key) -> std::string {
                    const std::string k = key + "=";
                    const std::size_t p = authUrl.find(k);
                    if (p == std::string::npos) return {};
                    const std::size_t e = authUrl.find('&', p + k.size());
                    return authUrl.substr(p + k.size(), e == std::string::npos ? std::string::npos : e - (p + k.size()));
                };
                const std::string stTok = extractParam("state");
                std::string redir = extractParam("redirect_uri");
                // Decode %3A and %2F minimally for localhost URL
                auto rep = [&](const std::string& a, const std::string& b) {
                    std::size_t p = 0;
                    while ((p = redir.find(a, p)) != std::string::npos) { redir.replace(p, a.size(), b); p += b.size(); }
                };
                rep("%3A", ":"); rep("%2F", "/");
                const std::string cbUrl = redir + "?code=AUTH_CODE_XYZ&state=" + stTok;
                std::thread([cbUrl]() {
                    WinHttpNotebookTransport cbHttp;
                    cbHttp.SendJson("GET", cbUrl, {}, nullptr, 4000);
                }).detach();
                return true;
            });
        Check(oauthOk && oauthTok.accessTokenUtf8 == "ya29.test_access_secret_98765" &&
              oauthTok.refreshTokenUtf8 == "1//test_refresh_secret_54321",
              "Real OAuth 2.0 PKCE loopback server completes authorization & token exchange");
        store.MutableTokens() = oauthTok;
        store.SaveTokensDpapi(nullptr);

        // 4B. Real REST v1 Incremental Sync (New -> SkipUnchanged -> Modified v2 + 429 Retry-After)
        const std::filesystem::path pdfPath = tmpDir / L"Guia_Cap1.pdf";
        { std::ofstream f(pdfPath, std::ios::binary); f << "%PDF-1.4\nContenido original fisica\n%%EOF\n"; }

        AcademicMaterialCandidate cPdf{};
        cPdf.inputKind = SourceInputKind::LocalFile;
        cPdf.localFilePathUtf8 = pdfPath.u8string();
        cPdf.displayNameUtf8 = "Guia_Cap1.pdf";
        cPdf.tecCourseUtf8 = "Fisica I";
        cPdf.folderPathUtf8 = "Capitulo 1";

        AcademicMaterialCandidate cYt{};
        cYt.inputKind = SourceInputKind::YouTubeUrl;
        cYt.sourceUrlUtf8 = "https://www.youtube.com/watch?v=fNk_zzaMoSs";
        cYt.displayNameUtf8 = "Clase Vectores";

        const auto plan1 = store.ComputeIncrementalSyncPlan("tec:FisicaI", "Fisica I", { cPdf, cYt }, "2026-11-21");
        Check(plan1.countNew == 2u && plan1.countUnchanged == 0u && plan1.countVersioned == 0u,
              "Dry-run pass 1 classifies both sources as New");

        GeminiNotebookRestClient rest(store, &winHttp);
        const auto sum1 = rest.ExecuteSyncPlan(plan1);
        Check(sum1.ok && sum1.uploadedCount == 2u && sum1.skippedCount == 0u &&
              sum1.remoteNotebookNameUtf8 == "notebooks/nb-fisica-01",
              "REST v1 ExecuteSyncPlan creates notebook, uploads PDF (recovering from HTTP 429) & adds YouTube URL");
        Check(srv.uploadAttempts.load() == 2 && srv.sawProjectHeader.load() &&
              srv.sawUploadProtocolRaw.load() && srv.sawUploadFileName.load() && srv.lastUploadBytes.load() > 20u,
              "Binary PDF upload sent raw bytes with X-Goog-Upload-Protocol, X-Goog-Upload-File-Name & x-goog-user-project");

        // Pass 2: unchanged SHA-256 -> ZERO uploads!
        const auto plan2 = store.ComputeIncrementalSyncPlan("tec:FisicaI", "Fisica I", { cPdf, cYt }, "2026-11-21");
        Check(plan2.countNew == 0u && plan2.countUnchanged == 2u, "Dry-run pass 2 skips unchanged SHA-256 (0 duplicates)");
        const auto sum2 = rest.ExecuteSyncPlan(plan2);
        Check(sum2.ok && sum2.uploadedCount == 0u && sum2.skippedCount == 2u,
              "Incremental sync pass 2 performs 0 redundant uploads");

        // Pass 3: local file modified -> safe versioned [v2 2026-11-21] upload + prior marked SupersededLocal
        { std::ofstream f(pdfPath, std::ios::binary); f << "%PDF-1.4\nContenido ACTUALIZADO v2 fisica\n%%EOF\n"; }
        const auto plan3 = store.ComputeIncrementalSyncPlan("tec:FisicaI", "Fisica I", { cPdf }, "2026-11-21");
        Check(plan3.countVersioned == 1u &&
              plan3.items[0].targetDisplayNameUtf8 == "[v2 2026-11-21] Guia_Cap1.pdf",
              "Modified local file triggers safe versioned upload instead of blind overwrite");
        const auto sum3 = rest.ExecuteSyncPlan(plan3);
        Check(sum3.ok && sum3.versionedCount == 1u, "Versioned update uploads cleanly and updates manifest");

        // Verify ListNotebooks & ListSources
        std::vector<RemoteNotebookRecord> listedNbs;
        std::vector<RemoteSourceRecord> listedSrcs;
        Check(rest.ListNotebooks(listedNbs) && listedNbs.size() == 1u, "REST ListNotebooks returns remote notebooks");
        Check(rest.ListSources("notebooks/nb-fisica-01", listedSrcs) && listedSrcs.size() == 1u, "REST ListSources returns remote sources");

        // 4C. Real Streamable HTTP MCP Client (initialize + Mcp-Session-Id + dynamic tools/list capability discovery)
        GeminiNotebookMcpClient mcp(store, &winHttp);
        Check(mcp.Initialize(), "MCP client initialize succeeds over Streamable HTTP");
        Check(mcp.SessionId() == "sess-42", "MCP client retains Mcp-Session-Id header");
        GeminiNotebookCapabilities caps{};
        Check(!caps.queryNotebookApi, "queryNotebookApi starts false before discovery");
        Check(mcp.DiscoverToolsAndCapabilities(caps) && caps.queryNotebookApi && srv.sawMcpSessionOnFollowup.load(),
              "MCP tools/list discovers remote tools and auto-enables roadmap capability flags");
        std::string toolOut;
        Check(mcp.CallTool("list_sources", "{}", toolOut) && toolOut.find("ok_mcp_call") != std::string::npos,
              "MCP tools/call executes cleanly");

        srv.Stop();
    }

    // =========================================================================
    // 5. LOCAL AGENT MCP BRIDGE (JSON SNIPPET + REAL CuadernU.exe --mcp-stdio)
    // =========================================================================
    {
        GeminiNotebookSyncStore st((tmpDir / L"mcp_bridge_store").wstring());
        st.LoadAll(nullptr);
        const std::string cfgJson = CuadernUMcpBridge::GenerateAgentMcpConfigJson(L"C:\\CuadernU\\CuadernU.exe", st.Config());
        Check(cfgJson.find("cuadernu-notebook-bridge") != std::string::npos &&
              cfgJson.find("--mcp-stdio") != std::string::npos &&
              cfgJson.find("https://notebook.googleapis.com/mcp") != std::string::npos,
              "GenerateAgentMcpConfigJson emits both local bridge and official Google MCP endpoint");

        const std::string initResp = CuadernUMcpBridge::HandleJsonRpcRequestLine(
            R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})", st);
        Check(initResp.find("cuadernu-notebook-bridge") != std::string::npos, "In-process MCP bridge responds to initialize");

        const std::string listResp = CuadernUMcpBridge::HandleJsonRpcRequestLine(
            R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})", st);
        Check(listResp.find("cuadernu_plan_tec_course_sync") != std::string::npos &&
              listResp.find("cuadernu_sync_sources") != std::string::npos,
              "In-process MCP bridge exposes CuadernU sync & TEC planning tools");

        if (!cuadernuExePath.empty() && std::filesystem::exists(std::filesystem::path(cuadernuExePath), ec)) {
            const std::wstring cmd = L"\"" + cuadernuExePath + L"\" --profile-dir \"" +
                (tmpDir / L"mcp_cli_prof").wstring() + L"\" --mcp-stdio";
            const std::string reqs =
                "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n"
                "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}\n";
            std::string childOut;
            Check(RunChildStdioProcess(cmd, reqs, childOut), "Real CuadernU.exe --mcp-stdio exits cleanly with code 0");
            Check(childOut.find("cuadernu-notebook-bridge") != std::string::npos &&
                  childOut.find("cuadernu_export_cudoc_to_markdown") != std::string::npos,
                  "Real CuadernU.exe --mcp-stdio responds to initialize and tools/list over stdio");
        }
    }
#endif

    // =========================================================================
    // 6. DISCOVERABILITY & guías de interfaz HYGIENE (ZERO EMOJIS, ZERO INTERNAL JARGON)
    // =========================================================================
    {
        CommandPaletteRegistry reg;
        reg.RegisterDefaultCommands();
        Check(reg.All().size() >= 185u, "CommandPaletteRegistry exposes >= 185 discoverable commands");

        const std::filesystem::path dlgPath = srcRoot / "app" / "studio_gemini_notebook_dialog.hpp";
        const std::string dlgContent = ReadWholeFile(dlgPath);
        Check(!dlgContent.empty() && !ContainsUtf8FourByteEmoji(dlgContent),
              "Zero 4-byte emojis in studio_gemini_notebook_dialog.hpp");
        const char* bannedUiJargon[] = { "(DPAPI)", "(PKCE)", "(WinHTTP)", "(JSON-RPC)", "(SAPI)", "(WebView2)" };
        bool hasJargon = false;
        for (const char* j : bannedUiJargon) {
            if (dlgContent.find(j) != std::string::npos) hasJargon = true;
        }
        Check(!hasJargon, "Zero banned internal API jargon in studio_gemini_notebook_dialog.hpp");
    }

    std::filesystem::remove_all(tmpDir, ec);
#ifdef _WIN32
    RoUninitialize();
    CoUninitialize();
#endif

    std::cout << "GeminiNotebookAndInlineUrlTest: " << g_pass << " passed, " << g_fail << " failed.\n";
    return (g_fail == 0) ? 0 : 1;
}
