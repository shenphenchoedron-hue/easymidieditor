#pragma once
#include <cstdint>

namespace mc::model {

using Tick = std::int64_t;              // musical time, PPQ ticks
inline constexpr int kPPQ = 960;        // ticks per quarter note
using NoteId = std::uint64_t;

// A plain MIDI note. Chords are always stored as individual Notes.
struct Note {
    NoteId id = 0;
    int pitch = 60;           // 0..127
    int velocity = 100;       // 1..127
    Tick start = 0;
    Tick length = kPPQ;
    int channel = -1;         // -1 = use track channel, otherwise 0..15

    Tick end() const { return start + length; }
    bool operator==(const Note&) const = default;
};

} // namespace mc::model
