#include "gui/TransportBar.h"
#include "sequencer/Timing.h"
#include "gui/Theme.h"

namespace mc::gui {

TransportBar::TransportBar(AppContext& a) : app(a)
{
    for (juce::Component* c : std::initializer_list<juce::Component*>{&toStart, &playBtn, &stopBtn, &recBtn, &loopBtn, &clickBtn, &countInBox,
                               &position, &bpmLabel, &bpm, &sigLabel, &sigNum, &sigDen, &loopLabel, &loopStartBar, &loopEndBar,
                               &undoBtn, &redoBtn})
        addAndMakeVisible(c);

    toStart.onClick = [this] { app.returnToStart(); };
    playBtn.onClick = [this] { app.play(); };
    stopBtn.onClick = [this] { app.stop(); };
    recBtn.onClick = [this] { app.toggleRecord(); };
    theme::setStyle(playBtn, theme::stylePlay);
    theme::setStyle(recBtn, theme::styleRecord);
    theme::setStyle(stopBtn, theme::styleTransport);
    theme::setStyle(toStart, theme::styleTransport);
    theme::setStyle(loopBtn, theme::styleTransport);
    // metronome + count-in
    theme::setStyle(clickBtn, theme::styleWarm);
    clickBtn.setTooltip("Metronome on/off (K). Right-click for the volume.");
    clickBtn.onClick = [this] {
        if (clickBtn.rightClick) showMetronomeMenu();
        else app.setMetronome(!app.metronome());
    };
    countInBox.addItem("No count-in", 1);
    for (int n : {1, 2, 4}) countInBox.addItem("Count-in " + juce::String(n) + (n == 1 ? " bar" : " bars"), n + 1);
    countInBox.setTooltip("Bars of metronome count-in before recording starts");
    countInBox.onChange = [this] { app.setCountInBars(countInBox.getSelectedId() - 1); };

    for (auto* l : {&bpmLabel, &sigLabel, &loopLabel}) {
        l->setFont(theme::uiFont(13.5f, true));
        l->setColour(juce::Label::textColourId, theme::col::textDim);
        l->setJustificationType(juce::Justification::centredRight);
        l->setBorderSize({0, 0, 0, 2});
    }
    logo = theme::createLogo();
    loopBtn.setClickingTogglesState(true);
    loopBtn.onClick = [this] { app.setLoop(loopBtn.getToggleState(), app.project.loopStart, app.project.loopEnd); };

    position.setFont(theme::monoFont(17.0f));
    position.setJustificationType(juce::Justification::centred);
    position.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    position.setColour(juce::Label::textColourId, theme::col::displayText);

    bpm.setRange(20, 400, 1);
    bpm.onValueChange = [this] { if (bpm.getValue() != app.project.tempoBpm) app.setTempo(bpm.getValue()); };

    for (int i = 1; i <= 16; ++i) sigNum.addItem(juce::String(i), i);
    for (int d : {2, 4, 8, 16}) sigDen.addItem(juce::String(d), d);
    auto sig = [this] { app.setTimeSignature(sigNum.getSelectedId(), sigDen.getSelectedId()); };
    sigNum.onChange = sig;
    sigDen.onChange = sig;

    for (auto* s : {&loopStartBar, &loopEndBar}) { s->setRange(1, 999, 1); s->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22); }
    auto loopChanged = [this] {
        const auto bar = seq::ticksPerBar(app.project.timeSig);
        const int s = (int)loopStartBar.getValue(), e = std::max(s + 1, (int)loopEndBar.getValue());
        app.setLoop(app.project.loopEnabled, (s - 1) * bar, (e - 1) * bar);
    };
    loopStartBar.onValueChange = loopChanged;
    loopEndBar.onValueChange = loopChanged;

    undoBtn.onClick = [this] { app.undo.undo(); };
    redoBtn.onClick = [this] { app.undo.redo(); };

    app.addChangeListener(this);
    refreshFromModel();
}

TransportBar::~TransportBar() { app.removeChangeListener(this); }

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(theme::col::panel);
    if (logo) logo->drawWithin(g, logoArea, juce::RectanglePlacement::centred, 1.0f);
    // transport display
    g.setColour(theme::col::display);
    g.fillRoundedRectangle(position.getBounds().toFloat(), theme::radius);
    g.setColour(theme::col::displayText.withAlpha(0.18f));
    g.drawRoundedRectangle(position.getBounds().toFloat().reduced(0.5f), theme::radius, 1.0f);
    // group separators
    g.setColour(theme::col::borderSoft);
    for (int x : separators) g.drawVerticalLine(x, 10.0f, (float)getHeight() - 10.0f);
}

