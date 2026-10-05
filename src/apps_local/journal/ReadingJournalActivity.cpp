#include "ReadingJournalActivity.h"

#include <Bitmap.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iterator>

#include "../../RecentBooksStore.h"
#include "../../components/UITheme.h"
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
  struct tm parts{};
  localtime_r(&raw, &parts);
  std::snprintf(out, cap, "%d %s %d  %02d:%02d", parts.tm_mday, kMonths[parts.tm_mon], parts.tm_year + 1900,
                parts.tm_hour, parts.tm_min);
}

void formatReadingTime(const uint32_t seconds, char* out, const size_t cap) {
  const uint32_t minutes = seconds / 60;
  if (minutes >= 60)
    std::snprintf(out, cap, "%luh %lu min", static_cast<unsigned long>(minutes / 60),
                  static_cast<unsigned long>(minutes % 60));
  else
    std::snprintf(out, cap, "%lu min", static_cast<unsigned long>(minutes));
}

void formatWorkReadingPay(const uint32_t seconds, char* out, const size_t cap) {
  // $44.9995/hour, stored in ten-thousandths of a dollar. Convert directly
  // from seconds to rounded cents without floating-point drift.
  constexpr uint64_t RATE_TEN_THOUSANDTHS = 449995;
  constexpr uint64_t CENTS_DENOMINATOR = 3600 * 100;
  const uint64_t cents =
      (static_cast<uint64_t>(seconds) * RATE_TEN_THOUSANDTHS + CENTS_DENOMINATOR / 2) / CENTS_DENOMINATOR;
  std::snprintf(out, cap, "$%llu.%02llu", static_cast<unsigned long long>(cents / 100),
                static_cast<unsigned long long>(cents % 100));
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
        renderer.drawLine(cx + px[i] * r / 100, cy + py[i] * r / 100, cx + px[(i + 1) % 10] * r / 100,
                          cy + py[(i + 1) % 10] * r / 100, true);
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
      entries[i].readingSeconds = static_cast<uint32_t>((i + 2) * 47 * 60);
      entries[i].readingSessions = static_cast<uint16_t>(i + 2);
      entries[i].pageTurns = static_cast<uint32_t>((i + 2) * 61);
      entries[i].lastReadDate = journal::today();
      entries[i].readingDays = static_cast<uint16_t>(i + 2);
    }
    count = 3;
    stats.totalReadingSeconds = 63 * 3600 + 5 * 60;
    stats.totalPageTurns = 4919;
    stats.totalSessions = 324;
    stats.workReadingSeconds = 17 * 3600 + 20 * 60;
    stats.currentStreakDays = 39;
    stats.bestStreakDays = 39;
    const uint32_t timeDemo[4] = {8 * 3600, 5 * 3600, 9 * 3600, 18 * 3600};
    const uint32_t weekDemo[7] = {7 * 3600, 12 * 3600, 4 * 3600, 6 * 3600, 5 * 3600, 3 * 3600, 7 * 3600};
    std::copy(std::begin(timeDemo), std::end(timeDemo), std::begin(stats.timeOfDaySeconds));
    std::copy(std::begin(weekDemo), std::end(weekDemo), std::begin(stats.dayOfWeekSeconds));
  }
#endif
  if (stats.totalSessions == 0) journal::loadStats(stats);
  selected = count > 0 ? count - 1 : -1;
  if (!focusPath.empty()) {
    returnToCaller = true;
    for (int i = 0; i < count; ++i) {
      if (std::strncmp(entries[i].path, focusPath.c_str(), journal::kPathBytes) == 0) {
        selected = i;
        view = View::Summary;
        summaryField = 2;
        break;
      }
    }
  }
  const uint32_t currentDate = journal::today();
  if (currentDate != 0) {
    year = static_cast<int>(currentDate / 10000);
    month = static_cast<int>((currentDate / 100) % 100);
    calendarSelectedDate = currentDate;
    const int todayEntry = firstEntryForDate(currentDate);
    if (todayEntry >= 0) selected = todayEntry;
  } else if (selected >= 0) {
    const uint32_t date =
        journal::dateKey(entries[selected].finishedAt ? entries[selected].finishedAt : entries[selected].startedAt);
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
  calendarSelectedDate = 0;
  month += delta;
  if (month < 1) {
    month = 12;
    --year;
  } else if (month > 12) {
    month = 1;
    ++year;
  }
}

