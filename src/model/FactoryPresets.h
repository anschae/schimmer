#pragma once
// Built-in presets, built in code from parameter overrides (everything not
// listed keeps its default), so they always match the current parameter set.

#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include "Parameters.h"

namespace schimmer
{
struct FactoryPreset
{
    juce::String name;
    juce::String category;
    std::vector<std::pair<P, float>> values;
};

namespace FactoryPresets
{
const std::vector<FactoryPreset>& all();
/** Full state tree ("SchimmerState") for a preset. */
juce::ValueTree makeState (const FactoryPreset& p);
/** Plain values of all parameters for a preset. */
std::array<float, kNumParams> values (const FactoryPreset& p);
} // namespace FactoryPresets
} // namespace schimmer
