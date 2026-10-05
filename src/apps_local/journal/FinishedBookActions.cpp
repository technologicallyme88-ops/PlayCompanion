#include "FinishedBookActions.h"

#include <HalStorage.h>
#include <Logging.h>

#include <functional>

#include "CrossPointState.h"
#include "ReadingJournal.h"
#include "RecentBooksStore.h"

namespace journal {
namespace {
constexpr char READ_FOLDER[] = "/read";

std::string buildReadFolderDestination(const std::string& srcPath) {
  const size_t lastSlash = srcPath.rfind('/');
  const std::string filename = lastSlash != std::string::npos ? srcPath.substr(lastSlash + 1) : srcPath;

  Storage.mkdir(READ_FOLDER);
  std::string dstPath = std::string(READ_FOLDER) + "/" + filename;
  if (!Storage.exists(dstPath.c_str())) return dstPath;

  const size_t dotPos = filename.rfind('.');
  const std::string base = dotPos != std::string::npos ? filename.substr(0, dotPos) : filename;
  const std::string ext = dotPos != std::string::npos ? filename.substr(dotPos) : "";
  int suffix = 2;
  do {
    dstPath = std::string(READ_FOLDER) + "/" + base + " (" + std::to_string(suffix) + ")" + ext;
    suffix++;
  } while (Storage.exists(dstPath.c_str()) && suffix < 100);
  return dstPath;
}
}  // namespace

bool isInReadFolder(const std::string& path) {
  constexpr size_t length = sizeof(READ_FOLDER) - 1;
  return path.size() > length && path.compare(0, length, READ_FOLDER) == 0 && path[length] == '/';
}

std::string epubCachePath(const std::string& path) {
  return "/.crosspoint/epub_" + std::to_string(std::hash<std::string>{}(path));
}

std::string moveFinishedBookToReadFolder(const std::string& srcPath, const std::string& oldCachePath) {
  if (isInReadFolder(srcPath)) return srcPath;
  const std::string dstPath = buildReadFolderDestination(srcPath);
  LOG_INF("FINISH", "Moving finished epub: %s -> %s", srcPath.c_str(), dstPath.c_str());
  if (!Storage.rename(srcPath.c_str(), dstPath.c_str())) {
    LOG_ERR("FINISH", "Failed to move finished book to '/read' folder");
    return srcPath;
  }

  const std::string newCachePath = epubCachePath(dstPath);
  if (!oldCachePath.empty() && Storage.exists(oldCachePath.c_str()) &&
      !Storage.rename(oldCachePath.c_str(), newCachePath.c_str())) {
    LOG_ERR("FINISH", "Failed to rename cache dir %s -> %s", oldCachePath.c_str(), newCachePath.c_str());
  }

  RECENT_BOOKS.updatePath(srcPath, dstPath, oldCachePath, newCachePath);
  if (!updatePath(srcPath.c_str(), dstPath.c_str())) {
    LOG_ERR("FINISH", "Failed to update finished book's journal path");
  }
  if (APP_STATE.openEpubPath == srcPath) {
    APP_STATE.openEpubPath = dstPath;
    APP_STATE.saveToFile();
  }
  return dstPath;
}

}  // namespace journal
