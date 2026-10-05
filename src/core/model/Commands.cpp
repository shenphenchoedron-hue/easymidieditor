#include "model/Commands.h"

namespace mc::model {

void UndoStack::perform(std::unique_ptr<Command> c)
{
    c->apply(project_);
    done_.push_back(std::move(c));
    if (done_.size() > kMaxDepth) done_.erase(done_.begin());
    undone_.clear();
    project_.notifyChanged();
}

bool UndoStack::undo()
{
    if (done_.empty()) return false;
    auto c = std::move(done_.back());
    done_.pop_back();
    c->revert(project_);
    undone_.push_back(std::move(c));
    project_.notifyChanged();
    return true;
}

bool UndoStack::redo()
{
    if (undone_.empty()) return false;
    auto c = std::move(undone_.back());
    undone_.pop_back();
    c->apply(project_);
    done_.push_back(std::move(c));
    project_.notifyChanged();
    return true;
}

// ---- notes
void AddNotesCommand::apply(Project& p)
{
    auto* t = p.midiTrack(track_);
    if (!t) return;
    const bool first = ids_.empty();
    for (size_t i = 0; i < notes_.size(); ++i) {
        Note n = notes_[i];
        n.id = first ? 0 : ids_[i]; // reuse ids on redo
        const NoteId id = t->addNote(n);
        if (first) ids_.push_back(id);
    }
}

void AddNotesCommand::revert(Project& p)
{
    if (auto* t = p.midiTrack(track_))
        for (auto id : ids_) t->removeNote(id);
}

void RemoveNotesCommand::apply(Project& p)
{
    removed_.clear();
    auto* t = p.midiTrack(track_);
    if (!t) return;
    for (auto id : ids_)
        if (auto* n = t->findNote(id)) { removed_.push_back(*n); t->removeNote(id); }
}

void RemoveNotesCommand::revert(Project& p)
{
    if (auto* t = p.midiTrack(track_))
        for (auto& n : removed_) t->addNote(n);
}

void ModifyNotesCommand::apply(Project& p)
{
    if (auto* t = p.midiTrack(track_)) for (auto& n : after_) t->updateNote(n);
}

void ModifyNotesCommand::revert(Project& p)
{
    if (auto* t = p.midiTrack(track_)) for (auto& n : before_) t->updateNote(n);
}

// ---- tracks
void AddTrackCommand::apply(Project& p)
{
    if (stash_) { id_ = stash_->id(); p.insertTrack((int)p.tracks().size(), std::move(stash_)); }
    else id_ = p.addMidiTrack(name_).id();
    p.activeTrack = id_;
}

void AddTrackCommand::revert(Project& p) { stash_ = p.removeTrack(id_); }

void RemoveTrackCommand::apply(Project& p)
{
    prevActive_ = p.activeTrack;
    index_ = p.indexOf(id_);
    stash_ = p.removeTrack(id_);
}

void RemoveTrackCommand::revert(Project& p)
{
    if (stash_) p.insertTrack(index_, std::move(stash_));
    p.activeTrack = prevActive_;
}

TrackProperties TrackProperties::from(const Track& t)
{
    TrackProperties tp;
    tp.name = t.name; tp.mute = t.mute; tp.solo = t.solo; tp.volume = t.volume; tp.pan = t.pan; tp.colour = t.colour;
    if (auto* m = dynamic_cast<const MidiTrack*>(&t)) { tp.channel = m->channel; tp.plugin = m->plugin; }
    return tp;
}

void TrackProperties::applyTo(Track& t) const
{
    t.name = name; t.mute = mute; t.solo = solo; t.volume = volume; t.pan = pan; t.colour = colour;
    if (auto* m = dynamic_cast<MidiTrack*>(&t)) { m->channel = channel; m->plugin = plugin; }
}

void SetTrackPropertiesCommand::apply(Project& p)  { if (auto* t = p.track(id_)) after_.applyTo(*t); }
void SetTrackPropertiesCommand::revert(Project& p) { if (auto* t = p.track(id_)) before_.applyTo(*t); }

} // namespace mc::model
