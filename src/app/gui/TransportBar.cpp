#include "gui/TransportBar.h"
#include "sequencer/Timing.h"
#include "gui/Theme.h"

namespace mc::gui {

TransportBar::TransportBar(AppContext& a) : app(a)
{
    for (juce::Component* c : std::initializer_list<juce::Component*>{&toStart, &playBtn, &stopBtn, &recBtn, &loopBtn, &position, &bpmLabel, &bpm,
                               &sigLabel, &sigNum, &sigDen, &loopLabel, &loopStartBar, &loopEndBar, &gridLabel, &gridBox,
                               &snapBtn, &undoBtn, &redoBtn})
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
    for (auto* l : {&bpmLabel, &sigLabel, &loopLabel, &gridLabel}) {
        l->setFont(theme::uiFont(12.0f));
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

    // Grid values; extendable with triplet/dotted variants (GridValue supports them).
    gridBox.addItem("1/4", 4); gridBox.addItem("1/8", 8); gridBox.addItem("1/16", 16); gridBox.addItem("1/32", 32);
    gridBox.addItem("1/8 T", 108); gridBox.addItem("1/16 T", 116);
    gridBox.setSelectedId(16, juce::dontSendNotification);
    gridBox.onChange = [this] {
        const int id = gridBox.getSelectedId();
        app.grid = id > 100 ? seq::GridValue{id - 100, true} : seq::GridValue{id};
        app.sendChangeMessage();
    };
    snapBtn.setToggleState(true, juce::dontSendNotification);
    snapBtn.onClick = [this] { app.snapEnabled = snapBtn.getToggleState(); };

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
    place(position, 136);
    group();
    place(bpmLabel, 32); place(bpm, 96);
    group();
    place(sigLabel, 26); place(sigNum, 52); place(sigDen, 52);
    group();
    place(loopLabel, 62); place(loopStartBar, 88); place(loopEndBar, 88);
    group();
    place(gridLabel, 32); place(gridBox, 78, gap); place(snapBtn, 64);
    group();
    place(undoBtn, 54); place(redoBtn, 54);
}

void TransportBar::refreshPosition()
{
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
    const auto bar = seq::ticksPerBar(p.timeSig);
    loopStartBar.setValue((double)(p.loopStart / bar + 1), juce::dontSendNotification);
    loopEndBar.setValue((double)(p.loopEnd / bar + 1), juce::dontSendNotification);
    undoBtn.setEnabled(app.undo.canUndo());
    undoBtn.setTooltip("Undo " + juce::String(app.undo.undoName()));
    redoBtn.setEnabled(app.undo.canRedo());
    refreshPosition();
}

} // namespace mc::gui
