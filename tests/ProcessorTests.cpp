#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"
#include "model/FactoryPresets.h"
#include "Analysis.h"

using namespace schimmer;

TEST_CASE ("Parameter IDs are stable", "[params]")
{
    // Written out by hand on purpose: DAW projects and automation depend on these IDs.
    // Never rename or remove one; append new parameters at the end of the table.
    const char* expected[] = {
        "mode", "size", "preDelay", "preDelaySync", "preDelayNote", "decay", "freeze", "freezeMode", "diffusion", "bloom",
        "modRate", "modDepth", "modDrift", "lowCut", "highCut", "lowMult", "highMult", "lowXover", "highXover", "color", "warmth", "quality",
        "inHP", "inLP", "tilt", "width", "bassMono", "mix", "mixLock", "output",
        "shimMode", "pitch", "pitchSnap", "fine", "pitch2", "voiceMix", "shimAmount", "shimLowCut", "shimHighCut", "shimSmooth", "shimPlace",
        "grainOn", "grainSource", "grainDest", "grainSize", "grainDensity", "grainPitch", "grainQuant", "grainSpray", "grainPitchJit",
        "grainReverse", "grainScatter", "grainAmount",
        "delayOn", "delayTime", "delaySync", "delayNote", "delayFeedback", "delayPingPong", "delayOffset", "delayHP", "delayLP", "delayMod",
        "delayToReverb", "delayLevel",
        "clearTail", "clearTarget", "autoClear",
        "duckOn", "duckSource", "duckHP", "duckLP", "duckDetMode", "duckRms", "duckThresh", "duckDepth", "duckFull", "duckKnee",
        "duckAttack", "duckHold", "duckRelease", "duckRelSync", "duckRelNote", "duckLook", "duckTarget", "duckPos",
        "morphOn", "morph" };
    REQUIRE (std::size (expected) == (size_t) kNumParams);
    SchimmerProcessor p;
    REQUIRE (p.getParameters().size() == kNumParams);
    for (int k = 0; k < kNumParams; ++k)
    {
        CHECK (juce::String (pid ((P) k)) == expected[k]);
        CHECK (p.getApvts().getParameter (expected[k]) != nullptr);
    }
}

TEST_CASE ("Every parameter has a tooltip and a sensible default", "[params]")
{
    for (int k = 0; k < kNumParams; ++k)
    {
        const auto& i = info ((P) k);
        INFO (i.id);
        CHECK (juce::String (i.tooltip).length() > 10);
        CHECK (i.def >= i.min);
        CHECK (i.def <= i.max);
    }
}

TEST_CASE ("State round trip restores parameters and morph snapshots", "[state]")
{
    SchimmerProcessor a;
    a.setValueNotifying (P::decay, 33.0f);
    a.setValueNotifying (P::mode, 3.0f);
    a.setValueNotifying (P::shimMode, 4.0f);
    a.setValueNotifying (P::pitch, -7.0f);
    auto snap = a.captureSnapshot();
    snap[(size_t) P::size] = 12.0f;
    a.setMorphSnapshot (1, snap);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    SchimmerProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (std::abs (b.value (P::decay) - 33.0f) < 0.01f);
    CHECK (b.value (P::mode) == 3.0f);
    CHECK (b.value (P::shimMode) == 4.0f);
    CHECK (std::abs (b.value (P::pitch) + 7.0f) < 0.01f);
    CHECK (b.hasMorphSnapshot (1));
    CHECK (! b.hasMorphSnapshot (0));
    CHECK (std::abs (b.getMorphSnapshot (1)[(size_t) P::size] - 12.0f) < 0.01f);
}

TEST_CASE ("An older state without newer parameters loads with defaults", "[state]")
{
    // simulate a state written before some parameters existed
    SchimmerProcessor a;
    a.setValueNotifying (P::decay, 20.0f);
    a.setValueNotifying (P::grainAmount, 90.0f);
    auto st = a.captureState();
    auto params = st.getChildWithName ("PARAMS");
    for (int i = params.getNumChildren(); --i >= 0;)
        if (params.getChild (i).getProperty ("id").toString() == "grainAmount")
            params.removeChild (i, nullptr);
    SchimmerProcessor b;
    b.setValueNotifying (P::grainAmount, 10.0f);
    b.restoreState (st);
    CHECK (std::abs (b.value (P::decay) - 20.0f) < 0.01f);
    CHECK (std::abs (b.value (P::grainAmount) - info (P::grainAmount).def) < 0.01f);
}

