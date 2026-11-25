#include "native_video_engine.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wrl/client.h>
#endif

namespace DocStudio {

namespace {

std::string TrimAscii(const std::string& s) {
    std::size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    std::size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string LowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

#ifdef _WIN32
using Microsoft::WRL::ComPtr;

std::mutex g_mfInitMtx;
bool g_mfStarted = false;

bool CopyRgb32SampleToBgra32(
    IMFMediaBuffer* buf,
    UINT32 width,
    UINT32 height,
    LONG defaultStride,
    RasterImageBgra32& outImg)
{
    if (!buf || width == 0 || height == 0 || width > 4096 || height > 4096) return false;
    BYTE* data = nullptr;
    DWORD curLen = 0;
    LONG stride = defaultStride;
    ComPtr<IMF2DBuffer> buf2d;
    bool locked2d = false;
    if (SUCCEEDED(buf->QueryInterface(IID_PPV_ARGS(&buf2d))) && buf2d) {
        if (SUCCEEDED(buf2d->Lock2D(&data, &stride)) && data) {
            locked2d = true;
        }
    }
    if (!locked2d) {
        if (FAILED(buf->Lock(&data, nullptr, &curLen)) || !data) return false;
        if (stride == 0) stride = static_cast<LONG>(width * 4u);
    }

    const int w = static_cast<int>(width);
    const int h = static_cast<int>(height);
    const LONG absStride = (stride < 0) ? -stride : stride;
    if (absStride < static_cast<LONG>(width * 4u) ||
        (!locked2d && curLen > 0 && static_cast<std::size_t>(absStride) * static_cast<std::size_t>(h) > static_cast<std::size_t>(curLen))) {
        if (locked2d) buf2d->Unlock2D();
        else buf->Unlock();
        return false;
    }

    outImg.width = w;
    outImg.height = h;
    outImg.pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);

    // IMF2DBuffer::Lock2D always returns `data` pointing to scanline 0 (even when stride < 0).
    // Contiguous IMFMediaBuffer::Lock with stride < 0 starts at the bottom row instead.
    const BYTE* basePtr = (!locked2d && stride < 0)
        ? (data + static_cast<std::ptrdiff_t>(absStride) * (h - 1))
        : data;
    const std::ptrdiff_t stepBytes = static_cast<std::ptrdiff_t>(stride);

    for (int y = 0; y < h; ++y) {
        const BYTE* srcRow = basePtr + static_cast<std::ptrdiff_t>(y) * stepBytes;
        std::uint8_t* dstRow = outImg.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 4u;
        for (int x = 0; x < w; ++x) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + 0];
            dstRow[x * 4 + 1] = srcRow[x * 4 + 1];
            dstRow[x * 4 + 2] = srcRow[x * 4 + 2];
            dstRow[x * 4 + 3] = 255;
        }
    }

    if (locked2d) buf2d->Unlock2D();
    else buf->Unlock();
    return true;
}

