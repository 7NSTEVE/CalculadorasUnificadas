#pragma once
// ============================================================================
// ui_glass/hig_dialog_theme.hpp
// Unified guías de interfaz + macOS 27 Golden Gate theming for all Win32 dialogs/sheets
// AND a native material translúcido 2.0 Floating Menu (replaces mismatched white Win32
// TrackPopupMenu in Dark Mode and High Contrast).
// Uses ONLY documented Win32/UxTheme/DWM/GDI+ APIs. Zero emojis. DPI-scaled.
// ============================================================================

#include "gdi_translucent.hpp"
#include "translucent_tokens.hpp"
#include "vector_icons.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windowsx.h>

namespace UiGlass {

inline UINT DpiOfWindow(HWND hwnd) noexcept {
    using GetDpiFn = UINT(WINAPI*)(HWND);
    static auto fn = reinterpret_cast<GetDpiFn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn && hwnd) {
        const UINT d = fn(hwnd);
        if (d >= 72u && d <= 480u) return d;
    }
    return 96u;
}

inline int ScaleDpi(int px96, UINT dpi) noexcept {
    return MulDiv(px96, static_cast<int>(dpi), 96);
}

inline void ApplyControlDarkTheme(HWND hCtrl, bool dark) noexcept {
    if (!hCtrl || !IsWindow(hCtrl)) return;
    using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    static HMODULE hUx = LoadLibraryW(L"uxtheme.dll");
    static auto fn = hUx ? reinterpret_cast<SetWindowThemeFn>(GetProcAddress(hUx, "SetWindowTheme")) : nullptr;
    if (!fn) return;
    wchar_t cls[64]{};
    GetClassNameW(hCtrl, cls, 64);
    if (_wcsicmp(cls, L"COMBOBOX") == 0) {
        fn(hCtrl, dark ? L"DarkMode_CFD" : L"Explorer", nullptr);
    } else if (_wcsicmp(cls, L"BUTTON") == 0) {
        const LONG_PTR bs = GetWindowLongPtrW(hCtrl, GWL_STYLE) & BS_TYPEMASK;
        const bool isCheckOrRadio = (bs == BS_CHECKBOX || bs == BS_AUTOCHECKBOX ||
                                     bs == BS_3STATE || bs == BS_AUTO3STATE ||
                                     bs == BS_RADIOBUTTON || bs == BS_AUTORADIOBUTTON ||
                                     bs == BS_GROUPBOX);
        if (isCheckOrRadio && dark) {
            fn(hCtrl, L"", L"");
        } else {
            fn(hCtrl, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        }
    } else {
        fn(hCtrl, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
}

struct HigDialogTheme {
    GlassSettings settings{};
    GlassPalette pal{};
    HBRUSH brBg = nullptr;
    HBRUSH brCard = nullptr;
    HBRUSH brInput = nullptr;

    void Init(HWND hwnd, const GlassSettings* overrideSettings = nullptr) {
        Destroy();
        if (overrideSettings) {
            settings = *overrideSettings;
        } else {
            LoadGlassSettings(settings);
        }
        pal = GetPalette(settings);
        ApplyGoldenGateWindowFrame(hwnd, settings.IsDark(), settings.reduceTransparency || settings.IsHighContrast());
        brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
        brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
        brInput = CreateSolidBrush(pal.contentCardElevated.ToColorRef());
    }

    void ThemeAllChildren(HWND hwnd) const noexcept {
        if (!hwnd) return;
        const bool dark = settings.IsDark();
        EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
            ApplyControlDarkTheme(child, lp != 0);
            return TRUE;
        }, dark ? 1 : 0);
    }

    bool HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT& outResult) const noexcept {
        (void)lp;
        if (msg == WM_ERASEBKGND && brBg) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            FillRect(reinterpret_cast<HDC>(wp), &rc, brBg);
            outResult = 1;
            return true;
        }
        if (msg == WM_CTLCOLORSTATIC && brBg) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            HWND hCtrl = reinterpret_cast<HWND>(lp);
            wchar_t cls[32]{};
            if (hCtrl && GetClassNameW(hCtrl, cls, 32) > 0 && _wcsicmp(cls, L"EDIT") == 0 && brCard) {
                SetTextColor(hdc, pal.labelPrimary.ToColorRef());
                SetBkColor(hdc, pal.contentCard.ToColorRef());
                outResult = reinterpret_cast<LRESULT>(brCard);
                return true;
            }
            SetTextColor(hdc, pal.labelPrimary.ToColorRef());
            SetBkColor(hdc, pal.sidebarBg.ToColorRef());
            SetBkMode(hdc, TRANSPARENT);
            outResult = reinterpret_cast<LRESULT>(brBg);
            return true;
        }
        if ((msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX) && brCard) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            SetTextColor(hdc, pal.labelPrimary.ToColorRef());
            SetBkColor(hdc, pal.contentCard.ToColorRef());
            outResult = reinterpret_cast<LRESULT>(brCard);
            return true;
        }
        if (msg == WM_CTLCOLORBTN && brBg) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            SetTextColor(hdc, pal.labelPrimary.ToColorRef());
            SetBkColor(hdc, pal.sidebarBg.ToColorRef());
            outResult = reinterpret_cast<LRESULT>(brBg);
            return true;
        }
        return false;
    }

    void Destroy() noexcept {
        if (brBg) { DeleteObject(brBg); brBg = nullptr; }
        if (brCard) { DeleteObject(brCard); brCard = nullptr; }
        if (brInput) { DeleteObject(brInput); brInput = nullptr; }
    }

    ~HigDialogTheme() { Destroy(); }
};

