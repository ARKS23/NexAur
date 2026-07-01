#include "pch.h"
#include "vulkan_renderer_system.h"

#include "Core/Events/window_event.h"
#include "Function/Platform/platform_services.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Renderer/data/render_scene_frame.h"
#include "Function/Renderer/data/render_shadow_cascade.h"
#include "Function/Renderer/data/render_view.h"
#include "Function/Renderer/frontend/render_scene_frame_builder.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list_builder.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_executor.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"
#include "Function/Renderer/Vulkan/passes/vulkan_bloom_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_debug_draw_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_forward_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_object_id_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_post_process_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_shadow_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_skybox_pass.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"
#include "Function/Renderer/Vulkan/resources/vulkan_debug_draw_buffer.h"
#include "Function/Renderer/Vulkan/resources/vulkan_frame_lighting_resource.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"
#include "Function/Renderer/Vulkan/targets/vulkan_bloom_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_picking_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_scene_color_target.h"
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
#include <array>
#include <chrono>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <string>
#include <utility>

namespace NexAur {
    namespace {
        constexpr uint32_t kDefaultViewportWidth = 1280;
        constexpr uint32_t kDefaultViewportHeight = 720;
        constexpr uint32_t kDefaultShadowMapResolution = 2048;
        constexpr uint32_t kRequiredVulkanApiVersion = VK_API_VERSION_1_3;

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

