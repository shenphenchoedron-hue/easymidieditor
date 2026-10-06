#include "gui/ChordPanel.h"
#include "theory/Scale.h"
#include "gui/Theme.h"

namespace mc::gui {

ChordPanel::ChordPanel(AppContext& a) : app(a)
{
    for (int pc = 0; pc < 12; ++pc) root.addItem(theory::pitchClassName(pc), pc + 1);
    auto& scales = theory::ScaleRegistry::instance().all();
    for (size_t i = 0; i < scales.size(); ++i) scale.addItem(scales[i].displayName, (int)i + 1);
    root.onChange = [this] { push(); };
    scale.onChange = [this] { push(); };
    chordMode.setClickingTogglesState(true);
    chordMode.onClick = [this] { push(); };
    chordMode.setTooltip("Chord Input Mode: played/clicked notes become diatonic chords of the selected scale");

    for (int i = 0; i < (int)input::Modifier::Count; ++i) {
        auto& b = mods[(size_t)i];
        b.setButtonText(input::modifierName((input::Modifier)i));
        b.setClickingTogglesState(true);
        b.setColour(juce::TextButton::buttonOnColourId, theme::col::accent2);
        b.setTooltip("Latch modifier (or hold its key on the MIDI keyboard)");
        b.onClick = [this, i] { app.setModifierLatched((input::Modifier)i, !app.isModifierLatched((input::Modifier)i)); };
        addAndMakeVisible(b);
    }
    chordLabel.setFont(theme::uiFont(15.0f, true));
    chordLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6b4a12));
    chordLabel.setJustificationType(juce::Justification::centredLeft);
    chordLabel.setBorderSize({0, 10, 0, 10});
    for (size_t i = 0; i < nextBtns.size(); ++i) {
        auto& b = nextBtns[i];
        b.onClick = [this, i] { playSuggestion(i); };
        addChildComponent(b);
    }
    nextLabel.setTooltip("Suggested next chords (circle of fifths). Click to hear one.");
    addChildComponent(nextLabel);
    for (auto* l : {&rootLabel, &scaleLabel, &modsLabel, &nextLabel}) {
        l->setFont(theme::uiFont(13.5f, true));
        l->setColour(juce::Label::textColourId, theme::col::textDim);
        l->setJustificationType(juce::Justification::centredRight);
        l->setBorderSize({0, 0, 0, 2});
    }
    for (juce::Component* c : std::initializer_list<juce::Component*>{&rootLabel, &root, &scaleLabel, &scale, &chordMode, &modsLabel, &chordLabel})
        addAndMakeVisible(c);
    app.addChangeListener(this);
    changeListenerCallback(nullptr);
}

ChordPanel::~ChordPanel() { app.removeChangeListener(this); }

void ChordPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xffeef1f4));
    g.setColour(theme::col::borderSoft);
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
    // key/status box
    auto box = chordLabel.getBounds().toFloat().reduced(0.5f);
    g.setColour(theme::col::warmSoft);
    g.fillRoundedRectangle(box, theme::radius);
    g.setColour(theme::col::warm.withAlpha(0.55f));
    g.drawRoundedRectangle(box, theme::radius, 1.0f);
}

void ChordPanel::resized()
{
    using namespace theme;
    auto r = getLocalBounds().reduced(gap, 6);
    r.removeFromLeft(30 + gapGroup); // align with the transport row (logo column)
    auto place = [&](juce::Component& c, int w, int after = gapS) { c.setBounds(r.removeFromLeft(w)); r.removeFromLeft(after); };
    place(rootLabel, 34); place(root, 64, gapGroup);
    place(scaleLabel, 38); place(scale, 150, gapGroup);
    place(chordMode, 132, gapGroup);
    place(modsLabel, 64);
    for (auto& b : mods) place(b, 96);
    r.removeFromLeft(gapGroup);
    const int textW = (int)juce::GlyphArrangement::getStringWidth(chordLabel.getFont(), chordLabel.getText()) + 24;
    chordLabel.setBounds(r.removeFromLeft(juce::jlimit(120, std::max(120, r.getWidth()), textW)));

    r.removeFromLeft(gapGroup);
    nextLabel.setBounds(r.removeFromLeft(34));
    r.removeFromLeft(gapS);
    for (size_t i = 0; i < nextBtns.size(); ++i) {
        auto& b = nextBtns[i];
        const int w = std::max(44, (int)juce::GlyphArrangement::getStringWidth(theme::uiFont(16.0f, true), b.getButtonText()) + 20);
        b.setBounds(r.getWidth() >= w ? r.removeFromLeft(w) : juce::Rectangle<int>());
        r.removeFromLeft(gapS);
    }
}

void ChordPanel::updateSuggestions(int lastRoot)
{
    const auto& h = app.project.harmony;
    const juce::String key = juce::String(lastRoot) + "|" + juce::String(h.root) + "|" + juce::String(h.scaleId) + "|" + (h.chordMode ? "1" : "0");
    if (key == suggestionKey) return;
    suggestionKey = key;

    suggestions.clear();
    if (h.chordMode && lastRoot >= 0) {
        const theory::Scale s(h.root, theory::ScaleRegistry::instance().byIdOrDefault(h.scaleId));
        suggestions = theory::ChordSuggestions::next(s, lastRoot, theory::Extension::Triad, (int)nextBtns.size());
    }
    nextLabel.setVisible(!suggestions.empty());
    for (size_t i = 0; i < nextBtns.size(); ++i) {
        auto& b = nextBtns[i];
        const bool show = i < suggestions.size();
        b.setVisible(show);
        if (show) {
            b.setButtonText(juce::String::fromUTF8(suggestions[i].symbol.c_str()));
            b.setTooltip(juce::String(suggestions[i].reason));
        }
    }
    resized();
}

void ChordPanel::playSuggestion(size_t i)
{
    if (i >= suggestions.size()) return;
    const auto& h = app.project.harmony;
    const theory::Scale s(h.root, theory::ScaleRegistry::instance().byIdOrDefault(h.scaleId));
    // Use the current modifiers (inversion / 7th / 9th) so it sounds like playing that key.
    const auto chord = theory::ChordEngine::build(s, app.chordRequestFor(suggestions[i].rootPitch));
    app.previewNotes(chord.notes, 90, 700);
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
    updateSuggestions(d.chordMode ? d.lastRoot : -1);
    if (text != chordLabel.getText()) {
        chordLabel.setText(text, juce::dontSendNotification);
        resized();
        repaint();
    }
}

} // namespace mc::gui