// ---------------- Native material translúcido 2.0 Floating Menu ----------------
// Replaces white legacy Win32 TrackPopupMenu with an optically refracted,
// concentric, Light/Dark/HighContrast-synchronized floating glass menu.

struct GlassMenuItem {
    UINT cmdId = 0;
    std::wstring label;
    std::wstring shortcut;
    bool separator = false;
    bool disabled = false;
    bool checked = false;
    bool hasIcon = false;
    IconId icon = IconId::Document;
    std::vector<GlassMenuItem> submenu;

    static GlassMenuItem Sep() {
        GlassMenuItem it{};
        it.separator = true;
        return it;
    }
    static GlassMenuItem Action(UINT id, const std::wstring& text, const std::wstring& key = L"",
                                bool isChecked = false, bool isDisabled = false,
                                bool showIcon = false, IconId ico = IconId::Document) {
        GlassMenuItem it{};
        it.cmdId = id;
        it.label = text;
        it.shortcut = key;
        it.checked = isChecked;
        it.disabled = isDisabled;
        it.hasIcon = showIcon;
        it.icon = ico;
        return it;
    }
    static GlassMenuItem Sub(const std::wstring& text, std::vector<GlassMenuItem> children,
                             bool showIcon = false, IconId ico = IconId::FolderOpen) {
        GlassMenuItem it{};
        it.label = text;
        it.submenu = std::move(children);
        it.hasIcon = showIcon;
        it.icon = ico;
        return it;
    }
};

// Recursively convert an existing Win32 HMENU (already populated with AppendMenuW)
// into a GlassMenuItem hierarchy so ALL menus in CuadernU inherit material translúcido 2.0
// automatically without duplicating 200+ menu item definitions!
inline std::vector<GlassMenuItem> ExtractGlassMenuItemsFromHmenu(HMENU hm) {
    std::vector<GlassMenuItem> out;
    if (!hm) return out;
    const int count = GetMenuItemCount(hm);
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU | MIIM_STRING;
        if (!GetMenuItemInfoW(hm, static_cast<UINT>(i), TRUE, &mii)) continue;
        if ((mii.fType & MFT_SEPARATOR) != 0) {
            out.push_back(GlassMenuItem::Sep());
            continue;
        }
        std::wstring text;
        if (mii.cch > 0) {
            text.resize(static_cast<std::size_t>(mii.cch) + 1u);
            mii.dwTypeData = text.data();
            mii.cch += 1;
            GetMenuItemInfoW(hm, static_cast<UINT>(i), TRUE, &mii);
            text.resize(std::wcslen(text.c_str()));
        }
        // Split trailing "(Ctrl+...)" hint into right-aligned shortcut column cleanly
        std::wstring shortcut;
        if (text.size() > 6 && text.back() == L')') {
            const auto lp = text.rfind(L" (");
            if (lp != std::wstring::npos && lp > 2) {
                const std::wstring cand = text.substr(lp + 2, text.size() - lp - 3);
                if (cand.find(L"Ctrl+") == 0 || cand.find(L"Alt+") == 0 || cand.find(L"F") == 0) {
                    shortcut = cand;
                    text = text.substr(0, lp);
                }
            }
        }
        GlassMenuItem item{};
        item.cmdId = mii.wID;
        item.label = text;
        item.shortcut = shortcut;
        item.checked = (mii.fState & MFS_CHECKED) != 0;
        item.disabled = (mii.fState & (MFS_DISABLED | MFS_GRAYED)) != 0;
        if (mii.hSubMenu) {
            item.submenu = ExtractGlassMenuItemsFromHmenu(mii.hSubMenu);
        }
        out.push_back(std::move(item));
    }
    return out;
}

