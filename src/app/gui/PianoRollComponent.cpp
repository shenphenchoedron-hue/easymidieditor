#include "gui/PianoRollComponent.h"
#include "sequencer/Timing.h"
#include "theory/ChordEngine.h"
#include "gui/Theme.h"

namespace mc::gui {

using model::Note;
using model::Tick;

namespace {
constexpr int kKeyboardWidth = 64;
constexpr int kTimelineHeight = 26;
constexpr int kVelocityHeight = 64;
constexpr int kScrollbar = 10;

bool isBlackKey(int p) { const int pc = p % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

theory::Scale currentScale(const AppContext& app)
{
    return {app.project.harmony.root, theory::ScaleRegistry::instance().byIdOrDefault(app.project.harmony.scaleId)};
}

namespace tc = theme::col;
const juce::Colour kBg = tc::rollBg, kRowBlack = tc::rollRowBlack, kRowWhite = tc::rollRowWhite, kRowScale = tc::rollRowScale,
    kLineGrid = tc::gridSub, kLineBeat = tc::gridBeat, kLineBar = tc::gridBar, kNote = tc::note, kNoteSel = tc::noteSel,
    kPlayhead = tc::playhead, kLoop = tc::loop, kRec = tc::record;

// Notes use the track's colour (set in the track list); default theme colour if none.
juce::Colour trackNoteColour(const model::Track& t)
{
    return t.colour ? juce::Colour(t.colour).withAlpha(1.0f) : kNote;
}
} // namespace

// ============================================================ Timeline
class PianoRollComponent::Timeline final : public juce::Component {
public:
    explicit Timeline(PianoRollComponent& r) : roll(r) {}
    void paint(juce::Graphics& g) override
    {
        auto& p = roll.app.project;
        g.fillAll(tc::timelineBg);
        g.setColour(tc::gridBeat);
        g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());
        const Tick bar = seq::ticksPerBar(p.timeSig), beat = seq::ticksPerBeat(p.timeSig);
        if (p.loopEnabled || dragLoop) {
            const auto x1 = (float)roll.tickToX((double)p.loopStart), x2 = (float)roll.tickToX((double)p.loopEnd);
            g.setColour(p.loopEnabled ? tc::accent.withAlpha(0.55f) : juce::Colour(0x33888888));
            g.fillRoundedRectangle(x1, 2.0f, x2 - x1, 7.0f, 3.0f);
        }
        const Tick first = std::max<Tick>(0, (Tick)roll.xToTick(0) / beat * beat);
        const Tick last = (Tick)roll.xToTick(getWidth()) + beat;
        g.setFont(theme::uiFont(11.5f, true));
        for (Tick t = first; t <= last; t += beat) {
            const float x = (float)roll.tickToX((double)t);
            const bool isBar = t % bar == 0;
            g.setColour(isBar ? tc::gridBar : tc::gridBeat);
            g.drawVerticalLine((int)x, isBar ? 4.0f : (float)getHeight() * 0.65f, (float)getHeight());
            if (isBar && roll.pxPerTick * (double)bar > 28) {
                g.setColour(juce::Colour(0xffc9d1da));
                g.drawText(juce::String(t / bar + 1), (int)x + 4, 9, 40, 14, juce::Justification::left);
            }
        }
        const float px = (float)roll.tickToX(roll.app.engine->position());
        g.setColour(kPlayhead);
        juce::Path tri;
        tri.addTriangle(px - 6, (float)getHeight() - 10, px + 6, (float)getHeight() - 10, px, (float)getHeight());
        g.fillPath(tri);
    }
    // Click: set position. Shift/right-drag: define loop range.
    void mouseDown(const juce::MouseEvent& e) override
    {
        dragLoop = e.mods.isShiftDown() || e.mods.isPopupMenu();
        anchor = roll.snap(roll.xToTick(e.x));
        if (!dragLoop) roll.app.setPosition(std::max<Tick>(0, (Tick)roll.xToTick(e.x)));
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const Tick t = roll.snap(roll.xToTick(e.x));
        if (dragLoop) {
            if (t != anchor) roll.app.setLoop(true, std::min(anchor, t), std::max(anchor, t));
        } else roll.app.setPosition(std::max<Tick>(0, (Tick)roll.xToTick(e.x)));
    }
    void mouseUp(const juce::MouseEvent&) override { dragLoop = false; }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        roll.zoomHorizontal(w.deltaY > 0 ? 1.15 : 1 / 1.15, e.x);
    }
private:
    PianoRollComponent& roll;
    bool dragLoop = false;
    Tick anchor = 0;
};

