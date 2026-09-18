#include "DungeonRunScreens.h"

#include <cstdio>

namespace dungeonrunui {

namespace {

constexpr int kHeader = 76;
constexpr int kMargin = 24;
constexpr int kGap = 10;

void header(toybox::Screen& screen, const char* title) {
  fui::HeaderProps props;
  props.title = title;
  props.borderEdges = fui::EdgesNone;
  screen.header(props);
  screen.target().fill(fui::makeRect(0, kHeader + 4, screen.device().screen().width, toybox::kRule),
                       fui::Paint::solid(fui::Color::Black));
}

void button(toybox::Screen& screen, const fui::Rect& rect, const char* label, const fui::ActionId action,
            const int value = 0, const bool outline = false) {
  fui::ButtonProps props;
  props.label = label;
  props.action = action;
  props.value = value;
  if (outline) props.styles = toybox::rowStyles();
  screen.button(props, rect);
}

void text(toybox::Screen& screen, const fui::Rect& rect, const char* value, const fui::FontId font,
          const fui::TextAlign align = fui::TextAlign::Left, const uint8_t maxLines = 1) {
  fui::TextStyle style;
  style.font = font;
  style.align = align;
  style.maxLines = maxLines;
  screen.target().text(rect, value, style);
}

const char* roomCopy(const dungeonrun::Room& room, const dungeonrun::Game& game) {
  if (game.enemyAlive()) return room.kind == dungeonrun::Kind::Boss ? "THE WARDEN BARS THE LAST DOOR." : "SOMETHING MOVES IN THE DARK.";
  switch (room.kind) {
    case dungeonrun::Kind::Start: return "YOUR BROKEN CHAINS LIE IN THE DUST.";
    case dungeonrun::Kind::Fountain:
      return game.fountainUsed() ? "THE WATER HAS GONE STILL." : "COLD WATER RUNS BENEATH THE STONE.";
    case dungeonrun::Kind::Key:
      return game.keyFound() ? "THE CRACKED HOOK IS EMPTY." : "AN IRON KEY HANGS FROM A CRACKED HOOK.";
    case dungeonrun::Kind::Smith:
      if (game.damage() < 3) return "THE OLD FORGE CAN STRENGTHEN YOUR WEAPON.";
      if (!game.hasShield()) return "THE OLD FORGE CAN FIT YOU WITH A SHIELD.";
      return "THE FORGE HAS NOTHING MORE TO OFFER.";
    case dungeonrun::Kind::Ladder: return "THE STAIR CLIMBS INTO OLDER DARKNESS.";
    case dungeonrun::Kind::Trap:
      return game.cleared(game.room()) ? "THE SPIKES ARE JAMMED. THE WAY IS CLEAR." : "SPIKES CLOSE AROUND YOU. ROLL TO BREAK FREE.";
    case dungeonrun::Kind::Puzzle:
      return game.cleared(game.room()) ? "THE RUNES ARE DARK. YOU TOOK THEIR COINS." : "TWO BONES PLUS TWO BONES. CHOOSE THE TOTAL.";
    case dungeonrun::Kind::Shrine:
      return game.cleared(game.room()) ? "THE SHRINE'S BLESSING HAS FADED." : "A QUIET SHRINE OFFERS A SINGLE BLESSING.";
    case dungeonrun::Kind::Treasure:
      return game.cleared(game.room()) ? "THE TREASURE CHEST IS EMPTY." : "AN OLD CHEST WAITS BENEATH THE DUST.";
    case dungeonrun::Kind::Boss: return "DAYLIGHT CUTS THROUGH THE OPEN DOOR.";
    default: return "THE PASSAGE IS QUIET NOW.";
  }
}

}  // namespace

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  header(screen, "DUNGEON RUN");
  const fui::Rect bounds = screen.device().screen();

  const int heroY = 142;
  const int buttonY = bounds.height - 176;

