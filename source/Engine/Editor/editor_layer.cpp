#include "pch.h"
#include "editor_layer.h"
#include "Editor/Panels/editor_panel.h"

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
        m_context.active_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
        m_context.selected_entity = Entity();
        m_context.renderer_system = g_runtime_global_context.m_renderer_system;
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
            
            // 通知 EditorCamera 修改宽高比，临时方案，camera目前设计有问题
            auto editor_camera = g_runtime_global_context.m_scene_manager->getActiveScene()->getEditorCamera();
            editor_camera->setViewportSize(viewportPanelSize.x, viewportPanelSize.y);
        }

        // 获取渲染结果贴图 ID
        uint32_t textureID = renderer_system->getViewportColorAttachment();
        
        // 渲染画面到 ImGui 窗口
        // 强转指针以适配 ImGui 接口类型；注意 OpenGL 的纹理 UV Y 轴是上下颠倒的，所以我们要 (0,1) 到 (1,0)
        ImGui::Image(reinterpret_cast<void*>((intptr_t)textureID), 
                     ImVec2{viewportPanelSize.x, viewportPanelSize.y}, 
                     ImVec2{0, 1}, ImVec2{1, 0});

        {
            // 测试ImGuizmo
            auto active_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
            if (active_scene) {
                // 测试gold Cube0
                Entity selected_entity = active_scene->findEntityByName("gold Cube0");
                auto editor_camera = active_scene->getEditorCamera();

                if (selected_entity && selected_entity.hasComponent<TransformComponent>() && editor_camera) {
                    // 1. 设置 ImGuizmo 的基本环境
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();

                    //  将操作区限制在当前的 ImGui Window 内,防止乱飘
                    ImVec2 windowPos = ImGui::GetWindowPos();
                    ImVec2 windowSize = ImGui::GetWindowSize();
                    ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);

                    //  准备 Camera 数据
                    const glm::mat4& cameraProjection = editor_camera->getProjection();
                    glm::mat4 cameraView = editor_camera->getView();

                    //  准备和转换 Entity 的 Transform 数据
                    auto& tc = selected_entity.getComponent<TransformComponent>();
                    glm::mat4 transform = tc.getTransform(); // 获取实体的 mat4 变换矩阵

                    // 调用 ImGuizmo 绘制 Manipulator 轴，这里暂时固定只测试 平移（TRANSLATE）
                    //  可以把 ImGuizmo::TRANSLATE 换成 ImGuizmo::ROTATE 或 ImGuizmo::SCALE
                    ImGuizmo::Manipulate(glm::value_ptr(cameraView), 
                                        glm::value_ptr(cameraProjection),
                                        ImGuizmo::TRANSLATE, 
                                        ImGuizmo::WORLD, 
                                        glm::value_ptr(transform));

                    // 在此帧内拖拽了 Gizmo 轴，则立刻反算并写回组件的平移/旋转/缩放
                    if (ImGuizmo::IsUsing()) {
                        // 解析 ImGuizmo 修改后的矩阵，重写赋值给 TransformComponent
                        float translation[3], rotation[3], scale[3];
                        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), translation, rotation, scale);

                        tc.translation = glm::vec3(translation[0], translation[1], translation[2]);
                        // ImGuizmo 出来的 rotation 是角度(Degrees)，需要转成弧度赋值给现有的结构
                        tc.rotation = glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2])); 
                        tc.scale = glm::vec3(scale[0], scale[1], scale[2]);
                    }
                }
            }
        }
                     
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
