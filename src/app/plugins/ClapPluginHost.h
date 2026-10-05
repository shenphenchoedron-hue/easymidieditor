#pragma once
// CLAP host adapter (Milestone 14). Lives entirely behind InstrumentPluginHost,
// so adding real CLAP support needs no changes to tracks, sequencer or chord code.
//
// Implementation plan: vendor the header-only CLAP SDK (github.com/free-audio/clap)
// into external/clap, load bundles with juce::DynamicLibrary, enumerate
// clap_plugin_factory, wrap clap_plugin in an InstrumentPlugin that converts
// juce::MidiBuffer <-> clap_event_note, and map state to clap_plugin_state.
#include "plugins/InstrumentPlugin.h"

namespace mc::plugins {

class ClapPluginHost final : public InstrumentPluginHost {
public:
    std::string hostName() const override { return "CLAP"; }
    juce::StringArray formats() const override { return {"CLAP"}; }
    std::vector<PluginInfo> scan(const std::function<bool(float, const juce::String&)>&) override;
    bool stillExists(const PluginInfo& i) const override;
    std::unique_ptr<InstrumentPlugin> create(const PluginInfo&, double, int, juce::String& error) override;

    static juce::FileSearchPath defaultSearchPaths();
};

} // namespace mc::plugins
