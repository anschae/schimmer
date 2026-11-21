#pragma once
#include "Widgets.h"

namespace schimmer
{
class SchimmerProcessor;

/** Preset browser: prev / name (menu: load, save, delete, folder) / next. */
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (SchimmerProcessor& p);
    ~PresetBar() override;
    void resized() override;

private:
    void timerCallback() override;
    void showMenu();
    void askSaveName();
    void refresh();

    SchimmerProcessor& proc;
    juce::TextButton prev { juce::CharPointer_UTF8 ("\xe2\x97\x82") }, next { juce::CharPointer_UTF8 ("\xe2\x96\xb8") }, name;
};
} // namespace schimmer
