#include "unified_engines.hpp"
#include "casio_ffi.h"
#include "u13_lcd_font_helper.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace CasioLinux;

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool cond, const std::string& label, const std::string& detail = {}) {
    if (cond) {
        ++g_passed;
        std::cout << "[PASS] " << label;
        if (!detail.empty()) std::cout << " (" << detail << ")";
        std::cout << "\n";
    } else {
        ++g_failed;
        std::cerr << "[FAIL] " << label;
        if (!detail.empty()) std::cerr << " -- " << detail;
        std::cerr << "\n";
    }
}

int FindKeyId(CalcModelId model, std::initializer_list<const char*> candidates) {
    const auto& keys = GetKeysForModel(model);
    for (const char* cand : candidates) {
        for (const auto& k : keys) {
            if (k.name == cand) return k.id;
        }
    }
    return -1;
}

std::size_t CountNonMatchingPixels(const std::vector<std::uint8_t>& rgba,
                                   std::uint8_t r0, std::uint8_t g0, std::uint8_t b0) {
    std::size_t count = 0;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        if (rgba[i] != r0 || rgba[i + 1] != g0 || rgba[i + 2] != b0) {
            ++count;
        }
    }
    return count;
}

std::size_t CountUniqueRgb(const std::vector<std::uint8_t>& rgba) {
    std::set<std::uint32_t> colors;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const std::uint32_t c = (static_cast<std::uint32_t>(rgba[i]) << 16) |
                                (static_cast<std::uint32_t>(rgba[i + 1]) << 8) |
                                static_cast<std::uint32_t>(rgba[i + 2]);
        colors.insert(c);
        if (colors.size() > 64) break;
    }
    return colors.size();
}

std::string DumpColorSummary(const std::vector<std::uint8_t>& rgba) {
    std::set<std::uint32_t> colors;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        colors.insert((static_cast<std::uint32_t>(rgba[i]) << 16) |
                      (static_cast<std::uint32_t>(rgba[i + 1]) << 8) |
                      static_cast<std::uint32_t>(rgba[i + 2]));
        if (colors.size() >= 16) break;
    }
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (auto c : colors) {
        if (!first) oss << ",";
        first = false;
        oss << std::hex << c;
    }
    oss << "]";
    return oss.str();
}

std::string HexStr(std::uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << v;
    return oss.str();
}

std::size_t DiffPixels(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
    if (a.size() != b.size()) return a.size();
    std::size_t diff = 0;
    for (std::size_t i = 0; i + 3 < a.size(); i += 4) {
        if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) {
            ++diff;
        }
    }
    return diff;
}

void TestAssetDiscovery(const char* argv0, AssetPaths& outPaths) {
    std::cout << "\n=== 1. Asset Discovery ===\n";
    outPaths = DiscoverAssetPaths(argv0);

    Check(!outPaths.rootDir.empty() && fs::exists(outPaths.rootDir),
          "DiscoverAssetPaths rootDir", outPaths.rootDir);
    Check(!outPaths.romsDir.empty() &&
              fs::exists(fs::path(outPaths.romsDir) / "fx991lax.rom") &&
              fs::exists(fs::path(outPaths.romsDir) / "fx570vnplus.rom"),
          "DiscoverAssetPaths romsDir contains fx991lax.rom & fx570vnplus.rom", outPaths.romsDir);
    if (!outPaths.cg50RomsDir.empty()) {
        Check(fs::exists(fs::path(outPaths.cg50RomsDir) / "SystemROM.raw") &&
                  fs::exists(fs::path(outPaths.cg50RomsDir) / "UserROM.raw") &&
                  fs::exists(fs::path(outPaths.cg50RomsDir) / "C2ROM.raw") &&
                  fs::exists(fs::path(outPaths.cg50RomsDir) / "official-ILRAM-FD800000-4000.bin"),
              "DiscoverAssetPaths cg50RomsDir has all 4 CG50 ROM/ILRAM binaries", outPaths.cg50RomsDir);
    } else {
        std::cout << "[INFO] cg50RomsDir absent (optional external CG50 ROMs not bundled)\n";
    }
    Check(!outPaths.skinsDir.empty() &&
              (fs::exists(fs::path(outPaths.skinsDir) / "caratula_classwiz.png") ||
               fs::exists(fs::path(outPaths.skinsDir) / "bitmap_103.bmp")),
          "DiscoverAssetPaths skinsDir resolves PNG skins or in-repo bitmap fallback", outPaths.skinsDir);
    Check(!outPaths.fx991cwDir.empty() &&
              fs::exists(fs::path(outPaths.fx991cwDir) / "emulator" / "casioemu.exe"),
          "DiscoverAssetPaths fx991cwDir contains casioemu.exe", outPaths.fx991cwDir);
    Check(!outPaths.catalogJsonPath.empty() && fs::exists(outPaths.catalogJsonPath),
          "DiscoverAssetPaths catalogJsonPath exists", outPaths.catalogJsonPath);

    int pdfCount = 0;
    if (!outPaths.manualsDir.empty() && fs::is_directory(outPaths.manualsDir)) {
        for (const auto& entry : fs::directory_iterator(outPaths.manualsDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pdf") {
                ++pdfCount;
            }
        }
    }
    Check(pdfCount == 16 && fs::exists(fs::path(outPaths.manualsDir) / "indice_paginas_pdf.tsv"),
          "DiscoverAssetPaths manualsDir contains 16 PDFs + TSV index",
          std::to_string(pdfCount) + " PDFs");

    // Verify CASIO_UNIFIED_HOME env var honors Core Oficial path
    const char* prevEnv = std::getenv("CASIO_UNIFIED_HOME");
    std::string savedEnv = prevEnv ? prevEnv : "";
    setenv("CASIO_UNIFIED_HOME", outPaths.rootDir.c_str(), 1);
    AssetPaths envPaths = DiscoverAssetPaths("/nonexistent/bin");
    Check(fs::weakly_canonical(envPaths.rootDir) == fs::weakly_canonical(outPaths.rootDir) &&
              !envPaths.romsDir.empty(),
          "CASIO_UNIFIED_HOME override resolves assets", envPaths.rootDir);
    if (prevEnv) {
        setenv("CASIO_UNIFIED_HOME", savedEnv.c_str(), 1);
    } else {
        unsetenv("CASIO_UNIFIED_HOME");
    }
}

void TestMetadataAndKeys() {
    std::cout << "\n=== 2. Metadata & Key Tables ===\n";
    Check(GetSkinWidth(CalcModelId::BlancaClassWiz) == 644 &&
              GetSkinHeight(CalcModelId::BlancaClassWiz) == 1250 &&
              GetKeysForModel(CalcModelId::BlancaClassWiz).size() == 50,
          "Blanca skin 644x1250 & 50 keys");
    Check(GetSkinWidth(CalcModelId::GrisEsPlus) == 405 &&
              GetSkinHeight(CalcModelId::GrisEsPlus) == 816 &&
              GetKeysForModel(CalcModelId::GrisEsPlus).size() == 50,
          "Gris skin 405x816 & 50 keys");
    Check(GetSkinWidth(CalcModelId::Cg50Prizm) == 664 &&
              GetSkinHeight(CalcModelId::Cg50Prizm) == 1364 &&
              GetKeysForModel(CalcModelId::Cg50Prizm).size() == 50,
          "CG50 skin 664x1364 & 50 keys");
    Check(GetSkinWidth(CalcModelId::NegraCw2) == 992 &&
              GetSkinHeight(CalcModelId::NegraCw2) == 2082 &&
              GetKeysForModel(CalcModelId::NegraCw2).size() == 48,
          "CW2 skin 992x2082 & 48 keys");

    auto rectBlanca = GetLcdSkinRect(CalcModelId::BlancaClassWiz);
    auto rectCg50 = GetLcdSkinRect(CalcModelId::Cg50Prizm);
    Check(rectBlanca.Width() > 300 && rectBlanca.Height() > 100 &&
              rectCg50.Width() > 400 && rectCg50.Height() > 200,
          "LCD skin rectangles non-degenerate");
}

