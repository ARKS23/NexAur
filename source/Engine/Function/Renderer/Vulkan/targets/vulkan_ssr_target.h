#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    using VulkanSsrImageView = VulkanImageViewState;

    struct VulkanSsrRenderTarget {
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

    class NEXAUR_API VulkanSsrTarget {
    public:
        VulkanSsrTarget() = default;
        ~VulkanSsrTarget();

        VulkanSsrTarget(const VulkanSsrTarget&) = delete;
        VulkanSsrTarget& operator=(const VulkanSsrTarget&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VkFormat reflection_format,
            VkFormat hit_mask_format,
            uint32_t width,
            uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFormat getReflectionFormat() const { return m_reflection_format; }
        VkFormat getHitMaskFormat() const { return m_hit_mask_format; }
        VkSampler getSampler() const { return m_sampler.get(); }

        const VulkanSsrImageView& getRawReflectionImage() const { return m_raw_reflection_image.getView(); }
        const VulkanSsrImageView& getHitMaskImage() const { return m_hit_mask_image.getView(); }
        VulkanSsrRenderTarget getRawReflectionRenderTarget() const;
        VulkanSsrRenderTarget getHitMaskRenderTarget() const;

        void setRawReflectionLayout(VkImageLayout layout) { m_raw_reflection_image.setLayout(layout); }
        void setHitMaskLayout(VkImageLayout layout) { m_hit_mask_image.setLayout(layout); }

    private:
        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(uint32_t width, uint32_t height, VkFormat format, VulkanOwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_reflection_format = VK_FORMAT_UNDEFINED;
        VkFormat m_hit_mask_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        VulkanOwnedImage m_raw_reflection_image;
        VulkanOwnedImage m_hit_mask_image;
        VulkanOwnedSampler m_sampler;
        bool m_ready = false;
    };
} // namespace NexAur
