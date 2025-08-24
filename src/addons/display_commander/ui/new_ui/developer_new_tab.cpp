#include "developer_new_tab.hpp"
#include "developer_new_tab_settings.hpp"
#include "settings_wrapper.hpp"
#include "../../addon.hpp"
#include "../../nvapi/nvapi_fullscreen_prevention.hpp"
#include "../../nvapi/nvapi_hdr_monitor.hpp"
#include "../ui_common.hpp"
#include "../../display_cache.hpp"
#include <sstream>
#include <iomanip>
#include <thread>

namespace ui::new_ui {

void InitDeveloperNewTab() {
    // Ensure settings are loaded
    static bool settings_loaded = false;
    if (!settings_loaded) {
        g_developerTabSettings.LoadAll();
        settings_loaded = true;
    }
    
}

void DrawDeveloperNewTab() {
    ImGui::Text("Developer Tab - Advanced Features");
    ImGui::Separator();
    
    // Developer Settings Section
    DrawDeveloperSettings();
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Sync Interval Settings Section
    DrawSyncIntervalSettings();
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // NVAPI Settings Section (merged with HDR and Colorspace Settings)
    DrawNvapiSettings();
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Reflex Settings Section
    DrawReflexSettings();
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Latency Display Section
    DrawLatencyDisplay();
}

void DrawDeveloperSettings() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "=== Developer Settings ===");
    
    // Enable unstable ReShade features (requires custom dxgi.dll)
    if (CheckboxSetting(g_developerTabSettings.enable_unstable_reshade_features, "Enable unstable ReShade features (requires custom dxgi.dll)")) {
        ::s_enable_unstable_reshade_features = g_developerTabSettings.enable_unstable_reshade_features.GetValue() ? 1.0f : 0.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable experimental features that rely on a custom dxgi.dll build. Off by default.");
    }
    
    // Prevent Fullscreen (global)
    if (CheckboxSetting(g_developerTabSettings.prevent_fullscreen, "Prevent Fullscreen")) {
        // Update global variable for compatibility
        s_prevent_fullscreen = g_developerTabSettings.prevent_fullscreen.GetValue() ? 1.0f : 0.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Prevent exclusive fullscreen; keep borderless/windowed for stability and HDR.");
    }
    
    // Spoof Fullscreen State
    const char* spoof_fullscreen_labels[] = {"Disabled", "Spoof as Fullscreen", "Spoof as Windowed"};
    int spoof_fullscreen_state = static_cast<int>(g_developerTabSettings.spoof_fullscreen_state.GetValue());
    if (ImGui::Combo("Spoof Fullscreen State", &spoof_fullscreen_state, spoof_fullscreen_labels, 3)) {
        g_developerTabSettings.spoof_fullscreen_state.SetValue(spoof_fullscreen_state != 0);
        s_spoof_fullscreen_state = static_cast<float>(spoof_fullscreen_state);
        
        // Log the change
        std::ostringstream oss;
        oss << "Fullscreen state spoofing changed to ";
        if (spoof_fullscreen_state < 0.5f) {
            oss << "Disabled";
        } else if (spoof_fullscreen_state < 1.5f) {
            oss << "Spoof as Fullscreen";
        } else {
            oss << "Spoof as Windowed";
        }
        extern void LogInfo(const char* message);
        ::LogInfo(oss.str().c_str());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Spoof fullscreen state detection for applications that query fullscreen status. Useful for games that change behavior based on fullscreen state.");
    }
    
    // Reset button for Fullscreen State (only show if not at default)
    if (s_spoof_fullscreen_state != 0.0f) {
        ImGui::SameLine();
        if (ImGui::Button("Reset##DevFullscreen")) {
            g_developerTabSettings.spoof_fullscreen_state.SetValue(false);
            s_spoof_fullscreen_state = 0.0f;
            extern void LogInfo(const char* message);
            ::LogInfo("Fullscreen state spoofing reset to disabled");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to default (Disabled)");
        }
    }
    
    // Spoof Window Focus
    const char* spoof_focus_labels[] = {"Disabled", "Spoof as Focused", "Spoof as Unfocused"};
    int spoof_focus_state = static_cast<int>(g_developerTabSettings.spoof_window_focus.GetValue());
    if (ImGui::Combo("Spoof Window Focus", &spoof_focus_state, spoof_focus_labels, 3)) {
        g_developerTabSettings.spoof_window_focus.SetValue(spoof_focus_state != 0);
        s_spoof_window_focus = static_cast<float>(spoof_focus_state);
        
        // Log the change
        std::ostringstream oss;
        oss << "Window focus spoofing changed to ";
        if (spoof_focus_state < 0.5f) {
            oss << "Disabled";
        } else if (spoof_focus_state < 1.5f) {
            oss << "Spoof as Focused";
        } else {
            oss << "Spoof as Unfocused";
        }
        extern void LogInfo(const char* message);
        ::LogInfo(oss.str().c_str());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Spoof window focus state for applications that query focus status. Useful for games that change behavior based on focus state.");
    }
    
    // Reset button for Window Focus (only show if not at default)
    if (s_spoof_window_focus != 0.0f) {
        ImGui::SameLine();
        if (ImGui::Button("Reset##DevFocus")) {
            g_developerTabSettings.spoof_window_focus.SetValue(false);
            s_spoof_window_focus = 0.0f;
            extern void LogInfo(const char* message);
            ::LogInfo("Window focus spoofing reset to disabled");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to default (Disabled)");
        }
    }
    
    ImGui::Spacing();
    
    // Continuous Monitoring moved to Main tab as 'Auto-apply'
    
    // Prevent Always On Top
    if (CheckboxSetting(g_developerTabSettings.prevent_always_on_top, "Prevent Always On Top")) {
        s_prevent_always_on_top = g_developerTabSettings.prevent_always_on_top.GetValue() ? 1.0f : 0.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Prevents windows from becoming always on top, even if they are moved or resized.");
    }
    
    // Remove Top Bar
    if (CheckboxSetting(g_developerTabSettings.remove_top_bar, "Remove Top Bar")) {
        s_remove_top_bar = g_developerTabSettings.remove_top_bar.GetValue() ? 1.0f : 0.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Remove the window title bar for a borderless appearance.");
    }
}

