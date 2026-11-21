#pragma once
// Randomize with per-section locks (message thread). Only "randomizable"
// parameters of unlocked sections change, and only within musically sensible
// ranges (narrower than the full parameter ranges; see Randomizer.cpp).

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

namespace schimmer
{
class SchimmerProcessor;

namespace Randomizer
{
/** Musically sensible range for a parameter: {lo, hi, logarithmic}. */
struct Range { float lo, hi; bool log; };
Range rangeFor (P p);

/** Randomizes all unlocked sections. `locks` = bit per section (kSections). */
void randomize (SchimmerProcessor& proc, uint32_t locks, juce::Random& rng);
} // namespace Randomizer
} // namespace schimmer
