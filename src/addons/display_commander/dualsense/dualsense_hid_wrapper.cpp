#include "dualsense_hid_wrapper.hpp"
#include "../utils.hpp"
#include <deps/imgui/imgui.h>
#include <include/reshade.hpp>
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

namespace display_commander::dualsense {

// Global instance
std::unique_ptr<DualSenseHIDWrapper> g_dualsense_hid_wrapper = nullptr;

DualSenseHIDWrapper::DualSenseHIDWrapper() {
    // Initialize empty
}

DualSenseHIDWrapper::~DualSenseHIDWrapper() {
    Cleanup();
}

bool DualSenseHIDWrapper::Initialize() {
    if (is_initialized_.load()) {
        return true;
    }

    LogInfo("DualSenseHIDWrapper::Initialize() - Starting DualSense HID wrapper initialization");

    // Setup XInput_HID integration
    if (!SetupXInputHIDIntegration()) {
        LogError("DualSenseHIDWrapper::Initialize() - Failed to setup XInput_HID integration");
        return false;
    }

    // Enumerate devices
    EnumerateDevices();

    is_initialized_.store(true);
    LogInfo("DualSenseHIDWrapper::Initialize() - DualSense HID wrapper initialization complete");

    return true;
}

void DualSenseHIDWrapper::Cleanup() {
    if (!is_initialized_.load()) {
        return;
    }

    LogInfo("DualSenseHIDWrapper::Cleanup() - Cleaning up DualSense HID wrapper");

    // Cleanup devices
    for (auto& device : devices_) {
        if (device.hid_device && device.hid_device->hDeviceFile != INVALID_HANDLE_VALUE) {
            CloseHandle(device.hid_device->hDeviceFile);
        }
    }

    devices_.clear();
    is_initialized_.store(false);
}

void DualSenseHIDWrapper::EnumerateDevices() {
    if (enumeration_in_progress_.exchange(true)) {
        return;
    }

    LogInfo("DualSenseHIDWrapper::EnumerateDevices() - Starting device enumeration");

    // Clear existing devices
    devices_.clear();

    // Enumerate HID devices
    EnumerateHIDDevices();

    enumeration_in_progress_.store(false);
    LogInfo("DualSenseHIDWrapper::EnumerateDevices() - Found %zu DualSense device(s)", devices_.size());
}

void DualSenseHIDWrapper::EnumerateHIDDevices() {
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, nullptr, nullptr, DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        LogError("DualSenseHIDWrapper::EnumerateHIDDevices() - Failed to get HID device info set");
        return;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {};
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD deviceIndex = 0;
    while (SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_HID, deviceIndex, &deviceInterfaceData)) {
        // Get required buffer size
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);

