#include "model/Track.h"
#include <algorithm>

namespace mc::model {

NoteId MidiTrack::addNote(Note n)
{
    if (n.id == 0 || findNote(n.id)) n.id = nextId_;
    nextId_ = std::max(nextId_, n.id + 1);
    n.pitch = std::clamp(n.pitch, 0, 127);
    n.velocity = std::clamp(n.velocity, 1, 127);
    n.length = std::max<Tick>(1, n.length);
    n.start = std::max<Tick>(0, n.start);
    notes_.push_back(n);
    sort();
    return n.id;
}

bool MidiTrack::removeNote(NoteId id)
{
    auto it = std::find_if(notes_.begin(), notes_.end(), [id](const Note& n) { return n.id == id; });
    if (it == notes_.end()) return false;
    notes_.erase(it);
    return true;
}

bool MidiTrack::updateNote(const Note& n)
{
    auto it = std::find_if(notes_.begin(), notes_.end(), [&](const Note& x) { return x.id == n.id; });
    if (it == notes_.end()) return false;
    *it = n;
    it->pitch = std::clamp(it->pitch, 0, 127);
    it->velocity = std::clamp(it->velocity, 1, 127);
    it->length = std::max<Tick>(1, it->length);
    it->start = std::max<Tick>(0, it->start);
    sort();
    return true;
}

const Note* MidiTrack::findNote(NoteId id) const
{
    for (auto& n : notes_) if (n.id == id) return &n;
    return nullptr;
}

void StepPattern::normalise()
{
    pitch = std::clamp(pitch, 0, 127);
    numSteps = std::clamp(numSteps, 1, 64);
    stepTicks = std::max<Tick>(1, stepTicks);
    repeats = std::clamp(repeats, 1, 256);
    startTick = std::max<Tick>(0, startTick);
    steps.resize((size_t)numSteps, 0);
    for (auto& v : steps) v = (std::uint8_t)std::min<int>(v, 127);
}

void MidiTrack::regenerateStepNotes()
{
    step.normalise();
    notes_.clear();
    nextId_ = 1;
    const Tick len = std::max<Tick>(1, step.stepTicks - step.stepTicks / 8); // short gap between hits
    for (int r = 0; r < step.repeats; ++r)
        for (int i = 0; i < step.numSteps; ++i)
            if (const int v = step.steps[(size_t)i]; v > 0) {
                Note n;
                n.pitch = step.pitch;
                n.velocity = v;
                n.start = step.startTick + ((Tick)r * step.numSteps + i) * step.stepTicks;
                n.length = len;
                addNote(n);
            }
}

void MidiTrack::sort()
{
    std::stable_sort(notes_.begin(), notes_.end(), [](const Note& a, const Note& b) {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });
}

} // namespace mc::model
