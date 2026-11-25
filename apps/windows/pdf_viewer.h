#pragma once
// ============================================================================
// pdf_viewer.h
// Visor nativo de PDF 100% integrado en Win32 sin dependencias externas.
// Utiliza la API del sistema Windows.Data.Pdf (WinRT COM ABI cargada en forma
// dinamica desde combase.dll / shcore.dll / ole32.dll) ejecutada en un hilo
// MTA dedicado para mantener la interfaz fluida e independiente de procesos
// externos o navegadores web.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <commdlg.h>
#include <ole2.h>

#pragma warning(push, 3)
#pragma warning(disable: 4458 4100 4265)
#include <inspectable.h>
#include <hstring.h>
#include <windows.data.pdf.h>
#pragma warning(pop)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ui_theme.h"
#include "manual_catalog.h"

namespace PdfViewerInternal {

inline constexpr UINT WM_PDF_WORKER_RESULT = WM_APP + 42;

struct UniqueBitmap {
    HBITMAP handle = nullptr;
    UniqueBitmap() noexcept = default;
    explicit UniqueBitmap(HBITMAP h) noexcept : handle(h) {}
    ~UniqueBitmap() { Reset(); }
    UniqueBitmap(const UniqueBitmap&) = delete;
    UniqueBitmap& operator=(const UniqueBitmap&) = delete;
    UniqueBitmap(UniqueBitmap&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
    UniqueBitmap& operator=(UniqueBitmap&& o) noexcept {
        if (this != &o) {
            Reset();
            handle = o.handle;
            o.handle = nullptr;
        }
        return *this;
    }
    void Reset(HBITMAP h = nullptr) noexcept {
        if (handle && handle != h) {
            DeleteObject(handle);
        }
        handle = h;
    }
    HBITMAP Get() const noexcept { return handle; }
    HBITMAP Release() noexcept {
        HBITMAP h = handle;
        handle = nullptr;
        return h;
    }
    explicit operator bool() const noexcept { return handle != nullptr; }
};

struct UniqueHandle {
    HANDLE handle = nullptr;
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(HANDLE h) noexcept : handle(h) {}
    ~UniqueHandle() { Reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) {
            Reset();
            handle = o.handle;
            o.handle = nullptr;
        }
        return *this;
    }
    void Reset(HANDLE h = nullptr) noexcept {
        if (handle && handle != INVALID_HANDLE_VALUE && handle != h) {
            CloseHandle(handle);
        }
        handle = h;
    }
    HANDLE Get() const noexcept { return handle; }
    explicit operator bool() const noexcept { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }
};

struct FileIdentity {
    std::uint64_t sizeBytes = 0;
    std::uint64_t lastWrite100ns = 0;
    bool valid = false;

    bool operator==(const FileIdentity& o) const noexcept {
        return valid == o.valid && sizeBytes == o.sizeBytes && lastWrite100ns == o.lastWrite100ns;
    }
    bool operator!=(const FileIdentity& o) const noexcept { return !(*this == o); }
};

inline FileIdentity QueryFileIdentityW(const std::wstring& path) {
    FileIdentity id{};
    if (path.empty()) return id;
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
        return id;
    }
    id.valid = true;
    id.sizeBytes = (static_cast<std::uint64_t>(fad.nFileSizeHigh) << 32) |
        static_cast<std::uint64_t>(fad.nFileSizeLow);
    id.lastWrite100ns = (static_cast<std::uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
        static_cast<std::uint64_t>(fad.ftLastWriteTime.dwLowDateTime);
    return id;
}

inline std::atomic<int>& ActiveProbeWorkerCounter() {
    static std::atomic<int> s_count{ 0 };
    return s_count;
}

inline std::atomic<int>& ActiveViewerWorkerCounter() {
    static std::atomic<int> s_count{ 0 };
    return s_count;
}

template <typename T>
class PdfComPtr {
public:
    PdfComPtr() noexcept = default;
    ~PdfComPtr() { Reset(); }
    PdfComPtr(const PdfComPtr&) = delete;
    PdfComPtr& operator=(const PdfComPtr&) = delete;
    PdfComPtr(PdfComPtr&& o) noexcept : ptr_(o.ptr_) { o.ptr_ = nullptr; }
    PdfComPtr& operator=(PdfComPtr&& o) noexcept {
        if (this != &o) {
            Reset();
            ptr_ = o.ptr_;
            o.ptr_ = nullptr;
        }
        return *this;
    }

    T* Get() const noexcept { return ptr_; }
    T** ReleaseAndGetAddressOf() noexcept {
        Reset();
        return &ptr_;
    }
    void Attach(T* p) noexcept {
        Reset();
        ptr_ = p;
    }
    T* Detach() noexcept {
        T* p = ptr_;
        ptr_ = nullptr;
        return p;
    }
    void Reset() noexcept {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    T* ptr_ = nullptr;
};

struct WinRtPdfApi {
    using PfnRoInitialize = HRESULT(WINAPI*)(int);
    using PfnRoUninitialize = void(WINAPI*)();
    using PfnRoGetActivationFactory = HRESULT(WINAPI*)(HSTRING, REFIID, void**);
    using PfnRoActivateInstance = HRESULT(WINAPI*)(HSTRING, IInspectable**);
    using PfnWindowsCreateString = HRESULT(WINAPI*)(LPCWSTR, UINT32, HSTRING*);
    using PfnWindowsDeleteString = HRESULT(WINAPI*)(HSTRING);
    using PfnCreateRandomAccessStreamOverStream = HRESULT(WINAPI*)(IStream*, int, REFIID, void**);
    using PfnCreateStreamOnHGlobal = HRESULT(WINAPI*)(HGLOBAL, BOOL, IStream**);
    using PfnGetOpenFileNameW = BOOL(WINAPI*)(LPOPENFILENAMEW);

    HMODULE hCombase = nullptr;
    HMODULE hShcore = nullptr;
    HMODULE hOle32 = nullptr;
    HMODULE hComdlg32 = nullptr;

    PfnRoInitialize pRoInitialize = nullptr;
    PfnRoUninitialize pRoUninitialize = nullptr;
    PfnRoGetActivationFactory pRoGetActivationFactory = nullptr;
    PfnRoActivateInstance pRoActivateInstance = nullptr;
    PfnWindowsCreateString pWindowsCreateString = nullptr;
    PfnWindowsDeleteString pWindowsDeleteString = nullptr;
    PfnCreateRandomAccessStreamOverStream pCreateRandomAccessStreamOverStream = nullptr;
    PfnCreateStreamOnHGlobal pCreateStreamOnHGlobal = nullptr;
    PfnGetOpenFileNameW pGetOpenFileNameW = nullptr;
    bool loaded = false;

    static WinRtPdfApi& Instance() {
        static WinRtPdfApi s_api;
        static std::once_flag s_once;
        std::call_once(s_once, []() {
            s_api.hCombase = LoadLibraryW(L"combase.dll");
            s_api.hShcore = LoadLibraryW(L"shcore.dll");
            s_api.hOle32 = LoadLibraryW(L"ole32.dll");
            if (s_api.hCombase) {
                s_api.pRoInitialize = reinterpret_cast<PfnRoInitialize>(
                    GetProcAddress(s_api.hCombase, "RoInitialize"));
                s_api.pRoUninitialize = reinterpret_cast<PfnRoUninitialize>(
                    GetProcAddress(s_api.hCombase, "RoUninitialize"));
                s_api.pRoGetActivationFactory = reinterpret_cast<PfnRoGetActivationFactory>(
                    GetProcAddress(s_api.hCombase, "RoGetActivationFactory"));
                s_api.pRoActivateInstance = reinterpret_cast<PfnRoActivateInstance>(
                    GetProcAddress(s_api.hCombase, "RoActivateInstance"));
                s_api.pWindowsCreateString = reinterpret_cast<PfnWindowsCreateString>(
                    GetProcAddress(s_api.hCombase, "WindowsCreateString"));
                s_api.pWindowsDeleteString = reinterpret_cast<PfnWindowsDeleteString>(
                    GetProcAddress(s_api.hCombase, "WindowsDeleteString"));
            }
            if (s_api.hShcore) {
                s_api.pCreateRandomAccessStreamOverStream =
                    reinterpret_cast<PfnCreateRandomAccessStreamOverStream>(
                        GetProcAddress(s_api.hShcore, "CreateRandomAccessStreamOverStream"));
            }
            if (s_api.hOle32) {
                s_api.pCreateStreamOnHGlobal = reinterpret_cast<PfnCreateStreamOnHGlobal>(
                    GetProcAddress(s_api.hOle32, "CreateStreamOnHGlobal"));
            }
            s_api.loaded = s_api.pRoInitialize && s_api.pRoUninitialize &&
                s_api.pRoGetActivationFactory && s_api.pRoActivateInstance &&
                s_api.pWindowsCreateString && s_api.pWindowsDeleteString &&
                s_api.pCreateRandomAccessStreamOverStream && s_api.pCreateStreamOnHGlobal;
        });
        return s_api;
    }

    bool EnsureComdlgLoaded() {
        if (pGetOpenFileNameW) return true;
        if (!hComdlg32) hComdlg32 = LoadLibraryW(L"comdlg32.dll");
        if (hComdlg32) {
            pGetOpenFileNameW = reinterpret_cast<PfnGetOpenFileNameW>(
                GetProcAddress(hComdlg32, "GetOpenFileNameW"));
        }
        return pGetOpenFileNameW != nullptr;
    }
};

class PdfHString {
public:
    explicit PdfHString(const wchar_t* text) {
        auto& api = WinRtPdfApi::Instance();
        if (api.pWindowsCreateString && text) {
            UINT32 len = static_cast<UINT32>(wcslen(text));
            api.pWindowsCreateString(text, len, &str_);
        }
    }
    ~PdfHString() {
        auto& api = WinRtPdfApi::Instance();
        if (str_ && api.pWindowsDeleteString) {
            api.pWindowsDeleteString(str_);
            str_ = nullptr;
        }
    }
    PdfHString(const PdfHString&) = delete;
    PdfHString& operator=(const PdfHString&) = delete;

