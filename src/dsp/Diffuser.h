#pragma once
// Multichannel input diffuser: a chain of 8-channel "matrix allpass" stages.
//
// Each stage:  t = U * D(w)      (delays, then shuffle/flip + orthogonal mix)
//              w = x + g * t      (written into the delays)
//              y = t - g * w
// U orthogonal => the stage is lossless (allpass) for any |g| < 1. g = 0 gives
// the plain Signalsmith "delay -> shuffle -> Hadamard" step; larger g adds
// recirculation inside the stage (slow fade-in, long diffusion tail).
//
// Stereo: channels 0..3 carry L, 4..7 carry R. Each stage mixes within a half
// with a 4-point Hadamard and rotates between the halves by an angle phi
// (phi = pi/4 is a full 8-point Hadamard, i.e. L and R fully mixed).

#include "Filters.h"
#include "Hadamard.h"
#include "RingBuffer.h"

namespace schimmer::dsp
{
class Diffuser
{
public:
    static constexpr int kCh = 8;
    static constexpr int kMaxStages = 6;

    void prepare (float sampleRate, float maxRangeMs)
    {
        fs = sampleRate;
        const int maxLen = (int) std::ceil ((maxRangeMs + 2.0f) * 0.001f * fs) + 8;
        for (auto& st : stages)
            for (auto& b : st.buf)
                b.allocate (maxLen);
        Rng rng (0xd1ff05e5u);
        for (int s = 0; s < kMaxStages; ++s)
        {
            auto& st = stages[(size_t) s];
            // random delay within equal sub-ranges, channel order shuffled
            int order[kCh];
            for (int c = 0; c < kCh; ++c) order[c] = c;
            for (int c = kCh - 1; c > 0; --c) std::swap (order[c], order[rng.next() % (uint32_t) (c + 1)]);
            for (int c = 0; c < kCh; ++c)
                st.rel[c] = ((float) order[c] + 0.15f + 0.7f * rng.uniform()) / (float) kCh;
            for (int c = 0; c < kCh; ++c)
            {
                st.perm[c] = c;
                st.flip[c] = (rng.next() & 1u) ? -1.0f : 1.0f;
                st.modRate[c] = 0.17f + 0.6f * rng.uniform();
                st.modPhase[c] = rng.uniform() * kTwoPi;
            }
            for (int c = kCh - 1; c > 0; --c) std::swap (st.perm[c], st.perm[rng.next() % (uint32_t) (c + 1)]);
        }
        reset();
    }

    void reset()
    {
        for (auto& st : stages)
        {
            for (auto& b : st.buf) b.reset();
            for (int c = 0; c < kCh; ++c) st.delay[c].snap (-1.0f);
        }
        first = true;
    }

    void clear() noexcept
    {
        for (auto& st : stages)
            for (auto& b : st.buf) b.clearFromNow();
    }

    struct Params
    {
        int numStages = 5;
        float rangeMs = 100.0f;   // range of the last (longest) stage; earlier stages halve
        float g = 0.5f;           // allpass coefficient
        float phi = 0.785398f;    // L/R mixing angle (pi/4 = full mix)
        float modMs = 0.12f;      // small decorrelating modulation
        bool hermite = true;
    };

    /** Call once per chunk before process(); n = chunk length. */
    void setParams (const Params& p, int n) noexcept
    {
        params = p;
        cphi = std::cos (p.phi);
        sphi = std::sin (p.phi);
        const float dtChunk = (float) n / fs;
        for (int s = 0; s < p.numStages; ++s)
        {
            auto& st = stages[(size_t) s];
            const float range = p.rangeMs * std::pow (0.5f, (float) (p.numStages - 1 - s)) * 0.001f * fs;
            const float modS = p.modMs * 0.001f * fs * std::min (1.0f, range / (0.004f * fs));
            for (int c = 0; c < kCh; ++c)
            {
                st.modPhase[c] += kTwoPi * st.modRate[c] * dtChunk;
                if (st.modPhase[c] > kTwoPi) st.modPhase[c] -= kTwoPi;
                const float target = std::max (3.0f, st.rel[c] * range + modS * (1.0f + std::sin (st.modPhase[c])));
                if (first) st.delay[c].snap (target);
                st.delay[c].setTarget (target, n);
            }
        }
        first = false;
    }

    /** In-place on 8 channels for one sample. */
    inline void processSample (float* x) noexcept
    {
        float t[kCh];
        for (int s = 0; s < params.numStages; ++s)
        {
            auto& st = stages[(size_t) s];
            for (int c = 0; c < kCh; ++c)
            {
                const float d = st.delay[c].next();
                t[st.perm[c]] = st.flip[c] * st.buf[c].read (d, params.hermite);
            }
            mix (t);
            for (int c = 0; c < kCh; ++c)
            {
                const float w = x[c] + params.g * t[c];
                st.buf[c].write (w);
                x[c] = t[c] - params.g * w;
            }
        }
    }

private:
    inline void mix (float* t) const noexcept
    {
        hadamardInPlace<4> (t);
        hadamardInPlace<4> (t + 4);
        for (int c = 0; c < 4; ++c)
        {
            const float a = t[c], b = t[c + 4];
            t[c] = cphi * a + sphi * b;
            t[c + 4] = sphi * a - cphi * b;
        }
    }

    struct Stage
    {
        RingBuffer buf[kCh];
        float rel[kCh] {};
        int perm[kCh] {};
        float flip[kCh] {};
        float modRate[kCh] {}, modPhase[kCh] {};
        Ramp delay[kCh];
    };

    float fs = 48000.0f;
    Params params;
    float cphi = 0.7071f, sphi = 0.7071f;
    std::array<Stage, kMaxStages> stages;
    bool first = true;
};
} // namespace schimmer::dsp
