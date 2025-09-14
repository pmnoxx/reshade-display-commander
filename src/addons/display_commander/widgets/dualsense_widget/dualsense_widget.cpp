#include "dualsense_widget.hpp"
#include "../../utils.hpp"
#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>
#include <chrono>
#include <vector>
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <initguid.h>
#include <cstring>

// Define GUID_DEVINTERFACE_HID if not already defined
#ifndef GUID_DEVINTERFACE_HID
DEFINE_GUID(GUID_DEVINTERFACE_HID, 0x4d1e55b2, 0xf16f, 0x11cf, 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);
#endif

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace display_commander::widgets::dualsense_widget {

// Global shared state
std::shared_ptr<DualSenseSharedState> DualSenseWidget::g_shared_state = nullptr;

// Global widget instance
std::unique_ptr<DualSenseWidget> g_dualsense_widget = nullptr;

DualSenseWidget::DualSenseWidget() {
    // Initialize shared state if not already done
    if (!g_shared_state) {
        g_shared_state = std::make_shared<DualSenseSharedState>();
    }
}

void DualSenseWidget::Initialize() {
    if (is_initialized_) return;

    LogInfo("DualSenseWidget::Initialize() - Starting DualSense widget initialization");

    // Load settings
    LoadSettings();

    // Initialize HID wrapper
    display_commander::dualsense::InitializeDualSenseHID();

    is_initialized_ = true;
    LogInfo("DualSenseWidget::Initialize() - DualSense widget initialization complete");
}

void DualSenseWidget::Cleanup() {
    if (!is_initialized_) return;

    // Save settings
    SaveSettings();

    // Cleanup HID wrapper
    display_commander::dualsense::CleanupDualSenseHID();

    is_initialized_ = false;
}

void DualSenseWidget::OnDraw() {
    if (!is_initialized_) {
        Initialize();
    }

    if (!g_shared_state) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "DualSense shared state not initialized");
        return;
    }

    // Draw the DualSense widget UI
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "=== DualSense Controller Monitor ===");
    ImGui::Spacing();

    // Draw settings
    DrawSettings();
    ImGui::Spacing();

    // Draw event counters
    DrawEventCounters();
    ImGui::Spacing();

    // Draw device list
    DrawDeviceList();
    ImGui::Spacing();

    // Draw selected device info
    DrawDeviceInfo();
}

void DualSenseWidget::DrawSettings() {
    if (ImGui::CollapsingHeader("DualSense Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Enable DualSense detection
        bool enable_detection = g_shared_state->enable_dualsense_detection.load();
        if (ImGui::Checkbox("Enable DualSense Detection", &enable_detection)) {
            g_shared_state->enable_dualsense_detection.store(enable_detection);
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable detection and monitoring of DualSense controllers");
        }

        if (enable_detection) {
            // Show device IDs
            bool show_device_ids = g_shared_state->show_device_ids.load();
            if (ImGui::Checkbox("Show Device IDs", &show_device_ids)) {
                g_shared_state->show_device_ids.store(show_device_ids);
                SaveSettings();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Display vendor and product IDs for each device");
            }

            // Show connection type
            bool show_connection_type = g_shared_state->show_connection_type.load();
            if (ImGui::Checkbox("Show Connection Type", &show_connection_type)) {
                g_shared_state->show_connection_type.store(show_connection_type);
                SaveSettings();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Display whether device is connected via USB or Bluetooth");
            }

            // Show battery info
            bool show_battery_info = g_shared_state->show_battery_info.load();
            if (ImGui::Checkbox("Show Battery Information", &show_battery_info)) {
                g_shared_state->show_battery_info.store(show_battery_info);
                SaveSettings();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Display battery level and charging status");
            }

            // Show advanced features
            bool show_advanced_features = g_shared_state->show_advanced_features.load();
            if (ImGui::Checkbox("Show Advanced Features", &show_advanced_features)) {
                g_shared_state->show_advanced_features.store(show_advanced_features);
                SaveSettings();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Display DualSense-specific features like adaptive triggers and touchpad");
            }

            ImGui::Spacing();

            // HID device type selection
            int hid_type = g_shared_state->selected_hid_type.load();
            const char* hid_types[] = {
                "Auto (All Supported)",
                "DualSense Regular Only",
                "DualSense Edge Only",
                "DualShock 4 Only",
                "All Sony Controllers"
            };

            if (ImGui::Combo("Device Type Filter", &hid_type, hid_types, 5)) {
                g_shared_state->selected_hid_type.store(hid_type);
                display_commander::dualsense::g_dualsense_hid_wrapper->SetHIDTypeFilter(hid_type);
                SaveSettings();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Select which type of Sony controllers to detect and monitor");
            }

            ImGui::Spacing();

            // Manual refresh button
            if (ImGui::Button("Refresh Device List")) {
                display_commander::dualsense::EnumerateDualSenseDevices();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Manually refresh the list of connected devices");
            }
        }
    }
}

void DualSenseWidget::DrawEventCounters() {
    if (ImGui::CollapsingHeader("Event Counters", ImGuiTreeNodeFlags_DefaultOpen)) {
        uint64_t total_events = g_shared_state->total_events.load();
        uint64_t button_events = g_shared_state->button_events.load();
        uint64_t stick_events = g_shared_state->stick_events.load();
        uint64_t trigger_events = g_shared_state->trigger_events.load();
        uint64_t touchpad_events = g_shared_state->touchpad_events.load();

        ImGui::Text("Total Events: %llu", total_events);
        ImGui::Text("Button Events: %llu", button_events);
        ImGui::Text("Stick Events: %llu", stick_events);
        ImGui::Text("Trigger Events: %llu", trigger_events);
        ImGui::Text("Touchpad Events: %llu", touchpad_events);

        // Reset button
        if (ImGui::Button("Reset Counters")) {
            g_shared_state->total_events.store(0);
            g_shared_state->button_events.store(0);
            g_shared_state->stick_events.store(0);
            g_shared_state->trigger_events.store(0);
            g_shared_state->touchpad_events.store(0);
        }
    }
}

void DualSenseWidget::DrawDeviceList() {
    if (ImGui::CollapsingHeader("Connected Devices", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!g_shared_state->enable_dualsense_detection.load()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "DualSense detection is disabled");
            return;
        }

        // Update device states periodically
        static auto last_update = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count() > 100) {
            UpdateDeviceStates();
            last_update = now;
        }

        // Get devices from HID wrapper
        const auto& hid_devices = display_commander::dualsense::g_dualsense_hid_wrapper->GetDevices();
        g_shared_state->devices.assign(hid_devices.begin(), hid_devices.end());
        const auto& devices = g_shared_state->devices;

        if (devices.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No DualSense devices detected");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Make sure your DualSense controller is connected via USB or Bluetooth");
        } else {
            ImGui::Text("Found %zu DualSense device(s):", devices.size());
            ImGui::Spacing();

            for (size_t i = 0; i < devices.size(); ++i) {
                const auto& device = devices[i];

                ImGui::PushID(static_cast<int>(i));

                // Device status indicator
                ImVec4 status_color = device.is_connected ?
                    ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                    ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

                ImGui::TextColored(status_color, "●");
                ImGui::SameLine();

                // Device name and basic info
                std::string device_name = device.device_name.empty() ?
                    "DualSense Controller" : device.device_name;

                if (g_shared_state->show_connection_type.load()) {
                    device_name += " (" + device.connection_type + ")";
                }

                if (g_shared_state->show_device_ids.load()) {
                    char vid_str[16], pid_str[16];
                    sprintf_s(vid_str, "%04X", device.vendor_id);
                    sprintf_s(pid_str, "%04X", device.product_id);
                    device_name += " [VID:0x" + std::string(vid_str) +
                        " PID:0x" + std::string(pid_str) + "]";
                }

                if (ImGui::Selectable(device_name.c_str(), selected_device_ == static_cast<int>(i))) {
                    selected_device_ = static_cast<int>(i);
                }

                // Show additional info on hover
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Click to select this device for detailed view");
                }

                ImGui::PopID();
            }
        }
    }
}

