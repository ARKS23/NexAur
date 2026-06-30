#pragma once
#include "Core/Base.h"
#include "Core/Events/mouse_event.h"
#include "Editor/editor_tool_state.h"
#include "editor_panel.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

#include <cstdint>
#include <string>
#include <vector>

namespace NexAur {
    struct ViewportOutput;
    struct ViewportPickRequest;
    struct ViewportPickResult;

    class ViewportPanel : public EditorPanel {
    public:
        ViewportPanel(const std::string& name = "Viewport") : EditorPanel(name) {}
        ~ViewportPanel() override = default;

        virtual void onUpdate(TimeStep delta_time) override;
        virtual void onUIRender() override;
        virtual void onEvent(Event& event) override;

    private:
        bool beginViewportWindow();
        void drawViewportModeToolbar();
        void updateViewportWindowState();
        void syncViewportResize();
        void drawViewportOutput();
        void drawVulkanImGuiViewport(const ViewportOutput& output);
        void drawExternalSwapchainNotice(const ViewportOutput& output);
        void drawNoViewportOutputNotice();
        void drawViewportPlaceholder(const char* message);
        void drawViewportPlaceholder(const std::vector<std::string>& lines);
        void handleGizmoHotkeys();
        void drawGizmo();
        void endViewportWindow();

        bool canUseEmbeddedViewportOutput() const;
        bool isSceneViewMode() const;
        bool canUseSceneViewTools() const;
        void syncEditorCameraSize(uint32_t width, uint32_t height);
        void applyGizmoToSelectedEntity(const glm::mat4& transform, EditorToolOperation operation);

        bool onMouseButtonPressed(MouseButtonPressedEvent& e);
        void pickEntityAtMouse();
        void applyPickResult(const ViewportPickRequest& request, const ViewportPickResult& result);

    private:
        glm::vec2 m_viewport_size{ 1280, 720 };
        glm::vec2 m_viewport_bounds[2];
        glm::vec2 m_camera_viewport_size{ 0.0f, 0.0f };
        bool m_viewport_focused = false;
        bool m_viewport_hovered = false;
        
        bool m_was_using_gizmo_last_frame = false;
    };
} // namespace NexAur
