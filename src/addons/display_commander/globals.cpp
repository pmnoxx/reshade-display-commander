#include "globals.hpp"
#include "background_window.hpp"
#include "dxgi/custom_fps_limiter.hpp"
#include "latency/latency_manager.hpp"
#include "settings/developer_tab_settings.hpp"
#include "settings/experimental_tab_settings.hpp"
#include "settings/main_tab_settings.hpp"
#include "settings/swapchain_tab_settings.hpp"
#include "utils.hpp"
#include "utils/srwlock_wrapper.hpp"
#include <algorithm>
#include "../../../external/nvapi/nvapi.h"

#include <d3d11.h>
#include <reshade.hpp>
#include <wrl/client.h>

#include <atomic>
#include <array>

// Global variables
// UI mode removed - now using new tab system

// DLL initialization state - prevents DXGI calls during DllMain
std::atomic<bool> g_dll_initialization_complete{false};

// Module handle for pinning/unpinning
HMODULE g_hmodule = nullptr;

// Shared DXGI factory to avoid redundant CreateDXGIFactory calls
std::atomic<Microsoft::WRL::ComPtr<IDXGIFactory1> *> g_shared_dxgi_factory{nullptr};

// Window settings
std::atomic<WindowMode> s_window_mode{WindowMode::kFullscreen}; // kFullscreen = Borderless Fullscreen (default),
                                                                // kAspectRatio = Borderless Windowed (Aspect Ratio)

std::atomic<AspectRatioType> s_aspect_index{AspectRatioType::k16_9}; // Default to 16:9
std::atomic<int> s_aspect_width{0}; // 0 = Display Width, 1 = 3840, 2 = 2560, etc.

// Window alignment when repositioning is needed (0 = Center, 1 = Top Left, 2 = Top Right, 3 = Bottom Left, 4 = Bottom
// Right)
std::atomic<WindowAlignment> s_window_alignment{WindowAlignment::kCenter}; // default to center (slot 0)


// NVAPI Fullscreen Prevention
std::atomic<bool> s_nvapi_fullscreen_prevention{false}; // disabled by default


// Mouse position spoofing for auto-click sequences
std::atomic<bool> s_spoof_mouse_position{false}; // disabled by default
std::atomic<int> s_spoofed_mouse_x{0};
std::atomic<int> s_spoofed_mouse_y{0};

// Keyboard Shortcuts
std::atomic<bool> s_enable_mute_unmute_shortcut{true};
std::atomic<bool> s_enable_background_toggle_shortcut{true};
std::atomic<bool> s_enable_timeslowdown_shortcut{true};
std::atomic<bool> s_enable_adhd_toggle_shortcut{true};
std::atomic<bool> s_enable_autoclick_shortcut{false};

// Auto-click enabled state (atomic, not loaded from config)
std::atomic<bool> g_auto_click_enabled{false};

// Performance: background FPS cap

// VSync and tearing controls

// Monitor and display settings
std::atomic<DxgiBypassMode> s_dxgi_composition_state{DxgiBypassMode::kUnknown};

// Continue rendering in background
std::atomic<bool> s_continue_rendering{false}; // Disabled by default

// DirectInput hook suppression
std::atomic<bool> s_suppress_dinput_hooks{false}; // Disabled by default

// Input blocking in background (0.0f off, 1.0f on)

// Render blocking in background

// Present blocking in background

// Fix HDR10 color space when backbuffer is RGB10A2
std::atomic<bool> s_nvapi_fix_hdr10_colorspace{false};
std::atomic<bool> s_auto_colorspace{false};

// Hide HDR capabilities from applications
std::atomic<bool> s_hide_hdr_capabilities{false};
std::atomic<bool> s_enable_flip_chain{false};

// D3D9 to D3D9Ex upgrade
std::atomic<bool> s_enable_d3d9_upgrade{true}; // Enabled by default
std::atomic<bool> s_d3d9_upgrade_successful{false}; // Track if upgrade was successful
std::atomic<bool> g_used_flipex{false}; // Track if FLIPEX is currently being used

// ReShade runtimes for input blocking (multiple runtimes support)
std::vector<reshade::api::effect_runtime *> g_reshade_runtimes;
SRWLOCK g_reshade_runtimes_lock = SRWLOCK_INIT;

// Prevent always on top behavior
std::atomic<bool> s_prevent_always_on_top{true}; // Prevent games from staying on top by default

// Background feature - show black window behind game when not fullscreen

