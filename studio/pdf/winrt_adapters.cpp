#include "winrt_adapters.hpp"
#include "../core/utf_conv.hpp"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shcore.h>
#include <wincodec.h>
#include <roapi.h>
#include <winstring.h>
#include <robuffer.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.globalization.h>
#include <windows.storage.h>
#include <windows.storage.streams.h>
#include <windows.graphics.imaging.h>
#include <windows.media.ocr.h>
#include <windows.data.pdf.h>
#include <windows.ui.h>
#include <windows.ui.input.inking.h>
#include <windows.ui.input.inking.analysis.h>

namespace DocStudio {

namespace {

using Microsoft::WRL::ComPtr;

struct WinRtDynApi {
    HMODULE hCombase = nullptr;
    HMODULE hWinTypes = nullptr;
    HMODULE hShcore = nullptr;
    HMODULE hOle32 = nullptr;
    HMODULE hD2d1 = nullptr;
    HMODULE hD3d11 = nullptr;
    HMODULE hDxgi = nullptr;

    HRESULT(WINAPI* pRoInitialize)(RO_INIT_TYPE) = nullptr;
    void(WINAPI* pRoUninitialize)() = nullptr;
    HRESULT(WINAPI* pRoActivateInstance)(HSTRING, IInspectable**) = nullptr;
    HRESULT(WINAPI* pRoGetActivationFactory)(HSTRING, REFIID, void**) = nullptr;
    HRESULT(WINAPI* pWindowsCreateString)(PCNZWCH, UINT32, HSTRING*) = nullptr;
    HRESULT(WINAPI* pWindowsDeleteString)(HSTRING) = nullptr;
    PCWSTR(WINAPI* pWindowsGetStringRawBuffer)(HSTRING, UINT32*) = nullptr;
    HRESULT(WINAPI* pCreateStreamOnHGlobal)(HGLOBAL, BOOL, LPSTREAM*) = nullptr;
    HRESULT(WINAPI* pCreateRandomAccessStreamOverStream)(IStream*, BSOS_OPTIONS, REFIID, void**) = nullptr;
    HRESULT(WINAPI* pCreateRandomAccessStreamOnFile)(PCWSTR, DWORD, REFIID, void**) = nullptr;

    bool loaded = false;

    static WinRtDynApi& Instance() {
        static WinRtDynApi inst;
        return inst;
    }

private:
    WinRtDynApi() {
        hCombase = LoadLibraryW(L"combase.dll");
        hWinTypes = LoadLibraryW(L"api-ms-win-core-winrt-string-l1-1-0.dll");
        hShcore = LoadLibraryW(L"shcore.dll");
        hOle32 = LoadLibraryW(L"ole32.dll");
        // Keep d2d1/d3d11/dxgi pinned with refcount >= 1 before Windows.Data.Pdf.dll
        // activates so CoFreeUnusedLibrariesEx / LdrShutdownProcess never unloads
        // d3d11/atidxx64 underneath Windows.Data.Pdf.dll's D3D11 device teardown.
        hDxgi = LoadLibraryW(L"dxgi.dll");
        hD3d11 = LoadLibraryW(L"d3d11.dll");
        hD2d1 = LoadLibraryW(L"d2d1.dll");
        if (!hCombase) hCombase = LoadLibraryW(L"api-ms-win-core-winrt-l1-1-0.dll");

        if (hCombase) {
            pRoInitialize = reinterpret_cast<decltype(pRoInitialize)>(GetProcAddress(hCombase, "RoInitialize"));
            pRoUninitialize = reinterpret_cast<decltype(pRoUninitialize)>(GetProcAddress(hCombase, "RoUninitialize"));
            pRoActivateInstance = reinterpret_cast<decltype(pRoActivateInstance)>(GetProcAddress(hCombase, "RoActivateInstance"));
            pRoGetActivationFactory = reinterpret_cast<decltype(pRoGetActivationFactory)>(GetProcAddress(hCombase, "RoGetActivationFactory"));
        }
        HMODULE strMod = hWinTypes ? hWinTypes : hCombase;
        if (strMod) {
            pWindowsCreateString = reinterpret_cast<decltype(pWindowsCreateString)>(GetProcAddress(strMod, "WindowsCreateString"));
            pWindowsDeleteString = reinterpret_cast<decltype(pWindowsDeleteString)>(GetProcAddress(strMod, "WindowsDeleteString"));
            pWindowsGetStringRawBuffer = reinterpret_cast<decltype(pWindowsGetStringRawBuffer)>(GetProcAddress(strMod, "WindowsGetStringRawBuffer"));
        }
        if (hShcore) {
            pCreateRandomAccessStreamOverStream = reinterpret_cast<decltype(pCreateRandomAccessStreamOverStream)>(GetProcAddress(hShcore, "CreateRandomAccessStreamOverStream"));
            pCreateRandomAccessStreamOnFile = reinterpret_cast<decltype(pCreateRandomAccessStreamOnFile)>(GetProcAddress(hShcore, "CreateRandomAccessStreamOnFile"));
        }
        if (hOle32) {
            pCreateStreamOnHGlobal = reinterpret_cast<decltype(pCreateStreamOnHGlobal)>(GetProcAddress(hOle32, "CreateStreamOnHGlobal"));
        }
        loaded = pRoInitialize && pRoActivateInstance && pRoGetActivationFactory &&
                 pWindowsCreateString && pWindowsDeleteString && pWindowsGetStringRawBuffer &&
                 pCreateStreamOnHGlobal && pCreateRandomAccessStreamOverStream && pCreateRandomAccessStreamOnFile;
    }
};

struct ScopedHString {
    HSTRING h = nullptr;
    explicit ScopedHString(const wchar_t* s) {
        auto& api = WinRtDynApi::Instance();
        if (api.pWindowsCreateString && s) {
            api.pWindowsCreateString(s, static_cast<UINT32>(wcslen(s)), &h);
        }
    }
    ~ScopedHString() {
        auto& api = WinRtDynApi::Instance();
        if (h && api.pWindowsDeleteString) api.pWindowsDeleteString(h);
    }
    ScopedHString(const ScopedHString&) = delete;
    ScopedHString& operator=(const ScopedHString&) = delete;
};

std::string HStringToUtf8(HSTRING hs) {
    if (!hs) return {};
    auto& api = WinRtDynApi::Instance();
    if (!api.pWindowsGetStringRawBuffer) return {};
    UINT32 len = 0;
    PCWSTR raw = api.pWindowsGetStringRawBuffer(hs, &len);
    if (!raw || len == 0) return {};
    return WideToUtf8(std::wstring(raw, raw + len));
}

class MtaDispatcher {
public:
    static MtaDispatcher& Instance() {
        static MtaDispatcher inst;
        return inst;
    }

