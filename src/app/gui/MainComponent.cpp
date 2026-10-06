#include "gui/MainComponent.h"
#include "gui/PluginBrowser.h"
#include "gui/SettingsComponent.h"
#include "gui/Theme.h"

namespace mc::gui {

namespace {
enum MenuId {
    New = 1, Open, Save, SaveAs, ImportMidi, ImportMusicXml, ExportMidi, ExportMusicXml, Quit,
    Undo = 100, Redo, Cut, Copy, Paste, Delete, SelectAll, AddTrack, DeleteTrack, AddStepSeq,
    Settings = 200, Plugins, Rescan, SoundsFolder, Manual, About
};
const char* kProjectExt = "*.mcproj";
}

MainComponent::MainComponent(AppContext& a)
    : app(a), transport(a), chordPanel(a), trackList(a), pianoRoll(a), stepSeq(a, pianoRoll)
{
    for (juce::Component* c : std::initializer_list<juce::Component*>{&transport, &chordPanel, &page})
        addAndMakeVisible(c);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&trackList, &pianoRoll, &stepSeq,
                                                                      &zoomLabel, &zoomOutH, &zoomInH, &zoomOutV, &zoomInV,
                                                                      &gridLabel, &gridBox, &snapBtn})
        pageContent.addAndMakeVisible(c);
    // Note grid + snap for the piano roll (extendable with triplet/dotted variants; GridValue supports them).
    gridLabel.setFont(theme::uiFont(13.5f, true));
    gridLabel.setColour(juce::Label::textColourId, theme::col::textDim);
    gridLabel.setJustificationType(juce::Justification::centredRight);
    gridBox.addItem("1/4", 4); gridBox.addItem("1/8", 8); gridBox.addItem("1/16", 16); gridBox.addItem("1/32", 32);
    gridBox.addItem("1/8 T", 108); gridBox.addItem("1/16 T", 116);
    gridBox.setSelectedId(app.grid.triplet ? app.grid.denominator + 100 : app.grid.denominator, juce::dontSendNotification);
    gridBox.setTooltip("Note grid of the piano roll");
    gridBox.onChange = [this] {
        const int id = gridBox.getSelectedId();
        app.grid = id > 100 ? seq::GridValue{id - 100, true} : seq::GridValue{id};
        app.sendChangeMessage();
    };
    snapBtn.setToggleState(app.snapEnabled, juce::dontSendNotification);
    snapBtn.setTooltip("Snap notes to the grid");
    snapBtn.onClick = [this] { app.snapEnabled = snapBtn.getToggleState(); };
    page.setViewedComponent(&pageContent, false);
    page.setScrollBarsShown(true, false);
    page.setScrollBarThickness(12);
    pianoRoll.onViewChanged = [this] { stepSeq.viewChanged(); };
    stepSeq.onHeightChanged = [this] { resized(); };
    zoomLabel.setFont(theme::uiFont(13.5f, true));
    zoomLabel.setColour(juce::Label::textColourId, theme::col::textDim);
    zoomLabel.setJustificationType(juce::Justification::centredRight);
    zoomOutH.setTooltip("Zoom out in time (-, Ctrl+wheel)");
    zoomInH.setTooltip("Zoom in in time (+, Ctrl+wheel)");
    zoomOutV.setTooltip("Smaller rows and keys (Alt+wheel)");
    zoomInV.setTooltip("Taller rows and keys (Alt+wheel)");
    zoomOutH.onClick = [this] { pianoRoll.zoomOutH(); };
    zoomInH.onClick = [this] { pianoRoll.zoomInH(); };
    zoomOutV.onClick = [this] { pianoRoll.zoomOutV(); };
    zoomInV.onClick = [this] { pianoRoll.zoomInV(); };
#if !JUCE_MAC
    addAndMakeVisible(menuBar);
#else
    juce::MenuBarModel::setMacMainMenu(this);
