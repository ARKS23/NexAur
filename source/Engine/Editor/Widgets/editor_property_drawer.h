#pragma once

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"

#include <glm/glm.hpp>
#include <imgui.h>

#include <cstddef>
#include <string>

namespace NexAur {
    class AssetManager;
} // namespace NexAur

namespace NexAur::EditorPropertyDrawer {
    NEXAUR_API bool drawComponentHeader(
        const char* title,
        bool default_open = true);

    NEXAUR_API bool drawStringProperty(
        const char* label,
        std::string& value,
        size_t buffer_size = 256);

    NEXAUR_API bool drawFloatProperty(
        const char* label,
        float& value,
        float speed = 0.05f,
        float min = 0.0f,
        float max = 0.0f,
        const char* format = "%.3f",
        ImGuiSliderFlags flags = 0);

    NEXAUR_API bool drawIntProperty(
        const char* label,
        int& value,
        int speed = 1,
        int min = 0,
        int max = 0,
        ImGuiSliderFlags flags = 0);

    NEXAUR_API bool drawBoolProperty(
        const char* label,
        bool& value);

    NEXAUR_API bool drawVec3Property(
        const char* label,
        glm::vec3& value,
        float speed = 0.05f,
        float min = 0.0f,
        float max = 0.0f,
        const char* format = "%.3f",
        ImGuiSliderFlags flags = 0);

    NEXAUR_API bool drawColor3Property(
        const char* label,
        glm::vec3& value);

    NEXAUR_API void drawAssetField(
        const char* label,
        AssetHandle handle,
        const AssetManager* asset_manager = nullptr);

    NEXAUR_API void drawReadOnlyText(
        const char* label,
        const std::string& value);
} // namespace NexAur::EditorPropertyDrawer