void DualSenseWidget::DrawDeviceInfo() {
    if (selected_device_ < 0 || selected_device_ >= static_cast<int>(g_shared_state->devices.size())) {
        return;
    }

    const auto& device = g_shared_state->devices[selected_device_];

    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "=== Device Details ===");
    ImGui::Spacing();

    DrawDeviceDetails(device);
}

void DualSenseWidget::DrawDeviceDetails(const DualSenseDeviceInfo& device) {
    // Basic device information
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Device: %s",
                       device.device_name.empty() ? "DualSense Controller" : device.device_name.c_str());

    ImGui::Text("Connection: %s", device.connection_type.c_str());
    ImGui::Text("Vendor ID: 0x%04X", device.vendor_id);
    ImGui::Text("Product ID: 0x%04X", device.product_id);
    ImGui::Text("Status: %s", device.is_connected ? "Connected" : "Disconnected");

    if (device.is_wireless) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Wireless: Yes");
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Wireless: No (USB)");
    }

    ImGui::Spacing();

    // Device type information
    ImGui::Text("Device Type: %s", GetDeviceTypeString(device).c_str());

    // Last update time
    if (device.last_update_time > 0) {
        DWORD now = GetTickCount();
        DWORD age_ms = now - device.last_update_time;
        ImGui::Text("Last Update: %lu ms ago", age_ms);
    }

    ImGui::Spacing();

    // Controller state (if connected)
    if (device.is_connected) {
        DrawButtonStates(device);
        ImGui::Spacing();
        DrawStickStates(device);
        ImGui::Spacing();
        DrawTriggerStates(device);
        ImGui::Spacing();
    }

    // Battery information
    if (g_shared_state->show_battery_info.load()) {
        DrawBatteryStatus(device);
        ImGui::Spacing();
    }

    // Advanced features
    if (g_shared_state->show_advanced_features.load()) {
        DrawAdvancedFeatures(device);
    }

    // Input report debug display
    if (device.hid_device && device.hid_device->input_report.size() > 0) {
        ImGui::Text("Input Report Size: %zu bytes", device.hid_device->input_report.size());
        ImGui::Text("First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
                   device.hid_device->input_report[0], device.hid_device->input_report[1],
                   device.hid_device->input_report[2], device.hid_device->input_report[3],
                   device.hid_device->input_report[4], device.hid_device->input_report[5],
                   device.hid_device->input_report[6], device.hid_device->input_report[7]);
    } else {
        ImGui::Text("No input report data available");
    }
    ImGui::Spacing();

    DrawInputReport(device);

    // Raw input parsing with debug offset
    DrawRawButtonStates(device);
    ImGui::Spacing();
    DrawRawStickStates(device);
    ImGui::Spacing();
    DrawRawTriggerStates(device);
    ImGui::Spacing();
}

