// =============================================================================
// WinDragCompat.h — Windows 提权 (UIPI) 拖放兼容层 (v0.5.0)
//
// 问题：进程以管理员运行时，Windows UIPI 阻断普通权限 Explorer 发起的
//       OLE 拖放（COM 跨完整性级别调用被内核拦截），光标全程禁止符。
//       JUCE 无条件 RegisterDragDrop 注册了 OLE 目标 → Explorer 只走 OLE，
//       被拦截后不回退 → 提权进程永远收不到拖放。
//
// 两层方案：
//   1) 首选 — 自动降权重启（relaunchDeElevated）：经 explorer.exe 代理重启
//      自身为普通完整性级别，OLE 拖放完整可用（含拖放悬停 HUD）。
//      AVXGuiApplication::initialise 里提权时自动调用。
//   2) 兜底 — WM_DROPFILES 老协议：若降权重启失败（仍提权运行），注销 OLE
//      目标 + DragAcceptFiles，让 Explorer 回退到 WM_DROPFILES 链路（该消息
//      已被 JUCE 的 ChangeWindowMessageFilterEx 放行，官方允许跨 UIPI）。
//      窗口子类过程收到后转 ComponentPeer::DragInfo 走 JUCE 分发链。
//
// 诊断：onStatus 回调把每一步状态（收到 WM_DROPFILES / 文件数 / 分发结果）
//       送给 UI（MainComponent 接到面板进度文本），便于远程排障。
//
// 非 Windows 平台：全部空实现。
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace avx::winDragCompat
{
    /// 进程是否以管理员 (elevated) 运行。非 Windows 恒 false。
    bool isProcessElevated();

    /// 经 explorer.exe 代理重启自身为普通权限。成功返回 true（调用方应退出当前实例）。
    /// args 追加 --avx-deelevated 标志，防止 UAC 最低档环境下无限重启循环。
    bool relaunchDeElevated();

    /// 诊断状态回调（消息线程调用；可空）。
    extern std::function<void (const juce::String&)> onStatus;

    /// WM_DROPFILES 到达后的直接交付回调（消息线程调用；可空）。
    /// 实测结论（T4）：JUCE 的 handleDragMove/findDragAndDropTarget 分发链在
    /// 跨协议 relay 时不可靠，直连业务回调（loadFile 等）是确定性行为。
    extern std::function<void (const juce::StringArray&)> onNativeFilesDropped;

    /// 单行状态汇总（面板/HUD 显示用）：
    /// "elevated=0/1 oleRevoked=0/1 fallback=on/na wmDropFiles=N"
    juce::String getStatusLine();

    /// 在组件 peer 的原生窗口上安装拖放兼容层：
    ///   · 无条件 DragAcceptFiles（OLE 失败时 Explorer 自动回退 WM_DROPFILES）
    ///   · 提权时额外 RevokeDragDrop（老协议成为唯一通道）
    ///   · SetWindowSubclass 挂 WM_DROPFILES 兜底处理
    /// 必须在消息线程调用（OLE STA 约束）。
    void installDragCompat (juce::Component* component);
}