// ============================================================ Keyboard
class PianoRollComponent::Keyboard final : public juce::Component {
public:
    explicit Keyboard(PianoRollComponent& r) : roll(r) {}
    void paint(juce::Graphics& g) override
    {
        const auto scale = currentScale(roll.app);
        const auto disp = roll.app.chordDisplay();
        const std::set<int> sounding(disp.soundingNotes.begin(), disp.soundingNotes.end());
        g.fillAll(tc::keyWhite);
        const float w = (float)getWidth(), blackW = w * 0.62f;
        for (int p = 0; p < 128; ++p) {
            const float y = (float)roll.pitchToY(p), h = (float)roll.rowHeight;
            if (y > getHeight() || y + h < 0) continue;
            const bool black = isBlackKey(p);
            const bool lit = sounding.count(p) || p == mousePitch;
            // white key strip (full width) with a subtle in-scale tint
            juce::Colour wc = tc::keyWhite;
            if (!black && scale.contains(p)) wc = wc.interpolatedWith(tc::accent, 0.08f);
            if (!black && lit) wc = tc::warm;
            g.setColour(wc);
            g.fillRect(0.0f, y, w, h);
            if (black) {
                g.setColour(tc::keyWhite);
                g.fillRect(blackW, y, w - blackW, h);
                juce::Colour bc = tc::keyBlack;
                if (scale.contains(p)) bc = bc.interpolatedWith(tc::accent, 0.22f);
                if (lit) bc = tc::warm.darker(0.15f);
                g.setColour(bc);
                g.fillRoundedRectangle(0.0f, y + 0.5f, blackW, h - 1.0f, 2.0f);
            }
            const bool isC = p % 12 == 0;
            g.setColour(isC ? juce::Colour(0xff9aa5b1) : tc::keyLine);
            g.drawHorizontalLine((int)(y + h), black ? blackW : 0.0f, w);
            if (isC || (scale.degreeOf(p) == 0 && h >= 10)) {
                g.setColour(isC ? tc::text : tc::textDim);
                g.setFont(theme::uiFont(juce::jmin(11.5f, h), isC));
                g.drawText(theory::midiNoteName(p), 2, (int)y, getWidth() - 6, (int)h, juce::Justification::centredRight);
            }
        }
        g.setColour(tc::border);
        g.drawVerticalLine(getWidth() - 1, 0.0f, (float)getHeight());
    }
    void mouseDown(const juce::MouseEvent& e) override { press(roll.yToPitch(e.y)); }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const int p = roll.yToPitch(e.y);
        if (p != mousePitch) { release(); press(p); }
    }
    void mouseUp(const juce::MouseEvent&) override { release(); }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isAltDown()) roll.zoomVertical(w.deltaY > 0 ? 1.1 : 1 / 1.1, e.y);
        else roll.scrollBy(0, -w.deltaY * 200);
    }
private:
    void press(int p) { mousePitch = p; roll.app.handleKeyboardNote(true, p, 100); repaint(); }
    void release() { if (mousePitch >= 0) roll.app.handleKeyboardNote(false, mousePitch, 0); mousePitch = -1; repaint(); }
    PianoRollComponent& roll;
    int mousePitch = -1;
};

// ============================================================ Grid
class PianoRollComponent::Grid final : public juce::Component {
public:
    explicit Grid(PianoRollComponent& r) : roll(r) { setWantsKeyboardFocus(true); }

