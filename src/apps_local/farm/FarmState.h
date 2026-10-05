#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>

namespace farm {

constexpr int PLOT_COUNT = 4;
constexpr int CROP_COUNT = 8;
constexpr int QUEST_COUNT = 6;

enum class CropBranch : uint8_t { Default = 0, Scholar = 1, Wild = 2 };
enum class CareAction : uint8_t { Water = 0, Shade = 1, Weed = 2, Fertilize = 3, Tend = 4 };
enum class WeatherEffect : uint8_t { Clear = 0, Cloudy = 1, Rain = 2, Snow = 3 };

struct QuestProgress {
  uint16_t progress;
  uint16_t goal;
  uint16_t reward;
  bool complete;
  bool claimed;
};

struct Plot {
  uint8_t cropId = 0;
  uint8_t stage = 0;
  bool withered = false;
  int32_t plantedFarmDay = 0;
  uint16_t pagesRead = 0;
  uint16_t careScoreTotal = 0;
  uint16_t careSamples = 0;
  uint8_t moisture = 80;
  uint8_t sunlight = 80;
  uint8_t health = 100;
  uint8_t nutrients = 80;
  CropBranch branch = CropBranch::Default;
};

struct CropDefinition {
  uint8_t season;
  uint8_t growDays;
  uint8_t seedPrice;
  uint8_t sellPrice;
  bool survivesNextSeason;
};

class FarmState : public PersistableStore<FarmState> {
  FarmState();
  friend class PersistableStore<FarmState>;

 public:
  static const char* getFilePath() { return "/.crosspoint/farm.json"; }

  bool initializeFromFile();
  bool saveToFile() const;
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool refreshForToday();
  bool weatherCheckDue(int32_t utcMinute) const;
  bool applyWeather(WeatherEffect effect, int32_t utcMinute);
  WeatherEffect weatherEffect() const { return currentWeather; }
  bool buySeasonalSeedAndPlant();
  bool buySeedAndPlant(uint8_t cropId);
  bool buyNextPlot();
  bool harvestAll();
  bool sellAll();
  bool careForPlot(CareAction action, uint8_t plotIndex = 0);
  bool claimQuest(uint8_t index);
  bool refillWater();
  bool restockFertilizer();
  bool buyUpgrade(uint8_t upgrade);
  void onPageTurn();
  void onBookFinished();
  void finishReadingSession(uint16_t pages);

  uint8_t season() const { return static_cast<uint8_t>((farmDay / 28) % 4); }
  int32_t day() const { return farmDay; }
  uint16_t coinBalance() const { return coins; }
  uint8_t ownedPlots() const { return ownedPlotCount; }
  uint16_t nextPlotPrice() const;
  uint8_t waterCharges() const { return waterStock; }
  uint8_t fertilizerCharges() const { return fertilizerStock; }
  uint8_t careCreditCount() const { return careCredits; }
  uint8_t claimableQuestCount() const;
  bool hasClaimableQuest() const { return claimableQuestCount() > 0; }
  bool plotNeedsCare(uint8_t plotIndex) const;
  bool hasPlotNeedingCare() const;
  bool hasFarmNotification() const;
  uint16_t readingStreak() const { return currentStreak; }
  uint8_t finishedBooks() const { return booksFinished; }
  uint32_t discoveredStages() const { return herbariumStages; }
  uint16_t harvestedCount(uint8_t cropId, CropBranch branch) const;
  uint16_t soldCount(uint8_t cropId, CropBranch branch) const;
  uint16_t seedsPurchased(uint8_t cropId) const;
  uint16_t branchSellPrice(uint8_t cropId, CropBranch branch) const;
  QuestProgress quest(uint8_t index) const;
  bool ownsUpgrade(uint8_t upgrade) const { return upgrade < 5 && (upgrades & (1U << upgrade)); }
  const std::array<Plot, PLOT_COUNT>& getPlots() const { return plots; }

  static const CropDefinition& crop(uint8_t cropId);

 private:
  std::array<Plot, PLOT_COUNT> plots{};
  std::array<std::array<uint16_t, 3>, CROP_COUNT> harvested{};
  std::array<std::array<uint16_t, 3>, CROP_COUNT> sold{};
  std::array<uint16_t, CROP_COUNT> purchased{};
  uint16_t coins = 20;
  int32_t farmDay = 0;
  int32_t lastLocalDay = 0;
  uint8_t nextSeasonCrop = 0;
  uint8_t ownedPlotCount = 1;
  uint8_t waterStock = 3;
  uint8_t fertilizerStock = 3;
  uint8_t upgrades = 0;
  uint32_t herbariumStages = 0;
  uint16_t currentStreak = 0;
  uint8_t booksFinished = 0;
  uint16_t pagesToday = 0;
  uint8_t tendsToday = 0;
  uint8_t watersToday = 0;
  uint8_t maxSessionPages = 0;
  uint16_t nightPages = 0;
  uint8_t questClaims = 0;
  uint8_t questRewardClaims = 0;
  uint8_t careCredits = 0;
  int32_t questDay = 0;
  int32_t lastVitalMinute = 0;
  int32_t lastWeatherMinute = 0;
  WeatherEffect currentWeather = WeatherEffect::Clear;
  bool persistenceReady = false;

  void advanceDays(int32_t days);
  void advanceHours(int32_t hours);
  void resetQuestsIfNeeded(int32_t localDay);
  void updateBranch(Plot& plot);
  void discover(const Plot& plot);
  void awardCompletedQuests();
};

}  // namespace farm

#define FARM_STATE farm::FarmState::getInstance()
