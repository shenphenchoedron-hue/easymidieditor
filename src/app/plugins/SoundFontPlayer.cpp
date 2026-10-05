#include "plugins/SoundFontPlayer.h"
#include "plugins/BuiltinInstruments.h"
#include <map>
#include <mutex>

#if defined(__clang__) || defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wcast-align"
 #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
 #pragma GCC diagnostic ignored "-Wfloat-equal"
 #pragma GCC diagnostic ignored "-Wold-style-cast"
 #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#elif defined(_MSC_VER)
 #pragma warning(push, 0)
#endif
#define TSF_IMPLEMENTATION
#include "third_party/tsf.h"
#if defined(__clang__) || defined(__GNUC__)
 #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
 #pragma warning(pop)
#endif

namespace mc::plugins::soundfont {

namespace {

// ---------------------------------------------------------------- shared banks
// One loaded tsf per file, kept for the app lifetime; instruments use copies.
class Library {
public:
    static Library& instance() { static Library l; return l; }

    tsf* acquire(const juce::File& file, juce::String& error)
    {
        std::lock_guard l(mutex);
        const auto key = file.getFullPathName();
        auto it = banks.find(key);
        if (it == banks.end()) {
            if (!file.existsAsFile()) { error = "SoundFont not found: " + key; return nullptr; }
            tsf* f = tsf_load_filename(key.toRawUTF8());
            if (!f) { error = "Cannot read SoundFont: " + key; return nullptr; }
            it = banks.emplace(key, f).first;
        }
        tsf* copy = tsf_copy(it->second);
        if (!copy) error = "Out of memory";
        return copy;
    }

private:
    ~Library() { for (auto& [k, f] : banks) tsf_close(f); }
    std::mutex mutex;
    std::map<juce::String, tsf*> banks;
};

juce::File& userFolderRef()
{
    static juce::File f;
    return f;
}

// ---------------------------------------------------------------- instrument
class SoundFontInstrument final : public InstrumentPlugin {
public:
    SoundFontInstrument(tsf* f, int b, int p, std::string n, std::string fmt)
        : synth(f), bank(b), preset(p), name_(std::move(n)), fmt_(std::move(fmt)) {}
    ~SoundFontInstrument() override { tsf_close(synth); }

    std::string name() const override { return name_; }
    std::string format() const override { return fmt_; }

    void prepare(double sr, int bs) override
    {
        // -4 dB headroom: SoundFont presets are often mixed hot; chords add up.
        tsf_set_output(synth, TSF_STEREO_UNWEAVED, (int)sr, -4.0f);
        tsf_set_max_voices(synth, 128);   // pre-allocate: no allocation on the audio thread
        if (!tsf_channel_set_bank_preset(synth, 0, bank, preset))
            tsf_channel_set_presetnumber(synth, 0, preset, bank == 128); // fallback
        tsf_channel_set_pitchrange(synth, 0, 2.0f);
        scratch.assign((size_t)std::max(bs, 1024) * 2, 0.0f);
    }
    void release() override { tsf_channel_sounds_off_all(synth, 0); }
    bool hasEditor() const override { return false; }
    juce::Component* createEditor() override { return nullptr; }
    juce::MemoryBlock getState() override { return {}; }
    void setState(const juce::MemoryBlock&) override {}

    void process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        const int n = audio.getNumSamples();
        int pos = 0;
        for (const auto meta : midi) {
            const int at = juce::jlimit(0, n, meta.samplePosition);
            render(audio, pos, at);
            pos = std::max(pos, at);
            handle(meta.getMessage());
        }
        render(audio, pos, n);
    }

private:
    void handle(const juce::MidiMessage& m)
    {
        if (m.isNoteOn()) tsf_channel_note_on(synth, 0, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) tsf_channel_note_off(synth, 0, m.getNoteNumber());
        else if (m.isController()) tsf_channel_midi_control(synth, 0, m.getControllerNumber(), m.getControllerValue());
        else if (m.isPitchWheel()) tsf_channel_set_pitchwheel(synth, 0, m.getPitchWheelValue());
        else if (m.isAllNotesOff()) tsf_channel_note_off_all(synth, 0);
        else if (m.isAllSoundOff()) tsf_channel_sounds_off_all(synth, 0);
    }

    // Renders [from, to) into the first two channels (TSF writes L block then R block).
    void render(juce::AudioBuffer<float>& audio, int from, int to)
    {
        const int cap = (int)scratch.size() / 2;
        while (from < to) {
            const int len = std::min(to - from, cap);
            tsf_render_float(synth, scratch.data(), len, 0);
            audio.addFrom(0, from, scratch.data(), len);
            if (audio.getNumChannels() > 1) audio.addFrom(1, from, scratch.data() + len, len);
            from += len;
        }
    }