TEST_CASE ("Factory presets load and produce finite audio", "[presets]")
{
    const auto& all = FactoryPresets::all();
    REQUIRE (all.size() >= 20);
    SchimmerProcessor p;
    p.prepareToPlay (48000.0, 256);
    for (const auto& e : p.getPresets().list())
    {
        if (! e.factory) continue;
        INFO (e.name);
        REQUIRE (p.getPresets().load (e));
        CHECK (p.getCurrentPresetName() == "Factory / " + e.name);
        juce::AudioBuffer<float> buf (4, 256);
        juce::MidiBuffer midi;
        juce::Random rng (3);
        bool finite = true;
        for (int b = 0; b < 100; ++b)
        {
            buf.clear();
            if (b < 20)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i) buf.setSample (ch, i, rng.nextFloat() - 0.5f);
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i) finite = finite && std::isfinite (buf.getSample (ch, i));
        }
        CHECK (finite);
    }
}

TEST_CASE ("Mix lock keeps the mix when loading presets", "[presets]")
{
    SchimmerProcessor p;
    p.setValueNotifying (P::mix, 77.0f);
    p.setValueNotifying (P::mixLock, 1.0f);
    for (const auto& e : p.getPresets().list())
        if (e.name == "Send: Huge Hall")
            p.getPresets().load (e);
    CHECK (std::abs (p.value (P::mix) - 77.0f) < 0.01f);
    CHECK (std::abs (p.value (P::decay) - 6.0f) < 0.01f);
}

TEST_CASE ("User presets save, list and load", "[presets]")
{
    SchimmerProcessor p;
    p.setValueNotifying (P::decay, 42.0f);
    REQUIRE (p.getPresets().saveUser ("My Space"));
    p.setValueNotifying (P::decay, 3.0f);
    bool found = false;
    for (const auto& e : p.getPresets().list())
        if (! e.factory && e.name == "My Space")
        {
            found = true;
            REQUIRE (p.getPresets().load (e));
        }
    CHECK (found);
    CHECK (std::abs (p.value (P::decay) - 42.0f) < 0.01f);
}

TEST_CASE ("Lookahead latency is reported", "[latency]")
{
    SchimmerProcessor p;
    p.setValueNotifying (P::duckLook, 5.0f);
    p.prepareToPlay (48000.0, 512);
    CHECK (p.getLatencySamples() == 240);
}

TEST_CASE ("Dry path is delayed by exactly the reported latency", "[latency]")
{
    SchimmerProcessor p;
    p.setValueNotifying (P::duckLook, 3.0f);
    p.setValueNotifying (P::mix, 0.0f);
    p.prepareToPlay (48000.0, 64);
    const int lat = p.getLatencySamples();
    juce::AudioBuffer<float> buf (4, 64);
    juce::MidiBuffer midi;
    std::vector<float> out;
    for (int b = 0; b < 10; ++b)
    {
        buf.clear();
        if (b == 0) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); }
        p.processBlock (buf, midi);
        for (int i = 0; i < 64; ++i) out.push_back (buf.getSample (0, i));
    }
    const auto it = std::max_element (out.begin(), out.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
    CHECK ((int) std::distance (out.begin(), it) == lat);
}

TEST_CASE ("Mono input works and produces a stereo tail", "[buses]")
{
    SchimmerProcessor p;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::mono());
    layout.inputBuses.add (juce::AudioChannelSet::disabled());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    REQUIRE (p.setBusesLayout (layout));
    p.setValueNotifying (P::mix, 100.0f);
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double l = 0, r = 0, lr = 0;
    for (int b = 0; b < 100; ++b)
    {
        buf.clear();
        if (b == 0) buf.setSample (0, 0, 1.0f);
        p.processBlock (buf, midi);
        if (b > 10)
            for (int i = 0; i < 512; ++i)
            {
                l += buf.getSample (0, i) * buf.getSample (0, i);
                r += buf.getSample (1, i) * buf.getSample (1, i);
                lr += buf.getSample (0, i) * buf.getSample (1, i);
            }
    }
    CHECK (l > 1.0e-4);
    CHECK (r > 1.0e-4);
    CHECK (std::abs (lr / std::sqrt (l * r)) < 0.3);
}

