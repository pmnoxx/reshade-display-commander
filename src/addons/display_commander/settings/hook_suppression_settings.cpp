#include "hook_suppression_settings.hpp"
#include "../utils/logging.hpp"

namespace settings {

HookSuppressionSettings::HookSuppressionSettings()
    : suppress_dxgi_hooks("DxgiHooks", false, "DisplayCommander.HookSuppression")
    , suppress_d3d_device_hooks("D3DDeviceHooks", false, "DisplayCommander.HookSuppression")
    , suppress_xinput_hooks("XInputHooks", false, "DisplayCommander.HookSuppression")
    , suppress_dinput_hooks("DInputHooks", false, "DisplayCommander.HookSuppression")
    , suppress_streamline_hooks("StreamlineHooks", false, "DisplayCommander.HookSuppression")
    , suppress_ngx_hooks("NGXHooks", false, "DisplayCommander.HookSuppression")
    , suppress_windows_gaming_input_hooks("WindowsGamingInputHooks", false, "DisplayCommander.HookSuppression")
    , suppress_hid_hooks("HidHooks", false, "DisplayCommander.HookSuppression")
    , suppress_api_hooks("ApiHooks", false, "DisplayCommander.HookSuppression")
    , suppress_sleep_hooks("SleepHooks", false, "DisplayCommander.HookSuppression")
    , suppress_timeslowdown_hooks("TimeslowdownHooks", false, "DisplayCommander.HookSuppression")
    , suppress_debug_output_hooks("DebugOutputHooks", false, "DisplayCommander.HookSuppression")
    , suppress_loadlibrary_hooks("LoadLibraryHooks", false, "DisplayCommander.HookSuppression")
    , suppress_display_settings_hooks("DisplaySettingsHooks", false, "DisplayCommander.HookSuppression")
    , suppress_windows_message_hooks("WindowsMessageHooks", false, "DisplayCommander.HookSuppression")
    , suppress_opengl_hooks("OpenGLHooks", false, "DisplayCommander.HookSuppression")
    , suppress_hid_suppression_hooks("HidSuppressionHooks", false, "DisplayCommander.HookSuppression")
    , suppress_nvapi_hooks("NvapiHooks", false, "DisplayCommander.HookSuppression")
    , suppress_process_exit_hooks("ProcessExitHooks", false, "DisplayCommander.HookSuppression")
    , dxgi_hooks_installed("DxgiHooks", false, "DisplayCommander.HooksInstalled")
    , d3d_device_hooks_installed("D3DDeviceHooks", false, "DisplayCommander.HooksInstalled")
    , xinput_hooks_installed("XInputHooks", false, "DisplayCommander.HooksInstalled")
    , dinput_hooks_installed("DInputHooks", false, "DisplayCommander.HooksInstalled")
    , streamline_hooks_installed("StreamlineHooks", false, "DisplayCommander.HooksInstalled")
    , ngx_hooks_installed("NGXHooks", false, "DisplayCommander.HooksInstalled")
    , windows_gaming_input_hooks_installed("WindowsGamingInputHooks", false, "DisplayCommander.HooksInstalled")
    , hid_hooks_installed("HidHooks", false, "DisplayCommander.HooksInstalled")
    , api_hooks_installed("ApiHooks", false, "DisplayCommander.HooksInstalled")
    , sleep_hooks_installed("SleepHooks", false, "DisplayCommander.HooksInstalled")
    , timeslowdown_hooks_installed("TimeslowdownHooks", false, "DisplayCommander.HooksInstalled")
    , debug_output_hooks_installed("DebugOutputHooks", false, "DisplayCommander.HooksInstalled")
    , loadlibrary_hooks_installed("LoadLibraryHooks", false, "DisplayCommander.HooksInstalled")
    , display_settings_hooks_installed("DisplaySettingsHooks", false, "DisplayCommander.HooksInstalled")
    , windows_message_hooks_installed("WindowsMessageHooks", false, "DisplayCommander.HooksInstalled")
    , opengl_hooks_installed("OpenGLHooks", false, "DisplayCommander.HooksInstalled")
    , hid_suppression_hooks_installed("HidSuppressionHooks", false, "DisplayCommander.HooksInstalled")
    , nvapi_hooks_installed("NvapiHooks", false, "DisplayCommander.HooksInstalled")
    , process_exit_hooks_installed("ProcessExitHooks", false, "DisplayCommander.HooksInstalled")
{
    // Initialize the all_settings_ vector
    all_settings_ = {
        &suppress_dxgi_hooks,
        &suppress_d3d_device_hooks,
        &suppress_xinput_hooks,
        &suppress_dinput_hooks,
        &suppress_streamline_hooks,
        &suppress_ngx_hooks,
        &suppress_windows_gaming_input_hooks,
        &suppress_hid_hooks,
        &suppress_api_hooks,
        &suppress_sleep_hooks,
        &suppress_timeslowdown_hooks,
        &suppress_debug_output_hooks,
        &suppress_loadlibrary_hooks,
        &suppress_display_settings_hooks,
        &suppress_windows_message_hooks,
        &suppress_opengl_hooks,
        &suppress_hid_suppression_hooks,
        &suppress_nvapi_hooks,
        &suppress_process_exit_hooks,
        &dxgi_hooks_installed,
        &d3d_device_hooks_installed,
        &xinput_hooks_installed,
        &dinput_hooks_installed,
        &streamline_hooks_installed,
        &ngx_hooks_installed,
        &windows_gaming_input_hooks_installed,
        &hid_hooks_installed,
        &api_hooks_installed,
        &sleep_hooks_installed,
        &timeslowdown_hooks_installed,
        &debug_output_hooks_installed,
        &loadlibrary_hooks_installed,
        &display_settings_hooks_installed,
        &windows_message_hooks_installed,
        &opengl_hooks_installed,
        &hid_suppression_hooks_installed,
        &nvapi_hooks_installed,
        &process_exit_hooks_installed,
    };
}

void HookSuppressionSettings::LoadAll() {
    LogInfo("HookSuppressionSettings::LoadAll() - Loading hook suppression settings");

    for (auto* setting : all_settings_) {
        setting->Load();
    }

    LogInfo("HookSuppressionSettings::LoadAll() - Hook suppression settings loaded");
}

std::vector<SettingBase *> HookSuppressionSettings::GetAllSettings() {
    return all_settings_;
}

} // namespace settings
