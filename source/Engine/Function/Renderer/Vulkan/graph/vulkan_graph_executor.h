#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"

namespace NexAur {
    class VulkanPassGraph;

    class VulkanGraphExecutor {
    public:
        bool execute(VulkanPassGraph& graph, VkCommandBuffer command_buffer);

    private:
        bool transitionImage(
            VkCommandBuffer command_buffer,
            VulkanPassGraph& graph,
            const VulkanGraphImageAccess& access,
            const char* pass_name) const;
        bool transitionBuffer(
            VkCommandBuffer command_buffer,
            VulkanPassGraph& graph,
            const VulkanGraphBufferAccess& access,
            const char* pass_name) const;
        bool transitionAccelerationStructure(
            VkCommandBuffer command_buffer,
            VulkanPassGraph& graph,
            const VulkanGraphAccelerationStructureAccess& access,
            const char* pass_name) const;
    };
} // namespace NexAur
