#include "MainView.h"
#include "PluginProcessor.h"

namespace schimmer
{
using namespace theme;

ParamPanel::ParamPanel (SchimmerProcessor& p, const juce::String& t, std::vector<P> params, juce::Colour accent, int cols)
    : title (t), columns (cols)
{
    auto& apvts = p.getApvts();
    for (auto prm : params)
    {
        const auto& i = info (prm);
        std::unique_ptr<juce::Component> c;
        if (i.unit == Unit::Bool)
        {
            auto chip = std::make_unique<ChipToggle> (apvts, i.id, i.name);
            chip->setTooltip (i.tooltip);
            c = std::move (chip);
        }
        else if (i.unit == Unit::Choice)
        {
            auto box = std::make_unique<juce::ComboBox>();
            const auto& names = i.choices.isEmpty() ? noteNames() : i.choices;
            box->addItemList (names, 1);
            box->setTooltip (juce::String (i.name) + ": " + i.tooltip);
            auto* raw = box.get();
            struct Holder : juce::Component
            {
                std::unique_ptr<juce::ComboBox> box;
                std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
                juce::String caption;
                void resized() override { box->setBounds (getLocalBounds().withTrimmedTop (16).withSizeKeepingCentre (getWidth() - 6, 26)); }
                void paint (juce::Graphics& g) override { drawCaption (g, caption.toUpperCase(), getLocalBounds().removeFromTop (16), textDim, 10.5f); }
            };
            auto h = std::make_unique<Holder>();
            h->caption = i.name;
            h->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, i.id, *raw);
            h->box = std::move (box);
            h->addAndMakeVisible (*raw);
            c = std::move (h);
        }
        else
        {
            auto k = std::make_unique<Knob> (apvts, i.id, i.name, accent);
            k->setTooltipText (i.tooltip);
            k->setValueFormatter ([prm] (float v) { return formatValue (prm, v); });
            c = std::move (k);
        }
        addAndMakeVisible (*c);
        controls.push_back (std::move (c));
    }
}

void ParamPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawCaption (g, title.toUpperCase(), getLocalBounds().reduced (12, 6).removeFromTop (18));
}

void ParamPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    r.removeFromTop (20);
    const int rows = ((int) controls.size() + columns - 1) / columns;
    const int cw = r.getWidth() / columns, rh = rows > 0 ? std::min (92, r.getHeight() / rows) : 0;
    for (size_t k = 0; k < controls.size(); ++k)
    {
        const int col = (int) k % columns, row = (int) k / columns;
        controls[k]->setBounds (r.getX() + col * cw, r.getY() + row * rh, cw, rh);
    }
}

MainView::MainView (SchimmerProcessor& p) : proc (p)
{
    addAndMakeVisible (presetBox);
    presetBox.setTooltip ("Factory and user presets.");
    presetBox.onChange = [this] {
        const auto all = proc.getPresets().list();
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx < (int) all.size())
            proc.getPresets().load (all[(size_t) idx]);
    };
    refreshPresets();
    addAndMakeVisible (clearButton);
    clearButton.setTooltip ("Fade out and clear the reverb tail immediately (also automatable as 'Clear Tail').");
    clearButton.onClick = [this] { proc.triggerClearTail(); };

    const auto cCore = juce::Colour (0xff7fb8ff), cTone = juce::Colour (0xff9fd6c8), cShim = juce::Colour (0xffd9b2ff),
               cGrain = juce::Colour (0xffffc58a), cDelay = juce::Colour (0xff2ec4d6), cOut = juce::Colour (0xffe6e9ef),
               cDuck = duck;
    auto add = [this, &p] (const char* t, std::vector<P> ps, juce::Colour c, int cols) {
        panels.push_back (std::make_unique<ParamPanel> (p, t, std::move (ps), c, cols));
        addAndMakeVisible (*panels.back());
    };
    add ("Reverb", { P::mode, P::size, P::decay, P::diffusion, P::bloom, P::preDelay, P::preDelaySync, P::preDelayNote,
                     P::modRate, P::modDepth, P::modDrift, P::freeze, P::freezeMode, P::quality }, cCore, 7);
    add ("Tone", { P::lowCut, P::highCut, P::lowMult, P::highMult, P::lowXover, P::highXover, P::color, P::warmth }, cTone, 4);
    add ("Shimmer", { P::shimMode, P::pitch, P::pitchSnap, P::fine, P::shimAmount, P::pitch2, P::voiceMix, P::shimLowCut,
                      P::shimHighCut, P::shimSmooth, P::shimPlace }, cShim, 6);
    add ("Grains", { P::grainOn, P::grainSource, P::grainDest, P::grainSize, P::grainDensity, P::grainPitch, P::grainQuant,
                     P::grainSpray, P::grainPitchJit, P::grainReverse, P::grainScatter, P::grainAmount }, cGrain, 6);
    add ("Delay", { P::delayOn, P::delayTime, P::delaySync, P::delayNote, P::delayFeedback, P::delayPingPong, P::delayOffset,
                    P::delayHP, P::delayLP, P::delayMod, P::delayToReverb, P::delayLevel }, cDelay, 6);
    add ("Output", { P::inHP, P::inLP, P::tilt, P::width, P::bassMono, P::mix, P::mixLock, P::output, P::clearTarget, P::autoClear,
                     P::morphOn, P::morph }, cOut, 6);
    add ("Ducking", { P::duckOn, P::duckSource, P::duckThresh, P::duckDepth, P::duckFull, P::duckKnee, P::duckAttack, P::duckHold,
                      P::duckRelease, P::duckRelSync, P::duckRelNote, P::duckLook, P::duckTarget, P::duckPos, P::duckHP,
                      P::duckLP, P::duckDetMode, P::duckRms }, cDuck, 9);
}

MainView::~MainView() = default;

void MainView::refreshPresets()
{
    presetBox.clear (juce::dontSendNotification);
    int id = 1, sel = 0;
    for (const auto& e : proc.getPresets().list())
    {
        if (e.displayName() == proc.getCurrentPresetName()) sel = id;
        presetBox.addItem (e.category + " / " + e.name, id++);
    }
    presetBox.setSelectedId (sel, juce::dontSendNotification);
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    auto head = getLocalBounds().removeFromTop (52);
    g.setColour (header);
    g.fillRect (head);
    g.setColour (text);
    g.setFont (font (24.0f, true).withExtraKerningFactor (0.12f));
    g.drawText ("SCHIMMER", head.reduced (20, 0), juce::Justification::centredLeft);
}

void MainView::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (52).reduced (16, 10);
    head.removeFromLeft (200);
    presetBox.setBounds (head.removeFromLeft (360));
    clearButton.setBounds (head.removeFromRight (120));
    r.reduce (10, 8);
    auto row1 = r.removeFromTop (205);
    panels[0]->setBounds (row1.removeFromLeft (r.getWidth() * 62 / 100).reduced (4));
    panels[1]->setBounds (row1.reduced (4));
    auto row2 = r.removeFromTop (205);
    panels[2]->setBounds (row2.removeFromLeft (r.getWidth() / 2).reduced (4));
    panels[3]->setBounds (row2.reduced (4));
    auto row3 = r.removeFromTop (205);
    panels[4]->setBounds (row3.removeFromLeft (r.getWidth() / 2).reduced (4));
    panels[5]->setBounds (row3.reduced (4));
    panels[6]->setBounds (r.reduced (4));
}
} // namespace schimmer
