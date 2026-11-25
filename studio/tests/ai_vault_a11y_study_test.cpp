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
#pragma comment(lib, "ws2_32.lib")
#endif

#include "../core/accessibility_engine.hpp"
#include "../core/ai_provider_vault.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/command_stack.hpp"
#include "../core/document_model.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/structured_database.hpp"
#include "../core/studio_profile_store.hpp"
#include "../core/study_assistant_tools.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../render/d2d_renderer.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace DocStudio;
namespace fs = std::filesystem;

namespace {

int gPassed = 0;
int gFailed = 0;

void Check(bool cond, const char* msg) {
    if (cond) {
        ++gPassed;
    } else {
        ++gFailed;
        std::cerr << "[FAIL] " << msg << "\n";
    }
}

std::size_t CountDifferentPixels(const RasterImageBgra32& a, const RasterImageBgra32& b, int threshold = 6) {
    if (a.width != b.width || a.height != b.height || a.pixels.size() != b.pixels.size()) return 0;
    std::size_t diff = 0;
    for (std::size_t i = 0; i + 3 < a.pixels.size(); i += 4) {
        const int db = std::abs(static_cast<int>(a.pixels[i + 0]) - static_cast<int>(b.pixels[i + 0]));
        const int dg = std::abs(static_cast<int>(a.pixels[i + 1]) - static_cast<int>(b.pixels[i + 1]));
        const int dr = std::abs(static_cast<int>(a.pixels[i + 2]) - static_cast<int>(b.pixels[i + 2]));
        if (db + dg + dr >= threshold) ++diff;
    }
    return diff;
}

#ifdef _WIN32
// Minimal single-use local loopback HTTP server for deterministic multi-provider failover & /models verification
class LoopbackMockAiServer {
public:
    bool Start() {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        wsaInit_ = true;
        listenSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSock_ == INVALID_SOCKET) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (bind(listenSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
        int len = sizeof(addr);
        if (getsockname(listenSock_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) return false;
        port_ = ntohs(addr.sin_port);
        if (listen(listenSock_, 8) != 0) return false;

        running_.store(true);
        worker_ = std::thread([this]() { ServeLoop(); });
        return true;
    }

    ~LoopbackMockAiServer() {
        Stop();
    }

    void Stop() {
        running_.store(false);
        if (listenSock_ != INVALID_SOCKET) {
            closesocket(listenSock_);
            listenSock_ = INVALID_SOCKET;
        }
        if (worker_.joinable()) worker_.join();
        if (wsaInit_) {
            WSACleanup();
            wsaInit_ = false;
        }
    }

    std::uint16_t Port() const noexcept { return port_; }
    int HitsFail() const noexcept { return hitsFail_.load(); }
    int HitsOk() const noexcept { return hitsOk_.load(); }

private:
    void ServeLoop() {
        while (running_.load()) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(listenSock_, &rfds);
            timeval tv{ 0, 100000 };
            const int sel = select(0, &rfds, nullptr, nullptr, &tv);
            if (sel <= 0) continue;
            SOCKET client = accept(listenSock_, nullptr, nullptr);
            if (client == INVALID_SOCKET) continue;

            DWORD rcvTo = 1000;
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rcvTo), sizeof(rcvTo));
            char buf[8192]{};
            const int got = recv(client, buf, sizeof(buf) - 1, 0);
            if (got > 0) {
                const std::string req(buf, static_cast<std::size_t>(got));
                std::string status = "200 OK";
                std::string body = "{}";
                if (req.find("POST /fail") != std::string::npos) {
                    hitsFail_.fetch_add(1);
                    status = "429 Too Many Requests";
                    body = R"({"error":{"message":"rate limit simulated"}})";
                } else if (req.find("POST /ok") != std::string::npos) {
                    hitsOk_.fetch_add(1);
                    body = R"({"choices":[{"message":{"content":"FAILOVER_VERIFIED_OK"}}]})";
                } else if (req.find("GET /models") != std::string::npos) {
                    body = R"({"data":[{"id":"loopback-flash-1"},{"id":"loopback-pro-2"}]})";
                }
                const std::string resp =
                    "HTTP/1.1 " + status + "\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: " + std::to_string(body.size()) + "\r\n"
                    "Connection: close\r\n\r\n" + body;
                send(client, resp.data(), static_cast<int>(resp.size()), 0);
            }
            closesocket(client);
        }
    }

    bool wsaInit_ = false;
    SOCKET listenSock_ = INVALID_SOCKET;
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{ false };
    std::atomic<int> hitsFail_{ 0 };
    std::atomic<int> hitsOk_{ 0 };
    std::thread worker_;
};
#endif