    void paint(juce::Graphics& g) override
    {
        auto& app = roll.app;
        auto& p = app.project;
        const auto scale = currentScale(app);
        g.fillAll(kBg);

        for (int pitch = 0; pitch < 128; ++pitch) {
            const float y = (float)roll.pitchToY(pitch), h = (float)roll.rowHeight;
            if (y > getHeight() || y + h < 0) continue;
            g.setColour(scale.contains(pitch) ? kRowScale : (isBlackKey(pitch) ? kRowBlack : kRowWhite));
            g.fillRect(0.0f, y, (float)getWidth(), h);
            g.setColour(pitch % 12 == 0 ? tc::rollRowC : kRowBlack.darker(0.25f));
            g.drawHorizontalLine((int)(y + h), 0.0f, (float)getWidth());
        }

        const Tick bar = seq::ticksPerBar(p.timeSig), beat = seq::ticksPerBeat(p.timeSig);
        Tick step = roll.gridTicks();
        while (step * roll.pxPerTick < 6) step *= 2;
        const Tick first = std::max<Tick>(0, (Tick)roll.xToTick(0) / step * step);
        for (Tick t = first; roll.tickToX((double)t) < getWidth(); t += step) {
            const bool isBar = t % bar == 0, isBeat = t % beat == 0;
            g.setColour(isBar ? kLineBar.withAlpha(0.75f) : isBeat ? kLineBeat : kLineGrid);
            g.drawVerticalLine((int)roll.tickToX((double)t), 0.0f, (float)getHeight());
        }

        if (p.loopEnabled) {
            const auto x1 = (float)roll.tickToX((double)p.loopStart), x2 = (float)roll.tickToX((double)p.loopEnd);
            g.setColour(kLoop);
            g.fillRect(x1, 0.0f, x2 - x1, (float)getHeight());
        }

        // Other tracks shine through in their own colour, like looking through
        // tracing paper at the layers below. Not clickable.
        const auto bounds = getLocalBounds().toFloat();
        for (auto& t : p.tracks())
            if (auto* mt = dynamic_cast<model::MidiTrack*>(t.get()); mt && mt->id() != p.activeTrack) {
                const auto ghost = trackNoteColour(*mt).withMultipliedSaturation(0.6f); // muted: not the active track
                for (auto& n : mt->notes()) {
                    const auto r = noteRect(n).reduced(0.5f, 1.0f);
                    if (!r.intersects(bounds)) continue;
                    g.setColour(ghost.withAlpha(0.22f));
                    g.fillRoundedRectangle(r, 3.0f);
                    g.setColour(ghost.withAlpha(0.45f));
                    g.drawRoundedRectangle(r, 3.0f, 1.0f);
                }
            }

        if (auto* track = app.activeMidiTrack()) {
            const auto base = trackNoteColour(*track);
            for (auto& n : track->notes()) {
                auto r = noteRect(n);
                if (!r.intersects(bounds)) continue;
                const bool sel = roll.selection.count(n.id) > 0;
                // Active track in full colour; soft notes only slightly darker. Red = about to be deleted.
                auto c = (sel ? kNoteSel : base).interpolatedWith(kBg, 0.20f * (1.0f - (float)n.velocity / 127.0f));
                if (pendingDelete.count(n.id)) c = kRec.withAlpha(0.85f);
                const auto nr = r.reduced(0.5f, 1.0f);
                g.setColour(c);
                g.fillRoundedRectangle(nr, 3.0f);
                g.setColour(sel ? juce::Colour(0xfffff1d6) : c.darker(0.45f));
                g.drawRoundedRectangle(nr, 3.0f, 1.0f);
                if (r.getHeight() >= 11 && r.getWidth() > 26) {
                    g.setColour(juce::Colour(0xff10161c).withAlpha(0.75f));
                    g.setFont(juce::jmin(11.0f, r.getHeight() - 1));
                    g.drawText(theory::midiNoteName(n.pitch), r.reduced(3, 0), juce::Justification::centredLeft, false);
                }
            }
        }

        if (app.isRecording()) {
            g.setColour(kRec.withAlpha(0.8f));
            for (auto& n : app.recorder.pending()) g.fillRect(noteRect(n));
        }

        if (rubber) {
            const auto c = mode == Mode::RubberDelete ? kRec : juce::Colours::white; // red = delete box
            g.setColour(c.withAlpha(0.15f));
            g.fillRect(*rubber);
            g.setColour(c);
            g.drawRect(*rubber);
        }

        const float px = (float)roll.tickToX(app.engine->position());
        g.setColour(app.isRecording() ? kRec : kPlayhead);
        g.drawLine(px, 0, px, (float)getHeight(), 1.5f);

        if (!app.activeMidiTrack()) {
            g.setColour(tc::textDim);
            g.drawText("No MIDI track selected", getLocalBounds(), juce::Justification::centred);
        }
    }

    juce::Rectangle<float> noteRect(const Note& n) const
    {
        const auto x = (float)roll.tickToX((double)n.start);
        const auto w = std::max(3.0f, (float)(n.length * roll.pxPerTick));
        return {x, (float)roll.pitchToY(n.pitch), w, (float)roll.rowHeight};
    }

