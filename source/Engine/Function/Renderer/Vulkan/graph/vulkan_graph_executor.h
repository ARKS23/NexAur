#pragma once

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"

namespace NexAur {
    class VulkanPassGraph;

    class NEXAUR_API VulkanGraphExecutor {
    public:
        bool execute(VulkanPassGraph& graph, VkCommandBuffer command_buffer);

    private:
        bool transitionImage(
            VkCommandBuffer command_buffer,
            VulkanPassGraph& graph,
            const VulkanGraphImageAccess& access,
            const char* pass_name) const;

        VulkanGraphImageState stateForUsage(VulkanGraphImageUsage usage) const;
        VulkanGraphImageState stateForLayout(VkImageLayout layout) const;
    };
} // namespace NexAur