// Controlled live provisioning & sanitized verification using process env vars (NEVER hardcoded)
int RunProvisionUserVaultMode() {
    AiProviderVault vault;
    const std::wstring rootDir = StudioProfileStore::DefaultProfileRootDir();
    vault.LoadFromDirectory(rootDir);

    const AiKeyImportSummary sum = vault.ImportFromProcessEnvironment();
    std::cout << "[PROVISION] Imported from process environment: " << sum.importedCount << " provider(s)\n";
    if (sum.importedCount == 0) {
        std::cerr << "[PROVISION] No env keys found (set GEMINI_API_KEY / NVIDIA_API_KEY in process env).\n";
        return 2;
    }

    vault.SetEnabled(true);
    vault.SetActiveProviderId("google_gemini");
    vault.SetFailoverProviderId("nvidia_nim");

    bool geminiOk = false;
    bool nvidiaOk = false;

    auto probeProvider = [&](const std::string& pid, const std::vector<std::string>& preferredModels, bool& outOk) {
        auto* p = vault.FindProviderMutable(pid);
        if (!p || p->apiKeyUtf8.empty()) {
            std::cout << "[LIVE] " << pid << ": SKIPPED (no key)\n";
            return;
        }
        std::vector<std::string> models;
        std::string mErr;
        int mStatus = 0;
        const bool discOk = AiProviderVault::FetchAvailableModels(*p, models, &mErr, &mStatus);
        std::cout << "[LIVE] " << p->displayNameUtf8 << " /models -> ok=" << (discOk ? "true" : "false")
                  << " http=" << mStatus << " count=" << models.size();
        if (!discOk && !mErr.empty()) std::cout << " err=" << mErr;
        std::cout << "\n";

        if (discOk && !models.empty()) {
            p->suggestedModelsUtf8.clear();
            for (std::size_t i = 0; i < models.size() && i < 24u; ++i) p->suggestedModelsUtf8.push_back(models[i]);
            bool picked = false;
            for (const auto& pref : preferredModels) {
                if (std::find(models.begin(), models.end(), pref) != models.end()) {
                    p->modelNameUtf8 = pref;
                    picked = true;
                    break;
                }
            }
            if (!picked) {
                // Pick first chat/flash/instruct model if available
                for (const auto& m : models) {
                    if (m.find("flash") != std::string::npos || m.find("instruct") != std::string::npos) {
                        p->modelNameUtf8 = m;
                        picked = true;
                        break;
                    }
                }
                if (!picked) p->modelNameUtf8 = models.front();
            }
        }

        const std::string origModel = p->modelNameUtf8;
        AiQueryResult qr = AiProviderVault::QuerySingleProvider(*p, "Responde brevemente en espanol.", "Responde exactamente: CONEXION_OK");
        if (!qr.ok && pid == "google_gemini") {
            for (const auto& alt : preferredModels) {
                if (alt == p->modelNameUtf8) continue;
                p->modelNameUtf8 = alt;
                qr = AiProviderVault::QuerySingleProvider(*p, "Responde brevemente.", "Responde exactamente: CONEXION_OK");
                if (qr.ok) break;
            }
            if (!qr.ok) p->modelNameUtf8 = origModel;
        }
        std::cout << "[LIVE] " << p->displayNameUtf8 << " inference (model=" << p->modelNameUtf8 << ") -> ok="
                  << (qr.ok ? "true" : "false") << " http=" << qr.httpStatus << " latency=" << qr.latencyMs << "ms";
        if (qr.ok) {
            std::string preview = qr.replyTextUtf8.substr(0, 64);
            for (char& ch : preview) if (ch == '\r' || ch == '\n') ch = ' ';
            std::cout << " reply=\"" << preview << "\"\n";
            outOk = true;
        } else {
            std::cout << " err=\"" << qr.errorUtf8 << "\"\n";
        }
    };

    probeProvider("google_gemini", { "gemini-2.5-flash", "gemini-3.5-flash", "gemini-3.8-flash", "gemini-2.0-flash" }, geminiOk);
    probeProvider("nvidia_nim", { "nvidia/nemotron-3.5-bolt-e4b", "minimaxai/minimax-m3.1-flash", "google/gemma-4-31b-it" }, nvidiaOk);

    std::string saveErr;
    const bool saved = vault.SaveToDirectory(rootDir, &saveErr);
    std::cout << "[PROVISION] DPAPI SaveToDirectory -> " << (saved ? "CONFIRMED" : ("FAILED: " + saveErr)) << "\n";

    // Reload from DPAPI to prove encrypted roundtrip on real user profile
    AiProviderVault reloadVault;
    const bool loaded = reloadVault.LoadFromDirectory(rootDir);
    const auto* gP = reloadVault.FindProvider("google_gemini");
    const auto* nP = reloadVault.FindProvider("nvidia_nim");
    const bool hasBoth = loaded && gP && !gP->apiKeyUtf8.empty() && nP && !nP->apiKeyUtf8.empty();
    std::cout << "[PROVISION] DPAPI Reload check -> " << (hasBoth ? "CONFIRMED (both keys decrypted)" : "FAILED") << "\n";

    // Also update ui_settings.json metadata (zero secrets!) so CuadernU startup shows active LLM ready
    StudioUiPreferences uiPrefs{};
    StudioProfileStore::LoadUiPreferences((fs::path(rootDir) / L"ui_settings.json").wstring(), uiPrefs);
    uiPrefs.llmConfig.enabled = true;
    if (gP) {
        uiPrefs.llmConfig.endpointUrlUtf8 = gP->endpointUrlUtf8;
        uiPrefs.llmConfig.modelNameUtf8 = gP->modelNameUtf8;
    }
    StudioProfileStore::SaveUiPreferences((fs::path(rootDir) / L"ui_settings.json").wstring(), uiPrefs);

    std::cout << "[PROVISION] Live inference summary: geminiOk=" << (geminiOk ? "true" : "false")
              << " nvidiaOk=" << (nvidiaOk ? "true" : "false") << "\n";
    return (saved && hasBoth) ? 0 : 1;
}