// Desktop Resolution Override
std::atomic<int> s_selected_monitor_index{0}; // Primary monitor by default

// Display Tab Enhanced Settings
std::atomic<int> s_selected_resolution_index{0};   // Default to first available resolution
std::atomic<int> s_selected_refresh_rate_index{0}; // Default to first available refresh rate

std::atomic<bool> s_initial_auto_selection_done{false}; // Track if we've done initial auto-selection

// Auto-restore resolution on game close
std::atomic<bool> s_auto_restore_resolution_on_close{true}; // Enabled by default

// Auto-apply resolution and refresh rate changes
std::atomic<bool> s_auto_apply_resolution_change{false};   // Disabled by default
std::atomic<bool> s_auto_apply_refresh_rate_change{false}; // Disabled by default

// Apply display settings at game start
std::atomic<bool> s_apply_display_settings_at_start{false}; // Disabled by default

// Track if resolution was successfully applied at least once
std::atomic<bool> s_resolution_applied_at_least_once{false}; // Disabled by default

// Atomic variables
std::atomic<int> g_comp_query_counter{0};
std::atomic<int> g_comp_last_logged{0};
std::atomic<void*> g_last_swapchain_ptr_unsafe{nullptr}; // TODO: unsafe remove later
std::atomic<int> g_last_reshade_device_api{0};
std::atomic<uint32_t> g_last_api_version{0};
std::atomic<std::shared_ptr<reshade::api::swapchain_desc>> g_last_swapchain_desc{nullptr};
std::atomic<uint64_t> g_init_apply_generation{0};
std::atomic<HWND> g_last_swapchain_hwnd{nullptr};
std::atomic<bool> g_shutdown{false};
std::atomic<bool> g_muted_applied{false};

// Continuous monitoring system
std::atomic<bool> s_continuous_monitoring_enabled{true}; // Enabled by default
std::atomic<bool> g_monitoring_thread_running{false};
std::thread g_monitoring_thread;

// Render thread tracking
std::atomic<DWORD> g_render_thread_id{0};

// Global window state instance
std::atomic<std::shared_ptr<GlobalWindowState>> g_window_state = std::make_shared<GlobalWindowState>();

// Global background window manager instance
BackgroundWindowManager g_backgroundWindowManager;

// Global Custom FPS Limiter Manager instance
namespace dxgi::fps_limiter {
std::unique_ptr<CustomFpsLimiter> g_customFpsLimiter = std::make_unique<CustomFpsLimiter>();
}

// Global Latent Sync Manager instance
namespace dxgi::latent_sync {
std::unique_ptr<LatentSyncManager> g_latentSyncManager = std::make_unique<LatentSyncManager>();
}

// Global DXGI Device Info Manager instance
// std::unique_ptr<DXGIDeviceInfoManager> g_dxgiDeviceInfoManager = std::make_unique<DXGIDeviceInfoManager>();

// Global Latency Manager instance
std::unique_ptr<LatencyManager> g_latencyManager = std::make_unique<LatencyManager>();

// Global Swapchain Tracking Manager instance
SwapchainTrackingManager g_swapchainTrackingManager;

// Backbuffer dimensions
std::atomic<int> g_last_backbuffer_width{0};
std::atomic<int> g_last_backbuffer_height{0};
// Background/foreground state (updated by monitoring thread)
std::atomic<bool> g_app_in_background{false};

// FPS limiter mode: 0 = Disabled, 1 = Reflex, 2 = OnPresentSync, 3 = OnPresentSyncLowLatency, 4 = VBlank Scanline Sync (VBlank)
std::atomic<FpsLimiterMode> s_fps_limiter_mode{FpsLimiterMode::kDisabled};

// FPS limiter injection timing: 0 = Default (Direct DX9/10/11/12), 1 = Fallback(1) (Through ReShade), 2 = Fallback(2) (Through ReShade)

// Scanline offset

// VBlank Sync Divisor (like VSync /2 /3 /4) - 0 to 8, default 1 (0 = off)

// Performance stats (FPS/frametime) shared state
std::atomic<uint32_t> g_perf_ring_head{0};
PerfSample g_perf_ring[kPerfRingCapacity] = {};
std::atomic<double> g_perf_time_seconds{0.0};
std::atomic<bool> g_perf_reset_requested{false};
std::atomic<std::shared_ptr<const std::string>> g_perf_text_shared{std::make_shared<const std::string>("")};

