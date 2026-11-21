#pragma once
// Bloom network: three cascaded 8-line feedback networks between the input
// diffuser and the tank. Each stage stores the incoming energy and releases it
// with an exponential decay; in series, the released energy follows a gamma
// curve (t^2 e^(-t/tau)) that rises smoothly and peaks around the Bloom time.
// The tank then integrates it, so the wet signal swells in slowly - for plucks
// as well as pads. See docs/RESEARCH.md section 12.

#include "Filters.h"
#include "Hadamard.h"
#include "RingBuffer.h"

namespace schimmer::dsp
{
class Bloom
{
public:
    static constexpr int kCh = 8;
    static constexpr int kStages = 3;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        Rng r (0xb100b100u);
        for (int s = 0; s < kStages; ++s)
            for (int i = 0; i < kCh; ++i)
            {
                auto& st = stages[(size_t) s];
                const float rel = ((float) ((i * 3 + s) % kCh) + 0.25f + 0.5f * r.uniform()) / (float) kCh;
                st.baseMs[i] = 37.0f * std::pow (3.0f, rel);   // 37 .. 111 ms
                st.buf[i].allocate ((int) std::ceil ((st.baseMs[i] * 1.5f + 4.0f) * 0.001f * fs) + 8);
                st.flip[i] = (r.next() & 1u) ? -1.0f : 1.0f;
                st.modPhase[i] = r.uniform() * kTwoPi;
                st.modRate[i] = 0.11f + 0.3f * r.uniform();
            }
        reset();
    }

    void reset()
    {
        for (auto& st : stages)
        {
            for (auto& b : st.buf) b.reset();
            for (auto& f : st.lp) f.reset();
        }
        first = true;
    }

    void clear() noexcept
    {
        for (auto& st : stages)
        {
            for (auto& b : st.buf) b.clearFromNow();
            for (auto& f : st.lp) f.reset();
        }
    }

    /** bloomSec: 0 = off; scale = tank size scale. */
    void setParams (float bloomSec, float sizeScale, bool freeze, int n) noexcept
    {
        if (first)
        {
            sMix.setTime (0.1f, fs);
            sT60.setTime (0.1f, fs);
        }
        const float mixT = freeze ? 0.0f : smoothstep (bloomSec / 0.3f);
        // per-stage energy time constant tau = T60 / 13.8; the cascade (plus the
        // tank) peaks at roughly 2.2 tau -> tau ~ bloom / 2.2
        const float t60T = std::max (0.05f, bloomSec * 13.8f / 2.2f);
        if (first) { sMix.snap (mixT); sT60.snap (t60T); }
        mixRamp.setTarget (sMix.next (mixT, n), n);
        const float t60 = sT60.next (t60T, n);
        // the shelf losses grow with the number of passes: make up ~1.2 dB per second of bloom
        const float outT = 1.25f * std::pow (10.0f, 0.06f * clampf (bloomSec, 0.0f, 6.0f));
        if (first) outGain.snap (outT);
        outGain.setTarget (outT, n);
        const float sc = std::sqrt (clampf (sizeScale, 0.25f, 2.0f));
        for (auto& st : stages)
        {
            float sumG2 = 0.0f;
            for (int i = 0; i < kCh; ++i)
            {
                st.modPhase[i] += kTwoPi * st.modRate[i] * (float) n / fs;
                if (st.modPhase[i] > kTwoPi) st.modPhase[i] -= kTwoPi;
                const float lenS = (st.baseMs[i] * sc + 1.0f + std::sin (st.modPhase[i])) * 0.001f * fs;
                if (first) st.len[i].snap (lenS);
                st.len[i].setTarget (lenS, n);
                const float g = decayGain (lenS / fs, t60);
                if (first) st.gain[i].snap (g);
                st.gain[i].setTarget (g, n);
                sumG2 += g * g;
                st.lp[i].setCutoff (7000.0f, fs);
            }
            // normalise the stored energy (~1 / (1 - g^2)) back to unity
            const float normT = std::sqrt (std::max (0.0f, 1.0f - sumG2 / (float) kCh));
            if (first) st.norm.snap (normT);
            st.norm.setTarget (normT, n);
        }
        first = false;
    }

    inline bool isActive() const noexcept { return mixRamp.value > 1.0e-4f || mixRamp.step > 0.0f; }

    /** In place on 8 channels. */
    inline void processSample (float* x) noexcept
    {
        const float mix = mixRamp.next();
        const float og = outGain.next();
        float sig[kCh];
        for (int i = 0; i < kCh; ++i) sig[i] = x[i];
        for (auto& st : stages)
        {
            const float nm = st.norm.next();
            float y[kCh], v[kCh];
            for (int i = 0; i < kCh; ++i)
                y[i] = st.buf[i].readLinear (st.len[i].next());
            for (int i = 0; i < kCh; ++i)
                v[(i * 5 + 1) & 7] = st.flip[i] * st.lp[i].highShelf (y[i], 0.85f) * st.gain[i].next();
            hadamardInPlace<kCh> (v);
            for (int i = 0; i < kCh; ++i)
            {
                st.buf[i].write (v[i] + sig[i]);
                sig[i] = y[i] * nm;
            }
        }
        for (int i = 0; i < kCh; ++i)
            x[i] += mix * (sig[i] * og - x[i]);
    }

private:
    struct Stage
    {
        RingBuffer buf[kCh];
        float baseMs[kCh] {}, flip[kCh] {}, modPhase[kCh] {}, modRate[kCh] {};
        OnePole lp[kCh];
        Ramp len[kCh], gain[kCh], norm;
    };
    float fs = 48000.0f;
    std::array<Stage, kStages> stages;
    Ramp mixRamp, outGain;
    Smoother sMix, sT60;
    bool first = true;
};
} // namespace schimmer::dsp
