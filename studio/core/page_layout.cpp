#include "page_layout.hpp"

#include <algorithm>

namespace DocStudio {

void PageLayout::Rebuild(
    const std::vector<PageModel>& pages,
    double pageGapPt,
    double topMarginPt,
    const std::vector<bool>* collapsedMask,
    bool doublePageSpread)
{
    gapPt_ = (std::max)(0.0, pageGapPt);
    topMarginPt_ = (std::max)(0.0, topMarginPt);
    doublePageSpread_ = doublePageSpread;
    placements_.clear();
    placements_.reserve(pages.size());

    auto isCollapsedAt = [&](std::size_t idx) -> bool {
        return collapsedMask && idx < collapsedMask->size() && (*collapsedMask)[idx];
    };

    double curY = topMarginPt_;
    maxWidth_ = 0.0;
    constexpr double kCollapsedBannerH = 34.0;
    constexpr double kSpreadGapX = 16.0;

    std::size_t i = 0;
    while (i < pages.size()) {
        if (isCollapsedAt(i)) {
            std::size_t endIdx = i;
            while (endIdx + 1 < pages.size() && isCollapsedAt(endIdx + 1)) {
                ++endIdx;
            }
            const double w = (std::max)(320.0, pages[i].EffectiveWidthPt());
            // First page in collapsed run holds the visible clickable accordion banner;
            // subsequent pages in the run have height 0 so total vertical height truly collapses!
            PagePlacement hdr{};
            hdr.pageIndex = i;
            hdr.leftWorld = -w * 0.5;
            hdr.topWorld = curY;
            hdr.widthWorld = w;
            hdr.heightWorld = kCollapsedBannerH;
            hdr.collapsed = true;
            hdr.collapsedHeader = true;
            hdr.collapsedEndPage0 = static_cast<int>(endIdx);
            placements_.push_back(hdr);
            if (w > maxWidth_) maxWidth_ = w;
            for (std::size_t k = i + 1; k <= endIdx; ++k) {
                PagePlacement hidden{};
                hidden.pageIndex = k;
                hidden.leftWorld = -w * 0.5;
                hidden.topWorld = curY + kCollapsedBannerH;
                hidden.widthWorld = 0.0;
                hidden.heightWorld = 0.0;
                hidden.collapsed = true;
                hidden.collapsedHeader = false;
                hidden.collapsedEndPage0 = static_cast<int>(endIdx);
                placements_.push_back(hidden);
            }
            curY += kCollapsedBannerH + gapPt_;
            i = endIdx + 1;
            continue;
        }

        if (doublePageSpread && (i + 1 < pages.size()) && !isCollapsedAt(i + 1)) {
            const double w0 = pages[i].EffectiveWidthPt();
            const double h0 = pages[i].EffectiveHeightPt();
            const double w1 = pages[i + 1].EffectiveWidthPt();
            const double h1 = pages[i + 1].EffectiveHeightPt();
            const double totalW = w0 + kSpreadGapX + w1;
            const double rowH = (std::max)(h0, h1);
            const double x0 = -totalW * 0.5;

            PagePlacement p0{};
            p0.pageIndex = i;
            p0.leftWorld = x0;
            p0.topWorld = curY;
            p0.widthWorld = w0;
            p0.heightWorld = h0;
            placements_.push_back(p0);

            PagePlacement p1{};
            p1.pageIndex = i + 1;
            p1.leftWorld = x0 + w0 + kSpreadGapX;
            p1.topWorld = curY;
            p1.widthWorld = w1;
            p1.heightWorld = h1;
            placements_.push_back(p1);

            if (totalW > maxWidth_) maxWidth_ = totalW;
            curY += rowH + gapPt_;
            i += 2;
            continue;
        }

        const double w = pages[i].EffectiveWidthPt();
        const double h = pages[i].EffectiveHeightPt();
        PagePlacement pl{};
        pl.pageIndex = i;
        pl.leftWorld = -w * 0.5;
        pl.topWorld = curY;
        pl.widthWorld = w;
        pl.heightWorld = h;
        placements_.push_back(pl);
        if (w > maxWidth_) maxWidth_ = w;
        curY += h + gapPt_;
        ++i;
    }
    totalHeight_ = pages.empty() ? 0.0 : (curY - gapPt_ + topMarginPt_);
}

const PagePlacement* PageLayout::PlacementAt(std::size_t pageIndex) const noexcept
{
    if (pageIndex >= placements_.size()) return nullptr;
    return &placements_[pageIndex];
}

bool PageLayout::ComputeVisibleRange(
    double viewTopWorld,
    double viewBottomWorld,
    std::size_t& outFirst,
    std::size_t& outLast,
    std::size_t prefetchMargin) const noexcept
{
    if (placements_.empty()) return false;
    if (viewBottomWorld < viewTopWorld) std::swap(viewTopWorld, viewBottomWorld);

    // Note: topWorld is guaranteed monotonically non-decreasing even for double-page spreads
    // with differing page heights and collapsed ranges (whereas topWorld + heightWorld is NOT).
    auto itPast = std::upper_bound(
        placements_.begin(), placements_.end(), viewBottomWorld,
        [](double y, const PagePlacement& p) {
            return y < p.topWorld;
        });
    const std::size_t last = (itPast == placements_.begin())
        ? 0
        : static_cast<std::size_t>((itPast - placements_.begin()) - 1);

    // Find earliest placement in [0..last] whose vertical span reaches viewTopWorld
    auto itTop = std::lower_bound(
        placements_.begin(), placements_.begin() + static_cast<std::ptrdiff_t>(last + 1), viewTopWorld,
        [](const PagePlacement& p, double y) {
            return p.topWorld < y;
        });
    std::size_t first = (itTop == placements_.end())
        ? last
        : static_cast<std::size_t>(itTop - placements_.begin());
    // Step backward across any earlier rows whose height still overlaps viewTopWorld
    while (first > 0) {
        const auto& prev = placements_[first - 1];
        const bool prevPairOverlap = (first >= 2 &&
            placements_[first - 2].topWorld == prev.topWorld &&
            (placements_[first - 2].topWorld + placements_[first - 2].heightWorld) >= viewTopWorld);
        if ((prev.topWorld + prev.heightWorld) >= viewTopWorld || prevPairOverlap || prev.collapsed) {
            --first;
        } else {
            break;
        }
    }

    outFirst = (first > prefetchMargin) ? (first - prefetchMargin) : 0;
    outLast = (std::min)(placements_.size() - 1, last + prefetchMargin);
    return true;
}

bool PageLayout::HitTestWorldPoint(
    double wx,
    double wy,
    std::size_t& outPageIndex,
    double& outLocalX,
    double& outLocalY) const noexcept
{
    if (placements_.empty()) return false;
    // topWorld is strictly non-decreasing; upper_bound gives first placement with topWorld > wy.
    auto itPast = std::upper_bound(
        placements_.begin(), placements_.end(), wy,
        [](double y, const PagePlacement& p) {
            return y < p.topWorld;
        });
    if (itPast == placements_.begin()) return false;

    std::size_t idx = static_cast<std::size_t>((itPast - placements_.begin()) - 1);
    // Walk backward across same-row spread pair and any taller overlapping row above
    std::size_t scannedNonCollapsed = 0;
    while (true) {
        const auto& pl = placements_[idx];
        if (pl.widthWorld > 0.0 && pl.heightWorld > 0.0) {
            const DocRect r = pl.WorldRect();
            if (r.Contains(wx, wy, 0.0)) {
                outPageIndex = pl.pageIndex;
                outLocalX = wx - r.x;
                outLocalY = wy - r.y;
                return true;
            }
            ++scannedNonCollapsed;
            if ((pl.topWorld + pl.heightWorld) < wy && scannedNonCollapsed >= 4) break;
        }
        if (idx == 0) break;
        --idx;
    }
    return false;
}

} // namespace DocStudio
