#pragma once

#include "components/themes/lyra/LyraTheme.h"

namespace HarvestMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = LyraMetrics::values;
  v.listRowGap = 8;
  v.listRowRadius = 4;
  v.listInset = 16;
  v.listSidePadding = 24;
  v.headerSidePadding = 24;
  v.headerUnderlineSize = 4;
  v.headerTitleAlign = 0;
  v.menuSpacing = 8;
  v.popupCornerRadius = 4;
  v.optionPopupSelectionRadius = 4;
  return v;
}();
}  // namespace HarvestMetrics

// A monochrome, farm-sign interpretation of the existing card theme. It keeps
// RoundedRaff's proven home geometry and changes only static chrome and menu
// surfaces, where extra ink does not amplify e-ink ghosting during gameplay.
class HarvestTheme final : public LyraTheme {
 public:
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           std::function<bool()> storeCoverBuffer) const override;
  Rect getHomeFarmPlotRect(Rect coverRect) const override;
  void drawHomeFarmPlot(const GfxRenderer& renderer, Rect rect) const override;
};
