#pragma once
// Reusable controls: Knob (rotary + value + caption, double-click to type a
// value), Segmented (row of connected buttons for a choice), ChipToggle.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace schimmer
{
/** Rotary knob with the value above the caption. Either attached to a parameter
    or driven by callbacks (for non-parameter values such as Randomize Amount). */
class Knob : public juce::Component, private juce::Slider::Listener
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption,
          juce::Colour colour);
    /** Non-parameter knob, range 0..1, shown as percent. */
    Knob (const juce::String& caption, juce::Colour colour, std::function<float()> get, std::function<void (float)> set);

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void setColour (juce::Colour c);
    void setTooltipText (const juce::String& t) { slider.setTooltip (t); }
    void setValueFormatter (std::function<juce::String (float)> f) { formatter = std::move (f); repaint(); }
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void refresh() { if (getter) slider.setValue (getter(), juce::dontSendNotification); repaint(); }
    juce::Slider& getSlider() noexcept { return slider; }

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    void showEditor();
    juce::String valueText() const;

    struct DblClickSlider : juce::Slider
    {
        std::function<void()> onDouble;
        void mouseDoubleClick (const juce::MouseEvent&) override { if (onDouble) onDouble(); }
    };

    DblClickSlider slider;
    juce::String caption;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::function<float()> getter;
    std::function<void (float)> setter;
    std::function<juce::String (float)> formatter;
    std::unique_ptr<juce::TextEditor> editor;
};

/** A row of connected toggle buttons for a choice parameter (or for a callback). */
class Segmented : public juce::Component
{
public:
    /** Attached to a choice parameter; `labels` override the parameter's choice names. */
    Segmented (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, juce::StringArray labels = {});
    /** Free-standing: call setSelected() to update, onChange is called on clicks. */
    explicit Segmented (juce::StringArray labels);

    std::function<void (int)> onChange;
    void setSelected (int index);
    int getSelected() const noexcept { return selected; }
    void setAccent (juce::Colour c);
    void setItemAccent (int index, juce::Colour c);
    void setItemEnabled (int index, bool enabled);
    void setItemTooltip (int index, const juce::String& t);
    /** Tooltip for all items that have none of their own. */
    void setTooltip (const juce::String& t);
    void resized() override;
    int getIdealWidth (float fontHeight = 15.0f) const;

private:
    void build (const juce::StringArray& labels);
    juce::OwnedArray<juce::TextButton> buttons;
    int selected = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::RangedAudioParameter* param = nullptr;
};

/** Toggle button (button-like) attached to a bool parameter. */
class ChipToggle : public juce::TextButton
{
public:
    ChipToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& text);

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

/** Switch-style toggle attached to a bool parameter. */
class SwitchToggle : public juce::ToggleButton
{
public:
    SwitchToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& text);

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};
} // namespace schimmer
