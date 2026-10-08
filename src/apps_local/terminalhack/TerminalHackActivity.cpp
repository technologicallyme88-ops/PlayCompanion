#include "TerminalHackActivity.h"

#include <CompanionMood.h>
#include <HalClock.h>
#include <Logging.h>
#include <Memory.h>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "CrossPointSettings.h"
#include "TerminalHackScreens.h"
#include "TerminalHackStats.h"

std::unique_ptr<Activity> TerminalHackActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TerminalHackActivity>(renderer, mappedInput);
}

void TerminalHackActivity::startGame(const bool daily, const terminalhack::Difficulty difficulty) {
  dailyMode = daily;
  regularRewardRecorded = false;
  const uint32_t seed = daily ? terminalhack::dailySeed(dailyDay) : static_cast<uint32_t>(millis()) * 2654435761u + 1u;
  if (daily) {
    terminalhack::start(game, seed, 6);
  } else {
    selectedDifficulty = difficulty;
    TERMINAL_HACK_STATS.selectDifficulty(difficulty);
    level = static_cast<uint16_t>(TERMINAL_HACK_STATS.winsFor(difficulty) + 1);
    terminalhack::start(game, seed, difficulty, level);
  }
  gridPage = 0;
  notice = "SELECT PASSWORD";
  view = View::Board;
  persistRun();
  requestUpdate();
}

void TerminalHackActivity::persistRun() {
  TERMINAL_HACK_STATS.saveRun(game, dailyMode, gridPage);
  if (!TERMINAL_HACK_STATS.saveToFile()) LOG_ERR("TERMHACK", "Failed to save active run");
}

void TerminalHackActivity::clearRun() {
  TERMINAL_HACK_STATS.clearRun();
  if (!TERMINAL_HACK_STATS.saveToFile()) LOG_ERR("TERMHACK", "Failed to clear active run");
}

void TerminalHackActivity::finishRegular() {
  if (dailyMode || game.result == terminalhack::Result::Playing || regularRewardRecorded) return;
  regularRewardRecorded = true;
  const bool won = game.result == terminalhack::Result::Won;
  const uint16_t xp =
      won ? terminalhack::xpForWin(game.difficulty, game.attempts) : terminalhack::xpForLoss(game.difficulty);
  game.xpDelta = TERMINAL_HACK_STATS.recordLevelResult(game.difficulty, won, xp);
  level = static_cast<uint16_t>(TERMINAL_HACK_STATS.winsFor(game.difficulty) + 1);
}

void TerminalHackActivity::finishDaily() {
  if (!dailyMode || game.result == terminalhack::Result::Playing) return;
  TERMINAL_HACK_STATS.record(dailyDay, game.result == terminalhack::Result::Won);
}

bool TerminalHackActivity::refreshDailyDay() {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.getUtcDateTime(year, month, day, hour, minute)) return false;
  uint8_t biasedOffset = SETTINGS.clockUtcOffsetQ;
  if (biasedOffset > 104) biasedOffset = 104;
  dailyDay = companion::localDayNumber(year, month, day, hour, minute, static_cast<int32_t>(biasedOffset) - 48);
  return true;
}

void TerminalHackActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  TERMINAL_HACK_STATS.loadFromFile();
  level = TERMINAL_HACK_STATS.unlockedLevel();
  selectedDifficulty = TERMINAL_HACK_STATS.selectedDifficulty();
  refreshDailyDay();
  if (TERMINAL_HACK_STATS.restoreRun(game, dailyMode, gridPage)) {
    view = View::Board;
    notice = game.result == terminalhack::Result::Won      ? "EXACT MATCH"
             : game.result == terminalhack::Result::Locked ? "ENTRY DENIED"
             : game.guessCount > 0                         ? "ENTRY DENIED"
                                                           : "SELECT PASSWORD";
  } else {
    view = View::Menu;
  }
  requestUpdate();
}