void RunAiVaultAndFailoverTests() {
    const fs::path tmpDir = fs::temp_directory_path() / L"cuadernu_ai_vault_test";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);

    // 1. Catalog & prefix auto-detection
    AiProviderVault v;
    Check(v.Providers().size() >= 13u, "Catalog has 13 built-in providers");
    Check(AiProviderVault::DetectProviderFromKeyPrefix("AQ.SyntheticDemoToken123456") == AiProviderKind::GoogleGemini, "Detect AQ. -> GoogleGemini");
    Check(AiProviderVault::DetectProviderFromKeyPrefix("AIzaSyntheticDemoToken123") == AiProviderKind::GoogleGemini, "Detect AIza -> GoogleGemini");
    Check(AiProviderVault::DetectProviderFromKeyPrefix("nvapi-SyntheticDemoToken123") == AiProviderKind::NvidiaNim, "Detect nvapi- -> NvidiaNim");
    Check(AiProviderVault::DetectProviderFromKeyPrefix("sk-ant-SyntheticDemoToken") == AiProviderKind::Anthropic, "Detect sk-ant- -> Anthropic");
    Check(AiProviderVault::DetectProviderFromKeyPrefix("sk-or-SyntheticDemoToken") == AiProviderKind::OpenRouter, "Detect sk-or- -> OpenRouter");
    Check(AiProviderVault::DetectProviderFromKeyPrefix("gsk_SyntheticDemoToken") == AiProviderKind::Groq, "Detect gsk_ -> Groq");
    Check(AiProviderVault::DetectProviderFromKeyPrefix("sk-SyntheticDemoToken") == AiProviderKind::OpenAI, "Detect sk- -> OpenAI");

    // 2. Masking
    Check(AiProviderVault::MaskApiKeyForDisplay("") == "(Sin clave)", "Mask empty key");
    const std::string synKey = "nvapi-SYNTHETIC-TEST-KEY-9999";
    const std::string masked = AiProviderVault::MaskApiKeyForDisplay(synKey);
    Check(masked.find("SYNTHETIC") == std::string::npos && masked.rfind("nvapi-", 0) == 0 && masked.find("9999") != std::string::npos,
          "Mask hides middle secret cleanly");

    // 3. .env and JSON (including OpenCode auth.json) importers
    const std::string sampleEnv =
        "# Sample synthetic .env\n"
        "export GEMINI_API_KEY=\"AQ.SyntheticGemini001122\"\n"
        "NVIDIA_API_KEY='nvapi-SyntheticNvidia334455'\n"
        "ANTHROPIC_API_KEY=sk-ant-SyntheticClaude6677 # inline comment\n"
        "OPENCODE_API_KEY=oc-SyntheticOpenCode8899\n";
    const auto envSum = v.ImportFromEnvContent(sampleEnv);
    Check(envSum.importedCount == 4u, "ImportFromEnvContent imports 4 providers");
    Check(v.FindProvider("google_gemini")->apiKeyUtf8 == "AQ.SyntheticGemini001122", "Gemini key parsed cleanly");
    Check(v.FindProvider("nvidia_nim")->apiKeyUtf8 == "nvapi-SyntheticNvidia334455", "NVIDIA key parsed cleanly");
    Check(v.FindProvider("anthropic")->apiKeyUtf8 == "sk-ant-SyntheticClaude6677", "Anthropic key parsed cleanly");
    Check(v.FindProvider("opencode")->apiKeyUtf8 == "oc-SyntheticOpenCode8899", "OpenCode key parsed cleanly");

    AiProviderVault vJson;
    const std::string openCodeJson = R"({
        "google": { "type": "api", "key": "AQ.JsonKeyGemini" },
        "nvidia": { "key": "nvapi-JsonKeyNvidia", "model": "meta/llama-3.3-70b-instruct" },
        "openai": "sk-JsonKeyOpenAI"
    })";
    const auto jSum = vJson.ImportFromJsonContent(openCodeJson);
    Check(jSum.importedCount == 3u, "ImportFromJsonContent handles OpenCode + flat JSON");
    Check(vJson.ImportFromJsonContent("{invalid json").importedCount == 0u, "Invalid JSON negative test");

    // 4. DPAPI Save/Load + zero plaintext on disk + tampered blob negative test
    std::string err;
    Check(v.SaveToDirectory(tmpDir.wstring(), &err), "DPAPI SaveToDirectory succeeds");
    {
        std::ifstream mfs(tmpDir / L"ai_providers.json", std::ios::binary);
        const std::string metaRaw((std::istreambuf_iterator<char>(mfs)), std::istreambuf_iterator<char>());
        Check(metaRaw.find("SyntheticGemini") == std::string::npos, "ai_providers.json has ZERO Gemini plaintext");
        Check(metaRaw.find("SyntheticNvidia") == std::string::npos, "ai_providers.json has ZERO NVIDIA plaintext");
        Check(metaRaw.find("SyntheticClaude") == std::string::npos, "ai_providers.json has ZERO Anthropic plaintext");
    }
    {
        std::ifstream kfs(tmpDir / L"ai_keys.dpapi", std::ios::binary);
        const std::string encRaw((std::istreambuf_iterator<char>(kfs)), std::istreambuf_iterator<char>());
        Check(!encRaw.empty(), "ai_keys.dpapi non-empty");
        Check(encRaw.find("SyntheticGemini") == std::string::npos, "ai_keys.dpapi is encrypted (no plaintext key)");
    }

    AiProviderVault loaded;
    Check(loaded.LoadFromDirectory(tmpDir.wstring(), &err), "DPAPI LoadFromDirectory succeeds");
    Check(loaded.FindProvider("google_gemini")->apiKeyUtf8 == "AQ.SyntheticGemini001122", "DPAPI roundtrip Gemini");
    Check(loaded.FindProvider("nvidia_nim")->apiKeyUtf8 == "nvapi-SyntheticNvidia334455", "DPAPI roundtrip NVIDIA");

    // Negative DPAPI test: corrupt bytes must fail decryption
    {
        std::vector<std::uint8_t> plain{ 's', 'e', 'c', 'r', 'e', 't' };
        std::vector<std::uint8_t> enc, dec;
        Check(AiProviderVault::ProtectBytesDpapi(plain, enc), "ProtectBytesDpapi positive");
        enc[enc.size() / 2] ^= 0xFFu;
        Check(!AiProviderVault::UnprotectBytesDpapi(enc, dec), "Tampered DPAPI blob rejected");
    }

    // 5. Wire format builders & response parsers
    {
        std::string url, body, parsed;
        std::vector<std::pair<std::string, std::string>> hdrs;
        Check(AiProviderVault::BuildProviderChatRequest(*v.FindProvider("google_gemini"), "Sys", "Hola", url, hdrs, body), "Gemini wire request built");
        Check(url.find(":generateContent") != std::string::npos && !hdrs.empty() && hdrs[0].first == "x-goog-api-key", "Gemini uses x-goog-api-key header");
        Check(AiProviderVault::ParseProviderChatResponse(AiProviderKind::GoogleGemini,
            R"({"candidates":[{"content":{"parts":[{"text":"Hola desde Gemini"}]}}]})", parsed) && parsed == "Hola desde Gemini", "Gemini response parser");

        Check(AiProviderVault::BuildProviderChatRequest(*v.FindProvider("anthropic"), "Sys", "Hola", url, hdrs, body), "Anthropic wire request built");
        Check(hdrs.size() >= 2u && hdrs[0].first == "x-api-key" && hdrs[1].first == "anthropic-version", "Anthropic native headers");
        Check(AiProviderVault::ParseProviderChatResponse(AiProviderKind::Anthropic,
            R"({"content":[{"type":"text","text":"Hola Claude"}]})", parsed) && parsed == "Hola Claude", "Anthropic response parser");

        std::vector<std::string> gModels;
        Check(AiProviderVault::ParseProviderModelsResponse(AiProviderKind::GoogleGemini,
            R"({"models":[{"name":"models/gemini-3.5-flash","supportedGenerationMethods":["generateContent"]},{"name":"models/embedding-001","supportedGenerationMethods":["embedContent"]}]})",
            gModels) && gModels.size() == 1u && gModels[0] == "gemini-3.5-flash", "Gemini models parser filters generateContent");
    }

    // 6. Real Winsock2 Loopback HTTP failover (Primary HTTP 429 -> Failover HTTP 200) & /models discovery
