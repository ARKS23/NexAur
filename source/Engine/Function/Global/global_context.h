#pragma once

#include <memory>

#include "Core/Base.h"

namespace NexAur {
    // 前置声明
    class WindowSystem;
    class InputSystem;
    class RendererSystem;
    class FileSystem;

    class NEXAUR_API RunTimeGlobalContext {
    public:
        void startSystems();
        void shutdownSystems();

    public:
        std::shared_ptr<FileSystem> m_file_system;
        std::shared_ptr<WindowSystem> m_window_system;
        std::shared_ptr<InputSystem> m_input_system;
        std::shared_ptr<RendererSystem> m_renderer_system;
    };

    extern RunTimeGlobalContext g_runtime_global_context;

    // 资源路径宏定义，方便使用
    #define NX_ASSET(relative_path) \
        (NexAur::g_runtime_global_context.m_file_system->getAssetPath(relative_path).string())
} // namespace NexAur