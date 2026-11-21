#pragma once
// Host parameters (AudioProcessorValueTreeState), defined in one table
// (Parameters.cpp). IDs are stable: NEVER rename or remove an ID; new
// parameters get a higher version hint (kParamVersion) and are appended.
// tests/ProcessorTests.cpp lists every ID by hand on purpose.

#include <juce_audio_processors/juce_audio_processors.h>

namespace schimmer
{
inline constexpr int kParamVersion = 1;

enum class Unit { Percent, Hz, Ms, Sec, Db, Semis, Cents, Mult, PerSec, Bool, Choice };

/** Index into the parameter table. Order = table order. */
enum class P : int
{
    // reverb core
    mode, size, preDelay, preDelaySync, preDelayNote, decay, freeze, freezeMode, diffusion, bloom,
    modRate, modDepth, modDrift, lowCut, highCut, lowMult, highMult, lowXover, highXover, color, warmth, quality,
    // input / output
    inHP, inLP, tilt, width, bassMono, mix, mixLock, output,
    // shimmer
    shimMode, pitch, pitchSnap, fine, pitch2, voiceMix, shimAmount, shimLowCut, shimHighCut, shimSmooth, shimPlace,
    // grains
    grainOn, grainSource, grainDest, grainSize, grainDensity, grainPitch, grainQuant, grainSpray, grainPitchJit,
    grainReverse, grainScatter, grainAmount,
    // delay
    delayOn, delayTime, delaySync, delayNote, delayFeedback, delayPingPong, delayOffset, delayHP, delayLP, delayMod,
    delayToReverb, delayLevel,
    // clear tail
    clearTail, clearTarget, autoClear,
    // ducking (IDs as in Prog Delay)
    duckOn, duckSource, duckHP, duckLP, duckDetMode, duckRms, duckThresh, duckDepth, duckFull, duckKnee,
    duckAttack, duckHold, duckRelease, duckRelSync, duckRelNote, duckLook, duckTarget, duckPos,
    // morph
    morphOn, morph,
    count
};
inline constexpr int kNumParams = (int) P::count;

struct ParamInfo
{
    const char* id;
    const char* name;
    Unit unit;
    float min, max, def, centre;     // centre: value at mid position (skew); <= min = linear
    juce::StringArray choices;       // Unit::Choice
    const char* tooltip;
    bool automatable = true;
    bool randomizable = true;        // part of Randomize
    int section = 0;                 // Randomize lock group, see kSections
};

const ParamInfo& info (P p);
inline const char* pid (P p) { return info (p).id; }

/** Randomize / lock groups. */
inline const char* const kSections[] = { "Reverb", "Tone", "Shimmer", "Grains", "Delay", "Output", "Ducking" };
inline constexpr int kNumSections = 7;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// ---- tempo-synced note values (pre-delay, delay)
inline const juce::StringArray& noteNames()
{
    static const juce::StringArray n { "1/64", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                       "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1T", "1/1", "1/1D", "2/1" };
    return n;
}
/** Length of note value index i in whole notes. */
inline double noteWholes (int i)
{
    static const double w[] = { 1.0 / 64, 1.0 / 48, 1.0 / 32, 3.0 / 64, 1.0 / 24, 1.0 / 16, 3.0 / 32, 1.0 / 12, 1.0 / 8,
                                3.0 / 16, 1.0 / 6, 1.0 / 4, 3.0 / 8, 1.0 / 3, 1.0 / 2, 3.0 / 4, 2.0 / 3, 1.0, 1.5, 2.0 };
    return w[juce::jlimit (0, 19, i)];
}
/** Tempo-synced duck release values in whole notes; the last entry (0) = one bar of the host signature. */
inline constexpr double kRelNoteWholes[] = { 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 0.0 };

juce::String formatValue (P p, float v);
} // namespace schimmer