TEST_CASE ("Any block size gives the same output", "[blocksize]")
{
    auto run = [] (int block) {
        SchimmerProcessor p;
        p.setValueNotifying (P::mix, 100.0f);
        p.prepareToPlay (48000.0, block);
        std::vector<float> out;
        juce::MidiBuffer midi;
        const int total = 48000;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            juce::AudioBuffer<float> buf (4, n);
            buf.clear();
            if (pos == 0) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); }
            p.processBlock (buf, midi);
            for (int i = 0; i < n; ++i) out.push_back (buf.getSample (0, i));
        }
        return out;
    };
    const auto a = run (512), b = run (17), c = run (1024);
    // not bit-exact (parameter interpolation follows the block boundaries), but inaudibly close
    double d1 = 0, d2 = 0, pk = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        d1 += (double) (a[i] - b[i]) * (a[i] - b[i]);
        d2 += (double) (a[i] - c[i]) * (a[i] - c[i]);
        pk += (double) a[i] * a[i];
    }
    INFO ("difference energy " << 10.0 * std::log10 (d1 / pk) << " / " << 10.0 * std::log10 (d2 / pk) << " dB");
    CHECK (d1 < pk * 1.0e-6);
    CHECK (d2 < pk * 1.0e-6);
}

TEST_CASE ("Clear Tail parameter triggers on the rising edge only", "[clear]")
{
    SchimmerProcessor p;
    p.setValueNotifying (P::mix, 100.0f);
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (4, 512);
    juce::MidiBuffer midi;
    auto block = [&] { buf.clear(); p.processBlock (buf, midi); };
    block();
    p.setValueNotifying (P::clearTail, 1.0f);
    for (int i = 0; i < 5; ++i) block();
    CHECK (p.getEngine().clearCount.load() == 1);
    p.setValueNotifying (P::clearTail, 0.0f);
    block();
    p.setValueNotifying (P::clearTail, 1.0f);
    block();
    CHECK (p.getEngine().clearCount.load() == 2);
}

#include "model/Randomizer.h"

TEST_CASE ("Randomize respects locks and sensible ranges", "[randomize]")
{
    SchimmerProcessor p;
    juce::Random rng (11);
    const float mixBefore = p.value (P::mix);
    const uint32_t lockDelay = 1u << 4;
    const float delayFbBefore = p.value (P::delayFeedback);
    for (int round = 0; round < 50; ++round)
    {
        Randomizer::randomize (p, lockDelay, rng);
        CHECK (p.value (P::mix) == mixBefore);                 // never randomized
        CHECK (p.value (P::delayFeedback) == delayFbBefore);   // locked section
        CHECK (p.value (P::decay) >= 2.4f);
        CHECK (p.value (P::decay) <= 40.5f);
        CHECK (p.value (P::diffusion) >= 54.0f);
        CHECK (p.value (P::freeze) < 0.5f);
    }
}

TEST_CASE ("Morph glides between snapshot A and B", "[morph]")
{
    auto tailEnergy = [] (float morph) {
        SchimmerProcessor p;
        p.setValueNotifying (P::mix, 100.0f);
        p.setValueNotifying (P::decay, 1.0f);
        p.setMorphSnapshot (0, p.captureSnapshot());
        p.setValueNotifying (P::decay, 30.0f);
        p.setMorphSnapshot (1, p.captureSnapshot());
        p.setValueNotifying (P::decay, 5.0f);   // the knob value is ignored while morphing
        p.setValueNotifying (P::morphOn, 1.0f);
        p.setValueNotifying (P::morph, morph);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (4, 512);
        juce::MidiBuffer midi;
        double e = 0.0;
        for (int b = 0; b < 400; ++b)
        {
            buf.clear();
            if (b == 0) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); }
            p.processBlock (buf, midi);
            if (b > 190)   // after 2 s
                for (int i = 0; i < 512; ++i) e += buf.getSample (0, i) * buf.getSample (0, i);
        }
        return 10.0 * std::log10 (e + 1.0e-30);
    };
    const double a = tailEnergy (0.0f), mid = tailEnergy (50.0f), b = tailEnergy (100.0f);
    INFO ("tail energy after 2 s: A " << a << " dB, 50 % " << mid << " dB, B " << b << " dB");
    CHECK (a < mid - 20.0);
    CHECK (mid < b);
}