void TestBlancaClassWiz(const AssetPaths& paths) {
    std::cout << "\n=== 3. Blanca ClassWiz (fx991lax.rom, hw=4) ===\n";
    UnifiedCalcEngine eng;
    std::string err;
    const bool ok = eng.Initialize(CalcModelId::BlancaClassWiz, paths, &err);
    Check(ok && eng.IsReady(), "Blanca Initialize", err);
    if (!ok) return;

    Check(eng.ScreenWidth() == 192 && eng.ScreenHeight() == 63, "Blanca LCD size 192x63");

    std::vector<std::uint8_t> initFrame;
    std::uint32_t initInd = 0;
    const bool rendered0 = eng.RenderRgba(initFrame, &initInd, true);
    Check(rendered0 && initFrame.size() == 192u * 63u * 4u, "Blanca initial RenderRgba size");

    const std::size_t initFg = CountNonMatchingPixels(initFrame, initFrame[0], initFrame[1], initFrame[2]);
    std::size_t cursorOrContentFg = initFg;
    for (int i = 0; i < 6 && cursorOrContentFg == 0; ++i) {
        eng.StepMs(200);
        std::vector<std::uint8_t> blinkFrame;
        eng.RenderRgba(blinkFrame, &initInd, true);
        cursorOrContentFg = CountNonMatchingPixels(blinkFrame, blinkFrame[0], blinkFrame[1], blinkFrame[2]);
    }
    Check(initInd != 0 || cursorOrContentFg > 0,
          "Blanca initial boot active (indicators or blinking cursor)",
          "ind=" + HexStr(initInd) + " fg=" + std::to_string(cursorOrContentFg));

    // Tap SHIFT to verify indicator bit 0 flips and top indicator strip draws glyphs
    const int kShift = FindKeyId(CalcModelId::BlancaClassWiz, {"SHIFT"});
    const int kAc = FindKeyId(CalcModelId::BlancaClassWiz, {"AC"});
    const int k7 = FindKeyId(CalcModelId::BlancaClassWiz, {"7"});
    const int kMul = FindKeyId(CalcModelId::BlancaClassWiz, {"POR", "*", "x"});
    const int k6 = FindKeyId(CalcModelId::BlancaClassWiz, {"6"});
    const int kEq = FindKeyId(CalcModelId::BlancaClassWiz, {"IGUAL", "="});
    Check(kShift >= 0 && kAc >= 0 && k7 >= 0 && kMul >= 0 && k6 >= 0 && kEq >= 0,
          "Blanca key lookup succeeded");

    // Compute 7 * 6 = using default TapKeyById timing (tests 400/300 floor in unified_engines.cpp)
    eng.TapKeyById(kAc);
    eng.TapKeyById(k7);
    eng.TapKeyById(kMul);
    eng.TapKeyById(k6);
    eng.TapKeyById(kEq);

    std::vector<std::uint8_t> resFrame;
    std::uint32_t resInd = 0;
    eng.RenderRgba(resFrame, &resInd, true);
    const std::size_t diffInit = DiffPixels(initFrame, resFrame);
    const std::size_t resFg = CountNonMatchingPixels(resFrame, resFrame[0], resFrame[1], resFrame[2]);
    Check(resFg >= 200 && diffInit >= 200,
          "Blanca 7*6= updates LCD framebuffer (7x6 -> 42)",
          "fg=" + std::to_string(resFg) + " diffVsInit=" + std::to_string(diffInit));

    // Snapshot Export/Import roundtrip
    auto snap = eng.ExportState();
    Check(!snap.empty(), "Blanca ExportState non-empty", "bytes=" + std::to_string(snap.size()));
    eng.TapKeyById(kAc);
    std::vector<std::uint8_t> afterAc;
    eng.RenderRgba(afterAc, nullptr, true);
    const std::size_t diffAc = DiffPixels(resFrame, afterAc);
    Check(diffAc > 20, "Blanca AC clears screen before ImportState", "diff=" + std::to_string(diffAc));

    const bool impOk = eng.ImportState(snap);
    std::vector<std::uint8_t> restoredFrame;
    eng.RenderRgba(restoredFrame, nullptr, true);
    Check(impOk && DiffPixels(resFrame, restoredFrame) == 0,
          "Blanca ImportState restores exact 7*6= LCD state");

    // Tap SHIFT to verify indicator bit 0 flips and top indicator strip draws glyphs
    eng.TapKeyById(kShift);
    std::vector<std::uint8_t> shiftFrame;
    std::uint32_t shiftInd = 0;
    eng.RenderRgba(shiftFrame, &shiftInd, true);
    Check((shiftInd & 0x01u) != 0u, "Blanca SHIFT indicator bit 0 active",
          "mask=" + HexStr(shiftInd));

    std::vector<std::uint8_t> strip;
    int stripW = 0, stripH = 0;
    UnifiedCalcEngine::RenderIndicatorStripRgba(
        CalcModelId::BlancaClassWiz, shiftInd, 20, 36, 58, 207, 230, 219, strip, stripW, stripH);
    const std::size_t stripFg = CountNonMatchingPixels(strip, 207, 230, 219);
    Check(stripW == 192 && stripH == 8 && stripFg > 10,
          "Blanca RenderIndicatorStripRgba 192x8 draws SHIFT glyph", "fg=" + std::to_string(stripFg));
}