    const Note* noteAt(juce::Point<float> pos) const
    {
        auto* t = roll.app.activeMidiTrack();
        if (!t) return nullptr;
        const Note* hit = nullptr;
        for (auto& n : t->notes()) if (noteRect(n).contains(pos)) hit = &n; // topmost = last
        return hit;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        auto& app = roll.app;
        auto* track = app.activeMidiTrack();
        if (!track) return;
        roll.selectionTrack = track->id();
        mode = Mode::None;
        downTick = roll.xToTick(e.x);
        downPitch = roll.yToPitch(e.y);
        const Note* hit = noteAt(e.position);

        if (e.mods.isPopupMenu()) {
            // Right click on a note deletes it. Right-drag draws a delete box:
            // every note touched by the box is deleted on release (one undo step).
            mode = Mode::RubberDelete;
            rubberStart = e.position;
            pendingDelete.clear();
            if (hit) pendingDelete.insert(hit->id);
            repaint();
            return;
        }

        if (hit) {
            if (e.mods.isShiftDown()) {
                if (!roll.selection.erase(hit->id)) roll.selection.insert(hit->id);
            } else if (!roll.selection.count(hit->id)) {
                roll.selection = {hit->id};
            }
            anchorId = hit->id;
            const auto edge = edgeAt(*hit, e.position.x);
            mode = edge > 0 ? Mode::Resize : edge < 0 ? Mode::ResizeLeft : Mode::Move;
            beginDrag(*track);
            app.previewNotes({hit->pitch}, hit->velocity, 200);
            repaint();
            return;
        }

        if (e.mods.isCommandDown() || e.mods.isShiftDown()) {
            mode = Mode::Rubber;
            if (!e.mods.isShiftDown()) roll.selection.clear();
            rubberStart = e.position;
            return;
        }

        // create: a single note, or a whole diatonic chord in Chord Input Mode
        const Tick start = roll.app.snapEnabled ? seq::snapFloor((Tick)std::max(0.0, downTick), roll.gridTicks()) : (Tick)std::max(0.0, downTick);
        std::vector<int> pitches{downPitch};
        if (app.project.harmony.chordMode)
            pitches = theory::ChordEngine::build(currentScale(app), app.chordRequestFor(downPitch)).notes;
        std::vector<Note> notes;
        for (int p : pitches) notes.push_back({0, p, lastVelocity, start, lastLength, -1});
        auto cmd = std::make_unique<model::AddNotesCommand>(track->id(), notes, pitches.size() > 1 ? "Insert chord" : "Add note");
        auto* raw = cmd.get();
        app.undo.perform(std::move(cmd));
        roll.selection = std::set<model::NoteId>(raw->addedIds().begin(), raw->addedIds().end());
        anchorId = raw->addedIds().empty() ? 0 : raw->addedIds().front();
        app.previewNotes(pitches, lastVelocity, 250);
        mode = Mode::Resize;
        beginDrag(*track);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        auto* track = roll.app.activeMidiTrack();
        if (!track) return;
        if (mode == Mode::RubberDelete) {
            rubber = juce::Rectangle<float>(rubberStart, e.position);
            pendingDelete.clear();
            for (auto& n : track->notes()) if (noteRect(n).intersects(*rubber) || noteRect(n).contains(rubberStart)) pendingDelete.insert(n.id);
            repaint();
            return;
        }
        if (mode == Mode::Rubber) {
            rubber = juce::Rectangle<float>(rubberStart, e.position);
            std::set<model::NoteId> sel = e.mods.isShiftDown() ? roll.selection : std::set<model::NoteId>{};
            for (auto& n : track->notes()) if (noteRect(n).intersects(*rubber)) sel.insert(n.id);
            roll.selection = sel;
            repaint();
            return;
        }
        if (mode != Mode::Move && mode != Mode::Resize && mode != Mode::ResizeLeft) return;
        const double dtRaw = roll.xToTick(e.x) - downTick;
        const Note* anchor = nullptr;
        for (auto& o : original) if (o.id == anchorId) anchor = &o;
        if (!anchor && !original.empty()) anchor = &original.front();
        if (!anchor) return;

        if (mode == Mode::Move) {
            const Tick newStart = roll.snap((double)anchor->start + dtRaw);
            const Tick dt = std::max<Tick>(newStart - anchor->start, -minStart);
            const int dp = juce::jlimit(-minPitch, 127 - maxPitch, roll.yToPitch(e.y) - downPitch);
            for (auto o : original) { o.start += dt; o.pitch += dp; track->updateNote(o); }
            if (dp != lastPreviewDp) { lastPreviewDp = dp; roll.app.previewNotes({anchor->pitch + dp}, anchor->velocity, 150); }
        } else if (mode == Mode::ResizeLeft) {
            // Drag the start edge: the end stays put, the note grows/shrinks to the left.
            const Tick minLen = roll.app.snapEnabled ? std::max<Tick>(1, roll.gridTicks() / 4) : 1;
            const Tick newStart = roll.snap((double)anchor->start + dtRaw);
            Tick dt = newStart - anchor->start;
            dt = std::max<Tick>(dt, -minStart);
            for (auto o : original) {
                const Tick end = o.end();
                o.start = std::clamp<Tick>(o.start + dt, 0, end - minLen);
                o.length = end - o.start;
                track->updateNote(o);
            }
        } else {
            const Tick newEnd = roll.snap((double)anchor->end() + dtRaw);
            const Tick dl = newEnd - anchor->end();
            for (auto o : original) {
                o.length = std::max<Tick>(roll.app.snapEnabled ? roll.gridTicks() / 4 : 1, o.length + dl);
                track->updateNote(o);
            }
        }
        changed = true;
        roll.app.modelChangedWithoutUndo();
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        auto* track = roll.app.activeMidiTrack();
        rubber.reset();
        if (mode == Mode::RubberDelete) {
            mode = Mode::None;
            if (track && !pendingDelete.empty()) {
                for (auto id : pendingDelete) roll.selection.erase(id);
                const std::vector<model::NoteId> ids(pendingDelete.begin(), pendingDelete.end());
                pendingDelete.clear();
                roll.app.undo.perform(std::make_unique<model::RemoveNotesCommand>(track->id(), ids));
            }
            repaint();
            return;
        }
        if (track && changed && (mode == Mode::Move || mode == Mode::Resize || mode == Mode::ResizeLeft)) {
            std::vector<Note> after;
            for (auto& o : original) if (auto* n = track->findNote(o.id)) after.push_back(*n);
            for (auto& o : original) track->updateNote(o); // restore, then apply through the undo stack
            if (auto* a = track->findNote(anchorId)) { lastLength = a->length; }
            for (auto& n : after) if (n.id == anchorId) lastLength = n.length;
            roll.app.undo.perform(std::make_unique<model::ModifyNotesCommand>(track->id(), original, after,
                                                                                mode == Mode::Move ? "Move notes" : "Resize notes"));
        }
        mode = Mode::None;
        changed = false;
        updateCursor(e.position);
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {}

    // Show a resize cursor when hovering over the left or right edge of a note.
    void mouseMove(const juce::MouseEvent& e) override { updateCursor(e.position); }
    void mouseExit(const juce::MouseEvent&) override { setMouseCursor(juce::MouseCursor::NormalCursor); }

    void updateCursor(juce::Point<float> pos)
    {
        if (mode == Mode::Resize || mode == Mode::ResizeLeft) { setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
        const Note* hit = noteAt(pos);
        setMouseCursor(hit && edgeAt(*hit, pos.x) != 0 ? juce::MouseCursor::LeftRightResizeCursor
                                                       : juce::MouseCursor::NormalCursor);
    }

    // -1 = left edge, +1 = right edge, 0 = body. Edge zone shrinks for short notes.
    int edgeAt(const Note& n, float x) const
    {
        const auto r = noteRect(n);
        const float zone = juce::jlimit(2.0f, 7.0f, r.getWidth() / 3.0f);
        if (x >= r.getRight() - zone) return 1;
        if (x <= r.getX() + zone) return -1;
        return 0;
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown()) roll.zoomHorizontal(w.deltaY > 0 ? 1.15 : 1 / 1.15, e.x);
        else if (e.mods.isAltDown()) roll.zoomVertical(w.deltaY > 0 ? 1.1 : 1 / 1.1, e.y);
        else if (e.mods.isShiftDown()) roll.scrollBy(-w.deltaY * 600 / roll.pxPerTick * 0.1, 0);
        else roll.scrollBy(-w.deltaX * 600 / roll.pxPerTick * 0.1, -w.deltaY * 200);
    }

    int lastVelocity = 100;
    Tick lastLength = model::kPPQ / 2;

private:
    enum class Mode { None, Move, Resize, ResizeLeft, Rubber, RubberDelete };
    std::set<model::NoteId> pendingDelete; // notes inside the right-drag delete box
    void beginDrag(const model::MidiTrack& t)
    {
        original.clear();
        minStart = std::numeric_limits<Tick>::max();
        minPitch = 127; maxPitch = 0;
        for (auto id : roll.selection)
            if (auto* n = t.findNote(id)) {
                original.push_back(*n);
                minStart = std::min(minStart, n->start);
                minPitch = std::min(minPitch, n->pitch);
                maxPitch = std::max(maxPitch, n->pitch);
            }
        changed = false;
        lastPreviewDp = 0;
    }

    PianoRollComponent& roll;
    Mode mode = Mode::None;
    double downTick = 0;
    int downPitch = 60;
    model::NoteId anchorId = 0;
    std::vector<Note> original;
    Tick minStart = 0;
    int minPitch = 0, maxPitch = 127, lastPreviewDp = 0;
    bool changed = false;
    juce::Point<float> rubberStart;
    std::optional<juce::Rectangle<float>> rubber;
};

// ============================================================ Velocity lane
class PianoRollComponent::VelocityLane final : public juce::Component {
public:
    explicit VelocityLane(PianoRollComponent& r) : roll(r) {}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(tc::velLaneBg);
        g.setColour(tc::gridBeat);
        g.drawHorizontalLine(0, 0, (float)getWidth());
        auto* t = roll.app.activeMidiTrack();
        if (!t) return;
        for (auto& n : t->notes()) {
            const float x = (float)roll.tickToX((double)n.start);
            if (x < -4 || x > getWidth()) continue;
            const float h = (float)(getHeight() - 4) * (float)n.velocity / 127.0f;
            g.setColour((roll.selection.count(n.id) ? kNoteSel : trackNoteColour(*t)).withAlpha(0.85f));
            g.fillRect(x + 1.0f, (float)getHeight() - h, 2.0f, h);
            g.fillEllipse(x - 1.5f, (float)getHeight() - h - 3, 7, 7);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        before.clear();
        if (auto* t = roll.app.activeMidiTrack()) for (auto& n : t->notes()) before.push_back(n);
        mouseDrag(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        auto* t = roll.app.activeMidiTrack();
        if (!t) return;
        const int vel = juce::jlimit(1, 127, (int)std::round(127.0 * (getHeight() - e.y) / (getHeight() - 4)));
        const bool onlySel = !roll.selection.empty();
        bool any = false;
        for (auto n : t->notes()) {
            const float x = (float)roll.tickToX((double)n.start);
            if (std::abs(x - (float)e.x) > 4) continue;
            if (onlySel && !roll.selection.count(n.id)) continue;
            n.velocity = vel;
            t->updateNote(n);
            any = true;
        }
        if (any) roll.app.modelChangedWithoutUndo();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        auto* t = roll.app.activeMidiTrack();
        if (!t) return;
        std::vector<Note> b, a;
        for (auto& o : before)
            if (auto* n = t->findNote(o.id); n && n->velocity != o.velocity) { b.push_back(o); a.push_back(*n); }
        if (a.empty()) return;
        for (auto& o : b) t->updateNote(o);
        roll.app.undo.perform(std::make_unique<model::ModifyNotesCommand>(t->id(), b, a, "Change velocity"));
    }
private:
    PianoRollComponent& roll;
    std::vector<Note> before;
};

// ============================================================ PianoRollComponent
PianoRollComponent::PianoRollComponent(AppContext& a) : app(a)
{
    timeline = std::make_unique<Timeline>(*this);
    keyboard = std::make_unique<Keyboard>(*this);
    grid = std::make_unique<Grid>(*this);
    velocity = std::make_unique<VelocityLane>(*this);
    for (juce::Component* c : {(juce::Component*)timeline.get(), (juce::Component*)keyboard.get(),
                               (juce::Component*)grid.get(), (juce::Component*)velocity.get()})
        addAndMakeVisible(c);
    addAndMakeVisible(hbar);
    addAndMakeVisible(vbar);
    hbar.addListener(this);
    vbar.addListener(this);
    hbar.setAutoHide(false);
    vbar.setAutoHide(false);
    for (auto* b : {&hbar, &vbar}) {
        b->setColour(juce::ScrollBar::backgroundColourId, tc::timelineBg);
        b->setColour(juce::ScrollBar::trackColourId, tc::timelineBg);
    }
    app.addChangeListener(this);
    setWantsKeyboardFocus(true);
}

PianoRollComponent::~PianoRollComponent() { app.removeChangeListener(this); }

void PianoRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(tc::timelineBg);
    g.setColour(juce::Colour(0xff8f9aa6));
    g.setFont(theme::uiFont(11.0f));
    g.drawText("Vel", 0, getHeight() - kScrollbar - kVelocityHeight, kKeyboardWidth, 16, juce::Justification::centred);
}

void PianoRollComponent::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop(kTimelineHeight);
    top.removeFromLeft(kKeyboardWidth);
    timeline->setBounds(top.withTrimmedRight(kScrollbar));
    auto bottom = r.removeFromBottom(kScrollbar);
    hbar.setBounds(bottom.withTrimmedLeft(kKeyboardWidth).withTrimmedRight(kScrollbar));
    auto vel = r.removeFromBottom(kVelocityHeight);
    velocity->setBounds(vel.withTrimmedLeft(kKeyboardWidth).withTrimmedRight(kScrollbar));
    vbar.setBounds(r.removeFromRight(kScrollbar));
    keyboard->setBounds(r.removeFromLeft(kKeyboardWidth));
    grid->setBounds(r);
    if (firstLayout && grid->getHeight() > 0) { // centre around C4
        firstLayout = false;
        scrollY = juce::jmax(0.0, (127 - 72) * rowHeight - 20);
    }
    updateScrollbars();
}

