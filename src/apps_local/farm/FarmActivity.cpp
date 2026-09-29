#include "FarmActivity.h"

#include <Bitmap.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "FarmState.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace farm {
namespace {
constexpr int SIDE = 18;
constexpr int FOOTER = 42;
constexpr int DETAIL_GAP = 3;
constexpr uint16_t UPGRADE_PRICES[5] = {250, 400, 500, 650, 300};

const StrId CROP_NAMES[CROP_COUNT] = {
    StrId::STR_FARM_CROP_PARSNIP, StrId::STR_FARM_CROP_CAULIFLOWER, StrId::STR_FARM_CROP_BLUEBERRY,
    StrId::STR_FARM_CROP_MELON, StrId::STR_FARM_CROP_CORN, StrId::STR_FARM_CROP_PUMPKIN,
    StrId::STR_FARM_CROP_WINTER_ROOT, StrId::STR_FARM_CROP_SNOW_YAM};

const StrId MENU_NAMES[7] = {StrId::STR_FARM_BUY_SEEDS, StrId::STR_FARM_HARVEST, StrId::STR_FARM_SELL_CROPS,
                               StrId::STR_FARM_CARE, StrId::STR_FARM_SHOP, StrId::STR_FARM_QUESTS,
                               StrId::STR_FARM_HERBARIUM};

const StrId CARE_NAMES[5] = {StrId::STR_FARM_WATER, StrId::STR_FARM_SHADE, StrId::STR_FARM_WEED,
                              StrId::STR_FARM_FERTILIZE, StrId::STR_FARM_TEND};
const StrId CARE_HELP[5] = {StrId::STR_FARM_CARE_WATER_HELP, StrId::STR_FARM_CARE_SHADE_HELP,
                             StrId::STR_FARM_CARE_WEED_HELP, StrId::STR_FARM_CARE_FERTILIZE_HELP,
                             StrId::STR_FARM_CARE_TEND_HELP};
const StrId SHOP_NAMES[5] = {StrId::STR_FARM_MOSS_POLE, StrId::STR_FARM_SELF_WATERING,
                              StrId::STR_FARM_SLOW_FERTILIZER, StrId::STR_FARM_GREENHOUSE,
                              StrId::STR_FARM_PREMIUM_SPRAYER};
const StrId SHOP_HELP[5] = {StrId::STR_FARM_SHOP_MOSS_HELP, StrId::STR_FARM_SHOP_WATER_HELP,
                             StrId::STR_FARM_SHOP_FERTILIZER_HELP, StrId::STR_FARM_SHOP_GREENHOUSE_HELP,
                             StrId::STR_FARM_SHOP_SPRAYER_HELP};
const StrId QUEST_NAMES[QUEST_COUNT] = {StrId::STR_FARM_QUEST_READ, StrId::STR_FARM_QUEST_TEND,
                                         StrId::STR_FARM_QUEST_WATER, StrId::STR_FARM_QUEST_SPEEDY,
                                         StrId::STR_FARM_QUEST_NIGHT, StrId::STR_FARM_QUEST_STREAK};
const StrId QUEST_HELP[QUEST_COUNT] = {
    StrId::STR_FARM_QUEST_READ_HELP,   StrId::STR_FARM_QUEST_TEND_HELP,
    StrId::STR_FARM_QUEST_WATER_HELP,  StrId::STR_FARM_QUEST_SPEEDY_HELP,
    StrId::STR_FARM_QUEST_NIGHT_HELP,  StrId::STR_FARM_QUEST_STREAK_HELP,
};

const char* branchName(const CropBranch branch) {
  if (branch == CropBranch::Scholar) return tr(STR_FARM_PREMIUM);
  if (branch == CropBranch::Wild) return tr(STR_FARM_WILD);
  return tr(STR_FARM_STANDARD);
}

const char* seasonName(const uint8_t season) {
  static constexpr std::array<StrId, 4> SEASON_NAMES = {
      StrId::STR_FARM_SPRING,
      StrId::STR_FARM_SUMMER,
      StrId::STR_FARM_FALL,
      StrId::STR_FARM_WINTER,
  };
  return I18N.get(SEASON_NAMES[std::min<uint8_t>(season, SEASON_NAMES.size() - 1)]);
}
}  // namespace

FarmActivity::FarmActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Farm", renderer, mappedInput) {}

void FarmActivity::onEnter() {
  Activity::onEnter();
  setScreen(Screen::Menu);
}

