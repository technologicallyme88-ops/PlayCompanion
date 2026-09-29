#include "FarmState.h"

#include <CompanionMood.h>
#include <HalClock.h>
#include <Logging.h>

#include <algorithm>

#include "CrossPointSettings.h"

namespace farm {
namespace {
constexpr std::array<CropDefinition, CROP_COUNT> CROPS = {{
    {0, 4, 20, 35, false}, {0, 7, 35, 60, false},  // spring
    {1, 5, 25, 45, false}, {1, 8, 40, 75, true},   // summer
    {2, 6, 30, 55, false}, {2, 9, 45, 85, false},  // fall
    {3, 5, 25, 50, false}, {3, 8, 40, 80, true},   // winter
}};
constexpr std::array<uint16_t, PLOT_COUNT - 1> PLOT_PRICES = {250, 600, 1200};
constexpr std::array<uint16_t, QUEST_COUNT> QUEST_GOALS = {30, 5, 3, 15, 10, 3};
constexpr std::array<uint16_t, QUEST_COUNT> QUEST_REWARDS = {1, 1, 1, 1, 1, 1};
constexpr uint8_t MAX_VITAL = 100;
constexpr uint8_t MAX_CARE_CREDITS = 5;

uint8_t clampAdd(const uint8_t value, const uint8_t amount) {
  return static_cast<uint8_t>(std::min<int>(MAX_VITAL, value + amount));
}

uint8_t clampSub(const uint8_t value, const uint8_t amount) {
  return value > amount ? static_cast<uint8_t>(value - amount) : 0;
}

int32_t signedUtcOffsetQuarterHours() {
  uint8_t biased = SETTINGS.clockUtcOffsetQ;
  if (biased > 104) biased = 104;
  return static_cast<int32_t>(biased) - 48;
}
}  // namespace

FarmState::FarmState() = default;

const CropDefinition& FarmState::crop(const uint8_t cropId) {
  const int index = cropId > 0 && cropId <= CROP_COUNT ? cropId - 1 : 0;
  return CROPS[index];
}

void FarmState::toJson(JsonDocument& doc) const {
  doc["coins"] = coins;
  doc["farmDay"] = farmDay;
  doc["lastLocalDay"] = lastLocalDay;
  doc["nextSeasonCrop"] = nextSeasonCrop;
  doc["ownedPlots"] = ownedPlotCount;
  doc["waterStock"] = waterStock;
  doc["fertilizerStock"] = fertilizerStock;
  doc["upgrades"] = upgrades;
  doc["herbariumStages"] = herbariumStages;
  doc["streak"] = currentStreak;
  doc["booksFinished"] = booksFinished;
  doc["pagesToday"] = pagesToday;
  doc["tendsToday"] = tendsToday;
  doc["watersToday"] = watersToday;
  doc["maxSessionPages"] = maxSessionPages;
  doc["nightPages"] = nightPages;
  doc["questClaims"] = questClaims;
  doc["questRewardClaims"] = questRewardClaims;
  doc["careCredits"] = careCredits;
  doc["questDay"] = questDay;
  JsonArray plotArray = doc["plots"].to<JsonArray>();
  for (const Plot& plot : plots) {
    JsonObject obj = plotArray.add<JsonObject>();
    obj["crop"] = plot.cropId;
    obj["stage"] = plot.stage;
    obj["withered"] = plot.withered;
    obj["plantedDay"] = plot.plantedFarmDay;
    obj["pagesRead"] = plot.pagesRead;
    obj["careTotal"] = plot.careScoreTotal;
    obj["careSamples"] = plot.careSamples;
    obj["moisture"] = plot.moisture;
    obj["sunlight"] = plot.sunlight;
    obj["health"] = plot.health;
    obj["nutrients"] = plot.nutrients;
    obj["branch"] = static_cast<uint8_t>(plot.branch);
  }
  JsonArray harvestedArray = doc["harvested"].to<JsonArray>();
  for (const auto& cropCounts : harvested) {
    JsonArray cropArray = harvestedArray.add<JsonArray>();
    for (const uint16_t count : cropCounts) cropArray.add(count);
  }
  JsonArray soldArray = doc["sold"].to<JsonArray>();
  for (const auto& cropCounts : sold) {
    JsonArray cropArray = soldArray.add<JsonArray>();
    for (const uint16_t count : cropCounts) cropArray.add(count);
  }
  JsonArray purchasedArray = doc["purchased"].to<JsonArray>();
  for (const uint16_t count : purchased) purchasedArray.add(count);
}

bool FarmState::fromJson(const JsonVariantConst doc) {
  coins = doc["coins"] | static_cast<uint16_t>(0);
  farmDay = doc["farmDay"] | 0;
  lastLocalDay = doc["lastLocalDay"] | 0;
  nextSeasonCrop = doc["nextSeasonCrop"] | static_cast<uint8_t>(0);
  ownedPlotCount = doc["ownedPlots"] | static_cast<uint8_t>(1);
  if (ownedPlotCount < 1) ownedPlotCount = 1;
  if (ownedPlotCount > PLOT_COUNT) ownedPlotCount = PLOT_COUNT;
  waterStock = std::min<uint8_t>(3, doc["waterStock"] | static_cast<uint8_t>(3));
  fertilizerStock = std::min<uint8_t>(3, doc["fertilizerStock"] | static_cast<uint8_t>(3));
  upgrades = doc["upgrades"] | static_cast<uint8_t>(0);
  herbariumStages = doc["herbariumStages"] | static_cast<uint32_t>(0);
  currentStreak = doc["streak"] | static_cast<uint16_t>(0);
  booksFinished = doc["booksFinished"] | static_cast<uint8_t>(0);
  pagesToday = doc["pagesToday"] | static_cast<uint16_t>(0);
  tendsToday = doc["tendsToday"] | static_cast<uint8_t>(0);
  watersToday = doc["watersToday"] | static_cast<uint8_t>(0);
  maxSessionPages = doc["maxSessionPages"] | static_cast<uint8_t>(0);
  nightPages = doc["nightPages"] | static_cast<uint8_t>(0);
  questClaims = doc["questClaims"] | static_cast<uint8_t>(0);
  questRewardClaims = doc["questRewardClaims"] | static_cast<uint8_t>(0);
  careCredits = std::min<uint8_t>(MAX_CARE_CREDITS, doc["careCredits"] | static_cast<uint8_t>(0));
  questDay = doc["questDay"] | 0;

  plots = {};
  int plotIndex = 0;
  for (const JsonObjectConst obj : doc["plots"].as<JsonArrayConst>()) {
    if (plotIndex >= PLOT_COUNT) break;
    Plot& plot = plots[plotIndex++];
    plot.cropId = obj["crop"] | static_cast<uint8_t>(0);
    if (plot.cropId > CROP_COUNT) plot.cropId = 0;
    plot.stage = obj["stage"] | static_cast<uint8_t>(0);
    if (plot.stage > 4) plot.stage = 4;
    plot.withered = obj["withered"] | false;
    plot.plantedFarmDay = obj["plantedDay"] | 0;
    plot.pagesRead = obj["pagesRead"] | static_cast<uint16_t>(0);
    plot.careScoreTotal = obj["careTotal"] | static_cast<uint16_t>(0);
    plot.careSamples = obj["careSamples"] | static_cast<uint16_t>(0);
    plot.moisture = std::min<uint8_t>(MAX_VITAL, obj["moisture"] | static_cast<uint8_t>(80));
    plot.sunlight = std::min<uint8_t>(MAX_VITAL, obj["sunlight"] | static_cast<uint8_t>(80));
    plot.health = std::min<uint8_t>(MAX_VITAL, obj["health"] | static_cast<uint8_t>(100));
    plot.nutrients = std::min<uint8_t>(MAX_VITAL, obj["nutrients"] | static_cast<uint8_t>(80));
    const uint8_t branch = obj["branch"] | static_cast<uint8_t>(0);
    plot.branch = static_cast<CropBranch>(branch > 2 ? 0 : branch);
  }

  harvested = {};
  int cropIndex = 0;
  for (const JsonVariantConst value : doc["harvested"].as<JsonArrayConst>()) {
    if (cropIndex >= CROP_COUNT) break;
    if (value.is<JsonArrayConst>()) {
      int branch = 0;
      for (const uint16_t count : value.as<JsonArrayConst>()) {
        if (branch >= 3) break;
        harvested[cropIndex][branch++] = count;
      }
    } else {
      harvested[cropIndex][0] = value.as<uint16_t>();
    }
    cropIndex++;
  }
  sold = {};
  cropIndex = 0;
  for (const JsonArrayConst cropArray : doc["sold"].as<JsonArrayConst>()) {
    if (cropIndex >= CROP_COUNT) break;
    int branch = 0;
    for (const uint16_t count : cropArray) {
      if (branch >= 3) break;
      sold[cropIndex][branch++] = count;
    }
    cropIndex++;
  }
  purchased = {};
  cropIndex = 0;
  for (const uint16_t count : doc["purchased"].as<JsonArrayConst>()) {
    if (cropIndex >= CROP_COUNT) break;
    purchased[cropIndex++] = count;
  }
  return true;
}

void FarmState::advanceDays(const int32_t days) {
  if (days <= 0) return;
  const uint8_t oldSeason = season();
  farmDay += days;
  const uint8_t newSeason = season();

  for (int i = 0; i < ownedPlotCount; ++i) {
    Plot& plot = plots[i];
    if (plot.cropId == 0 || plot.withered) continue;
    const CropDefinition& definition = crop(plot.cropId);
    if (oldSeason != newSeason && definition.season != newSeason && !definition.survivesNextSeason) {
      plot.withered = true;
      continue;
    }
    uint8_t moistureDecay = static_cast<uint8_t>(std::min<int32_t>(100, days * 15));
    uint8_t sunlightDecay = static_cast<uint8_t>(std::min<int32_t>(100, days * 8));
    uint8_t nutrientDecay = static_cast<uint8_t>(std::min<int32_t>(100, days * 5));
    if (ownsUpgrade(1)) moistureDecay /= 2;
    if (ownsUpgrade(0)) sunlightDecay /= 2;
    plot.moisture = clampSub(plot.moisture, moistureDecay);
    plot.sunlight = clampSub(plot.sunlight, sunlightDecay);
    plot.nutrients = clampSub(plot.nutrients, nutrientDecay);
    if (ownsUpgrade(2)) plot.nutrients = clampAdd(plot.nutrients, static_cast<uint8_t>(std::min<int32_t>(100, days * 12)));
    if (plot.moisture < 20 || plot.nutrients < 20) {
      uint8_t healthDecay = static_cast<uint8_t>(std::min<int32_t>(100, days * 12));
      if (ownsUpgrade(3)) healthDecay /= 2;
      plot.health = clampSub(plot.health, healthDecay);
    }
    const int32_t age = farmDay - plot.plantedFarmDay;
    plot.stage = static_cast<uint8_t>(std::min<int32_t>(4, 1 + age * 4 / definition.growDays));
    updateBranch(plot);
    discover(plot);
  }
}

bool FarmState::refreshForToday() {
  if (!SETTINGS.farmingEnabled) return false;
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t dayOfMonth = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.getUtcDateTime(year, month, dayOfMonth, hour, minute)) return false;
  const int32_t localDay = companion::localDayNumber(year, month, dayOfMonth, hour, minute,
                                                      signedUtcOffsetQuarterHours());
  resetQuestsIfNeeded(localDay);
  if (lastLocalDay == 0) {
    lastLocalDay = localDay;
    return true;
  }
  if (localDay <= lastLocalDay) return false;
  advanceDays(localDay - lastLocalDay);
  lastLocalDay = localDay;
  return true;
}

