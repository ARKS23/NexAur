#include "pch.h"
#include "vulkan_renderer_system.h"

#include "Core/Events/window_event.h"
#include "Function/Platform/platform_services.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Renderer/frontend/render_scene_frame_builder.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list_builder.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_executor.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"
#include "Function/Renderer/Vulkan/passes/vulkan_debug_draw_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_forward_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_object_id_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_shadow_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_skybox_pass.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"
#include "Function/Renderer/Vulkan/resources/vulkan_debug_draw_buffer.h"
#include "Function/Renderer/Vulkan/resources/vulkan_frame_lighting_resource.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"
#include "Function/Renderer/Vulkan/targets/vulkan_picking_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_shadow_map_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_viewport_target.h"
#include "Function/Renderer/Vulkan/ui/vulkan_imgui_renderer.h"
#include "Function/Renderer/Vulkan/vulkan_render_resource_cache.h"

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
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <utility>

namespace NexAur {
    namespace {
        constexpr uint32_t kDefaultViewportWidth = 1280;
        constexpr uint32_t kDefaultViewportHeight = 720;
        constexpr uint32_t kShadowMapResolution = 2048;
        constexpr uint32_t kRequiredVulkanApiVersion = VK_API_VERSION_1_3;

        struct VulkanShadowFrame {
            glm::mat4 light_view_projection{ 1.0f };
        };

        glm::mat4 toVulkanProjection(const glm::mat4& engine_projection) {
            glm::mat4 clip_transform{ 1.0f };
            clip_transform[1][1] = -1.0f;
            clip_transform[2][2] = 0.5f;
            clip_transform[3][2] = 0.5f;
            return clip_transform * engine_projection;
        }

        glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
            if (glm::dot(value, value) <= 0.000001f) {
                return fallback;
            }
            return glm::normalize(value);
        }

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
                !shader_library.init(device.device) ||
                !descriptor_layout_cache.init(device.device) ||
                !descriptor_allocator.init(device.device) ||
                !frame_lighting_resource.init(createResourceContext(), descriptor_layout_cache, descriptor_allocator) ||
                !pipeline_cache.init(device.device, shader_library) ||
                !resource_cache.init(createResourceContext(), descriptor_layout_cache, descriptor_allocator) ||
                !debug_draw_buffer.init(createResourceContext()) ||
                !createSwapchain() ||
                !createCommandResources() ||
                !createSyncObjects()) {
                shutdown();
                return false;
            }

            if (!viewport_target.init(createResourceContext(), swapchain.image_format, surface_width, surface_height)) {
                shutdown();
                return false;
            }

            if (!picking_target.init(createResourceContext(), surface_width, surface_height)) {
                shutdown();
                return false;
            }

            if (!shadow_target.init(createResourceContext(), kShadowMapResolution) ||
                !frame_lighting_resource.updateShadowMap(shadow_target.getDepthImageView(), shadow_target.getSampler())) {
                shutdown();
                return false;
            }

            VulkanShadowPassContext shadow_context;
            shadow_context.device = device.device;
            shadow_context.depth_format = shadow_target.getDepthFormat();
            shadow_context.pipeline_cache = &pipeline_cache;
            if (!shadow_pass.init(shadow_context)) {
                shutdown();
                return false;
            }

            VulkanObjectIdPassContext object_id_context;
            object_id_context.device = device.device;
            object_id_context.object_id_format = picking_target.getObjectIdFormat();
            object_id_context.depth_format = picking_target.getDepthFormat();
            object_id_context.pipeline_cache = &pipeline_cache;
            if (!object_id_pass.init(object_id_context)) {
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

            imgui_renderer.shutdown();
            debug_draw_pass.shutdown();
            skybox_pass.shutdown();
            shadow_pass.shutdown();
            object_id_pass.shutdown();
            shadow_target.shutdown();
            picking_target.shutdown();
            viewport_target.shutdown();
            forward_pass.shutdown();
            cleanupSwapchain();
            debug_draw_buffer.shutdown();
            resource_cache.shutdown();
            frame_lighting_resource.shutdown();
            pipeline_cache.shutdown();
            descriptor_allocator.shutdown();
            descriptor_layout_cache.shutdown();
            shader_library.shutdown();
            cleanupSyncObjects();
            cleanupCommandResources();

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

            auto [render_width, render_height] = getViewportRenderExtent();
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
            if (!initialized || !viewport_target.isReady()) {
                return;
            }

            width = std::max(1u, width);
            height = std::max(1u, height);
            const VkExtent2D current_extent = viewport_target.getExtent();
            if (current_extent.width == width && current_extent.height == height) {
                return;
            }

            vkDeviceWaitIdle(device.device);
            imgui_renderer.releaseViewportTexture();
            const bool viewport_resized = viewport_target.resize(width, height);
            const bool picking_resized = picking_target.resize(width, height);
            if (!viewport_resized || !picking_resized) {
                NX_CORE_ERROR("VulkanRendererSystem failed to resize viewport or picking target.");
                return;
            }
            picking_frame_ready = false;

            if (imgui_renderer.isInitialized()) {
                registerViewportTexture();
            }
        }

        std::pair<uint32_t, uint32_t> getViewportSize() const {
            return getViewportRenderExtent();
        }

        ViewportPickResult pickViewport(const ViewportPickRequest& request) {
            ViewportPickResult result;
            result.supported = true;

            if (!initialized || !picking_target.isReady() || !picking_frame_ready) {
                return result;
            }

            const VkExtent2D extent = picking_target.getExtent();
            if (request.x < 0 || request.y < 0 ||
                request.x >= static_cast<int>(extent.width) ||
                request.y >= static_cast<int>(extent.height)) {
                result.ready = true;
                result.entity_id = -1;
                return result;
            }

            int32_t entity_id = -1;
            if (!readPickingPixel(static_cast<uint32_t>(request.x), static_cast<uint32_t>(request.y), entity_id)) {
                return result;
            }

            result.ready = true;
            result.entity_id = entity_id;
            return result;
        }

        ViewportOutput getViewportOutput() const {
            ViewportOutput output;
            output.backend = RendererBackendType::Vulkan;
            output.coordinate_origin = ViewportCoordinateOrigin::TopLeft;

            if (!initialized) {
                return output;
            }

            if (viewport_target.isReady() && imgui_renderer.hasViewportTexture()) {
                const VkExtent2D extent = viewport_target.getExtent();
                output.kind = ViewportOutputKind::VulkanImGuiTexture;
                output.width = extent.width;
                output.height = extent.height;
                output.native_handle = imgui_renderer.getViewportTextureHandle();
                return output;
            }

            output.kind = ViewportOutputKind::ExternalSwapchain;
            auto [surface_render_width, surface_render_height] = getRenderExtent();
            output.width = surface_render_width;
            output.height = surface_render_height;
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

        void onUIContextInitialized() {
            if (!initImGuiRenderer()) {
                NX_CORE_WARN("VulkanRendererSystem could not initialize the ImGui Vulkan renderer backend yet.");
            }
        }

        void beginUIFrame() {
            imgui_renderer.beginFrame();
        }

        void onUIContextShutdown() {
            if (device.device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device.device);
            }
            imgui_renderer.shutdown();
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

        std::pair<uint32_t, uint32_t> getViewportRenderExtent() const {
            if (viewport_target.isReady()) {
                const VkExtent2D extent = viewport_target.getExtent();
                return { extent.width, extent.height };
            }

            return getRenderExtent();
        }

        VulkanShadowFrame buildDirectionalShadowFrame(const VulkanDrawList& draw_list) const {
            VulkanShadowFrame frame;
            if (!draw_list.directional_light.cast_shadow || !shadow_target.isReady()) {
                return frame;
            }

            const float distance = std::max(1.0f, draw_list.directional_light.shadow_distance);
            const glm::vec3 fallback_light_direction = glm::normalize(glm::vec3{ -0.2f, -1.0f, -0.3f });
            const glm::vec3 light_direction = safeNormalize(
                draw_list.directional_light.direction,
                fallback_light_direction);
            const glm::vec3 camera_forward = safeNormalize(
                -glm::vec3(draw_list.view.inverse_view_matrix[2]),
                glm::vec3{ 0.0f, 0.0f, -1.0f });

            const glm::vec3 center = draw_list.view.camera_position + camera_forward * (distance * 0.5f);
            const glm::vec3 eye = center - light_direction * distance;
            const glm::vec3 world_up{ 0.0f, 1.0f, 0.0f };
            const glm::vec3 up = std::abs(glm::dot(light_direction, world_up)) > 0.95f ?
                glm::vec3{ 0.0f, 0.0f, 1.0f } :
                world_up;

            const glm::mat4 light_view = glm::lookAt(eye, center, up);
            const float radius = std::max(5.0f, distance);
            const glm::mat4 light_projection = glm::ortho(
                -radius,
                radius,
                -radius,
                radius,
                0.1f,
                distance * 3.0f);
            frame.light_view_projection = toVulkanProjection(light_projection) * light_view;
            return frame;
        }

        float getShadowMapSize() const {
            if (!shadow_target.isReady()) {
                return 1.0f;
            }

            return static_cast<float>(shadow_target.getExtent().width);
        }

        VulkanImGuiRendererContext createImGuiRendererContext() const {
            VulkanImGuiRendererContext context;
            context.instance = instance.instance;
            context.physical_device = physical_device.physical_device;
            context.device = device.device;
            context.graphics_queue = graphics_queue;
            context.graphics_queue_family = graphics_queue_family;
            context.swapchain_color_format = swapchain.image_format;
            context.min_image_count = std::max(2u, swapchain.image_count);
            context.image_count = std::max(2u, swapchain.image_count);
            context.api_version = device_api_version;
            return context;
        }

        bool initImGuiRenderer() {
            if (!initialized) {
                return false;
            }

            if (!imgui_renderer.init(createImGuiRendererContext())) {
                return false;
            }

            return registerViewportTexture();
        }

        bool registerViewportTexture() {
            if (!viewport_target.isReady() || !imgui_renderer.isInitialized()) {
                return false;
            }

            return imgui_renderer.registerViewportTexture(
                viewport_target.getColorImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
            pass_context.frame_descriptor_set_layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::FrameGlobal);
            pass_context.material_descriptor_set_layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::Material);
            pass_context.pipeline_cache = &pipeline_cache;

            VulkanSkyboxPassContext skybox_context;
            skybox_context.device = device.device;
            skybox_context.color_format = swapchain.image_format;
            skybox_context.pipeline_cache = &pipeline_cache;

            const bool recreated_skybox_pass = skybox_pass.recreateResources(skybox_context);
            const bool recreated_forward_pass = recreated_skybox_pass && forward_pass.recreateSwapchainResources(pass_context);
            if (!recreated_forward_pass) {
                return false;
            }

            VulkanDebugDrawPassContext debug_draw_context;
            debug_draw_context.device = device.device;
            debug_draw_context.color_format = swapchain.image_format;
            debug_draw_context.depth_format = forward_pass.getDepthFormat();
            debug_draw_context.frame_descriptor_set_layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::FrameGlobal);
            debug_draw_context.pipeline_cache = &pipeline_cache;
            if (!debug_draw_pass.recreateResources(debug_draw_context)) {
                return false;
            }

            if (imgui_renderer.isInitialized()) {
                imgui_renderer.onSwapchainRecreated(std::max(2u, swapchain.image_count));
            }

            return true;
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
            debug_draw_pass.cleanupResources();
            skybox_pass.cleanupResources();
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

            const VulkanShadowFrame shadow_frame = buildDirectionalShadowFrame(draw_list);
            if (!frame_lighting_resource.update(
                    draw_list,
                    shadow_frame.light_view_projection,
                    getShadowMapSize())) {
                return;
            }

            if (!debug_draw_buffer.upload(draw_list.debug_draw)) {
                return;
            }

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

            if (!recordDrawCommands(image_index, draw_list, shadow_frame)) {
                return;
            }

            vkResetFences(device.device, 1, &in_flight);

            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
            if (picking_recorded_this_frame) {
                picking_frame_ready = true;
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

        bool recordDrawCommands(
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            const VulkanShadowFrame& shadow_frame) {
            if (image_index >= swapchain_images.size()) {
                return false;
            }
            picking_recorded_this_frame = false;

            if (!checkVk(vkResetCommandBuffer(command_buffer, 0), "vkResetCommandBuffer")) {
                return false;
            }

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!checkVk(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer")) {
                return false;
            }

            VulkanPassGraph graph;
            const bool render_to_viewport_image = imgui_renderer.isInitialized() && viewport_target.isReady();
            const bool graph_built = render_to_viewport_image ?
                buildViewportRenderGraph(graph, image_index, draw_list, shadow_frame) :
                buildSwapchainRenderGraph(graph, image_index, draw_list, shadow_frame);
            if (!graph_built) {
                return false;
            }

            if (!graph_executor.execute(graph, command_buffer)) {
                return false;
            }

            if (!checkVk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer")) {
                return false;
            }

            graph.commitImageLayouts();
            return true;
        }

        bool buildViewportRenderGraph(
            VulkanPassGraph& graph,
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            const VulkanShadowFrame& shadow_frame) {
            const VulkanGraphImageHandle shadow_depth = addShadowDepthImage(graph);
            const VulkanGraphImageHandle viewport_color = addViewportColorImage(graph);
            const VulkanGraphImageHandle viewport_depth = addViewportDepthImage(graph);
            const VulkanGraphImageHandle swapchain_color = addSwapchainColorImage(graph, image_index);
            if (!shadow_depth.valid() || !viewport_color.valid() || !viewport_depth.valid() || !swapchain_color.valid()) {
                return false;
            }

            if (!addDirectionalShadowPass(graph, shadow_depth, draw_list, shadow_frame)) {
                return false;
            }

            if (!addSkyboxPass(graph, viewport_color, makeViewportSkyboxTarget(), draw_list)) {
                return false;
            }

            graph.addPass("ForwardScene")
                .readImage(shadow_depth, VulkanGraphImageUsage::ShaderRead)
                .writeImage(viewport_color, VulkanGraphImageUsage::ColorAttachment)
                .writeImage(viewport_depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, &draw_list](VkCommandBuffer target_command_buffer) {
                    VulkanForwardPassRenderOptions options = forwardAfterSkyboxOptions();
                    return forward_pass.record(
                        target_command_buffer,
                        viewport_target.getRenderTarget(),
                        draw_list,
                        frame_lighting_resource.getDescriptorSet(),
                        options);
                });

            if (!addDebugDrawPass(graph, viewport_color, viewport_depth, viewport_target.getRenderTarget())) {
                return false;
            }

            if (!addObjectIdPass(graph, draw_list)) {
                return false;
            }

            graph.addPass("ImGuiComposite")
                .readImage(viewport_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(swapchain_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, image_index](VkCommandBuffer target_command_buffer) {
                    return recordImGuiToSwapchain(target_command_buffer, image_index);
                });

            graph.addPass("PresentTransition")
                .writeImage(swapchain_color, VulkanGraphImageUsage::Present);

            return true;
        }

        bool buildSwapchainRenderGraph(
            VulkanPassGraph& graph,
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            const VulkanShadowFrame& shadow_frame) {
            const VulkanGraphImageHandle shadow_depth = addShadowDepthImage(graph);
            const VulkanGraphImageHandle swapchain_color = addSwapchainColorImage(graph, image_index);
            const VulkanGraphImageHandle swapchain_depth = addSwapchainDepthImage(graph);
            if (!shadow_depth.valid() || !swapchain_color.valid() || !swapchain_depth.valid()) {
                return false;
            }

            if (!addDirectionalShadowPass(graph, shadow_depth, draw_list, shadow_frame)) {
                return false;
            }

            if (!addSkyboxPass(graph, swapchain_color, makeSwapchainSkyboxTarget(image_index), draw_list)) {
                return false;
            }

            graph.addPass("ForwardScene")
                .readImage(shadow_depth, VulkanGraphImageUsage::ShaderRead)
                .writeImage(swapchain_color, VulkanGraphImageUsage::ColorAttachment)
                .writeImage(swapchain_depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, image_index, &draw_list](VkCommandBuffer target_command_buffer) {
                    VulkanForwardPassRenderOptions options = forwardAfterSkyboxOptions();
                    return forward_pass.record(
                        target_command_buffer,
                        forward_pass.getSwapchainRenderTarget(image_index),
                        draw_list,
                        frame_lighting_resource.getDescriptorSet(),
                        options);
                });

            if (!addDebugDrawPass(graph, swapchain_color, swapchain_depth, forward_pass.getSwapchainRenderTarget(image_index))) {
                return false;
            }

            if (!addObjectIdPass(graph, draw_list)) {
                return false;
            }

            graph.addPass("PresentTransition")
                .writeImage(swapchain_color, VulkanGraphImageUsage::Present);

            return true;
        }

        bool addDirectionalShadowPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle shadow_depth,
            const VulkanDrawList& draw_list,
            const VulkanShadowFrame& shadow_frame) {
            if (!shadow_depth.valid() || !shadow_target.isReady()) {
                return false;
            }

            graph.addPass("DirectionalShadowMap")
                .writeImage(shadow_depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, &draw_list, shadow_frame](VkCommandBuffer target_command_buffer) {
                    return shadow_pass.record(
                        target_command_buffer,
                        shadow_target.getRenderTarget(),
                        draw_list,
                        shadow_frame.light_view_projection);
                });
            return true;
        }

        bool addSkyboxPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle color_image,
            VulkanSkyboxRenderTarget target,
            const VulkanDrawList& draw_list) {
            if (!color_image.valid() || !target.valid()) {
                return false;
            }

            graph.addPass("Skybox")
                .writeImage(color_image, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, target, &draw_list](VkCommandBuffer target_command_buffer) {
                    return skybox_pass.record(target_command_buffer, target, draw_list);
                });
            return true;
        }

        bool addDebugDrawPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle color_image,
            VulkanGraphImageHandle depth_image,
            VulkanRenderTarget target) {
            if (!debug_draw_buffer.hasVertices()) {
                return true;
            }
            if (!color_image.valid() || !depth_image.valid() || !target.valid()) {
                return false;
            }

            graph.addPass("DebugDraw")
                .writeImage(color_image, VulkanGraphImageUsage::ColorAttachment)
                .writeImage(depth_image, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, target](VkCommandBuffer target_command_buffer) {
                    return debug_draw_pass.record(
                        target_command_buffer,
                        target,
                        debug_draw_buffer,
                        frame_lighting_resource.getDescriptorSet());
                });
            return true;
        }

        bool addObjectIdPass(VulkanPassGraph& graph, const VulkanDrawList& draw_list) {
            if (!picking_target.isReady()) {
                return true;
            }

            const VulkanGraphImageHandle object_id = addPickingObjectIdImage(graph);
            const VulkanGraphImageHandle depth = addPickingDepthImage(graph);
            if (!object_id.valid() || !depth.valid()) {
                return false;
            }

            graph.addPass("ObjectIdPicking")
                .writeImage(object_id, VulkanGraphImageUsage::ColorAttachment)
                .writeImage(depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, &draw_list](VkCommandBuffer target_command_buffer) {
                    if (!object_id_pass.record(target_command_buffer, picking_target.getRenderTarget(), draw_list)) {
                        return false;
                    }

                    picking_recorded_this_frame = true;
                    return true;
                });

            return true;
        }

        VulkanGraphImageHandle addSwapchainColorImage(VulkanPassGraph& graph, uint32_t image_index) {
            if (image_index >= swapchain_images.size() || image_index >= swapchain_image_layouts.size()) {
                return {};
            }

            VulkanGraphImageDesc desc;
            desc.name = "SwapchainColor";
            desc.image = swapchain_images[image_index];
            desc.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = swapchain_image_layouts[image_index];
            desc.commit_layout = [this, image_index](VkImageLayout layout) {
                if (image_index < swapchain_image_layouts.size()) {
                    swapchain_image_layouts[image_index] = layout;
                }
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addSwapchainDepthImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "SwapchainDepth";
            desc.image = forward_pass.getDepthImage();
            desc.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.initial_layout = forward_pass.getDepthImageLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                forward_pass.setDepthImageLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addViewportColorImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "ViewportColor";
            desc.image = viewport_target.getColorImage();
            desc.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = viewport_target.getColorLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                viewport_target.setColorLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addViewportDepthImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "ViewportDepth";
            desc.image = viewport_target.getDepthImage();
            desc.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.initial_layout = viewport_target.getDepthLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                viewport_target.setDepthLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addPickingObjectIdImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "PickingObjectId";
            desc.image = picking_target.getObjectIdImage();
            desc.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = picking_target.getObjectIdLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                picking_target.setObjectIdLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addPickingDepthImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "PickingDepth";
            desc.image = picking_target.getDepthImage();
            desc.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.initial_layout = picking_target.getDepthLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                picking_target.setDepthLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addShadowDepthImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "ShadowDepth";
            desc.image = shadow_target.getDepthImage();
            desc.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.initial_layout = shadow_target.getDepthLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                shadow_target.setDepthLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanSkyboxRenderTarget makeViewportSkyboxTarget() const {
            VulkanSkyboxRenderTarget target;
            target.color_view = viewport_target.getColorImageView();
            target.color_format = viewport_target.getColorFormat();
            target.extent = viewport_target.getExtent();
            return target;
        }

        VulkanSkyboxRenderTarget makeSwapchainSkyboxTarget(uint32_t image_index) const {
            VulkanSkyboxRenderTarget target;
            target.color_view = forward_pass.getSwapchainColorImageView(image_index);
            target.color_format = swapchain.image_format;
            target.extent = swapchain.extent;
            return target;
        }

        VulkanForwardPassRenderOptions forwardAfterSkyboxOptions() const {
            VulkanForwardPassRenderOptions options;
            options.color_load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            options.depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            options.depth_clear_value.depthStencil.depth = 1.0f;
            options.depth_clear_value.depthStencil.stencil = 0;
            return options;
        }

        bool recordImGuiToSwapchain(VkCommandBuffer target_command_buffer, uint32_t image_index) {
            VkImageView swapchain_view = forward_pass.getSwapchainColorImageView(image_index);
            if (swapchain_view == VK_NULL_HANDLE) {
                return false;
            }

            VkClearValue clear_value{};
            clear_value.color.float32[0] = 0.08f;
            clear_value.color.float32[1] = 0.10f;
            clear_value.color.float32[2] = 0.14f;
            clear_value.color.float32[3] = 1.0f;

            VkRenderingAttachmentInfo color_attachment{};
            color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            color_attachment.imageView = swapchain_view;
            color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment.clearValue = clear_value;

            VkRenderingInfo rendering_info{};
            rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            rendering_info.renderArea.offset = { 0, 0 };
            rendering_info.renderArea.extent = swapchain.extent;
            rendering_info.layerCount = 1;
            rendering_info.colorAttachmentCount = 1;
            rendering_info.pColorAttachments = &color_attachment;

            vkCmdBeginRendering(target_command_buffer, &rendering_info);
            imgui_renderer.renderDrawData(target_command_buffer);
            vkCmdEndRendering(target_command_buffer);
            return true;
        }

        void transitionImageLayout(
            VkImage image,
            VkImageLayout old_layout,
            VkImageLayout new_layout,
            VkImageAspectFlags aspect_mask,
            VkAccessFlags src_access_mask,
            VkAccessFlags dst_access_mask,
            VkPipelineStageFlags src_stage,
            VkPipelineStageFlags dst_stage) {
            transitionImageLayout(
                command_buffer,
                image,
                old_layout,
                new_layout,
                aspect_mask,
                src_access_mask,
                dst_access_mask,
                src_stage,
                dst_stage);
        }

        void transitionImageLayout(
            VkCommandBuffer target_command_buffer,
            VkImage image,
            VkImageLayout old_layout,
            VkImageLayout new_layout,
            VkImageAspectFlags aspect_mask,
            VkAccessFlags src_access_mask,
            VkAccessFlags dst_access_mask,
            VkPipelineStageFlags src_stage,
            VkPipelineStageFlags dst_stage) {
            if (old_layout == new_layout) {
                return;
            }

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = src_access_mask;
            barrier.dstAccessMask = dst_access_mask;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = aspect_mask;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                target_command_buffer,
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

        bool readPickingPixel(uint32_t x, uint32_t y, int32_t& entity_id) {
            if (!picking_target.isReady() || picking_target.getReadbackBuffer() == VK_NULL_HANDLE) {
                return false;
            }

            if (!checkVk(vkDeviceWaitIdle(device.device), "vkDeviceWaitIdle(before picking readback)")) {
                return false;
            }

            VkCommandBuffer readback_command_buffer = VK_NULL_HANDLE;
            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.commandPool = command_pool;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandBufferCount = 1;
            if (!checkVk(vkAllocateCommandBuffers(device.device, &allocate_info, &readback_command_buffer), "vkAllocateCommandBuffers(picking readback)")) {
                return false;
            }

            auto free_readback_command_buffer = [&]() {
                if (readback_command_buffer != VK_NULL_HANDLE) {
                    vkFreeCommandBuffers(device.device, command_pool, 1, &readback_command_buffer);
                    readback_command_buffer = VK_NULL_HANDLE;
                }
            };

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!checkVk(vkBeginCommandBuffer(readback_command_buffer, &begin_info), "vkBeginCommandBuffer(picking readback)")) {
                free_readback_command_buffer();
                return false;
            }

            const VkImageLayout old_layout = picking_target.getObjectIdLayout();
            VkAccessFlags src_access = 0;
            VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
                src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                src_access = VK_ACCESS_TRANSFER_READ_BIT;
                src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }

            transitionImageLayout(
                readback_command_buffer,
                picking_target.getObjectIdImage(),
                old_layout,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT,
                src_access,
                VK_ACCESS_TRANSFER_READ_BIT,
                src_stage,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
            picking_target.setObjectIdLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            VkBufferImageCopy copy_region{};
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageOffset = {
                static_cast<int32_t>(x),
                static_cast<int32_t>(y),
                0
            };
            copy_region.imageExtent = { 1, 1, 1 };

            vkCmdCopyImageToBuffer(
                readback_command_buffer,
                picking_target.getObjectIdImage(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                picking_target.getReadbackBuffer(),
                1,
                &copy_region);

            if (!checkVk(vkEndCommandBuffer(readback_command_buffer), "vkEndCommandBuffer(picking readback)")) {
                free_readback_command_buffer();
                return false;
            }

            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &readback_command_buffer;

            const bool submitted = checkVk(vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE), "vkQueueSubmit(picking readback)");
            const bool waited = submitted && checkVk(vkQueueWaitIdle(graphics_queue), "vkQueueWaitIdle(picking readback)");
            free_readback_command_buffer();
            if (!waited) {
                return false;
            }

            entity_id = picking_target.readbackEntityId();
            return true;
        }

    private:
        GLFWwindow* window = nullptr;

        uint32_t surface_width = kDefaultViewportWidth;
        uint32_t surface_height = kDefaultViewportHeight;

        bool initialized = false;
        bool swapchain_dirty = false;
        bool picking_frame_ready = false;
        bool picking_recorded_this_frame = false;

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

        VulkanShaderLibrary shader_library;
        VulkanDescriptorLayoutCache descriptor_layout_cache;
        VulkanDescriptorAllocator descriptor_allocator;
        VulkanPipelineCache pipeline_cache;
        VulkanFrameLightingResource frame_lighting_resource;
        VulkanRenderResourceCache resource_cache;
        VulkanRenderDataTranslator translator;
        RenderSceneFrameBuilder scene_frame_builder;
        VulkanDrawListBuilder draw_list_builder;
        VulkanGraphExecutor graph_executor;
        VulkanDebugDrawBuffer debug_draw_buffer;
        VulkanDebugDrawPass debug_draw_pass;
        VulkanShadowPass shadow_pass;
        VulkanSkyboxPass skybox_pass;
        VulkanForwardPass forward_pass;
        VulkanObjectIdPass object_id_pass;
        VulkanViewportTarget viewport_target;
        VulkanPickingTarget picking_target;
        VulkanShadowMapTarget shadow_target;
        VulkanImGuiRenderer imgui_renderer;
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
        return m_backend->pickViewport(request);
    }

    void VulkanRendererSystem::onUIContextInitialized() {
        m_backend->onUIContextInitialized();
    }

    void VulkanRendererSystem::beginUIFrame() {
        m_backend->beginUIFrame();
    }

    void VulkanRendererSystem::onUIContextShutdown() {
        m_backend->onUIContextShutdown();
    }

    void VulkanRendererSystem::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& resize_event) {
            m_backend->resizeSurface(resize_event.getWidth(), resize_event.getHeight());
            return false;
        });
    }
} // namespace NexAur
