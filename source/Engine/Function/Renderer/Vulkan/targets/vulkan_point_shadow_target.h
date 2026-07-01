#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class NEXAUR_API VulkanPointShadowTarget {
    public:
        VulkanPointShadowTarget() = default;
        ~VulkanPointShadowTarget();

        VulkanPointShadowTarget(const VulkanPointShadowTarget&) = delete;
        VulkanPointShadowTarget& operator=(const VulkanPointShadowTarget&) = delete;

        bool init(const VulkanResourceContext& context, uint32_t resolution, uint32_t shadowed_light_capacity);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        uint32_t getShadowedLightCapacity() const { return m_shadowed_light_capacity; }
        uint32_t getLayerCount() const { return m_layer_count; }
        VkFormat getDepthFormat() const { return m_depth_format; }
        VkImage getDepthImage() const { return m_depth_image; }
        VkImageView getDepthImageView() const { return m_depth_image_view; }
        VkSampler getSampler() const { return m_sampler; }

        VkImageLayout getDepthLayout() const { return m_depth_layout; }
        void setDepthLayout(VkImageLayout layout) { m_depth_layout = layout; }

        VulkanDepthRenderTarget getFaceRenderTarget(uint32_t shadow_slot, uint32_t face_index) const;
        VulkanDepthRenderTarget getRenderTarget(uint32_t layer_index) const;

    private:
        bool createImage(uint32_t resolution, uint32_t shadowed_light_capacity);
        bool createSampler();
        void cleanupImage();
        void cleanupSampler();

        uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
        VkFormat findDepthFormat() const;

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};
        uint32_t m_shadowed_light_capacity = 1;
        uint32_t m_layer_count = 6;

        VkImage m_depth_image = VK_NULL_HANDLE;
        VkDeviceMemory m_depth_memory = VK_NULL_HANDLE;
        VkImageView m_depth_image_view = VK_NULL_HANDLE;
        std::vector<VkImageView> m_depth_layer_views;
        VkImageLayout m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkSampler m_sampler = VK_NULL_HANDLE;
        bool m_ready = false;
    };
} // namespace NexAur
