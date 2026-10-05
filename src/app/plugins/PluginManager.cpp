#include "plugins/PluginManager.h"
#include "io/Json.h"
#include "plugins/BasicSynth.h"
#include "plugins/ClapPluginHost.h"
#include "plugins/JucePluginHost.h"
#include "plugins/SoundFontPlayer.h"

namespace mc::plugins {

PluginManager::PluginManager(juce::File dataDir)
{
    dataDir.createDirectory();
    cacheFile = dataDir.getChildFile("plugin-cache.json");
    userSounds = soundfont::userSoundsFolder(dataDir);
    hosts.push_back(std::make_unique<InternalPluginHost>(userSounds));
    hosts.push_back(std::make_unique<JucePluginHost>(dataDir.getChildFile("plugin-scan-crashed.txt")));
    hosts.push_back(std::make_unique<ClapPluginHost>());
    loadCache();
}

juce::StringArray PluginManager::allFormats() const
{
    juce::StringArray s;
    for (auto& h : hosts) s.addArray(h->formats());
    s.removeDuplicates(false);
    return s;
}

InstrumentPluginHost* PluginManager::hostFor(const std::string& format) const
{
    for (auto& h : hosts) if (h->handles(format)) return h.get();
    return nullptr;
}

void PluginManager::rescan(const std::function<bool(float, const juce::String&)>& progress)
{
    std::vector<PluginInfo> found;
    for (size_t i = 0; i < hosts.size(); ++i) {
        auto part = hosts[i]->scan([&](float f, const juce::String& item) {
            return progress ? progress(((float)i + f) / (float)hosts.size(), item) : true;
        });
        found.insert(found.end(), part.begin(), part.end());
    }
    {
        std::lock_guard l(mutex);
        plugins_ = std::move(found);
    }
    saveCache();
}

const PluginInfo* PluginManager::find(const model::PluginReference& r) const
{
    std::lock_guard l(mutex);
    for (auto& p : plugins_) if (p.format == r.format && p.identifier == r.identifier) return &p;
    return nullptr;
}

model::PluginReference PluginManager::referenceFor(const PluginInfo& i)
{
    model::PluginReference r;
    r.format = i.format; r.identifier = i.identifier; r.name = i.name; r.manufacturer = i.manufacturer;
    return r;
}

std::unique_ptr<InstrumentPlugin> PluginManager::instantiate(const model::PluginReference& r, double sr, int bs, juce::String& error)
{
    PluginInfo info;
    if (auto* p = find(r)) info = *p;
    else if (r.format == InternalPluginHost::kFormat || r.format == soundfont::kUserFormat)
        info = {r.format, r.identifier, r.name, r.manufacturer, {}, {}}; // built-ins resolve from the identifier
    else { error = "Plugin not found: " + juce::String(r.name); return nullptr; }

    auto* host = hostFor(info.format);
    if (!host) { error = "No host for format " + juce::String(info.format); return nullptr; }
    std::unique_ptr<InstrumentPlugin> inst;
    try { inst = host->create(info, sr, bs, error); }
    catch (...) { error = "Plugin threw during creation"; return nullptr; }
    if (inst && !r.stateBase64.empty()) {
        juce::MemoryBlock m;
        if (m.fromBase64Encoding(juce::String(r.stateBase64))) inst->setState(m);
    }
    return inst;
}

void PluginManager::loadCache()
{
    plugins_ = InternalPluginHost::builtins();
    if (!cacheFile.existsAsFile()) return;
    try {
        auto j = io::Json::parse(cacheFile.loadFileAsString().toStdString());
        for (auto& e : j["plugins"].arr()) {
            PluginInfo i{e["format"].str(), e["identifier"].str(), e["name"].str(),
                         e["manufacturer"].str(), e["category"].str(), e["hostData"].str()};
            // Built-ins always come from the current app version, not from the cache.
            if (i.format == InternalPluginHost::kFormat) continue;
            plugins_.push_back(std::move(i));
        }
    } catch (...) {
        plugins_ = InternalPluginHost::builtins(); // corrupt cache: fall back, rescan later
    }
}

void PluginManager::saveCache() const
{
    io::Json::Array arr;
    {
        std::lock_guard l(mutex);
        for (auto& p : plugins_)
            arr.push_back(io::Json::Object{{"format", p.format}, {"identifier", p.identifier}, {"name", p.name},
                                           {"manufacturer", p.manufacturer}, {"category", p.category}, {"hostData", p.hostData}});
    }
    io::Json root;
    root.set("version", 1);
    root.set("plugins", arr);
    cacheFile.replaceWithText(root.dump(1));
}

} // namespace mc::plugins
