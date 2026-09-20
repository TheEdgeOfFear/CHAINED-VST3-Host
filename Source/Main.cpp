#include <juce_gui_basics/juce_gui_basics.h>
#include "ChainedAudioEngine.h"
#include "MainComponent.h"

class ChainedApplication : public juce::JUCEApplication
{
public:
    ChainedApplication() = default;

    const juce::String getApplicationName() override { return "CHAINED"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        engine = std::make_unique<ChainedAudioEngine>();
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), *engine);
        
        // Initialize audio devices and scanner after window is ready
        engine->initializeAudio();
    }

    void shutdown() override
    {
        if (engine != nullptr)
        {
            engine->getDeviceManager().closeAudioDevice();
        }
        mainWindow.reset();
        engine.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
        if (mainWindow != nullptr)
        {
            mainWindow->toFront(true);
        }
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name, ChainedAudioEngine& engineRef)
            : DocumentWindow(name + " - VST3 Pedalboard Host by THE EDGE OF FEAR",
                             juce::Colour(0xff121417),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            auto* comp = new MainComponent(engineRef);
            setContentOwned(comp, true);
            addKeyListener(comp);

            setResizable(true, true);
            setResizeLimits(1180, 720, 2560, 1600);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        ~MainWindow() override
        {
            if (auto* comp = dynamic_cast<MainComponent*>(getContentComponent()))
                removeKeyListener(comp);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        std::unique_ptr<MainComponent> mainComponent;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<ChainedAudioEngine> engine;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(ChainedApplication)