    ~MtaDispatcher() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        if (th_.joinable()) {
            // Normal teardown calls Shutdown() via ClearOpenDocumentCache() before RoUninitialize;
            // detaching here prevents CRT exit deadlocks if a caller exits without clearing.
            th_.detach();
        }
    }

    template <typename Fn>
    void Dispatch(Fn&& fn) {
        if (started_ && std::this_thread::get_id() == workerTid_) {
            fn();
            return;
        }
        std::mutex doneMtx;
        std::condition_variable doneCv;
        bool done = false;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (!started_) {
                stop_ = false;
                started_ = true;
                th_ = std::thread(&MtaDispatcher::WorkerLoop, this);
                workerTid_ = th_.get_id();
            }
            queue_.push_back([&]() {
                try {
                    fn();
                } catch (...) {
                }
                {
                    std::lock_guard<std::mutex> dlk(doneMtx);
                    done = true;
                }
                doneCv.notify_one();
            });
        }
        cv_.notify_one();
        std::unique_lock<std::mutex> dlk(doneMtx);
        doneCv.wait(dlk, [&]() { return done; });
    }

    void Shutdown() {
        std::thread toJoin;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (!started_) return;
            stop_ = true;
            started_ = false;
            workerTid_ = std::thread::id{};
            toJoin = std::move(th_);
        }
        cv_.notify_all();
        if (toJoin.joinable()) {
            if (std::this_thread::get_id() == toJoin.get_id()) {
                toJoin.detach();
            } else {
                toJoin.join();
            }
        }
    }

private:
    void WorkerLoop() {
        auto& api = WinRtDynApi::Instance();
        const HRESULT hrInit = (api.loaded && api.pRoInitialize) ? api.pRoInitialize(RO_INIT_MULTITHREADED) : E_FAIL;
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [&]() { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) break;
                task = std::move(queue_.front());
                queue_.pop_front();
            }
            if (task) task();
        }
        if (SUCCEEDED(hrInit) && api.pRoUninitialize) {
            api.pRoUninitialize();
        }
    }

    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> queue_;
    std::thread th_;
    std::thread::id workerTid_{};
    bool started_ = false;
    bool stop_ = false;
};

template <typename Fn>
void RunInMta(Fn&& fn) {
    auto& api = WinRtDynApi::Instance();
    if (!api.loaded) {
        fn();
        return;
    }
    // Always dispatch WinRT calls onto one persistent MTA worker thread so cached
    // Windows.Data.Pdf / OCR objects never outlive their apartment and tear down
    // cleanly in ClearOpenDocumentCache() before process exit.
    MtaDispatcher::Instance().Dispatch(std::forward<Fn>(fn));
}

template <typename OpT, typename HandlerIfaceT>
HRESULT WaitWinRtAsync(OpT* op, HANDLE doneEvent, const std::atomic<bool>* cancelFlag = nullptr, DWORD maxWaitMs = 20000) {
    DWORD waited = 0;
    while (waited < maxWaitMs) {
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            ComPtr<ABI::Windows::Foundation::IAsyncInfo> info;
            if (SUCCEEDED(op->QueryInterface(IID_PPV_ARGS(&info))) && info) info->Cancel();
            return E_ABORT;
        }
        const DWORD slice = 250;
        const DWORD rc = WaitForSingleObject(doneEvent, slice);
        if (rc == WAIT_OBJECT_0) return S_OK;
        waited += slice;
    }
    ComPtr<ABI::Windows::Foundation::IAsyncInfo> info;
    if (SUCCEEDED(op->QueryInterface(IID_PPV_ARGS(&info))) && info) info->Cancel();
    return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
}

