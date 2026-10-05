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

} // namespace mc::settings
