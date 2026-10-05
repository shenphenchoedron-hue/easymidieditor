juce_add_gui_app(MidiComposer
    PRODUCT_NAME "MIDI Composer"
    COMPANY_NAME "MidiComposer"
    BUNDLE_ID com.midicomposer.app
    MICROPHONE_PERMISSION_ENABLED FALSE)

target_sources(MidiComposer PRIVATE
    Main.cpp
    AppContext.cpp
    settings/Settings.cpp
    audio/AudioEngine.cpp
    midi/MidiDeviceManager.cpp
    recording/Recorder.cpp
    plugins/BasicSynth.cpp
    plugins/JucePluginHost.cpp
    plugins/ClapPluginHost.cpp
    plugins/PluginManager.cpp
    gui/MainComponent.cpp
    gui/TransportBar.cpp
    gui/TrackListComponent.cpp
    gui/PianoRollComponent.cpp
    gui/ChordPanel.cpp
    gui/PluginBrowser.cpp
    gui/SettingsComponent.cpp
)

target_include_directories(MidiComposer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(MC_HOST_AU 0)
set(MC_HOST_LV2 0)
if(APPLE)
    set(MC_HOST_AU 1)
endif()
if(UNIX AND NOT APPLE)
    set(MC_HOST_LV2 1)
endif()

target_compile_definitions(MidiComposer PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MidiComposer,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MidiComposer,JUCE_VERSION>"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=${MC_HOST_AU}
    JUCE_PLUGINHOST_LV2=${MC_HOST_LV2}
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MidiComposer PRIVATE
    mc_core
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_devices
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
