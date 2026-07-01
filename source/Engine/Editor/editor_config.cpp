#include "pch.h"
#include "editor_config.h"

#include "Editor/Commands/editor_command.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

#ifndef ENGINE_ROOT_DIR
#define ENGINE_ROOT_DIR "."
#endif

namespace NexAur {
    namespace {
        constexpr int kEditorConfigVersion = 1;

        std::filesystem::path getEngineRootPath() {
            return std::filesystem::path(ENGINE_ROOT_DIR);
        }

        EditorConfigData makeDefaultConfig(const EditorCommandRegistry& commands) {
            EditorConfigData config;
            config.version = kEditorConfigVersion;
            config.shortcuts = EditorConfigStore::captureShortcuts(commands);
            return config;
        }

        void ensureConfigDirectory(const std::filesystem::path& config_path) {
            const std::filesystem::path directory = config_path.parent_path();
            if (directory.empty()) {
                return;
            }

            std::error_code error;
            std::filesystem::create_directories(directory, error);
            if (error) {
                NX_CORE_WARN("Failed to create editor config directory: {}", error.message());
            }
        }

        void mergeDefaultShortcuts(EditorConfigData& config, const EditorCommandRegistry& commands) {
            std::unordered_map<std::string, size_t> existing;
            for (size_t i = 0; i < config.shortcuts.size(); ++i) {
                if (!config.shortcuts[i].command_id.empty()) {
                    existing.emplace(config.shortcuts[i].command_id, i);
                }
            }

            for (const EditorShortcutConfig& shortcut : EditorConfigStore::captureShortcuts(commands)) {
                if (shortcut.command_id.empty()) {
                    continue;
                }
                if (existing.find(shortcut.command_id) == existing.end()) {
                    config.shortcuts.push_back(shortcut);
                }
            }
        }

        EditorConfigData parseConfigJson(const nlohmann::json& json, const EditorCommandRegistry& commands) {
            EditorConfigData config = makeDefaultConfig(commands);

            config.version = json.value("version", kEditorConfigVersion);
            config.theme_variant = json.value("theme_variant", config.theme_variant);
            config.viewport_camera_speed = std::clamp(
                json.value("viewport_camera_speed", config.viewport_camera_speed),
                0.1f,
                100.0f);
            config.auto_save_layout = json.value("auto_save_layout", config.auto_save_layout);

            config.shortcuts.clear();
            if (json.contains("shortcuts") && json["shortcuts"].is_array()) {
                for (const auto& item : json["shortcuts"]) {
                    EditorShortcutConfig shortcut;
                    shortcut.command_id = item.value("command_id", std::string());
                    shortcut.shortcut_text = item.value("shortcut_text", std::string());
                    shortcut.key_chord = static_cast<ImGuiKeyChord>(item.value("key_chord", 0));

                    if (!shortcut.command_id.empty()) {
                        config.shortcuts.push_back(std::move(shortcut));
                    }
                }
            }

            if (json.contains("panels") && json["panels"].is_object()) {
                config.panel_open.clear();
                for (auto it = json["panels"].begin(); it != json["panels"].end(); ++it) {
                    if (it.value().is_boolean()) {
                        config.panel_open[it.key()] = it.value().get<bool>();
                    }
                }
            }

            mergeDefaultShortcuts(config, commands);
            return config;
        }

        nlohmann::json toJson(const EditorConfigData& config) {
            nlohmann::json json;
            json["version"] = kEditorConfigVersion;
            json["theme_variant"] = config.theme_variant;
            json["viewport_camera_speed"] = config.viewport_camera_speed;
            json["auto_save_layout"] = config.auto_save_layout;
            json["layout"]["imgui_ini"] = EditorConfigStore::getImguiIniPath().generic_string();

            json["shortcuts"] = nlohmann::json::array();
            for (const EditorShortcutConfig& shortcut : config.shortcuts) {
                json["shortcuts"].push_back({
                    { "command_id", shortcut.command_id },
                    { "shortcut_text", shortcut.shortcut_text },
                    { "key_chord", static_cast<int>(shortcut.key_chord) }
                });
            }

            json["panels"] = nlohmann::json::object();
            for (const auto& [name, open] : config.panel_open) {
                json["panels"][name] = open;
            }

            return json;
        }
    } // namespace

    namespace EditorConfigStore {
        std::filesystem::path getConfigDirectory() {
            return getEngineRootPath() / "saved" / "editor";
        }

        std::filesystem::path getConfigPath() {
            return getConfigDirectory() / "editor_config.json";
        }

        std::filesystem::path getImguiIniPath() {
            return getConfigDirectory() / "imgui.ini";
        }

        EditorConfigData loadOrCreateDefault(const EditorCommandRegistry& commands) {
            return loadOrCreateDefault(commands, getConfigPath());
        }

        EditorConfigData loadOrCreateDefault(
            const EditorCommandRegistry& commands,
            const std::filesystem::path& config_path) {
            ensureConfigDirectory(config_path);

            if (!std::filesystem::exists(config_path)) {
                EditorConfigData config = makeDefaultConfig(commands);
                save(config, config_path);
                return config;
            }

            std::ifstream stream(config_path);
            if (!stream.is_open()) {
                NX_CORE_WARN("Failed to open editor config: {}", config_path.string());
                return makeDefaultConfig(commands);
            }

            try {
                nlohmann::json json;
                stream >> json;
                return parseConfigJson(json, commands);
            } catch (const std::exception& e) {
                NX_CORE_WARN("Failed to parse editor config '{}': {}", config_path.string(), e.what());
                return makeDefaultConfig(commands);
            }
        }

        bool save(const EditorConfigData& config) {
            return save(config, getConfigPath());
        }

        bool save(const EditorConfigData& config, const std::filesystem::path& config_path) {
            ensureConfigDirectory(config_path);

            std::ofstream stream(config_path, std::ios::trunc);
            if (!stream.is_open()) {
                NX_CORE_WARN("Failed to write editor config: {}", config_path.string());
                return false;
            }

            stream << toJson(config).dump(4);
            return true;
        }

        void applyShortcuts(EditorCommandRegistry& commands, const EditorConfigData& config) {
            for (const EditorShortcutConfig& shortcut : config.shortcuts) {
                if (shortcut.command_id.empty()) {
                    continue;
                }

                commands.updateShortcut(shortcut.command_id, shortcut.key_chord, shortcut.shortcut_text);
            }
        }

        std::vector<EditorShortcutConfig> captureShortcuts(const EditorCommandRegistry& commands) {
            std::vector<EditorShortcutConfig> shortcuts;

            for (const std::string& command_id : commands.getCommandOrder()) {
                const EditorCommand* command = commands.find(command_id);
                if (!command) {
                    continue;
                }
                if (command->shortcut == ImGuiKey_None && command->shortcut_text.empty()) {
                    continue;
                }

                shortcuts.push_back({
                    command->id,
                    command->shortcut_text,
                    command->shortcut
                });
            }

            return shortcuts;
        }
    } // namespace EditorConfigStore
} // namespace NexAur
