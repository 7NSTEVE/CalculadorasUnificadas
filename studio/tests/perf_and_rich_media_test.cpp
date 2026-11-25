#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <objbase.h>
#include <objidl.h>
#include <roapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/studio_profile_store.hpp"
#include "../media/native_video_engine.hpp"
#include "../media/webview2_html_host.hpp"
#include "../render/d2d_renderer.hpp"
#include "../../ui_glass/translucent_tokens.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace DocStudio;

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* msg) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "[FAIL] " << msg << "\n";
    }
}

std::string ReadWholeFile(const std::filesystem::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) return {};
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

bool HasNonUniformPixels(const RasterImageBgra32& img) {
    if (!img.Valid() || img.pixels.size() < 16) return false;
    const std::uint8_t b0 = img.pixels[0], g0 = img.pixels[1], r0 = img.pixels[2];
    int diffCount = 0;
    for (std::size_t i = 0; i + 3 < img.pixels.size(); i += 16) {
        if (std::abs(static_cast<int>(img.pixels[i]) - static_cast<int>(b0)) > 12 ||
            std::abs(static_cast<int>(img.pixels[i + 1]) - static_cast<int>(g0)) > 12 ||
            std::abs(static_cast<int>(img.pixels[i + 2]) - static_cast<int>(r0)) > 12) {
            if (++diffCount > 40) return true;
        }
    }
    return false;
}

double RelativeLuminance(UiGlass::Rgba8 c) {
    auto lin = [](std::uint8_t u) -> double {
        const double s = static_cast<double>(u) / 255.0;
        return (s <= 0.03928) ? (s / 12.92) : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}

double ContrastRatio(UiGlass::Rgba8 a, UiGlass::Rgba8 b) {
    const double la = RelativeLuminance(a);
    const double lb = RelativeLuminance(b);
    const double hi = (std::max)(la, lb);
    const double lo = (std::min)(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

std::vector<std::uint8_t> EncodeTwoFrameTestGif89a(UINT16 delayCs0, UINT16 delayCs1) {
    std::vector<std::uint8_t> out;
    Microsoft::WRL::ComPtr<IWICImagingFactory> fac;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac)))) return out;
    Microsoft::WRL::ComPtr<IStream> strm;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &strm))) return out;
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> enc;
    if (FAILED(fac->CreateEncoder(GUID_ContainerFormatGif, nullptr, &enc))) return out;
    if (FAILED(enc->Initialize(strm.Get(), WICBitmapEncoderNoCache))) return out;

    const UINT w = 48, h = 48;
    const UINT16 delays[2] = { delayCs0, delayCs1 };
    for (int f = 0; f < 2; ++f) {
        std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h * 4u);
        for (std::size_t i = 0; i < px.size(); i += 4) {
            px[i + 0] = (f == 0) ? 240 : 40;
            px[i + 1] = (f == 0) ? 80  : 220;
            px[i + 2] = (f == 0) ? 20  : 30;
            px[i + 3] = 255;
        }
        Microsoft::WRL::ComPtr<IWICBitmap> srcBmp;
        if (FAILED(fac->CreateBitmapFromMemory(w, h, GUID_WICPixelFormat32bppBGRA, w * 4u, static_cast<UINT>(px.size()), px.data(), &srcBmp))) return out;
        Microsoft::WRL::ComPtr<IWICPalette> pal;
        if (FAILED(fac->CreatePalette(&pal))) return out;
        if (FAILED(pal->InitializeFromBitmap(srcBmp.Get(), 256, FALSE))) return out;
        Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
        if (FAILED(fac->CreateFormatConverter(&conv))) return out;
        if (FAILED(conv->Initialize(srcBmp.Get(), GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeNone, pal.Get(), 0.0, WICBitmapPaletteTypeCustom))) return out;

        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        if (FAILED(enc->CreateNewFrame(&frame, nullptr))) return out;
        if (FAILED(frame->Initialize(nullptr))) return out;
        frame->SetSize(w, h);
        WICPixelFormatGUID pf = GUID_WICPixelFormat8bppIndexed;
        frame->SetPixelFormat(&pf);
        frame->SetPalette(pal.Get());
        Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> mw;
        if (SUCCEEDED(frame->GetMetadataQueryWriter(&mw)) && mw) {
            PROPVARIANT pv{};
            PropVariantInit(&pv);
            pv.vt = VT_UI2;
            pv.uiVal = delays[f];
            mw->SetMetadataByName(L"/grctlext/Delay", &pv);
            PropVariantClear(&pv);
        }
        if (FAILED(frame->WriteSource(conv.Get(), nullptr))) return out;
        if (FAILED(frame->Commit())) return out;
    }
    if (FAILED(enc->Commit())) return out;
    HGLOBAL hMem = nullptr;
    if (SUCCEEDED(GetHGlobalFromStream(strm.Get(), &hMem)) && hMem) {
        const SIZE_T sz = GlobalSize(hMem);
        if (const void* ptr = GlobalLock(hMem)) {
            out.assign(static_cast<const std::uint8_t*>(ptr), static_cast<const std::uint8_t*>(ptr) + sz);
            GlobalUnlock(hMem);
        }
    }
    return out;
}

