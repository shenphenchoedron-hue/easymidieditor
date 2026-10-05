#include "io/MusicXml.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace mc::io {

using model::Tick;
using model::kPPQ;

namespace {

constexpr int kDivisions = 8;                        // per quarter note (allows 32nds for x/32 time)
constexpr Tick kTicksPerDiv = kPPQ / kDivisions;     // 120
constexpr int kQuantumDivs = 2;                      // quantize to 16th notes

struct Interval { int start, end; };                 // in divisions, end exclusive

// One staff/voice: pitch -> merged, non-overlapping intervals.
using Voice = std::map<int, std::vector<Interval>>;

std::string escape(const std::string& s)
{
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'': o += "&apos;"; break;
            default: o += c;
        }
    }
    return o;
}

int quantize(Tick t)
{
    const Tick q = kTicksPerDiv * kQuantumDivs;
    return (int)(((t + q / 2) / q) * kQuantumDivs);
}

void addNote(Voice& v, int pitch, int start, int end)
{
    auto& list = v[pitch];
    list.push_back({start, end});
    std::sort(list.begin(), list.end(), [](auto& a, auto& b) { return a.start < b.start; });
    std::vector<Interval> merged;
    for (auto& i : list) {
        if (!merged.empty() && i.start <= merged.back().end) merged.back().end = std::max(merged.back().end, i.end);
        else merged.push_back(i);
    }
    list = std::move(merged);
}

struct Spelling { const char* step; int alter; };

Spelling spell(int pc, bool flats)
{
    static const Spelling sharp[12] = {{"C",0},{"C",1},{"D",0},{"D",1},{"E",0},{"F",0},{"F",1},{"G",0},{"G",1},{"A",0},{"A",1},{"B",0}};
    static const Spelling flat[12]  = {{"C",0},{"D",-1},{"D",0},{"E",-1},{"E",0},{"F",0},{"G",-1},{"G",0},{"A",-1},{"A",0},{"B",-1},{"B",0}};
    return flats ? flat[pc] : sharp[pc];
}

// Largest plain or dotted note value (in divisions) that fits in `len`.
struct Value { int divs; const char* type; bool dot; };
Value largestValue(int len)
{
    static const Value values[] = {
        {48, "whole", true}, {32, "whole", false}, {24, "half", true}, {16, "half", false},
        {12, "quarter", true}, {8, "quarter", false}, {6, "eighth", true}, {4, "eighth", false},
        {3, "16th", true}, {2, "16th", false}, {1, "32nd", false},
    };
    for (auto& v : values) if (v.divs <= len) return v;
    return values[std::size(values) - 1];
}

void writePitchNote(std::ostringstream& o, int pitch, bool chord, const Value& val, int staff, int voice,
                    bool tieStop, bool tieStart, bool flats)
{
    const auto sp = spell(pitch % 12, flats);
    o << "      <note>\n";
    if (chord) o << "        <chord/>\n";
    o << "        <pitch><step>" << sp.step << "</step>";
    if (sp.alter) o << "<alter>" << sp.alter << "</alter>";
    o << "<octave>" << (pitch / 12 - 1) << "</octave></pitch>\n";
    o << "        <duration>" << val.divs << "</duration>\n";
    if (tieStop) o << "        <tie type=\"stop\"/>\n";
    if (tieStart) o << "        <tie type=\"start\"/>\n";
    o << "        <voice>" << voice << "</voice>\n";
    o << "        <type>" << val.type << "</type>\n";
    if (val.dot) o << "        <dot/>\n";
    if (staff > 0) o << "        <staff>" << staff << "</staff>\n";
    if (tieStop || tieStart) {
        o << "        <notations>";
        if (tieStop) o << "<tied type=\"stop\"/>";
        if (tieStart) o << "<tied type=\"start\"/>";
        o << "</notations>\n";
    }
    o << "      </note>\n";
}