#ifdef _WIN32
    LoopbackMockAiServer mockSrv;
    Check(mockSrv.Start(), "Loopback mock AI HTTP server started");
    if (mockSrv.Port() != 0) {
        AiProviderVault fv;
        fv.SetEnabled(true);
        fv.SetActiveProviderId("nvidia_nim");
        fv.SetFailoverProviderId("openai");
        auto* p1 = fv.FindProviderMutable("nvidia_nim");
        auto* p2 = fv.FindProviderMutable("openai");
        p1->endpointUrlUtf8 = "http://127.0.0.1:" + std::to_string(mockSrv.Port()) + "/fail";
        p1->apiKeyUtf8 = "nvapi-Loopback1";
        p1->timeoutMs = 4000;
        p2->endpointUrlUtf8 = "http://127.0.0.1:" + std::to_string(mockSrv.Port()) + "/ok";
        p2->modelsUrlUtf8 = "http://127.0.0.1:" + std::to_string(mockSrv.Port()) + "/models";
        p2->apiKeyUtf8 = "sk-Loopback2";
        p2->timeoutMs = 4000;

        const AiQueryResult qr = fv.QueryWithFailover("Sistema", "Pregunta");
        Check(qr.ok && qr.replyTextUtf8 == "FAILOVER_VERIFIED_OK", "QueryWithFailover recovers from HTTP 429 to secondary HTTP 200");
        Check(qr.providerIdUsedUtf8 == "openai" && mockSrv.HitsFail() == 1 && mockSrv.HitsOk() == 1, "Failover hit primary once then secondary once");

        std::vector<std::string> discModels;
        Check(AiProviderVault::FetchAvailableModels(*p2, discModels) && discModels.size() == 2u, "FetchAvailableModels discovers models over HTTP");
        mockSrv.Stop();
    }
