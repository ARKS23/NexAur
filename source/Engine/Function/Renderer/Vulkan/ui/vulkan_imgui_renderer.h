#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"

namespace NexAur {
    struct VulkanImGuiRendererContext {
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphics_queue = VK_NULL_HANDLE;
        uint32_t graphics_queue_family = 0;
        VkFormat swapchain_color_format = VK_FORMAT_UNDEFINED;
        uint32_t min_image_count = 2;
        uint32_t image_count = 2;
        uint32_t api_version = VK_API_VERSION_1_3;

        bool valid() const {
            return instance != VK_NULL_HANDLE &&
                   physical_device != VK_NULL_HANDLE &&
                   device != VK_NULL_HANDLE &&
                   graphics_queue != VK_NULL_HANDLE &&
                   swapchain_color_format != VK_FORMAT_UNDEFINED &&
                   image_count >= min_image_count &&
                   min_image_count >= 2 &&
                   api_version >= VK_API_VERSION_1_3;
        }
    };

    class NEXAUR_API VulkanImGuiRenderer {
    public:
        VulkanImGuiRenderer() = default;
        ~VulkanImGuiRenderer();

        VulkanImGuiRenderer(const VulkanImGuiRenderer&) = delete;
        VulkanImGuiRenderer& operator=(const VulkanImGuiRenderer&) = delete;

        bool init(const VulkanImGuiRendererContext& context);
        void shutdown();

        void beginFrame();
        void renderDrawData(VkCommandBuffer command_buffer);
        void onSwapchainRecreated(uint32_t image_count);

        bool registerViewportTexture(VkImageView image_view, VkImageLayout image_layout);
        void releaseViewportTexture();
        void* getViewportTextureHandle() const;
        bool hasViewportTexture() const { return m_viewport_descriptor != VK_NULL_HANDLE; }
        bool isInitialized() const { return m_initialized; }

    private:
        VkFormat m_swapchain_color_format = VK_FORMAT_UNDEFINED;
        uint32_t m_min_image_count = 2;
        bool m_initialized = false;
        VkDescriptorSet m_viewport_descriptor = VK_NULL_HANDLE;
    };
} // namespace NexAur
