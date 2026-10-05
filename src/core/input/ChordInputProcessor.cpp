#include "input/ChordInputProcessor.h"

namespace mc::input {

void ChordInputProcessor::setScale(int rootPc, const std::string& scaleId)
{
    root_ = rootPc;
    scaleId_ = scaleId;
}

theory::ChordRequest ChordInputProcessor::requestFor(int pitch) const
{
    theory::ChordRequest r;
    r.playedNote = pitch;
    if (active(Modifier::Ninth)) r.extension = theory::Extension::Ninth;
    else if (active(Modifier::Seventh)) r.extension = theory::Extension::Seventh;
    if (active(Modifier::Inversion2)) r.inversion = 2;
    else if (active(Modifier::Inversion1)) r.inversion = 1;
    return r;
}

theory::ChordResult ChordInputProcessor::chordFor(int pitch) const
{
    const theory::Scale scale(root_, theory::ScaleRegistry::instance().byIdOrDefault(scaleId_));
    return theory::ChordEngine::build(scale, requestFor(pitch));
}

void ChordInputProcessor::soundOn(int p, int v, std::vector<OutputEvent>& out)
{
    if (soundingCount_[p]++ == 0) out.push_back({true, p, v});
}

void ChordInputProcessor::soundOff(int p, std::vector<OutputEvent>& out)
{
    auto it = soundingCount_.find(p);
    if (it == soundingCount_.end()) return;
    if (--it->second <= 0) { soundingCount_.erase(it); out.push_back({false, p, 0}); }
}

std::vector<OutputEvent> ChordInputProcessor::noteOn(int pitch, int velocity)
{
    std::vector<OutputEvent> out;
    if (learning_) {
        // A key can only have one role.
        for (auto& n : mapping_.notes) if (n == pitch) n = -1;
        mapping_[*learning_] = pitch;
        learned_ = learning_;
        learning_.reset();
        return out;
    }
    if (chordMode_) {
        const int mod = mapping_.modifierFor(pitch);
        if (mod >= 0) { held_[(size_t)mod] = true; return out; } // modifiers never sound
    }
    if (keyToNotes_.count(pitch)) { // retrigger without release: release first
        auto o = noteOff(pitch);
        out.insert(out.end(), o.begin(), o.end());
    }
    std::vector<int> notes{pitch};
    if (chordMode_) {
        notes = chordFor(pitch).notes;
        lastRoot_ = pitch;
    }
    for (int n : notes) soundOn(n, velocity, out);
    keyToNotes_[pitch] = notes;
    return out;
}

std::vector<OutputEvent> ChordInputProcessor::noteOff(int pitch)
{
    std::vector<OutputEvent> out;
    const int mod = mapping_.modifierFor(pitch);
    if (mod >= 0 && held_[(size_t)mod]) { held_[(size_t)mod] = false; return out; }
    auto it = keyToNotes_.find(pitch);
    if (it == keyToNotes_.end()) return out;
    for (int n : it->second) soundOff(n, out);
    keyToNotes_.erase(it);
    return out;
}

std::vector<OutputEvent> ChordInputProcessor::allNotesOff()
{
    std::vector<OutputEvent> out;
    for (auto& [p, c] : soundingCount_) out.push_back({false, p, 0});
    soundingCount_.clear();
    keyToNotes_.clear();
    held_ = {};
    return out;
}

ChordDisplayState ChordInputProcessor::displayState() const
{
    ChordDisplayState s;
    s.chordMode = chordMode_;
    for (int i = 0; i < (int)Modifier::Count; ++i) s.modifiersHeld[(size_t)i] = active((Modifier)i);
    // Re-evaluated with the current modifiers so the display reacts immediately.
    if (chordMode_ && lastRoot_ >= 0) { s.currentChord = chordFor(lastRoot_).description; s.lastRoot = lastRoot_; }
    for (auto& [p, c] : soundingCount_) s.soundingNotes.push_back(p);
    return s;
}

} // namespace mc::input