#endif

    fs::remove_all(tmpDir, ec);
}

void RunAccessibilityAndD2DPixelTests() {
    // 1. Math verbalization
    const std::string spoken = AccessibilityEngine::VerbalizeMathExpressionForSpeech("E = m*c^2 + sqrt(x) <= pi");
    Check(spoken.find("al cuadrado") != std::string::npos, "Verbalize ^2 -> al cuadrado");
    Check(spoken.find("ra\xC3\xADz cuadrada") != std::string::npos, "Verbalize sqrt -> raiz cuadrada");
    Check(spoken.find("menor o igual que") != std::string::npos, "Verbalize <= -> menor o igual que");
    Check(spoken.find("pi") != std::string::npos, "Verbalize pi");

    // 2. Color-blind Okabe-Ito remapping (preserves neutrals, remaps red/green distinctly)
    const DocColor gray = AccessibilityEngine::RemapColorForColorBlind(DocColor::RGBA(40, 40, 40, 255), ColorBlindPaletteMode::Deuteranopia);
    Check(gray.r == 40 && gray.g == 40 && gray.b == 40, "ColorBlind preserves neutral dark gray");
    const DocColor cbRed = AccessibilityEngine::RemapColorForColorBlind(DocColor::RGBA(220, 30, 30, 255), ColorBlindPaletteMode::Deuteranopia);
    const DocColor cbGrn = AccessibilityEngine::RemapColorForColorBlind(DocColor::RGBA(20, 160, 40, 255), ColorBlindPaletteMode::Deuteranopia);
    Check(cbRed.r != cbGrn.r || cbRed.g != cbGrn.g || cbRed.b != cbGrn.b, "ColorBlind remaps red & green into distinct Okabe-Ito hues");
    Check(cbRed.r == 213 && cbRed.g == 94 && cbGrn.g == 158 && cbGrn.b == 115, "Okabe-Ito exact Vermilion / Bluish-Green");

    // 3. Windows SAPI 5.4 TTS initialization + voice enumeration + safe async stop & shutdown
    {
        SpeechSynthesizerWin32 tts;
        std::string ttsErr;
        const bool initOk = tts.Initialize(&ttsErr);
        Check(initOk, "Windows SAPI 5.4 ISpVoice initializes cleanly");
        const auto voices = tts.EnumerateInstalledVoices();
        Check(!voices.empty(), "Windows SAPI enumerates at least 1 installed voice");
        tts.SetRate(1);
        tts.SetVolume(0); // silent test volume
        Check(tts.SpeakAsync("Prueba de accesibilidad", &ttsErr), "SAPI SpeakAsync starts cleanly");
        tts.Stop();
        tts.Shutdown();
    }

    // 4. Persistence of AccessibilityPreferences in ui_settings.json
    const fs::path tmpPrefs = fs::temp_directory_path() / L"cuadernu_a11y_prefs.json";
    StudioUiPreferences pOut{}, pIn{};
    pOut.accessibility.readingRulerEnabled = true;
    pOut.accessibility.readingRulerHeightPx = 104;
    pOut.accessibility.dyslexiaFriendlyMode = true;
    pOut.accessibility.paperTint = PaperTintPreset::SepiaCalido;
    pOut.accessibility.colorBlindMode = ColorBlindPaletteMode::Protanopia;
    pOut.accessibility.uiFontScalePct = 130;
    pOut.accessibility.enhancedFocusRing = true;
    pOut.accessibility.ttsRate = 2;
    Check(StudioProfileStore::SaveUiPreferences(tmpPrefs.wstring(), pOut), "SaveUiPreferences with accessibility");
    Check(StudioProfileStore::LoadUiPreferences(tmpPrefs.wstring(), pIn), "LoadUiPreferences with accessibility");
    Check(pIn.accessibility.readingRulerEnabled && pIn.accessibility.readingRulerHeightPx == 104 &&
          pIn.accessibility.dyslexiaFriendlyMode && pIn.accessibility.paperTint == PaperTintPreset::SepiaCalido &&
          pIn.accessibility.colorBlindMode == ColorBlindPaletteMode::Protanopia && pIn.accessibility.uiFontScalePct == 130 &&
          pIn.accessibility.enhancedFocusRing && pIn.accessibility.ttsRate == 2,
          "AccessibilityPreferences exact roundtrip in ui_settings.json");
    std::error_code ec;
    fs::remove(tmpPrefs, ec);

    // 5. Direct2D pixel verification of Paper Tint, Reading Ruler, Dyslexia Mode, ColorBlind Remap, and Pomodoro HUD
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    {
        AnnotationObject tb{};
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 60.0, 120.0, 380.0, 160.0 };
        tb.textUtf8 = "Primera linea de lectura\nSegunda linea de comprobacion\nTercera linea de espaciado";
        tb.strokeColor = DocColor::RGBA(20, 24, 34, 255);
        tb.fontSizePt = 14.0;
        doc.AddCanvasObject(std::move(tb));

        AnnotationObject inkRed{};
        inkRed.kind = ObjectKind::InkStroke;
        inkRed.strokeColor = DocColor::RGBA(225, 25, 25, 255);
        inkRed.strokeWidth = 8.0;
        inkRed.inkPoints = { { 60.0, 320.0, 0.8f, 0, 0, 0 }, { 320.0, 320.0, 0.8f, 0, 0, 10 } };
        inkRed.RecomputeBounds();
        doc.AddCanvasObject(std::move(inkRed));
    }

    D2DRenderer renderer;
    Check(renderer.InitializeFactories(), "D2DRenderer factories initialized");
    Camera2D cam{};
    cam.panX = 0.0; cam.panY = 0.0; cam.zoom = 1.0;

    ViewportOverlayState ovBase{};
    RasterImageBgra32 imgBase, imgSepia, imgRuler, imgDys, imgCb, imgPomo;
    Check(renderer.RenderHwndViewportToOffscreen(doc, cam, 520, 400, nullptr, imgBase, &ovBase), "D2D baseline render");

    ViewportOverlayState ovSepia = ovBase;
    ovSepia.paperTintPreset = static_cast<int>(PaperTintPreset::SepiaCalido);
    Check(renderer.RenderHwndViewportToOffscreen(doc, cam, 520, 400, nullptr, imgSepia, &ovSepia), "D2D sepia render");
    Check(CountDifferentPixels(imgBase, imgSepia) > 50000u, "Sepia paper tint visibly changes background surface pixels");

    ViewportOverlayState ovRuler = ovBase;
    ovRuler.readingRulerEnabled = true;
    ovRuler.readingRulerCursorY = 180.0;
    ovRuler.readingRulerHeightPx = 76;
    Check(renderer.RenderHwndViewportToOffscreen(doc, cam, 520, 400, nullptr, imgRuler, &ovRuler), "D2D reading ruler render");
    Check(CountDifferentPixels(imgBase, imgRuler) > 50000u, "Reading ruler mask visibly darkens top/bottom outside focused band");
    // Verify center of band (y=180, x=10) remains bright while top mask (y=20, x=10) is dimmed
    const std::uint8_t* pxBand = &imgRuler.pixels[(180u * 520u + 10u) * 4u];
    const std::uint8_t* pxMask = &imgRuler.pixels[(20u * 520u + 10u) * 4u];
    Check(pxBand[2] > pxMask[2] + 40, "Reading ruler keeps center band clear and dims exterior");

    ViewportOverlayState ovDys = ovBase;
    ovDys.dyslexiaFriendlyMode = true;
    Check(renderer.RenderHwndViewportToOffscreen(doc, cam, 520, 400, nullptr, imgDys, &ovDys), "D2D dyslexia render");
    Check(CountDifferentPixels(imgBase, imgDys) > 200u, "Dyslexia mode visibly alters font/line-spacing pixels");

    ViewportOverlayState ovCb = ovBase;
    ovCb.colorBlindPaletteMode = static_cast<int>(ColorBlindPaletteMode::Deuteranopia);
    Check(renderer.RenderHwndViewportToOffscreen(doc, cam, 520, 400, nullptr, imgCb, &ovCb), "D2D colorblind render");
    Check(CountDifferentPixels(imgBase, imgCb) > 300u, "Color-blind Okabe-Ito mode remaps red stroke pixels");

    ViewportOverlayState ovPomo = ovBase;
    ovPomo.pomodoroVisible = true;
    ovPomo.pomodoroBadgeUtf8 = "Foco 24:59";
    Check(renderer.RenderHwndViewportToOffscreen(doc, cam, 520, 400, nullptr, imgPomo, &ovPomo), "D2D pomodoro pill render");
    Check(CountDifferentPixels(imgBase, imgPomo) > 500u, "Pomodoro HUD pill visibly renders in top-right viewport");
}

