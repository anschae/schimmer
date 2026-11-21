#include "MainView.h"
#include "PluginProcessor.h"
#include "model/Randomizer.h"

namespace schimmer
{
using namespace theme;

namespace
{
std::unique_ptr<juce::Component> makeControl (SchimmerProcessor& p, P prm, juce::Colour accentCol, const juce::String& strip)
{
    auto& apvts = p.getApvts();
    const auto& i = info (prm);
    juce::String caption (i.name);
    if (strip.isNotEmpty() && caption.startsWith (strip) && caption.length() > strip.length() + 2)
        caption = caption.substring (strip.length());
    if (i.unit == Unit::Bool)
    {
        struct Holder : juce::Component
        {
            Holder (juce::AudioProcessorValueTreeState& a, const char* id, const char* name, const char* tip, juce::Colour c)
                : chip (a, id, name)
            {
                chip.setTooltip (tip);
                chip.getProperties().set ("accent", c.toString());
                addAndMakeVisible (chip);
            }
            void resized() override { chip.setBounds (getLocalBounds().withSizeKeepingCentre (getWidth() - 8, 30)); }
            ChipToggle chip;
        };
        return std::make_unique<Holder> (apvts, i.id, caption.toRawUTF8(), i.tooltip, accentCol);
    }
    if (i.unit == Unit::Choice)
    {
        struct Holder : juce::Component
        {
            juce::ComboBox box;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
            juce::String caption;
            void resized() override { box.setBounds (getLocalBounds().withTrimmedTop (18).withSizeKeepingCentre (getWidth() - 8, 28)); }
            void paint (juce::Graphics& g) override
            {
                drawCaption (g, caption.toUpperCase(), getLocalBounds().removeFromTop (18).reduced (4, 0), textDim, 10.5f);
            }
        };
        auto h = std::make_unique<Holder>();
        h->caption = caption;
        h->box.addItemList (i.choices.isEmpty() ? noteNames() : i.choices, 1);
        h->box.setTooltip (juce::String (i.name) + ": " + i.tooltip);
        h->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, i.id, h->box);
        h->addAndMakeVisible (h->box);
        return h;
    }
    auto k = std::make_unique<Knob> (apvts, i.id, caption, accentCol);
    k->setTooltipText (juce::String (i.name) + ": " + i.tooltip);
    k->setValueFormatter ([prm] (float v) { return formatValue (prm, v); });
    return k;
}
} // namespace

// ============================================================================ ParamPanel
ParamPanel::ParamPanel (SchimmerProcessor& p, const juce::String& t, std::vector<P> params, juce::Colour accentCol, int cols, int rowHeight,
                        const juce::String& stripPrefix)
    : title (t), columns (cols), rowH (rowHeight)
{
    for (auto prm : params)
    {
        controls.push_back (makeControl (p, prm, accentCol, stripPrefix));
        ids.push_back (prm);
        addAndMakeVisible (*controls.back());
    }
}

void ParamPanel::setControlEnabled (P p, bool enabled)
{
    for (size_t k = 0; k < ids.size(); ++k)
        if (ids[k] == p && controls[k]->isEnabled() != enabled)
        {
            controls[k]->setEnabled (enabled);
            controls[k]->setAlpha (enabled ? 1.0f : 0.4f);
        }
}

void ParamPanel::paint (juce::Graphics& g)
{
    if (drawFrame)
        drawPanel (g, getLocalBounds().toFloat());
    if (title.isNotEmpty())
        drawCaption (g, title.toUpperCase(), getLocalBounds().reduced (12, 6).removeFromTop (18));
}

void ParamPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    if (title.isNotEmpty()) r.removeFromTop (20);
    const int rows = ((int) controls.size() + columns - 1) / columns;
    const int cw = r.getWidth() / columns;
    const int rh = rows > 0 ? std::min (rowH, r.getHeight() / rows) : 0;
    for (size_t k = 0; k < controls.size(); ++k)
    {
        const int col = (int) k % columns, row = (int) k / columns;
        controls[k]->setBounds (r.getX() + col * cw, r.getY() + row * rh, cw, rh);
    }
    freeArea = r.withTrimmedTop (rows * rh);
}

