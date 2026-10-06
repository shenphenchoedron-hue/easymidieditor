#pragma once
// Track model. Track is the generic base; MidiTrack is implemented now,
// AudioTrack exists as a placeholder so audio recording can be added later.
#include "model/Note.h"
#include <memory>
#include <string>
#include <vector>

namespace mc::model {

using TrackId = std::uint64_t;

enum class TrackKind { Midi, Audio };

class Track {
public:
    explicit Track(TrackId id, std::string n) : name(std::move(n)), id_(id) {}
    virtual ~Track() = default;
    virtual TrackKind kind() const = 0;
    virtual std::unique_ptr<Track> clone() const = 0;

    TrackId id() const { return id_; }

    std::string name;
    bool mute = false;
    bool solo = false;
    float volume = 0.8f;   // linear gain 0..1
    float pan = 0.0f;      // -1..1
    std::uint32_t colour = 0; // 0xAARRGGBB for the track header, 0 = default theme colour

protected:
    TrackId id_;
};

// Format-agnostic reference to an instrument plugin. The track only stores
// what is needed to restore the plugin; the plugin host layer interprets it.
struct PluginReference {
    std::string format;        // "VST3", "AudioUnit", "LV2", "CLAP", ... (opaque to the model)
    std::string identifier;    // host-specific unique id / path
    std::string name;
    std::string manufacturer;
    std::string stateBase64;   // opaque plugin state
    bool bypassed = false;

    bool empty() const { return identifier.empty(); }
};

// Step sequencer line. A step sequencer is a group of MidiTracks sharing the
// same stepGroup id; every track is one line with its own instrument. The
// line's notes are generated from the pattern, so playback, MIDI export and
// the piano roll see ordinary notes.
struct StepPattern {
    int pitch = 36;                  // note played by this line
    int numSteps = 16;               // steps per pattern
    Tick stepTicks = kPPQ / 4;       // 1/16 by default
    int repeats = 4;                 // pattern is repeated this many times
    Tick startTick = 0;
    std::vector<std::uint8_t> steps; // velocity per step, 0 = off (size == numSteps)

    void normalise();                // clamps values and resizes steps
    bool operator==(const StepPattern&) const = default;
};

class MidiTrack final : public Track {
public:
    using Track::Track;
    TrackKind kind() const override { return TrackKind::Midi; }
    std::unique_ptr<Track> clone() const override { return std::make_unique<MidiTrack>(*this); }

    int channel = 0; // 0..15
    PluginReference plugin;

    std::uint64_t stepGroup = 0;     // != 0: this track is a line of a step sequencer
    StepPattern step;
    bool isStepLine() const { return stepGroup != 0; }
    void regenerateStepNotes();      // replaces all notes with the pattern

    const std::vector<Note>& notes() const { return notes_; }
    NoteId addNote(Note n);                 // assigns id if 0; keeps notes sorted by start
    bool removeNote(NoteId id);
    bool updateNote(const Note& n);         // matched by id
    const Note* findNote(NoteId id) const;
    void clearNotes() { notes_.clear(); }
    NoteId nextNoteId() const { return nextId_; }

private:
    void sort();
    std::vector<Note> notes_;
    NoteId nextId_ = 1;
};

class AudioTrack final : public Track {
public:
    using Track::Track;
    TrackKind kind() const override { return TrackKind::Audio; }
    std::unique_ptr<Track> clone() const override { return std::make_unique<AudioTrack>(*this); }
};

} // namespace mc::model
