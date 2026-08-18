#include "pch.h"
#include "renderer_debug_panel.h"

#include "Editor/Widgets/editor_widgets.h"
#include "Function/Renderer/renderer_debug_service.h"

#include <imgui.h>

namespace NexAur {
    namespace {
        const char* boolToText(bool value) {
            return value ? "Yes" : "No";
        }

        const char* backendToText(RendererBackendType backend) {
            switch (backend) {
            case RendererBackendType::Vulkan:
                return "Vulkan";
            case RendererBackendType::Unknown:
            default:
                return "Unknown";
            }
        }

        const char* viewportOutputKindToText(ViewportOutputKind kind) {
            switch (kind) {
            case ViewportOutputKind::VulkanImGuiTexture:
                return "VulkanImGuiTexture";
            case ViewportOutputKind::ExternalSwapchain:
                return "ExternalSwapchain";
            case ViewportOutputKind::None:
            default:
                return "None";
            }
        }

        void drawKeyValue(const char* label, const char* value) {
            ImGui::Text("%s: %s", label, value);
        }

        void drawKeyValue(const char* label, uint32_t value) {
            ImGui::Text("%s: %u", label, value);
        }

        void drawKeyValue(const char* label, int32_t value) {
            ImGui::Text("%s: %d", label, value);
        }

        void drawKeyValue(const char* label, size_t value) {
            ImGui::Text("%s: %zu", label, value);
        }

        void drawKeyValue(const char* label, double value) {
            ImGui::Text("%s: %.3f", label, value);
        }

        void drawKeyValue64(const char* label, uint64_t value) {
            ImGui::Text("%s: %llu", label, static_cast<unsigned long long>(value));
        }

        void drawExtent(const char* label, uint32_t width, uint32_t height) {
            ImGui::Text("%s: %u x %u", label, width, height);
        }
    } // namespace

    void RendererDebugPanel::onUIRender() {
        bool& open_flag = getOpenFlag();
        if (!ImGui::Begin(getName().c_str(), &open_flag)) {
            ImGui::End();
            return;
        }

        if (!m_context || !m_context->renderer_debug_service) {
            ImGui::TextDisabled("Renderer debug service is unavailable.");
            ImGui::End();
            return;
        }

        const RendererDebugSnapshot snapshot =
            m_context->renderer_debug_service->getDebugSnapshot();

        drawRendererSection(snapshot);
        drawFrameSection(snapshot);
        drawViewSection(snapshot);
        drawTargetsSection(snapshot);
        drawEffectsSection(snapshot);
        drawResourcesSection(snapshot);

        ImGui::End();
    }

    void RendererDebugPanel::drawRendererSection(const RendererDebugSnapshot& snapshot) {
        if (!EditorWidgets::sectionHeader("Renderer")) {
            return;
        }

        drawKeyValue("Backend", backendToText(snapshot.backend.backend));
        drawKeyValue("Initialized", boolToText(snapshot.backend.initialized));
        drawKeyValue("Device API", snapshot.backend.device_api_version.c_str());
        drawKeyValue("Ray Query Supported", boolToText(snapshot.backend.ray_query_supported));
        drawKeyValue("Ray Query Enabled", boolToText(snapshot.backend.ray_query_enabled));
        drawKeyValue(
            "AS Functions Loaded",
            boolToText(snapshot.backend.acceleration_structure_functions_loaded));
        drawKeyValue(
            "Buffer Device Address",
            boolToText(snapshot.backend.buffer_device_address_enabled));
        drawKeyValue(
            "RT Pipeline Supported",
            boolToText(snapshot.backend.ray_tracing_pipeline_supported));
        drawKeyValue(
            "Ray Query Fallback",
            snapshot.backend.ray_query_fallback_reason.c_str());
        drawKeyValue64(
            "AS Scratch Alignment",
            snapshot.backend.acceleration_structure_min_scratch_alignment);
        drawKeyValue64(
            "AS Max Geometries",
            snapshot.backend.acceleration_structure_max_geometry_count);
        drawKeyValue64(
            "AS Max Instances",
            snapshot.backend.acceleration_structure_max_instance_count);
        drawKeyValue("Swapchain Ready", boolToText(snapshot.backend.swapchain_ready));
        drawExtent(
            "Swapchain Extent",
            snapshot.backend.swapchain_width,
            snapshot.backend.swapchain_height);
        drawKeyValue("Swapchain Images", snapshot.backend.swapchain_image_count);
        drawKeyValue("Swapchain Format", snapshot.backend.swapchain_format.c_str());
        drawKeyValue(
            "Viewport Output",
            viewportOutputKindToText(snapshot.backend.viewport_output_kind));
    }

