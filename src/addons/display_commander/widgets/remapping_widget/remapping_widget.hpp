/*
 * Copyright (C) 2024 Display Commander
 * Input remapping widget for configuring gamepad to keyboard mappings
 */

#pragma once

#include <imgui.h>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <windows.h>
#include <xinput.h>
#include "../../input_remapping/input_remapping.hpp"

namespace display_commander::widgets::remapping_widget
{
    // Remapping widget class
    class RemappingWidget
    {
    public:
        RemappingWidget();
        ~RemappingWidget() = default;

        // Main draw function - call this from the main tab
        void OnDraw();

        // Initialize the widget (call once at startup)
        void Initialize();

        // Cleanup the widget (call at shutdown)
        void Cleanup();

    private:
        // UI state
        bool is_initialized_ = false;
        int selected_controller_ = 0;
        bool show_add_remap_dialog_ = false;
        bool show_edit_remap_dialog_ = false;
        int editing_remap_index_ = -1;

        // Add/Edit dialog state
        struct RemapDialogState
        {
            int selected_gamepad_button = 0;
            int selected_keyboard_key = 0;
            int selected_input_method = 0;
            bool hold_mode = true;
            bool enabled = true;
        } dialog_state_;

        // UI helper functions
        void DrawRemappingSettings();
        void DrawRemappingList();
        void DrawAddRemapDialog();
        void DrawEditRemapDialog();
        void DrawInputMethodSlider();
        void DrawControllerSelector();
        void DrawRemapEntry(const input_remapping::ButtonRemap& remap, int index);

        // Helper functions
        std::string GetGamepadButtonName(int button_index) const;
        std::string GetKeyboardKeyName(int key_index) const;
        std::string GetGamepadButtonNameFromCode(WORD button_code) const;
        WORD GetGamepadButtonFromIndex(int index) const;
        int GetKeyboardVkFromIndex(int index) const;
        void ResetDialogState();
        void LoadRemapToDialog(const input_remapping::ButtonRemap& remap);

        // Settings management
        void LoadSettings();
        void SaveSettings();

        // Counter management
        void ResetTriggerCounters();

    public:
        // Global widget instance
        static std::unique_ptr<RemappingWidget> g_remapping_widget;
    };

    // Global functions for integration
    void InitializeRemappingWidget();
    void CleanupRemappingWidget();
    void DrawRemappingWidget();
}