// ============================================================================ MainView
MainView::MainView (SchimmerProcessor& p)
    : proc (p), hub (p), presetBar (p), freezeButton (p.getApvts(), pid (P::freeze), "Freeze"),
      tailView (p, hub), decayView (p), stereoMeter (p, hub),
      modeSel (p.getApvts(), pid (P::mode)), colorSel (p.getApvts(), pid (P::color)),
      preSync (p.getApvts(), pid (P::preDelaySync), "Sync"),
      tabs (juce::StringArray { "Shimmer", "Grains", "Delay", "Tone & Stereo", "Ducking", "Freeze / Clear / Morph" }),
      duckDisplay (p)
{
    addAndMakeVisible (presetBar);
    for (auto* b : { &randomButton, &lockButton, &clearButton })
        addAndMakeVisible (b);
    randomButton.setTooltip ("Randomize all unlocked sections within musically sensible ranges (Mix is never changed).");
    randomButton.getProperties().set ("accent", accent.toString());
    randomButton.onClick = [this] {
        juce::Random rng;
        Randomizer::randomize (proc, proc.randomLocks, rng);
    };
    lockButton.setTooltip ("Choose which sections Randomize leaves alone.");
    lockButton.onClick = [this] { showLockMenu(); };
    clearButton.setTooltip ("Fade out and clear the reverb tail immediately. Automatable as the 'Clear Tail' parameter: "
                            "a rising edge (0 -> 1) triggers it.");
    clearButton.onClick = [this] { proc.triggerClearTail(); };
    addAndMakeVisible (freezeButton);
    freezeButton.setTooltip (info (P::freeze).tooltip);
    freezeButton.getProperties().set ("accent", shimmer.toString());
    addAndMakeVisible (qualityBox);
    qualityBox.addItemList (info (P::quality).choices, 1);
    qualityBox.setTooltip (info (P::quality).tooltip);
    qualityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getApvts(), pid (P::quality), qualityBox);

    addAndMakeVisible (tailView);
    addAndMakeVisible (decayView);
    addAndMakeVisible (stereoMeter);

    addAndMakeVisible (modeSel);
    modeSel.setTooltip (info (P::mode).tooltip);
    modeSel.setAccent (reverb);
    addAndMakeVisible (colorSel);
    colorSel.setTooltip (info (P::color).tooltip);
    colorSel.setAccent (tone);
    addAndMakeVisible (preSync);
    preSync.setTooltip (info (P::preDelaySync).tooltip);
    addAndMakeVisible (preNote);
    preNote.addItemList (noteNames(), 1);
    preNote.setTooltip (info (P::preDelayNote).tooltip);
    preNoteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getApvts(), pid (P::preDelayNote), preNote);

    for (auto prm : { P::size, P::decay, P::diffusion, P::bloom, P::preDelay, P::modRate, P::modDepth, P::modDrift, P::mix, P::output })
    {
        const auto c = prm == P::mix || prm == P::output ? text : reverb;
        auto k = std::make_unique<Knob> (p.getApvts(), pid (prm), info (prm).name, c);
        k->setTooltipText (juce::String (info (prm).name) + ": " + info (prm).tooltip);
        k->setValueFormatter ([prm] (float v) { return formatValue (prm, v); });
        addAndMakeVisible (*k);
        coreKnobs.push_back (std::move (k));
    }

    // ---- pages
    addAndMakeVisible (tabs);
    tabs.onChange = [this] (int i) { showPage (i); };
    auto add = [this, &p] (std::vector<P> ps, juce::Colour c, int cols, const char* strip = "") {
        auto panel = std::make_unique<ParamPanel> (p, "", std::move (ps), c, cols, 104, strip);
        addChildComponent (*panel);
        pages.push_back (std::move (panel));
    };
    add ({ P::shimMode, P::pitch, P::fine, P::shimAmount, P::shimSmooth, P::shimPlace, P::pitchSnap, P::pitch2, P::voiceMix,
           P::shimLowCut, P::shimHighCut }, shimmer, 6);
    add ({ P::grainOn, P::grainSource, P::grainDest, P::grainSize, P::grainDensity, P::grainAmount, P::grainPitch, P::grainQuant,
           P::grainPitchJit, P::grainSpray, P::grainReverse, P::grainScatter }, grains, 6, "Grain ");
    add ({ P::delayOn, P::delaySync, P::delayNote, P::delayTime, P::delayFeedback, P::delayToReverb, P::delayPingPong,
           P::delayOffset, P::delayHP, P::delayLP, P::delayMod, P::delayLevel }, delayL, 6);
    add ({ P::lowCut, P::highCut, P::lowMult, P::highMult, P::lowXover, P::highXover, P::warmth, P::inHP, P::inLP, P::tilt,
           P::width, P::bassMono }, tone, 6);
    add ({ P::duckOn, P::duckSource, P::duckTarget, P::duckPos, P::duckDetMode, P::duckFull, P::duckThresh, P::duckDepth,
           P::duckKnee, P::duckAttack, P::duckHold, P::duckRelease, P::duckRelSync, P::duckRelNote, P::duckLook, P::duckHP,
           P::duckLP, P::duckRms }, duck, 9, "Duck ");
    add ({ P::freezeMode, P::clearTarget, P::autoClear, P::mixLock, P::morphOn, P::morph }, accent, 6);
    addChildComponent (duckDisplay);
    for (auto* b : { &storeA, &storeB })
    {
        addChildComponent (b);
        b->getProperties().set ("accent", accent.toString());
    }
    storeA.setTooltip ("Store the current settings as morph snapshot A (Morph 0 %).");
    storeB.setTooltip ("Store the current settings as morph snapshot B (Morph 100 %).");
    storeA.onClick = [this] { proc.setMorphSnapshot (0, proc.captureSnapshot()); repaint(); };
    storeB.onClick = [this] { proc.setMorphSnapshot (1, proc.captureSnapshot()); repaint(); };

    hub.onUpdate = [this] { tailView.repaint(); stereoMeter.repaint(); };
    tabs.setSelected (0);
    showPage (0);
    startTimerHz (10);
}