void DualSenseWidget::DrawButtonStates(const DualSenseDeviceInfo& device) {
    if (ImGui::CollapsingHeader("Buttons", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Parse buttons from input report with debug offset
        WORD buttons = 0;
        int offset = g_shared_state->debug_offset.load();

        if (device.hid_device && device.hid_device->input_report.size() > 0) {
            const auto& inputReport = device.hid_device->input_report;

            // Buttons 1 (with offset)
            if (1 + offset < inputReport.size()) {
                BYTE button1 = inputReport[1 + offset];
                if (button1 & 0x10) buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;  // L1
                if (button1 & 0x20) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER; // R1
                if (button1 & 0x01) buttons |= XINPUT_GAMEPAD_X;              // Square -> X
                if (button1 & 0x02) buttons |= XINPUT_GAMEPAD_A;              // Cross -> A
                if (button1 & 0x04) buttons |= XINPUT_GAMEPAD_B;              // Circle -> B
                if (button1 & 0x08) buttons |= XINPUT_GAMEPAD_Y;              // Triangle -> Y
            }

            // Buttons 2 (with offset)
            if (2 + offset < inputReport.size()) {
                BYTE button2 = inputReport[2 + offset];
                if (button2 & 0x01) buttons |= XINPUT_GAMEPAD_BACK;           // Share
                if (button2 & 0x02) buttons |= XINPUT_GAMEPAD_START;          // Options
                if (button2 & 0x04) buttons |= XINPUT_GAMEPAD_LEFT_THUMB;     // L3
                if (button2 & 0x08) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;    // R3
                if (button2 & 0x10) buttons |= 0x0400;                        // PS (custom constant)
            }

            // D-Pad (with offset)
            if (3 + offset < inputReport.size()) {
                BYTE dpad = inputReport[3 + offset] & 0x0F;
                buttons |= dpad; // D-Pad is already in the correct format
            }
        } else {
            // Fallback to XInput data if no input report
            buttons = device.current_state.Gamepad.wButtons;
        }

        // Create a grid of buttons
        const struct {
            WORD mask;
            const char* name;
        } button_list[] = {
            {XINPUT_GAMEPAD_A, "A"},
            {XINPUT_GAMEPAD_B, "B"},
            {XINPUT_GAMEPAD_X, "X"},
            {XINPUT_GAMEPAD_Y, "Y"},
            {XINPUT_GAMEPAD_LEFT_SHOULDER, "LB"},
            {XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB"},
            {XINPUT_GAMEPAD_BACK, "Back"},
            {XINPUT_GAMEPAD_START, "Start"},
            {XINPUT_GAMEPAD_LEFT_THUMB, "LS"},
            {XINPUT_GAMEPAD_RIGHT_THUMB, "RS"},
            {XINPUT_GAMEPAD_DPAD_UP, "D-Up"},
            {XINPUT_GAMEPAD_DPAD_DOWN, "D-Down"},
            {XINPUT_GAMEPAD_DPAD_LEFT, "D-Left"},
            {XINPUT_GAMEPAD_DPAD_RIGHT, "D-Right"},
        };

        ImGui::Text("Using offset: %d", offset);
        for (size_t i = 0; i < sizeof(button_list) / sizeof(button_list[0]); i += 2) {
            if (i + 1 < sizeof(button_list) / sizeof(button_list[0])) {
                // Draw two buttons per row
                bool pressed1 = IsButtonPressed(buttons, button_list[i].mask);
                bool pressed2 = IsButtonPressed(buttons, button_list[i + 1].mask);

                ImGui::PushStyleColor(ImGuiCol_Button, pressed1 ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::Button(button_list[i].name, ImVec2(60, 30));
                ImGui::PopStyleColor();

                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button, pressed2 ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::Button(button_list[i + 1].name, ImVec2(60, 30));
                ImGui::PopStyleColor();
            } else {
                // Single button on last row
                bool pressed = IsButtonPressed(buttons, button_list[i].mask);

                ImGui::PushStyleColor(ImGuiCol_Button, pressed ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::Button(button_list[i].name, ImVec2(60, 30));
                ImGui::PopStyleColor();
            }
        }
    }
}

void DualSenseWidget::DrawStickStates(const DualSenseDeviceInfo& device) {
    if (ImGui::CollapsingHeader("Analog Sticks", ImGuiTreeNodeFlags_DefaultOpen)) {
        int offset = g_shared_state->debug_offset.load();

        // Parse stick data from input report with debug offset
        SHORT leftX = 0, leftY = 0, rightX = 0, rightY = 0;

        if (device.hid_device && device.hid_device->input_report.size() > 0) {
            const auto& inputReport = device.hid_device->input_report;

            // Left stick (with offset)
            if (4 + offset + 1 < inputReport.size()) {
                leftX = static_cast<SHORT>((inputReport[5 + offset] << 8) | inputReport[4 + offset]);
                leftY = static_cast<SHORT>((inputReport[7 + offset] << 8) | inputReport[6 + offset]);
            }

            // Right stick (with offset)
            if (8 + offset + 1 < inputReport.size()) {
                rightX = static_cast<SHORT>((inputReport[9 + offset] << 8) | inputReport[8 + offset]);
                rightY = static_cast<SHORT>((inputReport[11 + offset] << 8) | inputReport[10 + offset]);
            }
        } else {
            // Fallback to XInput data if no input report
            leftX = device.current_state.Gamepad.sThumbLX;
            leftY = device.current_state.Gamepad.sThumbLY;
            rightX = device.current_state.Gamepad.sThumbRX;
            rightY = device.current_state.Gamepad.sThumbRY;
        }

        // Left stick
        ImGui::Text("Left Stick:");
        float lx = ShortToFloat(leftX);
        float ly = ShortToFloat(leftY);
        ImGui::Text("X: %.3f (Raw: %d)", lx, leftX);
        ImGui::Text("Y: %.3f (Raw: %d)", ly, leftY);

        // Visual representation
        ImGui::Text("Position:");
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImVec2(100, 100);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Draw circle
        ImVec2 center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
        draw_list->AddCircle(center, canvas_size.x * 0.4f, ImColor(100, 100, 100, 255), 32, 2.0f);

        // Draw crosshairs
        draw_list->AddLine(ImVec2(canvas_pos.x, center.y), ImVec2(canvas_pos.x + canvas_size.x, center.y), ImColor(100, 100, 100, 255), 1.0f);
        draw_list->AddLine(ImVec2(center.x, canvas_pos.y), ImVec2(center.x, canvas_pos.y + canvas_size.y), ImColor(100, 100, 100, 255), 1.0f);

        // Draw stick position
        ImVec2 stick_pos = ImVec2(center.x + lx * canvas_size.x * 0.4f, center.y - ly * canvas_size.y * 0.4f);
        draw_list->AddCircleFilled(stick_pos, 5.0f, ImColor(0, 255, 0, 255));

        ImGui::Dummy(canvas_size);

        // Right stick
        ImGui::Text("Right Stick:");
        float rx = ShortToFloat(rightX);
        float ry = ShortToFloat(rightY);
        ImGui::Text("X: %.3f (Raw: %d)", rx, rightX);
        ImGui::Text("Y: %.3f (Raw: %d)", ry, rightY);

        // Visual representation for right stick
        ImGui::Text("Position:");
        canvas_pos = ImGui::GetCursorScreenPos();
        draw_list = ImGui::GetWindowDrawList();

        // Draw circle
        center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
        draw_list->AddCircle(center, canvas_size.x * 0.4f, ImColor(100, 100, 100, 255), 32, 2.0f);

        // Draw crosshairs
        draw_list->AddLine(ImVec2(canvas_pos.x, center.y), ImVec2(canvas_pos.x + canvas_size.x, center.y), ImColor(100, 100, 100, 255), 1.0f);
        draw_list->AddLine(ImVec2(center.x, canvas_pos.y), ImVec2(center.x, canvas_pos.y + canvas_size.y), ImColor(100, 100, 100, 255), 1.0f);

        // Draw stick position
        stick_pos = ImVec2(center.x + rx * canvas_size.x * 0.4f, center.y - ry * canvas_size.y * 0.4f);
        draw_list->AddCircleFilled(stick_pos, 5.0f, ImColor(0, 255, 0, 255));

        ImGui::Dummy(canvas_size);

        ImGui::Text("Using offset: %d", offset);
    }
}

void DualSenseWidget::DrawTriggerStates(const DualSenseDeviceInfo& device) {
    if (ImGui::CollapsingHeader("Triggers", ImGuiTreeNodeFlags_DefaultOpen)) {
        int offset = g_shared_state->debug_offset.load();

        // Parse trigger data from input report with debug offset
        USHORT leftTrigger = 0, rightTrigger = 0;

        if (device.hid_device && device.hid_device->input_report.size() > 0) {
            const auto& inputReport = device.hid_device->input_report;

            // Left trigger (with offset)
            if (12 + offset + 1 < inputReport.size()) {
                leftTrigger = static_cast<USHORT>((inputReport[13 + offset] << 8) | inputReport[12 + offset]);
            }

            // Right trigger (with offset)
            if (14 + offset + 1 < inputReport.size()) {
                rightTrigger = static_cast<USHORT>((inputReport[15 + offset] << 8) | inputReport[14 + offset]);
            }
        } else {
            // Fallback to XInput data if no input report
            leftTrigger = device.current_state.Gamepad.bLeftTrigger;
            rightTrigger = device.current_state.Gamepad.bRightTrigger;
        }

        // Left trigger
        ImGui::Text("Left Trigger: %u/65535 (%.1f%%)",
                   leftTrigger,
                   (static_cast<float>(leftTrigger) / 65535.0f) * 100.0f);

        // Visual bar for left trigger
        float left_trigger_norm = static_cast<float>(leftTrigger) / 65535.0f;
        ImGui::ProgressBar(left_trigger_norm, ImVec2(-1, 0), "");

        // Right trigger
        ImGui::Text("Right Trigger: %u/65535 (%.1f%%)",
                   rightTrigger,
                   (static_cast<float>(rightTrigger) / 65535.0f) * 100.0f);

        // Visual bar for right trigger
        float right_trigger_norm = static_cast<float>(rightTrigger) / 65535.0f;
        ImGui::ProgressBar(right_trigger_norm, ImVec2(-1, 0), "");

        ImGui::Text("Using offset: %d", offset);
    }
}

void DualSenseWidget::DrawBatteryStatus(const DualSenseDeviceInfo& device) {
    if (ImGui::CollapsingHeader("Battery Status", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!device.battery_info_valid) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Battery information not available");
            return;
        }

        // Battery level
        std::string level_str;
        ImVec4 level_color(1.0f, 1.0f, 1.0f, 1.0f);
        float level_progress = 0.0f;

        switch (device.battery_level) {
            case 0: level_str = "Empty"; level_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); level_progress = 0.0f; break;
            case 1: level_str = "Low"; level_color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); level_progress = 0.25f; break;
            case 2: level_str = "Medium"; level_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); level_progress = 0.5f; break;
            case 3: level_str = "High"; level_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); level_progress = 0.75f; break;
            case 4: level_str = "Full"; level_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); level_progress = 1.0f; break;
            default: level_str = "Unknown"; level_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); level_progress = 0.0f; break;
        }

        ImGui::TextColored(level_color, "Level: %s", level_str.c_str());

        // Visual battery level bar
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, level_color);
        ImGui::ProgressBar(level_progress, ImVec2(-1, 0), "");
        ImGui::PopStyleColor();
    }
}

