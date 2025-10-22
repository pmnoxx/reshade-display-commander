#include "experimental_tab.hpp"
#include "../../res/forkawesome.h"
#include "../../autoclick/autoclick_manager.hpp"
#include "../../dlss/dlss_indicator_manager.hpp"
#include "../../globals.hpp"
#include "../../hooks/sleep_hooks.hpp"
#include "../../hooks/timeslowdown_hooks.hpp"
#include "../../hooks/hid_suppression_hooks.hpp"
#include "../../hooks/debug_output_hooks.hpp"
#include "../../settings/experimental_tab_settings.hpp"
#include "../../widgets/dualsense_widget/dualsense_widget.hpp"
#include "../../utils.hpp"
#include "../../utils/timing.hpp"
#include "../../utils/stack_trace.hpp"

#include <windows.h>

#include <atomic>
#include <thread>

namespace ui::new_ui {

// Initialize experimental tab
void InitExperimentalTab() {
    LogInfo("InitExperimentalTab() - Settings already loaded at startup");

    // Apply the loaded settings to the actual hook system
    // This ensures the hook system matches the UI settings
    LogInfo("InitExperimentalTab() - Applying loaded timer hook settings to hook system");
    display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::QueryPerformanceCounter,
                                    static_cast<display_commanderhooks::TimerHookType>(
                                        settings::g_experimentalTabSettings.query_performance_counter_hook.GetValue()));
    display_commanderhooks::SetTimerHookTypeById(
        display_commanderhooks::TimerHookIdentifier::GetTickCount,
        static_cast<display_commanderhooks::TimerHookType>(settings::g_experimentalTabSettings.get_tick_count_hook.GetValue()));
    display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::GetTickCount64,
                                    static_cast<display_commanderhooks::TimerHookType>(
                                        settings::g_experimentalTabSettings.get_tick_count64_hook.GetValue()));
    display_commanderhooks::SetTimerHookTypeById(
        display_commanderhooks::TimerHookIdentifier::TimeGetTime,
        static_cast<display_commanderhooks::TimerHookType>(settings::g_experimentalTabSettings.time_get_time_hook.GetValue()));
    display_commanderhooks::SetTimerHookTypeById(
        display_commanderhooks::TimerHookIdentifier::GetSystemTime,
        static_cast<display_commanderhooks::TimerHookType>(settings::g_experimentalTabSettings.get_system_time_hook.GetValue()));
    display_commanderhooks::SetTimerHookTypeById(
        display_commanderhooks::TimerHookIdentifier::GetSystemTimeAsFileTime,
        static_cast<display_commanderhooks::TimerHookType>(
            settings::g_experimentalTabSettings.get_system_time_as_file_time_hook.GetValue()));
    display_commanderhooks::SetTimerHookTypeById(
        display_commanderhooks::TimerHookIdentifier::GetSystemTimePreciseAsFileTime,
        static_cast<display_commanderhooks::TimerHookType>(
            settings::g_experimentalTabSettings.get_system_time_precise_as_file_time_hook.GetValue()));
    display_commanderhooks::SetTimerHookTypeById(
        display_commanderhooks::TimerHookIdentifier::GetLocalTime,
        static_cast<display_commanderhooks::TimerHookType>(settings::g_experimentalTabSettings.get_local_time_hook.GetValue()));
    display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::NtQuerySystemTime,
                                    static_cast<display_commanderhooks::TimerHookType>(
                                        settings::g_experimentalTabSettings.nt_query_system_time_hook.GetValue()));

    // Apply DirectInput hook suppression setting
    s_suppress_dinput_hooks.store(settings::g_experimentalTabSettings.suppress_dinput_hooks.GetValue());

    LogInfo("InitExperimentalTab() - Experimental tab settings loaded and applied to hook system");
}


