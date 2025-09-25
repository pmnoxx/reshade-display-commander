#include "remapping_widget.hpp"
#include "../../utils.hpp"
#include <imgui.h>
#include <reshade.hpp>

namespace display_commander::widgets::remapping_widget {
// Global widget instance
std::unique_ptr<RemappingWidget> RemappingWidget::g_remapping_widget = nullptr;

RemappingWidget::RemappingWidget() {
    // Initialize dialog state
    ResetDialogState();
}

void RemappingWidget::Initialize() {
    if (is_initialized_)
        return;

    LogInfo("RemappingWidget::Initialize() - Starting remapping widget initialization");

    // Load settings
    LoadSettings();

    is_initialized_ = true;
    LogInfo("RemappingWidget::Initialize() - Remapping widget initialization complete");
}

void RemappingWidget::Cleanup() {
    if (!is_initialized_)
        return;

    // Save settings
    SaveSettings();

    is_initialized_ = false;
}

void RemappingWidget::OnDraw() {
    if (!is_initialized_) {
        Initialize();
    }

    // Draw the remapping widget UI
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "=== Gamepad Remapping ===");
    ImGui::Spacing();

    // Draw controller selector
    DrawControllerSelector();
    ImGui::Spacing();

    // Draw remapping settings
    DrawRemappingSettings();
    ImGui::Spacing();

    // Draw input method slider
    DrawInputMethodSlider();
    ImGui::Spacing();

    // Draw remapping list
    DrawRemappingList();
    ImGui::Spacing();

    // Draw dialogs
    if (show_add_remap_dialog_) {
        DrawAddRemapDialog();
    }
    if (show_edit_remap_dialog_) {
        DrawEditRemapDialog();
    }
}

void RemappingWidget::DrawControllerSelector() {
    if (ImGui::CollapsingHeader("Controller Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Select Controller:");
        ImGui::SameLine();

        const char *controllers[] = {"Controller 1", "Controller 2", "Controller 3", "Controller 4"};
        if (ImGui::Combo("##Controller", &selected_controller_, controllers, 4)) {
            LogInfo("RemappingWidget::DrawControllerSelector() - Selected controller %d", selected_controller_);
        }

        ImGui::Text("Note: Remappings apply to all controllers");
    }
}

void RemappingWidget::DrawRemappingSettings() {
    if (ImGui::CollapsingHeader("Remapping Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto &remapper = input_remapping::InputRemapper::get_instance();

        // Enable/disable remapping
        bool remapping_enabled = remapper.is_remapping_enabled();
        if (ImGui::Checkbox("Enable Gamepad Remapping", &remapping_enabled)) {
            remapper.set_remapping_enabled(remapping_enabled);
            LogInfo("RemappingWidget::DrawRemappingSettings() - Remapping %s",
                    remapping_enabled ? "enabled" : "disabled");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("When enabled, gamepad buttons will be mapped to keyboard inputs");
        }

        // Show statistics
        const auto &remappings = remapper.get_remappings();
        ImGui::Text("Active Remappings: %zu", remappings.size());

        // Quick actions
        ImGui::Spacing();
        if (ImGui::Button("Add New Remapping")) {
            ResetDialogState();
            show_add_remap_dialog_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All Remappings")) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
                remapper.clear_all_remaps();
                LogInfo("RemappingWidget::DrawRemappingSettings() - Cleared all remappings");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Hold Ctrl and click to confirm");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Counters")) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
                ResetTriggerCounters();
                LogInfo("RemappingWidget::DrawRemappingSettings() - Reset all trigger counters");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Hold Ctrl and click to confirm");
            }
        }
    }
}

