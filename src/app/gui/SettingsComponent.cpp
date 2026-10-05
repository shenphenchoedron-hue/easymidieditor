#include "gui/SettingsComponent.h"
#include "theory/Scale.h"

namespace mc::gui {

namespace {
class SettingsWindow final : public juce::DocumentWindow {
public:
    explicit SettingsWindow(AppContext& app) : DocumentWindow("Settings", juce::Colours::darkgrey, closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new SettingsComponent(app), true);
        setResizable(true, false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override
    {
        juce::Component::SafePointer<juce::Component> self(this);
        juce::MessageManager::callAsync([self] { delete self.getComponent(); });
    }
};
juce::Component::SafePointer<SettingsWindow> openWindow;
} // namespace

void SettingsComponent::show(AppContext& app)
{
    if (openWindow) { openWindow->toFront(true); return; }
    openWindow = new SettingsWindow(app);
}

SettingsComponent::SettingsComponent(AppContext& a)
    : app(a), audioSelector(a.deviceManager, 0, 0, 2, 2, false, false, true, false)
{
    addAndMakeVisible(audioSelector);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&midiLabel, &midiInput, &refreshBtn, &mapLabel, &resetBtn}) addAndMakeVisible(c);
    mapLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    refreshBtn.onClick = [this] { refreshMidiInputs(); };
    midiInput.onChange = [this] {
        const int i = midiInput.getSelectedItemIndex();
        const auto id = i > 0 ? midiIds[i - 1] : juce::String();
        app.midiDevices->selectInput(id);
        app.settings.setMidiInputIdentifier(id);
    };
    resetBtn.onClick = [this] { app.setControllerMapping({}); };

    for (int i = 0; i < (int)input::Modifier::Count; ++i) {
        auto& r = rows[(size_t)i];
        r.name.setText(input::modifierName((input::Modifier)i), juce::dontSendNotification);
        r.note.setRange(-1, 127, 1);
        r.note.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 44, 22);
        r.note.onValueChange = [this, i] {
            auto mapping = app.project.controllerMapping;
            mapping.notes[i] = (int)rows[(size_t)i].note.getValue();
            app.setControllerMapping(mapping);
        };
        r.learn.setClickingTogglesState(false);
        r.learn.onClick = [this, i] {
            app.beginMidiLearn((input::Modifier)i);
            refreshMapping();
        };
        for (juce::Component* c : std::initializer_list<juce::Component*>{&r.name, &r.note, &r.noteName, &r.learn}) addAndMakeVisible(c);
    }
    refreshMidiInputs();
    refreshMapping();
    app.addChangeListener(this);
    setSize(560, 640);
}

SettingsComponent::~SettingsComponent()
{
    app.removeChangeListener(this);
    if (auto xml = app.deviceManager.createStateXml()) app.settings.setAudioDeviceState(xml.get());
}

void SettingsComponent::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff2b2e34)); }

void SettingsComponent::resized()
{
    auto r = getLocalBounds().reduced(10);
    audioSelector.setBounds(r.removeFromTop(300));
    r.removeFromTop(10);
    auto m = r.removeFromTop(26);
    midiLabel.setBounds(m.removeFromLeft(90));
    refreshBtn.setBounds(m.removeFromRight(80));
    m.removeFromRight(6);
    midiInput.setBounds(m);
    r.removeFromTop(16);
    auto h = r.removeFromTop(24);
    resetBtn.setBounds(h.removeFromRight(80));
    mapLabel.setBounds(h);
    for (auto& row : rows) {
        r.removeFromTop(6);
        auto l = r.removeFromTop(26);
        row.name.setBounds(l.removeFromLeft(120));
        row.note.setBounds(l.removeFromLeft(120));
        l.removeFromLeft(6);
        row.noteName.setBounds(l.removeFromLeft(80));
        row.learn.setBounds(l.removeFromLeft(140));
    }
}

void SettingsComponent::refreshMidiInputs()
{
    midiInput.clear(juce::dontSendNotification);
    midiIds.clear();
    midiInput.addItem("(none)", 1);
    int sel = 1;
    for (auto& d : app.midiDevices->availableInputs()) {
        midiIds.add(d.identifier);
        midiInput.addItem(d.name, midiIds.size() + 1);
        if (d.identifier == app.midiDevices->selectedInput()) sel = midiIds.size() + 1;
    }
    midiInput.setSelectedId(sel, juce::dontSendNotification);
}

void SettingsComponent::refreshMapping()
{
    const auto& mapping = app.project.controllerMapping;
    for (int i = 0; i < (int)input::Modifier::Count; ++i) {
        auto& r = rows[(size_t)i];
        const int n = mapping.notes[i];
        r.note.setValue(n, juce::dontSendNotification);
        r.noteName.setText(n >= 0 ? juce::String(theory::midiNoteName(n)) : juce::String("(unset)"), juce::dontSendNotification);
        const auto learning = app.learningModifier();
        const bool isLearning = learning && (int)*learning == i;
        r.learn.setButtonText(isLearning ? "Press a key..." : "MIDI Learn");
        r.learn.setColour(juce::TextButton::buttonColourId, isLearning ? juce::Colour(0xffc07020) : juce::Colour(0xff3a3f47));
    }
}

void SettingsComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refreshMapping();
}

} // namespace mc::gui