        if (requiredSize > 0) {
            std::vector<BYTE> buffer(requiredSize);
            auto* pDeviceInterfaceDetail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(buffer.data());
            pDeviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            SP_DEVINFO_DATA deviceInfoData = {};
            deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

            if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, pDeviceInterfaceDetail, requiredSize, nullptr, &deviceInfoData)) {
                // Open the device
                HANDLE hDevice = CreateFile(pDeviceInterfaceDetail->DevicePath,
                                          GENERIC_READ | GENERIC_WRITE,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          nullptr, OPEN_EXISTING, 0, nullptr);

                if (hDevice != INVALID_HANDLE_VALUE) {
                    // Get device attributes
                    HIDD_ATTRIBUTES attributes = {};
                    attributes.Size = sizeof(HIDD_ATTRIBUTES);

                    if (HidD_GetAttributes(hDevice, &attributes)) {
                        // Check if this is a supported device based on current filter
                        int current_filter = hid_type_filter_.load();
                        if (IsDeviceTypeEnabled(attributes.VendorID, attributes.ProductID, current_filter)) {
                            DualSenseDevice device;

                            // Convert device path to string
                            device.device_path = pDeviceInterfaceDetail->DevicePath;

                            device.vendor_id = attributes.VendorID;
                            device.product_id = attributes.ProductID;
                            device.is_connected = true;

                            // Determine connection type
                            DetermineConnectionType(device.device_path, device.is_wireless);
                            device.connection_type = device.is_wireless ? "Bluetooth" : "USB";

                            // Set device name
                            device.device_name = GetDeviceTypeString(device.vendor_id, device.product_id);

                            // Set DualSense-specific features
                            device.has_adaptive_triggers = true;
                            device.has_touchpad = true;
                            device.has_microphone = true;
                            device.has_speaker = true;

                            // Initialize state
                            ZeroMemory(&device.current_state, sizeof(XINPUT_STATE));
                            ZeroMemory(&device.previous_state, sizeof(XINPUT_STATE));
                            device.last_update_time = GetTickCount();

                            // Create HID device wrapper
                            if (CreateHIDDevice(device.device_path, device)) {
                                devices_.push_back(device);

                                LogInfo("DualSenseHIDWrapper::EnumerateHIDDevices() - Found DualSense device: %s [VID:0x%04X PID:0x%04X] %s",
                                       device.device_name.c_str(), device.vendor_id, device.product_id, device.connection_type.c_str());
                            } else {
                                LogError("DualSenseHIDWrapper::EnumerateHIDDevices() - Failed to create HID device wrapper for %s", device.device_name.c_str());
                            }
                        }
                    }

                    CloseHandle(hDevice);
                }
            }
        }

        deviceIndex++;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
}

bool DualSenseHIDWrapper::CreateHIDDevice(const std::string& device_path, DualSenseDevice& device) {
    // For now, we'll create a minimal HID device structure
    // In a full implementation, this would integrate with XInput_HID's device management

    // Create a basic HID device structure
    auto hid_device = std::make_shared<hid_device_file_s>();

    // Convert string to wide string for device path
    std::wstring wide_path(device_path.begin(), device_path.end());
    wcscpy_s(hid_device->wszDevicePath, MAX_PATH, wide_path.c_str());

    // Open the device file
    hid_device->hDeviceFile = CreateFileW(wide_path.c_str(),
                                       GENERIC_READ | GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       nullptr, OPEN_EXISTING, 0, nullptr);

    if (hid_device->hDeviceFile == INVALID_HANDLE_VALUE) {
        LogError("DualSenseHIDWrapper::CreateHIDDevice() - Failed to open device: %s", device_path.c_str());
        return false;
    }

    // Set up input report buffer
    hid_device->input_report.resize(64); // Standard HID report size

    // Set up the input report function based on connection type
    if (device.is_wireless) {
        // For Bluetooth, we would use SK_DualSense_GetInputReportBt
        // For now, we'll use a stub
        hid_device->get_input_report = nullptr; // Will be set up later
    } else {
        // For USB, we would use SK_DualSense_GetInputReportUSB
        // For now, we'll use a stub
        hid_device->get_input_report = nullptr; // Will be set up later
    }

    // Set device info
    hid_device->devinfo.vid = device.vendor_id;
    hid_device->devinfo.pid = device.product_id;
    hid_device->bConnected = true;
    hid_device->bWireless = device.is_wireless;

    device.hid_device = hid_device;

    return true;
}

void DualSenseHIDWrapper::UpdateDeviceStates() {
    for (auto& device : devices_) {
        if (device.is_connected && device.hid_device) {
            UpdateDeviceFromHID(device);
        }
    }
}

