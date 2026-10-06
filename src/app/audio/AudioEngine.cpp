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

// `slots` and each Slot::plugin are only ever modified on the message thread
// (under slotLock, so the audio thread never sees a half-done change). Reading
// them from the message thread therefore needs no lock. Taking the lock here
// would make every UI action wait for the audio thread to finish rendering
// (which can take most of a block with heavy plugins like Kontakt), and would
// make the audio thread drop blocks whenever the UI held the lock.
plugins::InstrumentPlugin* AudioEngine::instrument(model::TrackId id) const
{
    jassert(juce::MessageManager::existsAndIsCurrentThread());
    auto* s = findSlot(id);
    return s ? s->plugin.get() : nullptr;
}

void AudioEngine::setBypassed(model::TrackId id, bool b)
{
    jassert(juce::MessageManager::existsAndIsCurrentThread());
    if (auto* s = findSlot(id)) s->bypassed = b; // atomic, read by the audio thread
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
    jassert(juce::MessageManager::existsAndIsCurrentThread()); // see instrument()
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
    numPendingClicks = 0;
    if (stopRequested_.exchange(false)) {
        playing_ = false;
        countInLeft_ = 0;
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

    // count-in: metronome only, the song position does not move
    int seqStart = 0;
    if (playing_ && countInLeft_.load() > 0) {
        const double tps = seq::ticksPerSample(tempo_.load(), sampleRate_.load());
        const double total = countInTotal_.load(), left = countInLeft_.load();
        const double elapsed = total - left, end = elapsed + n * tps;
        scheduleClicks(elapsed, std::min(end, total), tps, 0, n, elapsed);
        if (end >= total) { // count-in ends inside this chunk: the song starts at that sample
            seqStart = juce::jlimit(0, n, (int)std::ceil(left / tps));
            countInLeft_ = 0;
        } else {
            countInLeft_ = total - end;
            seqStart = n;
        }
    }

    // sequencer
    if (playing_ && audioSnapshot && seqStart < n) {
        const double tps = seq::ticksPerSample(tempo_.load(), sampleRate_.load());
        int sampleAt = seqStart;
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
            if (metronomeOn_) scheduleClicks(pos, segEnd, tps, sampleAt, n, pos);
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
    renderClicks(out, numOut, start, n);
}

// Clicks on every beat in [fromTick, toTick); the bar's first beat is accented.
// originTick is the tick at sample `sampleAt` of this chunk.
void AudioEngine::scheduleClicks(double fromTick, double toTick, double tps, int sampleAt, int n, double originTick)
{
    const auto beat = (double)beatTicks_.load(), bar = (double)barTicks_.load();
    for (double t = std::ceil(fromTick / beat) * beat; t < toTick; t += beat) {
        if (numPendingClicks >= (int)pendingClicks.size()) break;
        const int off = juce::jlimit(0, n - 1, sampleAt + (int)((t - originTick) / tps));
        const bool accent = std::fmod(t, bar) < 0.5;
        pendingClicks[(size_t)numPendingClicks++] = {off, accent};
    }
}

void AudioEngine::renderClicks(float* const* out, int numOut, int start, int n)
{
    if (numPendingClicks == 0 && clickLeft <= 0) return;
    const double sr = sampleRate_.load();
    const float level = metronomeLevel_.load();
    const int len = (int)(sr * 0.045);              // 45 ms click
    const double decay = std::exp(-1.0 / (sr * 0.009));
    int next = 0;
    for (int i = 0; i < n; ++i) {
        while (next < numPendingClicks && pendingClicks[(size_t)next].offset == i) {
            const bool accent = pendingClicks[(size_t)next].accent;
            clickFreq = accent ? 1760.0 : 1175.0;
            clickGain = (accent ? 0.75 : 0.5) * level;
            clickPhase = 0;
            clickLeft = len;
            ++next;
        }
        if (clickLeft <= 0) continue;
        const float v = (float)(std::sin(clickPhase) * clickGain);
        clickPhase += juce::MathConstants<double>::twoPi * clickFreq / sr;
        clickGain *= decay;
        --clickLeft;
        for (int c = 0; c < std::min(numOut, 2); ++c) if (out[c]) out[c][start + i] += v;
    }
}

} // namespace mc::audio
