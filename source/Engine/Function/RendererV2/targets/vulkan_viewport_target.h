#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/RendererV2/vulkan_render_target.h"
#include "Function/RendererV2/vulkan_resource_context.h"

namespace NexAur {
    class NEXAUR_API VulkanViewportTarget {
    public:
        VulkanViewportTarget() = default;
        ~VulkanViewportTarget();

        VulkanViewportTarget(const VulkanViewportTarget&) = delete;
        VulkanViewportTarget& operator=(const VulkanViewportTarget&) = delete;

        bool init(const VulkanResourceContext& context, VkFormat color_format, uint32_t width, uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFormat getColorFormat() const { return m_color_format; }
        VkFormat getDepthFormat() const { return m_depth_format; }
        VkImage getColorImage() const { return m_color_image; }
        VkImage getDepthImage() const { return m_depth_image; }
        VkImageView getColorImageView() const { return m_color_image_view; }
        VkSampler getSampler() const { return m_sampler; }

        VkImageLayout getColorLayout() const { return m_color_layout; }
        VkImageLayout getDepthLayout() const { return m_depth_layout; }
        void setColorLayout(VkImageLayout layout) { m_color_layout = layout; }
        void setDepthLayout(VkImageLayout layout) { m_depth_layout = layout; }

        VulkanRenderTarget getRenderTarget() const;

    private:
        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageUsageFlags usage,
            VkImageAspectFlags aspect,
            VkImage& image,
            VkDeviceMemory& memory,
            VkImageView& image_view);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        VkImage m_color_image = VK_NULL_HANDLE;
        VkDeviceMemory m_color_memory = VK_NULL_HANDLE;
        VkImageView m_color_image_view = VK_NULL_HANDLE;
        VkImageLayout m_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage m_depth_image = VK_NULL_HANDLE;
        VkDeviceMemory m_depth_memory = VK_NULL_HANDLE;
        VkImageView m_depth_image_view = VK_NULL_HANDLE;
        VkImageLayout m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkSampler m_sampler = VK_NULL_HANDLE;
        bool m_ready = false;
    };
} // namespace NexAur
