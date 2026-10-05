#include "midi/MidiDeviceManager.h"

namespace mc::midi {

MidiDeviceManager::MidiDeviceManager(juce::AudioDeviceManager& d, Handler h) : devices(d), handler(std::move(h))
{
    devices.addMidiInputDeviceCallback({}, this); // receives from all enabled inputs
    startTimer(2000);
}

MidiDeviceManager::~MidiDeviceManager()
{
    stopTimer();
    devices.removeMidiInputDeviceCallback({}, this);
}

void MidiDeviceManager::selectInput(const juce::String& id)
{
    for (auto& info : juce::MidiInput::getAvailableDevices())
        devices.setMidiInputDeviceEnabled(info.identifier, info.identifier == id);
    selected = id;
}

bool MidiDeviceManager::isConnected() const
{
    return selected.isNotEmpty() && devices.isMidiInputDeviceEnabled(selected);
}

void MidiDeviceManager::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    if (handler) handler(m);
}

void MidiDeviceManager::timerCallback()
{
    // Reconnect when the selected device re-appears (hot-plug).
    if (selected.isEmpty() || devices.isMidiInputDeviceEnabled(selected)) return;
    for (auto& info : juce::MidiInput::getAvailableDevices())
        if (info.identifier == selected) { devices.setMidiInputDeviceEnabled(selected, true); break; }
}

} // namespace mc::midi