void DrawExperimentalTab() {
    ImGui::Text("Experimental Tab - Advanced Features");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Direct3D 9 FLIPEX Upgrade", ImGuiTreeNodeFlags_None)) {
        DrawD3D9FlipExControls();
    }
    ImGui::Spacing();


    if (ImGui::CollapsingHeader("Backbuffer Format Override", ImGuiTreeNodeFlags_None)) {
        // Draw backbuffer format override section
        DrawBackbufferFormatOverride();

        ImGui::Spacing();

        // Draw buffer resolution upgrade section
        DrawBufferResolutionUpgrade();

        ImGui::Spacing();

        // Draw texture format upgrade section
        DrawTextureFormatUpgrade();
    }
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Auto-Click Sequences", ImGuiTreeNodeFlags_None)) {

        // Display current cursor position prominently at the top
        POINT mouse_pos;
        GetCursorPos(&mouse_pos);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "=== LIVE CURSOR POSITION ===");
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "X: %ld  |  Y: %ld", mouse_pos.x, mouse_pos.y);

        // Show game window coordinates if available
        HWND hwnd = g_last_swapchain_hwnd.load();
        if (hwnd && IsWindow(hwnd)) {
            POINT client_pos = mouse_pos;
            ScreenToClient(hwnd, &client_pos);
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Game Window: X: %ld  |  Y: %ld", client_pos.x,
                            client_pos.y);
        }

        // Copy coordinates buttons
        ImGui::Spacing();
        if (ImGui::Button("Copy Screen Coords")) {
            std::string coords = std::to_string(mouse_pos.x) + ", " + std::to_string(mouse_pos.y);
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                HGLOBAL h_clipboard_data = GlobalAlloc(GMEM_DDESHARE, coords.length() + 1);
                if (h_clipboard_data) {
                    char *pch_data = static_cast<char*>(GlobalLock(h_clipboard_data));
                    if (pch_data) {
                        strcpy_s(pch_data, coords.length() + 1, coords.c_str());
                        GlobalUnlock(h_clipboard_data);
                        SetClipboardData(CF_TEXT, h_clipboard_data);
                    }
                }
                CloseClipboard();
                LogInfo("Screen coordinates copied to clipboard: %s", coords.c_str());
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Copy current screen coordinates to clipboard.");
        }

        if (hwnd && IsWindow(hwnd)) {
            ImGui::SameLine();
            if (ImGui::Button("Copy Game Window Coords")) {
                POINT client_pos = mouse_pos;
                ScreenToClient(hwnd, &client_pos);
                std::string coords = std::to_string(client_pos.x) + ", " + std::to_string(client_pos.y);
                if (OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    HGLOBAL h_clipboard_data = GlobalAlloc(GMEM_DDESHARE, coords.length() + 1);
                    if (h_clipboard_data) {
                        char *pch_data = static_cast<char*>(GlobalLock(h_clipboard_data));
                        if (pch_data) {
                            strcpy_s(pch_data, coords.length() + 1, coords.c_str());
                            GlobalUnlock(h_clipboard_data);
                            SetClipboardData(CF_TEXT, h_clipboard_data);
                        }
                    }
                    CloseClipboard();
                    LogInfo("Game window coordinates copied to clipboard: %s", coords.c_str());
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Copy current game window coordinates to clipboard.");
            }
        }

        // Draw auto-click feature
        autoclick::DrawAutoClickFeature();

        ImGui::Separator();

        // Draw mouse coordinates display
        DrawMouseCoordinatesDisplay();
    }
    ImGui::Spacing();

    // Draw sleep hook controls
    if (ImGui::CollapsingHeader("Sleep Hook Controls", ImGuiTreeNodeFlags_None)) {
        DrawSleepHookControls();
    }
    ImGui::Spacing();

    // Draw time slowdown controls
    if (ImGui::CollapsingHeader("Time Slowdown Controls", ImGuiTreeNodeFlags_None)) {
        DrawTimeSlowdownControls();
    }
    ImGui::Spacing();

    // Draw HID suppression controls
    if (ImGui::CollapsingHeader("HID Suppression", ImGuiTreeNodeFlags_None)) {
        DrawHIDSuppression();
    }
    ImGui::Spacing();

    // Draw DualSense widget
    if (ImGui::CollapsingHeader("DualSense Controller Monitor", ImGuiTreeNodeFlags_None)) {
        display_commander::widgets::dualsense_widget::DrawDualSenseWidget();
    }
    ImGui::Spacing();

    // Draw developer tools
    if (ImGui::CollapsingHeader("Developer Tools", ImGuiTreeNodeFlags_None)) {
        DrawDeveloperTools();
    }

    ImGui::Spacing();

    // Draw debug output hooks
    if (ImGui::CollapsingHeader("Debug Output Hooks", ImGuiTreeNodeFlags_None)) {
        DrawDebugOutputHooks();
    }

}


void DrawMouseCoordinatesDisplay() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Current Cursor Position ===");

    // Get current mouse position
    POINT mouse_pos;
    GetCursorPos(&mouse_pos);

    // Display current cursor position prominently
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.8f, 1.0f), "Current Cursor Position:");
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Screen: (%ld, %ld)", mouse_pos.x, mouse_pos.y);

    // Get game window handle and convert to client coordinates
    HWND hwnd = g_last_swapchain_hwnd.load();
    if (hwnd && IsWindow(hwnd)) {
        POINT client_pos = mouse_pos;
        ScreenToClient(hwnd, &client_pos);

        // Display client coordinates prominently
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Game Window: (%ld, %ld)", client_pos.x, client_pos.y);

        // Get window rectangle for reference
        RECT window_rect;
        if (GetWindowRect(hwnd, &window_rect)) {
            ImGui::Text("Game Window Screen Position: (%ld, %ld) to (%ld, %ld)", window_rect.left, window_rect.top,
                        window_rect.right, window_rect.bottom);
            ImGui::Text("Game Window Size: %ld x %ld", window_rect.right - window_rect.left,
                        window_rect.bottom - window_rect.top);
        }

        // Check if mouse is over the game window
        bool mouse_over_window = (mouse_pos.x >= window_rect.left && mouse_pos.x <= window_rect.right &&
                                  mouse_pos.y >= window_rect.top && mouse_pos.y <= window_rect.bottom);

        if (mouse_over_window) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ICON_FK_OK " Mouse is over game window");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), ICON_FK_WARNING " Mouse is outside game window");
        }

    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), ICON_FK_WARNING " No valid game window handle available");
    }

    // Refresh button to update coordinates
    if (ImGui::Button("Refresh Coordinates")) {
        // Coordinates are updated automatically, this is just for user feedback
        LogInfo("Mouse coordinates refreshed");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Refresh the mouse coordinate display (coordinates update automatically).");
    }

    // Additional debugging info
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Debug Information:");
    ImGui::Text("Game Window Handle: 0x%p", hwnd);
    ImGui::Text("Window Valid: %s", (hwnd && IsWindow(hwnd)) ? "Yes" : "No");

    // Show current foreground window for comparison
    HWND foreground_hwnd = GetForegroundWindow();
    ImGui::Text("Foreground Window: 0x%p", foreground_hwnd);
    ImGui::Text("Game Window is Foreground: %s", (hwnd == foreground_hwnd) ? "Yes" : "No");
}

// Cleanup function to stop background threads
void CleanupExperimentalTab() {
    // Disable auto-click (thread will sleep when disabled)
    if (g_auto_click_enabled.load()) {
        g_auto_click_enabled.store(false);
        LogInfo("Experimental tab cleanup: Auto-click disabled (thread will sleep)");
    }
}