  // The display face is deliberately wide. Two explicit line budgets keep the
  // renderer from silently truncating the final word (its ellipsis glyph is
  // not in the subset font, so truncation otherwise looks like missing text).
  text(screen, fui::makeRect(kMargin + 12, heroY, bounds.width - 2 * kMargin - 24, 104), "BREAK OUT. GO DEEP.",
       toybox::kDisplayFont, fui::TextAlign::Center, 2);
  text(screen, fui::makeRect(64, heroY + 112, bounds.width - 128, 116),
       "MAP TWO FLOORS. FIGHT MONSTERS. OPEN THE LAST DOOR.", toybox::kUiFont,
       fui::TextAlign::Center, 3);

  char record[64];
  std::snprintf(record, sizeof(record), "%d ROOMS FOUND  /  %d DEATHS", model.visited, model.deaths);
  text(screen, fui::makeRect(kMargin + 8, heroY + 250, bounds.width - 2 * kMargin - 16, 28), record, toybox::kTileFont,
       fui::TextAlign::Center);

  const int slots = model.hasSave ? 3 : 2;
  const int width = (bounds.width - 2 * kMargin - (slots - 1) * kGap) / slots;
  int slot = 0;
  if (model.hasSave) {
    button(screen, fui::makeRect(kMargin, buttonY, width, toybox::kPillHeight), "RESUME", ActionResume);
    ++slot;
  }
  button(screen, fui::makeRect(kMargin + slot * (width + kGap), buttonY, width, toybox::kPillHeight),
         model.hasSave ? "START OVER" : "DESCEND", ActionNew, 0, model.hasSave);
  ++slot;
  button(screen, fui::makeRect(kMargin + slot * (width + kGap), buttonY, width, toybox::kPillHeight), "HOW TO",
         ActionHowTo, 0, true);
}

