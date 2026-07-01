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

        ImGui::Spacing();
        ImGui::TextUnformatted("AO Target");
        drawKeyValue("  Ready", boolToText(snapshot.ao.ready));
        drawExtent("  Size", snapshot.ao.width, snapshot.ao.height);
        drawKeyValue("  Color Format", snapshot.ao.color_format.c_str());
        drawKeyValue("  Half Resolution", boolToText(snapshot.ao.half_resolution));
    }

    void RendererDebugPanel::drawEffectsSection(const RendererDebugSnapshot& snapshot) {
        if (!EditorWidgets::sectionHeader("Effects")) {
            return;
        }

        drawKeyValue("Debug View", snapshot.effects.debug_view.c_str());
        drawKeyValue("Bloom Mip", snapshot.effects.bloom_mip);
        drawKeyValue("Shadow Cascade", snapshot.effects.shadow_cascade);
        drawKeyValue("Bloom Debug Ready", boolToText(snapshot.effects.bloom_debug_available));
        drawKeyValue("AO Debug Ready", boolToText(snapshot.effects.ao_debug_available));
        drawKeyValue("Shadow Debug Ready", boolToText(snapshot.effects.shadow_debug_available));
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
