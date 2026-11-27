#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio::Mcp {

struct McpImageBlock {
    std::string mimeTypeUtf8 = "image/png";
    std::vector<std::uint8_t> bytes;
};

struct McpToolResult {
    nlohmann::json data = nlohmann::json::object();
    std::vector<McpImageBlock> images;
    bool isError = false;
    std::string errorMessageUtf8;

    static McpToolResult Ok(nlohmann::json payload = nlohmann::json::object()) {
        McpToolResult r;
        r.data = std::move(payload);
        r.isError = false;
        return r;
    }

    static McpToolResult OkWithImage(nlohmann::json payload, std::vector<std::uint8_t> pngBytes, std::string mime = "image/png") {
        McpToolResult r;
        r.data = std::move(payload);
        if (!pngBytes.empty()) {
            r.images.push_back({ std::move(mime), std::move(pngBytes) });
        }
        r.isError = false;
        return r;
    }

    static McpToolResult Err(std::string msg, nlohmann::json extra = nlohmann::json::object()) {
        McpToolResult r;
        r.isError = true;
        r.errorMessageUtf8 = std::move(msg);
        r.data = std::move(extra);
        return r;
    }
};

struct McpRequestContext {
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
    nlohmann::json progressToken = nullptr;
    std::function<void(double progress, double total, const std::string& message)> reportProgress;

    bool IsCancelled() const noexcept {
        return cancelled && cancelled->load(std::memory_order_acquire);
    }

    void Progress(double progress, double total, const std::string& message = "") const {
        if (reportProgress && !progressToken.is_null()) {
            reportProgress(progress, total, message);
        }
    }
};

using McpToolHandler = std::function<McpToolResult(const nlohmann::json& args, const McpRequestContext& ctx)>;

struct McpToolDef {
    std::string name;
    std::string description;
    nlohmann::json inputSchema;
    McpToolHandler handler;
};

struct McpResourceContent {
    std::string uri;
    std::string mimeType = "application/json";
    std::string textUtf8;
    std::vector<std::uint8_t> blobBytes;
    bool isBlob = false;
};

struct McpResourceDef {
    std::string uri;
    std::string name;
    std::string description;
    std::string mimeType = "application/json";
};

struct McpResourceTemplateDef {
    std::string uriTemplate;
    std::string name;
    std::string description;
    std::string mimeType = "application/json";
};

struct McpPromptArgDef {
    std::string name;
    std::string description;
    bool required = false;
};

struct McpPromptDef {
    std::string name;
    std::string description;
    std::vector<McpPromptArgDef> arguments;
    std::function<nlohmann::json(const nlohmann::json& args)> builder;
};

using McpResourceReader = std::function<bool(const std::string& uri, McpResourceContent& outContent, std::string& outErr)>;

class SecretRedactor {
public:
    void RegisterSecret(const std::string& secret);
    void ClearSecrets();
    std::string RedactString(std::string input) const;
    nlohmann::json RedactJson(nlohmann::json value) const;

private:
    mutable std::mutex mtx_;
    std::vector<std::string> secrets_;
};

std::string Base64Encode(const std::uint8_t* data, std::size_t len);
std::string Base64Encode(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> Base64Decode(const std::string& b64);
std::string GenerateTokenHex192();
bool ConstantTimeEquals(const std::string& a, const std::string& b) noexcept;

class McpProtocolRouter {
public:
    explicit McpProtocolRouter(std::string serverName = "cuadernu", std::string serverVersion = "2.1.0");

    void RegisterTool(McpToolDef def);
    void RegisterResource(McpResourceDef def);
    void RegisterResourceTemplate(McpResourceTemplateDef def);
    void SetResourceReader(McpResourceReader reader);
    void RegisterPrompt(McpPromptDef def);

    SecretRedactor& Redactor() noexcept { return redactor_; }
    const SecretRedactor& Redactor() const noexcept { return redactor_; }

    const std::vector<McpToolDef>& Tools() const noexcept { return tools_; }
    const std::vector<McpResourceDef>& Resources() const noexcept { return resources_; }
    const std::vector<McpResourceTemplateDef>& ResourceTemplates() const noexcept { return resourceTemplates_; }
    const std::vector<McpPromptDef>& Prompts() const noexcept { return prompts_; }

    const std::string& ServerName() const noexcept { return serverName_; }
    const std::string& ServerVersion() const noexcept { return serverVersion_; }
    std::string LogLevel() const {
        std::lock_guard<std::mutex> lk(logLevelMtx_);
        return logLevel_;
    }

    // Synchronous single-line JSON-RPC 2.0 dispatch (returns empty string for notifications!)
    std::string HandleJsonRpcLine(
        const std::string& lineUtf8,
        const McpRequestContext* externalCtx = nullptr,
        std::function<void(const std::string& notificationJsonLine)> emitNotification = nullptr);

    // Direct tool invocation helper for internal C++ callers & tests
    McpToolResult CallToolDirect(const std::string& toolName, const nlohmann::json& args, const McpRequestContext* externalCtx = nullptr);

    // Full binary LF stdio server loop (dups stdout, redirects fd 1 -> stderr, async worker + immediate ping/cancel)
    int RunStdioServer();

    // Optional loopback-only 127.0.0.1 HTTP JSON-RPC server (requires Bearer token)
    int RunHttpServer(std::uint16_t port, const std::string& bearerTokenUtf8, std::atomic<bool>* stopFlag = nullptr);

private:
    std::string serverName_;
    std::string serverVersion_;
    mutable std::mutex logLevelMtx_;
    std::string logLevel_ = "info";
    SecretRedactor redactor_;

    std::vector<McpToolDef> tools_;
    std::unordered_map<std::string, std::size_t> toolIndex_;
    std::vector<McpResourceDef> resources_;
    std::vector<McpResourceTemplateDef> resourceTemplates_;
    McpResourceReader resourceReader_;
    std::vector<McpPromptDef> prompts_;
    std::unordered_map<std::string, std::size_t> promptIndex_;
};

} // namespace DocStudio::Mcp