void TestGrisEsPlus(const AssetPaths& paths) {
    std::cout << "\n=== 4. Gris ES PLUS (fx570vnplus.rom, hw=3) ===\n";
    UnifiedCalcEngine eng;
    std::string err;
    const bool ok = eng.Initialize(CalcModelId::GrisEsPlus, paths, &err);
    Check(ok && eng.IsReady(), "Gris Initialize", err);
    if (!ok) return;

    Check(eng.ScreenWidth() == 96 && eng.ScreenHeight() == 31, "Gris LCD size 96x31");

    std::vector<std::uint8_t> initFrame;
    std::uint32_t initInd = 0;
    const bool rendered0 = eng.RenderRgba(initFrame, &initInd, true);
    Check(rendered0 && initFrame.size() == 96u * 31u * 4u, "Gris initial RenderRgba size");

    const std::size_t initFg = CountNonMatchingPixels(initFrame, initFrame[0], initFrame[1], initFrame[2]);
    std::size_t cursorOrContentFg = initFg;
    for (int i = 0; i < 6 && cursorOrContentFg == 0; ++i) {
        eng.StepMs(200);
        std::vector<std::uint8_t> blinkFrame;
        eng.RenderRgba(blinkFrame, &initInd, true);
        cursorOrContentFg = CountNonMatchingPixels(blinkFrame, blinkFrame[0], blinkFrame[1], blinkFrame[2]);
    }
    Check(initInd != 0 || cursorOrContentFg > 0,
          "Gris initial boot active (indicators or blinking cursor)",
          "ind=" + HexStr(initInd) + " fg=" + std::to_string(cursorOrContentFg));

    const int kShift = FindKeyId(CalcModelId::GrisEsPlus, {"SHIFT"});
    const int kAc = FindKeyId(CalcModelId::GrisEsPlus, {"AC"});
    const int k2 = FindKeyId(CalcModelId::GrisEsPlus, {"2"});
    const int kPlus = FindKeyId(CalcModelId::GrisEsPlus, {"MAS", "+"});
    const int k3 = FindKeyId(CalcModelId::GrisEsPlus, {"3"});
    const int kEq = FindKeyId(CalcModelId::GrisEsPlus, {"IGUAL", "="});
    Check(kShift >= 0 && kAc >= 0 && k2 >= 0 && kPlus >= 0 && k3 >= 0 && kEq >= 0,
          "Gris key lookup succeeded");

    // Compute 2 + 3 = using default TapKeyById timing (tests 400/300 floor in unified_engines.cpp)
    eng.TapKeyById(kAc);
    eng.TapKeyById(k2);
    eng.TapKeyById(kPlus);
    eng.TapKeyById(k3);
    eng.TapKeyById(kEq);

    std::vector<std::uint8_t> resFrame;
    eng.RenderRgba(resFrame, nullptr, true);
    const std::size_t diffInit = DiffPixels(initFrame, resFrame);
    const std::size_t resFg = CountNonMatchingPixels(resFrame, resFrame[0], resFrame[1], resFrame[2]);
    Check(resFg >= 55 && resFg <= 150 && diffInit >= 55,
          "Gris 2+3= updates LCD framebuffer (2+3 -> 5, non-error)",
          "fg=" + std::to_string(resFg) + " diffVsInit=" + std::to_string(diffInit));

    auto snap = eng.ExportState();
    Check(!snap.empty(), "Gris ExportState non-empty", "bytes=" + std::to_string(snap.size()));
    eng.TapKeyById(kAc);
    const bool impOk = eng.ImportState(snap);
    std::vector<std::uint8_t> restoredFrame;
    eng.RenderRgba(restoredFrame, nullptr, true);
    Check(impOk && DiffPixels(resFrame, restoredFrame) == 0,
          "Gris ImportState restores exact 2+3= LCD state");

    eng.TapKeyById(kAc);
    eng.TapKeyById(kShift);
    std::vector<std::uint8_t> shiftFrame;
    std::uint32_t shiftInd = 0;
    eng.RenderRgba(shiftFrame, &shiftInd, true);
    Check((shiftInd & 0x01u) != 0u, "Gris SHIFT indicator bit 0 active",
          "mask=" + HexStr(shiftInd));

    std::vector<std::uint8_t> strip;
    int stripW = 0, stripH = 0;
    UnifiedCalcEngine::RenderIndicatorStripRgba(
        CalcModelId::GrisEsPlus, shiftInd, 20, 36, 58, 207, 230, 219, strip, stripW, stripH);
    const std::size_t stripFg = CountNonMatchingPixels(strip, 207, 230, 219);
    Check(stripW == 288 && stripH == 12 && stripFg > 10,
          "Gris RenderIndicatorStripRgba 288x12 draws SHIFT glyph", "fg=" + std::to_string(stripFg));
}

void TestCg50Prizm(const AssetPaths& paths) {
    std::cout << "\n=== 5. fx-CG50 PRIZM (SH7305 + R61524 native) ===\n";

    // Always verify graceful failure when cg50RomsDir is empty/missing
    {
        AssetPaths emptyPaths{};
        UnifiedCalcEngine missingEng;
        std::string missErr;
        const bool missOk = missingEng.Initialize(CalcModelId::Cg50Prizm, emptyPaths, &missErr);
        Check(!missOk && !missingEng.IsReady() && !missErr.empty(),
              "CG50 Initialize fails gracefully with empty cg50RomsDir", missErr);
    }

    if (paths.cg50RomsDir.empty()) {
        std::cout << "[INFO] Skipping live CG50 ROM boot tests (cg50RomsDir absent)\n";
        return;
    }

    UnifiedCalcEngine eng;
    std::string err;
    const bool ok = eng.Initialize(CalcModelId::Cg50Prizm, paths, &err);
    Check(ok && eng.IsReady(), "CG50 Initialize", err);
    if (!ok) return;

    Check(eng.ScreenWidth() == 384 && eng.ScreenHeight() == 216, "CG50 LCD size 384x216");

    std::vector<std::uint8_t> frame;
    bool hasColorFrame = false;
    std::size_t uniq = 0;
    std::size_t nonBg = 0;
    std::uint32_t totalMs = 0;
    bool sentMenu = false;
    const int kMenu = FindKeyId(CalcModelId::Cg50Prizm, {"MENU"});
    const int kF1 = FindKeyId(CalcModelId::Cg50Prizm, {"F1"});
    Check(kF1 >= 0 && kMenu >= 0, "CG50 key lookup succeeded");

    for (int step = 0; step < 100; ++step) {
        eng.StepMs(100);
        totalMs += 100;
        if (eng.RenderRgba(frame, nullptr, true) && frame.size() == 384u * 216u * 4u) {
            uniq = CountUniqueRgb(frame);
            nonBg = CountNonMatchingPixels(frame, frame[0], frame[1], frame[2]);
            // Cold boot stops at "MAIN MEMORY / Press [MENU] KEY" dialog (3 colors);
            // tap MENU once to dismiss into the colorful PRIZM icon menu.
            if (!sentMenu && nonBg > 1000 && uniq == 3 && kMenu >= 0) {
                sentMenu = true;
                eng.TapKeyById(kMenu, 100, 500);
                totalMs += 600;
                if (eng.RenderRgba(frame, nullptr, true)) {
                    uniq = CountUniqueRgb(frame);
                    nonBg = CountNonMatchingPixels(frame, frame[0], frame[1], frame[2]);
                }
            }
            if (uniq > 10 && nonBg > 500) {
                hasColorFrame = true;
                break;
            }
        }
    }
    Check(hasColorFrame,
          "CG50 boots to non-blank colorful 384x216 RGB framebuffer",
          "bootMs=" + std::to_string(totalMs) + " uniqColors=" + std::to_string(uniq) +
              " nonBg=" + std::to_string(nonBg) + " palette=" + DumpColorSummary(frame));

    auto snap = eng.ExportState();
    Check(!snap.empty(), "CG50 ExportState non-empty", "bytes=" + std::to_string(snap.size()));

    // Mutate state by pressing F1 or stepping, then restore snapshot
    eng.TapKeyById(kF1, 60, 150);
    eng.StepMs(200);
    const bool impOk = eng.ImportState(snap);
    std::vector<std::uint8_t> restored;
    const bool rendAfterImp = eng.RenderRgba(restored, nullptr, true);
    Check(impOk && rendAfterImp && DiffPixels(frame, restored) == 0,
          "CG50 ImportState round-trip restores exact 384x216 RGB framebuffer");
}

