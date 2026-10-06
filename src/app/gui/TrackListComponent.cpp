#include "gui/TrackListComponent.h"
#include "gui/PluginBrowser.h"
#include "plugins/BasicSynth.h"
#include "plugins/SoundFontPlayer.h"
#include "gui/Theme.h"
#include <algorithm>

namespace mc::gui {

namespace {
constexpr int kRowHeight = 80;

// Track colour picker shown in a call-out: colour field + sliders, preset
// palette and user swatches (saved in the user settings, not the project).
// The colour is previewed live; closing the picker records one undo step.
class TrackColourPicker final : public juce::Component, private juce::ChangeListener {
public:
    TrackColourPicker(AppContext& a, model::TrackId i) : app(a), id(i)
    {
        if (auto* t = app.project.track(id)) before = model::TrackProperties::from(*t);
        swatches = app.settings.colourSwatches();

        selector.setCurrentColour(before.colour ? juce::Colour(before.colour) : theme::trackPalette[0].second, juce::dontSendNotification);
        selector.addChangeListener(this);
        addAndMakeVisible(selector);

        saveBtn.setTooltip("Save the current colour as a swatch");
        saveBtn.onClick = [this] {
            const auto c = selector.getCurrentColour().withAlpha(1.0f).getARGB();
            swatches.erase(std::remove(swatches.begin(), swatches.end(), c), swatches.end());
            swatches.insert(swatches.begin(), c);
            if (swatches.size() > kMaxSwatches) swatches.resize(kMaxSwatches);
            app.settings.setColourSwatches(swatches);
            layout();
        };
        defaultBtn.setTooltip("Use the default theme colour");
        defaultBtn.onClick = [this] { apply(0); };
        addAndMakeVisible(saveBtn);
        addAndMakeVisible(defaultBtn);
        layout();
    }

    ~TrackColourPicker() override
    {
        selector.removeChangeListener(this);
        // Turn the live preview into a single undoable change.
        auto* t = app.project.track(id);
        if (!t) return;
        auto after = model::TrackProperties::from(*t);
        if (after.colour == before.colour) return;
        before.applyTo(*t);
        app.undo.perform(std::make_unique<model::SetTrackPropertiesCommand>(id, before, after));
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(theme::uiFont(12.0f));
        g.setColour(theme::col::textDim);
        g.drawText("Presets", presetLabel, juce::Justification::centredLeft);
        g.drawText(swatches.empty() ? "Saved (none yet - click \"Save swatch\")" : "Saved (right-click to remove)", savedLabel, juce::Justification::centredLeft);
        const auto current = app.project.track(id) ? app.project.track(id)->colour : 0u;
        auto drawSwatch = [&](juce::Rectangle<int> r, juce::Colour c) {
            const auto f = r.toFloat().reduced(1.5f);
            g.setColour(c);
            g.fillRoundedRectangle(f, 3.0f);
            const bool sel = c.getARGB() == current;
            g.setColour(sel ? juce::Colours::white : c.darker(0.4f));
            g.drawRoundedRectangle(f, 3.0f, sel ? 2.0f : 1.0f);
        };
        for (size_t i = 0; i < theme::trackPalette.size(); ++i) drawSwatch(presetCell((int)i), theme::trackPalette[i].second);
        for (size_t i = 0; i < swatches.size(); ++i) drawSwatch(savedCell((int)i), juce::Colour(swatches[i]));
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < (int)theme::trackPalette.size(); ++i)
            if (presetCell(i).contains(e.getPosition())) { pick(theme::trackPalette[(size_t)i].second); return; }
        for (int i = 0; i < (int)swatches.size(); ++i)
            if (savedCell(i).contains(e.getPosition())) {
                if (e.mods.isPopupMenu()) {
                    swatches.erase(swatches.begin() + i);
                    app.settings.setColourSwatches(swatches);
                    layout();
                } else {
                    pick(juce::Colour(swatches[(size_t)i]));
                }
                return;
            }
    }

    void resized() override {}

private:
    static constexpr int kCell = 22, kCols = 10, kPad = 8;
    static constexpr size_t kMaxSwatches = 30;

