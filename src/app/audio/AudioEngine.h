#pragma once
// Realtime engine: runs the sequencer inside the audio callback, routes
// sequenced + live MIDI to each track's instrument and mixes the result.
// The audio thread never touches the Project; it consumes immutable snapshots,
// atomics and a lock-free FIFO. No allocations happen on the audio thread.
#include "plugins/InstrumentPlugin.h"
#include "sequencer/Sequence.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <bitset>

namespace mc::audio {

class AudioEngine final : private juce::AudioIODeviceCallback {
public:
    explicit AudioEngine(juce::AudioDeviceManager&);
    ~AudioEngine() override;

    // ---- message thread
    void setSnapshot(std::shared_ptr<const seq::SequenceSnapshot>);
    // Returns the previous instrument so it is destroyed on the caller's thread.
    std::unique_ptr<plugins::InstrumentPlugin> setInstrument(model::TrackId, std::unique_ptr<plugins::InstrumentPlugin>);
    plugins::InstrumentPlugin* instrument(model::TrackId) const;
    void setBypassed(model::TrackId, bool);
    std::unique_ptr<plugins::InstrumentPlugin> removeTrack(model::TrackId);
    std::vector<model::TrackId> trackIds() const;

    double sampleRate() const { return sampleRate_.load(); }
    int blockSize() const { return blockSize_.load(); }

    // ---- transport (any thread)
    void play() { playing_ = true; }
    void stop() { stopRequested_ = true; }
    bool isPlaying() const { return playing_.load(); }
    void setPosition(model::Tick t) { pendingSeek_ = (double)std::max<model::Tick>(0, t); }
    double position() const { return position_.load(); }
    void setTempo(double bpm) { tempo_ = juce::jlimit(10.0, 999.0, bpm); }
    void setLoop(bool enabled, model::Tick start, model::Tick end);

    // ---- live input (any thread, lock-free for the audio thread)
    void pushLive(model::TrackId, const juce::MidiMessage&);

private:
    struct Slot {
        model::TrackId id = 0;
        std::unique_ptr<plugins::InstrumentPlugin> plugin;
        bool bypassed = false;
        juce::MidiBuffer midi;
        std::bitset<16 * 128> held;    // notes sent by the sequencer
    };
    struct LiveEvent { model::TrackId track; std::uint8_t data[3]; int size; };

    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void renderChunk(float* const* out, int numOut, int start, int n);
    static void releaseHeld(Slot&, int sampleOffset);
    Slot* findSlot(model::TrackId) const;

    juce::AudioDeviceManager& devices;
    mutable juce::CriticalSection slotLock;
    std::vector<std::unique_ptr<Slot>> slots;

    juce::SpinLock snapshotLock;
    std::shared_ptr<const seq::SequenceSnapshot> pendingSnapshot, audioSnapshot;
    std::vector<std::shared_ptr<const seq::SequenceSnapshot>> retired; // freed on message thread

    juce::SpinLock livePushLock;
    juce::AbstractFifo liveFifo{2048};
    std::array<LiveEvent, 2048> liveBuffer{};

    std::atomic<double> sampleRate_{44100.0};
    std::atomic<int> blockSize_{512};
    std::atomic<bool> playing_{false}, stopRequested_{false};
    std::atomic<double> position_{0}, pendingSeek_{-1}, tempo_{120};
    std::atomic<bool> loopEnabled_{false};
    std::atomic<model::Tick> loopStart_{0}, loopEnd_{0};
    juce::AudioBuffer<float> mixBuffer;
};

} // namespace mc::audio
