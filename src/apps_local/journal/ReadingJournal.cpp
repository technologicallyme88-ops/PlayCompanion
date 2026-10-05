#include "ReadingJournal.h"

#include <HalStorage.h>
#include <HalClock.h>
#include <Logging.h>
#include <Memory.h>
#include <CompanionMood.h>
#include <Arduino.h>

#include "CrossPointSettings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iterator>

namespace journal {
namespace {

constexpr char kPath[] = "/.crosspoint/reading-journal.bin";
constexpr char kStatsPath[] = "/.crosspoint/reading-stats.bin";
constexpr char kStatsTmpPath[] = "/.crosspoint/reading-stats.bin.tmp";
constexpr uint32_t kMagic = 0x314A5243;  // CRJ1
constexpr uint32_t kStatsMagic = 0x31535243;  // CRS1
constexpr uint32_t kEarliestValidEpoch = 1577836800u;  // 2020-01-01

struct Header {
  uint32_t magic;
  uint16_t count;
  uint16_t entrySize;
};

// Layout written before per-book analytics were introduced. Header::entrySize
// lets current firmware migrate it without discarding the user's journal.
struct LegacyEntry {
  char path[kPathBytes] = {};
  char title[kTitleBytes] = {};
  char author[kAuthorBytes] = {};
  uint32_t startedAt = 0;
  uint32_t finishedAt = 0;
  uint8_t rating = 0;
};

// Per-book analytics layout written before distinct reading days were tracked.
struct AnalyticsEntryV1 {
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
};

// Per-book analytics layout written before work-window reading was tracked.
struct AnalyticsEntryV2 {
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
};

struct StatsV1 {
  uint32_t totalReadingSeconds = 0;
  uint32_t totalPageTurns = 0;
  uint32_t totalSessions = 0;
  uint16_t currentStreakDays = 0;
  uint16_t bestStreakDays = 0;
  uint32_t timeOfDaySeconds[4] = {};
  uint32_t dayOfWeekSeconds[7] = {};
};

struct StatsFileV1 {
  uint32_t magic = kStatsMagic;
  uint16_t version = 1;
  uint16_t size = 0;
  StatsV1 stats{};
  int32_t lastReadDay = companion::DayLedger::NEVER;
};

struct StatsFile {
  uint32_t magic = kStatsMagic;
  uint16_t version = 2;
  uint16_t size = 0;
  Stats stats{};
  int32_t lastReadDay = companion::DayLedger::NEVER;
};

struct ActiveSession {
  companion::SessionAccumulator accumulator;
  char path[kPathBytes] = {};
  uint32_t startedSeconds = 0;
  uint32_t openingSeconds = 0;
  uint32_t pageTurns = 0;
  uint32_t workReadingSeconds = 0;
  uint8_t localHour = 0;
  uint8_t weekday = 0;
  int32_t localDay = companion::DayLedger::NEVER;
  bool active = false;
};

StatsFile statsFile;
bool statsLoaded = false;
ActiveSession activeSession;

uint32_t saturatedAdd(const uint32_t lhs, const uint32_t rhs) {
  return UINT32_MAX - lhs < rhs ? UINT32_MAX : lhs + rhs;
}

constexpr bool isWorkReadingTime(const uint8_t weekday, const uint8_t hour) {
  return (hour >= 18 && weekday <= 3) || (hour < 6 && weekday >= 1 && weekday <= 4);
}

static_assert(isWorkReadingTime(0, 18));  // Sunday evening
static_assert(isWorkReadingTime(4, 5));   // Thursday morning
static_assert(!isWorkReadingTime(4, 18));
static_assert(!isWorkReadingTime(0, 5));

int signedUtcOffsetQuarterHours() {
  return std::clamp(static_cast<int>(SETTINGS.clockUtcOffsetQ), 0, 104) - 48;
}

bool readLocalTime(int32_t& localDay, uint8_t& localHour, uint8_t& weekday) {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.getUtcDateTime(year, month, day, hour, minute)) return false;
  const int offsetMinutes = signedUtcOffsetQuarterHours() * 15;
  const int localMinutes = static_cast<int>(hour) * 60 + minute + offsetMinutes;
  const int normalizedLocalMinutes = ((localMinutes % 1440) + 1440) % 1440;
  localDay = companion::localDayNumber(year, month, day, hour, minute, signedUtcOffsetQuarterHours());
  localHour = static_cast<uint8_t>(normalizedLocalMinutes / 60);
  weekday = static_cast<uint8_t>(((localDay + 4) % 7 + 7) % 7);  // 1970-01-01 was Thursday.
  return true;
}

bool ensureStatsLoaded() {
  if (statsLoaded) return true;
  statsLoaded = true;
  HalFile file;
  if (!Storage.openFileForRead("JRNL", kStatsPath, file)) return true;
  struct FileHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
  } header{};
  if (file.read(&header, sizeof(header)) != sizeof(header) || header.magic != kStatsMagic || !file.seekSet(0)) {
    LOG_ERR("JRNL", "Ignoring invalid reading stats file");
    return false;
  }
  if (header.version == 2 && header.size == sizeof(StatsFile)) {
    StatsFile loaded{};
    if (file.read(&loaded, sizeof(loaded)) != sizeof(loaded)) {
      LOG_ERR("JRNL", "Reading stats file is truncated");
      return false;
    }
    statsFile = loaded;
  } else if (header.version == 1 && header.size == sizeof(StatsFileV1)) {
    StatsFileV1 old{};
    if (file.read(&old, sizeof(old)) != sizeof(old)) {
      LOG_ERR("JRNL", "Legacy reading stats file is truncated");
      return false;
    }
    statsFile.stats.totalReadingSeconds = old.stats.totalReadingSeconds;
    statsFile.stats.totalPageTurns = old.stats.totalPageTurns;
    statsFile.stats.totalSessions = old.stats.totalSessions;
    statsFile.stats.currentStreakDays = old.stats.currentStreakDays;
    statsFile.stats.bestStreakDays = old.stats.bestStreakDays;
    std::copy(std::begin(old.stats.timeOfDaySeconds), std::end(old.stats.timeOfDaySeconds),
              std::begin(statsFile.stats.timeOfDaySeconds));
    std::copy(std::begin(old.stats.dayOfWeekSeconds), std::end(old.stats.dayOfWeekSeconds),
              std::begin(statsFile.stats.dayOfWeekSeconds));
    statsFile.lastReadDay = old.lastReadDay;
  } else {
    LOG_ERR("JRNL", "Ignoring unsupported reading stats file");
    return false;
  }
  return true;
}

