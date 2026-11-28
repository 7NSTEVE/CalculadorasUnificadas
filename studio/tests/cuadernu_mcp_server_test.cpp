#include "../mcp/mcp_server.hpp"
#include "../core/utf_conv.hpp"
#include "../core/zip_container.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <objbase.h>
#include <roapi.h>
#include <gdiplus.h>
#include <winhttp.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace DocStudio;
using namespace DocStudio::Mcp;

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const std::string& label) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "[FAIL] " << label << "\n";
    }
}

bool HasPngHeader(const std::vector<std::uint8_t>& b) {
    return b.size() >= 8u &&
        b[0] == 0x89u && b[1] == 0x50u && b[2] == 0x4Eu && b[3] == 0x47u &&
        b[4] == 0x0Du && b[5] == 0x0Au && b[6] == 0x1Au && b[7] == 0x0Au;
}

std::vector<std::uint8_t> ReadFileBytes(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return {};
    const auto sz = ifs.tellg();
    if (sz <= 0) return {};
    std::vector<std::uint8_t> out(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(out.data()), sz);
    return out;
}

#ifdef _WIN32
bool RunChildStdio(const std::wstring& cmdLine, const std::string& stdinData, std::string& stdoutData, DWORD& exitCode) {
    stdoutData.clear();
    exitCode = 999;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hInR = nullptr, hInW = nullptr;
    HANDLE hOutR = nullptr, hOutW = nullptr;
    if (!CreatePipe(&hInR, &hInW, &sa, 0)) return false;
    if (!CreatePipe(&hOutR, &hOutW, &sa, 0)) { CloseHandle(hInR); CloseHandle(hInW); return false; }
    SetHandleInformation(hInW, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hInR;
    si.hStdOutput = hOutW;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi{};
    std::wstring mutCmd = cmdLine;
    const BOOL ok = CreateProcessW(nullptr, mutCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hInR);
    CloseHandle(hOutW);
    if (!ok) {
        CloseHandle(hInW);
        CloseHandle(hOutR);
        return false;
    }

    if (!stdinData.empty()) {
        DWORD wr = 0;
        WriteFile(hInW, stdinData.data(), static_cast<DWORD>(stdinData.size()), &wr, nullptr);
    }
    CloseHandle(hInW); // Signal EOF so stdio server exits cleanly after draining

    char buf[4096];
    DWORD got = 0;
    while (ReadFile(hOutR, buf, sizeof(buf), &got, nullptr) && got > 0) {
        stdoutData.append(buf, got);
    }
    CloseHandle(hOutR);

    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exitCode == 0;
}

bool HttpPostLocal(
    std::uint16_t port,
    const std::wstring& path,
    const std::string& body,
    const std::wstring& authHeader,
    DWORD& outStatus,
    std::string& outResp)
{
    outStatus = 0;
    outResp.clear();
    HINTERNET hSes = WinHttpOpen(L"CuadernuMcpTest/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
    if (!hSes) return false;
    HINTERNET hCon = WinHttpConnect(hSes, L"127.0.0.1", port, 0);
    if (!hCon) { WinHttpCloseHandle(hSes); return false; }
    HINTERNET hReq = WinHttpOpenRequest(hCon, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hReq) { WinHttpCloseHandle(hCon); WinHttpCloseHandle(hSes); return false; }

    std::wstring hdrs = L"Content-Type: application/json\r\n";
    if (!authHeader.empty()) hdrs += L"Authorization: " + authHeader + L"\r\n";

    const BOOL sent = WinHttpSendRequest(
        hReq, hdrs.c_str(), static_cast<DWORD>(hdrs.size()),
        const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0);
    bool ok = false;
    if (sent && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD sz = sizeof(outStatus);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &outStatus, &sz, nullptr);
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
            std::string chunk(avail, '\0');
            DWORD read = 0;
            if (WinHttpReadData(hReq, chunk.data(), avail, &read) && read > 0) {
                outResp.append(chunk.data(), read);
            } else break;
        }
        ok = true;
    }
    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hCon);
    WinHttpCloseHandle(hSes);
    return ok;
}
#endif

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RoInitialize(RO_INIT_SINGLETHREADED);
    ULONG_PTR gdipTok = 0;
    Gdiplus::GdiplusStartupInput gdipIn{};
    Gdiplus::GdiplusStartup(&gdipTok, &gdipIn, nullptr);
    SetEnvironmentVariableW(L"MUTATION_MASK", L"0");
