#include "accessibility_engine.hpp"
#include "utf_conv.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sapi.h>
#include <wrl/client.h>
#endif

namespace DocStudio {

namespace {

void ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

bool IsWordChar(char ch) noexcept {
    const unsigned char u = static_cast<unsigned char>(ch);
    return std::isalnum(u) != 0 || ch == '_';
}

void ReplaceWord(std::string& s, const std::string& word, const std::string& replacement) {
    if (word.empty()) return;
    std::size_t pos = 0;
    while ((pos = s.find(word, pos)) != std::string::npos) {
        const bool leftOk = (pos == 0) || !IsWordChar(s[pos - 1]);
        const bool rightOk = (pos + word.size() >= s.size()) || !IsWordChar(s[pos + word.size()]);
        if (leftOk && rightOk) {
            s.replace(pos, word.size(), replacement);
            pos += replacement.size();
        } else {
            pos += word.size();
        }
    }
}

void RgbToHsv(std::uint8_t r8, std::uint8_t g8, std::uint8_t b8, double& hDeg, double& sat, double& val) noexcept {
    const double r = r8 / 255.0;
    const double g = g8 / 255.0;
    const double b = b8 / 255.0;
    const double mx = (std::max)({ r, g, b });
    const double mn = (std::min)({ r, g, b });
    const double d = mx - mn;
    val = mx;
    sat = (mx > 1e-6) ? (d / mx) : 0.0;
    if (d < 1e-6) {
        hDeg = 0.0;
        return;
    }
    if (mx == r) hDeg = 60.0 * std::fmod(((g - b) / d) + 6.0, 6.0);
    else if (mx == g) hDeg = 60.0 * (((b - r) / d) + 2.0);
    else hDeg = 60.0 * (((r - g) / d) + 4.0);
}

} // namespace

const char* AccessibilityEngine::PaperTintLabelUtf8(PaperTintPreset preset) noexcept {
    switch (preset) {
    case PaperTintPreset::Porcelana:    return "Porcelana (Neutro)";
    case PaperTintPreset::SepiaCalido:  return "Sepia C\xC3\xA1lido (Anti-Fatiga)";
    case PaperTintPreset::MentaSuave:   return "Menta Suave (Baja Reflexi\xC3\xB3n)";
    case PaperTintPreset::LavandaTenue: return "Lavanda Tenue (Confort Visual)";
    case PaperTintPreset::NocheSuave:   return "Noche Suave (Oscuro Mate)";
    }
    return "Porcelana (Neutro)";
}

const char* AccessibilityEngine::ColorBlindModeLabelUtf8(ColorBlindPaletteMode mode) noexcept {
    switch (mode) {
    case ColorBlindPaletteMode::Normal:       return "Est\xC3\xA1ndar";
    case ColorBlindPaletteMode::Protanopia:   return "Protanop\xC3\xAD""a (Okabe-Ito)";
    case ColorBlindPaletteMode::Deuteranopia: return "Deuteranop\xC3\xAD""a (Okabe-Ito)";
    case ColorBlindPaletteMode::Tritanopia:   return "Tritanop\xC3\xAD""a (Alto Contraste)";
    }
    return "Est\xC3\xA1ndar";
}

DocColor AccessibilityEngine::PaperTintSurfaceColor(PaperTintPreset preset, bool darkTheme, bool highContrast) noexcept {
    if (highContrast) {
        return darkTheme ? DocColor::RGBA(14, 16, 22, 255) : DocColor::RGBA(255, 255, 255, 255);
    }
    switch (preset) {
    case PaperTintPreset::SepiaCalido:
        return darkTheme ? DocColor::RGBA(42, 37, 30, 255) : DocColor::RGBA(248, 241, 224, 255);
    case PaperTintPreset::MentaSuave:
        return darkTheme ? DocColor::RGBA(26, 40, 35, 255) : DocColor::RGBA(236, 247, 240, 255);
    case PaperTintPreset::LavandaTenue:
        return darkTheme ? DocColor::RGBA(33, 30, 44, 255) : DocColor::RGBA(242, 240, 252, 255);
    case PaperTintPreset::NocheSuave:
        return DocColor::RGBA(28, 32, 41, 255);
    case PaperTintPreset::Porcelana:
    default:
        return darkTheme ? DocColor::RGBA(25, 28, 36, 255) : DocColor::RGBA(255, 255, 255, 255);
    }
}

DocColor AccessibilityEngine::RemapColorForColorBlind(DocColor c, ColorBlindPaletteMode mode) noexcept {
    if (mode == ColorBlindPaletteMode::Normal) return c;
    double h = 0.0, s = 0.0, v = 0.0;
    RgbToHsv(c.r, c.g, c.b, h, s, v);
    // Preserve black, white, and neutral grays untouched
    if (s < 0.22 || v < 0.14) return c;

    if (mode == ColorBlindPaletteMode::Protanopia || mode == ColorBlindPaletteMode::Deuteranopia) {
        // Okabe-Ito colorblind-safe categorical remapping:
        // Red -> Vermilion (#D55E00), Orange/Yellow -> Amber (#E69F00),
        // Green -> Bluish Green (#009E73), Cyan/Blue -> Deep Blue (#0072B2), Purple/Pink -> Reddish Purple (#CC79A7)
        if (h < 22.0 || h >= 335.0) return DocColor::RGBA(213, 94, 0, c.a);
        if (h < 72.0)               return DocColor::RGBA(230, 159, 0, c.a);
        if (h < 168.0)              return DocColor::RGBA(0, 158, 115, c.a);
        if (h < 260.0)              return DocColor::RGBA(0, 114, 178, c.a);
        return DocColor::RGBA(204, 121, 167, c.a);
    }

    // Tritanopia (Blue-Yellow deficiency): remap to high-contrast Crimson / Teal / Magenta / Dark Cyan
    if (h < 35.0 || h >= 325.0) return DocColor::RGBA(212, 36, 66, c.a);
    if (h < 100.0)              return DocColor::RGBA(240, 128, 128, c.a);
    if (h < 210.0)              return DocColor::RGBA(0, 146, 146, c.a);
    return DocColor::RGBA(15, 76, 129, c.a);
}

std::string AccessibilityEngine::VerbalizeMathExpressionForSpeech(const std::string& exprUtf8) {
    if (exprUtf8.empty()) return {};
    std::string out = exprUtf8;

    // Strip common inline math delimiters cleanly
    ReplaceAll(out, "$$", " ");
    ReplaceAll(out, "$", " ");
    ReplaceAll(out, "\\left", "");
    ReplaceAll(out, "\\right", "");
    ReplaceAll(out, "\\frac", " fracci\xC3\xB3n ");
    ReplaceAll(out, "\\sqrt", " ra\xC3\xADz cuadrada de ");
    ReplaceAll(out, "\\int", " integral de ");
    ReplaceAll(out, "\\sum", " sumatoria de ");
    ReplaceAll(out, "\\pm", " m\xC3\xA1s menos ");
    ReplaceAll(out, "\\times", " por ");
    ReplaceAll(out, "\\cdot", " por ");
    ReplaceAll(out, "\\leq", " menor o igual que ");
    ReplaceAll(out, "\\geq", " mayor o igual que ");
    ReplaceAll(out, "\\neq", " distinto de ");
    ReplaceAll(out, "\\to", " tiende a ");
    ReplaceAll(out, "\\infty", " infinito ");

    // Unicode symbols
    ReplaceAll(out, "\xE2\x88\x9A", " ra\xC3\xADz cuadrada de ");
    ReplaceAll(out, "\xE2\x88\xAB", " integral de ");
    ReplaceAll(out, "\xE2\x88\x91", " sumatoria de ");
    ReplaceAll(out, "\xCF\x80", " pi ");
    ReplaceAll(out, "\xCE\xB1", " alfa ");
    ReplaceAll(out, "\xCE\xB2", " beta ");
    ReplaceAll(out, "\xCE\xB3", " gamma ");
    ReplaceAll(out, "\xCE\xB4", " delta ");
    ReplaceAll(out, "\xCE\x94", " delta ");
    ReplaceAll(out, "\xCE\xB8", " teta ");
    ReplaceAll(out, "\xCE\xBB", " lambda ");
    ReplaceAll(out, "\xCE\xBC", " mu ");
    ReplaceAll(out, "\xCF\x83", " sigma ");
    ReplaceAll(out, "\xCF\x89", " omega ");
    ReplaceAll(out, "\xCE\xA9", " ohmios ");
    ReplaceAll(out, "\xE2\x89\xA4", " menor o igual que ");
    ReplaceAll(out, "\xE2\x89\xA5", " mayor o igual que ");
    ReplaceAll(out, "\xE2\x89\xA0", " distinto de ");
    ReplaceAll(out, "\xC2\xB1", " m\xC3\xA1s menos ");
    ReplaceAll(out, "\xE2\x86\x92", " implica ");
    ReplaceAll(out, "\xE2\x88\x9E", " infinito ");

    // Common ASCII math operators & functions
    ReplaceAll(out, "<=", " menor o igual que ");
    ReplaceAll(out, ">=", " mayor o igual que ");
    ReplaceAll(out, "!=", " distinto de ");
    ReplaceAll(out, "->", " tiende a ");
    ReplaceAll(out, "^2", " al cuadrado ");
    ReplaceAll(out, "^3", " al cubo ");
    ReplaceAll(out, "^", " elevado a ");

    ReplaceWord(out, "sqrt", "ra\xC3\xADz cuadrada de");
    ReplaceWord(out, "sin", "seno de");
    ReplaceWord(out, "cos", "coseno de");
    ReplaceWord(out, "tan", "tangente de");
    ReplaceWord(out, "ln", "logaritmo natural de");
    ReplaceWord(out, "log", "logaritmo de");
    ReplaceWord(out, "int", "integral de");
    ReplaceWord(out, "sum", "sumatoria de");
    ReplaceWord(out, "pi", "pi");
    ReplaceWord(out, "alpha", "alfa");
    ReplaceWord(out, "beta", "beta");
    ReplaceWord(out, "gamma", "gamma");
    ReplaceWord(out, "theta", "teta");
    ReplaceWord(out, "omega", "omega");
    ReplaceWord(out, "delta", "delta");

    ReplaceAll(out, "*", " por ");
    ReplaceAll(out, "/", " entre ");
    ReplaceAll(out, "=", " igual a ");
    ReplaceAll(out, "+", " m\xC3\xA1s ");
    ReplaceAll(out, "{", " ");
    ReplaceAll(out, "}", " ");
    ReplaceAll(out, "\\", " ");

    // Collapse multiple spaces cleanly
    std::string clean;
    clean.reserve(out.size());
    bool prevSpace = true;
    for (char ch : out) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!prevSpace) { clean.push_back(' '); prevSpace = true; }
        } else {
            clean.push_back(ch);
            prevSpace = false;
        }
    }
    if (!clean.empty() && clean.back() == ' ') clean.pop_back();
    return clean;
}