bool saveStats() {
  statsFile.size = static_cast<uint16_t>(sizeof(StatsFile));
  HalFile file;
  if (!Storage.openFileForWrite("JRNL", kStatsTmpPath, file)) return false;
  if (file.write(&statsFile, sizeof(statsFile)) != sizeof(statsFile)) {
    LOG_ERR("JRNL", "Could not write reading stats");
    return false;
  }
  file.close();  // The temporary file must be closed before the atomic rename.
  Storage.remove(kStatsPath);
  if (!Storage.rename(kStatsTmpPath, kStatsPath)) {
    LOG_ERR("JRNL", "Could not install reading stats");
    return false;
  }
  return true;
}

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
      (header.entrySize != sizeof(Entry) && header.entrySize != sizeof(AnalyticsEntryV2) &&
       header.entrySize != sizeof(AnalyticsEntryV1) && header.entrySize != sizeof(LegacyEntry))) {
    LOG_ERR("JRNL", "Ignoring invalid journal file");
    return 0;
  }
  const int count = std::min<int>(header.count, capacity);
  if (header.entrySize == sizeof(Entry)) {
    const size_t bytes = static_cast<size_t>(count) * sizeof(Entry);
    if (file.read(entries, bytes) != static_cast<int>(bytes)) {
      LOG_ERR("JRNL", "Journal file is truncated");
      return 0;
    }
  } else if (header.entrySize == sizeof(AnalyticsEntryV2)) {
    for (int i = 0; i < count; ++i) {
      AnalyticsEntryV2 old{};
      if (file.read(&old, sizeof(old)) != sizeof(old)) {
        LOG_ERR("JRNL", "Reading-days journal file is truncated");
        return 0;
      }
      copyText(entries[i].path, kPathBytes, old.path);
      copyText(entries[i].title, kTitleBytes, old.title);
      copyText(entries[i].author, kAuthorBytes, old.author);
      entries[i].startedAt = old.startedAt;
      entries[i].finishedAt = old.finishedAt;
      entries[i].rating = old.rating;
      entries[i].readingSeconds = old.readingSeconds;
      entries[i].readingSessions = old.readingSessions;
      entries[i].pageTurns = old.pageTurns;
      entries[i].lastReadDate = old.lastReadDate;
      entries[i].readingDays = old.readingDays;
    }
  } else if (header.entrySize == sizeof(AnalyticsEntryV1)) {
    for (int i = 0; i < count; ++i) {
      AnalyticsEntryV1 old{};
      if (file.read(&old, sizeof(old)) != sizeof(old)) {
        LOG_ERR("JRNL", "Analytics journal file is truncated");
        return 0;
      }
      copyText(entries[i].path, kPathBytes, old.path);
      copyText(entries[i].title, kTitleBytes, old.title);
      copyText(entries[i].author, kAuthorBytes, old.author);
      entries[i].startedAt = old.startedAt;
      entries[i].finishedAt = old.finishedAt;
      entries[i].rating = old.rating;
      entries[i].readingSeconds = old.readingSeconds;
      entries[i].readingSessions = old.readingSessions;
      entries[i].pageTurns = old.pageTurns;
      entries[i].lastReadDate = old.lastReadDate;
      entries[i].readingDays = old.lastReadDate != 0 ? 1 : 0;
    }
  } else {
    for (int i = 0; i < count; ++i) {
      LegacyEntry old{};
      if (file.read(&old, sizeof(old)) != sizeof(old)) {
        LOG_ERR("JRNL", "Legacy journal file is truncated");
        return 0;
      }
      copyText(entries[i].path, kPathBytes, old.path);
      copyText(entries[i].title, kTitleBytes, old.title);
      copyText(entries[i].author, kAuthorBytes, old.author);
      entries[i].startedAt = old.startedAt;
      entries[i].finishedAt = old.finishedAt;
      entries[i].rating = old.rating;
    }
  }
  return count;
}

