#pragma once

#include "../ui/new_ui/settings_wrapper.hpp"

#include <vector>

namespace settings {

// Bring setting types into scope
using ui::new_ui::BoolSetting;
using ui::new_ui::BoolSettingRef;
using ui::new_ui::ComboSetting;
using ui::new_ui::FixedIntArraySetting;
using ui::new_ui::FloatSetting;
using ui::new_ui::FloatSettingRef;
using ui::new_ui::IntSetting;
using ui::new_ui::SettingBase;

// Settings manager for the swapchain tab
class SwapchainTabSettings {
  public:
    SwapchainTabSettings();
    ~SwapchainTabSettings() = default;

    // Load all settings from ReShade config
    void LoadAll();

    // Get all settings for loading
    std::vector<SettingBase *> GetAllSettings();

    // DLSS preset override settings
    BoolSetting dlss_preset_override_enabled;
    ComboSetting dlss_sr_preset_override;
    ComboSetting dlss_rr_preset_override;

  private:
    std::vector<SettingBase *> all_settings_;
};

} // namespace settings
