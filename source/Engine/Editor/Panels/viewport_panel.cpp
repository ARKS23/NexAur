#include "pch.h"
#include "viewport_panel.h"

#include "Editor/editor_services.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/renderer_service.h"
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
    } // namespace

    void ViewportPanel::onUpdate(TimeStep delta_time) {
        (void)delta_time;
    }

    void ViewportPanel::onUIRender() {
        beginViewportWindow();

        drawViewportModeToolbar();
        updateViewportWindowState();
        syncViewportResize();
        drawViewportOutput();
        handleGizmoHotkeys();
        drawGizmo();

        endViewportWindow();
    }

    void ViewportPanel::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<MouseButtonPressedEvent>(NX_BIND_EVENT_FN(ViewportPanel::onMouseButtonPressed));

        if (m_context && m_context->viewport_camera && isSceneViewMode() && (m_viewport_hovered || m_viewport_focused)) {
            m_context->viewport_camera->onEvent(event);
        }
    }

    void ViewportPanel::beginViewportWindow() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Scene Viewport");
    }

    void ViewportPanel::drawViewportModeToolbar() {
        if (!m_context) {
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));

        const bool scene_view = m_context->viewport_view_mode == EditorViewportViewMode::SceneView;
        const bool game_view = m_context->viewport_view_mode == EditorViewportViewMode::GameView;

        if (scene_view) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Scene")) {
            m_context->viewport_view_mode = EditorViewportViewMode::SceneView;
        }
        if (scene_view) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (game_view) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Game")) {
            m_context->viewport_view_mode = EditorViewportViewMode::GameView;
        }
        if (game_view) {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleVar();
        ImGui::Separator();
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

    void ViewportPanel::handleGizmoHotkeys() {
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
            m_gizmo_operation = ImGuizmo::TRANSLATE;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) {
            m_gizmo_operation = ImGuizmo::ROTATE;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            m_gizmo_operation = ImGuizmo::SCALE;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_T)) {
            m_gizmo_mode = (m_gizmo_mode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        }
    }

    void ViewportPanel::drawGizmo() {
        if (!m_show_gizmo) {
            return;
        }
        if (!isSceneViewMode()) {
            return;
        }
        if (!m_context || !m_context->active_scene) {
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

        setGizmoStyle();
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

        float snap_values[3] = { m_translate_snap, m_translate_snap, m_translate_snap };
        if (m_gizmo_operation == ImGuizmo::ROTATE) {
            snap_values[0] = m_rotate_snap;
            snap_values[1] = m_rotate_snap;
            snap_values[2] = m_rotate_snap;
        } else if (m_gizmo_operation == ImGuizmo::SCALE) {
            snap_values[0] = m_scale_snap;
            snap_values[1] = m_scale_snap;
            snap_values[2] = m_scale_snap;
        }

        ImGuizmo::MODE mode = (m_gizmo_operation == ImGuizmo::ROTATE) ? ImGuizmo::LOCAL : m_gizmo_mode;
        glm::mat4 delta(1.0f);

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            m_gizmo_operation,
            mode,
            glm::value_ptr(transform),
            glm::value_ptr(delta),
            m_use_snap ? snap_values : nullptr
        );

        if (ImGuizmo::IsUsing()) {
            if (m_gizmo_operation == ImGuizmo::ROTATE) {
                glm::quat q_current = glm::quat(tc.rotation);
                glm::quat q_delta = glm::normalize(glm::quat_cast(glm::mat3(delta)));
                glm::quat q_new = glm::normalize(q_delta * q_current);
                tc.rotation = glm::eulerAngles(q_new);
            } else {
                applyGizmoToSelectedEntity(transform);
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

    void ViewportPanel::setGizmoStyle() {
        ImGuizmo::Style& style = ImGuizmo::GetStyle();
        style.RotationLineThickness = 15.0f;
        style.ScaleLineThickness = 3.0f;
    }

    void ViewportPanel::applyGizmoToSelectedEntity(const glm::mat4& transform) {
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

        if (m_gizmo_operation == ImGuizmo::TRANSLATE) {
            tc.translation = glm::vec3(translation[0], translation[1], translation[2]);
            return;
        }

        if (m_gizmo_operation == ImGuizmo::ROTATE) {
            tc.rotation = glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2]));
            return;
        }

        if (m_gizmo_operation == ImGuizmo::SCALE) {
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
        if (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) {
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
