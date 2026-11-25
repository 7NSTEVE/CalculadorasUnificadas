#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <cwctype>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")

struct OperationCaptureItem {
    bool isCapture = true;
    int calcIdx = 0;
    int keyId = 0;
    std::wstring keyName;

    HBITMAP hThumb = nullptr;
    int thumbW = 0;
    int thumbH = 0;
    std::wstring filePath;
    std::wstring timeText;
    std::wstring calcName;
    RECT hitRect{ 0, 0, 0, 0 };
};

static inline std::wstring GetUnifiedExecutableDirectory()
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    const size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        dir.resize(pos);
    } else {
        dir = L".";
    }
    return dir;
}

static inline bool EnsureDirectoryTree(const std::wstring& dir)
{
    if (dir.empty()) return false;
    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    size_t pos = 0;
    while ((pos = dir.find_first_of(L"\\/", pos + 1)) != std::wstring::npos) {
        std::wstring sub = dir.substr(0, pos);
        if (sub.empty() || (sub.size() == 2 && sub[1] == L':')) continue;
        CreateDirectoryW(sub.c_str(), nullptr);
    }
    CreateDirectoryW(dir.c_str(), nullptr);
    attr = GetFileAttributesW(dir.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

static inline bool IsDirectoryWritable(const std::wstring& dir)
{
    if (dir.empty()) return false;
    const DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return false;
    }
    wchar_t probe[MAX_PATH * 2]{};
    swprintf_s(probe, L"%s\\.__cu_wprobe_%lu.tmp", dir.c_str(), GetCurrentProcessId());
    HANDLE hFile = CreateFileW(
        probe,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(hFile);
    return true;
}

static inline std::wstring ResolveWritableBaseDirectory(const std::wstring& overrideCandidateDir = L"")
{
    std::wstring candidate = overrideCandidateDir.empty()
        ? GetUnifiedExecutableDirectory()
        : overrideCandidateDir;

    if (IsDirectoryWritable(candidate)) {
        return candidate;
    }

    wchar_t localApp[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        std::wstring fallback = std::wstring(localApp) + L"\\CalculadoraUnificada";
        if (EnsureDirectoryTree(fallback) && IsDirectoryWritable(fallback)) {
            return fallback;
        }
    }

    wchar_t tempDir[MAX_PATH]{};
    DWORD tn = GetTempPathW(MAX_PATH, tempDir);
    if (tn > 0 && tn < MAX_PATH) {
        std::wstring fallback(tempDir);
        while (!fallback.empty() && (fallback.back() == L'\\' || fallback.back() == L'/')) {
            fallback.pop_back();
        }
        fallback += L"\\CalculadoraUnificada";
        if (EnsureDirectoryTree(fallback) && IsDirectoryWritable(fallback)) {
            return fallback;
        }
    }
    return candidate;
}

static inline std::wstring ResolveWritableConfigIniPath(const std::wstring& overrideCandidateDir = L"")
{
    return ResolveWritableBaseDirectory(overrideCandidateDir) + L"\\config_unificada.ini";
}

static inline std::wstring ResolveWritableOutputDir(const std::wstring& subdirName, const std::wstring& overrideCandidateDir = L"")
{
    std::wstring dir = ResolveWritableBaseDirectory(overrideCandidateDir) + L"\\" + subdirName;
    EnsureDirectoryTree(dir);
    return dir;
}

static inline bool EnsureGdiplusInitialized()
{
    static struct GdiplusGuard {
        ULONG_PTR token = 0;
        bool ok = false;
        GdiplusGuard() {
            Gdiplus::GdiplusStartupInput input;
            ok = (Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok);
        }
    } s_guard;
    return s_guard.ok;
}

static inline bool FindGdiplusEncoderClsid(const wchar_t* mimeType, CLSID* pClsid)
{
    if (!EnsureGdiplusInitialized() || !mimeType || !pClsid) return false;
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0 || num == 0) return false;

    std::vector<BYTE> buf(size);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    if (Gdiplus::GetImageEncoders(num, size, encoders) != Gdiplus::Ok) {
        return false;
    }
    for (UINT i = 0; i < num; ++i) {
        if (encoders[i].MimeType && wcscmp(encoders[i].MimeType, mimeType) == 0) {
            *pClsid = encoders[i].Clsid;
            return true;
        }
    }
    return false;
}

static inline bool SaveBitmapToFile(const wchar_t* filename, HBITMAP hBmp, int w, int h)
{
    if (!filename || !hBmp || w <= 0 || h <= 0) return false;
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    DWORD dwBmpSize = static_cast<DWORD>(((w * bi.biBitCount + 31) / 32) * 4 * h);
    std::vector<BYTE> lpBits(dwBmpSize);

    HDC hdc = GetDC(nullptr);
    int gotRows = GetDIBits(hdc, hBmp, 0, static_cast<UINT>(h), lpBits.data(), reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);
    if (gotRows <= 0) return false;

    BITMAPFILEHEADER bfh{};
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD dwWritten = 0;
    BOOL ok1 = WriteFile(hFile, &bfh, sizeof(bfh), &dwWritten, nullptr);
    BOOL ok2 = WriteFile(hFile, &bi, sizeof(bi), &dwWritten, nullptr);
    BOOL ok3 = WriteFile(hFile, lpBits.data(), dwBmpSize, &dwWritten, nullptr);
    CloseHandle(hFile);
    return ok1 && ok2 && ok3;
}

static inline bool SaveBitmapToPngFile(const wchar_t* filename, HBITMAP hBmp)
{
    if (!filename || !hBmp || !EnsureGdiplusInitialized()) return false;

    BITMAP bm{};
    if (GetObjectW(hBmp, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight == 0) {
        return false;
    }
    const int w = bm.bmWidth;
    const int h = (bm.bmHeight < 0) ? -bm.bmHeight : bm.bmHeight;
    const int stride = ((w * 3 + 3) / 4) * 4;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down for PixelFormat24bppRGB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<BYTE> pixels(static_cast<size_t>(stride) * h);
    HDC hdc = GetDC(nullptr);
    int gotRows = GetDIBits(hdc, hBmp, 0, static_cast<UINT>(h), pixels.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);
    if (gotRows <= 0) return false;

    CLSID pngClsid{};
    if (!FindGdiplusEncoderClsid(L"image/png", &pngClsid)) {
        return false;
    }

    Gdiplus::Bitmap outBmp(w, h, stride, PixelFormat24bppRGB, pixels.data());
    return outBmp.Save(filename, &pngClsid, nullptr) == Gdiplus::Ok;
}

static inline bool EndsWithCaseInsensitive(const std::wstring& str, const std::wstring& suffix)
{
    if (str.size() < suffix.size()) return false;
    const size_t offset = str.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (towlower(str[offset + i]) != towlower(suffix[i])) return false;
    }
    return true;
}

static inline bool SaveBitmapByExtension(const wchar_t* filename, HBITMAP hBmp, int w, int h)
{
    if (!filename || !hBmp) return false;
    std::wstring path(filename);
    if (EndsWithCaseInsensitive(path, L".bmp")) {
        return SaveBitmapToFile(filename, hBmp, w, h);
    }
    if (SaveBitmapToPngFile(filename, hBmp)) {
        return true;
    }
    return SaveBitmapToFile(filename, hBmp, w, h);
}

static inline std::wstring GetCapturesDirectory()
{
    return ResolveWritableOutputDir(L"capturas");
}

static inline bool SaveBitmapAsOperationCapture(
    HBITMAP hBmpFull,
    int w,
    int h,
    int calcIdx,
    const std::wstring& calcName,
    int keyId,
    const std::wstring& keyName,
    const std::wstring& filePrefix,
    OperationCaptureItem& outItem)
{
    if (!hBmpFull || w <= 0 || h <= 0) return false;
    HDC hdcSrc = GetDC(nullptr);
    if (!hdcSrc) return false;

    const std::wstring capDir = GetCapturesDirectory();

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuffer[64];
    swprintf_s(timeBuffer, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

    wchar_t pathBuffer[MAX_PATH];
    const wchar_t* prefix = filePrefix.empty() ? L"captura" : filePrefix.c_str();
    const wchar_t* modelTag = (calcIdx == 0) ? L"570LAX"
                            : (calcIdx == 1) ? L"570VN"
                            : (calcIdx == 2) ? L"fxCG50"
                            : (calcIdx == 3) ? L"fx991CW" : L"calc";
    swprintf_s(pathBuffer, L"%s\\%s_%04d%02d%02d_%02d%02d%02d_%03d_%s.png",
               capDir.c_str(), prefix,
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
               static_cast<int>(st.wMilliseconds), modelTag);

    if (!SaveBitmapToPngFile(pathBuffer, hBmpFull)) {
        swprintf_s(pathBuffer, L"%s\\%s_%04d%02d%02d_%02d%02d%02d_%03d_%s.bmp",
                   capDir.c_str(), prefix,
                   st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                   static_cast<int>(st.wMilliseconds), modelTag);
        SaveBitmapToFile(pathBuffer, hBmpFull, w, h);
    }

    const int thumbW = 260;
    const int thumbH = (thumbW > 0 && w > 0)
        ? (((static_cast<long long>(h) * thumbW / w) > 1) ? static_cast<int>(static_cast<long long>(h) * thumbW / w) : 1)
        : 1;

    HDC hdcMem = CreateCompatibleDC(hdcSrc);
    HGDIOBJ hOldMem = SelectObject(hdcMem, hBmpFull);
    HDC hdcThumb = CreateCompatibleDC(hdcSrc);
    HBITMAP hBmpThumb = CreateCompatibleBitmap(hdcSrc, thumbW, thumbH);
    HGDIOBJ hOldThumb = SelectObject(hdcThumb, hBmpThumb);

    SetStretchBltMode(hdcThumb, HALFTONE);
    SetBrushOrgEx(hdcThumb, 0, 0, nullptr);
    StretchBlt(hdcThumb, 0, 0, thumbW, thumbH, hdcMem, 0, 0, w, h, SRCCOPY);

    SelectObject(hdcThumb, hOldThumb);
    DeleteDC(hdcThumb);
    SelectObject(hdcMem, hOldMem);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcSrc);

    outItem.isCapture = true;
    outItem.calcIdx = calcIdx;
    outItem.keyId = keyId;
    outItem.keyName = keyName;
    outItem.calcName = calcName;
    outItem.hThumb = hBmpThumb;
    outItem.thumbW = thumbW;
    outItem.thumbH = thumbH;
    outItem.filePath = pathBuffer;
    outItem.timeText = timeBuffer;
    return hBmpThumb != nullptr;
}

static inline bool CaptureWindowSnapshot(HWND hWndSrc, int calcIdx, const std::wstring& calcName, OperationCaptureItem& outItem)
{
    if (!hWndSrc || !IsWindow(hWndSrc)) return false;

    RECT rc{};
    GetClientRect(hWndSrc, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;

    HDC hdcSrc = GetDC(hWndSrc);
    if (!hdcSrc) return false;

    HDC hdcMem = CreateCompatibleDC(hdcSrc);
    HBITMAP hBmpFull = CreateCompatibleBitmap(hdcSrc, w, h);
    HGDIOBJ hOldMem = SelectObject(hdcMem, hBmpFull);

    if (!PrintWindow(hWndSrc, hdcMem, PW_CLIENTONLY | 0x00000002 /*PW_RENDERFULLCONTENT*/)) {
        BitBlt(hdcMem, 0, 0, w, h, hdcSrc, 0, 0, SRCCOPY);
    }

    SelectObject(hdcMem, hOldMem);
    DeleteDC(hdcMem);
    ReleaseDC(hWndSrc, hdcSrc);

    const bool ok = SaveBitmapAsOperationCapture(
        hBmpFull, w, h, calcIdx, calcName, 0, L"", L"captura", outItem);
    DeleteObject(hBmpFull);
    return ok;
}

static inline void OpenCapturesFolder()
{
    const std::wstring capDir = GetCapturesDirectory();
    ShellExecuteW(nullptr, L"open", capDir.c_str(), nullptr, nullptr, SW_SHOW);
}

struct AppMenuThumbEntry {
    HBITMAP hBmp = nullptr;
    int w = 0;
    int h = 0;
};

inline std::vector<std::pair<std::wstring, AppMenuThumbEntry>>& GetAppMenuThumbCache()
{
    static std::vector<std::pair<std::wstring, AppMenuThumbEntry>> s_cache;
    return s_cache;
}

inline void StoreAppMenuThumb(const std::wstring& keyPath, HBITMAP hSrcBmp, int srcW, int srcH)
{
    if (keyPath.empty() || !hSrcBmp || srcW <= 0 || srcH <= 0) return;
    auto& cache = GetAppMenuThumbCache();
    for (auto& kv : cache) {
        if (kv.first == keyPath) {
            if (kv.second.hBmp) DeleteObject(kv.second.hBmp);
            kv.second.hBmp = static_cast<HBITMAP>(CopyImage(hSrcBmp, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
            kv.second.w = srcW;
            kv.second.h = srcH;
            return;
        }
    }
    if (cache.size() >= 64) {
        if (cache.front().second.hBmp) DeleteObject(cache.front().second.hBmp);
        cache.erase(cache.begin());
    }
    HBITMAP hCopy = static_cast<HBITMAP>(CopyImage(hSrcBmp, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    if (hCopy) {
        cache.push_back({ keyPath, { hCopy, srcW, srcH } });
    }
}

inline const AppMenuThumbEntry* FindAppMenuThumb(const std::wstring& keyPath)
{
    if (keyPath.empty()) return nullptr;
    const auto& cache = GetAppMenuThumbCache();
    for (const auto& kv : cache) {
        if (kv.first == keyPath && kv.second.hBmp) return &kv.second;
    }
    return nullptr;
}

inline void ClearAppMenuThumbCache()
{
    auto& cache = GetAppMenuThumbCache();
    for (auto& kv : cache) {
        if (kv.second.hBmp) DeleteObject(kv.second.hBmp);
    }
    cache.clear();
}
