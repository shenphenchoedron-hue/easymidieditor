#pragma once
#include "AppContext.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace mc::gui {

// Audio device / sample rate / buffer size, MIDI input, chord modifier keys with MIDI Learn.
class SettingsComponent final : public juce::Component, private juce::ChangeListener {
public:
    explicit SettingsComponent(AppContext&);
    ~SettingsComponent() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    static void show(AppContext&);

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refreshMidiInputs();
    void refreshMapping();

    AppContext& app;
    juce::AudioDeviceSelectorComponent audioSelector;
    juce::Label midiLabel{{}, "MIDI input"}, mapLabel{{}, "Chord modifier keys"};
    juce::ComboBox midiInput;
    juce::TextButton refreshBtn{"Refresh"}, resetBtn{"Defaults"};
    struct MapRow {
        juce::Label name;
        juce::Slider note{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
        juce::Label noteName;
        juce::TextButton learn{"MIDI Learn"};
    };
    std::array<MapRow, (int)input::Modifier::Count> rows;
    juce::StringArray midiIds;
};

} // namespace mc::gui
