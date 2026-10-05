#pragma once
// Built-in instruments, so the app makes sound without any plugins installed:
// Basic Synth, the bundled band instruments (GeneralUser GS SoundFont) and
// the user's own .sf2 files. Exposed through the same host interface as
// external plugins.
#include "plugins/InstrumentPlugin.h"

namespace mc::plugins {

class InternalPluginHost final : public InstrumentPluginHost {
public:
    static constexpr const char* kFormat = "Internal";
    static constexpr const char* kBasicSynthId = "internal:basic-synth";
    static PluginInfo basicSynthInfo();
    // Basic Synth + built-in band instruments (cheap, no file access besides an existence check).
    static std::vector<PluginInfo> builtins();
    // Default for new tracks: Grand Piano if the built-in sounds are installed, else Basic Synth.
    static PluginInfo defaultInstrument();

    explicit InternalPluginHost(juce::File userSoundsFolder) : userSounds(std::move(userSoundsFolder)) {}

    std::string hostName() const override { return "Internal"; }
    juce::StringArray formats() const override;
    std::vector<PluginInfo> scan(const std::function<bool(float, const juce::String&)>&) override;
    bool stillExists(const PluginInfo&) const override;
    std::unique_ptr<InstrumentPlugin> create(const PluginInfo&, double sr, int bs, juce::String& error) override;

private:
    juce::File userSounds;
};

} // namespace mc::plugins
