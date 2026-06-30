#include "pch.h"
#include "project_panel.h"

#include "Editor/Widgets/editor_widgets.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

namespace NexAur {
    namespace {
        std::string toLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

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
            if (isDirectory(entry)) {
                return "Folder";
            }

            const std::string extension = toLower(entry.path().extension().string());
            if (extension == ".gltf" || extension == ".glb" ||
                extension == ".obj" || extension == ".fbx") {
                return "Model";
            }
            if (extension == ".png" || extension == ".jpg" ||
                extension == ".jpeg" || extension == ".tga" ||
                extension == ".ktx" || extension == ".hdr") {
                return "Texture";
            }
            if (extension == ".hlsl" || extension == ".hlsli" || extension == ".spv") {
                return "Shader";
            }
            if (extension == ".nxscene") {
                return "Scene";
            }
            if (extension == ".json" || extension == ".md") {
                return "Document";
            }

            return "File";
        }

        bool isHiddenPath(const std::filesystem::path& path) {
            const std::string name = path.filename().string();
            return !name.empty() && name[0] == '.';
        }

        std::string fileSizeText(const std::filesystem::directory_entry& entry) {
            if (isDirectory(entry)) {
                return "-";
            }

            std::error_code ec;
            const uintmax_t bytes = entry.file_size(ec);
            if (ec) {
                return "-";
            }

            constexpr double kKiB = 1024.0;
            constexpr double kMiB = kKiB * 1024.0;

            std::ostringstream stream;
            if (bytes >= static_cast<uintmax_t>(kMiB)) {
                stream << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / kMiB) << " MB";
            } else if (bytes >= static_cast<uintmax_t>(kKiB)) {
                stream << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / kKiB) << " KB";
            } else {
                stream << bytes << " B";
            }

            return stream.str();
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

        drawToolbar();

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

    void ProjectPanel::drawToolbar() {
        if (EditorWidgets::toolbarButton("Assets", "Open the project assets root")) {
            m_selected_path = m_root_path;
        }

        ImGui::SameLine();
        const bool can_go_up = m_selected_path != m_root_path;
        if (!can_go_up) {
            ImGui::BeginDisabled();
        }
        if (EditorWidgets::toolbarButton("Up", "Open parent folder")) {
            const std::filesystem::path parent = m_selected_path.parent_path();
            if (!parent.empty()) {
                const std::string root = m_root_path.generic_string();
                const std::string parent_text = parent.generic_string();
                if (parent_text == root || parent_text.find(root + "/") == 0) {
                    m_selected_path = parent;
                }
            }
        }
        if (!can_go_up) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        EditorWidgets::searchBox("##ProjectFilter", m_filter.data(), m_filter.size(), "Search assets");

        ImGui::SameLine();
        ImGui::Checkbox("Hidden", &m_show_hidden_files);

        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_selected_path.generic_string().c_str());

        ImGui::Separator();
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
        const std::vector<std::filesystem::directory_entry> directories =
            collectEntries(path, true);
        std::vector<std::filesystem::directory_entry> files =
            collectEntries(path, false);

        const ImGuiTableFlags table_flags =
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("ProjectContentsTable", 3, table_flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.64f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.18f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 0.18f);
            ImGui::TableHeadersRow();

            int visible_count = 0;
            for (const auto& entry : directories) {
                if (!matchesFilter(entry.path())) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                const std::string label = "> " + displayName(entry.path());
                if (EditorWidgets::tableSelectableRow(label.c_str())) {
                    m_selected_path = entry.path();
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", entryTypeText(entry));

                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("%s", fileSizeText(entry).c_str());

                ++visible_count;
            }

            for (const auto& entry : files) {
                if (isDirectory(entry)) {
                    continue;
                }
                if (!matchesFilter(entry.path())) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                const std::string label = displayName(entry.path());
                EditorWidgets::tableSelectableRow(label.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", entryTypeText(entry));

                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("%s", fileSizeText(entry).c_str());

                ++visible_count;
            }

            if (visible_count == 0) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", m_filter[0] == '\0' ? "Folder is empty." : "No assets match the current filter.");
            }

            ImGui::EndTable();
        }
    }

    bool ProjectPanel::matchesFilter(const std::filesystem::path& path) const {
        if (m_filter[0] == '\0') {
            return true;
        }

        const std::string needle = toLower(m_filter.data());
        const std::string filename = toLower(path.filename().string());
        const std::string extension = toLower(path.extension().string());
        return filename.find(needle) != std::string::npos ||
            extension.find(needle) != std::string::npos;
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

            if (!m_show_hidden_files && isHiddenPath(entry.path())) {
                continue;
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
