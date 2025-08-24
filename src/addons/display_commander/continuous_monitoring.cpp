#include "addon.hpp"
#include "utils.hpp"
#include "background_window.hpp"
#include "dxgi/dxgi_device_info.hpp"
#include <thread>
#include <chrono>
#include <sstream>

// Forward declarations
void ComputeDesiredSize(int& out_w, int& out_h);

// Global variables for monitoring
extern float s_continuous_monitoring_enabled;
extern std::atomic<bool> g_monitoring_thread_running;
extern std::thread g_monitoring_thread;
extern std::atomic<HWND> g_last_swapchain_hwnd;

// Additional global variables needed for monitoring
extern float s_remove_top_bar;
extern float s_prevent_always_on_top;

extern float s_background_feature_enabled; // Added this line

// ReShade runtime for input blocking


// Background window manager
extern class BackgroundWindowManager g_backgroundWindowManager;

// Input blocking state

extern std::atomic<bool> g_app_in_background;

// Main monitoring thread function
void ContinuousMonitoringThread() {
    LogInfo("Continuous monitoring thread started");
    
    static int seconds_counter = 0;
    auto last_cache_refresh = std::chrono::steady_clock::now();
    while (g_monitoring_thread_running.load()) {
        // Check if monitoring is still enabled
        if (s_continuous_monitoring_enabled < 0.5f) {
            break;
        }
        
        // Get the current swapchain window
        HWND hwnd = g_last_swapchain_hwnd.load();
        if (hwnd != nullptr && IsWindow(hwnd)) {
            // BACKGROUND DETECTION: Check if the app is in background
            bool current_background = (GetForegroundWindow() != hwnd);
            bool background_changed = (current_background != g_app_in_background.load());
            
            if (background_changed) {
                g_app_in_background.store(current_background);
                
                if (current_background) {
                    LogInfo("Continuous monitoring: App moved to BACKGROUND");
                } else {
                    LogInfo("Continuous monitoring: App moved to FOREGROUND");
                }
            }
            
            // Apply window changes - the function will automatically determine what needs to be changed
            ApplyWindowChange(hwnd, "continuous_monitoring_auto_fix");
            
            // BACKGROUND WINDOW MANAGEMENT: Update background window if feature is enabled
            static int background_check_counter = 0;
            if (++background_check_counter % 10 == 0) { // Log every 10 seconds
                std::ostringstream oss;
                oss << "Continuous monitoring: Background feature check - s_background_feature_enabled = " << s_background_feature_enabled 
                    << ", has_background_window = " << g_backgroundWindowManager.HasBackgroundWindow();
                LogInfo(oss.str().c_str());
            }
            
            // FOCUS LOSS DETECTION: Close background window when main window loses focus
            HWND foreground_window = GetForegroundWindow();
            if (foreground_window != hwnd && g_backgroundWindowManager.HasBackgroundWindow()) {
                // Main window lost focus, close background window
                LogInfo("Continuous monitoring: Main window lost focus - closing background window");
         
         //       g_backgroundWindowManager.DestroyBackgroundWindow();
            }
            
            if (s_background_feature_enabled >= 0.5f) {
                // Only create/update background window if main window has focus
                if (foreground_window == hwnd) {
                    LogInfo("Continuous monitoring: Calling UpdateBackgroundWindow for background window management");
                    g_backgroundWindowManager.UpdateBackgroundWindow(hwnd);
                } else {
                    if (background_check_counter % 10 == 0) { // Log occasionally
                        LogInfo("Continuous monitoring: Skipping background window update - main window not focused");
                    }
                }
            } else {
                if (background_check_counter % 10 == 0) { // Log occasionally
                    LogInfo("Continuous monitoring: Background feature disabled (s_background_feature_enabled < 0.5f)");
                }
            }
        }

        // Update monitor labels cache periodically off the UI thread
        {
            // Refresh every 2 seconds to avoid excessive work
            if ((seconds_counter % 2) == 0) {
                auto labels = MakeMonitorLabels();
                ::g_monitor_labels_lock.lock();
                ::g_monitor_labels = std::move(labels);
                ::g_monitor_labels_lock.unlock();
                ::g_monitor_labels_need_update.store(false, std::memory_order_release);
            }
        }

        // BACKGROUND: Composition state logging and periodic device/colorspace refresh
        {
            auto* swapchain = g_last_swapchain_ptr.load();
            if (swapchain != nullptr) {
                // Compute DXGI composition state and log on change
                DxgiBypassMode mode = GetIndependentFlipState(swapchain);
                int state = 0;
                switch (mode) {
                    case DxgiBypassMode::kComposed: state = 1; break;
                    case DxgiBypassMode::kOverlay: state = 2; break;
                    case DxgiBypassMode::kIndependentFlip: state = 3; break;
                    case DxgiBypassMode::kUnknown:
                    default: state = 0; break;
                }

                // Update shared state for fast reads on present
                s_dxgi_composition_state = static_cast<float>(state);

                int last = g_comp_last_logged.load();
                if (state != last) {
                    g_comp_last_logged.store(state);
                    std::ostringstream oss;
                    oss << "DXGI Composition State (background): " << DxgiBypassModeToString(mode) << " (" << state << ")";
                    LogInfo(oss.str().c_str());
                }

                // Periodically refresh colorspace and enumerate devices (approx every 4 seconds)
                if ((seconds_counter % 4) == 0) {
                    g_current_colorspace = swapchain->get_color_space();

                    extern std::unique_ptr<DXGIDeviceInfoManager> g_dxgiDeviceInfoManager;
                    if (g_dxgiDeviceInfoManager && g_dxgiDeviceInfoManager->IsInitialized()) {
                        g_dxgiDeviceInfoManager->EnumerateDevicesOnPresent();
                    }
                }
            }
        }
        
        // Periodic display cache refresh off the UI thread
        {
            auto now = std::chrono::steady_clock::now();
            if (now - last_cache_refresh >= std::chrono::seconds(5)) {
                display_cache::g_displayCache.Refresh();
                last_cache_refresh = now;
            }
        }
        
        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++seconds_counter;
    }
    
    LogInfo("Continuous monitoring thread stopped");
}

// Start continuous monitoring
void StartContinuousMonitoring() {
    if (g_monitoring_thread_running.load()) {
        LogDebug("Continuous monitoring already running");
        return;
    }
    
    if (s_continuous_monitoring_enabled < 0.5f) {
        LogDebug("Continuous monitoring disabled, not starting");
        return;
    }
    
    g_monitoring_thread_running.store(true);
    
    // Start the monitoring thread
    if (g_monitoring_thread.joinable()) {
        g_monitoring_thread.join();
    }
    
    g_monitoring_thread = std::thread(ContinuousMonitoringThread);
    
    LogInfo("Continuous monitoring started");
}

// Stop continuous monitoring
void StopContinuousMonitoring() {
    if (!g_monitoring_thread_running.load()) {
        LogDebug("Continuous monitoring not running");
        return;
    }
    
    g_monitoring_thread_running.store(false);
    
    // Wait for thread to finish
    if (g_monitoring_thread.joinable()) {
        g_monitoring_thread.join();
    }
    
    LogInfo("Continuous monitoring stopped");
}
