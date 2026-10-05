#include "AppContext.h"
#include "gui/MainComponent.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace {

class MainWindow final : public juce::DocumentWindow {
public:
    explicit MainWindow(mc::AppContext& app)
        : DocumentWindow("MIDI Composer", juce::Colour(0xff23262b), DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        auto* content = new mc::gui::MainComponent(app);
        main = content;
        content->onTitleChanged = [this](const juce::String& t) { setName(t); };
        setContentOwned(content, true);
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    mc::gui::MainComponent* main = nullptr;
};

class MidiComposerApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& cmdLine) override
    {
        context = std::make_unique<mc::AppContext>();
        window = std::make_unique<MainWindow>(*context);
        const auto arg = cmdLine.unquoted().trim();
        if (arg.isNotEmpty()) {
            juce::File f(arg);
            juce::String err;
            if (f.hasFileExtension("mid;midi")) context->importMidi(f, err);
            else if (f.existsAsFile()) context->openProject(f, err);
        }
    }

    void shutdown() override
    {
        window.reset();
        context.reset();
    }

    void systemRequestedQuit() override
    {
        if (window && window->main) window->main->requestQuit([] { juce::JUCEApplication::quit(); });
        else quit();
    }

private:
    std::unique_ptr<mc::AppContext> context;
    std::unique_ptr<MainWindow> window;
};

} // namespace

START_JUCE_APPLICATION(MidiComposerApplication)
