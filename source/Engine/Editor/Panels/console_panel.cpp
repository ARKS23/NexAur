#include "pch.h"
#include "console_panel.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace NexAur {
    namespace {
        std::string toLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        ImVec4 levelColor(spdlog::level::level_enum level) {
            switch (level) {
            case spdlog::level::warn:
                return ImVec4(0.96f, 0.72f, 0.30f, 1.0f);
            case spdlog::level::err:
            case spdlog::level::critical:
                return ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
            case spdlog::level::debug:
            case spdlog::level::trace:
                return ImVec4(0.60f, 0.68f, 0.76f, 1.0f);
            case spdlog::level::info:
            default:
                return ImVec4(0.74f, 0.82f, 0.90f, 1.0f);
            }
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

        drawToolbar();
        drawMessages();

        ImGui::End();
    }

    void ConsolePanel::drawToolbar() {
        if (ImGui::Button("Clear")) {
            LogSystem::clearRecentMessages();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##ConsoleFilter", "Filter", m_filter.data(), m_filter.size());

        ImGui::SameLine();
        ImGui::Checkbox("Auto Scroll", &m_auto_scroll);

        ImGui::Separator();
    }

    void ConsolePanel::drawMessages() {
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_AlwaysVerticalScrollbar |
            ImGuiWindowFlags_HorizontalScrollbar;

        if (!ImGui::BeginChild("ConsoleMessages", ImVec2(0.0f, 0.0f), false, flags)) {
            ImGui::EndChild();
            return;
        }

        const std::vector<LogMessage> messages = LogSystem::getRecentMessages();
        if (messages.empty()) {
            ImGui::TextDisabled("No log messages.");
            ImGui::EndChild();
            return;
        }

        const bool should_scroll =
            m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;

        int visible_count = 0;
        for (const LogMessage& message : messages) {
            if (!passesFilter(message)) {
                continue;
            }

            drawMessage(message);
            ++visible_count;
        }

        if (visible_count == 0) {
            ImGui::TextDisabled("No messages match the current filter.");
        } else if (should_scroll) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
    }

    bool ConsolePanel::passesFilter(const LogMessage& message) const {
        if (m_filter[0] == '\0') {
            return true;
        }

        const std::string needle = toLower(m_filter.data());
        const std::string level = toLower(LogSystem::levelToString(message.level));
        const std::string logger = toLower(message.logger_name);
        const std::string text = toLower(message.message);

        return level.find(needle) != std::string::npos ||
            logger.find(needle) != std::string::npos ||
            text.find(needle) != std::string::npos;
    }

    void ConsolePanel::drawMessage(const LogMessage& message) const {
        const char* level = LogSystem::levelToString(message.level);
        ImGui::PushStyleColor(ImGuiCol_Text, levelColor(message.level));
        ImGui::Text("[%s]", level);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextDisabled("%s", message.logger_name.c_str());

        ImGui::SameLine();
        ImGui::TextUnformatted(message.message.c_str());
    }
} // namespace NexAur