bool ConfigureVideoReader(
    const std::wstring& path,
    ComPtr<IMFSourceReader>& outReader,
    UINT32& outW,
    UINT32& outH,
    LONG& outStride,
    std::uint64_t& outDurationMs,
    bool enableAudio,
    bool& outHasAudio,
    WAVEFORMATEX& outWaveFmt,
    std::string* outErr)
{
    outW = outH = 0;
    outStride = 0;
    outDurationMs = 0;
    outHasAudio = false;
    std::memset(&outWaveFmt, 0, sizeof(outWaveFmt));

    if (!EnsureMediaFoundationStarted()) {
        if (outErr) *outErr = "No se pudo inicializar Windows Media Foundation.";
        return false;
    }

    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(&attrs, 2);
    if (attrs) {
        attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        attrs->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE);
    }

    HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), attrs.Get(), &outReader);
    if (FAILED(hr) || !outReader) {
        if (outErr) *outErr = "Media Foundation no pudo abrir el archivo de video.";
        return false;
    }

    // Request RGB32 (BGRA layout in memory) software output -> 0% GPU 3D context needed
    ComPtr<IMFMediaType> outType;
    MFCreateMediaType(&outType);
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    hr = outReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, outType.Get());
    if (FAILED(hr)) {
        if (outErr) *outErr = "Formato de video no soportado por Media Foundation.";
        outReader.Reset();
        return false;
    }

    ComPtr<IMFMediaType> curType;
    if (SUCCEEDED(outReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &curType)) && curType) {
        MFGetAttributeSize(curType.Get(), MF_MT_FRAME_SIZE, &outW, &outH);
        UINT32 strideAttr = 0;
        if (SUCCEEDED(curType->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideAttr))) {
            outStride = static_cast<LONG>(strideAttr);
        } else {
            outStride = static_cast<LONG>(outW * 4u);
        }
    }

    PROPVARIANT var{};
    PropVariantInit(&var);
    if (SUCCEEDED(outReader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &var))) {
        if (var.vt == VT_UI8) {
            outDurationMs = static_cast<std::uint64_t>(var.uhVal.QuadPart / 10000ull);
        } else if (var.vt == VT_I8 && var.hVal.QuadPart > 0) {
            outDurationMs = static_cast<std::uint64_t>(var.hVal.QuadPart / 10000ll);
        }
    }
    PropVariantClear(&var);

    if (enableAudio) {
        ComPtr<IMFMediaType> audReq;
        MFCreateMediaType(&audReq);
        audReq->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        audReq->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        if (SUCCEEDED(outReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, audReq.Get()))) {
            ComPtr<IMFMediaType> audCur;
            if (SUCCEEDED(outReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &audCur)) && audCur) {
                WAVEFORMATEX* pwfx = nullptr;
                UINT32 cb = 0;
                if (SUCCEEDED(MFCreateWaveFormatExFromMFMediaType(audCur.Get(), &pwfx, &cb)) && pwfx) {
                    outWaveFmt = *pwfx;
                    CoTaskMemFree(pwfx);
                    outHasAudio = (outWaveFmt.nSamplesPerSec > 0 && outWaveFmt.nChannels > 0);
                }
            }
        }
    }
    return (outW > 0 && outH > 0);
}
#endif

} // namespace

bool IsSafeExternalLinkUrl(const std::string& urlUtf8) noexcept {
    const std::string t = TrimAscii(urlUtf8);
    if (t.size() < 6 || t.size() > 2048) return false;
    for (unsigned char c : t) {
        if (c < 0x20 || c == 0x7F) return false;
    }
    const std::string low = LowerAscii(t);
    if (low.rfind("https://", 0) == 0 && t.size() > 8) return true;
    if (low.rfind("http://", 0) == 0 && t.size() > 7) return true;
    if (low.rfind("mailto:", 0) == 0 && t.size() > 7 && t.find('@') != std::string::npos) return true;
    return false;
}

std::string ExtractUrlDomainUtf8(const std::string& urlUtf8) {
    const std::string t = TrimAscii(urlUtf8);
    const auto schemePos = t.find("://");
    std::size_t start = (schemePos == std::string::npos) ? 0u : (schemePos + 3u);
    std::size_t end = t.find_first_of("/?#", start);
    if (end == std::string::npos) end = t.size();
    std::string host = t.substr(start, end - start);
    if (host.rfind("www.", 0) == 0 && host.size() > 4) host = host.substr(4);
    return host.empty() ? t : host;
}