void RemappingWidget::DrawInputMethodSlider() {
    if (ImGui::CollapsingHeader("Input Method Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto &remapper = input_remapping::InputRemapper::get_instance();

        // Get available input methods
        auto methods = input_remapping::get_available_keyboard_input_methods();
        int current_method = static_cast<int>(remapper.get_default_input_method());

        ImGui::Text("Default Input Method:");
        if (ImGui::SliderInt("##InputMethod", &current_method, 0, methods.size() - 1,
                             methods[current_method].c_str())) {
            remapper.set_default_input_method(static_cast<input_remapping::KeyboardInputMethod>(current_method));
            LogInfo("RemappingWidget::DrawInputMethodSlider() - Set input method to %s",
                    methods[current_method].c_str());
        }

        ImGui::TextDisabled("SendInput: Most reliable, works with most applications");
        ImGui::TextDisabled("keybd_event: Legacy method, may not work with some games");
        ImGui::TextDisabled("SendMessage: Sends to active window, may not work with fullscreen games");
        ImGui::TextDisabled("PostMessage: Asynchronous, may not work with some applications");
    }
}

void RemappingWidget::DrawRemappingList() {
    if (ImGui::CollapsingHeader("Current Remappings", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto &remapper = input_remapping::InputRemapper::get_instance();
        const auto &remappings = remapper.get_remappings();

        if (remappings.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No remappings configured");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Click 'Add New Remapping' to get started");
        } else {
            // Create a table for remappings
            if (ImGui::BeginTable("RemappingsTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Gamepad Button");
                ImGui::TableSetupColumn("Keyboard Key");
                ImGui::TableSetupColumn("Input Method");
                ImGui::TableSetupColumn("Hold Mode");
                ImGui::TableSetupColumn("Trigger Count");
                ImGui::TableSetupColumn("Enabled");
                ImGui::TableSetupColumn("Actions");
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < remappings.size(); ++i) {
                    DrawRemapEntry(remappings[i], static_cast<int>(i));
                }

                ImGui::EndTable();
            }
        }
    }
}

void RemappingWidget::DrawRemapEntry(const input_remapping::ButtonRemap &remap, int index) {
    ImGui::TableNextRow();

    // Gamepad Button
    ImGui::TableNextColumn();
    ImGui::Text("%s", GetGamepadButtonNameFromCode(remap.gamepad_button).c_str());

    // Keyboard Key
    ImGui::TableNextColumn();
    ImGui::Text("%s", remap.keyboard_name.c_str());

    // Input Method
    ImGui::TableNextColumn();
    ImGui::Text("%s", input_remapping::get_keyboard_input_method_name(remap.input_method).c_str());

    // Hold Mode
    ImGui::TableNextColumn();
    ImGui::Text("%s", remap.hold_mode ? "Yes" : "No");

    // Trigger Count
    ImGui::TableNextColumn();
    ImGui::Text("%llu", remap.trigger_count.load());

    // Enabled
    ImGui::TableNextColumn();
    bool enabled = remap.enabled;
    if (ImGui::Checkbox(("##Enabled" + std::to_string(index)).c_str(), &enabled)) {
        // Update enabled state
        auto &remapper = input_remapping::InputRemapper::get_instance();
        remapper.update_remap(remap.gamepad_button, remap.keyboard_vk, remap.keyboard_name, remap.input_method,
                              remap.hold_mode);
        LogInfo("RemappingWidget::DrawRemapEntry() - Toggled remap %d enabled state", index);
    }

    // Actions
    ImGui::TableNextColumn();
    if (ImGui::Button(("Edit##" + std::to_string(index)).c_str())) {
        editing_remap_index_ = index;
        LoadRemapToDialog(remap);
        show_edit_remap_dialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(("Delete##" + std::to_string(index)).c_str())) {
        auto &remapper = input_remapping::InputRemapper::get_instance();
        remapper.remove_button_remap(remap.gamepad_button);
        LogInfo("RemappingWidget::DrawRemapEntry() - Deleted remap %d", index);
    }
}

