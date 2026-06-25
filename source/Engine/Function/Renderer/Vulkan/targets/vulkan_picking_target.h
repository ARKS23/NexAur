#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class NEXAUR_API VulkanPickingTarget {
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

        VkImage getObjectIdImage() const { return m_object_id_image; }
        VkImageView getObjectIdImageView() const { return m_object_id_image_view; }
        VkImage getDepthImage() const { return m_depth_image; }
        VkImageView getDepthImageView() const { return m_depth_image_view; }

        VkImageLayout getObjectIdLayout() const { return m_object_id_layout; }
        VkImageLayout getDepthLayout() const { return m_depth_layout; }
        void setObjectIdLayout(VkImageLayout layout) { m_object_id_layout = layout; }
        void setDepthLayout(VkImageLayout layout) { m_depth_layout = layout; }

        VkBuffer getReadbackBuffer() const { return m_readback_buffer; }
        int32_t readbackEntityId() const;

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
        bool createReadbackBuffer();
        void cleanupImages();
        void cleanupReadbackBuffer();

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkExtent2D m_extent{};
        VkFormat m_object_id_format = VK_FORMAT_R32_SINT;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;

        VkImage m_object_id_image = VK_NULL_HANDLE;
        VkDeviceMemory m_object_id_memory = VK_NULL_HANDLE;
        VkImageView m_object_id_image_view = VK_NULL_HANDLE;
        VkImageLayout m_object_id_layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage m_depth_image = VK_NULL_HANDLE;
        VkDeviceMemory m_depth_memory = VK_NULL_HANDLE;
        VkImageView m_depth_image_view = VK_NULL_HANDLE;
        VkImageLayout m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkBuffer m_readback_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_readback_memory = VK_NULL_HANDLE;

        bool m_ready = false;
    };
} // namespace NexAur
