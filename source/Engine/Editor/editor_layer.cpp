#include "pch.h"
#include "editor_layer.h"

#include "Editor/Panels/properties_panel.h"
#include "Editor/Panels/scene_hierarchy_panel.h"
#include "Editor/Panels/viewport_panel.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/RHI/renderer_service.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Renderer/editor_camera.h"
#include "Function/Scene/scene_service.h"
#include "Function/UI/ui_system.h"

#include <imgui.h>
#include <utility>

namespace NexAur {
    EditorLayer::EditorLayer()
        : EditorLayer(std::make_shared<EditorContext>()) {}

    EditorLayer::EditorLayer(std::shared_ptr<EditorContext> context)
        : m_context(std::move(context)) {
        init();
        NX_CORE_INFO("EditorLayer created.");
    }

    EditorLayer::~EditorLayer() {
        shutdown();
        NX_CORE_INFO("EditorLayer destroyed.");
    }

    void EditorLayer::init() {
        if (!m_context) {
            m_context = std::make_shared<EditorContext>();
        }

        m_context->active_scene = m_context->scene_service
            ? m_context->scene_service->getActiveScene()
            : nullptr;
        m_context->selected_entity = Entity();
        m_context->selection_source = "None";

        if (!m_context->viewport_camera) {
            m_context->viewport_camera = std::make_shared<EditorCamera>(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
        }
        m_context->viewport_camera->setInputService(m_context->input_service);

        m_panels.push_back(std::make_shared<ViewportPanel>("Viewport Panel"));
        m_panels.push_back(std::make_shared<SceneHierarchyPanel>("Scene Hierarchy Panel"));
        m_panels.push_back(std::make_shared<PropertiesPanel>("Properties Panel"));

        NX_CORE_INFO("EditorLayer initialized.");
    }

    void EditorLayer::shutdown() {
        m_panels.clear();
        NX_CORE_INFO("EditorLayer shutdown.");
    }

    Entity EditorLayer::getSelectedEntity() const {
        return m_context ? m_context->selected_entity : Entity();
    }

    void EditorLayer::setSelectedEntity(Entity entity, const std::string& source) {
        if (!m_context) {
            return;
        }

        m_context->selected_entity = entity;
        m_context->selection_source = source;
    }

    void EditorLayer::clearSelection() {
        if (!m_context) {
            return;
        }

        m_context->selected_entity = Entity();
        m_context->selection_source = "None";
    }

    const std::string& EditorLayer::getSelectionSource() const {
        static const std::string kNone = "None";
        return m_context ? m_context->selection_source : kNone;
    }

    bool EditorLayer::isViewportFocused() const {
        return m_context && m_context->viewport_focused;
    }

    bool EditorLayer::isViewportHovered() const {
        return m_context && m_context->viewport_hovered;
    }

    void EditorLayer::onUpdate(TimeStep delta_time) {
        if (!m_is_enabled) {
            return;
        }

        // 每帧先刷新 EditorContext，再让面板和相机读取最新服务状态。
        syncPanelContext();
        updateViewportCamera(delta_time);

        for (auto& panel : m_panels) {
            if (panel->isOpen()) {
                panel->onUpdate(delta_time);
            }
        }
    }

    void EditorLayer::onEvent(Event& event) {
        if (!m_is_enabled) {
            return;
        }

        for (auto& panel : m_panels) {
            if (panel->isOpen()) {
                panel->onEvent(event);
            }
            if (event.handled) {
                break;
            }
        }
    }

    void EditorLayer::onUIRender() {
        if (!m_is_enabled) {
            return;
        }

        // DockSpace 是编辑器 UI 根窗口，所有 Panel 都挂在它里面。
        beginDockSpace();

        for (auto& panel : m_panels) {
            if (panel->isOpen()) {
                panel->onUIRender();
            }
        }

        syncViewportCameraToRenderPacket();
        endDockSpace();
    }

    void EditorLayer::beginDockSpace() {
        static bool dock_space_open = true;
        static bool opt_fullscreen = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("NexAur Editor DockSpace", &dock_space_open, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen) {
            ImGui::PopStyleVar(2);
        }

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
    }

    void EditorLayer::endDockSpace() {
        ImGui::End();
    }

    void EditorLayer::syncPanelContext() {
        if (m_context && m_context->scene_service) {
            m_context->active_scene = m_context->scene_service->getActiveScene();
        }

        // Panel 不自己去找全局服务，只吃 EditorContext。
        for (auto& panel : m_panels) {
            panel->syncPanelContext(m_context);
        }
    }

    void EditorLayer::updateViewportCamera(TimeStep delta_time) {
        if (!m_context || !m_context->viewport_camera) {
            return;
        }

        const bool text_input_captured =
            m_context->ui_service && m_context->ui_service->wantsTextInput();

        // 普通 ImGui 键盘捕获不阻止 viewport 相机；只有文本输入时才屏蔽 WASD。
        if (!text_input_captured && (m_context->viewport_focused || m_context->viewport_hovered)) {
            m_context->viewport_camera->onUpdate(delta_time);
        }

        syncViewportCameraToRenderPacket();
    }

    void EditorLayer::syncViewportCameraToRenderPacket() const {
        if (!m_context || !m_context->viewport_camera || !m_context->render_context) {
            return;
        }

        // Editor 视口使用独立相机，写入 RenderDataPacket 覆盖场景 CameraComponent。
        auto& camera_data = m_context->render_context->getWriteData().camera_data;
        const auto& viewport_camera = *m_context->viewport_camera;

        camera_data.view_matrix = viewport_camera.getView();
        camera_data.projection_matrix = viewport_camera.getProjection();
        camera_data.view_projection_matrix = viewport_camera.getViewProjection();
        camera_data.position = viewport_camera.getPosition();
        camera_data.near_clip = viewport_camera.getNearClip();
        camera_data.far_clip = viewport_camera.getFarClip();
    }
} // namespace NexAur
