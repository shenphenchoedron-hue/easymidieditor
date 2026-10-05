#pragma once
// SoundFont (.sf2) instruments, played with TinySoundFont.
//  - Built-in band instruments from the bundled GeneralUser GS bank.
//  - User sounds: every preset of every .sf2 file in the user's Sounds folder.
// Each loaded .sf2 file is kept in memory once and shared by all instruments
// using it (tsf_copy shares sample data).
#include "plugins/InstrumentPlugin.h"

namespace mc::plugins {

namespace soundfont {

inline constexpr const char* kUserFormat = "SoundFont";   // format of user .sf2 presets
inline constexpr const char* kBuiltinPrefix = "internal:gm:";
inline constexpr const char* kUserPrefix = "sf2:";

// Location of the bundled GeneralUser GS file, or a non-existent File if not installed.
juce::File builtinFile();

// The user's folder for extra .sf2 files (created on demand).
juce::File userSoundsFolder(const juce::File& dataDir);

// Built-in instruments (format "Internal", category "Rock"/"Pop"/"Jazz").
// Empty if the bundled SoundFont is missing.
std::vector<PluginInfo> builtinInstruments();

// All presets of all .sf2 files in `folder` (format "SoundFont", category = file name).
// Background-thread safe.
std::vector<PluginInfo> scanUserFolder(const juce::File& folder, const std::function<bool(float, const juce::String&)>& progress);

bool isSoundFontInfo(const PluginInfo&);
bool stillExists(const PluginInfo&);
// Message thread.
std::unique_ptr<InstrumentPlugin> create(const PluginInfo&, double sampleRate, int blockSize, juce::String& error);

} // namespace soundfont
} // namespace mc::plugins
