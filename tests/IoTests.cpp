#include "Test.h"
#include <clocale>
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

TEST(step_sequencer_bars_are_independent_and_roundtrip)
{
    Project p;
    auto& t = p.addMidiTrack("Kick");
    t.stepGroup = 1;
    t.step.pitch = 36;
    t.step.bars = 2;                       // 2 bars of 16 x 1/16
    t.step.normalise();
    CHECK_EQ(t.step.totalSteps(), 32);
    t.step.steps[0] = 100; t.step.steps[4] = 90;   // bar 1
    t.step.steps[16 + 8] = 110;                    // bar 2: different
    t.regenerateStepNotes();
    CHECK_EQ(t.notes().size(), (size_t)3);
    CHECK_EQ(t.notes()[2].start, (Tick)((16 + 8) * kPPQ / 4));

    Project q;
    CHECK(io::ProjectSerializer::fromString(io::ProjectSerializer::toString(p), q).ok);
    auto* l = q.midiTrack(t.id());
    CHECK(l && l->isStepLine());
    if (l) { CHECK(l->step == t.step); CHECK_EQ(l->notes().size(), (size_t)3); }

    UndoStack undo(p);
    auto after = t.step;
    after.steps[2] = 127;
    undo.perform(std::make_unique<SetStepPatternsCommand>(std::vector<SetStepPatternsCommand::Entry>{{t.id(), t.step}},
                                                          std::vector<SetStepPatternsCommand::Entry>{{t.id(), after}}));
    CHECK_EQ(p.midiTrack(t.id())->notes().size(), (size_t)4);
    undo.undo();
    CHECK_EQ(p.midiTrack(t.id())->notes().size(), (size_t)3);
}

TEST(step_sequencer_bar_operations)
{
    StepPattern s;
    s.bars = 2;
    s.normalise();
    s.steps[0] = 100; s.steps[16 + 3] = 50;
    s.duplicateBar(0);                     // bars: A, A, B
    CHECK_EQ(s.bars, 3);
    CHECK_EQ((int)s.steps[16], 100);
    CHECK_EQ((int)s.steps[32 + 3], 50);
    s.steps[16 + 2] = 70;                  // vary bar 2 only
    CHECK_EQ((int)s.steps[2], 0);
    s.clearBar(0);
    CHECK_EQ((int)s.steps[0], 0);
    CHECK_EQ((int)s.steps[16], 100);
    s.deleteBar(0);                        // bars: A', B
    CHECK_EQ(s.bars, 2);
    CHECK_EQ((int)s.steps[0], 100);
    CHECK_EQ((int)s.steps[2], 70);
    CHECK_EQ((int)s.steps[16 + 3], 50);
    s.copyBar(1, 0);
    CHECK_EQ((int)s.steps[3], 50);
    CHECK_EQ((int)s.steps[0], 0);
    s.setStepTicks(kPPQ / 8);              // 1/32: rhythm kept
    CHECK_EQ(s.totalSteps(), 64);
    CHECK_EQ((int)s.steps[6], 50);
}

TEST(step_sequencer_old_format_is_converted_to_bars)
{
    const std::string old = R"({"format":"midicomposer-project","version":1,"ppq":960,"timeSignature":{"numerator":4,"denominator":4},
        "tracks":[{"id":1,"name":"Kick","type":"midi","stepGroup":1,
        "step":{"pitch":36,"numSteps":16,"stepTicks":240,"repeats":3,"start":0,"steps":[100,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}}]})";
    Project q;
    CHECK(io::ProjectSerializer::fromString(old, q).ok);
    auto* l = q.midiTrack(1);
    CHECK(l != nullptr);
    if (l) {
        CHECK_EQ(l->step.bars, 3);
        CHECK_EQ(l->notes().size(), (size_t)3);
        CHECK_EQ((int)l->step.steps[32], 100);
    }
}

TEST(json_numbers_ignore_c_locale)
{
    // GTK sets the locale from the environment; with e.g. da_DK, strtod/printf use ','.
    const char* old = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = old ? old : "C";
    if (std::setlocale(LC_NUMERIC, "da_DK.UTF-8") || std::setlocale(LC_NUMERIC, "de_DE.UTF-8")) {
        const auto j = io::Json::parse("{\"a\": 120, \"b\": 0.8}");
        CHECK_EQ(j["a"].num(0), 120.0);
        CHECK_EQ(j["b"].num(0), 0.8);
        io::Json o;
        o.set("v", 0.25);
        CHECK(o.dump(0).find("0.25") != std::string::npos);
    }
    std::setlocale(LC_NUMERIC, saved.c_str());
}