bool FarmState::buySeasonalSeedAndPlant() {
  const uint8_t cropId = static_cast<uint8_t>(season() * 2 + (nextSeasonCrop % 2) + 1);
  if (!buySeedAndPlant(cropId)) return false;
  nextSeasonCrop = static_cast<uint8_t>((nextSeasonCrop + 1) % 2);
  return true;
}

bool FarmState::buySeedAndPlant(const uint8_t cropId) {
  if (cropId == 0 || cropId > CROP_COUNT) return false;
  const uint8_t cropIndex = static_cast<uint8_t>(cropId - 1);
  const CropDefinition& definition = CROPS[cropIndex];
  if (definition.season != season()) return false;
  Plot* target = nullptr;
  for (int i = 0; i < ownedPlotCount; ++i) {
    Plot& plot = plots[i];
    if (plot.cropId == 0 || plot.withered) {
      target = &plot;
      break;
    }
  }
  if (!target) return false;

  if (coins < definition.seedPrice) return false;
  coins -= definition.seedPrice;
  purchased[cropIndex] = static_cast<uint16_t>(std::min<uint32_t>(UINT16_MAX, purchased[cropIndex] + 1));
  *target = Plot{};
  target->cropId = cropId;
  target->stage = 1;
  target->plantedFarmDay = farmDay;
  discover(*target);
  return true;
}

