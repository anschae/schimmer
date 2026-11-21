#pragma once
// Topology-preserving-transform (TPT) filters: one-pole LP/HP/shelves and a
// state-variable filter. Coefficients may change every sample without clicks.

#include "Common.h"

namespace schimmer::dsp
{
/** g = tan(pi fc / fs), clamped below Nyquist. */
inline float tptG (float fc, float fs) noexcept
{
    return std::tan (kPi * clampf (fc, 1.0f, 0.49f * fs) / fs);
}

struct OnePole
{
    float s = 0.0f, G = 0.5f;
    void setCutoff (float fc, float fs) noexcept
    {
        const float g = tptG (fc, fs);
        G = g / (1.0f + g);
    }
    void setG (float g) noexcept { G = g / (1.0f + g); }
    void reset() noexcept { s = 0.0f; }
    inline float lowpass (float x) noexcept
    {
        const float v = (x - s) * G;
        const float lp = v + s;
        s = lp + v;
        return lp;
    }
    inline float highpass (float x) noexcept { return x - lowpass (x); }
    /** DC gain A, HF gain 1:  H = (s + A wc) / (s + wc). */
    inline float lowShelf (float x, float A) noexcept { return x + (A - 1.0f) * lowpass (x); }
    /** DC gain 1, HF gain A:  H = (A s + wc) / (s + wc). */
    inline float highShelf (float x, float A) noexcept
    {
        const float lp = lowpass (x);
        return lp + A * (x - lp);
    }
};

/** TPT state-variable filter (Zavalishin). */
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.41421356f;

    void set (float fc, float q, float fs) noexcept
    {
        const float g = tptG (fc, fs);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    struct Out { float lp, bp, hp; };
    inline Out tick (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }
    inline float lowpass (float x) noexcept { return tick (x).lp; }
    inline float highpass (float x) noexcept { return tick (x).hp; }
    inline float bandpass (float x) noexcept { return tick (x).bp; }
};

/** 8th-order Butterworth low-pass as four SVF sections (for the Dark color). */
struct Butter8
{
    Svf s[4];
    void set (float fc, float fs) noexcept
    {
        static constexpr float q[4] = { 0.50979558f, 0.60134489f, 0.89997622f, 2.56291545f };
        for (int i = 0; i < 4; ++i)
            s[i].set (fc, q[i], fs);
    }
    void reset() noexcept
    {
        for (auto& f : s) f.reset();
    }
    inline float process (float x) noexcept
    {
        for (auto& f : s) x = f.lowpass (x);
        return x;
    }
};

/** First-order DC blocker. */
struct DcBlock
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
    void set (float fc, float fs) noexcept { r = std::exp (-kTwoPi * fc / fs); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    inline float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};
} // namespace schimmer::dsp