void DrawBackbufferFormatOverride() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Backbuffer Format Override ===");

    // Warning about experimental nature
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " EXPERIMENTAL FEATURE - May cause compatibility issues!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This feature overrides the backbuffer format during swapchain creation.\nUse with caution "
                          "as it may cause rendering issues or crashes in some games.");
    }

    ImGui::Spacing();

    // Enable/disable checkbox
    if (CheckboxSetting(settings::g_experimentalTabSettings.backbuffer_format_override_enabled,
                        "Enable Backbuffer Format Override")) {
        LogInfo("Backbuffer format override %s",
                settings::g_experimentalTabSettings.backbuffer_format_override_enabled.GetValue() ? "enabled"
                                                                                                  : "disabled");
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Override the backbuffer format during swapchain creation.\nRequires restart to take effect.");
    }

    // Format selection combo (only enabled when override is enabled)
    if (settings::g_experimentalTabSettings.backbuffer_format_override_enabled.GetValue()) {
        ImGui::Spacing();
        ImGui::Text("Target Format:");

        if (ComboSettingWrapper(settings::g_experimentalTabSettings.backbuffer_format_override, "Format")) {
            LogInfo("Backbuffer format override changed to: %s",
                    settings::g_experimentalTabSettings.backbuffer_format_override
                        .GetLabels()[settings::g_experimentalTabSettings.backbuffer_format_override.GetValue()]);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select the target backbuffer format:\n"
                              "• R8G8B8A8_UNORM: Standard 8-bit per channel (32-bit total)\n"
                              "• R10G10B10A2_UNORM: 10-bit RGB + 2-bit alpha (32-bit total)\n"
                              "• R16G16B16A16_FLOAT: 16-bit HDR floating point (64-bit total)");
        }

        // Show current format info
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Note: Changes require restart to take effect");
    }
}

void DrawBufferResolutionUpgrade() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Buffer Resolution Upgrade ===");

    // Warning about experimental nature
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " EXPERIMENTAL FEATURE - May cause performance issues!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This feature upgrades internal buffer resolutions during resource creation.\nUse with "
                          "caution as it may cause performance issues or rendering artifacts.");
    }

    ImGui::Spacing();

    // Enable/disable checkbox
    if (CheckboxSetting(settings::g_experimentalTabSettings.buffer_resolution_upgrade_enabled,
                        "Enable Buffer Resolution Upgrade")) {
        LogInfo("Buffer resolution upgrade %s",
                settings::g_experimentalTabSettings.buffer_resolution_upgrade_enabled.GetValue() ? "enabled"
                                                                                                 : "disabled");
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Upgrade internal buffer resolutions during resource creation.\nRequires restart to take effect.");
    }

    // Resolution upgrade controls (only enabled when upgrade is enabled)
    if (settings::g_experimentalTabSettings.buffer_resolution_upgrade_enabled.GetValue()) {
        ImGui::Spacing();

        // Mode selection
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.buffer_resolution_upgrade_mode, "Upgrade Mode")) {
            LogInfo("Buffer resolution upgrade mode changed to: %s",
                    settings::g_experimentalTabSettings.buffer_resolution_upgrade_mode
                        .GetLabels()[settings::g_experimentalTabSettings.buffer_resolution_upgrade_mode.GetValue()]);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Select the buffer resolution upgrade mode:\n"
                "• Upgrade 1280x720 by Scale Factor: Specifically upgrade 1280x720 buffers by the scale factor\n"
                "• Upgrade by Scale Factor: Scale all buffers by the specified factor\n"
                "• Upgrade Custom Resolution: Upgrade specific resolution to custom target");
        }

        // Scale factor control (for both mode 0 and mode 1)
        if (settings::g_experimentalTabSettings.buffer_resolution_upgrade_mode.GetValue() == 0 ||
            settings::g_experimentalTabSettings.buffer_resolution_upgrade_mode.GetValue() == 1) {
            ImGui::Spacing();
            ImGui::Text("Scale Factor:");

            if (SliderIntSetting(settings::g_experimentalTabSettings.buffer_resolution_upgrade_scale_factor,
                                 "Scale Factor")) {
                LogInfo("Buffer resolution upgrade scale factor changed to: %d",
                        settings::g_experimentalTabSettings.buffer_resolution_upgrade_scale_factor.GetValue());
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Scale factor to apply to all buffer resolutions (1-4x)");
            }
        }

        // Custom resolution controls (for custom mode)
        if (settings::g_experimentalTabSettings.buffer_resolution_upgrade_mode.GetValue() == 2) { // Custom Resolution
            ImGui::Spacing();
            ImGui::Text("Target Resolution:");

            ImGui::SetNextItemWidth(120);
            if (SliderIntSetting(settings::g_experimentalTabSettings.buffer_resolution_upgrade_width, "Width")) {
                LogInfo("Buffer resolution upgrade width changed to: %d",
                        settings::g_experimentalTabSettings.buffer_resolution_upgrade_width.GetValue());
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            if (SliderIntSetting(settings::g_experimentalTabSettings.buffer_resolution_upgrade_height, "Height")) {
                LogInfo("Buffer resolution upgrade height changed to: %d",
                        settings::g_experimentalTabSettings.buffer_resolution_upgrade_height.GetValue());
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Target resolution for buffer upgrades.\nWidth: 320-7680, Height: 240-4320");
            }
        }

        // Show current settings info
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Note: Changes require restart to take effect");

        // Show what the upgrade will do
        int mode = settings::g_experimentalTabSettings.buffer_resolution_upgrade_mode.GetValue();
        int scale = settings::g_experimentalTabSettings.buffer_resolution_upgrade_scale_factor.GetValue();
        if (mode == 0) {
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Will upgrade 1280x720 buffers to %dx%d (%dx scale)",
                               1280 * scale, 720 * scale, scale);
        } else if (mode == 1) {
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Will scale all buffers by %dx", scale);
        } else if (mode == 2) {
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Will upgrade buffers to: %dx%d",
                               settings::g_experimentalTabSettings.buffer_resolution_upgrade_width.GetValue(),
                               settings::g_experimentalTabSettings.buffer_resolution_upgrade_height.GetValue());
        }
    }
}

void DrawTextureFormatUpgrade() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Texture Format Upgrade ===");

    // Warning about experimental nature
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " EXPERIMENTAL FEATURE - May cause performance issues!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This feature upgrades texture formats to RGB16A16 during resource creation.\nUse with "
                          "caution as it may cause performance issues or rendering artifacts.");
    }

    ImGui::Spacing();

    // Enable/disable checkbox
    if (CheckboxSetting(settings::g_experimentalTabSettings.texture_format_upgrade_enabled,
                        "Upgrade Textures to RGB16A16")) {
        LogInfo("Texture format upgrade %s",
                settings::g_experimentalTabSettings.texture_format_upgrade_enabled.GetValue() ? "enabled" : "disabled");
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Upgrade texture formats to RGB16A16 (16-bit per channel) for textures at 720p, 1440p, and "
                          "4K resolutions.\nRequires restart to take effect.");
    }

    // Show current settings info
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Note: Changes require restart to take effect");

    if (settings::g_experimentalTabSettings.texture_format_upgrade_enabled.GetValue()) {
        ImGui::TextColored(
            ImVec4(0.8f, 1.0f, 0.8f, 1.0f),
            "Will upgrade texture formats to RGB16A16 (16-bit per channel) for 720p, 1440p, and 4K textures");
    }
}

