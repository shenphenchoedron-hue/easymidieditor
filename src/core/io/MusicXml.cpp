#include "io/MusicXml.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
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

// ============================================================== import

namespace {

// Minimal, non-validating XML DOM: elements, attributes, text, entities,
// CDATA. Skips the prolog, comments, processing instructions and DOCTYPE.
struct XNode {
    std::string name, text;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<XNode> children;

    const XNode* child(const char* n) const
    {
        for (auto& c : children) if (c.name == n) return &c;
        return nullptr;
    }
    std::string childText(const char* n, const std::string& def = {}) const
    {
        auto* c = child(n);
        return c ? c->text : def;
    }
    std::string attr(const char* n, const std::string& def = {}) const
    {
        for (auto& [k, v] : attrs) if (k == n) return v;
        return def;
    }
};

class XmlParser {
public:
    explicit XmlParser(const std::string& s) : src(s) {}

    bool parse(XNode& root, std::string& error)
    {
        try {
            if (src.compare(0, 3, "\xEF\xBB\xBF") == 0) pos = 3;
            skipMisc();
            if (peek() != '<') throw std::runtime_error("No root element");
            parseElement(root);
            return true;
        } catch (const std::exception& e) {
            error = std::string("Invalid XML: ") + e.what();
            return false;
        }
    }

private:
    const std::string& src;
    size_t pos = 0;

    char peek() const { return pos < src.size() ? src[pos] : '\0'; }
    bool starts(const char* s) const { return src.compare(pos, std::char_traits<char>::length(s), s) == 0; }
    void expect(char c)
    {
        if (peek() != c) throw std::runtime_error(std::string("expected '") + c + "' at offset " + std::to_string(pos));
        ++pos;
    }
    void skipWs() { while (pos < src.size() && std::isspace((unsigned char)src[pos])) ++pos; }
    void skipPast(const char* end)
    {
        const auto e = src.find(end, pos);
        if (e == std::string::npos) throw std::runtime_error("unterminated construct");
        pos = e + std::char_traits<char>::length(end);
    }
    void skipDoctype()
    {
        int depth = 0;
        for (; pos < src.size(); ++pos) {
            if (src[pos] == '[') ++depth;
            else if (src[pos] == ']') --depth;
            else if (src[pos] == '>' && depth <= 0) { ++pos; return; }
        }
        throw std::runtime_error("unterminated DOCTYPE");
    }
    // whitespace, <?...?>, <!--...-->, <!DOCTYPE ...>
    void skipMisc()
    {
        for (;;) {
            skipWs();
            if (starts("<?")) skipPast("?>");
            else if (starts("<!--")) skipPast("-->");
            else if (starts("<!DOCTYPE")) skipDoctype();
            else return;
        }
    }
    std::string parseName()
    {
        const size_t b = pos;
        while (pos < src.size() && !std::isspace((unsigned char)src[pos]) && src[pos] != '>' && src[pos] != '/' && src[pos] != '=') ++pos;
        if (b == pos) throw std::runtime_error("expected name at offset " + std::to_string(pos));
        return src.substr(b, pos - b);
    }
    static void appendUtf8(std::string& o, unsigned cp)
    {
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
        else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    }
    std::string decode(const std::string& s)
    {
        std::string o;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] != '&') { o += s[i]; continue; }
            const auto semi = s.find(';', i);
            if (semi == std::string::npos) { o += s[i]; continue; }
            const auto ent = s.substr(i + 1, semi - i - 1);
            if (ent == "lt") o += '<';
            else if (ent == "gt") o += '>';
            else if (ent == "amp") o += '&';
            else if (ent == "quot") o += '"';
            else if (ent == "apos") o += '\'';
            else if (!ent.empty() && ent[0] == '#') {
                try {
                    const unsigned cp = (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X'))
                        ? (unsigned)std::stoul(ent.substr(2), nullptr, 16) : (unsigned)std::stoul(ent.substr(1));
                    appendUtf8(o, cp);
                } catch (...) { o += s.substr(i, semi - i + 1); }
            } else o += s.substr(i, semi - i + 1);
            i = semi;
        }
        return o;
    }
    void parseElement(XNode& n)
    {
        expect('<');
        n.name = parseName();
        for (;;) {
            skipWs();
            if (starts("/>")) { pos += 2; return; }
            if (peek() == '>') { ++pos; break; }
            auto key = parseName();
            skipWs(); expect('='); skipWs();
            const char q = peek();
            if (q != '"' && q != '\'') throw std::runtime_error("expected quote at offset " + std::to_string(pos));
            ++pos;
            const auto e = src.find(q, pos);
            if (e == std::string::npos) throw std::runtime_error("unterminated attribute");
            n.attrs.emplace_back(std::move(key), decode(src.substr(pos, e - pos)));
            pos = e + 1;
        }
        std::string text;
        for (;;) {
            if (pos >= src.size()) throw std::runtime_error("unexpected end inside <" + n.name + ">");
            if (starts("</")) {
                pos += 2;
                const auto close = parseName();
                if (close != n.name) throw std::runtime_error("mismatched </" + close + "> for <" + n.name + ">");
                skipWs(); expect('>');
                break;
            }
            if (starts("<!--")) { skipPast("-->"); continue; }
            if (starts("<![CDATA[")) {
                pos += 9;
                const auto e = src.find("]]>", pos);
                if (e == std::string::npos) throw std::runtime_error("unterminated CDATA");
                text += src.substr(pos, e - pos);
                pos = e + 3;
                continue;
            }
            if (starts("<?")) { skipPast("?>"); continue; }
            if (peek() == '<') {
                n.children.emplace_back();
                parseElement(n.children.back());
                continue;
            }
            const auto e = src.find('<', pos);
            text += decode(src.substr(pos, (e == std::string::npos ? src.size() : e) - pos));
            pos = e == std::string::npos ? src.size() : e;
        }
        // trim
        const auto b = text.find_first_not_of(" \t\r\n");
        n.text = b == std::string::npos ? std::string() : text.substr(b, text.find_last_not_of(" \t\r\n") - b + 1);
    }
};