    HSTRING Get() const noexcept { return str_; }
    explicit operator bool() const noexcept { return str_ != nullptr; }

private:
    HSTRING str_ = nullptr;
};

class PdfDocLoadedHandler final
    : public ABI::Windows::Foundation::IAsyncOperationCompletedHandler<ABI::Windows::Data::Pdf::PdfDocument*>
    , public IAgileObject {
public:
    PdfDocLoadedHandler() {
        hEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    ~PdfDocLoadedHandler() {
        if (hEvent_) CloseHandle(hEvent_);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG c = InterlockedDecrement(&refCount_);
        if (c == 0) delete this;
        return static_cast<ULONG>(c);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;
        using HandlerIntf = ABI::Windows::Foundation::IAsyncOperationCompletedHandler<ABI::Windows::Data::Pdf::PdfDocument*>;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(HandlerIntf)) {
            *ppvObject = static_cast<HandlerIntf*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(IAgileObject)) {
            *ppvObject = static_cast<IAgileObject*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(
        __FIAsyncOperation_1_Windows__CData__CPdf__CPdfDocument*,
        ABI::Windows::Foundation::AsyncStatus asyncStatus) override {
        status_ = asyncStatus;
        if (hEvent_) SetEvent(hEvent_);
        return S_OK;
    }

    bool Wait(DWORD timeoutMs = 30000) const {
        return hEvent_ && WaitForSingleObject(hEvent_, timeoutMs) == WAIT_OBJECT_0;
    }
    ABI::Windows::Foundation::AsyncStatus Status() const noexcept { return status_; }

private:
    LONG volatile refCount_ = 1;
    HANDLE hEvent_ = nullptr;
    ABI::Windows::Foundation::AsyncStatus status_ = ABI::Windows::Foundation::AsyncStatus::Started;
};

class PdfActionCompletedHandler final
    : public ABI::Windows::Foundation::IAsyncActionCompletedHandler
    , public IAgileObject {
public:
    PdfActionCompletedHandler() {
        hEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    ~PdfActionCompletedHandler() {
        if (hEvent_) CloseHandle(hEvent_);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG c = InterlockedDecrement(&refCount_);
        if (c == 0) delete this;
        return static_cast<ULONG>(c);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;
        if (riid == __uuidof(IUnknown) ||
            riid == __uuidof(ABI::Windows::Foundation::IAsyncActionCompletedHandler)) {
            *ppvObject = static_cast<ABI::Windows::Foundation::IAsyncActionCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        if (riid == __uuidof(IAgileObject)) {
            *ppvObject = static_cast<IAgileObject*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE Invoke(
        ABI::Windows::Foundation::IAsyncAction*,
        ABI::Windows::Foundation::AsyncStatus asyncStatus) override {
        status_ = asyncStatus;
        if (hEvent_) SetEvent(hEvent_);
        return S_OK;
    }

    bool Wait(DWORD timeoutMs = 30000) const {
        return hEvent_ && WaitForSingleObject(hEvent_, timeoutMs) == WAIT_OBJECT_0;
    }
    ABI::Windows::Foundation::AsyncStatus Status() const noexcept { return status_; }

private:
    LONG volatile refCount_ = 1;
    HANDLE hEvent_ = nullptr;
    ABI::Windows::Foundation::AsyncStatus status_ = ABI::Windows::Foundation::AsyncStatus::Started;
};

inline void EnsureGdiplusForPdf() {
    static std::once_flag s_gpOnce;
    static ULONG_PTR s_gpToken = 0;
    std::call_once(s_gpOnce, []() {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&s_gpToken, &input, nullptr);
    });
}

inline void CloseWinRtObject(IUnknown* obj) {
    if (!obj) return;
    PdfComPtr<ABI::Windows::Foundation::IClosable> closable;
    if (SUCCEEDED(obj->QueryInterface(
            __uuidof(ABI::Windows::Foundation::IClosable),
            reinterpret_cast<void**>(closable.ReleaseAndGetAddressOf()))) && closable) {
        closable->Close();
    }
}

class WinRtPdfSession {
public:
    ~WinRtPdfSession() { Close(); }

    void Close() {
        CloseWinRtObject(pdfDoc_.Get());
        pdfDoc_.Reset();
        CloseWinRtObject(fileRas_.Get());
        fileRas_.Reset();
        fileStream_.Reset();
        pageCount_ = 0;
        openedPath_.clear();
    }

    const std::wstring& OpenedPath() const noexcept { return openedPath_; }
    UINT32 PageCount() const noexcept { return pageCount_; }
    bool IsOpen() const noexcept { return static_cast<bool>(pdfDoc_); }

    HRESULT Open(const std::wstring& path) {
        Close();
        auto& api = WinRtPdfApi::Instance();
        if (!api.loaded) return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
        if (path.empty() || !PathFileExistsW(path.c_str())) {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        HRESULT hr = SHCreateStreamOnFileEx(
            path.c_str(),
            STGM_READ | STGM_SHARE_DENY_NONE,
            FILE_ATTRIBUTE_NORMAL,
            FALSE,
            nullptr,
            fileStream_.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !fileStream_) {
            Close();
            return FAILED(hr) ? hr : E_FAIL;
        }

        hr = api.pCreateRandomAccessStreamOverStream(
            fileStream_.Get(),
            0, // BSOS_DEFAULT
            __uuidof(ABI::Windows::Storage::Streams::IRandomAccessStream),
            reinterpret_cast<void**>(fileRas_.ReleaseAndGetAddressOf()));
        if (FAILED(hr) || !fileRas_) {
            Close();
            return FAILED(hr) ? hr : E_FAIL;
        }

        PdfHString classId(RuntimeClass_Windows_Data_Pdf_PdfDocument);
        if (!classId) {
            Close();
            return E_OUTOFMEMORY;
        }

        PdfComPtr<ABI::Windows::Data::Pdf::IPdfDocumentStatics> statics;
        hr = api.pRoGetActivationFactory(
            classId.Get(),
            __uuidof(ABI::Windows::Data::Pdf::IPdfDocumentStatics),
            reinterpret_cast<void**>(statics.ReleaseAndGetAddressOf()));
        if (FAILED(hr) || !statics) {
            Close();
            return FAILED(hr) ? hr : E_NOINTERFACE;
        }

        PdfComPtr<__FIAsyncOperation_1_Windows__CData__CPdf__CPdfDocument> loadOp;
        hr = statics->LoadFromStreamAsync(fileRas_.Get(), loadOp.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !loadOp) {
            Close();
            return FAILED(hr) ? hr : E_FAIL;
        }

        PdfComPtr<PdfDocLoadedHandler> handler;
        handler.Attach(new PdfDocLoadedHandler());
        hr = loadOp->put_Completed(handler.Get());
        if (FAILED(hr)) {
            Close();
            return hr;
        }
        if (!handler->Wait(30000)) {
            Close();
            return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        }
        if (handler->Status() != ABI::Windows::Foundation::AsyncStatus::Completed) {
            HRESULT asyncErr = E_FAIL;
            PdfComPtr<IAsyncInfo> info;
            if (SUCCEEDED(loadOp->QueryInterface(
                    __uuidof(IAsyncInfo),
                    reinterpret_cast<void**>(info.ReleaseAndGetAddressOf()))) && info) {
                info->get_ErrorCode(&asyncErr);
            }
            Close();
            return FAILED(asyncErr) ? asyncErr : E_FAIL;
        }

        hr = loadOp->GetResults(pdfDoc_.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !pdfDoc_) {
            Close();
            return FAILED(hr) ? hr : E_FAIL;
        }

        hr = pdfDoc_->get_PageCount(&pageCount_);
        if (FAILED(hr) || pageCount_ == 0) {
            Close();
            return FAILED(hr) ? hr : E_FAIL;
        }

        openedPath_ = path;
        return S_OK;
    }

    HRESULT RenderPageToDIB(
        UINT32 pageIndex0,
        int targetWidthPx,
        UniqueBitmap* outBmp,
        int* outWidth,
        int* outHeight,
        int* outInkPixels,
        const std::atomic<bool>* cancelFlag = nullptr,
        std::size_t maxBitmapBytes = 0) {
        if (outBmp) outBmp->Reset();
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        if (outInkPixels) *outInkPixels = 0;

        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            return E_ABORT;
        }
        if (!pdfDoc_ || pageIndex0 >= pageCount_) return E_INVALIDARG;
        auto& api = WinRtPdfApi::Instance();
        if (!api.loaded) return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);

        PdfComPtr<ABI::Windows::Data::Pdf::IPdfPage> page;
        HRESULT hr = pdfDoc_->GetPage(pageIndex0, page.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !page) return FAILED(hr) ? hr : E_FAIL;

        ABI::Windows::Foundation::Size pageSize{ 595.0f, 842.0f };
        page->get_Size(&pageSize);
        const double aspect = (pageSize.Width > 1.0f && pageSize.Height > 1.0f)
            ? static_cast<double>(pageSize.Height) / static_cast<double>(pageSize.Width)
            : 1.4142;

        int reqW = (std::max)(160, (std::min)(3000, targetWidthPx));
        int reqH = (std::max)(160, static_cast<int>(std::lround(static_cast<double>(reqW) * aspect)));
        if (reqH > 4000) {
            reqH = 4000;
            reqW = (std::max)(160, static_cast<int>(std::lround(static_cast<double>(reqH) / aspect)));
        }
        if (maxBitmapBytes > 0) {
            const std::uint64_t estBytes = static_cast<std::uint64_t>(reqW) * static_cast<std::uint64_t>(reqH) * 4ull;
            if (estBytes > maxBitmapBytes) {
                const double scaleDown = std::sqrt(static_cast<double>(maxBitmapBytes) / static_cast<double>(estBytes));
                reqW = (std::max)(64, static_cast<int>(std::floor(static_cast<double>(reqW) * scaleDown)));
                reqH = (std::max)(64, static_cast<int>(std::floor(static_cast<double>(reqH) * scaleDown)));
            }
        }

        PdfComPtr<ABI::Windows::Data::Pdf::IPdfPageRenderOptions> opts;
        PdfHString optsClassId(RuntimeClass_Windows_Data_Pdf_PdfPageRenderOptions);
        if (optsClassId) {
            PdfComPtr<IInspectable> insp;
            if (SUCCEEDED(api.pRoActivateInstance(optsClassId.Get(), insp.ReleaseAndGetAddressOf())) && insp) {
                insp->QueryInterface(
                    __uuidof(ABI::Windows::Data::Pdf::IPdfPageRenderOptions),
                    reinterpret_cast<void**>(opts.ReleaseAndGetAddressOf()));
            }
        }
        if (opts) {
            opts->put_DestinationWidth(static_cast<UINT32>(reqW));
            opts->put_DestinationHeight(static_cast<UINT32>(reqH));
            ABI::Windows::UI::Color whiteBg{ 255, 255, 255, 255 };
            opts->put_BackgroundColor(whiteBg);
            opts->put_IsIgnoringHighContrast(TRUE);
        }

        PdfComPtr<IStream> memStream;
        hr = api.pCreateStreamOnHGlobal(nullptr, TRUE, memStream.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !memStream) {
            CloseWinRtObject(page.Get());
            return FAILED(hr) ? hr : E_OUTOFMEMORY;
        }

        PdfComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> outRas;
        hr = api.pCreateRandomAccessStreamOverStream(
            memStream.Get(),
            0,
            __uuidof(ABI::Windows::Storage::Streams::IRandomAccessStream),
            reinterpret_cast<void**>(outRas.ReleaseAndGetAddressOf()));
        if (FAILED(hr) || !outRas) {
            CloseWinRtObject(page.Get());
            return FAILED(hr) ? hr : E_FAIL;
        }

        PdfComPtr<ABI::Windows::Foundation::IAsyncAction> renderAction;
        if (opts) {
            hr = page->RenderWithOptionsToStreamAsync(
                outRas.Get(), opts.Get(), renderAction.ReleaseAndGetAddressOf());
        } else {
            hr = page->RenderToStreamAsync(outRas.Get(), renderAction.ReleaseAndGetAddressOf());
        }
        if (FAILED(hr) || !renderAction) {
            CloseWinRtObject(outRas.Get());
            CloseWinRtObject(page.Get());
            return FAILED(hr) ? hr : E_FAIL;
        }

        PdfComPtr<PdfActionCompletedHandler> handler;
        handler.Attach(new PdfActionCompletedHandler());
        hr = renderAction->put_Completed(handler.Get());
        if (FAILED(hr) || !handler->Wait(30000)) {
            CloseWinRtObject(outRas.Get());
            CloseWinRtObject(page.Get());
            return FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        }
        if (handler->Status() != ABI::Windows::Foundation::AsyncStatus::Completed) {
            HRESULT asyncErr = E_FAIL;
            PdfComPtr<IAsyncInfo> info;
            if (SUCCEEDED(renderAction->QueryInterface(
                    __uuidof(IAsyncInfo),
                    reinterpret_cast<void**>(info.ReleaseAndGetAddressOf()))) && info) {
                info->get_ErrorCode(&asyncErr);
            }
            CloseWinRtObject(outRas.Get());
            CloseWinRtObject(page.Get());
            return FAILED(asyncErr) ? asyncErr : E_FAIL;
        }
        hr = renderAction->GetResults();
        CloseWinRtObject(outRas.Get());
        CloseWinRtObject(page.Get());
        if (FAILED(hr)) return hr;
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            return E_ABORT;
        }

        LARGE_INTEGER zero{};
        memStream->Seek(zero, STREAM_SEEK_SET, nullptr);

        EnsureGdiplusForPdf();
        std::unique_ptr<Gdiplus::Bitmap> gpRaw(Gdiplus::Bitmap::FromStream(memStream.Get(), FALSE));
        if (!gpRaw || gpRaw->GetLastStatus() != Gdiplus::Ok) {
            return E_FAIL;
        }

        const UINT bmpW = gpRaw->GetWidth();
        const UINT bmpH = gpRaw->GetHeight();
        if (bmpW == 0 || bmpH == 0) {
            return E_FAIL;
        }

        Gdiplus::BitmapData bd{};
        Gdiplus::Rect lockRect(0, 0, static_cast<INT>(bmpW), static_cast<INT>(bmpH));
        if (gpRaw->LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bd) != Gdiplus::Ok) {
            return E_FAIL;
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = static_cast<LONG>(bmpW);
        bmi.bmiHeader.biHeight = -static_cast<LONG>(bmpH);
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* dstBits = nullptr;
        HDC hdcScreen = GetDC(nullptr);
        UniqueBitmap hDib(CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &dstBits, nullptr, 0));
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);

        if (!hDib || !dstBits) {
            gpRaw->UnlockBits(&bd);
            return E_OUTOFMEMORY;
        }

        const UINT rowBytes = bmpW * 4u;
        int inkCount = 0;
        const uint8_t* srcBase = static_cast<const uint8_t*>(bd.Scan0);
        uint8_t* dstBase = static_cast<uint8_t*>(dstBits);
        for (UINT y = 0; y < bmpH; ++y) {
            const uint8_t* srcRow = srcBase + static_cast<size_t>(y) * static_cast<size_t>(bd.Stride);
            uint8_t* dstRow = dstBase + static_cast<size_t>(y) * static_cast<size_t>(rowBytes);
            std::memcpy(dstRow, srcRow, rowBytes);
            if (outInkPixels) {
                for (UINT x = 0; x < bmpW; ++x) {
                    const uint8_t b = srcRow[x * 4u + 0u];
                    const uint8_t g = srcRow[x * 4u + 1u];
                    const uint8_t r = srcRow[x * 4u + 2u];
                    if (r < 245 || g < 245 || b < 245) {
                        ++inkCount;
                    }
                }
            }
        }

        gpRaw->UnlockBits(&bd);
        gpRaw.reset();

        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            return E_ABORT;
        }

        if (outWidth) *outWidth = static_cast<int>(bmpW);
        if (outHeight) *outHeight = static_cast<int>(bmpH);
        if (outInkPixels) *outInkPixels = inkCount;
        if (outBmp) {
            *outBmp = std::move(hDib);
        }
        return S_OK;
    }

private:
    PdfComPtr<IStream> fileStream_;
    PdfComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> fileRas_;
    PdfComPtr<ABI::Windows::Data::Pdf::IPdfDocument> pdfDoc_;
    UINT32 pageCount_ = 0;
    std::wstring openedPath_;
};

} // namespace PdfViewerInternal

struct PdfTopicShortcut {
    std::wstring heading;
    int pageNo = 1;
};

// Sonda headless para pruebas automatizadas (abre y rasteriza una pagina PDF a DIB en MTA).
// Soporta timeout configurable y cancelacion cooperativa con vida compartida sin use-after-scope.
inline HRESULT PdfNativeProbeRenderPageToDIBEx(
    const std::wstring& pdfPath,
    int pageOneBased,
    int targetWidthPx,
    DWORD timeoutMs,
    int* outPageCount,
    int* outWidth,
    int* outHeight,
    int* outNonWhitePixels,
    DWORD simulatedWorkerDelayMs = 0,
    std::size_t maxBitmapBytes = 0)
{
    if (outPageCount) *outPageCount = 0;
    if (outWidth) *outWidth = 0;
    if (outHeight) *outHeight = 0;
    if (outNonWhitePixels) *outNonWhitePixels = 0;

    struct ProbeCtx {
        std::wstring path;
        int page1 = 1;
        int targetW = 900;
        DWORD simulatedDelayMs = 0;
        std::size_t maxBytes = 0;
        std::atomic<bool> cancelled{ false };
        std::atomic<bool> finished{ false };
        int pageCount = 0;
        int w = 0;
        int h = 0;
        int ink = 0;
        HRESULT hr = E_FAIL;
    };

    auto ctx = std::make_shared<ProbeCtx>();
    ctx->path = pdfPath;
    ctx->page1 = pageOneBased;
    ctx->targetW = targetWidthPx;
    ctx->simulatedDelayMs = simulatedWorkerDelayMs;
    ctx->maxBytes = maxBitmapBytes;

    auto threadProc = [](LPVOID lp) -> DWORD {
        std::shared_ptr<ProbeCtx> c;
        {
            auto* holder = static_cast<std::shared_ptr<ProbeCtx>*>(lp);
            if (holder) {
                c = std::move(*holder);
                delete holder;
            }
        }
        if (!c) return 1;

        struct WorkerGuard {
            std::shared_ptr<ProbeCtx>& ctxRef;
            WorkerGuard(std::shared_ptr<ProbeCtx>& ref) : ctxRef(ref) {
                PdfViewerInternal::ActiveProbeWorkerCounter().fetch_add(1, std::memory_order_acq_rel);
            }
            ~WorkerGuard() {
                if (ctxRef) ctxRef->finished.store(true, std::memory_order_release);
                PdfViewerInternal::ActiveProbeWorkerCounter().fetch_sub(1, std::memory_order_acq_rel);
            }
        } guard(c);

        if (c->simulatedDelayMs > 0) {
            const DWORD step = 10;
            DWORD elapsed = 0;
            while (elapsed < c->simulatedDelayMs && !c->cancelled.load(std::memory_order_acquire)) {
                Sleep((std::min)(step, c->simulatedDelayMs - elapsed));
                elapsed += step;
            }
        }
        if (c->cancelled.load(std::memory_order_acquire)) {
            c->hr = E_ABORT;
            return 0;
        }

        auto& api = PdfViewerInternal::WinRtPdfApi::Instance();
        if (!api.loaded) {
            c->hr = HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
            return 1;
        }
        const HRESULT hrInit = api.pRoInitialize(1); // RO_INIT_MULTITHREADED
        {
            PdfViewerInternal::WinRtPdfSession sess;
            c->hr = sess.Open(c->path);
            if (SUCCEEDED(c->hr) && !c->cancelled.load(std::memory_order_acquire)) {
                c->pageCount = static_cast<int>(sess.PageCount());
                int idx = (c->page1 >= 1) ? (c->page1 - 1) : 0;
                if (idx >= c->pageCount) idx = (std::max)(0, c->pageCount - 1);
                PdfViewerInternal::UniqueBitmap bmp;
                c->hr = sess.RenderPageToDIB(
                    static_cast<UINT32>(idx),
                    c->targetW,
                    &bmp,
                    &c->w,
                    &c->h,
                    &c->ink,
                    &c->cancelled,
                    c->maxBytes);
            } else if (c->cancelled.load(std::memory_order_acquire)) {
                c->hr = E_ABORT;
            }
        }
        if (SUCCEEDED(hrInit)) {
            api.pRoUninitialize();
        }
        return 0;
    };

    auto* argHolder = new std::shared_ptr<ProbeCtx>(ctx);
    PdfViewerInternal::UniqueHandle hTh(CreateThread(nullptr, 0, threadProc, argHolder, 0, nullptr));
    if (!hTh) {
        const DWORD err = GetLastError();
        delete argHolder;
        return HRESULT_FROM_WIN32(err ? err : ERROR_NOT_ENOUGH_MEMORY);
    }

    const DWORD waitRc = WaitForSingleObject(hTh.Get(), timeoutMs);
    if (waitRc == WAIT_TIMEOUT) {
        // Never touch outputs or read ctx asynchronously after timeout: signal cancel and let worker drop last ref.
        ctx->cancelled.store(true, std::memory_order_release);
        return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }
    if (waitRc != WAIT_OBJECT_0) {
        const DWORD err = GetLastError();
        ctx->cancelled.store(true, std::memory_order_release);
        return HRESULT_FROM_WIN32(err ? err : ERROR_GEN_FAILURE);
    }

    if (outPageCount) *outPageCount = ctx->pageCount;
    if (outWidth) *outWidth = ctx->w;
    if (outHeight) *outHeight = ctx->h;
    if (outNonWhitePixels) *outNonWhitePixels = ctx->ink;
    return ctx->hr;
}

inline HRESULT PdfNativeProbeRenderPageToDIB(
    const std::wstring& pdfPath,
    int pageOneBased,
    int targetWidthPx,
    int* outPageCount,
    int* outWidth,
    int* outHeight,
    int* outNonWhitePixels)
{
    return PdfNativeProbeRenderPageToDIBEx(
        pdfPath,
        pageOneBased,
        targetWidthPx,
        45000,
        outPageCount,
        outWidth,
        outHeight,
        outNonWhitePixels,
        0,
        0);
}

namespace PdfViewerUi {

enum PdfToolbarControlId {
    BTN_MANUALS = 0,
    BTN_OPEN = 1,
    BTN_TOPICS = 2,
    BTN_PREV = 3,
    BTN_NEXT = 4,
    BTN_ZOOM_OUT = 5,
    BTN_FIT = 6,
    BTN_ZOOM_IN = 7,
    BTN_STUDIO = 8,
    BTN_COUNT = 9
};

inline bool LaunchDocumentStudioImpl(HWND owner, const std::wstring& args = L"") {
    wchar_t modPath[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, modPath, MAX_PATH);
    std::wstring exeDir;
    if (n > 0 && n < MAX_PATH) {
        std::wstring full(modPath, n);
        const std::size_t slash = full.find_last_of(L"\\/");
        if (slash != std::wstring::npos) exeDir = full.substr(0, slash);
    }

    auto fileExists = [](const std::wstring& p) -> bool {
        const DWORD attr = GetFileAttributesW(p.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    };

    std::wstring studioExe;
    const wchar_t* const exeNames[2] = { L"CuadernU.exe", L"DocumentStudio.exe" };
    for (const wchar_t* nm : exeNames) {
        const std::wstring cand = exeDir.empty() ? std::wstring(nm) : (exeDir + L"\\" + nm);
        if (fileExists(cand)) { studioExe = cand; break; }
        if (!exeDir.empty()) {
            const std::wstring devSibling1 = exeDir + L"\\..\\..\\studio-x64\\Release\\" + nm;
            if (fileExists(devSibling1)) { studioExe = devSibling1; break; }
            const std::wstring devSibling2 = exeDir + L"\\..\\..\\build-studio-x64\\Release\\" + nm;
            if (fileExists(devSibling2)) { studioExe = devSibling2; break; }
        }
    }
    if (studioExe.empty()) {
        studioExe = exeDir.empty() ? L"CuadernU.exe" : (exeDir + L"\\CuadernU.exe");
    }

    if (!fileExists(studioExe)) {
        const std::wstring msg = L"No se encontr\u00F3 CuadernU.exe junto a CalculadoraUnificada.exe:\n" + studioExe;
        MessageBoxW(owner, msg.c_str(), L"CuadernU", MB_ICONWARNING | MB_OK);
        return false;
    }

    const INT_PTR rc = reinterpret_cast<INT_PTR>(ShellExecuteW(
        owner,
        L"open",
        studioExe.c_str(),
        args.empty() ? nullptr : args.c_str(),
        exeDir.empty() ? nullptr : exeDir.c_str(),
        SW_SHOWNORMAL));
    if (rc <= 32) {
        const std::wstring msg = L"No se pudo iniciar CuadernU.exe (c\u00F3digo ShellExecute=" + std::to_wstring(rc) + L").";
        MessageBoxW(owner, msg.c_str(), L"CuadernU", MB_ICONERROR | MB_OK);
        return false;
    }
    return true;
}

struct CachedPdfPage {
    int pageIndex0 = -1;
    int renderWidth = 0;
    int bmpW = 0;
    int bmpH = 0;
    std::size_t byteSize = 0;
    PdfViewerInternal::UniqueBitmap bmp;

    HBITMAP BitmapHandle() const noexcept { return bmp.Get(); }
};

struct WorkerRenderResult {
    std::wstring path;
    PdfViewerInternal::FileIdentity fileId;
    int generation = 0;
    int pageIndex0 = 0;
    int pageCount = 0;
    int renderWidth = 0;
    int bmpW = 0;
    int bmpH = 0;
    PdfViewerInternal::UniqueBitmap bmp;
    HRESULT hr = E_FAIL;
};

struct PdfViewerState {
    HWND hWnd = nullptr;
    HWND hCanvas = nullptr;
    HWND hPageEdit = nullptr;
    WNDPROC prevEditProc = nullptr;
    HFONT hEditFont = nullptr;
    float editFontScale = 0.0f;
    float uiScale = 1.0f;

    std::wstring currentPath;
    PdfViewerInternal::FileIdentity currentFileId;
    std::wstring manualsDir;
    std::wstring docTitle;
    std::vector<PdfTopicShortcut> topics;

    int currentPage0 = 0;
    int pageCount = 0;
    bool fitWidth = true;
    float zoomFactor = 1.0f;
    int targetRenderWidth = 960;
    bool isLoading = false;
    HRESULT lastError = S_OK;
    bool initFailed = false;
    std::wstring initErrorText;

    int scrollX = 0;
    int scrollY = 0;
    int hoverBtn = -1;
    RECT btnRects[BTN_COUNT]{};
    RECT editBoxChromeRc{};
    RECT pageTotalRc{};
    RECT titleBadgeRc{};
    RECT toolbarRc{};

    bool dragging = false;
    POINT dragStartPt{};
    int dragStartScrollX = 0;
    int dragStartScrollY = 0;

    std::size_t maxCacheBytes = 128ull * 1024ull * 1024ull;
    std::size_t cacheBytesUsed = 0;
    std::deque<CachedPdfPage> cache;

    // Sincronizacion con el hilo MTA de renderizado y propiedad compartida segura.
    std::atomic<bool> cancelRequested{ false };
    std::atomic<bool> workerTimedOutOnClose{ false };
    DWORD simulatedWorkerDelayMs = 0;

    std::mutex reqMtx;
    HANDLE hWakeEvent = nullptr;
    HANDLE hWorkerThread = nullptr;
    bool workerQuit = false;
    int pendingGen = 0;
    int lastHandledGen = 0;
    std::wstring pendingPath;
    PdfViewerInternal::FileIdentity pendingFileId;
    int pendingPage0 = 0;
    int pendingTargetW = 960;

    std::mutex resultMtx;
    HWND workerNotifyHwnd = nullptr;
    std::deque<WorkerRenderResult> pendingResults;

    ~PdfViewerState() {
        {
            std::lock_guard<std::mutex> lk(resultMtx);
            pendingResults.clear();
            workerNotifyHwnd = nullptr;
        }
        cache.clear();
        cacheBytesUsed = 0;
        if (hWorkerThread) {
            CloseHandle(hWorkerThread);
            hWorkerThread = nullptr;
        }
        if (hWakeEvent) {
            CloseHandle(hWakeEvent);
            hWakeEvent = nullptr;
        }
    }
};

struct PdfWndBinding {
    std::shared_ptr<PdfViewerState> state;
};

inline PdfViewerState* GetPdfViewerStateFromWnd(HWND hWnd) noexcept {
    if (!hWnd) return nullptr;
    auto* binding = reinterpret_cast<PdfWndBinding*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    return binding ? binding->state.get() : nullptr;
}

inline HWND g_hPdfViewerWnd = nullptr;

inline std::wstring GetExecutableDirectoryForPdfViewer() {
    DWORD cap = 512;
    std::wstring buf;
    for (;;) {
        buf.assign(cap, L'\0');
        SetLastError(ERROR_SUCCESS);
        const DWORD len = GetModuleFileNameW(nullptr, buf.data(), cap);
        if (len == 0) return {};
        if (len < cap && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            buf.resize(len);
            return std::filesystem::path(buf).parent_path().wstring();
        }
        if (cap >= 32768) {
            buf.resize(len);
            return std::filesystem::path(buf).parent_path().wstring();
        }
        cap *= 2;
    }
}

inline std::wstring ResolveDefaultManualsDirForPdfViewer() {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path exeDir(GetExecutableDirectoryForPdfViewer());
    const fs::path candidates[] = {
        exeDir / L"manuales",
        exeDir.parent_path() / L"manuales",
        exeDir.parent_path().parent_path() / L"manuales",
        exeDir.parent_path().parent_path() / L"private_local" / L"manuales",
        exeDir.parent_path().parent_path().parent_path() / L"private_local" / L"manuales",
#ifdef CASIO_MONOREPO_ROOT
        fs::path(CASIO_MONOREPO_ROOT) / L"private_local" / L"manuales",
#endif
    };
    for (const auto& c : candidates) {
        if (fs::exists(c, ec) && !ec) return c.wstring();
        ec.clear();
    }
    return (exeDir / L"manuales").wstring();
}

inline std::wstring FileNameOnlyW(const std::wstring& fullPath) {
    const wchar_t* name = PathFindFileNameW(fullPath.c_str());
    return name ? std::wstring(name) : fullPath;
}

inline std::wstring ResolvePdfDisplayTitle(const std::wstring& fullPath) {
    const std::wstring fn = FileNameOnlyW(fullPath);
    for (const auto& m : g_officialManuals) {
        if (_wcsicmp(fn.c_str(), m.fileName) == 0) {
            return m.shortButtonLabel;
        }
    }
    return fn;
}

inline std::vector<PdfTopicShortcut> LoadTopicsFromTsv(
    const std::wstring& manualsDir,
    const std::wstring& pdfFullPath)
{
    std::vector<PdfTopicShortcut> out;
    if (manualsDir.empty() || pdfFullPath.empty()) return out;
    const std::wstring fn = FileNameOnlyW(pdfFullPath);
    const std::wstring tsvPath = manualsDir + L"\\indice_paginas_pdf.tsv";
    std::ifstream in(tsvPath, std::ios::binary);
    if (!in) return out;

    std::string line;
    int lastPage = -1;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        int wlen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), nullptr, 0);
        if (wlen <= 0) continue;
        std::wstring wline(static_cast<size_t>(wlen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()), wline.data(), wlen);

