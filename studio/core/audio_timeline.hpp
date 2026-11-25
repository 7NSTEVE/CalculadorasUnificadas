#pragma once

#include "document_model.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace DocStudio {

struct AudioBookmark {
    std::string labelUtf8;
    std::uint64_t timestampMs = 0;
};

struct TranscriptSegment {
    std::uint64_t startMs = 0;
    std::uint64_t endMs = 0;
    double rmsDb = -60.0;
    bool isSilence = true;
    std::string textUtf8;
};

struct SpeakerTranscriptSegment {
    std::uint64_t startMs = 0;
    std::uint64_t endMs = 0;
    std::string speakerIdUtf8; // e.g. "Hablante 1", "Hablante 2"
    std::string textUtf8;
    double pitchEstimateHz = 0.0;
    double rmsDb = -60.0;
};

struct MeetingMinutesReport {
    bool valid = false;
    bool recurringSeries = false;
    std::string eventUidUtf8;
    std::string eventTitleUtf8;
    std::string eventDateIsoUtf8;
    std::string organizerUtf8;
    std::vector<std::string> attendeesUtf8;
    std::string executiveSummaryUtf8;
    std::vector<std::string> actionItemsUtf8;
    std::string combinedMarkdownUtf8;
};

struct MicCaptureReport {
    bool hardwareUsed = false;
    bool fallbackUsed = false;
    bool unplugRecovered = false;
    std::uint64_t capturedMs = 0;
    std::string deviceNameUtf8;
    std::string statusNoteUtf8;
};

// Pure C++17 16-bit PCM WAV codec + real OS mic capture (Win32 waveIn / Linux ALSA-PulseAudio)
// + synchronized Ink Replay timeline, bookmarks, clamped offset, offline segmentation/transcript,
// and synced HTML5 tutorial export.
class AudioTimelineSession {
public:
    AudioTimelineSession() = default;
    ~AudioTimelineSession();
    AudioTimelineSession(const AudioTimelineSession&) = delete;
    AudioTimelineSession& operator=(const AudioTimelineSession&) = delete;

    static bool EncodeWavPcm16(
        const std::vector<std::int16_t>& samples,
        std::uint32_t sampleRateHz,
        std::vector<std::uint8_t>& outWavBytes);

    static bool DecodeWavPcm16(
        const std::uint8_t* wavBytes,
        std::size_t size,
        std::vector<std::int16_t>& outSamples,
        std::uint32_t& outSampleRateHz);

    void StartRecording(std::uint32_t sampleRateHz = 16000);
    void AppendSamples(const std::int16_t* samples, std::size_t count);
    void AppendSyntheticTone(double freqHz, std::uint64_t durationMs, std::int16_t amplitude = 6000);
    void StopRecording();

    // Real OS microphone capture (Win32 waveIn / Linux arecord/parec) for durationMs.
    // Gracefully falls back to quiet PCM if no physical capture device is attached or
    // simulateUnplug is true (preserving partial audio safely without crashing).
    MicCaptureReport CaptureFromMicrophone(
        std::uint64_t durationMs = 400,
        std::uint32_t sampleRateHz = 16000,
        bool simulateUnplug = false);

    // Non-blocking continuous asynchronous microphone streaming (F-153, WinMM waveIn 4-buffer ring)
    bool StartContinuousMicRecording(std::uint32_t sampleRateHz = 16000, std::string* outStatusUtf8 = nullptr);
    MicCaptureReport StopContinuousMicRecording();
    bool IsContinuousRecording() const noexcept { return asyncRecording_.load(std::memory_order_acquire); }

    // Real speaker hardware PCM playback synchronized with Ink Replay (F-155, WinMM waveOut)
    bool StartAudioPlayback(std::uint64_t startCursorMs = 0, std::string* outErr = nullptr);
    void StopAudioPlayback();
    bool IsAudioPlayingHardware() const noexcept { return asyncPlaying_.load(std::memory_order_acquire); }
    std::uint64_t GetHardwarePlaybackCursorMs() const noexcept;

    bool LoadFromWavBytes(const std::uint8_t* wavBytes, std::size_t size);
    bool LoadFromWavFile(const std::string& pathUtf8, std::string* outErr = nullptr);
    bool ExportWavBytes(std::vector<std::uint8_t>& outWavBytes) const;
    bool ExportWavFile(const std::string& pathUtf8, std::string* outErr = nullptr) const;

    bool IsRecording() const noexcept { return recording_; }
    bool IsPlaying() const noexcept { return playing_; }
    void SetPlaying(bool play) noexcept { playing_ = play; }

