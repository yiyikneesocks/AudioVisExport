// =============================================================================
// Main.cpp — AudioVisGUI entry point
//
// Global LookAndFeel override pins UI font to Segoe UI (Windows standard) so
// that rendering is stable regardless of the system locale.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"
#include "WinDragCompat.h"
#include "../core/CrashReporter.h"

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

            // v0.5.0: UIPI 拖放兼容层（提权时切换 WM_DROPFILES 协议 + 子类兜底）。
            // 此刻窗口 peer 已创建、仍在消息线程，满足 OLE STA 约束。
            avx::winDragCompat::installDragCompat (getContentComponent());
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
            // 中文渲染：无需在此特殊处理。JUCE 文本布局在主字体缺字形时会自动走
            // Font::findSuitableFontForText → 系统回退（Windows 上 DirectWrite 命中微软雅黑等），
            // 且 FontOptions::fallbackEnabled 默认 true。之前的"中文乱码"根因是**解码**而非字体：
            // WM_DROPFILES 兜底用了 DragQueryFile A 版返回 GBK 字节再按 CharPointer_ASCII 解析
            // （见 WinDragCompat.cpp 的 v0.5.4 修复）——解码修好后回退链自然生效。
            // 注：setPreferredFallbackFamilies 是 Font 成员函数、非全局静态，故不在此逐字体设置。
        }

        void initialise (const juce::String&) override
        {
            // v0.5.4 #8：崩溃报告（Windows：未处理异常 → exe/crash/*.dmp+txt）
            CrashReporter::install();

            // v0.5.0: 提权进程自动降权重启（经 explorer.exe 代理）。
            // 管理员运行会触发 UIPI 拖放拦截；降权后 OLE 拖放完整可用
            // （含拖放悬停 HUD）。WM_DROPFILES 兼容层仅作重启失败时的兜底。
            if (avx::winDragCompat::isProcessElevated()
                && avx::winDragCompat::relaunchDeElevated())
            {
                quit();   // 新实例已拉起，本实例退出
                return;
            }

            mainWindow = std::make_unique<AVXGuiWindow> (getApplicationName());
        }

        void unhandledException (const std::exception* e,
                                 const juce::String& sourceFile, int lineNumber) override
        {
            CrashReporter::writeTextReport (e != nullptr ? juce::String (e->what()) : juce::String ("unknown"),
                                            sourceFile, lineNumber);
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
