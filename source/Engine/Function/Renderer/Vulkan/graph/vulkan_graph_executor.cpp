#include "pch.h"
#include "vulkan_graph_executor.h"

#include "Function/Renderer/Vulkan/graph/vulkan_graph_state_planner.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

namespace NexAur {
    bool VulkanGraphExecutor::execute(VulkanPassGraph& graph, VkCommandBuffer command_buffer) {
        if (command_buffer == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanGraphExecutor requires a valid command buffer.");
            return false;
        }

        for (const VulkanGraphPass& pass : graph.m_passes) {
            for (const VulkanGraphImageAccess& access : pass.getImageAccesses()) {
                if (!transitionImage(command_buffer, graph, access, pass.getName().c_str())) {
                    return false;
                }
            }

            if (!pass.execute(command_buffer)) {
                NX_CORE_ERROR("Vulkan graph pass failed: {}", pass.getName());
                return false;
            }
        }

        return true;
    }

    bool VulkanGraphExecutor::transitionImage(
        VkCommandBuffer command_buffer,
        VulkanPassGraph& graph,
        const VulkanGraphImageAccess& access,
        const char* pass_name) const {
        VulkanPassGraph::ImageResource* image = graph.getImage(access.image);
        if (!image || !image->desc.valid()) {
            NX_CORE_ERROR("Vulkan graph pass '{}' referenced an invalid image.", pass_name);
            return false;
        }

        const VulkanGraphImageState destination = VulkanGraphStatePlanner::stateForUsage(
            access.usage,
            access.access_type,
            image->desc.subresource_range);
        const VulkanGraphImageTransitionPlan transition =
            VulkanGraphStatePlanner::planImageTransition(image->state, destination);

        if (transition.requires_barrier) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = transition.source.access;
            barrier.dstAccessMask = transition.destination.access;
            barrier.oldLayout = transition.source.layout;
            barrier.newLayout = transition.destination.layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image->desc.image;
            barrier.subresourceRange = transition.barrier_range.toVulkan();

            vkCmdPipelineBarrier(
                command_buffer,
                transition.source.stage,
                transition.destination.stage,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }

        image->state = transition.destination;
        return true;
    }
} // namespace NexAur