template <typename AsyncOpT, typename ResultT>
class AsyncOpCompletedHandler final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ABI::Windows::Foundation::IAsyncOperationCompletedHandler<ResultT>,
          IAgileObject>
{
public:
    AsyncOpCompletedHandler() { event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
    ~AsyncOpCompletedHandler() override { if (event_) CloseHandle(event_); }
    HANDLE Event() const noexcept { return event_; }
    HRESULT STDMETHODCALLTYPE Invoke(ABI::Windows::Foundation::IAsyncOperation<ResultT>*, ABI::Windows::Foundation::AsyncStatus status) override {
        status_ = status;
        if (event_) SetEvent(event_);
        return S_OK;
    }
    ABI::Windows::Foundation::AsyncStatus Status() const noexcept { return status_; }
private:
    HANDLE event_ = nullptr;
    ABI::Windows::Foundation::AsyncStatus status_ = ABI::Windows::Foundation::AsyncStatus::Started;
};

class ActionCompletedHandler final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ABI::Windows::Foundation::IAsyncActionCompletedHandler,
          IAgileObject>
{
public:
    ActionCompletedHandler() { event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
    ~ActionCompletedHandler() override { if (event_) CloseHandle(event_); }
    HANDLE Event() const noexcept { return event_; }
    HRESULT STDMETHODCALLTYPE Invoke(ABI::Windows::Foundation::IAsyncAction*, ABI::Windows::Foundation::AsyncStatus status) override {
        status_ = status;
        if (event_) SetEvent(event_);
        return S_OK;
    }
private:
    HANDLE event_ = nullptr;
    ABI::Windows::Foundation::AsyncStatus status_ = ABI::Windows::Foundation::AsyncStatus::Started;
};

// Lightweight IBuffer + IBufferByteAccess wrapper over caller BGRA32 memory
class RawMemoryIBuffer final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::Storage::Streams::IBuffer,
          ::Windows::Storage::Streams::IBufferByteAccess,
          IAgileObject>
{
public:
    RawMemoryIBuffer(const std::uint8_t* ptr, UINT32 size)
        : bytes_(ptr, ptr + size), cap_(size), len_(size) {}

    HRESULT STDMETHODCALLTYPE get_Capacity(UINT32* value) override {
        if (!value) return E_POINTER;
        *value = cap_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_Length(UINT32* value) override {
        if (!value) return E_POINTER;
        *value = len_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_Length(UINT32 value) override {
        if (value > cap_) return E_INVALIDARG;
        len_ = value;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Buffer(byte** value) override {
        if (!value) return E_POINTER;
        *value = bytes_.data();
        return S_OK;
    }

private:
    std::vector<std::uint8_t> bytes_;
    UINT32 cap_ = 0;
    UINT32 len_ = 0;
};

struct CachedPdfKey {
    std::wstring path;
    std::uint64_t fileSize = 0;
    std::uint64_t mtime = 0;
    bool operator==(const CachedPdfKey& o) const noexcept {
        return fileSize == o.fileSize && mtime == o.mtime && path == o.path;
    }
};

struct OpenPdfEntry {
    CachedPdfKey key;
    ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> fileRas;
    ComPtr<ABI::Windows::Data::Pdf::IPdfDocument> doc;
    std::shared_ptr<std::mutex> docMtx;
};

class PdfDocCache {
public:
    static PdfDocCache& Instance() {
        static PdfDocCache c;
        return c;
    }

    ComPtr<ABI::Windows::Data::Pdf::IPdfDocument> Acquire(
        const std::wstring& path,
        std::shared_ptr<std::mutex>& outDocMtx,
        std::string* outErr)
    {
        outDocMtx.reset();
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
            if (outErr) *outErr = "No se encontr\xC3\xB3 el archivo PDF.";
            return nullptr;
        }
        CachedPdfKey key;
        key.path = path;
        key.fileSize = (static_cast<std::uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
        key.mtime = (static_cast<std::uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) | fad.ftLastWriteTime.dwLowDateTime;

        ComPtr<ABI::Windows::Data::Pdf::IPdfDocument> cachedHit;
        std::shared_ptr<std::mutex> cachedMtx;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            for (auto it = lru_.begin(); it != lru_.end(); ++it) {
                if (it->key == key) {
                    cachedHit = it->doc;
                    cachedMtx = it->docMtx;
                    lru_.splice(lru_.begin(), lru_, it);
                    break;
                }
            }
        }
        if (cachedHit && cachedMtx) {
            UINT32 chk = 0;
            bool alive = false;
            {
                std::lock_guard<std::mutex> docLk(*cachedMtx);
                alive = SUCCEEDED(cachedHit->get_PageCount(&chk));
            }
            if (alive) {
                outDocMtx = cachedMtx;
                return cachedHit;
            }
            std::lock_guard<std::mutex> lk(mtx_);
            for (auto it = lru_.begin(); it != lru_.end(); ++it) {
                if (it->key == key && it->doc.Get() == cachedHit.Get()) {
                    lru_.erase(it);
                    break;
                }
            }
        }

        // Open outside lock
        auto& api = WinRtDynApi::Instance();
        if (!api.loaded) {
            if (outErr) *outErr = "APIs WinRT de Windows.Data.Pdf no disponibles.";
            return nullptr;
        }

        ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> ras;
        HRESULT hr = api.pCreateRandomAccessStreamOnFile(
            path.c_str(),
            0 /* STGM_READ */,
            __uuidof(ABI::Windows::Storage::Streams::IRandomAccessStream),
            reinterpret_cast<void**>(ras.ReleaseAndGetAddressOf()));
        if (FAILED(hr) || !ras) {
            if (outErr) *outErr = "Error abriendo flujo WinRT para PDF.";
            return nullptr;
        }

        ScopedHString statClass(RuntimeClass_Windows_Data_Pdf_PdfDocument);
        ComPtr<ABI::Windows::Data::Pdf::IPdfDocumentStatics> statics;
        hr = api.pRoGetActivationFactory(
            statClass.h,
            __uuidof(ABI::Windows::Data::Pdf::IPdfDocumentStatics),
            reinterpret_cast<void**>(statics.ReleaseAndGetAddressOf()));
        if (FAILED(hr) || !statics) {
            if (outErr) *outErr = "Fallo al obtener PdfDocumentStatics.";
            return nullptr;
        }

        ComPtr<__FIAsyncOperation_1_Windows__CData__CPdf__CPdfDocument> loadOp;
        hr = statics->LoadFromStreamAsync(ras.Get(), loadOp.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !loadOp) {
            if (outErr) *outErr = "LoadFromStreamAsync fall\xC3\xB3.";
            return nullptr;
        }

        auto handler = Microsoft::WRL::Make<AsyncOpCompletedHandler<__FIAsyncOperation_1_Windows__CData__CPdf__CPdfDocument, ABI::Windows::Data::Pdf::PdfDocument*>>();
        loadOp->put_Completed(handler.Get());
        hr = WaitWinRtAsync<__FIAsyncOperation_1_Windows__CData__CPdf__CPdfDocument, ABI::Windows::Foundation::IAsyncOperationCompletedHandler<ABI::Windows::Data::Pdf::PdfDocument*>>(
            loadOp.Get(), handler->Event());
        if (FAILED(hr)) {
            if (outErr) *outErr = "Timeout o cancelaci\xC3\xB3n abriendo PDF.";
            return nullptr;
        }

        ComPtr<ABI::Windows::Data::Pdf::IPdfDocument> pdfDoc;
        hr = loadOp->GetResults(pdfDoc.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !pdfDoc) {
            if (outErr) *outErr = "El archivo no pudo decodificarse como PDF v\xC3\xA1lido.";
            return nullptr;
        }

        auto newMtx = std::make_shared<std::mutex>();
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto it = lru_.begin(); it != lru_.end(); ++it) {
            if (it->key == key && it->doc && it->docMtx) {
                ComPtr<ABI::Windows::Data::Pdf::IPdfDocument> existing = it->doc;
                outDocMtx = it->docMtx;
                lru_.splice(lru_.begin(), lru_, it);
                return existing;
            }
        }
        lru_.push_front({ std::move(key), std::move(ras), pdfDoc, newMtx });
        while (lru_.size() > 6) lru_.pop_back();
        outDocMtx = newMtx;
        return pdfDoc;
    }

    void Clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        lru_.clear();
    }

private:
    std::mutex mtx_;
    std::list<OpenPdfEntry> lru_;
};

void ApplyCropAndDeltaRotationBgra32(
    const std::uint8_t* src,
    int srcW,
    int srcH,
    const NormalizedCropRect& cropIn,
    int deltaRotDeg,
    RasterImageBgra32& out)
{
    NormalizedCropRect crop = cropIn;
    crop.Clamp();
    const int cx0 = std::clamp(static_cast<int>(std::floor(crop.left * srcW)), 0, srcW - 1);
    const int cy0 = std::clamp(static_cast<int>(std::floor(crop.top * srcH)), 0, srcH - 1);
    const int cx1 = std::clamp(static_cast<int>(std::ceil(crop.right * srcW)), cx0 + 1, srcW);
    const int cy1 = std::clamp(static_cast<int>(std::ceil(crop.bottom * srcH)), cy0 + 1, srcH);
    const int cw = cx1 - cx0;
    const int ch = cy1 - cy0;
    const int r = ((deltaRotDeg % 360) + 360) % 360;

    if (r == 0 && cx0 == 0 && cy0 == 0 && cw == srcW && ch == srcH) {
        out.width = srcW;
        out.height = srcH;
        out.pixels.assign(src, src + static_cast<std::size_t>(srcW) * srcH * 4u);
        return;
    }

    out.width = (r == 90 || r == 270) ? ch : cw;
    out.height = (r == 90 || r == 270) ? cw : ch;
    out.pixels.resize(static_cast<std::size_t>(out.width) * out.height * 4u);

    for (int y = 0; y < ch; ++y) {
        const std::uint8_t* srcRow = src + static_cast<std::size_t>(cy0 + y) * srcW * 4u;
        for (int x = 0; x < cw; ++x) {
            int dx = x, dy = y;
            if (r == 90) { dx = ch - 1 - y; dy = x; }
            else if (r == 180) { dx = cw - 1 - x; dy = ch - 1 - y; }
            else if (r == 270) { dx = y; dy = cw - 1 - x; }
            const std::uint8_t* sp = srcRow + static_cast<std::size_t>(cx0 + x) * 4u;
            std::uint8_t* dp = out.pixels.data() + (static_cast<std::size_t>(dy) * out.width + dx) * 4u;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = sp[3];
        }
    }
}

bool DecodeStreamToBgra32ViaWic(IStream* stream, std::vector<std::uint8_t>& outBgra, int& outW, int& outH) {
    LARGE_INTEGER zero{};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    UINT w = 0, h = 0;
    if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0) return false;
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(factory->CreateFormatConverter(&conv))) return false;
    if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;
    outW = static_cast<int>(w);
    outH = static_cast<int>(h);
    outBgra.resize(static_cast<std::size_t>(w) * h * 4u);
    return SUCCEEDED(conv->CopyPixels(nullptr, w * 4u, static_cast<UINT>(outBgra.size()), outBgra.data()));
}

} // namespace

