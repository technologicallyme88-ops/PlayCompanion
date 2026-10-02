#pragma once

#include <cstdint>
#include <cstring>

namespace terminalhack {

constexpr int kWordLength = 7;
constexpr int kWordCount = 20;
constexpr int kMaxAttempts = 5;
constexpr int kHackCount = 4;
constexpr int kDifficultyCount = 5;
constexpr uint16_t kXpPerRank = 100;

enum class Difficulty : uint8_t { Novice, Standard, Hard, Expert, Elite };

enum class Result : uint8_t { Playing, Won, Locked };
enum class HackEffect : uint8_t { DudRemoved, AttemptsRestored, None };

inline constexpr uint16_t xpForWin(Difficulty difficulty, uint8_t attemptsRemaining) {
  return static_cast<uint16_t>(20 + static_cast<uint8_t>(difficulty) * 10 + attemptsRemaining * 5);
}

inline constexpr uint8_t attemptLimitFor(Difficulty difficulty) {
  const uint8_t tier = static_cast<uint8_t>(difficulty);
  return tier == 0 ? 5 : (tier < 3 ? 4 : 3);
}

inline constexpr uint16_t xpForLoss(Difficulty difficulty) {
  return static_cast<uint16_t>(xpForWin(difficulty, attemptLimitFor(difficulty)) + 10);
}

inline constexpr uint16_t rankForXp(uint32_t experience) {
  return static_cast<uint16_t>(experience / kXpPerRank + 1);
}

struct Guess {
  int8_t word = -1;
  uint8_t likeness = 0;
};

struct Game {
  uint32_t seed = 1;
  uint16_t level = 1;
  Difficulty difficulty = Difficulty::Novice;
  uint8_t bank = 0;
  uint8_t password = 0;
  uint8_t candidateCount = 8;
  uint8_t attempts = kMaxAttempts;
  uint8_t attemptLimit = kMaxAttempts;
  bool active[kWordCount] = {};
  bool guessed[kWordCount] = {};
  bool hackUsed[kHackCount] = {};
  Guess guesses[kMaxAttempts] = {};
  uint8_t guessCount = 0;
  Result result = Result::Playing;
  int16_t xpDelta = 0;
};

inline const char* difficultyName(Difficulty difficulty) {
  static constexpr const char* kNames[] = {"NOVICE", "STANDARD", "HARD", "EXPERT", "ELITE"};
  return kNames[static_cast<uint8_t>(difficulty)];
}

inline uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

inline int32_t civilDay(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

inline uint32_t dailySeed(int32_t day) {
  uint32_t seed = static_cast<uint32_t>(day) ^ 0x5445524Du;
  return nextRandom(seed);
}

inline const char* word(const Game& game, int index) {
  static constexpr const char* kBanks[][kWordCount] = {
      {"CAPTURE", "RAPTURE", "PICTURE", "FIXTURE", "MIXTURE", "VENTURE", "DENTURE", "TEXTURE", "POSTURE", "PASTURE", "FEATURE", "TORTURE", "FAILURE", "CULTURE", "VULTURE", "FUTURES", "LECTURE", "GESTURE", "CLOSURE", "MEASURE"},
      {"BARRIER", "CARRIER", "EARLIER", "SCARIER", "TARDIER", "HARDIER", "WORDIER", "DIRTIER", "RUSTIER", "VARIOUS", "CURIOUS", "SERIOUS", "SOLDIER", "WARRIOR", "JUNIPER", "PREMIER", "VICTORY", "HUNTERS", "RANGERS", "SAILORS"},
      {"HACKING", "PACKING", "BACKING", "LACKING", "TACKING", "RACKING", "SACKING", "WAITING", "WRITING", "RUNNING", "SETTING", "GETTING", "BINDING", "FINDING", "MINDING", "WINDING", "LANDING", "PENDING", "SENDING", "RENTING"},
      {"NETWORK", "GATEWAY", "KEYCARD", "PAYLOAD", "LOCKOUT", "UPLINKS", "SIGNALS", "CIPHERS", "ARCHIVE", "MALWARE", "ROUTERS", "CONSOLE", "DEFENSE", "EXPLOIT", "SYSTEMS", "BYPASS", "PACKETS", "SERVERS", "KERNELS", "RUNTIME"},
  };
  if (index < 0 || index >= kWordCount) return "";
  return kBanks[game.bank % 4][index];
}

inline int likeness(const char* a, const char* b) {
  int same = 0;
  for (int i = 0; i < kWordLength; ++i) {
    if (a[i] == b[i]) ++same;
  }
  return same;
}

inline void start(Game& game, uint32_t seed, uint16_t level = 1) {
  game = Game{};
  game.seed = seed == 0 ? 1 : seed;
  game.level = level == 0 ? 1 : level;
  const uint8_t tier = static_cast<uint8_t>((game.level - 1) / 5);
  game.difficulty = static_cast<Difficulty>(tier > 4 ? 4 : tier);
  game.attemptLimit = tier == 0 ? 5 : (tier < 3 ? 4 : 3);
  game.attempts = game.attemptLimit;
  uint32_t random = game.seed;
  game.bank = static_cast<uint8_t>(nextRandom(random) % 4);
  game.candidateCount = static_cast<uint8_t>(8 + (tier > 4 ? 12 : tier * 3));
  game.password = static_cast<uint8_t>(nextRandom(random) % game.candidateCount);
  for (int i = 0; i < kWordCount; ++i) game.active[i] = i < game.candidateCount;
}

inline void start(Game& game, uint32_t seed, Difficulty difficulty, uint16_t level) {
  game = Game{};
  game.seed = seed == 0 ? 1 : seed;
  game.level = level == 0 ? 1 : level;
  game.difficulty = difficulty;
  const uint8_t tier = static_cast<uint8_t>(difficulty);
  game.attemptLimit = attemptLimitFor(difficulty);
  game.attempts = game.attemptLimit;
  uint32_t random = game.seed;
  game.bank = static_cast<uint8_t>(nextRandom(random) % 4);
  game.candidateCount = static_cast<uint8_t>(8 + tier * 3);
  game.password = static_cast<uint8_t>(nextRandom(random) % game.candidateCount);
  for (int i = 0; i < kWordCount; ++i) game.active[i] = i < game.candidateCount;
}

inline bool guess(Game& game, int index) {
  if (game.result != Result::Playing || index < 0 || index >= kWordCount || !game.active[index] ||
      game.guessed[index]) {
    return false;
  }
  game.guessed[index] = true;
  const int score = likeness(word(game, index), word(game, game.password));
  if (game.guessCount < kMaxAttempts) {
    game.guesses[game.guessCount++] = Guess{static_cast<int8_t>(index), static_cast<uint8_t>(score)};
  }
  if (index == game.password) {
    game.result = Result::Won;
    return true;
  }
  if (game.attempts > 0) --game.attempts;
  if (game.attempts == 0) game.result = Result::Locked;
  return true;
}

inline HackEffect useHack(Game& game, int index) {
  if (game.result != Result::Playing || index < 0 || index >= kHackCount || game.hackUsed[index]) {
    return HackEffect::None;
  }
  game.hackUsed[index] = true;
  if (index == static_cast<int>(game.seed % kHackCount)) {
    game.attempts = game.attemptLimit;
    return HackEffect::AttemptsRestored;
  }
  for (int offset = 1; offset < kWordCount; ++offset) {
    const int candidate = (game.password + offset + index * 3) % kWordCount;
    if (candidate != game.password && game.active[candidate] && !game.guessed[candidate]) {
      game.active[candidate] = false;
      return HackEffect::DudRemoved;
    }
  }
  return HackEffect::None;
}

}  // namespace terminalhack
