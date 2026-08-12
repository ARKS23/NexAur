#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    using VulkanBloomImageView = VulkanImageViewState;

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
        VkSampler getSampler() const { return m_sampler.get(); }

        const VulkanBloomImageView& getDownsampleImage(uint32_t index) const;
        const VulkanBloomImageView& getUpsampleImage(uint32_t index) const;
        const VulkanBloomImageView& getCompositeImage() const { return m_composite_image.getView(); }

        VulkanBloomRenderTarget getDownsampleRenderTarget(uint32_t index) const;
        VulkanBloomRenderTarget getUpsampleRenderTarget(uint32_t index) const;
        VulkanBloomRenderTarget getCompositeRenderTarget() const;

        void setDownsampleLayout(uint32_t index, VkImageLayout layout);
        void setUpsampleLayout(uint32_t index, VkImageLayout layout);
        void setCompositeLayout(VkImageLayout layout) { m_composite_image.setLayout(layout); }

    private:
        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(uint32_t width, uint32_t height, VulkanOwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();
        uint32_t computeMipCount(uint32_t width, uint32_t height) const;

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        std::vector<VulkanOwnedImage> m_downsample_images;
        std::vector<VulkanOwnedImage> m_upsample_images;
        VulkanOwnedImage m_composite_image;
        VulkanOwnedSampler m_sampler;
        bool m_ready = false;
    };
} // namespace NexAur
