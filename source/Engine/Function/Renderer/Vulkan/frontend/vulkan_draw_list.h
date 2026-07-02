#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Renderer/data/render_scene_frame.h"
#include "Function/Renderer/data/render_view.h"

namespace NexAur {
    class VulkanEnvironmentResource;
    class VulkanMaterialResource;
    class VulkanMeshResource;

    struct VulkanMeshDrawItem {
        const VulkanMeshResource* mesh = nullptr;
        const VulkanMaterialResource* material = nullptr;
        glm::mat4 transform{ 1.0f };
        int entity_id = -1;
        uint64_t sort_key = 0;
    };

    struct VulkanActiveReflectionProbe {
        bool enabled = false;
        AssetHandle environment_asset;
        const VulkanEnvironmentResource* environment = nullptr;
        glm::vec3 position{ 0.0f };
        glm::vec3 box_extents{ 4.0f, 3.0f, 4.0f };
        float intensity = 0.0f;
        float blend_distance = 0.75f;
        bool box_projection = true;
        uint32_t prefilter_mip_count = 1;
    };

    struct VulkanDrawList {
        RenderView view;

        RenderFrameDirectionalLight directional_light;
        std::vector<RenderFramePointLight> point_lights;
        std::vector<RenderFrameRectLight> rect_lights;
        std::vector<RenderFrameReflectionProbe> reflection_probes;
        VulkanActiveReflectionProbe active_reflection_probe;
        AssetHandle environment_asset;
        const VulkanEnvironmentResource* environment = nullptr;
        glm::vec3 environment_color{ 0.08f, 0.10f, 0.14f };
        float skybox_intensity = 0.75f;
        float ibl_intensity = 0.65f;

        std::vector<VulkanMeshDrawItem> opaque_items;
        std::vector<VulkanMeshDrawItem> transparent_items;
        RenderDebugDrawData debug_draw;

        bool hasRenderableItems() const {
            return !opaque_items.empty() || !transparent_items.empty();
        }
    };
} // namespace NexAur
