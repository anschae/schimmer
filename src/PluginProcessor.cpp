#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace schimmer
{
SchimmerProcessor::SchimmerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int k = 0; k < kNumParams; ++k)
    {
        raw[(size_t) k] = apvts.getRawParameterValue (pid ((P) k));
        jassert (raw[(size_t) k] != nullptr);
    }
    startTimerHz (20);
}

SchimmerProcessor::~SchimmerProcessor() { stopTimer(); }

bool SchimmerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void SchimmerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.setQuality ((dsp::Quality) juce::roundToInt (value (P::quality)));
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (dsp::Engine::lookaheadSamplesFor (value (P::duckLook), sampleRate));
}

void SchimmerProcessor::reset() { engine.reset(); }

void SchimmerProcessor::setValueNotifying (P p, float plainValue)
{
    if (auto* prm = apvts.getParameter (pid (p)))
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
}

void SchimmerProcessor::triggerClearTail()
{
    setValueNotifying (P::clearTail, 1.0f);
    clearResetCountdown = 3;   // timer ticks (~150 ms) until the button springs back
}

void SchimmerProcessor::timerCallback()
{
    // latency (lookahead) changes are reported from the message thread
    const int lat = dsp::Engine::lookaheadSamplesFor (value (P::duckLook), currentSampleRate);
    if (lat != getLatencySamples())
        setLatencySamples (lat);

    // quality changes reallocate the tank: never on the audio thread
    const auto q = (dsp::Quality) juce::roundToInt (value (P::quality));
    if ((q == dsp::Quality::Eco) != (engine.getQuality() == dsp::Quality::Eco))
    {
        suspendProcessing (true);
        engine.setQuality (q);
        engine.reset();
        suspendProcessing (false);
    }

    if (clearResetCountdown > 0 && --clearResetCountdown == 0)
        setValueNotifying (P::clearTail, 0.0f);
}

bool SchimmerProcessor::isMorphable (P p) noexcept
{
    switch (p)
    {
        case P::morphOn: case P::morph: case P::clearTail: case P::quality: case P::mixLock: case P::output:
        case P::freeze: case P::duckLook:
            return false;
        default: return true;
    }
}

SchimmerProcessor::Snapshot SchimmerProcessor::captureSnapshot() const
{
    Snapshot s {};
    for (int k = 0; k < kNumParams; ++k)
        s[(size_t) k] = value ((P) k);
    return s;
}

void SchimmerProcessor::setMorphSnapshot (int ab, const Snapshot& s)
{
    ab = juce::jlimit (0, 1, ab);
    morphSnap[ab] = s;
    morphValid[ab] = true;
    publishMorph();
}

void SchimmerProcessor::publishMorph()
{
    const juce::SpinLock::ScopedLockType lock (morphLock);
    for (int ab = 0; ab < 2; ++ab)
        morphAudio[ab] = morphValid[ab] ? morphSnap[ab] : morphSnap[1 - ab];
    morphAudioValid = morphValid[0] || morphValid[1];
}

