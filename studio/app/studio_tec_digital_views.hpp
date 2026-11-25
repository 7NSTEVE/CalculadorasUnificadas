#pragma once
// ============================================================================
// studio/app/studio_tec_digital_views.hpp
// Native guías de interfaz + material translúcido 2.0 Software Vector Views for TEC Digital
// (0% GPU, 0% idle CPU, zero background timers, PerMonitorV2 4K,
// Porcelain Light / Graphite Dark / High Contrast synchronized).
// ============================================================================

#ifdef _WIN32

#include "../core/utf_conv.hpp"
#include "../integrations/tec_digital/tec_academic_store.hpp"
#include "../../ui_glass/hig_dialog_theme.hpp"
#include "../../ui_glass/translucent_tokens.hpp"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace DocStudio::TecDigitalUi::Views {

namespace td = academic::tec_digital;

// ----------------------------------------------------------------------------
// Shared guías de interfaz Micro-Components (Software Vector GDI+)
// ----------------------------------------------------------------------------

inline void FillSolidRectDc(HDC hdc, int w, int h, UiGlass::Rgba8 c) noexcept {
    if (!hdc || w <= 0 || h <= 0) return;
    RECT rc{ 0, 0, w, h };
    HBRUSH br = CreateSolidBrush(RGB(c.r, c.g, c.b));
    if (br) {
        FillRect(hdc, &rc, br);
        DeleteObject(br);
    }
}

inline void DrawSurface(Gdiplus::Graphics& g, float x, float y, float w, float h, float r,
                        UiGlass::Rgba8 fill, UiGlass::Rgba8 border, float borderW) {
    if (w <= 2.0f || h <= 2.0f) return;
    Gdiplus::GraphicsPath path;
    UiGlass::AddRoundPath(path, x, y, w, h, r);
    Gdiplus::SolidBrush br(UiGlass::ToGdip(fill));
    g.FillPath(&br, &path);
    if (borderW > 0.0f && border.a > 0) {
        Gdiplus::Pen pen(UiGlass::ToGdip(border), borderW);
        g.DrawPath(&pen, &path);
    }
}

inline UiGlass::Rgba8 SoftTint(UiGlass::Rgba8 accent, bool dark) noexcept {
    if (dark) {
        return UiGlass::Rgba8{
            static_cast<BYTE>(28 + (accent.r / 5)),
            static_cast<BYTE>(30 + (accent.g / 5)),
            static_cast<BYTE>(34 + (accent.b / 5)),
            255
        };
    }
    return UiGlass::Rgba8{
        static_cast<BYTE>(255 - ((255 - accent.r) / 7)),
        static_cast<BYTE>(255 - ((255 - accent.g) / 7)),
        static_cast<BYTE>(255 - ((255 - accent.b) / 7)),
        255
    };
}

inline UiGlass::Rgba8 DeptAccent(const std::string& dept, const std::string& type,
                                 const UiGlass::GlassPalette& pal) noexcept {
    if (type != "CLASE") return pal.accentOrange;
    std::string d;
    for (char c : dept) d.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    if (d == "E" || d == "EL" || d == "MI" || d == "EMT") return pal.accentBlue;
    if (d == "FI" || d == "QU" || d == "BI") return pal.accentIndigo;
    if (d == "MA" || d == "EM") return pal.accentPurple;
    if (d == "CA" || d == "IC" || d == "CE" || d == "CES") return pal.accentTeal;
    return pal.accentBlue;
}

