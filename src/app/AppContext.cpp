#include "AppContext.h"
#include "io/MidiFile.h"
#include "io/MusicXml.h"
#include "io/ProjectSerializer.h"
#include "plugins/BasicSynth.h"

namespace mc {

namespace {
class PluginEditorWindow final : public juce::DocumentWindow {
public:
    PluginEditorWindow(const juce::String& title, juce::Component* editor, std::function<void()> onClose)
        : DocumentWindow(title, juce::Colour(0xffe9edf2), DocumentWindow::closeButton), closeFn(std::move(onClose))
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);
        if (auto* ape = dynamic_cast<juce::AudioProcessorEditor*>(editor)) setResizable(ape->isResizable(), false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override { juce::MessageManager::callAsync(closeFn); }
private:
    std::function<void()> closeFn;
};

model::PluginReference defaultInstrument()
{
    return plugins::PluginManager::referenceFor(plugins::InternalPluginHost::basicSynthInfo());
}
} // namespace

AppContext::AppContext()
{
    auto state = settings.audioDeviceState();
    const auto err = deviceManager.initialise(0, 2, state.get(), true);
    if (err.isNotEmpty()) DBG("Audio init: " << err);

    engine = std::make_unique<audio::AudioEngine>(deviceManager);
    plugins = std::make_unique<plugins::PluginManager>(settings.dataDirectory());
    midiDevices = std::make_unique<midi::MidiDeviceManager>(deviceManager, [this](const juce::MidiMessage& m) { handleMidi(m); });
    midiDevices->selectInput(settings.midiInputIdentifier());

    chordInput.setMapping(settings.controllerMapping());
    project.controllerMapping = chordInput.mapping();

    listenerHandle = project.addListener([this] { onModelChanged(); });
    newProject();
}

AppContext::~AppContext()
{
    project.removeListener(listenerHandle);
    editorWindows.clear();
    midiDevices.reset();
    if (auto xml = deviceManager.createStateXml()) settings.setAudioDeviceState(xml.get());
    engine.reset();
}

// ---------------------------------------------------------------- model sync
void AppContext::onModelChanged()
{
    if (!suppressDirty) dirty = true;
    syncEngine();
    sendChangeMessage();
}

void AppContext::syncEngine()
{
    // remove engine slots for deleted tracks
    for (auto id : engine->trackIds())
        if (!project.midiTrack(id)) {
            closeEditor(id);
            engine->removeTrack(id);
            loadedRefs.erase(id);
            missing.erase(id);
            errors.erase(id);
        }

    for (auto& t : project.tracks()) {
        auto* mt = dynamic_cast<model::MidiTrack*>(t.get());
        if (!mt) continue;
        auto it = loadedRefs.find(mt->id());
        const bool same = it != loadedRefs.end() && it->second.format == mt->plugin.format && it->second.identifier == mt->plugin.identifier;
        if (!same) {
            closeEditor(mt->id());
            std::unique_ptr<plugins::InstrumentPlugin> inst;
            missing.erase(mt->id());
            errors.erase(mt->id());
            if (!mt->plugin.empty()) {
                juce::String err;
                inst = plugins->instantiate(mt->plugin, engine->sampleRate(), engine->blockSize(), err);
                if (!inst) { missing.insert(mt->id()); errors[mt->id()] = err; }
            }
            engine->setInstrument(mt->id(), std::move(inst)); // old instance destroyed here, on the message thread
            loadedRefs[mt->id()] = mt->plugin;
        }
        engine->setBypassed(mt->id(), mt->plugin.bypassed);
    }

    activeTrackAtomic = project.activeTrack;
    if (auto* a = activeMidiTrack()) activeChannelAtomic = a->channel;
    engine->setTempo(project.tempoBpm);
    engine->setLoop(project.loopEnabled, project.loopStart, project.loopEnd);
    engine->setSnapshot(std::make_shared<seq::SequenceSnapshot>(seq::SequenceSnapshot::build(project)));
}

juce::String AppContext::pluginError(model::TrackId id) const
{
    auto it = errors.find(id);
    return it == errors.end() ? juce::String() : it->second;
}

void AppContext::captureAllPluginStates()
{
    for (auto& t : project.tracks())
        if (auto* mt = dynamic_cast<model::MidiTrack*>(t.get()))
            if (!isPluginMissing(mt->id()))
                if (auto* inst = engine->instrument(mt->id())) {
                    auto state = inst->getState();
                    mt->plugin.stateBase64 = state.getSize() ? state.toBase64Encoding().toStdString() : std::string();
                }
}

// ---------------------------------------------------------------- files
void AppContext::newProject()
{
    stop();
    suppressDirty = true;
    model::Project p;
    p.controllerMapping = chordInput.mapping();
    auto& t = p.addMidiTrack("Piano");
    t.plugin = defaultInstrument();
    project = p;
    undo.clear();
    currentFile = juce::File();
    setHarmony(project.harmony.root, project.harmony.scaleId, project.harmony.chordMode);
    engine->setPosition(0);
    project.notifyChanged();
    suppressDirty = false;
    dirty = false;
}

bool AppContext::openProject(const juce::File& f, juce::String& error)
{
    model::Project p;
    auto r = io::ProjectSerializer::loadFile(f.getFullPathName().toStdString(), p);
    if (!r.ok) { error = r.error; return false; }
    stop();
    suppressDirty = true;
    // Mapping belongs to the user's keyboard: settings win over the project copy.
    p.controllerMapping = chordInput.mapping();
    loadedRefs.clear(); // force re-instantiation with stored state
    for (auto id : engine->trackIds()) { closeEditor(id); engine->removeTrack(id); }
    project = p;
    undo.clear();
    currentFile = f;
    setHarmony(project.harmony.root, project.harmony.scaleId, project.harmony.chordMode);
    engine->setPosition(project.playhead);
    project.notifyChanged();
    suppressDirty = false;
    dirty = false;
    if (r.newerThanApp) error = "Project was saved by a newer version; some data may be ignored.";
    return true;
}

bool AppContext::saveProject(const juce::File& f, juce::String& error)
{
    captureAllPluginStates();
    project.playhead = (model::Tick)engine->position();
    std::string e;
    if (!io::ProjectSerializer::saveFile(project, f.getFullPathName().toStdString(), &e)) { error = e; return false; }
    currentFile = f;
    dirty = false;
    sendChangeMessage();
    return true;
}

bool AppContext::importMidi(const juce::File& f, juce::String& error)
{
    model::Project imported;
    auto r = io::MidiFile::readFile(f.getFullPathName().toStdString(), imported, true);
    if (!r.ok) { error = r.error; return false; }
    // Imported tracks are added through undoable commands.
    project.tempoBpm = imported.tempoBpm;
    project.timeSig = imported.timeSig;
    for (auto& t : imported.tracks()) {
        auto* src = dynamic_cast<model::MidiTrack*>(t.get());
        auto cmd = std::make_unique<model::AddTrackCommand>(src->name);
        auto* raw = cmd.get();
        undo.perform(std::move(cmd));
        if (auto* dst = project.midiTrack(raw->trackId())) {
            dst->channel = src->channel;
            dst->plugin = defaultInstrument();
            std::vector<model::Note> notes(src->notes().begin(), src->notes().end());
            for (auto& n : notes) n.id = 0;
            undo.perform(std::make_unique<model::AddNotesCommand>(dst->id(), notes, "Import MIDI"));
        }
    }
    return true;
}

bool AppContext::exportMidi(const juce::File& f, juce::String& error)
{
    std::string e;
    if (!io::MidiFile::writeFile(project, f.getFullPathName().toStdString(), &e)) { error = e; return false; }
    return true;
}

bool AppContext::exportMusicXml(const juce::File& f, juce::String& error)
{
    std::string e;
    if (!io::MusicXml::writeFile(project, f.getFullPathName().toStdString(), &e)) { error = e; return false; }
    return true;
}

// ---------------------------------------------------------------- tracks
void AppContext::addTrack()
{
    auto cmd = std::make_unique<model::AddTrackCommand>();
    auto* raw = cmd.get();
    undo.perform(std::move(cmd));
    if (auto* t = project.midiTrack(raw->trackId()); t && t->plugin.empty()) {
        t->plugin = defaultInstrument();
        project.notifyChanged();
    }
}

void AppContext::deleteTrack(model::TrackId id)
{
    if (project.track(id)) undo.perform(std::make_unique<model::RemoveTrackCommand>(id));
}

void AppContext::selectTrack(model::TrackId id)
{
    if (project.activeTrack == id) return;
    for (auto& e : chordInput.allNotesOff()) engine->pushLive(activeTrackAtomic, juce::MidiMessage::noteOff(activeChannelAtomic + 1, e.pitch));
    project.activeTrack = id;
    suppressDirty = true;
    project.notifyChanged();
    suppressDirty = false;
}

void AppContext::setTrackPlugin(model::TrackId id, const plugins::PluginInfo* info)
{
    auto* t = project.track(id);
    if (!t) return;
    auto before = model::TrackProperties::from(*t);
    auto after = before;
    after.plugin = info ? plugins::PluginManager::referenceFor(*info) : model::PluginReference{};
    undo.perform(std::make_unique<model::SetTrackPropertiesCommand>(id, before, after));
}

void AppContext::setTrackBypass(model::TrackId id, bool b)
{
    auto* t = project.track(id);
    if (!t) return;
    auto before = model::TrackProperties::from(*t);
    auto after = before;
    after.plugin.bypassed = b;
    undo.perform(std::make_unique<model::SetTrackPropertiesCommand>(id, before, after));
}

void AppContext::closeEditor(model::TrackId id) { editorWindows.erase(id); }

void AppContext::openPluginEditor(model::TrackId id)
{
    if (auto it = editorWindows.find(id); it != editorWindows.end()) { it->second->toFront(true); return; }
    auto* inst = engine->instrument(id);
    if (!inst || !inst->hasEditor()) return;
    if (auto* ed = inst->createEditor()) {
        auto* t = project.track(id);
        editorWindows[id] = std::make_unique<PluginEditorWindow>(
            juce::String(t ? t->name : "") + " - " + inst->name(), ed, [this, id] { closeEditor(id); });
    }
}

// ---------------------------------------------------------------- transport
void AppContext::play() { engine->play(); sendChangeMessage(); }

void AppContext::stop()
{
    if (!engine) return;
    const bool wasPlaying = engine->isPlaying();
    engine->stop();
    if (recorder.isRecording()) {
        auto notes = recorder.finish((model::Tick)engine->position());
        if (!notes.empty() && project.midiTrack(project.activeTrack))
            undo.perform(std::make_unique<model::AddNotesCommand>(project.activeTrack, notes, "Record"));
    } else if (!wasPlaying) {
        engine->setPosition(0); // second Stop returns to start
    }
    sendChangeMessage();
}

void AppContext::toggleRecord()
{
    if (recorder.isRecording()) { stop(); return; }
    if (!activeMidiTrack()) return;
    recorder.start(project.loopStart, project.loopEnd, project.loopEnabled);
    play();
}

void AppContext::returnToStart() { engine->setPosition(project.loopEnabled ? project.loopStart : 0); sendChangeMessage(); }
void AppContext::setPosition(model::Tick t) { engine->setPosition(t); sendChangeMessage(); }

void AppContext::setTempo(double bpm)
{
    project.tempoBpm = juce::jlimit(10.0, 999.0, bpm);
    project.notifyChanged();
}

void AppContext::setTimeSignature(int num, int den)
{
    project.timeSig = {juce::jlimit(1, 32, num), den};
    project.notifyChanged();
}

void AppContext::setLoop(bool enabled, model::Tick start, model::Tick end)
{
    project.loopEnabled = enabled;
    project.loopStart = std::max<model::Tick>(0, start);
    project.loopEnd = std::max(project.loopStart + 1, end);
    project.notifyChanged();
}

// ---------------------------------------------------------------- harmony
void AppContext::setHarmony(int root, const std::string& scaleId, bool chordMode)
{
    {
        std::lock_guard l(chordMutex);
        if (chordInput.chordMode() != chordMode)
            for (auto& e : chordInput.allNotesOff()) engine->pushLive(activeTrackAtomic, juce::MidiMessage::noteOff(activeChannelAtomic + 1, e.pitch));
        chordInput.setScale(root, scaleId);
        chordInput.setChordMode(chordMode);
    }
    project.harmony = {root, scaleId, chordMode};
    project.notifyChanged();
}

void AppContext::setModifierLatched(input::Modifier m, bool on)
{
    { std::lock_guard l(chordMutex); chordInput.setLatched(m, on); }
    sendChangeMessage();
}

bool AppContext::isModifierLatched(input::Modifier m) const
{
    std::lock_guard l(chordMutex);
    return chordInput.isLatched(m);
}

theory::ChordRequest AppContext::chordRequestFor(int pitch) const
{
    std::lock_guard l(chordMutex);
    return chordInput.requestFor(pitch);
}

input::ChordDisplayState AppContext::chordDisplay() const
{
    std::lock_guard l(chordMutex);
    return chordInput.displayState();
}

void AppContext::beginMidiLearn(input::Modifier m)
{
    { std::lock_guard l(chordMutex); chordInput.beginLearn(m); }
    sendChangeMessage();
}

std::optional<input::Modifier> AppContext::learningModifier() const
{
    std::lock_guard l(chordMutex);
    return chordInput.learning();
}

void AppContext::setControllerMapping(const input::ControllerMapping& m)
{
    { std::lock_guard l(chordMutex); chordInput.setMapping(m); }
    settings.setControllerMapping(m);
    project.controllerMapping = m;
    sendChangeMessage();
}

// ---------------------------------------------------------------- live input
void AppContext::sendLive(const std::vector<input::OutputEvent>& events)
{
    const auto track = activeTrackAtomic.load();
    const int ch = activeChannelAtomic.load() + 1;
    const auto now = (model::Tick)engine->position();
    const bool rec = recorder.isRecording();
    for (auto& e : events) {
        engine->pushLive(track, e.noteOn ? juce::MidiMessage::noteOn(ch, e.pitch, (juce::uint8)e.velocity)
                                         : juce::MidiMessage::noteOff(ch, e.pitch));
        if (rec) { if (e.noteOn) recorder.noteOn(e.pitch, e.velocity, now); else recorder.noteOff(e.pitch, now); }
    }
}

void AppContext::handleKeyboardNote(bool on, int pitch, int velocity)
{
    std::vector<input::OutputEvent> out;
    std::optional<input::Modifier> learned;
    input::ControllerMapping mapping;
    {
        std::lock_guard l(chordMutex);
        out = on ? chordInput.noteOn(pitch, velocity) : chordInput.noteOff(pitch);
        learned = chordInput.takeLearnedModifier();
        mapping = chordInput.mapping();
    }
    sendLive(out);
    if (learned)
        juce::MessageManager::callAsync([this, mapping] { setControllerMapping(mapping); });
    // The chord display is refreshed by MainComponent's 30 Hz timer. Broadcasting a
    // change per note would rebuild the menu bar, track list and piano roll on
    // every key press, which makes playing laggy with heavy plugins open.
}

void AppContext::handleMidi(const juce::MidiMessage& m)
{
    if (m.isNoteOn()) handleKeyboardNote(true, m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff()) handleKeyboardNote(false, m.getNoteNumber(), 0);
    else if (m.isController() || m.isPitchWheel() || m.isChannelPressure() || m.isAftertouch()) {
        auto copy = m;
        copy.setChannel(activeChannelAtomic.load() + 1);
        engine->pushLive(activeTrackAtomic, copy);
    }
}

void AppContext::previewNotes(const std::vector<int>& pitches, int velocity, int ms)
{
    const auto track = activeTrackAtomic.load();
    const int ch = activeChannelAtomic.load() + 1;
    for (int p : pitches) engine->pushLive(track, juce::MidiMessage::noteOn(ch, p, (juce::uint8)velocity));
    auto* eng = engine.get();
    juce::Timer::callAfterDelay(ms, [eng, track, ch, pitches] {
        for (int p : pitches) eng->pushLive(track, juce::MidiMessage::noteOff(ch, p));
    });
}

} // namespace mc