// Vector variables
std::atomic<std::shared_ptr<const std::vector<MonitorInfo>>> g_monitors{std::make_shared<std::vector<MonitorInfo>>()};

// Colorspace variables - removed, now queried directly in UI

// HDR10 override status (thread-safe, updated by background thread, read by UI thread)
// Use UpdateHdr10OverrideStatus() to update, or g_hdr10_override_status.load()->c_str() to read
std::atomic<std::shared_ptr<const std::string>> g_hdr10_override_status{std::make_shared<std::string>("Not applied")};

// HDR10 override timestamp (thread-safe, updated by background thread, read by UI thread)
// Use UpdateHdr10OverrideTimestamp() to update, or g_hdr10_override_timestamp.load()->c_str() to read
std::atomic<std::shared_ptr<const std::string>> g_hdr10_override_timestamp{std::make_shared<std::string>("Never")};

// Monitor labels cache removed - UI now uses GetDisplayInfoForUI() directly for better reliability

// Keyboard Shortcut Settings (moved to earlier in file)

// Performance optimization settings
std::atomic<LONGLONG> g_flush_before_present_time_ns{0};

// Helper function for updating HDR10 override status atomically
void UpdateHdr10OverrideStatus(const std::string &status) {
    g_hdr10_override_status.store(std::make_shared<std::string>(status));
}

// Helper function for updating HDR10 override timestamp atomically
void UpdateHdr10OverrideTimestamp(const std::string &timestamp) {
    g_hdr10_override_timestamp.store(std::make_shared<std::string>(timestamp));
}

// Helper function to get shared DXGI factory (thread-safe)
Microsoft::WRL::ComPtr<IDXGIFactory1> GetSharedDXGIFactory() {
    // Skip DXGI calls during DLL initialization to avoid loader lock violations
    if (!g_dll_initialization_complete.load()) {
        return nullptr;
    }

    // Check if factory already exists
    auto factory_ptr = g_shared_dxgi_factory.load();
    if (factory_ptr && *factory_ptr) {
        return *factory_ptr;
    }

    // Create new factory
    auto new_factory_ptr = std::make_unique<Microsoft::WRL::ComPtr<IDXGIFactory1>>();
    LogInfo("Creating shared DXGI factory");
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(new_factory_ptr->GetAddressOf()));
    if (FAILED(hr)) {
        LogWarn("Failed to create shared DXGI factory");
        return nullptr;
    }

    // Try to store the new factory atomically
    Microsoft::WRL::ComPtr<IDXGIFactory1> *expected = nullptr;
    if (g_shared_dxgi_factory.compare_exchange_strong(expected, new_factory_ptr.get())) {
        LogInfo("Shared DXGI factory created successfully");
        (void)new_factory_ptr.release(); // Don't delete, it's now managed by the atomic
        return *g_shared_dxgi_factory.load();
    } else {
        // Another thread created the factory first, use the existing one
        return *expected;
    }
}

// Helper function to get flip state based on API type
DxgiBypassMode GetFlipStateForAPI(int api) {
    // For D3D9, use FlipEx state instead of DXGI composition state
    if (api == static_cast<int>(reshade::api::device_api::d3d9)) {
        bool using_flipex = g_used_flipex.load();
        return using_flipex ? DxgiBypassMode::kIndependentFlip : DxgiBypassMode::kComposed;
    } else {
        return s_dxgi_composition_state.load();
    }
}

// Swapchain event counters - reset on each swapchain creation
std::array<std::atomic<uint32_t>, NUM_EVENTS> g_swapchain_event_counters = {}; // Array for all On* events

std::atomic<uint32_t> g_swapchain_event_total_count{0}; // Total events across all types

// Present pacing delay as percentage of frame time - 0% to 100%
// This adds a delay after present to improve frame pacing and reduce CPU usage
// Higher values create more consistent frame timing but may increase latency
// 0% = no delay, 100% = full frame time delay between simulation start and present

std::atomic<LONGLONG> late_amount_ns{0};

// GPU completion measurement using EnqueueSetEvent
std::atomic<HANDLE> g_gpu_completion_event{nullptr};  // Event handle for GPU completion measurement
std::atomic<LONGLONG> g_gpu_completion_time_ns{0};  // Last measured GPU completion time
std::atomic<LONGLONG> g_gpu_duration_ns{0};  // Last measured GPU duration (smoothed)

// GPU completion failure tracking
std::atomic<const char*> g_gpu_fence_failure_reason{nullptr};  // Reason why GPU fence creation/usage failed (nullptr if no failure)

