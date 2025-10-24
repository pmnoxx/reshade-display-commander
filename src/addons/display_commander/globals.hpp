#pragma once

#include "display_cache.hpp"
#include "dxgi/custom_fps_limiter.hpp"
#include "latent_sync/latent_sync_manager.hpp"
#include "utils/srwlock_wrapper.hpp"

#include <windows.h>

#include <d3d11.h>
#include <reshade_imgui.hpp>
#include <wrl/client.h>


#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>

// NVAPI types
#include "../../../external/nvapi/nvapi.h"

// Fake NVAPI manager
#include "nvapi/fake_nvapi_manager.hpp"

// Constants
#define DEBUG_LEVEL_0


enum class DeviceTypeDC {
    DX9,
    DX10,
    DX11,
    DX12,
    OpenGL
};

// Forward declarations

class SpinLock;
class BackgroundWindowManager;
class LatentSyncManager;
class LatencyManager;
class SwapchainTrackingManager;

// Unified parameter value that can hold multiple types
struct ParameterValue {
    enum Type { INT, UINT, FLOAT, DOUBLE, ULL };
    Type type;
    union {
        int int_val;
        unsigned int uint_val;
        float float_val;
        double double_val;
        uint64_t ull_val;
    };

    ParameterValue() : type(INT), int_val(0) {}
    ParameterValue(int val) : type(INT), int_val(val) {}
    ParameterValue(unsigned int val) : type(UINT), uint_val(val) {}
    ParameterValue(float val) : type(FLOAT), float_val(val) {}
    ParameterValue(double val) : type(DOUBLE), double_val(val) {}
    ParameterValue(uint64_t val) : type(ULL), ull_val(val) {}

    // Type conversion methods
    int get_as_int() const {
        switch (type) {
            case INT: return int_val;
            case UINT: return static_cast<int>(uint_val);
            case FLOAT: return static_cast<int>(float_val);
            case DOUBLE: return static_cast<int>(double_val);
            case ULL: return static_cast<int>(ull_val);
            default: return 0;
        }
    }

    unsigned int get_as_uint() const {
        switch (type) {
            case INT: return static_cast<unsigned int>(int_val);
            case UINT: return uint_val;
            case FLOAT: return static_cast<unsigned int>(float_val);
            case DOUBLE: return static_cast<unsigned int>(double_val);
            case ULL: return static_cast<unsigned int>(ull_val);
            default: return 0;
        }
    }

    float get_as_float() const {
        switch (type) {
            case INT: return static_cast<float>(int_val);
            case UINT: return static_cast<float>(uint_val);
            case FLOAT: return float_val;
            case DOUBLE: return static_cast<float>(double_val);
            case ULL: return static_cast<float>(ull_val);
            default: return 0.0f;
        }
    }

    double get_as_double() const {
        switch (type) {
            case INT: return static_cast<double>(int_val);
            case UINT: return static_cast<double>(uint_val);
            case FLOAT: return static_cast<double>(float_val);
            case DOUBLE: return double_val;
            case ULL: return static_cast<double>(ull_val);
            default: return 0.0;
        }
    }

    uint64_t get_as_ull() const {
        switch (type) {
            case INT: return static_cast<uint64_t>(int_val);
            case UINT: return static_cast<uint64_t>(uint_val);
            case FLOAT: return static_cast<uint64_t>(float_val);
            case DOUBLE: return static_cast<uint64_t>(double_val);
            case ULL: return ull_val;
            default: return 0;
        }
    }
};

// Unified atomic parameter storage system
class UnifiedParameterMap {
private:
    std::atomic<std::shared_ptr<std::unordered_map<std::string, ParameterValue>>> data_ = std::make_shared<std::unordered_map<std::string, ParameterValue>>();

public:
    UnifiedParameterMap() {}

    // Update parameter value (thread-safe)
    void update(const std::string& key, const ParameterValue& value) {
        // First, try to update existing data without copying
        {
            auto current_data = data_.load();
            if (current_data->find(key) != current_data->end()) {
                // Key exists, update in place (this is safe because we're only updating values, not structure)
                (*current_data)[key] = value;
                return;
            }
        }

        // Key doesn't exist, need to copy and update
        auto current_data = data_.load();
        auto new_data = std::make_shared<std::unordered_map<std::string, ParameterValue>>(*current_data);
        (*new_data)[key] = value;
        data_.store(new_data);
    }

    // Convenience update methods
    void update_int(const std::string& key, int value) { update(key, ParameterValue(value)); }
    void update_uint(const std::string& key, unsigned int value) { update(key, ParameterValue(value)); }
    void update_float(const std::string& key, float value) { update(key, ParameterValue(value)); }
    void update_double(const std::string& key, double value) { update(key, ParameterValue(value)); }
    void update_ull(const std::string& key, uint64_t value) { update(key, ParameterValue(value)); }