dsp::Settings SchimmerProcessor::makeSettings (const float* v, double bpm, double barWholes) noexcept
{
    auto f = [v] (P p) { return v[(int) p]; };
    auto b = [v] (P p) { return v[(int) p] > 0.5f; };
    auto c = [v] (P p) { return juce::roundToInt (v[(int) p]); };
    const double wholeMs = 240000.0 / juce::jmax (20.0, bpm);

    dsp::Settings s;
    s.mode = c (P::mode);
    s.size = f (P::size) * 0.01f;
    s.preDelayMs = b (P::preDelaySync) ? (float) (noteWholes (c (P::preDelayNote)) * wholeMs) : f (P::preDelay);
    s.decaySec = f (P::decay);
    s.freeze = b (P::freeze);
    s.freezeMode = (dsp::FreezeMode) c (P::freezeMode);
    s.diffusion = f (P::diffusion) * 0.01f;
    s.bloomSec = f (P::bloom);
    s.modRateHz = f (P::modRate);
    s.modDepth = f (P::modDepth) * 0.01f;
    s.drift = f (P::modDrift) * 0.01f;
    s.lowCutHz = f (P::lowCut);
    s.highCutHz = f (P::highCut);
    s.lowMult = f (P::lowMult);
    s.highMult = f (P::highMult);
    s.lowXHz = f (P::lowXover);
    s.highXHz = f (P::highXover);
    s.dark = c (P::color) == 1;
    s.warmth = f (P::warmth) * 0.01f;
    s.quality = (dsp::Quality) c (P::quality);

    s.inHpHz = f (P::inHP);
    s.inLpHz = f (P::inLP);
    s.tiltDb = f (P::tilt);
    s.width = f (P::width) * 0.01f;
    s.bassMonoHz = f (P::bassMono);
    s.mix = f (P::mix) * 0.01f;
    s.outDb = f (P::output);

    auto& sh = s.shimmer;
    sh.mode = (dsp::ShimmerMode) c (P::shimMode);
    const bool snap = b (P::pitchSnap);
    const float fine = f (P::fine) / 100.0f;
    sh.semis = (snap ? std::round (f (P::pitch)) : f (P::pitch)) + fine;
    sh.semis2 = (snap ? std::round (f (P::pitch2)) : f (P::pitch2)) + fine;
    sh.voiceMix = f (P::voiceMix) * 0.01f;
    sh.amount = f (P::shimAmount) * 0.01f;
    sh.lowCutHz = f (P::shimLowCut);
    sh.highCutHz = f (P::shimHighCut);
    sh.smooth = f (P::shimSmooth) * 0.01f;
    s.shimmerPre = c (P::shimPlace) == 1;

    auto& g = s.grains;
    g.on = b (P::grainOn);
    g.sourceTank = c (P::grainSource) == 1;
    g.intoTank = c (P::grainDest) == 1;
    g.sizeMs = f (P::grainSize);
    g.density = f (P::grainDensity);
    g.pitch = f (P::grainPitch);
    g.quantize = b (P::grainQuant);
    g.sprayMs = f (P::grainSpray);
    g.pitchJitter = f (P::grainPitchJit);
    g.reverseProb = f (P::grainReverse) * 0.01f;
    g.scatter = f (P::grainScatter) * 0.01f;
    g.amount = f (P::grainAmount) * 0.01f;

    auto& e = s.echo;
    e.on = b (P::delayOn);
    e.timeMs = b (P::delaySync) ? (float) (noteWholes (c (P::delayNote)) * wholeMs) : f (P::delayTime);
    e.timeMs = juce::jlimit (1.0f, 5900.0f, e.timeMs);
    e.feedback = f (P::delayFeedback) * 0.01f;
    e.pingPong = b (P::delayPingPong);
    e.offsetMs = f (P::delayOffset);
    e.hpHz = f (P::delayHP);
    e.lpHz = f (P::delayLP);
    e.mod = f (P::delayMod) * 0.01f;
    s.delayToReverb = f (P::delayToReverb) * 0.01f;
    s.delayDirectDb = f (P::delayLevel);

    s.clearTrigger = b (P::clearTail);
    s.clearDelayToo = c (P::clearTarget) == 1;

    auto& d = s.duck;
    d.on = b (P::duckOn);
    d.hpHz = f (P::duckHP);
    d.lpHz = f (P::duckLP);
    d.mode = (dsp::DetMode) c (P::duckDetMode);
    d.rmsMs = f (P::duckRms);
    d.threshDb = f (P::duckThresh);
    d.depthDb = f (P::duckDepth);
    d.full = b (P::duckFull);
    d.kneeDb = f (P::duckKnee);
    d.attackMs = f (P::duckAttack);
    d.holdMs = f (P::duckHold);
    d.releaseMs = f (P::duckRelease);
    if (b (P::duckRelSync))
    {
        double wholes = kRelNoteWholes[juce::jlimit (0, 4, c (P::duckRelNote))];
        if (wholes <= 0.0) wholes = barWholes;
        d.releaseMs = (float) juce::jlimit (10.0, 10000.0, wholes * wholeMs);
    }
    s.duckSource = (dsp::DuckSource) c (P::duckSource);
    s.duckTarget = (dsp::DuckTarget) c (P::duckTarget);
    s.duckPos = (dsp::DuckPos) c (P::duckPos);
    s.lookaheadMs = f (P::duckLook);
    return s;
}

void SchimmerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int n = buffer.getNumSamples();
    if (mainOut.getNumChannels() < 2 || n == 0)
        return;
    if (mainIn.getNumChannels() == 1)
        mainOut.copyFrom (1, 0, mainOut, 0, 0, n);   // mono in -> duplicate (the engine takes stereo)

    // ---- host timing
    double bpm = 120.0, barWholes = 1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto t = pos->getBpm()) bpm = *t;
            if (auto sig = pos->getTimeSignature())
                barWholes = (double) sig->numerator / juce::jmax (1, sig->denominator);
            playing = pos->getIsPlaying();
        }
    hostBpm.store (bpm, std::memory_order_relaxed);
    hostPlaying.store (playing, std::memory_order_relaxed);

    // ---- parameter values (morph applied)
    std::array<float, kNumParams> v;
    for (int k = 0; k < kNumParams; ++k)
        v[(size_t) k] = raw[(size_t) k]->load (std::memory_order_relaxed);
    if (v[(int) P::morphOn] > 0.5f)
    {
        const juce::SpinLock::ScopedTryLockType lock (morphLock);
        if (lock.isLocked() && morphAudioValid)
        {
            const float t = v[(int) P::morph] * 0.01f;
            const bool lockMix = v[(int) P::mixLock] > 0.5f;
            for (int k = 0; k < kNumParams; ++k)
            {
                const auto p = (P) k;
                if (! isMorphable (p) || (lockMix && p == P::mix))
                    continue;
                const auto u = info (p).unit;
                const float a = morphAudio[0][(size_t) k], bb = morphAudio[1][(size_t) k];
                v[(size_t) k] = (u == Unit::Bool || u == Unit::Choice) ? (t < 0.5f ? a : bb) : a + t * (bb - a);
            }
        }
    }

    auto s = makeSettings (v.data(), bpm, barWholes);

    // auto clear on transport start
    if (playing && ! wasPlaying && v[(int) P::autoClear] > 0.5f)
        transportClearPulse.store (true);
    wasPlaying = playing;
    if (transportClearPulse.exchange (false))
    {
        // a one-block pulse: force an edge (off for this call, then on)
        s.clearTrigger = false;
        engine.process (mainOut.getWritePointer (0), mainOut.getWritePointer (1), 0, s);
        s.clearTrigger = true;
        s.clearDelayToo = true;
    }

    // ---- sidechain
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            scL = sc.getReadPointer (0);
            scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
        }
    }
    engine.process (mainOut.getWritePointer (0), mainOut.getWritePointer (1), n, s, scL, scR);
}

double SchimmerProcessor::getTailLengthSeconds() const
{
    if (value (P::freeze) > 0.5f)
        return 600.0;
    double t = value (P::decay) * 1.3 + value (P::preDelay) * 0.001 + value (P::bloom) * 2.0;
    if (value (P::delayOn) > 0.5f)
    {
        const double fb = value (P::delayFeedback) * 0.01;
        const double repeats = fb > 0.001 ? std::ceil (std::log (0.001) / std::log (std::max (0.001, fb))) : 1.0;
        t += std::min (60.0, repeats * value (P::delayTime) * 0.001);
    }
    return std::min (t, 600.0);
}

juce::AudioProcessorEditor* SchimmerProcessor::createEditor() { return new SchimmerEditor (*this); }

// ---------------------------------------------------------------------------- state
juce::ValueTree SchimmerProcessor::captureState()
{
    juce::ValueTree root ("SchimmerState");
    root.setProperty ("stateVersion", kStateVersion, nullptr);
    root.setProperty ("presetName", currentPresetName, nullptr);
    root.setProperty ("randomLocks", (int) randomLocks, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    juce::ValueTree morph ("MORPH");
    for (int ab = 0; ab < 2; ++ab)
    {
        if (! morphValid[ab]) continue;
        juce::ValueTree snap (ab == 0 ? "A" : "B");
        for (int k = 0; k < kNumParams; ++k)
            snap.setProperty (pid ((P) k), morphSnap[ab][(size_t) k], nullptr);
        morph.appendChild (snap, nullptr);
    }
    root.appendChild (morph, nullptr);
    return root;
}

void SchimmerProcessor::restoreState (const juce::ValueTree& state)
{
    if (! state.hasType ("SchimmerState"))
        return;
    // future versions: migrate older states here (stateVersion < kStateVersion)
    const auto params = state.getChildWithName (apvts.state.getType());
    if (params.isValid())
    {
        // parameters missing in an older state keep their defaults
        for (int k = 0; k < kNumParams; ++k)
            if (auto* prm = apvts.getParameter (pid ((P) k)))
                prm->setValueNotifyingHost (prm->getDefaultValue());
        apvts.replaceState (params);
    }
    if (state.hasProperty ("presetName"))
        currentPresetName = state.getProperty ("presetName").toString();
    if (state.hasProperty ("randomLocks"))
        randomLocks = (uint32_t) (int) state.getProperty ("randomLocks");
    const auto morph = state.getChildWithName ("MORPH");
    for (int ab = 0; ab < 2; ++ab)
    {
        morphValid[ab] = false;
        const auto snap = morph.getChildWithName (ab == 0 ? "A" : "B");
        if (! snap.isValid()) continue;
        Snapshot s = captureSnapshot();
        for (int k = 0; k < kNumParams; ++k)
            if (snap.hasProperty (pid ((P) k)))
                s[(size_t) k] = (float) snap.getProperty (pid ((P) k));
        morphSnap[ab] = s;
        morphValid[ab] = true;
    }
    publishMorph();
}

void SchimmerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = captureState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SchimmerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreState (juce::ValueTree::fromXml (*xml));
}
} // namespace schimmer
