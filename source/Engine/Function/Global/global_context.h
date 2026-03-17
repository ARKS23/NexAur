#pragma once

#include <memory>

#include "Core/Base.h"

namespace NexAur {
    // 前置声明
    class WindowSystem;
    class InputSystem;
    class RendererSystem;
    class FileSystem;
    class SceneManager;
    class RenderContext;

    class NEXAUR_API RunTimeGlobalContext {
    public:
        void startSystems();
        void shutdownSystems();

    public:
        std::shared_ptr<FileSystem> m_file_system;
        std::shared_ptr<WindowSystem> m_window_system;
        std::shared_ptr<InputSystem> m_input_system;
        std::shared_ptr<SceneManager> m_scene_manager;
        std::shared_ptr<RendererSystem> m_renderer_system;
        std::shared_ptr<RenderContext> m_render_context;
    };

    extern NEXAUR_API RunTimeGlobalContext g_runtime_global_context;

    // 资源路径宏定义，方便使用
#if defined(NDEBUG) || defined(NX_DIST)
    #define NX_ASSET(relative_path) (std::string(relative_path))
#else
    #define NX_ASSET(relative_path) \
        (NexAur::g_runtime_global_context.m_file_system->getAssetPath(relative_path).string())
#endif
} // namespace NexAur