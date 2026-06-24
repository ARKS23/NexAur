#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/RendererV2/render_scene_frame.h"
#include "Function/RendererV2/render_view.h"

namespace NexAur {
    class VulkanMaterialResource;
    class VulkanMeshResource;

    struct VulkanMeshDrawItem {
        const VulkanMeshResource* mesh = nullptr;
        const VulkanMaterialResource* material = nullptr;
        glm::mat4 transform{ 1.0f };
        int entity_id = -1;
        uint64_t sort_key = 0;
    };

    struct VulkanDrawList {
        RenderView view;

        RenderFrameDirectionalLight directional_light;
        std::vector<RenderFramePointLight> point_lights;
        float environment_intensity = 1.0f;

        std::vector<VulkanMeshDrawItem> opaque_items;
        std::vector<VulkanMeshDrawItem> transparent_items;

        bool hasRenderableItems() const {
            return !opaque_items.empty() || !transparent_items.empty();
        }
    };
} // namespace NexAur
