#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonCompanionArt.h"

#include <Bitmap.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

namespace pokemon {
namespace {

const char* moodFileName(const companion::Mood mood) {
  switch (mood) {
    case companion::Mood::Thriving:  return "thriving";
    case companion::Mood::Content:   return "content";
    case companion::Mood::Peckish:   return "peckish";
    case companion::Mood::Neglected: return "neglected";
  }
  return "content";
}

}  // namespace

bool drawPokemonCompanionMoodArt(const GfxRenderer& renderer,
                                 const uint16_t speciesId,
                                 const companion::Mood mood,
                                 const Rect bounds) {
  if (speciesId == 0 || speciesId > 151 || bounds.width <= 0 || bounds.height <= 0) {
    return false;
  }

  char path[96]{};
  std::snprintf(path, sizeof(path),
                "/.crosspoint/pokemon/companions/%03u-%s.bmp",
                static_cast<unsigned>(speciesId), moodFileName(mood));

  HalFile file;
  if (!Storage.openFileForRead("PKCOMP", path, file)) {
    // Missing mood art is expected while the SD library is being populated.
    // Do not draw a '?' here; the caller deliberately falls back to the
    // already-working species hero artwork.
    return false;
  }

  Bitmap bitmap(file);
  const BmpReaderError parseResult = bitmap.parseHeaders();
  if (parseResult != BmpReaderError::Ok) {
    LOG_ERR("PKCOMP", "Invalid Pokemon companion art: %s", path);
    file.close();
    return false;
  }

  renderer.drawBitmap(bitmap, bounds.x, bounds.y, bounds.width, bounds.height,
                      0.0f, 0.0f);

  file.close();
  return true;
}

}  // namespace pokemon

#endif  // CROSSINK_ENABLE_POKEMON
