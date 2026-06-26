#pragma once

#include <functional>
#include <limits>
#include <string>

#include <vulkan/vulkan.h>

namespace NexAur {
    enum class VulkanGraphImageUsage {
        ColorAttachment,
        DepthStencilAttachment,
        ShaderRead,
        TransferSource,
        Present
    };

    struct VulkanGraphImageHandle {
        uint32_t index = std::numeric_limits<uint32_t>::max();

        bool valid() const {
            return index != std::numeric_limits<uint32_t>::max();
        }
    };

    struct VulkanGraphImageDesc {
        std::string name;
        VkImage image = VK_NULL_HANDLE;
        VkImageAspectFlags aspect_mask = 0;
        VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        std::function<void(VkImageLayout)> commit_layout;

        bool valid() const {
            return image != VK_NULL_HANDLE && aspect_mask != 0;
        }
    };

    struct VulkanGraphImageAccess {
        VulkanGraphImageHandle image;
        VulkanGraphImageUsage usage = VulkanGraphImageUsage::ColorAttachment;
    };

    struct VulkanGraphImageState {
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags access = 0;
        VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    };
} // namespace NexAur