#endif
    setWantsKeyboardFocus(true);
    app.addChangeListener(this);
    startTimerHz(30);
    setSize(1400, 860);

    if (!app.plugins->hasCache()) // first start: offer a scan
        juce::MessageManager::callAsync([this] {
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Instrument plugins",
                "Scan for installed instrument plugins now? (Can be done later via Options)", "Scan", "Later", this,
                juce::ModalCallbackFunction::create([this](int r) { if (r == 1) PluginBrowser::rescanWithProgress(app, {}); }));
        });
}

MainComponent::~MainComponent()
{
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
    app.removeChangeListener(this);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
#if !JUCE_MAC
    menuBar.setBounds(r.removeFromTop(24));
#endif
    transport.setBounds(r.removeFromTop(44));
    chordPanel.setBounds(r.removeFromTop(40));
    r.removeFromTop(1);
    page.setBounds(r);
    const int viewH = r.getHeight();
    const int stepsH = stepSeq.preferredHeight();
    // The piano roll height never depends on the step sequencers' content: a fixed strip below it shows
    // the start of the step sequencers, and new lines / step sequencers are added further down (scroll to them).
    const int peek = stepsH > 0 ? std::min(StepSequencerComponent::kPeekHeight, (int)(viewH * 0.3)) + 1 : 0;
    const int topH = std::max(260, viewH - peek);
    const int totalH = std::max(topH + (stepsH > 0 ? 1 + stepsH : 0), topH + peek);
    const int w = totalH > viewH ? r.getWidth() - page.getScrollBarThickness() : r.getWidth();
    pageContent.setSize(w, totalH);

    auto top = juce::Rectangle<int>(0, 0, w, topH);
    stepSeq.cardWidth = 290 + 1;
    stepSeq.setBounds(stepsH > 0 ? juce::Rectangle<int>(0, topH + 1, w, totalH - topH - 1) : juce::Rectangle<int>());
    trackList.setBounds(top.removeFromLeft(290));
    top.removeFromLeft(1);
    auto bar = top.removeFromTop(34);
    top.removeFromTop(1);
    {
        auto b = bar.reduced(theme::gap, 5);
        zoomLabel.setBounds(b.removeFromLeft(48));
        b.removeFromLeft(theme::gapS);
        for (auto* z : {&zoomOutH, &zoomInH, &zoomOutV, &zoomInV}) { z->setBounds(b.removeFromLeft(84)); b.removeFromLeft(theme::gapS); }
        b.removeFromLeft(theme::gapSection);
        gridLabel.setBounds(b.removeFromLeft(40));
        b.removeFromLeft(theme::gapS);
        gridBox.setBounds(b.removeFromLeft(84));
        b.removeFromLeft(theme::gap);
        snapBtn.setBounds(b.removeFromLeft(70));
    }
    pageContent.toolbar = bar;
    pianoRoll.setBounds(top);
}

void MainComponent::Page::paint(juce::Graphics& g)
{
    g.fillAll(theme::col::border); // 1px seams between sections
    g.setColour(theme::col::panel);
    g.fillRect(toolbar);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(theme::col::border); // 1px seams between sections
}

