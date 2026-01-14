#include "pch.h"
#include "global_context.h"

#include "Core/Log/log_system.h"

namespace NexAur {
    RunTimeGlobalContext g_runtime_global_context; // 全局运行时上下文实例

    void RunTimeGlobalContext::startSystems() {
        LogSystem::init();  // 初始化全局静态单例日志系统
    }

    void RunTimeGlobalContext::shutdownSystems() {
        
    }
} // namespace NexAur