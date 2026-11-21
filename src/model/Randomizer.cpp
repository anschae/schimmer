#include "Randomizer.h"
#include "PluginProcessor.h"

namespace schimmer::Randomizer
{
Range rangeFor (P p)
{
    switch (p)
    {
        case P::size: return { 35, 100, false };
        case P::preDelay: return { 0, 120, false };
        case P::decay: return { 2.5f, 40, true };
        case P::diffusion: return { 55, 100, false };
        case P::bloom: return { 0, 3, false };
        case P::modRate: return { 0.08f, 1.5f, true };
        case P::modDepth: return { 10, 65, false };
        case P::modDrift: return { 20, 100, false };
        case P::lowCut: return { 10, 180, true };
        case P::highCut: return { 3500, 20000, true };
        case P::lowMult: return { 0.6f, 1.6f, false };
        case P::highMult: return { 0.3f, 0.9f, false };
        case P::lowXover: return { 150, 500, true };
        case P::highXover: return { 2000, 6000, true };
        case P::warmth: return { 0, 40, false };
        case P::inHP: return { 10, 200, true };
        case P::inLP: return { 6000, 20000, true };
        case P::tilt: return { -3, 1.5f, false };
        case P::width: return { 90, 150, false };
        case P::bassMono: return { 20, 200, true };
        case P::fine: return { -8, 8, false };
        case P::voiceMix: return { 20, 80, false };
        case P::shimAmount: return { 20, 70, false };
        case P::shimLowCut: return { 100, 600, true };
        case P::shimHighCut: return { 4000, 12000, true };
        case P::shimSmooth: return { 15, 95, false };
        case P::grainSize: return { 40, 300, true };
        case P::grainDensity: return { 4, 40, true };
        case P::grainSpray: return { 50, 1200, true };
        case P::grainPitchJit: return { 0, 7, false };
        case P::grainReverse: return { 0, 60, false };
        case P::grainScatter: return { 30, 100, false };
        case P::grainAmount: return { 20, 60, false };
        case P::delayFeedback: return { 15, 65, false };
        case P::delayOffset: return { -20, 20, false };
        case P::delayHP: return { 40, 400, true };
        case P::delayLP: return { 2500, 12000, true };
        case P::delayMod: return { 0, 40, false };
        case P::delayToReverb: return { 40, 100, false };
        case P::duckThresh: return { -40, -15, false };
        case P::duckDepth: return { -18, -4, false };
        case P::duckAttack: return { 1, 30, true };
        case P::duckRelease: return { 100, 900, true };
        default:
        {
            const auto& i = info (p);
            return { i.min, i.max, false };
        }
    }
}

namespace
{
float pickChoice (P p, juce::Random& rng)
{
    // musically weighted choices
    switch (p)
    {
        case P::shimMode:   // Bypass 35 %, otherwise any shimmer mode
            return rng.nextFloat() < 0.35f ? 0.0f : (float) (1 + rng.nextInt (5));
        case P::pitch:
        case P::pitch2:
        case P::grainPitch:
        {
            static const float intervals[] = { 12, 12, 12, 7, 5, -12, 19, 24, -5, 0 };
            return intervals[rng.nextInt ((int) std::size (intervals))];
        }
        case P::delayNote:
        {
            static const int notes[] = { 8, 9, 11, 12, 10, 7 };   // 1/8, 1/8D, 1/4, 1/4D, 1/4T, 1/8T
            return (float) notes[rng.nextInt ((int) std::size (notes))];
        }
        case P::color: return rng.nextFloat() < 0.25f ? 1.0f : 0.0f;
        default:
        {
            const auto& i = info (p);
            return (float) rng.nextInt ((int) i.max - (int) i.min + 1) + i.min;
        }
    }
}
} // namespace

void randomize (SchimmerProcessor& proc, uint32_t locks, juce::Random& rng)
{
    for (int k = 0; k < kNumParams; ++k)
    {
        const auto p = (P) k;
        const auto& i = info (p);
        if (! i.randomizable || (locks >> i.section) & 1u)
            continue;
        float v;
        if (i.unit == Unit::Choice || p == P::pitch || p == P::pitch2 || p == P::grainPitch)
            v = pickChoice (p, rng);
        else if (i.unit == Unit::Bool)
            v = rng.nextBool() ? 1.0f : 0.0f;
        else
        {
            const auto r = rangeFor (p);
            const float t = rng.nextFloat();
            v = r.log ? r.lo * std::pow (r.hi / r.lo, t) : r.lo + t * (r.hi - r.lo);
        }
        proc.setValueNotifying (p, v);
    }
    proc.setCurrentPresetName ("Randomized");
}
} // namespace schimmer::Randomizer
