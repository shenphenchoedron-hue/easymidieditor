#pragma once
// Step sequencer view. A step sequencer is a group of MidiTracks (one per
// line, each with its own instrument). Clicking steps edits the line's
// pattern; the notes are generated from it, so playback and export just work.
#include "AppContext.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace mc::gui {

class StepSequencerComponent final : public juce::Component, private juce::ChangeListener, private juce::Timer {
public:
    explicit StepSequencerComponent(AppContext&);
    ~StepSequencerComponent() override;
    void resized() override;
    void paint(juce::Graphics&) override;

    std::uint64_t shownGroup() const { return group; }
    int currentStep() const { return playStep; }

    class Line;
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuild();
    void refreshHeader();
    // Applies f to the pattern of every line in the shown group (one undo step).
    void changeAll(const std::function<void(model::StepPattern&)>& f, const std::string& name);

    AppContext& app;
    std::uint64_t group = 0;
    int playStep = -1;

    juce::Label title{{}, "STEP SEQUENCER"}, stepsLabel{{}, "Steps"}, sizeLabel{{}, "Step"}, repLabel{{}, "Repeat"}, startLabel{{}, "Start bar"};
    juce::ComboBox groupBox, stepsBox, sizeBox;
    juce::Slider repeats{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::Slider startBar{juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft};
    juce::TextButton newBtn{"+ New step sequencer"}, addLineBtn{"+ Line"};
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Line>> lines;
};

} // namespace mc::gui