void WinRtPdfRenderer::ClearOpenDocumentCache() {
    RunInMta([]() {
        PdfDocCache::Instance().Clear();
        // Flush WinRT/COM DLLs (notably Windows.Data.Pdf.dll and its D3D11/D2D1 GPU
        // device) cleanly while still inside the live MTA apartment and outside the
        // OS loader lock, giving threadpoolwinrt a brief window to finish async
        // release so atidxx64.dll/d3d11.dll never tear down during LdrShutdownProcess.
        for (int attempt = 0; attempt < 20; ++attempt) {
            CoFreeUnusedLibrariesEx(0, 0);
            CoFreeUnusedLibrariesEx(0, 0);
            if (!GetModuleHandleW(L"Windows.Data.Pdf.dll")) break;
            Sleep(25);
        }
    });
    MtaDispatcher::Instance().Shutdown();
    CoFreeUnusedLibrariesEx(0, 0);
}

bool WinRtPdfRenderer::GetPageCountAndSize(
    const std::wstring& pdfPath,
    int pageIndex0,
    int& outPageCount,
    double& outWidthPt,
    double& outHeightPt,
    std::string* outErr)
{
    bool ok = false;
    RunInMta([&]() {
        std::shared_ptr<std::mutex> docMtx;
        ComPtr<ABI::Windows::Data::Pdf::IPdfDocument> pdfDoc = PdfDocCache::Instance().Acquire(pdfPath, docMtx, outErr);
        if (!pdfDoc || !docMtx) return;
        std::lock_guard<std::mutex> docLk(*docMtx);
        UINT32 count = 0;
        if (FAILED(pdfDoc->get_PageCount(&count))) return;
        outPageCount = static_cast<int>(count);
        if (pageIndex0 < 0 || static_cast<UINT32>(pageIndex0) >= count) return;
        ComPtr<ABI::Windows::Data::Pdf::IPdfPage> page;
        if (FAILED(pdfDoc->GetPage(static_cast<UINT32>(pageIndex0), page.ReleaseAndGetAddressOf())) || !page) return;
        ABI::Windows::Foundation::Size sz{};
        if (SUCCEEDED(page->get_Size(&sz))) {
            outWidthPt = sz.Width;
            outHeightPt = sz.Height;
            ok = true;
        }
    });
    return ok;
}

bool WinRtPdfRenderer::RenderPageToBgra32(
    const std::wstring& pdfPath,
    int pageIndex0,
    int targetWidthPx,
    int deltaRotationDeg,
    const NormalizedCropRect& crop,
    RasterImageBgra32& outImage,
    const std::atomic<bool>* cancelFlag,
    std::string* outErr)
{
    bool ok = false;
    RunInMta([&]() {
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) return;
        auto& api = WinRtDynApi::Instance();
        if (!api.loaded) {
            if (outErr) *outErr = "WinRT PDF API no disponible.";
            return;
        }

        std::shared_ptr<std::mutex> docMtx;
        ComPtr<ABI::Windows::Data::Pdf::IPdfDocument> pdfDoc = PdfDocCache::Instance().Acquire(pdfPath, docMtx, outErr);
        if (!pdfDoc || !docMtx) return;

        ComPtr<IStream> memStream;
        {
            std::lock_guard<std::mutex> docLk(*docMtx);
            UINT32 count = 0;
            if (FAILED(pdfDoc->get_PageCount(&count)) || pageIndex0 < 0 || static_cast<UINT32>(pageIndex0) >= count) {
                if (outErr) *outErr = "\xC3\x8Dndice de p\xC3\xA1gina PDF fuera de rango.";
                return;
            }

            ComPtr<ABI::Windows::Data::Pdf::IPdfPage> page;
            if (FAILED(pdfDoc->GetPage(static_cast<UINT32>(pageIndex0), page.ReleaseAndGetAddressOf())) || !page) {
                if (outErr) *outErr = "GetPage fall\xC3\xB3.";
                return;
            }

            ABI::Windows::Foundation::Size sz{ 595.0f, 842.0f };
            page->get_Size(&sz);
            const double aspect = (sz.Width > 1.0f && sz.Height > 1.0f)
                ? static_cast<double>(sz.Height) / static_cast<double>(sz.Width) : 1.4142;

            int reqW = std::clamp(targetWidthPx, 48, 3000);
            int reqH = (std::max)(48, static_cast<int>(std::lround(reqW * aspect)));
            if (reqH > 3800) {
                reqH = 3800;
                reqW = (std::max)(48, static_cast<int>(std::lround(reqH / aspect)));
            }
            // Cap single page buffer <= 42 MiB
            constexpr std::uint64_t kMaxPageBytes = 42ull * 1024ull * 1024ull;
            const std::uint64_t estBytes = static_cast<std::uint64_t>(reqW) * reqH * 4ull;
            if (estBytes > kMaxPageBytes) {
                const double f = std::sqrt(static_cast<double>(kMaxPageBytes) / static_cast<double>(estBytes));
                reqW = (std::max)(48, static_cast<int>(reqW * f));
                reqH = (std::max)(48, static_cast<int>(reqH * f));
            }

            ComPtr<ABI::Windows::Data::Pdf::IPdfPageRenderOptions> opts;
            ScopedHString optsId(RuntimeClass_Windows_Data_Pdf_PdfPageRenderOptions);
            ComPtr<IInspectable> insp;
            if (SUCCEEDED(api.pRoActivateInstance(optsId.h, insp.ReleaseAndGetAddressOf())) && insp) {
                insp.As(&opts);
            }
            if (opts) {
                opts->put_DestinationWidth(static_cast<UINT32>(reqW));
                opts->put_DestinationHeight(static_cast<UINT32>(reqH));
                opts->put_BackgroundColor(ABI::Windows::UI::Color{ 255, 255, 255, 255 });
                opts->put_IsIgnoringHighContrast(TRUE);
            }

            if (FAILED(api.pCreateStreamOnHGlobal(nullptr, TRUE, memStream.ReleaseAndGetAddressOf())) || !memStream) return;
            ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> ras;
            if (FAILED(api.pCreateRandomAccessStreamOverStream(memStream.Get(), BSOS_DEFAULT, IID_PPV_ARGS(&ras))) || !ras) return;

            ComPtr<ABI::Windows::Foundation::IAsyncAction> act;
            const HRESULT hrRender = opts
                ? page->RenderWithOptionsToStreamAsync(ras.Get(), opts.Get(), act.ReleaseAndGetAddressOf())
                : page->RenderToStreamAsync(ras.Get(), act.ReleaseAndGetAddressOf());
            if (FAILED(hrRender) || !act) return;

            auto handler = Microsoft::WRL::Make<ActionCompletedHandler>();
            act->put_Completed(handler.Get());
            const HRESULT hrWait = WaitWinRtAsync<ABI::Windows::Foundation::IAsyncAction, ABI::Windows::Foundation::IAsyncActionCompletedHandler>(
                act.Get(), handler->Event(), cancelFlag);
            if (FAILED(hrWait)) {
                if (outErr) *outErr = "Renderizado cancelado o fuera de tiempo.";
                return;
            }
            act->GetResults();
        }

        std::vector<std::uint8_t> rawBgra;
        int w = 0, h = 0;
        if (!DecodeStreamToBgra32ViaWic(memStream.Get(), rawBgra, w, h)) {
            if (outErr) *outErr = "Error WIC decodificando imagen de p\xC3\xA1gina PDF.";
            return;
        }

        ApplyCropAndDeltaRotationBgra32(rawBgra.data(), w, h, crop, deltaRotationDeg, outImage);
        ok = outImage.Valid();
    });
    return ok;
}

