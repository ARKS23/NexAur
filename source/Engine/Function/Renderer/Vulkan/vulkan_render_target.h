#pragma once

#include <vulkan/vulkan.h>

namespace NexAur {
    struct VulkanRenderTarget {
        VkImageView color_view = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkImageView depth_view = VK_NULL_HANDLE;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};

        bool valid() const {
            return color_view != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   depth_view != VK_NULL_HANDLE &&
                   depth_format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0;
        }
    };
} // namespace NexAur