        std::vector<std::wstring> cols;
        size_t start = 0;
        while (start <= wline.size()) {
            size_t tab = wline.find(L'\t', start);
            if (tab == std::wstring::npos) {
                cols.push_back(wline.substr(start));
                break;
            }
            cols.push_back(wline.substr(start, tab - start));
            start = tab + 1;
        }
        if (cols.size() < 5) continue;
        if (_wcsicmp(cols[0].c_str(), fn.c_str()) != 0) continue;

        int pg = _wtoi(cols[3].c_str());
        if (pg < 1) continue;
        std::wstring head = cols[4];
        if (head.empty()) head = L"P\u00E1gina " + std::to_wstring(pg);
        if (head.size() > 54) head = head.substr(0, 51) + L"...";
        if (pg == lastPage && !out.empty() && out.back().heading == head) continue;
        lastPage = pg;
        out.push_back({ head, pg });
    }
    return out;
}

inline void ClearPageCache(PdfViewerState* st) {
    if (!st) return;
    st->cache.clear();
    st->cacheBytesUsed = 0;
}

inline const CachedPdfPage* FindBestCachedPage(
    const PdfViewerState* st,
    int page0,
    int wantedRenderW,
    bool* exactMatch = nullptr)
{
    if (exactMatch) *exactMatch = false;
    if (!st) return nullptr;
    const CachedPdfPage* fallback = nullptr;
    int bestDiff = 1000000;
    for (const auto& c : st->cache) {
        if (c.pageIndex0 == page0 && c.bmp) {
            int diff = std::abs(c.renderWidth - wantedRenderW);
            if (diff <= 16) {
                if (exactMatch) *exactMatch = true;
                return &c;
            }
            if (diff < bestDiff) {
                bestDiff = diff;
                fallback = &c;
            }
        }
    }
    return fallback;
}

