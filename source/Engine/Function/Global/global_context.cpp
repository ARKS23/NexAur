#include "pch.h"
#include "global_context.h"

#include "Core/Log/log_system.h"
#include "Function/Renderer/window_system.h"
#include "Function/Input/input_system_glfw.h"

namespace NexAur {
    RunTimeGlobalContext g_runtime_global_context; // 全局运行时上下文实例

    void RunTimeGlobalContext::startSystems() {
        // 初始化全局静态单例日志系统
        LogSystem::init();

        // 窗口系统
        m_window_system = std::make_shared<WindowSystem>();
        WindowSpecification specification;
        m_window_system->init(specification);

        // 输入系统
        m_input_system = std::make_shared<InputSystemGLFW>();

        // 渲染系统
        

        // 物理系统
    }

    void RunTimeGlobalContext::shutdownSystems() {
        m_window_system->shutdown();
        m_window_system.reset();
    }
} // namespace NexAur