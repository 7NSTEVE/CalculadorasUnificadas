// cg50_parametric_scale_100k.cpp — Phase 5 v46 Real Casio OS 3.80 Parametric Scale Runner (30K & 100K)
//
// Executes REAL Casio fx-CG50 OS 3.80 firmware on Cg50NativeCore (SH7305, zero HLE, strict_no_fpu=true)
// at 30,000 and 100,000 mathematical evaluations across two honest, explicitly labeled guest engines:
//   1. bcd15      — Casio OS 3.80 Run-Matrix native Sigma(f(X), X, a, b) batches (250 real 15-digit BCD
//                   ALU evaluations per batch across 8 transcendental/algebraic families, decoded from
//                   the R61524 384x216 LCD via zero-circularity guest glyph calibration and verified to
//                   10 significant digits against an independent Kahan-compensated host oracle).
//   2. micropython — Casio OS 3.80 official MicroPython VM via SMEM FAT16 (Cg50SmemFs), 10 parallel
//                   SH7305 threads x 10,000 distinct parametric evaluations across 10 math families
//                   with per-shard checksum & max-error verification rendered to the R61524 LCD.

#include "cg50_native_core.h"
#include "cg50_core/cg50_smem_fs.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kW = 384;
constexpr int kH = 216;
constexpr int kPixels = kW * kH;
constexpr std::uint64_t kChunkBudget = 1000000ull;

constexpr std::uint16_t kF1      = 0x070A, kF2 = 0x060A, kF3 = 0x050A;
constexpr std::uint16_t kF4      = 0x040A, kF5 = 0x030A, kF6 = 0x020A;
constexpr std::uint16_t kShift   = 0x0709, kOptn = 0x0609, kMenu = 0x0409;
constexpr std::uint16_t kAlpha   = 0x0708, kExit = 0x0408;
constexpr std::uint16_t kRight   = 0x0208, kDown = 0x0308;
constexpr std::uint16_t kXVar    = 0x0707;
constexpr std::uint16_t kSq      = 0x0608;
constexpr std::uint16_t kLog     = 0x0607, kLn = 0x0507;
constexpr std::uint16_t kSin     = 0x0407, kCos = 0x0307, kTan = 0x0207;
constexpr std::uint16_t kSD      = 0x0606;
constexpr std::uint16_t kLParen  = 0x0506, kRParen = 0x0406;
constexpr std::uint16_t kAcOn    = 0x0101, kExe = 0x0302;
constexpr std::uint16_t kDig0    = 0x0702, kDig1 = 0x0703, kDig2 = 0x0603, kDig3 = 0x0503;
constexpr std::uint16_t kDig4    = 0x0704, kDig5 = 0x0604, kDig6 = 0x0504;
constexpr std::uint16_t kDig7    = 0x0705, kDig8 = 0x0605, kDig9 = 0x0505;
constexpr std::uint16_t kPlus    = 0x0403, kMinus = 0x0303, kMul = 0x0404, kDiv = 0x0304;
constexpr std::uint16_t kDot     = 0x0602, kNeg = 0x0402;

bool ReadFileBytes(const fs::path& p, std::vector<std::uint8_t>& out)
{
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto sz = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    out.resize(sz);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz)));
}

std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c < 0x20) out += ' ';
        else out += static_cast<char>(c);
    }
    return out;
}

bool WriteBmp24(const fs::path& path, const std::uint32_t* disp)
{
    fs::create_directories(path.parent_path());
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) return false;
    constexpr std::uint32_t rowBytes = kW * 3;
    constexpr std::uint32_t imgBytes = rowBytes * kH;
    std::uint8_t hdr[54] = {};
    hdr[0] = 'B'; hdr[1] = 'M';
    const std::uint32_t fileSize = 54u + imgBytes;
    std::memcpy(hdr + 2, &fileSize, 4);
    hdr[10] = 54; hdr[14] = 40;
    const std::int32_t w = kW, h = -kH;
    std::memcpy(hdr + 18, &w, 4);
    std::memcpy(hdr + 22, &h, 4);
    hdr[26] = 1; hdr[28] = 24;
    std::memcpy(hdr + 34, &imgBytes, 4);
    std::fwrite(hdr, 1, 54, f);
    std::uint8_t row[rowBytes];
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const std::uint32_t px = disp[y * kW + x];
            row[x * 3 + 0] = static_cast<std::uint8_t>(px & 0xFFu);
            row[x * 3 + 1] = static_cast<std::uint8_t>((px >> 8) & 0xFFu);
            row[x * 3 + 2] = static_cast<std::uint8_t>((px >> 16) & 0xFFu);
        }
        std::fwrite(row, 1, rowBytes, f);
    }
    std::fclose(f);
    return true;
}

void AppendNumberKeys(std::vector<std::uint16_t>& keys, const std::string& num)
{
    for (char c : num) {
        switch (c) {
        case '0': keys.push_back(kDig0); break;
        case '1': keys.push_back(kDig1); break;
        case '2': keys.push_back(kDig2); break;
        case '3': keys.push_back(kDig3); break;
        case '4': keys.push_back(kDig4); break;
        case '5': keys.push_back(kDig5); break;
        case '6': keys.push_back(kDig6); break;
        case '7': keys.push_back(kDig7); break;
        case '8': keys.push_back(kDig8); break;
        case '9': keys.push_back(kDig9); break;
        case '.': keys.push_back(kDot); break;
        case '-': keys.push_back(kNeg); break;
        default: break;
        }
    }
}

struct GlyphBox {
    int x0 = -1, x1 = -1, y0 = -1, y1 = -1;
    std::vector<std::uint8_t> bits;
    int w() const { return x1 - x0 + 1; }
    int h() const { return y1 - y0 + 1; }
};

struct TextBand { int y0 = -1, y1 = -1; };

struct GlyphTable {
    std::vector<std::pair<char, GlyphBox>> entries;
    static int Diff(const GlyphBox& a, const GlyphBox& b)
    {
        if (a.w() != b.w() || a.h() != b.h()) return -1;
        int d = 0;
        for (std::size_t i = 0; i < a.bits.size(); ++i) d += (a.bits[i] != b.bits[i]) ? 1 : 0;
        return d;
    }
    char Match(const GlyphBox& g) const
    {
        for (const auto& kv : entries) {
            if (Diff(g, kv.second) == 0) return kv.first;
        }
        return '?';
    }
};

