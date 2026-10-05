#pragma once
#include "input/ControllerMapping.h"
#include "model/Track.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mc::model {

struct TimeSignature {
    int numerator = 4;
    int denominator = 4;
    bool operator==(const TimeSignature&) const = default;
};

struct HarmonySettings {
    int root = 9;                         // pitch class, A
    std::string scaleId = "natural_minor";
    bool chordMode = false;
};

class Project {
public:
    Project();
    Project(const Project&);
    Project& operator=(const Project&);

    // ---- tracks
    const std::vector<std::unique_ptr<Track>>& tracks() const { return tracks_; }
    Track* track(TrackId id) const;
    MidiTrack* midiTrack(TrackId id) const;
    int indexOf(TrackId id) const;
    MidiTrack& addMidiTrack(const std::string& name = {});
    void insertTrack(int index, std::unique_ptr<Track> t);  // used by undo
    std::unique_ptr<Track> removeTrack(TrackId id);
    TrackId nextTrackId() const { return nextTrackId_; }

    TrackId activeTrack = 0;

    // ---- global song settings
    double tempoBpm = 120.0;
    TimeSignature timeSig;
    Tick playhead = 0;
    bool loopEnabled = false;
    Tick loopStart = 0;
    Tick loopEnd = 4 * 4 * kPPQ;
    HarmonySettings harmony;
    input::ControllerMapping controllerMapping;

    // ---- change notification (UI observes the model; model never knows the UI)
    using Listener = std::function<void()>;
    int addListener(Listener l);
    void removeListener(int handle);
    void notifyChanged() const;

private:
    std::vector<std::unique_ptr<Track>> tracks_;
    TrackId nextTrackId_ = 1;
    std::vector<std::pair<int, Listener>> listeners_;
    int nextListener_ = 1;
};

} // namespace mc::model