uint16_t FarmState::nextPlotPrice() const {
  return ownedPlotCount < PLOT_COUNT ? PLOT_PRICES[ownedPlotCount - 1] : 0;
}

bool FarmState::buyNextPlot() {
  const uint16_t price = nextPlotPrice();
  if (price == 0 || coins < price) return false;
  coins -= price;
  ownedPlotCount++;
  return true;
}

bool FarmState::harvestAll() {
  bool changed = false;
  for (int i = 0; i < ownedPlotCount; ++i) {
    Plot& plot = plots[i];
    if (plot.cropId == 0 || plot.withered || plot.stage < 4) continue;
    harvested[plot.cropId - 1][static_cast<uint8_t>(plot.branch)]++;
    plot = {};
    changed = true;
  }
  return changed;
}

bool FarmState::sellAll() {
  bool changed = false;
  for (int i = 0; i < CROP_COUNT; ++i) {
    for (int branch = 0; branch < 3; ++branch) {
      if (harvested[i][branch] == 0) continue;
      uint16_t value = CROPS[i].sellPrice;
      if (branch == static_cast<int>(CropBranch::Scholar)) value = static_cast<uint16_t>(value * 5 / 4);
      if (branch == static_cast<int>(CropBranch::Wild)) value = static_cast<uint16_t>(value * 4 / 5);
      coins = static_cast<uint16_t>(
          std::min<uint32_t>(UINT16_MAX, coins + static_cast<uint32_t>(harvested[i][branch]) * value));
      sold[i][branch] = static_cast<uint16_t>(
          std::min<uint32_t>(UINT16_MAX, sold[i][branch] + harvested[i][branch]));
      harvested[i][branch] = 0;
      changed = true;
    }
  }
  return changed;
}

