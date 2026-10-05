#include "io/MidiFile.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace mc::io {

using namespace mc::model;

namespace {

void be32(std::vector<std::uint8_t>& o, std::uint32_t v) { for (int s = 24; s >= 0; s -= 8) o.push_back((std::uint8_t)(v >> s)); }
void be16(std::vector<std::uint8_t>& o, std::uint16_t v) { o.push_back((std::uint8_t)(v >> 8)); o.push_back((std::uint8_t)v); }
void vlq(std::vector<std::uint8_t>& o, std::uint32_t v)
{
    std::uint8_t buf[5]; int n = 0;
    buf[n++] = v & 0x7F;
    while (v >>= 7) buf[n++] = (std::uint8_t)(0x80 | (v & 0x7F));
    while (n) o.push_back(buf[--n]);
}

struct Ev { Tick t; int order; std::vector<std::uint8_t> bytes; };

void writeTrack(std::vector<std::uint8_t>& out, std::vector<Ev> evs)
{
    std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.t != b.t ? a.t < b.t : a.order < b.order; });
    std::vector<std::uint8_t> body;
    Tick last = 0;
    for (auto& e : evs) { vlq(body, (std::uint32_t)(e.t - last)); last = e.t; body.insert(body.end(), e.bytes.begin(), e.bytes.end()); }
    vlq(body, 0); body.insert(body.end(), {0xFF, 0x2F, 0x00});
    out.insert(out.end(), {'M', 'T', 'r', 'k'});
    be32(out, (std::uint32_t)body.size());
    out.insert(out.end(), body.begin(), body.end());
}

std::vector<std::uint8_t> metaText(std::uint8_t type, const std::string& s)
{
    std::vector<std::uint8_t> b{0xFF, type};
    vlq(b, (std::uint32_t)s.size());
    b.insert(b.end(), s.begin(), s.end());
    return b;
}

struct Reader {
    const std::vector<std::uint8_t>& d;
    size_t i;
    size_t end;
    std::uint8_t u8() { if (i >= end) throw std::runtime_error("truncated"); return d[i++]; }
    std::uint32_t u32() { std::uint32_t v = 0; for (int k = 0; k < 4; ++k) v = (v << 8) | u8(); return v; }
    std::uint16_t u16() { return (std::uint16_t)((u8() << 8) | u8()); }
    std::uint32_t vlq() { std::uint32_t v = 0; for (int k = 0; k < 4; ++k) { auto b = u8(); v = (v << 7) | (b & 0x7F); if (!(b & 0x80)) return v; } throw std::runtime_error("bad vlq"); }
};

} // namespace

std::vector<std::uint8_t> MidiFile::write(const Project& p)
{
    std::vector<const MidiTrack*> tracks;
    for (auto& t : p.tracks()) if (auto* m = dynamic_cast<const MidiTrack*>(t.get())) tracks.push_back(m);

    std::vector<std::uint8_t> out{'M', 'T', 'h', 'd'};
    be32(out, 6); be16(out, 1); be16(out, (std::uint16_t)(tracks.size() + 1)); be16(out, (std::uint16_t)kPPQ);

    // Conductor track
    std::vector<Ev> conductor;
    const auto usPerQ = (std::uint32_t)std::lround(60'000'000.0 / p.tempoBpm);
    conductor.push_back({0, 0, {0xFF, 0x51, 0x03, (std::uint8_t)(usPerQ >> 16), (std::uint8_t)(usPerQ >> 8), (std::uint8_t)usPerQ}});
    int dpow = 0; while ((1 << dpow) < p.timeSig.denominator) ++dpow;
    conductor.push_back({0, 1, {0xFF, 0x58, 0x04, (std::uint8_t)p.timeSig.numerator, (std::uint8_t)dpow, 24, 8}});
    writeTrack(out, conductor);

    for (auto* m : tracks) {
        std::vector<Ev> evs;
        evs.push_back({0, -2, metaText(0x03, m->name)});
        if (!m->plugin.name.empty()) evs.push_back({0, -1, metaText(0x04, m->plugin.name)});
        for (auto& n : m->notes()) {
            const auto ch = (std::uint8_t)((n.channel >= 0 ? n.channel : m->channel) & 15);
            evs.push_back({n.end(), 0, {(std::uint8_t)(0x80 | ch), (std::uint8_t)n.pitch, 0}});
            evs.push_back({n.start, 1, {(std::uint8_t)(0x90 | ch), (std::uint8_t)n.pitch, (std::uint8_t)n.velocity}});
        }
        writeTrack(out, evs);
    }
    return out;
}