bool ReadingJournalActivity::entryMatchesDate(const int index, const uint32_t date) const {
  if (index < 0 || index >= count || date == 0) return false;
  const auto& entry = entries[index];
  return journal::dateKey(entry.startedAt) == date || journal::dateKey(entry.finishedAt) == date ||
         entry.lastReadDate == date || (date == journal::today() && entry.finishedAt == 0);
}

void ReadingJournalActivity::drawViewTabs(const int y) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  constexpr int gap = 8;
  constexpr int height = 38;
  const int width = (sw - side * 2 - gap * 2) / 3;
  static constexpr const char* labels[] = {"CALENDAR", "LIST", "STATS"};
  const View views[] = {View::Calendar, View::List, View::Stats};
  for (int i = 0; i < 3; ++i) {
    viewTabRects[i] = Rect{side + i * (width + gap), y, width, height};
    renderer.drawRoundedRect(viewTabRects[i].x, y, width, height, 6, view == views[i] ? 2 : 1, true);
    UITheme::drawCenteredText(renderer, viewTabRects[i], UI_10_FONT_ID, y + (height - 24) / 2, labels[i], true,
                              EpdFontFamily::BOLD);
  }
}

bool ReadingJournalActivity::handleViewTabTap(const int x, const int y) {
  const View views[] = {View::Calendar, View::List, View::Stats};
  for (int i = 0; i < 3; ++i) {
    const Rect& rect = viewTabRects[i];
    if (x < rect.x || x >= rect.x + rect.width || y < rect.y || y >= rect.y + rect.height) continue;
    view = views[i];
    requestUpdate();
    return true;
  }
  return false;
}

int ReadingJournalActivity::firstEntryForDate(const uint32_t date) const {
  for (int i = count - 1; i >= 0; --i) {
    if (entryMatchesDate(i, date)) return i;
  }
  return -1;
}

