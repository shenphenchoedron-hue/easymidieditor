#include "io/ProjectSerializer.h"
#include "io/Json.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mc::io {

using namespace mc::model;

std::string ProjectSerializer::toString(const Project& p)
{
    Json root;
    root.set("format", kProjectMagic);
    root.set("version", kProjectFormatVersion);
    root.set("tempoBpm", p.tempoBpm);
    root.set("timeSignature", Json::Object{{"numerator", p.timeSig.numerator}, {"denominator", p.timeSig.denominator}});
    root.set("ppq", kPPQ);
    root.set("playhead", (long long)p.playhead);
    root.set("loop", Json::Object{{"enabled", p.loopEnabled}, {"start", (long long)p.loopStart}, {"end", (long long)p.loopEnd}});
    root.set("activeTrack", (long long)p.activeTrack);
    root.set("harmony", Json::Object{{"root", p.harmony.root}, {"scale", p.harmony.scaleId}, {"chordMode", p.harmony.chordMode}});
    Json::Array mapping;
    for (int n : p.controllerMapping.notes) mapping.push_back(n);
    root.set("controllerMapping", Json::Object{{"inversion1", mapping[0]}, {"inversion2", mapping[1]},
                                               {"seventh", mapping[2]}, {"ninth", mapping[3]}});

    Json::Array tracks;
    for (auto& t : p.tracks()) {
        Json jt;
        jt.set("id", (long long)t->id());
        jt.set("name", t->name);
        jt.set("mute", t->mute);
        jt.set("solo", t->solo);
        jt.set("volume", t->volume);
        jt.set("pan", t->pan);
        if (auto* m = dynamic_cast<const MidiTrack*>(t.get())) {
            jt.set("type", "midi");
            jt.set("channel", m->channel);
            jt.set("plugin", Json::Object{{"format", m->plugin.format}, {"identifier", m->plugin.identifier},
                                          {"name", m->plugin.name}, {"manufacturer", m->plugin.manufacturer},
                                          {"state", m->plugin.stateBase64}, {"bypassed", m->plugin.bypassed}});
            Json::Array notes;
            for (auto& n : m->notes())
                notes.push_back(Json::Array{(long long)n.start, (long long)n.length, n.pitch, n.velocity, n.channel});
            jt.set("notes", notes);
        } else {
            jt.set("type", "audio");
        }
        tracks.push_back(jt);
    }
    root.set("tracks", tracks);
    return root.dump(1);
}

LoadResult ProjectSerializer::fromString(const std::string& text, Project& out)
{
    LoadResult r;
    Json root;
    try { root = Json::parse(text); } catch (const std::exception& e) { r.error = e.what(); return r; }
    if (root["format"].str() != kProjectMagic) { r.error = "Not a project file"; return r; }
    r.fileVersion = (int)root["version"].num(0);
    r.newerThanApp = r.fileVersion > kProjectFormatVersion;

    Project p;
    p.tempoBpm = root["tempoBpm"].num(120.0);
    if (p.tempoBpm < 10 || p.tempoBpm > 999) p.tempoBpm = 120.0;
    p.timeSig.numerator = (int)root["timeSignature"]["numerator"].num(4);
    p.timeSig.denominator = (int)root["timeSignature"]["denominator"].num(4);
    if (p.timeSig.numerator < 1) p.timeSig.numerator = 4;
    if (p.timeSig.denominator < 1) p.timeSig.denominator = 4;
    const double ppq = root["ppq"].num(kPPQ);
    auto tick = [&](const Json& j, double def) { return (Tick)(j.num(def) * kPPQ / (ppq > 0 ? ppq : kPPQ)); };
    p.playhead = tick(root["playhead"], 0);
    p.loopEnabled = root["loop"]["enabled"].boolean(false);
    p.loopStart = tick(root["loop"]["start"], 0);
    p.loopEnd = tick(root["loop"]["end"], 16 * ppq);
    p.harmony.root = ((int)root["harmony"]["root"].num(9) % 12 + 12) % 12;
    p.harmony.scaleId = root["harmony"]["scale"].str("natural_minor");
    p.harmony.chordMode = root["harmony"]["chordMode"].boolean(false);
    auto& cm = root["controllerMapping"];
    const char* keys[] = {"inversion1", "inversion2", "seventh", "ninth"};
    for (int i = 0; i < 4; ++i) p.controllerMapping.notes[i] = (int)cm[keys[i]].num(p.controllerMapping.notes[i]);

    for (auto& jt : root["tracks"].arr()) {
        const auto id = (TrackId)jt["id"].num((double)p.nextTrackId());
        std::unique_ptr<Track> t;
        if (jt["type"].str("midi") == "audio") {
            t = std::make_unique<AudioTrack>(id, jt["name"].str("Audio"));
        } else {
            auto m = std::make_unique<MidiTrack>(id, jt["name"].str("Track"));
            m->channel = (int)jt["channel"].num(0) & 15;
            auto& jp = jt["plugin"];
            m->plugin.format = jp["format"].str();
            m->plugin.identifier = jp["identifier"].str();
            m->plugin.name = jp["name"].str();
            m->plugin.manufacturer = jp["manufacturer"].str();
            m->plugin.stateBase64 = jp["state"].str();
            m->plugin.bypassed = jp["bypassed"].boolean(false);
            for (auto& jn : jt["notes"].arr()) {
                auto& a = jn.arr();
                if (a.size() < 4) continue;
                Note n;
                n.start = tick(a[0], 0);
                n.length = tick(a[1], ppq);
                n.pitch = (int)a[2].num(60);
                n.velocity = (int)a[3].num(100);
                n.channel = a.size() > 4 ? (int)a[4].num(-1) : -1;
                m->addNote(n);
            }
            t = std::move(m);
        }
        t->mute = jt["mute"].boolean(false);
        t->solo = jt["solo"].boolean(false);
        t->volume = (float)jt["volume"].num(0.8);
        t->pan = (float)jt["pan"].num(0.0);
        if (p.track(id)) continue; // duplicate id: skip rather than corrupt
        p.insertTrack((int)p.tracks().size(), std::move(t));
    }
    p.activeTrack = (TrackId)root["activeTrack"].num(0);
    if (!p.track(p.activeTrack)) p.activeTrack = p.tracks().empty() ? 0 : p.tracks().front()->id();

    out = p;
    r.ok = true;
    return r;
}

bool ProjectSerializer::saveFile(const Project& p, const std::string& path, std::string* error)
{
    namespace fs = std::filesystem;
    const fs::path target(path);
    const fs::path tmp = target.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) { if (error) *error = "Cannot write " + tmp.string(); return false; }
        f << toString(p);
        if (!f) { if (error) *error = "Write failed"; return false; }
    }
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (ec) { // e.g. Windows when target exists
        fs::remove(target, ec);
        fs::rename(tmp, target, ec);
        if (ec) { if (error) *error = ec.message(); return false; }
    }
    return true;
}

LoadResult ProjectSerializer::loadFile(const std::string& path, Project& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {false, "Cannot open " + path};
    std::stringstream ss;
    ss << f.rdbuf();
    return fromString(ss.str(), out);
}

} // namespace mc::io
