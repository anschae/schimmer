#pragma once
// Reverb modes: base sizes and character of each algorithm. Indices are stored
// in projects (parameter "mode"): never reorder, only append.

#include "Common.h"

namespace schimmer::dsp
{
struct ModeConfig
{
    const char* name;
    bool monoIn;            // mono-in / stereo-out
    float tankMeanMs;       // geometric mean of the tank line lengths at size scale 1
    float tankSpread;       // ratio longest / shortest line
    float diffRangeMs;      // range of the longest input diffuser stage at size scale 1
    int diffStages;         // number of input diffuser stages (max 6)
    float loopApMs;         // mean in-loop allpass length
    float earlyLevel;       // level of the diffuser ("early") output in the wet signal
    float maxModMs;         // modulation depth at Mod Depth = 100 %
    float stereoSpread;     // 0 = input L/R mixed into all channels, 1 = true stereo split
};

inline constexpr int kNumModes = 4;

inline constexpr ModeConfig kModes[kNumModes] = {
    // name        mono   mean   spread diffRng stages ap    early mod   stereo
    { "Nebula",    true,  210.0f, 2.6f, 150.0f, 6,    22.0f, 0.10f, 3.5f, 0.0f },
    { "Cathedral", false, 165.0f, 2.5f, 120.0f, 6,    18.0f, 0.18f, 3.0f, 0.85f },
    { "Hall",      false,  95.0f, 2.3f,  70.0f, 5,    11.0f, 0.30f, 2.2f, 0.75f },
    { "Chamber",   false,  38.0f, 2.1f,  26.0f, 4,     5.5f, 0.45f, 1.6f, 0.65f },
};

/** Size control (0..1) to scale factor: 0.25x .. 2x, 0.5 -> ~0.71x. */
inline float sizeToScale (float size01) noexcept
{
    return 0.25f * std::pow (8.0f, size01);
}
} // namespace schimmer::dsp
