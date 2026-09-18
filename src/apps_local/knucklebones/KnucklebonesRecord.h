#pragma once

#include <cstddef>
#include <cstdint>

namespace knucklebones {
struct Record {
  uint32_t wins = 0;
  uint32_t losses = 0;
  uint32_t draws = 0;
};

void addResult(Record& record, int yours, int theirs);
// Exact name bytes are hex encoded, avoiding path separators and hash collisions.
bool recordPath(const char* name, char* path, size_t size);
Record loadRecord(const char* path);
bool saveRecord(const char* path, const Record& record);
}  // namespace knucklebones
