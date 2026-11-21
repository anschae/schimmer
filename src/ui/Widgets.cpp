#include "Widgets.h"

namespace schimmer
{
// ============================================================================ Knob
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& cap, juce::Colour c)
    : caption (cap)
{
    param = apvts.getParameter (paramId);
    jassert (param != nullptr);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.onDouble = [this] { showEditor(); };
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    slider.addListener (this);
    addAndMakeVisible (slider);
    setColour (c);
    if (param != nullptr)
        slider.setTooltip (param->getName (64));
}

Knob::Knob (const juce::String& cap, juce::Colour c, std::function<float()> get, std::function<void (float)> set)
    : caption (cap), getter (std::move (get)), setter (std::move (set))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setRange (0.0, 1.0, 0.01);
    slider.setMouseDragSensitivity (220);
    slider.setValue (getter(), juce::dontSendNotification);
    slider.onValueChange = [this] { if (setter) setter ((float) slider.getValue()); };
    slider.onDouble = [this] { showEditor(); };
    slider.addListener (this);
    addAndMakeVisible (slider);
    setColour (c);
}

void Knob::setColour (juce::Colour c)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.repaint();
}

juce::String Knob::valueText() const
{
    if (formatter)
        return formatter ((float) slider.getValue());
    if (param != nullptr)
        return param->getCurrentValueAsText();
    return juce::String (juce::roundToInt (slider.getValue() * 100.0)) + " %";
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int textH = 34;
    auto knobArea = r.withTrimmedBottom (textH);
    const int s = std::min (knobArea.getWidth(), knobArea.getHeight());
    slider.setBounds (knobArea.withSizeKeepingCentre (s, s));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto textArea = r.removeFromBottom (34).expanded (4, 0);
    const bool narrow = getWidth() < 70;
    g.setColour (theme::text);
    g.setFont (theme::font (narrow ? 13.0f : 14.5f));
    g.drawFittedText (valueText(), textArea.removeFromTop (17), juce::Justification::centred, 1, 0.75f);
    g.setColour (theme::textDim);
    g.setFont (theme::font (narrow ? 9.5f : 10.5f, true).withExtraKerningFactor (narrow ? 0.06f : 0.14f));
    g.drawFittedText (caption.toUpperCase(), textArea, juce::Justification::centredTop, 1, 0.7f);
}

void Knob::mouseDoubleClick (const juce::MouseEvent&) { showEditor(); }

void Knob::showEditor()
{
    editor = std::make_unique<juce::TextEditor>();
    editor->setJustification (juce::Justification::centred);
    editor->setFont (theme::font (14.0f));
    editor->setText (valueText().upToFirstOccurrenceOf (" ", false, false), false);
    editor->selectAll();
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds().removeFromBottom (34).removeFromTop (19).reduced (2, 0));
    editor->grabKeyboardFocus();

    auto commit = [this] {
        if (editor == nullptr)
            return;
        const auto t = editor->getText().trim();
        if (t.isNotEmpty())
        {
            if (param != nullptr)
            {
                const float norm = param->getValueForText (t);
                param->beginChangeGesture();
                param->setValueNotifyingHost (norm);
                param->endChangeGesture();
            }
            else if (setter)
            {
                const float v = juce::jlimit (0.0f, 1.0f, t.getFloatValue() / 100.0f);
                slider.setValue (v, juce::sendNotificationSync);
            }
        }
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Knob> (this)] {
            if (sp != nullptr)
                sp->editor.reset();
        });
    };
    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    editor->onEscapeKey = [this] {
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Knob> (this)] {
            if (sp != nullptr)
                sp->editor.reset();
        });
    };
}

// ============================================================================ Segmented
Segmented::Segmented (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, juce::StringArray labels)
{
    param = apvts.getParameter (paramId);
    jassert (param != nullptr);
    if (labels.isEmpty())
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
            labels = choice->choices;
    build (labels);
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) {
        setSelected (juce::roundToInt (v));
    });
    attachment->sendInitialUpdate();
    if (param != nullptr)
        setTooltip (param->getName (64));
}

Segmented::Segmented (juce::StringArray labels) { build (labels); }

void Segmented::build (const juce::StringArray& labels)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        int flags = 0;
        if (i > 0) flags |= juce::Button::ConnectedOnLeft;
        if (i < labels.size() - 1) flags |= juce::Button::ConnectedOnRight;
        b->setConnectedEdges (flags);
        b->onClick = [this, i] {
            if (attachment != nullptr)
                attachment->setValueAsCompleteGesture ((float) i);
            else
                setSelected (i);
            if (onChange)
                onChange (i);
        };
        addAndMakeVisible (b);
    }
}

void Segmented::setSelected (int index)
{
    selected = index;
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

void Segmented::setAccent (juce::Colour c)
{
    for (auto* b : buttons)
        b->getProperties().set ("accent", c.toString());
    repaint();
}

void Segmented::setItemAccent (int index, juce::Colour c)
{
    if (auto* b = buttons[index])
        b->getProperties().set ("accent", c.toString());
}

void Segmented::setItemEnabled (int index, bool enabled)
{
    if (auto* b = buttons[index])
        b->setEnabled (enabled);
}

void Segmented::setItemTooltip (int index, const juce::String& t)
{
    if (auto* b = buttons[index])
    {
        b->setTooltip (t);
        b->getProperties().set ("sharedTip", false);
    }
}

void Segmented::setTooltip (const juce::String& t)
{
    for (auto* b : buttons)
        if (b->getTooltip().isEmpty() || b->getProperties()["sharedTip"])
        {
            b->setTooltip (t);
            b->getProperties().set ("sharedTip", true);
        }
}

int Segmented::getIdealWidth (float fontHeight) const
{
    int w = 0;
    for (auto* b : buttons)
        w += juce::GlyphArrangement::getStringWidthInt (theme::font (fontHeight), b->getButtonText()) + 22;
    return w;
}

void Segmented::resized()
{
    // widths proportional to the text
    std::vector<int> widths;
    int total = 0;
    for (auto* b : buttons)
    {
        const int w = juce::GlyphArrangement::getStringWidthInt (theme::font (15.0f), b->getButtonText()) + 22;
        widths.push_back (w);
        total += w;
    }
    const float scale = total > 0 ? (float) getWidth() / (float) total : 1.0f;
    float x = 0.0f;
    for (int i = 0; i < buttons.size(); ++i)
    {
        const float w = widths[(size_t) i] * scale;
        buttons[i]->setBounds (juce::roundToInt (x), 0, juce::roundToInt (x + w) - juce::roundToInt (x), getHeight());
        x += w;
    }
}

// ============================================================================ toggles
ChipToggle::ChipToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& t)
    : juce::TextButton (t), attachment (apvts, paramId, *this)
{
    setClickingTogglesState (true);
    if (auto* p = apvts.getParameter (paramId))
        setTooltip (p->getName (64));
}

SwitchToggle::SwitchToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& t)
    : juce::ToggleButton (t), attachment (apvts, paramId, *this)
{
    if (auto* p = apvts.getParameter (paramId))
        setTooltip (p->getName (64));
}
} // namespace schimmer
