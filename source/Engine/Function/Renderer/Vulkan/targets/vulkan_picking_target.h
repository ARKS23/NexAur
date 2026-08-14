#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanPickingTarget {
    public:
        VulkanPickingTarget() = default;
        ~VulkanPickingTarget();

        VulkanPickingTarget(const VulkanPickingTarget&) = delete;
        VulkanPickingTarget& operator=(const VulkanPickingTarget&) = delete;

        bool init(const VulkanResourceContext& context, uint32_t width, uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFormat getObjectIdFormat() const { return m_object_id_format; }
        VkFormat getDepthFormat() const { return m_depth_format; }

        VkImage getObjectIdImage() const { return m_object_id_image.getImage(); }
        VkImageView getObjectIdImageView() const { return m_object_id_image.getImageView(); }
        VkImage getDepthImage() const { return m_depth_image.getImage(); }
        VkImageView getDepthImageView() const { return m_depth_image.getImageView(); }

        VkImageLayout getObjectIdLayout() const { return m_object_id_image.getLayout(); }
        VkImageLayout getDepthLayout() const { return m_depth_image.getLayout(); }
        void setObjectIdLayout(VkImageLayout layout) { m_object_id_image.setLayout(layout); }
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
        void cleanupImages();

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkExtent2D m_extent{};
        VkFormat m_object_id_format = VK_FORMAT_R32_SINT;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;

        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VulkanOwnedImage m_object_id_image;

        VulkanOwnedImage m_depth_image;

        bool m_ready = false;
    };
} // namespace NexAur
