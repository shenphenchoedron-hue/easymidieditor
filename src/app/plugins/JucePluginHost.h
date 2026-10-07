#pragma once
// Hosts every format JUCE supports natively (VST3, AU/AUv3, LV2) behind the
// generic InstrumentPluginHost interface.
#include "plugins/InstrumentPlugin.h"

namespace mc::plugins {

// Parses an LV2 bundle's manifest.ttl: plugin URI -> its lv2:binary (exposed for tests).
std::vector<std::pair<juce::String, juce::File>> parseLv2Manifest(const juce::String& manifestText, const juce::File& bundle);

class JucePluginHost final : public InstrumentPluginHost {
public:
    explicit JucePluginHost(juce::File deadMansPedalFile);
    std::string hostName() const override { return "JUCE"; }
    juce::StringArray formats() const override;
    std::vector<PluginInfo> scan(const std::function<bool(float, const juce::String&)>& progress) override;
    bool stillExists(const PluginInfo&) const override;
    std::unique_ptr<InstrumentPlugin> create(const PluginInfo&, double sampleRate, int blockSize, juce::String& error) override;

private:
    static std::unique_ptr<juce::PluginDescription> descriptionFrom(const PluginInfo&);
    juce::AudioPluginFormatManager formatManager;
    juce::File deadMansPedal;
    juce::FileSearchPath lv2SearchPath() const;
};

} // namespace mc::plugins
