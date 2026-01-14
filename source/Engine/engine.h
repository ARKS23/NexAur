#pragma once

#include <string>
#include <chrono>

#include "Core/Base.h"

namespace NexAur {
    /*
     * Engine负责管理应用程序和各个子系统的生命周期，作为调度器协调全局上下文中的子系统按序工作。
     * 采用逻辑和渲染分离的架构设计，游戏逻辑更新和渲染更新解耦。
     */

    class NEXAUR_API Engine {
    public:
        void startEngine();
        void shutdownEngine();

        bool isRunning() const { return m_is_running; }
        void run();
        bool tickOneFrame(float delta_time);

        int getFPS() const { return m_fps; }

    protected:
        void logicalTick(float delta_time);
        void rendererTick(float delta_time);
        void calculateFPS(float delta_time);
        float calculateDeltaTime();
        
    private:
        bool m_is_running = false;
        std::chrono::steady_clock::time_point m_last_tick_time_point{std::chrono::steady_clock::now()};
        int m_fps = 0;
    };
} // namespace NexAur