#include "model/Project.h"
#include <algorithm>

namespace mc::model {

Project::Project() = default;

Project::Project(const Project& o) { *this = o; }

Project& Project::operator=(const Project& o)
{
    if (this == &o) return *this;
    tracks_.clear();
    for (auto& t : o.tracks_) tracks_.push_back(t->clone());
    nextTrackId_ = o.nextTrackId_;
    activeTrack = o.activeTrack;
    tempoBpm = o.tempoBpm;
    timeSig = o.timeSig;
    playhead = o.playhead;
    loopEnabled = o.loopEnabled;
    loopStart = o.loopStart;
    loopEnd = o.loopEnd;
    harmony = o.harmony;
    controllerMapping = o.controllerMapping;
    // listeners are intentionally not copied
    return *this;
}

Track* Project::track(TrackId id) const
{
    for (auto& t : tracks_) if (t->id() == id) return t.get();
    return nullptr;
}

MidiTrack* Project::midiTrack(TrackId id) const { return dynamic_cast<MidiTrack*>(track(id)); }

int Project::indexOf(TrackId id) const
{
    for (size_t i = 0; i < tracks_.size(); ++i) if (tracks_[i]->id() == id) return (int)i;
    return -1;
}

MidiTrack& Project::addMidiTrack(const std::string& name)
{
    const TrackId id = nextTrackId_++;
    auto t = std::make_unique<MidiTrack>(id, name.empty() ? "Track " + std::to_string(tracks_.size() + 1) : name);
    t->channel = (int)(tracks_.size() % 16);
    auto& ref = *t;
    tracks_.push_back(std::move(t));
    if (activeTrack == 0) activeTrack = id;
    return ref;
}

void Project::insertTrack(int index, std::unique_ptr<Track> t)
{
    nextTrackId_ = std::max(nextTrackId_, t->id() + 1);
    index = std::clamp(index, 0, (int)tracks_.size());
    tracks_.insert(tracks_.begin() + index, std::move(t));
}

std::unique_ptr<Track> Project::removeTrack(TrackId id)
{
    const int i = indexOf(id);
    if (i < 0) return nullptr;
    auto t = std::move(tracks_[(size_t)i]);
    tracks_.erase(tracks_.begin() + i);
    if (activeTrack == id) activeTrack = tracks_.empty() ? 0 : tracks_[(size_t)std::min(i, (int)tracks_.size() - 1)]->id();
    return t;
}

int Project::addListener(Listener l)
{
    listeners_.emplace_back(nextListener_, std::move(l));
    return nextListener_++;
}

void Project::removeListener(int h)
{
    std::erase_if(listeners_, [h](auto& p) { return p.first == h; });
}

void Project::notifyChanged() const
{
    auto copy = listeners_;
    for (auto& [h, l] : copy) if (l) l();
}

} // namespace mc::model