void FarmActivity::setScreen(const Screen screen, const int selection) {
  screen_ = screen;
  selected_ = std::clamp(selection, 0, std::max(0, itemCount() - 1));
  if (screen != Screen::Menu) plotSelectionArmed_ = false;
  feedback_[0] = '\0';
  cleanRefreshNeeded_ = true;
  requestUpdate();
}

int FarmActivity::itemCount() const {
  switch (screen_) {
    case Screen::Menu: return 7;
    case Screen::Seeds: return 2;
    case Screen::Harvest: return 1;
    case Screen::Sell: return 1;
    case Screen::Care: return 5;
    case Screen::Shop: return 8;
    case Screen::Quests: return QUEST_COUNT;
    case Screen::Herbarium: return CROP_COUNT;
    case Screen::HerbDetail: return 0;
  }
  return 0;
}

int FarmActivity::rowsPerPage() const { return std::max(1, visibleRows_); }
int FarmActivity::firstVisible() const { return selected_ / rowsPerPage() * rowsPerPage(); }

const char* FarmActivity::title() const {
  switch (screen_) {
    case Screen::Menu: return tr(STR_FARM);
    case Screen::Seeds: return tr(STR_FARM_BUY_SEEDS);
    case Screen::Harvest: return tr(STR_FARM_HARVEST);
    case Screen::Sell: return tr(STR_FARM_SELL_CROPS);
    case Screen::Care: return tr(STR_FARM_CARE);
    case Screen::Shop: return tr(STR_FARM_SHOP);
    case Screen::Quests: return tr(STR_FARM_QUESTS);
    case Screen::Herbarium:
    case Screen::HerbDetail: return tr(STR_FARM_HERBARIUM);
  }
  return tr(STR_FARM);
}

void FarmActivity::goBack() {
  if (screen_ == Screen::Menu) {
    finish();
  } else if (screen_ == Screen::HerbDetail) {
    setScreen(Screen::Herbarium, herbCrop_ - 1);
  } else {
    setScreen(Screen::Menu);
  }
}

void FarmActivity::saveIfChanged(const bool changed, const char* unchangedFeedback) {
  if (changed) {
    if (!FARM_STATE.saveToFile()) LOG_ERR("FARM", "Failed to save farm action");
    snprintf(feedback_.data(), feedback_.size(), "%s", tr(STR_FARM_ACTION_APPLIED));
  } else if (unchangedFeedback != nullptr) {
    snprintf(feedback_.data(), feedback_.size(), "%s", unchangedFeedback);
  }
  requestUpdate();
}

void FarmActivity::activate() {
  switch (screen_) {
    case Screen::Menu: {
      static constexpr Screen destinations[7] = {Screen::Seeds, Screen::Harvest, Screen::Sell, Screen::Care,
                                                  Screen::Shop, Screen::Quests, Screen::Herbarium};
      setScreen(destinations[selected_]);
      break;
    }
    case Screen::Seeds: {
      const uint8_t cropId = static_cast<uint8_t>(FARM_STATE.season() * 2 + selected_ + 1);
      saveIfChanged(FARM_STATE.buySeedAndPlant(cropId));
      break;
    }
    case Screen::Harvest: saveIfChanged(FARM_STATE.harvestAll(), tr(STR_FARM_NOTHING_TO_HARVEST)); break;
    case Screen::Sell: saveIfChanged(FARM_STATE.sellAll()); break;
    case Screen::Care:
      saveIfChanged(FARM_STATE.careForPlot(static_cast<CareAction>(selected_), selectedPlot_));
      break;
    case Screen::Shop: {
      bool changed = false;
      if (selected_ < 5)
        changed = FARM_STATE.buyUpgrade(static_cast<uint8_t>(selected_));
      else if (selected_ == 5)
        changed = FARM_STATE.refillWater();
      else if (selected_ == 6)
        changed = FARM_STATE.restockFertilizer();
      else
        changed = FARM_STATE.buyNextPlot();
      saveIfChanged(changed);
      break;
    }
    case Screen::Herbarium:
      herbCrop_ = static_cast<uint8_t>(selected_ + 1);
      setScreen(Screen::HerbDetail);
      break;
    case Screen::Quests: saveIfChanged(FARM_STATE.claimQuest(static_cast<uint8_t>(selected_))); break;
    case Screen::HerbDetail: break;
  }
}

