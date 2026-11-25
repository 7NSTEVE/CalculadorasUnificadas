#include "ai_provider_vault.hpp"
#include "utf_conv.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winhttp.lib")
#endif

namespace DocStudio {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

constexpr const char* kDpapiEntropy = "CuadernU.AiVault.v1";

std::string TrimAscii(const std::string& s) {
    std::size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    std::size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string ToLowerAscii(std::string s) {
    for (char& ch : s) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return s;
}

std::string ToUpperAscii(std::string s) {
    for (char& ch : s) {
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    }
    return s;
}

std::string StripSurroundingQuotes(const std::string& raw) {
    const std::string t = TrimAscii(raw);
    if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') || (t.front() == '\'' && t.back() == '\''))) {
        return t.substr(1, t.size() - 2);
    }
    return t;
}

std::string SanitizeUrlForLog(const std::string& url) {
    const auto q = url.find('?');
    return (q == std::string::npos) ? url : url.substr(0, q);
}

std::string MapAliasToProviderId(const std::string& rawName) {
    const std::string k = ToUpperAscii(TrimAscii(rawName));
    if (k == "GEMINI_API_KEY" || k == "GOOGLE_API_KEY" || k == "GOOGLE_AI_API_KEY" ||
        k == "GOOGLE_GEMINI" || k == "GOOGLE" || k == "GEMINI") {
        return "google_gemini";
    }
    if (k == "NVIDIA_API_KEY" || k == "NVAPI_KEY" || k == "NVIDIA_NIM_API_KEY" ||
        k == "NVIDIA_NIM" || k == "NVIDIA" || k == "NIM") {
        return "nvidia_nim";
    }
    if (k == "OPENAI_API_KEY" || k == "OPENAI") {
        return "openai";
    }
    if (k == "ANTHROPIC_API_KEY" || k == "CLAUDE_API_KEY" || k == "ANTHROPIC" || k == "CLAUDE") {
        return "anthropic";
    }
    if (k == "OPENCODE_API_KEY" || k == "OPENCODE_ZEN_API_KEY" || k == "OPENCODE") {
        return "opencode";
    }
    if (k == "OPENROUTER_API_KEY" || k == "OPENROUTER") {
        return "openrouter";
    }
    if (k == "GROQ_API_KEY" || k == "GROQ") {
        return "groq";
    }
    if (k == "DEEPSEEK_API_KEY" || k == "DEEPSEEK") {
        return "deepseek";
    }
    if (k == "MISTRAL_API_KEY" || k == "MISTRAL") {
        return "mistral";
    }
    if (k == "XAI_API_KEY" || k == "GROK_API_KEY" || k == "XAI" || k == "XAI_GROK" || k == "GROK") {
        return "xai_grok";
    }
    if (k == "OLLAMA" || k == "OLLAMA_LOCAL") {
        return "ollama_local";
    }
    if (k == "LMSTUDIO" || k == "LMSTUDIO_LOCAL" || k == "LM_STUDIO") {
        return "lmstudio_local";
    }
    if (k == "CUSTOM_AI_API_KEY" || k == "CUSTOM_OPENAI" || k == "CUSTOM") {
        return "custom_openai";
    }
    return {};
}

bool WriteBinaryFile(const fs::path& p, const std::uint8_t* data, std::size_t len, std::string* outErr) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        if (outErr) *outErr = "No se pudo escribir archivo de configuraci\xC3\xB3n IA.";
        return false;
    }
    if (len > 0 && data) {
        ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
    }
    return ofs.good();
}

bool ReadBinaryFile(const fs::path& p, std::vector<std::uint8_t>& outBytes, std::string* outErr) {
    outBytes.clear();
    std::ifstream ifs(p, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        if (outErr) *outErr = "Archivo no encontrado.";
        return false;
    }
    const auto sz = ifs.tellg();
    if (sz <= 0) return true;
    outBytes.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(outBytes.data()), sz);
    return ifs.good() || ifs.eof();
}

void SecureWipeString(std::string& s) {
    if (!s.empty()) {
#ifdef _WIN32
        SecureZeroMemory(s.data(), s.size());
#else
        volatile char* p = s.data();
        for (std::size_t i = 0; i < s.size(); ++i) p[i] = 0;
#endif
        s.clear();
    }
}

void SecureWipeBytes(std::vector<std::uint8_t>& v) {
    if (!v.empty()) {
#ifdef _WIN32
        SecureZeroMemory(v.data(), v.size());
#else
        volatile std::uint8_t* p = v.data();
        for (std::size_t i = 0; i < v.size(); ++i) p[i] = 0;
#endif
        v.clear();
    }
}

} // namespace

AiProviderVault::AiProviderVault() {
    ResetToDefaultCatalog();
}

AiProviderVault::~AiProviderVault() {
    for (auto& p : providers_) {
        SecureWipeString(p.apiKeyUtf8);
    }
}

