#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>

#include "TerminalHackCore.h"

namespace terminalhack {

class DailyStats : public PersistableStore<DailyStats> {
  DailyStats() = default;
  friend class PersistableStore<DailyStats>;

 public:
  static const char* getFilePath() { return "/.crosspoint/terminal-hack.json"; }

  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool completed(int32_t day) const { return lastPlayedDay == day; }
  bool record(int32_t day, bool won);
  int16_t recordLevelResult(Difficulty difficulty, bool won, uint16_t xpAmount);
  void selectDifficulty(Difficulty difficulty) { selected = difficulty; }
  void saveRun(const Game& game, bool daily, uint8_t page);
  void clearRun();
  bool restoreRun(Game& game, bool& daily, uint8_t& page) const;

  uint16_t currentStreak() const { return streak; }
  uint16_t bestStreak() const { return best; }
  uint16_t unlockedLevel() const { return level; }
  uint32_t experiencePoints() const { return experience; }
  uint16_t playerRank() const { return rankForXp(experience); }
  uint16_t regularWinCount() const { return regularWins; }
  uint16_t winsFor(Difficulty difficulty) const { return difficultyWins[static_cast<uint8_t>(difficulty)]; }
  Difficulty selectedDifficulty() const { return selected; }

 private:
  int32_t lastPlayedDay = 0;
  int32_t lastWonDay = 0;
  uint16_t streak = 0;
  uint16_t best = 0;
  uint16_t level = 1;
  uint32_t experience = 0;
  uint16_t regularWins = 0;
  uint16_t difficultyWins[kDifficultyCount] = {};
  Difficulty selected = Difficulty::Novice;
  Game savedGame{};
  bool runSaved = false;
  bool savedDaily = false;
  uint8_t savedPage = 0;
};

}  // namespace terminalhack

#define TERMINAL_HACK_STATS terminalhack::DailyStats::getInstance()
