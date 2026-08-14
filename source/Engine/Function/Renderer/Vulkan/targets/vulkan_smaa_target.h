#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    using VulkanSmaaImageView = VulkanImageViewState;

    struct VulkanSmaaRenderTarget {
        VkImageView color_view = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};

        bool valid() const {
            return color_view != VK_NULL_HANDLE &&
                   color_format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0;
        }
    };

    class VulkanSmaaTarget {
    public:
        VulkanSmaaTarget() = default;
        ~VulkanSmaaTarget();

        VulkanSmaaTarget(const VulkanSmaaTarget&) = delete;
        VulkanSmaaTarget& operator=(const VulkanSmaaTarget&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VkFormat source_format,
            VkFormat mask_format,
            uint32_t width,
            uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFormat getSourceFormat() const { return m_source_format; }
        VkFormat getMaskFormat() const { return m_mask_format; }
        VkSampler getSampler() const { return m_sampler.get(); }

        const VulkanSmaaImageView& getSourceImage() const { return m_source_image.getView(); }
        const VulkanSmaaImageView& getEdgeImage() const { return m_edge_image.getView(); }
        const VulkanSmaaImageView& getBlendImage() const { return m_blend_image.getView(); }

        VulkanSmaaRenderTarget getSourceRenderTarget() const;
        VulkanSmaaRenderTarget getEdgeRenderTarget() const;
        VulkanSmaaRenderTarget getBlendRenderTarget() const;

        void setSourceLayout(VkImageLayout layout) { m_source_image.setLayout(layout); }
        void setEdgeLayout(VkImageLayout layout) { m_edge_image.setLayout(layout); }
        void setBlendLayout(VkImageLayout layout) { m_blend_image.setLayout(layout); }

    private:
        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(uint32_t width, uint32_t height, VkFormat format, VulkanOwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_source_format = VK_FORMAT_UNDEFINED;
        VkFormat m_mask_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        VulkanOwnedImage m_source_image;
        VulkanOwnedImage m_edge_image;
        VulkanOwnedImage m_blend_image;
        VulkanOwnedSampler m_sampler;
        bool m_ready = false;
    };
} // namespace NexAur
