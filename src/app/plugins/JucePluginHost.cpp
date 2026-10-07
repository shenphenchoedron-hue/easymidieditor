#include "plugins/JucePluginHost.h"
#include <map>
#include <mutex>
#include <set>
#if JUCE_LINUX || JUCE_BSD
 #include <dlfcn.h>
#endif

namespace mc::plugins {

namespace {

// Some plugins (e.g. TX16Wx) start background threads (GLib/GIO) that outlive the
// plugin instance. When the host unloads the plugin's library after scanning or
// after removing an instrument, those threads jump into unmapped code and crash
// the whole app. Keeping the library loaded for the rest of the session avoids
// that (Carla and Ardour do the same). Linux only: there VST3 and LV2 plugins are
// plain shared objects.
#if JUCE_LINUX || JUCE_BSD
void pinLibraries(const juce::Array<juce::File>& libs)
{
    static std::mutex m;
    static std::set<juce::String> pinned;
    std::lock_guard l(m);
    for (auto& lib : libs) {
        const auto path = lib.getFullPathName();
        if (!lib.existsAsFile() || !pinned.insert(path).second) continue;
        // RTLD_NODELETE: later dlclose() calls never unmap it. The handle is intentionally never closed.
        if (dlopen(path.toRawUTF8(), RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE) == nullptr)
            pinned.erase(path);
    }
}
#endif
} // namespace

// LV2 plugins are identified by URI. Each bundle's manifest.ttl says which
// library (lv2:binary) implements which plugin URI; only that library is pinned,
// never the plugin's UI libraries (a GTK2 UI loaded into the app can crash it).
std::vector<std::pair<juce::String, juce::File>> parseLv2Manifest(const juce::String& text, const juce::File& bundle)
{
    std::vector<std::pair<juce::String, juce::File>> out;
    // tokens: <iri>, "literals", prefixed names / keywords, and the '.' ';' ',' separators
    juce::StringArray tokens;
    for (int i = 0; i < text.length();) {
        const auto c = text[i];
        if (juce::CharacterFunctions::isWhitespace(c)) { ++i; continue; }
        if (c == '#') { while (i < text.length() && text[i] != '\n') ++i; continue; } // comment
        if (c == '<') { const int e = text.indexOfChar(i, '>'); if (e < 0) break; tokens.add(text.substring(i, e + 1)); i = e + 1; continue; }
        if (c == '"') { int e = i + 1; while (e < text.length() && text[e] != '"') e += text[e] == '\\' ? 2 : 1; tokens.add("\"\""); i = e + 1; continue; }
        if (c == ';' || c == ',' || (c == '.' && (i + 1 >= text.length() || juce::CharacterFunctions::isWhitespace(text[i + 1])))) {
            tokens.add(juce::String::charToString(c)); ++i; continue;
        }
        int e = i;
        while (e < text.length() && !juce::CharacterFunctions::isWhitespace(text[e]) && text[e] != ';' && text[e] != ','
               && text[e] != '<' && !(text[e] == '.' && (e + 1 >= text.length() || juce::CharacterFunctions::isWhitespace(text[e + 1]))))
            ++e;
        tokens.add(text.substring(i, e));
        i = e;
    }
    std::map<juce::String, juce::String> prefixes;
    auto expand = [&](const juce::String& t) -> juce::String {
        if (t.startsWithChar('<')) return t.substring(1, t.length() - 1);
        const int colon = t.indexOfChar(':');
        if (colon >= 0) if (auto it = prefixes.find(t.substring(0, colon)); it != prefixes.end()) return it->second + t.substring(colon + 1);
        return t;
    };
    const juce::String lv2Binary = "http://lv2plug.in/ns/lv2core#binary";
    juce::String subject;
    bool expectSubject = true, expectPredicate = false;
    juce::String predicate;
    for (int i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        if (t == "@prefix" && i + 2 < tokens.size()) {
            prefixes[tokens[i + 1].upToLastOccurrenceOf(":", false, false)] = expand(tokens[i + 2]);
            i += 2;
            continue;
        }
        if (t == ".") { expectSubject = true; continue; }
        if (t == ";") { expectPredicate = true; continue; }
        if (t == ",") continue;
        if (expectSubject) { subject = expand(t); expectSubject = false; expectPredicate = true; continue; }
        if (expectPredicate) { predicate = expand(t); expectPredicate = false; continue; }
        if (predicate == lv2Binary) {
            const auto bin = expand(t);
            out.push_back({subject, juce::File::isAbsolutePath(bin) ? juce::File(bin) : bundle.getChildFile(bin)});
        }
    }
    return out;
}

namespace {

void pinPluginModule(const juce::String& fileOrIdentifier, const juce::FileSearchPath& lv2Paths)
{
   #if JUCE_LINUX || JUCE_BSD
    const juce::File f(fileOrIdentifier);
    juce::Array<juce::File> libs;
    if (juce::File::isAbsolutePath(fileOrIdentifier) && f.exists()) {
        if (f.isDirectory()) // VST3 bundle: Contents/<arch>-linux/*.so
            libs = f.getChildFile("Contents").findChildFiles(juce::File::findFiles, true, "*.so");
        else if (f.hasFileExtension("so"))
            libs.add(f);
    } else if (fileOrIdentifier.isNotEmpty()) { // LV2 plugin URI
        static std::mutex m;
        static std::map<juce::String, juce::Array<juce::File>> index; // URI -> binaries
        static juce::String indexedPaths;
        std::lock_guard l(m);
        if (indexedPaths != lv2Paths.toString() || index.find(fileOrIdentifier) == index.end()) {
            index.clear(); // (re)build: cheap, only manifest.ttl files are read
            indexedPaths = lv2Paths.toString();
            for (int p = 0; p < lv2Paths.getNumPaths(); ++p)
                for (auto& bundle : lv2Paths.getRawString(p).isEmpty() ? juce::Array<juce::File>()
                                     : juce::File(lv2Paths.getRawString(p)).findChildFiles(juce::File::findDirectories, false, "*.lv2"))
                    for (auto& [uri, bin] : parseLv2Manifest(bundle.getChildFile("manifest.ttl").loadFileAsString(), bundle))
                        index[uri].addIfNotAlreadyThere(bin);
        }
        if (auto it = index.find(fileOrIdentifier); it != index.end()) libs = it->second;
    }
    pinLibraries(libs);
   #else
    juce::ignoreUnused(fileOrIdentifier, lv2Paths);
   #endif
}

class JuceInstrument final : public InstrumentPlugin {
public:
    // hasEditor() is queried once and cached: for VST3, JUCE answers it by creating
    // (and destroying) the plugin's whole editor view. For Native Instruments
    // plugins that means building a Qt GUI, which made every UI refresh crawl.
    JuceInstrument(std::unique_ptr<juce::AudioPluginInstance> i, std::string fmt)
        : inst(std::move(i)), fmt_(std::move(fmt)), hasEditor_(inst->hasEditor()) {}
    ~JuceInstrument() override { inst->releaseResources(); }

