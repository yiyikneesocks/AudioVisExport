// =============================================================================
// Main.cpp — AudioVisGUI 应用入口
//
// 窗口：1280x800，原生标题栏，内容 = MainComponent
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
        const juce::String getApplicationVersion() override { return "0.3.0"; }
        bool moreThanOneInstanceAllowed() override          { return true; }

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
