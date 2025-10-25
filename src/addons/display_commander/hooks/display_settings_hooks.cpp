#include "display_settings_hooks.hpp"
#include "../utils.hpp"
#include "../utils/logging.hpp"
#include "../globals.hpp"
#include "../settings/developer_tab_settings.hpp"
#include "hook_suppression_manager.hpp"
#include <MinHook.h>
#include <windows.h>
#include <wingdi.h>

// Original function pointers
ChangeDisplaySettingsA_pfn ChangeDisplaySettingsA_Original = nullptr;
ChangeDisplaySettingsW_pfn ChangeDisplaySettingsW_Original = nullptr;
ChangeDisplaySettingsExA_pfn ChangeDisplaySettingsExA_Original = nullptr;
ChangeDisplaySettingsExW_pfn ChangeDisplaySettingsExW_Original = nullptr;

// Window management original function pointers
// SetWindowPos_Original moved to api_hooks.cpp to avoid duplicate declaration
ShowWindow_pfn ShowWindow_Original = nullptr;

// Hook installation state
static std::atomic<bool> g_display_settings_hooks_installed{false};

// Hook detour functions
LONG WINAPI ChangeDisplaySettingsA_Detour(DEVMODEA *lpDevMode, DWORD dwFlags) {
    g_display_settings_hook_counters[DISPLAY_SETTINGS_HOOK_CHANGEDISPLAYSETTINGSA].fetch_add(1);
    g_display_settings_hook_total_count.fetch_add(1);

    // Check if fullscreen prevention is enabled
    if (settings::g_developerTabSettings.prevent_fullscreen.GetValue()) {
        LogInfo("ChangeDisplaySettingsA blocked - fullscreen prevention enabled");
        return DISP_CHANGE_SUCCESSFUL; // Return success without changing display mode
    }

    return ChangeDisplaySettingsA_Original(lpDevMode, dwFlags);
}

LONG WINAPI ChangeDisplaySettingsW_Detour(DEVMODEW *lpDevMode, DWORD dwFlags) {
    g_display_settings_hook_counters[DISPLAY_SETTINGS_HOOK_CHANGEDISPLAYSETTINGSW].fetch_add(1);
    g_display_settings_hook_total_count.fetch_add(1);

    // Check if fullscreen prevention is enabled
    if (settings::g_developerTabSettings.prevent_fullscreen.GetValue()) {
        LogInfo("ChangeDisplaySettingsW blocked - fullscreen prevention enabled");
        return DISP_CHANGE_SUCCESSFUL; // Return success without changing display mode
    }

    return ChangeDisplaySettingsW_Original(lpDevMode, dwFlags);
}

LONG WINAPI ChangeDisplaySettingsExA_Detour(LPCSTR lpszDeviceName, DEVMODEA *lpDevMode, HWND hWnd, DWORD dwFlags, LPVOID lParam) {
    g_display_settings_hook_counters[DISPLAY_SETTINGS_HOOK_CHANGEDISPLAYSETTINGSEXA].fetch_add(1);
    g_display_settings_hook_total_count.fetch_add(1);

    // Check if fullscreen prevention is enabled
    if (settings::g_developerTabSettings.prevent_fullscreen.GetValue()) {
        LogInfo("ChangeDisplaySettingsExA blocked - fullscreen prevention enabled");
        return DISP_CHANGE_SUCCESSFUL; // Return success without changing display mode
    }

    return ChangeDisplaySettingsExA_Original(lpszDeviceName, lpDevMode, hWnd, dwFlags, lParam);
}

LONG WINAPI ChangeDisplaySettingsExW_Detour(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode, HWND hWnd, DWORD dwFlags, LPVOID lParam) {
    g_display_settings_hook_counters[DISPLAY_SETTINGS_HOOK_CHANGEDISPLAYSETTINGSEXW].fetch_add(1);
    g_display_settings_hook_total_count.fetch_add(1);

    // Check if fullscreen prevention is enabled
    if (settings::g_developerTabSettings.prevent_fullscreen.GetValue()) {
        LogInfo("ChangeDisplaySettingsExW blocked - fullscreen prevention enabled");
        return DISP_CHANGE_SUCCESSFUL; // Return success without changing display mode
    }

    return ChangeDisplaySettingsExW_Original(lpszDeviceName, lpDevMode, hWnd, dwFlags, lParam);
}

// SetWindowPos_Detour function moved to api_hooks.cpp to avoid duplicate hook creation

BOOL WINAPI ShowWindow_Detour(HWND hWnd, int nCmdShow) {
    g_display_settings_hook_counters[DISPLAY_SETTINGS_HOOK_SHOWWINDOW].fetch_add(1);
    g_display_settings_hook_total_count.fetch_add(1);

    // Check if fullscreen prevention is enabled
    if (settings::g_developerTabSettings.prevent_fullscreen.GetValue()) {
        // Prevent maximize operations that could lead to fullscreen
        if (nCmdShow == SW_MAXIMIZE || nCmdShow == SW_SHOWMAXIMIZED) {
            LogInfo("ShowWindow blocked maximize attempt - forcing normal window");
            return ShowWindow_Original(hWnd, SW_SHOWNORMAL);
        }
    }

    return ShowWindow_Original(hWnd, nCmdShow);
}



