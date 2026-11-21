#pragma once
// Measurement helpers for the sound-quality tests (plain C++, no JUCE).
// Methods: docs/RESEARCH.md section 13.

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <vector>
#include "dsp/Engine.h"
#include "TestSignals.h"

namespace schimmer::analysis
{
using Vec = std::vector<float>;
inline constexpr double kPiD = 3.14159265358979323846;

struct Render
{
    Vec l, r;
    double fs = 48000.0;
    Vec mono() const
    {
        Vec m (l.size());
        for (size_t i = 0; i < l.size(); ++i) m[i] = 0.5f * (l[i] + r[i]);
        return m;
    }
};

/** Runs a signal through a fresh engine (blocks of `block` samples). */
inline Render renderEngine (dsp::Settings s, const signals::Stereo& in, double fs, int block = 512,
                            dsp::Quality q = dsp::Quality::High)
{
    dsp::Engine e;
    e.setQuality (q);
    e.prepare (fs, block);
    Render r;
    r.fs = fs;
    r.l = in.l;
    r.r = in.r;
    for (size_t i = 0; i < r.l.size(); i += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, r.l.size() - i);
        e.process (r.l.data() + i, r.r.data() + i, n, s);
    }
    return r;
}

inline dsp::Settings wetSettings (int mode, float decay)
{
    dsp::Settings s;
    s.mode = mode;
    s.decaySec = decay;
    s.mix = 1.0f;
    s.preDelayMs = 0.0f;
    s.lowMult = 1.0f;
    s.highMult = 1.0f;
    return s;
}

