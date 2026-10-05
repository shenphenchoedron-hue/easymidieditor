#include "plugins/BasicSynth.h"
#include "plugins/SoundFontPlayer.h"

namespace mc::plugins {

namespace {

struct Sound : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

struct Voice : juce::SynthesiserVoice {
    using juce::SynthesiserVoice::renderNextBlock;
    juce::ADSR adsr;
    double phase = 0, delta = 0;
    float level = 0;

    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }
    void startNote(int note, float velocity, juce::SynthesiserSound*, int) override
    {
        phase = 0;
        delta = juce::MidiMessage::getMidiNoteInHertz(note) / getSampleRate();
        level = velocity * 0.18f;
        adsr.setSampleRate(getSampleRate());
        adsr.setParameters({0.005f, 0.25f, 0.6f, 0.25f});
        adsr.noteOn();
    }
    void stopNote(float, bool allowTail) override
    {
        if (allowTail) adsr.noteOff(); else { adsr.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (!isVoiceActive()) return;
        for (int i = 0; i < num; ++i) {
            // soft "triangle-ish" tone: sine + a little 2nd/3rd harmonic
            const double p = phase * juce::MathConstants<double>::twoPi;
            const float s = (float)(std::sin(p) + 0.3 * std::sin(2 * p) + 0.12 * std::sin(3 * p)) * level * adsr.getNextSample();
            phase += delta;
            if (phase >= 1.0) phase -= 1.0;
            for (int c = 0; c < out.getNumChannels(); ++c) out.addSample(c, start + i, s);
        }
        if (!adsr.isActive()) clearCurrentNote();
    }
};

class BasicSynth final : public InstrumentPlugin {
public:
    BasicSynth()
    {
        for (int i = 0; i < 24; ++i) synth.addVoice(new Voice());
        synth.addSound(new Sound());
    }
    std::string name() const override { return "Basic Synth"; }
    std::string format() const override { return InternalPluginHost::kFormat; }
    void prepare(double sr, int) override { synth.setCurrentPlaybackSampleRate(sr); }
    void release() override {}
    bool hasEditor() const override { return false; }
    juce::Component* createEditor() override { return nullptr; }
    juce::MemoryBlock getState() override { return {}; }
    void setState(const juce::MemoryBlock&) override {}
    void process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        synth.renderNextBlock(audio, midi, 0, audio.getNumSamples());
    }
private:
    juce::Synthesiser synth;
};

} // namespace

PluginInfo InternalPluginHost::basicSynthInfo()
{
    return {kFormat, kBasicSynthId, "Basic Synth", "MidiComposer", "Synth", {}};
}

std::vector<PluginInfo> InternalPluginHost::builtins()
{
    std::vector<PluginInfo> v{basicSynthInfo()};
    auto band = soundfont::builtinInstruments();
    v.insert(v.end(), band.begin(), band.end());
    return v;
}

PluginInfo InternalPluginHost::defaultInstrument()
{
    for (auto& i : soundfont::builtinInstruments())
        if (i.identifier == std::string(soundfont::kBuiltinPrefix) + "pop-piano") return i;
    return basicSynthInfo();
}

juce::StringArray InternalPluginHost::formats() const { return {kFormat, soundfont::kUserFormat}; }

std::vector<PluginInfo> InternalPluginHost::scan(const std::function<bool(float, const juce::String&)>& progress)
{
    auto v = builtins();
    auto user = soundfont::scanUserFolder(userSounds, progress);
    v.insert(v.end(), user.begin(), user.end());
    return v;
}

bool InternalPluginHost::stillExists(const PluginInfo& i) const
{
    if (i.identifier == kBasicSynthId) return true;
    return soundfont::stillExists(i);
}

std::unique_ptr<InstrumentPlugin> InternalPluginHost::create(const PluginInfo& info, double sr, int bs, juce::String& error)
{
    if (soundfont::isSoundFontInfo(info)) return soundfont::create(info, sr, bs, error);
    if (info.identifier != kBasicSynthId) { error = "Unknown internal instrument"; return nullptr; }
    auto p = std::make_unique<BasicSynth>();
    p->prepare(sr, bs);
    return p;
}

} // namespace mc::plugins
