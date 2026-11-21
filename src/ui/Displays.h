#pragma once
// Visualisations: Tail View (timeline from the input), decay-vs-frequency
// curve, stereo/band meters. Data from the audio thread arrives through the
// engine's lock-free meter FIFO, read by MeterHub on the message thread.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Parameters.h"
#include "dsp/Engine.h"

namespace schimmer
{
class SchimmerProcessor;

/** Reads the meter FIFO at 30 Hz and keeps short histories for the displays. */
class MeterHub : private juce::Timer
{
public:
    explicit MeterHub (SchimmerProcessor& p);
    ~MeterHub() override;

    struct Point { float t, wetDb, grDb; };
    /** Trace of the wet level since the last detected input onset. */
    const std::vector<Point>& getTrace() const noexcept { return trace; }
    dsp::MeterFrame latest {};
    float corrSmoothed = 0.0f, lowDb = -120, midDb = -120, highDb = -120, wetDb = -120;
    std::function<void()> onUpdate;

private:
    void timerCallback() override;
    SchimmerProcessor& proc;
    std::vector<Point> trace;
    float traceT = 0.0f, minIn = 0.0f, lastIn = -140.0f;
    int sinceOnset = 1000000;
};

/** Settings as the engine sees them right now (message thread). */
dsp::Settings currentSettings (SchimmerProcessor& p);

class TailView : public juce::Component, public juce::SettableTooltipClient
{
public:
    TailView (SchimmerProcessor& p, MeterHub& hub);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum class Handle { None, PreDelay, Decay, DelayTime };
    juce::Rectangle<float> plotArea() const;
    float timeToX (float t) const;
    float xToTime (float x) const;
    float dbToY (float db) const;
    Handle hitTest (juce::Point<float>) const;
    juce::Point<float> handlePos (Handle) const;
    void setParam (P p, float plain);

    SchimmerProcessor& proc;
    MeterHub& hub;
    float tMax = 10.0f;
    float anchorDb = -12.0f;   // level of the expected envelope's peak (follows the measured level)
    Handle dragging = Handle::None, hover = Handle::None;
    float dragStartValue = 0.0f;
    static constexpr float kDbTop = 0.0f, kDbBottom = -72.0f;
};

class DecayCurveView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DecayCurveView (SchimmerProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Handle { None, Low, Mid, High, LowX, HighX, LowCut, HighCut };
    juce::Rectangle<float> plotArea() const;
    float fToX (float f) const;
    float xToF (float x) const;
    float tToY (float t) const;
    float yToT (float y) const;
    Handle hitTest (juce::Point<float>) const;
    juce::Point<float> handlePos (Handle) const;
    void setParam (P p, float plain);

    SchimmerProcessor& proc;
    Handle dragging = Handle::None, hover = Handle::None;
};

class StereoMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    StereoMeter (SchimmerProcessor& p, MeterHub& hub);
    void paint (juce::Graphics&) override;

private:
    SchimmerProcessor& proc;
    MeterHub& hub;
};
} // namespace schimmer
