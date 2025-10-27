#pragma once

#include <reshade_imgui.hpp>

namespace ui::new_ui {

void InitMainNewTab();

void InitDeveloperNewTab();

// Draw the main new tab content
void DrawMainNewTab(reshade::api::effect_runtime* runtime);

// Draw display settings section
void DrawDisplaySettings(reshade::api::effect_runtime* runtime);

// Draw audio settings section
void DrawAudioSettings();

// Draw window controls section
void DrawWindowControls();

// Draw ADHD Multi-Monitor Mode controls section
void DrawAdhdMultiMonitorControls(bool hasBlackCurtainSetting);

// Draw important information section (Flip State)
void DrawImportantInfo();

// Draw frame time graph section
void DrawFrameTimeGraph();

// Draw compact frame time graph for overlay (fixed width)
void DrawFrameTimeGraphOverlay();

}  // namespace ui::new_ui
