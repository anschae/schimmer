#pragma once
// Small shared helpers for the DSP code. Everything under src/dsp is plain C++
// (no JUCE), so it can be tested and rendered without the plugin wrapper.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace schimmer::dsp
{
inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 2.0f * kPi;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clampf (float x, float lo, float hi) noexcept { return std::min (hi, std::max (lo, x)); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
inline float smoothstep (float x) noexcept
{
    x = clampf (x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

/** Decay gain for a loop of `delaySec` seconds so that the level falls by 60 dB in t60 seconds. */
inline float decayGain (float delaySec, float t60) noexcept
{
    if (t60 <= 0.0f) return 0.0f;
    return std::pow (10.0f, -3.0f * delaySec / t60);
}

/** Deterministic xorshift32 random generator (audio-thread safe). */
struct Rng
{
    uint32_t state = 0x12345678u;
    explicit Rng (uint32_t seed = 0x12345678u) noexcept : state (seed ? seed : 0x9e3779b9u) {}
    inline uint32_t next() noexcept
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return state = x;
    }
    /** [0, 1) */
    inline float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    /** [-1, 1) */
    inline float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }
};

/** One-pole parameter smoother, updated once per chunk of n samples (the
    coefficient follows n, so the timing is independent of the chunk length). */
struct Smoother
{
    float value = 0.0f, tauSamples = 0.0f, coef = 1.0f;
    int cachedN = -1;
    void setTime (float seconds, float sampleRate) noexcept
    {
        tauSamples = seconds * sampleRate;
        cachedN = -1;
    }
    void snap (float v) noexcept { value = v; }
    inline float next (float target, int n) noexcept
    {
        if (n != cachedN)
        {
            coef = tauSamples <= 0.0f ? 1.0f : 1.0f - std::exp (-(float) n / tauSamples);
            cachedN = n;
        }
        return value += coef * (target - value);
    }
};

/** Linear ramp across one chunk (used for per-sample interpolation of chunk-rate values). */
struct Ramp
{
    float value = 0.0f, step = 0.0f;
    inline void setTarget (float target, int numSamples) noexcept { step = (target - value) / (float) numSamples; }
    inline float next() noexcept { return value += step; }
    void snap (float v) noexcept { value = v; step = 0.0f; }
};

inline float hermite (float ym1, float y0, float y1, float y2, float t) noexcept
{
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + y0;
}

inline float softClip (float x) noexcept
{
    // rational tanh approximation, exact sign, |y| < 1
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline bool isFinite (float x) noexcept { return std::isfinite (x); }

inline int nextPow2 (int n) noexcept
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}
} // namespace schimmer::dsp
