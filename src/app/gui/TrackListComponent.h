#pragma once
#include "AppContext.h"

namespace mc::gui {

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