    void RendererDebugPanel::drawFrameSection(const RendererDebugSnapshot& snapshot) {
        if (!EditorWidgets::sectionHeader("Frame")) {
            return;
        }

        drawKeyValue("Engine Delta Ms", snapshot.frame.engine_delta_ms);
        drawKeyValue("Renderer CPU Ms", snapshot.frame.renderer_cpu_ms);
        drawKeyValue("Frame Wait Ms", snapshot.frame.frame_wait_ms);
        drawKeyValue("Frame Slots", snapshot.frame.frame_slot_count);
        drawKeyValue("Current Frame Slot", snapshot.frame.current_frame_slot);
        drawKeyValue("Frames In Flight", snapshot.frame.frames_in_flight);
        drawKeyValue(
            "Swapchain Images In Flight",
            snapshot.frame.swapchain_images_in_flight);
        drawKeyValue("Opaque Objects", snapshot.frame.opaque_object_count);
        drawKeyValue("Transparent Objects", snapshot.frame.transparent_object_count);
        drawKeyValue("Opaque Draw Items", snapshot.frame.opaque_draw_item_count);
        drawKeyValue("Transparent Draw Items", snapshot.frame.transparent_draw_item_count);
        drawKeyValue("Point Lights", snapshot.frame.point_light_count);
        drawKeyValue("Point Shadow Requests", snapshot.frame.point_shadow_request_count);
        drawKeyValue("Shadowed Point Lights", snapshot.frame.shadowed_point_light_count);
        drawKeyValue(
            "Point Shadows Clipped",
            snapshot.frame.point_shadow_request_count > snapshot.frame.shadowed_point_light_count ?
                snapshot.frame.point_shadow_request_count - snapshot.frame.shadowed_point_light_count :
                0u);
        drawKeyValue("Rect Lights", snapshot.frame.rect_light_count);
        drawKeyValue("Rect Lights Clipped", snapshot.frame.rect_light_clipped_count);
        drawKeyValue("Rect Shadow Requests", snapshot.frame.rect_shadow_request_count);
        drawKeyValue("Shadowed Rect Lights", snapshot.frame.shadowed_rect_light_count);
        drawKeyValue(
            "Rect Shadows Clipped",
            snapshot.frame.rect_shadow_request_count > snapshot.frame.shadowed_rect_light_count ?
                snapshot.frame.rect_shadow_request_count - snapshot.frame.shadowed_rect_light_count :
                0u);
        drawKeyValue("Reflection Probes", snapshot.frame.reflection_probe_count);
        drawKeyValue("Active Reflection Probe", boolToText(snapshot.frame.active_reflection_probe));
        drawKeyValue("Debug Lines", snapshot.frame.debug_line_count);
    }

    void RendererDebugPanel::drawViewSection(const RendererDebugSnapshot& snapshot) {
        if (!EditorWidgets::sectionHeader("View")) {
            return;
        }

        drawExtent("Viewport", snapshot.view.viewport_width, snapshot.view.viewport_height);
        ImGui::Text(
            "Camera Position: %.3f, %.3f, %.3f",
            snapshot.view.camera_position.x,
            snapshot.view.camera_position.y,
            snapshot.view.camera_position.z);
        ImGui::Text("Clip: %.3f / %.3f", snapshot.view.near_clip, snapshot.view.far_clip);
    }

