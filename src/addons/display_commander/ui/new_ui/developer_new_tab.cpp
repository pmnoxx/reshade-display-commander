#include "developer_new_tab.hpp"
#include "../../globals.hpp"
#include "../../nvapi/fake_nvapi_manager.hpp"
#include "../../nvapi/nvapi_fullscreen_prevention.hpp"
#include "../../res/forkawesome.h"
#include "../../settings/developer_tab_settings.hpp"
#include "../../settings/experimental_tab_settings.hpp"
#include "../../utils/logging.hpp"
#include "../../utils/reshade_global_config.hpp"
#include "../../utils/general_utils.hpp"
#include "imgui.h"
#include "settings_wrapper.hpp"


#include <atomic>
#include <set>

#include <dxgi1_6.h>
#include <wrl/client.h>

// External atomic variables from settings
extern std::atomic<bool> s_nvapi_auto_enable_enabled;

namespace ui::new_ui {

void InitDeveloperNewTab() {
    // Ensure settings are loaded
    static bool settings_loaded = false;
    if (!settings_loaded) {
        // Settings already loaded at startup

        // Apply LoadFromDllMain setting to ReShade on startup
        utils::SetLoadFromDllMain(settings::g_developerTabSettings.load_from_dll_main.GetValue());

        settings_loaded = true;
    }
}

void DrawDeveloperNewTab() {
    if (ImGui::CollapsingHeader("Features Enabled By Default", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawFeaturesEnabledByDefault();
    }
    ImGui::Spacing();

    // Developer Settings Section
    if (ImGui::CollapsingHeader("Developer Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawDeveloperSettings();
    }

    ImGui::Spacing();

    // HDR and Display Settings Section
    if (ImGui::CollapsingHeader("HDR and Display Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawHdrDisplaySettings();
    }

    ImGui::Spacing();

    // NVAPI Settings Section
    if (ImGui::CollapsingHeader("NVAPI Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawNvapiSettings();
    }

    ImGui::Spacing();

    // Keyboard Shortcuts Section

    if (ImGui::CollapsingHeader("Keyboard Shortcuts", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawKeyboardShortcutsSettings();
    }

    ImGui::Spacing();

    // ReShade Global Config Section
    if (ImGui::CollapsingHeader("ReShade Global Config", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawReShadeGlobalConfigSettings();
    }

    ImGui::Spacing();
    ImGui::Separator();
}

void DrawFeaturesEnabledByDefault() {
    ImGui::Indent();

    // Prevent Fullscreen
    CheckboxSetting(settings::g_developerTabSettings.prevent_fullscreen, "Prevent Fullscreen");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Prevent exclusive fullscreen; keep borderless/windowed for stability and HDR.");
    }

    CheckboxSetting(settings::g_developerTabSettings.prevent_always_on_top, "Prevent Always On Top");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Prevents windows from becoming always on top, even if they are moved or resized.");
    }
#if 0
    // LoadFromDllMain setting
    if (CheckboxSetting(settings::g_developerTabSettings.load_from_dll_main, "Load DLL Early (ReShade) (requires restart)")) {
        LogInfo("LoadFromDllMain setting changed to: %s",
                settings::g_developerTabSettings.load_from_dll_main.GetValue() ? "enabled" : "disabled");
        // Apply the setting to ReShade immediately
        utils::SetLoadFromDllMain(settings::g_developerTabSettings.load_from_dll_main.GetValue());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Sets LoadFromDllMain=1 in ReShade configuration.\n"
            "This setting controls how ReShade loads and initializes.\n"
            "When enabled, ReShade will load from DllMain instead of the normal loading process.\n"
            "This setting requires a game restart to take effect.");
    }

    // Load Streamline setting
    if (CheckboxSetting(settings::g_developerTabSettings.load_streamline, "Hook Streamline SDK (sl.interposer.dll)")) {
        LogInfo("Load Streamline setting changed to: %s",
                settings::g_developerTabSettings.load_streamline.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Controls whether to load and hook into sl.interposer.dll (Streamline SDK).\n"
            "When enabled, Display Commander will install hooks for Streamline functions.\n"
            "This setting is automatically disabled when safemode is enabled.\n"
            "This setting requires a game restart to take effect.");
    }

    // Load _nvngx setting
    if (CheckboxSetting(settings::g_developerTabSettings.load_nvngx, "Hook NVIDIA NGX SDK (_nvngx.dll)")) {
        LogInfo("Load _nvngx setting changed to: %s",
                settings::g_developerTabSettings.load_nvngx.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Controls whether to load and hook into _nvngx.dll (NVIDIA NGX SDK).\n"
            "When enabled, Display Commander will install hooks for NGX functions.\n"
            "This setting is automatically disabled when safemode is enabled.\n"
            "This setting requires a game restart to take effect.");
    }

    // Load nvapi64 setting
    if (CheckboxSetting(settings::g_developerTabSettings.load_nvapi64, "Hook NVIDIA API (nvapi64.dll)")) {
        LogInfo("Load nvapi64 setting changed to: %s",
                settings::g_developerTabSettings.load_nvapi64.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Controls whether to load and hook into nvapi64.dll (NVIDIA API).\n"
            "When enabled, Display Commander will install hooks for NVAPI functions.\n"
            "This setting is automatically disabled when safemode is enabled.\n"
            "This setting requires a game restart to take effect.");
    }

    // Load XInput setting
    if (CheckboxSetting(settings::g_developerTabSettings.load_xinput, "Hook XInput (xinput*.dll)")) {
        LogInfo("Load XInput setting changed to: %s",
                settings::g_developerTabSettings.load_xinput.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Controls whether to load and hook into XInput libraries (xinput1_4.dll, xinput1_3.dll, etc.).\n"
            "When enabled, Display Commander will install hooks for XInput functions.\n"
            "This setting is automatically disabled when safemode is enabled.\n"
            "This setting requires a game restart to take effect.");
    }

    ImGui::Spacing();
    #endif

    ImGui::Unindent();
}

void DrawDeveloperSettings() {
    ImGui::Indent();

    // Continue Rendering
    if (CheckboxSetting(settings::g_developerTabSettings.continue_rendering, "Continue Rendering in Background")) {
        s_continue_rendering.store(settings::g_developerTabSettings.continue_rendering.GetValue());
        LogInfo("Continue rendering in background %s",
                settings::g_developerTabSettings.continue_rendering.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Prevent games from pausing or reducing performance when alt-tabbed. Blocks window focus "
            "messages to keep games running in background.");
    }

    // Safemode setting
    if (CheckboxSetting(settings::g_developerTabSettings.safemode, "Safemode (requires restart)")) {
        LogInfo("Safemode setting changed to: %s",
                settings::g_developerTabSettings.safemode.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Safemode disables all auto-apply settings and sets FPS limiter to disabled.\n"
            "When enabled, it will automatically set itself to 0 and disable:\n"
            "- Auto-apply resolution changes\n"
            "- Auto-apply refresh rate changes\n"
            "- Apply display settings at start\n"
            "- FPS limiter mode (set to disabled)\n\n"
            "This setting requires a game restart to take effect.");
    }

    // Suppress MinHook setting
    if (CheckboxSetting(settings::g_developerTabSettings.suppress_minhook, "Suppress MinHook Initialization")) {
        LogInfo("Suppress MinHook setting changed to: %s",
                settings::g_developerTabSettings.suppress_minhook.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Suppress all MinHook initialization calls (MH_Initialize).\n"
            "When enabled, all hook functions will skip MinHook initialization.\n"
            "This can help with compatibility issues or debugging.\n"
            "This setting is automatically enabled when safemode is active.\n\n"
            "This setting requires a game restart to take effect.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Debug Layer checkbox with warning
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), ICON_FK_WARNING);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "REQUIRES SETUP:");
    ImGui::SameLine();
    if (CheckboxSetting(settings::g_developerTabSettings.debug_layer_enabled, "Enable DX11/DX12 Debug Layer")) {
        LogInfo("Debug layer setting changed to: %s",
                settings::g_developerTabSettings.debug_layer_enabled.GetValue() ? "enabled" : "disabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            ICON_FK_WARNING " WARNING: Debug Layer Setup Required " ICON_FK_WARNING "\n\n"
            "REQUIREMENTS:\n"
            "- Windows 11 SDK must be installed\n"
            "- Download: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/\n"
            "- Install 'Graphics Tools' and 'Debugging Tools for Windows'\n\n"
            "SETUP STEPS:\n"
            "1. Install Windows 11 SDK with Graphics Tools\n"
            "2. Run DbgView.exe as Administrator\n"
            "3. Enable this setting\n"
            "4. RESTART THE GAME for changes to take effect\n\n"
            "FEATURES:\n"
            "- D3D11: Adds D3D11_CREATE_DEVICE_DEBUG flag\n"
            "- D3D12: Enables debug layer via D3D12GetDebugInterface\n"
            "- Breaks on all severity levels (ERROR, WARNING, INFO)\n"
            "- Debug output appears in DbgView\n\n"
            ICON_FK_WARNING " May significantly impact performance when enabled!");
    }

    // Show status when debug layer is enabled
    if (settings::g_developerTabSettings.debug_layer_enabled.GetValue()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ICON_FK_OK " ACTIVE");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Debug layer is currently ENABLED.\n"
                "- Debug output should appear in DbgView\n"
                "- Performance may be significantly reduced\n"
                "- Restart game if you just enabled this setting\n"
                "- Disable when not debugging to restore performance");
        }
    }

