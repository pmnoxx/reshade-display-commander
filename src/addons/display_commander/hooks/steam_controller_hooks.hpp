#pragma once

#include <windows.h>
#include <atomic>
#include <cstdint>

// Forward declaration for Steam controller state structure
struct SteamControllerState001_t;

// Steam API calling convention
#ifndef S_CALLTYPE
#define S_CALLTYPE __stdcall
#endif

namespace renodx::hooks {

// Steam Controller API function pointer types
using SteamAPI_ISteamController_GetControllerState_pfn = bool (S_CALLTYPE *)(
    intptr_t instancePtr,
    uint32_t unControllerIndex,
    struct SteamControllerState001_t * pState
);

using SteamAPI_ISteamController_Init_pfn = bool (S_CALLTYPE *)(
    intptr_t instancePtr,
    const char * pchAbsolutePathToControllerConfigVDF
);

using SteamAPI_ISteamController_RunFrame_pfn = void (S_CALLTYPE *)(
    intptr_t instancePtr
);

// Global state for Steam controller suppression
extern std::atomic<bool> g_steam_controller_suppression_enabled;

// Original function pointers
extern SteamAPI_ISteamController_GetControllerState_pfn SteamAPI_ISteamController_GetControllerState_Original;
extern SteamAPI_ISteamController_Init_pfn SteamAPI_ISteamController_Init_Original;
extern SteamAPI_ISteamController_RunFrame_pfn SteamAPI_ISteamController_RunFrame_Original;

// Hook functions
bool S_CALLTYPE SteamAPI_ISteamController_GetControllerState_Detour(
    intptr_t instancePtr,
    uint32_t unControllerIndex,
    struct SteamControllerState001_t * pState
);

bool S_CALLTYPE SteamAPI_ISteamController_Init_Detour(
    intptr_t instancePtr,
    const char * pchAbsolutePathToControllerConfigVDF
);

void S_CALLTYPE SteamAPI_ISteamController_RunFrame_Detour(
    intptr_t instancePtr
);

// Hook management functions
bool InstallSteamControllerHooks();
void UninstallSteamControllerHooks();
void SetSteamControllerSuppression(bool enabled);

} // namespace renodx::hooks
