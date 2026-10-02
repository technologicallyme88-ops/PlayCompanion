#include "TerminalHackStats.h"

#include <algorithm>

namespace terminalhack {

void DailyStats::toJson(JsonDocument& doc) const {
  doc["lastPlayedDay"] = lastPlayedDay;
  doc["lastWonDay"] = lastWonDay;
  doc["streak"] = streak;
  doc["best"] = best;
  doc["level"] = level;
  doc["experience"] = experience;
  doc["regularWins"] = regularWins;
  JsonArray wins = doc["difficultyWins"].to<JsonArray>();
  for (int i = 0; i < kDifficultyCount; ++i) wins.add(difficultyWins[i]);
  doc["selectedDifficulty"] = static_cast<uint8_t>(selected);
  doc["runSaved"] = runSaved;
  if (runSaved) {
    doc["runSeed"] = savedGame.seed;
    doc["runLevel"] = savedGame.level;
    doc["runAttempts"] = savedGame.attempts;
    doc["runResult"] = static_cast<uint8_t>(savedGame.result);
    doc["runDifficulty"] = static_cast<uint8_t>(savedGame.difficulty);
    doc["runXpDelta"] = savedGame.xpDelta;
    doc["runDaily"] = savedDaily;
    doc["runPage"] = savedPage;

    uint32_t activeMask = 0;
    uint32_t guessedMask = 0;
    uint8_t hackMask = 0;
    for (int i = 0; i < kWordCount; ++i) {
      if (savedGame.active[i]) activeMask |= 1UL << i;
      if (savedGame.guessed[i]) guessedMask |= 1UL << i;
    }
    for (int i = 0; i < kHackCount; ++i) {
      if (savedGame.hackUsed[i]) hackMask |= static_cast<uint8_t>(1U << i);
    }
    doc["runActive"] = activeMask;
    doc["runGuessed"] = guessedMask;
    doc["runHacks"] = hackMask;
    JsonArray guesses = doc["runGuessWords"].to<JsonArray>();
    for (int i = 0; i < savedGame.guessCount; ++i) guesses.add(savedGame.guesses[i].word);
  }
}

bool DailyStats::fromJson(const JsonVariantConst doc) {
  lastPlayedDay = doc["lastPlayedDay"] | 0;
  lastWonDay = doc["lastWonDay"] | 0;
  streak = doc["streak"] | static_cast<uint16_t>(0);
  best = std::max<uint16_t>(streak, doc["best"] | static_cast<uint16_t>(0));
  level = std::max<uint16_t>(1, doc["level"] | static_cast<uint16_t>(1));
  experience = doc["experience"] | static_cast<uint32_t>(0);
  regularWins = doc["regularWins"] | static_cast<uint16_t>(0);
  JsonArrayConst wins = doc["difficultyWins"];
  if (wins.size() == kDifficultyCount) {
    regularWins = 0;
    for (int i = 0; i < kDifficultyCount; ++i) {
      difficultyWins[i] = wins[i] | static_cast<uint16_t>(0);
      regularWins = UINT16_MAX - regularWins < difficultyWins[i]
                        ? UINT16_MAX
                        : static_cast<uint16_t>(regularWins + difficultyWins[i]);
    }
  } else {
    difficultyWins[0] = regularWins;
  }
  const uint8_t selectedValue = doc["selectedDifficulty"] | static_cast<uint8_t>(0);
  selected = selectedValue < kDifficultyCount ? static_cast<Difficulty>(selectedValue) : Difficulty::Novice;
  runSaved = doc["runSaved"] | false;
  if (runSaved) {
    const uint32_t seed = doc["runSeed"] | static_cast<uint32_t>(0);
    const uint16_t runLevel = doc["runLevel"] | static_cast<uint16_t>(0);
    const uint8_t attempts = doc["runAttempts"] | static_cast<uint8_t>(0);
    const uint8_t result = doc["runResult"] | static_cast<uint8_t>(0);
    if (seed == 0 || runLevel == 0 || result > static_cast<uint8_t>(Result::Locked)) {
      runSaved = false;
      return true;
    }
    const uint8_t runDifficulty = doc["runDifficulty"] | static_cast<uint8_t>(kDifficultyCount);
    if (runDifficulty < kDifficultyCount) {
      start(savedGame, seed, static_cast<Difficulty>(runDifficulty), runLevel);
    } else {
      start(savedGame, seed, runLevel);
    }
    if (attempts > savedGame.attemptLimit) {
      runSaved = false;
      return true;
    }
    savedGame.attempts = attempts;
    savedGame.result = static_cast<Result>(result);
    savedGame.xpDelta = doc["runXpDelta"] | static_cast<int16_t>(0);
    savedDaily = doc["runDaily"] | false;
    savedPage = doc["runPage"] | static_cast<uint8_t>(0);
    const uint8_t pageCount = static_cast<uint8_t>((savedGame.candidateCount + 11) / 12);
    if (savedPage >= pageCount) savedPage = 0;

    const uint32_t activeMask = doc["runActive"] | static_cast<uint32_t>(0);
    const uint32_t guessedMask = doc["runGuessed"] | static_cast<uint32_t>(0);
    const uint8_t hackMask = doc["runHacks"] | static_cast<uint8_t>(0);
    for (int i = 0; i < kWordCount; ++i) {
      savedGame.active[i] = (activeMask & (1UL << i)) != 0;
      savedGame.guessed[i] = (guessedMask & (1UL << i)) != 0;
    }
    for (int i = 0; i < kHackCount; ++i) savedGame.hackUsed[i] = (hackMask & (1U << i)) != 0;

    JsonArrayConst guesses = doc["runGuessWords"];
    savedGame.guessCount = 0;
    const int guessCount = std::min(static_cast<int>(guesses.size()), kMaxAttempts);
    for (int i = 0; i < guessCount; ++i) {
      const int wordIndex = guesses[i] | -1;
      if (wordIndex < 0 || wordIndex >= kWordCount) {
        runSaved = false;
        break;
      }
      savedGame.guesses[savedGame.guessCount++] =
          Guess{static_cast<int8_t>(wordIndex), static_cast<uint8_t>(likeness(word(savedGame, wordIndex),
                                                                             word(savedGame, savedGame.password)))};
    }
  }
  return true;
}

bool DailyStats::record(const int32_t day, const bool won) {
  if (day <= 0 || completed(day)) return false;
  lastPlayedDay = day;
  if (won) {
    streak = lastWonDay == day - 1 ? static_cast<uint16_t>(streak + 1) : 1;
    lastWonDay = day;
    if (streak > best) best = streak;
  } else {
    streak = 0;
  }
  return true;
}

int16_t DailyStats::recordLevelResult(const Difficulty difficulty, const bool won, const uint16_t xpAmount) {
  const uint8_t index = static_cast<uint8_t>(difficulty);
  if (index >= kDifficultyCount) return 0;
  if (won) {
    experience = UINT32_MAX - experience < xpAmount ? UINT32_MAX : experience + xpAmount;
    if (regularWins < UINT16_MAX) ++regularWins;
    if (difficultyWins[index] < UINT16_MAX) ++difficultyWins[index];
    return static_cast<int16_t>(xpAmount);
  }
  const uint16_t applied = static_cast<uint16_t>(experience < xpAmount ? experience : xpAmount);
  experience -= applied;
  return -static_cast<int16_t>(applied);
}

void DailyStats::saveRun(const Game& game, const bool daily, const uint8_t page) {
  savedGame = game;
  savedDaily = daily;
  savedPage = page;
  runSaved = true;
}

void DailyStats::clearRun() { runSaved = false; }

bool DailyStats::restoreRun(Game& game, bool& daily, uint8_t& page) const {
  if (!runSaved) return false;
  game = savedGame;
  daily = savedDaily;
  page = savedPage;
  return true;
}

}  // namespace terminalhack