std::string AccessibilityEngine::ExtractReadableSpeechText(
    const DocumentModel& doc,
    std::size_t activePageIndex,
    std::uint64_t selectedObjectId)
{
    auto formatObject = [](const AnnotationObject& o) -> std::string {
        if (!o.formulaExprUtf8.empty()) {
            std::string line = VerbalizeMathExpressionForSpeech(o.formulaExprUtf8);
            if (!o.evaluatedResultUtf8.empty()) {
                line += ", resultado: " + VerbalizeMathExpressionForSpeech(o.evaluatedResultUtf8);
            }
            return line;
        }
        if (!o.textUtf8.empty()) return o.textUtf8;
        if (!o.htrRecognizedTextUtf8.empty()) return o.htrRecognizedTextUtf8;
        if (!o.captionUtf8.empty()) return o.captionUtf8;
        return {};
    };

    if (selectedObjectId != 0) {
        if (const auto* co = doc.FindCanvasObject(selectedObjectId)) {
            const std::string t = formatObject(*co);
            if (!t.empty()) return t;
        }
        if (activePageIndex < doc.Pages().size()) {
            for (const auto& o : doc.Pages()[activePageIndex].objects) {
                if (o.id == selectedObjectId) {
                    const std::string t = formatObject(o);
                    if (!t.empty()) return t;
                }
            }
        }
    }

    std::ostringstream oss;
    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        for (const auto& o : doc.CanvasObjects()) {
            const std::string t = formatObject(o);
            if (!t.empty()) oss << t << ".\n";
        }
    } else if (activePageIndex < doc.Pages().size()) {
        const auto& pg = doc.Pages()[activePageIndex];
        if (!pg.extractedTextUtf8.empty()) oss << pg.extractedTextUtf8 << "\n";
        else if (!pg.ocrTextUtf8.empty()) oss << pg.ocrTextUtf8 << "\n";
        for (const auto& o : pg.objects) {
            const std::string t = formatObject(o);
            if (!t.empty()) oss << t << ".\n";
        }
    }
    return oss.str();
}

