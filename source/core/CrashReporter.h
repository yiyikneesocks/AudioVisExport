// =============================================================================
// CrashReporter.h — Windows 崩溃报告（v0.5.4 #8）
//
// 安装顶层异常过滤器：未处理异常时写
//   <exe目录>/crash/crash_YYYYMMDD_HHMMSS.dmp   （MiniDump，Visual Studio/WinDbg 可打开）
//   <exe目录>/crash/crash_YYYYMMDD_HHMMSS.txt   （异常码/地址/版本/系统摘要）
// 用户把这两个文件发回来即可定位崩溃模块与偏移。
// 仅 Windows 生效；其它平台为空实现。
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>

namespace CrashReporter
{
    // 在 app initialise() 最开始调用一次。dir 为空 = exe 同级 /crash/。
    void install (const juce::File& dir = {});

    // 供 JUCEApplication::unhandledException（消息线程异常）写入文本报告
    void writeTextReport (const juce::String& what, const juce::String& where, int line);
}
