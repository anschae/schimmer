// Sound-quality measurements on the DSP engine (docs/RESEARCH.md §13).
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Analysis.h"

using namespace schimmer;
using namespace schimmer::analysis;

namespace
{
constexpr double kFs = 48000.0;

Render impulseResponse (const dsp::Settings& s, double seconds, double fs = kFs, dsp::Quality q = dsp::Quality::High)
{
    return renderEngine (s, signals::impulse (fs, seconds), fs, 512, q);
}
} // namespace

TEST_CASE ("RT60 follows the Decay control", "[dsp][decay]")
{
    const int mode = GENERATE (0, 1, 2, 3);
    const float decay = GENERATE (1.5f, 4.0f, 12.0f);
    auto s = wetSettings (mode, decay);
    const auto r = impulseResponse (s, decay * 1.1 + 2.0);
    const auto m = r.mono();
    // mid band (1 kHz) with flat low/high multipliers; T20 (the onset/early part is excluded by -5 dB)
    const double t = bandT60 (m, kFs, 1000.0, 20.0);
    INFO ("mode " << mode << " decay " << decay << " measured " << t);
    CHECK (t > decay * 0.82);
    CHECK (t < decay * 1.15);
}

TEST_CASE ("Low/High decay multipliers shape the decay per band", "[dsp][decay]")
{
    auto s = wetSettings (2, 4.0f);
    s.lowMult = 2.0f;
    s.highMult = 0.4f;
    s.lowXHz = 300.0f;
    s.highXHz = 3000.0f;
    const auto m = impulseResponse (s, 12.0).mono();
    const double lo = bandT60 (m, kFs, 100.0, 20.0), mid = bandT60 (m, kFs, 1000.0, 20.0), hi = bandT60 (m, kFs, 8000.0, 20.0);
    INFO ("low " << lo << " mid " << mid << " high " << hi);
    CHECK (lo > mid * 1.5);
    CHECK (hi < mid * 0.6);
}

TEST_CASE ("Echo density builds up quickly at high diffusion", "[dsp][density]")
{
    const int mode = GENERATE (0, 1, 2, 3);
    auto s = wetSettings (mode, 6.0f);
    s.diffusion = 0.85f;
    const auto m = impulseResponse (s, 3.0).mono();
    const double onset = nedOnset (m, kFs, 0.9);
    const auto d = ned (m, kFs);
    double late = 0.0;
    int cnt = 0;
    for (size_t i = 30; i < 100 && i < d.size(); ++i) { late += d[i]; ++cnt; }
    late /= std::max (1, cnt);
    INFO ("mode " << mode << " NED >= 0.9 after " << onset * 1000.0 << " ms, mean NED 0.3-1 s " << late);
    CHECK (onset < 0.15);
    CHECK (late > 0.92);
}

TEST_CASE ("No narrow-band resonances or metallic ringing in the tail", "[dsp][resonance]")
{
    const int mode = GENERATE (0, 1, 2, 3);
    const int color = GENERATE (0, 1);
    auto s = wetSettings (mode, 8.0f);
    s.dark = color == 1;
    const auto m = impulseResponse (s, 8.0).mono();
    const auto seg = flattenedSegment (m, kFs, 1.0, 4.0);
    double at = 0.0;
    // (Dark rolls off steeply above 5 kHz: the smoothing reference is biased there)
    const double ex = maxSpectralExcess (seg, kFs, 100.0, s.dark ? 5000.0 : 12000.0, &at);
    const double ref = noiseReferenceExcess (seg.size(), kFs);
    INFO ("mode " << mode << (s.dark ? " dark" : " bright") << ": max excess " << ex << " dB at " << at << " Hz, Gaussian noise reference " << ref << " dB");
    // Chamber is small: its lower mode density is allowed slightly more ripple (like ValhallaShimmer's Small mode)
    CHECK (ex < ref + (mode == 3 ? 4.0 : 3.0));
}

TEST_CASE ("Smooth decay without flutter", "[dsp][flutter]")
{
    const int mode = GENERATE (0, 1, 2, 3);
    auto s = wetSettings (mode, 6.0f);
    const auto m = impulseResponse (s, 6.0).mono();
    const double f = flutter (m, kFs, 0.8, 3.0);
    INFO ("mode " << mode << " envelope autocorrelation peak " << f);
    CHECK (f < 0.35);
}