inline void PromoteCachedPageLru(PdfViewerState* st, int page0, int renderW) {
    if (!st || st->cache.size() <= 1) return;
    for (auto it = st->cache.begin(); it != st->cache.end(); ++it) {
        if (it->pageIndex0 == page0 && std::abs(it->renderWidth - renderW) <= 16) {
            if (it != st->cache.begin()) {
                CachedPdfPage item = std::move(*it);
                st->cache.erase(it);
                st->cache.push_front(std::move(item));
            }
            return;
        }
    }
}

inline void StoreCachedPage(
    PdfViewerState* st,
    int page0,
    int renderW,
    int bmpW,
    int bmpH,
    PdfViewerInternal::UniqueBitmap bmp)
{
    if (!st || !bmp || bmpW <= 0 || bmpH <= 0) return;
    const std::size_t entryBytes =
        static_cast<std::size_t>(bmpW) * static_cast<std::size_t>(bmpH) * 4u;

    for (auto it = st->cache.begin(); it != st->cache.end(); ++it) {
        if (it->pageIndex0 == page0 && std::abs(it->renderWidth - renderW) <= 16) {
            if (st->cacheBytesUsed >= it->byteSize) {
                st->cacheBytesUsed -= it->byteSize;
            } else {
                st->cacheBytesUsed = 0;
            }
            st->cache.erase(it);
            break;
        }
    }

    if (st->maxCacheBytes > 0) {
        while (!st->cache.empty() && (st->cacheBytesUsed + entryBytes > st->maxCacheBytes)) {
            const std::size_t backBytes = st->cache.back().byteSize;
            st->cache.pop_back();
            st->cacheBytesUsed = (st->cacheBytesUsed >= backBytes) ? (st->cacheBytesUsed - backBytes) : 0;
        }
        if (entryBytes > st->maxCacheBytes) {
            // Single bitmap exceeds tiny test/configured byte budget; avoid exceeding budget invariant.
            return;
        }
    }

    CachedPdfPage item;
    item.pageIndex0 = page0;
    item.renderWidth = renderW;
    item.bmpW = bmpW;
    item.bmpH = bmpH;
    item.byteSize = entryBytes;
    item.bmp = std::move(bmp);
    st->cache.push_front(std::move(item));
    st->cacheBytesUsed += entryBytes;
}

inline int ComputeDesiredRenderWidth(const PdfViewerState* st) {
    if (!st) return 960;
    RECT rc{};
    if (st->hCanvas && IsWindow(st->hCanvas)) {
        GetClientRect(st->hCanvas, &rc);
    }
    int availW = (std::max)(360, static_cast<int>(rc.right - rc.left) - 44);
    if (st->fitWidth) {
        return (std::max)(420, (std::min)(2400, availW));
    }
    const double baseW = 794.0 * static_cast<double>(st->uiScale); // A4 a 96 DPI * uiScale
    int zoomed = static_cast<int>(std::lround(baseW * static_cast<double>(st->zoomFactor)));
    return (std::max)(260, (std::min)(2800, zoomed));
}

inline void UpdatePageEditText(PdfViewerState* st) {
    if (!st || !st->hPageEdit || !IsWindow(st->hPageEdit)) return;
    wchar_t buf[32]{};
    swprintf_s(buf, L"%d", st->currentPage0 + 1);
    wchar_t cur[32]{};
    GetWindowTextW(st->hPageEdit, cur, 32);
    if (wcscmp(cur, buf) != 0) {
        SetWindowTextW(st->hPageEdit, buf);
    }
}

inline void UpdateCanvasScrollbars(PdfViewerState* st) {
    if (!st || !st->hCanvas || !IsWindow(st->hCanvas)) return;
    RECT rc{};
    GetClientRect(st->hCanvas, &rc);
    int viewW = (std::max)(1, static_cast<int>(rc.right - rc.left));
    int viewH = (std::max)(1, static_cast<int>(rc.bottom - rc.top));

    const CachedPdfPage* pg = FindBestCachedPage(st, st->currentPage0, st->targetRenderWidth);
    const int pad = 28;
    int docW = pg ? (pg->bmpW + pad * 2) : viewW;
    int docH = pg ? (pg->bmpH + pad * 2) : viewH;

    int maxSX = (std::max)(0, docW - viewW);
    int maxSY = (std::max)(0, docH - viewH);
    st->scrollX = (std::max)(0, (std::min)(maxSX, st->scrollX));
    st->scrollY = (std::max)(0, (std::min)(maxSY, st->scrollY));

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (maxSY > 0) ? (docH - 1) : 0;
    si.nPage = (maxSY > 0) ? static_cast<UINT>(viewH) : 1u;
    si.nPos = st->scrollY;
    SetScrollInfo(st->hCanvas, SB_VERT, &si, TRUE);

    si.nMax = (maxSX > 0) ? (docW - 1) : 0;
    si.nPage = (maxSX > 0) ? static_cast<UINT>(viewW) : 1u;
    si.nPos = st->scrollX;
    SetScrollInfo(st->hCanvas, SB_HORZ, &si, TRUE);
}