void DrawSleepHookControls() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Sleep Hook Controls ===");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                       ICON_FK_WARNING " EXPERIMENTAL FEATURE - Hooks game sleep calls for FPS control!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "This feature hooks Windows Sleep APIs (Sleep, SleepEx, WaitForSingleObject, WaitForMultipleObjects) to "
            "modify sleep durations.\nUseful for games that use sleep-based FPS limiting like Unity games.");
    }

    ImGui::Spacing();

    // Enable/disable checkbox
    if (CheckboxSetting(settings::g_experimentalTabSettings.sleep_hook_enabled, "Enable Sleep Hooks")) {
        LogInfo("Sleep hooks %s",
                settings::g_experimentalTabSettings.sleep_hook_enabled.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable hooks for Windows Sleep APIs to modify sleep durations for FPS control.");
    }

    // Render thread only option removed

    if (settings::g_experimentalTabSettings.sleep_hook_enabled.GetValue()) {
        ImGui::Spacing();

        // Sleep multiplier slider
        if (SliderFloatSetting(settings::g_experimentalTabSettings.sleep_multiplier, "Sleep Multiplier", "%.2fx")) {
            LogInfo("Sleep multiplier set to %.2fx", settings::g_experimentalTabSettings.sleep_multiplier.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Multiplier applied to sleep durations. 1.0 = no change, 0.5 = half duration, 2.0 = double duration.");
        }

        // Min sleep duration slider
        if (SliderIntSetting(settings::g_experimentalTabSettings.min_sleep_duration_ms, "Min Sleep Duration (ms)",
                             "%d ms")) {
            LogInfo("Min sleep duration set to %d ms",
                    settings::g_experimentalTabSettings.min_sleep_duration_ms.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Minimum sleep duration in milliseconds. 0 = no minimum limit.");
        }

        // Max sleep duration slider
        if (SliderIntSetting(settings::g_experimentalTabSettings.max_sleep_duration_ms, "Max Sleep Duration (ms)",
                             "%d ms")) {
            LogInfo("Max sleep duration set to %d ms",
                    settings::g_experimentalTabSettings.max_sleep_duration_ms.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Maximum sleep duration in milliseconds. 0 = no maximum limit.");
        }

        ImGui::Spacing();

        // Show current settings summary
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Current Settings:");
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Multiplier: %.2fx",
                           settings::g_experimentalTabSettings.sleep_multiplier.GetValue());
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Min Duration: %d ms",
                           settings::g_experimentalTabSettings.min_sleep_duration_ms.GetValue());
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Max Duration: %d ms",
                           settings::g_experimentalTabSettings.max_sleep_duration_ms.GetValue());

        // Show hook statistics if available
        if (display_commanderhooks::g_sleep_hook_stats.total_calls.load() > 0) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Hook Statistics:");
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Total Calls: %llu",
                               display_commanderhooks::g_sleep_hook_stats.total_calls.load());
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Modified Calls: %llu",
                               display_commanderhooks::g_sleep_hook_stats.modified_calls.load());

            uint64_t total_original = display_commanderhooks::g_sleep_hook_stats.total_original_duration_ms.load();
            uint64_t total_modified = display_commanderhooks::g_sleep_hook_stats.total_modified_duration_ms.load();
            if (total_original > 0) {
                ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Total Original Duration: %llu ms",
                                   total_original);
                ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Total Modified Duration: %llu ms",
                                   total_modified);
                ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Time Saved: %lld ms",
                                   static_cast<int64_t>(total_original) - static_cast<int64_t>(total_modified));
            }
        }
    }
}