double toDouble(const std::string& s, double def)
{
    try { return s.empty() ? def : std::stod(s); } catch (...) { return def; }
}

int stepToPc(const std::string& step)
{
    if (step.empty()) return -1;
    switch (std::toupper((unsigned char)step[0])) {
        case 'C': return 0; case 'D': return 2; case 'E': return 4; case 'F': return 5;
        case 'G': return 7; case 'A': return 9; case 'B': return 11;
        default: return -1;
    }
}

} // namespace

MusicXmlImportResult MusicXml::read(const std::string& xml, model::Project& project, bool adoptTempo)
{
    MusicXmlImportResult r;
    XNode root;
    if (!XmlParser(xml).parse(root, r.error)) return r;
    if (root.name == "score-timewise") { r.error = "Timewise MusicXML is not supported (re-save it as partwise)."; return r; }
    if (root.name != "score-partwise") { r.error = "Not a MusicXML score (root element <" + root.name + ">)."; return r; }

    // part id -> name, MIDI channel
    std::map<std::string, std::pair<std::string, int>> partInfo;
    if (auto* pl = root.child("part-list"))
        for (auto& sp : pl->children) {
            if (sp.name != "score-part") continue;
            int ch = -1;
            if (auto* mi = sp.child("midi-instrument")) ch = (int)toDouble(mi->childText("midi-channel"), 0) - 1;
            partInfo[sp.attr("id")] = {sp.childText("part-name"), ch};
        }

    bool tempoSet = false, timeSet = false;
    int partIndex = 0;
    for (auto& part : root.children) {
        if (part.name != "part") continue;
        const auto info = partInfo[part.attr("id")];
        auto& track = project.addMidiTrack(info.first.empty() ? "Part " + std::to_string(partIndex + 1) : info.first);
        track.channel = info.second >= 0 && info.second < 16 ? info.second : partIndex % 16;
        ++partIndex;

        double divisions = 1;
        double measureStart = 0;          // in ticks
        std::vector<model::Note> notes;
        std::map<std::pair<int, std::string>, size_t> openTies; // (pitch, voice) -> index in notes

        for (auto& measure : part.children) {
            if (measure.name != "measure") continue;
            double pos = measureStart, maxPos = measureStart, lastStart = measureStart;
            auto ticks = [&](const std::string& d) { return toDouble(d, 0) * kPPQ / std::max(1e-9, divisions); };

            for (auto& el : measure.children) {
                if (el.name == "attributes") {
                    divisions = toDouble(el.childText("divisions"), divisions);
                    if (auto* k = el.child("key"); k && !r.hasKey && k->child("fifths")) {
                        const int fifths = (int)toDouble(k->childText("fifths"), 0);
                        const int majorPc = ((fifths * 7) % 12 + 12) % 12;
                        const bool minor = k->childText("mode") == "minor";
                        r.hasKey = true;
                        r.harmony.root = minor ? (majorPc + 9) % 12 : majorPc;
                        r.harmony.scaleId = minor ? "natural_minor" : "major";
                    }
                    if (auto* t = el.child("time"); t && !timeSet && adoptTempo) {
                        const int num = (int)toDouble(t->childText("beats"), 0);
                        const int den = (int)toDouble(t->childText("beat-type"), 0);
                        if (num > 0 && den > 0) { project.timeSig = {num, den}; timeSet = true; }
                    }
                } else if (el.name == "backup") {
                    pos -= ticks(el.childText("duration"));
                    pos = std::max(pos, measureStart);
                } else if (el.name == "forward") {
                    pos += ticks(el.childText("duration"));
                } else if (el.name == "direction" || el.name == "sound") {
                    const XNode* snd = el.name == "sound" ? &el : el.child("sound");
                    if (snd && !tempoSet && adoptTempo) {
                        const double bpm = toDouble(snd->attr("tempo"), 0);
                        if (bpm > 0) { project.tempoBpm = bpm; tempoSet = true; }
                    }
                } else if (el.name == "note") {
                    if (el.child("grace") || el.child("cue")) continue; // no duration / not played
                    const bool chord = el.child("chord") != nullptr;
                    const double dur = ticks(el.childText("duration"));
                    const double start = chord ? lastStart : pos;
                    if (!chord) { lastStart = pos; pos += dur; }
                    maxPos = std::max(maxPos, start + dur);

                    const auto* pitch = el.child("pitch");
                    if (el.child("rest") || !pitch) continue;
                    const int pc = stepToPc(pitch->childText("step"));
                    if (pc < 0) continue;
                    const int midi = (int)toDouble(pitch->childText("octave"), 4) * 12 + 12 + pc +
                                     (int)std::lround(toDouble(pitch->childText("alter"), 0));
                    if (midi < 0 || midi > 127 || dur <= 0) continue;

                    bool tieStart = false, tieStop = false;
                    for (auto& c : el.children)
                        if (c.name == "tie") { (c.attr("type") == "start" ? tieStart : tieStop) = true; }
                    const auto key = std::make_pair(midi, el.childText("voice", "1"));

                    const auto s = (Tick)std::llround(start), e = (Tick)std::llround(start + dur);
                    auto open = openTies.find(key);
                    if (tieStop && open != openTies.end() && std::llabs(notes[open->second].end() - s) <= kPPQ / 32) {
                        notes[open->second].length = e - notes[open->second].start; // continue the tied note
                        if (!tieStart) openTies.erase(open);
                        continue;
                    }
                    int vel = 90;
                    if (auto d = el.attr("dynamics"); !d.empty()) vel = std::clamp((int)std::lround(toDouble(d, 100) * 0.9), 1, 127);
                    notes.push_back({0, midi, vel, s, std::max<Tick>(1, e - s), -1});
                    if (tieStart) openTies[key] = notes.size() - 1;
                    else if (open != openTies.end()) openTies.erase(open);
                }
            }
            measureStart = std::max(maxPos, pos);
        }
        for (auto& n : notes) track.addNote(n);
        ++r.tracksCreated;
    }
    if (r.tracksCreated == 0) { r.error = "The score contains no parts."; return r; }
    r.ok = true;
    return r;
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