    // SetBreakOnSeverity checkbox (only shown when debug layer is enabled)
    if (settings::g_developerTabSettings.debug_layer_enabled.GetValue()) {
        ImGui::Indent();
        if (CheckboxSetting(settings::g_developerTabSettings.debug_break_on_severity, "SetBreakOnSeverity (All Levels)")) {
            LogInfo("Debug break on severity setting changed to: %s",
                    settings::g_developerTabSettings.debug_break_on_severity.GetValue() ? "enabled" : "disabled");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Enable SetBreakOnSeverity for all debug message levels.\n"
                "When enabled, the debugger will break on:\n"
                "- ERROR messages\n"
                "- CORRUPTION messages\n"
                "- WARNING messages\n"
                "- INFO messages\n"
                "- MESSAGE messages\n\n"
                "This setting only takes effect when debug layer is enabled.\n"
                "Requires a game restart to take effect.");
        }
        ImGui::Unindent();
    }

    ImGui::Unindent();
}

void DrawHdrDisplaySettings() {
    ImGui::Indent();

    // Hide HDR Capabilities
    if (CheckboxSetting(settings::g_developerTabSettings.hide_hdr_capabilities, "Hide game's native HDR")) {
        s_hide_hdr_capabilities.store(settings::g_developerTabSettings.hide_hdr_capabilities.GetValue());
        LogInfo("HDR hiding setting changed to: %s",
                settings::g_developerTabSettings.hide_hdr_capabilities.GetValue() ? "true" : "false");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Hides HDR capabilities from applications by intercepting CheckColorSpaceSupport and GetDesc calls.\n"
            "This can prevent games from detecting HDR support and force them to use SDR mode.");
    }

    // Enable Flip Chain
    if (CheckboxSetting(settings::g_developerTabSettings.enable_flip_chain, "Enable flip chain")) {
        s_enable_flip_chain.store(settings::g_developerTabSettings.enable_flip_chain.GetValue());
        LogInfo("Enable flip chain setting changed to: %s",
                settings::g_developerTabSettings.enable_flip_chain.GetValue() ? "true" : "false");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Forces games to use flip model swap chains (FLIP_DISCARD) for better performance.\n"
            "This setting requires a game restart to take effect.\n"
            "Only works with DirectX 10/11/12 (DXGI) games.");
    }

    // Auto Color Space checkbox
    bool auto_colorspace = settings::g_developerTabSettings.auto_colorspace.GetValue();
    if (ImGui::Checkbox("Auto color space", &auto_colorspace)) {
        settings::g_developerTabSettings.auto_colorspace.SetValue(auto_colorspace);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Automatically sets the appropriate color space on the game's swap chain based on the current format.\n"
            "- HDR10 format (R10G10B10A2) → HDR10 color space (ST2084)\n"
            "- FP16 format (R16G16B16A16) → scRGB color space (Linear)\n"
            "- SDR format (R8G8B8A8) → sRGB color space (Non-linear)\n"
            "Only works with DirectX 11/12 games.\n"
            "Applied automatically in presentBefore.");
    }




    // Show upgrade status
    if (s_d3d9e_upgrade_successful.load()) {
        ImGui::Indent();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ICON_FK_OK " D3D9 upgraded to D3D9Ex successfully");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Direct3D 9 was successfully upgraded to Direct3D 9Ex.\n"
                "Your game is now using the enhanced D3D9Ex API.");
        }
        ImGui::Unindent();
    } else if (settings::g_experimentalTabSettings.d3d9_flipex_enabled.GetValue()) {
        ImGui::Indent();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Waiting for D3D9 device creation...");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "The upgrade will occur when the game creates a Direct3D 9 device.\n"
                "If the game is not using D3D9, this setting has no effect.");
        }
        ImGui::Unindent();
    }

    ImGui::Unindent();
}

