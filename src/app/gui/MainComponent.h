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
    // toolbar above the piano roll: zoom
    juce::TextButton zoomOutH{"Zoom out"}, zoomInH{"Zoom in"}, zoomOutV{"Lower"}, zoomInV{"Taller"};
    juce::Label zoomLabel{{}, "Zoom"}, gridLabel{{}, "Grid"};
    juce::ComboBox gridBox;
    juce::ToggleButton snapBtn{"Snap"};
    juce::Rectangle<int> toolbarArea;
    // Track list + piano roll + step sequencers form one page that scrolls vertically:
    // step sequencers are placed after the piano roll and never shrink it.
    struct Page final : juce::Component {
        juce::Rectangle<int> toolbar;
        void paint(juce::Graphics&) override;
    };
    juce::Viewport page;
    Page pageContent;
    juce::TooltipWindow tooltips{this};
    std::unique_ptr<juce::FileChooser> chooser;
#if !JUCE_MAC
    juce::MenuBarComponent menuBar{this};
#endif
};

} // namespace mc::gui
