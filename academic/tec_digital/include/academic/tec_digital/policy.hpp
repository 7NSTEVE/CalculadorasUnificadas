#pragma once

#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/types.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

// Explicitly opted-in student actions triggered by the user in CuadernU.
// ReadOnlyRequestPolicy defaults to AllowedStudentMutation::None (strictly read-only for automated tests).
enum class AllowedStudentMutation : uint32_t {
    None                     = 0u,
    UploadEvaluation         = 1u << 0, // POST .../evaluation/tda-ce-estudiante/ajax/upload_evaluation
    DeleteEvaluationRevision = 1u << 1, // POST .../evaluation/tda-ce-estudiante/ajax/delete_version_answer
    ManageNotifications      = 1u << 2, // Notifications read/delete/delete_all, file-storage alerts, eval/browser alerts
    RefreshEnrolledCourses   = 1u << 3, // POST /dotlrn/ajax/actualizar_cursos & expediente DAR actualizar_cursos2 / actualizar__datos
    ManagePlagiarism         = 1u << 4, // POST/GET /tda-control-plagio/ajax/(fileUpload|addDocumentEphorus|actualizaVisibilidad)
    ManagePrematricula       = 1u << 5, // GET/POST /tda-prematricula/ajax/(guardar_prematricula|recordRequestR|service_courses_student)
    ManageInclusions         = 1u << 6, // GraphQL student inclusion mutations (SetInclusionRequest|DeleteInclusionRequest|ToogleInclusion|SetScholarship)
    ManageFairs              = 1u << 7, // POST/GET /tda-feria-negocios/ajax/registrar_proyectos/(add_folder|record_project|...)
    ManageProfileAndConsent  = 1u << 8, // GET/POST /tda-um/ajax/(insert_personal_info|save_privacy_settings) & consent set_term_condition_answer
    ManagePersonalFiles      = 1u << 9  // POST/PUT/DELETE /dotlrn/file-storage/api/(folder|file|move|copy)/ & delete-elements
};

inline constexpr uint32_t MutationMask(AllowedStudentMutation m) noexcept {
    return static_cast<uint32_t>(m);
}

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
    uint32_t allowed_mutations_mask = 0u;
    std::string link_label;
};

class ReadOnlyRequestPolicy {
public:
    explicit ReadOnlyRequestPolicy(PolicyOptions opts = {});

    PolicyDecision Evaluate(const PolicyRequestContext& ctx) const;
    PolicyDecision EvaluateHttpRequest(const HttpRequest& req,
                                       bool allow_login = false,
                                       uint32_t allowed_mutations_mask = 0u) const;

    bool IsAllowedOrigin(std::string_view url) const;

    static bool IsReadOnlyGraphQL(std::string_view query);
    static bool IsPrematriculaReadOnly(std::string_view body);
    static bool IsMutatingUiLabel(std::string_view label);
    static bool IsReadOnlyPostPath(std::string_view pathname);
    static bool IsPublicGuiaHorariosReadOnlyPostPath(std::string_view pathname);
    static bool IsAllowedUserMutationRequest(std::string_view method,
                                             std::string_view norm_path,
                                             uint32_t allowed_mutations_mask);

private:
    bool IsTecDigitalOrigin(const ParsedUrl& parsed) const;
    bool IsGraphQlEndpoint(const ParsedUrl& parsed) const;
    bool IsGaapApiOrigin(const ParsedUrl& parsed) const;
    bool IsPublicGuiaHorariosEndpoint(const ParsedUrl& parsed) const;
    static bool IsAllowedStudentGraphQlMutation(std::string_view query, uint32_t allowed_mutations_mask);

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

    // Scoped user-confirmed mutation refcounts (default strictly 0 = read-only).
    void AcquireMutation(AllowedStudentMutation m);
    void ReleaseMutation(AllowedStudentMutation m);
    uint32_t ActiveMutationsMask() const noexcept;

    const ReadOnlyRequestPolicy& Policy() const { return policy_; }

    HttpResponse Execute(const HttpRequest& req) override;

private:
    IHttpTransport& inner_;
    ReadOnlyRequestPolicy policy_;
    std::atomic<bool> allow_login_{false};
    std::atomic<int> allow_login_depth_{0};
    std::atomic<int> mut_upload_depth_{0};
    std::atomic<int> mut_delete_rev_depth_{0};
    std::atomic<int> mut_notif_depth_{0};
    std::atomic<int> mut_refresh_courses_depth_{0};
    std::atomic<int> mut_plagiarism_depth_{0};
    std::atomic<int> mut_prematricula_depth_{0};
    std::atomic<int> mut_inclusions_depth_{0};
    std::atomic<int> mut_fairs_depth_{0};
    std::atomic<int> mut_profile_consent_depth_{0};
    std::atomic<int> mut_personal_files_depth_{0};
};

// RAII guard enabling exactly one user-confirmed mutation kind for the lifetime of a single call.
class ScopedUserMutationGuard {
public:
    ScopedUserMutationGuard(PolicyEnforcingTransport& transport, AllowedStudentMutation mutation)
        : transport_(transport), mutation_(mutation) {
        transport_.AcquireMutation(mutation_);
    }
    ~ScopedUserMutationGuard() {
        transport_.ReleaseMutation(mutation_);
    }
    ScopedUserMutationGuard(const ScopedUserMutationGuard&) = delete;
    ScopedUserMutationGuard& operator=(const ScopedUserMutationGuard&) = delete;

private:
    PolicyEnforcingTransport& transport_;
    AllowedStudentMutation mutation_;
};

}  // namespace academic::tec_digital