void DualSenseWidget::DrawAdvancedFeatures(const DualSenseDeviceInfo& device) {
    if (ImGui::CollapsingHeader("Advanced Features", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Adaptive Triggers: %s", device.has_adaptive_triggers ? "Yes" : "No");
        ImGui::Text("Touchpad: %s", device.has_touchpad ? "Yes" : "No");
        ImGui::Text("Microphone: %s", device.has_microphone ? "Yes" : "No");
        ImGui::Text("Speaker: %s", device.has_speaker ? "Yes" : "No");

        if (device.has_touchpad) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Touchpad input not yet implemented");
        }

        if (device.has_adaptive_triggers) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Adaptive trigger control not yet implemented");
        }
    }
}

std::string DualSenseWidget::GetButtonName(WORD button) const {
    switch (button) {
        case XINPUT_GAMEPAD_A: return "A";
        case XINPUT_GAMEPAD_B: return "B";
        case XINPUT_GAMEPAD_X: return "X";
        case XINPUT_GAMEPAD_Y: return "Y";
        case XINPUT_GAMEPAD_LEFT_SHOULDER: return "LB";
        case XINPUT_GAMEPAD_RIGHT_SHOULDER: return "RB";
        case XINPUT_GAMEPAD_BACK: return "Back";
        case XINPUT_GAMEPAD_START: return "Start";
        case XINPUT_GAMEPAD_LEFT_THUMB: return "LS";
        case XINPUT_GAMEPAD_RIGHT_THUMB: return "RS";
        case XINPUT_GAMEPAD_DPAD_UP: return "D-Up";
        case XINPUT_GAMEPAD_DPAD_DOWN: return "D-Down";
        case XINPUT_GAMEPAD_DPAD_LEFT: return "D-Left";
        case XINPUT_GAMEPAD_DPAD_RIGHT: return "D-Right";
        default: return "Unknown";
    }
}

std::string DualSenseWidget::GetDeviceStatus(const DualSenseDeviceInfo& device) const {
    return device.is_connected ? "Connected" : "Disconnected";
}

bool DualSenseWidget::IsButtonPressed(WORD buttons, WORD button) const {
    return (buttons & button) != 0;
}

std::string DualSenseWidget::GetConnectionTypeString(const DualSenseDeviceInfo& device) const {
    return device.connection_type;
}

std::string DualSenseWidget::GetDeviceTypeString(const DualSenseDeviceInfo& device) const {
    if (device.vendor_id == 0x054c) { // Sony
        switch (device.product_id) {
            case 0x0CE6: return "DualSense Controller";           // Regular DualSense
            case 0x0DF2: return "DualSense Edge Controller";      // DualSense Edge
            case 0x05C4: return "DualShock 4 Controller";
            case 0x09CC: return "DualShock 4 Controller (Rev 2)";
            case 0x0BA0: return "DualShock 4 Controller (Dongle)";
            default: return "Sony Controller";
        }
    }
    return "Unknown Controller";
}

