#include "gui/PluginBrowser.h"
#include "gui/Theme.h"

namespace mc::gui {

namespace {
// Runs the scan loop on a worker thread; the plugin loading itself is marshalled
// to the message thread by the host. The message thread must therefore never
// block waiting for this thread (ThreadWithProgressWindow does on cancel, which
// would deadlock), so cancellation only signals and completion is polled.
class ScanThread final : public juce::Thread, private juce::Timer {
public:
    ScanThread(AppContext& a, std::function<void()> d)
        : Thread("Plugin scan"), app(a), done(std::move(d)),
          window("Scanning plugins...", {}, juce::MessageBoxIconType::NoIcon)
    {
        window.addProgressBarComponent(progress);
        window.addButton("Cancel", 1);
        window.enterModalState(true, juce::ModalCallbackFunction::create([this](int) { signalThreadShouldExit(); }), false);
    }
    void launch()
    {
        startThread();
        startTimer(100);
    }
    void run() override
    {
        app.plugins->rescan([this](float f, const juce::String& item) {
            progress = f;
            { const juce::ScopedLock sl(lock); message = item; }
            return !threadShouldExit();
        });
    }
private:
    void timerCallback() override
    {
        { const juce::ScopedLock sl(lock); window.setMessage(message); }
        if (isThreadRunning()) return;
        stopTimer();
        if (window.isCurrentlyModal()) window.exitModalState(0);
        window.setVisible(false);
        if (done) done();
        delete this;
    }

    AppContext& app;
    std::function<void()> done;
    double progress = 0.0;
    juce::CriticalSection lock;
    juce::String message;
    juce::AlertWindow window;
};

class BrowserWindow final : public juce::DocumentWindow {
public:
    BrowserWindow(AppContext& app, PluginBrowser::Choose choose)
        : DocumentWindow("Instrument plugins", theme::col::appBg, closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new PluginBrowser(app, [this, choose](const plugins::PluginInfo& i) {
            choose(i);
            closeButtonPressed();
        }), false);
        setResizable(true, true);
        centreWithSize(720, 480);
        setVisible(true);
    }
    void closeButtonPressed() override
    {
        juce::Component::SafePointer<juce::Component> self(this);
        juce::MessageManager::callAsync([self] { delete self.getComponent(); });
    }
};

enum Col { Name = 1, Manufacturer, Format, Category };
} // namespace

void PluginBrowser::rescanWithProgress(AppContext& app, std::function<void()> done)
{
    (new ScanThread(app, std::move(done)))->launch();
}

void PluginBrowser::show(AppContext& app, Choose onChoose) { new BrowserWindow(app, std::move(onChoose)); }

PluginBrowser::PluginBrowser(AppContext& a, Choose c) : app(a), onChoose(std::move(c))
{
    formatFilter.addItem("All formats", 1);
    int id = 2;
    for (auto& f : app.plugins->allFormats()) formatFilter.addItem(f, id++);
    formatFilter.setSelectedId(1, juce::dontSendNotification);
    formatFilter.onChange = [this] { refilter(); };
    search.setTextToShowWhenEmpty("Search...", juce::Colours::grey);
    search.onTextChange = [this] { refilter(); };
    rescanBtn.onClick = [this] {
        juce::Component::SafePointer<PluginBrowser> self(this);
        rescanWithProgress(app, [self] { if (self) self->refilter(); });
    };
    chooseBtn.onClick = [this] {
        const int r = table.getSelectedRow();
        if (r >= 0 && r < (int)filtered.size() && onChoose) onChoose(filtered[(size_t)r]);
    };
    auto& h = table.getHeader();
    h.addColumn("Name", Name, 240);
    h.addColumn("Manufacturer", Manufacturer, 160);
    h.addColumn("Format", Format, 90);
    h.addColumn("Category", Category, 140);
    h.setSortColumnId(Name, true);
    for (juce::Component* comp : std::initializer_list<juce::Component*>{&formatFilter, &search, &rescanBtn, &chooseBtn, &table, &status}) addAndMakeVisible(comp);
    status.setColour(juce::Label::textColourId, theme::col::textDim);
    status.setFont(theme::uiFont(12.5f));
    table.setRowHeight(26);
    table.setColour(juce::ListBox::outlineColourId, theme::col::border);
    table.setOutlineThickness(1);
    refilter();
    setSize(720, 480);
}

void PluginBrowser::paint(juce::Graphics& g) { g.fillAll(theme::col::panel); }

void PluginBrowser::resized()
{
    auto r = getLocalBounds().reduced(theme::gap);
    auto top = r.removeFromTop(28);
    formatFilter.setBounds(top.removeFromLeft(150));
    top.removeFromLeft(6);
    rescanBtn.setBounds(top.removeFromRight(120));
    top.removeFromRight(6);
    search.setBounds(top);
    auto bottom = r.removeFromBottom(30);
    chooseBtn.setBounds(bottom.removeFromRight(120).reduced(0, 2));
    status.setBounds(bottom);
    r.removeFromTop(6);
    table.setBounds(r);
}

void PluginBrowser::refilter()
{
    filtered.clear();
    const auto fmt = formatFilter.getSelectedId() > 1 ? formatFilter.getText() : juce::String();
    const auto q = search.getText().trim();
    for (auto& p : app.plugins->plugins()) {
        if (fmt.isNotEmpty() && fmt != juce::String(p.format)) continue;
        if (q.isNotEmpty() && !juce::String(p.name).containsIgnoreCase(q) && !juce::String(p.manufacturer).containsIgnoreCase(q)) continue;
        filtered.push_back(p);
    }
    auto key = [this](const plugins::PluginInfo& p) -> const std::string& {
        switch (sortCol) { case Manufacturer: return p.manufacturer; case Format: return p.format; case Category: return p.category; default: return p.name; }
    };
    std::sort(filtered.begin(), filtered.end(), [&](auto& a, auto& b) {
        const int c = juce::String(key(a)).compareIgnoreCase(juce::String(key(b)));
        return sortForward ? c < 0 : c > 0;
    });
    status.setText(juce::String((int)filtered.size()) + " instruments" +
                   (app.plugins->hasCache() ? "" : "  (not scanned yet - click Rescan)"), juce::dontSendNotification);
    table.updateContent();
    table.repaint();
}

void PluginBrowser::paintRowBackground(juce::Graphics& g, int row, int, int, bool selected)
{
    g.fillAll(selected ? theme::col::accentSoft : (row % 2 ? theme::col::panel : theme::col::field));
}

void PluginBrowser::paintCell(juce::Graphics& g, int row, int col, int w, int h, bool)
{
    if (row >= (int)filtered.size()) return;
    auto& p = filtered[(size_t)row];
    const std::string* s = &p.name;
    if (col == Manufacturer) s = &p.manufacturer;
    else if (col == Format) s = &p.format;
    else if (col == Category) s = &p.category;
    g.setColour(col == Name ? theme::col::text : theme::col::textDim);
    g.setFont(theme::uiFont(13.5f, col == Name));
    g.drawText(juce::String(*s), 6, 0, w - 10, h, juce::Justification::centredLeft);
}

void PluginBrowser::cellDoubleClicked(int row, int, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int)filtered.size() && onChoose) onChoose(filtered[(size_t)row]);
}

void PluginBrowser::sortOrderChanged(int col, bool fwd)
{
    sortCol = col;
    sortForward = fwd;
    refilter();
}

} // namespace mc::gui