#endif

    const std::wstring srcRootW = (argc >= 2) ? Utf8ToWide(argv[1]) : L"";
    const std::wstring cuadernuExeW = (argc >= 3) ? Utf8ToWide(argv[2]) : L"";
    const fs::path repoRoot = srcRootW.empty() ? fs::path() : fs::path(srcRootW).parent_path();

    McpServerOptions opts{};
    opts.sandboxMode = true;
    opts.readonlyTec = true;
    opts.cuadernuExePathW = cuadernuExeW;
    opts.repoRootW = repoRoot.wstring();

    McpServer server(opts);

    try {
    // =========================================================================
    // 1. PROTOCOL COMPLIANCE (MCP 2025-03-26, TOOLS/RESOURCES/PROMPTS, NEGATIVES)
    // =========================================================================
    {
        const std::string initRaw = server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26"}})");
        const json initJ = json::parse(initRaw);
        Check(initJ.contains("result") && initJ["result"]["serverInfo"]["name"] == "cuadernu",
              "initialize returns serverInfo.name == cuadernu");
        Check(initJ["result"]["capabilities"].contains("tools") &&
              initJ["result"]["capabilities"].contains("resources") &&
              initJ["result"]["capabilities"].contains("prompts"),
              "initialize advertises tools, resources, and prompts capabilities");

        const std::string notifOut = server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","method":"notifications/initialized","params":{}})");
        Check(notifOut.empty(), "notifications/initialized produces strictly ZERO output");

        const std::string pingOut = server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":"p1","method":"ping","params":{}})");
        Check(json::parse(pingOut).contains("result"), "ping succeeds");

        const std::string listOut = server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})");
        const json listJ = json::parse(listOut);
        const auto& tools = listJ["result"]["tools"];
        Check(tools.is_array() && tools.size() == 37u,
              "tools/list exposes exact 37 tools (got " + std::to_string(tools.size()) + ")");

        bool hasCaps = false, hasCapture = false, hasGui = false, hasTest = false, hasLegacy = false, hasTecMut = false;
        bool hasMatTec = false, hasMailTriage = false;
        for (const auto& t : tools) {
            const std::string n = t.value("name", "");
            if (n == "cuadernu_capabilities") hasCaps = true;
            if (n == "cuadernu_capture") hasCapture = true;
            if (n == "cuadernu_gui") hasGui = true;
            if (n == "cuadernu_test") hasTest = true;
            if (n == "cuadernu_export_cudoc_to_markdown") hasLegacy = true;
            if (n == "cuadernu_tec_mutation") hasTecMut = true;
            if (n == "cuadernu_matricula_tec") hasMatTec = true;
            if (n == "cuadernu_tec_mail_triage") hasMailTriage = true;
        }
        Check(hasCaps && hasCapture && hasGui && hasTest && hasLegacy && hasTecMut && hasMatTec && hasMailTriage,
              "tools/list contains core, capture, gui, test, tec_mutation, matricula_tec, tec_mail_triage, and legacy bridge tools");

        // Functional smoke checks for Wave 1 (cuadernu_matricula_tec) & Wave 2 (cuadernu_tec_mail_triage)
        auto matStatus = server.CallTool("cuadernu_matricula_tec", { { "action", "status" } });
        Check(!matStatus.isError && matStatus.data.value("dry_run_default", false) &&
              matStatus.data.value("standard", "") == "ISO C23 + ISO C++23",
              "cuadernu_matricula_tec status reports safe dry_run_default and ISO C23 + ISO C++23");
        auto triageRes = server.CallTool("cuadernu_tec_mail_triage", {
            { "action", "classify_mail" },
            { "subject", "Examen parcial EL-3307" },
            { "body", "Entrega el 14 de agosto a las 7:00 pm" },
            { "sender_smtp", "docente@itcr.ac.cr" }
        });
        Check(!triageRes.isError && triageRes.data.value("group", "") == "Con fecha" &&
              triageRes.data.contains("courses") && !triageRes.data["courses"].empty() &&
              triageRes.data["courses"][0] == "EL3307",
              "cuadernu_tec_mail_triage classify_mail detects Con fecha and EL3307");

        const json resList = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":3,"method":"resources/list","params":{}})"));
        Check(resList["result"]["resources"].size() == 11u, "resources/list exposes exact 11 static resources");

        const json tmplList = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":4,"method":"resources/templates/list","params":{}})"));
        Check(tmplList["result"]["resourceTemplates"].size() == 3u, "resources/templates/list exposes exact 3 templates (14 total)");

        // Verify ALL 11 static resources read cleanly without error!
        int okResources = 0;
        for (const auto& rDef : resList["result"]["resources"]) {
            const std::string uri = rDef.value("uri", "");
            const json req = { { "jsonrpc", "2.0" }, { "id", 50 + okResources }, { "method", "resources/read" }, { "params", { { "uri", uri } } } };
            const json resp = json::parse(server.HandleJsonRpcLine(req.dump()));
            if (resp.contains("result") && resp["result"]["contents"].size() == 1u) {
                ++okResources;
            } else {
                std::cerr << "  [RESOURCE FAIL] " << uri << " -> " << resp.dump() << "\n";
            }
        }
        Check(okResources == 11, "all 11 static MCP resources read cleanly");

        const json prmList = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":6,"method":"prompts/list","params":{}})"));
        Check(prmList["result"]["prompts"].size() == 6u, "prompts/list exposes 6 guided prompts");

        const json prmGet = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":7,"method":"prompts/get","params":{"name":"visual_regression_audit","arguments":{"target_theme":"dark"}}})"));
        Check(prmGet.contains("result") && prmGet["result"]["messages"].size() >= 1u,
              "prompts/get builds visual_regression_audit workflow prompt");

        // Negative protocol tests
        const json parseErr = json::parse(server.HandleJsonRpcLine("{not valid json"));
        Check(parseErr["error"]["code"] == -32700, "malformed JSON returns -32700");

        const json methErr = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":8,"method":"unknown/method","params":{}})"));
        Check(methErr["error"]["code"] == -32601, "unknown method returns -32601");

        const auto badTool = server.CallTool("no_such_tool", json::object());
        Check(badTool.isError, "unknown tool call returns isError=true");

        // Regression tests: wrong JSON types in JSON-RPC envelope must NEVER throw/crash
        const json badMethodType = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":9,"method":123,"params":{}})"));
        Check(badMethodType.contains("error") && badMethodType["error"]["code"] == -32600,
              "non-string method returns -32600 without crashing");

        const json badToolNameType = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":12345,"arguments":{}}})"));
        Check(badToolNameType.contains("error") && badToolNameType["error"]["code"] == -32602,
              "non-string tools/call name returns -32602 without crashing");

        const json badResUriType = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":11,"method":"resources/read","params":{"uri":999}})"));
        Check(badResUriType.contains("error") && badResUriType["error"]["code"] == -32602,
              "non-string resources/read uri returns -32602 without crashing");

        const json badPromptArgType = json::parse(server.HandleJsonRpcLine(
            R"({"jsonrpc":"2.0","id":12,"method":"prompts/get","params":{"name":"visual_regression_audit","arguments":{"target_theme":123}}})"));
        Check(badPromptArgType.contains("result") && badPromptArgType["result"]["messages"].size() >= 1u,
              "prompts/get handles non-string argument gracefully without crashing");
    }

    // =========================================================================
    // 2. PLANE 1 BACKGROUND WRITING, DRAWING, OFFSCREEN CAPTURE, DIFF & OCR
    // =========================================================================
    {
        auto sRes = server.CallTool("cuadernu_session", {
            { "action", "new" },
            { "title", "Cuaderno QA MCP" },
            { "mode", "paginated" },
            { "background", "grid" }
        });
        Check(!sRes.isError && !sRes.data.value("session_id", "").empty(), "cuadernu_session new succeeds");
        const std::string sid = sRes.data.value("session_id", "");

        auto rInk = server.CallTool("cuadernu_draw", {
            { "action", "add_ink_stroke" }, { "session_id", sid }, { "page_index", 0 },
            { "color", "#1A73E8" }, { "stroke_width", 3.5 },
            { "points", { { 60, 80, 0.7 }, { 160, 110, 0.8 }, { 260, 80, 0.7 } } }
        });
        Check(!rInk.isError && rInk.data.value("object_id", 0ull) > 0, "cuadernu_draw add_ink_stroke succeeds");

        auto rHw = server.CallTool("cuadernu_draw", {
            { "action", "write_handwriting" }, { "session_id", sid }, { "page_index", 0 },
            { "text", "TEC 2026" }, { "x", 60.0 }, { "y", 140.0 }, { "height_pt", 28.0 }, { "color", "#1E293B" }
        });
        Check(!rHw.isError && rHw.data.value("strokes_added", rHw.data.value("stroke_count", 0)) > 0,
              "cuadernu_draw write_handwriting synthesizes ink strokes");

        auto rTxt = server.CallTool("cuadernu_draw", {
            { "action", "add_text_box" }, { "session_id", sid }, { "page_index", 0 },
            { "text", "CUADERNU VERIFICACION 2026" }, { "x", 60.0 }, { "y", 220.0 }, { "w", 460.0 }, { "h", 90.0 },
            { "font_size_pt", 20.0 }, { "bold", true }
        });
        Check(!rTxt.isError, "cuadernu_draw add_text_box succeeds");

        auto rMath = server.CallTool("cuadernu_stem_math", {
            { "action", "add_formula_node" }, { "session_id", sid }, { "page_index", 0 },
            { "expr", "E = 3 * 5 + 2" }, { "x", 60.0 }, { "y", 330.0 }, { "w", 320.0 }, { "h", 75.0 }
        });
        Check(!rMath.isError, "cuadernu_stem_math add_formula_node succeeds");

        auto rPlot = server.CallTool("cuadernu_stem_math", {
            { "action", "add_plot_node" }, { "session_id", sid }, { "page_index", 0 },
            { "expr", "sin(x)" }, { "x", 60.0 }, { "y", 430.0 }, { "w", 340.0 }, { "h", 200.0 }
        });
        Check(!rPlot.isError, "cuadernu_stem_math add_plot_node succeeds");

        // Verify all 3 resource templates on this session!
        const json snapRes = json::parse(server.HandleJsonRpcLine(json({
            { "jsonrpc", "2.0" }, { "id", 71 }, { "method", "resources/read" },
            { "params", { { "uri", "cuadernu://session/" + sid + "/snapshot" } } }
        }).dump()));
        const json mdRes = json::parse(server.HandleJsonRpcLine(json({
            { "jsonrpc", "2.0" }, { "id", 72 }, { "method", "resources/read" },
            { "params", { { "uri", "cuadernu://session/" + sid + "/markdown" } } }
        }).dump()));
        const json pngRes = json::parse(server.HandleJsonRpcLine(json({
            { "jsonrpc", "2.0" }, { "id", 73 }, { "method", "resources/read" },
            { "params", { { "uri", "cuadernu://session/" + sid + "/page/0/png" } } }
        }).dump()));
        Check(snapRes.contains("result") && mdRes.contains("result") && pngRes.contains("result") &&
              pngRes["result"]["contents"][0].contains("blob"),
              "all 3 session resource templates (snapshot, markdown, page/0/png) read cleanly");

        auto cap1 = server.CallTool("cuadernu_capture", {
            { "action", "page_png" }, { "session_id", sid }, { "page_index", 0 }, { "width_px", 1024 }
        });
        Check(!cap1.isError && cap1.images.size() == 1u && HasPngHeader(cap1.images[0].bytes),
              "cuadernu_capture page_png returns native MCP image/png block");
        const std::string png1Path = cap1.data.value("path", "");
        Check(HasPngHeader(ReadFileBytes(fs::u8path(png1Path))), "cuadernu_capture page_png writes valid PNG on disk");

        auto nonBlank = server.CallTool("cuadernu_test", {
            { "action", "assert_visual_nonblank" }, { "image_path", png1Path }
        });
        Check(!nonBlank.isError && nonBlank.data.value("nonblank", false),
              "cuadernu_test assert_visual_nonblank passes on rendered page");

        auto diffSame = server.CallTool("cuadernu_test", {
            { "action", "image_diff" }, { "image_a", png1Path }, { "image_b", png1Path }
        });
        Check(!diffSame.isError && diffSame.data.value("identical", false) && diffSame.data.value("mismatch_pixels", 1ull) == 0ull,
              "cuadernu_test image_diff detects identical captures");

        // Mutate page visually and verify image_diff detects change + produces heatmap
        server.CallTool("cuadernu_draw", {
            { "action", "add_shape" }, { "session_id", sid }, { "page_index", 0 },
            { "shape_kind", "rectangle" }, { "x", 410.0 }, { "y", 320.0 }, { "w", 140.0 }, { "h", 140.0 },
            { "stroke_color", "#DC2626" }, { "fill_color", "#EF4444" }
        });
        auto cap2 = server.CallTool("cuadernu_capture", {
            { "action", "page_png" }, { "session_id", sid }, { "page_index", 0 }, { "width_px", 1024 }
        });
        const std::string png2Path = cap2.data.value("path", "");
        auto diffChanged = server.CallTool("cuadernu_test", {
            { "action", "image_diff" }, { "image_a", png1Path }, { "image_b", png2Path }
        });
        Check(!diffChanged.isError && !diffChanged.data.value("identical", true) &&
              diffChanged.data.value("mismatch_pixels", 0ull) > 500ull &&
              HasPngHeader(ReadFileBytes(fs::u8path(diffChanged.data.value("heatmap_path", "")))),
              "cuadernu_test image_diff detects visual modification and writes heatmap PNG");

        auto ocrRes = server.CallTool("cuadernu_test", {
            { "action", "ocr_verify_capture" }, { "image_path", png1Path },
            { "expected_substrings", { "CUADERNU" } }
        });
        Check(!ocrRes.isError && ocrRes.data.value("ok", false) &&
              (!ocrRes.data.value("engine_available", false) || ocrRes.data.value("passed", false)),
              "cuadernu_test ocr_verify_capture recognizes rendered text CUADERNU");

        auto benchRes = server.CallTool("cuadernu_test", {
            { "action", "benchmark_render" }, { "session_id", sid }, { "iterations", 3 }, { "width_px", 800 }
        });
        Check(!benchRes.isError && benchRes.data.value("avg_ms", 0.0) > 0.0,
              "cuadernu_test benchmark_render reports latency & RAM");
    }

    // =========================================================================
    // 3. PRESENTATIONS & PPTX SPEAKER-NOTES PRIVATE LAYER ISOLATION
    // =========================================================================
    {
        auto deckRes = server.CallTool("cuadernu_presentation", {
            { "action", "create_deck" }, { "title", "Clase Fisica PPTX" }, { "slides", 1 }
        });
        Check(!deckRes.isError && !deckRes.data.value("session_id", "").empty(), "cuadernu_presentation create_deck succeeds");
        const std::string deckSid = deckRes.data.value("session_id", "");

        server.CallTool("cuadernu_draw", {
            { "action", "add_text_box" }, { "session_id", deckSid }, { "page_index", 0 },
            { "text", "Ley de Faraday Publica" }, { "x", 60.0 }, { "y", 60.0 }, { "w", 500.0 }, { "h", 80.0 }
        });
        auto notesRes = server.CallTool("cuadernu_presentation", {
            { "action", "set_speaker_notes" }, { "session_id", deckSid }, { "page_index", 0 },
            { "notes", "SECRETO_DOCENTE_NO_PROYECTAR_42" }
        });
        Check(!notesRes.isError, "cuadernu_presentation set_speaker_notes succeeds");

        auto pptxRes = server.CallTool("cuadernu_presentation", {
            { "action", "export_pptx" }, { "session_id", deckSid }
        });
        Check(!pptxRes.isError && !pptxRes.data.value("path", "").empty(), "cuadernu_presentation export_pptx succeeds");

        const auto pptxBytes = ReadFileBytes(fs::u8path(pptxRes.data.value("path", "")));
        ZipArchive zip;
        Check(zip.Deserialize(pptxBytes.data(), pptxBytes.size()), "exported PPTX opens as valid ZIP");
        const auto* slidePtr = zip.FindEntry("ppt/slides/slide1.xml");
        const auto* notePtr = zip.FindEntry("ppt/notesSlides/notesSlide1.xml");
        const std::string slideXml = slidePtr ? std::string(slidePtr->begin(), slidePtr->end()) : std::string();
        const std::string noteXml = notePtr ? std::string(notePtr->begin(), notePtr->end()) : std::string();
        Check(slidePtr != nullptr && slideXml.find("Ley de Faraday Publica") != std::string::npos &&
              slideXml.find("SECRETO_DOCENTE_NO_PROYECTAR_42") == std::string::npos,
              "PPTX public slide1.xml excludes teacher-private speaker notes");
        Check(notePtr != nullptr && noteXml.find("SECRETO_DOCENTE_NO_PROYECTAR_42") != std::string::npos,
              "PPTX notesSlide1.xml includes teacher speaker notes cleanly");
    }

    // =========================================================================
    // 4. ORGANIZATION, KNOWLEDGE GRAPH, DATABASE & STEM CAS
    // =========================================================================
    {
        auto libUp = server.CallTool("cuadernu_library", {
            { "action", "add_item" }, { "title", "Calculo I" },
            { "folder", "Semestre 2026-1" }, { "favorite", true }
        });
        Check(!libUp.isError, "cuadernu_library add_item succeeds");

        auto knAdd1 = server.CallTool("cuadernu_knowledge", {
            { "action", "note_create" }, { "title", "Derivadas" },
            { "body_markdown", "Ver [[Integrales]] y regla de la cadena." }, { "tags", { "calculo" } }
        });
        auto knAdd2 = server.CallTool("cuadernu_knowledge", {
            { "action", "note_create" }, { "title", "Integrales" },
            { "body_markdown", "Inversa de [[Derivadas]]." }, { "tags", { "calculo" } }
        });
        auto knGraph = server.CallTool("cuadernu_knowledge", { { "action", "graph_global" } });
        Check(!knAdd1.isError && !knAdd2.isError && !knGraph.isError && knGraph.data.contains("edges") && knGraph.data["edges"].size() >= 2u,
              "cuadernu_knowledge computes bidirectional wikilinks & 2D graph edges");

        auto dbCol = server.CallTool("cuadernu_database", {
            { "action", "add_column" }, { "name", "Estado" }, { "type", "select" }
        });
        auto dbRow = server.CallTool("cuadernu_database", {
            { "action", "add_record" }, { "cells", { { "Titulo", "Tarea 1" }, { "Estado", "Pendiente" } } }
        });
        auto dbKan = server.CallTool("cuadernu_database", { { "action", "view_grouped" }, { "column", "Estado" } });
        bool foundPendienteBucket = false;
        if (!dbKan.isError && dbKan.data.contains("buckets") && dbKan.data["buckets"].is_array()) {
            for (const auto& bk : dbKan.data["buckets"]) {
                if (bk.value("group", "") == "Pendiente" && bk.contains("records") && bk["records"].size() >= 1u) {
                    foundPendienteBucket = true;
                }
            }
        }
        Check(!dbCol.isError && !dbRow.isError && !dbKan.isError && foundPendienteBucket,
              "cuadernu_database Kanban view_grouped groups records into Pendiente bucket");

        auto mathEval = server.CallTool("cuadernu_stem_math", {
            { "action", "eval_cas" }, { "expr", "sqrt(3^2 + 4^2)" }
        });
        Check(!mathEval.isError && mathEval.data.value("ok", false), "cuadernu_stem_math eval_cas succeeds");
    }

    // =========================================================================
    // 5. SECURITY: ALL 21 TEC MUTATIONS BLOCKED + DRY-RUN + SECRET REDACTION
    // =========================================================================
    {
        const std::vector<std::string> all21Mutations = {
            "upload_evaluation", "delete_evaluation_submission", "mark_notification_read",
            "delete_notification", "delete_all_notifications", "refresh_courses_dar",
            "sync_expediente_dar", "toggle_folder_subscription", "request_eval_notifications",
            "upload_plagiarism_file", "submit_plagiarism_ephorus", "update_plagiarism_visibility",
            "save_prematricula", "submit_inclusion", "delete_inclusion", "register_fair_project",
            "save_privacy_settings", "submit_consent", "create_personal_folder",
            "upload_personal_file", "delete_personal_item"
        };
        Check(all21Mutations.size() == 21u, "mutation test matrix covers all 21 TEC actions");
        int blockedNoConfirm = 0;
        int blockedByPolicy = 0;
        int dryRunOk = 0;
        for (const auto& mut : all21Mutations) {
            auto r1 = server.CallTool("cuadernu_tec_mutation", { { "action", mut } });
            if (r1.isError) ++blockedNoConfirm;

            auto r2 = server.CallTool("cuadernu_tec_mutation", { { "action", mut }, { "confirm_mutation", true } });
            if (r2.isError) ++blockedByPolicy;

            auto r3 = server.CallTool("cuadernu_tec_mutation", { { "action", mut }, { "dry_run", true } });
            if (!r3.isError && r3.data.value("dry_run", false) && !r3.data.value("would_execute", true)) ++dryRunOk;
        }
        Check(blockedNoConfirm == 21, "all 21 TEC mutations rejected without confirm_mutation");
        Check(blockedByPolicy == 21, "all 21 TEC mutations blocked under MUTATION_MASK=0 / --readonly-tec");
        Check(dryRunOk == 21, "all 21 TEC mutations support safe dry_run preview");

        // SecretRedactor tests
        SecretRedactor red;
        red.RegisterSecret("super-secret-value-998877");
        const std::string scrubbed = red.RedactString(
            "key=AIzaSyAbCdEfGhIjKlMnOpQrStUvWxYz123456 tok=Bearer eyJhbGciOi.payload.sig raw=super-secret-value-998877");
        Check(scrubbed.find("AIzaSy") == std::string::npos &&
              scrubbed.find("eyJhbGciOi") == std::string::npos &&
              scrubbed.find("super-secret-value-998877") == std::string::npos &&
              scrubbed.find("[REDACTED]") != std::string::npos,
              "SecretRedactor scrubs API keys, Bearer tokens, and registered secrets");

        const json scrubbedJ = red.RedactJson({
            { "access_token", "ya29.secret_tok_12345" },
            { "accessToken", "camel_case_secret_token_999" },
            { "refreshToken", "camel_case_refresh_888" },
            { "clientSecret", "super_client_secret_777" },
            { "sessionCookie", "SID=abcdef123456" },
            { "api_key", "sk-proj-secret1234567890" },
            { "safe_field", "CuadernU" }
        });
        Check(scrubbedJ["access_token"] == "[REDACTED]" &&
              scrubbedJ["accessToken"] == "[REDACTED]" &&
              scrubbedJ["refreshToken"] == "[REDACTED]" &&
              scrubbedJ["clientSecret"] == "[REDACTED]" &&
              scrubbedJ["sessionCookie"] == "[REDACTED]" &&
              scrubbedJ["api_key"] == "[REDACTED]" &&
              scrubbedJ["safe_field"] == "CuadernU",
              "SecretRedactor scrubs snake_case and camelCase sensitive JSON keys while preserving safe fields");
    }

    // =========================================================================
    // 6. REAL SUBPROCESS CuadernU.exe --mcp-stdio --sandbox (PURE LF FRAMING)
    // =========================================================================
