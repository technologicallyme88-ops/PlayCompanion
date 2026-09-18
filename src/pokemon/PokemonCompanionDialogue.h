#pragma once

#if defined(CROSSINK_ENABLE_POKEMON)

#include <CompanionMood.h>
#include <cstdint>

namespace pokemon {

// Stage 3D.1: species-aware companion dialogue for the initial test set.
// Returns nullptr / 0 for unsupported species so Home falls back to the
// existing generic companion dialogue.
const char* pokemonCompanionQuote(companion::Mood mood, uint32_t rotation);
uint8_t pokemonCompanionQuoteCount(companion::Mood mood);

}  // namespace pokemon

#endif  // CROSSINK_ENABLE_POKEMON
