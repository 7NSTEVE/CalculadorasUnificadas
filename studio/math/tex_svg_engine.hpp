#pragma once
// ============================================================================
// studio/math/tex_svg_engine.hpp
// Native C++23 TeX Live -> dvisvgm Vector SVG Math Engine (ported from
// E:\Proyectos\CuadernU\CuadernU_Windows\src\core\math\MathEngine.cpp).
// Features:
//   - Zero Qt dependencies (Win32 CreateProcessW + Job Object sandbox)
//   - Mandatory -no-shell-escape sandbox flag + temp cleanup
//   - Thread-safe LRU-500 cache with in-flight deduplication
//   - Graceful fallback when TeX Live is unavailable
// ============================================================================

#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace DocStudio::Math {

struct TexSvgResult {
    bool ok = false;
    bool from_cache = false;
    std::string svg_utf8;
    std::string error_utf8;
};

class TexSvgEngine {
public:
    static TexSvgEngine& Instance();

    // Fast heuristic: returns true if text likely contains LaTeX/math notation.
    static bool IsMathContent(std::string_view text) noexcept;

    // Locates latex.exe and dvisvgm.exe (checks E:\texlive\2024\bin\windows and PATH).
    void LocateTexLive();
    bool IsTexLiveAvailable() const noexcept;
    std::string LatexBinPath() const;
    std::string DvisvgmBinPath() const;

    // Synchronous cached render (uses LRU-500 cache; invokes sandboxed latex -> dvisvgm on miss).
    TexSvgResult RenderSvgSync(std::string_view latex_code,
                               uint8_t r = 24, uint8_t g = 24, uint8_t b = 27,
                               uint32_t timeout_ms = 4000);

    // Non-blocking async render with in-flight deduplication.
    void RenderSvgAsync(std::string latex_code,
                        uint8_t r, uint8_t g, uint8_t b,
                        std::function<void(TexSvgResult)> on_done,
                        uint32_t timeout_ms = 4000);

    // Cache inspection / management
    size_t CacheSize() const;
    void ClearCache();

private:
    TexSvgEngine();

    static std::string ComputeKey(std::string_view latex, uint8_t r, uint8_t g, uint8_t b);
    TexSvgResult CompileViaTexLive(std::string_view latex_code,
                                   uint8_t r, uint8_t g, uint8_t b,
                                   const std::string& key,
                                   uint32_t timeout_ms) const;
    void CachePutLocked(const std::string& key, const std::string& svg);

    static constexpr size_t kMaxCacheEntries = 500;

    mutable std::mutex mtx_;
    std::string latex_bin_;
    std::string dvisvgm_bin_;
    bool has_texlive_ = false;

    std::list<std::pair<std::string, std::string>> lru_list_;
    std::unordered_map<std::string, std::list<std::pair<std::string, std::string>>::iterator> lru_map_;
    std::unordered_map<std::string, std::vector<std::function<void(TexSvgResult)>>> in_flight_;
};

} // namespace DocStudio::Math
