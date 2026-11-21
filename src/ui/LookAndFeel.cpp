#include "LookAndFeel.h"

namespace schimmer
{
using namespace theme;

SchLookAndFeel::SchLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::buttonOnColourId, controlOn);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c212a));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, controlOn);
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0e1116));
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, straight);
    setColour (juce::TextEditor::highlightColourId, straight.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, straight);
    setColour (juce::Slider::rotarySliderFillColourId, straight);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0e1116));
    setColour (juce::Slider::textBoxOutlineColourId, panelEdge);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff232935));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, panelEdge);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, panelEdge);
    setColour (juce::ScrollBar::thumbColourId, controlOn);
    setColour (juce::ListBox::backgroundColourId, panel);
}

void SchLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                      juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float trackW = std::max (2.5f, radius * 0.11f);
    const float arcR = radius - trackW * 0.5f;
    const auto fill = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float angle = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (control.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar parameters (range symmetric around 0) draw from the centre
    const bool bipolar = s.getMinimum() < 0.0 && std::abs (s.getMinimum() + s.getMaximum()) < 1.0e-6;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (s.isEnabled() ? fill : fill.withSaturation (0.0f).withAlpha (0.4f));
        g.strokePath (val, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = arcR - trackW * 1.3f;
    juce::ColourGradient grad (juce::Colour (0xff2a303b), c.x, c.y - bodyR, juce::Colour (0xff171b22), c.x, c.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c), 1.0f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-1.25f, -bodyR * 0.82f, 2.5f, bodyR * 0.45f, 1.0f);
    g.setColour (text);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c));
}

void SchLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 2.0f, (float) w, 4.0f);
    g.setColour (control.brighter (0.15f));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.fillRoundedRectangle (r.withRight (pos), 2.0f);
    g.setColour (text);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ pos, r.getCentreY() }));
}

void SchLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float cr = 6.0f;
    const bool cl = b.isConnectedOnLeft(), cr2 = b.isConnectedOnRight();

    juce::Colour c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (down)
        c = c.brighter (0.12f);
    else if (over)
        c = c.brighter (0.06f);
    if (! b.isEnabled())
        c = c.withMultipliedAlpha (0.5f);

    juce::Path p;
    p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), cr, cr, ! cl, ! cr2, ! cl, ! cr2);
    g.setColour (c);
    g.fillPath (p);

    const auto accentCol = b.getProperties()["accent"];
    if (b.getToggleState() && ! accentCol.isVoid())
    {
        const auto a = juce::Colour::fromString (accentCol.toString());
        g.setColour (a.withAlpha (0.18f));
        g.fillPath (p);
        g.setColour (a.withAlpha (0.7f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    else
    {
        g.setColour (panelEdge);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
}

void SchLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto accentCol = b.getProperties()["accent"];
    juce::Colour c = b.getToggleState() ? b.findColour (juce::TextButton::textColourOnId)
                                        : b.findColour (juce::TextButton::textColourOffId);
    if (b.getToggleState() && ! accentCol.isVoid())
        c = juce::Colour::fromString (accentCol.toString()).brighter (0.3f);
    if (! b.isEnabled())
        c = c.withMultipliedAlpha (0.45f);
    g.setColour (c);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
}

void SchLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const float h = std::min (18.0f, r.getHeight() - 4.0f);
    auto pill = juce::Rectangle<float> (0.0f, 0.0f, h * 1.8f, h).withCentre ({ r.getX() + h * 0.9f + 2.0f, r.getCentreY() });
    const bool on = b.getToggleState();
    const auto accentCol = b.getProperties()["accent"];
    const auto a = accentCol.isVoid() ? straight : juce::Colour::fromString (accentCol.toString());
    g.setColour (on ? a : control.brighter (over ? 0.25f : 0.15f));
    g.fillRoundedRectangle (pill, h * 0.5f);
    const float k = h - 4.0f;
    const float kx = on ? pill.getRight() - k - 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? juce::Colours::white : textDim);
    g.fillEllipse (kx, pill.getY() + 2.0f, k, k);

    g.setColour (b.isEnabled() ? text : textFaint);
    g.setFont (font (13.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (pill.getRight() + 8.0f), juce::Justification::centredLeft);
}

void SchLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (textDim);
    g.fillPath (arrow);
}

void SchLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void SchLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
    g.setColour (juce::Colour (0xff232935));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (text);
    g.setFont (font (13.0f));
    g.drawFittedText (t, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> SchLookAndFeel::getTooltipBounds (const juce::String& t, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (t, font (13.0f));
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 320.0f);
    const int w = (int) tl.getWidth() + 18, h = (int) tl.getHeight() + 10;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 18, w, h)
        .constrainedWithin (parent);
}

void SchLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (juce::Colour (0xff1c212a));
    g.fillPath (path);
    g.setColour (panelEdge);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}
} // namespace schimmer
