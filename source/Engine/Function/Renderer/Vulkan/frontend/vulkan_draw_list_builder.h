#pragma once

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"

namespace NexAur {
    class AssetManager;
    class VulkanRenderResourceCache;

    class NEXAUR_API VulkanDrawListBuilder {
    public:
        VulkanDrawList buildDrawList(
            const RenderSceneFrame& scene_frame,
            VulkanRenderResourceCache& resource_cache,
            AssetManager& asset_manager) const;
    };
} // namespace NexAur