void writeRest(std::ostringstream& o, const Value& val, int staff, int voice, bool wholeMeasure, int measureDivs)
{
    o << "      <note>\n";
    if (wholeMeasure) {
        o << "        <rest measure=\"yes\"/>\n        <duration>" << measureDivs << "</duration>\n";
        o << "        <voice>" << voice << "</voice>\n";
    } else {
        o << "        <rest/>\n        <duration>" << val.divs << "</duration>\n";
        o << "        <voice>" << voice << "</voice>\n";
        o << "        <type>" << val.type << "</type>\n";
        if (val.dot) o << "        <dot/>\n";
    }
    if (staff > 0) o << "        <staff>" << staff << "</staff>\n";
    o << "      </note>\n";
}

// Writes one voice's content for the measure [mStart, mEnd).
void writeVoiceMeasure(std::ostringstream& o, const Voice& v, int mStart, int mEnd, int staff, int voiceNo, bool flats)
{
    // slice points
    std::set<int> cuts{mStart, mEnd};
    for (auto& [pitch, list] : v)
        for (auto& i : list) {
            if (i.start > mStart && i.start < mEnd) cuts.insert(i.start);
            if (i.end > mStart && i.end < mEnd) cuts.insert(i.end);
        }
    std::vector<int> pts(cuts.begin(), cuts.end());

    bool anyNote = false;
    for (auto& [pitch, list] : v)
        for (auto& i : list) if (i.start < mEnd && i.end > mStart) anyNote = true;
    if (!anyNote) { writeRest(o, {}, staff, voiceNo, true, mEnd - mStart); return; }

    for (size_t k = 0; k + 1 < pts.size(); ++k) {
        const int s = pts[k], e = pts[k + 1];
        struct Sounding { int pitch; bool fromBefore, toAfter; };
        std::vector<Sounding> chord;
        for (auto& [pitch, list] : v)
            for (auto& i : list)
                if (i.start <= s && i.end >= e) { chord.push_back({pitch, i.start < s, i.end > e}); break; }

        // split the slice into notatable values, tied together
        int pos = s;
        while (pos < e) {
            const auto val = largestValue(e - pos);
            const bool firstPiece = pos == s, lastPiece = pos + val.divs >= e;
            if (chord.empty()) writeRest(o, val, staff, voiceNo, false, 0);
            for (size_t c = 0; c < chord.size(); ++c) {
                const bool tieStop = firstPiece ? chord[c].fromBefore : true;
                const bool tieStart = lastPiece ? chord[c].toAfter : true;
                writePitchNote(o, chord[c].pitch, c > 0, val, staff, voiceNo, tieStop, tieStart, flats);
            }
            pos += val.divs;
        }
    }
}

int pitchClassFifths(int pc)
{
    static const int f[12] = {0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5}; // C Db D Eb E F F# G Ab A Bb B
    return f[((pc % 12) + 12) % 12];
}

} // namespace

int MusicXml::keyFifths(const model::HarmonySettings& h)
{
    // Offset from the mode's tonic to its relative major.
    static const std::map<std::string, int> toMajor = {
        {"major", 0}, {"natural_minor", 3}, {"harmonic_minor", 3}, {"melodic_minor", 3},
        {"dorian", 10}, {"phrygian", 8}, {"lydian", 7}, {"mixolydian", 5}, {"locrian", 1},
    };
    auto it = toMajor.find(h.scaleId);
    return pitchClassFifths(h.root + (it == toMajor.end() ? 0 : it->second));
}

