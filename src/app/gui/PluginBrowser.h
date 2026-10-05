#pragma once
// Lists scanned instrument plugins with format filter and search; rescans on demand.
#include "AppContext.h"

namespace mc::gui {

class PluginBrowser final : public juce::Component, private juce::TableListBoxModel {
public:
    using Choose = std::function<void(const plugins::PluginInfo&)>;
    PluginBrowser(AppContext&, Choose onChoose);
    void resized() override;

    static void show(AppContext&, Choose onChoose);
    static void rescanWithProgress(AppContext&, std::function<void()> done);

private:
    int getNumRows() override { return (int)filtered.size(); }
    void paintRowBackground(juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell(juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    void cellDoubleClicked(int row, int col, const juce::MouseEvent&) override;
    void sortOrderChanged(int col, bool forwards) override;
    void refilter();

    AppContext& app;
    Choose onChoose;
    juce::ComboBox formatFilter;
    juce::TextEditor search;
    juce::TextButton rescanBtn{"Rescan plugins"}, chooseBtn{"Use selected"};
    juce::TableListBox table{"plugins", this};
    juce::Label status;
    std::vector<plugins::PluginInfo> filtered;
    int sortCol = 1;
    bool sortForward = true;
};

} // namespace mc::gui
