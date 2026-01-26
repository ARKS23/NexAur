#include "pch.h"
#include "engine.h"

#include "Function/Global/global_context.h"
#include "Function/Input/input_system.h"
#include "Function/Renderer/window_system.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Core/Events/window_event.h"

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
        logicalTick(delta_time);
        calculateFPS(delta_time);

        //TODO: 同步渲染数据

        rendererTick(delta_time);

        g_runtime_global_context.m_window_system->pollEvents(); // 窗口获取输入
        g_runtime_global_context.m_window_system->setTitle(std::string("NexAur: " + std::to_string(m_fps) + "FPS").c_str());

        return m_is_running;
    }

    void Engine::logicalTick(TimeStep delta_time) {

    }

    void Engine::rendererTick(TimeStep delta_time) {
        g_runtime_global_context.m_renderer_system->tick(delta_time); // 渲染

        g_runtime_global_context.m_window_system->update(); // 目前版本是window_system负责交换缓冲区
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

        // 事件拦截测试
        if (!event.handled)
            NX_CORE_INFO("havn't handled event: {}\n", event.toString());
        else
            NX_CORE_INFO("event handled: {}\n", event.toString());

        // 子系统事件分发显式调用链
        
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