TEST_CASE ("Wide, decorrelated stereo that survives mono", "[dsp][stereo]")
{
    const int mode = GENERATE (0, 1, 2, 3);
    auto s = wetSettings (mode, 5.0f);
    const auto r = impulseResponse (s, 5.0);
    const size_t a = (size_t) (0.5 * kFs), b = (size_t) (3.0 * kFs);
    const double rho = correlation (r.l, r.r, a, b);
    const double mono = monoRatioDb (r.l, r.r, a, b);
    INFO ("mode " << mode << " correlation " << rho << " mono ratio " << mono << " dB");
    CHECK (std::abs (rho) < 0.2);
    CHECK (std::abs (mono) < 1.5);
}

TEST_CASE ("Same sound at 44.1, 48 and 96 kHz", "[dsp][samplerate]")
{
    const int mode = GENERATE (0, 2);
    auto s = wetSettings (mode, 5.0f);
    double t[3], e[3];
    const double rates[3] = { 44100.0, 48000.0, 96000.0 };
    for (int k = 0; k < 3; ++k)
    {
        auto r = impulseResponse (s, 7.0, rates[k]);
        auto m = r.mono();
        t[k] = bandT60 (m, rates[k], 1000.0, 20.0);
        // a delay network's echoes keep their amplitude at any rate: the impulse energy stays the same
        e[k] = energyDb (m);
    }
    INFO ("T60 " << t[0] << " / " << t[1] << " / " << t[2] << "  energy " << e[0] << " / " << e[1] << " / " << e[2]);
    for (int k : { 0, 2 })
    {
        CHECK (std::abs (t[k] / t[1] - 1.0) < 0.1);
        CHECK (std::abs (e[k] - e[1]) < 1.5);
    }
}

TEST_CASE ("Level stays consistent across modes and sizes", "[dsp][level]")
{
    std::vector<double> levels;
    for (int mode = 0; mode < 4; ++mode)
        for (float size : { 0.2f, 0.9f })
        {
            auto s = wetSettings (mode, 4.0f);
            s.size = size;
            levels.push_back (energyDb (impulseResponse (s, 7.0).mono()));
        }
    const auto [mn, mx] = std::minmax_element (levels.begin(), levels.end());
    INFO ("energy range " << *mn << " .. " << *mx << " dB");
    CHECK (*mx - *mn < 5.0);
}

TEST_CASE ("Clear Tail fades out quickly, clears everything and is ready again", "[dsp][clear]")
{
    auto s = wetSettings (1, 20.0f);
    s.echo.on = true;
    s.echo.feedback = 0.8f;
    dsp::Engine e;
    e.prepare (kFs, 256);
    auto noise = signals::noiseBursts (kFs, 4.0, 1.0, 10.0);
    Render r { noise.l, noise.r, kFs };
    const size_t trig = (size_t) (2.0 * kFs);
    for (size_t i = 0; i < r.l.size(); i += 256)
    {
        s.clearTrigger = i >= trig;
        e.process (r.l.data() + i, r.r.data() + i, 256, s);
    }
    const auto m = r.mono();
    CHECK (e.clearCount.load() == 1);
    // before: loud tail
    CHECK (energyDb (m, trig - 4800, trig) > -40.0);
    // after 10 ms: silent
    CHECK (peakAbs (m, trig + 256 + 480, r.l.size()) < 1.0e-6);
    // the fade itself is click-free: no HF peak above the tail's own HF content
    const auto hp = filter (m, Biquad::highpass (8000.0, 0.7071, kFs), 2);
    CHECK (peakAbs (hp, trig, trig + 1500) < peakAbs (hp, trig - 9600, trig) * 1.2);

    // ready again: a new impulse produces the normal response
    s.clearTrigger = true;   // still held: no new clear
    std::vector<float> l ((size_t) kFs, 0.0f), rr ((size_t) kFs, 0.0f);
    l[0] = rr[0] = 1.0f;
    for (size_t i = 0; i < l.size(); i += 256) e.process (l.data() + i, rr.data() + i, (int) std::min<size_t> (256, l.size() - i), s);
    CHECK (e.clearCount.load() == 1);
    CHECK (energyDb (l) > -30.0);
}

