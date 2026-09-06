// =============================================================================
// WinDragCompat.cpp — Windows 提权 (UIPI) 拖放兼容层实现 (v0.5.0)
// 详见头文件说明。
// =============================================================================
#include "WinDragCompat.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <ole2.h>       // RevokeDragDrop (LEAN_AND_MEAN 会排除它)
 #include <shellapi.h>   // ShellExecuteW / DragAcceptFiles / DragQueryFile
 #include <commctrl.h>   // SetWindowSubclass / DefSubclassProc
 #include <atomic>

namespace avx::winDragCompat
{
    std::function<void (const juce::String&)> onStatus;
    std::function<void (const juce::StringArray&)> onNativeFilesDropped;

    namespace
    {
        void reportStatus (const juce::String& s)
        {
            if (onStatus) onStatus (s);
            juce::Logger::writeToLog ("[WinDragCompat] " + s);
        }

        constexpr UINT_PTR kSubclassId = 0xA027A0D5;   // 任意唯一 ID

        // 子类过程与安装点之间的会话数据
        juce::ComponentPeer* relayPeer = nullptr;
        bool oleRevoked = false;
        std::atomic<int> wmDropCount { 0 };

        // 把 WM_DROPFILES 的 HDROP 交付给业务层。
        // 实测教训（T4）：JUCE 的 handleDragMove/findDragAndDropTarget 分发链
        // 在跨协议 relay 时不可靠（目标查找失败静默吞 drop）——优先直连
        // onNativeFilesDropped 回调（同消息线程，确定性），JUCE 链仅作后备。
        void relayDrop (HDROP hDrop)
        {
            wmDropCount.fetch_add (1);
            auto* peer = relayPeer;
            if (peer == nullptr)
            {
                reportStatus ("WM_DROPFILES arrived but peer is null (dropped)");
                DragFinish (hDrop);
                return;
            }

            juce::ComponentPeer::DragInfo info;
            const UINT numFiles = DragQueryFile (hDrop, (UINT) -1, nullptr, 0);
            for (UINT i = 0; i < numFiles; ++i)
            {
                const UINT len = DragQueryFile (hDrop, i, nullptr, 0);
                std::vector<TCHAR> buf ((size_t) len + 1, 0);
                DragQueryFile (hDrop, i, buf.data(), (UINT) buf.size());
                info.files.add (juce::String (buf.data()));
            }
            DragFinish (hDrop);

            if (info.files.isEmpty())
            {
                reportStatus ("WM_DROPFILES arrived but file list empty");
                return;
            }

            reportStatus ("WM_DROPFILES arrived: " + juce::String ((int) numFiles)
                          + " file(s)");

            // 直连业务回调（确定性路径）
            if (onNativeFilesDropped)
            {
                onNativeFilesDropped (info.files);
                reportStatus ("Delivered via onNativeFilesDropped (direct)");
                return;
            }

            // 后备：JUCE 分发链（回调未接线时）
            info.position = peer->getComponent().getLocalBounds().getCentre();
            const bool delivered = peer->handleDragDrop (info);
            reportStatus (delivered ? "Delivered to JUCE drag-drop chain (async filesDropped)"
                                    : "handleDragDrop returned false (no interested target found!)");
        }