// Sim-start-to-display latency measurement
std::atomic<LONGLONG> g_sim_start_ns_for_measurement{0};  // g_sim_start_ns captured when EnqueueGPUCompletion is called
std::atomic<bool> g_present_update_after2_called{false};  // Tracks if OnPresentUpdateAfter2 was called
std::atomic<bool> g_gpu_completion_callback_finished{false};  // Tracks if GPU completion callback finished
std::atomic<LONGLONG> g_sim_to_display_latency_ns{0};  // Measured sim-start-to-display latency (smoothed)

// GPU late time measurement (how much later GPU finishes compared to OnPresentUpdateAfter2)
std::atomic<LONGLONG> g_present_update_after2_time_ns{0};  // Time when OnPresentUpdateAfter2 was called
std::atomic<LONGLONG> g_gpu_completion_callback_time_ns{0};  // Time when GPU completion callback finished
std::atomic<LONGLONG> g_gpu_late_time_ns{0};  // GPU late time (0 if GPU finished first, otherwise difference)

// NVIDIA Reflex minimal controls (disabled by default)
std::atomic<bool> s_reflex_auto_configure{false}; // Disabled by default
std::atomic<bool> s_reflex_enable{false};
std::atomic<bool> s_reflex_enable_current_frame{false};
std::atomic<bool> s_reflex_low_latency{false};
std::atomic<bool> s_reflex_boost{false};
std::atomic<bool> s_reflex_use_markers{true};
std::atomic<bool> s_reflex_enable_sleep{false}; // Disabled by default
std::atomic<bool> s_enable_reflex_logging{false}; // Disabled by default


// DLLS-G (DLSS Frame Generation) status
std::atomic<bool> g_dlls_g_loaded{false};
std::atomic<std::shared_ptr<const std::string>> g_dlls_g_version{std::make_shared<const std::string>("Unknown")};

// NVAPI SetSleepMode tracking
std::atomic<std::shared_ptr<NV_SET_SLEEP_MODE_PARAMS>> g_last_nvapi_sleep_mode_params{nullptr};
std::atomic<IUnknown*> g_last_nvapi_sleep_mode_dev_ptr{nullptr};

// NVAPI Reflex timing tracking
std::atomic<LONGLONG> g_sleep_reflex_injected_ns{0};
std::atomic<LONGLONG> g_sleep_reflex_native_ns{0};

// Reflex debug counters
std::atomic<uint32_t> g_reflex_sleep_count{0};
std::atomic<uint32_t> g_reflex_apply_sleep_mode_count{0};
std::atomic<LONGLONG> g_reflex_sleep_duration_ns{0};

// Individual marker type counters
std::atomic<uint32_t> g_reflex_marker_simulation_start_count{0};
std::atomic<uint32_t> g_reflex_marker_simulation_end_count{0};
std::atomic<uint32_t> g_reflex_marker_rendersubmit_start_count{0};
std::atomic<uint32_t> g_reflex_marker_rendersubmit_end_count{0};
std::atomic<uint32_t> g_reflex_marker_present_start_count{0};
std::atomic<uint32_t> g_reflex_marker_present_end_count{0};
std::atomic<uint32_t> g_reflex_marker_input_sample_count{0};

// DX11 Proxy HWND for filtering
HWND g_proxy_hwnd = nullptr;


// Experimental tab settings global instance
namespace settings {
ExperimentalTabSettings g_experimentalTabSettings;
DeveloperTabSettings g_developerTabSettings;
MainTabSettings g_mainTabSettings;
SwapchainTabSettings g_swapchainTabSettings;
// Function to load all settings at startup
void LoadAllSettingsAtStartup() {
    g_developerTabSettings.LoadAll();
    g_experimentalTabSettings.LoadAll();
    g_mainTabSettings.LoadSettings();
    g_swapchainTabSettings.LoadAll();
    LogInfo("All settings loaded at startup");
}

} // namespace settings

// NGX Parameter Storage global instance
UnifiedParameterMap g_ngx_parameters;

