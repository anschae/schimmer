#pragma once
// Granular overlap-add pitch shifter for one channel (used in the shimmer path).
//
// Four Hann-windowed grains overlap by 75 %. Each grain reads the input buffer
// at `ratio` times real time (or backwards, for the Reverse modes). The ratio,
// length and start position are fixed per grain, so continuous pitch sweeps
// glide without discontinuities (each new grain picks up the new ratio).
// Jitter randomises grain length and start position: from smooth (no jitter)
// to the noisy, "orchestral" sidebands of a randomised shifter.

#include "Filters.h"
#include "RingBuffer.h"

namespace schimmer::dsp
{
/** Shared Hann window table. */
struct HannTable
{
    static constexpr int kSize = 1024;
    float v[kSize + 2] {};
    HannTable() noexcept
    {
        for (int i = 0; i <= kSize + 1; ++i)
        {
            const float s = std::sin (kPi * (float) std::min (i, kSize) / (float) kSize);
            v[i] = s * s;
        }
    }
    inline float operator() (float phase01) const noexcept
    {
        const float p = phase01 * (float) kSize;
        const int i = (int) p;
        const float f = p - (float) i;
        return v[i] + f * (v[i + 1] - v[i]);
    }
    static const HannTable& get() noexcept
    {
        static const HannTable t;
        return t;
    }
};

struct ShiftVoice
{
    float ratio = 2.0f;       // playback speed (> 0)
    bool reverse = false;
    float grainSec = 0.08f;
    float jitter = 0.0f;      // 0..1
};

class GrainShifter
{
public:
    static constexpr int kGrains = 4;
    static constexpr float kMaxBufferSec = 0.45f;

    void prepare (float sampleRate, uint32_t seed)
    {
        fs = sampleRate;
        buf.allocate ((int) std::ceil (kMaxBufferSec * fs) + 16);
        agcCoef = 1.0f - std::exp (-1.0f / (0.12f * fs));
        agcSmooth = 1.0f - std::exp (-1.0f / (0.05f * fs));
        rng = Rng (seed);
        reset();
    }

    void reset() noexcept
    {
        buf.reset();
        aa.reset();
        for (int k = 0; k < kGrains; ++k)
        {
            grains[k] = {};
            grains[k].phase = (float) k / (float) kGrains;
            grains[k].inc = 0.0f;   // started lazily
        }
        started = false;
        eIn = eOut = 0.0f;
        agc = 1.0f;
    }

    void clear() noexcept
    {
        buf.clearFromNow();
        aa.reset();
    }

    /** Anti-alias filter for upward shifts (set per chunk): 8th-order Butterworth
        well below fs / (2 ratio), so octaves up never fold back (test "does not alias"). */
    void setAntiAlias (float ratio) noexcept
    {
        aaOn = ratio > 1.001f;
        if (aaOn && std::abs (ratio - aaRatio) > 1.0e-4f)
        {
            aa.set (std::min (0.45f * fs, 0.36f * fs / ratio), fs);
            aaRatio = ratio;
        }
    }

    inline void push (float x) noexcept
    {
        const float y = aaOn ? aa.process (x) : x;
        eIn += agcCoef * (y * y - eIn);
        buf.write (y);
    }

    /** Output for the current sample (call after push()). */
    inline float process (const ShiftVoice& v) noexcept
    {
        if (! started)
        {
            started = true;
            for (int k = 0; k < kGrains; ++k)
            {
                startGrain (grains[k], v);
                grains[k].phase = (float) k / (float) kGrains;
                // emulate a grain that has already been running for phase * length samples
                auto& g = grains[k];
                const double n = (double) (g.phase / g.inc);
                g.pos += g.reverse ? -(1.0 + g.rate) * n : (g.rate - 1.0) * n;
            }
        }
        const auto& win = HannTable::get();
        float sum = 0.0f;
        for (auto& g : grains)
        {
            sum += win (g.phase) * buf.readAbsolute (g.pos);
            g.pos += g.reverse ? -g.rate : g.rate;
            g.phase += g.inc;
            if (g.phase >= 1.0f)
                startGrain (g, v);
        }
        // Energy-preserving AGC: overlap-add gain depends on how coherent the
        // grains are (pitch, jitter, material). Matching the slow RMS of output
        // and input keeps the shifter energy-neutral, so the reverb decay time
        // stays right whatever the shimmer settings.
        sum *= norm;
        eOut += agcCoef * (sum * sum - eOut);
        if (eOut > 1.0e-12f && eIn > 1.0e-12f)
        {
            const float target = clampf (std::sqrt (eIn / eOut), 0.35f, 1.8f);
            agc += agcSmooth * (target - agc);
        }
        return sum * agc;
    }

private:
    struct Grain
    {
        double pos = 0.0;
        float rate = 1.0f, phase = 0.0f, inc = 0.0f;
        bool reverse = false;
    };

    void startGrain (Grain& g, const ShiftVoice& v) noexcept
    {
        const float r = clampf (v.ratio, 0.125f, 4.0f);
        const float maxSpan = kMaxBufferSec * fs - 16.0f;
        float len = v.grainSec * fs * (1.0f + v.jitter * 0.45f * rng.bipolar());
        // the buffer must hold the whole grain span
        const float spanPerLen = v.reverse ? (1.0f + r) : std::abs (r - 1.0f);
        const float posJit = v.jitter * 0.5f * rng.uniform();
        len = std::min (len, maxSpan / (spanPerLen + posJit + 0.01f));
        len = std::max (len, 64.0f);
        const float margin = 4.0f + posJit * len;
        float d0;
        if (v.reverse) d0 = margin;
        else if (r > 1.0f) d0 = (r - 1.0f) * len + margin;
        else d0 = margin;
        g.pos = (double) buf.position() - (double) d0;
        g.rate = r;
        g.reverse = v.reverse;
        g.inc = 1.0f / len;
        g.phase = 0.0f;
        // energy normalisation: 4 x Hann overlap -> sum w^2 = 1.5 for incoherent
        // grains, sum w = 2 for coherent ones (ratio ~ 1)
        const float semis = std::abs (12.0f * std::log2 (r));
        norm = v.reverse ? 0.8165f : lerp (0.5f, 0.8165f, clampf (semis, 0.0f, 1.0f));
    }

    float fs = 48000.0f;
    RingBuffer buf;
    Butter8 aa;
    bool aaOn = false;
    float aaRatio = -1.0f;
    Grain grains[kGrains];
    Rng rng;
    float norm = 0.8165f;
    bool started = false;
    float eIn = 0.0f, eOut = 0.0f, agc = 1.0f, agcCoef = 0.001f, agcSmooth = 0.001f;
};
} // namespace schimmer::dsp
