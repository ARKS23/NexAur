#include "pch.h"
#include "console_panel.h"

#include <imgui.h>

#include <array>
#include <string>

namespace NexAur {
    namespace {
        bool passesFilter(const char* level, const char* text, const std::array<char, 128>& filter) {
            if (filter[0] == '\0') {
                return true;
            }

            const std::string needle(filter.data());
            const std::string level_text(level ? level : "");
            const std::string message_text(text ? text : "");
            return level_text.find(needle) != std::string::npos ||
                message_text.find(needle) != std::string::npos;
        }
    } // namespace

    ConsolePanel::ConsolePanel(const std::string& name)
        : EditorPanel(name) {}

    void ConsolePanel::onUIRender() {
        bool& open_flag = getOpenFlag();
        if (!ImGui::Begin(getName().c_str(), &open_flag)) {
            ImGui::End();
            return;
        }

        ImGui::BeginDisabled();
        ImGui::Button("Clear");
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##ConsoleFilter", "Filter", m_filter.data(), m_filter.size());

        ImGui::Separator();

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_AlwaysVerticalScrollbar |
            ImGuiWindowFlags_HorizontalScrollbar;
        if (ImGui::BeginChild("ConsoleMessages", ImVec2(0.0f, 0.0f), false, flags)) {
            drawMessage("Info", "NexAur Editor ready.");
            drawMessage("Info", "Console sink is reserved for a later logging pass.");
            drawMessage("Trace", "Use Window -> Console to restore this panel.");
        }
        ImGui::EndChild();

        ImGui::End();
    }

    void ConsolePanel::drawMessage(const char* level, const char* text) const {
        if (!passesFilter(level, text, m_filter)) {
            return;
        }

        ImGui::TextDisabled("[%s]", level);
        ImGui::SameLine();
        ImGui::TextUnformatted(text);
    }
} // namespace NexAur
