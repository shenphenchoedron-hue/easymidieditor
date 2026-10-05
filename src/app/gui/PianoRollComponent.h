#pragma once
// Piano roll: views and edits the active MidiTrack model through commands.
// Owns no musical data itself (only view state and the current selection).
#include "AppContext.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>

namespace mc::gui {

class PianoRollComponent final : public juce::Component, private juce::ScrollBar::Listener, private juce::ChangeListener {
public:
    explicit PianoRollComponent(AppContext&);
    ~PianoRollComponent() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void refreshPlayhead(); // called by a UI timer

    // ---- view state (shared by subcomponents)
    double pxPerTick = 0.1;
    double rowHeight = 14.0;
    double scrollX = 0;      // ticks at left edge
    double scrollY = 0;      // pixels from pitch 127 at top
    double tickToX(double t) const { return (t - scrollX) * pxPerTick; }
    double xToTick(double x) const { return x / pxPerTick + scrollX; }
    double pitchToY(int p) const { return (127 - p) * rowHeight - scrollY; }
    int yToPitch(double y) const { return juce::jlimit(0, 127, 127 - (int)std::floor((y + scrollY) / rowHeight)); }
    model::Tick snap(double tick) const;
    model::Tick gridTicks() const { return app.grid.ticks(); }
    void zoomHorizontal(double factor, double anchorX);
    void zoomVertical(double factor, double anchorY);
    void scrollBy(double dxTicks, double dyPixels);
    void updateScrollbars();

    AppContext& app;
    std::set<model::NoteId> selection;
    model::TrackId selectionTrack = 0;

    void copySelection();
    void paste();
    void deleteSelection();
    void selectAll();

private:
    void scrollBarMoved(juce::ScrollBar*, double) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    class Timeline; class Keyboard; class Grid; class VelocityLane;
    std::unique_ptr<Timeline> timeline;
    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<Grid> grid;
    std::unique_ptr<VelocityLane> velocity;
    juce::ScrollBar hbar{false}, vbar{true};
    double lastPlayheadX = -1;
    bool firstLayout = true;
    friend class Grid;
};

} // namespace mc::gui
