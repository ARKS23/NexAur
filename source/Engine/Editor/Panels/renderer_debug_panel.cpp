#include "pch.h"
#include "renderer_debug_panel.h"

#include "Editor/Widgets/editor_widgets.h"
#include "Function/Renderer/renderer_debug_service.h"
#include "Function/Renderer/data/render_context.h"

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

        int toneMappingModeToIndex(RenderToneMappingMode mode) {
            switch (mode) {
            case RenderToneMappingMode::ACES:
                return 1;
            case RenderToneMappingMode::None:
            default:
                return 0;
            }
        }

        RenderToneMappingMode toneMappingModeFromIndex(int index) {
            return index == 1 ? RenderToneMappingMode::ACES : RenderToneMappingMode::None;
        }

        int iblDebugModeToIndex(RenderIblDebugMode mode) {
            return static_cast<int>(mode);
        }

        RenderIblDebugMode iblDebugModeFromIndex(int index) {
            switch (index) {
            case 1:
                return RenderIblDebugMode::DiffuseIbl;
            case 2:
                return RenderIblDebugMode::SpecularIbl;
            case 3:
                return RenderIblDebugMode::CombinedIbl;
            case 4:
                return RenderIblDebugMode::Normal;
            case 5:
                return RenderIblDebugMode::Metallic;
            case 6:
                return RenderIblDebugMode::Roughness;
            case 7:
                return RenderIblDebugMode::AmbientOcclusion;
            case 8:
                return RenderIblDebugMode::Emissive;
            case 9:
                return RenderIblDebugMode::Irradiance;
            case 10:
                return RenderIblDebugMode::PrefilteredEnvironment;
            case 11:
                return RenderIblDebugMode::BrdfLut;
            case 0:
            default:
                return RenderIblDebugMode::FinalLit;
            }
        }

        int shadowFilterModeToIndex(RenderShadowFilterMode mode) {
            switch (mode) {
            case RenderShadowFilterMode::PCF3x3:
                return 1;
            case RenderShadowFilterMode::PCF5x5:
                return 2;
            case RenderShadowFilterMode::PoissonPCF:
                return 3;
            case RenderShadowFilterMode::PCSS:
                return 4;
            case RenderShadowFilterMode::Hard:
            default:
                return 0;
            }
        }

        RenderShadowFilterMode shadowFilterModeFromIndex(int index) {
            switch (index) {
            case 1:
                return RenderShadowFilterMode::PCF3x3;
            case 2:
                return RenderShadowFilterMode::PCF5x5;
            case 3:
                return RenderShadowFilterMode::PoissonPCF;
            case 4:
                return RenderShadowFilterMode::PCSS;
            case 0:
            default:
                return RenderShadowFilterMode::Hard;
            }
        }

        int shadowMapResolutionToIndex(uint32_t resolution) {
            if (resolution >= 4096u) {
                return 2;
            }
            if (resolution >= 2048u) {
                return 1;
            }
            return 0;
        }

        uint32_t shadowMapResolutionFromIndex(int index) {
            switch (index) {
            case 1:
                return 2048u;
            case 2:
                return 4096u;
            case 0:
            default:
                return 1024u;
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

        drawDebugVisualizationSection();
        drawEffectsSection();
        drawRendererSection(snapshot);
        drawFrameSection(snapshot);
        drawViewSection(snapshot);
        drawTargetsSection(snapshot);
        drawResourcesSection(snapshot);

        ImGui::End();
    }

    void RendererDebugPanel::drawDebugVisualizationSection() {
        if (!EditorWidgets::sectionHeader("Debug Visualization")) {
            return;
        }

        if (!m_context || !m_context->render_context) {
            ImGui::TextDisabled("Render context is unavailable.");
            return;
        }

        RenderDebugVisualizationOptions options =
            m_context->render_context->getDebugVisualizationOptions();

        bool changed = false;
        changed |= ImGui::Checkbox("Debug Draw", &options.enabled);

        if (!options.enabled) {
            ImGui::BeginDisabled();
        }

        changed |= ImGui::Checkbox("Camera Frustum", &options.camera_frustum);
        changed |= ImGui::Checkbox("Light Gizmos", &options.light_gizmos);

        if (!options.enabled) {
            ImGui::EndDisabled();
        }

        if (changed) {
            m_context->render_context->setDebugVisualizationOptions(options);
        }
    }

    void RendererDebugPanel::drawEffectsSection() {
        if (!EditorWidgets::sectionHeader("Effects")) {
            return;
        }

        if (!m_context || !m_context->render_context) {
            ImGui::TextDisabled("Render context is unavailable.");
            return;
        }

        RenderSettings settings = m_context->render_context->getRenderSettings();
        bool changed = false;

        const char* tone_mapping_items[] = { "None", "ACES" };
        int tone_mapping_index = toneMappingModeToIndex(settings.post_process.tone_mapping_mode);
        if (ImGui::Combo("Tone Mapping", &tone_mapping_index, tone_mapping_items, IM_ARRAYSIZE(tone_mapping_items))) {
            settings.post_process.tone_mapping_mode = toneMappingModeFromIndex(tone_mapping_index);
            changed = true;
        }

        changed |= ImGui::SliderFloat("Exposure", &settings.post_process.exposure, 0.0f, 4.0f, "%.2f");
        changed |= ImGui::Checkbox("Bloom", &settings.post_process.bloom_enabled);
        changed |= ImGui::SliderFloat("Bloom Intensity", &settings.post_process.bloom_intensity, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::SliderFloat("Bloom Scatter", &settings.post_process.bloom_scatter, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Bloom Radius", &settings.post_process.bloom_radius, 0.25f, 2.5f, "%.2f");

        ImGui::Spacing();
        changed |= ImGui::Checkbox("Shadow", &settings.shadow.enabled);

        const char* shadow_filter_items[] = { "Hard", "PCF 3x3", "PCF 5x5", "Poisson PCF", "PCSS" };
        int shadow_filter_index = shadowFilterModeToIndex(settings.shadow.filter_mode);
        if (ImGui::Combo("Shadow Filter", &shadow_filter_index, shadow_filter_items, IM_ARRAYSIZE(shadow_filter_items))) {
            settings.shadow.filter_mode = shadowFilterModeFromIndex(shadow_filter_index);
            changed = true;
        }

        changed |= ImGui::SliderFloat("Shadow Strength", &settings.shadow.strength, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Shadow Bias", &settings.shadow.constant_bias, 0.0f, 0.02f, "%.5f");
        changed |= ImGui::SliderFloat("Shadow Normal Bias", &settings.shadow.normal_bias, 0.0f, 0.2f, "%.4f");
        changed |= ImGui::SliderFloat("Shadow Slope Bias", &settings.shadow.slope_bias, 0.0f, 0.02f, "%.5f");
        changed |= ImGui::SliderFloat("Shadow Filter Radius", &settings.shadow.filter_radius, 0.25f, 3.0f, "%.2f");

        const bool pcss_selected = settings.shadow.filter_mode == RenderShadowFilterMode::PCSS;
        if (!pcss_selected) {
            ImGui::BeginDisabled();
        }
        changed |= ImGui::SliderFloat("PCSS Light Radius", &settings.shadow.pcss_light_radius, 0.01f, 4.0f, "%.2f");
        changed |= ImGui::SliderFloat("PCSS Search Radius", &settings.shadow.pcss_search_radius, 0.5f, 12.0f, "%.2f");
        changed |= ImGui::SliderFloat("PCSS Min Radius", &settings.shadow.pcss_min_filter_radius, 0.25f, 4.0f, "%.2f");
        changed |= ImGui::SliderFloat("PCSS Max Radius", &settings.shadow.pcss_max_filter_radius, 1.0f, 16.0f, "%.2f");
        if (!pcss_selected) {
            ImGui::EndDisabled();
        }

        changed |= ImGui::SliderFloat("Shadow Distance", &settings.shadow.distance, 1.0f, 120.0f, "%.1f");

        const char* shadow_map_items[] = { "1024", "2048", "4096" };
        int shadow_map_index = shadowMapResolutionToIndex(settings.shadow.map_resolution);
        if (ImGui::Combo("Shadow Map", &shadow_map_index, shadow_map_items, IM_ARRAYSIZE(shadow_map_items))) {
            settings.shadow.map_resolution = shadowMapResolutionFromIndex(shadow_map_index);
            changed = true;
        }
        changed |= ImGui::Checkbox("Shadow Stabilize", &settings.shadow.stabilize);
        changed |= ImGui::Checkbox("CSM", &settings.shadow.cascades_enabled);
        if (!settings.shadow.cascades_enabled) {
            ImGui::BeginDisabled();
        }
        int cascade_count = static_cast<int>(settings.shadow.cascade_count);
        if (ImGui::SliderInt("Cascade Count", &cascade_count, 1, 4)) {
            settings.shadow.cascade_count = static_cast<uint32_t>(cascade_count);
            changed = true;
        }
        changed |= ImGui::SliderFloat("Split Lambda", &settings.shadow.cascade_split_lambda, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::Checkbox("Cascade Overlay", &settings.shadow.cascade_debug_overlay);
        if (!settings.shadow.cascades_enabled) {
            ImGui::EndDisabled();
        }

        ImGui::Spacing();
        const char* ibl_debug_items[] = {
            "Final Lit",
            "Diffuse IBL",
            "Specular IBL",
            "Combined IBL",
            "Normal",
            "Metallic",
            "Roughness",
            "Ambient Occlusion",
            "Emissive",
            "Irradiance",
            "Prefiltered Environment",
            "BRDF LUT"
        };
        int ibl_debug_index = iblDebugModeToIndex(settings.ibl_debug.mode);
        if (ImGui::Combo("IBL Debug", &ibl_debug_index, ibl_debug_items, IM_ARRAYSIZE(ibl_debug_items))) {
            settings.ibl_debug.mode = iblDebugModeFromIndex(ibl_debug_index);
            changed = true;
        }
        changed |= ImGui::SliderFloat("Prefilter Mip", &settings.ibl_debug.prefilter_mip, 0.0f, 7.0f, "%.1f");

        if (changed) {
            m_context->render_context->setRenderSettings(settings);
        }
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
        ImGui::TextUnformatted("Bloom Target");
        drawKeyValue("  Ready", boolToText(snapshot.bloom.ready));
        drawExtent("  Size", snapshot.bloom.width, snapshot.bloom.height);
        drawKeyValue("  Mip Count", snapshot.bloom.mip_count);
        drawKeyValue("  Color Format", snapshot.bloom.color_format.c_str());
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
    }
} // namespace NexAur
