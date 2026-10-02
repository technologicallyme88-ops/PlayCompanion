#include "WakeResume.h"

#include <Memory.h>

#include <cstring>

#include "Shelf.h"
#include "farm/FarmActivity.h"
#include "journal/ReadingJournalActivity.h"
#if defined(CROSSINK_ENABLE_POKEMON)
#include "activities/pokemon/PokemonActivity.h"
#endif

namespace wake_resume {

bool open(const char* activityName, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  if (activityName == nullptr || *activityName == '\0') return false;
  if (shelf::resumeItemByTitle(activityName, renderer, mappedInput)) return true;

  std::unique_ptr<Activity> activity;
  if (std::strcmp(activityName, "Farm") == 0) {
    activity = makeUniqueNoThrow<farm::FarmActivity>(renderer, mappedInput);
  } else if (std::strcmp(activityName, "ReadingJournal") == 0) {
    activity = ReadingJournalActivity::create(renderer, mappedInput);
#if defined(CROSSINK_ENABLE_POKEMON)
  } else if (std::strcmp(activityName, "Pokemon") == 0) {
    activity = makeUniqueNoThrow<PokemonActivity>(renderer, mappedInput);
#endif
  } else {
    return false;
  }

  if (!activity) {
    LOG_ERR("WAKE", "OOM resuming %s", activityName);
    return false;
  }
  activityManager.replaceActivity(std::move(activity));
  return true;
}

}  // namespace wake_resume
