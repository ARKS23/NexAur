#pragma once

#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"

namespace NexAur {
    struct VulkanGraphImageTransitionPlan {
        VulkanGraphImageState source;
        VulkanGraphImageState destination;
        VulkanGraphImageSubresourceRange barrier_range;
        bool requires_barrier = false;
    };

    class VulkanGraphStatePlanner {
    public:
        static constexpr VulkanGraphImageTransitionPlan planImageTransition(
            const VulkanGraphImageState& source,
            const VulkanGraphImageState& destination) {
            const VulkanGraphImageSubresourceRange barrier_range =
                intersectSubresources(source.subresource_range, destination.subresource_range);
            if (!barrier_range.valid()) {
                return { source, destination, {}, false };
            }

            const bool layout_transition = source.layout != destination.layout;
            const bool memory_hazard =
                hasWriteAccess(source.last_access) || hasWriteAccess(destination.last_access);
            return { source, destination, barrier_range, layout_transition || memory_hazard };
        }

        static constexpr VulkanGraphImageState stateForUsage(
            VulkanGraphImageUsage usage,
            VulkanGraphAccessType access_type,
            VulkanGraphImageSubresourceRange subresource_range) {
            switch (usage) {
                case VulkanGraphImageUsage::ColorAttachment:
                    return {
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        colorAttachmentAccess(access_type),
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        access_type,
                        subresource_range
                    };
                case VulkanGraphImageUsage::DepthStencilAttachment:
                    return {
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        depthStencilAttachmentAccess(access_type),
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        access_type,
                        subresource_range
                    };
                case VulkanGraphImageUsage::ShaderRead:
                    return {
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_ACCESS_2_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        access_type,
                        subresource_range
                    };
                case VulkanGraphImageUsage::TransferSource:
                    return {
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_2_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        access_type,
                        subresource_range
                    };
                case VulkanGraphImageUsage::Present:
                    return {
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_NONE,
                        access_type,
                        subresource_range
                    };
            }

            return {};
        }

        static constexpr VulkanGraphImageState stateForLayout(
            VkImageLayout layout,
            VulkanGraphImageSubresourceRange subresource_range) {
            switch (layout) {
                case VK_IMAGE_LAYOUT_UNDEFINED:
                    return {
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_NONE,
                        VulkanGraphAccessType::None,
                        subresource_range
                    };
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    return {
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VulkanGraphAccessType::Write,
                        subresource_range
                    };
                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                    return {
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        VulkanGraphAccessType::ReadWrite,
                        subresource_range
                    };
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return {
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_ACCESS_2_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VulkanGraphAccessType::Read,
                        subresource_range
                    };
                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    return {
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_2_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VulkanGraphAccessType::Read,
                        subresource_range
                    };
                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                    return {
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_NONE,
                        VulkanGraphAccessType::Read,
                        subresource_range
                    };
                default:
                    return {
                        layout,
                        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                        VulkanGraphAccessType::ReadWrite,
                        subresource_range
                    };
            }
        }

        static constexpr VulkanGraphImageState stateForImport(
            VkImageLayout layout,
            VulkanGraphImageSubresourceRange subresource_range,
            VkPipelineStageFlags2 external_acquire_stage) {
            VulkanGraphImageState state = stateForLayout(layout, subresource_range);
            if (external_acquire_stage != VK_PIPELINE_STAGE_2_NONE) {
                state.stage = external_acquire_stage;
            }
            return state;
        }

        static constexpr bool subresourcesOverlap(
            const VulkanGraphImageSubresourceRange& lhs,
            const VulkanGraphImageSubresourceRange& rhs) {
            return intersectSubresources(lhs, rhs).valid();
        }

    private:
        static constexpr VkAccessFlags2 colorAttachmentAccess(VulkanGraphAccessType access_type) {
            switch (access_type) {
                case VulkanGraphAccessType::Read:
                    return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
                case VulkanGraphAccessType::Write:
                    return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                case VulkanGraphAccessType::ReadWrite:
                    return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                           VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                case VulkanGraphAccessType::None:
                default:
                    return VK_ACCESS_2_NONE;
            }
        }

        static constexpr VkAccessFlags2 depthStencilAttachmentAccess(VulkanGraphAccessType access_type) {
            switch (access_type) {
                case VulkanGraphAccessType::Read:
                    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                case VulkanGraphAccessType::Write:
                case VulkanGraphAccessType::ReadWrite:
                    return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                case VulkanGraphAccessType::None:
                default:
                    return VK_ACCESS_2_NONE;
            }
        }

        static constexpr bool hasWriteAccess(VulkanGraphAccessType access_type) {
            return access_type == VulkanGraphAccessType::Write ||
                   access_type == VulkanGraphAccessType::ReadWrite;
        }

        static constexpr VulkanGraphImageSubresourceRange intersectSubresources(
            const VulkanGraphImageSubresourceRange& lhs,
            const VulkanGraphImageSubresourceRange& rhs) {
            const VkImageAspectFlags aspect_mask = lhs.aspect_mask & rhs.aspect_mask;
            const uint64_t mip_begin = lhs.base_mip_level > rhs.base_mip_level ?
                lhs.base_mip_level : rhs.base_mip_level;
            const uint64_t mip_end = rangeEnd(lhs.base_mip_level, lhs.mip_count) <
                rangeEnd(rhs.base_mip_level, rhs.mip_count) ?
                rangeEnd(lhs.base_mip_level, lhs.mip_count) :
                rangeEnd(rhs.base_mip_level, rhs.mip_count);
            const uint64_t layer_begin = lhs.base_array_layer > rhs.base_array_layer ?
                lhs.base_array_layer : rhs.base_array_layer;
            const uint64_t layer_end = rangeEnd(lhs.base_array_layer, lhs.layer_count) <
                rangeEnd(rhs.base_array_layer, rhs.layer_count) ?
                rangeEnd(lhs.base_array_layer, lhs.layer_count) :
                rangeEnd(rhs.base_array_layer, rhs.layer_count);

            if (aspect_mask == 0 || mip_begin >= mip_end || layer_begin >= layer_end) {
                return {};
            }

            VulkanGraphImageSubresourceRange intersection;
            intersection.aspect_mask = aspect_mask;
            intersection.base_mip_level = static_cast<uint32_t>(mip_begin);
            intersection.mip_count = static_cast<uint32_t>(mip_end - mip_begin);
            intersection.base_array_layer = static_cast<uint32_t>(layer_begin);
            intersection.layer_count = static_cast<uint32_t>(layer_end - layer_begin);
            return intersection;
        }

        static constexpr uint64_t rangeEnd(uint32_t base, uint32_t count) {
            return static_cast<uint64_t>(base) + count;
        }
    };
} // namespace NexAur
