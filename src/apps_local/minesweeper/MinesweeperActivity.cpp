#include "MinesweeperActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstdlib>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "../ui/GameButtonPointer.h"
#include "MinesweeperScreens.h"

namespace ms = minesweeper;

namespace {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
constexpr char kHistoryPath[] = "/.crosspoint/minesweeper.sav";
#endif

// How long a finger must rest on a cell to plant a flag. Long enough that a
// deliberate dig never trips it, short enough that it does not feel broken on a
// panel whose own refresh is half a second.
constexpr unsigned long kFlagHoldMs = 400;
}  // namespace

std::unique_ptr<Activity> MinesweeperActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MinesweeperActivity>(renderer, mappedInput);
}

void MinesweeperActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  screen = ms::Screen::Menu;
  menuSelected = mappedInput.hasTouch() ? -1 : 0;
  boardColumn = 0;
  boardRow = 0;
  boardModeFocused = false;
  resultSelected = 0;
  loadHistory();
  requestUpdate();
}

void MinesweeperActivity::loadHistory() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.exists(kHistoryPath)) return;
  char buffer[512] = {};
  if (Storage.readFileToBuffer(kHistoryPath, buffer, sizeof(buffer)) == 0) return;

  // Parsed into locals and committed only if the whole line is good: a
  // truncated file leaves an empty menu rather than half a minefield. Losing
  // this costs an ornament, so it fails quietly.
  int values[2 + ms::kCells] = {};
  char* cursor = buffer;
  for (int i = 0; i < static_cast<int>(sizeof(values) / sizeof(values[0])); ++i) {
    char* next = nullptr;
    const long value = strtol(cursor, &next, 10);
    if (next == cursor) return;
    values[i] = static_cast<int>(value);
    cursor = next;
  }

  int at = 0;
  wins = values[at++];
  losses = values[at++];
  ms::Game board{};
  for (int column = 0; column < ms::kColumns; ++column) {
    for (int row = 0; row < ms::kRows; ++row) board.cell[column][row] = static_cast<uint8_t>(values[at++]);
  }
  lastBoard = board;
  hasHistory = true;
#endif
}

void MinesweeperActivity::recordResult() {
  if (resultRecorded) return;
  resultRecorded = true;

  lastBoard = game;
  hasHistory = true;
  if (game.status == ms::Status::Won) ++wins;
  if (game.status == ms::Status::Lost) ++losses;

#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  char line[512];
  int used = snprintf(line, sizeof(line), "%d %d", wins, losses);
  for (int column = 0; column < ms::kColumns && used > 0 && used < static_cast<int>(sizeof(line)); ++column) {
    for (int row = 0; row < ms::kRows; ++row) {
      used += snprintf(line + used, sizeof(line) - used, " %d", lastBoard.cell[column][row]);
    }
  }
  if (used <= 0 || used >= static_cast<int>(sizeof(line))) {
    LOG_ERR("MINE", "History line did not fit %d bytes", static_cast<int>(sizeof(line)));
    return;
  }
  snprintf(line + used, sizeof(line) - used, "\n");
  Storage.writeFile(kHistoryPath, String(line));
#endif
}

void MinesweeperActivity::goTo(const ms::Screen next) {
  screen = next;
  requestUpdate();
}

void MinesweeperActivity::beginGame() {
  // millis() is the only entropy that differs between two boots, and it enters
  // here and nowhere deeper: the core takes a seed and never reaches for a
  // clock, which is what keeps a board replayable from its seed.
  ms::start(game, static_cast<uint32_t>(millis()) * 2654435761u + 1u);
  holdColumn = -1;
  holdRow = -1;
  holdFired = false;
  flagMode = false;
  resultRecorded = false;
  boardColumn = 0;
  boardRow = 0;
  boardModeFocused = false;
  goTo(ms::Screen::Board);
}

