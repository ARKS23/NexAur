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
    class UISystem;
    class ModuleManager;
    class ModuleRegistry;

    class NEXAUR_API RunTimeGlobalContext {
    public:
        RunTimeGlobalContext();
        ~RunTimeGlobalContext();

        RunTimeGlobalContext(const RunTimeGlobalContext&) = delete;
        RunTimeGlobalContext& operator=(const RunTimeGlobalContext&) = delete;
        RunTimeGlobalContext(RunTimeGlobalContext&&) = delete;
        RunTimeGlobalContext& operator=(RunTimeGlobalContext&&) = delete;

        void startSystems();
        void shutdownSystems();

        ModuleManager* getModuleManager();
        const ModuleManager* getModuleManager() const;
        ModuleRegistry* getModuleRegistry();
        const ModuleRegistry* getModuleRegistry() const;

    public:
        // Legacy compatibility views. New module code should prefer ModuleRegistry
        // or explicit constructor/function dependencies over these public pointers.
        std::shared_ptr<FileSystem> m_file_system;
        std::shared_ptr<WindowSystem> m_window_system;
        std::shared_ptr<InputSystem> m_input_system;
        std::shared_ptr<SceneManager> m_scene_manager;
        std::shared_ptr<RendererSystem> m_renderer_system;
        std::shared_ptr<RenderContext> m_render_context;
        std::shared_ptr<UISystem> m_ui_system;

    private:
        void clearCompatibilityServices();

    private:
        std::unique_ptr<ModuleManager> m_module_manager;
    };

    extern NEXAUR_API RunTimeGlobalContext g_runtime_global_context;

    // 资源路径宏定义，方便使用
#ifndef NX_ASSET
#if defined(NDEBUG) || defined(NX_DIST)
    #define NX_ASSET(relative_path) (std::string(relative_path))
#else
    #define NX_ASSET(relative_path) \
        (NexAur::g_runtime_global_context.m_file_system->getAssetPath(relative_path).string())
#endif
#endif
} // namespace NexAur
