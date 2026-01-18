#pragma once

#include <string>
#include <chrono>

#include "Core/Events/event.h"
#include "Core/Time/Clock.h"
#include "Core/Base.h"

namespace NexAur {
    /*
     * Engine负责管理应用程序和各个子系统的生命周期，作为调度器协调全局上下文中的子系统按序工作。
     * 采用逻辑和渲染分离的架构设计，游戏逻辑更新和渲染更新解耦。
     */
    class WindowCloseEvent;
    class WindowResizeEvent;

    class NEXAUR_API Engine {
    public:
        void startEngine();
        void shutdownEngine();

        bool isRunning() const { return m_is_running; }
        void run();
        bool tickOneFrame(TimeStep delta_time);

        void onEvent(Event& event);

        int getFPS() const { return m_fps; }

    protected:
        void logicalTick(TimeStep delta_time);
        void rendererTick(TimeStep delta_time);
        void calculateFPS(TimeStep delta_time);
        TimeStep calculateDeltaTime();

    private:
        bool onWindowClose(WindowCloseEvent& event);
        bool onWindowResize(WindowResizeEvent& event);
        
    private:
        bool m_is_running = true;
        Clock m_clock;

        int m_frame_count_accumulator = 0;  // 累计帧数
        TimeStep m_fps_timer{0.0};  // 累计时间
        int m_fps = 0;
    };
} // namespace NexAur