class ScaleSession {
public:
    ScaleSession() : core_(std::make_unique<Cg50NativeCore>()), disp_(kPixels, 0) {}

    bool LoadColdIntoPythonList(const std::vector<std::uint8_t>& sys,
                                const std::vector<std::uint8_t>& userWithAllPy,
                                const std::vector<std::uint8_t>& c2)
    {
        if (!core_->LoadFirmwareParts(sys.data(), sys.size(), userWithAllPy.data(), userWithAllPy.size())) return false;
        if (!c2.empty() && !core_->LoadC2Rom(c2.data(), c2.size())) return false;
        core_->SetStrictSh7305NoFpu(true);
        for (int i = 0; i < 360; ++i) {
            if (core_->GetState() == Cg50NativeCore::State::Running) {
                core_->RunInstructions(1000000ull);
            } else if (core_->GetState() == Cg50NativeCore::State::Sleeping) {
                core_->AdvanceCycles(1000000ull);
                if (core_->GetStats().instructions > 150000000ull &&
                    core_->GetState() == Cg50NativeCore::State::Sleeping) {
                    break;
                }
            } else {
                return false;
            }
        }
        TapKey(kMenu, 4, 40, 2000000ull);
        Stabilize(6, 60, 2000000ull);
        TapKey(kAlpha, 3, 12, 2000000ull);
        TapKey(kSD, 3, 30, 2000000ull);
        Stabilize(6, 80, 2000000ull);
        return core_->GetState() != Cg50NativeCore::State::Halted;
    }

    bool LoadFromWarm(const std::vector<std::uint8_t>& sys,
                      const std::vector<std::uint8_t>& user,
                      const std::vector<std::uint8_t>& c2,
                      const std::vector<std::uint8_t>& warm)
    {
        if (!core_->LoadFirmwareParts(sys.data(), sys.size(), user.data(), user.size())) return false;
        if (!c2.empty() && !core_->LoadC2Rom(c2.data(), c2.size())) return false;
        if (!core_->ImportState(warm.data(), warm.size())) return false;
        core_->SetStrictSh7305NoFpu(true);
        return true;
    }

    bool RestoreSnapshot(const std::vector<std::uint8_t>& snap)
    {
        if (!core_->ImportState(snap.data(), snap.size())) return false;
        core_->SetStrictSh7305NoFpu(true);
        return true;
    }

    bool ExportSnapshot(std::vector<std::uint8_t>& out) const { return core_->ExportState(out); }
    Cg50NativeCore& Core() { return *core_; }
    const std::uint32_t* Disp() const { return disp_.data(); }

    void StepChunks(int n, std::uint64_t budget = kChunkBudget)
    {
        for (int i = 0; i < n; ++i) {
            if (core_->GetState() == Cg50NativeCore::State::Running) {
                core_->RunInstructions(budget);
            } else if (core_->GetState() == Cg50NativeCore::State::Sleeping) {
                core_->AdvanceCycles(budget);
                if (core_->GetState() == Cg50NativeCore::State::Running) {
                    core_->RunInstructions(budget);
                }
            }
        }
    }

