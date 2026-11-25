#pragma once
// ============================================================================
// doc_studio/integrations/gemini_notebook/gemini_notebook_client.hpp
// Hybrid Google Gemini Notebook Client:
//   - WinHTTP TLS 1.2+ Streaming Transport (64 KiB chunk upload up to 200 MiB)
//   - Native OAuth 2.0 + PKCE (RFC 7636) Loopback Auth + Refresh + gcloud ADC
//   - REST API v1 Client (notebooks, sources, UploadSource, 429 Retry-After)
//   - Streamable HTTP MCP Client (notebook.googleapis.com/mcp, JSON-RPC 2.0)
//   - Local Agent MCP Stdio Bridge (CuadernU.exe --mcp-stdio)
// ============================================================================

#include "gemini_notebook_sync_store.hpp"
#include "gemini_notebook_types.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace DocStudio::GeminiNotebook {

// Default native Windows WinHTTP TLS transport (streaming upload, zero extra DLLs)
class WinHttpNotebookTransport final : public INotebookHttpTransport {
public:
    NotebookHttpResponse SendJson(
        const std::string& methodUtf8,
        const std::string& urlUtf8,
        const std::vector<std::pair<std::string, std::string>>& headersUtf8,
        const std::string* bodyUtf8,
        int timeoutMs) override;

    NotebookHttpResponse UploadFileRaw(
        const std::string& urlUtf8,
        const std::vector<std::pair<std::string, std::string>>& headersUtf8,
        const std::wstring& filePath,
        int timeoutMs,
        std::function<bool()> cancelCheck = nullptr) override;
};

// OAuth 2.0 + PKCE (RFC 7636) helper for personal Google Accounts
class GoogleOAuthPkceHelper {
public:
    static std::string Base64UrlEncode(const std::uint8_t* data, std::size_t len);
    static std::string UrlEncode(const std::string& raw);
    static std::string GenerateCodeVerifier(std::size_t length = 64u);
    static std::string ComputeCodeChallengeS256(const std::string& codeVerifier);

    static std::string BuildAuthorizeUrl(
        const GeminiNotebookConfig& cfg,
        const std::string& redirectUriUtf8,
        const std::string& codeChallengeS256,
        const std::string& stateToken);

    static bool IsWorkspaceAccountRejection(int httpStatus, const std::string& responseBodyUtf8) noexcept;

    // Interactive loopback OAuth flow: binds ephemeral 127.0.0.1:0, launches browser,
    // receives callback code, validates state, exchanges code+verifier for tokens, closes socket.
    static bool RunLoopbackAuthorization(
        const GeminiNotebookConfig& cfg,
        INotebookHttpTransport& transport,
        GeminiNotebookAuthTokens& outTokens,
        int timeoutSeconds = 180,
        std::string* outErr = nullptr,
        std::function<bool(const std::string& authorizeUrl)> browserLauncherOverride = nullptr);

    static bool ExchangeAuthorizationCode(
        const GeminiNotebookConfig& cfg,
        INotebookHttpTransport& transport,
        const std::string& codeUtf8,
        const std::string& redirectUriUtf8,
        const std::string& codeVerifierUtf8,
        GeminiNotebookAuthTokens& outTokens,
        std::string* outErr = nullptr);

    static bool RefreshAccessToken(
        const GeminiNotebookConfig& cfg,
        INotebookHttpTransport& transport,
        GeminiNotebookAuthTokens& inOutTokens,
        std::string* outErr = nullptr);

    // Optional convenience: invoke `gcloud auth application-default print-access-token` if installed
    static bool TryImportGcloudAdcToken(std::string& outAccessTokenUtf8, std::string* outErr = nullptr);
};

// Primary REST API v1 Client + Roadmap Capability Interface
class GeminiNotebookRestClient final : public IGeminiNotebookRoadmapCapabilities {
public:
    GeminiNotebookRestClient(
        GeminiNotebookSyncStore& store,
        INotebookHttpTransport* transport = nullptr,
        UploadRateLimiter* rateLimiter = nullptr);

    void SetCapabilities(const GeminiNotebookCapabilities& caps) { caps_ = caps; }
    GeminiNotebookCapabilities CurrentCapabilities() const override { return caps_; }

    bool EnsureValidAccessToken(std::string* outErr = nullptr);

    bool ListNotebooks(
        std::vector<RemoteNotebookRecord>& outNotebooks,
        std::string* outErr = nullptr,
        bool* outWorkspaceRejected = nullptr);

    bool CreateNotebook(
        const std::string& displayNameUtf8,
        RemoteNotebookRecord& outNotebook,
        std::string* outErr = nullptr,
        bool* outWorkspaceRejected = nullptr);

    bool GetNotebook(
        const std::string& resourceNameUtf8,
        RemoteNotebookRecord& outNotebook,
        std::string* outErr = nullptr);

    bool RenameNotebook(
        const std::string& resourceNameUtf8,
        const std::string& newDisplayNameUtf8,
        RemoteNotebookRecord& outNotebook,
        std::string* outErr = nullptr);

    bool ListSources(
        const std::string& notebookResourceNameUtf8,
        std::vector<RemoteSourceRecord>& outSources,
        std::string* outErr = nullptr);