        float srgbToLinear(float value) {
            if (value <= 0.04045f) {
                return value / 12.92f;
            }

            return std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        bool isSrgbColorFormat(VkFormat format) {
            switch (format) {
            case VK_FORMAT_R8_SRGB:
            case VK_FORMAT_R8G8_SRGB:
            case VK_FORMAT_R8G8B8_SRGB:
            case VK_FORMAT_B8G8R8_SRGB:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                return true;
            default:
                return false;
            }
        }

        float colorForAttachment(float srgb_value, VkFormat format) {
            return isSrgbColorFormat(format) ? srgbToLinear(srgb_value) : srgb_value;
        }

        float sanitizeMin(float value, float fallback, float minimum) {
            return std::isfinite(value) && value >= minimum ? value : fallback;
        }

        uint32_t sanitizeShadowMapResolution(uint32_t resolution) {
            if (resolution >= 4096u) {
                return 4096u;
            }
            if (resolution >= 2048u) {
                return 2048u;
            }
            return 1024u;
        }

        uint32_t sanitizeShadowCascadeCount(const RenderShadowSettings& settings) {
            if (!settings.cascades_enabled) {
                return 1u;
            }

            return std::clamp(settings.cascade_count, 1u, kMaxRenderShadowCascadeCount);
        }

        bool isBloomDebugView(RenderEffectDebugView view) {
            return view == RenderEffectDebugView::BloomComposite ||
                   view == RenderEffectDebugView::BloomDownsampleMip ||
                   view == RenderEffectDebugView::BloomUpsampleMip;
        }

        bool isShadowCascadeDebugView(RenderEffectDebugView view) {
            return view == RenderEffectDebugView::ShadowCascades;
        }

        const char* effectDebugViewToText(RenderEffectDebugView view) {
            switch (view) {
            case RenderEffectDebugView::HdrSceneColor:
                return "HDR Scene Color";
            case RenderEffectDebugView::BloomComposite:
                return "Bloom Composite";
            case RenderEffectDebugView::BloomDownsampleMip:
                return "Bloom Downsample Mip";
            case RenderEffectDebugView::BloomUpsampleMip:
                return "Bloom Upsample Mip";
            case RenderEffectDebugView::ShadowMap:
                return "Shadow Map";
            case RenderEffectDebugView::ShadowCascades:
                return "Shadow Cascades";
            case RenderEffectDebugView::FinalLit:
            default:
                return "Final Lit";
            }
        }

        float sanitizeUnit(float value, float fallback) {
            if (!std::isfinite(value)) {
                return fallback;
            }
            return std::clamp(value, 0.0f, 1.0f);
        }

        float computeCascadeSplitDepth(float near_clip, float far_clip, float split_ratio, float split_lambda) {
            const float safe_near = std::max(0.001f, near_clip);
            const float safe_far = std::max(safe_near + 0.001f, far_clip);
            const float linear_split = safe_near + (safe_far - safe_near) * split_ratio;
            const float logarithmic_split = safe_near * std::pow(safe_far / safe_near, split_ratio);
            return linear_split * (1.0f - split_lambda) + logarithmic_split * split_lambda;
        }

        float viewDepthToNdcZ(const glm::mat4& projection, float view_depth) {
            const glm::vec4 clip = projection * glm::vec4{ 0.0f, 0.0f, -view_depth, 1.0f };
            if (std::abs(clip.w) <= 0.000001f) {
                return 0.0f;
            }
            return clip.z / clip.w;
        }

        glm::vec3 unprojectNdcPoint(const RenderView& view, const glm::vec3& ndc) {
            glm::vec4 view_position = view.inverse_projection_matrix * glm::vec4{ ndc, 1.0f };
            if (std::abs(view_position.w) > 0.000001f) {
                view_position /= view_position.w;
            }

            glm::vec4 world_position = view.inverse_view_matrix * view_position;
            if (std::abs(world_position.w) > 0.000001f) {
                world_position /= world_position.w;
            }
            return glm::vec3(world_position);
        }

        std::array<glm::vec3, 8> buildCascadeFrustumCorners(
            const RenderView& view,
            float near_depth,
            float far_depth) {
            const float near_z = viewDepthToNdcZ(view.projection_matrix, near_depth);
            const float far_z = viewDepthToNdcZ(view.projection_matrix, far_depth);
            const std::array<glm::vec3, 8> ndc_corners{
                glm::vec3{ -1.0f, -1.0f, near_z },
                glm::vec3{  1.0f, -1.0f, near_z },
                glm::vec3{ -1.0f,  1.0f, near_z },
                glm::vec3{  1.0f,  1.0f, near_z },
                glm::vec3{ -1.0f, -1.0f, far_z },
                glm::vec3{  1.0f, -1.0f, far_z },
                glm::vec3{ -1.0f,  1.0f, far_z },
                glm::vec3{  1.0f,  1.0f, far_z }
            };

            std::array<glm::vec3, 8> corners{};
            for (size_t index = 0; index < ndc_corners.size(); ++index) {
                corners[index] = unprojectNdcPoint(view, ndc_corners[index]);
            }
            return corners;
        }

        glm::mat4 buildCascadeLightViewProjection(
            const std::array<glm::vec3, 8>& corners,
            const glm::vec3& light_direction,
            const glm::vec3& up,
            float shadow_map_size,
            bool stabilize) {
            glm::vec3 center{ 0.0f };
            for (const glm::vec3& corner : corners) {
                center += corner;
            }
            center /= static_cast<float>(corners.size());

            float radius = 0.0f;
            for (const glm::vec3& corner : corners) {
                radius = std::max(radius, glm::length(corner - center));
            }
            radius = std::max(radius, 1.0f);

            const glm::vec3 eye = center - light_direction * (radius + 10.0f);
            const glm::mat4 light_view = glm::lookAt(eye, center, up);

            glm::vec3 min_bounds{ std::numeric_limits<float>::max() };
            glm::vec3 max_bounds{ std::numeric_limits<float>::lowest() };
            for (const glm::vec3& corner : corners) {
                const glm::vec3 light_space_corner = glm::vec3(light_view * glm::vec4(corner, 1.0f));
                min_bounds = glm::min(min_bounds, light_space_corner);
                max_bounds = glm::max(max_bounds, light_space_corner);
            }

            const float half_extent = std::max(
                0.5f,
                std::max(max_bounds.x - min_bounds.x, max_bounds.y - min_bounds.y) * 0.5f);
            glm::vec2 center_xy{
                (min_bounds.x + max_bounds.x) * 0.5f,
                (min_bounds.y + max_bounds.y) * 0.5f
            };

            if (stabilize && shadow_map_size > 1.0f) {
                const float texel_world_size = (half_extent * 2.0f) / shadow_map_size;
                if (texel_world_size > 0.0f && std::isfinite(texel_world_size)) {
                    center_xy.x = std::round(center_xy.x / texel_world_size) * texel_world_size;
                    center_xy.y = std::round(center_xy.y / texel_world_size) * texel_world_size;
                }
            }

            constexpr float kCascadeDepthPadding = 10.0f;
            const float near_plane = std::max(0.01f, -max_bounds.z - kCascadeDepthPadding);
            const float far_plane = std::max(near_plane + 1.0f, -min_bounds.z + kCascadeDepthPadding);
            const glm::mat4 light_projection = glm::ortho(
                center_xy.x - half_extent,
                center_xy.x + half_extent,
                center_xy.y - half_extent,
                center_xy.y + half_extent,
                near_plane,
                far_plane);
            return toVulkanProjection(light_projection) * light_view;
        }

        glm::vec3 stabilizeShadowCenter(
            const glm::vec3& center,
            const glm::vec3& light_direction,
            const glm::vec3& up,
            float radius,
            float shadow_map_size,
            bool enabled) {
            if (!enabled || radius <= 0.0f || shadow_map_size <= 1.0f) {
                return center;
            }

            const float texel_world_size = (radius * 2.0f) / shadow_map_size;
            if (texel_world_size <= 0.0f || !std::isfinite(texel_world_size)) {
                return center;
            }

            const glm::mat4 light_basis = glm::lookAt(glm::vec3{ 0.0f }, light_direction, up);
            glm::vec4 light_space_center = light_basis * glm::vec4(center, 1.0f);
            light_space_center.x = std::round(light_space_center.x / texel_world_size) * texel_world_size;
            light_space_center.y = std::round(light_space_center.y / texel_world_size) * texel_world_size;

            const glm::vec4 snapped_center = glm::inverse(light_basis) * light_space_center;
            return glm::vec3(snapped_center);
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

        std::string vkFormatToString(VkFormat format) {
            switch (format) {
            case VK_FORMAT_UNDEFINED:
                return "Undefined";
            case VK_FORMAT_B8G8R8A8_SRGB:
                return "B8G8R8A8_SRGB";
            case VK_FORMAT_B8G8R8A8_UNORM:
                return "B8G8R8A8_UNORM";
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return "R16G16B16A16_SFLOAT";
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
                return "B10G11R11_UFLOAT_PACK32";
            case VK_FORMAT_R32_SINT:
                return "R32_SINT";
            case VK_FORMAT_D16_UNORM:
                return "D16_UNORM";
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return "D24_UNORM_S8_UINT";
            case VK_FORMAT_D32_SFLOAT:
                return "D32_SFLOAT";
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return "D32_SFLOAT_S8_UINT";
            default:
                return "VkFormat(" + std::to_string(static_cast<int>(format)) + ")";
            }
        }

        std::string apiVersionToString(uint32_t api_version) {
            return std::to_string(VK_API_VERSION_MAJOR(api_version)) + "." +
                   std::to_string(VK_API_VERSION_MINOR(api_version)) + "." +
                   std::to_string(VK_API_VERSION_PATCH(api_version));
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

        VkFormat findHdrSceneColorFormat(VkPhysicalDevice physical_device) {
            constexpr VkFormatFeatureFlags required_features =
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            const std::array<VkFormat, 2> candidates{
                VK_FORMAT_R16G16B16A16_SFLOAT,
                VK_FORMAT_B10G11R11_UFLOAT_PACK32
            };

            for (VkFormat format : candidates) {
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
                if ((properties.optimalTilingFeatures & required_features) == required_features) {
                    return format;
                }
            }

            return VK_FORMAT_UNDEFINED;
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

            if (!scene_color_target.init(createResourceContext(), scene_color_format, surface_width, surface_height) ||
                !bloom_target.init(createResourceContext(), scene_color_format, surface_width, surface_height) ||
                !recreateBloomPassResourcesIfReady()) {
                shutdown();
                return false;
            }

            if (!picking_target.init(createResourceContext(), surface_width, surface_height)) {
                shutdown();
                return false;
            }

            if (!shadow_target.init(createResourceContext(), kDefaultShadowMapResolution) ||
                !frame_lighting_resource.updateShadowMap(shadow_target.getDepthImageView(), shadow_target.getSampler()) ||
                !updatePostProcessInput()) {
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
            post_process_pass.shutdown();
            bloom_pass.shutdown();
            debug_draw_pass.shutdown();
            skybox_pass.shutdown();
            shadow_pass.shutdown();
            object_id_pass.shutdown();
            shadow_target.shutdown();
            picking_target.shutdown();
            bloom_target.shutdown();
            scene_color_target.shutdown();
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
            scene_color_format = VK_FORMAT_UNDEFINED;
            initialized = false;
        }

        void render(TimeStep ts, const RenderDataPacket& render_data) {
            const auto render_start_time = std::chrono::steady_clock::now();

            if (!initialized || surface_width == 0 || surface_height == 0) {
                updateDebugSnapshot(ts, render_data, nullptr, nullptr, nullptr, render_start_time);
                return;
            }

            if (swapchain_dirty && !recreateSwapchain()) {
                updateDebugSnapshot(ts, render_data, nullptr, nullptr, nullptr, render_start_time);
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

            drawFrame(draw_list, render_data.render_settings);
            updateDebugSnapshot(
                ts,
                render_data,
                &render_view,
                &scene_frame,
                &draw_list,
                render_start_time);
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
            const bool scene_color_resized = scene_color_target.resize(width, height);
            const bool bloom_resized = bloom_target.resize(width, height);
            const bool picking_resized = picking_target.resize(width, height);
            if (!viewport_resized ||
                !scene_color_resized ||
                !bloom_resized ||
                !picking_resized ||
                !recreateBloomPassResourcesIfReady() ||
                !updatePostProcessInput()) {
                NX_CORE_ERROR("VulkanRendererSystem failed to resize viewport, HDR scene color, bloom, or picking target.");
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

        RendererDebugSnapshot getDebugSnapshot() const {
            return debug_snapshot;
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

        void updateDebugSnapshot(
            TimeStep ts,
            const RenderDataPacket& render_data,
            const RenderView* render_view,
            const RenderSceneFrame* scene_frame,
            const VulkanDrawList* draw_list,
            std::chrono::steady_clock::time_point render_start_time) {
            RendererDebugSnapshot snapshot;
            snapshot.backend = buildBackendDebugStats();
            snapshot.frame = buildFrameDebugStats(
                ts,
                render_data,
                scene_frame,
                draw_list,
                render_start_time);
            snapshot.view = buildViewDebugStats(render_view);
            snapshot.viewport_target = buildViewportTargetDebugStats();
            snapshot.hdr_scene_target = buildHdrSceneTargetDebugStats();
            snapshot.picking_target = buildPickingTargetDebugStats();
            snapshot.shadow_target = buildShadowTargetDebugStats();
            snapshot.post_process = buildPostProcessDebugStats();
            snapshot.bloom = buildBloomDebugStats();
            snapshot.effects = buildEffectsDebugStats(render_data.render_settings);
            snapshot.resources = buildResourceDebugStats(draw_list);

            debug_snapshot = std::move(snapshot);
        }

        RendererDebugBackendStats buildBackendDebugStats() const {
            RendererDebugBackendStats stats;
            stats.backend = RendererBackendType::Vulkan;
            stats.initialized = initialized;
            stats.device_api_version = apiVersionToString(device_api_version);
            stats.swapchain_ready = swapchain.swapchain != VK_NULL_HANDLE && !swapchain_images.empty();
            stats.swapchain_width = swapchain.extent.width;
            stats.swapchain_height = swapchain.extent.height;
            stats.swapchain_image_count = static_cast<uint32_t>(swapchain_images.size());
            stats.swapchain_format = vkFormatToString(swapchain.image_format);
            stats.viewport_output_kind = getViewportOutput().kind;
            return stats;
        }

        RendererDebugFrameStats buildFrameDebugStats(
            TimeStep ts,
            const RenderDataPacket& render_data,
            const RenderSceneFrame* scene_frame,
            const VulkanDrawList* draw_list,
            std::chrono::steady_clock::time_point render_start_time) const {
            RendererDebugFrameStats stats;
            stats.engine_delta_ms = ts.GetMilliseconds();
            stats.renderer_cpu_ms =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - render_start_time).count();

            stats.opaque_object_count = scene_frame ?
                scene_frame->opaque_objects.size() :
                render_data.opaque_objects.size();
            stats.transparent_object_count = scene_frame ?
                scene_frame->transparent_objects.size() :
                render_data.transparent_objects.size();
            stats.opaque_draw_item_count = draw_list ? draw_list->opaque_items.size() : 0;
            stats.transparent_draw_item_count = draw_list ? draw_list->transparent_items.size() : 0;
            stats.point_light_count = draw_list ?
                draw_list->point_lights.size() :
                render_data.point_lights_data.size();
            stats.debug_line_count = draw_list ?
                draw_list->debug_draw.lines.size() :
                render_data.debug_draw.lines.size();
            return stats;
        }

        RendererDebugViewStats buildViewDebugStats(const RenderView* render_view) const {
            RendererDebugViewStats stats;
            if (!render_view) {
                return stats;
            }

            stats.viewport_width = render_view->viewport_width;
            stats.viewport_height = render_view->viewport_height;
            stats.camera_position = render_view->camera_position;
            stats.near_clip = render_view->near_clip;
            stats.far_clip = render_view->far_clip;
            return stats;
        }

        RendererDebugRenderTargetStats buildViewportTargetDebugStats() const {
            RendererDebugRenderTargetStats stats;
            stats.ready = viewport_target.isReady();
            if (!stats.ready) {
                return stats;
            }

            const VkExtent2D extent = viewport_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.color_format = vkFormatToString(viewport_target.getColorFormat());
            stats.depth_format = vkFormatToString(viewport_target.getDepthFormat());
            return stats;
        }

        RendererDebugRenderTargetStats buildHdrSceneTargetDebugStats() const {
            RendererDebugRenderTargetStats stats;
            stats.ready = scene_color_target.isReady();
            if (!stats.ready) {
                return stats;
            }

            const VkExtent2D extent = scene_color_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.color_format = vkFormatToString(scene_color_target.getColorFormat());
            stats.depth_format = "Shared";
            return stats;
        }

        RendererDebugPickingTargetStats buildPickingTargetDebugStats() const {
            RendererDebugPickingTargetStats stats;
            stats.ready = picking_target.isReady();
            stats.frame_ready = picking_frame_ready;
            if (!stats.ready) {
                return stats;
            }

            const VkExtent2D extent = picking_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.object_id_format = vkFormatToString(picking_target.getObjectIdFormat());
            stats.depth_format = vkFormatToString(picking_target.getDepthFormat());
            return stats;
        }

        RendererDebugShadowTargetStats buildShadowTargetDebugStats() const {
            RendererDebugShadowTargetStats stats;
            stats.ready = shadow_target.isReady();
            if (!stats.ready) {
                return stats;
            }

            const VkExtent2D extent = shadow_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.layer_count = shadow_target.getLayerCount();
            stats.depth_format = vkFormatToString(shadow_target.getDepthFormat());
            return stats;
        }

        RendererDebugPostProcessStats buildPostProcessDebugStats() const {
            RendererDebugPostProcessStats stats;
            stats.enabled = true;
            stats.ready = post_process_pass.isReady();
            if (post_process_pass.getOutputColorFormat() != VK_FORMAT_UNDEFINED) {
                stats.output_format = vkFormatToString(post_process_pass.getOutputColorFormat());
            }
            return stats;
        }

        RendererDebugBloomStats buildBloomDebugStats() const {
            RendererDebugBloomStats stats;
            stats.ready = bloom_target.isReady() && bloom_pass.isReady();
            if (!bloom_target.isReady()) {
                return stats;
            }

            const VkExtent2D extent = bloom_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.mip_count = bloom_target.getMipCount();
            stats.color_format = vkFormatToString(bloom_target.getColorFormat());
            return stats;
        }

        RendererDebugEffectsStats buildEffectsDebugStats(const RenderSettings& render_settings) const {
            RendererDebugEffectsStats stats;
            stats.debug_view = effectDebugViewToText(render_settings.effects_debug.view);
            stats.bloom_mip = render_settings.effects_debug.bloom_mip;
            stats.shadow_cascade = render_settings.effects_debug.shadow_cascade;
            stats.bloom_debug_available = bloom_target.isReady() && bloom_pass.isReady();
            stats.shadow_debug_available = shadow_target.isReady();
            return stats;
        }

        RendererDebugResourceStats buildResourceDebugStats(const VulkanDrawList* draw_list) const {
            RendererDebugResourceStats stats;
            if (!resource_cache.isInitialized()) {
                return stats;
            }

            stats.model_count = resource_cache.getModelCount();
            stats.texture_count = resource_cache.getTextureCount();
            stats.environment_count = resource_cache.getEnvironmentCount();
            stats.mesh_count = resource_cache.getMeshCount();
            stats.material_count = resource_cache.getMaterialCount();
            stats.fallback_white_texture_ready = resource_cache.hasFallbackWhiteTexture();
            stats.fallback_material_ready = resource_cache.hasFallbackMaterial();
            stats.fallback_environment_ready = resource_cache.hasFallbackEnvironment();

            const VulkanEnvironmentResource* active_environment = draw_list ? draw_list->environment : nullptr;
            if (!active_environment || !active_environment->isReady()) {
                active_environment = resource_cache.getFallbackEnvironment();
            }
            if (active_environment && active_environment->isReady()) {
                stats.active_environment_ready = true;
                stats.active_environment_name = active_environment->getDebugName();
                stats.environment_source_width = active_environment->getSourceWidth();
                stats.environment_source_height = active_environment->getSourceHeight();
                stats.environment_size = active_environment->getEnvironmentSize();
                stats.irradiance_size = active_environment->getIrradianceSize();
                stats.prefilter_size = active_environment->getPrefilterSize();
                stats.prefilter_mip_count = active_environment->getPrefilterMipCount();
                stats.brdf_lut_ready = active_environment->hasBrdfLut();
            }
            return stats;
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

        bool ensureShadowTarget(const RenderShadowSettings& shadow_settings) {
            const uint32_t resolution = sanitizeShadowMapResolution(shadow_settings.map_resolution);
            const uint32_t layer_count = sanitizeShadowCascadeCount(shadow_settings);
            if (shadow_target.isReady() &&
                shadow_target.getExtent().width == resolution &&
                shadow_target.getLayerCount() == layer_count) {
                return true;
            }

            if (device.device == VK_NULL_HANDLE) {
                return false;
            }

            vkDeviceWaitIdle(device.device);
            shadow_target.shutdown();
            if (!shadow_target.init(createResourceContext(), resolution, layer_count)) {
                return false;
            }

            return frame_lighting_resource.updateShadowMap(
                       shadow_target.getDepthImageView(),
                       shadow_target.getSampler()) &&
                   updatePostProcessInputIfReady();
        }

        RenderShadowCascadeFrame buildDirectionalShadowFrame(
            const VulkanDrawList& draw_list,
            const RenderSettings& render_settings) const {
            RenderShadowCascadeFrame frame;
            if (!render_settings.shadow.enabled ||
                !draw_list.directional_light.cast_shadow ||
                !shadow_target.isReady()) {
                return frame;
            }

            const float distance = sanitizeMin(
                render_settings.shadow.distance,
                draw_list.directional_light.shadow_distance,
                1.0f);
            const glm::vec3 fallback_light_direction = glm::normalize(glm::vec3{ -0.2f, -1.0f, -0.3f });
            const glm::vec3 light_direction = safeNormalize(
                draw_list.directional_light.direction,
                fallback_light_direction);
            const glm::vec3 camera_forward = safeNormalize(
                -glm::vec3(draw_list.view.inverse_view_matrix[2]),
                glm::vec3{ 0.0f, 0.0f, -1.0f });

            const glm::vec3 center = draw_list.view.camera_position + camera_forward * (distance * 0.5f);
            const glm::vec3 world_up{ 0.0f, 1.0f, 0.0f };
            const glm::vec3 up = std::abs(glm::dot(light_direction, world_up)) > 0.95f ?
                glm::vec3{ 0.0f, 0.0f, 1.0f } :
                world_up;

            const uint32_t cascade_count = sanitizeShadowCascadeCount(render_settings.shadow);
            frame.cascade_count = cascade_count;
            frame.cascades_enabled = cascade_count > 1;
            frame.debug_overlay =
                frame.cascades_enabled &&
                (render_settings.shadow.cascade_debug_overlay ||
                 isShadowCascadeDebugView(render_settings.effects_debug.view));

            if (cascade_count > 1) {
                const float near_clip = std::max(0.001f, draw_list.view.near_clip);
                const float far_clip = std::max(near_clip + 0.001f, distance);
                const float split_lambda = sanitizeUnit(render_settings.shadow.cascade_split_lambda, 0.65f);
                float cascade_near = near_clip;
                for (uint32_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index) {
                    const float split_ratio = static_cast<float>(cascade_index + 1u) / static_cast<float>(cascade_count);
                    const float cascade_far = cascade_index + 1u == cascade_count ?
                        far_clip :
                        computeCascadeSplitDepth(near_clip, far_clip, split_ratio, split_lambda);
                    const std::array<glm::vec3, 8> corners =
                        buildCascadeFrustumCorners(draw_list.view, cascade_near, cascade_far);
                    frame.light_view_projections[cascade_index] = buildCascadeLightViewProjection(
                        corners,
                        light_direction,
                        up,
                        getShadowMapSize(),
                        render_settings.shadow.stabilize);
                    frame.split_depths[cascade_index] = cascade_far;
                    cascade_near = cascade_far;
                }
                return frame;
            }

            const float radius = std::max(5.0f, distance);
            const glm::vec3 stable_center = stabilizeShadowCenter(
                center,
                light_direction,
                up,
                radius,
                getShadowMapSize(),
                render_settings.shadow.stabilize);
            const glm::vec3 eye = stable_center - light_direction * distance;
            const glm::mat4 light_view = glm::lookAt(eye, stable_center, up);
            const glm::mat4 light_projection = glm::ortho(
                -radius,
                radius,
                -radius,
                radius,
                0.1f,
                distance * 3.0f);
            frame.light_view_projections[0] = toVulkanProjection(light_projection) * light_view;
            frame.split_depths[0] = distance;
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

        bool updatePostProcessInput() {
            if (!post_process_pass.isReady() || !scene_color_target.isReady()) {
                return false;
            }

            const VulkanPostProcessInput input =
                (bloom_target.isReady() && bloom_pass.isReady()) ?
                    makeBloomCompositePostProcessInput() :
                    makeScenePostProcessInput();
            return post_process_pass.updateInput(input);
        }

        bool updatePostProcessInputIfReady() {
            if (!post_process_pass.isReady() || !scene_color_target.isReady()) {
                return true;
            }

            return updatePostProcessInput();
        }

        bool recreateBloomPassResourcesIfReady() {
            if (!bloom_target.isReady()) {
                return true;
            }

            VulkanBloomPassContext bloom_context;
            bloom_context.device = device.device;
            bloom_context.color_format = bloom_target.getColorFormat();
            bloom_context.single_input_descriptor_set_layout =
                descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::PostProcessInput);
            bloom_context.dual_input_descriptor_set_layout =
                descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::BloomDualInput);
            bloom_context.descriptor_allocator = &descriptor_allocator;
            bloom_context.pipeline_cache = &pipeline_cache;

            return bloom_pass.recreateResources(bloom_context, bloom_target.getMipCount()) &&
                   updateBloomInputsIfReady();
        }

        bool updateBloomInputs() {
            if (!bloom_pass.isReady() || !scene_color_target.isReady() || !bloom_target.isReady()) {
                return false;
            }

            VulkanBloomInput scene_color;
            scene_color.color_view = scene_color_target.getColorImageView();
            scene_color.sampler = scene_color_target.getSampler();
            scene_color.extent = scene_color_target.getExtent();
            scene_color.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return bloom_pass.updateInputs(scene_color, bloom_target);
        }

        bool updateBloomInputsIfReady() {
            if (!bloom_pass.isReady() || !scene_color_target.isReady() || !bloom_target.isReady()) {
                return true;
            }

            return updateBloomInputs();
        }

        VulkanPostProcessInput makeScenePostProcessInput() const {
            VulkanPostProcessInput input;
            input.color_view = scene_color_target.getColorImageView();
            input.sampler = scene_color_target.getSampler();
            input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            fillPostProcessShadowInput(input);
            return input;
        }

        VulkanPostProcessInput makeBloomCompositePostProcessInput() const {
            VulkanPostProcessInput input;
            input.color_view = bloom_target.getCompositeImage().view;
            input.sampler = bloom_target.getSampler();
            input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            fillPostProcessShadowInput(input);
            return input;
        }

        VulkanPostProcessInput makeBloomDownsamplePostProcessInput(uint32_t mip_index) const {
            const VulkanBloomImageView& image = bloom_target.getDownsampleImage(mip_index);
            VulkanPostProcessInput input;
            input.color_view = image.view;
            input.sampler = bloom_target.getSampler();
            input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            fillPostProcessShadowInput(input);
            return input;
        }

        VulkanPostProcessInput makeBloomUpsamplePostProcessInput(uint32_t mip_index) const {
            const VulkanBloomImageView& image = bloom_target.getUpsampleImage(mip_index);
            VulkanPostProcessInput input;
            input.color_view = image.view;
            input.sampler = bloom_target.getSampler();
            input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            fillPostProcessShadowInput(input);
            return input;
        }

        void fillPostProcessShadowInput(VulkanPostProcessInput& input) const {
            input.shadow_view = shadow_target.getDepthImageView();
            input.shadow_sampler = shadow_target.getSampler();
            input.shadow_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            input.shadow_layer_count = shadow_target.getLayerCount();
        }

        bool shouldRenderBloom(
            const RenderPostProcessSettings& post_process_settings,
            const RenderEffectDebugSettings& debug_settings) const {
            const bool final_output =
                debug_settings.view == RenderEffectDebugView::FinalLit ||
                debug_settings.view == RenderEffectDebugView::ShadowCascades;
            const bool bloom_enabled =
                (final_output &&
                 post_process_settings.bloom_enabled &&
                 post_process_settings.bloom_intensity > 0.0f) ||
                isBloomDebugView(debug_settings.view);
            return bloom_enabled &&
                   bloom_target.isReady() &&
                   bloom_pass.isReady();
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

            if (scene_color_format == VK_FORMAT_UNDEFINED) {
                scene_color_format = findHdrSceneColorFormat(physical_device.physical_device);
                if (scene_color_format == VK_FORMAT_UNDEFINED) {
                    NX_CORE_ERROR("VulkanRendererSystem failed to find a supported HDR scene color format.");
                    return false;
                }
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
            pass_context.color_format = scene_color_format;
            pass_context.swapchain_color_format = swapchain.image_format;
            pass_context.extent = swapchain.extent;
            pass_context.color_images = swapchain_images;
            pass_context.frame_descriptor_set_layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::FrameGlobal);
            pass_context.material_descriptor_set_layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::Material);
            pass_context.environment_descriptor_set_layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::Environment);
            pass_context.pipeline_cache = &pipeline_cache;

            VulkanSkyboxPassContext skybox_context;
            skybox_context.device = device.device;
            skybox_context.color_format = scene_color_format;
            skybox_context.environment_descriptor_set_layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::Environment);
            skybox_context.pipeline_cache = &pipeline_cache;

            const bool recreated_skybox_pass = skybox_pass.recreateResources(skybox_context);
            const bool recreated_forward_pass = recreated_skybox_pass && forward_pass.recreateSwapchainResources(pass_context);
            if (!recreated_forward_pass) {
                return false;
            }

            VulkanDebugDrawPassContext debug_draw_context;
            debug_draw_context.device = device.device;
            debug_draw_context.color_format = scene_color_format;
            debug_draw_context.depth_format = forward_pass.getDepthFormat();
            debug_draw_context.frame_descriptor_set_layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::FrameGlobal);
            debug_draw_context.pipeline_cache = &pipeline_cache;
            if (!debug_draw_pass.recreateResources(debug_draw_context)) {
                return false;
            }

            if (!recreateBloomPassResourcesIfReady()) {
                return false;
            }

            VulkanPostProcessPassContext post_process_context;
            post_process_context.device = device.device;
            post_process_context.output_color_format = swapchain.image_format;
            post_process_context.input_descriptor_set_layout =
                descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::PostProcessInput);
            post_process_context.descriptor_allocator = &descriptor_allocator;
            post_process_context.pipeline_cache = &pipeline_cache;
            if (!post_process_pass.recreateResources(post_process_context) || !updatePostProcessInputIfReady()) {
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
            post_process_pass.cleanupResources();
            bloom_pass.cleanupResources();
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

        void drawFrame(const VulkanDrawList& draw_list, const RenderSettings& render_settings) {
            if (swapchain.swapchain == VK_NULL_HANDLE || swapchain_images.empty()) {
                return;
            }

            vkWaitForFences(device.device, 1, &in_flight, VK_TRUE, UINT64_MAX);

            if (!ensureShadowTarget(render_settings.shadow)) {
                return;
            }

            const RenderShadowCascadeFrame shadow_frame = buildDirectionalShadowFrame(draw_list, render_settings);
            if (!frame_lighting_resource.update(
                    draw_list,
                    shadow_frame,
                    getShadowMapSize(),
                    render_settings)) {
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

            if (!recordDrawCommands(image_index, draw_list, shadow_frame, render_settings)) {
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

        VkDescriptorSet resolveEnvironmentDescriptorSet(const VulkanDrawList& draw_list) const {
            const VulkanEnvironmentResource* environment = draw_list.environment;
            if (!environment || !environment->isReady()) {
                environment = resource_cache.getFallbackEnvironment();
            }

            return environment && environment->isReady() ?
                environment->getDescriptorSet() :
                VK_NULL_HANDLE;
        }

        bool recordDrawCommands(
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            const RenderShadowCascadeFrame& shadow_frame,
            const RenderSettings& render_settings) {
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
                buildViewportRenderGraph(graph, image_index, draw_list, shadow_frame, render_settings) :
                buildSwapchainRenderGraph(graph, image_index, draw_list, shadow_frame, render_settings);
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
            const RenderShadowCascadeFrame& shadow_frame,
            const RenderSettings& render_settings) {
            const VulkanGraphImageHandle shadow_depth = addShadowDepthImage(graph);
            const VulkanGraphImageHandle scene_color = addSceneColorImage(graph);
            const VulkanGraphImageHandle viewport_color = addViewportColorImage(graph);
            const VulkanGraphImageHandle viewport_depth = addViewportDepthImage(graph);
            const VulkanGraphImageHandle swapchain_color = addSwapchainColorImage(graph, image_index);
            if (!shadow_depth.valid() ||
                !scene_color.valid() ||
                !viewport_color.valid() ||
                !viewport_depth.valid() ||
                !swapchain_color.valid()) {
                return false;
            }

            if (!addDirectionalShadowPass(graph, shadow_depth, draw_list, shadow_frame)) {
                return false;
            }

            if (!addSkyboxPass(graph, scene_color, makeSceneSkyboxTarget(), draw_list)) {
                return false;
            }

            graph.addPass("ForwardScene")
                .readImage(shadow_depth, VulkanGraphImageUsage::ShaderRead)
                .writeImage(scene_color, VulkanGraphImageUsage::ColorAttachment)
                .writeImage(viewport_depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, &draw_list](VkCommandBuffer target_command_buffer) {
                    VulkanForwardPassRenderOptions options = forwardAfterSkyboxOptions();
                    return forward_pass.record(
                        target_command_buffer,
                        makeViewportSceneRenderTarget(),
                        draw_list,
                        frame_lighting_resource.getDescriptorSet(),
                        resolveEnvironmentDescriptorSet(draw_list),
                        options);
                });

            if (!addDebugDrawPass(graph, scene_color, viewport_depth, makeViewportSceneRenderTarget())) {
                return false;
            }

            if (!addObjectIdPass(graph, draw_list)) {
                return false;
            }

            VulkanGraphImageHandle post_process_input_color = scene_color;
            VulkanPostProcessInput post_process_input = makeScenePostProcessInput();
            if (!addBloomPass(
                    graph,
                    scene_color,
                    render_settings.post_process,
                    render_settings.effects_debug,
                    post_process_input_color,
                    post_process_input)) {
                return false;
            }

            if (!addPostProcessPass(
                    graph,
                    post_process_input_color,
                    viewport_color,
                    makeViewportPostProcessTarget(),
                    post_process_input,
                    render_settings.post_process,
                    render_settings.effects_debug)) {
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
            const RenderShadowCascadeFrame& shadow_frame,
            const RenderSettings& render_settings) {
            const VulkanGraphImageHandle shadow_depth = addShadowDepthImage(graph);
            const VulkanGraphImageHandle scene_color = addSceneColorImage(graph);
            const VulkanGraphImageHandle swapchain_color = addSwapchainColorImage(graph, image_index);
            const VulkanGraphImageHandle swapchain_depth = addSwapchainDepthImage(graph);
            if (!shadow_depth.valid() ||
                !scene_color.valid() ||
                !swapchain_color.valid() ||
                !swapchain_depth.valid()) {
                return false;
            }

            if (!addDirectionalShadowPass(graph, shadow_depth, draw_list, shadow_frame)) {
                return false;
            }

            if (!addSkyboxPass(graph, scene_color, makeSceneSkyboxTarget(), draw_list)) {
                return false;
            }

            graph.addPass("ForwardScene")
                .readImage(shadow_depth, VulkanGraphImageUsage::ShaderRead)
                .writeImage(scene_color, VulkanGraphImageUsage::ColorAttachment)
                .writeImage(swapchain_depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, image_index, &draw_list](VkCommandBuffer target_command_buffer) {
                    VulkanForwardPassRenderOptions options = forwardAfterSkyboxOptions();
                    return forward_pass.record(
                        target_command_buffer,
                        makeSwapchainSceneRenderTarget(image_index),
                        draw_list,
                        frame_lighting_resource.getDescriptorSet(),
                        resolveEnvironmentDescriptorSet(draw_list),
                        options);
                });

            if (!addDebugDrawPass(graph, scene_color, swapchain_depth, makeSwapchainSceneRenderTarget(image_index))) {
                return false;
            }

            if (!addObjectIdPass(graph, draw_list)) {
                return false;
            }

            VulkanGraphImageHandle post_process_input_color = scene_color;
            VulkanPostProcessInput post_process_input = makeScenePostProcessInput();
            if (!addBloomPass(
                    graph,
                    scene_color,
                    render_settings.post_process,
                    render_settings.effects_debug,
                    post_process_input_color,
                    post_process_input)) {
                return false;
            }

            if (!addPostProcessPass(
                    graph,
                    post_process_input_color,
                    swapchain_color,
                    makeSwapchainPostProcessTarget(image_index),
                    post_process_input,
                    render_settings.post_process,
                    render_settings.effects_debug)) {
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
            const RenderShadowCascadeFrame& shadow_frame) {
            if (!shadow_depth.valid() || !shadow_target.isReady()) {
                return false;
            }

            const uint32_t cascade_count = std::clamp(
                shadow_frame.cascade_count,
                1u,
                std::min(shadow_target.getLayerCount(), kMaxRenderShadowCascadeCount));
            graph.addPass("DirectionalShadowMap")
                .writeImage(shadow_depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, &draw_list, shadow_frame, cascade_count](VkCommandBuffer target_command_buffer) {
                    for (uint32_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index) {
                        if (!shadow_pass.record(
                                target_command_buffer,
                                shadow_target.getRenderTarget(cascade_index),
                                draw_list,
                                shadow_frame.light_view_projections[cascade_index])) {
                            return false;
                        }
                    }
                    return true;
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
                    return skybox_pass.record(
                        target_command_buffer,
                        target,
                        draw_list,
                        resolveEnvironmentDescriptorSet(draw_list));
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

        bool addBloomPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle scene_color,
            const RenderPostProcessSettings& post_process_settings,
            const RenderEffectDebugSettings& debug_settings,
            VulkanGraphImageHandle& composite_color,
            VulkanPostProcessInput& post_process_input) {
            composite_color = scene_color;
            post_process_input = makeScenePostProcessInput();
            if (!scene_color.valid()) {
                return false;
            }

            if (!shouldRenderBloom(post_process_settings, debug_settings)) {
                return true;
            }

            const uint32_t mip_count = bloom_target.getMipCount();
            if (mip_count == 0) {
                return true;
            }

            std::vector<VulkanGraphImageHandle> downsample_images;
            downsample_images.reserve(mip_count);
            for (uint32_t mip_index = 0; mip_index < mip_count; ++mip_index) {
                VulkanGraphImageHandle image = addBloomDownsampleImage(graph, mip_index);
                if (!image.valid()) {
                    return false;
                }
                downsample_images.push_back(image);
            }

            std::vector<VulkanGraphImageHandle> upsample_images;
            upsample_images.reserve(mip_count > 1 ? mip_count - 1 : 0);
            for (uint32_t mip_index = 0; mip_index + 1 < mip_count; ++mip_index) {
                VulkanGraphImageHandle image = addBloomUpsampleImage(graph, mip_index);
                if (!image.valid()) {
                    return false;
                }
                upsample_images.push_back(image);
            }

            for (uint32_t mip_index = 0; mip_index < mip_count; ++mip_index) {
                const VulkanGraphImageHandle input_color = mip_index == 0 ? scene_color : downsample_images[mip_index - 1];
                const VulkanGraphImageHandle output_color = downsample_images[mip_index];
                const VulkanBloomRenderTarget target = bloom_target.getDownsampleRenderTarget(mip_index);
                graph.addPass("BloomDownsample" + std::to_string(mip_index))
                    .readImage(input_color, VulkanGraphImageUsage::ShaderRead)
                    .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                    .execute([this, mip_index, target](VkCommandBuffer target_command_buffer) {
                        return bloom_pass.recordDownsample(target_command_buffer, mip_index, target);
                    });
            }

            for (uint32_t step = 0; step + 1 < mip_count; ++step) {
                const uint32_t mip_index = mip_count - 2u - step;
                const VulkanGraphImageHandle high_color = downsample_images[mip_index];
                const VulkanGraphImageHandle low_color = (mip_index + 1u == mip_count - 1u) ?
                    downsample_images[mip_index + 1u] :
                    upsample_images[mip_index + 1u];
                const VulkanGraphImageHandle output_color = upsample_images[mip_index];
                const VulkanBloomRenderTarget target = bloom_target.getUpsampleRenderTarget(mip_index);

                graph.addPass("BloomUpsample" + std::to_string(mip_index))
                    .readImage(high_color, VulkanGraphImageUsage::ShaderRead)
                    .readImage(low_color, VulkanGraphImageUsage::ShaderRead)
                    .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                    .execute([this, mip_index, target, post_process_settings](VkCommandBuffer target_command_buffer) {
                        return bloom_pass.recordUpsample(target_command_buffer, mip_index, target, post_process_settings);
                    });
            }

            if (debug_settings.view == RenderEffectDebugView::BloomDownsampleMip) {
                const uint32_t selected_mip = std::min(debug_settings.bloom_mip, mip_count - 1u);
                composite_color = downsample_images[selected_mip];
                post_process_input = makeBloomDownsamplePostProcessInput(selected_mip);
                return true;
            }

            if (debug_settings.view == RenderEffectDebugView::BloomUpsampleMip) {
                if (upsample_images.empty()) {
                    composite_color = downsample_images[0];
                    post_process_input = makeBloomDownsamplePostProcessInput(0);
                    return true;
                }

                const uint32_t selected_mip =
                    std::min(debug_settings.bloom_mip, static_cast<uint32_t>(upsample_images.size() - 1u));
                composite_color = upsample_images[selected_mip];
                post_process_input = makeBloomUpsamplePostProcessInput(selected_mip);
                return true;
            }

            const VulkanGraphImageHandle final_bloom_color =
                mip_count > 1 ? upsample_images[0] : downsample_images[0];
            const VulkanGraphImageHandle bloom_composite = addBloomCompositeImage(graph);
            if (!bloom_composite.valid()) {
                return false;
            }

            const VulkanBloomRenderTarget composite_target = bloom_target.getCompositeRenderTarget();
            graph.addPass("BloomComposite")
                .readImage(scene_color, VulkanGraphImageUsage::ShaderRead)
                .readImage(final_bloom_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(bloom_composite, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, composite_target, post_process_settings](VkCommandBuffer target_command_buffer) {
                    return bloom_pass.recordComposite(target_command_buffer, composite_target, post_process_settings);
                });

            composite_color = bloom_composite;
            post_process_input = makeBloomCompositePostProcessInput();
            return true;
        }

        bool addPostProcessPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle input_color,
            VulkanGraphImageHandle output_color,
            VulkanPostProcessRenderTarget target,
            const VulkanPostProcessInput& input,
            RenderPostProcessSettings post_process_settings,
            RenderEffectDebugSettings debug_settings) {
            if (!input_color.valid() || !output_color.valid() || !target.valid() || !input.valid() || !post_process_pass.isReady()) {
                return false;
            }

            if (!post_process_pass.updateInput(input)) {
                return false;
            }

            graph.addPass("PostProcess")
                .readImage(input_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, target, post_process_settings, debug_settings](VkCommandBuffer target_command_buffer) {
                    return post_process_pass.record(
                        target_command_buffer,
                        target,
                        post_process_settings,
                        debug_settings);
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

        VulkanGraphImageHandle addSceneColorImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "HDRSceneColor";
            desc.image = scene_color_target.getColorImage();
            desc.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = scene_color_target.getColorLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                scene_color_target.setColorLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addBloomDownsampleImage(VulkanPassGraph& graph, uint32_t mip_index) {
            const VulkanBloomImageView& bloom_image = bloom_target.getDownsampleImage(mip_index);
            VulkanGraphImageDesc desc;
            desc.name = "BloomDownsample" + std::to_string(mip_index);
            desc.image = bloom_image.image;
            desc.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = bloom_image.layout;
            desc.commit_layout = [this, mip_index](VkImageLayout layout) {
                bloom_target.setDownsampleLayout(mip_index, layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addBloomUpsampleImage(VulkanPassGraph& graph, uint32_t mip_index) {
            const VulkanBloomImageView& bloom_image = bloom_target.getUpsampleImage(mip_index);
            VulkanGraphImageDesc desc;
            desc.name = "BloomUpsample" + std::to_string(mip_index);
            desc.image = bloom_image.image;
            desc.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = bloom_image.layout;
            desc.commit_layout = [this, mip_index](VkImageLayout layout) {
                bloom_target.setUpsampleLayout(mip_index, layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addBloomCompositeImage(VulkanPassGraph& graph) {
            const VulkanBloomImageView& bloom_image = bloom_target.getCompositeImage();
            VulkanGraphImageDesc desc;
            desc.name = "BloomComposite";
            desc.image = bloom_image.image;
            desc.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = bloom_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                bloom_target.setCompositeLayout(layout);
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
            desc.layer_count = shadow_target.getLayerCount();
            desc.initial_layout = shadow_target.getDepthLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                shadow_target.setDepthLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanSkyboxRenderTarget makeSceneSkyboxTarget() const {
            VulkanSkyboxRenderTarget target;
            target.color_view = scene_color_target.getColorImageView();
            target.color_format = scene_color_target.getColorFormat();
            target.extent = scene_color_target.getExtent();
            return target;
        }

        VulkanRenderTarget makeViewportSceneRenderTarget() const {
            const VulkanRenderTarget depth_source = viewport_target.getRenderTarget();

            VulkanRenderTarget target;
            target.color_view = scene_color_target.getColorImageView();
            target.color_format = scene_color_target.getColorFormat();
            target.depth_view = depth_source.depth_view;
            target.depth_format = depth_source.depth_format;
            target.extent = scene_color_target.getExtent();
            return target;
        }

        VulkanRenderTarget makeSwapchainSceneRenderTarget(uint32_t image_index) const {
            const VulkanRenderTarget depth_source = forward_pass.getSwapchainRenderTarget(image_index);

            VulkanRenderTarget target;
            target.color_view = scene_color_target.getColorImageView();
            target.color_format = scene_color_target.getColorFormat();
            target.depth_view = depth_source.depth_view;
            target.depth_format = depth_source.depth_format;
            target.extent = scene_color_target.getExtent();
            return target;
        }

        VulkanPostProcessRenderTarget makeViewportPostProcessTarget() const {
            VulkanPostProcessRenderTarget target;
            target.color_view = viewport_target.getColorImageView();
            target.color_format = viewport_target.getColorFormat();
            target.extent = viewport_target.getExtent();
            return target;
        }

        VulkanPostProcessRenderTarget makeSwapchainPostProcessTarget(uint32_t image_index) const {
            VulkanPostProcessRenderTarget target;
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
            clear_value.color.float32[0] = colorForAttachment(0.08f, swapchain.image_format);
            clear_value.color.float32[1] = colorForAttachment(0.10f, swapchain.image_format);
            clear_value.color.float32[2] = colorForAttachment(0.14f, swapchain.image_format);
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
        VkFormat scene_color_format = VK_FORMAT_UNDEFINED;

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
        VulkanBloomPass bloom_pass;
        VulkanDebugDrawPass debug_draw_pass;
        VulkanShadowPass shadow_pass;
        VulkanSkyboxPass skybox_pass;
        VulkanForwardPass forward_pass;
        VulkanPostProcessPass post_process_pass;
        VulkanObjectIdPass object_id_pass;
        VulkanViewportTarget viewport_target;
        VulkanSceneColorTarget scene_color_target;
        VulkanBloomTarget bloom_target;
        VulkanPickingTarget picking_target;
        VulkanShadowMapTarget shadow_target;
        VulkanImGuiRenderer imgui_renderer;
        RendererDebugSnapshot debug_snapshot;
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

    RendererDebugSnapshot VulkanRendererSystem::getDebugSnapshot() const {
        return m_backend ? m_backend->getDebugSnapshot() : RendererDebugSnapshot();
    }

    void VulkanRendererSystem::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& resize_event) {
            m_backend->resizeSurface(resize_event.getWidth(), resize_event.getHeight());
            return false;
        });
    }
} // namespace NexAur