void DrawTimeSlowdownControls() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Time Slowdown Controls ===");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                       ICON_FK_WARNING " EXPERIMENTAL FEATURE - Manipulates game time via multiple timer APIs!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This feature hooks multiple timer APIs to manipulate game time.\nUseful for bypassing FPS "
                          "limits and slowing down/speeding up games that use various timing methods.");
    }

    ImGui::Spacing();

    // Enable/disable checkbox
    if (CheckboxSetting(settings::g_experimentalTabSettings.timeslowdown_enabled, "Enable Time Slowdown")) {
        LogInfo("Time slowdown %s",
                settings::g_experimentalTabSettings.timeslowdown_enabled.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable time manipulation via timer API hooks.");
    }
    ImGui::SameLine();

    // Compatibility mode checkbox
    if (CheckboxSetting(settings::g_experimentalTabSettings.timeslowdown_compatibility_mode, "Compatibility Mode")) {
        LogInfo("Time slowdown compatibility mode %s",
                settings::g_experimentalTabSettings.timeslowdown_compatibility_mode.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable compatibility mode for time slowdown hooks. This may improve compatibility with certain games.");
    }
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset TS")) {
		// Reset time slowdown to defaults
		settings::g_experimentalTabSettings.timeslowdown_enabled.SetValue(false);
		display_commanderhooks::SetTimeslowdownEnabled(false);
		settings::g_experimentalTabSettings.timeslowdown_multiplier.SetValue(1.0f);
		display_commanderhooks::SetTimeslowdownMultiplier(1.0f);
		LogInfo("Time slowdown reset: disabled and multiplier set to 1.0x");
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Disable Time Slowdown and set multiplier to 1.0x.");
	}

    if (settings::g_experimentalTabSettings.timeslowdown_enabled.GetValue()) {
        ImGui::Spacing();

        // Max time multiplier slider (controls upper bound of Time Multiplier)
        if (SliderFloatSetting(settings::g_experimentalTabSettings.timeslowdown_max_multiplier, "Max Time Multiplier",
                               "%.0fx")) {
            float new_max = settings::g_experimentalTabSettings.timeslowdown_max_multiplier.GetValue();
            settings::g_experimentalTabSettings.timeslowdown_multiplier.SetMax(new_max);
            float cur = settings::g_experimentalTabSettings.timeslowdown_multiplier.GetValue();
            if (cur > new_max) {
                settings::g_experimentalTabSettings.timeslowdown_multiplier.SetValue(new_max);
            }
            LogInfo("Max time multiplier set to %.0fx", new_max);
        } else {
            // Ensure the slider respects the current max even if unchanged this frame
            settings::g_experimentalTabSettings.timeslowdown_multiplier.SetMax(
                settings::g_experimentalTabSettings.timeslowdown_max_multiplier.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Sets the maximum allowed value for Time Multiplier (1–1000x).");
        }

        // Time multiplier slider
        if (SliderFloatSetting(settings::g_experimentalTabSettings.timeslowdown_multiplier, "Time Multiplier",
                               "%.2fx")) {
            LogInfo("Time multiplier set to %.2fx",
                    settings::g_experimentalTabSettings.timeslowdown_multiplier.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Multiplier for game time. 1.0 = normal speed, 0.5 = half speed, 2.0 = double speed.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Timer Hook Selection
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Timer Hook Selection:");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Choose which timer APIs to hook (None/Enabled)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select which timer APIs to hook for time manipulation.");
        }

        ImGui::Spacing();

        // QueryPerformanceCounter hook
        uint64_t qpc_calls = display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::QueryPerformanceCounter);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.query_performance_counter_hook,
                                "QueryPerformanceCounter")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.query_performance_counter_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::QueryPerformanceCounter, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", qpc_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("High-resolution timer used by most modern games for precise timing.");
        }

        // GetTickCount hook
        uint64_t gtc_calls = display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::GetTickCount);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.get_tick_count_hook, "GetTickCount")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.get_tick_count_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::GetTickCount, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", gtc_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("32-bit millisecond timer, commonly used by older games.");
        }

        // GetTickCount64 hook
        uint64_t gtc64_calls = display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::GetTickCount64);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.get_tick_count64_hook, "GetTickCount64")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.get_tick_count64_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::GetTickCount64, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", gtc64_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("64-bit millisecond timer, used by some modern games.");
        }

        // timeGetTime hook
        uint64_t tgt_calls = display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::TimeGetTime);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.time_get_time_hook, "timeGetTime")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.time_get_time_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::TimeGetTime, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", tgt_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Multimedia timer, often used for audio/video timing.");
        }

        // GetSystemTime hook
        uint64_t gst_calls = display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::GetSystemTime);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.get_system_time_hook, "GetSystemTime")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.get_system_time_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::GetSystemTime, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", gst_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("System time in SYSTEMTIME format, used by some games for timestamps.");
        }

        // GetSystemTimeAsFileTime hook
        uint64_t gst_aft_calls = display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::GetSystemTimeAsFileTime);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.get_system_time_as_file_time_hook,
                                "GetSystemTimeAsFileTime")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.get_system_time_as_file_time_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::GetSystemTimeAsFileTime, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", gst_aft_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("System time in FILETIME format, used by some games for high-precision timestamps.");
        }

        // GetSystemTimePreciseAsFileTime hook
        uint64_t gstp_aft_calls =
            display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::GetSystemTimePreciseAsFileTime);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.get_system_time_precise_as_file_time_hook,
                                "GetSystemTimePreciseAsFileTime")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.get_system_time_precise_as_file_time_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::GetSystemTimePreciseAsFileTime, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", gstp_aft_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("High-precision system time (Windows 8+), used by modern games for precise timing.");
        }

        // GetLocalTime hook
        uint64_t glt_calls = display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::GetLocalTime);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.get_local_time_hook, "GetLocalTime")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.get_local_time_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::GetLocalTime, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", glt_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Local system time (vs UTC), used by some games for timezone-aware timing.");
        }

        // NtQuerySystemTime hook
        uint64_t ntqst_calls = display_commanderhooks::GetTimerHookCallCountById(display_commanderhooks::TimerHookIdentifier::NtQuerySystemTime);
        if (ComboSettingWrapper(settings::g_experimentalTabSettings.nt_query_system_time_hook, "NtQuerySystemTime")) {
            display_commanderhooks::TimerHookType type = static_cast<display_commanderhooks::TimerHookType>(
                settings::g_experimentalTabSettings.nt_query_system_time_hook.GetValue());
            display_commanderhooks::SetTimerHookTypeById(display_commanderhooks::TimerHookIdentifier::NtQuerySystemTime, type);
        }
        ImGui::SameLine();
        ImGui::Text("[%llu calls]", ntqst_calls);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Native API system time, used by some games for low-level timing access.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Show current settings summary
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Current Settings:");
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Time Multiplier: %.2fx",
                           settings::g_experimentalTabSettings.timeslowdown_multiplier.GetValue());
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Max Time Multiplier: %.0fx",
                           settings::g_experimentalTabSettings.timeslowdown_max_multiplier.GetValue());

        // Time comparison display
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Time Comparison:");

        // Get both real and spoofed time
        LONGLONG real_time_ns = utils::get_real_time_ns();
        LONGLONG spoofed_time_ns = utils::get_now_ns();
        LONGLONG time_difference_ns = spoofed_time_ns - real_time_ns;

        // Convert to more readable units
        double real_time_ms = real_time_ns / 1000000.0;
        double spoofed_time_ms = spoofed_time_ns / 1000000.0;
        double time_difference_ms = time_difference_ns / 1000000.0;

        // Calculate percentage difference
        double percentage_diff = 0.0;
        if (real_time_ns > 0) {
            percentage_diff = static_cast<double>(time_difference_ns) / real_time_ns * 100.0;
        }

        // Display the comparison
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "  Real Time: %.3f ms", real_time_ms);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.6f, 1.0f), "  Spoofed Time: %.3f ms", spoofed_time_ms);

        // Color code the difference based on magnitude
        ImVec4 diff_color;
        if (abs(percentage_diff) < 0.1) {
            diff_color = ImVec4(0.6f, 1.0f, 0.6f, 1.0f); // Green for minimal difference
        } else if (abs(percentage_diff) < 1.0) {
            diff_color = ImVec4(1.0f, 1.0f, 0.6f, 1.0f); // Yellow for small difference
        } else {
            diff_color = ImVec4(1.0f, 0.6f, 0.6f, 1.0f); // Red for significant difference
        }

        ImGui::TextColored(diff_color, "  Difference: %+.3f ms (%+.2f%%)", time_difference_ms, percentage_diff);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Shows the difference between real system time and time as seen by the game.\n"
                             "Positive values mean the game sees time as faster than real time.\n"
                             "Negative values mean the game sees time as slower than real time.");
        }

        // Show hook status
        bool hooks_installed = display_commanderhooks::AreTimeslowdownHooksInstalled();
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Hooks Status: %s",
                           hooks_installed ? "Installed" : "Not Installed");

        // Show current runtime values
        double current_multiplier = display_commanderhooks::GetTimeslowdownMultiplier();
        bool current_enabled = display_commanderhooks::IsTimeslowdownEnabled();
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Runtime Multiplier: %.2fx", current_multiplier);
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Runtime Enabled: %s", current_enabled ? "Yes" : "No");

        // Show active hooks
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Active Hooks:");
        const char *hook_names[] = {"QueryPerformanceCounter",
                                    "GetTickCount",
                                    "GetTickCount64",
                                    "timeGetTime",
                                    "GetSystemTime",
                                    "GetSystemTimeAsFileTime",
                                    "GetSystemTimePreciseAsFileTime",
                                    "GetLocalTime",
                                    "NtQuerySystemTime"};
        display_commanderhooks::TimerHookIdentifier hook_identifiers[] = {
            display_commanderhooks::TimerHookIdentifier::QueryPerformanceCounter,
            display_commanderhooks::TimerHookIdentifier::GetTickCount,
            display_commanderhooks::TimerHookIdentifier::GetTickCount64,
            display_commanderhooks::TimerHookIdentifier::TimeGetTime,
            display_commanderhooks::TimerHookIdentifier::GetSystemTime,
            display_commanderhooks::TimerHookIdentifier::GetSystemTimeAsFileTime,
            display_commanderhooks::TimerHookIdentifier::GetSystemTimePreciseAsFileTime,
            display_commanderhooks::TimerHookIdentifier::GetLocalTime,
            display_commanderhooks::TimerHookIdentifier::NtQuerySystemTime
        };

        for (int i = 0; i < 9; i++) {
            if (display_commanderhooks::IsTimerHookEnabledById(hook_identifiers[i])) {
                ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "    %s", hook_names[i]);
            }
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " WARNING: This affects all time-based game logic!");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Time slowdown affects all game systems that use the selected timer APIs for timing.");
        }
    }
    if (ImGui::CollapsingHeader("DLSS Indicator Controls", ImGuiTreeNodeFlags_None)) {
        DrawDlssIndicatorControls();
    }
}

