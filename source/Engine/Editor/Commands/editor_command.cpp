#include "pch.h"
#include "editor_command.h"

#include <utility>

namespace NexAur {
    bool EditorCommandRegistry::registerCommand(EditorCommand command) {
        if (command.id.empty()) {
            return false;
        }

        auto found = m_command_indices.find(command.id);
        if (found != m_command_indices.end()) {
            m_commands[found->second] = std::move(command);
            return true;
        }

        const size_t index = m_commands.size();
        m_command_indices.emplace(command.id, index);
        m_ordered_ids.push_back(command.id);
        m_commands.push_back(std::move(command));
        return true;
    }

    bool EditorCommandRegistry::execute(const std::string& id) const {
        const EditorCommand* command = find(id);
        if (!command || !isEnabled(id) || !command->execute) {
            return false;
        }

        command->execute();
        return true;
    }

    bool EditorCommandRegistry::processShortcuts() const {
        if (!ImGui::GetCurrentContext() || ImGui::GetIO().WantTextInput) {
            return false;
        }

        for (const std::string& id : m_ordered_ids) {
            const EditorCommand* command = find(id);
            if (!command || command->shortcut == ImGuiKey_None) {
                continue;
            }

            if (ImGui::IsKeyChordPressed(command->shortcut) && execute(id)) {
                return true;
            }
        }

        return false;
    }

    bool EditorCommandRegistry::isEnabled(const std::string& id) const {
        const EditorCommand* command = find(id);
        if (!command) {
            return false;
        }

        return command->enabled ? command->enabled() : true;
    }

    bool EditorCommandRegistry::isSelected(const std::string& id) const {
        const EditorCommand* command = find(id);
        if (!command || !command->selected) {
            return false;
        }

        return command->selected();
    }

    const EditorCommand* EditorCommandRegistry::find(const std::string& id) const {
        const auto found = m_command_indices.find(id);
        if (found == m_command_indices.end() || found->second >= m_commands.size()) {
            return nullptr;
        }

        return &m_commands[found->second];
    }
} // namespace NexAur
