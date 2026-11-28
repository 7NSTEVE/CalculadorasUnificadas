#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace academic::tec_mail_triage {

enum class MailGroup : uint8_t {
    Fecha = 0,
    Accion = 1,
    Info = 2,
    Ruido = 3
};

enum class UrgencyLevel : uint8_t {
    Ninguna = 0,
    Pronto = 1,
    Urgente = 2,
    Vencido = 3
};

struct LocalDateTime {
    int year = 2026;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;

    int64_t ToMinutesKey() const noexcept;
    int64_t ToDayNumber() const noexcept;
    std::string FormatYmd() const;
    std::string FormatHhmm() const;
};

struct DetectedDate {
    LocalDateTime instant{};
    bool has_time = false;
    std::string ymd;
    std::string hhmm;
    std::string context_snippet;
};

struct MailMessage {
    std::string id;
    std::string subject;
    std::string body;
    std::string sender_name;
    std::string sender_smtp;
    std::string folder;
    std::optional<LocalDateTime> received;
    bool has_list_unsubscribe = false;
    bool is_meeting = false;
    bool has_attachment = false;
    bool unread = false;
    bool in_spam = false;

    // Populated by MailTriageEngine::Classify
    MailGroup group = MailGroup::Ruido;
    bool rescued_from_spam = false;
    int score = 0;
    std::vector<std::string> reasons;
    std::vector<std::string> courses;
    std::vector<DetectedDate> dates;
};

struct NotebookLmFile {
    std::string source_path;
    std::string filename;
    int64_t bytes = 0;
    std::string community;
    std::string folder;
};

struct NotebookLmBundle {
    std::string name;
    std::vector<NotebookLmFile> files;
    int64_t total_bytes = 0;
};

struct NotebookLmPlan {
    std::vector<NotebookLmBundle> bundles;
    std::map<std::string, int> discarded_by_ext;
    int total_included = 0;
};

struct NotebookLmBuildSummary {
    int folders_created = 0;
    int hardlinked = 0;
    int copied = 0;
    int already_present = 0;
    int missing_sources = 0;
    int failures = 0;
};

struct OutlookReadResult {
    bool ok = false;
    std::string error_code;
    std::string error_message;
    std::string fix_hint;
    std::vector<MailMessage> messages;
    std::vector<std::string> accounts;
    std::vector<std::string> folder_errors;
    bool spam_read = false;
};

std::string MailGroupToString(MailGroup g);
std::string UrgencyToString(UrgencyLevel u);

} // namespace academic::tec_mail_triage