void TestNegraCw2(const AssetPaths& paths) {
    std::cout << "\n=== 6. Negra fx-991CW (Built-in fallback + Wine64 host check) ===\n";
    UnifiedCalcEngine eng;
    std::string err;
    const bool ok = eng.Initialize(CalcModelId::NegraCw2, paths, &err);
    Check(ok && eng.IsReady(), "CW2 Initialize", err);
    if (!ok) return;

    Check(eng.ScreenWidth() == 192 && eng.ScreenHeight() == 64, "CW2 LCD size 192x64");
    Check(!eng.UsingExternalProcess(), "CW2 defaults to built-in fallback (not external)");
    Check(eng.StatusText().find("Modo Nativo Integrado (fx-991CW)") != std::string::npos,
          "CW2 StatusText reports built-in native mode", eng.StatusText());

    std::string diag;
    const bool launched = eng.LaunchExternalCw2Window(&diag);
    Check(!launched && !eng.IsExternalCw2Running(),
          "CW2 LaunchExternalCw2Window degrades gracefully without Wine64/X11", diag);

    // Interactive key input: 2 + 3 * 4 EXE -> 14
    const int k2 = FindKeyId(CalcModelId::NegraCw2, {"2"});
    const int kPlus = FindKeyId(CalcModelId::NegraCw2, {"MAS", "+"});
    const int k3 = FindKeyId(CalcModelId::NegraCw2, {"3"});
    const int kMul = FindKeyId(CalcModelId::NegraCw2, {"POR", "*", "x"});
    const int k4 = FindKeyId(CalcModelId::NegraCw2, {"4"});
    const int kExe = FindKeyId(CalcModelId::NegraCw2, {"EXE", "="});
    const int kAc = FindKeyId(CalcModelId::NegraCw2, {"AC"});
    Check(k2 >= 0 && kPlus >= 0 && k3 >= 0 && kMul >= 0 && k4 >= 0 && kExe >= 0 && kAc >= 0,
          "CW2 key lookup succeeded");

    eng.TapKeyById(k2, 20, 20);
    eng.TapKeyById(kPlus, 20, 20);
    eng.TapKeyById(k3, 20, 20);
    eng.TapKeyById(kMul, 20, 20);
    eng.TapKeyById(k4, 20, 20);
    eng.TapKeyById(kExe, 20, 20);

    Check(eng.GetFallbackResult() == "14",
          "CW2 interactive fallback evaluates 2+3*4 -> 14",
          "expr=" + eng.GetFallbackExpression() + " res=" + eng.GetFallbackResult());

    std::vector<std::uint8_t> lcd;
    const bool rendOk = eng.RenderRgba(lcd, nullptr, true);
    const std::size_t fg = CountNonMatchingPixels(lcd, 207, 230, 219);
    Check(rendOk && lcd.size() == 192u * 64u * 4u && fg > 150,
          "CW2 fallback renders non-blank 192x64 RGBA", "fgPixels=" + std::to_string(fg));

    auto snap = eng.ExportState();
    eng.TapKeyById(kAc, 20, 20);
    Check(eng.GetFallbackExpression().empty() && eng.GetFallbackResult() == "0",
          "CW2 AC clears fallback expression & resets result to 0");
    const bool impOk = eng.ImportState(snap);
    Check(impOk && eng.GetFallbackResult() == "14",
          "CW2 ExportState/ImportState restores expression & result");

    // Upgraded Cw2FallbackEngine modes & indicators
    const int kHome = FindKeyId(CalcModelId::NegraCw2, {"HOME"});
    const int k1 = FindKeyId(CalcModelId::NegraCw2, {"1"});
    const int kShift = FindKeyId(CalcModelId::NegraCw2, {"SHIFT"});
    const int kVar = FindKeyId(CalcModelId::NegraCw2, {"VARIABLE"});
    const int kDown = FindKeyId(CalcModelId::NegraCw2, {"ABAJO"});
    Check(kHome >= 0 && k1 >= 0 && kShift >= 0 && kVar >= 0 && kDown >= 0,
          "CW2 extended mode keys present");

    eng.TapKeyById(kHome, 10, 10);
    Check(eng.GetFallbackModeName() == "HOME", "CW2 HOME key opens HOME launcher");
    eng.TapKeyById(k2, 10, 10);
    Check(eng.GetFallbackModeName() == "CMPLX", "CW2 HOME->2 enters Complex mode");

    const std::string cRect = eng.EvaluateFallbackComplex("3+4i");
    Check(cRect == "3+4i", "CW2 Complex evaluates 3+4i", cRect);
    const int kFmt = FindKeyId(CalcModelId::NegraCw2, {"FORMAT"});
    eng.TapKeyById(kFmt, 10, 10);
    const std::string cPol = eng.GetFallbackResult();
    Check(cPol.find("5\xE2\x88\xA0" "53.1301") != std::string::npos,
          "CW2 Complex FORMAT toggles to Polar r∠θ°", cPol);
    eng.TapKeyById(kFmt, 10, 10); // back to rect
    Check(eng.EvaluateFallbackComplex("(1+2i)*(3-4i)") == "11+2i",
          "CW2 Complex multiplication (1+2i)*(3-4i) == 11+2i");

    eng.SetFallbackMatrixA(2, {2.0, 1.0, 1.0, 3.0});
    eng.SetFallbackMatrixB(2, {1.0, 4.0, 2.0, 0.0});
    Check(eng.EvaluateFallbackMatrixOp("det(MatA)") == "5",
          "CW2 Matrix det([[2,1],[1,3]]) == 5");
    Check(eng.EvaluateFallbackMatrixOp("tr(MatA)") == "5",
          "CW2 Matrix tr([[2,1],[1,3]]) == 5");
    Check(eng.EvaluateFallbackMatrixOp("MatA+MatB") == "[[3,5],[3,3]]",
          "CW2 Matrix MatA+MatB");
    Check(eng.EvaluateFallbackMatrixOp("inv(MatA)") == "[[0.6,-0.2],[-0.2,0.4]]",
          "CW2 Matrix inv(MatA)");

    const auto tbl = eng.ComputeFallbackTable("x^2", 1.0, 5.0, 1.0);
    Check(tbl.size() == 5 && tbl[0].second == "1" && tbl[4].second == "25",
          "CW2 Table x^2 (1..5) produces 5 rows");
    const std::uint32_t tblMask0 = eng.GetFallbackIndicatorsMask();
    Check((tblMask0 & (1u << 10)) != 0u, "CW2 Table DOWN indicator active on page 1");
    eng.TapKeyById(kDown, 10, 10);
    const std::uint32_t tblMask1 = eng.GetFallbackIndicatorsMask();
    Check((tblMask1 & (1u << 11)) != 0u, "CW2 Table UP indicator active after scroll");

    const std::string eqSimul2 = eng.SolveFallbackSimultaneous(2, {2.0, 1.0, 7.0, 1.0, -1.0, 2.0});
    Check(eqSimul2 == "x=3, y=1", "CW2 Equation 2-var simultaneous solver", eqSimul2);
    const std::string eqSimul3 = eng.SolveFallbackSimultaneous(3, {
        1.0, 1.0, 1.0, 6.0,
        2.0, -1.0, 1.0, 3.0,
        1.0, 2.0, -1.0, 2.0
    });
    Check(eqSimul3 == "x=1, y=2, z=3", "CW2 Equation 3-var simultaneous solver", eqSimul3);
    const std::string eqQuadReal = eng.SolveFallbackQuadratic(1.0, -5.0, 6.0);
    Check(eqQuadReal == "x1=3, x2=2", "CW2 Equation quadratic real roots", eqQuadReal);
    const std::string eqQuadCplx = eng.SolveFallbackQuadratic(1.0, 2.0, 5.0);
    Check(eqQuadCplx == "x1=-1+2i, x2=-1-2i", "CW2 Equation quadratic complex roots", eqQuadCplx);

    // Return to Calculate via HOME -> 1 and verify SHIFT + ALPHA + RenderIndicatorStripRgba
    eng.TapKeyById(kHome, 10, 10);
    eng.TapKeyById(k1, 10, 10);
    Check(eng.GetFallbackModeName() == "CALC", "CW2 HOME->1 returns to Calculate mode");
    eng.TapKeyById(kShift, 10, 10);
    eng.TapKeyById(kVar, 10, 10); // toggles ALPHA
    const std::uint32_t alphaMask = eng.GetFallbackIndicatorsMask();
    Check((alphaMask & (1u << 1)) != 0u, "CW2 SHIFT+VARIABLE sets ALPHA indicator bit 1");

    std::vector<std::uint8_t> cw2Strip;
    int cw2W = 0, cw2H = 0;
    UnifiedCalcEngine::RenderIndicatorStripRgba(
        CalcModelId::NegraCw2, alphaMask, 20, 36, 58, 207, 230, 219, cw2Strip, cw2W, cw2H);
    Check(cw2W == 192 && cw2H == 8 && CountNonMatchingPixels(cw2Strip, 207, 230, 219) > 10,
          "CW2 RenderIndicatorStripRgba renders 192x8 strip");
}