void TransportBar::resized()
{
    using namespace theme;
    auto r = getLocalBounds().reduced(gap, 7);
    separators.clear();
    logoArea = r.removeFromLeft(30).toFloat();
    r.removeFromLeft(gapGroup);
    auto place = [&](juce::Component& c, int w, int after = gapS) { c.setBounds(r.removeFromLeft(w)); r.removeFromLeft(after); };
    auto group = [&] { r.removeFromLeft(gapS); separators.push_back(r.getX()); r.removeFromLeft(gapGroup - gapS + 2); };
    place(toStart, 36); place(playBtn, 58); place(stopBtn, 58); place(recBtn, 52); place(loopBtn, 54, gap);
    place(clickBtn, 58); place(countInBox, 132, gap);
    place(position, 136);
    group();
    place(bpmLabel, 32); place(bpm, 96);
    group();
    place(sigLabel, 26); place(sigNum, 52); place(sigDen, 52);
    group();
    place(loopLabel, 62); place(loopStartBar, 88); place(loopEndBar, 88);
    group();
    place(undoBtn, 54); place(redoBtn, 54);
}

void TransportBar::showMetronomeMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader("Metronome volume");
    const std::pair<const char*, float> levels[] = {{"Low", 0.25f}, {"Medium", 0.45f}, {"High", 0.7f}, {"Max", 1.0f}};
    for (int i = 0; i < 4; ++i)
        m.addItem(i + 1, levels[i].first, true, std::abs(app.metronomeLevel() - levels[i].second) < 0.05f);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&clickBtn), [this, levels](int r) {
        if (r > 0) app.setMetronomeLevel(levels[r - 1].second);
    });
}

void TransportBar::refreshPosition()
{
    if (app.isCountingIn()) {
        const auto beat = (double)seq::ticksPerBeat(app.project.timeSig);
        const int beatsLeft = (int)std::ceil(app.engine->countInRemaining() / beat);
        position.setText("COUNT-IN " + juce::String(beatsLeft), juce::dontSendNotification);
        playBtn.setToggleState(true, juce::dontSendNotification);
        recBtn.setToggleState(app.isRecording(), juce::dontSendNotification);
        return;
    }
    const auto bb = seq::toBarBeat((model::Tick)app.engine->position(), app.project.timeSig);
    position.setText(juce::String::formatted("%3d . %d . %03d", bb.bar, bb.beat, (int)(bb.tick * 1000 / seq::ticksPerBeat(app.project.timeSig))),
                     juce::dontSendNotification);
    playBtn.setToggleState(app.engine->isPlaying(), juce::dontSendNotification);
    recBtn.setToggleState(app.isRecording(), juce::dontSendNotification);
}

void TransportBar::changeListenerCallback(juce::ChangeBroadcaster*) { refreshFromModel(); }

void TransportBar::refreshFromModel()
{
    auto& p = app.project;
    bpm.setValue(p.tempoBpm, juce::dontSendNotification);
    sigNum.setSelectedId(p.timeSig.numerator, juce::dontSendNotification);
    sigDen.setSelectedId(p.timeSig.denominator, juce::dontSendNotification);
    loopBtn.setToggleState(p.loopEnabled, juce::dontSendNotification);
    clickBtn.setToggleState(app.metronome(), juce::dontSendNotification);
    countInBox.setSelectedId(app.countInBars() + 1, juce::dontSendNotification);
    if (countInBox.getSelectedId() == 0) countInBox.setText("Count-in " + juce::String(app.countInBars()), juce::dontSendNotification);
    const auto bar = seq::ticksPerBar(p.timeSig);
    loopStartBar.setValue((double)(p.loopStart / bar + 1), juce::dontSendNotification);
    loopEndBar.setValue((double)(p.loopEnd / bar + 1), juce::dontSendNotification);
    undoBtn.setEnabled(app.undo.canUndo());
    undoBtn.setTooltip("Undo " + juce::String(app.undo.undoName()));
    redoBtn.setEnabled(app.undo.canRedo());
    refreshPosition();
}

} // namespace mc::gui
