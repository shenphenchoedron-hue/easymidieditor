#include "sequencer/Sequence.h"
#include <algorithm>

namespace mc::seq {

std::vector<SeqEvent>::const_iterator TrackSequence::lowerBound(Tick t) const
{
    return std::lower_bound(events.begin(), events.end(), t, [](const SeqEvent& e, Tick v) { return e.tick < v; });
}

SequenceSnapshot SequenceSnapshot::build(const model::Project& p)
{
    SequenceSnapshot s;
    const bool anySolo = std::any_of(p.tracks().begin(), p.tracks().end(), [](auto& t) { return t->solo; });
    for (auto& t : p.tracks()) {
        auto* mt = dynamic_cast<const model::MidiTrack*>(t.get());
        if (!mt) continue;
        TrackSequence ts;
        ts.trackId = mt->id();
        ts.audible = !mt->mute && (!anySolo || mt->solo);
        ts.volume = mt->volume;
        ts.pan = mt->pan;
        ts.channel = mt->channel;
        for (auto& n : mt->notes()) {
            const auto ch = (std::uint8_t)(n.channel >= 0 ? n.channel : mt->channel);
            ts.events.push_back({n.start, true, ch, (std::uint8_t)n.pitch, (std::uint8_t)n.velocity});
            ts.events.push_back({n.end(), false, ch, (std::uint8_t)n.pitch, 0});
            s.lengthTicks = std::max(s.lengthTicks, n.end());
        }
        std::stable_sort(ts.events.begin(), ts.events.end(), [](const SeqEvent& a, const SeqEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return !a.noteOn && b.noteOn; // offs first
        });
        s.tracks.push_back(std::move(ts));
    }
    return s;
}

} // namespace mc::seq
