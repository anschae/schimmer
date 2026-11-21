#pragma once
// The reverb tank: a modulated feedback delay network with 16 (or 8) lines.
//
// Per line and sample:
//   read (modulated, Hermite or linear) -> in-loop allpass (Diffusion)
//   -> absorption (gain + low/high shelf: exact per-band decay times)
//   -> low cut / high cut / Dark LP -> DC blocker -> warmth
//   -> [output taps] -> permutation/flip + Hadamard mix
//   -> shimmer (granular shifter replaces the signal of a fraction of lines)
//   -> safety limiter gain -> + input -> write
// See docs/RESEARCH.md sections 2, 4, 5, 7 and 9.

#include "Filters.h"
#include "GrainShifter.h"
#include "Hadamard.h"
#include "Modes.h"
#include "RingBuffer.h"

namespace schimmer::dsp
{
enum class ShimmerMode : int { Bypass = 0, Single, Dual, SingleReverse, DualReverse, TwoVoice };

struct ShimmerParams
{
    ShimmerMode mode = ShimmerMode::Bypass;
    float semis = 12.0f;     // voice A (Single/Dual: +/- this amount)
    float semis2 = 7.0f;     // voice B (Two-Voice)
    float voiceMix = 0.5f;   // 0 = all voice A, 1 = all voice B (Two-Voice)
    float amount = 0.0f;     // fraction of the feedback that is shifted (0..1)
    float lowCutHz = 200.0f, highCutHz = 9000.0f;
    float smooth = 0.6f;     // 0 = noisy/orchestral, 1 = smooth
};

struct TankParams
{
    int mode = 0;
    float sizeScale = 1.0f;
    float decaySec = 4.0f;
    bool freeze = false;
    float lowMult = 1.0f, highMult = 0.6f;
    float lowXHz = 250.0f, highXHz = 4000.0f;
    float lowCutHz = 10.0f, highCutHz = 20000.0f;
    float diffusion = 0.8f;
    float modRateHz = 0.5f, modDepth = 0.3f, drift = 0.5f;
    bool dark = false;
    float warmth = 0.0f;
    ShimmerParams shimmer;     // in-loop shimmer (amount 0 when placed "Pre")
};

class Tank
{
public:
    static constexpr int kMaxLines = 16;

    void prepare (float sampleRate, int numLines);
    void reset();
    void clear() noexcept;   // O(1) buffer clear + filter states

    /** Once per chunk (n samples). */
    void setParams (const TankParams& p, int n) noexcept;

    /** in8: 8 channels from the diffuser/bloom network. */
    inline void processSample (const float* in8, float& outL, float& outR) noexcept;

    /** Call after each chunk: safety limiter + NaN guard. */
    void endChunk (int n) noexcept;

    int getNumLines() const noexcept { return N; }
    float getLimiterGain() const noexcept { return limGain; }
    float getMeanLoopSeconds() const noexcept { return meanLoopSec; }
    /** Seconds from tank input to the first output (shortest line). */
    float getFirstArrivalSeconds() const noexcept { return firstArrivalSec; }
    bool hadNonFinite() const noexcept { return nanResets > 0; }

private:
    float fs = 48000.0f;
    int N = 16;
    int chunkN = 32;
    bool first = true;

    RingBuffer buf[kMaxLines], ap[kMaxLines];
    float rel[kMaxLines] {}, apRel[kMaxLines] {};
    int perm[kMaxLines] {};
    float flip[kMaxLines] {}, inSign[kMaxLines] {}, cL[kMaxLines] {}, cR[kMaxLines] {};
    int inCh[kMaxLines] {};
    int shimmerOrder[kMaxLines] {};

    Ramp len[kMaxLines], apLen[kMaxLines], gM[kMaxLines], aL[kMaxLines], aH[kMaxLines], shMix[kMaxLines];
    OnePole shelfLo[kMaxLines], shelfHi[kMaxLines], lowCut[kMaxLines], highCut[kMaxLines], darkLp[kMaxLines];
    OnePole shHp[kMaxLines], shLp[kMaxLines];
    DcBlock dc[kMaxLines];
    GrainShifter shifter[kMaxLines];
    ShiftVoice voice[kMaxLines];
    bool shActive[kMaxLines] {};

    // modulation
    float lfoPhase[kMaxLines] {}, lfoRate[kMaxLines] {};
    float rndPrev[kMaxLines] {}, rndNext[kMaxLines] {}, rndPhase[kMaxLines] {};
    Rng rng { 0x7a4b11u };

    // smoothed controls (chunk rate)
    Smoother sScale, sSpread, sLogT60, sLowMult, sHighMult, sFreeze, sLowCutMix, sHighCutMix, sDarkMix;
    Smoother sLowCutHz, sHighCutHz, sWarmth, sApG, sDepth, sInGain;
    Ramp apG, lcMix, hcMix, dkMix, warmK, inGain, limRamp;

    float limGain = 1.0f;
    double energyAcc = 0.0;
    int nanResets = 0;
    bool nanFlag = false;
    float meanLoopSec = 0.1f, firstArrivalSec = 0.05f;
    bool hermiteReads = true;
};

// ---------------------------------------------------------------------------- inline
inline void Tank::processSample (const float* in8, float& outL, float& outR) noexcept
{
    float y[kMaxLines];
    const float g = apG.next();
    const float lcm = lcMix.next(), hcm = hcMix.next(), dkm = dkMix.next();
    const float wk = warmK.next();
    const float ig = inGain.next();
    const float lg = limRamp.next();

    for (int i = 0; i < N; ++i)
        y[i] = buf[i].read (len[i].next(), hermiteReads);

    // in-loop allpass
    for (int i = 0; i < N; ++i)
    {
        const float a = ap[i].readLinear (apLen[i].next());
        const float w = y[i] + g * a;
        ap[i].write (w);
        y[i] = a - g * w;
    }

    // output taps (before the loop attenuation, so the first pass has unity gain)
    float l = 0.0f, r = 0.0f;
    double e = 0.0;
    for (int i = 0; i < N; ++i)
    {
        l += cL[i] * y[i];
        r += cR[i] * y[i];
        e += (double) y[i] * y[i];
    }
    energyAcc += e;
    outL = l;
    outR = r;

    // absorption, filters, warmth
    for (int i = 0; i < N; ++i)
    {
        float x = shelfLo[i].lowShelf (y[i], aL[i].next());
        x = shelfHi[i].highShelf (x, aH[i].next());
        x *= gM[i].next();
        x += lcm * (lowCut[i].highpass (x) - x);
        x += hcm * (highCut[i].lowpass (x) - x);
        x += dkm * (darkLp[i].lowpass (x) - x);
        x = dc[i].process (x);
        if (wk > 0.001f)
            x = softClip (x * wk) / wk;
        y[i] = x;
    }

    float v[kMaxLines];
    for (int i = 0; i < N; ++i)
        v[perm[i]] = flip[i] * y[i];
    hadamard (v, N);

    for (int i = 0; i < N; ++i)
    {
        const float m = shMix[i].next();
        shifter[i].push (v[i]);
        if (shActive[i])
        {
            float s = shifter[i].process (voice[i]);
            s = shHp[i].highpass (s);
            s = shLp[i].lowpass (s);
            v[i] += m * (s - v[i]);
        }
    }

    for (int i = 0; i < N; ++i)
        buf[i].write (v[i] * lg + ig * inSign[i] * in8[inCh[i]]);
}
} // namespace schimmer::dsp