bool EnsureMediaFoundationStarted() {
#ifdef _WIN32
    std::lock_guard<std::mutex> lk(g_mfInitMtx);
    if (g_mfStarted) return true;
    const HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (SUCCEEDED(hr)) {
        g_mfStarted = true;
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool ExtractVideoPosterAndMetadata(
    const std::wstring& videoFilePath,
    RasterImageBgra32& outPosterBgra,
    std::vector<std::uint8_t>& outPosterPng,
    std::uint64_t& outDurationMs,
    int& outVideoW,
    int& outVideoH,
    std::string* outErr)
{
    outPosterBgra = {};
    outPosterPng.clear();
    outDurationMs = 0;
    outVideoW = 0;
    outVideoH = 0;

#ifdef _WIN32
    ComPtr<IMFSourceReader> reader;
    UINT32 w = 0, h = 0;
    LONG stride = 0;
    bool hasAud = false;
    WAVEFORMATEX wfx{};
    if (!ConfigureVideoReader(videoFilePath, reader, w, h, stride, outDurationMs, false, hasAud, wfx, outErr)) {
        return false;
    }
    outVideoW = static_cast<int>(w);
    outVideoH = static_cast<int>(h);

    // Seek slightly into video (up to 800 ms) so poster skips black intro frames
    if (outDurationMs > 1200ull) {
        PROPVARIANT pos{};
        PropVariantInit(&pos);
        pos.vt = VT_I8;
        pos.hVal.QuadPart = static_cast<LONGLONG>((std::min<std::uint64_t>)(outDurationMs / 5ull, 800ull)) * 10000ll;
        reader->SetCurrentPosition(GUID_NULL, pos);
        PropVariantClear(&pos);
    }

    bool gotFrame = false;
    for (int tries = 0; tries < 8 && !gotFrame; ++tries) {
        DWORD streamFlags = 0;
        LONGLONG ts = 0;
        ComPtr<IMFSample> sample;
        const HRESULT hr = reader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0, nullptr, &streamFlags, &ts, &sample);
        if (FAILED(hr) || (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
        if (!sample) continue;
        ComPtr<IMFMediaBuffer> buf;
        if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf)) && buf) {
            gotFrame = CopyRgb32SampleToBgra32(buf.Get(), w, h, stride, outPosterBgra);
        }
    }

    // Explicitly release reader immediately -> 0% CPU/GPU/handle retention
    reader.Reset();
    if (!gotFrame || !outPosterBgra.Valid()) {
        if (outErr) *outErr = "No se pudo decodificar un fotograma de vista previa del video.";
        return false;
    }

    WinRtPdfRenderer::EncodeBgra32ToPngBytes(outPosterBgra, outPosterPng);
    return true;
#else
    (void)videoFilePath;
    if (outErr) *outErr = "Media Foundation requiere Windows.";
    return false;
#endif
}

bool WriteSyntheticTestVideoFile(
    const std::wstring& outPath,
    int widthPx,
    int heightPx,
    int fps,
    int frameCount,
    std::string* outErr)
{
#ifdef _WIN32
    if (!EnsureMediaFoundationStarted()) {
        if (outErr) *outErr = "MFStartup fallo.";
        return false;
    }
    const UINT32 w = static_cast<UINT32>((std::max)(64, widthPx & ~3));
    const UINT32 h = static_cast<UINT32>((std::max)(64, heightPx & ~3));
    const UINT32 rate = static_cast<UINT32>(std::clamp(fps, 5, 60));
    const int nFrames = std::clamp(frameCount, 4, 180);

    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(&attrs, 1);
    if (attrs) attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);

    ComPtr<IMFSinkWriter> writer;
    HRESULT hr = MFCreateSinkWriterFromURL(outPath.c_str(), nullptr, attrs.Get(), &writer);
    if (FAILED(hr) || !writer) {
        if (outErr) *outErr = "MFCreateSinkWriterFromURL fallo.";
        return false;
    }

    ComPtr<IMFMediaType> outType;
    MFCreateMediaType(&outType);
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    outType->SetUINT32(MF_MT_AVG_BITRATE, 450000);
    outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, w, h);
    MFSetAttributeRatio(outType.Get(), MF_MT_FRAME_RATE, rate, 1);
    MFSetAttributeRatio(outType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    DWORD streamIdx = 0;
    hr = writer->AddStream(outType.Get(), &streamIdx);
    if (FAILED(hr)) {
        // Fallback to WMV3 if H264 encoder is unavailable on headless SKU
        outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_WMV3);
        hr = writer->AddStream(outType.Get(), &streamIdx);
        if (FAILED(hr)) {
            if (outErr) *outErr = "IMFSinkWriter::AddStream fallo.";
            return false;
        }
    }

    ComPtr<IMFMediaType> inType;
    MFCreateMediaType(&inType);
    inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(inType.Get(), MF_MT_FRAME_SIZE, w, h);
    MFSetAttributeRatio(inType.Get(), MF_MT_FRAME_RATE, rate, 1);
    MFSetAttributeRatio(inType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    hr = writer->SetInputMediaType(streamIdx, inType.Get(), nullptr);
    if (FAILED(hr)) {
        if (outErr) *outErr = "IMFSinkWriter::SetInputMediaType fallo.";
        return false;
    }
    if (FAILED(writer->BeginWriting())) {
        if (outErr) *outErr = "IMFSinkWriter::BeginWriting fallo.";
        return false;
    }

    const LONGLONG frameDur = 10000000ll / static_cast<LONGLONG>(rate);
    const DWORD cbFrame = w * h * 4u;
    for (int f = 0; f < nFrames; ++f) {
        ComPtr<IMFMediaBuffer> buf;
        if (FAILED(MFCreateMemoryBuffer(cbFrame, &buf)) || !buf) return false;
        BYTE* dst = nullptr;
        if (FAILED(buf->Lock(&dst, nullptr, nullptr)) || !dst) return false;
        const int shift = (f * 10) % static_cast<int>(w);
        for (UINT32 y = 0; y < h; ++y) {
            for (UINT32 x = 0; x < w; ++x) {
                const std::size_t o = (static_cast<std::size_t>(y) * w + x) * 4u;
                const bool bar = (static_cast<int>(x) >= shift && static_cast<int>(x) < shift + 28);
                dst[o + 0] = bar ? 245 : static_cast<BYTE>((x * 220u) / w);
                dst[o + 1] = bar ? 130 : static_cast<BYTE>((y * 200u) / h);
                dst[o + 2] = bar ? 30  : static_cast<BYTE>(40 + (f * 8) % 180);
                dst[o + 3] = 255;
            }
        }
        buf->Unlock();
        buf->SetCurrentLength(cbFrame);

        ComPtr<IMFSample> sample;
        MFCreateSample(&sample);
        sample->AddBuffer(buf.Get());
        sample->SetSampleTime(static_cast<LONGLONG>(f) * frameDur);
        sample->SetSampleDuration(frameDur);
        if (FAILED(writer->WriteSample(streamIdx, sample.Get()))) {
            if (outErr) *outErr = "IMFSinkWriter::WriteSample fallo.";
            return false;
        }
    }
    return SUCCEEDED(writer->Finalize());
#else
    (void)outPath; (void)widthPx; (void)heightPx; (void)fps; (void)frameCount;
    if (outErr) *outErr = "Media Foundation requiere Windows.";
    return false;
#endif
}

