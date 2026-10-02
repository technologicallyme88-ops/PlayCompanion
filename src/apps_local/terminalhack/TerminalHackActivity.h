#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "TerminalHackCore.h"

class TerminalHackActivity final : public Activity {
 public:
  TerminalHackActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Terminal Hack", renderer, mappedInput) {}

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Menu, Board };

  void startGame(bool daily, terminalhack::Difficulty difficulty = terminalhack::Difficulty::Novice);
  void finishDaily();
  void finishRegular();
  void persistRun();
  void clearRun();
  bool refreshDailyDay();

  terminalhack::Game game;
  uint16_t level = 1;
  terminalhack::Difficulty selectedDifficulty = terminalhack::Difficulty::Novice;
  uint8_t gridPage = 0;
  int32_t dailyDay = 0;
  View view = View::Menu;
  bool dailyMode = false;
  bool regularRewardRecorded = false;
  const char* notice = nullptr;
  toybox::Interactions interactions;
  bool interactionsReady = false;
};
