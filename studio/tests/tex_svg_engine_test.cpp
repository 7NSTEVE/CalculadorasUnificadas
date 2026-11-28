#include "../math/tex_svg_engine.hpp"

#include <iostream>

int main() {
    using DocStudio::Math::TexSvgEngine;
    int pass = 0, fail = 0;
    auto check = [&](bool cond, const char* name) {
        if (cond) { ++pass; std::cout << "[PASS] " << name << "\n"; }
        else { ++fail; std::cerr << "[FAIL] " << name << "\n"; }
    };

    check(TexSvgEngine::IsMathContent("\\frac{1}{2} + \\alpha"), "detects_latex_commands");
    check(TexSvgEngine::IsMathContent("E = mc^2"), "detects_exponent");
    check(!TexSvgEngine::IsMathContent("Hola mundo normal"), "ignores_plain_prose");

    auto& eng = TexSvgEngine::Instance();
    eng.ClearCache();
    check(eng.CacheSize() == 0, "cache_clear");

    if (eng.IsTexLiveAvailable()) {
        const auto r1 = eng.RenderSvgSync("\\frac{d}{dx} e^x = e^x", 20, 20, 20, 8000);
        check(r1.ok && !r1.from_cache && r1.svg_utf8.contains("<svg"), "texlive_renders_real_svg");
        const auto r2 = eng.RenderSvgSync("\\frac{d}{dx} e^x = e^x", 20, 20, 20, 8000);
        check(r2.ok && r2.from_cache && r2.svg_utf8 == r1.svg_utf8, "lru_cache_hit");
        check(eng.CacheSize() == 1, "cache_size_is_1");
    } else {
        const auto r1 = eng.RenderSvgSync("x^2 + 1");
        check(!r1.ok && !r1.error_utf8.empty(), "graceful_fallback_without_texlive");
    }

    std::cout << "SUMMARY: passed=" << pass << " failed=" << fail << "\n";
    if (fail == 0) {
        std::cout << "TEX_SVG_ENGINE_TESTS_PASS\n";
        return 0;
    }
    return 1;
}