bool loadStats(Stats& stats) {
  ensureStatsLoaded();
  stats = statsFile.stats;
  int32_t todayDay = companion::DayLedger::NEVER;
  uint8_t hour = 0;
  uint8_t weekday = 0;
  if (readLocalTime(todayDay, hour, weekday) && statsFile.lastReadDay != companion::DayLedger::NEVER &&
      todayDay > statsFile.lastReadDay + 1)
    stats.currentStreakDays = 0;
  return true;
}

bool noteStarted(const char* path, const char* title, const char* author) {
  return path && *path && update(path, title, author, false, -1);
}

bool noteFinished(const char* path) { return path && *path && update(path, nullptr, nullptr, true, -1); }

bool setRating(const char* path, uint8_t rating) {
  if (rating > 5) rating = 5;
  return path && *path && update(path, nullptr, nullptr, false, rating);
}

void beginReadingSession(const char* path) {
  activeSession.accumulator.reset();
  activeSession.path[0] = '\0';
  activeSession.startedSeconds = 0;
  activeSession.openingSeconds = 0;
  activeSession.pageTurns = 0;
  activeSession.workReadingSeconds = 0;
  activeSession.localHour = 0;
  activeSession.weekday = 0;
  activeSession.localDay = companion::DayLedger::NEVER;
  activeSession.active = false;
  if (!path || !*path) return;
  copyText(activeSession.path, kPathBytes, path);
  activeSession.startedSeconds = millis() / 1000;
  readLocalTime(activeSession.localDay, activeSession.localHour, activeSession.weekday);
  activeSession.active = true;
}

void notePageTurn() {
  if (!activeSession.active) return;
  const uint32_t now = millis() / 1000;
  if (activeSession.pageTurns == 0 && activeSession.accumulator.creditedSeconds() == 0) {
    constexpr uint32_t OPENING_PAGE_CAP_SECONDS = 300;
    const uint32_t opening = now - activeSession.startedSeconds;
    activeSession.openingSeconds = std::min(opening, OPENING_PAGE_CAP_SECONDS);
  }
  activeSession.accumulator.onPageTurn(now);
  activeSession.pageTurns = saturatedAdd(activeSession.pageTurns, 1);
}