bool WinRtPdfRenderer::EncodeBgra32ToPngBytes(
    const RasterImageBgra32& img,
    std::vector<std::uint8_t>& outPngBytes)
{
    if (!img.Valid()) return false;
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool didCo = SUCCEEDED(hrCo);
    bool ok = false;
    {
        ComPtr<IWICImagingFactory> factory;
        ComPtr<IStream> stream;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) {
            ComPtr<IWICBitmapEncoder> encoder;
            if (SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
                SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
                ComPtr<IWICBitmapFrameEncode> frame;
                if (SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
                    SUCCEEDED(frame->Initialize(nullptr)) &&
                    SUCCEEDED(frame->SetSize(static_cast<UINT>(img.width), static_cast<UINT>(img.height)))) {
                    WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
                    frame->SetPixelFormat(&pf);
                    if (SUCCEEDED(frame->WritePixels(
                            static_cast<UINT>(img.height),
                            static_cast<UINT>(img.width * 4),
                            static_cast<UINT>(img.pixels.size()),
                            const_cast<BYTE*>(img.pixels.data()))) &&
                        SUCCEEDED(frame->Commit()) &&
                        SUCCEEDED(encoder->Commit())) {
                        STATSTG st{};
                        if (SUCCEEDED(stream->Stat(&st, STATFLAG_NONAME)) && st.cbSize.QuadPart > 0) {
                            LARGE_INTEGER zero{};
                            stream->Seek(zero, STREAM_SEEK_SET, nullptr);
                            outPngBytes.resize(static_cast<std::size_t>(st.cbSize.QuadPart));
                            ULONG rd = 0;
                            if (SUCCEEDED(stream->Read(outPngBytes.data(), static_cast<ULONG>(outPngBytes.size()), &rd))) {
                                outPngBytes.resize(rd);
                                ok = !outPngBytes.empty();
                            }
                        }
                    }
                }
            }
        }
    }
    if (didCo) CoUninitialize();
    return ok;
}

std::vector<std::string> WinRtOcrAdapter::ListAvailableLanguageTags() {
    std::vector<std::string> tags;
    RunInMta([&]() {
        auto& api = WinRtDynApi::Instance();
        if (!api.loaded) return;
        ScopedHString cls(RuntimeClass_Windows_Media_Ocr_OcrEngine);
        ComPtr<ABI::Windows::Media::Ocr::IOcrEngineStatics> statics;
        if (FAILED(api.pRoGetActivationFactory(cls.h, __uuidof(ABI::Windows::Media::Ocr::IOcrEngineStatics), reinterpret_cast<void**>(statics.ReleaseAndGetAddressOf()))) || !statics) return;

        ComPtr<__FIVectorView_1_Windows__CGlobalization__CLanguage> langs;
        if (FAILED(statics->get_AvailableRecognizerLanguages(&langs)) || !langs) return;
        UINT32 size = 0;
        if (FAILED(langs->get_Size(&size))) return;
        for (UINT32 i = 0; i < size; ++i) {
            ComPtr<ABI::Windows::Globalization::ILanguage> lang;
            if (SUCCEEDED(langs->GetAt(i, &lang)) && lang) {
                HSTRING tagH = nullptr;
                if (SUCCEEDED(lang->get_LanguageTag(&tagH)) && tagH) {
                    tags.push_back(HStringToUtf8(tagH));
                    if (api.pWindowsDeleteString) api.pWindowsDeleteString(tagH);
                }
            }
        }
    });
    return tags;
}

