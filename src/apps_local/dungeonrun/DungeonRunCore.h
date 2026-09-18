#pragma once

#include <cstdint>

namespace dungeonrun {

constexpr int kRoomCount = 16;
constexpr int kDirectionCount = 4;
constexpr uint8_t kNoRoom = 0xFF;

enum class Direction : uint8_t { North, East, South, West };
enum class Kind : uint8_t { Start, Enemy, Fountain, Key, Smith, Ladder, Trap, Puzzle, Shrine, Treasure, Boss };
enum class Result : uint8_t {
  None,
  Miss,
  Hit,
  Blocked,
  Hurt,
  Victory,
  Death,
  Escaped,
  Bought,
  Healed,
  KeyFound,
  StaleWater,
  TrapFailed,
  TrapEscaped,
  PuzzleWrong,
  PuzzleSolved,
  Blessed,
  TreasureFound
};

struct Room {
  const char* name;
  Kind kind;
  uint8_t level;
  uint8_t x;
  uint8_t y;
  uint8_t exits[kDirectionCount];
  uint8_t enemyHp;
  uint8_t loot;
  bool locked;
};

extern const Room kRooms[kRoomCount];

struct Save {
  uint8_t version = 2;
  uint8_t room = 0;
  uint8_t hp = 10;
  uint8_t coins = 0;
  uint8_t damage = 1;
  uint8_t keys = 0;
  uint8_t deaths = 0;
  uint8_t flags = 0;
  uint16_t visited = 1;
  uint16_t cleared = 0;
  uint8_t enemyHp[kRoomCount] = {};
  uint32_t rng = 1;
};

class Game {
 public:
  void start(uint32_t seed);
  bool restore(const Save& state);
  Save save() const;

  int room() const { return state.room; }
  int hp() const { return state.hp; }
  int coins() const { return state.coins; }
  int damage() const { return state.damage; }
  int keys() const { return state.keys; }
  int deaths() const { return state.deaths; }
  bool escaped() const { return (state.flags & EscapedFlag) != 0; }
  bool hasShield() const { return (state.flags & ShieldFlag) != 0; }
  bool keyFound() const { return (state.flags & KeyFlag) != 0; }
  bool fountainUsed() const { return kRooms[state.room].kind == Kind::Fountain && cleared(state.room); }
  bool trapped() const { return kRooms[state.room].kind == Kind::Trap && !cleared(state.room); }
  bool enemyAlive() const;
  int enemyHealth() const;
  bool visited(int index) const;
  bool cleared(int index) const;
  bool canMove(Direction direction) const;
  bool canTravel(int index) const;
  int adjacent(Direction direction) const;

  Result move(Direction direction);
  Result travel(int index);
  Result act();
  Result solvePuzzle(uint8_t answer);
  Result roll();

 private:
  enum : uint8_t { ShieldFlag = 1, ShieldReadyFlag = 2, EscapedFlag = 4, FountainFlag = 8, KeyFlag = 16 };
  uint8_t die();
  Result enter(int index);
  void handleDeath();

  Save state;
};

}  // namespace dungeonrun
