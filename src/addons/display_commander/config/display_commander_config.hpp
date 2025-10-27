#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../utils/srwlock_wrapper.hpp"

namespace display_commander::config {

// Forward declaration
class IniFile;

// Configuration manager for DisplayCommander settings
class DisplayCommanderConfigManager {
public:
    static DisplayCommanderConfigManager& GetInstance();

    // Initialize the config system
    void Initialize();

    // Get configuration value (compatible with reshade::get_config_value API)
    bool GetConfigValue(const char* section, const char* key, std::string& value);
    bool GetConfigValue(const char* section, const char* key, int& value);
    bool GetConfigValue(const char* section, const char* key, uint32_t& value);
    bool GetConfigValue(const char* section, const char* key, float& value);
    bool GetConfigValue(const char* section, const char* key, double& value);
    bool GetConfigValue(const char* section, const char* key, bool& value);
    bool GetConfigValue(const char* section, const char* key, std::vector<std::string>& values);

    // Set configuration value (compatible with reshade::set_config_value API)
    void SetConfigValue(const char* section, const char* key, const std::string& value);
    void SetConfigValue(const char* section, const char* key, const char* value);
    void SetConfigValue(const char* section, const char* key, int value);
    void SetConfigValue(const char* section, const char* key, uint32_t value);
    void SetConfigValue(const char* section, const char* key, float value);
    void SetConfigValue(const char* section, const char* key, double value);
    void SetConfigValue(const char* section, const char* key, bool value);
    void SetConfigValue(const char* section, const char* key, const std::vector<std::string>& values);

    // Save configuration to file
    void SaveConfig();

    // Get config file path
    std::string GetConfigPath() const;

private:
    DisplayCommanderConfigManager() = default;
    ~DisplayCommanderConfigManager() = default;
    DisplayCommanderConfigManager(const DisplayCommanderConfigManager&) = delete;
    DisplayCommanderConfigManager& operator=(const DisplayCommanderConfigManager&) = delete;

    void EnsureConfigFileExists();
    std::string GetConfigFilePath();

    std::unique_ptr<IniFile> config_file_;
    std::string config_path_;
    mutable SRWLOCK config_mutex_ = SRWLOCK_INIT;
    bool initialized_ = false;
};

// Global functions that replace reshade::get_config_value and reshade::set_config_value
bool get_config_value(const char* section, const char* key, std::string& value);
bool get_config_value(const char* section, const char* key, int& value);
bool get_config_value(const char* section, const char* key, uint32_t& value);
bool get_config_value(const char* section, const char* key, float& value);
bool get_config_value(const char* section, const char* key, double& value);
bool get_config_value(const char* section, const char* key, bool& value);
bool get_config_value(const char* section, const char* key, std::vector<std::string>& values);
// Special overload for char buffer with size (compatible with ReShade API)
bool get_config_value(const char* section, const char* key, char* buffer, size_t* buffer_size);

void set_config_value(const char* section, const char* key, const std::string& value);
void set_config_value(const char* section, const char* key, const char* value);
void set_config_value(const char* section, const char* key, int value);
void set_config_value(const char* section, const char* key, uint32_t value);
void set_config_value(const char* section, const char* key, float value);
void set_config_value(const char* section, const char* key, double value);
void set_config_value(const char* section, const char* key, bool value);
void set_config_value(const char* section, const char* key, const std::vector<std::string>& values);

// Save configuration to file
void save_config();

} // namespace display_commander::config
