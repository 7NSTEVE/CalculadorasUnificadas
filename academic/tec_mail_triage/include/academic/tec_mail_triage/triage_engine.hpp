#pragma once

#include "academic/tec_mail_triage/models.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_mail_triage {

class MailTriageEngine {
public:
    static std::string NormalizeEsAscii(std::string_view input);
    static bool LooksHumanSender(std::string_view smtp);
    static std::vector<std::string> DetectCourseCodes(std::string_view subject, std::string_view body);
    static std::vector<DetectedDate> ExtractSpanishDates(std::string_view text, const LocalDateTime& received);
    static MailMessage Classify(MailMessage msg, const LocalDateTime& now);

    // Human-readable urgency helpers (migrated from Textos.cs)
    static int DaysUntil(const LocalDateTime& target, const LocalDateTime& now) noexcept;
    static UrgencyLevel UrgencyOf(const LocalDateTime& target, const LocalDateTime& now) noexcept;
    static std::string FormatWhen(const DetectedDate& d, const LocalDateTime& now);
    static std::string FormatShortDate(const std::optional<LocalDateTime>& d);
};

class NotebookLmBundler {
public:
    static constexpr int kMaxPerNotebook = 300;

    static std::string RealExtension(std::string_view filename);
    static bool IsCompatible(std::string_view filename);
    static NotebookLmPlan Plan(const std::vector<NotebookLmFile>& files, int max_per_notebook = kMaxPerNotebook);
    static NotebookLmBuildSummary BuildHardlinkFolders(const NotebookLmPlan& plan, const std::string& dest_dir_utf8);
};

class OutlookComReader {
public:
    // Reads Outlook Classic Inbox (6) + Junk (23) via late-bound OLE/IDispatch COM.
    // Never throws; returns structured diagnostics in OutlookReadResult.
    static OutlookReadResult ReadMailboxes(int max_messages = 500, int max_body_chars = 2500);
};

} // namespace academic::tec_mail_triage
