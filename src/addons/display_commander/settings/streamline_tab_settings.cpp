#include "streamline_tab_settings.hpp"
#include "../utils.hpp"

// Atomic variables for DLSS override settings
std::atomic<bool> s_dlss_override_enabled{false};
std::string s_dlss_override_folder;
std::atomic<bool> s_dlss_override_dlss{false};
std::atomic<bool> s_dlss_override_dlss_fg{false};
std::atomic<bool> s_dlss_override_dlss_rr{false};

namespace settings {

StreamlineTabSettings::StreamlineTabSettings()
    : dlss_override_enabled("dlss_override_enabled", false, "DisplayCommander"),
      dlss_override_folder("dlss_override_folder", "", "DisplayCommander"),
      dlss_override_dlss("dlss_override_dlss", s_dlss_override_dlss, false, "DisplayCommander"),
      dlss_override_dlss_fg("dlss_override_dlss_fg", s_dlss_override_dlss_fg, false, "DisplayCommander"),
      dlss_override_dlss_rr("dlss_override_dlss_rr", s_dlss_override_dlss_rr, false, "DisplayCommander") {

    // Initialize the all_settings_ vector
    all_settings_ = {
        &dlss_override_enabled,
        &dlss_override_folder,
        &dlss_override_dlss,
        &dlss_override_dlss_fg,
        &dlss_override_dlss_rr,
    };
}

void StreamlineTabSettings::LoadAll() {
    LogInfo("StreamlineTabSettings::LoadAll() called");
    LoadTabSettings(all_settings_);
    LogInfo("StreamlineTabSettings::LoadAll() completed");
}

std::vector<ui::new_ui::SettingBase*> StreamlineTabSettings::GetAllSettings() {
    return all_settings_;
}

} // namespace settings