void DrawSyncIntervalSettings() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "=== Sync Interval Settings ===");
    
    // Sync Interval dropdown
    if (ComboSettingWrapper(g_developerTabSettings.sync_interval, "Sync Interval")) {
        s_sync_interval = static_cast<float>(g_developerTabSettings.sync_interval.GetValue());
        
        // Log the change
        std::ostringstream oss;
        oss << "Sync interval changed to ";
        switch (g_developerTabSettings.sync_interval.GetValue()) {
            case 0: oss << "Application-Controlled"; break;
            case 1: oss << "No-VSync (0)"; break;
            case 2: oss << "V-Sync (1)"; break;
            case 3: oss << "V-Sync 2x (2) incompatible with flip-model swap chains (extra latency)"; break;
            case 4: oss << "V-Sync 3x (3) incompatible with flip-model swap chains (extra latency)"; break;
            case 5: oss << "V-Sync 4x (4) incompatible with flip-model swap chains (extra latency)"; break;
            default: oss << "Unknown"; break;
        }
        extern void LogInfo(const char* message);
        ::LogInfo(oss.str().c_str());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Control the sync interval for frame presentation. This setting requires a game restart to take effect.");
    }
    
    // Information display
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Sync Interval Information:");
    
    // Get current monitor refresh rate for calculations
    int refresh_rate = 60; // Default fallback
    if (display_cache::g_displayCache.IsInitialized()) {
        display_cache::RationalRefreshRate refresh_rate_rational;
        if (display_cache::g_displayCache.GetCurrentRefreshRate(0, refresh_rate_rational)) {
            refresh_rate = static_cast<int>(refresh_rate_rational.ToHz());
        }
    }
    
    ImGui::BulletText("Application-Controlled: Uses game's default sync interval");
    ImGui::BulletText("No-VSync (0): Immediate presentation, potential tearing");
    ImGui::BulletText("V-Sync (1): Synchronized with monitor refresh rate (%d FPS on %dHz)", refresh_rate, refresh_rate);
    
    if (refresh_rate >= 120) {
        ImGui::BulletText("V-Sync 2x (2): Present every 2 refresh cycles (%d FPS on %dHz)", refresh_rate / 2, refresh_rate);
        ImGui::BulletText("V-Sync 3x (3): Present every 3 refresh cycles (%d FPS on %dHz)", refresh_rate / 3, refresh_rate);
        ImGui::BulletText("V-Sync 4x (4): Present every 4 refresh cycles (%d FPS on %dHz)", refresh_rate / 4, refresh_rate);
    } else if (refresh_rate >= 60) {
        ImGui::BulletText("V-Sync 2x (2): Present every 2 refresh cycles (%d FPS on %dHz)", refresh_rate / 2, refresh_rate);
        ImGui::BulletText("V-Sync 3x (3): Present every 3 refresh cycles (%d FPS on %dHz)", refresh_rate / 3, refresh_rate);
    } else {
        ImGui::BulletText("V-Sync 2x (2): Present every 2 refresh cycles (%d FPS on %dHz)", refresh_rate / 2, refresh_rate);
    }
    
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.8f, 1.0f), "Note: This setting requires a game restart to take effect.");
}