struct NativeVideoPlayer::Impl {
    std::uint64_t objectId = 0;
    std::wstring path;
    bool playing = false;
    bool muted = false;
    bool loop = false;
    double rate = 1.0;
    std::uint64_t durationMs = 0;
    std::uint64_t currentPosMs = 0;
    std::uint64_t basePosMs = 0;
    ULONGLONG startTickMs = 0;
    bool hasDecodedFrame = false;
    RasterImageBgra32 frameBgra;

#ifdef _WIN32
    ComPtr<IMFSourceReader> reader;
    UINT32 vidW = 0;
    UINT32 vidH = 0;
    LONG vidStride = 0;
    bool hasAudio = false;
    WAVEFORMATEX wfx{};
    HWAVEOUT hWaveOut = nullptr;
    struct AudioBuf {
        WAVEHDR hdr{};
        std::vector<BYTE> data;
    };
    std::vector<std::unique_ptr<AudioBuf>> audioRing;

    void CloseAudio() {
        if (hWaveOut) {
            waveOutReset(hWaveOut);
            for (auto& b : audioRing) {
                if (b && (b->hdr.dwFlags & WHDR_PREPARED)) {
                    waveOutUnprepareHeader(hWaveOut, &b->hdr, sizeof(WAVEHDR));
                }
            }
            waveOutClose(hWaveOut);
            hWaveOut = nullptr;
        }
        audioRing.clear();
    }