void DualSenseHIDWrapper::UpdateDeviceFromHID(DualSenseDevice& device) {
    if (!device.hid_device || device.hid_device->hDeviceFile == INVALID_HANDLE_VALUE) {
        return;
    }

    // Store previous state
    device.previous_state = device.current_state;

    // Read input report directly from HID device
    DWORD bytesRead = 0;
    BYTE inputReport[78] = {0}; // Max size for Bluetooth reports

    // Try to read input report
    if (ReadFile(device.hid_device->hDeviceFile, inputReport, sizeof(inputReport), &bytesRead, nullptr)) {
        if (bytesRead > 0) {
            // Update timestamp
            device.last_update_time = GetTickCount();
            device.input_timestamp = GetTickCount();

            // Debug: Log raw input report (first few times)
            static int debug_count = 0;
            if (debug_count++ < 5) { // Only log first 5 reports
                LogInfo("DualSense raw input report [%d bytes]: %02X %02X %02X %02X %02X %02X %02X %02X...",
                       bytesRead, inputReport[0], inputReport[1], inputReport[2], inputReport[3],
                       inputReport[4], inputReport[5], inputReport[6], inputReport[7]);
            }

            // Store the raw input report for debugging
            if (device.hid_device) {
                // Resize if needed to accommodate the actual report size
                if (device.hid_device->input_report.size() < bytesRead) {
                    device.hid_device->input_report.resize(bytesRead);
                }

                std::memcpy(device.hid_device->input_report.data(), inputReport, bytesRead);

                // Zero out the rest if the report is shorter than expected
                if (bytesRead < device.hid_device->input_report.size()) {
                    std::memset(device.hid_device->input_report.data() + bytesRead, 0,
                               device.hid_device->input_report.size() - bytesRead);
                }

                // Debug: Log that we stored the input report
                static int store_debug_count = 0;
                if (store_debug_count++ < 3) {
                    LogInfo("Stored input report [%d bytes] for device %s",
                           bytesRead, device.device_name.c_str());
                }
            }

            // Process the input report based on connection type
            if (device.is_wireless) {
                ProcessBluetoothInputReport(device, inputReport, bytesRead);
            } else {
                ProcessUSBInputReport(device, inputReport, bytesRead);
            }

            // Update packet number for change detection
            device.current_state.dwPacketNumber++;

            // Check for state changes
            if (device.current_state.dwPacketNumber != device.previous_state.dwPacketNumber) {
                // State changed - we could trigger events here
                LogInfo("DualSense input state changed for device %s - Buttons: 0x%04X, LStick: (%d,%d), RStick: (%d,%d), LTrig: %d, RTrig: %d",
                       device.device_name.c_str(), device.current_state.Gamepad.wButtons,
                       device.current_state.Gamepad.sThumbLX, device.current_state.Gamepad.sThumbLY,
                       device.current_state.Gamepad.sThumbRX, device.current_state.Gamepad.sThumbRY,
                       device.current_state.Gamepad.bLeftTrigger, device.current_state.Gamepad.bRightTrigger);
            }
        }
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
            LogError("Failed to read input report from DualSense device: %s, error: %lu", device.device_name.c_str(), error);
        }
    }
}

DualSenseDevice* DualSenseHIDWrapper::GetDevice(size_t index) {
    if (index >= devices_.size()) {
        return nullptr;
    }
    return &devices_[index];
}

bool DualSenseHIDWrapper::IsDualSenseDevice(USHORT vendor_id, USHORT product_id) const {
    // Sony vendor ID
    if (vendor_id != 0x054c) {
        return false;
    }

    // DualSense product IDs
    return (product_id == 0x0CE6) ||  // DualSense Controller (Regular)
           (product_id == 0x0DF2);    // DualSense Edge Controller
}

bool DualSenseHIDWrapper::IsDeviceTypeEnabled(USHORT vendor_id, USHORT product_id, int hid_type) const {
    // Sony vendor ID
    if (vendor_id != 0x054c) {
        return false;
    }

    switch (hid_type) {
        case 0: // Auto - detect all supported devices
            return (product_id == 0x0CE6) ||  // DualSense Controller (Regular)
                   (product_id == 0x0DF2) ||  // DualSense Edge Controller
                   (product_id == 0x05C4) ||  // DualShock 4 Controller
                   (product_id == 0x09CC) ||  // DualShock 4 Controller (Rev 2)
                   (product_id == 0x0BA0);    // DualShock 4 Controller (Dongle)
        case 1: // DualSense Regular only
            return (product_id == 0x0CE6);
        case 2: // DualSense Edge only
            return (product_id == 0x0DF2);
        case 3: // DualShock 4 only
            return (product_id == 0x05C4) ||  // DualShock 4 Controller
                   (product_id == 0x09CC) ||  // DualShock 4 Controller (Rev 2)
                   (product_id == 0x0BA0);    // DualShock 4 Controller (Dongle)
        case 4: // All Sony controllers
            return true;
        default:
            return false;
    }
}

