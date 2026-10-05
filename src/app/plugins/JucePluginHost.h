#pragma once
// Hosts every format JUCE supports natively (VST3, AU/AUv3, LV2) behind the
// generic InstrumentPluginHost interface.
#include "plugins/InstrumentPlugin.h"

namespace mc::plugins {

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
};

} // namespace mc::plugins
