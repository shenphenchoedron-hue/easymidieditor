#include "gui/StepSequencerComponent.h"
#include "gui/Theme.h"
#include "gui/TrackListComponent.h"
#include "sequencer/Timing.h"
#include "theory/Scale.h"

namespace mc::gui {

namespace {
constexpr int kHeaderHeight = 40;
constexpr int kLineHeight = 40;
constexpr int kLabelsWidth = 400; // name + instrument + note + M + X
namespace tc = theme::col;
} // namespace

// ============================================================ one line
class StepSequencerComponent::Line final : public juce::Component {
public:
    Line(StepSequencerComponent& o, AppContext& a, model::TrackId i) : owner(o), app(a), id(i)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*>{&name, &instBtn, &noteBox, &mute, &delBtn, &cells})
            addAndMakeVisible(c);
        name.setEditable(false, true);
        name.setFont(theme::uiFont(14.0f, true));
        name.setColour(juce::Label::textColourId, tc::text);
        name.onTextChange = [this] { changeProps([&](model::TrackProperties& p) { p.name = name.getText().toStdString(); }); };
        name.addMouseListener(this, false);

        theme::setStyle(instBtn, theme::styleSelector);
        instBtn.onClick = [this] { app.selectTrack(id); showInstrumentMenu(app, id, instBtn); };

        for (int p = 127; p >= 0; --p) noteBox.addItem(juce::String(theory::midiNoteName(p)) + " (" + juce::String(p) + ")", p + 1);
        noteBox.setTooltip("Note played by this line (GM drums: C1 kick, D1 snare, F#1 closed hat)");
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