bool FarmActivity::handleTap() {
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return false;
  if (screen_ == Screen::Menu && x >= farmPreviewX_ && x < farmPreviewX_ + farmPreviewW_ &&
      y >= farmPreviewY_ && y < farmPreviewY_ + farmPreviewH_) {
    const int column = (x - farmPreviewX_) * 2 / farmPreviewW_;
    const int row = (y - farmPreviewY_) * 2 / farmPreviewH_;
    const uint8_t tappedPlot = static_cast<uint8_t>(std::clamp(row * 2 + column, 0, PLOT_COUNT - 1));
    if (!plotSelectionArmed_ || tappedPlot != selectedPlot_) {
      selectedPlot_ = tappedPlot;
      plotSelectionArmed_ = true;
      requestUpdate();
      return true;
    }
    if (selectedPlot_ < FARM_STATE.ownedPlots())
      setScreen(Screen::Care);
    else
      setScreen(Screen::Shop, 7);
    return true;
  }
  if (rowHeight_ <= 0 || y < listTop_) return false;
  const int row = (y - listTop_) / rowHeight_;
  if (row < 0 || row >= visibleRows_) return false;
  const int index = firstVisible() + row;
  if (index >= itemCount()) return false;
  plotSelectionArmed_ = false;
  selected_ = index;
  activate();
  return true;
}

void FarmActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    goBack();
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Right) {
    goBack();
    return;
  }
  if (screen_ == Screen::HerbDetail) {
    if (swipe == MappedInputManager::SwipeDir::Up ||
        mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      herbCrop_ = static_cast<uint8_t>(herbCrop_ % CROP_COUNT + 1);
      requestUpdate();
    } else if (swipe == MappedInputManager::SwipeDir::Down ||
               mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      herbCrop_ = static_cast<uint8_t>((herbCrop_ + CROP_COUNT - 2) % CROP_COUNT + 1);
      requestUpdate();
    }
    return;
  }
  if (handleTap()) return;
  const int count = itemCount();
  if (count <= 0) return;
  if (swipe == MappedInputManager::SwipeDir::Up || mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selected_ = (selected_ + 1) % count;
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Down || mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selected_ = (selected_ + count - 1) % count;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) activate();
}

void FarmActivity::drawRow(const int row, const int logicalIndex, const char* label, const char* detail,
                           const bool enabled) {
  const int sw = renderer.getScreenWidth();
  const int y = listTop_ + row * rowHeight_;
  const bool selected = logicalIndex == selected_;
  if (!enabled) renderer.fillRectDither(SIDE + 2, y + 4, sw - SIDE * 2 - 4, rowHeight_ - 8, Color::LightGray);
  if (selected) renderer.fillRect(SIDE, y + 3, sw - SIDE * 2, rowHeight_ - 6, true);
  const bool ink = !selected;
  renderer.drawText(UI_12_FONT_ID, SIDE + 14, y + 9, label, ink, EpdFontFamily::BOLD);
  if (detail && *detail) renderer.drawText(SMALL_FONT_ID, SIDE + 14, y + 37, detail, ink);
}

