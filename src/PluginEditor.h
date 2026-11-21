#pragma once
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/MainView.h"

namespace schimmer
{
/** Hosts the MainView at a fixed design size and scales it (resizable, fixed aspect ratio). */
class SchimmerEditor : public juce::AudioProcessorEditor
{
public:
    explicit SchimmerEditor (SchimmerProcessor&);
    ~SchimmerEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    MainView& getView() noexcept { return view; }

private:
    SchimmerProcessor& plugin;
    SchLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 };
};
} // namespace schimmer
