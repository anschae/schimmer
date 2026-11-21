#include "FactoryPresets.h"

namespace schimmer
{
namespace
{
std::vector<FactoryPreset> build()
{
    // Shimmer modes: 0 Bypass, 1 Single, 2 Dual, 3 Single Reverse, 4 Dual Reverse, 5 Two-Voice
    // Modes: 0 Nebula, 1 Cathedral, 2 Hall, 3 Chamber. Note indices: see noteNames() (8 = 1/8, 9 = 1/8D, 11 = 1/4, 12 = 1/4D)
    // clang-format off
    return {
        { "Init", "Basics", {} },

        // ---- pads / soundscapes
        { "Endless Pad", "Pads", { { P::mode, 0 }, { P::size, 72 }, { P::decay, 24 }, { P::diffusion, 92 }, { P::bloom, 1.2f },
            { P::modRate, 0.25f }, { P::modDepth, 35 }, { P::modDrift, 75 }, { P::highMult, 0.5f }, { P::lowMult, 1.0f },
            { P::lowCut, 40 }, { P::mix, 45 } } },
        { "Nebula Drift", "Pads", { { P::mode, 0 }, { P::size, 90 }, { P::decay, 45 }, { P::diffusion, 100 }, { P::bloom, 2.5f },
            { P::modRate, 0.12f }, { P::modDepth, 55 }, { P::modDrift, 100 }, { P::highMult, 0.4f }, { P::lowCut, 60 },
            { P::tilt, -1.5f }, { P::mix, 50 } } },
        { "Bloom Keys", "Pads", { { P::mode, 2 }, { P::size, 65 }, { P::decay, 5.5f }, { P::bloom, 1.0f }, { P::diffusion, 88 },
            { P::modDepth, 30 }, { P::mix, 35 } } },

        // ---- shimmer
        { "Cathedral of Light", "Shimmer", { { P::mode, 1 }, { P::size, 78 }, { P::decay, 14 }, { P::diffusion, 90 },
            { P::shimMode, 1 }, { P::pitch, 12 }, { P::shimAmount, 35 }, { P::shimSmooth, 65 }, { P::shimHighCut, 8000 },
            { P::shimLowCut, 300 }, { P::lowCut, 50 }, { P::mix, 40 } } },
        { "Eno Shimmer", "Shimmer", { { P::mode, 2 }, { P::size, 70 }, { P::decay, 10 }, { P::diffusion, 90 },
            { P::shimMode, 1 }, { P::pitch, 12 }, { P::shimAmount, 55 }, { P::shimSmooth, 25 }, { P::shimHighCut, 7500 },
            { P::modDepth, 35 }, { P::mix, 40 } } },
        { "Celestial Choir", "Shimmer", { { P::mode, 1 }, { P::size, 75 }, { P::decay, 16 }, { P::diffusion, 92 },
            { P::shimMode, 2 }, { P::pitch, 12 }, { P::shimAmount, 50 }, { P::shimSmooth, 15 }, { P::shimHighCut, 7000 },
            { P::mix, 45 } } },
        { "Fifth Heaven", "Shimmer", { { P::mode, 2 }, { P::size, 68 }, { P::decay, 12 }, { P::shimMode, 5 }, { P::pitch, 12 },
            { P::pitch2, 7 }, { P::voiceMix, 45 }, { P::shimAmount, 45 }, { P::shimSmooth, 60 }, { P::mix, 40 } } },
        { "Octave Abyss", "Shimmer", { { P::mode, 0 }, { P::size, 80 }, { P::decay, 28 }, { P::shimMode, 1 }, { P::pitch, -12 },
            { P::shimAmount, 40 }, { P::shimSmooth, 70 }, { P::shimLowCut, 60 }, { P::shimHighCut, 5000 }, { P::color, 1 },
            { P::mix, 45 } } },
        { "Pitched Echoes", "Shimmer", { { P::mode, 2 }, { P::size, 70 }, { P::diffusion, 0 }, { P::decay, 7 }, { P::shimMode, 1 },
            { P::pitch, 7 }, { P::shimAmount, 70 }, { P::shimSmooth, 80 }, { P::modDepth, 15 }, { P::mix, 35 } } },

        // ---- organ-like (reverse shimmer)
        { "Organ Pipes", "Organ", { { P::mode, 1 }, { P::size, 80 }, { P::decay, 13 }, { P::diffusion, 92 },
            { P::shimMode, 4 }, { P::pitch, 12 }, { P::shimAmount, 50 }, { P::shimSmooth, 85 }, { P::shimHighCut, 6500 },
            { P::mix, 45 } } },
        { "Reverse Cathedral Organ", "Organ", { { P::mode, 0 }, { P::size, 85 }, { P::decay, 20 }, { P::shimMode, 3 },
            { P::pitch, 12 }, { P::shimAmount, 45 }, { P::shimSmooth, 90 }, { P::bloom, 1.5f }, { P::mix, 50 } } },

        // ---- ambient guitar
        { "Ambient Guitar Wash", "Guitar", { { P::mode, 2 }, { P::size, 66 }, { P::decay, 7 }, { P::delayOn, 1 }, { P::delaySync, 1 },
            { P::delayNote, 9 }, { P::delayFeedback, 40 }, { P::delayPingPong, 1 }, { P::delayLP, 6000 }, { P::duckOn, 1 },
            { P::duckThresh, -30 }, { P::duckDepth, -9 }, { P::duckRelease, 400 }, { P::mix, 35 } } },
        { "Swell Guitar", "Guitar", { { P::mode, 1 }, { P::size, 70 }, { P::decay, 9 }, { P::bloom, 3.0f }, { P::diffusion, 95 },
            { P::modDepth, 40 }, { P::inHP, 120 }, { P::mix, 45 } } },
        { "Dotted Echo Cloud", "Guitar", { { P::mode, 1 }, { P::size, 72 }, { P::decay, 12 }, { P::delayOn, 1 }, { P::delaySync, 1 },
            { P::delayNote, 12 }, { P::delayFeedback, 55 }, { P::delayLevel, -8 }, { P::delayToReverb, 70 }, { P::delayMod, 25 },
            { P::mix, 40 } } },

        // ---- grains
        { "Glitter Grains", "Grains", { { P::mode, 1 }, { P::decay, 10 }, { P::grainOn, 1 }, { P::grainSource, 1 }, { P::grainDest, 0 },
            { P::grainPitch, 12 }, { P::grainDensity, 20 }, { P::grainSize, 80 }, { P::grainScatter, 90 }, { P::grainReverse, 30 },
            { P::grainAmount, 45 }, { P::grainSpray, 500 }, { P::mix, 45 } } },
        { "Granular Cloud", "Grains", { { P::mode, 0 }, { P::size, 70 }, { P::decay, 15 }, { P::grainOn, 1 }, { P::grainSource, 0 },
            { P::grainDest, 1 }, { P::grainPitch, 0 }, { P::grainPitchJit, 7 }, { P::grainQuant, 1 }, { P::grainDensity, 25 },
            { P::grainSize, 220 }, { P::grainSpray, 900 }, { P::grainAmount, 55 }, { P::mix, 50 } } },

        // ---- drones
        { "Frozen Drone", "Drones", { { P::mode, 0 }, { P::size, 95 }, { P::decay, 60 }, { P::diffusion, 100 }, { P::lowCut, 70 },
            { P::highMult, 0.4f }, { P::modDrift, 100 }, { P::modDepth, 45 }, { P::modRate, 0.15f }, { P::freezeMode, 1 },
            { P::mix, 60 } } },
        { "Deep Space Drone", "Drones", { { P::mode, 0 }, { P::size, 100 }, { P::decay, 90 }, { P::shimMode, 3 }, { P::pitch, -12 },
            { P::shimAmount, 25 }, { P::shimSmooth, 90 }, { P::color, 1 }, { P::warmth, 30 }, { P::lowCut, 40 }, { P::mix, 60 } } },

        // ---- vintage / dark
        { "Vintage Dark Hall", "Vintage", { { P::mode, 2 }, { P::size, 62 }, { P::decay, 4.5f }, { P::color, 1 }, { P::modDepth, 45 },
            { P::modRate, 0.8f }, { P::modDrift, 30 }, { P::highMult, 0.5f }, { P::mix, 30 } } },
        { "Dark Plate 1979", "Vintage", { { P::mode, 3 }, { P::size, 70 }, { P::decay, 2.6f }, { P::diffusion, 100 }, { P::color, 1 },
            { P::modDepth, 35 }, { P::lowMult, 0.8f }, { P::mix, 25 } } },
        { "Tape Sky", "Vintage", { { P::mode, 1 }, { P::size, 68 }, { P::decay, 9 }, { P::warmth, 55 }, { P::color, 1 },
            { P::modDepth, 60 }, { P::modRate, 0.6f }, { P::modDrift, 40 }, { P::mix, 40 } } },

        // ---- chorus / small
        { "Chorus Ambience", "Ambience", { { P::mode, 3 }, { P::size, 40 }, { P::decay, 1.6f }, { P::diffusion, 55 },
            { P::modDepth, 75 }, { P::modRate, 1.1f }, { P::modDrift, 0 }, { P::mix, 30 } } },
        { "Ducked Vocal Space", "Ambience", { { P::mode, 2 }, { P::size, 55 }, { P::decay, 3.8f }, { P::preDelay, 60 }, { P::inHP, 150 },
            { P::duckOn, 1 }, { P::duckThresh, -28 }, { P::duckDepth, -10 }, { P::duckRelease, 350 }, { P::mix, 25 } } },

        // ---- send effects (100 % wet)
        { "Send: Huge Hall", "Send", { { P::mode, 1 }, { P::size, 70 }, { P::decay, 6 }, { P::inHP, 150 }, { P::bassMono, 150 },
            { P::mix, 100 } } },
        { "Send: Shimmer", "Send", { { P::mode, 2 }, { P::size, 68 }, { P::decay, 9 }, { P::shimMode, 1 }, { P::pitch, 12 },
            { P::shimAmount, 40 }, { P::shimSmooth, 55 }, { P::inHP, 120 }, { P::mix, 100 } } },
    };
    // clang-format on
}
} // namespace

const std::vector<FactoryPreset>& FactoryPresets::all()
{
    static const std::vector<FactoryPreset> v = build();
    return v;
}

std::array<float, kNumParams> FactoryPresets::values (const FactoryPreset& p)
{
    std::array<float, kNumParams> v {};
    for (int k = 0; k < kNumParams; ++k)
        v[(size_t) k] = info ((P) k).def;
    for (auto& [prm, val] : p.values)
        v[(size_t) prm] = val;
    return v;
}

juce::ValueTree FactoryPresets::makeState (const FactoryPreset& p)
{
    const auto v = values (p);
    juce::ValueTree root ("SchimmerState");
    root.setProperty ("stateVersion", 1, nullptr);
    root.setProperty ("presetName", "Factory / " + p.name, nullptr);
    juce::ValueTree ps ("PARAMS");
    for (int k = 0; k < kNumParams; ++k)
    {
        juce::ValueTree c ("PARAM");
        c.setProperty ("id", pid ((P) k), nullptr);
        c.setProperty ("value", v[(size_t) k], nullptr);
        ps.appendChild (c, nullptr);
    }
    root.appendChild (ps, nullptr);
    return root;
}
} // namespace schimmer