void DrawNvapiSettings() {
    ImGui::Indent();

    // NVAPI Auto-enable checkbox
    if (CheckboxSetting(settings::g_developerTabSettings.nvapi_auto_enable_enabled, "Enable NVAPI Auto-enable for Games")) {
        s_nvapi_auto_enable_enabled.store(settings::g_developerTabSettings.nvapi_auto_enable_enabled.GetValue());
        LogInfo("NVAPI Auto-enable setting changed to: %s",
                settings::g_developerTabSettings.nvapi_auto_enable_enabled.GetValue() ? "true" : "false");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically enable NVAPI features for supported games when they are launched.");
    }

    // Display current game status
    ImGui::Spacing();
    std::string gameStatus = GetNvapiAutoEnableGameStatus();
    bool isGameSupported = IsGameInNvapiAutoEnableList(GetCurrentProcessName());

    if (isGameSupported) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ICON_FK_OK " Current Game: %s", gameStatus.c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This game is supported for NVAPI auto-enable features.");
        }
        // Warning about Alt+Enter requirement
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FK_WARNING " Warning: Requires pressing Alt+Enter once");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Press Alt-Enter to enable HDR.\n"
                "This is required for proper HDR functionality.");
        }

    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), ICON_FK_CANCEL " Current Game: %s", gameStatus.c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This game is not in the NVAPI auto-enable supported games list.");
        }
    }

    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "NVAPI Auto-enable for Games");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Automatically enable NVAPI features for specific games.\n\n"
            "Note: DLDSR needs to be off for proper functionality\n\n"
            "Supported games:\n"
            "- Armored Core 6\n"
            "- Devil May Cry 5\n"
            "- Elden Ring\n"
            "- Hitman\n"
            "- Resident Evil 2\n"
            "- Resident Evil 3\n"
            "- Resident Evil 7\n"
            "- Resident Evil 8\n"
            "- Sekiro: Shadows Die Twice");
    }


    // Display restart warning if needed
    if (s_restart_needed_nvapi.load()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Game restart required to apply NVAPI changes.");
    }
    if (::g_nvapiFullscreenPrevention.IsAvailable()) {
        // Library loaded successfully
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ICON_FK_OK " NVAPI Library: Loaded");
    } else {
        // Library not loaded
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), ICON_FK_CANCEL " NVAPI Library: Not Loaded");
    }

    // Minimal NVIDIA Reflex Controls (device runtime dependent)
    if (ImGui::CollapsingHeader("NVIDIA Reflex (Minimal)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        // Native Reflex Status Indicator
        bool is_native_reflex_active = IsNativeReflexActive();
        if (is_native_reflex_active) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ICON_FK_OK " Native Reflex: ACTIVE Native Frame Pacing: ON");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "The game has native Reflex support and is actively using it. "
                    "Do not enable addon Reflex features to avoid conflicts.");
            }
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), ICON_FK_MINUS " Native Reflex: INACTIVE Native Frame Pacing: OFF");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "No native Reflex activity detected. "
                    "The game may not have Reflex support or it is disabled.");
            }
        }
        ImGui::Spacing();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Enabling Reflex when the game already has it can cause conflicts, instability, or "
                "performance issues. Check the game's graphics settings first.");
        }

        bool reflex_auto_configure = settings::g_developerTabSettings.reflex_auto_configure.GetValue();
        bool reflex_enable = settings::g_developerTabSettings.reflex_enable.GetValue();

        bool reflex_low_latency = settings::g_developerTabSettings.reflex_low_latency.GetValue();
        bool reflex_boost = settings::g_developerTabSettings.reflex_boost.GetValue();
        bool reflex_use_markers = settings::g_developerTabSettings.reflex_use_markers.GetValue();
        bool reflex_generate_markers = settings::g_developerTabSettings.reflex_generate_markers.GetValue();
        bool reflex_enable_sleep = settings::g_developerTabSettings.reflex_enable_sleep.GetValue();

        if (ImGui::Checkbox("Auto Configure Reflex", &reflex_auto_configure)) {
            settings::g_developerTabSettings.reflex_auto_configure.SetValue(reflex_auto_configure);
            s_reflex_auto_configure.store(reflex_auto_configure);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically configure Reflex settings on startup");
        }
        if (reflex_auto_configure) {
            ImGui::BeginDisabled();
            ImGui::Text("Auto-configure is handled by continuous monitoring");
        }

        if (ImGui::Checkbox("Enable Reflex", &reflex_enable)) {
            settings::g_developerTabSettings.reflex_enable.SetValue(reflex_enable);
            s_reflex_enable.store(reflex_enable);
        }
        if (reflex_auto_configure) {
            ImGui::EndDisabled();
            ImGui::Text("Auto-configure is handled by continuous monitoring");
        }

        if (reflex_enable) {
            if (ImGui::Checkbox("Low Latency Mode", &reflex_low_latency)) {
                settings::g_developerTabSettings.reflex_low_latency.SetValue(reflex_low_latency);
                s_reflex_low_latency.store(reflex_low_latency);
            }
            if (ImGui::Checkbox("Boost", &reflex_boost)) {
                settings::g_developerTabSettings.reflex_boost.SetValue(reflex_boost);
                s_reflex_boost.store(reflex_boost);
            }
            if (reflex_auto_configure) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Checkbox("Use Reflex Markers", &reflex_use_markers)) {
                settings::g_developerTabSettings.reflex_use_markers.SetValue(reflex_use_markers);
                s_reflex_use_markers.store(reflex_use_markers);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Tell NVIDIA Reflex to use markers for optimization");
            }

            if (ImGui::Checkbox("Generate Reflex Markers", &reflex_generate_markers)) {
                settings::g_developerTabSettings.reflex_generate_markers.SetValue(reflex_generate_markers);
                s_reflex_generate_markers.store(reflex_generate_markers);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Generate markers in the frame timeline for latency measurement");
            }
            // Warning about enabling Reflex when game already has it
            if (is_native_reflex_active && settings::g_developerTabSettings.reflex_generate_markers.GetValue()) {
                ImGui::SameLine();
                ImGui::TextColored(
                    ImVec4(1.0f, 0.6f, 0.0f, 1.0f), ICON_FK_WARNING
                    " Warning: Do not enable 'Generate Reflex Markers' if the game already has built-in Reflex support!");
            }

            if (ImGui::Checkbox("Enable Reflex Sleep Mode", &reflex_enable_sleep)) {
                settings::g_developerTabSettings.reflex_enable_sleep.SetValue(reflex_enable_sleep);
                s_reflex_enable_sleep.store(reflex_enable_sleep);
            }
            if (is_native_reflex_active && settings::g_developerTabSettings.reflex_enable_sleep.GetValue()) {
                ImGui::SameLine();
                ImGui::TextColored(
                    ImVec4(1.0f, 0.6f, 0.0f, 1.0f), ICON_FK_WARNING
                    " Warning: Do not enable 'Enable Reflex Sleep Mode' if the game already has built-in Reflex support!");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable Reflex sleep mode calls (disabled by default for safety).");
            }
            if (reflex_auto_configure) {
                ImGui::EndDisabled();
            }
            bool reflex_logging = settings::g_developerTabSettings.reflex_logging.GetValue();
            if (ImGui::Checkbox("Enable Reflex Logging", &reflex_logging)) {
                settings::g_developerTabSettings.reflex_logging.SetValue(reflex_logging);
                s_enable_reflex_logging.store(reflex_logging);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable detailed logging of Reflex marker operations for debugging purposes.");
            }
        }

        // Reflex Debug Counters Section
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Reflex Debug Counters", ImGuiTreeNodeFlags_DefaultOpen)) {
            extern std::atomic<uint32_t> g_reflex_sleep_count;
            extern std::atomic<uint32_t> g_reflex_apply_sleep_mode_count;
            extern std::atomic<LONGLONG> g_reflex_sleep_duration_ns;
            extern std::atomic<uint32_t> g_reflex_marker_simulation_start_count;
            extern std::atomic<uint32_t> g_reflex_marker_simulation_end_count;
            extern std::atomic<uint32_t> g_reflex_marker_rendersubmit_start_count;
            extern std::atomic<uint32_t> g_reflex_marker_rendersubmit_end_count;
            extern std::atomic<uint32_t> g_reflex_marker_present_start_count;
            extern std::atomic<uint32_t> g_reflex_marker_present_end_count;
            extern std::atomic<uint32_t> g_reflex_marker_input_sample_count;

            uint32_t sleep_count = ::g_reflex_sleep_count.load();
            uint32_t apply_sleep_mode_count = ::g_reflex_apply_sleep_mode_count.load();
            LONGLONG sleep_duration_ns = ::g_reflex_sleep_duration_ns.load();
            uint32_t sim_start_count = ::g_reflex_marker_simulation_start_count.load();
            uint32_t sim_end_count = ::g_reflex_marker_simulation_end_count.load();
            uint32_t render_start_count = ::g_reflex_marker_rendersubmit_start_count.load();
            uint32_t render_end_count = ::g_reflex_marker_rendersubmit_end_count.load();
            uint32_t present_start_count = ::g_reflex_marker_present_start_count.load();
            uint32_t present_end_count = ::g_reflex_marker_present_end_count.load();
            uint32_t input_sample_count = ::g_reflex_marker_input_sample_count.load();

            uint32_t total_marker_count = sim_start_count + sim_end_count + render_start_count + render_end_count
                                          + present_start_count + present_end_count + input_sample_count;

            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Reflex API Call Counters:");
            ImGui::Indent();
            ImGui::Text("Sleep calls: %u", sleep_count);
            if (sleep_count > 0) {
                double sleep_duration_ms = sleep_duration_ns / 1000000.0;
                ImGui::Text("Avg Sleep Duration: %.3f ms", sleep_duration_ms);
            }
            ImGui::Text("ApplySleepMode calls: %u", apply_sleep_mode_count);
            ImGui::Text("Total SetMarker calls: %u", total_marker_count);
            ImGui::Unindent();

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Individual Marker Type Counts:");
            ImGui::Indent();
            ImGui::Text("SIMULATION_START: %u", sim_start_count);
            ImGui::Text("SIMULATION_END: %u", sim_end_count);
            ImGui::Text("RENDERSUBMIT_START: %u", render_start_count);
            ImGui::Text("RENDERSUBMIT_END: %u", render_end_count);
            ImGui::Text("PRESENT_START: %u", present_start_count);
            ImGui::Text("PRESENT_END: %u", present_end_count);
            ImGui::Text("INPUT_SAMPLE: %u", input_sample_count);
            ImGui::Unindent();

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                               "These counters help debug Reflex FPS limiter issues in DX9 games.");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Marker counts show which specific markers are being set:\n"
                    "- SIMULATION_START/END: Frame simulation markers\n"
                    "- RENDERSUBMIT_START/END: GPU submission markers\n"
                    "- PRESENT_START/END: Present call markers\n"
                    "- INPUT_SAMPLE: Input sampling markers\n\n"
                    "If all marker counts are 0, Reflex markers are not being set.\n"
                    "If Sleep calls are 0, the Reflex sleep mode is not being called.\n"
                    "If ApplySleepMode calls are 0, the Reflex configuration is not being applied.");
            }

            if (ImGui::Button("Reset Counters")) {
                ::g_reflex_sleep_count.store(0);
                ::g_reflex_apply_sleep_mode_count.store(0);
                ::g_reflex_sleep_duration_ns.store(0);
                ::g_reflex_marker_simulation_start_count.store(0);
                ::g_reflex_marker_simulation_end_count.store(0);
                ::g_reflex_marker_rendersubmit_start_count.store(0);
                ::g_reflex_marker_rendersubmit_end_count.store(0);
                ::g_reflex_marker_present_start_count.store(0);
                ::g_reflex_marker_present_end_count.store(0);
                ::g_reflex_marker_input_sample_count.store(0);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Reset all Reflex debug counters to zero.");
            }
        }
        ImGui::Unindent();
    }

    // Fake NVAPI Settings
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("AntiLag 2 / XeLL support (fakenvapi / custom nvapi64.dll)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Fake NVAPI (Experimental)");

        bool fake_nvapi_enabled = settings::g_developerTabSettings.fake_nvapi_enabled.GetValue();
        if (ImGui::Checkbox("Enable custom nvapi64.dll loading / fakenvapi", &fake_nvapi_enabled)) {
            settings::g_developerTabSettings.fake_nvapi_enabled.SetValue(fake_nvapi_enabled);
            settings::g_developerTabSettings.fake_nvapi_enabled.Save();
            s_restart_needed_nvapi.store(true);
        }
         if (ImGui::IsItemHovered()) {
             ImGui::SetTooltip(
                 "Enable fake NVAPI to spoof NVIDIA detection on non-NVIDIA systems.\n"
                 "This allows DLSS and other NVIDIA features to work on AMD/Intel GPUs.\n\n"
                 "WARNING: This is experimental and may cause instability!\n"
                 "Requires nvapi64.dll or fakenvapi.dll to be placed next to the addon.\n"
                 "For newer optiscaler builds, use nvapi64.dll (rename fakenvapi.dll if needed).\n\n"
                 "Based on fakenvapi project: https://github.com/emoose/fakenvapi\n"
                 "Download from: https://github.com/optiscaler/fakenvapi/releases");
         }

        // Fake NVAPI Status
        auto stats = nvapi::g_fakeNvapiManager.GetStatistics();
        std::string status_msg = nvapi::g_fakeNvapiManager.GetStatusMessage();

        // Show warning if fakenvapi.dll is found (needs renaming)
        if (fake_nvapi_enabled && stats.fakenvapi_dll_found) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), ICON_FK_WARNING " Warning: fakenvapi.dll found - rename to nvapi64.dll");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "fakenvapi.dll was found in the addon directory.\n"
                    "For newer optiscaler builds, rename fakenvapi.dll to nvapi64.dll\n"
                    "to ensure proper functionality.");
            }
        }

        if (stats.is_nvapi64_loaded && !stats.fake_nvapi_loaded) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: nvapi64.dll was auto-loaded by the game.");
        } else if (stats.fake_nvapi_loaded) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: nvapi64.dll was loaded by DC from local directory.");
        } else if (!stats.last_error.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Status: %s", stats.last_error.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Status: %s", status_msg.c_str());
        }

    // Statistics
    if (ImGui::CollapsingHeader("Fake NVAPI Statistics", ImGuiTreeNodeFlags_None)) {
        ImGui::Text("nvapi64.dll loaded before DC: %s", stats.was_nvapi64_loaded_before_dc ? "Yes" : "No");
        ImGui::Text("nvapi64.dll currently loaded: %s", stats.is_nvapi64_loaded ? "Yes" : "No");
        ImGui::Text("libxell.dll loaded: %s", stats.is_libxell_loaded ? "Yes" : "No");
        ImGui::Text("Fake NVAPI Loaded: %s", stats.fake_nvapi_loaded ? "Yes" : "No");
        ImGui::Text("Override Enabled: %s", stats.override_enabled ? "Yes" : "No");

        if (stats.fakenvapi_dll_found) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), ICON_FK_WARNING ": fakenvapi.dll found: Yes (needs renaming to nvapi64.dll)");
        } else {
            ImGui::Text("fakenvapi.dll found: No");
        }

            if (!stats.last_error.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last Error: %s", stats.last_error.c_str());
            }
        }

        // Warning about experimental nature
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), ICON_FK_WARNING " Experimental Feature");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Fake NVAPI is experimental and may cause:\n"
                "- Game crashes or instability\n"
                "- Performance issues\n"
                "- Incompatibility with some games\n\n"
                "Use at your own risk!");
        }
        ImGui::Unindent();
    }

    ImGui::Unindent();
}