        LRESULT CALLBACK subclassProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                       UINT_PTR, DWORD_PTR)
        {
            if (msg == WM_DROPFILES)
            {
                relayDrop ((HDROP) wp);
                return 0;
            }
            return DefSubclassProc (hwnd, msg, wp, lp);
        }
    }

    juce::String getStatusLine()
    {
        return "elevated=" + juce::String (isProcessElevated() ? 1 : 0)
             + " oleRevoked=" + juce::String (oleRevoked ? 1 : 0)
             + " fallback=on"
             + " wmDropFiles=" + juce::String (wmDropCount.load());
    }

    bool isProcessElevated()
    {
        BOOL isAdmin = FALSE;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (! AllocateAndInitializeSid (&ntAuthority, 2,
                                        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                        0, 0, 0, 0, 0, 0, &adminGroup))
            return false;
        CheckTokenMembership (nullptr, adminGroup, &isAdmin);
        FreeSid (adminGroup);
        return isAdmin != FALSE;
    }

    bool relaunchDeElevated()
    {
        // 防死循环：环境变量标志（实测教训：explorer.exe 代理无法传命令行参数——
        // 拼在 args 里的 --avx-deelevated 会被 explorer 当作"导航位置"解析失败，
        // 最终打开 Documents 文件夹而非目标程序。ShellExecuteW 创建的子进程
        // 继承当前进程环境块，环境变量是可靠的跨实例通道）。
        wchar_t flagBuf[8] {};
        if (GetEnvironmentVariableW (L"AVX_DEELEVATED", flagBuf, 8) != 0)
        {
            reportStatus ("already de-elevated attempt (env flag); not retrying relaunch");
            return false;
        }

        wchar_t exePath[MAX_PATH] {};
        if (GetModuleFileNameW (nullptr, exePath, MAX_PATH) == 0)
            return false;

        // 标志走环境块（子进程自动继承）
        SetEnvironmentVariableW (L"AVX_DEELEVATED", L"1");

        // 关键手法：以 explorer.exe 为代理启动 → 子进程继承 explorer 的
        // Medium 完整性级别（而非当前进程的 High），实现自动降权。
        // explorer.exe 退出不影响子进程存活。args 只放引号包住的 exe 路径，
        // 不放任何附加参数（见上，explorer 会把它当导航目标）。
        juce::String args = "\"" + juce::String ((const wchar_t*) exePath) + "\"";
        HINSTANCE h = ShellExecuteW (nullptr, L"open", L"explorer.exe",
                                     args.toWideCharPointer(), nullptr, SW_SHOWNORMAL);
        const bool ok = (intptr_t) h > 32;
        reportStatus (ok ? "De-elevation relaunch via explorer.exe requested (env flag set)"
                         : "De-elevation relaunch FAILED (ShellExecute error "
                           + juce::String ((int) (intptr_t) h) + ")");
        return ok;
    }

    void installDragCompat (juce::Component* component)
    {
        if (component == nullptr)
            return;

        auto* peer = component->getPeer();
        if (peer == nullptr)
            return;

        auto* hwnd = (HWND) peer->getNativeHandle();
        if (hwnd == nullptr || ! IsWindow (hwnd))
            return;

        relayPeer = peer;

        // 1) 无条件启用老协议：OLE 注册失败/被杀软 hook 时，Explorer 见无
        //    OLE 目标会自动回退 WM_DROPFILES 链路；OLE 正常时零影响。
        DragAcceptFiles (hwnd, TRUE);
        oleRevoked = false;

        // 2) 提权场景：额外注销 OLE 目标（老协议成为唯一通道）
        if (isProcessElevated())
        {
            const HRESULT hr = RevokeDragDrop (hwnd);
            oleRevoked = true;
            reportStatus ("Elevated: OLE drop target revoked (hr=0x"
                          + juce::String::toHexString ((int) hr)
                          + "), WM_DROPFILES is the only channel");
        }

        // 3) 所有场景都挂 WM_DROPFILES 子类兜底
        if (! SetWindowSubclass (hwnd, subclassProc, kSubclassId, 0))
            reportStatus ("SetWindowSubclass FAILED (err="
                          + juce::String ((int) GetLastError()) + ")");

        reportStatus ("installed: elevated=" + juce::String (isProcessElevated() ? 1 : 0)
                      + " oleRevoked=" + juce::String (oleRevoked ? 1 : 0)
                      + " fallback=on wmDropFiles=0");
    }
}

#else  // 非 Windows：空实现

namespace avx::winDragCompat
{
    std::function<void (const juce::String&)> onStatus;
    std::function<void (const juce::StringArray&)> onNativeFilesDropped;
    bool isProcessElevated() { return false; }
    bool relaunchDeElevated() { return false; }
    juce::String getStatusLine() { return {}; }
    void installDragCompat (juce::Component*) {}
}

#endif