void buildRoom(toybox::Screen& screen, const RoomModel& model) {
  const dungeonrun::Game& game = *model.game;
  const dungeonrun::Room& room = dungeonrun::kRooms[game.room()];
  header(screen, room.name);
  const fui::Rect bounds = screen.device().screen();

  char stats[80];
  std::snprintf(stats, sizeof(stats), "HP %d/10   COINS %d   DMG %d   KEYS %d", game.hp(), game.coins(), game.damage(),
                game.keys());
  text(screen, fui::makeRect(kMargin, 104, bounds.width - 2 * kMargin, 30), stats, toybox::kUiFont);

  // Room prose is information, not a second headline. The display cut needed
  // three or four lines here and was still truncating the final word on the
  // device; the UI cut gives the sentence room and keeps the result line below
  // it in a separate band.
  text(screen, fui::makeRect(kMargin + 20, 154, bounds.width - 2 * kMargin - 40, 104), roomCopy(room, game),
       toybox::kUiFont, fui::TextAlign::Center, 3);
  if (model.message != nullptr) {
    text(screen, fui::makeRect(kMargin + 8, 276, bounds.width - 2 * kMargin - 16, 34), model.message,
         toybox::kTileFont,
         fui::TextAlign::Center);
  }

  if (game.enemyAlive()) {
    char foe[48];
    std::snprintf(foe, sizeof(foe), "ENEMY %d / %d", game.enemyHealth(), room.enemyHp);
    text(screen, fui::makeRect(kMargin, 326, bounds.width - 2 * kMargin, 32), foe, toybox::kUiFont,
         fui::TextAlign::Center);
    const int barW = bounds.width - 128;
    screen.target().stroke(fui::makeRect(64, 370, barW, 24), fui::Paint::solid(fui::Color::Black), 2);
    screen.target().fill(fui::makeRect(68, 374, (barW - 8) * game.enemyHealth() / room.enemyHp, 16),
                         fui::Paint::solid(fui::Color::Black));
    button(screen, fui::makeRect(kMargin, 430, bounds.width - 2 * kMargin, toybox::kPillHeight), "ROLL THE BONE",
           ActionAct);
  } else if (game.trapped()) {
    button(screen, fui::makeRect(kMargin, 326, bounds.width - 2 * kMargin, toybox::kPillHeight), "ROLL TO ESCAPE",
           ActionAct);
  } else if (room.kind == dungeonrun::Kind::Puzzle && !game.cleared(game.room())) {
    static constexpr int kAnswers[3] = {2, 4, 6};
    const int answerW = (bounds.width - 2 * kMargin - 2 * kGap) / 3;
    for (int i = 0; i < 3; ++i) {
      char answer[4];
      std::snprintf(answer, sizeof(answer), "%d", kAnswers[i]);
      button(screen, fui::makeRect(kMargin + i * (answerW + kGap), 326, answerW, toybox::kPillHeight), answer,
             ActionPuzzle, kAnswers[i], true);
    }
  } else {
    const bool smithUseful = room.kind == dungeonrun::Kind::Smith && (game.damage() < 3 || !game.hasShield());
    const bool oneTimeUseful = (room.kind == dungeonrun::Kind::Shrine || room.kind == dungeonrun::Kind::Treasure) &&
                               !game.cleared(game.room());
    const bool useful = room.kind == dungeonrun::Kind::Key || room.kind == dungeonrun::Kind::Fountain || smithUseful ||
                        oneTimeUseful;
    if (useful && (room.kind != dungeonrun::Kind::Key || !game.keyFound())) {
      char forgeLabel[32];
      const char* label = "SEARCH";
      if (room.kind == dungeonrun::Kind::Smith) {
        if (game.damage() < 3) {
          std::snprintf(forgeLabel, sizeof(forgeLabel), "UPGRADE - %d COINS", game.damage() * 5);
        } else {
          std::snprintf(forgeLabel, sizeof(forgeLabel), "SHIELD - 8 COINS");
        }
        label = forgeLabel;
      } else if (room.kind == dungeonrun::Kind::Shrine) {
        label = "PRAY";
      } else if (room.kind == dungeonrun::Kind::Treasure) {
        label = "OPEN CHEST";
      }
      button(screen, fui::makeRect(kMargin, 326, bounds.width - 2 * kMargin, toybox::kPillHeight), label, ActionAct,
             0, true);
    }
  }

  // Four full direction names do not fit safely across the 480px panel in the
  // Toybox face. Compass letters stay unambiguous and leave generous glyph
  // bearings on both sides of every button.
  static constexpr const char* kDirName[4] = {"N", "E", "S", "W"};
  const int dirY = bounds.height - 180;
  const int dirW = (bounds.width - 2 * kMargin - 3 * 6) / 4;
  for (int d = 0; d < 4; ++d) {
    if (!game.canMove(static_cast<dungeonrun::Direction>(d))) continue;
    button(screen, fui::makeRect(kMargin + d * (dirW + 6), dirY, dirW, toybox::kPillHeight), kDirName[d], ActionMove,
           d, true);
  }
  const fui::Rect mapButton =
      fui::makeRect(kMargin, bounds.height - 104, bounds.width - 2 * kMargin, toybox::kPillHeight);
  if (game.enemyAlive()) {
    screen.target().fill(mapButton, fui::Paint::dither(fui::Color::LightGray));
    screen.target().stroke(mapButton, fui::Paint::solid(fui::Color::Black), 1);
    text(screen, mapButton, "NO ESCAPE - DEFEAT THE MONSTER", toybox::kTileFont, fui::TextAlign::Center);
  } else if (game.trapped()) {
    screen.target().fill(mapButton, fui::Paint::dither(fui::Color::LightGray));
    screen.target().stroke(mapButton, fui::Paint::solid(fui::Color::Black), 1);
    text(screen, mapButton, "TRAPPED - ROLL TO ESCAPE", toybox::kTileFont, fui::TextAlign::Center);
  } else {
    button(screen, mapButton, "MASTER MAP", ActionMap);
  }
}