void RemappingWidget::DrawAddRemapDialog() {
    ImGui::OpenPopup("Add Remapping");
    if (ImGui::BeginPopupModal("Add Remapping", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configure new gamepad to keyboard mapping");
        ImGui::Separator();

        // Gamepad Button Selection
        ImGui::Text("Gamepad Button:");
        const char *gamepad_buttons[] = {"A",          "B",          "X",           "Y",           "D-Pad Up",
                                         "D-Pad Down", "D-Pad Left", "D-Pad Right", "Start",       "Back",
                                         "Guide",      "Left Stick", "Right Stick", "Left Bumper", "Right Bumper"};
        if (ImGui::Combo("##GamepadButton", &dialog_state_.selected_gamepad_button, gamepad_buttons, 15)) {
            // Update selection
        }

        // Keyboard Key Selection
        ImGui::Text("Keyboard Key:");
        const char *keyboard_keys[] = {"Space", "Enter", "Escape", "Tab", "Shift", "Ctrl", "Alt", "F1",  "F2", "F3",
                                       "F4",    "F5",    "F6",     "F7",  "F8",    "F9",   "F10", "F11", "F12"};
        if (ImGui::Combo("##KeyboardKey", &dialog_state_.selected_keyboard_key, keyboard_keys, 19)) {
            // Update selection
        }

        // Input Method Selection
        ImGui::Text("Input Method:");
        const char *input_methods[] = {"SendInput", "keybd_event", "SendMessage", "PostMessage"};
        if (ImGui::Combo("##InputMethod", &dialog_state_.selected_input_method, input_methods, 4)) {
            // Update selection
        }

        // Hold Mode
        ImGui::Checkbox("Hold Mode", &dialog_state_.hold_mode);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If enabled, the keyboard key will be held down while the gamepad button is pressed");
        }

        // Enabled
        ImGui::Checkbox("Enabled", &dialog_state_.enabled);

        ImGui::Separator();

        // Buttons
        if (ImGui::Button("Add")) {
            // Add the remapping
            WORD gamepad_button = GetGamepadButtonFromIndex(dialog_state_.selected_gamepad_button);
            int keyboard_vk = GetKeyboardVkFromIndex(dialog_state_.selected_keyboard_key);
            std::string keyboard_name = GetKeyboardKeyName(dialog_state_.selected_keyboard_key);
            auto input_method = static_cast<input_remapping::KeyboardInputMethod>(dialog_state_.selected_input_method);

            input_remapping::ButtonRemap remap(gamepad_button, keyboard_vk, keyboard_name, dialog_state_.enabled,
                                               input_method, dialog_state_.hold_mode);

            auto &remapper = input_remapping::InputRemapper::get_instance();
            remapper.add_button_remap(remap);

            show_add_remap_dialog_ = false;
            ResetDialogState();
            LogInfo("RemappingWidget::DrawAddRemapDialog() - Added new remapping");
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            show_add_remap_dialog_ = false;
            ResetDialogState();
        }

        ImGui::EndPopup();
    }
}

void RemappingWidget::DrawEditRemapDialog() {
    ImGui::OpenPopup("Edit Remapping");
    if (ImGui::BeginPopupModal("Edit Remapping", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Edit gamepad to keyboard mapping");
        ImGui::Separator();

        // Same UI as add dialog
        ImGui::Text("Gamepad Button:");
        const char *gamepad_buttons[] = {"A",          "B",          "X",           "Y",           "D-Pad Up",
                                         "D-Pad Down", "D-Pad Left", "D-Pad Right", "Start",       "Back",
                                         "Guide",      "Left Stick", "Right Stick", "Left Bumper", "Right Bumper"};
        ImGui::Combo("##GamepadButton", &dialog_state_.selected_gamepad_button, gamepad_buttons, 15);

        ImGui::Text("Keyboard Key:");
        const char *keyboard_keys[] = {"Space", "Enter", "Escape", "Tab", "Shift", "Ctrl", "Alt", "F1",  "F2", "F3",
                                       "F4",    "F5",    "F6",     "F7",  "F8",    "F9",   "F10", "F11", "F12"};
        ImGui::Combo("##KeyboardKey", &dialog_state_.selected_keyboard_key, keyboard_keys, 19);

        ImGui::Text("Input Method:");
        const char *input_methods[] = {"SendInput", "keybd_event", "SendMessage", "PostMessage"};
        ImGui::Combo("##InputMethod", &dialog_state_.selected_input_method, input_methods, 4);

        ImGui::Checkbox("Hold Mode", &dialog_state_.hold_mode);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If enabled, the keyboard key will be held down while the gamepad button is pressed");
        }

        ImGui::Checkbox("Enabled", &dialog_state_.enabled);

        ImGui::Separator();

        if (ImGui::Button("Save")) {
            // Update the remapping
            WORD gamepad_button = GetGamepadButtonFromIndex(dialog_state_.selected_gamepad_button);
            int keyboard_vk = GetKeyboardVkFromIndex(dialog_state_.selected_keyboard_key);
            std::string keyboard_name = GetKeyboardKeyName(dialog_state_.selected_keyboard_key);
            auto input_method = static_cast<input_remapping::KeyboardInputMethod>(dialog_state_.selected_input_method);

            auto &remapper = input_remapping::InputRemapper::get_instance();
            remapper.update_remap(gamepad_button, keyboard_vk, keyboard_name, input_method, dialog_state_.hold_mode);

            show_edit_remap_dialog_ = false;
            editing_remap_index_ = -1;
            ResetDialogState();
            LogInfo("RemappingWidget::DrawEditRemapDialog() - Updated remapping");
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            show_edit_remap_dialog_ = false;
            editing_remap_index_ = -1;
            ResetDialogState();
        }

        ImGui::EndPopup();
    }
}

