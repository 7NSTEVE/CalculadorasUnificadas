#pragma once
// ============================================================================
// ui_glass/vector_icons.hpp
// 100% Vector SF-Symbols-equivalent Icon Library for Windows GDI+ & Direct2D
// Crisp optical stroke weight (1.5px), rounded caps/joins, DPI-independent.
// Zero raw emoji or low-res bitmaps in floating material translúcido toolbars & sidebars.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>

namespace UiGlass {

enum class IconId {
    SidebarLeft = 0,
    InspectorRight,
    Document,
    PdfPages,
    InfiniteCanvas,
    HandPan,
    PenTool,
    HighlighterTool,
    EraserTool,
    LassoTool,
    LaserTool,
    ShapeRect,
    ShapeArrow,
    TextMath,
    CropTool,
    Undo,
    Redo,
    Search,
    Save,
    ShareExport,
    StemAtom,
    StudySparkles,
    AcademicCap,
    CalendarGrid,
    MicAudio,
    SlidersGlass,
    SunLight,
    MoonDark,
    ContrastCircle,
    MoreEllipsis,
    ZoomIn,
    ZoomOut,
    Calculator,
    CompareGrid,
    ClockHistory,
    HelpCircle,
    LayersStack,
    SnapMagnet,
    FolderOpen,
    ChevronDown,
    // Extended SF-Symbols-equivalent semantic set for CuadernU full workspace discoverability
    LibraryBooks,
    GraphNetwork,
    DatabaseTable,
    ReadingBook,
    AccessibilityPerson,
    RulerGraduated,
    PaperGrid,
    KeyVault,
    PomodoroTimer,
    Flashcards,
    MindMap,
    CommentBubble,
    SyncCloud,
    Checklist,
    QuoteCite,
    ZoomBox,
    SplitView,
    SettingsGear,
    PlusCircle,
    ChevronRight,
    AudioWave,
    SpeakerTts,
    LinkChain,
    VideoPlay,
    HtmlCode
};

inline void DrawVectorIconGdiPlus(
    Gdiplus::Graphics& g,
    IconId id,
    float cx, float cy,
    float size,
    Gdiplus::Color color,
    float strokeW = 1.55f)
{
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const float u = (std::max)(4.0f, size * 0.44f);
    const float effStroke = strokeW * std::clamp(size / 15.0f, 0.9f, 2.5f);
    Gdiplus::Pen pen(color, effStroke);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush br(color);

    auto roundBox = [&](float x, float y, float w, float h, float r) {
        float rr = (std::min)(r, (std::min)(w, h) * 0.45f);
        float d = rr * 2.0f;
        Gdiplus::GraphicsPath p;
        p.AddArc(x, y, d, d, 180.0f, 90.0f);
        p.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
        p.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
        p.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
        p.CloseFigure();
        g.DrawPath(&pen, &p);
    };

    switch (id) {
    case IconId::SidebarLeft:
        roundBox(cx - u, cy - u * 0.8f, u * 2.0f, u * 1.6f, 2.2f);
        g.DrawLine(&pen, cx - u * 0.34f, cy - u * 0.8f, cx - u * 0.34f, cy + u * 0.8f);
        g.DrawLine(&pen, cx - u * 0.72f, cy - u * 0.32f, cx - u * 0.56f, cy - u * 0.32f);
        g.DrawLine(&pen, cx - u * 0.72f, cy + u * 0.04f, cx - u * 0.56f, cy + u * 0.04f);
        break;
    case IconId::InspectorRight:
        roundBox(cx - u, cy - u * 0.8f, u * 2.0f, u * 1.6f, 2.2f);
        g.DrawLine(&pen, cx + u * 0.34f, cy - u * 0.8f, cx + u * 0.34f, cy + u * 0.8f);
        g.DrawLine(&pen, cx + u * 0.56f, cy - u * 0.32f, cx + u * 0.72f, cy - u * 0.32f);
        g.DrawLine(&pen, cx + u * 0.56f, cy + u * 0.04f, cx + u * 0.72f, cy + u * 0.04f);
        break;
    case IconId::Document:
    case IconId::PdfPages: {
        Gdiplus::PointF pts[5] = {
            { cx - u * 0.68f, cy - u * 0.92f },
            { cx + u * 0.20f, cy - u * 0.92f },
            { cx + u * 0.68f, cy - u * 0.44f },
            { cx + u * 0.68f, cy + u * 0.92f },
            { cx - u * 0.68f, cy + u * 0.92f }
        };
        g.DrawPolygon(&pen, pts, 5);
        g.DrawLine(&pen, cx + u * 0.20f, cy - u * 0.92f, cx + u * 0.20f, cy - u * 0.44f);
        g.DrawLine(&pen, cx + u * 0.20f, cy - u * 0.44f, cx + u * 0.68f, cy - u * 0.44f);
        g.DrawLine(&pen, cx - u * 0.36f, cy + u * 0.12f, cx + u * 0.36f, cy + u * 0.12f);
        g.DrawLine(&pen, cx - u * 0.36f, cy + u * 0.46f, cx + u * 0.20f, cy + u * 0.46f);
        break;
    }
    case IconId::InfiniteCanvas: {
        // Infinity lemniscate loop
        g.DrawEllipse(&pen, cx - u * 0.96f, cy - u * 0.46f, u * 0.96f, u * 0.92f);
        g.DrawEllipse(&pen, cx, cy - u * 0.46f, u * 0.96f, u * 0.92f);
        break;
    }
    case IconId::HandPan: {
        // Clean 4-way pan arrows
        g.DrawLine(&pen, cx - u * 0.85f, cy, cx + u * 0.85f, cy);
        g.DrawLine(&pen, cx, cy - u * 0.85f, cx, cy + u * 0.85f);
        g.DrawLine(&pen, cx - u * 0.55f, cy - u * 0.26f, cx - u * 0.85f, cy);
        g.DrawLine(&pen, cx - u * 0.55f, cy + u * 0.26f, cx - u * 0.85f, cy);
        g.DrawLine(&pen, cx + u * 0.55f, cy - u * 0.26f, cx + u * 0.85f, cy);
        g.DrawLine(&pen, cx + u * 0.55f, cy + u * 0.26f, cx + u * 0.85f, cy);
        g.DrawLine(&pen, cx - u * 0.26f, cy - u * 0.55f, cx, cy - u * 0.85f);
        g.DrawLine(&pen, cx + u * 0.26f, cy - u * 0.55f, cx, cy - u * 0.85f);
        g.DrawLine(&pen, cx - u * 0.26f, cy + u * 0.55f, cx, cy + u * 0.85f);
        g.DrawLine(&pen, cx + u * 0.26f, cy + u * 0.55f, cx, cy + u * 0.85f);
        break;
    }
    case IconId::PenTool: {
        Gdiplus::PointF pts[4] = {
            { cx - u * 0.78f, cy + u * 0.78f },
            { cx - u * 0.48f, cy + u * 0.14f },
            { cx + u * 0.46f, cy - u * 0.80f },
            { cx + u * 0.80f, cy - u * 0.46f }
        };
        g.DrawLine(&pen, pts[0], pts[1]);
        g.DrawLine(&pen, pts[1], pts[2]);
        g.DrawLine(&pen, pts[2], pts[3]);
        g.DrawLine(&pen, pts[3], Gdiplus::PointF(cx - u * 0.14f, cy + u * 0.48f));
        g.DrawLine(&pen, Gdiplus::PointF(cx - u * 0.14f, cy + u * 0.48f), pts[0]);
        break;
    }
    case IconId::HighlighterTool: {
        g.DrawLine(&pen, cx - u * 0.55f, cy + u * 0.75f, cx + u * 0.65f, cy + u * 0.75f);
        g.DrawLine(&pen, cx - u * 0.50f, cy + u * 0.42f, cx + u * 0.35f, cy - u * 0.72f);
        g.DrawLine(&pen, cx - u * 0.08f, cy + u * 0.58f, cx + u * 0.75f, cy - u * 0.42f);
        g.DrawLine(&pen, cx + u * 0.35f, cy - u * 0.72f, cx + u * 0.75f, cy - u * 0.42f);
        break;
    }
    case IconId::EraserTool: {
        Gdiplus::PointF pts[4] = {
            { cx - u * 0.75f, cy + u * 0.20f },
            { cx + u * 0.10f, cy - u * 0.68f },
            { cx + u * 0.78f, cy - u * 0.02f },
            { cx - u * 0.05f, cy + u * 0.76f }
        };
        g.DrawPolygon(&pen, pts, 4);
        g.DrawLine(&pen, cx - u * 0.78f, cy + u * 0.76f, cx + u * 0.72f, cy + u * 0.76f);
        break;
    }
    case IconId::LassoTool: {
        Gdiplus::Pen dashPen(color, effStroke);
        dashPen.SetDashStyle(Gdiplus::DashStyleDash);
        g.DrawEllipse(&dashPen, cx - u * 0.88f, cy - u * 0.64f, u * 1.76f, u * 1.18f);
        g.FillEllipse(&br, cx - u * 0.16f, cy + u * 0.42f, u * 0.36f, u * 0.36f);
        break;
    }
    case IconId::LaserTool: {
        g.FillEllipse(&br, cx - u * 0.32f, cy - u * 0.32f, u * 0.64f, u * 0.64f);
        g.DrawEllipse(&pen, cx - u * 0.78f, cy - u * 0.78f, u * 1.56f, u * 1.56f);
        break;
    }
    case IconId::ShapeRect:
        roundBox(cx - u * 0.82f, cy - u * 0.66f, u * 1.64f, u * 1.32f, 2.5f);
        break;
    case IconId::ShapeArrow:
        g.DrawLine(&pen, cx - u * 0.76f, cy + u * 0.66f, cx + u * 0.74f, cy - u * 0.66f);
        g.DrawLine(&pen, cx + u * 0.18f, cy - u * 0.66f, cx + u * 0.74f, cy - u * 0.66f);
        g.DrawLine(&pen, cx + u * 0.74f, cy - u * 0.10f, cx + u * 0.74f, cy - u * 0.66f);
        break;
    case IconId::TextMath: {
        // Capital Sigma / Math symbol
        Gdiplus::PointF pts[5] = {
            { cx + u * 0.62f, cy - u * 0.76f },
            { cx - u * 0.62f, cy - u * 0.76f },
            { cx + u * 0.08f, cy },
            { cx - u * 0.62f, cy + u * 0.76f },
            { cx + u * 0.62f, cy + u * 0.76f }
        };
        g.DrawLines(&pen, pts, 5);
        break;
    }
    case IconId::CropTool:
        g.DrawLine(&pen, cx - u * 0.48f, cy - u * 0.86f, cx - u * 0.48f, cy + u * 0.52f);
        g.DrawLine(&pen, cx - u * 0.48f, cy + u * 0.52f, cx + u * 0.86f, cy + u * 0.52f);
        g.DrawLine(&pen, cx - u * 0.86f, cy - u * 0.48f, cx + u * 0.48f, cy - u * 0.48f);
        g.DrawLine(&pen, cx + u * 0.48f, cy - u * 0.48f, cx + u * 0.48f, cy + u * 0.86f);
        break;
    case IconId::Undo:
        g.DrawArc(&pen, cx - u * 0.68f, cy - u * 0.46f, u * 1.46f, u * 1.24f, 190.0f, 220.0f);
        g.DrawLine(&pen, cx - u * 0.76f, cy - u * 0.42f, cx - u * 0.30f, cy - u * 0.42f);
        g.DrawLine(&pen, cx - u * 0.76f, cy - u * 0.42f, cx - u * 0.76f, cy + u * 0.04f);
        break;
    case IconId::Redo:
        g.DrawArc(&pen, cx - u * 0.78f, cy - u * 0.46f, u * 1.46f, u * 1.24f, -50.0f, 220.0f);
        g.DrawLine(&pen, cx + u * 0.76f, cy - u * 0.42f, cx + u * 0.30f, cy - u * 0.42f);
        g.DrawLine(&pen, cx + u * 0.76f, cy - u * 0.42f, cx + u * 0.76f, cy + u * 0.04f);
        break;
    case IconId::Search:
        g.DrawEllipse(&pen, cx - u * 0.82f, cy - u * 0.82f, u * 1.26f, u * 1.26f);
        g.DrawLine(&pen, cx + u * 0.24f, cy + u * 0.24f, cx + u * 0.84f, cy + u * 0.84f);
        break;
    case IconId::Save:
        g.DrawLine(&pen, cx, cy - u * 0.80f, cx, cy + u * 0.34f);
        g.DrawLine(&pen, cx - u * 0.44f, cy - u * 0.08f, cx, cy + u * 0.36f);
        g.DrawLine(&pen, cx + u * 0.44f, cy - u * 0.08f, cx, cy + u * 0.36f);
        g.DrawLine(&pen, cx - u * 0.76f, cy + u * 0.76f, cx + u * 0.76f, cy + u * 0.76f);
        break;
    case IconId::ShareExport:
        g.DrawLine(&pen, cx, cy + u * 0.22f, cx, cy - u * 0.84f);
        g.DrawLine(&pen, cx - u * 0.40f, cy - u * 0.44f, cx, cy - u * 0.84f);
        g.DrawLine(&pen, cx + u * 0.40f, cy - u * 0.44f, cx, cy - u * 0.84f);
        g.DrawLine(&pen, cx - u * 0.76f, cy + u * 0.04f, cx - u * 0.76f, cy + u * 0.78f);
        g.DrawLine(&pen, cx - u * 0.76f, cy + u * 0.78f, cx + u * 0.76f, cy + u * 0.78f);
        g.DrawLine(&pen, cx + u * 0.76f, cy + u * 0.78f, cx + u * 0.76f, cy + u * 0.04f);
        break;
    case IconId::StemAtom:
        g.DrawEllipse(&pen, cx - u * 0.88f, cy - u * 0.38f, u * 1.76f, u * 0.76f);
        g.DrawEllipse(&pen, cx - u * 0.38f, cy - u * 0.88f, u * 0.76f, u * 1.76f);
        g.FillEllipse(&br, cx - u * 0.20f, cy - u * 0.20f, u * 0.40f, u * 0.40f);
        break;
    case IconId::StudySparkles: {
        Gdiplus::PointF s1[4] = {
            { cx, cy - u * 0.90f }, { cx + u * 0.24f, cy }, { cx, cy + u * 0.90f }, { cx - u * 0.24f, cy }
        };
        Gdiplus::PointF s2[4] = {
            { cx - u * 0.90f, cy }, { cx, cy - u * 0.24f }, { cx + u * 0.90f, cy }, { cx, cy + u * 0.24f }
        };
        g.DrawPolygon(&pen, s1, 4);
        g.DrawPolygon(&pen, s2, 4);
        break;
    }
    case IconId::AcademicCap: {
        Gdiplus::PointF cap[4] = {
            { cx, cy - u * 0.72f },
            { cx + u * 0.95f, cy - u * 0.22f },
            { cx, cy + u * 0.28f },
            { cx - u * 0.95f, cy - u * 0.22f }
        };
        g.DrawPolygon(&pen, cap, 4);
        g.DrawLine(&pen, cx - u * 0.54f, cy + u * 0.04f, cx - u * 0.54f, cy + u * 0.62f);
        g.DrawLine(&pen, cx - u * 0.54f, cy + u * 0.62f, cx + u * 0.54f, cy + u * 0.62f);
        g.DrawLine(&pen, cx + u * 0.54f, cy + u * 0.62f, cx + u * 0.54f, cy + u * 0.04f);
        g.DrawLine(&pen, cx + u * 0.95f, cy - u * 0.22f, cx + u * 0.95f, cy + u * 0.50f);
        break;
    }
    case IconId::CalendarGrid:
        roundBox(cx - u * 0.84f, cy - u * 0.72f, u * 1.68f, u * 1.52f, 2.2f);
        g.DrawLine(&pen, cx - u * 0.84f, cy - u * 0.22f, cx + u * 0.84f, cy - u * 0.22f);
        g.DrawLine(&pen, cx - u * 0.36f, cy - u * 0.92f, cx - u * 0.36f, cy - u * 0.58f);
        g.DrawLine(&pen, cx + u * 0.36f, cy - u * 0.92f, cx + u * 0.36f, cy - u * 0.58f);
        break;
    case IconId::MicAudio:
        roundBox(cx - u * 0.34f, cy - u * 0.86f, u * 0.68f, u * 1.06f, u * 0.34f);
        g.DrawArc(&pen, cx - u * 0.68f, cy - u * 0.42f, u * 1.36f, u * 1.02f, 0.0f, 180.0f);
        g.DrawLine(&pen, cx, cy + u * 0.60f, cx, cy + u * 0.90f);
        g.DrawLine(&pen, cx - u * 0.36f, cy + u * 0.90f, cx + u * 0.36f, cy + u * 0.90f);
        break;
    case IconId::SlidersGlass:
        g.DrawLine(&pen, cx - u * 0.88f, cy - u * 0.44f, cx + u * 0.88f, cy - u * 0.44f);
        g.FillEllipse(&br, cx - u * 0.46f, cy - u * 0.68f, u * 0.48f, u * 0.48f);
        g.DrawLine(&pen, cx - u * 0.88f, cy + u * 0.44f, cx + u * 0.88f, cy + u * 0.44f);
        g.FillEllipse(&br, cx + u * 0.04f, cy + u * 0.20f, u * 0.48f, u * 0.48f);
        break;
    case IconId::SunLight:
        g.DrawEllipse(&pen, cx - u * 0.42f, cy - u * 0.42f, u * 0.84f, u * 0.84f);
        for (int i = 0; i < 8; ++i) {
            const float a = static_cast<float>(i) * 0.78539816f;
            g.DrawLine(&pen, cx + std::cos(a) * u * 0.64f, cy + std::sin(a) * u * 0.64f,
                             cx + std::cos(a) * u * 0.92f, cy + std::sin(a) * u * 0.92f);
        }
        break;
    case IconId::MoonDark:
        g.DrawArc(&pen, cx - u * 0.75f, cy - u * 0.75f, u * 1.50f, u * 1.50f, 40.0f, 250.0f);
        g.DrawArc(&pen, cx - u * 0.30f, cy - u * 0.62f, u * 1.10f, u * 1.15f, 70.0f, 190.0f);
        break;
    case IconId::ContrastCircle:
        g.DrawEllipse(&pen, cx - u * 0.82f, cy - u * 0.82f, u * 1.64f, u * 1.64f);
        g.FillPie(&br, cx - u * 0.82f, cy - u * 0.82f, u * 1.64f, u * 1.64f, 90.0f, 180.0f);
        break;
    case IconId::MoreEllipsis: {
        const float r = (std::max)(1.4f, u * 0.22f);
        g.FillEllipse(&br, cx - u * 0.65f - r, cy - r, r * 2.0f, r * 2.0f);
        g.FillEllipse(&br, cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.FillEllipse(&br, cx + u * 0.65f - r, cy - r, r * 2.0f, r * 2.0f);
        break;
    }
    case IconId::ZoomIn:
        g.DrawLine(&pen, cx - u * 0.62f, cy, cx + u * 0.62f, cy);
        g.DrawLine(&pen, cx, cy - u * 0.62f, cx, cy + u * 0.62f);
        break;
    case IconId::ZoomOut:
        g.DrawLine(&pen, cx - u * 0.62f, cy, cx + u * 0.62f, cy);
        break;
    case IconId::Calculator:
        roundBox(cx - u * 0.68f, cy - u * 0.88f, u * 1.36f, u * 1.76f, 2.2f);
        g.DrawLine(&pen, cx - u * 0.42f, cy - u * 0.42f, cx + u * 0.42f, cy - u * 0.42f);
        g.FillEllipse(&br, cx - u * 0.36f, cy + u * 0.04f, u * 0.24f, u * 0.24f);
        g.FillEllipse(&br, cx + u * 0.12f, cy + u * 0.04f, u * 0.24f, u * 0.24f);
        g.FillEllipse(&br, cx - u * 0.36f, cy + u * 0.44f, u * 0.24f, u * 0.24f);
        g.FillEllipse(&br, cx + u * 0.12f, cy + u * 0.44f, u * 0.24f, u * 0.24f);
        break;
    case IconId::CompareGrid:
        roundBox(cx - u * 0.88f, cy - u * 0.72f, u * 0.76f, u * 1.44f, 1.8f);
        roundBox(cx + u * 0.12f, cy - u * 0.72f, u * 0.76f, u * 1.44f, 1.8f);
        break;
    case IconId::ClockHistory:
        g.DrawEllipse(&pen, cx - u * 0.84f, cy - u * 0.84f, u * 1.68f, u * 1.68f);
        g.DrawLine(&pen, cx, cy - u * 0.46f, cx, cy + u * 0.04f);
        g.DrawLine(&pen, cx, cy + u * 0.04f, cx + u * 0.42f, cy + u * 0.22f);
        break;
    case IconId::HelpCircle:
        g.DrawEllipse(&pen, cx - u * 0.84f, cy - u * 0.84f, u * 1.68f, u * 1.68f);
        g.DrawArc(&pen, cx - u * 0.34f, cy - u * 0.52f, u * 0.68f, u * 0.58f, 190.0f, 210.0f);
        g.DrawLine(&pen, cx, cy + u * 0.06f, cx, cy + u * 0.28f);
        g.FillEllipse(&br, cx - 1.0f, cy + u * 0.48f, 2.0f, 2.0f);
        break;
    case IconId::LayersStack: {
        Gdiplus::PointF top[4] = {
            { cx, cy - u * 0.86f }, { cx + u * 0.88f, cy - u * 0.30f },
            { cx, cy + u * 0.26f }, { cx - u * 0.88f, cy - u * 0.30f }
        };
        g.DrawPolygon(&pen, top, 4);
        g.DrawLine(&pen, cx - u * 0.80f, cy + u * 0.28f, cx, cy + u * 0.82f);
        g.DrawLine(&pen, cx, cy + u * 0.82f, cx + u * 0.80f, cy + u * 0.28f);
        break;
    }
    case IconId::SnapMagnet:
        g.DrawArc(&pen, cx - u * 0.64f, cy - u * 0.72f, u * 1.28f, u * 1.28f, 180.0f, 180.0f);
        g.DrawLine(&pen, cx - u * 0.64f, cy - u * 0.08f, cx - u * 0.64f, cy + u * 0.70f);
        g.DrawLine(&pen, cx + u * 0.64f, cy - u * 0.08f, cx + u * 0.64f, cy + u * 0.70f);
        break;
    case IconId::FolderOpen: {
        Gdiplus::PointF pts[6] = {
            { cx - u * 0.88f, cy - u * 0.64f },
            { cx - u * 0.22f, cy - u * 0.64f },
            { cx,             cy - u * 0.34f },
            { cx + u * 0.88f, cy - u * 0.34f },
            { cx + u * 0.88f, cy + u * 0.68f },
            { cx - u * 0.88f, cy + u * 0.68f }
        };
        g.DrawPolygon(&pen, pts, 6);
        break;
    }
    case IconId::ChevronDown:
        g.DrawLine(&pen, cx - u * 0.56f, cy - u * 0.24f, cx, cy + u * 0.34f);
        g.DrawLine(&pen, cx, cy + u * 0.34f, cx + u * 0.56f, cy - u * 0.24f);
        break;
    case IconId::ChevronRight:
        g.DrawLine(&pen, cx - u * 0.24f, cy - u * 0.54f, cx + u * 0.34f, cy);
        g.DrawLine(&pen, cx + u * 0.34f, cy, cx - u * 0.24f, cy + u * 0.54f);
        break;
    case IconId::LibraryBooks:
        roundBox(cx - u * 0.90f, cy - u * 0.80f, u * 0.52f, u * 1.60f, 1.5f);
        roundBox(cx - u * 0.26f, cy - u * 0.80f, u * 0.52f, u * 1.60f, 1.5f);
        g.DrawLine(&pen, cx + u * 0.42f, cy - u * 0.72f, cx + u * 0.88f, cy + u * 0.78f);
        g.DrawLine(&pen, cx + u * 0.42f, cy - u * 0.72f, cx + u * 0.74f, cy - u * 0.82f);
        break;
    case IconId::GraphNetwork:
        g.DrawLine(&pen, cx - u * 0.52f, cy - u * 0.42f, cx + u * 0.50f, cy - u * 0.48f);
        g.DrawLine(&pen, cx - u * 0.52f, cy - u * 0.42f, cx - u * 0.10f, cy + u * 0.56f);
        g.DrawLine(&pen, cx + u * 0.50f, cy - u * 0.48f, cx - u * 0.10f, cy + u * 0.56f);
        g.DrawLine(&pen, cx + u * 0.50f, cy - u * 0.48f, cx + u * 0.68f, cy + u * 0.44f);
        g.FillEllipse(&br, cx - u * 0.76f, cy - u * 0.66f, u * 0.48f, u * 0.48f);
        g.FillEllipse(&br, cx + u * 0.26f, cy - u * 0.72f, u * 0.48f, u * 0.48f);
        g.FillEllipse(&br, cx - u * 0.34f, cy + u * 0.32f, u * 0.48f, u * 0.48f);
        g.FillEllipse(&br, cx + u * 0.48f, cy + u * 0.24f, u * 0.40f, u * 0.40f);
        break;
    case IconId::DatabaseTable:
        roundBox(cx - u * 0.86f, cy - u * 0.76f, u * 1.72f, u * 1.52f, 2.2f);
        g.DrawLine(&pen, cx - u * 0.86f, cy - u * 0.22f, cx + u * 0.86f, cy - u * 0.22f);
        g.DrawLine(&pen, cx - u * 0.86f, cy + u * 0.28f, cx + u * 0.86f, cy + u * 0.28f);
        g.DrawLine(&pen, cx - u * 0.22f, cy - u * 0.22f, cx - u * 0.22f, cy + u * 0.76f);
        break;
    case IconId::ReadingBook:
        g.DrawArc(&pen, cx - u * 0.92f, cy - u * 0.66f, u * 0.96f, u * 0.52f, 180.0f, 180.0f);
        g.DrawArc(&pen, cx - u * 0.04f, cy - u * 0.66f, u * 0.96f, u * 0.52f, 180.0f, 180.0f);
        g.DrawLine(&pen, cx - u * 0.92f, cy - u * 0.40f, cx - u * 0.92f, cy + u * 0.66f);
        g.DrawLine(&pen, cx + u * 0.92f, cy - u * 0.40f, cx + u * 0.92f, cy + u * 0.66f);
        g.DrawLine(&pen, cx, cy - u * 0.40f, cx, cy + u * 0.68f);
        g.DrawLine(&pen, cx - u * 0.92f, cy + u * 0.66f, cx, cy + u * 0.54f);
        g.DrawLine(&pen, cx + u * 0.92f, cy + u * 0.66f, cx, cy + u * 0.54f);
        break;
    case IconId::AccessibilityPerson:
        g.DrawEllipse(&pen, cx - u * 0.88f, cy - u * 0.88f, u * 1.76f, u * 1.76f);
        g.FillEllipse(&br, cx - u * 0.16f, cy - u * 0.58f, u * 0.32f, u * 0.32f);
        g.DrawLine(&pen, cx - u * 0.46f, cy - u * 0.14f, cx + u * 0.46f, cy - u * 0.14f);
        g.DrawLine(&pen, cx, cy - u * 0.14f, cx, cy + u * 0.20f);
        g.DrawLine(&pen, cx, cy + u * 0.20f, cx - u * 0.26f, cy + u * 0.58f);
        g.DrawLine(&pen, cx, cy + u * 0.20f, cx + u * 0.26f, cy + u * 0.58f);
        break;
    case IconId::RulerGraduated:
        roundBox(cx - u * 0.92f, cy - u * 0.36f, u * 1.84f, u * 0.72f, 1.8f);
        g.DrawLine(&pen, cx - u * 0.54f, cy - u * 0.36f, cx - u * 0.54f, cy + u * 0.04f);
        g.DrawLine(&pen, cx - u * 0.18f, cy - u * 0.36f, cx - u * 0.18f, cy - u * 0.06f);
        g.DrawLine(&pen, cx + u * 0.18f, cy - u * 0.36f, cx + u * 0.18f, cy + u * 0.04f);
        g.DrawLine(&pen, cx + u * 0.54f, cy - u * 0.36f, cx + u * 0.54f, cy - u * 0.06f);
        break;
    case IconId::PaperGrid:
        roundBox(cx - u * 0.82f, cy - u * 0.82f, u * 1.64f, u * 1.64f, 2.0f);
        g.DrawLine(&pen, cx - u * 0.28f, cy - u * 0.82f, cx - u * 0.28f, cy + u * 0.82f);
        g.DrawLine(&pen, cx + u * 0.28f, cy - u * 0.82f, cx + u * 0.28f, cy + u * 0.82f);
        g.DrawLine(&pen, cx - u * 0.82f, cy - u * 0.28f, cx + u * 0.82f, cy - u * 0.28f);
        g.DrawLine(&pen, cx - u * 0.82f, cy + u * 0.28f, cx + u * 0.82f, cy + u * 0.28f);
        break;
    case IconId::KeyVault:
        g.DrawEllipse(&pen, cx - u * 0.86f, cy - u * 0.40f, u * 0.80f, u * 0.80f);
        g.DrawLine(&pen, cx - u * 0.06f, cy, cx + u * 0.88f, cy);
        g.DrawLine(&pen, cx + u * 0.48f, cy, cx + u * 0.48f, cy + u * 0.34f);
        g.DrawLine(&pen, cx + u * 0.82f, cy, cx + u * 0.82f, cy + u * 0.28f);
        break;
    case IconId::PomodoroTimer:
        g.DrawEllipse(&pen, cx - u * 0.76f, cy - u * 0.64f, u * 1.52f, u * 1.52f);
        g.DrawLine(&pen, cx - u * 0.28f, cy - u * 0.90f, cx + u * 0.28f, cy - u * 0.90f);
        g.DrawLine(&pen, cx, cy - u * 0.90f, cx, cy - u * 0.64f);
        g.DrawLine(&pen, cx, cy + u * 0.12f, cx + u * 0.36f, cy - u * 0.24f);
        break;
    case IconId::Flashcards:
        roundBox(cx - u * 0.84f, cy - u * 0.46f, u * 1.38f, u * 1.14f, 2.0f);
        g.DrawLine(&pen, cx - u * 0.50f, cy - u * 0.76f, cx + u * 0.86f, cy - u * 0.76f);
        g.DrawLine(&pen, cx + u * 0.86f, cy - u * 0.76f, cx + u * 0.86f, cy + u * 0.34f);
        g.DrawLine(&pen, cx - u * 0.54f, cy + u * 0.10f, cx + u * 0.22f, cy + u * 0.10f);
        break;
    case IconId::MindMap:
        roundBox(cx - u * 0.36f, cy - u * 0.28f, u * 0.72f, u * 0.56f, 2.0f);
        g.DrawLine(&pen, cx - u * 0.36f, cy, cx - u * 0.78f, cy - u * 0.48f);
        g.DrawLine(&pen, cx - u * 0.36f, cy, cx - u * 0.78f, cy + u * 0.48f);
        g.DrawLine(&pen, cx + u * 0.36f, cy, cx + u * 0.78f, cy - u * 0.48f);
        g.DrawLine(&pen, cx + u * 0.36f, cy, cx + u * 0.78f, cy + u * 0.48f);
        g.FillEllipse(&br, cx - u * 0.92f, cy - u * 0.62f, u * 0.28f, u * 0.28f);
        g.FillEllipse(&br, cx - u * 0.92f, cy + u * 0.34f, u * 0.28f, u * 0.28f);
        g.FillEllipse(&br, cx + u * 0.64f, cy - u * 0.62f, u * 0.28f, u * 0.28f);
        g.FillEllipse(&br, cx + u * 0.64f, cy + u * 0.34f, u * 0.28f, u * 0.28f);
        break;
    case IconId::CommentBubble:
        roundBox(cx - u * 0.86f, cy - u * 0.74f, u * 1.72f, u * 1.18f, 2.8f);
        g.DrawLine(&pen, cx - u * 0.34f, cy + u * 0.44f, cx - u * 0.60f, cy + u * 0.84f);
        g.DrawLine(&pen, cx - u * 0.60f, cy + u * 0.84f, cx + u * 0.04f, cy + u * 0.44f);
        break;
    case IconId::SyncCloud:
        g.DrawArc(&pen, cx - u * 0.74f, cy - u * 0.74f, u * 1.48f, u * 1.48f, 210.0f, 140.0f);
        g.DrawArc(&pen, cx - u * 0.74f, cy - u * 0.74f, u * 1.48f, u * 1.48f, 30.0f, 140.0f);
        g.DrawLine(&pen, cx + u * 0.64f, cy - u * 0.42f, cx + u * 0.74f, cy - u * 0.08f);
        g.DrawLine(&pen, cx - u * 0.64f, cy + u * 0.42f, cx - u * 0.74f, cy + u * 0.08f);
        break;
    case IconId::Checklist:
        g.DrawLine(&pen, cx - u * 0.84f, cy - u * 0.38f, cx - u * 0.56f, cy - u * 0.14f);
        g.DrawLine(&pen, cx - u * 0.56f, cy - u * 0.14f, cx - u * 0.26f, cy - u * 0.58f);
        g.DrawLine(&pen, cx - u * 0.02f, cy - u * 0.36f, cx + u * 0.84f, cy - u * 0.36f);
        g.DrawLine(&pen, cx - u * 0.84f, cy + u * 0.36f, cx - u * 0.56f, cy + u * 0.60f);
        g.DrawLine(&pen, cx - u * 0.56f, cy + u * 0.60f, cx - u * 0.26f, cy + u * 0.16f);
        g.DrawLine(&pen, cx - u * 0.02f, cy + u * 0.38f, cx + u * 0.84f, cy + u * 0.38f);
        break;
    case IconId::QuoteCite:
        roundBox(cx - u * 0.78f, cy - u * 0.46f, u * 0.54f, u * 0.54f, 1.5f);
        g.DrawLine(&pen, cx - u * 0.24f, cy + u * 0.08f, cx - u * 0.44f, cy + u * 0.56f);
        roundBox(cx + u * 0.16f, cy - u * 0.46f, u * 0.54f, u * 0.54f, 1.5f);
        g.DrawLine(&pen, cx + u * 0.70f, cy + u * 0.08f, cx + u * 0.50f, cy + u * 0.56f);
        break;
    case IconId::ZoomBox:
        roundBox(cx - u * 0.86f, cy - u * 0.72f, u * 1.72f, u * 1.44f, 2.2f);
        g.DrawEllipse(&pen, cx - u * 0.40f, cy - u * 0.38f, u * 0.66f, u * 0.66f);
        g.DrawLine(&pen, cx + u * 0.16f, cy + u * 0.18f, cx + u * 0.46f, cy + u * 0.46f);
        break;
    case IconId::SplitView:
        roundBox(cx - u * 0.88f, cy - u * 0.72f, u * 1.76f, u * 1.44f, 2.2f);
        g.DrawLine(&pen, cx, cy - u * 0.72f, cx, cy + u * 0.72f);
        break;
    case IconId::SettingsGear:
        g.DrawEllipse(&pen, cx - u * 0.34f, cy - u * 0.34f, u * 0.68f, u * 0.68f);
        g.DrawEllipse(&pen, cx - u * 0.72f, cy - u * 0.72f, u * 1.44f, u * 1.44f);
        g.DrawLine(&pen, cx, cy - u * 0.94f, cx, cy - u * 0.72f);
        g.DrawLine(&pen, cx, cy + u * 0.72f, cx, cy + u * 0.94f);
        g.DrawLine(&pen, cx - u * 0.94f, cy, cx - u * 0.72f, cy);
        g.DrawLine(&pen, cx + u * 0.72f, cy, cx + u * 0.94f, cy);
        break;
    case IconId::PlusCircle:
        g.DrawEllipse(&pen, cx - u * 0.84f, cy - u * 0.84f, u * 1.68f, u * 1.68f);
        g.DrawLine(&pen, cx - u * 0.38f, cy, cx + u * 0.38f, cy);
        g.DrawLine(&pen, cx, cy - u * 0.38f, cx, cy + u * 0.38f);
        break;
    case IconId::AudioWave:
        g.DrawLine(&pen, cx - u * 0.72f, cy - u * 0.24f, cx - u * 0.72f, cy + u * 0.24f);
        g.DrawLine(&pen, cx - u * 0.36f, cy - u * 0.62f, cx - u * 0.36f, cy + u * 0.62f);
        g.DrawLine(&pen, cx,             cy - u * 0.84f, cx,             cy + u * 0.84f);
        g.DrawLine(&pen, cx + u * 0.36f, cy - u * 0.48f, cx + u * 0.36f, cy + u * 0.48f);
        g.DrawLine(&pen, cx + u * 0.72f, cy - u * 0.22f, cx + u * 0.72f, cy + u * 0.22f);
        break;
    case IconId::SpeakerTts: {
        Gdiplus::PointF spk[5] = {
            { cx - u * 0.84f, cy - u * 0.28f },
            { cx - u * 0.42f, cy - u * 0.28f },
            { cx + u * 0.04f, cy - u * 0.68f },
            { cx + u * 0.04f, cy + u * 0.68f },
            { cx - u * 0.42f, cy + u * 0.28f }
        };
        g.DrawPolygon(&pen, spk, 5);
        g.DrawArc(&pen, cx - u * 0.04f, cy - u * 0.38f, u * 0.50f, u * 0.76f, -55.0f, 110.0f);
        g.DrawArc(&pen, cx + u * 0.06f, cy - u * 0.64f, u * 0.80f, u * 1.28f, -55.0f, 110.0f);
        break;
    }
    case IconId::LinkChain:
        roundBox(cx - u * 0.90f, cy - u * 0.36f, u * 1.02f, u * 0.72f, u * 0.34f);
        roundBox(cx - u * 0.12f, cy - u * 0.36f, u * 1.02f, u * 0.72f, u * 0.34f);
        break;
    case IconId::VideoPlay: {
        roundBox(cx - u * 0.88f, cy - u * 0.68f, u * 1.76f, u * 1.36f, 2.4f);
        Gdiplus::PointF tri[3] = {
            { cx - u * 0.22f, cy - u * 0.34f },
            { cx + u * 0.42f, cy },
            { cx - u * 0.22f, cy + u * 0.34f }
        };
        g.FillPolygon(&br, tri, 3);
        break;
    }
    case IconId::HtmlCode:
        g.DrawLine(&pen, cx - u * 0.46f, cy - u * 0.46f, cx - u * 0.86f, cy);
        g.DrawLine(&pen, cx - u * 0.86f, cy, cx - u * 0.46f, cy + u * 0.46f);
        g.DrawLine(&pen, cx + u * 0.46f, cy - u * 0.46f, cx + u * 0.86f, cy);
        g.DrawLine(&pen, cx + u * 0.86f, cy, cx + u * 0.46f, cy + u * 0.46f);
        g.DrawLine(&pen, cx + u * 0.16f, cy - u * 0.62f, cx - u * 0.16f, cy + u * 0.62f);
        break;
    }
}

} // namespace UiGlass