void AiProviderVault::ResetToDefaultCatalog() {
    globalEnabled_ = false;
    activeProviderId_ = "google_gemini";
    failoverProviderId_ = "nvidia_nim";
    providers_ = {
        {
            "google_gemini", "Google Gemini (AI Studio)", AiProviderKind::GoogleGemini,
            "https://generativelanguage.googleapis.com/v1beta",
            "https://generativelanguage.googleapis.com/v1beta/models",
            "gemini-2.5-flash",
            { "gemini-2.5-flash", "gemini-3.5-flash", "gemini-3.8-flash", "gemini-2.5-pro", "gemini-2.0-flash" },
            "", true, true, 30000
        },
        {
            "google_vertex_express", "Google Vertex AI (Express Mode)", AiProviderKind::GoogleGemini,
            "https://aiplatform.googleapis.com/v1/publishers/google",
            "",
            "gemini-2.5-flash",
            { "gemini-2.5-flash", "gemini-3.5-flash", "gemini-3.8-flash", "gemini-2.5-pro" },
            "", true, true, 30000
        },
        {
            "nvidia_nim", "NVIDIA NIM", AiProviderKind::NvidiaNim,
            "https://integrate.api.nvidia.com/v1/chat/completions",
            "https://integrate.api.nvidia.com/v1/models",
            "nvidia/nemotron-3.5-bolt-e4b",
            { "nvidia/nemotron-3.5-bolt-e4b", "minimaxai/minimax-m3.1-flash", "google/gemma-4-31b-it", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", "mistralai/mistral-large-4-1t-a52b", "moonshotai/kimi-k3.1-pro" },
            "", true, true, 35000
        },
        {
            "openai", "OpenAI", AiProviderKind::OpenAI,
            "https://api.openai.com/v1/chat/completions",
            "https://api.openai.com/v1/models",
            "gpt-4o-mini",
            { "gpt-4o-mini", "gpt-4.1-mini", "gpt-4.1", "o3-mini" },
            "", true, true, 30000
        },
        {
            "anthropic", "Anthropic Claude", AiProviderKind::Anthropic,
            "https://api.anthropic.com/v1/messages",
            "https://api.anthropic.com/v1/models",
            "claude-sonnet-4-5",
            { "claude-sonnet-4-5", "claude-haiku-4-5", "claude-3-7-sonnet-latest" },
            "", true, true, 30000
        },
        {
            "opencode", "OpenCode", AiProviderKind::OpenCode,
            "https://opencode.ai/zen/v1/chat/completions",
            "https://opencode.ai/zen/v1/models",
            "gpt-5-nano",
            { "gpt-5-nano", "claude-sonnet-4-5", "gemini-2.5-flash" },
            "", true, true, 30000
        },
        {
            "openrouter", "OpenRouter", AiProviderKind::OpenRouter,
            "https://openrouter.ai/api/v1/chat/completions",
            "https://openrouter.ai/api/v1/models",
            "google/gemini-2.5-flash",
            { "google/gemini-2.5-flash", "meta-llama/llama-3.3-70b-instruct", "anthropic/claude-sonnet-4" },
            "", true, true, 30000
        },
        {
            "groq", "Groq", AiProviderKind::Groq,
            "https://api.groq.com/openai/v1/chat/completions",
            "https://api.groq.com/openai/v1/models",
            "llama-3.3-70b-versatile",
            { "llama-3.3-70b-versatile", "llama-3.1-8b-instant", "mixtral-8x7b-32768" },
            "", true, true, 25000
        },
        {
            "deepseek", "DeepSeek", AiProviderKind::DeepSeek,
            "https://api.deepseek.com/v1/chat/completions",
            "https://api.deepseek.com/v1/models",
            "deepseek-chat",
            { "deepseek-chat", "deepseek-reasoner" },
            "", true, true, 30000
        },
        {
            "mistral", "Mistral AI", AiProviderKind::Mistral,
            "https://api.mistral.ai/v1/chat/completions",
            "https://api.mistral.ai/v1/models",
            "mistral-small-latest",
            { "mistral-small-latest", "mistral-large-latest", "codestral-latest" },
            "", true, true, 30000
        },
        {
            "xai_grok", "xAI Grok", AiProviderKind::XaiGrok,
            "https://api.x.ai/v1/chat/completions",
            "https://api.x.ai/v1/models",
            "grok-3-mini",
            { "grok-3-mini", "grok-3" },
            "", true, true, 30000
        },
        {
            "ollama_local", "Ollama (Local)", AiProviderKind::OllamaLocal,
            "http://127.0.0.1:11434/api/chat",
            "http://127.0.0.1:11434/api/tags",
            "llama3.2",
            { "llama3.2", "qwen2.5", "mistral", "deepseek-r1:8b" },
            "", false, true, 20000
        },
        {
            "lmstudio_local", "LM Studio (Local)", AiProviderKind::LmStudioLocal,
            "http://127.0.0.1:1234/v1/chat/completions",
            "http://127.0.0.1:1234/v1/models",
            "local-model",
            { "local-model" },
            "", false, true, 20000
        },
        {
            "custom_openai", "Proveedor Personalizado (OpenAI-Compat)", AiProviderKind::CustomOpenAICompat,
            "http://127.0.0.1:8080/v1/chat/completions",
            "http://127.0.0.1:8080/v1/models",
            "default-model",
            { "default-model" },
            "", false, true, 25000
        }
    };
}

bool AiProviderVault::SetActiveProviderId(const std::string& idUtf8) {
    if (!FindProvider(idUtf8)) return false;
    activeProviderId_ = idUtf8;
    return true;
}

bool AiProviderVault::SetFailoverProviderId(const std::string& idUtf8) {
    if (idUtf8.empty()) {
        failoverProviderId_.clear();
        return true;
    }
    if (!FindProvider(idUtf8)) return false;
    failoverProviderId_ = idUtf8;
    return true;
}

std::vector<std::string> AiProviderVault::EffectiveFailoverChain() const {
    std::vector<std::string> chain;
    std::set<std::string> seen;
    auto pushIfReady = [&](const AiProviderProfile* p) {
        if (!p || !p->enabled) return;
        if (seen.count(p->idUtf8)) return;
        if (p->requiresApiKey && TrimAscii(p->apiKeyUtf8).empty()) return;
        seen.insert(p->idUtf8);
        chain.push_back(p->idUtf8);
    };
    pushIfReady(FindProvider(activeProviderId_));
    if (!failoverProviderId_.empty()) {
        pushIfReady(FindProvider(failoverProviderId_));
    }
    // Secondary configured cloud providers first, then local endpoints as final network-free fallback
    for (const auto& p : providers_) {
        if (p.requiresApiKey && !TrimAscii(p.apiKeyUtf8).empty()) pushIfReady(&p);
    }
    for (const auto& p : providers_) {
        if (!p.requiresApiKey) pushIfReady(&p);
    }
    return chain;
}

const AiProviderProfile* AiProviderVault::FindProvider(const std::string& idUtf8) const {
    for (const auto& p : providers_) {
        if (p.idUtf8 == idUtf8) return &p;
    }
    return nullptr;
}

AiProviderProfile* AiProviderVault::FindProviderMutable(const std::string& idUtf8) {
    for (auto& p : providers_) {
        if (p.idUtf8 == idUtf8) return &p;
    }
    return nullptr;
}

const AiProviderProfile* AiProviderVault::FindProviderByKind(AiProviderKind kind) const {
    for (const auto& p : providers_) {
        if (p.kind == kind) return &p;
    }
    return nullptr;
}

AiProviderProfile* AiProviderVault::FindProviderByKindMutable(AiProviderKind kind) {
    for (auto& p : providers_) {
        if (p.kind == kind) return &p;
    }
    return nullptr;
}

bool AiProviderVault::SetApiKey(const std::string& providerIdUtf8, const std::string& rawKeyUtf8) {
    auto* p = FindProviderMutable(providerIdUtf8);
    if (!p) return false;
    p->apiKeyUtf8 = TrimAscii(rawKeyUtf8);
    if (!p->apiKeyUtf8.empty()) {
        p->enabled = true;
        globalEnabled_ = true;
    }
    return true;
}

bool AiProviderVault::ClearApiKey(const std::string& providerIdUtf8) {
    auto* p = FindProviderMutable(providerIdUtf8);
    if (!p) return false;
    SecureWipeString(p->apiKeyUtf8);
    return true;
}

bool AiProviderVault::HasConfiguredCloudOrLocalProvider() const {
    if (!globalEnabled_) return false;
    return !EffectiveFailoverChain().empty();
}

bool AiProviderVault::ProtectBytesDpapi(
    const std::vector<std::uint8_t>& plainBytes,
    std::vector<std::uint8_t>& outProtected,
    std::string* outErr)
{
    outProtected.clear();
#ifdef _WIN32
    DATA_BLOB inBlob{};
    inBlob.pbData = plainBytes.empty() ? nullptr : const_cast<BYTE*>(plainBytes.data());
    inBlob.cbData = static_cast<DWORD>(plainBytes.size());

    DATA_BLOB entropyBlob{};
    entropyBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kDpapiEntropy));
    entropyBlob.cbData = static_cast<DWORD>(std::strlen(kDpapiEntropy));

    DATA_BLOB outBlob{};
    if (!CryptProtectData(&inBlob, L"CuadernU AI Keys", &entropyBlob, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        if (outErr) *outErr = "CryptProtectData fall\xC3\xB3 (c\xC3\xB3""digo " + std::to_string(GetLastError()) + ").";
        return false;
    }
    outProtected.assign(outBlob.pbData, outBlob.pbData + outBlob.cbData);
    SecureZeroMemory(outBlob.pbData, outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
#else
    (void)plainBytes;
    if (outErr) *outErr = "DPAPI solo disponible en Windows.";
    return false;
#endif
}

bool AiProviderVault::UnprotectBytesDpapi(
    const std::vector<std::uint8_t>& protectedBytes,
    std::vector<std::uint8_t>& outPlain,
    std::string* outErr)
{
    outPlain.clear();
    if (protectedBytes.empty()) return false;
#ifdef _WIN32
    DATA_BLOB inBlob{};
    inBlob.pbData = const_cast<BYTE*>(protectedBytes.data());
    inBlob.cbData = static_cast<DWORD>(protectedBytes.size());

    DATA_BLOB entropyBlob{};
    entropyBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kDpapiEntropy));
    entropyBlob.cbData = static_cast<DWORD>(std::strlen(kDpapiEntropy));

    DATA_BLOB outBlob{};
    if (!CryptUnprotectData(&inBlob, nullptr, &entropyBlob, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        if (outErr) *outErr = "CryptUnprotectData fall\xC3\xB3 (c\xC3\xB3""digo " + std::to_string(GetLastError()) + ").";
        return false;
    }
    outPlain.assign(outBlob.pbData, outBlob.pbData + outBlob.cbData);
    SecureZeroMemory(outBlob.pbData, outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
#else
    (void)protectedBytes;
    if (outErr) *outErr = "DPAPI solo disponible en Windows.";
    return false;
#endif
}

bool AiProviderVault::SaveToDirectory(const std::wstring& rootDir, std::string* outErr) const {
    const fs::path root(rootDir);
    std::error_code ec;
    fs::create_directories(root, ec);

    // 1. Non-sensitive metadata JSON (NEVER contains any apiKey!)
    json meta;
    meta["schemaVersion"] = 1;
    meta["globalEnabled"] = globalEnabled_;
    meta["activeProviderId"] = activeProviderId_;
    meta["failoverProviderId"] = failoverProviderId_;
    json pArr = json::array();
    json secretsObj = json::object();

    for (const auto& p : providers_) {
        pArr.push_back({
            {"id", p.idUtf8},
            {"displayName", p.displayNameUtf8},
            {"kind", static_cast<int>(p.kind)},
            {"endpointUrl", p.endpointUrlUtf8},
            {"modelsUrl", p.modelsUrlUtf8},
            {"modelName", p.modelNameUtf8},
            {"suggestedModels", p.suggestedModelsUtf8},
            {"requiresApiKey", p.requiresApiKey},
            {"enabled", p.enabled},
            {"timeoutMs", p.timeoutMs}
        });
        if (!p.apiKeyUtf8.empty()) {
            secretsObj[p.idUtf8] = p.apiKeyUtf8;
        }
    }
    meta["providers"] = std::move(pArr);

    const std::string metaStr = meta.dump(2);
    const bool okMeta = WriteBinaryFile(
        root / L"ai_providers.json",
        reinterpret_cast<const std::uint8_t*>(metaStr.data()),
        metaStr.size(),
        outErr);
    if (!okMeta) return false;

    // 2. DPAPI-encrypted secrets blob (ai_keys.dpapi)
    std::string secJson = secretsObj.dump();
    std::vector<std::uint8_t> plainBytes(secJson.begin(), secJson.end());
    SecureWipeString(secJson);

    std::vector<std::uint8_t> protectedBytes;
    const bool okProt = ProtectBytesDpapi(plainBytes, protectedBytes, outErr);
    SecureWipeBytes(plainBytes);
    if (!okProt) return false;

    return WriteBinaryFile(root / L"ai_keys.dpapi", protectedBytes.data(), protectedBytes.size(), outErr);
}

bool AiProviderVault::LoadFromDirectory(const std::wstring& rootDir, std::string* outErr) {
    const fs::path root(rootDir);
    const fs::path metaPath = root / L"ai_providers.json";
    const fs::path keysPath = root / L"ai_keys.dpapi";
    if (!fs::exists(metaPath) && !fs::exists(keysPath)) return false;

    bool loadedAny = false;
    if (fs::exists(metaPath)) {
        std::vector<std::uint8_t> raw;
        if (ReadBinaryFile(metaPath, raw, outErr) && !raw.empty()) {
            try {
                const json j = json::parse(raw.begin(), raw.end());
                globalEnabled_ = j.value("globalEnabled", globalEnabled_);
                const std::string act = j.value("activeProviderId", activeProviderId_);
                const std::string fail = j.value("failoverProviderId", failoverProviderId_);
                if (j.contains("providers") && j["providers"].is_array()) {
                    for (const auto& pj : j["providers"]) {
                        const std::string id = pj.value("id", "");
                        if (id.empty()) continue;
                        if (auto* existing = FindProviderMutable(id)) {
                            existing->endpointUrlUtf8 = pj.value("endpointUrl", existing->endpointUrlUtf8);
                            existing->modelsUrlUtf8 = pj.value("modelsUrl", existing->modelsUrlUtf8);
                            existing->modelNameUtf8 = pj.value("modelName", existing->modelNameUtf8);
                            existing->enabled = pj.value("enabled", existing->enabled);
                            existing->timeoutMs = std::clamp(pj.value("timeoutMs", existing->timeoutMs), 3000, 120000);
                            if (pj.contains("suggestedModels") && pj["suggestedModels"].is_array()) {
                                std::vector<std::string> sm;
                                for (const auto& s : pj["suggestedModels"]) {
                                    if (s.is_string() && !s.get<std::string>().empty()) sm.push_back(s.get<std::string>());
                                }
                                if (!sm.empty()) existing->suggestedModelsUtf8 = std::move(sm);
                            }
                        }
                    }
                }
                if (FindProvider(act)) activeProviderId_ = act;
                if (fail.empty() || FindProvider(fail)) failoverProviderId_ = fail;
                loadedAny = true;
            } catch (const std::exception& ex) {
                if (outErr) *outErr = ex.what();
            }
        }
    }

    if (fs::exists(keysPath)) {
        std::vector<std::uint8_t> encBytes;
        if (ReadBinaryFile(keysPath, encBytes, outErr) && !encBytes.empty()) {
            std::vector<std::uint8_t> plainBytes;
            if (UnprotectBytesDpapi(encBytes, plainBytes, outErr) && !plainBytes.empty()) {
                try {
                    const json sj = json::parse(plainBytes.begin(), plainBytes.end());
                    if (sj.is_object()) {
                        for (auto it = sj.begin(); it != sj.end(); ++it) {
                            if (!it.value().is_string()) continue;
                            if (auto* p = FindProviderMutable(it.key())) {
                                p->apiKeyUtf8 = it.value().get<std::string>();
                            }
                        }
                        loadedAny = true;
                    }
                } catch (const std::exception& ex) {
                    if (outErr) *outErr = ex.what();
                }
                SecureWipeBytes(plainBytes);
            }
        }
    }
    return loadedAny;
}

std::string AiProviderVault::MaskApiKeyForDisplay(const std::string& rawKeyUtf8) {
    const std::string k = TrimAscii(rawKeyUtf8);
    if (k.empty()) return "(Sin clave)";
    if (k.size() <= 10) return "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2";
    return k.substr(0, 6) + "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2" + k.substr(k.size() - 4);
}

AiProviderKind AiProviderVault::DetectProviderFromKeyPrefix(const std::string& rawKeyUtf8) {
    const std::string k = TrimAscii(rawKeyUtf8);
    if (k.rfind("AQ.", 0) == 0 || k.rfind("AIza", 0) == 0) return AiProviderKind::GoogleGemini;
    if (k.rfind("nvapi-", 0) == 0) return AiProviderKind::NvidiaNim;
    if (k.rfind("sk-ant-", 0) == 0) return AiProviderKind::Anthropic;
    if (k.rfind("sk-or-", 0) == 0) return AiProviderKind::OpenRouter;
    if (k.rfind("gsk_", 0) == 0) return AiProviderKind::Groq;
    if (k.rfind("xai-", 0) == 0) return AiProviderKind::XaiGrok;
    if (k.rfind("sk-", 0) == 0) return AiProviderKind::OpenAI;
    return AiProviderKind::CustomOpenAICompat;
}

std::string AiProviderVault::ProviderIdForKind(AiProviderKind kind) {
    switch (kind) {
    case AiProviderKind::GoogleGemini:       return "google_gemini";
    case AiProviderKind::NvidiaNim:          return "nvidia_nim";
    case AiProviderKind::OpenAI:             return "openai";
    case AiProviderKind::Anthropic:          return "anthropic";
    case AiProviderKind::OpenCode:           return "opencode";
    case AiProviderKind::OpenRouter:         return "openrouter";
    case AiProviderKind::Groq:               return "groq";
    case AiProviderKind::DeepSeek:           return "deepseek";
    case AiProviderKind::Mistral:            return "mistral";
    case AiProviderKind::XaiGrok:            return "xai_grok";
    case AiProviderKind::OllamaLocal:        return "ollama_local";
    case AiProviderKind::LmStudioLocal:      return "lmstudio_local";
    case AiProviderKind::CustomOpenAICompat: return "custom_openai";
    }
    return "custom_openai";
}

AiKeyImportSummary AiProviderVault::ImportFromEnvContent(const std::string& envTextUtf8) {
    AiKeyImportSummary sum{};
    std::istringstream iss(envTextUtf8);
    std::string line;
    std::set<std::string> imported;

    while (std::getline(iss, line)) {
        line = TrimAscii(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("export ", 0) == 0) line = TrimAscii(line.substr(7));
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string keyName = TrimAscii(line.substr(0, eq));
        std::string val = TrimAscii(line.substr(eq + 1));
        if (!val.empty() && val.front() != '"' && val.front() != '\'') {
            const auto hashPos = val.find(" #");
            if (hashPos != std::string::npos) val = TrimAscii(val.substr(0, hashPos));
        }
        val = StripSurroundingQuotes(val);
        if (val.empty()) continue;

        const std::string keyUpper = ToUpperAscii(keyName);
        if (keyUpper == "CUSTOM_AI_ENDPOINT") {
            if (auto* cp = FindProviderMutable("custom_openai")) cp->endpointUrlUtf8 = val;
            continue;
        }
        if (keyUpper == "CUSTOM_AI_MODEL") {
            if (auto* cp = FindProviderMutable("custom_openai")) cp->modelNameUtf8 = val;
            continue;
        }

        std::string pid = MapAliasToProviderId(keyName);
        if (pid.empty() && keyUpper.find("API_KEY") != std::string::npos) {
            pid = ProviderIdForKind(DetectProviderFromKeyPrefix(val));
        }
        if (!pid.empty() && SetApiKey(pid, val)) {
            imported.insert(pid);
        }
    }

    sum.importedProviderIds.assign(imported.begin(), imported.end());
    sum.importedCount = sum.importedProviderIds.size();
    sum.noteUtf8 = "Claves importadas desde .env: " + std::to_string(sum.importedCount);
    return sum;
}

AiKeyImportSummary AiProviderVault::ImportFromEnvFile(const std::wstring& filePath, std::string* outErr) {
    std::vector<std::uint8_t> bytes;
    if (!ReadBinaryFile(fs::path(filePath), bytes, outErr)) return {};
    std::string content(bytes.begin(), bytes.end());
    SecureWipeBytes(bytes);
    auto res = ImportFromEnvContent(content);
    SecureWipeString(content);
    return res;
}

AiKeyImportSummary AiProviderVault::ImportFromJsonContent(const std::string& jsonTextUtf8) {
    AiKeyImportSummary sum{};
    std::set<std::string> imported;
    try {
        const json j = json::parse(jsonTextUtf8);
        auto ingestEntry = [&](const std::string& name, const json& val) {
            std::string pid = MapAliasToProviderId(name);
            if (val.is_string()) {
                const std::string k = TrimAscii(val.get<std::string>());
                if (k.empty()) return;
                if (pid.empty()) pid = ProviderIdForKind(DetectProviderFromKeyPrefix(k));
                if (!pid.empty() && SetApiKey(pid, k)) imported.insert(pid);
            } else if (val.is_object()) {
                std::string k;
                if (val.contains("apiKey") && val["apiKey"].is_string()) k = val["apiKey"].get<std::string>();
                else if (val.contains("api_key") && val["api_key"].is_string()) k = val["api_key"].get<std::string>();
                else if (val.contains("key") && val["key"].is_string()) k = val["key"].get<std::string>();
                else if (val.contains("token") && val["token"].is_string()) k = val["token"].get<std::string>();
                k = TrimAscii(k);
                if (pid.empty() && !k.empty()) pid = ProviderIdForKind(DetectProviderFromKeyPrefix(k));
                if (!pid.empty()) {
                    if (auto* p = FindProviderMutable(pid)) {
                        if (val.contains("endpoint") && val["endpoint"].is_string() && !val["endpoint"].get<std::string>().empty()) {
                            p->endpointUrlUtf8 = val["endpoint"].get<std::string>();
                        }
                        if (val.contains("endpointUrl") && val["endpointUrl"].is_string() && !val["endpointUrl"].get<std::string>().empty()) {
                            p->endpointUrlUtf8 = val["endpointUrl"].get<std::string>();
                        }
                        if (val.contains("model") && val["model"].is_string() && !val["model"].get<std::string>().empty()) {
                            p->modelNameUtf8 = val["model"].get<std::string>();
                        }
                    }
                    if (!k.empty() && SetApiKey(pid, k)) imported.insert(pid);
                }
            }
        };

        if (j.is_object()) {
            if (j.contains("providers") && j["providers"].is_object()) {
                for (auto it = j["providers"].begin(); it != j["providers"].end(); ++it) {
                    ingestEntry(it.key(), it.value());
                }
            }
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (it.key() == "providers") continue;
                ingestEntry(it.key(), it.value());
            }
        }
    } catch (...) {
        sum.noteUtf8 = "JSON inv\xC3\xA1lido al importar claves.";
        return sum;
    }

    sum.importedProviderIds.assign(imported.begin(), imported.end());
    sum.importedCount = sum.importedProviderIds.size();
    sum.noteUtf8 = "Claves importadas desde JSON: " + std::to_string(sum.importedCount);
    return sum;
}

