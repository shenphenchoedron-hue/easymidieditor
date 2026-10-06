#pragma once
#include "AppContext.h"
#include "gui/ChordPanel.h"
#include "gui/PianoRollComponent.h"
#include "gui/StepSequencerComponent.h"
#include "gui/TrackListComponent.h"
#include "gui/TransportBar.h"

namespace mc::gui {

class MainComponent final : public juce::Component, public juce::MenuBarModel, private juce::Timer, private juce::ChangeListener {
public:
    explicit MainComponent(AppContext&);
    ~MainComponent() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int, const juce::String&) override;
    void menuItemSelected(int id, int) override;

    void requestQuit(std::function<void()> quit);
    std::function<void(const juce::String&)> onTitleChanged;

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void openDialog();
    void saveDialog(std::function<void()> after = {});
    void save(std::function<void()> after = {});
    void importDialog();
    void exportDialog();
    void exportMusicXmlDialog();
    void importMusicXmlDialog();
    void confirmDiscard(std::function<void()> proceed);
    void showError(const juce::String&);

    AppContext& app;
    TransportBar transport;
    ChordPanel chordPanel;
    TrackListComponent trackList;
    PianoRollComponent pianoRoll;
    StepSequencerComponent stepSeq;
    // view toolbar above the editor: Piano roll / Step sequencer mode + zoom
    juce::TextButton stepModeBtn{"Step Sequencer"}, zoomOutH{"Zoom out"}, zoomInH{"Zoom in"}, zoomOutV{"Lower"}, zoomInV{"Taller"};
    juce::Label zoomLabel{{}, "Zoom"};
    void setStepMode(bool);
    juce::Rectangle<int> toolbarArea;
    juce::TooltipWindow tooltips{this};
    std::unique_ptr<juce::FileChooser> chooser;
#if !JUCE_MAC
    juce::MenuBarComponent menuBar{this};
#endif
};

} // namespace mc::gui
