#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanSceneColorTarget {
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
        VkImage getColorImage() const { return m_color_image.getImage(); }
        VkImageView getColorImageView() const { return m_color_image.getImageView(); }
        VkSampler getSampler() const { return m_sampler.get(); }

        VkImageLayout getColorLayout() const { return m_color_image.getLayout(); }
        void setColorLayout(VkImageLayout layout) { m_color_image.setLayout(layout); }

    private:
        bool recreateImage(uint32_t width, uint32_t height);
        bool createImage(uint32_t width, uint32_t height);
        bool createSampler();
        void cleanupImage();
        void cleanupSampler();

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        VulkanOwnedImage m_color_image;
        VulkanOwnedSampler m_sampler;
        bool m_ready = false;
    };
} // namespace NexAur
