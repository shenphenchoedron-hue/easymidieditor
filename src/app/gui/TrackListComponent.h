#pragma once
#include "AppContext.h"

namespace mc::gui {

// Instrument popup (Internal / SoundFonts / plugin formats / editor / bypass) for one track.
void showInstrumentMenu(AppContext&, model::TrackId, juce::Component& target);
// Colour picker call-out; every track in the list gets the colour (one undo step).
void showTrackColourPicker(AppContext&, std::vector<model::TrackId>, juce::Component& target);

class TrackListComponent final : public juce::Component, private juce::ChangeListener {
public:
    explicit TrackListComponent(AppContext&);
    ~TrackListComponent() override;
    void resized() override;
    void paint(juce::Graphics&) override;

    class Row;
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void rebuild();

    AppContext& app;
    juce::TextButton addBtn{"+ Track"}, delBtn{"- Track"};
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Row>> rows;
};

} // namespace mc::gui
