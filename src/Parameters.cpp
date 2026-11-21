#include "Parameters.h"

namespace schimmer
{
namespace
{
using U = Unit;
const juce::StringArray kNone {};

// clang-format off
const std::vector<ParamInfo>& table()
{
    static const std::vector<ParamInfo> t = {
        // ---------------------------------------------------------------- reverb core (section 0) / tone (1)
        { "mode", "Mode", U::Choice, 0, 3, 1, 0, { "Nebula", "Cathedral", "Hall", "Chamber" },
          "Reverb algorithm. Nebula: mono-in, enormous, slow fade-in. Cathedral: huge true-stereo space. "
          "Hall: medium size, fast shimmer build-up. Chamber: small, for short ambiences and chorus-like sounds.", true, true, 0 },
        { "size", "Size", U::Percent, 0, 100, 60, 0, kNone,
          "Scales all delay lengths. Fast changes bend the pitch like tape (intended), slow changes are pitch-neutral.", true, true, 0 },
        { "preDelay", "Pre-Delay", U::Ms, 0, 2000, 20, 200, kNone, "Time before the reverb starts.", true, true, 0 },
        { "preDelaySync", "Pre-Delay Sync", U::Bool, 0, 1, 0, 0, kNone, "Sync the pre-delay to the host tempo.", true, false, 0 },
        { "preDelayNote", "Pre-Delay Note", U::Choice, 0, 19, 5, 0, {}, "Pre-delay as a note value (when synced).", true, false, 0 },
        { "decay", "Decay", U::Sec, 0.3f, 120, 8, 8, kNone,
          "Reverb time (RT60) in the mid band. Use Freeze for an infinite tail.", true, true, 0 },
        { "freeze", "Freeze", U::Bool, 0, 1, 0, 0, kNone, "Hold the tail forever (infinite decay).", true, false, 0 },
        { "freezeMode", "Freeze Mode", U::Choice, 0, 1, 0, 0, { "Freeze", "Freeze + Add" },
          "Freeze blocks new input; Freeze + Add keeps layering new input onto the frozen tail.", true, false, 0 },
        { "diffusion", "Diffusion", U::Percent, 0, 100, 85, 0, kNone,
          "0 % = clear echoes (recirculating delay), 50-60 % = slow fade-in, high = dense and long even with short decay.", true, true, 0 },
        { "bloom", "Bloom", U::Sec, 0, 5, 0, 1, kNone, "Slow blooming of the tail: the wet signal swells in over roughly this time.", true, true, 0 },
        { "modRate", "Mod Rate", U::Hz, 0.01f, 5, 0.4f, 0.5f, kNone, "Speed of the delay modulation inside the tank.", true, true, 0 },
        { "modDepth", "Mod Depth", U::Percent, 0, 100, 30, 0, kNone, "Depth of the delay modulation: lushness and detuning.", true, true, 0 },
        { "modDrift", "Drift", U::Percent, 0, 100, 60, 0, kNone,
          "0 % = regular LFO (chorus-like vibrato), 100 % = smoothed random wander (no audible vibrato).", true, true, 0 },
        { "lowCut", "Low Cut", U::Hz, 10, 2000, 10, 150, kNone,
          "High-pass in the feedback path: every recirculation loses more low end (10 Hz = off).", true, true, 1 },
        { "highCut", "High Cut", U::Hz, 500, 20000, 20000, 4000, kNone,
          "Low-pass in the feedback path: the tail darkens as it fades (20 kHz = off).", true, true, 1 },
        { "lowMult", "Low Decay", U::Mult, 0.25f, 4, 1.2f, 1, kNone, "Decay time of the low band relative to Decay.", true, true, 1 },
        { "highMult", "High Decay", U::Mult, 0.1f, 2, 0.55f, 0.7f, kNone, "Decay time of the high band relative to Decay.", true, true, 1 },
        { "lowXover", "Low Crossover", U::Hz, 50, 1000, 300, 220, kNone, "Crossover between the low and mid band.", true, true, 1 },
        { "highXover", "High Crossover", U::Hz, 1000, 12000, 3500, 3500, kNone, "Crossover between the mid and high band.", true, true, 1 },
        { "color", "Color", U::Choice, 0, 1, 0, 0, { "Bright", "Dark" },
          "Bright: full bandwidth, hi-fi. Dark: late-70s/early-80s digital reverb (steep roll-off above 10 kHz, grainier modulation).", true, true, 1 },
        { "warmth", "Warmth", U::Percent, 0, 100, 0, 0, kNone, "Gentle saturation inside the loop; rounds off loud tails.", true, true, 1 },
        { "quality", "Quality", U::Choice, 0, 2, 1, 0, { "Eco", "High", "Ultra" },
          "Eco: 8 delay lines, lighter diffusion (about half the CPU). High: 16 lines. Ultra: 16 lines plus an extra diffusion stage for the smoothest onset.", false, false, 5 },
        // ---------------------------------------------------------------- input / output (5)
        { "inHP", "Input Low Cut", U::Hz, 10, 1000, 10, 100, kNone, "High-pass on the signal going into the reverb (10 Hz = off).", true, true, 5 },
        { "inLP", "Input High Cut", U::Hz, 1000, 20000, 20000, 6000, kNone, "Low-pass on the signal going into the reverb (20 kHz = off).", true, true, 5 },
        { "tilt", "Tilt", U::Db, -6, 6, 0, 0, kNone, "Tilts the wet signal around 800 Hz: negative = darker, positive = brighter.", true, true, 5 },
        { "width", "Width", U::Percent, 0, 200, 100, 0, kNone, "Stereo width of the wet signal (0 % = mono, 200 % = extra wide).", true, true, 5 },
        { "bassMono", "Bass Mono", U::Hz, 20, 500, 20, 120, kNone, "Wet signal below this frequency is mono (20 Hz = off).", true, true, 5 },
        { "mix", "Mix", U::Percent, 0, 100, 35, 0, kNone, "Dry/wet balance (equal-power). 100 % for send use.", true, false, 5 },
        { "mixLock", "Mix Lock", U::Bool, 0, 1, 0, 0, kNone, "Keep the current Mix when loading presets or randomizing.", false, false, 5 },
        { "output", "Output", U::Db, -24, 12, 0, 0, kNone, "Output level.", true, false, 5 },
        // ---------------------------------------------------------------- shimmer (2)
        { "shimMode", "Shimmer Mode", U::Choice, 0, 5, 0, 0, { "Bypass", "Single", "Dual", "Single Reverse", "Dual Reverse", "Two-Voice" },
          "Single: shift up or down. Dual: up and down at once. Reverse: reversed grains, smoother and organ-like. "
          "Two-Voice: two independent intervals.", true, true, 2 },
        { "pitch", "Pitch", U::Semis, -24, 24, 12, 0, kNone, "Shift interval (voice A). Sweeping it live glides smoothly.", true, true, 2 },
        { "pitchSnap", "Snap", U::Bool, 0, 1, 1, 0, kNone, "Snap Pitch and Pitch 2 to whole semitones.", true, false, 2 },
        { "fine", "Fine", U::Cents, -50, 50, 0, 0, kNone, "Fine tuning of both voices in cents.", true, true, 2 },
        { "pitch2", "Pitch 2", U::Semis, -24, 24, 7, 0, kNone, "Interval of voice B (Two-Voice mode).", true, true, 2 },
        { "voiceMix", "Voice Mix", U::Percent, 0, 100, 50, 0, kNone, "Balance between voice A and voice B (Two-Voice mode).", true, true, 2 },
        { "shimAmount", "Shimmer", U::Percent, 0, 100, 50, 0, kNone,
          "How much of the reverb is pitch shifted on every recirculation (In Loop) or once (Pre).", true, true, 2 },
        { "shimLowCut", "Shimmer Low Cut", U::Hz, 20, 2000, 250, 250, kNone, "High-pass on the shifted signal.", true, true, 2 },
        { "shimHighCut", "Shimmer High Cut", U::Hz, 1000, 20000, 9000, 6000, kNone, "Low-pass on the shifted signal: keeps the cascade glossy.", true, true, 2 },
        { "shimSmooth", "Smoothness", U::Percent, 0, 100, 60, 0, kNone,
          "100 % = smooth and clean. Lower = randomised grains with noisy, 'orchestral' sidebands.", true, true, 2 },
        { "shimPlace", "Placement", U::Choice, 0, 1, 0, 0, { "In Loop", "Pre" },
          "In Loop: shifted on every pass (cascading, higher and higher). Pre: shifted once before the reverb.", true, true, 2 },
        // ---------------------------------------------------------------- grains (3)
        { "grainOn", "Grains", U::Bool, 0, 1, 0, 0, kNone, "Granular engine on/off.", true, false, 3 },
        { "grainSource", "Grain Source", U::Choice, 0, 1, 0, 0, { "Input", "Tank" }, "Where the grains are taken from.", true, true, 3 },
        { "grainDest", "Grain Destination", U::Choice, 0, 1, 0, 0, { "Parallel", "Into Tank" },
          "Parallel: grains glitter over the tail. Into Tank: grains are reverberated into clouds.", true, true, 3 },
        { "grainSize", "Grain Size", U::Ms, 10, 500, 120, 100, kNone, "Length of each grain.", true, true, 3 },
        { "grainDensity", "Density", U::PerSec, 1, 80, 12, 12, kNone, "Grains per second.", true, true, 3 },
        { "grainPitch", "Grain Pitch", U::Semis, -24, 24, 12, 0, kNone, "Pitch of the grains.", true, true, 3 },
        { "grainQuant", "Quantize", U::Bool, 0, 1, 1, 0, kNone, "Quantize grain pitch (incl. jitter) to semitones.", true, true, 3 },
        { "grainSpray", "Spray", U::Ms, 0, 2000, 200, 300, kNone, "Random offset of the read position into the past.", true, true, 3 },
        { "grainPitchJit", "Pitch Jitter", U::Semis, 0, 12, 0, 0, kNone, "Random pitch deviation per grain.", true, true, 3 },
        { "grainReverse", "Reverse", U::Percent, 0, 100, 0, 0, kNone, "Probability that a grain plays backwards.", true, true, 3 },
        { "grainScatter", "Scatter", U::Percent, 0, 100, 60, 0, kNone, "Random stereo position of the grains.", true, true, 3 },
        { "grainAmount", "Grain Amount", U::Percent, 0, 100, 40, 0, kNone, "Level of the grains.", true, true, 3 },
        // ---------------------------------------------------------------- delay (4)
        { "delayOn", "Delay", U::Bool, 0, 1, 0, 0, kNone, "Echo in front of the reverb on/off.", true, false, 4 },
        { "delayTime", "Delay Time", U::Ms, 1, 4000, 375, 400, kNone, "Delay time (when not synced).", true, true, 4 },
        { "delaySync", "Delay Sync", U::Bool, 0, 1, 1, 0, kNone, "Sync the delay time to the host tempo.", true, false, 4 },
        { "delayNote", "Delay Note", U::Choice, 0, 19, 9, 0, {}, "Delay time as a note value (when synced).", true, true, 4 },
        { "delayFeedback", "Delay Feedback", U::Percent, 0, 98, 35, 0, kNone, "Repeats; up to just below self-oscillation.", true, true, 4 },
        { "delayPingPong", "Ping-Pong", U::Bool, 0, 1, 0, 0, kNone, "Repeats alternate between left and right.", true, true, 4 },
        { "delayOffset", "Stereo Offset", U::Ms, -50, 50, 0, 0, kNone, "Right channel delay time offset (widens ping-pong).", true, true, 4 },
        { "delayHP", "Delay Low Cut", U::Hz, 20, 2000, 80, 200, kNone, "High-pass in the delay feedback path.", true, true, 4 },
        { "delayLP", "Delay High Cut", U::Hz, 1000, 20000, 8000, 6000, kNone, "Low-pass in the delay feedback path.", true, true, 4 },
        { "delayMod", "Delay Mod", U::Percent, 0, 100, 10, 0, kNone, "Subtle tape-like modulation of the delay.", true, true, 4 },
        { "delayToReverb", "Delay > Reverb", U::Percent, 0, 100, 100, 0, kNone, "How much of the delay feeds the reverb.", true, true, 4 },
        { "delayLevel", "Delay Direct", U::Db, -60, 6, -60, 0, kNone, "Delay straight to the wet output, without reverb (-inf = off).", true, true, 4 },
        // ---------------------------------------------------------------- clear tail
        { "clearTail", "Clear Tail", U::Bool, 0, 1, 0, 0, kNone,
          "Rising edge (0 -> 1) fades the tail out within a few ms and clears all buffers. Automate a short spike to trigger it.", true, false, 5 },
        { "clearTarget", "Clear Target", U::Choice, 0, 1, 1, 0, { "Reverb", "Reverb + Delay" }, "What Clear Tail clears.", true, false, 5 },
        { "autoClear", "Auto Clear", U::Bool, 0, 1, 0, 0, kNone, "Clear the tail automatically when the host transport starts.", true, false, 5 },
        // ---------------------------------------------------------------- ducking (6)
        { "duckOn", "Ducking On", U::Bool, 0, 1, 0, 0, kNone, "Ducking on/off: lowers the wet signal while you play.", true, false, 6 },
        { "duckSource", "Duck Source", U::Choice, 0, 1, 0, 0, { "Input", "Sidechain" }, "Detector input: the plugin input or the sidechain bus.", true, false, 6 },
        { "duckHP", "Duck Detector HP", U::Hz, 20, 1000, 100, 100, kNone, "High-pass on the detector signal.", true, true, 6 },
        { "duckLP", "Duck Detector LP", U::Hz, 1000, 20000, 20000, 5000, kNone, "Low-pass on the detector signal.", true, true, 6 },
        { "duckDetMode", "Duck Detector", U::Choice, 0, 1, 0, 0, { "Peak", "RMS" }, "Peak reacts to transients, RMS to loudness.", true, true, 6 },
        { "duckRms", "Duck RMS Window", U::Ms, 10, 100, 30, 40, kNone, "Averaging time of the RMS detector.", true, true, 6 },
        { "duckThresh", "Duck Threshold", U::Db, -60, 0, -24, 0, kNone, "Level above which ducking starts.", true, true, 6 },
        { "duckDepth", "Duck Depth", U::Db, -48, 0, -12, 0, kNone, "Maximum gain reduction.", true, true, 6 },
        { "duckFull", "Duck Full", U::Bool, 0, 1, 0, 0, kNone, "Duck to silence.", true, false, 6 },
        { "duckKnee", "Duck Knee", U::Db, 0, 12, 6, 0, kNone, "Soft knee width around the threshold.", true, true, 6 },
        { "duckAttack", "Duck Attack", U::Ms, 0.1f, 100, 5, 10, kNone, "How fast the ducking engages.", true, true, 6 },
        { "duckHold", "Duck Hold", U::Ms, 0, 500, 50, 100, kNone, "How long the reduction is held after the input drops.", true, true, 6 },
        { "duckRelease", "Duck Release", U::Ms, 10, 3000, 250, 300, kNone, "How fast the wet signal comes back.", true, true, 6 },
        { "duckRelSync", "Duck Release Sync", U::Bool, 0, 1, 0, 0, kNone, "Release as a note value.", true, false, 6 },
        { "duckRelNote", "Duck Release Note", U::Choice, 0, 4, 1, 0, { "1/16", "1/8", "1/4", "1/2", "1 bar" }, "Synced release time.", true, true, 6 },
        { "duckLook", "Duck Lookahead", U::Ms, 0, 10, 0, 3, kNone, "Lookahead (adds latency, reported to the host).", true, false, 6 },
        { "duckTarget", "Duck Target", U::Choice, 0, 2, 0, 0, { "All Wet", "Reverb", "Delay" }, "What is ducked.", true, true, 6 },
        { "duckPos", "Duck Position", U::Choice, 0, 1, 0, 0, { "Post", "Pre" },
          "Post: duck the wet output. Pre: duck the signal going into the reverb/delay.", true, true, 6 },
        // ---------------------------------------------------------------- morph
        { "morphOn", "Morph On", U::Bool, 0, 1, 0, 0, kNone, "Play the morph between snapshot A and B instead of the knob settings.", true, false, 5 },
        { "morph", "Morph", U::Percent, 0, 100, 0, 0, kNone, "Position between snapshot A (0 %) and B (100 %).", true, false, 5 },
    };
    return t;
}
// clang-format on

juce::String fmtHz (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}
} // namespace

const ParamInfo& info (P p)
{
    static_assert (kNumParams == 88, "update the parameter table");
    jassert (table().size() == (size_t) kNumParams);
    return table()[(size_t) p];
}

juce::String formatValue (P p, float v)
{
    const auto& i = info (p);
    switch (i.unit)
    {
        case Unit::Percent: return juce::String (juce::roundToInt (v)) + " %";
        case Unit::Hz:
            if (p == P::lowCut && v <= 10.5f) return "Off";
            if ((p == P::highCut || p == P::inLP || p == P::duckLP) && v >= 19900.0f) return "Off";
            if (p == P::inHP && v <= 10.5f) return "Off";
            if (p == P::bassMono && v <= 20.5f) return "Off";
            return v < 10.0f ? juce::String (v, 2) + " Hz" : fmtHz (v);
        case Unit::Ms:
            if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
            if (std::abs (v) < 10.0f) return juce::String (v, 1) + " ms";
            return juce::String (juce::roundToInt (v)) + " ms";
        case Unit::Sec:
            if (p == P::bloom && v < 0.005f) return "Off";
            return v < 10.0f ? juce::String (v, 2) + " s" : juce::String (v, 1) + " s";
        case Unit::Db:
            if (p == P::delayLevel && v <= -59.9f) return "-inf dB";
            return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
        case Unit::Semis: return (v > 0.005f ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.005f ? 0 : 2) + " st";
        case Unit::Cents: return (v > 0.5f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct";
        case Unit::Mult: return juce::String (v, 2) + " x";
        case Unit::PerSec: return juce::String (v, v < 10.0f ? 1 : 0) + " /s";
        case Unit::Bool: return v > 0.5f ? "On" : "Off";
        case Unit::Choice:
        {
            const auto& c = i.choices.isEmpty() ? noteNames() : i.choices;
            return c[juce::jlimit (0, c.size() - 1, juce::roundToInt (v))];
        }
    }
    return juce::String (v);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int k = 0; k < kNumParams; ++k)
    {
        const auto p = (P) k;
        const auto& i = info (p);
        const juce::ParameterID id { i.id, kParamVersion };
        if (i.unit == Unit::Bool)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (
                id, i.name, i.def > 0.5f, juce::AudioParameterBoolAttributes().withAutomatable (i.automatable)));
        }
        else if (i.unit == Unit::Choice)
        {
            const auto& c = i.choices.isEmpty() ? noteNames() : i.choices;
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                id, i.name, c, (int) i.def, juce::AudioParameterChoiceAttributes().withAutomatable (i.automatable)));
        }
        else
        {
            float step = 0.0f;
            switch (i.unit)
            {
                case Unit::Percent: step = 0.1f; break;
                case Unit::Semis: step = 0.01f; break;
                case Unit::Cents: step = 1.0f; break;
                case Unit::Db: step = 0.1f; break;
                default: step = 0.0f; break;
            }
            juce::NormalisableRange<float> r (i.min, i.max, step);
            if (i.centre > i.min && i.centre < i.max)
                r.setSkewForCentre (i.centre);
            const char* label = "";
            switch (i.unit)
            {
                case Unit::Percent: label = "%"; break;
                case Unit::Hz: label = "Hz"; break;
                case Unit::Ms: label = "ms"; break;
                case Unit::Sec: label = "s"; break;
                case Unit::Db: label = "dB"; break;
                case Unit::Semis: label = "st"; break;
                case Unit::Cents: label = "ct"; break;
                case Unit::Mult: label = "x"; break;
                case Unit::PerSec: label = "/s"; break;
                default: break;
            }
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                id, i.name, r, i.def,
                juce::AudioParameterFloatAttributes()
                    .withLabel (label)
                    .withAutomatable (i.automatable)
                    .withStringFromValueFunction ([p] (float v, int) { return formatValue (p, v); })));
        }
    }
    return layout;
}
} // namespace schimmer
