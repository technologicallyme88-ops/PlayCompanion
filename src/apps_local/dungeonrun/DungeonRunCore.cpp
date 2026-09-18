#include "DungeonRunCore.h"

#include <cstring>

namespace dungeonrun {

const Room kRooms[kRoomCount] = {
    {"BROKEN CELL", Kind::Start, 0, 0, 2, {1, 12, 0xFF, 0xFF}, 0, 0, false},
    {"RAT TUNNEL", Kind::Enemy, 0, 0, 1, {3, 2, 0, 0xFF}, 3, 3, false},
    {"DRY WELL", Kind::Fountain, 0, 1, 1, {4, 0xFF, 0xFF, 1}, 0, 0, false},
    {"IRON HALL", Kind::Enemy, 0, 0, 0, {0xFF, 4, 1, 0xFF}, 5, 5, false},
    {"KEY VAULT", Kind::Key, 0, 1, 0, {0xFF, 5, 2, 3}, 0, 0, false},
    {"SEALED STAIR", Kind::Ladder, 0, 2, 0, {0xFF, 0xFF, 13, 4}, 0, 0, true},
    {"UPPER LANDING", Kind::Ladder, 1, 2, 2, {8, 0xFF, 0xFF, 0xFF}, 0, 0, false},
    {"FORGE", Kind::Smith, 1, 1, 1, {9, 8, 0xFF, 14}, 0, 0, false},
    {"BONE GALLERY", Kind::Enemy, 1, 2, 1, {10, 0xFF, 6, 7}, 7, 7, false},
    {"MOON CISTERN", Kind::Fountain, 1, 1, 0, {0xFF, 10, 7, 15}, 0, 0, false},
    {"WARDEN GATE", Kind::Enemy, 1, 2, 0, {0xFF, 11, 8, 9}, 9, 9, false},
    {"THE LAST DOOR", Kind::Boss, 1, 3, 0, {0xFF, 0xFF, 0xFF, 10}, 12, 20, false},
    {"SPIKE PIT", Kind::Trap, 0, 1, 2, {2, 13, 0xFF, 0}, 0, 0, false},
    {"RUNE LOCK", Kind::Puzzle, 0, 2, 2, {5, 0xFF, 0xFF, 12}, 0, 6, false},
    {"FORGOTTEN SHRINE", Kind::Shrine, 1, 0, 1, {15, 7, 0xFF, 0xFF}, 0, 0, false},
    {"HIDDEN TREASURY", Kind::Treasure, 1, 0, 0, {0xFF, 9, 14, 0xFF}, 0, 8, false},
};

void Game::start(const uint32_t seed) {
  state = Save{};
  state.rng = seed == 0 ? 1 : seed;
  for (int i = 0; i < kRoomCount; ++i) state.enemyHp[i] = kRooms[i].enemyHp;
}

bool Game::restore(const Save& candidate) {
  if (candidate.version != 2 || candidate.room >= kRoomCount || candidate.hp == 0 || candidate.hp > 10 ||
      candidate.damage < 1 || candidate.damage > 3 || candidate.keys > 2 || (candidate.visited & 1u) == 0) {
    return false;
  }
  for (int i = 0; i < kRoomCount; ++i) {
    if (candidate.enemyHp[i] > kRooms[i].enemyHp) return false;
  }
  state = candidate;
  if (state.rng == 0) state.rng = 1;
  return true;
}

Save Game::save() const { return state; }

bool Game::visited(const int index) const {
  return index >= 0 && index < kRoomCount && (state.visited & (1u << index)) != 0;
}

bool Game::cleared(const int index) const {
  return index >= 0 && index < kRoomCount && (state.cleared & (1u << index)) != 0;
}

bool Game::enemyAlive() const { return kRooms[state.room].enemyHp != 0 && state.enemyHp[state.room] != 0; }
int Game::enemyHealth() const { return state.enemyHp[state.room]; }

int Game::adjacent(const Direction direction) const { return kRooms[state.room].exits[static_cast<int>(direction)]; }

bool Game::canMove(const Direction direction) const {
  if (enemyAlive() || trapped()) return false;
  const int next = adjacent(direction);
  if (next == kNoRoom) return false;
  return !kRooms[next].locked || state.keys > 0 || visited(next);
}

bool Game::canTravel(const int index) const { return visited(index) && !enemyAlive() && !trapped(); }

Result Game::enter(const int index) {
  if (index < 0 || index >= kRoomCount) return Result::None;
  if (kRooms[index].locked && !visited(index)) {
    if (state.keys == 0) return Result::None;
    --state.keys;
  }
  state.room = static_cast<uint8_t>(index);
  state.visited = static_cast<uint16_t>(state.visited | (1u << index));
  if (kRooms[index].kind == Kind::Ladder && index == 5) {
    state.room = 6;
    state.visited = static_cast<uint16_t>(state.visited | (1u << 6));
  }
  state.flags = static_cast<uint8_t>((state.flags & ~ShieldReadyFlag) | (hasShield() ? ShieldReadyFlag : 0));
  return Result::None;
}

Result Game::move(const Direction direction) {
  if (!canMove(direction)) return Result::None;
  return enter(adjacent(direction));
}

Result Game::travel(const int index) {
  if (!canTravel(index)) return Result::None;
  return enter(index);
}

uint8_t Game::die() {
  state.rng ^= state.rng << 13;
  state.rng ^= state.rng >> 17;
  state.rng ^= state.rng << 5;
  return static_cast<uint8_t>(state.rng % 6u + 1u);
}

void Game::handleDeath() {
  ++state.deaths;
  state.hp = 10;
  state.coins = state.coins > 5 ? static_cast<uint8_t>(state.coins - 5) : 0;
  state.room = 0;
  state.flags = static_cast<uint8_t>(state.flags & ~ShieldReadyFlag);
}

Result Game::roll() {
  if (!enemyAlive()) return Result::None;
  const uint8_t value = die();
  if (value == 1) return Result::Miss;
  if (value <= 3) {
    if ((state.flags & ShieldReadyFlag) != 0) {
      state.flags = static_cast<uint8_t>(state.flags & ~ShieldReadyFlag);
      return Result::Blocked;
    }
    if (state.hp <= 2) {
      handleDeath();
      return Result::Death;
    }
    state.hp = static_cast<uint8_t>(state.hp - 2);
    return Result::Hurt;
  }
  const uint8_t dealt = state.damage;
  uint8_t& hp = state.enemyHp[state.room];
  hp = hp > dealt ? static_cast<uint8_t>(hp - dealt) : 0;
  if (hp != 0) return Result::Hit;
  state.cleared = static_cast<uint16_t>(state.cleared | (1u << state.room));
  state.coins = static_cast<uint8_t>(state.coins + kRooms[state.room].loot);
  if (kRooms[state.room].kind == Kind::Boss) {
    state.flags = static_cast<uint8_t>(state.flags | EscapedFlag);
    return Result::Escaped;
  }
  return Result::Victory;
}

Result Game::act() {
  const Room& room = kRooms[state.room];
  if (enemyAlive()) return roll();
  if (room.kind == Kind::Trap && !cleared(state.room)) {
    const uint8_t value = die();
    if (value >= 4) {
      state.cleared = static_cast<uint16_t>(state.cleared | (1u << state.room));
      return Result::TrapEscaped;
    }
    if (state.hp <= 1) {
      handleDeath();
      return Result::Death;
    }
    --state.hp;
    return Result::TrapFailed;
  }
  if (room.kind == Kind::Key && (state.flags & KeyFlag) == 0) {
    state.flags = static_cast<uint8_t>(state.flags | KeyFlag);
    ++state.keys;
    return Result::KeyFound;
  }
  if (room.kind == Kind::Fountain && !cleared(state.room) && state.hp < 10) {
    state.cleared = static_cast<uint16_t>(state.cleared | (1u << state.room));
    state.hp = 10;
    return Result::Healed;
  }
  if (room.kind == Kind::Fountain && cleared(state.room)) return Result::StaleWater;
  if (room.kind == Kind::Smith) {
    if (state.damage < 3) {
      const uint8_t cost = static_cast<uint8_t>(state.damage * 5);
      if (state.coins < cost) return Result::None;
      state.coins = static_cast<uint8_t>(state.coins - cost);
      ++state.damage;
      return Result::Bought;
    }
    if (!hasShield() && state.coins >= 8) {
      state.coins = static_cast<uint8_t>(state.coins - 8);
      state.flags = static_cast<uint8_t>(state.flags | ShieldFlag | ShieldReadyFlag);
      return Result::Bought;
    }
  }
  if (room.kind == Kind::Shrine && !cleared(state.room)) {
    state.cleared = static_cast<uint16_t>(state.cleared | (1u << state.room));
    state.hp = static_cast<uint8_t>(state.hp > 6 ? 10 : state.hp + 4);
    return Result::Blessed;
  }
  if (room.kind == Kind::Treasure && !cleared(state.room)) {
    state.cleared = static_cast<uint16_t>(state.cleared | (1u << state.room));
    state.coins = static_cast<uint8_t>(state.coins + room.loot);
    return Result::TreasureFound;
  }
  return Result::None;
}

Result Game::solvePuzzle(const uint8_t answer) {
  const Room& room = kRooms[state.room];
  if (room.kind != Kind::Puzzle || cleared(state.room)) return Result::None;
  if (answer != 4) return Result::PuzzleWrong;
  state.cleared = static_cast<uint16_t>(state.cleared | (1u << state.room));
  state.coins = static_cast<uint8_t>(state.coins + room.loot);
  return Result::PuzzleSolved;
}

}  // namespace dungeonrun