inline void RequestPageRender(PdfViewerState* st, bool resetScroll = true) {
    if (!st || st->currentPath.empty()) return;
    if (resetScroll) {
        st->scrollX = 0;
        st->scrollY = 0;
    }
    const int wantedW = ComputeDesiredRenderWidth(st);
    st->targetRenderWidth = wantedW;

    if (st->initFailed) {
        st->isLoading = false;
        if (st->hWnd) InvalidateRect(st->hWnd, nullptr, FALSE);
        if (st->hCanvas) InvalidateRect(st->hCanvas, nullptr, FALSE);
        return;
    }

    bool exact = false;
    const CachedPdfPage* hit = FindBestCachedPage(st, st->currentPage0, wantedW, &exact);
    if (exact && hit && st->pageCount > 0) {
        PromoteCachedPageLru(st, st->currentPage0, hit->renderWidth);
        st->isLoading = false;
        st->lastError = S_OK;
        UpdatePageEditText(st);
        UpdateCanvasScrollbars(st);
        if (st->hWnd) InvalidateRect(st->hWnd, nullptr, FALSE);
        if (st->hCanvas) InvalidateRect(st->hCanvas, nullptr, FALSE);
        return;
    }

    st->isLoading = true;
    st->lastError = S_OK;
    const PdfViewerInternal::FileIdentity curId = PdfViewerInternal::QueryFileIdentityW(st->currentPath);
    if (curId != st->currentFileId) {
        ClearPageCache(st);
        st->currentFileId = curId;
    }
    {
        std::lock_guard<std::mutex> lk(st->reqMtx);
        ++st->pendingGen;
        st->pendingPath = st->currentPath;
        st->pendingFileId = st->currentFileId;
        st->pendingPage0 = st->currentPage0;
        st->pendingTargetW = wantedW;
    }
    if (st->hWakeEvent) SetEvent(st->hWakeEvent);
    UpdatePageEditText(st);
    if (st->hWnd) InvalidateRect(st->hWnd, nullptr, FALSE);
    if (st->hCanvas) InvalidateRect(st->hCanvas, nullptr, FALSE);
}

inline void NavigatePdfPage(PdfViewerState* st, int targetPageOneBased) {
    if (!st) return;
    int p1 = targetPageOneBased;
    if (st->pageCount > 0) {
        p1 = (std::max)(1, (std::min)(st->pageCount, p1));
    } else if (p1 < 1) {
        p1 = 1;
    }
    st->currentPage0 = p1 - 1;
    RequestPageRender(st, true);
}

inline void SwitchPdfDocument(
    PdfViewerState* st,
    const std::wstring& pdfPath,
    int startPageOneBased,
    const std::vector<PdfTopicShortcut>* topicsOpt = nullptr)
{
    if (!st || pdfPath.empty()) return;
    const PdfViewerInternal::FileIdentity newFileId = PdfViewerInternal::QueryFileIdentityW(pdfPath);
    const bool samePath = (!st->currentPath.empty() && _wcsicmp(st->currentPath.c_str(), pdfPath.c_str()) == 0);
    const bool sameDiskVersion = samePath && (newFileId == st->currentFileId) && newFileId.valid;

    if (!sameDiskVersion) {
        ClearPageCache(st);
        {
            std::lock_guard<std::mutex> rlk(st->resultMtx);
            st->pendingResults.clear();
        }
        st->currentPath = pdfPath;
        st->currentFileId = newFileId;
        st->pageCount = 0;
        st->docTitle = ResolvePdfDisplayTitle(pdfPath);
        if (topicsOpt) {
            st->topics = *topicsOpt;
        } else {
            if (st->manualsDir.empty()) st->manualsDir = ResolveDefaultManualsDirForPdfViewer();
            st->topics = LoadTopicsFromTsv(st->manualsDir, pdfPath);
        }
        if (st->hWnd) {
            std::wstring winTitle = L"Visor Nativo PDF \u2014 " + st->docTitle;
            SetWindowTextW(st->hWnd, winTitle.c_str());
        }
    } else if (topicsOpt && !topicsOpt->empty()) {
        st->topics = *topicsOpt;
    }
    NavigatePdfPage(st, (std::max)(1, startPageOneBased));
}

inline void ProcessPendingWorkerResults(PdfViewerState* st) {
    if (!st) return;
    std::deque<WorkerRenderResult> localQueue;
    {
        std::lock_guard<std::mutex> rlk(st->resultMtx);
        localQueue.swap(st->pendingResults);
    }
    if (localQueue.empty()) return;

    int activeGen = 0;
    {
        std::lock_guard<std::mutex> lk(st->reqMtx);
        activeGen = st->pendingGen;
    }

    bool anyApplied = false;
    for (auto& res : localQueue) {
        const bool matchesPath = (!st->currentPath.empty() &&
            _wcsicmp(res.path.c_str(), st->currentPath.c_str()) == 0);
        const bool matchesFile = (res.fileId == st->currentFileId);
        const bool matchesGen = (res.generation == activeGen);
        if (!matchesPath || !matchesFile || !matchesGen) {
            // Stale result from older generation, superseded zoom, or previous document: drop cleanly.
            continue;
        }
        if (res.pageCount > 0) {
            st->pageCount = res.pageCount;
        }
        if (SUCCEEDED(res.hr) && res.bmp) {
            StoreCachedPage(st, res.pageIndex0, res.renderWidth, res.bmpW, res.bmpH, std::move(res.bmp));
        }
        st->isLoading = false;
        st->lastError = res.hr;
        anyApplied = true;
    }

    if (anyApplied) {
        UpdatePageEditText(st);
        UpdateCanvasScrollbars(st);
        if (st->hWnd && IsWindow(st->hWnd)) InvalidateRect(st->hWnd, nullptr, FALSE);
        if (st->hCanvas && IsWindow(st->hCanvas)) InvalidateRect(st->hCanvas, nullptr, FALSE);
    }
}

inline DWORD WINAPI PdfWorkerThreadProc(LPVOID lp) {
    std::shared_ptr<PdfViewerState> st;
    {
        auto* holder = static_cast<std::shared_ptr<PdfViewerState>*>(lp);
        if (holder) {
            st = std::move(*holder);
            delete holder;
        }
    }
    if (!st || !st->hWakeEvent) return 1;

    struct ActiveGuard {
        ActiveGuard() {
            PdfViewerInternal::ActiveViewerWorkerCounter().fetch_add(1, std::memory_order_acq_rel);
        }
        ~ActiveGuard() {
            PdfViewerInternal::ActiveViewerWorkerCounter().fetch_sub(1, std::memory_order_acq_rel);
        }
    } activeGuard;

    auto& api = PdfViewerInternal::WinRtPdfApi::Instance();
    if (!api.loaded) return 1;

    const HRESULT hrInit = api.pRoInitialize(1); // RO_INIT_MULTITHREADED
    PdfViewerInternal::WinRtPdfSession session;
    PdfViewerInternal::FileIdentity openedFileId{};

    for (;;) {
        const DWORD waitRes = WaitForSingleObject(st->hWakeEvent, INFINITE);
        if (waitRes != WAIT_OBJECT_0) break;

        for (;;) {
            std::wstring jobPath;
            PdfViewerInternal::FileIdentity jobFileId{};
            int jobPage0 = 0;
            int jobTargetW = 960;
            int jobGen = 0;
            bool shouldQuit = false;
            DWORD simDelayMs = st->simulatedWorkerDelayMs;

            {
                std::lock_guard<std::mutex> lk(st->reqMtx);
                if (st->workerQuit || st->cancelRequested.load(std::memory_order_acquire)) {
                    shouldQuit = true;
                } else if (st->pendingGen != st->lastHandledGen) {
                    jobGen = st->pendingGen;
                    st->lastHandledGen = jobGen;
                    jobPath = st->pendingPath;
                    jobFileId = st->pendingFileId;
                    jobPage0 = st->pendingPage0;
                    jobTargetW = st->pendingTargetW;
                } else {
                    break;
                }
            }

            if (shouldQuit) {
                session.Close();
                if (SUCCEEDED(hrInit)) api.pRoUninitialize();
                return 0;
            }

            if (simDelayMs > 0) {
                DWORD elapsed = 0;
                while (elapsed < simDelayMs && !st->cancelRequested.load(std::memory_order_acquire)) {
                    Sleep((std::min<DWORD>)(10, simDelayMs - elapsed));
                    elapsed += 10;
                }
            }
            if (st->cancelRequested.load(std::memory_order_acquire)) {
                session.Close();
                if (SUCCEEDED(hrInit)) api.pRoUninitialize();
                return 0;
            }

            WorkerRenderResult res;
            res.path = jobPath;
            res.fileId = jobFileId;
            res.generation = jobGen;
            res.renderWidth = jobTargetW;

            HRESULT hr = S_OK;
            const bool needReopen = !session.IsOpen() ||
                _wcsicmp(session.OpenedPath().c_str(), jobPath.c_str()) != 0 ||
                openedFileId != jobFileId;
            if (needReopen) {
                hr = session.Open(jobPath);
                openedFileId = SUCCEEDED(hr) ? jobFileId : PdfViewerInternal::FileIdentity{};
            }
            if (SUCCEEDED(hr)) {
                res.pageCount = static_cast<int>(session.PageCount());
                int p0 = (std::max)(0, (std::min)(res.pageCount - 1, jobPage0));
                res.pageIndex0 = p0;
                hr = session.RenderPageToDIB(
                    static_cast<UINT32>(p0),
                    jobTargetW,
                    &res.bmp,
                    &res.bmpW,
                    &res.bmpH,
                    nullptr,
                    &st->cancelRequested,
                    st->maxCacheBytes);
            }
            res.hr = hr;

            bool stillCurrent = false;
            {
                std::lock_guard<std::mutex> lk(st->reqMtx);
                stillCurrent = !st->workerQuit &&
                    !st->cancelRequested.load(std::memory_order_acquire) &&
                    (jobGen == st->pendingGen);
            }
            if (!stillCurrent) {
                // Superseded by a newer request or window teardown; drop result locally.
                continue;
            }

            HWND notifyHwnd = nullptr;
            {
                std::lock_guard<std::mutex> rlk(st->resultMtx);
                notifyHwnd = st->workerNotifyHwnd;
                if (notifyHwnd && IsWindow(notifyHwnd)) {
                    while (st->pendingResults.size() >= 4) {
                        st->pendingResults.pop_front();
                    }
                    st->pendingResults.push_back(std::move(res));
                }
            }
            if (notifyHwnd && IsWindow(notifyHwnd)) {
                PostMessageW(notifyHwnd, PdfViewerInternal::WM_PDF_WORKER_RESULT, 0, 0);
            }
        }
    }

    session.Close();
    if (SUCCEEDED(hrInit)) api.pRoUninitialize();
    return 0;
}

inline void LayoutPdfViewer(PdfViewerState* st) {
    if (!st || !st->hWnd || !IsWindow(st->hWnd)) return;
    RECT rc{};
    GetClientRect(st->hWnd, &rc);
    const int cw = static_cast<int>(rc.right - rc.left);
    const int ch = static_cast<int>(rc.bottom - rc.top);

    const float sc = CasioTheme::ComputeDialogUiScale(st->hWnd, 1020, 780);
    st->uiScale = sc;
    auto S = [sc](int v) { return CasioTheme::UiToDevice(v, sc); };

    const int tbH = S(52);
    st->toolbarRc = RECT{ 0, 0, cw, tbH };

    const int y0 = S(10);
    const int btnH = S(32);
    int x = S(12);

    st->btnRects[BTN_MANUALS] = RECT{ x, y0, x + S(96), y0 + btnH };
    x += S(100);
    st->btnRects[BTN_OPEN] = RECT{ x, y0, x + S(70), y0 + btnH };
    x += S(74);
    st->btnRects[BTN_TOPICS] = RECT{ x, y0, x + S(98), y0 + btnH };
    x += S(112);

    st->btnRects[BTN_PREV] = RECT{ x, y0, x + S(32), y0 + btnH };
    x += S(36);
    st->editBoxChromeRc = RECT{ x, y0 + S(2), x + S(46), y0 + btnH - S(2) };
    if (st->hPageEdit && IsWindow(st->hPageEdit)) {
        if (!st->hEditFont || std::fabs(st->editFontScale - sc) > 0.01f) {
            HFONT newF = CasioTheme::CreateUiFont(CasioTheme::UiToDevice(13, sc), true);
            if (newF) {
                HFONT oldF = st->hEditFont;
                st->hEditFont = newF;
                st->editFontScale = sc;
                SendMessageW(st->hPageEdit, WM_SETFONT, reinterpret_cast<WPARAM>(newF), TRUE);
                if (oldF) DeleteObject(oldF);
            }
        }
        MoveWindow(
            st->hPageEdit,
            st->editBoxChromeRc.left + S(4),
            st->editBoxChromeRc.top + S(5),
            (std::max)(24, static_cast<int>(st->editBoxChromeRc.right - st->editBoxChromeRc.left) - S(8)),
            (std::max)(18, static_cast<int>(st->editBoxChromeRc.bottom - st->editBoxChromeRc.top) - S(8)),
            TRUE);
    }
    x += S(50);
    st->pageTotalRc = RECT{ x, y0, x + S(52), y0 + btnH };
    x += S(54);
    st->btnRects[BTN_NEXT] = RECT{ x, y0, x + S(32), y0 + btnH };
    x += S(46);

    st->btnRects[BTN_ZOOM_OUT] = RECT{ x, y0, x + S(30), y0 + btnH };
    x += S(34);
    st->btnRects[BTN_FIT] = RECT{ x, y0, x + S(74), y0 + btnH };
    x += S(78);
    st->btnRects[BTN_ZOOM_IN] = RECT{ x, y0, x + S(30), y0 + btnH };
    x += S(40);
    st->btnRects[BTN_STUDIO] = RECT{ x, y0, x + S(98), y0 + btnH };
    x += S(106);

    st->titleBadgeRc = RECT{ x, y0, (std::max)(x, cw - S(12)), y0 + btnH };

    if (st->hCanvas && IsWindow(st->hCanvas)) {
        MoveWindow(st->hCanvas, 0, tbH, cw, (std::max)(1, ch - tbH), TRUE);
    }
}

