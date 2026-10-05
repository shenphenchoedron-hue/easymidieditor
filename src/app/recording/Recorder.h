#pragma once
// Collects played notes (after chord generation) with tick timestamps.
// record() is called from the MIDI thread; finish() on the message thread.
#include "model/Note.h"
#include <map>
#include <mutex>
#include <vector>

namespace mc::recording {

class Recorder {
public:
    void start(model::Tick loopStart, model::Tick loopEnd, bool loop);
    bool isRecording() const { std::lock_guard l(m); return active; }
    void noteOn(int pitch, int velocity, model::Tick at);
    void noteOff(int pitch, model::Tick at);
    std::vector<model::Note> finish(model::Tick at); // closes hanging notes
    std::vector<model::Note> pending() const;         // for live drawing

private:
    model::Tick close(const model::Note&, model::Tick at) const;
    mutable std::mutex m;
    bool active = false, loop = false;
    model::Tick loopStart = 0, loopEnd = 0;
    std::map<int, model::Note> open;
    std::vector<model::Note> done;
};

} // namespace mc::recording