    void RendererDebugPanel::drawTargetsSection(const RendererDebugSnapshot& snapshot) {
        if (!EditorWidgets::sectionHeader("Targets")) {
            return;
        }

        ImGui::TextUnformatted("Viewport Target");
        drawKeyValue("  Ready", boolToText(snapshot.viewport_target.ready));
        drawExtent(
            "  Size",
            snapshot.viewport_target.width,
            snapshot.viewport_target.height);
        drawKeyValue("  Color Format", snapshot.viewport_target.color_format.c_str());
        drawKeyValue("  Depth Format", snapshot.viewport_target.depth_format.c_str());

        ImGui::Spacing();
        ImGui::TextUnformatted("HDR Scene Target");
        drawKeyValue("  Ready", boolToText(snapshot.hdr_scene_target.ready));
        drawExtent(
            "  Size",
            snapshot.hdr_scene_target.width,
            snapshot.hdr_scene_target.height);
        drawKeyValue("  Color Format", snapshot.hdr_scene_target.color_format.c_str());
        drawKeyValue("  Depth Format", snapshot.hdr_scene_target.depth_format.c_str());

        ImGui::Spacing();
        ImGui::TextUnformatted("PostProcess");
        drawKeyValue("  Enabled", boolToText(snapshot.post_process.enabled));
        drawKeyValue("  Ready", boolToText(snapshot.post_process.ready));
        drawKeyValue("  Output Format", snapshot.post_process.output_format.c_str());
        drawKeyValue("  Tone Mapping", snapshot.post_process.tone_mapping.c_str());
        ImGui::Text("  Exposure: %.2f", snapshot.post_process.exposure);
        drawKeyValue("  Bloom", boolToText(snapshot.post_process.bloom_enabled));
        ImGui::Text("  Bloom Intensity: %.3f", snapshot.post_process.bloom_intensity);
        drawKeyValue("  Color Grading", boolToText(snapshot.post_process.color_grading_enabled));
        ImGui::Text("  Grade Exposure: %.2f", snapshot.post_process.color_grading_exposure_offset);
        ImGui::Text("  Contrast: %.2f", snapshot.post_process.color_grading_contrast);
        ImGui::Text("  Saturation: %.2f", snapshot.post_process.color_grading_saturation);
        ImGui::Text("  Temperature: %.2f", snapshot.post_process.color_grading_temperature);
        ImGui::Text("  Tint: %.2f", snapshot.post_process.color_grading_tint);
        ImGui::Text("  Black / White: %.3f / %.2f", snapshot.post_process.color_grading_black_point, snapshot.post_process.color_grading_white_point);
        ImGui::Text("  Vignette: %.2f", snapshot.post_process.vignette_intensity);
        ImGui::Text("  Sharpen: %.2f", snapshot.post_process.sharpen_intensity);

        ImGui::Spacing();
        ImGui::TextUnformatted("Picking Target");
        drawKeyValue("  Ready", boolToText(snapshot.picking_target.ready));
        drawExtent("  Size", snapshot.picking_target.width, snapshot.picking_target.height);
        drawKeyValue("  ObjectId Format", snapshot.picking_target.object_id_format.c_str());
        drawKeyValue("  Depth Format", snapshot.picking_target.depth_format.c_str());
        drawKeyValue("  Frame Ready", boolToText(snapshot.picking_target.frame_ready));
        drawKeyValue("  Pending Requests", snapshot.picking_target.pending_request_count);
        drawKeyValue("  Readbacks In Flight", snapshot.picking_target.readback_in_flight_count);
        drawKeyValue("  Last Request", snapshot.picking_target.last_request_status.c_str());

        ImGui::Spacing();
        ImGui::TextUnformatted("Shadow Target");
        drawKeyValue("  Ready", boolToText(snapshot.shadow_target.ready));
        drawExtent("  Size", snapshot.shadow_target.width, snapshot.shadow_target.height);
        drawKeyValue("  Layers", snapshot.shadow_target.layer_count);
        drawKeyValue("  Depth Format", snapshot.shadow_target.depth_format.c_str());

        ImGui::Spacing();
        ImGui::TextUnformatted("Point Shadow Target");
        drawKeyValue("  Ready", boolToText(snapshot.point_shadow_target.ready));
        drawExtent("  Size", snapshot.point_shadow_target.width, snapshot.point_shadow_target.height);
        drawKeyValue("  Layers", snapshot.point_shadow_target.layer_count);
        drawKeyValue("  Depth Format", snapshot.point_shadow_target.depth_format.c_str());

        ImGui::Spacing();
        ImGui::TextUnformatted("Rect Shadow Target");
        drawKeyValue("  Ready", boolToText(snapshot.rect_shadow_target.ready));
        drawExtent("  Size", snapshot.rect_shadow_target.width, snapshot.rect_shadow_target.height);
        drawKeyValue("  Layers", snapshot.rect_shadow_target.layer_count);
        drawKeyValue("  Depth Format", snapshot.rect_shadow_target.depth_format.c_str());

        ImGui::Spacing();
        ImGui::TextUnformatted("Bloom Target");
        drawKeyValue("  Enabled", boolToText(snapshot.bloom.enabled));
        drawKeyValue("  Ready", boolToText(snapshot.bloom.ready));
        drawExtent("  Size", snapshot.bloom.width, snapshot.bloom.height);
        drawKeyValue("  Mip Count", snapshot.bloom.mip_count);
        drawKeyValue("  Color Format", snapshot.bloom.color_format.c_str());

        ImGui::Spacing();
        ImGui::TextUnformatted("AO Target");
        drawKeyValue("  Enabled", boolToText(snapshot.ao.enabled));
        drawKeyValue("  Ready", boolToText(snapshot.ao.ready));
        drawExtent("  Size", snapshot.ao.width, snapshot.ao.height);
        drawKeyValue("  Color Format", snapshot.ao.color_format.c_str());
        drawKeyValue("  Half Resolution", boolToText(snapshot.ao.half_resolution));
        drawKeyValue("  Requested Method", snapshot.ao.requested_mode.c_str());
        drawKeyValue("  Active Technique", snapshot.ao.active_technique.c_str());
        drawKeyValue("  RTAO Available", boolToText(snapshot.ao.ray_query_available));
        drawKeyValue("  RTAO Active", boolToText(snapshot.ao.ray_query_active));
        drawKeyValue("  RTAO Fallback", snapshot.ao.fallback_reason.c_str());
        drawKeyValue("  RTAO Ray Count", snapshot.ao.ray_count);
        ImGui::Text("  Filter Depth: %.2f", snapshot.ao.filter_depth_threshold);
        ImGui::Text("  Filter Normal: %.2f", snapshot.ao.filter_normal_threshold);
        drawKeyValue(
            "  Spatial Filter",
            boolToText(snapshot.ao.spatial_filter_enabled));

        ImGui::Spacing();
        ImGui::TextUnformatted("SSR Target");
        drawKeyValue("  Enabled", boolToText(snapshot.ssr.enabled));
        drawKeyValue("  Ready", boolToText(snapshot.ssr.ready));
        drawExtent("  Size", snapshot.ssr.width, snapshot.ssr.height);
        drawKeyValue("  Reflection Format", snapshot.ssr.reflection_format.c_str());
        drawKeyValue("  Hit Mask Format", snapshot.ssr.hit_mask_format.c_str());
        ImGui::Text("  Max Distance: %.1f", snapshot.ssr.max_distance);
        drawKeyValue("  Max Steps", snapshot.ssr.max_steps);
        ImGui::Text("  Thickness: %.3f", snapshot.ssr.thickness);
        ImGui::Text("  Stride: %.2f", snapshot.ssr.stride);
        ImGui::Text("  Roughness Fade: %.2f", snapshot.ssr.roughness_fade);
        ImGui::Text("  Edge Fade: %.2f", snapshot.ssr.edge_fade);
        ImGui::Text("  Intensity: %.2f", snapshot.ssr.intensity);

        ImGui::Spacing();
        ImGui::TextUnformatted("SMAA Target");
        drawKeyValue("  Enabled", boolToText(snapshot.smaa.enabled));
        drawKeyValue("  Ready", boolToText(snapshot.smaa.ready));
        drawExtent("  Size", snapshot.smaa.width, snapshot.smaa.height);
        drawKeyValue("  Source Format", snapshot.smaa.source_format.c_str());
        drawKeyValue("  Edge Format", snapshot.smaa.edge_format.c_str());
        drawKeyValue("  Blend Format", snapshot.smaa.blend_format.c_str());
        drawKeyValue("  Mode", snapshot.smaa.mode.c_str());
        ImGui::Text("  Edge Threshold: %.3f", snapshot.smaa.edge_threshold);
        ImGui::Text("  Contrast Factor: %.2f", snapshot.smaa.contrast_factor);
        drawKeyValue("  Search Steps", snapshot.smaa.max_search_steps);
        ImGui::Text("  Blend Strength: %.2f", snapshot.smaa.blend_strength);
    }