inline void OpenPdfManualPickerMenu(PdfViewerState* st) {
    if (!st || !st->hWnd) return;
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    const std::wstring curFn = FileNameOnlyW(st->currentPath);
    const wchar_t* groupNames[] = {
        L"Negra (fx-991CW)",
        L"Blanca (ClassWiz)",
        L"Gris (ES/VN PLUS)",
        L"Graficadora (fx-CG50)",
        L"Emulador y Licencia"
    };

    const int n = static_cast<int>(sizeof(g_officialManuals) / sizeof(g_officialManuals[0]));
    auto addGroup = [&](const wchar_t* hdr, int catMatch, bool cwOnly) {
        HMENU hSub = CreatePopupMenu();
        int count = 0;
        for (int i = 0; i < n; ++i) {
            const auto& m = g_officialManuals[i];
            const bool isCw = (m.catId == 1 && (m.calcMask & 8) != 0);
            bool match = false;
            if (cwOnly) match = isCw;
            else if (catMatch == 1) match = (m.catId == 1 && !isCw);
            else match = (m.catId == catMatch);
            if (!match) continue;
            UINT flags = MF_STRING;
            if (_wcsicmp(curFn.c_str(), m.fileName) == 0) flags |= MF_CHECKED;
            AppendMenuW(hSub, flags, static_cast<UINT_PTR>(5000 + i), m.fullTitle);
            ++count;
        }
        if (count > 0) {
            AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSub), hdr);
        } else {
            DestroyMenu(hSub);
        }
    };

    addGroup(groupNames[0], 1, true);
    addGroup(groupNames[1], 1, false);
    addGroup(groupNames[2], 2, false);
    addGroup(groupNames[3], 3, false);
    addGroup(groupNames[4], 4, false);

    POINT pt{ st->btnRects[BTN_MANUALS].left, st->btnRects[BTN_MANUALS].bottom + 4 };
    ClientToScreen(st->hWnd, &pt);
    UINT cmd = static_cast<UINT>(TrackPopupMenuEx(
        hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, st->hWnd, nullptr));
    DestroyMenu(hMenu);

    if (cmd >= 5000 && static_cast<int>(cmd - 5000) < n) {
        const auto& m = g_officialManuals[cmd - 5000];
        if (st->manualsDir.empty()) st->manualsDir = ResolveDefaultManualsDirForPdfViewer();
        std::wstring full = st->manualsDir + L"\\" + m.fileName;
        if (PathFileExistsW(full.c_str())) {
            SwitchPdfDocument(st, full, 1, nullptr);
        } else {
            MessageBoxW(st->hWnd, (L"No se encontr\u00F3 el archivo PDF:\n" + full).c_str(),
                L"Visor Nativo PDF", MB_ICONWARNING | MB_OK);
        }
    }
}

inline void OpenPdfTopicsMenu(PdfViewerState* st) {
    if (!st || !st->hWnd || st->topics.empty()) return;
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    const int total = static_cast<int>(st->topics.size());
    const int chunkSize = 24;
    if (total <= chunkSize) {
        for (int i = 0; i < total; ++i) {
            wchar_t label[160]{};
            swprintf_s(label, L"P\u00E1g. %d \u2014 %s", st->topics[i].pageNo, st->topics[i].heading.c_str());
            AppendMenuW(hMenu, MF_STRING, static_cast<UINT_PTR>(7000 + i), label);
        }
    } else {
        for (int base = 0; base < total; base += chunkSize) {
            int end = (std::min)(total, base + chunkSize);
            HMENU hSub = CreatePopupMenu();
            for (int i = base; i < end; ++i) {
                wchar_t label[160]{};
                swprintf_s(label, L"P\u00E1g. %d \u2014 %s", st->topics[i].pageNo, st->topics[i].heading.c_str());
                AppendMenuW(hSub, MF_STRING, static_cast<UINT_PTR>(7000 + i), label);
            }
            wchar_t grp[64]{};
            swprintf_s(grp, L"P\u00E1ginas %d \u2013 %d", st->topics[base].pageNo, st->topics[end - 1].pageNo);
            AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSub), grp);
        }
    }

    POINT pt{ st->btnRects[BTN_TOPICS].left, st->btnRects[BTN_TOPICS].bottom + 4 };
    ClientToScreen(st->hWnd, &pt);
    UINT cmd = static_cast<UINT>(TrackPopupMenuEx(
        hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x, pt.y, st->hWnd, nullptr));
    DestroyMenu(hMenu);

    if (cmd >= 7000 && static_cast<int>(cmd - 7000) < total) {
        NavigatePdfPage(st, st->topics[cmd - 7000].pageNo);
    }
}

inline void OpenPdfFileBrowser(PdfViewerState* st) {
    if (!st || !st->hWnd) return;
    std::wstring initDir = st->manualsDir.empty() ? ResolveDefaultManualsDirForPdfViewer() : st->manualsDir;
    std::wstring chosenPath;

    // Prefer COM IFileOpenDialog for arbitrary Unicode path lengths (> MAX_PATH).
    IFileOpenDialog* pDlg = nullptr;
    HRESULT hrCom = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg));
    if (SUCCEEDED(hrCom) && pDlg) {
        DWORD opts = 0;
        if (SUCCEEDED(pDlg->GetOptions(&opts))) {
            pDlg->SetOptions(opts | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM);
        }
        static constexpr COMDLG_FILTERSPEC kFilters[] = {
            { L"Documentos PDF (*.pdf)", L"*.pdf" },
            { L"Todos los archivos (*.*)", L"*.*" }
        };
        pDlg->SetFileTypes(2, kFilters);
        pDlg->SetFileTypeIndex(1);
        if (!initDir.empty()) {
            IShellItem* pFolder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(initDir.c_str(), nullptr, IID_PPV_ARGS(&pFolder))) && pFolder) {
                pDlg->SetFolder(pFolder);
                pFolder->Release();
            }
        }
        if (SUCCEEDED(pDlg->Show(st->hWnd))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pDlg->GetResult(&pItem)) && pItem) {
                PWSTR rawPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &rawPath)) && rawPath) {
                    chosenPath = rawPath;
                    CoTaskMemFree(rawPath);
                }
                pItem->Release();
            }
        }
        pDlg->Release();
        if (!chosenPath.empty()) {
            SwitchPdfDocument(st, chosenPath, 1, nullptr);
        }
        return;
    }

    auto& api = PdfViewerInternal::WinRtPdfApi::Instance();
    if (!api.EnsureComdlgLoaded()) return;

    std::vector<wchar_t> fileBuf(8192, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = st->hWnd;
    ofn.lpstrFile = fileBuf.data();
    ofn.nMaxFile = static_cast<DWORD>(fileBuf.size());
    ofn.lpstrFilter = L"Documentos PDF (*.pdf)\0*.pdf\0Todos los archivos (*.*)\0*.*\0";
    ofn.lpstrInitialDir = initDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (api.pGetOpenFileNameW(&ofn)) {
        SwitchPdfDocument(st, fileBuf.data(), 1, nullptr);
    }
}

inline void AdjustPdfZoom(PdfViewerState* st, int deltaSteps) {
    if (!st) return;
    if (st->fitWidth) {
        const CachedPdfPage* pg = FindBestCachedPage(st, st->currentPage0, st->targetRenderWidth);
        const double baseW = 794.0 * static_cast<double>(st->uiScale);
        st->zoomFactor = (pg && baseW > 1.0)
            ? static_cast<float>(static_cast<double>(pg->bmpW) / baseW)
            : 1.0f;
        st->fitWidth = false;
    }
    st->zoomFactor += static_cast<float>(deltaSteps) * 0.2f;
    st->zoomFactor = (std::max)(0.4f, (std::min)(3.0f, st->zoomFactor));
    RequestPageRender(st, false);
}

inline void CommitPageEditJump(PdfViewerState* st) {
    if (!st || !st->hPageEdit) return;
    wchar_t buf[32]{};
    GetWindowTextW(st->hPageEdit, buf, 32);
    int req = _wtoi(buf);
    if (req >= 1) {
        NavigatePdfPage(st, req);
    } else {
        UpdatePageEditText(st);
    }
    if (st->hCanvas) SetFocus(st->hCanvas);
}

inline LRESULT CALLBACK PdfPageEditSubclassProc(
    HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<PdfViewerState*>(GetWindowLongPtrW(hEdit, GWLP_USERDATA));
    if (msg == WM_GETDLGCODE) {
        return DLGC_WANTALLKEYS;
    }
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        CommitPageEditJump(st);
        return 0;
    }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE && st && st->hCanvas) {
        UpdatePageEditText(st);
        SetFocus(st->hCanvas);
        return 0;
    }
    if (st && st->prevEditProc) {
        return CallWindowProcW(st->prevEditProc, hEdit, msg, wParam, lParam);
    }
    return DefWindowProcW(hEdit, msg, wParam, lParam);
}

inline void HandlePdfKeyShortcut(PdfViewerState* st, WPARAM vk) {
    if (!st) return;
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (ctrl && (vk == 'O' || vk == 'o')) {
        OpenPdfFileBrowser(st);
        return;
    }
    switch (vk) {
    case VK_LEFT:
    case VK_PRIOR:
        NavigatePdfPage(st, st->currentPage0); // 1-based previous = (current0 + 1) - 1
        break;
    case VK_RIGHT:
    case VK_NEXT:
    case VK_SPACE:
        NavigatePdfPage(st, st->currentPage0 + 2);
        break;
    case VK_HOME:
        NavigatePdfPage(st, 1);
        break;
    case VK_END:
        if (st->pageCount > 0) NavigatePdfPage(st, st->pageCount);
        break;
    case VK_ADD:
    case VK_OEM_PLUS:
        AdjustPdfZoom(st, +1);
        break;
    case VK_SUBTRACT:
    case VK_OEM_MINUS:
        AdjustPdfZoom(st, -1);
        break;
    case 'F':
    case '0':
        st->fitWidth = true;
        RequestPageRender(st, true);
        break;
    case VK_ESCAPE:
        if (st->hWnd) DestroyWindow(st->hWnd);
        break;
    default:
        break;
    }
}

inline void ExecuteToolbarButton(PdfViewerState* st, int id) {
    if (!st) return;
    switch (id) {
    case BTN_MANUALS:
        OpenPdfManualPickerMenu(st);
        break;
    case BTN_OPEN:
        OpenPdfFileBrowser(st);
        break;
    case BTN_TOPICS:
        OpenPdfTopicsMenu(st);
        break;
    case BTN_PREV:
        NavigatePdfPage(st, st->currentPage0);
        break;
    case BTN_NEXT:
        NavigatePdfPage(st, st->currentPage0 + 2);
        break;
    case BTN_ZOOM_OUT:
        AdjustPdfZoom(st, -1);
        break;
    case BTN_FIT:
        st->fitWidth = !st->fitWidth;
        if (!st->fitWidth) {
            const CachedPdfPage* pg = FindBestCachedPage(st, st->currentPage0, st->targetRenderWidth);
            const double baseW = 794.0 * static_cast<double>(st->uiScale);
            st->zoomFactor = (pg && baseW > 1.0)
                ? static_cast<float>(static_cast<double>(pg->bmpW) / baseW)
                : 1.0f;
        }
        RequestPageRender(st, false);
        break;
    case BTN_ZOOM_IN:
        AdjustPdfZoom(st, +1);
        break;
    case BTN_STUDIO: {
        std::wstring args;
        if (!st->currentPath.empty()) {
            args = L"--open \"" + st->currentPath + L"\" --page " + std::to_wstring((std::max)(1, st->currentPage0 + 1));
        }
        LaunchDocumentStudioImpl(st->hWnd, args);
        break;
    }
    default:
        break;
    }
}

