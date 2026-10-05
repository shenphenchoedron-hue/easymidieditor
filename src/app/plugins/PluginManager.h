#pragma once
// Aggregates all plugin hosts, owns the persisted scan cache and resolves the
// format-agnostic PluginReference stored on tracks into live instruments.
#include "model/Track.h"
#include "plugins/InstrumentPlugin.h"
#include <mutex>

namespace mc::plugins {

class PluginManager {
public:
    explicit PluginManager(juce::File dataDir);

    const std::vector<PluginInfo>& plugins() const { return plugins_; }
    juce::StringArray allFormats() const;

    // Background-thread safe. Replaces the cache; persists it.
    void rescan(const std::function<bool(float, const juce::String&)>& progress);
    bool hasCache() const { return cacheFile.existsAsFile(); }
    // Folder where users drop extra .sf2 files (picked up by the next rescan).
    juce::File userSoundsFolder() const { return userSounds; }

    const PluginInfo* find(const model::PluginReference&) const;
    static model::PluginReference referenceFor(const PluginInfo&);

    // Creates the instrument and restores its state. nullptr + error if missing/failing.
    std::unique_ptr<InstrumentPlugin> instantiate(const model::PluginReference&, double sr, int bs, juce::String& error);

private:
    InstrumentPluginHost* hostFor(const std::string& format) const;
    void loadCache();
    void saveCache() const;

    juce::File cacheFile, userSounds;
    std::vector<std::unique_ptr<InstrumentPluginHost>> hosts;
    std::vector<PluginInfo> plugins_;
    mutable std::mutex mutex;
};

} // namespace mc::plugins
