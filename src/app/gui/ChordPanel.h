#pragma once
#include "AppContext.h"
#include "theory/ChordSuggestions.h"

namespace mc::gui {

// Quick-access harmony strip: root, scale, chord mode, modifiers, current chord.
class ChordPanel final : public juce::Component, private juce::ChangeListener {
public:
    explicit ChordPanel(AppContext&);
    ~ChordPanel() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void refreshDisplay();

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void push();
    void updateSuggestions(int lastRoot);
    void playSuggestion(size_t i);

    AppContext& app;
    juce::Label rootLabel{{}, "Root"}, scaleLabel{{}, "Scale"}, modsLabel{{}, "Modifiers"}, chordLabel;
    juce::ComboBox root, scale;
    juce::TextButton chordMode{"Chord Mode: OFF"};
    std::array<juce::TextButton, (int)input::Modifier::Count> mods;

    // Next-chord suggestions (circle of fifths), shown in chord mode after a chord was played.
    juce::Label nextLabel{{}, "Next"};
    std::array<juce::TextButton, 4> nextBtns;
    std::vector<theory::ChordSuggestion> suggestions;
    juce::String suggestionKey; // inputs the current suggestions were built from
};

} // namespace mc::gui
