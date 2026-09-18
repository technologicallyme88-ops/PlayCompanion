#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "DungeonRunCore.h"

class DungeonRunActivity final : public Activity {
 public:
  DungeonRunActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Dungeon Run", renderer, mappedInput) {}

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Menu, Room, Map, HowTo, Won };

  void startNew();
  void route(int action, int value);
  void saveState() const;
  void clearSave();
  bool loadState();
  void touchSave();
  void flushSave();

  dungeonrun::Game game;
  View view = View::Menu;
  toybox::Interactions interactions;
  bool interactionsReady = false;
  bool hasSave = false;
  bool flashOnNextPaint = false;
  uint8_t unsavedActions = 0;
  const char* message = nullptr;
};
