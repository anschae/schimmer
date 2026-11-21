#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace schimmer::theme
{
inline const juce::Colour bg        { 0xff0d0f16 };
inline const juce::Colour header    { 0xff11131c };
inline const juce::Colour panel     { 0xff151824 };
inline const juce::Colour panelEdge { 0xff252a3a };
inline const juce::Colour plot      { 0xff0f121b };
inline const juce::Colour control   { 0xff1c2030 };
inline const juce::Colour controlOn { 0xff2a3047 };
inline const juce::Colour gridLine  { 0xff222838 };
inline const juce::Colour text      { 0xffe8e9f2 };
inline const juce::Colour textDim   { 0xff8b91a8 };
inline const juce::Colour textFaint { 0xff5a6078 };
inline const juce::Colour straight  { 0xff7fb8ff };  // reverb blue (default accent)
inline const juce::Colour triplet   { 0xffff8a3d };  // orange
inline const juce::Colour hostBar   { 0xff3ed68a };  // green
inline const juce::Colour accent    { 0xffb69cff };  // violet: shimmer, randomize
inline const juce::Colour duck      { 0xff4cd68a };
inline const juce::Colour warn      { 0xfff2c94c };
inline const juce::Colour reverb    { 0xff7fb8ff };
inline const juce::Colour shimmer   { 0xffc9a8ff };
inline const juce::Colour grains    { 0xffffc58a };
inline const juce::Colour delayL    { 0xff2ec4d6 };
inline const juce::Colour delayR    { 0xffff7eb6 };
inline const juce::Colour tone      { 0xff9fd6c8 };
inline const juce::Colour bandLow   { 0xffff9a62 };
inline const juce::Colour bandMid   { 0xff7fb8ff };
inline const juce::Colour bandHigh  { 0xffe3d4ff };

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