// Get DLSS/DLSS-G summary from NGX parameters
DLSSGSummary GetDLSSGSummary() {
    DLSSGSummary summary;

    // Check if any NGX parameters exist (indicates DLSS is active)
    if (g_ngx_parameters.size() > 0) {
        summary.dlss_active = true;
    }

    // Check DLSS-G activity
    unsigned int is_recording;
    if (g_ngx_parameters.get_as_uint("DLSSG.IsRecording", is_recording)) {
        summary.dlss_g_active = (is_recording == 1);
    }

    // Get resolutions - using correct parameter names
    unsigned int internal_width, internal_height, output_width, output_height;
    bool has_internal_width = g_ngx_parameters.get_as_uint("DLSS.Render.Subrect.Dimensions.Width", internal_width);
    bool has_internal_height = g_ngx_parameters.get_as_uint("DLSS.Render.Subrect.Dimensions.Height", internal_height);
    bool has_output_width = g_ngx_parameters.get_as_uint("OutWidth", output_width);
    bool has_output_height = g_ngx_parameters.get_as_uint("OutHeight", output_height);

    if (has_internal_width && has_internal_height) {
        summary.internal_resolution = std::to_string(internal_width) + "x" + std::to_string(internal_height);
    }
    if (has_output_width && has_output_height) {
        summary.output_resolution = std::to_string(output_width) + "x" + std::to_string(output_height);
    }

    // Calculate scaling ratio
    if (has_internal_width && has_internal_height && has_output_width && has_output_height &&
        internal_width > 0 && internal_height > 0) {
        float scale_x = static_cast<float>(output_width) / internal_width;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2fx", scale_x);
        summary.scaling_ratio = std::string(buffer);
    }

    // Get quality preset (try to infer from available presets)
    unsigned int dummy_uint;
    if (g_ngx_parameters.get_as_uint("DLSS.Hint.Render.Preset.Quality", dummy_uint)) {
        summary.quality_preset = "Quality";
    } else if (g_ngx_parameters.get_as_uint("DLSS.Hint.Render.Preset.Balanced", dummy_uint)) {
        summary.quality_preset = "Balanced";
    } else if (g_ngx_parameters.get_as_uint("DLSS.Hint.Render.Preset.Performance", dummy_uint)) {
        summary.quality_preset = "Performance";
    } else if (g_ngx_parameters.get_as_uint("DLSS.Hint.Render.Preset.UltraPerformance", dummy_uint)) {
        summary.quality_preset = "Ultra Performance";
    } else if (g_ngx_parameters.get_as_uint("DLSS.Hint.Render.Preset.UltraQuality", dummy_uint)) {
        summary.quality_preset = "Ultra Quality";
    } else if (g_ngx_parameters.get_as_uint("DLSS.Hint.Render.Preset.DLAA", dummy_uint)) {
        summary.quality_preset = "DLAA";
    }

    // Get camera information
    float aspect_ratio;
    if (g_ngx_parameters.get_as_float("DLSSG.CameraAspectRatio", aspect_ratio)) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.4f", aspect_ratio);
        summary.aspect_ratio = std::string(buffer);
    }

    float fov;
    if (g_ngx_parameters.get_as_float("DLSSG.CameraFOV", fov)) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.4f", fov);
        summary.fov = std::string(buffer);
    }

    // Get jitter offset
    float jitter_x, jitter_y;
    bool has_jitter_x = g_ngx_parameters.get_as_float("DLSSG.JitterOffsetX", jitter_x);
    bool has_jitter_y = g_ngx_parameters.get_as_float("DLSSG.JitterOffsetY", jitter_y);
    if (!has_jitter_x) {
        has_jitter_x = g_ngx_parameters.get_as_float("Jitter.Offset.X", jitter_x);
    }
    if (!has_jitter_y) {
        has_jitter_y = g_ngx_parameters.get_as_float("Jitter.Offset.Y", jitter_y);
    }
    if (has_jitter_x && has_jitter_y) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.4f, %.4f", jitter_x, jitter_y);
        summary.jitter_offset = std::string(buffer);
    }

    // Get exposure information
    float pre_exposure, exposure_scale;
    bool has_pre_exposure = g_ngx_parameters.get_as_float("DLSS.Pre.Exposure", pre_exposure);
    bool has_exposure_scale = g_ngx_parameters.get_as_float("DLSS.Exposure.Scale", exposure_scale);
    if (has_pre_exposure && has_exposure_scale) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Pre: %.2f, Scale: %.2f", pre_exposure, exposure_scale);
        summary.exposure = std::string(buffer);
    }

    // Get depth inversion status
    int depth_inverted;
    if (g_ngx_parameters.get_as_int("DLSSG.DepthInverted", depth_inverted)) {
        summary.depth_inverted = (depth_inverted == 1) ? "Yes" : "No";
    }

    // Get HDR status
    int hdr_enabled;
    if (g_ngx_parameters.get_as_int("DLSSG.ColorBuffersHDR", hdr_enabled)) {
        summary.hdr_enabled = (hdr_enabled == 1) ? "Yes" : "No";
    }

    // Get motion vectors status
    int motion_included;
    if (g_ngx_parameters.get_as_int("DLSSG.CameraMotionIncluded", motion_included)) {
        summary.motion_vectors_included = (motion_included == 1) ? "Yes" : "No";
    }

    // Get frame time delta
    float frame_time;
    if (g_ngx_parameters.get_as_float("FrameTimeDeltaInMsec", frame_time)) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f ms", frame_time);
        summary.frame_time_delta = std::string(buffer);
    }

    // Get sharpness
    float sharpness;
    if (g_ngx_parameters.get_as_float("Sharpness", sharpness)) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.3f", sharpness);
        summary.sharpness = std::string(buffer);
    }

    // Get tonemapper type
    unsigned int tonemapper;
    if (g_ngx_parameters.get_as_uint("TonemapperType", tonemapper)) {
        summary.tonemapper_type = std::to_string(tonemapper);
    }

    // Get DLSS-G frame generation mode
    int enable_interp;
    if (g_ngx_parameters.get_as_int("DLSSG.EnableInterp", enable_interp)) {
        if (enable_interp == 1) {
            // DLSS-G is enabled, check MultiFrameCount for mode
            unsigned int multi_frame_count;
            if (g_ngx_parameters.get_as_uint("DLSSG.MultiFrameCount", multi_frame_count)) {
                if (multi_frame_count == 1) {
                    summary.fg_mode = "2x";
                } else if (multi_frame_count == 2) {
                    summary.fg_mode = "3x";
                } else if (multi_frame_count == 3) {
                    summary.fg_mode = "4x";
                } else {
                    char buffer[16];
                    snprintf(buffer, sizeof(buffer), "%dx", multi_frame_count + 1);
                    summary.fg_mode = std::string(buffer);
                }
            } else {
                summary.fg_mode = "Active (mode unknown)";
            }
        } else {
            summary.fg_mode = "Disabled";
        }
    } else {
        summary.fg_mode = "Unknown";
    }

    // Get NVIDIA Optical Flow Accelerator (OFA) status
    int ofa_enabled;
    if (g_ngx_parameters.get_as_int("Enable.OFA", ofa_enabled)) {
        summary.ofa_enabled = (ofa_enabled == 1) ? "Yes" : "No";
    }

    return summary;
}

