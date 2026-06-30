#include "pch.h"
#include "editor_widgets.h"

#include "Editor/Style/editor_theme.h"

namespace NexAur::EditorWidgets {
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

    bool toolbarButton(const char* label, const char* tooltip, bool selected, ImVec2 size) {
        const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();

        int pushed_colors = 0;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, EditorThemeTokens::withAlpha(theme.colors.accent_blue, 0.42f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorThemeTokens::withAlpha(theme.colors.accent_blue, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorThemeTokens::withAlpha(theme.colors.accent_cyan, 0.58f));
            ImGui::PushStyleColor(ImGuiCol_Text, theme.colors.text_primary);
            pushed_colors = 4;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 3.0f));
        const bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleVar();

        if (pushed_colors > 0) {
            ImGui::PopStyleColor(pushed_colors);
        }

        drawTooltipIfHovered(tooltip);
        return pressed;
    }

    bool searchBox(const char* id, char* buffer, size_t buffer_size, const char* hint, float width) {
        ImGui::SetNextItemWidth(width);
        return ImGui::InputTextWithHint(id, hint, buffer, buffer_size);
    }

    void panelHeader(const char* title) {
        const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();

        ImGui::PushStyleColor(ImGuiCol_Text, theme.colors.text_primary);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    bool sectionHeader(const char* title, bool default_open) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (default_open) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        return ImGui::CollapsingHeader(title, flags);
    }

    void propertyRow(const char* label, const std::function<void()>& draw_value) {
        constexpr float kLabelWidth = 118.0f;

        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(kLabelWidth);
        if (draw_value) {
            draw_value();
        }
        ImGui::PopID();
    }

    void statusPill(const char* label, ImVec4 color) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
    }

    void consoleLine(const char* level, ImVec4 level_color, const char* logger, const char* message) {
        ImGui::PushStyleColor(ImGuiCol_Text, level_color);
        ImGui::Text("[%s]", level ? level : "");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextDisabled("%s", logger ? logger : "");

        ImGui::SameLine();
        ImGui::TextUnformatted(message ? message : "");
    }

    bool tableSelectableRow(const char* label, bool selected, ImGuiSelectableFlags flags) {
        return ImGui::Selectable(label, selected, flags);
    }
} // namespace NexAur::EditorWidgets
