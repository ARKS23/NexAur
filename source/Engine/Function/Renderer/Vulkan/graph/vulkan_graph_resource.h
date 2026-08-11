#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>

#include <vulkan/vulkan.h>

namespace NexAur {
    enum class VulkanGraphAccessType : uint8_t {
        None = 0,
        Read,
        Write,
        ReadWrite
    };

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

    struct VulkanGraphImageSubresourceRange {
        VkImageAspectFlags aspect_mask = 0;
        uint32_t base_mip_level = 0;
        uint32_t mip_count = 1;
        uint32_t base_array_layer = 0;
        uint32_t layer_count = 1;

        constexpr bool valid() const {
            constexpr uint64_t kMaxRangeEnd =
                static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1u;
            return aspect_mask != 0 &&
                   mip_count > 0 &&
                   layer_count > 0 &&
                   static_cast<uint64_t>(base_mip_level) + mip_count <= kMaxRangeEnd &&
                   static_cast<uint64_t>(base_array_layer) + layer_count <= kMaxRangeEnd;
        }

        constexpr VkImageSubresourceRange toVulkan() const {
            VkImageSubresourceRange range{};
            range.aspectMask = aspect_mask;
            range.baseMipLevel = base_mip_level;
            range.levelCount = mip_count;
            range.baseArrayLayer = base_array_layer;
            range.layerCount = layer_count;
            return range;
        }
    };

    struct VulkanGraphImageDesc {
        std::string name;
        VkImage image = VK_NULL_HANDLE;
        VulkanGraphImageSubresourceRange subresource_range;
        VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        std::function<void(VkImageLayout)> commit_layout;

        bool valid() const {
            return image != VK_NULL_HANDLE && subresource_range.valid();
        }
    };

    struct VulkanGraphImageAccess {
        VulkanGraphImageHandle image;
        VulkanGraphImageUsage usage = VulkanGraphImageUsage::ColorAttachment;
        VulkanGraphAccessType access_type = VulkanGraphAccessType::None;
    };

    struct VulkanGraphImageState {
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags access = 0;
        VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VulkanGraphAccessType last_access = VulkanGraphAccessType::None;
        VulkanGraphImageSubresourceRange subresource_range;
    };
} // namespace NexAur
