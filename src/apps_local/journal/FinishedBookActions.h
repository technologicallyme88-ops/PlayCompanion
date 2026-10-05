#pragma once

#include <string>

namespace journal {

bool isInReadFolder(const std::string& path);
std::string epubCachePath(const std::string& path);
std::string moveFinishedBookToReadFolder(const std::string& srcPath, const std::string& oldCachePath);

}  // namespace journal