    void RendererDebugPanel::drawEffectsSection(const RendererDebugSnapshot& snapshot) {
        if (!EditorWidgets::sectionHeader("Effects")) {
            return;
        }

        drawKeyValue("Lighting Preset", snapshot.effects.lighting_preset.c_str());
        drawKeyValue("Debug View", snapshot.effects.debug_view.c_str());
        drawKeyValue("Bloom Mip", snapshot.effects.bloom_mip);
        drawKeyValue("Shadow Cascade", snapshot.effects.shadow_cascade);
        drawKeyValue("Point Shadow Layer", snapshot.effects.point_shadow_layer);
        drawKeyValue("Rect Shadow Layer", snapshot.effects.rect_shadow_layer);
        drawKeyValue("Bloom Debug Ready", boolToText(snapshot.effects.bloom_debug_available));
        drawKeyValue("AO Debug Ready", boolToText(snapshot.effects.ao_debug_available));
        drawKeyValue("SSR Debug Ready", boolToText(snapshot.effects.ssr_debug_available));
        drawKeyValue("Ray Query Debug Ready", boolToText(snapshot.effects.ray_query_debug_available));
        drawKeyValue("SMAA Debug Ready", boolToText(snapshot.effects.smaa_debug_available));
        drawKeyValue("Shadow Debug Ready", boolToText(snapshot.effects.shadow_debug_available));
        drawKeyValue("Point Shadow Debug Ready", boolToText(snapshot.effects.point_shadow_debug_available));
        drawKeyValue("Rect Shadow Debug Ready", boolToText(snapshot.effects.rect_shadow_debug_available));
        drawKeyValue(
            "Ray Query Shadow Available",
            boolToText(snapshot.effects.ray_query_shadow_available));
        drawKeyValue(
            "Ray Query Shadow Enabled",
            boolToText(snapshot.effects.ray_query_shadow_enabled));
        drawKeyValue(
            "Ray Query Shadow Active",
            boolToText(snapshot.effects.ray_query_shadow_active));
        drawKeyValue("Ray Query Shadow Mode", snapshot.effects.ray_query_shadow_mode.c_str());
        drawKeyValue(
            "Ray Query Shadow Fallback",
            snapshot.effects.ray_query_shadow_fallback_reason.c_str());
        ImGui::Text(
            "Ray Query Shadow Distance: %.1f",
            snapshot.effects.ray_query_shadow_max_distance);
        ImGui::Text(
            "Ray Query Shadow Normal Bias: %.4f",
            snapshot.effects.ray_query_shadow_normal_bias);
        ImGui::Text(
            "Ray Query Shadow Direction Bias: %.4f",
            snapshot.effects.ray_query_shadow_direction_bias);
        drawKeyValue(
            "Ray Query GPU Timing",
            boolToText(snapshot.effects.ray_query_gpu_timing_supported));
        drawKeyValue64(
            "Ray Query GPU Samples",
            snapshot.effects.ray_query_gpu_sample_count);
        ImGui::Text(
            "Ray Query Forward GPU: %.3f ms",
            snapshot.effects.ray_query_forward_gpu_ms);
        drawKeyValue("Point Shadows", boolToText(snapshot.effects.point_shadow_enabled));
        drawKeyValue("Rect Shadows", boolToText(snapshot.effects.rect_shadow_enabled));
        drawKeyValue("Contact Shadows", boolToText(snapshot.effects.contact_shadow_enabled));
        drawKeyValue("Point Shadow Budget", snapshot.effects.point_shadow_budget);
        drawKeyValue("Rect Shadow Budget", snapshot.effects.rect_shadow_budget);
        drawKeyValue("Point Shadow Map", snapshot.effects.point_shadow_map_resolution);
        drawKeyValue("Rect Shadow Map", snapshot.effects.rect_shadow_map_resolution);
        ImGui::Text("Point Shadow Strength: %.2f", snapshot.effects.point_shadow_strength);
        ImGui::Text("Point Shadow Bias: %.4f", snapshot.effects.point_shadow_bias);
        ImGui::Text("Point Shadow Normal Bias: %.4f", snapshot.effects.point_shadow_normal_bias);
        ImGui::Text("Point Shadow Filter: %.2f", snapshot.effects.point_shadow_filter_radius);
        ImGui::Text("Rect Shadow Strength: %.2f", snapshot.effects.rect_shadow_strength);
        ImGui::Text("Rect Shadow Bias: %.4f", snapshot.effects.rect_shadow_bias);
        ImGui::Text("Rect Shadow Normal Bias: %.4f", snapshot.effects.rect_shadow_normal_bias);
        ImGui::Text("Rect Shadow Filter: %.2f", snapshot.effects.rect_shadow_filter_radius);
        ImGui::Text("Rect Shadow Margin: %.2f", snapshot.effects.rect_shadow_projection_margin);
        drawKeyValue("Rect Soft Shadow", boolToText(snapshot.effects.rect_shadow_soft_enabled));
        ImGui::Text("Rect PCSS Light: %.2f", snapshot.effects.rect_shadow_pcss_light_radius);
        ImGui::Text("Rect PCSS Search: %.2f", snapshot.effects.rect_shadow_pcss_search_radius);
        ImGui::Text("Rect PCSS Min: %.2f", snapshot.effects.rect_shadow_pcss_min_filter_radius);
        ImGui::Text("Rect PCSS Max: %.2f", snapshot.effects.rect_shadow_pcss_max_filter_radius);
        drawKeyValue("Rect PCSS Blocker Taps", snapshot.effects.rect_shadow_pcss_blocker_taps);
        drawKeyValue("Rect PCSS Filter Taps", snapshot.effects.rect_shadow_pcss_filter_taps);
        drawKeyValue("Rect LTC Specular", boolToText(snapshot.effects.rect_ltc_specular_enabled));
        drawKeyValue("Rect LTC Only", boolToText(snapshot.effects.rect_ltc_debug_only));
        ImGui::Text("Rect LTC Scale: %.2f", snapshot.effects.rect_ltc_specular_scale);
    }

