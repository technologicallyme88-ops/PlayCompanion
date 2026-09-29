#include "HarvestTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

#include "CrossPointSettings.h"
#include "RecentBooksStore.h"
#include "apps_local/farm/FarmState.h"
#include "components/UITheme.h"
#include "components/icons/cover.h"

#ifndef HARVEST_PLOT_VARIANT
#define HARVEST_PLOT_VARIANT 1
#endif

namespace {
constexpr int kColumns = 2;
constexpr int kRows = 2;
constexpr int kCoverInset = 8;
constexpr int kCoverSelectionPadding = 10;
constexpr int kCoverSelectionRadius = 6;
constexpr int kFallbackCoverWidth = 136;
int harvestCoverWidth = kFallbackCoverWidth;
using CropSprite = std::array<std::string_view, 16>;

static constexpr CropSprite kSeedSprite = {
    "................", "....###..###....", "...##########...", "..############..",
    "....########....", ".....######.....", "....########....", "...##########...",
    "....##....##....", "....#......#....", "....#......#....", ".....#....#.....",
    ".....######.....", "......####......", "................", "................",
};

static constexpr CropSprite kSproutSprite = {
    "................", "..####......##..", ".######....####..", "..######..#####.",
    "....##########..", ".....########...", "......######....",  ".....########...",
    "....##....####..", "....#......###..", "....#.......#...",  ".....#.....#....",
    ".....#######....", "......#####.....", ".......##.......",  "................",
};

static constexpr CropSprite kLeafySprite = {
    "....##....##....", "...####..####...",  "....##....##....", ".......##.......",
    "..##...####...##", ".####...##...####", "..##....##....##", "........##......",
    "......####......", "....########....",  "...##.####.##...", ".....######.....",
    "......####......", ".......##.......",  ".......##.......", "................",
};

static constexpr CropSprite kParsnipSprite = {
    "..###......###..", ".#####....#####..", "..#####..#####..", "....########....",
    ".....######.....", "....########....",  "...##########...", "...##......##...",
    "..##........##..", "..##........##..",  "...##......##...", "....########....",
    ".....######.....", "......####......",  ".......##.......", "................",
};

void drawPlant(const GfxRenderer& renderer, const int cx, const int baseY, const int stage) {
  if (stage <= 0) return;
  const CropSprite* sprite = &kSeedSprite;
  if (stage == 2)
    sprite = &kSproutSprite;
  else if (stage == 3)
    sprite = &kLeafySprite;
  else if (stage >= 4)
    sprite = &kParsnipSprite;

  constexpr int scale = 3;
  constexpr int spriteSize = 16 * scale;
  const int left = cx - spriteSize / 2;
  const int top = baseY - spriteSize - 10;
  for (int row = 0; row < 16; ++row) {
    for (int col = 0; col < 16; ++col) {
      if ((*sprite)[row][col] == '#') renderer.fillRect(left + col * scale, top + row * scale, scale, scale, true);
    }
  }
}

const char* branchSlug(const farm::CropBranch branch) {
  if (branch == farm::CropBranch::Scholar) return "scholar";
  if (branch == farm::CropBranch::Wild) return "wild";
  return "default";
}

bool drawPlantFromSd(const GfxRenderer& renderer, const int cx, const int baseY, const int size,
                     const farm::Plot& plot) {
  if (plot.cropId == 0 || plot.stage == 0) return false;
  char path[96]{};
  std::snprintf(path, sizeof(path), "/.crosspoint/harvest/crops/%02u-%u-%s.bmp", static_cast<unsigned>(plot.cropId),
                static_cast<unsigned>(plot.stage), branchSlug(plot.branch));
  HalFile file;
  if (!Storage.openFileForRead("HARVEST", path, file)) {
    std::snprintf(path, sizeof(path), "/.crosspoint/harvest/crops/stage-%u.bmp", static_cast<unsigned>(plot.stage));
    if (!Storage.openFileForRead("HARVEST", path, file)) return false;
  }
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) {
    LOG_ERR("HARVEST", "Invalid crop art: %s", path);
    return false;
  }
  renderer.drawBitmap(bitmap, cx - size / 2, baseY - size, size, size, 0.0f, 0.0f);
  return true;
}
}  // namespace

