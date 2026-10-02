#include "TerminalHackScreens.h"

#include <cstdio>
#include <cstring>

namespace terminalhackui {
namespace {

constexpr int kDumpRows = 12;
constexpr int kCharactersPerColumn = 18;
constexpr char kSymbols[] = "!@#$%^&*()_+-={}[]|\\:;'<>,.?/~`";

void fillJumble(char (&text)[kCharactersPerColumn + 1], int seed) {
  constexpr int symbolCount = sizeof(kSymbols) - 1;
  for (int i = 0; i < kCharactersPerColumn; ++i) text[i] = kSymbols[(seed * 7 + i * 11) % symbolCount];
  text[kCharactersPerColumn] = '\0';
}

void drawFixedGridText(toybox::Screen& screen, const fui::Rect& line, const char* text) {
  fui::TextStyle style;
  style.font = toybox::kSmallFont;
  style.align = fui::TextAlign::Left;
  style.maxLines = 1;
  screen.target().text(line, text, style);
}

int hackSlot(const terminalhack::Game& game, const int hack) {
  static constexpr uint8_t kOnePageSteps[] = {1, 5, 7, 11};
  static constexpr uint8_t kTwoPageSteps[] = {5, 7, 11, 13};
  const int slotCount = game.candidateCount > 12 ? 24 : 12;
  const int offset = static_cast<int>((game.seed >> 7) % slotCount);
  const int step = game.candidateCount > 12 ? kTwoPageSteps[(game.seed >> 13) % 4]
                                            : kOnePageSteps[(game.seed >> 13) % 4];
  return (offset + hack * step) % slotCount;
}

void header(toybox::Screen& screen, const terminalhack::Game& game, bool daily) {
  char attempts[24];
  std::snprintf(attempts, sizeof(attempts), "ATTEMPTS %u/%u", static_cast<unsigned>(game.attempts),
                static_cast<unsigned>(game.attemptLimit));
  char level[28];
  if (daily) {
    std::snprintf(level, sizeof(level), "DAILY");
  } else {
    std::snprintf(level, sizeof(level), "L%u %s", static_cast<unsigned>(game.level),
                  terminalhack::difficultyName(game.difficulty));
  }
  fui::HeaderProps props;
  props.title = level;
  props.rightLabel = attempts;
  props.titleText.font = toybox::kUiFont;
  props.titleText.color = fui::Color::White;
  props.subtitleText.font = toybox::kUiFont;
  props.subtitleText.color = fui::Color::White;
  props.subtitleText.align = fui::TextAlign::Right;
  props.borderEdges = fui::EdgesNone;
  screen.header(props);
  screen.target().fill(fui::makeRect(0, toybox::kHeaderHeight + 4, screen.device().width, toybox::kRule),
                       fui::Paint::solid(fui::Color::Black));
}

void dumpCell(toybox::Screen& screen, const terminalhack::Game& game, int page, int column, int row,
              const fui::Rect& line) {
  char text[kCharactersPerColumn + 1];
  bool focused = false;
  fui::Rect focusRect = line;
  fillJumble(text, column * kDumpRows + row);
  if ((row & 1) == 0) {
    const int wordIndex = page * 12 + column * 6 + row / 2;
    const int wordStart = (wordIndex * 5) % (kCharactersPerColumn - terminalhack::kWordLength + 1);
    if (wordIndex < terminalhack::kWordCount && game.active[wordIndex]) {
      std::memcpy(text + wordStart, terminalhack::word(game, wordIndex), terminalhack::kWordLength);
    } else {
      std::memset(text + wordStart, '.', terminalhack::kWordLength);
    }
    if (wordIndex < terminalhack::kWordCount && game.active[wordIndex] && !game.guessed[wordIndex]) {
      screen.frame().hit(line, ActionWord, static_cast<int16_t>(wordIndex));
      focused = fui::hasState(screen.frame().stateFor(ActionWord, static_cast<int16_t>(wordIndex)), fui::StateFocused);
    }
  } else {
    const int gap = row / 2;
    const int slot = page * 12 + column * 6 + gap;
    const int bracketOffset = 2 + static_cast<int>((game.seed >> 4) % 5);
    for (int hack = 0; hack < terminalhack::kHackCount; ++hack) {
      if (hackSlot(game, hack) != slot) continue;
      static constexpr const char* kSequences[] = {"(<+>)", "{#|#}", "[!?!]", "<^&>"};
      const char* sequence = kSequences[(static_cast<int>(game.seed >> (hack * 3)) + hack) % 4];
      std::memcpy(text + bracketOffset, game.hackUsed[hack] ? "....." : sequence, 5);
      if (!game.hackUsed[hack]) {
        char prefix[kCharactersPerColumn + 1];
        std::memcpy(prefix, text, bracketOffset);
        prefix[bracketOffset] = '\0';
        fui::TextStyle style;
        style.font = toybox::kSmallFont;
        style.align = fui::TextAlign::Left;
        style.maxLines = 1;
        const int16_t prefixWidth = screen.target().measureText(style.font, prefix, style).width;
        const int16_t sequenceWidth = screen.target().measureText(style.font, sequence, style).width;
        focusRect = fui::makeRect(static_cast<int16_t>(line.x + prefixWidth), line.y, sequenceWidth, line.height);
        screen.frame().hit(focusRect, ActionHack, static_cast<int16_t>(hack));
        focused = fui::hasState(screen.frame().stateFor(ActionHack, static_cast<int16_t>(hack)), fui::StateFocused);
      }
      break;
    }
  }
  drawFixedGridText(screen, line, text);
  if (focused) screen.target().stroke(focusRect, fui::Paint::solid(fui::Color::Black), 2);
}

void transcript(toybox::Screen& screen, const terminalhack::Game& game, const fui::Rect& box,
                const char* notice) {
  fui::TextStyle text;
  text.font = toybox::kSmallFont;
  text.align = fui::TextAlign::Left;
  text.maxLines = 1;

  int16_t y = box.y + 8;
  char status[48];
  if (game.guessCount > 0 && notice != nullptr && std::strcmp(notice, "ENTRY DENIED") == 0 &&
      game.result == terminalhack::Result::Playing) {
    const terminalhack::Guess& latest = game.guesses[game.guessCount - 1];
    std::snprintf(status, sizeof(status), "INCORRECT. LIKENESS %u/%d", static_cast<unsigned>(latest.likeness),
                  terminalhack::kWordLength);
  } else {
    std::snprintf(status, sizeof(status), "%s", notice == nullptr ? "SELECT PASSWORD" : notice);
  }
  screen.target().text(fui::makeRect(box.x, y, box.width, 24), status, text);
  y += 32;
  screen.target().text(fui::makeRect(box.x, y, box.width, 24), "PRIOR GUESSES", text);
  y += 28;

  for (int i = game.guessCount - 1; i >= 0 && y + 24 <= box.bottom(); --i) {
    char line[48];
    std::snprintf(line, sizeof(line), "%s    LIKENESS %u/%d", terminalhack::word(game, game.guesses[i].word),
                  static_cast<unsigned>(game.guesses[i].likeness), terminalhack::kWordLength);
    screen.target().text(fui::makeRect(box.x, y, box.width, 24), line, text);
    y += 26;
  }
}

}  // namespace

void buildMenu(toybox::Screen& screen, const Model& model) {
  fui::HeaderProps headerProps;
  headerProps.title = "TERMINAL HACK";
  headerProps.borderEdges = fui::EdgesNone;
  screen.header(headerProps);
  screen.insetContent(fui::Insets{toybox::kMargin, 36, toybox::kMargin, 36});

  fui::TextStyle text;
  text.font = toybox::kSmallFont;
  text.align = fui::TextAlign::Center;
  text.maxLines = 1;

  char progress[48];
  std::snprintf(progress, sizeof(progress), "RANK %u   XP %lu/%u", static_cast<unsigned>(model.rank),
                static_cast<unsigned long>(model.experience % terminalhack::kXpPerRank),
                static_cast<unsigned>(terminalhack::kXpPerRank));
  screen.target().text(screen.takeTop(24), progress, text);
  fui::ProgressBarProps xpBar;
  xpBar.value = static_cast<int16_t>(model.experience % terminalhack::kXpPerRank);
  xpBar.max = terminalhack::kXpPerRank;
  xpBar.track = fui::Paint::solid(fui::Color::White);
  xpBar.border = fui::Paint::solid(fui::Color::Black);
  xpBar.borderWidth = 1;
  fui::progressBar(screen.frame(), screen.takeTop(12, 16), xpBar);

  for (int i = 0; i < terminalhack::kDifficultyCount; ++i) {
    char label[40];
    std::snprintf(label, sizeof(label), "%c %-8s  %u WINS",
                  model.selectedDifficulty == static_cast<terminalhack::Difficulty>(i) ? '>' : ' ',
                  terminalhack::difficultyName(static_cast<terminalhack::Difficulty>(i)),
                  static_cast<unsigned>(model.difficultyWins[i]));
    fui::ButtonProps difficulty;
    difficulty.label = label;
    difficulty.action = ActionDifficulty;
    difficulty.value = static_cast<int16_t>(i);
    screen.button(difficulty, screen.takeTop(48, 8));
  }

  fui::ButtonProps dailyButton;
  dailyButton.label = "DAILY TERMINAL";
  dailyButton.action = ActionDaily;
  // Keep this actionable so a press can re-read a clock synchronized after
  // the menu was rendered, including the first press after midnight.
  dailyButton.enabled = true;
  screen.button(dailyButton, screen.takeTop(52, 12));

  char today[48];
  std::snprintf(today, sizeof(today), "TODAY: %s", !model.dailyAvailable ? "CLOCK NEEDED"
                                                    : model.dailyComplete ? "COMPLETE"
                                                                          : "NOT COMPLETE");
  screen.target().text(screen.takeTop(28), today, text);
}

void buildBoard(toybox::Screen& screen, const Model& model) {
  if (model.menu) {
    buildMenu(screen, model);
    return;
  }
  if (model.game == nullptr) return;
  const terminalhack::Game& game = *model.game;
  header(screen, game, model.daily);
  screen.insetContent(fui::Insets{toybox::kMargin, 12, toybox::kMargin, 12});

  if (game.result != terminalhack::Result::Playing) {
    fui::TextStyle result;
    result.font = toybox::kDisplayFont;
    result.align = fui::TextAlign::Center;
    screen.target().text(screen.takeTop(300, 20),
                         game.result == terminalhack::Result::Won ? "ACCESS GRANTED" : "TERMINAL LOCKED", result);
    if (!model.daily) {
      char xp[32];
      std::snprintf(xp, sizeof(xp), game.xpDelta >= 0 ? "XP GAINED +%d" : "XP LOST %d",
                    static_cast<int>(game.xpDelta));
      fui::TextStyle xpText;
      xpText.font = toybox::kUiFont;
      xpText.align = fui::TextAlign::Center;
      xpText.maxLines = 1;
      screen.target().text(screen.takeTop(32, 8), xp, xpText);
      fui::ProgressBarProps xpBar;
      xpBar.value = static_cast<int16_t>(model.experience % terminalhack::kXpPerRank);
      xpBar.max = terminalhack::kXpPerRank;
      xpBar.track = fui::Paint::solid(fui::Color::White);
      xpBar.border = fui::Paint::solid(fui::Color::Black);
      xpBar.borderWidth = 1;
      fui::progressBar(screen.frame(), screen.takeTop(12), xpBar);
    }
    fui::ButtonProps again;
    again.label = model.daily ? "BACK TO MODES"
                  : game.result == terminalhack::Result::Won ? "NEXT LEVEL"
                                                             : "RETRY LEVEL";
    again.action = ActionAgain;
    screen.button(again, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));
    return;
  }

