#include "DuckingDisplay.h"
#include "PluginProcessor.h"

namespace schimmer
{
using namespace theme;

DuckingDisplay::DuckingDisplay (SchimmerProcessor& p) : proc (p)
{
    history.assign (kHistory, { -120.0f, 0.0f });
    scratch.resize (2048);
    // discard what queued up while no editor was open
    proc.getEngine().getDucker().readFrames (scratch.data(), (int) scratch.size());
    setTooltip ("Grey: detector level. Yellow: threshold. Green: gain of the ducked signal.");
    startTimerHz (30);
}

void DuckingDisplay::timerCallback()
{
    const int n = proc.getEngine().getDucker().readFrames (scratch.data(), (int) scratch.size());
    for (int i = 0; i < n; ++i)
    {
        history[(size_t) writePos] = scratch[(size_t) i];
        writePos = (writePos + 1) % kHistory;
    }
    if (n > 0 || isMouseOver())
        repaint();
}

float DuckingDisplay::dbToY (float db, juce::Rectangle<float> r) const
{
    const float t = juce::jlimit (0.0f, 1.0f, -db / 60.0f);  // 0 dB top, -60 dB bottom
    return r.getY() + t * r.getHeight();
}

void DuckingDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (plot);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    auto r = bounds.reduced (2.0f, 6.0f);
    const bool on = proc.value (P::duckOn) > 0.5f;
    const float thresh = proc.value (P::duckThresh);

    // grid
    g.setColour (gridLine.withAlpha (0.6f));
    for (float db : { -12.0f, -24.0f, -36.0f, -48.0f })
        g.drawHorizontalLine (juce::roundToInt (dbToY (db, r)), r.getX(), r.getRight());

    const float dx = r.getWidth() / (float) (kHistory - 1);
    // detector level (filled)
    juce::Path level;
    level.startNewSubPath (r.getX(), r.getBottom());
    for (int i = 0; i < kHistory; ++i)
    {
        const auto& f = history[(size_t) ((writePos + i) % kHistory)];
        level.lineTo (r.getX() + i * dx, dbToY (f.levelDb, r));
    }
    level.lineTo (r.getRight(), r.getBottom());
    level.closeSubPath();
    g.setColour (textDim.withAlpha (on ? 0.28f : 0.12f));
    g.fillPath (level);
    g.setColour (textDim.withAlpha (on ? 0.5f : 0.2f));
    g.strokePath (level, juce::PathStrokeType (1.0f));

    // threshold
    const float ty = dbToY (thresh, r);
    const float dash[] = { 5.0f, 4.0f };
    g.setColour (warn.withAlpha (on ? 0.85f : 0.3f));
    g.drawDashedLine ({ r.getX(), ty, r.getRight(), ty }, dash, 2, 1.2f);
    g.setFont (font (11.5f));
    g.drawText ("threshold " + juce::String (juce::roundToInt (thresh)) + " dB",
                juce::Rectangle<float> (r.getX() + 6.0f, ty - 16.0f, 140.0f, 14.0f), juce::Justification::centredLeft);

    // gain-reduction curve (gain of the ducked signal, same dB scale)
    juce::Path gr;
    for (int i = 0; i < kHistory; ++i)
    {
        const auto& f = history[(size_t) ((writePos + i) % kHistory)];
        const float y = dbToY (f.grDb, r);
        if (i == 0) gr.startNewSubPath (r.getX(), y);
        else gr.lineTo (r.getX() + i * dx, y);
    }
    g.setColour (duck.withAlpha (on ? 1.0f : 0.3f));
    g.strokePath (gr, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // GR readout
    const float now = proc.getEngine().getDucker().meterGrDb.load();
    const juce::String txt = ! on ? juce::String ("Ducking off")
                                  : "GR " + (now <= dsp::Ducker::kFloorDb + 0.5f ? juce::String ("-inf") : juce::String (now, 1)) + " dB";
    g.setFont (font (12.5f, true));
    g.setColour (on ? duck : textFaint);
    g.drawText (txt, bounds.reduced (8.0f, 4.0f).removeFromTop (16.0f), juce::Justification::centredLeft);
}
} // namespace schimmer
