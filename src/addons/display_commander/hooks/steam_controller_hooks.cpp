#include "steam_controller_hooks.hpp"
#include "../utils.hpp"

// Include MinHook
#include <MinHook.h>

namespace renodx::hooks {

// Global state for Steam controller suppression
std::atomic<bool> g_steam_controller_suppression_enabled{false};

// Original function pointers
SteamAPI_ISteamController_GetControllerState_pfn SteamAPI_ISteamController_GetControllerState_Original = nullptr;
SteamAPI_ISteamController_Init_pfn SteamAPI_ISteamController_Init_Original = nullptr;
SteamAPI_ISteamController_RunFrame_pfn SteamAPI_ISteamController_RunFrame_Original = nullptr;

// Hook functions
bool S_CALLTYPE SteamAPI_ISteamController_GetControllerState_Detour(
    intptr_t instancePtr,
    uint32_t unControllerIndex,
    struct SteamControllerState001_t * pState
) {
    // Check if suppression is enabled
    if (g_steam_controller_suppression_enabled.load()) {
        // Return false to indicate no controller state available
        LogInfo("Steam Controller state suppressed for controller %u", unControllerIndex);
        return false;
    }

    // Call original function if suppression is disabled
    if (SteamAPI_ISteamController_GetControllerState_Original) {
        return SteamAPI_ISteamController_GetControllerState_Original(instancePtr, unControllerIndex, pState);
    }

    return false;
}

bool S_CALLTYPE SteamAPI_ISteamController_Init_Detour(
    intptr_t instancePtr,
    const char * pchAbsolutePathToControllerConfigVDF
) {
    LogInfo("Steam Controller Init called - suppression enabled: %s",
            g_steam_controller_suppression_enabled.load() ? "true" : "false");

    // Always call original init function
    if (SteamAPI_ISteamController_Init_Original) {
        return SteamAPI_ISteamController_Init_Original(instancePtr, pchAbsolutePathToControllerConfigVDF);
    }

    return false;
}

void S_CALLTYPE SteamAPI_ISteamController_RunFrame_Detour(
    intptr_t instancePtr
) {
    // Check if suppression is enabled
    if (g_steam_controller_suppression_enabled.load()) {
        // Skip calling original function to suppress controller input processing
        LogInfo("Steam Controller RunFrame suppressed");
        return;
    }

    // Call original function if suppression is disabled
    if (SteamAPI_ISteamController_RunFrame_Original) {
        SteamAPI_ISteamController_RunFrame_Original(instancePtr);
    }
}

// Hook management functions
bool InstallSteamControllerHooks() {
    LogInfo("Installing Steam Controller hooks...");

    // Get Steam client DLL handle
    HMODULE hSteamClient = GetModuleHandleA("steamclient.dll");
    if (!hSteamClient) {
        hSteamClient = GetModuleHandleA("steamclient64.dll");
    }

    if (!hSteamClient) {
        LogWarn("Steam client DLL not found - Steam Controller hooks not installed");
        return false;
    }

    // Get function addresses
    auto GetControllerState = reinterpret_cast<SteamAPI_ISteamController_GetControllerState_pfn>(
        GetProcAddress(hSteamClient, "SteamAPI_ISteamController_GetControllerState"));
    auto Init = reinterpret_cast<SteamAPI_ISteamController_Init_pfn>(
        GetProcAddress(hSteamClient, "SteamAPI_ISteamController_Init"));
    auto RunFrame = reinterpret_cast<SteamAPI_ISteamController_RunFrame_pfn>(
        GetProcAddress(hSteamClient, "SteamAPI_ISteamController_RunFrame"));

    if (!GetControllerState || !Init || !RunFrame) {
        LogWarn("Steam Controller API functions not found - hooks not installed");
        return false;
    }

    // Store original function pointers
    SteamAPI_ISteamController_GetControllerState_Original = GetControllerState;
    SteamAPI_ISteamController_Init_Original = Init;
    SteamAPI_ISteamController_RunFrame_Original = RunFrame;

    // Install hooks using MinHook
    MH_STATUS status;

    // Hook GetControllerState
    status = MH_CreateHook(GetControllerState, SteamAPI_ISteamController_GetControllerState_Detour,
                          (LPVOID*)&SteamAPI_ISteamController_GetControllerState_Original);
    if (status != MH_OK) {
        LogError("Failed to create Steam Controller GetControllerState hook: %d", status);
        return false;
    }

    // Hook Init
    status = MH_CreateHook(Init, SteamAPI_ISteamController_Init_Detour,
                          (LPVOID*)&SteamAPI_ISteamController_Init_Original);
    if (status != MH_OK) {
        LogError("Failed to create Steam Controller Init hook: %d", status);
        return false;
    }

    // Hook RunFrame
    status = MH_CreateHook(RunFrame, SteamAPI_ISteamController_RunFrame_Detour,
                          (LPVOID*)&SteamAPI_ISteamController_RunFrame_Original);
    if (status != MH_OK) {
        LogError("Failed to create Steam Controller RunFrame hook: %d", status);
        return false;
    }

    // Enable hooks
    status = MH_EnableHook(GetControllerState);
    if (status != MH_OK) {
        LogError("Failed to enable Steam Controller GetControllerState hook: %d", status);
        return false;
    }

    status = MH_EnableHook(Init);
    if (status != MH_OK) {
        LogError("Failed to enable Steam Controller Init hook: %d", status);
        return false;
    }

    status = MH_EnableHook(RunFrame);
    if (status != MH_OK) {
        LogError("Failed to enable Steam Controller RunFrame hook: %d", status);
        return false;
    }

    LogInfo("Steam Controller hooks installed successfully");
    return true;
}

void UninstallSteamControllerHooks() {
    LogInfo("Uninstalling Steam Controller hooks...");

    // Disable hooks
    if (SteamAPI_ISteamController_GetControllerState_Original) {
        MH_DisableHook(SteamAPI_ISteamController_GetControllerState_Original);
    }
    if (SteamAPI_ISteamController_Init_Original) {
        MH_DisableHook(SteamAPI_ISteamController_Init_Original);
    }
    if (SteamAPI_ISteamController_RunFrame_Original) {
        MH_DisableHook(SteamAPI_ISteamController_RunFrame_Original);
    }

    // Clear function pointers
    SteamAPI_ISteamController_GetControllerState_Original = nullptr;
    SteamAPI_ISteamController_Init_Original = nullptr;
    SteamAPI_ISteamController_RunFrame_Original = nullptr;

    LogInfo("Steam Controller hooks uninstalled");
}

void SetSteamControllerSuppression(bool enabled) {
    g_steam_controller_suppression_enabled.store(enabled);
    LogInfo("Steam Controller suppression %s", enabled ? "enabled" : "disabled");
}

} // namespace renodx::hooks
