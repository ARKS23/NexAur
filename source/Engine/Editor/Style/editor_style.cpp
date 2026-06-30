#include "pch.h"
#include "editor_style.h"

#include "Editor/Style/editor_icons.h"

#include <ImGuizmo.h>

namespace NexAur::EditorStyle {
    namespace {
        ImVec4 color(float r, float g, float b, float a = 1.0f) {
            return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
        }

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

        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(6.0f, 3.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);
        style.ItemSpacing = ImVec2(6.0f, 4.0f);
        style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
        style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
        style.IndentSpacing = 14.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 8.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;

        style.WindowRounding = 1.0f;
        style.ChildRounding = 2.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.GrabRounding = 2.0f;
        style.TabRounding = 3.0f;

        style.WindowMenuButtonPosition = ImGuiDir_Left;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.SeparatorTextBorderSize = 1.0f;
        style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
        style.SeparatorTextPadding = ImVec2(8.0f, 4.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = color(226.0f, 231.0f, 238.0f);
        colors[ImGuiCol_TextDisabled] = color(122.0f, 133.0f, 146.0f);
        colors[ImGuiCol_WindowBg] = color(37.0f, 39.0f, 43.0f);
        colors[ImGuiCol_ChildBg] = color(33.0f, 35.0f, 39.0f);
        colors[ImGuiCol_PopupBg] = color(34.0f, 37.0f, 42.0f, 0.98f);
        colors[ImGuiCol_Border] = color(77.0f, 84.0f, 93.0f, 0.75f);
        colors[ImGuiCol_BorderShadow] = color(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_FrameBg] = color(49.0f, 56.0f, 66.0f);
        colors[ImGuiCol_FrameBgHovered] = color(61.0f, 72.0f, 86.0f);
        colors[ImGuiCol_FrameBgActive] = color(72.0f, 91.0f, 112.0f);
        colors[ImGuiCol_TitleBg] = color(32.0f, 35.0f, 40.0f);
        colors[ImGuiCol_TitleBgActive] = color(43.0f, 50.0f, 60.0f);
        colors[ImGuiCol_TitleBgCollapsed] = color(27.0f, 29.0f, 33.0f);
        colors[ImGuiCol_MenuBarBg] = color(30.0f, 32.0f, 36.0f);
        colors[ImGuiCol_ScrollbarBg] = color(30.0f, 33.0f, 38.0f);
        colors[ImGuiCol_ScrollbarGrab] = color(84.0f, 93.0f, 104.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = color(103.0f, 115.0f, 130.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = color(120.0f, 136.0f, 154.0f);
        colors[ImGuiCol_CheckMark] = color(109.0f, 172.0f, 235.0f);
        colors[ImGuiCol_SliderGrab] = color(86.0f, 141.0f, 196.0f);
        colors[ImGuiCol_SliderGrabActive] = color(124.0f, 185.0f, 244.0f);
        colors[ImGuiCol_Button] = color(50.0f, 58.0f, 70.0f);
        colors[ImGuiCol_ButtonHovered] = color(65.0f, 80.0f, 98.0f);
        colors[ImGuiCol_ButtonActive] = color(83.0f, 111.0f, 141.0f);
        colors[ImGuiCol_Header] = color(52.0f, 68.0f, 86.0f);
        colors[ImGuiCol_HeaderHovered] = color(65.0f, 86.0f, 109.0f);
        colors[ImGuiCol_HeaderActive] = color(80.0f, 110.0f, 140.0f);
        colors[ImGuiCol_Separator] = color(73.0f, 81.0f, 91.0f);
        colors[ImGuiCol_SeparatorHovered] = color(90.0f, 128.0f, 164.0f);
        colors[ImGuiCol_SeparatorActive] = color(115.0f, 170.0f, 220.0f);
        colors[ImGuiCol_ResizeGrip] = color(77.0f, 96.0f, 116.0f, 0.45f);
        colors[ImGuiCol_ResizeGripHovered] = color(96.0f, 132.0f, 168.0f, 0.75f);
        colors[ImGuiCol_ResizeGripActive] = color(126.0f, 180.0f, 232.0f, 0.90f);
        colors[ImGuiCol_Tab] = color(39.0f, 45.0f, 53.0f);
        colors[ImGuiCol_TabHovered] = color(62.0f, 82.0f, 104.0f);
        colors[ImGuiCol_TabActive] = color(54.0f, 68.0f, 84.0f);
        colors[ImGuiCol_TabUnfocused] = color(34.0f, 38.0f, 44.0f);
        colors[ImGuiCol_TabUnfocusedActive] = color(42.0f, 51.0f, 61.0f);
        colors[ImGuiCol_DockingPreview] = color(98.0f, 151.0f, 205.0f, 0.65f);
        colors[ImGuiCol_DockingEmptyBg] = color(28.0f, 30.0f, 34.0f);
        colors[ImGuiCol_PlotLines] = color(132.0f, 156.0f, 181.0f);
        colors[ImGuiCol_PlotLinesHovered] = color(132.0f, 190.0f, 245.0f);
        colors[ImGuiCol_PlotHistogram] = color(91.0f, 145.0f, 199.0f);
        colors[ImGuiCol_PlotHistogramHovered] = color(125.0f, 183.0f, 235.0f);
        colors[ImGuiCol_TableHeaderBg] = color(44.0f, 52.0f, 62.0f);
        colors[ImGuiCol_TableBorderStrong] = color(78.0f, 88.0f, 99.0f);
        colors[ImGuiCol_TableBorderLight] = color(58.0f, 65.0f, 74.0f);
        colors[ImGuiCol_TableRowBg] = color(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_TableRowBgAlt] = color(255.0f, 255.0f, 255.0f, 0.035f);
        colors[ImGuiCol_TextSelectedBg] = color(83.0f, 126.0f, 170.0f, 0.45f);
        colors[ImGuiCol_DragDropTarget] = color(124.0f, 185.0f, 244.0f, 0.90f);
        colors[ImGuiCol_NavHighlight] = color(109.0f, 172.0f, 235.0f, 0.85f);
        colors[ImGuiCol_NavWindowingHighlight] = color(220.0f, 225.0f, 232.0f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = color(0.0f, 0.0f, 0.0f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = color(0.0f, 0.0f, 0.0f, 0.35f);
    }

    void applyGizmoStyle() {
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
        style.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(0.34f, 0.79f, 0.35f, 1.00f);
        style.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(0.24f, 0.52f, 0.96f, 1.00f);
        style.Colors[ImGuizmo::PLANE_X] = ImVec4(0.93f, 0.22f, 0.24f, 0.36f);
        style.Colors[ImGuizmo::PLANE_Y] = ImVec4(0.34f, 0.79f, 0.35f, 0.36f);
        style.Colors[ImGuizmo::PLANE_Z] = ImVec4(0.24f, 0.52f, 0.96f, 0.36f);
        style.Colors[ImGuizmo::SELECTION] = ImVec4(1.00f, 0.77f, 0.24f, 1.00f);
        style.Colors[ImGuizmo::INACTIVE] = ImVec4(0.55f, 0.60f, 0.67f, 0.56f);
        style.Colors[ImGuizmo::TRANSLATION_LINE] = ImVec4(0.78f, 0.83f, 0.90f, 0.76f);
        style.Colors[ImGuizmo::SCALE_LINE] = ImVec4(0.78f, 0.83f, 0.90f, 0.76f);
        style.Colors[ImGuizmo::ROTATION_USING_BORDER] = ImVec4(1.00f, 0.82f, 0.28f, 1.00f);
        style.Colors[ImGuizmo::ROTATION_USING_FILL] = ImVec4(1.00f, 0.77f, 0.24f, 0.20f);
        style.Colors[ImGuizmo::HATCHED_AXIS_LINES] = ImVec4(0.78f, 0.83f, 0.90f, 0.42f);
        style.Colors[ImGuizmo::TEXT] = ImVec4(0.90f, 0.93f, 0.96f, 1.00f);
        style.Colors[ImGuizmo::TEXT_SHADOW] = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
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
        const bool pressed = ImGui::Button(label, size);
        drawTooltipIfHovered(tooltip);
        return pressed;
    }

    bool segmentedButton(const char* label, bool active, ImVec2 size) {
        int pushed_colors = 0;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            pushed_colors = 3;
        }

        const bool pressed = ImGui::Button(label, size);

        if (pushed_colors > 0) {
            ImGui::PopStyleColor(pushed_colors);
        }

        return !active && pressed;
    }
} // namespace NexAur::EditorStyle
