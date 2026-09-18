#include "ReadingJournalActivity.h"

#include <Memory.h>
#include <Logging.h>
#include <Bitmap.h>
#include <HalStorage.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../../components/UITheme.h"
#include "../../RecentBooksStore.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "fontIds.h"

namespace {

constexpr const char* kMonths[] = {"JANUARY", "FEBRUARY", "MARCH",     "APRIL",   "MAY",      "JUNE",
                                   "JULY",    "AUGUST",   "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
constexpr const char* kWeek[] = {"S", "M", "T", "W", "T", "F", "S"};

void formatStamp(const uint32_t epoch, char* out, const size_t cap) {
  if (epoch == 0) {
    std::snprintf(out, cap, "—");
    return;
  }
  const time_t raw = static_cast<time_t>(epoch);
  struct tm parts {};
  localtime_r(&raw, &parts);
  std::snprintf(out, cap, "%d %s %d  %02d:%02d", parts.tm_mday, kMonths[parts.tm_mon], parts.tm_year + 1900,
                parts.tm_hour, parts.tm_min);
}

void drawStar(const GfxRenderer& renderer, const int cx, const int cy, const int radius, const bool filled) {
  static constexpr int8_t px[10] = {0, 22, 95, 36, 59, 0, -59, -36, -95, -22};
  static constexpr int8_t py[10] = {-100, -31, -31, 12, 81, 38, 81, 12, -31, -31};
  int x[10];
  int y[10];
  for (int i = 0; i < 10; ++i) {
    x[i] = cx + px[i] * radius / 100;
    y[i] = cy + py[i] * radius / 100;
  }
  for (int i = 0; i < 10; ++i) renderer.drawLine(x[i], y[i], x[(i + 1) % 10], y[(i + 1) % 10], true);
  if (filled) {
    for (int r = 2; r < radius - 2; r += 2) {
      for (int i = 0; i < 10; ++i) {
        renderer.drawLine(cx + px[i] * r / 100, cy + py[i] * r / 100,
                          cx + px[(i + 1) % 10] * r / 100, cy + py[(i + 1) % 10] * r / 100, true);
      }
    }
  }
}

}  // namespace

std::unique_ptr<Activity> ReadingJournalActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ReadingJournalActivity>(renderer, mappedInput);
}

void ReadingJournalActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  entries = makeUniqueNoThrow<journal::Entry[]>(journal::kMaxEntries);
  if (!entries) {
    LOG_ERR("JRNL", "OOM: journal screen");
    shelf::leave(renderer, mappedInput);
    return;
  }
  count = journal::load(entries.get(), journal::kMaxEntries);
#if defined(SIMULATOR)
  if (count == 0 && std::getenv("CROSSPLAY_JOURNAL_DEMO")) {
    const time_t base = time(nullptr);
    const char* titles[] = {"The Left Hand of Darkness", "Piranesi", "A Psalm for the Wild-Built"};
    const char* authors[] = {"Ursula K. Le Guin", "Susanna Clarke", "Becky Chambers"};
    for (int i = 0; i < 3; ++i) {
      std::snprintf(entries[i].path, journal::kPathBytes, "/books/demo-%d.epub", i);
      std::snprintf(entries[i].title, journal::kTitleBytes, "%s", titles[i]);
      std::snprintf(entries[i].author, journal::kAuthorBytes, "%s", authors[i]);
      entries[i].startedAt = static_cast<uint32_t>(base - (28 - i * 9) * 86400);
      entries[i].finishedAt = i < 2 ? entries[i].startedAt + static_cast<uint32_t>((6 + i * 3) * 86400) : 0;
      entries[i].rating = static_cast<uint8_t>(i < 2 ? 5 - i : 0);
    }
    count = 3;
  }
#endif
  selected = count > 0 ? count - 1 : -1;
  const uint32_t currentDate = journal::today();
  if (currentDate != 0) {
    year = static_cast<int>(currentDate / 10000);
    month = static_cast<int>((currentDate / 100) % 100);
  } else if (selected >= 0) {
    const uint32_t date = journal::dateKey(entries[selected].finishedAt ? entries[selected].finishedAt
                                                                       : entries[selected].startedAt);
    year = static_cast<int>(date / 10000);
    month = static_cast<int>((date / 100) % 100);
  }
  requestUpdate();
}