void HarvestTheme::drawRecentBookCover(GfxRenderer& renderer, const Rect rect,
                                       const std::vector<RecentBook>& recentBooks, const int selectorIndex,
                                       bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                                       std::function<bool()> storeCoverBuffer) const {
  (void)bufferRestored;
  if (recentBooks.empty()) return;

  const RecentBook& book = recentBooks[0];
  const int coverX = rect.x + HarvestMetrics::values.contentSidePadding + kCoverInset;
  const int coverY = rect.y + kCoverInset;
  if (!coverRendered) {
    bool hasCover = false;
    if (!book.coverBmpPath.empty()) {
      const std::string coverPath =
          UITheme::getCoverThumbPath(book.coverBmpPath, HarvestMetrics::values.homeCoverHeight);
      HalFile file;
      if (Storage.openFileForRead("HOME", coverPath, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          harvestCoverWidth = bitmap.getWidth();
          renderer.drawBitmap(bitmap, coverX, coverY, harvestCoverWidth, HarvestMetrics::values.homeCoverHeight);
          hasCover = true;
        }
      }
    }

    renderer.drawRect(coverX, coverY, harvestCoverWidth, HarvestMetrics::values.homeCoverHeight, 1, true);
    if (!hasCover) {
      renderer.fillRect(coverX, coverY + HarvestMetrics::values.homeCoverHeight / 3, harvestCoverWidth,
                        HarvestMetrics::values.homeCoverHeight * 2 / 3, true);
      renderer.drawIcon(CoverIcon, coverX + 24, coverY + 24, 32);
    }

    // Cache only the unselected cover so moving focus can cleanly add or
    // remove the selection bubble without re-reading the SD card.
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  if (selectorIndex != 0) return;

  const int bubbleX = coverX - kCoverSelectionPadding;
  const int bubbleY = coverY - kCoverSelectionPadding;
  const int bubbleWidth = harvestCoverWidth + kCoverSelectionPadding * 2;
  const int coverHeight = HarvestMetrics::values.homeCoverHeight;
  renderer.fillRoundedRect(bubbleX, bubbleY, bubbleWidth, kCoverSelectionPadding, kCoverSelectionRadius, true, true,
                           false, false, Color::LightGray);
  renderer.fillRectDither(bubbleX, coverY, kCoverSelectionPadding, coverHeight, Color::LightGray);
  renderer.fillRectDither(coverX + harvestCoverWidth, coverY, kCoverSelectionPadding, coverHeight, Color::LightGray);
  renderer.fillRoundedRect(bubbleX, coverY + coverHeight, bubbleWidth, kCoverSelectionPadding, kCoverSelectionRadius,
                           false, false, true, true, Color::LightGray);
}

Rect HarvestTheme::getHomeFarmPlotRect(const Rect coverRect) const {
  if (!SETTINGS.farmingEnabled) return {};
  // Lyra's cover occupies the left 170-ish pixels. Start the farm immediately
  // after it so this panel replaces Lyra's title/author block rather than
  // drawing on top of it.
  constexpr int x = 198;
  return Rect{x, coverRect.y + 12, coverRect.x + coverRect.width - HarvestMetrics::values.contentSidePadding - x,
              coverRect.height - 24};
}

void HarvestTheme::drawHomeFarmPlot(const GfxRenderer& renderer, const Rect rect, const bool selected) const {
  if (!SETTINGS.farmingEnabled || rect.width <= 0 || rect.height <= 0) return;
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  const int cell = std::max(1, std::min((rect.width - 12) / kColumns, (rect.height - 12) / kRows));
  const int gridW = cell * kColumns;
  const int gridH = cell * kRows;
  const int left = rect.x + (rect.width - gridW) / 2;
  const int top = rect.y + (rect.height - gridH) / 2;

#if HARVEST_PLOT_VARIANT == 1
  renderer.drawRect(left - 3, top - 3, gridW + 6, gridH + 6, 2, true);
#elif HARVEST_PLOT_VARIANT == 2
  renderer.fillRectDither(left - 4, top - 4, gridW + 8, gridH + 8, Color::LightGray);
  renderer.drawRect(left - 4, top - 4, gridW + 8, gridH + 8, 2, true);
#else
  for (int row = 0; row < kRows; ++row) {
    renderer.fillRectDither(left, top + row * cell + 3, gridW, cell - 6, Color::LightGray);
  }
#endif

  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kColumns; ++col) {
      const int plotIndex = row * kColumns + col;
      const farm::Plot& plot = FARM_STATE.getPlots()[plotIndex];
      const int x = left + col * cell;
      const int y = top + row * cell;
#if HARVEST_PLOT_VARIANT != 3
      renderer.drawRect(x, y, cell, cell, 1, true);
#else
      renderer.drawLine(x, y + cell - 3, x + cell - 1, y + cell - 3, 1, true);
#endif
      if (plotIndex >= FARM_STATE.ownedPlots()) {
        renderer.drawRect(x + cell / 2 - 13, y + cell / 2 - 7, 26, 22, 2, true);
        renderer.drawRect(x + cell / 2 - 8, y + cell / 2 - 18, 16, 14, 2, true);
      } else if (plot.withered) {
        renderer.drawLine(x + cell / 2 - 16, y + cell / 2 - 18, x + cell / 2 + 16, y + cell / 2 + 14, 3, true);
        renderer.drawLine(x + cell / 2 + 16, y + cell / 2 - 18, x + cell / 2 - 16, y + cell / 2 + 14, 3, true);
      } else {
        const int artSize = std::max(32, std::min(72, cell - 10));
        if (!drawPlantFromSd(renderer, x + cell / 2, y + cell - 4, artSize, plot)) {
          // Sparse soil marks keep the fallback plots from reading as empty
          // white panels without turning them into a dense gray e-ink fill.
          renderer.fillRect(x + 13, y + cell - 17, 9, 3, true);
          renderer.fillRect(x + cell - 24, y + cell - 14, 11, 3, true);
          renderer.fillRect(x + 15, y + 16, 3, 3, true);
          renderer.fillRect(x + cell - 18, y + 22, 3, 3, true);
          drawPlant(renderer, x + cell / 2, y + cell - 6, plot.stage);
        }
        if (plot.stage >= 3 && plot.branch != farm::CropBranch::Default) {
          const int markX = x + cell - 15;
          const int markY = y + 8;
          if (plot.branch == farm::CropBranch::Scholar) {
            renderer.drawLine(markX - 5, markY + 5, markX + 5, markY + 5, 2, true);
            renderer.drawLine(markX, markY, markX, markY + 10, 2, true);
          } else {
            renderer.drawLine(markX - 5, markY, markX + 5, markY + 10, 2, true);
            renderer.drawLine(markX + 5, markY, markX - 5, markY + 10, 2, true);
          }
        }
      }
    }
  }

  if (selected) {
    const int bubbleX = rect.x - kCoverSelectionPadding;
    const int bubbleY = rect.y - kCoverSelectionPadding;
    const int bubbleWidth = rect.width + kCoverSelectionPadding * 2;
    renderer.fillRoundedRect(bubbleX, bubbleY, bubbleWidth, kCoverSelectionPadding, kCoverSelectionRadius, true, true,
                             false, false, Color::LightGray);
    renderer.fillRectDither(bubbleX, rect.y, kCoverSelectionPadding, rect.height, Color::LightGray);
    renderer.fillRectDither(rect.x + rect.width, rect.y, kCoverSelectionPadding, rect.height, Color::LightGray);
    renderer.fillRoundedRect(bubbleX, rect.y + rect.height, bubbleWidth, kCoverSelectionPadding, kCoverSelectionRadius,
                             false, false, true, true, Color::LightGray);
  }
}
