#include "ReadingJournal.h"

#include <HalStorage.h>
#include <HalClock.h>
#include <Logging.h>
#include <Memory.h>

#include "CrossPointSettings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace journal {
namespace {

constexpr char kPath[] = "/.crosspoint/reading-journal.bin";
constexpr uint32_t kMagic = 0x314A5243;  // CRJ1
constexpr uint32_t kEarliestValidEpoch = 1577836800u;  // 2020-01-01

struct Header {
  uint32_t magic;
  uint16_t count;
  uint16_t entrySize;
};

uint32_t nowEpoch() {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.getUtcDateTime(year, month, day, hour, minute) || year < 2020 || month < 1 || month > 12 || day < 1 ||
      day > 31) {
    return 0;
  }

  // Convert the RTC's UTC fields without relying on time(), which remains near
  // the Unix epoch until SNTP has run even when the hardware RTC is valid.
  int y = year;
  const unsigned m = month;
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = static_cast<int64_t>(era) * 146097 + doe - 719468;
  int64_t seconds = days * 86400 + hour * 3600 + minute * 60;
  const int offsetQ = std::clamp(static_cast<int>(SETTINGS.clockUtcOffsetQ), 0, 104) - 48;
  seconds += static_cast<int64_t>(offsetQ) * 15 * 60;
  return seconds >= kEarliestValidEpoch && seconds <= UINT32_MAX ? static_cast<uint32_t>(seconds) : 0;
}

void copyText(char* dst, const size_t cap, const char* src) {
  if (cap == 0) return;
  std::snprintf(dst, cap, "%s", src ? src : "");
}

bool save(const Entry* entries, const int count) {
  HalFile file;
  if (!Storage.openFileForWrite("JRNL", kPath, file)) {
    LOG_ERR("JRNL", "Could not open journal for write");
    return false;
  }
  const Header header{kMagic, static_cast<uint16_t>(count), static_cast<uint16_t>(sizeof(Entry))};
  if (file.write(&header, sizeof(header)) != sizeof(header) ||
      (count > 0 && file.write(entries, static_cast<size_t>(count) * sizeof(Entry)) !=
                        static_cast<size_t>(count) * sizeof(Entry))) {
    LOG_ERR("JRNL", "Could not write journal");
    return false;
  }
  return true;
}

bool update(const char* path, const char* title, const char* author, const bool finish, const int rating) {
  auto entries = makeUniqueNoThrow<Entry[]>(kMaxEntries);
  if (!entries) {
    LOG_ERR("JRNL", "OOM: journal entries");
    return false;
  }
  int count = load(entries.get(), kMaxEntries);
  int found = -1;
  bool added = false;
  bool changed = false;
  for (int i = 0; i < count; ++i) {
    if (std::strncmp(entries[i].path, path, kPathBytes) == 0) {
      found = i;
      break;
    }
  }
  if (found < 0) {
    if (finish || count >= kMaxEntries) return false;
    found = count++;
    added = true;
    copyText(entries[found].path, kPathBytes, path);
    copyText(entries[found].title, kTitleBytes, title);
    copyText(entries[found].author, kAuthorBytes, author);
    entries[found].startedAt = nowEpoch();
  } else if (!finish && entries[found].startedAt < kEarliestValidEpoch) {
    const uint32_t now = nowEpoch();
    if (now != 0) {
      entries[found].startedAt = now;
      changed = true;
    }
  }
  if (finish && entries[found].finishedAt == 0) {
    entries[found].finishedAt = nowEpoch();
    changed = true;
  }
  if (rating >= 0 && entries[found].rating != rating) {
    entries[found].rating = static_cast<uint8_t>(rating);
    changed = true;
  }
  return (changed || added) ? save(entries.get(), count) : true;
}

}  // namespace

int load(Entry* entries, const int capacity) {
  if (!entries || capacity <= 0 || !Storage.exists(kPath)) return 0;
  HalFile file;
  if (!Storage.openFileForRead("JRNL", kPath, file)) return 0;
  Header header{};
  if (file.read(&header, sizeof(header)) != sizeof(header) || header.magic != kMagic ||
      header.entrySize != sizeof(Entry)) {
    LOG_ERR("JRNL", "Ignoring invalid journal file");
    return 0;
  }
  const int count = std::min<int>(header.count, capacity);
  const size_t bytes = static_cast<size_t>(count) * sizeof(Entry);
  if (file.read(entries, bytes) != static_cast<int>(bytes)) {
    LOG_ERR("JRNL", "Journal file is truncated");
    return 0;
  }
  return count;
}