    void layout()
    {
        const int w = kCols * kCell + 2 * kPad;
        int y = kPad;
        selector.setBounds(kPad, y, w - 2 * kPad, 210);
        y += 214;
        presetLabel = {kPad, y, w - 2 * kPad, 16};
        y += 18;
        presetTop = y;
        y += kCell * (((int)theme::trackPalette.size() + kCols - 1) / kCols) + 6;
        savedLabel = {kPad, y, w - 2 * kPad, 16};
        y += 18;
        savedTop = y;
        y += kCell * std::max(1, ((int)swatches.size() + kCols - 1) / kCols) + 8;
        const int bw = (w - 2 * kPad - theme::gapS) / 2;
        saveBtn.setBounds(kPad, y, bw, 24);
        defaultBtn.setBounds(kPad + bw + theme::gapS, y, bw, 24);
        y += 24 + kPad;
        setSize(w, y);
        repaint();
    }
    juce::Rectangle<int> presetCell(int i) const { return {kPad + (i % kCols) * kCell, presetTop + (i / kCols) * kCell, kCell, kCell}; }
    juce::Rectangle<int> savedCell(int i) const { return {kPad + (i % kCols) * kCell, savedTop + (i / kCols) * kCell, kCell, kCell}; }

    void pick(juce::Colour c)
    {
        selector.setCurrentColour(c, juce::dontSendNotification);
        apply(c.withAlpha(1.0f).getARGB());
    }
    void apply(std::uint32_t argb)
    {
        if (auto* t = app.project.track(id); t && t->colour != argb) {
            t->colour = argb;
            app.project.notifyChanged();
        }
        repaint();
    }
    void changeListenerCallback(juce::ChangeBroadcaster*) override { apply(selector.getCurrentColour().withAlpha(1.0f).getARGB()); }

    AppContext& app;
    model::TrackId id;
    model::TrackProperties before;
    std::vector<std::uint32_t> swatches;
    juce::ColourSelector selector{juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders |
                                  juce::ColourSelector::showColourspace | juce::ColourSelector::editableColour};
    juce::TextButton saveBtn{"Save swatch"}, defaultBtn{"Default"};
    juce::Rectangle<int> presetLabel, savedLabel;
    int presetTop = 0, savedTop = 0;
};
} // namespace

