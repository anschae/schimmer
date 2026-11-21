#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>
#include "model/PresetManager.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    // never touch the user's real preset folder
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SchimmerTestPresets");
    tmp.deleteRecursively();
    schimmer::PresetManager::setUserFolderOverride (tmp);
    const int result = Catch::Session().run (argc, argv);
    tmp.deleteRecursively();
    return result;
}