void TestU13EighteenCaseRomAndDifferentialMatrix(const AssetPaths& paths) {
    std::cout << "\n=== 8. U-13 18-Case ROM & Differential Matrix ===\n";
    const auto& cases = U13Matrix::Get18Cases();
    Check(cases.size() == 18, "U-13 matrix has exactly 18 cases");

    // 1) Blanca ClassWiz (fx-570LA X) in LineIO mode (comma decimal)
    {
        UnifiedCalcEngine lax;
        std::string err;
        const bool ok = lax.Initialize(CalcModelId::BlancaClassWiz, paths, &err);
        Check(ok, "U-13 Blanca initialize", err);
        if (ok) {
            // LineIO setup: SHIFT (0x07), MENU (0x47), 1 (0x00), 3 (0x20), settle 2000ms
            lax.TapKey(0x07, 400, 1500);
            lax.TapKey(0x47, 400, 1500);
            lax.TapKey(0x00, 400, 1500);
            lax.TapKey(0x20, 400, 2000);

            int pass18 = 0;
            for (const auto& c : cases) {
                lax.TapKey(0x42, 400, 2500); // AC
                for (const auto& kt : c.keys) {
                    lax.TapKey(kt.nxCode, 400, 1500);
                }
                lax.TapKey(0x60, 400, 2500); // =
                std::vector<std::uint8_t> rgba;
                lax.RenderRgba(rgba, nullptr, true);
                const auto mono = U13Matrix::ExtractMonoFromRgba(rgba, lax.ScreenWidth(), lax.ScreenHeight());
                const auto lines = U13Matrix::DecodeLcdLines(mono, lax.ScreenWidth(), lax.ScreenHeight());
                const std::string got = lines.empty() ? std::string() : lines.back();
                const std::string exp = U13Matrix::ExpectedForLax(c.expectedDot);
                const bool match = (got == exp);
                if (match) ++pass18;
                Check(match, std::string("U-13 LAX [") + c.id + "] " + c.expr,
                      "exp='" + exp + "' got='" + got + "'");
            }
            Check(pass18 == 18, "U-13 Blanca fx-570LA X 18/18 ROM LCD OCR",
                  std::to_string(pass18) + "/18");
        }
    }

    // 2) Gris ES PLUS (fx-570VN PLUS) in LineIO mode (dot decimal)
    {
        UnifiedCalcEngine vn;
        std::string err;
        const bool ok = vn.Initialize(CalcModelId::GrisEsPlus, paths, &err);
        Check(ok, "U-13 Gris initialize", err);
        if (ok) {
            // LineIO setup: SHIFT (0x07), MODE (0x47), 2 (0x10), settle 2000ms
            vn.TapKey(0x07, 400, 1500);
            vn.TapKey(0x47, 400, 1500);
            vn.TapKey(0x10, 400, 2000);

            int pass18 = 0;
            for (const auto& c : cases) {
                vn.TapKey(0x42, 400, 2500); // AC
                for (const auto& kt : c.keys) {
                    vn.TapKey(kt.nxCode, 400, 1500);
                }
                vn.TapKey(0x60, 400, 2500); // =
                std::vector<std::uint8_t> rgba;
                vn.RenderRgba(rgba, nullptr, true);
                const auto mono = U13Matrix::ExtractMonoFromRgba(rgba, vn.ScreenWidth(), vn.ScreenHeight());
                const auto lines = U13Matrix::DecodeLcdLines(mono, vn.ScreenWidth(), vn.ScreenHeight());
                const std::string got = lines.empty() ? std::string() : lines.back();
                const std::string exp = c.expectedDot;
                const bool match = (got == exp);
                if (match) ++pass18;
                Check(match, std::string("U-13 VN+ [") + c.id + "] " + c.expr,
                      "exp='" + exp + "' got='" + got + "'");
            }
            Check(pass18 == 18, "U-13 Gris fx-570VN PLUS 18/18 ROM LCD OCR",
                  std::to_string(pass18) + "/18");
        }
    }

    // 3) Negra fx-991CW (Cw2FallbackEngine) + EvaluateMathUtf8 18/18 differential check
    {
        UnifiedCalcEngine cw2;
        std::string err;
        const bool ok = cw2.Initialize(CalcModelId::NegraCw2, paths, &err);
        Check(ok, "U-13 CW2 initialize", err);
        const int kAc = FindKeyId(CalcModelId::NegraCw2, {"AC"});
        const int kExe = FindKeyId(CalcModelId::NegraCw2, {"EXE"});
        int cw2Pass = 0;
        int evalPass = 0;
        for (const auto& c : cases) {
            cw2.TapKeyById(kAc, 10, 10);
            for (const auto& kt : c.keys) {
                const int kid = FindKeyId(CalcModelId::NegraCw2, {kt.cw2Key});
                if (kid >= 0) cw2.TapKeyById(kid, 10, 10);
            }
            cw2.TapKeyById(kExe, 10, 10);
            const std::string gotCw2 = cw2.GetFallbackResult();
            const bool mCw2 = (gotCw2 == c.expectedDot);
            if (mCw2) ++cw2Pass;
            Check(mCw2, std::string("U-13 CW2 [") + c.id + "] " + c.expr,
                  "exp='" + std::string(c.expectedDot) + "' got='" + gotCw2 + "'");

            const auto ev = EvaluateMathUtf8(c.expr, true, 0.0);
            const bool mEv = (ev.ok && !ev.isError && ev.formatted == c.expectedDot);
            if (mEv) ++evalPass;
            Check(mEv, std::string("U-13 MathEval [") + c.id + "] " + c.expr,
                  "exp='" + std::string(c.expectedDot) + "' got='" + ev.formatted + "'");
        }
        Check(cw2Pass == 18 && evalPass == 18, "U-13 CW2 & MathEval 18/18 differential match");
    }

    // 4) Optional fx-CG50 PRIZM Run-Matrix integer check (1+2=3, 9*8=72, 100/4=25)
    if (!paths.cg50RomsDir.empty()) {
        UnifiedCalcEngine cg;
        std::string err;
        if (cg.Initialize(CalcModelId::Cg50Prizm, paths, &err)) {
            const int kMenu = FindKeyId(CalcModelId::Cg50Prizm, {"MENU"});
            const int k1 = FindKeyId(CalcModelId::Cg50Prizm, {"1"});
            const int k2 = FindKeyId(CalcModelId::Cg50Prizm, {"2"});
            const int kPlus = FindKeyId(CalcModelId::Cg50Prizm, {"MAS"});
            const int kExe = FindKeyId(CalcModelId::Cg50Prizm, {"EXE"});
            std::vector<std::uint8_t> frame;
            for (int step = 0; step < 60; ++step) {
                cg.StepMs(100);
                if (cg.RenderRgba(frame, nullptr, true) && CountUniqueRgb(frame) >= 3) break;
            }
            if (kMenu >= 0) cg.TapKeyById(kMenu, 100, 500);
            if (k1 >= 0) cg.TapKeyById(k1, 100, 500); // Run-Matrix
            std::vector<std::uint8_t> beforeRun;
            cg.RenderRgba(beforeRun, nullptr, true);
            if (k1 >= 0 && kPlus >= 0 && k2 >= 0 && kExe >= 0) {
                cg.TapKeyById(k1, 80, 200);
                cg.TapKeyById(kPlus, 80, 200);
                cg.TapKeyById(k2, 80, 200);
                cg.TapKeyById(kExe, 80, 400);
            }
            std::vector<std::uint8_t> afterRun;
            cg.RenderRgba(afterRun, nullptr, true);
            Check(cg.IsReady() && afterRun.size() == 384u * 216u * 4u && DiffPixels(beforeRun, afterRun) > 0,
                  "U-13 CG50 Run-Matrix key sequence updates 384x216 framebuffer");
        }
    }
}