void DrawKeyboardShortcutsSettings() {
    ImGui::Indent();

    // Enable Hotkeys Master Toggle
    if (CheckboxSetting(settings::g_developerTabSettings.enable_hotkeys, "Enable Hotkeys")) {
        // Update the atomic variable when the setting changes
        s_enable_hotkeys.store(settings::g_developerTabSettings.enable_hotkeys.GetValue());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Master toggle for all keyboard shortcuts. When disabled, all hotkey settings below will be hidden and shortcuts will not work.");
    }

    // Only show individual hotkey settings if hotkeys are enabled
    if (settings::g_developerTabSettings.enable_hotkeys.GetValue()) {
        ImGui::Indent();

        // Enable Mute/Unmute Shortcut (Ctrl+M)
        if (CheckboxSetting(settings::g_developerTabSettings.enable_mute_unmute_shortcut,
                            "Enable Mute/Unmute Shortcut (Ctrl+M)")) {
            ::s_enable_mute_unmute_shortcut.store(settings::g_developerTabSettings.enable_mute_unmute_shortcut.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Enable keyboard shortcut Ctrl+M to quickly mute/unmute audio. Only works when the game is "
                "in the foreground.");
        }

        // Info text for Ctrl+M
        if (settings::g_developerTabSettings.enable_mute_unmute_shortcut.GetValue()) {
            ImGui::Indent();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Press Ctrl+M to toggle audio mute state");
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Shortcut works when game is in foreground");
            ImGui::Unindent();
        }

    // Enable Background Toggle Shortcut (Ctrl+R)
    if (CheckboxSetting(settings::g_developerTabSettings.enable_background_toggle_shortcut,
                        "Enable Background Toggle Shortcut (Ctrl+R)")) {
        ::s_enable_background_toggle_shortcut.store(
            settings::g_developerTabSettings.enable_background_toggle_shortcut.GetValue());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Enable keyboard shortcut Ctrl+R to quickly toggle both 'No Render in Background' and 'No "
            "Present in Background' settings. Only works when the game is in the foreground.");
    }

    // Info text for Ctrl+R
    if (settings::g_developerTabSettings.enable_background_toggle_shortcut.GetValue()) {
        ImGui::Indent();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                           "Press Ctrl+R to toggle background rendering/present settings");
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Shortcut works when game is in foreground");
        ImGui::Unindent();
    }

    // Enable Time Slowdown Shortcut (Ctrl+T)
    if (CheckboxSetting(settings::g_developerTabSettings.enable_timeslowdown_shortcut,
                        "Enable Time Slowdown Shortcut (Ctrl+T)")) {
        ::s_enable_timeslowdown_shortcut.store(
            settings::g_developerTabSettings.enable_timeslowdown_shortcut.GetValue());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Enable keyboard shortcut Ctrl+T to quickly toggle Time Slowdown. Only works when the game is "
            "in the foreground.");
    }

    // Info text for Ctrl+T
    if (settings::g_developerTabSettings.enable_timeslowdown_shortcut.GetValue()) {
        ImGui::Indent();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Press Ctrl+T to toggle Time Slowdown");
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Shortcut works when game is in foreground");
        ImGui::Unindent();
    }

        // Enable ADHD Toggle Shortcut (Ctrl+D)
        if (CheckboxSetting(settings::g_developerTabSettings.enable_adhd_toggle_shortcut,
                            "Enable ADHD Toggle Shortcut (Ctrl+D)")) {
            ::s_enable_adhd_toggle_shortcut.store(settings::g_developerTabSettings.enable_adhd_toggle_shortcut.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Enable keyboard shortcut Ctrl+D to quickly toggle ADHD Multi-Monitor Mode. Only works when the game is "
                "in the foreground.");
        }

        // Info text for Ctrl+D
        if (settings::g_developerTabSettings.enable_adhd_toggle_shortcut.GetValue()) {
            ImGui::Indent();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Press Ctrl+D to toggle ADHD Multi-Monitor Mode");
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Shortcut works when game is in foreground");
            ImGui::Unindent();
        }

        // Enable Input Blocking Shortcut (Ctrl+I)
        if (CheckboxSetting(settings::g_developerTabSettings.enable_input_blocking_shortcut,
                            "Enable Input Blocking Shortcut (Ctrl+I)")) {
            ::s_enable_input_blocking_shortcut.store(settings::g_developerTabSettings.enable_input_blocking_shortcut.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Enable keyboard shortcut Ctrl+I to quickly toggle input blocking. Only works when the game is "
                "in the foreground.");
        }

        // Info text for Ctrl+I
        if (settings::g_developerTabSettings.enable_input_blocking_shortcut.GetValue()) {
            ImGui::Indent();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Press Ctrl+I to toggle input blocking");
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Shortcut works when game is in foreground");
            ImGui::Unindent();
        }

        // Enable Auto-Click Shortcut (Ctrl+P)
        if (CheckboxSetting(settings::g_developerTabSettings.enable_autoclick_shortcut,
                            "Enable Auto-Click Shortcut (Ctrl+P)")) {
            ::s_enable_autoclick_shortcut.store(settings::g_developerTabSettings.enable_autoclick_shortcut.GetValue());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Enable keyboard shortcut Ctrl+P to quickly toggle Auto-Click sequences. Only works when the game is "
                "in the foreground.");
        }

        // Info text for Ctrl+P
        if (settings::g_developerTabSettings.enable_autoclick_shortcut.GetValue()) {
            ImGui::Indent();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Press Ctrl+P to toggle Auto-Click sequences");
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Shortcut works when game is in foreground");
            ImGui::Unindent();
        }

        ImGui::Unindent(); // Close the indentation for all hotkey settings
    }

    ImGui::Unindent();
}