void ReadingJournalActivity::selectNext(const int delta) {
  if (count <= 0) return;
  selected = (selected + delta + count) % count;
  const uint32_t date =
      journal::dateKey(entries[selected].finishedAt ? entries[selected].finishedAt : entries[selected].startedAt);
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
  if (editingFinish)
    entries[selected].finishedAt = epoch;
  else
    entries[selected].startedAt = epoch;
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
      if (returnToCaller)
        finish();
      else {
        view = View::Calendar;
        requestUpdate();
      }
    } else {
      if (returnToCaller)
        finish();
      else
        shelf::leave(renderer, mappedInput);
    }
    return;
  }
  if (view == View::Calendar) {
    int tapX = 0;
    int tapY = 0;
    if (mappedInput.wasScreenTapped(tapX, tapY)) {
      if (handleViewTabTap(tapX, tapY)) {
        return;
      } else if (tapX >= calendarGridRect.x && tapX < calendarGridRect.x + calendarGridRect.width &&
                 tapY >= calendarGridRect.y && tapY < calendarGridRect.y + calendarGridRect.height) {
        const int cellW = calendarGridRect.width / 7;
        const int calendarRows = (journal::weekday(year, month, 1) + journal::daysInMonth(year, month) + 6) / 7;
        const int cellH = calendarGridRect.height / calendarRows;
        const int slot = ((tapY - calendarGridRect.y) / cellH) * 7 + (tapX - calendarGridRect.x) / cellW;
        const int day = slot - journal::weekday(year, month, 1) + 1;
        if (day >= 1 && day <= journal::daysInMonth(year, month)) {
          const uint32_t date = static_cast<uint32_t>(year * 10000 + month * 100 + day);
          const int firstMatch = firstEntryForDate(date);
          calendarSelectedDate = date;
          if (firstMatch >= 0) selected = firstMatch;
          requestUpdate();
          return;
        }
      } else if (tapX >= bookCardRect.x && tapX < bookCardRect.x + bookCardRect.width && tapY >= bookCardRect.y &&
                 tapY < bookCardRect.y + bookCardRect.height) {
        constexpr int ROW_HEIGHT = 64;
        const int touched = (tapY - bookCardRect.y) / ROW_HEIGHT;
        const int index = touched >= 0 && touched < calendarVisibleCount ? calendarVisibleEntries[touched] : -1;
        if (index >= 0 && index < count) {
          selected = index;
          summaryField = 0;
          view = View::Summary;
          requestUpdate();
          return;
        }
      }
    }
    const auto swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Left)
      stepMonth(1);
    else if (swipe == MappedInputManager::SwipeDir::Right)
      stepMonth(-1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::PageBack))
      stepMonth(-1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::PageForward))
      stepMonth(1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Left))
      stepMonth(-1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Right))
      stepMonth(1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Up))
      selectNext(-1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Down))
      selectNext(1);
    else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && selected >= 0 &&
             (calendarSelectedDate == 0 || entryMatchesDate(selected, calendarSelectedDate))) {
      summaryField = 0;
      view = View::Summary;
    } else
      return;
  } else if (view == View::List) {
    int tapX = 0;
    int tapY = 0;
    if (mappedInput.wasScreenTapped(tapX, tapY)) {
      if (handleViewTabTap(tapX, tapY)) return;
      for (int i = 0; i < listVisibleCount; ++i) {
        const Rect& rect = listTileRects[i];
        if (tapX >= rect.x && tapX < rect.x + rect.width && tapY >= rect.y && tapY < rect.y + rect.height) {
          selected = listTileEntries[i];
          summaryField = 0;
          view = View::Summary;
          requestUpdate();
          return;
        }
      }
    }
    const auto swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Up || mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      listOffset = std::min(std::max(0, count - 1), listOffset + 1);
    } else if (swipe == MappedInputManager::SwipeDir::Down || mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      listOffset = std::max(0, listOffset - 1);
    } else {
      return;
    }
  } else if (view == View::Stats) {
    int tapX = 0;
    int tapY = 0;
    if (!mappedInput.wasScreenTapped(tapX, tapY) || !handleViewTabTap(tapX, tapY)) return;
  } else if (view == View::Summary) {
    journal::Entry& entry = entries[selected];
    int tapX = 0;
    int tapY = 0;
    if (entry.finishedAt != 0 && mappedInput.wasScreenTapped(tapX, tapY)) {
      const int starCenterY = summaryRowsRect.y + summaryRowsRect.height - 25;
      if (tapY >= starCenterY - 32 && tapY <= starCenterY + 32) {
        const int firstStarX = renderer.getScreenWidth() / 2 - 104;
        const int rating = std::clamp((tapX - firstStarX + 26) / 52 + 1, 1, 5);
        if (tapX >= firstStarX - 26 && tapX <= firstStarX + 4 * 52 + 26 &&
            journal::setRating(entry.path, static_cast<uint8_t>(rating))) {
          entry.rating = static_cast<uint8_t>(rating);
          summaryField = 2;
          requestUpdate();
          return;
        }
      }
    }
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
    if (mappedInput.wasReleased(MappedInputManager::Button::Left) && next > 0)
      --next;
    else if (mappedInput.wasReleased(MappedInputManager::Button::Right) && next < 5)
      ++next;
    else
      return;
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
      if (tappedDay >= 1 && tappedDay <= journal::daysInMonth(editYear, editMonth))
        editDay = tappedDay;
      else
        return;
    } else {
      const auto swipe = mappedInput.wasSwipe();
      if (swipe == MappedInputManager::SwipeDir::Left)
        stepEditDay(1);
      else if (swipe == MappedInputManager::SwipeDir::Right)
        stepEditDay(-1);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Left))
        stepEditDay(-1);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Right))
        stepEditDay(1);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Up))
        stepEditMonth(-1);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Down))
        stepEditMonth(1);
      else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (saveDateEdit()) view = View::Summary;
      } else
        return;
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
    UITheme::drawCenteredText(renderer, Rect{side + col * cellW, top, cellW, 24}, UI_10_FONT_ID, top, kWeek[col], true,
                              EpdFontFamily::BOLD);
  }
  const int first = journal::weekday(year, month, 1);
  const int days = journal::daysInMonth(year, month);
  const int calendarRows = (first + days + 6) / 7;
  for (int day = 1; day <= days; ++day) {
    const int slot = first + day - 1;
    const int col = slot % 7;
    const int row = slot / 7;
    const int x = side + col * cellW;
    const int y = top + 32 + row * cellH;
    char number[4];
    std::snprintf(number, sizeof(number), "%d", day);
    const uint32_t wanted = static_cast<uint32_t>(year * 10000 + month * 100 + day);
    if (calendarSelectedDate == wanted) renderer.drawRect(x + 5, y - 5, cellW - 10, 34, true);
    UITheme::drawCenteredText(renderer, Rect{x, y, cellW, 24}, UI_10_FONT_ID, y + 4, number);
    bool start = false;
    bool finish = false;
    for (int i = 0; i < count; ++i) {
      start = start || journal::dateKey(entries[i].startedAt) == wanted;
      finish = finish || journal::dateKey(entries[i].finishedAt) == wanted;
    }
    if (start) renderer.drawRoundedRect(x + cellW / 2 - (finish ? 12 : 4), y + 31, 8, 8, 2, 4, true);
    if (finish) renderer.fillRoundedRect(x + cellW / 2 + (start ? 4 : -4), y + 31, 8, 8, 4, Black);
  }
  calendarGridRect = Rect{side, top + 32, cellW * 7, cellH * calendarRows};
  const int tabsY = top + 32 + calendarRows * cellH + 4;
  drawViewTabs(tabsY);
  const int cardY = tabsY + 58;
  calendarVisibleCount = 0;
  std::fill(std::begin(calendarVisibleEntries), std::end(calendarVisibleEntries), -1);
  if (calendarSelectedDate != 0) {
    for (int i = count - 1; i >= 0 && calendarVisibleCount < 3; --i) {
      if (entryMatchesDate(i, calendarSelectedDate)) calendarVisibleEntries[calendarVisibleCount++] = i;
    }
  } else {
    const int firstEntry = std::clamp(selected - 1, 0, std::max(0, count - 3));
    for (int i = firstEntry; i < count && calendarVisibleCount < 3; ++i)
      calendarVisibleEntries[calendarVisibleCount++] = i;
  }
  constexpr int ROW_HEIGHT = 64;
  bookCardRect = Rect{side, cardY - 12, sw - side * 2, calendarVisibleCount * ROW_HEIGHT};
  if (calendarVisibleCount > 0) {
    for (int row = 0; row < calendarVisibleCount; ++row) {
      const int index = calendarVisibleEntries[row];
      const int rowY = cardY + row * ROW_HEIGHT;
      const auto& entry = entries[index];
      renderer.drawRoundedRect(side, rowY - 12, sw - side * 2, 56, 8, index == selected ? 2 : 1, true);
      UITheme::drawCenteredWrappedText(renderer, Rect{side + 12, rowY - 8, sw - side * 2 - 24, 24}, UI_10_FONT_ID,
                                       entry.title[0] ? entry.title : entry.path, 1, true, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, side + 12, rowY + 20,
                        entry.finishedAt ? "FINISHED  •  DETAILS" : "READING  •  DETAILS");
    }
  } else {
    UITheme::drawCenteredText(renderer, Rect{side, cardY, sw - side * 2, 80}, UI_12_FONT_ID, cardY,
                              count == 0 ? "OPEN A BOOK TO BEGIN YOUR JOURNAL" : "NO READING ON THIS DAY");
  }
  const auto labels = mappedInput.mapLabels("Back", "Summary", "Books", "Month");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingJournalActivity::drawList() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int side = metrics.contentSidePadding;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, "READING JOURNAL");
  const int tabsY = metrics.topPadding + metrics.headerHeight + 10;
  drawViewTabs(tabsY);
  int y = tabsY + 55;
  listVisibleCount = 0;
  int priorYear = -1;
  int priorMonth = -1;
  for (int position = listOffset; position < count; ++position) {
    const int index = count - 1 - position;
    const auto& entry = entries[index];
    const uint32_t date = journal::dateKey(entry.startedAt);
    const int entryYear = static_cast<int>(date / 10000);
    const int entryMonth = static_cast<int>((date / 100) % 100);
    if (entryYear != priorYear || entryMonth != priorMonth) {
      if (y + 34 > sh - metrics.buttonHintsHeight) break;
      char monthHeading[24];
      if (entryMonth >= 1 && entryMonth <= 12)
        std::snprintf(monthHeading, sizeof(monthHeading), "%s %d", kMonths[entryMonth - 1], entryYear);
      else
        std::snprintf(monthHeading, sizeof(monthHeading), "DATE UNKNOWN");
      renderer.drawText(UI_10_FONT_ID, side, y, monthHeading, true, EpdFontFamily::BOLD);
      y += 34;
      priorYear = entryYear;
      priorMonth = entryMonth;
    }
    constexpr int TILE_H = 68;
    if (y + TILE_H > sh - metrics.buttonHintsHeight) break;
    renderer.drawRoundedRect(side, y, sw - side * 2, TILE_H - 8, 8, 1, true);
    if (listVisibleCount < kMaxVisibleListTiles) {
      listTileRects[listVisibleCount] = Rect{side, y, sw - side * 2, TILE_H - 8};
      listTileEntries[listVisibleCount] = index;
      ++listVisibleCount;
    }
    UITheme::drawCenteredWrappedText(renderer, Rect{side + 12, y + 6, sw - side * 2 - 24, 24}, UI_10_FONT_ID,
                                     entry.title[0] ? entry.title : entry.path, 1, true, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, side + 12, y + 34, entry.finishedAt ? "FINISHED" : "READING");
    y += TILE_H;
  }
  if (count == 0)
    UITheme::drawCenteredText(renderer, Rect{side, y + 40, sw - side * 2, 80}, UI_12_FONT_ID, y + 60,
                              "OPEN A BOOK TO BEGIN YOUR JOURNAL");
  const auto labels = mappedInput.mapLabels("Back", "", "Scroll", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingJournalActivity::drawStats() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, "READING STATS");
  const int tabsY = metrics.topPadding + metrics.headerHeight + 10;
  drawViewTabs(tabsY);

  int booksRead = 0;
  for (int i = 0; i < count; ++i)
    if (entries[i].finishedAt != 0) booksRead++;
  char readingTime[24];
  char workReading[24];
  formatReadingTime(stats.totalReadingSeconds, readingTime, sizeof(readingTime));
  formatWorkReadingPay(stats.workReadingSeconds, workReading, sizeof(workReading));
  char values[6][24];
  std::snprintf(values[0], sizeof(values[0]), "%lu", static_cast<unsigned long>(stats.totalSessions));
  std::snprintf(values[1], sizeof(values[1]), "%s", readingTime);
  std::snprintf(values[2], sizeof(values[2]), "%lu", static_cast<unsigned long>(stats.totalPageTurns));
  std::snprintf(values[3], sizeof(values[3]), "%s", workReading);
  std::snprintf(values[4], sizeof(values[4]), "%u days", stats.currentStreakDays);
  std::snprintf(values[5], sizeof(values[5]), "%d", booksRead);
  static constexpr const char* labelLine1[] = {"SESSIONS", "READING", "PAGES", "WORK", "READING", "BOOKS"};
  static constexpr const char* labelLine2[] = {"", "TIME", "READ", "READING", "STREAK", "READ"};
  const int cardsY = tabsY + 54;
  const int cardW = (sw - side * 2) / 3;
  for (int i = 0; i < 6; ++i) {
    const int x = side + (i % 3) * cardW;
    const int y = cardsY + (i / 3) * 74;
    UITheme::drawCenteredText(renderer, Rect{x, y, cardW, 28}, UI_12_FONT_ID, y, values[i], true,
                              EpdFontFamily::BOLD);
    if (labelLine2[i][0] == '\0') {
      UITheme::drawCenteredText(renderer, Rect{x + 4, y + 34, cardW - 8, 24}, UI_10_FONT_ID, y + 34,
                                labelLine1[i]);
    } else {
      UITheme::drawCenteredText(renderer, Rect{x + 4, y + 27, cardW - 8, 24}, UI_10_FONT_ID, y + 27,
                                labelLine1[i]);
      UITheme::drawCenteredText(renderer, Rect{x + 4, y + 49, cardW - 8, 24}, UI_10_FONT_ID, y + 49,
                                labelLine2[i]);
    }
  }

  const auto drawBars = [&](const int top, const char* title, const char* const* names, const uint32_t* values,
                            const int valueCount) {
    constexpr int rowStride = 32;
    renderer.drawRect(side, top, sw - side * 2, 34 + valueCount * rowStride, true);
    UITheme::drawCenteredText(renderer, Rect{side, top + 4, sw - side * 2, 24}, UI_10_FONT_ID, top + 4, title, true,
                              EpdFontFamily::BOLD);
    uint32_t maximum = 1;
    for (int i = 0; i < valueCount; ++i) maximum = std::max(maximum, values[i]);
    const int labelW = 128;
    const int barX = side + labelW;
    const int barMax = sw - side - barX - 12;
    for (int i = 0; i < valueCount; ++i) {
      const int rowY = top + 38 + i * rowStride;
      renderer.drawText(UI_10_FONT_ID, side + 8, rowY, names[i]);
      const int width = static_cast<int>((static_cast<uint64_t>(values[i]) * barMax) / maximum);
      if (width > 0) renderer.fillRect(barX, rowY + 5, width, 10, Black);
    }
  };
  static constexpr const char* timeNames[] = {"Morning", "Afternoon", "Evening", "Night"};
  static constexpr const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const int timeY = cardsY + 165;
  drawBars(timeY, "TIME OF DAY", timeNames, stats.timeOfDaySeconds, 4);
  drawBars(timeY + 174, "DAY OF WEEK", dayNames, stats.dayOfWeekSeconds, 7);
  const auto hints = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
}

