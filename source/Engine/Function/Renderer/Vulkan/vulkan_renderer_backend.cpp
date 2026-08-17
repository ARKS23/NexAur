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
#include "Function/Renderer/Vulkan/features/vulkan_ao_feature.h"
#include "Function/Renderer/Vulkan/features/vulkan_bloom_feature.h"
#include "Function/Renderer/Vulkan/features/vulkan_post_process_feature.h"
#include "Function/Renderer/Vulkan/features/vulkan_shadow_feature.h"
#include "Function/Renderer/Vulkan/features/vulkan_smaa_feature.h"
#include "Function/Renderer/Vulkan/features/vulkan_ssr_feature.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list_builder.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_prepared_frame.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_context.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_graph_builder.h"
#include "Function/Renderer/Vulkan/frame/vulkan_render_feature_plan.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_executor.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_static_mesh_blas_cache.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_tlas_manager.h"
#include "Function/Renderer/Vulkan/core/vulkan_device_context.h"
#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/core/vulkan_retirement_queue.h"
#include "Function/Renderer/Vulkan/core/vulkan_swapchain_manager.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/passes/vulkan_debug_draw_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_forward_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_object_id_pass.h"
#include "Function/Renderer/Vulkan/passes/vulkan_skybox_pass.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"
#include "Function/Renderer/Vulkan/targets/vulkan_scene_color_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_viewport_target.h"
#include "Function/Renderer/Vulkan/ui/vulkan_imgui_renderer.h"
#include "Function/Renderer/Vulkan/vulkan_picking_manager.h"
#include "Function/Renderer/Vulkan/vulkan_reflection_probe_manager.h"
#include "Function/Renderer/Vulkan/vulkan_render_resource_cache.h"

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
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
        bool init(const VulkanRendererInitContext& init_context) {
            if (initialized) {
                return true;
            }

            if (!init_context.valid()) {
                NX_CORE_ERROR("VulkanRendererSystem requires WindowService and AssetManager services.");
                return false;
            }

            WindowService& service = *init_context.window_service;
            asset_manager = init_context.asset_manager;

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

            if (!device_context.init(
                    native_window,
                    service.getRequiredVulkanInstanceExtensions(),
                    init_context.ray_tracing_options) ||
                !gpu_allocator.init(createResourceContext()) ||
                !shader_library.init(device.device) ||
                !descriptor_layout_cache.init(device.device) ||
                !descriptor_allocator.init(device.device) ||
                !initFrameContexts() ||
                !pipeline_cache.init(device.device, shader_library) ||
                !resource_cache.init(
                    createResourceContext(),
                    descriptor_layout_cache,
                    descriptor_allocator,
                    *asset_manager) ||
                !createSwapchain()) {
                shutdown();
                return false;
            }

            const VulkanRayTracingCapabilities& ray_tracing_capabilities =
                device_context.getRayTracingCapabilities();
            if (ray_tracing_capabilities.ray_query_enabled &&
                !static_mesh_blas_cache.init(
                    createResourceContext(),
                    device_context.getRayTracingFunctions(),
                    ray_tracing_capabilities.min_scratch_alignment)) {
                NX_CORE_WARN(
                    "Static mesh BLAS cache initialization failed; Ray Query features will remain unavailable.");
            }
            if (static_mesh_blas_cache.isInitialized() &&
                !tlas_manager.init(
                    createResourceContext(),
                    device_context.getRayTracingFunctions(),
                    ray_tracing_capabilities.min_scratch_alignment)) {
                NX_CORE_WARN(
                    "TLAS manager initialization failed; Ray Query scene instances will remain unavailable.");
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

            const RenderSettings default_settings;
            const VulkanRenderFeatureContext feature_context = createRenderFeatureContext();
            if (!scene_color_target.init(
                    createResourceContext(),
                    scene_color_format,
                    surface_width,
                    surface_height) ||
                !ao_feature.init(
                    feature_context,
                    ao_format,
                    surface_width,
                    surface_height,
                    default_settings.ao.half_resolution) ||
                !bloom_feature.init(
                    feature_context,
                    scene_color_format,
                    surface_width,
                    surface_height,
                    makeSceneColorFeatureInput()) ||
                !ssr_feature.init(
                    feature_context,
                    scene_color_format,
                    ssr_hit_mask_format,
                    surface_width,
                    surface_height) ||
                !smaa_feature.init(
                    feature_context,
                    swapchain.image_format,
                    smaa_mask_format,
                    swapchain.image_format,
                    surface_width,
                    surface_height) ||
                !shadow_feature.init(
                    feature_context,
                    default_settings) ||
                !initializeFrameShadowBindings() ||
                !post_process_feature.init(feature_context, swapchain.image_format)) {
                shutdown();
                return false;
            }

            if (!picking_manager.init(
                    createResourceContext(),
                    surface_width,
                    surface_height)) {
                shutdown();
                return false;
            }

            if (!reflection_probe_manager.init(
                    createResourceContext(),
                    scene_color_format,
                    forward_pass.getDepthFormat())) {
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
                waitForDeviceIdle("vkDeviceWaitIdle(renderer shutdown)");
            }

            imgui_renderer.shutdown();
            post_process_feature.shutdown();
            smaa_feature.shutdown();
            bloom_feature.shutdown();
            ssr_feature.shutdown();
            ao_feature.shutdown();
            debug_draw_pass.shutdown();
            skybox_pass.shutdown();
            object_id_pass.shutdown();
            reflection_probe_manager.shutdown();
            picking_manager.shutdown();
            shadow_feature.shutdown();
            scene_color_target.shutdown();
            viewport_target.shutdown();
            forward_pass.shutdown();
            cleanupSwapchain();
            tlas_manager.shutdown();
            static_mesh_blas_cache.shutdown();
            resource_cache.shutdown();
            cleanupFrameContexts();
            retirement_queue.drain();
            pipeline_cache.shutdown();
            descriptor_allocator.shutdown();
            descriptor_layout_cache.shutdown();
            shader_library.shutdown();

            gpu_allocator.shutdown();
            device_context.shutdown();
            scene_color_format = VK_FORMAT_UNDEFINED;
            ao_format = VK_FORMAT_UNDEFINED;
            ssr_hit_mask_format = VK_FORMAT_UNDEFINED;
            smaa_mask_format = VK_FORMAT_UNDEFINED;
            asset_manager = nullptr;
            initialized = false;
        }

        void render(TimeStep ts, const RenderDataPacket& render_data) {
            const auto render_start_time = std::chrono::steady_clock::now();
            last_frame_wait_ms = 0.0;
            last_frame_slot_index = current_frame_index;
            if (initialized) {
                collectSignaledFrames();
                resource_cache.refreshAsyncResources(*asset_manager);
            }
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
                *asset_manager);
            if (!resource_cache.processUploads()) {
                NX_CORE_ERROR("VulkanRendererSystem failed to process asynchronous uploads.");
            }
            prepareStaticMeshBlas(prepared_frame.draw_list);

            VulkanFrameContext& frame_context = frame_contexts[current_frame_index];
            if (!waitForFrame(frame_context)) {
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
            prepareTlas(frame_context.getFrameIndex(), prepared_frame.draw_list);
            const VulkanReflectionProbeCaptureCallbacks capture_callbacks =
                createReflectionProbeCaptureCallbacks(frame_context);
            reflection_probe_manager.processFrame(
                prepared_frame,
                resource_cache,
                *asset_manager,
                capture_callbacks);
            if (!prepareFrameTargets(prepared_frame, frame_context)) {
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
            drawFrame(prepared_frame, feature_plan, frame_context);
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

            if (!waitForDeviceIdle("vkDeviceWaitIdle(viewport resize)")) {
                return;
            }
            imgui_renderer.releaseViewportTexture();
            const bool viewport_resized = viewport_target.resize(width, height);
            const bool scene_color_resized = scene_color_target.resize(width, height);
            const bool ao_resized =
                ao_feature.resize(width, height, RenderSettings().ao.half_resolution);
            const bool bloom_resized =
                bloom_feature.resize(width, height, makeSceneColorFeatureInput());
            const bool ssr_resized = ssr_feature.resize(width, height);
            const bool smaa_resized = smaa_feature.resize(width, height);
            const bool picking_resized = picking_manager.resize(width, height);
            if (!viewport_resized ||
                !scene_color_resized ||
                !ao_resized ||
                !bloom_resized ||
                !ssr_resized ||
                !smaa_resized ||
                !picking_resized) {
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
                waitForDeviceIdle("vkDeviceWaitIdle(UI context shutdown)");
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
        VulkanResourceContext createResourceContext() {
            VulkanResourceContext context;
            context.instance = instance.instance;
            context.physical_device = physical_device.physical_device;
            context.device = device.device;
            context.graphics_queue = graphics_queue;
            context.graphics_queue_family = graphics_queue_family;
            context.api_version = device_api_version;
            context.buffer_device_address_enabled =
                device_context.getRayTracingCapabilities().ray_query_enabled;
            context.gpu_allocator = &gpu_allocator;
            context.retirement_queue = &retirement_queue;
            return context;
        }

        VulkanRenderFeatureContext createRenderFeatureContext() {
            VulkanRenderFeatureContext context;
            context.resources = createResourceContext();
            context.descriptor_layout_cache = &descriptor_layout_cache;
            context.descriptor_allocator = &descriptor_allocator;
            context.pipeline_cache = &pipeline_cache;
            return context;
        }

        void prepareStaticMeshBlas(const VulkanDrawList& draw_list) {
            if (!static_mesh_blas_cache.isInitialized() ||
                draw_list.opaque_items.empty()) {
                return;
            }

            std::vector<const VulkanMeshResource*> meshes;
            meshes.reserve(draw_list.opaque_items.size());
            for (const VulkanMeshDrawItem& draw_item : draw_list.opaque_items) {
                if (draw_item.mesh != nullptr) {
                    meshes.push_back(draw_item.mesh);
                }
            }
            static_mesh_blas_cache.prepare(meshes);
        }

        void prepareTlas(
            uint32_t frame_index,
            const VulkanDrawList& draw_list) {
            if (!tlas_manager.isInitialized()) {
                return;
            }
            tlas_manager.buildFrame(
                frame_index,
                draw_list.opaque_items,
                static_mesh_blas_cache);
        }

        bool waitForDeviceIdle(const char* operation) {
            if (device.device == VK_NULL_HANDLE) {
                return true;
            }
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkDeviceWaitIdle(device.device),
                    operation)) {
                return false;
            }
            completeFramesThrough(retirement_queue.getSubmittedSerial());
            return true;
        }

        bool waitForFrame(VulkanFrameContext& frame_context) {
            if (!frame_context.isInFlight()) {
                return true;
            }
            if (device.device == VK_NULL_HANDLE ||
                frame_context.getFence() == VK_NULL_HANDLE) {
                return false;
            }

            const auto wait_start = std::chrono::steady_clock::now();
            const VkFence fence = frame_context.getFence();
            const bool waited = VulkanDiagnosticsCollector::checkVk(
                vkWaitForFences(device.device, 1, &fence, VK_TRUE, UINT64_MAX),
                "vkWaitForFences(renderer frame context)");
            last_frame_wait_ms += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - wait_start).count();
            if (!waited) {
                return false;
            }

            completeFramesThrough(frame_context.getSubmissionSerial());
            return true;
        }

        void completeFramesThrough(uint64_t completed_serial) {
            if (completed_serial == 0) {
                return;
            }
            retirement_queue.markCompleted(completed_serial);
            resource_cache.onSubmissionsCompleted(completed_serial);
            picking_manager.onSubmissionsCompleted(completed_serial);
            for (VulkanFrameContext& frame_context : frame_contexts) {
                if (!frame_context.isInFlight() ||
                    frame_context.getSubmissionSerial() > completed_serial) {
                    continue;
                }
                swapchain_image_flights.releaseFrame(
                    frame_context.getFrameIndex(),
                    frame_context.getSubmissionSerial());
                frame_context.markCompleted();
            }
        }

        void collectSignaledFrames() {
            uint64_t completed_serial =
                resource_cache.collectCompletedUploadSerial();
            for (const VulkanFrameContext& frame_context : frame_contexts) {
                if (!frame_context.isInFlight()) {
                    continue;
                }
                const VkResult status = vkGetFenceStatus(
                    device.device,
                    frame_context.getFence());
                if (status == VK_SUCCESS) {
                    completed_serial = std::max(
                        completed_serial,
                        frame_context.getSubmissionSerial());
                } else if (status != VK_NOT_READY) {
                    VulkanDiagnosticsCollector::checkVk(
                        status,
                        "vkGetFenceStatus(renderer frame context)");
                }
            }
            completeFramesThrough(completed_serial);
        }

        VulkanReflectionProbeCaptureCallbacks createReflectionProbeCaptureCallbacks(
            VulkanFrameContext& frame_context) {
            VulkanFrameLightingResource* lighting_resource =
                &frame_context.getLightingResource();
            VulkanReflectionProbeCaptureCallbacks callbacks;
            callbacks.prepare = [this, lighting_resource](
                                    const RenderSettings& capture_settings,
                                    std::string& error_message) {
                if (shadow_feature.prepare(capture_settings) &&
                    shadow_feature.updateLightingResource(*lighting_resource)) {
                    return true;
                }

                error_message =
                    "Failed to prepare shadow targets for reflection probe capture.";
                return false;
            };
            callbacks.record_shadows = [this, lighting_resource](
                                           VkCommandBuffer target_command_buffer,
                                           const RenderView& capture_view,
                                           const VulkanDrawList& capture_draw_list,
                                           const RenderSettings& capture_settings,
                                           std::string& error_message) {
                const VulkanShadowFeatureFrames frames = shadow_feature.buildFrames(
                    capture_view,
                    capture_draw_list,
                    capture_settings,
                    capture_settings.effects_debug);
                if (!lighting_resource->update(
                        capture_draw_list,
                        frames.directional,
                        frames.point,
                        frames.rect,
                        shadow_feature.getDirectionalMapSize(),
                        shadow_feature.getPointMapSize(),
                        shadow_feature.getRectMapSize(),
                        capture_settings)) {
                    error_message =
                        "Failed to update frame globals for reflection probe capture.";
                    return false;
                }

                if (!shadow_feature.recordCapturePasses(
                        target_command_buffer,
                        capture_draw_list,
                        frames,
                        capture_settings)) {
                    error_message =
                        "Failed to record shadow maps for reflection probe capture.";
                    return false;
                }

                return true;
            };
            callbacks.record_face = [this, lighting_resource](
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
                        lighting_resource->getDescriptorSet(),
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
            availability.post_process = post_process_feature.isReady();
            availability.bloom = bloom_feature.isReady();
            availability.ao = ao_feature.isReady();
            availability.ssr = ssr_feature.isReady();
            availability.smaa = smaa_feature.isReady();
            availability.directional_shadow = shadow_feature.isDirectionalReady();
            availability.point_shadow = shadow_feature.isPointReady();
            availability.rect_shadow = shadow_feature.isRectReady();
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
            snapshot.shadow_target = shadow_feature.buildDirectionalDebugStats();
            snapshot.point_shadow_target = shadow_feature.buildPointDebugStats();
            snapshot.rect_shadow_target = shadow_feature.buildRectDebugStats();
            snapshot.post_process = post_process_feature.buildDebugStats(
                scene_frame.render_settings.post_process,
                feature_plan.getAvailability().post_process,
                feature_plan.rendersBloom());
            snapshot.bloom = bloom_feature.buildDebugStats(feature_plan.rendersBloom());
            snapshot.ao = ao_feature.buildDebugStats(feature_plan.rendersAo());
            snapshot.ssr = ssr_feature.buildDebugStats(
                scene_frame.render_settings.ssr,
                feature_plan.rendersSsr());
            snapshot.smaa = smaa_feature.buildDebugStats(
                scene_frame.render_settings.anti_aliasing,
                feature_plan.rendersSmaa());
            snapshot.effects = buildEffectsDebugStats(scene_frame.render_settings, feature_plan);
            snapshot.resources = buildResourceDebugStats(draw_list);

            debug_snapshot = std::move(snapshot);
        }

        RendererDebugBackendStats buildBackendDebugStats() const {
            RendererDebugBackendStats stats;
            stats.backend = RendererBackendType::Vulkan;
            stats.initialized = initialized;
            stats.device_api_version = VulkanDiagnosticsCollector::apiVersionToString(device_api_version);
            const VulkanRayTracingCapabilities& ray_tracing_capabilities =
                device_context.getRayTracingCapabilities();
            stats.ray_query_supported = ray_tracing_capabilities.supportsRayQuery();
            stats.ray_query_enabled = ray_tracing_capabilities.ray_query_enabled;
            stats.acceleration_structure_functions_loaded =
                device_context.getRayTracingFunctions().valid();
            stats.buffer_device_address_enabled =
                gpu_allocator.isBufferDeviceAddressEnabled();
            stats.ray_tracing_pipeline_supported = ray_tracing_capabilities.ray_tracing_pipeline;
            stats.ray_query_fallback_reason =
                ray_tracing_capabilities.unavailable_reason.empty() ?
                    "None" : ray_tracing_capabilities.unavailable_reason;
            stats.acceleration_structure_min_scratch_alignment =
                ray_tracing_capabilities.min_scratch_alignment;
            stats.acceleration_structure_max_geometry_count =
                ray_tracing_capabilities.max_geometry_count;
            stats.acceleration_structure_max_instance_count =
                ray_tracing_capabilities.max_instance_count;
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
            stats.frame_wait_ms = last_frame_wait_ms;
            stats.frame_slot_count = static_cast<uint32_t>(std::count_if(
                frame_contexts.begin(),
                frame_contexts.end(),
                [](const VulkanFrameContext& frame_context) {
                    return frame_context.isReady();
                }));
            stats.current_frame_slot = last_frame_slot_index;
            stats.frames_in_flight = static_cast<uint32_t>(std::count_if(
                frame_contexts.begin(),
                frame_contexts.end(),
                [](const VulkanFrameContext& frame_context) {
                    return frame_context.isInFlight();
                }));
            stats.swapchain_images_in_flight = static_cast<uint32_t>(
                swapchain_image_flights.getInFlightImageCount());

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
            const VulkanRetirementQueueStats retirement_stats =
                retirement_queue.getStats();
            stats.gpu_submitted_serial = retirement_stats.submitted_serial;
            stats.gpu_completed_serial = retirement_stats.completed_serial;
            stats.gpu_retirement_pending_count = retirement_stats.pending_count;
            stats.gpu_retired_count = retirement_stats.retired_count;
            stats.gpu_collected_count = retirement_stats.collected_count;
            if (!resource_cache.isInitialized()) {
                return stats;
            }

            const VulkanUploadManagerStats upload_stats =
                resource_cache.getUploadStats();
            stats.upload_pending_bytes = upload_stats.pending_bytes;
            stats.upload_submitted_bytes_this_frame =
                upload_stats.submitted_bytes_this_frame;
            stats.upload_byte_budget_per_frame =
                upload_stats.byte_budget_per_frame;
            stats.upload_pending_requests = upload_stats.pending_requests;
            stats.upload_in_flight_requests = upload_stats.in_flight_requests;
            stats.upload_submitted_requests_this_frame =
                upload_stats.submitted_requests_this_frame;
            stats.upload_request_budget_per_frame =
                upload_stats.request_budget_per_frame;
            stats.upload_ready_requests = upload_stats.ready_requests;
            stats.upload_failed_requests = upload_stats.failed_requests;
            stats.upload_cancelled_requests = upload_stats.cancelled_requests;

            stats.model_count = resource_cache.getModelCount();
            stats.texture_count = resource_cache.getTextureCount();
            stats.environment_count = resource_cache.getEnvironmentCount();
            stats.mesh_count = resource_cache.getMeshCount();
            stats.device_address_mesh_count =
                resource_cache.getDeviceAddressMeshCount();
            const VulkanStaticMeshBlasCacheStats blas_stats =
                static_mesh_blas_cache.getStats();
            stats.static_mesh_blas_cache_ready = blas_stats.initialized;
            stats.static_mesh_blas_entry_count = blas_stats.entry_count;
            stats.static_mesh_blas_ready_count = blas_stats.ready_entry_count;
            stats.static_mesh_blas_failed_count = blas_stats.failed_entry_count;
            stats.static_mesh_blas_build_count = blas_stats.build_count;
            stats.static_mesh_blas_cache_hit_count = blas_stats.cache_hit_count;
            stats.static_mesh_blas_failed_build_count = blas_stats.failed_build_count;
            stats.static_mesh_blas_bytes = blas_stats.acceleration_structure_bytes;
            stats.static_mesh_blas_last_failure = blas_stats.last_failure_reason;
            const VulkanTlasBuildStats tlas_stats = tlas_manager.getStats();
            stats.tlas_manager_ready = tlas_stats.initialized;
            stats.tlas_ready = tlas_stats.ready;
            stats.tlas_source_instance_count = tlas_stats.source_instance_count;
            stats.tlas_built_instance_count = tlas_stats.built_instance_count;
            stats.tlas_skipped_blas_count = tlas_stats.skipped_blas_count;
            stats.tlas_skipped_transform_count = tlas_stats.skipped_transform_count;
            stats.tlas_build_count = tlas_stats.build_count;
            stats.tlas_instance_buffer_bytes = tlas_stats.instance_buffer_bytes;
            stats.tlas_bytes = tlas_stats.acceleration_structure_bytes;
            stats.tlas_last_failure = tlas_stats.last_failure_reason;
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

        VulkanFeatureImageInput makeSceneColorFeatureInput() const {
            VulkanFeatureImageInput input;
            if (!scene_color_target.isReady()) {
                return input;
            }
            input.view = scene_color_target.getColorImageView();
            input.sampler = scene_color_target.getSampler();
            input.extent = scene_color_target.getExtent();
            return input;
        }

        VulkanPostProcessInput makePostProcessInput(
            const VulkanFeatureImageInput& color_input,
            VkImageView scene_depth_view) const {
            VulkanPostProcessInput input;
            input.color_view = color_input.view;
            input.sampler = color_input.sampler;
            input.layout = color_input.layout;

            const VulkanShadowFeatureInput shadows = shadow_feature.getPostProcessInput();
            input.shadow_view = shadows.directional_view;
            input.shadow_sampler = shadows.directional_sampler;
            input.shadow_layout = shadows.layout;
            input.shadow_layer_count = shadows.directional_layer_count;
            input.point_shadow_view = shadows.point_view;
            input.point_shadow_sampler = shadows.point_sampler;
            input.point_shadow_layout = shadows.layout;
            input.point_shadow_layer_count = shadows.point_layer_count;
            input.rect_shadow_view = shadows.rect_view;
            input.rect_shadow_sampler = shadows.rect_sampler;
            input.rect_shadow_layout = shadows.layout;
            input.rect_shadow_layer_count = shadows.rect_layer_count;
            input.scene_depth_view = scene_depth_view;
            input.scene_depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            const VulkanAoFeatureInput ao = ao_feature.getPostProcessInput();
            input.ao_raw_view = ao.raw_view;
            input.ao_blurred_view = ao.blurred_view;
            input.ao_sampler = ao.sampler;
            input.ao_layout = ao.layout;

            const VulkanSsrFeatureInput ssr = ssr_feature.getPostProcessInput();
            input.ssr_raw_reflection_view = ssr.raw_reflection_view;
            input.ssr_hit_mask_view = ssr.hit_mask_view;
            input.ssr_sampler = ssr.sampler;
            input.ssr_layout = ssr.layout;
            return input;
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

            if (!bloom_feature.recreateSwapchainResources(makeSceneColorFeatureInput())) {
                return false;
            }
            if (!ssr_feature.recreateSwapchainResources()) {
                return false;
            }
            if (!smaa_feature.recreateSwapchainResources(swapchain.image_format)) {
                return false;
            }
            if (!post_process_feature.recreateSwapchainResources(swapchain.image_format)) {
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

            if (!waitForDeviceIdle("vkDeviceWaitIdle(swapchain recreation)")) {
                return false;
            }
            cleanupSwapchain();
            return createSwapchain();
        }

        void cleanupSwapchain() {
            cleanupRenderFinishedSemaphores();
            smaa_feature.cleanupSwapchainResources();
            post_process_feature.cleanupSwapchainResources();
            bloom_feature.cleanupSwapchainResources();
            ssr_feature.cleanupSwapchainResources();
            debug_draw_pass.cleanupResources();
            skybox_pass.cleanupResources();
            forward_pass.cleanupSwapchainResources();
            swapchain_manager.shutdown();
        }

        bool initFrameContexts() {
            const VulkanResourceContext context = createResourceContext();
            for (uint32_t frame_index = 0;
                 frame_index < static_cast<uint32_t>(frame_contexts.size());
                 ++frame_index) {
                if (!frame_contexts[frame_index].init(
                        context,
                        descriptor_layout_cache,
                        descriptor_allocator,
                        frame_index)) {
                    cleanupFrameContexts();
                    return false;
                }
            }
            current_frame_index = 0;
            last_frame_slot_index = 0;
            return true;
        }

        void cleanupFrameContexts() {
            for (VulkanFrameContext& frame_context : frame_contexts) {
                frame_context.shutdown();
            }
            swapchain_image_flights.reset();
            current_frame_index = 0;
            last_frame_slot_index = 0;
            last_frame_wait_ms = 0.0;
        }

        bool initializeFrameShadowBindings() {
            for (VulkanFrameContext& frame_context : frame_contexts) {
                if (!shadow_feature.updateLightingResource(
                        frame_context.getLightingResource())) {
                    return false;
                }
            }
            return true;
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
            swapchain_image_flights.reset(render_finished_semaphores.size());
            return true;
        }

        void cleanupRenderFinishedSemaphores() {
            for (VkSemaphore semaphore : render_finished_semaphores) {
                if (semaphore != VK_NULL_HANDLE) {
                    vkDestroySemaphore(device.device, semaphore, nullptr);
                }
            }
            render_finished_semaphores.clear();
            swapchain_image_flights.reset();
        }

        bool prepareFrameTargets(
            const VulkanPreparedFrame& prepared_frame,
            VulkanFrameContext& frame_context) {
            const RenderSettings& render_settings = prepared_frame.scene.render_settings;
            const VulkanDrawList& draw_list = prepared_frame.draw_list;
            const bool ready =
                shadow_feature.prepare(render_settings) &&
                shadow_feature.updateLightingResource(
                    frame_context.getLightingResource()) &&
                ao_feature.prepare(
                    render_settings.ao,
                    draw_list.view.viewport_width,
                    draw_list.view.viewport_height);
            collectSignaledFrames();
            return ready;
        }

        void drawFrame(
            const VulkanPreparedFrame& prepared_frame,
            const VulkanRenderFeaturePlan& feature_plan,
            VulkanFrameContext& frame_context) {
            const VulkanDrawList& draw_list = prepared_frame.draw_list;
            const RenderSettings& render_settings = prepared_frame.scene.render_settings;
            if (swapchain.swapchain == VK_NULL_HANDLE || swapchain_images.empty()) {
                return;
            }

            const VulkanShadowFeatureFrames shadow_frames = shadow_feature.buildFrames(
                prepared_frame.scene.view,
                draw_list,
                render_settings,
                feature_plan.getDebugSettings());
            VulkanFrameLightingResource& lighting_resource =
                frame_context.getLightingResource();
            if (!lighting_resource.update(
                    draw_list,
                    shadow_frames.directional,
                    shadow_frames.point,
                    shadow_frames.rect,
                    shadow_feature.getDirectionalMapSize(),
                    shadow_feature.getPointMapSize(),
                    shadow_feature.getRectMapSize(),
                    render_settings)) {
                return;
            }

            if (!frame_context.getDebugDrawBuffer().upload(draw_list.debug_draw)) {
                return;
            }

            const VkSemaphore image_available =
                frame_context.getImageAvailableSemaphore();
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

            if (const VulkanFrameSubmission* tracked_submission =
                    swapchain_image_flights.get(image_index)) {
                const VulkanFrameSubmission submission = *tracked_submission;
                if (submission.frame_index >= frame_contexts.size()) {
                    NX_CORE_ERROR(
                        "Swapchain image {} referenced invalid frame slot {}.",
                        image_index,
                        submission.frame_index);
                    return;
                }
                VulkanFrameContext& owner = frame_contexts[submission.frame_index];
                if (owner.isInFlight() &&
                    owner.getSubmissionSerial() == submission.serial &&
                    !waitForFrame(owner)) {
                    return;
                }
                swapchain_image_flights.releaseFrame(
                    submission.frame_index,
                    submission.serial);
            }

            if (!recordDrawCommands(
                    frame_context,
                    image_index,
                    draw_list,
                    shadow_frames,
                    render_settings,
                    feature_plan)) {
                return;
            }

            const VkFence frame_fence = frame_context.getFence();
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkResetFences(device.device, 1, &frame_fence),
                    "vkResetFences(renderer frame context)")) {
                return;
            }

            const VkCommandBuffer command_buffer = frame_context.getCommandBuffer();
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

            if (!VulkanDiagnosticsCollector::checkVk(
                    vkQueueSubmit(
                        graphics_queue,
                        1,
                        &submit_info,
                        frame_fence),
                    "vkQueueSubmit(renderer frame context)")) {
                return;
            }
            const uint64_t submission_serial = retirement_queue.markSubmitted();
            frame_context.markSubmitted(submission_serial);
            swapchain_image_flights.markSubmitted(
                image_index,
                frame_context.getFrameIndex(),
                submission_serial);
            picking_manager.onFrameSubmitted(
                frame_context.getFrameIndex(),
                submission_serial);
            current_frame_index =
                (current_frame_index + 1u) %
                static_cast<uint32_t>(frame_contexts.size());

            VkPresentInfoKHR present_info{};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = &render_finished;
            present_info.swapchainCount = 1;
            present_info.pSwapchains = &swapchain.swapchain;
            present_info.pImageIndices = &image_index;

            VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);
            if (acquire_result == VK_SUBOPTIMAL_KHR ||
                present_result == VK_ERROR_OUT_OF_DATE_KHR ||
                present_result == VK_SUBOPTIMAL_KHR) {
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
            VulkanFrameContext& frame_context,
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            const VulkanShadowFeatureFrames& shadow_frames,
            const RenderSettings& render_settings,
            const VulkanRenderFeaturePlan& feature_plan) {
            if (image_index >= swapchain_images.size()) {
                return false;
            }
            picking_manager.beginFrameRecording(frame_context.getFrameIndex());

            const VkCommandBuffer command_buffer = frame_context.getCommandBuffer();
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
                    frame_context,
                    image_index,
                    draw_list,
                    shadow_frames,
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
            VulkanFrameContext& frame_context,
            uint32_t image_index,
            const VulkanDrawList& draw_list,
            const VulkanShadowFeatureFrames& shadow_frames,
            const RenderSettings& render_settings,
            const VulkanRenderFeaturePlan& feature_plan) {
            const bool render_to_viewport =
                feature_plan.getOutputRoute() == VulkanFrameOutputRoute::Viewport;
            const VulkanRenderTarget scene_target = render_to_viewport ?
                makeViewportSceneRenderTarget() :
                makeSwapchainSceneRenderTarget(image_index);

            const VulkanShadowFeatureGraphResources shadow_resources =
                shadow_feature.addGraphResources(graph);
            const VulkanAoFeatureGraphResources ao_resources =
                ao_feature.addGraphResources(graph);
            const VulkanSsrFeatureGraphResources ssr_resources =
                ssr_feature.addGraphResources(graph);
            VulkanFrameGraphResources resources;
            resources.directional_shadow_depth = shadow_resources.directional_depth;
            resources.point_shadow_depth = shadow_resources.point_depth;
            resources.rect_shadow_depth = shadow_resources.rect_depth;
            resources.scene_color = addSceneColorImage(graph);
            resources.scene_depth = render_to_viewport ?
                addViewportDepthImage(graph) :
                addSwapchainDepthImage(graph);
            resources.ao_raw = ao_resources.raw;
            resources.ao_blurred = ao_resources.blurred;
            resources.ssr_raw_reflection = ssr_resources.raw_reflection;
            resources.ssr_hit_mask = ssr_resources.hit_mask;
            resources.swapchain_color = addSwapchainColorImage(graph, image_index);
            resources.final_color = render_to_viewport ?
                addViewportColorImage(graph) :
                resources.swapchain_color;
            if (feature_plan.rendersSmaa()) {
                resources.smaa_source = smaa_feature.addSourceImage(graph);
            }

            const VulkanFeatureImageInput scene_color_input = makeSceneColorFeatureInput();
            const uint32_t frame_index = frame_context.getFrameIndex();
            const VkDescriptorSet frame_descriptor_set =
                frame_context.getLightingResource().getDescriptorSet();
            VulkanFeatureImageInput post_process_color_input = scene_color_input;
            VulkanFrameGraphCallbacks callbacks;
            callbacks.add_directional_shadow =
                [this, &draw_list, &shadow_frames](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle depth) {
                    return shadow_feature.addDirectionalPass(
                        target_graph,
                        depth,
                        draw_list,
                        shadow_frames.directional);
                };
            callbacks.add_point_shadow =
                [this, &draw_list, &shadow_frames](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle depth) {
                    return shadow_feature.addPointPass(
                        target_graph,
                        depth,
                        draw_list,
                        shadow_frames.point);
                };
            callbacks.add_rect_shadow =
                [this, &draw_list, &shadow_frames](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle depth) {
                    return shadow_feature.addRectPass(
                        target_graph,
                        depth,
                        draw_list,
                        shadow_frames.rect);
                };
            callbacks.add_skybox =
                [this, &draw_list](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle color) {
                    return addSkyboxPass(target_graph, color, makeSceneSkyboxTarget(), draw_list);
                };
            callbacks.record_forward =
                [this, &draw_list, scene_target, frame_descriptor_set](VkCommandBuffer target_command_buffer) {
                    return forward_pass.record(
                        target_command_buffer,
                        scene_target,
                        draw_list,
                        frame_descriptor_set,
                        resolveEnvironmentDescriptorSet(draw_list),
                        resolveReflectionProbeDescriptorSet(draw_list),
                        forwardAfterSkyboxOptions());
                };
            callbacks.add_ao =
                [this, &draw_list, scene_target, &render_settings, frame_index](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle depth,
                    VulkanGraphImageHandle raw,
                    VulkanGraphImageHandle blurred) {
                    return ao_feature.addPasses(
                        target_graph,
                        depth,
                        VulkanAoFeatureGraphResources{ raw, blurred },
                        scene_target.depth_view,
                        draw_list.view,
                        render_settings.ao,
                        frame_index);
                };
            callbacks.add_ssr =
                [this, &draw_list, scene_target, scene_color_input, &render_settings, frame_index](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle scene_color,
                    VulkanGraphImageHandle scene_depth,
                    VulkanGraphImageHandle raw_reflection,
                    VulkanGraphImageHandle hit_mask) {
                    return ssr_feature.addPasses(
                        target_graph,
                        scene_color,
                        scene_depth,
                        VulkanSsrFeatureGraphResources{ raw_reflection, hit_mask },
                        scene_color_input,
                        scene_target.depth_view,
                        draw_list.view,
                        render_settings.ssr,
                        frame_index);
                };
            callbacks.add_debug_draw =
                [this, &frame_context, scene_target](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle color,
                    VulkanGraphImageHandle depth) {
                    return addDebugDrawPass(
                        target_graph,
                        color,
                        depth,
                        scene_target,
                        frame_context);
                };
            callbacks.add_object_id =
                [this, &draw_list, frame_index](VulkanPassGraph& target_graph) {
                    return addObjectIdPass(
                        target_graph,
                        draw_list,
                        frame_index);
                };
            callbacks.add_bloom =
                [this, scene_color_input, &render_settings, &feature_plan, &post_process_color_input](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle scene_color,
                    VulkanGraphImageHandle& composite_color) {
                    VulkanBloomFeatureOutput output;
                    if (!bloom_feature.addPasses(
                            target_graph,
                            scene_color,
                            scene_color_input,
                            render_settings.post_process,
                            feature_plan.getDebugSettings(),
                            output)) {
                        return false;
                    }
                    composite_color = output.color;
                    post_process_color_input = output.input;
                    return true;
                };
            callbacks.add_post_process =
                [this, image_index, render_to_viewport, scene_target, &render_settings, &feature_plan, &post_process_color_input, frame_index](
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
                    return post_process_feature.addPass(
                        target_graph,
                        input_color,
                        output_color,
                        scene_depth,
                        ao_raw,
                        ao_blurred,
                        ssr_raw_reflection,
                        ssr_hit_mask,
                        target,
                        makePostProcessInput(
                            post_process_color_input,
                            scene_target.depth_view),
                        render_settings.post_process,
                        ao_settings,
                        ssr_settings,
                        feature_plan.getPostProcessDebugSettings(),
                        feature_plan.isolatesForwardDebug(),
                        frame_index);
                };
            callbacks.add_smaa =
                [this, image_index, render_to_viewport, &render_settings, &feature_plan, frame_index](
                    VulkanPassGraph& target_graph,
                    VulkanGraphImageHandle source_color,
                    VulkanGraphImageHandle output_color) {
                    const VulkanSmaaRenderTarget target = render_to_viewport ?
                        makeViewportSmaaOutputTarget() :
                        makeSwapchainSmaaOutputTarget(image_index);
                    return smaa_feature.addPasses(
                        target_graph,
                        source_color,
                        output_color,
                        target,
                        render_settings.anti_aliasing,
                        feature_plan.getDebugSettings(),
                        frame_index);
                };
            if (render_to_viewport) {
                callbacks.record_imgui =
                    [this, image_index](VkCommandBuffer target_command_buffer) {
                        return recordImGuiToSwapchain(target_command_buffer, image_index);
                    };
            }

            return frame_graph_builder.build(graph, feature_plan, resources, callbacks);
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
            VulkanRenderTarget target,
            const VulkanFrameContext& frame_context) {
            const VulkanDebugDrawBuffer* debug_draw_buffer =
                &frame_context.getDebugDrawBuffer();
            if (!debug_draw_buffer->hasVertices()) {
                return true;
            }
            if (!color_image.valid() || !depth_image.valid() || !target.valid()) {
                return false;
            }
            const VkDescriptorSet frame_descriptor_set =
                frame_context.getLightingResource().getDescriptorSet();

            graph.addPass("DebugDraw")
                .readWriteImage(color_image, VulkanGraphImageUsage::ColorAttachment)
                .readWriteImage(depth_image, VulkanGraphImageUsage::DepthStencilAttachment)
                .execute([this, target, debug_draw_buffer, frame_descriptor_set](VkCommandBuffer target_command_buffer) {
                    return debug_draw_pass.record(
                        target_command_buffer,
                        target,
                        *debug_draw_buffer,
                        frame_descriptor_set);
                });
            return true;
        }

        bool addObjectIdPass(
            VulkanPassGraph& graph,
            const VulkanDrawList& draw_list,
            uint32_t frame_index) {
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

            return picking_manager.addReadbackPass(
                graph,
                object_id,
                frame_index);
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
            const VulkanSmaaRenderTarget source_target = smaa_feature.getSourceRenderTarget();
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

    private:
        VulkanDeviceContext device_context;
        VulkanGpuAllocator gpu_allocator;
        VulkanRetirementQueue retirement_queue;
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

        std::array<VulkanFrameContext, kVulkanFramesInFlight> frame_contexts;
        VulkanSwapchainImageFlightTracker swapchain_image_flights;
        std::vector<VkSemaphore> render_finished_semaphores;
        uint32_t current_frame_index = 0;
        uint32_t last_frame_slot_index = 0;
        double last_frame_wait_ms = 0.0;

        VulkanShaderLibrary shader_library;
        VulkanDescriptorLayoutCache descriptor_layout_cache;
        VulkanDescriptorAllocator descriptor_allocator;
        VulkanPipelineCache pipeline_cache;
        VulkanRenderResourceCache resource_cache;
        VulkanStaticMeshBlasCache static_mesh_blas_cache;
        VulkanTlasManager tlas_manager;
        AssetManager* asset_manager = nullptr;
        VulkanRenderDataTranslator translator;
        RenderSceneFrameBuilder scene_frame_builder;
        VulkanDrawListBuilder draw_list_builder;
        VulkanGraphExecutor graph_executor;
        VulkanFrameGraphBuilder frame_graph_builder;
        VulkanAoFeature ao_feature;
        VulkanSsrFeature ssr_feature;
        VulkanBloomFeature bloom_feature;
        VulkanSmaaFeature smaa_feature;
        VulkanShadowFeature shadow_feature;
        VulkanPostProcessFeature post_process_feature;
        VulkanDebugDrawPass debug_draw_pass;
        VulkanSkyboxPass skybox_pass;
        VulkanForwardPass forward_pass;
        VulkanObjectIdPass object_id_pass;
        VulkanViewportTarget viewport_target;
        VulkanSceneColorTarget scene_color_target;
        VulkanPickingManager picking_manager;
        VulkanReflectionProbeManager reflection_probe_manager;
        VulkanImGuiRenderer imgui_renderer;
        VulkanFrameOutputRoute last_output_route = VulkanFrameOutputRoute::DirectSwapchain;
        RendererDebugSnapshot debug_snapshot;
    };

    VulkanRendererSystem::VulkanRendererSystem()
        : m_backend(std::make_unique<Backend>()) {}

    VulkanRendererSystem::~VulkanRendererSystem() {
        shutdown();
    }

    bool VulkanRendererSystem::init(const VulkanRendererInitContext& context) {
        return m_backend->init(context);
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
