#pragma once

// ============================================================================
// studio_closure_dialogs.hpp
// Full guías de interfaz + material translúcido Win32 dialogs wiring every previously
// engine-only / test-only capability in CuadernU.exe (Phases A-F).
// Zero decorative emojis. Neutral language only. PerMonitorV2 DPI scaled.
// ============================================================================

#include "studio_feature_dialogs.hpp"
#include "../core/accessibility_engine.hpp"
#include "../core/image_processing.hpp"
#include "../core/study_assistant_tools.hpp"
#include "../media/native_video_engine.hpp"
#include "../pdf/table_interop.hpp"

#include <ctime>
#include <functional>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <winhttp.h>
#include <wincodec.h>

namespace DocStudio::ClosureDialogs {

using FeatureDialogs::DpiOf;
using FeatureDialogs::Sc;
using FeatureDialogs::MakeUiFont;
using FeatureDialogs::GetControlTextW;
using FeatureDialogs::PickFolderWin32;
using FeatureDialogs::PickOpenFileWin32;
using FeatureDialogs::PickSaveFileWin32;

// Simple single-line / two-line inline prompt helper inside ClosureDialogs
inline bool QuickInputDialog(
    HWND owner,
    const std::wstring& title,
    const std::wstring& label1,
    std::wstring& val1,
    const std::wstring& label2 = L"",
    std::wstring* val2 = nullptr,
    bool mask1 = false)
{
    struct St {
        std::wstring l1, l2, v1, v2;
        bool has2 = false, mask1 = false, accepted = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hE1 = nullptr, hE2 = nullptr;
    } st{};
    st.l1 = label1; st.l2 = label2; st.v1 = val1;
    st.has2 = (!label2.empty() && val2 != nullptr);
    if (st.has2) st.v2 = *val2;
    st.mask1 = mask1;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);
                int y = Sc(12, s->dpi);
                HWND h1 = CreateWindowExW(0, L"STATIC", s->l1.c_str(), WS_CHILD | WS_VISIBLE,
                    Sc(16, s->dpi), y, Sc(404, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(h1, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                y += Sc(22, s->dpi);
                s->hE1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", s->v1.c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | (s->mask1 ? ES_PASSWORD : 0),
                    Sc(16, s->dpi), y, Sc(404, s->dpi), Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(101), nullptr, nullptr);
                SendMessageW(s->hE1, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);
                if (s->has2) {
                    HWND h2 = CreateWindowExW(0, L"STATIC", s->l2.c_str(), WS_CHILD | WS_VISIBLE,
                        Sc(16, s->dpi), y, Sc(404, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(h2, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                    y += Sc(22, s->dpi);
                    s->hE2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", s->v2.c_str(),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                        Sc(16, s->dpi), y, Sc(404, s->dpi), Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(102), nullptr, nullptr);
                    SendMessageW(s->hE2, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                    y += Sc(36, s->dpi);
                }
                HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(224, s->dpi), y, Sc(92, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                HWND hOk = CreateWindowExW(0, L"BUTTON", L"Aceptar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(324, s->dpi), y, Sc(96, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(hCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SetFocus(s->hE1);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == IDOK) {
                    s->v1 = GetControlTextW(s->hE1);
                    if (s->has2 && s->hE2) s->v2 = GetControlTextW(s->hE2);
                    s->accepted = true; s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUQuickInputDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 140, 140, 740, 520 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(452, st.dpi);
    const int h = Sc(st.has2 ? 226 : 168, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUQuickInputDlg", title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (st.accepted) {
        val1 = st.v1;
        if (st.has2 && val2) *val2 = st.v2;
    }
    return st.accepted;
}

// Helper: Encode BGRA32 buffer to PNG bytes via WIC
inline bool EncodeBgra32ToPngBytes(int w, int h, const std::uint8_t* bgra, std::vector<std::uint8_t>& outPng) {
    outPng.clear();
    if (w <= 0 || h <= 0 || !bgra) return false;
    Microsoft::WRL::ComPtr<IWICImagingFactory> fac;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac))) || !fac) return false;
    Microsoft::WRL::ComPtr<IStream> memStream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream)) || !memStream) return false;
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> enc;
    if (FAILED(fac->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) || !enc) return false;
    if (FAILED(enc->Initialize(memStream.Get(), WICBitmapEncoderNoCache))) return false;
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    if (FAILED(enc->CreateNewFrame(&frame, nullptr)) || !frame) return false;
    if (FAILED(frame->Initialize(nullptr))) return false;
    if (FAILED(frame->SetSize(static_cast<UINT>(w), static_cast<UINT>(h)))) return false;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(frame->SetPixelFormat(&fmt))) return false;
    if (FAILED(frame->WritePixels(static_cast<UINT>(h), static_cast<UINT>(w * 4), static_cast<UINT>(w * h * 4), const_cast<BYTE*>(bgra)))) return false;
    if (FAILED(frame->Commit()) || FAILED(enc->Commit())) return false;
    HGLOBAL hg = nullptr;
    if (FAILED(GetHGlobalFromStream(memStream.Get(), &hg)) || !hg) return false;
    const SIZE_T sz = GlobalSize(hg);
    const void* ptr = GlobalLock(hg);
    if (!ptr || sz == 0) { if (ptr) GlobalUnlock(hg); return false; }
    outPng.assign(static_cast<const std::uint8_t*>(ptr), static_cast<const std::uint8_t*>(ptr) + sz);
    GlobalUnlock(hg);
    return !outPng.empty();
}

// Helper: Global multi-monitor desktop screen capture (BitBlt -> PNG) (ON-EDIT-041)
inline bool CaptureDesktopScreenPngBytes(std::vector<std::uint8_t>& outPng, int& outW, int& outH) {
    return ImageProc::CaptureVirtualScreenPngBytes(outPng, outW, outH);
}

// Helper: Perspective scan rectification + whiteboard lighting normalization -> PNG (NT-EDIT-037, AP-EDIT-037)
inline bool RectifyScanFileToPngBytes(
    const std::wstring& inImagePath,
    std::vector<std::uint8_t>& outPng,
    int& outW,
    int& outH,
    std::string* outErr = nullptr)
{
    outPng.clear(); outW = 0; outH = 0;
    std::ifstream ifs(inImagePath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) { if (outErr) *outErr = "No se pudo abrir la imagen."; return false; }
    const auto sz = ifs.tellg();
    if (sz <= 0) return false;
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(raw.data()), sz);

    ImageProc::RgbaImage img{};
    if (!ImageProc::DecodeImageBytesWic(raw.data(), raw.size(), img)) {
        if (outErr) *outErr = "No se pudo decodificar la imagen con WIC.";
        return false;
    }
    const float insetX = static_cast<float>(img.width) * 0.04f;
    const float insetY = static_cast<float>(img.height) * 0.04f;
    const std::array<tinta::Punto, 4> quad{
        tinta::Punto{ insetX, insetY },
        tinta::Punto{ static_cast<float>(img.width - 1) - insetX, insetY },
        tinta::Punto{ static_cast<float>(img.width - 1) - insetX, static_cast<float>(img.height - 1) - insetY },
        tinta::Punto{ insetX, static_cast<float>(img.height - 1) - insetY }
    };
    const int targetW = (std::min)(1400, (std::max)(320, img.width));
    const int targetH = (std::min)(1800, (std::max)(320, img.height));
    ImageProc::RgbaImage rect = ImageProc::RectifyPerspectiveQuad(img, quad, targetW, targetH);
    ImageProc::EnhanceWhiteboardLighting(rect, 0.12f, 0.88f);
    if (!rect.Valid()) return false;
    std::vector<std::uint8_t> bgra(rect.rgba.size());
    for (std::size_t i = 0; i + 3 < rect.rgba.size(); i += 4) {
        bgra[i + 0] = rect.rgba[i + 2];
        bgra[i + 1] = rect.rgba[i + 1];
        bgra[i + 2] = rect.rgba[i + 0];
        bgra[i + 3] = rect.rgba[i + 3];
    }
    outW = rect.width;
    outH = rect.height;
    return EncodeBgra32ToPngBytes(rect.width, rect.height, bgra.data(), outPng);
}

