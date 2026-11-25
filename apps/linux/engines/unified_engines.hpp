#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace CasioLinux {

enum class CalcModelId : int {
    BlancaClassWiz = 0, // fx-570LA X / 991LA X (644x1250, native 192x63 + indicators)
    GrisEsPlus     = 1, // fx-570VN PLUS       (405x816,  native 96x31  + indicators)
    Cg50Prizm      = 2, // fx-CG50 PRIZM       (664x1364, native 384x216 RGB)
    NegraCw2       = 3  // fx-991CW ClassWiz II(992x2082, Wine64/CasioEmuX or internal fallback)
};

struct SkinRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int Width() const noexcept { return right - left; }
    int Height() const noexcept { return bottom - top; }
};

struct KeyDef {
    int id = 0;
    int cx = 0;
    int cy = 0;
    int w = 0;
    int h = 0;
    std::uint16_t matrixCode = 0;
    std::string name;
};

struct AssetPaths {
    std::string rootDir;          // Core Oficial root (or dist root)
    std::string romsDir;          // contains fx991lax.rom, fx570vnplus.rom
    std::string cg50RomsDir;      // contains SystemROM.raw, UserROM.raw, C2ROM.raw, official-ILRAM-FD800000-4000.bin
    std::string skinsDir;         // contains caratula_*.png / extracted_* bitmaps
    std::string fx991cwDir;       // contains runtime/fx991cw
    std::string manualsDir;       // contains 16 PDFs + indice_paginas_pdf.tsv
    std::string catalogJsonPath;  // catalog/casio_knowledge_catalog.json
    std::string documentStudioBin;// sibling document-studio binary path
};

// Discovers Core Oficial / dist-linux asset directories relative to argv[0], cwd, or CASIO_UNIFIED_HOME.
AssetPaths DiscoverAssetPaths(const char* argv0 = nullptr);

// Metadata & exact key tables (pure UTF-8, zero Win32 dependencies)
int GetSkinWidth(CalcModelId model);
int GetSkinHeight(CalcModelId model);
SkinRect GetLcdSkinRect(CalcModelId model);
const std::vector<KeyDef>& GetKeysForModel(CalcModelId model);

// Unified C++ wrapper around:
//   - CalcEmuCore (nX-U8) for Blanca (hw=4) & Gris (hw=3)
//   - Cg50NativeCore (SH7305/SH-4A + R61524) for fx-CG50 PRIZM
//   - Cw2LinuxHost (Wine64 casioemu.exe process host + built-in scientific fallback) for fx-991CW
class UnifiedCalcEngine {
public:
    UnifiedCalcEngine();
    ~UnifiedCalcEngine();

    UnifiedCalcEngine(const UnifiedCalcEngine&) = delete;
    UnifiedCalcEngine& operator=(const UnifiedCalcEngine&) = delete;

    bool Initialize(CalcModelId model, const AssetPaths& paths, std::string* outErr = nullptr);
    void Shutdown();

    bool IsReady() const noexcept;
    CalcModelId Model() const noexcept;
    std::string StatusText() const;
    bool UsingExternalProcess() const noexcept; // true when fx-991CW launched external Wine64 window

    // Advance CPU simulation by milliseconds (Blanca/Gris) or instruction frames (CG50)
    void StepMs(std::uint32_t ms);

    // Key input
    void KeyDown(std::uint16_t matrixCode);
    void KeyUp(std::uint16_t matrixCode = 0);
    void TapKey(std::uint16_t matrixCode, std::uint32_t holdMs = 40, std::uint32_t settleMs = 100);
    void TapKeyById(int keyId, std::uint32_t holdMs = 40, std::uint32_t settleMs = 100);
    void Reset();

    // LCD framebuffer in top-down RGBA8888 format.
    // Width/Height: Blanca 192x63, Gris 96x31, CG50 384x216, CW2 fallback 192x64.
    int ScreenWidth() const noexcept;
    int ScreenHeight() const noexcept;
    // outIndicators bitmask (nX-U8):
    // bit 0:S, 1:A, 2:M, 3:STO, 4:Math, 5:D, 6:R, 7:G, 8:FIX, 9:SCI, 10:DOWN, 11:UP
    bool RenderRgba(std::vector<std::uint8_t>& outRgba, std::uint32_t* outIndicators = nullptr, bool forceCopy = true);

    // Draws top indicator strip (RGBA8888) for Blanca (192x8) or Gris (288x12) using indicator bitmask.
    static void RenderIndicatorStripRgba(
        CalcModelId model,
        std::uint32_t indicatorsMask,
        std::uint8_t fgR, std::uint8_t fgG, std::uint8_t fgB,
        std::uint8_t bgR, std::uint8_t bgG, std::uint8_t bgB,
        std::vector<std::uint8_t>& outRgba,
        int& outW,
        int& outH);

    // RAM/Snapshot state persistence (supported on Blanca, Gris, CG50, and fallback CW2)
    std::vector<std::uint8_t> ExportState() const;
    bool ImportState(const std::vector<std::uint8_t>& state);

    // Wine64 external process management for fx-991CW (no-op on native engines)
    bool LaunchExternalCw2Window(std::string* outDiag = nullptr);
    bool IsExternalCw2Running() const;
    void StopExternalCw2();
    bool SendKeyToCw2OrFallback(int keyId, std::string* outNote = nullptr);
    std::string GetFallbackExpression() const;
    std::string GetFallbackResult() const;
    std::string GetFallbackModeName() const;
    std::uint32_t GetFallbackIndicatorsMask() const;
    std::string EvaluateFallbackComplex(const std::string& expr);
    void SetFallbackMatrixA(int dim, const std::vector<double>& rowMajor);
    void SetFallbackMatrixB(int dim, const std::vector<double>& rowMajor);
    std::string EvaluateFallbackMatrixOp(const std::string& opName);
    std::vector<std::pair<double, std::string>> ComputeFallbackTable(
        const std::string& fxExpr, double start, double end, double step);
    std::string SolveFallbackSimultaneous(int nVars, const std::vector<double>& coeffs);
    std::string SolveFallbackQuadratic(double a, double b, double c);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Pure-C++ expression evaluator parity with calc_engine.h (ASCII/UTF-8 interface)
struct EvalOutput {
    bool ok = false;
    bool isError = false;
    double value = 0.0;
    std::string formatted;
    std::string errorMessage;
};

EvalOutput EvaluateMathUtf8(const std::string& expr, bool degMode = true, double lastAns = 0.0);

} // namespace CasioLinux