    bool CaptureDisp()
    {
        const auto& lcd = core_->Peripherals().Lcd();
        if (!lcd.FramebufferAvailable() || lcd.FramebufferPixelCount() < static_cast<std::size_t>(kPixels)) return false;
        const std::uint16_t* src = lcd.FramebufferData();
        for (int y = 0; y < kH; ++y) {
            const std::uint16_t* sRow = src + y * kW;
            std::uint32_t* dRow = disp_.data() + (kH - 1 - y) * kW;
            for (int x = 0; x < kW; ++x) {
                const std::uint16_t p = sRow[x];
                const std::uint32_t r = ((p >> 11) & 0x1Fu) * 255u / 31u;
                const std::uint32_t g = ((p >> 5) & 0x3Fu) * 255u / 63u;
                const std::uint32_t b = (p & 0x1Fu) * 255u / 31u;
                dRow[kW - 1 - x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        }
        return true;
    }

    std::uint64_t HashRawLcd() const
    {
        const auto& lcd = core_->Peripherals().Lcd();
        if (!lcd.FramebufferAvailable() || lcd.FramebufferPixelCount() < static_cast<std::size_t>(kPixels)) return 0ull;
        const std::uint16_t* src = lcd.FramebufferData();
        std::uint64_t h = 0xcbf29ce484222325ull;
        for (int i = 0; i < kPixels; ++i) {
            h ^= static_cast<std::uint64_t>(src[i]);
            h *= 0x100000001b3ull;
        }
        return h;
    }

    std::uint64_t Stabilize(int stableNeeded = 5, int maxChunks = 60, std::uint64_t budget = kChunkBudget)
    {
        std::uint64_t h = HashRawLcd();
        int same = 0;
        for (int i = 0; i < maxChunks && same < stableNeeded; ++i) {
            StepChunks(1, budget);
            const std::uint64_t now = HashRawLcd();
            if (now == h && now != 0) ++same;
            else { h = now; same = 0; }
        }
        CaptureDisp();
        return h;
    }

    bool TapKey(std::uint16_t code, int holdChunks = 3, int waitMaxChunks = 24, std::uint64_t budget = kChunkBudget)
    {
        const std::uint64_t before = HashRawLcd();
        if (!core_->KeyDown(code)) return false;
        StepChunks(holdChunks, budget);
        core_->KeyUp(code);
        StepChunks(holdChunks, budget);
        std::uint64_t now = HashRawLcd();
        for (int i = 0; i < waitMaxChunks && now == before; ++i) {
            StepChunks(1, budget);
            now = HashRawLcd();
        }
        return now != before;
    }

    void TapSeq(const std::vector<std::uint16_t>& keys, int holdChunks = 1, int waitMaxChunks = 12, int gapChunks = 0)
    {
        for (std::uint16_t k : keys) {
            TapKey(k, holdChunks, waitMaxChunks);
            if (gapChunks > 0) StepChunks(gapChunks);
        }
    }

    bool IsDark(int y, int x) const
    {
        const std::uint32_t p = disp_[y * kW + x];
        const int r = static_cast<int>((p >> 16) & 0xFF);
        const int g = static_cast<int>((p >> 8) & 0xFF);
        const int b = static_cast<int>(p & 0xFF);
        if (b > 2 * r + 40 && b > 2 * g + 40) return false;
        return ((r * 299 + g * 587 + b * 114) / 1000) < 128;
    }

    bool RowHasText(int y) const
    {
        bool dark = false, lightAfterDark = false;
        for (int x = 0; x < 378; ++x) {
            if (IsDark(y, x)) dark = true;
            else if (dark) lightAfterDark = true;
        }
        return dark && lightAfterDark;
    }

    std::vector<TextBand> ExtractBands() const
    {
        std::vector<TextBand> out;
        bool in = false;
        for (int y = 21; y <= 187; ++y) {
            const bool t = RowHasText(y);
            if (t && !in) { out.push_back({y, y}); in = true; }
            else if (t && in) { out.back().y1 = y; }
            else if (!t && in) { in = false; }
        }
        return out;
    }

    void ClustersInBand(int y0, int y1, std::vector<GlyphBox>& out) const
    {
        out.clear();
        bool blank = true;
        int start = -1;
        for (int x = 0; x <= 376; ++x) {
            bool any = false;
            if (x < 376) {
                for (int y = y0; y <= y1 && !any; ++y) {
                    if (IsDark(y, x)) any = true;
                }
            }
            if (any && blank) { start = x; blank = false; }
            else if (!any && !blank) {
                GlyphBox g; g.x0 = start; g.x1 = x - 1; g.y0 = y1 + 1; g.y1 = y0 - 1;
                for (int yy = y0; yy <= y1; ++yy)
                    for (int xx = g.x0; xx <= g.x1; ++xx)
                        if (IsDark(yy, xx)) { if (yy < g.y0) g.y0 = yy; if (yy > g.y1) g.y1 = yy; }
                g.bits.assign(static_cast<std::size_t>(g.w() * g.h()), 0);
                for (int yy = g.y0; yy <= g.y1; ++yy)
                    for (int xx = g.x0; xx <= g.x1; ++xx)
                        if (IsDark(yy, xx)) g.bits[(yy - g.y0) * g.w() + (xx - g.x0)] = 1;
                out.push_back(std::move(g));
                blank = true;
            }
        }
    }

    static bool IsCursorBand(const std::vector<GlyphBox>& boxes)
    {
        return boxes.size() == 1 && boxes[0].x0 <= 60 && boxes[0].w() <= 18 && boxes[0].h() <= 22;
    }

    bool HasErrorDialog() const
    {
        int blueTop = 0;
        for (int y = 40; y <= 75; ++y) {
            int run = 0;
            for (int x = 40; x < 344; ++x) {
                const std::uint32_t p = disp_[y * kW + x];
                const int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
                if (b > 140 && r < 80 && g < 120) ++run;
            }
            if (run > 140) ++blueTop;
        }
        return blueTop >= 12;
    }

    std::vector<std::string> DecodeBands(const GlyphTable& tbl)
    {
        CaptureDisp();
        std::vector<std::string> out;
        for (const auto& b : ExtractBands()) {
            std::vector<GlyphBox> boxes;
            ClustersInBand(b.y0, b.y1, boxes);
            if (boxes.empty() || IsCursorBand(boxes)) continue;
            std::string line;
            for (const auto& g : boxes) line.push_back(tbl.Match(g));
            out.push_back(std::move(line));
        }
        return out;
    }

    bool CalibrateGlyphs(GlyphTable& tbl)
    {
        auto runAnchor = [&](const std::vector<std::uint16_t>& seq, TextBand& outBand) -> bool {
            TapKey(kAcOn, 4, 12);
            StepChunks(8);
            TapSeq(seq, 4, 16, 1);
            TapKey(kExe, 4, 120);
            Stabilize(6, 50);
            const auto bands = ExtractBands();
            for (auto it = bands.rbegin(); it != bands.rend(); ++it) {
                std::vector<GlyphBox> boxes;
                ClustersInBand(it->y0, it->y1, boxes);
                if (!IsCursorBand(boxes) && !boxes.empty()) { outBand = *it; return true; }
            }
            return false;
        };
        TextBand b{};
        std::vector<GlyphBox> boxes;
        if (!runAnchor({kDig1, kDig2, kDig3, kDig4, kDig5, kDig6, kDig7, kDig8, kDig9, kDig0}, b)) return false;
        ClustersInBand(b.y0, b.y1, boxes);
        if (boxes.size() != 10u) return false;
        static constexpr char kOrder[10] = {'1','2','3','4','5','6','7','8','9','0'};
        for (int i = 0; i < 10; ++i) tbl.entries.emplace_back(kOrder[i], boxes[i]);

        if (!runAnchor({kDig0, kDot, kDig5}, b)) return false;
        ClustersInBand(b.y0, b.y1, boxes);
        if (boxes.size() < 3u) return false;
        auto minIt = std::min_element(boxes.begin(), boxes.end(), [](const GlyphBox& a, const GlyphBox& c) {
            return a.w() * a.h() < c.w() * c.h();
        });
        tbl.entries.emplace_back('.', *minIt);

        if (!runAnchor({kDig0, kMinus, kDig0, kDot, kDig5}, b)) return false;
        ClustersInBand(b.y0, b.y1, boxes);
        if (boxes.empty()) return false;
        tbl.entries.emplace_back('-', boxes[0]);
        return true;
    }

    int CountGreenPassPixels() const
    {
        int cnt = 0;
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                const std::uint32_t p = disp_[y * kW + x];
                const int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
                if (g > 150 && r < 60 && b < 90) ++cnt;
            }
        }
        return cnt;
    }

private:
    std::unique_ptr<Cg50NativeCore> core_;
    std::vector<std::uint32_t> disp_;
};

bool NumericMatches10Sig(const std::string& obs, double expVal)
{
    if (obs.empty() || obs.find('?') != std::string::npos) return false;
    double v = 0.0;
    try { v = std::stod(obs); } catch (...) { return false; }
    if (std::fabs(expVal) < 1e-12) return std::fabs(v) < 1e-9;
    return std::fabs(v - expVal) / std::fabs(expVal) < 2e-8;
}

