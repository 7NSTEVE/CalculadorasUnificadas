#include "smart_script_reflow.hpp"
#include "utf_conv.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <spellcheck.h>
#include <wrl/client.h>
#endif

namespace DocStudio {

namespace {

const std::vector<AnnotationObject>& ObjectsForTarget(const DocumentModel& doc, ObjectTarget target) {
    if (target.isCanvas) return doc.CanvasObjects();
    return doc.Pages()[target.pageIndex].objects;
}

DocRect UnionRect(const DocRect& a, const DocRect& b) noexcept {
    if (a.Empty()) return b;
    if (b.Empty()) return a;
    const double l = (std::min)(a.x, b.x);
    const double t = (std::min)(a.y, b.y);
    const double r = (std::max)(a.Right(), b.Right());
    const double bt = (std::max)(a.Bottom(), b.Bottom());
    return { l, t, r - l, bt - t };
}

// Monoline vector glyph polylines in normalized [0..1] x [0..1] box (y=0 top, y=1 baseline, y=1.22 descender)
// Each glyph is a list of separate pen-down strokes so every letter is visually legible.
using GlyphPolyline = std::vector<std::pair<double, double>>;
using GlyphStrokes = std::vector<GlyphPolyline>;

GlyphStrokes GetMonolineGlyph(char32_t cp) {
    // Handle Spanish accents & ñ by adding diacritic stroke over base glyph
    auto addAccent = [](GlyphStrokes base, char32_t accentKind) -> GlyphStrokes {
        if (accentKind == U'\'') { // acute accent
            base.push_back({ {0.42, 0.14}, {0.62, 0.02} });
        } else if (accentKind == U'~') { // tilde (~)
            base.push_back({ {0.22, 0.14}, {0.42, 0.06}, {0.62, 0.16}, {0.80, 0.08} });
        } else if (accentKind == U':') { // umlaut (¨)
            base.push_back({ {0.32, 0.10}, {0.36, 0.10} });
            base.push_back({ {0.64, 0.10}, {0.68, 0.10} });
        }
        return base;
    };

    switch (cp) {
        case U'\xE1': return addAccent(GetMonolineGlyph(U'a'), U'\''); // á
        case U'\xE9': return addAccent(GetMonolineGlyph(U'e'), U'\''); // é
        case U'\xED': return addAccent(GetMonolineGlyph(U'i'), U'\''); // í
        case U'\xF3': return addAccent(GetMonolineGlyph(U'o'), U'\''); // ó
        case U'\xFA': return addAccent(GetMonolineGlyph(U'u'), U'\''); // ú
        case U'\xFC': return addAccent(GetMonolineGlyph(U'u'), U':');  // ü
        case U'\xF1': return addAccent(GetMonolineGlyph(U'n'), U'~');  // ñ
        case U'\xC1': return addAccent(GetMonolineGlyph(U'A'), U'\''); // Á
        case U'\xC9': return addAccent(GetMonolineGlyph(U'E'), U'\''); // É
        case U'\xCD': return addAccent(GetMonolineGlyph(U'I'), U'\''); // Í
        case U'\xD3': return addAccent(GetMonolineGlyph(U'O'), U'\''); // Ó
        case U'\xDA': return addAccent(GetMonolineGlyph(U'U'), U'\''); // Ú
        case U'\xD1': return addAccent(GetMonolineGlyph(U'N'), U'~');  // Ñ
        default: break;
    }

    switch (cp) {
        // Uppercase
        case U'A': return { {{0.10,1.00},{0.50,0.08},{0.90,1.00}}, {{0.24,0.68},{0.76,0.68}} };
        case U'B': return { {{0.16,0.08},{0.16,1.00}}, {{0.16,0.08},{0.66,0.08},{0.82,0.20},{0.82,0.40},{0.66,0.52},{0.16,0.52}}, {{0.66,0.52},{0.86,0.64},{0.86,0.88},{0.68,1.00},{0.16,1.00}} };
        case U'C': return { {{0.86,0.22},{0.66,0.08},{0.34,0.08},{0.14,0.28},{0.14,0.78},{0.34,1.00},{0.66,1.00},{0.86,0.84}} };
        case U'D': return { {{0.16,0.08},{0.16,1.00}}, {{0.16,0.08},{0.58,0.08},{0.84,0.28},{0.84,0.78},{0.58,1.00},{0.16,1.00}} };
        case U'E': return { {{0.82,0.08},{0.16,0.08},{0.16,1.00},{0.84,1.00}}, {{0.16,0.52},{0.68,0.52}} };
        case U'F': return { {{0.82,0.08},{0.16,0.08},{0.16,1.00}}, {{0.16,0.52},{0.66,0.52}} };
        case U'G': return { {{0.86,0.22},{0.64,0.08},{0.32,0.08},{0.14,0.30},{0.14,0.78},{0.34,1.00},{0.72,1.00},{0.86,0.82},{0.86,0.56},{0.54,0.56}} };
        case U'H': return { {{0.16,0.08},{0.16,1.00}}, {{0.84,0.08},{0.84,1.00}}, {{0.16,0.52},{0.84,0.52}} };
        case U'I': return { {{0.28,0.08},{0.72,0.08}}, {{0.50,0.08},{0.50,1.00}}, {{0.28,1.00},{0.72,1.00}} };
        case U'J': return { {{0.30,0.08},{0.78,0.08}}, {{0.68,0.08},{0.68,0.82},{0.52,1.00},{0.28,1.00},{0.14,0.84}} };
        case U'K': return { {{0.16,0.08},{0.16,1.00}}, {{0.84,0.08},{0.16,0.56},{0.86,1.00}} };
        case U'L': return { {{0.18,0.08},{0.18,1.00},{0.84,1.00}} };
        case U'M': return { {{0.12,1.00},{0.12,0.08},{0.50,0.62},{0.88,0.08},{0.88,1.00}} };
        case U'N': return { {{0.16,1.00},{0.16,0.08},{0.84,1.00},{0.84,0.08}} };
        case U'O': return { {{0.50,0.08},{0.22,0.20},{0.14,0.54},{0.22,0.86},{0.50,1.00},{0.78,0.86},{0.86,0.54},{0.78,0.20},{0.50,0.08}} };
        case U'P': return { {{0.16,1.00},{0.16,0.08},{0.68,0.08},{0.84,0.22},{0.84,0.42},{0.68,0.56},{0.16,0.56}} };
        case U'Q': return { {{0.50,0.08},{0.20,0.22},{0.14,0.54},{0.22,0.86},{0.50,1.00},{0.78,0.86},{0.86,0.54},{0.78,0.22},{0.50,0.08}}, {{0.58,0.74},{0.90,1.06}} };
        case U'R': return { {{0.16,1.00},{0.16,0.08},{0.66,0.08},{0.82,0.22},{0.82,0.42},{0.66,0.54},{0.16,0.54}}, {{0.52,0.54},{0.86,1.00}} };
        case U'S': return { {{0.82,0.22},{0.64,0.08},{0.32,0.08},{0.16,0.22},{0.18,0.40},{0.42,0.52},{0.72,0.62},{0.84,0.80},{0.68,1.00},{0.32,1.00},{0.14,0.86}} };
        case U'T': return { {{0.10,0.08},{0.90,0.08}}, {{0.50,0.08},{0.50,1.00}} };
        case U'U': return { {{0.16,0.08},{0.16,0.78},{0.34,1.00},{0.66,1.00},{0.84,0.78},{0.84,0.08}} };
        case U'V': return { {{0.12,0.08},{0.50,1.00},{0.88,0.08}} };
        case U'W': return { {{0.08,0.08},{0.28,1.00},{0.50,0.42},{0.72,1.00},{0.92,0.08}} };
        case U'X': return { {{0.14,0.08},{0.86,1.00}}, {{0.86,0.08},{0.14,1.00}} };
        case U'Y': return { {{0.12,0.08},{0.50,0.54},{0.88,0.08}}, {{0.50,0.54},{0.50,1.00}} };
        case U'Z': return { {{0.14,0.08},{0.86,0.08},{0.14,1.00},{0.86,1.00}} };

        // Lowercase
        case U'a': return { {{0.76,0.50},{0.58,0.38},{0.30,0.42},{0.16,0.64},{0.20,0.90},{0.42,1.00},{0.68,0.92},{0.78,0.72}}, {{0.78,0.40},{0.78,1.00}} };
        case U'b': return { {{0.18,0.06},{0.18,1.00}}, {{0.18,0.56},{0.38,0.40},{0.68,0.42},{0.82,0.64},{0.78,0.90},{0.56,1.00},{0.28,0.96},{0.18,0.84}} };
        case U'c': return { {{0.82,0.52},{0.64,0.38},{0.36,0.38},{0.18,0.56},{0.18,0.84},{0.36,1.00},{0.66,1.00},{0.82,0.88}} };
        case U'd': return { {{0.80,0.06},{0.80,1.00}}, {{0.80,0.56},{0.62,0.40},{0.32,0.42},{0.18,0.64},{0.22,0.90},{0.44,1.00},{0.70,0.96},{0.80,0.84}} };
        case U'e': return { {{0.20,0.68},{0.82,0.68},{0.80,0.50},{0.62,0.38},{0.34,0.38},{0.18,0.58},{0.20,0.86},{0.40,1.00},{0.74,0.98}} };
        case U'f': return { {{0.76,0.12},{0.58,0.06},{0.44,0.18},{0.44,1.00}}, {{0.20,0.46},{0.72,0.46}} };
        case U'g': return { {{0.80,0.46},{0.58,0.38},{0.30,0.44},{0.18,0.66},{0.24,0.90},{0.48,0.98},{0.72,0.90},{0.80,0.72}}, {{0.80,0.40},{0.80,1.12},{0.64,1.26},{0.36,1.26},{0.20,1.16}} };
        case U'h': return { {{0.18,0.06},{0.18,1.00}}, {{0.18,0.58},{0.40,0.40},{0.66,0.40},{0.80,0.56},{0.80,1.00}} };
        case U'i': return { {{0.50,0.40},{0.50,1.00}}, {{0.48,0.20},{0.52,0.20}} };
        case U'j': return { {{0.60,0.40},{0.60,1.12},{0.46,1.26},{0.26,1.24}}, {{0.58,0.20},{0.62,0.20}} };
        case U'k': return { {{0.20,0.06},{0.20,1.00}}, {{0.78,0.40},{0.22,0.70},{0.82,1.00}} };
        case U'l': return { {{0.46,0.06},{0.46,0.94},{0.60,1.00}} };
        case U'm': return { {{0.10,0.40},{0.10,1.00}}, {{0.10,0.54},{0.26,0.40},{0.44,0.42},{0.50,0.58},{0.50,1.00}}, {{0.50,0.54},{0.66,0.40},{0.84,0.42},{0.90,0.58},{0.90,1.00}} };
        case U'n': return { {{0.18,0.40},{0.18,1.00}}, {{0.18,0.56},{0.38,0.40},{0.66,0.40},{0.82,0.56},{0.82,1.00}} };
        case U'o': return { {{0.50,0.38},{0.24,0.48},{0.16,0.70},{0.26,0.94},{0.50,1.00},{0.76,0.94},{0.84,0.70},{0.76,0.48},{0.50,0.38}} };
        case U'p': return { {{0.18,0.40},{0.18,1.26}}, {{0.18,0.56},{0.38,0.40},{0.68,0.42},{0.82,0.64},{0.76,0.90},{0.54,1.00},{0.28,0.96},{0.18,0.84}} };
        case U'q': return { {{0.80,0.40},{0.80,1.26}}, {{0.80,0.56},{0.62,0.40},{0.32,0.42},{0.18,0.64},{0.24,0.90},{0.46,1.00},{0.70,0.96},{0.80,0.84}} };
        case U'r': return { {{0.24,0.40},{0.24,1.00}}, {{0.24,0.60},{0.42,0.42},{0.68,0.40},{0.80,0.46}} };
        case U's': return { {{0.78,0.48},{0.60,0.38},{0.34,0.38},{0.20,0.50},{0.28,0.64},{0.68,0.74},{0.80,0.88},{0.64,1.00},{0.34,1.00},{0.18,0.90}} };
        case U't': return { {{0.46,0.14},{0.46,0.90},{0.60,1.00},{0.76,0.96}}, {{0.22,0.42},{0.72,0.42}} };
        case U'u': return { {{0.18,0.40},{0.18,0.84},{0.36,1.00},{0.62,0.98},{0.80,0.82}}, {{0.80,0.40},{0.80,1.00}} };
        case U'v': return { {{0.16,0.40},{0.50,1.00},{0.84,0.40}} };
        case U'w': return { {{0.10,0.40},{0.28,1.00},{0.50,0.58},{0.72,1.00},{0.90,0.40}} };
        case U'x': return { {{0.18,0.40},{0.82,1.00}}, {{0.82,0.40},{0.18,1.00}} };
        case U'y': return { {{0.18,0.40},{0.50,0.96}}, {{0.82,0.40},{0.42,1.22},{0.24,1.26}} };
        case U'z': return { {{0.18,0.40},{0.82,0.40},{0.18,1.00},{0.82,1.00}} };

        // Digits
        case U'0': return { {{0.50,0.08},{0.24,0.24},{0.18,0.54},{0.24,0.84},{0.50,1.00},{0.76,0.84},{0.82,0.54},{0.76,0.24},{0.50,0.08}}, {{0.26,0.80},{0.74,0.26}} };
        case U'1': return { {{0.30,0.26},{0.52,0.08},{0.52,1.00}}, {{0.28,1.00},{0.76,1.00}} };
        case U'2': return { {{0.18,0.28},{0.34,0.10},{0.66,0.10},{0.82,0.26},{0.78,0.48},{0.18,1.00},{0.84,1.00}} };
        case U'3': return { {{0.20,0.20},{0.42,0.08},{0.72,0.12},{0.80,0.32},{0.62,0.50},{0.40,0.50}}, {{0.62,0.50},{0.84,0.68},{0.74,0.94},{0.46,1.00},{0.18,0.88}} };
        case U'4': return { {{0.70,1.00},{0.70,0.08},{0.16,0.70},{0.86,0.70}} };
        case U'5': return { {{0.78,0.08},{0.24,0.08},{0.20,0.48},{0.52,0.42},{0.80,0.56},{0.80,0.84},{0.58,1.00},{0.26,1.00},{0.16,0.88}} };
        case U'6': return { {{0.76,0.16},{0.54,0.08},{0.26,0.24},{0.18,0.62},{0.28,0.92},{0.54,1.00},{0.78,0.86},{0.80,0.62},{0.56,0.48},{0.26,0.56},{0.18,0.68}} };
        case U'7': return { {{0.16,0.08},{0.84,0.08},{0.42,1.00}} };
        case U'8': return { {{0.50,0.50},{0.24,0.36},{0.26,0.16},{0.50,0.08},{0.74,0.16},{0.76,0.36},{0.50,0.50},{0.20,0.68},{0.24,0.92},{0.50,1.00},{0.76,0.92},{0.80,0.68},{0.50,0.50}} };
        case U'9': return { {{0.80,0.36},{0.68,0.54},{0.40,0.56},{0.20,0.40},{0.24,0.18},{0.50,0.08},{0.76,0.18},{0.82,0.52},{0.74,0.86},{0.48,1.00},{0.26,0.94}} };

        // Punctuation & Math
        case U'.': return { {{0.46,0.94},{0.54,0.94}} };
        case U',': return { {{0.52,0.90},{0.42,1.12}} };
        case U':': return { {{0.48,0.46},{0.52,0.46}}, {{0.48,0.90},{0.52,0.90}} };
        case U';': return { {{0.48,0.46},{0.52,0.46}}, {{0.52,0.88},{0.42,1.10}} };
        case U'-': return { {{0.20,0.60},{0.80,0.60}} };
        case U'+': return { {{0.20,0.60},{0.80,0.60}}, {{0.50,0.30},{0.50,0.90}} };
        case U'=': return { {{0.18,0.48},{0.82,0.48}}, {{0.18,0.72},{0.82,0.72}} };
        case U'/': return { {{0.78,0.10},{0.22,1.02}} };
        case U'(': return { {{0.66,0.08},{0.42,0.32},{0.38,0.60},{0.44,0.86},{0.66,1.06}} };
        case U')': return { {{0.34,0.08},{0.58,0.32},{0.62,0.60},{0.56,0.86},{0.34,1.06}} };
        case U'!': return { {{0.50,0.08},{0.50,0.76}}, {{0.48,0.94},{0.52,0.94}} };
        case U'?': return { {{0.24,0.26},{0.40,0.08},{0.68,0.10},{0.78,0.28},{0.66,0.46},{0.50,0.58},{0.50,0.74}}, {{0.48,0.94},{0.52,0.94}} };
        default:   return { {{0.24,0.54},{0.76,0.54}} };
    }
}

std::vector<char32_t> DecodeUtf8Codepoints(const std::string& s) {
    std::vector<char32_t> cps;
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        if (b < 0x80u) {
            cps.push_back(b);
            ++i;
        } else if ((b & 0xE0u) == 0xC0u && i + 1 < s.size()) {
            char32_t cp = ((b & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
            cps.push_back(cp);
            i += 2;
        } else if ((b & 0xF0u) == 0xE0u && i + 2 < s.size()) {
            char32_t cp = ((b & 0x0Fu) << 12) |
                          ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
                          (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
            cps.push_back(cp);
            i += 3;
        } else if ((b & 0xF8u) == 0xF0u && i + 3 < s.size()) {
            char32_t cp = ((b & 0x07u) << 18) |
                          ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
                          ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) |
                          (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
            cps.push_back(cp);
            i += 4;
        } else {
            ++i;
        }
    }
    return cps;
}

std::size_t LevenshteinDist(const std::string& a, const std::string& b) {
    const std::size_t m = a.size(), n = b.size();
    std::vector<std::size_t> dp(n + 1);
    for (std::size_t j = 0; j <= n; ++j) dp[j] = j;
    for (std::size_t i = 1; i <= m; ++i) {
        std::size_t prev = dp[0];
        dp[0] = i;
        for (std::size_t j = 1; j <= n; ++j) {
            const std::size_t tmp = dp[j];
            const std::size_t cost = (a[i - 1] == b[j - 1]) ? 0u : 1u;
            dp[j] = (std::min)({ dp[j] + 1, dp[j - 1] + 1, prev + cost });
            prev = tmp;
        }
    }
    return dp[n];
}

// Native Windows ISpellChecker lookup with STEM lexicon fallback
bool CheckWordWithWindowsSpellChecker(const std::string& wordUtf8, std::string& outSuggestionUtf8) {
    outSuggestionUtf8.clear();
#ifdef _WIN32
    Microsoft::WRL::ComPtr<ISpellCheckerFactory> factory;
    const HRESULT hrCreate = CoCreateInstance(
        __uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hrCreate) && factory) {
        static const wchar_t* kLangs[] = { L"es-ES", L"es-CR", L"es-MX", L"en-US" };
        Microsoft::WRL::ComPtr<ISpellChecker> checker;
        for (const wchar_t* lang : kLangs) {
            BOOL supported = FALSE;
            if (SUCCEEDED(factory->IsSupported(lang, &supported)) && supported) {
                if (SUCCEEDED(factory->CreateSpellChecker(lang, &checker)) && checker) break;
            }
        }
        if (checker) {
            const std::wstring wWord = Utf8ToWide(wordUtf8);
            Microsoft::WRL::ComPtr<IEnumSpellingError> errs;
            if (SUCCEEDED(checker->Check(wWord.c_str(), &errs)) && errs) {
                Microsoft::WRL::ComPtr<ISpellingError> err;
                if (errs->Next(&err) == S_OK && err) {
                    CORRECTIVE_ACTION act = CORRECTIVE_ACTION_NONE;
                    err->get_CorrectiveAction(&act);
                    if (act == CORRECTIVE_ACTION_REPLACE) {
                        LPWSTR repl = nullptr;
                        if (SUCCEEDED(err->get_Replacement(&repl)) && repl) {
                            outSuggestionUtf8 = WideToUtf8(repl);
                            CoTaskMemFree(repl);
                            return !outSuggestionUtf8.empty() && outSuggestionUtf8 != wordUtf8;
                        }
                    } else if (act == CORRECTIVE_ACTION_GET_SUGGESTIONS) {
                        Microsoft::WRL::ComPtr<IEnumString> suggs;
                        if (SUCCEEDED(checker->Suggest(wWord.c_str(), &suggs)) && suggs) {
                            LPOLESTR s = nullptr;
                            ULONG fetched = 0;
                            if (suggs->Next(1, &s, &fetched) == S_OK && fetched == 1 && s) {
                                outSuggestionUtf8 = WideToUtf8(s);
                                CoTaskMemFree(s);
                                return !outSuggestionUtf8.empty() && outSuggestionUtf8 != wordUtf8;
                            }
                        }
                    }
                }
                return false; // Windows ISpellChecker confirmed the word is valid
            }
        }
    }
#endif
    // Fallback STEM Spanish/English lexicon using edit distance <= 2 if Windows spellchecker is unavailable
    static const std::vector<std::string> kStemLexicon = {
        "f\xC3\xADsica", "c\xC3\xA1lculo", "derivada", "integral", "voltaje", "corriente", "resistencia",
        "ecuaci\xC3\xB3n", "teorema", "matriz", "vector", "funci\xC3\xB3n", "l\xC3\xADmite", "serie",
        "convergencia", "divergencia", "laplace", "fourier", "impedancia", "circuito", "qu\xC3\xADmica",
        "estequiometr\xC3\xAD""a", "mol\xC3\xA9""cula", "energ\xC3\xAD""a", "potencia", "frecuencia",
        "velocidad", "aceleraci\xC3\xB3n", "probabilidad", "estad\xC3\xADstica", "algoritmo", "geometr\xC3\xAD""a",
        "physics", "calculus", "derivative", "integral", "voltage", "equation", "theorem", "matrix"
    };
    const std::string low = FoldUtf8ForSearch(wordUtf8, false, true);
    if (low.size() < 4) return false;
    std::size_t bestDist = 3;
    std::string bestMatch;
    for (const auto& cand : kStemLexicon) {
        const std::string cFold = FoldUtf8ForSearch(cand, false, true);
        if (cFold == low) {
            if (cand != wordUtf8) {
                outSuggestionUtf8 = cand;
                return true;
            }
            return false;
        }
        const std::size_t d = LevenshteinDist(low, cFold);
        if (d < bestDist) {
            bestDist = d;
            bestMatch = cand;
        }
    }
    if (!bestMatch.empty() && bestDist <= 2) {
        outSuggestionUtf8 = bestMatch;
        return true;
    }
    return false;
}

} // namespace

std::vector<HandwrittenWordCluster> SmartScriptReflowEngine::ClusterHandwrittenWords(
    const DocumentModel& doc,
    ObjectTarget target,
    double wordGapThresholdPt,
    double lineGapThresholdPt)
{
    std::vector<const AnnotationObject*> strokes;
    for (const auto& obj : ObjectsForTarget(doc, target)) {
        if (obj.kind == ObjectKind::InkStroke && !obj.inkPoints.empty() && !obj.lockedInPlace) {
            strokes.push_back(&obj);
        }
    }
    if (strokes.empty()) return {};

    std::sort(strokes.begin(), strokes.end(), [&](const AnnotationObject* a, const AnnotationObject* b) {
        const double cyA = a->bounds.y + a->bounds.h * 0.5;
        const double cyB = b->bounds.y + b->bounds.h * 0.5;
        if (std::abs(cyA - cyB) > lineGapThresholdPt) return cyA < cyB;
        return a->bounds.x < b->bounds.x;
    });

    std::vector<HandwrittenWordCluster> clusters;
    for (const auto* s : strokes) {
        if (clusters.empty()) {
            HandwrittenWordCluster c;
            c.strokeIds.push_back(s->id);
            c.bounds = s->bounds;
            c.recognizedWordUtf8 = s->htrRecognizedTextUtf8;
            c.dominantColor = s->strokeColor;
            c.averageStrokeWidth = s->strokeWidth;
            clusters.push_back(std::move(c));
            continue;
        }
        auto& prev = clusters.back();
        const double cyPrev = prev.bounds.y + prev.bounds.h * 0.5;
        const double cyCur = s->bounds.y + s->bounds.h * 0.5;
        const bool sameLine = (std::abs(cyCur - cyPrev) <= lineGapThresholdPt);
        const double horizGap = s->bounds.x - prev.bounds.Right();
        const bool sameRecognizedWord = (!prev.recognizedWordUtf8.empty() && prev.recognizedWordUtf8 == s->htrRecognizedTextUtf8);
        const bool distinctRecognizedWords = (!prev.recognizedWordUtf8.empty() && !s->htrRecognizedTextUtf8.empty() && !sameRecognizedWord);

        if (sameLine && horizGap <= wordGapThresholdPt && !distinctRecognizedWords) {
            prev.strokeIds.push_back(s->id);
            prev.bounds = UnionRect(prev.bounds, s->bounds);
            if (prev.recognizedWordUtf8.empty() && !s->htrRecognizedTextUtf8.empty()) {
                prev.recognizedWordUtf8 = s->htrRecognizedTextUtf8;
            }
        } else {
            HandwrittenWordCluster c;
            c.strokeIds.push_back(s->id);
            c.bounds = s->bounds;
            c.recognizedWordUtf8 = s->htrRecognizedTextUtf8;
            c.dominantColor = s->strokeColor;
            c.averageStrokeWidth = s->strokeWidth;
            clusters.push_back(std::move(c));
        }
    }
    return clusters;
}

std::size_t SmartScriptReflowEngine::ReflowHandwrittenWordsToWidth(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double leftMarginPt,
    double topMarginPt,
    double maxLineWidthPt,
    double lineHeightPt,
    double wordSpacingPt)
{
    auto clusters = ClusterHandwrittenWords(doc, target);
    if (clusters.empty() || maxLineWidthPt < 24.0) return 0;

    std::vector<AnnotationObject> beforeObjs;
    std::vector<AnnotationObject> afterObjs;

    double curX = leftMarginPt;
    double curY = topMarginPt;
    std::size_t movedWords = 0;

    for (const auto& cl : clusters) {
        const double w = (std::max)(8.0, cl.bounds.w);
        if (curX > leftMarginPt && (curX + w > leftMarginPt + maxLineWidthPt)) {
            curX = leftMarginPt;
            curY += (std::max)(24.0, lineHeightPt);
        }
        const double dx = curX - cl.bounds.x;
        const double dy = curY - cl.bounds.y;
        curX += w + wordSpacingPt;

        if (std::abs(dx) < 0.2 && std::abs(dy) < 0.2) continue;
        ++movedWords;

        for (std::uint64_t sid : cl.strokeIds) {
            const AnnotationObject* cur = target.isCanvas
                ? doc.FindCanvasObject(sid)
                : doc.FindPageObjectMutable(target.pageIndex, sid);
            if (!cur) continue;
            beforeObjs.push_back(*cur);
            AnnotationObject moved = *cur;
            moved.Translate(dx, dy);
            afterObjs.push_back(std::move(moved));
        }
    }

    if (!beforeObjs.empty()) {
        cmdStack.Execute(doc, std::make_unique<BatchModifyObjectsCommand>(target, std::move(beforeObjs), std::move(afterObjs)));
    }
    return movedWords;
}

bool SmartScriptReflowEngine::ScaleAndTransformInkSelection(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::vector<std::uint64_t>& objectIds,
    double scaleX,
    double scaleY,
    double translateDx,
    double translateDy)
{
    if (objectIds.empty() || scaleX <= 0.05 || scaleY <= 0.05) return false;

    DocRect collective{};
    std::vector<const AnnotationObject*> found;
    for (std::uint64_t id : objectIds) {
        const AnnotationObject* o = target.isCanvas
            ? doc.FindCanvasObject(id)
            : doc.FindPageObjectMutable(target.pageIndex, id);
        if (o && !o->lockedInPlace) {
            found.push_back(o);
            collective = UnionRect(collective, o->bounds);
        }
    }
    if (found.empty() || collective.Empty()) return false;

    const double pivotX = collective.x;
    const double pivotY = collective.y;
    std::vector<AnnotationObject> beforeObjs;
    std::vector<AnnotationObject> afterObjs;

    for (const auto* o : found) {
        beforeObjs.push_back(*o);
        AnnotationObject tr = *o;
        for (auto& pt : tr.inkPoints) {
            pt.x = pivotX + (pt.x - pivotX) * scaleX + translateDx;
            pt.y = pivotY + (pt.y - pivotY) * scaleY + translateDy;
        }
        tr.bounds.x = pivotX + (tr.bounds.x - pivotX) * scaleX + translateDx;
        tr.bounds.y = pivotY + (tr.bounds.y - pivotY) * scaleY + translateDy;
        tr.bounds.w *= scaleX;
        tr.bounds.h *= scaleY;
        tr.shapeStart.x = pivotX + (tr.shapeStart.x - pivotX) * scaleX + translateDx;
        tr.shapeStart.y = pivotY + (tr.shapeStart.y - pivotY) * scaleY + translateDy;
        tr.shapeEnd.x = pivotX + (tr.shapeEnd.x - pivotX) * scaleX + translateDx;
        tr.shapeEnd.y = pivotY + (tr.shapeEnd.y - pivotY) * scaleY + translateDy;
        tr.RecomputeBounds();
        afterObjs.push_back(std::move(tr));
    }

    return cmdStack.Execute(doc, std::make_unique<BatchModifyObjectsCommand>(target, std::move(beforeObjs), std::move(afterObjs)));
}

std::vector<AnnotationObject> SmartScriptReflowEngine::SynthesizeHandwritingStrokesFromText(
    const std::string& textUtf8,
    double startX,
    double startY,
    double charHeightPt,
    DocColor inkColor,
    double strokeWidth,
    double slantRatio)
{
    std::vector<AnnotationObject> strokes;
    const double ch = (std::max)(12.0, charHeightPt);
    const double cw = (std::max)(7.5, ch * 0.58);
    double curX = startX;
    double curY = startY;
    std::uint64_t ts = 10;

    // Split into lines and words so each word's glyphs share its htrRecognizedTextUtf8 for clean clustering & reflow
    std::size_t pos = 0;
    while (pos < textUtf8.size()) {
        if (textUtf8[pos] == '\n') {
            curX = startX;
            curY += ch * 1.65;
            ++pos;
            continue;
        }
        if (textUtf8[pos] == '\r' || textUtf8[pos] == ' ' || textUtf8[pos] == '\t') {
            if (textUtf8[pos] != '\r') curX += cw * 0.95;
            ++pos;
            continue;
        }
        std::size_t wEnd = pos;
        while (wEnd < textUtf8.size() && textUtf8[wEnd] != ' ' && textUtf8[wEnd] != '\t' &&
               textUtf8[wEnd] != '\n' && textUtf8[wEnd] != '\r') {
            ++wEnd;
        }
        const std::string wordUtf8 = textUtf8.substr(pos, wEnd - pos);
        const auto cps = DecodeUtf8Codepoints(wordUtf8);

        for (char32_t cp : cps) {
            const GlyphStrokes glyph = GetMonolineGlyph(cp);
            for (const auto& poly : glyph) {
                if (poly.size() < 2) continue;
                std::vector<InkPoint> pts;
                pts.reserve(poly.size() * 2u);
                for (std::size_t k = 0; k < poly.size(); ++k) {
                    const double nx = poly[k].first;
                    const double ny = poly[k].second;
                    // Slant leans top of glyph slightly right for authentic calligraphic handwriting
                    const double slantOff = (1.0 - ny) * ch * slantRatio;
                    const double px = curX + nx * cw + slantOff;
                    const double py = curY + ny * ch;
                    // Calligraphic pressure: heavier on downward strokes, lighter on upward moves
                    float press = 0.62f;
                    if (k > 0) {
                        const double dy = poly[k].second - poly[k - 1].second;
                        press = (dy > 0.02) ? 0.80f : 0.46f;
                    }
                    pts.push_back({ px, py, press, 12.0f, -6.0f, ts += 10 });
                }
                AnnotationObject s;
                s.kind = ObjectKind::InkStroke;
                s.penStyle = InkPenStyle::Fountain;
                s.strokeColor = inkColor;
                s.strokeWidth = strokeWidth;
                s.inkPoints = InkEngine::SmoothInkStroke(pts, 0.18);
                s.htrRecognizedTextUtf8 = wordUtf8;
                s.inkStartTimeMs = s.inkPoints.front().timestampMs;
                s.inkEndTimeMs = s.inkPoints.back().timestampMs;
                s.RecomputeBounds();
                strokes.push_back(std::move(s));
            }
            curX += cw * 1.08;
        }
        pos = wEnd;
    }
    return strokes;
}

std::vector<std::uint64_t> SmartScriptReflowEngine::PasteTextAsHandwriting(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::string& textUtf8,
    double startX,
    double startY)
{
    std::vector<std::uint64_t> ids;
    auto strokes = SynthesizeHandwritingStrokesFromText(textUtf8, startX, startY);
    for (auto& s : strokes) {
        s.layerId = doc.ActiveLayerId();
        if (cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(s)))) {
            const auto& vec = ObjectsForTarget(doc, target);
            if (!vec.empty()) ids.push_back(vec.back().id);
        }
    }
    return ids;
}

