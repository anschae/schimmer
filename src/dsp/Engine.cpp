#include "Engine.h"

namespace schimmer::dsp
{
namespace
{
constexpr float kReverbOutGain = 1.0f;   // calibrated by tests/RenderChecks (level matching)
constexpr float kMaxPreDelaySec = 2.0f;
constexpr float kClearFadeSec = 0.005f;
}

void Engine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    fs = sampleRate;
    fsf = (float) sampleRate;
    float maxDiffRange = 0.0f;
    for (const auto& m : kModes) maxDiffRange = std::max (maxDiffRange, m.diffRangeMs);
    diffuser.prepare (fsf, maxDiffRange * 2.0f + 2.0f);
    bloom.prepare (fsf);
    tank.prepare (fsf, quality == Quality::Eco ? 8 : 16);
    echo.prepare (fsf);
    grains.prepare (fsf);
    ducker.prepare (fs);
    for (int i = 0; i < 4; ++i) preShift[i].prepare (fsf, 0x9e11u + (uint32_t) i * 104729u);
    for (auto& b : preDelay) b.allocate ((int) std::ceil (kMaxPreDelaySec * fsf) + 64);
    for (auto& b : lookahead) b.allocate ((int) std::ceil (kMaxLookaheadMs * 0.001f * fsf) + kChunk + 8);
    meterLen = std::max (1, (int) (fs * 0.01));
    reset();
}

void Engine::setQuality (Quality q)
{
    // Allocates: call only while processing is suspended (message thread / prepare).
    if (q == quality) return;
    quality = q;
    tank.prepare (fsf, q == Quality::Eco ? 8 : 16);
}

void Engine::reset()
{
    diffuser.reset();
    bloom.reset();
    tank.reset();
    echo.reset();
    grains.reset();
    ducker.reset();
    for (auto& s : preShift) s.reset();
    for (auto& b : preDelay) b.reset();
    for (auto& b : lookahead) b.reset();
    for (int c = 0; c < 2; ++c)
    {
        inHp[c].reset(); inLp[c].reset(); darkIn[c].reset(); darkOut[c].reset(); tiltLp[c].reset();
        meterLp[c].reset(); meterHp[c].reset();
    }
    bassMonoHp.reset();
    firstChunk = true;
    chunkPos = 0;
    activeMode = -1;
    modeSwitchPending = false;
    clearState = ClearState::Idle;
    clearGain = 1.0f;
    grainFeedL = grainFeedR = 0.0f;
    preDelayX = 1.0f;
    meterCount = 0;
    mIn = mWet = mLow = mMid = mHigh = mLR = mLL = mRR = 0.0;
}

void Engine::doClear (bool delayToo) noexcept
{
    diffuser.clear();
    bloom.clear();
    tank.clear();
    grains.clear();
    for (auto& s : preShift) s.clear();
    for (auto& b : preDelay) b.clearFromNow();
    for (int c = 0; c < 2; ++c) { darkIn[c].reset(); darkOut[c].reset(); }
    if (delayToo) echo.clear();
    grainFeedL = grainFeedR = 0.0f;
    clearCount.fetch_add (1, std::memory_order_relaxed);
}

void Engine::process (float* L, float* R, int numSamples, const Settings& settingsIn, const float* scL, const float* scR) noexcept
{
    // A mode switch changes the whole structure: fade the wet signal out, clear,
    // continue with the new mode (like Clear Tail, but the delay keeps running).
    if (activeMode < 0) activeMode = settingsIn.mode;
    if (settingsIn.mode != activeMode && clearState == ClearState::Idle)
    {
        clearState = ClearState::Fading;
        clearDelayPending = false;
        modeSwitchPending = true;
    }
    Settings s = settingsIn;
    s.mode = activeMode;

    // rising edge of the clear trigger
    if (s.clearTrigger && ! lastClearTrigger && clearState == ClearState::Idle)
    {
        clearState = ClearState::Fading;
        clearDelayPending = s.clearDelayToo;
    }
    lastClearTrigger = s.clearTrigger;

    // Parameters are updated on a fixed grid of kChunk samples (absolute time),
    // independent of the host block size: any block size gives the same output.
    pendingMode = settingsIn.mode;
    int done = 0;
    while (done < numSamples)
    {
        if (chunkPos == 0)
            updateChunkParams (s, kChunk);
        const int n = std::min (kChunk - chunkPos, numSamples - done);
        processChunk (L + done, R + done, n, s,
                      scL != nullptr ? scL + done : nullptr, scR != nullptr ? scR + done : nullptr);
        done += n;
        chunkPos += n;
        if (chunkPos >= kChunk)
        {
            chunkPos = 0;
            tank.endChunk (kChunk);
            limiterGain.store (tank.getLimiterGain(), std::memory_order_relaxed);
        }
    }
}

