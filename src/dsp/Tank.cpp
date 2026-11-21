#include "Tank.h"

namespace schimmer::dsp
{
namespace
{
constexpr float kMaxScale = 2.0f;
constexpr float kNominalLineRms = 0.7f;
constexpr float kInputCal = 2.7f;          // calibrated: impulse energy gain ~ -1 dB at T60 = 2 s
}

void Tank::prepare (float sampleRate, int numLines)
{
    fs = sampleRate;
    N = numLines >= 16 ? 16 : 8;

    float maxMean = 0.0f, maxAp = 0.0f, maxMod = 0.0f, maxSpread = 0.0f;
    for (const auto& m : kModes)
    {
        maxMean = std::max (maxMean, m.tankMeanMs);
        maxAp = std::max (maxAp, m.loopApMs);
        maxMod = std::max (maxMod, m.maxModMs);
        maxSpread = std::max (maxSpread, m.tankSpread);
    }
    const float maxLineMs = maxMean * kMaxScale * std::sqrt (maxSpread) * 1.15f + maxMod * 1.5f * 2.0f + 5.0f;
    const float maxApMs = maxAp * kMaxScale * 1.5f + 2.0f;

    Rng r (0x5c1e771u);
    // line lengths: geometric spread with jitter inside equal sub-ranges, shuffled
    int order[kMaxLines];
    for (int i = 0; i < N; ++i) order[i] = i;
    for (int i = N - 1; i > 0; --i) std::swap (order[i], order[r.next() % (uint32_t) (i + 1)]);
    for (int i = 0; i < N; ++i)
    {
        rel[i] = ((float) order[i] + 0.2f + 0.6f * r.uniform()) / (float) N;  // 0..1 position in the spread
        apRel[i] = 0.55f + 0.9f * r.uniform();
        buf[i].allocate ((int) std::ceil (maxLineMs * 0.001f * fs) + 8);
        ap[i].allocate ((int) std::ceil (maxApMs * 0.001f * fs) + 8);
        shifter[i].prepare (fs, 0xabc000u + (uint32_t) i * 7919u);
        perm[i] = i;
        flip[i] = (r.next() & 1u) ? -1.0f : 1.0f;
        inSign[i] = (r.next() & 1u) ? -1.0f : 1.0f;
        lfoRate[i] = 0.62f + 0.99f * std::fmod (0.6180339887f * (float) (i + 1), 1.0f);
        lfoPhase[i] = r.uniform() * kTwoPi;
    }
    for (int i = N - 1; i > 0; --i) std::swap (perm[i], perm[r.next() % (uint32_t) (i + 1)]);

    // input: lines of the first half get the L half of the diffuser (channels 0..3),
    // the second half the R half (4..7). For N = 8, line i <- channel i.
    for (int i = 0; i < N; ++i)
        inCh[i] = N == 16 ? ((i < 8 ? 0 : 4) + (i & 3)) : i;

    // output taps: two orthogonal rows of the 8-point Hadamard, repeated on both halves,
    // weighted towards the own side (set in setParams via the mode's stereo spread).
    // shimmer order: alternate halves so a partial amount is spread over L and R.
    for (int k = 0; k < N; ++k)
        shimmerOrder[k] = N == 16 ? ((k & 1) ? 8 : 0) + ((k >> 1) * 5 + 3) % 8 : (k * 5 + 3) % 8;

    reset();
}

void Tank::reset()
{
    for (int i = 0; i < N; ++i)
    {
        buf[i].reset();
        ap[i].reset();
        shelfLo[i].reset(); shelfHi[i].reset(); lowCut[i].reset(); highCut[i].reset(); darkLp[i].reset();
        shHp[i].reset(); shLp[i].reset();
        dc[i].reset();
        shifter[i].reset();
        rndPrev[i] = rndNext[i] = 0.0f;
        rndPhase[i] = rng.uniform();
    }
    limGain = 1.0f;
    energyAcc = 0.0;
    first = true;
}

void Tank::clear() noexcept
{
    for (int i = 0; i < N; ++i)
    {
        buf[i].clearFromNow();
        ap[i].clearFromNow();
        shelfLo[i].reset(); shelfHi[i].reset(); lowCut[i].reset(); highCut[i].reset(); darkLp[i].reset();
        shHp[i].reset(); shLp[i].reset();
        dc[i].reset();
        shifter[i].clear();
    }
    limGain = 1.0f;
    limRamp.snap (1.0f);
}

void Tank::setParams (const TankParams& p, int n) noexcept
{
    chunkN = n;
    const auto& cfg = kModes[std::clamp (p.mode, 0, kNumModes - 1)];
    if (first)
    {
        sScale.setTime (0.35f, fs);
        sSpread.setTime (0.4f, fs);
        sLogT60.setTime (0.08f, fs);
        sLowMult.setTime (0.08f, fs);
        sHighMult.setTime (0.08f, fs);
        sFreeze.setTime (0.08f, fs);
        sLowCutMix.setTime (0.05f, fs);
        sHighCutMix.setTime (0.05f, fs);
        sDarkMix.setTime (0.05f, fs);
        sLowCutHz.setTime (0.05f, fs);
        sHighCutHz.setTime (0.05f, fs);
        sWarmth.setTime (0.05f, fs);
        sApG.setTime (0.1f, fs);
        sDepth.setTime (0.1f, fs);
        sInGain.setTime (0.1f, fs);
    }

    const float spreadTarget = lerp (1.05f, cfg.tankSpread, smoothstep (p.diffusion / 0.25f));
    const float scale = first ? (sScale.snap (p.sizeScale), p.sizeScale) : sScale.next (p.sizeScale, n);
    const float spread = first ? (sSpread.snap (spreadTarget), spreadTarget) : sSpread.next (spreadTarget, n);
    const float logT60 = std::log (std::max (0.05f, p.decaySec));
    if (first) { sLogT60.snap (logT60); sLowMult.snap (p.lowMult); sHighMult.snap (p.highMult); sFreeze.snap (p.freeze ? 1.0f : 0.0f); }
    const float t60 = std::exp (sLogT60.next (logT60, n));
    const float lowMult = sLowMult.next (p.lowMult, n);
    const float highMult = sHighMult.next (p.highMult, n);
    const float frz = sFreeze.next (p.freeze ? 1.0f : 0.0f, n);

    hermiteReads = ! p.dark;

    // in-loop diffusion (allpass coefficient)
    const float apTarget = 0.62f * smoothstep ((p.diffusion - 0.3f) / 0.7f);
    if (first) sApG.snap (apTarget);
    apG.setTarget (sApG.next (apTarget, n), n);

    // modulation depth in samples
    const float depthTarget = p.modDepth * cfg.maxModMs * std::sqrt (scale) * 0.001f * fs;
    if (first) sDepth.snap (depthTarget);
    const float depth = sDepth.next (depthTarget, n);
    const float dt = (float) n / fs;

    // shimmer assignment
    const auto& sh = p.shimmer;
    const bool shimOn = sh.mode != ShimmerMode::Bypass && sh.amount > 0.01f;
    const float grainSec = lerp (0.035f, 0.11f, sh.smooth);
    const float jitter = 1.0f - sh.smooth;
    const bool rev = sh.mode == ShimmerMode::SingleReverse || sh.mode == ShimmerMode::DualReverse;
    float shimTarget[kMaxLines] {};
    for (int k = 0; k < N; ++k)
    {
        const int i = shimmerOrder[k];
        // perceptual taper: the shifted fraction per pass is amount^2 (100 % = everything, like ValhallaShimmer)
        const float f = shimOn ? clampf (sh.amount * sh.amount * (float) N - (float) k, 0.0f, 1.0f) : 0.0f;
        shimTarget[i] = f;
        float semis = sh.semis;
        switch (sh.mode)
        {
            case ShimmerMode::Dual:
            case ShimmerMode::DualReverse:
                semis = (k & 1) ? -std::abs (sh.semis) : std::abs (sh.semis);
                break;
            case ShimmerMode::TwoVoice:
                semis = std::fmod (0.6180339887f * (float) k + 0.31f, 1.0f) < sh.voiceMix ? sh.semis2 : sh.semis;
                break;
            default: break;
        }
        voice[i].ratio = std::pow (2.0f, semis / 12.0f);
        voice[i].reverse = rev;
        voice[i].grainSec = grainSec;
        voice[i].jitter = jitter;
    }

    meanLoopSec = 0.0f;
    float minLen = 1.0e9f;
    float sumG2 = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        // ---- lengths
        const float lineMs = cfg.tankMeanMs * scale * std::pow (spread, rel[i] - 0.5f);
        // modulation: LFO + smoothed random drift
        lfoPhase[i] += kTwoPi * p.modRateHz * lfoRate[i] * dt;
        if (lfoPhase[i] > kTwoPi) lfoPhase[i] -= kTwoPi;
        rndPhase[i] += p.modRateHz * lfoRate[i] * 0.7f * dt;
        if (rndPhase[i] >= 1.0f)
        {
            rndPhase[i] -= std::floor (rndPhase[i]);
            rndPrev[i] = rndNext[i];
            rndNext[i] = rng.bipolar();
        }
        const float rnd = lerp (rndPrev[i], rndNext[i], smoothstep (rndPhase[i]));
        const float m = (1.0f - p.drift) * std::sin (lfoPhase[i]) + p.drift * rnd;
        const float lenS = std::max (4.0f, lineMs * 0.001f * fs + depth * (1.0f + m));
        // integer target: linear interpolation inside an allpass loop is not allpass (HF ripple)
        const float apS = std::max (2.0f, std::round (cfg.loopApMs * apRel[i] * scale * 0.001f * fs));
        if (first) { len[i].snap (lenS); apLen[i].snap (apS); }
        len[i].setTarget (lenS, n);
        apLen[i].setTarget (apS, n);

        // ---- shimmer
        const bool wasActive = shActive[i];
        shActive[i] = shimTarget[i] > 0.0f || shMix[i].value > 1.0e-4f;
        if (shActive[i] && ! wasActive) shMix[i].snap (0.0f);
        shMix[i].setTarget (shimTarget[i], n);
        shifter[i].setAntiAlias (voice[i].ratio);
        shHp[i].setCutoff (sh.lowCutHz, fs);
        shLp[i].setCutoff (sh.highCutHz, fs);
        const float r = voice[i].ratio;
        const float shExtra = shimTarget[i] * (rev ? (1.0f + r) : std::abs (r - 1.0f)) * grainSec * 0.5f * fs;

        // ---- decay gains (attenuation proportional to the loop length)
        const float loopSec = (lenS + apS + shExtra) / fs;
        meanLoopSec += loopSec / (float) N;
        minLen = std::min (minLen, lenS / fs);
        const float gm = decayGain (loopSec, t60);
        const float gl = decayGain (loopSec, t60 * lowMult);
        const float gh = decayGain (loopSec, t60 * highMult);
        const float gmT = lerp (gm, 1.0f, frz);
        const float alT = lerp (gl / std::max (gm, 1.0e-6f), 1.0f, frz);
        const float ahT = lerp (gh / std::max (gm, 1.0e-6f), 1.0f, frz);
        if (first) { gM[i].snap (gmT); aL[i].snap (alT); aH[i].snap (ahT); }
        gM[i].setTarget (gmT, n);
        aL[i].setTarget (alT, n);
        aH[i].setTarget (ahT, n);
        sumG2 += gm * gm;

        shelfLo[i].setCutoff (p.lowXHz, fs);
        shelfHi[i].setCutoff (p.highXHz, fs);
    }
    firstArrivalSec = minLen;