bool ContainsUtf8FourByteEmoji(const std::string& s) {
    for (std::size_t i = 0; i + 3 < s.size(); ++i) {
        const unsigned char b0 = static_cast<unsigned char>(s[i]);
        const unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
        if (b0 == 0xF0 && b1 == 0x9F) return true; // U+1F000..U+1FFFF emojis
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RoInitialize(RO_INIT_SINGLETHREADED);

    const std::filesystem::path srcRoot = (argc > 1)
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();
    const std::filesystem::path tmpDir = std::filesystem::temp_directory_path() / L"CuadernU_PerfMediaTest";
    std::error_code ec;
    std::filesystem::create_directories(tmpDir, ec);

    // =========================================================================
    // 1. ZERO-GPU RENDERER & MEMORY/TIMER EFFICIENCY CONTRACTS
    // =========================================================================
    {
        D2DRenderer ren;
        Check(ren.InitializeFactories(), "D2DRenderer factories initialize cleanly");
        Check(!ren.IsHardwareAccelerationEnabled(), "D2DRenderer defaults to Software mode (0% GPU)");
        ren.SetHardwareAcceleration(true);
        Check(ren.IsHardwareAccelerationEnabled(), "D2DRenderer optional HW toggle activates");
        ren.SetHardwareAcceleration(false);
        Check(!ren.IsHardwareAccelerationEnabled(), "D2DRenderer optional HW toggle returns to Software 0% GPU");
        ren.TrimCaches(true);
        Check(ren.CachedBitmapCount() == 0, "TrimCaches(true) drops cached bitmaps to 0");

        // Direct runtime check of PageRenderCache default <= 64 MiB cap
        {
            PageRenderCache defaultCache;
            Check(defaultCache.MaxBytes() <= 64ull * 1024ull * 1024ull, "PageRenderCache default budget is <= 64 MiB");
            defaultCache.Shutdown();
        }

        // StudioProfileStore persistence of hardwareGpuAcceleration
        StudioUiPreferences prefs{};
        Check(!prefs.hardwareGpuAcceleration, "StudioUiPreferences defaults hardwareGpuAcceleration=false");
        prefs.hardwareGpuAcceleration = true;
        const std::filesystem::path profDir = tmpDir / L"profile_gpu_test";
        std::filesystem::create_directories(profDir, ec);
        const std::wstring prefsFile = (profDir / L"ui_settings.json").wstring();
        Check(StudioProfileStore::SaveUiPreferences(prefsFile, prefs), "SaveUiPreferences succeeds");
        StudioUiPreferences loaded{};
        Check(StudioProfileStore::LoadUiPreferences(prefsFile, loaded), "LoadUiPreferences succeeds");
        Check(loaded.hardwareGpuAcceleration == true, "hardwareGpuAcceleration persists cleanly");

        // Source-level Zero-Idle & Zero-GPU architectural verification
        const std::string mainSrc = ReadWholeFile(srcRoot / "app" / "main_studio.cpp");
        Check(!mainSrc.empty(), "Read main_studio.cpp for architectural contract checks");
        Check(mainSrc.find("SetTimer(hwnd, 702") == std::string::npos, "No permanent 450ms timer 702 in main_studio.cpp");
        Check(mainSrc.find("DrawTranslucentCapsuleOnHdc") == std::string::npos, "No heavy CPU Dual Kawase blur in main_studio.cpp");
        Check(mainSrc.find("TrackPopupMenu(") == std::string::npos, "No unthemed Win32 TrackPopupMenu in main_studio.cpp");
        Check(mainSrc.find("d3d11_translucent") == std::string::npos, "No D3D11 material translúcido shaders referenced in main_studio.cpp");
        Check(mainSrc.find("FindFirstChangeNotificationW") != std::string::npos, "Sleeping Win32 FindFirstChangeNotificationW watcher present");
        Check(mainSrc.find("ApplyWindowsNativeMicaBackdrop") != std::string::npos, "Windows 11 DWM Mica native backdrop applied");
        Check(mainSrc.find("SIZE_MINIMIZED") != std::string::npos, "WM_SIZE SIZE_MINIMIZED pause & RAM trim implemented");
        Check(mainSrc.find("SetProcessWorkingSetSizeEx") != std::string::npos, "Working set trim API wired");
        Check(mainSrc.find("64ull * 1024ull * 1024ull") != std::string::npos, "PageRenderCache lean 64 MiB budget configured");
        Check(!std::filesystem::exists(srcRoot / "render" / "d3d11_translucent.cpp"), "d3d11_translucent.cpp removed");
    }

    // =========================================================================
    // 2. LINKCARD SECURITY + PERSISTENCE + UNDO/REDO + RENDER
    // =========================================================================
    {
        Check(IsSafeExternalLinkUrl("https://www.desmos.com/calculator"), "Accepts https:// URL");
        Check(IsSafeExternalLinkUrl("http://example.org/physics"), "Accepts http:// URL");
        Check(IsSafeExternalLinkUrl("mailto:docente@tec.ac.cr"), "Accepts mailto: URL");
        Check(!IsSafeExternalLinkUrl("javascript:alert(1)"), "Blocks javascript: scheme");
        Check(!IsSafeExternalLinkUrl("file:///C:/Windows/System32/cmd.exe"), "Blocks file:// scheme");
        Check(!IsSafeExternalLinkUrl("vbscript:msgbox(1)"), "Blocks vbscript: scheme");
        Check(!IsSafeExternalLinkUrl("cmd:/c calc.exe"), "Blocks cmd: scheme");
        Check(!IsSafeExternalLinkUrl("data:text/html,<script>alert(1)</script>"), "Blocks data: scheme");
        Check(!IsSafeExternalLinkUrl(""), "Blocks empty URL");
        Check(ExtractUrlDomainUtf8("https://www.desmos.com/calculator?lang=es") == "desmos.com", "Extracts clean URL domain");

        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack stack;
        AnnotationObject lk{};
        lk.kind = ObjectKind::LinkCard;
        lk.bounds = { 24.0, 24.0, 320.0, 86.0 };
        lk.linkTitleUtf8 = "Simulador Desmos";
        lk.linkUrlUtf8 = "https://www.desmos.com/calculator";
        lk.linkDescriptionUtf8 = "Funciones interactivas";
        lk.linkTargetPageIndex = -1;
        Check(stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, lk)), "Add LinkCard via CommandStack");
        Check(doc.CanvasObjects().size() == 1, "Canvas holds LinkCard");
        Check(stack.Undo(doc) && doc.CanvasObjects().empty(), "Undo removes LinkCard");
        Check(stack.Redo(doc) && doc.CanvasObjects().size() == 1, "Redo restores LinkCard");

        // Internal page link
        AnnotationObject lkInt{};
        lkInt.kind = ObjectKind::LinkCard;
        lkInt.bounds = { 24.0, 126.0, 320.0, 74.0 };
        lkInt.linkTitleUtf8 = "Saltar a Resumen";
        lkInt.linkTargetPageIndex = 2;
        doc.AddCanvasObject(lkInt);

        std::vector<std::uint8_t> bytes;
        Check(CudocStorage::SerializeToBytes(doc, bytes), "Serialize LinkCard .cudoc");
        DocumentModel reloaded;
        Check(CudocStorage::DeserializeFromBytes(bytes.data(), bytes.size(), reloaded), "Deserialize LinkCard .cudoc");
        Check(reloaded.CanvasObjects().size() == 2, "Both LinkCards restored");
        Check(reloaded.CanvasObjects()[0].kind == ObjectKind::LinkCard &&
              reloaded.CanvasObjects()[0].linkUrlUtf8 == "https://www.desmos.com/calculator" &&
              reloaded.CanvasObjects()[0].linkTitleUtf8 == "Simulador Desmos",
              "External LinkCard fields roundtrip cleanly");
        Check(reloaded.CanvasObjects()[1].linkTargetPageIndex == 2, "Internal page LinkCard target roundtrips");

        D2DRenderer ren;
        RasterImageBgra32 img;
        Camera2D cam{};
        cam.zoom = 1.0; cam.panX = 0.0; cam.panY = 0.0;
        Check(ren.RenderHwndViewportToOffscreen(reloaded, cam, 400, 240, nullptr, img), "Render LinkCard offscreen");
        Check(HasNonUniformPixels(img), "Rendered LinkCard has non-blank visual content");
    }

    // =========================================================================
    // 3. GIF89a REAL WIC /grctlext/Delay + FRAME DIFF + PAUSE + OFFSCREEN CULL
    // =========================================================================
    {
        const auto gifBytes = EncodeTwoFrameTestGif89a(12, 25); // 120ms, 250ms
        Check(gifBytes.size() > 64 && std::memcmp(gifBytes.data(), "GIF8", 4) == 0, "Encoded valid 2-frame GIF89a");

        int fCnt = 0;
        std::vector<int> delays;
        Check(D2DRenderer::InspectGifMetadataWic(gifBytes, fCnt, delays), "InspectGifMetadataWic succeeds");
        Check(fCnt == 2 && delays.size() == 2, "InspectGifMetadataWic detects 2 frames");
        Check(delays.size() >= 2 && delays[0] == 120 && delays[1] == 250, "InspectGifMetadataWic reads exact /grctlext/Delay (120ms, 250ms)");

        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject gifObj{};
        gifObj.kind = ObjectKind::Image;
        gifObj.bounds = { 20.0, 20.0, 120.0, 120.0 };
        gifObj.pngBytes = gifBytes;
        gifObj.imagePixelW = 48;
        gifObj.imagePixelH = 48;
        gifObj.gifFrameCount = fCnt;
        gifObj.gifAvgDelayMs = 185;
        doc.AddCanvasObject(gifObj);

        D2DRenderer ren;
        Camera2D cam{};
        cam.zoom = 1.0; cam.panX = 0.0; cam.panY = 0.0;
        ViewportOverlayState ov0{}, ov1{};
        ov0.animationClockMs = 20;  // Inside Frame 0 (0..120ms -> blue)
        ov1.animationClockMs = 200; // Inside Frame 1 (120..370ms -> green)
        RasterImageBgra32 img0, img1;
        Check(ren.RenderHwndViewportToOffscreen(doc, cam, 180, 180, nullptr, img0, &ov0), "Render GIF frame 0");
        Check(ren.LastFrameHadVisibleAnimatedGif(), "Visible active GIF signals dynamic timer ON");
        Check(ren.LastFrameMinGifDelayMs() >= 20 && ren.LastFrameMinGifDelayMs() <= 250, "Min GIF delay reported cleanly");
        Check(ren.RenderHwndViewportToOffscreen(doc, cam, 180, 180, nullptr, img1, &ov1), "Render GIF frame 1");
        Check(img0.pixels != img1.pixels, "Frame 0 and Frame 1 pixels differ based on real GIF delay clock");

        // Offscreen frustum culling pauses GIF automatically
        Camera2D farCam{};
        farCam.zoom = 1.0; farCam.panX = 5000.0; farCam.panY = 5000.0;
        RasterImageBgra32 imgFar;
        Check(ren.RenderHwndViewportToOffscreen(doc, farCam, 180, 180, nullptr, imgFar, &ov0), "Render scrolled offscreen");
        Check(!ren.LastFrameHadVisibleAnimatedGif(), "Offscreen GIF pauses dynamic timer automatically");

        // User-paused GIF pauses dynamic timer automatically
        if (auto* mut = doc.FindCanvasObjectMutable(doc.CanvasObjects()[0].id)) mut->gifPaused = true;
        RasterImageBgra32 imgPaused;
        Check(ren.RenderHwndViewportToOffscreen(doc, cam, 180, 180, nullptr, imgPaused, &ov1), "Render paused GIF");
        Check(!ren.LastFrameHadVisibleAnimatedGif(), "Paused GIF signals dynamic timer OFF (0% CPU)");
    }

    // =========================================================================
    // 4. NATIVE WINDOWS MEDIA FOUNDATION VIDEO ENGINE + .cudoc + RENDER
    // =========================================================================
    {
        const std::wstring mp4Path = (tmpDir / L"mf_synth_test.mp4").wstring();
        std::string vErr;
        Check(WriteSyntheticTestVideoFile(mp4Path, 320, 180, 15, 20, &vErr), "WriteSyntheticTestVideoFile creates real MP4 via Media Foundation");

        RasterImageBgra32 posterBgra;
        std::vector<std::uint8_t> posterPng;
        std::uint64_t durMs = 0;
        int vw = 0, vh = 0;
        Check(ExtractVideoPosterAndMetadata(mp4Path, posterBgra, posterPng, durMs, vw, vh, &vErr),
              "ExtractVideoPosterAndMetadata extracts poster + duration");
        Check(posterBgra.Valid() && vw == 320 && vh == 180, "Video dimensions 320x180 verified");
        Check(posterPng.size() > 64 && posterPng[0] == 0x89 && posterPng[1] == 'P' && posterPng[2] == 'N' && posterPng[3] == 'G',
              "Poster encoded as valid PNG");
        Check(durMs >= 500, "Video duration metadata > 0 ms");

        // Real playback decode & pause resource release
        NativeVideoPlayer player;
        Check(!player.IsPlaying(), "NativeVideoPlayer starts idle");
        Check(player.Start(42, mp4Path, 0, 1.0, true, false, &vErr), "NativeVideoPlayer::Start succeeds");
        Check(player.IsPlaying() && player.ActiveObjectId() == 42, "NativeVideoPlayer playing active object #42");
        bool gotFrame = false;
        for (int tries = 0; tries < 10 && !gotFrame; ++tries) {
            gotFrame = player.TickAndDecodeFrame() || player.CurrentFrameBgra().Valid();
            if (!gotFrame) Sleep(30);
        }
        Check(gotFrame && player.CurrentFrameBgra().Valid(), "NativeVideoPlayer decodes live BGRA frame via Media Foundation");
        Check(player.SeekMs(500), "NativeVideoPlayer::SeekMs seeks cleanly to 500ms");
        Check(player.CurrentPositionMs() == 500, "NativeVideoPlayer reports seeked position");
        bool gotSeekFrame = false;
        for (int tries = 0; tries < 6 && !gotSeekFrame; ++tries) {
            gotSeekFrame = player.TickAndDecodeFrame();
            if (!gotSeekFrame) Sleep(20);
        }
        Check(gotSeekFrame, "NativeVideoPlayer decodes frame after SeekMs");
        std::vector<std::uint8_t> capPng;
        Check(player.CaptureCurrentFramePng(capPng) && capPng.size() > 64, "CaptureCurrentFramePng produces valid PNG");
        player.Pause();
        Check(!player.IsPlaying(), "NativeVideoPlayer::Pause stops & releases decoder (0% CPU/GPU)");

        // .cudoc persistence (including embedded <= 16 MiB videoAssetKey) + offscreen render
        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);
        std::vector<std::uint8_t> mp4RawBytes;
        {
            std::ifstream vifs(mp4Path, std::ios::binary);
            if (vifs.is_open()) {
                mp4RawBytes.assign(std::istreambuf_iterator<char>(vifs), std::istreambuf_iterator<char>());
            }
        }
        const std::string embeddedVidKey = doc.RegisterAsset(mp4RawBytes);
        Check(!embeddedVidKey.empty() && mp4RawBytes.size() > 128, "RegisterAsset embeds <= 16 MiB video cleanly");
        AnnotationObject vid{};
        vid.kind = ObjectKind::VideoClip;
        vid.bounds = { 20.0, 20.0, 320.0, 180.0 };
        vid.captionUtf8 = "Demo Video Nativo";
        vid.videoSourcePathUtf8 = "mf_synth_test.mp4";
        vid.videoAssetKey = embeddedVidKey;
        vid.videoDurationMs = durMs;
        vid.videoPosterPngBytes = posterPng;
        vid.videoMuted = true;
        vid.videoPlaybackRate = 1.5;
        vid.videoLoop = true;
        doc.AddCanvasObject(vid);

        std::vector<std::uint8_t> bytes;
        Check(CudocStorage::SerializeToBytes(doc, bytes), "Serialize VideoClip .cudoc");
        DocumentModel reloaded;
        Check(CudocStorage::DeserializeFromBytes(bytes.data(), bytes.size(), reloaded), "Deserialize VideoClip .cudoc");
        Check(reloaded.CanvasObjects().size() == 1 && reloaded.CanvasObjects()[0].kind == ObjectKind::VideoClip, "VideoClip kind restored");
        const auto* restoredAsset = reloaded.FindAsset(reloaded.CanvasObjects()[0].videoAssetKey);
        Check(restoredAsset != nullptr && restoredAsset->size() == mp4RawBytes.size(),
              "Embedded videoAssetKey bytes roundtrip losslessly in .cudoc");
        Check(reloaded.CanvasObjects()[0].videoDurationMs == durMs &&
              reloaded.CanvasObjects()[0].videoPosterPngBytes.size() == posterPng.size() &&
              reloaded.CanvasObjects()[0].videoLoop == true &&
              std::abs(reloaded.CanvasObjects()[0].videoPlaybackRate - 1.5) < 1e-6,
              "VideoClip metadata & poster PNG roundtrip cleanly");

        D2DRenderer ren;
        RasterImageBgra32 img;
        Camera2D cam{};
        Check(ren.RenderHwndViewportToOffscreen(reloaded, cam, 380, 240, nullptr, img), "Render VideoClip poster offscreen");
        Check(HasNonUniformPixels(img), "Rendered VideoClip poster & controls non-blank");
    }

    // =========================================================================
    // 5. LAZY WEBVIEW2 HTML EMBED (ZERO STARTUP + SNAPSHOT + TRYSUSPEND + .cudoc)
    // =========================================================================
    {
        WebView2HtmlHost host;
        Check(!host.IsEnvironmentInitialized() && host.ActiveBlockCount() == 0,
              "WebView2HtmlHost has ZERO startup initialization cost");
        std::string wv2Ver;
        const bool hasRuntime = WebView2HtmlHost::IsWebView2RuntimeInstalled(&wv2Ver);
        Check(hasRuntime && !wv2Ver.empty(), "Microsoft Edge WebView2 Runtime detected on Windows");

        std::vector<std::uint8_t> capturedSnapPng;
        if (hasRuntime) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"CuadernU_Wv2TestHostWnd";
            RegisterClassExW(&wc);
            HWND hTestWnd = CreateWindowExW(0, wc.lpszClassName, L"WV2Test", WS_POPUP,
                -2000, -2000, 320, 220, nullptr, nullptr, wc.hInstance, nullptr);
            if (hTestWnd) {
                bool readyFlag = false;
                bool snapDone = false;
                const std::string testHtml =
                    "<!DOCTYPE html><html><body style='margin:0;background:#1d4ed8;color:#fff;font-family:Segoe UI;padding:18px'>"
                    "<h2 id='st'>CuadernU HTML5 Animado</h2><svg width='120' height='40'><circle cx='30' cy='20' r='14' fill='#38bdf8'/></svg>"
                    "<script>fetch('https://example.invalid/blocked').then(r=>{if(r.status===403)document.body.style.background='#065f46';}).catch(()=>{document.body.style.background='#065f46';});</script>"
                    "</body></html>";
                RECT bnd{ 0, 0, 320, 220 };
                const bool actOk = host.ActivateBlock(hTestWnd, 99, bnd, testHtml, "", false,
                    [&](std::uint64_t, bool ok) { readyFlag = ok; });
                Check(actOk, "WebView2HtmlHost::ActivateBlock launches lazily");

                auto pumpUntil = [](auto pred, ULONGLONG maxMs) {
                    const ULONGLONG t0 = GetTickCount64();
                    MSG m{};
                    while (!pred() && (GetTickCount64() - t0) < maxMs) {
                        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
                            TranslateMessage(&m);
                            DispatchMessageW(&m);
                        }
                        Sleep(15);
                    }
                };

                pumpUntil([&]() { return readyFlag; }, 9000);
                Check(readyFlag && host.IsEnvironmentInitialized() && host.IsBlockActive(99),
                      "WebView2 block ready in offline sandbox");
                if (readyFlag) {
                    // Brief render dwell so WebView2 paints at least 1 frame before CapturePreview
                    pumpUntil([]() { return false; }, 400);
                    host.CaptureSnapshotAndSuspend(99, [&](std::uint64_t, std::vector<std::uint8_t> png) {
                        capturedSnapPng = std::move(png);
                        snapDone = true;
                    });
                    pumpUntil([&]() { return snapDone; }, 6000);
                    pumpUntil([&]() { return host.IsBlockSuspended(99); }, 3000);
                    Check(snapDone && capturedSnapPng.size() > 256 &&
                          capturedSnapPng[0] == 0x89 && capturedSnapPng[1] == 'P',
                          "WebView2 CapturePreview produces valid PNG snapshot");
                    Check(host.IsBlockSuspended(99), "ICoreWebView2_3::TrySuspend suspends WebView2 (~0% idle CPU)");
                }
                host.Shutdown();
                DestroyWindow(hTestWnd);
            }
        }

        // .cudoc persistence + offscreen render of HtmlEmbed
        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject html{};
        html.kind = ObjectKind::HtmlEmbed;
        html.bounds = { 20.0, 20.0, 360.0, 220.0 };
        html.htmlTitleUtf8 = "Onda HTML5";
        html.htmlSourceUtf8 = "<html><body><h1>Onda</h1></body></html>";
        html.htmlAllowNetwork = false;
        html.htmlSnapshotPngBytes = capturedSnapPng;
        doc.AddCanvasObject(html);

        std::vector<std::uint8_t> bytes;
        Check(CudocStorage::SerializeToBytes(doc, bytes), "Serialize HtmlEmbed .cudoc");
        DocumentModel reloaded;
        Check(CudocStorage::DeserializeFromBytes(bytes.data(), bytes.size(), reloaded), "Deserialize HtmlEmbed .cudoc");
        Check(reloaded.CanvasObjects().size() == 1 && reloaded.CanvasObjects()[0].kind == ObjectKind::HtmlEmbed, "HtmlEmbed kind restored");
        Check(reloaded.CanvasObjects()[0].htmlTitleUtf8 == "Onda HTML5" &&
              reloaded.CanvasObjects()[0].htmlSourceUtf8 == "<html><body><h1>Onda</h1></body></html>" &&
              reloaded.CanvasObjects()[0].htmlAllowNetwork == false &&
              reloaded.CanvasObjects()[0].htmlSnapshotPngBytes.size() == capturedSnapPng.size(),
              "HtmlEmbed source, sandbox flag & snapshot PNG roundtrip cleanly");

        D2DRenderer ren;
        RasterImageBgra32 img;
        Camera2D cam{};
        Check(ren.RenderHwndViewportToOffscreen(reloaded, cam, 400, 260, nullptr, img), "Render HtmlEmbed snapshot offscreen");
        Check(HasNonUniformPixels(img), "Rendered HtmlEmbed has non-blank visual content");
    }

    // =========================================================================
    // 6. FULL FEATURE DISCOVERABILITY + guías de interfaz + WCAG AA/AAA CONTRAST
    // =========================================================================
    {
        CommandPaletteRegistry palReg;
        palReg.RegisterDefaultCommands();
        Check(palReg.All().size() >= 150, "CommandPaletteRegistry exposes >= 150 discoverable commands");
        std::set<std::string> cats;
        for (const auto& c : palReg.All()) {
            if (!c.categoryUtf8.empty()) cats.insert(c.categoryUtf8);
        }
        Check(cats.size() >= 10, "CommandPaletteRegistry spans >= 10 semantic categories");

        // Zero raw emojis & zero banned internal API jargon across UI files
        const std::filesystem::path uiFiles[] = {
            srcRoot / "app" / "main_studio.cpp",
            srcRoot / "app" / "studio_feature_dialogs.hpp",
            srcRoot / "app" / "studio_closure_dialogs.hpp",
            srcRoot.parent_path() / "ui_glass" / "hig_dialog_theme.hpp"
        };
        const char* bannedUiJargon[] = {
            "(DPAPI)", "(SAPI)", "Windows SAPI", "(WebView2)", "(TrySuspend)",
            "(Media Foundation)", "(ISpellChecker)", "Direct2D Software"
        };
        for (const auto& fp : uiFiles) {
            const std::string content = ReadWholeFile(fp);
            Check(!content.empty() && !ContainsUtf8FourByteEmoji(content),
                  ("Zero 4-byte emojis in " + fp.filename().string()).c_str());
            bool hasJargon = false;
            for (const char* j : bannedUiJargon) {
                if (content.find(j) != std::string::npos) hasJargon = true;
            }
            Check(!hasJargon, ("Zero internal API jargon in " + fp.filename().string()).c_str());
        }
        for (const auto& cmd : palReg.All()) {
            for (const char* j : bannedUiJargon) {
                if (cmd.titleUtf8.find(j) != std::string::npos) {
                    Check(false, ("Command palette exposes internal jargon: " + cmd.titleUtf8).c_str());
                }
            }
        }

        // WCAG AA/AAA contrast on all 3 guías de interfaz palettes (primary, tertiary, and tinted status pills)
        auto blendOver = [](UiGlass::Rgba8 fg, std::uint8_t alpha, UiGlass::Rgba8 bg) -> UiGlass::Rgba8 {
            const double a = static_cast<double>(alpha) / 255.0;
            return {
                static_cast<std::uint8_t>(std::lround(fg.r * a + bg.r * (1.0 - a))),
                static_cast<std::uint8_t>(std::lround(fg.g * a + bg.g * (1.0 - a))),
                static_cast<std::uint8_t>(std::lround(fg.b * a + bg.b * (1.0 - a))),
                255
            };
        };
        UiGlass::GlassSettings gsLight{}; gsLight.mode = UiGlass::AppearanceMode::Light;
        UiGlass::GlassSettings gsDark{};  gsDark.mode  = UiGlass::AppearanceMode::Dark;
        UiGlass::GlassSettings gsHc{};    gsHc.mode    = UiGlass::AppearanceMode::HighContrast;
        const auto pL = UiGlass::GetPalette(gsLight);
        const auto pD = UiGlass::GetPalette(gsDark);
        const auto pH = UiGlass::GetPalette(gsHc);
        Check(ContrastRatio(pL.labelPrimary, pL.contentCard) >= 4.5 &&
              ContrastRatio(pL.labelPrimary, pL.sidebarBg) >= 4.5 &&
              ContrastRatio(pL.labelTertiary, pL.sidebarBg) >= 4.5 &&
              ContrastRatio(pL.accentGreen, blendOver(pL.accentGreen, 20, pL.sidebarBg)) >= 4.5 &&
              ContrastRatio(pL.accentPurple, blendOver(pL.accentPurple, 20, pL.sidebarBg)) >= 4.5,
              "Light palette (text, tertiary, GPU/AI pills) satisfies WCAG AA (>= 4.5:1)");
        Check(ContrastRatio(pD.labelPrimary, pD.contentCard) >= 4.5 &&
              ContrastRatio(pD.labelPrimary, pD.sidebarBg) >= 4.5 &&
              ContrastRatio(pD.labelTertiary, pD.sidebarBg) >= 4.5 &&
              ContrastRatio(pD.accentGreen, blendOver(pD.accentGreen, 46, pD.sidebarBg)) >= 4.5 &&
              ContrastRatio(pD.accentPurple, blendOver(pD.accentPurple, 46, pD.sidebarBg)) >= 4.5,
              "Dark palette (text, tertiary, GPU/AI pills) satisfies WCAG AA (>= 4.5:1)");
        Check(ContrastRatio(pH.labelPrimary, pH.contentCard) >= 7.0 &&
              ContrastRatio(pH.labelPrimary, pH.sidebarBg) >= 7.0 &&
              ContrastRatio(pH.labelTertiary, pH.sidebarBg) >= 7.0 &&
              ContrastRatio(pH.accentGreen, blendOver(pH.accentGreen, 16, pH.sidebarBg)) >= 7.0 &&
              ContrastRatio(pH.accentPurple, blendOver(pH.accentPurple, 16, pH.sidebarBg)) >= 7.0,
              "HighContrast palette (text, tertiary, GPU/AI pills) satisfies WCAG AAA (>= 7.0:1)");
    }

    std::filesystem::remove_all(tmpDir, ec);
    RoUninitialize();
    CoUninitialize();

    std::cout << "PerfAndRichMediaTest: " << g_pass << " passed, " << g_fail << " failed.\n";
    return (g_fail == 0) ? 0 : 1;
}
