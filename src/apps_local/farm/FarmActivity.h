#pragma once

#include <array>
#include <cstdint>

#include "activities/Activity.h"

namespace farm {

class FarmActivity final : public Activity {
 public:
  FarmActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { Menu, Seeds, Harvest, Sell, Care, Shop, Quests, Herbarium, HerbDetail };

  void setScreen(Screen screen, int selection = 0);
  void goBack();
  void activate();
  void saveIfChanged(bool changed, const char* unchangedFeedback = nullptr);
  int itemCount() const;
  int rowsPerPage() const;
  int firstVisible() const;
  const char* title() const;
  void renderList();
  void renderFarmOverview();
  void renderHerbDetail();
  void drawStageArt(int x, int y, int size, uint8_t cropId, uint8_t stage, bool known);
  void drawRow(int row, int logicalIndex, const char* label, const char* detail, bool enabled = true);
  bool handleTap();

  Screen screen_ = Screen::Menu;
  int selected_ = 0;
  uint8_t herbCrop_ = 1;
  uint8_t selectedPlot_ = 0;
  uint8_t careStatsPlot_ = 0;
  bool plotSelectionArmed_ = false;
  bool cleanRefreshNeeded_ = true;
  int listTop_ = 0;
  int rowHeight_ = 0;
  int visibleRows_ = 0;
  int farmPreviewX_ = 0;
  int farmPreviewY_ = 0;
  int farmPreviewW_ = 0;
  int farmPreviewH_ = 0;
  uint32_t nextVitalRefreshMs_ = 0;
  std::array<char, 128> feedback_{};
};

}  // namespace farm
