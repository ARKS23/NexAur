#include "pch.h"
#include "editor_layer.h"
#include "Editor/Panels/viewport_panel.h"

#include <imgui.h>

// 测试
#include "Function/Scene/component.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Global/global_context.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Renderer/editor_camera.h"
#include "ImGuizmo.h"

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
        m_context = std::make_shared<EditorContext>();
        m_context->active_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
        m_context->selected_entity = Entity();
        m_context->renderer_system = g_runtime_global_context.m_renderer_system;
        m_panels.push_back(std::make_shared<ViewportPanel>());
        NX_CORE_INFO("EditorLayer initialized.");
    }

    void EditorLayer::shutdown() {
        NX_CORE_INFO("EditorLayer shutdown.");
    }

    void EditorLayer::onUpdate(TimeStep delta_time) {
        // TODO: 更新editor camera的逻辑
        synPanelContext();

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
        
        // 测试Transform控制
        ImGui::Begin("Properties", &m_show_properties_panel);
        ImGui::Text("=== Scene Object Controller ===");

        if (m_active_scene) {
            m_selected_entity = m_active_scene->findEntityByName("gold Cube0");
            
            if (m_selected_entity && m_selected_entity.hasComponent<TransformComponent>()) {
                ImGui::Text("Controlling Entity: %s", m_selected_entity.getComponent<TagComponent>().name.c_str());
                ImGui::Spacing();
                
                auto& transform = m_selected_entity.getComponent<TransformComponent>();
                ImGui::DragFloat3("Position", glm::value_ptr(transform.translation), 0.1f);
                
                glm::vec3 rotationDegrees = glm::degrees(transform.rotation);
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), 1.0f)) {
                    transform.rotation = glm::radians(rotationDegrees);
                }
                
                ImGui::DragFloat3("Scale", glm::value_ptr(transform.scale), 0.1f);
                
                ImGui::Separator();
                if (ImGui::Button("Reset Transform")) {
                    transform.translation = glm::vec3(0.0f);
                    transform.rotation = glm::vec3(0.0f);
                    transform.scale = glm::vec3(1.0f);
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Target Entity not found!");
            }
        } else {
            ImGui::Text("No active scene!");
        }
        ImGui::End(); // Properties 结束

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

    void EditorLayer::synPanelContext() {
        for (auto& panel : m_panels) {
            panel->synPanelContext(m_context);
        }
    }
} // namespace NexAur