    std::string name() const override { return inst->getName().toStdString(); }
    std::string format() const override { return fmt_; }
    void prepare(double sr, int bs) override
    {
        inst->releaseResources();
        inst->setRateAndBufferSizeDetails(sr, bs);
        inst->prepareToPlay(sr, bs);
        maxBlock = bs;
        scratch.setSize(std::max({2, inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels()}), bs);
    }
    void release() override { inst->releaseResources(); }
    bool hasEditor() const override { return hasEditor_; }
    juce::Component* createEditor() override
    {
        if (hasEditor_) if (auto* e = inst->createEditorIfNeeded()) return e;
        return new juce::GenericAudioProcessorEditor(*inst);
    }
    juce::MemoryBlock getState() override { juce::MemoryBlock m; inst->getStateInformation(m); return m; }
    void setState(const juce::MemoryBlock& m) override { if (m.getSize() > 0) inst->setStateInformation(m.getData(), (int)m.getSize()); }

    void process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        const int n = audio.getNumSamples();
        if (n > maxBlock) return; // engine guarantees this never happens
        scratch.setSize(scratch.getNumChannels(), n, false, false, true);
        scratch.clear();
        inst->processBlock(scratch, midi);
        const int outs = std::max(1, inst->getTotalNumOutputChannels());
        for (int c = 0; c < audio.getNumChannels(); ++c)
            audio.copyFrom(c, 0, scratch, std::min(c, outs - 1), 0, n);
    }

private:
    std::unique_ptr<juce::AudioPluginInstance> inst;
    std::string fmt_;
    bool hasEditor_ = false;
    juce::AudioBuffer<float> scratch;
    int maxBlock = 0;
};

} // namespace

