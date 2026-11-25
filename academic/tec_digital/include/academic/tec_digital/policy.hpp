#pragma once

#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/types.hpp"

#include <atomic>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

struct PolicyOptions {
    // Only loopback http:// origins (e.g. "http://127.0.0.1:12345") are accepted here for tests.
    std::vector<std::string> allowed_test_origins;
};

struct PolicyDecision {
    bool allowed = false;
    std::string reason;

    static PolicyDecision Allow() { return {true, {}}; }
    static PolicyDecision Block(std::string r) { return {false, std::move(r)}; }
};

struct PolicyRequestContext {
    std::string url;
    std::string method = "GET";
    std::string resource_type = "document";
    std::string post_data;
    bool allow_login = false;
    std::string link_label;
};

class ReadOnlyRequestPolicy {
public:
    explicit ReadOnlyRequestPolicy(PolicyOptions opts = {});

    PolicyDecision Evaluate(const PolicyRequestContext& ctx) const;
    PolicyDecision EvaluateHttpRequest(const HttpRequest& req,
                                       bool allow_login = false) const;

    bool IsAllowedOrigin(std::string_view url) const;

    static bool IsReadOnlyGraphQL(std::string_view query);
    static bool IsPrematriculaReadOnly(std::string_view body);
    static bool IsMutatingUiLabel(std::string_view label);
    static bool IsReadOnlyPostPath(std::string_view pathname);
    static bool IsPublicGuiaHorariosReadOnlyPostPath(std::string_view pathname);

private:
    bool IsTecDigitalOrigin(const ParsedUrl& parsed) const;
    bool IsGraphQlEndpoint(const ParsedUrl& parsed) const;
    bool IsPublicGuiaHorariosEndpoint(const ParsedUrl& parsed) const;

    PolicyOptions opts_;
};

class PolicyEnforcingTransport final : public IHttpTransport {
public:
    PolicyEnforcingTransport(IHttpTransport& inner,
                             ReadOnlyRequestPolicy policy = ReadOnlyRequestPolicy{});

    void SetAllowLogin(bool allow) { allow_login_.store(allow, std::memory_order_release); }
    bool AllowLogin() const {
        return allow_login_.load(std::memory_order_acquire) ||
               allow_login_depth_.load(std::memory_order_acquire) > 0;
    }
    void AcquireAllowLogin() { allow_login_depth_.fetch_add(1, std::memory_order_acq_rel); }
    void ReleaseAllowLogin() {
        int prev = allow_login_depth_.load(std::memory_order_acquire);
        while (prev > 0 &&
               !allow_login_depth_.compare_exchange_weak(
                   prev, prev - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
        }
    }

    const ReadOnlyRequestPolicy& Policy() const { return policy_; }

    HttpResponse Execute(const HttpRequest& req) override;

private:
    IHttpTransport& inner_;
    ReadOnlyRequestPolicy policy_;
    std::atomic<bool> allow_login_{false};
    std::atomic<int> allow_login_depth_{0};
};

}  // namespace academic::tec_digital
