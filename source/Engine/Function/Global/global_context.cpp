#include "pch.h"
#include "global_context.h"

#include "Core/Log/log_system.h"
#include "Function/Renderer/window_system.h"
#include "Function/Input/input_system_glfw.h"

namespace NexAur {
    RunTimeGlobalContext g_runtime_global_context; // 全局运行时上下文实例

    void RunTimeGlobalContext::startSystems() {
        LogSystem::init();  // 初始化全局静态单例日志系统

        m_window_system = std::make_shared<WindowSystem>();
        WindowSpecification specification;
        m_window_system->init(specification);

        m_input_system = std::make_shared<InputSystemGLFW>();
    }

    void RunTimeGlobalContext::shutdownSystems() {
        m_window_system.reset();
    }
} // namespace NexAur