bool MidiFile::writeFile(const Project& p, const std::string& path, std::string* error)
{
    auto bytes = write(p);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) { if (error) *error = "Cannot write " + path; return false; }
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    return (bool)f;
}

MidiImportResult MidiFile::read(const std::vector<std::uint8_t>& d, Project& project, bool adoptTempo)
{
    MidiImportResult r;
    try {
        Reader h{d, 0, d.size()};
        if (h.u32() != 0x4D546864) throw std::runtime_error("not a MIDI file");
        const auto hlen = h.u32();
        const auto format = h.u16();
        const auto ntrk = h.u16();
        const auto division = h.u16();
        h.i += hlen - 6;
        if (division & 0x8000) throw std::runtime_error("SMPTE time division not supported");
        if (format > 1) throw std::runtime_error("SMF format 2 not supported");
        const double scale = (double)kPPQ / division;
        bool tempoSet = false, sigSet = false;

        for (int tn = 0; tn < ntrk && h.i + 8 <= d.size(); ++tn) {
            const auto id = h.u32();
            const auto len = h.u32();
            Reader t{d, h.i, std::min(d.size(), h.i + len)};
            h.i = t.end;
            if (id != 0x4D54726B) continue;

            std::string name;
            std::map<int, std::vector<Note>> byChannel;
            std::map<int, std::deque<Note>> open; // key = ch*128+pitch
            Tick now = 0;
            std::uint8_t running = 0;
            while (t.i < t.end) {
                now += t.vlq();
                std::uint8_t st = t.u8();
                if (st < 0x80) { if (!running) throw std::runtime_error("bad running status"); --t.i; st = running; }
                if (st == 0xFF) {
                    const auto type = t.u8(); const auto l = t.vlq();
                    const size_t at = t.i; t.i += l;
                    if (t.i > t.end) throw std::runtime_error("truncated meta");
                    if (type == 0x03 && name.empty()) name.assign(d.begin() + (long)at, d.begin() + (long)t.i);
                    if (type == 0x51 && l == 3 && !tempoSet && adoptTempo) {
                        const auto us = (d[at] << 16) | (d[at + 1] << 8) | d[at + 2];
                        if (us > 0) project.tempoBpm = 60'000'000.0 / us;
                        tempoSet = true;
                    }
                    if (type == 0x58 && l >= 2 && !sigSet && adoptTempo) {
                        project.timeSig = {d[at], 1 << d[at + 1]};
                        sigSet = true;
                    }
                    if (type == 0x2F) break;
                    continue;
                }
                if (st == 0xF0 || st == 0xF7) { t.i += t.vlq(); running = 0; continue; }
                running = st;
                const int hi = st & 0xF0, ch = st & 0x0F;
                const int a = t.u8();
                const int b = (hi == 0xC0 || hi == 0xD0) ? 0 : t.u8();
                const Tick at = (Tick)std::llround((double)now * scale);
                if (hi == 0x90 && b > 0) {
                    Note n; n.pitch = a; n.velocity = b; n.start = at; n.channel = -1;
                    open[ch * 128 + a].push_back(n);
                } else if (hi == 0x80 || (hi == 0x90 && b == 0)) {
                    auto& q = open[ch * 128 + a];
                    if (!q.empty()) { Note n = q.front(); q.pop_front(); n.length = std::max<Tick>(1, at - n.start); byChannel[ch].push_back(n); }
                }
                // controllers/program/pitchbend are currently not modelled
            }
            const Tick endTick = (Tick)std::llround((double)now * scale);
            for (auto& [key, q] : open)
                for (auto& n : q) { n.length = std::max<Tick>(1, endTick - n.start); byChannel[key / 128].push_back(n); }

            for (auto& [ch, notes] : byChannel) {
                std::string tname = name.empty() ? "Track " + std::to_string(project.tracks().size() + 1) : name;
                if (byChannel.size() > 1) tname += " (ch " + std::to_string(ch + 1) + ")";
                auto& mt = project.addMidiTrack(tname);
                mt.channel = ch;
                for (auto& n : notes) mt.addNote(n);
                ++r.tracksCreated;
            }
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = e.what();
    }
    return r;
}

MidiImportResult MidiFile::readFile(const std::string& path, Project& project, bool adoptTempo)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {false, "Cannot open " + path, 0};
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return read(data, project, adoptTempo);
}

} // namespace mc::io
