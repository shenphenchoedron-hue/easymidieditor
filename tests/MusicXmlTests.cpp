#include "Test.h"
#include "io/MusicXml.h"
#include <algorithm>

using namespace mc;
using namespace mc::model;

namespace {
int count(const std::string& s, const std::string& sub)
{
    int n = 0;
    for (size_t p = s.find(sub); p != std::string::npos; p = s.find(sub, p + 1)) ++n;
    return n;
}
bool has(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }
}

TEST(musicxml_key_signatures)
{
    CHECK_EQ(io::MusicXml::keyFifths({0, "major", false}), 0);          // C
    CHECK_EQ(io::MusicXml::keyFifths({9, "natural_minor", false}), 0);  // Am
    CHECK_EQ(io::MusicXml::keyFifths({7, "major", false}), 1);          // G
    CHECK_EQ(io::MusicXml::keyFifths({5, "major", false}), -1);         // F
    CHECK_EQ(io::MusicXml::keyFifths({2, "natural_minor", false}), -1); // Dm
    CHECK_EQ(io::MusicXml::keyFifths({2, "dorian", false}), 0);         // D dorian
}

TEST(musicxml_c_major_chord_in_one_measure)
{
    Project p;
    p.harmony = {0, "major", false};
    auto& t = p.addMidiTrack("Piano & Co");
    for (int pitch : {60, 64, 67}) t.addNote({0, pitch, 100, 0, 4 * kPPQ, -1}); // whole-note C E G
    const auto x = io::MusicXml::write(p);
    CHECK(has(x, "<score-partwise version=\"4.0\">"));
    CHECK(has(x, "<part-name>Piano &amp; Co</part-name>"));
    CHECK_EQ(count(x, "<measure number="), 1);
    CHECK_EQ(count(x, "<type>whole</type>"), 3);
    CHECK_EQ(count(x, "<chord/>"), 2);
    CHECK(!has(x, "<tie"));
    CHECK(has(x, "<step>E</step><octave>4</octave>"));
    CHECK(has(x, "<fifths>0</fifths><mode>major</mode>"));
}

TEST(musicxml_ties_across_barline_and_rests)
{
    Project p;
    auto& t = p.addMidiTrack("Lead");
    t.addNote({0, 72, 100, 3 * kPPQ, 2 * kPPQ, -1}); // beat 4 of bar 1 to beat 1 of bar 2
    const auto x = io::MusicXml::write(p);
    CHECK_EQ(count(x, "<measure number="), 2);
    CHECK_EQ(count(x, "<tie type=\"start\"/>"), 1);
    CHECK_EQ(count(x, "<tie type=\"stop\"/>"), 1);
    CHECK(has(x, "<rest/>"));
}

TEST(musicxml_grand_staff_for_wide_range)
{
    Project p;
    auto& t = p.addMidiTrack("Piano");
    t.addNote({0, 40, 100, 0, kPPQ, -1});
    t.addNote({0, 76, 100, 0, kPPQ, -1});
    const auto x = io::MusicXml::write(p);
    CHECK(has(x, "<staves>2</staves>"));
    CHECK(has(x, "<backup>"));
    CHECK(has(x, "<staff>2</staff>"));
}

TEST(musicxml_flat_key_spells_flats)
{
    Project p;
    p.harmony = {5, "major", false}; // F major
    p.addMidiTrack("x").addNote({0, 70, 100, 0, kPPQ, -1}); // Bb4
    const auto x = io::MusicXml::write(p);
    CHECK(has(x, "<step>B</step><alter>-1</alter><octave>4</octave>"));
}

namespace {
std::vector<Note> notesOf(const Project& p, size_t trackIndex)
{
    auto* t = dynamic_cast<const MidiTrack*>(p.tracks()[trackIndex].get());
    std::vector<Note> v(t->notes().begin(), t->notes().end());
    for (auto& n : v) { n.id = 0; n.velocity = 0; } // compare timing/pitch only
    std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; });
    return v;
}
}
namespace mc::model {
std::ostream& operator<<(std::ostream& o, const Note& n) { return o << "(" << n.pitch << "@" << n.start << "+" << n.length << ")"; }
}