inline void PaintPdfToolbar(PdfViewerState* st, HDC hdc, const RECT& cli) {
    const int w = cli.right - cli.left;
    const int h = st->toolbarRc.bottom;
    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP hMem = CreateCompatibleBitmap(hdc, (std::max)(1, w), (std::max)(1, h));
    HGDIOBJ oldBmp = SelectObject(memDc, hMem);

    RECT bg{ 0, 0, w, h };
    CasioTheme::DrawTranslucentHeaderStrip(memDc, bg, true);

    const float sc = st->uiScale;
    const int pad = CasioTheme::UiToDevice(4, sc);
    const float pillR = static_cast<float>(CasioTheme::UiToDevice(14, sc));

    // guías de interfaz material translúcido 2.0 floating grouped toolbar capsules (never glass-on-glass)
    RECT grpDocs{
        st->btnRects[BTN_MANUALS].left - pad, st->btnRects[BTN_MANUALS].top - pad / 2,
        st->btnRects[BTN_TOPICS].right + pad, st->btnRects[BTN_TOPICS].bottom + pad / 2
    };
    RECT grpNav{
        st->btnRects[BTN_PREV].left - pad, st->btnRects[BTN_PREV].top - pad / 2,
        st->btnRects[BTN_NEXT].right + pad, st->btnRects[BTN_NEXT].bottom + pad / 2
    };
    RECT grpZoom{
        st->btnRects[BTN_ZOOM_OUT].left - pad, st->btnRects[BTN_ZOOM_OUT].top - pad / 2,
        st->btnRects[BTN_ZOOM_IN].right + pad, st->btnRects[BTN_ZOOM_IN].bottom + pad / 2
    };
    CasioTheme::DrawFloatingGlassCapsule(memDc, grpDocs, 1.0f, -1.0f, false);
    CasioTheme::DrawFloatingGlassCapsule(memDc, grpNav, 1.0f, -1.0f, false);
    CasioTheme::DrawFloatingGlassCapsule(memDc, grpZoom, 1.0f, -1.0f, false);

    HFONT fUi = CasioTheme::CreateUiFont(CasioTheme::UiToDevice(13, sc), true);
    HFONT fSmall = CasioTheme::CreateUiFont(CasioTheme::UiToDevice(12, sc), false);

    auto drawBtn = [&](int id, const std::wstring& lbl, int variant, bool active) {
        CasioTheme::DrawStudioButton(
            memDc, st->btnRects[id], lbl, fUi, variant, active, st->hoverBtn == id, pillR);
    };

    drawBtn(BTN_MANUALS, L"Manuales", CasioTheme::BTN_TOOLBAR_GHOST, false);
    drawBtn(BTN_OPEN, L"Abrir...", CasioTheme::BTN_TOOLBAR_GHOST, false);

    if (!st->topics.empty()) {
        wchar_t tbuf[48]{};
        swprintf_s(tbuf, L"Temas (%zu)", st->topics.size());
        drawBtn(BTN_TOPICS, tbuf, CasioTheme::BTN_TOOLBAR_GHOST, true);
    } else {
        drawBtn(BTN_TOPICS, L"Temas", CasioTheme::BTN_TOOLBAR_GHOST, false);
    }

    drawBtn(BTN_PREV, L"<", CasioTheme::BTN_TOOLBAR_GHOST, false);
    drawBtn(BTN_NEXT, L">", CasioTheme::BTN_TOOLBAR_GHOST, false);

    // Chrome alrededor del cuadro editable de pagina.
    CasioTheme::FillRoundRectAA(
        memDc, st->editBoxChromeRc,
        static_cast<float>(CasioTheme::UiToDevice(7, sc)),
        CasioTheme::StudioInputBg(),
        CasioTheme::StudioBorderStrong(),
        1.0f);

    wchar_t totalBuf[32]{};
    if (st->pageCount > 0) {
        swprintf_s(totalBuf, L"/ %d", st->pageCount);
    } else {
        wcscpy_s(totalBuf, L"/ --");
    }
    SetBkMode(memDc, TRANSPARENT);
    SetTextColor(memDc, CasioTheme::StudioTextSec());
    HGDIOBJ oldF = SelectObject(memDc, fSmall);
    DrawTextW(memDc, totalBuf, -1, &st->pageTotalRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    drawBtn(BTN_ZOOM_OUT, L"-", CasioTheme::BTN_TOOLBAR_GHOST, false);
    if (st->fitWidth) {
        drawBtn(BTN_FIT, L"Ajustar", CasioTheme::BTN_TOOLBAR_GHOST, true);
    } else {
        wchar_t zbuf[32]{};
        swprintf_s(zbuf, L"%d%%", static_cast<int>(std::lround(st->zoomFactor * 100.0f)));
        drawBtn(BTN_FIT, zbuf, CasioTheme::BTN_TOOLBAR_GHOST, false);
    }
    drawBtn(BTN_ZOOM_IN, L"+", CasioTheme::BTN_TOOLBAR_GHOST, false);
    drawBtn(BTN_STUDIO, L"Estudio PDF", CasioTheme::BTN_PRIMARY, false);

    if (st->titleBadgeRc.right - st->titleBadgeRc.left > CasioTheme::UiToDevice(80, sc)) {
        SetTextColor(memDc, CasioTheme::StudioTextMuted());
        RECT tr = st->titleBadgeRc;
        DrawTextW(memDc, st->docTitle.c_str(), -1, &tr,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    SelectObject(memDc, oldF);
    DeleteObject(fUi);
    DeleteObject(fSmall);

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(hMem);
    DeleteDC(memDc);
}

inline LRESULT CALLBACK PdfCanvasWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<PdfViewerState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN:
        if (st) {
            SetFocus(hWnd);
            st->dragging = true;
            st->dragStartPt = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            st->dragStartScrollX = st->scrollX;
            st->dragStartScrollY = st->scrollY;
            SetCapture(hWnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (st && st->dragging) {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            st->scrollX = st->dragStartScrollX - (pt.x - st->dragStartPt.x);
            st->scrollY = st->dragStartScrollY - (pt.y - st->dragStartPt.y);
            UpdateCanvasScrollbars(st);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (st && st->dragging) {
            st->dragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_VSCROLL:
        if (st) {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hWnd, SB_VERT, &si);
            int pos = si.nPos;
            int step = CasioTheme::UiToDevice(48, st->uiScale);
            switch (LOWORD(wParam)) {
            case SB_LINEUP: pos -= step; break;
            case SB_LINEDOWN: pos += step; break;
            case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
            case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
            case SB_THUMBTRACK: pos = si.nTrackPos; break;
            case SB_TOP: pos = si.nMin; break;
            case SB_BOTTOM: pos = si.nMax; break;
            default: break;
            }
            st->scrollY = pos;
            UpdateCanvasScrollbars(st);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_HSCROLL:
        if (st) {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hWnd, SB_HORZ, &si);
            int pos = si.nPos;
            int step = CasioTheme::UiToDevice(48, st->uiScale);
            switch (LOWORD(wParam)) {
            case SB_LINELEFT: pos -= step; break;
            case SB_LINERIGHT: pos += step; break;
            case SB_PAGELEFT: pos -= static_cast<int>(si.nPage); break;
            case SB_PAGERIGHT: pos += static_cast<int>(si.nPage); break;
            case SB_THUMBTRACK: pos = si.nTrackPos; break;
            default: break;
            }
            st->scrollX = pos;
            UpdateCanvasScrollbars(st);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st) {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            const WORD keys = GET_KEYSTATE_WPARAM(wParam);
            if ((keys & MK_CONTROL) != 0) {
                AdjustPdfZoom(st, (delta > 0) ? +1 : -1);
            } else if ((keys & MK_SHIFT) != 0) {
                st->scrollX -= (delta / WHEEL_DELTA) * CasioTheme::UiToDevice(72, st->uiScale);
                UpdateCanvasScrollbars(st);
                InvalidateRect(hWnd, nullptr, FALSE);
            } else {
                st->scrollY -= (delta / WHEEL_DELTA) * CasioTheme::UiToDevice(96, st->uiScale);
                UpdateCanvasScrollbars(st);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (st) {
            HandlePdfKeyShortcut(st, wParam);
        }
        return 0;

    case WM_SIZE:
        if (st) {
            if (st->fitWidth && !st->currentPath.empty()) {
                int wantedW = ComputeDesiredRenderWidth(st);
                if (std::abs(wantedW - st->targetRenderWidth) > 28) {
                    RequestPageRender(st, false);
                }
            }
            UpdateCanvasScrollbars(st);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        const int vw = (std::max)(1, static_cast<int>(rc.right - rc.left));
        const int vh = (std::max)(1, static_cast<int>(rc.bottom - rc.top));

        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP hBack = CreateCompatibleBitmap(hdc, vw, vh);
        HGDIOBJ oldBack = SelectObject(memDc, hBack);

        HBRUSH brStage = CreateSolidBrush(CasioTheme::StudioStageBg());
        FillRect(memDc, &rc, brStage);
        DeleteObject(brStage);

        if (st) {
            const CachedPdfPage* pg = FindBestCachedPage(st, st->currentPage0, st->targetRenderWidth);
            if (pg && pg->bmp) {
                const int pad = 28;
                const int drawW = st->fitWidth ? (std::min)(pg->bmpW, (std::max)(160, vw - 44)) : pg->bmpW;
                const int drawH = (drawW == pg->bmpW)
                    ? pg->bmpH
                    : (std::max)(1, static_cast<int>(std::lround(
                          static_cast<double>(pg->bmpH) * static_cast<double>(drawW) /
                          static_cast<double>((std::max)(1, pg->bmpW)))));

                const int contentW = drawW + pad * 2;
                const int originX = (contentW < vw) ? ((vw - drawW) / 2) : (pad - st->scrollX);
                const int originY = ((drawH + pad * 2) < vh) ? ((vh - drawH) / 2) : (pad - st->scrollY);

                RECT paperRc{ originX, originY, originX + drawW, originY + drawH };
                CasioTheme::DrawSoftShadow(memDc, paperRc, 2.0f, 4);

                HDC bmpDc = CreateCompatibleDC(memDc);
                HGDIOBJ oldBmp = SelectObject(bmpDc, pg->bmp.Get());
                if (drawW == pg->bmpW && drawH == pg->bmpH) {
                    BitBlt(memDc, originX, originY, drawW, drawH, bmpDc, 0, 0, SRCCOPY);
                } else {
                    SetStretchBltMode(memDc, HALFTONE);
                    SetBrushOrgEx(memDc, 0, 0, nullptr);
                    StretchBlt(memDc, originX, originY, drawW, drawH, bmpDc, 0, 0, pg->bmpW, pg->bmpH, SRCCOPY);
                }
                SelectObject(bmpDc, oldBmp);
                DeleteDC(bmpDc);

                HPEN pBorder = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
                HGDIOBJ oldPen = SelectObject(memDc, pBorder);
                HGDIOBJ oldBr = SelectObject(memDc, GetStockObject(NULL_BRUSH));
                Rectangle(memDc, paperRc.left - 1, paperRc.top - 1, paperRc.right + 1, paperRc.bottom + 1);
                SelectObject(memDc, oldBr);
                SelectObject(memDc, oldPen);
                DeleteObject(pBorder);
            } else {
                SetBkMode(memDc, TRANSPARENT);
                SetTextColor(memDc, (st->initFailed || FAILED(st->lastError))
                    ? CasioTheme::StudioRoseText() : CasioTheme::StudioTextSec());
                HFONT fMsg = CasioTheme::CreateUiFont(CasioTheme::UiToDevice(15, st->uiScale), true);
                HGDIOBJ oldF = SelectObject(memDc, fMsg);
                std::wstring statusText = st->initFailed
                    ? (st->initErrorText.empty() ? L"Error inicializando el visor PDF." : st->initErrorText)
                    : (FAILED(st->lastError)
                        ? L"No se pudo renderizar el documento PDF."
                        : (st->isLoading ? L"Renderizando p\u00E1gina..." : L"Seleccione un manual o archivo PDF."));
                RECT tr = rc;
                DrawTextW(memDc, statusText.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(memDc, oldF);
                DeleteObject(fMsg);
            }
        }

        BitBlt(hdc, 0, 0, vw, vh, memDc, 0, 0, SRCCOPY);
        SelectObject(memDc, oldBack);
        DeleteObject(hBack);
        DeleteDC(memDc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

inline LRESULT CALLBACK PdfViewerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* binding = reinterpret_cast<PdfWndBinding*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    PdfViewerState* st = binding ? binding->state.get() : nullptr;
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        binding = static_cast<PdfWndBinding*>(cs->lpCreateParams);
        st = binding ? binding->state.get() : nullptr;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(binding));
        if (!st) return -1;
        st->hWnd = hWnd;
        {
            std::lock_guard<std::mutex> rlk(st->resultMtx);
            st->workerNotifyHwnd = hWnd;
        }

        CasioTheme::ApplyDwmDarkTitleBar(hWnd, CasioTheme::g_studioDarkTheme);

        HINSTANCE hInst = GetModuleHandleW(nullptr);
        st->hPageEdit = CreateWindowExW(
            0, L"EDIT", L"1",
            WS_CHILD | WS_VISIBLE | ES_CENTER | ES_NUMBER,
            0, 0, 48, 22, hWnd, reinterpret_cast<HMENU>(101), hInst, nullptr);
        if (st->hPageEdit) {
            SetWindowLongPtrW(st->hPageEdit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            st->prevEditProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(st->hPageEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PdfPageEditSubclassProc)));
            st->hEditFont = CasioTheme::CreateUiFont(13, true);
            st->editFontScale = 1.0f;
            SendMessageW(st->hPageEdit, WM_SETFONT, reinterpret_cast<WPARAM>(st->hEditFont), TRUE);
        }

        WNDCLASSW wcCanvas{};
        wcCanvas.style = CS_HREDRAW | CS_VREDRAW;
        wcCanvas.lpfnWndProc = PdfCanvasWndProc;
        wcCanvas.hInstance = hInst;
        wcCanvas.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcCanvas.lpszClassName = L"CasioNativePdfCanvas";
        RegisterClassW(&wcCanvas);

        st->hCanvas = CreateWindowExW(
            0, L"CasioNativePdfCanvas", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
            0, 52, 800, 600, hWnd, reinterpret_cast<HMENU>(102), hInst, nullptr);
        if (st->hCanvas) {
            SetWindowLongPtrW(st->hCanvas, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            CasioTheme::EnableThemeScrollbar(st->hCanvas, CasioTheme::g_studioDarkTheme);
        }

        st->hWakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!st->hWakeEvent) {
            st->initFailed = true;
            st->lastError = HRESULT_FROM_WIN32(GetLastError());
            st->initErrorText = L"Error creando evento de sincronizaci\u00F3n del visor PDF.";
        } else {
            auto* workerArg = new std::shared_ptr<PdfViewerState>(binding->state);
            st->hWorkerThread = CreateThread(nullptr, 0, PdfWorkerThreadProc, workerArg, 0, nullptr);
            if (!st->hWorkerThread) {
                delete workerArg;
                CloseHandle(st->hWakeEvent);
                st->hWakeEvent = nullptr;
                st->initFailed = true;
                st->lastError = HRESULT_FROM_WIN32(GetLastError());
                st->initErrorText = L"Error iniciando hilo de renderizado PDF.";
            }
        }
        LayoutPdfViewer(st);
        return 0;
    }

    case WM_CTLCOLOREDIT:
        if (reinterpret_cast<HWND>(lParam) == (st ? st->hPageEdit : nullptr)) {
            return CasioTheme::HandleStudioEditCtlColor(wParam);
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);

    case WM_SIZE:
        LayoutPdfViewer(st);
        if (st) InvalidateRect(hWnd, &st->toolbarRc, FALSE);
        return 0;

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            SetWindowPos(hWnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        LayoutPdfViewer(st);
        if (st) {
            if (!st->currentPath.empty()) {
                RequestPageRender(st, false);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            if (st->hCanvas) InvalidateRect(st->hCanvas, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (st) {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int hit = -1;
            for (int i = 0; i < BTN_COUNT; ++i) {
                if (PtInRect(&st->btnRects[i], pt)) {
                    hit = i;
                    break;
                }
            }
            if (hit != st->hoverBtn) {
                st->hoverBtn = hit;
                InvalidateRect(hWnd, &st->toolbarRc, FALSE);
            }
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;

    case WM_MOUSELEAVE:
        if (st && st->hoverBtn != -1) {
            st->hoverBtn = -1;
            InvalidateRect(hWnd, &st->toolbarRc, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (st) {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            for (int i = 0; i < BTN_COUNT; ++i) {
                if (PtInRect(&st->btnRects[i], pt)) {
                    ExecuteToolbarButton(st, i);
                    return 0;
                }
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (st) {
            HandlePdfKeyShortcut(st, wParam);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st && st->hCanvas) {
            SendMessageW(st->hCanvas, WM_MOUSEWHEEL, wParam, lParam);
        }
        return 0;

    case WM_COMMAND:
        if (st && HIWORD(wParam) == EN_KILLFOCUS && reinterpret_cast<HWND>(lParam) == st->hPageEdit) {
            CommitPageEditJump(st);
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);

    case PdfViewerInternal::WM_PDF_WORKER_RESULT:
        if (st) {
            ProcessPendingWorkerResults(st);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        if (st) {
            PaintPdfToolbar(st, hdc, rc);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (st) {
            st->cancelRequested.store(true, std::memory_order_release);
            {
                std::lock_guard<std::mutex> rlk(st->resultMtx);
                st->workerNotifyHwnd = nullptr;
                st->pendingResults.clear();
            }
            {
                std::lock_guard<std::mutex> lk(st->reqMtx);
                st->workerQuit = true;
            }
            if (st->hWakeEvent) SetEvent(st->hWakeEvent);
            if (st->hWorkerThread) {
                const DWORD waitRc = WaitForSingleObject(st->hWorkerThread, 1500);
                CloseHandle(st->hWorkerThread);
                st->hWorkerThread = nullptr;
                if (waitRc == WAIT_TIMEOUT) {
                    // Worker still holds shared_ptr<PdfViewerState> and keeps hWakeEvent valid until exit.
                    st->workerTimedOutOnClose.store(true, std::memory_order_release);
                } else if (st->hWakeEvent) {
                    CloseHandle(st->hWakeEvent);
                    st->hWakeEvent = nullptr;
                }
            } else if (st->hWakeEvent) {
                CloseHandle(st->hWakeEvent);
                st->hWakeEvent = nullptr;
            }
            if (st->hPageEdit && st->prevEditProc) {
                SetWindowLongPtrW(st->hPageEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(st->prevEditProc));
            }
            if (st->hEditFont) {
                DeleteObject(st->hEditFont);
                st->hEditFont = nullptr;
            }
            ClearPageCache(st);
            if (g_hPdfViewerWnd == hWnd) {
                g_hPdfViewerWnd = nullptr;
            }
            st->hWnd = nullptr;
        }
        return 0;

    case WM_NCDESTROY:
        if (binding) {
            delete binding;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

} // namespace PdfViewerUi

inline void SyncPdfViewerTheme() {
    HWND hWnd = PdfViewerUi::g_hPdfViewerWnd;
    if (!hWnd || !IsWindow(hWnd)) return;
    PdfViewerUi::PdfViewerState* st = PdfViewerUi::GetPdfViewerStateFromWnd(hWnd);
    CasioTheme::ApplyDwmDarkTitleBar(hWnd, CasioTheme::g_studioDarkTheme);
    if (st && st->hCanvas) {
        CasioTheme::EnableThemeScrollbar(st->hCanvas, CasioTheme::g_studioDarkTheme);
        InvalidateRect(st->hCanvas, nullptr, TRUE);
    }
    if (st && st->hPageEdit) {
        InvalidateRect(st->hPageEdit, nullptr, TRUE);
    }
    InvalidateRect(hWnd, nullptr, TRUE);
}

inline HWND CreateNativePdfViewerWindow(
    const std::wstring& pdfPath,
    int startPage = 1,
    HWND ownerHwnd = nullptr,
    const std::wstring& manualsDir = L"",
    const std::vector<PdfTopicShortcut>* topicsOpt = nullptr,
    bool showWindow = true,
    DWORD simulatedWorkerDelayMs = 0,
    std::size_t maxCacheBytes = 128ull * 1024ull * 1024ull,
    bool reuseSingleton = true)
{
    if (pdfPath.empty() || !PathFileExistsW(pdfPath.c_str())) {
        if (showWindow) {
            MessageBoxW(ownerHwnd,
                (L"No se encontr\u00F3 el archivo PDF en:\n" + pdfPath).c_str(),
                L"Visor Nativo PDF", MB_ICONWARNING | MB_OK);
        }
        return nullptr;
    }

    if (reuseSingleton && PdfViewerUi::g_hPdfViewerWnd && IsWindow(PdfViewerUi::g_hPdfViewerWnd)) {
        PdfViewerUi::PdfViewerState* st = PdfViewerUi::GetPdfViewerStateFromWnd(PdfViewerUi::g_hPdfViewerWnd);
        if (st) {
            if (!manualsDir.empty()) st->manualsDir = manualsDir;
            st->maxCacheBytes = maxCacheBytes;
            PdfViewerUi::SwitchPdfDocument(st, pdfPath, startPage, topicsOpt);
        }
        if (showWindow) {
            ShowWindow(PdfViewerUi::g_hPdfViewerWnd, SW_RESTORE);
            SetForegroundWindow(PdfViewerUi::g_hPdfViewerWnd);
        }
        return PdfViewerUi::g_hPdfViewerWnd;
    }

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PdfViewerUi::PdfViewerWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CasioNativePdfViewerWnd";
    if (ownerHwnd && IsWindow(ownerHwnd)) {
        wc.hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(ownerHwnd, GCLP_HICON));
    }
    RegisterClassW(&wc);

    auto sp = std::make_shared<PdfViewerUi::PdfViewerState>();
    sp->manualsDir = manualsDir.empty() ? PdfViewerUi::ResolveDefaultManualsDirForPdfViewer() : manualsDir;
    sp->simulatedWorkerDelayMs = simulatedWorkerDelayMs;
    sp->maxCacheBytes = maxCacheBytes;
    auto* binding = new PdfViewerUi::PdfWndBinding{ sp };

    HMONITOR hMon = (ownerHwnd && IsWindow(ownerHwnd))
        ? MonitorFromWindow(ownerHwnd, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    int workX = 80, workY = 40, workW = 1280, workH = 800;
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        workX = mi.rcWork.left;
        workY = mi.rcWork.top;
        workW = mi.rcWork.right - mi.rcWork.left;
        workH = mi.rcWork.bottom - mi.rcWork.top;
    }
    const float sc = CasioTheme::ComputeDialogUiScale(ownerHwnd, 1020, 780);
    int winW = (std::min)(workW - 40, CasioTheme::UiToDevice(1020, sc));
    int winH = (std::min)(workH - 40, CasioTheme::UiToDevice(800, sc));
    int winX = workX + (workW - winW) / 2;
    int winY = workY + (workH - winH) / 2;

    HWND hWnd = CreateWindowExW(
        0,
        L"CasioNativePdfViewerWnd",
        L"Visor Nativo PDF \u2014 CASIO",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        winX, winY, winW, winH,
        nullptr, nullptr, hInst, binding);
    if (!hWnd) {
        delete binding;
        return nullptr;
    }

    if (reuseSingleton) {
        PdfViewerUi::g_hPdfViewerWnd = hWnd;
    }
    PdfViewerUi::SwitchPdfDocument(sp.get(), pdfPath, startPage, topicsOpt);
    if (showWindow) {
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
        if (sp->hCanvas) SetFocus(sp->hCanvas);
    }
    return hWnd;
}

inline void OpenNativePdfViewer(
    const std::wstring& pdfPath,
    int startPage = 1,
    HWND ownerHwnd = nullptr,
    const std::wstring& manualsDir = L"",
    const std::vector<PdfTopicShortcut>* topicsOpt = nullptr)
{
    CreateNativePdfViewerWindow(pdfPath, startPage, ownerHwnd, manualsDir, topicsOpt, true, 0, 128ull * 1024ull * 1024ull, true);
}

inline bool LaunchDocumentStudio(HWND owner, const std::wstring& args) {
    return PdfViewerUi::LaunchDocumentStudioImpl(owner, args);
}
