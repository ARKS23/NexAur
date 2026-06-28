#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class NEXAUR_API VulkanSceneColorTarget {
    public:
        VulkanSceneColorTarget() = default;
        ~VulkanSceneColorTarget();

        VulkanSceneColorTarget(const VulkanSceneColorTarget&) = delete;
        VulkanSceneColorTarget& operator=(const VulkanSceneColorTarget&) = delete;

        bool init(const VulkanResourceContext& context, VkFormat color_format, uint32_t width, uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFormat getColorFormat() const { return m_color_format; }
        VkImage getColorImage() const { return m_color_image; }
        VkImageView getColorImageView() const { return m_color_image_view; }
        VkSampler getSampler() const { return m_sampler; }

        VkImageLayout getColorLayout() const { return m_color_layout; }
        void setColorLayout(VkImageLayout layout) { m_color_layout = layout; }

    private:
        bool recreateImage(uint32_t width, uint32_t height);
        bool createImage(uint32_t width, uint32_t height);
        bool createSampler();
        void cleanupImage();
        void cleanupSampler();

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        VkImage m_color_image = VK_NULL_HANDLE;
        VkDeviceMemory m_color_memory = VK_NULL_HANDLE;
        VkImageView m_color_image_view = VK_NULL_HANDLE;
        VkImageLayout m_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkSampler m_sampler = VK_NULL_HANDLE;
        bool m_ready = false;
    };
} // namespace NexAur