    // ---- in-loop filters (bypassed in freeze)
    const float lcHz = first ? (sLowCutHz.snap (p.lowCutHz), p.lowCutHz) : sLowCutHz.next (p.lowCutHz, n);
    const float hcHz = first ? (sHighCutHz.snap (p.highCutHz), p.highCutHz) : sHighCutHz.next (p.highCutHz, n);
    for (int i = 0; i < N; ++i)
    {
        lowCut[i].setCutoff (lcHz, fs);
        highCut[i].setCutoff (hcHz, fs);
        darkLp[i].setCutoff (9000.0f, fs);
        dc[i].set (4.0f, fs);
    }
    const float lcmT = (p.lowCutHz > 10.5f ? 1.0f : 0.0f) * (1.0f - frz);
    const float hcmT = (p.highCutHz < 19900.0f ? 1.0f : 0.0f) * (1.0f - frz);
    const float dkmT = (p.dark ? 1.0f : 0.0f) * (1.0f - frz);
    if (first) { sLowCutMix.snap (lcmT); sHighCutMix.snap (hcmT); sDarkMix.snap (dkmT); }
    lcMix.setTarget (sLowCutMix.next (lcmT, n), n);
    hcMix.setTarget (sHighCutMix.next (hcmT, n), n);
    dkMix.setTarget (sDarkMix.next (dkmT, n), n);
    const float wkT = p.warmth * 3.0f;
    if (first) sWarmth.snap (wkT);
    warmK.setTarget (sWarmth.next (wkT, n), n);

