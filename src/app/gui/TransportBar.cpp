#include "gui/TransportBar.h"
#include "sequencer/Timing.h"

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
    recBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffc03030));
    playBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff30a050));
    loopBtn.setClickingTogglesState(true);
    loopBtn.onClick = [this] { app.setLoop(loopBtn.getToggleState(), app.project.loopStart, app.project.loopEnd); };

    position.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    position.setJustificationType(juce::Justification::centred);
    position.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    position.setColour(juce::Label::textColourId, juce::Colour(0xff7fffa0));

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

void TransportBar::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff33373e)); }

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced(4);
    auto place = [&](juce::Component& c, int w) { c.setBounds(r.removeFromLeft(w)); r.removeFromLeft(4); };
    place(toStart, 34); place(playBtn, 52); place(stopBtn, 52); place(recBtn, 46); place(loopBtn, 50);
    place(position, 130);
    place(bpmLabel, 34); place(bpm, 100);
    place(sigLabel, 28); place(sigNum, 52); place(sigDen, 52);
    place(loopLabel, 64); place(loopStartBar, 90); place(loopEndBar, 90);
    place(gridLabel, 34); place(gridBox, 74); place(snapBtn, 60);
    place(undoBtn, 50); place(redoBtn, 50);
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