void MainComponent::timerCallback()
{
    transport.refreshPosition();
    chordPanel.refreshDisplay();
    pianoRoll.refreshPlayhead();
    stepSeq.refreshPlayhead();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    menuItemsChanged();
    if (onTitleChanged) {
        juce::String t = app.currentFile.existsAsFile() ? app.currentFile.getFileNameWithoutExtension() : juce::String("Untitled");
        if (app.hasUnsavedChanges()) t += " *";
        onTitleChanged(t + " - MIDI Composer");
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& k)
{
    const bool cmd = k.getModifiers().isCommandDown(), shift = k.getModifiers().isShiftDown();
    if (k == juce::KeyPress::spaceKey) { app.engine->isPlaying() ? app.stop() : app.play(); return true; }
    if (k == juce::KeyPress::homeKey) { app.returnToStart(); return true; }
    if (cmd && k.getKeyCode() == 'Z') { shift ? app.undo.redo() : app.undo.undo(); return true; }
    if (cmd && k.getKeyCode() == 'Y') { app.undo.redo(); return true; }
    if (cmd && k.getKeyCode() == 'S') { shift ? saveDialog() : save(); return true; }
    if (cmd && k.getKeyCode() == 'O') { openDialog(); return true; }
    if (cmd && k.getKeyCode() == 'N') { menuItemSelected(New, 0); return true; }
    if (!cmd && k.getKeyCode() == 'R') { app.toggleRecord(); return true; }
    if (!cmd && k.getKeyCode() == 'K') { app.setMetronome(!app.metronome()); return true; }
    if (!cmd && k.getKeyCode() == 'L') { app.setLoop(!app.project.loopEnabled, app.project.loopStart, app.project.loopEnd); return true; }
    if (!cmd && k.getKeyCode() == 'C') { auto& h = app.project.harmony; app.setHarmony(h.root, h.scaleId, !h.chordMode); return true; }
    return false;
}

juce::StringArray MainComponent::getMenuBarNames() { return {"File", "Edit", "Options"}; }

juce::PopupMenu MainComponent::getMenuForIndex(int idx, const juce::String&)
{
    juce::PopupMenu m;
    if (idx == 0) {
        m.addItem(New, "New project");
        m.addItem(Open, "Open project...");
        m.addItem(Save, "Save");
        m.addItem(SaveAs, "Save as...");
        m.addSeparator();
        m.addItem(ImportMidi, "Import MIDI file...");
        m.addItem(ImportMusicXml, "Import MusicXML (MuseScore)...");
        m.addItem(ExportMidi, "Export MIDI file...");
        m.addItem(ExportMusicXml, "Export MusicXML (MuseScore)...");
        m.addSeparator();
        m.addItem(Quit, "Quit");
    } else if (idx == 1) {
        m.addItem(Undo, "Undo " + juce::String(app.undo.undoName()), app.undo.canUndo());
        m.addItem(Redo, "Redo", app.undo.canRedo());
        m.addSeparator();
        m.addItem(Cut, "Cut notes");
        m.addItem(Copy, "Copy notes");
        m.addItem(Paste, "Paste notes at playhead", !app.clipboard.empty());
        m.addItem(Delete, "Delete notes");
        m.addItem(SelectAll, "Select all notes");
        m.addSeparator();
        m.addItem(AddTrack, "Add piano roll track");
        m.addItem(AddStepSeq, "Add step sequencer track");
        m.addItem(DeleteTrack, "Delete active track", app.project.track(app.project.activeTrack) != nullptr);
    } else {
        m.addItem(Settings, "Settings (audio, MIDI, modifier keys)...");
        m.addItem(Plugins, "Plugin browser...");
        m.addItem(Rescan, "Rescan plugins");
        m.addItem(SoundsFolder, "Open Sounds folder (add .sf2 files)...");
        m.addSeparator();
        m.addItem(Manual, "User manual");
        m.addItem(About, "About MIDI Composer...");
    }
    return m;
}

void MainComponent::menuItemSelected(int id, int)
{
    switch (id) {
        case New: confirmDiscard([this] { app.newProject(); }); break;
        case Open: openDialog(); break;
        case Save: save(); break;
        case SaveAs: saveDialog(); break;
        case ImportMidi: importDialog(); break;
        case ImportMusicXml: importMusicXmlDialog(); break;
        case ExportMidi: exportDialog(); break;
        case ExportMusicXml: exportMusicXmlDialog(); break;
        case Quit: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
        case Undo: app.undo.undo(); break;
        case Redo: app.undo.redo(); break;
        case Cut: pianoRoll.copySelection(); pianoRoll.deleteSelection(); break;
        case Copy: pianoRoll.copySelection(); break;
        case Paste: pianoRoll.paste(); break;
        case Delete: pianoRoll.deleteSelection(); break;
        case SelectAll: pianoRoll.selectAll(); break;
        case AddTrack: app.addTrack(); break;
        case AddStepSeq: app.addStepSequencer(); break;
        case DeleteTrack: app.deleteTrack(app.project.activeTrack); break;
        case Settings: SettingsComponent::show(app); break;
        case Plugins: {
            const auto tid = app.project.activeTrack;
            PluginBrowser::show(app, [this, tid](const plugins::PluginInfo& i) { app.setTrackPlugin(tid, &i); });
            break;
        }
        case Rescan: PluginBrowser::rescanWithProgress(app, {}); break;
        case SoundsFolder: app.openSoundsFolder(); break;
        case Manual: app.openManual(); break;
        case About: app.showAbout(); break;
        default: break;
    }
}

void MainComponent::showError(const juce::String& msg)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "MIDI Composer", msg);
}