std::vector<SpellingSuggestion> SmartScriptReflowEngine::CheckHandwritingSpelling(
    const DocumentModel& doc,
    ObjectTarget target)
{
    std::vector<SpellingSuggestion> out;
    const auto clusters = ClusterHandwrittenWords(doc, target);
    for (std::size_t i = 0; i < clusters.size(); ++i) {
        std::string word = clusters[i].recognizedWordUtf8;
        // Trim whitespace
        while (!word.empty() && std::isspace(static_cast<unsigned char>(word.front()))) word.erase(word.begin());
        while (!word.empty() && std::isspace(static_cast<unsigned char>(word.back()))) word.pop_back();
        if (word.empty()) continue;

        std::string suggestion;
        if (CheckWordWithWindowsSpellChecker(word, suggestion) && !suggestion.empty() && suggestion != word) {
            out.push_back({ i, word, suggestion, clusters[i].bounds });
        }
    }
    return out;
}

bool SmartScriptReflowEngine::ApplyHandwritingSpellingCorrection(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const SpellingSuggestion& suggestion)
{
    const auto clusters = ClusterHandwrittenWords(doc, target);
    if (suggestion.clusterIndex >= clusters.size()) return false;
    const auto& cl = clusters[suggestion.clusterIndex];
    if (cl.strokeIds.empty()) return false;

    if (!cmdStack.Execute(doc, std::make_unique<BatchEraseObjectsCommand>(target, cl.strokeIds))) {
        return false;
    }

    auto replacementStrokes = SynthesizeHandwritingStrokesFromText(
        suggestion.correctedWordUtf8,
        cl.bounds.x,
        cl.bounds.y,
        (std::max)(16.0, cl.bounds.h * 0.85),
        cl.dominantColor,
        cl.averageStrokeWidth);
    for (auto& s : replacementStrokes) {
        s.layerId = doc.ActiveLayerId();
        cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(s)));
    }
    return true;
}

