#include "pch.h"
#include "editor_layer.h"

#include <imgui.h>

// 测试
#include "Function/Scene/component.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Global/global_context.h"

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
        NX_CORE_INFO("EditorLayer initialized.");
    }

    void EditorLayer::shutdown() {
        NX_CORE_INFO("EditorLayer shutdown.");
    }

    void EditorLayer::onUpdate(TimeStep delta_time) {
        // TODO: 更新editor camera的逻辑
    }

    void EditorLayer::onEvent(Event& event) {
        // TODO: 处理事件
    }

    void EditorLayer::onUIRender() {
        static bool dock_space_open = true;
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
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
        ImGui::Begin("DockSpace Demo", &dock_space_open, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen) ImGui::PopStyleVar(2);

        // 提交DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        // viewport
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 }); // 让画面完全贴合面板边缘，无黑边
        ImGui::Begin("Scene Viewport");
        
        // 检查 Viewport 是否发生缩放
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        std::shared_ptr<RendererSystem> renderer_system = g_runtime_global_context.m_renderer_system;
        
        auto [current_w, current_h] = renderer_system->getViewportSize();
        if (viewportPanelSize.x > 0.0f && viewportPanelSize.y > 0.0f && // 不能缩成负数
            (viewportPanelSize.x != current_w || viewportPanelSize.y != current_h)) {
            
            // 通知 RendererSystem 调整 FBO 尺寸
            renderer_system->setViewportSize((uint32_t)viewportPanelSize.x, (uint32_t)viewportPanelSize.y);
            
            // TODO: 也要通知 EditorCamera 修改宽高比
        }

        // 获取渲染结果贴图 ID
        uint32_t textureID = renderer_system->getViewportColorAttachment();
        
        // 渲染画面到 ImGui 窗口
        // 强转指针以适配 ImGui 接口类型；注意 OpenGL 的纹理 UV Y 轴是上下颠倒的，所以我们要 (0,1) 到 (1,0)
        ImGui::Image(reinterpret_cast<void*>((intptr_t)textureID), 
                     ImVec2{viewportPanelSize.x, viewportPanelSize.y}, 
                     ImVec2{0, 1}, ImVec2{1, 0});
                     
        ImGui::End();
        ImGui::PopStyleVar();

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

        ImGui::End(); // DockSpace 结束
    }
} // namespace NexAur
