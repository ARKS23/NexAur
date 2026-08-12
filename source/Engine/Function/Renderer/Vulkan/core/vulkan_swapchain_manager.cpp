#include "pch.h"
#include "Function/Renderer/Vulkan/core/vulkan_swapchain_manager.h"

#include "Function/Renderer/Vulkan/core/vulkan_device_context.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

namespace NexAur {
    VulkanSwapchainManager::~VulkanSwapchainManager() {
        shutdown();
    }

    bool VulkanSwapchainManager::create(const VulkanDeviceContext& device_context) {
        if (m_surface_width == 0 || m_surface_height == 0 || !device_context.isReady()) {
            return false;
        }

        VkSurfaceFormatKHR preferred_format{};
        preferred_format.format = VK_FORMAT_B8G8R8A8_SRGB;
        preferred_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        VkSurfaceFormatKHR fallback_format{};
        fallback_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        fallback_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        auto swapchain_result = vkb::SwapchainBuilder(
                device_context.getDeviceBundle(),
                device_context.getSurface())
            .set_desired_extent(m_surface_width, m_surface_height)
            .set_desired_format(preferred_format)
            .add_fallback_format(fallback_format)
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build();

        if (!swapchain_result) {
            VulkanDiagnosticsCollector::logVkbFailure("Vulkan swapchain creation", swapchain_result);
            return false;
        }

        m_swapchain = swapchain_result.value();
        auto images_result = m_swapchain.get_images();
        if (!images_result) {
            VulkanDiagnosticsCollector::logVkbFailure("Vulkan swapchain image query", images_result);
            vkb::destroy_swapchain(m_swapchain);
            m_swapchain = {};
            return false;
        }

        m_images = images_result.value();
        m_image_layouts.assign(m_images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
        m_dirty = false;
        return true;
    }

    void VulkanSwapchainManager::shutdown() {
        m_images.clear();
        m_image_layouts.clear();

        if (m_swapchain.swapchain != VK_NULL_HANDLE) {
            vkb::destroy_swapchain(m_swapchain);
            m_swapchain = {};
        }
    }

    void VulkanSwapchainManager::setSurfaceSize(uint32_t width, uint32_t height) {
        m_surface_width = width;
        m_surface_height = height;
        if (width == 0 || height == 0) {
            return;
        }

        if (isReady()) {
            m_dirty = true;
        }
    }

    VkImageLayout VulkanSwapchainManager::getImageLayout(uint32_t image_index) const {
        if (image_index >= m_image_layouts.size()) {
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }
        return m_image_layouts[image_index];
    }

    void VulkanSwapchainManager::setImageLayout(uint32_t image_index, VkImageLayout layout) {
        if (image_index < m_image_layouts.size()) {
            m_image_layouts[image_index] = layout;
        }
    }
} // namespace NexAur
