#include "pch.h"
#include "viewport_panel.h"
#include "Function/Scene/component.h"
#include "Function/Scene/scene_v2.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/editor_camera.h"
#include "Function/Global/global_context.h"
#include "Function/Input/input_system.h"

#include <algorithm>

namespace NexAur {
    void ViewportPanel::onUpdate(TimeStep delta_time) {
        
    }

    void ViewportPanel::onUIRender() {
        beginViewportWindow();

        updateViewportWindowState();
        syncViewportResize();
        drawSceneTexture();
        handleGizmoHotkeys();
        drawGizmo();
                     
        endViewportWindow();
    }

    void ViewportPanel::onEvent(Event& event) {
        EventDispatcher dispatcher(event);

        // 派发鼠标点击事件，响应物体选中
        dispatcher.dispatch<MouseButtonPressedEvent>(NX_BIND_EVENT_FN(ViewportPanel::onMouseButtonPressed));

        if (m_context && m_context->viewport_camera && (m_viewport_hovered || m_viewport_focused)) {
            m_context->viewport_camera->onEvent(event);
        }
    }

    void ViewportPanel::beginViewportWindow() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 }); // 让画面完全贴合面板边缘，无黑边
        ImGui::Begin("Scene Viewport");
    }

    void ViewportPanel::updateViewportWindowState() {
        m_viewport_focused = ImGui::IsWindowFocused();
        m_viewport_hovered = ImGui::IsWindowHovered();
        if (m_context) {
            m_context->viewport_focused = m_viewport_focused;
            m_context->viewport_hovered = m_viewport_hovered;
        }

        const ImVec2 window_pos = ImGui::GetWindowPos();
        const ImVec2 content_min = ImGui::GetWindowContentRegionMin();
        const ImVec2 content_max = ImGui::GetWindowContentRegionMax();

        m_viewport_bounds[0] = { window_pos.x + content_min.x, window_pos.y + content_min.y };
        m_viewport_bounds[1] = { window_pos.x + content_max.x, window_pos.y + content_max.y };
        m_viewport_size = {
            m_viewport_bounds[1].x - m_viewport_bounds[0].x,
            m_viewport_bounds[1].y - m_viewport_bounds[0].y
        };
    }

    void ViewportPanel::syncViewportResize() {
        if (!m_context || !m_context->renderer_system || !m_context->active_scene) return;
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) return;

        auto renderer_system = m_context->renderer_system;
        auto editor_camera = m_context->viewport_camera;
        if (!editor_camera) return;

        auto [current_w, current_h] = renderer_system->getViewportSize();
        const uint32_t target_w = static_cast<uint32_t>(m_viewport_size.x);
        const uint32_t target_h = static_cast<uint32_t>(m_viewport_size.y);

        if (target_w == 0 || target_h == 0) return;
        if (target_w == current_w && target_h == current_h) return;

        renderer_system->setViewportSize(target_w, target_h);
        editor_camera->setViewportSize(static_cast<float>(target_w), static_cast<float>(target_h));
    }

    void ViewportPanel::drawSceneTexture() {
        if (!m_context || !m_context->renderer_system) return;
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) return;

        const uint32_t texture_id = m_context->renderer_system->getViewportColorAttachment();
        ImGui::Image(
            reinterpret_cast<void*>((intptr_t)texture_id),
            ImVec2(m_viewport_size.x, m_viewport_size.y),
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f)
        );
    }

    void ViewportPanel::handleGizmoHotkeys() {
        if (!m_viewport_focused) return;
        if (ImGui::GetIO().WantTextInput) return;

        // if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_show_gizmo = !m_show_gizmo;
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmo_operation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmo_operation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmo_operation = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_T)) {
            m_gizmo_mode = (m_gizmo_mode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        }
    }

    void ViewportPanel::drawGizmo() {
        if (!m_show_gizmo) return;
        if (!m_context || !m_context->active_scene) return;
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) return;

        Entity selected = m_context->selected_entity;
        if (!selected || !selected.hasComponent<TransformComponent>()) return;

        std::shared_ptr<EditorCamera> editor_camera = m_context->viewport_camera;
        if (!editor_camera) return;

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

        // 数值步长设置
        float snap_values[3] = { m_translate_snap, m_translate_snap, m_translate_snap };
        if (m_gizmo_operation == ImGuizmo::ROTATE) {
            snap_values[0] = m_rotate_snap;
            snap_values[1] = m_rotate_snap;
            snap_values[2] = m_rotate_snap;
        } 
        else if (m_gizmo_operation == ImGuizmo::SCALE) {
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
                // 用增量四元数叠加，避免欧拉分解跳变
                glm::quat q_current = glm::quat(tc.rotation);
                glm::quat q_delta = glm::normalize(glm::quat_cast(glm::mat3(delta)));
                glm::quat q_new = glm::normalize(q_delta * q_current);
                tc.rotation = glm::eulerAngles(q_new);
            } else {
                applyGizmoToSelectedEntity(transform);
            }
        }

        const bool using_now = ImGuizmo::IsUsing();

        if (m_was_using_gizmo_last_frame && !using_now) {
            // TODO
        }
        if (!m_was_using_gizmo_last_frame && using_now) {
            // TODO:
        }
        
        m_was_using_gizmo_last_frame = using_now;
    }

    void ViewportPanel::setGizmoStyle() {
        ImGuizmo::Style& style = ImGuizmo::GetStyle();
        style.RotationLineThickness = 15.0f;
        style.ScaleLineThickness = 3.0f;
    }

    void ViewportPanel::applyGizmoToSelectedEntity(const glm::mat4& transform) {
        if (!m_context || !m_context->selected_entity) return;

        Entity selected = m_context->selected_entity;
        if (!selected.hasComponent<TransformComponent>()) return;

        auto& tc = selected.getComponent<TransformComponent>();

        float translation[3], rotation[3], scale[3];
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
            return;
        }
    }

    void ViewportPanel::endViewportWindow() {
        ImGui::End();
        ImGui::PopStyleVar();
    }

    bool ViewportPanel::onMouseButtonPressed(MouseButtonPressedEvent& e) {
        if (e.GetMouseButton() != static_cast<int>(MouseCode::ButtonLeft)) return false;
        if (!m_viewport_hovered) return false;
        if (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) return false; // 鼠标在操作Gizmo时不响应选中
        pickEntityAtMouse();
        return false;
    }

    void ViewportPanel::pickEntityAtMouse() {
        if (!m_context || !m_context->renderer_system || !m_context->active_scene) return;
        if (m_viewport_size.x <= 0.0f || m_viewport_size.y <= 0.0f) return;

        auto [mx, my] = g_runtime_global_context.m_input_system->getMousePosition();

        if (mx < m_viewport_bounds[0].x || mx >= m_viewport_bounds[1].x ||
            my < m_viewport_bounds[0].y || my >= m_viewport_bounds[1].y) {
            return;
        }

        auto fb = m_context->renderer_system->getViewportFramebuffer();
        if (!fb) return;

        auto [framebuffer_width, framebuffer_height] = m_context->renderer_system->getViewportSize();
        if (framebuffer_width == 0 || framebuffer_height == 0) return;

        const float local_x = mx - m_viewport_bounds[0].x;
        const float local_y = my - m_viewport_bounds[0].y;
        const float framebuffer_x = local_x * static_cast<float>(framebuffer_width) / m_viewport_size.x;
        const float framebuffer_y = (m_viewport_size.y - local_y) * static_cast<float>(framebuffer_height) / m_viewport_size.y;

        // ImGui 坐标是浮点数，边界点击可能落到宽高值，读 FBO 前夹到合法像素范围。
        const int pixel_x = std::clamp(static_cast<int>(framebuffer_x), 0, static_cast<int>(framebuffer_width) - 1);
        const int pixel_y = std::clamp(static_cast<int>(framebuffer_y), 0, static_cast<int>(framebuffer_height) - 1); // Y翻转

        fb->bind();
        int picked_id = fb->readPixel(1, pixel_x, pixel_y); // 对应 RED_INTEGER
        fb->unbind();

        if (picked_id < 0) {
            m_context->selected_entity = Entity();
            return;
        }

        NX_CORE_WARN("Mouse Pick at ({}, {}), picked ID: {}", pixel_x, pixel_y, picked_id);
        auto& registry = m_context->active_scene->getRegistry();
        entt::entity handle = static_cast<entt::entity>(picked_id);
        if (!registry.valid(handle)) {
            m_context->selected_entity = Entity();
            NX_CORE_FATAL("Picked invalid entity handle: {}", picked_id);
            return;
        }

        m_context->selected_entity = Entity(handle, m_context->active_scene.get());
        m_context->selection_source = this->getName();
    }
} // namespace NexAur