TEST(musicxml_roundtrip_notes_ties_grand_staff)
{
    Project src;
    src.tempoBpm = 96;
    src.timeSig = {3, 4};
    src.harmony = {2, "natural_minor", false}; // D minor
    auto& t = src.addMidiTrack("Piano");
    t.addNote({0, 62, 100, 0, kPPQ, -1});                  // chord D F A on beat 1
    t.addNote({0, 65, 100, 0, kPPQ, -1});
    t.addNote({0, 69, 100, 0, kPPQ, -1});
    t.addNote({0, 74, 100, 2 * kPPQ, 3 * kPPQ, -1});       // tied across the barline
    t.addNote({0, 38, 100, 0, 6 * kPPQ, -1});              // long bass note: lower staff, 2 bars
    t.addNote({0, 70, 100, kPPQ + kPPQ / 2, kPPQ / 2, -1}); // Bb, off-beat eighth
    src.addMidiTrack("Bass").addNote({0, 45, 100, kPPQ, kPPQ, -1});

    Project dst;
    auto r = io::MusicXml::read(io::MusicXml::write(src), dst);
    CHECK(r.ok);
    CHECK_EQ(r.tracksCreated, 2);
    CHECK_EQ(dst.tempoBpm, 96.0);
    CHECK(dst.timeSig == (TimeSignature{3, 4}));
    CHECK(r.hasKey);
    CHECK_EQ(r.harmony.root, 2);
    CHECK_EQ(r.harmony.scaleId, std::string("natural_minor"));
    CHECK_EQ(dst.tracks()[0]->name, std::string("Piano"));
    CHECK_EQ(notesOf(dst, 0), notesOf(src, 0));
    CHECK_EQ(notesOf(dst, 1), notesOf(src, 1));
}

TEST(musicxml_import_handwritten_score)
{
    // Typical MuseScore-style output: DOCTYPE, comments, entities, divisions 2,
    // pickup measure, rests, chord, forward, grace note.
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="4.0">
  <!-- comment -->
  <part-list><score-part id="P1"><part-name>Fl&#248;jte &amp; co</part-name></score-part></part-list>
  <part id="P1">
    <measure number="0" implicit="yes">
      <attributes><divisions>2</divisions><key><fifths>1</fifths></key><time><beats>4</beats><beat-type>4</beat-type></time></attributes>
      <direction><sound tempo="72"/></direction>
      <note><pitch><step>D</step><octave>5</octave></pitch><duration>2</duration><voice>1</voice><type>quarter</type></note>
    </measure>
    <measure number="1">
      <note><grace/><pitch><step>A</step><octave>4</octave></pitch><voice>1</voice></note>
      <note><pitch><step>G</step><octave>4</octave></pitch><duration>4</duration><voice>1</voice></note>
      <note><chord/><pitch><step>B</step><octave>4</octave></pitch><duration>4</duration><voice>1</voice></note>
      <note><rest/><duration>1</duration><voice>1</voice></note>
      <forward><duration>1</duration></forward>
      <note><pitch><step>F</step><alter>1</alter><octave>4</octave></pitch><duration>2</duration><voice>1</voice></note>
    </measure>
  </part>
</score-partwise>)";
    Project p;
    auto r = io::MusicXml::read(xml, p);
    CHECK(r.ok);
    CHECK_EQ(p.tracks()[0]->name, std::string("Fl\xC3\xB8jte & co"));
    CHECK_EQ(p.tempoBpm, 72.0);
    CHECK_EQ(r.harmony.root, 7); // G major
    CHECK_EQ(r.harmony.scaleId, std::string("major"));
    std::vector<Note> expect = {
        {0, 74, 0, 0, kPPQ, -1},                 // D5 pickup
        {0, 67, 0, kPPQ, 2 * kPPQ, -1},          // G4 half (bar 1 starts after the 1-beat pickup)
        {0, 71, 0, kPPQ, 2 * kPPQ, -1},          // B4 chord
        {0, 66, 0, 4 * kPPQ, kPPQ, -1},          // F#4 after eighth rest + eighth forward
    };
    CHECK_EQ(notesOf(p, 0), expect);
}

TEST(musicxml_import_rejects_garbage)
{
    Project p;
    CHECK(!io::MusicXml::read("not xml", p).ok);
    CHECK(!io::MusicXml::read("<html><body/></html>", p).ok);
    CHECK(!io::MusicXml::read("<score-partwise><part id=\"P1\"><measure>", p).ok);
    CHECK(p.tracks().empty());
}

TEST(musicxml_measure_durations_add_up)
{
    // Every measure of a single-staff part must sum to the measure length.
    Project p;
    p.timeSig = {3, 4};
    auto& t = p.addMidiTrack("x");
    t.addNote({0, 60, 100, 100, 700, -1});   // off-grid, gets quantized
    t.addNote({0, 64, 100, 500, 2000, -1});  // overlaps, crosses barline
    const auto x = io::MusicXml::write(p);
    size_t pos = 0;
    while ((pos = x.find("<measure number=", pos)) != std::string::npos) {
        const auto end = x.find("</measure>", pos);
        const auto m = x.substr(pos, end - pos);
        int sum = 0;
        for (size_t q = m.find("<note>"); q != std::string::npos; q = m.find("<note>", q + 1)) {
            const auto noteEnd = m.find("</note>", q);
            const auto n = m.substr(q, noteEnd - q);
            if (has(n, "<chord/>")) continue;
            const auto d = n.find("<duration>");
            sum += std::stoi(n.substr(d + 10));
        }
        CHECK_EQ(sum, 3 * 8);
        pos = end;
    }
}