void ReadingJournalActivity::drawSummary() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  const auto& entry = entries[selected];
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, "BOOK SUMMARY");

#ifndef JOURNAL_BOOK_STATS_VARIANT
#define JOURNAL_BOOK_STATS_VARIANT 1
#endif
  const int contentY = metrics.topPadding + metrics.headerHeight + 18;
  int variant = JOURNAL_BOOK_STATS_VARIANT;
#if defined(SIMULATOR)
  if (const char* preview = std::getenv("CROSSPLAY_JOURNAL_VARIANT")) variant = std::clamp(std::atoi(preview), 1, 3);
#endif
  const int coverW = variant == 3 ? 86 : variant == 2 ? 112 : 122;
  const int coverH = variant == 3 ? 132 : variant == 2 ? 174 : 190;
  const int statsY = variant == 2 ? contentY : contentY + coverH + (variant == 3 ? 12 : 14);
  const int heroY = variant == 2 ? statsY + 166 : contentY;

  const auto drawBookStats = [&]() {
    constexpr int panelH = 154;
    const int panelW = sw - side * 2;
    char readingTime[24];
    char sessions[16];
    char pages[16];
    char readingDays[16];
    char workReading[24];
    char rating[16];
    formatReadingTime(entry.readingSeconds, readingTime, sizeof(readingTime));
    std::snprintf(sessions, sizeof(sessions), "%u", entry.readingSessions);
    std::snprintf(pages, sizeof(pages), "%lu", static_cast<unsigned long>(entry.pageTurns));
    std::snprintf(readingDays, sizeof(readingDays), "%u", entry.readingDays);
    formatWorkReadingPay(entry.workReadingSeconds, workReading, sizeof(workReading));
    if (entry.rating > 0)
      std::snprintf(rating, sizeof(rating), "%u / 5", entry.rating);
    else
      std::snprintf(rating, sizeof(rating), "—");
    const char* values[] = {sessions, readingTime, pages, readingDays, workReading, rating};
    const char* labels1[] = {"SESSIONS", "READING", "PAGES", "READING", "WORK", "RATING"};
    const char* labels2[] = {"", "TIME", "READ", "DAYS", "READING", ""};
    renderer.drawRect(side, statsY, panelW, panelH, true);
    for (int i = 0; i < 6; ++i) {
      const bool secondRow = i >= 3;
      const int row = secondRow ? 1 : 0;
      constexpr int columns = 3;
      const int width = panelW / columns;
      const int column = secondRow ? i - 3 : i;
      const int x = side + column * width;
      const int y = statsY + 14 + row * 72;
      UITheme::drawCenteredText(renderer, Rect{x, y, width, 25}, UI_10_FONT_ID, y, values[i], true,
                                EpdFontFamily::BOLD);
      UITheme::drawCenteredText(renderer, Rect{x + 3, y + 28, width - 6, 20}, UI_10_FONT_ID, y + 28, labels1[i]);
      if (labels2[i][0] != '\0')
        UITheme::drawCenteredText(renderer, Rect{x + 3, y + 47, width - 6, 20}, UI_10_FONT_ID, y + 47, labels2[i]);
    }
  };

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
  UITheme::drawCenteredWrappedText(renderer, Rect{textX, heroY + 10, textW, 82}, UI_12_FONT_ID, entry.title, 3, true,
                                   EpdFontFamily::BOLD);
  UITheme::drawCenteredWrappedText(renderer, Rect{textX, heroY + coverH - 72, textW, 56}, UI_10_FONT_ID,
                                   entry.author[0] ? entry.author : "UNKNOWN AUTHOR", 2);
  drawBookStats();

  const int contentBottom = std::max(heroY + coverH, statsY + 154);
  int y = contentBottom + 16;
  constexpr int rowH = 58;
  summaryRowsRect = Rect{side, y - 6, sw - side * 2, rowH * 3};
  char stamp[40];
  formatStamp(entry.startedAt, stamp, sizeof(stamp));
  if (summaryField == 0) renderer.drawRoundedRect(side - 8, y - 6, sw - side * 2 + 16, rowH, 7, 2, true);
  renderer.drawText(UI_10_FONT_ID, side, y, "STARTED", true, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, side + 112, y, stamp);
  y += rowH;
  formatStamp(entry.finishedAt, stamp, sizeof(stamp));
  if (summaryField == 1) renderer.drawRoundedRect(side - 8, y - 6, sw - side * 2 + 16, rowH, 7, 2, true);
  renderer.drawText(UI_10_FONT_ID, side, y, "FINISHED", true, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, side + 112, y, stamp);
  y += rowH;
  if (summaryField == 2) renderer.drawRoundedRect(side - 8, y - 6, sw - side * 2 + 16, rowH, 7, 2, true);
  UITheme::drawCenteredText(renderer, Rect{side, y - 2, sw - side * 2, 22}, UI_10_FONT_ID, y - 2,
                            entry.finishedAt ? "YOUR RATING" : "RATE WHEN FINISHED", true, EpdFontFamily::BOLD);
  for (int i = 0; i < 5; ++i) drawStar(renderer, sw / 2 - 104 + i * 52, y + 36, 16, i < entry.rating);
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
    UITheme::drawCenteredText(renderer, Rect{side + col * cellW, gridY, cellW, 24}, UI_10_FONT_ID, gridY, kWeek[col],
                              true, EpdFontFamily::BOLD);
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
  renderer.drawRoundedRect(dateCancelRect.x, dateCancelRect.y, dateCancelRect.width, dateCancelRect.height, 7, 2, true);
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
  if (view == View::Calendar)
    drawCalendar();
  else if (view == View::List)
    drawList();
  else if (view == View::Stats)
    drawStats();
  else if (view == View::Summary)
    drawSummary();
  else
    drawDateEditor();
  renderer.displayBuffer();
}
