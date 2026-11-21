// Offline render tool: sends test signals through the plugin and writes WAV
// files and spectrogram PNGs, so a person can listen and Claude can look.
//
//   SchimmerRender <out-folder> [--preset NAME] [--signals impulse,pad,...] [--parity] [--fs 48000] [--quick]
//
// Without --preset every factory preset is rendered. --parity renders the
// ValhallaShimmer parity demos (docs/PARITY.md). An INDEX.md with levels and
// measured RT60 is written next to the files.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <iostream>

#include "PluginProcessor.h"
#include "model/FactoryPresets.h"
#include "TestSignals.h"

using namespace schimmer;

namespace
{
struct Job
{
    juce::String name;
    std::array<float, kNumParams> values;
    juce::String signal;
    double seconds;
};

juce::Colour magma (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    const juce::Colour stops[] = { juce::Colour (0xff000004), juce::Colour (0xff2c115f), juce::Colour (0xff721f81),
                                   juce::Colour (0xffb73779), juce::Colour (0xfff1605d), juce::Colour (0xfffeb078),
                                   juce::Colour (0xfffcfdbf) };
    const float x = t * 6.0f;
    const int i = juce::jmin (5, (int) x);
    return stops[i].interpolatedWith (stops[i + 1], x - (float) i);
}

/** Spectrogram (log frequency 30 Hz .. fs/2) with a level strip underneath. */
juce::Image spectrogram (const juce::AudioBuffer<float>& b, double fs, const juce::String& title)
{
    const int W = 1100, H = 420, strip = 90, top = 22;
    juce::Image img (juce::Image::RGB, W, top + H + strip, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour (0xff0b0d12));
    const int order = 11, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) n), buf ((size_t) n * 2);
    for (int i = 0; i < n; ++i)   // Blackman-Harris: low leakage, so bass lines don't smear
    {
        const float x = juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1);
        win[(size_t) i] = 0.35875f - 0.48829f * std::cos (x) + 0.14128f * std::cos (2 * x) - 0.01168f * std::cos (3 * x);
    }
    const int len = b.getNumSamples();
    const double fMin = 30.0, fMax = fs * 0.5;
    for (int x = 0; x < W; ++x)
    {
        const int centre = (int) ((double) x / W * len);
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const int idx = centre - n / 2 + i;
            if (idx >= 0 && idx < len)
                buf[(size_t) i] = 0.5f * (b.getSample (0, idx) + b.getSample (1, idx)) * win[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int y = 0; y < H; ++y)
        {
            const double f = fMin * std::pow (fMax / fMin, 1.0 - (double) y / (H - 1));
            const int bin = juce::jlimit (1, n / 2 - 1, (int) std::lround (f / fs * n));
            const float mag = buf[(size_t) bin] / (float) n * 5.6f;
            const float db = 20.0f * std::log10 (std::max (mag, 1.0e-9f));
            img.setPixelAt (x, top + y, magma ((db + 120.0f) / 100.0f));
        }
    }
    // frequency labels
    g.setFont (11.0f);
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        const int y = top + (int) ((1.0 - std::log (f / fMin) / std::log (fMax / fMin)) * (H - 1));
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawHorizontalLine (y, 0.0f, (float) W);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000)) + " kHz" : juce::String ((int) f) + " Hz", 4, y - 13, 60, 12,
                    juce::Justification::left);
    }
    // level strip (RMS per column, -100..0 dB)
    const int sy = top + H;
    g.setColour (juce::Colour (0xff141820));
    g.fillRect (0, sy, W, strip);
    juce::Path p;
    for (int x = 0; x < W; ++x)
    {
        const int a = (int) ((double) x / W * len), e = (int) ((double) (x + 1) / W * len);
        double s = 0.0;
        for (int i = a; i < e; ++i) s += 0.5 * (b.getSample (0, i) * b.getSample (0, i) + b.getSample (1, i) * b.getSample (1, i));
        const float db = 10.0f * std::log10 ((float) std::max (1.0e-12, s / std::max (1, e - a)));
        const float y = sy + juce::jlimit (0.0f, 1.0f, -db / 100.0f) * (strip - 4) + 2;
        if (x == 0) p.startNewSubPath ((float) x, y);
        else p.lineTo ((float) x, y);
    }
    g.setColour (juce::Colour (0xff7fb8ff));
    g.strokePath (p, juce::PathStrokeType (1.2f));
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    for (int s = 1; s < (int) (len / fs); ++s)
    {
        const int x = (int) (s * fs / len * W);
        g.drawVerticalLine (x, (float) top, (float) (sy + strip));
    }
    g.setColour (juce::Colours::white);
    g.setFont (13.0f);
    g.drawText (title + "   (" + juce::String (len / fs, 1) + " s, grid = 1 s, level -100..0 dB)", 6, 2, W - 12, 18,
                juce::Justification::left);
    return img;
}

