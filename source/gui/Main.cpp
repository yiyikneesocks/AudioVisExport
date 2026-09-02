// =============================================================================
// Main.cpp — AudioVisGUI entry point
//
// Global LookAndFeel override pins UI font to Segoe UI (Windows standard) so
// that rendering is stable regardless of the system locale.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

namespace
{
    class AVXGuiWindow : public juce::DocumentWindow
    {
    public:
        AVXGuiWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff202028),
                              DocumentWindow::allButtons, true)
        {
            setUsingNativeTitleBar (true);
            setResizeLimits (960, 640, 8192, 8192);
            setContentOwned (new MainComponent(), true);
            centreWithSize (1280, 800);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            if (auto* app = juce::JUCEApplication::getInstance())
                app->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE (AVXGuiWindow)
    };

    class AVXGuiApplication : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override    { return "AudioVisGUI"; }
        const juce::String getApplicationVersion() override { return "0.3.1"; }
        bool moreThanOneInstanceAllowed() override          { return true; }

        AVXGuiApplication()
        {
            auto& lf = juce::LookAndFeel::getDefaultLookAndFeel();
            lf.setDefaultSansSerifTypefaceName ("Segoe UI");
            // JUCE 8 API: setDefaultSansSerifFont also accepts options
        }

        void initialise (const juce::String&) override
        {
            mainWindow = std::make_unique<AVXGuiWindow> (getApplicationName());
        }

        void shutdown() override
        {
            mainWindow = nullptr;
        }

    private:
        std::unique_ptr<AVXGuiWindow> mainWindow;
    };
}

START_JUCE_APPLICATION (AVXGuiApplication)