void Engine::updateChunkParams (const Settings& s, int n) noexcept
{
    const auto& cfg = kModes[std::clamp (s.mode, 0, kNumModes - 1)];
    if (firstChunk)
    {
        for (Smoother* sm : { &sMix, &sOut, &sTilt, &sWidth, &sEarly, &sDelRev, &sDelDir, &sDark, &sPreShim, &sInHp, &sInLp })
            sm->setTime (0.03f, fsf);
        sGate.setTime (0.05f, fsf);
        sDiffMix.setTime (0.15f, fsf);
    }

    // mix: equal-power crossfade
    const float mix = clampf (s.mix, 0.0f, 1.0f);
    const float outG = dbToGain (s.outDb);
    const float tilt = s.tiltDb;
    const float early = cfg.earlyLevel;
    const float gateT = (s.freeze && s.freezeMode == FreezeMode::Freeze) ? 0.0f : 1.0f;
    const float darkT = s.dark ? 1.0f : 0.0f;
    const bool preShimOn = s.shimmerPre && s.shimmer.mode != ShimmerMode::Bypass;
    const float preShimT = preShimOn ? s.shimmer.amount : 0.0f;
    const float diffMixT = smoothstep (s.diffusion / 0.3f);
    const float delDirT = s.delayDirectDb <= -59.9f ? 0.0f : dbToGain (s.delayDirectDb);
    if (firstChunk)
    {
        sMix.snap (mix); sOut.snap (outG); sTilt.snap (tilt); sWidth.snap (s.width); sEarly.snap (early);
        sDelRev.snap (s.delayToReverb); sDelDir.snap (delDirT); sGate.snap (gateT); sDark.snap (darkT);
        sPreShim.snap (preShimT); sDiffMix.snap (diffMixT); sInHp.snap (s.inHpHz); sInLp.snap (s.inLpHz);
    }
    const float m = sMix.next (mix, n);
    const float og = sOut.next (outG, n);
    auto set = [n, this] (Ramp& r, float v) { if (firstChunk) r.snap (v); r.setTarget (v, n); };
    set (rDry, std::cos (m * 0.5f * kPi) * og);
    set (rWet, std::sin (m * 0.5f * kPi) * og);
    const float t = sTilt.next (tilt, n);
    set (rTiltLo, dbToGain (-0.5f * t));
    set (rTiltHi, dbToGain (0.5f * t));
    set (rWidth, sWidth.next (s.width, n));
    set (rEarly, sEarly.next (early, n));
    set (rDelRev, sDelRev.next (s.delayToReverb, n));
    set (rDelDir, sDelDir.next (delDirT, n));
    set (rGate, sGate.next (gateT, n));
    set (rDark, sDark.next (darkT, n));
    set (rPreShim, sPreShim.next (preShimT, n));
    set (rDiffMix, sDiffMix.next (diffMixT, n));

    const float inHpHz = sInHp.next (s.inHpHz, n), inLpHz = sInLp.next (s.inLpHz, n);
    for (int c = 0; c < 2; ++c)
    {
        inHp[c].set (inHpHz, 0.7071f, fsf);
        inLp[c].set (inLpHz, 0.7071f, fsf);
        tiltLp[c].setCutoff (800.0f, fsf);
        darkIn[c].set (10000.0f, fsf);
        darkOut[c].set (10000.0f, fsf);
        meterLp[c].setCutoff (300.0f, fsf);
        meterHp[c].setCutoff (3000.0f, fsf);
    }
    bassMonoHp.set (std::max (20.0f, s.bassMonoHz), 0.7071f, fsf);

    // ---- components
    const float scale = sizeToScale (s.size);
    Diffuser::Params dp;
    dp.numStages = cfg.diffStages;
    dp.rangeMs = cfg.diffRangeMs * scale;
    dp.g = 0.72f * smoothstep ((s.diffusion - 0.3f) / 0.7f);
    dp.phi = cfg.monoIn ? 0.7853982f : 0.7853982f * (1.0f - cfg.stereoSpread);
    dp.modMs = 0.12f + 0.15f * s.modDepth;
    dp.hermite = ! s.dark;
    diffuser.setParams (dp, n);

    bloom.setParams (s.bloomSec, scale, s.freeze, n);

    TankParams tp;
    tp.mode = s.mode;
    tp.sizeScale = scale;
    tp.decaySec = s.decaySec;
    tp.freeze = s.freeze;
    tp.lowMult = s.lowMult;
    tp.highMult = s.highMult;
    tp.lowXHz = s.lowXHz;
    tp.highXHz = s.highXHz;
    tp.lowCutHz = s.lowCutHz;
    tp.highCutHz = s.highCutHz;
    tp.diffusion = s.diffusion;
    tp.modRateHz = s.modRateHz;
    tp.modDepth = s.modDepth;
    tp.drift = s.drift;
    tp.dark = s.dark;
    tp.warmth = s.warmth;
    tp.shimmer = s.shimmer;
    if (s.shimmerPre) tp.shimmer.amount = 0.0f;
    tank.setParams (tp, n);

    echo.setParams (s.echo, n);
    grains.setParams (s.grains, n);

    // pre-shimmer voices: [0] L voice A, [1] L voice B, [2] R voice A, [3] R voice B
    {
        const auto& sh = s.shimmer;
        const bool rev = sh.mode == ShimmerMode::SingleReverse || sh.mode == ShimmerMode::DualReverse;
        float a = sh.semis, b = sh.semis2;
        if (sh.mode == ShimmerMode::Dual || sh.mode == ShimmerMode::DualReverse) { a = std::abs (sh.semis); b = -std::abs (sh.semis); }
        for (int k = 0; k < 4; ++k)
        {
            auto& v = preVoice[k];
            v.ratio = std::pow (2.0f, ((k & 1) ? b : a) / 12.0f);
            v.reverse = rev;
            v.grainSec = lerp (0.035f, 0.11f, sh.smooth);
            v.jitter = 1.0f - sh.smooth;
            preShift[k].setAntiAlias (v.ratio);
        }
    }

    // pre-delay (crossfade on change)
    const float pdTarget = clampf (s.preDelayMs * 0.001f * fsf, 0.0f, kMaxPreDelaySec * fsf - 8.0f);
    if (firstChunk) { preDelayCur = preDelayNext = pdTarget; preDelayX = 1.0f; }
    if (preDelayX >= 1.0f && std::abs (pdTarget - preDelayCur) > 0.5f) { preDelayNext = pdTarget; preDelayX = 0.0f; }

    firstChunk = false;
}