AiKeyImportSummary AiProviderVault::ImportFromJsonFile(const std::wstring& filePath, std::string* outErr) {
    std::vector<std::uint8_t> bytes;
    if (!ReadBinaryFile(fs::path(filePath), bytes, outErr)) return {};
    std::string content(bytes.begin(), bytes.end());
    SecureWipeBytes(bytes);
    auto res = ImportFromJsonContent(content);
    SecureWipeString(content);
    return res;
}

AiKeyImportSummary AiProviderVault::ImportFromProcessEnvironment() {
    AiKeyImportSummary sum{};
#ifdef _WIN32
    static constexpr const wchar_t* kEnvVars[] = {
        L"GEMINI_API_KEY", L"GOOGLE_API_KEY", L"GOOGLE_AI_API_KEY",
        L"NVIDIA_API_KEY", L"NVAPI_KEY", L"NVIDIA_NIM_API_KEY",
        L"OPENAI_API_KEY", L"ANTHROPIC_API_KEY", L"CLAUDE_API_KEY",
        L"OPENCODE_API_KEY", L"OPENCODE_ZEN_API_KEY",
        L"OPENROUTER_API_KEY", L"GROQ_API_KEY", L"DEEPSEEK_API_KEY",
        L"MISTRAL_API_KEY", L"XAI_API_KEY", L"GROK_API_KEY",
        L"CUSTOM_AI_API_KEY"
    };
    std::set<std::string> imported;
    wchar_t buf[2048]{};
    for (const wchar_t* name : kEnvVars) {
        const DWORD n = GetEnvironmentVariableW(name, buf, static_cast<DWORD>(std::size(buf)));
        if (n > 0 && n < std::size(buf)) {
            std::string val = TrimAscii(WideToUtf8(std::wstring(buf, n)));
            SecureZeroMemory(buf, sizeof(buf));
            if (val.empty()) continue;
            const std::string pid = MapAliasToProviderId(WideToUtf8(name));
            if (!pid.empty() && SetApiKey(pid, val)) {
                imported.insert(pid);
            }
            SecureWipeString(val);
        }
    }
    sum.importedProviderIds.assign(imported.begin(), imported.end());
    sum.importedCount = sum.importedProviderIds.size();
    sum.noteUtf8 = "Claves detectadas en variables de entorno: " + std::to_string(sum.importedCount);
#endif
    return sum;
}