std::string DualSenseWidget::GetHIDTypeString(int hid_type) const {
    switch (hid_type) {
        case 0: return "Auto (All Supported)";
        case 1: return "DualSense Regular Only";
        case 2: return "DualSense Edge Only";
        case 3: return "DualShock 4 Only";
        case 4: return "All Sony Controllers";
        default: return "Unknown";
    }
}

bool DualSenseWidget::IsDeviceTypeEnabled(USHORT product_id) const {
    int hid_type = g_shared_state->selected_hid_type.load();
    return display_commander::dualsense::g_dualsense_hid_wrapper->IsDeviceTypeEnabled(0x054c, product_id, hid_type);
}

void DualSenseWidget::LoadSettings() {
    // Load enable detection setting
    bool enable_detection;
    if (reshade::get_config_value(nullptr, "DisplayCommander.DualSenseWidget", "EnableDetection", enable_detection)) {
        g_shared_state->enable_dualsense_detection.store(enable_detection);
    }

    // Load show device IDs setting
    bool show_device_ids;
    if (reshade::get_config_value(nullptr, "DisplayCommander.DualSenseWidget", "ShowDeviceIds", show_device_ids)) {
        g_shared_state->show_device_ids.store(show_device_ids);
    }

    // Load show connection type setting
    bool show_connection_type;
    if (reshade::get_config_value(nullptr, "DisplayCommander.DualSenseWidget", "ShowConnectionType", show_connection_type)) {
        g_shared_state->show_connection_type.store(show_connection_type);
    }

    // Load show battery info setting
    bool show_battery_info;
    if (reshade::get_config_value(nullptr, "DisplayCommander.DualSenseWidget", "ShowBatteryInfo", show_battery_info)) {
        g_shared_state->show_battery_info.store(show_battery_info);
    }

    // Load show advanced features setting
    bool show_advanced_features;
    if (reshade::get_config_value(nullptr, "DisplayCommander.DualSenseWidget", "ShowAdvancedFeatures", show_advanced_features)) {
        g_shared_state->show_advanced_features.store(show_advanced_features);
    }

    // Load HID type filter setting
    int hid_type;
    if (reshade::get_config_value(nullptr, "DisplayCommander.DualSenseWidget", "HIDTypeFilter", hid_type)) {
        g_shared_state->selected_hid_type.store(hid_type);
    }
}

void DualSenseWidget::SaveSettings() {
    // Save enable detection setting
    reshade::set_config_value(nullptr, "DisplayCommander.DualSenseWidget", "EnableDetection", g_shared_state->enable_dualsense_detection.load());

    // Save show device IDs setting
    reshade::set_config_value(nullptr, "DisplayCommander.DualSenseWidget", "ShowDeviceIds", g_shared_state->show_device_ids.load());

    // Save show connection type setting
    reshade::set_config_value(nullptr, "DisplayCommander.DualSenseWidget", "ShowConnectionType", g_shared_state->show_connection_type.load());

    // Save show battery info setting
    reshade::set_config_value(nullptr, "DisplayCommander.DualSenseWidget", "ShowBatteryInfo", g_shared_state->show_battery_info.load());

    // Save show advanced features setting
    reshade::set_config_value(nullptr, "DisplayCommander.DualSenseWidget", "ShowAdvancedFeatures", g_shared_state->show_advanced_features.load());

    // Save HID type filter setting
    reshade::set_config_value(nullptr, "DisplayCommander.DualSenseWidget", "HIDTypeFilter", g_shared_state->selected_hid_type.load());
}


void DualSenseWidget::UpdateDeviceStates() {
    // Update device states using HID wrapper
    display_commander::dualsense::UpdateDualSenseDeviceStates();
}


std::shared_ptr<DualSenseSharedState> DualSenseWidget::GetSharedState() {
    return g_shared_state;
}

// Global functions for integration
void InitializeDualSenseWidget() {
    if (!g_dualsense_widget) {
        g_dualsense_widget = std::make_unique<DualSenseWidget>();
        g_dualsense_widget->Initialize();
    }
}

void CleanupDualSenseWidget() {
    if (g_dualsense_widget) {
        g_dualsense_widget->Cleanup();
        g_dualsense_widget.reset();
    }
}

void DrawDualSenseWidget() {
    if (g_dualsense_widget) {
        g_dualsense_widget->OnDraw();
    }
}

