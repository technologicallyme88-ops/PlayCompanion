#pragma once

#include "../ui/ToyboxScreen.h"
#include "DungeonRunCore.h"

namespace dungeonrunui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionNew = 1,
  ActionResume,
  ActionHowTo,
  ActionMap,
  ActionAct,
  ActionMove,
  ActionTravel,
  ActionPuzzle,
  ActionBack,
};

struct MenuModel {
  bool hasSave = false;
  int deaths = 0;
  int visited = 0;
};

struct RoomModel {
  const dungeonrun::Game* game = nullptr;
  const char* message = nullptr;
};

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildRoom(toybox::Screen& screen, const RoomModel& model);
void buildMap(toybox::Screen& screen, const dungeonrun::Game& game);
void buildHowTo(toybox::Screen& screen);
void buildWon(toybox::Screen& screen, const dungeonrun::Game& game);

}  // namespace dungeonrunui
