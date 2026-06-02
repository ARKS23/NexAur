#include "pch.h"
#include "editor_layer.h"
#include "Editor/Panels/viewport_panel.h"
#include "Editor/Panels/scene_hierarchy_panel.h"
#include "Editor/Panels/properties_panel.h"

#include <imgui.h>

#include "Function/Global/global_context.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Renderer/editor_camera.h"
#include "Function/Renderer/data/render_context.h"

namespace NexAur {
    EditorLayer::EditorLayer() {
        init();
        NX_CORE_INFO("EditorLayer created.");
    }

    EditorLayer::~EditorLayer() {
        shutdown();
        NX_CORE_INFO("EditorLayer destroyed.");
    }

    void EditorLayer::init() {
        // 编辑器上下文初始化
        m_context = std::make_shared<EditorContext>();
        m_context->active_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
        m_context->selected_entity = Entity();
        m_context->renderer_system = g_runtime_global_context.m_renderer_system;
        m_context->viewport_camera = std::make_shared<EditorCamera>(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
        m_context->selection_source = "None";

        // 子面板注册
        m_panels.push_back(std::make_shared<ViewportPanel>("Viewport Panel"));
        m_panels.push_back(std::make_shared<SceneHierarchyPanel>("Scene Hierarchy Panel"));
        m_panels.push_back(std::make_shared<PropertiesPanel>("Properties Panel"));
        NX_CORE_INFO("EditorLayer initialized.");
    }

    void EditorLayer::shutdown() {
        NX_CORE_INFO("EditorLayer shutdown.");
    }

    void EditorLayer::onUpdate(TimeStep delta_time) {
        syncPanelContext();
        updateViewportCamera(delta_time);

        for (auto& panel : m_panels) {
            if (panel->isOpen()) panel->onUpdate(delta_time);
        }
    }

    void EditorLayer::onEvent(Event& event) {
        for (auto& panel : m_panels) {
            if (panel->isOpen()) panel->onEvent(event);
            if (event.handled) break;
        }
    }

    void EditorLayer::onUIRender() {
        beginDockSpace();

        for (auto& panel : m_panels) {
            if (panel->isOpen()) panel->onUIRender();
        }

        // 视口面板可能在 UI 阶段更新相机宽高，交换渲染包前再同步一次矩阵。
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
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("NexAur Editor DockSpace", &dock_space_open, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen) ImGui::PopStyleVar(2);

        // 提交DockSpace
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
        for (auto& panel : m_panels) {
            panel->syncPanelContext(m_context);
        }
    }

    void EditorLayer::updateViewportCamera(TimeStep delta_time) {
        if (!m_context || !m_context->viewport_camera) return;

        // 编辑器观察相机只响应视口交互，但始终覆盖渲染包里的视口相机数据。
        if (m_context->viewport_focused || m_context->viewport_hovered) {
            m_context->viewport_camera->onUpdate(delta_time);
        }

        syncViewportCameraToRenderPacket();
    }

    void EditorLayer::syncViewportCameraToRenderPacket() const {
        if (!m_context || !m_context->viewport_camera) return;
        if (!g_runtime_global_context.m_render_context) return;

        auto& camera_data = g_runtime_global_context.m_render_context->getWriteData().camera_data;
        const auto& viewport_camera = *m_context->viewport_camera;

        camera_data.view_matrix = viewport_camera.getView();
        camera_data.projection_matrix = viewport_camera.getProjection();
        camera_data.view_projection_matrix = viewport_camera.getViewProjection();
        camera_data.position = viewport_camera.getPosition();
    }
} // namespace NexAur