        theme::setStyle(delBtn, theme::styleDanger);
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
        instBtn.setButtonText(t->plugin.empty() ? "(no instrument)" : juce::String(t->plugin.name));
        theme::setStyle(instBtn, app.isPluginMissing(id) ? theme::styleDanger : theme::styleSelector);
        noteBox.setSelectedId(t->step.pitch + 1, juce::dontSendNotification);
        mute.setToggleState(t->mute, juce::dontSendNotification);
        active = app.project.activeTrack == id;
        colour = t->colour ? juce::Colour(t->colour) : tc::accent;
        cells.repaint();
        repaint();
    }

    void repaintCells() { cells.repaint(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(active ? tc::accentSoft : tc::panel);
        g.setColour(colour);
        g.fillRect(0, 0, active ? 5 : 3, getHeight());
        g.setColour(tc::borderSoft);
        g.drawHorizontalLine(getHeight() - 1, 0, (float)getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(theme::gap, 6).withTrimmedLeft(4);
        auto left = r.removeFromLeft(kLabelsWidth - theme::gap - 4);
        name.setBounds(left.removeFromLeft(120));
        left.removeFromLeft(theme::gapS);
        instBtn.setBounds(left.removeFromLeft(150));
        left.removeFromLeft(theme::gapS);
        delBtn.setBounds(left.removeFromRight(28));
        left.removeFromRight(theme::gapS);
        mute.setBounds(left.removeFromRight(28));
        left.removeFromRight(theme::gapS);
        noteBox.setBounds(left);
        r.removeFromLeft(theme::gap);
        cells.setBounds(r);
    }

    void mouseDown(const juce::MouseEvent&) override { app.selectTrack(id); }

private:
    template <typename F> void changeProps(F&& f)
    {
        auto* t = app.project.track(id);
        if (!t) return;
        auto before = model::TrackProperties::from(*t);
        auto after = before;
        f(after);
        app.undo.perform(std::make_unique<model::SetTrackPropertiesCommand>(id, before, after));
    }

    // The clickable step cells.
    struct Cells final : juce::Component {
        explicit Cells(Line& l) : line(l) {}
        Line& line;
        std::optional<model::StepPattern> before;
        int paintValue = 0; // velocity written while dragging (0 = clearing)

        int numSteps() const
        {
            auto* t = line.app.project.midiTrack(line.id);
            return t ? std::max(1, t->step.numSteps) : 16;
        }
        juce::Rectangle<float> cell(int i) const
        {
            const float w = (float)getWidth() / (float)numSteps();
            return {w * (float)i, 0.0f, w, (float)getHeight()};
        }
        int stepAt(float x) const { return juce::jlimit(0, numSteps() - 1, (int)(x / ((float)getWidth() / (float)numSteps()))); }

        void paint(juce::Graphics& g) override
        {
            auto* t = line.app.project.midiTrack(line.id);
            if (!t) return;
            const auto& pat = t->step;
            const int n = pat.numSteps;
            const int beatSteps = std::max<int>(1, (int)(model::kPPQ / std::max<model::Tick>(1, pat.stepTicks)));
            const int play = line.owner.currentStep();
            for (int i = 0; i < n; ++i) {
                auto r = cell(i).reduced(2.0f, 1.0f);
                const bool beatGroup = (i / beatSteps) % 2 == 0;
                const int v = i < (int)pat.steps.size() ? pat.steps[(size_t)i] : 0;
                juce::Colour bg = beatGroup ? juce::Colour(0xffe3e8ee) : juce::Colour(0xffd3dae2);
                g.setColour(bg);
                g.fillRoundedRectangle(r, 4.0f);
                if (v > 0) {
                    g.setColour(line.colour.withAlpha(0.45f + 0.55f * (float)v / 127.0f));
                    g.fillRoundedRectangle(r, 4.0f);
                }
                if (i == play) {
                    g.setColour(tc::playhead.withAlpha(v > 0 ? 0.9f : 0.5f));
                    g.drawRoundedRectangle(r, 4.0f, 2.5f);
                } else {
                    g.setColour(v > 0 ? line.colour.darker(0.35f) : tc::border);
                    g.drawRoundedRectangle(r, 4.0f, 1.0f);
                }
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            line.app.selectTrack(line.id);
            auto* t = line.app.project.midiTrack(line.id);
            if (!t) return;
            before = t->step;
            const int i = stepAt(e.position.x);
            const int cur = t->step.steps[(size_t)i];
            // Click toggles; Shift-click = accent; right-click clears.
            if (e.mods.isPopupMenu()) paintValue = 0;
            else if (e.mods.isShiftDown()) paintValue = cur == 127 ? 0 : 127;
            else paintValue = cur > 0 ? 0 : 100;
            write(i);
            if (paintValue > 0) line.app.previewNotes({t->step.pitch}, paintValue, 150);
        }
        void mouseDrag(const juce::MouseEvent& e) override { write(stepAt(e.position.x)); }
        void mouseUp(const juce::MouseEvent&) override
        {
            auto* t = line.app.project.midiTrack(line.id);
            if (!t || !before) return;
            auto after = t->step;
            t->step = *before; // restore, then apply through the undo stack
            t->regenerateStepNotes();
            before.reset();
            line.app.setStepPatterns({{line.id, after}}, "Edit steps");
        }
        void write(int i)
        {
            auto* t = line.app.project.midiTrack(line.id);
            if (!t || !before || i < 0 || i >= (int)t->step.steps.size()) return;
            if (t->step.steps[(size_t)i] == paintValue) return;
            t->step.steps[(size_t)i] = (std::uint8_t)paintValue;
            t->regenerateStepNotes();
            line.app.modelChangedWithoutUndo();
        }
    };

    StepSequencerComponent& owner;
    AppContext& app;
    model::TrackId id;
    bool active = false;
    juce::Colour colour = tc::accent;
    juce::Label name;
    juce::TextButton instBtn, mute{"M"}, delBtn{"X"};
    juce::ComboBox noteBox;
    Cells cells{*this};
};

// ============================================================ component
StepSequencerComponent::StepSequencerComponent(AppContext& a) : app(a)
{
    for (juce::Component* c : std::initializer_list<juce::Component*>{&title, &groupBox, &newBtn, &addLineBtn, &stepsLabel, &stepsBox,
                                                                      &sizeLabel, &sizeBox, &repLabel, &repeats, &startLabel, &startBar, &viewport})
        addAndMakeVisible(c);
    title.setFont(theme::uiFont(13.0f, true));
    title.setColour(juce::Label::textColourId, tc::textDim);
    for (auto* l : {&stepsLabel, &sizeLabel, &repLabel, &startLabel}) {
        l->setFont(theme::uiFont(13.5f, true));
        l->setColour(juce::Label::textColourId, tc::textDim);
        l->setJustificationType(juce::Justification::centredRight);
    }
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);

    newBtn.setTooltip("Insert a new step sequencer track");
    newBtn.onClick = [this] { app.addStepSequencer(); };
    addLineBtn.setTooltip("Add a line with its own instrument");
    addLineBtn.onClick = [this] { if (group) app.addStepLine(group); };

    groupBox.setTooltip("Choose step sequencer");
    groupBox.onChange = [this] {
        const auto g = (std::uint64_t)groupBox.getSelectedId();
        const auto l = app.stepLines(g);
        if (!l.empty()) app.selectTrack(l.front()->id());
    };

    for (int n : {4, 8, 12, 16, 24, 32, 48, 64}) stepsBox.addItem(juce::String(n), n);
    stepsBox.onChange = [this] {
        const int n = stepsBox.getSelectedId();
        if (n > 0) changeAll([n](model::StepPattern& p) { p.numSteps = n; }, "Change step count");
    };
    sizeBox.addItem("1/4", 4); sizeBox.addItem("1/8", 8); sizeBox.addItem("1/16", 16); sizeBox.addItem("1/32", 32);
    sizeBox.addItem("1/8 T", 12); sizeBox.addItem("1/16 T", 24);
    sizeBox.onChange = [this] {
        const int d = sizeBox.getSelectedId();
        if (d > 0) changeAll([d](model::StepPattern& p) { p.stepTicks = (model::Tick)(model::kPPQ * 4 / d); }, "Change step size");
    };
    repeats.setRange(1, 256, 1);
    repeats.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    repeats.setTooltip("How many times the pattern is played");
    repeats.onValueChange = [this] {
        const int n = (int)repeats.getValue();
        changeAll([n](model::StepPattern& p) { p.repeats = n; }, "Change repeats");
    };
    startBar.setRange(1, 999, 1);
    startBar.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    startBar.setTooltip("Bar where the step sequencer starts");
    startBar.onValueChange = [this] {
        const auto t = (model::Tick)(startBar.getValue() - 1) * seq::ticksPerBar(app.project.timeSig);
        changeAll([t](model::StepPattern& p) { p.startTick = t; }, "Move step sequencer");
    };

    app.addChangeListener(this);
    rebuild();
    startTimerHz(30);
}

StepSequencerComponent::~StepSequencerComponent() { app.removeChangeListener(this); }

void StepSequencerComponent::changeAll(const std::function<void(model::StepPattern&)>& f, const std::string& name)
{
    std::vector<model::SetStepPatternsCommand::Entry> after;
    for (auto* t : app.stepLines(group)) { auto p = t->step; f(p); after.push_back({t->id(), p}); }
    if (!after.empty()) app.setStepPatterns(std::move(after), name);
}

void StepSequencerComponent::paint(juce::Graphics& g)
{
    g.fillAll(tc::panel);
    g.setColour(tc::borderSoft);
    g.drawHorizontalLine(kHeaderHeight - 1, 0.0f, (float)getWidth());
    if (lines.empty()) {
        g.setColour(tc::textDim);
        g.setFont(theme::uiFont(16.0f, true));
        g.drawFittedText("No step sequencer selected.\nClick \"+ New step sequencer\" to insert one as a track,\n"
                         "or select a step sequencer line in the track list.",
                         getLocalBounds().withTrimmedTop(kHeaderHeight), juce::Justification::centred, 3);
    }
}

void StepSequencerComponent::resized()
{
    using namespace theme;
    auto r = getLocalBounds();
    auto h = r.removeFromTop(kHeaderHeight).reduced(gap, 7);
    auto place = [&](juce::Component& c, int w, int after = gapS) { c.setBounds(h.removeFromLeft(w)); h.removeFromLeft(after); };
    place(title, 128);
    place(groupBox, 150);
    place(newBtn, 180);
    place(addLineBtn, 76, gapGroup);
    place(stepsLabel, 46); place(stepsBox, 64, gapGroup);
    place(sizeLabel, 38); place(sizeBox, 80, gapGroup);
    place(repLabel, 56); place(repeats, 92, gapGroup);
    place(startLabel, 70); place(startBar, 92);
    viewport.setBounds(r);
    content.setSize(viewport.getMaximumVisibleWidth(), (int)lines.size() * kLineHeight);
    for (size_t i = 0; i < lines.size(); ++i) lines[i]->setBounds(0, (int)i * kLineHeight, content.getWidth(), kLineHeight);
}

void StepSequencerComponent::changeListenerCallback(juce::ChangeBroadcaster*) { rebuild(); }

void StepSequencerComponent::rebuild()
{
    // Which group to show: the active track's, else keep the current one, else the first.
    std::vector<std::uint64_t> groups;
    for (auto& t : app.project.tracks())
        if (auto* m = dynamic_cast<model::MidiTrack*>(t.get()); m && m->isStepLine())
            if (std::find(groups.begin(), groups.end(), m->stepGroup) == groups.end()) groups.push_back(m->stepGroup);
    if (const auto g = app.activeStepGroup()) group = g;
    else if (std::find(groups.begin(), groups.end(), group) == groups.end()) group = groups.empty() ? 0 : groups.front();

    std::vector<model::TrackId> ids;
    for (auto* t : app.stepLines(group)) ids.push_back(t->id());
    bool same = ids.size() == lines.size();
    for (size_t i = 0; same && i < ids.size(); ++i) same = lines[i]->trackId() == ids[i];
    if (!same) {
        lines.clear();
        for (auto id : ids) { lines.push_back(std::make_unique<Line>(*this, app, id)); content.addAndMakeVisible(*lines.back()); }
        resized();
        repaint();
    } else {
        for (auto& l : lines) l->refresh();
    }

    groupBox.clear(juce::dontSendNotification);
    for (auto g : groups) groupBox.addItem("Step sequencer " + juce::String((juce::int64)g), (int)g);
    groupBox.setSelectedId((int)group, juce::dontSendNotification);
    refreshHeader();
}

void StepSequencerComponent::refreshHeader()
{
    const auto l = app.stepLines(group);
    const bool has = !l.empty();
    for (juce::Component* c : std::initializer_list<juce::Component*>{&addLineBtn, &stepsBox, &sizeBox, &repeats, &startBar, &groupBox})
        c->setEnabled(has);
    if (!has) return;
    const auto& p = l.front()->step;
    stepsBox.setSelectedId(p.numSteps, juce::dontSendNotification);
    if (stepsBox.getSelectedId() != p.numSteps) stepsBox.setText(juce::String(p.numSteps), juce::dontSendNotification);
    const int d = (int)(model::kPPQ * 4 / std::max<model::Tick>(1, p.stepTicks));
    sizeBox.setSelectedId(d, juce::dontSendNotification);
    repeats.setValue(p.repeats, juce::dontSendNotification);
    startBar.setValue((double)(p.startTick / seq::ticksPerBar(app.project.timeSig) + 1), juce::dontSendNotification);
}

void StepSequencerComponent::timerCallback()
{
    int s = -1;
    const auto l = app.stepLines(group);
    if (!l.empty() && app.engine->isPlaying()) {
        const auto& p = l.front()->step;
        const double pos = app.engine->position() - (double)p.startTick;
        const double total = (double)p.stepTicks * p.numSteps * p.repeats;
        if (pos >= 0 && pos < total) s = (int)(pos / (double)p.stepTicks) % p.numSteps;
    }
    if (s != playStep) {
        playStep = s;
        for (auto& line : lines) line->repaintCells();
    }
}

} // namespace mc::gui