void MainComponent::confirmDiscard(std::function<void()> proceed)
{
    if (!app.hasUnsavedChanges()) { proceed(); return; }
    juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon, "Unsaved changes",
        "Save changes to the current project?", "Save", "Discard", "Cancel", this,
        juce::ModalCallbackFunction::create([this, proceed](int r) {
            if (r == 1) save(proceed);
            else if (r == 2) proceed();
        }));
}

void MainComponent::requestQuit(std::function<void()> quit) { confirmDiscard(std::move(quit)); }

void MainComponent::openDialog()
{
    confirmDiscard([this] {
        chooser = std::make_unique<juce::FileChooser>("Open project", app.settings.lastDirectory(), kProjectExt);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            app.settings.setLastDirectory(f.getParentDirectory());
            juce::String err;
            if (!app.openProject(f, err)) showError("Could not open project:\n" + err);
            else if (err.isNotEmpty()) showError(err);
            for (auto& t : app.project.tracks())
                if (app.isPluginMissing(t->id())) { showError("Some instrument plugins are missing. Their tracks are marked \"Plugin missing\"; MIDI data is intact."); break; }
        });
    });
}

void MainComponent::save(std::function<void()> after)
{
    if (!app.currentFile.existsAsFile()) { saveDialog(std::move(after)); return; }
    juce::String err;
    if (!app.saveProject(app.currentFile, err)) { showError("Save failed:\n" + err); return; }
    if (after) after();
}

void MainComponent::saveDialog(std::function<void()> after)
{
    chooser = std::make_unique<juce::FileChooser>("Save project", app.settings.lastDirectory().getChildFile("Untitled.mcproj"), kProjectExt);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, after](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (!f.hasFileExtension("mcproj")) f = f.withFileExtension("mcproj");
        app.settings.setLastDirectory(f.getParentDirectory());
        juce::String err;
        if (!app.saveProject(f, err)) { showError("Save failed:\n" + err); return; }
        if (after) after();
    });
}

void MainComponent::importDialog()
{
    chooser = std::make_unique<juce::FileChooser>("Import MIDI file", app.settings.lastDirectory(), "*.mid;*.midi");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        app.settings.setLastDirectory(f.getParentDirectory());
        juce::String err;
        if (!app.importMidi(f, err)) showError("Import failed:\n" + err);
    });
}

void MainComponent::exportDialog()
{
    chooser = std::make_unique<juce::FileChooser>("Export MIDI file", app.settings.lastDirectory().getChildFile("export.mid"), "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (!f.hasFileExtension("mid")) f = f.withFileExtension("mid");
        juce::String err;
        if (!app.exportMidi(f, err)) showError("Export failed:\n" + err);
    });
}

void MainComponent::importMusicXmlDialog()
{
    chooser = std::make_unique<juce::FileChooser>("Import MusicXML", app.settings.lastDirectory(), "*.musicxml;*.mxl;*.xml");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        app.settings.setLastDirectory(f.getParentDirectory());
        juce::String err;
        if (!app.importMusicXml(f, err)) showError("Import failed:\n" + err);
    });
}

void MainComponent::exportMusicXmlDialog()
{
    const auto name = (app.currentFile.existsAsFile() ? app.currentFile.getFileNameWithoutExtension() : juce::String("export")) + ".musicxml";
    chooser = std::make_unique<juce::FileChooser>("Export MusicXML", app.settings.lastDirectory().getChildFile(name), "*.musicxml");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (!f.hasFileExtension("musicxml")) f = f.withFileExtension("musicxml");
        app.settings.setLastDirectory(f.getParentDirectory());
        juce::String err;
        if (!app.exportMusicXml(f, err)) showError("Export failed:\n" + err);
    });
}

} // namespace mc::gui