void TerminalHackActivity::loop() {
  namespace fui = freeink::ui;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view == View::Board) {
      view = View::Menu;
      clearRun();
      requestUpdate();
      return;
    }
    shelf::leave(renderer, mappedInput);
    return;
  }

  if (view == View::Board && game.result == terminalhack::Result::Playing) {
    const uint8_t pageCount = static_cast<uint8_t>((game.candidateCount + 11) / 12);
    const auto swipe = mappedInput.wasSwipe();
    const bool pageDown =
        swipe == MappedInputManager::SwipeDir::Up || mappedInput.wasReleased(MappedInputManager::Button::Down);
    const bool pageUp =
        swipe == MappedInputManager::SwipeDir::Down || mappedInput.wasReleased(MappedInputManager::Button::Up);
    if (pageDown && gridPage + 1 < pageCount) {
      ++gridPage;
      persistRun();
      requestUpdate();
      return;
    }
    if (pageUp && gridPage > 0) {
      --gridPage;
      persistRun();
      requestUpdate();
      return;
    }
  }

  fui::InputSnapshot input{};
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(x);
    input.touchY = static_cast<int16_t>(y);
  }
  if (!mappedInput.hasTouch()) {
    const bool pagedBoard =
        view == View::Board && game.result == terminalhack::Result::Playing && game.candidateCount > 12;
    input.focusNext =
        mappedInput.wasReleased(pagedBoard ? MappedInputManager::Button::Right : MappedInputManager::Button::NavNext);
    input.focusPrev = mappedInput.wasReleased(pagedBoard ? MappedInputManager::Button::Left
                                                         : MappedInputManager::Button::NavPrevious);
    input.confirm = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  }
  if ((!input.touchReleased && !input.focusNext && !input.focusPrev && !input.confirm) || !interactionsReady) return;

  const int16_t previousFocus = interactions.focusedIndex();
  const fui::ActionEvent event = interactions.route(input);
  if (interactions.focusedIndex() != previousFocus) requestUpdate();
  if (event.action == terminalhackui::ActionWord) {
    if (terminalhack::guess(game, event.value)) {
      notice = game.result == terminalhack::Result::Won ? "EXACT MATCH" : "ENTRY DENIED";
      finishDaily();
      finishRegular();
      if (game.result != terminalhack::Result::Playing) interactions.setFocusedIndex(0);
      persistRun();
      requestUpdate();
    }
  } else if (event.action == terminalhackui::ActionHack) {
    const terminalhack::HackEffect effect = terminalhack::useHack(game, event.value);
    if (effect == terminalhack::HackEffect::DudRemoved) notice = "DUD REMOVED";
    if (effect == terminalhack::HackEffect::AttemptsRestored) notice = "ALLOWANCE RESET";
    if (effect != terminalhack::HackEffect::None) {
      persistRun();
      requestUpdate();
    }
  } else if (event.action == terminalhackui::ActionAgain) {
    if (dailyMode) {
      view = View::Menu;
      clearRun();
      requestUpdate();
    } else {
      startGame(false, game.difficulty);
    }
  } else if (event.action == terminalhackui::ActionDifficulty) {
    if (event.value >= 0 && event.value < terminalhack::kDifficultyCount) {
      startGame(false, static_cast<terminalhack::Difficulty>(event.value));
    }
  } else if (event.action == terminalhackui::ActionDaily) {
    const int32_t previousDay = dailyDay;
    if (refreshDailyDay() && !TERMINAL_HACK_STATS.completed(dailyDay)) {
      startGame(true);
    } else if (dailyDay != previousDay) {
      requestUpdate();
    }
  }
}

void TerminalHackActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, toybox::terminalFaces());
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen screen(frame);
  terminalhackui::Model model;
  model.game = &game;
  model.notice = notice;
  model.menu = view == View::Menu;
  model.daily = dailyMode;
  model.dailyAvailable = dailyDay > 0;
  model.dailyComplete = dailyDay > 0 && TERMINAL_HACK_STATS.completed(dailyDay);
  model.level = level;
  model.streak = TERMINAL_HACK_STATS.currentStreak();
  model.bestStreak = TERMINAL_HACK_STATS.bestStreak();
  model.experience = TERMINAL_HACK_STATS.experiencePoints();
  model.rank = TERMINAL_HACK_STATS.playerRank();
  model.gridPage = gridPage;
  model.selectedDifficulty = selectedDifficulty;
  for (int i = 0; i < terminalhack::kDifficultyCount; ++i) {
    model.difficultyWins[i] = TERMINAL_HACK_STATS.winsFor(static_cast<terminalhack::Difficulty>(i));
  }
  terminalhackui::buildBoard(screen, model);
  toybox::reportOverflow(interactions, "Terminal Hack");
  const bool seedButtonFocus =
      !mappedInput.hasTouch() && interactions.count() > 0 &&
      (interactions.focusedIndex() < 0 || interactions.focusedIndex() >= static_cast<int16_t>(interactions.count()));
  if (seedButtonFocus) interactions.setFocusedIndex(0);
  interactionsReady = true;
  renderer.displayBuffer();
  if (seedButtonFocus) requestUpdate();
}