void FarmActivity::renderFarmOverview() {
  const int sw = renderer.getScreenWidth();
  renderer.drawText(UI_12_FONT_ID, SIDE + 6, 29, tr(STR_FARM), true, EpdFontFamily::BOLD);
  char line[96];
  snprintf(line, sizeof(line), "%s: %u", tr(STR_FARM_COINS), FARM_STATE.coinBalance());
  renderer.drawText(SMALL_FONT_ID, SIDE + 6, 69, line);
  snprintf(line, sizeof(line), "%s: %u/3", tr(STR_FARM_WATER_STOCK), FARM_STATE.waterCharges());
  renderer.drawText(SMALL_FONT_ID, SIDE + 6, 91, line);
  snprintf(line, sizeof(line), "%s: %u/3", tr(STR_FARM_FERTILIZER_STOCK), FARM_STATE.fertilizerCharges());
  renderer.drawText(SMALL_FONT_ID, SIDE + 6, 113, line);
  const Plot& plot = FARM_STATE.getPlots()[selectedPlot_];
  snprintf(line, sizeof(line), "%s %u", tr(STR_FARM_PLOT), selectedPlot_ + 1);
  renderer.drawText(SMALL_FONT_ID, SIDE + 6, 144, line, true, EpdFontFamily::BOLD);
  snprintf(line, sizeof(line), "%s: %u", tr(STR_FARM_MOISTURE), plot.moisture);
  renderer.drawText(SMALL_FONT_ID, SIDE + 6, 166, line);
  snprintf(line, sizeof(line), "%s: %u", tr(STR_FARM_LIGHT), plot.sunlight);
  renderer.drawText(SMALL_FONT_ID, SIDE + 6, 188, line);
  snprintf(line, sizeof(line), "%s: %u", tr(STR_FARM_HEALTH), plot.health);
  renderer.drawText(SMALL_FONT_ID, SIDE + 6, 210, line);
  snprintf(line, sizeof(line), "%s: %u", tr(STR_FARM_NUTRIENTS), plot.nutrients);
  renderer.drawText(SMALL_FONT_ID, SIDE + 6, 232, line);

  farmPreviewX_ = 174;
  farmPreviewY_ = 18;
  farmPreviewW_ = sw - farmPreviewX_ - SIDE;
  farmPreviewH_ = 236;
  GUI.drawHomeFarmPlot(renderer, Rect{farmPreviewX_, farmPreviewY_, farmPreviewW_, farmPreviewH_});
  const int cell = std::max(1, std::min((farmPreviewW_ - 12) / 2, (farmPreviewH_ - 12) / 2));
  const int gridX = farmPreviewX_ + (farmPreviewW_ - cell * 2) / 2;
  const int gridY = farmPreviewY_ + (farmPreviewH_ - cell * 2) / 2;
  const int selectedX = gridX + (selectedPlot_ % 2) * cell;
  const int selectedY = gridY + (selectedPlot_ / 2) * cell;
  renderer.drawRect(selectedX + 3, selectedY + 3, cell - 6, cell - 6, 3, true);
}

void FarmActivity::renderList() {
  const int count = itemCount();
  const int start = firstVisible();
  char label[96];
  char detail[128];
  for (int row = 0; row < visibleRows_ && start + row < count; ++row) {
    const int i = start + row;
    label[0] = detail[0] = '\0';
    bool enabled = true;
    switch (screen_) {
      case Screen::Menu:
        if (i == 5 && FARM_STATE.claimableQuestCount() > 0)
          snprintf(label, sizeof(label), "%s (%u)", I18N.get(MENU_NAMES[i]), FARM_STATE.claimableQuestCount());
        else
          snprintf(label, sizeof(label), "%s", I18N.get(MENU_NAMES[i]));
        break;
      case Screen::Seeds: {
        const uint8_t cropId = static_cast<uint8_t>(FARM_STATE.season() * 2 + i + 1);
        const auto& crop = FarmState::crop(cropId);
        snprintf(label, sizeof(label), "%s", I18N.get(CROP_NAMES[cropId - 1]));
        snprintf(detail, sizeof(detail), "%s: %u %s  |  %s: %u %s  |  %s: %u", tr(STR_FARM_BUY_PRICE),
                 crop.seedPrice, tr(STR_FARM_COIN), tr(STR_FARM_GROWTH_TIME), crop.growDays, tr(STR_FARM_DAYS),
                 tr(STR_FARM_SEEDS_BOUGHT), FARM_STATE.seedsPurchased(cropId));
        enabled = FARM_STATE.coinBalance() >= crop.seedPrice;
        break;
      }
      case Screen::Harvest:
        snprintf(label, sizeof(label), "%s", tr(STR_FARM_HARVEST));
        snprintf(detail, sizeof(detail), "%s", tr(STR_FARM_HARVEST_HELP));
        break;
      case Screen::Sell:
        snprintf(label, sizeof(label), "%s", tr(STR_FARM_SELL_CROPS));
        snprintf(detail, sizeof(detail), "%s", tr(STR_FARM_SELL_HELP));
        break;
      case Screen::Care:
        snprintf(label, sizeof(label), "%s", I18N.get(CARE_NAMES[i]));
        snprintf(detail, sizeof(detail), "%s", I18N.get(CARE_HELP[i]));
        if (i == 0) snprintf(label, sizeof(label), "%s (%u/3)", I18N.get(CARE_NAMES[i]), FARM_STATE.waterCharges());
        if (i == 3)
          snprintf(label, sizeof(label), "%s (%u/3)", I18N.get(CARE_NAMES[i]), FARM_STATE.fertilizerCharges());
        break;
      case Screen::Shop:
        if (i < 5) {
          snprintf(label, sizeof(label), "%s", I18N.get(SHOP_NAMES[i]));
          if (FARM_STATE.ownsUpgrade(i))
            snprintf(detail, sizeof(detail), "%s  |  %s", tr(STR_FARM_OWNED), I18N.get(SHOP_HELP[i]));
          else
            snprintf(detail, sizeof(detail), "%u %s  |  %s", UPGRADE_PRICES[i], tr(STR_FARM_COIN),
                     I18N.get(SHOP_HELP[i]));
        } else if (i == 5) {
          snprintf(label, sizeof(label), "%s (%u/3)", tr(STR_FARM_REFILL_WATER), FARM_STATE.waterCharges());
          snprintf(detail, sizeof(detail), "%s", tr(STR_FARM_FREE));
        } else if (i == 6) {
          snprintf(label, sizeof(label), "%s (%u/3)", tr(STR_FARM_RESTOCK_FERTILIZER),
                   FARM_STATE.fertilizerCharges());
          snprintf(detail, sizeof(detail), "30 %s", tr(STR_FARM_COIN));
        } else {
          snprintf(label, sizeof(label), "%s %u", tr(STR_FARM_BUY_PLOT), FARM_STATE.ownedPlots() + 1);
          snprintf(detail, sizeof(detail), "%u %s", FARM_STATE.nextPlotPrice(), tr(STR_FARM_COIN));
        }
        break;
      case Screen::Quests: {
        const QuestProgress quest = FARM_STATE.quest(i);
        snprintf(label, sizeof(label), "%s", I18N.get(QUEST_NAMES[i]));
        const char* status = quest.claimed    ? tr(STR_FARM_REWARD_CLAIMED)
                             : quest.complete ? tr(STR_FARM_CLAIM_REWARD)
                                              : I18N.get(QUEST_HELP[i]);
        snprintf(detail, sizeof(detail), "%u / %u - %s", quest.progress, quest.goal, status);
        break;
      }
      case Screen::Herbarium: {
        const uint8_t cropId = static_cast<uint8_t>(i + 1);
        uint8_t known = 0;
        for (uint8_t stage = 0; stage < 4; ++stage)
          if (FARM_STATE.discoveredStages() & (1UL << (i * 4 + stage))) known++;
        snprintf(label, sizeof(label), "%s", I18N.get(CROP_NAMES[i]));
        snprintf(detail, sizeof(detail), "%s: %u/4  |  %s: %u", tr(STR_FARM_DISCOVERED), known,
                 tr(STR_FARM_LIFETIME_SOLD),
                 FARM_STATE.soldCount(cropId, CropBranch::Default) +
                     FARM_STATE.soldCount(cropId, CropBranch::Scholar) + FARM_STATE.soldCount(cropId, CropBranch::Wild));
        break;
      }
      case Screen::HerbDetail: break;
    }
    drawRow(row, i, label, detail, enabled);
  }
}