void DrawD3D9FlipExControls() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Direct3D 9 FLIPEX Upgrade ===");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                       ICON_FK_WARNING " EXPERIMENTAL FEATURE - Upgrades D3D9 games to use FLIPEX swap effect!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This feature upgrades Direct3D 9 games to use the D3DSWAPEFFECT_FLIPEX swap effect.\n"
                         "FLIPEX leverages the Desktop Window Manager (DWM) for better performance on Windows Vista+.\n"
                         "Requirements:\n"
                         "  - Direct3D 9Ex support (Windows Vista or later)\n"
                         "  - Full-screen mode (not windowed)\n"
                         "  - At least 2 back buffers\n"
                         "  - Driver support for FLIPEX\n"
                         "\n"
                         "Benefits:\n"
                         "  - Reduced input latency\n"
                         "  - Better frame pacing\n"
                         "  - Improved performance in full-screen mode\n"
                         "\n"
                         "Note: Not all games and drivers support FLIPEX. If device creation fails,\n"
                         "disable this feature.");
    }

    ImGui::Spacing();

    // Enable/disable checkbox
    if (CheckboxSetting(settings::g_experimentalTabSettings.d3d9_flipex_enabled, "Enable D3D9 FLIPEX Upgrade")) {
        LogInfo("D3D9 FLIPEX upgrade %s",
                settings::g_experimentalTabSettings.d3d9_flipex_enabled.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable automatic upgrade of D3D9 games to use FLIPEX swap effect for better performance.\n"
                         "This feature requires the game to run in full-screen mode and support D3D9Ex.");
    }

    ImGui::Spacing();

    // Display current D3D9 state if applicable
    int current_api = g_last_reshade_device_api.load();
    uint32_t api_version = g_last_api_version.load();

    if (current_api == static_cast<int>(reshade::api::device_api::d3d9)) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Current Game API:");
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "  Direct3D 9");

        if (api_version == 0x9100) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "  API Version: Direct3D 9Ex (FLIPEX compatible)");
        } else if (api_version == 0x9000) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "  API Version: Direct3D 9 (Needs D3D9Ex upgrade)");
        } else {
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  API Version: 0x%x", api_version);
        }

        // Display current FlipEx state
        bool using_flipex = g_used_flipex.load();
        if (using_flipex) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "  Swap Effect: FLIPEX (Fast Flip)");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "  Swap Effect: Composite (Standard)");
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Current game is not using Direct3D 9");
    }

    ImGui::Spacing();

    // Information
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "How it works:");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "1. Enable the feature above");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "2. Restart the game");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "3. The addon will upgrade D3D9 to D3D9Ex if needed");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "4. The addon will modify swap effect to FLIPEX");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "5. Check the log file for upgrade status");

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " WARNING: If the game fails to start, disable this feature!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Some games and drivers don't support FLIPEX.\n"
                         "If you experience crashes or black screens, disable this feature.");
    }
}

