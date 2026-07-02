#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    struct VulkanSsrImageView {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

        bool valid() const {
            return image != VK_NULL_HANDLE &&
                   view != VK_NULL_HANDLE &&
                   format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0;
        }
    };

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
        VkSampler getSampler() const { return m_sampler; }

        const VulkanSsrImageView& getRawReflectionImage() const { return m_raw_reflection_image.view; }
        const VulkanSsrImageView& getHitMaskImage() const { return m_hit_mask_image.view; }
        VulkanSsrRenderTarget getRawReflectionRenderTarget() const;
        VulkanSsrRenderTarget getHitMaskRenderTarget() const;

        void setRawReflectionLayout(VkImageLayout layout) { m_raw_reflection_image.view.layout = layout; }
        void setHitMaskLayout(VkImageLayout layout) { m_hit_mask_image.view.layout = layout; }

    private:
        struct OwnedImage {
            VulkanSsrImageView view;
            VkDeviceMemory memory = VK_NULL_HANDLE;
        };

        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(uint32_t width, uint32_t height, VkFormat format, OwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_reflection_format = VK_FORMAT_UNDEFINED;
        VkFormat m_hit_mask_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        OwnedImage m_raw_reflection_image;
        OwnedImage m_hit_mask_image;
        VkSampler m_sampler = VK_NULL_HANDLE;
        bool m_ready = false;
    };
} // namespace NexAur
