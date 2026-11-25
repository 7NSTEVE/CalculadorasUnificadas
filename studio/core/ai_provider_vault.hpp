#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace DocStudio {

enum class AiProviderKind : std::uint8_t {
    GoogleGemini = 0,
    NvidiaNim = 1,
    OpenAI = 2,
    Anthropic = 3,
    OpenCode = 4,
    OpenRouter = 5,
    Groq = 6,
    DeepSeek = 7,
    Mistral = 8,
    XaiGrok = 9,
    OllamaLocal = 10,
    LmStudioLocal = 11,
    CustomOpenAICompat = 12
};

struct AiProviderProfile {
    std::string idUtf8;                 // e.g. "google_gemini", "nvidia_nim"
    std::string displayNameUtf8;        // e.g. "Google Gemini"
    AiProviderKind kind = AiProviderKind::CustomOpenAICompat;
    std::string endpointUrlUtf8;        // chat/generate base URL
    std::string modelsUrlUtf8;          // optional /models or /api/tags URL
    std::string modelNameUtf8;          // active model ID
    std::vector<std::string> suggestedModelsUtf8;
    std::string apiKeyUtf8;             // Stored ONLY in encrypted DPAPI blob on disk
    bool requiresApiKey = true;
    bool enabled = true;
    int timeoutMs = 25000;
};

struct AiQueryResult {
    bool ok = false;
    int httpStatus = 0;
    std::uint64_t latencyMs = 0;
    std::string providerIdUsedUtf8;
    std::string providerNameUsedUtf8;
    std::string modelUsedUtf8;
    std::string replyTextUtf8;
    std::string errorUtf8;              // Strictly sanitized: never contains API keys
};

struct AiKeyImportSummary {
    std::size_t importedCount = 0;
    std::vector<std::string> importedProviderIds;
    std::string noteUtf8;
};

class AiProviderVault {
public:
    AiProviderVault();
    ~AiProviderVault();
    AiProviderVault(const AiProviderVault&) = default;
    AiProviderVault& operator=(const AiProviderVault&) = default;
    AiProviderVault(AiProviderVault&&) noexcept = default;
    AiProviderVault& operator=(AiProviderVault&&) noexcept = default;

    // Reset to built-in 13-provider catalog
    void ResetToDefaultCatalog();

    bool IsEnabled() const noexcept { return globalEnabled_; }
    void SetEnabled(bool enabled) noexcept { globalEnabled_ = enabled; }

    const std::string& ActiveProviderId() const noexcept { return activeProviderId_; }
    bool SetActiveProviderId(const std::string& idUtf8);

    const std::string& FailoverProviderId() const noexcept { return failoverProviderId_; }
    bool SetFailoverProviderId(const std::string& idUtf8);

    std::vector<std::string> EffectiveFailoverChain() const;

    const std::vector<AiProviderProfile>& Providers() const noexcept { return providers_; }
    std::vector<AiProviderProfile>& MutableProviders() noexcept { return providers_; }

    const AiProviderProfile* FindProvider(const std::string& idUtf8) const;
    AiProviderProfile* FindProviderMutable(const std::string& idUtf8);
    const AiProviderProfile* FindProviderByKind(AiProviderKind kind) const;
    AiProviderProfile* FindProviderByKindMutable(AiProviderKind kind);

    bool SetApiKey(const std::string& providerIdUtf8, const std::string& rawKeyUtf8);
    bool ClearApiKey(const std::string& providerIdUtf8);
    bool HasConfiguredCloudOrLocalProvider() const;

    // Persistence: non-sensitive metadata in ai_providers.json, secrets ONLY in Windows DPAPI ai_keys.dpapi
    bool SaveToDirectory(const std::wstring& rootDir, std::string* outErr = nullptr) const;
    bool LoadFromDirectory(const std::wstring& rootDir, std::string* outErr = nullptr);

    // Low-level DPAPI helpers (tested directly in positive & negative unit tests)
    static bool ProtectBytesDpapi(
        const std::vector<std::uint8_t>& plainBytes,
        std::vector<std::uint8_t>& outProtected,
        std::string* outErr = nullptr);
    static bool UnprotectBytesDpapi(
        const std::vector<std::uint8_t>& protectedBytes,
        std::vector<std::uint8_t>& outPlain,
        std::string* outErr = nullptr);

    // Masking & Auto-detection
    static std::string MaskApiKeyForDisplay(const std::string& rawKeyUtf8);
    static AiProviderKind DetectProviderFromKeyPrefix(const std::string& rawKeyUtf8);
    static std::string ProviderIdForKind(AiProviderKind kind);

    // Importers: .env, JSON (flat, providers map, OpenCode auth.json), OS environment variables
    AiKeyImportSummary ImportFromEnvContent(const std::string& envTextUtf8);
    AiKeyImportSummary ImportFromEnvFile(const std::wstring& filePath, std::string* outErr = nullptr);
    AiKeyImportSummary ImportFromJsonContent(const std::string& jsonTextUtf8);
    AiKeyImportSummary ImportFromJsonFile(const std::wstring& filePath, std::string* outErr = nullptr);
    AiKeyImportSummary ImportFromProcessEnvironment();

    // Wire-format builders & response parsers (deterministic, independently testable)
    static bool BuildProviderChatRequest(
        const AiProviderProfile& prof,
        const std::string& systemPromptUtf8,
        const std::string& userPromptUtf8,
        std::string& outUrlUtf8,
        std::vector<std::pair<std::string, std::string>>& outHeadersUtf8,
        std::string& outBodyUtf8);

    static bool ParseProviderChatResponse(
        AiProviderKind kind,
        const std::string& rawJsonUtf8,
        std::string& outTextUtf8,
        std::string* outErr = nullptr);

    static bool ParseProviderModelsResponse(
        AiProviderKind kind,
        const std::string& rawJsonUtf8,
        std::vector<std::string>& outModelsUtf8);

    // Live WinHTTP calls (HTTPS/HTTP) with custom headers, latency measurement, and failover
    static bool HttpRequestWinHttp(
        const std::string& urlUtf8,
        const wchar_t* method,
        const std::string* optionalBodyUtf8,
        const std::vector<std::pair<std::string, std::string>>& extraHeadersUtf8,
        int timeoutMs,
        std::string& outResponseUtf8,
        int* outHttpStatus = nullptr,
        std::uint64_t* outLatencyMs = nullptr,
        std::string* outErr = nullptr);

    static bool FetchAvailableModels(
        const AiProviderProfile& prof,
        std::vector<std::string>& outModelsUtf8,
        std::string* outErr = nullptr,
        int* outHttpStatus = nullptr);

    static AiQueryResult QuerySingleProvider(
        const AiProviderProfile& prof,
        const std::string& systemPromptUtf8,
        const std::string& userPromptUtf8);

    AiQueryResult QueryWithFailover(
        const std::string& systemPromptUtf8,
        const std::string& userPromptUtf8) const;

private:
    bool globalEnabled_ = false;
    std::string activeProviderId_ = "google_gemini";
    std::string failoverProviderId_ = "nvidia_nim";
    std::vector<AiProviderProfile> providers_;
};

} // namespace DocStudio
