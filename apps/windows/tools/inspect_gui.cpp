#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <vector>
#include <string>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    auto* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == nullptr) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

void CaptureHwndToPng(HWND hwnd, const std::wstring& filename) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC hdcWindow = GetDC(hwnd);
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);
    HBITMAP hbm = CreateCompatibleBitmap(hdcWindow, w, h);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMemDC, hbm);

    PrintWindow(hwnd, hdcMemDC, PW_CLIENTONLY);

    Bitmap bitmap(hbm, nullptr);
    CLSID pngClsid;
    GetEncoderClsid(L"image/png", &pngClsid);
    bitmap.Save(filename.c_str(), &pngClsid, nullptr);

    SelectObject(hdcMemDC, hOld);
    DeleteObject(hbm);
    DeleteDC(hdcMemDC);
    ReleaseDC(hwnd, hdcWindow);
    std::wcout << L"Saved window screenshot to: " << filename << std::endl;
}

struct InspectCtx {
    DWORD pid = 0;
    HWND foundHwnd = nullptr;
    HWND foundCanvas = nullptr;
};

BOOL CALLBACK EnumWin(HWND hwnd, LPARAM lParam) {
    auto* ctx = (InspectCtx*)lParam;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (ctx->pid != 0 && pid == ctx->pid) {
        wchar_t cls[256]{};
        GetClassNameW(hwnd, cls, 256);
        if (wcscmp(cls, L"CasioNativeMain") == 0) {
            ctx->foundHwnd = hwnd;
        }
    }
    return TRUE;
}

BOOL CALLBACK EnumChild(HWND hwnd, LPARAM lParam) {
    auto* ctx = (InspectCtx*)lParam;
    wchar_t cls[256]{};
    GetClassNameW(hwnd, cls, 256);
    if (wcscmp(cls, L"CasioCanvasArea") == 0) {
        ctx->foundCanvas = hwnd;
    }
    return TRUE;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wcout << L"Usage: InspectGui <pid> [action: capture [filename] | click <x> <y>]" << std::endl;
        return 1;
    }
    DWORD pid = _wtoi(argv[1]);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    InspectCtx ctx;
    ctx.pid = pid;
    EnumWindows(EnumWin, (LPARAM)&ctx);

    if (ctx.foundHwnd) {
        EnumChildWindows(ctx.foundHwnd, EnumChild, (LPARAM)&ctx);
    }

    if (argc >= 3 && wcscmp(argv[2], L"capture") == 0) {
        HWND target = ctx.foundCanvas ? ctx.foundCanvas : ctx.foundHwnd;
        if (target) {
            std::wstring outPath = (argc >= 4) ? argv[3] : L"screen_capture.png";
            CaptureHwndToPng(target, outPath);
        } else {
            std::wcout << L"Target window not found for PID " << pid << std::endl;
        }
    } else if (argc >= 4 && wcscmp(argv[2], L"mode") == 0) {
        int modeId = _wtoi(argv[3]);
        if (ctx.foundHwnd) {
            std::wcout << L"Sending WM_COMMAND modeId=" << modeId << L" to HWND 0x"
                       << std::hex << (uintptr_t)ctx.foundHwnd << std::dec << std::endl;
            SendMessageW(ctx.foundHwnd, WM_COMMAND, MAKEWPARAM(modeId, BN_CLICKED), 0);
            Sleep(200);
        }
    } else if (argc >= 4 && wcscmp(argv[2], L"command") == 0) {
        int cmdId = _wtoi(argv[3]);
        if (ctx.foundHwnd) {
            SendMessageW(ctx.foundHwnd, WM_COMMAND, MAKEWPARAM(cmdId, BN_CLICKED), 0);
            Sleep(200);
        }
    } else if (argc >= 4 && wcscmp(argv[2], L"vk") == 0) {
        int vk = _wtoi(argv[3]);
        HWND target = ctx.foundCanvas ? ctx.foundCanvas : ctx.foundHwnd;
        if (target) {
            std::wcout << L"Sending VK key=" << vk << L" to HWND 0x" << std::hex << (uintptr_t)target << std::dec << std::endl;
            SendMessageW(target, WM_KEYDOWN, vk, 0);
            Sleep(100);
            SendMessageW(target, WM_KEYUP, vk, 0);
            Sleep(200);
        }
    } else if (argc >= 5 && wcscmp(argv[2], L"click") == 0) {
        int x = _wtoi(argv[3]);
        int y = _wtoi(argv[4]);
        HWND target = ctx.foundCanvas ? ctx.foundCanvas : ctx.foundHwnd;
        if (target) {
            std::wcout << L"Sending click to HWND 0x" << std::hex << (uintptr_t)target << std::dec << L" at (" << x << L"," << y << L")" << std::endl;
            SendMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
            Sleep(80);
            SendMessageW(target, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
            Sleep(150);
        }
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}