// ----------------------------------------------------------------------------
// 1. Full Library Manager Dialog (Folders, SmartFolders, Tags, Rename, Merge, Pin)
// ----------------------------------------------------------------------------
inline bool ShowFullLibraryManagerDialog(
    HWND owner,
    LibraryContentViewsManager& lib,
    std::wstring& outOpenPath,
    bool& outMutated)
{
    outOpenPath.clear();
    outMutated = false;

    struct St {
        LibraryContentViewsManager* lib = nullptr;
        std::vector<LibraryItemRecord> shown;
        int viewMode = 0; // 0=Todos,1=Fotos,2=Audio,3=Marcadores,4=Adjuntos,5=Fav/Fijados,6=Recientes,7=Papelera,8=PDF,9=SmartFolder
        std::string activeTagFilter;
        std::wstring openPath;
        bool wantOpen = false, mutated = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hMode = nullptr, hList = nullptr, hInfo = nullptr;
    } st{};
    st.lib = &lib;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    auto refresh = [](St* s) {
        SendMessageW(s->hList, LB_RESETCONTENT, 0, 0);
        if (!s->activeTagFilter.empty()) {
            s->shown = s->lib->QueryByTag(s->activeTagFilter);
        } else {
            switch (s->viewMode) {
            case 1: s->shown = s->lib->QueryPhotosContentView(); break;
            case 2: s->shown = s->lib->QueryRecordingsContentView(); break;
            case 3: s->shown = s->lib->QueryBookmarksContentView(); break;
            case 4: s->shown = s->lib->QueryAttachmentsContentView(); break;
            case 5: s->shown = s->lib->QueryFavoritesAndPinned(); break;
            case 6: s->shown = s->lib->QueryRecentItems(50); break;
            case 7: s->shown = s->lib->QueryTrashItems(); break;
            case 8: s->shown = s->lib->QueryByKind("pdf"); break;
            case 9: {
                SmartFolderRule rule{};
                rule.nameUtf8 = "Carpeta Inteligente";
                if (!s->lib->SmartFolders().empty()) rule = s->lib->SmartFolders().back();
                else rule.onlyFavorites = true;
                s->shown = s->lib->EvaluateSmartFolder(rule);
                break;
            }
            default: {
                s->shown.clear();
                for (const auto& it : s->lib->Items()) if (!it.inTrash) s->shown.push_back(it);
                break;
            }
            }
        }
        for (const auto& it : s->shown) {
            std::string fName = "Raiz";
            for (const auto& fl : s->lib->Folders()) if (fl.folderId == it.folderId) { fName = fl.nameUtf8; break; }
            std::string line = (it.pinned ? "[Fijado] " : "") + std::string(it.favorite ? "[Fav] " : "") +
                it.titleUtf8 + "  [" + it.kindUtf8 + " | " + fName + "]";
            if (!it.tagsUtf8.empty()) line += " " + it.tagsUtf8.front();
            SendMessageW(s->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(line).c_str()));
        }
        if (!s->shown.empty()) SendMessageW(s->hList, LB_SETCURSEL, 0, 0);
        const std::string inf = "Carpetas: " + std::to_string(s->lib->Folders().size()) +
            "  |  Elementos visibles: " + std::to_string(s->shown.size()) +
            (s->activeTagFilter.empty() ? "" : ("  |  Etiqueta: " + s->activeTagFilter));
        SetWindowTextW(s->hInfo, Utf8ToWide(inf).c_str());
    };

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            auto doRefresh = [&](St* stPtr) {
                SendMessageW(stPtr->hList, LB_RESETCONTENT, 0, 0);
                if (!stPtr->activeTagFilter.empty()) {
                    stPtr->shown = stPtr->lib->QueryByTag(stPtr->activeTagFilter);
                } else {
                    switch (stPtr->viewMode) {
                    case 1: stPtr->shown = stPtr->lib->QueryPhotosContentView(); break;
                    case 2: stPtr->shown = stPtr->lib->QueryRecordingsContentView(); break;
                    case 3: stPtr->shown = stPtr->lib->QueryBookmarksContentView(); break;
                    case 4: stPtr->shown = stPtr->lib->QueryAttachmentsContentView(); break;
                    case 5: stPtr->shown = stPtr->lib->QueryFavoritesAndPinned(); break;
                    case 6: stPtr->shown = stPtr->lib->QueryRecentItems(50); break;
                    case 7: stPtr->shown = stPtr->lib->QueryTrashItems(); break;
                    case 8: stPtr->shown = stPtr->lib->QueryByKind("pdf"); break;
                    case 9: {
                        SmartFolderRule rule{};
                        rule.nameUtf8 = "Inteligente";
                        if (!stPtr->lib->SmartFolders().empty()) rule = stPtr->lib->SmartFolders().back();
                        stPtr->shown = stPtr->lib->EvaluateSmartFolder(rule);
                        break;
                    }
                    default: {
                        stPtr->shown.clear();
                        for (const auto& it : stPtr->lib->Items()) if (!it.inTrash) stPtr->shown.push_back(it);
                        break;
                    }
                    }
                }
                for (const auto& it : stPtr->shown) {
                    std::string fName = "Raiz";
                    for (const auto& fl : stPtr->lib->Folders()) if (fl.folderId == it.folderId) { fName = fl.nameUtf8; break; }
                    std::string line = (it.pinned ? "[Fijado] " : "") + std::string(it.favorite ? "[Fav] " : "") +
                        it.titleUtf8 + "  [" + it.kindUtf8 + " | " + fName + "]";
                    if (!it.tagsUtf8.empty()) line += " " + it.tagsUtf8.front();
                    SendMessageW(stPtr->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(line).c_str()));
                }
                if (!stPtr->shown.empty()) SendMessageW(stPtr->hList, LB_SETCURSEL, 0, 0);
                const std::string inf = "Carpetas: " + std::to_string(stPtr->lib->Folders().size()) +
                    "  |  Elementos: " + std::to_string(stPtr->shown.size()) +
                    (stPtr->activeTagFilter.empty() ? "" : ("  |  Etiqueta: " + stPtr->activeTagFilter));
                SetWindowTextW(stPtr->hInfo, Utf8ToWide(inf).c_str());
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                s->hMode = CreateWindowExW(0, L"COMBOBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(250, s->dpi), Sc(240, s->dpi), hwnd, reinterpret_cast<HMENU>(701), nullptr, nullptr);
                SendMessageW(s->hMode, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                const wchar_t* kModes[10] = {
                    L"Todos los cuadernos", L"Vista de Fotos", L"Vista de Grabaciones", L"Vista de Marcadores",
                    L"Vista de Adjuntos", L"Favoritos y Fijados", L"Recientes", L"Papelera",
                    L"Solo PDF", L"Carpeta Inteligente"
                };
                for (int i = 0; i < 10; ++i) SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kModes[i]));
                SendMessageW(s->hMode, CB_SETCURSEL, 0, 0);

                s->hInfo = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                    Sc(278, s->dpi), Sc(16, s->dpi), Sc(410, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hInfo, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(46, s->dpi), Sc(670, s->dpi), Sc(250, s->dpi), hwnd, reinterpret_cast<HMENU>(702), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                struct BtnDef { int id; const wchar_t* txt; };
                const BtnDef row1[6] = {
                    { 710, L"Nueva Carpeta" }, { 711, L"Mover Carpeta" }, { 712, L"Renombrar" },
                    { 713, L"Fijar / Fav" },   { 714, L"Fusionar" },      { 715, L"Papelera/Rest." }
                };
                const BtnDef row2[6] = {
                    { 716, L"Etiquetar" },     { 717, L"Renom. Etiqueta" }, { 718, L"Filtrar Etiqueta" },
                    { 719, L"Smart Folder" },  { IDOK, L"Abrir" },          { IDCANCEL, L"Cerrar" }
                };
                for (int i = 0; i < 6; ++i) {
                    HWND b1 = CreateWindowExW(0, L"BUTTON", row1[i].txt, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        Sc(16 + i * 112, s->dpi), Sc(306, s->dpi), Sc(106, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(row1[i].id)), nullptr, nullptr);
                    SendMessageW(b1, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                    HWND b2 = CreateWindowExW(0, L"BUTTON", row2[i].txt,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | (row2[i].id == IDOK ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(16 + i * 112, s->dpi), Sc(344, s->dpi), Sc(106, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(row2[i].id)), nullptr, nullptr);
                    SendMessageW(b2, WM_SETFONT, reinterpret_cast<WPARAM>(row2[i].id == IDOK ? s->fBold : s->fNorm), TRUE);
                }
                doRefresh(s);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelSecondary.ToColorRef());
                SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_CTLCOLORLISTBOX && s->brCard) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.contentCard.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brCard);
            }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                if (cid == 701 && HIWORD(wp) == CBN_SELCHANGE) {
                    s->viewMode = static_cast<int>(SendMessageW(s->hMode, CB_GETCURSEL, 0, 0));
                    s->activeTagFilter.clear();
                    doRefresh(s);
                    return 0;
                }
                const LRESULT sel = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                const bool hasSel = (sel >= 0 && static_cast<std::size_t>(sel) < s->shown.size());
                if (cid == 710) {
                    std::wstring fn = L"Materia Nueva";
                    if (QuickInputDialog(hwnd, L"Crear Carpeta en Biblioteca", L"Nombre de la carpeta:", fn) && !fn.empty()) {
                        s->lib->CreateFolder(WideToUtf8(fn), 0, 0);
                        s->mutated = true; doRefresh(s);
                    }
                    return 0;
                }
                if (cid == 711 && hasSel) {
                    if (s->lib->Folders().empty()) s->lib->CreateFolder("General", 0, 0);
                    std::wstring fidW = std::to_wstring(s->lib->Folders().back().folderId);
                    if (QuickInputDialog(hwnd, L"Mover Elemento a Carpeta", L"ID de carpeta destino (0 = Raiz):", fidW)) {
                        const std::uint64_t fid = static_cast<std::uint64_t>(_wtoi64(fidW.c_str()));
                        s->lib->MoveItemToFolder(s->shown[static_cast<std::size_t>(sel)].itemId, fid);
                        s->mutated = true; doRefresh(s);
                    }
                    return 0;
                }
                if (cid == 712 && hasSel) {
                    std::wstring nt = Utf8ToWide(s->shown[static_cast<std::size_t>(sel)].titleUtf8);
                    if (QuickInputDialog(hwnd, L"Renombrar Elemento", L"Nuevo t\u00EDtulo:", nt) && !nt.empty()) {
                        s->lib->RenameItem(s->shown[static_cast<std::size_t>(sel)].itemId, WideToUtf8(nt));
                        s->mutated = true; doRefresh(s);
                    }
                    return 0;
                }
                if (cid == 713 && hasSel) {
                    const auto& it = s->shown[static_cast<std::size_t>(sel)];
                    s->lib->SetPinned(it.itemId, !it.pinned);
                    s->lib->SetFavorite(it.itemId, !it.favorite);
                    s->mutated = true; doRefresh(s);
                    return 0;
                }
                if (cid == 714 && s->shown.size() >= 2) {
                    const std::size_t idx1 = hasSel ? static_cast<std::size_t>(sel) : 0u;
                    const std::size_t idx2 = (idx1 + 1u) % s->shown.size();
                    std::wstring mt = Utf8ToWide(s->shown[idx1].titleUtf8 + " + " + s->shown[idx2].titleUtf8);
                    if (QuickInputDialog(hwnd, L"Fusionar Elementos de Biblioteca", L"T\u00EDtulo combinado:", mt) && !mt.empty()) {
                        s->lib->MergeItems(s->shown[idx1].itemId, s->shown[idx2].itemId, WideToUtf8(mt));
                        s->mutated = true; doRefresh(s);
                    }
                    return 0;
                }
                if (cid == 715 && hasSel) {
                    const auto& it = s->shown[static_cast<std::size_t>(sel)];
                    if (it.inTrash) s->lib->RestoreFromTrash(it.itemId);
                    else s->lib->MoveToTrash(it.itemId);
                    s->mutated = true; doRefresh(s);
                    return 0;
                }
                if (cid == 716 && hasSel) {
                    std::wstring tagW = L"#estudio/tema";
                    if (QuickInputDialog(hwnd, L"Asignar Etiqueta", L"Etiqueta (soporta #padre/hijo):", tagW) && !tagW.empty()) {
                        s->lib->AssignTag(s->shown[static_cast<std::size_t>(sel)].itemId, WideToUtf8(tagW));
                        s->mutated = true; doRefresh(s);
                    }
                    return 0;
                }
                if (cid == 717) {
                    std::wstring oldT = L"#estudio", newT = L"#proyecto";
                    if (QuickInputDialog(hwnd, L"Renombrar Etiqueta en toda la Biblioteca", L"Prefijo actual:", oldT, L"Nuevo prefijo:", &newT) && !oldT.empty() && !newT.empty()) {
                        const std::size_t n = s->lib->RenameTagAcrossLibrary(WideToUtf8(oldT), WideToUtf8(newT));
                        s->mutated = (n > 0); doRefresh(s);
                    }
                    return 0;
                }
                if (cid == 718) {
                    std::wstring fTag = Utf8ToWide(s->activeTagFilter);
                    if (QuickInputDialog(hwnd, L"Filtrar por Etiqueta", L"Prefijo de etiqueta (vac\u00EDo = limpiar filtro):", fTag)) {
                        s->activeTagFilter = WideToUtf8(fTag);
                        doRefresh(s);
                    }
                    return 0;
                }
                if (cid == 719) {
                    std::wstring tagP = L"#estudio", txtQ = L"";
                    if (QuickInputDialog(hwnd, L"Crear y Evaluar Carpeta Inteligente", L"Etiqueta requerida (opcional):", tagP, L"Texto contenido (opcional):", &txtQ)) {
                        SmartFolderRule rule{};
                        rule.nameUtf8 = "Filtro " + WideToUtf8(tagP.empty() ? txtQ : tagP);
                        rule.requiredTagPrefixUtf8 = WideToUtf8(tagP);
                        rule.textQueryUtf8 = WideToUtf8(txtQ);
                        s->lib->AddSmartFolder(rule);
                        s->activeTagFilter.clear();
                        s->viewMode = 9;
                        SendMessageW(s->hMode, CB_SETCURSEL, 9, 0);
                        s->mutated = true; doRefresh(s);
                    }
                    return 0;
                }
                if ((cid == IDOK || (cid == 702 && HIWORD(wp) == LBN_DBLCLK)) && hasSel) {
                    s->openPath = Utf8ToWide(s->shown[static_cast<std::size_t>(sel)].filePathUtf8);
                    s->wantOpen = !s->openPath.empty();
                    s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUFullLibraryDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    (void)refresh;
    RECT prc{ 100, 100, 900, 620 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(716, st.dpi), h = Sc(428, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUFullLibraryDlg", L"Biblioteca, Carpetas, Etiquetas y Vistas de Contenido",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outOpenPath = st.openPath;
    outMutated = st.mutated;
    return st.wantOpen;
}

// ----------------------------------------------------------------------------
// 2. Backup & Restore Manager (Full Package + Single-Note Recovery)
// ----------------------------------------------------------------------------
inline void ShowBackupRestoreDialog(HWND owner, const std::wstring& profileRootDir, std::string& outStatus) {
    struct St {
        std::wstring rootDir;
        std::string status;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hPass = nullptr, hNoteRel = nullptr;
    } st{};
    st.rootDir = profileRootDir;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);
                HWND hL1 = CreateWindowExW(0, L"STATIC", L"Contrase\u00F1a AES-256-GCM opcional (vac\u00EDo = ZIP sin cifrar):",
                    WS_CHILD | WS_VISIBLE, Sc(16, s->dpi), Sc(14, s->dpi), Sc(440, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hL1, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                s->hPass = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(36, s->dpi), Sc(440, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hPass, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                HWND hL2 = CreateWindowExW(0, L"STATIC", L"Ruta relativa para recuperar nota individual (ej. vault/Notas.md):",
                    WS_CHILD | WS_VISIBLE, Sc(16, s->dpi), Sc(72, s->dpi), Sc(440, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hL2, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                s->hNoteRel = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"library_index.json", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(94, s->dpi), Sc(440, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hNoteRel, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                HWND b1 = CreateWindowExW(0, L"BUTTON", L"Crear Respaldo...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(16, s->dpi), Sc(136, s->dpi), Sc(140, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(801), nullptr, nullptr);
                HWND b2 = CreateWindowExW(0, L"BUTTON", L"Restaurar Todo...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(166, s->dpi), Sc(136, s->dpi), Sc(140, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(802), nullptr, nullptr);
                HWND b3 = CreateWindowExW(0, L"BUTTON", L"Restaurar 1 Nota...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(316, s->dpi), Sc(136, s->dpi), Sc(140, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(803), nullptr, nullptr);
                SendMessageW(b1, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(b2, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(b3, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                const std::string pass = WideToUtf8(GetControlTextW(s->hPass));
                std::string err;
                if (LOWORD(wp) == 801) {
                    std::wstring outPkg;
                    if (PickSaveFileWin32(hwnd, L"Respaldo CuadernU (*.cuadernu-backup)\0*.cuadernu-backup\0\0",
                            L"cuadernu-backup", L"respaldo_cuadernu.cuadernu-backup", L"Guardar Paquete de Respaldo", outPkg)) {
                        if (LocalFirstSyncEngine::CreateBackupPackage(s->rootDir, outPkg, pass, &err)) {
                            s->status = "Respaldo verificado creado exitosamente";
                            MessageBoxW(hwnd, L"Paquete de respaldo creado correctamente.", L"Respaldo", MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hwnd, Utf8ToWide(err).c_str(), L"Error creando respaldo", MB_ICONERROR);
                        }
                    }
                    return 0;
                }
                if (LOWORD(wp) == 802) {
                    std::wstring inPkg, dstDir;
                    if (PickOpenFileWin32(hwnd, L"Respaldo CuadernU (*.cuadernu-backup)\0*.cuadernu-backup\0\0", L"Seleccionar Respaldo", inPkg) &&
                        PickFolderWin32(hwnd, L"Carpeta destino de restauraci\u00F3n", dstDir)) {
                        if (LocalFirstSyncEngine::RestoreFullBackupPackage(inPkg, dstDir, pass, &err)) {
                            s->status = "Respaldo completo restaurado correctamente";
                            MessageBoxW(hwnd, L"Respaldo completo restaurado sin p\u00E9rdida.", L"Restauraci\u00F3n", MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hwnd, Utf8ToWide(err).c_str(), L"Error restaurando", MB_ICONERROR);
                        }
                    }
                    return 0;
                }
                if (LOWORD(wp) == 803) {
                    std::wstring inPkg, outSingle;
                    const std::string rel = WideToUtf8(GetControlTextW(s->hNoteRel));
                    if (!rel.empty() &&
                        PickOpenFileWin32(hwnd, L"Respaldo CuadernU (*.cuadernu-backup)\0*.cuadernu-backup\0\0", L"Seleccionar Respaldo", inPkg) &&
                        PickSaveFileWin32(hwnd, L"Todos (*.*)\0*.*\0\0", L"md", L"nota_recuperada.md", L"Guardar Nota Individual", outSingle)) {
                        if (LocalFirstSyncEngine::RestoreSingleNoteFromBackup(inPkg, rel, outSingle, pass, &err)) {
                            s->status = "Nota individual recuperada desde respaldo: " + rel;
                            MessageBoxW(hwnd, L"Nota individual recuperada correctamente.", L"Recuperaci\u00F3n Quir\u00FArgica", MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hwnd, Utf8ToWide(err).c_str(), L"Error recuperando nota", MB_ICONERROR);
                        }
                    }
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUBackupRestoreDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 140, 140, 740, 520 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(488, st.dpi), h = Sc(224, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUBackupRestoreDlg", L"Respaldo y Restauraci\u00F3n (Completo / Individual)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (!st.status.empty()) outStatus = st.status;
}

// ----------------------------------------------------------------------------
// 3. Share Links & Permissions Manager (CreateShareLink/Revoke/CanUserEdit/Comment)
// ----------------------------------------------------------------------------
inline bool ShowShareLinksAndPermissionsDialog(
    HWND owner,
    CollaborationPermissionManager& perms,
    const std::string& docTitle,
    std::size_t activePageIndex,
    std::string& outStatus)
{
    struct St {
        CollaborationPermissionManager* perms = nullptr;
        std::string docTitle;
        std::size_t pageIdx = 0;
        bool mutated = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hList = nullptr, hRole = nullptr, hInfo = nullptr;
    } st{};
    st.perms = &perms; st.docTitle = docTitle; st.pageIdx = activePageIndex;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            auto refresh = [](St* sp) {
                SendMessageW(sp->hList, LB_RESETCONTENT, 0, 0);
                static const char* kR[4] = { "Lector", "Comentarista", "Editor", "Propietario" };
                for (const auto& tk : sp->perms->ShareLinks()) {
                    const std::string ln = tk.tokenIdUtf8 + " | " + (tk.isPublic ? "Publico" : "Privado") +
                        " | " + kR[static_cast<int>(tk.grantedRole)] + (tk.active ? " [Activo]" : " [Revocado]");
                    SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                }
                const bool canEdit = sp->perms->CanUserEditDocument("colaborador");
                const bool canCom = sp->perms->CanUserCommentDocument("colaborador");
                const bool privPg = sp->perms->IsPagePrivate(sp->pageIdx);
                const std::string inf = std::string("Colaborador -> Editar: ") + (canEdit ? "Si" : "No") +
                    " | Comentar: " + (canCom ? "Si" : "No") +
                    " | Pagina privada: " + (privPg ? "Si" : "No");
                SetWindowTextW(sp->hInfo, Utf8ToWide(inf).c_str());
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                s->hInfo = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(480, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hInfo, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(38, s->dpi), Sc(480, s->dpi), Sc(168, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                s->hRole = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(16, s->dpi), Sc(214, s->dpi), Sc(140, s->dpi), Sc(140, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hRole, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hRole, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Lector"));
                SendMessageW(s->hRole, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Comentarista"));
                SendMessageW(s->hRole, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Editor"));
                SendMessageW(s->hRole, CB_SETCURSEL, 1, 0);

                HWND bAdd = CreateWindowExW(0, L"BUTTON", L"Crear Enlace", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(164, s->dpi), Sc(212, s->dpi), Sc(104, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(821), nullptr, nullptr);
                HWND bRev = CreateWindowExW(0, L"BUTTON", L"Revocar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(274, s->dpi), Sc(212, s->dpi), Sc(80, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(822), nullptr, nullptr);
                HWND bPriv = CreateWindowExW(0, L"BUTTON", L"P\u00E1g. Privada", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(360, s->dpi), Sc(212, s->dpi), Sc(136, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(823), nullptr, nullptr);
                SendMessageW(bAdd, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(bRev, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bPriv, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                refresh(s);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == 821) {
                    const int rIdx = static_cast<int>(SendMessageW(s->hRole, CB_GETCURSEL, 0, 0));
                    const auto role = static_cast<CollaborationRole>(std::clamp(rIdx, 0, 2));
                    s->perms->SetUserRole("colaborador", role);
                    s->perms->CreateShareLink(s->docTitle.empty() ? "Cuaderno" : s->docTitle, false, role);
                    s->mutated = true; refresh(s); return 0;
                }
                if (LOWORD(wp) == 822) {
                    const LRESULT sel = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && static_cast<std::size_t>(sel) < s->perms->ShareLinks().size()) {
                        s->perms->RevokeShareLink(s->perms->ShareLinks()[static_cast<std::size_t>(sel)].tokenIdUtf8);
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (LOWORD(wp) == 823) {
                    s->perms->SetPagePrivate(s->pageIdx, !s->perms->IsPagePrivate(s->pageIdx));
                    s->mutated = true; refresh(s); return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUSharePermsDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 140, 140, 740, 520 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(528, st.dpi), h = Sc(296, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUSharePermsDlg", L"Enlaces Compartidos y Permisos de Nota",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (st.mutated) outStatus = "Permisos y enlaces actualizados (" + std::to_string(perms.ShareLinks().size()) + " enlaces)";
    return st.mutated;
}

// ----------------------------------------------------------------------------
// 4. Interactive Knowledge Links Dialog (Backlinks, Outgoing, Unlinked -> WikiLink, Broken)
// ----------------------------------------------------------------------------
inline bool ShowKnowledgeLinksDialog(
    HWND owner,
    KnowledgeVault& vault,
    const std::string& activeTitle,
    std::string& outOpenNoteTitle,
    bool& outMutated)
{
    outOpenNoteTitle.clear();
    outMutated = false;

    struct St {
        KnowledgeVault* vault = nullptr;
        std::string title;
        int tab = 0; // 0=Backlinks, 1=Salientes, 2=Menciones sin enlazar, 3=Enlaces rotos
        std::vector<KnowledgeLink> links;
        std::vector<UnlinkedMention> mentions;
        std::string openTarget;
        bool mutated = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hMode = nullptr, hList = nullptr, hPrev = nullptr;
    } st{};
    st.vault = &vault; st.title = activeTitle;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            auto refresh = [](St* sp) {
                SendMessageW(sp->hList, LB_RESETCONTENT, 0, 0);
                sp->links.clear(); sp->mentions.clear();
                if (sp->tab == 0) {
                    sp->links = sp->vault->Backlinks(sp->title);
                    for (const auto& l : sp->links) {
                        const std::string ln = "[<- " + l.sourceNoteTitleUtf8 + "] " + l.contextSnippetUtf8;
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                } else if (sp->tab == 1) {
                    sp->links = sp->vault->OutgoingLinks(sp->title);
                    for (const auto& l : sp->links) {
                        const std::string ln = "[-> " + l.targetNoteTitleUtf8 + "] " + l.contextSnippetUtf8;
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                } else if (sp->tab == 2) {
                    sp->mentions = sp->vault->FindUnlinkedMentions(sp->title);
                    for (const auto& m : sp->mentions) {
                        const std::string ln = "[Mencion en " + m.sourceNoteTitleUtf8 + "] " + m.contextSnippetUtf8;
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                } else {
                    sp->links = sp->vault->FindBrokenWikiLinks();
                    for (const auto& l : sp->links) {
                        const std::string ln = "[Roto en " + l.sourceNoteTitleUtf8 + "] -> [[" + l.targetRawUtf8 + "]]";
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                }
                if (SendMessageW(sp->hList, LB_GETCOUNT, 0, 0) > 0) SendMessageW(sp->hList, LB_SETCURSEL, 0, 0);
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                s->hMode = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(280, s->dpi), Sc(180, s->dpi), hwnd, reinterpret_cast<HMENU>(831), nullptr, nullptr);
                SendMessageW(s->hMode, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Backlinks (Entrantes)"));
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Enlaces Salientes"));
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Menciones sin Enlazar"));
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Enlaces Rotos en B\u00F3veda"));
                SendMessageW(s->hMode, CB_SETCURSEL, 0, 0);

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(46, s->dpi), Sc(540, s->dpi), Sc(176, s->dpi), hwnd, reinterpret_cast<HMENU>(832), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hPrev = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Vista previa de enlace...",
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                    Sc(16, s->dpi), Sc(230, s->dpi), Sc(540, s->dpi), Sc(72, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hPrev, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                HWND bConv = CreateWindowExW(0, L"BUTTON", L"Convertir a [[Enlace]]", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(16, s->dpi), Sc(312, s->dpi), Sc(170, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(833), nullptr, nullptr);
                HWND bOpen = CreateWindowExW(0, L"BUTTON", L"Abrir Nota", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(350, s->dpi), Sc(312, s->dpi), Sc(100, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                HWND bClose = CreateWindowExW(0, L"BUTTON", L"Cerrar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(456, s->dpi), Sc(312, s->dpi), Sc(100, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                SendMessageW(bConv, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bOpen, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(bClose, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                refresh(s);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                if (cid == 831 && HIWORD(wp) == CBN_SELCHANGE) {
                    s->tab = static_cast<int>(SendMessageW(s->hMode, CB_GETCURSEL, 0, 0));
                    refresh(s); return 0;
                }
                const LRESULT sel = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                if (cid == 832 && HIWORD(wp) == LBN_SELCHANGE && sel >= 0) {
                    std::string target;
                    if (s->tab == 2 && static_cast<std::size_t>(sel) < s->mentions.size()) target = s->mentions[static_cast<std::size_t>(sel)].sourceNoteTitleUtf8;
                    else if (static_cast<std::size_t>(sel) < s->links.size()) {
                        target = (s->tab == 1) ? s->links[static_cast<std::size_t>(sel)].targetRawUtf8 : s->links[static_cast<std::size_t>(sel)].sourceNoteTitleUtf8;
                    }
                    const auto prev = s->vault->ResolveLinkPreview(target);
                    SetWindowTextW(s->hPrev, Utf8ToWide(prev.found ? (prev.targetTitleUtf8 + "\r\n" + prev.previewExcerptUtf8) : "Sin vista previa disponible.").c_str());
                    return 0;
                }
                if (cid == 833 && s->tab == 2 && sel >= 0 && static_cast<std::size_t>(sel) < s->mentions.size()) {
                    const auto& m = s->mentions[static_cast<std::size_t>(sel)];
                    if (s->vault->ConvertMentionToWikiLink(m.sourceNoteIdUtf8, m.byteOffset, m.byteLength, m.targetNoteTitleUtf8)) {
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == IDOK && sel >= 0) {
                    if (s->tab == 2 && static_cast<std::size_t>(sel) < s->mentions.size()) s->openTarget = s->mentions[static_cast<std::size_t>(sel)].sourceNoteTitleUtf8;
                    else if (static_cast<std::size_t>(sel) < s->links.size()) {
                        s->openTarget = (s->tab == 1) ? s->links[static_cast<std::size_t>(sel)].targetNoteTitleUtf8 : s->links[static_cast<std::size_t>(sel)].sourceNoteTitleUtf8;
                    }
                    s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUKnowledgeLinksDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 820, 560 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(588, st.dpi), h = Sc(396, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUKnowledgeLinksDlg", L"Panel de Enlaces, Backlinks y Menciones",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outOpenNoteTitle = st.openTarget;
    outMutated = st.mutated;
    return !outOpenNoteTitle.empty();
}

// ----------------------------------------------------------------------------
// 5. Vault Search, Property Filter, Move & Atomic Link-Propagating Rename
// ----------------------------------------------------------------------------
inline bool ShowVaultSearchRenameDialog(
    HWND owner,
    KnowledgeVault& vault,
    std::string& outPickedTitle,
    bool& outMutated,
    std::string& outStatus)
{
    outPickedTitle.clear();
    outMutated = false;

    struct St {
        KnowledgeVault* vault = nullptr;
        std::vector<std::string> titles;
        std::vector<std::string> ids;
        std::string picked;
        std::string status;
        bool mutated = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hQuery = nullptr, hPropK = nullptr, hPropV = nullptr, hList = nullptr;
    } st{};
    st.vault = &vault;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            auto runSearch = [](St* sp) {
                SendMessageW(sp->hList, LB_RESETCONTENT, 0, 0);
                sp->titles.clear(); sp->ids.clear();
                const std::string q = WideToUtf8(GetControlTextW(sp->hQuery));
                const std::string pk = WideToUtf8(GetControlTextW(sp->hPropK));
                const std::string pv = WideToUtf8(GetControlTextW(sp->hPropV));
                if (!pk.empty()) {
                    NoteFilterCriteria crit{};
                    crit.propKeyUtf8 = pk;
                    crit.propOpUtf8 = pv.empty() ? "contains" : "==";
                    crit.propValueUtf8 = pv;
                    for (const auto& n : sp->vault->FilterNotes(crit)) {
                        sp->titles.push_back(n.titleUtf8);
                        sp->ids.push_back(n.noteIdUtf8);
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(n.titleUtf8 + " [" + n.folderPathUtf8 + "]").c_str()));
                    }
                } else if (!q.empty()) {
                    for (const auto& h : sp->vault->SearchWorkspace(q, 50)) {
                        sp->titles.push_back(h.titleUtf8);
                        sp->ids.push_back(h.noteIdUtf8);
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(h.titleUtf8 + " — " + h.snippetUtf8).c_str()));
                    }
                } else {
                    for (const auto& n : sp->vault->ActiveNotes()) {
                        sp->titles.push_back(n.titleUtf8);
                        sp->ids.push_back(n.noteIdUtf8);
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(n.titleUtf8 + " [" + n.folderPathUtf8 + "]").c_str()));
                    }
                }
                if (!sp->titles.empty()) SendMessageW(sp->hList, LB_SETCURSEL, 0, 0);
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                s->hQuery = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(230, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                s->hPropK = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(252, s->dpi), Sc(12, s->dpi), Sc(110, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                s->hPropV = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(368, s->dpi), Sc(12, s->dpi), Sc(110, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                HWND bGo = CreateWindowExW(0, L"BUTTON", L"Buscar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(484, s->dpi), Sc(11, s->dpi), Sc(80, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(841), nullptr, nullptr);
                SendMessageW(s->hQuery, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hPropK, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hPropV, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bGo, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(s->hQuery, 0x1501, TRUE, reinterpret_cast<LPARAM>(L"Buscar en b\u00F3veda..."));
                SendMessageW(s->hPropK, 0x1501, TRUE, reinterpret_cast<LPARAM>(L"Propiedad"));
                SendMessageW(s->hPropV, 0x1501, TRUE, reinterpret_cast<LPARAM>(L"Valor"));

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(48, s->dpi), Sc(548, s->dpi), Sc(212, s->dpi), hwnd, reinterpret_cast<HMENU>(842), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                struct B { int id; const wchar_t* t; };
                const B btns[5] = {
                    { 843, L"Renom. Nota" }, { 844, L"Renom. T\u00EDtulo/Bloque" }, { 845, L"Mover/Prop." },
                    { IDOK, L"Abrir" }, { IDCANCEL, L"Cerrar" }
                };
                for (int i = 0; i < 5; ++i) {
                    HWND hb = CreateWindowExW(0, L"BUTTON", btns[i].t,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | (btns[i].id == IDOK ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(16 + i * 110, s->dpi), Sc(270, s->dpi), Sc(104, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(btns[i].id)), nullptr, nullptr);
                    SendMessageW(hb, WM_SETFONT, reinterpret_cast<WPARAM>(btns[i].id == IDOK ? s->fBold : s->fNorm), TRUE);
                }
                runSearch(s);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                if (cid == 841) { runSearch(s); return 0; }
                const LRESULT sel = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                const bool hasSel = (sel >= 0 && static_cast<std::size_t>(sel) < s->titles.size());
                if (cid == 843 && hasSel) {
                    const std::string oldT = s->titles[static_cast<std::size_t>(sel)];
                    std::wstring newT = Utf8ToWide(oldT);
                    if (QuickInputDialog(hwnd, L"Renombrar Nota (Propaga Wikilinks)", L"Nuevo t\u00EDtulo:", newT) && !newT.empty()) {
                        const auto rep = s->vault->RenameNote(oldT, WideToUtf8(newT));
                        s->mutated = true;
                        s->status = "Nota renombrada (" + std::to_string(rep.referencesUpdated) + " enlaces actualizados)";
                        runSearch(s);
                    }
                    return 0;
                }
                if (cid == 844 && hasSel) {
                    const std::string noteT = s->titles[static_cast<std::size_t>(sel)];
                    std::wstring oldRef = L"Resumen", newRef = L"Sintesis";
                    if (QuickInputDialog(hwnd, L"Renombrar Encabezado o ^Bloque", L"Encabezado o ^bloque actual:", oldRef, L"Nuevo nombre:", &newRef) && !oldRef.empty() && !newRef.empty()) {
                        const std::string o = WideToUtf8(oldRef), n = WideToUtf8(newRef);
                        const auto rep = (!o.empty() && o[0] == '^')
                            ? s->vault->RenameBlockIdInNote(noteT, o.substr(1), (!n.empty() && n[0] == '^') ? n.substr(1) : n)
                            : s->vault->RenameHeadingInNote(noteT, o, n);
                        s->mutated = true;
                        s->status = "Referencias propagadas: " + std::to_string(rep.referencesUpdated);
                        runSearch(s);
                    }
                    return 0;
                }
                if (cid == 845 && hasSel) {
                    const std::string nid = s->ids[static_cast<std::size_t>(sel)];
                    std::wstring dstF = L"Cuadernos", propKv = L"estado=Revisado";
                    if (QuickInputDialog(hwnd, L"Mover Nota y Asignar Propiedad", L"Carpeta destino:", dstF, L"Propiedad (clave=valor):", &propKv)) {
                        if (!dstF.empty()) s->vault->MoveNote(nid, WideToUtf8(dstF));
                        const std::string kv = WideToUtf8(propKv);
                        const auto eq = kv.find('=');
                        if (eq != std::string::npos && eq > 0) {
                            NoteProperty np; np.keyUtf8 = kv.substr(0, eq); np.valueUtf8 = kv.substr(eq + 1);
                            s->vault->SetNoteProperty(nid, np);
                        }
                        s->mutated = true; runSearch(s);
                    }
                    return 0;
                }
                if ((cid == IDOK || (cid == 842 && HIWORD(wp) == LBN_DBLCLK)) && hasSel) {
                    s->picked = s->titles[static_cast<std::size_t>(sel)];
                    s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUVaultSearchRenameDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 820, 560 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(596, st.dpi), h = Sc(356, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUVaultSearchRenameDlg", L"B\u00FAsqueda en B\u00F3veda, Filtros y Renombrado Propagado",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outPickedTitle = st.picked;
    outMutated = st.mutated;
    if (!st.status.empty()) outStatus = st.status;
    return !outPickedTitle.empty();
}

// ----------------------------------------------------------------------------
// 6. Full Structured Database CRUD + Formulas + Views + Templates + Vault Sync Dialog
// ----------------------------------------------------------------------------
inline bool ShowDatabaseEditorDialog(
    HWND owner,
    StructuredDatabase& db,
    KnowledgeVault& vault,
    AutomationRuleEngine& automations,
    std::string& outStatus)
{
    struct St {
        StructuredDatabase* db = nullptr;
        KnowledgeVault* vault = nullptr;
        AutomationRuleEngine* autos = nullptr;
        bool mutated = false, done = false;
        std::string status;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hList = nullptr, hInfo = nullptr;
    } st{};
    st.db = &db; st.vault = &vault; st.autos = &automations;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            auto refresh = [](St* sp) {
                sp->db->RecomputeFormulasAndRollups(sp->vault);
                SendMessageW(sp->hList, LB_RESETCONTENT, 0, 0);
                for (const auto& r : sp->db->Records()) {
                    std::string ln = r.titleUtf8 + "  [";
                    bool first = true;
                    for (const auto& c : sp->db->Columns()) {
                        auto it = r.cells.find(c.idUtf8);
                        if (it == r.cells.end()) continue;
                        if (!first) ln += " | ";
                        first = false;
                        ln += c.nameUtf8 + "=";
                        if (c.type == DbColumnType::Checkbox) ln += (it->second.boolVal ? "Si" : "No");
                        else if (!it->second.textUtf8.empty()) ln += it->second.textUtf8;
                        else ln += std::to_string(it->second.numberVal);
                    }
                    ln += "]";
                    SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                }
                if (!sp->db->Records().empty()) SendMessageW(sp->hList, LB_SETCURSEL, 0, 0);
                const std::string inf = "Columnas: " + std::to_string(sp->db->Columns().size()) +
                    " | Registros: " + std::to_string(sp->db->Records().size()) +
                    " | Vistas: " + std::to_string(sp->db->Views().size());
                SetWindowTextW(sp->hInfo, Utf8ToWide(inf).c_str());
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                s->hInfo = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(610, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hInfo, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(38, s->dpi), Sc(612, s->dpi), Sc(224, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                struct B { int id; const wchar_t* t; };
                const B r1[5] = { {851, L"+ Registro"}, {852, L"Editar Celda"}, {853, L"Eliminar"}, {854, L"+ Col/F\u00F3rmula"}, {855, L"Vista/Filtro"} };
                const B r2[5] = { {856, L"Plantilla"},  {857, L"Sync B\u00F3veda"}, {858, L"Bot\u00F3n Acci\u00F3n"}, {859, L"Recalcular"}, {IDCANCEL, L"Cerrar"} };
                for (int i = 0; i < 5; ++i) {
                    HWND b1 = CreateWindowExW(0, L"BUTTON", r1[i].t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        Sc(16 + i * 124, s->dpi), Sc(272, s->dpi), Sc(118, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(r1[i].id)), nullptr, nullptr);
                    HWND b2 = CreateWindowExW(0, L"BUTTON", r2[i].t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        Sc(16 + i * 124, s->dpi), Sc(310, s->dpi), Sc(118, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(r2[i].id)), nullptr, nullptr);
                    SendMessageW(b1, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                    SendMessageW(b2, WM_SETFONT, reinterpret_cast<WPARAM>(r2[i].id == 858 ? s->fBold : s->fNorm), TRUE);
                }
                refresh(s);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                const LRESULT sel = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                const bool hasSel = (sel >= 0 && static_cast<std::size_t>(sel) < s->db->Records().size());
                if (cid == 851) {
                    std::wstring tW = L"Nuevo registro", stW = L"Pendiente";
                    if (QuickInputDialog(hwnd, L"Nuevo Registro", L"T\u00EDtulo:", tW, L"Estado inicial:", &stW) && !tW.empty()) {
                        DbRecord r; r.titleUtf8 = WideToUtf8(tW);
                        const std::string rid = s->db->AddRecord(r);
                        s->db->SetCellText(rid, "estado", WideToUtf8(stW));
                        s->db->SetCellBool(rid, "hecho", false);
                        s->autos->TriggerEvent(AutomationTriggerKind::OnRecordCreated, *s->db, rid);
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == 852 && hasSel) {
                    const std::string rid = s->db->Records()[static_cast<std::size_t>(sel)].recordIdUtf8;
                    std::wstring colW = L"estado", valW = L"Completado";
                    if (QuickInputDialog(hwnd, L"Editar Celda", L"ID de columna:", colW, L"Nuevo valor (true/false/n\u00FAmero/texto):", &valW) && !colW.empty()) {
                        const std::string colId = WideToUtf8(colW), val = WideToUtf8(valW);
                        const auto* col = s->db->FindColumn(colId);
                        if (col && col->type == DbColumnType::Checkbox) {
                            s->db->SetCellBool(rid, colId, val == "true" || val == "1" || val == "Si");
                        } else if (col && col->type == DbColumnType::Number) {
                            s->db->SetCellNumber(rid, colId, std::atof(val.c_str()));
                        } else {
                            s->db->SetCellText(rid, colId, val);
                        }
                        s->autos->TriggerEvent(AutomationTriggerKind::OnPropertyChanged, *s->db, rid, colId, val);
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == 853 && hasSel) {
                    s->db->RemoveRecord(s->db->Records()[static_cast<std::size_t>(sel)].recordIdUtf8);
                    s->mutated = true; refresh(s); return 0;
                }
                if (cid == 854) {
                    std::wstring colW = L"doble_prio", exprW = L"prop(\"prioridad\") * 2";
                    if (QuickInputDialog(hwnd, L"Agregar Columna o F\u00F3rmula", L"ID de columna nueva:", colW, L"F\u00F3rmula (vac\u00EDo = columna texto):", &exprW) && !colW.empty()) {
                        DbColumnDef c{};
                        c.idUtf8 = WideToUtf8(colW);
                        c.nameUtf8 = c.idUtf8;
                        if (!exprW.empty()) { c.type = DbColumnType::Formula; c.formulaExprUtf8 = WideToUtf8(exprW); }
                        else c.type = DbColumnType::Text;
                        s->db->AddColumn(std::move(c));
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == 855) {
                    std::wstring fCol = L"estado", fVal = L"En curso";
                    if (QuickInputDialog(hwnd, L"Guardar Vista con Filtro y Orden", L"Columna a filtrar (vac\u00EDo = quitar filtro):", fCol, L"Valor requerido:", &fVal)) {
                        DbViewConfig vc{};
                        vc.viewIdUtf8 = "vista_usuario";
                        vc.nameUtf8 = "Vista Filtrada";
                        vc.kind = DbViewKind::Table;
                        if (!fCol.empty() && !fVal.empty()) {
                            vc.filters.push_back({ WideToUtf8(fCol), "==", WideToUtf8(fVal) });
                        }
                        vc.sorts.push_back({ "prioridad", true });
                        s->db->AddOrUpdateView(vc);
                        s->db->SetActiveView(vc.viewIdUtf8);
                        s->mutated = true;
                        s->status = "Vista con filtro/orden aplicada";
                        refresh(s);
                    }
                    return 0;
                }
                if (cid == 856) {
                    DbRecordTemplate tpl{};
                    tpl.templateIdUtf8 = "tpl_tarea";
                    tpl.nameUtf8 = "Tarea Estandar";
                    tpl.defaultTitlePrefixUtf8 = "Tarea";
                    tpl.defaultCells["estado"] = DbCellValue{ "En curso", 0.0, false, {} };
                    tpl.defaultCells["prioridad"] = DbCellValue{ "1", 1.0, false, {} };
                    s->db->AddRecordTemplate(tpl);
                    std::wstring sufW = L"Nueva";
                    if (QuickInputDialog(hwnd, L"Crear Registro desde Plantilla", L"Sufijo del t\u00EDtulo:", sufW)) {
                        s->db->CreateRecordFromTemplate("tpl_tarea", WideToUtf8(sufW));
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == 857) {
                    s->db->ImportRecordsFromVaultNotes(*s->vault);
                    s->db->SyncRecordsToVaultNotes(*s->vault);
                    s->mutated = true;
                    s->status = "Base sincronizada bidireccionalmente con Boveda Markdown";
                    refresh(s); return 0;
                }
                if (cid == 858 && hasSel) {
                    const std::string rid = s->db->Records()[static_cast<std::size_t>(sel)].recordIdUtf8;
                    const std::size_t fired = s->autos->TriggerEvent(AutomationTriggerKind::OnActionButtonClicked, *s->db, rid);
                    s->mutated = (fired > 0);
                    s->status = "Boton de accion ejecutado (" + std::to_string(fired) + " regla(s))";
                    refresh(s); return 0;
                }
                if (cid == 859) { refresh(s); s->status = "Formulas y rollups recalculados"; return 0; }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUDbEditorDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 100, 100, 860, 560 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(660, st.dpi), h = Sc(396, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUDbEditorDlg", L"Editor de Base de Datos Estructurada (.base)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (!st.status.empty()) outStatus = st.status;
    return st.mutated;
}

// ----------------------------------------------------------------------------
// 7. Connected Database Multi-Field Form Submission Dialog (NO-DATA-056 -> SubmitForm)
// ----------------------------------------------------------------------------
inline bool ShowDatabaseFormDialog(HWND owner, StructuredDatabase& db, std::string& outStatus) {
    DbConnectedForm form{};
    form.formIdUtf8 = "form_default";
    form.titleUtf8 = "Formulario: " + db.Title();
    for (const auto& c : db.Columns()) {
        if (c.type == DbColumnType::Formula || c.type == DbColumnType::Rollup) continue;
        // Mark primary column required so real SubmitForm validation is enforced in GUI
        const bool isReq = form.fields.empty();
        form.fields.push_back({ c.idUtf8, c.nameUtf8, isReq });
        if (form.fields.size() >= 6) break;
    }
    db.RegisterForm(form);

    struct St {
        StructuredDatabase* db = nullptr;
        DbConnectedForm form{};
        std::string* outStatus = nullptr;
        bool submitted = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hTitle = nullptr;
        std::vector<HWND> hFields;
    } st{};
    st.db = &db;
    st.form = form;
    st.outStatus = &outStatus;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                int y = 14;
                HWND lblT = CreateWindowExW(0, L"STATIC", L"T\u00EDtulo del registro *:",
                    WS_CHILD | WS_VISIBLE, Sc(16, s->dpi), Sc(y, s->dpi), Sc(400, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(lblT, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                y += 22;
                s->hTitle = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(y, s->dpi), Sc(410, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hTitle, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += 34;

                s->hFields.clear();
                for (const auto& f : s->form.fields) {
                    const std::string lblStr = f.labelUtf8 + (f.required ? " * (obligatorio):" : ":");
                    HWND lbl = CreateWindowExW(0, L"STATIC", Utf8ToWide(lblStr).c_str(),
                        WS_CHILD | WS_VISIBLE, Sc(16, s->dpi), Sc(y, s->dpi), Sc(410, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(lbl, WM_SETFONT, reinterpret_cast<WPARAM>(f.required ? s->fBold : s->fNorm), TRUE);
                    y += 21;
                    HWND ed = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                        Sc(16, s->dpi), Sc(y, s->dpi), Sc(410, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(ed, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                    s->hFields.push_back(ed);
                    y += 32;
                }
                y += 6;
                HWND bOk = CreateWindowExW(0, L"BUTTON", L"Enviar Formulario", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(198, s->dpi), Sc(y, s->dpi), Sc(132, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                HWND bCn = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(338, s->dpi), Sc(y, s->dpi), Sc(88, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                SendMessageW(bOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(bCn, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SetFocus(s->hTitle);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == IDOK) {
                    const std::string titleUtf8 = WideToUtf8(GetControlTextW(s->hTitle));
                    if (titleUtf8.empty()) {
                        MessageBoxW(hwnd, L"El t\u00EDtulo del registro es obligatorio.", L"Validaci\u00F3n de Formulario", MB_ICONWARNING);
                        return 0;
                    }
                    std::unordered_map<std::string, DbCellValue> vals;
                    for (std::size_t i = 0; i < s->form.fields.size() && i < s->hFields.size(); ++i) {
                        const std::string valTxt = WideToUtf8(GetControlTextW(s->hFields[i]));
                        if (valTxt.empty()) continue;
                        DbCellValue cv{};
                        cv.textUtf8 = valTxt;
                        cv.numberVal = std::atof(valTxt.c_str());
                        cv.boolVal = (valTxt == "true" || valTxt == "1" || valTxt == "si" || valTxt == "Si");
                        vals[s->form.fields[i].columnIdUtf8] = std::move(cv);
                    }
                    std::string createdId, err;
                    if (s->db->SubmitForm(s->form.formIdUtf8, titleUtf8, vals, &createdId, &err)) {
                        s->db->RecomputeFormulasAndRollups();
                        if (s->outStatus) *s->outStatus = "Formulario enviado (" + std::to_string(s->form.fields.size()) + " campos): " + createdId;
                        s->submitted = true;
                        DestroyWindow(hwnd);
                    } else {
                        MessageBoxW(hwnd, Utf8ToWide(err).c_str(), L"Validaci\u00F3n de Formulario", MB_ICONWARNING);
                    }
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernU_DbConnectedFormDlg";
        RegisterClassExW(&wc);
        reg = true;
    }
    const int dlgH = 148 + static_cast<int>(form.fields.size()) * 53;
    RECT prc{ 110, 110, 820, 620 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(458, st.dpi), h = Sc(dlgH, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernU_DbConnectedFormDlg", Utf8ToWide(form.titleUtf8).c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h,
        owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (dlg && IsWindow(dlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(dlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    return st.submitted;
}

// ----------------------------------------------------------------------------
// 8. Grounded Local / Multi-Provider AI Studio Dialog (Provider Switch + TTS + Citations + Export)
// ----------------------------------------------------------------------------
inline bool ShowGroundedAiStudioDialog(
    HWND owner,
    const DocumentModel& doc,
    const std::vector<SpeakerTranscriptSegment>& audioSegs,
    const LocalLlmConfig* optionalLlm,
    std::string& outInsertMarkdown,
    int* outJumpPage0 = nullptr,
    AiProviderVault* optionalVault = nullptr,
    SpeechSynthesizerWin32* optionalTts = nullptr)
{
    outInsertMarkdown.clear();
    if (outJumpPage0) *outJumpPage0 = -1;

    struct St {
        const DocumentModel* doc = nullptr;
        const std::vector<SpeakerTranscriptSegment>* segs = nullptr;
        const LocalLlmConfig* llm = nullptr;
        AiProviderVault* vault = nullptr;
        SpeechSynthesizerWin32* tts = nullptr;
        std::vector<GroundedCitation> citations;
        std::string lastAnswer;
        std::string insertMd;
        int jumpPage = -1;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hProvCombo = nullptr, hPrompt = nullptr, hOut = nullptr, hCites = nullptr;
    } st{};
    st.doc = &doc; st.segs = &audioSegs; st.llm = optionalLlm;
    st.vault = optionalVault; st.tts = optionalTts;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                HWND hPLbl = CreateWindowExW(0, L"STATIC", L"Motor IA:", WS_CHILD | WS_VISIBLE,
                    Sc(16, s->dpi), Sc(14, s->dpi), Sc(72, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hPLbl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                s->hProvCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                    Sc(90, s->dpi), Sc(10, s->dpi), Sc(386, s->dpi), Sc(220, s->dpi), hwnd, reinterpret_cast<HMENU>(860), nullptr, nullptr);
                SendMessageW(s->hProvCombo, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hProvCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Solo BM25 Local (Sin Red)"));
                int selProv = 0;
                if (s->vault) {
                    const auto& provs = s->vault->Providers();
                    for (std::size_t i = 0; i < provs.size(); ++i) {
                        const std::string item = provs[i].displayNameUtf8 + " (" + provs[i].modelNameUtf8 + ")" +
                            (provs[i].requiresApiKey && provs[i].apiKeyUtf8.empty() ? " [Sin clave]" : "");
                        SendMessageW(s->hProvCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(item).c_str()));
                        if (s->vault->IsEnabled() && provs[i].idUtf8 == s->vault->ActiveProviderId()) {
                            selProv = static_cast<int>(i) + 1;
                        }
                    }
                }
                SendMessageW(s->hProvCombo, CB_SETCURSEL, static_cast<WPARAM>(selProv), 0);

                HWND bSpeak = CreateWindowExW(0, L"BUTTON", L"Leer voz", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(482, s->dpi), Sc(10, s->dpi), Sc(104, s->dpi), Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(867), nullptr, nullptr);
                SendMessageW(bSpeak, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hPrompt = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"\u00BFQu\u00E9 conceptos clave contiene este cuaderno?",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(44, s->dpi), Sc(460, s->dpi), Sc(28, s->dpi), hwnd, nullptr, nullptr, nullptr);
                HWND bAsk = CreateWindowExW(0, L"BUTTON", L"Consultar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(482, s->dpi), Sc(43, s->dpi), Sc(104, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(861), nullptr, nullptr);
                SendMessageW(s->hPrompt, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bAsk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                s->hOut = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Seleccione Consultar, Resumen Ejecutivo o Tutor Paso a Paso.",
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                    Sc(16, s->dpi), Sc(80, s->dpi), Sc(570, s->dpi), Sc(170, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hOut, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hCites = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(258, s->dpi), Sc(570, s->dpi), Sc(78, s->dpi), hwnd, reinterpret_cast<HMENU>(862), nullptr, nullptr);
                SendMessageW(s->hCites, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                struct B { int id; const wchar_t* t; };
                const B btns[5] = {
                    {863, L"Resumen Ejec."}, {864, L"Exportar PDF"}, {865, L"Exportar PNG"},
                    {866, L"Insertar Nota"}, {IDCANCEL, L"Cerrar"}
                };
                for (int i = 0; i < 5; ++i) {
                    HWND b = CreateWindowExW(0, L"BUTTON", btns[i].t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        Sc(16 + i * 115, s->dpi), Sc(344, s->dpi), Sc(110, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(btns[i].id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                }
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                if (cid == 860 && HIWORD(wp) == CBN_SELCHANGE && s->vault) {
                    const LRESULT sel = SendMessageW(s->hProvCombo, CB_GETCURSEL, 0, 0);
                    if (sel <= 0) {
                        s->vault->SetEnabled(false);
                    } else if (static_cast<std::size_t>(sel - 1) < s->vault->Providers().size()) {
                        s->vault->SetActiveProviderId(s->vault->Providers()[static_cast<std::size_t>(sel - 1)].idUtf8);
                        s->vault->SetEnabled(true);
                    }
                    return 0;
                }
                if (cid == 867 && s->tts && !s->lastAnswer.empty()) {
                    if (s->tts->IsSpeaking()) s->tts->Stop();
                    else s->tts->SpeakAsync(s->lastAnswer);
                    return 0;
                }
                if (cid == 861) {
                    HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                    const AiProviderVault* vPtr = (s->vault && s->vault->IsEnabled()) ? s->vault : nullptr;
                    const auto ans = GroundedStudyAiAssistant::AnswerQuestionWithCitations(*s->doc, *s->segs, WideToUtf8(GetControlTextW(s->hPrompt)), s->llm, vPtr);
                    SetCursor(oldC);
                    s->lastAnswer = ans.answered ? ans.answerTextUtf8 : "Sin coincidencias en el cuaderno actual.";
                    s->citations = ans.citations;
                    std::string crlf;
                    for (char ch : s->lastAnswer) { if (ch == '\n') crlf += "\r\n"; else crlf.push_back(ch); }
                    SetWindowTextW(s->hOut, Utf8ToWide(crlf).c_str());
                    SendMessageW(s->hCites, LB_RESETCONTENT, 0, 0);
                    for (const auto& c : s->citations) {
                        const std::string ln = "Pag " + std::to_string(c.pageIndex0 + 1) + " [" + c.sourceKindUtf8 + "]: " + c.quoteSnippetUtf8;
                        SendMessageW(s->hCites, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                    return 0;
                }
                if (cid == 863) {
                    HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                    const AiProviderVault* vPtr = (s->vault && s->vault->IsEnabled()) ? s->vault : nullptr;
                    s->lastAnswer = GroundedStudyAiAssistant::GenerateExecutiveSummary(*s->doc, *s->segs, s->llm, vPtr);
                    SetCursor(oldC);
                    std::string crlf;
                    for (char ch : s->lastAnswer) { if (ch == '\n') crlf += "\r\n"; else crlf.push_back(ch); }
                    SetWindowTextW(s->hOut, Utf8ToWide(crlf).c_str());
                    return 0;
                }
                if (cid == 864 || cid == 865) {
                    if (s->lastAnswer.empty()) {
                        MessageBoxW(hwnd, L"Primero genere una respuesta con Consultar o Resumen Ejecutivo.", L"Exportar Respuesta", MB_ICONWARNING);
                        return 0;
                    }
                    DocumentModel tmp;
                    tmp.ResetToBlank(DocumentMode::PaginatedPdf);
                    tmp.SetTitle("Respuesta Asistente");
                    AnnotationObject tb;
                    tb.kind = ObjectKind::TextBox;
                    tb.bounds = { 48.0, 56.0, 500.0, 660.0 };
                    tb.textUtf8 = s->lastAnswer;
                    tmp.AddPageObject(0, std::move(tb));
                    std::wstring outPath;
                    std::string err;
                    if (cid == 864 && PickSaveFileWin32(hwnd, L"PDF (*.pdf)\0*.pdf\0\0", L"pdf", L"respuesta_asistente.pdf", L"Exportar Respuesta a PDF", outPath)) {
                        std::vector<std::uint8_t> bytes;
                        PdfSelectiveExportOptions opts{};
                        if (FlexcilStudyOverlay::ExportSelectivePdfToBytes(tmp, opts, bytes, &err)) {
                            std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
                            ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                            if (ofs.good()) {
                                MessageBoxW(hwnd, L"Respuesta exportada a PDF.", L"Exportaci\u00F3n", MB_ICONINFORMATION);
                            } else {
                                MessageBoxW(hwnd, L"No se pudo escribir el archivo PDF en disco.", L"Error de Exportaci\u00F3n", MB_ICONERROR);
                            }
                        } else {
                            MessageBoxW(hwnd, Utf8ToWide(err.empty() ? "Error generando PDF." : err).c_str(), L"Error de Exportaci\u00F3n", MB_ICONERROR);
                        }
                    } else if (cid == 865 && PickSaveFileWin32(hwnd, L"PNG (*.png)\0*.png\0\0", L"png", L"respuesta_asistente.png", L"Exportar Respuesta a PNG", outPath)) {
                        if (FlexcilStudyOverlay::ExportPageToPngOrJpegFile(tmp, 0, outPath, false, 1200, &err)) {
                            MessageBoxW(hwnd, L"Respuesta exportada como imagen PNG.", L"Exportaci\u00F3n", MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hwnd, Utf8ToWide(err.empty() ? "Error generando imagen PNG." : err).c_str(), L"Error de Exportaci\u00F3n", MB_ICONERROR);
                        }
                    }
                    return 0;
                }
                if (cid == 866 && !s->lastAnswer.empty()) {
                    s->insertMd = s->lastAnswer; s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (cid == 862 && HIWORD(wp) == LBN_DBLCLK) {
                    const LRESULT sel = SendMessageW(s->hCites, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && static_cast<std::size_t>(sel) < s->citations.size()) {
                        s->jumpPage = static_cast<int>(s->citations[static_cast<std::size_t>(sel)].pageIndex0);
                        s->done = true; DestroyWindow(hwnd); return 0;
                    }
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUGroundedAiStudioDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 110, 110, 840, 580 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(618, st.dpi), h = Sc(430, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUGroundedAiStudioDlg", L"Asistente de Estudio con Citas, Resumen y Exportaci\u00F3n",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outInsertMarkdown = st.insertMd;
    if (outJumpPage0) *outJumpPage0 = st.jumpPage;
    return !outInsertMarkdown.empty() || st.jumpPage >= 0;
}

// ----------------------------------------------------------------------------
// 9. Layers Manager Dialog (Reorder MoveLayer, RemoveLayer, Visibility, Lock)
// ----------------------------------------------------------------------------
inline bool ShowLayersManagerDialog(HWND owner, DocumentModel& doc, std::string& outStatus) {
    struct St {
        DocumentModel* doc = nullptr;
        bool mutated = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hList = nullptr;
    } st{};
    st.doc = &doc; st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            auto refresh = [](St* sp) {
                SendMessageW(sp->hList, LB_RESETCONTENT, 0, 0);
                for (const auto& ly : sp->doc->Layers()) {
                    const std::string ln = (ly.id == sp->doc->ActiveLayerId() ? "* " : "  ") +
                        ly.nameUtf8 + (ly.visible ? " [Visible]" : " [Oculta]") +
                        (ly.locked ? " [Bloq]" : "") + (ly.includeInExport ? " [Export]" : " [NoExp]");
                    SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                }
                if (!sp->doc->Layers().empty()) SendMessageW(sp->hList, LB_SETCURSEL, 0, 0);
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);
                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(14, s->dpi), Sc(460, s->dpi), Sc(180, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                struct B { int id; const wchar_t* t; };
                const B btns[6] = { {871, L"+ Capa"}, {872, L"Subir"}, {873, L"Bajar"}, {874, L"Ocultar/Ver"}, {875, L"Eliminar"}, {IDCANCEL, L"Cerrar"} };
                for (int i = 0; i < 6; ++i) {
                    HWND b = CreateWindowExW(0, L"BUTTON", btns[i].t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        Sc(16 + (i % 3) * 156, s->dpi), Sc(204 + (i / 3) * 36, s->dpi), Sc(148, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(btns[i].id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                }
                refresh(s);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                const LRESULT sel = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                const auto& lays = s->doc->Layers();
                const bool hasSel = (sel >= 0 && static_cast<std::size_t>(sel) < lays.size());
                if (cid == 871) {
                    std::wstring nm = L"Capa de Notas";
                    if (QuickInputDialog(hwnd, L"Nueva Capa", L"Nombre de la capa:", nm) && !nm.empty()) {
                        const std::uint64_t lid = s->doc->AddLayer(WideToUtf8(nm));
                        s->doc->SetActiveLayerId(lid);
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == 872 && hasSel && sel > 0) {
                    s->doc->MoveLayer(static_cast<std::size_t>(sel), static_cast<std::size_t>(sel - 1));
                    s->mutated = true; refresh(s); return 0;
                }
                if (cid == 873 && hasSel && static_cast<std::size_t>(sel + 1) < lays.size()) {
                    s->doc->MoveLayer(static_cast<std::size_t>(sel), static_cast<std::size_t>(sel + 1));
                    s->mutated = true; refresh(s); return 0;
                }
                if (cid == 874 && hasSel) {
                    const auto ly = lays[static_cast<std::size_t>(sel)];
                    s->doc->SetLayerProperties(ly.id, !ly.visible, ly.locked, ly.includeInExport, ly.opacity);
                    s->doc->SetActiveLayerId(ly.id);
                    s->mutated = true; refresh(s); return 0;
                }
                if (cid == 875 && hasSel && lays.size() > 1) {
                    if (s->doc->RemoveLayer(lays[static_cast<std::size_t>(sel)].id)) { s->mutated = true; refresh(s); }
                    return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernULayersMgrDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 140, 140, 740, 520 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(508, st.dpi), h = Sc(324, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernULayersMgrDlg", L"Gestor de Capas (Orden, Visibilidad y Exportaci\u00F3n)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (st.mutated) outStatus = "Capas actualizadas (" + std::to_string(doc.Layers().size()) + " capas)";
    return st.mutated;
}

// ----------------------------------------------------------------------------
// 10. PDF Annotations, Bookmarks, Source Metadata, Attachments & InkLinks Manager
// ----------------------------------------------------------------------------
inline bool ShowPdfAnnotationsAndBookmarksDialog(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    FlexcilStudyOverlay& flexcil,
    std::size_t activePage0,
    int& outJumpPage0,
    std::string& outStatus)
{
    outJumpPage0 = -1;
    struct St {
        DocumentModel* doc = nullptr;
        CommandStack* stack = nullptr;
        FlexcilStudyOverlay* flex = nullptr;
        std::size_t curPage = 0;
        int tab = 0; // 0=Anotaciones, 1=Marcadores, 2=Adjuntos, 3=InkLinks
        std::vector<PdfAnnotationInventoryItem> annots;
        int jumpPage = -1;
        bool mutated = false, done = false;
        std::string status;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hMode = nullptr, hList = nullptr;
    } st{};
    st.doc = &doc; st.stack = &cmdStack; st.flex = &flexcil; st.curPage = activePage0;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            auto refresh = [](St* sp) {
                SendMessageW(sp->hList, LB_RESETCONTENT, 0, 0);
                sp->annots.clear();
                if (sp->tab == 0) {
                    sp->annots = FlexcilStudyOverlay::ListAllAnnotations(*sp->doc);
                    for (const auto& a : sp->annots) {
                        const std::string ln = "Pag " + std::to_string(a.pageIndex0 + 1) + " [#" + std::to_string(a.objectId) + "] " + a.summaryUtf8;
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                } else if (sp->tab == 1) {
                    for (const auto& bm : sp->doc->Bookmarks()) {
                        const std::string ln = "Pag " + std::to_string(bm.pageIndex + 1) + " — " + bm.titleUtf8;
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                } else if (sp->tab == 2) {
                    for (const auto& at : sp->doc->Attachments()) {
                        const std::string ln = at.filenameUtf8 + " (" + std::to_string(at.sizeBytes) + " B)";
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                } else {
                    for (const auto& lk : sp->doc->InkLinks()) {
                        const std::string ln = lk.labelUtf8 + " (Pag " + std::to_string(lk.sourcePageIndex0 + 1) + ")";
                        SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                    }
                }
                if (SendMessageW(sp->hList, LB_GETCOUNT, 0, 0) > 0) SendMessageW(sp->hList, LB_SETCURSEL, 0, 0);
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);
                s->hMode = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(280, s->dpi), Sc(160, s->dpi), hwnd, reinterpret_cast<HMENU>(881), nullptr, nullptr);
                SendMessageW(s->hMode, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Inventario de Anotaciones"));
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Marcadores e \u00CDndice PDF"));
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Archivos Adjuntos"));
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Enlaces de Tinta (InkLinks)"));
                SendMessageW(s->hMode, CB_SETCURSEL, 0, 0);

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(46, s->dpi), Sc(580, s->dpi), Sc(196, s->dpi), hwnd, reinterpret_cast<HMENU>(882), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                struct B { int id; const wchar_t* t; };
                const B r1[4] = { {883, L"Exportar MD"}, {884, L"Importar PDF Edit."}, {885, L"Firmar Tinta"}, {886, L"Metadatos PDF"} };
                const B r2[4] = { {887, L"Renom. Marcador"}, {888, L"Eliminar Elem."}, {IDOK, L"Ir a P\u00E1gina"}, {IDCANCEL, L"Cerrar"} };
                for (int i = 0; i < 4; ++i) {
                    HWND b1 = CreateWindowExW(0, L"BUTTON", r1[i].t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        Sc(16 + i * 146, s->dpi), Sc(252, s->dpi), Sc(140, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(r1[i].id)), nullptr, nullptr);
                    HWND b2 = CreateWindowExW(0, L"BUTTON", r2[i].t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        Sc(16 + i * 146, s->dpi), Sc(290, s->dpi), Sc(140, s->dpi), Sc(30, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(r2[i].id)), nullptr, nullptr);
                    SendMessageW(b1, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                    SendMessageW(b2, WM_SETFONT, reinterpret_cast<WPARAM>(r2[i].id == IDOK ? s->fBold : s->fNorm), TRUE);
                }
                refresh(s);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                if (cid == 881 && HIWORD(wp) == CBN_SELCHANGE) {
                    s->tab = static_cast<int>(SendMessageW(s->hMode, CB_GETCURSEL, 0, 0));
                    refresh(s); return 0;
                }
                const LRESULT sel = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                if (cid == 883) {
                    std::wstring outMd;
                    if (PickSaveFileWin32(hwnd, L"Markdown (*.md)\0*.md\0\0", L"md", L"resumen_anotaciones.md", L"Exportar Resumen de Anotaciones", outMd)) {
                        const std::string md = FlexcilStudyOverlay::ExportAnnotationsMarkdownSummary(*s->doc);
                        std::ofstream ofs(outMd, std::ios::binary | std::ios::trunc);
                        ofs.write(md.data(), static_cast<std::streamsize>(md.size()));
                        s->status = "Resumen de anotaciones exportado a Markdown";
                    }
                    return 0;
                }
                if (cid == 884) {
                    std::wstring inPdf;
                    if (PickOpenFileWin32(hwnd, L"PDF (*.pdf)\0*.pdf\0\0", L"Importar Anotaciones Editables PDF", inPdf)) {
                        std::ifstream ifs(inPdf, std::ios::binary | std::ios::ate);
                        if (ifs.is_open()) {
                            const auto sz = ifs.tellg();
                            std::vector<std::uint8_t> bytes(static_cast<std::size_t>((std::max)(sz, std::streampos(0))));
                            ifs.seekg(0, std::ios::beg);
                            if (!bytes.empty()) ifs.read(reinterpret_cast<char*>(bytes.data()), sz);
                            DocumentModel imported; std::string err;
                            if (FlexcilStudyOverlay::ImportEditablePdfAnnotationsFromBytes(bytes, imported, &err)) {
                                if (!imported.Pages().empty() && s->curPage < s->doc->Pages().size()) {
                                    for (const auto& o : imported.Pages()[0].objects) s->doc->AddPageObject(s->curPage, o);
                                    s->mutated = true; s->status = "Anotaciones PDF editables importadas"; refresh(s);
                                }
                            } else {
                                MessageBoxW(hwnd, Utf8ToWide(err).c_str(), L"Importar Anotaciones PDF", MB_ICONWARNING);
                            }
                        }
                    }
                    return 0;
                }
                if (cid == 885) {
                    // Sin firma falsa (revisión del 27-nov-2026): antes insertaba un trazo fijo de 4
                    // puntos como si fuera la firma de la persona.
                    MessageBoxW(hwnd,
                        L"Esta opción ponía un trazo fijo en lugar de tu firma, así que se quitó.\n\n"
                        L"Para firmar, escribe tu firma con el lápiz directamente sobre la página.",
                        L"Firmar con tinta", MB_ICONINFORMATION);
                    return 0;
                }
                if (cid == 886) {
                    PdfSourceMetadata m{};
                    m.docIdUtf8 = "doc_" + std::to_string(s->doc->Pages().size());
                    m.titleUtf8 = s->doc->Title();
                    m.authorUtf8 = "Autor Local";
                    m.pageCount = s->doc->Pages().size();
                    s->flex->IndexPdfSourceMetadata(m);
                    std::wstring qW = Utf8ToWide(s->doc->Title());
                    if (QuickInputDialog(hwnd, L"Buscar Metadatos de Fuentes PDF", L"Consulta:", qW)) {
                        const auto hits = s->flex->SearchIndexedPdfMetadata(WideToUtf8(qW));
                        std::string summaryTxt = "Fuentes indexadas coincidentes: " + std::to_string(hits.size());
                        for (const auto& h : hits) summaryTxt += "\n- " + h.titleUtf8 + " (" + std::to_string(h.pageCount) + " pags)";
                        MessageBoxW(hwnd, Utf8ToWide(summaryTxt).c_str(), L"Metadatos PDF", MB_ICONINFORMATION);
                    }
                    return 0;
                }
                if (cid == 887 && s->tab == 1 && sel >= 0 && static_cast<std::size_t>(sel) < s->doc->Bookmarks().size()) {
                    const auto bm = s->doc->Bookmarks()[static_cast<std::size_t>(sel)];
                    std::wstring nt = Utf8ToWide(bm.titleUtf8);
                    if (QuickInputDialog(hwnd, L"Renombrar Marcador", L"Nuevo t\u00EDtulo:", nt) && !nt.empty()) {
                        FlexcilStudyOverlay::RenameBookmark(*s->doc, bm.id, WideToUtf8(nt));
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == 888 && sel >= 0) {
                    if (s->tab == 1 && static_cast<std::size_t>(sel) < s->doc->Bookmarks().size()) {
                        s->doc->RemoveBookmark(s->doc->Bookmarks()[static_cast<std::size_t>(sel)].id); s->mutated = true; refresh(s);
                    } else if (s->tab == 2 && static_cast<std::size_t>(sel) < s->doc->Attachments().size()) {
                        s->doc->RemoveAttachment(s->doc->Attachments()[static_cast<std::size_t>(sel)].assetKey); s->mutated = true; refresh(s);
                    } else if (s->tab == 3 && static_cast<std::size_t>(sel) < s->doc->InkLinks().size()) {
                        s->doc->RemoveInkLink(s->doc->InkLinks()[static_cast<std::size_t>(sel)].id); s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == IDOK && sel >= 0) {
                    if (s->tab == 0 && static_cast<std::size_t>(sel) < s->annots.size()) s->jumpPage = static_cast<int>(s->annots[static_cast<std::size_t>(sel)].pageIndex0);
                    else if (s->tab == 1 && static_cast<std::size_t>(sel) < s->doc->Bookmarks().size()) s->jumpPage = static_cast<int>(s->doc->Bookmarks()[static_cast<std::size_t>(sel)].pageIndex);
                    else if (s->tab == 3 && static_cast<std::size_t>(sel) < s->doc->InkLinks().size()) s->jumpPage = s->doc->InkLinks()[static_cast<std::size_t>(sel)].sourcePageIndex0;
                    s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUPdfAnnotsMgrDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 110, 110, 840, 560 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(628, st.dpi), h = Sc(374, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUPdfAnnotsMgrDlg", L"Gestor de Anotaciones, Marcadores, Metadatos y Adjuntos PDF",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outJumpPage0 = st.jumpPage;
    if (!st.status.empty()) outStatus = st.status;
    return st.mutated;
}

// ----------------------------------------------------------------------------
// 11. Snippet Library Full Manager (FilterByCategory, UpdateSnippetName, RemoveSnippet, Insert)
// ----------------------------------------------------------------------------
inline std::string ShowSnippetManagerDialog(HWND owner, ReusableElementLibrary& lib, bool& outMutated) {
    outMutated = false;
    struct St {
        ReusableElementLibrary* lib = nullptr;
        std::vector<ReusableElementEntry> shown;
        std::string catFilter;
        std::string chosenId;
        bool mutated = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hCat = nullptr, hList = nullptr;
    } st{};
    st.lib = &lib; st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            auto refresh = [](St* sp) {
                SendMessageW(sp->hList, LB_RESETCONTENT, 0, 0);
                sp->shown = sp->catFilter.empty() ? sp->lib->Entries() : sp->lib->FilterByCategory(sp->catFilter);
                for (const auto& e : sp->shown) {
                    const std::string ln = e.nameUtf8 + " [" + e.categoryUtf8 + "] (" + std::to_string(e.objects.size()) + " obj)";
                    SendMessageW(sp->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                }
                if (!sp->shown.empty()) SendMessageW(sp->hList, LB_SETCURSEL, 0, 0);
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);
                s->hCat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(320, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                HWND bF = CreateWindowExW(0, L"BUTTON", L"Filtrar Cat.", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(342, s->dpi), Sc(11, s->dpi), Sc(104, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(891), nullptr, nullptr);
                SendMessageW(s->hCat, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bF, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hCat, 0x1501, TRUE, reinterpret_cast<LPARAM>(L"Filtrar por categor\u00EDa (ej. Fragmentos)..."));

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(46, s->dpi), Sc(430, s->dpi), Sc(180, s->dpi), hwnd, reinterpret_cast<HMENU>(892), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                HWND bRen = CreateWindowExW(0, L"BUTTON", L"Renombrar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(16, s->dpi), Sc(236, s->dpi), Sc(100, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(893), nullptr, nullptr);
                HWND bDel = CreateWindowExW(0, L"BUTTON", L"Eliminar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(122, s->dpi), Sc(236, s->dpi), Sc(100, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(894), nullptr, nullptr);
                HWND bOk = CreateWindowExW(0, L"BUTTON", L"Insertar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(240, s->dpi), Sc(236, s->dpi), Sc(100, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                HWND bCan = CreateWindowExW(0, L"BUTTON", L"Cerrar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(346, s->dpi), Sc(236, s->dpi), Sc(100, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                SendMessageW(bRen, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bDel, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(bCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                refresh(s);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                const LRESULT sel = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                const bool hasSel = (sel >= 0 && static_cast<std::size_t>(sel) < s->shown.size());
                if (cid == 891) { s->catFilter = WideToUtf8(GetControlTextW(s->hCat)); refresh(s); return 0; }
                if (cid == 893 && hasSel) {
                    std::wstring nm = Utf8ToWide(s->shown[static_cast<std::size_t>(sel)].nameUtf8);
                    if (QuickInputDialog(hwnd, L"Renombrar Fragmento", L"Nuevo nombre:", nm) && !nm.empty()) {
                        s->lib->UpdateSnippetName(s->shown[static_cast<std::size_t>(sel)].idUtf8, WideToUtf8(nm));
                        s->mutated = true; refresh(s);
                    }
                    return 0;
                }
                if (cid == 894 && hasSel) {
                    s->lib->RemoveSnippet(s->shown[static_cast<std::size_t>(sel)].idUtf8);
                    s->mutated = true; refresh(s); return 0;
                }
                if ((cid == IDOK || (cid == 892 && HIWORD(wp) == LBN_DBLCLK)) && hasSel) {
                    s->chosenId = s->shown[static_cast<std::size_t>(sel)].idUtf8;
                    s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUSnippetMgrDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 140, 140, 740, 520 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(478, st.dpi), h = Sc(318, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUSnippetMgrDlg", L"Gestor de Elementos y Pegatinas Reutilizables",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outMutated = st.mutated;
    return st.chosenId;
}

// ----------------------------------------------------------------------------
// 12. Universal Accessibility Center Dialog (SAPI TTS, Reading Ruler, Dyslexia, Tints, ColorBlind)
// ----------------------------------------------------------------------------
inline bool ShowAccessibilityCenterDialog(
    HWND owner,
    AccessibilityPreferences& prefs,
    SpeechSynthesizerWin32& tts)
{
    struct St {
        AccessibilityPreferences work{};
        AccessibilityPreferences* outPrefs = nullptr;
        SpeechSynthesizerWin32* tts = nullptr;
        std::vector<std::string> voices;
        bool accepted = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hRuler = nullptr, hRulerH = nullptr, hDyslexia = nullptr, hFocusRing = nullptr;
        HWND hTint = nullptr, hColorBlind = nullptr, hScale = nullptr;
        HWND hVoice = nullptr, hRate = nullptr;
    } st{};
    st.work = prefs;
    st.outPrefs = &prefs;
    st.tts = &tts;
    st.voices = tts.EnumerateInstalledVoices();
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                auto lbl = [&](const wchar_t* t, int x, int y, int w, int h, bool bold = false) {
                    HWND hw = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(h, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };

                lbl(L"Lectura y Confort Visual:", 16, 12, 460, 18, true);
                s->hRuler = CreateWindowExW(0, L"BUTTON", L"Regla de lectura enfocada (sigue el puntero)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(16, s->dpi), Sc(36, s->dpi), Sc(310, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hRuler, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hRuler, BM_SETCHECK, s->work.readingRulerEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

                s->hRulerH = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(336, s->dpi), Sc(34, s->dpi), Sc(150, s->dpi), Sc(140, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hRulerH, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                const wchar_t* kHeights[4] = { L"Banda 56 px", L"Banda 76 px", L"Banda 104 px", L"Banda 136 px" };
                const int kHeightVals[4] = { 56, 76, 104, 136 };
                int hSel = 1;
                for (int i = 0; i < 4; ++i) {
                    SendMessageW(s->hRulerH, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kHeights[i]));
                    if (std::abs(s->work.readingRulerHeightPx - kHeightVals[i]) <= 12) hSel = i;
                }
                SendMessageW(s->hRulerH, CB_SETCURSEL, static_cast<WPARAM>(hSel), 0);

                s->hDyslexia = CreateWindowExW(0, L"BUTTON", L"Modo lectura amigable para dislexia (Verdana, interlineado 1.48x)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(16, s->dpi), Sc(64, s->dpi), Sc(470, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hDyslexia, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hDyslexia, BM_SETCHECK, s->work.dyslexiaFriendlyMode ? BST_CHECKED : BST_UNCHECKED, 0);

                s->hFocusRing = CreateWindowExW(0, L"BUTTON", L"Anillo de foco reforzado (2.6 px alto contraste)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(16, s->dpi), Sc(90, s->dpi), Sc(470, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hFocusRing, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hFocusRing, BM_SETCHECK, s->work.enhancedFocusRing ? BST_CHECKED : BST_UNCHECKED, 0);

                lbl(L"Tinte de papel anti-fatiga:", 16, 122, 224, 18);
                s->hTint = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(16, s->dpi), Sc(142, s->dpi), Sc(228, s->dpi), Sc(160, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hTint, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                for (int i = 0; i <= 4; ++i) {
                    SendMessageW(s->hTint, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(AccessibilityEngine::PaperTintLabelUtf8(static_cast<PaperTintPreset>(i))).c_str()));
                }
                SendMessageW(s->hTint, CB_SETCURSEL, static_cast<WPARAM>(static_cast<int>(s->work.paperTint)), 0);

                lbl(L"Paleta para daltonismo:", 258, 122, 228, 18);
                s->hColorBlind = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(258, s->dpi), Sc(142, s->dpi), Sc(228, s->dpi), Sc(160, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hColorBlind, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                for (int i = 0; i <= 3; ++i) {
                    SendMessageW(s->hColorBlind, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(AccessibilityEngine::ColorBlindModeLabelUtf8(static_cast<ColorBlindPaletteMode>(i))).c_str()));
                }
                SendMessageW(s->hColorBlind, CB_SETCURSEL, static_cast<WPARAM>(static_cast<int>(s->work.colorBlindMode)), 0);

                lbl(L"Escala de interfaz:", 16, 178, 220, 18);
                s->hScale = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(16, s->dpi), Sc(198, s->dpi), Sc(228, s->dpi), Sc(140, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hScale, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                const wchar_t* kScales[4] = { L"100% (Est\u00E1ndar)", L"115% (Amplio)", L"130% (Grande)", L"150% (Extra grande)" };
                const int kScaleVals[4] = { 100, 115, 130, 150 };
                int scSel = 0;
                for (int i = 0; i < 4; ++i) {
                    SendMessageW(s->hScale, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kScales[i]));
                    if (s->work.uiFontScalePct >= kScaleVals[i] - 5) scSel = i;
                }
                SendMessageW(s->hScale, CB_SETCURSEL, static_cast<WPARAM>(scSel), 0);

                lbl(L"Lectura en voz alta (voz del sistema):", 16, 236, 460, 18, true);
                s->hVoice = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                    Sc(16, s->dpi), Sc(258, s->dpi), Sc(306, s->dpi), Sc(180, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hVoice, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hVoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"(Voz predeterminada del sistema)"));
                int vSel = 0;
                for (std::size_t i = 0; i < s->voices.size(); ++i) {
                    SendMessageW(s->hVoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(s->voices[i]).c_str()));
                    if (s->voices[i] == s->work.ttsVoiceNameUtf8) vSel = static_cast<int>(i) + 1;
                }
                SendMessageW(s->hVoice, CB_SETCURSEL, static_cast<WPARAM>(vSel), 0);

                s->hRate = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(330, s->dpi), Sc(258, s->dpi), Sc(156, s->dpi), Sc(160, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hRate, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                const wchar_t* kRates[5] = { L"0.75x (Lento)", L"1.0x (Normal)", L"1.25x (Agil)", L"1.5x (R\u00E1pido)", L"1.8x (Muy r\u00E1pido)" };
                const int kRateVals[5] = { -3, 0, 2, 4, 6 };
                int rSel = 1;
                for (int i = 0; i < 5; ++i) {
                    SendMessageW(s->hRate, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kRates[i]));
                    if (std::abs(s->work.ttsRate - kRateVals[i]) <= 1) rSel = i;
                }
                SendMessageW(s->hRate, CB_SETCURSEL, static_cast<WPARAM>(rSel), 0);

                auto mkBtn = [&](const wchar_t* t, int x, int w, int id, bool bold = false) {
                    HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (bold ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(x, s->dpi), Sc(302, s->dpi), Sc(w, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };
                mkBtn(L"Probar voz", 16, 102, 910);
                mkBtn(L"Detener voz", 124, 106, 911);
                mkBtn(L"Cancelar", 274, 100, IDCANCEL);
                mkBtn(L"Aplicar", 382, 104, IDOK, true);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                const int kRateVals[5] = { -3, 0, 2, 4, 6 };
                const int kHeightVals[4] = { 56, 76, 104, 136 };
                const int kScaleVals[4] = { 100, 115, 130, 150 };
                if (cid == 910) {
                    const LRESULT vIdx = SendMessageW(s->hVoice, CB_GETCURSEL, 0, 0);
                    const LRESULT rIdx = SendMessageW(s->hRate, CB_GETCURSEL, 0, 0);
                    if (vIdx > 0 && static_cast<std::size_t>(vIdx - 1) < s->voices.size()) {
                        s->tts->SelectVoiceByName(s->voices[static_cast<std::size_t>(vIdx - 1)]);
                    }
                    s->tts->SetRate(kRateVals[std::clamp<int>(static_cast<int>(rIdx), 0, 4)]);
                    s->tts->SpeakAsync("Prueba de lectura en voz alta de CuadernU. " + AccessibilityEngine::VerbalizeMathExpressionForSpeech("E = m * c^2"));
                    return 0;
                }
                if (cid == 911) { s->tts->Stop(); return 0; }
                if (cid == IDOK) {
                    s->work.readingRulerEnabled = (SendMessageW(s->hRuler, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    s->work.readingRulerHeightPx = kHeightVals[std::clamp<int>(static_cast<int>(SendMessageW(s->hRulerH, CB_GETCURSEL, 0, 0)), 0, 3)];
                    s->work.dyslexiaFriendlyMode = (SendMessageW(s->hDyslexia, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    s->work.enhancedFocusRing = (SendMessageW(s->hFocusRing, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    s->work.paperTint = static_cast<PaperTintPreset>(std::clamp<int>(static_cast<int>(SendMessageW(s->hTint, CB_GETCURSEL, 0, 0)), 0, 4));
                    s->work.colorBlindMode = static_cast<ColorBlindPaletteMode>(std::clamp<int>(static_cast<int>(SendMessageW(s->hColorBlind, CB_GETCURSEL, 0, 0)), 0, 3));
                    s->work.uiFontScalePct = kScaleVals[std::clamp<int>(static_cast<int>(SendMessageW(s->hScale, CB_GETCURSEL, 0, 0)), 0, 3)];
                    s->work.ttsRate = kRateVals[std::clamp<int>(static_cast<int>(SendMessageW(s->hRate, CB_GETCURSEL, 0, 0)), 0, 4)];
                    const LRESULT vIdx = SendMessageW(s->hVoice, CB_GETCURSEL, 0, 0);
                    s->work.ttsVoiceNameUtf8 = (vIdx > 0 && static_cast<std::size_t>(vIdx - 1) < s->voices.size()) ? s->voices[static_cast<std::size_t>(vIdx - 1)] : "";
                    *s->outPrefs = s->work;
                    s->tts->SetRate(s->work.ttsRate);
                    if (!s->work.ttsVoiceNameUtf8.empty()) s->tts->SelectVoiceByName(s->work.ttsVoiceNameUtf8);
                    s->accepted = true; s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUA11yCenterDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 110, 110, 820, 560 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(518, st.dpi), h = Sc(386, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUA11yCenterDlg", L"Centro de Accesibilidad Universal",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    return st.accepted;
}

// ----------------------------------------------------------------------------
// 13. Guided Step-by-Step Tutor Dialog (3 Progressive Levels + Socratic Mode + TTS)
// ----------------------------------------------------------------------------
inline bool ShowGuidedTutorDialog(
    HWND owner,
    const DocumentModel& doc,
    const AiProviderVault* vault,
    SpeechSynthesizerWin32* tts,
    std::string& outInsertMarkdown)
{
    outInsertMarkdown.clear();
    struct St {
        const DocumentModel* doc = nullptr;
        const AiProviderVault* vault = nullptr;
        SpeechSynthesizerWin32* tts = nullptr;
        std::string lastText;
        std::string insertMd;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hTopic = nullptr, hSocratic = nullptr, hOut = nullptr, hMeta = nullptr;
    } st{};
    st.doc = &doc; st.vault = vault; st.tts = tts;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto runLevel = [&](TutorHintLevel lvl) {
                const std::string topic = WideToUtf8(GetControlTextW(s->hTopic));
                if (topic.empty()) return;
                const bool socratic = (SendMessageW(s->hSocratic, BM_GETCHECK, 0, 0) == BST_CHECKED);
                HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                const GuidedTutorResult r = StudyAssistantTools::RunGuidedTutor(*s->doc, topic, lvl, socratic, s->vault);
                SetCursor(oldC);
                if (r.ok) {
                    s->lastText = r.responseMarkdownUtf8;
                    std::string crlf;
                    for (char ch : s->lastText) { if (ch == '\n') crlf += "\r\n"; else crlf.push_back(ch); }
                    SetWindowTextW(s->hOut, Utf8ToWide(crlf).c_str());
                    const std::string m = "Motor: " + r.providerUsedUtf8 + (r.usedLiveAi ? (" (" + std::to_string(r.latencyMs) + " ms)") : "");
                    SetWindowTextW(s->hMeta, Utf8ToWide(m).c_str());
                }
            };

            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                HWND hl = CreateWindowExW(0, L"STATIC", L"Problema, ecuaci\u00F3n o concepto a estudiar:", WS_CHILD | WS_VISIBLE,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(460, s->dpi), Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                s->hTopic = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Integraci\u00F3n por partes o resoluci\u00F3n de circuito RC",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(34, s->dpi), Sc(568, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hTopic, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hSocratic = CreateWindowExW(0, L"BUTTON", L"Modo gu\u00ED""a socr\u00E1tica (no revelar resultado final en niveles 1 y 2)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(16, s->dpi), Sc(66, s->dpi), Sc(460, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hSocratic, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hSocratic, BM_SETCHECK, BST_CHECKED, 0);

                auto mkBtn = [&](const wchar_t* t, int x, int y, int w, int id, bool bold = false) {
                    HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (bold ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };
                mkBtn(L"1. Pista Conceptual", 16, 94, 182, 921, true);
                mkBtn(L"2. Pasos Guiados",    208, 94, 182, 922);
                mkBtn(L"3. Verificar Soluci\u00F3n", 400, 94, 184, 923);

                s->hMeta = CreateWindowExW(0, L"STATIC", L"Seleccione un nivel de gu\u00ED""a progresiva.", WS_CHILD | WS_VISIBLE,
                    Sc(16, s->dpi), Sc(128, s->dpi), Sc(568, s->dpi), Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hMeta, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hOut = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                    Sc(16, s->dpi), Sc(150, s->dpi), Sc(568, s->dpi), Sc(182, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hOut, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                mkBtn(L"Leer voz",            16, 342, 110, 924);
                mkBtn(L"Insertar en nota",   340, 342, 134, 925);
                mkBtn(L"Cerrar",             482, 342, 102, IDCANCEL);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                if (cid == 921) { runLevel(TutorHintLevel::ConceptualHint); return 0; }
                if (cid == 922) { runLevel(TutorHintLevel::StepByStepPlan); return 0; }
                if (cid == 923) { runLevel(TutorHintLevel::FullVerification); return 0; }
                if (cid == 924 && s->tts && !s->lastText.empty()) {
                    if (s->tts->IsSpeaking()) s->tts->Stop();
                    else s->tts->SpeakAsync(s->lastText);
                    return 0;
                }
                if (cid == 925 && !s->lastText.empty()) {
                    s->insertMd = s->lastText; s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUGuidedTutorDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 110, 110, 840, 580 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(616, st.dpi), h = Sc(424, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUGuidedTutorDlg", L"Tutor Paso a Paso (Gu\u00ED""a Socr\u00E1tica)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outInsertMarkdown = st.insertMd;
    return !outInsertMarkdown.empty();
}

// ----------------------------------------------------------------------------
// 14. Smart Pomodoro Timer Dialog (25/5, 50/10, Custom + Focus Mode)
// ----------------------------------------------------------------------------
inline bool ShowPomodoroDialog(HWND owner, PomodoroTimerState& pomo) {
    struct St {
        PomodoroTimerState* pomo = nullptr;
        bool changed = false, done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hFocusMin = nullptr, hBreakMin = nullptr, hAutoFocus = nullptr, hReadout = nullptr;
    } st{};
    st.pomo = &pomo;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto refreshReadout = [&]() {
                const std::string txt = "Estado: " + s->pomo->FormatHudBadgeUtf8() +
                    "  |  Ciclos completados: " + std::to_string(s->pomo->completedCycles);
                SetWindowTextW(s->hReadout, Utf8ToWide(txt).c_str());
            };

            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                s->hReadout = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                    Sc(16, s->dpi), Sc(14, s->dpi), Sc(390, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hReadout, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                auto mkBtn = [&](const wchar_t* t, int x, int y, int w, int id, bool bold = false) {
                    HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (bold ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };
                mkBtn(L"Cl\u00E1sico 25 / 5", 16, 44, 124, 931);
                mkBtn(L"Profundo 50 / 10", 148, 44, 128, 932);
                mkBtn(L"Sprint 15 / 3", 284, 44, 120, 933);

                HWND lf = CreateWindowExW(0, L"STATIC", L"Foco (min):", WS_CHILD | WS_VISIBLE, Sc(16, s->dpi), Sc(86, s->dpi), Sc(84, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(lf, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                s->hFocusMin = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(s->pomo->focusMinutes).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, Sc(104, s->dpi), Sc(83, s->dpi), Sc(72, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hFocusMin, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                HWND lb = CreateWindowExW(0, L"STATIC", L"Pausa (min):", WS_CHILD | WS_VISIBLE, Sc(204, s->dpi), Sc(86, s->dpi), Sc(90, s->dpi), Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(lb, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                s->hBreakMin = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(s->pomo->breakMinutes).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER, Sc(298, s->dpi), Sc(83, s->dpi), Sc(72, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hBreakMin, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hAutoFocus = CreateWindowExW(0, L"BUTTON", L"Activar Modo Concentraci\u00F3n al iniciar bloque de foco",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(16, s->dpi), Sc(118, s->dpi), Sc(388, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hAutoFocus, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hAutoFocus, BM_SETCHECK, s->pomo->autoEnableFocusMode ? BST_CHECKED : BST_UNCHECKED, 0);

                mkBtn(s->pomo->running && !s->pomo->paused ? L"Pausar" : L"Iniciar", 16, 152, 120, 934, true);
                mkBtn(L"Reiniciar", 144, 152, 120, 935);
                mkBtn(L"Cerrar", 284, 152, 120, IDCANCEL);
                refreshReadout();
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                if (cid == 931) { SetWindowTextW(s->hFocusMin, L"25"); SetWindowTextW(s->hBreakMin, L"5"); return 0; }
                if (cid == 932) { SetWindowTextW(s->hFocusMin, L"50"); SetWindowTextW(s->hBreakMin, L"10"); return 0; }
                if (cid == 933) { SetWindowTextW(s->hFocusMin, L"15"); SetWindowTextW(s->hBreakMin, L"3"); return 0; }
                if (cid == 934) {
                    const int fm = (std::max)(1, _wtoi(GetControlTextW(s->hFocusMin).c_str()));
                    const int bm = (std::max)(1, _wtoi(GetControlTextW(s->hBreakMin).c_str()));
                    const bool af = (SendMessageW(s->hAutoFocus, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (!s->pomo->running) {
                        s->pomo->Configure(fm, bm, af);
                        s->pomo->Start(GetTickCount64());
                    } else {
                        s->pomo->autoEnableFocusMode = af;
                        s->pomo->TogglePause(GetTickCount64());
                    }
                    s->changed = true;
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (cid == 935) {
                    const int fm = (std::max)(1, _wtoi(GetControlTextW(s->hFocusMin).c_str()));
                    const int bm = (std::max)(1, _wtoi(GetControlTextW(s->hBreakMin).c_str()));
                    s->pomo->Configure(fm, bm, SendMessageW(s->hAutoFocus, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    s->changed = true;
                    refreshReadout();
                    return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUPomodoroDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 140, 140, 740, 520 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(438, st.dpi), h = Sc(236, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUPomodoroDlg", L"Temporizador Pomodoro de Estudio",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    return st.changed;
}

// ----------------------------------------------------------------------------
// 15. Academic Bibliography Citation Generator Dialog (APA 7 / IEEE / BibTeX)
// ----------------------------------------------------------------------------
inline bool ShowBibliographyCitationDialog(
    HWND owner,
    const DocumentModel& doc,
    const FlexcilStudyOverlay* flexcil,
    std::string& outInsertText)
{
    outInsertText.clear();
    struct St {
        std::vector<AcademicSourceInfo> candidates;
        std::string formatted;
        std::string insertText;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hCand = nullptr, hAuthors = nullptr, hTitle = nullptr, hYear = nullptr, hVenue = nullptr, hUrl = nullptr, hStyle = nullptr, hOut = nullptr;
    } st{};
    st.candidates = StudyAssistantTools::HarvestCitationCandidates(doc, flexcil);
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto rebuildCitation = [&]() {
                AcademicSourceInfo src{};
                src.authorsUtf8 = WideToUtf8(GetControlTextW(s->hAuthors));
                src.titleUtf8 = WideToUtf8(GetControlTextW(s->hTitle));
                src.yearUtf8 = WideToUtf8(GetControlTextW(s->hYear));
                src.venueOrPublisherUtf8 = WideToUtf8(GetControlTextW(s->hVenue));
                src.urlUtf8 = WideToUtf8(GetControlTextW(s->hUrl));
                const int stIdx = std::clamp<int>(static_cast<int>(SendMessageW(s->hStyle, CB_GETCURSEL, 0, 0)), 0, 2);
                s->formatted = StudyAssistantTools::FormatAcademicCitation(src, static_cast<CitationStyle>(stIdx), 1);
                std::string crlf;
                for (char ch : s->formatted) { if (ch == '\n') crlf += "\r\n"; else crlf.push_back(ch); }
                SetWindowTextW(s->hOut, Utf8ToWide(crlf).c_str());
            };

            auto loadCandidate = [&](int idx) {
                if (idx < 0 || static_cast<std::size_t>(idx) >= s->candidates.size()) return;
                const auto& c = s->candidates[static_cast<std::size_t>(idx)];
                SetWindowTextW(s->hAuthors, Utf8ToWide(c.authorsUtf8).c_str());
                SetWindowTextW(s->hTitle, Utf8ToWide(c.titleUtf8).c_str());
                SetWindowTextW(s->hYear, Utf8ToWide(c.yearUtf8).c_str());
                SetWindowTextW(s->hVenue, Utf8ToWide(c.venueOrPublisherUtf8).c_str());
                SetWindowTextW(s->hUrl, Utf8ToWide(c.urlUtf8).c_str());
                rebuildCitation();
            };

            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                auto lbl = [&](const wchar_t* t, int x, int y, int w, int h) {
                    HWND hw = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(h, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                };
                lbl(L"Fuente detectada:", 16, 12, 140, 18);
                s->hCand = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(16, s->dpi), Sc(32, s->dpi), Sc(360, s->dpi), Sc(150, s->dpi), hwnd, reinterpret_cast<HMENU>(941), nullptr, nullptr);
                SendMessageW(s->hCand, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                for (const auto& c : s->candidates) {
                    SendMessageW(s->hCand, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(c.titleUtf8).c_str()));
                }
                SendMessageW(s->hCand, CB_SETCURSEL, 0, 0);

                lbl(L"Formato:", 386, 12, 120, 18);
                s->hStyle = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(386, s->dpi), Sc(32, s->dpi), Sc(148, s->dpi), Sc(120, s->dpi), hwnd, reinterpret_cast<HMENU>(942), nullptr, nullptr);
                SendMessageW(s->hStyle, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(s->hStyle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"APA 7"));
                SendMessageW(s->hStyle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"IEEE"));
                SendMessageW(s->hStyle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"BibTeX"));
                SendMessageW(s->hStyle, CB_SETCURSEL, 0, 0);

                lbl(L"Autores:", 16, 66, 260, 18);
                s->hAuthors = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(86, s->dpi), Sc(380, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);
                lbl(L"A\u00F1o:", 406, 66, 100, 18);
                s->hYear = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(406, s->dpi), Sc(86, s->dpi), Sc(128, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);

                lbl(L"T\u00EDtulo:", 16, 116, 400, 18);
                s->hTitle = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(136, s->dpi), Sc(518, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);

                lbl(L"Editorial / Revista:", 16, 166, 250, 18);
                s->hVenue = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(186, s->dpi), Sc(250, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);
                lbl(L"URL / DOI:", 278, 166, 250, 18);
                s->hUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(278, s->dpi), Sc(186, s->dpi), Sc(256, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);

                for (HWND ed : { s->hAuthors, s->hYear, s->hTitle, s->hVenue, s->hUrl }) {
                    SendMessageW(ed, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                }

                s->hOut = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                    Sc(16, s->dpi), Sc(222, s->dpi), Sc(518, s->dpi), Sc(88, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hOut, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                auto mkBtn = [&](const wchar_t* t, int x, int w, int id, bool bold = false) {
                    HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (bold ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(x, s->dpi), Sc(320, s->dpi), Sc(w, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };
                mkBtn(L"Actualizar", 16, 98, 943);
                mkBtn(L"Copiar", 122, 92, 944);
                mkBtn(L"Insertar en nota", 296, 134, 945, true);
                mkBtn(L"Cerrar", 438, 96, IDCANCEL);
                loadCandidate(0);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) { RECT rc{}; GetClientRect(hwnd, &rc); FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg); return 1; }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                const int cid = LOWORD(wp);
                if (cid == 941 && HIWORD(wp) == CBN_SELCHANGE) { loadCandidate(static_cast<int>(SendMessageW(s->hCand, CB_GETCURSEL, 0, 0))); return 0; }
                if (cid == 942 && HIWORD(wp) == CBN_SELCHANGE) { rebuildCitation(); return 0; }
                if (cid == 943) { rebuildCitation(); return 0; }
                if (cid == 944) {
                    rebuildCitation();
                    if (OpenClipboard(hwnd)) {
                        EmptyClipboard();
                        const std::wstring w = Utf8ToWide(s->formatted);
                        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
                        if (hg) {
                            std::memcpy(GlobalLock(hg), w.c_str(), (w.size() + 1) * sizeof(wchar_t));
                            GlobalUnlock(hg);
                            SetClipboardData(CF_UNICODETEXT, hg);
                        }
                        CloseClipboard();
                    }
                    MessageBoxW(hwnd, L"Cita copiada al portapapeles.", L"Referencias Bibliogr\u00E1""ficas", MB_ICONINFORMATION);
                    return 0;
                }
                if (cid == 945) { rebuildCitation(); s->insertText = s->formatted; s->done = true; DestroyWindow(hwnd); return 0; }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUBibCiteDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 820, 560 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(566, st.dpi), h = Sc(402, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUBibCiteDlg", L"Generador de Citas Bibliogr\u00E1""ficas (APA 7 / IEEE / BibTeX)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outInsertText = st.insertText;
    return !outInsertText.empty();
}

// ============================================================================
// 16. Insert Interactive LinkCard Dialog (External HTTPS/Mailto or Internal Page)
// ============================================================================
struct LinkCardInput {
    std::string titleUtf8;
    std::string urlUtf8;
    std::string descriptionUtf8;
    int targetPageIndex0 = -1;
};

inline bool ShowInsertLinkCardDialog(
    HWND owner,
    std::size_t totalPages,
    LinkCardInput& ioLink)
{
    struct St {
        LinkCardInput data{};
        std::size_t maxPages = 1;
        bool accepted = false, done = false;
        UINT dpi = 96;
        UiGlass::HigDialogTheme theme{};
        HFONT fNorm = nullptr, fBold = nullptr, fSmall = nullptr;
        HWND hTitle = nullptr, hUrl = nullptr, hDesc = nullptr, hPage = nullptr, hHint = nullptr;
    } st{};
    st.data = ioLink;
    st.maxPages = (std::max<std::size_t>)(1u, totalPages);
    st.dpi = DpiOf(owner);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            if (msg == WM_CREATE) {
                s->theme.Init(hwnd);
                s->fNorm = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_NORMAL);
                s->fBold = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_SEMIBOLD);
                s->fSmall = UiGlass::CreateHigUiFont(Sc(11, s->dpi), FW_NORMAL);
                const int pad = Sc(16, s->dpi);
                const int fw = Sc(460, s->dpi);
                int y = Sc(14, s->dpi);

                auto addLbl = [&](const std::wstring& t) {
                    HWND h = CreateWindowExW(0, L"STATIC", t.c_str(), WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                    y += Sc(20, s->dpi);
                };
                addLbl(L"T\u00EDtulo del enlace:");
                s->hTitle = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->data.titleUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(701), nullptr, nullptr);
                SendMessageW(s->hTitle, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);

                addLbl(L"Direcci\u00F3n URL (https://, http:// o mailto:):");
                s->hUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->data.urlUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(702), nullptr, nullptr);
                SendMessageW(s->hUrl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);

                addLbl(L"O salto interno a p\u00E1gina del cuaderno (opcional, 1.." + std::to_wstring(s->maxPages) + L", vac\u00EDo = URL):");
                s->hPage = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                    (s->data.targetPageIndex0 >= 0 ? std::to_wstring(s->data.targetPageIndex0 + 1) : L"").c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, Sc(120, s->dpi), Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(703), nullptr, nullptr);
                SendMessageW(s->hPage, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);

                addLbl(L"Descripci\u00F3n breve (opcional):");
                s->hDesc = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->data.descriptionUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(704), nullptr, nullptr);
                SendMessageW(s->hDesc, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(32, s->dpi);

                s->hHint = CreateWindowExW(0, L"STATIC", L"Solo se permiten esquemas seguros (https://, http://, mailto:) o saltos internos.",
                    WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hHint, WM_SETFONT, reinterpret_cast<WPARAM>(s->fSmall), TRUE);
                y += Sc(26, s->dpi);

                HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    pad + fw - Sc(206, s->dpi), y, Sc(98, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                HWND hOk = CreateWindowExW(0, L"BUTTON", L"Insertar enlace", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    pad + fw - Sc(102, s->dpi), y, Sc(102, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(hCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                s->theme.ThemeAllChildren(hwnd);
                SetFocus(s->hUrl);
                return 0;
            }
            LRESULT tr = 0;
            if (s->theme.HandleMessage(hwnd, msg, wp, lp, tr)) return tr;
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == IDOK) {
                    s->data.titleUtf8 = WideToUtf8(GetControlTextW(s->hTitle));
                    s->data.urlUtf8 = WideToUtf8(GetControlTextW(s->hUrl));
                    s->data.descriptionUtf8 = WideToUtf8(GetControlTextW(s->hDesc));
                    const std::wstring pgW = GetControlTextW(s->hPage);
                    s->data.targetPageIndex0 = -1;
                    if (!pgW.empty()) {
                        const int p1 = _wtoi(pgW.c_str());
                        if (p1 >= 1 && static_cast<std::size_t>(p1) <= s->maxPages) {
                            s->data.targetPageIndex0 = p1 - 1;
                        }
                    }
                    if (s->data.targetPageIndex0 < 0 && !IsSafeExternalLinkUrl(s->data.urlUtf8)) {
                        if (s->hHint) SetWindowTextW(s->hHint, L"URL no v\u00E1lida: use https://, http://, mailto: o indique una p\u00E1gina interna.");
                        return 0;
                    }
                    if (s->data.titleUtf8.empty()) {
                        s->data.titleUtf8 = (s->data.targetPageIndex0 >= 0)
                            ? ("Ir a p\xC3\xA1gina " + std::to_string(s->data.targetPageIndex0 + 1))
                            : ExtractUrlDomainUtf8(s->data.urlUtf8);
                    }
                    s->accepted = true;
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->fSmall) DeleteObject(s->fSmall);
                s->theme.Destroy();
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUInsertLinkDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 820, 560 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(508, st.dpi), h = Sc(334, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUInsertLinkDlg", L"Insertar Enlace Interactivo",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (st.accepted) { ioLink = st.data; return true; }
    return false;
}

// ============================================================================
// 17. Insert Animated / Rendered HTML Block Dialog (Lazy WebView2 + Sandbox)
// ============================================================================
struct HtmlEmbedInput {
    std::string titleUtf8 = "Onda Arm\xC3\xB3nica Animada";
    std::string htmlSourceUtf8;
    std::string urlUtf8;
    bool allowNetwork = false;
};

inline std::string DefaultAnimatedWaveHtmlTemplate() {
    return
        "<!DOCTYPE html><html><head><meta charset='utf-8'><style>"
        "body{margin:0;background:#0f172a;color:#e2e8f0;font-family:'Segoe UI',sans-serif;display:flex;flex-direction:column;height:100vh;overflow:hidden}"
        ".bar{padding:8px 12px;font-size:12px;display:flex;justify-content:space-between;background:#1e293b}"
        "canvas{flex:1;width:100%;height:100%}"
        "</style></head><body>"
        "<div class='bar'><span>Onda senoidal y = A sen(kx - \xCF\x89t)</span><span id='val'>A=1.0</span></div>"
        "<canvas id='c'></canvas><script>"
        "const c=document.getElementById('c'),x=c.getContext('2d');let t=0;"
        "function r(){c.width=c.clientWidth;c.height=c.clientHeight;}r();addEventListener('resize',r);"
        "function loop(){x.clearRect(0,0,c.width,c.height);x.strokeStyle='#38bdf8';x.lineWidth=2.5;x.beginPath();"
        "for(let i=0;i<c.width;i++){const y=c.height/2+Math.sin(i*0.03-t)*c.height*0.32;if(i===0)x.moveTo(i,y);else x.lineTo(i,y);}"
        "x.stroke();t+=0.06;requestAnimationFrame(loop);}loop();"
        "</script></body></html>";
}

inline bool ShowInsertHtmlEmbedDialog(HWND owner, HtmlEmbedInput& ioHtml) {
    struct St {
        HtmlEmbedInput data{};
        bool accepted = false, done = false;
        UINT dpi = 96;
        UiGlass::HigDialogTheme theme{};
        HFONT fNorm = nullptr, fBold = nullptr, fMono = nullptr;
        HWND hTitle = nullptr, hUrl = nullptr, hCode = nullptr, hNet = nullptr;
    } st{};
    st.data = ioHtml;
    if (st.data.htmlSourceUtf8.empty() && st.data.urlUtf8.empty()) {
        st.data.htmlSourceUtf8 = DefaultAnimatedWaveHtmlTemplate();
    }
    st.dpi = DpiOf(owner);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            if (msg == WM_CREATE) {
                s->theme.Init(hwnd);
                s->fNorm = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_NORMAL);
                s->fBold = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_SEMIBOLD);
                s->fMono = CreateFontW(-Sc(12, s->dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
                const int pad = Sc(16, s->dpi);
                const int fw = Sc(560, s->dpi);
                int y = Sc(12, s->dpi);

                auto addLbl = [&](const std::wstring& t) {
                    HWND h = CreateWindowExW(0, L"STATIC", t.c_str(), WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                    y += Sc(20, s->dpi);
                };
                addLbl(L"T\u00EDtulo del bloque HTML:");
                s->hTitle = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->data.titleUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(721), nullptr, nullptr);
                SendMessageW(s->hTitle, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);

                addLbl(L"Archivo .html local o URL externa (opcional; si est\u00E1 vac\u00EDo usa el HTML inferior):");
                s->hUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->data.urlUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, fw - Sc(104, s->dpi), Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(722), nullptr, nullptr);
                SendMessageW(s->hUrl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                HWND hBrowse = CreateWindowExW(0, L"BUTTON", L"Archivo .html", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    pad + fw - Sc(98, s->dpi), y, Sc(98, s->dpi), Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(725), nullptr, nullptr);
                SendMessageW(hBrowse, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);

                addLbl(L"C\u00F3digo HTML / CSS / JS animado (sandboxed offline por defecto):");
                s->hCode = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->data.htmlSourceUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                    pad, y, fw, Sc(168, s->dpi), hwnd, reinterpret_cast<HMENU>(723), nullptr, nullptr);
                SendMessageW(s->hCode, WM_SETFONT, reinterpret_cast<WPARAM>(s->fMono ? s->fMono : s->fNorm), TRUE);
                y += Sc(176, s->dpi);

                s->hNet = CreateWindowExW(0, L"BUTTON", L"Permitir acceso a red externa (solo URLs de confianza; desactivado = 100% offline)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, pad, y, fw, Sc(22, s->dpi), hwnd, reinterpret_cast<HMENU>(724), nullptr, nullptr);
                SendMessageW(s->hNet, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hNet, BM_SETCHECK, s->data.allowNetwork ? BST_CHECKED : BST_UNCHECKED, 0);
                y += Sc(30, s->dpi);

                HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    pad + fw - Sc(224, s->dpi), y, Sc(100, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                HWND hOk = CreateWindowExW(0, L"BUTTON", L"Insertar HTML", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    pad + fw - Sc(118, s->dpi), y, Sc(118, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(hCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                s->theme.ThemeAllChildren(hwnd);
                SetFocus(s->hTitle);
                return 0;
            }
            LRESULT tr = 0;
            if (s->theme.HandleMessage(hwnd, msg, wp, lp, tr)) return tr;
            if (msg == WM_COMMAND) {
                const WORD cid = LOWORD(wp);
                if (cid == 725) {
                    std::wstring fp;
                    if (PickOpenFileWin32(hwnd, L"HTML (*.html;*.htm;*.svg)\0*.html;*.htm;*.svg\0\0", L"Seleccionar archivo HTML", fp)) {
                        SetWindowTextW(s->hUrl, fp.c_str());
                        SetWindowTextW(s->hCode, L"");
                    }
                    return 0;
                }
                if (cid == IDOK) {
                    s->data.titleUtf8 = WideToUtf8(GetControlTextW(s->hTitle));
                    s->data.urlUtf8 = WideToUtf8(GetControlTextW(s->hUrl));
                    s->data.htmlSourceUtf8 = WideToUtf8(GetControlTextW(s->hCode));
                    s->data.allowNetwork = (SendMessageW(s->hNet, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (s->data.titleUtf8.empty()) s->data.titleUtf8 = "HTML Interactivo";
                    if (!s->data.urlUtf8.empty() && IsSafeExternalLinkUrl(s->data.urlUtf8)) {
                        s->data.allowNetwork = true; // User explicitly provided an external https:// URL
                    }
                    s->accepted = true;
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->fMono) DeleteObject(s->fMono);
                s->theme.Destroy();
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUInsertHtmlDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 100, 80, 920, 680 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(608, st.dpi), h = Sc(432, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUInsertHtmlDlg", L"Insertar Bloque HTML / CSS / JS Animado",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (st.accepted) { ioHtml = st.data; return true; }
    return false;
}

} // namespace DocStudio::ClosureDialogs
#endif