model::Tick PianoRollComponent::snap(double tick) const
{
    tick = std::max(0.0, tick);
    return app.snapEnabled ? seq::snapNearest((Tick)std::llround(tick), gridTicks()) : (Tick)std::llround(tick);
}

void PianoRollComponent::zoomHorizontal(double f, double anchorX)
{
    const double t = xToTick(anchorX);
    pxPerTick = juce::jlimit(0.005, 2.0, pxPerTick * f);
    scrollX = std::max(0.0, t - anchorX / pxPerTick);
    updateScrollbars();
    repaint();
}

void PianoRollComponent::zoomVertical(double f, double anchorY)
{
    const double pitchPos = (anchorY + scrollY) / rowHeight;
    rowHeight = juce::jlimit(5.0, 40.0, rowHeight * f);
    scrollY = pitchPos * rowHeight - anchorY;
    updateScrollbars();
    repaint();
}

juce::Point<double> PianoRollComponent::gridCentre() const
{
    return {grid->getWidth() * 0.5, grid->getHeight() * 0.5};
}

void PianoRollComponent::scrollBy(double dx, double dy)
{
    scrollX = std::max(0.0, scrollX + dx);
    scrollY += dy;
    updateScrollbars();
    repaint();
}

void PianoRollComponent::updateScrollbars()
{
    const double totalH = 128 * rowHeight;
    const double visH = grid->getHeight();
    scrollY = juce::jlimit(0.0, std::max(0.0, totalH - visH), scrollY);
    vbar.setRangeLimits(0, totalH, juce::dontSendNotification);
    vbar.setCurrentRange(scrollY, visH, juce::dontSendNotification);

    Tick len = seq::ticksPerBar(app.project.timeSig) * 64;
    for (auto& t : app.project.tracks())
        if (auto* m = dynamic_cast<model::MidiTrack*>(t.get()); m && !m->notes().empty())
            len = std::max(len, m->notes().back().end() + seq::ticksPerBar(app.project.timeSig) * 16);
    const double visT = grid->getWidth() / pxPerTick;
    hbar.setRangeLimits(0, std::max((double)len, scrollX + visT), juce::dontSendNotification);
    hbar.setCurrentRange(scrollX, visT, juce::dontSendNotification);
}

