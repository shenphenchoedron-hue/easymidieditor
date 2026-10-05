#pragma once
// Built-in fallback instrument so the app makes sound without any plugins
// installed. Exposed through the same host interface as external plugins.
#include "plugins/InstrumentPlugin.h"

namespace mc::plugins {

class InternalPluginHost final : public InstrumentPluginHost {
public:
    static constexpr const char* kFormat = "Internal";
    static constexpr const char* kBasicSynthId = "internal:basic-synth";
    static PluginInfo basicSynthInfo();

    std::string hostName() const override { return "Internal"; }
    juce::StringArray formats() const override { return {kFormat}; }
    std::vector<PluginInfo> scan(const std::function<bool(float, const juce::String&)>&) override { return {basicSynthInfo()}; }
    bool stillExists(const PluginInfo& i) const override { return i.identifier == kBasicSynthId; }
    std::unique_ptr<InstrumentPlugin> create(const PluginInfo&, double sr, int bs, juce::String& error) override;
};

} // namespace mc::plugins