void DrawDlssIndicatorControls() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== DLSS Indicator Controls ===");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                       ICON_FK_WARNING " EXPERIMENTAL FEATURE - Modifies NVIDIA registry settings!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This feature modifies the NVIDIA registry to enable/disable the DLSS indicator.\n"
                         "The indicator appears in the bottom left corner when enabled.\n"
                         "Requires administrator privileges to modify registry.");
    }

    ImGui::Spacing();

    // Current status display
    bool current_status = dlss::DlssIndicatorManager::IsDlssIndicatorEnabled();
    DWORD current_value = dlss::DlssIndicatorManager::GetDlssIndicatorValue();

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Current Status:");
    ImGui::TextColored(current_status ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
                       "  DLSS Indicator: %s", current_status ? "ENABLED" : "DISABLED");
    ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Registry Value: %lu (0x%lX)", current_value, current_value);
    ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Registry Path: HKEY_LOCAL_MACHINE\\%s",
                       dlss::DlssIndicatorManager::GetRegistryKeyPath().c_str());
    ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Value Name: %s",
                       dlss::DlssIndicatorManager::GetRegistryValueName().c_str());

    ImGui::Spacing();

    // Enable/disable checkbox
    if (CheckboxSetting(settings::g_experimentalTabSettings.dlss_indicator_enabled, "Enable DLSS Indicator")) {
        LogInfo("DLSS Indicator setting %s",
                settings::g_experimentalTabSettings.dlss_indicator_enabled.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable DLSS indicator in games. This modifies the NVIDIA registry.");
    }

    ImGui::Spacing();

    // Action buttons
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Registry Actions:");

    // Generate Enable .reg file button
    if (ImGui::Button("Generate Enable .reg File")) {
        std::string reg_content = dlss::DlssIndicatorManager::GenerateEnableRegFile();
        std::string filename = "dlss_indicator_enable.reg";

        if (dlss::DlssIndicatorManager::WriteRegFile(reg_content, filename)) {
            LogInfo("DLSS Indicator: Enable .reg file generated: %s", filename.c_str());
        } else {
            LogError("DLSS Indicator: Failed to generate enable .reg file");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Generate a .reg file to enable DLSS indicator.\n"
                         "The file will be created in the current directory.");
    }

    ImGui::SameLine();

    // Generate Disable .reg file button
    if (ImGui::Button("Generate Disable .reg File")) {
        std::string reg_content = dlss::DlssIndicatorManager::GenerateDisableRegFile();
        std::string filename = "dlss_indicator_disable.reg";

        if (dlss::DlssIndicatorManager::WriteRegFile(reg_content, filename)) {
            LogInfo("DLSS Indicator: Disable .reg file generated: %s", filename.c_str());
        } else {
            LogError("DLSS Indicator: Failed to generate disable .reg file");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Generate a .reg file to disable DLSS indicator.\n"
                         "The file will be created in the current directory.");
    }

    ImGui::SameLine();

    // Open folder button
    if (ImGui::Button("Open .reg Files Folder")) {
        // Get current working directory
        char current_dir[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, current_dir) != 0) {
            // Use ShellExecute to open the folder in Windows Explorer
            HINSTANCE result = ShellExecuteA(nullptr, "open", current_dir, nullptr, nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) <= 32) {
                LogError("DLSS Indicator: Failed to open folder, error: %ld",
                        reinterpret_cast<INT_PTR>(result));
            } else {
                LogInfo("DLSS Indicator: Opened folder: %s", current_dir);
            }
        } else {
            LogError("DLSS Indicator: Failed to get current directory");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open the folder containing the generated .reg files in Windows Explorer.");
    }

    ImGui::Spacing();

    // Instructions
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Instructions:");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "1. Generate the appropriate .reg file using the buttons above");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "2. Open the folder and double-click the .reg file to apply changes");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "3. Windows will prompt for administrator privileges when executing");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "4. Restart your game to see the DLSS indicator");
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "5. The indicator appears in the bottom left corner when enabled");

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " WARNING: Registry modifications require administrator privileges!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The registry modification requires administrator privileges.\n"
                         "Windows will prompt for elevation when executing .reg files.");
    }
}

void DrawDeveloperTools() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Developer Tools ===");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " EXPERIMENTAL FEATURE - For debugging purposes only!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("These tools are for developers and debugging purposes.\nUse with caution as they can cause crashes or unexpected behavior.");
    }

    ImGui::Spacing();

    // Debugger Trigger Button
    if (ImGui::Button("Trigger Debugger Break")) {
        LogInfo("Debugger break triggered by user");
        __debugbreak();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Triggers a debugger breakpoint. Useful for attaching a debugger at a specific moment.\nWARNING: Will crash if no debugger is attached!");
    }

    ImGui::SameLine();

    // Stack Trace Test Button
    if (ImGui::Button("Test Stack Trace")) {
        LogInfo("Stack trace test triggered by user");
        stack_trace::PrintStackTraceToDbgView();
        LogInfo("Stack trace printed to DbgView - check DebugView application");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Generates and prints a stack trace to DbgView.\nUseful for testing stack trace functionality.\nMake sure DebugView is running to see the output!");
    }

    ImGui::SameLine();

    // Test Crash Handler Button
    if (ImGui::Button("Test Crash Handler")) {
        LogInfo("Test crash handler triggered by user - this will cause an intentional crash!");
        // Trigger an intentional access violation to test our crash handler
        int* null_ptr = nullptr;
        *null_ptr = 42; // This will cause an access violation and trigger our UnhandledExceptionHandler
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Triggers an intentional crash to test the SetUnhandledExceptionFilter spoofing and crash logging system.\nWARNING: This will crash the application!\nUse this to verify that our exception handler is working correctly.");
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Note: Debugger break button will trigger a debugger breakpoint when clicked.");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Make sure you have a debugger attached before using the debugger break feature.");
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "WARNING: Crash Handler test will intentionally crash the application!");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Use it to test our SetUnhandledExceptionFilter spoofing and crash logging system.");
}

