#include "plugins/JucePluginHost.h"

namespace mc::plugins {

namespace {

class JuceInstrument final : public InstrumentPlugin {
public:
    JuceInstrument(std::unique_ptr<juce::AudioPluginInstance> i, std::string fmt) : inst(std::move(i)), fmt_(std::move(fmt)) {}
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
    bool hasEditor() const override { return inst->hasEditor(); }
    juce::Component* createEditor() override
    {
        if (inst->hasEditor()) if (auto* e = inst->createEditorIfNeeded()) return e;
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
    juce::AudioBuffer<float> scratch;
    int maxBlock = 0;
};

} // namespace

JucePluginHost::JucePluginHost(juce::File pedal) : deadMansPedal(std::move(pedal))
{
    formatManager.addDefaultFormats();
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
            try { more = scanner.scanNextFile(true, current); }
            catch (...) { more = true; } // defective plugin threw: skip it
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
    auto inst = formatManager.createPluginInstance(*d, sr, bs, error);
    if (!inst) return nullptr;
    auto p = std::make_unique<JuceInstrument>(std::move(inst), info.format);
    p->prepare(sr, bs);
    return p;
}

} // namespace mc::plugins
