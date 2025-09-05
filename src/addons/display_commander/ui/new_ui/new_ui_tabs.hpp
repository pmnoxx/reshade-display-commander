#pragma once

#include <deps/imgui/imgui.h>
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <memory>

namespace ui::new_ui {

// Tab structure for the new UI system
struct Tab {
    std::string name;
    std::string id;
    std::function<void()> on_draw;
    bool is_visible = true;
};

// Main tab manager class
// Thread-safe implementation using atomic shared_ptr for copy-on-write semantics
class TabManager {
public:
    TabManager();
    ~TabManager() = default;

    // Add a new tab (thread-safe)
    void AddTab(const std::string& name, const std::string& id, std::function<void()> on_draw);

    // Draw the tab bar and content (thread-safe)
    void Draw();

    // Get current active tab (thread-safe)
    int GetActiveTab() const { return active_tab_; }

    // Set active tab (thread-safe)
    void SetActiveTab(int tab) { active_tab_ = tab; }

private:
    std::atomic<std::shared_ptr<const std::vector<Tab>>> tabs_;
    int active_tab_ = 0;
};

// Global tab manager instance
extern TabManager g_tab_manager;

// Initialize the new UI system
void InitializeNewUI(int debug_mode);

// Draw the new UI
void DrawNewUI();

} // namespace ui::new_ui
