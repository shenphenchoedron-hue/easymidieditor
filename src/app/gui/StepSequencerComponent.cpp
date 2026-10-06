#include "gui/StepSequencerComponent.h"
#include "gui/Theme.h"
#include "gui/TrackListComponent.h"
#include "sequencer/Timing.h"
#include "theory/Scale.h"

namespace mc::gui {

namespace {
constexpr int kRow = 26;                         // control row height in the card
constexpr int kHeaderHeight = 3 * kRow + 4 * 4;  // title + 2 settings rows
constexpr int kCollapsedHeight = kRow + 8;        // title row only
constexpr int kLineHeight = 30;
namespace tc = theme::col;
using model::Tick;

juce::Colour groupColour(const model::MidiTrack* t)
{
    return t && t->colour ? juce::Colour(t->colour) : juce::Colour(0xffe08f4f);
}

// Draws bar/beat lines of the piano roll grid into [x0, x1).
void drawTimeGrid(juce::Graphics& g, const PianoRollComponent& roll, const model::Project& p, int x0, int x1, int y, int h)
{
    const auto bar = seq::ticksPerBar(p.timeSig), beat = seq::ticksPerBeat(p.timeSig);
    const auto first = std::max<model::Tick>(0, (model::Tick)roll.xToTick(0) / beat * beat);
    for (auto t = first; roll.tickToX((double)t) < x1 - x0; t += beat) {
        const bool isBar = t % bar == 0;
        g.setColour(isBar ? tc::gridBar.withAlpha(0.75f) : tc::gridBeat);
        g.drawVerticalLine(x0 + (int)roll.tickToX((double)t), (float)y, (float)(y + h));
    }
}
} // namespace

// ============================================================ one line
class StepSequencerComponent::LineRow final : public juce::Component {
public:
    LineRow(StepSequencerComponent& o, model::TrackId i) : owner(o), app(o.app), roll(o.roll), id(i)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*>{&name, &instBtn, &noteBox, &mute, &delBtn})
            addAndMakeVisible(c);
        name.setEditable(false, true);
        name.setFont(theme::uiFont(13.5f, true));
        name.setColour(juce::Label::textColourId, tc::text);
        name.setTooltip("Line name (double-click to rename)");
        name.onTextChange = [this] { changeProps([&](model::TrackProperties& p) { p.name = name.getText().toStdString(); }); };

        theme::setStyle(instBtn, theme::styleSelector);
        instBtn.onClick = [this] { showInstrumentMenu(app, id, instBtn); };

        for (int p = 127; p >= 0; --p) noteBox.addItem(juce::String(theory::midiNoteName(p)), p + 1);
        noteBox.setTooltip("Note played by this line (drum kits: C2 kick, D2 snare, F#2 closed hi-hat, A#2 open hi-hat)");
        noteBox.onChange = [this] {
            auto* t = app.project.midiTrack(id);
            if (!t) return;
            auto pat = t->step;
            pat.pitch = noteBox.getSelectedId() - 1;
            app.setStepPatterns({{id, pat}}, "Change step note");
        };

        mute.setClickingTogglesState(true);
        theme::setStyle(mute, theme::styleWarm);
        mute.setTooltip("Mute line");
        mute.onClick = [this] { changeProps([&](model::TrackProperties& p) { p.mute = mute.getToggleState(); }); };