inline float DrawPill(Gdiplus::Graphics& g, float x, float y, const std::wstring& text,
                      float dpiScale, UiGlass::Rgba8 fill, UiGlass::Rgba8 fg,
                      bool hc = false, float maxW = 240.0f) {
    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font f(&ff, 11.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::RectF meas{};
    g.MeasureString(text.c_str(), -1, &f, Gdiplus::PointF(0, 0), &meas);
    const float pw = std::min(maxW * dpiScale, meas.Width + 16.0f * dpiScale);
    const float ph = 21.0f * dpiScale;
    DrawSurface(g, x, y, pw, ph, ph * 0.5f, fill, hc ? fg : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.2f : 0.0f);
    Gdiplus::SolidBrush tbr(UiGlass::ToGdip(fg));
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    g.DrawString(text.c_str(), -1, &f, Gdiplus::RectF(x + 4.0f, y, pw - 8.0f, ph), &sf, &tbr);
    return pw;
}

inline void DrawProgressBar(Gdiplus::Graphics& g, float x, float y, float w, float h,
                            float ratio01, UiGlass::Rgba8 track, UiGlass::Rgba8 fill) {
    if (w <= 4.0f || h <= 2.0f) return;
    const float r = h * 0.5f;
    DrawSurface(g, x, y, w, h, r, track, { 0, 0, 0, 0 }, 0.0f);
    const float clamped = std::clamp(ratio01, 0.0f, 1.0f);
    const float fw = std::max(clamped > 0.005f ? h : 0.0f, w * clamped);
    if (fw > 1.0f) {
        DrawSurface(g, x, y, fw, h, r, fill, { 0, 0, 0, 0 }, 0.0f);
    }
}

struct SegmentedHit {
    RECT rc{};
    int index = 0;
};

inline void DrawSegmentedBar(Gdiplus::Graphics& g, float x, float y, float w, float h,
                             const std::vector<std::wstring>& labels, int selectedIdx, int hoverIdx,
                             float dpiScale, const UiGlass::GlassPalette& pal, bool hc,
                             std::vector<SegmentedHit>& outHits) {
    outHits.clear();
    if (labels.empty() || w <= 20.0f) return;
    const float radius = 8.0f * dpiScale;
    DrawSurface(g, x, y, w, h, radius, pal.sidebarBg,
                hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 0.9f);
    const float pad = 3.0f * dpiScale;
    const float segW = (w - pad * 2.0f) / static_cast<float>(labels.size());
    const float segH = h - pad * 2.0f;

    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font fReg(&ff, 12.2f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font fBold(&ff, 12.2f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    for (size_t i = 0; i < labels.size(); ++i) {
        const float sx = x + pad + static_cast<float>(i) * segW;
        const float sy = y + pad;
        const bool sel = (static_cast<int>(i) == selectedIdx);
        const bool hov = (static_cast<int>(i) == hoverIdx && !sel);
        if (sel) {
            DrawSurface(g, sx, sy, segW, segH, 6.0f * dpiScale, pal.contentCardElevated,
                        hc ? pal.accentBlue : pal.borderSubtle, hc ? 1.5f : 0.8f);
        } else if (hov) {
            DrawSurface(g, sx, sy, segW, segH, 6.0f * dpiScale, pal.sidebarHover, { 0, 0, 0, 0 }, 0.0f);
        }
        Gdiplus::SolidBrush tbr(UiGlass::ToGdip(sel ? pal.labelPrimary : pal.labelSecondary));
        g.DrawString(labels[i].c_str(), -1, sel ? &fBold : &fReg,
                     Gdiplus::RectF(sx + 4.0f, sy, std::max(8.0f, segW - 8.0f), segH), &sf, &tbr);
        SegmentedHit hit{};
        hit.rc = { static_cast<LONG>(sx), static_cast<LONG>(sy),
                   static_cast<LONG>(sx + segW), static_cast<LONG>(sy + segH) };
        hit.index = static_cast<int>(i);
        outHits.push_back(hit);
    }
}

inline void DrawKpiTile(Gdiplus::Graphics& g, float x, float y, float w, float h,
                        const std::wstring& title, const std::wstring& bigVal, const std::wstring& sub,
                        UiGlass::IconId icon, UiGlass::Rgba8 accent,
                        float dpiScale, const UiGlass::GlassPalette& pal, bool dark, bool hc) {
    DrawSurface(g, x, y, w, h, 10.0f * dpiScale, pal.contentCardElevated,
                hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 0.9f);

    // Subtle squircle icon chip at top-right
    const float icSz = 26.0f * dpiScale;
    const float icX = x + w - icSz - 12.0f * dpiScale;
    const float icY = y + 11.0f * dpiScale;
    DrawSurface(g, icX, icY, icSz, icSz, 7.0f * dpiScale, SoftTint(accent, dark),
                hc ? accent : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.2f : 0.0f);
    UiGlass::DrawVectorIconGdiPlus(g, icon, icX + icSz * 0.5f, icY + icSz * 0.5f,
                                   13.5f * dpiScale, UiGlass::ToGdip(accent), 1.55f * dpiScale);

    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font fLbl(&ff, 11.2f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font fVal(&ff, 21.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::Font fSub(&ff, 11.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
    Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
    Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
    Gdiplus::StringFormat sf;
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    const float padX = 13.0f * dpiScale;
    g.DrawString(title.c_str(), -1, &fLbl,
                 Gdiplus::RectF(x + padX, y + 11.0f * dpiScale, std::max(20.0f, w - icSz - 28.0f * dpiScale), 16.0f * dpiScale),
                 &sf, &brSec);
    g.DrawString(bigVal.c_str(), -1, &fVal,
                 Gdiplus::RectF(x + padX, y + 30.0f * dpiScale, std::max(20.0f, w - 26.0f * dpiScale), 28.0f * dpiScale),
                 &sf, &brPrim);
    if (!sub.empty()) {
        g.DrawString(sub.c_str(), -1, &fSub,
                     Gdiplus::RectF(x + padX, y + h - 22.0f * dpiScale, std::max(20.0f, w - 26.0f * dpiScale), 16.0f * dpiScale),
                     &sf, &brTert);
    }
}

inline void DrawScrollbarThumb(Gdiplus::Graphics& g, int w, int h, int scrollY, int maxScrollY,
                               float dpiScale, const UiGlass::GlassPalette& pal) {
    if (maxScrollY <= 0 || h <= 40) return;
    const float totalH = static_cast<float>(h + maxScrollY);
    const float barH = std::max(28.0f * dpiScale, static_cast<float>(h * h) / totalH);
    const float trackH = std::max(1.0f, static_cast<float>(h) - barH - 12.0f * dpiScale);
    const float barY = 6.0f * dpiScale + (static_cast<float>(scrollY) / static_cast<float>(maxScrollY)) * trackH;
    DrawSurface(g, static_cast<float>(w) - 7.5f * dpiScale, barY, 4.0f * dpiScale, barH, 2.0f * dpiScale,
                UiGlass::Rgba8{ pal.labelTertiary.r, pal.labelTertiary.g, pal.labelTertiary.b, 125 },
                { 0, 0, 0, 0 }, 0.0f);
}

inline std::wstring FileExtBadgeLabel(const std::string& filename, UiGlass::Rgba8& outColor,
                                      const UiGlass::GlassPalette& pal) {
    std::string ext;
    const size_t dot = filename.rfind('.');
    if (dot != std::string::npos && dot + 1 < filename.size()) {
        ext = filename.substr(dot + 1);
        for (char& c : ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (ext == "PDF") { outColor = pal.accentRed; return L"PDF"; }
    if (ext == "DOC" || ext == "DOCX") { outColor = pal.accentBlue; return L"DOC"; }
    if (ext == "XLS" || ext == "XLSX" || ext == "CSV") { outColor = pal.accentGreen; return L"XLS"; }
    if (ext == "PPT" || ext == "PPTX") { outColor = pal.accentOrange; return L"PPT"; }
    if (ext == "ZIP" || ext == "RAR" || ext == "7Z") { outColor = pal.accentPurple; return L"ZIP"; }
    if (ext == "PNG" || ext == "JPG" || ext == "JPEG") { outColor = pal.accentTeal; return L"IMG"; }
    if (ext.size() >= 2 && ext.size() <= 4) { outColor = pal.accentIndigo; return Utf8ToWide(ext); }
    outColor = pal.labelSecondary;
    return L"DOC";
}

// ----------------------------------------------------------------------------
// GDI+ PNG Capture Helper for Visual Audit CLI (--tec-digital-preview)
// ----------------------------------------------------------------------------

inline bool GetPngEncoderClsid(CLSID* outClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buf(size);
    auto* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    if (Gdiplus::GetImageEncoders(num, size, info) != Gdiplus::Ok) return false;
    for (UINT i = 0; i < num; ++i) {
        if (info[i].MimeType && std::wcscmp(info[i].MimeType, L"image/png") == 0) {
            *outClsid = info[i].Clsid;
            return true;
        }
    }
    return false;
}

inline bool SaveWindowScreenshotPngW(HWND hwnd, const wchar_t* outPngPathW) {
    if (!hwnd || !IsWindow(hwnd) || !outPngPathW || !outPngPathW[0]) return false;
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    const int w = std::max<int>(1, rc.right - rc.left);
    const int h = std::max<int>(1, rc.bottom - rc.top);

    HDC scrDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(scrDc);
    HBITMAP bmp = CreateCompatibleBitmap(scrDc, w, h);
    HGDIOBJ old = SelectObject(memDc, bmp);

    BOOL ok = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT);
    if (!ok) {
        ok = PrintWindow(hwnd, memDc, 0);
    }

    bool saved = false;
    CLSID pngClsid{};
    if (ok && GetPngEncoderClsid(&pngClsid)) {
        Gdiplus::Bitmap gBmp(bmp, nullptr);
        saved = (gBmp.Save(outPngPathW, &pngClsid, nullptr) == Gdiplus::Ok);
    }

    SelectObject(memDc, old);
    DeleteObject(bmp);
    DeleteDC(memDc);
    ReleaseDC(nullptr, scrDc);
    return saved;
}

// ----------------------------------------------------------------------------
// Zero-PII Synthetic Visual-Audit Demo Snapshot (--tec-demo-data ONLY)
// ----------------------------------------------------------------------------

inline DocStudio::TecDigital::TecAcademicSnapshot BuildVisualAuditDemoSnapshot() {
    DocStudio::TecDigital::TecAcademicSnapshot s{};
    s.sessionAuthenticated = true;
    s.syncedAtUtc = "2026-11-25T08:00:00Z";
    s.inclusionServiceStatus = "active";

    // Student general info (synthetic, zero PII)
    td::StudentGeneralInfo gi{};
    gi.career = "Ingeniería en Electrónica";
    gi.weighted_average = 86.42;
    gi.outstanding_courses = 18;
    gi.available_registration_credits = 18;
    s.studentGeneralInfo = gi;

    // 3 Active Courses + 1 Community
    td::CommunityInfo c1{ "101", "el2113", "Circuitos Eléctricos en Corriente Continua GR 50", "CLASE", "E", "S-2-2026", "/dotlrn/classes/e/el2113/el2113-2026-2-50/", "701" };
    td::CommunityInfo c2{ "102", "fi1201", "Física General II GR 50", "CLASE", "FI", "S-2-2026", "/dotlrn/classes/fi/fi1201/fi1201-2026-2-50/", "702" };
    td::CommunityInfo c3{ "103", "ma2104", "Cálculo Superior GR 51", "CLASE", "MA", "S-2-2026", "/dotlrn/classes/ma/ma2104/ma2104-2026-2-51/", "703" };
    td::CommunityInfo c4{ "104", "escuela-e", "Escuela de Ingeniería en Electrónica", "COMUNIDAD", "E", "Permanente", "/dotlrn/escuela/escuela.E/", "704" };
    s.communities = { c1, c2, c3, c4 };

    // Course 1 Overview
    td::CourseOverviewSnapshot ov1{};
    ov1.teacher.name = "Prof. Ejemplo Electrónica";
    ov1.teacher.office = "Edificio B3 · Cubículo 14";
    ov1.teacher.teams_handle = "Consulta K-J 14:00-16:00";
    ov1.teacher.consultation_hours = { "Martes 14:00 - 15:30", "Jueves 14:00 - 15:30" };
    ov1.official_docs = {
        { "Programa oficial del curso EL2113", "https://tecdigital.tec.ac.cr/dotlrn/", "Programa" },
        { "Cronograma Semestre II 2026", "https://tecdigital.tec.ac.cr/dotlrn/", "Cronograma" }
    };
    ov1.weekly_sections = {
        "Semana 11 · Análisis de nodos, mallas y superposición",
        "Semana 12 · Teoremas de Thévenin, Norton y máxima transferencia",
        "Semana 13 · Respuesta transitoria en circuitos RC y RL"
    };
    ov1.tabs = {
        { 0, "Portada", "", false },
        { 1, "Calendario", "", false },
        { 2, "Documentos", "", false },
        { 3, "Evaluaciones", "", false },
        { 5, "Laboratorios y Simulación", "", true }
    };
    s.overviewsByPath[c1.url] = ov1;

    // Course 1 Files
    td::FolderWalkResult fwr1{};
    td::FileRecord f1{}; f1.object_id = "9001"; f1.name = "EL2113_Programa_2S2026.pdf"; f1.folder_path = "Programa"; f1.content_size = 248000; f1.status = td::FileStatus::Downloaded;
    td::FileRecord f2{}; f2.object_id = "9002"; f2.name = "Semana11_Nodos_y_Mallas.pdf"; f2.folder_path = "Presentaciones"; f2.content_size = 1420000; f2.status = td::FileStatus::Downloaded;
    td::FileRecord f3{}; f3.object_id = "9003"; f3.name = "Practica_Thevenin_Norton.docx"; f3.folder_path = "Prácticas"; f3.content_size = 86000; f3.status = td::FileStatus::Pending;
    td::FileRecord f4{}; f4.object_id = "9004"; f4.name = "Guia_Laboratorio_04_RC.pdf"; f4.folder_path = "Laboratorios"; f4.content_size = 512000; f4.status = td::FileStatus::Downloaded; f4.plagiarism_percentage = 4.2;
    fwr1.files = { f1, f2, f3, f4 };
    s.foldersByPath[c1.url] = fwr1;

    // Course 1 Evaluations
    td::DetailedEvaluationsReport ev1{};
    ev1.community_slug = "el2113";
    ev1.unrounded_total = 56.8;
    td::DetailedEvaluationCategory catEx{ "1", "Exámenes Parciales", 60.0, 34.4, {} };
    td::DetailedEvaluationItem ex1{}; ex1.assign_id = "A1"; ex1.name = "Examen Parcial I"; ex1.weight_pct = 20.0; ex1.earned_pct = 17.6; ex1.raw_grade_100 = 88.0; ex1.is_graded = true; ex1.due_date_text = "18/09/2026"; ex1.has_rubric = true;
    td::DetailedEvaluationItem ex2{}; ex2.assign_id = "A2"; ex2.name = "Examen Parcial II"; ex2.weight_pct = 20.0; ex2.earned_pct = 16.8; ex2.raw_grade_100 = 84.0; ex2.is_graded = true; ex2.due_date_text = "23/10/2026";
    td::DetailedEvaluationItem ex3{}; ex3.assign_id = "A3"; ex3.name = "Examen Final"; ex3.weight_pct = 20.0; ex3.earned_pct = 0.0; ex3.is_graded = false; ex3.submission_status = "Pendiente"; ex3.due_date_text = "02/12/2026 13:00"; ex3.due_date_iso = "2026-12-02";
    catEx.items = { ex1, ex2, ex3 };

    td::DetailedEvaluationCategory catLab{ "2", "Laboratorios y Tareas", 40.0, 22.4, {} };
    td::DetailedEvaluationItem lb1{}; lb1.assign_id = "L1"; lb1.name = "Reporte Laboratorio 1-3"; lb1.weight_pct = 25.0; lb1.earned_pct = 22.4; lb1.raw_grade_100 = 89.6; lb1.is_graded = true; lb1.due_date_text = "06/11/2026"; lb1.has_rubric = true; lb1.teacher_comment = "Excelente análisis de incertidumbre y curvas.";
    td::DetailedEvaluationItem lb2{}; lb2.assign_id = "L2"; lb2.name = "Proyecto Circuitos Transitorios"; lb2.weight_pct = 15.0; lb2.earned_pct = 0.0; lb2.is_graded = false; lb2.submission_status = "Sin entregar"; lb2.due_date_text = "28/11/2026 23:59"; lb2.due_date_iso = "2026-11-28";
    catLab.items = { lb1, lb2 };
    ev1.categories = { catEx, catLab };
    s.evaluationsByPath[c1.url] = ev1;

    // News, Members & GAAP
    s.newsByPath[c1.url] = {
        { "Cambio de aula para consulta del jueves", "", "", "La sesión de consulta se traslada al laboratorio B3-12.", "24/11/2026" },
        { "Publicada guía de ejercicios para Examen Final", "", "", "Disponible en Documentos -> Prácticas.", "21/11/2026" }
    };
    s.gaapByPath[c1.url] = {
        { "g1", "Quiz 3 · Teorema de Thévenin", "Quiz", "15/11/2026", 92.0, "Completado" },
        { "g2", "Quiz 4 · Circuitos RC / RL", "Quiz", "29/11/2026", -1.0, "Disponible" }
    };
    s.membersByPath[c1.url] = {
        { "1", "Prof. Ejemplo", "Electrónica", "Prof. Ejemplo Electrónica", "", "Docente" },
        { "2", "Asistente", "Laboratorio", "Asistente de Laboratorio", "", "Asistente" }
    };

    // Weekly Current Schedule
    s.currentSchedule = {
        { 0, "07:30 - 09:20", 450, 560, "EL2113", "Circuitos Eléctricos CC", "50", "B3-04", "EL2113" },
        { 1, "09:30 - 11:20", 570, 680, "FI1201", "Física General II", "50", "D2-01", "FI1201" },
        { 2, "07:30 - 09:20", 450, 560, "EL2113", "Circuitos Eléctricos CC", "50", "B3-04", "EL2113" },
        { 2, "13:00 - 14:50", 780, 890, "MA2104", "Cálculo Superior", "51", "A1-08", "MA2104" },
        { 3, "09:30 - 11:20", 570, 680, "FI1201", "Física General II", "50", "D2-01", "FI1201" },
        { 4, "13:00 - 14:50", 780, 890, "MA2104", "Cálculo Superior", "51", "A1-08", "MA2104" }
    };

    // Unified Calendar Events
    s.unifiedCalendarEvents = {
        { "ev1", "2026-11-26", "14:00", "15:30", "Consulta Examen Final · Circuitos", "EL2113 GR 50", "Edificio B3-12", "calendar" },
        { "ev2", "2026-11-28", "23:59", "", "Entrega: Proyecto Circuitos Transitorios", "EL2113 GR 50", "Valor 15%", "evaluation" },
        { "ev3", "2026-12-01", "08:00", "10:00", "Cita de Matrícula Semestre I 2027", "Matrícula", "Modalidad Semestre", "matricula" },
        { "ev4", "2026-12-02", "13:00", "16:00", "Examen Final · Circuitos Eléctricos CC", "EL2113 GR 50", "Valor 20%", "evaluation" },
        { "ev5", "2026-12-04", "09:30", "12:00", "Examen Final · Física General II", "FI1201 GR 50", "Valor 25%", "evaluation" }
    };

    // Notifications
    td::NotificationsFeed nf{};
    nf.unread_quantity = 3;
    nf.notifications = {
        { "n1", 1, "2026-11-24", "", 1, "Reporte Laboratorio 1-3 · Circuitos Eléctricos", "", "Nueva calificación publicada", "U" },
        { "n2", 1, "2026-11-23", "", 1, "Se agregó Guía_Examen_Final.pdf", "", "Nuevo documento en Física General II", "U" },
        { "n3", 1, "2026-11-22", "", 1, "Proyecto Circuitos Transitorios vence en 3 días", "", "Recordatorio de entrega", "U" },
        { "n4", 1, "2026-11-20", "", 1, "Calendario de exposiciones de proyectos", "", "Escuela de Electrónica", "R" }
    };
    s.notifications = nf;

    // Expediente + Grades
    td::ExpedienteSummary ex{};
    auto& st = ex.structured.stats;
    st.weighted_avg_general = 86.42;
    st.weighted_avg_passed = 88.10;
    st.weighted_avg_failed = 0.0;
    st.passed_pct = 94.0;
    st.hours_taken = 112;
    st.credits_passed = 78;
    st.credits_failed = 0;
    st.credits_total = 148;
    st.count_aprobadas = 24;
    st.count_en_curso = 3;
    st.count_reprobadas = 0;
    st.count_pendientes = 18;
    ex.structured.citas_matricula = { { "2026-12-01", "2027-S1", "Semestre", "08:00 am" } };
    ex.structured.grades = {
        { 2026, "En curso", 0.0, "--", "EL2113", "Circuitos Eléctricos en Corriente Continua", "Semestre", "2", 4 },
        { 2026, "En curso", 0.0, "--", "FI1201", "Física General II", "Semestre", "2", 3 },
        { 2026, "En curso", 0.0, "--", "MA2104", "Cálculo Superior", "Semestre", "2", 4 },
        { 2026, "Aprobada", 90.0, "90", "MA1103", "Cálculo y Álgebra Lineal", "Semestre", "1", 4 },
        { 2026, "Aprobada", 85.0, "85", "FI1101", "Física General I", "Semestre", "1", 3 },
        { 2026, "Aprobada", 92.0, "92", "EL1100", "Introducción a la Electrónica", "Semestre", "1", 2 },
        { 2025, "Aprobada", 82.0, "82", "MA1102", "Cálculo Diferencial e Integral", "Semestre", "2", 4 },
        { 2025, "Aprobada", 88.0, "88", "QU1106", "Química Básica I", "Semestre", "2", 3 }
    };
    s.expediente = ex;

    // Curriculum Graph + Student Profile
    td::CurriculumGraph cg{};
    cg.dsc_curriculum = "Bachillerato en Ingeniería en Electrónica (Plan 410)";
    cg.id_curriculum = 410;
    td::CurriculumLevel lv1{ "1", {
        { "MA1102", "Cálculo Diferencial", "Cálculo Dif.", 4, 6, {}, {}, { "MA1103", "FI1101" }, {}, "" },
        { "QU1106", "Química Básica I", "Química I", 3, 4, {}, {}, {}, {}, "" },
        { "EL1100", "Intro. a la Electrónica", "Intro. Elec.", 2, 3, {}, {}, { "EL2113" }, {}, "" }
    } };
    td::CurriculumLevel lv2{ "2", {
        { "MA1103", "Cálculo y Álgebra Lineal", "Cálc. y Álg.", 4, 6, { "MA1102" }, {}, { "MA2104", "EL2113" }, {}, "" },
        { "FI1101", "Física General I", "Física I", 3, 4, { "MA1102" }, {}, { "FI1201" }, {}, "" }
    } };
    td::CurriculumLevel lv3{ "3", {
        { "EL2113", "Circuitos Eléctricos CC", "Circuitos CC", 4, 6, { "EL1100", "MA1103" }, { "FI1201" }, { "EL2114" }, {}, "" },
        { "FI1201", "Física General II", "Física II", 3, 4, { "FI1101" }, {}, { "EL2114" }, {}, "" },
        { "MA2104", "Cálculo Superior", "Cálc. Sup.", 4, 6, { "MA1103" }, {}, { "EL2207" }, {}, "" }
    } };
    td::CurriculumLevel lv4{ "4", {
        { "EL2114", "Circuitos Eléctricos CA", "Circuitos CA", 4, 6, { "EL2113", "FI1201" }, {}, {}, {}, "" },
        { "EL2207", "Electrónica Analógica I", "Analógica I", 4, 6, { "EL2113", "MA2104" }, {}, {}, {}, "" }
    } };
    cg.levels = { lv1, lv2, lv3, lv4 };
    s.curriculumGraph = cg;

    td::StudentCurriculumProfile sp{};
    sp.grades = {
        { "MA1102", "aprobada", { { 2025, 2, "S", "", "", 1, 82.0, 50, {} } } },
        { "QU1106", "aprobada", { { 2025, 2, "S", "", "", 1, 88.0, 50, {} } } },
        { "EL1100", "aprobada", { { 2026, 1, "S", "", "", 1, 92.0, 50, {} } } },
        { "MA1103", "aprobada", { { 2026, 1, "S", "", "", 1, 90.0, 50, {} } } },
        { "FI1101", "aprobada", { { 2026, 1, "S", "", "", 1, 85.0, 50, {} } } },
        { "EL2113", "en_curso", {} },
        { "FI1201", "en_curso", {} },
        { "MA2104", "en_curso", {} }
    };
    s.studentProfile = sp;

    // Graduate Attributes
    s.graduateAttributes = {
        { "Resolución de problemas de ingeniería", "Modelado de circuitos y sistemas", "EL2113, FI1201", 72.0, 20.0, 32.0, 20.0, 72.0 },
        { "Experimentación y análisis de datos", "Laboratorio de mediciones", "EL2113", 64.0, 18.0, 28.0, 18.0, 64.0 },
        { "Comunicación técnica efectiva", "Reportes técnicos", "EL1100, EL2113", 80.0, 15.0, 35.0, 30.0, 80.0 }
    };

    // Plagiarism Tree
    td::PlagiarismTreeSnapshot pt{};
    pt.my_files_name = "Mis Documentos";
    pt.my_files_folder_id = "700";
    pt.documents = {
        { "doc1", "Guia_Laboratorio_04_RC.pdf", "done", "Completado", 4.2 },
        { "doc2", "Informe_Proyecto_Sensores.docx", "done", "Completado", 7.8 }
    };
    s.plagiarismTree = pt;

    return s;
}

}  // namespace DocStudio::TecDigitalUi::Views

#endif  // _WIN32