void DualSenseWidget::DrawInputReport(const DualSenseDeviceInfo& device) {
    if (!ImGui::CollapsingHeader("Input Report Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    // Debug offset control
    ImGui::Text("Debug Offset:");
    int offset = g_shared_state->debug_offset.load();
    if (ImGui::SliderInt("##debug_offset", &offset, 0, 20)) {
        g_shared_state->debug_offset.store(offset);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        g_shared_state->debug_offset.store(0);
    }
    ImGui::Text("Using offset: %d (accessing inputReport[2 + %d])", offset, offset);

    // Get the current input report from the device
    if (device.hid_device && device.hid_device->input_report.size() > 0) {
        const auto& inputReport = device.hid_device->input_report;
        int reportSize = static_cast<int>(inputReport.size());

        ImGui::Text("Report Size: %d bytes", reportSize);
        ImGui::Text("Connection: %s", device.connection_type.c_str());

        // Show raw bytes in a table format
        if (ImGui::BeginTable("InputReport", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Offset");
            ImGui::TableSetupColumn("Byte");
            ImGui::TableSetupColumn("Hex");
            ImGui::TableSetupColumn("Binary");
            ImGui::TableSetupColumn("Description");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Notes");
            ImGui::TableSetupColumn("Status");
            ImGui::TableHeadersRow();

            for (int i = 0; i < reportSize; i++) {
                ImGui::TableNextRow();

                // Offset
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%02d", i);

                // Byte value
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", inputReport[i]);

                // Hex value
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("0x%02X", inputReport[i]);

                // Binary value
                ImGui::TableSetColumnIndex(3);
                std::string binary = "";
                for (int bit = 7; bit >= 0; bit--) {
                    binary += (inputReport[i] & (1 << bit)) ? "1" : "0";
                    if (bit == 4) binary += " "; // Add space for readability
                }
                ImGui::Text("%s", binary.c_str());

                // Description based on position
                ImGui::TableSetColumnIndex(4);
                std::string description = GetByteDescription(i, device.connection_type);
                ImGui::Text("%s", description.c_str());

                // Value interpretation
                ImGui::TableSetColumnIndex(5);
                std::string value = GetByteValue(inputReport, i, device.connection_type);
                ImGui::Text("%s", value.c_str());

                // Notes
                ImGui::TableSetColumnIndex(6);
                std::string notes = GetByteNotes(i, device.connection_type);
                ImGui::Text("%s", notes.c_str());

                // Status (highlight offset bytes)
                ImGui::TableSetColumnIndex(7);
                if (i == 2 + offset) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "OFFSET");
                } else if (i >= 2 && i < 2 + offset) {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "SKIP");
                } else {
                    ImGui::Text("");
                }
            }
            ImGui::EndTable();
        }

        // Show specific data at offset
        if (2 + offset < reportSize) {
            ImGui::Spacing();
            ImGui::Text("Data at offset %d (inputReport[2 + %d]):", offset, offset);
            int dataOffset = 2 + offset;
            ImGui::Text("Byte: %d (0x%02X)", inputReport[dataOffset], inputReport[dataOffset]);

            // Show as 16-bit value if we have enough bytes
            if (dataOffset + 1 < reportSize) {
                SHORT value16 = static_cast<SHORT>((inputReport[dataOffset + 1] << 8) | inputReport[dataOffset]);
                ImGui::Text("16-bit: %d (0x%04X)", value16, value16);
            }
        }

    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No input report data available");
    }
}

std::string DualSenseWidget::GetByteDescription(int offset, const std::string& connectionType) const {
    if (connectionType == "Bluetooth") {
        // Bluetooth report format
        switch (offset) {
            case 0: return "Report ID";
            case 1: return "Buttons 1";
            case 2: return "Buttons 2";
            case 3: return "D-Pad";
            case 4: return "Left Stick X (low)";
            case 5: return "Left Stick X (high)";
            case 6: return "Left Stick Y (low)";
            case 7: return "Left Stick Y (high)";
            case 8: return "Right Stick X (low)";
            case 9: return "Right Stick X (high)";
            case 10: return "Right Stick Y (low)";
            case 11: return "Right Stick Y (high)";
            case 12: return "Left Trigger (low)";
            case 13: return "Left Trigger (high)";
            case 14: return "Right Trigger (low)";
            case 15: return "Right Trigger (high)";
            case 16: return "Counter";
            case 17: return "Battery";
            case 18: return "Touchpad 1";
            case 19: return "Touchpad 2";
            case 20: return "Touchpad 3";
            case 21: return "Touchpad 4";
            case 22: return "Touchpad 5";
            case 23: return "Touchpad 6";
            case 24: return "Touchpad 7";
            case 25: return "Touchpad 8";
            case 26: return "Touchpad 9";
            case 27: return "Touchpad 10";
            case 28: return "Touchpad 11";
            case 29: return "Touchpad 12";
            case 30: return "Touchpad 13";
            case 31: return "Touchpad 14";
            case 32: return "Touchpad 15";
            case 33: return "Touchpad 16";
            case 34: return "Touchpad 17";
            case 35: return "Touchpad 18";
            case 36: return "Touchpad 19";
            case 37: return "Touchpad 20";
            case 38: return "Touchpad 21";
            case 39: return "Touchpad 22";
            case 40: return "Touchpad 23";
            case 41: return "Touchpad 24";
            case 42: return "Touchpad 25";
            case 43: return "Touchpad 26";
            case 44: return "Touchpad 27";
            case 45: return "Touchpad 28";
            case 46: return "Touchpad 29";
            case 47: return "Touchpad 30";
            case 48: return "Touchpad 31";
            case 49: return "Touchpad 32";
            case 50: return "Touchpad 33";
            case 51: return "Touchpad 34";
            case 52: return "Touchpad 35";
            case 53: return "Touchpad 36";
            case 54: return "Touchpad 37";
            case 55: return "Touchpad 38";
            case 56: return "Touchpad 39";
            case 57: return "Touchpad 40";
            case 58: return "Touchpad 41";
            case 59: return "Touchpad 42";
            case 60: return "Touchpad 43";
            case 61: return "Touchpad 44";
            case 62: return "Touchpad 45";
            case 63: return "Touchpad 46";
            case 64: return "Touchpad 47";
            case 65: return "Touchpad 48";
            case 66: return "Touchpad 49";
            case 67: return "Touchpad 50";
            case 68: return "Touchpad 51";
            case 69: return "Touchpad 52";
            case 70: return "Touchpad 53";
            case 71: return "Touchpad 54";
            case 72: return "Touchpad 55";
            case 73: return "Touchpad 56";
            case 74: return "Touchpad 57";
            case 75: return "Touchpad 58";
            case 76: return "Touchpad 59";
            case 77: return "Touchpad 60";
            default: return "Unknown";
        }
    } else {
        // USB report format
        switch (offset) {
            case 0: return "Report ID";
            case 1: return "Buttons 1";
            case 2: return "Buttons 2";
            case 3: return "D-Pad";
            case 4: return "Left Stick X (low)";
            case 5: return "Left Stick X (high)";
            case 6: return "Left Stick Y (low)";
            case 7: return "Left Stick Y (high)";
            case 8: return "Right Stick X (low)";
            case 9: return "Right Stick X (high)";
            case 10: return "Right Stick Y (low)";
            case 11: return "Right Stick Y (high)";
            case 12: return "Left Trigger (low)";
            case 13: return "Left Trigger (high)";
            case 14: return "Right Trigger (low)";
            case 15: return "Right Trigger (high)";
            case 16: return "Counter";
            case 17: return "Battery";
            case 18: return "Touchpad 1";
            case 19: return "Touchpad 2";
            case 20: return "Touchpad 3";
            case 21: return "Touchpad 4";
            case 22: return "Touchpad 5";
            case 23: return "Touchpad 6";
            case 24: return "Touchpad 7";
            case 25: return "Touchpad 8";
            case 26: return "Touchpad 9";
            case 27: return "Touchpad 10";
            case 28: return "Touchpad 11";
            case 29: return "Touchpad 12";
            case 30: return "Touchpad 13";
            case 31: return "Touchpad 14";
            case 32: return "Touchpad 15";
            case 33: return "Touchpad 16";
            case 34: return "Touchpad 17";
            case 35: return "Touchpad 18";
            case 36: return "Touchpad 19";
            case 37: return "Touchpad 20";
            case 38: return "Touchpad 21";
            case 39: return "Touchpad 22";
            case 40: return "Touchpad 23";
            case 41: return "Touchpad 24";
            case 42: return "Touchpad 25";
            case 43: return "Touchpad 26";
            case 44: return "Touchpad 27";
            case 45: return "Touchpad 28";
            case 46: return "Touchpad 29";
            case 47: return "Touchpad 30";
            case 48: return "Touchpad 31";
            case 49: return "Touchpad 32";
            case 50: return "Touchpad 33";
            case 51: return "Touchpad 34";
            case 52: return "Touchpad 35";
            case 53: return "Touchpad 36";
            case 54: return "Touchpad 37";
            case 55: return "Touchpad 38";
            case 56: return "Touchpad 39";
            case 57: return "Touchpad 40";
            case 58: return "Touchpad 41";
            case 59: return "Touchpad 42";
            case 60: return "Touchpad 43";
            case 61: return "Touchpad 44";
            case 62: return "Touchpad 45";
            case 63: return "Touchpad 46";
            default: return "Unknown";
        }
    }
}