// ---------------------------------------------------------------------------- filters
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    static Biquad bandpass (double fc, double q, double fs)
    {
        const double w = 2.0 * kPiD * fc / fs, al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
        Biquad b;
        b.b0 = al / a0; b.b1 = 0; b.b2 = -al / a0; b.a1 = -2.0 * std::cos (w) / a0; b.a2 = (1.0 - al) / a0;
        return b;
    }
    static Biquad highpass (double fc, double q, double fs)
    {
        const double w = 2.0 * kPiD * fc / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
        Biquad b;
        b.b0 = (1 + c) / 2 / a0; b.b1 = -(1 + c) / a0; b.b2 = (1 + c) / 2 / a0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0;
        return b;
    }
    double tick (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

inline Vec filter (const Vec& x, Biquad b, int passes = 1)
{
    Vec y = x;
    for (int p = 0; p < passes; ++p)
    {
        Biquad f = b;
        for (auto& v : y) v = (float) f.tick (v);
    }
    return y;
}

// ---------------------------------------------------------------------------- decay
/** Reverb time from the Schroeder energy decay curve: fit between -5 and -5-range dB. */
inline double t60 (const Vec& h, double fs, double range = 30.0)
{
    const size_t n = h.size();
    std::vector<double> e (n + 1, 0.0);
    for (size_t i = n; i-- > 0;) e[i] = e[i + 1] + (double) h[i] * h[i];
    if (e[0] <= 0.0) return 0.0;
    long i5 = -1, iEnd = -1;
    for (size_t i = 0; i < n; ++i)
    {
        const double d = 10.0 * std::log10 (std::max (1.0e-30, e[i] / e[0]));
        if (i5 < 0 && d <= -5.0) i5 = (long) i;
        if (iEnd < 0 && d <= -5.0 - range) { iEnd = (long) i; break; }
    }
    if (i5 < 0 || iEnd <= i5) return 0.0;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int c = 0;
    for (long i = i5; i < iEnd; i += 8)
    {
        const double x = (double) i / fs, y = 10.0 * std::log10 (std::max (1.0e-30, e[(size_t) i] / e[0]));
        sx += x; sy += y; sxx += x * x; sxy += x * y; ++c;
    }
    const double slope = (c * sxy - sx * sy) / std::max (1.0e-12, c * sxx - sx * sx);
    return slope < 0.0 ? -60.0 / slope : 0.0;
}

inline double bandT60 (const Vec& h, double fs, double fc, double range = 30.0)
{
    return t60 (filter (h, Biquad::bandpass (fc, 1.4, fs), 2), fs, range);
}

// ---------------------------------------------------------------------------- echo density
/** Normalised echo density profile (Abel & Huang 2006), window `win` s, hop win/2. */
inline std::vector<double> ned (const Vec& h, double fs, double win = 0.02)
{
    const size_t n = (size_t) (win * fs), hop = n / 2;
    std::vector<double> out;
    for (size_t i = 0; i + n <= h.size(); i += hop)
    {
        double m = 0.0;
        for (size_t k = 0; k < n; ++k) m += (double) h[i + k] * h[i + k];
        const double sd = std::sqrt (m / (double) n);
        if (sd < 1.0e-12) { out.push_back (0.0); continue; }
        size_t cnt = 0;
        for (size_t k = 0; k < n; ++k) if (std::abs (h[i + k]) > sd) ++cnt;
        out.push_back ((double) cnt / (double) n / 0.3173);
    }
    return out;
}

inline double firstArrival (const Vec& h, double fs, double relThresh = 1.0e-3)
{
    float pk = 0.0f;
    for (auto v : h) pk = std::max (pk, std::abs (v));
    for (size_t i = 0; i < h.size(); ++i)
        if (std::abs (h[i]) > relThresh * pk) return (double) i / fs;
    return 0.0;
}

/** Time (s, after the first arrival) at which the NED first reaches `level`. */
inline double nedOnset (const Vec& h, double fs, double level = 0.9)
{
    const double t0 = firstArrival (h, fs);
    const auto d = ned (h, fs);
    for (size_t i = 0; i < d.size(); ++i)
        if (d[i] >= level && (double) i * 0.01 >= t0) return (double) i * 0.01 - t0;
    return 1.0e9;
}

// ---------------------------------------------------------------------------- spectra
inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPiD / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/** Welch PSD (Hann, 50 % overlap), returns power per bin (n/2 bins). */
inline std::vector<double> welch (const Vec& x, size_t nfft)
{
    std::vector<double> p (nfft / 2, 0.0);
    std::vector<double> w (nfft);
    for (size_t i = 0; i < nfft; ++i) w[i] = 0.5 - 0.5 * std::cos (2.0 * kPiD * (double) i / (double) (nfft - 1));
    int segs = 0;
    std::vector<std::complex<double>> a (nfft);
    for (size_t s = 0; s + nfft <= x.size(); s += nfft / 2)
    {
        for (size_t i = 0; i < nfft; ++i) a[i] = x[s + i] * w[i];
        fft (a);
        for (size_t i = 0; i < nfft / 2; ++i) p[i] += std::norm (a[i]);
        ++segs;
    }
    for (auto& v : p) v /= std::max (1, segs);
    return p;
}

/** Tail segment with its decay envelope removed (so a long window can be averaged). */
inline Vec flattenedSegment (const Vec& x, double fs, double start, double length)
{
    const size_t a = (size_t) (start * fs), b = std::min (x.size(), a + (size_t) (length * fs));
    Vec seg (x.begin() + (long) a, x.begin() + (long) b);
    // remove the exponential decay: fit log energy per 50 ms block
    const size_t blk = (size_t) (0.05 * fs);
    std::vector<double> t, y;
    for (size_t i = 0; i + blk <= seg.size(); i += blk)
    {
        double e = 0;
        for (size_t k = 0; k < blk; ++k) e += (double) seg[i + k] * seg[i + k];
        t.push_back ((double) (i + blk / 2));
        y.push_back (0.5 * std::log (std::max (1.0e-30, e / (double) blk)));
    }
    if (t.size() < 2) return seg;
    const double mt = std::accumulate (t.begin(), t.end(), 0.0) / t.size(), my = std::accumulate (y.begin(), y.end(), 0.0) / y.size();
    double num = 0, den = 0;
    for (size_t i = 0; i < t.size(); ++i) { num += (t[i] - mt) * (y[i] - my); den += (t[i] - mt) * (t[i] - mt); }
    const double k = num / den, c = my - k * mt;
    for (size_t i = 0; i < seg.size(); ++i) seg[i] = (float) (seg[i] / std::exp (c + k * (double) i));
    return seg;
}

/** Largest excess (dB) of the PSD over its 1/3-octave-smoothed version between f0 and f1. */
inline double maxSpectralExcess (const Vec& seg, double fs, double f0 = 100.0, double f1 = 10000.0, double* atHz = nullptr)
{
    const size_t nfft = 8192;
    const auto p = welch (seg, nfft);
    const double df = fs / (double) nfft;
    std::vector<double> pc (p.size() + 1, 0.0);
    for (size_t i = 0; i < p.size(); ++i) pc[i + 1] = pc[i] + p[i];
    double best = -100.0;
    for (size_t i = 2; i < p.size(); ++i)
    {
        const double f = (double) i * df;
        if (f < f0 || f > f1) continue;
        const size_t lo = std::max<size_t> (1, (size_t) (f / std::pow (2.0, 1.0 / 6.0) / df));
        const size_t hi = std::min (p.size() - 1, (size_t) (f * std::pow (2.0, 1.0 / 6.0) / df));
        const double mean = (pc[hi + 1] - pc[lo]) / (double) (hi + 1 - lo);
        const double ex = 10.0 * std::log10 (std::max (1.0e-30, p[i] / std::max (1.0e-30, mean)));
        if (ex > best) { best = ex; if (atHz) *atHz = f; }
    }
    return best;
}

/** The same measure on Gaussian noise of the same length (statistical reference). */
inline double noiseReferenceExcess (size_t len, double fs)
{
    Vec n (len);
    uint32_t s = 2024u;
    for (size_t i = 0; i < len; i += 2)
    {
        const double u1 = std::max (1.0e-12, (double) (signals::xs (s) >> 8) / 16777216.0);
        const double u2 = (double) (signals::xs (s) >> 8) / 16777216.0;
        const double r = std::sqrt (-2.0 * std::log (u1));
        n[i] = (float) (r * std::cos (2.0 * kPiD * u2));
        if (i + 1 < len) n[i + 1] = (float) (r * std::sin (2.0 * kPiD * u2));
    }
    return maxSpectralExcess (n, fs);
}

// ---------------------------------------------------------------------------- envelope / stereo
/** Peak of the autocorrelation of the (detrended) dB envelope at lags 20..250 ms: periodic flutter. */
inline double flutter (const Vec& x, double fs, double start, double length)
{
    const auto seg = flattenedSegment (x, fs, start, length);
    const size_t hop = (size_t) (0.002 * fs);
    Vec env;
    double e = 0.0;
    const double c = std::exp (-1.0 / (0.004 * fs));
    for (size_t i = 0; i < seg.size(); ++i)
    {
        e = c * e + (1.0 - c) * (double) seg[i] * seg[i];
        if (i % hop == 0) env.push_back ((float) (10.0 * std::log10 (e + 1.0e-20)));
    }
    if (env.size() < 300) return 0.0;
    env.erase (env.begin(), env.begin() + 20);
    const double m = std::accumulate (env.begin(), env.end(), 0.0) / env.size();
    for (auto& v : env) v = (float) (v - m);
    double ac0 = 0;
    for (auto v : env) ac0 += (double) v * v;
    double best = 0.0;
    for (size_t lag = 10; lag <= 125 && lag < env.size(); ++lag)
    {
        double a = 0;
        for (size_t i = 0; i + lag < env.size(); ++i) a += (double) env[i] * env[i + lag];
        best = std::max (best, a / ac0);
    }
    return best;
}

inline double correlation (const Vec& l, const Vec& r, size_t a, size_t b)
{
    double lr = 0, ll = 0, rr = 0;
    for (size_t i = a; i < b && i < l.size(); ++i) { lr += (double) l[i] * r[i]; ll += (double) l[i] * l[i]; rr += (double) r[i] * r[i]; }
    return lr / std::sqrt (ll * rr + 1.0e-30);
}

/** Energy of the mono sum relative to the L+R energy (0 dB = no mono loss). */
inline double monoRatioDb (const Vec& l, const Vec& r, size_t a, size_t b)
{
    double m = 0, s = 0;
    for (size_t i = a; i < b && i < l.size(); ++i) { const double x = (double) l[i] + r[i]; m += x * x; s += (double) l[i] * l[i] + (double) r[i] * r[i]; }
    return 10.0 * std::log10 (m / (s + 1.0e-30) + 1.0e-30);
}

inline double energyDb (const Vec& x, size_t a = 0, size_t b = (size_t) -1)
{
    double e = 0;
    for (size_t i = a; i < std::min (b, x.size()); ++i) e += (double) x[i] * x[i];
    return 10.0 * std::log10 (e + 1.0e-30);
}

inline double peakAbs (const Vec& x, size_t a = 0, size_t b = (size_t) -1)
{
    double p = 0;
    for (size_t i = a; i < std::min (b, x.size()); ++i) p = std::max (p, (double) std::abs (x[i]));
    return p;
}

inline bool allFinite (const Vec& x)
{
    for (auto v : x) if (! std::isfinite (v)) return false;
    return true;
}

/** Click detector: peak of the 8 kHz high-passed signal relative to the signal RMS (dB), over [a, b). */
inline double hfSpikeDb (const Vec& x, double fs, size_t a, size_t b)
{
    const auto hp = filter (x, Biquad::highpass (8000.0, 0.7071, fs), 2);
    double rms = 0;
    size_t n = 0;
    for (size_t i = a; i < std::min (b, x.size()); ++i) { rms += (double) x[i] * x[i]; ++n; }
    rms = std::sqrt (rms / std::max<size_t> (1, n));
    return 20.0 * std::log10 (peakAbs (hp, a, b) / (rms + 1.0e-20) + 1.0e-20);
}

/** Sine input (both channels) for click tests. */
inline signals::Stereo sine (double fs, double seconds, double hz, float amp = 0.3f)
{
    signals::Stereo s;
    s.resize ((size_t) (fs * seconds));
    for (size_t i = 0; i < s.size(); ++i)
        s.l[i] = s.r[i] = amp * (float) std::sin (2.0 * kPiD * hz * (double) i / fs);
    return s;
}
} // namespace schimmer::analysis
