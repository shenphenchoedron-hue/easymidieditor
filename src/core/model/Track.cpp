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
    stepTicks = std::max<Tick>(1, stepTicks);
    barTicks = std::max<Tick>(1, barTicks);
    bars = std::clamp(bars, 1, 999);
    startTick = std::max<Tick>(0, startTick);
    steps.resize((size_t)totalSteps(), 0);
    for (auto& v : steps) v = (std::uint8_t)std::min<int>(v, 127);
}

namespace {
// Value at an absolute pattern tick, 0 if no step starts exactly there.
std::uint8_t valueAt(const StepPattern& p, const std::vector<std::uint8_t>& steps, Tick t)
{
    if (t < 0 || t % p.stepTicks) return 0;
    const auto i = (size_t)(t / p.stepTicks);
    return i < steps.size() ? steps[i] : 0;
}
} // namespace

void StepPattern::setStepTicks(Tick t)
{
    t = std::max<Tick>(1, t);
    if (t == stepTicks) return;
    const auto old = steps;
    const Tick oldTicks = stepTicks;
    stepTicks = t;
    steps.assign((size_t)totalSteps(), 0);
    for (size_t i = 0; i < old.size(); ++i)
        if (old[i]) {
            const Tick tick = (Tick)i * oldTicks;
            const auto j = (size_t)((tick + t / 2) / t);
            if (j < steps.size()) steps[j] = std::max(steps[j], old[i]);
        }
}

void StepPattern::setBars(int n)
{
    bars = std::clamp(n, 1, 999);
    steps.resize((size_t)totalSteps(), 0);
}

void StepPattern::copyBar(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= bars || to >= bars) return;
    const auto src = steps;
    for (int i = 0; i < (int)steps.size(); ++i)
        if (barOfStep(i) == to) {
            const Tick offset = (Tick)i * stepTicks - (Tick)to * barTicks;
            steps[(size_t)i] = valueAt(*this, src, (Tick)from * barTicks + offset);
        }
}

void StepPattern::clearBar(int b)
{
    for (int i = 0; i < (int)steps.size(); ++i)
        if (barOfStep(i) == b) steps[(size_t)i] = 0;
}

void StepPattern::duplicateBar(int b)
{
    if (b < 0 || b >= bars) return;
    // shift everything after bar b one bar to the right, then copy b into the gap
    const auto src = steps;
    setBars(bars + 1);
    for (int i = 0; i < (int)steps.size(); ++i) {
        const int bar = barOfStep(i);
        if (bar <= b) continue;
        steps[(size_t)i] = valueAt(*this, src, (Tick)i * stepTicks - barTicks);
    }
    copyBar(b, b + 1);
}

void StepPattern::deleteBar(int b)
{
    if (bars <= 1 || b < 0 || b >= bars) { clearBar(b); return; }
    const auto src = steps;
    for (int i = 0; i < (int)steps.size(); ++i)
        if (barOfStep(i) >= b) steps[(size_t)i] = valueAt(*this, src, (Tick)i * stepTicks + barTicks);
    setBars(bars - 1);
}

void MidiTrack::regenerateStepNotes()
{
    step.normalise();
    notes_.clear();
    nextId_ = 1;
    const Tick len = std::max<Tick>(1, step.stepTicks - step.stepTicks / 8); // short gap between hits
    for (int i = 0; i < (int)step.steps.size(); ++i)
        if (const int v = step.steps[(size_t)i]; v > 0) {
            Note n;
            n.pitch = step.pitch;
            n.velocity = v;
            n.start = step.stepStart(i);
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