        delBtn.setTooltip("Delete line (can be undone)");
        delBtn.onClick = [this] { auto& ctx = app; const auto tid = id; juce::MessageManager::callAsync([&ctx, tid] { ctx.deleteTrack(tid); }); };
        refresh();
    }

    model::TrackId trackId() const { return id; }

    void refresh()
    {
        auto* t = app.project.midiTrack(id);
        if (!t) return;
        name.setText(t->name, juce::dontSendNotification);
        instBtn.setButtonText(t->plugin.empty() ? "(none)" : juce::String(t->plugin.name));
        instBtn.setTooltip(app.isPluginMissing(id) ? app.pluginError(id) : "Instrument of this line");
        theme::setStyle(instBtn, app.isPluginMissing(id) ? theme::styleDanger : theme::styleSelector);
        noteBox.setSelectedId(t->step.pitch + 1, juce::dontSendNotification);
        mute.setToggleState(t->mute, juce::dontSendNotification);
        colour = groupColour(t);
        repaint();
    }

    void resized() override
    {
        auto r = juce::Rectangle<int>(0, 0, owner.cardWidth - 1, getHeight()).reduced(theme::gap, 3).withTrimmedLeft(4);
        delBtn.setBounds(r.removeFromRight(26));
        r.removeFromRight(theme::gapS);
        mute.setBounds(r.removeFromRight(26));
        r.removeFromRight(theme::gapS);
        name.setBounds(r.removeFromLeft(86));
        r.removeFromLeft(theme::gapS);
        instBtn.setBounds(r);
        // note selector in the column under the piano keyboard
        noteBox.setBounds(juce::Rectangle<int>(owner.cardWidth, 0, PianoRollComponent::kKeyboardWidth, getHeight()).reduced(3, 3));
    }

    void paint(juce::Graphics& g) override
    {
        auto* t = app.project.midiTrack(id);
        if (!t) return;
        const int cardW = owner.cardWidth - 1, x0 = owner.cellsX(), x1 = getWidth();
        // card part
        g.setColour(tc::panel.interpolatedWith(colour, 0.12f));
        g.fillRect(0, 0, cardW, getHeight());
        g.setColour(colour);
        g.fillRect(0, 0, 4, getHeight());
        g.setColour(tc::border);
        g.fillRect(cardW, 0, 1, getHeight());
        // label column (aligned with the piano keyboard)
        g.setColour(tc::keyWhite);
        g.fillRect(owner.cardWidth, 0, PianoRollComponent::kKeyboardWidth, getHeight());
        g.setColour(tc::border);
        g.drawVerticalLine(x0 - 1, 0.0f, (float)getHeight());

        // cells, aligned with the piano roll grid
        g.saveState();
        g.reduceClipRegion(x0, 0, x1 - x0, getHeight());
        g.setColour(tc::rollBg);
        g.fillRect(x0, 0, x1 - x0, getHeight());
        const auto& p = t->step;
        const int total = p.totalSteps();
        const double stepW = (double)p.stepTicks * roll.pxPerTick;
        const auto beat = seq::ticksPerBeat(app.project.timeSig);
        const double fromT = roll.xToTick(0), toT = roll.xToTick(x1 - x0);
        const int k0 = std::max(0, (int)std::floor((fromT - (double)p.startTick) / (double)p.stepTicks));
        const int k1 = std::min(total, (int)std::ceil((toT - (double)p.startTick) / (double)p.stepTicks) + 1);
        const float gap = stepW > 8 ? 1.5f : 0.0f;
        for (int k = k0; k < k1; ++k) {
            const Tick rel = (Tick)k * p.stepTicks;
            const float x = (float)(x0 + roll.tickToX((double)p.stepStart(k)));
            const juce::Rectangle<float> r(x + gap, 3.0f, (float)stepW - 2 * gap, (float)getHeight() - 6.0f);
            const int v = k < (int)p.steps.size() ? p.steps[(size_t)k] : 0;
            const bool evenBeat = ((rel % p.barTicks) / std::max<Tick>(1, beat)) % 2 == 0;
            g.setColour((evenBeat ? tc::rollRowScale : tc::rollRowWhite).brighter(0.25f));
            g.fillRoundedRectangle(r, 3.0f);
            if (v > 0) {
                g.setColour(colour.withAlpha(0.45f + 0.55f * (float)v / 127.0f));
                g.fillRoundedRectangle(r, 3.0f);
            }
            if (rel % p.barTicks == 0) { // bar start
                g.setColour(colour.withAlpha(0.9f));
                g.fillRect(x, 0.0f, 2.0f, (float)getHeight());
            }
        }
        drawTimeGrid(g, roll, app.project, x0, x1, 0, 2);
        const float px = (float)(x0 + roll.tickToX(app.engine->position()));
        g.setColour(tc::playhead);
        g.drawLine(px, 0, px, (float)getHeight(), 1.5f);
        g.restoreState();
        g.setColour(tc::rollRowBlack);
        g.drawHorizontalLine(getHeight() - 1, (float)x0, (float)x1);
    }

    // ---- editing steps
    int stepIndexAt(float x, const model::StepPattern& p) const
    {
        const double tick = roll.xToTick(x - (float)owner.cellsX()) - (double)p.startTick;
        if (tick < 0) return -1;
        const int k = (int)(tick / (double)p.stepTicks);
        return k < p.totalSteps() ? k : -1;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto* t = app.project.midiTrack(id);
        if (!t || e.x < owner.cellsX()) return;
        const int i = stepIndexAt(e.position.x, t->step);
        if (i < 0) return;
        before = t->step;
        const int cur = t->step.steps[(size_t)i];
        // Click toggles; Shift-click = accent; right-click clears.
        if (e.mods.isPopupMenu()) paintValue = 0;
        else if (e.mods.isShiftDown()) paintValue = cur == 127 ? 0 : 127;
        else paintValue = cur > 0 ? 0 : 100;
        write(i);
        if (paintValue > 0) app.previewOnTrack(id, t->step.pitch, paintValue, 150);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (auto* t = app.project.midiTrack(id); t && before) write(stepIndexAt(e.position.x, t->step));
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        auto* t = app.project.midiTrack(id);
        if (!t || !before) return;
        auto after = t->step;
        t->step = *before; // restore, then apply through the undo stack
        t->regenerateStepNotes();
        before.reset();
        app.setStepPatterns({{id, after}}, "Edit steps");
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        // Ctrl+wheel / Shift+wheel act on the shared time axis, like in the piano roll.
        if (e.x >= owner.cellsX() && e.mods.isCommandDown()) roll.zoomHorizontal(w.deltaY > 0 ? 1.15 : 1 / 1.15, e.x - owner.cellsX());
        else if (e.x >= owner.cellsX() && e.mods.isShiftDown()) roll.scrollBy(-w.deltaY * 600 / roll.pxPerTick * 0.1, 0);
        else juce::Component::mouseWheelMove(e, w); // scroll the panel
    }