void MinesweeperActivity::loop() {
  namespace fui = freeink::ui;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (ms::leavesApp(screen)) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    goTo(ms::back(screen));
    return;
  }

  // Native button navigation for X3/X4. The generic software pointer is not
  // used: focus belongs to the control itself.
  if (!mappedInput.hasTouch()) {
    if (screen == ms::Screen::Menu) {
      if (mappedInput.wasReleased(MappedInputManager::Button::NavNext) ||
          mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
        menuSelected = 1 - (menuSelected < 0 ? 0 : menuSelected);
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (menuSelected <= 0) beginGame();
        else {
          howToPage = 0;
          goTo(ms::Screen::HowTo);
        }
        return;
      }
    }

    if (screen == ms::Screen::HowTo) {
      if (mappedInput.wasReleased(MappedInputManager::Button::NavNext) ||
          mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (howToPage + 1 < mineui::howToPages()) ++howToPage;
        else goTo(ms::Screen::Menu);
        requestUpdate();
        return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
        if (howToPage > 0) --howToPage;
        else goTo(ms::Screen::Menu);
        requestUpdate();
        return;
      }
    }

    if (screen == ms::Screen::Board) {
      if (ms::over(game)) {
        if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
          goTo(ms::Screen::Result);
          return;
        }
      } else {
        if (mappedInput.wasReleased(MappedInputManager::Button::ScreenLeft)) {
          if (boardModeFocused) boardModeFocused = false;
          else if (boardColumn > 0) --boardColumn;
          requestUpdate();
          return;
        }
        if (mappedInput.wasReleased(MappedInputManager::Button::ScreenRight)) {
          if (boardModeFocused) boardModeFocused = false;
          else if (boardColumn + 1 < ms::kColumns) ++boardColumn;
          requestUpdate();
          return;
        }
        if (mappedInput.wasReleased(MappedInputManager::Button::ScreenUp)) {
          if (boardModeFocused) {
            boardModeFocused = false;
            boardRow = ms::kRows - 1;
          } else if (boardRow > 0) {
            --boardRow;
          }
          requestUpdate();
          return;
        }
        if (mappedInput.wasReleased(MappedInputManager::Button::ScreenDown)) {
          if (!boardModeFocused) {
            if (boardRow + 1 < ms::kRows) ++boardRow;
            else boardModeFocused = true;
          }
          requestUpdate();
          return;
        }
        if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
          if (boardModeFocused) {
            flagMode = !flagMode;
          } else {
            const bool changed = flagMode ? ms::toggleFlag(game, boardColumn, boardRow)
                                          : ms::reveal(game, boardColumn, boardRow);
            if (changed && ms::over(game)) recordResult();
          }
          requestUpdate();
          return;
        }
      }
    }
  }

  // The result screen is a real two-item button menu on X3/X4. Do not make
  // the user steer the generic pointer onto these large buttons: both front
  // and side navigation keys move focus, and Confirm activates it.
  if (screen == ms::Screen::Result && !mappedInput.hasTouch()) {
    if (mappedInput.wasReleased(MappedInputManager::Button::NavNext) ||
        mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
      resultSelected = 1 - resultSelected;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (resultSelected == 0) beginGame();
      else goTo(ms::Screen::Menu);
      return;
    }
  }

  // The rules decide when it is over; the record is written the moment they
  // do (latched by resultRecorded). The screen does NOT change: the settled
  // board stays, mines bared, wearing the verdict as its capsule. Navigating
  // away here was what made every ending anticlimactic -- the finished
  // minefield flashed for under a repaint and was gone.
  if (screen == ms::Screen::Board && ms::over(game)) recordResult();

  // Hold to flag, tap to dig.
  //
  // The grid is not in the interaction buffer -- eighty cells against a
  // twenty-four slot buffer -- so neither the tap nor the hold can go through
  // FreeInkUI's TouchHoldRouter, which routes against that buffer. Both are
  // done here against the same geometry that drew the pixels.
  //
  // swallowCurrentTouch() is the piece that makes a hold usable: it exists
  // precisely so a long press can fire while the finger is still down without
  // the ensuing lift also arriving as a tap. Without it, planting a flag would
  // immediately dig the cell it was planted on.
  if (screen == ms::Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    int hx = 0;
    int hy = 0;
    int column = 0;
    int row = 0;
    if (mappedInput.isScreenTouchHeld(hx, hy) && mineui::cellAt(device, hx, hy, column, row)) {
      if (column != holdColumn || row != holdRow) {
        // Moved to another cell: the hold starts again there, so sliding a
        // finger across the board never plants a flag you did not mean.
        holdColumn = column;
        holdRow = row;
        holdSinceMs = millis();
        holdFired = false;
        requestUpdate();
      } else if (!holdFired && millis() - holdSinceMs >= kFlagHoldMs) {
        // Still a flag, in both modes, rather than "whatever the mode is not".
        // The symmetry was tempting and is a trap: in FLAG mode it would make
        // a held finger dig, and digging is the move that can end the game.
        // A hold you did not mean should never cost more than an extra tap to
        // undo. In FLAG mode it simply agrees with the tap.
        holdFired = true;
        if (ms::toggleFlag(game, column, row)) requestUpdate();
        mappedInput.swallowCurrentTouch();
      }
      return;
    }
    if (holdColumn >= 0) {
      holdColumn = -1;
      holdRow = -1;
      requestUpdate();
    }
  }

  fui::InputSnapshot input{};
  int tapX = 0;
  int tapY = 0;
  bool tapped = mappedInput.wasScreenTapped(tapX, tapY);
  const gameinput::PointerResult pointer = gameinput::readPointer(mappedInput, renderer, tapX, tapY);
  if (pointer == gameinput::PointerResult::Moved) {
    requestUpdate();
    return;
  }
  if (pointer == gameinput::PointerResult::Tap) tapped = true;
  if (tapped) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  if (screen == ms::Screen::Board) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    int column = 0;
    int row = 0;
    if (mineui::cellAt(device, tapX, tapY, column, row)) {
      // The rules decide legality, not the screen and not here.
      const bool changed = flagMode ? ms::toggleFlag(game, column, row) : ms::reveal(game, column, row);
      if (changed) requestUpdate();
      return;
    }
  }

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case mineui::ActionMenuRow:
      switch (static_cast<mineui::MenuRow>(event.value)) {
        case mineui::MenuRow::Play:
          beginGame();
          return;
        case mineui::MenuRow::HowTo:
          howToPage = 0;
          goTo(ms::Screen::HowTo);
          return;
        case mineui::MenuRow::Count:
          return;
      }
      return;

    case mineui::ActionHowToNext:
      if (howToPage + 1 < mineui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(ms::Screen::Menu);
      return;

    case mineui::ActionAgain:
      beginGame();
      return;

    case mineui::ActionDone:
      goTo(ms::Screen::Menu);
      return;

    case mineui::ActionToggleMode:
      flagMode = !flagMode;
      requestUpdate();
      return;

    case mineui::ActionSeeResult:
      goTo(ms::Screen::Result);
      return;

    default:
      return;
  }
}

void MinesweeperActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  switch (screen) {
    case ms::Screen::Menu: {
      mineui::MenuModel model;
      model.selected = menuSelected;
      model.hasHistory = hasHistory;
      model.lastBoard = lastBoard;
      model.wins = wins;
      model.losses = losses;
      mineui::buildMenu(surface, model);
      break;
    }
    case ms::Screen::HowTo: {
      mineui::HowToModel model;
      model.page = howToPage;
      mineui::buildHowTo(surface, model);
      break;
    }
    case ms::Screen::Board: {
      mineui::BoardModel model;
      model.game = game;
      model.holdColumn = mappedInput.hasTouch() ? holdColumn : (boardModeFocused ? -1 : boardColumn);
      model.holdRow = mappedInput.hasTouch() ? holdRow : (boardModeFocused ? -1 : boardRow);
      model.showMines = ms::over(game);
      model.flagMode = flagMode;
      model.modeFocused = !mappedInput.hasTouch() && boardModeFocused;
      mineui::buildBoard(surface, model);
      break;
    }
    case ms::Screen::Result: {
      mineui::ResultModel model;
      model.won = game.status == ms::Status::Won;
      int revealed = 0;
      int flagsRight = 0;
      for (int column = 0; column < ms::kColumns; ++column) {
        for (int row = 0; row < ms::kRows; ++row) {
          const uint8_t cell = game.cell[column][row];
          if ((cell & ms::kRevealed) && !(cell & ms::kMine)) ++revealed;
          if ((cell & ms::kFlagged) && (cell & ms::kMine)) ++flagsRight;
        }
      }
      model.revealed = revealed;
      model.flagsRight = flagsRight;
      model.selected = mappedInput.hasTouch() ? -1 : resultSelected;
      mineui::buildResult(surface, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Minesweeper");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
