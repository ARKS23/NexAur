#pragma once

#include "Core/Base.h"

#include <imgui.h>

#include <string_view>

namespace NexAur {
    struct NEXAUR_API EditorThemeColors {
        ImVec4 background_main;
        ImVec4 background_panel;
        ImVec4 background_panel_light;
        ImVec4 background_input;
        ImVec4 header;
        ImVec4 header_hovered;
        ImVec4 header_active;
        ImVec4 border_subtle;
        ImVec4 text_primary;
        ImVec4 text_secondary;
        ImVec4 accent_blue;
        ImVec4 accent_cyan;
        ImVec4 accent_orange;
        ImVec4 error_red;
        ImVec4 success_green;
    };

    struct NEXAUR_API EditorThemeMetrics {
        ImVec2 window_padding;
        ImVec2 frame_padding;
        ImVec2 cell_padding;
        ImVec2 item_spacing;
        ImVec2 item_inner_spacing;
        float indent_spacing = 0.0f;
        float scrollbar_size = 0.0f;
        float grab_min_size = 0.0f;
        float window_rounding = 0.0f;
        float panel_rounding = 0.0f;
        float frame_rounding = 0.0f;
        float tab_rounding = 0.0f;
        float border_size = 0.0f;
    };

    struct NEXAUR_API EditorTheme {
        EditorThemeColors colors;
        EditorThemeMetrics metrics;
    };

    namespace EditorThemeTokens {
        inline constexpr std::string_view ModernBlack = "ModernBlack";

        NEXAUR_API void setActiveThemeVariant(std::string_view variant);
        NEXAUR_API std::string_view getActiveThemeVariant();
        NEXAUR_API const EditorTheme& getDefaultTheme();
        NEXAUR_API ImVec4 withAlpha(ImVec4 color, float alpha);
        NEXAUR_API ImVec4 fromSrgb(ImVec4 color);
    } // namespace EditorThemeTokens
} // namespace NexAur