    tsf* synth;
    int bank, preset;
    std::string name_, fmt_;
    std::vector<float> scratch;
};

const BuiltinInstrument* findBuiltin(const std::string& identifier)
{
    if (identifier.rfind(kBuiltinPrefix, 0) != 0) return nullptr;
    const auto id = identifier.substr(std::char_traits<char>::length(kBuiltinPrefix));
    for (auto& b : kBuiltinInstruments) if (id == b.id) return &b;
    return nullptr;
}

// "sf2:<relative path>|<bank>|<preset>"
struct UserRef { juce::String relPath; int bank = 0, preset = 0; };
bool parseUser(const std::string& identifier, UserRef& out)
{
    if (identifier.rfind(kUserPrefix, 0) != 0) return false;
    const juce::String rest(identifier.substr(std::char_traits<char>::length(kUserPrefix)));
    const int p2 = rest.lastIndexOfChar('|');
    if (p2 <= 0) return false;
    const int p1 = rest.substring(0, p2).lastIndexOfChar('|');
    if (p1 <= 0) return false;
    out.relPath = rest.substring(0, p1);
    out.bank = rest.substring(p1 + 1, p2).getIntValue();
    out.preset = rest.substring(p2 + 1).getIntValue();
    return true;
}

juce::File userFile(const UserRef& r) { return userFolderRef().getChildFile(r.relPath); }

} // namespace

juce::File builtinFile()
{
    static const juce::File found = [] {
        const auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const auto app = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        const juce::File candidates[] = {
            app.getChildFile("Contents/Resources").getChildFile(kBuiltinSoundFontFile),          // macOS bundle
            exe.getParentDirectory().getChildFile(kBuiltinSoundFontFile),                       // Windows / Linux tarball
            exe.getParentDirectory().getChildFile("../share/midi-composer").getChildFile(kBuiltinSoundFontFile),
            juce::File("/usr/share/midi-composer").getChildFile(kBuiltinSoundFontFile),          // Debian package
        };
        for (auto& c : candidates) if (c.existsAsFile()) return c;
        return juce::File();
    }();
    return found;
}

juce::File userSoundsFolder(const juce::File& dataDir)
{
    auto f = dataDir.getChildFile("Sounds");
    f.createDirectory();
    userFolderRef() = f;
    return f;
}

std::vector<PluginInfo> builtinInstruments()
{
    std::vector<PluginInfo> out;
    if (!builtinFile().existsAsFile()) return out;
    for (auto& b : kBuiltinInstruments)
        out.push_back({"Internal", std::string(kBuiltinPrefix) + b.id, b.name, "GeneralUser GS", b.group, {}});
    return out;
}

std::vector<PluginInfo> scanUserFolder(const juce::File& folder, const std::function<bool(float, const juce::String&)>& progress)
{
    std::vector<PluginInfo> out;
    auto files = folder.findChildFiles(juce::File::findFiles, true, "*.sf2");
    files.sort();
    for (int i = 0; i < files.size(); ++i) {
        const auto& file = files.getReference(i);
        if (progress && !progress((float)i / (float)std::max(1, files.size()), "SoundFont: " + file.getFileName())) break;
        tsf* f = tsf_load_filename(file.getFullPathName().toRawUTF8());
        if (!f) continue; // not a valid SoundFont: skip
        const auto rel = file.getRelativePathFrom(folder).replaceCharacter('\\', '/');
        for (int p = 0; p < tsf_get_presetcount(f); ++p) {
            const auto& pr = f->presets[p];
            PluginInfo info;
            info.format = kUserFormat;
            info.identifier = (kUserPrefix + rel + "|" + juce::String(pr.bank) + "|" + juce::String(pr.preset)).toStdString();
            info.name = juce::String::fromUTF8(pr.presetName).trim().toStdString();
            if (info.name.empty()) info.name = "Preset " + std::to_string(pr.preset);
            info.manufacturer = file.getFileNameWithoutExtension().toStdString();
            info.category = info.manufacturer;   // grouped by file in the menus
            out.push_back(std::move(info));
        }
        tsf_close(f);
    }
    return out;
}

bool isSoundFontInfo(const PluginInfo& i)
{
    return i.format == kUserFormat || findBuiltin(i.identifier) != nullptr;
}

bool stillExists(const PluginInfo& i)
{
    if (findBuiltin(i.identifier)) return builtinFile().existsAsFile();
    UserRef r;
    return parseUser(i.identifier, r) && userFile(r).existsAsFile();
}

std::unique_ptr<InstrumentPlugin> create(const PluginInfo& info, double sr, int bs, juce::String& error)
{
    juce::File file;
    int bank = 0, preset = 0;
    if (auto* b = findBuiltin(info.identifier)) {
        file = builtinFile();
        if (!file.existsAsFile()) { error = "The built-in sounds (GeneralUser GS) are not installed"; return nullptr; }
        bank = b->bank; preset = b->preset;
    } else {
        UserRef r;
        if (!parseUser(info.identifier, r)) { error = "Invalid SoundFont reference"; return nullptr; }
        file = userFile(r);
        bank = r.bank; preset = r.preset;
    }
    tsf* f = Library::instance().acquire(file, error);
    if (!f) return nullptr;
    auto p = std::make_unique<SoundFontInstrument>(f, bank, preset, info.name, info.format);
    p->prepare(sr, bs);
    return p;
}

} // namespace mc::plugins::soundfont
