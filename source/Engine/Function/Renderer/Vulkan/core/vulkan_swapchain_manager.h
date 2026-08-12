#pragma once

#include <cstdint>
#include <vector>

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

namespace NexAur {
    class VulkanDeviceContext;

    class VulkanSwapchainManager final {
    public:
        VulkanSwapchainManager() = default;
        ~VulkanSwapchainManager();

        VulkanSwapchainManager(const VulkanSwapchainManager&) = delete;
        VulkanSwapchainManager& operator=(const VulkanSwapchainManager&) = delete;

        bool create(const VulkanDeviceContext& device_context);
        void shutdown();

        void setSurfaceSize(uint32_t width, uint32_t height);
        void markDirty() { m_dirty = true; }

        bool isDirty() const { return m_dirty; }
        bool isReady() const { return m_swapchain.swapchain != VK_NULL_HANDLE && !m_images.empty(); }
        uint32_t getSurfaceWidth() const { return m_surface_width; }
        uint32_t getSurfaceHeight() const { return m_surface_height; }
        VkExtent2D getExtent() const { return m_swapchain.extent; }
        VkFormat getImageFormat() const { return m_swapchain.image_format; }
        uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }
        const std::vector<VkImage>& getImages() const { return m_images; }
        std::vector<VkImage>& getImagesValue() { return m_images; }
        std::vector<VkImageLayout>& getImageLayoutsValue() { return m_image_layouts; }
        uint32_t& getSurfaceWidthValue() { return m_surface_width; }
        uint32_t& getSurfaceHeightValue() { return m_surface_height; }
        bool& getDirtyValue() { return m_dirty; }
        VkImageLayout getImageLayout(uint32_t image_index) const;
        void setImageLayout(uint32_t image_index, VkImageLayout layout);
        vkb::Swapchain& getSwapchain() { return m_swapchain; }
        const vkb::Swapchain& getSwapchain() const { return m_swapchain; }

    private:
        uint32_t m_surface_width = 1280;
        uint32_t m_surface_height = 720;
        bool m_dirty = false;
        vkb::Swapchain m_swapchain;
        std::vector<VkImage> m_images;
        std::vector<VkImageLayout> m_image_layouts;
    };
} // namespace NexAur