double t30 (const juce::AudioBuffer<float>& b, double fs)
{
    const int n = b.getNumSamples();
    std::vector<double> e ((size_t) n + 1, 0.0);
    for (int i = n - 1; i >= 0; --i)
    {
        const double m = 0.5 * (b.getSample (0, i) + b.getSample (1, i));
        e[(size_t) i] = e[(size_t) i + 1] + m * m;
    }
    if (e[0] <= 0.0) return 0.0;
    int i5 = -1, i35 = -1;
    for (int i = 0; i < n; ++i)
    {
        const double d = 10.0 * std::log10 (std::max (1.0e-30, e[(size_t) i] / e[0]));
        if (i5 < 0 && d <= -5.0) i5 = i;
        if (i35 < 0 && d <= -35.0) { i35 = i; break; }
    }
    if (i5 < 0 || i35 <= i5) return 0.0;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int cnt = 0;
    for (int i = i5; i < i35; i += 16)
    {
        const double x = i / fs, y = 10.0 * std::log10 (std::max (1.0e-30, e[(size_t) i] / e[0]));
        sx += x; sy += y; sxx += x * x; sxy += x * y; ++cnt;
    }
    const double slope = (cnt * sxy - sx * sy) / std::max (1.0e-12, cnt * sxx - sx * sx);
    return slope < 0.0 ? -60.0 / slope : 0.0;
}

std::array<float, kNumParams> defaults()
{
    std::array<float, kNumParams> v {};
    for (int k = 0; k < kNumParams; ++k) v[(size_t) k] = info ((P) k).def;
    return v;
}

/** ValhallaShimmer parity demos (see docs/PARITY.md). */
std::vector<Job> parityJobs()
{
    std::vector<Job> jobs;
    auto base = [] {
        auto v = defaults();
        v[(int) P::mode] = 2; v[(int) P::decay] = 8; v[(int) P::mix] = 45; v[(int) P::size] = 65;
        return v;
    };
    auto add = [&] (const char* name, std::initializer_list<std::pair<P, float>> kv, const char* sig = "pad", double secs = 14.0) {
        auto v = base();
        for (auto& [p, x] : kv) v[(size_t) p] = x;
        jobs.push_back ({ juce::String ("parity_") + name, v, sig, secs });
    };
    add ("mix_25", { { P::mix, 25 } });
    add ("mix_100", { { P::mix, 100 } });
    add ("pitch_plus12", { { P::shimMode, 1 }, { P::pitch, 12 }, { P::shimAmount, 50 } });
    add ("pitch_minus12", { { P::shimMode, 1 }, { P::pitch, -12 }, { P::shimAmount, 50 } });
    add ("pitch_plus7", { { P::shimMode, 1 }, { P::pitch, 7 }, { P::shimAmount, 50 } });
    add ("feedback_short_2s", { { P::decay, 2 } });
    add ("feedback_long_30s", { { P::decay, 30 } }, "pad", 24.0);
    add ("feedback_infinite_freeze", { { P::decay, 30 }, { P::freeze, 1 } }, "pad", 14.0);
    add ("diffusion_0_echoes", { { P::diffusion, 0 } }, "guitar");
    add ("diffusion_55_fadein", { { P::diffusion, 55 } }, "guitar");
    add ("diffusion_100", { { P::diffusion, 100 }, { P::decay, 1.5f } }, "guitar");
    add ("size_small", { { P::size, 15 } }, "guitar");
    add ("size_large", { { P::size, 100 } }, "guitar");
    add ("lowcut_400", { { P::lowCut, 400 } });
    add ("highcut_2k", { { P::highCut, 2000 } });
    add ("mod_slow_deep", { { P::modRate, 0.2f }, { P::modDepth, 90 }, { P::modDrift, 0 } });
    add ("mod_fast_chorus", { { P::modRate, 2.5f }, { P::modDepth, 70 }, { P::modDrift, 0 }, { P::mode, 3 }, { P::decay, 2 } }, "guitar");
    add ("mode_mono_nebula", { { P::mode, 0 } }, "guitar");
    add ("mode_big_cathedral", { { P::mode, 1 } }, "guitar");
    add ("mode_medium_hall", { { P::mode, 2 } }, "guitar");
    add ("mode_small_chamber", { { P::mode, 3 }, { P::decay, 2 } }, "guitar");
    add ("pitchmode_single", { { P::shimMode, 1 }, { P::shimAmount, 60 }, { P::shimSmooth, 30 } });
    add ("pitchmode_dual", { { P::shimMode, 2 }, { P::shimAmount, 60 }, { P::shimSmooth, 30 } });
    add ("pitchmode_single_reverse", { { P::shimMode, 3 }, { P::shimAmount, 60 }, { P::shimSmooth, 80 } });
    add ("pitchmode_dual_reverse", { { P::shimMode, 4 }, { P::shimAmount, 60 }, { P::shimSmooth, 80 } });
    add ("pitchmode_bypass", { { P::shimMode, 0 } });
    add ("pitchmode_two_voice", { { P::shimMode, 5 }, { P::pitch, 12 }, { P::pitch2, 7 }, { P::shimAmount, 60 } });
    add ("color_bright", { { P::color, 0 } }, "snare");
    add ("color_dark", { { P::color, 1 } }, "snare");
    return jobs;
}