void PianoRollComponent::scrollBarMoved(juce::ScrollBar* bar, double start)
{
    if (bar == &hbar) scrollX = start; else scrollY = start;
    repaint();
}

void PianoRollComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (selectionTrack != app.project.activeTrack) { selection.clear(); selectionTrack = app.project.activeTrack; }
    if (auto* t = app.activeMidiTrack())
        for (auto it = selection.begin(); it != selection.end();) it = t->findNote(*it) ? std::next(it) : selection.erase(it);
    updateScrollbars();
    repaint();
}

void PianoRollComponent::refreshPlayhead()
{
    const double pos = app.engine->position();
    // Follow during playback, and also when the playhead is moved while stopped
    // (Return to start, second Stop, loop start). A jump that lands inside the
    // visible area, like clicking in the timeline, does not scroll.
    const bool jumped = pos != lastPlayheadPos;
    lastPlayheadPos = pos;
    if (app.engine->isPlaying() || jumped) {
        const double visT = grid->getWidth() / pxPerTick;
        if (pos > scrollX + visT * 0.95 || pos < scrollX) { scrollX = std::max(0.0, pos - visT * 0.05); updateScrollbars(); grid->repaint(); timeline->repaint(); velocity->repaint(); }
    }
    const double x = tickToX(pos);
    if (x != lastPlayheadX || app.isRecording()) {
        lastPlayheadX = x;
        grid->repaint();
        timeline->repaint();
    }
    keyboard->repaint();
}

