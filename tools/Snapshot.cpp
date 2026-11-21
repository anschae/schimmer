// Developer tool: renders the plugin editor to PNG files, without a DAW.
//
//   SchimmerSnapshot <output-folder> [factory-preset-name]
//
// Build with -DSCH_BUILD_TOOLS=ON. On Linux without a display, run it under xvfb-run.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
void pump (int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

bool save (juce::Component& c, const juce::File& f, float scale = 1.0f)
{
    const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    f.deleteFile();
    juce::FileOutputStream s (f);
    juce::PNGImageFormat png;
    const bool ok = s.openedOk() && png.writeImageToStream (img, s);
    std::cout << (ok ? "wrote " : "FAILED ") << f.getFullPathName() << std::endl;
    return ok;
}

struct FakePlayHead : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (96.0);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setIsPlaying (true);
        return p;
    }
};
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc < 2)
    {
        std::cout << "usage: SchimmerSnapshot <output-folder> [factory-preset]" << std::endl;
        return 1;
    }
    const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    out.createDirectory();

    schimmer::SchimmerProcessor proc;
    const juce::String presetName = argc > 2 ? juce::String (argv[2]) : juce::String ("Cathedral of Light");
    for (const auto& e : proc.getPresets().list())
        if (e.name == presetName)
            proc.getPresets().load (e);

    FakePlayHead ph;
    proc.setPlayHead (&ph);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (4, 512);
    juce::MidiBuffer midi;
    juce::Random rng (1);
    auto run = [&] (int blocks, bool pumpUi) {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 4; ++ch)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (ch, i, (ch < 2 && b % 90 < 12) ? (rng.nextFloat() - 0.5f) * 0.5f : 0.0f);
            proc.processBlock (buf, midi);
            if (pumpUi && b % 8 == 0)
                pump (15);
        }
    };
    run (200, false);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    pump (200);
    run (400, true);
    pump (200);
    save (*ed, out.getChildFile ("editor.png"));
    save (*ed, out.getChildFile ("editor@2x.png"), 2.0f);
    if (auto* se = dynamic_cast<schimmer::SchimmerEditor*> (ed.get()))
        for (int page = 1; page < schimmer::MainView::kNumPages; ++page)
        {
            se->getView().selectPage (page);
            pump (100);
            save (*ed, out.getChildFile ("editor_page" + juce::String (page) + ".png"));
        }
    ed.reset();
    proc.setPlayHead (nullptr);
    return 0;
}
