#pragma once

#include <array>

#include <vulkan/vulkan.h>

namespace NexAur {
    inline constexpr uint32_t kVulkanAuxiliaryColorAttachmentCount = 3;

    struct VulkanRenderTarget {
        VkImageView color_view = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        std::array<VkImageView, kVulkanAuxiliaryColorAttachmentCount> auxiliary_color_views{};
        std::array<VkFormat, kVulkanAuxiliaryColorAttachmentCount> auxiliary_color_formats{};
        uint32_t auxiliary_color_attachment_count = 0;
        VkImageView depth_view = VK_NULL_HANDLE;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};

        bool valid() const {
            return color_view != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   auxiliary_color_attachment_count <= kVulkanAuxiliaryColorAttachmentCount &&
                   depth_view != VK_NULL_HANDLE &&
                   depth_format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0 &&
                   auxiliaryColorAttachmentsValid();
        }

        bool auxiliaryColorAttachmentsValid() const {
            for (uint32_t index = 0;
                 index < auxiliary_color_attachment_count;
                 ++index) {
                if (auxiliary_color_views[index] == VK_NULL_HANDLE ||
                    auxiliary_color_formats[index] == VK_FORMAT_UNDEFINED) {
                    return false;
                }
            }
            return true;
        }
    };

    struct VulkanDepthRenderTarget {
        VkImageView depth_view = VK_NULL_HANDLE;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};

        bool valid() const {
            return depth_view != VK_NULL_HANDLE &&
                   depth_format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0;
        }
    };
} // namespace NexAur