std::size_t SmartScriptReflowEngine::ConvertHandwritingAndDiagramsOnDoubleTap(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target)
{
    const auto clusters = ClusterHandwrittenWords(doc, target);
    if (clusters.empty()) return 0;

    std::size_t converted = 0;
    for (const auto& cl : clusters) {
        if (cl.strokeIds.empty()) continue;

        // 1. If single-stroke geometric shape without HTR word, snap to Shape
        if (cl.strokeIds.size() == 1 && cl.recognizedWordUtf8.empty()) {
            const AnnotationObject* s = target.isCanvas
                ? doc.FindCanvasObject(cl.strokeIds.front())
                : doc.FindPageObjectMutable(target.pageIndex, cl.strokeIds.front());
            if (s) {
                const auto snap = InkEngine::RecognizeHeldShape(s->inkPoints);
                if (snap.recognized) {
                    AnnotationObject shapeObj = *s;
                    shapeObj.kind = ObjectKind::Shape;
                    shapeObj.shapeKind = snap.kind;
                    shapeObj.shapeStart = snap.start;
                    shapeObj.shapeEnd = snap.end;
                    shapeObj.bounds = snap.bounds;
                    shapeObj.inkSnapped = true;
                    shapeObj.inkPoints.clear();
                    cmdStack.Execute(doc, std::make_unique<ModifyObjectCommand>(target, *s, std::move(shapeObj)));
                    ++converted;
                    continue;
                }
            }
        }

        // 2. Determine text: use existing HTR or invoke real WinRT InkAnalyzer/OCR over cluster strokes
        std::string recognizedText = cl.recognizedWordUtf8;
        if (recognizedText.empty()) {
            std::vector<AnnotationObject> clusterStrokes;
            for (std::uint64_t sid : cl.strokeIds) {
                const AnnotationObject* s = target.isCanvas
                    ? doc.FindCanvasObject(sid)
                    : doc.FindPageObjectMutable(target.pageIndex, sid);
                if (s) clusterStrokes.push_back(*s);
            }
            const HtrOutput htr = WinRtInkHtrAdapter::RecognizeStrokes(clusterStrokes);
            if (htr.recognized && !htr.textUtf8.empty()) {
                recognizedText = htr.textUtf8;
            }
        }
        // Do NOT destroy ink strokes by substituting fake literal "Texto" if recognition failed
        if (recognizedText.empty()) continue;

        cmdStack.Execute(doc, std::make_unique<BatchEraseObjectsCommand>(target, cl.strokeIds));
        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        tb.layerId = doc.ActiveLayerId();
        tb.bounds = { cl.bounds.x, cl.bounds.y, (std::max)(64.0, cl.bounds.w + 16.0), (std::max)(28.0, cl.bounds.h + 8.0) };
        tb.strokeColor = cl.dominantColor;
        tb.fontSizePt = 13.5;
        tb.textUtf8 = recognizedText;
        cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(tb)));
        ++converted;
    }
    return converted;
}