// ---------------------------------------------------------------- editing
void PianoRollComponent::copySelection()
{
    auto* t = app.activeMidiTrack();
    if (!t || selection.empty()) return;
    app.clipboard.clear();
    Tick first = std::numeric_limits<Tick>::max();
    for (auto id : selection) if (auto* n = t->findNote(id)) { app.clipboard.push_back(*n); first = std::min(first, n->start); }
    for (auto& n : app.clipboard) { n.start -= first; n.id = 0; }
}

void PianoRollComponent::paste()
{
    auto* t = app.activeMidiTrack();
    if (!t || app.clipboard.empty()) return;
    const Tick at = snap(app.engine->position());
    auto notes = app.clipboard;
    for (auto& n : notes) n.start += at;
    auto cmd = std::make_unique<model::AddNotesCommand>(t->id(), notes, "Paste");
    auto* raw = cmd.get();
    app.undo.perform(std::move(cmd));
    selection = std::set<model::NoteId>(raw->addedIds().begin(), raw->addedIds().end());
    selectionTrack = t->id();
    repaint();
}

void PianoRollComponent::deleteSelection()
{
    auto* t = app.activeMidiTrack();
    if (!t || selection.empty()) return;
    app.undo.perform(std::make_unique<model::RemoveNotesCommand>(t->id(), std::vector<model::NoteId>(selection.begin(), selection.end())));
    selection.clear();
}

