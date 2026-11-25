#pragma once

#include "doc_types.hpp"
#include "document_model.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

enum class PaperTintPreset : std::uint8_t {
    Porcelana = 0,
    SepiaCalido = 1,
    MentaSuave = 2,
    LavandaTenue = 3,
    NocheSuave = 4
};

enum class ColorBlindPaletteMode : std::uint8_t {
    Normal = 0,
    Protanopia = 1,
    Deuteranopia = 2,
    Tritanopia = 3
};

struct AccessibilityPreferences {
    bool readingRulerEnabled = false;
    int readingRulerHeightPx = 76;
    double readingRulerMaskOpacity = 0.42;
    bool dyslexiaFriendlyMode = false;
    PaperTintPreset paperTint = PaperTintPreset::Porcelana;
    ColorBlindPaletteMode colorBlindMode = ColorBlindPaletteMode::Normal;
    int uiFontScalePct = 100; // 100, 115, 130, 150
    bool enhancedFocusRing = false;
    int ttsRate = 0;          // SAPI range -6..+8
    int ttsVolume = 95;       // 0..100
    std::string ttsVoiceNameUtf8;
};

class AccessibilityEngine {
public:
    static const char* PaperTintLabelUtf8(PaperTintPreset preset) noexcept;
    static const char* ColorBlindModeLabelUtf8(ColorBlindPaletteMode mode) noexcept;

    // Ergonomic anti-fatigue paper surface tint (returns crisp white for Porcelana/HighContrast)
    static DocColor PaperTintSurfaceColor(PaperTintPreset preset, bool darkTheme, bool highContrast) noexcept;

    // Remaps chromatic colors to scientifically distinguishable Okabe-Ito palettes while preserving neutral inks
    static DocColor RemapColorForColorBlind(DocColor c, ColorBlindPaletteMode mode) noexcept;

    // Natural Spanish spoken verbalization for 2D math expressions and scientific symbols
    static std::string VerbalizeMathExpressionForSpeech(const std::string& exprUtf8);

    // Extracts clean speech text from selected object or active page/canvas
    static std::string ExtractReadableSpeechText(
        const DocumentModel& doc,
        std::size_t activePageIndex,
        std::uint64_t selectedObjectId = 0);
};

// Native Windows SAPI 5.4 asynchronous speech synthesizer (zero external runtime, strict COM cleanup)
class SpeechSynthesizerWin32 {
public:
    SpeechSynthesizerWin32();
    ~SpeechSynthesizerWin32();

    SpeechSynthesizerWin32(const SpeechSynthesizerWin32&) = delete;
    SpeechSynthesizerWin32& operator=(const SpeechSynthesizerWin32&) = delete;

    bool Initialize(std::string* outErr = nullptr);
    void Shutdown() noexcept;

    std::vector<std::string> EnumerateInstalledVoices() const;
    bool SelectVoiceByName(const std::string& voiceNameUtf8);
    void SetRate(int rateMinus10ToPlus10);
    void SetVolume(int volume0To100);

    bool SpeakAsync(const std::string& textUtf8, std::string* outErr = nullptr);
    void Pause();
    void Resume();
    void Stop();
    bool IsSpeaking() const;
    bool IsPaused() const noexcept { return paused_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool paused_ = false;
    int rate_ = 0;
    int volume_ = 95;
    std::string selectedVoiceUtf8_;
};

} // namespace DocStudio
