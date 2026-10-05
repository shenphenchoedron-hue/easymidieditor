#include "gui/TrackListComponent.h"
#include "gui/PluginBrowser.h"

namespace mc::gui {

namespace { constexpr int kRowHeight = 74; }

class TrackListComponent::Row final : public juce::Component {
public:
    Row(AppContext& a, model::TrackId i) : app(a), id(i)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*>{&name, &mute, &solo, &volume, &pan, &channel, &pluginBtn, &editBtn})
            addAndMakeVisible(c);
        name.setEditable(false, true);
        name.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        name.onTextChange = [this] { change([&](model::TrackProperties& p) { p.name = name.getText().toStdString(); }); };
        name.addMouseListener(this, false);

        mute.setClickingTogglesState(true);
        solo.setClickingTogglesState(true);
        mute.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange.darker());
        solo.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow.darker());
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
        if (missing) label = "Plugin mangler: " + label;
        else if (t->plugin.bypassed) label += " [bypass]";
        pluginBtn.setButtonText(label);
        pluginBtn.setTooltip(missing ? app.pluginError(id) : juce::String(t->plugin.format + " - " + t->plugin.manufacturer));
        pluginBtn.setColour(juce::TextButton::buttonColourId, missing ? juce::Colour(0xff902020) : juce::Colour(0xff3a3f47));
        auto* inst = app.engine->instrument(id);
        editBtn.setEnabled(inst && inst->hasEditor());
        active = app.project.activeTrack == id;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(active ? juce::Colour(0xff3b4554) : juce::Colour(0xff2b2e34));
        if (active) { g.setColour(juce::Colour(0xff4fa3e0)); g.fillRect(0, 0, 4, getHeight()); }
        g.setColour(juce::Colours::black);
        g.drawHorizontalLine(getHeight() - 1, 0, (float)getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(6, 4).withTrimmedLeft(4);
        auto l1 = r.removeFromTop(22);
        solo.setBounds(l1.removeFromRight(24));
        l1.removeFromRight(2);
        mute.setBounds(l1.removeFromRight(24));
        l1.removeFromRight(4);
        channel.setBounds(l1.removeFromRight(70));
        name.setBounds(l1);
        r.removeFromTop(2);
        auto l2 = r.removeFromTop(20);
        pan.setBounds(l2.removeFromRight(70));
        volume.setBounds(l2);
        r.removeFromTop(2);
        auto l3 = r.removeFromTop(22);
        editBtn.setBounds(l3.removeFromRight(30));
        l3.removeFromRight(2);
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

    void showPluginMenu()
    {
        app.selectTrack(id);
        auto* t = app.project.midiTrack(id);
        if (!t) return;
        juce::PopupMenu m;
        std::map<std::string, juce::PopupMenu> byFormat;
        const auto& list = app.plugins->plugins();
        for (size_t i = 0; i < list.size(); ++i)
            byFormat[list[i].format].addItem((int)i + 1000, juce::String(list[i].name) + "  (" + juce::String(list[i].manufacturer) + ")");
        for (auto& [fmt, sub] : byFormat) m.addSubMenu(juce::String(fmt), sub);
        m.addItem(1, "Browse plugins...");
        m.addSeparator();
        auto* inst = app.engine->instrument(id);
        m.addItem(2, "Open editor", inst && inst->hasEditor());
        m.addItem(3, "Bypass", !t->plugin.empty(), t->plugin.bypassed);
        m.addItem(4, "Remove instrument", !t->plugin.empty());
        juce::Component::SafePointer<Row> self(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pluginBtn), [self](int r) {
            if (!self || r == 0) return;
            auto& ctx = self->app;
            const auto tid = self->id;
            if (r >= 1000) {
                auto& l = ctx.plugins->plugins();
                if ((size_t)(r - 1000) < l.size()) { auto info = l[(size_t)(r - 1000)]; ctx.setTrackPlugin(tid, &info); }
            } else if (r == 1) {
                PluginBrowser::show(ctx, [&ctx, tid](const plugins::PluginInfo& i) { ctx.setTrackPlugin(tid, &i); });
            } else if (r == 2) ctx.openPluginEditor(tid);
            else if (r == 3) { if (auto* tr = ctx.project.midiTrack(tid)) ctx.setTrackBypass(tid, !tr->plugin.bypassed); }
            else if (r == 4) ctx.setTrackPlugin(tid, nullptr);
        });
    }

    AppContext& app;
    model::TrackId id;
    bool active = false;
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

void TrackListComponent::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff23262b)); }

void TrackListComponent::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop(30).reduced(4);
    addBtn.setBounds(top.removeFromLeft(top.getWidth() / 2).reduced(2, 0));
    delBtn.setBounds(top.reduced(2, 0));
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
