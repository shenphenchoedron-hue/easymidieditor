#include "plugins/ClapPluginHost.h"

namespace mc::plugins {

juce::FileSearchPath ClapPluginHost::defaultSearchPaths()
{
    juce::FileSearchPath p;
   #if JUCE_WINDOWS
    p.add(juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory).getChildFile("Common Files/CLAP"));
    p.add(juce::File::getSpecialLocation(juce::File::windowsLocalAppData).getChildFile("Programs/Common/CLAP"));
   #elif JUCE_MAC
    p.add(juce::File("/Library/Audio/Plug-Ins/CLAP"));
    p.add(juce::File("~/Library/Audio/Plug-Ins/CLAP"));
   #else
    p.add(juce::File("~/.clap"));
    p.add(juce::File("/usr/lib/clap"));
    p.add(juce::File("/usr/local/lib/clap"));
   #endif
    return p;
}

std::vector<PluginInfo> ClapPluginHost::scan(const std::function<bool(float, const juce::String&)>&)
{
    // Not yet implemented: CLAP bundles are discovered but not instantiated,
    // so they are not listed as usable instruments yet.
    return {};
}

bool ClapPluginHost::stillExists(const PluginInfo& i) const { return juce::File(i.identifier).exists(); }

std::unique_ptr<InstrumentPlugin> ClapPluginHost::create(const PluginInfo&, double, int, juce::String& error)
{
    error = "CLAP hosting is not implemented yet";
    return nullptr;
}

} // namespace mc::plugins