bool AiProviderVault::BuildProviderChatRequest(
    const AiProviderProfile& prof,
    const std::string& systemPromptUtf8,
    const std::string& userPromptUtf8,
    std::string& outUrlUtf8,
    std::vector<std::pair<std::string, std::string>>& outHeadersUtf8,
    std::string& outBodyUtf8)
{
    outUrlUtf8.clear();
    outHeadersUtf8.clear();
    outBodyUtf8.clear();

    const std::string ep = TrimAscii(prof.endpointUrlUtf8);
    const std::string model = TrimAscii(prof.modelNameUtf8);
    if (ep.empty() || model.empty()) return false;

    if (prof.kind == AiProviderKind::GoogleGemini && ep.find("/openai/") == std::string::npos) {
        std::string base = ep;
        while (!base.empty() && base.back() == '/') base.pop_back();
        std::string modelPath = model;
        if (modelPath.rfind("models/", 0) != 0) modelPath = "models/" + modelPath;
        outUrlUtf8 = base + "/" + modelPath + ":generateContent";
        if (!prof.apiKeyUtf8.empty()) {
            outHeadersUtf8.emplace_back("x-goog-api-key", prof.apiKeyUtf8);
        }
        json body;
        if (!systemPromptUtf8.empty()) {
            body["system_instruction"] = { {"parts", { {{"text", systemPromptUtf8}} }} };
        }
        body["contents"] = json::array({
            {
                {"role", "user"},
                {"parts", { {{"text", userPromptUtf8}} }}
            }
        });
        body["generationConfig"] = { {"temperature", 0.35}, {"maxOutputTokens", 1400} };
        outBodyUtf8 = body.dump();
        return true;
    }

    if (prof.kind == AiProviderKind::Anthropic) {
        outUrlUtf8 = ep;
        if (!prof.apiKeyUtf8.empty()) {
            outHeadersUtf8.emplace_back("x-api-key", prof.apiKeyUtf8);
        }
        outHeadersUtf8.emplace_back("anthropic-version", "2023-06-01");
        json body = {
            {"model", model},
            {"max_tokens", 1400},
            {"messages", {
                {{"role", "user"}, {"content", userPromptUtf8}}
            }}
        };
        if (!systemPromptUtf8.empty()) {
            body["system"] = systemPromptUtf8;
        }
        outBodyUtf8 = body.dump();
        return true;
    }

    if (prof.kind == AiProviderKind::OllamaLocal || ep.find("/api/chat") != std::string::npos) {
        outUrlUtf8 = ep;
        if (!prof.apiKeyUtf8.empty()) {
            outHeadersUtf8.emplace_back("Authorization", "Bearer " + prof.apiKeyUtf8);
        }
        json body = {
            {"model", model},
            {"stream", false},
            {"messages", {
                {{"role", "system"}, {"content", systemPromptUtf8}},
                {{"role", "user"}, {"content", userPromptUtf8}}
            }}
        };
        outBodyUtf8 = body.dump();
        return true;
    }

    // OpenAI-compatible wire format (NVIDIA NIM, OpenAI, OpenCode, OpenRouter, Groq, DeepSeek, Mistral, xAI, LM Studio, Gemini OpenAI-compat, Custom)
    outUrlUtf8 = ep;
    if (!prof.apiKeyUtf8.empty()) {
        outHeadersUtf8.emplace_back("Authorization", "Bearer " + prof.apiKeyUtf8);
    }
    if (prof.kind == AiProviderKind::OpenRouter) {
        outHeadersUtf8.emplace_back("HTTP-Referer", "https://cuadernu.local");
        outHeadersUtf8.emplace_back("X-Title", "CuadernU");
    }
    json msgs = json::array();
    if (!systemPromptUtf8.empty()) {
        msgs.push_back({ {"role", "system"}, {"content", systemPromptUtf8} });
    }
    msgs.push_back({ {"role", "user"}, {"content", userPromptUtf8} });
    json body = {
        {"model", model},
        {"stream", false},
        {"temperature", 0.35},
        {"max_tokens", 1400},
        {"messages", std::move(msgs)}
    };
    outBodyUtf8 = body.dump();
    return true;
}