    bool GetSource(
        const std::string& sourceResourceNameUtf8,
        RemoteSourceRecord& outSource,
        std::string* outErr = nullptr);

    bool CreateWebOrYouTubeSource(
        const std::string& notebookResourceNameUtf8,
        const std::string& displayNameUtf8,
        const std::string& urlUtf8,
        RemoteSourceRecord& outSource,
        std::string* outErr = nullptr,
        bool* outWorkspaceRejected = nullptr);

    bool CreateTextOrMarkdownSource(
        const std::string& notebookResourceNameUtf8,
        const std::string& displayNameUtf8,
        const std::string& textUtf8,
        const std::string& mimeTypeUtf8,
        RemoteSourceRecord& outSource,
        std::string* outErr = nullptr,
        bool* outWorkspaceRejected = nullptr);

    bool UploadBinaryFileSource(
        const std::string& notebookResourceNameUtf8,
        const std::string& displayNameUtf8,
        const std::wstring& filePath,
        const std::string& mimeTypeUtf8,
        RemoteSourceRecord& outSource,
        std::string* outErr = nullptr,
        bool* outWorkspaceRejected = nullptr,
        std::function<bool()> cancelCheck = nullptr);

    // Execute an incremental dry-run plan with rate limiting, retries on 429, and local manifest persistence
    SyncExecutionSummary ExecuteSyncPlan(
        const SyncDiffPlan& plan,
        std::function<void(std::size_t done, std::size_t total, const std::string& statusUtf8)> onProgress = nullptr,
        std::function<bool()> cancelCheck = nullptr,
        bool sleepOnRateLimit = true);

    // Roadmap capabilities (gracefully report PENDING_OFFICIAL_API until Google enables them)
    bool QueryNotebookRemote(
        const std::string& remoteNotebookNameUtf8,
        const std::string& questionUtf8,
        std::string& outAnswerUtf8,
        std::string* outErr = nullptr) override;

    bool GenerateArtifactRemote(
        const std::string& remoteNotebookNameUtf8,
        const std::string& artifactKindUtf8,
        std::string& outArtifactJsonUtf8,
        std::string* outErr = nullptr) override;

    bool DeleteRemoteSource(
        const std::string& remoteSourceNameUtf8,
        std::string* outErr = nullptr) override;

private:
    std::vector<std::pair<std::string, std::string>> BuildAuthHeaders(bool jsonContentType) const;
    static std::uint64_t ParseRetryAfterMs(const NotebookHttpResponse& resp);
    static bool ParseNotebookJson(const std::string& bodyUtf8, RemoteNotebookRecord& outNb);
    static bool ParseSourceJson(const std::string& bodyUtf8, RemoteSourceRecord& outSrc);

    GeminiNotebookSyncStore& store_;
    std::unique_ptr<WinHttpNotebookTransport> ownedTransport_;
    INotebookHttpTransport* transport_ = nullptr;
    std::unique_ptr<UploadRateLimiter> ownedLimiter_;
    UploadRateLimiter* limiter_ = nullptr;
    GeminiNotebookCapabilities caps_{};
};

// Streamable HTTP MCP Client (JSON-RPC 2.0) for https://notebook.googleapis.com/mcp
class GeminiNotebookMcpClient {
public:
    GeminiNotebookMcpClient(
        GeminiNotebookSyncStore& store,
        INotebookHttpTransport* transport = nullptr);

    bool Initialize(std::string* outServerInfoUtf8 = nullptr, std::string* outErr = nullptr);
    bool DiscoverToolsAndCapabilities(GeminiNotebookCapabilities& inOutCaps, std::string* outErr = nullptr);
    bool CallTool(
        const std::string& toolNameUtf8,
        const std::string& argumentsJsonUtf8,
        std::string& outResultJsonUtf8,
        std::string* outErr = nullptr);

    const std::string& SessionId() const noexcept { return sessionId_; }

private:
    bool SendRpc(
        const std::string& methodUtf8,
        const std::string& paramsJsonUtf8,
        std::string& outResultJsonUtf8,
        std::string* outErr);

    GeminiNotebookSyncStore& store_;
    std::unique_ptr<WinHttpNotebookTransport> ownedTransport_;
    INotebookHttpTransport* transport_ = nullptr;
    std::string sessionId_;
    int nextId_ = 1;
};

// Local Agent MCP Bridge (JSON snippet generator + CuadernU.exe --mcp-stdio server)
class CuadernUMcpBridge {
public:
    static std::string GenerateAgentMcpConfigJson(
        const std::wstring& cuadernuExePath,
        const GeminiNotebookConfig& cfg);

    // Handle one JSON-RPC 2.0 request line; returns empty string for notifications (no response required).
    static std::string HandleJsonRpcRequestLine(
        const std::string& lineUtf8,
        GeminiNotebookSyncStore& store,
        INotebookHttpTransport* transportOverride = nullptr);

    // Blocking stdio loop for `CuadernU.exe --mcp-stdio` (0% GPU, no window created)
    static int RunStdioServer(GeminiNotebookSyncStore& store);
};

} // namespace DocStudio::GeminiNotebook
