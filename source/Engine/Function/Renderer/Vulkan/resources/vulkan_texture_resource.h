#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/upload/vulkan_upload_manager.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class TextureAsset;

    class VulkanTextureResource {
    public:
        VulkanTextureResource() = default;
        ~VulkanTextureResource();

        VulkanTextureResource(const VulkanTextureResource&) = delete;
        VulkanTextureResource& operator=(const VulkanTextureResource&) = delete;

        bool create(const VulkanResourceUploadContext& context, const TextureAsset& texture);
        void reset();

        bool isReady() const {
            return m_upload_ticket.isReady() &&
                   m_image.isReady() &&
                   m_sampler.isReady() &&
                   m_width > 0 &&
                   m_height > 0;
        }

        VulkanUploadStatus getUploadStatus() const {
            return m_upload_ticket.getStatus();
        }
        const VulkanUploadTicket& getUploadTicket() const {
            return m_upload_ticket;
        }
        VkImage getImage() const { return m_image.getImage(); }
        VkImageView getImageView() const { return m_image.getImageView(); }
        VkSampler getSampler() const { return m_sampler.get(); }
        VkImageLayout getImageLayout() const { return m_image.getLayout(); }
        VkFormat getFormat() const { return m_format; }
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }
        uint32_t getMipLevels() const { return m_mip_levels; }

    private:
        VulkanOwnedImage m_image;
        VulkanOwnedSampler m_sampler;
        VulkanUploadTicket m_upload_ticket;
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_mip_levels = 1;
    };
} // namespace NexAur
