#include "pch.h"
#include "vulkan_renderer_system.h"

#include "Core/Events/window_event.h"
#include "Function/Platform/platform_services.h"
#include "Function/Resource/asset_manager.h"
#include "Function/RendererV2/render_scene_frame_builder.h"
#include "Function/RendererV2/passes/vulkan_forward_pass.h"
#include "Function/RendererV2/vulkan_draw_list_builder.h"
#include "Function/RendererV2/vulkan_render_data_translator.h"
#include "Function/RendererV2/vulkan_render_resource_cache.h"

#ifdef NX_PLATFORM_WINDOWS
    #define VK_USE_PLATFORM_WIN32_KHR
    #define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <VkBootstrap.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#ifdef NX_PLATFORM_WINDOWS
    #include <vulkan/vulkan_win32.h>
#endif

#include <algorithm>

namespace NexAur {
    namespace {
        constexpr uint32_t kDefaultViewportWidth = 1280;
        constexpr uint32_t kDefaultViewportHeight = 720;
        constexpr uint32_t kRequiredVulkanApiVersion = VK_API_VERSION_1_3;

        const char* vkResultToString(VkResult result) {
            switch (result) {
            case VK_SUCCESS:
                return "VK_SUCCESS";
            case VK_NOT_READY:
                return "VK_NOT_READY";
            case VK_TIMEOUT:
                return "VK_TIMEOUT";
            case VK_EVENT_SET:
                return "VK_EVENT_SET";
            case VK_EVENT_RESET:
                return "VK_EVENT_RESET";
            case VK_INCOMPLETE:
                return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED:
                return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST:
                return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED:
                return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT:
                return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT:
                return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER:
                return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS:
                return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED:
                return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_SURFACE_LOST_KHR:
                return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
                return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR:
                return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR:
                return "VK_ERROR_OUT_OF_DATE_KHR";
            default:
                return "Unknown VkResult";
            }
        }

        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {} ({})", operation, vkResultToString(result), static_cast<int>(result));
            return false;
        }

        template<typename T>
        void logVkbFailure(const char* operation, const vkb::Result<T>& result) {
            if (result) {
                return;
            }

            NX_CORE_ERROR("{} failed: {} ({})", operation, result.error().message(), vkResultToString(result.vk_result()));
            for (const std::string& reason : result.detailed_failure_reasons()) {
                NX_CORE_ERROR("{} detail: {}", operation, reason);
            }
        }