bool AiProviderVault::ParseProviderChatResponse(
    AiProviderKind kind,
    const std::string& rawJsonUtf8,
    std::string& outTextUtf8,
    std::string* outErr)
{
    outTextUtf8.clear();
    try {
        const json rj = json::parse(rawJsonUtf8);

        // Google Gemini native response: candidates[0].content.parts[*].text
        if (rj.contains("candidates") && rj["candidates"].is_array() && !rj["candidates"].empty()) {
            const auto& c0 = rj["candidates"][0];
            if (c0.contains("content") && c0["content"].is_object() &&
                c0["content"].contains("parts") && c0["content"]["parts"].is_array()) {
                std::ostringstream oss;
                for (const auto& part : c0["content"]["parts"]) {
                    if (part.value("thought", false)) continue;
                    if (part.contains("text") && part["text"].is_string()) {
                        oss << part["text"].get<std::string>();
                    }
                }
                outTextUtf8 = TrimAscii(oss.str());
                if (!outTextUtf8.empty()) return true;
            }
        }

        // Anthropic native response: content[*].text
        if (kind == AiProviderKind::Anthropic && rj.contains("content") && rj["content"].is_array()) {
            std::ostringstream oss;
            for (const auto& blk : rj["content"]) {
                if (blk.is_object() && blk.value("type", "") == "text" && blk.contains("text") && blk["text"].is_string()) {
                    oss << blk["text"].get<std::string>();
                }
            }
            outTextUtf8 = TrimAscii(oss.str());
            if (!outTextUtf8.empty()) return true;
        }

        // Ollama /api/chat response: message.content
        if (rj.contains("message") && rj["message"].is_object() && rj["message"].contains("content")) {
            if (rj["message"]["content"].is_string()) {
                outTextUtf8 = TrimAscii(rj["message"]["content"].get<std::string>());
                if (!outTextUtf8.empty()) return true;
            }
        }

        // OpenAI / NVIDIA NIM / OpenCode / OpenRouter / Groq / DeepSeek / Mistral / xAI response: choices[0].message.content
        if (rj.contains("choices") && rj["choices"].is_array() && !rj["choices"].empty()) {
            const auto& ch0 = rj["choices"][0];
            if (ch0.contains("message") && ch0["message"].is_object()) {
                const auto& m = ch0["message"];
                if (m.contains("content") && m["content"].is_string()) {
                    outTextUtf8 = TrimAscii(m["content"].get<std::string>());
                } else if (m.contains("content") && m["content"].is_array()) {
                    std::ostringstream oss;
                    for (const auto& part : m["content"]) {
                        if (part.is_object() && part.contains("text") && part["text"].is_string()) {
                            oss << part["text"].get<std::string>();
                        }
                    }
                    outTextUtf8 = TrimAscii(oss.str());
                }
                if (outTextUtf8.empty() && m.contains("reasoning_content") && m["reasoning_content"].is_string()) {
                    outTextUtf8 = TrimAscii(m["reasoning_content"].get<std::string>());
                }
                if (!outTextUtf8.empty()) return true;
            }
        }

        if (outErr) *outErr = "La respuesta del proveedor no contuvo texto utilizable.";
        return false;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = std::string("Error interpretando respuesta JSON: ") + ex.what();
        return false;
    }
}

