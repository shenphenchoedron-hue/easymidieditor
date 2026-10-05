#pragma once
// Application controller: owns the model and all services and wires them
// together. GUI components talk to this, never to each other's data.
#include "audio/AudioEngine.h"
#include "input/ChordInputProcessor.h"
#include "midi/MidiDeviceManager.h"
#include "model/Commands.h"
#include "plugins/PluginManager.h"
#include "recording/Recorder.h"
#include "sequencer/Timing.h"
#include "settings/Settings.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <set>

namespace mc {

class AppContext final : public juce::ChangeBroadcaster {
public:
    AppContext();
    ~AppContext() override;

    model::Project project;
    model::UndoStack undo{project};
    settings::Settings settings;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<audio::AudioEngine> engine;
    std::unique_ptr<plugins::PluginManager> plugins;
    std::unique_ptr<midi::MidiDeviceManager> midiDevices;
    recording::Recorder recorder;
    std::vector<model::Note> clipboard; // starts relative to 0

    // ---- project files
    void newProject();
    bool openProject(const juce::File&, juce::String& error);
    bool saveProject(const juce::File&, juce::String& error);
    bool importMidi(const juce::File&, juce::String& error);
    bool exportMidi(const juce::File&, juce::String& error);
    bool exportMusicXml(const juce::File&, juce::String& error);
    bool importMusicXml(const juce::File&, juce::String& error); // .musicxml, .xml, .mxl
    juce::File currentFile;
    bool hasUnsavedChanges() const { return dirty; }

    // ---- tracks
    void addTrack();
    void deleteTrack(model::TrackId);
    void selectTrack(model::TrackId);
    model::MidiTrack* activeMidiTrack() const { return project.midiTrack(project.activeTrack); }
    void setTrackPlugin(model::TrackId, const plugins::PluginInfo*); // nullptr = remove
    void setTrackBypass(model::TrackId, bool);
    void openPluginEditor(model::TrackId);
    bool isPluginMissing(model::TrackId id) const { return missing.count(id) > 0; }
    juce::String pluginError(model::TrackId id) const;

    // ---- transport
    void play();
    void stop();
    void toggleRecord();
    bool isRecording() const { return recorder.isRecording(); }
    void returnToStart();
    void setPosition(model::Tick);
    void setTempo(double bpm);
    void setTimeSignature(int num, int den);
    void setLoop(bool enabled, model::Tick start, model::Tick end);

    // ---- harmony / chord input
    void setHarmony(int root, const std::string& scaleId, bool chordMode);
    void setModifierLatched(input::Modifier, bool on);
    bool isModifierLatched(input::Modifier m) const;
    theory::ChordRequest chordRequestFor(int pitch) const; // honours latched + held modifiers
    input::ChordDisplayState chordDisplay() const;
    void beginMidiLearn(input::Modifier);
    std::optional<input::Modifier> learningModifier() const;
    void setControllerMapping(const input::ControllerMapping&);

    // ---- live note input (any thread): from MIDI keyboard or on-screen keyboard
    void handleKeyboardNote(bool on, int pitch, int velocity);
    void previewNotes(const std::vector<int>& pitches, int velocity, int durationMs); // GUI: audition inserted notes

    // Call after modifying the model without a command (e.g. live drag).
    void modelChangedWithoutUndo() { project.notifyChanged(); }

    // Snaps for UI
    seq::GridValue grid;
    bool snapEnabled = true;

private:
    void onModelChanged();
    void syncEngine();
    void addImportedTracks(const model::Project& imported); // undoable; adopts tempo + time signature
    void captureAllPluginStates();
    void handleMidi(const juce::MidiMessage&);
    void sendLive(const std::vector<input::OutputEvent>&);
    void closeEditor(model::TrackId);

    mutable std::mutex chordMutex;
    input::ChordInputProcessor chordInput;

    std::atomic<model::TrackId> activeTrackAtomic{0};
    std::atomic<int> activeChannelAtomic{0};
    std::map<model::TrackId, model::PluginReference> loadedRefs;
    std::set<model::TrackId> missing;
    std::map<model::TrackId, juce::String> errors;
    std::map<model::TrackId, std::unique_ptr<juce::DocumentWindow>> editorWindows;
    int listenerHandle = 0;
    bool dirty = false;
    bool suppressDirty = false;
};

} // namespace mc