void RunStudyToolsAndAutomationsTests() {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle("Calculo y Circuitos");
    CommandStack stack;

    {
        AnnotationObject f{};
        f.kind = ObjectKind::MathBlock;
        f.bounds = { 50.0, 50.0, 260.0, 64.0 };
        f.formulaExprUtf8 = "V = I * R";
        f.evaluatedResultUtf8 = "12 V";
        doc.AddPageObject(0, std::move(f));

        AnnotationObject notes{};
        notes.kind = ObjectKind::TextBox;
        notes.bounds = { 50.0, 140.0, 440.0, 220.0 };
        notes.textUtf8 =
            "Impedancia: Oposicion total al flujo de corriente alterna en un circuito.\n"
            "Resonancia: Condicion donde reactancia inductiva y capacitiva se cancelan.\n"
            "- [ ] Resolver guia de ejercicios 2026-12-05\n"
            "Entrega: Informe de laboratorio RC 12/12/2026\n"
            "Examen: Parcial II 2026-12-18\n";
        doc.AddPageObject(0, std::move(notes));

        AnnotationObject code{};
        code.kind = ObjectKind::CodeBlock;
        code.bounds = { 50.0, 380.0, 320.0, 90.0 };
        code.codeLanguageUtf8 = "cpp";
        code.textUtf8 = "double ohm(double i, double r) { return i * r; }";
        doc.AddPageObject(0, std::move(code));
    }

    // 1. Guided Tutor (3 levels offline)
    const auto t1 = StudyAssistantTools::RunGuidedTutor(doc, "Ley de Ohm y circuitos", TutorHintLevel::ConceptualHint, true, nullptr);
    const auto t2 = StudyAssistantTools::RunGuidedTutor(doc, "Ley de Ohm y circuitos", TutorHintLevel::StepByStepPlan, true, nullptr);
    const auto t3 = StudyAssistantTools::RunGuidedTutor(doc, "Ley de Ohm y circuitos", TutorHintLevel::FullVerification, false, nullptr);
    Check(t1.ok && t1.responseMarkdownUtf8.find("Nivel 1") != std::string::npos, "Tutor level 1 hint generated");
    Check(t2.ok && t2.responseMarkdownUtf8.find("Nivel 2") != std::string::npos, "Tutor level 2 steps generated");
    Check(t3.ok && t3.responseMarkdownUtf8.find("Nivel 3") != std::string::npos, "Tutor level 3 verification generated");

    // 2. Smart Flashcards + StudyTape insertion + Undo/Redo
    const auto cards = StudyAssistantTools::GenerateSmartFlashcards(doc, 6, nullptr);
    Check(cards.size() >= 3u, "Smart flashcards harvests math + definitions offline");
    const std::size_t beforeObjs = doc.Pages()[0].objects.size();
    const std::size_t insertedTapes = StudyAssistantTools::InsertPracticeStudyTapes(doc, stack, ObjectTarget{ false, 0 }, cards);
    Check(insertedTapes == cards.size() && doc.Pages()[0].objects.size() == beforeObjs + cards.size(), "Practice StudyTapes inserted");
    Check(stack.Undo(doc) && doc.Pages()[0].objects.size() == beforeObjs, "1-step Undo removes all practice StudyTapes");
    Check(stack.Redo(doc) && doc.Pages()[0].objects.size() == beforeObjs + cards.size(), "1-step Redo restores practice StudyTapes");

    // 3. Concept Map with magnetic connectors + 1-step Undo/Redo
    const std::size_t beforeCanvas = doc.CanvasObjects().size();
    const auto mapRes = StudyAssistantTools::GenerateConceptMapInDocument(doc, stack, ObjectTarget{ true, 0 }, "Circuitos AC", nullptr);
    Check(mapRes.ok && mapRes.nodeCount >= 4u && mapRes.connectorCount >= 3u, "Concept map creates nodes and magnetic connectors");
    bool hasBoundConnector = false;
    for (const auto& o : doc.CanvasObjects()) {
        if (o.kind == ObjectKind::Shape && o.shapeKind == ShapeKind::Connector && o.connectorFromId != 0 && o.connectorToId != 0) {
            hasBoundConnector = true;
            break;
        }
    }
    Check(hasBoundConnector, "Concept map connectors carry valid connectorFromId/connectorToId");
    Check(stack.Undo(doc) && doc.CanvasObjects().size() == beforeCanvas, "1-step Undo removes entire concept map");
    Check(stack.Redo(doc) && doc.CanvasObjects().size() == beforeCanvas + mapRes.nodeCount + mapRes.connectorCount, "1-step Redo restores concept map");

    // 4. Formula Sheet & Glossary Builder + Append page
    const auto sheet = StudyAssistantTools::BuildFormulaSheetAndGlossary(doc, nullptr);
    Check(sheet.formulaCount >= 1u && sheet.glossaryCount >= 2u && sheet.codeSnippetCount >= 1u, "Formula Sheet & Glossary extracts formulas, definitions, code");
    const std::size_t pagesBefore = doc.Pages().size();
    Check(StudyAssistantTools::AppendFormulaSheetPage(doc, stack, sheet) && doc.Pages().size() == pagesBefore + 1, "AppendFormulaSheetPage inserts review page");

    // 5. Pomodoro Timer state machine
    PomodoroTimerState pomo{};
    pomo.Configure(1, 1, true);
    pomo.Start(10000ull);
    std::uint64_t added = 0;
    bool trans = false;
    Check(pomo.Tick(40000ull, added, &trans) && added == 30ull && !trans && pomo.remainingSeconds == 30, "Pomodoro counts 30s accurately");
    Check(pomo.Tick(75000ull, added, &trans) && added == 30ull && trans && pomo.inBreakPhase && pomo.completedCycles == 1, "Pomodoro transitions Focus -> Break and increments cycle");
    Check(pomo.FormatHudBadgeUtf8().rfind("Pausa ", 0) == 0, "Pomodoro HUD badge formats Break phase");
    pomo.Reset();
    Check(!pomo.running && !pomo.inBreakPhase && pomo.remainingSeconds == 60, "Pomodoro resets cleanly");

    // 6. Task & Deadline Extractor -> StructuredDatabase + RFC 5545 .ics + deduplication
    StructuredDatabase db("db_test", "Entregas");
    const auto taskRep1 = StudyAssistantTools::ExtractTasksAndDeadlines(doc, nullptr, &db);
    Check(taskRep1.extracted.size() == 3u, "Extracts 3 tasks (- [ ], Entrega:, Examen:)");
    Check(taskRep1.insertedIntoDatabaseCount == 3u && db.Records().size() == 3u, "Inserts 3 records into StructuredDatabase");
    Check(taskRep1.icsCalendarUtf8.find("BEGIN:VCALENDAR") != std::string::npos &&
          taskRep1.icsCalendarUtf8.find("DTSTART;VALUE=DATE:20261205") != std::string::npos &&
          taskRep1.icsCalendarUtf8.find("DTSTART;VALUE=DATE:20261212") != std::string::npos &&
          taskRep1.icsCalendarUtf8.find("END:VCALENDAR") != std::string::npos,
          "Generated .ics contains valid RFC 5545 VEVENT dates (ISO + DD/MM/YYYY)");
    const auto taskRep2 = StudyAssistantTools::ExtractTasksAndDeadlines(doc, nullptr, &db);
    Check(taskRep2.insertedIntoDatabaseCount == 0u && db.Records().size() == 3u, "Re-running task extraction deduplicates database rows");

    // 7. 1-Click Post-Audio Lecture Pipeline
    AudioTimelineSession audio;
    audio.StartRecording(16000);
    audio.AppendSyntheticTone(440.0, 300);
    audio.StopRecording();
    const auto postRep = StudyAssistantTools::RunPostLectureAutomation(doc, stack, ObjectTarget{ false, 0 }, audio, db, nullptr);
    Check(postRep.ok && postRep.insertedCardObjId != 0 && !postRep.summaryMarkdownUtf8.empty(), "Post-Audio 1-click pipeline inserts executive synthesis card");

    // 8. Academic Citations (APA 7, IEEE, BibTeX)
    AcademicSourceInfo src{};
    src.authorsUtf8 = "Stewart, J.";
    src.titleUtf8 = "Calculo de varias variables";
    src.yearUtf8 = "2021";
    src.venueOrPublisherUtf8 = "Cengage Learning";
    src.pagesUtf8 = "1-980";
    src.sourceKindUtf8 = "book";
    const std::string apa = StudyAssistantTools::FormatAcademicCitation(src, CitationStyle::Apa7);
    const std::string ieee = StudyAssistantTools::FormatAcademicCitation(src, CitationStyle::Ieee, 3);
    const std::string bib = StudyAssistantTools::FormatAcademicCitation(src, CitationStyle::BibTeX);
    Check(apa.find("Stewart, J. (2021). Calculo de varias variables.") != std::string::npos, "APA 7 citation formatted");
    Check(ieee.find("[3] Stewart, J., \"Calculo de varias variables,\"") != std::string::npos, "IEEE citation formatted");
    Check(bib.find("@book{stewart2021calcul,") != std::string::npos && bib.find("publisher = {Cengage Learning}") != std::string::npos, "BibTeX citation formatted");

    FlexcilStudyOverlay flex;
    PdfSourceMetadata pm{};
    pm.docIdUtf8 = "pdf1"; pm.titleUtf8 = "Guia Fisica II"; pm.authorUtf8 = "Sears"; pm.pageCount = 120;
    flex.IndexPdfSourceMetadata(pm);
    const auto cands = StudyAssistantTools::HarvestCitationCandidates(doc, &flex);
    Check(!cands.empty() && cands[0].titleUtf8 == "Guia Fisica II", "HarvestCitationCandidates picks indexed PDF metadata");
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif
    int exitCode = 0;
    {
        if (argc >= 2 && std::strcmp(argv[1], "--provision-user-vault") == 0) {
            exitCode = RunProvisionUserVaultMode();
        } else {
            RunAiVaultAndFailoverTests();
            RunAccessibilityAndD2DPixelTests();
            RunStudyToolsAndAutomationsTests();
            std::cout << "[AiVaultA11yStudyTest] PASSED=" << gPassed << " FAILED=" << gFailed << "\n";
            exitCode = (gFailed == 0) ? 0 : 1;
        }
    }
#ifdef _WIN32
    if (SUCCEEDED(hrCo)) CoUninitialize();
#endif
    return exitCode;
}
