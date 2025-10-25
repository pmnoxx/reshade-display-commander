/*
 * Copyright (C) 2024 Display Commander
 * Window procedure hooks implementation using SetWindowLongPtr
 */

#include "window_proc_hooks.hpp"
#include "../exit_handler.hpp"
#include "../globals.hpp"
#include "../utils/logging.hpp"
#include "../ui/new_ui/window_info_tab.hpp"
#include <atomic>


namespace display_commanderhooks {

// Global variables for hook state
static std::atomic<bool> g_hooks_installed{false};
static HWND g_target_window = nullptr;
static WNDPROC g_original_window_proc = nullptr;

// Hooked window procedure
LRESULT CALLBACK WindowProc_Detour(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Track only known messages for debugging
    ui::new_ui::AddMessageToHistoryIfKnown(uMsg, wParam, lParam);

    // Check if continue rendering is enabled
    bool continue_rendering_enabled = s_continue_rendering.load();

    static bool sent_activate = false;
    if (g_target_window == hwnd && !sent_activate) {
        SendFakeActivationMessages(hwnd);
        sent_activate = true;
    }

    // Handle specific window messages here
    switch (uMsg) {
    case WM_ACTIVATE: {
        // Handle window activation
        if (continue_rendering_enabled) {
            // Suppress focus loss messages when continue rendering is enabled
            if (LOWORD(wParam) == WA_INACTIVE) {
                LogInfo("Suppressed window deactivation message due to continue rendering - HWND: 0x%p", hwnd);
                SendFakeActivationMessages(hwnd);
                return 0; // Suppress the message
            }
        }
        break;
    }

    case WM_SETFOCUS:
        // Handle focus changes - always allow focus gained
        break;

    case WM_KILLFOCUS:
        // Handle focus loss - suppress if continue rendering is enabled
        if (continue_rendering_enabled) {
            LogInfo("Suppressed WM_KILLFOCUS message due to continue rendering - HWND: 0x%p", hwnd);
           //SendFakeActivationMessages(hwnd);
            return 0; // Suppress the message
        }
        LogInfo("Window focus lost message received - HWND: 0x%p", hwnd);
        break;

    case WM_ACTIVATEAPP:
        // Handle application activation/deactivation
        if (continue_rendering_enabled) {
            if (wParam == FALSE) { // Application is being deactivated
                LogInfo("WM_ACTIVATEAPP: Suppressing application deactivation - HWND: 0x%p", hwnd);
                // Send fake activation to keep the game thinking it's active
           //     SendFakeActivationMessages(hwnd);
                return 0; // Suppress the message
            } else {
                // Application is being activated - ensure proper state
                LogInfo("WM_ACTIVATEAPP: Application activated - ensuring continued rendering - HWND: 0x%p", hwnd);
                // Send fake focus message to maintain active state
                DetourWindowMessage(hwnd, WM_SETFOCUS, 0, 0);
            }
        }
        break;

    case WM_NCACTIVATE:
        // Handle non-client area activation
        if (continue_rendering_enabled) {
            if (wParam != FALSE) {
                // Non-client area is being activated - ensure window stays active
                LogInfo("WM_NCACTIVATE: Window activated - ensuring continued rendering - HWND: 0x%p", hwnd);
                // Send fake focus message to maintain active state
                // DetourWindowMessage(hwnd, WM_SETFOCUS, 0, 0);
                return 0;
            } else {
                // Non-client area is being deactivated - suppress and fake activation
                LogInfo("WM_NCACTIVATE: Suppressing deactivation - HWND: 0x%p", hwnd);
           //     SendFakeActivationMessages(hwnd);
                return 0; // Suppress the message
            }
        }
        break;

    case WM_WINDOWPOSCHANGING: {
        // Handle window position changes
        WINDOWPOS *pWp = (WINDOWPOS *)lParam;

        // Check if this is a minimize/restore operation that might affect focus
        if (continue_rendering_enabled && (pWp->flags & SWP_SHOWWINDOW)) {
            // Check if window is being minimized
            if (IsIconic(hwnd)) {
                pWp->flags &= ~SWP_SHOWWINDOW; // Remove show window flag
            }
        }
        break;
    }

    case WM_WINDOWPOSCHANGED:
        // Handle window position changes
        if (continue_rendering_enabled) {
            WINDOWPOS *pWp = (WINDOWPOS *)lParam;
            // Check if window is being minimized or hidden
            if (pWp->flags & SWP_HIDEWINDOW) {
                LogInfo("WM_WINDOWPOSCHANGED: Suppressing window hide - HWND: 0x%p", hwnd);
          //      SendFakeActivationMessages(hwnd);
                return 0; // Suppress the message
            }
        }
        break;

    case WM_SHOWWINDOW:
        // Handle window visibility changes
        if (continue_rendering_enabled && wParam == FALSE) {
            // Suppress window hide messages when continue rendering is enabled
            // Send fake activation to keep the game thinking it's active
       //     SendFakeActivationMessages(hwnd);
            return 0; // Suppress the message
        }
        break;

    case WM_MOUSEACTIVATE:
        // Handle mouse activation
        if (continue_rendering_enabled) {
            LogInfo("WM_MOUSEACTIVATE: Activating and eating message - HWND: 0x%p", hwnd);
            // Always activate and eat the message to prevent focus loss
            return MA_ACTIVATEANDEAT; // Activate and eat the message
        }
        break;

    case WM_STYLECHANGING:
        // Handle style changes
        break;

    case WM_STYLECHANGED:
        // Handle style changes
        break;

    case WM_SYSCOMMAND:
        // Handle system commands
        if (continue_rendering_enabled) {
            // Prevent minimization when continue rendering is enabled
            if (wParam == SC_MINIMIZE) {
                LogInfo("WM_SYSCOMMAND: Suppressing minimize command - HWND: 0x%p", hwnd);
           //     SendFakeActivationMessages(hwnd);
                return 0; // Suppress the message
            }
        }
        break;

    case WM_ENTERSIZEMOVE:
        break;

    case WM_EXITSIZEMOVE:
        // Handle window exiting size/move mode
       // SendFakeActivationMessages(hwnd);
        break;

    case WM_QUIT:
        // Handle window quit message
        LogInfo("WM_QUIT: Window quit message received - HWND: 0x%p", hwnd);
        exit_handler::OnHandleExit(exit_handler::ExitSource::WINDOW_QUIT, "WM_QUIT message received");
        break;

    case WM_CLOSE:
        // Handle window close message
        LogInfo("WM_CLOSE: Window close message received - HWND: 0x%p", hwnd);
        exit_handler::OnHandleExit(exit_handler::ExitSource::WINDOW_CLOSE, "WM_CLOSE message received");
        break;

    case WM_DESTROY:
        // Handle window destroy message
        LogInfo("WM_DESTROY: Window destroy message received - HWND: 0x%p", hwnd);
        exit_handler::OnHandleExit(exit_handler::ExitSource::WINDOW_DESTROY, "WM_DESTROY message received");
        break;

    default:
        break;
    }

    // Call the original window procedure
    if (g_original_window_proc) {
        return CallWindowProc(g_original_window_proc, hwnd, uMsg, wParam, lParam);
    }

    // Fallback to default window procedure
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool InstallWindowProcHooks(HWND g_target_window) {
    LogInfo("InstallWindowProcHooks called");

    if (g_hooks_installed.load()) {
        LogInfo("Window procedure hooks already installed");
        return true;
    }

    if (g_target_window == nullptr) {
        LogError("No target window set for window procedure hooks");
        return false;
    }

    if (!IsWindow(g_target_window)) {
        LogError("Target window is not valid - HWND: 0x%p", g_target_window);
        return false;
    }

    // Get the current window procedure
    g_original_window_proc = (WNDPROC)GetWindowLongPtr(g_target_window, GWLP_WNDPROC);
    if (g_original_window_proc == nullptr) {
        LogError("Failed to get original window procedure for window - HWND: 0x%p", g_target_window);
        return false;
    }

    // Use SetWindowLongPtr to replace the window procedure instead of MinHook
    // This is more reliable for window procedures as they can be system procedures
    WNDPROC new_proc = (WNDPROC)SetWindowLongPtr(g_target_window, GWLP_WNDPROC, (LONG_PTR)WindowProc_Detour);
    if (new_proc == nullptr) {
        DWORD error = GetLastError();
        LogError("Failed to set window procedure - Error: %lu (0x%lx)", error, error);
        return false;
    }

    // Store the original procedure for restoration
    g_original_window_proc = new_proc;

    g_hooks_installed.store(true);
    LogInfo("Window procedure hooks installed successfully for window - HWND: 0x%p", g_target_window);

    // Debug: Show current continue rendering state
    bool current_state = s_continue_rendering.load();
    LogInfo("Window procedure hooks installed - continue_rendering state: %s", current_state ? "enabled" : "disabled");

    return true;
}

void UninstallWindowProcHooks() {
    if (!g_hooks_installed.load()) {
        LogInfo("Window procedure hooks not installed");
        return;
    }

    // Restore the original window procedure
    if (g_target_window && g_original_window_proc) {
        WNDPROC restored_proc =
            (WNDPROC)SetWindowLongPtr(g_target_window, GWLP_WNDPROC, (LONG_PTR)g_original_window_proc);
        if (restored_proc == nullptr) {
            DWORD error = GetLastError();
            LogWarn("Failed to restore original window procedure - Error: %lu (0x%lx)", error, error);
        } else {
            LogInfo("Original window procedure restored successfully");
        }
    }

    // Clean up
    g_original_window_proc = nullptr;
    g_target_window = nullptr;

    g_hooks_installed.store(false);
    LogInfo("Window procedure hooks uninstalled successfully");
}


bool IsContinueRenderingEnabled() { return s_continue_rendering.load(); }

// Fake activation functions
void SendFakeActivationMessages(HWND hwnd) {
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }

    // Send fake activation messages to keep the game thinking it's active
    // Based on Special-K's patterns for better compatibility
    PostMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
    PostMessage(hwnd, WM_SETFOCUS, 0, 0);
    PostMessage(hwnd, WM_ACTIVATEAPP, TRUE, 0);
    PostMessage(hwnd, WM_NCACTIVATE, TRUE, 0);

    LogInfo("Sent fake activation messages to window - HWND: 0x%p", hwnd);
}

// Set the target window to hook
void SetTargetWindow(HWND hwnd) {
    if (g_hooks_installed.load()) {
        LogWarn("Cannot change target window while hooks are installed");
        return;
    }

    g_target_window = hwnd;
    LogInfo("Target window set for window procedure hooks - HWND: 0x%p", hwnd);
}

// Get the currently hooked window
HWND GetHookedWindow() { return g_target_window; }

// Message detouring function (similar to Special-K's SK_DetourWindowProc)
LRESULT DetourWindowMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return 0;
    }

    // Send the message directly to the window procedure
    return SendMessage(hwnd, uMsg, wParam, lParam);
}

} // namespace display_commanderhooks
