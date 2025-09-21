#include "resolution_widget.hpp"
#include "../../display_cache.hpp"
#include "../../resolution_helpers.hpp"
#include "../../globals.hpp"
#include "../../display_restore.hpp"
#include "../../utils/timing.hpp"
#include <imgui.h>
#include <reshade.hpp>
#include <algorithm>
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace display_commander::widgets::resolution_widget {

// Helper function to format refresh rate string
std::string FormatRefreshRateString(int refresh_numerator, int refresh_denominator) {
    if (refresh_numerator > 0 && refresh_denominator > 0) {
        double refresh_hz = static_cast<double>(refresh_numerator) / static_cast<double>(refresh_denominator);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << refresh_hz;
        std::string refresh_str = oss.str();

        // Remove trailing zeros and decimal point if not needed
        refresh_str.erase(refresh_str.find_last_not_of('0') + 1, std::string::npos);
        if (refresh_str.back() == '.') {
            refresh_str.pop_back();
        }

        return "@" + refresh_str + "Hz";
    }
    return "";
}

// Global widget instance
std::unique_ptr<ResolutionWidget> g_resolution_widget = nullptr;

ResolutionWidget::ResolutionWidget() = default;

void ResolutionWidget::Initialize() {
    if (is_initialized_) return;

    LogInfo("ResolutionWidget::Initialize() - Starting resolution widget initialization");

    // Initialize settings if not already done
    InitializeResolutionSettings();

    // Set initial display to current monitor
    selected_display_index_ = 0; // Auto (Current)
    LogInfo("ResolutionWidget::Initialize() - Set selected_display_index_ = %d (Auto/Current)", selected_display_index_);

    // Capture original settings when widget is first initialized
    CaptureOriginalSettings();

    is_initialized_ = true;
    needs_refresh_ = true;

    LogInfo("ResolutionWidget::Initialize() - Resolution widget initialization complete");
}

void ResolutionWidget::Cleanup() {
    if (!is_initialized_) return;

    // Save any pending changes
    if (g_resolution_settings && g_resolution_settings->HasAnyDirty()) {
        g_resolution_settings->SaveAllDirty();
    }

    is_initialized_ = false;
}

void ResolutionWidget::OnDraw() {
    if (!is_initialized_) {
        Initialize();
    }

    if (!g_resolution_settings) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Resolution settings not initialized");
        return;
    }

    // Try to capture original settings if not captured yet
    if (!original_settings_.captured) {
        CaptureOriginalSettings();
    }

    // Refresh data if needed
    if (needs_refresh_) {
        RefreshDisplayData();
        needs_refresh_ = false;
    }

    // Apply loaded settings to UI selection (only once)
    if (!settings_applied_to_ui_) {
    //  LogInfo("ResolutionWidget::OnDraw() - First draw, applying loaded settings to UI");
        UpdateCurrentSelectionFromSettings();
        settings_applied_to_ui_ = true;
    //     LogInfo("ResolutionWidget::OnDraw() - Applied settings to UI indices: display=%d, resolution=%d, refresh=%d",
    //             selected_display_index_, selected_resolution_index_, selected_refresh_index_);
    }

    // Draw the resolution widget UI
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "=== Resolution Control ===");
    ImGui::Spacing();

    // Log auto-apply state on every draw
    bool auto_apply_state = g_resolution_settings->GetAutoApply();
    //  LogInfo("ResolutionWidget::OnDraw() - Auto-apply changes is: %s", auto_apply_state ? "ON" : "OFF");

    // Auto-apply checkbox
    DrawAutoApplyCheckbox();
    ImGui::Spacing();

    // Auto-restore checkbox
    DrawAutoRestoreCheckbox();
    ImGui::Spacing();

    // Original settings info
    DrawOriginalSettingsInfo();
    ImGui::Spacing();

    // Display selector
    DrawDisplaySelector();
    ImGui::Spacing();

    // Resolution selector
    DrawResolutionSelector();
    ImGui::Spacing();

    // Refresh rate selector
    DrawRefreshRateSelector();
    ImGui::Spacing();

    // Action buttons
    DrawActionButtons();

    // Confirmation dialog
    if (show_confirmation_) {
        DrawConfirmationDialog();
    }
}

void ResolutionWidget::DrawAutoApplyCheckbox() {
    bool auto_apply = g_resolution_settings->GetAutoApply();
    if (ImGui::Checkbox("Auto-apply changes", &auto_apply)) {
        g_resolution_settings->SetAutoApply(auto_apply);
        LogInfo("ResolutionWidget::DrawAutoApplyCheckbox() - Auto-apply changes set to: %s", auto_apply ? "true" : "false");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically apply resolution changes when selections are made");
    }
}

