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

uint32_t dateKey(uint32_t epoch);
uint32_t today();
int daysInMonth(int year, int month);
int weekday(int year, int month, int day);  // Sunday = 0
uint32_t epochForDate(int year, int month, int day);

}  // namespace journal
