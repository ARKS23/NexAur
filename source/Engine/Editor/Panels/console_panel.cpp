#include "pch.h"
#include "console_panel.h"

#include "Editor/Style/editor_theme.h"
#include "Editor/Widgets/editor_widgets.h"

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
            const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();
            switch (level) {
            case spdlog::level::warn:
                return theme.colors.accent_orange;
            case spdlog::level::err:
            case spdlog::level::critical:
                return theme.colors.error_red;
            case spdlog::level::debug:
            case spdlog::level::trace:
                return theme.colors.text_secondary;
            case spdlog::level::info:
            default:
                return theme.colors.text_primary;
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
        if (EditorWidgets::toolbarButton("Clear", "Clear recent editor log messages")) {
            LogSystem::clearRecentMessages();
        }

        ImGui::SameLine();
        EditorWidgets::searchBox("##ConsoleFilter", m_filter.data(), m_filter.size(), "Filter");

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
        EditorWidgets::consoleLine(
            level,
            levelColor(message.level),
            message.logger_name.c_str(),
            message.message.c_str());
    }
} // namespace NexAur
