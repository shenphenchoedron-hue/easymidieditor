#pragma once
// Per-user settings (not per project): devices, controller mappings, paths.
#include "input/ControllerMapping.h"
#include <juce_data_structures/juce_data_structures.h>

namespace mc::settings {

class Settings {
public:
    Settings();
    juce::File dataDirectory() const;

    input::ControllerMapping controllerMapping() const;
    void setControllerMapping(const input::ControllerMapping&);

    juce::String midiInputIdentifier() const;
    void setMidiInputIdentifier(const juce::String&);

    std::unique_ptr<juce::XmlElement> audioDeviceState() const;
    void setAudioDeviceState(const juce::XmlElement*);

    juce::File lastDirectory() const;
    void setLastDirectory(const juce::File&);

private:
    mutable juce::ApplicationProperties props;
    juce::PropertiesFile& file() const { return *props.getUserSettings(); }
};

} // namespace mc::settings