    void OpenAudioIfReady() {
        CloseAudio();
        if (!hasAudio || muted || std::fabs(rate - 1.0) > 0.05) return;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            hWaveOut = nullptr;
        }
    }

    void PumpAudioTo(std::uint64_t targetMs) {
        if (!reader || !hasAudio || !hWaveOut || muted) return;
        // Reclaim completed waveOut buffers
        audioRing.erase(std::remove_if(audioRing.begin(), audioRing.end(), [&](const std::unique_ptr<AudioBuf>& b) {
            if (!b) return true;
            if ((b->hdr.dwFlags & WHDR_DONE) != 0) {
                waveOutUnprepareHeader(hWaveOut, &b->hdr, sizeof(WAVEHDR));
                return true;
            }
            return false;
        }), audioRing.end());

        // Keep at most 6 queued PCM chunks ahead (~120-200 ms) for low latency & low RAM
        for (int k = 0; k < 4 && audioRing.size() < 6u; ++k) {
            DWORD flags = 0;
            LONGLONG ts = 0;
            ComPtr<IMFSample> sample;
            if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, &ts, &sample))) break;
            if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) || !sample) break;
            ComPtr<IMFMediaBuffer> buf;
            if (FAILED(sample->ConvertToContiguousBuffer(&buf)) || !buf) continue;
            BYTE* p = nullptr;
            DWORD len = 0;
            if (SUCCEEDED(buf->Lock(&p, nullptr, &len)) && p && len > 0) {
                auto ab = std::make_unique<AudioBuf>();
                ab->data.assign(p, p + len);
                buf->Unlock();
                ab->hdr.lpData = reinterpret_cast<LPSTR>(ab->data.data());
                ab->hdr.dwBufferLength = static_cast<DWORD>(ab->data.size());
                if (waveOutPrepareHeader(hWaveOut, &ab->hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR &&
                    waveOutWrite(hWaveOut, &ab->hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
                    audioRing.push_back(std::move(ab));
                }
            }
            const std::uint64_t audMs = (ts > 0) ? static_cast<std::uint64_t>(ts / 10000ll) : 0ull;
            if (audMs > targetMs + 160ull) break;
        }
    }

    void ReleaseDecoder() {
        CloseAudio();
        reader.Reset();
    }
#endif
};

NativeVideoPlayer::NativeVideoPlayer() : impl_(new Impl()) {}
NativeVideoPlayer::~NativeVideoPlayer() {
    Stop();
    delete impl_;
}

bool NativeVideoPlayer::Start(
    std::uint64_t objectId,
    const std::wstring& videoFilePath,
    std::uint64_t startPosMs,
    double playbackRate,
    bool muted,
    bool loop,
    std::string* outErr)
{
    Stop();
#ifdef _WIN32
    if (!ConfigureVideoReader(videoFilePath, impl_->reader, impl_->vidW, impl_->vidH, impl_->vidStride,
            impl_->durationMs, true, impl_->hasAudio, impl_->wfx, outErr)) {
        return false;
    }
    impl_->objectId = objectId;
    impl_->path = videoFilePath;
    impl_->rate = std::clamp(playbackRate, 0.25, 4.0);
    impl_->muted = muted;
    impl_->loop = loop;
    impl_->basePosMs = (impl_->durationMs > 0 && startPosMs >= impl_->durationMs) ? 0ull : startPosMs;
    impl_->currentPosMs = impl_->basePosMs;
    impl_->startTickMs = GetTickCount64();
    impl_->hasDecodedFrame = false;
    impl_->playing = true;

    if (impl_->basePosMs > 0) {
        PROPVARIANT pos{};
        PropVariantInit(&pos);
        pos.vt = VT_I8;
        pos.hVal.QuadPart = static_cast<LONGLONG>(impl_->basePosMs) * 10000ll;
        impl_->reader->SetCurrentPosition(GUID_NULL, pos);
        PropVariantClear(&pos);
    }
    impl_->OpenAudioIfReady();
    TickAndDecodeFrame();
    return true;
#else
    (void)objectId; (void)videoFilePath; (void)startPosMs; (void)playbackRate; (void)muted; (void)loop;
    if (outErr) *outErr = "Media Foundation requiere Windows.";
    return false;
#endif
}

void NativeVideoPlayer::Pause() {
    if (!impl_) return;
    impl_->playing = false;
    impl_->hasDecodedFrame = false;
#ifdef _WIN32
    impl_->ReleaseDecoder(); // Zero CPU/GPU/RAM retention while paused!
#endif
}

void NativeVideoPlayer::Stop() {
    if (!impl_) return;
    impl_->playing = false;
    impl_->hasDecodedFrame = false;
#ifdef _WIN32
    impl_->ReleaseDecoder();
#endif
    impl_->objectId = 0;
}

bool NativeVideoPlayer::SeekMs(std::uint64_t targetMs) {
    if (!impl_) return false;
    impl_->basePosMs = (impl_->durationMs > 0) ? (std::min)(targetMs, impl_->durationMs) : targetMs;
    impl_->currentPosMs = impl_->basePosMs;
    impl_->hasDecodedFrame = false;
#ifdef _WIN32
    impl_->startTickMs = GetTickCount64();
    if (!impl_->reader) return false;
    impl_->CloseAudio();
    PROPVARIANT pos{};
    PropVariantInit(&pos);
    pos.vt = VT_I8;
    pos.hVal.QuadPart = static_cast<LONGLONG>(impl_->basePosMs) * 10000ll;
    const HRESULT hr = impl_->reader->SetCurrentPosition(GUID_NULL, pos);
    PropVariantClear(&pos);
    impl_->OpenAudioIfReady();
    return SUCCEEDED(hr);
#else
    return false;
#endif
}