void DrawNvapiSettings() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "=== NVAPI Settings ===");
    
    // HDR10 Colorspace Fix
    if (CheckboxSetting(g_developerTabSettings.fix_hdr10_colorspace, "Fix NVAPI HDR10 Colorspace for reshade addon")) {
        s_fix_hdr10_colorspace = g_developerTabSettings.fix_hdr10_colorspace.GetValue() ? 1.0f : 0.0f;

        
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically fix HDR10 colorspace when swapchain format is RGB10A2 and colorspace is currently sRGB. Only works when the game is using sRGB colorspace.");
    }
    
    // NVAPI Fullscreen Prevention
    if (CheckboxSetting(g_developerTabSettings.nvapi_fullscreen_prevention, "NVAPI Fullscreen Prevention")) {
        s_nvapi_fullscreen_prevention = g_developerTabSettings.nvapi_fullscreen_prevention.GetValue() ? 1.0f : 0.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Use NVAPI to prevent fullscreen mode at the driver level.");
    }
    if (s_nvapi_fullscreen_prevention >= 0.5f && ::g_nvapiFullscreenPrevention.IsAvailable()) {
        
        // NVAPI HDR Logging
        if (CheckboxSetting(g_developerTabSettings.nvapi_hdr_logging, "NVAPI HDR Logging")) {
            s_nvapi_hdr_logging = g_developerTabSettings.nvapi_hdr_logging.GetValue() ? 1.0f : 0.0f;
            
            if (g_developerTabSettings.nvapi_hdr_logging.GetValue()) {
                std::thread(::RunBackgroundNvapiHdrMonitor).detach();
            }
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable HDR monitor information logging via NVAPI.");
        }
        
        // NVAPI HDR Interval
        if (SliderFloatSetting(g_developerTabSettings.nvapi_hdr_interval_sec, "HDR Logging Interval (seconds)", "%.1f")) {
            s_nvapi_hdr_interval_sec = g_developerTabSettings.nvapi_hdr_interval_sec.GetValue();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Interval between HDR monitor information logging.");
        }
        
        // NVAPI Force HDR10
        if (CheckboxSetting(g_developerTabSettings.nvapi_force_hdr10, "NVAPI Force HDR10")) {
            s_nvapi_force_hdr10 = g_developerTabSettings.nvapi_force_hdr10.GetValue() ? 1.0f : 0.0f;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Force HDR10 mode using NVAPI.");
        }
        if (g_developerTabSettings.nvapi_hdr_logging.GetValue()) {
        
            // NVAPI Debug Information Display
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "NVAPI Debug Information:");
            
            extern NVAPIFullscreenPrevention g_nvapiFullscreenPrevention;
        
            // Library loaded successfully
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ NVAPI Library: Loaded");
            
            // Driver version info
            std::string driverVersion = ::g_nvapiFullscreenPrevention.GetDriverVersion();
            if (driverVersion != "Failed to get driver version") {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Driver Version: %s", driverVersion.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ Driver Version: %s", driverVersion.c_str());
            }
            
            // Hardware detection
            if (::g_nvapiFullscreenPrevention.HasNVIDIAHardware()) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ NVIDIA Hardware: Detected");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ NVIDIA Hardware: Not Found");
            }
            
            // Fullscreen prevention status
            if (::g_nvapiFullscreenPrevention.IsFullscreenPreventionEnabled()) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Fullscreen Prevention: ACTIVE");
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ Fullscreen Prevention: Inactive");
            }
            
            // Function availability check
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Core Functions: Available");
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ DRS Functions: Available");
        }
    } else {
        // Library not loaded
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ NVAPI Library: Not Loaded");
        
        // Try to get error information
        std::string lastError = ::g_nvapiFullscreenPrevention.GetLastError();
        if (!lastError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Error: %s", lastError.c_str());
        }
        
        // Common failure reasons
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 0.8f), "Common Issues:");
        ImGui::BulletText("nvapi64.dll not found in system PATH");
        ImGui::BulletText("No NVIDIA drivers installed");
        ImGui::BulletText("Running on non-NVIDIA hardware");
        ImGui::BulletText("Insufficient permissions to load DLL");
        
        // Function availability (all unavailable when library not loaded)
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ Core Functions: Unavailable");
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ DRS Functions: Unavailable");
    }
    
    // Additional debug info
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Technical Details:");
    ImGui::BulletText("Uses dynamic loading of nvapi64.dll");
    ImGui::BulletText("Creates NVIDIA driver profiles per application");
    ImGui::BulletText("Sets OGL_DX_PRESENT_DEBUG_ID (0x20324987)");
    ImGui::BulletText("Applies DISABLE_FULLSCREEN_OPT (0x00000001)");
    ImGui::BulletText("Settings persist across application restarts");
    
    // DLL version info
    if (!::g_nvapiFullscreenPrevention.IsAvailable()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "DLL Information:");
        std::string dllInfo = ::g_nvapiFullscreenPrevention.GetDllVersionInfo();
        if (dllInfo != "No library loaded") {
            ImGui::TextWrapped("%s", dllInfo.c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "DLL not loaded - cannot get version info");
        }
    }
}

