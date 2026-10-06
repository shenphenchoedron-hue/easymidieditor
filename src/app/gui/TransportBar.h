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
    // Remembers whether the last click was a right-click (onClick fires for both).
    struct MenuButton final : juce::TextButton {
        using juce::TextButton::TextButton;
        bool rightClick = false;
        void clicked(const juce::ModifierKeys& m) override { rightClick = m.isPopupMenu(); juce::TextButton::clicked(m); }
    };
    void showMetronomeMenu();

    juce::TextButton toStart{"|<"}, playBtn{"Play"}, stopBtn{"Stop"}, recBtn{"Rec"}, loopBtn{"Loop"};
    MenuButton clickBtn{"Click"};
    juce::ComboBox countInBox;
    juce::Label position, bpmLabel{{}, "BPM"}, sigLabel{{}, "Sig"}, loopLabel{{}, "Loop bars"};
    juce::Slider bpm{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::ComboBox sigNum, sigDen;
    juce::Slider loopStartBar{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::Slider loopEndBar{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::TextButton undoBtn{"Undo"}, redoBtn{"Redo"};
    std::unique_ptr<juce::Drawable> logo;
    juce::Rectangle<float> logoArea;
    std::vector<int> separators;
};

} // namespace mc::gui
