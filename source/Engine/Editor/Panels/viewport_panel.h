#pragma once
#include "Core/Base.h"
#include "Core/Events/mouse_event.h"
#include "editor_panel.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

namespace NexAur {
    class ViewportPanel : public EditorPanel {
    public:
        ViewportPanel();
        ~ViewportPanel() override = default;

        virtual void onUpdate(TimeStep delta_time) override;
        virtual void onUIRender() override;
        virtual void onEvent(Event& event) override;

    private:
        void beginViewportWindow();
        void updateViewportWindowState();
        void syncViewportResize();
        void drawSceneTexture();
        void handleGizmoHotkeys();
        void drawGizmo();
        void endViewportWindow();

        void applyGizmoToSelectedEntity(const glm::mat4& transform);

        bool onMouseButtonPressed(MouseButtonPressedEvent& e);
        void pickEntityAtMouse();

    private:
        glm::vec2 m_viewport_size{ 1280, 720 };
        glm::vec2 m_viewport_bounds[2];
        bool m_viewport_focused = false;
        bool m_viewport_hovered = false;
        
        bool m_show_gizmo = true;
        ImGuizmo::OPERATION m_gizmo_operation = ImGuizmo::TRANSLATE;
        ImGuizmo::MODE m_gizmo_mode = ImGuizmo::WORLD;

        bool m_use_snap = false;
        float m_translate_snap = 0.5f; // 平移时的吸附间隔
        float m_rotate_snap = 15.0f;   // 旋转时的吸附间隔（单位：度）
        float m_scale_snap = 0.25f;    // 缩放时的吸附间隔

        bool m_was_using_gizmo_last_frame = false;
    };
} // namespace NexAur
