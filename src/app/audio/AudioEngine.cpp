#include "audio/AudioEngine.h"
#include "sequencer/Timing.h"

namespace mc::audio {

AudioEngine::AudioEngine(juce::AudioDeviceManager& d) : devices(d)
{
    mixBuffer.setSize(2, 4096);
    devices.addAudioCallback(this);
}

AudioEngine::~AudioEngine()
{
    devices.removeAudioCallback(this);
    const juce::ScopedLock l(slotLock);
    slots.clear();
}

void AudioEngine::setSnapshot(std::shared_ptr<const seq::SequenceSnapshot> s)
{
    {
        const juce::SpinLock::ScopedLockType l(snapshotLock);
        if (pendingSnapshot) retired.push_back(pendingSnapshot);
        pendingSnapshot = std::move(s);
    }
    // keep a few old snapshots alive so the audio thread never frees memory
    while (retired.size() > 4) retired.erase(retired.begin());
}

void AudioEngine::setLoop(bool enabled, model::Tick start, model::Tick end)
{
    loopStart_ = start;
    loopEnd_ = end;
    loopEnabled_ = enabled && end > start;
}

AudioEngine::Slot* AudioEngine::findSlot(model::TrackId id) const
{
    for (auto& s : slots) if (s->id == id) return s.get();
    return nullptr;
}

std::unique_ptr<plugins::InstrumentPlugin> AudioEngine::setInstrument(model::TrackId id, std::unique_ptr<plugins::InstrumentPlugin> p)
{
    if (p) p->prepare(sampleRate(), blockSize());
    const juce::ScopedLock l(slotLock);
    auto* s = findSlot(id);
    if (!s) {
        auto ns = std::make_unique<Slot>();
        ns->id = id;
        ns->midi.ensureSize(8192);
        s = ns.get();
        slots.push_back(std::move(ns));
    }
    std::swap(s->plugin, p);
    s->held.reset();
    return p;
}

plugins::InstrumentPlugin* AudioEngine::instrument(model::TrackId id) const
{
    const juce::ScopedLock l(slotLock);
    auto* s = findSlot(id);
    return s ? s->plugin.get() : nullptr;
}

void AudioEngine::setBypassed(model::TrackId id, bool b)
{
    const juce::ScopedLock l(slotLock);
    if (auto* s = findSlot(id)) s->bypassed = b;
}

std::unique_ptr<plugins::InstrumentPlugin> AudioEngine::removeTrack(model::TrackId id)
{
    std::unique_ptr<Slot> removed;
    {
        const juce::ScopedLock l(slotLock);
        for (auto it = slots.begin(); it != slots.end(); ++it)
            if ((*it)->id == id) { removed = std::move(*it); slots.erase(it); break; }
    }
    return removed ? std::move(removed->plugin) : nullptr;
}

std::vector<model::TrackId> AudioEngine::trackIds() const
{
    const juce::ScopedLock l(slotLock);
    std::vector<model::TrackId> ids;
    for (auto& s : slots) ids.push_back(s->id);
    return ids;
}

void AudioEngine::pushLive(model::TrackId track, const juce::MidiMessage& m)
{
    if (m.getRawDataSize() > 3) return;
    const juce::SpinLock::ScopedLockType l(livePushLock);
    int s1, n1, s2, n2;
    liveFifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 + n2 < 1) return; // full: drop
    auto& e = liveBuffer[(size_t)(n1 > 0 ? s1 : s2)];
    e.track = track;
    e.size = m.getRawDataSize();
    std::memcpy(e.data, m.getRawData(), (size_t)e.size);
    liveFifo.finishedWrite(1);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* d)
{
    sampleRate_ = d->getCurrentSampleRate();
    blockSize_ = d->getCurrentBufferSizeSamples();
    mixBuffer.setSize(2, std::max(4096, blockSize_.load()));
    const juce::ScopedLock l(slotLock);
    for (auto& s : slots) if (s->plugin) s->plugin->prepare(sampleRate_, blockSize_);
}

void AudioEngine::audioDeviceStopped()
{
    const juce::ScopedLock l(slotLock);
    for (auto& s : slots) if (s->plugin) s->plugin->release();
}