bool FarmState::careForPlot(const CareAction action, const uint8_t plotIndex) {
  if (plotIndex >= ownedPlotCount || plots[plotIndex].cropId == 0 || plots[plotIndex].withered) return false;
  Plot* target = &plots[plotIndex];
  const bool enhanced = careCredits > 0;
  switch (action) {
    case CareAction::Water:
      if (!enhanced && waterStock == 0) return false;
      if (!enhanced) waterStock--;
      watersToday = std::min<uint8_t>(3, watersToday + 1);
      target->moisture = clampAdd(target->moisture, enhanced ? 50 : 35);
      break;
    case CareAction::Shade:
      target->sunlight = clampAdd(target->sunlight, enhanced ? 40 : (ownsUpgrade(4) ? 30 : 20));
      break;
    case CareAction::Weed: target->health = clampAdd(target->health, enhanced ? 35 : 25); break;
    case CareAction::Fertilize:
      if (!enhanced && fertilizerStock == 0) return false;
      if (!enhanced) fertilizerStock--;
      target->nutrients = clampAdd(target->nutrients, enhanced ? 50 : 35);
      break;
    case CareAction::Tend:
      target->health = clampAdd(target->health, enhanced ? 15 : 10);
      target->sunlight = clampAdd(target->sunlight, enhanced ? 10 : 5);
      tendsToday = std::min<uint8_t>(5, tendsToday + 1);
      break;
  }
  if (enhanced) careCredits--;
  const uint16_t average = static_cast<uint16_t>(target->moisture + target->sunlight + target->health + target->nutrients) / 4;
  target->careScoreTotal = static_cast<uint16_t>(std::min<uint32_t>(UINT16_MAX, target->careScoreTotal + average));
  target->careSamples++;
  updateBranch(*target);
  awardCompletedQuests();
  return true;
}

