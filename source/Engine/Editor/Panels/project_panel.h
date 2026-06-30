#pragma once

#include "editor_panel.h"

#include <array>
#include <filesystem>
#include <vector>

namespace NexAur {
    class NEXAUR_API ProjectPanel : public EditorPanel {
    public:
        explicit ProjectPanel(const std::string& name = "Project");
        ~ProjectPanel() override = default;

        void onUIRender() override;

    private:
        void drawToolbar();
        void drawDirectoryTree(const std::filesystem::path& path);
        void drawDirectoryContents(const std::filesystem::path& path);
        bool matchesFilter(const std::filesystem::path& path) const;
        std::vector<std::filesystem::directory_entry> collectEntries(
            const std::filesystem::path& path,
            bool directories_only) const;

    private:
        std::filesystem::path m_root_path{ "assets" };
        std::filesystem::path m_selected_path{ "assets" };
        std::array<char, 128> m_filter{};
        bool m_show_hidden_files = false;
    };
} // namespace NexAur