void showInstrumentMenu(AppContext& app, model::TrackId id, juce::Component& target)
{
    auto* t = app.project.midiTrack(id);
    if (!t) return;
    juce::PopupMenu m;
    const auto& list = app.plugins->plugins();
    auto isCurrent = [&](const plugins::PluginInfo& p) { return p.format == t->plugin.format && p.identifier == t->plugin.identifier; };

    // Internal: built-in band instruments grouped Rock / Pop / Jazz, then Basic Synth.
    // SoundFont: the user's own .sf2 files, one sub-menu per file.
    // Everything else (VST3, AU, LV2, CLAP): one sub-menu per format.
    juce::PopupMenu internal, userSounds;
    std::map<std::string, juce::PopupMenu> groups, files, byFormat;
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& p = list[i];
        const int itemId = (int)i + 1000;
        if (p.format == plugins::InternalPluginHost::kFormat) {
            if (p.category == "Rock" || p.category == "Pop" || p.category == "Jazz")
                groups[p.category].addItem(itemId, juce::String(p.name), true, isCurrent(p));
            else
                internal.addItem(itemId, juce::String(p.name), true, isCurrent(p));
        } else if (p.format == plugins::soundfont::kUserFormat) {
            files[p.category].addItem(itemId, juce::String(p.name), true, isCurrent(p));
        } else {
            byFormat[p.format].addItem(itemId, juce::String(p.name) + "  (" + juce::String(p.manufacturer) + ")", true, isCurrent(p));
        }
    }
    juce::PopupMenu internalMenu;
    for (const char* g : {"Rock", "Pop", "Jazz"})
        if (groups.count(g)) internalMenu.addSubMenu(g, groups[g]);
    if (internal.getNumItems() > 0) { internalMenu.addSeparator(); for (juce::PopupMenu::MenuItemIterator it(internal); it.next();) internalMenu.addItem(it.getItem()); }
    m.addSubMenu("Internal", internalMenu);

    for (auto& [file, sub] : files) userSounds.addSubMenu(juce::String(file), sub);
    if (!files.empty()) userSounds.addSeparator();
    userSounds.addItem(5, "Open Sounds folder (add .sf2 files)...");
    m.addSubMenu("SoundFonts", userSounds);

    for (auto& [fmt, sub] : byFormat) m.addSubMenu(juce::String(fmt), sub);
    m.addSeparator();
    m.addItem(1, "Browse plugins...");
    m.addSeparator();
    auto* inst = app.engine->instrument(id);
    m.addItem(2, "Open editor", inst && inst->hasEditor());
    m.addItem(3, "Bypass", !t->plugin.empty(), t->plugin.bypassed);
    m.addItem(4, "Remove instrument", !t->plugin.empty());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target), [&ctx = app, tid = id](int r) {
        if (r == 0) return;
        if (r >= 1000) {
            auto& l = ctx.plugins->plugins();
            if ((size_t)(r - 1000) < l.size()) { auto info = l[(size_t)(r - 1000)]; ctx.setTrackPlugin(tid, &info); }
        } else if (r == 1) {
            PluginBrowser::show(ctx, [&ctx, tid](const plugins::PluginInfo& i) { ctx.setTrackPlugin(tid, &i); });
        } else if (r == 2) ctx.openPluginEditor(tid);
        else if (r == 3) { if (auto* tr = ctx.project.midiTrack(tid)) ctx.setTrackBypass(tid, !tr->plugin.bypassed); }
        else if (r == 4) ctx.setTrackPlugin(tid, nullptr);
        else if (r == 5) ctx.openSoundsFolder();
    });
}