bool AiProviderVault::ParseProviderModelsResponse(
    AiProviderKind kind,
    const std::string& rawJsonUtf8,
    std::vector<std::string>& outModelsUtf8)
{
    outModelsUtf8.clear();
    try {
        const json j = json::parse(rawJsonUtf8);
        std::set<std::string> uniq;

        if (kind == AiProviderKind::GoogleGemini && j.contains("models") && j["models"].is_array()) {
            for (const auto& m : j["models"]) {
                if (!m.is_object()) continue;
                std::string name = m.value("name", "");
                if (name.rfind("models/", 0) == 0) name = name.substr(7);
                if (name.empty()) continue;
                bool supportsGen = true;
                if (m.contains("supportedGenerationMethods") && m["supportedGenerationMethods"].is_array()) {
                    supportsGen = false;
                    for (const auto& sm : m["supportedGenerationMethods"]) {
                        if (sm.is_string() && sm.get<std::string>() == "generateContent") {
                            supportsGen = true;
                            break;
                        }
                    }
                }
                if (supportsGen) uniq.insert(name);
            }
        } else if (kind == AiProviderKind::OllamaLocal && j.contains("models") && j["models"].is_array()) {
            for (const auto& m : j["models"]) {
                if (m.is_object()) {
                    const std::string nm = m.value("name", "");
                    if (!nm.empty()) uniq.insert(nm);
                }
            }
        } else if (j.contains("data") && j["data"].is_array()) {
            for (const auto& m : j["data"]) {
                if (m.is_object()) {
                    const std::string id = m.value("id", "");
                    if (!id.empty()) uniq.insert(id);
                }
            }
        } else if (j.contains("models") && j["models"].is_array()) {
            for (const auto& m : j["models"]) {
                if (m.is_string() && !m.get<std::string>().empty()) uniq.insert(m.get<std::string>());
                else if (m.is_object() && m.contains("id") && m["id"].is_string()) uniq.insert(m["id"].get<std::string>());
            }
        }

        outModelsUtf8.assign(uniq.begin(), uniq.end());
        return !outModelsUtf8.empty();
    } catch (...) {
        return false;
    }
}

