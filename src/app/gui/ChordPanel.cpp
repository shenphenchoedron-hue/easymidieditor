#include "gui/ChordPanel.h"
#include "theory/Scale.h"

namespace mc::gui {

ChordPanel::ChordPanel(AppContext& a) : app(a)
{
    for (int pc = 0; pc < 12; ++pc) root.addItem(theory::pitchClassName(pc), pc + 1);
    auto& scales = theory::ScaleRegistry::instance().all();
    for (size_t i = 0; i < scales.size(); ++i) scale.addItem(scales[i].displayName, (int)i + 1);
    root.onChange = [this] { push(); };
    scale.onChange = [this] { push(); };
    chordMode.setClickingTogglesState(true);
    chordMode.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2f8f4f));
    chordMode.onClick = [this] { push(); };
    chordMode.setTooltip("Chord Input Mode: played/clicked notes become diatonic chords of the selected scale");

    for (int i = 0; i < (int)input::Modifier::Count; ++i) {
        auto& b = mods[(size_t)i];
        b.setButtonText(input::modifierName((input::Modifier)i));
        b.setClickingTogglesState(true);
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd08020));
        b.setTooltip("Latch modifier (or hold its key on the MIDI keyboard)");
        b.onClick = [this, i] { app.setModifierLatched((input::Modifier)i, !app.isModifierLatched((input::Modifier)i)); };
        addAndMakeVisible(b);
    }
    chordLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    chordLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffd27f));
    for (juce::Component* c : std::initializer_list<juce::Component*>{&rootLabel, &root, &scaleLabel, &scale, &chordMode, &modsLabel, &chordLabel})
        addAndMakeVisible(c);
    app.addChangeListener(this);
    changeListenerCallback(nullptr);
}

ChordPanel::~ChordPanel() { app.removeChangeListener(this); }

void ChordPanel::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff2b2f36)); }

void ChordPanel::resized()
{
    auto r = getLocalBounds().reduced(4);
    auto place = [&](juce::Component& c, int w) { c.setBounds(r.removeFromLeft(w)); r.removeFromLeft(4); };
    place(rootLabel, 36); place(root, 64);
    place(scaleLabel, 40); place(scale, 140);
    place(chordMode, 130);
    place(modsLabel, 64);
    for (auto& b : mods) place(b, 92);
    r.removeFromLeft(8);
    chordLabel.setBounds(r);
}

void ChordPanel::push()
{
    auto& scales = theory::ScaleRegistry::instance().all();
    const int si = juce::jlimit(0, (int)scales.size() - 1, scale.getSelectedId() - 1);
    app.setHarmony(root.getSelectedId() - 1, scales[(size_t)si].id, chordMode.getToggleState());
}

void ChordPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    auto& h = app.project.harmony;
    root.setSelectedId(h.root + 1, juce::dontSendNotification);
    auto& scales = theory::ScaleRegistry::instance().all();
    for (size_t i = 0; i < scales.size(); ++i)
        if (scales[i].id == h.scaleId) scale.setSelectedId((int)i + 1, juce::dontSendNotification);
    chordMode.setToggleState(h.chordMode, juce::dontSendNotification);
    refreshDisplay();
}

void ChordPanel::refreshDisplay()
{
    const auto d = app.chordDisplay();
    chordMode.setButtonText(d.chordMode ? "Chord Mode: ON" : "Chord Mode: OFF");
    for (int i = 0; i < (int)input::Modifier::Count; ++i) {
        // lit if latched in GUI or held on keyboard
        mods[(size_t)i].setToggleState(d.modifiersHeld[(size_t)i], juce::dontSendNotification);
        mods[(size_t)i].setEnabled(d.chordMode);
    }
    juce::String text = juce::String(theory::pitchClassName(app.project.harmony.root)) + " " +
                        theory::ScaleRegistry::instance().byIdOrDefault(app.project.harmony.scaleId).displayName;
    if (d.chordMode) text += "   |   Chord: " + juce::String::fromUTF8(d.currentChord.empty() ? "-" : d.currentChord.c_str());
    chordLabel.setText(text, juce::dontSendNotification);
}

} // namespace mc::gui