void PianoRollComponent::selectAll()
{
    selection.clear();
    if (auto* t = app.activeMidiTrack()) for (auto& n : t->notes()) selection.insert(n.id);
    repaint();
}

bool PianoRollComponent::keyPressed(const juce::KeyPress& k)
{
    const bool cmd = k.getModifiers().isCommandDown();
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSelection(); return true; }
    if (cmd && k.getKeyCode() == 'A') { selectAll(); return true; }
    if (cmd && k.getKeyCode() == 'C') { copySelection(); return true; }
    if (cmd && k.getKeyCode() == 'X') { copySelection(); deleteSelection(); return true; }
    if (cmd && k.getKeyCode() == 'V') { paste(); return true; }
    // Zoom: + / - horizontal (time)
    if (!cmd && (k.getTextCharacter() == '+' || k.getKeyCode() == juce::KeyPress::numberPadAdd)) { zoomInH(); return true; }
    if (!cmd && (k.getTextCharacter() == '-' || k.getKeyCode() == juce::KeyPress::numberPadSubtract)) { zoomOutH(); return true; }
    if (!cmd && (k.getKeyCode() == juce::KeyPress::upKey || k.getKeyCode() == juce::KeyPress::downKey)) {
        auto* t = app.activeMidiTrack();
        if (!t || selection.empty()) return false;
        const int d = (k.getKeyCode() == juce::KeyPress::upKey ? 1 : -1) * (k.getModifiers().isShiftDown() ? 12 : 1);
        std::vector<Note> b, a;
        for (auto id : selection)
            if (auto* n = t->findNote(id)) {
                if (n->pitch + d < 0 || n->pitch + d > 127) return true;
                b.push_back(*n); auto m = *n; m.pitch += d; a.push_back(m);
            }
        app.undo.perform(std::make_unique<model::ModifyNotesCommand>(t->id(), b, a, "Transpose"));
        return true;
    }
    if (!cmd && (k.getKeyCode() == juce::KeyPress::leftKey || k.getKeyCode() == juce::KeyPress::rightKey)) {
        auto* t = app.activeMidiTrack();
        if (!t || selection.empty()) return false;
        const Tick d = (k.getKeyCode() == juce::KeyPress::rightKey ? 1 : -1) * gridTicks();
        std::vector<Note> b, a;
        for (auto id : selection)
            if (auto* n = t->findNote(id)) {
                if (n->start + d < 0) return true;
                b.push_back(*n); auto m = *n; m.start += d; a.push_back(m);
            }
        app.undo.perform(std::make_unique<model::ModifyNotesCommand>(t->id(), b, a, "Nudge"));
        return true;
    }
    return false;
}

} // namespace mc::gui