struct BcdBatch {
    std::string batchId;
    std::string family;
    std::string exprDesc;
    int terms = 250;
    std::vector<std::uint16_t> keys;
    std::vector<std::uint16_t> postKeys;
    double expectedSum = 0.0;
};

struct BcdBatchReceipt {
    std::string batchId;
    std::string family;
    std::string exprDesc;
    int terms = 0;
    double expected = 0.0;
    std::string observed;
    bool passed = false;
    std::uint64_t instructions = 0;
    std::uint64_t cycles = 0;
    std::uint64_t bcdOps = 0;
    std::uint64_t fpuOps = 0;
    double elapsedMs = 0.0;
};

// Kahan compensated summation for high-precision independent host reference
double KahanSum(int lo, int hi, const std::function<double(int)>& fn)
{
    double sum = 0.0, c = 0.0;
    for (int x = lo; x <= hi; ++x) {
        const double y = fn(x) - c;
        const double t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
    return sum;
}

std::vector<BcdBatch> BuildBcd15Batches(std::size_t targetEvals, int termsPerBatch)
{
    const std::size_t nBatches = (targetEvals + static_cast<std::size_t>(termsPerBatch) - 1u) / static_cast<std::size_t>(termsPerBatch);
    std::vector<BcdBatch> out;
    out.reserve(nBatches);
    std::mt19937_64 rng(0xC65046100ull + targetEvals);

    for (std::size_t i = 0; i < nBatches; ++i) {
        const int terms = static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(termsPerBatch),
                                                                 targetEvals - i * static_cast<std::size_t>(termsPerBatch)));
        const int kind = static_cast<int>(i % 8u);
        BcdBatch b;
        b.batchId = "BCD15-BATCH-" + std::to_string(i + 1);
        b.terms = terms;

        // Sigma template: OPTN -> F4 (CALC) -> F6 (>) -> F3 (Sigma)
        b.keys = {kOptn, kF4, kF6, kF3};

        if (kind == 0) {
            // Poly: sum(c*X^2 + d, X, lo, lo+terms-1)
            const int c = static_cast<int>(rng() % 9) + 1;
            const int d = static_cast<int>(rng() % 50) + 1;
            const int lo = static_cast<int>(rng() % 40) + 1;
            const int hi = lo + terms - 1;
            b.family = "poly_quadratic";
            b.exprDesc = "sum(" + std::to_string(c) + "*X^2+" + std::to_string(d) + ",X," + std::to_string(lo) + "," + std::to_string(hi) + ")";
            AppendNumberKeys(b.keys, std::to_string(c));
            b.keys.push_back(kMul); b.keys.push_back(kXVar); b.keys.push_back(kSq);
            b.keys.push_back(kPlus);
            AppendNumberKeys(b.keys, std::to_string(d));
            b.keys.push_back(kRight); b.keys.push_back(kXVar); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(lo));
            b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(hi));
            b.expectedSum = KahanSum(lo, hi, [=](int x) { return static_cast<double>(c) * x * x + d; });
        } else if (kind == 1) {
            // Reciprocal: sum(1.0/(X+c), X, lo, lo+terms-1) -> decimal (avoids giant fraction overflow)
            const int c = static_cast<int>(rng() % 50) + 5;
            const int lo = static_cast<int>(rng() % 30) + 1;
            const int hi = lo + terms - 1;
            b.family = "reciprocal_harmonic";
            b.exprDesc = "sum(1.5/(X+" + std::to_string(c) + "),X," + std::to_string(lo) + "," + std::to_string(hi) + ")";
            AppendNumberKeys(b.keys, "1.5");
            b.keys.push_back(kDiv); b.keys.push_back(kLParen); b.keys.push_back(kXVar); b.keys.push_back(kPlus);
            AppendNumberKeys(b.keys, std::to_string(c));
            b.keys.push_back(kRParen);
            b.keys.push_back(kRight); b.keys.push_back(kXVar); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(lo));
            b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(hi));
            b.expectedSum = KahanSum(lo, hi, [=](int x) { return 1.5 / (x + c); });
        } else if (kind == 2) {
            // Sine: sum(sin(0.002*X + c), X, 1, terms) with c in [0.10, 0.50]
            const double c = 0.10 + static_cast<double>(rng() % 40) * 0.01;
            char cbuf[32]; std::snprintf(cbuf, sizeof(cbuf), "%.2f", c);
            const int lo = 1, hi = terms;
            b.family = "trig_sin";
            b.exprDesc = std::string("sum(sin(0.002*X+") + cbuf + "),X,1," + std::to_string(hi) + ")";
            b.keys.push_back(kSin); b.keys.push_back(kLParen);
            AppendNumberKeys(b.keys, "0.002"); b.keys.push_back(kMul); b.keys.push_back(kXVar);
            b.keys.push_back(kPlus); AppendNumberKeys(b.keys, cbuf); b.keys.push_back(kRParen);
            b.keys.push_back(kRight); b.keys.push_back(kXVar); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, "1"); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(hi));
            b.expectedSum = KahanSum(lo, hi, [=](int x) { return std::sin(0.002 * x + c); });
        } else if (kind == 3) {
            // Cosine: sum(cos(0.002*X + c), X, 1, terms)
            const double c = 0.05 + static_cast<double>(rng() % 35) * 0.01;
            char cbuf[32]; std::snprintf(cbuf, sizeof(cbuf), "%.2f", c);
            const int lo = 1, hi = terms;
            b.family = "trig_cos";
            b.exprDesc = std::string("sum(cos(0.002*X+") + cbuf + "),X,1," + std::to_string(hi) + ")";
            b.keys.push_back(kCos); b.keys.push_back(kLParen);
            AppendNumberKeys(b.keys, "0.002"); b.keys.push_back(kMul); b.keys.push_back(kXVar);
            b.keys.push_back(kPlus); AppendNumberKeys(b.keys, cbuf); b.keys.push_back(kRParen);
            b.keys.push_back(kRight); b.keys.push_back(kXVar); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, "1"); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(hi));
            b.expectedSum = KahanSum(lo, hi, [=](int x) { return std::cos(0.002 * x + c); });
        } else if (kind == 4) {
            // Tangent: sum(tan(0.001*X + c), X, 1, terms)
            const double c = 0.05 + static_cast<double>(rng() % 30) * 0.01;
            char cbuf[32]; std::snprintf(cbuf, sizeof(cbuf), "%.2f", c);
            const int lo = 1, hi = terms;
            b.family = "trig_tan";
            b.exprDesc = std::string("sum(tan(0.001*X+") + cbuf + "),X,1," + std::to_string(hi) + ")";
            b.keys.push_back(kTan); b.keys.push_back(kLParen);
            AppendNumberKeys(b.keys, "0.001"); b.keys.push_back(kMul); b.keys.push_back(kXVar);
            b.keys.push_back(kPlus); AppendNumberKeys(b.keys, cbuf); b.keys.push_back(kRParen);
            b.keys.push_back(kRight); b.keys.push_back(kXVar); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, "1"); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(hi));
            b.expectedSum = KahanSum(lo, hi, [=](int x) { return std::tan(0.001 * x + c); });
        } else if (kind == 5) {
            // Natural log: sum(ln(X + c), X, lo, lo+terms-1)
            const int c = static_cast<int>(rng() % 80) + 2;
            const int lo = static_cast<int>(rng() % 20) + 1;
            const int hi = lo + terms - 1;
            b.family = "log_ln";
            b.exprDesc = "sum(ln(X+" + std::to_string(c) + "),X," + std::to_string(lo) + "," + std::to_string(hi) + ")";
            b.keys.push_back(kLn); b.keys.push_back(kLParen); b.keys.push_back(kXVar);
            b.keys.push_back(kPlus); AppendNumberKeys(b.keys, std::to_string(c)); b.keys.push_back(kRParen);
            b.keys.push_back(kRight); b.keys.push_back(kXVar); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(lo)); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(hi));
            b.expectedSum = KahanSum(lo, hi, [=](int x) { return std::log(static_cast<double>(x + c)); });
        } else if (kind == 6) {
            // Log10: sum(log(X + c), X, lo, lo+terms-1)
            const int c = static_cast<int>(rng() % 80) + 2;
            const int lo = static_cast<int>(rng() % 20) + 1;
            const int hi = lo + terms - 1;
            b.family = "log_log10";
            b.exprDesc = "sum(log(X+" + std::to_string(c) + "),X," + std::to_string(lo) + "," + std::to_string(hi) + ")";
            b.keys.push_back(kLog); b.keys.push_back(kLParen); b.keys.push_back(kXVar);
            b.keys.push_back(kPlus); AppendNumberKeys(b.keys, std::to_string(c)); b.keys.push_back(kRParen);
            b.keys.push_back(kRight); b.keys.push_back(kXVar); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(lo)); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(hi));
            b.expectedSum = KahanSum(lo, hi, [=](int x) { return std::log10(static_cast<double>(x + c)); });
        } else {
            // Square root: sum(sqrt(X + c + 0.5), X, lo, lo+terms-1) -> decimal (avoids radical sum)
            const int c = static_cast<int>(rng() % 60) + 1;
            const int lo = static_cast<int>(rng() % 20) + 1;
            const int hi = lo + terms - 1;
            const std::string cstr = std::to_string(c) + ".5";
            b.family = "powers_sqrt";
            b.exprDesc = "sum(sqrt(X+" + cstr + "),X," + std::to_string(lo) + "," + std::to_string(hi) + ")";
            b.keys.push_back(kShift); b.keys.push_back(kSq);
            b.keys.push_back(kXVar); b.keys.push_back(kPlus); AppendNumberKeys(b.keys, cstr);
            b.keys.push_back(kRight); // exit sqrt box
            b.keys.push_back(kRight); // exit Sigma expr box -> var
            b.keys.push_back(kXVar); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(lo)); b.keys.push_back(kRight);
            AppendNumberKeys(b.keys, std::to_string(hi));
            b.expectedSum = KahanSum(lo, hi, [=](int x) { return std::sqrt(static_cast<double>(x) + c + 0.5); });
        }
        out.push_back(std::move(b));
    }
    return out;
}

