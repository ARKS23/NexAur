#pragma once

#include "Core/Base.h"
#include "Function/Renderer/data/render_view.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_render_view.h"

namespace NexAur {
    // Converts canonical renderer data into Vulkan-native CPU descriptions.
    class NEXAUR_API VulkanRenderDataTranslator {
    public:
        VulkanRenderView buildRenderView(const RenderView& render_view) const;
    };
} // namespace NexAur
