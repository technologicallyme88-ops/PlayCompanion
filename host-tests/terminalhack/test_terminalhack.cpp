#include <cstdio>

#include "../../src/apps_local/terminalhack/TerminalHackCore.h"

namespace th = terminalhack;

int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

int main() {
  th::Game game;
  th::start(game, 17);
  CHECK(game.result == th::Result::Playing);
  CHECK(game.attempts == th::kMaxAttempts);
  CHECK(th::likeness("CONTROL", "CONSOLE") == 3);
  CHECK(th::xpForWin(th::Difficulty::Novice, 5) == 45);
  CHECK(th::xpForWin(th::Difficulty::Elite, 3) == 75);
  CHECK(th::xpForLoss(th::Difficulty::Novice) == 55);
  CHECK(th::xpForLoss(th::Difficulty::Elite) == 85);
  CHECK(th::rankForXp(0) == 1);
  CHECK(th::rankForXp(199) == 2);

  for (int bank = 0; bank < 4; ++bank) {
    th::Game words;
    words.bank = static_cast<uint8_t>(bank);
    bool differentInitial = false;
    for (int i = 0; i < th::kWordCount; ++i) {
      CHECK(std::strlen(th::word(words, i)) == th::kWordLength);
      if (i > 0 && th::word(words, i)[0] != th::word(words, 0)[0]) differentInitial = true;
    }
    CHECK(differentInitial);
  }

  const int password = game.password;
  CHECK(th::guess(game, password));
  CHECK(game.result == th::Result::Won);

  th::start(game, 23);
  int wrong = game.password == 0 ? 1 : 0;
  CHECK(th::guess(game, wrong));
  CHECK(!th::guess(game, wrong));
  for (int i = 0; i < th::kWordCount && game.result == th::Result::Playing; ++i) {
    if (i != game.password && i != wrong) CHECK(th::guess(game, i));
  }
  CHECK(game.result == th::Result::Locked);

  th::start(game, 31);
  const int resetHack = static_cast<int>(game.seed % th::kHackCount);
  const int guessed = game.password == 0 ? 1 : 0;
  CHECK(th::guess(game, guessed));
  game.attempts = 1;
  CHECK(th::useHack(game, resetHack) == th::HackEffect::AttemptsRestored);
  CHECK(game.attempts == game.attemptLimit);
  CHECK(th::useHack(game, resetHack) == th::HackEffect::None);

  for (int hack = 0; hack < th::kHackCount; ++hack) {
    if (hack != resetHack) th::useHack(game, hack);
  }
  CHECK(game.active[guessed]);
  CHECK(game.guessed[guessed]);

  th::start(game, 37);
  CHECK(th::useHack(game, 1) != th::HackEffect::None);
  CHECK(th::useHack(game, 0) != th::HackEffect::None);
  CHECK(game.hackUsed[0]);
  CHECK(game.hackUsed[1]);

  th::start(game, 41, 21);
  CHECK(game.level == 21);
  CHECK(game.difficulty == th::Difficulty::Elite);
  CHECK(game.attemptLimit == 3);
  CHECK(game.candidateCount == 20);

  th::start(game, 43, th::Difficulty::Hard, 7);
  CHECK(game.level == 7);
  CHECK(game.difficulty == th::Difficulty::Hard);
  CHECK(game.attemptLimit == 4);
  CHECK(game.candidateCount == 14);

  CHECK(th::civilDay(2026, 9, 30) + 1 == th::civilDay(2026, 10, 1));
  CHECK(th::civilDay(2024, 2, 28) + 1 == th::civilDay(2024, 2, 29));
  CHECK(th::dailySeed(th::civilDay(2026, 9, 30)) == th::dailySeed(th::civilDay(2026, 9, 30)));
  CHECK(th::dailySeed(th::civilDay(2026, 9, 30)) != th::dailySeed(th::civilDay(2026, 10, 1)));

  std::printf("terminalhack: %s\n", failures == 0 ? "ok" : "failed");
  return failures == 0 ? 0 : 1;
}
