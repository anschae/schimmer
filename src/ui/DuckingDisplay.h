#pragma once
#include <vector>
#include "Theme.h"
#include "dsp/Ducker.h"

namespace schimmer
{
class SchimmerProcessor;

/** Scrolling history of the detector level, the threshold line and the
    gain-reduction curve, plus a GR readout. Fed by the ducker's lock-free FIFO. */
class DuckingDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit DuckingDisplay (SchimmerProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float dbToY (float db, juce::Rectangle<float> r) const;

    SchimmerProcessor& proc;
    static constexpr int kHistory = 400;   // frames of 10 ms = 4 s
    std::vector<dsp::Ducker::Frame> history;
    int writePos = 0;
    std::vector<dsp::Ducker::Frame> scratch;
};
} // namespace schimmer
