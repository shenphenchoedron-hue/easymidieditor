#pragma once
#include "AppContext.h"

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

    AppContext& app;
    juce::Label rootLabel{{}, "Root"}, scaleLabel{{}, "Scale"}, modsLabel{{}, "Modifiers"}, chordLabel;
    juce::ComboBox root, scale;
    juce::TextButton chordMode{"Chord Mode: OFF"};
    std::array<juce::TextButton, (int)input::Modifier::Count> mods;
};

} // namespace mc::gui