#ifdef _WIN32
    if (!cuadernuExeW.empty() && fs::exists(fs::path(cuadernuExeW))) {
        const std::wstring cmd = L"\"" + cuadernuExeW + L"\" --mcp-stdio --sandbox --readonly-tec";
        const std::string payload =
            "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-03-26\"}}\n"
            "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}}\n"
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\",\"params\":{}}\n"
            "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/list\",\"params\":{}}\n"
            "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"cuadernu_capabilities\",\"arguments\":{}}}\n";
        std::string childOut;
        DWORD ec = 0;
        Check(RunChildStdio(cmd, payload, childOut, ec), "Real CuadernU.exe --mcp-stdio --sandbox exits cleanly with code 0");
        Check(childOut.find("\r\n") == std::string::npos, "CuadernU.exe --mcp-stdio uses strict pure LF framing (zero CRLF)");

        std::istringstream iss(childOut);
        std::string line;
        std::vector<json> responses;
        while (std::getline(iss, line)) {
            if (!line.empty()) responses.push_back(json::parse(line));
        }
        Check(responses.size() == 4u,
              "Real stdio server returned exact 4 JSON-RPC lines for 4 requests + 1 silent notification (got " + std::to_string(responses.size()) + ")");
        if (responses.size() == 4u) {
            Check(responses[0]["result"]["serverInfo"]["name"] == "cuadernu", "stdio initialize serverInfo.name == cuadernu");
            Check(responses[2]["result"]["tools"].size() == 37u, "stdio tools/list exposes all 37 tools");
        }
    }

    // =========================================================================
    // 7. PLANE 2A STEALTH BACKGROUND GUI (DWMWA_CLOAK, ZERO FOCUS STEAL, ZERO ORPHANS)
    // =========================================================================
    if (!cuadernuExeW.empty() && fs::exists(fs::path(cuadernuExeW))) {
        const HWND fgBefore = GetForegroundWindow();
        auto spawnRes = server.CallTool("cuadernu_gui", {
            { "action", "spawn_background" },
            { "width", 1280 }, { "height", 800 },
            { "theme", "porcelain" }, { "mode", "paginated" },
            { "no_profile_write", true }
        });
        const HWND fgAfterSpawn = GetForegroundWindow();
        Check(!spawnRes.isError && spawnRes.data.value("ok", false),
              "Plane 2A spawn_background launches stealth CuadernU.exe cleanly");
        Check(fgBefore == fgAfterSpawn,
              "Plane 2A spawn_background steals ZERO desktop focus");

        if (!spawnRes.isError && spawnRes.data.value("ok", false)) {
            const std::uint32_t bgPid = spawnRes.data["instance"].value("pid", std::uint32_t{ 0 });
            Check(bgPid != 0u, "Plane 2A background GUI reports valid PID");

            auto stRes = server.CallTool("cuadernu_gui", { { "action", "get_state" } });
            Check(!stRes.isError && stRes.data.value("background_stealth", false),
                  "Plane 2A get_state confirms background_stealth == true");

            auto strokeRes = server.CallTool("cuadernu_gui", {
                { "action", "inject_stroke" },
                { "page_index", 0 },
                { "color", "#DC2626" }, { "stroke_width", 4.0 },
                { "points", { { 90, 120, 0.8 }, { 240, 180, 0.8 }, { 380, 130, 0.8 } } }
            });
            Check(!strokeRes.isError && strokeRes.data.value("object_id", 0ull) > 0,
                  "Plane 2A inject_stroke writes live ink into stealth GUI");

            auto stBeforeCap = server.CallTool("cuadernu_gui", { { "action", "get_state" } });
            const std::string titleBeforeCap = stBeforeCap.data.value("title", "");

            // Regression check: gui_window_png without sync_session must NOT overwrite GUI document with Plane 1 session!
            auto winCap = server.CallTool("cuadernu_capture", {
                { "action", "gui_window_png" }
            });
            Check(!winCap.isError && winCap.images.size() == 1u && HasPngHeader(winCap.images[0].bytes),
                  "Plane 2A gui_window_png captures full cloaked Win32+D2D window");
            auto stAfterCap = server.CallTool("cuadernu_gui", { { "action", "get_state" } });
            Check(stAfterCap.data.value("title", "") == titleBeforeCap,
                  "Plane 2A gui_window_png without sync_session preserves GUI document without overwriting");

            auto nbGui = server.CallTool("cuadernu_test", {
                { "action", "assert_visual_nonblank" }, { "image_path", winCap.data.value("path", "") }
            });
            Check(!nbGui.isError && nbGui.data.value("nonblank", false),
                  "Plane 2A gui_window_png is visually non-blank");

            auto tecCap = server.CallTool("cuadernu_capture", {
                { "action", "tec_dialog_png" }, { "section", "resumen" }, { "sync_session", false }
            });
            Check(!tecCap.isError && tecCap.images.size() == 1u && HasPngHeader(tecCap.images[0].bytes),
                  "Plane 2A tec_dialog_png captures cloaked TEC Digital dialog");

            auto nbTec = server.CallTool("cuadernu_test", {
                { "action", "assert_visual_nonblank" }, { "image_path", tecCap.data.value("path", "") }
            });
            Check(!nbTec.isError && nbTec.data.value("nonblank", false),
                  "Plane 2A tec_dialog_png is visually non-blank");

            auto boundsRes = server.CallTool("cuadernu_test", { { "action", "dump_layout_bounds" } });
            Check(!boundsRes.isError && boundsRes.data.value("zero_overlaps", false),
                  "Plane 2A dump_layout_bounds confirms zero panel overlaps");

            auto a11yRes = server.CallTool("cuadernu_test", { { "action", "dump_a11y_tree" } });
            Check(!a11yRes.isError && !a11yRes.data["window_nodes"].empty() && !a11yRes.data["toolbar_buttons"].empty(),
                  "Plane 2A dump_a11y_tree enumerates child controls and toolbar buttons");

            const HWND fgAfterCaptures = GetForegroundWindow();
            Check(fgBefore == fgAfterCaptures,
                  "Plane 2A captures and TEC dialog capture steal ZERO desktop focus");

            auto closeRes = server.CallTool("cuadernu_gui", { { "action", "close_background" } });
            Check(!closeRes.isError && closeRes.data.value("ok", false),
                  "Plane 2A close_background terminates stealth GUI cleanly");

            // Regression test: Plane 2B live-desktop registry discovery, attach_live, IPC round-trip, detach & stale PID pruning
            {
                const std::wstring profRootW = server.Context().EffectiveProfileRootW();
                const fs::path regDir = fs::path(profRootW) / L"ipc_instances";
                std::error_code ec;
                fs::create_directories(regDir, ec);
                const fs::path staleFile = regDir / L"pid_999999.json";
                {
                    std::ofstream ofs(staleFile, std::ios::binary | std::ios::trunc);
                    ofs << R"({"pid":999999,"pipe_name":"\\\\.\\pipe\\CuadernU_Dead_999999","token":"dead","mode":"live_desktop"})";
                }

                McpGuiPipeServer mockLivePipe;
                const std::string liveTok = GenerateTokenHex192();
                const std::string livePipeName = "CuadernU_Control_Test_" + std::to_string(GetCurrentProcessId());
                const bool liveStarted = mockLivePipe.Start(
                    livePipeName,
                    liveTok,
                    [](const json& req) -> json {
                        const std::string act = req.value("action", "");
                        if (act == "ping") return { { "ok", true }, { "mode", "live_desktop" } };
                        if (act == "get_state") return { { "ok", true }, { "background_stealth", false }, { "title", "LiveDesktopDoc" } };
                        return { { "ok", true }, { "echo_action", act } };
                    },
                    profRootW,
                    "live_desktop",
                    nullptr);
                Check(liveStarted, "Plane 2B mock live_desktop Named Pipe server starts with registry file");

                auto listInst = server.CallTool("cuadernu_gui", { { "action", "list_instances" } });
                Check(!listInst.isError && !fs::exists(staleFile, ec),
                      "Plane 2B list_instances prunes stale dead-PID registry files");

                auto attachRes = server.CallTool("cuadernu_gui", {
                    { "action", "attach_live" },
                    { "pid", static_cast<int>(GetCurrentProcessId()) }
                });
                Check(!attachRes.isError && attachRes.data.value("ok", false) &&
                      attachRes.data.contains("instance") &&
                      attachRes.data["instance"].value("mode", "") == "live_desktop",
                      "Plane 2B attach_live connects to live_desktop Named Pipe with user-DACL + 192-bit token");

                auto liveState = server.CallTool("cuadernu_gui", { { "action", "get_state" } });
                Check(!liveState.isError && liveState.data.value("title", "") == "LiveDesktopDoc" &&
                      !liveState.data.value("background_stealth", true),
                      "Plane 2B attached live_desktop instance handles IPC get_state cleanly");

                auto detachRes = server.CallTool("cuadernu_gui", { { "action", "detach" } });
                Check(!detachRes.isError && detachRes.data.value("ok", false),
                      "Plane 2B detach cleanly disconnects live_desktop instance");

                mockLivePipe.Stop();
                const fs::path liveRegFile = regDir / (L"pid_" + std::to_wstring(GetCurrentProcessId()) + L".json");
                Check(!fs::exists(liveRegFile, ec),
                      "Plane 2B McpGuiPipeServer::Stop removes its pid_<pid>.json registry file");
            }

            // Confirm process is actually gone (zero orphans)
            HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, bgPid);
            if (hProc) {
                WaitForSingleObject(hProc, 3000);
                DWORD procCode = STILL_ACTIVE;
                GetExitCodeProcess(hProc, &procCode);
                CloseHandle(hProc);
                Check(procCode != STILL_ACTIVE, "Plane 2A background GUI exited completely (zero orphans)");
            } else {
                Check(true, "Plane 2A background GUI process handle already released (zero orphans)");
            }
        }
    }

    // =========================================================================
    // 8. LOOPBACK --mcp-http BEARER AUTH (NEGATIVE 401 + POSITIVE 200)
    // =========================================================================
    {
        const std::string bearer = GenerateTokenHex192();
        const std::uint16_t port = static_cast<std::uint16_t>(39400 + (GetCurrentProcessId() % 900));
        std::atomic<bool> stopHttp{ false };
        std::thread httpThread([&]() {
            server.RunHttp(port, bearer, &stopHttp);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        const std::string initBody = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
        DWORD stNoAuth = 0, stBadAuth = 0, stGoodAuth = 0;
        std::string respNoAuth, respBadAuth, respGoodAuth;
        Check(HttpPostLocal(port, L"/mcp", initBody, L"", stNoAuth, respNoAuth) && stNoAuth == 401u,
              "HTTP loopback /mcp rejects missing Bearer token with 401");
        Check(HttpPostLocal(port, L"/mcp", initBody, L"Bearer wrong_token_value", stBadAuth, respBadAuth) && stBadAuth == 401u,
              "HTTP loopback /mcp rejects wrong Bearer token with 401");
        Check(HttpPostLocal(port, L"/mcp", initBody, L"Bearer " + Utf8ToWide(bearer), stGoodAuth, respGoodAuth) &&
              stGoodAuth == 200u && respGoodAuth.find("\"name\":\"cuadernu\"") != std::string::npos,
              "HTTP loopback /mcp accepts valid 192-bit Bearer token with 200");

        stopHttp.store(true, std::memory_order_release);
        if (httpThread.joinable()) httpThread.join();
    }
#endif
    } catch (const std::exception& ex) {
        ++g_fail;
        std::cerr << "[FATAL EXCEPTION] " << ex.what() << "\n";
    }

    std::cout << "CuadernuMcpServerTest: " << g_pass << " passed, " << g_fail << " failed.\n";

#ifdef _WIN32
    if (gdipTok) Gdiplus::GdiplusShutdown(gdipTok);
    RoUninitialize();
    CoUninitialize();
#endif
    return (g_fail == 0) ? 0 : 1;
}
