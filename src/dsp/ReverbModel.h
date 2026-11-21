#pragma once
// Analytic model of the reverb for the displays (Tail View, decay curve).
// It uses the same formulas as the Tank (absorption shelves, low/high cut,
// Dark LP) so the curves match what you hear; tests compare it with renders.

#include "Engine.h"

namespace schimmer::dsp::model
{
/** Mean loop length of the tank in seconds (line + in-loop allpass). */
inline float meanLoopSeconds (const Settings& s) noexcept
{
    const auto& cfg = kModes[std::clamp (s.mode, 0, kNumModes - 1)];
    const float scale = sizeToScale (s.size);
    return (cfg.tankMeanMs + cfg.loopApMs) * scale * 0.001f;
}

namespace detail
{
inline double ratio (double f, double fc, double fs)
{
    const double nyq = 0.4999 * fs;
    return std::tan (3.14159265358979 * std::min (f, nyq) / fs) / std::tan (3.14159265358979 * std::min (fc, nyq) / fs);
}
inline double lp2 (double f, double fc, double fs) { const double r = ratio (f, fc, fs); return 1.0 / (1.0 + r * r); }
inline double hp2 (double f, double fc, double fs) { const double r = ratio (f, fc, fs); return r * r / (1.0 + r * r); }
inline double lowShelf2 (double f, double fc, double a, double fs) { const double r = ratio (f, fc, fs); return (r * r + a * a) / (r * r + 1.0); }
inline double highShelf2 (double f, double fc, double a, double fs) { const double r = ratio (f, fc, fs); return (a * a * r * r + 1.0) / (r * r + 1.0); }
} // namespace detail

/** Reverb time (s) at frequency f. Infinite (1e9) when frozen. */
inline double t60At (const Settings& s, double f, double fs = 48000.0)
{
    if (s.freeze) return 1.0e9;
    const double L = meanLoopSeconds (s);
    const double t60 = std::max (0.05f, s.decaySec);
    const double gm = std::pow (10.0, -3.0 * L / t60);
    const double gl = std::pow (10.0, -3.0 * L / (t60 * s.lowMult));
    const double gh = std::pow (10.0, -3.0 * L / (t60 * s.highMult));
    double m2 = gm * gm;
    m2 *= detail::lowShelf2 (f, s.lowXHz, gl / gm, fs);
    m2 *= detail::highShelf2 (f, s.highXHz, gh / gm, fs);
    if (s.lowCutHz > 10.5f) m2 *= detail::hp2 (f, s.lowCutHz, fs);
    if (s.highCutHz < 19900.0f) m2 *= detail::lp2 (f, s.highCutHz, fs);
    if (s.dark) m2 *= detail::lp2 (f, 9000.0, fs);
    m2 *= detail::hp2 (f, 4.0, fs);   // DC blocker
    const double m = std::sqrt (m2);
    if (m >= 1.0) return 1.0e9;
    return std::min (1.0e9, -3.0 * L / std::log10 (m));
}

/** Expected envelope (dB) of the wet tail at time t after an input impulse,
    for the reverb time t60 of one band. Includes pre-delay, the diffusion
    fade-in and Bloom. 0 dB = the peak of the mid band without bloom. */
inline double envelopeDb (const Settings& s, double t, double t60)
{
    const double pre = s.preDelayMs * 0.001;
    const double tt = t - pre;
    if (tt <= 0.0) return -200.0;
    // energy time constants
    const double tauT = std::max (0.02, t60) / 13.8155;
    // fade-in: diffusion stages (g up to 0.72) and the first arrival of the tank
    const auto& cfg = kModes[std::clamp (s.mode, 0, kNumModes - 1)];
    const double scale = sizeToScale (s.size);
    const double g = 0.72 * smoothstep ((s.diffusion - 0.3f) / 0.7f);
    const double tauR = 0.004 + cfg.diffRangeMs * 0.001 * scale * (0.15 + 1.6 * g * g);
    double e;
    if (s.bloomSec > 0.02f && ! s.freeze)
    {
        // tank integrates the bloom network's release: difference of exponentials
        const double tauB = std::max (0.02, s.bloomSec * 1.6) / 13.8155;
        const double a = tauT, b = std::max (tauB + 1.0e-4, tauR);
        e = (std::exp (-tt / a) - std::exp (-tt / b)) / (1.0 - b / a);
        const double tPk = a * b / (a - b) * std::log (a / b);
        const double pk = (std::exp (-tPk / a) - std::exp (-tPk / b)) / (1.0 - b / a);
        e /= std::max (1.0e-9, pk);
        // the bloom network spreads the level over time: lower peak
        e *= std::pow (std::max (0.05, tauR / b), 0.35);
    }
    else
    {
        const double rise = 1.0 - std::exp (-tt / tauR);
        e = rise * std::exp (-tt / tauT);
    }
    return 10.0 * std::log10 (std::max (1.0e-20, e));
}
} // namespace schimmer::dsp::model
