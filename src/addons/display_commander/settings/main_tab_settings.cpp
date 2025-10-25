#include "main_tab_settings.hpp"
#include "../adhd_multi_monitor/adhd_simple_api.hpp"
#include "../hooks/api_hooks.hpp"
#include "../utils.hpp"
#include "../utils/logging.hpp"

#include <windows.h>

// Atomic variables used by main tab settings
std::atomic<bool> s_background_feature_enabled{false}; // Disabled by default
std::atomic<int> s_scanline_offset{0};
std::atomic<int> s_vblank_sync_divisor{1};
std::atomic<float> s_fps_limit{0.f};
std::atomic<float> s_fps_limit_background{30.f};
std::atomic<float> s_present_pacing_delay_percentage{0.0f}; // Default to 0% (no delay)
std::atomic<bool> s_force_vsync_on{false};
std::atomic<bool> s_force_vsync_off{false};
std::atomic<bool> s_prevent_tearing{false};
std::atomic<float> s_audio_volume_percent{100.f};
std::atomic<bool> s_audio_mute{false};
std::atomic<bool> s_mute_in_background{false};
std::atomic<bool> s_mute_in_background_if_other_audio{false};
std::atomic<InputBlockingMode> s_keyboard_input_blocking{InputBlockingMode::kEnabledInBackground};
std::atomic<InputBlockingMode> s_mouse_input_blocking{InputBlockingMode::kEnabledInBackground};
std::atomic<InputBlockingMode> s_gamepad_input_blocking{InputBlockingMode::kDisabled};
std::atomic<bool> s_no_render_in_background{false};
std::atomic<bool> s_no_present_in_background{false};
std::atomic<bool> s_auto_apply_display_setting{true};  // Enabled by default
std::atomic<ScreensaverMode> s_screensaver_mode{ScreensaverMode::kDefault};
std::atomic<FrameTimeMode> s_frame_time_mode{FrameTimeMode::kPresent};