void ReadingJournalActivity::onExit() {
  entries.reset();
  Activity::onExit();
}

void ReadingJournalActivity::stepMonth(const int delta) {
  month += delta;
  if (month < 1) {
    month = 12;
    --year;
  } else if (month > 12) {
    month = 1;
    ++year;
  }
}

void ReadingJournalActivity::selectNext(const int delta) {
  if (count <= 0) return;
  selected = (selected + delta + count) % count;
  const uint32_t date = journal::dateKey(entries[selected].finishedAt ? entries[selected].finishedAt
                                                                     : entries[selected].startedAt);
  if (date) {
    year = static_cast<int>(date / 10000);
    month = static_cast<int>((date / 100) % 100);
  }
}

void ReadingJournalActivity::beginDateEdit(const bool finish) {
  editingFinish = finish;
  uint32_t date = journal::dateKey(finish ? entries[selected].finishedAt : entries[selected].startedAt);
  if (date == 0) date = journal::today();
  if (date == 0) return;
  editYear = static_cast<int>(date / 10000);
  editMonth = static_cast<int>((date / 100) % 100);
  editDay = static_cast<int>(date % 100);
  view = View::DateEditor;
}

bool ReadingJournalActivity::saveDateEdit() {
  if (selected < 0 || selected >= count) return false;
  const uint32_t date = static_cast<uint32_t>(editYear * 10000 + editMonth * 100 + editDay);
  const bool saved = editingFinish ? journal::setFinishedDate(entries[selected].path, date)
                                   : journal::setStartedDate(entries[selected].path, date);
  if (!saved) {
    LOG_ERR("JRNL", "Could not save selected journal date");
    return false;
  }
  const uint32_t epoch = journal::epochForDate(editYear, editMonth, editDay);
  if (editingFinish) entries[selected].finishedAt = epoch;
  else entries[selected].startedAt = epoch;
  return true;
}

void ReadingJournalActivity::stepEditMonth(const int delta) {
  editMonth += delta;
  if (editMonth < 1) {
    editMonth = 12;
    --editYear;
  } else if (editMonth > 12) {
    editMonth = 1;
    ++editYear;
  }
  editDay = std::min(editDay, journal::daysInMonth(editYear, editMonth));
}

void ReadingJournalActivity::stepEditDay(const int delta) {
  editDay += delta;
  if (editDay < 1) {
    stepEditMonth(-1);
    editDay = journal::daysInMonth(editYear, editMonth);
  } else if (editDay > journal::daysInMonth(editYear, editMonth)) {
    editDay = 1;
    stepEditMonth(1);
  }
}

void ReadingJournalActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view == View::DateEditor) {
      view = View::Summary;
      requestUpdate();
    } else if (view == View::Summary) {
      view = View::Calendar;
      requestUpdate();
    } else {
      shelf::leave(renderer, mappedInput);
    }
    return;
  }
  if (view == View::Calendar) {
    int touched = -1;
    const auto cardTouch = mappedInput.rowTouch(touched, bookCardRect.y, bookCardRect.height, selected >= 0 ? 1 : 0,
                                                bookCardRect.x, bookCardRect.x + bookCardRect.width,
                                                bookCardRect.height);
    if (cardTouch == MappedInputManager::RowTouch::Tap && selected >= 0) {
      summaryField = 0;
      view = View::Summary;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) stepMonth(-1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) stepMonth(1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Up)) selectNext(-1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) selectNext(1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && selected >= 0) {
      summaryField = 0;
      view = View::Summary;
    }
    else return;
  } else if (view == View::Summary) {
    journal::Entry& entry = entries[selected];
    int touched = -1;
    const int rowH = summaryRowsRect.height / 3;
    const auto rowTouch = mappedInput.rowTouch(touched, summaryRowsRect.y, rowH, 3, summaryRowsRect.x,
                                               summaryRowsRect.x + summaryRowsRect.width, rowH);
    if (rowTouch == MappedInputManager::RowTouch::Down && touched >= 0) {
      summaryField = touched;
      requestUpdate();
      return;
    }
    if (rowTouch == MappedInputManager::RowTouch::Tap && touched >= 0) {
      summaryField = touched;
      if (summaryField < 2) beginDateEdit(summaryField == 1);
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      summaryField = (summaryField + 2) % 3;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      summaryField = (summaryField + 1) % 3;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && summaryField < 2) {
      beginDateEdit(summaryField == 1);
      requestUpdate();
      return;
    }
    if (summaryField != 2 || entry.finishedAt == 0) return;
    uint8_t next = entry.rating;
    if (mappedInput.wasReleased(MappedInputManager::Button::Left) && next > 0) --next;
    else if (mappedInput.wasReleased(MappedInputManager::Button::Right) && next < 5) ++next;
    else return;
    if (next != entry.rating && journal::setRating(entry.path, next)) entry.rating = next;
  } else {
    int tapX = 0;
    int tapY = 0;
    const bool tapped = mappedInput.wasScreenTapped(tapX, tapY);
    const auto inside = [tapX, tapY](const Rect& rect) {
      return tapX >= rect.x && tapX < rect.x + rect.width && tapY >= rect.y && tapY < rect.y + rect.height;
    };
    if (tapped && inside(dateCancelRect)) {
      view = View::Summary;
    } else if (tapped && inside(dateConfirmRect)) {
      if (saveDateEdit()) view = View::Summary;
    } else if (tapped && editingFinish && entries[selected].finishedAt != 0 && inside(clearFinishRect)) {
      if (journal::clearFinishedDate(entries[selected].path)) {
        entries[selected].finishedAt = 0;
        entries[selected].rating = 0;
        view = View::Summary;
      }
    } else if (tapped && inside(dateGridRect)) {
      const int cellW = dateGridRect.width / 7;
      const int cellH = dateGridRect.height / 6;
      const int slot = ((tapY - dateGridRect.y) / cellH) * 7 + (tapX - dateGridRect.x) / cellW;
      const int tappedDay = slot - journal::weekday(editYear, editMonth, 1) + 1;
      if (tappedDay >= 1 && tappedDay <= journal::daysInMonth(editYear, editMonth)) editDay = tappedDay;
      else return;
    } else {
      const auto swipe = mappedInput.wasSwipe();
      if (swipe == MappedInputManager::SwipeDir::Left) stepEditDay(1);
      else if (swipe == MappedInputManager::SwipeDir::Right) stepEditDay(-1);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Left)) stepEditDay(-1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) stepEditDay(1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Up)) stepEditMonth(-1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) stepEditMonth(1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (saveDateEdit()) view = View::Summary;
    } else return;
    }
  }
  requestUpdate();
}