bool AiProviderVault::HttpRequestWinHttp(
    const std::string& urlUtf8,
    const wchar_t* method,
    const std::string* optionalBodyUtf8,
    const std::vector<std::pair<std::string, std::string>>& extraHeadersUtf8,
    int timeoutMs,
    std::string& outResponseUtf8,
    int* outHttpStatus,
    std::uint64_t* outLatencyMs,
    std::string* outErr)
{
    outResponseUtf8.clear();
    if (outHttpStatus) *outHttpStatus = 0;
    if (outLatencyMs) *outLatencyMs = 0;

#ifdef _WIN32
    const auto t0 = std::chrono::steady_clock::now();
    const std::wstring wUrl = Utf8ToWide(urlUtf8);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostBuf[512]{};
    wchar_t pathBuf[2048]{};
    wchar_t extraBuf[1024]{};
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = static_cast<DWORD>(std::size(hostBuf));
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = static_cast<DWORD>(std::size(pathBuf));
    uc.lpszExtraInfo = extraBuf;
    uc.dwExtraInfoLength = static_cast<DWORD>(std::size(extraBuf));

    if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.size()), 0, &uc)) {
        if (outErr) *outErr = "URL inv\xC3\xA1lida: " + SanitizeUrlForLog(urlUtf8);
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"CuadernU/2.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        if (outErr) *outErr = "WinHttpOpen fall\xC3\xB3.";
        return false;
    }
    const int to = std::clamp(timeoutMs, 2000, 120000);
    WinHttpSetTimeouts(hSession, to, to, to, to);

    HINTERNET hConnect = WinHttpConnect(hSession, hostBuf, uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        if (outErr) *outErr = "No se pudo conectar al host: " + WideToUtf8(hostBuf);
        return false;
    }

    std::wstring fullPath = pathBuf[0] ? std::wstring(pathBuf) : std::wstring(L"/");
    if (extraBuf[0]) fullPath += extraBuf;

    const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, method, fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        if (outErr) *outErr = "WinHttpOpenRequest fall\xC3\xB3.";
        return false;
    }

    std::wstring headers = L"Accept: application/json\r\n";
    if (optionalBodyUtf8) {
        headers += L"Content-Type: application/json; charset=utf-8\r\n";
    }
    for (const auto& kv : extraHeadersUtf8) {
        headers += Utf8ToWide(kv.first) + L": " + Utf8ToWide(kv.second) + L"\r\n";
    }

    const BOOL sent = WinHttpSendRequest(
        hReq,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        optionalBodyUtf8 ? const_cast<char*>(optionalBodyUtf8->data()) : WINHTTP_NO_REQUEST_DATA,
        optionalBodyUtf8 ? static_cast<DWORD>(optionalBodyUtf8->size()) : 0,
        optionalBodyUtf8 ? static_cast<DWORD>(optionalBodyUtf8->size()) : 0,
        0);

    bool ok = false;
    if (sent && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD statusCode = 0;
        DWORD szStatus = sizeof(statusCode);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &szStatus, WINHTTP_NO_HEADER_INDEX);
        if (outHttpStatus) *outHttpStatus = static_cast<int>(statusCode);

        constexpr std::size_t kMaxBytes = 4u * 1024u * 1024u;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0 && outResponseUtf8.size() < kMaxBytes) {
            const DWORD toRead = (std::min<DWORD>)(avail, static_cast<DWORD>(kMaxBytes - outResponseUtf8.size()));
            std::string chunk(toRead, '\0');
            DWORD got = 0;
            if (!WinHttpReadData(hReq, chunk.data(), toRead, &got) || got == 0) break;
            chunk.resize(got);
            outResponseUtf8 += chunk;
        }
        if (statusCode >= 200 && statusCode < 300) {
            ok = true;
        } else if (outErr) {
            std::string detail;
            try {
                const json ej = json::parse(outResponseUtf8);
                if (ej.contains("error") && ej["error"].is_object() && ej["error"].contains("message") && ej["error"]["message"].is_string()) {
                    detail = " — " + ej["error"]["message"].get<std::string>().substr(0, 140);
                } else if (ej.contains("detail") && ej["detail"].is_string()) {
                    detail = " — " + ej["detail"].get<std::string>().substr(0, 140);
                }
            } catch (...) {}
            *outErr = "HTTP " + std::to_string(statusCode) + " (" + SanitizeUrlForLog(urlUtf8) + ")" + detail;
        }
    } else if (outErr) {
        *outErr = "Sin respuesta HTTP de " + SanitizeUrlForLog(urlUtf8) + " (WinError " + std::to_string(GetLastError()) + ")";
    }

    if (outLatencyMs) {
        const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        *outLatencyMs = static_cast<std::uint64_t>((std::max<long long>)(0LL, dt));
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
#else
    (void)urlUtf8; (void)method; (void)optionalBodyUtf8; (void)extraHeadersUtf8; (void)timeoutMs;
    if (outErr) *outErr = "WinHTTP solo disponible en Windows.";
    return false;
#endif
}

