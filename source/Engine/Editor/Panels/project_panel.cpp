#include "pch.h"
#include "project_panel.h"

#include <imgui.h>

#include <algorithm>
#include <system_error>

namespace NexAur {
    namespace {
        std::string displayName(const std::filesystem::path& path) {
            if (path.filename().empty()) {
                return path.string();
            }

            return path.filename().string();
        }

        bool isDirectory(const std::filesystem::directory_entry& entry) {
            std::error_code ec;
            return entry.is_directory(ec) && !ec;
        }

        const char* entryTypeText(const std::filesystem::directory_entry& entry) {
            return isDirectory(entry) ? "Folder" : "File";
        }
    } // namespace

    ProjectPanel::ProjectPanel(const std::string& name)
        : EditorPanel(name) {}

    void ProjectPanel::onUIRender() {
        bool& open_flag = getOpenFlag();
        if (!ImGui::Begin(getName().c_str(), &open_flag)) {
            ImGui::End();
            return;
        }

        std::error_code ec;
        if (!std::filesystem::exists(m_root_path, ec)) {
            ImGui::TextDisabled("Assets folder unavailable.");
            ImGui::End();
            return;
        }

        if (!std::filesystem::exists(m_selected_path, ec)) {
            m_selected_path = m_root_path;
        }

        const float tree_width = std::min(180.0f, ImGui::GetContentRegionAvail().x * 0.35f);
        ImGui::BeginChild("ProjectTree", ImVec2(tree_width, 0.0f), true);
        drawDirectoryTree(m_root_path);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("ProjectContents", ImVec2(0.0f, 0.0f), true);
        drawDirectoryContents(m_selected_path);
        ImGui::EndChild();

        ImGui::End();
    }

    void ProjectPanel::drawDirectoryTree(const std::filesystem::path& path) {
        std::error_code ec;
        const bool selected = std::filesystem::equivalent(path, m_selected_path, ec) && !ec;
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const std::vector<std::filesystem::directory_entry> directories =
            collectEntries(path, true);

        if (directories.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const std::string label = displayName(path);
        const bool opened = ImGui::TreeNodeEx(path.string().c_str(), flags, "%s", label.c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_selected_path = path;
        }

        if (opened && !directories.empty()) {
            for (const auto& entry : directories) {
                drawDirectoryTree(entry.path());
            }
            ImGui::TreePop();
        }
    }

    void ProjectPanel::drawDirectoryContents(const std::filesystem::path& path) {
        ImGui::TextDisabled("%s", path.generic_string().c_str());
        ImGui::Separator();

        const std::vector<std::filesystem::directory_entry> directories =
            collectEntries(path, true);
        std::vector<std::filesystem::directory_entry> files =
            collectEntries(path, false);

        const ImGuiTableFlags table_flags =
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("ProjectContentsTable", 2, table_flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.75f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableHeadersRow();

            for (const auto& entry : directories) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                const std::string label = "> " + displayName(entry.path());
                if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                    m_selected_path = entry.path();
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", entryTypeText(entry));
            }

            for (const auto& entry : files) {
                if (isDirectory(entry)) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                const std::string label = displayName(entry.path());
                ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", entryTypeText(entry));
            }

            ImGui::EndTable();
        }

        if (directories.empty() && files.empty()) {
            ImGui::TextDisabled("Folder is empty.");
        }
    }

    std::vector<std::filesystem::directory_entry> ProjectPanel::collectEntries(
        const std::filesystem::path& path,
        bool directories_only) const {
        std::vector<std::filesystem::directory_entry> entries;

        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
            return entries;
        }

        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            if (ec) {
                break;
            }

            if (directories_only && !entry.is_directory(ec)) {
                continue;
            }

            entries.push_back(entry);
        }

        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            const bool lhs_directory = isDirectory(lhs);
            const bool rhs_directory = isDirectory(rhs);
            if (lhs_directory != rhs_directory) {
                return lhs_directory > rhs_directory;
            }

            return lhs.path().filename().string() < rhs.path().filename().string();
        });

        return entries;
    }
} // namespace NexAur
