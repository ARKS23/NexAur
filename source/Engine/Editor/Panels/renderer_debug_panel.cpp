#include "pch.h"
#include "renderer_debug_panel.h"

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
        drawRendererSection(snapshot);
        drawFrameSection(snapshot);
        drawViewSection(snapshot);
        drawTargetsSection(snapshot);
        drawResourcesSection(snapshot);

        ImGui::End();
    }

    void RendererDebugPanel::drawDebugVisualizationSection() {
        if (!ImGui::CollapsingHeader("Debug Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
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

    void RendererDebugPanel::drawRendererSection(const RendererDebugSnapshot& snapshot) {
        if (!ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        if (!ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        if (!ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        if (!ImGui::CollapsingHeader("Targets", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        drawKeyValue("  Depth Format", snapshot.shadow_target.depth_format.c_str());
    }

    void RendererDebugPanel::drawResourcesSection(const RendererDebugSnapshot& snapshot) {
        if (!ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        drawKeyValue("Models", snapshot.resources.model_count);
        drawKeyValue("Textures", snapshot.resources.texture_count);
        drawKeyValue("Meshes", snapshot.resources.mesh_count);
        drawKeyValue("Materials", snapshot.resources.material_count);
        drawKeyValue(
            "Fallback White Texture",
            boolToText(snapshot.resources.fallback_white_texture_ready));
        drawKeyValue("Fallback Material", boolToText(snapshot.resources.fallback_material_ready));
    }
} // namespace NexAur
