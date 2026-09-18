#include "DungeonRunActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <cstring>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/GameButtonPointer.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"
#include "DungeonRunScreens.h"

namespace {

namespace fui = freeink::ui;
namespace ui = dungeonrunui;
constexpr char kSavePath[] = "/.crosspoint/dungeon-run.sav";

const char* resultText(const dungeonrun::Result result) {
  switch (result) {
    case dungeonrun::Result::Miss: return "MISS. THE DARK LAUGHS.";
    case dungeonrun::Result::Hit: return "HIT. IT STAGGERS.";
    case dungeonrun::Result::Blocked: return "THE SHIELD TAKES THE BLOW.";
    case dungeonrun::Result::Hurt: return "THE MONSTER HITS FOR 2 HP.";
    case dungeonrun::Result::Victory: return "VICTORY. TAKE THE COINS.";
    case dungeonrun::Result::Death: return "DEFEATED. YOUR FOE STAYS WOUNDED.";
    case dungeonrun::Result::Bought: return "THE FORGE MAKES YOU STRONGER.";
    case dungeonrun::Result::Healed: return "THE WATER RESTORES 10 HP.";
    case dungeonrun::Result::KeyFound: return "YOU TAKE THE IRON KEY.";
    case dungeonrun::Result::StaleWater: return "THE WATER DOES NOT FEEL AS FRESH.";
    case dungeonrun::Result::TrapFailed: return "THE SPIKES BITE. LOSE 1 HP.";
    case dungeonrun::Result::TrapEscaped: return "THE TRAP BREAKS. YOU ARE FREE.";
    case dungeonrun::Result::PuzzleWrong: return "THE RUNES REJECT THAT ANSWER.";
    case dungeonrun::Result::PuzzleSolved: return "THE RUNES OPEN. TAKE 6 COINS.";
    case dungeonrun::Result::Blessed: return "THE SHRINE RESTORES UP TO 4 HP.";
    case dungeonrun::Result::TreasureFound: return "THE CHEST HOLDS 8 COINS.";
    default: return nullptr;
  }
}

int bitCount(uint16_t value) {
  int count = 0;
  while (value != 0) {
    value = static_cast<uint16_t>(value & (value - 1));
    ++count;
  }
  return count;
}

}  // namespace

std::unique_ptr<Activity> DungeonRunActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<DungeonRunActivity>(renderer, mappedInput);
}

void DungeonRunActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  hasSave = loadState();
  if (hasSave && game.escaped()) clearSave();
  if (!hasSave) game.start(toybox::seed());
  view = View::Menu;
  requestUpdate();
}

void DungeonRunActivity::onExit() {
  flushSave();
  Activity::onExit();
}

void DungeonRunActivity::startNew() {
  game.start(toybox::seed());
  hasSave = true;
  message = nullptr;
  view = View::Room;
  flashOnNextPaint = true;
  saveState();
  requestUpdate();
}

void DungeonRunActivity::route(const int action, const int value) {
  switch (action) {
    case ui::ActionNew:
      startNew();
      return;
    case ui::ActionResume:
      view = game.escaped() ? View::Won : View::Room;
      break;
    case ui::ActionHowTo:
      view = View::HowTo;
      break;
    case ui::ActionMap:
      view = View::Map;
      break;
    case ui::ActionMove:
      message = resultText(game.move(static_cast<dungeonrun::Direction>(value)));
      touchSave();
      break;
    case ui::ActionTravel:
      game.travel(value);
      message = nullptr;
      view = View::Room;
      touchSave();
      break;
    case ui::ActionAct: {
      const bool fightingWarden = dungeonrun::kRooms[game.room()].kind == dungeonrun::Kind::Boss && game.enemyAlive();
      const dungeonrun::Result result = game.act();
      message = result == dungeonrun::Result::Death && fightingWarden
                    ? "WARDEN WOUNDED. RETURN BY MAP."
                    : resultText(result);
      if (result == dungeonrun::Result::Escaped) {
        view = View::Won;
        flashOnNextPaint = true;
        clearSave();
      }
      if (result == dungeonrun::Result::Death) {
        unsavedActions = 1;
        flushSave();
      } else if (result != dungeonrun::Result::Escaped) {
        touchSave();
      }
      break;
    }
    case ui::ActionPuzzle:
      message = resultText(game.solvePuzzle(static_cast<uint8_t>(value)));
      touchSave();
      break;
    case ui::ActionBack:
      if (view == View::Won) {
        shelf::leave(renderer, mappedInput);
        return;
      }
      if (view == View::HowTo) {
        view = View::Menu;
      } else if (view == View::Map) {
        view = hasSave ? View::Room : View::Menu;
      } else {
        view = View::Menu;
      }
      break;
    default:
      return;
  }
  requestUpdate();
}

void DungeonRunActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view == View::Menu) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (view == View::HowTo) {
      view = View::Menu;
    } else if (view == View::Map) {
      view = hasSave ? View::Room : View::Menu;
    } else {
      view = View::Menu;
    }
    requestUpdate();
    return;
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
  if (!tapped || !interactionsReady) return;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent hit = interactions.route(input);
  route(static_cast<int>(hit.action), static_cast<int>(hit.value));
}

void DungeonRunActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, toybox::toyboxFaces());
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions);
  toybox::Screen screen(frame);

  switch (view) {
    case View::Menu: {
      const dungeonrun::Save state = game.save();
      ui::MenuModel model;
      model.hasSave = hasSave;
      model.deaths = game.deaths();
      model.visited = bitCount(state.visited);
      ui::buildMenu(screen, model);
      break;
    }
    case View::Room:
      ui::buildRoom(screen, ui::RoomModel{&game, message});
      break;
    case View::Map:
      ui::buildMap(screen, game);
      break;
    case View::HowTo:
      ui::buildHowTo(screen);
      break;
    case View::Won:
      ui::buildWon(screen, game);
      break;
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Dungeon Run");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  gameinput::drawPointer(renderer, mappedInput);
  renderer.displayBuffer(flashOnNextPaint ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  flashOnNextPaint = false;
}

void DungeonRunActivity::saveState() const {
  HalFile file;
  if (!Storage.openFileForWrite("DRUN", kSavePath, file)) return;
  const dungeonrun::Save state = game.save();
  file.write(reinterpret_cast<const uint8_t*>(&state), sizeof(state));
  file.flush();
}

void DungeonRunActivity::clearSave() {
  Storage.remove(kSavePath);
  hasSave = false;
  unsavedActions = 0;
}

bool DungeonRunActivity::loadState() {
  if (!Storage.exists(kSavePath)) return false;
  HalFile file;
  if (!Storage.openFileForRead("DRUN", kSavePath, file)) return false;
  dungeonrun::Save state;
  if (file.read(reinterpret_cast<uint8_t*>(&state), sizeof(state)) != sizeof(state)) return false;
  return game.restore(state);
}

void DungeonRunActivity::touchSave() {
  if (++unsavedActions >= 4) flushSave();
}

void DungeonRunActivity::flushSave() {
  if (!hasSave || unsavedActions == 0) return;
  saveState();
  unsavedActions = 0;
}
