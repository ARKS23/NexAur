#pragma once

#include "Core/Base.h"

#include <imgui.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NexAur {
    struct NEXAUR_API EditorCommand {
        std::string id;
        std::string display_name;
        std::string tooltip;
        std::string shortcut_text;
        ImGuiKeyChord shortcut = ImGuiKey_None;
        std::function<bool()> enabled;
        std::function<bool()> selected;
        std::function<void()> execute;
    };

    class NEXAUR_API EditorCommandRegistry {
    public:
        bool registerCommand(EditorCommand command);
        bool execute(const std::string& id) const;
        bool processShortcuts() const;
        bool isEnabled(const std::string& id) const;
        bool isSelected(const std::string& id) const;

        const EditorCommand* find(const std::string& id) const;
        const std::vector<std::string>& getCommandOrder() const { return m_ordered_ids; }

    private:
        std::vector<EditorCommand> m_commands;
        std::vector<std::string> m_ordered_ids;
        std::unordered_map<std::string, size_t> m_command_indices;
    };
} // namespace NexAur
