#include "pch.h"
#include "editor_style.h"

#include "Editor/Style/editor_icons.h"
#include "Editor/Style/editor_theme.h"
#include "Editor/Widgets/editor_widgets.h"

#include <ImGuizmo.h>

namespace NexAur::EditorStyle {
    namespace {
        void drawTooltipIfHovered(const char* text) {
            if (!text || text[0] == '\0') {
                return;
            }

            if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                return;
            }

            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 36.0f);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    } // namespace

    void applyTheme() {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();
        const EditorThemeColors& palette = theme.colors;
        const EditorThemeMetrics& metrics = theme.metrics;

        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowPadding = metrics.window_padding;
        style.FramePadding = metrics.frame_padding;
        style.CellPadding = metrics.cell_padding;
        style.ItemSpacing = metrics.item_spacing;
        style.ItemInnerSpacing = metrics.item_inner_spacing;
        style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
        style.IndentSpacing = metrics.indent_spacing;
        style.ScrollbarSize = metrics.scrollbar_size;
        style.GrabMinSize = metrics.grab_min_size;
        style.DisabledAlpha = 0.46f;

        style.WindowBorderSize = metrics.border_size;
        style.ChildBorderSize = metrics.border_size;
        style.PopupBorderSize = metrics.border_size;
        style.FrameBorderSize = metrics.border_size;
        style.TabBorderSize = 0.0f;

        style.WindowRounding = metrics.window_rounding;
        style.ChildRounding = metrics.panel_rounding;
        style.FrameRounding = metrics.frame_rounding;
        style.PopupRounding = metrics.panel_rounding;
        style.ScrollbarRounding = metrics.frame_rounding;
        style.GrabRounding = metrics.frame_rounding;
        style.TabRounding = metrics.tab_rounding;

        style.WindowMenuButtonPosition = ImGuiDir_Left;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.SeparatorTextBorderSize = metrics.border_size;
        style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
        style.SeparatorTextPadding = ImVec2(8.0f, 4.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = palette.text_primary;
        colors[ImGuiCol_TextDisabled] = palette.text_secondary;
        colors[ImGuiCol_WindowBg] = palette.background_main;
        colors[ImGuiCol_ChildBg] = palette.background_panel;
        colors[ImGuiCol_PopupBg] = EditorThemeTokens::withAlpha(palette.background_panel_light, 0.98f);
        colors[ImGuiCol_Border] = EditorThemeTokens::withAlpha(palette.border_subtle, 0.82f);
        colors[ImGuiCol_BorderShadow] = EditorThemeTokens::withAlpha(palette.background_main, 0.0f);
        colors[ImGuiCol_FrameBg] = palette.background_panel_light;
        colors[ImGuiCol_FrameBgHovered] = EditorThemeTokens::withAlpha(palette.header_hovered, 0.92f);
        colors[ImGuiCol_FrameBgActive] = palette.header_active;
        colors[ImGuiCol_TitleBg] = palette.background_main;
        colors[ImGuiCol_TitleBgActive] = palette.background_panel;
        colors[ImGuiCol_TitleBgCollapsed] = palette.background_main;
        colors[ImGuiCol_MenuBarBg] = palette.background_main;
        colors[ImGuiCol_ScrollbarBg] = palette.background_panel;
        colors[ImGuiCol_ScrollbarGrab] = palette.border_subtle;
        colors[ImGuiCol_ScrollbarGrabHovered] = palette.header_hovered;
        colors[ImGuiCol_ScrollbarGrabActive] = palette.header_active;
        colors[ImGuiCol_CheckMark] = palette.accent_cyan;
        colors[ImGuiCol_SliderGrab] = palette.accent_blue;
        colors[ImGuiCol_SliderGrabActive] = palette.accent_cyan;
        colors[ImGuiCol_Button] = palette.header;
        colors[ImGuiCol_ButtonHovered] = palette.header_hovered;
        colors[ImGuiCol_ButtonActive] = palette.header_active;
        colors[ImGuiCol_Header] = palette.header;
        colors[ImGuiCol_HeaderHovered] = palette.header_hovered;
        colors[ImGuiCol_HeaderActive] = palette.header_active;
        colors[ImGuiCol_Separator] = palette.border_subtle;
        colors[ImGuiCol_SeparatorHovered] = EditorThemeTokens::withAlpha(palette.accent_blue, 0.72f);
        colors[ImGuiCol_SeparatorActive] = palette.accent_cyan;
        colors[ImGuiCol_ResizeGrip] = EditorThemeTokens::withAlpha(palette.accent_blue, 0.24f);
        colors[ImGuiCol_ResizeGripHovered] = EditorThemeTokens::withAlpha(palette.accent_blue, 0.52f);
        colors[ImGuiCol_ResizeGripActive] = EditorThemeTokens::withAlpha(palette.accent_cyan, 0.76f);
        colors[ImGuiCol_Tab] = palette.background_panel;
        colors[ImGuiCol_TabHovered] = palette.header_hovered;
        colors[ImGuiCol_TabActive] = palette.header_active;
        colors[ImGuiCol_TabUnfocused] = palette.background_main;
        colors[ImGuiCol_TabUnfocusedActive] = palette.header;
        colors[ImGuiCol_DockingPreview] = EditorThemeTokens::withAlpha(palette.accent_blue, 0.55f);
        colors[ImGuiCol_DockingEmptyBg] = palette.background_main;
        colors[ImGuiCol_PlotLines] = palette.text_secondary;
        colors[ImGuiCol_PlotLinesHovered] = palette.accent_cyan;
        colors[ImGuiCol_PlotHistogram] = palette.accent_blue;
        colors[ImGuiCol_PlotHistogramHovered] = palette.accent_cyan;
        colors[ImGuiCol_TableHeaderBg] = palette.header;
        colors[ImGuiCol_TableBorderStrong] = palette.border_subtle;
        colors[ImGuiCol_TableBorderLight] = EditorThemeTokens::withAlpha(palette.border_subtle, 0.62f);
        colors[ImGuiCol_TableRowBg] = EditorThemeTokens::withAlpha(palette.background_main, 0.0f);
        colors[ImGuiCol_TableRowBgAlt] = EditorThemeTokens::withAlpha(palette.text_primary, 0.032f);
        colors[ImGuiCol_TextSelectedBg] = EditorThemeTokens::withAlpha(palette.accent_blue, 0.46f);
        colors[ImGuiCol_DragDropTarget] = EditorThemeTokens::withAlpha(palette.accent_cyan, 0.86f);
        colors[ImGuiCol_NavHighlight] = EditorThemeTokens::withAlpha(palette.accent_blue, 0.78f);
        colors[ImGuiCol_NavWindowingHighlight] = EditorThemeTokens::withAlpha(palette.text_primary, 0.64f);
        colors[ImGuiCol_NavWindowingDimBg] = EditorThemeTokens::withAlpha(palette.background_main, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = EditorThemeTokens::withAlpha(palette.background_main, 0.42f);
    }

    void applyGizmoStyle() {
        const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();
        const EditorThemeColors& palette = theme.colors;
        ImGuizmo::Style& style = ImGuizmo::GetStyle();

        style.TranslationLineThickness = 3.0f;
        style.TranslationLineArrowSize = 6.0f;
        style.RotationLineThickness = 3.0f;
        style.RotationOuterLineThickness = 3.0f;
        style.ScaleLineThickness = 3.0f;
        style.ScaleLineCircleSize = 6.0f;
        style.HatchedAxisLineThickness = 2.0f;
        style.CenterCircleSize = 5.0f;

        style.Colors[ImGuizmo::DIRECTION_X] = ImVec4(0.93f, 0.22f, 0.24f, 1.00f);
        style.Colors[ImGuizmo::DIRECTION_Y] = palette.success_green;
        style.Colors[ImGuizmo::DIRECTION_Z] = palette.accent_blue;
        style.Colors[ImGuizmo::PLANE_X] = ImVec4(0.93f, 0.22f, 0.24f, 0.34f);
        style.Colors[ImGuizmo::PLANE_Y] = EditorThemeTokens::withAlpha(palette.success_green, 0.34f);
        style.Colors[ImGuizmo::PLANE_Z] = EditorThemeTokens::withAlpha(palette.accent_blue, 0.34f);
        style.Colors[ImGuizmo::SELECTION] = palette.accent_orange;
        style.Colors[ImGuizmo::INACTIVE] = EditorThemeTokens::withAlpha(palette.text_secondary, 0.52f);
        style.Colors[ImGuizmo::TRANSLATION_LINE] = EditorThemeTokens::withAlpha(palette.text_primary, 0.76f);
        style.Colors[ImGuizmo::SCALE_LINE] = EditorThemeTokens::withAlpha(palette.text_primary, 0.76f);
        style.Colors[ImGuizmo::ROTATION_USING_BORDER] = palette.accent_orange;
        style.Colors[ImGuizmo::ROTATION_USING_FILL] = EditorThemeTokens::withAlpha(palette.accent_orange, 0.20f);
        style.Colors[ImGuizmo::HATCHED_AXIS_LINES] = EditorThemeTokens::withAlpha(palette.text_primary, 0.40f);
        style.Colors[ImGuizmo::TEXT] = palette.text_primary;
        style.Colors[ImGuizmo::TEXT_SHADOW] = EditorThemeTokens::withAlpha(palette.background_main, 0.72f);
    }

    bool drawSectionHeader(const char* label, ImGuiTreeNodeFlags flags) {
        return ImGui::CollapsingHeader(label, flags);
    }

    void drawSubtleText(const char* text) {
        if (!text) {
            return;
        }

        ImGui::TextDisabled("%s", text);
    }

    void drawHelpTooltip(const char* text) {
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextDisabled("%s", EditorIcons::Help);
        drawTooltipIfHovered(text);
    }

    bool iconButton(const char* label, const char* tooltip, ImVec2 size) {
        return EditorWidgets::toolbarButton(label, tooltip, false, size);
    }

    bool segmentedButton(const char* label, bool active, ImVec2 size) {
        const bool pressed = EditorWidgets::toolbarButton(label, nullptr, active, size);
        return !active && pressed;
    }
} // namespace NexAur::EditorStyle
