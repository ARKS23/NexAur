#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    struct VulkanAoImageView {
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

    struct VulkanAoRenderTarget {
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

    class NEXAUR_API VulkanAoTarget {
    public:
        VulkanAoTarget() = default;
        ~VulkanAoTarget();

        VulkanAoTarget(const VulkanAoTarget&) = delete;
        VulkanAoTarget& operator=(const VulkanAoTarget&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VkFormat color_format,
            uint32_t width,
            uint32_t height,
            bool half_resolution);
        bool resize(uint32_t width, uint32_t height, bool half_resolution);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkFormat getColorFormat() const { return m_color_format; }
        VkExtent2D getExtent() const { return m_extent; }
        bool isHalfResolution() const { return m_half_resolution; }
        VkSampler getSampler() const { return m_sampler; }

        const VulkanAoImageView& getRawImage() const { return m_raw_image.view; }
        const VulkanAoImageView& getBlurredImage() const { return m_blurred_image.view; }
        VulkanAoRenderTarget getRawRenderTarget() const;
        VulkanAoRenderTarget getBlurredRenderTarget() const;

        void setRawLayout(VkImageLayout layout) { m_raw_image.view.layout = layout; }
        void setBlurredLayout(VkImageLayout layout) { m_blurred_image.view.layout = layout; }

    private:
        struct OwnedImage {
            VulkanAoImageView view;
            VkDeviceMemory memory = VK_NULL_HANDLE;
        };

        bool recreateImages(uint32_t width, uint32_t height, bool half_resolution);
        bool createImage(uint32_t width, uint32_t height, OwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};
        bool m_half_resolution = true;

        OwnedImage m_raw_image;
        OwnedImage m_blurred_image;
        VkSampler m_sampler = VK_NULL_HANDLE;
        bool m_ready = false;
    };
} // namespace NexAur
