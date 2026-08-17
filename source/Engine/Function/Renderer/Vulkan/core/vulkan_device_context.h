#pragma once

#ifdef NX_PLATFORM_WINDOWS
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef VK_USE_PLATFORM_WIN32_KHR
        #define VK_USE_PLATFORM_WIN32_KHR
    #endif
#endif

#include <cstdint>
#include <vector>

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_capabilities.h"

struct GLFWwindow;

namespace NexAur {
    class VulkanDeviceContext final {
    public:
        VulkanDeviceContext() = default;
        ~VulkanDeviceContext();

        VulkanDeviceContext(const VulkanDeviceContext&) = delete;
        VulkanDeviceContext& operator=(const VulkanDeviceContext&) = delete;

        bool init(
            GLFWwindow* window,
            const std::vector<const char*>& required_extensions,
            const VulkanRayTracingOptions& ray_tracing_options = {});
        void shutdown();

        bool isReady() const { return m_device.device != VK_NULL_HANDLE; }

        VkInstance getInstance() const { return m_instance.instance; }
        VkSurfaceKHR getSurface() const { return m_surface; }
        VkPhysicalDevice getPhysicalDevice() const { return m_physical_device.physical_device; }
        VkDevice getDevice() const { return m_device.device; }
        VkQueue getGraphicsQueue() const { return m_graphics_queue; }
        VkQueue getPresentQueue() const { return m_present_queue; }
        uint32_t getGraphicsQueueFamily() const { return m_graphics_queue_family; }
        uint32_t getApiVersion() const { return m_device_api_version; }
        const VulkanRayTracingCapabilities& getRayTracingCapabilities() const {
            return m_ray_tracing_capabilities;
        }

        vkb::Instance& getInstanceBundle() { return m_instance; }
        const vkb::Instance& getInstanceBundle() const { return m_instance; }
        VkSurfaceKHR& getSurfaceHandle() { return m_surface; }
        const VkSurfaceKHR& getSurfaceHandle() const { return m_surface; }
        vkb::PhysicalDevice& getPhysicalDeviceBundle() { return m_physical_device; }
        const vkb::PhysicalDevice& getPhysicalDeviceBundle() const { return m_physical_device; }
        vkb::Device& getDeviceBundle() { return m_device; }
        const vkb::Device& getDeviceBundle() const { return m_device; }
        VkQueue& getGraphicsQueueHandle() { return m_graphics_queue; }
        const VkQueue& getGraphicsQueueHandle() const { return m_graphics_queue; }
        VkQueue& getPresentQueueHandle() { return m_present_queue; }
        const VkQueue& getPresentQueueHandle() const { return m_present_queue; }
        uint32_t& getGraphicsQueueFamilyValue() { return m_graphics_queue_family; }
        const uint32_t& getGraphicsQueueFamilyValue() const { return m_graphics_queue_family; }
        uint32_t& getApiVersionValue() { return m_device_api_version; }
        const uint32_t& getApiVersionValue() const { return m_device_api_version; }

    private:
        bool createInstance(const std::vector<const char*>& required_extensions);
        bool createSurface();
        bool createDevice(const VulkanRayTracingOptions& ray_tracing_options);
        void cachePhysicalDeviceProperties();
        bool buildRequiredVulkan13Features(VkPhysicalDeviceVulkan13Features& enabled_features) const;

        GLFWwindow* m_window = nullptr;
        vkb::Instance m_instance;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        vkb::PhysicalDevice m_physical_device;
        vkb::Device m_device;
        VkQueue m_graphics_queue = VK_NULL_HANDLE;
        VkQueue m_present_queue = VK_NULL_HANDLE;
        uint32_t m_graphics_queue_family = 0;
        uint32_t m_device_api_version = VK_API_VERSION_1_3;
        VulkanRayTracingCapabilities m_ray_tracing_capabilities;
    };
} // namespace NexAur
