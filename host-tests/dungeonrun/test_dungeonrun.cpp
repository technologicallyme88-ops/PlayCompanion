#include <cstdio>

#include "../../src/apps_local/dungeonrun/DungeonRunCore.h"

namespace {
int checks = 0;
int failed = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failed; std::printf("FAIL %d: %s\n", __LINE__, #expr); } } while (0)
}

int main() {
  using namespace dungeonrun;
  Game game;
  game.start(7);
  CHECK(game.room() == 0);
  CHECK(game.hp() == 10);
  CHECK(game.visited(0));
  CHECK(game.canMove(Direction::North));
  game.move(Direction::North);
  CHECK(game.room() == 1);
  CHECK(game.enemyAlive());
  CHECK(!game.canMove(Direction::North));

  for (int i = 0; i < 100 && game.enemyAlive(); ++i) game.roll();
  CHECK(!game.enemyAlive() || game.room() == 0);

  Save bad = game.save();
  bad.hp = 99;
  CHECK(!game.restore(bad));

  Game restored;
  CHECK(restored.restore(game.save()));
  CHECK(restored.room() == game.room());
  CHECK(restored.hp() == game.hp());

  // Losing to the Warden is a retreat, not game over. Its wounds survive the
  // death and the explored room remains available through quick travel.
  Save warden = restored.save();
  warden.room = 11;
  warden.hp = 2;
  warden.visited = 0x0FFF;
  warden.enemyHp[11] = 5;
  bool died = false;
  for (uint32_t seed = 1; seed < 100 && !died; ++seed) {
    warden.rng = seed;
    CHECK(restored.restore(warden));
    died = restored.roll() == Result::Death;
  }
  CHECK(died);
  CHECK(restored.room() == 0);
  CHECK(restored.hp() == 10);
  CHECK(restored.canTravel(11));
  CHECK(restored.travel(11) == Result::None);
  CHECK(restored.enemyHealth() == 5);

  Save well = restored.save();
  well.room = 2;
  well.hp = 4;
  well.flags = 0;
  CHECK(restored.restore(well));
  CHECK(restored.act() == Result::Healed);
  CHECK(restored.hp() == 10);
  CHECK(restored.fountainUsed());
  CHECK(restored.act() == Result::StaleWater);
  CHECK(restored.hp() == 10);

  well.room = 9;
  well.hp = 3;
  well.cleared = static_cast<uint16_t>(1u << 2);
  CHECK(restored.restore(well));
  CHECK(!restored.fountainUsed());
  CHECK(restored.act() == Result::Healed);
  CHECK(restored.hp() == 10);
  CHECK(restored.fountainUsed());
  CHECK(restored.cleared(2));
  CHECK(restored.cleared(9));

  Save oldVersion = restored.save();
  oldVersion.version = 1;
  CHECK(!restored.restore(oldVersion));

  // Trap rooms hold the player until a successful roll. Failed attempts cost
  // one HP; the cleared trap remains harmless afterward.
  Save trap = restored.save();
  trap.room = 12;
  trap.hp = 10;
  trap.visited = static_cast<uint16_t>(trap.visited | (1u << 12));
  trap.cleared = static_cast<uint16_t>(trap.cleared & ~(1u << 12));
  bool trapFailed = false;
  for (uint32_t seed = 1; seed < 100 && !trapFailed; ++seed) {
    trap.rng = seed;
    CHECK(restored.restore(trap));
    trapFailed = restored.act() == Result::TrapFailed;
  }
  CHECK(trapFailed);
  CHECK(restored.hp() == 9);
  CHECK(restored.trapped());
  CHECK(!restored.canMove(Direction::West));
  CHECK(!restored.canTravel(0));

  bool trapEscaped = false;
  for (uint32_t seed = 1; seed < 100 && !trapEscaped; ++seed) {
    trap.rng = seed;
    CHECK(restored.restore(trap));
    trapEscaped = restored.act() == Result::TrapEscaped;
  }
  CHECK(trapEscaped);
  CHECK(!restored.trapped());
  CHECK(restored.canMove(Direction::West));

  Save puzzle = restored.save();
  puzzle.room = 13;
  puzzle.coins = 0;
  puzzle.visited = static_cast<uint16_t>(puzzle.visited | (1u << 13));
  puzzle.cleared = static_cast<uint16_t>(puzzle.cleared & ~(1u << 13));
  CHECK(restored.restore(puzzle));
  CHECK(restored.solvePuzzle(2) == Result::PuzzleWrong);
  CHECK(restored.coins() == 0);
  CHECK(restored.solvePuzzle(4) == Result::PuzzleSolved);
  CHECK(restored.coins() == 6);
  CHECK(restored.cleared(13));
  CHECK(restored.solvePuzzle(4) == Result::None);
  CHECK(restored.coins() == 6);

  Save shrine = restored.save();
  shrine.room = 14;
  shrine.hp = 3;
  shrine.visited = static_cast<uint16_t>(shrine.visited | (1u << 14));
  shrine.cleared = static_cast<uint16_t>(shrine.cleared & ~(1u << 14));
  CHECK(restored.restore(shrine));
  CHECK(restored.act() == Result::Blessed);
  CHECK(restored.hp() == 7);
  CHECK(restored.act() == Result::None);

  Save treasure = restored.save();
  treasure.room = 15;
  treasure.coins = 2;
  treasure.visited = static_cast<uint16_t>(treasure.visited | (1u << 15));
  treasure.cleared = static_cast<uint16_t>(treasure.cleared & ~(1u << 15));
  CHECK(restored.restore(treasure));
  CHECK(restored.act() == Result::TreasureFound);
  CHECK(restored.coins() == 10);
  CHECK(restored.act() == Result::None);
  CHECK(restored.coins() == 10);

  std::printf("%d checks, %d failed\n", checks, failed);
  return failed == 0 ? 0 : 1;
}
