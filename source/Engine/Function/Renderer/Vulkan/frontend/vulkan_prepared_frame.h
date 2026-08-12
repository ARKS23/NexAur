#pragma once

#include "Function/Renderer/data/render_scene_frame.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"

namespace NexAur {
    // Owns the immutable canonical frame snapshot and the Vulkan resources
    // prepared from that same snapshot for one renderer invocation.
    struct VulkanPreparedFrame {
        RenderSceneFrame scene;
        VulkanDrawList draw_list;
    };
} // namespace NexAur
