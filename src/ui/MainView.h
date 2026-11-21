#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Widgets.h"
#include "Parameters.h"
#include "Displays.h"
#include "DuckingDisplay.h"
#include "PresetBar.h"

namespace schimmer
{
class SchimmerProcessor;

/** A titled panel that lays out controls for a list of parameters in a grid. */
class ParamPanel : public juce::Component
{
public:
    ParamPanel (SchimmerProcessor& p, const juce::String& title, std::vector<P> params, juce::Colour accent, int columns,
                int rowHeight = 92, const juce::String& stripPrefix = {});
    /** Enables/disables the control of one parameter (e.g. delay time while synced). */
    void setControlEnabled (P p, bool enabled);
    void paint (juce::Graphics&) override;
    void resized() override;
    /** Area below the grid (for extra components). */
    juce::Rectangle<int> getFreeArea() const noexcept { return freeArea; }
    bool drawFrame = true;

private:
    juce::String title;
    int columns, rowH;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<P> ids;
    juce::Rectangle<int> freeArea;
};

/** The whole editor at its design size (scaled by the editor). */
class MainView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kDesignW = 1280, kDesignH = 820;
    explicit MainView (SchimmerProcessor&);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    /** Selects a tab page (also used by the snapshot tool). */
    void selectPage (int index) { tabs.setSelected (index); showPage (index); }
    static constexpr int kNumPages = 6;

private:
    void timerCallback() override;
    void showPage (int index);
    void showLockMenu();

    SchimmerProcessor& proc;
    MeterHub hub;
    PresetBar presetBar;
    juce::TextButton randomButton { "Randomize" }, lockButton { "Locks" }, clearButton { "Clear Tail" };
    ChipToggle freezeButton;
    juce::ComboBox qualityBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAtt;

    TailView tailView;
    DecayCurveView decayView;
    StereoMeter stereoMeter;

    // core row
    Segmented modeSel, colorSel;
    std::vector<std::unique_ptr<Knob>> coreKnobs;
    ChipToggle preSync;
    juce::ComboBox preNote;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> preNoteAtt;

    // pages
    Segmented tabs;
    std::vector<std::unique_ptr<juce::Component>> pages;
    DuckingDisplay duckDisplay;
    juce::TextButton storeA { "Store A" }, storeB { "Store B" };
    int currentPage = 0;
};
} // namespace schimmer