private:
    void write(int i)
    {
        auto* t = app.project.midiTrack(id);
        if (!t || !before || i < 0 || i >= (int)t->step.steps.size()) return;
        if (t->step.steps[(size_t)i] == paintValue) return;
        t->step.steps[(size_t)i] = (std::uint8_t)paintValue;
        t->regenerateStepNotes();
        app.modelChangedWithoutUndo();
    }
    template <typename F> void changeProps(F&& f)
    {
        auto* t = app.project.track(id);
        if (!t) return;
        auto b = model::TrackProperties::from(*t);
        auto a = b;
        f(a);
        app.undo.perform(std::make_unique<model::SetTrackPropertiesCommand>(id, b, a));
    }

    StepSequencerComponent& owner;
    AppContext& app;
    PianoRollComponent& roll;
    model::TrackId id;
    juce::Colour colour;
    std::optional<model::StepPattern> before;
    int paintValue = 0;
    juce::Label name;
    juce::TextButton instBtn, mute{"M"}, delBtn{"X"};
    juce::ComboBox noteBox;
};

// ============================================================ one step sequencer
class StepSequencerComponent::Block final : public juce::Component {
public:
    Block(StepSequencerComponent& o, std::uint64_t g) : owner(o), app(o.app), group(g)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*>{&collapseBtn, &colourBtn, &title, &addLineBtn, &delBtn, &barsLabel, &barsBox, &hint,
                                                                          &sizeLabel, &sizeBox, &startLabel, &startBar})
            addAndMakeVisible(c);
        title.setFont(theme::uiFont(14.0f, true));
        title.setColour(juce::Label::textColourId, tc::text);
        title.setEditable(false, true);
        title.setTooltip("Double-click to rename this step sequencer");
        title.onTextChange = [this] {
            const auto t = title.getText().trim().toStdString();
            changeAll([t](model::StepPattern& p) { p.title = t; }, "Rename step sequencer");
        };
        for (auto* l : {&barsLabel, &sizeLabel, &startLabel}) {
            l->setFont(theme::uiFont(13.0f, true));
            l->setColour(juce::Label::textColourId, tc::textDim);
            l->setJustificationType(juce::Justification::centredLeft);
        }
        hint.setFont(theme::uiFont(11.5f));
        hint.setColour(juce::Label::textColourId, tc::textDim);
        hint.setInterceptsMouseClicks(false, false);
        collapseBtn.setTooltip("Collapse / expand this step sequencer");
        collapseBtn.onClick = [this] {
            if (!owner.collapsedGroups.erase(group)) owner.collapsedGroups.insert(group);
            owner.layoutChanged();
        };
        colourBtn.setTooltip("Colour of this step sequencer");
        colourBtn.onClick = [this] {
            std::vector<model::TrackId> ids;
            for (auto* t : app.stepLines(group)) ids.push_back(t->id());
            showTrackColourPicker(app, ids, colourBtn);
        };
        addLineBtn.setTooltip("Add a line with its own instrument");
        addLineBtn.onClick = [this] { app.addStepLine(group); };
        delBtn.setTooltip("Delete this step sequencer (can be undone)");
        delBtn.onClick = [this] {
            auto& ctx = app; const auto g = group;
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Delete step sequencer",
                "Delete this step sequencer and all its lines? (can be undone)", "Delete", "Cancel", nullptr,
                juce::ModalCallbackFunction::create([&ctx, g](int r) { if (r == 1) ctx.deleteStepSequencer(g); }));
        };

        sizeBox.addItem("1/4", 4); sizeBox.addItem("1/8", 8); sizeBox.addItem("1/16", 16); sizeBox.addItem("1/32", 32);
        sizeBox.addItem("1/8 T", 12); sizeBox.addItem("1/16 T", 24);
        sizeBox.setTooltip("Length of each step");
        sizeBox.onChange = [this] {
            const int d = sizeBox.getSelectedId();
            if (d > 0) changeAll([d](model::StepPattern& p) { p.setStepTicks((model::Tick)(model::kPPQ * 4 / d)); }, "Change step size");
        };
        barsBox.setRange(1, 999, 1);
        barsBox.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 38, kRow);
        barsBox.setTooltip("Length in bars. Every bar has its own steps; new bars start empty. "
                           "Right-click a bar number above the steps to copy, duplicate, clear or delete a bar.");
        barsBox.onValueChange = [this] {
            const int n = (int)barsBox.getValue();
            changeAll([n](model::StepPattern& p) { p.setBars(n); }, "Change bars");
        };
        startBar.setRange(1, 999, 1);
        startBar.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 38, kRow);
        startBar.setTooltip("Bar of the song where the step sequencer starts");
        startBar.onValueChange = [this] {
            const auto t = (model::Tick)(startBar.getValue() - 1) * seq::ticksPerBar(app.project.timeSig);
            changeAll([t](model::StepPattern& p) { p.startTick = t; }, "Move step sequencer");
        };
        rebuild();
    }

    std::uint64_t groupId() const { return group; }
    bool collapsed() const { return owner.collapsedGroups.count(group) > 0; }
    int headerH() const { return collapsed() ? kCollapsedHeight : kHeaderHeight; }
    int preferredHeight() const { return collapsed() ? kCollapsedHeight : kHeaderHeight + (int)rows.size() * kLineHeight; }

    void rebuild()
    {
        const auto lines = app.stepLines(group);
        std::vector<model::TrackId> ids;
        for (auto* t : lines) ids.push_back(t->id());
        bool same = ids.size() == rows.size();
        for (size_t i = 0; same && i < ids.size(); ++i) same = rows[i]->trackId() == ids[i];
        if (!same) {
            rows.clear();
            for (auto id : ids) { rows.push_back(std::make_unique<LineRow>(owner, id)); addAndMakeVisible(*rows.back()); }
            resized();
        } else {
            for (auto& r : rows) r->refresh();
        }
        applyCollapsed();
        if (lines.empty()) return;
        const auto& p = lines.front()->step;
        colour = groupColour(lines.front());
        colourBtn.colour = colour;
        colourBtn.repaint();
        applyCollapsed();
        if (!title.isBeingEdited())
            title.setText(p.title.empty() ? "Step sequencer " + juce::String((juce::int64)group) : juce::String(p.title),
                          juce::dontSendNotification);
        sizeBox.setSelectedId((int)(model::kPPQ * 4 / std::max<model::Tick>(1, p.stepTicks)), juce::dontSendNotification);
        barsBox.setValue(p.bars, juce::dontSendNotification);
        startBar.setValue((double)(p.startTick / seq::ticksPerBar(app.project.timeSig) + 1), juce::dontSendNotification);
        repaint();
    }

    void repaintAll() { repaint(); for (auto& r : rows) r->repaint(); }

    void resized() override
    {
        using namespace theme;
        auto c = juce::Rectangle<int>(0, 0, owner.cardWidth - 1, kHeaderHeight).reduced(gap, 4).withTrimmedLeft(4);
        auto r1 = c.removeFromTop(kRow);
        collapseBtn.setBounds(r1.removeFromLeft(24));
        r1.removeFromLeft(gapS);
        colourBtn.setBounds(r1.removeFromLeft(20).withSizeKeepingCentre(18, 18));
        r1.removeFromLeft(gapS);
        delBtn.setBounds(r1.removeFromRight(24));
        r1.removeFromRight(gapS);
        addLineBtn.setBounds(r1.removeFromRight(70));
        title.setBounds(r1);
        c.removeFromTop(4);
        auto r2 = c.removeFromTop(kRow);
        const int half = (r2.getWidth() - gap) / 2;
        auto a = r2.removeFromLeft(half); r2.removeFromLeft(gap);
        sizeLabel.setBounds(a.removeFromLeft(40)); sizeBox.setBounds(a);
        barsLabel.setBounds(r2.removeFromLeft(40)); barsBox.setBounds(r2);
        c.removeFromTop(4);
        auto r3 = c.removeFromTop(kRow);
        auto b = r3.removeFromLeft(half); r3.removeFromLeft(gap);
        startLabel.setBounds(b.removeFromLeft(40)); startBar.setBounds(b);
        hint.setBounds(r3);
        for (size_t i = 0; i < rows.size(); ++i) rows[i]->setBounds(0, kHeaderHeight + (int)i * kLineHeight, getWidth(), kLineHeight);
    }

    void paint(juce::Graphics& g) override
    {
        const int cardW = owner.cardWidth - 1, x0 = owner.cellsX();
        g.setColour(tc::panel.interpolatedWith(colour, 0.22f));
        const int hh = headerH();
        g.fillRect(0, 0, cardW, hh);
        g.setColour(colour);
        g.fillRect(0, 0, 6, getHeight());
        g.setColour(tc::border);
        g.fillRect(cardW, 0, 1, getHeight());
        g.fillRect(0, 0, getWidth(), 1);
        // header strip on the time axis: bar numbers + where the step sequencer plays
        g.setColour(tc::timelineBg);
        g.fillRect(owner.cardWidth, 1, getWidth() - owner.cardWidth, hh - 1);
        auto lines = app.stepLines(group);
        if (lines.empty()) return;
        const auto& p = lines.front()->step;
        g.saveState();
        g.reduceClipRegion(x0, 0, getWidth() - x0, hh);
        auto& roll = owner.roll;
        const float sx = (float)(x0 + roll.tickToX((double)p.startTick));
        const float ex = (float)(x0 + roll.tickToX((double)(p.startTick + p.lengthTicks())));
        g.setColour(colour.withAlpha(0.35f));
        g.fillRoundedRectangle(sx, (float)hh - 22.0f, ex - sx, 16.0f, 4.0f);
        g.setColour(colour);
        g.drawRoundedRectangle(sx, (float)hh - 22.0f, ex - sx, 16.0f, 4.0f, 1.2f);
        g.setFont(theme::uiFont(11.0f, true));
        for (int b = 0; b < p.bars; ++b) {
            const float bx = (float)(x0 + roll.tickToX((double)(p.startTick + (model::Tick)b * p.barTicks)));
            const float bw = (float)(p.barTicks * roll.pxPerTick);
            if (b > 0) g.drawVerticalLine((int)bx, (float)hh - 22.0f, (float)hh - 6.0f);
            if (bw > 50) g.drawText("Bar " + juce::String(b + 1), (int)bx + 6, hh - 22, (int)bw - 8, 16, juce::Justification::centredLeft);
        }
        const auto bar = seq::ticksPerBar(app.project.timeSig);
        g.setFont(theme::uiFont(12.0f, true));
        const auto first = std::max<model::Tick>(0, (model::Tick)roll.xToTick(0) / bar * bar);
        for (auto t = first; roll.tickToX((double)t) < getWidth() - x0; t += bar) {
            const int x = x0 + (int)roll.tickToX((double)t);
            g.setColour(tc::gridBar);
            g.drawVerticalLine(x, 4.0f, (float)hh);
            if (!collapsed() && roll.pxPerTick * (double)bar > 28) {
                g.setColour(juce::Colour(0xffc9d1da));
                g.drawText(juce::String(t / bar + 1), x + 4, 6, 40, 14, juce::Justification::left);
            }
        }
        const float px = (float)(x0 + roll.tickToX(app.engine->position()));
        g.setColour(tc::playhead);
        g.drawLine(px, 0, px, (float)hh, 1.5f);
        g.restoreState();
    }

    // Right-click (or click) a bar in the header strip: bar operations for all lines.
    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto lines = app.stepLines(group);
        if (lines.empty() || e.x < owner.cellsX() || e.y >= headerH()) return;
        const auto& p = lines.front()->step;
        const double tick = owner.roll.xToTick(e.x - owner.cellsX()) - (double)p.startTick;
        if (tick < 0 || tick >= (double)p.lengthTicks()) return;
        const int b = (int)(tick / (double)p.barTicks);
        juce::PopupMenu m;
        m.addSectionHeader("Bar " + juce::String(b + 1));
        m.addItem(1, "Copy to next bar", b + 1 < p.bars);
        m.addItem(2, "Duplicate bar (insert copy after)");
        m.addItem(5, "Add empty bar after");
        m.addSeparator();
        m.addItem(3, "Clear bar");
        m.addItem(4, "Delete bar", p.bars > 1);
        juce::Component::SafePointer<Block> self(this);
        m.showMenuAsync(juce::PopupMenu::Options().withMousePosition(), [self, b](int r) {
            if (!self || r == 0) return;
            if (r == 1) self->changeAll([b](model::StepPattern& q) { q.copyBar(b, b + 1); }, "Copy bar");
            if (r == 2) self->changeAll([b](model::StepPattern& q) { q.duplicateBar(b); }, "Duplicate bar");
            if (r == 3) self->changeAll([b](model::StepPattern& q) { q.clearBar(b); }, "Clear bar");
            if (r == 4) self->changeAll([b](model::StepPattern& q) { q.deleteBar(b); }, "Delete bar");
            if (r == 5) self->changeAll([b](model::StepPattern& q) { q.duplicateBar(b); q.clearBar(b + 1); }, "Add bar");
        });
    }

