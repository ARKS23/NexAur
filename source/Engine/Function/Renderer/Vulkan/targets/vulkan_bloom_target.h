#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    struct VulkanBloomImageView {
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

    struct VulkanBloomRenderTarget {
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

    class NEXAUR_API VulkanBloomTarget {
    public:
        VulkanBloomTarget() = default;
        ~VulkanBloomTarget();

        VulkanBloomTarget(const VulkanBloomTarget&) = delete;
        VulkanBloomTarget& operator=(const VulkanBloomTarget&) = delete;

        bool init(const VulkanResourceContext& context, VkFormat color_format, uint32_t width, uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_ready; }
        uint32_t getMipCount() const { return static_cast<uint32_t>(m_downsample_images.size()); }
        VkFormat getColorFormat() const { return m_color_format; }
        VkExtent2D getExtent() const { return m_extent; }
        VkSampler getSampler() const { return m_sampler; }

        const VulkanBloomImageView& getDownsampleImage(uint32_t index) const;
        const VulkanBloomImageView& getUpsampleImage(uint32_t index) const;
        const VulkanBloomImageView& getCompositeImage() const { return m_composite_image.view; }

        VulkanBloomRenderTarget getDownsampleRenderTarget(uint32_t index) const;
        VulkanBloomRenderTarget getUpsampleRenderTarget(uint32_t index) const;
        VulkanBloomRenderTarget getCompositeRenderTarget() const;

        void setDownsampleLayout(uint32_t index, VkImageLayout layout);
        void setUpsampleLayout(uint32_t index, VkImageLayout layout);
        void setCompositeLayout(VkImageLayout layout) { m_composite_image.view.layout = layout; }

    private:
        struct OwnedImage {
            VulkanBloomImageView view;
            VkDeviceMemory memory = VK_NULL_HANDLE;
        };

        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(uint32_t width, uint32_t height, OwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();
        uint32_t computeMipCount(uint32_t width, uint32_t height) const;

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        std::vector<OwnedImage> m_downsample_images;
        std::vector<OwnedImage> m_upsample_images;
        OwnedImage m_composite_image;
        VkSampler m_sampler = VK_NULL_HANDLE;
        bool m_ready = false;
    };
} // namespace NexAur
