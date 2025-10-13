#include "display_commander_config.hpp"
#include "../utils.hpp"
#include "../utils/display_commander_logger.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <cctype>

namespace display_commander::config {

// Simple INI file parser/writer
class IniFile {
public:
    struct Section {
        std::string name;
        std::vector<std::pair<std::string, std::string>> key_values;
    };

    bool LoadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        sections_.clear();
        std::string line;
        Section* current_section = nullptr;

        while (std::getline(file, line)) {
            // Remove leading/trailing whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            // Skip empty lines and comments
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }

            // Check for section header [section_name]
            if (line[0] == '[' && line.back() == ']') {
                std::string section_name = line.substr(1, line.length() - 2);
                sections_.push_back({section_name, {}});
                current_section = &sections_.back();
            }
            // Check for key=value pair
            else if (current_section != nullptr) {
                size_t equal_pos = line.find('=');
                if (equal_pos != std::string::npos) {
                    std::string key = line.substr(0, equal_pos);
                    std::string value = line.substr(equal_pos + 1);

                    // Remove leading/trailing whitespace from key and value
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);

                    current_section->key_values.push_back({key, value});
                }
            }
        }

        return true;
    }

    bool SaveToFile(const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        for (const auto& section : sections_) {
            file << "[" << section.name << "]\n";
            for (const auto& kv : section.key_values) {
                file << kv.first << "=" << kv.second << "\n";
            }
            file << "\n";
        }

        return true;
    }

    bool GetValue(const std::string& section, const std::string& key, std::string& value) const {
        for (const auto& s : sections_) {
            if (s.name == section) {
                for (const auto& kv : s.key_values) {
                    if (kv.first == key) {
                        value = kv.second;

                        // Migration: Check if this is a device ID setting that was saved as an integer
                        if ((key.find("device_id") != std::string::npos || key.find("display_device_id") != std::string::npos ||
                             key == "target_display") && !value.empty() &&
                            std::all_of(value.begin(), value.end(), ::isdigit)) {
                            // This is likely an old integer value, return empty string to trigger default
                            value = "";
                            // Mark for removal from config file
                            const_cast<IniFile*>(this)->SetValue(section, key, "");
                        }

                        return true;
                    }
                }
            }
        }
        return false;
    }

    void SetValue(const std::string& section, const std::string& key, const std::string& value) {
        // Find existing section
        for (auto& s : sections_) {
            if (s.name == section) {
                // Find existing key in section
                for (auto& kv : s.key_values) {
                    if (kv.first == key) {
                        kv.second = value;
                        return;
                    }
                }
                // Key not found, add it
                s.key_values.push_back({key, value});
                return;
            }
        }
        // Section not found, create it
        sections_.push_back({section, {{key, value}}});
    }

    bool GetValue(const std::string& section, const std::string& key, std::vector<std::string>& values) const {
        values.clear();
        std::string value_str;
        if (GetValue(section, key, value_str)) {
            // Split by null character (ReShade format)
            std::stringstream ss(value_str);
            std::string item;
            while (std::getline(ss, item, '\0')) {
                if (!item.empty()) {
                    values.push_back(item);
                }
            }
            return !values.empty();
        }
        return false;
    }

    void SetValue(const std::string& section, const std::string& key, const std::vector<std::string>& values) {
        std::string value_str;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                value_str += '\0';
            }
            value_str += values[i];
        }
        SetValue(section, key, value_str);
    }

private:
    std::vector<Section> sections_;
};

// DisplayCommanderConfigManager implementation
DisplayCommanderConfigManager& DisplayCommanderConfigManager::GetInstance() {
    static DisplayCommanderConfigManager instance;
    return instance;
}

void DisplayCommanderConfigManager::Initialize() {
    std::lock_guard<std::mutex> lock(config_mutex_);

    if (initialized_) {
        return;
    }

    config_path_ = GetConfigFilePath();
    config_file_ = std::make_unique<IniFile>();

    // Initialize logger with DisplayCommander.log in the same directory as config
    std::filesystem::path config_dir = std::filesystem::path(config_path_).parent_path();
    std::string log_path = (config_dir / "DisplayCommander.log").string();
    display_commander::logger::Initialize(log_path);

    // Test the logger
    display_commander::logger::LogInfo("DisplayCommander config system initializing - logger test successful");

    EnsureConfigFileExists();

    // Load existing config if it exists
    if (!config_file_->LoadFromFile(config_path_)) {
        LogInfo("DisplayCommanderConfigManager: Created new config file at %s", config_path_.c_str());
    } else {
        LogInfo("DisplayCommanderConfigManager: Loaded existing config from %s", config_path_.c_str());
    }

    initialized_ = true;
}

