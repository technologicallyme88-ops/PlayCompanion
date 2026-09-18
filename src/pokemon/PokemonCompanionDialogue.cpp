#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonCompanionDialogue.h"

#include <cstddef>
#include <cstdio>

#include "PokemonService.h"

namespace pokemon {
namespace {

struct MoodQuotes {
  const char* thriving[3];
  const char* content[3];
  const char* peckish[3];
  const char* neglected[3];
};

constexpr MoodQuotes SQUIRTLE_QUOTES{
    {"Let's keep going! I have plenty of energy.", "Another chapter? I'm ready.", "This story has me fired up!"},
    {"Nice pace. I'm happy right here.", "This is a good reading spot.", "I can stay for another chapter."},
    {"A little more reading would hit the spot.", "Don't stop now. I was getting into it.", "Could we squeeze in one more chapter?"},
    {"I've been waiting a while.", "I miss our reading time.", "Come back when you're ready to read with me."}};

constexpr MoodQuotes KAKUNA_QUOTES{
    {"Quiet progress is still progress.", "I'm changing a little with every chapter.", "Keep going. Something good is taking shape."},
    {"Still, calm, and listening.", "I don't need much. Just keep reading.", "This is a comfortable place to wait."},
    {"A few more pages would help.", "I'm patient, but I'd like another chapter.", "Let's keep the momentum going."},
    {"It's been very quiet lately.", "Even patience has limits.", "I could use some time together."}};

constexpr MoodQuotes ABRA_QUOTES{
    {"I dreamed ahead. The next chapter looks promising.", "My mind is wide awake for this one.", "Keep reading. I'm curious where this goes."},
    {"I'm happy to stay for another chapter.", "This is peaceful. I can focus here.", "Good. I was hoping we'd keep reading."},
    {"I was just starting to get interested.", "One more chapter before I drift off?", "My attention is fading. More story might help."},
    {"I've slept through too many quiet days.", "Wake me when we're reading again.", "I miss having a story to listen to."}};

constexpr MoodQuotes MACHOP_QUOTES{
    {"Strong session! Let's keep training.", "Every chapter is another rep.", "Good work. We can handle more."},
    {"Steady pace. That's how you build strength.", "Nice and consistent. Keep it up.", "I'm good here. No need to rush."},
    {"We could use a few more reading reps.", "Don't cool down yet. One more chapter.", "A little more practice would be good."},
    {"We're getting rusty.", "Training only works if we show up.", "Let's get back into a reading routine."}};

constexpr MoodQuotes DITTO_QUOTES{
    {"I could be anything after this many chapters!", "Great mood. Great story. Great me.", "Keep going! I'll match the energy."},
    {"I'm comfortable being me for a while.", "This pace works for me.", "I can adapt to another chapter."},
    {"Maybe I should turn into a bookmark until we continue.", "I was getting used to reading.", "A few more pages would be nice."},
    {"I can copy a lot of things, but not reading time.", "It's been too long since our last chapter.", "I'd rather be part of the story again."}};

// One species-defining feature per Kanto Pokemon. The shared sentence shapes
// keep this table compact, while the feature makes every generated mood line
// species-specific. The five characters above retain their more elaborate
// hand-written voices.
constexpr const char* SPECIES_FEATURES[] = {
    "seedling bulb",        "growing flower bud",   "sun-fed blossom",       "bright tail flame",
    "restless tail fire",   "sky-high wing flame",  "polished shell",        "curling wave tail",
    "shoulder cannons",     "hungry leaf",          "patient chrysalis",     "powdery wings",
    "watchful horn",        "hardened cocoon",      "twin stingers",         "tiny homing sense",
    "keen-eyed crest",      "storm-cut wings",      "quick little fangs",    "bold whiskers",
    "sharp little beak",    "drill-fast beak",      "rattling snake coils",  "flaring cobra hood",
    "sparking cheek pouches", "lightning-bolt tail", "sand-digging claws",   "ice-sharp claws",
    "poison-sensitive ears", "sturdy poison ears",  "royal poison crown",    "proud poison ears",
    "battle-ready horn",    "drill-ready crown",    "fairy-soft steps",      "moonlit wings",
    "six warm tails",       "snow-white mane",
    "singing voice",        "dream-drawing song",   "cave-sensing wings",    "four poison fangs",
    "sun-seeking leaves",   "sweet-smelling bloom", "heavy rafflesia",       "burrowing claws",
    "glowing mushroom cap", "many searching eyes", "tinted compound eyes",  "mole-soft paws",
    "three busy heads",     "lucky coin charm",     "sleek night paws",      "scratching temper",
    "throbbing headache",   "quick fighting fists", "springy tail curl",     "ember-striped mane",
    "racing fire mane",     "spiral belly mark",    "swirling water gloves", "champion swimmer's arms",
    "sleepy psychic focus", "bending silver spoons", "three-spoon focus",    "training-hardened muscles",
    "four powerful arms",   "championship belt",    "springy vine",          "fly-catching leaves",
    "acidic pitcher",       "cool jelly body",      "crystal-clear domes",   "living stone fists",
    "rolling boulder body", "iron-hard ridges",     "warm ember mane",       "racing fire hooves",
    "patient sleepy gaze",  "spiral shell crown",   "clustered magnets",     "three humming magnets",
    "proud leek stalk",     "dueling heads",        "three watchful faces",  "seal-slick flippers",
    "frosted horn",         "sticky sludge body",   "toxic tidal shape",     "clamping shell",
    "armored pearl shell",  "sleepy gas cloud",     "mischievous grin",      "shadowy hands",
    "towering stone body",  "drowsing pendulum",    "dream-eating trunk",    "snapping crab claws",
    "mighty crusher claw",  "sparking orb body",    "volatile electric shell", "six patient seeds",
    "walking palm trunks",  "worn bone club",       "echoing skull helmet",  "spring-loaded kicks",
    "boxing-glove fists",   "far-reaching tongue",  "smoky gas vents",       "billowing twin fumes",
    "sturdy horned hide",   "rhinestone armor",     "lucky healing egg",     "tangling blue vines",
    "pouch-protected joey", "swift kicking legs",   "ink-spraying snout",    "golden tail fins",
    "silken water fins",    "gem-bright sea core",  "mysterious jewel core", "barrier-making hands",
    "scythe-sharp arms",    "chilling kiss",        "plug-shaped horns",     "duck-billed flame",
    "armored gripping jaws", "charging wild horns", "flashing tail fins",   "serpentine sea strength",
    "song-filled shell",    "ever-changing cells",  "bright neck ruff",      "bubbling neck frill",
    "needle-finned tail",   "crackling fur collar", "ember-soft fur collar", "digital polygon edges",
    "ancient spiral shell", "many hooked tentacles", "ancient shell shield", "twin fossil scythes",
    "bottomless appetite",  "frost-trailing wings", "thunder-trailing wings", "flame-trailing wings",
    "small draconic pearl", "long sea-serpent form", "kindhearted dragon wings", "vast psychic focus",
    "newborn psychic spark",
};

static_assert(sizeof(SPECIES_FEATURES) / sizeof(SPECIES_FEATURES[0]) == KANTO_SPECIES_COUNT);

const char* generatedQuote(const uint16_t speciesId, const companion::Mood mood, const uint32_t rotation) {
  if (speciesId == 0 || speciesId > KANTO_SPECIES_COUNT) return nullptr;

  static char line[128];
  const char* feature = SPECIES_FEATURES[speciesId - 1];
  const size_t variation = rotation % 3;
  constexpr const char* TEMPLATES[4][3] = {
      {"My %s is buzzing. Let's read on!", "My %s is at its best. Another chapter!",
       "You have my %s fired up for this story!"},
      {"My %s is peaceful here.", "My %s likes this steady reading pace.",
       "This is a fine chapter for my %s."},
      {"My %s could use another chapter.", "A few more pages would perk up my %s.",
       "Don't stop yet; my %s wants more story."},
      {"My %s has gone quiet without our stories.", "Even my %s misses reading with you.",
       "It has been too long; my %s is waiting."},
  };
  const size_t moodIndex = static_cast<size_t>(mood);
  const size_t safeMood = moodIndex < 4 ? moodIndex : 1;
  std::snprintf(line, sizeof(line), TEMPLATES[safeMood][variation], feature);
  return line;
}

const MoodQuotes* quotesForSpecies(const uint16_t speciesId) {
  switch (speciesId) {
    case 7:   return &SQUIRTLE_QUOTES;
    case 14:  return &KAKUNA_QUOTES;
    case 63:  return &ABRA_QUOTES;
    case 66:  return &MACHOP_QUOTES;
    case 132: return &DITTO_QUOTES;
    default:  return nullptr;
  }
}

const char* quoteAt(const MoodQuotes& q, const companion::Mood mood, const uint32_t rotation) {
  const size_t i = rotation % 3;
  switch (mood) {
    case companion::Mood::Thriving:  return q.thriving[i];
    case companion::Mood::Content:   return q.content[i];
    case companion::Mood::Peckish:   return q.peckish[i];
    case companion::Mood::Neglected: return q.neglected[i];
  }
  return q.content[i];
}

}  // namespace

uint8_t pokemonCompanionQuoteCount(const companion::Mood mood) {
  (void)mood;
  PokemonDashboardSnapshot snapshot{};
  if (devicePokemonService().loadDashboardSnapshot(snapshot) != ServiceStatus::Ok ||
      snapshot.leader.recordId == 0 ||
      snapshot.leader.speciesId == 0 ||
      snapshot.leader.speciesId > KANTO_SPECIES_COUNT) {
    return 0;
  }
  return 3;
}

const char* pokemonCompanionQuote(const companion::Mood mood, const uint32_t rotation) {
  PokemonDashboardSnapshot snapshot{};
  if (devicePokemonService().loadDashboardSnapshot(snapshot) != ServiceStatus::Ok ||
      snapshot.leader.recordId == 0) {
    return nullptr;
  }

  const MoodQuotes* quotes = quotesForSpecies(snapshot.leader.speciesId);
  return quotes ? quoteAt(*quotes, mood, rotation)
                : generatedQuote(snapshot.leader.speciesId, mood, rotation);
}

}  // namespace pokemon

#endif  // CROSSINK_ENABLE_POKEMON
