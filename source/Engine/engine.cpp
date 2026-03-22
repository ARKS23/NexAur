#include "pch.h"
#include "engine.h"

#include "Function/Global/global_context.h"
#include "Function/Input/input_system.h"
#include "Function/Renderer/window_system.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Scene/scene_v2.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/UI/ui_system.h"
#include "Core/Events/window_event.h"

#include <imgui.h>

// 测试
#include <glm/gtc/type_ptr.hpp>
#include "Function/Scene/entity.h"
#include "Function/Scene/component.h"

namespace NexAur {
    void Engine::startEngine() {
        g_runtime_global_context.startSystems();
        g_runtime_global_context.m_window_system->setEventCallback(NX_BIND_EVENT_FN(Engine::onEvent));
        NX_CORE_INFO("NexAur Engine started.");
    }

    void Engine::shutdownEngine() {
        g_runtime_global_context.shutdownSystems();
        NX_CORE_INFO("NexAur Engine shut down.");
    }

    void Engine::run() {
        std::shared_ptr<WindowSystem> window_system = g_runtime_global_context.m_window_system;
        while (m_is_running) {
            TimeStep delta_time = calculateDeltaTime();
            tickOneFrame(delta_time);
        }
    }

    bool Engine::tickOneFrame(TimeStep delta_time) {
        calculateFPS(delta_time);
        logicalTick(delta_time);

        // UI逻辑
        g_runtime_global_context.m_ui_system->beginFrame();
        this->editorTest();

        // TODO: 数据同步，后期改成多线程
        std::shared_ptr<RenderContext> render_context = g_runtime_global_context.m_render_context;
        render_context->swapBuffers();

        rendererTick(delta_time);

        g_runtime_global_context.m_ui_system->endFrameAndRender();

        g_runtime_global_context.m_window_system->update(); // 目前版本是window_system负责交换缓冲区
        g_runtime_global_context.m_window_system->pollEvents(); // 窗口获取输入
        g_runtime_global_context.m_window_system->setTitle(std::string("NexAur: " + std::to_string(m_fps) + "FPS").c_str());

        return m_is_running;
    }

    void Engine::editorTest() {
        if (!m_is_edtior_mode) return;

        ImGui::Begin("NexAur Editor (Inside Engine DLL)");

        ImGui::Text("=== Scene Object Controller ===");
        
        std::shared_ptr<SceneV2> active_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
        
        if (active_scene) {
            Entity target_entity = active_scene->findEntityByName("gold Cube0");

            if (target_entity && target_entity.hasComponent<TransformComponent>()) {
                
                ImGui::Text("Controlling Entity: %s", target_entity.getComponent<TagComponent>().name.c_str());
                ImGui::Spacing();
                
                // 拿到控制权的引用
                auto& transform = target_entity.getComponent<TransformComponent>();
                
                // 位置控制条，每次拖动数值变化 0.1f
                ImGui::DragFloat3("Position", glm::value_ptr(transform.translation), 0.1f);
                
                // 旋转控制条
                glm::vec3 rotationDegrees = glm::degrees(transform.rotation);
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotationDegrees), 1.0f)) {
                    transform.rotation = glm::radians(rotationDegrees);
                }
                
                // 缩放控制条
                ImGui::DragFloat3("Scale", glm::value_ptr(transform.scale), 0.1f);
                
                ImGui::Separator();
                
                // 加入一个重置按钮
                if (ImGui::Button("Reset Transform")) {
                    transform.translation = glm::vec3(0.0f);
                    transform.rotation = glm::vec3(0.0f);
                    transform.scale = glm::vec3(1.0f);
                }
            } 
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Target Entity 'gold Cube0' not found!");
            }
        } 
        else {
            ImGui::Text("No active scene!");
        }
        
        ImGui::End();
    }

    void Engine::logicalTick(TimeStep delta_time) {
        std::shared_ptr<SceneV2> active_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
        active_scene->tick(delta_time);

        std::shared_ptr<RenderContext> render_context = g_runtime_global_context.m_render_context;
        RenderDataPacket* write_packet = &render_context->getWriteData();
        active_scene->extractSceneData(write_packet); // 从场景提取数据到渲染数据包写入缓冲区
    }

    void Engine::rendererTick(TimeStep delta_time) {
        const RenderDataPacket& render_data = g_runtime_global_context.m_render_context->getReadData();
        g_runtime_global_context.m_renderer_system->tick(delta_time, render_data); // 将渲染数据包传给渲染系统进行渲染
    }

    void Engine::calculateFPS(TimeStep delta_time) {
        m_frame_count_accumulator++;
        m_fps_timer += delta_time;

        if (m_fps_timer.GetSeconds() >= 1.0) {
            m_fps = m_frame_count_accumulator;
            m_frame_count_accumulator = 0;
            m_fps_timer.reset();
        }
    }

    TimeStep Engine::calculateDeltaTime() {
        return m_clock.restart();
    }

    void Engine::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<WindowCloseEvent>(NX_BIND_EVENT_FN(Engine::onWindowClose));
        dispatcher.dispatch<WindowResizeEvent>(NX_BIND_EVENT_FN(Engine::onWindowResize));

        // 子系统事件分发显式调用链
        g_runtime_global_context.m_ui_system->onEvent(event);
        if (g_runtime_global_context.m_ui_system->isConsumeingInput()) {
            NX_CORE_INFO("Event consumed: True");
        }
    }

    bool Engine::onWindowClose(WindowCloseEvent& event) {
        NX_CORE_INFO("Window close event received. Shutting down engine.");
        m_is_running = false;
        return true;
    }

    bool Engine::onWindowResize(WindowResizeEvent& event) {
        NX_CORE_INFO("Window resize event received: {}x{}", event.getWidth(), event.getHeight());
        return false;
    }
} // namespace NexAur