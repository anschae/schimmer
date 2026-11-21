// Developer harness: renders the Engine (no JUCE) to a float WAV.
//   devrender out.wav signal seconds key=value ...
// Keys: mode size decay diff bloom rate depth drift lowcut highcut lowmult highmult dark warmth
//       mix sh shsemis shsemis2 shamt shsmooth shpre fs freeze width predelay echo echofb echotime
//       grains gsize gdens gpitch gamt gsrc gdest quality
#include "../../src/dsp/Engine.h"
#include "../TestSignals.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

using namespace schimmer;

static void writeWav (const char* path, const signals::Stereo& s, int fs)
{
    FILE* f = std::fopen (path, "wb");
    const uint32_t n = (uint32_t) s.size();
    const uint32_t dataBytes = n * 2 * 4;
    auto w32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (3); w16 (2); w32 ((uint32_t) fs); w32 ((uint32_t) fs * 8); w16 (8); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i) { std::fwrite (&s.l[i], 4, 1, f); std::fwrite (&s.r[i], 4, 1, f); }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage: devrender out.wav signal seconds [k=v ...]\n"); return 1; }
    std::map<std::string, std::string> kv;
    for (int i = 4; i < argc; ++i)
    {
        std::string a = argv[i];
        auto p = a.find ('=');
        if (p != std::string::npos) kv[a.substr (0, p)] = a.substr (p + 1);
    }
    auto num = [&] (const char* k, double d) { auto it = kv.find (k); return it == kv.end() ? d : std::atof (it->second.c_str()); };
    const int fs = (int) num ("fs", 48000);
    const double secs = std::atof (argv[3]);

    dsp::Settings s;
    s.mode = (int) num ("mode", 1);
    s.size = (float) num ("size", 0.6);
    s.decaySec = (float) num ("decay", 6);
    s.diffusion = (float) num ("diff", 0.85);
    s.bloomSec = (float) num ("bloom", 0);
    s.modRateHz = (float) num ("rate", 0.4);
    s.modDepth = (float) num ("depth", 0.3);
    s.drift = (float) num ("drift", 0.6);
    s.lowCutHz = (float) num ("lowcut", 10);
    s.highCutHz = (float) num ("highcut", 20000);
    s.lowMult = (float) num ("lowmult", 1.2);
    s.highMult = (float) num ("highmult", 0.55);
    s.dark = num ("dark", 0) > 0.5;
    s.warmth = (float) num ("warmth", 0);
    s.mix = (float) num ("mix", 1.0);
    s.width = (float) num ("width", 1.0);
    s.preDelayMs = (float) num ("predelay", 0);
    s.freeze = num ("freeze", 0) > 0.5;
    s.shimmer.mode = (dsp::ShimmerMode) (int) num ("sh", 0);
    s.shimmer.semis = (float) num ("shsemis", 12);
    s.shimmer.semis2 = (float) num ("shsemis2", 7);
    s.shimmer.amount = (float) num ("shamt", 0.5);
    s.shimmer.smooth = (float) num ("shsmooth", 0.6);
    s.shimmer.lowCutHz = (float) num ("shlc", 250);
    s.shimmer.highCutHz = (float) num ("shhc", 9000);
    s.shimmerPre = num ("shpre", 0) > 0.5;
    s.echo.on = num ("echo", 0) > 0.5;
    s.echo.feedback = (float) num ("echofb", 0.4);
    s.echo.timeMs = (float) num ("echotime", 375);
    s.echo.pingPong = num ("pingpong", 0) > 0.5;
    s.grains.on = num ("grains", 0) > 0.5;
    s.grains.sizeMs = (float) num ("gsize", 120);
    s.grains.density = (float) num ("gdens", 12);
    s.grains.pitch = (float) num ("gpitch", 12);
    s.grains.amount = (float) num ("gamt", 0.5);
    s.grains.sourceTank = num ("gsrc", 0) > 0.5;
    s.grains.intoTank = num ("gdest", 0) > 0.5;

    dsp::Engine e;
    e.setQuality ((dsp::Quality) (int) num ("quality", 1));
    e.prepare (fs, 512);
    auto sig = signals::byName (argv[2], fs, secs);
    const int block = 512;
    for (size_t i = 0; i < sig.size(); i += block)
    {
        const int n = (int) std::min<size_t> (block, sig.size() - i);
        e.process (sig.l.data() + i, sig.r.data() + i, n, s);
    }
    writeWav (argv[1], sig, fs);
    std::printf ("limiter=%.3f\n", e.limiterGain.load());
    return 0;
}
