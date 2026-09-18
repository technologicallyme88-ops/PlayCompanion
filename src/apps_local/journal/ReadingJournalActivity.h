#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../../components/themes/BaseTheme.h"
#include "ReadingJournal.h"

class ReadingJournalActivity final : public Activity {
 public:
  ReadingJournalActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingJournal", renderer, mappedInput) {}
  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Calendar, Summary, DateEditor };
  void stepMonth(int delta);
  void selectNext(int delta);
  void drawCalendar();
  void drawSummary();
  void drawDateEditor();
  void beginDateEdit(bool finish);
  bool saveDateEdit();
  void stepEditDay(int delta);
  void stepEditMonth(int delta);

  std::unique_ptr<journal::Entry[]> entries;
  int count = 0;
  int selected = -1;
  int year = 2026;
  int month = 1;
  View view = View::Calendar;
  int summaryField = 0;  // 0 started, 1 finished, 2 rating
  bool editingFinish = false;
  int editYear = 2026;
  int editMonth = 1;
  int editDay = 1;
  Rect bookCardRect{};
  Rect summaryRowsRect{};
  Rect dateGridRect{};
  Rect dateCancelRect{};
  Rect dateConfirmRect{};
  Rect clearFinishRect{};
};
