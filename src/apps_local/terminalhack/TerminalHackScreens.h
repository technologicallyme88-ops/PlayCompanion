#pragma once

#include "../ui/ToyboxScreen.h"
#include "TerminalHackCore.h"

namespace terminalhackui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionWord = 1,
  ActionHack = 2,
  ActionAgain = 3,
  ActionDifficulty = 4,
  ActionDaily = 5,
};

struct Model {
  const terminalhack::Game* game = nullptr;
  const char* notice = nullptr;
  bool menu = false;
  bool daily = false;
  bool dailyAvailable = false;
  bool dailyComplete = false;
  uint16_t level = 1;
  uint16_t streak = 0;
  uint16_t bestStreak = 0;
  uint32_t experience = 0;
  uint16_t rank = 1;
  uint8_t gridPage = 0;
  terminalhack::Difficulty selectedDifficulty = terminalhack::Difficulty::Novice;
  uint16_t difficultyWins[terminalhack::kDifficultyCount] = {};
};

void buildBoard(toybox::Screen& screen, const Model& model);

}  // namespace terminalhackui
