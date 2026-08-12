#include "pch.h"
#include "Function/Renderer/Vulkan/core/vulkan_device_context.h"

#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

#ifdef NX_PLATFORM_WINDOWS
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #include <vulkan/vulkan_win32.h>
#endif

namespace NexAur {
    VulkanDeviceContext::~VulkanDeviceContext() {
        shutdown();
    }

    bool VulkanDeviceContext::init(
        GLFWwindow* window,
        const std::vector<const char*>& required_extensions) {
        if (isReady()) {
            return true;
        }

        m_window = window;
        if (!m_window) {
            NX_CORE_ERROR("VulkanDeviceContext failed to initialize: native window is null.");
            return false;
        }

        if (!createInstance(required_extensions) || !createSurface() || !createDevice()) {
            shutdown();
            return false;
        }

        return true;
    }

    void VulkanDeviceContext::shutdown() {
        if (m_device.device != VK_NULL_HANDLE) {
            vkb::destroy_device(m_device);
            m_device = {};
        }

        if (m_surface != VK_NULL_HANDLE && m_instance.instance != VK_NULL_HANDLE) {
            vkb::destroy_surface(m_instance, m_surface);
            m_surface = VK_NULL_HANDLE;
        }

        if (m_instance.instance != VK_NULL_HANDLE) {
            vkb::destroy_instance(m_instance);
            m_instance = {};
        }

        m_window = nullptr;
        m_physical_device = {};
        m_graphics_queue = VK_NULL_HANDLE;
        m_present_queue = VK_NULL_HANDLE;
        m_graphics_queue_family = 0;
        m_device_api_version = VK_API_VERSION_1_3;
    }

    bool VulkanDeviceContext::createInstance(const std::vector<const char*>& required_extensions) {
        if (required_extensions.empty()) {
            NX_CORE_ERROR("VulkanDeviceContext failed to initialize: no Vulkan instance extensions were provided.");
            return false;
        }

        vkb::InstanceBuilder builder;
        builder
            .set_app_name("NexAur")
            .set_engine_name("NexAur")
            .require_api_version(1, 3, 0)
            .set_headless(true)
            .enable_extensions(required_extensions);

#if !defined(NDEBUG)
        bool validation_layers_available = false;
        auto system_info_result = vkb::SystemInfo::get_system_info();
        if (system_info_result) {
            validation_layers_available = system_info_result.value().validation_layers_available;
        } else {
            NX_CORE_WARN(
                "Vulkan validation availability query failed: {} ({}).",
                system_info_result.error().message(),
                VulkanDiagnosticsCollector::vkResultToString(system_info_result.vk_result()));
        }

        if (!validation_layers_available) {
            NX_CORE_WARN(
                "Vulkan validation was requested for this Debug build, but the standard validation layer is unavailable.");
        }

        builder.request_validation_layers().use_default_debug_messenger();
#endif

        auto instance_result = builder.build();
        if (!instance_result) {
            VulkanDiagnosticsCollector::logVkbFailure("Vulkan instance creation", instance_result);
            return false;
        }

        m_instance = instance_result.value();
#if !defined(NDEBUG)
        if (validation_layers_available && m_instance.debug_messenger != VK_NULL_HANDLE) {
            NX_CORE_INFO("Vulkan validation enabled for Debug build; debug messenger active.");
        } else if (validation_layers_available) {
            NX_CORE_WARN("Vulkan validation layer is available, but the debug messenger is inactive.");
        }
#endif
        return true;
    }

    bool VulkanDeviceContext::createSurface() {
#ifdef NX_PLATFORM_WINDOWS
        HWND hwnd = glfwGetWin32Window(m_window);
        if (!hwnd) {
            NX_CORE_ERROR("Vulkan surface creation failed: GLFW returned null Win32 window handle.");
            return false;
        }

        VkWin32SurfaceCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        create_info.hinstance = GetModuleHandle(nullptr);
        create_info.hwnd = hwnd;

        return VulkanDiagnosticsCollector::checkVk(
            vkCreateWin32SurfaceKHR(m_instance.instance, &create_info, nullptr, &m_surface),
            "vkCreateWin32SurfaceKHR");
#else
        NX_CORE_ERROR("Vulkan surface creation is currently implemented only for Windows.");
        return false;
#endif
    }