bool FarmState::refillWater() {
  if (waterStock == 3) return false;
  waterStock = 3;
  return true;
}

bool FarmState::restockFertilizer() {
  if (fertilizerStock == 3 || coins < 30) return false;
  coins -= 30;
  fertilizerStock = 3;
  return true;
}

bool FarmState::buyUpgrade(const uint8_t upgrade) {
  static constexpr std::array<uint16_t, 5> PRICES = {250, 400, 500, 650, 300};
  if (upgrade >= PRICES.size() || ownsUpgrade(upgrade) || coins < PRICES[upgrade]) return false;
  coins -= PRICES[upgrade];
  upgrades |= static_cast<uint8_t>(1U << upgrade);
  return true;
}

void FarmState::onPageTurn() {
  if (!SETTINGS.farmingEnabled) return;
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t dayOfMonth = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  bool nightReading = false;
  if (halClock.getUtcDateTime(year, month, dayOfMonth, hour, minute)) {
    resetQuestsIfNeeded(
        companion::localDayNumber(year, month, dayOfMonth, hour, minute, signedUtcOffsetQuarterHours()));
    const int localMinutes = static_cast<int>(hour) * 60 + minute + signedUtcOffsetQuarterHours() * 15;
    const int localHour = ((localMinutes / 60) % 24 + 24) % 24;
    nightReading = localHour >= 23 || localHour < 6;
  }
  pagesToday = std::min<uint16_t>(UINT16_MAX, pagesToday + 1);
  if (nightReading) nightPages = std::min<uint8_t>(10, nightPages + 1);
  for (int i = 0; i < ownedPlotCount; ++i) {
    Plot& plot = plots[i];
    if (plot.cropId == 0 || plot.withered) continue;
    plot.pagesRead++;
    if (plot.pagesRead % 10 == 0) plot.moisture = clampAdd(plot.moisture, 10);
    updateBranch(plot);
  }
  awardCompletedQuests();
}

void FarmState::onBookFinished() {
  if (!SETTINGS.farmingEnabled) return;
  booksFinished = std::min<uint8_t>(UINT8_MAX, booksFinished + 1);
}

void FarmState::finishReadingSession(const uint16_t pages) {
  if (!SETTINGS.farmingEnabled) return;
  maxSessionPages = std::max<uint8_t>(maxSessionPages, static_cast<uint8_t>(std::min<uint16_t>(UINT8_MAX, pages)));
  awardCompletedQuests();
}

void FarmState::resetQuestsIfNeeded(const int32_t localDay) {
  if (questDay == localDay) return;
  if (questDay == 0) {
    questDay = localDay;
    return;
  }
  if (pagesToday > 0)
    currentStreak++;
  else
    currentStreak = 0;
  questDay = localDay;
  pagesToday = 0;
  tendsToday = 0;
  watersToday = 0;
  maxSessionPages = 0;
  nightPages = 0;
  questClaims = 0;
  questRewardClaims = 0;
}

QuestProgress FarmState::quest(const uint8_t index) const {
  if (index >= QUEST_COUNT) return {};
  const uint16_t values[QUEST_COUNT] = {pagesToday, tendsToday, watersToday, maxSessionPages, nightPages,
                                        currentStreak};
  return {values[index], QUEST_GOALS[index], QUEST_REWARDS[index], (questClaims & (1U << index)) != 0,
          (questRewardClaims & (1U << index)) != 0};
}