bool SmartScriptReflowEngine::ApplyWordJoinOrSplitSpaceGesture(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::vector<InkPoint>& gestureStroke)
{
    if (gestureStroke.size() < 2) return false;
    const InkPoint& p0 = gestureStroke.front();
    const InkPoint& p1 = gestureStroke.back();
    const double dx = std::abs(p1.x - p0.x);
    const double dy = p1.y - p0.y;
    if (std::abs(dy) < 16.0 || dx > std::abs(dy) * 0.45) return false;

    const double splitX = 0.5 * (p0.x + p1.x);
    const double lineY = 0.5 * (p0.y + p1.y);
    const double shiftX = (dy > 0.0) ? 28.0 : -22.0;

    std::vector<AnnotationObject> beforeObjs;
    std::vector<AnnotationObject> afterObjs;
    for (const auto& obj : ObjectsForTarget(doc, target)) {
        if (obj.lockedInPlace) continue;
        const double cy = obj.bounds.y + obj.bounds.h * 0.5;
        if (std::abs(cy - lineY) <= 36.0 && obj.bounds.x >= splitX) {
            beforeObjs.push_back(obj);
            AnnotationObject moved = obj;
            moved.Translate(shiftX, 0.0);
            afterObjs.push_back(std::move(moved));
        }
    }
    if (beforeObjs.empty()) return false;
    return cmdStack.Execute(doc, std::make_unique<BatchModifyObjectsCommand>(target, std::move(beforeObjs), std::move(afterObjs)));
}