  const fui::Rect history = screen.takeBottom(174, 10);
  screen.target().fill(fui::makeRect(history.x, static_cast<int16_t>(history.y - 8), history.width, toybox::kRule),
                       fui::Paint::solid(fui::Color::Black));
  transcript(screen, game, history, model.notice);

  const fui::Rect dump = screen.body();
  constexpr int16_t indicatorWidth = 22;
  const fui::Rect grid = fui::makeRect(dump.x, dump.y, static_cast<int16_t>(dump.width - indicatorWidth), dump.height);
  const int16_t columnGap = 18;
  const int16_t columnWidth = static_cast<int16_t>((grid.width - columnGap) / 2);
  const int16_t rowHeight = static_cast<int16_t>(grid.height / kDumpRows);
  const int16_t dividerX = static_cast<int16_t>(grid.x + grid.width / 2);
  screen.target().fill(fui::makeRect(dividerX, grid.y, toybox::kRule, grid.height),
                       fui::Paint::solid(fui::Color::Black));
  for (int row = 0; row < kDumpRows; ++row) {
    const int16_t top = static_cast<int16_t>(grid.y + row * rowHeight);
    const int16_t height = row == kDumpRows - 1 ? static_cast<int16_t>(grid.bottom() - top) : rowHeight;
    for (int column = 0; column < 2; ++column) {
      const int16_t left = static_cast<int16_t>(grid.x + column * (columnWidth + columnGap));
      dumpCell(screen, game, model.gridPage, column, row, fui::makeRect(left, top, columnWidth, height));
    }
  }

  const uint8_t pageCount = static_cast<uint8_t>((game.candidateCount + 11) / 12);
  fui::TextStyle indicator;
  indicator.font = toybox::kSmallFont;
  indicator.align = fui::TextAlign::Center;
  indicator.maxLines = 1;
  const int16_t indicatorX = static_cast<int16_t>(grid.right() + 2);
  screen.target().text(fui::makeRect(indicatorX, grid.y, 18, 22), model.gridPage > 0 ? "^" : "", indicator);
  screen.target().text(fui::makeRect(indicatorX, static_cast<int16_t>(grid.bottom() - 22), 18, 22),
                       model.gridPage + 1 < pageCount ? "v" : "", indicator);
  char page[8];
  std::snprintf(page, sizeof(page), "%u/%u", static_cast<unsigned>(model.gridPage + 1),
                static_cast<unsigned>(pageCount));
  screen.target().text(fui::makeRect(indicatorX, static_cast<int16_t>(grid.y + grid.height / 2 - 10), 18, 22), page,
                       indicator);
}

}  // namespace terminalhackui
