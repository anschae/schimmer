#pragma once
// Test signals for the render tool and the measurement tests (plain C++).

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace schimmer::signals
{
struct Stereo
{
    std::vector<float> l, r;
    void resize (size_t n) { l.assign (n, 0.0f); r.assign (n, 0.0f); }
    size_t size() const { return l.size(); }
};

inline uint32_t xs (uint32_t& s)
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
inline float white (uint32_t& s) { return (float) (xs (s) >> 8) / 8388608.0f - 1.0f; }

/** Single-sample impulse (both channels) at t = 0, then silence. */
inline Stereo impulse (double fs, double seconds, float amp = 1.0f)
{
    Stereo s;
    s.resize ((size_t) (fs * seconds));
    s.l[0] = s.r[0] = amp;
    return s;
}

/** Bursts of white noise (decorrelated L/R) with 2 ms ramps. */
inline Stereo noiseBursts (double fs, double seconds, double burstSec = 0.25, double periodSec = 4.0, float amp = 0.25f)
{
    Stereo s;
    s.resize ((size_t) (fs * seconds));
    uint32_t a = 1234567u, b = 7654321u;
    const size_t burst = (size_t) (fs * burstSec), period = (size_t) (fs * periodSec), ramp = (size_t) (fs * 0.002);
    for (size_t i = 0; i < s.size(); ++i)
    {
        const size_t k = i % period;
        if (k >= burst) continue;
        float env = 1.0f;
        if (k < ramp) env = (float) k / (float) ramp;
        if (burst - k < ramp) env = (float) (burst - k) / (float) ramp;
        s.l[i] = amp * env * white (a);
        s.r[i] = amp * env * white (b);
    }
    return s;
}

/** Sine bursts at a few frequencies (ringing / resonance check). */
inline Stereo sineBursts (double fs, double seconds, float amp = 0.3f)
{
    Stereo s;
    s.resize ((size_t) (fs * seconds));
    const double freqs[] = { 220.0, 880.0, 3520.0 };
    const double slot = seconds / 3.0, burst = 0.4;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double t = (double) i / fs;
        const int k = std::min (2, (int) (t / slot));
        const double tl = t - k * slot;
        if (tl >= burst) continue;
        const double env = std::sin (3.14159265 * tl / burst);
        const float v = (float) (amp * env * env * std::sin (2.0 * 3.14159265 * freqs[k] * t));
        s.l[i] = s.r[i] = v;
    }
    return s;
}

/** Soft synthetic pad chord (detuned saws, low-passed), 4 s on, rest silence. */
inline Stereo padChord (double fs, double seconds, double onSec = 4.0, float amp = 0.07f)
{
    Stereo s;
    s.resize ((size_t) (fs * seconds));
    const double notes[] = { 130.81, 196.00, 246.94, 329.63, 392.00 };   // Cmaj9-ish
    double ph[5][3] = {};
    double lpL = 0, lpR = 0;
    const double c = 1.0 - std::exp (-2.0 * 3.14159265 * 1800.0 / fs);
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double t = (double) i / fs;
        double env = 0.0;
        if (t < onSec) env = std::min (1.0, t / 0.6) * std::min (1.0, (onSec - t) / 0.8);
        double l = 0, r = 0;
        for (int n = 0; n < 5; ++n)
            for (int d = 0; d < 3; ++d)
            {
                const double f = notes[n] * (1.0 + (d - 1) * 0.0035);
                ph[n][d] += f / fs;
                ph[n][d] -= std::floor (ph[n][d]);
                const double saw = 2.0 * ph[n][d] - 1.0;
                if (d == 0) l += saw;
                else if (d == 2) r += saw;
                else { l += 0.5 * saw; r += 0.5 * saw; }
            }
        lpL += c * (l - lpL);
        lpR += c * (r - lpR);
        s.l[i] = (float) (amp * env * lpL);
        s.r[i] = (float) (amp * env * lpR);
    }
    return s;
}

/** Snare-like transients: noise + tone with a fast decay, every 1.5 s for 6 s. */
inline Stereo snare (double fs, double seconds, float amp = 0.5f)
{
    Stereo s;
    s.resize ((size_t) (fs * seconds));
    uint32_t a = 99991u;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double t = (double) i / fs;
        if (t > 6.0) break;
        const double tl = std::fmod (t, 1.5);
        const double env = std::exp (-tl * 28.0), tone = std::exp (-tl * 40.0) * std::sin (2.0 * 3.14159265 * 185.0 * tl);
        const float v = (float) (amp * (0.7 * env * white (a) + 0.6 * tone));
        s.l[i] = s.r[i] = v;
    }
    return s;
}

/** Plucked-string arpeggio (Karplus-Strong), stereo-panned notes. */
inline Stereo guitarArp (double fs, double seconds, float amp = 0.4f)
{
    Stereo s;
    s.resize ((size_t) (fs * seconds));
    const double notes[] = { 164.81, 246.94, 329.63, 392.00, 493.88, 392.00, 329.63, 246.94 };
    uint32_t seed = 4242u;
    const double step = 0.35;
    for (int k = 0; k < 8; ++k)
    {
        const size_t start = (size_t) (fs * step * k);
        const int period = (int) std::lround (fs / notes[k]);
        std::vector<float> line ((size_t) period);
        for (auto& v : line) v = white (seed);
        const float pan = (k % 2 ? 0.35f : -0.35f);
        const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
        size_t idx = 0;
        float prev = 0.0f;
        for (size_t i = start; i < s.size() && i < start + (size_t) (fs * 2.5); ++i)
        {
            const float cur = line[idx];
            const float nxt = 0.4985f * (cur + prev);
            prev = cur;
            line[idx] = nxt;
            idx = (idx + 1) % (size_t) period;
            s.l[i] += amp * gl * cur;
            s.r[i] += amp * gr * cur;
        }
    }
    return s;
}

inline Stereo byName (const std::string& name, double fs, double seconds)
{
    if (name == "impulse") return impulse (fs, seconds);
    if (name == "noise") return noiseBursts (fs, seconds);
    if (name == "sine") return sineBursts (fs, seconds);
    if (name == "pad") return padChord (fs, seconds);
    if (name == "snare") return snare (fs, seconds);
    if (name == "guitar") return guitarArp (fs, seconds);
    return impulse (fs, seconds);
}

inline const char* const kAllSignals[] = { "impulse", "noise", "sine", "pad", "snare", "guitar" };
} // namespace schimmer::signals
