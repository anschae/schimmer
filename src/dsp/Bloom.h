#pragma once
// Bloom network: an 8-line FDN between the input diffuser and the tank. It
// stores incoming energy and releases it slowly into the tank, so the wet
// envelope rises over roughly the Bloom time (works for plucks and pads alike).
// See docs/RESEARCH.md section 12.

#include "Filters.h"
#include "Hadamard.h"
#include "RingBuffer.h"

namespace schimmer::dsp
{
class Bloom
{
public:
    static constexpr int kCh = 8;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        Rng r (0xb100b100u);
        for (int i = 0; i < kCh; ++i)
        {
            const float rel = ((float) ((i * 3) % kCh) + 0.25f + 0.5f * r.uniform()) / (float) kCh;
            baseMs[i] = 55.0f * std::pow (3.0f, rel);   // 55 .. 165 ms
            buf[i].allocate ((int) std::ceil ((baseMs[i] * 1.5f + 4.0f) * 0.001f * fs) + 8);
            flip[i] = (r.next() & 1u) ? -1.0f : 1.0f;
            modPhase[i] = r.uniform() * kTwoPi;
            modRate[i] = 0.11f + 0.3f * r.uniform();
        }
        reset();
    }

    void reset()
    {
        for (auto& b : buf) b.reset();
        for (auto& f : lp) f.reset();
        first = true;
    }

    void clear() noexcept
    {
        for (auto& b : buf) b.clearFromNow();
        for (auto& f : lp) f.reset();
    }

    /** bloomSec: 0 = off; scale = tank size scale. */
    void setParams (float bloomSec, float sizeScale, bool freeze, int n) noexcept
    {
        if (first)
        {
            sMix.setTime (0.1f, fs);
            sT60.setTime (0.1f, fs);
        }
        const float mixT = freeze ? 0.0f : smoothstep (bloomSec / 0.4f);
        const float t60T = std::max (0.05f, bloomSec * 1.6f);
        if (first) { sMix.snap (mixT); sT60.snap (t60T); }
        mixRamp.setTarget (sMix.next (mixT, n), n);
        const float t60 = sT60.next (t60T, n);
        const float sc = std::sqrt (clampf (sizeScale, 0.25f, 2.0f));
        float sumG2 = 0.0f;
        for (int i = 0; i < kCh; ++i)
        {
            modPhase[i] += kTwoPi * modRate[i] * (float) n / fs;
            if (modPhase[i] > kTwoPi) modPhase[i] -= kTwoPi;
            const float lenS = (baseMs[i] * sc + 1.0f + std::sin (modPhase[i])) * 0.001f * fs;
            if (first) len[i].snap (lenS);
            len[i].setTarget (lenS, n);
            const float g = decayGain (lenS / fs, t60);
            if (first) gain[i].snap (g);
            gain[i].setTarget (g, n);
            sumG2 += g * g;
            lp[i].setCutoff (7000.0f, fs);
        }
        // normalise the stored energy (~1 / (1 - g^2)) back to unity
        const float normT = std::sqrt (std::max (0.0f, 1.0f - sumG2 / (float) kCh));
        if (first) normRamp.snap (normT);
        normRamp.setTarget (normT, n);
        first = false;
    }

    inline bool isActive() const noexcept { return mixRamp.value > 1.0e-4f || mixRamp.step > 0.0f; }

    /** In place on 8 channels. */
    inline void processSample (float* x) noexcept
    {
        const float mix = mixRamp.next();
        const float nm = normRamp.next();
        float y[kCh];
        for (int i = 0; i < kCh; ++i)
            y[i] = buf[i].readLinear (len[i].next());
        float v[kCh];
        for (int i = 0; i < kCh; ++i)
            v[(i * 5 + 1) & 7] = flip[i] * lp[i].highShelf (y[i], 0.8f) * gain[i].next();
        hadamardInPlace<kCh> (v);
        for (int i = 0; i < kCh; ++i)
        {
            buf[i].write (v[i] + x[i]);
            x[i] += mix * (y[i] * nm * 1.6f - x[i]);
        }
    }

private:
    float fs = 48000.0f;
    RingBuffer buf[kCh];
    float baseMs[kCh] {}, flip[kCh] {}, modPhase[kCh] {}, modRate[kCh] {};
    OnePole lp[kCh];
    Ramp len[kCh], gain[kCh], mixRamp, normRamp;
    Smoother sMix, sT60;
    bool first = true;
};
} // namespace schimmer::dsp