MainView::~MainView() { stopTimer(); }

void MainView::timerCallback()
{
    // parameter changes from the host / automation: refresh the static displays
    decayView.repaint();
    tailView.repaint();
    const bool synced = proc.value (P::preDelaySync) > 0.5f;
    preNote.setEnabled (synced);
    coreKnobs[4]->setEnabled (! synced);
    coreKnobs[4]->setAlpha (synced ? 0.4f : 1.0f);
    auto panel = [this] (int i) { return dynamic_cast<ParamPanel*> (pages[(size_t) i].get()); };
    const bool dSync = proc.value (P::delaySync) > 0.5f;
    panel (2)->setControlEnabled (P::delayTime, ! dSync);
    panel (2)->setControlEnabled (P::delayNote, dSync);
    const auto mode = juce::roundToInt (proc.value (P::shimMode));
    panel (0)->setControlEnabled (P::pitch2, mode == 5);
    panel (0)->setControlEnabled (P::voiceMix, mode == 5);
    const bool relSync = proc.value (P::duckRelSync) > 0.5f;
    panel (4)->setControlEnabled (P::duckRelease, ! relSync);
    panel (4)->setControlEnabled (P::duckRelNote, relSync);
    panel (4)->setControlEnabled (P::duckRms, juce::roundToInt (proc.value (P::duckDetMode)) == 1);
}

void MainView::showPage (int index)
{
    currentPage = index;
    for (int i = 0; i < (int) pages.size(); ++i)
        pages[(size_t) i]->setVisible (i == index);
    duckDisplay.setVisible (index == 4);
    storeA.setVisible (index == 5);
    storeB.setVisible (index == 5);
    resized();
    repaint();
}

