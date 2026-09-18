#pragma once

#if defined(CROSSINK_ENABLE_POKEMON)

#include <CompanionMood.h>
#include <cstdint>

#include "GfxRenderer.h"
#include "components/themes/BaseTheme.h"

namespace pokemon {

// Draws a mood-specific Party-lead companion sprite from SD.
// Returns false when the mood sprite does not exist or cannot be rendered,
// allowing the caller to fall back to the existing species hero artwork.
bool drawPokemonCompanionMoodArt(const GfxRenderer& renderer,
                                 uint16_t speciesId,
                                 companion::Mood mood,
                                 Rect bounds);

}  // namespace pokemon

#endif  // CROSSINK_ENABLE_POKEMON
