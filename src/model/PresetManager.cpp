#include "PresetManager.h"
#include "PluginProcessor.h"
#include "FactoryPresets.h"

namespace schimmer
{
namespace
{
juce::File& folderOverride()
{
    static juce::File f;
    return f;
}
const juce::String kExt = ".schpreset";
} // namespace

PresetManager::PresetManager (SchimmerProcessor& p) : proc (p) {}

void PresetManager::setUserFolderOverride (const juce::File& f) { folderOverride() = f; }

juce::File PresetManager::getUserFolder()
{
    if (folderOverride() != juce::File())
        return folderOverride();
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("anschae").getChildFile ("Schimmer").getChildFile ("Presets");
}

std::vector<PresetManager::Entry> PresetManager::list() const
{
    std::vector<Entry> out;
    const auto& fp = FactoryPresets::all();
    for (int i = 0; i < (int) fp.size(); ++i)
        out.push_back ({ fp[(size_t) i].name, fp[(size_t) i].category, true, i, {} });

    auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, "*" + kExt);
    files.sort();
    for (const auto& f : files)
        out.push_back ({ f.getFileNameWithoutExtension(), "User", false, -1, f });
    return out;
}

bool PresetManager::load (const Entry& e)
{
    juce::ValueTree state;
    if (e.factory)
    {
        const auto& fp = FactoryPresets::all();
        if (e.factoryIndex < 0 || e.factoryIndex >= (int) fp.size())
            return false;
        state = FactoryPresets::makeState (fp[(size_t) e.factoryIndex]);
    }
    else if (auto xml = juce::XmlDocument::parse (e.file))
        state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid())
        return false;
    // Mix lock: keep the current mix
    const bool lockMix = proc.value (P::mixLock) > 0.5f;
    const float mix = proc.value (P::mix);
    proc.restoreState (state);
    if (lockMix)
    {
        proc.setValueNotifying (P::mix, mix);
        proc.setValueNotifying (P::mixLock, 1.0f);
    }
    proc.setCurrentPresetName (e.displayName());
    if (onChange)
        onChange();
    return true;
}

void PresetManager::loadNext (int direction)
{
    const auto all = list();
    if (all.empty())
        return;
    const auto current = getCurrentDisplayName();
    int idx = -1;
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].displayName() == current)
            idx = i;
    idx = idx < 0 ? 0 : (idx + direction + (int) all.size()) % (int) all.size();
    load (all[(size_t) idx]);
}

bool PresetManager::saveUser (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;
    auto folder = getUserFolder();
    if (! folder.createDirectory())
        return false;
    proc.setCurrentPresetName ("User / " + name);
    auto state = proc.captureState();
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (folder.getChildFile (name + kExt)))
        return false;
    if (onChange)
        onChange();
    return true;
}

bool PresetManager::deleteUser (const Entry& e)
{
    if (e.factory || ! e.file.existsAsFile())
        return false;
    const bool ok = e.file.deleteFile();
    if (ok && onChange)
        onChange();
    return ok;
}

juce::String PresetManager::getCurrentDisplayName() const { return proc.getCurrentPresetName(); }

juce::String PresetManager::getCurrentName() const
{
    return getCurrentDisplayName().fromFirstOccurrenceOf (" / ", false, false);
}

void PresetManager::switchAB (int index)
{
    index = juce::jlimit (0, 1, index);
    if (index == activeAB)
        return;
    ab[activeAB] = proc.captureState();
    if (! ab[index].isValid())
        ab[index] = ab[activeAB].createCopy();
    proc.restoreState (ab[index]);
    activeAB = index;
    if (onChange)
        onChange();
}

void PresetManager::copyActiveToOther()
{
    ab[1 - activeAB] = proc.captureState();
}
} // namespace schimmer
