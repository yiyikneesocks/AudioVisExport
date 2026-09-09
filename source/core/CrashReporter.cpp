// =============================================================================
// CrashReporter.cpp — Windows 崩溃报告实现（#8）
// =============================================================================
#include "CrashReporter.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <excpt.h>
 #include <dbghelp.h>
 #include <juce_gui_basics/juce_gui_basics.h>   // JUCEApplicationBase（版本信息）
 #pragma comment (lib, "dbghelp.lib")
#endif

namespace
{
    juce::File crashDir;

#if JUCE_WINDOWS
    // 异常码 → 简短说明
    juce::String describeCode (DWORD code)
    {
        switch (code)
        {
            case EXCEPTION_ACCESS_VIOLATION:        return "ACCESS_VIOLATION (wild pointer / heap corruption)";
            case EXCEPTION_STACK_OVERFLOW:          return "STACK_OVERFLOW";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:   return "ARRAY_BOUNDS";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:      return "DIVIDE_BY_ZERO";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:      return "FLT_DIVIDE_BY_ZERO";
            case 0xC0000374:                        return "HEAP_CORRUPTION";
            default: break;
        }
        return "0x" + juce::String::toHexString ((int) code);
    }

    LONG WINAPI topLevelFilter (EXCEPTION_POINTERS* ep)
    {
        if (crashDir == juce::File())
            return EXCEPTION_CONTINUE_SEARCH;
        crashDir.createDirectory();

        const auto t    = juce::Time::getCurrentTime();
        const auto stamp = t.formatted ("%Y%m%d_%H%M%S");
        const auto code  = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
        const auto addr  = ep && ep->ExceptionRecord ? (uintptr_t) ep->ExceptionRecord->ExceptionAddress : 0;

        // ---- 文本摘要（人人能看懂）----
        juce::String txt;
        txt << "AudioVisExport crash report\n"
            << "time   : " << t.toString (true, true) << "\n"
            << "code   : " << describeCode (code) << "\n"
            << "address: 0x" << juce::String::toHexString ((juce::int64) addr) << "\n"
            << "module : (see .dmp for stack; compare address with AudioVisGUI.exe base in WinDbg)\n"
            << "version: " << (juce::JUCEApplicationBase::getInstance() != nullptr
                              ? juce::JUCEApplicationBase::getInstance()->getApplicationName()
                              : juce::String ("?")) << "\n"
            << "build  : " << __DATE__ << " " << __TIME__ << "\n";
        crashDir.getChildFile ("crash_" + stamp + ".txt").replaceWithText (txt);

        // ---- MiniDump ----
        HANDLE hFile = CreateFileW (crashDir.getChildFile ("crash_" + stamp + ".dmp")
                                        .getFullPathName().toWideCharPointer(),
                                    GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE && ep != nullptr)
        {
            MINIDUMP_EXCEPTION_INFORMATION mei { GetCurrentThreadId(), ep, FALSE };
            MiniDumpWriteDump (GetCurrentProcess(), GetCurrentProcessId(), hFile,
                               MiniDumpNormal, &mei, nullptr, nullptr);
        }
        if (hFile != INVALID_HANDLE_VALUE)
            CloseHandle (hFile);

        return EXCEPTION_CONTINUE_SEARCH;   // 不吞异常，保留系统 WER 行为
    }
#endif
}

void CrashReporter::install (const juce::File& dir)
{
#if JUCE_WINDOWS
    crashDir = dir == juce::File()
                   ? juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                         .getParentDirectory().getChildFile ("crash")
                   : dir;
    SetUnhandledExceptionFilter (topLevelFilter);
    // CRT 的 abort/assert 也走这里（纯虚调用等）
    _set_abort_behavior (_CALL_REPORTFAULT, _CALL_REPORTFAULT);
#else
    juce::ignoreUnused (dir);
#endif
}

void CrashReporter::writeTextReport (const juce::String& what, const juce::String& where, int line)
{
#if JUCE_WINDOWS
    if (crashDir == juce::File())
        crashDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                       .getParentDirectory().getChildFile ("crash");
    crashDir.createDirectory();
    juce::String txt;
    txt << "JUCE unhandled exception (message thread)\n"
        << "what : " << what << "\n"
        << "where: " << where << " : " << line << "\n";
    crashDir.getChildFile ("crash_juce_"
                           + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".txt")
        .replaceWithText (txt);
#else
    juce::ignoreUnused (what, where, line);
#endif
}
