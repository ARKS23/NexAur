#pragma once

#include "Core/Base.h"

#include <imgui.h>

#include <cstddef>
#include <functional>

namespace NexAur::EditorWidgets {
    NEXAUR_API bool toolbarButton(
        const char* label,
        const char* tooltip = nullptr,
        bool selected = false,
        ImVec2 size = ImVec2(0.0f, 0.0f));

    NEXAUR_API bool searchBox(
        const char* id,
        char* buffer,
        size_t buffer_size,
        const char* hint = "Search",
        float width = 220.0f);

    NEXAUR_API void panelHeader(const char* title);

    NEXAUR_API bool sectionHeader(
        const char* title,
        bool default_open = true);

    NEXAUR_API void propertyRow(
        const char* label,
        const std::function<void()>& draw_value);

    NEXAUR_API void statusPill(
        const char* label,
        ImVec4 color);

    NEXAUR_API void consoleLine(
        const char* level,
        ImVec4 level_color,
        const char* logger,
        const char* message);

    NEXAUR_API bool tableSelectableRow(
        const char* label,
        bool selected = false,
        ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns);
} // namespace NexAur::EditorWidgets