    std::uint32_t SampleRateHz() const noexcept { return sampleRateHz_; }
    std::uint64_t DurationMs() const noexcept;
    std::uint64_t CursorMs() const noexcept { return cursorMs_; }
    void SeekMs(std::uint64_t ms) noexcept;
    std::uint64_t AdvancePlaybackMs(std::uint64_t deltaMs) noexcept;

    // Offset & Bookmarks/Chapters (clamped safely to audio duration)
    std::int64_t OffsetMs() const noexcept { return offsetMs_; }
    std::int64_t SetOffsetMsClamped(std::int64_t requestedOffsetMs) noexcept;
    void AddBookmark(const std::string& labelUtf8, std::uint64_t timestampMs);
    const std::vector<AudioBookmark>& Bookmarks() const noexcept { return bookmarks_; }

    // Real ASR transcription (whisper.cpp CLI if configured, or Windows SAPI 5.4 speech recognition on WAV)
    // Falls back honestly to acoustic VAD segmentation when no speech recognition engine/model is installed.
    void SetWhisperCliConfig(const std::string& whisperExePathUtf8, const std::string& whisperModelPathUtf8) {
        whisperExePathUtf8_ = whisperExePathUtf8;
        whisperModelPathUtf8_ = whisperModelPathUtf8;
    }
    const std::string& WhisperExePath() const noexcept { return whisperExePathUtf8_; }
    const std::string& WhisperModelPath() const noexcept { return whisperModelPathUtf8_; }

    std::vector<TranscriptSegment> TranscribeOffline() const;

    // Self-contained synced Voice + Ink + Casio KeyLog HTML5 tutorial export
    bool ExportSyncedTutorialHtml5(
        const DocumentModel& doc,
        const std::string& outHtmlPathUtf8,
        std::string* outErr = nullptr) const;

    // Normalized [0..1] amplitude peaks per bin for waveform UI / PDF card rendering
    std::vector<float> ComputeEnvelopeBins(std::size_t numBins) const;

    const std::vector<std::int16_t>& Samples() const noexcept { return samples_; }

    // M6 Extended Audio: Pause/Resume, Playback Speed (0.5x..2.0x), +/-10s skip, Track Merge, Noise Reduction,
    // Multi-Speaker Diarization, Transcript Search & Jump, and .ics Calendar Meeting Notes Generator
    void PauseRecording() noexcept;
    void ResumeRecording() noexcept;
    bool IsPaused() const noexcept { return paused_; }

    void SetPlaybackSpeed(double factor) noexcept;
    double PlaybackSpeed() const noexcept { return playbackSpeed_; }
    std::uint64_t SkipRelativeMs(std::int64_t deltaMs) noexcept;

    bool MergeWithTrack(const AudioTimelineSession& other);
    void ApplyNoiseReductionHighPass();

    std::vector<SpeakerTranscriptSegment> DiarizeMultiSpeakerTranscript(
        const std::vector<std::string>& customUtterances = {}) const;

    bool SearchTranscriptAndSeek(
        const std::string& queryUtf8,
        const std::vector<SpeakerTranscriptSegment>& segments,
        std::uint64_t* outMatchedTimestampMs = nullptr);

    static MeetingMinutesReport GenerateCalendarMeetingNotesFromIcs(
        const std::string& icsContentUtf8,
        const std::vector<SpeakerTranscriptSegment>& segments,
        const std::string& writtenNotesUtf8 = "");

private:
    bool recording_ = false;
    bool paused_ = false;
    bool playing_ = false;
    double playbackSpeed_ = 1.0;
    std::uint32_t sampleRateHz_ = 16000;
    std::uint64_t cursorMs_ = 0;
    std::int64_t offsetMs_ = 0;
    std::vector<std::int16_t> samples_;
    std::vector<AudioBookmark> bookmarks_;

    mutable std::mutex audioMutex_;
    std::atomic<bool> asyncRecording_{ false };
    std::atomic<bool> stopRecRequested_{ false };
    std::atomic<bool> recHardwareUsed_{ false };
    std::string recDeviceNameUtf8_;
    std::thread recThread_;

    std::atomic<bool> asyncPlaying_{ false };
    std::atomic<bool> stopPlayRequested_{ false };
    std::atomic<std::uint64_t> hwPlaybackCursorMs_{ 0 };
    std::thread playThread_;
    std::string whisperExePathUtf8_;
    std::string whisperModelPathUtf8_;
};

} // namespace DocStudio
