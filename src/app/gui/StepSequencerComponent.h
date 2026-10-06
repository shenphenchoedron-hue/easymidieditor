#pragma once
// Step sequencer panel, shown below the piano roll. Every step sequencer track
// is a block: its setup card sits in the left column (like the track cards),
// and its step rows sit to the right, aligned with the piano roll grid
// (same scroll and zoom), so rhythms line up with the notes above.
// A step sequencer is a group of MidiTracks (one per line, each with its own
// instrument). The notes are generated from the pattern.
#include "AppContext.h"
#include "gui/PianoRollComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>

namespace mc::gui {

class StepSequencerComponent final : public juce::Component, private juce::ChangeListener {
public:
    StepSequencerComponent(AppContext&, PianoRollComponent&);
    ~StepSequencerComponent() override;
    void resized() override;
    void paint(juce::Graphics&) override;

    int preferredHeight() const;          // 0 when there is no step sequencer
    int firstBlockHeight() const;         // height of the first step sequencer (kept visible below the piano roll)
    void refreshPlayhead();               // UI timer
    void viewChanged();                   // piano roll scrolled/zoomed
    std::function<void()> onHeightChanged;
    void layoutChanged();                 // a block was collapsed/expanded
    std::set<std::uint64_t> collapsedGroups;

    int cardWidth = 291;                  // left column width (track list + seam)
    int cellsX() const { return cardWidth + PianoRollComponent::kKeyboardWidth; }

    class Block;
    class LineRow;
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void rebuild();

    AppContext& app;
    PianoRollComponent& roll;
    std::vector<std::unique_ptr<Block>> blocks;
    double lastPlayX = -1;
};

} // namespace mc::gui