    // Get parameter value (thread-safe)
    bool get(const std::string& key, ParameterValue& value) const {
        auto current_data = data_.load();
        auto it = current_data->find(key);
        if (it != current_data->end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    // Type-specific get methods with conversion
    bool get_as_int(const std::string& key, int& value) const {
        ParameterValue param;
        if (get(key, param)) {
            value = param.get_as_int();
            return true;
        }
        return false;
    }

    bool get_as_uint(const std::string& key, unsigned int& value) const {
        ParameterValue param;
        if (get(key, param)) {
            value = param.get_as_uint();
            return true;
        }
        return false;
    }

    bool get_as_float(const std::string& key, float& value) const {
        ParameterValue param;
        if (get(key, param)) {
            value = param.get_as_float();
            return true;
        }
        return false;
    }

    bool get_as_double(const std::string& key, double& value) const {
        ParameterValue param;
        if (get(key, param)) {
            value = param.get_as_double();
            return true;
        }
        return false;
    }

    bool get_as_ull(const std::string& key, uint64_t& value) const {
        ParameterValue param;
        if (get(key, param)) {
            value = param.get_as_ull();
            return true;
        }
        return false;
    }

    // Get all parameters (thread-safe)
    std::shared_ptr<std::unordered_map<std::string, ParameterValue>> get_all() const {
        return data_.load();
    }

    // Get parameter count (thread-safe)
    size_t size() const {
        return data_.load()->size();
    }

    // Clear all parameters (thread-safe)
    void clear() {
        data_.store(std::make_shared<std::unordered_map<std::string, ParameterValue>>());
    }
};

// DLL initialization state
extern std::atomic<bool> g_dll_initialization_complete;

// Module handle for pinning/unpinning
extern HMODULE g_hmodule;

// Shared DXGI factory to avoid redundant CreateDXGIFactory calls
extern std::atomic<Microsoft::WRL::ComPtr<IDXGIFactory1>*> g_shared_dxgi_factory;

// Helper function to get shared DXGI factory (thread-safe)
Microsoft::WRL::ComPtr<IDXGIFactory1> GetSharedDXGIFactory();

// Enums
enum class DxgiBypassMode : std::uint8_t {
    kUnset,                    // Initial state, not yet queried
    kUnknown,                  // Query succeeded but unknown composition mode
    kComposed,                 // Composed presentation mode
    kOverlay,                  // Hardware overlay (MPO) presentation mode
    kIndependentFlip,          // Independent flip presentation mode
    kQueryFailedSwapchainNull, // Query failed: swapchain is null
    kQueryFailedNoSwapchain1,  // Query failed: IDXGISwapChain1 not available
    kQueryFailedNoMedia,       // Query failed: IDXGISwapChainMedia not available
    kQueryFailedNoStats        // Query failed: GetFrameStatisticsMedia failed
};
enum class WindowStyleMode : std::uint8_t { KEEP, BORDERLESS, OVERLAPPED_WINDOW };
enum class FpsLimiterMode : std::uint8_t { kDisabled = 0, kReflex = 1, kOnPresentSync = 2, kLatentSync = 3, kNonReflexLowLatency = 4 };
enum class WindowMode : std::uint8_t { kFullscreen = 0, kAspectRatio = 1 };
enum class AspectRatioType : std::uint8_t {
    k3_2 = 0,     // 3:2
    k4_3 = 1,     // 4:3
    k16_10 = 2,   // 16:10
    k16_9 = 3,    // 16:9
    k19_9 = 4,    // 19:9
    k19_5_9 = 5,  // 19.5:9
    k21_9 = 6,    // 21:9
    k32_9 = 7     // 32:9
};

enum class WindowAlignment : std::uint8_t {
    kCenter = 0,      // Center (default)
    kTopLeft = 1,     // Top Left
    kTopRight = 2,    // Top Right
    kBottomLeft = 3,  // Bottom Left
    kBottomRight = 4  // Bottom Right
};

enum class ScreensaverMode : std::uint8_t {
    kDefault = 0,             // Default (no change)
    kDisableWhenFocused = 1,  // Disable when focused
    kDisable = 2              // Disable
};

enum class InputBlockingMode : std::uint8_t {
    kDisabled = 0,              // Disabled
    kEnabled = 1,               // Always enabled
    kEnabledInBackground = 2    // Only enabled when in background
};

// Structures
struct GlobalWindowState {
    int desired_width = 0;
    int desired_height = 0;
    int target_x = 0;
    int target_y = 0;
    int target_w = 0;
    int target_h = 0;
    bool needs_resize = false;
    bool needs_move = false;
    bool style_changed = false;
    bool style_changed_ex = false;
    int current_style = 0;
    int current_ex_style = 0;
    int new_style = 0;
    int new_ex_style = 0;
    WindowStyleMode style_mode = WindowStyleMode::BORDERLESS;
    const char* reason = "unknown";

    int show_cmd = 0;
    int current_monitor_index = 0;
    display_cache::RationalRefreshRate current_monitor_refresh_rate;

    // Current display dimensions
    int display_width = 0;
    int display_height = 0;

    void reset() {
        desired_width = 0;
        desired_height = 0;
        target_x = 0;
        target_y = 0;
        target_w = 0;
        target_h = 0;
        needs_resize = false;
        needs_move = false;
        style_changed = false;
        style_changed_ex = false;
        style_mode = WindowStyleMode::BORDERLESS;
        reason = "unknown";
        current_monitor_index = 0;
        current_monitor_refresh_rate = display_cache::RationalRefreshRate();
        display_width = 0;
        display_height = 0;
    }
};

// Swapchain tracking manager for thread-safe swapchain management
class SwapchainTrackingManager {
private:
    std::unordered_set<IDXGISwapChain*> hooked_swapchains_;
    mutable SRWLOCK lock_;

public:
    SwapchainTrackingManager() : lock_(SRWLOCK_INIT) {}

    // Add a swapchain to the tracked set
    bool AddSwapchain(IDXGISwapChain* swapchain) {
        if (swapchain == nullptr) {
            return false;
        }

        utils::SRWLockExclusive lock(lock_);

        // Check if already tracked
        if (hooked_swapchains_.find(swapchain) != hooked_swapchains_.end()) {
            return false; // Already tracked
        }

        hooked_swapchains_.insert(swapchain);
        return true;
    }

    // Remove a swapchain from the tracked set
    bool RemoveSwapchain(IDXGISwapChain* swapchain) {
        if (swapchain == nullptr) {
            return false;
        }

        utils::SRWLockExclusive lock(lock_);

        auto it = hooked_swapchains_.find(swapchain);
        if (it != hooked_swapchains_.end()) {
            hooked_swapchains_.erase(it);
            return true;
        }

        return false; // Not found
    }

    // Check if a swapchain is being tracked
    bool IsSwapchainTracked(IDXGISwapChain* swapchain) const {
        if (swapchain == nullptr) {
            return false;
        }

        utils::SRWLockShared lock(lock_);
        return hooked_swapchains_.find(swapchain) != hooked_swapchains_.end();
    }

    // Get all tracked swapchains (returns a copy for thread safety)
    std::vector<IDXGISwapChain*> GetAllTrackedSwapchains() const {
        utils::SRWLockShared lock(lock_);
        return std::vector<IDXGISwapChain*>(hooked_swapchains_.begin(), hooked_swapchains_.end());
    }

    // Get the number of tracked swapchains
    size_t GetTrackedSwapchainCount() const {
        utils::SRWLockShared lock(lock_);
        return hooked_swapchains_.size();
    }

    // Clear all tracked swapchains
    void ClearAll() {
        utils::SRWLockExclusive lock(lock_);
        hooked_swapchains_.clear();
    }

    // Check if any swapchains are being tracked
    bool HasTrackedSwapchains() const {
        utils::SRWLockShared lock(lock_);
        return !hooked_swapchains_.empty();
    }
};

// Performance stats structure
struct PerfSample {
    double timestamp_seconds;
    float fps;
};

// Monitor info structure
struct MonitorInfo;

// External variable declarations - centralized here to avoid duplication

// Desktop Resolution Override
extern std::atomic<int> s_selected_monitor_index;

// Display Tab Enhanced Settings
extern std::atomic<int> s_selected_resolution_index;
extern std::atomic<int> s_selected_refresh_rate_index;
extern std::atomic<bool> s_initial_auto_selection_done;

// Auto-restore and auto-apply settings
extern std::atomic<bool> s_auto_restore_resolution_on_close;
extern std::atomic<bool> s_auto_apply_resolution_change;
extern std::atomic<bool> s_auto_apply_refresh_rate_change;
extern std::atomic<bool> s_apply_display_settings_at_start;
extern std::atomic<bool> s_resolution_applied_at_least_once;

// Window management
extern std::atomic<WindowMode> s_window_mode;
extern std::atomic<AspectRatioType> s_aspect_index;
extern std::atomic<int> s_aspect_width;


// Auto color space setting

// Hide HDR capabilities from applications

// D3D9 to D3D9Ex upgrade
//extern std::atomic<bool> s_enable_d3d9e_upgrade;
extern std::atomic<bool> s_d3d9e_upgrade_successful;
extern std::atomic<bool> g_used_flipex;

// Window Management Settings
extern std::atomic<WindowAlignment> s_window_alignment;  // Window alignment when repositioning is needed
extern std::atomic<DxgiBypassMode> s_dxgi_composition_state;

// Mouse position spoofing for auto-click sequences
extern std::atomic<bool> s_spoof_mouse_position;
extern std::atomic<int> s_spoofed_mouse_x;
extern std::atomic<int> s_spoofed_mouse_y;

// Render blocking in background

// Present blocking in background

// NVAPI Settings
extern std::atomic<bool> s_nvapi_auto_enable;

// Audio Settings

// Keyboard Shortcuts

// Auto-click enabled state (atomic, not loaded from config)
extern std::atomic<bool> g_auto_click_enabled;

// FPS Limiter Settings

// VSync and Tearing Controls

// ReShade Integration
extern std::vector<reshade::api::effect_runtime*> g_reshade_runtimes;
extern SRWLOCK g_reshade_runtimes_lock;
extern void (*g_custom_fps_limiter_callback)();

// ReShade runtime management functions
void AddReShadeRuntime(reshade::api::effect_runtime* runtime);
void RemoveReShadeRuntime(reshade::api::effect_runtime* runtime);
reshade::api::effect_runtime* GetFirstReShadeRuntime();
std::vector<reshade::api::effect_runtime*> GetAllReShadeRuntimes();
size_t GetReShadeRuntimeCount();

// Monitor Management
// g_monitor_labels removed - UI now uses GetDisplayInfoForUI() directly for better reliability

// Continuous monitoring and rendering

// Atomic variables
extern std::atomic<int> g_comp_query_counter;
extern std::atomic<DxgiBypassMode> g_comp_last_logged;
extern std::atomic<void*> g_last_swapchain_ptr_unsafe; // Using void* to avoid reshade dependency // TODO: unsafe remove later
extern std::atomic<int> g_last_reshade_device_api; // Store device API type
extern std::atomic<uint32_t> g_last_api_version; // Store API version/feature level (e.g., D3D_FEATURE_LEVEL_11_1)
extern std::atomic<std::shared_ptr<reshade::api::swapchain_desc>> g_last_swapchain_desc; // Store last swapchain description
extern std::atomic<uint64_t> g_init_apply_generation;
extern std::atomic<HWND> g_last_swapchain_hwnd;
extern std::atomic<bool> g_shutdown;
extern std::atomic<bool> g_muted_applied;

// Global instances
extern std::atomic<std::shared_ptr<GlobalWindowState>> g_window_state;
extern BackgroundWindowManager g_backgroundWindowManager;

// Custom FPS Limiter Manager
namespace dxgi::fps_limiter {
extern std::unique_ptr<CustomFpsLimiter> g_customFpsLimiter;
}

// Latent Sync Manager
namespace dxgi::latent_sync {
extern std::unique_ptr<LatentSyncManager> g_latentSyncManager;
}

// Latency Manager
extern std::unique_ptr<LatencyManager> g_latencyManager;

// Global frame ID for latency management
extern std::atomic<uint64_t> g_global_frame_id;

// Global Swapchain Tracking Manager instance
extern SwapchainTrackingManager g_swapchainTrackingManager;

// Present duration tracking
extern std::atomic<LONGLONG> g_present_duration_ns;

// Simulation duration tracking
extern std::atomic<LONGLONG> g_simulation_duration_ns;

// FPS limiter start duration tracking (nanoseconds)
extern std::atomic<LONGLONG> fps_sleep_before_on_present_ns;

// FPS limiter start duration tracking (nanoseconds)
extern std::atomic<LONGLONG> fps_sleep_after_on_present_ns;

// FPS limiter start duration tracking (nanoseconds)
extern std::atomic<LONGLONG> g_reshade_overhead_duration_ns;

// Render submit duration tracking (nanoseconds)
extern std::atomic<LONGLONG> g_render_submit_duration_ns;

// Render start time tracking
extern std::atomic<LONGLONG> g_submit_start_time_ns;

// Backbuffer dimensions
extern std::atomic<int> g_last_backbuffer_width;
extern std::atomic<int> g_last_backbuffer_height;

// Background/foreground state
extern std::atomic<bool> g_app_in_background;

// FPS limiter mode: 0 = Disabled, 1 = OnPresentSync, 2 = OnPresentSyncLowLatency, 3 = VBlank Scanline Sync (VBlank)
extern std::atomic<FpsLimiterMode> s_fps_limiter_mode;

// FPS limiter injection timing: 0 = Default (Direct DX9/10/11/12), 1 = Fallback(1) (Through ReShade), 2 = Fallback(2) (Through ReShade)
#define FPS_LIMITER_INJECTION_DEFAULT                0
#define FPS_LIMITER_INJECTION_FALLBACK1              1
#define FPS_LIMITER_INJECTION_FALLBACK2              2

// Performance stats (FPS/frametime) shared state
extern std::atomic<uint32_t> g_perf_ring_head;
extern PerfSample g_perf_ring[];
extern std::atomic<double> g_perf_time_seconds;
extern std::atomic<bool> g_perf_reset_requested;
extern std::atomic<std::shared_ptr<const std::string>> g_perf_text_shared;

// Lock-free ring buffer for recent FPS samples (60s window at ~240 Hz -> 14400 max)
constexpr size_t kPerfRingCapacity = 65536;

// Vector variables
extern std::atomic<std::shared_ptr<const std::vector<MonitorInfo>>> g_monitors;

// Colorspace variables - removed, now queried directly in UI
extern std::atomic<std::shared_ptr<const std::string>> g_hdr10_override_status;
extern std::atomic<std::shared_ptr<const std::string>> g_hdr10_override_timestamp;

// Helper function for updating HDR10 override status atomically
void UpdateHdr10OverrideStatus(const std::string& status);

// Helper function for updating HDR10 override timestamp atomically
void UpdateHdr10OverrideTimestamp(const std::string& timestamp);

// Helper function to get flip state based on API type
DxgiBypassMode GetFlipStateForAPI(int api);

// Performance optimization settings
extern std::atomic<LONGLONG> g_flush_before_present_time_ns;

// Sleep delay after present as percentage of frame time - 0% to 100%
extern std::atomic<float> s_sleep_after_present_frame_time_percentage;

// Monitoring thread
extern std::atomic<bool> g_monitoring_thread_running;
extern std::thread g_monitoring_thread;

// Render thread tracking
extern std::atomic<DWORD> g_render_thread_id;


// DirectInput hook suppression
extern std::atomic<bool> s_suppress_dinput_hooks;

// External declarations for atomic variables moved to developer_tab_settings.cpp
extern std::atomic<bool> s_continue_rendering;
extern std::atomic<bool> s_continuous_monitoring_enabled;
extern std::atomic<bool> s_hide_hdr_capabilities;
extern std::atomic<bool> s_enable_flip_chain;
extern std::atomic<bool> s_auto_colorspace;
extern std::atomic<bool> s_nvapi_fullscreen_prevention;

// Reflex settings
extern std::atomic<bool> s_reflex_auto_configure;
extern std::atomic<bool> s_reflex_enable;
extern std::atomic<bool> s_reflex_enable_current_frame;
extern std::atomic<bool> s_reflex_low_latency;
extern std::atomic<bool> s_reflex_boost;
extern std::atomic<bool> s_reflex_use_markers;
extern std::atomic<bool> s_reflex_generate_markers;
extern std::atomic<bool> s_reflex_enable_sleep;
extern std::atomic<bool> s_reflex_supress_native;
extern std::atomic<bool> s_enable_reflex_logging;

// Shortcut settings
extern std::atomic<bool> s_enable_mute_unmute_shortcut;
extern std::atomic<bool> s_enable_background_toggle_shortcut;
extern std::atomic<bool> s_enable_timeslowdown_shortcut;
extern std::atomic<bool> s_enable_adhd_toggle_shortcut;
extern std::atomic<bool> s_enable_autoclick_shortcut;

// Forward declaration for tab settings
namespace settings {
class ExperimentalTabSettings;
class DeveloperTabSettings;
class MainTabSettings;
class SwapchainTabSettings;
class StreamlineTabSettings;
extern ExperimentalTabSettings g_experimentalTabSettings;
extern DeveloperTabSettings g_developerTabSettings;
extern MainTabSettings g_mainTabSettings;
extern SwapchainTabSettings g_swapchainTabSettings;
extern StreamlineTabSettings g_streamlineTabSettings;

// Function to load all settings at startup
void LoadAllSettingsAtStartup();
}  // namespace settings

// OpenGL hook counter indices
enum OpenGLHookIndex {
    OPENGL_HOOK_WGL_SWAPBUFFERS,
    OPENGL_HOOK_WGL_MAKECURRENT,
    OPENGL_HOOK_WGL_CREATECONTEXT,
    OPENGL_HOOK_WGL_DELETECONTEXT,
    OPENGL_HOOK_WGL_CHOOSEPIXELFORMAT,
    OPENGL_HOOK_WGL_SETPIXELFORMAT,
    OPENGL_HOOK_WGL_GETPIXELFORMAT,
    OPENGL_HOOK_WGL_DESCRIBEPIXELFORMAT,
    OPENGL_HOOK_WGL_CREATECONTEXTATTRIBSARB,
    OPENGL_HOOK_WGL_CHOOSEPIXELFORMATARB,
    OPENGL_HOOK_WGL_GETPIXELFORMATATTRIBIVARB,
    OPENGL_HOOK_WGL_GETPIXELFORMATATTRIBFVARB,
    OPENGL_HOOK_WGL_GETPROCADDRESS,
    OPENGL_HOOK_WGL_SWAPINTERVALEXT,
    OPENGL_HOOK_WGL_GETSWAPINTERVALEXT,
    NUM_OPENGL_HOOKS
};

// Display settings hook counter indices
enum DisplaySettingsHookIndex {
    DISPLAY_SETTINGS_HOOK_CHANGEDISPLAYSETTINGSA,
    DISPLAY_SETTINGS_HOOK_CHANGEDISPLAYSETTINGSW,
    DISPLAY_SETTINGS_HOOK_CHANGEDISPLAYSETTINGSEXA,
    DISPLAY_SETTINGS_HOOK_CHANGEDISPLAYSETTINGSEXW,
    // Window management hooks for OpenGL fullscreen prevention
    DISPLAY_SETTINGS_HOOK_SETWINDOWPOS,
    DISPLAY_SETTINGS_HOOK_SHOWWINDOW,
    DISPLAY_SETTINGS_HOOK_SETWINDOWLONGA,
    DISPLAY_SETTINGS_HOOK_SETWINDOWLONGW,
    DISPLAY_SETTINGS_HOOK_SETWINDOWLONGPTRA,
    DISPLAY_SETTINGS_HOOK_SETWINDOWLONGPTRW,
    NUM_DISPLAY_SETTINGS_HOOKS
};

// ReShade Events (0-37)
enum ReShadeEventIndex {
    RESHADE_EVENT_BEGIN_RENDER_PASS,
    RESHADE_EVENT_END_RENDER_PASS,
    RESHADE_EVENT_CREATE_SWAPCHAIN_CAPTURE,
    RESHADE_EVENT_INIT_SWAPCHAIN,
    RESHADE_EVENT_PRESENT_UPDATE_AFTER,
    RESHADE_EVENT_PRESENT_UPDATE_BEFORE,
    RESHADE_EVENT_PRESENT_UPDATE_BEFORE2_UNUSED,
    RESHADE_EVENT_INIT_COMMAND_LIST,
    RESHADE_EVENT_EXECUTE_COMMAND_LIST,
    RESHADE_EVENT_BIND_PIPELINE,
    RESHADE_EVENT_INIT_COMMAND_QUEUE,
    RESHADE_EVENT_RESET_COMMAND_LIST,
    RESHADE_EVENT_PRESENT_FLAGS,
    RESHADE_EVENT_DRAW,
    RESHADE_EVENT_DRAW_INDEXED,
    RESHADE_EVENT_DRAW_OR_DISPATCH_INDIRECT,
    // Power saving event counters
    RESHADE_EVENT_DISPATCH,
    RESHADE_EVENT_DISPATCH_MESH,
    RESHADE_EVENT_DISPATCH_RAYS,
    RESHADE_EVENT_COPY_RESOURCE,
    RESHADE_EVENT_UPDATE_BUFFER_REGION,
    RESHADE_EVENT_UPDATE_BUFFER_REGION_COMMAND,
    RESHADE_EVENT_BIND_RESOURCE,
    RESHADE_EVENT_MAP_RESOURCE,
    // Additional frame-specific GPU operations for power saving
    RESHADE_EVENT_COPY_BUFFER_REGION,
    RESHADE_EVENT_COPY_BUFFER_TO_TEXTURE,
    RESHADE_EVENT_COPY_TEXTURE_TO_BUFFER,
    RESHADE_EVENT_COPY_TEXTURE_REGION,
    RESHADE_EVENT_RESOLVE_TEXTURE_REGION,
    RESHADE_EVENT_CLEAR_RENDER_TARGET_VIEW,
    RESHADE_EVENT_CLEAR_DEPTH_STENCIL_VIEW,
    RESHADE_EVENT_CLEAR_UNORDERED_ACCESS_VIEW_UINT,
    RESHADE_EVENT_CLEAR_UNORDERED_ACCESS_VIEW_FLOAT,
    RESHADE_EVENT_GENERATE_MIPMAPS,
    RESHADE_EVENT_BLIT,
    RESHADE_EVENT_BEGIN_QUERY,
    RESHADE_EVENT_END_QUERY,
    RESHADE_EVENT_RESOLVE_QUERY_DATA,
    NUM_RESHADE_EVENTS
};

// DXGI Core Methods (38-47)
enum DxgiCoreEventIndex {
    DXGI_CORE_EVENT_PRESENT,
    DXGI_CORE_EVENT_GETBUFFER,
    DXGI_CORE_EVENT_SETFULLSCREENSTATE,
    DXGI_CORE_EVENT_GETFULLSCREENSTATE,
    DXGI_CORE_EVENT_GETDESC,
    DXGI_CORE_EVENT_RESIZEBUFFERS,
    DXGI_CORE_EVENT_RESIZETARGET,
    DXGI_CORE_EVENT_GETCONTAININGOUTPUT,
    DXGI_CORE_EVENT_GETFRAMESTATISTICS,
    DXGI_CORE_EVENT_GETLASTPRESENTCOUNT,
    NUM_DXGI_CORE_EVENTS
};

// DXGI SwapChain1 Methods (48-58)
enum DxgiSwapChain1EventIndex {
    DXGI_SC1_EVENT_GETDESC1,
    DXGI_SC1_EVENT_GETFULLSCREENDESC,
    DXGI_SC1_EVENT_GETHWND,
    DXGI_SC1_EVENT_GETCOREWINDOW,
    DXGI_SC1_EVENT_PRESENT1,
    DXGI_SC1_EVENT_ISTEMPORARYMONOSUPPORTED,
    DXGI_SC1_EVENT_GETRESTRICTTOOUTPUT,
    DXGI_SC1_EVENT_SETBACKGROUNDCOLOR,
    DXGI_SC1_EVENT_GETBACKGROUNDCOLOR,
    DXGI_SC1_EVENT_SETROTATION,
    DXGI_SC1_EVENT_GETROTATION,
    NUM_DXGI_SC1_EVENTS
};

// DXGI SwapChain2 Methods (59-65)
enum DxgiSwapChain2EventIndex {
    DXGI_SC2_EVENT_SETSOURCESIZE,
    DXGI_SC2_EVENT_GETSOURCESIZE,
    DXGI_SC2_EVENT_SETMAXIMUMFRAMELATENCY,
    DXGI_SC2_EVENT_GETMAXIMUMFRAMELATENCY,
    DXGI_SC2_EVENT_GETFRAMELATENCYWAIABLEOBJECT,
    DXGI_SC2_EVENT_SETMATRIXTRANSFORM,
    DXGI_SC2_EVENT_GETMATRIXTRANSFORM,
    NUM_DXGI_SC2_EVENTS
};

// DXGI SwapChain3 Methods (66-69)
enum DxgiSwapChain3EventIndex {
    DXGI_SC3_EVENT_GETCURRENTBACKBUFFERINDEX,
    DXGI_SC3_EVENT_CHECKCOLORSPACESUPPORT,
    DXGI_SC3_EVENT_SETCOLORSPACE1,
    DXGI_SC3_EVENT_RESIZEBUFFERS1,
    NUM_DXGI_SC3_EVENTS
};

// DXGI Factory Methods (70-72)
enum DxgiFactoryEventIndex {
    DXGI_FACTORY_EVENT_CREATESWAPCHAIN,
    DXGI_FACTORY_EVENT_CREATEFACTORY,
    DXGI_FACTORY_EVENT_CREATEFACTORY1,
    NUM_DXGI_FACTORY_EVENTS
};

// DXGI SwapChain4 Methods (73)
enum DxgiSwapChain4EventIndex {
    DXGI_SC4_EVENT_SETHDRMETADATA,
    NUM_DXGI_SC4_EVENTS
};

// DXGI Output Methods (74-76)
enum DxgiOutputEventIndex {
    DXGI_OUTPUT_EVENT_SETGAMMACONTROL,
    DXGI_OUTPUT_EVENT_GETGAMMACONTROL,
    DXGI_OUTPUT_EVENT_GETDESC,
    NUM_DXGI_OUTPUT_EVENTS
};

// DirectX 9 Methods (77)
enum Dx9EventIndex {
    DX9_EVENT_PRESENT,
    NUM_DX9_EVENTS
};

// Streamline Methods (75-78)
enum StreamlineEventIndex {
    STREAMLINE_EVENT_SL_INIT,
    STREAMLINE_EVENT_SL_IS_FEATURE_SUPPORTED,
    STREAMLINE_EVENT_SL_GET_NATIVE_INTERFACE,
    STREAMLINE_EVENT_SL_UPGRADE_INTERFACE,
    NUM_STREAMLINE_EVENTS
};

// NVAPI event counters - separate from swapchain events
enum NvapiEventIndex {
    NVAPI_EVENT_GET_HDR_CAPABILITIES,
    NVAPI_EVENT_D3D_SET_LATENCY_MARKER,
    NVAPI_EVENT_D3D_SET_SLEEP_MODE,
    NVAPI_EVENT_D3D_SLEEP,
    NVAPI_EVENT_D3D_GET_LATENCY,
    NUM_NVAPI_EVENTS
};

// Swapchain event counters - reset on each swapchain creation
// Separate event counter arrays for each category
extern std::array<std::atomic<uint32_t>, NUM_RESHADE_EVENTS> g_reshade_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_DXGI_CORE_EVENTS> g_dxgi_core_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_DXGI_SC1_EVENTS> g_dxgi_sc1_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_DXGI_SC2_EVENTS> g_dxgi_sc2_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_DXGI_SC3_EVENTS> g_dxgi_sc3_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_DXGI_FACTORY_EVENTS> g_dxgi_factory_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_DXGI_SC4_EVENTS> g_dxgi_sc4_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_DXGI_OUTPUT_EVENTS> g_dxgi_output_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_DX9_EVENTS> g_dx9_event_counters;
extern std::array<std::atomic<uint32_t>, NUM_STREAMLINE_EVENTS> g_streamline_event_counters;

// NVAPI event counters - separate from swapchain events
extern std::array<std::atomic<uint32_t>, NUM_NVAPI_EVENTS> g_nvapi_event_counters;  // Array for NVAPI events

// NVAPI sleep timestamp tracking
extern std::atomic<uint64_t> g_nvapi_last_sleep_timestamp_ns;  // Last NVAPI_D3D_Sleep call timestamp in nanoseconds
extern std::atomic<uint32_t> g_swapchain_event_total_count;   // Total events across all types

// OpenGL hook counters
extern std::array<std::atomic<uint64_t>, NUM_OPENGL_HOOKS> g_opengl_hook_counters;  // Array for all OpenGL hook events
extern std::atomic<uint64_t> g_opengl_hook_total_count;   // Total OpenGL hook events across all types

// Display settings hook counters
extern std::array<std::atomic<uint64_t>, NUM_DISPLAY_SETTINGS_HOOKS> g_display_settings_hook_counters;  // Array for all display settings hook events
extern std::atomic<uint64_t> g_display_settings_hook_total_count;   // Total display settings hook events across all types

// Unsorted TODO: Add in correct order above
extern std::atomic<LONGLONG> g_present_start_time_ns;

// Present pacing delay as percentage of frame time - 0% to 100%

extern std::atomic<LONGLONG> late_amount_ns;

// GPU completion measurement using EnqueueSetEvent
extern std::atomic<HANDLE> g_gpu_completion_event;  // Event handle for GPU completion measurement
extern std::atomic<LONGLONG> g_gpu_completion_time_ns;  // Last measured GPU completion time
extern std::atomic<LONGLONG> g_gpu_duration_ns;  // Last measured GPU duration (smoothed)

// GPU completion failure tracking
extern std::atomic<const char*> g_gpu_fence_failure_reason;  // Reason why GPU fence creation/usage failed (nullptr if no failure)

// Sim-start-to-display latency measurement
extern std::atomic<LONGLONG> g_sim_start_ns_for_measurement;  // g_sim_start_ns captured when EnqueueGPUCompletion is called
extern std::atomic<bool> g_present_update_after2_called;  // Tracks if OnPresentUpdateAfter2 was called
extern std::atomic<bool> g_gpu_completion_callback_finished;  // Tracks if GPU completion callback finished
extern std::atomic<LONGLONG> g_sim_to_display_latency_ns;  // Measured sim-start-to-display latency (smoothed)

// GPU late time measurement (how much later GPU finishes compared to OnPresentUpdateAfter2)
extern std::atomic<LONGLONG> g_present_update_after2_time_ns;  // Time when OnPresentUpdateAfter2 was called
extern std::atomic<LONGLONG> g_gpu_completion_callback_time_ns;  // Time when GPU completion callback finished
extern std::atomic<LONGLONG> g_gpu_late_time_ns;  // GPU late time (0 if GPU finished first, otherwise difference)

// NVIDIA Reflex minimal controls



// DLLS-G (DLSS Frame Generation) status
extern std::atomic<bool> g_dlls_g_loaded;                                 // DLLS-G loaded status
extern std::atomic<std::shared_ptr<const std::string>> g_dlls_g_version;  // DLLS-G version string

// NGX Feature status tracking (set in CreateFeature detours)
extern std::atomic<bool> g_dlss_enabled;          // DLSS Super Resolution enabled
extern std::atomic<bool> g_dlssg_enabled;         // DLSS Frame Generation enabled
extern std::atomic<bool> g_ray_reconstruction_enabled; // Ray Reconstruction enabled

// NGX Parameter Storage (unified thread-safe atomic shared_ptr hashmap)
extern UnifiedParameterMap g_ngx_parameters; // Unified NGX parameters supporting all types

// NGX Counters structure for tracking NGX function calls
struct NGXCounters {
    // Parameter functions
    std::atomic<uint32_t> parameter_setf_count;
    std::atomic<uint32_t> parameter_setd_count;
    std::atomic<uint32_t> parameter_seti_count;
    std::atomic<uint32_t> parameter_setui_count;
    std::atomic<uint32_t> parameter_setull_count;
    std::atomic<uint32_t> parameter_geti_count;
    std::atomic<uint32_t> parameter_getui_count;
    std::atomic<uint32_t> parameter_getull_count;
    std::atomic<uint32_t> parameter_getvoidpointer_count;

    // D3D12 Feature management functions
    std::atomic<uint32_t> d3d12_init_count;
    std::atomic<uint32_t> d3d12_init_ext_count;
    std::atomic<uint32_t> d3d12_init_projectid_count;
    std::atomic<uint32_t> d3d12_createfeature_count;
    std::atomic<uint32_t> d3d12_releasefeature_count;
    std::atomic<uint32_t> d3d12_evaluatefeature_count;
    std::atomic<uint32_t> d3d12_getparameters_count;
    std::atomic<uint32_t> d3d12_allocateparameters_count;

    // D3D11 Feature management functions
    std::atomic<uint32_t> d3d11_init_count;
    std::atomic<uint32_t> d3d11_init_ext_count;
    std::atomic<uint32_t> d3d11_init_projectid_count;
    std::atomic<uint32_t> d3d11_createfeature_count;
    std::atomic<uint32_t> d3d11_releasefeature_count;
    std::atomic<uint32_t> d3d11_evaluatefeature_count;
    std::atomic<uint32_t> d3d11_getparameters_count;
    std::atomic<uint32_t> d3d11_allocateparameters_count;

    // Total counter
    std::atomic<uint32_t> total_count;

    // Constructor to initialize all counters to 0
    NGXCounters() :
        parameter_setf_count(0), parameter_setd_count(0), parameter_seti_count(0),
        parameter_setui_count(0), parameter_setull_count(0), parameter_geti_count(0),
        parameter_getui_count(0), parameter_getull_count(0), parameter_getvoidpointer_count(0),
        d3d12_init_count(0), d3d12_init_ext_count(0), d3d12_init_projectid_count(0),
        d3d12_createfeature_count(0), d3d12_releasefeature_count(0), d3d12_evaluatefeature_count(0),
        d3d12_getparameters_count(0), d3d12_allocateparameters_count(0),
        d3d11_init_count(0), d3d11_init_ext_count(0), d3d11_init_projectid_count(0),
        d3d11_createfeature_count(0), d3d11_releasefeature_count(0), d3d11_evaluatefeature_count(0),
        d3d11_getparameters_count(0), d3d11_allocateparameters_count(0),
        total_count(0) {}

    // Reset all counters to 0
    void reset() {
        parameter_setf_count.store(0);
        parameter_setd_count.store(0);
        parameter_seti_count.store(0);
        parameter_setui_count.store(0);
        parameter_setull_count.store(0);
        parameter_geti_count.store(0);
        parameter_getui_count.store(0);
        parameter_getull_count.store(0);
        parameter_getvoidpointer_count.store(0);
        d3d12_init_count.store(0);
        d3d12_init_ext_count.store(0);
        d3d12_init_projectid_count.store(0);
        d3d12_createfeature_count.store(0);
        d3d12_releasefeature_count.store(0);
        d3d12_evaluatefeature_count.store(0);
        d3d12_getparameters_count.store(0);
        d3d12_allocateparameters_count.store(0);
        d3d11_init_count.store(0);
        d3d11_init_ext_count.store(0);
        d3d11_init_projectid_count.store(0);
        d3d11_createfeature_count.store(0);
        d3d11_releasefeature_count.store(0);
        d3d11_evaluatefeature_count.store(0);
        d3d11_getparameters_count.store(0);
        d3d11_allocateparameters_count.store(0);
        total_count.store(0);
    }
};

// NGX Counters global instance
extern NGXCounters g_ngx_counters;

// DLSS/DLSS-G Summary structure
struct DLSSGSummary {
    bool dlss_active = false;
    bool dlss_g_active = false;
    bool ray_reconstruction_active = false;
    std::string internal_resolution = "N/A";
    std::string output_resolution = "N/A";
    std::string scaling_ratio = "N/A";
    std::string quality_preset = "N/A";
    std::string aspect_ratio = "N/A";
    std::string fov = "N/A";
    std::string jitter_offset = "N/A";
    std::string exposure = "N/A";
    std::string depth_inverted = "N/A";
    std::string hdr_enabled = "N/A";
    std::string motion_vectors_included = "N/A";
    std::string frame_time_delta = "N/A";
    std::string sharpness = "N/A";
    std::string tonemapper_type = "N/A";
    std::string fg_mode = "N/A";
    std::string ofa_enabled = "N/A";
    std::string dlss_dll_version = "N/A";
    std::string dlssg_dll_version = "N/A";
    std::string dlssd_dll_version = "N/A";
    std::string supported_dlss_presets = "N/A";
    std::string supported_dlss_rr_presets = "N/A";
};

// DLSS Model Profile structure
struct DLSSModelProfile {
    bool is_valid = false;
    int sr_quality_preset = 0;
    int sr_balanced_preset = 0;
    int sr_performance_preset = 0;
    int sr_ultra_performance_preset = 0;
    int sr_ultra_quality_preset = 0;
    int sr_dlaa_preset = 0;
    int rr_quality_preset = 0;
    int rr_balanced_preset = 0;
    int rr_performance_preset = 0;
    int rr_ultra_performance_preset = 0;
    int rr_ultra_quality_preset = 0;
};

// Function to get DLSS/DLSS-G summary from NGX parameters
DLSSGSummary GetDLSSGSummary();

// Function to get DLSS Model Profile
DLSSModelProfile GetDLSSModelProfile();

// NVAPI SetSleepMode tracking
extern std::atomic<std::shared_ptr<NV_SET_SLEEP_MODE_PARAMS>> g_last_nvapi_sleep_mode_params;  // Last SetSleepMode parameters
extern std::atomic<IUnknown*> g_last_nvapi_sleep_mode_dev_ptr;  // Last device pointer for SetSleepMode

// NVAPI Reflex timing tracking
extern std::atomic<LONGLONG> g_sleep_reflex_injected_ns;  // Time between injected Reflex sleep calls
extern std::atomic<LONGLONG> g_sleep_reflex_native_ns;    // Time between native Reflex sleep calls

// Reflex debug counters
extern std::atomic<uint32_t> g_reflex_sleep_count;          // Total Sleep calls
extern std::atomic<uint32_t> g_reflex_apply_sleep_mode_count; // Total ApplySleepMode calls
extern std::atomic<LONGLONG> g_reflex_sleep_duration_ns;    // Rolling average sleep duration in nanoseconds

// Individual marker type counters
extern std::atomic<uint32_t> g_reflex_marker_simulation_start_count;
extern std::atomic<uint32_t> g_reflex_marker_simulation_end_count;
extern std::atomic<uint32_t> g_reflex_marker_rendersubmit_start_count;
extern std::atomic<uint32_t> g_reflex_marker_rendersubmit_end_count;
extern std::atomic<uint32_t> g_reflex_marker_present_start_count;
extern std::atomic<uint32_t> g_reflex_marker_present_end_count;
extern std::atomic<uint32_t> g_reflex_marker_input_sample_count;

// DX11 Proxy HWND for filtering
extern HWND g_proxy_hwnd;

// NGX preset initialization tracking
extern std::atomic<bool> g_ngx_presets_initialized;

// Continuous monitoring functions
void StartContinuousMonitoring();
void StopContinuousMonitoring();
void HandleReflexAutoConfigure();

