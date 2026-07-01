#pragma once

#include "Core/Base.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <imgui.h>

namespace NexAur {
    class EditorCommandRegistry;

    struct NEXAUR_API EditorShortcutConfig {
        std::string command_id;
        std::string shortcut_text;
        ImGuiKeyChord key_chord = ImGuiKey_None;
    };

    struct NEXAUR_API EditorConfigData {
        int version = 1;
        std::string theme_variant = "ModernBlack";
        float viewport_camera_speed = 5.0f;
        bool auto_save_layout = true;
        std::vector<EditorShortcutConfig> shortcuts;
        std::unordered_map<std::string, bool> panel_open;
    };

    namespace EditorConfigStore {
        NEXAUR_API std::filesystem::path getConfigDirectory();
        NEXAUR_API std::filesystem::path getConfigPath();
        NEXAUR_API std::filesystem::path getImguiIniPath();

        NEXAUR_API EditorConfigData loadOrCreateDefault(const EditorCommandRegistry& commands);
        NEXAUR_API EditorConfigData loadOrCreateDefault(
            const EditorCommandRegistry& commands,
            const std::filesystem::path& config_path);
        NEXAUR_API bool save(const EditorConfigData& config);
        NEXAUR_API bool save(const EditorConfigData& config, const std::filesystem::path& config_path);
        NEXAUR_API void applyShortcuts(EditorCommandRegistry& commands, const EditorConfigData& config);
        NEXAUR_API std::vector<EditorShortcutConfig> captureShortcuts(const EditorCommandRegistry& commands);
    } // namespace EditorConfigStore
} // namespace NexAur