bool noteStarted(const char* path, const char* title, const char* author) {
  return path && *path && update(path, title, author, false, -1);
}

bool noteFinished(const char* path) { return path && *path && update(path, nullptr, nullptr, true, -1); }

bool setRating(const char* path, uint8_t rating) {
  if (rating > 5) rating = 5;
  return path && *path && update(path, nullptr, nullptr, false, rating);
}

bool setDateField(const char* path, const uint32_t date, const bool finish) {
  if (!path || !*path) return false;
  const uint32_t epoch = epochForDate(static_cast<int>(date / 10000), static_cast<int>((date / 100) % 100),
                                      static_cast<int>(date % 100));
  if (epoch == 0) return false;
  auto entries = makeUniqueNoThrow<Entry[]>(kMaxEntries);
  if (!entries) {
    LOG_ERR("JRNL", "OOM: journal date update");
    return false;
  }
  const int count = load(entries.get(), kMaxEntries);
  for (int i = 0; i < count; ++i) {
    if (std::strncmp(entries[i].path, path, kPathBytes) != 0) continue;
    if (finish) {
      entries[i].finishedAt = epoch;
    } else {
      entries[i].startedAt = epoch;
    }
    return save(entries.get(), count);
  }
  return false;
}

bool setStartedDate(const char* path, const uint32_t date) { return setDateField(path, date, false); }

bool setFinishedDate(const char* path, const uint32_t date) { return setDateField(path, date, true); }

bool clearFinishedDate(const char* path) {
  if (!path || !*path) return false;
  auto entries = makeUniqueNoThrow<Entry[]>(kMaxEntries);
  if (!entries) {
    LOG_ERR("JRNL", "OOM: clear journal finish date");
    return false;
  }
  const int count = load(entries.get(), kMaxEntries);
  for (int i = 0; i < count; ++i) {
    if (std::strncmp(entries[i].path, path, kPathBytes) != 0) continue;
    entries[i].finishedAt = 0;
    entries[i].rating = 0;
    return save(entries.get(), count);
  }
  return false;
}

bool updatePath(const char* oldPath, const char* newPath) {
  if (!oldPath || !newPath || !*oldPath || !*newPath) return false;
  auto entries = makeUniqueNoThrow<Entry[]>(kMaxEntries);
  if (!entries) {
    LOG_ERR("JRNL", "OOM: journal path update");
    return false;
  }
  const int count = load(entries.get(), kMaxEntries);
  for (int i = 0; i < count; ++i) {
    if (std::strncmp(entries[i].path, oldPath, kPathBytes) == 0) {
      copyText(entries[i].path, kPathBytes, newPath);
      return save(entries.get(), count);
    }
  }
  return true;
}

uint32_t dateKey(const uint32_t epoch) {
  if (epoch < kEarliestValidEpoch) return 0;
  const time_t raw = static_cast<time_t>(epoch);
  struct tm parts {};
  localtime_r(&raw, &parts);
  return static_cast<uint32_t>((parts.tm_year + 1900) * 10000 + (parts.tm_mon + 1) * 100 + parts.tm_mday);
}

uint32_t today() { return dateKey(nowEpoch()); }

int daysInMonth(const int year, const int month) {
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 30;
  if (month != 2) return days[month - 1];
  return 28 + ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 1 : 0);
}

int weekday(const int year, const int month, const int day) {
  int y = year;
  int m = month;
  if (m < 3) {
    m += 12;
    --y;
  }
  const int value = (day + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
  return (value + 6) % 7;
}

uint32_t epochForDate(const int year, const int month, const int day) {
  if (year < 2020 || month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)) return 0;
  int y = year - (month <= 2 ? 1 : 0);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned adjustedMonth = static_cast<unsigned>(month + (month > 2 ? -3 : 9));
  const unsigned doy = (153 * adjustedMonth + 2) / 5 + static_cast<unsigned>(day - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = static_cast<int64_t>(era) * 146097 + doe - 719468;
  const int64_t seconds = days * 86400 + 12 * 3600;  // noon avoids date rollover at every supported UTC offset
  return seconds >= kEarliestValidEpoch && seconds <= UINT32_MAX ? static_cast<uint32_t>(seconds) : 0;
}

}  // namespace journal
