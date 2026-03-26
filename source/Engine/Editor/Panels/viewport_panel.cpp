#include "pch.h"
#include "viewport_panel.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace NexAur {
    ViewportPanel::ViewportPanel() {
        NX_CORE_INFO("ViewportPanel created.");
    }

    void ViewportPanel::onUpdate(TimeStep delta_time) {
        
    }

    void ViewportPanel::onUIRender() {
        // // viewport
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 }); // 让画面完全贴合面板边缘，无黑边
        // ImGui::Begin("Scene Viewport");
        
        // // 检查 Viewport 是否发生缩放
        // ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        // std::shared_ptr<RendererSystem> renderer_system = g_runtime_global_context.m_renderer_system;
        
        // auto [current_w, current_h] = renderer_system->getViewportSize();
        // if (viewportPanelSize.x > 0.0f && viewportPanelSize.y > 0.0f && // 不能缩成负数
        //     (viewportPanelSize.x != current_w || viewportPanelSize.y != current_h)) {
            
        //     // 通知 RendererSystem 调整 FBO 尺寸
        //     renderer_system->setViewportSize((uint32_t)viewportPanelSize.x, (uint32_t)viewportPanelSize.y);
            
        //     // 通知 EditorCamera 修改宽高比，临时方案，camera目前设计有问题
        //     auto editor_camera = g_runtime_global_context.m_scene_manager->getActiveScene()->getEditorCamera();
        //     editor_camera->setViewportSize(viewportPanelSize.x, viewportPanelSize.y);
        // }

        // // 获取渲染结果贴图 ID
        // uint32_t textureID = renderer_system->getViewportColorAttachment();
        
        // // 渲染画面到 ImGui 窗口
        // // 强转指针以适配 ImGui 接口类型；注意 OpenGL 的纹理 UV Y 轴是上下颠倒的，所以我们要 (0,1) 到 (1,0)
        // ImGui::Image(reinterpret_cast<void*>((intptr_t)textureID), 
        //              ImVec2{viewportPanelSize.x, viewportPanelSize.y}, 
        //              ImVec2{0, 1}, ImVec2{1, 0});

        // {
        //     // 测试ImGuizmo
        //     auto active_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
        //     if (active_scene) {
        //         // 测试gold Cube0
        //         Entity selected_entity = active_scene->findEntityByName("gold Cube0");
        //         auto editor_camera = active_scene->getEditorCamera();

        //         if (selected_entity && selected_entity.hasComponent<TransformComponent>() && editor_camera) {
        //             // 1. 设置 ImGuizmo 的基本环境
        //             ImGuizmo::SetOrthographic(false);
        //             ImGuizmo::SetDrawlist();

        //             //  将操作区限制在当前的 ImGui Window 内,防止乱飘
        //             ImVec2 windowPos = ImGui::GetWindowPos();
        //             ImVec2 windowSize = ImGui::GetWindowSize();
        //             ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);

        //             //  准备 Camera 数据
        //             const glm::mat4& cameraProjection = editor_camera->getProjection();
        //             glm::mat4 cameraView = editor_camera->getView();

        //             //  准备和转换 Entity 的 Transform 数据
        //             auto& tc = selected_entity.getComponent<TransformComponent>();
        //             glm::mat4 transform = tc.getTransform(); // 获取实体的 mat4 变换矩阵

        //             // 调用 ImGuizmo 绘制 Manipulator 轴，这里暂时固定只测试 平移（TRANSLATE）
        //             //  可以把 ImGuizmo::TRANSLATE 换成 ImGuizmo::ROTATE 或 ImGuizmo::SCALE
        //             ImGuizmo::Manipulate(glm::value_ptr(cameraView), 
        //                                 glm::value_ptr(cameraProjection),
        //                                 ImGuizmo::TRANSLATE, 
        //                                 ImGuizmo::WORLD, 
        //                                 glm::value_ptr(transform));

        //             // 在此帧内拖拽了 Gizmo 轴，则立刻反算并写回组件的平移/旋转/缩放
        //             if (ImGuizmo::IsUsing()) {
        //                 // 解析 ImGuizmo 修改后的矩阵，重写赋值给 TransformComponent
        //                 float translation[3], rotation[3], scale[3];
        //                 ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), translation, rotation, scale);

        //                 tc.translation = glm::vec3(translation[0], translation[1], translation[2]);
        //                 // ImGuizmo 出来的 rotation 是角度(Degrees)，需要转成弧度赋值给现有的结构
        //                 tc.rotation = glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2])); 
        //                 tc.scale = glm::vec3(scale[0], scale[1], scale[2]);
        //             }
        //         }
        //     }
        // }
                     
        // ImGui::End();
        // ImGui::PopStyleVar();
    }

    void ViewportPanel::onEvent(Event& event) {
    
    }
} // namespace NexAur
