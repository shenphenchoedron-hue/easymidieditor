#include "Test.h"
#include "io/Json.h"
#include "io/MidiFile.h"
#include "io/ProjectSerializer.h"
#include "model/Commands.h"

using namespace mc;
using namespace mc::model;

static Project sample()
{
    Project p;
    p.tempoBpm = 96;
    p.timeSig = {3, 4};
    p.loopEnabled = true;
    p.loopStart = 960;
    p.loopEnd = 7680;
    p.harmony = {2, "major", true};
    p.controllerMapping.notes[2] = 41;
    auto& t = p.addMidiTrack("Keys \"1\"");
    t.channel = 3; t.colour = 0xff5578d1; t.volume = 0.5f; t.pan = -0.25f; t.mute = true;
    t.plugin = {"VST3", "/x/Synth.vst3|1234", "Synth", "ACME", "AAEC", true};
    t.addNote({0, 57, 101, 0, 960, -1});
    t.addNote({0, 64, 80, 480, 240, 5});
    p.addMidiTrack("Bass").addNote({0, 33, 110, 1920, 960, -1});
    return p;
}

TEST(json_roundtrip)
{
    auto j = io::Json::parse(R"({"a":[1,2.5,"x\n\u00e6"],"b":{"c":true,"d":null}})");
    CHECK_EQ(j["a"].arr()[1].num(), 2.5);
    CHECK_EQ(j["a"].arr()[2].str(), std::string("x\n\xC3\xA6"));
    CHECK(j["b"]["c"].boolean());
    CHECK(j["missing"]["deep"].isNull());
    auto again = io::Json::parse(j.dump());
    CHECK_EQ(again.dump(0), j.dump(0));
}

TEST(project_roundtrip)
{
    auto p = sample();
    Project q;
    auto r = io::ProjectSerializer::fromString(io::ProjectSerializer::toString(p), q);
    CHECK(r.ok);
    CHECK_EQ(q.tempoBpm, 96.0);
    CHECK(q.timeSig == (TimeSignature{3, 4}));
    CHECK(q.loopEnabled);
    CHECK_EQ(q.loopEnd, (Tick)7680);
    CHECK_EQ(q.harmony.scaleId, std::string("major"));
    CHECK(q.harmony.chordMode);
    CHECK_EQ(q.controllerMapping.notes[2], 41);
    CHECK_EQ(q.tracks().size(), (size_t)2);
    auto* t = dynamic_cast<MidiTrack*>(q.tracks()[0].get());
    CHECK_EQ(t->name, std::string("Keys \"1\""));
    CHECK_EQ(t->channel, 3);
    CHECK_EQ(t->colour, 0xff5578d1u);
    CHECK(t->mute);
    CHECK_EQ(t->plugin.stateBase64, std::string("AAEC"));
    CHECK(t->plugin.bypassed);
    CHECK(t->notes() == dynamic_cast<MidiTrack*>(p.tracks()[0].get())->notes());
}

TEST(project_load_is_tolerant)
{
    Project q;
    CHECK(!io::ProjectSerializer::fromString("garbage", q).ok);
    auto r = io::ProjectSerializer::fromString(R"({"format":"midicomposer-project","version":99,"future":1,
        "tracks":[{"id":5,"type":"midi","name":"X","notes":[[0,960,60,100]],"plugin":{"format":"CLAP","identifier":"gone"}}]})", q);
    CHECK(r.ok);
    CHECK(r.newerThanApp);
    CHECK_EQ(q.tracks().size(), (size_t)1);
    CHECK_EQ(q.activeTrack, (TrackId)5);
    CHECK_EQ(q.midiTrack(5)->plugin.identifier, std::string("gone")); // kept even if plugin missing
}

TEST(midi_file_roundtrip)
{
    auto p = sample();
    auto bytes = io::MidiFile::write(p);
    Project q;
    auto r = io::MidiFile::read(bytes, q);
    CHECK(r.ok);
    CHECK_EQ(r.tracksCreated, 3); // track 1 has notes on ch 3 and ch 5 -> split
    CHECK_EQ(q.tempoBpm, 96.0);
    CHECK(q.timeSig == (TimeSignature{3, 4}));
    auto* t0 = dynamic_cast<MidiTrack*>(q.tracks()[0].get());
    CHECK_EQ(t0->channel, 3);
    CHECK_EQ(t0->notes().size(), (size_t)1);
    CHECK_EQ(t0->notes()[0].pitch, 57);
    CHECK_EQ(t0->notes()[0].length, (Tick)960);
    CHECK_EQ(t0->notes()[0].velocity, 101);
    auto* bass = dynamic_cast<MidiTrack*>(q.tracks()[2].get());
    CHECK_EQ(bass->name, std::string("Bass"));
    CHECK_EQ(bass->notes()[0].start, (Tick)1920);
}

TEST(midi_file_rejects_garbage)
{
    Project q;
    CHECK(!io::MidiFile::read({1, 2, 3}, q).ok);
}

TEST(step_sequencer_pattern_generates_notes_and_roundtrips)
{
    Project p;
    auto& t = p.addMidiTrack("Kick");
    t.stepGroup = 1;
    t.step.pitch = 36;
    t.step.numSteps = 8;
    t.step.repeats = 2;
    t.step.steps = {100, 0, 0, 0, 90, 0, 0, 0};
    t.regenerateStepNotes();
    CHECK_EQ(t.notes().size(), (size_t)4);
    CHECK_EQ(t.notes()[1].start, (Tick)(4 * kPPQ / 4));
    CHECK_EQ(t.notes()[2].start, (Tick)(8 * kPPQ / 4));
    CHECK_EQ(t.notes()[1].velocity, 90);

    Project q;
    CHECK(io::ProjectSerializer::fromString(io::ProjectSerializer::toString(p), q).ok);
    auto* l = q.midiTrack(t.id());
    CHECK(l && l->isStepLine());
    if (l) { CHECK(l->step == t.step); CHECK_EQ(l->notes().size(), (size_t)4); }

    UndoStack undo(p);
    auto after = t.step;
    after.steps[2] = 127;
    undo.perform(std::make_unique<SetStepPatternsCommand>(std::vector<SetStepPatternsCommand::Entry>{{t.id(), t.step}},
                                                          std::vector<SetStepPatternsCommand::Entry>{{t.id(), after}}));
    CHECK_EQ(p.midiTrack(t.id())->notes().size(), (size_t)6);
    undo.undo();
    CHECK_EQ(p.midiTrack(t.id())->notes().size(), (size_t)4);
}