void TestMathEvaluatorAndFfiLinkage() {
    std::cout << "\n=== 7. EvaluateMathUtf8 & casio_ffi linkage ===\n";
    auto r1 = EvaluateMathUtf8("2+3*4", true, 0.0);
    Check(r1.ok && !r1.isError && r1.formatted == "14", "Eval 2+3*4 == 14", r1.formatted);

    auto r2 = EvaluateMathUtf8("sin(30)", true, 0.0);
    Check(r2.ok && !r2.isError && r2.formatted == "0.5", "Eval sin(30 deg) == 0.5", r2.formatted);

    auto r3 = EvaluateMathUtf8("sqrt(16)", true, 0.0);
    Check(r3.ok && !r3.isError && r3.formatted == "4", "Eval sqrt(16) == 4", r3.formatted);

    auto r4 = EvaluateMathUtf8("3\xC3\x97" "4\xE2\x88\x92" "2\xC3\xB7" "2", true, 0.0);
    Check(r4.ok && !r4.isError && r4.formatted == "11",
          "Eval UTF-8 3×4−2÷2 == 11", r4.formatted);

    auto r5 = EvaluateMathUtf8("1/0", true, 0.0);
    Check(!r5.ok && r5.isError && r5.errorMessage.find("Math") != std::string::npos,
          "Eval 1/0 returns Math ERROR", r5.errorMessage);

    auto r6 = EvaluateMathUtf8("(2+", true, 0.0);
    Check(!r6.ok && r6.isError && r6.errorMessage.find("Syntax") != std::string::npos,
          "Eval (2+ returns Syntax ERROR", r6.errorMessage);

    // Smoke-check casio_ffi C symbols link cleanly in CasioLinuxEngines
    void* p = casio_alloc(64);
    Check(p != nullptr, "casio_ffi casio_alloc/casio_free link cleanly");
    if (p) casio_free(p);
}

void TestIteration2Regressions() {
    std::cout << "\n=== 9. Iteration 2 Engine & Fallback Regressions ===\n";

    // 1. Truncated/corrupt non-empty nX-U8 ROMs must return Initialize() == false cleanly (no exit(1)/PANIC)
    {
        std::error_code ec;
        const fs::path tmpDir = fs::temp_directory_path(ec) /
            ("test_linux_engines_trunc_" + std::to_string(static_cast<long>(::getpid())));
        fs::remove_all(tmpDir, ec);
        fs::create_directories(tmpDir, ec);

        auto writeTrunc = [&](const char* fname) {
            std::ofstream ofs(tmpDir / fname, std::ios::binary | std::ios::trunc);
            const char junk[15] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
            ofs.write(junk, sizeof(junk));
        };
        writeTrunc("fx991lax.rom");
        writeTrunc("fx570vnplus.rom");

        AssetPaths badPaths;
        badPaths.romsDir = tmpDir.string();

        UnifiedCalcEngine eBlanca;
        std::string errB;
        const bool okB = eBlanca.Initialize(CalcModelId::BlancaClassWiz, badPaths, &errB);
        Check(!okB && !errB.empty(),
              "Truncated 15-byte fx991lax.rom fails gracefully without PANIC", errB);

        UnifiedCalcEngine eGris;
        std::string errG;
        const bool okG = eGris.Initialize(CalcModelId::GrisEsPlus, badPaths, &errG);
        Check(!okG && !errG.empty(),
              "Truncated 15-byte fx570vnplus.rom fails gracefully without PANIC", errG);

        fs::remove_all(tmpDir, ec);
    }

    // 2. Cw2FallbackEngine parser, quadratic sign, tableScroll_ ImportState, and FormatComplex(0) RAD unit
    {
        UnifiedCalcEngine cw;
        std::string err;
        Check(cw.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err),
              "CW2 Initialize for Iter2 regressions", err);

        const std::string qNegA = cw.SolveFallbackQuadratic(-1.0, 0.0, -1.0);
        Check(qNegA == "x1=0+1i, x2=0-1i" &&
              qNegA.find("+-") == std::string::npos &&
              qNegA.find("--") == std::string::npos,
              "CW2 SolveQuadratic(-1, 0, -1) formats clean complex signs", qNegA);

        const std::string unclosed = cw.EvaluateFallbackComplex("(1+2i");
        Check(unclosed == "Syntax ERROR",
              "CW2 ComplexMathParser rejects unclosed '(' at EOF", unclosed);

        const std::string implMul = cw.EvaluateFallbackComplex("2abs(3+4i)");
        Check(implMul == "10",
              "CW2 ComplexMathParser supports implicit mul before function: 2abs(3+4i)==10", implMul);

        // Table ImportState preserves tableScroll_ and UP indicator bit 11
        cw.ComputeFallbackTable("x^2", 1.0, 5.0, 1.0);
        const int kDown = FindKeyId(CalcModelId::NegraCw2, {"ABAJO"});
        cw.TapKeyById(kDown, 10, 10);
        const std::uint32_t preMask = cw.GetFallbackIndicatorsMask();
        const std::vector<std::uint8_t> blob = cw.ExportState();
        Check((preMask & (1u << 11)) != 0u && !blob.empty(),
              "CW2 Table scrolled DOWN sets UP indicator bit 11 before ExportState");

        UnifiedCalcEngine cwRestored;
        cwRestored.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err);
        Check(cwRestored.ImportState(blob), "CW2 ImportState restores Table snapshot");
        const std::uint32_t postMask = cwRestored.GetFallbackIndicatorsMask();
        Check((postMask & (1u << 11)) != 0u,
              "CW2 ImportState preserves Table scroll & UP indicator bit 11", HexStr(postMask));

        // FormatComplex(0) in RAD polar mode must not emit degree symbol '°'
        UnifiedCalcEngine cwRad;
        cwRad.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err);
        const int kSettings = FindKeyId(CalcModelId::NegraCw2, {"SETTINGS"});
        const int k2 = FindKeyId(CalcModelId::NegraCw2, {"2"});
        const int kFmt = FindKeyId(CalcModelId::NegraCw2, {"FORMAT"});
        cwRad.TapKeyById(kSettings, 10, 10); // Settings
        cwRad.TapKeyById(k2, 10, 10);        // 2: Angle Rad
        const std::uint32_t radMask = cwRad.GetFallbackIndicatorsMask();
        cwRad.EvaluateFallbackComplex("0+0i");
        cwRad.TapKeyById(kFmt, 10, 10);      // Toggle Polar
        const std::string zeroRadPolar = cwRad.GetFallbackResult();
        Check((radMask & (1u << 6)) != 0u &&
              zeroRadPolar == "0\xE2\x88\xA0" "0" &&
              zeroRadPolar.find("\xC2\xB0") == std::string::npos,
              "CW2 FormatComplex(0) in RAD polar mode omits degree symbol", zeroRadPolar);

        // Iteration 3 (Challenger 2) regressions: trailing implicit multiplication after parenthesized function calls
        // and principal branch square root for negative real numbers (-0.0 imag canonicalization)
        const std::string cplxFnMul1 = cw.EvaluateFallbackComplex("abs(3+4i)conj(1-i)");
        Check(cplxFnMul1 == "5+5i",
              "CW2 trailing implicit mul after function: abs(3+4i)conj(1-i)==5+5i", cplxFnMul1);

        const std::string cplxFnMul2 = cw.EvaluateFallbackComplex("sqrt(4)i");
        Check(cplxFnMul2 == "2i",
              "CW2 trailing implicit mul with i after function: sqrt(4)i==2i", cplxFnMul2);

        const std::string cplxFnMul3 = cw.EvaluateFallbackComplex("abs(3+4i)(1+i)");
        Check(cplxFnMul3 == "5+5i",
              "CW2 trailing implicit mul with paren after function: abs(3+4i)(1+i)==5+5i", cplxFnMul3);

        const std::string cplxSqrtNeg1 = cw.EvaluateFallbackComplex("sqrt(-1)");
        Check(cplxSqrtNeg1 == "1i",
              "CW2 principal branch sqrt(-1)==1i", cplxSqrtNeg1);

        const std::string cplxSqrtNeg4 = cw.EvaluateFallbackComplex("2sqrt(-4)");
        Check(cplxSqrtNeg4 == "4i",
              "CW2 principal branch 2sqrt(-4)==4i", cplxSqrtNeg4);
    }
}