// ---------------- SpeechSynthesizerWin32 (Windows SAPI 5.4) ----------------

struct SpeechSynthesizerWin32::Impl {
#ifdef _WIN32
    Microsoft::WRL::ComPtr<ISpVoice> voice;
#endif
};

SpeechSynthesizerWin32::SpeechSynthesizerWin32()
    : impl_(new Impl()) {}

SpeechSynthesizerWin32::~SpeechSynthesizerWin32() {
    Shutdown();
    delete impl_;
    impl_ = nullptr;
}

bool SpeechSynthesizerWin32::Initialize(std::string* outErr) {
#ifdef _WIN32
    if (!impl_) return false;
    if (impl_->voice) return true;
    const HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice,
        reinterpret_cast<void**>(impl_->voice.GetAddressOf()));
    if (FAILED(hr) || !impl_->voice) {
        if (outErr) *outErr = "No se pudo inicializar Windows SAPI 5.4 (ISpVoice).";
        return false;
    }
    impl_->voice->SetRate(static_cast<long>(std::clamp(rate_, -10, 10)));
    impl_->voice->SetVolume(static_cast<USHORT>(std::clamp(volume_, 0, 100)));
    if (!selectedVoiceUtf8_.empty()) {
        SelectVoiceByName(selectedVoiceUtf8_);
    }
    return true;
