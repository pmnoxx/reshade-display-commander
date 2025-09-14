#pragma once

#include <windows.h>
#include <xinput.h>
#include <vector>
#include <memory>
#include <atomic>
#include <string>

// Forward declarations for XInput_HID types
using GetInputReport_pfn = bool (*)(void*);

// HID device structure (simplified version for our use)
struct hid_device_file_s {
    wchar_t wszDevicePath[MAX_PATH] = {};
    HANDLE hDeviceFile = INVALID_HANDLE_VALUE;
    bool bConnected = false;
    bool bWireless = false;

    struct {
        USHORT vid = 0;
        USHORT pid = 0;
    } devinfo;

    struct {
        XINPUT_STATE current = {};
        XINPUT_STATE prev = {};
        DWORD input_timestamp = 0;
    } state;

    std::vector<BYTE> input_report;
    GetInputReport_pfn get_input_report = nullptr;

    bool GetInputReport() {
        if (get_input_report) {
            return get_input_report(this);
        }
        return false;
    }
};

namespace display_commander::dualsense {

// DualSense device information
struct DualSenseDevice {
    std::string device_path;
    std::string device_name;
    std::string connection_type; // "USB" or "Bluetooth"
    USHORT vendor_id;
    USHORT product_id;
    bool is_connected;
    bool is_wireless;
    DWORD last_update_time;
    DWORD input_timestamp;

    // Device state
    XINPUT_STATE current_state;
    XINPUT_STATE previous_state;

    // Device-specific features
    bool has_adaptive_triggers;
    bool has_touchpad;
    bool has_microphone;
    bool has_speaker;

    // Battery information (if available)
    bool battery_info_valid;
    BYTE battery_level;
    BYTE battery_type;

    // XInput_HID integration
    std::shared_ptr<hid_device_file_s> hid_device;
    GetInputReport_pfn get_input_report;

    DualSenseDevice() : vendor_id(0), product_id(0), is_connected(false),
                       is_wireless(false), last_update_time(0), input_timestamp(0),
                       has_adaptive_triggers(false), has_touchpad(false),
                       has_microphone(false), has_speaker(false),
                       battery_info_valid(false), battery_level(0), battery_type(0),
                       hid_device(nullptr), get_input_report(nullptr) {
        ZeroMemory(&current_state, sizeof(XINPUT_STATE));
        ZeroMemory(&previous_state, sizeof(XINPUT_STATE));
    }
};

// DualSense HID wrapper class
class DualSenseHIDWrapper {
public:
    DualSenseHIDWrapper();
    ~DualSenseHIDWrapper();

    // Initialize the wrapper
    bool Initialize();

    // Cleanup the wrapper
    void Cleanup();

    // Enumerate connected DualSense devices
    void EnumerateDevices();

    // Update device states (poll input)
    void UpdateDeviceStates();

    // Get list of devices
    const std::vector<DualSenseDevice>& GetDevices() const { return devices_; }

    // Get device by index
    DualSenseDevice* GetDevice(size_t index);

    // Check if device is DualSense
    bool IsDualSenseDevice(USHORT vendor_id, USHORT product_id) const;

    // Check if device matches selected HID type
    bool IsDeviceTypeEnabled(USHORT vendor_id, USHORT product_id, int hid_type) const;

    // Get device type string
    std::string GetDeviceTypeString(USHORT vendor_id, USHORT product_id) const;

    // Set HID device type filter
    void SetHIDTypeFilter(int hid_type) { hid_type_filter_ = hid_type; }

    // Input report processing
    void ProcessUSBInputReport(DualSenseDevice& device, const BYTE* inputReport, DWORD bytesRead);
    void ProcessBluetoothInputReport(DualSenseDevice& device, const BYTE* inputReport, DWORD bytesRead);
    void ParseDualSenseButtons(DualSenseDevice& device, const BYTE* inputReport);
    void ParseDualSenseSticks(DualSenseDevice& device, const BYTE* inputReport);
    void ParseDualSenseTriggers(DualSenseDevice& device, const BYTE* inputReport);

private:
    // Device management
    std::vector<DualSenseDevice> devices_;
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> enumeration_in_progress_{false};
    std::atomic<int> hid_type_filter_{0}; // 0 = Auto, 1 = DualSense Regular, 2 = DualSense Edge, 3 = DualShock 4, 4 = All Sony

    // XInput_HID integration
    bool SetupXInputHIDIntegration();
    bool CreateHIDDevice(const std::string& device_path, DualSenseDevice& device);
    void UpdateDeviceFromHID(DualSenseDevice& device);

    // Device enumeration helpers
    void EnumerateHIDDevices();
    bool OpenDevice(const std::string& device_path, HANDLE& handle);
    bool GetDeviceAttributes(HANDLE handle, USHORT& vendor_id, USHORT& product_id);
    bool DetermineConnectionType(const std::string& device_path, bool& is_wireless);
};

// Global instance
extern std::unique_ptr<DualSenseHIDWrapper> g_dualsense_hid_wrapper;

// Global functions
void InitializeDualSenseHID();
void CleanupDualSenseHID();
void EnumerateDualSenseDevices();
void UpdateDualSenseDeviceStates();

} // namespace display_commander::dualsense
