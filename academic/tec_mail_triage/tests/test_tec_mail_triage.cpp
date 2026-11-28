#include "academic/tec_mail_triage/triage_c_api.h"
#include "academic/tec_mail_triage/triage_engine.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace academic::tec_mail_triage;
namespace fs = std::filesystem;

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        ++g_pass;
        std::cout << "[PASS] " << name << "\n";
    } else {
        ++g_fail;
        std::cerr << "[FAIL] " << name << "\n";
    }
}

} // namespace

int main() {
    // 1. C23 Spanish normalization & Costa Rican month ("setiembre") + NotebookLM ".pdf-1"
    Check(MailTriageEngine::NormalizeEsAscii("Evaluación Diseño Eléctrico — Miércoles") ==
              "evaluacion diseno electrico   miercoles",
          "c23_normalize_es_ascii");
    Check(tec_parse_spanish_month("setiembre", 9) == 9 && tec_parse_spanish_month("set", 3) == 9,
          "c23_costa_rica_setiembre");
    Check(tec_days_in_month(2024, 2) == 29 && tec_days_in_month(2026, 2) == 28, "c23_leap_year");
    Check(NotebookLmBundler::RealExtension("guia_semana3.pdf-1") == ".pdf", "c23_tecdigital_pdf_dash_1");
    Check(NotebookLmBundler::IsCompatible("guia_semana3.pdf-1"), "c23_notebooklm_compatible_pdf_dash_1");
    Check(!NotebookLmBundler::IsCompatible("clase_grabada.mp4"), "c23_notebooklm_rejects_mp4");

    // 2. Date extraction (Costa Rica dd/mm + textual + pm time + 45-day grace rollover)
    const LocalDateTime now{ 2026, 8, 1, 10, 0 };
    {
        const auto dates = MailTriageEngine::ExtractSpanishDates(
            "La entrega del proyecto EL3307 es el 14 de agosto a las 7:00 pm sin prorroga.", now);
        Check(dates.size() == 1, "date_extract_count");
        Check(dates[0].ymd == "2026-08-14" && dates[0].has_time && dates[0].hhmm == "19:00", "date_extract_14_ago_19_00");
        Check(MailTriageEngine::FormatWhen(dates[0], LocalDateTime{ 2026, 8, 13, 9, 0 }) == "MANANA · 19:00",
              "urgency_format_manana");
        Check(MailTriageEngine::UrgencyOf(dates[0].instant, LocalDateTime{ 2026, 8, 12, 9, 0 }) == UrgencyLevel::Urgente,
              "urgency_level_urgente");
    }

    // 3. Rule-based TEC Mail Classifier & Spam Rescue (parity with Clasificador.cs)
    {
        MailMessage prof_exam{};
        prof_exam.subject = "Examen parcial EL-3307";
        prof_exam.body = "Estimados, el examen de EL3307 sera el 5 de agosto a las 08:30 am. Obligatorio.";
        prof_exam.sender_smtp = "jose.ramirez@itcr.ac.cr";
        prof_exam.received = now;
        prof_exam.in_spam = true; // TEC spam filter false positive!
        const auto res = MailTriageEngine::Classify(prof_exam, now);
        Check(res.group == MailGroup::Fecha, "classify_prof_exam_as_fecha");
        Check(res.rescued_from_spam, "classify_rescues_from_spam");
        Check(res.courses.size() == 1 && res.courses[0] == "EL3307", "classify_detects_EL3307");
        Check(!res.reasons.empty(), "classify_explainable_reasons");
    }
    {
        MailMessage spam_promo{};
        spam_promo.subject = "Boletin semanal: te invitamos al webinar y sorteo";
        spam_promo.body = "Promocion especial de aniversario.";
        spam_promo.sender_smtp = "comunicaciones@itcr.ac.cr";
        spam_promo.has_list_unsubscribe = true;
        spam_promo.received = now;
        const auto res = MailTriageEngine::Classify(spam_promo, now);
        Check(res.group == MailGroup::Ruido && res.score < 0, "classify_newsletter_as_ruido");
    }

    // 4. NotebookLM Hardlink Bundler (<= 300 files/bundle + NTFS hardlink verification)
    {
        const fs::path tmp_root = fs::temp_directory_path() / "cuadernu_nlm_test_c23";
        std::error_code ec;
        fs::remove_all(tmp_root, ec);
        fs::create_directories(tmp_root / "src", ec);

        auto p2u8 = [](const fs::path& p) {
            const auto u8 = p.u8string();
            return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
        };
        std::vector<NotebookLmFile> files;
        for (int i = 0; i < 6; ++i) {
            const fs::path f = tmp_root / "src" / ("doc_" + std::to_string(i) + ".pdf");
            std::ofstream ofs(f, std::ios::binary);
            ofs << "%PDF-1.4 sample " << i;
            ofs.close();
            files.push_back(NotebookLmFile{ p2u8(f), p2u8(f.filename()), 20, "Fisica III", "Semana 1" });
        }
        files.push_back(NotebookLmFile{ p2u8(tmp_root / "src" / "vid.mp4"), "vid.mp4", 99999, "Fisica III", "Videos" });

        const auto plan = NotebookLmBundler::Plan(files, 4); // split 6 PDFs into 2 bundles (4 + 2), discard 1 mp4
        Check(plan.total_included == 6, "nlm_plan_included_6");
        Check(plan.bundles.size() == 2, "nlm_plan_splits_into_2_bundles");
        Check(plan.discarded_by_ext.at(".mp4") == 1, "nlm_plan_discards_mp4");

        const auto built = NotebookLmBundler::BuildHardlinkFolders(plan, p2u8(tmp_root / "out"));
        Check(built.folders_created == 2 && (built.hardlinked + built.copied) == 6 && built.failures == 0,
              "nlm_build_hardlinks_ok");
        fs::remove_all(tmp_root, ec);
    }

    std::cout << "SUMMARY: passed=" << g_pass << " failed=" << g_fail << "\n";
    if (g_fail == 0) {
        std::cout << "TEC_MAIL_TRIAGE_TESTS_PASS\n";
        return 0;
    }
    return 1;
}
