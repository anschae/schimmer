#pragma once
// Real-time granular engine coupled with the reverb (docs/RESEARCH.md §8).
// A 4 s stereo capture buffer, Poisson-ish grain scheduling, Hann windows,
// pitch (optionally quantised), position spray, pitch jitter, reverse
// probability and stereo scatter. All grains are pre-allocated.

#include "GrainShifter.h"
#include "RingBuffer.h"

namespace schimmer::dsp
{
struct GrainParams
{
    bool on = false;
    bool sourceTank = false;      // false = input, true = tank output
    bool intoTank = false;        // false = parallel to the output, true = into the tank
    float sizeMs = 120.0f;        // 10..500
    float density = 12.0f;        // grains per second, 1..80
    float pitch = 0.0f;           // semitones
    bool quantize = true;
    float sprayMs = 200.0f;       // 0..2000
    float pitchJitter = 0.0f;     // semitones
    float reverseProb = 0.0f;     // 0..1
    float scatter = 0.6f;         // 0..1
    float amount = 0.5f;          // 0..1
};

class Granulator
{
public:
    static constexpr int kMaxGrains = 48;
    static constexpr float kBufferSec = 4.0f;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& b : buf) b.allocate ((int) std::ceil (kBufferSec * fs) + 16);
        reset();
    }
    void reset()
    {
        for (auto& b : buf) b.reset();
        for (auto& g : grains) g.active = false;
        countdown = 0.0f;
        first = true;
    }
    void clear() noexcept
    {
        for (auto& b : buf) b.clearFromNow();
        for (auto& g : grains) g.active = false;
    }

    void setParams (const GrainParams& p, int n) noexcept
    {
        params = p;
        if (first) { sAmt.setTime (0.05f, fs); sAmt.snap (p.on ? p.amount : 0.0f); }
        const float norm = 1.0f / std::sqrt (std::max (1.0f, p.density * p.sizeMs * 0.001f * 1.5f));
        amtRamp.setTarget (sAmt.next (p.on ? p.amount : 0.0f, n) * norm * 1.4f, n);
        first = false;
    }

    bool isActive() const noexcept { return params.on || amtRamp.value > 1.0e-5f; }

    inline void processSample (float inL, float inR, float& outL, float& outR) noexcept
    {
        buf[0].write (inL);
        buf[1].write (inR);
        const float amt = amtRamp.next();
        if (params.on)
        {
            countdown -= 1.0f;
            if (countdown <= 0.0f)
            {
                spawn();
                const float mean = fs / clampf (params.density, 0.5f, 120.0f);
                // mixture of regular and exponential (Poisson) spacing
                const float u = std::max (1.0e-4f, rng.uniform());
                countdown += mean * lerp (1.0f, -std::log (u), 0.75f);
            }
        }
        float l = 0.0f, r = 0.0f;
        const auto& win = HannTable::get();
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float w = win (g.phase);
            const float xl = buf[0].readAbsolute (g.pos), xr = buf[1].readAbsolute (g.pos);
            const float mid = 0.5f * (xl + xr), side = 0.5f * (xl - xr);
            const float s = mid * w;
            l += s * g.gl + side * w * g.keep;
            r += s * g.gr - side * w * g.keep;
            g.pos += g.reverse ? -g.rate : g.rate;
            g.phase += g.inc;
            if (g.phase >= 1.0f) g.active = false;
        }
        outL = l * amt;
        outR = r * amt;
    }

private:
    struct Grain
    {
        bool active = false, reverse = false;
        double pos = 0.0;
        float rate = 1.0f, phase = 0.0f, inc = 0.0f, gl = 0.7f, gr = 0.7f, keep = 0.0f;
    };

    void spawn() noexcept
    {
        Grain* g = nullptr;
        for (auto& c : grains)
            if (! c.active) { g = &c; break; }
        if (g == nullptr) return;
        float semis = params.pitch + params.pitchJitter * rng.bipolar();
        if (params.quantize) semis = std::round (semis);
        const float r = std::pow (2.0f, clampf (semis, -36.0f, 24.0f) / 12.0f);
        const bool rev = rng.uniform() < params.reverseProb;
        float len = clampf (params.sizeMs, 5.0f, 600.0f) * 0.001f * fs;
        const float maxSpan = kBufferSec * fs - 64.0f;
        const float spray = clampf (params.sprayMs, 0.0f, 2000.0f) * 0.001f * fs * rng.uniform();
        const float span = rev ? (1.0f + r) * len : std::max (0.0f, r - 1.0f) * len;
        if (span + spray > maxSpan) len = std::max (64.0f, (maxSpan - spray) / std::max (0.01f, span / len));
        const float d0 = (rev ? 4.0f : std::max (0.0f, r - 1.0f) * len + 4.0f) + spray;
        g->pos = (double) buf[0].position() - (double) d0;
        g->rate = r;
        g->reverse = rev;
        g->inc = 1.0f / len;
        g->phase = 0.0f;
        const float pan = clampf (params.scatter * rng.bipolar(), -1.0f, 1.0f);
        const float ang = (pan + 1.0f) * 0.25f * kPi;
        g->gl = std::cos (ang) * 1.4142f;
        g->gr = std::sin (ang) * 1.4142f;
        g->keep = 1.0f - params.scatter;   // keep some of the source stereo at low scatter
        g->active = true;
    }

    float fs = 48000.0f;
    GrainParams params;
    RingBuffer buf[2];
    Grain grains[kMaxGrains];
    Rng rng { 0x6a1a1u };
    float countdown = 0.0f;
    Smoother sAmt;
    Ramp amtRamp;
    bool first = true;
};
} // namespace schimmer::dsp