JucePluginHost::JucePluginHost(juce::File pedal) : deadMansPedal(std::move(pedal))
{
    formatManager.addDefaultFormats();
}

juce::FileSearchPath JucePluginHost::lv2SearchPath() const
{
    for (auto* f : formatManager.getFormats())
        if (f->getName() == "LV2") return f->getDefaultLocationsToSearch();
    return {};
}

juce::StringArray JucePluginHost::formats() const
{
    juce::StringArray s;
    for (auto* f : formatManager.getFormats()) s.add(f->getName());
    return s;
}

std::vector<PluginInfo> JucePluginHost::scan(const std::function<bool(float, const juce::String&)>& progress)
{
    std::vector<PluginInfo> out;
    juce::KnownPluginList list;
    const int nFormats = formatManager.getNumFormats();
    for (int fi = 0; fi < nFormats; ++fi) {
        auto* format = formatManager.getFormat(fi);
        if (!format->canScanForPlugins()) continue;
        // The dead man's pedal file records the plugin being scanned; if a plugin
        // crashes the process it is blacklisted on the next scan.
        juce::PluginDirectoryScanner scanner(list, *format, format->getDefaultLocationsToSearch(), true, deadMansPedal, true);
        juce::String current;
        for (;;) {
            current = scanner.getNextPluginFileThatWillBeScanned();
            if (progress && !progress(((float)fi + scanner.getProgress()) / (float)nFormats, format->getName() + ": " + current))
                return out;
            bool more = false;
            // Plugins must be loaded on the message thread: many (e.g. Native
            // Instruments' Qt-based ones) touch AppKit/HIToolbox or X11 during
            // load and abort the process when called from a worker thread.
            auto doScan = [&] {
                pinPluginModule(current, lv2SearchPath());
                try { more = scanner.scanNextFile(true, current); }
                catch (...) { more = true; } // defective plugin threw: skip it
            };
            if (auto* mm = juce::MessageManager::getInstanceWithoutCreating(); mm != nullptr && !mm->isThisTheMessageThread())
                mm->callFunctionOnMessageThread([](void* f) -> void* { (*static_cast<decltype(doScan)*>(f))(); return nullptr; }, &doScan);
            else
                doScan();
            if (!more) break;
        }
    }
    for (auto& d : list.getTypes()) {
        if (!d.isInstrument) continue;
        PluginInfo i;
        i.format = d.pluginFormatName.toStdString();
        i.identifier = d.createIdentifierString().toStdString();
        i.name = d.name.toStdString();
        i.manufacturer = d.manufacturerName.toStdString();
        i.category = d.category.toStdString();
        if (auto xml = d.createXml()) i.hostData = xml->toString(juce::XmlElement::TextFormat().singleLine()).toStdString();
        out.push_back(std::move(i));
    }
    return out;
}

std::unique_ptr<juce::PluginDescription> JucePluginHost::descriptionFrom(const PluginInfo& info)
{
    auto d = std::make_unique<juce::PluginDescription>();
    if (auto xml = juce::parseXML(juce::String(info.hostData)))
        if (d->loadFromXml(*xml)) return d;
    return nullptr;
}

bool JucePluginHost::stillExists(const PluginInfo& info) const
{
    auto d = descriptionFrom(info);
    if (!d) return false;
    for (auto* f : formatManager.getFormats())
        if (f->getName() == d->pluginFormatName) return f->doesPluginStillExist(*d);
    return false;
}

std::unique_ptr<InstrumentPlugin> JucePluginHost::create(const PluginInfo& info, double sr, int bs, juce::String& error)
{
    auto d = descriptionFrom(info);
    if (!d) { error = "Invalid plugin description"; return nullptr; }
    pinPluginModule(d->fileOrIdentifier, lv2SearchPath());
    auto inst = formatManager.createPluginInstance(*d, sr, bs, error);
    if (!inst) return nullptr;
    auto p = std::make_unique<JuceInstrument>(std::move(inst), info.format);
    p->prepare(sr, bs);
    return p;
}

} // namespace mc::plugins