std::string DualSenseWidget::GetByteValue(const std::vector<BYTE>& inputReport, int offset, const std::string& connectionType) const {
    if (offset >= inputReport.size()) return "N/A";

    BYTE value = inputReport[offset];

    // Special handling for certain bytes
    if (offset == 0) {
        return std::to_string(value) + " (0x" + std::to_string(value) + ")";
    } else if (offset >= 4 && offset <= 11) {
        // Stick values - show as 16-bit
        if (offset % 2 == 0 && offset + 1 < inputReport.size()) {
            SHORT stickValue = static_cast<SHORT>((inputReport[offset + 1] << 8) | inputReport[offset]);
            return std::to_string(stickValue);
        }
    } else if (offset >= 12 && offset <= 15) {
        // Trigger values - show as 16-bit
        if (offset % 2 == 0 && offset + 1 < inputReport.size()) {
            USHORT triggerValue = static_cast<USHORT>((inputReport[offset + 1] << 8) | inputReport[offset]);
            return std::to_string(triggerValue);
        }
    } else if (offset == 17) {
        // Battery level
        return std::to_string(value) + "%";
    }

    return std::to_string(value);
}

std::string DualSenseWidget::GetByteNotes(int offset, const std::string& connectionType) const {
    if (offset == 0) {
        return connectionType == "Bluetooth" ? "Should be 0x31" : "Should be 0x01";
    } else if (offset == 1) {
        return "Square, Cross, Circle, Triangle, L1, R1, L2, R2";
    } else if (offset == 2) {
        return "Share, Options, L3, R3, PS, Touchpad";
    } else if (offset == 3) {
        return "D-Pad direction";
    } else if (offset >= 4 && offset <= 11) {
        return "Stick data (16-bit signed)";
    } else if (offset >= 12 && offset <= 15) {
        return "Trigger data (16-bit unsigned)";
    } else if (offset == 16) {
        return "Packet counter";
    } else if (offset == 17) {
        return "Battery level (0-100)";
    } else if (offset >= 18) {
        return "Touchpad data";
    }

    return "";
}