TEST_CASE ("Parameter sweeps are click-free", "[dsp][clicks]")
{
    // A low sine through the reverb is narrow-band; a click would show up as a broadband HF spike.
    const int which = GENERATE (0, 1, 2, 3, 4, 5, 6);
    auto s = wetSettings (2, 3.0f);
    s.mix = 0.5f;
    s.highCutHz = 6000.0f;
    // (the shimmer cascade is kept below the 8 kHz detector band by its own high cut)
    if (which == 4) { s.shimmer.mode = dsp::ShimmerMode::Single; s.shimmer.amount = 0.5f; s.shimmer.semis = -12.0f; s.shimmer.highCutHz = 2500.0f; }
    dsp::Engine e;
    e.prepare (kFs, 128);
    auto in = sine (kFs, 8.0, 220.0, 0.25f);
    Render r { in.l, in.r, kFs };
    const size_t sweepStart = (size_t) (3.0 * kFs), sweepEnd = (size_t) (6.0 * kFs);
    for (size_t i = 0; i < r.l.size(); i += 128)
    {
        const float t = i < sweepStart ? 0.0f : i > sweepEnd ? 1.0f : (float) (i - sweepStart) / (float) (sweepEnd - sweepStart);
        switch (which)
        {
            case 0: s.size = 0.2f + 0.7f * t; break;
            case 1: s.decaySec = 3.0f + 20.0f * t; break;
            case 2: s.diffusion = 0.9f - 0.9f * t; break;
            case 3: s.preDelayMs = 400.0f * t; break;
            case 4: s.shimmer.semis = -12.0f + 24.0f * t; break;
            case 5: s.modDepth = t; s.modRateHz = 0.2f + 3.0f * t; break;
            case 6: s.freeze = t > 0.5f; break;
            default: break;
        }
        e.process (r.l.data() + i, r.r.data() + i, 128, s);
    }
    const auto m = r.mono();
    const double before = hfSpikeDb (m, kFs, (size_t) (1.5 * kFs), sweepStart);
    const double during = hfSpikeDb (m, kFs, sweepStart, sweepEnd + (size_t) (0.5 * kFs));
    INFO ("sweep " << which << ": HF spike before " << before << " dB, during " << during << " dB");
    CHECK (during < std::max (before + 6.0, -30.0));
    CHECK (allFinite (m));
}

TEST_CASE ("Shimmer cascades upwards and stays bounded", "[dsp][shimmer]")
{
    const int mode = GENERATE (1, 2, 3, 4, 5);
    auto s = wetSettings (1, 20.0f);
    s.shimmer.mode = (dsp::ShimmerMode) mode;
    s.shimmer.amount = 1.0f;
    s.shimmer.semis = 12.0f;
    s.shimmer.highCutHz = 16000.0f;
    auto in = signals::padChord (kFs, 30.0, 4.0);
    const auto r = renderEngine (s, in, kFs);
    const auto m = r.mono();
    INFO ("shimmer mode " << mode << " peak " << peakAbs (m));
    CHECK (allFinite (m));
    CHECK (peakAbs (m) < 2.0);
    // energy above 2 kHz grows relative to the input (octaves are added)
    const auto hiIn = filter (in.l, Biquad::highpass (2000.0, 0.7, kFs), 2);
    const auto hiOut = filter (m, Biquad::highpass (2000.0, 0.7, kFs), 2);
    CHECK (energyDb (hiOut, (size_t) (4.0 * kFs), (size_t) (10.0 * kFs)) - energyDb (m, (size_t) (4.0 * kFs), (size_t) (10.0 * kFs))
           > energyDb (hiIn) - energyDb (in.l) + 3.0);
}

TEST_CASE ("Delay feedback up to 98 % stays bounded", "[dsp][delay]")
{
    auto s = wetSettings (1, 2.0f);
    s.echo.on = true;
    s.echo.feedback = 0.98f;
    s.echo.timeMs = 120.0f;
    s.echo.hpHz = 20.0f;
    s.echo.lpHz = 20000.0f;
    s.delayDirectDb = 0.0f;
    const auto r = renderEngine (s, signals::noiseBursts (kFs, 30.0, 2.0, 40.0, 0.9f), kFs);
    CHECK (allFinite (r.l));
    CHECK (peakAbs (r.l) < 6.0);   // loud noise in, reverb + limited echo out: bounded
}

