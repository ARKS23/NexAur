#pragma once

#include <memory>

#include "Core/Base.h"

namespace NexAur {
    // 前置声明
    class WindowSystem;
    class InputSystem;

    class NEXAUR_API RunTimeGlobalContext {
    public:
        void startSystems();
        void shutdownSystems();

    public:
        std::shared_ptr<WindowSystem> m_window_system;
        std::shared_ptr<InputSystem> m_input_system;
    };

    extern RunTimeGlobalContext g_runtime_global_context;
} // namespace NexAur