private:
    void applyCollapsed()
    {
        const bool c = collapsed();
        collapseBtn.setButtonText(c ? juce::String::fromUTF8("\xe2\x96\xb8") : juce::String::fromUTF8("\xe2\x96\xbe")); // ▸ / ▾
        for (juce::Component* x : std::initializer_list<juce::Component*>{&barsLabel, &barsBox, &hint, &sizeLabel, &sizeBox, &startLabel, &startBar})
            x->setVisible(!c);
        for (auto& r : rows) r->setVisible(!c);
    }

    struct Swatch final : juce::Button {
        Swatch() : juce::Button("colour") {}
        juce::Colour colour;
        void paintButton(juce::Graphics& g, bool over, bool) override
        {
            auto r = getLocalBounds().toFloat().reduced(1.5f);
            g.setColour(colour);
            g.fillEllipse(r);
            g.setColour(over ? tc::accent : colour.darker(0.3f));
            g.drawEllipse(r, over ? 2.0f : 1.0f);
        }
    };

    void changeAll(const std::function<void(model::StepPattern&)>& f, const std::string& name)
    {
        std::vector<model::SetStepPatternsCommand::Entry> after;
        for (auto* t : app.stepLines(group)) { auto p = t->step; f(p); after.push_back({t->id(), p}); }
        if (!after.empty()) app.setStepPatterns(std::move(after), name);
    }

    StepSequencerComponent& owner;
    AppContext& app;
    std::uint64_t group;
    juce::Colour colour{0xffe08f4f};
    std::vector<std::unique_ptr<LineRow>> rows;
    juce::Label title, barsLabel{{}, "Bars"}, sizeLabel{{}, "Step"}, startLabel{{}, "Start"},
        hint{{}, "Right-click a bar for copy/clear"};
    juce::TextButton addLineBtn{"+ Line"}, delBtn{"X"}, collapseBtn;
    Swatch colourBtn;
    juce::ComboBox sizeBox;
    juce::Slider barsBox{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::Slider startBar{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
};

// ============================================================ panel
StepSequencerComponent::StepSequencerComponent(AppContext& a, PianoRollComponent& r) : app(a), roll(r)
{
    app.addChangeListener(this);
    rebuild();
}

StepSequencerComponent::~StepSequencerComponent() { app.removeChangeListener(this); }

int StepSequencerComponent::preferredHeight() const
{
    int h = 0;
    for (auto& b : blocks) h += b->preferredHeight();
    return h;
}

int StepSequencerComponent::firstBlockHeight() const { return blocks.empty() ? 0 : blocks.front()->preferredHeight(); }

void StepSequencerComponent::paint(juce::Graphics& g) { g.fillAll(tc::rollBg); }

void StepSequencerComponent::resized()
{
    // Leave the piano roll's scrollbar column free so the step rows end where the grid ends.
    const int w = std::max(0, getWidth() - PianoRollComponent::kScrollbarWidth);
    int y = 0;
    for (auto& b : blocks) { b->setBounds(0, y, w, b->preferredHeight()); y += b->preferredHeight(); }
}

void StepSequencerComponent::changeListenerCallback(juce::ChangeBroadcaster*) { rebuild(); }

void StepSequencerComponent::rebuild()
{
    const int oldH = preferredHeight();
    const auto groups = app.stepGroups();
    bool same = groups.size() == blocks.size();
    for (size_t i = 0; same && i < groups.size(); ++i) same = blocks[i]->groupId() == groups[i];
    if (!same) {
        blocks.clear();
        for (auto g : groups) { blocks.push_back(std::make_unique<Block>(*this, g)); addAndMakeVisible(*blocks.back()); }
    } else {
        for (auto& b : blocks) b->rebuild();
    }
    resized();
    if (preferredHeight() != oldH && onHeightChanged) onHeightChanged();
}

void StepSequencerComponent::layoutChanged()
{
    for (auto& b : blocks) b->rebuild();
    resized();
    if (onHeightChanged) onHeightChanged();
}

void StepSequencerComponent::viewChanged()
{
    for (auto& b : blocks) b->repaintAll();
}

void StepSequencerComponent::refreshPlayhead()
{
    const double x = roll.tickToX(app.engine->position());
    if (x == lastPlayX) return;
    lastPlayX = x;
    viewChanged();
}

} // namespace mc::gui
