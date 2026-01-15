#include "pch.h"
#include "engine.h"

#include "Function/Global/global_context.h"
#include "Function/Renderer/window_system.h"

namespace NexAur {
    void Engine::startEngine() {
        g_runtime_global_context.startSystems();

        NX_CORE_INFO("NexAur Engine started.");
    }

    void Engine::shutdownEngine() {
        g_runtime_global_context.shutdownSystems();

        NX_CORE_INFO("NexAur Engine shut down.");
    }

    void Engine::run() {
        std::shared_ptr<WindowSystem> window_system = g_runtime_global_context.m_window_system;
        while (!window_system->shouldClose()) {
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
        // 测试
        g_runtime_global_context.m_window_system->update();
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
} // namespace NexAur