OcrOutput WinRtOcrAdapter::RecognizeBgra32(
    int width,
    int height,
    const std::uint8_t* bgraBytes,
    const std::string& preferredLangTag)
{
    OcrOutput result;
    if (width <= 0 || height <= 0 || !bgraBytes) {
        result.errorUtf8 = "Imagen de entrada inv\xC3\xA1lida para OCR.";
        return result;
    }

    RunInMta([&]() {
        auto& api = WinRtDynApi::Instance();
        if (!api.loaded) {
            result.errorUtf8 = "WinRT OCR no disponible en este sistema.";
            return;
        }

        ScopedHString ocrCls(RuntimeClass_Windows_Media_Ocr_OcrEngine);
        ComPtr<ABI::Windows::Media::Ocr::IOcrEngineStatics> ocrStatics;
        if (FAILED(api.pRoGetActivationFactory(ocrCls.h, __uuidof(ABI::Windows::Media::Ocr::IOcrEngineStatics), reinterpret_cast<void**>(ocrStatics.ReleaseAndGetAddressOf()))) || !ocrStatics) {
            result.errorUtf8 = "No se pudo inicializar OcrEngineStatics.";
            return;
        }

        ComPtr<ABI::Windows::Media::Ocr::IOcrEngine> engine;
        ComPtr<__FIVectorView_1_Windows__CGlobalization__CLanguage> avail;
        ocrStatics->get_AvailableRecognizerLanguages(&avail);

        if (!preferredLangTag.empty() && avail) {
            UINT32 cnt = 0;
            avail->get_Size(&cnt);
            for (UINT32 i = 0; i < cnt; ++i) {
                ComPtr<ABI::Windows::Globalization::ILanguage> lang;
                if (SUCCEEDED(avail->GetAt(i, &lang)) && lang) {
                    HSTRING tagH = nullptr;
                    lang->get_LanguageTag(&tagH);
                    const std::string tag = HStringToUtf8(tagH);
                    if (tagH && api.pWindowsDeleteString) api.pWindowsDeleteString(tagH);
                    if (_stricmp(tag.c_str(), preferredLangTag.c_str()) == 0 ||
                        tag.rfind(preferredLangTag, 0) == 0) {
                        ocrStatics->TryCreateFromLanguage(lang.Get(), engine.ReleaseAndGetAddressOf());
                        if (engine) break;
                    }
                }
            }
        }
        if (!engine) {
            ocrStatics->TryCreateFromUserProfileLanguages(engine.ReleaseAndGetAddressOf());
        }
        if (!engine && avail) {
            UINT32 cnt = 0;
            avail->get_Size(&cnt);
            if (cnt > 0) {
                ComPtr<ABI::Windows::Globalization::ILanguage> lang0;
                if (SUCCEEDED(avail->GetAt(0, &lang0)) && lang0) {
                    ocrStatics->TryCreateFromLanguage(lang0.Get(), engine.ReleaseAndGetAddressOf());
                }
            }
        }
        if (!engine) {
            result.engineAvailable = false;
            result.errorUtf8 = "No hay paquetes de idioma OCR instalados en Windows.";
            return;
        }
        result.engineAvailable = true;

        ComPtr<ABI::Windows::Globalization::ILanguage> recLang;
        if (SUCCEEDED(engine->get_RecognizerLanguage(&recLang)) && recLang) {
            HSTRING tagH = nullptr;
            if (SUCCEEDED(recLang->get_LanguageTag(&tagH)) && tagH) {
                result.languageTagUtf8 = HStringToUtf8(tagH);
                if (api.pWindowsDeleteString) api.pWindowsDeleteString(tagH);
            }
        }

        ScopedHString sbCls(RuntimeClass_Windows_Graphics_Imaging_SoftwareBitmap);
        ComPtr<ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics> sbStatics;
        if (FAILED(api.pRoGetActivationFactory(sbCls.h, __uuidof(ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics), reinterpret_cast<void**>(sbStatics.ReleaseAndGetAddressOf()))) || !sbStatics) {
            result.errorUtf8 = "No se pudo obtener SoftwareBitmapStatics.";
            return;
        }

        const UINT32 byteLen = static_cast<UINT32>(width) * static_cast<UINT32>(height) * 4u;
        auto buf = Microsoft::WRL::Make<RawMemoryIBuffer>(bgraBytes, byteLen);
        ComPtr<ABI::Windows::Graphics::Imaging::ISoftwareBitmap> sb;
        if (FAILED(sbStatics->CreateCopyWithAlphaFromBuffer(
                buf.Get(),
                ABI::Windows::Graphics::Imaging::BitmapPixelFormat_Bgra8,
                static_cast<INT32>(width),
                static_cast<INT32>(height),
                ABI::Windows::Graphics::Imaging::BitmapAlphaMode_Premultiplied,
                sb.ReleaseAndGetAddressOf())) || !sb) {
            result.errorUtf8 = "No se pudo construir SoftwareBitmap BGRA8 para OCR.";
            return;
        }

        ComPtr<__FIAsyncOperation_1_Windows__CMedia__COcr__COcrResult> op;
        if (FAILED(engine->RecognizeAsync(sb.Get(), op.ReleaseAndGetAddressOf())) || !op) {
            result.errorUtf8 = "RecognizeAsync fall\xC3\xB3.";
            return;
        }

        auto handler = Microsoft::WRL::Make<AsyncOpCompletedHandler<__FIAsyncOperation_1_Windows__CMedia__COcr__COcrResult, ABI::Windows::Media::Ocr::OcrResult*>>();
        op->put_Completed(handler.Get());
        const HRESULT hrWait = WaitWinRtAsync<__FIAsyncOperation_1_Windows__CMedia__COcr__COcrResult, ABI::Windows::Foundation::IAsyncOperationCompletedHandler<ABI::Windows::Media::Ocr::OcrResult*>>(
            op.Get(), handler->Event());
        if (FAILED(hrWait)) {
            result.errorUtf8 = "Timeout ejecutando OCR.";
            return;
        }

        ComPtr<ABI::Windows::Media::Ocr::IOcrResult> ocrRes;
        if (FAILED(op->GetResults(ocrRes.ReleaseAndGetAddressOf())) || !ocrRes) {
            result.errorUtf8 = "No se obtuvo IOcrResult.";
            return;
        }

        ComPtr<__FIVectorView_1_Windows__CMedia__COcr__COcrLine> lines;
        if (SUCCEEDED(ocrRes->get_Lines(&lines)) && lines) {
            UINT32 lineCount = 0;
            lines->get_Size(&lineCount);
            for (UINT32 li = 0; li < lineCount; ++li) {
                ComPtr<ABI::Windows::Media::Ocr::IOcrLine> line;
                if (SUCCEEDED(lines->GetAt(li, &line)) && line) {
                    HSTRING lineTxtH = nullptr;
                    if (SUCCEEDED(line->get_Text(&lineTxtH)) && lineTxtH) {
                        std::string lineTxt = HStringToUtf8(lineTxtH);
                        if (api.pWindowsDeleteString) api.pWindowsDeleteString(lineTxtH);
                        if (!lineTxt.empty()) {
                            result.linesUtf8.push_back(lineTxt);
                            if (!result.fullTextUtf8.empty()) result.fullTextUtf8 += '\n';
                            result.fullTextUtf8 += lineTxt;
                        }
                    }
                    ComPtr<__FIVectorView_1_Windows__CMedia__COcr__COcrWord> words;
                    if (SUCCEEDED(line->get_Words(&words)) && words) {
                        UINT32 wordCount = 0;
                        words->get_Size(&wordCount);
                        for (UINT32 wi = 0; wi < wordCount; ++wi) {
                            ComPtr<ABI::Windows::Media::Ocr::IOcrWord> word;
                            if (SUCCEEDED(words->GetAt(wi, &word)) && word) {
                                HSTRING wtH = nullptr;
                                ABI::Windows::Foundation::Rect rc{};
                                word->get_Text(&wtH);
                                word->get_BoundingRect(&rc);
                                OcrWordBox wb;
                                wb.textUtf8 = HStringToUtf8(wtH);
                                if (wtH && api.pWindowsDeleteString) api.pWindowsDeleteString(wtH);
                                wb.pixelRect = { rc.X, rc.Y, rc.Width, rc.Height };
                                result.words.push_back(std::move(wb));
                            }
                        }
                    }
                }
            }
        }
        if (result.fullTextUtf8.empty()) {
            HSTRING fullH = nullptr;
            if (SUCCEEDED(ocrRes->get_Text(&fullH)) && fullH) {
                result.fullTextUtf8 = HStringToUtf8(fullH);
                if (api.pWindowsDeleteString) api.pWindowsDeleteString(fullH);
            }
        }
    });

    return result;
}