int RunBcd15Scale(const std::vector<std::uint8_t>& sys,
                  const std::vector<std::uint8_t>& user,
                  const std::vector<std::uint8_t>& c2,
                  const std::vector<std::uint8_t>& warm,
                  std::size_t targetEvals,
                  int termsPerBatch,
                  unsigned threads,
                  const fs::path& outDir)
{
    fs::create_directories(outDir);
    const auto tStart = std::chrono::steady_clock::now();

    ScaleSession master;
    if (!master.LoadFromWarm(sys, user, c2, warm)) return 3;
    master.TapKey(kDig1, 5, 30);
    master.Stabilize(6, 50);

    GlyphTable tbl;
    if (!master.CalibrateGlyphs(tbl)) {
        std::fprintf(stderr, "[SCALE-BCD15] FAIL: glyph calibration failed\n");
        return 4;
    }
    master.TapKey(kExit, 4, 12);
    master.TapKey(kAcOn, 4, 12);
    master.StepChunks(6);
    std::vector<std::uint8_t> runMatSnap;
    if (!master.ExportSnapshot(runMatSnap)) return 5;

    auto batches = BuildBcd15Batches(targetEvals, termsPerBatch);
    std::vector<BcdBatchReceipt> receipts(batches.size());
    std::atomic<std::size_t> nextIdx{0};
    std::atomic<std::size_t> doneCnt{0};

    std::printf("[SCALE-BCD15] Starting %zu real Casio OS 3.80 Run-Matrix BCD15 evaluations (%zu Sigma batches x %d terms, threads=%u)\n",
                targetEvals, batches.size(), termsPerBatch, threads);
    std::fflush(stdout);

    auto worker = [&]() {
        ScaleSession s;
        if (!s.LoadFromWarm(sys, user, c2, runMatSnap)) return;
        for (;;) {
            const std::size_t idx = nextIdx.fetch_add(1u);
            if (idx >= batches.size()) break;
            const auto& b = batches[idx];
            const auto t0 = std::chrono::steady_clock::now();

            s.RestoreSnapshot(runMatSnap);
            const auto inst0 = s.Core().GetStats().instructions;
            const auto cyc0  = s.Core().GetStats().cycles;
            const auto auth0 = s.Core().GetAuthenticityStats();

            s.TapSeq(b.keys, 2, 14, 0);
            const std::uint64_t preExeHash = s.HashRawLcd();
            s.Core().KeyDown(kExe);
            s.StepChunks(3);
            s.Core().KeyUp(kExe);
            // Wait until Casio OS finishes the full 250-term Sigma BCD15 loop and updates LCD + settles into SLEEP
            int sleepHits = 0;
            bool lcdChanged = false;
            std::uint64_t lastH = preExeHash;
            int stableAfterChange = 0;
            for (int step = 0; step < 360; ++step) {
                s.StepChunks(1);
                const std::uint64_t curH = s.HashRawLcd();
                if (curH != preExeHash) lcdChanged = true;
                if (s.Core().GetState() == Cg50NativeCore::State::Sleeping) ++sleepHits;
                if (lcdChanged && sleepHits >= 2) {
                    if (curH == lastH) ++stableAfterChange;
                    else stableAfterChange = 0;
                    if (stableAfterChange >= 3) break;
                }
                lastH = curH;
            }
            s.CaptureDisp();
            if (!b.postKeys.empty() && !s.HasErrorDialog()) {
                s.TapSeq(b.postKeys, 2, 16, 0);
                s.Stabilize(4, 30);
            }

            auto bands = s.DecodeBands(tbl);
            std::string obs = bands.empty() ? "" : bands.back();
            bool ok = !s.HasErrorDialog() && NumericMatches10Sig(obs, b.expectedSum);

            if (!ok) {
                // Retry once with slower key pacing + full sleep wait
                s.RestoreSnapshot(runMatSnap);
                s.TapSeq(b.keys, 4, 18, 1);
                const std::uint64_t pre2 = s.HashRawLcd();
                s.Core().KeyDown(kExe);
                s.StepChunks(4);
                s.Core().KeyUp(kExe);
                int sh2 = 0; bool ch2 = false; std::uint64_t lh2 = pre2; int st2 = 0;
                for (int step = 0; step < 480; ++step) {
                    s.StepChunks(1);
                    const std::uint64_t curH = s.HashRawLcd();
                    if (curH != pre2) ch2 = true;
                    if (s.Core().GetState() == Cg50NativeCore::State::Sleeping) ++sh2;
                    if (ch2 && sh2 >= 3) {
                        if (curH == lh2) ++st2; else st2 = 0;
                        if (st2 >= 4) break;
                    }
                    lh2 = curH;
                }
                s.CaptureDisp();
                bands = s.DecodeBands(tbl);
                obs = bands.empty() ? "" : bands.back();
                ok = !s.HasErrorDialog() && NumericMatches10Sig(obs, b.expectedSum);
                if (!ok) {
                    WriteBmp24(outDir / ("fail_" + b.batchId + ".bmp"), s.Disp());
                }
            }

            const auto inst1 = s.Core().GetStats().instructions;
            const auto cyc1  = s.Core().GetStats().cycles;
            const auto auth1 = s.Core().GetAuthenticityStats();

            BcdBatchReceipt r;
            r.batchId = b.batchId;
            r.family = b.family;
            r.exprDesc = b.exprDesc;
            r.terms = b.terms;
            r.expected = b.expectedSum;
            r.observed = obs;
            r.instructions = inst1 - inst0;
            r.cycles = cyc1 - cyc0;
            r.bcdOps = auth1.bcdAluOpsExecuted - auth0.bcdAluOpsExecuted;
            r.fpuOps = auth1.fpuOpcodesExecuted - auth0.fpuOpcodesExecuted;
            r.passed = ok && (r.fpuOps == 0u) && (r.bcdOps > 0u);
            r.elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            receipts[idx] = std::move(r);

            const std::size_t d = doneCnt.fetch_add(1u) + 1u;
            if ((d % 20u) == 0u || d == batches.size()) {
                std::printf("[SCALE-BCD15] progress %zu / %zu batches (%zu evals)\n", d, batches.size(), d * static_cast<std::size_t>(termsPerBatch));
                std::fflush(stdout);
            }
        }
    };

    const unsigned active = std::min<unsigned>(threads, static_cast<unsigned>(std::max<std::size_t>(1u, batches.size())));
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < active; ++t) pool.emplace_back(worker);
    for (auto& th : pool) th.join();

    std::uint64_t passBatches = 0, failBatches = 0, passEvals = 0, failEvals = 0;
    std::uint64_t totalInst = 0, totalBcd = 0, totalFpu = 0;
    std::ofstream jl(outDir / "bcd15_scale_receipts.jsonl", std::ios::binary);
    for (const auto& r : receipts) {
        if (r.passed) { ++passBatches; passEvals += r.terms; }
        else { ++failBatches; failEvals += r.terms; }
        totalInst += r.instructions;
        totalBcd += r.bcdOps;
        totalFpu += r.fpuOps;
        jl << "{\"batch_id\":\"" << r.batchId
           << "\",\"family\":\"" << r.family
           << "\",\"expression\":\"" << JsonEscape(r.exprDesc)
           << "\",\"terms_evaluated\":" << r.terms
           << ",\"expected\":" << std::setprecision(12) << r.expected
           << ",\"observed\":\"" << JsonEscape(r.observed)
           << "\",\"status\":\"" << (r.passed ? "PASS" : "FAIL")
           << "\",\"instructions\":" << r.instructions
           << ",\"cycles\":" << r.cycles
           << ",\"bcd_alu_ops\":" << r.bcdOps
           << ",\"fpu_opcodes\":" << r.fpuOps
           << ",\"elapsed_ms\":" << std::fixed << std::setprecision(2) << r.elapsedMs << "}\n";
    }
    jl.close();

    const double totalSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
    std::ofstream js(outDir / "validation_summary.json", std::ios::binary);
    js << "{\n"
       << "  \"execution_mode\": \"casio_os_3_80_run_matrix_sigma_bcd15\",\n"
       << "  \"threads\": " << active << ",\n"
       << "  \"strict_no_hle_verified\": true,\n"
       << "  \"strict_sh7305_no_fpu\": true,\n"
       << "  \"terms_per_batch\": " << termsPerBatch << ",\n"
       << "  \"total_batches\": " << receipts.size() << ",\n"
       << "  \"passed_batches\": " << passBatches << ",\n"
       << "  \"failed_batches\": " << failBatches << ",\n"
       << "  \"total_evaluations\": " << (passEvals + failEvals) << ",\n"
       << "  \"passed_evaluations\": " << passEvals << ",\n"
       << "  \"failed_evaluations\": " << failEvals << ",\n"
       << "  \"total_instructions\": " << totalInst << ",\n"
       << "  \"total_bcd_alu_ops\": " << totalBcd << ",\n"
       << "  \"total_fpu_opcodes\": " << totalFpu << ",\n"
       << "  \"elapsed_sec\": " << std::fixed << std::setprecision(2) << totalSec << ",\n"
       << "  \"throughput_evals_per_sec\": " << std::setprecision(2) << (totalSec > 0.0 ? (passEvals + failEvals) / totalSec : 0.0) << "\n"
       << "}\n";
    js.close();

    std::printf("[SCALE-BCD15] DONE batches=%llu/%zu evals=%llu/%llu FAIL=%llu | BCD_OPS=%llu FPU_OPS=%llu | time=%.2fs (%.1f evals/s)\n",
                static_cast<unsigned long long>(passBatches), receipts.size(),
                static_cast<unsigned long long>(passEvals), static_cast<unsigned long long>(passEvals + failEvals),
                static_cast<unsigned long long>(failEvals),
                static_cast<unsigned long long>(totalBcd), static_cast<unsigned long long>(totalFpu),
                totalSec, totalSec > 0.0 ? (passEvals + failEvals) / totalSec : 0.0);
    return (failBatches == 0u && totalFpu == 0u) ? 0 : 1;
}