std::string DualSenseHIDWrapper::GetDeviceTypeString(USHORT vendor_id, USHORT product_id) const {
    if (vendor_id == 0x054c) { // Sony
        switch (product_id) {
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

bool DualSenseHIDWrapper::DetermineConnectionType(const std::string& device_path, bool& is_wireless) {
    // Check if the device path contains Bluetooth indicators
    is_wireless = (device_path.find("&col01") == std::string::npos) &&
                  (device_path.find("bluetooth") != std::string::npos ||
                   device_path.find("bt") != std::string::npos);
    return true;
}

bool DualSenseHIDWrapper::SetupXInputHIDIntegration() {
    // This is where we would integrate with XInput_HID
    // For now, we'll return true as a placeholder
    // In a full implementation, this would:
    // 1. Load XInput_HID library
    // 2. Get function pointers for DualSense input report functions
    // 3. Set up device enumeration callbacks
    return true;
}

// Global functions
void InitializeDualSenseHID() {
    if (!g_dualsense_hid_wrapper) {
        g_dualsense_hid_wrapper = std::make_unique<DualSenseHIDWrapper>();
        g_dualsense_hid_wrapper->Initialize();
    }
}

void CleanupDualSenseHID() {
    if (g_dualsense_hid_wrapper) {
        g_dualsense_hid_wrapper->Cleanup();
        g_dualsense_hid_wrapper.reset();
    }
}

void EnumerateDualSenseDevices() {
    if (g_dualsense_hid_wrapper) {
        g_dualsense_hid_wrapper->EnumerateDevices();
    }
}

void UpdateDualSenseDeviceStates() {
    if (g_dualsense_hid_wrapper) {
        g_dualsense_hid_wrapper->UpdateDeviceStates();
    }
}

// Input report processing methods
void DualSenseHIDWrapper::ProcessUSBInputReport(DualSenseDevice& device, const BYTE* inputReport, DWORD bytesRead) {
    // DualSense USB input report format
    // Report ID 0x01, 64 bytes total
    if (bytesRead < 64 || inputReport[0] != 0x01) {
        return;
    }

    // Parse the input report
    ParseDualSenseButtons(device, inputReport);
    ParseDualSenseSticks(device, inputReport);
    ParseDualSenseTriggers(device, inputReport);
}

void DualSenseHIDWrapper::ProcessBluetoothInputReport(DualSenseDevice& device, const BYTE* inputReport, DWORD bytesRead) {
    // DualSense Bluetooth input report format
    // Report ID 0x31, 78 bytes total
    if (bytesRead < 78 || inputReport[0] != 0x31) {
        return;
    }

    // Parse the input report (same format as USB but with different report ID)
    ParseDualSenseButtons(device, inputReport);
    ParseDualSenseSticks(device, inputReport);
    ParseDualSenseTriggers(device, inputReport);
}

void DualSenseHIDWrapper::ParseDualSenseButtons(DualSenseDevice& device, const BYTE* inputReport) {
    // DualSense button layout (based on actual USB report format)
    // Byte 1: Button states
    // Bit 0: Square, Bit 1: Cross, Bit 2: Circle, Bit 3: Triangle
    // Bit 4: L1, Bit 5: R1, Bit 6: L2, Bit 7: R2
    // Byte 2: Button states continued
    // Bit 0: Share, Bit 1: Options, Bit 2: L3, Bit 3: R3
    // Bit 4: PS, Bit 5: Touchpad, Bit 6: Mute, Bit 7: (unused)
    // Byte 3: D-pad (0-7 for 8 directions, 8 for neutral)

    WORD buttons = 0;

    // Map DualSense buttons to XInput buttons
    if (inputReport[1] & 0x10) buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;  // L1
    if (inputReport[1] & 0x20) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER; // R1
    // Note: L2 and R2 are handled as triggers, not buttons in XInput

    if (inputReport[2] & 0x01) buttons |= XINPUT_GAMEPAD_BACK;           // Share
    if (inputReport[2] & 0x02) buttons |= XINPUT_GAMEPAD_START;          // Options
    if (inputReport[2] & 0x04) buttons |= XINPUT_GAMEPAD_LEFT_THUMB;     // L3
    if (inputReport[2] & 0x08) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;    // R3
    // Note: PS button doesn't have a direct XInput equivalent, we'll use a custom constant
    if (inputReport[2] & 0x10) buttons |= 0x0400;                        // PS (custom constant)

    // Face buttons (mapped to XInput layout)
    if (inputReport[1] & 0x01) buttons |= XINPUT_GAMEPAD_X;              // Square -> X
    if (inputReport[1] & 0x02) buttons |= XINPUT_GAMEPAD_A;              // Cross -> A
    if (inputReport[1] & 0x04) buttons |= XINPUT_GAMEPAD_B;              // Circle -> B
    if (inputReport[1] & 0x08) buttons |= XINPUT_GAMEPAD_Y;              // Triangle -> Y

    // D-pad
    BYTE dpad = inputReport[3] & 0x0F;
    switch (dpad) {
        case 0: buttons |= XINPUT_GAMEPAD_DPAD_UP; break;
        case 1: buttons |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_RIGHT; break;
        case 2: buttons |= XINPUT_GAMEPAD_DPAD_RIGHT; break;
        case 3: buttons |= XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_RIGHT; break;
        case 4: buttons |= XINPUT_GAMEPAD_DPAD_DOWN; break;
        case 5: buttons |= XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT; break;
        case 6: buttons |= XINPUT_GAMEPAD_DPAD_LEFT; break;
        case 7: buttons |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_LEFT; break;
        // case 8: neutral (no buttons)
    }

    device.current_state.Gamepad.wButtons = buttons;
}

void DualSenseHIDWrapper::ParseDualSenseSticks(DualSenseDevice& device, const BYTE* inputReport) {
    // Left stick: bytes 4-5 (X), 6-7 (Y)
    // Right stick: bytes 8-9 (X), 10-11 (Y)
    // Values are 16-bit signed integers

    // Left stick
    SHORT leftX = static_cast<SHORT>((inputReport[5] << 8) | inputReport[4]);
    SHORT leftY = static_cast<SHORT>((inputReport[7] << 8) | inputReport[6]);

    // Right stick
    SHORT rightX = static_cast<SHORT>((inputReport[9] << 8) | inputReport[8]);
    SHORT rightY = static_cast<SHORT>((inputReport[11] << 8) | inputReport[10]);

    device.current_state.Gamepad.sThumbLX = leftX;
    device.current_state.Gamepad.sThumbLY = leftY;
    device.current_state.Gamepad.sThumbRX = rightX;
    device.current_state.Gamepad.sThumbRY = rightY;
}

void DualSenseHIDWrapper::ParseDualSenseTriggers(DualSenseDevice& device, const BYTE* inputReport) {
    // Triggers: bytes 12-13 (L2), 14-15 (R2)
    // Values are 16-bit unsigned integers (0-65535)

    USHORT leftTrigger = static_cast<USHORT>((inputReport[13] << 8) | inputReport[12]);
    USHORT rightTrigger = static_cast<USHORT>((inputReport[15] << 8) | inputReport[14]);

    // Convert to 8-bit values (0-255) for XInput
    device.current_state.Gamepad.bLeftTrigger = static_cast<BYTE>(leftTrigger >> 8);
    device.current_state.Gamepad.bRightTrigger = static_cast<BYTE>(rightTrigger >> 8);
}

} // namespace display_commander::dualsense
