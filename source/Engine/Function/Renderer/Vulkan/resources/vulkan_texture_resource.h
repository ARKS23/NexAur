#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class TextureAsset;

    class NEXAUR_API VulkanTextureResource {
    public:
        VulkanTextureResource() = default;
        ~VulkanTextureResource();

        VulkanTextureResource(const VulkanTextureResource&) = delete;
        VulkanTextureResource& operator=(const VulkanTextureResource&) = delete;

        bool create(const VulkanResourceUploadContext& context, const TextureAsset& texture);
        void reset();

        bool isReady() const {
            return m_image != VK_NULL_HANDLE &&
                   m_image_view != VK_NULL_HANDLE &&
                   m_sampler != VK_NULL_HANDLE &&
                   m_width > 0 &&
                   m_height > 0;
        }

        VkImage getImage() const { return m_image; }
        VkImageView getImageView() const { return m_image_view; }
        VkSampler getSampler() const { return m_sampler; }
        VkImageLayout getImageLayout() const { return m_image_layout; }
        VkFormat getFormat() const { return m_format; }
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }

    private:
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        VmaAllocation m_allocation = VK_NULL_HANDLE;
        VkImageView m_image_view = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
        VkImageLayout m_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };
} // namespace NexAur