namespace {

// Lightweight IIterable<Point> / IIterator<Point> for WinRT IInkStrokeBuilder::CreateStroke
class PointIterator final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          __FIIterator_1_Windows__CFoundation__CPoint>
{
public:
    explicit PointIterator(std::vector<ABI::Windows::Foundation::Point> pts) : pts_(std::move(pts)) {}
    HRESULT STDMETHODCALLTYPE get_Current(ABI::Windows::Foundation::Point* current) override {
        if (!current) return E_POINTER;
        if (idx_ >= pts_.size()) return E_BOUNDS;
        *current = pts_[idx_];
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_HasCurrent(boolean* hasCurrent) override {
        if (!hasCurrent) return E_POINTER;
        *hasCurrent = (idx_ < pts_.size()) ? TRUE : FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE MoveNext(boolean* hasCurrent) override {
        if (idx_ < pts_.size()) ++idx_;
        if (hasCurrent) *hasCurrent = (idx_ < pts_.size()) ? TRUE : FALSE;
        return S_OK;
    }
private:
    std::vector<ABI::Windows::Foundation::Point> pts_;
    std::size_t idx_ = 0;
};

class PointIterable final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          __FIIterable_1_Windows__CFoundation__CPoint>
{
public:
    explicit PointIterable(std::vector<ABI::Windows::Foundation::Point> pts) : pts_(std::move(pts)) {}
    HRESULT STDMETHODCALLTYPE First(__FIIterator_1_Windows__CFoundation__CPoint** first) override {
        if (!first) return E_POINTER;
        auto it = Microsoft::WRL::Make<PointIterator>(pts_);
        return it.CopyTo(first);
    }
private:
    std::vector<ABI::Windows::Foundation::Point> pts_;
};

bool TryRecognizeStrokesViaWinRtInkAnalyzer(
    const std::vector<AnnotationObject>& strokes,
    double minX,
    double minY,
    std::string& outTextUtf8)
{
    outTextUtf8.clear();
    auto& api = WinRtDynApi::Instance();
    if (!api.loaded || !api.pRoActivateInstance) return false;

    bool ok = false;
    RunInMta([&]() {
        ScopedHString analyzerCls(RuntimeClass_Windows_UI_Input_Inking_Analysis_InkAnalyzer);
        ComPtr<IInspectable> inspAnalyzer;
        if (FAILED(api.pRoActivateInstance(analyzerCls.h, &inspAnalyzer)) || !inspAnalyzer) return;
        ComPtr<ABI::Windows::UI::Input::Inking::Analysis::IInkAnalyzer> analyzer;
        if (FAILED(inspAnalyzer.As(&analyzer)) || !analyzer) return;

        ScopedHString builderCls(RuntimeClass_Windows_UI_Input_Inking_InkStrokeBuilder);
        ComPtr<IInspectable> inspBuilder;
        if (FAILED(api.pRoActivateInstance(builderCls.h, &inspBuilder)) || !inspBuilder) return;
        ComPtr<ABI::Windows::UI::Input::Inking::IInkStrokeBuilder> builder;
        if (FAILED(inspBuilder.As(&builder)) || !builder) return;

        int addedCount = 0;
        for (const auto& s : strokes) {
            if (s.kind != ObjectKind::InkStroke || s.inkPoints.empty()) continue;
            std::vector<ABI::Windows::Foundation::Point> pts;
            pts.reserve(s.inkPoints.size());
            for (const auto& p : s.inkPoints) {
                ABI::Windows::Foundation::Point wp{};
                wp.X = static_cast<FLOAT>((p.x - minX) * 1.5 + 24.0);
                wp.Y = static_cast<FLOAT>((p.y - minY) * 1.5 + 24.0);
                pts.push_back(wp);
            }
            if (pts.size() == 1) pts.push_back({ pts[0].X + 1.0f, pts[0].Y + 1.0f });
            auto iterable = Microsoft::WRL::Make<PointIterable>(std::move(pts));
            ComPtr<ABI::Windows::UI::Input::Inking::IInkStroke> winStroke;
            if (SUCCEEDED(builder->CreateStroke(iterable.Get(), &winStroke)) && winStroke) {
                if (SUCCEEDED(analyzer->AddDataForStroke(winStroke.Get()))) {
                    ++addedCount;
                }
            }
        }
        if (addedCount == 0) return;

        ComPtr<__FIAsyncOperation_1_Windows__CUI__CInput__CInking__CAnalysis__CInkAnalysisResult> op;
        if (FAILED(analyzer->AnalyzeAsync(&op)) || !op) return;

        using InkAnalysisOpT = __FIAsyncOperation_1_Windows__CUI__CInput__CInking__CAnalysis__CInkAnalysisResult;
        using InkAnalysisHandlerT = ABI::Windows::Foundation::IAsyncOperationCompletedHandler<
            ABI::Windows::UI::Input::Inking::Analysis::InkAnalysisResult*>;
        auto handler = Microsoft::WRL::Make<AsyncOpCompletedHandler<
            InkAnalysisOpT,
            ABI::Windows::UI::Input::Inking::Analysis::InkAnalysisResult*>>();
        op->put_Completed(handler.Get());
        const HRESULT hrWait = WaitWinRtAsync<InkAnalysisOpT, InkAnalysisHandlerT>(op.Get(), handler->Event());
        if (FAILED(hrWait)) {
            return;
        }

        ComPtr<ABI::Windows::UI::Input::Inking::Analysis::IInkAnalysisResult> res;
        if (FAILED(op->GetResults(&res)) || !res) return;
        ABI::Windows::UI::Input::Inking::Analysis::InkAnalysisStatus st{};
        if (FAILED(res->get_Status(&st)) || st != ABI::Windows::UI::Input::Inking::Analysis::InkAnalysisStatus_Updated) return;

        ComPtr<ABI::Windows::UI::Input::Inking::Analysis::IInkAnalysisRoot> root;
        if (FAILED(analyzer->get_AnalysisRoot(&root)) || !root) return;
        HSTRING hText = nullptr;
        if (SUCCEEDED(root->get_RecognizedText(&hText)) && hText) {
            std::string txt = HStringToUtf8(hText);
            if (api.pWindowsDeleteString) api.pWindowsDeleteString(hText);
            // Trim whitespace
            while (!txt.empty() && std::isspace(static_cast<unsigned char>(txt.front()))) txt.erase(txt.begin());
            while (!txt.empty() && std::isspace(static_cast<unsigned char>(txt.back()))) txt.pop_back();
            if (!txt.empty()) {
                outTextUtf8 = std::move(txt);
                ok = true;
            }
        }
    });
    return ok;
}

} // namespace

HtrOutput WinRtInkHtrAdapter::RecognizeStrokes(
    const std::vector<AnnotationObject>& strokes,
    const std::string& preferredLangTag)
{
    HtrOutput out{};
    // Collect bounding box of all valid InkStroke points
    double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
    std::size_t strokeCount = 0;
    for (const auto& s : strokes) {
        if (s.kind != ObjectKind::InkStroke || s.inkPoints.empty()) continue;
        ++strokeCount;
        for (const auto& pt : s.inkPoints) {
            minX = (std::min)(minX, pt.x);
            minY = (std::min)(minY, pt.y);
            maxX = (std::max)(maxX, pt.x);
            maxY = (std::max)(maxY, pt.y);
        }
    }
    if (strokeCount == 0 || minX > maxX || minY > maxY) {
        out.errorUtf8 = "No hay trazos de tinta seleccionados para reconocimiento.";
        return out;
    }

    // 1. Primary path: Native WinRT Windows.UI.Input.Inking.Analysis.InkAnalyzer (real cursive & block HTR)
    std::string inkAnalyzed;
    if (TryRecognizeStrokesViaWinRtInkAnalyzer(strokes, minX, minY, inkAnalyzed) && !inkAnalyzed.empty()) {
        out.recognized = true;
        out.textUtf8 = inkAnalyzed;
        out.candidatesUtf8.push_back(inkAnalyzed);
        std::string mathNorm = inkAnalyzed;
        auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
            std::size_t p = 0;
            while ((p = s.find(from, p)) != std::string::npos) {
                s.replace(p, from.size(), to);
                p += to.size();
            }
        };
        replaceAll(mathNorm, "\xC3\x97", "*");
        replaceAll(mathNorm, "\xC3\xB7", "/");
        replaceAll(mathNorm, "\xE2\x88\x92", "-");
        out.mathLatexUtf8 = std::move(mathNorm);
        return out;
    }