void MainView::showLockMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Randomize leaves these alone:");
    for (int s = 0; s < kNumSections; ++s)
        m.addItem (1 + s, kSections[s], true, (proc.randomLocks >> s) & 1u);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&lockButton), [this] (int r) {
        if (r >= 1 && r <= kNumSections)
        {
            proc.randomLocks ^= 1u << (r - 1);
            showLockMenu();   // keep the menu open for more toggles
        }
    });
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    auto head = getLocalBounds().removeFromTop (56);
    g.setGradientFill (juce::ColourGradient (header, 0.0f, 0.0f, bg, 0.0f, 56.0f, false));
    g.fillRect (head);
    g.setColour (text);
    g.setFont (font (25.0f, true).withExtraKerningFactor (0.22f));
    g.drawText ("SCHIMMER", head.reduced (22, 0), juce::Justification::centredLeft);
    g.setColour (shimmer.withAlpha (0.6f));
    g.setFont (font (11.0f).withExtraKerningFactor (0.15f));
    g.drawText ("anschae", head.reduced (24, 0).withTrimmedLeft (175), juce::Justification::centredLeft);

    // core row frame
    drawPanel (g, juce::Rectangle<float> (12.0f, 372.0f, 1256.0f, 164.0f));
    drawCaption (g, "MODE", { 26, 378, 200, 16 });
    drawCaption (g, "COLOR", { 26, 438, 200, 16 });
    drawCaption (g, "PRE-DELAY SYNC", { 26, 486, 200, 16 });
    // page frame
    drawPanel (g, juce::Rectangle<float> (12.0f, 578.0f, 1256.0f, 232.0f));
    if (currentPage == 5)
    {
        g.setColour (textDim);
        g.setFont (font (12.0f));
        auto r = juce::Rectangle<int> (24, 720, 1220, 70);
        juce::String sA = proc.hasMorphSnapshot (0) ? "A stored" : "A empty", sB = proc.hasMorphSnapshot (1) ? "B stored" : "B empty";
        g.drawFittedText ("Morph: store two snapshots, switch Morph On and move Morph (automatable) to glide between them. "
                          "Choices and switches flip at 50 %. Mix stays put when Mix Lock is on.   [" + sA + " / " + sB + "]",
                          r.withTrimmedLeft (260), juce::Justification::centredLeft, 3);
    }
}

void MainView::resized()
{
    auto head = juce::Rectangle<int> (0, 0, kDesignW, 56).reduced (16, 12);
    head.removeFromLeft (250);
    presetBar.setBounds (head.removeFromLeft (380));
    head.removeFromLeft (12);
    randomButton.setBounds (head.removeFromLeft (100));
    head.removeFromLeft (4);
    lockButton.setBounds (head.removeFromLeft (64));
    clearButton.setBounds (head.removeFromRight (110));
    head.removeFromRight (8);
    freezeButton.setBounds (head.removeFromRight (90));
    head.removeFromRight (8);
    qualityBox.setBounds (head.removeFromRight (90));

    tailView.setBounds (12, 64, 820, 300);
    decayView.setBounds (844, 64, 424, 180);
    stereoMeter.setBounds (844, 252, 424, 112);

    modeSel.setBounds (22, 396, 222, 32);
    colorSel.setBounds (22, 456, 222, 26);
    preSync.setBounds (22, 504, 70, 26);
    preNote.setBounds (100, 504, 144, 26);
    const int x0 = 256, w = (1260 - x0) / (int) coreKnobs.size();
    for (size_t k = 0; k < coreKnobs.size(); ++k)
        coreKnobs[k]->setBounds (x0 + (int) k * w, 384, w, 140);

    tabs.setBounds (12, 544, 860, 28);
    auto pageArea = juce::Rectangle<int> (16, 584, 1248, 222);
    for (auto& pg : pages)
        pg->setBounds (pageArea);
    if (currentPage == 4)
    {
        pages[4]->setBounds (pageArea.withWidth (900));
        duckDisplay.setBounds (pageArea.withTrimmedLeft (910).reduced (4, 8));
    }
    if (currentPage == 5)
    {
        pages[5]->setBounds (pageArea.withHeight (120));
        storeA.setBounds (28, 740, 100, 30);
        storeB.setBounds (136, 740, 100, 30);
    }
    for (auto& pg : pages)
        if (auto* pp = dynamic_cast<ParamPanel*> (pg.get())) pp->drawFrame = false;
}
} // namespace schimmer