#else
    if (outErr) *outErr = "SAPI solo disponible en Windows.";
    return false;
#endif
}

void SpeechSynthesizerWin32::Shutdown() noexcept {
#ifdef _WIN32
    if (impl_ && impl_->voice) {
        impl_->voice->Speak(nullptr, SPF_PURGEBEFORESPEAK | SPF_ASYNC, nullptr);
        if (paused_) {
            impl_->voice->Resume();
            paused_ = false;
        }
        impl_->voice.Reset();
    }
#endif
}

std::vector<std::string> SpeechSynthesizerWin32::EnumerateInstalledVoices() const {
    std::vector<std::string> names;
#ifdef _WIN32
    Microsoft::WRL::ComPtr<ISpObjectTokenCategory> cat;
    if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
            reinterpret_cast<void**>(cat.GetAddressOf()))) || !cat) {
        return names;
    }
    if (FAILED(cat->SetId(SPCAT_VOICES, FALSE))) return names;

    Microsoft::WRL::ComPtr<IEnumSpObjectTokens> en;
    if (FAILED(cat->EnumTokens(nullptr, nullptr, en.GetAddressOf())) || !en) return names;

    ULONG count = 0;
    if (FAILED(en->GetCount(&count))) return names;
    for (ULONG i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<ISpObjectToken> tok;
        if (SUCCEEDED(en->Item(i, tok.GetAddressOf())) && tok) {
            LPWSTR desc = nullptr;
            if (SUCCEEDED(tok->GetStringValue(nullptr, &desc)) && desc) {
                names.push_back(WideToUtf8(desc));
                CoTaskMemFree(desc);
            }
        }
    }
#endif
    return names;
}

