#pragma once
// Turns raw keyboard note on/off into the notes that should actually sound /
// be recorded. Handles modifier keys, chord generation and MIDI Learn.
// Pure logic: no MIDI devices, no threads, no GUI.
#include "input/ControllerMapping.h"
#include "theory/ChordEngine.h"
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mc::input {

struct OutputEvent {
    bool noteOn;
    int pitch;
    int velocity;
    bool operator==(const OutputEvent&) const = default;
};

struct ChordDisplayState {
    bool chordMode = false;
    std::array<bool, (int)Modifier::Count> modifiersHeld{};
    std::string currentChord;        // e.g. "Am7 – 1st inversion" (last/held chord)
    std::vector<int> soundingNotes;  // generated notes currently held
    int lastRoot = -1;               // played key of the last chord, -1 = none
};

class ChordInputProcessor {
public:
    void setScale(int rootPc, const std::string& scaleId);
    void setChordMode(bool on) { chordMode_ = on; }
    bool chordMode() const { return chordMode_; }
    void setMapping(const ControllerMapping& m) { mapping_ = m; }
    const ControllerMapping& mapping() const { return mapping_; }

    // MIDI Learn: next incoming note-on is assigned to `m` and consumed.
    void beginLearn(Modifier m) { learning_ = m; }
    void cancelLearn() { learning_.reset(); }
    std::optional<Modifier> learning() const { return learning_; }

    // Modifiers latched from the GUI (act as if the modifier key were held).
    void setLatched(Modifier m, bool on) { latched_[(size_t)m] = on; }
    bool isLatched(Modifier m) const { return latched_[(size_t)m]; }

    std::vector<OutputEvent> noteOn(int pitch, int velocity);
    std::vector<OutputEvent> noteOff(int pitch);
    std::vector<OutputEvent> allNotesOff();

    // Current modifier combination -> request (also used for mouse input in the piano roll).
    theory::ChordRequest requestFor(int pitch) const;
    theory::ChordResult chordFor(int pitch) const;

    ChordDisplayState displayState() const;
    // Set when a learn completed; cleared by the caller after persisting.
    std::optional<Modifier> takeLearnedModifier() { auto r = learned_; learned_.reset(); return r; }

private:
    void soundOn(int p, int v, std::vector<OutputEvent>& out);
    void soundOff(int p, std::vector<OutputEvent>& out);

    int root_ = 9;
    std::string scaleId_ = "natural_minor";
    bool chordMode_ = false;
    ControllerMapping mapping_;
    std::array<bool, (int)Modifier::Count> held_{}, latched_{};
    bool active(Modifier m) const { return held_[(size_t)m] || latched_[(size_t)m]; }
    std::optional<Modifier> learning_, learned_;
    std::map<int, std::vector<int>> keyToNotes_; // played key -> generated notes
    std::map<int, int> soundingCount_;          // pitch -> refcount
    int lastRoot_ = -1;
};

} // namespace mc::input