void ReadingJournalActivity::drawCalendar() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int top = metrics.topPadding + metrics.headerHeight + 28;
  const int side = metrics.contentSidePadding;
  const int cellW = (sw - side * 2) / 7;
  const int cellH = 61;
  char heading[24];
  std::snprintf(heading, sizeof(heading), "%s %d", kMonths[month - 1], year);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, heading);
  for (int col = 0; col < 7; ++col) {
    UITheme::drawCenteredText(renderer, Rect{side + col * cellW, top, cellW, 24}, UI_10_FONT_ID, top, kWeek[col],
                              true, EpdFontFamily::BOLD);
  }
  const int first = journal::weekday(year, month, 1);
  const int days = journal::daysInMonth(year, month);
  for (int day = 1; day <= days; ++day) {
    const int slot = first + day - 1;
    const int col = slot % 7;
    const int row = slot / 7;
    const int x = side + col * cellW;
    const int y = top + 32 + row * cellH;
    char number[4];
    std::snprintf(number, sizeof(number), "%d", day);
    UITheme::drawCenteredText(renderer, Rect{x, y, cellW, 24}, UI_10_FONT_ID, y + 4, number);
    bool start = false;
    bool finish = false;
    for (int i = 0; i < count; ++i) {
      const uint32_t wanted = static_cast<uint32_t>(year * 10000 + month * 100 + day);
      start = start || journal::dateKey(entries[i].startedAt) == wanted;
      finish = finish || journal::dateKey(entries[i].finishedAt) == wanted;
    }
    if (start) renderer.drawRoundedRect(x + cellW / 2 - (finish ? 12 : 4), y + 31, 8, 8, 2, 4, true);
    if (finish) renderer.fillRoundedRect(x + cellW / 2 + (start ? 4 : -4), y + 31, 8, 8, 4, Black);
  }
  const int cardY = top + 32 + 6 * cellH + 12;
  bookCardRect = Rect{side, cardY - 12, sw - side * 2, 82};
  if (selected >= 0) {
    const auto& entry = entries[selected];
    renderer.drawRoundedRect(bookCardRect.x, bookCardRect.y, bookCardRect.width, bookCardRect.height, 8, 2, true);
    renderer.drawText(UI_12_FONT_ID, side, cardY, entry.title, true, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, side, cardY + 36,
                      entry.finishedAt ? "FINISHED  •  DETAILS" : "READING  •  DETAILS");
  } else {
    UITheme::drawCenteredText(renderer, Rect{side, cardY, sw - side * 2, 80}, UI_12_FONT_ID, cardY,
                              "OPEN A BOOK TO BEGIN YOUR JOURNAL");
  }
  const auto labels = mappedInput.mapLabels("Back", "Summary", "Books", "Month");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingJournalActivity::drawSummary() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  const auto& entry = entries[selected];
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, "BOOK SUMMARY");
  const int heroY = metrics.topPadding + metrics.headerHeight + 28;
  constexpr int coverW = 122;
  constexpr int coverH = 190;
  bool coverDrawn = false;
  const RecentBook book = RECENT_BOOKS.getDataFromBook(entry.path);
  if (!book.coverBmpPath.empty()) {
    const std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, metrics.homeCoverHeight);
    HalFile coverFile;
    if (Storage.openFileForRead("JRNL", coverPath, coverFile)) {
      Bitmap bitmap(coverFile);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        renderer.drawBitmap(bitmap, side, heroY, coverW, coverH);
        coverDrawn = true;
      }
    }
  }
  renderer.drawRect(side, heroY, coverW, coverH, true);
  if (!coverDrawn) {
    renderer.drawLine(side, heroY, side + coverW, heroY + coverH, true);
    renderer.drawLine(side + coverW, heroY, side, heroY + coverH, true);
  }
  const int textX = side + coverW + 18;
  const int textW = sw - side - textX;
  UITheme::drawCenteredWrappedText(renderer, Rect{textX, heroY + 15, textW, 92}, UI_12_FONT_ID, entry.title, 3,
                                   true, EpdFontFamily::BOLD);
  UITheme::drawCenteredWrappedText(renderer, Rect{textX, heroY + 112, textW, 58}, UI_10_FONT_ID,
                                   entry.author[0] ? entry.author : "UNKNOWN AUTHOR", 2);
  int y = heroY + coverH + 30;
  summaryRowsRect = Rect{side, y - 12, sw - side * 2, 255};
  char stamp[40];
  formatStamp(entry.startedAt, stamp, sizeof(stamp));
  if (summaryField == 0) renderer.drawRoundedRect(side - 8, y - 12, sw - side * 2 + 16, 76, 7, 2, true);
  renderer.drawText(UI_10_FONT_ID, side, y, "STARTED", true, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, side, y + 27, stamp);
  y += 85;
  formatStamp(entry.finishedAt, stamp, sizeof(stamp));
  if (summaryField == 1) renderer.drawRoundedRect(side - 8, y - 12, sw - side * 2 + 16, 76, 7, 2, true);
  renderer.drawText(UI_10_FONT_ID, side, y, "FINISHED", true, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, side, y + 27, stamp);
  y += 85;
  if (summaryField == 2) renderer.drawRoundedRect(side - 8, y - 12, sw - side * 2 + 16, 92, 7, 2, true);
  UITheme::drawCenteredText(renderer, Rect{side, y, sw - side * 2, 30}, UI_10_FONT_ID, y,
                            entry.finishedAt ? "YOUR RATING" : "RATE WHEN FINISHED", true,
                            EpdFontFamily::BOLD);
  for (int i = 0; i < 5; ++i) drawStar(renderer, sw / 2 - 104 + i * 52, y + 48, 20, i < entry.rating);
  const auto labels = mappedInput.mapLabels("Calendar", "Select", "Field", summaryField == 2 ? "Rating" : "Date");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingJournalActivity::drawDateEditor() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight},
                 editingFinish ? "SET FINISH DATE" : "SET START DATE");

  const int top = metrics.topPadding + metrics.headerHeight + 65;
  char date[24];
  std::snprintf(date, sizeof(date), "%s %d, %d", kMonths[editMonth - 1], editDay, editYear);
  UITheme::drawCenteredText(renderer, Rect{side, top, sw - side * 2, 70}, UI_12_FONT_ID, top + 18, date, true,
                            EpdFontFamily::BOLD);

  const int cellW = (sw - side * 2) / 7;
  const int cellH = 58;
  const int gridY = top + 105;
  dateGridRect = Rect{side, gridY + 34, cellW * 7, cellH * 6};
  for (int col = 0; col < 7; ++col) {
    UITheme::drawCenteredText(renderer, Rect{side + col * cellW, gridY, cellW, 24}, UI_10_FONT_ID, gridY,
                              kWeek[col], true, EpdFontFamily::BOLD);
  }
  const int first = journal::weekday(editYear, editMonth, 1);
  const int days = journal::daysInMonth(editYear, editMonth);
  for (int day = 1; day <= days; ++day) {
    const int slot = first + day - 1;
    const int x = side + (slot % 7) * cellW;
    const int y = gridY + 34 + (slot / 7) * cellH;
    char number[4];
    std::snprintf(number, sizeof(number), "%d", day);
    if (day == editDay) renderer.drawRoundedRect(x + 5, y - 7, cellW - 10, 39, 6, 2, true);
    UITheme::drawCenteredText(renderer, Rect{x, y, cellW, 28}, UI_10_FONT_ID, y, number, true,
                              day == editDay ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }
  UITheme::drawCenteredWrappedText(renderer, Rect{side, gridY + 365, sw - side * 2, 44}, UI_10_FONT_ID,
                                   "LEFT/RIGHT: DAY    UP/DOWN: MONTH", 2);
  const int screenH = renderer.getScreenHeight();
  const int buttonGap = 14;
  const int buttonH = 48;
  const int buttonW = (sw - side * 2 - buttonGap) / 2;
  const int buttonY = screenH - buttonH - 20;
  dateCancelRect = Rect{side, buttonY, buttonW, buttonH};
  dateConfirmRect = Rect{side + buttonW + buttonGap, buttonY, buttonW, buttonH};
  renderer.drawRoundedRect(dateCancelRect.x, dateCancelRect.y, dateCancelRect.width, dateCancelRect.height, 7, 2,
                           true);
  renderer.drawRoundedRect(dateConfirmRect.x, dateConfirmRect.y, dateConfirmRect.width, dateConfirmRect.height, 7, 2,
                           true);
  UITheme::drawCenteredText(renderer, dateCancelRect, UI_10_FONT_ID, dateCancelRect.y + 12, "CANCEL", true,
                            EpdFontFamily::BOLD);
  UITheme::drawCenteredText(renderer, dateConfirmRect, UI_10_FONT_ID, dateConfirmRect.y + 12, "CONFIRM", true,
                            EpdFontFamily::BOLD);
  clearFinishRect = Rect{};
  if (editingFinish && entries[selected].finishedAt != 0) {
    clearFinishRect = Rect{side, buttonY - buttonH - 12, sw - side * 2, buttonH};
    renderer.drawRoundedRect(clearFinishRect.x, clearFinishRect.y, clearFinishRect.width, clearFinishRect.height, 7, 2,
                             true);
    UITheme::drawCenteredText(renderer, clearFinishRect, UI_10_FONT_ID, clearFinishRect.y + 12, "CLEAR FINISH", true,
                              EpdFontFamily::BOLD);
  }
}

void ReadingJournalActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (view == View::Calendar) drawCalendar();
  else if (view == View::Summary) drawSummary();
  else drawDateEditor();
  renderer.displayBuffer();
}