void DualSenseWidget::DrawRawButtonStates(const DualSenseDeviceInfo& device) {
    if (!ImGui::CollapsingHeader("Raw Buttons (with Debug Offset)", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (!device.hid_device || device.hid_device->input_report.size() == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No input report data available");
        return;
    }

    const auto& inputReport = device.hid_device->input_report;
    int offset = g_shared_state->debug_offset.load();

    // Parse buttons with debug offset
    WORD buttons = 0;

    // Buttons 1 (with offset)
    if (1 + offset < inputReport.size()) {
        BYTE button1 = inputReport[1 + offset];
        if (button1 & 0x10) buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;  // L1
        if (button1 & 0x20) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER; // R1
        if (button1 & 0x01) buttons |= XINPUT_GAMEPAD_X;              // Square -> X
        if (button1 & 0x02) buttons |= XINPUT_GAMEPAD_A;              // Cross -> A
        if (button1 & 0x04) buttons |= XINPUT_GAMEPAD_B;              // Circle -> B
        if (button1 & 0x08) buttons |= XINPUT_GAMEPAD_Y;              // Triangle -> Y
    }

    // Buttons 2 (with offset)
    if (2 + offset < inputReport.size()) {
        BYTE button2 = inputReport[2 + offset];
        if (button2 & 0x01) buttons |= XINPUT_GAMEPAD_BACK;           // Share
        if (button2 & 0x02) buttons |= XINPUT_GAMEPAD_START;          // Options
        if (button2 & 0x04) buttons |= XINPUT_GAMEPAD_LEFT_THUMB;     // L3
        if (button2 & 0x08) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;    // R3
        if (button2 & 0x10) buttons |= 0x0400;                        // PS (custom constant)
    }

    // D-Pad (with offset)
    if (3 + offset < inputReport.size()) {
        BYTE dpad = inputReport[3 + offset] & 0x0F;
        buttons |= dpad; // D-Pad is already in the correct format
    }

    // Display buttons
    const struct {
        WORD mask;
        const char* name;
    } button_list[] = {
        {XINPUT_GAMEPAD_A, "A"},
        {XINPUT_GAMEPAD_B, "B"},
        {XINPUT_GAMEPAD_X, "X"},
        {XINPUT_GAMEPAD_Y, "Y"},
        {XINPUT_GAMEPAD_LEFT_SHOULDER, "L1"},
        {XINPUT_GAMEPAD_RIGHT_SHOULDER, "R1"},
        {XINPUT_GAMEPAD_BACK, "Share"},
        {XINPUT_GAMEPAD_START, "Options"},
        {XINPUT_GAMEPAD_LEFT_THUMB, "L3"},
        {XINPUT_GAMEPAD_RIGHT_THUMB, "R3"},
        {0x0400, "PS"},
    };

    ImGui::Text("Using offset: %d", offset);
    ImGui::Columns(3, "RawButtonColumns", false);
    for (const auto& button : button_list) {
        bool pressed = IsButtonPressed(buttons, button.mask);
        ImGui::TextColored(pressed ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                          "%s: %s", button.name, pressed ? "PRESSED" : "Released");
        ImGui::NextColumn();
    }
    ImGui::Columns(1);

    // D-Pad
    ImGui::Text("D-Pad:");
    const char* dpad_directions[] = {"None", "Up", "Up-Right", "Right", "Down-Right", "Down", "Down-Left", "Left", "Up-Left"};
    ImGui::Text("Direction: %s", dpad_directions[buttons & 0x0F]);
}

void DualSenseWidget::DrawRawStickStates(const DualSenseDeviceInfo& device) {
    if (!ImGui::CollapsingHeader("Raw Analog Sticks (with Debug Offset)", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (!device.hid_device || device.hid_device->input_report.size() == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No input report data available");
        return;
    }

    const auto& inputReport = device.hid_device->input_report;
    int offset = g_shared_state->debug_offset.load();

    ImGui::Text("Using offset: %d", offset);

    // Left stick
    ImGui::Text("Left Stick:");
    SHORT leftX = 0, leftY = 0;
    if (4 + offset + 1 < inputReport.size()) {
        leftX = static_cast<SHORT>((inputReport[5 + offset] << 8) | inputReport[4 + offset]);
        leftY = static_cast<SHORT>((inputReport[7 + offset] << 8) | inputReport[6 + offset]);
    }

    float lx = ShortToFloat(leftX);
    float ly = ShortToFloat(leftY);
    ImGui::Text("X: %.3f (Raw: %d)", lx, leftX);
    ImGui::Text("Y: %.3f (Raw: %d)", ly, leftY);

    // Visual representation for left stick
    ImGui::Text("Position:");
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_size = ImVec2(100, 100);

    // Draw circle
    ImVec2 center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
    draw_list->AddCircle(center, canvas_size.x * 0.4f, ImColor(100, 100, 100, 255), 32, 2.0f);

    // Draw crosshairs
    draw_list->AddLine(ImVec2(canvas_pos.x, center.y), ImVec2(canvas_pos.x + canvas_size.x, center.y), ImColor(100, 100, 100, 255), 1.0f);
    draw_list->AddLine(ImVec2(center.x, canvas_pos.y), ImVec2(center.x, canvas_pos.y + canvas_size.y), ImColor(100, 100, 100, 255), 1.0f);

    // Draw stick position
    ImVec2 stick_pos = ImVec2(center.x + lx * canvas_size.x * 0.4f, center.y - ly * canvas_size.y * 0.4f);
    draw_list->AddCircleFilled(stick_pos, 5.0f, ImColor(0, 255, 0, 255));

    ImGui::Dummy(canvas_size);

    // Right stick
    ImGui::Text("Right Stick:");
    SHORT rightX = 0, rightY = 0;
    if (8 + offset + 1 < inputReport.size()) {
        rightX = static_cast<SHORT>((inputReport[9 + offset] << 8) | inputReport[8 + offset]);
        rightY = static_cast<SHORT>((inputReport[11 + offset] << 8) | inputReport[10 + offset]);
    }

    float rx = ShortToFloat(rightX);
    float ry = ShortToFloat(rightY);
    ImGui::Text("X: %.3f (Raw: %d)", rx, rightX);
    ImGui::Text("Y: %.3f (Raw: %d)", ry, rightY);

    // Visual representation for right stick
    ImGui::Text("Position:");
    canvas_pos = ImGui::GetCursorScreenPos();
    draw_list = ImGui::GetWindowDrawList();

    // Draw circle
    center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
    draw_list->AddCircle(center, canvas_size.x * 0.4f, ImColor(100, 100, 100, 255), 32, 2.0f);

    // Draw crosshairs
    draw_list->AddLine(ImVec2(canvas_pos.x, center.y), ImVec2(canvas_pos.x + canvas_size.x, center.y), ImColor(100, 100, 100, 255), 1.0f);
    draw_list->AddLine(ImVec2(center.x, canvas_pos.y), ImVec2(center.x, canvas_pos.y + canvas_size.y), ImColor(100, 100, 100, 255), 1.0f);

    // Draw stick position
    stick_pos = ImVec2(center.x + rx * canvas_size.x * 0.4f, center.y - ry * canvas_size.y * 0.4f);
    draw_list->AddCircleFilled(stick_pos, 5.0f, ImColor(0, 255, 0, 255));

    ImGui::Dummy(canvas_size);
}

void DualSenseWidget::DrawRawTriggerStates(const DualSenseDeviceInfo& device) {
    if (!ImGui::CollapsingHeader("Raw Triggers (with Debug Offset)", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (!device.hid_device || device.hid_device->input_report.size() == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No input report data available");
        return;
    }

    const auto& inputReport = device.hid_device->input_report;
    int offset = g_shared_state->debug_offset.load();

    ImGui::Text("Using offset: %d", offset);

    // Left trigger
    USHORT leftTrigger = 0;
    if (12 + offset + 1 < inputReport.size()) {
        leftTrigger = static_cast<USHORT>((inputReport[13 + offset] << 8) | inputReport[12 + offset]);
    }

    ImGui::Text("Left Trigger: %u/65535 (%.1f%%)",
               leftTrigger,
               (static_cast<float>(leftTrigger) / 65535.0f) * 100.0f);

    // Right trigger
    USHORT rightTrigger = 0;
    if (14 + offset + 1 < inputReport.size()) {
        rightTrigger = static_cast<USHORT>((inputReport[15 + offset] << 8) | inputReport[14 + offset]);
    }

    ImGui::Text("Right Trigger: %u/65535 (%.1f%%)",
               rightTrigger,
               (static_cast<float>(rightTrigger) / 65535.0f) * 100.0f);
}

// Global functions for device enumeration
void EnumerateDualSenseDevices() {
    display_commander::dualsense::EnumerateDualSenseDevices();
}

void UpdateDualSenseDeviceStates() {
    if (g_dualsense_widget) {
        g_dualsense_widget->UpdateDeviceStates();
    }
}

} // namespace display_commander::widgets::dualsense_widget