    // 2. Fallback path: Rasterize strokes onto high-contrast white BGRA32 canvas for WinRT OCR recognition
    const double spanW = (std::max)(24.0, maxX - minX);
    const double spanH = (std::max)(24.0, maxY - minY);
    const double scale = std::clamp(220.0 / (std::max)(spanH, 32.0), 1.5, 4.5);
    const int pad = 32;
    const int imgW = std::clamp(static_cast<int>(std::ceil(spanW * scale)) + pad * 2, 128, 2200);
    const int imgH = std::clamp(static_cast<int>(std::ceil(spanH * scale)) + pad * 2, 96, 1200);
    const double effScale = (std::min)(
        static_cast<double>(imgW - pad * 2) / spanW,
        static_cast<double>(imgH - pad * 2) / spanH);

    std::vector<std::uint8_t> bgra(static_cast<std::size_t>(imgW) * imgH * 4u, 255u);
    auto drawDisk = [&](int cx, int cy, int r) {
        for (int dy = -r; dy <= r; ++dy) {
            const int py = cy + dy;
            if (py < 0 || py >= imgH) continue;
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy > r * r) continue;
                const int px = cx + dx;
                if (px < 0 || px >= imgW) continue;
                const std::size_t idx = (static_cast<std::size_t>(py) * imgW + px) * 4u;
                bgra[idx + 0] = 16u;
                bgra[idx + 1] = 16u;
                bgra[idx + 2] = 16u;
                bgra[idx + 3] = 255u;
            }
        }
    };

    for (const auto& s : strokes) {
        if (s.kind != ObjectKind::InkStroke || s.inkPoints.empty()) continue;
        const int radius = std::clamp(static_cast<int>(std::lround((std::max)(1.8, s.strokeWidth * 0.55) * effScale)), 2, 9);
        for (std::size_t i = 0; i < s.inkPoints.size(); ++i) {
            const double x0 = pad + (s.inkPoints[i].x - minX) * effScale;
            const double y0 = pad + (s.inkPoints[i].y - minY) * effScale;
            if (i == 0) {
                drawDisk(static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)), radius);
            } else {
                const double px0 = pad + (s.inkPoints[i - 1].x - minX) * effScale;
                const double py0 = pad + (s.inkPoints[i - 1].y - minY) * effScale;
                const double dist = std::hypot(x0 - px0, y0 - py0);
                const int steps = (std::max)(1, static_cast<int>(std::ceil(dist)));
                for (int k = 1; k <= steps; ++k) {
                    const double t = static_cast<double>(k) / static_cast<double>(steps);
                    drawDisk(
                        static_cast<int>(std::lround(px0 + (x0 - px0) * t)),
                        static_cast<int>(std::lround(py0 + (y0 - py0) * t)),
                        radius);
                }
            }
        }
    }

    const OcrOutput ocr = WinRtOcrAdapter::RecognizeBgra32(imgW, imgH, bgra.data(), preferredLangTag);
    if (!ocr.fullTextUtf8.empty()) {
        out.recognized = true;
        out.textUtf8 = ocr.fullTextUtf8;
        out.candidatesUtf8.push_back(ocr.fullTextUtf8);
        // Normalize common handwritten math glyph shortcuts for Formula conversion (F-045)
        std::string mathNorm = ocr.fullTextUtf8;
        auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
            std::size_t p = 0;
            while ((p = s.find(from, p)) != std::string::npos) {
                s.replace(p, from.size(), to);
                p += to.size();
            }
        };
        replaceAll(mathNorm, "\xC3\x97", "*");
        replaceAll(mathNorm, "\xC3\xB7", "/");
        replaceAll(mathNorm, "\xE2\x88\x92", "-");
        out.mathLatexUtf8 = std::move(mathNorm);
    } else {
        out.errorUtf8 = ocr.errorUtf8.empty()
            ? "No se detect\xC3\xB3 texto legible en los trazos seleccionados."
            : ocr.errorUtf8;
    }
    return out;
}

} // namespace DocStudio