void ResolutionWidget::DrawDisplaySelector() {
    // Get available displays
    std::vector<std::string> display_names;

    // Format Auto (Current) option with detailed info like legacy format
    std::string auto_label = "Auto (Current)";
    HWND hwnd = g_last_swapchain_hwnd.load();
    if (hwnd) {
        HMONITOR current_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (current_monitor) {
            const auto* display = display_cache::g_displayCache.GetDisplayByHandle(current_monitor);
            if (display) {
                int width = display->width;
                int height = display->height;
                double refresh_rate = display->current_refresh_rate.ToHz();

                // Format refresh rate with precision
                std::ostringstream rate_oss;
                rate_oss << std::fixed << std::setprecision(6) << refresh_rate;
                std::string rate_str = rate_oss.str();

                // Remove trailing zeros and decimal point if not needed
                rate_str.erase(rate_str.find_last_not_of('0') + 1, std::string::npos);
                if (rate_str.back() == '.') {
                    rate_str.pop_back();
                }

                // Use cached primary monitor flag and device name
                bool is_primary = display->is_primary;
                std::string primary_text = is_primary ? " Primary" : "";
                std::string device_name(display->device_name.begin(), display->device_name.end());

                auto_label = "Auto (Current) [" + device_name + "] " +
                            std::to_string(width) + "x" + std::to_string(height) +
                            "@" + rate_str + "Hz" + primary_text;
            }
        }
    }
    display_names.push_back(auto_label);

    auto displays = display_cache::g_displayCache.GetDisplays();
    if (displays) {
        for (size_t i = 0; i < (std::min)(displays->size(), static_cast<size_t>(4)); ++i) {
            const auto* display = (*displays)[i].get();
            if (display) {
                // Format with resolution, refresh rate, and primary status like Auto Current
                int width = display->width;
                int height = display->height;
                double refresh_rate = display->current_refresh_rate.ToHz();

                // Format refresh rate with precision
                std::ostringstream rate_oss;
                rate_oss << std::fixed << std::setprecision(6) << refresh_rate;
                std::string rate_str = rate_oss.str();

                // Remove trailing zeros and decimal point if not needed
                rate_str.erase(rate_str.find_last_not_of('0') + 1, std::string::npos);
                if (rate_str.back() == '.') {
                    rate_str.pop_back();
                }

                // Use cached primary monitor flag and device name
                bool is_primary = display->is_primary;
                std::string primary_text = is_primary ? " Primary" : "";
                std::string device_name(display->device_name.begin(), display->device_name.end());

                std::string name = "[" + device_name + "] " +
                std::to_string(width) + "x" + std::to_string(height) +
                "@" + rate_str + "Hz" + primary_text;

                display_names.push_back(name);
            }
        }
    }

    ImGui::PushID("display_selector");
    if (ImGui::BeginCombo("##display", display_names[selected_display_index_].c_str())) {
        for (int i = 0; i < static_cast<int>(display_names.size()); ++i) {
            const bool is_selected = (i == selected_display_index_);
            if (ImGui::Selectable(display_names[i].c_str(), is_selected)) {
                selected_display_index_ = i;
                needs_refresh_ = true;
                UpdateCurrentSelectionFromSettings();
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Text("Display");

}

void ResolutionWidget::DrawResolutionSelector() {
    if (resolution_labels_.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No resolutions available");
        return;
    }

    ImGui::PushID("resolution_selector");
    if (ImGui::BeginCombo("##resolution", resolution_labels_[selected_resolution_index_].c_str())) {
        for (int i = 0; i < static_cast<int>(resolution_labels_.size()); ++i) {
            const bool is_selected = (i == selected_resolution_index_);
            if (ImGui::Selectable(resolution_labels_[i].c_str(), is_selected)) {
                selected_resolution_index_ = i;
                selected_refresh_index_ = 0; // Reset refresh rate selection
                UpdateSettingsFromCurrentSelection();

                // Auto-apply if enabled
                if (g_resolution_settings->GetAutoApply()) {
                    ApplyCurrentSelection();
                }
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Text("Resolution");

}

void ResolutionWidget::DrawRefreshRateSelector() {

    if (refresh_labels_.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No refresh rates available");
        return;
    }

    ImGui::PushID("refresh_selector");
    if (ImGui::BeginCombo("##refresh", refresh_labels_[selected_refresh_index_].c_str())) {
        for (int i = 0; i < static_cast<int>(refresh_labels_.size()); ++i) {
            const bool is_selected = (i == selected_refresh_index_);
            if (ImGui::Selectable(refresh_labels_[i].c_str(), is_selected)) {
                selected_refresh_index_ = i;
                UpdateSettingsFromCurrentSelection();

                // Auto-apply if enabled
                if (g_resolution_settings->GetAutoApply()) {
                    ApplyCurrentSelection();
                }
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Text("Refresh Rate");
}

void ResolutionWidget::DrawActionButtons() {
    int actual_display = GetActualDisplayIndex();
    auto& display_settings = g_resolution_settings->GetDisplaySettings(actual_display);

    // Show dirty state indicator
    if (display_settings.IsDirty()) {
        const ResolutionData& current = display_settings.GetCurrentState();
        const ResolutionData& last_saved = display_settings.GetLastSavedState();

        // Format resolution and refresh rate as "widthxheight@refreshHz"
        auto formatResolution = [this, actual_display](const ResolutionData& data) -> std::string {
            if (data.is_current) {
                // Get actual current resolution and refresh rate
                int current_width, current_height;
                display_cache::RationalRefreshRate current_refresh;

                if (display_cache::g_displayCache.GetCurrentResolution(actual_display, current_width, current_height) &&
                    display_cache::g_displayCache.GetCurrentRefreshRate(actual_display, current_refresh)) {

                    std::string resolution = std::to_string(current_width) + "x" + std::to_string(current_height);

                    // Determine refresh rate to use
                    int current_refresh_numerator = data.refresh_numerator;
                    int current_refresh_denominator = data.refresh_denominator;

                    if (current_refresh_numerator == 0) {
                        // Use current refresh rate if no specific refresh rate is set
                        current_refresh_numerator = static_cast<int>(current_refresh.numerator);
                        current_refresh_denominator = static_cast<int>(current_refresh.denominator);
                    }

                    resolution += FormatRefreshRateString(current_refresh_numerator, current_refresh_denominator);
                    return resolution;
                } else {
                    return "Current Resolution";
                }
            }

            std::string resolution = std::to_string(data.width) + "x" + std::to_string(data.height);

            // Determine refresh rate to use
            int current_refresh_numerator = data.refresh_numerator;
            int current_refresh_denominator = data.refresh_denominator;

            if (current_refresh_numerator == 0) {
                // Find current refresh rate
                display_cache::RationalRefreshRate current_refresh;
                if (display_cache::g_displayCache.GetCurrentRefreshRate(actual_display, current_refresh)) {
                    current_refresh_numerator = static_cast<int>(current_refresh.numerator);
                    current_refresh_denominator = static_cast<int>(current_refresh.denominator);
                }
            }

            resolution += FormatRefreshRateString(current_refresh_numerator, current_refresh_denominator);
            return resolution;
        };

        std::string current_str = formatResolution(current);
        std::string saved_str = formatResolution(last_saved);

        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "● %s -> %s", saved_str.c_str(), current_str.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "● Settings saved");
    }

    ImGui::Spacing();

    // Apply button
    if (ImGui::Button("Apply Resolution")) {
        // Store pending resolution for confirmation
        if (!resolution_data_.empty() && !refresh_data_.empty()) {
            // Store the current resolution before changing
            int current_width, current_height;
            display_cache::RationalRefreshRate current_refresh;

            if (display_cache::g_displayCache.GetCurrentResolution(actual_display, current_width, current_height) &&
                display_cache::g_displayCache.GetCurrentRefreshRate(actual_display, current_refresh)) {

                previous_resolution_.width = current_width;
                previous_resolution_.height = current_height;
                previous_resolution_.refresh_numerator = static_cast<int>(current_refresh.numerator);
                previous_resolution_.refresh_denominator = static_cast<int>(current_refresh.denominator);
                previous_resolution_.is_current = false; // This is a specific resolution, not "current"

                // Store the same data for refresh rate
                previous_refresh_.width = current_width;
                previous_refresh_.height = current_height;
                previous_refresh_.refresh_numerator = static_cast<int>(current_refresh.numerator);
                previous_refresh_.refresh_denominator = static_cast<int>(current_refresh.denominator);
                previous_refresh_.is_current = false;
            }

            pending_resolution_ = resolution_data_[selected_resolution_index_];
            pending_refresh_ = refresh_data_[selected_refresh_index_];
            pending_display_index_ = actual_display;

            // Apply the resolution immediately
            if (TryApplyResolution(actual_display, pending_resolution_, pending_refresh_)) {
                // Start confirmation timer
                show_confirmation_ = true;
                confirmation_start_time_ns_ = utils::get_now_ns();
                confirmation_timer_seconds_ = 30;
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Apply the selected resolution and refresh rate");
    }

    ImGui::SameLine();

    // Save button
    if (display_settings.IsDirty()) {
        if (ImGui::Button("Save Settings")) {
            display_settings.SaveCurrentState();
            display_settings.Save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Save current settings to configuration");
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Button("Save Settings");
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Reset button
    if (display_settings.IsDirty()) {
        if (ImGui::Button("Reset")) {
            display_settings.ResetToLastSaved();
            UpdateCurrentSelectionFromSettings();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to last saved settings");
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Button("Reset");
        ImGui::PopStyleColor();
    }
}

void ResolutionWidget::RefreshDisplayData() {
    int actual_display = GetActualDisplayIndex();
    LogInfo("ResolutionWidget::RefreshDisplayData() - actual_display=%d, selected_resolution_index_=%d", actual_display, selected_resolution_index_);

    // Get resolution labels
    resolution_labels_ = display_cache::g_displayCache.GetResolutionLabels(actual_display);
    resolution_data_.clear();
    LogInfo("ResolutionWidget::RefreshDisplayData() - Found %zu resolution options", resolution_labels_.size());

    // Build resolution data
    for (size_t i = 0; i < resolution_labels_.size(); ++i) {
        ResolutionData data;
        if (i == 0) {
            // Current resolution
            data.is_current = true;
            LogInfo("ResolutionWidget::RefreshDisplayData() - Resolution[%zu]: Current Resolution", i);
        } else {
            // Parse resolution from label
            const std::string& label = resolution_labels_[i];
            size_t x_pos = label.find(" x ");
            if (x_pos != std::string::npos) {
                try {
                    data.width = std::stoi(label.substr(0, x_pos));
                    data.height = std::stoi(label.substr(x_pos + 3));
                    LogInfo("ResolutionWidget::RefreshDisplayData() - Resolution[%zu]: %dx%d", i, data.width, data.height);
                } catch (...) {
                    // Parse failed, use current resolution
                    data.is_current = true;
                    LogInfo("ResolutionWidget::RefreshDisplayData() - Resolution[%zu]: Parse failed, using current", i);
                }
            } else {
                data.is_current = true;
                LogInfo("ResolutionWidget::RefreshDisplayData() - Resolution[%zu]: No 'x' found, using current", i);
            }
        }
        resolution_data_.push_back(data);
    }

    // Get refresh rate labels
    refresh_labels_ = display_cache::g_displayCache.GetRefreshRateLabels(actual_display, selected_resolution_index_);
    refresh_data_.clear();
    LogInfo("ResolutionWidget::RefreshDisplayData() - Found %zu refresh rate options for resolution index %d", refresh_labels_.size(), selected_resolution_index_);

    // Build refresh rate data
    for (size_t i = 0; i < refresh_labels_.size(); ++i) {
        ResolutionData data;
        if (i == 0) {
            // Current refresh rate
            data.is_current = true;
            LogInfo("ResolutionWidget::RefreshDisplayData() - Refresh[%zu]: Current Refresh Rate", i);
        } else {
            // Parse refresh rate from label
            const std::string& label = refresh_labels_[i];
            size_t hz_pos = label.find("Hz");
            if (hz_pos != std::string::npos) {
                try {
                    double hz = std::stod(label.substr(0, hz_pos));
                    // Convert to rational (approximate)
                    data.refresh_numerator = static_cast<int>(hz * 1000);
                    data.refresh_denominator = 1000;
                    LogInfo("ResolutionWidget::RefreshDisplayData() - Refresh[%zu]: %.3f Hz (%d/%d)", i, hz, data.refresh_numerator, data.refresh_denominator);
                } catch (...) {
                    data.is_current = true;
                    LogInfo("ResolutionWidget::RefreshDisplayData() - Refresh[%zu]: Parse failed, using current", i);
                }
            } else {
                data.is_current = true;
                LogInfo("ResolutionWidget::RefreshDisplayData() - Refresh[%zu]: No 'Hz' found, using current", i);
            }
        }
        refresh_data_.push_back(data);
    }
}

void ResolutionWidget::RefreshResolutionData() {
    // Refresh resolution data for current display
    RefreshDisplayData();
}

void ResolutionWidget::RefreshRefreshRateData() {
    int actual_display = GetActualDisplayIndex();

    // Get refresh rate labels for current resolution
    refresh_labels_ = display_cache::g_displayCache.GetRefreshRateLabels(actual_display, selected_resolution_index_);
    refresh_data_.clear();

    // Build refresh rate data
    for (size_t i = 0; i < refresh_labels_.size(); ++i) {
        ResolutionData data;
        if (i == 0) {
            data.is_current = true;
        } else {
            const std::string& label = refresh_labels_[i];
            size_t hz_pos = label.find("Hz");
            if (hz_pos != std::string::npos) {
                try {
                    double hz = std::stod(label.substr(0, hz_pos));
                    data.refresh_numerator = static_cast<int>(hz * 1000);
                    data.refresh_denominator = 1000;
                } catch (...) {
                    data.is_current = true;
                }
            } else {
                data.is_current = true;
            }
        }
        refresh_data_.push_back(data);
    }
}

bool ResolutionWidget::ApplyCurrentSelection() {
    if (resolution_data_.empty() || refresh_data_.empty()) {
        return false;
    }

    int actual_display = GetActualDisplayIndex();
    const ResolutionData& resolution = resolution_data_[selected_resolution_index_];
    const ResolutionData& refresh = refresh_data_[selected_refresh_index_];

    return TryApplyResolution(actual_display, resolution, refresh);
}

bool ResolutionWidget::TryApplyResolution(int display_index, const ResolutionData& resolution, const ResolutionData& refresh) {
    if (resolution.is_current && refresh.is_current) {
        // Both are current, nothing to apply
        return true;
    }

    int width = resolution.width;
    int height = resolution.height;
    int refresh_num = refresh.refresh_numerator;
    int refresh_denom = refresh.refresh_denominator;

    // Get current values if needed
    if (resolution.is_current) {
        if (!display_cache::g_displayCache.GetCurrentResolution(display_index, width, height)) {
            return false;
        }
    }

    if (refresh.is_current) {
        display_cache::RationalRefreshRate current_refresh;
        if (!display_cache::g_displayCache.GetCurrentRefreshRate(display_index, current_refresh)) {
            return false;
        }
        refresh_num = static_cast<int>(current_refresh.numerator);
        refresh_denom = static_cast<int>(current_refresh.denominator);
    }

    // Apply using DXGI first, then fallback to legacy
    if (resolution::ApplyDisplaySettingsDXGI(display_index, width, height, refresh_num, refresh_denom)) {
        return true;
    }

    // Fallback: legacy ChangeDisplaySettingsExW
    const auto* display = display_cache::g_displayCache.GetDisplay(display_index);
    if (!display) return false;
    HMONITOR hmon = display->monitor_handle;
    MONITORINFOEXW mi;
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hmon, &mi)) return false;

    DEVMODEW dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    dm.dmPelsWidth = width;
    dm.dmPelsHeight = height;
    dm.dmDisplayFrequency = static_cast<DWORD>(std::lround(static_cast<double>(refresh_num) / static_cast<double>(refresh_denom)));

    LONG result = ChangeDisplaySettingsExW(mi.szDevice, &dm, nullptr, CDS_UPDATEREGISTRY, nullptr);
    return result == DISP_CHANGE_SUCCESSFUL;
}

void ResolutionWidget::DrawConfirmationDialog() {
    // Calculate remaining time using high-precision timing
    LONGLONG now_ns = utils::get_now_ns();
    LONGLONG elapsed_ns = now_ns - confirmation_start_time_ns_;
    LONGLONG elapsed_seconds = elapsed_ns / utils::SEC_TO_NS;
    int remaining_seconds = confirmation_timer_seconds_ - static_cast<int>(elapsed_seconds);

    if (remaining_seconds <= 0) {
        // Timer expired, revert resolution
        RevertResolution();
        show_confirmation_ = false;
        return;
    }

    // Center the dialog
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    // Create modal dialog
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Resolution Change Confirmation", &show_confirmation_,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {

        // Format the resolution change
        auto formatResolution = [this](const ResolutionData& data) -> std::string {
            if (data.is_current) {
                // Get actual current resolution and refresh rate
                int current_width, current_height;
                display_cache::RationalRefreshRate current_refresh;

                if (display_cache::g_displayCache.GetCurrentResolution(pending_display_index_, current_width, current_height) &&
                    display_cache::g_displayCache.GetCurrentRefreshRate(pending_display_index_, current_refresh)) {

                    std::string resolution = std::to_string(current_width) + "x" + std::to_string(current_height);

                    // Determine refresh rate to use
                    int current_refresh_numerator = data.refresh_numerator;
                    int current_refresh_denominator = data.refresh_denominator;

                    if (current_refresh_numerator == 0) {
                        // Use current refresh rate if no specific refresh rate is set
                        current_refresh_numerator = static_cast<int>(current_refresh.numerator);
                        current_refresh_denominator = static_cast<int>(current_refresh.denominator);
                    }

                    resolution += FormatRefreshRateString(current_refresh_numerator, current_refresh_denominator);
                    return resolution;
                } else {
                    return "Current Resolution";
                }
            }

            std::string resolution = std::to_string(data.width) + "x" + std::to_string(data.height);

            // Determine refresh rate to use
            int current_refresh_numerator = data.refresh_numerator;
            int current_refresh_denominator = data.refresh_denominator;

            if (current_refresh_numerator == 0) {
                // Find current refresh rate
                display_cache::RationalRefreshRate current_refresh;
                if (display_cache::g_displayCache.GetCurrentRefreshRate(pending_display_index_, current_refresh)) {
                    current_refresh_numerator = static_cast<int>(current_refresh.numerator);
                    current_refresh_denominator = static_cast<int>(current_refresh.denominator);
                }
            }

            resolution += FormatRefreshRateString(current_refresh_numerator, current_refresh_denominator);
            return resolution;
        };

        std::string resolution_str = formatResolution(pending_resolution_);

        // Display the change
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Resolution changed to:");
        ImGui::Text("Resolution: %s", resolution_str.c_str());

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Auto Revert: %ds", remaining_seconds);

        ImGui::Spacing();

        // Buttons
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        if (ImGui::Button("Confirm", ImVec2(100, 30))) {
            // User confirmed, save the settings
            auto& display_settings = g_resolution_settings->GetDisplaySettings(pending_display_index_);
            display_settings.SetCurrentState(pending_resolution_);
            display_settings.SaveCurrentState();
            display_settings.Save();
            show_confirmation_ = false;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Button("Revert", ImVec2(100, 30))) {
            // User reverted, restore previous resolution
            RevertResolution();
            show_confirmation_ = false;
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
}

void ResolutionWidget::RevertResolution() {
    // Apply the stored previous resolution
    if (previous_resolution_.width > 0 && previous_resolution_.height > 0) {
        TryApplyResolution(pending_display_index_, previous_resolution_, previous_refresh_);
    } else {
        // Fallback: get current resolution and apply it
        int current_width, current_height;
        display_cache::RationalRefreshRate current_refresh;

        if (display_cache::g_displayCache.GetCurrentResolution(pending_display_index_, current_width, current_height) &&
            display_cache::g_displayCache.GetCurrentRefreshRate(pending_display_index_, current_refresh)) {

            ResolutionData current_res;
            current_res.width = current_width;
            current_res.height = current_height;
            current_res.refresh_numerator = static_cast<int>(current_refresh.numerator);
            current_res.refresh_denominator = static_cast<int>(current_refresh.denominator);
            current_res.is_current = true;

            TryApplyResolution(pending_display_index_, current_res, current_res);
        }
    }
}

std::string ResolutionWidget::GetDisplayName(int display_index) const {
    if (display_index == 0) {
        // Format Auto (Current) option with detailed info like legacy format
        std::string auto_label = "Auto (Current)";
        HWND hwnd = g_last_swapchain_hwnd.load();
        if (hwnd) {
            HMONITOR current_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (current_monitor) {
                const auto* display = display_cache::g_displayCache.GetDisplayByHandle(current_monitor);
                if (display) {
                    int width = display->width;
                    int height = display->height;
                    double refresh_rate = display->current_refresh_rate.ToHz();

                    // Format refresh rate with precision
                    std::ostringstream rate_oss;
                    rate_oss << std::fixed << std::setprecision(6) << refresh_rate;
                    std::string rate_str = rate_oss.str();

                    // Remove trailing zeros and decimal point if not needed
                    rate_str.erase(rate_str.find_last_not_of('0') + 1, std::string::npos);
                    if (rate_str.back() == '.') {
                        rate_str.pop_back();
                    }

                    // Use cached primary monitor flag and device name
                    bool is_primary = display->is_primary;
                    std::string primary_text = is_primary ? " Primary" : "";
                    std::string device_name(display->device_name.begin(), display->device_name.end());

                    auto_label = "Auto (Current) [" + device_name + "] " +
                                std::to_string(width) + "x" + std::to_string(height) +
                                "@" + rate_str + "Hz" + primary_text;
                }
            }
        }
        return auto_label;
    }

    auto displays = display_cache::g_displayCache.GetDisplays();
    if (displays && display_index <= static_cast<int>(displays->size())) {
        const auto* display = (*displays)[display_index - 1].get();
        if (display) {
            // Format with resolution, refresh rate, and primary status like Auto Current
            int width = display->width;
            int height = display->height;
            double refresh_rate = display->current_refresh_rate.ToHz();

            // Format refresh rate with precision
            std::ostringstream rate_oss;
            rate_oss << std::fixed << std::setprecision(6) << refresh_rate;
            std::string rate_str = rate_oss.str();

            // Remove trailing zeros and decimal point if not needed
            rate_str.erase(rate_str.find_last_not_of('0') + 1, std::string::npos);
            if (rate_str.back() == '.') {
                rate_str.pop_back();
            }

            // Use cached primary monitor flag and device name
            bool is_primary = display->is_primary;
            std::string primary_text = is_primary ? " Primary" : "";
            std::string device_name(display->device_name.begin(), display->device_name.end());

            std::string name = "[" + device_name + "] " +
            std::to_string(width) + "x" + std::to_string(height) +
            "@" + rate_str + "Hz" + primary_text;

            return name;
        }
    }

    return "Display " + std::to_string(display_index);
}

int ResolutionWidget::GetActualDisplayIndex() const {
    if (selected_display_index_ == 0) {
        // Auto (Current) - find current monitor
        HWND hwnd = g_last_swapchain_hwnd.load();
        if (hwnd) {
            HMONITOR current_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (current_monitor) {
                auto displays = display_cache::g_displayCache.GetDisplays();
                if (displays) {
                    for (size_t i = 0; i < displays->size(); ++i) {
                        const auto* display = (*displays)[i].get();
                        if (display && display->monitor_handle == current_monitor) {
                            return static_cast<int>(i);
                        }
                    }
                }
            }
        }
        return 0; // Fallback to first display
    } else {
        return selected_display_index_ - 1;
    }
}

void ResolutionWidget::UpdateCurrentSelectionFromSettings() {
    int actual_display = GetActualDisplayIndex();
    auto& display_settings = g_resolution_settings->GetDisplaySettings(actual_display);
    const ResolutionData& current_state = display_settings.GetCurrentState();

    LogInfo("ResolutionWidget::UpdateCurrentSelectionFromSettings() - actual_display=%d, current_state: %dx%d @ %d/%d, is_current=%s",
            actual_display, current_state.width, current_state.height,
            current_state.refresh_numerator, current_state.refresh_denominator,
            current_state.is_current ? "true" : "false");

    // Find matching resolution index
    selected_resolution_index_ = 0;
    if (!current_state.is_current && current_state.width > 0 && current_state.height > 0) {
        // Look for exact width/height match
        for (size_t i = 0; i < resolution_data_.size(); ++i) {
            const auto& res = resolution_data_[i];
            if (!res.is_current && res.width == current_state.width && res.height == current_state.height) {
                selected_resolution_index_ = static_cast<int>(i);
                LogInfo("ResolutionWidget::UpdateCurrentSelectionFromSettings() - Found exact resolution match at index %d: %dx%d",
                        selected_resolution_index_, res.width, res.height);
                break;
            }
        }
    }

    // Find matching refresh rate index
    selected_refresh_index_ = 0;
    if (!current_state.is_current && current_state.refresh_numerator > 0 && current_state.refresh_denominator > 0) {
        // Look for refresh rate match (need to refresh data first to get refresh options for selected resolution)
        RefreshDisplayData();

        for (size_t i = 0; i < refresh_data_.size(); ++i) {
            const auto& refresh = refresh_data_[i];
            if (!refresh.is_current &&
                refresh.refresh_numerator == current_state.refresh_numerator &&
                refresh.refresh_denominator == current_state.refresh_denominator) {
                selected_refresh_index_ = static_cast<int>(i);
                LogInfo("ResolutionWidget::UpdateCurrentSelectionFromSettings() - Found exact refresh rate match at index %d: %d/%d",
                        selected_refresh_index_, refresh.refresh_numerator, refresh.refresh_denominator);
                break;
            }
        }
    }

    LogInfo("ResolutionWidget::UpdateCurrentSelectionFromSettings() - Set UI indices: display=%d, resolution=%d, refresh=%d",
            selected_display_index_, selected_resolution_index_, selected_refresh_index_);

    // Apply the loaded settings if they are not "current" (i.e., they are specific resolution/refresh rate settings)
    if (!current_state.is_current && current_state.width > 0 && current_state.height > 0) {
        LogInfo("ResolutionWidget::UpdateCurrentSelectionFromSettings() - Applying loaded resolution settings: %dx%d @ %d/%d",
                current_state.width, current_state.height, current_state.refresh_numerator, current_state.refresh_denominator);

        // Create resolution and refresh data from loaded settings
        ResolutionData resolution_data;
        resolution_data.width = current_state.width;
        resolution_data.height = current_state.height;
        resolution_data.is_current = false;

        ResolutionData refresh_data;
        refresh_data.refresh_numerator = current_state.refresh_numerator;
        refresh_data.refresh_denominator = current_state.refresh_denominator;
        refresh_data.is_current = false;

        // Apply the settings
        if (TryApplyResolution(actual_display, resolution_data, refresh_data)) {
            LogInfo("ResolutionWidget::UpdateCurrentSelectionFromSettings() - Successfully applied loaded resolution settings");
        } else {
            LogError("ResolutionWidget::UpdateCurrentSelectionFromSettings() - Failed to apply loaded resolution settings");
        }
    } else {
        LogInfo("ResolutionWidget::UpdateCurrentSelectionFromSettings() - Skipping resolution application (is_current=%s, width=%d, height=%d)",
                current_state.is_current ? "true" : "false", current_state.width, current_state.height);
    }
}

void ResolutionWidget::UpdateSettingsFromCurrentSelection() {
    if (resolution_data_.empty() || refresh_data_.empty()) {
        return;
    }

    int actual_display = GetActualDisplayIndex();
    auto& display_settings = g_resolution_settings->GetDisplaySettings(actual_display);

    // Create combined resolution data
    ResolutionData combined = resolution_data_[selected_resolution_index_];
    if (!refresh_data_[selected_refresh_index_].is_current) {
        combined.refresh_numerator = refresh_data_[selected_refresh_index_].refresh_numerator;
        combined.refresh_denominator = refresh_data_[selected_refresh_index_].refresh_denominator;
    }

    display_settings.SetCurrentState(combined);
}

void ResolutionWidget::CaptureOriginalSettings() {
    if (original_settings_.captured) {
        return; // Already captured
    }

    // Try to get current monitor from game window first
    HWND hwnd = g_last_swapchain_hwnd.load();
    HMONITOR current_monitor = nullptr;

    if (hwnd) {
        current_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    }

    // If no game window available, try to get primary monitor as fallback
    if (!current_monitor) {
        current_monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    }

    if (!current_monitor) {
        return;
    }

    // Get display info from cache
    const auto* display = display_cache::g_displayCache.GetDisplayByHandle(current_monitor);
    if (!display) {
        return;
    }

    // Capture original settings
    original_settings_.width = display->width;
    original_settings_.height = display->height;
    original_settings_.refresh_numerator = static_cast<int>(display->current_refresh_rate.numerator);
    original_settings_.refresh_denominator = static_cast<int>(display->current_refresh_rate.denominator);
    original_settings_.device_name = std::string(display->device_name.begin(), display->device_name.end());
    original_settings_.is_primary = display->is_primary;
    original_settings_.captured = true;

    // Mark this display for restore tracking
    display_restore::MarkOriginalForMonitor(current_monitor);
}

std::string ResolutionWidget::FormatOriginalSettingsString() const {
    if (!original_settings_.captured) {
        // Show debug info about why capture failed
        HWND hwnd = g_last_swapchain_hwnd.load();
        if (!hwnd) {
            return "Original settings not captured (no game window)";
        }

        HMONITOR current_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (!current_monitor) {
            return "Original settings not captured (no monitor)";
        }

        const auto* display = display_cache::g_displayCache.GetDisplayByHandle(current_monitor);
        if (!display) {
            return "Original settings not captured (no display cache)";
        }

        return "Original settings not captured (unknown reason)";
    }

    // Format refresh rate
    std::string refresh_str = "";
    if (original_settings_.refresh_numerator > 0 && original_settings_.refresh_denominator > 0) {
        double refresh_hz = static_cast<double>(original_settings_.refresh_numerator) /
            static_cast<double>(original_settings_.refresh_denominator);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << refresh_hz;
        refresh_str = oss.str();

        // Remove trailing zeros and decimal point if not needed
        refresh_str.erase(refresh_str.find_last_not_of('0') + 1, std::string::npos);
        if (refresh_str.back() == '.') {
            refresh_str.pop_back();
        }
        refresh_str = "@" + refresh_str + "Hz";
    }

    std::string primary_text = original_settings_.is_primary ? " Primary" : "";

    return "[" + original_settings_.device_name + "] " +
    std::to_string(original_settings_.width) + "x" + std::to_string(original_settings_.height) +
    refresh_str + primary_text;
}

void ResolutionWidget::DrawOriginalSettingsInfo() {
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Original Settings:");
    ImGui::SameLine();
    ImGui::Text("%s", FormatOriginalSettingsString().c_str());
}

void ResolutionWidget::DrawAutoRestoreCheckbox() {
    bool auto_restore = s_auto_restore_resolution_on_close.load();
    if (ImGui::Checkbox("Auto-restore on exit (WIP - not working)", &auto_restore)) {
        s_auto_restore_resolution_on_close.store(auto_restore);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically restore original display settings when the game closes");
    }
}

// Global functions
void InitializeResolutionWidget() {
    if (!g_resolution_widget) {
        g_resolution_widget = std::make_unique<ResolutionWidget>();
        g_resolution_widget->Initialize();
    }
}

void CleanupResolutionWidget() {
    if (g_resolution_widget) {
        g_resolution_widget->Cleanup();
        g_resolution_widget.reset();
    }
}

void DrawResolutionWidget() {
    if (g_resolution_widget) {
        g_resolution_widget->OnDraw();
    }
}

} // namespace display_commander::widgets::resolution_widget