bool DisplayCommanderConfigManager::GetConfigValue(const char* section, const char* key, std::string& value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (!initialized_) {
        Initialize();
    }
    return config_file_->GetValue(section != nullptr ? section : "", key != nullptr ? key : "", value);
}

bool DisplayCommanderConfigManager::GetConfigValue(const char* section, const char* key, int& value) {
    std::string str_value;
    if (GetConfigValue(section, key, str_value)) {
        try {
            value = std::stoi(str_value);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    return false;
}

bool DisplayCommanderConfigManager::GetConfigValue(const char* section, const char* key, uint32_t& value) {
    std::string str_value;
    if (GetConfigValue(section, key, str_value)) {
        try {
            value = static_cast<uint32_t>(std::stoul(str_value));
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    return false;
}

bool DisplayCommanderConfigManager::GetConfigValue(const char* section, const char* key, float& value) {
    std::string str_value;
    if (GetConfigValue(section, key, str_value)) {
        try {
            value = std::stof(str_value);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    return false;
}

bool DisplayCommanderConfigManager::GetConfigValue(const char* section, const char* key, double& value) {
    std::string str_value;
    if (GetConfigValue(section, key, str_value)) {
        try {
            value = std::stod(str_value);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    return false;
}

bool DisplayCommanderConfigManager::GetConfigValue(const char* section, const char* key, bool& value) {
    int int_value;
    if (GetConfigValue(section, key, int_value)) {
        value = (int_value != 0);
        return true;
    }
    return false;
}

bool DisplayCommanderConfigManager::GetConfigValue(const char* section, const char* key, std::vector<std::string>& values) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (!initialized_) {
        Initialize();
    }
    return config_file_->GetValue(section != nullptr ? section : "", key != nullptr ? key : "", values);
}

void DisplayCommanderConfigManager::SetConfigValue(const char* section, const char* key, const std::string& value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (!initialized_) {
        Initialize();
    }
    config_file_->SetValue(section != nullptr ? section : "", key != nullptr ? key : "", value);
}

void DisplayCommanderConfigManager::SetConfigValue(const char* section, const char* key, const char* value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (!initialized_) {
        Initialize();
    }
    config_file_->SetValue(section != nullptr ? section : "", key != nullptr ? key : "", value != nullptr ? value : "");
}

void DisplayCommanderConfigManager::SetConfigValue(const char* section, const char* key, int value) {
    SetConfigValue(section, key, std::to_string(value));
}

void DisplayCommanderConfigManager::SetConfigValue(const char* section, const char* key, uint32_t value) {
    SetConfigValue(section, key, std::to_string(value));
}

void DisplayCommanderConfigManager::SetConfigValue(const char* section, const char* key, float value) {
    SetConfigValue(section, key, std::to_string(value));
}

void DisplayCommanderConfigManager::SetConfigValue(const char* section, const char* key, double value) {
    SetConfigValue(section, key, std::to_string(value));
}

void DisplayCommanderConfigManager::SetConfigValue(const char* section, const char* key, bool value) {
    SetConfigValue(section, key, value ? 1 : 0);
}

void DisplayCommanderConfigManager::SetConfigValue(const char* section, const char* key, const std::vector<std::string>& values) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (!initialized_) {
        Initialize();
    }
    config_file_->SetValue(section != nullptr ? section : "", key != nullptr ? key : "", values);
}

void DisplayCommanderConfigManager::SaveConfig() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (!initialized_) {
        return;
    }

    EnsureConfigFileExists();
    if (config_file_->SaveToFile(config_path_)) {
        LogInfo("DisplayCommanderConfigManager: Saved config to %s", config_path_.c_str());
    } else {
        LogError("DisplayCommanderConfigManager: Failed to save config to %s", config_path_.c_str());
    }
}

std::string DisplayCommanderConfigManager::GetConfigPath() const {
    return config_path_;
}

void DisplayCommanderConfigManager::EnsureConfigFileExists() {
    if (config_path_.empty()) {
        config_path_ = GetConfigFilePath();
    }

    // Create directory if it doesn't exist
    std::filesystem::path config_dir = std::filesystem::path(config_path_).parent_path();
    if (!config_dir.empty() && !std::filesystem::exists(config_dir)) {
        std::filesystem::create_directories(config_dir);
    }
}

std::string DisplayCommanderConfigManager::GetConfigFilePath() {
    // Get the directory where the addon is located
    char module_path[MAX_PATH];

    // Try to get the module handle using a static variable address
    static int dummy = 0;
    HMODULE hModule = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(&dummy), &hModule) != 0) {
        GetModuleFileNameA(hModule, module_path, MAX_PATH);
    } else {
        // Fallback to current directory
        GetCurrentDirectoryA(MAX_PATH, module_path);
    }

    std::filesystem::path addon_dir = std::filesystem::path(module_path).parent_path();
    return (addon_dir / "DisplayCommander.ini").string();
}

// Global function implementations
bool get_config_value(const char* section, const char* key, std::string& value) {
    return DisplayCommanderConfigManager::GetInstance().GetConfigValue(section, key, value);
}

bool get_config_value(const char* section, const char* key, int& value) {
    return DisplayCommanderConfigManager::GetInstance().GetConfigValue(section, key, value);
}

bool get_config_value(const char* section, const char* key, uint32_t& value) {
    return DisplayCommanderConfigManager::GetInstance().GetConfigValue(section, key, value);
}

bool get_config_value(const char* section, const char* key, float& value) {
    return DisplayCommanderConfigManager::GetInstance().GetConfigValue(section, key, value);
}

bool get_config_value(const char* section, const char* key, double& value) {
    return DisplayCommanderConfigManager::GetInstance().GetConfigValue(section, key, value);
}

bool get_config_value(const char* section, const char* key, bool& value) {
    return DisplayCommanderConfigManager::GetInstance().GetConfigValue(section, key, value);
}

bool get_config_value(const char* section, const char* key, std::vector<std::string>& values) {
    return DisplayCommanderConfigManager::GetInstance().GetConfigValue(section, key, values);
}

bool get_config_value(const char* section, const char* key, char* buffer, size_t* buffer_size) {
    std::string value;
    if (DisplayCommanderConfigManager::GetInstance().GetConfigValue(section, key, value)) {
        if (buffer != nullptr && buffer_size != nullptr) {
            size_t copy_size = (value.length() < (*buffer_size - 1)) ? value.length() : (*buffer_size - 1);
            strncpy_s(buffer, *buffer_size, value.c_str(), copy_size);
            buffer[copy_size] = '\0';
            *buffer_size = copy_size + 1;
            return true;
        }
    }
    return false;
}

void set_config_value(const char* section, const char* key, const std::string& value) {
    DisplayCommanderConfigManager::GetInstance().SetConfigValue(section, key, value);
}

void set_config_value(const char* section, const char* key, const char* value) {
    DisplayCommanderConfigManager::GetInstance().SetConfigValue(section, key, value);
}

void set_config_value(const char* section, const char* key, int value) {
    DisplayCommanderConfigManager::GetInstance().SetConfigValue(section, key, value);
}

void set_config_value(const char* section, const char* key, uint32_t value) {
    DisplayCommanderConfigManager::GetInstance().SetConfigValue(section, key, value);
}

void set_config_value(const char* section, const char* key, float value) {
    DisplayCommanderConfigManager::GetInstance().SetConfigValue(section, key, value);
}

void set_config_value(const char* section, const char* key, double value) {
    DisplayCommanderConfigManager::GetInstance().SetConfigValue(section, key, value);
}

void set_config_value(const char* section, const char* key, bool value) {
    DisplayCommanderConfigManager::GetInstance().SetConfigValue(section, key, value);
}

void set_config_value(const char* section, const char* key, const std::vector<std::string>& values) {
    DisplayCommanderConfigManager::GetInstance().SetConfigValue(section, key, values);
}

void save_config() {
    DisplayCommanderConfigManager::GetInstance().SaveConfig();
}

} // namespace display_commander::config