namespace settings {

MainTabSettings::MainTabSettings()
    : window_mode("window_mode", s_window_mode, static_cast<int>(WindowMode::kFullscreen), {"Borderless Fullscreen", "Borderless Windowed (Aspect Ratio)"}, "DisplayCommander"),
      aspect_index("aspect_index", 3, {"3:2", "4:3", "16:10", "16:9", "19:9", "19.5:9", "21:9", "32:9"},
                   "DisplayCommander"), // Default to 16:9
      window_aspect_width("aspect_width", s_aspect_width, 0, {"Display Width", "3840", "2560", "1920", "1600", "1280", "1080", "900", "720"}, "DisplayCommander"),
      background_feature("background_feature", s_background_feature_enabled, s_background_feature_enabled.load(), "DisplayCommander"),
      alignment("alignment", 0, {"Center", "Top Left", "Top Right", "Bottom Left", "Bottom Right"}, "DisplayCommander"),
      fps_limiter_mode("fps_limiter_mode", 0,
                       {"Disabled", "Reflex (low latency)", "Sync to Sim Start Time (adds latency to offer more consistent frame timing)", "Sync to Display Refresh Rate (fraction of monitor refresh rate)", "Non-Reflex Low Latency Mode (not implemented)"}, "DisplayCommander"),
      scanline_offset("scanline_offset", s_scanline_offset, 0, -1000, 1000, "DisplayCommander"),
      vblank_sync_divisor("vblank_sync_divisor", s_vblank_sync_divisor, 1, 0, 8, "DisplayCommander"),
      fps_limit("fps_limit", s_fps_limit, 0.0f, 0.0f, 240.0f, "DisplayCommander"),
      fps_limit_background("fps_limit_background", s_fps_limit_background, 30.0f, 0.0f, 240.0f, "DisplayCommander"),
      present_pacing_delay_percentage("present_pacing_delay_percentage", s_present_pacing_delay_percentage, 0.0f, 0.0f,
                                      100.0f, "DisplayCommander"),
      force_vsync_on("force_vsync_on", s_force_vsync_on, s_force_vsync_on.load(), "DisplayCommander"),
      force_vsync_off("force_vsync_off", s_force_vsync_off, s_force_vsync_off.load(), "DisplayCommander"),
      prevent_tearing("prevent_tearing", s_prevent_tearing, s_prevent_tearing.load(), "DisplayCommander"),
      audio_volume_percent("audio_volume_percent", s_audio_volume_percent, 100.0f, 0.0f, 100.0f, "DisplayCommander"),
      audio_mute("audio_mute", s_audio_mute, s_audio_mute.load(), "DisplayCommander"),
      mute_in_background("mute_in_background", s_mute_in_background, s_mute_in_background.load(), "DisplayCommander"),
      mute_in_background_if_other_audio("mute_in_background_if_other_audio", s_mute_in_background_if_other_audio, s_mute_in_background_if_other_audio.load(),
                                        "DisplayCommander"),
      audio_volume_auto_apply("audio_volume_auto_apply", true, "DisplayCommander"),
      keyboard_input_blocking("keyboard_input_blocking", s_keyboard_input_blocking, static_cast<int>(InputBlockingMode::kEnabledInBackground), {"Disabled", "Enabled", "Enabled (in background)"}, "DisplayCommander"),
      mouse_input_blocking("mouse_input_blocking", s_mouse_input_blocking, static_cast<int>(InputBlockingMode::kEnabledInBackground), {"Disabled", "Enabled", "Enabled (in background)"}, "DisplayCommander"),
      gamepad_input_blocking("gamepad_input_blocking", s_gamepad_input_blocking, static_cast<int>(InputBlockingMode::kDisabled), {"Disabled", "Enabled", "Enabled (in background)"}, "DisplayCommander"),
      no_render_in_background("no_render_in_background", s_no_render_in_background, s_no_render_in_background.load(), "DisplayCommander"),
      no_present_in_background("no_present_in_background", s_no_present_in_background, s_no_present_in_background.load(), "DisplayCommander"),
      show_test_overlay("show_test_overlay", false, "DisplayCommander"),
      gpu_measurement_enabled("gpu_measurement_enabled", 1, 0, 1, "DisplayCommander"),
      target_display("target_display", "", "DisplayCommander"),
      game_window_display_device_id("game_window_display_device_id", "", "DisplayCommander"),
      selected_extended_display_device_id("selected_extended_display_device_id", "", "DisplayCommander"),
      adhd_multi_monitor_enabled("adhd_multi_monitor_enabled", false, "DisplayCommander"),
      screensaver_mode("screensaver_mode", s_screensaver_mode, static_cast<int>(ScreensaverMode::kDefault),
                       {"Default (no change)", "Disable when Focused", "Disable"}, "DisplayCommander"),
      frame_time_mode("frame_time_mode", s_frame_time_mode, static_cast<int>(FrameTimeMode::kPresent),
                      {"Frame Present Time", "Frame Start Time (input)", "Frame Display Time later (Present or GPU Completion whichever comes later)"}, "DisplayCommander"),
      advanced_settings_enabled("advanced_settings_enabled", false, "DisplayCommander"),
      show_xinput_tab("show_xinput_tab", false, "DisplayCommander"),
      skip_ansel_loading("skip_ansel_loading", false, "DisplayCommander"),
      auto_apply_display_setting("auto_apply_display_setting", s_auto_apply_display_setting, s_auto_apply_display_setting.load(), "DisplayCommander") {

    // Initialize the all_settings_ vector
    all_settings_ = {
        &window_mode,
        &aspect_index,
        &window_aspect_width,
        &background_feature,
        &alignment,
        &fps_limiter_mode,
        &scanline_offset,
        &vblank_sync_divisor,
        &fps_limit,
        &fps_limit_background,
        &present_pacing_delay_percentage,
        &force_vsync_on,
        &force_vsync_off,
        &prevent_tearing,
        &audio_volume_percent,
        &audio_mute,
        &mute_in_background,
        &mute_in_background_if_other_audio,
        &audio_volume_auto_apply,
        &keyboard_input_blocking,
        &mouse_input_blocking,
        &gamepad_input_blocking,
        &no_render_in_background,
        &no_present_in_background,
        &show_test_overlay,
        &gpu_measurement_enabled,
        &frame_time_mode,
        &target_display,
        &game_window_display_device_id,
        &selected_extended_display_device_id,
        &adhd_multi_monitor_enabled,
        &screensaver_mode,
        &advanced_settings_enabled,
        &show_xinput_tab,
        &skip_ansel_loading,
        &auto_apply_display_setting,
    };
}

// TODO add initialization of other settings
void MainTabSettings::LoadSettings() {
    LogInfo("MainTabSettings::LoadSettings() called");
    LoadTabSettings(all_settings_);

    // Apply ADHD Multi-Monitor Mode settings after loading
    adhd_multi_monitor::api::SetEnabled(adhd_multi_monitor_enabled.GetValue());


    LogInfo("MainTabSettings::LoadSettings() completed");
}

std::vector<ui::new_ui::SettingBase *> MainTabSettings::GetAllSettings() { return all_settings_; }

// Helper function to convert wstring to string
std::string WStringToString(const std::wstring &wstr) {
    if (wstr.empty())
        return std::string();

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return std::string();

    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

// Function to get display device ID from a window
std::string GetDisplayDeviceIdFromWindow(HWND hwnd) {
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return "No Window";
    }

    // Get the monitor that contains the window
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (hmon == nullptr) {
        return "No Monitor";
    }

    // Use the display cache function to get the extended device ID
    return display_cache::g_displayCache.GetExtendedDeviceIdFromMonitor(hmon);
}

// Function to save the display device ID for the game window
void SaveGameWindowDisplayDeviceId(HWND hwnd) {
    std::string device_id = GetDisplayDeviceIdFromWindow(hwnd);
    settings::g_mainTabSettings.game_window_display_device_id.SetValue(device_id);

    std::ostringstream oss;
    oss << "Saved game window display device ID: " << device_id;
    LogInfo(oss.str().c_str());
}


// Function to update the target display setting with current game window
void UpdateTargetDisplayFromGameWindow() {
    // Get the game window from the API hooks
    HWND game_window = display_commanderhooks::GetGameWindow();

    std::string display_id = GetDisplayDeviceIdFromWindow(game_window);
    settings::g_mainTabSettings.target_display.SetValue(display_id);
}

void UpdateFpsLimitMaximums() {
    // Only update if display cache is initialized
    if (!display_cache::g_displayCache.IsInitialized()) {
        return;
    }

    // Get the maximum refresh rate across all monitors
    double max_refresh_rate = display_cache::g_displayCache.GetMaxRefreshRateAcrossAllMonitors();

    // Update the maximum values for FPS limit settings
    // Add some buffer (e.g., 10%) to allow for slightly higher FPS than max refresh rate
    float max_fps = max(60.f, static_cast<float>(max_refresh_rate));

    // Update the maximum values
    if (g_mainTabSettings.fps_limit.GetMax() != max_fps) {
        auto old_fps = g_mainTabSettings.fps_limit.GetMax();
        g_mainTabSettings.fps_limit.SetMax(max_fps);
        g_mainTabSettings.fps_limit_background.SetMax(max_fps);

        LogInfo("Updated FPS limit maximum %.1f->%.1f FPS (based on max monitor refresh rate of %.1f Hz)", old_fps, max_fps,
                max_refresh_rate);
    }
}

} // namespace settings