        bool requireFeature(VkBool32 supported, const char* feature_name) {
            if (supported == VK_TRUE) {
                return true;
            }

            NX_CORE_ERROR("Vulkan 1.3 feature is required but not supported: {}", feature_name);
            return false;
        }
    } // namespace

    struct VulkanRendererSystem::Backend {
        bool init(WindowService& service) {
            if (initialized) {
                return true;
            }

            if (service.getGraphicsAPI() != WindowGraphicsAPI::Vulkan) {
                NX_CORE_ERROR("VulkanRendererSystem requires a Vulkan no-api window.");
                return false;
            }

            window = static_cast<GLFWwindow*>(service.getNativeWindow());
            if (!window) {
                NX_CORE_ERROR("VulkanRendererSystem failed to initialize: WindowService returned null native window.");
                return false;
            }

            auto [window_width, window_height] = service.getSize();
            surface_width = std::max(1u, window_width);
            surface_height = std::max(1u, window_height);

            if (!createInstance(service.getRequiredVulkanInstanceExtensions()) ||
                !createSurface() ||
                !createDevice() ||
                !createSwapchain() ||
                !createCommandResources() ||
                !createSyncObjects()) {
                shutdown();
                return false;
            }

            if (!resource_cache.init(createResourceContext())) {
                shutdown();
                return false;
            }

            initialized = true;
            NX_CORE_INFO(
                "VulkanRendererSystem initialized: surface {}x{}, swapchain {} images, API {}.{}.{}.",
                swapchain.extent.width,
                swapchain.extent.height,
                swapchain.image_count,
                VK_API_VERSION_MAJOR(device_api_version),
                VK_API_VERSION_MINOR(device_api_version),
                VK_API_VERSION_PATCH(device_api_version));
            return true;
        }

        void shutdown() {
            if (device.device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device.device);
            }

            resource_cache.shutdown();
            forward_pass.shutdown();
            cleanupSyncObjects();
            cleanupCommandResources();
            cleanupSwapchain();

            if (device.device != VK_NULL_HANDLE) {
                vkb::destroy_device(device);
                device = {};
            }

            if (surface != VK_NULL_HANDLE && instance.instance != VK_NULL_HANDLE) {
                vkb::destroy_surface(instance, surface);
                surface = VK_NULL_HANDLE;
            }

            if (instance.instance != VK_NULL_HANDLE) {
                vkb::destroy_instance(instance);
                instance = {};
            }

            physical_device = {};
            graphics_queue = VK_NULL_HANDLE;
            present_queue = VK_NULL_HANDLE;
            device_api_version = kRequiredVulkanApiVersion;
            initialized = false;
        }

        void render(TimeStep ts, const RenderDataPacket& render_data) {
            (void)ts;

            if (!initialized || surface_width == 0 || surface_height == 0) {
                return;
            }

            if (swapchain_dirty && !recreateSwapchain()) {
                return;
            }

            auto [render_width, render_height] = getRenderExtent();
            translator.resetFrame();
            const RenderView render_view = translator.buildRenderView(render_data, render_width, render_height);
            const RenderSceneFrame scene_frame = scene_frame_builder.buildRenderSceneFrame(render_data, render_view);
            const VulkanDrawList draw_list = draw_list_builder.buildDrawList(
                scene_frame,
                resource_cache,
                AssetManager::getInstance());

            drawFrame(draw_list);
        }

        void setViewportSize(uint32_t width, uint32_t height) {
            (void)width;
            (void)height;
        }

        std::pair<uint32_t, uint32_t> getViewportSize() const {
            return getRenderExtent();
        }

        ViewportOutput getViewportOutput() const {
            ViewportOutput output;
            output.backend = RendererBackendType::Vulkan;
            output.kind = initialized ? ViewportOutputKind::ExternalSwapchain : ViewportOutputKind::None;

            auto [width, height] = getRenderExtent();
            output.width = width;
            output.height = height;
            return output;
        }

        void resizeSurface(uint32_t width, uint32_t height) {
            surface_width = width;
            surface_height = height;

            if (width == 0 || height == 0) {
                return;
            }

            swapchain_dirty = initialized;
        }

    private:
        VulkanResourceContext createResourceContext() const {
            VulkanResourceContext context;
            context.instance = instance.instance;
            context.physical_device = physical_device.physical_device;
            context.device = device.device;
            context.graphics_queue = graphics_queue;
            context.graphics_queue_family = graphics_queue_family;
            context.api_version = device_api_version;
            return context;
        }

        std::pair<uint32_t, uint32_t> getRenderExtent() const {
            if (swapchain.extent.width > 0 && swapchain.extent.height > 0) {
                return { swapchain.extent.width, swapchain.extent.height };
            }

            return { std::max(1u, surface_width), std::max(1u, surface_height) };
        }

        bool createInstance(const std::vector<const char*>& required_extensions) {
            if (required_extensions.empty()) {
                NX_CORE_ERROR("VulkanRendererSystem failed to initialize: no Vulkan instance extensions were provided by WindowService.");
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
            builder.request_validation_layers().use_default_debug_messenger();
#endif

            auto instance_result = builder.build();
            if (!instance_result) {
                logVkbFailure("Vulkan instance creation", instance_result);
                return false;
            }

            instance = instance_result.value();
            return true;
        }

        bool createSurface() {
#ifdef NX_PLATFORM_WINDOWS
            HWND hwnd = glfwGetWin32Window(window);
            if (!hwnd) {
                NX_CORE_ERROR("Vulkan surface creation failed: GLFW returned null Win32 window handle.");
                return false;
            }

            VkWin32SurfaceCreateInfoKHR create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            create_info.hinstance = GetModuleHandle(nullptr);
            create_info.hwnd = hwnd;

            return checkVk(
                vkCreateWin32SurfaceKHR(instance.instance, &create_info, nullptr, &surface),
                "vkCreateWin32SurfaceKHR");
#else
            NX_CORE_ERROR("Vulkan surface creation is currently implemented only for Windows.");
            return false;
#endif
        }

        bool createDevice() {
            auto physical_device_result = vkb::PhysicalDeviceSelector(instance)
                .set_surface(surface)
                .require_present(true)
                .set_minimum_version(1, 3)
                .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
                .select();

            if (!physical_device_result) {
                logVkbFailure("Vulkan physical device selection", physical_device_result);
                return false;
            }

            physical_device = physical_device_result.value();
            cachePhysicalDeviceProperties();

            VkPhysicalDeviceVulkan13Features vulkan13_features{};
            if (!buildRequiredVulkan13Features(vulkan13_features)) {
                return false;
            }

            auto device_result = vkb::DeviceBuilder(physical_device)
                .add_pNext(&vulkan13_features)
                .build();
            if (!device_result) {
                logVkbFailure("Vulkan logical device creation", device_result);
                return false;
            }

            device = device_result.value();

            auto graphics_queue_result = device.get_queue(vkb::QueueType::graphics);
            if (!graphics_queue_result) {
                logVkbFailure("Vulkan graphics queue lookup", graphics_queue_result);
                return false;
            }
            graphics_queue = graphics_queue_result.value();

            auto present_queue_result = device.get_queue(vkb::QueueType::present);
            if (!present_queue_result) {
                logVkbFailure("Vulkan present queue lookup", present_queue_result);
                return false;
            }
            present_queue = present_queue_result.value();

            auto graphics_queue_index_result = device.get_queue_index(vkb::QueueType::graphics);
            if (!graphics_queue_index_result) {
                logVkbFailure("Vulkan graphics queue family lookup", graphics_queue_index_result);
                return false;
            }
            graphics_queue_family = graphics_queue_index_result.value();

            return true;
        }

        void cachePhysicalDeviceProperties() {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physical_device.physical_device, &properties);
            device_api_version = properties.apiVersion;
        }

        bool buildRequiredVulkan13Features(VkPhysicalDeviceVulkan13Features& enabled_features) const {
            VkPhysicalDeviceVulkan13Features supported_features{};
            supported_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

            VkPhysicalDeviceFeatures2 supported_features2{};
            supported_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            supported_features2.pNext = &supported_features;
            vkGetPhysicalDeviceFeatures2(physical_device.physical_device, &supported_features2);

            if (!requireFeature(supported_features.dynamicRendering, "dynamicRendering") ||
                !requireFeature(supported_features.synchronization2, "synchronization2") ||
                !requireFeature(supported_features.shaderDemoteToHelperInvocation, "shaderDemoteToHelperInvocation")) {
                return false;
            }

            enabled_features = {};
            enabled_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            enabled_features.dynamicRendering = VK_TRUE;
            enabled_features.synchronization2 = VK_TRUE;
            enabled_features.shaderDemoteToHelperInvocation = VK_TRUE;
            return true;
        }

        bool createSwapchain() {
            if (surface_width == 0 || surface_height == 0) {
                return false;
            }

            VkSurfaceFormatKHR preferred_format{};
            preferred_format.format = VK_FORMAT_B8G8R8A8_SRGB;
            preferred_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

            VkSurfaceFormatKHR fallback_format{};
            fallback_format.format = VK_FORMAT_B8G8R8A8_UNORM;
            fallback_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

            auto swapchain_result = vkb::SwapchainBuilder(device, surface)
                .set_desired_extent(surface_width, surface_height)
                .set_desired_format(preferred_format)
                .add_fallback_format(fallback_format)
                .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .build();

            if (!swapchain_result) {
                logVkbFailure("Vulkan swapchain creation", swapchain_result);
                return false;
            }

            swapchain = swapchain_result.value();

            auto images_result = swapchain.get_images();
            if (!images_result) {
                logVkbFailure("Vulkan swapchain image query", images_result);
                return false;
            }

            swapchain_images = images_result.value();
            swapchain_image_layouts.assign(swapchain_images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
            swapchain_dirty = false;

            VulkanForwardPassSwapchainContext pass_context;
            pass_context.physical_device = physical_device.physical_device;
            pass_context.device = device.device;
            pass_context.color_format = swapchain.image_format;
            pass_context.extent = swapchain.extent;
            pass_context.color_images = swapchain_images;
            return forward_pass.recreateSwapchainResources(pass_context);
        }

        bool recreateSwapchain() {
            if (surface_width == 0 || surface_height == 0 || device.device == VK_NULL_HANDLE) {
                return false;
            }

            vkDeviceWaitIdle(device.device);
            cleanupSwapchain();
            return createSwapchain();
        }

        void cleanupSwapchain() {
            forward_pass.cleanupSwapchainResources();
            swapchain_images.clear();
            swapchain_image_layouts.clear();

            if (swapchain.swapchain != VK_NULL_HANDLE) {
                vkb::destroy_swapchain(swapchain);
                swapchain = {};
            }
        }

        bool createCommandResources() {
            VkCommandPoolCreateInfo pool_info{};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool_info.queueFamilyIndex = graphics_queue_family;

            if (!checkVk(vkCreateCommandPool(device.device, &pool_info, nullptr, &command_pool), "vkCreateCommandPool")) {
                return false;
            }

            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.commandPool = command_pool;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandBufferCount = 1;

            return checkVk(vkAllocateCommandBuffers(device.device, &allocate_info, &command_buffer), "vkAllocateCommandBuffers");
        }

        void cleanupCommandResources() {
            if (command_pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device.device, command_pool, nullptr);
                command_pool = VK_NULL_HANDLE;
                command_buffer = VK_NULL_HANDLE;
            }
        }

        bool createSyncObjects() {
            VkSemaphoreCreateInfo semaphore_info{};
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            if (!checkVk(vkCreateSemaphore(device.device, &semaphore_info, nullptr, &image_available), "vkCreateSemaphore(image_available)") ||
                !checkVk(vkCreateSemaphore(device.device, &semaphore_info, nullptr, &render_finished), "vkCreateSemaphore(render_finished)")) {
                return false;
            }

            VkFenceCreateInfo fence_info{};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            return checkVk(vkCreateFence(device.device, &fence_info, nullptr, &in_flight), "vkCreateFence");
        }

        void cleanupSyncObjects() {
            if (in_flight != VK_NULL_HANDLE) {
                vkDestroyFence(device.device, in_flight, nullptr);
                in_flight = VK_NULL_HANDLE;
            }
            if (render_finished != VK_NULL_HANDLE) {
                vkDestroySemaphore(device.device, render_finished, nullptr);
                render_finished = VK_NULL_HANDLE;
            }
            if (image_available != VK_NULL_HANDLE) {
                vkDestroySemaphore(device.device, image_available, nullptr);
                image_available = VK_NULL_HANDLE;
            }
        }

        void drawFrame(const VulkanDrawList& draw_list) {
            if (swapchain.swapchain == VK_NULL_HANDLE || swapchain_images.empty()) {
                return;
            }

            vkWaitForFences(device.device, 1, &in_flight, VK_TRUE, UINT64_MAX);

            uint32_t image_index = 0;
            VkResult acquire_result = vkAcquireNextImageKHR(
                device.device,
                swapchain.swapchain,
                UINT64_MAX,
                image_available,
                VK_NULL_HANDLE,
                &image_index);

            if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
                swapchain_dirty = true;
                return;
            }
            if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
                NX_CORE_ERROR("vkAcquireNextImageKHR failed: {}", vkResultToString(acquire_result));
                return;
            }

            if (!recordDrawCommands(image_index, draw_list)) {
                return;
            }

            vkResetFences(device.device, 1, &in_flight);

            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = &image_available;
            submit_info.pWaitDstStageMask = &wait_stage;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = &render_finished;

            if (!checkVk(vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight), "vkQueueSubmit")) {
                return;
            }

            VkPresentInfoKHR present_info{};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &render_finished;
            present_info.swapchainCount = 1;
            present_info.pSwapchains = &swapchain.swapchain;
            present_info.pImageIndices = &image_index;

            VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);
            if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
                swapchain_dirty = true;
                return;
            }

            checkVk(present_result, "vkQueuePresentKHR");
        }

        bool recordDrawCommands(uint32_t image_index, const VulkanDrawList& draw_list) {
            if (image_index >= swapchain_images.size()) {
                return false;
            }

            if (!checkVk(vkResetCommandBuffer(command_buffer, 0), "vkResetCommandBuffer")) {
                return false;
            }

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!checkVk(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer")) {
                return false;
            }

            VkImage image = swapchain_images[image_index];
            const VkImageLayout old_layout = swapchain_image_layouts[image_index];
            transitionImageLayout(
                image,
                old_layout,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                old_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            if (!forward_pass.record(command_buffer, image_index, draw_list)) {
                return false;
            }

            transitionImageLayout(
                image,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
            swapchain_image_layouts[image_index] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            return checkVk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");
        }

        void transitionImageLayout(
            VkImage image,
            VkImageLayout old_layout,
            VkImageLayout new_layout,
            VkAccessFlags dst_access_mask,
            VkPipelineStageFlags src_stage,
            VkPipelineStageFlags dst_stage) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = old_layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_READ_BIT;
            barrier.dstAccessMask = dst_access_mask;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                command_buffer,
                src_stage,
                dst_stage,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }

    private:
        GLFWwindow* window = nullptr;

        uint32_t surface_width = kDefaultViewportWidth;
        uint32_t surface_height = kDefaultViewportHeight;

        bool initialized = false;
        bool swapchain_dirty = false;

        vkb::Instance instance;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        vkb::PhysicalDevice physical_device;
        vkb::Device device;
        vkb::Swapchain swapchain;
        std::vector<VkImage> swapchain_images;
        std::vector<VkImageLayout> swapchain_image_layouts;

        VkQueue graphics_queue = VK_NULL_HANDLE;
        VkQueue present_queue = VK_NULL_HANDLE;
        uint32_t graphics_queue_family = 0;
        uint32_t device_api_version = kRequiredVulkanApiVersion;

        VkCommandPool command_pool = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkSemaphore image_available = VK_NULL_HANDLE;
        VkSemaphore render_finished = VK_NULL_HANDLE;
        VkFence in_flight = VK_NULL_HANDLE;

        VulkanRenderResourceCache resource_cache;
        VulkanRenderDataTranslator translator;
        RenderSceneFrameBuilder scene_frame_builder;
        VulkanDrawListBuilder draw_list_builder;
        VulkanForwardPass forward_pass;
    };

    VulkanRendererSystem::VulkanRendererSystem()
        : m_backend(std::make_unique<Backend>()) {}

    VulkanRendererSystem::~VulkanRendererSystem() {
        shutdown();
    }

    bool VulkanRendererSystem::init(WindowService& window_service) {
        return m_backend->init(window_service);
    }

    void VulkanRendererSystem::shutdown() {
        if (m_backend) {
            m_backend->shutdown();
        }
    }

    void VulkanRendererSystem::render(TimeStep ts, const RenderDataPacket& render_data) {
        m_backend->render(ts, render_data);
    }

    void VulkanRendererSystem::setViewportSize(uint32_t width, uint32_t height) {
        m_backend->setViewportSize(width, height);
    }

    std::pair<uint32_t, uint32_t> VulkanRendererSystem::getViewportSize() const {
        return m_backend->getViewportSize();
    }

    ViewportOutput VulkanRendererSystem::getViewportOutput() const {
        return m_backend->getViewportOutput();
    }

    ViewportPickResult VulkanRendererSystem::pickViewport(const ViewportPickRequest& request) {
        (void)request;

        ViewportPickResult result;
        result.supported = false;
        result.ready = false;
        result.entity_id = -1;
        return result;
    }

    void VulkanRendererSystem::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& resize_event) {
            m_backend->resizeSurface(resize_event.getWidth(), resize_event.getHeight());
            return false;
        });
    }
} // namespace NexAur
