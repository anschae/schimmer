#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Widgets.h"
#include "Parameters.h"

namespace schimmer
{
class SchimmerProcessor;

/** A titled panel that lays out controls for a list of parameters in a grid. */
class ParamPanel : public juce::Component
{
public:
    ParamPanel (SchimmerProcessor& p, const juce::String& title, std::vector<P> params, juce::Colour accent, int columns);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title;
    int columns;
    std::vector<std::unique_ptr<juce::Component>> controls;
};

/** The whole editor at its design size (scaled by the editor). */
class MainView : public juce::Component
{
public:
    static constexpr int kDesignW = 1280, kDesignH = 820;
    explicit MainView (SchimmerProcessor&);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SchimmerProcessor& proc;
    juce::ComboBox presetBox;
    juce::TextButton clearButton { "Clear Tail" };
    std::vector<std::unique_ptr<ParamPanel>> panels;
    void refreshPresets();
};
} // namespace schimmer
