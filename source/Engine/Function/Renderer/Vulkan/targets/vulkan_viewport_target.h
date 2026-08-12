#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

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
        VkImage getColorImage() const { return m_color_image.getImage(); }
        VkImage getDepthImage() const { return m_depth_image.getImage(); }
        VkImageView getColorImageView() const { return m_color_image.getImageView(); }
        VkSampler getSampler() const { return m_sampler.get(); }

        VkImageLayout getColorLayout() const { return m_color_image.getLayout(); }
        VkImageLayout getDepthLayout() const { return m_depth_image.getLayout(); }
        void setColorLayout(VkImageLayout layout) { m_color_image.setLayout(layout); }
        void setDepthLayout(VkImageLayout layout) { m_depth_image.setLayout(layout); }

        VulkanRenderTarget getRenderTarget() const;

    private:
        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageUsageFlags usage,
            VkImageAspectFlags aspect,
            VulkanOwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        VulkanOwnedImage m_color_image;
        VulkanOwnedImage m_depth_image;

        VulkanOwnedSampler m_sampler;
        bool m_ready = false;
    };
} // namespace NexAur
