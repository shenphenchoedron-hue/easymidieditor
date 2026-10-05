#pragma once
#include "AppContext.h"

namespace mc::gui {

class TransportBar final : public juce::Component, private juce::ChangeListener {
public:
    explicit TransportBar(AppContext&);
    ~TransportBar() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void refreshPosition();

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refreshFromModel();

    AppContext& app;
    juce::TextButton toStart{"|<"}, playBtn{"Play"}, stopBtn{"Stop"}, recBtn{"Rec"}, loopBtn{"Loop"};
    juce::Label position, bpmLabel{{}, "BPM"}, sigLabel{{}, "Sig"}, loopLabel{{}, "Loop bars"}, gridLabel{{}, "Grid"};
    juce::Slider bpm{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::ComboBox sigNum, sigDen, gridBox;
    juce::Slider loopStartBar{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::Slider loopEndBar{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::ToggleButton snapBtn{"Snap"};
    juce::TextButton undoBtn{"Undo"}, redoBtn{"Redo"};
    std::unique_ptr<juce::Drawable> logo;
    juce::Rectangle<float> logoArea;
    std::vector<int> separators;
};

} // namespace mc::gui
