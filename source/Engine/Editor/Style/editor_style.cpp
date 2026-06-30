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

        style.WindowPadding = ImVec2(7.0f, 6.0f);
        style.FramePadding = ImVec2(6.0f, 2.0f);
        style.CellPadding = ImVec2(5.0f, 3.0f);
        style.ItemSpacing = ImVec2(5.0f, 3.0f);
        style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
        style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
        style.IndentSpacing = 14.0f;
        style.ScrollbarSize = 11.0f;
        style.GrabMinSize = 7.0f;
        style.DisabledAlpha = 0.46f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;

        style.WindowRounding = 0.0f;
        style.ChildRounding = 2.0f;
        style.FrameRounding = 2.0f;
        style.PopupRounding = 2.0f;
        style.ScrollbarRounding = 2.0f;
        style.GrabRounding = 2.0f;
        style.TabRounding = 2.0f;

        style.WindowMenuButtonPosition = ImGuiDir_Left;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.SeparatorTextBorderSize = 1.0f;
        style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
        style.SeparatorTextPadding = ImVec2(8.0f, 4.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = color(222.0f, 226.0f, 232.0f);
        colors[ImGuiCol_TextDisabled] = color(102.0f, 111.0f, 122.0f);
        colors[ImGuiCol_WindowBg] = color(27.0f, 29.0f, 32.0f);
        colors[ImGuiCol_ChildBg] = color(23.0f, 25.0f, 28.0f);
        colors[ImGuiCol_PopupBg] = color(24.0f, 27.0f, 31.0f, 0.98f);
        colors[ImGuiCol_Border] = color(50.0f, 56.0f, 64.0f, 0.82f);
        colors[ImGuiCol_BorderShadow] = color(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_FrameBg] = color(32.0f, 37.0f, 44.0f);
        colors[ImGuiCol_FrameBgHovered] = color(42.0f, 50.0f, 60.0f);
        colors[ImGuiCol_FrameBgActive] = color(50.0f, 62.0f, 76.0f);
        colors[ImGuiCol_TitleBg] = color(20.0f, 22.0f, 25.0f);
        colors[ImGuiCol_TitleBgActive] = color(28.0f, 33.0f, 39.0f);
        colors[ImGuiCol_TitleBgCollapsed] = color(19.0f, 21.0f, 24.0f);
        colors[ImGuiCol_MenuBarBg] = color(22.0f, 24.0f, 27.0f);
        colors[ImGuiCol_ScrollbarBg] = color(21.0f, 24.0f, 28.0f);
        colors[ImGuiCol_ScrollbarGrab] = color(58.0f, 66.0f, 76.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = color(75.0f, 86.0f, 99.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = color(92.0f, 108.0f, 126.0f);
        colors[ImGuiCol_CheckMark] = color(96.0f, 163.0f, 226.0f);
        colors[ImGuiCol_SliderGrab] = color(73.0f, 122.0f, 170.0f);
        colors[ImGuiCol_SliderGrabActive] = color(101.0f, 157.0f, 213.0f);
        colors[ImGuiCol_Button] = color(34.0f, 40.0f, 48.0f);
        colors[ImGuiCol_ButtonHovered] = color(44.0f, 54.0f, 66.0f);
        colors[ImGuiCol_ButtonActive] = color(54.0f, 68.0f, 84.0f);
        colors[ImGuiCol_Header] = color(35.0f, 43.0f, 52.0f);
        colors[ImGuiCol_HeaderHovered] = color(44.0f, 55.0f, 68.0f);
        colors[ImGuiCol_HeaderActive] = color(52.0f, 68.0f, 86.0f);
        colors[ImGuiCol_Separator] = color(43.0f, 49.0f, 57.0f);
        colors[ImGuiCol_SeparatorHovered] = color(69.0f, 94.0f, 120.0f);
        colors[ImGuiCol_SeparatorActive] = color(91.0f, 129.0f, 169.0f);
        colors[ImGuiCol_ResizeGrip] = color(60.0f, 75.0f, 92.0f, 0.35f);
        colors[ImGuiCol_ResizeGripHovered] = color(78.0f, 102.0f, 130.0f, 0.65f);
        colors[ImGuiCol_ResizeGripActive] = color(96.0f, 138.0f, 184.0f, 0.85f);
        colors[ImGuiCol_Tab] = color(25.0f, 30.0f, 36.0f);
        colors[ImGuiCol_TabHovered] = color(42.0f, 53.0f, 66.0f);
        colors[ImGuiCol_TabActive] = color(35.0f, 43.0f, 52.0f);
        colors[ImGuiCol_TabUnfocused] = color(22.0f, 26.0f, 31.0f);
        colors[ImGuiCol_TabUnfocusedActive] = color(29.0f, 35.0f, 42.0f);
        colors[ImGuiCol_DockingPreview] = color(83.0f, 129.0f, 178.0f, 0.55f);
        colors[ImGuiCol_DockingEmptyBg] = color(19.0f, 21.0f, 24.0f);
        colors[ImGuiCol_PlotLines] = color(116.0f, 134.0f, 155.0f);
        colors[ImGuiCol_PlotLinesHovered] = color(103.0f, 162.0f, 220.0f);
        colors[ImGuiCol_PlotHistogram] = color(74.0f, 121.0f, 168.0f);
        colors[ImGuiCol_PlotHistogramHovered] = color(100.0f, 159.0f, 216.0f);
        colors[ImGuiCol_TableHeaderBg] = color(31.0f, 38.0f, 47.0f);
        colors[ImGuiCol_TableBorderStrong] = color(54.0f, 63.0f, 74.0f);
        colors[ImGuiCol_TableBorderLight] = color(39.0f, 45.0f, 53.0f);
        colors[ImGuiCol_TableRowBg] = color(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_TableRowBgAlt] = color(255.0f, 255.0f, 255.0f, 0.028f);
        colors[ImGuiCol_TextSelectedBg] = color(62.0f, 97.0f, 135.0f, 0.52f);
        colors[ImGuiCol_DragDropTarget] = color(99.0f, 157.0f, 216.0f, 0.86f);
        colors[ImGuiCol_NavHighlight] = color(84.0f, 139.0f, 195.0f, 0.78f);
        colors[ImGuiCol_NavWindowingHighlight] = color(204.0f, 212.0f, 224.0f, 0.64f);
        colors[ImGuiCol_NavWindowingDimBg] = color(0.0f, 0.0f, 0.0f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = color(0.0f, 0.0f, 0.0f, 0.42f);
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
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
            ImGui::PushStyleColor(ImGuiCol_Text, color(238.0f, 242.0f, 248.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, color(73.0f, 105.0f, 138.0f, 0.9f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            pushed_colors = 5;
        }

        const bool pressed = ImGui::Button(label, size);

        if (active) {
            ImGui::PopStyleVar();
        }

        if (pushed_colors > 0) {
            ImGui::PopStyleColor(pushed_colors);
        }

        return !active && pressed;
    }
} // namespace NexAur::EditorStyle