std::string RemappingWidget::GetGamepadButtonName(int button_index) const {
    auto buttons = input_remapping::get_available_gamepad_buttons();
    if (button_index >= 0 && button_index < static_cast<int>(buttons.size())) {
        return buttons[button_index];
    }
    return "Unknown";
}

std::string RemappingWidget::GetKeyboardKeyName(int key_index) const {
    auto keys = input_remapping::get_available_keyboard_keys();
    if (key_index >= 0 && key_index < static_cast<int>(keys.size())) {
        return keys[key_index];
    }
    return "Unknown";
}

std::string RemappingWidget::GetGamepadButtonNameFromCode(WORD button_code) const {
    switch (button_code) {
    case XINPUT_GAMEPAD_DPAD_UP:
        return "D-Pad Up";
    case XINPUT_GAMEPAD_DPAD_DOWN:
        return "D-Pad Down";
    case XINPUT_GAMEPAD_DPAD_LEFT:
        return "D-Pad Left";
    case XINPUT_GAMEPAD_DPAD_RIGHT:
        return "D-Pad Right";
    case XINPUT_GAMEPAD_START:
        return "Start";
    case XINPUT_GAMEPAD_BACK:
        return "Back";
    case XINPUT_GAMEPAD_LEFT_THUMB:
        return "Left Stick";
    case XINPUT_GAMEPAD_RIGHT_THUMB:
        return "Right Stick";
    case XINPUT_GAMEPAD_LEFT_SHOULDER:
        return "Left Bumper";
    case XINPUT_GAMEPAD_RIGHT_SHOULDER:
        return "Right Bumper";
    case XINPUT_GAMEPAD_A:
        return "A";
    case XINPUT_GAMEPAD_B:
        return "B";
    case XINPUT_GAMEPAD_X:
        return "X";
    case XINPUT_GAMEPAD_Y:
        return "Y";
    case XINPUT_GAMEPAD_GUIDE:
        return "Guide";
    default:
        return "Unknown";
    }
}

WORD RemappingWidget::GetGamepadButtonFromIndex(int index) const {
    // Map index to XInput button constants
    switch (index) {
    case 0:
        return XINPUT_GAMEPAD_A;
    case 1:
        return XINPUT_GAMEPAD_B;
    case 2:
        return XINPUT_GAMEPAD_X;
    case 3:
        return XINPUT_GAMEPAD_Y;
    case 4:
        return XINPUT_GAMEPAD_DPAD_UP;
    case 5:
        return XINPUT_GAMEPAD_DPAD_DOWN;
    case 6:
        return XINPUT_GAMEPAD_DPAD_LEFT;
    case 7:
        return XINPUT_GAMEPAD_DPAD_RIGHT;
    case 8:
        return XINPUT_GAMEPAD_START;
    case 9:
        return XINPUT_GAMEPAD_BACK;
    case 10:
        return XINPUT_GAMEPAD_GUIDE;
    case 11:
        return XINPUT_GAMEPAD_LEFT_THUMB;
    case 12:
        return XINPUT_GAMEPAD_RIGHT_THUMB;
    case 13:
        return XINPUT_GAMEPAD_LEFT_SHOULDER;
    case 14:
        return XINPUT_GAMEPAD_RIGHT_SHOULDER;
    default:
        return 0;
    }
}

