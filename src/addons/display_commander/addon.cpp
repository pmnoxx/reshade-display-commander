#include "addon.hpp"

// Export addon information
extern "C" __declspec(dllexport) constexpr const char* NAME = "Display Commander";
extern "C" __declspec(dllexport) constexpr const char* DESCRIPTION =
    "RenoDX Display Commander - Advanced display and performance management.";