uint8_t FarmState::claimableQuestCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < QUEST_COUNT; ++i) {
    const uint8_t mask = static_cast<uint8_t>(1U << i);
    if ((questClaims & mask) != 0 && (questRewardClaims & mask) == 0) count++;
  }
  return count;
}

bool FarmState::plotNeedsCare(const uint8_t plotIndex) const {
  if (plotIndex >= ownedPlotCount) return false;
  const Plot& plot = plots[plotIndex];
  if (plot.cropId == 0 || plot.withered) return false;
  return plot.moisture < 50 || plot.sunlight < 50 || plot.nutrients < 50 || plot.health < 75;
}

bool FarmState::hasPlotNeedingCare() const {
  for (uint8_t i = 0; i < ownedPlotCount; ++i) {
    if (plotNeedsCare(i)) return true;
  }
  return false;
}

bool FarmState::hasFarmNotification() const {
  return SETTINGS.farmingEnabled && (hasClaimableQuest() || hasPlotNeedingCare());
}

bool FarmState::claimQuest(const uint8_t index) {
  if (index >= QUEST_COUNT || careCredits >= MAX_CARE_CREDITS) return false;
  const uint8_t mask = static_cast<uint8_t>(1U << index);
  if ((questClaims & mask) == 0 || (questRewardClaims & mask) != 0) return false;
  questRewardClaims |= mask;
  careCredits++;
  return true;
}

void FarmState::awardCompletedQuests() {
  for (uint8_t i = 0; i < QUEST_COUNT; ++i) {
    const QuestProgress progress = quest(i);
    if (progress.progress < progress.goal || progress.claimed) continue;
    questClaims |= static_cast<uint8_t>(1U << i);
  }
}

void FarmState::updateBranch(Plot& plot) {
  if (plot.stage < 3) return;
  const uint16_t averageCare = plot.careSamples == 0 ? 50 : plot.careScoreTotal / plot.careSamples;
  const uint16_t scholarPages = plot.stage >= 4 ? 120 : 60;
  if (currentStreak >= 7 && booksFinished > 0 && plot.pagesRead >= scholarPages && averageCare >= 70)
    plot.branch = CropBranch::Scholar;
  else if (currentStreak < 3 && plot.pagesRead < scholarPages / 2 && averageCare < 50)
    plot.branch = CropBranch::Wild;
  else
    plot.branch = CropBranch::Default;
}

void FarmState::discover(const Plot& plot) {
  if (plot.cropId == 0 || plot.stage == 0) return;
  const uint8_t cropFamily = static_cast<uint8_t>(plot.cropId - 1);
  const uint8_t visibleStage = static_cast<uint8_t>(std::min<int>(3, plot.stage - 1));
  herbariumStages |= static_cast<uint32_t>(1UL << (cropFamily * 4 + visibleStage));
}

uint16_t FarmState::harvestedCount(const uint8_t cropId, const CropBranch branch) const {
  if (cropId == 0 || cropId > CROP_COUNT) return 0;
  return harvested[cropId - 1][static_cast<uint8_t>(branch)];
}

uint16_t FarmState::soldCount(const uint8_t cropId, const CropBranch branch) const {
  if (cropId == 0 || cropId > CROP_COUNT) return 0;
  return sold[cropId - 1][static_cast<uint8_t>(branch)];
}

uint16_t FarmState::seedsPurchased(const uint8_t cropId) const {
  if (cropId == 0 || cropId > CROP_COUNT) return 0;
  return purchased[cropId - 1];
}

uint16_t FarmState::branchSellPrice(const uint8_t cropId, const CropBranch branch) const {
  if (cropId == 0 || cropId > CROP_COUNT) return 0;
  uint16_t value = crop(cropId).sellPrice;
  if (branch == CropBranch::Scholar) value = static_cast<uint16_t>(value * 5 / 4);
  if (branch == CropBranch::Wild) value = static_cast<uint16_t>(value * 4 / 5);
  return value;
}

}  // namespace farm
