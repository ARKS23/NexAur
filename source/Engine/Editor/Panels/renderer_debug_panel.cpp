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

        void drawKeyValue(const char* label, size_t value) {
            ImGui::Text("%s: %zu", label, value);
        }

        void drawKeyValue(const char* label, double value) {
            ImGui::Text("%s: %.3f", label, value);
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
        drawKeyValue("  Ready", boolToText(snapshot.bloom.ready));
        drawExtent("  Size", snapshot.bloom.width, snapshot.bloom.height);
        drawKeyValue("  Mip Count", snapshot.bloom.mip_count);
        drawKeyValue("  Color Format", snapshot.bloom.color_format.c_str());

        ImGui::Spacing();
        ImGui::TextUnformatted("AO Target");
        drawKeyValue("  Ready", boolToText(snapshot.ao.ready));
        drawExtent("  Size", snapshot.ao.width, snapshot.ao.height);
        drawKeyValue("  Color Format", snapshot.ao.color_format.c_str());
        drawKeyValue("  Half Resolution", boolToText(snapshot.ao.half_resolution));

        ImGui::Spacing();
        ImGui::TextUnformatted("SMAA Target");
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
        drawKeyValue("SMAA Debug Ready", boolToText(snapshot.effects.smaa_debug_available));
        drawKeyValue("Shadow Debug Ready", boolToText(snapshot.effects.shadow_debug_available));
        drawKeyValue("Point Shadow Debug Ready", boolToText(snapshot.effects.point_shadow_debug_available));
        drawKeyValue("Rect Shadow Debug Ready", boolToText(snapshot.effects.rect_shadow_debug_available));
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
        drawKeyValue("Materials", snapshot.resources.material_count);
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
        ImGui::Text("Probe Blend Distance: %.2f", snapshot.resources.active_reflection_probe_blend_distance);
    }
} // namespace NexAur