void DrawHIDSuppression() {
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "HID Suppression");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Suppress HID input reading for games to prevent them from detecting controllers.\nUseful for preventing games from interfering with controller input handling.");
    }

    // Master HID suppression enable
    if (CheckboxSetting(settings::g_experimentalTabSettings.hid_suppression_enabled, "Enable HID Suppression")) {
        LogInfo("HID suppression %s", settings::g_experimentalTabSettings.hid_suppression_enabled.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable/disable HID input suppression for games.");
    }

    // Direct control button
    ImGui::SameLine();
    bool current_state = settings::g_experimentalTabSettings.hid_suppression_enabled.GetValue();
    if (ImGui::Button("Toggle HID Suppression")) {
        renodx::hooks::SetHIDSuppressionEnabled(!current_state);
        LogInfo("HID suppression toggled via button: %s", !current_state ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Directly toggle HID suppression on/off using the SetHIDSuppressionEnabled function.");
    }

    if (settings::g_experimentalTabSettings.hid_suppression_enabled.GetValue()) {
        ImGui::Spacing();

        // DualSense only option
        if (CheckboxSetting(settings::g_experimentalTabSettings.hid_suppression_dualsense_only, "DualSense Only")) {
            LogInfo("HID suppression DualSense only %s", settings::g_experimentalTabSettings.hid_suppression_dualsense_only.GetValue() ? "enabled" : "disabled");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Only suppress DualSense controllers. If disabled, suppresses all HID devices.");
        }

        ImGui::Spacing();

        // Individual function blocking options
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Block Functions:");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select which HID functions to block for games.");
        }

        if (CheckboxSetting(settings::g_experimentalTabSettings.hid_suppression_block_readfile, "Block ReadFile")) {
            LogInfo("HID suppression ReadFile blocking %s", settings::g_experimentalTabSettings.hid_suppression_block_readfile.GetValue() ? "enabled" : "disabled");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Block ReadFile operations on potential HID devices.");
        }

        if (CheckboxSetting(settings::g_experimentalTabSettings.hid_suppression_block_getinputreport, "Block HidD_GetInputReport")) {
            LogInfo("HID suppression HidD_GetInputReport blocking %s", settings::g_experimentalTabSettings.hid_suppression_block_getinputreport.GetValue() ? "enabled" : "disabled");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Block HidD_GetInputReport operations for games.");
        }

        if (CheckboxSetting(settings::g_experimentalTabSettings.hid_suppression_block_getattributes, "Block HidD_GetAttributes")) {
            LogInfo("HID suppression HidD_GetAttributes blocking %s", settings::g_experimentalTabSettings.hid_suppression_block_getattributes.GetValue() ? "enabled" : "disabled");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Block HidD_GetAttributes operations to prevent device detection.");
        }

        if (CheckboxSetting(settings::g_experimentalTabSettings.hid_suppression_block_createfile, "Block CreateFile")) {
            LogInfo("HID suppression CreateFile blocking %s", settings::g_experimentalTabSettings.hid_suppression_block_createfile.GetValue() ? "enabled" : "disabled");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Block CreateFile operations on HID device paths (\\?\\hid#).");
        }

        ImGui::Spacing();

        // Show current settings summary
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Current Settings:");
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Target: %s", settings::g_experimentalTabSettings.hid_suppression_dualsense_only.GetValue() ? "DualSense Only" : "All HID Devices");
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  ReadFile: %s", settings::g_experimentalTabSettings.hid_suppression_block_readfile.GetValue() ? "Blocked" : "Allowed");
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  GetInputReport: %s", settings::g_experimentalTabSettings.hid_suppression_block_getinputreport.GetValue() ? "Blocked" : "Allowed");
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  GetAttributes: %s", settings::g_experimentalTabSettings.hid_suppression_block_getattributes.GetValue() ? "Blocked" : "Allowed");
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  CreateFile: %s", settings::g_experimentalTabSettings.hid_suppression_block_createfile.GetValue() ? "Blocked" : "Allowed");

        // Show hook status
        bool hooks_installed = renodx::hooks::AreHIDSuppressionHooksInstalled();
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "  Hooks Status: %s", hooks_installed ? "Installed" : "Not Installed");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " WARNING: This prevents games from reading HID input!");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("HID suppression prevents games from reading controller input directly.\nThis may cause games to not recognize controllers or behave unexpectedly.\nUse with caution and test thoroughly.");
        }
    }
}

void DrawDebugOutputHooks() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Debug Output Hooks ===");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " EXPERIMENTAL FEATURE - Hooks OutputDebugStringA/W to log to ReShade!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This feature hooks Windows debug output functions (OutputDebugStringA/W) and logs their output to the ReShade log file.\nUseful for debugging games that use debug output for logging or error reporting.");
    }

    ImGui::Spacing();

    // Log to ReShade setting
    if (CheckboxSetting(settings::g_experimentalTabSettings.debug_output_log_to_reshade, "Log to ReShade")) {
        LogInfo("Debug output logging to ReShade %s",
                settings::g_experimentalTabSettings.debug_output_log_to_reshade.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When enabled, debug output will be logged to ReShade.log.\nWhen disabled, debug output will only be passed through to the original functions.");
    }

    // Show statistics setting
    if (CheckboxSetting(settings::g_experimentalTabSettings.debug_output_show_stats, "Show Statistics")) {
        LogInfo("Debug output statistics display %s",
                settings::g_experimentalTabSettings.debug_output_show_stats.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Display statistics about captured debug output calls in the UI.");
    }

    // Show statistics if enabled
    if (settings::g_experimentalTabSettings.debug_output_show_stats.GetValue()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "=== Debug Output Statistics ===");

        auto& stats = display_commanderhooks::debug_output::GetDebugOutputStats();

        ImGui::Text("OutputDebugStringA calls: %llu",
                   static_cast<unsigned long long>(stats.output_debug_string_a_calls.load()));
        ImGui::Text("OutputDebugStringW calls: %llu",
                   static_cast<unsigned long long>(stats.output_debug_string_w_calls.load()));
        ImGui::Text("Total bytes logged: %llu",
                   static_cast<unsigned long long>(stats.total_bytes_logged.load()));

        // Reset statistics button
        if (ImGui::Button("Reset Statistics")) {
            stats.output_debug_string_a_calls.store(0);
            stats.output_debug_string_w_calls.store(0);
            stats.total_bytes_logged.store(0);
            LogInfo("Debug output statistics reset");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset all debug output statistics to zero.");
        }
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Note: This feature captures debug output from OutputDebugStringA and OutputDebugStringW calls.");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Debug output will appear in ReShade.log when enabled.");
}


} // namespace ui::new_ui
