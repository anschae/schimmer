#include "PresetBar.h"
#include "PluginProcessor.h"

namespace schimmer
{
PresetBar::PresetBar (SchimmerProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &name, &next })
        addAndMakeVisible (b);
    prev.setConnectedEdges (juce::Button::ConnectedOnRight);
    name.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    next.setConnectedEdges (juce::Button::ConnectedOnLeft);
    prev.onClick = [this] { proc.getPresets().loadNext (-1); refresh(); };
    next.onClick = [this] { proc.getPresets().loadNext (+1); refresh(); };
    name.onClick = [this] { showMenu(); };
    name.setColour (juce::TextButton::textColourOffId, theme::text);
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    name.setTooltip ("Presets: load, save, delete");
    refresh();
    startTimerHz (4);
}

PresetBar::~PresetBar() = default;

void PresetBar::refresh()
{
    name.setButtonText (proc.getPresets().getCurrentDisplayName());
}

void PresetBar::timerCallback() { refresh(); }

void PresetBar::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (30));
    next.setBounds (r.removeFromRight (30));
    name.setBounds (r);
}

void PresetBar::showMenu()
{
    auto& pm = proc.getPresets();
    const auto all = pm.list();
    juce::PopupMenu user, m;
    juce::StringArray cats;
    std::vector<juce::PopupMenu> catMenus;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& e = all[(size_t) i];
        const bool ticked = e.displayName() == pm.getCurrentDisplayName();
        if (! e.factory) { user.addItem (1 + i, e.name, true, ticked); continue; }
        int c = cats.indexOf (e.category);
        if (c < 0) { cats.add (e.category); catMenus.emplace_back(); c = cats.size() - 1; }
        catMenus[(size_t) c].addItem (1 + i, e.name, true, ticked);
    }
    for (int c = 0; c < cats.size(); ++c)
        m.addSubMenu (cats[c], catMenus[(size_t) c]);
    m.addSeparator();
    m.addSubMenu ("User", user, user.getNumItems() > 0);
    m.addSeparator();
    m.addItem (10001, "Save preset...");
    const auto current = pm.getCurrentDisplayName();
    const bool isUser = current.startsWith ("User / ");
    m.addItem (10002, "Delete '" + pm.getCurrentName() + "'", isUser);
    m.addSeparator();
    m.addItem (10004, "Show preset folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name), [this, all] (int r) {
        auto& presets = proc.getPresets();
        if (r >= 1 && r <= (int) all.size())
            presets.load (all[(size_t) r - 1]);
        else if (r == 10001)
            askSaveName();
        else if (r == 10002)
        {
            for (const auto& e : all)
                if (e.displayName() == presets.getCurrentDisplayName())
                    presets.deleteUser (e);
        }
        else if (r == 10004)
        {
            PresetManager::getUserFolder().createDirectory();
            PresetManager::getUserFolder().revealToUser();
        }
        refresh();
    });
}

void PresetBar::askSaveName()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name of the preset:", juce::MessageBoxIconType::NoIcon, this);
    const auto cur = proc.getPresets().getCurrentName();
    w->addTextEditor ("name", cur == "Init" ? juce::String() : cur);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result) {
        if (result == 1)
            proc.getPresets().saveUser (w->getTextEditorContents ("name"));
        refresh();
    }), true);
}
} // namespace schimmer
