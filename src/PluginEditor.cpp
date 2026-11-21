#include "PluginEditor.h"

namespace schimmer
{
SchimmerEditor::SchimmerEditor (SchimmerProcessor& p) : AudioProcessorEditor (p), plugin (p), view (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    const int w = MainView::kDesignW, h = MainView::kDesignH;
    setResizeLimits (w * 6 / 10, h * 6 / 10, w * 3 / 2, h * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) w / (double) h);
    setSize (w * 9 / 10, h * 9 / 10);
}

SchimmerEditor::~SchimmerEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SchimmerEditor::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void SchimmerEditor::resized()
{
    const float s = (float) getWidth() / (float) MainView::kDesignW;
    view.setTransform (juce::AffineTransform::scale (s));
    view.setBounds (0, 0, MainView::kDesignW, MainView::kDesignH);
}
} // namespace schimmer