void tickReadingSession() {
  if (!activeSession.active) return;
  const uint32_t before = activeSession.accumulator.creditedSeconds();
  activeSession.accumulator.onTick(millis() / 1000);
  const uint32_t credited = activeSession.accumulator.creditedSeconds();
  if (credited <= before) return;
  int32_t localDay = companion::DayLedger::NEVER;
  uint8_t localHour = 0;
  uint8_t weekday = 0;
  if (readLocalTime(localDay, localHour, weekday) && isWorkReadingTime(weekday, localHour))
    activeSession.workReadingSeconds = saturatedAdd(activeSession.workReadingSeconds, credited - before);
}

void endReadingSession() {
  if (!activeSession.active) return;
  tickReadingSession();
  const uint32_t pages = activeSession.pageTurns;
  const uint32_t seconds = saturatedAdd(activeSession.accumulator.creditedSeconds(), activeSession.openingSeconds);
  const bool openingAtWork = activeSession.localDay != companion::DayLedger::NEVER &&
                             isWorkReadingTime(activeSession.weekday, activeSession.localHour);
  const uint32_t workSeconds =
      saturatedAdd(activeSession.workReadingSeconds, openingAtWork ? activeSession.openingSeconds : 0);
  activeSession.active = false;
  if (pages == 0) return;

  ensureStatsLoaded();
  Stats& stats = statsFile.stats;
  stats.totalSessions = saturatedAdd(stats.totalSessions, 1);
  stats.totalPageTurns = saturatedAdd(stats.totalPageTurns, pages);
  stats.totalReadingSeconds = saturatedAdd(stats.totalReadingSeconds, seconds);
  stats.workReadingSeconds = saturatedAdd(stats.workReadingSeconds, workSeconds);
  if (activeSession.localDay != companion::DayLedger::NEVER) {
    const uint8_t bucket = activeSession.localHour < 5    ? 3
                           : activeSession.localHour < 12 ? 0
                           : activeSession.localHour < 17 ? 1
                           : activeSession.localHour < 22 ? 2
                                                          : 3;
    stats.timeOfDaySeconds[bucket] = saturatedAdd(stats.timeOfDaySeconds[bucket], seconds);
    stats.dayOfWeekSeconds[activeSession.weekday] =
        saturatedAdd(stats.dayOfWeekSeconds[activeSession.weekday], seconds);
  }

  if (activeSession.localDay != companion::DayLedger::NEVER && activeSession.localDay != statsFile.lastReadDay) {
    stats.currentStreakDays =
        activeSession.localDay == statsFile.lastReadDay + 1 && stats.currentStreakDays < UINT16_MAX
            ? static_cast<uint16_t>(stats.currentStreakDays + 1)
            : 1;
    stats.bestStreakDays = std::max(stats.bestStreakDays, stats.currentStreakDays);
    statsFile.lastReadDay = activeSession.localDay;
  }

  auto entries = makeUniqueNoThrow<Entry[]>(kMaxEntries);
  if (!entries) {
    LOG_ERR("JRNL", "OOM: per-book analytics");
  } else {
    const int count = load(entries.get(), kMaxEntries);
    for (int i = 0; i < count; ++i) {
      if (std::strncmp(entries[i].path, activeSession.path, kPathBytes) != 0) continue;
      entries[i].readingSeconds = saturatedAdd(entries[i].readingSeconds, seconds);
      entries[i].workReadingSeconds = saturatedAdd(entries[i].workReadingSeconds, workSeconds);
      entries[i].readingSessions = static_cast<uint16_t>(
          std::min<uint32_t>(UINT16_MAX, static_cast<uint32_t>(entries[i].readingSessions) + 1));
      entries[i].pageTurns = saturatedAdd(entries[i].pageTurns, pages);
      const uint32_t readDate = today();
      if (readDate != 0 && readDate != entries[i].lastReadDate && entries[i].readingDays < UINT16_MAX)
        ++entries[i].readingDays;
      entries[i].lastReadDate = readDate;
      if (!save(entries.get(), count)) LOG_ERR("JRNL", "Could not save per-book analytics");
      break;
    }
  }
  if (!saveStats()) LOG_ERR("JRNL", "Could not save reading stats");
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
