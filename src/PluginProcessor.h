#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/Engine.h"
#include "model/PresetManager.h"

namespace schimmer
{
class SchimmerProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int kStateVersion = 1;

    SchimmerProcessor();
    ~SchimmerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return SCH_PLUGIN_NAME; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- editor, presets, tests
    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    dsp::Engine& getEngine() noexcept { return engine; }
    float value (P p) const noexcept { return raw[(int) p]->load (std::memory_order_relaxed); }
    void setValueNotifying (P p, float plainValue);

    juce::ValueTree captureState();
    void restoreState (const juce::ValueTree& state);

    PresetManager& getPresets() noexcept { return presets; }
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName (const juce::String& n) { currentPresetName = n; }

    /** Triggers Clear Tail from the UI (sets the parameter and resets it shortly after). */
    void triggerClearTail();

    // ---- morph snapshots (message thread)
    using Snapshot = std::array<float, kNumParams>;
    Snapshot captureSnapshot() const;
    void setMorphSnapshot (int ab, const Snapshot& s);
    bool hasMorphSnapshot (int ab) const noexcept { return morphValid[ab]; }
    const Snapshot& getMorphSnapshot (int ab) const noexcept { return morphSnap[ab]; }
    /** Plain value the engine currently uses for a parameter (morph applied). */
    static bool isMorphable (P p) noexcept;

    /** Current host tempo (for the UI). */
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };

    /** Builds the engine settings from plain parameter values (also used by tests). */
    static dsp::Settings makeSettings (const float* v, double bpm, double barWholes) noexcept;

private:
    void timerCallback() override;
    void publishMorph();

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, kNumParams> raw {};
    dsp::Engine engine;
    PresetManager presets { *this };
    juce::String currentPresetName { "Factory / Init" };
    double currentSampleRate = 48000.0;
    bool wasPlaying = false;
    std::atomic<bool> transportClearPulse { false };
    int clearResetCountdown = 0;

    // morph: message thread writes, audio thread copies with a try-lock
    Snapshot morphSnap[2] {};
    bool morphValid[2] { false, false };
    juce::SpinLock morphLock;
    Snapshot morphAudio[2] {};
    bool morphAudioValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchimmerProcessor)
};
} // namespace schimmer