class TrackListComponent::Row final : public juce::Component {
public:
    Row(AppContext& a, model::TrackId i) : app(a), id(i)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*>{&colourBtn, &name, &mute, &solo, &volume, &pan, &channel, &pluginBtn, &editBtn})
            addAndMakeVisible(c);
        name.setEditable(false, true);
        name.setFont(theme::uiFont(14.0f, true));
        name.setColour(juce::Label::textColourId, theme::col::text);
        name.onTextChange = [this] { change([&](model::TrackProperties& p) { p.name = name.getText().toStdString(); }); };
        name.addMouseListener(this, false);

        mute.setClickingTogglesState(true);
        solo.setClickingTogglesState(true);
        theme::setStyle(mute, theme::styleWarm);
        mute.setTooltip("Mute");
        solo.setTooltip("Solo");
        mute.onClick = [this] { change([&](auto& p) { p.mute = mute.getToggleState(); }); };
        solo.onClick = [this] { change([&](auto& p) { p.solo = solo.getToggleState(); }); };

        volume.setRange(0, 1, 0.01);
        volume.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        volume.setTooltip("Volume");
        pan.setRange(-1, 1, 0.01);
        pan.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pan.setTooltip("Pan");
        pan.setDoubleClickReturnValue(true, 0.0);
        for (auto* s : {&volume, &pan}) {
            s->onDragStart = [this] { if (auto* t = app.project.track(id)) dragBefore = model::TrackProperties::from(*t); };
            s->onValueChange = [this] { // live, without undo
                if (auto* t = app.project.track(id)) { t->volume = (float)volume.getValue(); t->pan = (float)pan.getValue(); app.modelChangedWithoutUndo(); }
            };
            s->onDragEnd = [this] {
                if (auto* t = app.project.track(id); t && dragBefore) {
                    auto after = model::TrackProperties::from(*t);
                    dragBefore->applyTo(*t);
                    app.undo.perform(std::make_unique<model::SetTrackPropertiesCommand>(id, *dragBefore, after));
                }
                dragBefore.reset();
            };
        }

        for (int c = 1; c <= 16; ++c) channel.addItem("Ch " + juce::String(c), c);
        channel.onChange = [this] { change([&](auto& p) { p.channel = channel.getSelectedId() - 1; }); };

        pluginBtn.onClick = [this] { showPluginMenu(); };
        colourBtn.setTooltip("Track colour");
        colourBtn.onClick = [this] { showColourMenu(); };
        editBtn.setTooltip("Open plugin editor");
        editBtn.onClick = [this] { app.openPluginEditor(id); };
        refresh();
    }

    void refresh()
    {
        auto* t = app.project.midiTrack(id);
        if (!t) return;
        name.setText(t->name, juce::dontSendNotification);
        mute.setToggleState(t->mute, juce::dontSendNotification);
        solo.setToggleState(t->solo, juce::dontSendNotification);
        volume.setValue(t->volume, juce::dontSendNotification);
        pan.setValue(t->pan, juce::dontSendNotification);
        channel.setSelectedId(t->channel + 1, juce::dontSendNotification);
        const bool missing = app.isPluginMissing(id);
        juce::String label = t->plugin.empty() ? "(no instrument)" : juce::String(t->plugin.name);
        if (missing) label = "Plugin missing: " + label;
        else if (t->plugin.bypassed) label += " [bypass]";
        pluginBtn.setButtonText(label);
        pluginBtn.setTooltip(missing ? app.pluginError(id) : juce::String(t->plugin.format + " - " + t->plugin.manufacturer));
        theme::setStyle(pluginBtn, missing ? theme::styleDanger : theme::styleSelector);
        auto* inst = app.engine->instrument(id);
        editBtn.setEnabled(inst && inst->hasEditor());
        active = app.project.activeTrack == id;
        colour = t->colour;
        colourBtn.colour = colour ? juce::Colour(colour) : theme::col::border;
        colourBtn.repaint();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (colour) {
            // Active track: strong colour. Inactive: muted (desaturated, light tint).
            const juce::Colour tc(colour);
            const auto muted = tc.withMultipliedSaturation(0.45f);
            g.fillAll(theme::col::panel.interpolatedWith(active ? tc : muted, active ? 0.50f : 0.10f));
            g.setColour(active ? tc : muted.withAlpha(0.7f));
            g.fillRect(0, 0, active ? 6 : 3, getHeight());
            if (active) {
                g.setColour(tc);
                g.drawRect(getLocalBounds().withTrimmedLeft(6).withTrimmedBottom(1), 2);
            }
        } else {
            g.fillAll(active ? theme::col::accentSoft : theme::col::panel);
            if (active) { g.setColour(theme::col::accent); g.fillRect(0, 0, 3, getHeight()); }
        }
        g.setColour(theme::col::borderSoft);
        g.drawHorizontalLine(getHeight() - 1, 0, (float)getWidth());
    }

    void resized() override
    {
        using namespace theme;
        auto r = getLocalBounds().reduced(gap, 5).withTrimmedLeft(4);
        auto l1 = r.removeFromTop(22);
        solo.setBounds(l1.removeFromRight(26));
        l1.removeFromRight(gapS);
        mute.setBounds(l1.removeFromRight(26));
        l1.removeFromRight(gap);
        channel.setBounds(l1.removeFromRight(72));
        l1.removeFromRight(gapS);
        colourBtn.setBounds(l1.removeFromLeft(18).withSizeKeepingCentre(16, 16));
        name.setBounds(l1);
        r.removeFromTop(gapS);
        auto l2 = r.removeFromTop(16);
        pan.setBounds(l2.removeFromRight(72));
        l2.removeFromRight(gap);
        volume.setBounds(l2);
        r.removeFromTop(gapS);
        auto l3 = r.removeFromTop(22);
        editBtn.setBounds(l3.removeFromRight(30));
        l3.removeFromRight(gapS);
        pluginBtn.setBounds(l3);
    }

    void mouseDown(const juce::MouseEvent&) override { app.selectTrack(id); }