    bool VulkanDeviceContext::createDevice() {
        auto physical_device_result = vkb::PhysicalDeviceSelector(m_instance)
            .set_surface(m_surface)
            .require_present(true)
            .set_minimum_version(1, 3)
            .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
            .select();

        if (!physical_device_result) {
            VulkanDiagnosticsCollector::logVkbFailure(
                "Vulkan physical device selection",
                physical_device_result);
            return false;
        }

        m_physical_device = physical_device_result.value();
        cachePhysicalDeviceProperties();

        VkPhysicalDeviceFeatures optional_core_features{};
        optional_core_features.samplerAnisotropy = VK_TRUE;
        if (!m_physical_device.enable_features_if_present(optional_core_features)) {
            NX_CORE_WARN(
                "Vulkan samplerAnisotropy is not available; material textures will use trilinear mip sampling.");
        }

        VkPhysicalDeviceVulkan13Features vulkan13_features{};
        if (!buildRequiredVulkan13Features(vulkan13_features)) {
            return false;
        }

        auto device_result = vkb::DeviceBuilder(m_physical_device)
            .add_pNext(&vulkan13_features)
            .build();
        if (!device_result) {
            VulkanDiagnosticsCollector::logVkbFailure("Vulkan logical device creation", device_result);
            return false;
        }

        m_device = device_result.value();

        auto graphics_queue_result = m_device.get_queue(vkb::QueueType::graphics);
        if (!graphics_queue_result) {
            VulkanDiagnosticsCollector::logVkbFailure("Vulkan graphics queue lookup", graphics_queue_result);
            return false;
        }
        m_graphics_queue = graphics_queue_result.value();

        auto present_queue_result = m_device.get_queue(vkb::QueueType::present);
        if (!present_queue_result) {
            VulkanDiagnosticsCollector::logVkbFailure("Vulkan present queue lookup", present_queue_result);
            return false;
        }
        m_present_queue = present_queue_result.value();

        auto graphics_queue_index_result = m_device.get_queue_index(vkb::QueueType::graphics);
        if (!graphics_queue_index_result) {
            VulkanDiagnosticsCollector::logVkbFailure(
                "Vulkan graphics queue family lookup",
                graphics_queue_index_result);
            return false;
        }
        m_graphics_queue_family = graphics_queue_index_result.value();
        return true;
    }

    void VulkanDeviceContext::cachePhysicalDeviceProperties() {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_physical_device.physical_device, &properties);
        m_device_api_version = properties.apiVersion;
    }

    bool VulkanDeviceContext::buildRequiredVulkan13Features(
        VkPhysicalDeviceVulkan13Features& enabled_features) const {
        VkPhysicalDeviceVulkan13Features supported_features{};
        supported_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceFeatures2 supported_features2{};
        supported_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        supported_features2.pNext = &supported_features;
        vkGetPhysicalDeviceFeatures2(m_physical_device.physical_device, &supported_features2);

        if (!VulkanDiagnosticsCollector::requireFeature(
                supported_features.dynamicRendering,
                "dynamicRendering") ||
            !VulkanDiagnosticsCollector::requireFeature(
                supported_features.synchronization2,
                "synchronization2") ||
            !VulkanDiagnosticsCollector::requireFeature(
                supported_features.shaderDemoteToHelperInvocation,
                "shaderDemoteToHelperInvocation")) {
            return false;
        }

        enabled_features = {};
        enabled_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        enabled_features.dynamicRendering = VK_TRUE;
        enabled_features.synchronization2 = VK_TRUE;
        enabled_features.shaderDemoteToHelperInvocation = VK_TRUE;
        return true;
    }
} // namespace NexAur
