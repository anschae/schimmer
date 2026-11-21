#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace schimmer::theme
{
inline const juce::Colour bg        { 0xff12151b };
inline const juce::Colour header    { 0xff161a21 };
inline const juce::Colour panel     { 0xff181c24 };
inline const juce::Colour panelEdge { 0xff262c37 };
inline const juce::Colour plot      { 0xff141820 };
inline const juce::Colour control   { 0xff1f242e };
inline const juce::Colour controlOn { 0xff2c3442 };
inline const juce::Colour gridLine  { 0xff262c37 };
inline const juce::Colour text      { 0xffe6e9ef };
inline const juce::Colour textDim   { 0xff8a93a3 };
inline const juce::Colour textFaint { 0xff5b6372 };
inline const juce::Colour straight  { 0xff2ec4d6 };  // cyan
inline const juce::Colour triplet   { 0xffff8a3d };  // orange
inline const juce::Colour hostBar   { 0xff3ed68a };  // green: host bar markers, LEDs
inline const juce::Colour accent    { 0xffa58bff };  // purple: randomize
inline const juce::Colour duck      { 0xff4cd68a };
inline const juce::Colour warn      { 0xfff2c94c };

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

/** Spaced-out uppercase section caption, e.g. "VOLUME". */
inline void drawCaption (juce::Graphics& g, const juce::String& textStr, juce::Rectangle<int> area,
                         juce::Colour c = textDim, float height = 11.5f)
{
    g.setColour (c);
    g.setFont (font (height, true).withExtraKerningFactor (0.18f));
    g.drawText (textStr, area, juce::Justification::centredLeft, false);
}

inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 8.0f)
{
    g.setColour (panel);
    g.fillRoundedRectangle (r, radius);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}
} // namespace schimmer::theme