private:
    template <typename F> void change(F&& f)
    {
        auto* t = app.project.track(id);
        if (!t) return;
        auto before = model::TrackProperties::from(*t);
        auto after = before;
        f(after);
        app.undo.perform(std::make_unique<model::SetTrackPropertiesCommand>(id, before, after));
    }

    void showColourMenu()
    {
        app.selectTrack(id);
        auto picker = std::make_unique<TrackColourPicker>(app, id);
        juce::CallOutBox::launchAsynchronously(std::move(picker), colourBtn.getScreenBounds(), nullptr);
    }

    void showPluginMenu()
    {
        app.selectTrack(id);
        showInstrumentMenu(app, id, pluginBtn);
    }

    struct Swatch final : juce::Button {
        Swatch() : juce::Button("colour") {}
        juce::Colour colour;
        void paintButton(juce::Graphics& g, bool over, bool) override
        {
            auto r = getLocalBounds().toFloat().reduced(1.5f);
            g.setColour(colour);
            g.fillEllipse(r);
            g.setColour(over ? theme::col::accent : colour.darker(0.25f));
            g.drawEllipse(r, 1.0f);
        }
    };

    AppContext& app;
    model::TrackId id;
    bool active = false;
    std::uint32_t colour = 0;
    Swatch colourBtn;
    std::optional<model::TrackProperties> dragBefore;
    juce::Label name;
    juce::TextButton mute{"M"}, solo{"S"}, pluginBtn, editBtn{"E"};
    juce::Slider volume{juce::Slider::LinearHorizontal, juce::Slider::NoTextBox};
    juce::Slider pan{juce::Slider::LinearHorizontal, juce::Slider::NoTextBox};
    juce::ComboBox channel;
public:
    model::TrackId trackId() const { return id; }
};

TrackListComponent::TrackListComponent(AppContext& a) : app(a)
{
    addAndMakeVisible(addBtn);
    addAndMakeVisible(delBtn);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addBtn.onClick = [this] { app.addTrack(); };
    delBtn.onClick = [this] {
        const auto id = app.project.activeTrack;
        auto* t = app.project.track(id);
        if (!t) return;
        juce::Component::SafePointer<TrackListComponent> self(this);
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Delete track",
            "Delete \"" + juce::String(t->name) + "\"? (can be undone)", "Delete", "Cancel", nullptr,
            juce::ModalCallbackFunction::create([self, id](int r) { if (self && r == 1) self->app.deleteTrack(id); }));
    };
    app.addChangeListener(this);
    rebuild();
}

TrackListComponent::~TrackListComponent() { app.removeChangeListener(this); }

void TrackListComponent::paint(juce::Graphics& g)
{
    g.fillAll(theme::col::panel);
    g.setColour(theme::col::borderSoft);
    g.drawHorizontalLine(33, 0.0f, (float)getWidth());
    g.setColour(theme::col::textDim);
    g.setFont(theme::uiFont(13.0f, true));
    g.drawText("TRACKS", theme::gap + 4, 0, 60, 34, juce::Justification::centredLeft);
}

void TrackListComponent::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop(34).reduced(theme::gap, 6);
    delBtn.setBounds(top.removeFromRight(72));
    top.removeFromRight(theme::gapS);
    addBtn.setBounds(top.removeFromRight(72));
    viewport.setBounds(r);
    content.setSize(viewport.getMaximumVisibleWidth(), (int)rows.size() * kRowHeight);
    for (size_t i = 0; i < rows.size(); ++i) rows[i]->setBounds(0, (int)i * kRowHeight, content.getWidth(), kRowHeight);
}

void TrackListComponent::changeListenerCallback(juce::ChangeBroadcaster*) { rebuild(); }

void TrackListComponent::rebuild()
{
    std::vector<model::TrackId> ids;
    for (auto& t : app.project.tracks()) if (t->kind() == model::TrackKind::Midi) ids.push_back(t->id());
    bool same = ids.size() == rows.size();
    for (size_t i = 0; same && i < ids.size(); ++i) same = rows[i]->trackId() == ids[i];
    if (!same) {
        rows.clear();
        for (auto id : ids) { rows.push_back(std::make_unique<Row>(app, id)); content.addAndMakeVisible(*rows.back()); }
        resized();
    } else {
        for (auto& r : rows) r->refresh();
    }
}

} // namespace mc::gui