void render (const Job& job, double fs, const juce::File& dir, juce::String& index)
{
    SchimmerProcessor proc;
    for (int k = 0; k < kNumParams; ++k)
        proc.setValueNotifying ((P) k, job.values[(size_t) k]);
    proc.prepareToPlay (fs, 512);
    auto sig = signals::byName (job.signal.toStdString(), fs, job.seconds);
    const int len = (int) sig.size();
    juce::AudioBuffer<float> out (2, len);
    juce::AudioBuffer<float> block (4, 512);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < len; pos += 512)
    {
        const int n = std::min (512, len - pos);
        block.setSize (4, n, false, false, true);
        block.clear();
        block.copyFrom (0, 0, sig.l.data() + pos, n);
        block.copyFrom (1, 0, sig.r.data() + pos, n);
        proc.processBlock (block, midi);
        out.copyFrom (0, pos, block, 0, 0, n);
        out.copyFrom (1, pos, block, 1, 0, n);
    }
    const auto base = juce::File::createLegalFileName (job.name + "__" + job.signal);
    {
        auto f = dir.getChildFile (base + ".wav");
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), fs, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (out, 0, len);
    }
    {
        auto f = dir.getChildFile (base + ".png");
        f.deleteFile();
        juce::FileOutputStream s (f);
        juce::PNGImageFormat().writeImageToStream (spectrogram (out, fs, job.name + " - " + job.signal), s);
    }
    float peak = 0.0f;
    double ss = 0.0;
    bool finite = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < len; ++i)
        {
            const float v = out.getSample (ch, i);
            finite = finite && std::isfinite (v);
            peak = std::max (peak, std::abs (v));
            ss += (double) v * v;
        }
    const double rms = std::sqrt (ss / (2.0 * len));
    juce::String line = "| " + job.name + " | " + job.signal + " | " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " | "
                        + juce::String (juce::Decibels::gainToDecibels ((float) rms), 1) + " | ";
    line += job.signal == "impulse" ? juce::String (t30 (out, fs), 2) + " s" : juce::String ("-");
    line += finite ? " |" : " NON-FINITE |";
    index << line << "\n";
    std::cout << line << std::endl;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc < 2)
    {
        std::cout << "usage: SchimmerRender <out-folder> [--preset NAME] [--signals a,b] [--parity] [--fs 48000] [--quick]" << std::endl;
        return 1;
    }
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    dir.createDirectory();
    juce::String onlyPreset;
    juce::StringArray sigs { "impulse", "noise", "sine", "pad", "snare", "guitar" };
    bool parity = false, quick = false;
    double fs = 48000.0;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--preset" && i + 1 < argc) onlyPreset = argv[++i];
        else if (a == "--signals" && i + 1 < argc) sigs = juce::StringArray::fromTokens (argv[++i], ",", "");
        else if (a == "--parity") parity = true;
        else if (a == "--quick") quick = true;
        else if (a == "--fs" && i + 1 < argc) fs = juce::String (argv[++i]).getDoubleValue();
    }

    std::vector<Job> jobs;
    if (parity)
        jobs = parityJobs();
    else
        for (const auto& p : FactoryPresets::all())
        {
            if (onlyPreset.isNotEmpty() && p.name != onlyPreset) continue;
            const auto v = FactoryPresets::values (p);
            const double decay = v[(int) P::decay];
            for (const auto& s : sigs)
            {
                auto vv = v;
                double secs = s == "impulse" ? juce::jlimit (6.0, 40.0, decay * 1.3 + 3.0) : 14.0;
                if (s == "impulse") vv[(int) P::mix] = 100.0f;   // show the wet signal
                if (quick) secs = std::min (secs, 10.0);
                jobs.push_back ({ p.name, vv, s, secs });
            }
        }

    juce::String index;
    index << "# Schimmer audio demos\n\nRendered at " << juce::String ((int) fs) << " Hz. Peak/RMS in dBFS. "
          << "RT60 measured (T30) on the impulse renders (100 % wet).\n\n"
          << "| Preset | Signal | Peak dB | RMS dB | RT60 |\n|---|---|---|---|---|\n";
    for (const auto& j : jobs)
        render (j, fs, dir, index);
    dir.getChildFile ("INDEX.md").replaceWithText (index);
    return 0;
}