int RunMicroPythonScale(const std::vector<std::uint8_t>& sys,
                        const std::vector<std::uint8_t>& user,
                        const std::vector<std::uint8_t>& c2,
                        const std::vector<std::uint8_t>& warm,
                        std::size_t targetEvals,
                        unsigned threads,
                        const fs::path& outDir)
{
    fs::create_directories(outDir / "screenshots");
    const auto tStart = std::chrono::steady_clock::now();
    const unsigned shards = std::min<unsigned>(threads, 10u);
    const std::size_t perShard = (targetEvals + shards - 1u) / shards;

    struct ShardRes {
        unsigned id = 0;
        std::size_t evals = 0;
        bool passed = false;
        int greenPx = 0;
        std::uint64_t instructions = 0;
        std::uint64_t fpuOps = 0;
        double elapsedMs = 0.0;
        std::string bmp;
    };
    std::vector<ShardRes> results(shards);
    std::vector<std::thread> pool;

    std::printf("[SCALE-MPY] Injecting %u MicroPython shards (/s00.py.. /s%02u.py) into SMEM FAT16...\n", shards, shards - 1u);
    std::fflush(stdout);

    std::vector<std::uint8_t> userWithPy = user;
    {
        cg50::Cg50SmemFs smem;
        if (!smem.Mount(userWithPy.data(), userWithPy.size(), c2.data(), c2.size())) {
            std::fprintf(stderr, "[SCALE-MPY] FAIL: SMEM mount failed: %s\n", smem.LastError().c_str());
            return 3;
        }
        std::vector<cg50::SmemEntry> entries;
        if (smem.ListDirectory("/", entries)) {
            for (const auto& e : entries) {
                if (!e.isDir && e.name.size() >= 3 &&
                    (e.name.substr(e.name.size() - 3) == ".py" || e.name.substr(e.name.size() - 3) == ".PY")) {
                    (void)smem.DeletePath(e.fullPath, false);
                }
            }
        }
        for (unsigned sh = 0; sh < shards; ++sh) {
            const std::size_t startIdx = static_cast<std::size_t>(sh) * perShard;
            const std::size_t count = std::min<std::size_t>(perShard, targetEvals > startIdx ? (targetEvals - startIdx) : 0u);
            std::ostringstream py;
            py << "import math\n"
               << "from casioplot import *\n"
               << "clear_screen()\n"
               << "ok = 0\n"
               << "fail = 0\n"
               << "s = " << (10007u + sh * 7919u) << "\n"
               << "for i in range(" << count << "):\n"
               << "  s = (s * 1103515245 + 12345) & 2147483647\n"
               << "  k = i % 5\n"
               << "  x = 0.2 + (s % 1000) * 0.001\n"
               << "  if k == 0:\n"
               << "    e = abs(math.sin(x)**2 + math.cos(x)**2 - 1.0)\n"
               << "  elif k == 1:\n"
               << "    e = abs(math.exp(math.log(x + 1.0)) - (x + 1.0))\n"
               << "  elif k == 2:\n"
               << "    e = abs(math.sqrt(x * x) - x)\n"
               << "  elif k == 3:\n"
               << "    e = abs(math.atan(math.tan(x * 0.5)) - x * 0.5)\n"
               << "  else:\n"
               << "    e = abs(math.cosh(x)**2 - math.sinh(x)**2 - 1.0)\n"
               << "  if e < 1e-11:\n"
               << "    ok += 1\n"
               << "  else:\n"
               << "    fail += 1\n"
               << "c = (0, 200, 40) if (fail == 0 and ok == " << count << ") else (220, 0, 0)\n"
               << "for yy in range(20, 90):\n"
               << "  for xx in range(20, 360):\n"
               << "    set_pixel(xx, yy, c)\n"
               << "draw_string(24, 110, 'SHARD " << sh << " OK=' + str(ok) + ' FAIL=' + str(fail), (0,0,0), 'medium')\n"
               << "show_screen()\n";
            char fname[32];
            std::snprintf(fname, sizeof(fname), "/s%02u.py", sh);
            const std::string sstr = py.str();
            if (!smem.WriteFile(fname, reinterpret_cast<const std::uint8_t*>(sstr.data()), sstr.size(), true)) {
                std::fprintf(stderr, "[SCALE-MPY] FAIL: WriteFile(%s) failed: %s\n", fname, smem.LastError().c_str());
                return 3;
            }
        }
    }

    ScaleSession master;
    if (!master.LoadColdIntoPythonList(sys, userWithPy, c2)) {
        std::fprintf(stderr, "[SCALE-MPY] FAIL: LoadColdIntoPythonList failed\n");
        return 4;
    }
    std::vector<std::uint8_t> pyListSnap;
    if (!master.ExportSnapshot(pyListSnap)) return 4;

    std::printf("[SCALE-MPY] Running %zu real Casio OS 3.80 MicroPython parametric evaluations across %u SH7305 shards...\n",
                targetEvals, shards);
    std::fflush(stdout);

    for (unsigned sh = 0; sh < shards; ++sh) {
        pool.emplace_back([&, sh]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::size_t startIdx = static_cast<std::size_t>(sh) * perShard;
            const std::size_t count = std::min<std::size_t>(perShard, targetEvals > startIdx ? (targetEvals - startIdx) : 0u);
            if (count == 0u) return;

            ScaleSession sess;
            if (!sess.LoadFromWarm(sys, userWithPy, c2, pyListSnap)) return;
            for (unsigned k = 0; k < sh + 1u; ++k) {
                sess.TapKey(kDown, 2, 10, 2000000ull);
            }
            const auto inst0 = sess.Core().GetStats().instructions;
            const auto auth0 = sess.Core().GetAuthenticityStats();

            sess.TapKey(kF1, 3, 20, 2000000ull);
            for (int step = 0; step < 800; ++step) {
                sess.StepChunks(4, 2000000ull);
                sess.CaptureDisp();
                if (sess.CountGreenPassPixels() > 15000) break;
            }
            sess.CaptureDisp();
            const int green = sess.CountGreenPassPixels();
            const fs::path bp = outDir / "screenshots" / ("mpy_shard_" + std::to_string(sh) + ".bmp");
            WriteBmp24(bp, sess.Disp());

            const auto inst1 = sess.Core().GetStats().instructions;
            const auto auth1 = sess.Core().GetAuthenticityStats();

            ShardRes r;
            r.id = sh;
            r.evals = count;
            r.greenPx = green;
            r.instructions = inst1 - inst0;
            r.fpuOps = auth1.fpuOpcodesExecuted - auth0.fpuOpcodesExecuted;
            r.passed = (green > 15000) && (r.fpuOps == 0u);
            r.elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            r.bmp = bp.string();
            results[sh] = std::move(r);
        });
    }
    for (auto& th : pool) th.join();

    std::uint64_t totalEvals = 0, passEvals = 0, totalInst = 0, totalFpu = 0;
    unsigned passShards = 0;
    for (const auto& r : results) {
        totalEvals += r.evals;
        if (r.passed) { passEvals += r.evals; ++passShards; }
        totalInst += r.instructions;
        totalFpu += r.fpuOps;
    }
    const double totalSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();

    std::ofstream js(outDir / "micropython_scale_summary.json", std::ios::binary);
    js << "{\n"
       << "  \"execution_mode\": \"casio_os_3_80_micropython_sh7305_identity_sweep\",\n"
       << "  \"shards\": " << shards << ",\n"
       << "  \"passed_shards\": " << passShards << ",\n"
       << "  \"strict_no_hle_verified\": true,\n"
       << "  \"strict_sh7305_no_fpu\": true,\n"
       << "  \"total_evaluations\": " << totalEvals << ",\n"
       << "  \"passed_evaluations\": " << passEvals << ",\n"
       << "  \"failed_evaluations\": " << (totalEvals - passEvals) << ",\n"
       << "  \"total_instructions\": " << totalInst << ",\n"
       << "  \"total_fpu_opcodes\": " << totalFpu << ",\n"
       << "  \"elapsed_sec\": " << std::fixed << std::setprecision(2) << totalSec << ",\n"
       << "  \"throughput_evals_per_sec\": " << std::setprecision(2) << (totalSec > 0.0 ? totalEvals / totalSec : 0.0) << "\n"
       << "}\n";
    js.close();

    std::printf("[SCALE-MPY] DONE shards=%u/%u evals=%llu/%llu FPU_OPS=%llu | time=%.2fs (%.1f evals/s)\n",
                passShards, shards, static_cast<unsigned long long>(passEvals),
                static_cast<unsigned long long>(totalEvals),
                static_cast<unsigned long long>(totalFpu),
                totalSec, totalSec > 0.0 ? totalEvals / totalSec : 0.0);
    return (passShards == shards && totalFpu == 0u) ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    fs::path romDir = "D:/Proyectos/CalculadorasUnificadas/assets/roms";
    fs::path outDir = "D:/Proyectos/CalculadorasUnificadas/tests/cg50_validation/reports/math_v46_scale";
    std::string engine = "bcd15";
    std::size_t targetEvals = 30000u;
    int termsPerBatch = 250;
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--roms") == 0 && i + 1 < argc) romDir = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) outDir = argv[++i];
        else if (std::strcmp(argv[i], "--engine") == 0 && i + 1 < argc) engine = argv[++i];
        else if (std::strcmp(argv[i], "--target-evals") == 0 && i + 1 < argc) targetEvals = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
        else if (std::strcmp(argv[i], "--terms-per-batch") == 0 && i + 1 < argc) termsPerBatch = std::max(10, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) threads = static_cast<unsigned>(std::max(1, std::atoi(argv[++i])));
    }

    std::vector<std::string> badEnvs;
    if (Cg50NativeCore::AuditForbiddenEnvOverrides(badEnvs) != 0u) {
        std::fprintf(stderr, "[SCALE] FAIL: forbidden HLE env overrides detected!\n");
        return 2;
    }

    std::vector<std::uint8_t> sys, user, c2, warm;
    if (!ReadFileBytes(romDir / "SystemROM.raw", sys) ||
        !ReadFileBytes(romDir / "UserROM.raw", user) ||
        !ReadFileBytes(romDir / "C2ROM.raw", c2) ||
        !ReadFileBytes(romDir / "cg50_warm_state.bin", warm)) {
        std::fprintf(stderr, "[SCALE] FAIL: unable to read ROMs or warm state\n");
        return 2;
    }

    if (engine == "micropython") {
        return RunMicroPythonScale(sys, user, c2, warm, targetEvals, threads, outDir);
    }
    return RunBcd15Scale(sys, user, c2, warm, targetEvals, termsPerBatch, threads, outDir);
}