namespace
{
void stabilityRun (const dsp::Settings& s, double seconds)
{
    dsp::Engine e;
    e.prepare (kFs, 512);
    std::vector<float> l (512), r (512);
    uint32_t seed = 77u;
    double maxPeak = 0.0, dcSum = 0.0;
    size_t dcCount = 0;
    const size_t blocks = (size_t) (seconds * kFs / 512);
    for (size_t b = 0; b < blocks; ++b)
    {
        const bool burst = b < 200 || (b % 4000) < 60;   // loud noise at the start and now and then
        for (int i = 0; i < 512; ++i)
        {
            l[(size_t) i] = burst ? 0.7f * signals::white (seed) : 0.0f;
            r[(size_t) i] = burst ? 0.7f * signals::white (seed) : 0.0f;
        }
        e.process (l.data(), r.data(), 512, s);
        for (int i = 0; i < 512; ++i)
        {
            if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i]))
                FAIL ("non-finite output after " << (double) b * 512.0 / kFs << " s");
            maxPeak = std::max (maxPeak, (double) std::abs (l[(size_t) i]));
            dcSum += l[(size_t) i];
            ++dcCount;
        }
    }
    INFO ("peak " << maxPeak << " mean " << dcSum / (double) dcCount);
    // bounded (no runaway); extreme settings include +6 dB delay direct and 98 % feedback
    CHECK (maxPeak < 8.0);
    CHECK (std::abs (dcSum / (double) dcCount) < 1.0e-3);
}

dsp::Settings extremeSettings (int variant)
{
    auto s = wetSettings (variant % 4, 120.0f);
    s.size = 1.0f;
    s.diffusion = 1.0f;
    s.modDepth = 1.0f;
    s.modRateHz = 5.0f;
    s.lowMult = 4.0f;
    s.highMult = 2.0f;
    s.warmth = variant % 2 ? 1.0f : 0.0f;
    s.bloomSec = 5.0f;
    s.shimmer.mode = (dsp::ShimmerMode) (1 + variant % 5);
    s.shimmer.amount = 1.0f;
    s.shimmer.semis = variant % 2 ? 24.0f : -24.0f;
    s.shimmer.smooth = 0.0f;
    s.shimmer.highCutHz = 20000.0f;
    s.shimmer.lowCutHz = 20.0f;
    s.grains.on = true;
    s.grains.intoTank = true;
    s.grains.sourceTank = true;
    s.grains.amount = 1.0f;
    s.grains.density = 80.0f;
    s.echo.on = true;
    s.echo.feedback = 0.98f;
    s.delayDirectDb = 6.0f;
    return s;
}
} // namespace

TEST_CASE ("Stable at extreme settings (1 min)", "[dsp][stability]")
{
    const int variant = GENERATE (0, 1, 2, 3);
    stabilityRun (extremeSettings (variant), 60.0);
}

TEST_CASE ("Freeze holds the tail and stays stable (1 min)", "[dsp][stability][freeze]")
{
    const int mode = GENERATE (0, 3);
    auto s = wetSettings (mode, 10.0f);
    s.freeze = true;
    s.freezeMode = dsp::FreezeMode::FreezeAdd;
    stabilityRun (s, 60.0);
}

TEST_CASE ("Freeze holds the level", "[dsp][freeze]")
{
    auto s = wetSettings (1, 6.0f);
    dsp::Engine e;
    e.prepare (kFs, 512);
    auto in = signals::noiseBursts (kFs, 40.0, 1.0, 100.0, 0.3f);
    Render r { in.l, in.r, kFs };
    for (size_t i = 0; i < r.l.size(); i += 512)
    {
        s.freeze = i > (size_t) (1.2 * kFs);
        e.process (r.l.data() + i, r.r.data() + i, (int) std::min<size_t> (512, r.l.size() - i), s);
    }
    const auto m = r.mono();
    const double early = energyDb (m, (size_t) (3 * kFs), (size_t) (5 * kFs));
    const double late = energyDb (m, (size_t) (36 * kFs), (size_t) (38 * kFs));
    INFO ("frozen level at 4 s " << early << " dB, at 37 s " << late << " dB");
    CHECK (std::abs (late - early) < 3.0);
}

TEST_CASE ("10 minutes of freeze and extreme settings", "[.long]")
{
    SECTION ("freeze + add") {
        auto s = wetSettings (0, 30.0f);
        s.freeze = true;
        s.freezeMode = dsp::FreezeMode::FreezeAdd;
        stabilityRun (s, 600.0);
    }
    SECTION ("extreme shimmer") { stabilityRun (extremeSettings (1), 600.0); }
}