// Hook installation function
bool InstallDisplaySettingsHooks() {
    if (g_display_settings_hooks_installed.load()) {
        LogInfo("Display settings hooks already installed");
        return true;
    }

    // Check if display settings hooks should be suppressed
    if (display_commanderhooks::HookSuppressionManager::GetInstance().ShouldSuppressHook(display_commanderhooks::HookType::DISPLAY_SETTINGS)) {
        LogInfo("Display settings hooks installation suppressed by user setting");
        return false;
    }
    LogInfo("Installing display settings hooks...");

    // Hook ChangeDisplaySettingsA
    if (!CreateAndEnableHook(ChangeDisplaySettingsA, ChangeDisplaySettingsA_Detour, (LPVOID*)&ChangeDisplaySettingsA_Original, "ChangeDisplaySettingsA")) {
        LogError("Failed to create and enable ChangeDisplaySettingsA hook");
        return false;
    }

    // Hook ChangeDisplaySettingsW
    if (!CreateAndEnableHook(ChangeDisplaySettingsW, ChangeDisplaySettingsW_Detour, (LPVOID*)&ChangeDisplaySettingsW_Original, "ChangeDisplaySettingsW")) {
        LogError("Failed to create and enable ChangeDisplaySettingsW hook");
        return false;
    }

    // Hook ChangeDisplaySettingsExA
    if (!CreateAndEnableHook(ChangeDisplaySettingsExA, ChangeDisplaySettingsExA_Detour, (LPVOID*)&ChangeDisplaySettingsExA_Original, "ChangeDisplaySettingsExA")) {
        LogError("Failed to create and enable ChangeDisplaySettingsExA hook");
        return false;
    }

    // Hook ChangeDisplaySettingsExW
    if (!CreateAndEnableHook(ChangeDisplaySettingsExW, ChangeDisplaySettingsExW_Detour, (LPVOID*)&ChangeDisplaySettingsExW_Original, "ChangeDisplaySettingsExW")) {
        LogError("Failed to create and enable ChangeDisplaySettingsExW hook");
        return false;
    }

    // SetWindowPos hook moved to api_hooks.cpp to avoid duplicate hook creation

    if (!CreateAndEnableHook(ShowWindow, ShowWindow_Detour, (LPVOID*)&ShowWindow_Original, "ShowWindow")) {
        LogError("Failed to create and enable ShowWindow hook");
        return false;
    }


    #if 0
    if (!CreateAndEnableHook(SetWindowLongPtrW, SetWindowLongPtrW_Detour, (LPVOID*)&SetWindowLongPtrW_Original, "SetWindowLongPtrW")) {
        LogError("Failed to create and enable SetWindowLongPtrW hook");
        return false;
    }
    #endif

    g_display_settings_hooks_installed.store(true);
    LogInfo("Display settings hooks installed successfully");

    // Mark display settings hooks as installed
    display_commanderhooks::HookSuppressionManager::GetInstance().MarkHookInstalled(display_commanderhooks::HookType::DISPLAY_SETTINGS);

    return true;
}

// Hook uninstallation function
void UninstallDisplaySettingsHooks() {
    if (!g_display_settings_hooks_installed.load()) {
        return;
    }

    LogInfo("Uninstalling display settings hooks...");

    // Disable hooks
    if (ChangeDisplaySettingsA_Original) {
        MH_DisableHook(ChangeDisplaySettingsA);
        ChangeDisplaySettingsA_Original = nullptr;
    }

    if (ChangeDisplaySettingsW_Original) {
        MH_DisableHook(ChangeDisplaySettingsW);
        ChangeDisplaySettingsW_Original = nullptr;
    }

    if (ChangeDisplaySettingsExA_Original) {
        MH_DisableHook(ChangeDisplaySettingsExA);
        ChangeDisplaySettingsExA_Original = nullptr;
    }

    if (ChangeDisplaySettingsExW_Original) {
        MH_DisableHook(ChangeDisplaySettingsExW);
        ChangeDisplaySettingsExW_Original = nullptr;
    }

    // Disable window management hooks
    // SetWindowPos hook cleanup moved to api_hooks.cpp

    if (ShowWindow_Original) {
        MH_DisableHook(ShowWindow);
        ShowWindow_Original = nullptr;
    }



    g_display_settings_hooks_installed.store(false);
    LogInfo("Display settings hooks uninstalled");
}

// Check if hooks are installed
bool AreDisplaySettingsHooksInstalled() {
    return g_display_settings_hooks_installed.load();
}
