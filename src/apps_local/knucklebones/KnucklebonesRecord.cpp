#include "KnucklebonesRecord.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

namespace knucklebones {
namespace {
constexpr uint32_t kLimit = 999999;
}

void addResult(Record& record, const int yours, const int theirs) {
  uint32_t& count = yours > theirs ? record.wins : (yours < theirs ? record.losses : record.draws);
  if (count < kLimit) ++count;
}

bool recordPath(const char* name, char* path, const size_t size) {
  constexpr char prefix[] = "/.crosspoint/kb-vs-";
  constexpr char hex[] = "0123456789abcdef";
  if (size == 0) return false;
  path[0] = '\0';
  if (name == nullptr || name[0] == '\0') return false;
  const size_t length = std::strlen(name);
  if (length > 24 || sizeof(prefix) + length * 2 + 4 > size) return false;
  std::memcpy(path, prefix, sizeof(prefix) - 1);
  size_t at = sizeof(prefix) - 1;
  for (size_t i = 0; i < length; ++i) {
    const auto byte = static_cast<unsigned char>(name[i]);
    path[at++] = hex[byte >> 4];
    path[at++] = hex[byte & 15];
  }
  std::memcpy(path + at, ".sav", 5);
  return true;
}

Record loadRecord(const char* path) {
  Record record{};
  if (path == nullptr || path[0] == '\0') return record;
  char buffer[64] = {};
  if (Storage.readFileToBuffer(path, buffer, sizeof(buffer) - 1) == 0) return record;
  unsigned long w = 0, l = 0, d = 0;
  char extra = 0;
  if (std::sscanf(buffer, "KB1 %lu %lu %lu %c", &w, &l, &d, &extra) != 3 ||
      w > kLimit || l > kLimit || d > kLimit) return record;
  record.wins = static_cast<uint32_t>(w);
  record.losses = static_cast<uint32_t>(l);
  record.draws = static_cast<uint32_t>(d);
  return record;
}

bool saveRecord(const char* path, const Record& record) {
  char line[64];
  std::snprintf(line, sizeof(line), "KB1 %lu %lu %lu\n", static_cast<unsigned long>(record.wins),
                static_cast<unsigned long>(record.losses), static_cast<unsigned long>(record.draws));
  if (path == nullptr || path[0] == '\0' || !Storage.writeFile(path, String(line))) {
    LOG_ERR("KNUCK", "Could not save match record");
    return false;
  }
  return true;
}
}  // namespace knucklebones