inline UINT ShowTranslucentMenuItems(
    HWND owner,
    const std::vector<GlassMenuItem>& items,
    POINT screenPt,
    const GlassSettings& settings,
    bool alignRight = false)
{
    if (items.empty()) return 0;

    struct MenuState {
        const std::vector<GlassMenuItem>* items = nullptr;
        GlassSettings settings{};
        UINT dpi = 96;
        int hovered = -1;
        int scrollY = 0;
        int maxScrollY = 0;
        int contentH = 0;
        int winW = 280;
        int winH = 200;
        UINT chosenCmd = 0;
        bool done = false;
        bool subOpen = false;
        std::vector<RECT> itemRects;
        HFONT fNorm = nullptr;
        HFONT fBold = nullptr;
        HFONT fSmall = nullptr;
        HWND hwnd = nullptr;
    } st{};

    st.items = &items;
    st.settings = settings;
    st.dpi = DpiOfWindow(owner);
    st.fNorm = CreateHigUiFont(ScaleDpi(13, st.dpi), FW_NORMAL);
    st.fBold = CreateHigUiFont(ScaleDpi(13, st.dpi), FW_SEMIBOLD);
    st.fSmall = CreateHigUiFont(ScaleDpi(11, st.dpi), FW_NORMAL);

    const int pad = ScaleDpi(8, st.dpi);
    const int rowH = ScaleDpi(28, st.dpi);
    const int sepH = ScaleDpi(9, st.dpi);
    const int iconCol = ScaleDpi(26, st.dpi);

    // Measure ideal width dynamically from longest label + shortcut
    int maxTextW = ScaleDpi(210, st.dpi);
    {
        HDC hdc = GetDC(owner);
        HGDIOBJ oldF = hdc ? SelectObject(hdc, st.fNorm) : nullptr;
        for (const auto& it : items) {
            if (it.separator) continue;
            SIZE sz{};
            if (hdc && GetTextExtentPoint32W(hdc, it.label.c_str(), static_cast<int>(it.label.size()), &sz)) {
                int need = sz.cx + iconCol + ScaleDpi(38, st.dpi);
                if (!it.shortcut.empty()) need += ScaleDpi(68, st.dpi);
                if (!it.submenu.empty()) need += ScaleDpi(20, st.dpi);
                if (need > maxTextW) maxTextW = need;
            }
        }
        if (hdc && oldF) SelectObject(hdc, oldF);
        if (hdc) ReleaseDC(owner, hdc);
    }
    st.winW = std::clamp(maxTextW + pad * 2, ScaleDpi(220, st.dpi), ScaleDpi(440, st.dpi));

    int cy = pad;
    st.itemRects.resize(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        const int ih = items[i].separator ? sepH : rowH;
        st.itemRects[i] = { pad, cy, st.winW - pad, cy + ih };
        cy += ih;
    }
    st.contentH = cy + pad;

    // Clamp to active monitor work area so menus NEVER clip off-screen
    HMONITOR hMon = MonitorFromPoint(screenPt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    RECT work{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    if (GetMonitorInfoW(hMon, &mi)) work = mi.rcWork;

    const int maxAvailH = (std::max)(ScaleDpi(180, st.dpi), static_cast<int>(work.bottom - work.top) - ScaleDpi(24, st.dpi));
    st.winH = (std::min)(st.contentH, maxAvailH);
    st.maxScrollY = (std::max)(0, st.contentH - st.winH);

    int wx = alignRight ? (screenPt.x - st.winW) : screenPt.x;
    int wy = screenPt.y;
    if (wx + st.winW > work.right - 6) wx = work.right - st.winW - 6;
    if (wx < work.left + 6) wx = work.left + 6;
    if (wy + st.winH > work.bottom - 6) wy = (std::max)(static_cast<int>(work.top) + 6, static_cast<int>(work.bottom) - st.winH - 6);
    if (wy < work.top + 6) wy = work.top + 6;

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<MenuState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto openSub = [&](int idx) {
                if (idx < 0 || static_cast<std::size_t>(idx) >= s->items->size()) return;
                const auto& it = (*s->items)[static_cast<std::size_t>(idx)];
                if (it.submenu.empty() || it.disabled) return;
                RECT wr{};
                GetWindowRect(hwnd, &wr);
                POINT subPt{ wr.right - ScaleDpi(4, s->dpi), wr.top + s->itemRects[static_cast<std::size_t>(idx)].top - s->scrollY };
                s->subOpen = true;
                const UINT subCmd = ShowTranslucentMenuItems(hwnd, it.submenu, subPt, s->settings, false);
                s->subOpen = false;
                if (subCmd != 0) {
                    s->chosenCmd = subCmd;
                    s->done = true;
                    DestroyWindow(hwnd);
                } else {
                    SetForegroundWindow(hwnd);
                    SetFocus(hwnd);
                }
            };

            switch (msg) {
            case WM_CREATE: {
                ApplyGoldenGateWindowFrame(hwnd, s->settings.IsDark(), s->settings.reduceTransparency || s->settings.IsHighContrast());
                const int cr = ScaleDpi(14, s->dpi);
                HRGN rgn = CreateRoundRectRgn(0, 0, s->winW + 1, s->winH + 1, cr * 2, cr * 2);
                SetWindowRgn(hwnd, rgn, TRUE);
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_MOUSEWHEEL: {
                if (s->maxScrollY > 0) {
                    const int d = GET_WHEEL_DELTA_WPARAM(wp);
                    s->scrollY = std::clamp(s->scrollY - d / 2, 0, s->maxScrollY);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            case WM_MOUSEMOVE: {
                const int mx = GET_X_LPARAM(lp);
                const int my = GET_Y_LPARAM(lp) + s->scrollY;
                int hit = -1;
                for (std::size_t i = 0; i < s->itemRects.size(); ++i) {
                    if ((*s->items)[i].separator || (*s->items)[i].disabled) continue;
                    const RECT& r = s->itemRects[i];
                    if (mx >= r.left && mx < r.right && my >= r.top && my < r.bottom) {
                        hit = static_cast<int>(i);
                        break;
                    }
                }
                if (hit != s->hovered) {
                    s->hovered = hit;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            case WM_LBUTTONUP:
            case WM_LBUTTONDOWN: {
                if (msg == WM_LBUTTONDOWN) return 0;
                const int mx = GET_X_LPARAM(lp);
                const int my = GET_Y_LPARAM(lp);
                if (mx < 0 || my < 0 || mx >= s->winW || my >= s->winH) {
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                const int ly = my + s->scrollY;
                for (std::size_t i = 0; i < s->itemRects.size(); ++i) {
                    const auto& it = (*s->items)[i];
                    if (it.separator || it.disabled) continue;
                    const RECT& r = s->itemRects[i];
                    if (mx >= r.left && mx < r.right && ly >= r.top && ly < r.bottom) {
                        if (!it.submenu.empty()) {
                            openSub(static_cast<int>(i));
                        } else if (it.cmdId != 0) {
                            s->chosenCmd = it.cmdId;
                            s->done = true;
                            DestroyWindow(hwnd);
                        }
                        return 0;
                    }
                }
                return 0;
            }
            case WM_KEYDOWN: {
                if (wp == VK_ESCAPE || wp == VK_LEFT) {
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (wp == VK_DOWN || wp == VK_UP) {
                    const int step = (wp == VK_DOWN) ? 1 : -1;
                    const int n = static_cast<int>(s->items->size());
                    int cur = s->hovered;
                    for (int k = 0; k < n; ++k) {
                        cur = (cur + step + n) % n;
                        if (!(*s->items)[static_cast<std::size_t>(cur)].separator &&
                            !(*s->items)[static_cast<std::size_t>(cur)].disabled) {
                            s->hovered = cur;
                            const RECT& r = s->itemRects[static_cast<std::size_t>(cur)];
                            if (r.top < s->scrollY) s->scrollY = (std::max)(0, static_cast<int>(r.top) - 8);
                            if (r.bottom > s->scrollY + s->winH) s->scrollY = (std::min)(s->maxScrollY, static_cast<int>(r.bottom) - s->winH + 8);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            break;
                        }
                    }
                    return 0;
                }
                if ((wp == VK_RETURN || wp == VK_RIGHT) && s->hovered >= 0) {
                    const auto& it = (*s->items)[static_cast<std::size_t>(s->hovered)];
                    if (!it.submenu.empty()) {
                        openSub(s->hovered);
                    } else if (wp == VK_RETURN && it.cmdId != 0) {
                        s->chosenCmd = it.cmdId;
                        s->done = true;
                        DestroyWindow(hwnd);
                    }
                    return 0;
                }
                return 0;
            }
            case WM_ACTIVATE: {
                if (LOWORD(wp) == WA_INACTIVE && !s->subOpen && !s->done) {
                    s->done = true;
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                HDC mem = CreateCompatibleDC(hdc);
                HBITMAP bmp = CreateCompatibleBitmap(hdc, s->winW, s->winH);
                HGDIOBJ old = SelectObject(mem, bmp);

                const GlassPalette pal = GetPalette(s->settings);
                const float outerR = static_cast<float>(ScaleDpi(14, s->dpi));
                const float innerR = ComputeConcentricRadius(outerR, static_cast<float>(ScaleDpi(6, s->dpi)), 6.0f);

                // Zero-GPU guías de interfaz flat floating surface (clean solid card + crisp 1px border, no Dual Kawase blur)
                {
                    Gdiplus::Graphics gBg(mem);
                    gBg.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                    Gdiplus::GraphicsPath panelPath;
                    AddRoundPath(panelPath, 0.5f, 0.5f, static_cast<float>(s->winW) - 1.0f, static_cast<float>(s->winH) - 1.0f, outerR);
                    Gdiplus::SolidBrush fillBr(ToGdip(pal.contentCard));
                    gBg.FillPath(&fillBr, &panelPath);
                    Gdiplus::Pen borderPen(ToGdip(s->settings.IsHighContrast() ? pal.borderStrong : pal.borderSubtle), 1.0f);
                    gBg.DrawPath(&borderPen, &panelPath);
                }

                Gdiplus::Graphics g(mem);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                SetBkMode(mem, TRANSPARENT);

                const int iconX = ScaleDpi(18, s->dpi);
                const int txtLeft = ScaleDpi(30, s->dpi);
                for (std::size_t i = 0; i < s->items->size(); ++i) {
                    const auto& it = (*s->items)[i];
                    RECT r = s->itemRects[i];
                    OffsetRect(&r, 0, -s->scrollY);
                    if (r.bottom < 0 || r.top > s->winH) continue;

                    if (it.separator) {
                        const float sy = static_cast<float>(r.top + r.bottom) * 0.5f;
                        Gdiplus::Pen sp(ToGdip(pal.separator), 1.0f);
                        g.DrawLine(&sp, static_cast<float>(r.left + 8), sy, static_cast<float>(r.right - 8), sy);
                        continue;
                    }

                    const bool hov = (static_cast<int>(i) == s->hovered) && !it.disabled;
                    if (hov) {
                        Gdiplus::GraphicsPath hp;
                        AddRoundPath(hp, static_cast<float>(r.left), static_cast<float>(r.top),
                            static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top), innerR);
                        Gdiplus::SolidBrush hb(ToGdip(pal.accentBlue, s->settings.IsDark() ? 220 : 235));
                        g.FillPath(&hb, &hp);
                    }

                    const COLORREF fgCol = it.disabled
                        ? pal.labelTertiary.ToColorRef()
                        : (hov ? RGB(255, 255, 255) : pal.labelPrimary.ToColorRef());
                    const Gdiplus::Color iconCol = it.disabled
                        ? ToGdip(pal.labelTertiary)
                        : (hov ? Gdiplus::Color(255, 255, 255, 255) : ToGdip(pal.accentBlue));
                    const float cyMid = static_cast<float>(r.top + r.bottom) * 0.5f;

                    if (it.checked) {
                        SelectObject(mem, s->fBold);
                        SetTextColor(mem, hov ? RGB(255, 255, 255) : pal.accentBlue.ToColorRef());
                        RECT chkRc{ r.left + 4, r.top, r.left + txtLeft - 4, r.bottom };
                        DrawTextW(mem, L"\u2713", -1, &chkRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    } else if (it.hasIcon) {
                        DrawVectorIconGdiPlus(g, it.icon, static_cast<float>(r.left + iconX - 4), cyMid,
                            static_cast<float>(ScaleDpi(13, s->dpi)), iconCol, 1.45f);
                    }

                    SelectObject(mem, hov ? s->fBold : s->fNorm);
                    SetTextColor(mem, fgCol);
                    const int rightReserve = (!it.submenu.empty() ? ScaleDpi(22, s->dpi) :
                        (!it.shortcut.empty() ? ScaleDpi(74, s->dpi) : ScaleDpi(10, s->dpi)));
                    RECT lblRc{ r.left + txtLeft, r.top, r.right - rightReserve, r.bottom };
                    DrawTextW(mem, it.label.c_str(), -1, &lblRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                    if (!it.submenu.empty()) {
                        DrawVectorIconGdiPlus(g, IconId::ChevronRight, static_cast<float>(r.right - ScaleDpi(12, s->dpi)), cyMid,
                            static_cast<float>(ScaleDpi(11, s->dpi)),
                            hov ? Gdiplus::Color(255, 255, 255, 255) : ToGdip(pal.labelSecondary), 1.45f);
                    } else if (!it.shortcut.empty()) {
                        SelectObject(mem, s->fSmall);
                        SetTextColor(mem, hov ? RGB(235, 244, 255) : pal.labelTertiary.ToColorRef());
                        RECT scRc{ r.right - ScaleDpi(72, s->dpi), r.top, r.right - ScaleDpi(10, s->dpi), r.bottom };
                        DrawTextW(mem, it.shortcut.c_str(), -1, &scRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    }
                }

                // Subtle scroll indicator if menu scrolls
                if (s->maxScrollY > 0) {
                    const float barH = (std::max)(24.0f, static_cast<float>(s->winH * s->winH) / static_cast<float>(s->contentH));
                    const float barY = (static_cast<float>(s->scrollY) / static_cast<float>(s->maxScrollY)) * (static_cast<float>(s->winH) - barH - 8.0f) + 4.0f;
                    Gdiplus::SolidBrush sb(ToGdip(pal.labelTertiary, 120));
                    Gdiplus::GraphicsPath sp;
                    AddRoundPath(sp, static_cast<float>(s->winW - 5), barY, 3.0f, barH, 1.5f);
                    g.FillPath(&sb, &sp);
                }

                BitBlt(hdc, 0, 0, s->winW, s->winH, mem, 0, 0, SRCCOPY);
                SelectObject(mem, old);
                DeleteObject(bmp);
                DeleteDC(mem);
                EndPaint(hwnd, &ps);
                return 0;
            }
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUTranslucentMenuWnd";
        RegisterClassExW(&wc);
        reg = true;
    }

    st.hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"CuadernUTranslucentMenuWnd", L"",
        WS_POPUP | WS_VISIBLE,
        wx, wy, st.winW, st.winH,
        owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (st.hwnd) {
        SetForegroundWindow(st.hwnd);
        SetFocus(st.hwnd);
    }

    MSG msg{};
    while (!st.done && st.hwnd && IsWindow(st.hwnd)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (st.fNorm) DeleteObject(st.fNorm);
    if (st.fBold) DeleteObject(st.fBold);
    if (st.fSmall) DeleteObject(st.fSmall);
    return st.chosenCmd;
}

// Drop-in material translúcido 2.0 replacement for Win32 TrackPopupMenu
inline UINT TrackTranslucentPopupMenu(
    HMENU hMenu,
    POINT screenPt,
    HWND owner,
    const GlassSettings& settings,
    bool alignRight = false)
{
    const auto items = ExtractGlassMenuItemsFromHmenu(hMenu);
    return ShowTranslucentMenuItems(owner, items, screenPt, settings, alignRight);
}

} // namespace UiGlass

#endif // _WIN32
