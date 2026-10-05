#pragma once
// Device discovery + selection of the active MIDI input. Survives devices
// disappearing and reconnects automatically when they come back.
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>

namespace mc::midi {

class MidiDeviceManager final : private juce::MidiInputCallback, private juce::Timer {
public:
    using Handler = std::function<void(const juce::MidiMessage&)>; // called on the MIDI thread

    MidiDeviceManager(juce::AudioDeviceManager&, Handler);
    ~MidiDeviceManager() override;

    juce::Array<juce::MidiDeviceInfo> availableInputs() const { return juce::MidiInput::getAvailableDevices(); }
    void selectInput(const juce::String& identifier); // empty = none
    juce::String selectedInput() const { return selected; }
    bool isConnected() const;

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;

    juce::AudioDeviceManager& devices;
    Handler handler;
    juce::String selected;
};

} // namespace mc::midi
