#pragma once

#include <string>

namespace pokemon {

// True when the dynamic Party-lead Pokemon companion is active.
bool isPokemonCompanionSelected();

// Current Party-lead display name (nickname first, species name otherwise).
std::string pokemonCompanionDisplayName();

// Dynamic Character-menu label, e.g. "Abra (pokemon)".
std::string pokemonCompanionOptionLabel();

// Durable selection bit stored independently in ESP32 NVS.  This deliberately
// does not depend on the dynamically rebuilt companion enum in settings.json.
bool persistentPokemonCompanionSelected();
void setPokemonCompanionSelectedPersistent(bool selected);

}  // namespace pokemon