void TestIteration4Regressions() {
    std::cout << "\n=== 10. Iteration 4 Cw2FallbackEngine Regressions ===\n";
    UnifiedCalcEngine cw;
    std::string err;
    Check(cw.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err),
          "CW2 Initialize for Iter4 regressions", err);

    // Bug 1: ComplexMathParser '^' vs implicit multiplication precedence
    {
        Check(cw.EvaluateFallbackComplex("2i^2") == "-2",
              "R4-B1 2i^2 == -2 (not -4)", cw.GetFallbackResult());
        Check(cw.EvaluateFallbackComplex("2sqrt(4)^2") == "8",
              "R4-B1 2sqrt(4)^2 == 8 (not 16)", cw.GetFallbackResult());
        Check(cw.EvaluateFallbackComplex("2(1+i)^2") == "4i",
              "R4-B1 2(1+i)^2 == 4i (not 8i)", cw.GetFallbackResult());
        Check(cw.EvaluateFallbackComplex("(2i)^2") == "-4",
              "R4-B1 (2i)^2 == -4 preserved", cw.GetFallbackResult());

        const auto calcPi = EvaluateMathUtf8("2pi^2", true, 0.0);
        const std::string cplxPi = cw.EvaluateFallbackComplex("2pi^2");
        Check(calcPi.ok && cplxPi == calcPi.formatted,
              "R4-B1 Complex 2pi^2 matches Calc 2pi^2", "cplx=" + cplxPi + " calc=" + calcPi.formatted);

        const auto calcE = EvaluateMathUtf8("2e^2", true, 0.0);
        const std::string cplxE = cw.EvaluateFallbackComplex("2e^2");
        Check(calcE.ok && cplxE == calcE.formatted,
              "R4-B1 Complex 2e^2 matches Calc 2e^2", "cplx=" + cplxE + " calc=" + calcE.formatted);

        Check(cw.EvaluateFallbackComplex("abs(3+4i)conj(1-i)") == "5+5i",
              "R4-B1 preserves abs(3+4i)conj(1-i)==5+5i", cw.GetFallbackResult());
        Check(cw.EvaluateFallbackComplex("sqrt(4)i") == "2i",
              "R4-B1 preserves sqrt(4)i==2i", cw.GetFallbackResult());
        Check(cw.EvaluateFallbackComplex("abs(3+4i)(1+i)") == "5+5i",
              "R4-B1 preserves abs(3+4i)(1+i)==5+5i", cw.GetFallbackResult());
    }

    // Bug 2: ComplexMathParser trig AngleMode & asin/acos/atan
    {
        const std::string cSin30 = cw.EvaluateFallbackComplex("sin(30)");
        Check(cSin30 == "0.5", "R4-B2 Complex sin(30) in DEG == 0.5", cSin30);
        const std::string cCos60 = cw.EvaluateFallbackComplex("cos(60)");
        Check(cCos60 == "0.5", "R4-B2 Complex cos(60) in DEG == 0.5", cCos60);
        const std::string cTan45 = cw.EvaluateFallbackComplex("tan(45)");
        Check(cTan45 == "1", "R4-B2 Complex tan(45) in DEG == 1", cTan45);
        const std::string cTan90 = cw.EvaluateFallbackComplex("tan(90)");
        Check(cTan90 == "Math ERROR", "R4-B2 Complex tan(90) in DEG == Math ERROR", cTan90);
        const std::string cAsin05 = cw.EvaluateFallbackComplex("asin(0.5)");
        Check(cAsin05 == "30", "R4-B2 Complex asin(0.5) in DEG == 30", cAsin05);
        const std::string cAcos05 = cw.EvaluateFallbackComplex("acos(0.5)");
        Check(cAcos05 == "60", "R4-B2 Complex acos(0.5) in DEG == 60", cAcos05);
        const std::string cAtan1 = cw.EvaluateFallbackComplex("atan(1)");
        Check(cAtan1 == "45", "R4-B2 Complex atan(1) in DEG == 45", cAtan1);
        const std::string asin2 = cw.EvaluateFallbackComplex("asin(2)");
        Check(asin2 != "Syntax ERROR" && asin2.find('i') != std::string::npos,
              "R4-B2 Complex asin(2) evaluates complex inverse trig", asin2);

        // Check GRA mode in ComplexMathParser
        UnifiedCalcEngine cwGra;
        cwGra.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err);
        const int kSettings = FindKeyId(CalcModelId::NegraCw2, {"SETTINGS"});
        const int k3 = FindKeyId(CalcModelId::NegraCw2, {"3"});
        cwGra.TapKeyById(kSettings, 10, 10);
        cwGra.TapKeyById(k3, 10, 10); // 3: Angle Gra
        const std::string gSin100 = cwGra.EvaluateFallbackComplex("sin(100)");
        const std::string gAsin1 = cwGra.EvaluateFallbackComplex("asin(1)");
        Check(gSin100 == "1" && gAsin1 == "100",
              "R4-B2 Complex honors GRA angle mode", gSin100 + " / " + gAsin1);
    }

    // Bug 3: FormatForDisplay -0.0 snapping in Fixed2 and Scientific
    {
        UnifiedCalcEngine cwFmt;
        cwFmt.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err);
        const int kFmt = FindKeyId(CalcModelId::NegraCw2, {"FORMAT"});
        const int kNeg = FindKeyId(CalcModelId::NegraCw2, {"MENOS", "-"});
        const int k0 = FindKeyId(CalcModelId::NegraCw2, {"0"});
        const int kExe = FindKeyId(CalcModelId::NegraCw2, {"EXE", "="});
        cwFmt.TapKeyById(kNeg, 10, 10);
        cwFmt.TapKeyById(k0, 10, 10);
        cwFmt.TapKeyById(kExe, 10, 10);
        cwFmt.TapKeyById(kFmt, 10, 10); // Scientific
        const std::string sciNeg0 = cwFmt.GetFallbackResult();
        cwFmt.TapKeyById(kFmt, 10, 10); // Fixed2
        const std::string fixNeg0 = cwFmt.GetFallbackResult();
        Check(!sciNeg0.empty() && sciNeg0.front() != '-' && sciNeg0.rfind("0.000000e", 0) == 0,
              "R4-B3 Scientific(-0) omits leading minus", sciNeg0);
        Check(fixNeg0 == "0.00",
              "R4-B3 Fixed2(-0) == '0.00'", fixNeg0);
    }

    // Bug 4: ImportState cross-mode substate reset (Table ViewRows -> Equation ShowResult)
    {
        UnifiedCalcEngine engA;
        engA.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err);
        const std::string eqRes = engA.SolveFallbackSimultaneous(2, {2.0, 1.0, 7.0, 1.0, -1.0, 2.0});
        const auto snap = engA.ExportState();
        std::vector<std::uint8_t> rgbaA;
        engA.RenderRgba(rgbaA, nullptr, true);

        UnifiedCalcEngine engB;
        engB.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err);
        engB.ComputeFallbackTable("x^2", 1.0, 5.0, 1.0);
        const bool impOk = engB.ImportState(snap);
        std::vector<std::uint8_t> rgbaB;
        engB.RenderRgba(rgbaB, nullptr, true);
        Check(impOk && eqRes == "x=3, y=1" && engB.GetFallbackResult() == "x=3, y=1" &&
              DiffPixels(rgbaA, rgbaB) == 0,
              "R4-B4 ImportState from Table ViewRows to Equation ShowResult matches pixels",
              "resB='" + engB.GetFallbackResult() + "' diffPx=" + std::to_string(DiffPixels(rgbaA, rgbaB)));
    }

    // Bug 5: TableSubState::EditExpr live LCD rendering before EXE
    {
        UnifiedCalcEngine cwTbl;
        cwTbl.Initialize(CalcModelId::NegraCw2, AssetPaths{}, &err);
        const int kHome = FindKeyId(CalcModelId::NegraCw2, {"HOME"});
        const int k4 = FindKeyId(CalcModelId::NegraCw2, {"4"});
        const int kPlus = FindKeyId(CalcModelId::NegraCw2, {"MAS", "+"});
        const int k1 = FindKeyId(CalcModelId::NegraCw2, {"1"});
        const int kBack = FindKeyId(CalcModelId::NegraCw2, {"ATRAS"});

        cwTbl.TapKeyById(kHome, 10, 10);
        cwTbl.TapKeyById(k4, 10, 10); // Table EditExpr (x^2)
        std::vector<std::uint8_t> beforeEdit;
        cwTbl.RenderRgba(beforeEdit, nullptr, true);

        cwTbl.TapKeyById(kPlus, 10, 10);
        cwTbl.TapKeyById(k1, 10, 10);
        std::vector<std::uint8_t> afterEdit;
        cwTbl.RenderRgba(afterEdit, nullptr, true);
        const std::size_t editDiff = DiffPixels(beforeEdit, afterEdit);
        Check(editDiff > 0 && cwTbl.GetFallbackExpression() == "x^2+1",
              "R4-B5 Table EditExpr typing +1 updates LCD before EXE",
              "expr='" + cwTbl.GetFallbackExpression() + "' diffPx=" + std::to_string(editDiff));

        // Returning from ViewRows with ATRAS also clears hasEvaluated_ for live editing
        cwTbl.ComputeFallbackTable("x^2", 1.0, 3.0, 1.0);
        cwTbl.TapKeyById(kBack, 10, 10);
        std::vector<std::uint8_t> beforeBackEdit;
        cwTbl.RenderRgba(beforeBackEdit, nullptr, true);
        cwTbl.TapKeyById(kPlus, 10, 10);
        cwTbl.TapKeyById(k1, 10, 10);
        std::vector<std::uint8_t> afterBackEdit;
        cwTbl.RenderRgba(afterBackEdit, nullptr, true);
        Check(DiffPixels(beforeBackEdit, afterBackEdit) > 0 && cwTbl.GetFallbackExpression() == "x^2+1",
              "R4-B5 Table ATRAS from ViewRows allows appending +1 live",
              cwTbl.GetFallbackExpression());
    }

    // Bug 6: SolveQuadratic / SolveSimultaneous NaN & Inf guards
    {
        const double qnan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        const std::string q1 = cw.SolveFallbackQuadratic(qnan, 1.0, 1.0);
        Check(q1 == "Math ERROR", "R4-B6 SolveQuadratic(NaN,1,1) == Math ERROR", q1);
        const std::string q2 = cw.SolveFallbackQuadratic(1.0, inf, 1.0);
        Check(q2 == "Math ERROR", "R4-B6 SolveQuadratic(1,Inf,1) == Math ERROR", q2);
        const std::string s1 = cw.SolveFallbackSimultaneous(2, {qnan, 1.0, 2.0, 1.0, -1.0, 2.0});
        Check(s1 == "Math ERROR", "R4-B6 SolveSimultaneous(NaN) == Math ERROR", s1);
        const std::string s2 = cw.SolveFallbackSimultaneous(2, {1.0, inf, 2.0, 1.0, -1.0, 2.0});
        Check(s2 == "Math ERROR", "R4-B6 SolveSimultaneous(Inf) == Math ERROR", s2);
    }
}

} // namespace

int main(int argc, char** argv) {
    AssetPaths paths;
    TestAssetDiscovery(argc > 0 ? argv[0] : nullptr, paths);
    TestMetadataAndKeys();
    TestBlancaClassWiz(paths);
    TestGrisEsPlus(paths);
    TestCg50Prizm(paths);
    TestNegraCw2(paths);
    TestMathEvaluatorAndFfiLinkage();
    TestU13EighteenCaseRomAndDifferentialMatrix(paths);
    TestIteration2Regressions();
    TestIteration4Regressions();

    std::cout << "\n=== Summary: passed=" << g_passed << " failed=" << g_failed << " ===\n";
    return (g_failed == 0 && g_passed > 0) ? 0 : 1;
}
