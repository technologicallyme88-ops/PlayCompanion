#pragma once

#include <cstddef>
#include <cstdint>

namespace journal {

constexpr int kMaxEntries = 64;
constexpr size_t kTitleBytes = 72;
constexpr size_t kAuthorBytes = 48;
constexpr size_t kPathBytes = 128;

struct Entry {
  char path[kPathBytes] = {};
  char title[kTitleBytes] = {};
  char author[kAuthorBytes] = {};
  uint32_t startedAt = 0;
  uint32_t finishedAt = 0;
  uint8_t rating = 0;
  uint32_t readingSeconds = 0;
  uint16_t readingSessions = 0;
  uint32_t pageTurns = 0;
  uint32_t lastReadDate = 0;
  uint16_t readingDays = 0;
  uint32_t workReadingSeconds = 0;
};

struct Stats {
  uint32_t totalReadingSeconds = 0;
  uint32_t totalPageTurns = 0;
  uint32_t totalSessions = 0;
  uint32_t workReadingSeconds = 0;
  uint16_t currentStreakDays = 0;
  uint16_t bestStreakDays = 0;
  uint32_t timeOfDaySeconds[4] = {};
  uint32_t dayOfWeekSeconds[7] = {};
};

// These are deliberately transition-based: opening an already-known book and
// repainting its end screen do not rewrite the SD card.
bool noteStarted(const char* path, const char* title, const char* author);
bool noteFinished(const char* path);
bool setRating(const char* path, uint8_t rating);
bool setStartedDate(const char* path, uint32_t date);
bool setFinishedDate(const char* path, uint32_t date);
bool clearFinishedDate(const char* path);
bool updatePath(const char* oldPath, const char* newPath);
int load(Entry* entries, int capacity);
bool loadStats(Stats& stats);

// ReaderActivity feeds successful navigation into this tracker. Time is
// credited only inside the same five-minute active window used by Companion,
// so an abandoned open book cannot inflate the journal.
void beginReadingSession(const char* path);
void notePageTurn();
void tickReadingSession();
void endReadingSession();

uint32_t dateKey(uint32_t epoch);
uint32_t today();
int daysInMonth(int year, int month);
int weekday(int year, int month, int day);  // Sunday = 0
uint32_t epochForDate(int year, int month, int day);

}  // namespace journal
