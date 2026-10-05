#pragma once
// Format-agnostic instrument interface. Tracks, sequencer and engine only ever
// see this; VST3/AU/LV2/CLAP details live in concrete hosts.
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <string>

namespace mc::plugins {

// Result of a scan; persisted in the plugin cache.
struct PluginInfo {
    std::string format;        // "VST3", "AudioUnit", "LV2", "CLAP", "Internal"
    std::string identifier;    // unique, stable id within the host
    std::string name;
    std::string manufacturer;
    std::string category;
    std::string hostData;      // opaque, host specific (e.g. JUCE PluginDescription XML)
};

class InstrumentPlugin {
public:
    virtual ~InstrumentPlugin() = default;

    virtual std::string name() const = 0;
    virtual std::string format() const = 0;

    // Message thread
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void release() = 0;
    virtual bool hasEditor() const = 0;
    virtual juce::Component* createEditor() = 0;   // caller owns the returned component
    virtual juce::MemoryBlock getState() = 0;
    virtual void setState(const juce::MemoryBlock&) = 0;

    // Audio thread. `audio` is cleared, has >= 2 channels; must not allocate.
    virtual void process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) = 0;
};

// One per plugin technology (or family, e.g. everything JUCE can host).
class InstrumentPluginHost {
public:
    virtual ~InstrumentPluginHost() = default;
    virtual std::string hostName() const = 0;
    virtual juce::StringArray formats() const = 0;
    virtual bool handles(const std::string& format) const { return formats().contains(format); }

    // Called from a background thread. Must survive (skip) defective plugins.
    // `progress(fraction, currentItem)` returns false to cancel.
    virtual std::vector<PluginInfo> scan(const std::function<bool(float, const juce::String&)>& progress) = 0;

    virtual bool stillExists(const PluginInfo&) const = 0;
    virtual std::unique_ptr<InstrumentPlugin> create(const PluginInfo&, double sampleRate, int blockSize, juce::String& error) = 0;
};

} // namespace mc::plugins
