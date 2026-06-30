#include "pch.h"
#include "editor_property_drawer.h"

#include "Editor/Widgets/editor_widgets.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Resource/asset_metadata.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cfloat>
#include <sstream>
#include <string_view>
#include <vector>

namespace NexAur::EditorPropertyDrawer {
    namespace {
        constexpr float kValueWidth = -FLT_MIN;

        std::string assetFieldText(AssetHandle handle, const AssetManager* asset_manager) {
            if (!handle) {
                return "None";
            }

            if (asset_manager) {
                if (const AssetMetadata* metadata = asset_manager->getMetadata(handle)) {
                    if (!metadata->debug_name.empty()) {
                        return metadata->debug_name;
                    }
                    if (!metadata->path.empty()) {
                        return metadata->path;
                    }
                }
            }

            std::ostringstream stream;
            stream << "Asset " << static_cast<uint64_t>(handle.id);
            return stream.str();
        }

        void copyToBuffer(std::string_view text, char* buffer, size_t buffer_size) {
            if (!buffer || buffer_size == 0) {
                return;
            }

            const size_t copy_size = std::min(text.size(), buffer_size - 1);
            std::copy_n(text.data(), copy_size, buffer);
            buffer[copy_size] = '\0';
        }
    } // namespace

    bool drawComponentHeader(const char* title, bool default_open) {
        ImGui::Spacing();
        return EditorWidgets::sectionHeader(title, default_open);
    }

    bool drawStringProperty(const char* label, std::string& value, size_t buffer_size) {
        bool changed = false;
        EditorWidgets::propertyRow(label, [&]() {
            std::vector<char> buffer(buffer_size, '\0');
            copyToBuffer(value, buffer.data(), buffer.size());

            ImGui::SetNextItemWidth(kValueWidth);
            if (ImGui::InputText("##value", buffer.data(), buffer.size())) {
                value = std::string(buffer.data());
                changed = true;
            }
        });
        return changed;
    }

    bool drawFloatProperty(
        const char* label,
        float& value,
        float speed,
        float min,
        float max,
        const char* format,
        ImGuiSliderFlags flags) {
        bool changed = false;
        EditorWidgets::propertyRow(label, [&]() {
            ImGui::SetNextItemWidth(kValueWidth);
            changed = ImGui::DragFloat("##value", &value, speed, min, max, format, flags);
        });
        return changed;
    }

    bool drawIntProperty(
        const char* label,
        int& value,
        int speed,
        int min,
        int max,
        ImGuiSliderFlags flags) {
        bool changed = false;
        EditorWidgets::propertyRow(label, [&]() {
            ImGui::SetNextItemWidth(kValueWidth);
            changed = ImGui::DragInt("##value", &value, static_cast<float>(speed), min, max, "%d", flags);
        });
        return changed;
    }

    bool drawBoolProperty(const char* label, bool& value) {
        bool changed = false;
        EditorWidgets::propertyRow(label, [&]() {
            changed = ImGui::Checkbox("##value", &value);
        });
        return changed;
    }

    bool drawVec3Property(
        const char* label,
        glm::vec3& value,
        float speed,
        float min,
        float max,
        const char* format,
        ImGuiSliderFlags flags) {
        bool changed = false;
        EditorWidgets::propertyRow(label, [&]() {
            ImGui::SetNextItemWidth(kValueWidth);
            changed = ImGui::DragFloat3("##value", glm::value_ptr(value), speed, min, max, format, flags);
        });
        return changed;
    }

    bool drawColor3Property(const char* label, glm::vec3& value) {
        bool changed = false;
        EditorWidgets::propertyRow(label, [&]() {
            ImGui::SetNextItemWidth(kValueWidth);
            changed = ImGui::ColorEdit3("##value", glm::value_ptr(value));
        });
        return changed;
    }

    void drawAssetField(const char* label, AssetHandle handle, const AssetManager* asset_manager) {
        EditorWidgets::propertyRow(label, [&]() {
            std::array<char, 256> buffer{};
            copyToBuffer(assetFieldText(handle, asset_manager), buffer.data(), buffer.size());

            ImGui::SetNextItemWidth(kValueWidth);
            ImGui::InputText("##value", buffer.data(), buffer.size(), ImGuiInputTextFlags_ReadOnly);
        });

        if (!handle || !asset_manager) {
            return;
        }

        if (const AssetMetadata* metadata = asset_manager->getMetadata(handle)) {
            const char* type_text = assetTypeToString(metadata->type);
            EditorWidgets::propertyRow("Asset Type", [type_text]() {
                ImGui::TextDisabled("%s", type_text);
            });
        }
    }

    void drawReadOnlyText(const char* label, const std::string& value) {
        EditorWidgets::propertyRow(label, [&]() {
            ImGui::TextDisabled("%s", value.c_str());
        });
    }
} // namespace NexAur::EditorPropertyDrawer
