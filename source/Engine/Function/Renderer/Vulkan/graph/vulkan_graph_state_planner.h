#pragma once

#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"

namespace NexAur {
    struct VulkanGraphImageTransitionPlan {
        VulkanGraphImageState source;
        VulkanGraphImageState destination;
        bool requires_barrier = false;
    };

    class VulkanGraphStatePlanner {
    public:
        static constexpr VulkanGraphImageTransitionPlan planImageTransition(
            VkImageLayout current_layout,
            VulkanGraphImageUsage usage) {
            const VulkanGraphImageState source = stateForLayout(current_layout);
            const VulkanGraphImageState destination = stateForUsage(usage);
            return { source, destination, source.layout != destination.layout };
        }

        static constexpr VulkanGraphImageState stateForUsage(VulkanGraphImageUsage usage) {
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

        static constexpr VulkanGraphImageState stateForLayout(VkImageLayout layout) {
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
    };
} // namespace NexAur
