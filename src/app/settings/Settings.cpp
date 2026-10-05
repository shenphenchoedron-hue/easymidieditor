#include "settings/Settings.h"

namespace mc::settings {

Settings::Settings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "MidiComposer";
    o.filenameSuffix = ".settings";
    o.folderName = "MidiComposer";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    props.setStorageParameters(o);
}

juce::File Settings::dataDirectory() const { return file().getFile().getParentDirectory(); }

input::ControllerMapping Settings::controllerMapping() const
{
    input::ControllerMapping m;
    const char* keys[] = {"modInversion1", "modInversion2", "modSeventh", "modNinth"};
    for (int i = 0; i < (int)input::Modifier::Count; ++i) m.notes[i] = file().getIntValue(keys[i], m.notes[i]);
    return m;
}

void Settings::setControllerMapping(const input::ControllerMapping& m)
{
    const char* keys[] = {"modInversion1", "modInversion2", "modSeventh", "modNinth"};
    for (int i = 0; i < (int)input::Modifier::Count; ++i) file().setValue(keys[i], m.notes[i]);
    file().saveIfNeeded();
}

juce::String Settings::midiInputIdentifier() const { return file().getValue("midiInput"); }
void Settings::setMidiInputIdentifier(const juce::String& s) { file().setValue("midiInput", s); file().saveIfNeeded(); }

std::unique_ptr<juce::XmlElement> Settings::audioDeviceState() const { return file().getXmlValue("audioDevice"); }
void Settings::setAudioDeviceState(const juce::XmlElement* x)
{
    if (x) file().setValue("audioDevice", x);
    file().saveIfNeeded();
}

juce::File Settings::lastDirectory() const
{
    juce::File f(file().getValue("lastDir"));
    return f.isDirectory() ? f : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
}

void Settings::setLastDirectory(const juce::File& f) { file().setValue("lastDir", f.getFullPathName()); }

std::vector<std::uint32_t> Settings::colourSwatches() const
{
    std::vector<std::uint32_t> out;
    for (auto& s : juce::StringArray::fromTokens(file().getValue("colourSwatches"), ",", ""))
        if (s.trim().isNotEmpty()) out.push_back((std::uint32_t)s.trim().getHexValue64());
    return out;
}

void Settings::setColourSwatches(const std::vector<std::uint32_t>& v)
{
    juce::StringArray a;
    for (auto c : v) a.add(juce::String::toHexString((juce::int64)c));
    file().setValue("colourSwatches", a.joinIntoString(","));
}

} // namespace mc::settings