void Engine::processChunk (float* L, float* R, int n, const Settings& s, const float* scL, const float* scR) noexcept
{
    const auto& cfg = kModes[std::clamp (s.mode, 0, kNumModes - 1)];
    const auto& sh = s.shimmer;
    const bool dual = sh.mode == ShimmerMode::Dual || sh.mode == ShimmerMode::DualReverse;
    const bool two = sh.mode == ShimmerMode::TwoVoice;
    const bool grainsActive = grains.isActive();
    const bool bloomActive = bloom.isActive();
    const bool inHpOn = s.inHpHz > 10.5f, inLpOn = s.inLpHz < 19900.0f;
    const bool bassMono = s.bassMonoHz > 20.5f;

    // ---- ducking detector (undelayed input or sidechain)
    const bool useSc = s.duckSource == DuckSource::Sidechain && scL != nullptr;
    for (int i = 0; i < n; ++i)
    {
        detL[i] = useSc ? scL[i] : L[i];
        detR[i] = useSc ? (scR != nullptr ? scR[i] : scL[i]) : R[i];
    }
    DuckParams dpar = s.duck;
    ducker.process (detL, detR, n, dpar, duckGain);
    const int look = lookaheadSamplesFor (s.lookaheadMs, fs);
    const bool duckOn = s.duck.on;
    const bool duckPre = duckOn && s.duckPos == DuckPos::Pre;
    const bool duckPost = duckOn && s.duckPos == DuckPos::Post;
    const bool duckRev = s.duckTarget != DuckTarget::Delay;
    const bool duckDel = s.duckTarget != DuckTarget::Reverb;

    const float xfStep = 1.0f / (0.03f * fsf);
    const float clearStep = 1.0f / (kClearFadeSec * fsf);

    for (int i = 0; i < n; ++i)
    {
        // ---- lookahead (dry and everything else)
        float inL = L[i], inR = R[i];
        lookahead[0].write (inL);
        lookahead[1].write (inR);
        if (look > 0)
        {
            inL = lookahead[0].readInt (look + 1);
            inR = lookahead[1].readInt (look + 1);
        }
        const float dryL = inL, dryR = inR;

        // ---- input filters
        if (inHpOn) { inL = inHp[0].highpass (inL); inR = inHp[1].highpass (inR); }
        if (inLpOn) { inL = inLp[0].lowpass (inL); inR = inLp[1].lowpass (inR); }
        const float dg = duckGain[i];

        // ---- echo delay
        float eInL = inL, eInR = inR;
        if (duckPre && duckDel) { eInL *= dg; eInR *= dg; }
        float eL = 0.0f, eR = 0.0f;
        echo.processSample (eInL, eInR, eL, eR);

        // ---- reverb input
        const float dr = rDelRev.next();
        float rvL = inL + dr * eL, rvR = inR + dr * eR;
        if (duckPre && duckRev) { rvL *= dg; rvR *= dg; }

        // ---- pre-delay
        preDelay[0].write (rvL);
        preDelay[1].write (rvR);
        {
            float pl = preDelay[0].readHermite (preDelayCur + 2.0f), pr = preDelay[1].readHermite (preDelayCur + 2.0f);
            if (preDelayX < 1.0f)
            {
                const float w = smoothstep (preDelayX);
                pl += w * (preDelay[0].readHermite (preDelayNext + 2.0f) - pl);
                pr += w * (preDelay[1].readHermite (preDelayNext + 2.0f) - pr);
                preDelayX += xfStep;
                if (preDelayX >= 1.0f) { preDelayX = 1.0f; preDelayCur = preDelayNext; }
            }
            rvL = pl;
            rvR = pr;
        }

        // ---- pre-shimmer (applied once)
        const float ps = rPreShim.next();
        if (ps > 1.0e-4f || s.shimmerPre)
        {
            preShift[0].push (rvL); preShift[1].push (rvL); preShift[2].push (rvR); preShift[3].push (rvR);
            if (ps > 1.0e-4f)
            {
                float sl, sr;
                if (dual)
                {
                    sl = 0.7071f * (preShift[0].process (preVoice[0]) + preShift[1].process (preVoice[1]));
                    sr = 0.7071f * (preShift[2].process (preVoice[2]) + preShift[3].process (preVoice[3]));
                }
                else if (two)
                {
                    const float a = std::sqrt (1.0f - sh.voiceMix), b = std::sqrt (sh.voiceMix);
                    sl = a * preShift[0].process (preVoice[0]) + b * preShift[1].process (preVoice[1]);
                    sr = a * preShift[2].process (preVoice[2]) + b * preShift[3].process (preVoice[3]);
                }
                else
                {
                    sl = preShift[0].process (preVoice[0]);
                    sr = preShift[2].process (preVoice[2]);
                }
                const float c = std::cos (ps * 0.5f * kPi), sn = std::sin (ps * 0.5f * kPi);
                rvL = c * rvL + sn * sl;
                rvR = c * rvR + sn * sr;
            }
        }

        // ---- Dark: band-limit the tank input
        const float dk = rDark.next();
        if (dk > 1.0e-4f)
        {
            rvL += dk * (darkIn[0].process (rvL) - rvL);
            rvR += dk * (darkIn[1].process (rvR) - rvR);
        }

        // grains fed back into the tank (from the previous sample)
        rvL += grainFeedL;
        rvR += grainFeedR;

        // ---- diffuser
        float x8[8];
        if (cfg.monoIn)
        {
            const float mono = 0.25f * (rvL + rvR);
            for (int c = 0; c < 8; ++c) x8[c] = mono;
        }
        else
        {
            for (int c = 0; c < 4; ++c) { x8[c] = 0.5f * rvL; x8[c + 4] = 0.5f * rvR; }
        }
        float d8[8];
        std::memcpy (d8, x8, sizeof (d8));
        diffuser.processSample (d8);
        const float dm = rDiffMix.next();
        for (int c = 0; c < 8; ++c) d8[c] = x8[c] + dm * (d8[c] - x8[c]);

        // early (diffuser) output: orthogonal taps, own side weighted
        float earlyL = 0.0f, earlyR = 0.0f;
        for (int c = 0; c < 4; ++c)
        {
            earlyL += hadamardSign (1, c) * d8[c] + 0.35f * hadamardSign (2, c) * d8[c + 4];
            earlyR += hadamardSign (1, c) * d8[c + 4] + 0.35f * hadamardSign (2, c) * d8[c];
        }

        // ---- bloom
        if (bloomActive) bloom.processSample (d8);

        // ---- freeze gate / clear fade at the tank input
        const float gate = rGate.next() * clearGain;
        for (auto& v : d8) v *= gate;

        // ---- tank
        float tL, tR;
        tank.processSample (d8, tL, tR);
        const float el = rEarly.next() * 0.6f;
        float wL = (tL + el * earlyL) * kReverbOutGain;
        float wR = (tR + el * earlyR) * kReverbOutGain;

        // ---- grains
        if (grainsActive)
        {
            float gl, gr;
            if (s.grains.sourceTank) grains.processSample (wL, wR, gl, gr);
            else grains.processSample (rvL, rvR, gl, gr);
            if (s.grains.intoTank && ! (s.freeze && s.freezeMode == FreezeMode::Freeze))
            {
                grainFeedL = gl * 0.5f;
                grainFeedR = gr * 0.5f;
            }
            else
            {
                grainFeedL = grainFeedR = 0.0f;
                wL += gl;
                wR += gr;
            }
        }

        // ---- Dark output band-limit
        if (dk > 1.0e-4f)
        {
            wL += dk * (darkOut[0].process (wL) - wL);
            wR += dk * (darkOut[1].process (wR) - wR);
        }

        // ---- tilt
        {
            const float lo = rTiltLo.next(), hi = rTiltHi.next();
            const float ll = tiltLp[0].lowpass (wL), lr = tiltLp[1].lowpass (wR);
            wL = lo * ll + hi * (wL - ll);
            wR = lo * lr + hi * (wR - lr);
        }

        // ---- width / bass mono
        {
            const float mid = 0.5f * (wL + wR);
            float side = 0.5f * (wL - wR) * rWidth.next();
            if (bassMono) side = bassMonoHp.highpass (side);
            wL = mid + side;
            wR = mid - side;
        }

        // ---- echo direct to the output
        const float dd = rDelDir.next();
        float oeL = eL * dd, oeR = eR * dd;

        // ---- duck post
        if (duckPost)
        {
            if (duckRev) { wL *= dg; wR *= dg; }
            if (duckDel) { oeL *= dg; oeR *= dg; }
        }
        float outWL = wL + oeL, outWR = wR + oeR;

        // ---- clear tail fade
        if (clearState == ClearState::Fading)
        {
            clearGain -= clearStep;
            if (clearGain <= 0.0f)
            {
                clearGain = 0.0f;
                doClear (clearDelayPending);
                clearState = ClearState::Idle;
                if (modeSwitchPending)
                {
                    modeSwitchPending = false;
                    activeMode = pendingMode;
                }
            }
        }
        else if (clearGain < 1.0f)
            clearGain = 1.0f;   // buffers are silent: back to full gain at once
        outWL *= clearGain;
        outWR *= clearGain;

        // ---- mix
        const float dry = rDry.next(), wet = rWet.next();
        L[i] = dry * dryL + wet * outWL;
        R[i] = dry * dryR + wet * outWR;

        // ---- meters (wet, bands, correlation)
        {
            mIn += 0.5 * ((double) dryL * dryL + (double) dryR * dryR);
            mWet += 0.5 * ((double) outWL * outWL + (double) outWR * outWR);
            const float lo = 0.5f * (meterLp[0].lowpass (outWL) + meterLp[1].lowpass (outWR));
            const float hi = 0.5f * (meterHp[0].highpass (outWL) + meterHp[1].highpass (outWR));
            const float mid = 0.5f * (outWL + outWR) - lo - hi;
            mLow += (double) lo * lo; mMid += (double) mid * mid; mHigh += (double) hi * hi;
            mLR += (double) outWL * outWR; mLL += (double) outWL * outWL; mRR += (double) outWR * outWR;
            if (++meterCount >= meterLen)
            {
                const double k = 1.0 / (double) meterCount;
                auto db = [] (double e) { return e > 1.0e-14 ? (float) (10.0 * std::log10 (e)) : -140.0f; };
                const double den = std::sqrt (mLL * mRR);
                meterFifo.push ({ db (mIn * k), db (mWet * k), db (mLow * k), db (mMid * k), db (mHigh * k),
                                  ducker.getCurrentReductionDb(), den > 1.0e-14 ? (float) (mLR / den) : 1.0f });
                meterCount = 0;
                mIn = mWet = mLow = mMid = mHigh = mLR = mLL = mRR = 0.0;
            }
        }
    }
}
} // namespace schimmer::dsp
