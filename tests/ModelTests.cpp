#include "Test.h"
#include "model/Commands.h"
#include "sequencer/Sequence.h"
#include "sequencer/Timing.h"

using namespace mc::model;
using namespace mc::seq;

TEST(tracks_are_independent)
{
    Project p;
    auto& a = p.addMidiTrack("A");
    auto& b = p.addMidiTrack("B");
    a.addNote({0, 60, 100, 0, 480, -1});
    CHECK_EQ(a.notes().size(), (size_t)1);
    CHECK_EQ(b.notes().size(), (size_t)0);
}

TEST(undo_redo_notes_and_tracks)
{
    Project p;
    UndoStack u(p);
    u.perform(std::make_unique<AddTrackCommand>("Piano"));
    const auto tid = p.activeTrack;
    u.perform(std::make_unique<AddNotesCommand>(tid, std::vector<Note>{{0, 57, 100, 0, 960, -1}, {0, 60, 100, 0, 960, -1}}));
    CHECK_EQ(p.midiTrack(tid)->notes().size(), (size_t)2);

    auto before = p.midiTrack(tid)->notes()[0];
    auto after = before; after.start = 960; after.length = 480;
    u.perform(std::make_unique<ModifyNotesCommand>(tid, std::vector<Note>{before}, std::vector<Note>{after}));
    CHECK_EQ(p.midiTrack(tid)->findNote(before.id)->start, (Tick)960);
    u.undo();
    CHECK_EQ(p.midiTrack(tid)->findNote(before.id)->start, (Tick)0);
    u.undo();
    CHECK_EQ(p.midiTrack(tid)->notes().size(), (size_t)0);
    u.redo();
    CHECK_EQ(p.midiTrack(tid)->notes().size(), (size_t)2);

    u.perform(std::make_unique<RemoveTrackCommand>(tid));
    CHECK(p.tracks().empty());
    u.undo();
    CHECK_EQ(p.midiTrack(tid)->notes().size(), (size_t)2);
}

TEST(timing)
{
    TimeSignature ts{4, 4};
    CHECK_EQ(ticksPerBar(ts), (Tick)3840);
    CHECK_EQ(ticksToSeconds(960, 120), 0.5);
    auto bb = toBarBeat(3840 + 960 * 2 + 10, ts);
    CHECK_EQ(bb.bar, 2); CHECK_EQ(bb.beat, 3); CHECK_EQ(bb.tick, (Tick)10);
    CHECK_EQ((GridValue{16}.ticks()), (Tick)240);
    CHECK_EQ((GridValue{8, true}.ticks()), (Tick)320);
    CHECK_EQ(snapNearest(250, 240), (Tick)240);
    CHECK_EQ(snapFloor(479, 240), (Tick)240);
    CHECK_EQ(ticksPerBeat({6, 8}), (Tick)480);
}

TEST(sequence_snapshot_mute_solo_and_order)
{
    Project p;
    auto& a = p.addMidiTrack("A");
    auto& b = p.addMidiTrack("B");
    a.addNote({0, 60, 100, 0, 480, -1});
    a.addNote({0, 60, 100, 480, 480, -1});
    b.addNote({0, 64, 90, 0, 960, -1});
    b.solo = true;
    auto s = SequenceSnapshot::build(p);
    CHECK(!s.tracks[0].audible);
    CHECK(s.tracks[1].audible);
    // note-off at 480 precedes the next note-on at 480
    CHECK(!s.tracks[0].events[1].noteOn);
    CHECK(s.tracks[0].events[2].noteOn);
    int n = 0;
    s.tracks[0].forEachInRange(480, 960, [&](const SeqEvent&) { ++n; });
    CHECK_EQ(n, 2);
    CHECK_EQ(s.lengthTicks, (Tick)960);
}