void AudioEngine::releaseHeld(Slot& s, int offset)
{
    if (s.held.none()) return;
    for (int i = 0; i < 16 * 128; ++i)
        if (s.held[(size_t)i]) s.midi.addEvent(juce::MidiMessage::noteOff(i / 128 + 1, i % 128), offset);
    s.held.reset();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* out, int numOut, int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c) if (out[c]) juce::FloatVectorOperations::clear(out[c], numSamples);

    {
        const juce::SpinLock::ScopedTryLockType l(snapshotLock);
        if (l.isLocked() && pendingSnapshot != audioSnapshot) audioSnapshot = pendingSnapshot;
    }

    const juce::ScopedTryLock l(slotLock);
    if (!l.isLocked()) return; // instrument being swapped: skip one block rather than block

    const int maxChunk = std::min(mixBuffer.getNumSamples(), std::max(1, blockSize_.load()));
    for (int start = 0; start < numSamples; start += maxChunk)
        renderChunk(out, numOut, start, std::min(maxChunk, numSamples - start));
}

void AudioEngine::renderChunk(float* const* out, int numOut, int start, int n)
{
    // transport commands
    if (stopRequested_.exchange(false)) {
        playing_ = false;
        for (auto& s : slots) releaseHeld(*s, 0);
    }
    const double seek = pendingSeek_.exchange(-1.0);
    double pos = position_.load();
    if (seek >= 0) {
        pos = seek;
        for (auto& s : slots) releaseHeld(*s, 0);
    }

    // live MIDI
    {
        int s1, n1, s2, n2;
        liveFifo.prepareToRead(liveFifo.getNumReady(), s1, n1, s2, n2);
        auto take = [&](int from, int count) {
            for (int i = 0; i < count; ++i) {
                auto& e = liveBuffer[(size_t)(from + i)];
                if (auto* s = findSlot(e.track)) s->midi.addEvent(e.data, e.size, 0);
            }
        };
        take(s1, n1);
        take(s2, n2);
        liveFifo.finishedRead(n1 + n2);
    }

    // sequencer
    if (playing_ && audioSnapshot) {
        const double tps = seq::ticksPerSample(tempo_.load(), sampleRate_.load());
        int sampleAt = 0;
        while (sampleAt < n) {
            int len = n - sampleAt;
            double segEnd = pos + len * tps;
            bool wrap = false;
            const auto le = (double)loopEnd_.load();
            if (loopEnabled_ && pos < le && segEnd >= le) {
                len = std::max(1, (int)std::ceil((le - pos) / tps));
                len = std::min(len, n - sampleAt);
                segEnd = le;
                wrap = true;
            }
            const auto fromT = (model::Tick)std::ceil(pos);
            const auto toT = (model::Tick)std::ceil(segEnd);
            for (auto& s : slots) {
                const seq::TrackSequence* ts = nullptr;
                for (auto& t : audioSnapshot->tracks) if (t.trackId == s->id) { ts = &t; break; }
                if (!ts) continue;
                if (!ts->audible) { releaseHeld(*s, sampleAt); continue; }
                ts->forEachInRange(fromT, toT, [&](const seq::SeqEvent& e) {
                    const int off = juce::jlimit(0, n - 1, sampleAt + (int)(((double)e.tick - pos) / tps));
                    const auto idx = (size_t)(e.channel * 128 + e.pitch);
                    if (e.noteOn) {
                        s->midi.addEvent(juce::MidiMessage::noteOn(e.channel + 1, e.pitch, (juce::uint8)e.velocity), off);
                        s->held.set(idx);
                    } else if (s->held[idx]) {
                        s->midi.addEvent(juce::MidiMessage::noteOff(e.channel + 1, e.pitch), off);
                        s->held.reset(idx);
                    }
                });
            }
            sampleAt += len;
            pos = segEnd;
            if (wrap) {
                pos = (double)loopStart_.load();
                for (auto& s : slots) releaseHeld(*s, std::min(sampleAt, n - 1));
            }
        }
    }
    position_ = pos;

    // render + mix
    const auto* snap = audioSnapshot.get();
    for (auto& s : slots) {
        if (!s->plugin || s->bypassed) { s->midi.clear(); continue; }
        juce::AudioBuffer<float> buf(mixBuffer.getArrayOfWritePointers(), 2, n); // no allocation: refers to existing data
        buf.clear();
        s->plugin->process(buf, s->midi);
        s->midi.clear();
        float vol = 0.8f, pan = 0.0f;
        if (snap) for (auto& t : snap->tracks) if (t.trackId == s->id) { vol = t.volume; pan = t.pan; break; }
        const float gl = vol * std::min(1.0f, 1.0f - pan), gr = vol * std::min(1.0f, 1.0f + pan);
        if (numOut > 0 && out[0]) juce::FloatVectorOperations::addWithMultiply(out[0] + start, buf.getReadPointer(0), gl, n);
        if (numOut > 1 && out[1]) juce::FloatVectorOperations::addWithMultiply(out[1] + start, buf.getReadPointer(1), gr, n);
    }
}

} // namespace mc::audio
