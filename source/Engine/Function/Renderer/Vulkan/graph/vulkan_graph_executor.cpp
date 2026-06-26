#include "pch.h"
#include "vulkan_graph_executor.h"

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

        const VulkanGraphImageState old_state = stateForLayout(image->current_layout);
        const VulkanGraphImageState new_state = stateForUsage(access.usage);
        if (old_state.layout == new_state.layout) {
            return true;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = old_state.access;
        barrier.dstAccessMask = new_state.access;
        barrier.oldLayout = old_state.layout;
        barrier.newLayout = new_state.layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->desc.image;
        barrier.subresourceRange.aspectMask = image->desc.aspect_mask;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            command_buffer,
            old_state.stage,
            new_state.stage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        image->current_layout = new_state.layout;
        return true;
    }

    VulkanGraphImageState VulkanGraphExecutor::stateForUsage(VulkanGraphImageUsage usage) const {
        switch (usage) {
            case VulkanGraphImageUsage::ColorAttachment:
                return {
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                };
            case VulkanGraphImageUsage::DepthStencilAttachment:
                return {
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                };
            case VulkanGraphImageUsage::ShaderRead:
                return {
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                };
            case VulkanGraphImageUsage::TransferSource:
                return {
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                };
            case VulkanGraphImageUsage::Present:
                return {
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    0,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                };
        }

        return {};
    }

    VulkanGraphImageState VulkanGraphExecutor::stateForLayout(VkImageLayout layout) const {
        switch (layout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                return {
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    0,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                };
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return {
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                };
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return {
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                };
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return {
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                };
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return {
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                };
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                return {
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    0,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                };
            default:
                return {
                    layout,
                    VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                };
        }
    }
} // namespace NexAur
