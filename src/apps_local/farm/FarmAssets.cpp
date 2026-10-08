#include "FarmAssets.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "FarmCropAssets.h"

namespace farm {
namespace {
constexpr char kCropDirectory[] = "/.crosspoint/harvest/crops";
constexpr char kVersionPath[] = "/.crosspoint/harvest/crops/.pack-version";
}

bool ensureCropAssets() {
  Storage.mkdir("/.crosspoint/harvest");
  Storage.mkdir(kCropDirectory);

  char installed[sizeof(assets::kVersion) + 1] = {};
  const size_t read = Storage.readFileToBuffer(kVersionPath, installed, sizeof(installed));
  if (read == std::strlen(assets::kVersion) && std::strcmp(installed, assets::kVersion) == 0) return true;

  char path[64];
  for (const auto& asset : assets::kCropAssets) {
    std::snprintf(path, sizeof(path), "%s/%s", kCropDirectory, asset.name);
    HalFile file;
    if (!Storage.openFileForWrite("FARM", path, file) || file.write(asset.data, asset.size) != asset.size) {
      LOG_ERR("FARM", "Failed to install crop asset: %s", asset.name);
      return false;
    }
  }

  HalFile version;
  if (!Storage.openFileForWrite("FARM", kVersionPath, version) ||
      version.write(reinterpret_cast<const uint8_t*>(assets::kVersion), std::strlen(assets::kVersion)) !=
          std::strlen(assets::kVersion)) {
    LOG_ERR("FARM", "Failed to save crop asset version");
    return false;
  }
  return true;
}

}  // namespace farm