bool SmartScriptReflowEngine::UpdateZoomWritingBoxOnStroke(
    ZoomWritingBoxState& boxState,
    const std::vector<InkPoint>& latestPageStrokePoints)
{
    if (!boxState.active || latestPageStrokePoints.empty()) return false;
    double maxStrokeX = latestPageStrokePoints.front().x;
    for (const auto& pt : latestPageStrokePoints) {
        maxStrokeX = (std::max)(maxStrokeX, pt.x);
    }
    const double triggerX = boxState.pageTargetRect.x + boxState.pageTargetRect.w * boxState.autoAdvanceTriggerRatio;
    if (maxStrokeX < triggerX) return false;

    const double advanceStep = boxState.pageTargetRect.w * 0.65;
    if (boxState.pageTargetRect.x + advanceStep + boxState.pageTargetRect.w <= boxState.rightMarginPt) {
        boxState.pageTargetRect.x += advanceStep;
        ++boxState.autoAdvanceCount;
    } else {
        boxState.pageTargetRect.x = boxState.leftMarginPt;
        boxState.pageTargetRect.y += boxState.lineHeightPt;
        ++boxState.autoAdvanceCount;
        ++boxState.lineWrapCount;
    }
    return true;
}

void SmartScriptReflowEngine::HandleStylusDoubleTapToggle(FloatingInkToolbarState& toolbar) noexcept {
    toolbar.eraserActive = !toolbar.eraserActive;
    if (!toolbar.eraserActive) {
        toolbar.activePen = toolbar.previousPen;
    } else {
        toolbar.previousPen = toolbar.activePen;
    }
}

} // namespace DocStudio