void FarmActivity::drawStageArt(const int x, const int y, const int size, const uint8_t cropId,
                                const uint8_t stage, const bool known) {
  renderer.drawRect(x, y, size, size, 1, true);
  if (!known) {
    UITheme::drawCenteredText(renderer, Rect{x, y, size, size}, UI_12_FONT_ID, y + size / 2 - 12, "?");
    return;
  }
  char path[96];
  snprintf(path, sizeof(path), "/.crosspoint/harvest/crops/%02u-%u-default.bmp", cropId, stage);
  HalFile file;
  if (!Storage.openFileForRead("FARM", path, file)) {
    snprintf(path, sizeof(path), "/.crosspoint/harvest/crops/stage-%u.bmp", stage);
    if (!Storage.openFileForRead("FARM", path, file)) {
      const int cx = x + size / 2;
      const int base = y + size - 8;
      renderer.drawLine(cx, base, cx, base - 12 - stage * 5, 2, true);
      renderer.fillRect(cx - 3 - stage * 2, base - 13 - stage * 5, 7 + stage * 4, 5 + stage * 2, true);
      return;
    }
  }
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() == BmpReaderError::Ok) renderer.drawBitmap(bitmap, x + 3, y + 3, size - 6, size - 6, 0, 0);
}

void FarmActivity::renderHerbDetail() {
  const int sw = renderer.getScreenWidth();
  const int index = herbCrop_ - 1;
  constexpr int contentShift = 26;
  renderer.drawCenteredText(UI_12_FONT_ID, 72 + contentShift, I18N.get(CROP_NAMES[index]), true,
                            EpdFontFamily::BOLD);
  const int gap = 8;
  const int size = (sw - SIDE * 2 - gap * 3) / 4;
  for (uint8_t stage = 1; stage <= 4; ++stage) {
    const bool known = (FARM_STATE.discoveredStages() & (1UL << (index * 4 + stage - 1))) != 0;
    const int x = SIDE + (stage - 1) * (size + gap);
    drawStageArt(x, 110 + contentShift, size, herbCrop_, stage, known);
    char caption[24];
    snprintf(caption, sizeof(caption), "%s %u", tr(STR_FARM_STAGE), stage);
    UITheme::drawCenteredText(renderer, Rect{x, 110 + contentShift + size, size, 28}, SMALL_FONT_ID,
                              110 + contentShift + size + 5, caption);
  }
  const bool bought = FARM_STATE.seedsPurchased(herbCrop_) > 0;
  const auto& crop = FarmState::crop(herbCrop_);
  int y = 250 + contentShift;
  char line[96];
  snprintf(line, sizeof(line), "%s: %s", tr(STR_FARM_BUY_PRICE), bought ? "" : tr(STR_FARM_UNKNOWN));
  if (bought)
    snprintf(line, sizeof(line), "%s: %u %s", tr(STR_FARM_BUY_PRICE), crop.seedPrice, tr(STR_FARM_COIN));
  renderer.drawText(UI_12_FONT_ID, SIDE, y, line, true, EpdFontFamily::BOLD);
  y += 52;
  snprintf(line, sizeof(line), "%s: %s  |  %s: %u %s", tr(STR_FARM_SEASON), seasonName(crop.season),
           tr(STR_FARM_GROWTH_TIME), crop.growDays, tr(STR_FARM_DAYS));
  renderer.drawText(UI_12_FONT_ID, SIDE, y, line, true, EpdFontFamily::BOLD);
  y += 52;
  for (uint8_t branch = 0; branch < 3; ++branch) {
    const auto branchId = static_cast<CropBranch>(branch);
    const uint16_t sold = FARM_STATE.soldCount(herbCrop_, branchId);
    snprintf(line, sizeof(line), "%s: %s", branchName(branchId), sold ? "" : tr(STR_FARM_UNKNOWN));
    if (sold)
      snprintf(line, sizeof(line), "%s: %u %s  |  %s: %u", branchName(branchId),
               FARM_STATE.branchSellPrice(herbCrop_, branchId), tr(STR_FARM_COIN),
               tr(STR_FARM_LIFETIME_SOLD), sold);
    renderer.drawText(UI_12_FONT_ID, SIDE, y, line, true, EpdFontFamily::BOLD);
    y += 54;
  }
  renderer.drawText(SMALL_FONT_ID, SIDE, y + 12, tr(STR_FARM_DISCOVERY_HELP));
}

void FarmActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  farmPreviewX_ = farmPreviewY_ = farmPreviewW_ = farmPreviewH_ = 0;
  if (screen_ == Screen::HerbDetail) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, title());
    listTop_ = rowHeight_ = visibleRows_ = 0;
    renderHerbDetail();
  } else {
    if (screen_ == Screen::Menu) {
      renderFarmOverview();
      listTop_ = 261;
    } else {
      char headerStatus[32];
      const char* subtitle = nullptr;
      if (screen_ == Screen::Seeds) {
        snprintf(headerStatus, sizeof(headerStatus), "%s: %u", tr(STR_FARM_COINS), FARM_STATE.coinBalance());
        subtitle = headerStatus;
      }
      GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, title(), subtitle);
      listTop_ = metrics.topPadding + metrics.headerHeight + 12;
    }
    const int available = sh - listTop_ - FOOTER - 12;
    rowHeight_ = screen_ == Screen::Care || screen_ == Screen::Shop ? 72 : 64;
    visibleRows_ = std::max(1, available / rowHeight_);
    renderList();
  }
  if (feedback_[0]) renderer.drawText(SMALL_FONT_ID, SIDE, sh - FOOTER - 42, feedback_.data());
  if (screen_ == Screen::Care) {
    char credits[64];
    snprintf(credits, sizeof(credits), "%s: %u/5", tr(STR_FARM_CARE_CREDITS), FARM_STATE.careCreditCount());
    renderer.drawText(SMALL_FONT_ID, SIDE, sh - FOOTER - 20, credits);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_FARM_PREVIOUS_CROP),
                                            tr(STR_FARM_NEXT_CROP));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(cleanRefreshNeeded_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanRefreshNeeded_ = false;
}

}  // namespace farm