void DrawReShadeGlobalConfigSettings() {
    ImGui::Indent();

    static utils::ReShadeGlobalSettings currentSettings;
    static utils::ReShadeGlobalSettings globalSettings;
    static bool initialLoadDone = false;
    static std::string statusMessage;
    static ImVec4 statusColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Auto-load settings on first run
    if (!initialLoadDone) {
        // Always load current settings
        utils::ReadCurrentReShadeSettings(currentSettings);

        // Try to load global settings (may not exist, which is fine)
        utils::LoadGlobalSettings(globalSettings);

        initialLoadDone = true;
        LogInfo("Auto-loaded ReShade settings for comparison");
    }

    ImGui::TextWrapped(
        "Manage global ReShade settings (EffectSearchPaths, TextureSearchPaths, keyboard shortcuts, etc.).");
    ImGui::TextWrapped("Copy settings between current game and global profile.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Display current ReShade.ini path info
    std::filesystem::path dcConfigPath = utils::GetDisplayCommanderConfigPath();
    std::string dcConfigPathStr = dcConfigPath.string();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Global profile location:");
    ImGui::Indent();
    ImGui::TextWrapped("%s", dcConfigPathStr.c_str());
    ImGui::Unindent();

    ImGui::Spacing();

    // Compare button
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Configuration comparison:");

    if (ImGui::Button("Compare local config vs global config")) {
        // Reload both settings for fresh comparison
        bool currentLoaded = utils::ReadCurrentReShadeSettings(currentSettings);
        bool globalLoaded = utils::LoadGlobalSettings(globalSettings);

        if (currentLoaded && globalLoaded) {
            statusMessage = ICON_FK_OK " Reloaded both configurations for comparison";
            statusColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            LogInfo("Reloaded both current and global settings for comparison");
        } else if (currentLoaded) {
            statusMessage = ICON_FK_WARNING " Reloaded current settings, global profile not found";
            statusColor = ImVec4(1.0f, 0.7f, 0.0f, 1.0f);
            LogInfo("Reloaded current settings, global profile not found");
        } else if (globalLoaded) {
            statusMessage = ICON_FK_WARNING " Reloaded global profile, current settings failed to load";
            statusColor = ImVec4(1.0f, 0.7f, 0.0f, 1.0f);
            LogInfo("Reloaded global settings, current settings failed to load");
        } else {
            statusMessage = ICON_FK_CANCEL " Failed to reload both configurations";
            statusColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            LogInfo("Failed to reload both configurations");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Reload and compare current game's ReShade settings with global profile\n(Useful if you edited either "
            "ReShade.ini or DisplayCommander.ini manually)");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Unified comparison view
    if (ImGui::CollapsingHeader("Configuration Comparison", ImGuiTreeNodeFlags_None)) {
        ImGui::TextWrapped("Shows differences between local (current game) and global configurations:");
        ImGui::Spacing();

        bool anyChanges = false;

        // Go through all sections in both settings
        std::set<std::string> allSections;
        for (const auto& [section, _] : currentSettings.additional_settings) {
            allSections.insert(section);
        }
        for (const auto& [section, _] : globalSettings.additional_settings) {
            allSections.insert(section);
        }

        for (const auto& section : allSections) {
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "[%s]", section.c_str());
            ImGui::Indent();

            auto currentSectionIt = currentSettings.additional_settings.find(section);
            auto globalSectionIt = globalSettings.additional_settings.find(section);

            // Get all keys in this section
            std::set<std::string> allKeys;
            if (currentSectionIt != currentSettings.additional_settings.end()) {
                for (const auto& [key, _] : currentSectionIt->second) {
                    allKeys.insert(key);
                }
            }
            if (globalSectionIt != globalSettings.additional_settings.end()) {
                for (const auto& [key, _] : globalSectionIt->second) {
                    allKeys.insert(key);
                }
            }

            bool sectionHasChanges = false;
            for (const auto& key : allKeys) {
                std::string currentValue;
                std::string globalValue;

                if (currentSectionIt != currentSettings.additional_settings.end()) {
                    auto keyIt = currentSectionIt->second.find(key);
                    if (keyIt != currentSectionIt->second.end()) {
                        currentValue = keyIt->second;
                    }
                }

                if (globalSectionIt != globalSettings.additional_settings.end()) {
                    auto keyIt = globalSectionIt->second.find(key);
                    if (keyIt != globalSectionIt->second.end()) {
                        globalValue = keyIt->second;
                    }
                }

                if (currentValue != globalValue) {
                    sectionHasChanges = true;
                    anyChanges = true;
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s:", key.c_str());
                    ImGui::Indent();

                    // Show both values side by side for better comparison
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Local:  ");
                    ImGui::SameLine();
                    if (currentValue.empty()) {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(empty)");
                    } else {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", currentValue.c_str());
                    }

                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Global: ");
                    ImGui::SameLine();
                    if (globalValue.empty()) {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(empty)");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "%s", globalValue.c_str());
                    }

                    ImGui::Unindent();
                }
            }

            if (!sectionHasChanges) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "No differences");
            }

            ImGui::Unindent();
            ImGui::Spacing();
        }

        if (!anyChanges) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "All settings are identical!");
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           "Legend: Local = Current game settings, Global = DisplayCommander.ini profile");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Action buttons
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.6f, 1.0f), "Actions:");
    ImGui::Spacing();

    // Apply current -> global
    if (ImGui::Button("Apply: Current -> Global")) {
        // Refresh current settings before saving
        utils::ReadCurrentReShadeSettings(currentSettings);

        if (utils::SaveGlobalSettings(currentSettings)) {
            statusMessage = ICON_FK_OK " Copied current settings to global profile";
            statusColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            LogInfo("Saved current settings to global profile");

            // Reload global settings to reflect changes
            utils::LoadGlobalSettings(globalSettings);
        } else {
            statusMessage = ICON_FK_CANCEL " Failed to save to global profile";
            statusColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            LogInfo("Failed to save to global profile");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Copy current game's ReShade settings to global profile\n(Overwrites DisplayCommander.ini)");
    }

    ImGui::SameLine();

    // Apply global -> current
    if (ImGui::Button("Apply: Global -> Current")) {
        // Refresh global settings before applying
        if (utils::LoadGlobalSettings(globalSettings)) {
            if (utils::WriteCurrentReShadeSettings(globalSettings)) {
                statusMessage = ICON_FK_OK " Applied global profile to current game";
                statusColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                LogInfo("Applied global settings to current ReShade.ini");

                // Reload current settings to reflect changes
                utils::ReadCurrentReShadeSettings(currentSettings);
            } else {
                statusMessage = ICON_FK_CANCEL " Failed to apply global settings";
                statusColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                LogInfo("Failed to apply global settings");
            }
        } else {
            statusMessage = ICON_FK_CANCEL " No global profile found (create one first)";
            statusColor = ImVec4(1.0f, 0.7f, 0.0f, 1.0f);
            LogInfo("No global settings file found");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Apply global profile to current game's ReShade settings\n(Overwrites current game's ReShade.ini)");
    }
    // warn requires pressing reload button on Home page in reshade for settings to be visible
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f),
                       "Warning: Requires pressing 'RELOAD' button on Home page in ReShade for settings to be visible");

    // Status message
    if (!statusMessage.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(statusColor, "%s", statusMessage.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // View current settings
    if (ImGui::TreeNode("View Current Game Settings")) {
        for (const auto& [section, keys_values] : currentSettings.additional_settings) {
            ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "[%s]", section.c_str());
            if (keys_values.empty()) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(empty)");
                ImGui::Unindent();
            } else {
                for (const auto& [key, value] : keys_values) {
                    ImGui::Indent();
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s:", key.c_str());
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s", value.c_str());
                    ImGui::Unindent();
                }
            }
            ImGui::Spacing();
        }

        ImGui::TreePop();
    }

    // View global settings
    if (ImGui::TreeNode("View Global Profile")) {
        if (globalSettings.additional_settings.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f),
                               "No global profile found. Create one using 'Apply: Current → Global'.");
        } else {
            for (const auto& [section, keys_values] : globalSettings.additional_settings) {
                ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "[%s]", section.c_str());
                if (keys_values.empty()) {
                    ImGui::Indent();
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(empty)");
                    ImGui::Unindent();
                } else {
                    for (const auto& [key, value] : keys_values) {
                        ImGui::Indent();
                        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s:", key.c_str());
                        ImGui::SameLine();
                        ImGui::TextWrapped("%s", value.c_str());
                        ImGui::Unindent();
                    }
                }
                ImGui::Spacing();
            }
        }

        ImGui::TreePop();
    }

    ImGui::Unindent();
}

}  // namespace ui::new_ui
