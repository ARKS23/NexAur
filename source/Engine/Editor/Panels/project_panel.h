#pragma once

#include "editor_panel.h"

#include <filesystem>
#include <vector>

namespace NexAur {
    class NEXAUR_API ProjectPanel : public EditorPanel {
    public:
        explicit ProjectPanel(const std::string& name = "Project");
        ~ProjectPanel() override = default;

        void onUIRender() override;

    private:
        void drawDirectoryTree(const std::filesystem::path& path);
        void drawDirectoryContents(const std::filesystem::path& path);
        std::vector<std::filesystem::directory_entry> collectEntries(
            const std::filesystem::path& path,
            bool directories_only) const;

    private:
        std::filesystem::path m_root_path{ "assets" };
        std::filesystem::path m_selected_path{ "assets" };
    };
} // namespace NexAur