void DrawReflexSettings() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "=== Reflex Settings ===");
    
    // Enable NVIDIA Reflex
    if (CheckboxSetting(g_developerTabSettings.reflex_enabled, "Enable NVIDIA Reflex")) {
        s_reflex_enabled = g_developerTabSettings.reflex_enabled.GetValue() ? 1.0f : 0.0f;
        
        if (g_developerTabSettings.reflex_enabled.GetValue()) {
            extern void InstallReflexHooks();
            ::InstallReflexHooks();
        } else {
            extern void UninstallReflexHooks();
            ::UninstallReflexHooks();
        }
        
        // Mark that Reflex settings have changed to force sleep mode update
        extern std::atomic<bool> g_reflex_settings_changed;
        ::g_reflex_settings_changed.store(true);
    }
}

void DrawLatencyDisplay() {
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "=== Latency Information ===");
    
    // Current Latency Display
    extern std::atomic<float> g_current_latency_ms;
    float latency = ::g_current_latency_ms.load();
    
    std::ostringstream oss;
    oss << "Current Latency: " << std::fixed << std::setprecision(2) << latency << " ms";
    ImGui::TextUnformatted(oss.str().c_str());
    
    // PCL AV Latency Display (Most Important)
    extern std::atomic<float> g_pcl_av_latency_ms;
    float pcl_latency = ::g_pcl_av_latency_ms.load();
    
    oss.str("");
    oss.clear();
    oss << "PCL AV Latency: " << std::fixed << std::setprecision(2) << pcl_latency << " ms";
    ImGui::TextUnformatted(oss.str().c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "(30-frame avg)");
    
    // Reflex Status Display
    extern std::atomic<bool> g_reflex_active;
    extern std::atomic<uint64_t> g_current_frame;
    bool is_active = ::g_reflex_active.load();
    uint64_t frame = ::g_current_frame.load();
    
    oss.str("");
    oss.clear();
    if (is_active) {
        oss << "Reflex Status: Active (Frame " << frame << ")";
    } else {
        oss << "Reflex Status: Inactive";
    }
    ImGui::TextUnformatted(oss.str().c_str());
    
    // Debug Button to show atomic variable values
    if (ImGui::Button("Log All Latency Values")) {
        extern std::atomic<float> g_average_latency_ms;
        extern std::atomic<float> g_min_latency_ms;
        extern std::atomic<float> g_max_latency_ms;
        
        oss.str("");
        oss.clear();
        oss << "=== LATENCY DEBUG INFO ===" << std::endl;
        oss << "Current Latency: " << std::fixed << std::setprecision(2) << ::g_current_latency_ms.load() << " ms" << std::endl;
        oss << "PCL AV Latency: " << std::fixed << std::setprecision(2) << ::g_pcl_av_latency_ms.load() << " ms" << std::endl;
        oss << "Average Latency: " << std::fixed << std::setprecision(2) << ::g_average_latency_ms.load() << " ms" << std::endl;
        oss << "Min Latency: " << std::fixed << std::setprecision(2) << ::g_min_latency_ms.load() << " ms" << std::endl;
        oss << "Max Latency: " << std::fixed << std::setprecision(2) << ::g_max_latency_ms.load() << " ms" << std::endl;
        oss << "Current Frame: " << ::g_current_frame.load() << std::endl;
        oss << "Reflex Active: " << (::g_reflex_active.load() ? "Yes" : "No") << std::endl;
        oss << "========================";
        
        extern void LogInfo(const char* message);
        ::LogInfo(oss.str().c_str());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to log all current latency values for debugging");
    }
}

} // namespace ui::new_ui