    // ---- output taps (orthogonal, weighted towards the own side in stereo modes)
    const float theta = 0.7853982f * (1.0f - (cfg.monoIn ? 0.0f : cfg.stereoSpread) * 0.55f);
    const float a = std::cos (theta), b = std::sin (theta);
    const float tapNorm = 1.0f / std::sqrt ((float) N * 0.5f);
    for (int i = 0; i < N; ++i)
    {
        const int j = i & 7;
        const float hl = hadamardSign (3, j), hr = hadamardSign (5, j);
        const bool leftHalf = N == 16 ? i < 8 : (i & 1) == 0;
        cL[i] = tapNorm * hl * (leftHalf ? a : b);
        cR[i] = tapNorm * hr * (leftHalf ? b : a);
    }

    // ---- input gain: level normalisation across modes, sizes and decay times.
    // The tail energy of an FDN is ~ 1 / (1 - g^2) (g = gain per pass), so a
    // longer loop or a shorter decay gives less energy. We normalise that away
    // and let the level grow only gently with the decay time: (T60 / 2 s)^0.4.
    const float t60Eff = lerp (t60, 60.0f, frz);
    const float g2 = lerp (sumG2 / (float) N, decayGain (meanLoopSec, 60.0f) * decayGain (meanLoopSec, 60.0f), frz);
    const float e = 1.0f / std::max (1.0e-5f, 1.0f - g2);
    const float igT = kInputCal * std::sqrt (std::pow (std::max (0.1f, t60Eff) / 2.0f, 0.4f) / e);
    if (first) sInGain.snap (igT);
    inGain.setTarget (sInGain.next (igT, n), n);

    limRamp.setTarget (limGain, n);
    if (first) limRamp.snap (limGain);
    first = false;
}

void Tank::endChunk (int n) noexcept
{
    const float rms = (float) std::sqrt (energyAcc / (double) (n * N));
    energyAcc = 0.0;
    if (! std::isfinite (rms))
    {
        ++nanResets;
        for (int i = 0; i < N; ++i)
        {
            buf[i].reset();
            ap[i].reset();
        }
        clear();
        return;
    }
    const float target = rms > kNominalLineRms ? kNominalLineRms / rms : 1.0f;
    const float c = target < limGain ? 0.5f : 1.0f - std::exp (-(float) n / (0.3f * fs));
    limGain += c * (target - limGain);
    limGain = clampf (limGain, 0.05f, 1.0f);
}
} // namespace schimmer::dsp
