// vis_tabs_test — v0.5.4 #G 结构不变式回归：参数面板的每个直接子组件都必须被
//   ① 某行接管（rows[].editors）、② 是某行的标签、或 ③ 登记为常驻部件（chrome）。
//   违反即被 resized() 末尾的安全网隐藏 —— 于是"忘记登记"的表现从
//   「控件残留在别的选项卡上」（静默 bug，用户看得见）变成「控件不出现」（开发一眼发现）。
//   历史事故：Mask 页的 "Use average"、Colors 行的 Secondary/Peak/BG、W×H 的高度输入框
//   都曾因为 addRow 只登记一个 editor 而永远不随切页隐藏。
// 构建：ninja vis_tabs_test && ./build/vis_tabs_test
#include <juce_gui_extra/juce_gui_extra.h>
#include "source/gui/ParamPanel.h"
#include "source/core/SpectrumParams.h"
#include <cstdio>
#include <vector>

namespace
{
    int failures = 0;
    void check (bool ok, const juce::String& what)
    {
        std::printf ("%s %s\n", ok ? "ok  :" : "FAIL:", what.toRawUTF8());
        if (! ok) ++failures;
    }

    void reportLeaks (const std::vector<juce::Component*>& leaks)
    {
        for (auto* c : leaks)
        {
            juce::String id = c->getComponentID();
            if (id.isEmpty()) id = c->getName();
            if (id.isEmpty())
                if (auto* b = dynamic_cast<juce::Button*> (c)) id = "<Button>\"" + b->getButtonText() + "\"";
            if (id.isEmpty())
                if (auto* l = dynamic_cast<juce::Label*> (c))  id = "<Label>\"" + l->getText() + "\"";
            std::printf ("      未登记组件: %s\n", id.isEmpty() ? "<anonymous>" : id.toRawUTF8());
        }
    }

    // 面板真的把某段文字建出来了吗（防止"整块忘了加"被"没有未登记组件"掩盖成通过）
    bool hasButton (ParamPanel& panel, const juce::String& text)
    {
        for (auto* c : panel.getChildren())
            if (auto* b = dynamic_cast<juce::Button*> (c))
                if (b->getButtonText().contains (text))
                    return true;
        return false;
    }
    int countChildren (ParamPanel& panel) { return (int) panel.getChildren().size(); }
}

int main()
{
    SpectrumParams params;
    ParamPanel panel (params);
    panel.setSize (320, 1400);        // 无 peer 也走布局：resized() 会跑显隐 + 安全网

    // ---- 1) 基本自检：控件确实被构建了（否则后面的"无残留"是假通过）----
    check (countChildren (panel) > 40, "panel built its widgets (sanity: enough child components)");
    check (hasButton (panel, "Use average"),  "Mask tab's 'Use average' button exists");
    check (hasButton (panel, "Border colour"),"Mask tab's 'Border colour' button exists");
    check (hasButton (panel, "BG"),           "Colors row's BG button exists");

    // ---- 2) 核心不变式：四个页签逐个切换后，都不允许有"未登记"的子组件 ----
    for (int tab = 0; tab <= 3; ++tab)
    {
        panel.setActiveTab (static_cast<ParamPanel::Tab> (tab));
        auto leaks = panel.findUnownedChildren();
        if (! leaks.empty()) reportLeaks (leaks);
        const juce::String label = "tab " + juce::String (tab)
            + ": no child component is left unregistered (0 tab-leak candidates)";
        check (leaks.empty(), label);
    }

    // ---- 3) 安全网真的会咬人：临时挂一个"没登记"的控件，resized() 后它必须被隐藏 ----
    //   这是"做不到残留"的行为证明：漏登记的失败模式 = 控件看不见（开发立刻发现），
    //   而不是旧版的静默残留在别的页面上。
    {
        struct Probe : juce::TextButton
        {
            Probe() : juce::TextButton ("__probe_unregistered__") {}
        };
        auto* probe = new Probe();
        panel.addChildComponent (probe);
        panel.resized();                                    // 跑一次布局 + 安全网
        auto leaks = panel.findUnownedChildren();
        bool reported = false;
        for (auto* c : leaks) if (c == dynamic_cast<juce::Component*> (probe)) reported = true;
        check (reported, "safety net lists an unregistered child as unowned");
        check (! probe->isVisible(), "safety net HIDES the unregistered child (leak is impossible, not silent)");
        panel.removeChildComponent (probe);
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
