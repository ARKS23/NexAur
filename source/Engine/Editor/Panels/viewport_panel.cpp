#include "pch.h"
#include "viewport_panel.h"

#include "Editor/editor_services.h"
#include "Editor/Style/editor_style.h"
#include "Editor/Style/editor_theme.h"
#include "Editor/Widgets/editor_widgets.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/renderer_service.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Renderer/renderer_debug_service.h"
#include "Editor/Camera/editor_camera.h"
#include "Function/Scene/component.h"
#include "Function/Scene/scene_v2.h"

#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace NexAur {
    namespace {
        bool isEmbeddedViewportOutput(const ViewportOutput& output) {
            if (!output.valid()) {
                return false;
            }

            if (output.kind == ViewportOutputKind::VulkanImGuiTexture) {
                return output.native_handle != nullptr;
            }

            return false;
        }

        ImGuizmo::OPERATION toGizmoOperation(EditorToolOperation operation) {
            switch (operation) {
            case EditorToolOperation::Rotate:
                return ImGuizmo::ROTATE;
            case EditorToolOperation::Scale:
                return ImGuizmo::SCALE;
            case EditorToolOperation::Translate:
            case EditorToolOperation::Select:
            default:
                return ImGuizmo::TRANSLATE;
            }
        }

        ImGuizmo::MODE toGizmoMode(EditorToolSpace space) {
            return space == EditorToolSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        }

        const char* viewportModeToText(EditorViewportViewMode mode) {
            switch (mode) {
            case EditorViewportViewMode::GameView:
                return "Game";
            case EditorViewportViewMode::SceneView:
            default:
                return "Scene";
            }
        }

        const char* viewportOutputKindToText(ViewportOutputKind kind) {
            switch (kind) {
            case ViewportOutputKind::VulkanImGuiTexture:
                return "Vulkan";
            case ViewportOutputKind::ExternalSwapchain:
                return "Swapchain";
            case ViewportOutputKind::None:
            default:
                return "No Output";
            }
        }

        const char* toolOperationToText(EditorToolOperation operation) {
            switch (operation) {
            case EditorToolOperation::Select:
                return "Select";
            case EditorToolOperation::Rotate:
                return "Rotate";
            case EditorToolOperation::Scale:
                return "Scale";
            case EditorToolOperation::Translate:
            default:
                return "Move";
            }
        }

        const char* toolSpaceToText(EditorToolSpace space) {
            return space == EditorToolSpace::Local ? "Local" : "World";
        }

        std::string selectedEntityName(const Entity& entity) {
            if (!entity) {
                return "None";
            }

            if (entity.hasComponent<TagComponent>()) {
                const std::string& name = entity.getComponent<TagComponent>().name;
                if (!name.empty()) {
                    return name;
                }
            }

            return "Entity " + std::to_string(static_cast<uint32_t>(entity));
        }

        ImVec2 add(ImVec2 a, ImVec2 b) {
            return ImVec2(a.x + b.x, a.y + b.y);
        }

        ImVec2 viewportMin(const glm::vec2 bounds[2]) {
            return ImVec2(bounds[0].x, bounds[0].y);
        }

        ImVec2 viewportMax(const glm::vec2 bounds[2]) {
            return ImVec2(bounds[1].x, bounds[1].y);
        }

        bool beginOverlayWindow(const char* id, ImVec2 position, ImVec2 pivot, bool accept_input = true) {
            const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoFocusOnAppearing;

            if (!accept_input) {
                flags |= ImGuiWindowFlags_NoInputs;
            }

            ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
            if (const ImGuiViewport* viewport = ImGui::GetMainViewport()) {
                ImGui::SetNextWindowViewport(viewport->ID);
            }

            ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorThemeTokens::withAlpha(theme.colors.background_main, 0.84f));
            ImGui::PushStyleColor(ImGuiCol_Border, EditorThemeTokens::withAlpha(theme.colors.border_subtle, 0.82f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 5.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

            return ImGui::Begin(id, nullptr, flags);
        }

        void endOverlayWindow() {
            ImGui::End();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
        }

        void drawOverlaySeparator() {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
        }
    } // namespace

    void ViewportPanel::onUpdate(TimeStep delta_time) {
        (void)delta_time;
    }

    void ViewportPanel::onUIRender() {
        if (!beginViewportWindow()) {
            endViewportWindow();
            return;
        }

        updateViewportWindowState();
        syncViewportResize();
        drawViewportOutput();
        handleGizmoHotkeys();
        drawGizmo();
        drawViewportChrome();
        drawViewportOverlay();

        endViewportWindow();
    }

    void ViewportPanel::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<MouseButtonPressedEvent>(NX_BIND_EVENT_FN(ViewportPanel::onMouseButtonPressed));

        if (m_context && m_context->viewport_camera && isSceneViewMode() && (m_viewport_hovered || m_viewport_focused)) {
            m_context->viewport_camera->onEvent(event);
        }
    }

    bool ViewportPanel::beginViewportWindow() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        bool& open_flag = getOpenFlag();
        return ImGui::Begin(getName().c_str(), &open_flag);
    }

    void ViewportPanel::updateViewportWindowState() {
        m_viewport_focused = ImGui::IsWindowFocused();
        m_viewport_hovered = ImGui::IsWindowHovered();
        if (m_context) {
            m_context->viewport_focused = m_viewport_focused;
            m_context->viewport_hovered = m_viewport_hovered;
        }

        const ImVec2 viewport_min = ImGui::GetCursorScreenPos();
        const ImVec2 available_size = ImGui::GetContentRegionAvail();

        m_viewport_bounds[0] = { viewport_min.x, viewport_min.y };
        m_viewport_bounds[1] = { viewport_min.x + available_size.x, viewport_min.y + available_size.y };
        m_viewport_size = { available_size.x, available_size.y };
        m_viewport_overlay_hovered = false;
    }

    void ViewportPanel::syncViewportResize() {
        if (!m_context || !m_context->renderer_service || !m_context->active_scene) {
            return;
        }
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) {
            return;
        }

        std::shared_ptr<RendererService> renderer_service = m_context->renderer_service;

        const uint32_t target_w = static_cast<uint32_t>(m_viewport_size.x);
        const uint32_t target_h = static_cast<uint32_t>(m_viewport_size.y);

        if (target_w == 0 || target_h == 0) {
            return;
        }

        const ViewportOutput output = renderer_service->getViewportOutput();
        if (output.kind == ViewportOutputKind::ExternalSwapchain) {
            if (output.valid()) {
                syncEditorCameraSize(output.width, output.height);
            }
            return;
        }

        if (output.kind == ViewportOutputKind::None) {
            return;
        }

        auto [current_w, current_h] = renderer_service->getViewportSize();
        if (target_w != current_w || target_h != current_h) {
            renderer_service->setViewportSize(target_w, target_h);
        }

        syncEditorCameraSize(target_w, target_h);
    }

    void ViewportPanel::drawViewportOutput() {
        if (!m_context || !m_context->renderer_service) {
            return;
        }
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) {
            return;
        }

        const ViewportOutput output = m_context->renderer_service->getViewportOutput();
        switch (output.kind) {
        case ViewportOutputKind::VulkanImGuiTexture:
            drawVulkanImGuiViewport(output);
            break;
        case ViewportOutputKind::ExternalSwapchain:
            drawExternalSwapchainNotice(output);
            break;
        case ViewportOutputKind::None:
        default:
            drawNoViewportOutputNotice();
            break;
        }
    }

    void ViewportPanel::drawVulkanImGuiViewport(const ViewportOutput& output) {
        if (!output.valid() || !output.native_handle) {
            drawViewportPlaceholder("Vulkan viewport image pending");
            return;
        }

        ImGui::Image(
            output.native_handle,
            ImVec2(m_viewport_size.x, m_viewport_size.y),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f)
        );
    }

    void ViewportPanel::drawExternalSwapchainNotice(const ViewportOutput& output) {
        std::vector<std::string> lines = {
            "Vulkan renders to the main swapchain",
            "Embedded viewport, picking, and gizmo are pending"
        };

        if (output.valid()) {
            lines.push_back(
                "Swapchain: " +
                std::to_string(output.width) +
                " x " +
                std::to_string(output.height));
        }

        drawViewportPlaceholder(lines);
    }

    void ViewportPanel::drawNoViewportOutputNotice() {
        drawViewportPlaceholder("No viewport output");
    }

    void ViewportPanel::drawViewportPlaceholder(const char* message) {
        drawViewportPlaceholder(std::vector<std::string>{ message });
    }

    void ViewportPanel::drawViewportPlaceholder(const std::vector<std::string>& lines) {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size(m_viewport_size.x, m_viewport_size.y);
        const ImVec2 max(origin.x + size.x, origin.y + size.y);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(origin, max, IM_COL32(20, 22, 26, 255));

        const float line_height = ImGui::GetTextLineHeightWithSpacing();
        const float total_text_height = line_height * static_cast<float>(lines.size());
        float y = origin.y + std::max(0.0f, (size.y - total_text_height) * 0.5f);

        for (const std::string& line : lines) {
            const ImVec2 text_size = ImGui::CalcTextSize(line.c_str());
            const ImVec2 text_pos(
                origin.x + std::max(0.0f, (size.x - text_size.x) * 0.5f),
                y
            );
            draw_list->AddText(text_pos, IM_COL32(185, 190, 200, 255), line.c_str());
            y += line_height;
        }

        ImGui::InvisibleButton("##ViewportOutputPlaceholder", size);
    }

    void ViewportPanel::drawViewportChrome() {
        drawViewportFrame();
    }

    void ViewportPanel::drawViewportOverlay() {
        if (!m_context || !m_context->renderer_service) {
            return;
        }
        if (m_viewport_size.x <= 96.0f || m_viewport_size.y <= 72.0f) {
            return;
        }

        const ViewportOutput output = m_context->renderer_service->getViewportOutput();

        drawViewportToolbarOverlay(output);
        drawViewportInfoOverlay(output);
        drawViewportStatusOverlay();
    }

    void ViewportPanel::drawViewportToolbarOverlay(const ViewportOutput& output) {
        if (!m_context) {
            return;
        }

        const ImVec2 position = add(viewportMin(m_viewport_bounds), ImVec2(10.0f, 10.0f));
        if (!beginOverlayWindow("##ViewportToolbarOverlay", position, ImVec2(0.0f, 0.0f))) {
            endOverlayWindow();
            return;
        }

        const bool scene_view = m_context->viewport_view_mode == EditorViewportViewMode::SceneView;
        const bool game_view = m_context->viewport_view_mode == EditorViewportViewMode::GameView;

        if (EditorStyle::segmentedButton("Scene", scene_view)) {
            m_context->viewport_view_mode = EditorViewportViewMode::SceneView;
        }
        ImGui::SameLine();
        if (EditorStyle::segmentedButton("Game", game_view)) {
            m_context->viewport_view_mode = EditorViewportViewMode::GameView;
        }

        drawOverlaySeparator();

        EditorWidgets::toolbarButton("Shaded", "Viewport shading mode.", true);
        ImGui::SameLine();
        ImGui::BeginDisabled();
        EditorWidgets::toolbarButton("Wire", "Wireframe view is reserved for a later renderer debug pass.");
        ImGui::EndDisabled();

        drawOverlaySeparator();

        bool debug_draw_enabled = false;
        if (m_context->render_context) {
            RenderDebugVisualizationOptions options =
                m_context->render_context->getDebugVisualizationOptions();
            debug_draw_enabled = options.enabled;

            if (EditorWidgets::toolbarButton("Debug", "Toggle renderer debug draw.", debug_draw_enabled)) {
                options.enabled = !options.enabled;
                m_context->render_context->setDebugVisualizationOptions(options);
            }
        } else {
            ImGui::BeginDisabled();
            EditorWidgets::toolbarButton("Debug", "Render context is unavailable.");
            ImGui::EndDisabled();
        }

        drawOverlaySeparator();
        ImGui::TextDisabled("%s", viewportOutputKindToText(output.kind));

        m_viewport_overlay_hovered |= ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        endOverlayWindow();
    }

    void ViewportPanel::drawViewportInfoOverlay(const ViewportOutput& output) {
        if (!m_context) {
            return;
        }

        const ImVec2 position = add(viewportMax(m_viewport_bounds), ImVec2(-10.0f, 10.0f - m_viewport_size.y));
        if (!beginOverlayWindow("##ViewportInfoOverlay", position, ImVec2(1.0f, 0.0f), false)) {
            endOverlayWindow();
            return;
        }

        const EditorToolState& tool_state = m_context->tool_state;
        ImGui::Text(
            "%s | %u x %u",
            viewportModeToText(m_context->viewport_view_mode),
            output.width,
            output.height);

        ImGui::TextDisabled(
            "%s | %s | Snap %s",
            toolOperationToText(tool_state.operation),
            toolSpaceToText(tool_state.space),
            tool_state.snap_enabled ? "On" : "Off");

        if (m_context->viewport_camera) {
            ImGui::TextDisabled("Speed %.1f", m_context->viewport_camera->getMoveSpeed());
        }

        endOverlayWindow();
    }

    void ViewportPanel::drawViewportStatusOverlay() {
        if (!m_context) {
            return;
        }

        const ImVec2 bottom_left = add(viewportMax(m_viewport_bounds), ImVec2(10.0f - m_viewport_size.x, -10.0f));
        if (beginOverlayWindow("##ViewportStatusLeftOverlay", bottom_left, ImVec2(0.0f, 1.0f), false)) {
            const std::string entity_name = selectedEntityName(m_context->selected_entity);
            ImGui::Text("Selected: %s", entity_name.c_str());
        }
        endOverlayWindow();

        const ImVec2 bottom_right = add(viewportMax(m_viewport_bounds), ImVec2(-10.0f, -10.0f));
        if (beginOverlayWindow("##ViewportStatusRightOverlay", bottom_right, ImVec2(1.0f, 1.0f), false)) {
            const ImGuiIO& io = ImGui::GetIO();
            double frame_ms = 0.0;
            size_t draw_items = 0;

            if (m_context->renderer_debug_service) {
                const RendererDebugSnapshot snapshot =
                    m_context->renderer_debug_service->getDebugSnapshot();
                frame_ms = snapshot.frame.engine_delta_ms;
                draw_items = snapshot.frame.opaque_draw_item_count + snapshot.frame.transparent_draw_item_count;
            }

            if (frame_ms > 0.0) {
                ImGui::Text("FPS %.0f | %.2f ms", io.Framerate, frame_ms);
            } else {
                ImGui::Text("FPS %.0f", io.Framerate);
            }

            if (draw_items > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("| Draws %zu", draw_items);
            }
        }
        endOverlayWindow();
    }

    void ViewportPanel::drawViewportFrame() const {
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) {
            return;
        }

        const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 min = viewportMin(m_viewport_bounds);
        const ImVec2 max = viewportMax(m_viewport_bounds);

        draw_list->AddRect(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(EditorThemeTokens::withAlpha(theme.colors.border_subtle, 0.92f)),
            0.0f,
            0,
            1.0f);

        if (m_viewport_hovered) {
            draw_list->AddRect(
                ImVec2(min.x + 1.0f, min.y + 1.0f),
                ImVec2(max.x - 1.0f, max.y - 1.0f),
                ImGui::ColorConvertFloat4ToU32(EditorThemeTokens::withAlpha(theme.colors.accent_blue, 0.28f)),
                0.0f,
                0,
                1.0f);
        }
    }

    void ViewportPanel::handleGizmoHotkeys() {
        if (m_context && m_context->command_registry) {
            return;
        }
        if (!isSceneViewMode()) {
            return;
        }
        if (!m_viewport_focused) {
            return;
        }
        if (ImGui::GetIO().WantTextInput) {
            return;
        }
        if (!canUseEmbeddedViewportOutput()) {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_W)) {
            m_context->tool_state.operation = EditorToolOperation::Translate;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) {
            m_context->tool_state.operation = EditorToolOperation::Rotate;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            m_context->tool_state.operation = EditorToolOperation::Scale;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_T)) {
            m_context->tool_state.space =
                m_context->tool_state.space == EditorToolSpace::World
                    ? EditorToolSpace::Local
                    : EditorToolSpace::World;
        }
    }

    void ViewportPanel::drawGizmo() {
        if (!isSceneViewMode()) {
            return;
        }
        if (!m_context || !m_context->active_scene) {
            return;
        }
        const EditorToolState& tool_state = m_context->tool_state;
        if (!tool_state.gizmo_visible || tool_state.operation == EditorToolOperation::Select) {
            return;
        }
        if (!canUseEmbeddedViewportOutput()) {
            return;
        }
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) {
            return;
        }

        Entity selected = m_context->selected_entity;
        if (!selected || !selected.hasComponent<TransformComponent>()) {
            return;
        }

        std::shared_ptr<EditorCamera> editor_camera = m_context->viewport_camera;
        if (!editor_camera) {
            return;
        }

        EditorStyle::applyGizmoStyle();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(
            m_viewport_bounds[0].x,
            m_viewport_bounds[0].y,
            m_viewport_size.x,
            m_viewport_size.y
        );

        glm::mat4 view = editor_camera->getView();
        const glm::mat4& projection = editor_camera->getProjection();

        auto& tc = selected.getComponent<TransformComponent>();
        glm::mat4 transform = tc.getTransform();

        const ImGuizmo::OPERATION gizmo_operation = toGizmoOperation(tool_state.operation);
        const ImGuizmo::MODE gizmo_mode = toGizmoMode(tool_state.space);

        float snap_values[3] = { tool_state.translate_snap, tool_state.translate_snap, tool_state.translate_snap };
        if (tool_state.operation == EditorToolOperation::Rotate) {
            snap_values[0] = tool_state.rotate_snap;
            snap_values[1] = tool_state.rotate_snap;
            snap_values[2] = tool_state.rotate_snap;
        } else if (tool_state.operation == EditorToolOperation::Scale) {
            snap_values[0] = tool_state.scale_snap;
            snap_values[1] = tool_state.scale_snap;
            snap_values[2] = tool_state.scale_snap;
        }

        glm::mat4 delta(1.0f);

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            gizmo_operation,
            gizmo_mode,
            glm::value_ptr(transform),
            glm::value_ptr(delta),
            tool_state.snap_enabled ? snap_values : nullptr
        );

        if (ImGuizmo::IsUsing()) {
            if (tool_state.operation == EditorToolOperation::Rotate) {
                glm::quat q_current = glm::quat(tc.rotation);
                glm::quat q_delta = glm::normalize(glm::quat_cast(glm::mat3(delta)));
                glm::quat q_new = glm::normalize(q_delta * q_current);
                tc.rotation = glm::eulerAngles(q_new);
            } else {
                applyGizmoToSelectedEntity(transform, tool_state.operation);
            }
        }

        const bool using_now = ImGuizmo::IsUsing();
        m_was_using_gizmo_last_frame = using_now;
    }

    bool ViewportPanel::canUseEmbeddedViewportOutput() const {
        if (!m_context || !m_context->renderer_service) {
            return false;
        }

        const ViewportOutput output = m_context->renderer_service->getViewportOutput();
        return isEmbeddedViewportOutput(output);
    }

    bool ViewportPanel::isSceneViewMode() const {
        return !m_context || m_context->viewport_view_mode == EditorViewportViewMode::SceneView;
    }

    bool ViewportPanel::canUseSceneViewTools() const {
        return isSceneViewMode() && canUseEmbeddedViewportOutput();
    }

    void ViewportPanel::syncEditorCameraSize(uint32_t width, uint32_t height) {
        if (!m_context || !m_context->viewport_camera || width == 0 || height == 0) {
            return;
        }

        const glm::vec2 target_size(static_cast<float>(width), static_cast<float>(height));
        if (target_size == m_camera_viewport_size) {
            return;
        }

        m_context->viewport_camera->setViewportSize(target_size.x, target_size.y);
        m_camera_viewport_size = target_size;
    }

    void ViewportPanel::applyGizmoToSelectedEntity(const glm::mat4& transform, EditorToolOperation operation) {
        if (!m_context || !m_context->selected_entity) {
            return;
        }

        Entity selected = m_context->selected_entity;
        if (!selected.hasComponent<TransformComponent>()) {
            return;
        }

        auto& tc = selected.getComponent<TransformComponent>();

        float translation[3];
        float rotation[3];
        float scale[3];
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), translation, rotation, scale);

        if (operation == EditorToolOperation::Translate) {
            tc.translation = glm::vec3(translation[0], translation[1], translation[2]);
            return;
        }

        if (operation == EditorToolOperation::Rotate) {
            tc.rotation = glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2]));
            return;
        }

        if (operation == EditorToolOperation::Scale) {
            tc.scale = glm::vec3(scale[0], scale[1], scale[2]);
        }
    }

    void ViewportPanel::endViewportWindow() {
        ImGui::End();
        ImGui::PopStyleVar();
    }

    bool ViewportPanel::onMouseButtonPressed(MouseButtonPressedEvent& e) {
        if (e.GetMouseButton() != static_cast<int>(MouseCode::ButtonLeft)) {
            return false;
        }
        if (!m_viewport_hovered) {
            return false;
        }
        if (!canUseSceneViewTools()) {
            return false;
        }
        if (m_viewport_overlay_hovered || m_was_using_gizmo_last_frame || ImGuizmo::IsUsing() || ImGuizmo::IsOver()) {
            return false;
        }

        pickEntityAtMouse();
        return false;
    }

    void ViewportPanel::pickEntityAtMouse() {
        if (!m_context || !m_context->renderer_service || !m_context->input_service || !m_context->active_scene) {
            return;
        }
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) {
            return;
        }

        const ViewportOutput output = m_context->renderer_service->getViewportOutput();
        if (!canUseSceneViewTools() || !isEmbeddedViewportOutput(output)) {
            return;
        }

        auto [mx, my] = m_context->input_service->getState().getMousePosition();
        if (mx < m_viewport_bounds[0].x || mx >= m_viewport_bounds[1].x ||
            my < m_viewport_bounds[0].y || my >= m_viewport_bounds[1].y) {
            return;
        }

        const uint32_t framebuffer_width = output.width;
        const uint32_t framebuffer_height = output.height;
        if (framebuffer_width == 0 || framebuffer_height == 0) {
            return;
        }

        const float local_x = mx - m_viewport_bounds[0].x;
        const float local_y = my - m_viewport_bounds[0].y;
        const float framebuffer_x = local_x * static_cast<float>(framebuffer_width) / m_viewport_size.x;
        const float framebuffer_y =
            output.coordinate_origin == ViewportCoordinateOrigin::BottomLeft
                ? (m_viewport_size.y - local_y) * static_cast<float>(framebuffer_height) / m_viewport_size.y
                : local_y * static_cast<float>(framebuffer_height) / m_viewport_size.y;

        const int pixel_x = std::clamp(static_cast<int>(framebuffer_x), 0, static_cast<int>(framebuffer_width) - 1);
        const int pixel_y = std::clamp(static_cast<int>(framebuffer_y), 0, static_cast<int>(framebuffer_height) - 1);

        ViewportPickRequest request;
        request.x = pixel_x;
        request.y = pixel_y;

        const ViewportPickResult result = m_context->renderer_service->pickViewport(request);
        applyPickResult(request, result);
    }

    void ViewportPanel::applyPickResult(const ViewportPickRequest& request, const ViewportPickResult& result) {
        if (!result.supported || !result.ready) {
            return;
        }
        if (!m_context || !m_context->active_scene) {
            return;
        }

        std::shared_ptr<SelectionService> selection_service = m_context->selection_service.lock();
        if (result.entity_id < 0) {
            if (selection_service) {
                selection_service->clearSelection();
            } else {
                m_context->selected_entity = Entity();
            }
            return;
        }

        NX_CORE_WARN("Mouse Pick at ({}, {}), picked ID: {}", request.x, request.y, result.entity_id);
        auto& registry = m_context->active_scene->getRegistry();
        entt::entity handle = static_cast<entt::entity>(result.entity_id);
        if (!registry.valid(handle)) {
            if (selection_service) {
                selection_service->clearSelection();
            } else {
                m_context->selected_entity = Entity();
            }
            NX_CORE_FATAL("Picked invalid entity handle: {}", result.entity_id);
            return;
        }

        Entity picked_entity(handle, m_context->active_scene.get());
        if (selection_service) {
            selection_service->setSelectedEntity(picked_entity, getName());
        } else {
            m_context->selected_entity = picked_entity;
            m_context->selection_source = getName();
        }
    }
} // namespace NexAur