    void RendererDebugPanel::drawResourcesSection(const RendererDebugSnapshot& snapshot) {
        if (!EditorWidgets::sectionHeader("Resources")) {
            return;
        }

        drawKeyValue("Models", snapshot.resources.model_count);
        drawKeyValue("Textures", snapshot.resources.texture_count);
        drawKeyValue("Environments", snapshot.resources.environment_count);
        drawKeyValue("Meshes", snapshot.resources.mesh_count);
        drawKeyValue("Device Address Meshes", snapshot.resources.device_address_mesh_count);
        drawKeyValue("Materials", snapshot.resources.material_count);
        ImGui::SeparatorText("Static Mesh BLAS");
        drawKeyValue(
            "Cache Ready",
            boolToText(snapshot.resources.static_mesh_blas_cache_ready));
        drawKeyValue("Entries", snapshot.resources.static_mesh_blas_entry_count);
        drawKeyValue("Ready", snapshot.resources.static_mesh_blas_ready_count);
        drawKeyValue("Failed", snapshot.resources.static_mesh_blas_failed_count);
        drawKeyValue64("Builds", snapshot.resources.static_mesh_blas_build_count);
        drawKeyValue64("Cache Hits", snapshot.resources.static_mesh_blas_cache_hit_count);
        drawKeyValue64(
            "Failed Builds",
            snapshot.resources.static_mesh_blas_failed_build_count);
        drawKeyValue64("AS Bytes", snapshot.resources.static_mesh_blas_bytes);
        drawKeyValue64(
            "Memory Budget",
            snapshot.resources.static_mesh_blas_memory_budget_bytes);
        drawKeyValue(
            "Compaction",
            boolToText(snapshot.resources.static_mesh_blas_compaction_enabled));
        drawKeyValue64(
            "Compacted",
            snapshot.resources.static_mesh_blas_compaction_count);
        drawKeyValue64(
            "Compaction Failures",
            snapshot.resources.static_mesh_blas_failed_compaction_count);
        drawKeyValue64(
            "Compaction Saved Bytes",
            snapshot.resources.static_mesh_blas_compaction_saved_bytes);
        drawKeyValue64(
            "Evictions",
            snapshot.resources.static_mesh_blas_eviction_count);
        drawKeyValue64(
            "Retired Entries",
            snapshot.resources.static_mesh_blas_retired_entry_count);
        drawKeyValue64(
            "Retired Bytes",
            snapshot.resources.static_mesh_blas_retired_bytes);
        drawKeyValue64(
            "Scratch Capacity",
            snapshot.resources.static_mesh_blas_scratch_capacity_bytes);
        drawKeyValue(
            "GPU Timing",
            boolToText(snapshot.resources.static_mesh_blas_gpu_timing_supported));
        drawKeyValue64(
            "GPU Samples",
            snapshot.resources.static_mesh_blas_gpu_timing_sample_count);
        ImGui::Text(
            "Build GPU: %.3f ms",
            snapshot.resources.static_mesh_blas_build_gpu_ms);
        ImGui::Text(
            "Compaction GPU: %.3f ms",
            snapshot.resources.static_mesh_blas_compaction_gpu_ms);
        drawKeyValue(
            "Last Failure",
            snapshot.resources.static_mesh_blas_last_failure.c_str());
        ImGui::SeparatorText("TLAS");
        drawKeyValue(
            "Manager Ready",
            boolToText(snapshot.resources.tlas_manager_ready));
        drawKeyValue("Ready", boolToText(snapshot.resources.tlas_ready));
        drawKeyValue("Source Instances", snapshot.resources.tlas_source_instance_count);
        drawKeyValue("Built Instances", snapshot.resources.tlas_built_instance_count);
        drawKeyValue("Skipped BLAS", snapshot.resources.tlas_skipped_blas_count);
        drawKeyValue(
            "Skipped Transforms",
            snapshot.resources.tlas_skipped_transform_count);
        drawKeyValue(
            "Skipped Materials",
            snapshot.resources.tlas_skipped_material_count);
        drawKeyValue64("Builds", snapshot.resources.tlas_build_count);
        drawKeyValue64("Rebuilds", snapshot.resources.tlas_rebuild_count);
        drawKeyValue64("Updates", snapshot.resources.tlas_update_count);
        drawKeyValue64("Reuses", snapshot.resources.tlas_reuse_count);
        drawKeyValue64("Allocations", snapshot.resources.tlas_allocation_count);
        drawKeyValue(
            "Last Mode",
            snapshot.resources.tlas_last_build_mode.c_str());
        drawKeyValue("Instance Capacity", snapshot.resources.tlas_instance_capacity);
        drawKeyValue64(
            "Instance Bytes",
            snapshot.resources.tlas_instance_buffer_bytes);
        drawKeyValue64(
            "Instance Capacity Bytes",
            snapshot.resources.tlas_instance_buffer_capacity_bytes);
        drawKeyValue64("TLAS Bytes", snapshot.resources.tlas_bytes);
        drawKeyValue64(
            "Scratch Capacity",
            snapshot.resources.tlas_scratch_capacity_bytes);
        drawKeyValue(
            "GPU Timing",
            boolToText(snapshot.resources.tlas_gpu_timing_supported));
        drawKeyValue64(
            "GPU Samples",
            snapshot.resources.tlas_gpu_timing_sample_count);
        ImGui::Text(
            "Build GPU: %.3f ms",
            snapshot.resources.tlas_build_gpu_ms);
        drawKeyValue("Last Failure", snapshot.resources.tlas_last_failure.c_str());
        ImGui::SeparatorText("RT Scene Table");
        drawKeyValue(
            "Capability",
            boolToText(snapshot.ray_tracing_scene_table.capability_supported));
        drawKeyValue(
            "Initialized",
            boolToText(snapshot.ray_tracing_scene_table.initialized));
        drawKeyValue("Ready", boolToText(snapshot.ray_tracing_scene_table.ready));
        drawKeyValue(
            "Descriptor Ready",
            boolToText(snapshot.ray_tracing_scene_table.descriptor_ready));
        drawKeyValue(
            "BDA Geometry",
            boolToText(snapshot.ray_tracing_scene_table.bda_geometry_fetch_enabled));
        drawKeyValue(
            "Descriptor Geometry",
            boolToText(
                snapshot.ray_tracing_scene_table
                    .descriptor_indexed_geometry_fetch_enabled));
        drawKeyValue("Instances", snapshot.ray_tracing_scene_table.instance_count);
        drawKeyValue("Geometries", snapshot.ray_tracing_scene_table.geometry_count);
        drawKeyValue("Materials", snapshot.ray_tracing_scene_table.material_count);
        ImGui::Text(
            "Textures: %u / %u, overflow %u",
            snapshot.ray_tracing_scene_table.texture_count,
            snapshot.ray_tracing_scene_table.texture_capacity,
            snapshot.ray_tracing_scene_table.texture_overflow_count);
        ImGui::Text(
            "Geometry Descriptors: %u, overflow %u",
            snapshot.ray_tracing_scene_table.geometry_descriptor_capacity,
            snapshot.ray_tracing_scene_table.geometry_overflow_count);
        ImGui::Text(
            "Table Bytes: %llu instance / %llu geometry / %llu material",
            static_cast<unsigned long long>(
                snapshot.ray_tracing_scene_table.instance_buffer_bytes),
            static_cast<unsigned long long>(
                snapshot.ray_tracing_scene_table.geometry_buffer_bytes),
            static_cast<unsigned long long>(
                snapshot.ray_tracing_scene_table.material_buffer_bytes));
        drawKeyValue(
            "Table Failure",
            snapshot.ray_tracing_scene_table.last_failure_reason.c_str());
        ImGui::Text(
            "GPU Submitted Serial: %llu",
            static_cast<unsigned long long>(snapshot.resources.gpu_submitted_serial));
        ImGui::Text(
            "GPU Completed Serial: %llu",
            static_cast<unsigned long long>(snapshot.resources.gpu_completed_serial));
        drawKeyValue(
            "GPU Retirement Pending",
            snapshot.resources.gpu_retirement_pending_count);
        ImGui::Text(
            "GPU Resources Retired: %llu",
            static_cast<unsigned long long>(snapshot.resources.gpu_retired_count));
        ImGui::Text(
            "GPU Resources Collected: %llu",
            static_cast<unsigned long long>(snapshot.resources.gpu_collected_count));
        ImGui::SeparatorText("Async Upload");
        ImGui::Text(
            "Bytes: %zu pending / %zu submitted / %zu budget",
            snapshot.resources.upload_pending_bytes,
            snapshot.resources.upload_submitted_bytes_this_frame,
            snapshot.resources.upload_byte_budget_per_frame);
        ImGui::Text(
            "Requests: %u pending / %u in flight / %u submitted / %u budget",
            snapshot.resources.upload_pending_requests,
            snapshot.resources.upload_in_flight_requests,
            snapshot.resources.upload_submitted_requests_this_frame,
            snapshot.resources.upload_request_budget_per_frame);
        ImGui::Text(
            "Completed: %llu ready / %llu failed / %llu cancelled",
            static_cast<unsigned long long>(snapshot.resources.upload_ready_requests),
            static_cast<unsigned long long>(snapshot.resources.upload_failed_requests),
            static_cast<unsigned long long>(snapshot.resources.upload_cancelled_requests));
        drawKeyValue(
            "Fallback White Texture",
            boolToText(snapshot.resources.fallback_white_texture_ready));
        drawKeyValue("Fallback Material", boolToText(snapshot.resources.fallback_material_ready));
        drawKeyValue("Fallback Environment", boolToText(snapshot.resources.fallback_environment_ready));
        drawKeyValue("Environment Ready", boolToText(snapshot.resources.active_environment_ready));
        drawKeyValue("Environment Source", snapshot.resources.active_environment_name.c_str());
        drawExtent(
            "Environment CPU",
            snapshot.resources.environment_source_width,
            snapshot.resources.environment_source_height);
        drawKeyValue("Environment Size", snapshot.resources.environment_size);
        drawKeyValue("Irradiance Size", snapshot.resources.irradiance_size);
        drawKeyValue("Prefilter Size", snapshot.resources.prefilter_size);
        drawKeyValue("Prefilter Mips", snapshot.resources.prefilter_mip_count);
        drawKeyValue("BRDF LUT", boolToText(snapshot.resources.brdf_lut_ready));
        drawKeyValue("Probe Ready", boolToText(snapshot.resources.active_reflection_probe_ready));
        drawKeyValue("Probe Source", snapshot.resources.active_reflection_probe_name.c_str());
        ImGui::Text("Probe Intensity: %.2f", snapshot.resources.active_reflection_probe_intensity);
        drawKeyValue("Probe Diffuse", boolToText(snapshot.resources.active_reflection_probe_diffuse_enabled));
        ImGui::Text("Probe Diffuse Intensity: %.2f", snapshot.resources.active_reflection_probe_diffuse_intensity);
        ImGui::Text("Probe Blend Distance: %.2f", snapshot.resources.active_reflection_probe_blend_distance);
        drawKeyValue("Probe Runtime", boolToText(snapshot.resources.active_reflection_probe_runtime));
        drawKeyValue("Probe Capture Size", snapshot.resources.active_reflection_probe_capture_resolution);
        drawKeyValue("Probe Capture Status", snapshot.resources.active_reflection_probe_capture_status.c_str());
        drawKeyValue("Probe Capture Pending", snapshot.resources.reflection_probe_capture_pending_count);
        drawKeyValue("Probe Capture Budget", snapshot.resources.reflection_probe_capture_budget_per_frame);
        drawKeyValue("Runtime Probe Count", snapshot.resources.reflection_probe_runtime_capture_count);
        drawKeyValue("Pinned Probe Count", snapshot.resources.reflection_probe_pinned_capture_count);
        drawKeyValue("Runtime Probe Limit", snapshot.resources.reflection_probe_runtime_capture_limit);
        drawKeyValue("Last Captured Probe", snapshot.resources.reflection_probe_last_captured_entity_id);
    }
} // namespace NexAur