bool AiProviderVault::FetchAvailableModels(
    const AiProviderProfile& prof,
    std::vector<std::string>& outModelsUtf8,
    std::string* outErr,
    int* outHttpStatus)
{
    outModelsUtf8.clear();
    if (prof.modelsUrlUtf8.empty()) {
        outModelsUtf8 = prof.suggestedModelsUtf8;
        return !outModelsUtf8.empty();
    }
    if (prof.requiresApiKey && TrimAscii(prof.apiKeyUtf8).empty()) {
        if (outErr) *outErr = "Se requiere configurar una API Key para " + prof.displayNameUtf8 + ".";
        return false;
    }

    std::vector<std::pair<std::string, std::string>> headers;
    if (prof.kind == AiProviderKind::GoogleGemini) {
        headers.emplace_back("x-goog-api-key", prof.apiKeyUtf8);
    } else if (prof.kind == AiProviderKind::Anthropic) {
        headers.emplace_back("x-api-key", prof.apiKeyUtf8);
        headers.emplace_back("anthropic-version", "2023-06-01");
    } else if (!prof.apiKeyUtf8.empty()) {
        headers.emplace_back("Authorization", "Bearer " + prof.apiKeyUtf8);
    }

    std::string rawResp;
    if (!HttpRequestWinHttp(prof.modelsUrlUtf8, L"GET", nullptr, headers, std::clamp(prof.timeoutMs, 4000, 20000), rawResp, outHttpStatus, nullptr, outErr)) {
        return false;
    }
    if (!ParseProviderModelsResponse(prof.kind, rawResp, outModelsUtf8)) {
        if (outErr) *outErr = "No se encontraron modelos en la respuesta de " + prof.displayNameUtf8 + ".";
        return false;
    }
    return true;
}

AiQueryResult AiProviderVault::QuerySingleProvider(
    const AiProviderProfile& prof,
    const std::string& systemPromptUtf8,
    const std::string& userPromptUtf8)
{
    AiQueryResult res{};
    res.providerIdUsedUtf8 = prof.idUtf8;
    res.providerNameUsedUtf8 = prof.displayNameUtf8;
    res.modelUsedUtf8 = prof.modelNameUtf8;

    if (prof.requiresApiKey && TrimAscii(prof.apiKeyUtf8).empty()) {
        res.errorUtf8 = "Clave API no configurada para " + prof.displayNameUtf8 + ".";
        return res;
    }

    std::string url, body;
    std::vector<std::pair<std::string, std::string>> headers;
    if (!BuildProviderChatRequest(prof, systemPromptUtf8, userPromptUtf8, url, headers, body)) {
        res.errorUtf8 = "Perfil de proveedor incompleto (" + prof.displayNameUtf8 + ").";
        return res;
    }

    std::string rawResp;
    if (!HttpRequestWinHttp(url, L"POST", &body, headers, prof.timeoutMs, rawResp, &res.httpStatus, &res.latencyMs, &res.errorUtf8)) {
        return res;
    }

    if (!ParseProviderChatResponse(prof.kind, rawResp, res.replyTextUtf8, &res.errorUtf8)) {
        return res;
    }

    res.ok = true;
    return res;
}

AiQueryResult AiProviderVault::QueryWithFailover(
    const std::string& systemPromptUtf8,
    const std::string& userPromptUtf8) const
{
    AiQueryResult lastRes{};
    if (!globalEnabled_) {
        lastRes.errorUtf8 = "Proveedores de IA desactivados.";
        return lastRes;
    }
    const auto chain = EffectiveFailoverChain();
    if (chain.empty()) {
        lastRes.errorUtf8 = "No hay proveedores de IA configurados.";
        return lastRes;
    }
    for (const auto& pid : chain) {
        const auto* p = FindProvider(pid);
        if (!p) continue;
        AiQueryResult attempt = QuerySingleProvider(*p, systemPromptUtf8, userPromptUtf8);
        if (attempt.ok) return attempt;
        lastRes = std::move(attempt);
    }
    return lastRes;
}

} // namespace DocStudio
