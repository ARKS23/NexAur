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
#include "Function/Renderer/frontend/render_shadow_frame_builder.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list_builder.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_prepared_frame.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_graph_builder.h"
#include "Function/Renderer/Vulkan/frame/vulkan_render_feature_plan.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_executor.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"
#include "Function/Renderer/Vulkan/core/vulkan_device_context.h"
#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/core/vulkan_swapchain_manager.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/passes/vulkan_ao_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_bloom_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_debug_draw_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_forward_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_object_id_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_post_process_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_smaa_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_shadow_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_skybox_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_ssr_pass.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"
#include "Function/Renderer/Vulkan/resources/vulkan_debug_draw_buffer.h"
#include "Function/Renderer/Vulkan/resources/vulkan_frame_lighting_resource.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"
#include "Function/Renderer/Vulkan/targets/vulkan_ao_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_bloom_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_point_shadow_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_scene_color_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_shadow_map_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_smaa_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_ssr_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_viewport_target.h"
#include "Function/Renderer/Vulkan/ui/vulkan_imgui_renderer.h"
#include "Function/Renderer/Vulkan/vulkan_picking_manager.h"
#include "Function/Renderer/Vulkan/vulkan_reflection_probe_manager.h"
#include "Function/Renderer/Vulkan/vulkan_render_resource_cache.h"

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
        constexpr uint32_t kDefaultShadowMapResolution = 2048;

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

        uint32_t sanitizeShadowMapResolution(uint32_t resolution) {
            if (resolution >= 4096u) {
                return 4096u;
            }
            if (resolution >= 2048u) {
                return 2048u;
            }
            return 1024u;
        }

        uint32_t sanitizePointShadowMapResolution(uint32_t resolution) {
            if (resolution >= 1024u) {
                return 1024u;
            }
            if (resolution >= 512u) {
                return 512u;
            }
            return 256u;
        }

        uint32_t sanitizeRectShadowMapResolution(uint32_t resolution) {
            if (resolution >= 2048u) {
                return 2048u;
            }
            if (resolution >= 1024u) {
                return 1024u;
            }
            return 512u;
        }

        uint32_t sanitizeShadowCascadeCount(const RenderShadowSettings& settings) {
            if (!settings.cascades_enabled) {
                return 1u;
            }

            return std::clamp(settings.cascade_count, 1u, kMaxRenderShadowCascadeCount);
        }

        const char* antiAliasingModeToText(RenderAntiAliasingMode mode) {
            switch (mode) {
            case RenderAntiAliasingMode::None:
                return "None";
            case RenderAntiAliasingMode::SMAA:
            default:
                return "SMAA";
            }
        }

        const char* toneMappingModeToText(RenderToneMappingMode mode) {
            switch (mode) {
            case RenderToneMappingMode::None:
                return "None";
            case RenderToneMappingMode::ACES:
            default:
                return "ACES";
            }
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
            case RenderEffectDebugView::SceneDepth:
                return "Scene Depth";
            case RenderEffectDebugView::AoRaw:
                return "AO Raw";
            case RenderEffectDebugView::AoBlurred:
                return "AO Blurred";
            case RenderEffectDebugView::PointShadowMap:
                return "Point Shadow Map";
            case RenderEffectDebugView::RectShadowMap:
                return "Rect Shadow Map";
            case RenderEffectDebugView::PostToneMap:
                return "Post Tone Map";
            case RenderEffectDebugView::ColorGraded:
                return "Color Graded";
            case RenderEffectDebugView::SmaaEdgeMask:
                return "SMAA Edge Mask";
            case RenderEffectDebugView::SmaaBlendWeight:
                return "SMAA Blend Weight";
            case RenderEffectDebugView::SmaaOutput:
                return "SMAA Output";
            case RenderEffectDebugView::SsrHitMask:
                return "SSR Hit Mask";
            case RenderEffectDebugView::SsrRaySteps:
                return "SSR Ray Steps";
            case RenderEffectDebugView::SsrRawReflection:
                return "SSR Raw Reflection";
            case RenderEffectDebugView::SsrSurfaceMask:
                return "SSR Surface Mask";
            case RenderEffectDebugView::FinalLit:
            default:
                return "Final Lit";
            }
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

            GLFWwindow* native_window = static_cast<GLFWwindow*>(service.getNativeWindow());
            if (!native_window) {
                NX_CORE_ERROR("VulkanRendererSystem failed to initialize: WindowService returned null native window.");
                return false;
            }

            auto [window_width, window_height] = service.getSize();
            swapchain_manager.setSurfaceSize(
                std::max(1u, window_width),
                std::max(1u, window_height));

            if (!device_context.init(native_window, service.getRequiredVulkanInstanceExtensions()) ||
                !gpu_allocator.init(createResourceContext()) ||
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

            ao_format = VulkanDiagnosticsCollector::findAoFormat(physical_device.physical_device);
            if (ao_format == VK_FORMAT_UNDEFINED) {
                NX_CORE_ERROR("VulkanRendererSystem failed to find a supported AO target format.");
                shutdown();
                return false;
            }
            ssr_hit_mask_format = ao_format;
            smaa_mask_format = VulkanDiagnosticsCollector::findSmaaMaskFormat(physical_device.physical_device);
            if (smaa_mask_format == VK_FORMAT_UNDEFINED) {
                NX_CORE_ERROR("VulkanRendererSystem failed to find a supported SMAA mask target format.");
                shutdown();
                return false;
            }

            if (!viewport_target.init(createResourceContext(), swapchain.image_format, surface_width, surface_height)) {
                shutdown();
                return false;
            }

            if (!scene_color_target.init(createResourceContext(), scene_color_format, surface_width, surface_height) ||
                !ao_target.init(createResourceContext(), ao_format, surface_width, surface_height, RenderSettings().ao.half_resolution) ||
                !bloom_target.init(createResourceContext(), scene_color_format, surface_width, surface_height) ||
                !ssr_target.init(createResourceContext(), scene_color_format, ssr_hit_mask_format, surface_width, surface_height) ||
                !smaa_target.init(createResourceContext(), swapchain.image_format, smaa_mask_format, surface_width, surface_height) ||
                !recreateAoPassResourcesIfReady() ||
                !recreateBloomPassResourcesIfReady() ||
                !recreateSsrPassResourcesIfReady() ||
                !recreateSmaaPassResourcesIfReady()) {
                shutdown();
                return false;
            }

            if (!picking_manager.init(
                    createResourceContext(),
                    command_pool,
                    surface_width,
                    surface_height)) {
                shutdown();
                return false;
            }

            if (!reflection_probe_manager.init(
                    createResourceContext(),
                    command_pool,
                    scene_color_format,
                    forward_pass.getDepthFormat())) {
                shutdown();
                return false;
            }
            reflection_probe_capture_callbacks = createReflectionProbeCaptureCallbacks();

            if (!shadow_target.init(createResourceContext(), kDefaultShadowMapResolution) ||
                !point_shadow_target.init(createResourceContext(), RenderSettings().point_shadow.map_resolution, 1u) ||
                !rect_shadow_target.init(createResourceContext(), RenderSettings().rect_shadow.map_resolution, 1u) ||
                !frame_lighting_resource.updateShadowMap(shadow_target.getDepthImageView(), shadow_target.getSampler()) ||
                !frame_lighting_resource.updatePointShadowMap(point_shadow_target.getDepthImageView(), point_shadow_target.getSampler()) ||
                !frame_lighting_resource.updateRectShadowMap(rect_shadow_target.getDepthImageView(), rect_shadow_target.getSampler()) ||
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
            object_id_context.object_id_format = picking_manager.getObjectIdFormat();
            object_id_context.depth_format = picking_manager.getDepthFormat();
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
            smaa_pass.shutdown();
            post_process_pass.shutdown();
            bloom_pass.shutdown();
            ssr_pass.shutdown();
            ao_pass.shutdown();
            debug_draw_pass.shutdown();
            skybox_pass.shutdown();
            shadow_pass.shutdown();
            object_id_pass.shutdown();
            reflection_probe_manager.shutdown();
            reflection_probe_capture_callbacks = {};
            picking_manager.shutdown();
            rect_shadow_target.shutdown();
            point_shadow_target.shutdown();
            shadow_target.shutdown();
            smaa_target.shutdown();
            ssr_target.shutdown();
            bloom_target.shutdown();
            ao_target.shutdown();
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

            gpu_allocator.shutdown();
            device_context.shutdown();
            scene_color_format = VK_FORMAT_UNDEFINED;
            ao_format = VK_FORMAT_UNDEFINED;
            ssr_hit_mask_format = VK_FORMAT_UNDEFINED;
            smaa_mask_format = VK_FORMAT_UNDEFINED;
            initialized = false;
        }

        void render(TimeStep ts, const RenderDataPacket& render_data) {
            const auto render_start_time = std::chrono::steady_clock::now();
            const auto build_scene_frame = [&]() {
                const auto [render_width, render_height] = getViewportRenderExtent();
                return scene_frame_builder.buildRenderSceneFrame(
                    render_data,
                    render_width,
                    render_height);
            };

            if (!initialized || surface_width == 0 || surface_height == 0) {
                const RenderSceneFrame scene_frame = build_scene_frame();
                const VulkanRenderFeaturePlan feature_plan =
                    buildRenderFeaturePlan(scene_frame.render_settings);
                last_output_route = feature_plan.getOutputRoute();
                updateDebugSnapshot(ts, scene_frame, nullptr, feature_plan, render_start_time);
                return;
            }

            if (swapchain_dirty && !recreateSwapchain()) {
                const RenderSceneFrame scene_frame = build_scene_frame();
                const VulkanRenderFeaturePlan feature_plan =
                    buildRenderFeaturePlan(scene_frame.render_settings);
                last_output_route = feature_plan.getOutputRoute();
                updateDebugSnapshot(ts, scene_frame, nullptr, feature_plan, render_start_time);
                return;
            }

            VulkanPreparedFrame prepared_frame;
            prepared_frame.scene = build_scene_frame();
            const VulkanRenderView vulkan_view = translator.buildRenderView(prepared_frame.scene.view);
            prepared_frame.draw_list = draw_list_builder.buildDrawList(
                prepared_frame.scene,
                vulkan_view,
                resource_cache,
                AssetManager::getInstance());

            waitForInFlightFrame();
            reflection_probe_manager.processFrame(
                prepared_frame,
                resource_cache,
                AssetManager::getInstance(),
                reflection_probe_capture_callbacks);
            if (!prepareFrameTargets(prepared_frame)) {
                const VulkanRenderFeaturePlan feature_plan =
                    buildRenderFeaturePlan(prepared_frame.scene.render_settings);
                last_output_route = feature_plan.getOutputRoute();
                updateDebugSnapshot(
                    ts,
                    prepared_frame.scene,
                    &prepared_frame.draw_list,
                    feature_plan,
                    render_start_time);
                return;
            }

            const VulkanRenderFeaturePlan feature_plan =
                buildRenderFeaturePlan(prepared_frame.scene.render_settings);
            last_output_route = feature_plan.getOutputRoute();
            drawFrame(prepared_frame, feature_plan);
            updateDebugSnapshot(
                ts,
                prepared_frame.scene,
                &prepared_frame.draw_list,
                feature_plan,
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
            const bool ao_resized = ao_target.resize(width, height, RenderSettings().ao.half_resolution);
            const bool bloom_resized = bloom_target.resize(width, height);
            const bool ssr_resized = ssr_target.resize(width, height);
            const bool smaa_resized = smaa_target.resize(width, height);
            const bool picking_resized = picking_manager.resize(width, height);
            if (!viewport_resized ||
                !scene_color_resized ||
                !ao_resized ||
                !bloom_resized ||
                !ssr_resized ||
                !smaa_resized ||
                !picking_resized ||
                !recreateAoPassResourcesIfReady() ||
                !recreateBloomPassResourcesIfReady() ||
                !recreateSsrPassResourcesIfReady() ||
                !recreateSmaaPassResourcesIfReady() ||
                !updatePostProcessInput()) {
                NX_CORE_ERROR("VulkanRendererSystem failed to resize viewport, HDR scene color, AO, bloom, SSR, SMAA, or picking target.");
                return;
            }
            if (imgui_renderer.isInitialized()) {
                registerViewportTexture();
            }
        }

        std::pair<uint32_t, uint32_t> getViewportSize() const {
            return getViewportRenderExtent();
        }

        ViewportPickResult pickViewport(const ViewportPickRequest& request) {
            if (!initialized) {
                ViewportPickResult result;
                result.supported = true;
                return result;
            }
            return picking_manager.pickViewport(request);
        }

        ViewportOutput getViewportOutput() const {
            ViewportOutput output;
            output.backend = RendererBackendType::Vulkan;
            output.coordinate_origin = ViewportCoordinateOrigin::TopLeft;

            if (!initialized) {
                return output;
            }

            if (last_output_route == VulkanFrameOutputRoute::Viewport) {
                if (viewport_target.isReady() && imgui_renderer.hasViewportTexture()) {
                    const VkExtent2D extent = viewport_target.getExtent();
                    output.kind = ViewportOutputKind::VulkanImGuiTexture;
                    output.width = extent.width;
                    output.height = extent.height;
                    output.native_handle = imgui_renderer.getViewportTextureHandle();
                }
                return output;
            }

            output.kind = ViewportOutputKind::ExternalSwapchain;
            auto [surface_render_width, surface_render_height] = getRenderExtent();
            output.width = surface_render_width;
            output.height = surface_render_height;
            return output;
        }

        void resizeSurface(uint32_t width, uint32_t height) {
            swapchain_manager.setSurfaceSize(width, height);

            if (width == 0 || height == 0) {
                return;
            }

            swapchain_dirty = initialized;
        }

        void onUIContextInitialized() {
            if (!initImGuiRenderer()) {
                NX_CORE_WARN("VulkanRendererSystem could not initialize the ImGui Vulkan renderer backend yet.");
                return;
            }

            last_output_route =
                buildRenderFeaturePlan(RenderSettings{}).getOutputRoute();
        }

        void beginUIFrame() {
            imgui_renderer.beginFrame();
        }

        void onUIContextShutdown() {
            if (device.device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device.device);
            }
            imgui_renderer.shutdown();
            last_output_route =
                buildRenderFeaturePlan(RenderSettings{}).getOutputRoute();
        }

        RendererDebugSnapshot getDebugSnapshot() const {
            return debug_snapshot;
        }

        bool requestReflectionProbeCapture(const ReflectionProbeCaptureRequest& request) {
            return initialized && reflection_probe_manager.requestCapture(request);
        }

        bool clearReflectionProbeCapture(int entity_id) {
            return reflection_probe_manager.clearCapture(entity_id);
        }

        ReflectionProbeCaptureState getReflectionProbeCaptureState(int entity_id) const {
            return reflection_probe_manager.getCaptureState(entity_id);
        }

        ReflectionProbeCaptureQueueState getReflectionProbeCaptureQueueState() const {
            return reflection_probe_manager.getQueueState();
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
            context.gpu_allocator = &gpu_allocator;
            return context;
        }

        void waitForInFlightFrame() const {
            if (device.device == VK_NULL_HANDLE || in_flight == VK_NULL_HANDLE) {
                return;
            }

            vkWaitForFences(device.device, 1, &in_flight, VK_TRUE, UINT64_MAX);
        }

        void transitionDepthImageToAttachment(
            VkCommandBuffer target_command_buffer,
            VkImage image,
            VkImageLayout old_layout,
            uint32_t layer_count) {
            VkAccessFlags src_access = 0;
            VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                src_access = VK_ACCESS_SHADER_READ_BIT;
                src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }

            transitionImageLayout(
                target_command_buffer,
                image,
                old_layout,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                src_access,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                src_stage,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,
                layer_count);
        }

        void transitionDepthImageToShaderRead(
            VkCommandBuffer target_command_buffer,
            VkImage image,
            VkImageLayout old_layout,
            uint32_t layer_count) {
            VkAccessFlags src_access = 0;
            VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            if (old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                src_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                src_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }

            transitionImageLayout(
                target_command_buffer,
                image,
                old_layout,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                src_access,
                VK_ACCESS_SHADER_READ_BIT,
                src_stage,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                layer_count);
        }

        bool recordCaptureShadowMaps(
            VkCommandBuffer target_command_buffer,
            const VulkanDrawList& capture_draw_list,
            const RenderShadowCascadeFrame& shadow_frame,
            const RenderPointShadowFrame& point_shadow_frame,
            const RenderRectShadowFrame& rect_shadow_frame,
            const RenderSettings& capture_settings) {
            if (shadow_target.isReady()) {
                const bool directional_shadow_enabled =
                    capture_settings.shadow.enabled &&
                    capture_draw_list.directional_light.cast_shadow;
                if (directional_shadow_enabled) {
                    transitionDepthImageToAttachment(
                        target_command_buffer,
                        shadow_target.getDepthImage(),
                        shadow_target.getDepthLayout(),
                        shadow_target.getLayerCount());
                    shadow_target.setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

                    const uint32_t cascade_count = std::clamp(
                        shadow_frame.cascade_count,
                        1u,
                        std::min(shadow_target.getLayerCount(), kMaxRenderShadowCascadeCount));
                    for (uint32_t cascade_index = 0; cascade_index < cascade_count; ++cascade_index) {
                        if (!shadow_pass.record(
                                target_command_buffer,
                                shadow_target.getRenderTarget(cascade_index),
                                capture_draw_list,
                                shadow_frame.light_view_projections[cascade_index])) {
                            return false;
                        }
                    }
                }

                transitionDepthImageToShaderRead(
                    target_command_buffer,
                    shadow_target.getDepthImage(),
                    shadow_target.getDepthLayout(),
                    shadow_target.getLayerCount());
                shadow_target.setDepthLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            if (point_shadow_target.isReady()) {
                if (point_shadow_frame.enabled && point_shadow_frame.face_count > 0) {
                    transitionDepthImageToAttachment(
                        target_command_buffer,
                        point_shadow_target.getDepthImage(),
                        point_shadow_target.getDepthLayout(),
                        point_shadow_target.getLayerCount());
                    point_shadow_target.setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

                    const uint32_t face_count = std::min(
                        point_shadow_frame.face_count,
                        point_shadow_target.getLayerCount());
                    for (uint32_t layer_index = 0; layer_index < face_count; ++layer_index) {
                        if (!shadow_pass.record(
                                target_command_buffer,
                                point_shadow_target.getRenderTarget(layer_index),
                                capture_draw_list,
                                point_shadow_frame.light_view_projections[layer_index])) {
                            return false;
                        }
                    }
                }

                transitionDepthImageToShaderRead(
                    target_command_buffer,
                    point_shadow_target.getDepthImage(),
                    point_shadow_target.getDepthLayout(),
                    point_shadow_target.getLayerCount());
                point_shadow_target.setDepthLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            if (rect_shadow_target.isReady()) {
                if (rect_shadow_frame.enabled && rect_shadow_frame.shadowed_light_count > 0) {
                    transitionDepthImageToAttachment(
                        target_command_buffer,
                        rect_shadow_target.getDepthImage(),
                        rect_shadow_target.getDepthLayout(),
                        rect_shadow_target.getLayerCount());
                    rect_shadow_target.setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

                    const uint32_t layer_count = std::min(
                        rect_shadow_frame.shadowed_light_count,
                        rect_shadow_target.getLayerCount());
                    for (uint32_t layer_index = 0; layer_index < layer_count; ++layer_index) {
                        if (!shadow_pass.record(
                                target_command_buffer,
                                rect_shadow_target.getRenderTarget(layer_index),
                                capture_draw_list,
                                rect_shadow_frame.light_view_projections[layer_index])) {
                            return false;
                        }
                    }
                }

                transitionDepthImageToShaderRead(
                    target_command_buffer,
                    rect_shadow_target.getDepthImage(),
                    rect_shadow_target.getDepthLayout(),
                    rect_shadow_target.getLayerCount());
                rect_shadow_target.setDepthLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            return true;
        }

        VulkanReflectionProbeCaptureCallbacks createReflectionProbeCaptureCallbacks() {
            VulkanReflectionProbeCaptureCallbacks callbacks;
            callbacks.prepare = [this](
                                    const RenderSettings& capture_settings,
                                    std::string& error_message) {
                if (ensureShadowTarget(capture_settings.shadow) &&
                    ensurePointShadowTarget(capture_settings.point_shadow) &&
                    ensureRectShadowTarget(capture_settings.rect_shadow)) {
                    return true;
                }

                error_message =
                    "Failed to prepare shadow targets for reflection probe capture.";
                return false;
            };
            callbacks.record_shadows = [this](
                                           VkCommandBuffer target_command_buffer,
                                           const RenderView& capture_view,
                                           const VulkanDrawList& capture_draw_list,
                                           const RenderSettings& capture_settings,
                                           std::string& error_message) {
                const RenderShadowCascadeFrame shadow_frame = shadow_target.isReady() ?
                    shadow_frame_builder.buildDirectionalShadowFrame(
                        capture_view,
                        capture_draw_list.directional_light,
                        capture_settings.shadow,
                        capture_settings.effects_debug,
                        getShadowMapSize()) :
                    RenderShadowCascadeFrame{};
                const RenderPointShadowFrame point_shadow_frame =
                    shadow_frame_builder.buildPointShadowFrame(
                        capture_draw_list.point_lights,
                        capture_settings.point_shadow,
                        point_shadow_target.isReady() ?
                            point_shadow_target.getShadowedLightCapacity() :
                            0u);
                const RenderRectShadowFrame rect_shadow_frame =
                    shadow_frame_builder.buildRectShadowFrame(
                        capture_draw_list.rect_lights,
                        capture_settings.rect_shadow,
                        rect_shadow_target.isReady() ?
                            rect_shadow_target.getLayerCount() :
                            0u);
                if (!frame_lighting_resource.update(
                        capture_draw_list,
                        shadow_frame,
                        point_shadow_frame,
                        rect_shadow_frame,
                        getShadowMapSize(),
                        getPointShadowMapSize(),
                        getRectShadowMapSize(),
                        capture_settings)) {
                    error_message =
                        "Failed to update frame globals for reflection probe capture.";
                    return false;
                }

                if (!recordCaptureShadowMaps(
                        target_command_buffer,
                        capture_draw_list,
                        shadow_frame,
                        point_shadow_frame,
                        rect_shadow_frame,
                        capture_settings)) {
                    error_message =
                        "Failed to record shadow maps for reflection probe capture.";
                    return false;
                }

                return true;
            };
            callbacks.record_face = [this](
                                        VkCommandBuffer target_command_buffer,
                                        const VulkanDrawList& capture_draw_list,
                                        VulkanRenderTarget face_target,
                                        bool include_skybox,
                                        std::string& error_message) {

                if (!face_target.valid()) {
                    error_message = "Reflection probe capture target face was invalid.";
                    return false;
                }

                VulkanForwardPassRenderOptions options;
                options.color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
                options.depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
                options.color_clear_value.color.float32[3] = 1.0f;
                options.depth_clear_value.depthStencil.depth = 1.0f;
                if (include_skybox) {
                    VulkanSkyboxRenderTarget skybox_target;
                    skybox_target.color_view = face_target.color_view;
                    skybox_target.color_format = face_target.color_format;
                    skybox_target.extent = face_target.extent;
                    if (!skybox_pass.record(
                            target_command_buffer,
                            skybox_target,
                            capture_draw_list,
                            resolveEnvironmentDescriptorSet(capture_draw_list))) {
                        error_message =
                            "Failed to record skybox for reflection probe capture.";
                        return false;
                    }
                    options = forwardAfterSkyboxOptions();
                }

                if (!forward_pass.record(
                        target_command_buffer,
                        face_target,
                        capture_draw_list,
                        frame_lighting_resource.getDescriptorSet(),
                        resolveEnvironmentDescriptorSet(capture_draw_list),
                        resolveReflectionProbeDescriptorSet(capture_draw_list),
                        options)) {
                    error_message =
                        "Failed to record forward pass for reflection probe capture.";
                    return false;
                }
                return true;
            };
            return callbacks;
        }

        VulkanRenderFeaturePlan buildRenderFeaturePlan(const RenderSettings& render_settings) const {
            VulkanRenderFeatureAvailability availability;
            availability.viewport_output =
                viewport_target.isReady() &&
                imgui_renderer.isInitialized();
            availability.post_process = post_process_pass.isReady();
            availability.bloom = bloom_target.isReady() && bloom_pass.isReady();
            availability.ao = ao_target.isReady() && ao_pass.isReady();
            availability.ssr = ssr_target.isReady() && ssr_pass.isReady();
            availability.smaa = smaa_target.isReady() && smaa_pass.isReady();
            availability.directional_shadow = shadow_target.isReady();
            availability.point_shadow = point_shadow_target.isReady();
            availability.rect_shadow = rect_shadow_target.isReady();
            return VulkanRenderFeaturePlan::build(render_settings, availability);
        }

        void updateDebugSnapshot(
            TimeStep ts,
            const RenderSceneFrame& scene_frame,
            const VulkanDrawList* draw_list,
            const VulkanRenderFeaturePlan& feature_plan,
            std::chrono::steady_clock::time_point render_start_time) {
            RendererDebugSnapshot snapshot;
            snapshot.backend = buildBackendDebugStats();
            snapshot.frame = buildFrameDebugStats(
                ts,
                scene_frame,
                draw_list,
                render_start_time);
            snapshot.view = buildViewDebugStats(scene_frame.view);
            snapshot.viewport_target = buildViewportTargetDebugStats();
            snapshot.hdr_scene_target = buildHdrSceneTargetDebugStats();
            snapshot.picking_target = buildPickingTargetDebugStats();
            snapshot.shadow_target = buildShadowTargetDebugStats();
            snapshot.point_shadow_target = buildPointShadowTargetDebugStats();
            snapshot.rect_shadow_target = buildRectShadowTargetDebugStats();
            snapshot.post_process = buildPostProcessDebugStats(scene_frame.render_settings, feature_plan);
            snapshot.bloom = buildBloomDebugStats(feature_plan);
            snapshot.ao = buildAoDebugStats(feature_plan);
            snapshot.ssr = buildSsrDebugStats(scene_frame.render_settings, feature_plan);
            snapshot.smaa = buildSmaaDebugStats(scene_frame.render_settings, feature_plan);
            snapshot.effects = buildEffectsDebugStats(scene_frame.render_settings, feature_plan);
            snapshot.resources = buildResourceDebugStats(draw_list);

            debug_snapshot = std::move(snapshot);
        }

        RendererDebugBackendStats buildBackendDebugStats() const {
            RendererDebugBackendStats stats;
            stats.backend = RendererBackendType::Vulkan;
            stats.initialized = initialized;
            stats.device_api_version = VulkanDiagnosticsCollector::apiVersionToString(device_api_version);
            stats.swapchain_ready = swapchain.swapchain != VK_NULL_HANDLE && !swapchain_images.empty();
            stats.swapchain_width = swapchain.extent.width;
            stats.swapchain_height = swapchain.extent.height;
            stats.swapchain_image_count = static_cast<uint32_t>(swapchain_images.size());
            stats.swapchain_format = VulkanDiagnosticsCollector::vkFormatToString(swapchain.image_format);
            stats.viewport_output_kind = getViewportOutput().kind;
            return stats;
        }

        RendererDebugFrameStats buildFrameDebugStats(
            TimeStep ts,
            const RenderSceneFrame& scene_frame,
            const VulkanDrawList* draw_list,
            std::chrono::steady_clock::time_point render_start_time) const {
            RendererDebugFrameStats stats;
            stats.engine_delta_ms = ts.GetMilliseconds();
            stats.renderer_cpu_ms =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - render_start_time).count();

            stats.opaque_object_count = draw_list ?
                scene_frame.opaque_objects.size() :
                scene_frame.source_counts.opaque_object_count;
            stats.transparent_object_count = draw_list ?
                scene_frame.transparent_objects.size() :
                scene_frame.source_counts.transparent_object_count;
            stats.opaque_draw_item_count = draw_list ? draw_list->opaque_items.size() : 0;
            stats.transparent_draw_item_count = draw_list ? draw_list->transparent_items.size() : 0;
            stats.point_light_count = draw_list ?
                draw_list->point_lights.size() :
                scene_frame.source_counts.point_light_count;
            stats.rect_light_count = draw_list ?
                draw_list->rect_lights.size() :
                scene_frame.source_counts.rect_light_count;
            stats.reflection_probe_count = draw_list ?
                draw_list->reflection_probes.size() :
                scene_frame.source_counts.reflection_probe_count;
            stats.active_reflection_probe =
                draw_list && draw_list->active_reflection_probe.enabled;
            const size_t extracted_rect_light_count = scene_frame.rect_lights.size();
            stats.rect_light_clipped_count =
                scene_frame.source_counts.rect_light_count > extracted_rect_light_count ?
                    scene_frame.source_counts.rect_light_count - extracted_rect_light_count :
                    0u;
            if (draw_list) {
                for (const RenderFramePointLight& light : draw_list->point_lights) {
                    if (light.shadow_requested) {
                        ++stats.point_shadow_request_count;
                    }
                    if (light.cast_shadow && light.shadow_slot >= 0) {
                        ++stats.shadowed_point_light_count;
                    }
                }
            } else {
                stats.point_shadow_request_count =
                    scene_frame.source_counts.point_shadow_request_count;
            }
            if (draw_list) {
                for (const RenderFrameRectLight& light : draw_list->rect_lights) {
                    if (light.shadow_requested) {
                        ++stats.rect_shadow_request_count;
                    }
                    if (light.cast_shadow && light.shadow_slot >= 0) {
                        ++stats.shadowed_rect_light_count;
                    }
                }
            } else {
                stats.rect_shadow_request_count =
                    scene_frame.source_counts.rect_shadow_request_count;
            }
            stats.debug_line_count = draw_list ?
                draw_list->debug_draw.lines.size() :
                scene_frame.source_counts.debug_line_count;
            return stats;
        }

        RendererDebugViewStats buildViewDebugStats(const RenderView& render_view) const {
            RendererDebugViewStats stats;
            stats.viewport_width = render_view.viewport_width;
            stats.viewport_height = render_view.viewport_height;
            stats.camera_position = render_view.camera_position;
            stats.near_clip = render_view.near_clip;
            stats.far_clip = render_view.far_clip;
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
            stats.color_format = VulkanDiagnosticsCollector::vkFormatToString(viewport_target.getColorFormat());
            stats.depth_format = VulkanDiagnosticsCollector::vkFormatToString(viewport_target.getDepthFormat());
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
            stats.color_format = VulkanDiagnosticsCollector::vkFormatToString(scene_color_target.getColorFormat());
            stats.depth_format = "Shared";
            return stats;
        }

        RendererDebugPickingTargetStats buildPickingTargetDebugStats() const {
            return picking_manager.buildDebugStats();
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
            stats.depth_format = VulkanDiagnosticsCollector::vkFormatToString(shadow_target.getDepthFormat());
            return stats;
        }

        RendererDebugShadowTargetStats buildPointShadowTargetDebugStats() const {
            RendererDebugShadowTargetStats stats;
            stats.ready = point_shadow_target.isReady();
            if (!stats.ready) {
                return stats;
            }

            const VkExtent2D extent = point_shadow_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.layer_count = point_shadow_target.getLayerCount();
            stats.depth_format = VulkanDiagnosticsCollector::vkFormatToString(point_shadow_target.getDepthFormat());
            return stats;
        }

        RendererDebugShadowTargetStats buildRectShadowTargetDebugStats() const {
            RendererDebugShadowTargetStats stats;
            stats.ready = rect_shadow_target.isReady();
            if (!stats.ready) {
                return stats;
            }

            const VkExtent2D extent = rect_shadow_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.layer_count = rect_shadow_target.getLayerCount();
            stats.depth_format = VulkanDiagnosticsCollector::vkFormatToString(rect_shadow_target.getDepthFormat());
            return stats;
        }

        RendererDebugPostProcessStats buildPostProcessDebugStats(
            const RenderSettings& render_settings,
            const VulkanRenderFeaturePlan& feature_plan) const {
            RendererDebugPostProcessStats stats;
            stats.enabled = feature_plan.getAvailability().post_process;
            stats.ready = feature_plan.getAvailability().post_process;
            if (post_process_pass.getOutputColorFormat() != VK_FORMAT_UNDEFINED) {
                stats.output_format = VulkanDiagnosticsCollector::vkFormatToString(post_process_pass.getOutputColorFormat());
            }
            stats.tone_mapping = toneMappingModeToText(render_settings.post_process.tone_mapping_mode);
            stats.exposure = render_settings.post_process.exposure;
            stats.bloom_enabled = feature_plan.rendersBloom();
            stats.bloom_intensity = render_settings.post_process.bloom_intensity;
            stats.color_grading_enabled = render_settings.post_process.color_grading_enabled;
            stats.color_grading_exposure_offset = render_settings.post_process.color_grading_exposure_offset;
            stats.color_grading_contrast = render_settings.post_process.color_grading_contrast;
            stats.color_grading_saturation = render_settings.post_process.color_grading_saturation;
            stats.color_grading_temperature = render_settings.post_process.color_grading_temperature;
            stats.color_grading_tint = render_settings.post_process.color_grading_tint;
            stats.color_grading_black_point = render_settings.post_process.color_grading_black_point;
            stats.color_grading_white_point = render_settings.post_process.color_grading_white_point;
            stats.vignette_intensity = render_settings.post_process.vignette_intensity;
            stats.sharpen_intensity = render_settings.post_process.sharpen_intensity;
            return stats;
        }

        RendererDebugBloomStats buildBloomDebugStats(const VulkanRenderFeaturePlan& feature_plan) const {
            RendererDebugBloomStats stats;
            stats.enabled = feature_plan.rendersBloom();
            stats.ready = feature_plan.getAvailability().bloom;
            if (!bloom_target.isReady()) {
                return stats;
            }

            const VkExtent2D extent = bloom_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.mip_count = bloom_target.getMipCount();
            stats.color_format = VulkanDiagnosticsCollector::vkFormatToString(bloom_target.getColorFormat());
            return stats;
        }

        RendererDebugAoStats buildAoDebugStats(const VulkanRenderFeaturePlan& feature_plan) const {
            RendererDebugAoStats stats;
            stats.enabled = feature_plan.rendersAo();
            stats.ready = feature_plan.getAvailability().ao;
            if (!ao_target.isReady()) {
                return stats;
            }

            const VkExtent2D extent = ao_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.color_format = VulkanDiagnosticsCollector::vkFormatToString(ao_target.getColorFormat());
            stats.half_resolution = ao_target.isHalfResolution();
            return stats;
        }

        RendererDebugSsrStats buildSsrDebugStats(
            const RenderSettings& render_settings,
            const VulkanRenderFeaturePlan& feature_plan) const {
            RendererDebugSsrStats stats;
            stats.enabled = feature_plan.rendersSsr();
            stats.ready = feature_plan.getAvailability().ssr;
            stats.max_distance = render_settings.ssr.max_distance;
            stats.max_steps = render_settings.ssr.max_steps;
            stats.thickness = render_settings.ssr.thickness;
            stats.stride = render_settings.ssr.stride;
            stats.roughness_fade = render_settings.ssr.roughness_fade;
            stats.edge_fade = render_settings.ssr.edge_fade;
            stats.intensity = render_settings.ssr.intensity;
            if (!ssr_target.isReady()) {
                return stats;
            }

            const VkExtent2D extent = ssr_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.reflection_format = VulkanDiagnosticsCollector::vkFormatToString(ssr_target.getReflectionFormat());
            stats.hit_mask_format = VulkanDiagnosticsCollector::vkFormatToString(ssr_target.getHitMaskFormat());
            return stats;
        }

        RendererDebugSmaaStats buildSmaaDebugStats(
            const RenderSettings& render_settings,
            const VulkanRenderFeaturePlan& feature_plan) const {
            RendererDebugSmaaStats stats;
            stats.enabled = feature_plan.rendersSmaa();
            stats.ready = feature_plan.getAvailability().smaa;
            stats.mode = antiAliasingModeToText(render_settings.anti_aliasing.mode);
            stats.edge_threshold = render_settings.anti_aliasing.smaa_edge_threshold;
            stats.contrast_factor = render_settings.anti_aliasing.smaa_contrast_factor;
            stats.max_search_steps = render_settings.anti_aliasing.smaa_max_search_steps;
            stats.blend_strength = render_settings.anti_aliasing.smaa_blend_strength;
            if (!smaa_target.isReady()) {
                return stats;
            }

            const VkExtent2D extent = smaa_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.source_format = VulkanDiagnosticsCollector::vkFormatToString(smaa_target.getSourceFormat());
            stats.edge_format = VulkanDiagnosticsCollector::vkFormatToString(smaa_target.getMaskFormat());
            stats.blend_format = VulkanDiagnosticsCollector::vkFormatToString(smaa_target.getMaskFormat());
            return stats;
        }

        RendererDebugEffectsStats buildEffectsDebugStats(
            const RenderSettings& render_settings,
            const VulkanRenderFeaturePlan& feature_plan) const {
            RendererDebugEffectsStats stats;
            stats.lighting_preset = renderLightingPresetName(render_settings.lighting.preset);
            const RenderEffectDebugSettings& debug_settings = feature_plan.getDebugSettings();
            const VulkanRenderFeatureAvailability& availability = feature_plan.getAvailability();
            stats.debug_view = effectDebugViewToText(debug_settings.view);
            stats.bloom_mip = debug_settings.bloom_mip;
            stats.shadow_cascade = debug_settings.shadow_cascade;
            stats.point_shadow_layer = debug_settings.point_shadow_layer;
            stats.rect_shadow_layer = debug_settings.rect_shadow_layer;
            stats.bloom_debug_available = availability.bloom;
            stats.ao_debug_available = availability.ao;
            stats.ssr_debug_available = availability.ssr;
            stats.smaa_debug_available = availability.smaa;
            stats.shadow_debug_available = availability.directional_shadow;
            stats.point_shadow_debug_available = availability.point_shadow;
            stats.rect_shadow_debug_available = availability.rect_shadow;
            stats.point_shadow_enabled = render_settings.point_shadow.enabled;
            stats.rect_shadow_enabled = render_settings.rect_shadow.enabled;
            stats.contact_shadow_enabled = render_settings.contact_shadow.enabled;
            stats.point_shadow_budget = std::clamp(
                render_settings.point_shadow.max_shadowed_lights,
                0u,
                kMaxRenderPointShadowLights);
            stats.rect_shadow_budget = std::clamp(
                render_settings.rect_shadow.max_shadowed_lights,
                0u,
                kMaxRenderRectShadowLights);
            stats.point_shadow_map_resolution = render_settings.point_shadow.map_resolution;
            stats.rect_shadow_map_resolution = render_settings.rect_shadow.map_resolution;
            stats.point_shadow_strength = render_settings.point_shadow.strength;
            stats.point_shadow_bias = render_settings.point_shadow.constant_bias;
            stats.point_shadow_normal_bias = render_settings.point_shadow.normal_bias;
            stats.point_shadow_filter_radius = render_settings.point_shadow.filter_radius;
            stats.rect_shadow_strength = render_settings.rect_shadow.strength;
            stats.rect_shadow_bias = render_settings.rect_shadow.constant_bias;
            stats.rect_shadow_normal_bias = render_settings.rect_shadow.normal_bias;
            stats.rect_shadow_filter_radius = render_settings.rect_shadow.filter_radius;
            stats.rect_shadow_projection_margin = render_settings.rect_shadow.projection_margin;
            stats.rect_shadow_soft_enabled = render_settings.rect_shadow.soft_shadow_enabled;
            stats.rect_shadow_pcss_light_radius = render_settings.rect_shadow.pcss_light_radius;
            stats.rect_shadow_pcss_search_radius = render_settings.rect_shadow.pcss_search_radius;
            stats.rect_shadow_pcss_min_filter_radius = render_settings.rect_shadow.pcss_min_filter_radius;
            stats.rect_shadow_pcss_max_filter_radius = render_settings.rect_shadow.pcss_max_filter_radius;
            stats.rect_shadow_pcss_blocker_taps = std::clamp(
                render_settings.rect_shadow.pcss_blocker_taps,
                1u,
                16u);
            stats.rect_shadow_pcss_filter_taps = std::clamp(
                render_settings.rect_shadow.pcss_filter_taps,
                1u,
                16u);
            stats.rect_ltc_specular_enabled = render_settings.rect_light.ltc_specular_enabled;
            stats.rect_ltc_debug_only = render_settings.rect_light.debug_ltc_only;
            stats.rect_ltc_specular_scale = render_settings.rect_light.specular_intensity_scale;
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
            const VulkanActiveReflectionProbe* active_probe =
                draw_list && draw_list->active_reflection_probe.enabled ?
                    &draw_list->active_reflection_probe :
                    nullptr;
            const VulkanEnvironmentResource* active_probe_environment =
                active_probe ? active_probe->environment : nullptr;
            if (active_probe_environment && active_probe_environment->isReady()) {
                stats.active_reflection_probe_ready = true;
                stats.active_reflection_probe_name = active_probe_environment->getDebugName();
                stats.active_reflection_probe_intensity = active_probe->intensity;
                stats.active_reflection_probe_diffuse_enabled = active_probe->diffuse_enabled;
                stats.active_reflection_probe_diffuse_intensity = active_probe->diffuse_intensity;
                stats.active_reflection_probe_blend_distance = active_probe->blend_distance;
                stats.active_reflection_probe_runtime = active_probe->using_runtime_capture;
                if (active_probe->entity_id >= 0) {
                    const ReflectionProbeCaptureState capture_state =
                        reflection_probe_manager.getCaptureState(active_probe->entity_id);
                    stats.active_reflection_probe_capture_resolution = capture_state.resolution;
                    stats.active_reflection_probe_capture_status = capture_state.message;
                }
            }
            const ReflectionProbeCaptureQueueState queue_state =
                reflection_probe_manager.getQueueState();
            stats.reflection_probe_capture_pending_count = queue_state.pending_count;
            stats.reflection_probe_capture_budget_per_frame =
                queue_state.capture_budget_per_frame;
            stats.reflection_probe_runtime_capture_count = queue_state.resident_capture_count;
            stats.reflection_probe_pinned_capture_count = queue_state.pinned_capture_count;
            stats.reflection_probe_runtime_capture_limit = queue_state.resident_capture_limit;
            stats.reflection_probe_last_captured_entity_id =
                queue_state.last_captured_entity_id;
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

        bool ensurePointShadowTarget(const RenderPointShadowSettings& point_shadow_settings) {
            const uint32_t resolution = sanitizePointShadowMapResolution(point_shadow_settings.map_resolution);
            const uint32_t light_capacity = std::clamp(
                std::max(1u, point_shadow_settings.max_shadowed_lights),
                1u,
                kMaxRenderPointShadowLights);
            if (point_shadow_target.isReady() &&
                point_shadow_target.getExtent().width == resolution &&
                point_shadow_target.getShadowedLightCapacity() == light_capacity) {
                return true;
            }

            if (device.device == VK_NULL_HANDLE) {
                return false;
            }

            vkDeviceWaitIdle(device.device);
            point_shadow_target.shutdown();
            if (!point_shadow_target.init(createResourceContext(), resolution, light_capacity)) {
                return false;
            }

            return frame_lighting_resource.updatePointShadowMap(
                       point_shadow_target.getDepthImageView(),
                       point_shadow_target.getSampler()) &&
                   updatePostProcessInputIfReady();
        }

        bool ensureRectShadowTarget(const RenderRectShadowSettings& rect_shadow_settings) {
            const uint32_t resolution = sanitizeRectShadowMapResolution(rect_shadow_settings.map_resolution);
            const uint32_t light_capacity = std::clamp(
                std::max(1u, rect_shadow_settings.max_shadowed_lights),
                1u,
                kMaxRenderRectShadowLights);
            if (rect_shadow_target.isReady() &&
                rect_shadow_target.getExtent().width == resolution &&
                rect_shadow_target.getLayerCount() == light_capacity) {
                return true;
            }

            if (device.device == VK_NULL_HANDLE) {
                return false;
            }

            vkDeviceWaitIdle(device.device);
            rect_shadow_target.shutdown();
            if (!rect_shadow_target.init(createResourceContext(), resolution, light_capacity)) {
                return false;
            }

            return frame_lighting_resource.updateRectShadowMap(
                       rect_shadow_target.getDepthImageView(),
                       rect_shadow_target.getSampler()) &&
                   updatePostProcessInputIfReady();
        }

        bool ensureAoTarget(const RenderAoSettings& ao_settings, uint32_t width, uint32_t height) {
            if (!ao_target.isReady()) {
                if (ao_format == VK_FORMAT_UNDEFINED) {
                    return false;
                }

                if (!ao_target.init(createResourceContext(), ao_format, width, height, ao_settings.half_resolution)) {
                    return false;
                }
                return recreateAoPassResourcesIfReady() && updatePostProcessInputIfReady();
            }

            const VkExtent2D current_extent = ao_target.getExtent();
            const VkExtent2D expected_extent{
                ao_settings.half_resolution ? std::max(1u, width / 2u) : std::max(1u, width),
                ao_settings.half_resolution ? std::max(1u, height / 2u) : std::max(1u, height)
            };
            if (current_extent.width == expected_extent.width &&
                current_extent.height == expected_extent.height &&
                ao_target.isHalfResolution() == ao_settings.half_resolution) {
                return true;
            }

            if (device.device == VK_NULL_HANDLE) {
                return false;
            }

            vkDeviceWaitIdle(device.device);
            if (!ao_target.resize(width, height, ao_settings.half_resolution)) {
                return false;
            }

            return updatePostProcessInputIfReady();
        }

        float getShadowMapSize() const {
            if (!shadow_target.isReady()) {
                return 1.0f;
            }

            return static_cast<float>(shadow_target.getExtent().width);
        }

        float getPointShadowMapSize() const {
            if (!point_shadow_target.isReady()) {
                return 1.0f;
            }

            return static_cast<float>(point_shadow_target.getExtent().width);
        }

        float getRectShadowMapSize() const {
            if (!rect_shadow_target.isReady()) {
                return 1.0f;
            }

            return static_cast<float>(rect_shadow_target.getExtent().width);
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
            if (!post_process_pass.isReady() || !scene_color_target.isReady() || !ssr_target.isReady()) {
                return false;
            }

            const VulkanPostProcessInput input =
                (bloom_target.isReady() && bloom_pass.isReady()) ?
                    makeBloomCompositePostProcessInput(viewport_target.getRenderTarget().depth_view) :
                    makeScenePostProcessInput(viewport_target.getRenderTarget().depth_view);
            return post_process_pass.updateInput(input);
        }

        bool updatePostProcessInputIfReady() {
            if (!post_process_pass.isReady() || !scene_color_target.isReady() || !ssr_target.isReady()) {
                return true;
            }

            return updatePostProcessInput();
        }

        bool recreateAoPassResourcesIfReady() {
            if (!ao_target.isReady()) {
                return true;
            }

            VulkanAoPassContext ao_context;
            ao_context.device = device.device;
            ao_context.color_format = ao_target.getColorFormat();
            ao_context.input_descriptor_set_layout =
                descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::AoInput);
            ao_context.descriptor_allocator = &descriptor_allocator;
            ao_context.pipeline_cache = &pipeline_cache;

            return ao_pass.recreateResources(ao_context);
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

        bool recreateSsrPassResourcesIfReady() {
            if (!ssr_target.isReady()) {
                return true;
            }

            VulkanSsrPassContext ssr_context;
            ssr_context.device = device.device;
            ssr_context.reflection_color_format = ssr_target.getReflectionFormat();
            ssr_context.hit_mask_format = ssr_target.getHitMaskFormat();
            ssr_context.input_descriptor_set_layout =
                descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::BloomDualInput);
            ssr_context.descriptor_allocator = &descriptor_allocator;
            ssr_context.pipeline_cache = &pipeline_cache;

            return ssr_pass.recreateResources(ssr_context);
        }

        bool recreateSmaaPassResourcesIfReady() {
            if (!smaa_target.isReady()) {
                return true;
            }

            VulkanSmaaPassContext smaa_context;
            smaa_context.device = device.device;
            smaa_context.source_color_format = smaa_target.getSourceFormat();
            smaa_context.mask_color_format = smaa_target.getMaskFormat();
            smaa_context.output_color_format = swapchain.image_format;
            smaa_context.single_input_descriptor_set_layout =
                descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::AoInput);
            smaa_context.dual_input_descriptor_set_layout =
                descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::BloomDualInput);
            smaa_context.descriptor_allocator = &descriptor_allocator;
            smaa_context.pipeline_cache = &pipeline_cache;

            return smaa_pass.recreateResources(smaa_context);
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

        VulkanPostProcessInput makeScenePostProcessInput(VkImageView scene_depth_view) const {
            VulkanPostProcessInput input;
            input.color_view = scene_color_target.getColorImageView();
            input.sampler = scene_color_target.getSampler();
            input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            fillPostProcessAuxiliaryInputs(input, scene_depth_view);
            return input;
        }

        VulkanPostProcessInput makeBloomCompositePostProcessInput(VkImageView scene_depth_view) const {
            VulkanPostProcessInput input;
            input.color_view = bloom_target.getCompositeImage().view;
            input.sampler = bloom_target.getSampler();
            input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            fillPostProcessAuxiliaryInputs(input, scene_depth_view);
            return input;
        }

        VulkanPostProcessInput makeBloomDownsamplePostProcessInput(uint32_t mip_index, VkImageView scene_depth_view) const {
            const VulkanBloomImageView& image = bloom_target.getDownsampleImage(mip_index);
            VulkanPostProcessInput input;
            input.color_view = image.view;
            input.sampler = bloom_target.getSampler();
            input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            fillPostProcessAuxiliaryInputs(input, scene_depth_view);
            return input;
        }

        VulkanPostProcessInput makeBloomUpsamplePostProcessInput(uint32_t mip_index, VkImageView scene_depth_view) const {
            const VulkanBloomImageView& image = bloom_target.getUpsampleImage(mip_index);
            VulkanPostProcessInput input;
            input.color_view = image.view;
            input.sampler = bloom_target.getSampler();
            input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            fillPostProcessAuxiliaryInputs(input, scene_depth_view);
            return input;
        }

        void fillPostProcessAuxiliaryInputs(VulkanPostProcessInput& input, VkImageView scene_depth_view) const {
            input.shadow_view = shadow_target.getDepthImageView();
            input.shadow_sampler = shadow_target.getSampler();
            input.shadow_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            input.shadow_layer_count = shadow_target.getLayerCount();
            input.point_shadow_view = point_shadow_target.getDepthImageView();
            input.point_shadow_sampler = point_shadow_target.getSampler();
            input.point_shadow_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            input.point_shadow_layer_count = point_shadow_target.getLayerCount();
            input.rect_shadow_view = rect_shadow_target.getDepthImageView();
            input.rect_shadow_sampler = rect_shadow_target.getSampler();
            input.rect_shadow_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            input.rect_shadow_layer_count = rect_shadow_target.getLayerCount();
            input.scene_depth_view = scene_depth_view;
            input.scene_depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            input.ao_raw_view = ao_target.getRawImage().view;
            input.ao_blurred_view = ao_target.getBlurredImage().view;
            input.ao_sampler = ao_target.getSampler();
            input.ao_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            input.ssr_raw_reflection_view = ssr_target.getRawReflectionImage().view;
            input.ssr_hit_mask_view = ssr_target.getHitMaskImage().view;
            input.ssr_sampler = ssr_target.getSampler();
            input.ssr_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        bool createSwapchain() {
            if (surface_width == 0 || surface_height == 0) {
                return false;
            }

            if (scene_color_format == VK_FORMAT_UNDEFINED) {
                scene_color_format = VulkanDiagnosticsCollector::findHdrSceneColorFormat(physical_device.physical_device);
                if (scene_color_format == VK_FORMAT_UNDEFINED) {
                    NX_CORE_ERROR("VulkanRendererSystem failed to find a supported HDR scene color format.");
                    return false;
                }
            }
            swapchain_manager.setSurfaceSize(surface_width, surface_height);
            if (!swapchain_manager.create(device_context)) {
                return false;
            }

            VulkanForwardPassSwapchainContext pass_context;
            pass_context.physical_device = physical_device.physical_device;
            pass_context.device = device.device;
            pass_context.gpu_allocator = &gpu_allocator;
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
            if (!recreateSsrPassResourcesIfReady()) {
                return false;
            }
            if (!recreateSmaaPassResourcesIfReady()) {
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

            return createRenderFinishedSemaphores();
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
            cleanupRenderFinishedSemaphores();
            smaa_pass.cleanupResources();
            post_process_pass.cleanupResources();
            bloom_pass.cleanupResources();
            ssr_pass.cleanupResources();
            debug_draw_pass.cleanupResources();
            skybox_pass.cleanupResources();
            forward_pass.cleanupSwapchainResources();
            swapchain_manager.shutdown();
        }

        bool createCommandResources() {
            VkCommandPoolCreateInfo pool_info{};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool_info.queueFamilyIndex = graphics_queue_family;

            if (!VulkanDiagnosticsCollector::checkVk(vkCreateCommandPool(device.device, &pool_info, nullptr, &command_pool), "vkCreateCommandPool")) {
                return false;
            }

            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.commandPool = command_pool;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandBufferCount = 1;

            return VulkanDiagnosticsCollector::checkVk(vkAllocateCommandBuffers(device.device, &allocate_info, &command_buffer), "vkAllocateCommandBuffers");
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

            if (!VulkanDiagnosticsCollector::checkVk(
                    vkCreateSemaphore(device.device, &semaphore_info, nullptr, &image_available),
                    "vkCreateSemaphore(image_available)")) {
                return false;
            }

            VkFenceCreateInfo fence_info{};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            return VulkanDiagnosticsCollector::checkVk(vkCreateFence(device.device, &fence_info, nullptr, &in_flight), "vkCreateFence");
        }

        bool createRenderFinishedSemaphores() {
            render_finished_semaphores.assign(swapchain_images.size(), VK_NULL_HANDLE);

            VkSemaphoreCreateInfo semaphore_info{};
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            for (VkSemaphore& semaphore : render_finished_semaphores) {
                if (!VulkanDiagnosticsCollector::checkVk(
                        vkCreateSemaphore(device.device, &semaphore_info, nullptr, &semaphore),
                        "vkCreateSemaphore(render_finished)")) {
                    cleanupRenderFinishedSemaphores();
                    return false;
                }
            }
            return true;
        }

        void cleanupRenderFinishedSemaphores() {
            for (VkSemaphore semaphore : render_finished_semaphores) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(device.device, semaphore, nullptr);
                }
            }
            render_finished_semaphores.clear();
        }

        void cleanupSyncObjects() {
            if (in_flight != VK_NULL_HANDLE) {
                vkDestroyFence(device.device, in_flight, nullptr);
                in_flight = VK_NULL_HANDLE;
            }
            if (image_available != VK_NULL_HANDLE) {
                vkDestroySemaphore(device.device, image_available, nullptr);
                image_available = VK_NULL_HANDLE;
            }
        }

        bool prepareFrameTargets(const VulkanPreparedFrame& prepared_frame) {
            const RenderSettings& render_settings = prepared_frame.scene.render_settings;
            const VulkanDrawList& draw_list = prepared_frame.draw_list;
            return ensureShadowTarget(render_settings.shadow) &&
                   ensurePointShadowTarget(render_settings.point_shadow) &&
                   ensureRectShadowTarget(render_settings.rect_shadow) &&
                   ensureAoTarget(
                       render_settings.ao,
                       draw_list.view.viewport_width,
                       draw_list.view.viewport_height);
        }

        void drawFrame(
            const VulkanPreparedFrame& prepared_frame,
            const VulkanRenderFeaturePlan& feature_plan) {
            const VulkanDrawList& draw_list = prepared_frame.draw_list;
            const RenderSettings& render_settings = prepared_frame.scene.render_settings;
            if (swapchain.swapchain == VK_NULL_HANDLE || swapchain_images.empty()) {
                return;
            }

            waitForInFlightFrame();

            const RenderShadowCascadeFrame shadow_frame = shadow_target.isReady() ?
                shadow_frame_builder.buildDirectionalShadowFrame(
                    prepared_frame.scene.view,
                    draw_list.directional_light,
                    render_settings.shadow,
                    feature_plan.getDebugSettings(),
                    getShadowMapSize()) :
                RenderShadowCascadeFrame{};
            const RenderPointShadowFrame point_shadow_frame = shadow_frame_builder.buildPointShadowFrame(
                draw_list.point_lights,
                render_settings.point_shadow,
                point_shadow_target.isReady() ? point_shadow_target.getShadowedLightCapacity() : 0u);
            const RenderRectShadowFrame rect_shadow_frame = shadow_frame_builder.buildRectShadowFrame(
                draw_list.rect_lights,
                render_settings.rect_shadow,
                rect_shadow_target.isReady() ? rect_shadow_target.getLayerCount() : 0u);
            if (!frame_lighting_resource.update(
                    draw_list,
                    shadow_frame,
                    point_shadow_frame,
                    rect_shadow_frame,
                    getShadowMapSize(),
                    getPointShadowMapSize(),
                    getRectShadowMapSize(),
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
                NX_CORE_ERROR("vkAcquireNextImageKHR failed: {}", VulkanDiagnosticsCollector::vkResultToString(acquire_result));
                return;
            }
            if (image_index >= render_finished_semaphores.size()) {
                NX_CORE_ERROR(
                    "VulkanRendererSystem acquired swapchain image {} without a render-finished semaphore.",
                    image_index);
                return;
            }
            const VkSemaphore render_finished = render_finished_semaphores[image_index];

            if (!recordDrawCommands(
                    image_index,
                    draw_list,
                    shadow_frame,
                    point_shadow_frame,
                    rect_shadow_frame,
                    render_settings,
                    feature_plan)) {
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

            if (!VulkanDiagnosticsCollector::checkVk(vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight), "vkQueueSubmit")) {
                return;
            }
            picking_manager.onFrameSubmitted();

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

            VulkanDiagnosticsCollector::checkVk(present_result, "vkQueuePresentKHR");
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

        VkDescriptorSet resolveReflectionProbeDescriptorSet(const VulkanDrawList& draw_list) const {
            const VulkanEnvironmentResource* environment = draw_list.active_reflection_probe.environment;
            if (!environment || !environment->isReady()) {
                environment = draw_list.environment;
            }
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
            const RenderPointShadowFrame& point_shadow_frame,
            const RenderRectShadowFrame& rect_shadow_frame,
            const RenderSettings& render_settings,
            const VulkanRenderFeaturePlan& feature_plan) {
            if (image_index >= swapchain_images.size()) {
                return false;
            }
            picking_manager.beginFrameRecording();

            if (!VulkanDiagnosticsCollector::checkVk(vkResetCommandBuffer(command_buffer, 0), "vkResetCommandBuffer")) {
                return false;
            }

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!VulkanDiagnosticsCollector::checkVk(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer")) {
                return false;
            }

            VulkanPassGraph graph;
            if (!buildFrameRenderGraph(
                    graph,
                    image_index,
                    draw_list,
                    shadow_frame,
                    point_shadow_frame,
                    rect_shadow_frame,
                    render_settings,
                    feature_plan)) {
                return false;
            }

            if (!graph_executor.execute(graph, command_buffer)) {
                return false;
            }

            if (!VulkanDiagnosticsCollector::checkVk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer")) {
                return false;
            }

            graph.commitImageLayouts();
            return true;
        }

        bool buildFrameRenderGraph(
            VulkanPassGraph& graph,
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            const RenderShadowCascadeFrame& shadow_frame,
            const RenderPointShadowFrame& point_shadow_frame,
            const RenderRectShadowFrame& rect_shadow_frame,
            const RenderSettings& render_settings,
            const VulkanRenderFeaturePlan& feature_plan) {
            const bool render_to_viewport =
                feature_plan.getOutputRoute() == VulkanFrameOutputRoute::Viewport;
            const VulkanRenderTarget scene_target = render_to_viewport ?
                makeViewportSceneRenderTarget() :
                makeSwapchainSceneRenderTarget(image_index);

            VulkanFrameGraphResources resources;
            resources.directional_shadow_depth = addShadowDepthImage(graph);
            resources.point_shadow_depth = addPointShadowDepthImage(graph);
            resources.rect_shadow_depth = addRectShadowDepthImage(graph);
            resources.scene_color = addSceneColorImage(graph);
            resources.scene_depth = render_to_viewport ?
                addViewportDepthImage(graph) :
                addSwapchainDepthImage(graph);
            resources.ao_raw = addAoRawImage(graph);
            resources.ao_blurred = addAoBlurredImage(graph);
            resources.ssr_raw_reflection = addSsrRawReflectionImage(graph);
            resources.ssr_hit_mask = addSsrHitMaskImage(graph);
            resources.swapchain_color = addSwapchainColorImage(graph, image_index);
            resources.final_color = render_to_viewport ?
                addViewportColorImage(graph) :
                resources.swapchain_color;
            if (feature_plan.rendersSmaa()) {
                resources.smaa_source = addSmaaSourceImage(graph);
            }

            VulkanPostProcessInput post_process_input =
                makeScenePostProcessInput(scene_target.depth_view);
            VulkanFrameGraphCallbacks callbacks;
            callbacks.add_directional_shadow =
                [this, &draw_list, &shadow_frame](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle depth) {
                    return addDirectionalShadowPass(target_graph, depth, draw_list, shadow_frame);
                };
            callbacks.add_point_shadow =
                [this, &draw_list, &point_shadow_frame](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle depth) {
                    return addPointShadowPass(target_graph, depth, draw_list, point_shadow_frame);
                };
            callbacks.add_rect_shadow =
                [this, &draw_list, &rect_shadow_frame](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle depth) {
                    return addRectShadowPass(target_graph, depth, draw_list, rect_shadow_frame);
                };
            callbacks.add_skybox =
                [this, &draw_list](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle color) {
                    return addSkyboxPass(target_graph, color, makeSceneSkyboxTarget(), draw_list);
                };
            callbacks.record_forward =
                [this, &draw_list, scene_target](VkCommandBuffer target_command_buffer) {
                    return forward_pass.record(
                        target_command_buffer,
                        scene_target,
                        draw_list,
                        frame_lighting_resource.getDescriptorSet(),
                        resolveEnvironmentDescriptorSet(draw_list),
                        resolveReflectionProbeDescriptorSet(draw_list),
                        forwardAfterSkyboxOptions());
                };
            callbacks.add_ao =
                [this, &draw_list, scene_target, &render_settings](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle depth,
                    VulkanGraphImageHandle raw,
                    VulkanGraphImageHandle blurred) {
                    return addAoPass(
                        target_graph,
                        depth,
                        raw,
                        blurred,
                        scene_target.depth_view,
                        draw_list.view,
                        render_settings.ao);
                };
            callbacks.add_ssr =
                [this, &draw_list, scene_target, &render_settings](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle scene_color,
                    VulkanGraphImageHandle scene_depth,
                    VulkanGraphImageHandle raw_reflection,
                    VulkanGraphImageHandle hit_mask) {
                    return addSsrPass(
                        target_graph,
                        scene_color,
                        scene_depth,
                        raw_reflection,
                        hit_mask,
                        scene_target.depth_view,
                        draw_list.view,
                        render_settings.ssr);
                };
            callbacks.add_debug_draw =
                [this, scene_target](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle color,
                    VulkanGraphImageHandle depth) {
                    return addDebugDrawPass(target_graph, color, depth, scene_target);
                };
            callbacks.add_object_id =
                [this, &draw_list](VulkanPassGraph& target_graph) {
                    return addObjectIdPass(target_graph, draw_list);
                };
            callbacks.add_bloom =
                [this, scene_target, &render_settings, &feature_plan, &post_process_input](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle scene_color,
                    VulkanGraphImageHandle& composite_color) {
                    return addBloomPass(
                        target_graph,
                        scene_color,
                        render_settings.post_process,
                        feature_plan.getDebugSettings(),
                        scene_target.depth_view,
                        composite_color,
                        post_process_input);
                };
            callbacks.add_post_process =
                [this, image_index, render_to_viewport, &render_settings, &feature_plan, &post_process_input](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle input_color,
                    VulkanGraphImageHandle output_color,
                    VulkanGraphImageHandle scene_depth,
                    VulkanGraphImageHandle ao_raw,
                    VulkanGraphImageHandle ao_blurred,
                    VulkanGraphImageHandle ssr_raw_reflection,
                    VulkanGraphImageHandle ssr_hit_mask) {
                    const VulkanPostProcessRenderTarget target = feature_plan.rendersSmaa() ?
                        makeSmaaSourcePostProcessTarget() :
                        (render_to_viewport ?
                            makeViewportPostProcessTarget() :
                            makeSwapchainPostProcessTarget(image_index));
                    RenderAoSettings ao_settings = render_settings.ao;
                    ao_settings.enabled = ao_settings.enabled && feature_plan.rendersAo();
                    RenderSsrSettings ssr_settings = render_settings.ssr;
                    ssr_settings.enabled = ssr_settings.enabled && feature_plan.rendersSsr();
                    return addPostProcessPass(
                        target_graph,
                        input_color,
                        output_color,
                        scene_depth,
                        ao_raw,
                        ao_blurred,
                        ssr_raw_reflection,
                        ssr_hit_mask,
                        target,
                        post_process_input,
                        render_settings.post_process,
                        ao_settings,
                        ssr_settings,
                        feature_plan.getPostProcessDebugSettings(),
                        feature_plan.isolatesForwardDebug());
                };
            callbacks.add_smaa =
                [this, image_index, render_to_viewport, &render_settings, &feature_plan](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle source_color,
                    VulkanGraphImageHandle output_color) {
                    const VulkanSmaaRenderTarget target = render_to_viewport ?
                        makeViewportSmaaOutputTarget() :
                        makeSwapchainSmaaOutputTarget(image_index);
                    return addSmaaPass(
                        target_graph,
                        source_color,
                        output_color,
                        target,
                        render_settings.anti_aliasing,
                        feature_plan.getDebugSettings());
                };
            if (render_to_viewport) {
                callbacks.record_imgui =
                    [this, image_index](VkCommandBuffer target_command_buffer) {
                        return recordImGuiToSwapchain(target_command_buffer, image_index);
                    };
            }

            return frame_graph_builder.build(graph, feature_plan, resources, callbacks);
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
                .readWriteImage(color_image, VulkanGraphImageUsage::ColorAttachment)
                .readWriteImage(depth_image, VulkanGraphImageUsage::DepthStencilAttachment)
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
            if (!picking_manager.isReady()) {
                return true;
            }

            const VulkanGraphImageHandle object_id = picking_manager.addObjectIdImage(graph);
            const VulkanGraphImageHandle depth = picking_manager.addDepthImage(graph);
            if (!object_id.valid() || !depth.valid()) {
                return false;
            }

            graph.addPass("ObjectIdPicking")
                .writeImage(object_id, VulkanGraphImageUsage::ColorAttachment)
                .writeImage(depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, &draw_list](VkCommandBuffer target_command_buffer) {
                    if (!object_id_pass.record(
                            target_command_buffer,
                            picking_manager.getRenderTarget(),
                            draw_list)) {
                        return false;
                    }

                    picking_manager.markPassRecorded();
                    return true;
                });

            return true;
        }

        bool addPointShadowPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle point_shadow_depth,
            const VulkanDrawList& draw_list,
            const RenderPointShadowFrame& point_shadow_frame) {
            if (!point_shadow_depth.valid() || !point_shadow_target.isReady()) {
                return false;
            }

            graph.addPass("PointShadowMap")
                .writeImage(point_shadow_depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, &draw_list, point_shadow_frame](VkCommandBuffer target_command_buffer) {
                    if (!point_shadow_frame.enabled || point_shadow_frame.face_count == 0) {
                        return true;
                    }

                    const uint32_t face_count = std::min(
                        point_shadow_frame.face_count,
                        point_shadow_target.getLayerCount());
                    for (uint32_t layer_index = 0; layer_index < face_count; ++layer_index) {
                        if (!shadow_pass.record(
                                target_command_buffer,
                                point_shadow_target.getRenderTarget(layer_index),
                                draw_list,
                                point_shadow_frame.light_view_projections[layer_index])) {
                            return false;
                        }
                    }
                    return true;
                });
            return true;
        }

        bool addRectShadowPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle rect_shadow_depth,
            const VulkanDrawList& draw_list,
            const RenderRectShadowFrame& rect_shadow_frame) {
            if (!rect_shadow_depth.valid() || !rect_shadow_target.isReady()) {
                return false;
            }

            graph.addPass("RectShadowMap")
                .writeImage(rect_shadow_depth, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, &draw_list, rect_shadow_frame](VkCommandBuffer target_command_buffer) {
                    if (!rect_shadow_frame.enabled || rect_shadow_frame.shadowed_light_count == 0) {
                        return true;
                    }

                    const uint32_t layer_count = std::min(
                        rect_shadow_frame.shadowed_light_count,
                        rect_shadow_target.getLayerCount());
                    for (uint32_t layer_index = 0; layer_index < layer_count; ++layer_index) {
                        if (!shadow_pass.record(
                                target_command_buffer,
                                rect_shadow_target.getRenderTarget(layer_index),
                                draw_list,
                                rect_shadow_frame.light_view_projections[layer_index])) {
                            return false;
                        }
                    }
                    return true;
                });
            return true;
        }

        bool addAoPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle depth_image,
            VulkanGraphImageHandle ao_raw,
            VulkanGraphImageHandle ao_blurred,
            VkImageView scene_depth_view,
            const VulkanRenderView& view,
            const RenderAoSettings& ao_settings) {
            if (!depth_image.valid() || !ao_raw.valid() || !ao_blurred.valid()) {
                return false;
            }

            VulkanAoInput depth_input;
            depth_input.view = scene_depth_view;
            depth_input.sampler = ao_target.getSampler();
            depth_input.extent = { view.viewport_width, view.viewport_height };
            depth_input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            const VulkanAoImageView& raw_image = ao_target.getRawImage();
            VulkanAoInput raw_input;
            raw_input.view = raw_image.view;
            raw_input.sampler = ao_target.getSampler();
            raw_input.extent = raw_image.extent;
            raw_input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (!ao_pass.updateInputs(depth_input, raw_input)) {
                return false;
            }

            graph.addPass("SSAO")
                .readImage(depth_image, VulkanGraphImageUsage::ShaderRead)
                .writeImage(ao_raw, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, view, ao_settings](VkCommandBuffer target_command_buffer) {
                    return ao_pass.recordSsao(
                        target_command_buffer,
                        ao_target.getRawRenderTarget(),
                        view,
                        ao_settings);
                });

            graph.addPass("AOBlur")
                .readImage(ao_raw, VulkanGraphImageUsage::ShaderRead)
                .writeImage(ao_blurred, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, ao_settings](VkCommandBuffer target_command_buffer) {
                    return ao_pass.recordBlur(
                        target_command_buffer,
                        ao_target.getBlurredRenderTarget(),
                        ao_settings);
                });

            return true;
        }

        bool addSsrPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle scene_color,
            VulkanGraphImageHandle scene_depth,
            VulkanGraphImageHandle ssr_raw_reflection,
            VulkanGraphImageHandle ssr_hit_mask,
            VkImageView scene_depth_view,
            const VulkanRenderView& view,
            const RenderSsrSettings& ssr_settings) {
            if (!scene_color.valid() ||
                !scene_depth.valid() ||
                !ssr_raw_reflection.valid() ||
                !ssr_hit_mask.valid()) {
                return false;
            }

            VulkanSsrInput input;
            input.scene_color_view = scene_color_target.getColorImageView();
            input.scene_depth_view = scene_depth_view;
            input.sampler = scene_color_target.getSampler();
            input.extent = view.viewport_width > 0 && view.viewport_height > 0 ?
                VkExtent2D{ view.viewport_width, view.viewport_height } :
                scene_color_target.getExtent();
            input.scene_color_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            input.scene_depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (!ssr_pass.updateInput(input)) {
                return false;
            }

            const VulkanSsrRenderTarget reflection_target = ssr_target.getRawReflectionRenderTarget();
            graph.addPass("SSRRawReflection")
                .readImage(scene_color, VulkanGraphImageUsage::ShaderRead)
                .readImage(scene_depth, VulkanGraphImageUsage::ShaderRead)
                .writeImage(ssr_raw_reflection, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, reflection_target, view, ssr_settings](VkCommandBuffer target_command_buffer) {
                    return ssr_pass.recordTrace(
                        target_command_buffer,
                        reflection_target,
                        view,
                        ssr_settings,
                        VulkanSsrOutputMode::RawReflection);
                });

            const VulkanSsrRenderTarget hit_mask_target = ssr_target.getHitMaskRenderTarget();
            graph.addPass("SSRHitMask")
                .readImage(scene_color, VulkanGraphImageUsage::ShaderRead)
                .readImage(scene_depth, VulkanGraphImageUsage::ShaderRead)
                .writeImage(ssr_hit_mask, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, hit_mask_target, view, ssr_settings](VkCommandBuffer target_command_buffer) {
                    return ssr_pass.recordTrace(
                        target_command_buffer,
                        hit_mask_target,
                        view,
                        ssr_settings,
                        VulkanSsrOutputMode::HitMask);
                });

            return true;
        }

        bool addBloomPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle scene_color,
            const RenderPostProcessSettings& post_process_settings,
            const RenderEffectDebugSettings& debug_settings,
            VkImageView scene_depth_view,
            VulkanGraphImageHandle& composite_color,
            VulkanPostProcessInput& post_process_input) {
            composite_color = scene_color;
            post_process_input = makeScenePostProcessInput(scene_depth_view);
            if (!scene_color.valid()) {
                return false;
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
                post_process_input = makeBloomDownsamplePostProcessInput(selected_mip, scene_depth_view);
                return true;
            }

            if (debug_settings.view == RenderEffectDebugView::BloomUpsampleMip) {
                if (upsample_images.empty()) {
                    composite_color = downsample_images[0];
                    post_process_input = makeBloomDownsamplePostProcessInput(0, scene_depth_view);
                    return true;
                }

                const uint32_t selected_mip =
                    std::min(debug_settings.bloom_mip, static_cast<uint32_t>(upsample_images.size() - 1u));
                composite_color = upsample_images[selected_mip];
                post_process_input = makeBloomUpsamplePostProcessInput(selected_mip, scene_depth_view);
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
            post_process_input = makeBloomCompositePostProcessInput(scene_depth_view);
            return true;
        }

        bool addSmaaPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle source_color,
            VulkanGraphImageHandle output_color,
            VulkanSmaaRenderTarget output_target,
            RenderAntiAliasingSettings anti_aliasing_settings,
            RenderEffectDebugSettings debug_settings) {
            if (!source_color.valid() || !output_color.valid() || !output_target.valid()) {
                return false;
            }
            const VulkanGraphImageHandle edge_color = addSmaaEdgeImage(graph);
            if (!edge_color.valid()) {
                return false;
            }

            const VulkanSmaaImageView& source_image = smaa_target.getSourceImage();
            const VulkanSmaaImageView& edge_image = smaa_target.getEdgeImage();
            const VulkanSmaaImageView& blend_image = smaa_target.getBlendImage();
            VulkanSmaaInput source_input;
            source_input.view = source_image.view;
            source_input.sampler = smaa_target.getSampler();
            source_input.extent = source_image.extent;
            source_input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VulkanSmaaInput edge_input;
            edge_input.view = edge_image.view;
            edge_input.sampler = smaa_target.getSampler();
            edge_input.extent = edge_image.extent;
            edge_input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VulkanSmaaInput blend_input;
            blend_input.view = blend_image.view;
            blend_input.sampler = smaa_target.getSampler();
            blend_input.extent = blend_image.extent;
            blend_input.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            const VulkanSmaaRenderTarget edge_target = smaa_target.getEdgeRenderTarget();
            graph.addPass("SMAAEdge")
                .readImage(source_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(edge_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, source_input, edge_input, blend_input, edge_target, anti_aliasing_settings](VkCommandBuffer target_command_buffer) {
                    if (!smaa_pass.updateInputs(source_input, edge_input, blend_input)) {
                        return false;
                    }
                    return smaa_pass.recordEdge(target_command_buffer, edge_target, anti_aliasing_settings);
                });

            if (debug_settings.view == RenderEffectDebugView::SmaaEdgeMask) {
                graph.addPass("SMAAEdgeDebug")
                    .readImage(edge_color, VulkanGraphImageUsage::ShaderRead)
                    .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                    .execute([this, output_target, edge_input](VkCommandBuffer target_command_buffer) {
                        return smaa_pass.recordDebugResolve(target_command_buffer, output_target, edge_input);
                    });
                return true;
            }

            const VulkanGraphImageHandle blend_color = addSmaaBlendImage(graph);
            if (!blend_color.valid()) {
                return false;
            }

            const VulkanSmaaRenderTarget blend_target = smaa_target.getBlendRenderTarget();
            graph.addPass("SMAABlend")
                .readImage(edge_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(blend_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, blend_target, anti_aliasing_settings](VkCommandBuffer target_command_buffer) {
                    return smaa_pass.recordBlend(target_command_buffer, blend_target, anti_aliasing_settings);
                });

            if (debug_settings.view == RenderEffectDebugView::SmaaBlendWeight) {
                graph.addPass("SMAABlendDebug")
                    .readImage(blend_color, VulkanGraphImageUsage::ShaderRead)
                    .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                    .execute([this, output_target, blend_input](VkCommandBuffer target_command_buffer) {
                        return smaa_pass.recordDebugResolve(target_command_buffer, output_target, blend_input);
                    });
                return true;
            }

            graph.addPass("SMAANeighborhood")
                .readImage(source_color, VulkanGraphImageUsage::ShaderRead)
                .readImage(blend_color, VulkanGraphImageUsage::ShaderRead)
                .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, output_target, anti_aliasing_settings](VkCommandBuffer target_command_buffer) {
                    return smaa_pass.recordNeighborhood(target_command_buffer, output_target, anti_aliasing_settings);
                });
            return true;
        }

        bool addPostProcessPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle input_color,
            VulkanGraphImageHandle output_color,
            VulkanGraphImageHandle scene_depth,
            VulkanGraphImageHandle ao_raw,
            VulkanGraphImageHandle ao_blurred,
            VulkanGraphImageHandle ssr_raw_reflection,
            VulkanGraphImageHandle ssr_hit_mask,
            VulkanPostProcessRenderTarget target,
            const VulkanPostProcessInput& input,
            RenderPostProcessSettings post_process_settings,
            RenderAoSettings ao_settings,
            RenderSsrSettings ssr_settings,
            RenderEffectDebugSettings debug_settings,
            bool isolate_forward_debug) {
            if (!input_color.valid() ||
                !output_color.valid() ||
                !scene_depth.valid() ||
                !ao_raw.valid() ||
                !ao_blurred.valid() ||
                !ssr_raw_reflection.valid() ||
                !ssr_hit_mask.valid() ||
                !target.valid() ||
                !input.valid() ||
                !post_process_pass.isReady()) {
                return false;
            }

            if (!post_process_pass.updateInput(input)) {
                return false;
            }

            graph.addPass("PostProcess")
                .readImage(input_color, VulkanGraphImageUsage::ShaderRead)
                .readImage(scene_depth, VulkanGraphImageUsage::ShaderRead)
                .readImage(ao_raw, VulkanGraphImageUsage::ShaderRead)
                .readImage(ao_blurred, VulkanGraphImageUsage::ShaderRead)
                .readImage(ssr_raw_reflection, VulkanGraphImageUsage::ShaderRead)
                .readImage(ssr_hit_mask, VulkanGraphImageUsage::ShaderRead)
                .writeImage(output_color, VulkanGraphImageUsage::ColorAttachment)
                .execute([this, target, post_process_settings, ao_settings, ssr_settings, debug_settings, isolate_forward_debug](VkCommandBuffer target_command_buffer) {
                    return post_process_pass.record(
                        target_command_buffer,
                        target,
                        post_process_settings,
                        ao_settings,
                        ssr_settings,
                        debug_settings,
                        isolate_forward_debug);
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
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = swapchain_image_layouts[image_index];
            desc.external_acquire_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
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
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = scene_color_target.getColorLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                scene_color_target.setColorLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addAoRawImage(VulkanPassGraph& graph) {
            const VulkanAoImageView& ao_image = ao_target.getRawImage();
            VulkanGraphImageDesc desc;
            desc.name = "AORaw";
            desc.image = ao_image.image;
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = ao_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                ao_target.setRawLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addAoBlurredImage(VulkanPassGraph& graph) {
            const VulkanAoImageView& ao_image = ao_target.getBlurredImage();
            VulkanGraphImageDesc desc;
            desc.name = "AOBlurred";
            desc.image = ao_image.image;
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = ao_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                ao_target.setBlurredLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addBloomDownsampleImage(VulkanPassGraph& graph, uint32_t mip_index) {
            const VulkanBloomImageView& bloom_image = bloom_target.getDownsampleImage(mip_index);
            VulkanGraphImageDesc desc;
            desc.name = "BloomDownsample" + std::to_string(mip_index);
            desc.image = bloom_image.image;
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
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
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
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
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = bloom_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                bloom_target.setCompositeLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addSsrRawReflectionImage(VulkanPassGraph& graph) {
            const VulkanSsrImageView& ssr_image = ssr_target.getRawReflectionImage();
            VulkanGraphImageDesc desc;
            desc.name = "SSRRawReflection";
            desc.image = ssr_image.image;
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = ssr_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                ssr_target.setRawReflectionLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addSsrHitMaskImage(VulkanPassGraph& graph) {
            const VulkanSsrImageView& ssr_image = ssr_target.getHitMaskImage();
            VulkanGraphImageDesc desc;
            desc.name = "SSRHitMask";
            desc.image = ssr_image.image;
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = ssr_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                ssr_target.setHitMaskLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addSwapchainDepthImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "SwapchainDepth";
            desc.image = forward_pass.getDepthImage();
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
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
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
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
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.initial_layout = viewport_target.getDepthLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                viewport_target.setDepthLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addShadowDepthImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "ShadowDepth";
            desc.image = shadow_target.getDepthImage();
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.subresource_range.layer_count = shadow_target.getLayerCount();
            desc.initial_layout = shadow_target.getDepthLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                shadow_target.setDepthLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addSmaaSourceImage(VulkanPassGraph& graph) {
            const VulkanSmaaImageView& smaa_image = smaa_target.getSourceImage();
            VulkanGraphImageDesc desc;
            desc.name = "SMAASource";
            desc.image = smaa_image.image;
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = smaa_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                smaa_target.setSourceLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addSmaaEdgeImage(VulkanPassGraph& graph) {
            const VulkanSmaaImageView& smaa_image = smaa_target.getEdgeImage();
            VulkanGraphImageDesc desc;
            desc.name = "SMAAEdge";
            desc.image = smaa_image.image;
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = smaa_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                smaa_target.setEdgeLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addSmaaBlendImage(VulkanPassGraph& graph) {
            const VulkanSmaaImageView& smaa_image = smaa_target.getBlendImage();
            VulkanGraphImageDesc desc;
            desc.name = "SMAABlend";
            desc.image = smaa_image.image;
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.initial_layout = smaa_image.layout;
            desc.commit_layout = [this](VkImageLayout layout) {
                smaa_target.setBlendLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addPointShadowDepthImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "PointShadowDepth";
            desc.image = point_shadow_target.getDepthImage();
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.subresource_range.layer_count = point_shadow_target.getLayerCount();
            desc.initial_layout = point_shadow_target.getDepthLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                point_shadow_target.setDepthLayout(layout);
            };
            return graph.addImage(std::move(desc));
        }

        VulkanGraphImageHandle addRectShadowDepthImage(VulkanPassGraph& graph) {
            VulkanGraphImageDesc desc;
            desc.name = "RectShadowDepth";
            desc.image = rect_shadow_target.getDepthImage();
            desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.subresource_range.layer_count = rect_shadow_target.getLayerCount();
            desc.initial_layout = rect_shadow_target.getDepthLayout();
            desc.commit_layout = [this](VkImageLayout layout) {
                rect_shadow_target.setDepthLayout(layout);
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

        VulkanPostProcessRenderTarget makeSmaaSourcePostProcessTarget() const {
            const VulkanSmaaRenderTarget source_target = smaa_target.getSourceRenderTarget();
            VulkanPostProcessRenderTarget target;
            target.color_view = source_target.color_view;
            target.color_format = source_target.color_format;
            target.extent = source_target.extent;
            return target;
        }

        VulkanPostProcessRenderTarget makeSwapchainPostProcessTarget(uint32_t image_index) const {
            VulkanPostProcessRenderTarget target;
            target.color_view = forward_pass.getSwapchainColorImageView(image_index);
            target.color_format = swapchain.image_format;
            target.extent = swapchain.extent;
            return target;
        }

        VulkanSmaaRenderTarget makeViewportSmaaOutputTarget() const {
            VulkanSmaaRenderTarget target;
            target.color_view = viewport_target.getColorImageView();
            target.color_format = viewport_target.getColorFormat();
            target.extent = viewport_target.getExtent();
            return target;
        }

        VulkanSmaaRenderTarget makeSwapchainSmaaOutputTarget(uint32_t image_index) const {
            VulkanSmaaRenderTarget target;
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
            VkPipelineStageFlags dst_stage,
            uint32_t base_array_layer = 0,
            uint32_t layer_count = 1,
            uint32_t level_count = 1) {
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
            barrier.subresourceRange.levelCount = level_count;
            barrier.subresourceRange.baseArrayLayer = base_array_layer;
            barrier.subresourceRange.layerCount = layer_count;

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

    private:
        VulkanDeviceContext device_context;
        VulkanGpuAllocator gpu_allocator;
        VulkanSwapchainManager swapchain_manager;

        // Transitional aliases keep the existing pass code readable while the
        // owning Vulkan state lives in the dedicated backend contexts.
        vkb::Instance& instance = device_context.getInstanceBundle();
        VkSurfaceKHR& surface = device_context.getSurfaceHandle();
        vkb::PhysicalDevice& physical_device = device_context.getPhysicalDeviceBundle();
        vkb::Device& device = device_context.getDeviceBundle();
        vkb::Swapchain& swapchain = swapchain_manager.getSwapchain();
        std::vector<VkImage>& swapchain_images = swapchain_manager.getImagesValue();
        std::vector<VkImageLayout>& swapchain_image_layouts = swapchain_manager.getImageLayoutsValue();
        VkQueue& graphics_queue = device_context.getGraphicsQueueHandle();
        VkQueue& present_queue = device_context.getPresentQueueHandle();
        uint32_t& graphics_queue_family = device_context.getGraphicsQueueFamilyValue();
        uint32_t& device_api_version = device_context.getApiVersionValue();
        uint32_t& surface_width = swapchain_manager.getSurfaceWidthValue();
        uint32_t& surface_height = swapchain_manager.getSurfaceHeightValue();
        bool& swapchain_dirty = swapchain_manager.getDirtyValue();

        bool initialized = false;
        VkFormat scene_color_format = VK_FORMAT_UNDEFINED;
        VkFormat ao_format = VK_FORMAT_UNDEFINED;
        VkFormat ssr_hit_mask_format = VK_FORMAT_UNDEFINED;
        VkFormat smaa_mask_format = VK_FORMAT_UNDEFINED;

        VkCommandPool command_pool = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkSemaphore image_available = VK_NULL_HANDLE;
        std::vector<VkSemaphore> render_finished_semaphores;
        VkFence in_flight = VK_NULL_HANDLE;

        VulkanShaderLibrary shader_library;
        VulkanDescriptorLayoutCache descriptor_layout_cache;
        VulkanDescriptorAllocator descriptor_allocator;
        VulkanPipelineCache pipeline_cache;
        VulkanFrameLightingResource frame_lighting_resource;
        VulkanRenderResourceCache resource_cache;
        VulkanRenderDataTranslator translator;
        RenderSceneFrameBuilder scene_frame_builder;
        RenderShadowFrameBuilder shadow_frame_builder;
        VulkanDrawListBuilder draw_list_builder;
        VulkanGraphExecutor graph_executor;
        VulkanFrameGraphBuilder frame_graph_builder;
        VulkanDebugDrawBuffer debug_draw_buffer;
        VulkanAoPass ao_pass;
        VulkanBloomPass bloom_pass;
        VulkanSsrPass ssr_pass;
        VulkanSmaaPass smaa_pass;
        VulkanDebugDrawPass debug_draw_pass;
        VulkanShadowPass shadow_pass;
        VulkanSkyboxPass skybox_pass;
        VulkanForwardPass forward_pass;
        VulkanPostProcessPass post_process_pass;
        VulkanObjectIdPass object_id_pass;
        VulkanViewportTarget viewport_target;
        VulkanSceneColorTarget scene_color_target;
        VulkanAoTarget ao_target;
        VulkanBloomTarget bloom_target;
        VulkanSsrTarget ssr_target;
        VulkanSmaaTarget smaa_target;
        VulkanPickingManager picking_manager;
        VulkanReflectionProbeManager reflection_probe_manager;
        VulkanReflectionProbeCaptureCallbacks reflection_probe_capture_callbacks;
        VulkanShadowMapTarget shadow_target;
        VulkanPointShadowTarget point_shadow_target;
        VulkanShadowMapTarget rect_shadow_target;
        VulkanImGuiRenderer imgui_renderer;
        VulkanFrameOutputRoute last_output_route = VulkanFrameOutputRoute::DirectSwapchain;
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

    bool VulkanRendererSystem::requestReflectionProbeCapture(const ReflectionProbeCaptureRequest& request) {
        return m_backend ? m_backend->requestReflectionProbeCapture(request) : false;
    }

    bool VulkanRendererSystem::clearReflectionProbeCapture(int entity_id) {
        return m_backend ? m_backend->clearReflectionProbeCapture(entity_id) : false;
    }

    ReflectionProbeCaptureState VulkanRendererSystem::getReflectionProbeCaptureState(int entity_id) const {
        return m_backend ? m_backend->getReflectionProbeCaptureState(entity_id) : ReflectionProbeCaptureState{};
    }

    ReflectionProbeCaptureQueueState VulkanRendererSystem::getReflectionProbeCaptureQueueState() const {
        return m_backend ? m_backend->getReflectionProbeCaptureQueueState() : ReflectionProbeCaptureQueueState{};
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
