#pragma once

#include "Core/Base.h"

#include <imgui.h>

namespace NexAur::EditorStyle {
    NEXAUR_API void applyTheme();
    NEXAUR_API void applyGizmoStyle();

    NEXAUR_API bool drawSectionHeader(
        const char* label,
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen);
    NEXAUR_API void drawSubtleText(const char* text);
    NEXAUR_API void drawHelpTooltip(const char* text);

    NEXAUR_API bool iconButton(
        const char* label,
        const char* tooltip = nullptr,
        ImVec2 size = ImVec2(0.0f, 0.0f));
    NEXAUR_API bool segmentedButton(
        const char* label,
        bool active,
        ImVec2 size = ImVec2(0.0f, 0.0f));
} // namespace NexAur::EditorStyle
