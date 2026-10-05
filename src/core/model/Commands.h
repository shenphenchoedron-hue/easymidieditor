#pragma once
// Every model mutation from the UI goes through a Command so it can be undone.
#include "model/Project.h"
#include <memory>
#include <string>
#include <vector>

namespace mc::model {

class Command {
public:
    virtual ~Command() = default;
    virtual void apply(Project&) = 0;
    virtual void revert(Project&) = 0;
    virtual std::string name() const = 0;
};

class UndoStack {
public:
    explicit UndoStack(Project& p) : project_(p) {}
    void perform(std::unique_ptr<Command> c);   // applies + records + notifies
    bool undo();
    bool redo();
    bool canUndo() const { return !done_.empty(); }
    bool canRedo() const { return !undone_.empty(); }
    void clear() { done_.clear(); undone_.clear(); }
    std::string undoName() const { return done_.empty() ? "" : done_.back()->name(); }
private:
    Project& project_;
    std::vector<std::unique_ptr<Command>> done_, undone_;
    static constexpr size_t kMaxDepth = 500;
};

// Adds notes (single note, pasted notes, a generated chord, a recorded take).
class AddNotesCommand : public Command {
public:
    AddNotesCommand(TrackId t, std::vector<Note> notes, std::string name = "Add notes")
        : track_(t), notes_(std::move(notes)), name_(std::move(name)) {}
    void apply(Project&) override;
    void revert(Project&) override;
    std::string name() const override { return name_; }
    const std::vector<NoteId>& addedIds() const { return ids_; }
private:
    TrackId track_;
    std::vector<Note> notes_;
    std::vector<NoteId> ids_;
    std::string name_;
};

class RemoveNotesCommand : public Command {
public:
    RemoveNotesCommand(TrackId t, std::vector<NoteId> ids) : track_(t), ids_(std::move(ids)) {}
    void apply(Project&) override;
    void revert(Project&) override;
    std::string name() const override { return "Delete notes"; }
private:
    TrackId track_;
    std::vector<NoteId> ids_;
    std::vector<Note> removed_;
};

// Move / resize / velocity change: stores before and after states (same ids).
class ModifyNotesCommand : public Command {
public:
    ModifyNotesCommand(TrackId t, std::vector<Note> before, std::vector<Note> after, std::string name = "Edit notes")
        : track_(t), before_(std::move(before)), after_(std::move(after)), name_(std::move(name)) {}
    void apply(Project&) override;
    void revert(Project&) override;
    std::string name() const override { return name_; }
private:
    TrackId track_;
    std::vector<Note> before_, after_;
    std::string name_;
};

class AddTrackCommand : public Command {
public:
    explicit AddTrackCommand(std::string name = {}) : name_(std::move(name)) {}
    void apply(Project&) override;
    void revert(Project&) override;
    std::string name() const override { return "Add track"; }
    TrackId trackId() const { return id_; }
private:
    std::string name_;
    TrackId id_ = 0;
    std::unique_ptr<Track> stash_;
};

class RemoveTrackCommand : public Command {
public:
    explicit RemoveTrackCommand(TrackId id) : id_(id) {}
    void apply(Project&) override;
    void revert(Project&) override;
    std::string name() const override { return "Delete track"; }
private:
    TrackId id_;
    int index_ = -1;
    TrackId prevActive_ = 0;
    std::unique_ptr<Track> stash_;
};

struct TrackProperties {
    std::string name;
    bool mute = false, solo = false;
    float volume = 0.8f, pan = 0.0f;
    int channel = 0;
    PluginReference plugin;
    static TrackProperties from(const Track&);
    void applyTo(Track&) const;
};

class SetTrackPropertiesCommand : public Command {
public:
    SetTrackPropertiesCommand(TrackId id, TrackProperties before, TrackProperties after)
        : id_(id), before_(std::move(before)), after_(std::move(after)) {}
    void apply(Project&) override;
    void revert(Project&) override;
    std::string name() const override { return "Change track"; }
private:
    TrackId id_;
    TrackProperties before_, after_;
};

} // namespace mc::model
