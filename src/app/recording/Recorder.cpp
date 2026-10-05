#include "recording/Recorder.h"
#include <algorithm>

namespace mc::recording {

void Recorder::start(model::Tick ls, model::Tick le, bool lp)
{
    std::lock_guard l(m);
    active = true; loop = lp; loopStart = ls; loopEnd = le;
    open.clear(); done.clear();
}

void Recorder::noteOn(int pitch, int velocity, model::Tick at)
{
    std::lock_guard l(m);
    if (!active) return;
    if (auto it = open.find(pitch); it != open.end()) {
        it->second.length = close(it->second, at);
        done.push_back(it->second);
    }
    model::Note n;
    n.pitch = pitch; n.velocity = velocity; n.start = at; n.length = 1;
    open[pitch] = n;
}

model::Tick Recorder::close(const model::Note& n, model::Tick at) const
{
    if (at < n.start) at = loop ? loopEnd : n.start + 1; // wrapped around the loop
    return std::max<model::Tick>(1, at - n.start);
}

void Recorder::noteOff(int pitch, model::Tick at)
{
    std::lock_guard l(m);
    auto it = open.find(pitch);
    if (it == open.end()) return;
    it->second.length = close(it->second, at);
    done.push_back(it->second);
    open.erase(it);
}

std::vector<model::Note> Recorder::finish(model::Tick at)
{
    std::lock_guard l(m);
    for (auto& [p, n] : open) { n.length = close(n, at); done.push_back(n); }
    open.clear();
    active = false;
    return std::move(done);
}

std::vector<model::Note> Recorder::pending() const
{
    std::lock_guard l(m);
    return done;
}

} // namespace mc::recording