void buildMap(toybox::Screen& screen, const dungeonrun::Game& game) {
  header(screen, "MASTER MAP");
  const fui::Rect bounds = screen.device().screen();
  text(screen, fui::makeRect(kMargin, 98, bounds.width - 2 * kMargin, 26), "TAP ANY ROOM YOU HAVE EXPLORED",
       toybox::kTileFont, fui::TextAlign::Center);
  constexpr int cell = 76;
  constexpr int gap = 6;
  constexpr int gridLeft = 100;
  static constexpr int floorTop[2] = {146, 410};
  for (int level = 0; level < 2; ++level) {
    char label[16];
    std::snprintf(label, sizeof(label), "FLOOR %d", level + 1);
    const int top = floorTop[level];
    text(screen, fui::makeRect(kMargin, top, 96, 24), label, toybox::kTileFont);
    for (int i = 0; i < dungeonrun::kRoomCount; ++i) {
      const dungeonrun::Room& room = dungeonrun::kRooms[i];
      if (room.level != level) continue;
      const fui::Rect box =
          fui::makeRect(gridLeft + room.x * (cell + gap), top - 12 + room.y * (cell + gap), cell, cell);
      if (!game.visited(i)) {
        screen.target().stroke(box, fui::Paint::dither(fui::Color::LightGray), 1, 4);
        continue;
      }
      char number[8];
      std::snprintf(number, sizeof(number), "%d", i + 1);
      button(screen, box, number, ActionTravel, i, i != game.room());
    }
  }
  button(screen, fui::makeRect(kMargin, bounds.height - 104, bounds.width - 2 * kMargin, toybox::kPillHeight),
         "BACK TO THE ROOM", ActionBack, 0, true);
}

void buildHowTo(toybox::Screen& screen) {
  header(screen, "HOW TO ESCAPE");
  const fui::Rect bounds = screen.device().screen();
  const int instructionsBottom = bounds.height - 128;
  text(screen, fui::makeRect(36, 112, bounds.width - 72, instructionsBottom - 112),
       "EXPLORE ROOMS OR QUICK-TRAVEL WITH THE MAP. MONSTERS AND TRAPS BLOCK ESCAPE. ROLL TO FIGHT OR BREAK FREE. ENEMIES STAY WOUNDED AFTER DEATH. SOLVE PUZZLES AND SEARCH TREASURE FOR COINS. SHRINES HEAL ONCE. KEYS OPEN PASSAGES. BUY GEAR AT THE FORGE. DEFEAT THE WARDEN.",
       toybox::kUiFont, fui::TextAlign::Left, 13);
  button(screen, fui::makeRect(kMargin, bounds.height - 104, bounds.width - 2 * kMargin, toybox::kPillHeight), "BACK",
         ActionBack, 0, true);
}

void buildWon(toybox::Screen& screen, const dungeonrun::Game& game) {
  header(screen, "FREEDOM");
  const fui::Rect bounds = screen.device().screen();
  text(screen, fui::makeRect(kMargin + 20, 170, bounds.width - 2 * kMargin - 40, 112),
       "THE LAST DOOR OPENS. YOU ARE FREE.", toybox::kUiFont, fui::TextAlign::Center, 2);
  char score[80];
  std::snprintf(score, sizeof(score), "%d COINS  /  %d DEATHS", game.coins(), game.deaths());
  text(screen, fui::makeRect(kMargin, 300, bounds.width - 2 * kMargin, 32), score, toybox::kUiFont,
       fui::TextAlign::Center);
  button(screen, fui::makeRect(kMargin, bounds.height - 176, bounds.width - 2 * kMargin, toybox::kPillHeight),
         "RUN AGAIN", ActionNew);
  button(screen, fui::makeRect(kMargin, bounds.height - 104, bounds.width - 2 * kMargin, toybox::kPillHeight),
         "BACK TO GAMES", ActionBack, 0, true);
}

}  // namespace dungeonrunui