void NativeVideoPlayer::SetMuted(bool muted) {
    if (!impl_) return;
    impl_->muted = muted;
#ifdef _WIN32
    if (muted) impl_->CloseAudio();
    else if (impl_->playing) impl_->OpenAudioIfReady();
#endif
}

void NativeVideoPlayer::SetPlaybackRate(double rate) {
    if (!impl_) return;
#ifdef _WIN32
    impl_->basePosMs = impl_->currentPosMs;
    impl_->startTickMs = GetTickCount64();
    impl_->rate = std::clamp(rate, 0.25, 4.0);
    if (impl_->playing) impl_->OpenAudioIfReady();
#else
    impl_->rate = std::clamp(rate, 0.25, 4.0);
#endif
}

void NativeVideoPlayer::SetLoop(bool loop) {
    if (impl_) impl_->loop = loop;
}

bool NativeVideoPlayer::TickAndDecodeFrame() {
#ifdef _WIN32
    if (!impl_ || !impl_->playing || !impl_->reader) return false;
    const ULONGLONG nowMs = GetTickCount64();
    const double elapsed = static_cast<double>(nowMs - impl_->startTickMs) * impl_->rate;
    const std::uint64_t targetMs = impl_->basePosMs + static_cast<std::uint64_t>(elapsed);

    impl_->PumpAudioTo(targetMs);

    // Honor real frame timestamp pacing so <30fps videos do not fast-forward on 33ms ticks
    if (impl_->hasDecodedFrame && impl_->currentPosMs > targetMs) {
        return false;
    }

    bool decodedAny = false;
    for (int step = 0; step < 6; ++step) {
        DWORD flags = 0;
        LONGLONG ts = 0;
        ComPtr<IMFSample> sample;
        const HRESULT hr = impl_->reader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0, nullptr, &flags, &ts, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (impl_->loop) {
                SeekMs(0);
                break;
            }
            Pause();
            break;
        }
        if (!sample) continue;
        const std::uint64_t frameMs = (ts > 0) ? static_cast<std::uint64_t>(ts / 10000ll) : impl_->currentPosMs;
        ComPtr<IMFMediaBuffer> buf;
        if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buf)) && buf) {
            if (CopyRgb32SampleToBgra32(buf.Get(), impl_->vidW, impl_->vidH, impl_->vidStride, impl_->frameBgra)) {
                impl_->currentPosMs = frameMs;
                impl_->hasDecodedFrame = true;
                decodedAny = true;
            }
        }
        if (frameMs >= targetMs) break;
    }
    return decodedAny;
#else
    return false;
#endif
}

bool NativeVideoPlayer::IsPlaying() const noexcept { return impl_ && impl_->playing; }
std::uint64_t NativeVideoPlayer::ActiveObjectId() const noexcept { return impl_ ? impl_->objectId : 0; }
std::uint64_t NativeVideoPlayer::CurrentPositionMs() const noexcept { return impl_ ? impl_->currentPosMs : 0; }
std::uint64_t NativeVideoPlayer::DurationMs() const noexcept { return impl_ ? impl_->durationMs : 0; }
double NativeVideoPlayer::PlaybackRate() const noexcept { return impl_ ? impl_->rate : 1.0; }
bool NativeVideoPlayer::IsMuted() const noexcept { return impl_ && impl_->muted; }

const RasterImageBgra32& NativeVideoPlayer::CurrentFrameBgra() const noexcept {
    static const RasterImageBgra32 kEmpty{};
    return impl_ ? impl_->frameBgra : kEmpty;
}

bool NativeVideoPlayer::CaptureCurrentFramePng(std::vector<std::uint8_t>& outPng) const {
    outPng.clear();
    if (!impl_ || !impl_->frameBgra.Valid()) return false;
    return WinRtPdfRenderer::EncodeBgra32ToPngBytes(impl_->frameBgra, outPng);
}

} // namespace DocStudio