// Helper functions for ReShade runtime management
void AddReShadeRuntime(reshade::api::effect_runtime* runtime) {
    if (runtime == nullptr) {
        return;
    }

    utils::SRWLockExclusive lock(g_reshade_runtimes_lock);

    // Check if runtime is already in the vector
    auto it = std::find(g_reshade_runtimes.begin(), g_reshade_runtimes.end(), runtime);
    if (it == g_reshade_runtimes.end()) {
        g_reshade_runtimes.push_back(runtime);
        LogInfo("Added ReShade runtime to vector - Total runtimes: %zu", g_reshade_runtimes.size());
    }
}

void RemoveReShadeRuntime(reshade::api::effect_runtime* runtime) {
    if (runtime == nullptr) {
        return;
    }

    utils::SRWLockExclusive lock(g_reshade_runtimes_lock);

    auto it = std::find(g_reshade_runtimes.begin(), g_reshade_runtimes.end(), runtime);
    if (it != g_reshade_runtimes.end()) {
        g_reshade_runtimes.erase(it);
        LogInfo("Removed ReShade runtime from vector - Total runtimes: %zu", g_reshade_runtimes.size());
    }
}

reshade::api::effect_runtime* GetFirstReShadeRuntime() {
    utils::SRWLockShared lock(g_reshade_runtimes_lock);

    if (g_reshade_runtimes.empty()) {
        return nullptr;
    }

    return g_reshade_runtimes[0];
}

std::vector<reshade::api::effect_runtime*> GetAllReShadeRuntimes() {
    utils::SRWLockShared lock(g_reshade_runtimes_lock);
    return g_reshade_runtimes;
}

size_t GetReShadeRuntimeCount() {
    utils::SRWLockShared lock(g_reshade_runtimes_lock);
    return g_reshade_runtimes.size();
}

// NGX preset initialization tracking
std::atomic<bool> g_ngx_presets_initialized{false};