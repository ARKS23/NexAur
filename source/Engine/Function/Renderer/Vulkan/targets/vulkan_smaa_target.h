#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    struct VulkanSmaaImageView {
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

    class NEXAUR_API VulkanSmaaTarget {
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
        VkSampler getSampler() const { return m_sampler; }

        const VulkanSmaaImageView& getSourceImage() const { return m_source_image.view; }
        const VulkanSmaaImageView& getEdgeImage() const { return m_edge_image.view; }
        const VulkanSmaaImageView& getBlendImage() const { return m_blend_image.view; }

        VulkanSmaaRenderTarget getSourceRenderTarget() const;
        VulkanSmaaRenderTarget getEdgeRenderTarget() const;
        VulkanSmaaRenderTarget getBlendRenderTarget() const;

        void setSourceLayout(VkImageLayout layout) { m_source_image.view.layout = layout; }
        void setEdgeLayout(VkImageLayout layout) { m_edge_image.view.layout = layout; }
        void setBlendLayout(VkImageLayout layout) { m_blend_image.view.layout = layout; }

    private:
        struct OwnedImage {
            VulkanSmaaImageView view;
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
        VkFormat m_source_format = VK_FORMAT_UNDEFINED;
        VkFormat m_mask_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        OwnedImage m_source_image;
        OwnedImage m_edge_image;
        OwnedImage m_blend_image;
        VkSampler m_sampler = VK_NULL_HANDLE;
        bool m_ready = false;
    };
} // namespace NexAur