int RemappingWidget::GetKeyboardVkFromIndex(int index) const {
    // Map index to virtual key codes
    switch (index) {
    case 0:
        return VK_SPACE;
    case 1:
        return VK_RETURN;
    case 2:
        return VK_ESCAPE;
    case 3:
        return VK_TAB;
    case 4:
        return VK_SHIFT;
    case 5:
        return VK_CONTROL;
    case 6:
        return VK_MENU;
    case 7:
        return VK_F1;
    case 8:
        return VK_F2;
    case 9:
        return VK_F3;
    case 10:
        return VK_F4;
    case 11:
        return VK_F5;
    case 12:
        return VK_F6;
    case 13:
        return VK_F7;
    case 14:
        return VK_F8;
    case 15:
        return VK_F9;
    case 16:
        return VK_F10;
    case 17:
        return VK_F11;
    case 18:
        return VK_F12;
    default:
        return 0;
    }
}

void RemappingWidget::ResetDialogState() {
    dialog_state_.selected_gamepad_button = 0;
    dialog_state_.selected_keyboard_key = 0;
    dialog_state_.selected_input_method = 0;
    dialog_state_.hold_mode = true;
    dialog_state_.enabled = true;
}

void RemappingWidget::LoadRemapToDialog(const input_remapping::ButtonRemap &remap) {
    // Find the index for the gamepad button
    auto gamepad_buttons = input_remapping::get_available_gamepad_buttons();
    for (size_t i = 0; i < gamepad_buttons.size(); ++i) {
        if (GetGamepadButtonFromIndex(static_cast<int>(i)) == remap.gamepad_button) {
            dialog_state_.selected_gamepad_button = static_cast<int>(i);
            break;
        }
    }

    // Find the index for the keyboard key
    auto keyboard_keys = input_remapping::get_available_keyboard_keys();
    for (size_t i = 0; i < keyboard_keys.size(); ++i) {
        if (GetKeyboardVkFromIndex(static_cast<int>(i)) == remap.keyboard_vk) {
            dialog_state_.selected_keyboard_key = static_cast<int>(i);
            break;
        }
    }

    dialog_state_.selected_input_method = static_cast<int>(remap.input_method);
    dialog_state_.hold_mode = remap.hold_mode;
    dialog_state_.enabled = remap.enabled;
}

void RemappingWidget::LoadSettings() {
    // Load selected controller
    int selected_controller;
    if (reshade::get_config_value(nullptr, "DisplayCommander.RemappingWidget", "SelectedController",
                                  selected_controller)) {
        selected_controller_ = selected_controller;
    }

    LogInfo("RemappingWidget::LoadSettings() - Settings loaded");
}

void RemappingWidget::SaveSettings() {
    // Save selected controller
    reshade::set_config_value(nullptr, "DisplayCommander.RemappingWidget", "SelectedController", selected_controller_);

    LogInfo("RemappingWidget::SaveSettings() - Settings saved");
}

void RemappingWidget::ResetTriggerCounters() {
    auto &remapper = input_remapping::InputRemapper::get_instance();
    const auto &remappings = remapper.get_remappings();

    // Reset all trigger counts
    for (const auto &remap : remappings) {
        input_remapping::ButtonRemap *mutable_remap = const_cast<input_remapping::ButtonRemap *>(&remap);
        mutable_remap->trigger_count.store(0);
    }

    LogInfo("RemappingWidget::ResetTriggerCounters() - Reset %zu trigger counters", remappings.size());
}

// Global functions
void InitializeRemappingWidget() {
    if (!RemappingWidget::g_remapping_widget) {
        RemappingWidget::g_remapping_widget = std::make_unique<RemappingWidget>();
        RemappingWidget::g_remapping_widget->Initialize();
    }
}

void CleanupRemappingWidget() {
    if (RemappingWidget::g_remapping_widget) {
        RemappingWidget::g_remapping_widget->Cleanup();
        RemappingWidget::g_remapping_widget.reset();
    }
}

void DrawRemappingWidget() {
    if (RemappingWidget::g_remapping_widget) {
        RemappingWidget::g_remapping_widget->OnDraw();
    }
}
} // namespace display_commander::widgets::remapping_widget