std::string MusicXml::write(const model::Project& p)
{
    const int num = std::max(1, p.timeSig.numerator);
    const int den = std::max(1, p.timeSig.denominator);
    const int measureDivs = std::max(1, num * kDivisions * 4 / den);
    const int fifths = keyFifths(p.harmony);
    const bool flats = fifths < 0;
    const bool minor = p.harmony.scaleId.find("minor") != std::string::npos;

    std::vector<const model::MidiTrack*> tracks;
    for (auto& t : p.tracks())
        if (auto* mt = dynamic_cast<const model::MidiTrack*>(t.get())) tracks.push_back(mt);

    int songEnd = 0;
    for (auto* t : tracks)
        for (auto& n : t->notes()) songEnd = std::max(songEnd, std::max(quantize(n.end()), quantize(n.start) + kQuantumDivs));
    const int measures = std::max(1, (songEnd + measureDivs - 1) / measureDivs);

    std::ostringstream o;
    o << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
         "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" "
         "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
         "<score-partwise version=\"4.0\">\n"
         "  <identification><encoding><software>MIDI Composer</software></encoding></identification>\n"
         "  <part-list>\n";
    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto name = tracks[i]->name.empty() ? "Track " + std::to_string(i + 1) : tracks[i]->name;
        o << "    <score-part id=\"P" << i + 1 << "\">\n"
          << "      <part-name>" << escape(name) << "</part-name>\n"
          << "      <midi-instrument id=\"P" << i + 1 << "-I1\"><midi-channel>" << tracks[i]->channel + 1
          << "</midi-channel></midi-instrument>\n"
          << "    </score-part>\n";
    }
    o << "  </part-list>\n";

    for (size_t ti = 0; ti < tracks.size(); ++ti) {
        auto* t = tracks[ti];
        int lo = 127, hi = 0;
        for (auto& n : t->notes()) { lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); }
        const bool empty = t->notes().empty();
        const bool grand = !empty && lo < 55 && hi >= 65;

        Voice upper, lower;
        for (auto& n : t->notes()) {
            const int s = quantize(n.start);
            const int e = std::max(s + kQuantumDivs, quantize(n.end()));
            addNote(grand && n.pitch < 60 ? lower : upper, n.pitch, s, e);
        }
        bool bassClef = false;
        if (!grand && !empty) {
            double sum = 0;
            for (auto& n : t->notes()) sum += n.pitch;
            bassClef = sum / (double)t->notes().size() < 55.0;
        }

        o << "  <part id=\"P" << ti + 1 << "\">\n";
        for (int m = 0; m < measures; ++m) {
            o << "    <measure number=\"" << m + 1 << "\">\n";
            if (m == 0) {
                o << "      <attributes>\n"
                  << "        <divisions>" << kDivisions << "</divisions>\n"
                  << "        <key><fifths>" << fifths << "</fifths><mode>" << (minor ? "minor" : "major") << "</mode></key>\n"
                  << "        <time><beats>" << num << "</beats><beat-type>" << den << "</beat-type></time>\n";
                if (grand)
                    o << "        <staves>2</staves>\n"
                         "        <clef number=\"1\"><sign>G</sign><line>2</line></clef>\n"
                         "        <clef number=\"2\"><sign>F</sign><line>4</line></clef>\n";
                else if (bassClef)
                    o << "        <clef><sign>F</sign><line>4</line></clef>\n";
                else
                    o << "        <clef><sign>G</sign><line>2</line></clef>\n";
                o << "      </attributes>\n";
                if (ti == 0)
                    o << "      <direction placement=\"above\">\n"
                         "        <direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>"
                      << (int)(p.tempoBpm + 0.5) << "</per-minute></metronome></direction-type>\n"
                      << "        <sound tempo=\"" << p.tempoBpm << "\"/>\n"
                         "      </direction>\n";
            }
            const int ms = m * measureDivs, me = ms + measureDivs;
            if (grand) {
                writeVoiceMeasure(o, upper, ms, me, 1, 1, flats);
                o << "      <backup><duration>" << measureDivs << "</duration></backup>\n";
                writeVoiceMeasure(o, lower, ms, me, 2, 2, flats);
            } else {
                writeVoiceMeasure(o, upper, ms, me, 0, 1, flats);
            }
            o << "    </measure>\n";
        }
        o << "  </part>\n";
    }
    o << "</score-partwise>\n";
    return o.str();
}

bool MusicXml::writeFile(const model::Project& p, const std::string& path, std::string* error)
{
    const auto xml = write(p);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) { if (error) *error = "Cannot write " + path; return false; }
    f << xml;
    return (bool)f;
}

} // namespace mc::io
