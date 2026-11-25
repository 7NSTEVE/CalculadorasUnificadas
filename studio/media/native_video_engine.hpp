#pragma once
// ============================================================================
// doc_studio/media/native_video_engine.hpp
// 100% Native Windows Media Foundation Video Engine (Zero GPU 3D context,
// Zero external DLLs, Zero idle CPU/GPU when paused).
// Also includes Safe External URL validation for LinkCard & inline links.
// ============================================================================

#include "../core/doc_types.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

// Strict allow-list URL validator (permits ONLY http://, https://, mailto:).
// Blocks javascript:, file://, vbscript:, cmd:, etc. before ShellExecuteW.
bool IsSafeExternalLinkUrl(const std::string& urlUtf8) noexcept;

// Extract clean domain/host display string from URL (e.g. "https://desmos.com/calculator" -> "desmos.com")
std::string ExtractUrlDomainUtf8(const std::string& urlUtf8);

// Ensure Windows Media Foundation is initialized (ref-counted, safe to call repeatedly)
bool EnsureMediaFoundationStarted();

// Extract a single crisp poster frame + duration + resolution via Windows Media Foundation
// IMFSourceReader and immediately release the decoder (0% CPU/GPU afterwards).
bool ExtractVideoPosterAndMetadata(
    const std::wstring& videoFilePath,
    RasterImageBgra32& outPosterBgra,
    std::vector<std::uint8_t>& outPosterPng,
    std::uint64_t& outDurationMs,
    int& outVideoW,
    int& outVideoH,
    std::string* outErr = nullptr);

// Synthesize a valid decodable MP4/WMV test clip using native Media Foundation IMFSinkWriter
// (used by automated tests without needing external video fixtures).
bool WriteSyntheticTestVideoFile(
    const std::wstring& outPath,
    int widthPx,
    int heightPx,
    int fps,
    int frameCount,
    std::string* outErr = nullptr);

// On-demand, zero-idle video playback session (pure Windows Media Foundation RGB32 decode
// + WinMM PCM audio -> 0% GPU usage; releases decoder cleanly on Pause/Stop).
class NativeVideoPlayer {
public:
    NativeVideoPlayer();
    ~NativeVideoPlayer();

    NativeVideoPlayer(const NativeVideoPlayer&) = delete;
    NativeVideoPlayer& operator=(const NativeVideoPlayer&) = delete;

    bool Start(
        std::uint64_t objectId,
        const std::wstring& videoFilePath,
        std::uint64_t startPosMs = 0,
        double playbackRate = 1.0,
        bool muted = false,
        bool loop = false,
        std::string* outErr = nullptr);

    void Pause();
    void Stop();
    bool SeekMs(std::uint64_t targetMs);
    void SetMuted(bool muted);
    void SetPlaybackRate(double rate);
    void SetLoop(bool loop);

    // Advance decoding up to monotonic clock and return true if a new video frame was decoded.
    bool TickAndDecodeFrame();

    bool IsPlaying() const noexcept;
    std::uint64_t ActiveObjectId() const noexcept;
    std::uint64_t CurrentPositionMs() const noexcept;
    std::uint64_t DurationMs() const noexcept;
    double PlaybackRate() const noexcept;
    bool IsMuted() const noexcept;

    const RasterImageBgra32& CurrentFrameBgra() const noexcept;
    bool CaptureCurrentFramePng(std::vector<std::uint8_t>& outPng) const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace DocStudio
