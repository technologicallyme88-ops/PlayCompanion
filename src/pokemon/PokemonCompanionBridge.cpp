#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonCompanionBridge.h"

#include <PokemonSpecies.h>
#include <Preferences.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "PokemonService.h"
#include "companion/CompanionSprites.generated.h"

namespace pokemon {
namespace {

constexpr char kPrefsNamespace[] = "pkmcomp";
constexpr char kPrefsSelected[] = "selected";

// -1 = not loaded yet, 0 = normal companion, 1 = dynamic Pokemon companion.
int8_t gPersistentSelection = -1;

bool loadPersistentSelection() {
  if (gPersistentSelection >= 0) return gPersistentSelection != 0;

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    gPersistentSelection = 0;
    return false;
  }
  const bool selected = prefs.getBool(kPrefsSelected, false);
  prefs.end();
  gPersistentSelection = selected ? 1 : 0;
  return selected;
}

}  // namespace

bool persistentPokemonCompanionSelected() { return loadPersistentSelection(); }

void setPokemonCompanionSelectedPersistent(const bool selected) {
  gPersistentSelection = selected ? 1 : 0;

  Preferences prefs;
  if (prefs.begin(kPrefsNamespace, false)) {
    prefs.putBool(kPrefsSelected, selected);
    prefs.end();
  }

  // Keep the live settings object synchronized too.  The NVS bit is the
  // authoritative reboot-safe copy; companionId remains useful to the normal
  // settings UI during the current session.
  if (selected) {
    SETTINGS.companionId = static_cast<uint8_t>(companion::CompanionId::Pokemon);
  }
}

bool isPokemonCompanionSelected() {
  return loadPersistentSelection() ||
         SETTINGS.companionId == static_cast<uint8_t>(companion::CompanionId::Pokemon);
}

std::string pokemonCompanionDisplayName() {
  PokemonDashboardSnapshot snapshot{};
  if (devicePokemonService().loadDashboardSnapshot(snapshot) != ServiceStatus::Ok ||
      snapshot.leader.recordId == 0) {
    return {};
  }

  const size_t nicknameLength = strnlen(snapshot.leader.nickname.data(), snapshot.leader.nickname.size());
  if (nicknameLength > 0) {
    return std::string(snapshot.leader.nickname.data(), nicknameLength);
  }

  const SpeciesData* species = speciesData(snapshot.leader.speciesId);
  return species && species->name ? std::string(species->name) : std::string{};
}

std::string pokemonCompanionOptionLabel() {
  std::string label = pokemonCompanionDisplayName();
  if (label.empty()) label = "Pokemon";
  label += " (pokemon)";
  return label;
}

}  // namespace pokemon

#endif  // CROSSINK_ENABLE_POKEMON