bool SpeechSynthesizerWin32::SelectVoiceByName(const std::string& voiceNameUtf8) {
    selectedVoiceUtf8_ = voiceNameUtf8;
#ifdef _WIN32
    if (voiceNameUtf8.empty()) return false;
    if (!impl_ || !impl_->voice) {
        if (!Initialize()) return false;
    }
    Microsoft::WRL::ComPtr<ISpObjectTokenCategory> cat;
    if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
            reinterpret_cast<void**>(cat.GetAddressOf()))) || !cat) {
        return false;
    }
    if (FAILED(cat->SetId(SPCAT_VOICES, FALSE))) return false;
    Microsoft::WRL::ComPtr<IEnumSpObjectTokens> en;
    if (FAILED(cat->EnumTokens(nullptr, nullptr, en.GetAddressOf())) || !en) return false;

    ULONG count = 0;
    if (FAILED(en->GetCount(&count))) return false;
    for (ULONG i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<ISpObjectToken> tok;
        if (SUCCEEDED(en->Item(i, tok.GetAddressOf())) && tok) {
            LPWSTR desc = nullptr;
            if (SUCCEEDED(tok->GetStringValue(nullptr, &desc)) && desc) {
                const std::string nm = WideToUtf8(desc);
                CoTaskMemFree(desc);
                if (nm == voiceNameUtf8 || nm.find(voiceNameUtf8) != std::string::npos) {
                    return SUCCEEDED(impl_->voice->SetVoice(tok.Get()));
                }
            }
        }
    }
#endif
    return false;
}

void SpeechSynthesizerWin32::SetRate(int rateMinus10ToPlus10) {
    rate_ = std::clamp(rateMinus10ToPlus10, -10, 10);
#ifdef _WIN32
    if (impl_ && impl_->voice) impl_->voice->SetRate(static_cast<long>(rate_));
#endif
}

void SpeechSynthesizerWin32::SetVolume(int volume0To100) {
    volume_ = std::clamp(volume0To100, 0, 100);
#ifdef _WIN32
    if (impl_ && impl_->voice) impl_->voice->SetVolume(static_cast<USHORT>(volume_));
#endif
}

bool SpeechSynthesizerWin32::SpeakAsync(const std::string& textUtf8, std::string* outErr) {
    if (textUtf8.empty()) {
        if (outErr) *outErr = "No hay texto para leer en voz alta.";
        return false;
    }
#ifdef _WIN32
    if (!Initialize(outErr)) return false;
    if (paused_) {
        impl_->voice->Resume();
        paused_ = false;
    }
    const std::wstring wText = Utf8ToWide(textUtf8);
    const HRESULT hr = impl_->voice->Speak(wText.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, nullptr);
    if (FAILED(hr)) {
        if (outErr) *outErr = "Error iniciando lectura en voz alta SAPI.";
        return false;
    }
    return true;
#else
    (void)textUtf8;
    if (outErr) *outErr = "SAPI solo disponible en Windows.";
    return false;
#endif
}

void SpeechSynthesizerWin32::Pause() {
#ifdef _WIN32
    if (impl_ && impl_->voice && !paused_) {
        impl_->voice->Pause();
        paused_ = true;
    }
#endif
}

void SpeechSynthesizerWin32::Resume() {
#ifdef _WIN32
    if (impl_ && impl_->voice && paused_) {
        impl_->voice->Resume();
        paused_ = false;
    }
#endif
}

void SpeechSynthesizerWin32::Stop() {
#ifdef _WIN32
    if (impl_ && impl_->voice) {
        if (paused_) {
            impl_->voice->Resume();
            paused_ = false;
        }
        impl_->voice->Speak(nullptr, SPF_PURGEBEFORESPEAK | SPF_ASYNC, nullptr);
    }
#endif
}

bool SpeechSynthesizerWin32::IsSpeaking() const {
#ifdef _WIN32
    if (!impl_ || !impl_->voice) return false;
    SPVOICESTATUS st{};
    if (SUCCEEDED(impl_->voice->GetStatus(&st, nullptr))) {
        return st.dwRunningState == SPRS_IS_SPEAKING;
    }
#endif
    return false;
}

} // namespace DocStudio
