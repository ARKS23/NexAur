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
            for (const VulkanGraphBufferAccess& access : pass.getBufferAccesses()) {
                if (!transitionBuffer(command_buffer, graph, access, pass.getName().c_str())) {
                    return false;
                }
            }
            for (const VulkanGraphAccelerationStructureAccess& access :
                 pass.getAccelerationStructureAccesses()) {
                if (!transitionAccelerationStructure(
                        command_buffer,
                        graph,
                        access,
                        pass.getName().c_str())) {
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
            VkImageMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask = transition.source.stage;
            barrier.srcAccessMask = transition.source.access;
            barrier.dstStageMask = transition.destination.stage;
            barrier.dstAccessMask = transition.destination.access;
            barrier.oldLayout = transition.source.layout;
            barrier.newLayout = transition.destination.layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image->desc.image;
            barrier.subresourceRange = transition.barrier_range.toVulkan();

            VkDependencyInfo dependency_info{};
            dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency_info.imageMemoryBarrierCount = 1;
            dependency_info.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        image->state = transition.destination;
        return true;
    }

    bool VulkanGraphExecutor::transitionBuffer(
        VkCommandBuffer command_buffer,
        VulkanPassGraph& graph,
        const VulkanGraphBufferAccess& access,
        const char* pass_name) const {
        VulkanPassGraph::BufferResource* buffer = graph.getBuffer(access.buffer);
        if (!buffer || !buffer->desc.valid()) {
            NX_CORE_ERROR("Vulkan graph pass '{}' referenced an invalid buffer.", pass_name);
            return false;
        }

        const VulkanGraphBufferState destination =
            VulkanGraphStatePlanner::stateForBufferUsage(
                access.usage,
                access.access_type);
        const VulkanGraphBufferTransitionPlan transition =
            VulkanGraphStatePlanner::planBufferTransition(buffer->state, destination);

        if (transition.requires_barrier) {
            VkBufferMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.srcStageMask = transition.source.stage;
            barrier.srcAccessMask = transition.source.access;
            barrier.dstStageMask = transition.destination.stage;
            barrier.dstAccessMask = transition.destination.access;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = buffer->desc.buffer;
            barrier.offset = buffer->desc.offset;
            barrier.size = buffer->desc.size;

            VkDependencyInfo dependency_info{};
            dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency_info.bufferMemoryBarrierCount = 1;
            dependency_info.pBufferMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        buffer->state = transition.destination;
        return true;
    }

    bool VulkanGraphExecutor::transitionAccelerationStructure(
        VkCommandBuffer command_buffer,
        VulkanPassGraph& graph,
        const VulkanGraphAccelerationStructureAccess& access,
        const char* pass_name) const {
        VulkanPassGraph::AccelerationStructureResource* acceleration_structure =
            graph.getAccelerationStructure(access.acceleration_structure);
        if (!acceleration_structure || !acceleration_structure->desc.valid()) {
            NX_CORE_ERROR(
                "Vulkan graph pass '{}' referenced an invalid acceleration structure.",
                pass_name);
            return false;
        }

        const VulkanGraphAccelerationStructureState destination =
            VulkanGraphStatePlanner::stateForAccelerationStructureUsage(
                access.usage,
                access.access_type);
        const VulkanGraphAccelerationStructureTransitionPlan transition =
            VulkanGraphStatePlanner::planAccelerationStructureTransition(
                acceleration_structure->state,
                destination);

        if (transition.requires_barrier) {
            VkMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            barrier.srcStageMask = transition.source.stage;
            barrier.srcAccessMask = transition.source.access;
            barrier.dstStageMask = transition.destination.stage;
            barrier.dstAccessMask = transition.destination.access;

            VkDependencyInfo dependency_info{};
            dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency_info.memoryBarrierCount = 1;
            dependency_info.pMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        acceleration_structure->state = transition.destination;
        return true;
    }
} // namespace NexAur
